// Copyright (c) 2014-2017 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#ifndef MULTICHAIN_CHUNKCOLLECTOR_H
#define MULTICHAIN_CHUNKCOLLECTOR_H

#include "utils/declare.h"
#include "wallet/chunkdb.h"
#include "wallet/wallettxdb.h"

#define MC_CCF_NONE                       0x00000000 
#define MC_CCF_NEW                        0x00000000 
#define MC_CCF_INSERTED                   0x00000001 
#define MC_CCF_DELETED                    0x00000002 
#define MC_CCF_ALL                        0xFFFFFFFF


typedef struct mc_ChunkCollectorRow
{
    unsigned char m_Hash[MC_CDB_CHUNK_HASH_SIZE];                               // Chunk hash
    mc_TxEntity m_Entity;
    unsigned char m_TxID[MC_TDB_TXID_SIZE];                               
    int m_Vout;
    uint32_t m_Size;
    uint32_t m_Flags;
    
    void Zero();
} mc_ChunkCollectorRow;


typedef struct mc_ChunkCollector
{    
    mc_Database *m_DB;                                                          // Database object
    mc_ChunkDB *m_ChunkDB;
    uint32_t m_KeyOffset;                                                       
    uint32_t m_KeySize;                                                         
    uint32_t m_ValueOffset;                                                     
    uint32_t m_ValueSize;                                                       
    uint32_t m_TotalSize;                                                       
    uint32_t m_ValueDBSize;                                                       
    uint32_t m_TotalDBSize;                                                       
    
    
    char m_Name[MC_PRM_NETWORK_NAME_MAX_SIZE+1];                                // Chain name
    char m_DBName[MC_DCT_DB_MAX_PATH];                                          // Full database name
    
    mc_Buffer *m_MarkPool;
    mc_Buffer *m_MemPool;
    mc_Buffer *m_MemPoolNext;
    mc_Buffer *m_MemPool1;
    mc_Buffer *m_MemPool2;
    
    void *m_Semaphore;                                                          // mc_TxDB object semaphore
    uint64_t m_LockedBy;                                                        // ID of the thread locking it
    
    uint32_t m_InitMode;    
    
    mc_ChunkCollector()
    {
        Zero();
    }
    
    ~mc_ChunkCollector()
    {
        Destroy();
    }

    int Initialize(                                                             // Initialization
              mc_ChunkDB *chunk_db,
              const char *name,                                                 // Chain name
              uint32_t mode);                                                   // Unused
    
    int InsertChunk(                                                            // Adds chunk to mempool
                 const unsigned char *hash,                                     // Chunk hash (before chopping)    
                 const mc_TxEntity *entity,                                     // Parent entity
                 const unsigned char *txid,
                 const int vout,
                 const uint32_t chunk_size);  
    
    int InsertChunkInternal(                  
                 const unsigned char *hash,   
                 const mc_TxEntity *entity,   
                 const unsigned char *txid,
                 const int vout,
                 const uint32_t chunk_size);  

    int MarkAndClear(uint32_t flag, int unmark);    
    int CopyFlags();    
    int FillMarkPoolByHash(const unsigned char *hash);    
    int FillMarkPoolByFlag(uint32_t flag, uint32_t not_flag);    
    
    int Commit();                                                      
    int CommitInternal(); 
    
    void Zero();    
    int Destroy();
    void Dump(const char *message);
    
    void LogString(const char *message);
    
    
    int Lock();
    int Lock(int write_mode, int allow_secondary);
    void UnLock();    
} mc_ChunkCollector;

#endif /* MULTICHAIN_CHUNKCOLLECTOR_H */

