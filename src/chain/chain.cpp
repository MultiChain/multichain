// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "chain/chain.h"

using namespace std;

/**
 * CChain implementation
 */
void CChain::SetTip(CBlockIndex *pindex) {
    mc_gState->ChainLock();
    if (pindex == NULL) {
        vChain.clear();
        mc_gState->ChainUnLock();
        return;
    }
    vChain.resize(pindex->nHeight + 1);
    while (pindex && vChain[pindex->nHeight] != pindex) {
        vChain[pindex->nHeight] = pindex;
        pindex = pindex->getpprev();
    }
    mc_gState->ChainUnLock();
}

CBlockLocator CChain::GetLocator(const CBlockIndex *pindex) const {
    int nStep = 1;
    std::vector<uint256> vHave;
    vHave.reserve(32);

    if (!pindex)
        pindex = Tip();
    while (pindex) {
        vHave.push_back(pindex->GetBlockHash());
        // Stop when we have added the genesis block.
        if (pindex->nHeight == 0)
            break;
        // Exponentially larger steps back, plus the genesis block.
        int nHeight = std::max(pindex->nHeight - nStep, 0);
        if (Contains(pindex)) {
            // Use O(1) CChain index if possible.
            pindex = (*this)[nHeight];
        } else {
            // Otherwise, use O(log n) skiplist.
            pindex = pindex->GetAncestor(nHeight);
        }
        if (vHave.size() > 10)
            nStep *= 2;
    }

    return CBlockLocator(vHave);
}

/* BLMP removed const for both method and parameter */    

CBlockIndex *CChain::FindFork(CBlockIndex *pindexIn) {
    CBlockIndex *pindex=pindexIn;
    if (pindex->nHeight > Height())
        pindex = pindex->GetAncestor(Height());
    while (pindex && !Contains(pindex))
        pindex = pindex->getpprev();
    return pindex;
}

CBlockIndex* CBlockIndex::getpprev() {
    return pprev;
}

void CBlockIndex::setpprev(CBlockIndex* p){
    pprev=p;
    hashPrev=0;
    if(pprev)
    {
        hashPrev=pprev->GetBlockHash();
    }
}

CBlockIndex* CBlockIndex::getpskip(){
    return pskip;
}

void CBlockIndex::setpskip(CBlockIndex* p){
    pskip=p;
    hashSkip=0;
    if(pskip)
    {
        hashSkip=pskip->GetBlockHash();
    }
}

CBlockIndex* CBlockIndex::getnextonthisheight(){
    return pNextOnThisHeight;
}

void CBlockIndex::setnextonthisheight(CBlockIndex* p){
    pNextOnThisHeight=p;
    hashNextOnThisHeight=0;
    if(pskip)
    {
        hashNextOnThisHeight=pNextOnThisHeight->GetBlockHash();
    }
}


