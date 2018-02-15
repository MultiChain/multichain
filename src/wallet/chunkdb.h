// Copyright (c) 2014-2017 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#ifndef MULTICHAIN_CHUNKDB_H
#define MULTICHAIN_CHUNKDB_H

#include "utils/declare.h"
#include "wallet/wallettxdb.h"

#define MC_CDB_CHUNK_HASH_SIZE       32
#define MC_CDB_ROW_SIZE              80
#define MC_CDB_HEADER_SIZE           40

#define MC_TDB_TXID_SIZE             32
#define MC_TDB_ENTITY_KEY_SIZE       32
#define MC_TDB_ENTITY_ID_SIZE        20
#define MC_TDB_ENTITY_TYPE_SIZE       4
#define MC_TDB_GENERATION_SIZE        4
#define MC_TDB_POS_SIZE               4
#define MC_TDB_ROW_SIZE              80

#define MC_CDB_TYPE_DB_STAT           0 
#define MC_CDB_TYPE_SUBSCRIPTION      1 
#define MC_CDB_TYPE_FILE              2 


/** File DB Row*/

typedef struct mc_SubscriptionFileDBRow
{
    uint32_t m_SubscriptionID;                                                  // Should be Zero
    uint32_t m_RecordType;                                                      // Should be MC_CDB_TYPE_FILE    
    uint32_t m_FileID;                                                          // File ID
    mc_TxEntity m_Entity;                                                       // Parent Entity
    uint32_t m_Size;                                                            // File size
    uint32_t m_StorageFlags;                                                    // Internal flags
    uint32_t m_Count;                                                           // Total count
    uint32_t m_FirstTimestamp;                                                  // Timestamp of the first record
    uint32_t m_FirstOffset;                                                     // First data offset
    uint32_t m_LastTimeStamp;                                                   // Timestamp of the last record
    uint32_t m_LastOffset;                                                      // Last data file size
    void Zero();
} mc_SubscriptionFileDBRow;


/** Entity DB Row*/

typedef struct mc_SubscriptionDBRow
{
    uint32_t m_Zero;                                                            // Should be Zero
    uint32_t m_RecordType;                                                      // Should be MC_CDB_TYPE_SUBSCRIPTION    
    uint32_t m_Zero1;                                                           // Should be Zero
    mc_TxEntity m_Entity;                                                       // Parent Entity
    uint32_t m_Flags;                                                           // Flags passed from higher level
    uint32_t m_SubscriptionID;                                                  // Subscription ID
    uint32_t m_Count;                                                           // Total chunk count
    uint32_t m_FirstFileID;                                                     // First data file ID/Reserved
    uint32_t m_FirstFileOffset;                                                 // First data file offset/Reserved
    uint32_t m_LastFileID;                                                      // Last data file ID
    uint32_t m_LastFileSize;                                                    // Last data file size

    char m_DirName[MC_DCT_DB_MAX_PATH];                                         // Full file name
    
    void Zero();
} mc_SubscriptionDBRow;

typedef struct mc_ChunkDBStat
{
    uint32_t m_Zero;                                                            // Should be Zero
    uint32_t m_RecordType;                                                      // Should be MC_CDB_TYPE_DB_STAT    
    uint32_t m_Zero1;                                                           // Should be Zero
    mc_TxEntity m_ZeroEntity;                                                   // Zero Entity
    uint32_t m_InitMode;
    uint32_t m_ChunkDBVersion;
    uint32_t m_Count;                                                           // Total tx count
    uint32_t m_FullSize;                                                        // Total tx size
    uint32_t m_LastSubscription; 
    uint32_t m_Reserved1; 
    uint32_t m_Reserved2; 
    void Zero();
} mc_ChunkDBStat;


/** Chunk DB Row **/

typedef struct mc_ChunkDBRow
{
    uint32_t m_SubscriptionID;                                                  // Subscription ID
    unsigned char m_Hash[MC_CDB_CHUNK_HASH_SIZE];                               // Chunk hash
    uint32_t m_Size;                                                            // Chunk Size
    uint32_t m_Flags;                                                           // Flags passed from higher level
    uint32_t m_StorageFlags;                                                    // Internal flags
    uint32_t m_InternalFileID;                                                  // Data file ID
    uint32_t m_InternalFileOffset;                                              // Offset in the data file
    uint32_t m_PrevSubscriptionID;                                              // Prev Subscription ID for this hash
    uint32_t m_NextSubscriptionID;                                              // Next Subscription ID for this hash
    
    void Zero();
} mc_ChunkDBRow;


/** Chunk DB **/

typedef struct mc_ChunkDB
{    
    mc_Database *m_DB;                                                          // Database object
    uint32_t m_KeyOffset;                                                       
    uint32_t m_KeySize;                                                         
    uint32_t m_ValueOffset;                                                     
    uint32_t m_ValueSize;                                                       
    uint32_t m_TotalSize;                                                       
    
    mc_ChunkDBStat m_DBStat;                                                 
    
    char m_Name[MC_PRM_NETWORK_NAME_MAX_SIZE+1];                                // Chain name
    char m_DirName[MC_DCT_DB_MAX_PATH];                                         // Chunk directory name
    char m_DBName[MC_DCT_DB_MAX_PATH];                                          // Full database name
    char m_LogFileName[MC_DCT_DB_MAX_PATH];                                     // Full log file name    
    
    mc_Buffer *m_Subscriptions;                                                 // List of import entities (mc_TxEntityStat)
    
    mc_ChunkDB()
    {
        Zero();
    }
    
    ~mc_ChunkDB()
    {
        Destroy();
    }
    
    int Initialize(                                                             // Initialization
              const char *name,                                                 // Chain name
              uint32_t mode);                                                   // Unused
    
    int AddSubscription(mc_SubscriptionDBRow *subscription);                          
    int AddEntity(mc_TxEntity *entity,uint32_t flags);                          // Adds entity
    
    mc_SubscriptionDBRow *FindSubscription(mc_TxEntity *entity);                // Finds subscription
    
    int AddChunk(                                                               // Adds chunk to mempool
                 const unsigned char *hash,                                     // Chunk hash (before chopping)    
                 const mc_TxEntity *entity,                                     // Parent entity
                 const unsigned char *chunk,                                    // Chunk data
                 const unsigned char *metadata,                                 // Chunk metadata
                 const uint32_t chunk_size,                                     // Chunk size
                 const uint32_t metadata_size,                                  // Chunk metadata size
                 const uint32_t flags);                                         // Flags
    
    int GetChunk(                                                               
                 const unsigned char *hash,                                     // Chunk hash (before chopping)    
                 const unsigned char *entity,                                   // Parent entity
                 unsigned char **chunk,                                         // Chunk data
                 unsigned char **metadata,                                      // Metadata
                 uint32_t *size,                                                // Chunk Size
                 uint32_t *metadata_size,                                       // Chunk Size
                 const uint32_t flags);                                         // Flags
    
    int Commit();                                                               // Commit mempool to disk
    
    void Zero();    
    int Destroy();
    void Dump(const char *message);
    
    void LogString(const char *message);
    
} mc_ChunkDB;

#endif /* MULTICHAIN_CHUNKDB_H */

