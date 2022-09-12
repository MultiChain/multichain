// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#ifndef BITCOIN_TXDB_H
#define BITCOIN_TXDB_H

#include "storage/leveldbwrapper.h"
#include "core/main.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

class CCoins;
class uint256;

//! -dbcache default (MiB)
static const int64_t nDefaultDbCache = 100;
//! max. -dbcache in (MiB)
static const int64_t nMaxDbCache = sizeof(void*) > 4 ? 4096 : 1024;
//! min. -dbcache in (MiB)
static const int64_t nMinDbCache = 4;

/** CCoinsView backed by the LevelDB coin database (chainstate/) */
class CCoinsViewDB : public CCoinsView
{
protected:
    CLevelDBWrapper db;
public:
    CCoinsViewDB(size_t nCacheSize, bool fMemory = false, bool fWipe = false);

    bool GetCoins(const uint256 &txid, CCoins &coins) const;
    bool HaveCoins(const uint256 &txid) const;
    uint256 GetBestBlock() const;
    bool BatchWrite(CCoinsMap &mapCoins, const uint256 &hashBlock);
    bool GetStats(CCoinsStats &stats) const;
};

class CBlockList
{
    private: 
        mc_Buffer *m_BlockList;
        int *m_Pos;
        int *m_Height;
        int *m_First;
        
        void Sort(int from_height,int to_height,int from, int to);
    public:
        CBlockList()
        {
            Zero();
        }
        ~CBlockList()
        {
            Destroy();
        }
        
        void Zero();
        int Init();
        void Destroy();
        
        int Add(uint256 hash,CBlockIndex *pindex);
        int Sort();
        CBlockIndex* GetBlockIndex(int height,uint256 hash);
        int GetSize();
        CBlockIndex *GetBlockIndex(int r);        
};

/** Access to the block database (blocks/index/) */
class CBlockTreeDB : public CLevelDBWrapper
{
public:
    CBlockTreeDB(size_t nCacheSize, bool fMemory = false, bool fWipe = false);
private:
    CBlockTreeDB(const CBlockTreeDB&);
    void operator=(const CBlockTreeDB&);
public:
    CBlockIndex *ReadBlockIndex(uint256 hash);
    bool WriteBlockIndex(const CDiskBlockIndex& blockindex);
    bool ReadBlockFileInfo(int nFile, CBlockFileInfo &fileinfo);
    bool WriteBlockFileInfo(int nFile, const CBlockFileInfo &fileinfo);
    bool ReadLastBlockFile(int &nFile);
    bool WriteLastBlockFile(int nFile);
    bool WriteReindexing(bool fReindex);
    bool ReadReindexing(bool &fReindex);
    bool ReadTxIndex(const uint256 &txid, CDiskTxPos &pos);
    bool WriteTxIndex(const std::vector<std::pair<uint256, CDiskTxPos> > &list);
    bool WriteFlag(const std::string &name, bool fValue);
    bool ReadFlag(const std::string &name, bool &fValue);
    bool UpdateBlockCacheValues();
    bool LoadBlockIndexGuts();
    bool WriteBlockCachedStatus(bool erase);
    bool ReadChainActiveHash(int height, uint256& hash);
    bool WriteChainActiveHash(int height, uint256 hash);
};

#endif // BITCOIN_TXDB_H
