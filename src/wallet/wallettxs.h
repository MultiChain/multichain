// Copyright (c) 2014-2016 The Bitcoin Core developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#ifndef MULTICHAIN_WALLETTXS_H
#define	MULTICHAIN_WALLETTXS_H

#include "core/main.h"
#include "utils/util.h"
#include "structs/base58.h"
#include "wallet/wallet.h"
#include "multichain/multichain.h"
#include "wallet/wallettxdb.h"
#include "wallet/chunkdb.h"
#include "wallet/chunkcollector.h"

#define MC_TDB_MAX_OP_RETURN_SIZE             256

#define MC_MTX_TAG_DIRECT_MASK                           0x00000000FFFFFFFF   
#define MC_MTX_TAG_EXTENSION_MASK                        0xFFFFFFFF00000000
#define MC_MTX_TAG_EXTENSION_TOTAL_SIZE                                  40

#define MC_MTX_TAG_NONE                                  0x0000000000000000
#define MC_MTX_TAG_EXTENDED_TAGS                         0x0000000000000001
#define MC_MTX_TAG_P2SH                                  0x0000000000000002
#define MC_MTX_TAG_COINBASE                              0x0000000000000004
#define MC_MTX_TAG_OP_RETURN                             0x0000000000000008
#define MC_MTX_TAG_NATIVE_TRANSFER                       0x0000000000000010
#define MC_MTX_TAG_ENTITY_CREATE                         0x0000000000000020
#define MC_MTX_TAG_ENTITY_UPDATE                         0x0000000000000040
#define MC_MTX_TAG_ASSET_GENESIS                         0x0000000000000080
#define MC_MTX_TAG_ASSET_FOLLOWON                        0x0000000000000100
#define MC_MTX_TAG_ASSET_TRANSFER                        0x0000000000000200
#define MC_MTX_TAG_GRANT_ENTITY                          0x0000000000000400
#define MC_MTX_TAG_REVOKE_ENTITY                         0x0000000000000800
#define MC_MTX_TAG_GRANT_LOW                             0x0000000000001000
#define MC_MTX_TAG_REVOKE_LOW                            0x0000000000002000
#define MC_MTX_TAG_GRANT_HIGH                            0x0000000000004000
#define MC_MTX_TAG_REVOKE_HIGH                           0x0000000000008000
#define MC_MTX_TAG_INLINE_DATA                           0x0000000000010000 
#define MC_MTX_TAG_RAW_DATA                              0x0000000000020000 
#define MC_MTX_TAG_STREAM_ITEM                           0x0000000000040000 
#define MC_MTX_TAG_OFFCHAIN                              0x0000000000080000 
#define MC_MTX_TAG_MULTIPLE_ASSETS                       0x0000000000100000 
#define MC_MTX_TAG_MULTIPLE_STREAM_ITEMS                 0x0000000000200000 
#define MC_MTX_TAG_FILTER_APPROVAL                       0x0000000000400000
#define MC_MTX_TAG_COMBINE                               0x0000000000800000

#define MC_MTX_TAG_ENTITY_MASK                           0x00000000FF000000     
#define MC_MTX_TAG_ENTITY_MASK_SHIFT                                     24

#define MC_MTX_TAG_NO_SINGLE_TX_TAG                      0x0000000100000000
#define MC_MTX_TAG_NO_KEY_SCRIPT_ID                      0x0000001000000000
#define MC_MTX_TAG_LICENSE_TOKEN                         0x0000002000000000
#define MC_MTX_TAG_UPGRADE_APPROVAL                      0x0000004000000000

#define MC_MTX_TFL_MULTIPLE_TXOUT_ASSETS                 0x0000000000000001
#define MC_MTX_TFL_IS_INPUT                              0x0000000000000001
/*
struct mc_AssetBalanceDetails
{
    uint256 m_TxID;
    uint32_t m_Vinout;
    uint32_t m_Flags;
    uint32_t m_AssetCount;
    uint32_t m_Reserved;
    int64_t m_Amount;
    int64_t m_Balance;    
};
*/
struct mc_TxAssetBalanceDetails
{
    uint256 m_TxID;
    int64_t m_Amount;
    int64_t m_Balance;    
};

struct mc_TxAddressAssetQuantity
{
    uint160 m_Address;
    uint160 m_Asset;
    int64_t m_Amount;
};

        
typedef struct mc_WalletCachedSubKey
{
    mc_TxEntity m_Entity;
    mc_TxEntity m_SubkeyEntity;
    uint160 m_SubKeyHash;
    uint32_t m_Flags;
    
    void Zero();
    void Set(mc_TxEntity* entity,mc_TxEntity* subkey_entity,uint160 subkey_hash,uint32_t flags);
    
    mc_WalletCachedSubKey()
    {
        Zero();
    }
    
} mc_WalletCachedSubKey;

typedef struct mc_WalletCachedAddTx
{
    std::vector<mc_Coin> m_TxOutsIn;
    std::vector<mc_Coin> m_TxOutsOut;
    std::vector<mc_WalletCachedSubKey> m_SubKeys;
    std::vector<mc_TxEntity> m_Entities;
    uint32_t m_Flags;
    bool fSingleInputEntity;

    
} mc_WalletCachedAddTx;

typedef struct mc_WalletTxs
{
    mc_TxDB *m_Database;
    mc_ChunkDB *m_ChunkDB;
    mc_ChunkCollector *m_ChunkCollector;
    CWallet *m_lpWallet;
    uint32_t m_Mode;
    std::map<COutPoint, mc_Coin> m_UTXOs[MC_TDB_MAX_IMPORTS];    
    std::map<uint256,CWalletTx> m_UnconfirmedSends;
    std::vector<uint256> m_UnconfirmedSendsHashes;
    std::map<uint256, CWalletTx> vAvailableCoins;    
 
    unsigned char* m_ChunkBuffer;
    mc_WalletTxs()
    {
        Zero();
    }
    
    ~mc_WalletTxs()
    {
        Destroy();
    }
    
    int Initialize(                                                             // Initialization
              const char *name,                                                 // Chain name
              uint32_t mode);                                                   // Unused

    int UpdateMode(uint32_t mode);
    
    int SetMode(                                                                // Sets wallet mode
                uint32_t mode,                                                  // Mode to set
                uint32_t mask);                                                 // Mask to set, old mode outside this mask will be untouched
    
    void BindWallet(CWallet *lpWallet);
    
    int AddEntity(mc_TxEntity *entity,uint32_t flags);                          // Adds entity to chain import
    int SaveEntityFlag(mc_TxEntity *entity,                                     // Changes Entity flag setting 
                   uint32_t flag,                                               // Flag to set/unset
                   int set_flag);                                               // 1 if set, 0 if unset
       
    mc_Buffer* GetEntityList();                                                 // Retruns list of chain import entities
       
    int AddTx(                                                                  // Adds tx to the wallet
              mc_TxImport *import,                                              // Import object, NULL if chain update
              const CTransaction& tx,                                           // Tx to add
              int block,                                                        // block height, -1 for mempool
              CDiskTxPos* block_pos,                                            // Position in the block
              uint32_t block_tx_index,                                          // Tx index in block
              uint256 block_hash);                                              // Block hash
                                           
    
    int AddTx(                                                                  // Adds tx to the wallet
              mc_TxImport *import,                                              // Import object, NULL if chain update
              const CWalletTx& tx,                                              // Tx to add
              int block,                                                        // block height, -1 for mempool
              CDiskTxPos* block_pos,                                            // Position in the block
              uint32_t block_tx_index,                                          // Tx index in block
              uint256 block_hash);                                              // Block hash
    
    int BeforeCommit(mc_TxImport *import);                                      // Should be called before re-adding tx while processing block
    int Commit(mc_TxImport *import);                                            // Commit when block was processed
    int RollBack(mc_TxImport *import,int block);                                // Rollback to specific block
    int CleanUpAfterBlock(mc_TxImport *import,int block,int prev_block);        // Should be called after full processing of the block, prev_block - block before UpdateTip
    
    int FindWalletTx(uint256 hash,mc_TxDefRow *txdef);                          // Returns only txdef
    CWalletTx GetWalletTx(uint256 hash,mc_TxDefRow *txdef,int *errOut);         // Returns wallet transaction. If not found returns empty transaction
    
    CWalletTx GetInternalWalletTx(uint256 hash,mc_TxDefRow *txdef,int *errOut);   
    
    int GetBlockItemIndex(                                                      // Returns item id for the last item confirmed in this block or before
                    mc_TxEntity *entity,                                        // Entity to return info for
                    int block);                                                 // Block to find item for
    
    
    int GetList(mc_TxEntity *entity,                                            // Returns Txs in range for specific entity
                    int from,                                                   // If positive - from this tx, if not positive - this number from the end
                    int count,                                                  // Number of txs to return
                    mc_Buffer *txs);                                            // Output list. mc_TxEntityRow
    
    int GetList(mc_TxEntity *entity,                                            // Returns Txs in range for specific entity
                    int generation,                                             // Entity generation
                    int from,                                                   // If positive - from this tx, if not positive - this number from the end
                    int count,                                                  // Number of txs to return
                    mc_Buffer *txs);                                            // Output list. mc_TxEntityRow
    
    int GetListSize(                                                            // Return total number of tx in the list for specific entity
                    mc_TxEntity *entity,                                       // Entity to return info for
                    int *confirmed);                                            // Out: number of confirmed items    

    int GetListSize(                                                            // Return total number of tx in the list for specific entity
                    mc_TxEntity *entity,                                        // Entity to return info for
                    int generation,                                            // Entity generation
                    int *confirmed);                                            // Out: number of confirmed items    
    
    int GetRow(
               mc_TxEntityRow *erow);
    
    int Unsubscribe(mc_Buffer *lpEntities,bool purge);                          // List of the entities to unsubscribe from
    
    mc_TxImport *StartImport(                                                   // Starts new import
                             mc_Buffer *lpEntities,                             // List of entities to import
                             int block,                                         // Star from this block            
                             int *err);                                         // Output. Error
    
    mc_TxImport *FindImport(                                                    // Find import with specific ID
                            int import_id);
    
    bool FindEntity(mc_TxEntityStat *entity);                                    // Finds entity in chain import
    
    int ImportGetBlock(                                                         // Returns last processed block in the import
                       mc_TxImport *import);
    
    int CompleteImport(mc_TxImport *import,uint32_t flags);                    // Completes import - merges with chain
    
    int DropImport(mc_TxImport *import);                                        // Drops uncompleted import

    std::string GetSubKey(void *hash,mc_TxDefRow *txdef,int *errOut);         
    std::string GetSubKey(mc_TxImport *import,void *hash,mc_TxDefRow *txdef,int *errOut);         
    
    int GetEntityListCount();
    mc_TxEntityStat *GetEntity(int row);

    std::string Summary();                                                      // Wallet summary
    
    int AddExplorerEntities(mc_Buffer *lpEntities);
    int AddExplorerTx(                                                          
              mc_TxImport *import,                                              // Import object, NULL if chain update
              const CTransaction& tx,                                           // Tx to add
              int block);                                                       // block height, -1 for mempool
    
    
// Internal functions
    
    void Zero();    
    int Destroy();
    
    int AddToUnconfirmedSends(int block,const CWalletTx& tx);
    int FlushUnconfirmedSends(int block);
    int SaveTxFlag(                                                             // Changes tx flag setting (if tx is found))
                   const unsigned char *hash,                                   // Tx ID
                   uint32_t flag,                                               // Flag to set/unset
                   int set_flag);                                               // 1 if set, 0 if unset
    
    int RollBackSubKeys(mc_TxImport *import,int block,mc_TxEntityStat *parent_entity,mc_Buffer *lpSubKeyEntRowBuffer); // Rollback subkeys to specific block

    int LoadUnconfirmedSends(int block,int file_block); 
    
    std::map<uint256,CWalletTx> GetUnconfirmedSends(int block,std::vector<uint256>& unconfirmedSendsHashes);                 // Internal. Retrieves list of unconfirmed txs sent by this wallet for specific block
    void GetSingleInputEntity(const CWalletTx& tx,mc_TxEntity *input_entity);
    int RemoveUnconfirmedSends(int block);              
    int SaveUTXOMap(int import_id,int block);
    int LoadUTXOMap(int import_id,int block);
    int RemoveUTXOMap(int import_id,int block);
    int GetBlock();
    
    void Lock();
    void UnLock();
    
    int WRPSync(int for_block);
    void WRPReadLock();
    void WRPWriteLock();
    void WRPReadUnLock();
    void WRPWriteUnLock();
    
    int WRPGetListSize(mc_TxEntity *entity,int *confirmed);
    
    int WRPGetListSize(mc_TxEntity *entity,int generation,int *confirmed);
    int WRPGetList(mc_TxEntity *entity,int generation,int from,int count,mc_Buffer *txs);    
    int WRPGetLastItem(mc_TxEntity *entity,int generation,mc_TxEntityRow *erow);    
    
    CWalletTx WRPGetWalletTx(uint256 hash,mc_TxDefRow *txdef,int *errOut);
    std::string WRPGetSubKey(void *hash,mc_TxDefRow *txdef,int *errOut);         
    int WRPGetRow(mc_TxEntityRow *erow);
    
    bool WRPFindEntity(mc_TxEntityStat *entity);                                    // Finds entity in chain import
    int WRPGetBlockItemIndex(                                                      // Returns item id for the last item confirmed in this block or before
                    mc_TxEntity *entity,                                        // Entity to return info for
                    int block);                                                 // Block to find item for
} mc_WalletTxs;



#endif	/* MULTICHAIN_WALLETTXS_H */

