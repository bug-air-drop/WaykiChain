// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2017-2019 The WaykiChain Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "cdpdb.h"

CCdpDBCache::CCdpDBCache(CDBAccess *pDbAccess)
    : globalStakedBcoinsCache(pDbAccess),
      globalOwedScoinsCache(pDbAccess),
      cdpCache(pDbAccess),
      regId2CDPCache(pDbAccess),
      ratioCDPIdCache(pDbAccess) {}

CCdpDBCache::CCdpDBCache(CCdpDBCache *pBaseIn)
    : globalStakedBcoinsCache(pBaseIn->globalStakedBcoinsCache),
      globalOwedScoinsCache(pBaseIn->globalOwedScoinsCache),
      cdpCache(pBaseIn->cdpCache),
      regId2CDPCache(pBaseIn->regId2CDPCache),
      ratioCDPIdCache(pBaseIn->ratioCDPIdCache) {}

bool CCdpDBCache::NewCDP(const int32_t blockHeight, CUserCDP &cdp) {
    assert(!cdpCache.HaveData(cdp.cdpid));
    return SaveCDPToDB(cdp) && SaveCDPToRatioDB(cdp);
}

bool CCdpDBCache::EraseCDP(const CUserCDP &oldCDP, const CUserCDP &cdp) {
    return EraseCDPFromDB(cdp) && EraseCDPFromRatioDB(oldCDP);
}

// Need to delete the old cdp(before updating cdp), then save the new cdp if necessary.
bool CCdpDBCache::UpdateCDP(const CUserCDP &oldCDP, const CUserCDP &newCDP) {
    return SaveCDPToDB(newCDP) && EraseCDPFromRatioDB(oldCDP) && SaveCDPToRatioDB(newCDP);
}

bool CCdpDBCache::GetCDPList(const CRegID &regId, vector<CUserCDP> &cdpList) {
    set<uint256> cdpTxids;
    if (!regId2CDPCache.GetData(regId.ToRawString(), cdpTxids)) {
        return false;
    }

    CUserCDP userCdp;
    for (const auto &item : cdpTxids) {
        if (!cdpCache.GetData(item, userCdp)) {
            return false;
        }

        cdpList.push_back(userCdp);
    }

    return true;
}

bool CCdpDBCache::GetCDP(const uint256 cdpid, CUserCDP &cdp) {
    return cdpCache.GetData(cdpid, cdp);
}

// Attention: update cdpCache and regId2CDPCache synchronously.
bool CCdpDBCache::SaveCDPToDB(const CUserCDP &cdp) {
    set<uint256> cdpTxids;
    regId2CDPCache.GetData(cdp.owner_regid.ToRawString(), cdpTxids);
    cdpTxids.insert(cdp.cdpid);   // failed to insert if txid existed.

    return cdpCache.SetData(cdp.cdpid, cdp) && regId2CDPCache.SetData(cdp.owner_regid.ToRawString(), cdpTxids);
}

bool CCdpDBCache::EraseCDPFromDB(const CUserCDP &cdp) {
    set<uint256> cdpTxids;
    regId2CDPCache.GetData(cdp.owner_regid.ToRawString(), cdpTxids);
    cdpTxids.erase(cdp.cdpid);

    // If cdpTxids is empty, regId2CDPCache will erase the key automatically.
    return cdpCache.EraseData(cdp.cdpid) && regId2CDPCache.SetData(cdp.owner_regid.ToRawString(), cdpTxids);
}

bool CCdpDBCache::SaveCDPToRatioDB(const CUserCDP &userCdp) {
    uint64_t globalStakedBcoins = GetGlobalStakedBcoins();
    uint64_t globalOwedScoins   = GetGlobalOwedScoins();

    globalStakedBcoins += userCdp.total_staked_bcoins;
    globalOwedScoins += userCdp.total_owed_scoins;

    globalStakedBcoinsCache.SetData(globalStakedBcoins);
    globalOwedScoinsCache.SetData(globalOwedScoins);

    // cdpr{Ratio}{cdpid} -> CUserCDP
    uint64_t boostedRatio = userCdp.collateral_ratio_base * CDP_BASE_RATIO_BOOST;
    uint64_t ratio        = (boostedRatio < userCdp.collateral_ratio_base /* overflown */) ? UINT64_MAX : boostedRatio;
    string strRatio       = strprintf("%016x", ratio);
    auto key              = std::make_pair(strRatio, userCdp.cdpid);

    ratioCDPIdCache.SetData(key, userCdp);

    return true;
}

bool CCdpDBCache::EraseCDPFromRatioDB(const CUserCDP &userCdp) {
    uint64_t globalStakedBcoins = GetGlobalStakedBcoins();
    uint64_t globalOwedScoins   = GetGlobalOwedScoins();

    globalStakedBcoins -= userCdp.total_staked_bcoins;
    globalOwedScoins -= userCdp.total_owed_scoins;

    globalStakedBcoinsCache.SetData(globalStakedBcoins);
    globalOwedScoinsCache.SetData(globalOwedScoins);

    uint64_t boostedRatio = userCdp.collateral_ratio_base * CDP_BASE_RATIO_BOOST;
    uint64_t ratio        = (boostedRatio < userCdp.collateral_ratio_base /* overflown */) ? UINT64_MAX : boostedRatio;
    string strRatio       = strprintf("%016x", ratio);
    auto key              = std::make_pair(strRatio, userCdp.cdpid);

    ratioCDPIdCache.EraseData(key);

    return true;
}

// global collateral ratio floor check
bool CCdpDBCache::CheckGlobalCollateralRatioFloorReached(const uint64_t bcoinMedianPrice,
                                                         const uint64_t globalCollateralRatioLimit) {
    return GetGlobalCollateralRatio(bcoinMedianPrice) < globalCollateralRatioLimit;
}

// global collateral amount ceiling check
bool CCdpDBCache::CheckGlobalCollateralCeilingReached(const uint64_t newBcoinsToStake,
                                                      const uint64_t globalCollateralCeiling) {
    return (newBcoinsToStake + GetGlobalStakedBcoins()) > globalCollateralCeiling * COIN;
}

bool CCdpDBCache::GetCdpListByCollateralRatio(const uint64_t collateralRatio, const uint64_t bcoinMedianPrice,
                                              set<CUserCDP> &userCdps) {
    double ratio = (double(collateralRatio) / RATIO_BOOST) / (double(bcoinMedianPrice) / PRICE_BOOST);
    assert(uint64_t(ratio * CDP_BASE_RATIO_BOOST) < UINT64_MAX);
    string strRatio = strprintf("%016x", uint64_t(ratio * CDP_BASE_RATIO_BOOST));

    return ratioCDPIdCache.GetAllElements(strRatio, userCdps);
}

uint64_t CCdpDBCache::GetGlobalStakedBcoins() const {
    uint64_t globalStakedBcoins = 0;
    globalStakedBcoinsCache.GetData(globalStakedBcoins);

    return globalStakedBcoins;
}

inline uint64_t CCdpDBCache::GetGlobalOwedScoins() const {
    uint64_t globalOwedScoins = 0;
    globalOwedScoinsCache.GetData(globalOwedScoins);

    return globalOwedScoins;
}

void CCdpDBCache::GetGlobalItem(uint64_t &globalStakedBcoins, uint64_t &globalOwedScoins) const {
    globalStakedBcoinsCache.GetData(globalStakedBcoins);
    globalOwedScoinsCache.GetData(globalOwedScoins);
}

uint64_t CCdpDBCache::GetGlobalCollateralRatio(const uint64_t bcoinMedianPrice) const {
    // If total owed scoins equal to zero, the global collateral ratio becomes infinite.
    uint64_t globalOwedScoins = GetGlobalOwedScoins();
    if (globalOwedScoins == 0) {
        return UINT64_MAX;
    }

    uint64_t globalStakedBcoins = GetGlobalStakedBcoins();

    return double(globalStakedBcoins) * bcoinMedianPrice / PRICE_BOOST / globalOwedScoins * RATIO_BOOST;
}

void CCdpDBCache::SetBaseViewPtr(CCdpDBCache *pBaseIn) {
    globalStakedBcoinsCache.SetBase(&pBaseIn->globalStakedBcoinsCache);
    globalOwedScoinsCache.SetBase(&pBaseIn->globalOwedScoinsCache);
    cdpCache.SetBase(&pBaseIn->cdpCache);
    regId2CDPCache.SetBase(&pBaseIn->regId2CDPCache);
    ratioCDPIdCache.SetBase(&pBaseIn->ratioCDPIdCache);
}

void CCdpDBCache::SetDbOpLogMap(CDBOpLogMap *pDbOpLogMapIn) {
    globalStakedBcoinsCache.SetDbOpLogMap(pDbOpLogMapIn);
    globalOwedScoinsCache.SetDbOpLogMap(pDbOpLogMapIn);
    cdpCache.SetDbOpLogMap(pDbOpLogMapIn);
    regId2CDPCache.SetDbOpLogMap(pDbOpLogMapIn);
    ratioCDPIdCache.SetDbOpLogMap(pDbOpLogMapIn);
}

bool CCdpDBCache::UndoData() {
    return globalStakedBcoinsCache.UndoData() && globalOwedScoinsCache.UndoData() && cdpCache.UndoData() &&
           regId2CDPCache.UndoData() && ratioCDPIdCache.UndoData();
}

uint32_t CCdpDBCache::GetCacheSize() const {
    return globalStakedBcoinsCache.GetCacheSize() + globalOwedScoinsCache.GetCacheSize() + cdpCache.GetCacheSize() +
           regId2CDPCache.GetCacheSize() + ratioCDPIdCache.GetCacheSize();
}

bool CCdpDBCache::Flush() {
    globalStakedBcoinsCache.Flush();
    globalOwedScoinsCache.Flush();
    cdpCache.Flush();
    regId2CDPCache.Flush();
    ratioCDPIdCache.Flush();

    return true;
}
