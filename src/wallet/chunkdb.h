// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#ifndef MULTICHAIN_CHUNKDB_H
#define MULTICHAIN_CHUNKDB_H

#include "utils/declare.h"
#include "wallet/wallettxdb.h"

#define MC_CDB_CHUNK_HASH_SIZE       32
#define MC_CDB_CHUNK_SALT_SIZE       32
#define MC_CDB_ROW_SIZE              80
#define MC_CDB_HEADER_SIZE           40


#define MC_CDB_TYPE_DB_STAT           0x00000000 
#define MC_CDB_TYPE_SUBSCRIPTION      0x01000000 
#define MC_CDB_TYPE_FILE              0x02000000 

#define MC_CDB_MAX_FILE_SIZE             0x08000000                             // Maximal data file size, 1GB
#define MC_CDB_MAX_CHUNK_DATA_POOL_SIZE  0x8000000                              // Maximal size of chunk pool before commit, 128MB
#define MC_CDB_MAX_FILE_READ_BUFFER_SIZE 0x0100000                              // Maximal size of chunk pool before commit, 1MB
#define MC_CDB_MAX_CHUNK_EXTRA_SIZE      1024 
#define MC_CDB_MAX_MEMPOOL_SIZE          1024 

#define MC_CDB_FLUSH_MODE_NONE        0x00000000
#define MC_CDB_FLUSH_MODE_FILE        0x00000001
#define MC_CDB_FLUSH_MODE_COMMIT      0x00000002
#define MC_CDB_FLUSH_MODE_DATASYNC    0x00000100

#define MC_CFL_STORAGE_FLUSHED        0x01000000 
#define MC_CFL_STORAGE_PURGED         0x02000000 

#define MC_CFL_FORMAT_MASK            0x00000007 
#define MC_CFL_SINGLE_CHUNK           0x00010000 

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
    uint32_t m_Zero2;                                                           // Should be Zero
    mc_TxEntity m_Entity;                                                       // Parent Entity
    uint32_t m_Flags;                                                           // Flags passed from higher level
    int32_t  m_SubscriptionID;                                                  // Subscription ID
    uint32_t m_Count;                                                           // Total chunk count
    uint32_t m_TmpFlags;           
    int32_t  m_FirstFileID;                                                     // First data file ID/Reserved
    uint32_t m_FirstFileOffset;                                                 // First data file offset/Reserved
    int32_t  m_LastFileID;                                                      // Last data file ID
    uint32_t m_LastFileSize;                                                    // Last data file size
    uint64_t m_FullSize;                                                        // Total tx size

    char m_DirName[MC_DCT_DB_MAX_PATH];                                         // Full file name
    
    void Zero();
} mc_SubscriptionDBRow;

typedef struct mc_ChunkDBStat
{
    uint32_t m_Zero;                                                            // Should be Zero
    uint32_t m_RecordType;                                                      // Should be MC_CDB_TYPE_DB_STAT    
    uint32_t m_Zero1;                                                           // Should be Zero
    uint32_t m_Zero2;                                                           // Should be Zero
    mc_TxEntity m_ZeroEntity;                                                   // Zero Entity
    uint32_t m_InitMode;
    int32_t  m_LastSubscription; 
    uint32_t m_Count;                                                           // Total tx count
    uint32_t m_ChunkDBVersion;
    uint32_t m_Reserved1; 
    uint32_t m_Reserved2; 
    uint32_t m_Reserved3; 
    uint32_t m_Reserved4; 
    uint64_t m_FullSize;                                                        // Total tx size
    void Zero();
} mc_ChunkDBStat;


/** Chunk DB Row **/

typedef struct mc_ChunkDBRow
{
    unsigned char m_Hash[MC_CDB_CHUNK_HASH_SIZE];                               // Chunk hash
    int32_t  m_SubscriptionID;                                                  // Subscription ID
    uint32_t m_Pos;                                                             // Position of this record for subscription/hash
    uint32_t m_Size;                                                            // Chunk Size
    uint32_t m_Flags;                                                           // Flags passed from higher level
    uint32_t m_HeaderSize;                                                      // Header size
    uint32_t m_StorageFlags;                                                    // Internal flags
    int32_t  m_ItemCount;                                                       // Number of times this chunk appears in subscription (if m_Pos=0)
    int32_t  m_TmpOnDiskItems;
    int32_t  m_InternalFileID;                                                  // Data file ID
    uint32_t m_InternalFileOffset;                                              // Offset in the data file
    uint32_t  m_TxIDStart;                                                       // First bytes of TxID
    int32_t  m_NextSubscriptionID;                                              // Next Subscription ID for this hash
    
    void Zero();
    void SwapPosBytes();
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
    mc_Buffer *m_MemPool;
    mc_Script *m_ChunkData;
    mc_Script *m_TmpScript;
    
    int m_FeedPos;

    void *m_Semaphore;                                                          // mc_TxDB object semaphore
    uint64_t m_LockedBy;                                                        // ID of the thread locking it
    
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
    int AddEntityInternal(mc_TxEntity *entity,uint32_t flags);                  
    int RemoveEntity(mc_TxEntity *entity,uint32_t *removed_chunks,uint64_t *removed_size);                          
    int RemoveEntityInternal(mc_TxEntity *entity,uint32_t *removed_chunks,uint64_t *removed_size);                          
    int SourceChunksRecovery();                          
    
    mc_SubscriptionDBRow *FindSubscription(const mc_TxEntity *entity);                // Finds subscription
    int FindSubscription(const mc_TxEntity *entity,mc_SubscriptionDBRow *subscription);   // Finds subscription
    
    int AddChunk(                                                               // Adds chunk to mempool
                 const unsigned char *hash,                                     // Chunk hash (before chopping)    
                 const mc_TxEntity *entity,                                     // Parent entity
                 const unsigned char *txid,
                 const int vout,
                 const unsigned char *chunk,                                    // Chunk data
                 const unsigned char *details,                                  // Chunk metadata
                 const unsigned char *salt,                                     // Chunk salt
                 const uint32_t chunk_size,                                     // Chunk size
                 const uint32_t details_size,                                   // Chunk metadata size
                 const uint32_t salt_size,                                      // Chunk salt size
                 const uint32_t flags);                                         // Flags

    int AddChunkInternal(                                                               // Adds chunk to mempool
                 const unsigned char *hash,                                     // Chunk hash (before chopping)    
                 const mc_TxEntity *entity,                                     // Parent entity
                 const unsigned char *txid,
                 const int vout,
                 const unsigned char *chunk,                                    // Chunk data
                 const unsigned char *details,                                  // Chunk metadata
                 const unsigned char *salt,                                     // Chunk salt
                 const uint32_t chunk_size,                                     // Chunk size
                 const uint32_t details_size,                                   // Chunk metadata size
                 const uint32_t salt_size,                                      // Chunk salt size
                 const uint32_t flags);                                         // Flags

    int GetChunkDefInternal(
                    mc_ChunkDBRow *chunk_def,
                    const unsigned char *hash,                                  // Chunk hash (before chopping)    
                    const void *entity,
                    const unsigned char *txid,
                    const int vout,
                    int *mempool_entity_row,
                    int check_limit);
            
    
    int GetChunkDef(
                    mc_ChunkDBRow *chunk_def,
                    const unsigned char *hash,                                  // Chunk hash (before chopping)    
                    const void *entity,
                    const unsigned char *txid,
                    const int vout);
    
    int GetChunkDefWithLimit(
                    mc_ChunkDBRow *chunk_def,
                    const unsigned char *hash,                                  // Chunk hash (before chopping)    
                    const void *entity,
                    const unsigned char *txid,
                    const int vout,
                    int check_limit);
    
    unsigned char *GetChunkInternal(mc_ChunkDBRow *chunk_def,
                                    int32_t offset,
                                    int32_t len,
                                    size_t *bytes,
                                    unsigned char *salt,
                                    uint32_t *salt_size);

    unsigned char *GetChunk(mc_ChunkDBRow *chunk_def,
                                    int32_t offset,
                                    int32_t len,
                                    size_t *bytes,
                                    unsigned char *salt,
                                    uint32_t *salt_size);
    
    void SetFileName(char *FileName,
                     mc_SubscriptionDBRow *subscription,
                     uint32_t fileid);
    
    int FlushDataFile(mc_SubscriptionDBRow *subscription,
                                  uint32_t fileid,
                                  uint32_t flush_mode);
    
    int RestoreChunkIfNeeded(mc_ChunkDBRow *chunk_def);
    
    int AddToFile(const void *chunk,                  
                          uint32_t size,
                          mc_SubscriptionDBRow *subscription,
                          uint32_t fileid,
                          uint32_t offset,
                          uint32_t flush_mode);
    
    int Commit(int block);                                                               // Commit mempool to disk

    int Commit(int block,uint32_t flush_mode);                                                               // Commit mempool to disk
    int CommitInternal(int block,uint32_t flush_mode); 
    
    int FlushSourceChunks(uint32_t flush_mode);
    
    void Zero();    
    int Destroy();
    void Dump(const char *message);
    void Dump(const char *message, int force);
    
    void LogString(const char *message);
    
    int Lock();
    int Lock(int write_mode, int allow_secondary);
    void UnLock();
    
} mc_ChunkDB;

#endif /* MULTICHAIN_CHUNKDB_H */

