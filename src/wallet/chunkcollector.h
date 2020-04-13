// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#ifndef MULTICHAIN_CHUNKCOLLECTOR_H
#define MULTICHAIN_CHUNKCOLLECTOR_H

#include "utils/declare.h"
#include "protocol/relay.h"
#include "wallet/chunkdb.h"
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

#define MC_CCW_TIMEOUT_QUERY                      25
#define MC_CCW_TIMEOUT_REQUEST                    10
#define MC_CCW_TIMEOUT_REQUEST_SHIFT               2
#define MC_CCW_MAX_CHUNKS_PER_QUERY             1000
#define MC_CCW_MAX_SIGNATURES_PER_REQEST           4
#define MC_CCW_DEFAULT_AUTOCOMMIT_DELAY          200
#define MC_CCW_WORST_RESPONSE_SCORE       1048576000
#define MC_CCW_DEFAULT_MEMPOOL_SIZE            60000
#define MC_CCW_MAX_KBS_PER_SECOND               8196
#define MC_CCW_MIN_KBS_PER_SECOND                128
#define MC_CCW_MAX_DELAY_BETWEEN_COLLECTS       1000
#define MC_CCW_QUERY_SPLIT                         4
#define MC_CCW_MAX_ITEMS_PER_CHUNKFOR_CHECK       16
#define MC_CCW_MAX_EF_SIZE                     65536


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
    uint32_t m_Reserved1;
    mc_OffchainMessageID m_Query;
    mc_OffchainMessageID m_Request;
    uint32_t m_QueryTimeStamp;
    uint32_t m_QuerySilenceTimestamp;
    uint32_t m_RequestTimeStamp;
    uint32_t m_RequestPos;
    
    void Zero();
} mc_ChunkEntityValue;

typedef struct mc_ChunkCollectorDBRow
{
    uint32_t m_QueryNextAttempt;
    int m_Vout;
    unsigned char m_TxID[MC_TDB_TXID_SIZE];                               
    mc_TxEntity m_Entity;
    unsigned char m_Hash[MC_CDB_CHUNK_HASH_SIZE];                               // Chunk hash
    
    uint32_t m_Size;
    uint32_t m_Flags;
    uint32_t m_QueryAttempts;
    uint32_t m_Status;
    int64_t m_TotalChunkSize;
    int64_t m_TotalChunkCount;      
    
    unsigned char m_Salt[MC_CDB_CHUNK_HASH_SIZE];                               // Salt size should not be large than hash size
    uint32_t m_SaltSize;
    uint32_t m_CollectorFlags;
    int64_t m_Reserved2;
    
    void Zero();
} mc_ChunkCollectorDBRow;

typedef struct mc_ChunkCollectorRow
{
    mc_ChunkEntityKey m_ChunkDef;
    uint32_t m_DBNextAttempt;
    int m_Vout;
    unsigned char m_TxID[MC_TDB_TXID_SIZE];                               
    unsigned char m_Salt[MC_CDB_CHUNK_HASH_SIZE];                               
    uint32_t m_SaltSize;
    uint32_t m_Flags;
    
    mc_ChunkEntityValue m_State;
    
    void Zero();
} mc_ChunkCollectorRow;

typedef struct mc_ChunkCollectorStat
{
    int64_t m_Pending;
    int64_t m_Delivered;
    int64_t m_Sleeping;
    int64_t m_Queried;
    int64_t m_Requested;    
    int64_t m_Unresponded;
    int64_t m_Undelivered;
    int64_t m_Baddelivered;
    
    void Zero();
} mc_ChunkCollectorStat;

typedef struct mc_ChunkCollector
{    
    mc_Database *m_DB;                                                          // Database object
    mc_ChunkDB *m_ChunkDB;
    uint32_t m_KeyOffset;                                                       
    uint32_t m_KeyDBOffset;                                                       
    uint32_t m_KeySize;                                                         
    uint32_t m_KeyDBSize;                                                         
    uint32_t m_ValueOffset;                                                     
    uint32_t m_ValueDBOffset;                                                     
    uint32_t m_ValueSize;                                                       
    uint32_t m_ValueDBSize;                                                       
    uint32_t m_TotalSize;                                                       
    uint32_t m_TotalDBSize;                                                       
    int64_t m_AutoCommitDelay;
    int64_t m_NextAutoCommitTimestamp;
    int64_t m_NextTryTimestamp;
    int m_MaxMemPoolSize;
    int m_TimeoutRequest;
    int m_TimeoutQuery;
    int m_MaxKBPerDestination;
    int m_MaxMaxKBPerDestination;
    int m_MinMaxKBPerDestination;
    int64_t m_LastKBPerDestinationChangeTimestamp;
    int m_MaxMBPerSecond;
    int64_t m_TotalChunkSize;
    int64_t m_TotalChunkCount;                                                       
    
    mc_ChunkCollectorStat m_StatLast[2];
    mc_ChunkCollectorStat m_StatTotal[2];
    
    char m_Name[MC_PRM_NETWORK_NAME_MAX_SIZE+1];                                // Chain name
    char m_DBName[MC_DCT_DB_MAX_PATH];                                          // Full database name
    
    mc_Buffer *m_MarkPool;
    mc_Buffer *m_MemPool;
    mc_Buffer *m_MemPoolNext;
    mc_Buffer *m_MemPool1;
    mc_Buffer *m_MemPool2;
    
    mc_ChunkCollectorDBRow m_DBRow;
    mc_ChunkCollectorDBRow m_LastDBRow;
    
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

    void SetDBRow(mc_ChunkCollectorRow *collect_row);
    void GetDBRow(mc_ChunkCollectorRow *collect_row);
    int DeleteDBRow(mc_ChunkCollectorRow *collect_row);
    int UpdateDBRow(mc_ChunkCollectorRow *collect_row);
    int InsertDBRow(mc_ChunkCollectorRow *collect_row);
    int SeekDB(void *dbrow);
    int ReadFromDB(mc_Buffer *mempool,int rows);
    int UpgradeDB();
    
    int Initialize(                                                             // Initialization
              mc_ChunkDB *chunk_db,
              const char *name,                                                 // Chain name
              uint32_t mode);                                                   // Unused
    
    int InsertChunk(                                                            // Adds chunk to mempool
                 const unsigned char *hash,                                     // Chunk hash (before chopping)    
                 const mc_TxEntity *entity,                                     // Parent entity
                 const unsigned char *txid,
                 const int vout,
                 const uint32_t chunk_size,
                 const uint32_t salt_size,
                 const uint32_t flags);  
    
    int InsertChunkInternal(                  
                 const unsigned char *hash,   
                 const mc_TxEntity *entity,   
                 const unsigned char *txid,
                 const int vout,
                 const uint32_t chunk_size,
                 const uint32_t salt_size,
                 const uint32_t flags);  

    int MarkAndClear(uint32_t flag, int unmark);    
    int CopyFlags();    
    int FillMarkPoolByHash(const unsigned char *hash);    
    int FillMarkPoolByFlag(uint32_t flag, uint32_t not_flag);    
    void AdjustKBPerDestination(CNode* pfrom,bool success);
        
    int Commit();                                                      
    int CommitInternal(int fill_mempool); 
    
    int Unsubscribe(mc_Buffer* lpEntities);
    
    void Zero();    
    int Destroy();
    void Dump(const char *message);
    
    void LogString(const char *message);
    
    
    int Lock();
    int Lock(int write_mode, int allow_secondary);
    void UnLock();    
} mc_ChunkCollector;

#endif /* MULTICHAIN_CHUNKCOLLECTOR_H */

