// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2017-2019 The WaykiChain Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef COIN_MINER_H
#define COIN_MINER_H

#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <tuple>
#include <vector>

#include "entities/key.h"
#include "commons/uint256.h"
#include "tx/tx.h"

class CBlock;
class CBlockIndex;
class CWallet;
class CBaseTx;
class CAccountDBCache;
class CAccount;

#include <cmath>

using namespace std;

struct TxPriority {
    double priority;
    double feePerKb;
    std::shared_ptr<CBaseTx> baseTx;

    TxPriority(const double priorityIn, const double feePerKbIn, const std::shared_ptr<CBaseTx> &baseTxIn)
        : priority(priorityIn), feePerKb(feePerKbIn), baseTx(baseTxIn) {}

    bool operator<(const TxPriority &other) const {
        if (fabs(this->priority - other.priority) <= 1000) {
            if (fabs(this->feePerKb < other.feePerKb) <= 1e-8) {
                return this->baseTx->GetHash() < other.baseTx->GetHash();
            } else {
                return this->feePerKb < other.feePerKb;
            }
        } else {
            return this->priority < other.priority;
        }
    }
};

// mined block info
class MinedBlockInfo {
public:
    int64_t time;             // block time
    int64_t nonce;            // nonce
    int32_t height;               // block height
    uint64_t totalFuel;       // the total fuels of all transactions in the block
    uint fuelRate;            // block fuel rate
    uint64_t totalFees;       // the total fees of all transactions in the block
    uint64_t txCount;         // transaction count in block, exclude coinbase
    uint64_t totalBlockSize;  // block size(bytes)
    uint256 hash;             // block hash
    uint256 hashPrevBlock;    // prev block has

public:
    MinedBlockInfo() { SetNull(); }
    void SetNull();
};

// get the info of mined blocks. thread safe.
vector<MinedBlockInfo> GetMinedBlocks(uint32_t count);

/** Run the miner threads */
void GenerateCoinBlock(bool fGenerate, CWallet *pWallet, int32_t nThreads);

/** Generate a new block pre-stable coin release */
std::unique_ptr<CBlock> CreateNewBlockPreStableCoinRelease(CCacheWrapper &cwIn);
/** Generate fund coin's genesis block */
std::unique_ptr<CBlock> CreateStableCoinGenesisBlock();
/** Generate a new block after stable coin release */
std::unique_ptr<CBlock> CreateNewBlockStableCoinRelease(CCacheWrapper &cwIn);

bool CreateBlockRewardTx(const int64_t currentTime, const CAccount &delegate, CAccountDBCache &accountCache,
                         CBlock *pBlock);

bool VerifyRewardTx(const CBlock *pBlock, CCacheWrapper &cwIn, bool bNeedRunTx = false);

/** Check mined block */
bool CheckWork(CBlock *pBlock, CWallet &wallet);

/** Get burn element */
uint32_t GetElementForBurn(CBlockIndex *pIndex);

void GetPriorityTx(vector<TxPriority> &vecPriority, int32_t nFuelRate);

#endif  // COIN_MINER_H
