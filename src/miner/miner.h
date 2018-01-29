// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2013 The Bitcoin developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2017 Coin Sciences Ltd
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

/** Run the miner threads */
void GenerateBitcoins(bool fGenerate, CWallet* pwallet, int nThreads);
/** Generate a new block, without valid proof-of-work */
CBlockTemplate* CreateNewBlock(const CScript& scriptPubKeyIn);
CBlockTemplate* CreateNewBlockWithKey(CReserveKey& reservekey);
CBlockTemplate* CreateNewBlockWithDefaultKey(CWallet *pwallet,int *canMine, const std::set<CTxDestination>* addresses = NULL,CBlockIndex** ppPrev = NULL);
/** Modify the extranonce in a block */
/* MCHN START */
void IncrementExtraNonce(CBlock* pblock, CBlockIndex* pindexPrev, unsigned int& nExtraNonce,CWallet *pwallet);
//void IncrementExtraNonce(CBlock* pblock, CBlockIndex* pindexPrev, unsigned int& nExtraNonce);
/* MCHN END */
/** Check mined block */
bool CheckWork(CBlock* pblock, CWallet& wallet, CReserveKey& reservekey);
bool UpdateTime(CBlockHeader* block, const CBlockIndex* pindexPrev);

extern double dHashesPerSec;
extern int64_t nHPSTimerStart;

#endif // BITCOIN_MINER_H
