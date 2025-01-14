// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2017-2019 The WaykiChain Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef TX_COIN_TRANSFER_H
#define TX_COIN_TRANSFER_H

#include "tx.h"

/**################################ Base Coin (WICC) Transfer ########################################**/
class CBaseCoinTransferTx : public CBaseTx {
public:
    mutable CUserID toUid;  // Recipient Regid or Keyid
    uint64_t coin_amount;   // coin amount (coin symbol: WICC)
    string memo;

public:
    CBaseCoinTransferTx() : CBaseTx(BCOIN_TRANSFER_TX) {}
    CBaseCoinTransferTx(const CUserID &txUidIn, const CUserID &toUidIn, const int32_t validHeightIn,
                        const uint64_t coinAmount, const uint64_t feesIn, const string &memoIn)
        : CBaseTx(BCOIN_TRANSFER_TX, txUidIn, validHeightIn, SYMB::WICC, feesIn),
          toUid(toUidIn),
          coin_amount(coinAmount),
          memo(memoIn) {}
    ~CBaseCoinTransferTx() {}

    IMPLEMENT_SERIALIZE(
        READWRITE(VARINT(this->nVersion));
        nVersion = this->nVersion;
        READWRITE(VARINT(valid_height));
        READWRITE(txUid);

        READWRITE(toUid);
        READWRITE(VARINT(llFees));
        READWRITE(VARINT(coin_amount));
        READWRITE(memo);
        READWRITE(signature);
    )

    TxID ComputeSignatureHash(bool recalculate = false) const {
        if (recalculate || sigHash.IsNull()) {
            CHashWriter ss(SER_GETHASH, 0);
            ss << VARINT(nVersion) << uint8_t(nTxType) << VARINT(valid_height) << txUid << toUid
               << VARINT(llFees) << VARINT(coin_amount) << memo;
            sigHash = ss.GetHash();
        }

        return sigHash;
    }

    virtual std::shared_ptr<CBaseTx> GetNewInstance() const { return std::make_shared<CBaseCoinTransferTx>(*this); }
    virtual string ToString(CAccountDBCache &accountCache);
    virtual Object ToJson(const CAccountDBCache &accountCache) const;

    virtual bool CheckTx(CTxExecuteContext &context);
    virtual bool ExecuteTx(CTxExecuteContext &context);
};

/**################################ Universal Coin Transfer ########################################**/

struct SingleTransfer {
    CUserID to_uid;
    TokenSymbol coin_symbol = SYMB::WICC;
    uint64_t coin_amount = 0;

    SingleTransfer() {}

    SingleTransfer(const CUserID &toUidIn, const TokenSymbol &coinSymbol, const uint64_t coinAmount)
        : to_uid(toUidIn), coin_symbol(coinSymbol), coin_amount(coinAmount) {}

    IMPLEMENT_SERIALIZE(
        READWRITE(to_uid);
        READWRITE(coin_symbol);
        READWRITE(VARINT(coin_amount));
    )
    string ToString(const CAccountDBCache &accountCache) const ;

    Object ToJson(const CAccountDBCache &accountCache) const;
};


/**
 * Universal Coin Transfer Tx
 *
 */
class CCoinTransferTx: public CBaseTx {
public:
    vector<SingleTransfer> transfers;
    string memo;

public:
    CCoinTransferTx()
        : CBaseTx(UCOIN_TRANSFER_TX) {}

    CCoinTransferTx(const CUserID &txUidIn, const CUserID &toUidIn, const int32_t validHeightIn,
                    const TokenSymbol &coinSymbol, const uint64_t coinAmount,
                    const TokenSymbol &feeSymbol, const uint64_t feesIn, const string &memoIn)
        : CBaseTx(UCOIN_TRANSFER_TX, txUidIn, validHeightIn, feeSymbol, feesIn),
          transfers( { {toUidIn, coinSymbol, coinAmount} } ),
          memo(memoIn) {}

    ~CCoinTransferTx() {}

    IMPLEMENT_SERIALIZE(
        READWRITE(VARINT(this->nVersion));
        nVersion = this->nVersion;
        READWRITE(VARINT(valid_height));
        READWRITE(txUid);
        READWRITE(fee_symbol);
        READWRITE(VARINT(llFees));
        READWRITE(transfers);
        READWRITE(memo);
        READWRITE(signature);
    )

    TxID ComputeSignatureHash(bool recalculate = false) const {
        if (recalculate || sigHash.IsNull()) {
            CHashWriter ss(SER_GETHASH, 0);
            ss << VARINT(nVersion) << uint8_t(nTxType) << VARINT(valid_height) << txUid
               << fee_symbol << VARINT(llFees) << transfers << memo;

            sigHash = ss.GetHash();
        }

        return sigHash;
    }

    virtual std::shared_ptr<CBaseTx> GetNewInstance() const { return std::make_shared<CCoinTransferTx>(*this); }
    virtual string ToString(CAccountDBCache &accountCache);
    virtual Object ToJson(const CAccountDBCache &accountCache) const;

    virtual bool CheckTx(CTxExecuteContext &context);
    virtual bool ExecuteTx(CTxExecuteContext &context);
};

#endif // TX_COIN_TRANSFER_H