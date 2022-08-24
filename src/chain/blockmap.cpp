// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "chain/blockmap.h"
#include "core/main.h"

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


CBlockIndex *CBlockMap::operator[](uint256 hash) {
    return mapBlockIndex[hash];
}

size_t CBlockMap::count(uint256 hash) {
    return mapBlockIndex.count(hash);
}

BlockMap::iterator CBlockMap::find(uint256 hash) {
    return mapBlockIndex.find(hash);
}

BlockMap::iterator CBlockMap::begin() {
    return mapBlockIndex.begin();
}

BlockMap::iterator CBlockMap::end() {
    return mapBlockIndex.end();
}

BlockMap::iterator CBlockMap::next(BlockMap::iterator& it){
    it++;
    return it;
}


std::pair<BlockMap::iterator, bool> CBlockMap::insert(std::pair<uint256, CBlockIndex*> x) {
    return mapBlockIndex.insert(x);
}

bool CBlockMap::empty() {
    return mapBlockIndex.empty();
}


/* Returns number of cached elements, not full size of the map */
size_t CBlockMap::size() {
    return mapBlockIndex.size();
}

void CBlockMap::clear() {
    
    BlockMap::iterator it1 = mapBlockIndex.begin();
    for (; it1 != mapBlockIndex.end(); it1++)
        delete (*it1).second;
    
    return mapBlockIndex.clear();
}

void CBlockMap::flush() {
    
}
