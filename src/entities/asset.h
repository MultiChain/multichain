// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#ifndef MULTICHAIN_ASSET_H
#define	MULTICHAIN_ASSET_H

#include "utils/declare.h"
#include "utils/dbwrapper.h"

#define MC_AST_ASSET_REF_SIZE        10
#define MC_AST_ASSET_BUF_TOTAL_SIZE  22
#define MC_AST_SHORT_TXID_OFFSET     16
#define MC_AST_SHORT_TXID_SIZE       16

#define MC_AST_ASSET_BUFFER_REF_SIZE        32
#define MC_AST_ASSET_FULLREF_SIZE           36
#define MC_AST_ASSET_QUANTITY_OFFSET        36
#define MC_AST_ASSET_QUANTITY_SIZE           8
#define MC_AST_ASSET_FULLREF_BUF_SIZE       48

#define MC_AST_ASSET_REF_TYPE_REF            0
#define MC_AST_ASSET_REF_TYPE_SHORT_TXID     1
#define MC_AST_ASSET_REF_TYPE_TXID           2

#define MC_AST_ASSET_REF_TYPE_GENESIS      256 
#define MC_AST_ASSET_REF_TYPE_SPECIAL      512

#define MC_ENT_ENTITY_RESTRICTION_NONE           0x00000000
#define MC_ENT_ENTITY_RESTRICTION_ONCHAIN        0x00000001
#define MC_ENT_ENTITY_RESTRICTION_OFFCHAIN       0x00000002
#define MC_ENT_ENTITY_RESTRICTION_NEED_SALTED    0x00000004



#define MC_ENT_REF_SIZE                                10
#define MC_ENT_REF_PREFIX_SIZE                          2
#define MC_ENT_MAX_NAME_SIZE                           32
#define MC_ENT_MAX_ITEM_KEY_SIZE                      256
#define MC_ENT_MAX_SCRIPT_SIZE_BEFORE_FILTERS        4096
#define MC_ENT_MAX_SCRIPT_SIZE                      65536
#define MC_ENT_MAX_FIXED_FIELDS_SIZE                  128 
#define MC_ENT_MAX_STORED_ISSUERS                     128 
#define MC_ENT_SCRIPT_ALLOC_SIZE                    66000 // > MC_ENT_MAX_SCRIPT_SIZE + MC_ENT_MAX_FIXED_FIELDS_SIZE + 27*MC_ENT_MAX_STORED_ISSUERS

#define MC_ENT_KEY_SIZE              32
#define MC_ENT_KEYTYPE_TXID           0x00000001
#define MC_ENT_KEYTYPE_REF            0x00000002
#define MC_ENT_KEYTYPE_NAME           0x00000003
#define MC_ENT_KEYTYPE_SHORT_TXID     0x00000004
#define MC_ENT_KEYTYPE_MASK           0x000000FF
#define MC_ENT_KEYTYPE_FOLLOW_ON      0x00000100

#define MC_ENT_TYPE_ANY               0xFF
#define MC_ENT_TYPE_NONE              0x00
#define MC_ENT_TYPE_ASSET             0x01
#define MC_ENT_TYPE_STREAM            0x02
#define MC_ENT_TYPE_STREAM_MAX        0x0F
#define MC_ENT_TYPE_UPGRADE           0x10
#define MC_ENT_TYPE_FILTER            0x11
#define MC_ENT_TYPE_LICENSE_TOKEN     0x12
#define MC_ENT_TYPE_VARIABLE          0x13
#define MC_ENT_TYPE_MAX               0x13

#define MC_ENT_SPRM_NAME                      0x01                              // Cross-entity parameters
#define MC_ENT_SPRM_FOLLOW_ONS                0x02
#define MC_ENT_SPRM_ISSUER                    0x03
#define MC_ENT_SPRM_ANYONE_CAN_WRITE          0x04
#define MC_ENT_SPRM_JSON_DETAILS              0x05
#define MC_ENT_SPRM_PERMISSIONS               0x06
#define MC_ENT_SPRM_RESTRICTIONS              0x07
#define MC_ENT_SPRM_JSON_VALUE                0x08

#define MC_ENT_SPRM_ASSET_MULTIPLE            0x41                              // Entity-specific parameters
#define MC_ENT_SPRM_UPGRADE_PROTOCOL_VERSION  0x42
#define MC_ENT_SPRM_UPGRADE_START_BLOCK       0x43
#define MC_ENT_SPRM_UPGRADE_CHAIN_PARAMS      0x44
#define MC_ENT_SPRM_FILTER_RESTRICTIONS       0x45
#define MC_ENT_SPRM_FILTER_CODE               0x46
#define MC_ENT_SPRM_FILTER_TYPE               0x47

#define MC_ENT_SPRM_ASSET_TOTAL               0x51                              // Optimization parameters
#define MC_ENT_SPRM_CHAIN_INDEX               0x52 
#define MC_ENT_SPRM_LEFT_POSITION             0x53 

#define MC_ENT_SPRM_LICENSE_LICENSE_HASH      0x60                              // License parameters
#define MC_ENT_SPRM_LICENSE_ISSUE_ADDRESS     0x61
#define MC_ENT_SPRM_LICENSE_CONFIRMATION_TIME 0x62
#define MC_ENT_SPRM_LICENSE_CONFIRMATION_REF  0x63
#define MC_ENT_SPRM_LICENSE_RESERVED_0X64     0x64
#define MC_ENT_SPRM_LICENSE_RESERVED_0X65     0x65
#define MC_ENT_SPRM_LICENSE_RESERVED_0X66     0x66
#define MC_ENT_SPRM_LICENSE_RESERVED_0X67     0x67
#define MC_ENT_SPRM_LICENSE_RESERVED_0X68     0x68
#define MC_ENT_SPRM_LICENSE_PUBKEY            0x69
#define MC_ENT_SPRM_LICENSE_MIN_NODE          0x6A
#define MC_ENT_SPRM_LICENSE_MIN_PROTOCOL      0x6B
#define MC_ENT_SPRM_LICENSE_RESERVED_0X6C     0x6C
#define MC_ENT_SPRM_LICENSE_RESERVED_0X6D     0x6D
#define MC_ENT_SPRM_LICENSE_CONFIRMATION_DETAILS  0x6E
#define MC_ENT_SPRM_LICENSE_SIGNATURE         0x6F


#define MC_ENT_SPRM_TIMESTAMP                 0x81                              // Chunk DB parameters
#define MC_ENT_SPRM_CHUNK_HASH                0x82
#define MC_ENT_SPRM_SOURCE_TXID               0x83
#define MC_ENT_SPRM_SOURCE_VOUT               0x84
#define MC_ENT_SPRM_CHUNK_SIZE                0x85
#define MC_ENT_SPRM_CHUNK_DETAILS             0x86
#define MC_ENT_SPRM_CHUNK_DATA                0x87
#define MC_ENT_SPRM_ITEM_COUNT                0x88
#define MC_ENT_SPRM_SALT                      0x89

#define MC_ENT_SPRM_FILE_END                  0xFF

#define MC_ENT_FLAG_OFFSET_IS_SET     0x00000001
#define MC_ENT_FLAG_NAME_IS_SET       0x00000010



/** Database record structure */

typedef struct mc_EntityDBRow
{    
    unsigned char m_Key[MC_ENT_KEY_SIZE];                                       // Entity key size - txid/entity-ref/name
    uint32_t m_KeyType;                                                         // Entity key type - MC_ENT_KEYTYPE_ constants
    int32_t m_Block;                                                            // Block entity is confirmed in
    int64_t m_LedgerPos;                                                        // Position in the ledger corresponding to this key
    int64_t m_ChainPos;                                                         // Position in the ledger corresponding to last object in the chain
    uint32_t m_EntityType;                                                      // Entity type - MC_ENT_TYPE_ constants
    uint32_t m_Reserved1;                                                       // Reserved to align to 64 bytes
    
    void Zero();
} mc_EntityDBRow;

/** Database */

typedef struct mc_EntityDB
{   
    char m_FileName[MC_DCT_DB_MAX_PATH];                                        // Full file name
    mc_Database *m_DB;                                                          // Database object
    uint32_t m_KeyOffset;                                                       // Offset of the key in mc_EntityDBRow structure, 0
    uint32_t m_KeySize;                                                         // Size of the database key, 36
    uint32_t m_ValueOffset;                                                     // Offset of the value in mc_EntityDBRow structure, 36 
    uint32_t m_ValueSize;                                                       // Size of the database value, 12 for protocol<=10003,28 otherwise 
    uint32_t m_TotalSize;                                                       // Totals size of the database row
    mc_EntityDB()
    {
        Zero();
    }
    
    ~mc_EntityDB()
    {
        Close();
    }
    void Zero();
    int Open();
    int Close();
    void SetName(const char *name);
} mc_EntityDB;

/** Ledger and mempool record structure */

typedef struct mc_EntityLedgerRow
{    
    unsigned char m_Key[MC_ENT_KEY_SIZE];                                       // Entity key size - txid/entity-ref/name
    uint32_t m_KeyType;                                                         // Entity key type - MC_ENT_KEYTYPE_ constants
    int32_t m_Block;                                                            // Block entity is confirmed in
    int32_t m_Offset;                                                           // Offset of the entity in the block
    uint32_t m_ScriptSize;                                                      // Script Size
    int64_t m_Quantity;                                                         // Total quantity of the entity (including follow-ons)
    uint32_t m_EntityType;                                                      // Entity type - MC_ENT_TYPE_ constants
    int32_t m_ExtendedScript;                                                   // Size of extended script when in file, row+1 in temp buffer if in mempool
    int64_t m_PrevPos;                                                          // Position of the previous entity in the ledger
    int64_t m_FirstPos;                                                         // Position in the ledger corresponding to first object in the chain
    int64_t m_LastPos;                                                          // Position in the ledger corresponding to last object in the chain before this object
    int64_t m_ChainPos;                                                         // Position in the ledger corresponding to last object in the chain
    int32_t m_ScriptMemPoolPos;                                                 // Position of script in temp script buffer, -1 if not in temp buffer
    int32_t m_ExtendedScriptMemPoolPos;                                         // Position of extended script in temp script buffer, -1 if not in temp buffer
    unsigned char m_Script[MC_ENT_SCRIPT_ALLOC_SIZE];                           // Script > MC_ENT_MAX_SCRIPT_SIZE + MC_ENT_MAX_FIXED_FIELDS_SIZE + 27*MC_ENT_MAX_STORED_ISSUERS
    
    void Zero();
} mc_EntityLedgerRow;

/** Entity details structure */

typedef struct mc_EntityDetails
{
    unsigned char m_Ref[MC_ENT_REF_SIZE];                                       // Entity reference
    unsigned char m_FullRef[MC_AST_ASSET_QUANTITY_OFFSET];                      // Full Entity reference, derived from short txid from v 10007
    char m_Name[MC_ENT_MAX_NAME_SIZE+6];                                        // Entity name
    uint32_t m_Flags;
    uint32_t m_Permissions;
    uint32_t m_ScriptPermissions;
    uint32_t m_Restrictions;
    unsigned char m_Reserved[36];   
    mc_EntityLedgerRow m_LedgerRow;
    void Zero();
    void Set(mc_EntityLedgerRow *row);
    const char* GetName();
    const unsigned char* GetTxID();
    const unsigned char* GetRef();    
    const unsigned char* GetFullRef();    
    const unsigned char* GetShortRef();
    const unsigned char* GetScript();    
    const unsigned char* GetParamUpgrades(int *size);    
    
    uint32_t GetScriptSize();
    int IsUnconfirmedGenesis();    
    int GetAssetMultiple();
    uint32_t GetFilterType();
    int IsFollowOn(); 
//    int HasFollowOns(); 
    int AllowedFollowOns(); 
    uint32_t Permissions(); 
    uint32_t Restrictions(); 
    int AnyoneCanWrite(); 
    int AnyoneCanRead(); 
    int UpgradeProtocolVersion(); 
    uint32_t UpgradeStartBlock(); 
    uint64_t GetQuantity();
    uint32_t GetEntityType();    
    const void* GetSpecialParam(uint32_t param,size_t* bytes);
    const void* GetSpecialParam(uint32_t param,size_t* bytes,int check_extended_script);
    const void* GetParam(const char *param,size_t* bytes);
    int32_t NextParam(uint32_t offset,uint32_t* param_value_start,size_t *bytes);
}mc_EntityDetails;

/** Ledger */

typedef struct mc_EntityLedger
{   
    char m_FileName[MC_DCT_DB_MAX_PATH];                                        // Full file name
    int m_FileHan;                                                              // File handle
    uint32_t m_KeyOffset;                                                       // Offset of the key in mc_EntityLedgerRow structure, 0
    uint32_t m_KeySize;                                                         // Size of the ledger key, 36
    uint32_t m_ValueOffset;                                                     // Offset of the value in mc_EntityLedgerRow structure, 36 
    uint32_t m_ValueSize;                                                       // Size of the ledger value 28 if protocol<=10003, 60 otherwise
    uint32_t m_TotalSize;                                                       // Totals size of the ledger row
    uint32_t m_MemPoolSize;                                                     // Totals size of the ledger row in mempool
    uint32_t m_MaxScriptMemPoolSize;                                            // Maximal script size stored in mempool
    unsigned char m_ZeroBuffer[96];
   
    mc_EntityLedger()
    {
        Zero();
    }
    
    ~mc_EntityLedger()
    {
        Close();
    }
    
    void Zero();
    int Open();
    int Close();
    void Flush();
    void SetName(const char *name);
    int GetRow(int64_t pos,mc_EntityLedgerRow *row);
    int64_t GetSize();
    int SetRow(int64_t pos,mc_EntityLedgerRow *row);
    int SetZeroRow(mc_EntityLedgerRow *row);
    
} mc_EntityLedger;


typedef struct mc_AssetDB
{    
    mc_EntityDB *m_Database;
    mc_EntityLedger *m_Ledger;
    
    mc_Buffer   *m_MemPool;
    mc_Buffer   *m_TmpRelevantEntities;
    mc_Buffer   *m_ShortTxIDCache;
    mc_Script   *m_ExtendedScripts;
    mc_Script   *m_RowExtendedScript;
    
    char m_Name[MC_PRM_NETWORK_NAME_MAX_SIZE+1]; 
    int m_Block;
    int64_t m_PrevPos;
    int64_t m_Pos;
    int64_t m_CheckPointPos;
    uint64_t m_CheckPointMemPoolSize;
    int m_DBRowCount;
    mc_RollBackPos m_RollBackPos;

    mc_AssetDB()
    {
        Zero();
    }
    
    ~mc_AssetDB()
    {
        Destroy();
    }
    

// External functions    

    int Initialize(const char *name,int mode);
        
    int InsertEntity(const void* txid, int offset, int entity_type, const void *script,size_t script_size, const void* special_script, size_t special_script_size,int32_t extended_script_row,int update_mempool);
    int InsertAsset(const void* txid, int offset, int asset_type, uint64_t quantity,const char *name,int multiple,const void *script,size_t script_size, const void* special_script, size_t special_script_size,int32_t extended_script_row,int update_mempool);
    int InsertAssetFollowOn(const void* txid, int offset, uint64_t quantity, const void *script,size_t script_size, const void* special_script, size_t special_script_size,int32_t extended_script_row,const void* original_txid,int update_mempool);
    int Commit();
    int RollBack(int block);
    int RollBack();
    int ClearMemPool();

    int SetCheckPoint();
    int RollBackToCheckPoint();
    
    int GetEntity(mc_EntityLedgerRow *row);

    int FindEntityByTxID(mc_EntityDetails *entity, const unsigned char* txid);
    int FindEntityByShortTxID (mc_EntityDetails *entity, const unsigned char* short_txid);
    int FindEntityByRef (mc_EntityDetails *entity, const unsigned char* asset_ref);
    int FindEntityByName(mc_EntityDetails *entity, const char* name);    
    int FindEntityByFollowOn(mc_EntityDetails *entity, const unsigned char* txid);    
    int FindEntityByFullRef (mc_EntityDetails *entity, unsigned char* full_ref);
    int FindLastEntity(mc_EntityDetails *last_entity, mc_EntityDetails *entity);    
    int FindLastEntityByGenesis(mc_EntityDetails *last_entity, mc_EntityDetails *genesis_entity);    
    
    unsigned char *CachedTxIDFromShortTxID(unsigned char *short_txid);
    int SetRollBackPos(int block,int offset,int inmempool);
    void ResetRollBackPos();
    
    
    void Dump();
    mc_Buffer *GetEntityList(mc_Buffer *old_result,const void* txid,uint32_t entity_type);
    void FreeEntityList(mc_Buffer *entities);
    mc_Buffer *GetFollowOns(const void* txid);
    mc_Buffer *GetFollowOnsByLastEntity(mc_EntityDetails *last_entity,int count,int start);
    int HasFollowOns(const void* txid);
    int64_t GetTotalQuantity(mc_EntityDetails *entity);
    int64_t GetTotalQuantity(mc_EntityDetails *entity,int32_t *chain_size);
    int64_t GetChainLeftPosition(mc_EntityDetails *entity,int32_t index);
    
    void RemoveFiles();
    uint32_t MaxEntityType();
    int MaxScriptSize();
    int MaxStoredIssuers();
    
//Internal functions    
    int Zero();
    int Destroy();
    int64_t GetTotalQuantity(mc_EntityLedgerRow *row,int32_t *chain_size);
    int64_t GetChainLeftPosition(mc_EntityLedgerRow *row,int32_t index);
    int64_t GetChainPosition(mc_EntityLedgerRow *row,int32_t index);
    void AddToMemPool(mc_EntityLedgerRow *row);
    void GetFromMemPool(mc_EntityLedgerRow *row,int mprow);
        
} mc_AssetDB;



uint32_t mc_GetABScriptType(void *ptr);
void mc_SetABScriptType(void *ptr,uint32_t type);
uint32_t mc_GetABRefType(void *ptr);
void mc_SetABRefType(void *ptr,uint32_t type);
int64_t mc_GetABQuantity(void *ptr);
void mc_SetABQuantity(void *ptr,int64_t quantity);
unsigned char* mc_GetABRef(void *ptr);
void mc_SetABRef(void *ptr,void *ref);
void mc_ZeroABRaw(void *ptr);
void mc_InitABufferMap(mc_Buffer *buf);
void mc_InitABufferDefault(mc_Buffer *buf);


#endif	/* MULTICHAIN_ASSET_H */

