// Copyright (c) 2014-2022 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#ifndef MULTICHAIN_BLOCKMAP_H
#define MULTICHAIN_BLOCKMAP_H

#include "structs/uint256.h"
#include <boost/unordered_map.hpp>

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
    
    BlockMap mapBlockIndex;

public:
    
    CBlockIndex *operator[](uint256 hash);
    size_t count(uint256 hash);    
    BlockMap::iterator find(uint256 hash);
    BlockMap::iterator begin();
    BlockMap::iterator end();
    BlockMap::iterator next(BlockMap::iterator& it);
    std::pair<BlockMap::iterator, bool> insert(std::pair<uint256, CBlockIndex*> x);
    bool empty();
    size_t size();
    void clear();
};


#endif /* MULTICHAIN_BLOCKMAP_H */

