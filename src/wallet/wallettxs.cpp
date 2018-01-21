// Copyright (c) 2014-2016 The Bitcoin Core developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2017 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "wallet/wallettxs.h"
#include "utils/core_io.h"

#include "json/json_spirit_utils.h"
#include "json/json_spirit_value.h"
using namespace json_spirit;

#include <boost/thread.hpp>
#include <boost/algorithm/string/replace.hpp>
#include "json/json_spirit_writer_template.h"

#define MC_TDB_UTXO_SET_WINDOW_SIZE        20

int64_t GetAdjustedTime();
void TxToJSON(const CTransaction& tx, const uint256 hashBlock, Object& entry);
bool CBitcoinAddressFromTxEntity(CBitcoinAddress &address,mc_TxEntity *lpEntity);

using namespace std;


void WalletTxNotify(mc_TxImport *imp,const CWalletTx& tx,int block,bool fFound,uint256 block_hash)
{
//    std::string strNotifyCmd = GetArg("-walletnotify", "");
    std::string strNotifyCmd = GetArg("-walletnotify", fFound ? "" : GetArg("-walletnotifynew", ""));
    if ( strNotifyCmd.empty() )
    {
        return;
    }
    if(imp->m_ImportID)
    {
        return;        
    }

    boost::replace_all(strNotifyCmd, "%s", tx.GetHash().ToString());

    boost::replace_all(strNotifyCmd, "%c", strprintf("%d",fFound ? 0 : 1));
    boost::replace_all(strNotifyCmd, "%n", strprintf("%d",block));
    boost::replace_all(strNotifyCmd, "%b", block_hash.ToString());
    boost::replace_all(strNotifyCmd, "%h", EncodeHexTx(*static_cast<const CTransaction*>(&tx)));

    string strAddresses="";
    string strEntities="";
    CBitcoinAddress address;
    mc_EntityDetails entity;
    uint256 txid;
    
    for(int i=0;i<imp->m_TmpEntities->GetCount();i++)
    {
        mc_TxEntity *lpent;
        lpent=(mc_TxEntity *)imp->m_TmpEntities->GetRow(i);
        if(lpent->m_EntityType & MC_TET_CHAINPOS)
        {
            switch(lpent->m_EntityType & MC_TET_TYPE_MASK)
            {
                case MC_TET_PUBKEY_ADDRESS:
                case MC_TET_SCRIPT_ADDRESS:
                    if(CBitcoinAddressFromTxEntity(address,lpent))
                    {
                        if(strAddresses.size())
                        {
                            strAddresses += ",";
                        }
                        strAddresses += address.ToString();
                    }
                    break;
                case MC_TET_STREAM:
                case MC_TET_ASSET:
                    if(mc_gState->m_Assets->FindEntityByShortTxID(&entity,lpent->m_EntityID))
                    {
                        if(strEntities.size())
                        {
                            strEntities += ",";                            
                        }        
                        txid=*(uint256*)entity.GetTxID();
                        strEntities += txid.ToString();
                    }
                    break;
            }
        }        
    }

    boost::replace_all(strNotifyCmd, "%a", (strAddresses.size() > 0) ? strAddresses : "\"\"");
    boost::replace_all(strNotifyCmd, "%e", (strEntities.size() > 0) ? strEntities : "\"\"");

    string str=strprintf("%s",mc_gState->m_NetworkParams->Name());              
    boost::replace_all(str, "\"", "\\\"");        
    boost::replace_all(strNotifyCmd, "%m", "\"" + str + "\"");
    
// If chain name (retrieved by %m) contains %j, it will be replaced by full JSON. 
// It is minor issue, but one should be careful when adding other "free text" specifications
    
    Object result;
    TxToJSON(tx, block_hash, result);        
    str=write_string(Value(result),false);
    boost::replace_all(str, "\"", "\\\"");        
    boost::replace_all(strNotifyCmd, "%j", "\"" + str + "\"");
    
    boost::thread t(runCommand, strNotifyCmd); // thread runs free        
}

void mc_Coin::Zero()
{
    m_EntityID=0;
    m_EntityType=MC_TET_NONE;
    m_Block=-1;
    m_Flags=0;    
    m_LockTime=0;
}

bool mc_Coin::IsFinal() const
{
    int nBlockHeight,nBlockTime;
    AssertLockHeld(cs_main);
    // Time based nLockTime implemented in 0.1.6
    if (m_LockTime == 0)
        return true;
    nBlockHeight = chainActive.Height();
    nBlockTime = GetAdjustedTime();
    if ((int64_t)m_LockTime < ((int64_t)m_LockTime < LOCKTIME_THRESHOLD ? (int64_t)nBlockHeight : nBlockTime))
        return true;
    
    return (m_Flags & MC_TFL_ALL_INPUTS_ARE_FINAL) > 0;
    
}

int mc_Coin::BlocksToMaturity() const
{
    if ((m_Flags & MC_TFL_IS_COINBASE) == 0)
    {
        return 0;
    }
    return max(0, (COINBASE_MATURITY+1) - GetDepthInMainChain());    
}

bool mc_Coin::IsTrusted() const
{
    if (!IsFinal())
    {
        return false;
    }
    int nDepth=GetDepthInMainChain();

    if (nDepth >= 1)
    {
        return true;
    }
    if (nDepth < 0)
    {
        return false;
    }

    return (m_Flags & MC_TFL_ALL_INPUTS_FROM_ME) > 0;
}

bool mc_Coin::IsTrustedNoDepth() const
{
    if (!IsFinal())
    {
        return false;
    }
    
    int nDepth=GetDepthInMainChain();

    if (nDepth < 0)
    {
        return false;
    }

    return (m_Flags & MC_TFL_ALL_INPUTS_FROM_ME) > 0;
}

int mc_Coin::GetDepthInMainChain() const
{
    int nDepth=0;
    if(m_Block >= 0)
    {
        nDepth= chainActive.Height()-m_Block+1;
    }
    return nDepth;
}

string mc_Coin::ToString() const 
{
    CBitcoinAddress addr;
    if( (m_EntityType & MC_TET_TYPE_MASK) == MC_TET_PUBKEY_ADDRESS )
    {
        CKeyID lpKeyID=CKeyID(m_EntityID);
        addr=CBitcoinAddress(lpKeyID);
    }
    if( (m_EntityType & MC_TET_TYPE_MASK) == MC_TET_SCRIPT_ADDRESS )
    {
        CScriptID lpScriptID=CScriptID(m_EntityID);
        addr=CBitcoinAddress(lpScriptID);
    }
    
    return strprintf("Coin: %s %s (%08X,%d) %s",m_OutPoint.ToString().c_str(),m_TXOut.ToString().c_str(),m_Flags,m_Block,addr.ToString().c_str());
}



void mc_WalletTxs::Zero()
{
    int i;
    m_Database=NULL;
    m_lpWallet=NULL;
    for(i=0;i<MC_TDB_MAX_IMPORTS;i++)
    {
        m_UTXOs[i].clear();
    }
    m_Mode=MC_WMD_NONE;
}

void mc_WalletTxs::BindWallet(CWallet *lpWallet)
{
    m_lpWallet=lpWallet;    
}

string mc_WalletTxs::Summary()
{
    if(m_Database->m_Imports->m_Block != m_Database->m_DBStat.m_Block)
    {
        LogPrintf("wtxs: ERROR! Wallet block count mismatch: %d -> %d\n",m_Database->m_Imports->m_Block != m_Database->m_DBStat.m_Block);
    }
    return strprintf("Block: %d, Txs: %d, Unconfirmed: %d, UTXOs: %d",
            m_Database->m_Imports->m_Block,
            (int)m_Database->m_DBStat.m_Count,
            (int)m_UnconfirmedSends.size(),
            (int)m_UTXOs[0].size());
}


int mc_WalletTxs::Initialize(
          const char *name,  
          uint32_t mode)
{
    int err,i;
    
    
    m_Database=new mc_TxDB;
    
    m_Mode=mode;
    err=m_Database->Initialize(name,mode);
    
    if(err == MC_ERR_NOERROR)
    {
        m_Mode=m_Database->m_DBStat.m_InitMode;
        for(i=0;i<MC_TDB_MAX_IMPORTS;i++)
        {
            if(m_Database->m_Imports[i].m_Entities)
            {
                err=LoadUTXOMap(m_Database->m_Imports[i].m_ImportID,m_Database->m_Imports[i].m_Block);
            }
        }
        
    }
    
    m_Mode |= (mode & MC_WMD_AUTOSUBSCRIBE_STREAMS);
    m_Mode |= (mode & MC_WMD_AUTOSUBSCRIBE_ASSETS);
    
    if(err == MC_ERR_NOERROR)
    {
//        m_UnconfirmedSends= GetUnconfirmedSends(m_Database->m_DBStat.m_Block,m_UnconfirmedSendsHashes);
        m_UnconfirmedSends.clear();
        m_UnconfirmedSendsHashes.clear();
        if(m_Database->m_DBStat.m_Block > 0)                                    // If node crashed during commit we may want to reload unconfirmed txs of the previous block    
        {
            LoadUnconfirmedSends(m_Database->m_DBStat.m_Block,m_Database->m_DBStat.m_Block-1);            
        }
        LoadUnconfirmedSends(m_Database->m_DBStat.m_Block,m_Database->m_DBStat.m_Block);
    }    
    return err;
}

int mc_WalletTxs::SetMode(uint32_t mode, uint32_t mask)
{
    m_Mode &= ~mask;
    m_Mode |= mode;
    
    return MC_ERR_NOERROR;
}

int mc_WalletTxs::Destroy()
{
    if(m_Database)
    {
        delete m_Database;
    }

    Zero();
    return MC_ERR_NOERROR;    
    
}

bool mc_WalletTxs::FindEntity(mc_TxEntityStat *entity)
{
    int row;
    if((m_Mode & MC_WMD_TXS) == 0)
    {
        return false;
    }    
    if(m_Database == NULL)
    {
        return false;
    }
    row=m_Database->FindEntity(NULL,entity);
    return (row >= 0) ? true : false;
}


int mc_WalletTxs::AddEntity(mc_TxEntity *entity,uint32_t flags)
{
    int err;
    if((m_Mode & MC_WMD_TXS) == 0)
    {
        return MC_ERR_NOERROR;
    }    
    if(m_Database == NULL)
    {
        return MC_ERR_INTERNAL_ERROR;
    }
    m_Database->Lock(1,0);
    err=m_Database->AddEntity(entity,flags);
    m_Database->UnLock();
    return err;
}

mc_Buffer* mc_WalletTxs::GetEntityList()
{
    if((m_Mode & MC_WMD_TXS) == 0)
    {
        return NULL;
    }    
    if(m_Database == NULL)
    {
        return NULL;
    }
    return m_Database->m_Imports->m_Entities;
}

void mc_WalletTxs::Lock()
{
    if(m_Database)
    {
        m_Database->Lock(0,0);
    }
}

void mc_WalletTxs::UnLock()
{
    if(m_Database)
    {
        m_Database->UnLock();
    }    
}

int mc_WalletTxs::GetBlock()
{
    if(m_Database)
    {
        return m_Database->m_DBStat.m_Block;
    }
    return -1;
}

/*
 BeforeCommit - called before adding block txs 
 
 Raw Txs: Do Nothing
 Entity lists by blockchain position: Clear mempool 
 Entity lists by timestamp: Do nothing
 UTXOs: Restore UTXO from disk (clear mempool)
 Unconfirmed txs: Do Nothing
 */

int mc_WalletTxs::BeforeCommit(mc_TxImport *import)
{
    int err,count;
    mc_TxImport *imp;
    if((m_Mode & MC_WMD_TXS) == 0)
    {
        return MC_ERR_NOERROR;
    }    
    if(m_Database == NULL)
    {
        return MC_ERR_INTERNAL_ERROR;
    }
    imp=m_Database->m_Imports;
    if(import)
    {        
        imp=import;
    }
    m_Database->Lock(1,0);

    count=0;
    if(imp->m_ImportID == 0)                                                    // No need for non-chain imports as there is no mempool there
    {
        count=m_Database->m_MemPools[0]->GetCount();
    }
    err=m_Database->BeforeCommit(imp);
    
    if(err == MC_ERR_NOERROR)                                                   // Clearing all UTXO changes since last block
    {
        if(count)       
        {
            err=LoadUTXOMap(imp->m_ImportID,m_Database->m_DBStat.m_Block);
        }
    }
    
    if(err)
    {
        LogPrintf("wtxs: BeforeCommit: Error: %d\n",err);       
        m_Database->Dump("Error in BeforeCommit");
    }
    if(fDebug)LogPrint("wallet","wtxs: BeforeCommit: Import: %d, Block: %d\n",imp->m_ImportID,imp->m_Block);
    
    m_Database->UnLock();
    return err;    
}

/*
 Commit - called after adding block txs 
 
 Raw Txs: Store raw tx mempool on disk
 Entity lists by blockchain position: Store mempool on disk
 Entity lists by timestamp: Store mempool on disk
 UTXOs: Store UTXO on disk
 Unconfirmed txs: After committing entities: Copy list from (block-1) to (block) without confirmed transactions
 */

int mc_WalletTxs::Commit(mc_TxImport *import)
{
    int err;
    mc_TxImport *imp;
    
    if((m_Mode & MC_WMD_TXS) == 0)
    {
        return MC_ERR_NOERROR;
    }    
    if(m_Database == NULL)
    {
        return MC_ERR_INTERNAL_ERROR;
    }
    err = MC_ERR_NOERROR;
    imp=m_Database->m_Imports;
    if(import)
    {        
        imp=import;
    }    
    m_Database->Lock(1,0);
    
    if(err == MC_ERR_NOERROR)
    {
        err=SaveUTXOMap(imp->m_ImportID,imp->m_Block+1);
    }
    
    if(err == MC_ERR_NOERROR)
    {
        err=m_Database->Commit(imp);
    }
    
    if(err == MC_ERR_NOERROR)
    {
        if(imp->m_ImportID == 0)
        {
            m_UnconfirmedSends.clear();
            m_UnconfirmedSendsHashes.clear();
            LoadUnconfirmedSends(m_Database->m_DBStat.m_Block,m_Database->m_DBStat.m_Block-1);
/*            
            std::vector<uint256> vUnconfirmedHashes;
            std::map<uint256,CWalletTx> mapUnconfirmed= GetUnconfirmedSends(m_Database->m_DBStat.m_Block-1,vUnconfirmedHashes);// Confirmed txs are filtered out
//            for (map<uint256,CWalletTx>::const_iterator it = mapUnconfirmed.begin(); it != mapUnconfirmed.end(); ++it)            
            BOOST_FOREACH(const uint256& hash, vUnconfirmedHashes) 
            {
                std::map<uint256,CWalletTx>::const_iterator it = mapUnconfirmed.find(hash);
                if (it != mapUnconfirmed.end())
                {                
                    AddToUnconfirmedSends(m_Database->m_DBStat.m_Block,it->second);
                }
            }
 */ 
            if(fDebug)LogPrint("wallet","wtxs: Unconfirmed wallet transactions: %d\n",m_UnconfirmedSends.size());
        }
    }
       
    FlushUnconfirmedSends(m_Database->m_DBStat.m_Block);
    
    if(err)
    {
        LogPrintf("wtxs: Commit: Error: %d\n",err);        
        m_Database->Dump("Error in Commit");
    }
    if(fDebug)LogPrint("wallet","wtxs: Commit: Import: %d, Block: %d\n",imp->m_ImportID,imp->m_Block);
    m_Database->UnLock();
    return err;        
}

int mc_WalletTxs::LoadUnconfirmedSends(int block,int file_block)
{
    std::vector<uint256> vUnconfirmedHashes;
    std::map<uint256,CWalletTx> mapUnconfirmed= GetUnconfirmedSends(file_block,vUnconfirmedHashes);// Confirmed txs are filtered out
    BOOST_FOREACH(const uint256& hash, vUnconfirmedHashes) 
    {
        std::map<uint256,CWalletTx>::const_iterator it = mapUnconfirmed.find(hash);
        if (it != mapUnconfirmed.end())
        {                
            std::map<uint256,CWalletTx>::const_iterator it1 = m_UnconfirmedSends.find(hash);
            if (it1 == m_UnconfirmedSends.end())
            {                
                AddToUnconfirmedSends(block,it->second);
            }
        }
    }
    return MC_ERR_NOERROR;
}

/*
 CleanUpAfterBlock - called after full processing of the block (either committed or rolled back)
 
 Raw Txs: Do Nothing
 Entity lists by blockchain position: Do Nothing
 Entity lists by timestamp: Do Nothing
 UTXOs: Remove previous block file
 Unconfirmed txs: Remove previous block file
 */

int mc_WalletTxs::CleanUpAfterBlock(mc_TxImport *import,int block,int prev_block)
{
    int err;
    mc_TxImport *imp;
    
    if((m_Mode & MC_WMD_TXS) == 0)
    {
        return MC_ERR_NOERROR;
    }    
    if(m_Database == NULL)
    {
        return MC_ERR_INTERNAL_ERROR;
    }
    err = MC_ERR_NOERROR;
    imp=m_Database->m_Imports;
    if(import)
    {        
        imp=import;
    }    
     
    m_Database->Lock(1,0);
    if(prev_block-MC_TDB_UTXO_SET_WINDOW_SIZE >= 0)
    {
        RemoveUTXOMap(imp->m_ImportID,prev_block-MC_TDB_UTXO_SET_WINDOW_SIZE);
        if(imp->m_ImportID == 0)
        {
            RemoveUnconfirmedSends(prev_block-MC_TDB_UTXO_SET_WINDOW_SIZE);
        }
    }
    if(fDebug)LogPrint("wallet","wtxs: CleanUpAfterBlock: Import: %d, Block: %d\n",imp->m_ImportID,imp->m_Block);
    m_Database->UnLock();
    
    return err;
}

/*
 * Returns input entity if it is identical for all inputs
 */

void mc_WalletTxs::GetSingleInputEntity(const CWalletTx& tx,mc_TxEntity *input_entity)
{
    mc_TxEntity entity;
    mc_TxDefRow prevtxdef;
    int err;
    
    input_entity->Zero();
    BOOST_FOREACH(const CTxIn& txin, tx.vin)
    {
        CWalletTx prevwtx=GetInternalWalletTx(txin.prevout.hash,&prevtxdef,&err);  
        if(err)
        {
            return;             
        }

        if(txin.prevout.n >= prevwtx.vout.size())
        {
            return;
        }
        
        const CScript& script1 = prevwtx.vout[txin.prevout.n].scriptPubKey;        
        CScript::const_iterator pc1 = script1.begin();

        CTxDestination addressRet;

        if(!ExtractDestination(script1, addressRet))
        {
            return;             
        }
        
        entity.Zero();
        const CKeyID *lpKeyID=boost::get<CKeyID> (&addressRet);
        const CScriptID *lpScriptID=boost::get<CScriptID> (&addressRet);
        if(lpKeyID)
        {
            memcpy(entity.m_EntityID,lpKeyID,MC_TDB_ENTITY_ID_SIZE);
            entity.m_EntityType=MC_TET_PUBKEY_ADDRESS | MC_TET_CHAINPOS;
        }
        if(lpScriptID)
        {
            memcpy(entity.m_EntityID,lpScriptID,MC_TDB_ENTITY_ID_SIZE);
            entity.m_EntityType=MC_TET_SCRIPT_ADDRESS | MC_TET_CHAINPOS;
        }
        if(input_entity->m_EntityType)
        {
            if(memcmp(input_entity,&entity,sizeof(mc_TxEntity)))
            {
                return;             
            }
        }
        else
        {
            memcpy(input_entity,&entity,sizeof(mc_TxEntity));
        }
    }
}

int mc_WalletTxs::RollBackSubKeys(mc_TxImport *import,int block,mc_TxEntityStat *parent_entity,mc_Buffer *lpSubKeyEntRowBuffer)
{
    int err,i,j,r,from,count;
    mc_TxImport *imp;
    mc_TxEntityRow *entrow;
    mc_TxEntity entity;
    mc_TxEntity subkey_entity;
    bool fInBlocks;   
    unsigned char item_key[MC_ENT_MAX_ITEM_KEY_SIZE];
    int item_key_size;
    uint160 subkey_hash160;
    uint160 stream_subkey_hash160;
    set<uint160> publishers_set;
    
    if((m_Mode & MC_WMD_TXS) == 0)
    {
        return MC_ERR_NOERROR;
    }    
    
    if(m_Database == NULL)
    {
        return MC_ERR_INTERNAL_ERROR;
    }
    err = MC_ERR_NOERROR;
    imp=m_Database->m_Imports;
    if(import)
    {        
        imp=import;
    }    
    
    if(block >= imp->m_Block)
    {
        if(block == imp->m_Block)
        {
            return MC_ERR_NOERROR;        
        }
        return MC_ERR_INTERNAL_ERROR;        
    }

    fInBlocks=true;
    from=0;
    count=10;
    while(fInBlocks)                                                    // While in relevant block range        
    {
        err=m_Database->GetList(&(parent_entity->m_Entity),from,count,lpSubKeyEntRowBuffer);
        if( (err == MC_ERR_NOERROR) && (lpSubKeyEntRowBuffer->GetCount() > 0) )
        {
            for(r=lpSubKeyEntRowBuffer->GetCount()-1;r >= 0;r--)              // Processing wallet rows in reverse order
            {
                entrow=(mc_TxEntityRow *)(lpSubKeyEntRowBuffer->GetRow(r));
                if(fInBlocks &&                                                 // Still in loop, no error
                  (entrow->m_Block > block))                                    // Block to delete, mempool rows will be deleted in db rollback
                {
                    uint256 hash;
                    memcpy(&hash,entrow->m_TxId,MC_TDB_TXID_SIZE);
                    CWalletTx wtx=GetInternalWalletTx(hash,NULL,&err);  // Tx to delete
                    if(err)
                    {
                        LogPrintf("wtxs: RollBackSubKeys: Couldn't find tx %s, error: %d\n",hash.ToString().c_str(),err);        
                        fInBlocks=false;                                                                
                    }
                    else
                    {
                        for(i=0;i<(int)wtx.vout.size();i++)             
                        {
                            const CTxOut txout=wtx.vout[i];
                            const CScript& script1 = txout.scriptPubKey;        
                            CScript::const_iterator pc1 = script1.begin();

                            mc_gState->m_TmpScript->Clear();
                            mc_gState->m_TmpScript->SetScript((unsigned char*)(&pc1[0]),(size_t)(script1.end()-pc1),MC_SCR_TYPE_SCRIPTPUBKEY);
                            mc_gState->m_TmpScript->ExtractAndDeleteDataFormat(NULL);
                            if( (mc_gState->m_TmpScript->IsOpReturnScript() != 0 ) && (mc_gState->m_TmpScript->GetNumElements() >= 3) )
                            {
                                mc_gState->m_TmpScript->DeleteDuplicatesInRange(1,mc_gState->m_TmpScript->GetNumElements()-1);
                                unsigned char short_txid[MC_AST_SHORT_TXID_SIZE];
                                mc_gState->m_TmpScript->SetElement(0);

                                if( (mc_gState->m_TmpScript->GetEntity(short_txid) == 0) &&           
                                    (memcmp(short_txid,parent_entity->m_Entity.m_EntityID,MC_AST_SHORT_TXID_SIZE) == 0) )    
                                {
                                    entity.Zero();
                                    memcpy(entity.m_EntityID,short_txid,MC_AST_SHORT_TXID_SIZE);
                                    entity.m_EntityType=MC_TET_STREAM_KEY | MC_TET_CHAINPOS;
                                    
                                    for(int e=mc_gState->m_TmpScript->GetNumElements()-2;e>=1;e--)
                                    {
                                        mc_gState->m_TmpScript->SetElement(e);
                                        if(mc_gState->m_TmpScript->GetItemKey(item_key,&item_key_size))   // Item key
                                        {
                                            err=MC_ERR_INTERNAL_ERROR;
                                            goto exitlbl;                                                                                                                                        
                                        }                    

                                        subkey_hash160=Hash160(item_key,item_key+item_key_size);
                                        mc_GetCompoundHash160(&stream_subkey_hash160,parent_entity->m_Entity.m_EntityID,&subkey_hash160);

                                        subkey_entity.Zero();
                                        memcpy(subkey_entity.m_EntityID,&stream_subkey_hash160,MC_TDB_ENTITY_ID_SIZE);
                                        subkey_entity.m_EntityType=MC_TET_SUBKEY_STREAM_KEY | MC_TET_CHAINPOS;
                                        err= m_Database->DecrementSubKey(imp,&entity,&subkey_entity);
                                        if(err)
                                        {
                                            goto exitlbl;
                                        }
                                    }
                                    
                                    publishers_set.clear();
                                    for (j = 0; j < (int)wtx.vin.size(); ++j)
                                    {
                                        int op_addr_offset,op_addr_size,is_redeem_script,sighash_type;
                                        const unsigned char *ptr;

                                        const CScript& script2 = wtx.vin[j].scriptSig;        
                                        CScript::const_iterator pc2 = script2.begin();

                                        ptr=mc_ExtractAddressFromInputScript((unsigned char*)(&pc2[0]),(int)(script2.end()-pc2),
                                                &op_addr_offset,&op_addr_size,&is_redeem_script,&sighash_type,0);                                
                                        if(ptr)
                                        {
                                            if( (sighash_type == SIGHASH_ALL) || ( (sighash_type == SIGHASH_SINGLE) && (j == i) ) )
                                            {                                        
                                                subkey_hash160=Hash160(ptr+op_addr_offset,ptr+op_addr_offset+op_addr_size);
                                                if(publishers_set.count(subkey_hash160) == 0)
                                                {
                                                    publishers_set.insert(subkey_hash160);
                                                    mc_GetCompoundHash160(&stream_subkey_hash160,parent_entity->m_Entity.m_EntityID,&subkey_hash160);

                                                    entity.m_EntityType=MC_TET_STREAM_PUBLISHER | MC_TET_CHAINPOS;
                                                    subkey_entity.Zero();
                                                    memcpy(subkey_entity.m_EntityID,&stream_subkey_hash160,MC_TDB_ENTITY_ID_SIZE);
                                                    subkey_entity.m_EntityType=MC_TET_SUBKEY_STREAM_PUBLISHER | MC_TET_CHAINPOS;
                                                    err= m_Database->DecrementSubKey(imp,&entity,&subkey_entity);
                                                    if(err)
                                                    {
                                                        goto exitlbl;
                                                    }
                                                }
                                            }
                                        }                            
                                    }                            
                                }
                            }                            
                        }                        
                    }
                }
                else
                {
                    if(entrow->m_Block >= 0)
                    {
                        fInBlocks=false;
                    }
                }
            }
        }
        else
        {
            if(err)
            {
                LogPrintf("wtxs: RollBackSubKeys: Error on getting rollback tx list: %d\n",err);        
            }
            fInBlocks=false;            
        }
        from-=count;
    }

exitlbl:    
    return err;
}

/*
 RollBack - called before restoring block transactions in mempool
 
 Raw Txs: Do Nothing
 Entity lists by blockchain position: Clear mempool. remove all transactions from the rolled back blocks
 Entity lists by timestamp: Do Nothing
 UTXOs: Erase all outputs, restore all inputs for removed transactions. Use full-wallet list by blockchain position
 Unconfirmed txs: Retrieve tx list from file (block) before rolling back, add all conflicted txs to (block-1). Other txs will be added by AddTx
 */

int mc_WalletTxs::RollBack(mc_TxImport *import,int block)
{
    int err,i,r,from,count,import_pos,last_err;
    mc_TxImport *imp;
    mc_TxDefRow txdef;
    mc_Buffer *lpEntRowBuffer;
    mc_TxEntityRow *entrow;
    mc_TxEntity entity;
    mc_TxEntity wallet_entity;
    mc_TxEntityStat *stat;
    mc_Buffer *lpSubKeyEntRowBuffer;
    bool fInBlocks;   
    std::vector<mc_Coin> txouts;
    std::vector<uint256> removed_coinbases;
    
    if((m_Mode & MC_WMD_TXS) == 0)
    {
        return MC_ERR_NOERROR;
    }    
    
    if(m_Database == NULL)
    {
        return MC_ERR_INTERNAL_ERROR;
    }
    err = MC_ERR_NOERROR;
    imp=m_Database->m_Imports;
    if(import)
    {        
        imp=import;
    }    
    import_pos=imp-m_Database->m_Imports;
    
    if(block >= imp->m_Block)
    {
        if(block == imp->m_Block)
        {
            return MC_ERR_NOERROR;        
        }
        return MC_ERR_INTERNAL_ERROR;        
    }
    
    
    m_Database->Lock(1,0);
    
    lpSubKeyEntRowBuffer=NULL;
    for(i=0;i<imp->m_Entities->GetCount();i++)                                  // Removing ordered-by-blockchain-position rows
    {
        stat=(mc_TxEntityStat*)imp->m_Entities->GetRow(i);
        if( stat->m_Entity.m_EntityType == (MC_TET_STREAM | MC_TET_CHAINPOS) )
        {        
            if(err == MC_ERR_NOERROR)
            {
                if(lpSubKeyEntRowBuffer == NULL)
                {
                    lpSubKeyEntRowBuffer=new mc_Buffer;
                    lpSubKeyEntRowBuffer->Initialize(MC_TDB_ENTITY_KEY_SIZE,sizeof(mc_TxEntityRow),MC_BUF_MODE_DEFAULT);                    
                }
                err=RollBackSubKeys(imp,block,stat,lpSubKeyEntRowBuffer);
            }            
        }
    }
    if(err)
    {
        LogPrintf("wtxs: RollBack: Error when rolling back soubkeys: %d\n",err);        
    }
    if(lpSubKeyEntRowBuffer)
    {
        delete lpSubKeyEntRowBuffer;
    }
    
    if(err == MC_ERR_NOERROR)
    {
        if(imp->m_ImportID == 0)                                                // Copying conflicted transactions, they may become valid and should be rechecked
        {
            std::vector<uint256> vUnconfirmedHashes;
            std::map<uint256,CWalletTx> mapUnconfirmed= GetUnconfirmedSends(m_Database->m_DBStat.m_Block,vUnconfirmedHashes);
            for (map<uint256,CWalletTx>::const_iterator it = mapUnconfirmed.begin(); it != mapUnconfirmed.end(); ++it)
            {
                m_Database->GetTx(&txdef,(unsigned char*)&(it->first));
                if(txdef.m_Flags & MC_TFL_INVALID)
                {
                    AddToUnconfirmedSends(block,it->second);
                }
            }
        }
    }
    if(err == MC_ERR_NOERROR)
    {
        wallet_entity.Zero();
        wallet_entity.m_EntityType=MC_TET_WALLET_ALL | MC_TET_CHAINPOS;
        if(imp->FindEntity(&wallet_entity) >= 0)                                // Only "address" imports have this entity and should update UTXOs
        {
            lpEntRowBuffer=new mc_Buffer;
            lpEntRowBuffer->Initialize(MC_TDB_ENTITY_KEY_SIZE,sizeof(mc_TxEntityRow),MC_BUF_MODE_DEFAULT);
            fInBlocks=true;
            from=0;
            count=10;
            while(fInBlocks)                                                    // While in relevant block range        
            {
                err=m_Database->GetList(&wallet_entity,from,count,lpEntRowBuffer);
                if( (err == MC_ERR_NOERROR) && (lpEntRowBuffer->GetCount() > 0) )
                {
                    for(r=lpEntRowBuffer->GetCount()-1;r >= 0;r--)              // Processing wallet rows in reverse order
                    {
                        entrow=(mc_TxEntityRow *)(lpEntRowBuffer->GetRow(r));
                        if(fInBlocks &&                                         // Still in loop, no error
                          ((entrow->m_Block > block) ||                         // Block to delete
                           (entrow->m_Block < 0)))                              // In mempool
                        {
                            uint256 hash;
                            memcpy(&hash,entrow->m_TxId,MC_TDB_TXID_SIZE);
                            CWalletTx wtx=GetInternalWalletTx(hash,NULL,&err);  // Tx to delete
                            if(err)
                            {
                                LogPrintf("wtxs: RollBack: Couldn't find tx %s, error: %d\n",hash.ToString().c_str(),err);        
                                fInBlocks=false;                                                                
                            }
                            else
                            {
                                BOOST_FOREACH(const CTxIn& txin, wtx.vin)       // Inputs-to-restore list
                                {
                                    COutPoint outp(txin.prevout.hash,txin.prevout.n);
                                    mc_TxDefRow prevtxdef;
                                    const CWalletTx& prevwtx=GetInternalWalletTx(txin.prevout.hash,&prevtxdef,&last_err);  
                                                                                // Previous transaction
                                    
                                    if(last_err == MC_ERR_NOERROR)              // Tx found, probably it is relevant input
                                    {
                                        if(txin.prevout.n < prevwtx.vout.size())
                                        {
                                            mc_Coin txout;
                                            txout.m_OutPoint=outp;
                                            txout.m_TXOut=prevwtx.vout[txin.prevout.n];
                                            txout.m_Flags=prevtxdef.m_Flags;
                                            txout.m_Block=prevtxdef.m_Block;
                                            txout.m_EntityID=0;
                                            txout.m_EntityType=MC_TET_NONE;
                                            txout.m_LockTime=prevwtx.nLockTime;

                                            const CScript& script1 = txout.m_TXOut.scriptPubKey;        
                                            txnouttype typeRet;
                                            int nRequiredRet;
                                            std::vector<CTxDestination> addressRets;

                                            ExtractDestinations(script1,typeRet,addressRets,nRequiredRet);
            
                                            BOOST_FOREACH(const CTxDestination& dest, addressRets)
                                            {
                                                entity.Zero();
                                                const CKeyID *lpKeyID=boost::get<CKeyID> (&dest);
                                                const CScriptID *lpScriptID=boost::get<CScriptID> (&dest);
                                                if(lpKeyID)
                                                {
                                                    memcpy(entity.m_EntityID,lpKeyID,MC_TDB_ENTITY_ID_SIZE);
                                                    entity.m_EntityType=MC_TET_PUBKEY_ADDRESS | MC_TET_CHAINPOS;
                                                }
                                                if(lpScriptID)
                                                {
                                                    memcpy(entity.m_EntityID,lpScriptID,MC_TDB_ENTITY_ID_SIZE);
                                                    entity.m_EntityType=MC_TET_SCRIPT_ADDRESS | MC_TET_CHAINPOS;
                                                }
                                                if(entity.m_EntityType)
                                                {
                                                    if(imp->FindEntity(&entity) >= 0)    // It is relevant input
                                                    {
                                                        if(addressRets.size() == 1)
                                                        {
                                                            memcpy(&(txout.m_EntityID),entity.m_EntityID,MC_TDB_ENTITY_ID_SIZE);
                                                            txout.m_EntityType=entity.m_EntityType;
                                                            isminefilter mine;
                                                            if(entity.m_EntityType & MC_TET_PUBKEY_ADDRESS)
                                                            {
                                                                mine = m_lpWallet ? IsMineKeyID(*m_lpWallet, *lpKeyID) : ISMINE_NO;
                                                            }
                                                            else
                                                            {
                                                                mine = m_lpWallet ? IsMineScriptID(*m_lpWallet, *lpScriptID) : ISMINE_NO;                                                                
                                                            }
//                                                            isminefilter mine = m_lpWallet ? IsMine(*m_lpWallet, dest) : ISMINE_NO;
                                                            if(mine & ISMINE_SPENDABLE) //Spendable flag is used to avoid IsMine calculation in coin selection
                                                            {
                                                                txout.m_Flags |= MC_TFL_IS_SPENDABLE;
                                                            }
                                                        }
                                                        mc_TxEntity input_entity;
                                                        GetSingleInputEntity(prevwtx,&input_entity); // Check if the entity coinsides with single input entity of prev tx - change
                                                        if(memcmp(&input_entity,&entity,sizeof(mc_TxEntity)) == 0)
                                                        {
                                                            txout.m_Flags |= MC_TFL_IS_CHANGE;
                                                        }
                                                        txouts.push_back(txout);
                                                    }
                                                }
                                            }                                            
                                        }
                                        else                                    // Index on prev tx out of range
                                        {
                                            fInBlocks=false;                                                                
                                            if(err == MC_ERR_NOERROR)           // Something is wrong, set error if not set
                                            {
                                                err=MC_ERR_CORRUPTED;
                                            }
                                        }
                                    }
                                    else                                        // prev tx not found  - it cannot be our input
                                    {
                                        if(last_err != MC_ERR_NOT_FOUND)        // If not our input - ignore it
                                        {
                                            fInBlocks=false;                                                                
                                            if(err == MC_ERR_NOERROR)
                                            {
                                                err=last_err;
                                            }
                                        }                                        
                                    }
                                    
                                }

                                for(i=0;i<(int)wtx.vout.size();i++)             // outputs-to-remove list
                                {
                                    mc_Coin txout;
                                    txout.m_OutPoint=COutPoint(wtx.GetHash(),i);
                                    txout.m_TXOut=wtx.vout[i];
                                    txout.m_Flags=MC_TFL_IMPOSSIBLE;            // We will delete this txout, setting this flag to distinguish from restored inputs
                                    txout.m_Block=entrow->m_Block;
                                    txout.m_EntityID=0;
                                    txout.m_EntityType=MC_TET_NONE;
                                    txout.m_LockTime=wtx.nLockTime;

                                    const CScript& script1 = txout.m_TXOut.scriptPubKey;        
                                    txnouttype typeRet;
                                    int nRequiredRet;
                                    std::vector<CTxDestination> addressRets;

                                    ExtractDestinations(script1,typeRet,addressRets,nRequiredRet);

                                    BOOST_FOREACH(const CTxDestination& dest, addressRets)
                                    {
                                        entity.Zero();
                                        const CKeyID *lpKeyID=boost::get<CKeyID> (&dest);
                                        const CScriptID *lpScriptID=boost::get<CScriptID> (&dest);
                                        if(lpKeyID)
                                        {
                                            memcpy(entity.m_EntityID,lpKeyID,MC_TDB_ENTITY_ID_SIZE);
                                            entity.m_EntityType=MC_TET_PUBKEY_ADDRESS | MC_TET_CHAINPOS;
                                        }
                                        if(lpScriptID)
                                        {
                                            memcpy(entity.m_EntityID,lpScriptID,MC_TDB_ENTITY_ID_SIZE);
                                            entity.m_EntityType=MC_TET_SCRIPT_ADDRESS | MC_TET_CHAINPOS;
                                        }
                                        if(entity.m_EntityType)
                                        {
                                            if(imp->FindEntity(&entity) >= 0)   // It is relevant output 
                                            {
                                                if(addressRets.size() == 1)
                                                {
                                                    memcpy(&(txout.m_EntityID),entity.m_EntityID,MC_TDB_ENTITY_ID_SIZE);
                                                    txout.m_EntityType=entity.m_EntityType;
                                                }
                                                txouts.push_back(txout);        // Added to the same array to preserve removing/restoring order
                                                                                // otherwise earlier transactions may restore just-deleted outputs
                                            }
                                        }
                                    }
                                }                                
                                if(fDebug)LogPrint("wallet","wtxs: Removing tx %s, block %d, flags: %08X, import %d\n",hash.ToString().c_str(),entrow->m_Block,entrow->m_Flags,imp->m_ImportID);
                                if(wtx.IsCoinBase())
                                {
                                    removed_coinbases.push_back(hash);
                                }
                            }
                        }
                        else
                        {
                            if(entrow->m_Block >= 0)
                            {
                                fInBlocks=false;                                
                            }
                        }
                    }
                }
                else
                {
                    if(err)
                    {
                        LogPrintf("wtxs: RollBack: Error on getting rollback tx list: %d\n",err);        
                    }
                    fInBlocks=false;
                }
                from-=count;
            }
            
            delete lpEntRowBuffer;
            
            if(err == MC_ERR_NOERROR)
            {
                for(i=0;i<(int)txouts.size();i++)
                {
                    if(txouts[i].m_Flags == MC_TFL_IMPOSSIBLE)                  // Outputs to delete
                    {
                        m_UTXOs[import_pos].erase(txouts[i].m_OutPoint);                        
                    }
                    else                                                        // Inputs to restore
                    {
                        std::map<COutPoint, mc_Coin>::const_iterator itold = m_UTXOs[import_pos].find(txouts[i].m_OutPoint);
                        if (itold == m_UTXOs[import_pos].end())
                        {
                            m_UTXOs[import_pos].insert(make_pair(txouts[i].m_OutPoint, txouts[i]));
                        }                    
                    }
                }
            }                
        }        
    }    
    if(err == MC_ERR_NOERROR)
    {
        err=SaveUTXOMap(imp->m_ImportID,block);
    }
    if(err == MC_ERR_NOERROR)
    {
        err=m_Database->RollBack(import,block);                                 // Database rollback    
        for(i=0;i<(int)removed_coinbases.size();i++)
        {
            uint256 hash=removed_coinbases[i];
            m_Database->SaveTxFlag((unsigned char*)&hash,MC_TFL_INVALID,1);
        }
    }
    if(err)
    {
        LogPrintf("wtxs: RollBack: Error: %d\n",err);        
        m_Database->Dump("Error in RollBack");
    }
    if(fDebug)LogPrint("wallet","wtxs: RollBack: Import: %d, Block: %d\n",imp->m_ImportID,imp->m_Block);
    m_Database->UnLock();
    return err;            
}

int mc_WalletTxs::GetRow(
               mc_TxEntityRow *erow)
{
    int err;
    if((m_Mode & MC_WMD_TXS) == 0)
    {
        return MC_ERR_NOT_SUPPORTED;
    }    
    if(m_Database == NULL)
    {
        return MC_ERR_INTERNAL_ERROR;
    }
    m_Database->Lock(0,0);
    err=m_Database->GetRow(erow);
    m_Database->UnLock();
    return err;                
}

int mc_WalletTxs::GetList(mc_TxEntity *entity,int from,int count,mc_Buffer *txs)
{
    int err;
    if((m_Mode & MC_WMD_TXS) == 0)
    {
        return MC_ERR_NOT_SUPPORTED;
    }    
    if(m_Database == NULL)
    {
        return MC_ERR_INTERNAL_ERROR;
    }
    m_Database->Lock(0,0);
    err=m_Database->GetList(entity,from,count,txs);
    m_Database->UnLock();
    return err;            
}

int mc_WalletTxs::GetList(mc_TxEntity *entity,int generation,int from,int count,mc_Buffer *txs)
{
    int err;
    if((m_Mode & MC_WMD_TXS) == 0)
    {
        return MC_ERR_NOT_SUPPORTED;
    }    
    if(m_Database == NULL)
    {
        return MC_ERR_INTERNAL_ERROR;
    }
    m_Database->Lock(0,0);
    err=m_Database->GetList(entity,generation,from,count,txs);
    m_Database->UnLock();
    return err;            
}

int mc_WalletTxs::GetBlockItemIndex(mc_TxEntity *entity, int block)
{
    int res;
    m_Database->Lock(0,0);
    res=m_Database->GetBlockItemIndex(NULL,entity,block);
    m_Database->UnLock();
    return res;            
}


int mc_WalletTxs::GetListSize(mc_TxEntity *entity,int *confirmed)
{
    int res;
    if((m_Mode & MC_WMD_TXS) == 0)
    {
        return 0;
    }    
    if(m_Database == NULL)
    {
        return 0;
    }
    m_Database->Lock(0,0);
    res=m_Database->GetListSize(entity,confirmed);
    m_Database->UnLock();
    return res;                
}

int mc_WalletTxs::GetListSize(mc_TxEntity *entity,int generation,int *confirmed)
{
    int res;
    if((m_Mode & MC_WMD_TXS) == 0)
    {
        return 0;
    }    
    if(m_Database == NULL)
    {
        return 0;
    }
    m_Database->Lock(0,0);
    res=m_Database->GetListSize(entity,generation,confirmed);
    m_Database->UnLock();
    return res;                
}

int mc_WalletTxs::Unsubscribe(mc_Buffer* lpEntities)
{
    int err;
    if((m_Mode & MC_WMD_TXS) == 0)
    {
        return MC_ERR_NOT_SUPPORTED;
    }    
    if(m_Database == NULL)
    {
        return MC_ERR_INTERNAL_ERROR;
    }
    m_Database->Lock(1,0);    
    err=m_Database->Unsubscribe(lpEntities);
    if(fDebug)LogPrint("wallet","wtxs: Unsubscribed from %d entities\n",lpEntities->GetCount());
    m_Database->UnLock();
    return err;                        
}

mc_TxImport *mc_WalletTxs::StartImport(mc_Buffer *lpEntities,int block,int *err)
{
    mc_TxImport *imp;
    if((m_Mode & MC_WMD_TXS) == 0)
    {
        *err=MC_ERR_NOT_SUPPORTED;
        return NULL;
    }    
    if(m_Database == NULL)
    {
        *err=MC_ERR_INTERNAL_ERROR;
        return NULL;
    }
    m_Database->Lock(1,0);
    imp=m_Database->StartImport(lpEntities,block,err);
    if(*err == MC_ERR_NOERROR)                                                  // BAD If block!=-1 old UXOs should be copied?
    {
        m_UTXOs[imp-m_Database->m_Imports].clear();
    }
    if(fDebug)LogPrint("wallet","wtxs: StartImport: Import: %d, Block: %d\n",imp->m_ImportID,imp->m_Block);
    m_Database->UnLock();
    return imp;                
}

mc_TxImport *mc_WalletTxs::FindImport(int import_id)
{
    mc_TxImport *imp;
    if((m_Mode & MC_WMD_TXS) == 0)
    {
        return NULL;
    }    
    if(m_Database == NULL)
    {
        return NULL;
    }
    m_Database->Lock(0,0);
    imp=m_Database->FindImport(import_id);
    m_Database->UnLock();
    return imp;                
}

int mc_WalletTxs::ImportGetBlock(mc_TxImport *import)
{
    int block;
    if((m_Mode & MC_WMD_TXS) == 0)
    {
        return -2;
    }    
    if(m_Database == NULL)
    {
        return MC_ERR_INTERNAL_ERROR;
    }
    m_Database->Lock(0,0);
    block=m_Database->ImportGetBlock(import);
    m_Database->UnLock();
    return block;                
    
}

int mc_WalletTxs::CompleteImport(mc_TxImport *import)
{
    int err,import_pos,gen,count;
    
    std::map<COutPoint, mc_Coin> mapCurrent;
    
    if((m_Mode & MC_WMD_TXS) == 0)
    {
        return MC_ERR_NOT_SUPPORTED;
    }    
    if(m_Database == NULL)
    {
        return MC_ERR_INTERNAL_ERROR;
    }
    m_Database->Lock(1,0);
    
    gen=import->m_ImportID;
    
    err=m_Database->CompleteImport(import);
    
                                                                                
    if(m_Database->m_DBStat.m_Block != m_Database->m_Imports->m_Block)
    {
        LogPrintf("wtxs: Internal error, block count mismatch: %d-%d\n",m_Database->m_DBStat.m_Block, m_Database->m_Imports->m_Block);        
        err=MC_ERR_INTERNAL_ERROR;
    }

    import_pos=import-m_Database->m_Imports;                                
    if(m_UTXOs[import_pos].size())
    {
        count=m_Database->m_MemPools[0]->GetCount();

        if(err == MC_ERR_NOERROR)                                                   
        {
            if(count > 0)                                                       // If mempool is not empty the following Save should not add mempool UTXOs
            {
                mapCurrent=m_UTXOs[0];
                err=LoadUTXOMap(0,m_Database->m_DBStat.m_Block);                // Loading confirmed UTXO map
            }
        }

        if(err == MC_ERR_NOERROR)                                               // Adding confirmed unspent coins
        {
            for (map<COutPoint, mc_Coin>::const_iterator it = m_UTXOs[import_pos].begin(); it != m_UTXOs[import_pos].end(); ++it)
            {
                std::map<COutPoint, mc_Coin>::iterator itold = m_UTXOs[0].find(it->first);
                if (itold == m_UTXOs[0].end())
                {
                    if(it->second.m_Block >= 0)
                    {
                        m_UTXOs[0].insert(make_pair(it->first, it->second));        
                    }
                }                    
                else
                {
                    itold->second.m_Flags=it->second.m_Flags;
                }
            }                
        }        

        if(err == MC_ERR_NOERROR)                                               // Saving confirmed UTXO map
        {
            err=SaveUTXOMap(0,m_Database->m_DBStat.m_Block);
        }

        if(err == MC_ERR_NOERROR)                                               // Restoring current UTXO map
        {
            if(count)
            {
                m_UTXOs[0]=mapCurrent;            
            }
        }    

        if(err == MC_ERR_NOERROR)
        {
            import_pos=import-m_Database->m_Imports;                            // Adding unspent coins to the "chain import"
            for (map<COutPoint, mc_Coin>::const_iterator it = m_UTXOs[import_pos].begin(); it != m_UTXOs[import_pos].end(); ++it)
            {
                std::map<COutPoint, mc_Coin>::iterator itold = m_UTXOs[0].find(it->first);
                if (itold == m_UTXOs[0].end())
                {
                    m_UTXOs[0].insert(make_pair(it->first, it->second));        
                }                    
                else
                {
                    itold->second.m_Flags=it->second.m_Flags;
                    // in case of merge flags should be merged. If flag is MC_TFL_FROM_ME and not MC_TFL_ALL_INPUTS_FROM_ME in both cases
                    // it may become MC_TFL_ALL_INPUTS_FROM_ME after merge.
                    // This flag is important as txout may become unspendable after rollback
                    // if flag is updated - should update transaction
                    // Current import implementation sets final flag as it scans all transactions from the beginning
                }
            }        
        }
    }
    
    if(err == MC_ERR_NOERROR)
    {
        RemoveUTXOMap(gen,m_Database->m_DBStat.m_Block);        
    }
    if(err)
    {
        LogPrintf("wtxs: CompleteImport: Error: %d\n",err);        
    }
    if(fDebug)LogPrint("wallet","wtxs: CompleteImport: Import: %d, Block: %d\n",gen,m_Database->m_DBStat.m_Block);
    m_Database->UnLock();
    return err;                
}

int mc_WalletTxs::DropImport(mc_TxImport *import)
{
    int err,gen;
    if((m_Mode & MC_WMD_TXS) == 0)
    {
        return MC_ERR_NOT_SUPPORTED;
    }    
    if(m_Database == NULL)
    {
        return MC_ERR_INTERNAL_ERROR;
    }
    m_Database->Lock(1,0);
    
    gen=import->m_ImportID;
    err=m_Database->DropImport(import);
    if(err == MC_ERR_NOERROR)
    {
        RemoveUTXOMap(import->m_ImportID,import->m_Block);        
    }
    if(fDebug)LogPrint("wallet","wtxs: DropImport: Import: %d, Block: %d\n",gen,m_Database->m_DBStat.m_Block);
    m_Database->UnLock();
    return err;                    
}

int mc_WalletTxs::FlushUnconfirmedSends(int block)
{
    FILE* fHan;
    char ShortName[65];                                     
    char FileName[MC_DCT_DB_MAX_PATH];                      
    uint256 hash;
    
    if((m_Mode & MC_WMD_TXS) == 0)
    {
        return MC_ERR_NOT_SUPPORTED;
    }    
    if(m_Database == NULL)
    {
        return MC_ERR_INTERNAL_ERROR;
    }

    sprintf(ShortName,"wallet/uncsend_%d",block);

    mc_GetFullFileName(m_Database->m_Name,ShortName,".dat",MC_FOM_RELATIVE_TO_DATADIR | MC_FOM_CREATE_DIR,FileName);
    fHan=fopen(FileName,"ab+");
    
    if(fHan == NULL)
    {
        return MC_ERR_FILE_WRITE_ERROR;
    }
    
    FileCommit(fHan);                                                           
    fclose(fHan);    
    
    return MC_ERR_NOERROR;
}

int mc_WalletTxs::AddToUnconfirmedSends(int block, const CWalletTx& tx)
{    
    FILE* fHan;
    char ShortName[65];                                     
    char FileName[MC_DCT_DB_MAX_PATH];                      
    uint256 hash;
    
    if((m_Mode & MC_WMD_TXS) == 0)
    {
        return MC_ERR_NOT_SUPPORTED;
    }    
    if(m_Database == NULL)
    {
        return MC_ERR_INTERNAL_ERROR;
    }

    sprintf(ShortName,"wallet/uncsend_%d",block);

    mc_GetFullFileName(m_Database->m_Name,ShortName,".dat",MC_FOM_RELATIVE_TO_DATADIR | MC_FOM_CREATE_DIR,FileName);
        
    fHan=fopen(FileName,"ab+");
    
    if(fHan == NULL)
    {
        return MC_ERR_FILE_WRITE_ERROR;
    }
    
    CAutoFile fileout(fHan, SER_DISK, CLIENT_VERSION);
    
    fileout << tx;

    hash=tx.GetHash();
    m_UnconfirmedSends.insert(make_pair(hash, tx));
    m_UnconfirmedSendsHashes.push_back(hash);
    
//    FileCommit(fHan);                                                           // If we not do it, in the worst case we'll lose some transactions
    
    return MC_ERR_NOERROR;
}

int mc_WalletTxs::SaveTxFlag(const unsigned char *hash,uint32_t flag,int set_flag)
{
    int err,lockres;
    if((m_Mode & MC_WMD_TXS) == 0)
    {
        return MC_ERR_NOT_SUPPORTED;
    }    
    if(m_Database == NULL)
    {
        return MC_ERR_INTERNAL_ERROR;
    }
    
    lockres=m_Database->Lock(1,1);
    
    err=m_Database->SaveTxFlag(hash,flag,set_flag);    
    
    if(lockres == 0)
    {
        m_Database->UnLock();
    }
    
    return err;
}

std::map<uint256,CWalletTx> mc_WalletTxs::GetUnconfirmedSends(int block,std::vector<uint256>& unconfirmedSendsHashes)
{
    map<uint256,CWalletTx> outMap;
    char ShortName[65];                                     
    char FileName[MC_DCT_DB_MAX_PATH];                      
    bool fHaveTx;
    uint256 hash;
    int err;
    mc_TxDefRow txdef;
        
    if((m_Mode & MC_WMD_TXS) == 0)
    {
        return outMap;
    }    
    if(m_Database == NULL)
    {
        return outMap;
    }
    sprintf(ShortName,"wallet/uncsend_%d",block);

    mc_GetFullFileName(m_Database->m_Name,ShortName,".dat",MC_FOM_RELATIVE_TO_DATADIR | MC_FOM_CREATE_DIR,FileName);
        
    CAutoFile filein(fopen(FileName,"rb+"), SER_DISK, CLIENT_VERSION);
    
    fHaveTx=true;
    while(fHaveTx)
    {
        CWalletTx wtx;
        try 
        {
            filein >> wtx;
            hash=wtx.GetHash();
            err=m_Database->GetTx(&txdef,(unsigned char*)&hash);
            if((err != MC_ERR_NOERROR) || (txdef.m_Block < 0) || ((txdef.m_Flags & MC_TFL_INVALID) != 0))
                                                                                // Invalid txs are returned for rechecking
            {
                std::map<uint256,CWalletTx>::const_iterator it = outMap.find(hash);
                if (it == outMap.end())
                {
                    outMap.insert(make_pair(hash, wtx));                
                    unconfirmedSendsHashes.push_back(hash);
                }            
            }
        } 
        catch (std::exception &e) 
        {           
            fHaveTx=false;
        }
    }
    
    return outMap;
}

int mc_WalletTxs::RemoveUnconfirmedSends(int block)
{
    char ShortName[65];                                     
    char FileName[MC_DCT_DB_MAX_PATH];                      
    
    if((m_Mode & MC_WMD_TXS) == 0)
    {
        return MC_ERR_NOT_SUPPORTED;
    }    
    if(m_Database == NULL)
    {
        return MC_ERR_INTERNAL_ERROR;
    }
    
    sprintf(ShortName,"wallet/uncsend_%d",block);

    mc_GetFullFileName(m_Database->m_Name,ShortName,".dat",MC_FOM_RELATIVE_TO_DATADIR | MC_FOM_CREATE_DIR, FileName);
    
    remove(FileName);
    
    return MC_ERR_NOERROR;        
}

int mc_WalletTxs::RemoveUTXOMap(int import_id,int block)
{
    char ShortName[65];                                     
    char FileName[MC_DCT_DB_MAX_PATH];                      
    
    if((m_Mode & MC_WMD_TXS) == 0)
    {
        return MC_ERR_NOT_SUPPORTED;
    }    
    if(m_Database == NULL)
    {
        return MC_ERR_INTERNAL_ERROR;
    }
    
    if(block < 0)
    {
        return MC_ERR_NOERROR;
    }
    
    sprintf(ShortName,"wallet/utxo%d_%d",import_id,block);

    mc_GetFullFileName(m_Database->m_Name,ShortName,".dat",MC_FOM_RELATIVE_TO_DATADIR | MC_FOM_CREATE_DIR, FileName);
    
    remove(FileName);
    
    return MC_ERR_NOERROR;
}

int mc_WalletTxs::SaveUTXOMap(int import_id,int block)
{
    char ShortName[65];                                     
    char FileName[MC_DCT_DB_MAX_PATH];                      
    FILE *fHan;
    int import_pos;
    
    if((m_Mode & MC_WMD_TXS) == 0)
    {
        return MC_ERR_NOT_SUPPORTED;
    }    
    if(m_Database == NULL)
    {
        return MC_ERR_INTERNAL_ERROR;
    }
    
    if(block < 0)
    {
        return MC_ERR_NOERROR;
    }
//        printf("Save UTXO file %d\n",block);
    
    import_pos=m_Database->FindImport(import_id)-m_Database->m_Imports;
    sprintf(ShortName,"wallet/utxo%d_%d",import_id,block);

    mc_GetFullFileName(m_Database->m_Name,ShortName,".dat",MC_FOM_RELATIVE_TO_DATADIR | MC_FOM_CREATE_DIR, FileName);
    
    remove(FileName);

    fHan=fopen(FileName,"wb+");
    
    if(fHan == NULL)
    {
        return MC_ERR_FILE_WRITE_ERROR;
    }
    
    CAutoFile fileout(fHan, SER_DISK, CLIENT_VERSION);
    
    for (map<COutPoint, mc_Coin>::const_iterator it = m_UTXOs[import_pos].begin(); it != m_UTXOs[import_pos].end(); ++it)
    {
        fileout << it->second;        
    }

    FileCommit(fHan);
    
    return MC_ERR_NOERROR;
}

int mc_WalletTxs::LoadUTXOMap(int import_id,int block)
{
    char ShortName[65];                                     
    char FileName[MC_DCT_DB_MAX_PATH];                      
    FILE *fHan;
    int import_pos;
    bool fHaveUtxo;
    map<COutPoint, mc_Coin> mapOut;
    
    if((m_Mode & MC_WMD_TXS) == 0)
    {
        return MC_ERR_NOT_SUPPORTED;
    }    
    if(m_Database == NULL)
    {
        return MC_ERR_INTERNAL_ERROR;
    }
    
    import_pos=m_Database->FindImport(import_id)-m_Database->m_Imports;

    if(block < 0)
    {
        m_UTXOs[import_pos].clear();
        return MC_ERR_NOERROR;
    }
    
    sprintf(ShortName,"wallet/utxo%d_%d",import_id,block);

    mc_GetFullFileName(m_Database->m_Name,ShortName,".dat",MC_FOM_RELATIVE_TO_DATADIR | MC_FOM_CREATE_DIR, FileName);
    
    fHan=fopen(FileName,"rb+");
    
    if(fHan == NULL)
    {
        LogPrintf("wtxs: Cannot open unspent outputs file\n");
        if(block > 0)
        {
            return MC_ERR_NOERROR;
        }
    }
    
    CAutoFile filein(fHan, SER_DISK, CLIENT_VERSION);
    
    fHaveUtxo=true;
    while(fHaveUtxo)
    {
        mc_Coin utxo;
        try 
        {
            filein >> utxo;
            mapOut.insert(make_pair(utxo.m_OutPoint, utxo));                
        } 
        catch (std::exception &e) 
        {           
            fHaveUtxo=false;
        }
    }
    
    m_UTXOs[import_pos]=mapOut;
    
    if(fDebug)LogPrint("wallet","wtxs: Loaded %u unspent outputs for import %d\n",m_UTXOs[import_pos].size(),import_pos);
            
    return MC_ERR_NOERROR;
}

/*
 * Preparing tx with shortened OP_RETURN metadata
 */

CWalletTx mc_WalletTxChoppedCopy(CWallet *lpWallet,const CWalletTx& tx)
{
    int i;
    CWalletTx wtx(lpWallet);

    wtx.mapValue = tx.mapValue;
    wtx.vOrderForm = tx.vOrderForm;
    wtx.nTimeReceived = tx.nTimeReceived;
    wtx.nTimeSmart = tx.nTimeSmart;
    wtx.fFromMe = tx.fFromMe;
    wtx.strFromAccount = tx.strFromAccount;
    wtx.nOrderPos = tx.nOrderPos;
    
    CMutableTransaction txNew(tx);
    txNew.vin.clear();
    txNew.vout.clear();
    
    
    BOOST_FOREACH(const CTxIn& txin, tx.vin)
    {
        txNew.vin.push_back(txin);
    }    
    for(i=0;i<(int)tx.vout.size();i++)
    {
        const CTxOut txout=tx.vout[i];
        const CScript& script1 = txout.scriptPubKey;        
        CScript::const_iterator pc1 = script1.begin();

        std::vector<CTxDestination> addressRets;
        CScript scriptOpReturn=CScript();
        bool fChopped=false;
        size_t full_size=0;
        
        mc_gState->m_TmpScript->Clear();
        mc_gState->m_TmpScript->SetScript((unsigned char*)(&pc1[0]),(size_t)(script1.end()-pc1),MC_SCR_TYPE_SCRIPTPUBKEY);
        if(mc_gState->m_TmpScript->IsOpReturnScript())
        {                
            size_t elem_size;    
            const unsigned char *elem;
            int num_elems=mc_gState->m_TmpScript->GetNumElements();
            
            for(int elem_id=0;elem_id<num_elems-1;elem_id++)
            {
                elem = mc_gState->m_TmpScript->GetData(elem_id,&elem_size);
                scriptOpReturn << vector<unsigned char>(elem, elem + elem_size) << OP_DROP;                
            }
            
            elem = mc_gState->m_TmpScript->GetData(num_elems-1,&elem_size);

            if(elem_size > MC_TDB_MAX_OP_RETURN_SIZE)
            {
                full_size=elem_size;
                elem_size = MC_TDB_MAX_OP_RETURN_SIZE;
                scriptOpReturn << OP_RETURN << vector<unsigned char>(elem, elem + elem_size);
                fChopped=true;
            }            
        }
        if(fChopped)
        {
            CTxOut txoutChopped(txout.nValue, scriptOpReturn);
            txNew.vout.push_back(txoutChopped);                        
            string str=strprintf("fullsize%05d",i);
            wtx.mapValue[str] = full_size;
        }
        else
        {
            txNew.vout.push_back(txout);            
        }
    }
    
    *static_cast<CTransaction*>(&wtx) = CTransaction(txNew);
    
    return wtx;
}

void mc_WalletCachedSubKey::Set(mc_TxEntity* entity,mc_TxEntity* subkey_entity,uint160 subkey_hash,uint32_t flags)
{
    memcpy(&m_Entity,entity,sizeof(mc_TxEntity));
    memcpy(&m_SubkeyEntity,subkey_entity,sizeof(mc_TxEntity));
    m_SubKeyHash=subkey_hash;
    m_Flags=flags;    
}

void mc_WalletCachedSubKey::Zero()
{
    m_Entity.Zero();
    m_SubkeyEntity.Zero();
    m_SubKeyHash=0;
    m_Flags=0;
}

int mc_WalletTxs::AddTx(mc_TxImport *import,const CTransaction& tx,int block,CDiskTxPos* block_pos,uint32_t block_tx_index,uint256 block_hash)
{
    if((m_Mode & MC_WMD_TXS) == 0)
    {
        return MC_ERR_NOERROR;
    }    
    CWalletTx wtx(m_lpWallet,tx);
    
    return AddTx(import,wtx,block,block_pos,block_tx_index,block_hash);
}

int mc_WalletTxs::AddTx(mc_TxImport *import,const CWalletTx& tx,int block,CDiskTxPos* block_pos,uint32_t block_tx_index,uint256 block_hash)
{
    int err,i,j,entcount,lockres,entpos,base_row;
    mc_TxImport *imp;
    mc_TxEntity entity;
    mc_TxEntity subkey_entity;
    mc_TxEntityStat* lpentstat;
    mc_TxEntity input_entity;
    mc_TxEntity *lpent;
    mc_TxDefRow txdef;
    mc_TxEntityRowExtension extension;
    mc_TxEntityRowExtension *lpext;
    
    const CWalletTx *fullTx;
    const CWalletTx *storedTx;
    CWalletTx stx;
    unsigned char item_key[MC_ENT_MAX_ITEM_KEY_SIZE];
    int item_key_size;
    uint256 subkey_hash256;
    uint160 subkey_hash160;
    uint160 stream_subkey_hash160;
    set<uint160> publishers_set;
    map<uint160,int> subkey_count_map;
    
    int import_pos;
    bool fFound;
    bool fIsFromMe;
    bool fIsToMe;
    bool fIsSpendable;
    bool fAllInputsFromMe;
    bool fAllInputsAreFinal;
    bool fSingleInputEntity;
    bool fRelevantEntity;
    bool fOutputIsSpendable;
    bool fNewStream;
    bool fNewAsset;
    std::vector<mc_Coin> txoutsIn;
    std::vector<mc_Coin> txoutsOut;
    uint256 hash;
    unsigned char *ptrOut;
    bool fAlreadyInTheWalletForNotify;   
    
    uint32_t txsize;
    uint32_t txfullsize;
    int block_file;
    uint32_t block_offset;
    uint32_t block_tx_offset;
    uint32_t flags;
    uint32_t timestamp;
    CDataStream ss(SER_DISK, CLIENT_VERSION);
    
    if((m_Mode & MC_WMD_TXS) == 0)
    {
        return MC_ERR_NOERROR;
    }    
    
    err=MC_ERR_NOERROR;
    lockres=m_Database->Lock(1,1);
    imp=import;
    if(imp == NULL)
    {
        imp=m_Database->FindImport(0);
    }
    
    import_pos=imp-m_Database->m_Imports;
    hash=tx.GetHash();
    
    fullTx=&tx;
    storedTx=&tx;
    
    fFound=false;
    txdef.Zero();
    if(m_Database->GetTx(&txdef,(unsigned char*)&hash) == MC_ERR_NOERROR)
    {
        fFound=true;        
    }
    
    if(!fFound)
    {
        for(i=0;i<(int)tx.vout.size();i++)                                      // Checking that tx has long OP_RETURN
        {
            const CTxOut txout=tx.vout[i];
            const CScript& script1 = txout.scriptPubKey;        
            CScript::const_iterator pc1 = script1.begin();

            std::vector<CTxDestination> addressRets;

            mc_gState->m_TmpScript->Clear();
            mc_gState->m_TmpScript->SetScript((unsigned char*)(&pc1[0]),(size_t)(script1.end()-pc1),MC_SCR_TYPE_SCRIPTPUBKEY);
            if(mc_gState->m_TmpScript->IsOpReturnScript())
            {                
                size_t elem_size;    
//                const unsigned char *elem;
                int num_elems=mc_gState->m_TmpScript->GetNumElements();

                mc_gState->m_TmpScript->GetData(num_elems-1,&elem_size);
                if(elem_size > MC_TDB_MAX_OP_RETURN_SIZE)
                {
                    storedTx=NULL;
                }
            }
        }
    }
    
    if(storedTx==NULL)
    {
        stx=mc_WalletTxChoppedCopy(m_lpWallet,tx);
        storedTx=&stx;
        if(fullTx->GetSerializeSize(SER_DISK, CLIENT_VERSION) <= storedTx->GetSerializeSize(SER_DISK, CLIENT_VERSION))
        {
            storedTx=fullTx;
        }
    }
            

    imp->m_TmpEntities->Clear();

    fIsFromMe=false;
    fIsToMe=false;
    fIsSpendable=false;
    fAllInputsFromMe=true;
    fAllInputsAreFinal=true;
    fSingleInputEntity=true;
    fNewStream=false;
    fNewAsset=false;
    input_entity.Zero();
    BOOST_FOREACH(const CTxIn& txin, tx.vin)                                    //Checking inputs    
    {
        if(!tx.IsCoinBase())
        {
            COutPoint outp(txin.prevout.hash,txin.prevout.n);
            std::map<COutPoint,mc_Coin>::const_iterator it = m_UTXOs[import_pos].find(outp);
            if (it != m_UTXOs[import_pos].end())                                // We have this input in our unspent coins list, otherwise - it is not our input
            {
                mc_Coin *lpTxOut;
                lpTxOut=(mc_Coin*)(&(it->second));
                mc_Coin utxo;
                utxo.m_OutPoint=outp;
                utxo.m_TXOut=lpTxOut->m_TXOut;
                utxo.m_Block=block;
                utxo.m_Flags=lpTxOut->m_Flags;
                utxo.m_EntityID=0;
                utxo.m_EntityType=MC_TET_NONE;
                utxo.m_LockTime=lpTxOut->m_LockTime;

                if (!txin.IsFinal())
                {
                    fAllInputsAreFinal=false;
                }

                const CScript& script1 = lpTxOut->m_TXOut.scriptPubKey;        
                CScript::const_iterator pc1 = script1.begin();

                txnouttype typeRet;
                int nRequiredRet;
                std::vector<CTxDestination> addressRets;

                ExtractDestinations(script1,typeRet,addressRets,nRequiredRet);

                BOOST_FOREACH(const CTxDestination& dest, addressRets)
                {
                    entity.Zero();
                    const CKeyID *lpKeyID=boost::get<CKeyID> (&dest);
                    const CScriptID *lpScriptID=boost::get<CScriptID> (&dest);
                    isminefilter mine=ISMINE_NO;
                    if(lpKeyID)
                    {
                        memcpy(entity.m_EntityID,lpKeyID,MC_TDB_ENTITY_ID_SIZE);
                        entity.m_EntityType=MC_TET_PUBKEY_ADDRESS | MC_TET_CHAINPOS;
                        mine = m_lpWallet ? IsMineKeyID(*m_lpWallet, *lpKeyID) : ISMINE_NO;
                    }
                    if(lpScriptID)
                    {
                        memcpy(entity.m_EntityID,lpScriptID,MC_TDB_ENTITY_ID_SIZE);
                        entity.m_EntityType=MC_TET_SCRIPT_ADDRESS | MC_TET_CHAINPOS;
                        mine = m_lpWallet ? IsMineScriptID(*m_lpWallet, *lpScriptID) : ISMINE_NO;                                                                
                    }
//                    isminefilter mine = IsMine(*m_lpWallet, dest);
                    if((mine & ISMINE_SPENDABLE) == ISMINE_NO)
                    {
                        fAllInputsFromMe=false;
                    }
                    if(entity.m_EntityType)
                    {
                        if(mine & ISMINE_SPENDABLE)
                        {
                            fIsSpendable=true;
                        }
                        if(mine & ISMINE_ALL)
                        {
                            fIsFromMe=true;
                        }
                        if(addressRets.size() == 1)
                        {
                            if(fSingleInputEntity)
                            {
                                if(input_entity.m_EntityType)
                                {
                                    if(memcmp(&input_entity,&entity,sizeof(mc_TxEntity)))
                                    {
                                        fSingleInputEntity=false;
                                    }
                                }
                                else
                                {
                                    memcpy(&input_entity,&entity,sizeof(mc_TxEntity));
                                }
                            }                        
                        }
                        else
                        {
                            fSingleInputEntity=false;
                        }
                        fRelevantEntity=false;
                        if(imp->FindEntity(&entity) >= 0)    
                        {
                            fRelevantEntity=true;
                            if( (imp->m_ImportID > 0) && (block < 0) )          // When replaying mempool when completing import
                            {
                                entpos=m_Database->m_Imports->FindEntity(&entity);
                                if(entpos >= 0)                                 // Ignore entities already processed in the chain import
                                {
                                    lpentstat=m_Database->m_Imports->GetEntity(entpos);
                                    if( (lpentstat->m_Flags & MC_EFL_NOT_IN_SYNC) == 0 )
                                    {
                                        fRelevantEntity=false;
                                    }
                                }
                            }
                        }
                        if(fRelevantEntity)    
                        {
                            if(addressRets.size() == 1)
                            {
                                memcpy(&(utxo.m_EntityID),entity.m_EntityID,MC_TDB_ENTITY_ID_SIZE);
                                utxo.m_EntityType=entity.m_EntityType;
                            }
                            txoutsIn.push_back(utxo);

                            if(mine & ISMINE_ALL)
                            {
                                if(imp->m_TmpEntities->Seek(&entity) < 0)
                                {
                                    imp->m_TmpEntities->Add(&entity,NULL);
                                }
                            }
                        }
                    }
                }
            }
            else
            {
                fAllInputsFromMe=false;        
                fSingleInputEntity=false;
            }
        }
        else
        {
            fAllInputsFromMe=false;        
            fSingleInputEntity=false;            
        }
    }
    
    mc_gState->m_TmpAssetsOut->Clear();    
    
    for(i=0;i<(int)tx.vout.size();i++)                                          // Checking outputs
    {        
        const CTxOut txout=tx.vout[i];
        const CScript& script1 = txout.scriptPubKey;        
        CScript::const_iterator pc1 = script1.begin();

        txnouttype typeRet;
        int nRequiredRet;
        std::vector<CTxDestination> addressRets;
        int64_t quantity;
        
        mc_gState->m_TmpScript->Clear();
        mc_gState->m_TmpScript->SetScript((unsigned char*)(&pc1[0]),(size_t)(script1.end()-pc1),MC_SCR_TYPE_SCRIPTPUBKEY);
        if(mc_gState->m_TmpScript->IsOpReturnScript() == 0)
        {                
            mc_Coin utxo;
            utxo.m_OutPoint=COutPoint(tx.GetHash(),i);
            utxo.m_TXOut=txout;
            utxo.m_Block=block;
            utxo.m_Flags=0;
            utxo.m_EntityID=0;
            utxo.m_EntityType=MC_TET_NONE;
            utxo.m_LockTime=tx.nLockTime;
            ExtractDestinations(script1,typeRet,addressRets,nRequiredRet);

            for (int e = 0; e < mc_gState->m_TmpScript->GetNumElements(); e++)
            {
                mc_gState->m_TmpScript->SetElement(e);
                mc_gState->m_TmpScript->GetAssetQuantities(mc_gState->m_TmpAssetsOut,MC_SCR_ASSET_SCRIPT_TYPE_TRANSFER | MC_SCR_ASSET_SCRIPT_TYPE_FOLLOWON);
                if(mc_gState->m_TmpScript->GetAssetGenesis(&quantity) == 0)
                {
                    fNewAsset=true;                    
                }
            }            
            
            BOOST_FOREACH(const CTxDestination& dest, addressRets)
            {
                entity.Zero();
                const CKeyID *lpKeyID=boost::get<CKeyID> (&dest);
                const CScriptID *lpScriptID=boost::get<CScriptID> (&dest);
                isminefilter mine=ISMINE_NO;
                if(lpKeyID)
                {
                    memcpy(entity.m_EntityID,lpKeyID,MC_TDB_ENTITY_ID_SIZE);
                    entity.m_EntityType=MC_TET_PUBKEY_ADDRESS | MC_TET_CHAINPOS;
                    mine = m_lpWallet ? IsMineKeyID(*m_lpWallet, *lpKeyID) : ISMINE_NO;
                }
                if(lpScriptID)
                {
                    memcpy(entity.m_EntityID,lpScriptID,MC_TDB_ENTITY_ID_SIZE);
                    entity.m_EntityType=MC_TET_SCRIPT_ADDRESS | MC_TET_CHAINPOS;
                    mine = m_lpWallet ? IsMineScriptID(*m_lpWallet, *lpScriptID) : ISMINE_NO;                                                                
                }
                if(entity.m_EntityType)
                {
//                    isminefilter mine = m_lpWallet ? IsMine(*m_lpWallet, dest) : ISMINE_NO;

                    fOutputIsSpendable=false;
                    if(mine & ISMINE_SPENDABLE)
                    {
                        fIsSpendable=true;
                        fOutputIsSpendable=true;
                    }
                    if(mine & ISMINE_ALL)
                    {
                        fIsToMe=true;
                    }
                    fRelevantEntity=false;
                    if(imp->FindEntity(&entity) >= 0)    
                    {
                        fRelevantEntity=true;
                        if( (imp->m_ImportID > 0) && (block < 0) )              // When replaying mempool when completing import
                        {
                            entpos=m_Database->m_Imports->FindEntity(&entity);
                            if(entpos >= 0)                                     // Ignore entities already processed in the chain import
                            {
                                lpentstat=m_Database->m_Imports->GetEntity(entpos);
                                if( (lpentstat->m_Flags & MC_EFL_NOT_IN_SYNC) == 0 )
                                {
                                    fRelevantEntity=false;
                                }
                            }
                        }
                    }
                    if(fRelevantEntity)    
                    {
                        if(addressRets.size() == 1)
                        {
                            memcpy(&(utxo.m_EntityID),entity.m_EntityID,MC_TDB_ENTITY_ID_SIZE);
                            utxo.m_EntityType=entity.m_EntityType;
                            if(fOutputIsSpendable)
                            {
                                utxo.m_Flags |= MC_TFL_IS_SPENDABLE;
                            }
                        }
                        txoutsOut.push_back(utxo);
                                                
                        if(mine & ISMINE_ALL)
                        {
                            if(imp->m_TmpEntities->Seek(&entity) < 0)
                            {
                                imp->m_TmpEntities->Add(&entity,NULL);
                            }
                        }
                    }
                }
            }
        }
        else
        {
            mc_gState->m_TmpScript->ExtractAndDeleteDataFormat(NULL);
            if(mc_gState->m_TmpScript->GetNumElements() >= 3) // 2 OP_DROPs + OP_RETURN - item key
            {
                mc_gState->m_TmpScript->DeleteDuplicatesInRange(1,mc_gState->m_TmpScript->GetNumElements()-1);
                
                unsigned char short_txid[MC_AST_SHORT_TXID_SIZE];
                mc_gState->m_TmpScript->SetElement(0);
                                                                            
                if(mc_gState->m_TmpScript->GetEntity(short_txid) == 0)           
                {
                    entity.Zero();
                    memcpy(entity.m_EntityID,short_txid,MC_AST_SHORT_TXID_SIZE);
                    entity.m_EntityType=MC_TET_STREAM | MC_TET_CHAINPOS;
                    if(imp->FindEntity(&entity) >= 0)    
                    {
//                        if(imp->m_TmpEntities->Seek(&entity) < 0)
                        {
                            extension.Zero();
                            extension.m_Output=i;
                            base_row=imp->m_TmpEntities->Seek(&entity);
                            if(base_row >= 0)
                            {
                                lpext=(mc_TxEntityRowExtension*)(imp->m_TmpEntities->GetRow(base_row)+sizeof(mc_TxEntity));
                                lpext->m_TmpLastCount+=1;
                                extension.m_Count=lpext->m_TmpLastCount;
                            }
                            
                            imp->m_TmpEntities->Add(&entity,&extension);
                            
                            extension.m_Count=0;
                            for(int e=1;e<mc_gState->m_TmpScript->GetNumElements()-1;e++)
                            {
                                mc_gState->m_TmpScript->SetElement(e);
                                if(mc_gState->m_TmpScript->GetItemKey(item_key,&item_key_size))   // Item key
                                {
                                    err=MC_ERR_INTERNAL_ERROR;
                                    goto exitlbl;                                                                                                                                        
                                }                    

                                subkey_hash160=Hash160(item_key,item_key+item_key_size);
                                subkey_hash256=0;
                                memcpy(&subkey_hash256,&subkey_hash160,sizeof(subkey_hash160));
                                err=m_Database->AddSubKeyDef(imp,(unsigned char*)&subkey_hash256,item_key,item_key_size,MC_SFL_SUBKEY);
                                if(err)
                                {
                                    goto exitlbl;
                                }

                                mc_GetCompoundHash160(&stream_subkey_hash160,entity.m_EntityID,&subkey_hash160);

                                map<uint160,int>::iterator it = subkey_count_map.find(stream_subkey_hash160);
                                if (it == subkey_count_map.end())
                                {
                                    subkey_count_map.insert(make_pair(stream_subkey_hash160,0));
                                }
                                else
                                {
                                    it->second += 1;
                                    extension.m_Count=it->second;
                                }
                                
                                entity.m_EntityType=MC_TET_STREAM_KEY | MC_TET_CHAINPOS;
                                subkey_entity.Zero();
                                memcpy(subkey_entity.m_EntityID,&stream_subkey_hash160,MC_TDB_ENTITY_ID_SIZE);
                                subkey_entity.m_EntityType=MC_TET_SUBKEY_STREAM_KEY | MC_TET_CHAINPOS;
                                err= m_Database->IncrementSubKey(imp,&entity,&subkey_entity,(unsigned char*)&subkey_hash160,(unsigned char*)&hash,&extension,block,0,fFound ? 0 : 1);
                                if(err)
                                {
                                    goto exitlbl;
                                }

                                entity.m_EntityType=MC_TET_STREAM_KEY | MC_TET_TIMERECEIVED;
                                subkey_entity.m_EntityType=MC_TET_SUBKEY_STREAM_KEY | MC_TET_TIMERECEIVED;
                                err= m_Database->IncrementSubKey(imp,&entity,&subkey_entity,(unsigned char*)&subkey_hash160,(unsigned char*)&hash,&extension,block,0,fFound ? 0 : 1);
                                if(err)
                                {
                                    goto exitlbl;
                                }                           
                            }

                            publishers_set.clear();
                            for (j = 0; j < (int)tx.vin.size(); ++j)
                            {
                                int op_addr_offset,op_addr_size,is_redeem_script,sighash_type;
                                uint32_t publisher_flags;
                                const unsigned char *ptr;

                                const CScript& script2 = tx.vin[j].scriptSig;        
                                CScript::const_iterator pc2 = script2.begin();

                                ptr=mc_ExtractAddressFromInputScript((unsigned char*)(&pc2[0]),(int)(script2.end()-pc2),&op_addr_offset,&op_addr_size,&is_redeem_script,&sighash_type,0);                                
                                if(ptr)
                                {
                                    if( (sighash_type == SIGHASH_ALL) || ( (sighash_type == SIGHASH_SINGLE) && (j == i) ) )
                                    {                                        
                                        subkey_hash160=Hash160(ptr+op_addr_offset,ptr+op_addr_offset+op_addr_size);
                                        if(publishers_set.count(subkey_hash160) == 0)
                                        {
                                            publishers_set.insert(subkey_hash160);
                                            subkey_hash256=0;
                                            memcpy(&subkey_hash256,&subkey_hash160,sizeof(subkey_hash160));
                                            publisher_flags=MC_SFL_SUBKEY | MC_SFL_IS_ADDRESS;
                                            if(is_redeem_script)
                                            {
                                                publisher_flags |= MC_SFL_IS_SCRIPTHASH;
                                            }
                                            err=m_Database->AddSubKeyDef(imp,(unsigned char*)&subkey_hash256,NULL,0,publisher_flags);
                                            if(err)
                                            {
                                                goto exitlbl;
                                            }
                                            
                                            mc_GetCompoundHash160(&stream_subkey_hash160,entity.m_EntityID,&subkey_hash160);
                            
                                            map<uint160,int>::iterator it = subkey_count_map.find(stream_subkey_hash160);
                                            if (it == subkey_count_map.end())
                                            {
                                                subkey_count_map.insert(make_pair(stream_subkey_hash160,0));
                                            }
                                            else
                                            {
                                                it->second += 1;
                                                extension.m_Count=it->second;
                                            }
                                            
                                            entity.m_EntityType=MC_TET_STREAM_PUBLISHER | MC_TET_CHAINPOS;
                                            subkey_entity.Zero();
                                            memcpy(subkey_entity.m_EntityID,&stream_subkey_hash160,MC_TDB_ENTITY_ID_SIZE);
                                            subkey_entity.m_EntityType=MC_TET_SUBKEY_STREAM_PUBLISHER | MC_TET_CHAINPOS;
                                            err= m_Database->IncrementSubKey(imp,&entity,&subkey_entity,(unsigned char*)&subkey_hash160,(unsigned char*)&hash,&extension,block,0,fFound ? 0 : 1);
                                            if(err)
                                            {
                                                goto exitlbl;
                                            }
                            
                                            entity.m_EntityType=MC_TET_STREAM_PUBLISHER | MC_TET_TIMERECEIVED;
                                            subkey_entity.m_EntityType=MC_TET_SUBKEY_STREAM_PUBLISHER | MC_TET_TIMERECEIVED;
                                            err= m_Database->IncrementSubKey(imp,&entity,&subkey_entity,(unsigned char*)&subkey_hash160,(unsigned char*)&hash,&extension,block,0,fFound ? 0 : 1);
                                            if(err)
                                            {
                                                goto exitlbl;
                                            }                                                                       
                                        }
                                    }
                                }        
                            }                            
                        }                            
                    }                    
                }
            }     
            if(m_Mode & MC_WMD_AUTOSUBSCRIBE_STREAMS)
            {
                if(mc_gState->m_TmpScript->GetNumElements() == 2) 
                {
                    uint32_t new_entity_type;
                    mc_gState->m_TmpScript->SetElement(0);
                                                                                // Should be spkn
                    if(mc_gState->m_TmpScript->GetNewEntityType(&new_entity_type) == 0)    // New entity element
                    {
                        if(new_entity_type == MC_ENT_TYPE_STREAM)
                        {
                            entity.Zero();
                            memcpy(entity.m_EntityID,(unsigned char*)&hash+MC_AST_SHORT_TXID_OFFSET,MC_AST_SHORT_TXID_SIZE);
                            entity.m_EntityType=MC_TET_STREAM | MC_TET_CHAINPOS;
                            if(imp->FindEntity(&entity) < 0)    
                            {
                                if(imp->m_ImportID == 0)
                                {
                                    fNewStream=true;
                                }
/*                                
                                else
                                {
                                    int chain_row=m_Database->m_Imports->FindEntity(&entity);
                                    if(chain_row < 0)    
                                    {
                                        fNewStream=true;
                                    }
                                    else
                                    {
                                        if(m_Database->m_Imports->GetEntity(chain_row)->m_Flags & MC_EFL_NOT_IN_SYNC)
                                        {
                                            fNewStream=true;                                            
                                        }
                                    }
                                }
 */ 
                            }
                        }
                    }
                }                
            }
        }
    }
    
    if(fNewAsset)
    {
        entity.Zero();
        if(mc_gState->m_Features->ShortTxIDInTx())
        {
            entity.m_EntityType=MC_TET_ASSET | MC_TET_CHAINPOS;
            memcpy(entity.m_EntityID,(unsigned char*)&hash+MC_AST_SHORT_TXID_OFFSET,MC_AST_SHORT_TXID_SIZE);
        }
        else
        {
            if(block >= 0)
            {
                entity.m_EntityType=MC_TET_ASSET | MC_TET_CHAINPOS;
                int tx_offset=sizeof(CBlockHeader)+block_pos->nTxOffset;
                mc_PutLE(entity.m_EntityID,&block,4);
                mc_PutLE(entity.m_EntityID+4,&tx_offset,4);
                for(i=0;i<MC_ENT_REF_PREFIX_SIZE;i++)
                {
                    entity.m_EntityID[8+i]=*((unsigned char*)&hash+MC_ENT_KEY_SIZE-1-i);
                }
            }
        }
        if(entity.m_EntityType)
        {
            if(imp->FindEntity(&entity) >= 0)    
            {
                if(imp->m_TmpEntities->Seek(&entity) < 0)
                {
                    imp->m_TmpEntities->Add(&entity,NULL);
                }
            }
            else
            {
                if(m_Mode & MC_WMD_AUTOSUBSCRIBE_ASSETS)
                {
                    fNewAsset=false;
                    if(imp->m_ImportID == 0)
                    {
                        fNewAsset=true;
                    }
                    if(fNewAsset)
                    {
                        m_Database->AddEntity(imp,&entity,0);
                        imp->m_TmpEntities->Add(&entity,NULL);
                        entity.m_EntityType=MC_TET_ASSET | MC_TET_TIMERECEIVED;
                        m_Database->AddEntity(imp,&entity,0);
                    }
                }
            }
        }
    }
    
    for(i=0;i<mc_gState->m_TmpAssetsOut->GetCount();i++)
    {
        ptrOut=mc_gState->m_TmpAssetsOut->GetRow(i);
        entity.Zero();
        if(mc_gState->m_Features->ShortTxIDInTx())
        {
            memcpy(entity.m_EntityID,ptrOut+MC_AST_SHORT_TXID_OFFSET,MC_AST_SHORT_TXID_SIZE);
        }
        else
        {
            memcpy(entity.m_EntityID,ptrOut,MC_AST_ASSET_REF_SIZE);            
        }
        entity.m_EntityType=MC_TET_ASSET | MC_TET_CHAINPOS;
        fRelevantEntity=false;
        if(imp->FindEntity(&entity) >= 0)    
        {
            fRelevantEntity=true;
            if( (imp->m_ImportID > 0) && (block < 0) )              // When replaying mempool when completing import
            {
                entpos=m_Database->m_Imports->FindEntity(&entity);
                if(entpos >= 0)                                     // Ignore entities already processed in the chain import
                {
                    lpentstat=m_Database->m_Imports->GetEntity(entpos);
                    if( (lpentstat->m_Flags & MC_EFL_NOT_IN_SYNC) == 0 )
                    {
                        fRelevantEntity=false;
                    }
                }
            }
        }
        if(fRelevantEntity)    
        {
            if(imp->m_TmpEntities->Seek(&entity) < 0)
            {
                imp->m_TmpEntities->Add(&entity,NULL);
            }
        }
    }
    
    
    entcount=imp->m_TmpEntities->GetCount();

    if( (imp->m_ImportID > 0) && (block < 0) )                                  // When replaying mempool when completing import
    {
        if(entcount == 0)                                                       // Skip AddTx if nothing was found
        {
            goto exitlbl;
        }        
    }
//    if(entcount == 0)
    if(!fIsFromMe && !fIsToMe && (entcount == 0))
    {
//        printf("Nothing found\n");
        goto exitlbl;
    }

    if(block == 0)                                                              // Ignoring genesis coinbase
    {
        goto exitlbl;        
    }
    
    if( (imp->m_ImportID == 0) || (block >= 0) )                                // Ignore when replaying mempool when completing import
    {        
        if(fIsFromMe || fIsToMe)
        {
            entity.Zero();
            entity.m_EntityType=MC_TET_WALLET_ALL | MC_TET_CHAINPOS;
            imp->m_TmpEntities->Add(&entity,NULL);
            if(fIsSpendable)
            {
                entity.m_EntityType=MC_TET_WALLET_SPENDABLE | MC_TET_CHAINPOS;
                imp->m_TmpEntities->Add(&entity,NULL);        
            }
        }
    }    
    
    entcount=imp->m_TmpEntities->GetCount();
    
    for(i=0;i<entcount;i++)
    {
        lpent=(mc_TxEntity*)imp->m_TmpEntities->GetRow(i);
        memcpy(&entity,lpent,sizeof(mc_TxEntity));
        entity.m_EntityType -= MC_TET_CHAINPOS;
        entity.m_EntityType |= MC_TET_TIMERECEIVED;
        imp->m_TmpEntities->Add(&entity,NULL);
    }
    
    ss.reserve(10000);
    ss << *storedTx;
    
    txsize=ss.size();
    if(fFound)
    {
        txfullsize=txdef.m_FullSize;
    }
    else
    {
        txfullsize=fullTx->GetSerializeSize(SER_DISK, CLIENT_VERSION);
    }
    flags=0;
    if(tx.IsCoinBase())
    {
        flags |= MC_TFL_IS_COINBASE;
    }    
    if(fAllInputsAreFinal)
    {
        flags |= MC_TFL_ALL_INPUTS_ARE_FINAL;
    }
    if(fIsFromMe)
    {
        flags |= MC_TFL_FROM_ME;
    }
    if(fAllInputsFromMe)
    {
        flags |= MC_TFL_ALL_INPUTS_FROM_ME;
    }
    
    timestamp=mc_TimeNowAsUInt();
    if(block >= 0)
    {
        block_file=block_pos->nFile;
        block_offset=block_pos->nPos;
        block_tx_offset=block_pos->nTxOffset;
    }
    else
    {
        block_file=-1;
        block_offset=0;
        block_tx_offset=0;        
    }
    
    fAlreadyInTheWalletForNotify=false;
    if(!GetArg("-walletnotify", "").empty() || !GetArg("-walletnotifynew", "").empty())
    {
        mc_TxDefRow StoredTxDef;
        if(m_Database->GetTx(&StoredTxDef,(unsigned char*)&hash) == 0)
        {
            fAlreadyInTheWalletForNotify=true;
        }        
    }
    
    
    if(fDebug)LogPrint("wallet","wtxs: Found %d entities in tx %s, flags: %08X, import %d\n",imp->m_TmpEntities->GetCount(),tx.GetHash().ToString().c_str(),flags,imp->m_ImportID);
    err=m_Database->AddTx(imp,(unsigned char*)&hash,(unsigned char*)&ss[0],txsize,txfullsize,block,block_file,block_offset,block_tx_offset,block_tx_index,flags,timestamp,imp->m_TmpEntities);
    if(err == MC_ERR_NOERROR)                                                   // Adding tx to unconfirmed send
    {
        if((block < 0) && (imp->m_ImportID == 0) && (fIsFromMe || (txsize != txfullsize)))
        {
            std::map<uint256,CWalletTx>::const_iterator it = m_UnconfirmedSends.find(hash);
            if (it == m_UnconfirmedSends.end())
            {
                err=AddToUnconfirmedSends(m_Database->m_DBStat.m_Block,tx);
            }
        }
    }
    
    WalletTxNotify(imp,tx,block,fAlreadyInTheWalletForNotify,block_hash);
    
    if(err == MC_ERR_NOERROR)                                                   // Updating UTXO map
    {
        for(i=0;i<(int)txoutsIn.size();i++)
        {
            m_UTXOs[import_pos].erase(txoutsIn[i].m_OutPoint);
        }
        for(i=0;i<(int)txoutsOut.size();i++)
        {
            std::map<COutPoint, mc_Coin>::const_iterator itold = m_UTXOs[import_pos].find(txoutsOut[i].m_OutPoint);
            if (itold == m_UTXOs[import_pos].end())
            {
                txoutsOut[i].m_Flags |= flags;
                if(fSingleInputEntity)
                {
                    if(input_entity.m_EntityType)
                    {
                        if(input_entity.m_EntityType == txoutsOut[i].m_EntityType)
                        {                                    
                            if(memcmp(input_entity.m_EntityID,&(txoutsOut[i].m_EntityID),MC_TDB_ENTITY_ID_SIZE) == 0)
                            {
                                txoutsOut[i].m_Flags |= MC_TFL_IS_CHANGE;
                            }
                        }
                    }
                }
                m_UTXOs[import_pos].insert(make_pair(txoutsOut[i].m_OutPoint, txoutsOut[i]));
            }                    
        }
    }    
    
exitlbl:
                                                
    if(err == MC_ERR_NOERROR)                                                   // Adding new entities
    {
        if(fNewStream)
        {
            entity.Zero();
            memcpy(entity.m_EntityID,(unsigned char*)&hash+MC_AST_SHORT_TXID_OFFSET,MC_AST_SHORT_TXID_SIZE);
            entity.m_EntityType=MC_TET_STREAM | MC_TET_CHAINPOS;
            m_Database->AddEntity(imp,&entity,0);
            entity.m_EntityType=MC_TET_STREAM | MC_TET_TIMERECEIVED;
            m_Database->AddEntity(imp,&entity,0);
            entity.m_EntityType=MC_TET_STREAM_KEY | MC_TET_CHAINPOS;
            m_Database->AddEntity(imp,&entity,0);
            entity.m_EntityType=MC_TET_STREAM_KEY | MC_TET_TIMERECEIVED;
            m_Database->AddEntity(imp,&entity,0);
            entity.m_EntityType=MC_TET_STREAM_PUBLISHER | MC_TET_CHAINPOS;
            m_Database->AddEntity(imp,&entity,0);
            entity.m_EntityType=MC_TET_STREAM_PUBLISHER | MC_TET_TIMERECEIVED;
            m_Database->AddEntity(imp,&entity,0);            
        }       
    }
    
    if(err)
    {
        LogPrintf("wtxs: AddTx  Error: %d\n",err);        
    }
    if(lockres == 0)
    {
        m_Database->UnLock();
    }
    return err;
}

int mc_WalletTxs::FindWalletTx(uint256 hash,mc_TxDefRow *txdef)
{
    int err,lockres;
    mc_TxDefRow StoredTxDef;
    
    lockres=1;
    if((m_Mode & MC_WMD_TXS) == 0)
    {
        err=MC_ERR_NOT_SUPPORTED;
        goto exitlbl;
    }    
    err = MC_ERR_NOERROR;
    
    if(m_Database == NULL)
    {
        goto exitlbl;
    }
    
    lockres=m_Database->Lock(0,1);
    
    err=m_Database->GetTx(&StoredTxDef,(unsigned char*)&hash);
    if(err)
    {
        goto exitlbl;
    }

    if(txdef)
    {
        memcpy(txdef,&StoredTxDef,sizeof(mc_TxDefRow));
    }
    
exitlbl:    

    if(lockres == 0)
    {
        m_Database->UnLock();
    }
    return err;
    
}

CWalletTx mc_WalletTxs::GetWalletTx(uint256 hash,mc_TxDefRow *txdef,int *errOut)
{
    int err;
    CWalletTx wtx;
    mc_TxDefRow StoredTxDef;
    FILE* fHan;
    char ShortName[65];                                     
    char FileName[MC_DCT_DB_MAX_PATH];                      
    
    if((m_Mode & MC_WMD_TXS) == 0)
    {
        err=MC_ERR_NOT_SUPPORTED;
        goto exitlbl;
    }    
    err = MC_ERR_NOERROR;
    
    if(m_Database == NULL)
    {
        goto exitlbl;
    }
    
    m_Database->Lock(0,0);
    
    err=m_Database->GetTx(&StoredTxDef,(unsigned char*)&hash);
    if(err)
    {
        goto exitlbl;
    }

    if((StoredTxDef.m_Block >= 0) || (StoredTxDef.m_Size == StoredTxDef.m_FullSize))// We have full tx in wallet or in the block
    {
        sprintf(ShortName,"wallet/txs%05u",StoredTxDef.m_InternalFileID);

        mc_GetFullFileName(m_Database->m_Name,ShortName,".dat",MC_FOM_RELATIVE_TO_DATADIR | MC_FOM_CREATE_DIR,FileName);

        fHan=fopen(FileName,"rb+");

        if(fHan == NULL)
        {
            goto exitlbl;
        }

        CAutoFile filein(fHan, SER_DISK, CLIENT_VERSION);

        fseek(filein.Get(), StoredTxDef.m_InternalFileOffset, SEEK_SET);
        try 
        {
            filein >> wtx;                                                      // Extract wallet tx anyway - for metadata
        } 
        catch (std::exception &e) 
        {           
            err=MC_ERR_FILE_READ_ERROR;
            goto exitlbl;
        }
        
        if(StoredTxDef.m_Size != StoredTxDef.m_FullSize)                        // if tx is shortened, extract OP_RETURN metadata from block
        {
            CDiskTxPos postx(CDiskBlockPos(StoredTxDef.m_BlockFileID,StoredTxDef.m_BlockOffset),StoredTxDef.m_BlockTxOffset);
            
            CAutoFile file(OpenBlockFile(postx, true), SER_DISK, CLIENT_VERSION);
            if (file.IsNull())
            {
                err=MC_ERR_FILE_READ_ERROR;
                goto exitlbl;
            }
            CBlockHeader header;
            CTransaction tx;
            try 
            {
                file >> header;
                fseek(file.Get(), postx.nTxOffset, SEEK_CUR);
                file >> tx;
            } 
            catch (std::exception &e) 
            {
                err=MC_ERR_FILE_READ_ERROR;
                goto exitlbl;
            }

            *static_cast<CTransaction*>(&wtx) = CTransaction(tx);           
        }
    }
    else                                                                        // We have full tx only in unconfirmed sends
    {
        std::map<uint256,CWalletTx>::const_iterator it = m_UnconfirmedSends.find(hash);
        if (it != m_UnconfirmedSends.end())
        {
            wtx=it->second;
        }
        else
        {
            err=MC_ERR_CORRUPTED;
        }
    }
    
    
exitlbl:
    
    if(err == MC_ERR_NOERROR)
    {
        if(wtx.GetHash() != hash)
        {
            err=MC_ERR_CORRUPTED;            
        }
    }

    if(err == MC_ERR_NOERROR)
    {      
        wtx.BindWallet(m_lpWallet);
        wtx.nTimeReceived=StoredTxDef.m_TimeReceived;
        wtx.nTimeSmart=StoredTxDef.m_TimeReceived;
        memcpy(&wtx.txDef,&StoredTxDef,sizeof(mc_TxDefRow));
        if(txdef)
        {
            memcpy(txdef,&StoredTxDef,sizeof(mc_TxDefRow));
        }
    }

    if(errOut)
    {
        *errOut=err;
    }

    m_Database->UnLock();
    return wtx;
}

/*
 * Extract wallet tx as it is stored in the wallet, ignoring OP_RETURN metadata
 */

CWalletTx mc_WalletTxs::GetInternalWalletTx(uint256 hash,mc_TxDefRow *txdef,int *errOut)
{
    int err;
    CWalletTx wtx;
    mc_TxDefRow StoredTxDef;
    char ShortName[65];                                     
    char FileName[MC_DCT_DB_MAX_PATH];                      
    
    err = MC_ERR_NOERROR;

    if(hash == 0)
    {
        err=MC_ERR_NOT_FOUND;
        goto exitlbl;
    }
    
    err=m_Database->GetTx(&StoredTxDef,(unsigned char*)&hash);
    
    if(err)
    {
        goto exitlbl;
    }
    else
    {
        sprintf(ShortName,"wallet/txs%05u",StoredTxDef.m_InternalFileID);

        mc_GetFullFileName(m_Database->m_Name,ShortName,".dat",MC_FOM_RELATIVE_TO_DATADIR | MC_FOM_CREATE_DIR,FileName);

        CAutoFile filein(fopen(FileName,"rb+"), SER_DISK, CLIENT_VERSION);

        fseek(filein.Get(), StoredTxDef.m_InternalFileOffset, SEEK_SET);
        try 
        {
            filein >> wtx;
        } 
        catch (std::exception &e) 
        {           
            err=MC_ERR_FILE_READ_ERROR;
            goto exitlbl;
        }
    }
           
exitlbl:
    
    if(err == MC_ERR_NOERROR)
    {
        if(StoredTxDef.m_Size == StoredTxDef.m_FullSize)
        {
            if(wtx.GetHash() != hash)                                           // Check it only if full transaction was found
            {
                err=MC_ERR_CORRUPTED;            
            }
        }
    }

    if(err == MC_ERR_NOERROR)
    {
        wtx.BindWallet(m_lpWallet);
        wtx.nTimeReceived=StoredTxDef.m_TimeReceived;
        wtx.nTimeSmart=StoredTxDef.m_TimeReceived;
        memcpy(&wtx.txDef,&StoredTxDef,sizeof(mc_TxDefRow));
        if(txdef)
        {
            memcpy(txdef,&StoredTxDef,sizeof(mc_TxDefRow));
        }
    }

    if(errOut)
    {
        *errOut=err;
    }

    return wtx;
}

string mc_WalletTxs::GetSubKey(void *hash,mc_TxDefRow *txdef,int *errOut)
{
    int err;
    string ret;
    mc_TxDefRow StoredTxDef;
    char ShortName[65];                                     
    char FileName[MC_DCT_DB_MAX_PATH];                      
    int  fHan=0;
    uint160 subkey_hash;
    
    err = MC_ERR_NOERROR;

    ret="";
    err=m_Database->GetTx(&StoredTxDef,(unsigned char*)hash);
    
    if(err)
    {
        goto exitlbl;
    }
    else
    {
        if(StoredTxDef.m_Flags & MC_SFL_NODATA)
        {
            if(StoredTxDef.m_Flags & MC_SFL_IS_ADDRESS)
            {
                memcpy(&subkey_hash,StoredTxDef.m_TxId,MC_TDB_ENTITY_ID_SIZE);
                if(StoredTxDef.m_Flags & MC_SFL_IS_SCRIPTHASH)
                {
                    ret=CBitcoinAddress((CScriptID)subkey_hash).ToString();
                }
                else
                {
                    ret=CBitcoinAddress((CKeyID)subkey_hash).ToString();                    
                }
            }
        }
        else
        {
            unsigned char buf[256];
            int total_bytes_read,bytes_to_read;
            
            sprintf(ShortName,"wallet/txs%05u",StoredTxDef.m_InternalFileID);

            mc_GetFullFileName(m_Database->m_Name,ShortName,".dat",MC_FOM_RELATIVE_TO_DATADIR | MC_FOM_CREATE_DIR,FileName);
            
            fHan=open(FileName,_O_BINARY | O_RDONLY, S_IRUSR | S_IWUSR);

            if(fHan <= 0)
            {
                err= MC_ERR_FILE_READ_ERROR;
                goto exitlbl;
                
            }

            if(lseek64(fHan,StoredTxDef.m_InternalFileOffset,SEEK_SET) != StoredTxDef.m_InternalFileOffset)
            {
                err= MC_ERR_FILE_READ_ERROR;
                goto exitlbl;
            }
                        
            total_bytes_read=0;
            while(total_bytes_read < (int)StoredTxDef.m_Size)
            {
                bytes_to_read=StoredTxDef.m_Size-total_bytes_read;
                if(bytes_to_read > 256)
                {
                    bytes_to_read=256;
                }
                if(read(fHan,buf,bytes_to_read) != bytes_to_read)
                {
                    err= MC_ERR_FILE_READ_ERROR;
                    goto exitlbl;
                }
                total_bytes_read+=bytes_to_read;
                ret += string((char*)buf,bytes_to_read);
            }
        }
    }
           
exitlbl:
    
    if(fHan)
    {
        close(fHan);
    }
                                                

    if(err == MC_ERR_NOERROR)
    {
        if(txdef)
        {
            memcpy(txdef,&StoredTxDef,sizeof(mc_TxDefRow));
        }
    }

    if(errOut)
    {
        *errOut=err;
    }

    return ret;    
}


int mc_WalletTxs::GetEntityListCount()
{
    if((m_Mode & MC_WMD_TXS) == 0)
    {
        return 0;
    }    
    if(m_Database == NULL)
    {
        return 0;
    }
    
    return m_Database->m_Imports->m_Entities->GetCount();
}

mc_TxEntityStat *mc_WalletTxs::GetEntity(int row)
{
    if((m_Mode & MC_WMD_TXS) == 0)
    {
        return NULL;
    }    
    if(m_Database == NULL)
    {
        return NULL;
    }
    
    return m_Database->m_Imports->GetEntity(row);    
}
