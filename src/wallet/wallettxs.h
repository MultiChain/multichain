// Copyright (c) 2014-2016 The Bitcoin Core developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#ifndef MULTICHAIN_WALLETTXS_H
#define	MULTICHAIN_WALLETTXS_H

#include "utils/util.h"
#include "structs/base58.h"
#include "wallet/wallet.h"
#include "multichain/multichain.h"
#include "wallet/wallettxdb.h"
#include "wallet/chunkdb.h"
#include "wallet/chunkcollector.h"

#define MC_TDB_MAX_OP_RETURN_SIZE             256

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
    
    int GetEntityListCount();
    mc_TxEntityStat *GetEntity(int row);

    std::string Summary();                                                      // Wallet summary
    
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
    
    
    
} mc_WalletTxs;



#endif	/* MULTICHAIN_WALLETTXS_H */

