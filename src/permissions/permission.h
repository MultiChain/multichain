// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#ifndef MULTICHAIN_PERMISSION_H
#define	MULTICHAIN_PERMISSION_H

#include "utils/declare.h"
#include "utils/dbwrapper.h"

#define MC_PTP_NONE             0x00000000
#define MC_PTP_CONNECT          0x00000001
#define MC_PTP_SEND             0x00000002
#define MC_PTP_RECEIVE          0x00000004
#define MC_PTP_WRITE            0x00000008
#define MC_PTP_ISSUE            0x00000010
#define MC_PTP_CREATE           0x00000020
#define MC_PTP_READ             0x00000080
#define MC_PTP_MINE             0x00000100
#define MC_PTP_CUSTOM1          0x00000200
#define MC_PTP_CUSTOM2          0x00000400
#define MC_PTP_CUSTOM3          0x00000800
#define MC_PTP_ADMIN            0x00001000
#define MC_PTP_ACTIVATE         0x00002000
#define MC_PTP_UPGRADE          0x00010000
#define MC_PTP_CUSTOM4          0x00020000
#define MC_PTP_CUSTOM5          0x00040000
#define MC_PTP_CUSTOM6          0x00080000
#define MC_PTP_BLOCK_MINER      0x01000000
#define MC_PTP_BLOCK_INDEX      0x02000000
#define MC_PTP_FILTER           0x04000000
#define MC_PTP_SPECIFIED        0x80000000
#define MC_PTP_ALL              0x00FFFFFF
#define MC_PTP_GLOBAL_ALL       0x00003137
#define MC_PTP_CUSTOM_ALL       0x000E0E00

#define MC_PTP_CACHED_SCRIPT_REQUIRED      0x02000000                           // Special temporary value for coin selection

#define MC_PTN_CUSTOM1          "low1"
#define MC_PTN_CUSTOM2          "low2"
#define MC_PTN_CUSTOM3          "low3"
#define MC_PTN_CUSTOM4          "high1"
#define MC_PTN_CUSTOM5          "high2"
#define MC_PTN_CUSTOM6          "high3"


#define MC_PFL_NONE             0x00000000
#define MC_PFL_IS_SCRIPTHASH    0x00000001
#define MC_PFL_ENTITY_GENESIS   0x00000002
#define MC_PFL_HAVE_PENDING     0x01000000

#define MC_PFB_NONE               0x00000000
#define MC_PFB_MINED_BEFORE       0x00000001                                    // Idential with FoundInDB value of normal row
#define MC_PFB_GOVERNANCE_CHANGE  0x00000002

#define MC_PLS_SIZE_ENTITY            32
#define MC_PLS_SIZE_ADDRESS           20
#define MC_PLS_SIZE_HASH              32
#define MC_PLS_SIZE_UPGRADE           16
#define MC_PLS_SIZE_OFFSETS_PER_ROW    6

#define MC_PPL_REPLAY             0x00000001    
#define MC_PPL_ADMINMINERGRANT    0x00000002    

#define MC_PSE_UPGRADE                0x01                                      
#define MC_PSE_ADMINMINERLIST         0x02                                      



typedef struct mc_MempoolPermissionRow
{    
    unsigned char m_Entity[MC_PLS_SIZE_ENTITY];                                 // Entity genesis transaction TxID, all 0s for master permissions 
    unsigned char m_Address[MC_PLS_SIZE_ADDRESS];                               // Address 
    uint32_t m_Type;                                                            // Permission type MC_PTP_ constants
} mc_MempoolPermissionRow;

/** Database record structure */

typedef struct mc_PermissionDBRow
{    
    unsigned char m_Entity[MC_PLS_SIZE_ENTITY];                                 // Entity genesis transaction TxID, all 0s for master permissions 
    unsigned char m_Address[MC_PLS_SIZE_ADDRESS];                               // Address 
    uint32_t m_Type;                                                            // Permission type MC_PTP_ constants
    uint32_t m_BlockFrom;                                                       // Permission block-from 
    uint32_t m_BlockTo;                                                         // Permission block-to
    uint64_t m_LedgerRow;                                                       // Row in the ledger corresponding to the last change for this key
    uint32_t m_Flags;                                                           // Flags MC_PFL_ constants
    uint32_t m_Reserved1;                                                       // Reserved to align to 80 bytes
    void Zero();
    int InBlockRange(uint32_t block);
} mc_PermissionDBRow;

/** Database block miner record structure */

typedef struct mc_BlockMinerDBRow
{    
    unsigned char m_BlockHash[MC_PLS_SIZE_ENTITY];                              // BlockHash
    unsigned char m_Null[MC_PLS_SIZE_ADDRESS];                                  // Should be Null
    uint32_t m_Type;                                                            // Permission type MC_PTP_ constants, should be MC_PTP_BLOCK_MINER
    unsigned char m_Address[MC_PLS_SIZE_ADDRESS];                               // Miner Address
    uint32_t m_AdminMinerCount;                                                 // Admin and miner counts, if 0 - should be recalculated
    void Zero();
} mc_BlockMinerDBRow;

/** Database admin/miner grants record structure */

typedef struct mc_AdminMinerGrantDBRow
{    
    unsigned char m_BlockHash[MC_PLS_SIZE_ENTITY];                              // BlockHash
    uint64_t m_Reserved1;                                                       // Reserved, should be 0
    uint64_t m_Reserved2;                                                       // Reserved, should be 0
    uint32_t m_RecordID;                                                        // ID for the record containing up to 6 tx offsets
    uint32_t m_Type;                                                            // Permission type MC_PTP_ constants, should be MC_PTP_BLOCK_MINER
    uint32_t m_Offsets[MC_PLS_SIZE_OFFSETS_PER_ROW];                            // Offsets of transactions with admin/miner grant
    void Zero();
} mc_AdminMinerGrantDBRow;


/** Database */

typedef struct mc_PermissionDB
{   
    char m_FileName[MC_DCT_DB_MAX_PATH];                                        // Full file name
    mc_Database *m_DB;                                                          // Database object
    uint32_t m_KeyOffset;                                                       // Offset of the key in mc_PermissionDBRow structure, 32 for protocol<=10003, 0 otherwise
    uint32_t m_KeySize;                                                         // Size of the database key, 24 for protocol<=10003, 56 otherwise
    uint32_t m_ValueOffset;                                                     // Offset of the value in mc_PermissionDBRow structure, 56 
    uint32_t m_ValueSize;                                                       // Size of the database value, 24
    uint32_t m_TotalSize;                                                       // Totals size of the database row
    mc_PermissionDB()
    {
        Zero();
    }
    
    ~mc_PermissionDB()
    {
        Close();
    }
    void Zero();
    int Open();
    int Close();
    void SetName(const char *name);
} mc_PermissionDB;

/** Ledger and mempool record structure */

typedef struct mc_PermissionLedgerRow
{    
    unsigned char m_Entity[MC_PLS_SIZE_ENTITY];                                 // Entity genesis transaction TxID, all 0s for master permissions 
    unsigned char m_Address[MC_PLS_SIZE_ADDRESS];                               // Address 
    uint32_t m_Type;                                                            // Permission type MC_PTP_ constants
    uint64_t m_PrevRow;                                                         // Previous row in the ledger corresponding to this key
    uint32_t m_BlockFrom;                                                       // Permission block-from after processing this row
    uint32_t m_BlockTo;                                                         // Permission block-to after processing this row
    uint32_t m_Consensus;                                                       // If consensus is reached returns number of admins, 0 otherwise
    uint32_t m_Flags;                                                           // Flags MC_PFL_ constants
    unsigned char m_Admin[MC_PLS_SIZE_ADDRESS];                                 // Admin address 
    uint32_t m_GrantFrom;                                                       // Permission block-from specified in this row
    uint32_t m_GrantTo;                                                         // Permission block-to specified in this row
    uint32_t m_Timestamp;                                                       // Timestamp of this row
//    uint32_t m_FoundInDB;                                                     // Row is found in database
    int32_t m_Offset;                                                           // Tx offset in the block, -1 if in mempool
    uint32_t m_BlockReceived;                                                   // Block this transaction was confirmed
    uint64_t m_ThisRow;                                                         // Row in the ledger    
    
    void Zero();
} mc_PermissionLedgerRow;

/** Block details row stored in permission ledger */

typedef struct mc_BlockLedgerRow
{    
    unsigned char m_Entity[MC_PLS_SIZE_ENTITY];                                 // Placeholder, always zeroes
    unsigned char m_Address[MC_PLS_SIZE_ADDRESS];                               // Miner Address or sprintf(...,"Block %08X row",<block height>)
    uint32_t m_Type;                                                            // MC_PTP_BLOCK_MINER or MC_PTP_BLOCK_INDEX
    uint64_t m_PrevRow;                                                         // Previous row in the ledger corresponding to this key
    uint32_t m_BlockFrom;                                                       // BlockHeight
    uint32_t m_BlockTo;                                                         // BlockHeight
    uint32_t m_AdminCount;                                                      // AdminCount
    uint32_t m_MinerCount;                                                      // MinerCount
    unsigned char m_CommitHash[MC_PLS_SIZE_HASH];                               // BlockHash
    uint32_t m_BlockFlags;                                                      // Flags MC_PBF_ constants
    uint32_t m_BlockReceived;                                                   // BlockHeight
    uint64_t m_ThisRow;                                                         // Row in the ledger    
    
    void Zero();
} mc_BlockLedgerRow;


/** Ledger */

typedef struct mc_PermissionLedger
{   
    char m_FileName[MC_DCT_DB_MAX_PATH];                                        // Full file name
    int m_FileHan;                                                              // File handle
    uint32_t m_KeyOffset;                                                       // Offset of the key in mc_PermissionLedgerRow structure, 32 for protocol<=10003, 0 otherwise
    uint32_t m_KeySize;                                                         // Size of the ledger key, 24 for protocol<=10003, 56 otherwise
    uint32_t m_ValueOffset;                                                     // Offset of the value in mc_PermissionLedgerRow structure, 56 
    uint32_t m_ValueSize;                                                       // Size of the ledger value 72
    uint32_t m_TotalSize;                                                       // Totals size of the ledger row
   
    mc_PermissionLedger()
    {
        Zero();
    }
    
    ~mc_PermissionLedger()
    {
        Close();
    }
    
    void Zero();
    int Open();
    int Close();
    void Flush();
    void SetName(const char *name);
    int GetRow(uint64_t RowID,mc_PermissionLedgerRow *row);
    uint64_t GetSize();
    int WriteRow(mc_PermissionLedgerRow *row);
    int SetRow(uint64_t RowID,mc_PermissionLedgerRow *row);
    
} mc_PermissionLedger;

/** Permission details structure */

typedef struct mc_PermissionDetails
{    
    unsigned char m_Entity[MC_PLS_SIZE_ENTITY];                                 // Entity genesis transaction TxID, all 0s for master permissions 
    unsigned char m_Address[MC_PLS_SIZE_ADDRESS];                               // Address 
    uint32_t m_Type;                                                            // Permission type MC_PTP_ constants
    uint32_t m_BlockFrom;                                                       // Permission block-from after processing this row
    uint32_t m_BlockTo;                                                         // Permission block-to after processing this row
    uint32_t m_Flags;                                                           // Flags MC_PFL_ constants
    int32_t m_RequiredAdmins;                                                   // Number of admins required for consensus                                                  
    unsigned char m_LastAdmin[MC_PLS_SIZE_ADDRESS];                             // Last admin address 
    uint32_t m_BlockReceived;                                                   // BlockHeight
    uint64_t m_LastRow;                                                         // Last row in the ledger        
    void Zero();
} mc_PermissionDetails;


typedef struct mc_RollBackPos
{
    int m_Block;
    int m_Offset;
    int m_InMempool;
    
    void Zero();
    int IsOut(int block,int offset);
    int InBlock();
    int InMempool();
    int NotApplied();
} mc_RollBackPos;


typedef struct mc_Permissions
{    
    mc_PermissionDB *m_Database;
    mc_PermissionLedger *m_Ledger;
    mc_Buffer   *m_MemPool;
    mc_Buffer   *m_TmpPool;
    char m_Name[MC_PRM_NETWORK_NAME_MAX_SIZE+1]; 
    char m_LogFileName[MC_DCT_DB_MAX_PATH+1]; 
    int m_Block;
    uint64_t m_Row;
    int m_AdminCount;
    int m_MinerCount;
//    int m_DBRowCount;

    uint64_t m_CheckPointRow;
    int m_CheckPointAdminCount;
    int m_CheckPointMinerCount;
    uint64_t m_CheckPointMemPoolSize;
    
    int m_CopiedBlock;
    int m_ForkBlock;
    uint64_t m_CopiedRow;
    int m_CopiedAdminCount;
    int m_CopiedMinerCount;
    int m_ClearedAdminCount;
    int m_ClearedMinerCount;
    int m_ClearedMinerCountForMinerVerification;
    int m_TmpSavedAdminCount;
    int m_TmpSavedMinerCount;
    
    mc_Buffer   *m_CopiedMemPool;

    int m_CheckForMempoolFlag;
    mc_Buffer               *m_MempoolPermissions;
    mc_Buffer               *m_MempoolPermissionsToReplay;    
    mc_RollBackPos m_RollBackPos;
    
    void *m_Semaphore;
    uint64_t m_LockedBy;

    mc_Permissions()
    {
        Zero();
    }
    
    ~mc_Permissions()
    {
        Destroy();
    }
    
// External functions    
    int Initialize(const char *name,int mode);

    int SetPermission(const void* lpEntity,const void* lpAddress,uint32_t type,const void* lpAdmin,uint32_t from,uint32_t to,uint32_t timestamp,
                                                                                                   uint32_t flags,int update_mempool,int offset);
    int SetApproval(const void* lpUpgrade,uint32_t approval,const void* lpAdmin,uint32_t from,uint32_t timestamp,uint32_t flags,int update_mempool,int offset);
    int Commit(const void* lpMiner,const void* lpHash);
    int RollBack(int block);
    int RollBack();
    
    int ClearMemPool();
    int CopyMemPool();
    int RestoreMemPool();
    
    int SetCheckPoint();
    int RollBackToCheckPoint();
    
    int SetRollBackPos(int block,int offset,int inmempool);
    void ResetRollBackPos();
    int RewindToRollBackPos(mc_PermissionLedgerRow *row);
    
    uint32_t GetAllPermissions(const void* lpEntity,const void* lpAddress,uint32_t type);
    uint32_t GetPermissionType(const char *str,uint32_t full_type);
    uint32_t GetPermissionType(const char *str,const void *entity_details);
    uint32_t GetPossiblePermissionTypes(uint32_t entity_type);
    uint32_t GetPossiblePermissionTypes(const void *entity_details);
    uint32_t GetCustomLowPermissionTypes();
    uint32_t GetCustomHighPermissionTypes();
    
    int GetAdminCount();
    int GetMinerCount();
    int GetActiveMinerCount();
    
    int GetBlockMiner(uint32_t block,unsigned char* lpMiner);
    uint32_t FindGovernanceModelChange(uint32_t from,uint32_t to);

    int IsApproved(const void* lpUpgrade, int check_current_block);
    
    int CanConnect(const void* lpEntity,const void* lpAddress);
    int CanConnectForVerify(const void* lpEntity,const void* lpAddress);
    int CanSend(const void* lpEntity,const void* lpAddress);
    int CanReceive(const void* lpEntity,const void* lpAddress);
    int CanWrite(const void* lpEntity,const void* lpAddress);
    int CanRead(const void* lpEntity,const void* lpAddress);
    int CanCreate(const void* lpEntity,const void* lpAddress);
    int CanIssue(const void* lpEntity,const void* lpAddress);
    int CanMine(const void* lpEntity,const void* lpAddress);
    int CanAdmin(const void* lpEntity,const void* lpAddress);    
    int CanActivate(const void* lpEntity,const void* lpAddress);    
    int CanCustom(const void* lpEntity,const void* lpAddress,uint32_t permission);    
    int FilterApproved(const void* lpEntity,const void* lpAddress);
    
    int CanMineBlock(const void* lpAddress,uint32_t block);
    
    int IsActivateEnough(uint32_t type);
    

    mc_Buffer *GetPermissionList(const void* lpEntity,const void* lpAddress,uint32_t type,mc_Buffer *old_buffer);
    mc_Buffer *GetUpgradeList(const void* lpUpgrade,mc_Buffer *old_buffer);
    mc_Buffer *GetPermissionDetails(mc_PermissionDetails *plsRow);
    void FreePermissionList(mc_Buffer *permissions);
    
    void MempoolPermissionsCopy();
    int MempoolPermissionsCheck(int from, int to);

    int RollBackBeforeMinerVerification(uint32_t block);
    int RestoreAfterMinerVerification();
    void SaveTmpCounts();    
    int StoreBlockInfo(const void* lpMiner,const void* lpHash);    
    int IncrementBlock(uint32_t admin_miner_count);    
    int GetBlockMiner(const void* lpHash,unsigned char* lpMiner,uint32_t *lpAdminMinerCount);
    int GetBlockAdminMinerGrants(const void* lpHash,int record,int32_t *offsets);
    int CanMineBlockOnFork(const void* lpAddress,uint32_t block,uint32_t last_after_fork);
    int IsBarredByDiversity(uint32_t block,uint32_t last,int miner_count);
    
    
    
// Internal functions    
    int Zero();    
    int Destroy();

    void Lock(int write_mode);
    void UnLock();
    
    int SetPermissionInternal(const void* lpEntity,const void* lpAddress,uint32_t type,const void* lpAdmin,uint32_t from,uint32_t to,uint32_t timestamp,
                                                                                                           uint32_t flags,int update_mempool,int offset);
    int CanConnectInternal(const void* lpEntity,const void* lpAddress,int with_implicit);
    int CommitInternal(const void* lpMiner,const void* lpHash);
    int StoreBlockInfoInternal(const void* lpMiner,const void* lpHash,int update_counts);    
    int RollBackInternal(int block);
    uint32_t CalculateBlockFlags();
    int FindLastAllowedMinerRow(mc_PermissionLedgerRow *row,uint32_t block,int prev_result);
    
    int IsApprovedInternal(const void* lpUpgrade, int check_current_block);
    
    int UpdateCounts();
    int AdminConsensus(const void* lpEntity,uint32_t type);
    
    int ClearMemPoolInternal();

    
    
    int RequiredForConsensus(const void* lpEntity,const void* lpAddress,uint32_t type,uint32_t from,uint32_t to);
    int VerifyConsensus(mc_PermissionLedgerRow *newRow,mc_PermissionLedgerRow *lastRow,int *remaining);
    int VerifyBlockHash(int32_t block,const void* lpHash);
    int FillPermissionDetails(mc_PermissionDetails *plsRow,mc_Buffer *plsDetailsBuffer);    
    
    uint32_t GetPermission(const void* lpAddress,uint32_t type);
    uint32_t GetPermission(const void* lpEntity,const void* lpAddress,uint32_t type);
    uint32_t GetPermission(const void* lpAddress,uint32_t type,mc_PermissionLedgerRow *row);
    uint32_t GetPermission(const void* lpEntity,const void* lpAddress,uint32_t type,mc_PermissionLedgerRow *row,int checkmempool);
    int IsSetupPeriod();
    

    void LogString(const char *message);
    void Dump();
    
    
} mc_Permissions;



#endif	/* MULTICHAIN_PERMISSION_H */

