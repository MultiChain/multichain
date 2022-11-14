// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Original code was distributed under the MIT/X11 software license.
// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "storage/txdb.h"

#include "chain/pow.h"
#include "structs/uint256.h"

#include <stdint.h>

#include <boost/thread.hpp>

using namespace std;

void mc_InitCachedBlockIndex();
bool mc_UpdateBlockCacheValues(std::map<uint256,CBlockIndex>& mapTempBlockIndex);
bool mc_UpdateBlockCacheValues(CBlockList *block_list);

void static BatchWriteCoins(CLevelDBBatch &batch, const uint256 &hash, const CCoins &coins) {
    if (coins.IsPruned())
    {
/* MCHN START */    
        if(fDebug)LogPrint("mccoin", "COIN: DB Erase  %s\n", hash.ToString().c_str());
/* MCHN END */    
        batch.Erase(make_pair('c', hash));
    }
    else
    {
/* MCHN START */    
        if(fDebug)LogPrint("mccoin", "COIN: DB Write  %s\n", hash.ToString().c_str());
/* MCHN END */    
        batch.Write(make_pair('c', hash), coins);
    }
}

void static BatchWriteHashBestChain(CLevelDBBatch &batch, const uint256 &hash) {
    batch.Write('B', hash);
}

CCoinsViewDB::CCoinsViewDB(size_t nCacheSize, bool fMemory, bool fWipe) : db(GetDataDir() / "chainstate", nCacheSize, fMemory, fWipe) {
}

bool CCoinsViewDB::GetCoins(const uint256 &txid, CCoins &coins) const {
    return db.Read(make_pair('c', txid), coins);
}

bool CCoinsViewDB::HaveCoins(const uint256 &txid) const {
    return db.Exists(make_pair('c', txid));
}

uint256 CCoinsViewDB::GetBestBlock() const {
    uint256 hashBestChain;
    if (!db.Read('B', hashBestChain))
        return uint256(0);
    return hashBestChain;
}

bool CCoinsViewDB::BatchWrite(CCoinsMap &mapCoins, const uint256 &hashBlock) {
    CLevelDBBatch batch;
    size_t count = 0;
    size_t changed = 0;
    for (CCoinsMap::iterator it = mapCoins.begin(); it != mapCoins.end();) {
        if (it->second.flags & CCoinsCacheEntry::DIRTY) {
            BatchWriteCoins(batch, it->first, it->second.coins);
            changed++;
        }
        count++;
        CCoinsMap::iterator itOld = it++;
        mapCoins.erase(itOld);
    }
    if (hashBlock != uint256(0))
        BatchWriteHashBestChain(batch, hashBlock);

    if(fDebug)LogPrint("coindb", "Committing %u changed transactions (out of %u) to coin database...\n", (unsigned int)changed, (unsigned int)count);
    return db.WriteBatch(batch);
}

CBlockTreeDB::CBlockTreeDB(size_t nCacheSize, bool fMemory, bool fWipe) : CLevelDBWrapper(GetDataDir() / "blocks" / "index", nCacheSize, fMemory, fWipe) {
}

bool CBlockTreeDB::WriteBlockIndex(const CDiskBlockIndex& blockindex)
{
    return Write(make_pair('b', blockindex.GetBlockHash()), blockindex);
}

bool CBlockTreeDB::WriteBlockFileInfo(int nFile, const CBlockFileInfo &info) {
    return Write(make_pair('f', nFile), info);
}

bool CBlockTreeDB::ReadBlockFileInfo(int nFile, CBlockFileInfo &info) {
    return Read(make_pair('f', nFile), info);
}

bool CBlockTreeDB::WriteLastBlockFile(int nFile) {
    return Write('l', nFile);
}

bool CBlockTreeDB::WriteReindexing(bool fReindexing) {
    if (fReindexing)
        return Write('R', '1');
    else
        return Erase('R');
}

bool CBlockTreeDB::ReadReindexing(bool &fReindexing) {
    fReindexing = Exists('R');
    return true;
}

bool CBlockTreeDB::ReadLastBlockFile(int &nFile) {
    return Read('l', nFile);
}

bool CCoinsViewDB::GetStats(CCoinsStats &stats) const {
    /* It seems that there are no "const iterators" for LevelDB.  Since we
       only need read operations on it, use a const-cast to get around
       that restriction.  */
    boost::scoped_ptr<leveldb::Iterator> pcursor(const_cast<CLevelDBWrapper*>(&db)->NewIterator());
    pcursor->SeekToFirst();

    CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
    stats.hashBlock = GetBestBlock();
    ss << stats.hashBlock;
    CAmount nTotalAmount = 0;
    while (pcursor->Valid()) {
        boost::this_thread::interruption_point();
        try {
            leveldb::Slice slKey = pcursor->key();
            CDataStream ssKey(slKey.data(), slKey.data()+slKey.size(), SER_DISK, CLIENT_VERSION);
            char chType;
            ssKey >> chType;
            if (chType == 'c') {
                leveldb::Slice slValue = pcursor->value();
                CDataStream ssValue(slValue.data(), slValue.data()+slValue.size(), SER_DISK, CLIENT_VERSION);
                CCoins coins;
                ssValue >> coins;
                uint256 txhash;
                ssKey >> txhash;
                ss << txhash;
                ss << VARINT(coins.nVersion);
                ss << (coins.fCoinBase ? 'c' : 'n');
                ss << VARINT(coins.nHeight);
                stats.nTransactions++;
                for (unsigned int i=0; i<coins.vout.size(); i++) {
                    const CTxOut &out = coins.vout[i];
                    if (!out.IsNull()) {
                        stats.nTransactionOutputs++;
                        ss << VARINT(i+1);
                        ss << out;
                        nTotalAmount += out.nValue;
                    }
                }
                stats.nSerializedSize += 32 + slValue.size();
                ss << VARINT(0);
            }
            pcursor->Next();
        } catch (std::exception &e) {
            return error("%s : Deserialize or I/O error - %s", __func__, e.what());
        }
    }
    stats.nHeight = mapBlockIndex.find(GetBestBlock())->nHeight;
    stats.hashSerialized = ss.GetHash();
    stats.nTotalAmount = nTotalAmount;
    return true;
}

bool CBlockTreeDB::ReadTxIndex(const uint256 &txid, CDiskTxPos &pos) {
    return Read(make_pair('t', txid), pos);
}

bool CBlockTreeDB::WriteTxIndex(const std::vector<std::pair<uint256, CDiskTxPos> >&vect) {
    CLevelDBBatch batch;
    for (std::vector<std::pair<uint256,CDiskTxPos> >::const_iterator it=vect.begin(); it!=vect.end(); it++)
        batch.Write(make_pair('t', it->first), it->second);
    return WriteBatch(batch);
}

bool CBlockTreeDB::WriteFlag(const std::string &name, bool fValue) {
    return Write(std::make_pair('F', name), fValue ? '1' : '0');
}

bool CBlockTreeDB::ReadFlag(const std::string &name, bool &fValue) {
    char ch;
    if (!Read(std::make_pair('F', name), ch))
        return false;
    fValue = ch == '1';
    return true;
}

bool CBlockTreeDB::ReadChainActiveHash(int height, uint256& hash)
{
    return Read(std::make_pair('a', height), hash);
}

bool CBlockTreeDB::WriteChainActiveHash(int height, uint256 hash)
{
    if(hash == 0)
    {
        return Erase(std::make_pair('a', height));            
    }
    
    return Write(std::make_pair('a', height), hash);
}

void DiskBlockIndexToCBlockIndex(CBlockIndex* pindexNew,CDiskBlockIndex &diskindex)
{
    pindexNew->nHeight        = diskindex.nHeight;
    pindexNew->nFile          = diskindex.nFile;
    pindexNew->nDataPos       = diskindex.nDataPos;
    pindexNew->nUndoPos       = diskindex.nUndoPos;
    pindexNew->nVersion       = diskindex.nVersion;
    pindexNew->hashMerkleRoot = diskindex.hashMerkleRoot;
    pindexNew->nTime          = diskindex.nTime;
    pindexNew->nBits          = diskindex.nBits;
    pindexNew->nNonce         = diskindex.nNonce;
    pindexNew->nStatus        = diskindex.nStatus;
    pindexNew->nTx            = diskindex.nTx;

    pindexNew->nSize          = diskindex.nSize;
    pindexNew->kMiner         = diskindex.kMiner;

    pindexNew->nChainTx       = diskindex.nChainTx;
    pindexNew->nChainWork     = diskindex.nChainWork;
    pindexNew->hashSkip       = diskindex.hashSkip;    
}

void CBlockList::Zero()
{
    m_BlockList=NULL;
    m_Pos=NULL;
    m_Height=NULL;    
    m_First=NULL;
}

int CBlockList::Init()
{
    m_BlockList=new mc_Buffer;
    return m_BlockList->Initialize(sizeof(uint256),sizeof(uint256)+sizeof(CBlockIndex),MC_BUF_MODE_DEFAULT);
}

void CBlockList::Destroy()
{
    if(m_BlockList)
    {
        delete m_BlockList;
    }
    if(m_Pos)
    {
        delete [] m_Pos;
    }
    if(m_Height)
    {
        delete [] m_Height;
    }
    if(m_First)
    {
        delete [] m_First;
    }

}

int CBlockList::Add(uint256 hash,CBlockIndex *pindex)
{
    return m_BlockList->Add(&hash,pindex);
}

void CBlockList::Sort(int from_height,int to_height,int from, int to)
{
    if(from_height + 1 >= to_height)
    {
        return;
    }
    
    if(from + 1 >= to)
    {
        return;
    }
    
    int left=from;
    int right=to;
    int mid_height=(from_height+to_height)/2;
    int t;
    
    
    while(left<right)
    {
        while( (left<right) && (m_Height[left]<mid_height) )
        {
            left++;
        }

        while( (right>left) && (m_Height[right-1]>=mid_height) )
        {
            right--;
        }
        
        if(left<right)
        {
            t=m_Height[right-1];
            m_Height[right-1]=m_Height[left];
            m_Height[left]=t;
            
            t=m_Pos[right-1];
            m_Pos[right-1]=m_Pos[left];
            m_Pos[left]=t;            
        }
    }
    
    Sort(from_height,mid_height,from,left);
    Sort(mid_height,to_height,left,to);
}

int CBlockList::Sort()
{
    int max_height=0;
    m_Pos=new int[m_BlockList->GetCount()];
    m_Height=new int[m_BlockList->GetCount()];
    
    for(int r=0;r<m_BlockList->GetCount();r++)
    {
        CBlockIndex *pindex=(CBlockIndex *)(m_BlockList->GetRow(r)+sizeof(uint256));
        m_Pos[r]=r;
        m_Height[r]=pindex->nHeight;
        pindex->hashBlock=*(uint256*)m_BlockList->GetRow(r);
        if(pindex->nHeight > max_height)
        {
            max_height=pindex->nHeight;
        }
    }
    
    Sort(0,max_height+1,0,m_BlockList->GetCount());
    
    m_First=new int[max_height];
    
    for(int r=m_BlockList->GetCount()-1;r>=0;r--)
    {
        m_First[m_Height[r]]=r;
    }
    
    return MC_ERR_NOERROR;
}

CBlockIndex* CBlockList::GetBlockIndex(int height,uint256 hash)
{
    if(height<0)
    {
        return NULL;
    }
    
    int r=m_First[height];
    while(m_Height[r] == height)
    {
        if(*(uint256*)m_BlockList->GetRow(m_Pos[r]) == hash)
        {
            return (CBlockIndex *)(m_BlockList->GetRow(m_Pos[r])+sizeof(uint256));
        }
        r++;
    }
    
    return NULL;
}

int CBlockList::GetSize()
{
    return m_BlockList->GetCount();
}

CBlockIndex *CBlockList::GetBlockIndex(int r)
{
    return (CBlockIndex *)(m_BlockList->GetRow(m_Pos[r])+sizeof(uint256));
}


bool CBlockTreeDB::UpdateBlockCacheValues()
{
    CBlockList *block_list=new CBlockList();
    block_list->Init();
    
    boost::scoped_ptr<leveldb::Iterator> pcursor(NewIterator());

    CDataStream ssKeySet(SER_DISK, CLIENT_VERSION);
    ssKeySet << make_pair('b', uint256(0));
    pcursor->Seek(ssKeySet.str());

    // Load mapBlockIndex
    while (pcursor->Valid()) {
        boost::this_thread::interruption_point();
        try {
            leveldb::Slice slKey = pcursor->key();
            CDataStream ssKey(slKey.data(), slKey.data()+slKey.size(), SER_DISK, CLIENT_VERSION);
            char chType;
            ssKey >> chType;
            if (chType == 'b') {
                leveldb::Slice slValue = pcursor->value();
                CDataStream ssValue(slValue.data(), slValue.data()+slValue.size(), SER_DISK, CLIENT_VERSION);
                CDiskBlockIndex diskindex;
                CBlockIndex blockindex;
                
                ssValue >> diskindex;

                blockindex.SetNull();
                DiskBlockIndexToCBlockIndex(&blockindex,diskindex);                
                blockindex.hashPrev=diskindex.hashPrev;
                
                block_list->Add(diskindex.GetBlockHash(), &blockindex);
                        
                pcursor->Next();
            } else {
                break; // if shutdown requested or finished loading block index
            }
        } catch (std::exception &e) {
            return error("%s : Deserialize or I/O error - %s", __func__, e.what());
        }
    }
    
    block_list->Sort();
    
    if(!mc_UpdateBlockCacheValues(block_list))
    {
        return false;
    }
    
    uint256 tipHash=pcoinsTip->GetBestBlock();
    CBlockIndex *pChainTip=NULL;
    
    for(int r=0;r<block_list->GetSize();r++)
    {
        CBlockIndex *pindex=block_list->GetBlockIndex(r);
        pindex->nStatus |= BLOCK_HAVE_CHAIN_CACHE;
        
        if(pindex->GetBlockHash() == tipHash)
        {
            pChainTip=pindex;
        }
        
        CDiskBlockIndex diskindex=CDiskBlockIndex(pindex,pindex->hashPrev);        

        if (!WriteBlockIndex(diskindex)) 
        {
            return error("Failed to write to block index");
        }
    }
    
    CBlockIndex *pwalk=pChainTip;
    
    while(pwalk)
    {
        if(!WriteChainActiveHash(pwalk->nHeight,pwalk->GetBlockHash()))
        {
            return error("Failed to write to active chain");            
        }
        pwalk=block_list->GetBlockIndex(pwalk->nHeight-1,pwalk->hashPrev);
    }
    
    delete block_list;

    pblocktree->Sync();
    return true;
}

/*
bool CBlockTreeDB::UpdateBlockCacheValues()
{
    std::map <uint256,CBlockIndex> mapTempBlockIndex;

    boost::scoped_ptr<leveldb::Iterator> pcursor(NewIterator());

    CDataStream ssKeySet(SER_DISK, CLIENT_VERSION);
    ssKeySet << make_pair('b', uint256(0));
    pcursor->Seek(ssKeySet.str());

    // Load mapBlockIndex
    while (pcursor->Valid()) {
        boost::this_thread::interruption_point();
        try {
            leveldb::Slice slKey = pcursor->key();
            CDataStream ssKey(slKey.data(), slKey.data()+slKey.size(), SER_DISK, CLIENT_VERSION);
            char chType;
            ssKey >> chType;
            if (chType == 'b') {
                leveldb::Slice slValue = pcursor->value();
                CDataStream ssValue(slValue.data(), slValue.data()+slValue.size(), SER_DISK, CLIENT_VERSION);
                CDiskBlockIndex diskindex;
                CBlockIndex blockindex;
                
                ssValue >> diskindex;

                blockindex.SetNull();
                DiskBlockIndexToCBlockIndex(&blockindex,diskindex);                
                blockindex.hashPrev=diskindex.hashPrev;
                mapTempBlockIndex.insert(make_pair(diskindex.GetBlockHash(), blockindex));
                        
                pcursor->Next();
            } else {
                break; // if shutdown requested or finished loading block index
            }
        } catch (std::exception &e) {
            return error("%s : Deserialize or I/O error - %s", __func__, e.what());
        }
    }
    
    if(!mc_UpdateBlockCacheValues(mapTempBlockIndex))
    {
        return false;
    }
    
    BOOST_FOREACH(const PAIRTYPE(uint256, CBlockIndex)& item, mapTempBlockIndex)
    {
        mapTempBlockIndex[item.first].nStatus |= BLOCK_HAVE_CHAIN_CACHE;
        mapTempBlockIndex[item.first].phashBlock = &(item.first);
//        CDiskBlockIndex diskindex=CDiskBlockIndex(&mapTempBlockIndex[item.first]);
        CDiskBlockIndex diskindex=CDiskBlockIndex(&mapTempBlockIndex[item.first],mapTempBlockIndex[item.first].hashPrev);        
//        diskindex.hashPrev=mapTempBlockIndex[item.first].hashPrev;

        if (!WriteBlockIndex(diskindex)) 
        {
            return error("Failed to write to block index");
        }
    }

    pblocktree->Sync();
    return true;
}
*/

CBlockIndex *CBlockTreeDB::ReadBlockIndex(uint256 hash,CBlockIndex *pindexIn) {
    
    CDiskBlockIndex diskindex;
    if(!Read(make_pair('b', hash), diskindex))
    {
        return NULL;
    }
    
    CBlockIndex* pindexNew=pindexIn;
    
    if(pindexNew == NULL)
    {
        pindexNew = new CBlockIndex();
    }
    
    DiskBlockIndexToCBlockIndex(pindexNew,diskindex);
    pindexNew->hashPrev=diskindex.hashPrev;
    
    return pindexNew;
}

bool CBlockTreeDB::LoadBlockIndexGuts()
{
    bool fBlockCacheValues;

    if(!ReadFlag("blockcachedvalues",fBlockCacheValues))
    {
        if(!UpdateBlockCacheValues())
        {
            return error("LoadBlockIndex() : Couldn't update block cached values");            
        }
        if(!WriteBlockCachedStatus(false))
        {
            return error("LoadBlockIndex() : Couldn't update block cached status");            
        }
        if(!WriteFlag("blockcachedvalues",true))
        {
            return error("LoadBlockIndex() : Couldn't set blockcachedvalues flag");            
        }
    }

    mapBlockCachedStatus.clear();
    if(Exists('I'))
    {
        Read('I', mapBlockCachedStatus);        
    }
    
    if(fInMemoryBlockIndex)
    {
        boost::scoped_ptr<leveldb::Iterator> pcursor(NewIterator());

        CDataStream ssKeySet(SER_DISK, CLIENT_VERSION);
        ssKeySet << make_pair('b', uint256(0));
        pcursor->Seek(ssKeySet.str());

        // Load mapBlockIndex
        while (pcursor->Valid()) {
            boost::this_thread::interruption_point();
            try {
                leveldb::Slice slKey = pcursor->key();
                CDataStream ssKey(slKey.data(), slKey.data()+slKey.size(), SER_DISK, CLIENT_VERSION);
                char chType;
                ssKey >> chType;
                if (chType == 'b') {
                    leveldb::Slice slValue = pcursor->value();
                    CDataStream ssValue(slValue.data(), slValue.data()+slValue.size(), SER_DISK, CLIENT_VERSION);
                    CDiskBlockIndex diskindex;
                    ssValue >> diskindex;

                    // Construct block index object
                    CBlockIndex* pindexNew = InsertBlockIndex(diskindex.GetBlockHash());
                    pindexNew->setpprev(InsertBlockIndex(diskindex.hashPrev));
                    DiskBlockIndexToCBlockIndex(pindexNew,diskindex);
                    if (!CheckProofOfWork(pindexNew->GetBlockHash(), pindexNew->nBits))
                        return error("LoadBlockIndex() : CheckProofOfWork failed: %s", pindexNew->ToString());

                    pcursor->Next();
                } else {
                    break; // if shutdown requested or finished loading block index
                }
            } catch (std::exception &e) {
                return error("%s : Deserialize or I/O error - %s", __func__, e.what());
            }
        }
    }
    
    mc_InitCachedBlockIndex();
    
    return true;
}

bool CBlockTreeDB::WriteBlockCachedStatus(bool erase)
{
    if (!erase)
        return Write('I', mapBlockCachedStatus);
    else
        return Erase('I');    
}