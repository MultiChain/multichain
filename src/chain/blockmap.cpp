// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "chain/blockmap.h"
#include "core/main.h"
#include "storage/txdb.h"

using namespace std;

int nBlockIndexCacheSize = DEFAULT_MAX_BLOCK_INDEX_CACHE_SIZE;

CBlockIndex* CCachedBlockIndex::getpindex()
{
    return m_PIndex;
}

void CCachedBlockIndex::setpindex(CBlockIndex* pindex)
{
    m_Hash = 0;
    if(pindex)
    {
        m_Hash = pindex->GetBlockHash();
    }
    m_PIndex = pindex;
}

CBlockIndex *CCachedBlockIndex::operator=(CBlockIndex* pindex)
{
    setpindex(pindex);
    return m_PIndex;
}

void CBlockMap::zero()
{
    m_Mode = MC_BMM_NONE;
    m_MaxSize = 0;
    m_Semaphore = NULL;
    m_MapBlockIndex.clear();
    m_MapLocked.clear();
    fInMemory=true;
    m_ChangeCount=0;
}

int CBlockMap::init(uint32_t mode,int maxsize)
{
    m_Mode=mode;
    m_MaxSize=maxsize;
    fInMemory=true;
    
    if(m_Mode & MC_BMM_LIMITED_SIZE)
    {
        fInMemory=false;
        LogPrintf("Block Index: Max size: %d\n",maxsize);
    }
    
    m_Semaphore=__US_SemCreate();
    if(m_Semaphore == NULL)
    {
        LogPrintf("Block Index: Cannot initialize semaphore");
        return MC_ERR_INTERNAL_ERROR;
    }
        
    return MC_ERR_NOERROR;
}

void CBlockMap::destroy()
{
    if(m_Semaphore)
    {
        __US_SemDestroy(m_Semaphore);
    }
    
    zero();
}

void CBlockMap::lock()
{
    __US_SemWait(m_Semaphore);     
}

void CBlockMap::unlock()
{
    __US_SemPost(m_Semaphore);    
}

void CBlockMap::lockhash(uint256 hash)
{
    uint64_t this_thread=__US_ThreadID();
    std::map <uint64_t, std::set <uint256> >::iterator thit=m_MapLocked.find(this_thread);
    std::set <uint256> emptySet;
    if(thit == m_MapLocked.end())
    {
        m_MapLocked.insert(make_pair(this_thread,emptySet));
        thit=m_MapLocked.find(this_thread);
    }
    thit->second.insert(hash);
}

bool CBlockMap::load(uint256 hash)
{
    lockhash(hash);
    
    int64_t time_now=GetTimeMicros();
    
    BlockMap::iterator it=m_MapBlockIndex.find(hash);
    if(it != m_MapBlockIndex.end())
    {
        it->second->nLastUsed=time_now;
        return true;
    }
    
//    LogPrint("mcblin","Block Index: Loading   : %s\n",hash.ToString().c_str());
    CBlockIndex *pindex=pblocktree->ReadBlockIndex(hash);
    
    if(pindex == NULL)
    {
        return false;
    }
    
    pindex->nLastUsed=time_now;
    m_ChangeCount++;
    
    m_MapBlockIndex.insert(make_pair(hash, pindex));
//    pindex->phashBlock = &((*mi).first);
    pindex->hashBlock=hash;
    
//    printf("Load: %4d -> %s\n",pindex->nHeight,pindex->phashBlock->ToString().c_str());
    
    LogPrint("mcblin","Block Index: Loaded    : %8d (%s)\n",pindex->nHeight,pindex->hashBlock.ToString().c_str());
    return true;
}

bool CBlockMap::canunload(uint256 hash)
{
    BOOST_FOREACH(const PAIRTYPE(uint64_t, std::set <uint256>)& item, m_MapLocked)
    {
        if(item.second.find(hash) != item.second.end())
        {
            return false;
        }
    }
    
    return true;
}

bool CBlockMap::unload(uint256 hash)
{
    if(canunload(hash))
    {    
        BlockMap::iterator mi = m_MapBlockIndex.find(hash);
        if (mi != m_MapBlockIndex.end())
        {
            LogPrint("mcblin","Block Index: Unloading : %8d (%s)\n",mi->second->nHeight,mi->first.ToString().c_str());
            delete mi->second;
            m_ChangeCount++;
            m_MapBlockIndex.erase(mi);
//            LogPrint("mcblin","Block Index: Unloaded  : %s\n",hash.ToString().c_str());
        }

        return true;
    }
    
    return false;
}

void CBlockMap::defragment()
{
    return;    
    if(m_ChangeCount < 1000)
    {
        return;
    }
    
    BOOST_FOREACH(PAIRTYPE(uint64_t, std::set <uint256>) item, m_MapLocked)
    {
        set <uint256> setTmp;
        for (set<uint256>::iterator lit = item.second.begin(); lit != item.second.end(); lit++)
        {
            setTmp.insert(*lit);
        }
        
        item.second.swap(setTmp);
    }
    
    BlockMap mapTmp;
    
    BlockMap::iterator it = m_MapBlockIndex.begin();
    while (it != m_MapBlockIndex.end()) 
    {
        mapTmp.insert(make_pair(it->first,it->second));
        it++;
    }
    m_MapBlockIndex.swap(mapTmp);    
    
    
    m_ChangeCount=0;
}


CBlockIndex *CBlockMap::operator[](uint256 hash) {
    if(fInMemory)
    {
        return m_MapBlockIndex[hash];
    }
    
//    LogPrintf("Block Index: op[] : %s\n",hash.ToString().c_str());
    CBlockIndex *pindex=NULL;

    lock();
    
    if(load(hash))
    {
        pindex=m_MapBlockIndex[hash];
    }
    
    unlock();
    
    return pindex;
}

size_t CBlockMap::count(uint256 hash) {
    if(fInMemory)
    {    
        return m_MapBlockIndex.count(hash);
    }
    
//    LogPrintf("Block Index: count: %s\n",hash.ToString().c_str());
    
    size_t result=0;
    
    lock();
    
    if(load(hash))
    {
        result=1;
    }
    
    unlock();
    
    return result;
}

CBlockIndex* CBlockMap::find(uint256 hash) {
    BlockMap::iterator mi;
    if(fInMemory)
    {
        mi = m_MapBlockIndex.find(hash);
        if(mi != m_MapBlockIndex.end())
        {
            return mi->second;
        }        
        return NULL;
    }
    
//    LogPrintf("Block Index: find : %s\n",hash.ToString().c_str());

    CBlockIndex *result=NULL;
    
    lock();

    mi=m_MapBlockIndex.end();
    if(load(hash))
    {
        mi=m_MapBlockIndex.find(hash);
        if(mi != m_MapBlockIndex.end())
        {
            result=mi->second;
        }
    }
    
    unlock();
    
    return result;
}

BlockMap::iterator CBlockMap::begin() {
    if(fInMemory)
    {
        return m_MapBlockIndex.begin();
    }
    
    // Not supported
    return m_MapBlockIndex.end();
}

BlockMap::iterator CBlockMap::end() {
    if(fInMemory)
    {
        return m_MapBlockIndex.end();
    }
    
    // Not supported
    return m_MapBlockIndex.end();
}

BlockMap::iterator CBlockMap::next(BlockMap::iterator& it){
    if(fInMemory)
    {
        it++;
        return it;
    }
    
    // Not supported
    return m_MapBlockIndex.end();
}


void CBlockMap::insert(std::pair<uint256, CBlockIndex*> x) {
    if(fInMemory)
    {
        m_MapBlockIndex.insert(x);
        return;
    }
    
    lock();

    lockhash(x.first);
    x.second->fUpdated = true;
    
    LogPrint("mcblin","Block Index: Inserting : %8d (%s)\n",x.second->nHeight,x.first.ToString().c_str());
    m_MapBlockIndex.insert(x);
    
    unlock();    
}

bool CBlockMap::empty() {
    if(fInMemory)
    {
        return m_MapBlockIndex.empty();
    }
    
    bool result=true;
    
    lock();
    
// Used only in initialization, before first flush
    
    result=m_MapBlockIndex.empty();
    
    unlock();
    
    return result;
}


/* Returns number of cached elements, not full size of the map */
size_t CBlockMap::size() {
    if(fInMemory)
    {
        return m_MapBlockIndex.size();
    }
    
    size_t result=0;
    
    lock();
    
    result=m_MapBlockIndex.size();
    
    unlock();
    
    return result;
}

void CBlockMap::clear() {
    if(fInMemory)
    {    
        BlockMap::iterator it1 = m_MapBlockIndex.begin();
        for (; it1 != m_MapBlockIndex.end(); it1++)
            delete (*it1).second;

        m_MapBlockIndex.clear();
        
        return;
    }    
    
    lock();
    
    m_MapLocked.clear();

    BlockMap::iterator it1 = m_MapBlockIndex.begin();
    for (; it1 != m_MapBlockIndex.end(); it1++)
        delete (*it1).second;

    m_MapBlockIndex.clear();
    
    unlock();
}



void CBlockMap::flush() {
    if(fInMemory)
    {
        return;
    }    
    
    lock();

    vector<pair<int64_t, uint256> > vSortedByTime;    
    std::map <uint64_t, std::set <uint256> >::iterator thit=m_MapLocked.find(__US_ThreadID());
    
    if(thit != m_MapLocked.end())
    {
        thit->second.clear();
    }
    
    int map_size=(int)m_MapBlockIndex.size();
    int start_map_size=map_size;
    
    if(map_size > m_MaxSize)
    {    
        BOOST_FOREACH(const PAIRTYPE(uint256, CBlockIndex*)& item, m_MapBlockIndex)
        {
            if(!item.second->fUpdated)
            {
                if(canunload(item.first))
                {
                    vSortedByTime.push_back(make_pair(item.second->nLastUsed, item.first));
                }
            }
        }

        sort(vSortedByTime.begin(), vSortedByTime.end());
    
        for(int i=0;i<(int)vSortedByTime.size();i++)
        {
            if(map_size > m_MaxSize)
            {    
                if(unload(vSortedByTime[i].second))
                {
                    map_size--;
                }
            }        
        }

        LogPrint("mcblin","Flushed block index, %d -> %d\n",start_map_size,(int)m_MapBlockIndex.size());
    }
    
    defragment();
    
    unlock();
}
