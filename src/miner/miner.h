// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2013 The Bitcoin developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#ifndef BITCOIN_MINER_H
#define BITCOIN_MINER_H

#include <stdint.h>

#include <script/standard.h>
#include <set>


class CBlock;
class CBlockHeader;
class CBlockIndex;
class CReserveKey;
class CScript;
class CWallet;

struct CBlockTemplate;

#define MC_MST_NO_MINER            0x00000000
#define MC_MST_MINING              0x00000001
#define MC_MST_NO_PEERS            0x00000002
#define MC_MST_NO_TXS              0x00000004
#define MC_MST_PAUSED              0x00000008
#define MC_MST_SLEEPING            0x00000010
#define MC_MST_NO_LOCKED_BLOCK     0x00000020
#define MC_MST_BAD_VERSION         0x00000040
#define MC_MST_REINDEX             0x00000080
#define MC_MST_RECENT              0x00000100
#define MC_MST_FOUND               0x00000200
#define MC_MST_DRIFT               0x00000400
#define MC_MST_MINER_MASK          0x00000F00
#define MC_MST_MINER_READY         0x00001000
#define MC_MST_MANY_MINERS         0x00002000
#define MC_MST_PROC_MASK           0x0000F000

uint32_t mc_GetMiningStatus(CPubKey &miner);

/** Run the miner threads */
void GenerateBitcoins(bool fGenerate, CWallet* pwallet, int nThreads);
/** Generate a new block, without valid proof-of-work */
CBlockTemplate* CreateNewBlock(const CScript& scriptPubKeyIn);
CBlockTemplate* CreateNewBlockWithKey(CReserveKey& reservekey);
CBlockTemplate* CreateNewBlockWithDefaultKey(CWallet *pwallet,int *canMine, const std::set<CTxDestination>* addresses = NULL,CBlockIndex** ppPrev = NULL);
/** Modify the extranonce in a block */
/* MCHN START */
void IncrementExtraNonce(CBlock* pblock, CBlockIndex* pindexPrev, unsigned int& nExtraNonce,CWallet *pwallet);
bool CreateBlockSignature(CBlock *block,uint32_t hash_type,CWallet *pwallet,uint256 *cachedMerkleRoot);
//void IncrementExtraNonce(CBlock* pblock, CBlockIndex* pindexPrev, unsigned int& nExtraNonce);
/* MCHN END */
/** Check mined block */
bool CheckWork(CBlock* pblock, CWallet& wallet, CReserveKey& reservekey);
bool UpdateTime(CBlockHeader* block, const CBlockIndex* pindexPrev);

extern double dHashesPerSec;
extern int64_t nHPSTimerStart;

#endif // BITCOIN_MINER_H
