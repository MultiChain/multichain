// Copyright (c) 2014-2016 The Bitcoin Core developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "wallet/wallettxs.h"
#include "utils/core_io.h"
#include "community/community.h"

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
bool IsLicenseTokenIssuance(mc_Script *lpScript,uint256 hash);
bool IsLicenseTokenTransfer(mc_Script *lpScript,mc_Buffer *amounts);
void SetRPCWRPReadLockFlag(int lock);

using namespace std;

uint32_t mc_AutosubscribeWalletMode(std::string parameters,bool check_license)
{
    uint32_t mode=0;
    
    vector<string> inputStrings;
    stringstream ss(parameters); 
    string tok;
    
    while(getline(ss, tok, ',')) 
    {
        inputStrings.push_back(tok);
    }
    
    for(int is=0;is<(int)inputStrings.size();is++)
    {
        if(inputStrings[is] == "assets")
        {
            mode |= MC_WMD_AUTOSUBSCRIBE_ASSETS;
        }
        if(inputStrings[is] == "streams")
        {
            mode |= MC_WMD_AUTOSUBSCRIBE_STREAMS;
        }
    }

    if(pEF->STR_CheckAutoSubscription(parameters,check_license))
    {
            mode |= MC_WMD_AUTOSUBSCRIBE_STREAMS;        
    }
    
    return mode;
}

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
    m_CSAssets.clear();
    m_CSDetails.m_Active=false;
    m_CSDetails.m_CSDestination=CNoDestination();
    m_CSDetails.m_Required=0;
    m_CSDetails.m_WithInlineData=false;
    m_CSDetails.m_IsEmpty=false;
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
    m_ChunkDB=NULL;
    m_ChunkCollector=NULL;
    m_lpWallet=NULL;
    m_ChunkBuffer=NULL;
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
    
    m_ChunkDB=new mc_ChunkDB;

    err=m_ChunkDB->Initialize(name,mode);

    if(err)
    {
        return err;
    }
    
    m_ChunkCollector=new mc_ChunkCollector;
    
    err=m_ChunkCollector->Initialize(m_ChunkDB,name,mode);
    if(err)
    {
        return err;
    }   
    
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
    
    m_ChunkBuffer=(unsigned char*)mc_New(MAX_CHUNK_SIZE);
    return err;
}

int mc_WalletTxs::UpdateMode(uint32_t mode)
{
    m_Mode |= mode;
    if(m_Database)
    {
        return m_Database->UpdateMode(mode);
    }
    
    return MC_ERR_INTERNAL_ERROR;
}
    


int mc_WalletTxs::SetMode(uint32_t mode, uint32_t mask)
{
    m_Mode &= ~mask;
    m_Mode |= mode;
    
    return MC_ERR_NOERROR;
}

int mc_WalletTxs::Destroy()
{
    if(m_ChunkCollector)
    {
        m_ChunkCollector->Commit();
    }
    
    if(m_ChunkDB)
    {
        m_ChunkDB->Commit(-1);
    }
    
    if(m_Database)
    {
        delete m_Database;
    }

    if(m_ChunkCollector)
    {
//        m_ChunkCollector->Commit();
        delete m_ChunkCollector;
    }    
    
    if(m_ChunkDB)
    {
//        m_ChunkDB->Commit(-1);
        delete m_ChunkDB;
    }

    if(m_ChunkBuffer)
    {
        mc_Delete(m_ChunkBuffer);
    }
    Zero();
    return MC_ERR_NOERROR;    
    
}

bool mc_WalletTxs::WRPFindEntity(mc_TxEntityStat *entity)
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
    
    int use_read=m_Database->WRPUsed();
    
    if(use_read == 0)
    {
        m_Database->Lock(0,0);
    }
    
    row=m_Database->FindEntity(NULL,entity);
    
    if(use_read == 0)
    {
        m_Database->UnLock();
    }
    
    return (row >= 0) ? true : false;
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
    WRPWriteLock();
    err=m_Database->AddEntity(entity,flags);    
    WRPWriteUnLock();
    m_Database->UnLock();
    
    if(err == MC_ERR_NOERROR)
    {
        if(mc_gState->m_Features->Chunks())
        {
            switch(entity->m_EntityType & MC_TET_TYPE_MASK)
            {
                case MC_TET_STREAM:
                    err=m_ChunkDB->AddEntity(entity,flags);
                    break;
            }
        }
    }
    
    return err;
}

int mc_WalletTxs::SaveEntityFlag(mc_TxEntity *entity,uint32_t flag,int set_flag)
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
    WRPWriteLock();
    err=m_Database->SaveEntityFlag(entity,flag,set_flag);
    WRPWriteUnLock();
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
//    LogPrintf("Lock %ld\n",__US_ThreadID());
    if(m_Database)
    {
        m_Database->Lock(0,0);
    }
}

void mc_WalletTxs::UnLock()
{
//    LogPrintf("UnLock %ld\n",__US_ThreadID());
    if(m_Database)
    {
        m_Database->UnLock();
    }    
}

void mc_WalletTxs::WRPReadLock()
{
//    LogPrintf("WRPLock %ld\n",__US_ThreadID());
    if(m_Database)
    {
        m_Database->WRPReadLock();
        SetRPCWRPReadLockFlag(1);
    }
}

void mc_WalletTxs::WRPWriteLock()
{
//    LogPrintf("WRPLock %ld\n",__US_ThreadID());
    if(m_Database)
    {
        m_Database->WRPWriteLock(1);
    }
}

void mc_WalletTxs::WRPReadUnLock()
{
//    LogPrintf("WRPUnLock %ld\n",__US_ThreadID());
    if(m_Database)
    {
        SetRPCWRPReadLockFlag(0);
        m_Database->WRPReadUnLock();
    }    
}

void mc_WalletTxs::WRPWriteUnLock()
{
//    LogPrintf("WRPUnLock %ld\n",__US_ThreadID());
    if(m_Database)
    {
        m_Database->WRPWriteUnLock(1);
    }    
}

int mc_WalletTxs::WRPSync(int for_block)
{
    if(m_Database)
    {
        if(for_block)
        {
            if(fDebug)LogPrint("mcwrp","mcwrp: Mempool synchronization for block started\n");
        }
        int ret=m_Database->WRPSync(for_block);
        if(for_block)
        {
            if(fDebug)LogPrint("mcwrp","mcwrp: Mempool synchronization for block completed\n");
        }
        return ret;
    }        
    
    return MC_ERR_NOERROR;
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
    if(mc_gState->m_Features->Chunks())
    {
        if(imp->m_ImportID == 0)
        {
            if(err == MC_ERR_NOERROR)
            {
                err=m_ChunkDB->Commit(imp->m_Block+1);
            }
            if(m_ChunkCollector)
            {
                err=m_ChunkCollector->Commit();
            }                
        }
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
                    if((entrow->m_Flags & MC_TFL_IS_EXTENSION) == 0)            // Extension rows are ignored, tx will appear in normal row and decrement subkey several times                     
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
                                                            if(prevtxdef.m_Flags & MC_TFL_IS_LICENSE_TOKEN) // License token transfer
                                                            {
                                                                txout.m_Flags |= MC_TFL_IS_LICENSE_TOKEN;
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
                            pEF->LIC_VerifyUpdateCoin(block,&(txouts[i]),true);
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

int mc_WalletTxs::WRPGetRow(
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
    int use_read=m_Database->WRPUsed();
    
    if(use_read == 0)
    {
        m_Database->Lock(0,0);
    }
    err=m_Database->WRPGetRow(erow);
    if(use_read == 0)
    {
        m_Database->UnLock();
    }
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

int mc_WalletTxs::WRPGetList(mc_TxEntity *entity,int generation,int from,int count,mc_Buffer *txs)
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

    int use_read=m_Database->WRPUsed();
    
    if(use_read == 0)
    {
        m_Database->Lock(0,0);
    }
    err=m_Database->WRPGetList(entity,generation,from,count,txs);
    if(use_read == 0)
    {
        m_Database->UnLock();
    }
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

int mc_WalletTxs::WRPGetBlockItemIndex(mc_TxEntity *entity, int block)
{
    int res;
    int use_read=m_Database->WRPUsed();
    
    if(use_read == 0)
    {
        m_Database->Lock(0,0);
    }
    res=m_Database->WRPGetBlockItemIndex(NULL,entity,block);
    if(use_read == 0)
    {
        m_Database->UnLock();
    }
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

int mc_WalletTxs::WRPGetListSize(mc_TxEntity *entity,int *confirmed)
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
    
    int use_read=m_Database->WRPUsed();
    
    if(use_read == 0)
    {
        m_Database->Lock(0,0);
    }
    res=m_Database->WRPGetListSize(entity,confirmed);
    if(use_read == 0)
    {
        m_Database->UnLock();
    }
    return res;                
}

int mc_WalletTxs::WRPGetLastItem(mc_TxEntity *entity,int generation,mc_TxEntityRow *erow)
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
    
    int use_read=m_Database->WRPUsed();
    
    if(use_read == 0)
    {
        m_Database->Lock(0,0);
    }
    res=m_Database->WRPGetLastItem(entity,generation,erow);
    if(use_read == 0)
    {
        m_Database->UnLock();
    }
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

int mc_WalletTxs::WRPGetListSize(mc_TxEntity *entity,int generation,int *confirmed)
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
    int use_read=m_Database->WRPUsed();
    
    if(use_read == 0)
    {
        m_Database->Lock(0,0);
    }
    res=m_Database->WRPGetListSize(entity,generation,confirmed);
    if(use_read == 0)
    {
        m_Database->UnLock();
    }
    return res;                
}

int mc_WalletTxs::Unsubscribe(mc_Buffer* lpEntities,bool purge)
{
    int err,j;
    if((m_Mode & MC_WMD_TXS) == 0)
    {
        return MC_ERR_NOT_SUPPORTED;
    }    
    if(m_Database == NULL)
    {
        return MC_ERR_INTERNAL_ERROR;
    }
    m_Database->Lock(1,0);   
    WRPWriteLock();
    err=m_Database->Unsubscribe(lpEntities);
    WRPWriteUnLock();
    if(err==MC_ERR_NOERROR)
    {
        if(mc_gState->m_Features->Chunks())
        {
            if(purge)
            {
                if(m_ChunkDB)
                {
                    for(j=0;j<lpEntities->GetCount();j++)                                       
                    {
                        m_ChunkDB->RemoveEntity((mc_TxEntity*)lpEntities->GetRow(j),NULL,NULL);
                    }
                }
            }
        }
    }
    if(fDebug)LogPrint("wallet","wtxs: Unsubscribed from %d entities\n",lpEntities->GetCount());
    m_Database->UnLock();
    
    if(m_ChunkCollector)
    {
        m_ChunkCollector->Unsubscribe(lpEntities);
    }
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

int mc_WalletTxs::CompleteImport(mc_TxImport *import,uint32_t flags)
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
    
    err=m_Database->CompleteImport(import,flags);
    
                                                                                
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
//    map<uint160,int> subkey_count_map;
    map<uint160,mc_TxEntityRowExtension> subkey_extension_map;
    
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
    bool fLicenseTokenTransfer;
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
    
    AddExplorerTx(imp,tx,block);
            
    import_pos=imp-m_Database->m_Imports;
    hash=tx.GetHash();
    
    if(fDebug)LogPrint("dwtxs02","dwtxs02:    --> %s\n",hash.ToString().c_str());
    
    fullTx=&tx;
    storedTx=&tx;
    
    fFound=false;
    txdef.Zero();
    
    m_Database->SetTxCache((unsigned char*)&hash);
    if(m_Database->m_TxCachedFlags == MC_TCF_FOUND)
    {
        fFound=true;
        memcpy(&txdef,&(m_Database->m_TxCachedDef),sizeof(mc_TxDefRow));
    }
/*    
    if(m_Database->GetTx(&txdef,(unsigned char*)&hash) == MC_ERR_NOERROR)
    {
        fFound=true;        
    }
*/    
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
    fLicenseTokenTransfer=false;
    
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
                mc_gState->m_TmpScript->GetAssetQuantities(mc_gState->m_TmpAssetsOut,MC_SCR_ASSET_SCRIPT_TYPE_TRANSFER | MC_SCR_ASSET_SCRIPT_TYPE_FOLLOWON | MC_SCR_ASSET_SCRIPT_TYPE_TOKEN);
                if(mc_gState->m_TmpScript->GetAssetGenesis(&quantity) == 0)
                {
                    fNewAsset=true;                    
                }
            }            
            
            if(fNewAsset)
            {
                if(IsLicenseTokenIssuance(mc_gState->m_TmpScript,hash))
                {
                    utxo.m_Flags |= MC_TFL_IS_LICENSE_TOKEN;
                    fNewAsset=false;
                }
            }
            if(IsLicenseTokenTransfer(mc_gState->m_TmpScript,mc_gState->m_TmpAssetsOut))
            {
                fLicenseTokenTransfer=true;
                utxo.m_Flags |= MC_TFL_IS_LICENSE_TOKEN;
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
            unsigned char *chunk_hashes;
            unsigned char *chunk_found;
            int chunk_count,chunk_err;
            int chunk_size,chunk_shift;
            size_t chunk_bytes;
            uint32_t salt_size;
            uint32_t format;
            uint32_t chunk_flags;
            chunk_flags=0;
            
            mc_gState->m_TmpScript->ExtractAndDeleteDataFormat(&format,&chunk_hashes,&chunk_count,NULL,&salt_size,0);
            if(chunk_count == 1)
            {
                chunk_flags=MC_CFL_SINGLE_CHUNK + (format & MC_CFL_FORMAT_MASK);
            }
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
                    
                    bool passed_filters=true;

                    if(imp->FindEntity(&entity) >= 0)    
                    {
                        if( (chunk_hashes != NULL) && 
                            (pEF->STR_NoRetrieve(&entity) == 0) )
                        {
                            mc_ChunkDBRow chunk_def;
                            mc_TxEntity chunk_entity;
                            chunk_entity.Zero();
                            memcpy(chunk_entity.m_EntityID,short_txid,MC_AST_SHORT_TXID_SIZE);
                            chunk_entity.m_EntityType=MC_TET_STREAM;            
                            for(int chunk=0;chunk<chunk_count;chunk++)
                            {
                                chunk_size=(int)mc_GetVarInt(chunk_hashes,MC_CDB_CHUNK_HASH_SIZE+16,-1,&chunk_shift);
                                chunk_hashes+=chunk_shift;
                                if(m_ChunkDB->GetChunkDef(&chunk_def,chunk_hashes,&chunk_entity,(unsigned char*)&hash,i) != MC_ERR_NOERROR)
                                {
                                    if(m_ChunkDB->GetChunkDef(&chunk_def,chunk_hashes,NULL,NULL,-1) == MC_ERR_NOERROR)
                                    {
                                        unsigned char salt[MC_CDB_CHUNK_SALT_SIZE];
                                        uint32_t salt_size;
                                        chunk_found=m_ChunkDB->GetChunk(&chunk_def,0,-1,&chunk_bytes,salt,&salt_size);
                                        if(chunk_found)
                                        {
                                            memcpy(m_ChunkBuffer,chunk_found,chunk_size);
                                            chunk_err=m_ChunkDB->AddChunk(chunk_hashes,&chunk_entity,(unsigned char*)&hash,i,m_ChunkBuffer,NULL,salt,chunk_size,0,salt_size,chunk_flags);
                                            if(chunk_err)
                                            {
                                                err=chunk_err;
                                                goto exitlbl;
                                            }
                                        }
                                        else
                                        {
                                            err=MC_ERR_CORRUPTED;
                                            goto exitlbl;      
                                        }
                                    }
                                    else
                                    {
                                        bool insert_it=false;
                                        mc_EntityDetails entity_details;

                                        if(mc_gState->m_Assets->FindEntityByShortTxID(&entity_details,short_txid))
                                        {
                                            if(entity_details.AnyoneCanRead())
                                            {
                                                insert_it=true;                                                
                                            }
                                            else
                                            {
                                                if(pEF->WLT_FindReadPermissionedAddress(&entity_details).IsValid())
                                                {
                                                    insert_it=true;                                                                                                    
                                                }
                                            }
                                        }
                                        if(insert_it)
                                        {
                                            m_ChunkCollector->InsertChunk(chunk_hashes,&chunk_entity,(unsigned char*)&hash,i,chunk_size,salt_size,chunk_flags);
                                        }
                                        // Feeding async chunk retriever here
                                    }
                                }
                                else
                                {
                                    m_ChunkDB->RestoreChunkIfNeeded(&chunk_def);
                                }
                                
                                chunk_hashes+=MC_CDB_CHUNK_HASH_SIZE;
                            }
                        }
/*                        
                        else
                        {
                            mc_MultiChainFilter* lpFilter;
                            int applied=0;
                            string filter_error;
                            int filter_block=-1;
                            int filter_offset=0;
                            if(block >= 0)
                            {
                                filter_block=block;
                                filter_offset=block_pos->nTxOffset;
                            }
                            else
                            {
                                if( imp->m_ImportID > 0 )
                                {
                                    filter_offset=-1;                                    
                                }
                            }
                            if(pMultiChainFilterEngine->RunStreamFilters(tx,i,entity.m_EntityID,filter_block, filter_offset, 
                                    filter_error,&lpFilter,&applied) != MC_ERR_NOERROR)
                            {
                                if(fDebug)LogPrint("mchn","mchn: Stream items rejected (%s): %s\n","Error while running filters",EncodeHexTx(tx));
                                passed_filters=false;
                            }
                            else
                            {
                                if(filter_error.size())
                                {
                                    if(fDebug)LogPrint("mchn","mchn: Rejecting filter: %s\n",lpFilter->m_FilterCaption.c_str());
                                    if(fDebug)LogPrint("mchn","mchn: Stream items rejected (%s): %s\n",filter_error.c_str(),EncodeHexTx(tx));                                
                                    passed_filters=false;
                                }
                            }                    
                        }
*/                            
//                        if(imp->m_TmpEntities->Seek(&entity) < 0)
                        if(passed_filters)
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
/*
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
 */ 
                                map<uint160,mc_TxEntityRowExtension>::iterator it = subkey_extension_map.find(stream_subkey_hash160);
                                if (it == subkey_extension_map.end())
                                {
                                    mc_TxEntityRowExtension new_extension;
                                    new_extension.Zero();
                                    new_extension.m_Output=i;
                                    subkey_extension_map.insert(make_pair(stream_subkey_hash160,new_extension));
                                    it = subkey_extension_map.find(stream_subkey_hash160);
                                }                                
                                else
                                {
                                    it->second.m_Count += 1;
                                    it->second.m_Output=i;
                                }
                                
                                entity.m_EntityType=MC_TET_STREAM_KEY | MC_TET_CHAINPOS;
                                subkey_entity.Zero();
                                memcpy(subkey_entity.m_EntityID,&stream_subkey_hash160,MC_TDB_ENTITY_ID_SIZE);
                                subkey_entity.m_EntityType=MC_TET_SUBKEY_STREAM_KEY | MC_TET_CHAINPOS;
                                err= m_Database->IncrementSubKey(imp,&entity,&subkey_entity,(unsigned char*)&subkey_hash160,(unsigned char*)&hash,&(it->second),block,0,fFound ? 0 : 1);
                                if(err)
                                {
                                    goto exitlbl;
                                }

                                entity.m_EntityType=MC_TET_STREAM_KEY | MC_TET_TIMERECEIVED;
                                subkey_entity.m_EntityType=MC_TET_SUBKEY_STREAM_KEY | MC_TET_TIMERECEIVED;
                                err= m_Database->IncrementSubKey(imp,&entity,&subkey_entity,(unsigned char*)&subkey_hash160,(unsigned char*)&hash,&(it->second),block,0,fFound ? 0 : 1);
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
/*                            
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
 */ 
                                            map<uint160,mc_TxEntityRowExtension>::iterator it = subkey_extension_map.find(stream_subkey_hash160);
                                            if (it == subkey_extension_map.end())
                                            {
                                                mc_TxEntityRowExtension new_extension;
                                                new_extension.Zero();
                                                new_extension.m_Output=i;
                                                subkey_extension_map.insert(make_pair(stream_subkey_hash160,new_extension));
                                                it = subkey_extension_map.find(stream_subkey_hash160);
                                            }                                
                                            else
                                            {
                                                it->second.m_Count += 1;
                                                it->second.m_Output=i;
                                            }
                                            
                                            entity.m_EntityType=MC_TET_STREAM_PUBLISHER | MC_TET_CHAINPOS;
                                            subkey_entity.Zero();
                                            memcpy(subkey_entity.m_EntityID,&stream_subkey_hash160,MC_TDB_ENTITY_ID_SIZE);
                                            subkey_entity.m_EntityType=MC_TET_SUBKEY_STREAM_PUBLISHER | MC_TET_CHAINPOS;
                                            err= m_Database->IncrementSubKey(imp,&entity,&subkey_entity,(unsigned char*)&subkey_hash160,(unsigned char*)&hash,&(it->second),block,0,fFound ? 0 : 1);
                                            if(err)
                                            {
                                                goto exitlbl;
                                            }
                            
                                            entity.m_EntityType=MC_TET_STREAM_PUBLISHER | MC_TET_TIMERECEIVED;
                                            subkey_entity.m_EntityType=MC_TET_SUBKEY_STREAM_PUBLISHER | MC_TET_TIMERECEIVED;
                                            err= m_Database->IncrementSubKey(imp,&entity,&subkey_entity,(unsigned char*)&subkey_hash160,(unsigned char*)&hash,&(it->second),block,0,fFound ? 0 : 1);
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
            if(mc_gState->m_TmpScript->GetNumElements() == 2) 
            {
                uint32_t new_entity_type;
                mc_gState->m_TmpScript->SetElement(0);
                                                                            // Should be spkn
                if(mc_gState->m_TmpScript->GetNewEntityType(&new_entity_type) == 0)    // New entity element
                {
                    if(new_entity_type == MC_ENT_TYPE_ASSET)
                    {
                        fNewAsset=true;
                    }
                    if(new_entity_type == MC_ENT_TYPE_STREAM)
                    {
                        if(m_Mode & MC_WMD_AUTOSUBSCRIBE_STREAMS)
                        {
                            entity.Zero();
                            memcpy(entity.m_EntityID,(unsigned char*)&hash+MC_AST_SHORT_TXID_OFFSET,MC_AST_SHORT_TXID_SIZE);
                            entity.m_EntityType=MC_TET_STREAM | MC_TET_CHAINPOS;
                            if(imp->FindEntity(&entity) < 0)    
                            {
                                if((imp->m_ImportID == 0) || fRescan)
                                {
                                    if(mc_AutosubscribeWalletMode(GetArg("-autosubscribe","none"),true) & MC_WMD_AUTOSUBSCRIBE_STREAMS)
                                    {
                                        fNewStream=true;
                                    }
                                }
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
        entity.m_EntityType=MC_TET_ASSET | MC_TET_CHAINPOS;
        memcpy(entity.m_EntityID,(unsigned char*)&hash+MC_AST_SHORT_TXID_OFFSET,MC_AST_SHORT_TXID_SIZE);
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
                    if((imp->m_ImportID == 0) || fRescan)
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
        mc_EntityDetails asset_details;        
        ptrOut=mc_gState->m_TmpAssetsOut->GetRow(i);
        entity.Zero();
        memcpy(entity.m_EntityID,ptrOut+MC_AST_SHORT_TXID_OFFSET,MC_AST_SHORT_TXID_SIZE);
        entity.m_EntityType=MC_TET_ASSET | MC_TET_CHAINPOS;
        if(mc_gState->m_Assets->FindEntityByShortTxID(&asset_details,ptrOut+MC_AST_SHORT_TXID_OFFSET))
        {
            if(asset_details.GetEntityType() == MC_ENT_TYPE_TOKEN)
            {
                memcpy(entity.m_EntityID,asset_details.GetParentTxID()+MC_AST_SHORT_TXID_OFFSET,MC_AST_SHORT_TXID_SIZE);                
            }
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
    if(fLicenseTokenTransfer)                                                   
    {
        flags |= MC_TFL_IS_LICENSE_TOKEN;                                       // Needed for rollback, license issuance is not pure license tx
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
            pEF->LIC_VerifyUpdateCoin(block,&(txoutsIn[i]),false);
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
                pEF->LIC_VerifyUpdateCoin(block,&(txoutsOut[i]),true);
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

            entity.m_EntityType=MC_TET_STREAM | MC_TET_CHAINPOS;
            err=pEF->STR_CreateAutoSubscription(&entity);
            if(err == MC_ERR_FOUND)
            {
                err=MC_ERR_NOERROR;
            }
            if(err != MC_ERR_NOERROR)
            {
                LogPrintf("wtxs: Create Enterprise Subscription  Error: %d\n",err);                        
            }
            if(mc_gState->m_Features->Chunks())
            {
                entity.m_EntityType=MC_TET_STREAM;
                m_ChunkDB->AddEntity(&entity,0);
            }            
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

    if(fDebug)LogPrint("dwtxs02","dwtxs02:    <-- %s\n",hash.ToString().c_str());
    
    return err;
}

int mc_EntityTypeToExplorerCode(int entity_type)
{
    if(entity_type <= MC_ENT_TYPE_STREAM)
    {
        return entity_type;
    }
    if(entity_type <= MC_ENT_TYPE_STREAM_MAX)
    {
        return 0x10+entity_type;
    }
    if(entity_type <= 0x10 + MC_ENT_TYPE_STREAM_MAX)
    {
        return entity_type-MC_ENT_TYPE_STREAM_MAX+MC_ENT_TYPE_STREAM;
    }
    return entity_type;
}

int mc_EntityExplorerCodeToType(int entity_explorer_code)
{
    if(entity_explorer_code <= MC_ENT_TYPE_STREAM)
    {
        return entity_explorer_code;
    }
    if(entity_explorer_code <= 0x10+MC_ENT_TYPE_STREAM)
    {
        return entity_explorer_code-MC_ENT_TYPE_STREAM+MC_ENT_TYPE_STREAM_MAX;
    }    
    if(entity_explorer_code <= 0x10 + MC_ENT_TYPE_STREAM_MAX)
    {
        return entity_explorer_code-0x10;
    }    
    return entity_explorer_code;
}

uint64_t mc_GetExplorerTxOutputDetails(int rpc_slot,
                                 const CTransaction& tx,
                                 std::vector< std::map<uint160,uint32_t> >& OutputAddresses,
                                 std::vector< std::map<uint160,int64_t> >& OutputAssetQuantities,
                                 std::vector< uint160 >&OutputStreams,
//                                 std::vector<uint64_t>& InputScriptTags,
                                 std::vector<uint64_t>& OutputScriptTags)
{
    uint64_t tx_tag=MC_MTX_TAG_NONE;
    
    mc_Script *tmpscript;
    tmpscript=mc_gState->m_TmpScript;
    if(rpc_slot >= 0)
    {
        tmpscript=mc_gState->m_TmpRPCBuffers[rpc_slot]->m_RpcScript1;
    }   
    
//    InputScriptTags.resize(tx.vin.size());
    OutputAddresses.resize(tx.vout.size());
    OutputScriptTags.resize(tx.vout.size());
    OutputAssetQuantities.resize(tx.vout.size());
    OutputStreams.resize(tx.vout.size());
    
    uint256 hash=tx.GetHash();
    int item_count=0;
    
 /*   
    for (int j = 0; j < (int)tx.vin.size(); ++j)
    {
        if(tx.IsCoinBase())
        {
            InputScriptTags[j]=MC_MTX_TAG_COINBASE;
        }
        else
        {
            InputScriptTags[j]=MC_MTX_TAG_NONE;
            int op_addr_offset,op_addr_size,is_redeem_script,sighash_type;
            const unsigned char *ptr;

            const CScript& script2 = tx.vin[j].scriptSig;        
            CScript::const_iterator pc2 = script2.begin();

            
            ptr=mc_ExtractAddressFromInputScript((unsigned char*)(&pc2[0]),(int)(script2.end()-pc2),&op_addr_offset,&op_addr_size,&is_redeem_script,&sighash_type,0);                                
            if(ptr)
            {
                if(is_redeem_script)
                {
                    InputScriptTags[j] |= MC_MTX_TAG_P2SH;
                }
            }
            else
            {
                InputScriptTags[j] |= MC_MTX_TAG_NO_KEY_SCRIPT_ID;              // Either non-standard or P2PK or bare multisig
            }
        }
        tx_tag |= InputScriptTags[j];
    }    
*/
    for(int i=0;i<(int)tx.vout.size();i++)                                          // Checking outputs
    {        
        const CTxOut txout=tx.vout[i];
        const CScript& script1 = txout.scriptPubKey;        
        CScript::const_iterator pc1 = script1.begin();

        OutputScriptTags[i]=MC_MTX_TAG_NONE;
        OutputStreams[i]=0;
        txnouttype typeRet;
        int nRequiredRet;
        int err;
        uint160 subkey_hash160;
        std::vector<CTxDestination> addressRets;
        int64_t quantity,issue_quantity;
        std::map<uint160,int64_t> assets;
        issue_quantity=0;
        mc_EntityDetails entity;
        uint32_t type,from,to,timestamp;
        unsigned char short_txid[MC_AST_SHORT_TXID_SIZE];
        
        tmpscript->Clear();
        tmpscript->SetScript((unsigned char*)(&pc1[0]),(size_t)(script1.end()-pc1),MC_SCR_TYPE_SCRIPTPUBKEY);
        if(tmpscript->IsOpReturnScript() == 0)
        {                
            if(txout.nValue)
            {
                assets.insert(make_pair(0,txout.nValue));   
                if(!tx.IsCoinBase())
                {
                    OutputScriptTags[i] |= MC_MTX_TAG_NATIVE_TRANSFER;                                    
                }
                else
                {
                    OutputScriptTags[i] |= MC_MTX_TAG_COINBASE;                                                        
                }
            }
            ExtractDestinations(script1,typeRet,addressRets,nRequiredRet);
            if(addressRets.size() == 1)
            {
                BOOST_FOREACH(const CTxDestination& dest, addressRets)
                {
                    uint32_t publisher_flags;
                    entity.Zero();
                    const CKeyID *lpKeyID=boost::get<CKeyID> (&dest);
                    const CScriptID *lpScriptID=boost::get<CScriptID> (&dest);
                    publisher_flags=MC_SFL_SUBKEY | MC_SFL_IS_ADDRESS;
                    subkey_hash160=0;

                    if( (lpKeyID !=NULL) && (typeRet == TX_PUBKEYHASH) )        
                    {
                        memcpy(&subkey_hash160,lpKeyID,MC_TDB_ENTITY_ID_SIZE);
                    }
                    if(lpScriptID)
                    {
                        memcpy(&subkey_hash160,lpScriptID,MC_TDB_ENTITY_ID_SIZE);
                        OutputScriptTags[i] |= MC_MTX_TAG_P2SH;
                        publisher_flags |= MC_SFL_IS_SCRIPTHASH;
                    }
                    if(subkey_hash160 != 0)
                    {
                        OutputAddresses[i].insert(make_pair(subkey_hash160,publisher_flags));                    
                    }
                    else
                    {
                        OutputScriptTags[i] |= MC_MTX_TAG_NO_KEY_SCRIPT_ID;
                    }
                }
            }
            else
            {
                OutputScriptTags[i] |= MC_MTX_TAG_NO_KEY_SCRIPT_ID;
            }

            bool prev_element_is_entity=false;
            for (int e = 0; e < tmpscript->GetNumElements(); e++)
            {
                uint160 asset=0;
                tmpscript->SetElement(e);
                mc_gState->m_TmpAssetsOut->Clear();                    
                err=tmpscript->GetAssetQuantities(mc_gState->m_TmpAssetsOut,MC_SCR_ASSET_SCRIPT_TYPE_TRANSFER);
                if(err == MC_ERR_NOERROR)
                {
                    OutputScriptTags[i] |= MC_MTX_TAG_ASSET_TRANSFER;
                }
                err=tmpscript->GetAssetQuantities(mc_gState->m_TmpAssetsOut,MC_SCR_ASSET_SCRIPT_TYPE_FOLLOWON | MC_SCR_ASSET_SCRIPT_TYPE_TOKEN);
                if(err == MC_ERR_NOERROR)
                {
                    OutputScriptTags[i] |= MC_MTX_TAG_ASSET_FOLLOWON;
                }
                int n=mc_gState->m_TmpAssetsOut->GetCount();
                quantity=0;
                if(n)
                {
                    unsigned char *ptrOut;
                    for(int k=0;k<n;k++)
                    {
                        ptrOut=mc_gState->m_TmpAssetsOut->GetRow(k);
                        asset=0;
                        memcpy(&asset,ptrOut+MC_AST_SHORT_TXID_OFFSET,MC_AST_SHORT_TXID_SIZE);                        
                        if(mc_gState->m_Assets->FindEntityByFullRef(&entity,ptrOut))
                        {
                            if(entity.GetEntityType() == MC_ENT_TYPE_TOKEN)
                            {
                                memcpy(&asset,entity.GetParentTxID()+MC_AST_SHORT_TXID_OFFSET,MC_AST_SHORT_TXID_SIZE);                                                        
                            }
                            quantity=mc_GetABQuantity(ptrOut);
                            if(quantity > 0)
                            {
                                if( (n == 1) && (quantity == 1) )
                                {
                                    if(entity.GetEntityType() == MC_ENT_TYPE_LICENSE_TOKEN)
                                    {
                                        OutputScriptTags[i] |= MC_MTX_TAG_LICENSE_TOKEN;
                                    }   
                                }
                                if( (OutputScriptTags[i] & MC_MTX_TAG_LICENSE_TOKEN) == 0 )
                                {
                                    std::map<uint160,int64_t>::iterator it = assets.find(asset);
                                    if(it == assets.end())
                                    {
                                        assets.insert(make_pair(asset,quantity));                        
                                    }
                                    else
                                    {
                                        it->second += quantity;
                                    }                            
                                }
                            }
                        }
                    }                    
                }
                if(tmpscript->GetAssetGenesis(&quantity) == 0)
                {
                    OutputScriptTags[i] |= MC_MTX_TAG_ASSET_GENESIS;
                    issue_quantity+=quantity;
                    asset=0;
                    memcpy(&asset,(unsigned char*)&hash+MC_AST_SHORT_TXID_OFFSET,MC_AST_SHORT_TXID_SIZE);
                    if(quantity > 0)
                    {
                        if(quantity == 1)
                        {
                            if(mc_gState->m_Assets->FindEntityByShortTxID(&entity,(const unsigned char*)&asset))
                            {
                                if(entity.GetEntityType() == MC_ENT_TYPE_LICENSE_TOKEN)
                                {
                                    OutputScriptTags[i] |= MC_MTX_TAG_LICENSE_TOKEN;
                                }                               
                            }
                        }
                        if( (OutputScriptTags[i] & MC_MTX_TAG_LICENSE_TOKEN) == 0 )
                        {
                            std::map<uint160,int64_t>::iterator it = assets.find(asset);
                            if(it == assets.end())
                            {
                                assets.insert(make_pair(asset,quantity));                        
                            }
                            else
                            {
                                it->second += quantity;
                            }                            
                        }                        
                    }
                }        
                if(tmpscript->GetPermission(&type,&from,&to,&timestamp) == 0) // Grant script
                {
                    if( type != MC_PTP_FILTER )
                    {
                        if(prev_element_is_entity)
                        {
                            if(type == MC_PTP_DETAILS)
                            {
                                if(mc_gState->m_Assets->FindEntityByShortTxID(&entity,short_txid))
                                {
                                    OutputScriptTags[i] |= mc_EntityTypeToExplorerCode(entity.GetEntityType()) << MC_MTX_TAG_ENTITY_MASK_SHIFT;
                                }
                                OutputScriptTags[i] |= MC_MTX_TAG_ENTITY_UPDATE;
                            }
                            else
                            {
                                OutputScriptTags[i] |= (from < to) ? MC_MTX_TAG_GRANT_ENTITY : MC_MTX_TAG_REVOKE_ENTITY;
                            }
                        }
                        else
                        {
                            if( type & ( MC_PTP_CREATE | MC_PTP_ISSUE | MC_PTP_ACTIVATE | MC_PTP_MINE | MC_PTP_ADMIN | 
                                         mc_gState->m_Permissions->GetCustomHighPermissionTypes()) )
                            {
                                OutputScriptTags[i] |= (from < to) ? MC_MTX_TAG_GRANT_HIGH : MC_MTX_TAG_REVOKE_HIGH;
                            }                    
                            if( type & ( MC_PTP_CONNECT | MC_PTP_SEND | MC_PTP_RECEIVE | MC_PTP_READ | MC_PTP_WRITE | 
                                         mc_gState->m_Permissions->GetCustomLowPermissionTypes()) )
                            {
                                OutputScriptTags[i] |= (from < to) ? MC_MTX_TAG_GRANT_LOW : MC_MTX_TAG_REVOKE_LOW;                                
                            }                    
                        }
                    }
                    if( type & MC_PTP_FILTER )
                    {
                        OutputScriptTags[i] |= MC_MTX_TAG_FILTER_APPROVAL;
                    }
                }
                if(tmpscript->GetEntity(short_txid) == 0)          
                {
                    prev_element_is_entity=true;
                }                
                else
                {
                    prev_element_is_entity=false;                    
                }
                
                unsigned char *ptr;
                int size;
                if(tmpscript->GetRawData(&ptr,&size) == 0)      
                {
                    OutputScriptTags[i] |= MC_MTX_TAG_INLINE_DATA;
                }                
            }      
            OutputAssetQuantities[i]=assets;
        }
        else
        {
            OutputScriptTags[i] |= MC_MTX_TAG_OP_RETURN;
            bool fDataTypeFound=false;
            int cs_offset,cs_new_offset,cs_size,cs_vin;
            unsigned char *cs_script;

            tmpscript->SetElement(0);
            cs_offset=0;
            if(tmpscript->GetCachedScript(cs_offset,&cs_new_offset,&cs_vin,&cs_script,&cs_size) == 0)
            {
//                OutputScriptTags[i] |= MC_MTX_TAG_CACHED_SCRIPT;
                fDataTypeFound=true;
            }
            
            int entity_update;
            int details_script_size;                                            
            uint32_t new_entity_type;                                           
            unsigned char details_script[MC_ENT_MAX_SCRIPT_SIZE];               

            for (int e = 0; e < tmpscript->GetNumElements(); e++)
            {
                tmpscript->SetElement(e);
                if(tmpscript->GetNewEntityType(&new_entity_type,&entity_update,details_script,&details_script_size) == 0)
                {
                    fDataTypeFound=true;
                    OutputScriptTags[i] |= mc_EntityTypeToExplorerCode(new_entity_type) << MC_MTX_TAG_ENTITY_MASK_SHIFT;
                    if(entity_update)
                    {
                        OutputScriptTags[i] |= MC_MTX_TAG_ENTITY_UPDATE;        
                    }
                    else
                    {
                        OutputScriptTags[i] |= MC_MTX_TAG_ENTITY_CREATE;                        
                    }                    
                }
            }
            if(tmpscript->GetNumElements() >= 3 )        
            {
                unsigned char short_txid[MC_AST_SHORT_TXID_SIZE];
                mc_EntityDetails entity;
                tmpscript->SetElement(0);
                                                                      
                if( (tmpscript->GetEntity(short_txid) == 0) && (mc_gState->m_Assets->FindEntityByShortTxID(&entity,short_txid) != 0) )
                {
                    fDataTypeFound=true;
                    if(entity.GetEntityType() == MC_ENT_TYPE_UPGRADE)                     
                    {
                        OutputScriptTags[i] |= MC_MTX_TAG_UPGRADE_APPROVAL;
                    }
                    if((entity.GetEntityType() >= MC_ENT_TYPE_STREAM) && (entity.GetEntityType() <= MC_ENT_TYPE_STREAM_MAX))
                    {
                        uint160 stream=0;
                        memcpy(&stream,short_txid,MC_AST_SHORT_TXID_SIZE);     
                        OutputStreams[i]=stream;
                        OutputScriptTags[i] |= MC_MTX_TAG_STREAM_ITEM;
                        item_count++;
                        uint32_t format;
                        unsigned char *chunk_hashes;
                        int chunk_count;   
                        int64_t total_chunk_size;
                        tmpscript->ExtractAndDeleteDataFormat(&format,&chunk_hashes,&chunk_count,&total_chunk_size);
                        if(chunk_hashes)                            
                        {
                            OutputScriptTags[i] |= MC_MTX_TAG_OFFCHAIN;
                        }
                    }
                }                               
            }      
            if(!fDataTypeFound)
            {
                OutputScriptTags[i] |= MC_MTX_TAG_RAW_DATA;
            }
        }
        
        if( ((OutputScriptTags[i] & MC_MTX_TAG_ENTITY_MASK) != 0) && 
            ((tx_tag & MC_MTX_TAG_ENTITY_MASK) != 0) &&               
            ((OutputScriptTags[i] & MC_MTX_TAG_ENTITY_MASK) != (tx_tag & MC_MTX_TAG_ENTITY_MASK)) )            
        {
            tx_tag |= MC_MTX_TAG_NO_SINGLE_TX_TAG;
        }
        else
        {
            tx_tag |= OutputScriptTags[i];            
        }        
    }    
    
    if(item_count > 1)
    {
        tx_tag |= MC_MTX_TAG_MULTIPLE_STREAM_ITEMS;
    }
    return tx_tag;
}

uint64_t mc_GetExplorerTxInputDetails(int rpc_slot,
                                 mc_TxImport *import,                   
                                 const CTransaction& tx,
                                 std::vector< std::map<uint160,uint32_t> >& InputAddresses,
                                 std::vector< std::map<uint160,int64_t> >& InputAssetQuantities,
                                 std::vector< int >& InputSigHashTypes,
                                 std::vector<uint64_t>& InputScriptTags)
{
    uint64_t tx_tag=MC_MTX_TAG_NONE;
    mc_TxImport *imp;
    mc_TxEntity entity;
    mc_TxEntity subkey_entity;
    mc_TxEntityStat *stat;
    mc_TxEntityRow erow;
    
    uint256 subkey_hash256;
    uint160 subkey_hash160;
    uint160 balance_subkey_hash160;
    int generation,err;
    
    imp=import;
    if(imp == NULL)
    {
        imp=pwalletTxsMain->m_Database->FindImport(0);
    }
    
    entity.Zero();
    entity.m_EntityType=MC_TET_EXP_TX_KEY | MC_TET_CHAINPOS;

    stat=imp->GetEntity(imp->FindEntity(&entity));
    
    generation=stat->m_Generation;
    
    InputScriptTags.resize(tx.vin.size());
    InputAssetQuantities.resize(tx.vin.size());
    InputAddresses.resize(tx.vin.size());
    InputSigHashTypes.resize(tx.vin.size());
    
    if(!tx.IsCoinBase())
    {
        for (int j = 0; j < (int)tx.vin.size(); ++j)
        {
            InputScriptTags[j] = MC_MTX_TAG_NONE;
            COutPoint txout=tx.vin[j].prevout;
            int op_addr_offset,op_addr_size,is_redeem_script,sighash_type;
            uint32_t publisher_flags;
            const unsigned char *ptr;

            const CScript& script2 = tx.vin[j].scriptSig;        
            CScript::const_iterator pc2 = script2.begin();
            
            ptr=mc_ExtractAddressFromInputScript((unsigned char*)(&pc2[0]),(int)(script2.end()-pc2),&op_addr_offset,&op_addr_size,&is_redeem_script,&sighash_type,0);                                
            if(ptr)
            {
                subkey_hash160=Hash160(ptr+op_addr_offset,ptr+op_addr_offset+op_addr_size);
                publisher_flags=MC_SFL_NONE;
                if(is_redeem_script)
                {
                    publisher_flags |= MC_SFL_IS_SCRIPTHASH;
                    InputScriptTags[j] |= MC_MTX_TAG_P2SH;
                }                
                InputSigHashTypes[j]=sighash_type;
                InputAddresses[j].insert(make_pair(subkey_hash160,publisher_flags));
            }
            else
            {
                InputScriptTags[j] |= MC_MTX_TAG_NO_KEY_SCRIPT_ID;              // Either non-standard or P2PK or bare multisig
            }
            
            InputAssetQuantities[j].clear();
            
            ptr=(unsigned char*)&txout;
            subkey_hash160=Hash160(ptr,ptr+sizeof(COutPoint));
            
            erow.Zero();
            subkey_entity.Zero();
            memcpy(subkey_entity.m_EntityID,&subkey_hash160,MC_TDB_ENTITY_ID_SIZE);
            subkey_entity.m_EntityType=MC_TET_SUBKEY_EXP_TXOUT_ASSETS_KEY | MC_TET_CHAINPOS;
            if(pwalletTxsMain->m_Database->GetLastItem(imp,&subkey_entity,generation,&erow) == MC_ERR_NOERROR)
            {
                if(erow.m_Flags & MC_MTX_TFL_MULTIPLE_TXOUT_ASSETS)
                {
                    string assets_str=pwalletTxsMain->GetSubKey(imp,erow.m_TxId,NULL,&err);
                    if(err)
                    {
                        goto exitlbl;                        
                    }
                    if(assets_str.size() % (sizeof(uint160) + sizeof(int64_t)))
                    {
                        goto exitlbl;                                                
                    }
                    ptr=(unsigned char*)assets_str.c_str();
                    const unsigned char *ptrEnd=ptr+assets_str.size();
                    while(ptr < ptrEnd)
                    {
                        InputAssetQuantities[j].insert(make_pair(*(uint160*)ptr,*(int64_t*)(ptr+sizeof(uint160))));
                        if(*(uint160*)ptr == 0)
                        {
                            InputScriptTags[j] |= MC_MTX_TAG_NATIVE_TRANSFER;                                                        
                        }
                        else
                        {
                            InputScriptTags[j] |= MC_MTX_TAG_ASSET_TRANSFER;                            
                        }
                        ptr+=sizeof(uint160) + sizeof(int64_t);
                    }
                }   
                else
                {
                    InputAssetQuantities[j].insert(make_pair(*(uint160*)(erow.m_TxId),*(int64_t*)(erow.m_TxId+sizeof(uint160))));
                    if(*(uint160*)(erow.m_TxId) == 0)
                    {
                        InputScriptTags[j] |= MC_MTX_TAG_NATIVE_TRANSFER;                                                        
                    }
                    else
                    {
                        InputScriptTags[j] |= MC_MTX_TAG_ASSET_TRANSFER;                            
                    }
                }
            }
            
            tx_tag |= InputScriptTags[j];
        }            
    }
    else
    {
        tx_tag |= MC_MTX_TAG_COINBASE;
    }
    
exitlbl:
                                    
    return tx_tag;
}

int mc_WalletTxs::AddExplorerEntities(mc_Buffer *lpEntities)
{
    if( (m_Mode & MC_WMD_EXPLORER_MASK) == 0 )
    {
        return MC_ERR_NOERROR;
    }
    
    mc_TxEntity entity;
    entity.Zero();
    entity.m_EntityType=MC_TET_EXP_TX | MC_TET_CHAINPOS;
    if(lpEntities) lpEntities->Add(&entity,NULL); else pwalletTxsMain->AddEntity(&entity,0);
    entity.m_EntityType=MC_TET_EXP_TX_KEY | MC_TET_CHAINPOS;
    if(lpEntities) lpEntities->Add(&entity,NULL); else pwalletTxsMain->AddEntity(&entity,0);
    entity.m_EntityType=MC_TET_EXP_TX_PUBLISHER | MC_TET_CHAINPOS;
    if(lpEntities) lpEntities->Add(&entity,NULL); else pwalletTxsMain->AddEntity(&entity,0);
    entity.m_EntityType=MC_TET_EXP_REDEEM | MC_TET_CHAINPOS;
    if(lpEntities) lpEntities->Add(&entity,NULL); else pwalletTxsMain->AddEntity(&entity,0);
    entity.m_EntityType=MC_TET_EXP_REDEEM_KEY | MC_TET_CHAINPOS;
    if(lpEntities) lpEntities->Add(&entity,NULL); else pwalletTxsMain->AddEntity(&entity,0);
    entity.m_EntityType=MC_TET_EXP_TXOUT_ASSETS | MC_TET_CHAINPOS;
    if(lpEntities) lpEntities->Add(&entity,NULL); else pwalletTxsMain->AddEntity(&entity,0);
    entity.m_EntityType=MC_TET_EXP_TXOUT_ASSETS_KEY | MC_TET_CHAINPOS;
    if(lpEntities) lpEntities->Add(&entity,NULL); else pwalletTxsMain->AddEntity(&entity,0);
    entity.m_EntityType=MC_TET_EXP_BALANCE_DETAILS | MC_TET_CHAINPOS;
    if(lpEntities) lpEntities->Add(&entity,NULL); else pwalletTxsMain->AddEntity(&entity,0);
    entity.m_EntityType=MC_TET_EXP_BALANCE_DETAILS_KEY | MC_TET_CHAINPOS;
    if(lpEntities) lpEntities->Add(&entity,NULL); else pwalletTxsMain->AddEntity(&entity,0);
    
    entity.m_EntityType=MC_TET_EXP_ADDRESS_ASSETS | MC_TET_CHAINPOS;
    if(lpEntities) lpEntities->Add(&entity,NULL); else pwalletTxsMain->AddEntity(&entity,0);
    entity.m_EntityType=MC_TET_EXP_ADDRESS_ASSETS_KEY | MC_TET_CHAINPOS;
    if(lpEntities) lpEntities->Add(&entity,NULL); else pwalletTxsMain->AddEntity(&entity,0);
    entity.m_EntityType=MC_TET_EXP_ASSET_ADDRESSES | MC_TET_CHAINPOS;
    if(lpEntities) lpEntities->Add(&entity,NULL); else pwalletTxsMain->AddEntity(&entity,0);
    entity.m_EntityType=MC_TET_EXP_ASSET_ADDRESSES_KEY | MC_TET_CHAINPOS;
    if(lpEntities) lpEntities->Add(&entity,NULL); else pwalletTxsMain->AddEntity(&entity,0);

    entity.m_EntityType=MC_TET_EXP_ADDRESS_STREAMS_PAIRS | MC_TET_CHAINPOS;
    if(lpEntities) lpEntities->Add(&entity,NULL); else pwalletTxsMain->AddEntity(&entity,0);
    entity.m_EntityType=MC_TET_EXP_ADDRESS_STREAMS_KEY | MC_TET_CHAINPOS;
    if(lpEntities) lpEntities->Add(&entity,NULL); else pwalletTxsMain->AddEntity(&entity,0);
    
    
    return MC_ERR_NOERROR;
}

uint32_t mc_CheckExplorerAssetTransfers(
                                 std::vector< std::map<uint160,uint32_t> >& InputAddresses,
                                 std::vector< std::map<uint160,int64_t> >& InputAssetQuantities,
                                 std::vector< std::map<uint160,uint32_t> >& OutputAddresses,
                                 std::vector< std::map<uint160,int64_t> >& OutputAssetQuantities,
                                 std::map<uint160,mc_TxAddressAssetQuantity>& AddressAssetQuantities)
{
    uint32_t tx_tag=MC_MTX_TAG_NONE;
    uint160 balance_subkey_hash160;
    int asset_count=0;
    bool same_address=true;
    int output_count=0;
    uint160 first_address=0;
    int inputs_outputs_shift=0;
    
    for (int j = 0; j < (int)InputAddresses.size(); ++j)
    {
        if(InputAddresses[j].size())
        {
            inputs_outputs_shift++;
            if(first_address == 0)
            {
                first_address=InputAddresses[j].begin()->first;
            }
            else
            {
                if(first_address != InputAddresses[j].begin()->first)
                {
                    same_address=false;
                }
            }
        }
        for (map<uint160,int64_t>::const_iterator it_asset = InputAssetQuantities[j].begin(); it_asset != InputAssetQuantities[j].end(); ++it_asset) 
        {
            for (map<uint160,uint32_t>::const_iterator it_address = InputAddresses[j].begin(); it_address != InputAddresses[j].end(); ++it_address) 
            {
                mc_GetCompoundHash160(&balance_subkey_hash160,&(it_address->first),&(it_asset->first));
                std::map<uint160,mc_TxAddressAssetQuantity>::iterator it_balance=AddressAssetQuantities.find(balance_subkey_hash160);
                if(it_balance != AddressAssetQuantities.end())
                {
                    it_balance->second.m_Amount -= it_asset->second;
                }
                else
                {
                    mc_TxAddressAssetQuantity aa_qty;
                    aa_qty.m_Address=it_address->first;
                    aa_qty.m_Asset=it_asset->first;
                    aa_qty.m_Amount=-it_asset->second;
                    AddressAssetQuantities.insert(make_pair(balance_subkey_hash160,aa_qty));
                }
            }                            
        }            
    }
    
    for(int i=0;i<(int)OutputAddresses.size();i++)       
    {        
        if(OutputAddresses[i].size())
        {
            inputs_outputs_shift--;
            output_count++;
            if(first_address != OutputAddresses[i].begin()->first)
            {
                same_address=false;
            }
        }
        for (map<uint160,int64_t>::const_iterator it_asset = OutputAssetQuantities[i].begin(); it_asset != OutputAssetQuantities[i].end(); ++it_asset) 
        {
            for (map<uint160,uint32_t>::const_iterator it_address = OutputAddresses[i].begin(); it_address != OutputAddresses[i].end(); ++it_address) 
            {
                mc_GetCompoundHash160(&balance_subkey_hash160,&(it_address->first),&(it_asset->first));
                std::map<uint160,mc_TxAddressAssetQuantity>::iterator it_balance=AddressAssetQuantities.find(balance_subkey_hash160);
                if(it_balance != AddressAssetQuantities.end())
                {
                    it_balance->second.m_Amount += it_asset->second;
                }
                else
                {
                    mc_TxAddressAssetQuantity aa_qty;
                    aa_qty.m_Address=it_address->first;
                    aa_qty.m_Asset=it_asset->first;
                    aa_qty.m_Amount=it_asset->second;
                    AddressAssetQuantities.insert(make_pair(balance_subkey_hash160,aa_qty));
                }
            }                            
        }            
    }    
    for (map<uint160,mc_TxAddressAssetQuantity>::const_iterator it_balance = AddressAssetQuantities.begin(); it_balance != AddressAssetQuantities.end(); ++it_balance) 
    {
        if(it_balance->second.m_Amount > 0)        
        {
            asset_count++;
            if(it_balance->second.m_Asset != 0)
            {
                tx_tag |= MC_MTX_TAG_ASSET_TRANSFER;
            }
            else
            {
                if(first_address != 0)
                {
                    tx_tag |= MC_MTX_TAG_NATIVE_TRANSFER;                
                }
            }
        }
    }
    
    if(asset_count > 1)
    {
        tx_tag |= MC_MTX_TAG_MULTIPLE_ASSETS;
    }
    
    if(same_address && (inputs_outputs_shift > 0) && (output_count > 0))
    {        
        tx_tag |= MC_MTX_TAG_COMBINE;        
    }
    
    return tx_tag;
}

uint256 mc_ExplorerTxIDHashMap(uint256 hash_in)
{
    unsigned char t;
    unsigned char temp[32];
    uint256 hash_out=hash_in;
    memcpy(temp,&hash_in,32);
    for(int i=1;i<32;i++)
    {
        if(temp[i] != temp[0])
        {
            t=temp[i];
            temp[i]=temp[0];
            temp[0]=t;
            memcpy(&hash_out,temp,32);
            return hash_out;
        }
    }    
    return hash_out;
}

int mc_WalletTxs::AddExplorerTx(                                                          
              mc_TxImport *import,                                              // Import object, NULL if chain update
              const CTransaction& tx,                                           // Tx to add
              int block)                                                        // block height, -1 for mempool
{
    
    int err,i,j;
    mc_TxImport *imp;
    mc_TxEntity entity;
    mc_TxEntity subkey_entity;
    mc_TxEntity subkey_aa_entity;
    mc_TxDefRow txdef;
    mc_TxEntityStat *stat;
    mc_TxEntityRow erow;
    
    uint256 subkey_hash256;
    uint256 tag_hash;
    uint160 subkey_hash160;
    uint160 balance_subkey_hash160;
    set<uint160> publishers_set;

    bool fFound;
    uint256 hash;
    uint32_t block_key;
    int generation;
    mc_TxAssetBalanceDetails tx_balance_details;
    
    uint32_t flags;
    uint32_t timestamp;
    
    std::vector< std::map<uint160,int64_t> >OutputAssetQuantities;
    std::vector< uint160 >OutputStreams;
    std::vector< std::map<uint160,int64_t> >InputAssetQuantities;
    std::vector< std::map<uint160,uint32_t> >InputAddresses;
    std::vector< int > InputSigHashTypes;
    std::vector< std::map<uint160,uint32_t> >OutputAddresses;
    std::map<uint160,mc_TxAddressAssetQuantity> AddressAssetQuantities;
    std::map<uint160,mc_TxAddressAssetQuantity> AddressStreams;
    std::vector<uint64_t>InputScriptTags;
    std::vector<uint64_t>OutputScriptTags;
    uint64_t tx_tag,real_tx_asset_tag;
    uint32_t direct_tx_tag;
    unsigned char extended_tag_data[MC_MTX_TAG_EXTENSION_TOTAL_SIZE];
    bool fLicenseTransfer=false;
    
    if((m_Mode & MC_WMD_TXS) == 0)
    {
        return MC_ERR_NOERROR;
    }    
    
    err=MC_ERR_NOERROR;

    imp=import;
    if(imp == NULL)
    {
        imp=m_Database->FindImport(0);
    }
    
    hash=tx.GetHash();
    
//    tx_tag=mc_GetExplorerTxDetails(-1,tx,OutputAssetQuantities,OutputStreams,InputScriptTags,OutputScriptTags);
    tx_tag=mc_GetExplorerTxOutputDetails(-1,tx,OutputAddresses,OutputAssetQuantities,OutputStreams,OutputScriptTags);
    
    fFound=false;
    txdef.Zero();
    
    m_Database->SetTxCache((unsigned char*)&hash);
    if(m_Database->m_TxCachedFlags == MC_TCF_FOUND)
    {
        fFound=true;
        memcpy(&txdef,&(m_Database->m_TxCachedDef),sizeof(mc_TxDefRow));
    }
    
    if(tx_tag & (MC_MTX_TAG_ENTITY_CREATE | MC_MTX_TAG_ASSET_GENESIS))
    {
        uint32_t entity_key=mc_EntityExplorerCodeToType((tx_tag & MC_MTX_TAG_ENTITY_MASK) >> MC_MTX_TAG_ENTITY_MASK_SHIFT);
        if(entity_key == 0)
        {
            for(i=0;i<(int)tx.vout.size();i++)                                  
            {        
                if(OutputScriptTags[i] & MC_MTX_TAG_ASSET_GENESIS)
                {
                    if(OutputScriptTags[i] & MC_MTX_TAG_LICENSE_TOKEN)
                    {
                        entity_key=MC_ENT_TYPE_LICENSE_TOKEN;
                    }
                    else
                    {
                        entity_key=MC_ENT_TYPE_ASSET;                        
                    }
                }
            }            
        }

        if(entity_key)
        {            
            memcpy(&subkey_hash160,&entity_key,sizeof(uint32_t));
            subkey_hash256=0;
            memcpy(&subkey_hash256,&subkey_hash160,sizeof(subkey_hash160));

            err=m_Database->AddSubKeyDef(imp,(unsigned char*)&subkey_hash256,NULL,0,MC_SFL_SUBKEY);
            if(err)
            {
                goto exitlbl;
            }

            entity.Zero();
            entity.m_EntityType=MC_TET_ENTITY_KEY | MC_TET_CHAINPOS;
            subkey_entity.Zero();
            memcpy(subkey_entity.m_EntityID,&subkey_hash160,MC_TDB_ENTITY_ID_SIZE);
            subkey_entity.m_EntityType=MC_TET_SUBKEY_ENTITY_KEY | MC_TET_CHAINPOS;
            err= m_Database->IncrementSubKey(imp,&entity,&subkey_entity,(unsigned char*)&subkey_hash160,(unsigned char*)&hash,NULL,block,tx_tag,fFound ? 0 : 1);
            if(err)
            {
                goto exitlbl;
            }                            
        }
    }
    else
    {
        if(tx.vout.size() == 1)
        {
            if(OutputScriptTags[0] & MC_MTX_TAG_LICENSE_TOKEN)                
            {
                fLicenseTransfer=true;
            }
        }
    }
    
    if( (m_Mode & MC_WMD_EXPLORER_MASK) == 0 )
    {
        return MC_ERR_NOERROR;
    }
    
    entity.Zero();    
    entity.m_EntityType=MC_TET_EXP_TX_KEY | MC_TET_CHAINPOS;

    stat=imp->GetEntity(imp->FindEntity(&entity));
    if(stat == NULL)
    {
        goto exitlbl;        
    }
    generation=stat->m_Generation;
    
    tx_tag |= mc_GetExplorerTxInputDetails(-1,imp,tx,InputAddresses,InputAssetQuantities,InputSigHashTypes,InputScriptTags);
        
    real_tx_asset_tag=mc_CheckExplorerAssetTransfers(InputAddresses,InputAssetQuantities,OutputAddresses,OutputAssetQuantities,AddressAssetQuantities);
    
    tx_tag |= real_tx_asset_tag & (MC_MTX_TAG_MULTIPLE_ASSETS | MC_MTX_TAG_COMBINE);
    
    if(tx_tag & MC_MTX_TAG_ASSET_TRANSFER)
    {
        if( (real_tx_asset_tag & MC_MTX_TAG_ASSET_TRANSFER) == 0)
        {
            tx_tag -= MC_MTX_TAG_ASSET_TRANSFER;
        }
    }
    if(tx_tag & MC_MTX_TAG_NATIVE_TRANSFER)
    {
        if( (real_tx_asset_tag & MC_MTX_TAG_NATIVE_TRANSFER) == 0)
        {
            tx_tag -= MC_MTX_TAG_NATIVE_TRANSFER;
        }
    }

    if(tx_tag & MC_MTX_TAG_NO_KEY_SCRIPT_ID)
    {
        if(tx_tag & MC_MTX_TAG_COMBINE)
        {
            tx_tag -= MC_MTX_TAG_COMBINE;
        }
    }   
    
    direct_tx_tag=(uint32_t)(tx_tag & MC_MTX_TAG_DIRECT_MASK);
    tag_hash=mc_ExplorerTxIDHashMap(hash);
    memset(extended_tag_data,0,MC_MTX_TAG_EXTENSION_TOTAL_SIZE);
    if(tx_tag & MC_MTX_TAG_EXTENSION_MASK)
    {
        direct_tx_tag = MC_MTX_TAG_EXTENDED_TAGS;
        memcpy(extended_tag_data,&hash,sizeof(uint256));
        mc_PutLE(extended_tag_data+sizeof(uint256),&tx_tag,sizeof(uint64_t));

//        memcpy(extended_tag_data+sizeof(uint256),&tx_tag,sizeof(uint64_t));
        tag_hash=Hash((unsigned char*)extended_tag_data,(unsigned char*)extended_tag_data+MC_MTX_TAG_EXTENSION_TOTAL_SIZE);
// No need to add subkey, it will be added by AddData below        
//        err=m_Database->AddSubKeyDef(imp,(unsigned char*)&tag_hash,extended_tag_data,MC_MTX_TAG_EXTENSION_TOTAL_SIZE,0);
        
        if(err)
        {
            goto exitlbl;
        }
    }
    
    block_key=chainActive.Height()+1;
    if(block>=0)
    {
        block_key=(uint32_t)block;
    }
    
    memcpy(&subkey_hash160,&block_key,sizeof(uint32_t));
    subkey_hash256=0;
    memcpy(&subkey_hash256,&subkey_hash160,sizeof(subkey_hash160));
    
    err=m_Database->AddSubKeyDef(imp,(unsigned char*)&subkey_hash256,NULL,0,MC_SFL_SUBKEY);
    if(err)
    {
        goto exitlbl;
    }
    
    for(i=0;i<(int)tx.vout.size();i++)                                          
    {        
        if(OutputStreams[i] != 0)
        {
            for (j = 0; j < (int)tx.vin.size(); ++j)
            {
                if( (InputSigHashTypes[j] == SIGHASH_ALL) || ( (InputSigHashTypes[j] == SIGHASH_SINGLE) && ((j == i) ) ) )                    
                {
                    mc_GetCompoundHash160(&balance_subkey_hash160,&(InputAddresses[j].begin()->first),&(OutputStreams[i]));
                    std::map<uint160,mc_TxAddressAssetQuantity>::iterator it_balance=AddressStreams.find(balance_subkey_hash160);
                    if(it_balance == AddressStreams.end())
                    {
                        mc_TxAddressAssetQuantity aa_qty;
                        aa_qty.m_Address=InputAddresses[j].begin()->first;
                        aa_qty.m_Asset=OutputStreams[i];
                        aa_qty.m_Amount=1;
                        AddressStreams.insert(make_pair(balance_subkey_hash160,aa_qty));
                    }                    
                }                
            }            
        }
    }    

    for (map<uint160,mc_TxAddressAssetQuantity>::const_iterator it_balance = AddressStreams.begin(); it_balance != AddressStreams.end(); ++it_balance) 
    {
        flags=0;
        erow.Zero();
        subkey_entity.Zero();
        memcpy(subkey_entity.m_EntityID,&(it_balance->first),MC_TDB_ENTITY_ID_SIZE);
        subkey_entity.m_EntityType=MC_TET_EXP_ADDRESS_STREAMS_PAIRS | MC_TET_CHAINPOS;
        if(m_Database->GetLastItem(imp,&subkey_entity,generation,&erow) == MC_ERR_NOT_FOUND)
        {
            entity.Zero();
            entity.m_EntityType=MC_TET_EXP_ADDRESS_STREAMS_KEY | MC_TET_CHAINPOS;
            subkey_aa_entity.Zero();
            memcpy(subkey_aa_entity.m_EntityID,&(it_balance->second.m_Address),MC_TDB_ENTITY_ID_SIZE);
            subkey_aa_entity.m_EntityType=MC_TET_SUBKEY_EXP_ADDRESS_STREAMS_KEY | MC_TET_CHAINPOS;
            subkey_hash256=0;
            memcpy(&subkey_hash256,&(it_balance->second.m_Asset),sizeof(uint160));
            err= m_Database->IncrementSubKey(imp,&entity,&subkey_aa_entity,(unsigned char*)&(it_balance->second.m_Address),(unsigned char*)&subkey_hash256,NULL,block,0,1);
            if(err)
            {
                goto exitlbl;
            }             

            entity.Zero();
            entity.m_EntityType=MC_TET_EXP_ADDRESS_STREAMS_PAIRS | MC_TET_CHAINPOS;            

            subkey_hash256=hash;
            err= m_Database->IncrementSubKey(imp,&entity,&subkey_entity,(unsigned char*)&(it_balance->first),(unsigned char*)&subkey_hash256,NULL,block,flags,1);
            if(err)
            {
                goto exitlbl;
            }        
        }
    }

    
    for (map<uint160,mc_TxAddressAssetQuantity>::const_iterator it_balance = AddressAssetQuantities.begin(); it_balance != AddressAssetQuantities.end(); ++it_balance) 
    {
        if(it_balance->second.m_Amount != 0)
        {
//            real_transfer=true;
            erow.Zero();
            subkey_entity.Zero();
            memcpy(subkey_entity.m_EntityID,&(it_balance->first),MC_TDB_ENTITY_ID_SIZE);
            subkey_entity.m_EntityType=MC_TET_SUBKEY_EXP_BALANCE_DETAILS_KEY | MC_TET_CHAINPOS;
            tx_balance_details.m_Balance=0;
            if(m_Database->GetLastItem(imp,&subkey_entity,generation,&erow) == MC_ERR_NOERROR)
            {
                string assets_str=GetSubKey(imp,erow.m_TxId,NULL,&err);
                if(err)
                {
                    goto exitlbl;                        
                }
                if(assets_str.size() != sizeof(mc_TxAssetBalanceDetails))
                {
                    goto exitlbl;                                                    
                }
                memcpy(&tx_balance_details,assets_str.c_str(),assets_str.size());
            }
            else
            {
                entity.Zero();
                entity.m_EntityType=MC_TET_EXP_ADDRESS_ASSETS_KEY | MC_TET_CHAINPOS;
                subkey_aa_entity.Zero();
                memcpy(subkey_aa_entity.m_EntityID,&(it_balance->second.m_Address),MC_TDB_ENTITY_ID_SIZE);
                subkey_aa_entity.m_EntityType=MC_TET_SUBKEY_EXP_ADDRESS_ASSETS_KEY | MC_TET_CHAINPOS;
                subkey_hash256=0;
                memcpy(&subkey_hash256,&(it_balance->second.m_Asset),sizeof(uint160));
                err= m_Database->IncrementSubKey(imp,&entity,&subkey_aa_entity,(unsigned char*)&(it_balance->second.m_Address),(unsigned char*)&subkey_hash256,NULL,block,0,1);
                if(err)
                {
                    goto exitlbl;
                }             

                entity.Zero();
                entity.m_EntityType=MC_TET_EXP_ASSET_ADDRESSES_KEY | MC_TET_CHAINPOS;
                subkey_aa_entity.Zero();
                memcpy(subkey_aa_entity.m_EntityID,&(it_balance->second.m_Asset),MC_TDB_ENTITY_ID_SIZE);
                subkey_aa_entity.m_EntityType=MC_TET_SUBKEY_EXP_ASSET_ADDRESSES_KEY | MC_TET_CHAINPOS;
                flags=0;
                subkey_hash256=0;
                memcpy(&subkey_hash256,&(it_balance->second.m_Address),sizeof(uint160));
                err= m_Database->IncrementSubKey(imp,&entity,&subkey_aa_entity,(unsigned char*)&(it_balance->second.m_Asset),(unsigned char*)&subkey_hash256,NULL,block,flags,1);
                if(err)
                {
                    goto exitlbl;
                }                                            
            }
            
            tx_balance_details.m_TxID=hash;
            tx_balance_details.m_Balance+=it_balance->second.m_Amount;
            tx_balance_details.m_Amount=it_balance->second.m_Amount;
            
            entity.Zero();
            entity.m_EntityType=MC_TET_EXP_BALANCE_DETAILS_KEY | MC_TET_CHAINPOS;            
            
            subkey_hash256=Hash((unsigned char*)&tx_balance_details,(unsigned char*)&tx_balance_details+sizeof(mc_TxAssetBalanceDetails));
            err=m_Database->AddSubKeyDef(imp,(unsigned char*)&subkey_hash256,(unsigned char*)&tx_balance_details,sizeof(mc_TxAssetBalanceDetails),0);
            if(err)
            {
                goto exitlbl;
            }
            err= m_Database->IncrementSubKey(imp,&entity,&subkey_entity,(unsigned char*)&(it_balance->first),(unsigned char*)&subkey_hash256,NULL,block,flags,1);
            if(err)
            {
                goto exitlbl;
            }                            
        }
    }
/*    
    if(tx_tag & MC_MTX_TAG_ASSET_TRANSFER)
    {
        if(!real_transfer)
        {
            tx_tag -= MC_MTX_TAG_ASSET_TRANSFER;
        }
    }
*/    
    entity.Zero();
    entity.m_EntityType=MC_TET_EXP_TX_KEY | MC_TET_CHAINPOS;
    subkey_hash160=0;
    memcpy(&subkey_hash160,&block_key,sizeof(uint32_t));
    subkey_entity.Zero();
    memcpy(subkey_entity.m_EntityID,&subkey_hash160,MC_TDB_ENTITY_ID_SIZE);
    subkey_entity.m_EntityType=MC_TET_SUBKEY_EXP_TX_KEY | MC_TET_CHAINPOS;
    err= m_Database->IncrementSubKey(imp,&entity,&subkey_entity,(unsigned char*)&subkey_hash160,(unsigned char*)&tag_hash,NULL,block,direct_tx_tag,fFound ? 0 : 1);
    if(err)
    {
        goto exitlbl;
    }                            
    
    publishers_set.clear();
    if(!tx.IsCoinBase())
    {
        for (j = 0; j < (int)tx.vin.size(); ++j)
        {
            COutPoint txout=tx.vin[j].prevout;
            
            const unsigned char *ptr;
            
            ptr=(unsigned char*)&txout;
            subkey_hash160=Hash160(ptr,ptr+sizeof(COutPoint));
            entity.m_EntityType=MC_TET_EXP_REDEEM_KEY | MC_TET_CHAINPOS;
            subkey_entity.Zero();
            memcpy(subkey_entity.m_EntityID,&subkey_hash160,MC_TDB_ENTITY_ID_SIZE);
            subkey_entity.m_EntityType=MC_TET_SUBKEY_REDEEM_KEY | MC_TET_CHAINPOS;
            err= m_Database->IncrementSubKey(imp,&entity,&subkey_entity,(unsigned char*)&subkey_hash160,(unsigned char*)&hash,NULL,block,(uint32_t)j,1);
            if(err)
            {
                goto exitlbl;
            }                            

            const CScript& script2 = tx.vin[j].scriptSig;        
            CScript::const_iterator pc2 = script2.begin();

            if(!fLicenseTransfer && (InputAddresses[j].size() == 1))
            {
                uint32_t publisher_flags;
                subkey_hash160=InputAddresses[j].begin()->first;
                if(publishers_set.count(subkey_hash160) == 0)
                {
                    publishers_set.insert(subkey_hash160);
                    subkey_hash256=0;
                    memcpy(&subkey_hash256,&subkey_hash160,sizeof(subkey_hash160));
                    publisher_flags=MC_SFL_SUBKEY | MC_SFL_IS_ADDRESS | InputAddresses[j].begin()->second;
                    err=m_Database->AddSubKeyDef(imp,(unsigned char*)&subkey_hash256,NULL,0,publisher_flags);
                    if(err)
                    {
                        goto exitlbl;
                    }

                    entity.m_EntityType=MC_TET_EXP_TX_PUBLISHER | MC_TET_CHAINPOS;
                    subkey_entity.Zero();
                    memcpy(subkey_entity.m_EntityID,&subkey_hash160,MC_TDB_ENTITY_ID_SIZE);
                    subkey_entity.m_EntityType=MC_TET_SUBKEY_EXP_TX_PUBLISHER | MC_TET_CHAINPOS;
                    err= m_Database->IncrementSubKey(imp,&entity,&subkey_entity,(unsigned char*)&subkey_hash160,(unsigned char*)&tag_hash,NULL,block,direct_tx_tag,fFound ? 0 : 1);
                    if(err)
                    {
                        goto exitlbl;
                    }      
                }     
            }
        }
    }
    
    if(block != 0)
    {
        for(i=0;i<(int)tx.vout.size();i++)                     
        {        
            const CTxOut txout=tx.vout[i];
            const CScript& script1 = txout.scriptPubKey;        
            CScript::const_iterator pc1 = script1.begin();

            mc_gState->m_TmpScript->Clear();
            mc_gState->m_TmpScript->SetScript((unsigned char*)(&pc1[0]),(size_t)(script1.end()-pc1),MC_SCR_TYPE_SCRIPTPUBKEY);
            if(mc_gState->m_TmpScript->IsOpReturnScript() == 0)
            {  
                if(((OutputScriptTags[0] & MC_MTX_TAG_LICENSE_TOKEN) == 0) && (OutputAddresses[i].size() == 1) )
                {
                    uint32_t publisher_flags;
                    entity.Zero();
                    subkey_hash160=OutputAddresses[i].begin()->first;
                    publisher_flags=MC_SFL_SUBKEY | MC_SFL_IS_ADDRESS | OutputAddresses[i].begin()->second;

                    if(publishers_set.count(subkey_hash160) == 0)
                    {
                        publishers_set.insert(subkey_hash160);
                        subkey_hash256=0;
                        memcpy(&subkey_hash256,&subkey_hash160,sizeof(subkey_hash160));
                        err=m_Database->AddSubKeyDef(imp,(unsigned char*)&subkey_hash256,NULL,0,publisher_flags);
                        if(err)
                        {
                            goto exitlbl;
                        }

                        entity.m_EntityType=MC_TET_EXP_TX_PUBLISHER | MC_TET_CHAINPOS;
                        subkey_entity.Zero();
                        memcpy(subkey_entity.m_EntityID,&subkey_hash160,MC_TDB_ENTITY_ID_SIZE);
                        subkey_entity.m_EntityType=MC_TET_SUBKEY_EXP_TX_PUBLISHER | MC_TET_CHAINPOS;
                        err= m_Database->IncrementSubKey(imp,&entity,&subkey_entity,(unsigned char*)&subkey_hash160,(unsigned char*)&tag_hash,NULL,block,direct_tx_tag,fFound ? 0 : 1);
                        if(err)
                        {
                            goto exitlbl;
                        }             
                    }
                }
            }

            if(OutputAssetQuantities[i].size())
            {
                COutPoint txout=COutPoint(hash,i);

                const unsigned char *ptr;

                ptr=(unsigned char*)&txout;
                subkey_hash160=Hash160(ptr,ptr+sizeof(COutPoint));
                entity.m_EntityType=MC_TET_EXP_TXOUT_ASSETS_KEY | MC_TET_CHAINPOS;
                subkey_entity.Zero();
                memcpy(subkey_entity.m_EntityID,&subkey_hash160,MC_TDB_ENTITY_ID_SIZE);
                subkey_entity.m_EntityType=MC_TET_SUBKEY_EXP_TXOUT_ASSETS_KEY | MC_TET_CHAINPOS;
                flags=0;
                if(OutputAssetQuantities[i].size() > 1)
                {
                    flags=MC_MTX_TFL_MULTIPLE_TXOUT_ASSETS;
                    mc_gState->m_TmpBuffers->m_ExplorerTxScript->Clear();
                    mc_gState->m_TmpBuffers->m_ExplorerTxScript->AddElement();
                    for (map<uint160,int64_t>::const_iterator it = OutputAssetQuantities[i].begin(); it != OutputAssetQuantities[i].end(); ++it) 
                    {
                        mc_gState->m_TmpBuffers->m_ExplorerTxScript->SetData((unsigned char*)&(it->first),sizeof(uint160));
                        mc_gState->m_TmpBuffers->m_ExplorerTxScript->SetData((unsigned char*)&(it->second),sizeof(int64_t));
                    }
                    subkey_hash256=0;
                    memcpy(&subkey_hash256,&subkey_hash160,sizeof(subkey_hash160));
                    memcpy((unsigned char*)&subkey_hash256+sizeof(subkey_hash160),(unsigned char*)&(subkey_entity.m_EntityType),sizeof(uint32_t));// To distinguish from other possible stuff for this txout 
                    subkey_hash256=Hash(mc_gState->m_TmpBuffers->m_ExplorerTxScript->m_lpData,
                                        mc_gState->m_TmpBuffers->m_ExplorerTxScript->m_lpData+mc_gState->m_TmpBuffers->m_ExplorerTxScript->GetSize());
                    err=m_Database->AddSubKeyDef(imp,(unsigned char*)&subkey_hash256,mc_gState->m_TmpBuffers->m_ExplorerTxScript->m_lpData,
                                                                                     mc_gState->m_TmpBuffers->m_ExplorerTxScript->GetSize(),0);
                    if(err)
                    {
                        goto exitlbl;
                    }
                    err= m_Database->IncrementSubKey(imp,&entity,&subkey_entity,(unsigned char*)&subkey_hash160,(unsigned char*)&subkey_hash256,NULL,block,flags,1);
                    if(err)
                    {
                        goto exitlbl;
                    }                            
                }
                else
                {
                    map<uint160,int64_t>::const_iterator it = OutputAssetQuantities[i].begin();
                    subkey_hash256=0;
                    memcpy(&subkey_hash256,&(it->first),sizeof(uint160));
                    memcpy((unsigned char*)&subkey_hash256+sizeof(subkey_hash160),&(it->second),sizeof(int64_t));
                    err= m_Database->IncrementSubKey(imp,&entity,&subkey_entity,(unsigned char*)&subkey_hash160,(unsigned char*)&subkey_hash256,NULL,block,flags,1);
                    if(err)
                    {
                        goto exitlbl;
                    }                                            
                }
            }
        }
    }    
    
    timestamp=mc_TimeNowAsUInt();
    
    imp->m_TmpEntities->Clear();
    entity.Zero();
    entity.m_EntityType=MC_TET_EXP_TX | MC_TET_CHAINPOS;
    imp->m_TmpEntities->Add(&entity,NULL);
//    if(tag_hash == hash)
    if(direct_tx_tag != MC_MTX_TAG_EXTENDED_TAGS)        
    {
        err=m_Database->AddData(imp,(unsigned char*)&tag_hash,NULL,0,block,direct_tx_tag,timestamp,imp->m_TmpEntities);
    }
    else
    {
        err=m_Database->AddData(imp,(unsigned char*)&tag_hash,extended_tag_data,MC_MTX_TAG_EXTENSION_TOTAL_SIZE,block,direct_tx_tag,timestamp,imp->m_TmpEntities);        
    }
    
exitlbl:    
                
    if(err)
    {
        LogPrintf("wtxs: AddExplorerTx  Error: %d\n",err);        
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


CWalletTx mc_WalletTxs::WRPGetWalletTx(uint256 hash,mc_TxDefRow *txdef,int *errOut)
{
    int err;
    CWalletTx wtx;
    mc_TxDefRow StoredTxDef;
    FILE* fHan;
    char ShortName[65];                                     
    char FileName[MC_DCT_DB_MAX_PATH];                      
    
    if(fDebug)LogPrint("dwtxs01","dwtxs01: %d: --> %s\n",GetRPCSlot(),hash.ToString().c_str());
    
    int use_read=m_Database->WRPUsed();
    
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
    
    
    if(use_read == 0)
    {
        m_Database->Lock(0,0);
    }
        
    err=m_Database->WRPGetTx(&StoredTxDef,(unsigned char*)&hash,0);
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
        if(use_read)                                                            // Not locked yet
        {
            m_Database->Lock(0,0);
        }
        std::map<uint256,CWalletTx>::const_iterator it = m_UnconfirmedSends.find(hash);
        if (it != m_UnconfirmedSends.end())
        {
            wtx=it->second;
        }
        else
        {
            err=MC_ERR_CORRUPTED;
        }
        if(use_read)
        {
            m_Database->UnLock();
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

    if(use_read == 0)
    {
        m_Database->UnLock();
    }

    if(fDebug)LogPrint("dwtxs01","dwtxs01: %d: <-- %s\n",GetRPCSlot(),hash.ToString().c_str());

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

string mc_WalletTxs::GetSubKey(mc_TxImport *import, void *hash,mc_TxDefRow *txdef,int *errOut)
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
    err=m_Database->GetTx(import,&StoredTxDef,(unsigned char*)hash,0);
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

string mc_WalletTxs::WRPGetSubKey(void *hash,mc_TxDefRow *txdef,int *errOut)
{
    int err;
    string ret;
    mc_TxDefRow StoredTxDef;
    char ShortName[65];                                     
    char FileName[MC_DCT_DB_MAX_PATH];                      
    int  fHan=0;
    uint160 subkey_hash;
    
    err = MC_ERR_NOERROR;

    int use_read=m_Database->WRPUsed();
        
    if(use_read == 0)
    {
        m_Database->Lock(0,0);
    }
    
    ret="";
    
    err=m_Database->WRPGetTx(&StoredTxDef,(unsigned char*)hash,0);
    
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

    if(use_read == 0)
    {
        m_Database->UnLock();
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
