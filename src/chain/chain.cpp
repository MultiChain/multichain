// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "chain/blockmap.h"
#include "chain/chain.h"
#include "core/main.h"
#include "storage/txdb.h"

using namespace std;

void CChainStorage::zero()
{
    vChain.clear();
    
    nSize=0;
    mapLoaded.clear();
    vHash.clear();
    vLastUsed.clear();
    m_Semaphore=NULL;           
    mapModified.clear();
    
    m_Mode=0;
    fInMemory=true;    
}

int CChainStorage::init(uint32_t mode,int maxsize)
{
    m_Mode=mode;
    nCacheSize=maxsize;
    fInMemory=true;
    
    if(m_Mode & MC_BMM_LIMITED_SIZE)
    {
        fInMemory=false;
        
        if(maxsize < 1)
        {
            LogPrintf("Active Chain: Invalid cache size: %d\n",maxsize);            
            return MC_ERR_INTERNAL_ERROR;
        }
        
        LogPrintf("Active Chain: Cache size: %d\n",maxsize);
    }
    
    m_Semaphore=__US_SemCreate();
    if(m_Semaphore == NULL)
    {
        LogPrintf("Active Chain: Cannot initialize semaphore");
        return MC_ERR_INTERNAL_ERROR;
    }
       
    vHeight.resize(nCacheSize,-1);
    vHash.resize(nCacheSize,0);
    vLastUsed.resize(nCacheSize,0);
    
    return MC_ERR_NOERROR;
    
}

void CChainStorage::destroy()
{
    if(m_Semaphore)
    {
        __US_SemDestroy(m_Semaphore);
    }
    
    zero();    
}

void CChainStorage::lock()
{
    __US_SemWait(m_Semaphore);         
}

void CChainStorage::unlock()
{
    __US_SemPost(m_Semaphore);        
}

int CChainStorage::load(int nHeight)
{
    int64_t time_now=GetTimeMicros();
    
    map<int,int>::iterator it=mapLoaded.find(nHeight);
    
    if(it != mapLoaded.end())
    {
        vLastUsed[it->second]=time_now;
        return it->second;
    }
    
    int64_t min_time=time_now;
    int min_index=-1;
    
    for(int i=0;i<nCacheSize;i++)
    {
        if(vLastUsed[i] <= min_time)
        {
            min_time=vLastUsed[i];
            min_index=i;
        }
    }
    
    if(min_index < 0)
    {
        return min_index;
    }
    
    uint256 hash;
    
    if(!pblocktree->ReadChainActiveHash(nHeight,hash))
    {
        return -1;
    }
        
    mapLoaded.erase(vHeight[min_index]);
    
    vLastUsed[min_index]=time_now;
    vHeight[min_index]=nHeight;
    vHash[min_index]=hash;
    
    mapLoaded.insert(make_pair(nHeight,min_index));

    return min_index;
}


CBlockIndex *CChainStorage::getptr(int nHeight)
{
    return mapBlockIndex[gethash(nHeight)];
}

void CChainStorage::setptr(int nHeight, CBlockIndex * ptr)
{
    if(ptr)
    {
        sethash(nHeight,ptr->GetBlockHash());
    }
    else
    {
        sethash(nHeight,0);
    }
}

uint256 CChainStorage::gethash(int nHeight)
{    
    if(fInMemory)
    {    
        if (nHeight < 0 || nHeight >= (int)vChain.size())
            return 0;
        
        return vChain[nHeight];
    }
        
    uint256 hash=0;
    int index;
    
    lock();
    
    if (nHeight >= 0 && nHeight < nSize)
    {    
        map<int,uint256>::iterator it=mapModified.find(nHeight);

        if(it != mapModified.end())
        {
            hash=it->second;
        }
        else
        {
            index=load(nHeight);

            if(index>=0)
            {
                hash=vHash[index];
            }
        }
    }
    
    unlock();
    
    return hash;
}

void CChainStorage::sethash(int nHeight, uint256 hash)
{
    if(fInMemory)
    {    
        if (nHeight < 0 || nHeight >= (int)vChain.size())
            return;
    
        vChain[nHeight] = hash;    
        return;
    }
    
    lock();
    
    mapModified.insert(make_pair(nHeight,hash));
    LogPrint("mcblin","ActiveChain: Inserting : %8d (%s)\n",nHeight,hash.ToString().c_str());
    
    unlock();
}

int CChainStorage::getsize() const
{
    if(fInMemory)
    {
        return (int)vChain.size();    
    }
    
    return nSize;
}

void CChainStorage::setsize(int size)
{
    if (size < 0)
        return;
    
    if(fInMemory)
    {
        if(size == 0)
        {
            vChain.clear();
            return;
        }
    
        vChain.resize(size);
        return;
    }
    
    lock();
    
    if(size)
    {
        for(int h=size;h<nSize;h++)
        {
            map<int,int>::iterator it=mapLoaded.find(h);
            if(it != mapLoaded.end())
            {
                vLastUsed[it->second]=0;
                vHeight[it->second]=-1;
                vHash[it->second]=0;
                mapLoaded.erase(it);
            }
            
            map<int,uint256>::iterator mit=mapModified.find(h);
            if(mit != mapModified.end())
            {
                mit->second = 0;
            }                        
        }
    }
            
    nSize=size;
    
    unlock();
}

bool CChainStorage::flush()
{
    if(fInMemory)
    {
        return true;
    }
    
    bool result=true;
            
    lock();
    
    
    BOOST_FOREACH(PAIRTYPE(const int, uint256)& item, mapModified)
    {
        LogPrint("mcblin","ActiveChain: Saving    : %8d (%s)\n",item.first,item.second.ToString().c_str());
        result &= pblocktree->WriteChainActiveHash(item.first,item.second);
    }
    
    if(result)
    {
        mapModified.clear();
    }
    
    unlock();
    
    return result;
}

/**
 * CChain implementation
 */

CBlockIndex * CChain::Genesis()  {
    return cChain.getsize() > 0 ? cChain.getptr(0) : NULL;
//        return vChain.size() > 0 ? vChain[0] : NULL;
}

/** Returns the index entry for the tip of this chain, or NULL if none. */
CBlockIndex * CChain::Tip()  {
    return cChain.getsize() > 0 ? cChain.getptr(cChain.getsize() - 1) : NULL;
//        return vChain.size() > 0 ? vChain[vChain.size() - 1] : NULL;
}

/** Returns the index entry at a particular height in this chain, or NULL if no such height exists. */
CBlockIndex * CChain::operator[](int nHeight)  {
    return cChain.getptr(nHeight);

//        if (nHeight < 0 || nHeight >= (int)vChain.size())
//            return NULL;
//        return vChain[nHeight];
}


/** Efficiently check whether a block is present in this chain. */
bool  CChain::Contains(const CBlockIndex *pindex)  {
    return cChain.gethash(pindex->nHeight) == pindex->GetBlockHash();
//        return (*this)[pindex->nHeight] == pindex;
}

/** Find the successor of a block in this chain, or NULL if the given index is not found or is the tip. */
CBlockIndex *CChain::Next(const CBlockIndex *pindex)  {
    if (Contains(pindex))
        return cChain.getptr(pindex->nHeight + 1);
//            return (*this)[pindex->nHeight + 1];
    else
        return NULL;
}

/** Return the maximal height in the chain. Is equal to chain.Tip() ? chain.Tip()->nHeight : -1. */
int  CChain::Height() const {
    return cChain.getsize() - 1;
//        return vChain.size() - 1;
}

void CChain::SetTip(CBlockIndex *pindex) {
    mc_gState->ChainLock();
    if (pindex == NULL) {
        cChain.setsize(0);
//        vChain.clear();
        mc_gState->ChainUnLock();
        return;
    }
/* BLMP Full scan on initialization if hashes not stored, if flush here make sure pindex is not required in caller */    
    cChain.setsize(pindex->nHeight + 1);
    while (pindex && cChain.gethash(pindex->nHeight) != pindex->GetBlockHash()) {
        cChain.setptr(pindex->nHeight,pindex);
        pindex = pindex->getpprev();
    }
    
//    vChain.resize(pindex->nHeight + 1);
//    while (pindex && vChain[pindex->nHeight] != pindex) {
//        vChain[pindex->nHeight] = pindex;
//        pindex = pindex->getpprev();
//    }
    mc_gState->ChainUnLock();
}

bool CChain::Flush()
{
    return cChain.flush();
}

int CChain::InitStorage(uint32_t mode,int maxsize)
{
    return cChain.init(mode,maxsize);
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


