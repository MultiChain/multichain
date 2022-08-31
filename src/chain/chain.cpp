// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "chain/blockmap.h"
#include "chain/chain.h"

using namespace std;

CBlockIndex *CChainPtrStorage::getptr(int nHeight)
{
    if (nHeight < 0 || nHeight >= getsize())
        return NULL;
    
    return mapBlockIndex[cHashChain.gethash(nHeight)];
}

void CChainPtrStorage::setptr(int nHeight, CBlockIndex * ptr)
{
    if (nHeight < 0 || nHeight >= getsize())
        return;
    
    if(ptr)
    {
        cHashChain.sethash(nHeight,ptr->GetBlockHash());
    }
    else
    {
        cHashChain.sethash(nHeight,0);
    }
}

int CChainPtrStorage::getsize() const
{
    return cHashChain.getsize();
}

void CChainPtrStorage::setsize(int nHeight)
{
    cHashChain.setsize(nHeight);
}

uint256 CChainPtrStorage::gethash(int nHeight)
{
    return cHashChain.gethash(nHeight);
}


uint256 CChainHashStorage::gethash(int nHeight)
{
    if (nHeight < 0 || nHeight >= (int)vChain.size())
        return 0;
    return vChain[nHeight];
}

void CChainHashStorage::sethash(int nHeight, uint256 hash)
{
    if (nHeight < 0 || nHeight >= (int)vChain.size())
        return;
    
    vChain[nHeight] = hash;    
}

int CChainHashStorage::getsize() const
{
    return (int)vChain.size();    
}

void CChainHashStorage::setsize(int nHeight)
{
    if (nHeight < 0)
        return;
    
    if(nHeight == 0)
    {
        vChain.clear();
        return;
    }
    
    vChain.resize(nHeight);
}


/**
 * CChain implementation
 */

CBlockIndex * CChain::Genesis()  {
    return cPtrChain.getsize() > 0 ? cPtrChain.getptr(0) : NULL;
//        return vChain.size() > 0 ? vChain[0] : NULL;
}

/** Returns the index entry for the tip of this chain, or NULL if none. */
CBlockIndex * CChain::Tip()  {
    return cPtrChain.getsize() > 0 ? cPtrChain.getptr(cPtrChain.getsize() - 1) : NULL;
//        return vChain.size() > 0 ? vChain[vChain.size() - 1] : NULL;
}

/** Returns the index entry at a particular height in this chain, or NULL if no such height exists. */
CBlockIndex * CChain::operator[](int nHeight)  {
    return cPtrChain.getptr(nHeight);

//        if (nHeight < 0 || nHeight >= (int)vChain.size())
//            return NULL;
//        return vChain[nHeight];
}


/** Efficiently check whether a block is present in this chain. */
bool  CChain::Contains(const CBlockIndex *pindex)  {
    return cPtrChain.gethash(pindex->nHeight) == pindex->GetBlockHash();
//        return (*this)[pindex->nHeight] == pindex;
}

/** Find the successor of a block in this chain, or NULL if the given index is not found or is the tip. */
CBlockIndex *CChain::Next(const CBlockIndex *pindex)  {
    if (Contains(pindex))
        return cPtrChain.getptr(pindex->nHeight + 1);
//            return (*this)[pindex->nHeight + 1];
    else
        return NULL;
}

/** Return the maximal height in the chain. Is equal to chain.Tip() ? chain.Tip()->nHeight : -1. */
int  CChain::Height() const {
    return cPtrChain.getsize() - 1;
//        return vChain.size() - 1;
}

void CChain::SetTip(CBlockIndex *pindex) {
    mc_gState->ChainLock();
    if (pindex == NULL) {
        cPtrChain.setsize(0);
//        vChain.clear();
        mc_gState->ChainUnLock();
        return;
    }
    
    cPtrChain.setsize(pindex->nHeight + 1);
    while (pindex && cPtrChain.gethash(pindex->nHeight) != pindex->GetBlockHash()) {
        cPtrChain.setptr(pindex->nHeight,pindex);
        pindex = pindex->getpprev();
    }
    
//    vChain.resize(pindex->nHeight + 1);
//    while (pindex && vChain[pindex->nHeight] != pindex) {
//        vChain[pindex->nHeight] = pindex;
//        pindex = pindex->getpprev();
//    }
    mc_gState->ChainUnLock();
}

CBlockLocator CChain::GetLocator(const CBlockIndex *pindex)  {
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
    return mapBlockIndex[hashPrev];
}

void CBlockIndex::setpprev(CBlockIndex* p){
    hashPrev=0;
    if(p)
    {
        hashPrev=p->GetBlockHash();
    }
}

CBlockIndex* CBlockIndex::getpskip(){
    if(hashSkip != 0)
    {
        return mapBlockIndex[hashSkip];
    }
    return NULL;
//    return pskip;
}

void CBlockIndex::setpskip(CBlockIndex* p){
//    pskip=p;
    hashSkip=0;
    if(p)
    {
        hashSkip=p->GetBlockHash();
    }
}

uint256 CChainPiece::TipHash()
{
    return hashTip;
}

int CChainPiece::Height()
{
    return heightTip;
}

void CChainPiece::SetTip(CBlockIndex *pindex)
{
    if(pindex == NULL)
    {
        hashTip = 0;
        heightTip = -1;
        return;
    }
    
    hashTip = pindex->GetBlockHash();
    heightTip = pindex->nHeight;
}


