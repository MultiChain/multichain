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
#define MC_CCF_SELECTED                   0x00000004 
#define MC_CCF_UPDATED                    0x00000008 
#define MC_CCF_WRONG_SIZE                 0x00010000 
#define MC_CCF_ERROR_MASK                 0x00FF0000 
#define MC_CCF_ALL                        0xFFFFFFFF

#define MC_CCW_TIMEOUT_QUERY                      60
#define MC_CCW_TIMEOUT_REQUEST                     5
#define MC_CCW_MAX_CHUNKS_PER_QUERY               64
#define MC_CCW_WORST_RESPONSE_SCORE             1000


typedef struct mc_ChunkEntityKey
{
    unsigned char m_Hash[MC_CDB_CHUNK_HASH_SIZE];                               // Chunk hash
    mc_TxEntity m_Entity;
    uint32_t m_Size;
    uint32_t m_Flags;
    
    void Zero();
} mc_ChunkEntityKey;

typedef struct mc_ChunkEntityValue
{
    uint32_t m_QueryAttempts;
    uint32_t m_QueryNextAttempt;
    uint32_t m_Status;
    int64_t m_Reserved1;
    int64_t m_Reserved2;    
    int64_t m_Query;
    uint32_t m_QueryTimeStamp;
    uint32_t m_Reserved3;
    int64_t m_Request;
    uint32_t m_RequestTimeStamp;
    uint32_t m_RequestPos;
    
    void Zero();
} mc_ChunkEntityValue;

typedef struct mc_ChunkCollectorRow
{
    mc_ChunkEntityKey m_ChunkDef;
    unsigned char m_TxID[MC_TDB_TXID_SIZE];                               
    int m_Vout;
    mc_ChunkEntityValue m_State;
    
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
    int64_t m_NextTryTimestamp;
    
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

