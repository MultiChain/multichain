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
    m_MapThreads.clear();
    fInMemory=true;
    m_ChangeCount=0;
    for(int i=0;i<64;i++)
    {
        m_SoftEntry[i]=NULL;
    }
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

    for(int i=0;i<64;i++)
    {
        m_SoftEntry[i]=new CBlockIndex();
    }
    return MC_ERR_NOERROR;
}

void CBlockMap::destroy()
{
    for(int i=0;i<64;i++)
    {
        if(m_SoftEntry[i])
        {
            delete m_SoftEntry[i];
        }
    }
    
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

int CBlockMap::getthreadid()
{
    uint64_t this_thread=__US_ThreadID();
    std::map <uint64_t, int >::iterator thit=m_MapThreads.find(this_thread);
    if(thit == m_MapThreads.end())
    {
        if((int)m_MapThreads.size() >= 64)
        {
            LogPrintf("ERROR: Too many block index threads\n");            
            return -1;
        }
        m_MapThreads.insert(make_pair(this_thread,(int)m_MapThreads.size()));
    }
    return thit->second;
}

uint64_t CBlockMap::getthreadbit()
{
    return 1 << getthreadid();
}

bool CBlockMap::load(uint256 hash)
{
    
    int64_t time_now=GetTimeMicros();
    
    BlockMap::iterator it=m_MapBlockIndex.find(hash);
    if(it != m_MapBlockIndex.end())
    {
        it->second->nLastUsed=time_now;
        it->second->nLockedByThread |= getthreadbit();
        return true;
    }
    
//    LogPrint("mcblin","Block Index: Loading   : %s\n",hash.ToString().c_str());
    CBlockIndex *pindex=pblocktree->ReadBlockIndex(hash);
    
    if(pindex == NULL)
    {
        return false;
    }
    
    pindex->nLastUsed=time_now;
    pindex->nLockedByThread |= getthreadbit();
    m_ChangeCount++;
    
    m_MapBlockIndex.insert(make_pair(hash, pindex));
    pindex->hashBlock=hash;
    
//    printf("Load: %4d -> %s\n",pindex->nHeight,pindex->phashBlock->ToString().c_str());
    
    LogPrint("mcblin","Block Index: Loaded    : %8d (%s)\n",pindex->nHeight,pindex->hashBlock.ToString().c_str());
    return true;
}

bool CBlockMap::unload(uint256 hash)
{
    BlockMap::iterator mi = m_MapBlockIndex.find(hash);
    if (mi != m_MapBlockIndex.end())
    {
        LogPrint("mcblin","Block Index: Unloading : %8d (%s)\n",mi->second->nHeight,mi->first.ToString().c_str());
        delete mi->second;
        m_ChangeCount++;
        m_MapBlockIndex.erase(mi);
    }
    return true;
}

void CBlockMap::defragment()
{
    return;    
    if(m_ChangeCount < 1000)
    {
        return;
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
    
    return find(hash);
            
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

CBlockIndex* CBlockMap::softfind(uint256 hash) 
{
    BlockMap::iterator mi;
    
    lock();
    
    CBlockIndex *presult=NULL;
    
    mi = m_MapBlockIndex.find(hash);
    if(mi != m_MapBlockIndex.end())
    {
        presult=mi->second;
        presult->nLockedByThread |= getthreadbit();
//    LogPrintf("Block Index: soft : %s\n",hash.ToString().c_str());
    }        
    else
    {
        int thread_id=getthreadid();
        if(thread_id >= 0)
        {
            presult=m_SoftEntry[thread_id];                        
        }
        presult=pblocktree->ReadBlockIndex(hash,presult);
    }
    
    unlock();
    
    if(presult == NULL)
    {
        LogPrintf("ERROR: Block Index: couldn't retrieve block %s\n",hash.ToString().c_str());        
    }
    
    return presult;
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

    CBlockIndex *presult=NULL;
    
    lock();

    mi=m_MapBlockIndex.end();
    if(load(hash))
    {
        mi=m_MapBlockIndex.find(hash);
        if(mi != m_MapBlockIndex.end())
        {
            presult=mi->second;
        }
    }
    
    unlock();
    
    return presult;
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

    x.second->fUpdated = true;
    x.second->nLockedByThread |= getthreadbit();
    
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
    
    m_MapThreads.clear();
    
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
    
    int64_t time_before_lock=GetTimeMillis();
    
    lock();

    int64_t time_after_lock=GetTimeMillis();
    
    if(time_after_lock-time_before_lock > 2000)
    {
        LogPrint("mcblin","Waiting for index flush for %dms, thread %lu\n",time_after_lock-time_before_lock, __US_ThreadID());        
    }
    
    vector<pair<int64_t, uint256> > vSortedByTime;    
    
    int map_size=(int)m_MapBlockIndex.size();
    int start_map_size=map_size;
    uint64_t threadbit=getthreadbit();
    uint64_t notthreadbit=~threadbit;
            
    BOOST_FOREACH(PAIRTYPE(uint256, CBlockIndex*) item, m_MapBlockIndex)
    {
        item.second->nLockedByThread &= notthreadbit;
    }
    
    if(map_size > m_MaxSize)
    {    
        size_t locked_hashes=0;
        BOOST_FOREACH(const PAIRTYPE(uint256, CBlockIndex*)& item, m_MapBlockIndex)
        {
            if(!item.second->fUpdated)
            {
                if(item.second->nLockedByThread == 0)
                {
                    vSortedByTime.push_back(make_pair(item.second->nLastUsed, item.first));
                }
            }
        }
        
        locked_hashes=m_MapBlockIndex.size()-vSortedByTime.size();
        
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
        
        LogPrint("mcblin","Flushed block index, %d -> %d, locked %u, thread %lu\n",start_map_size,(int)m_MapBlockIndex.size(),locked_hashes,__US_ThreadID());
    }
    
    defragment();
    
    unlock();
}
