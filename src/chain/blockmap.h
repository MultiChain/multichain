// Copyright (c) 2014-2022 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#ifndef MULTICHAIN_BLOCKMAP_H
#define MULTICHAIN_BLOCKMAP_H

#include "structs/uint256.h"
#include <boost/unordered_map.hpp>

#define MC_BMM_NONE                             0x00000000
#define MC_BMM_LIMITED_SIZE                     0x00000001

class CBlockIndex;

struct BlockHasher
{
    size_t operator()(const uint256& hash) const { return hash.GetLow64(); }
};

extern int nBlockIndexCacheSize;

typedef boost::unordered_map<uint256, CBlockIndex*, BlockHasher> BlockMap;

class CCachedBlockIndex{
private:
    uint256 m_Hash;
    CBlockIndex *m_PIndex;
public:
    CCachedBlockIndex(CBlockIndex* pindex)
    {
        setpindex(pindex);
    }
    
    CBlockIndex* getpindex();     
    void setpindex(CBlockIndex* pindex);     
    CBlockIndex *operator=(CBlockIndex* pindex);
};

class CBlockMap {
private:
    
    BlockMap m_MapBlockIndex;
    std::map <uint64_t, int> m_MapThreads;
    void *m_Semaphore;                                                          
    int m_MaxSize;
    uint32_t m_Mode;
    bool fInMemory;
    int m_ChangeCount;

public:
    
    CBlockMap()
    {
        zero();
    }
    
    ~CBlockMap()
    {
        destroy();
    }
    
    void zero();
    int init(uint32_t mode,int maxsize);
    void destroy();
    void lock();
    void unlock();
    bool load(uint256 hash);
    bool unload(uint256 hash);
    uint64_t getthreadbit();
    void defragment();
    
    CBlockIndex *operator[](uint256 hash);
    size_t count(uint256 hash);    
    CBlockIndex *find(uint256 hash);
    CBlockIndex *softfind(uint256 hash,bool *fNotInMap);
    BlockMap::iterator begin();
    BlockMap::iterator end();
    BlockMap::iterator next(BlockMap::iterator& it);
    void insert(std::pair<uint256, CBlockIndex*> x);
    bool empty();
    size_t size();
    void clear();
    void flush();
};

extern CBlockMap mapBlockIndex;

#endif /* MULTICHAIN_BLOCKMAP_H */

