// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2014-2016 The Bitcoin Core developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.


#include "rpc/rpcwallet.h"
#include "json/json_spirit_ubjson.h"
#include "json/json_spirit_reader_template.h"
#include "json/json_spirit_writer_template.h"
#include "community/community.h"

bool WRPSubKeyEntityFromPublisher(string str,mc_TxEntityStat entStat,mc_TxEntity *entity,bool ignore_unsubscribed,int *errCode,string *strError);
void TxToJSON(const CTransaction& tx, const uint256 hashBlock, Object& entry);
uint64_t mc_GetExplorerTxOutputDetails(int rpc_slot,
                                 const CTransaction& tx,
                                 std::vector< std::map<uint160,uint32_t> >& OutputAddresses,
                                 std::vector< std::map<uint160,int64_t> >& OutputAssetQuantities,
                                 std::vector< uint160 >&OutputStreams,
//                                 std::vector<uint64_t>& InputScriptTags,
                                 std::vector<uint64_t>& OutputScriptTags);
uint64_t mc_GetExplorerTxInputDetails(int rpc_slot,
                                 mc_TxImport *import,                   
                                 const CTransaction& tx,
                                 std::vector< std::map<uint160,uint32_t> >& InputAddresses,
                                 std::vector< std::map<uint160,int64_t> >& InputAssetQuantities,
                                 std::vector< int >& InputSigHashTypes,
                                 std::vector<uint64_t>& InputScriptTags);
uint32_t mc_CheckExplorerAssetTransfers(
                                 std::vector< std::map<uint160,uint32_t> >& InputAddresses,
                                 std::vector< std::map<uint160,int64_t> >& InputAssetQuantities,
                                 std::vector< std::map<uint160,uint32_t> >& OutputAddresses,
                                 std::vector< std::map<uint160,int64_t> >& OutputAssetQuantities,
                                 std::map<uint160,mc_TxAddressAssetQuantity>& AssetsAddressQuantities);
int mc_EntityExplorerCodeToType(int entity_explorer_code);
uint256 mc_ExplorerTxIDHashMap(uint256 hash_in);

/*
uint32_t mc_GetExplorerTxDetails(int rpc_slot,
                                 const CTransaction& tx,
                                 std::vector< std::map<uint160,int64_t> >& OutputAssetQuantities,
                                 std::vector< uint160 >&OutputStreams,
                                 std::vector<uint64_t>& InputScriptTags,
                                 std::vector<uint64_t>& OutputScriptTags);
*/

bool paramtobool_or_flag(Value param,int *flag)
{
    if(param.type() == str_type)
    {
        if(param.get_str() == "-")
        {
            *flag=2;
            return false;
        }
        return false;        
    }
    return paramtobool(param);
}

bool WRPSubKeyEntityFromExpAddress(string str,mc_TxEntityStat entStat,mc_TxEntity *entity,bool ignore_unsubscribed,int *errCode,string *strError)
{
    if(str == "*")
    {
        return false;
    }
    
    uint160 subkey_hash;
    CBitcoinAddress address(str);
    if (!address.IsValid())
    {
        *errCode=RPC_INVALID_ADDRESS_OR_KEY;
        *strError="Invalid address";
        return false;
    }
    CTxDestination dest=address.Get();
    CKeyID *lpKeyID=boost::get<CKeyID> (&dest);
    CScriptID *lpScriptID=boost::get<CScriptID> (&dest);

    
    if ((lpKeyID == NULL) && (lpScriptID == NULL) )
    {
        *errCode=RPC_INVALID_ADDRESS_OR_KEY;
        *strError="Invalid address";
        return false;
    }

    if(lpKeyID)
    {
        memcpy(&subkey_hash,lpKeyID,MC_TDB_ENTITY_ID_SIZE);
    }
    else
    {
        memcpy(&subkey_hash,lpScriptID,MC_TDB_ENTITY_ID_SIZE);
    }

    memcpy(entity->m_EntityID,&subkey_hash,MC_TDB_ENTITY_ID_SIZE);
    entity->m_EntityType=entStat.m_Entity.m_EntityType | MC_TET_SUBKEY;    
        
    return true;
}

bool WRPSubKeyEntityFromExpAsset(mc_EntityDetails *entity_details,mc_TxEntityStat entStat,mc_TxEntity *entity,bool ignore_unsubscribed,int *errCode,string *strError)
{
    uint160 subkey_hash=0;
    memcpy(&subkey_hash, entity_details->GetTxID()+MC_AST_SHORT_TXID_OFFSET,MC_AST_SHORT_TXID_SIZE);

    memcpy(entity->m_EntityID,&subkey_hash,MC_TDB_ENTITY_ID_SIZE);
    entity->m_EntityType=entStat.m_Entity.m_EntityType | MC_TET_SUBKEY;    
        
    return true;
}

Value TagEntry(uint64_t tag,bool tx) 
{
    Array tags;
    if(tag & MC_MTX_TAG_P2SH)
    {
        tags.push_back("pay-to-script-hash");
    }
    if(tag & MC_MTX_TAG_NO_KEY_SCRIPT_ID)
    {
        tags.push_back("not-p2pkh-p2sh");
    }
    if(tag & MC_MTX_TAG_COINBASE)
    {
        tags.push_back("coinbase");
    }
    if(tag & MC_MTX_TAG_GRANT_ENTITY)
    {
        tags.push_back("grant-per-entity");                        
    }
    if(tag & MC_MTX_TAG_REVOKE_ENTITY)
    {
        tags.push_back("revoke-per-entity");                        
    }
    if(tag & MC_MTX_TAG_GRANT_HIGH)
    {
        tags.push_back("grant-high");                        
    }
    if(tag & MC_MTX_TAG_REVOKE_HIGH)
    {
        tags.push_back("revoke-high");                        
    }
    if(tag & MC_MTX_TAG_GRANT_LOW)
    {
        tags.push_back("grant-low");                        
    }
    if(tag & MC_MTX_TAG_REVOKE_LOW)
    {
        tags.push_back("revoke-low");                        
    }
    if(tag & MC_MTX_TAG_ASSET_GENESIS)
    {
        if(tag & MC_MTX_TAG_LICENSE_TOKEN)
        {
            tags.push_back("issue-license-unit");            
        }
        else
        {
            tags.push_back("issue-asset-units");            
        }
    }
    if(tag & MC_MTX_TAG_ASSET_TRANSFER)
    {
        if(tag & MC_MTX_TAG_LICENSE_TOKEN)
        {            
            tags.push_back(tx ? "transfer-license" : "license");            
        }
        else
        {            
            tags.push_back(tx ? "transfer-asset" : "asset");            
        }
    }
    if(tag & MC_MTX_TAG_MULTIPLE_ASSETS)
    {
        tags.push_back("multiple-assets");                            
    }
    if(tag & MC_MTX_TAG_NATIVE_TRANSFER)
    {
        tags.push_back(tx ? "transfer-native" : "native");                    
    }
    if(tag & MC_MTX_TAG_ASSET_FOLLOWON)
    {
        tags.push_back("issuemore-asset-units");            
    }
    string entity="entity";
    if(tag & MC_MTX_TAG_ENTITY_MASK)
    {
        int ent=mc_EntityExplorerCodeToType((tag & MC_MTX_TAG_ENTITY_MASK) >> MC_MTX_TAG_ENTITY_MASK_SHIFT);
        switch(ent)
        {
            case MC_ENT_TYPE_ASSET: entity="asset"; break;
            case MC_ENT_TYPE_STREAM: entity="stream"; break;
            case MC_ENT_TYPE_UPGRADE: entity="upgrade"; break;
            case MC_ENT_TYPE_FILTER: entity="filter"; break;
            case MC_ENT_TYPE_LICENSE_TOKEN: entity="license"; break;
            case MC_ENT_TYPE_VARIABLE: entity="variable"; break;
            case MC_ENT_TYPE_LIBRARY: entity="library"; break;
            default:entity="pseudo-stream"; break;                
        }
    }
    
    if(tag & MC_MTX_TAG_ENTITY_CREATE)
    {
        if(entity=="asset")
        {
            tags.push_back("issue-asset-details");
        }
        else
        {
            tags.push_back("create-" + entity);
        }
    }
    if(tag & MC_MTX_TAG_ENTITY_UPDATE)
    {
        if(entity=="asset")
        {
            if(tag & MC_MTX_TAG_OP_RETURN)
            {
                tags.push_back("issuemore-asset-details");
            }
            else
            {
                tags.push_back("update-asset");                
            }
        }
        else
        {
            tags.push_back("update-" + entity);
        }
    }
    if(tag & MC_MTX_TAG_FILTER_APPROVAL)
    {
        tags.push_back("approve-filter-library");
    }
    if(tag & MC_MTX_TAG_UPGRADE_APPROVAL)
    {
        tags.push_back("approve-upgrade");
    }
    
    if(tag & MC_MTX_TAG_STREAM_ITEM)
    {
        if(tag & MC_MTX_TAG_OFFCHAIN)
        {
            tags.push_back("offchain-stream-item");                
        }
        else
        {
            tags.push_back("onchain-stream-item");                                
        }
    }
    if(tag & MC_MTX_TAG_MULTIPLE_STREAM_ITEMS)
    {
        tags.push_back("multiple-stream-items");                            
    }
    if(tag & MC_MTX_TAG_COMBINE)
    {
        tags.push_back("combine-utxos");                            
    }
    if(tag & MC_MTX_TAG_INLINE_DATA)
    {
        tags.push_back("inline-data");                                            
    }
    if(tag & MC_MTX_TAG_RAW_DATA)
    {
        if( (tag & MC_MTX_TAG_COINBASE) == 0 )
        {
            tags.push_back("raw-data");                                            
        }
    }
    if(!tx)
    {
        if(tag & MC_MTX_TAG_OP_RETURN)
        {
            tags.push_back("unspendable");                                                    
        }
    }
    
    return tags;
}

void AddBlockInfo(Object& entry,int block,int chain_height)
{
    if( (block<0) || (block>chain_height) )
    {
        entry.push_back(Pair("confirmations",0));
        entry.push_back(Pair("blockheight",Value::null));
    }
    else
    {
        mc_gState->ChainLock();
        int cur_height=chainActive.Height();
        if(cur_height >= block)
        {
            uint256 blockHash=chainActive[block]->GetBlockHash();
            entry.push_back(Pair("confirmations",chain_height-block+1));
            entry.push_back(Pair("blockhash", chainActive[block]->GetBlockHash().GetHex()));
            entry.push_back(Pair("blockheight", block));
            entry.push_back(Pair("blocktime", mapBlockIndex[blockHash]->GetBlockTime()));            
        }
        else
        {
            entry.push_back(Pair("confirmations",0));
            entry.push_back(Pair("blockheight",Value::null));            
        }
        mc_gState->ChainUnLock();
    }
}


Value explorerlistmap_operation(mc_TxEntity *parent_entity,vector<mc_TxEntity>& inputEntities,vector<string>& inputStrings,int count, int start, string mode,int *errCode,string *strError)
{
    mc_TxEntity entity;
    mc_TxEntityStat entStat;
    mc_TxEntity sec_entity;
    mc_TxEntityStat sec_entStat;
    Array retArray;
    mc_Buffer *entity_rows;
    mc_TxEntityRow erow;
    uint160 subkey_hash;    
    int row,enitity_count,sec_entity_count;
    
    int rpc_slot=GetRPCSlot();
    if(rpc_slot < 0)
    {
        *errCode=RPC_INTERNAL_ERROR;
        *strError="Couldn't find RPC Slot";
        return Value::null;
    }
    
    entity_rows=mc_gState->m_TmpRPCBuffers[rpc_slot]->m_RpcEntityRows;

    entStat.Zero();
    memcpy(&entStat,parent_entity,sizeof(mc_TxEntity));
    pwalletTxsMain->WRPFindEntity(&entStat);
    
    entity_rows->Clear();
    enitity_count=inputEntities.size();
    if(enitity_count == 0)
    {
        mc_AdjustStartAndCount(&count,&start,pwalletTxsMain->WRPGetListSize(parent_entity,entStat.m_Generation,NULL));
        entity_rows->Clear();
        WRPCheckWalletError(pwalletTxsMain->WRPGetList(parent_entity,entStat.m_Generation,start+1,count,entity_rows),parent_entity->m_EntityType,"",errCode,strError);
        enitity_count=entity_rows->GetCount();
    }
    else
    {
        mc_AdjustStartAndCount(&count,&start,enitity_count);       
        enitity_count=count;
    }
    
    
    for(int i=0;i<enitity_count;i++)
    {
        mc_TxEntityRow *lpEntTx;
        string key_string;
        if(entity_rows->GetCount())
        {
            lpEntTx=(mc_TxEntityRow*)entity_rows->GetRow(i);
            key_string=pwalletTxsMain->WRPGetSubKey(lpEntTx->m_TxId, NULL,NULL);
            entity.Zero();
            memcpy(entity.m_EntityID,lpEntTx->m_TxId,MC_TDB_ENTITY_ID_SIZE);
            entity.m_EntityType=parent_entity->m_EntityType | MC_TET_SUBKEY;
        }
        else
        {
            memcpy(&entity,&(inputEntities[i+start]),sizeof(mc_TxEntity));
            key_string=inputStrings[i+start];
        }
        
        int total,confirmed;
        total=pwalletTxsMain->WRPGetListSize(&entity,entStat.m_Generation,&confirmed);
        
        Object all_entry;
        int shift=total-1;
        if(shift == 0)
        {
            shift=1;
        }
        vector <mc_QueryCondition> conditions;

        if((parent_entity->m_EntityType & MC_TET_TYPE_MASK) == MC_TET_EXP_TX_PUBLISHER)
        {
            all_entry.push_back(Pair("address", key_string));        
            conditions.push_back(mc_QueryCondition(MC_QCT_PUBLISHER,key_string));
        }
/*        
        else
        {
            all_entry.push_back(Pair("block", key_string));         
            conditions.push_back(mc_QueryCondition(MC_QCT_KEY,key_string));
        }
 */ 
        all_entry.push_back(Pair("txs", total));                                                                        
        all_entry.push_back(Pair("confirmed", confirmed));                                                                        
        
        if((parent_entity->m_EntityType & MC_TET_TYPE_MASK) == MC_TET_EXP_TX_PUBLISHER)
        {        
            sec_entStat.Zero();
            sec_entStat.m_Entity.m_EntityType=MC_TET_EXP_ADDRESS_ASSETS_KEY;
            sec_entStat.m_Entity.m_EntityType |= MC_TET_CHAINPOS;
            sec_entity.Zero();
            WRPSubKeyEntityFromExpAddress(key_string,sec_entStat,&sec_entity,false,errCode,strError);
            if(strError->size())
            {
                return Value::null;
            }
            sec_entity_count=pwalletTxsMain->WRPGetListSize(&sec_entity,entStat.m_Generation,NULL);
            all_entry.push_back(Pair("assets", sec_entity_count));                                                                        
            sec_entStat.Zero();
            sec_entStat.m_Entity.m_EntityType=MC_TET_EXP_ADDRESS_STREAMS_KEY;
            sec_entStat.m_Entity.m_EntityType |= MC_TET_CHAINPOS;
            sec_entity.Zero();
            WRPSubKeyEntityFromExpAddress(key_string,sec_entStat,&sec_entity,false,errCode,strError);
            if(strError->size())
            {
                return Value::null;
            }
            sec_entity_count=pwalletTxsMain->WRPGetListSize(&sec_entity,entStat.m_Generation,NULL);
            all_entry.push_back(Pair("streams", sec_entity_count));                                                                        
        }
        if(mode == "all")
        {
            int chain_height=chainActive.Height();
            for(row=1;row<=total;row+=shift)
            {
                if( ( (row == 1) && (mode != "last") ) || ( (row == total) && (mode != "first") ) )
                {                    
                    erow.Zero();
                    memcpy(&erow.m_Entity,&entity,sizeof(mc_TxEntity));
                    erow.m_Generation=entStat.m_Generation;
                    erow.m_Pos=row;

                    if(pwalletTxsMain->WRPGetRow(&erow) == 0)
                    {
                        uint256 hash;

                        memcpy(&hash,erow.m_TxId,MC_TDB_TXID_SIZE);                

                        Object entry;

                        entry.push_back(Pair("txid", hash.ToString()));
                        int block=erow.m_Block;
                        AddBlockInfo(entry,block,chain_height);                        
                        if(row == 1)
                        {
                            all_entry.push_back(Pair("first", entry));                                                                        
                        }
                        if(row == total)
                        {
                            all_entry.push_back(Pair("last", entry));                                                                        
                        }
                    }
                }
            }
        }
        retArray.push_back(all_entry);                                
    }

    return retArray;
}

Value explorerlisttransactions(const json_spirit::Array& params, bool fHelp)
{
    if (fHelp || params.size() > 3)
        throw runtime_error("Help message not found\n");

    if((mc_gState->m_WalletMode & MC_WMD_EXPLORER_MASK) == 0)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "Explorer APIs are not enabled. To enable them, please run \"multichaind -explorersupport=1 -rescan\" ");        
    }   
           
//    LOCK(cs_main);
    
    mc_TxEntityStat entStat;
    mc_Buffer *entity_rows=NULL;
    Array retArray;
    int errCode=0;
    string strError;
    

    int count,start;
    bool verbose=false;
    int flag=0;
    
    if (params.size() > 0)    
    {
        verbose=paramtobool_or_flag(params[0],&flag);
    }
    
    count=10;
    if (params.size() > 1)    
    {
        count=paramtoint(params[1],true,0,"Invalid count");
    }
    start=-count;
    if (params.size() > 2)    
    {
        start=paramtoint(params[2],false,0,"Invalid start");
    }
    
    entStat.Zero();
    entStat.m_Entity.m_EntityType=MC_TET_EXP_TX | MC_TET_CHAINPOS;
    
    bool fWRPLocked=false;
    int chain_height; 
  
    int rpc_slot=GetRPCSlot();
    if(rpc_slot < 0)
    {
        errCode=RPC_INTERNAL_ERROR;
        strError="Couldn't find RPC Slot";
        goto exitlbl;
    }
    
    fWRPLocked=true;    
    pwalletTxsMain->WRPReadLock();
    
    if(!pwalletTxsMain->WRPFindEntity(&entStat))
    {
        errCode=RPC_NOT_SUBSCRIBED;
        strError="Not subscribed to this service";
        goto exitlbl;
    }
    
    entity_rows=mc_gState->m_TmpRPCBuffers[rpc_slot]->m_RpcEntityRows;
    entity_rows->Clear();
    
    if(flag == 2)
    {
        pwalletTxsMain->WRPReadUnLock();
        return pwalletTxsMain->WRPGetListSize(&entStat.m_Entity,entStat.m_Generation,NULL);
    }
    
    mc_AdjustStartAndCount(&count,&start,pwalletTxsMain->WRPGetListSize(&entStat.m_Entity,entStat.m_Generation,NULL));
    
//    CheckWalletError(pwalletTxsMain->GetList(&entStat.m_Entity,start+1,count,entity_rows),entStat.m_Entity.m_EntityType,"");
    WRPCheckWalletError(pwalletTxsMain->WRPGetList(&entStat.m_Entity,entStat.m_Generation,start+1,count,entity_rows),entStat.m_Entity.m_EntityType,"",&errCode,&strError);

    chain_height=chainActive.Height();
    for(int i=0;i<entity_rows->GetCount();i++)
    {
        mc_TxEntityRow *lpEntTx;
        lpEntTx=(mc_TxEntityRow*)entity_rows->GetRow(i);
        uint256 hash;
        uint64_t tag=lpEntTx->m_Flags;
        
        if(tag & MC_MTX_TAG_EXTENDED_TAGS)
        {
            int err;
            string tag_str=pwalletTxsMain->WRPGetSubKey(lpEntTx->m_TxId,NULL,&err);
            if(tag_str.size() == MC_MTX_TAG_EXTENSION_TOTAL_SIZE)
            {
                memcpy(&hash,tag_str.c_str(),sizeof(uint256));
                tag=mc_GetLE((unsigned char *)tag_str.c_str()+sizeof(uint256),sizeof(uint64_t));
            }                        
        }
        else
        {
            memcpy(&hash,lpEntTx->m_TxId,MC_TDB_TXID_SIZE);        
            hash=mc_ExplorerTxIDHashMap(hash);
        }

        Object entry;
        
        entry.push_back(Pair("txid", hash.ToString()));
        int block=lpEntTx->m_Block;
        entry.push_back(Pair("tags", TagEntry(tag,true)));
        AddBlockInfo(entry,block,chain_height);                        
        
        retArray.push_back(entry);                                
    }
    
exitlbl:
                
    if(fWRPLocked)
    {
        pwalletTxsMain->WRPReadUnLock();
    }

    if(strError.size())
    {
        throw JSONRPCError(errCode, strError);            
    }
    
    return retArray;    
}

Value explorergetrawtransaction(const json_spirit::Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)                        // MCHN
        throw runtime_error("Help message not found\n");

    if((mc_gState->m_WalletMode & MC_WMD_EXPLORER_MASK) == 0)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "Explorer APIs are not enabled. To enable them, please run \"multichaind -explorersupport=1 -rescan\" ");        
    }   
    
    uint256 hash = ParseHashV(params[0], "parameter 1");

    bool fVerbose = false;
    if (params.size() > 1)
        fVerbose=paramtobool(params[1],false);
//        fVerbose = (params[1].get_int() != 0);

    CTransaction tx;
    uint256 hashBlock = 0;
    Object result;
    Value block_entry;
    int block,chain_height;
    
    block=-1;
    chain_height=0;
    {
        LOCK(cs_main);
        if (!GetTransaction(hash, tx, hashBlock, true))
            throw JSONRPCError(RPC_TX_NOT_FOUND, "No information available about transaction");


        string strHex = EncodeHexTx(tx);

        if (!fVerbose)        
            return strHex;

        result.push_back(Pair("hex", strHex));
        TxToJSON(tx, hashBlock, result);
        
        BOOST_FOREACH(Pair& a, result) 
        {
            if(a.name_ == "confirmations")
            {            
                if(a.value_.get_int() > 0)
                {
                    block=chainActive.Height()-a.value_.get_int()+1;;
                    chain_height=chainActive.Height();
//                    block_entry=chainActive.Height()-a.value_.get_int()+1;
                    
                }
            }
        }
    }
    
    mc_TxEntityStat entStat;
    mc_TxEntity entity;
    int errCode;
    string strError;
    vector <mc_QueryCondition> conditions;
    Array retArray;
    mc_Buffer *entity_rows=NULL;
    std::vector< std::map<uint160,int64_t> >OutputAssetQuantities;
    std::vector< uint160 >OutputStreams;
    std::vector< std::map<uint160,int64_t> >InputAssetQuantities;
    std::vector< std::map<uint160,uint32_t> >InputAddresses;
    std::vector< int > InputSigHashTypes;
    std::vector< std::map<uint160,uint32_t> >OutputAddresses;
    std::vector<uint64_t>InputScriptTags;
    std::vector<uint64_t>OutputScriptTags;
    std::map<uint160,mc_TxAddressAssetQuantity> AddressAssetQuantities;
    uint64_t tx_tag,real_tx_asset_tag;
    bool flags_found=false;
    uint32_t address_flags=0;
    Array assets;
    
    entStat.Zero();
    entStat.m_Entity.m_EntityType=MC_TET_EXP_REDEEM_KEY;
    entStat.m_Entity.m_EntityType |= MC_TET_CHAINPOS;
    
    bool fWRPLocked=false;
    int rpc_slot=GetRPCSlot();
    if(rpc_slot < 0)
    {
        errCode=RPC_INTERNAL_ERROR;
        strError="Couldn't find RPC Slot";
        goto exitlbl;
    }
    
    fWRPLocked=true;
    pwalletTxsMain->WRPReadLock();
    if(!pwalletTxsMain->WRPFindEntity(&entStat))
    {
        errCode=RPC_NOT_SUBSCRIBED;
        strError="Not subscribed to this service";
        goto exitlbl;
    }
    
    
    tx_tag=mc_GetExplorerTxOutputDetails(rpc_slot,tx,OutputAddresses,OutputAssetQuantities,OutputStreams,OutputScriptTags);
    tx_tag |= mc_GetExplorerTxInputDetails(rpc_slot,NULL,tx,InputAddresses,InputAssetQuantities,InputSigHashTypes,InputScriptTags);
    
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
    
    if(OutputScriptTags.size() == 1)
    {
        if(OutputScriptTags[0] & MC_MTX_TAG_LICENSE_TOKEN)
        {
            InputScriptTags[0] |= MC_MTX_TAG_LICENSE_TOKEN | MC_MTX_TAG_ASSET_TRANSFER;
        }
    }
    
    for (map<uint160,mc_TxAddressAssetQuantity>::const_iterator it_balance = AddressAssetQuantities.begin(); it_balance != AddressAssetQuantities.end(); ++it_balance) 
    {
        mc_EntityDetails entity_details;
        if(it_balance->second.m_Amount != 0)
        {
            if(mc_gState->m_Assets->FindEntityByShortTxID(&entity_details,(unsigned  char*)&(it_balance->second.m_Asset)))
            {
                for(int i=0;i<(int)tx.vout.size();i++)
                {
                    for (map<uint160,uint32_t>::const_iterator it_address = OutputAddresses[i].begin(); it_address != OutputAddresses[i].end(); ++it_address) 
                    {
                        if(it_address->first == it_balance->second.m_Address)
                        {
                            address_flags=it_address->second;
                            flags_found=true;
                        }
                    }                
                }            
                Object asset_entry;
                asset_entry=AssetEntry(entity_details.GetTxID(),it_balance->second.m_Amount,0x44);
                if(address_flags & MC_SFL_IS_SCRIPTHASH)
                {
                    asset_entry.push_back(Pair("address", CBitcoinAddress(CScriptID(it_balance->second.m_Address)).ToString()));
                }
                else
                {
                    asset_entry.push_back(Pair("address", CBitcoinAddress(CKeyID(it_balance->second.m_Address)).ToString()));
                }
                assets.push_back(asset_entry);            
            }
        }
    }    
    
    AddBlockInfo(result,block,chain_height);                        
//    result.push_back(Pair("block", block_entry));
    result.push_back(Pair("assets", assets));
    result.push_back(Pair("tags", TagEntry(tx_tag,true)));
    
    BOOST_FOREACH(Pair& a, result) 
    {
        if(a.name_ == "vin")
        {            
            
            for(int j=0;j<(int)a.value_.get_array().size();j++)
            {
                a.value_.get_array()[j].get_obj().push_back(Pair("tags", TagEntry(InputScriptTags[j],false)));
                Array assets;
                for (map<uint160,int64_t>::const_iterator it_asset = InputAssetQuantities[j].begin(); it_asset != InputAssetQuantities[j].end(); ++it_asset) 
                {
                    mc_EntityDetails entity_details;
                    Object asset_entry;
                    if(it_asset->first != 0)
                    {
                        mc_gState->m_Assets->FindEntityByShortTxID(&entity_details,(unsigned char*)&(it_asset->first));
                        asset_entry=AssetEntry(entity_details.GetTxID(),it_asset->second,0x05);
                    }
                    else
                    {
                        int64_t multiple=COIN;
                        if(multiple <= 0)
                        {
                            multiple=1;
                        }
                        asset_entry.push_back(Pair("name", Value::null));
                        asset_entry.push_back(Pair("issuetxid", Value::null));
                        asset_entry.push_back(Pair("assetref", Value::null));
                        asset_entry.push_back(Pair("qty", (double)it_asset->second/(double)multiple));                        
                    }
                    assets.push_back(asset_entry);
                }
                if(assets.size())
                {
                    a.value_.get_array()[j].get_obj().push_back(Pair("assets", assets));
                }
                Array addresses;
                for (map<uint160,uint32_t>::const_iterator it_address = InputAddresses[j].begin(); it_address != InputAddresses[j].end(); ++it_address) 
                {
                    if(it_address->second & MC_SFL_IS_SCRIPTHASH)
                    {
                        addresses.push_back(CBitcoinAddress(CScriptID(it_address->first)).ToString());
                    }
                    else
                    {
                        addresses.push_back(CBitcoinAddress(CKeyID(it_address->first)).ToString());                        
                    }
                }                
                if(addresses.size())
                {
                    a.value_.get_array()[j].get_obj().push_back(Pair("addresses", addresses));
                }
            }
        }
        if(a.name_ == "vout")
        {          
            for(int i=0;i<(int)a.value_.get_array().size();i++)
            {
                if(tx.vout[i].nValue > 0)
                {
                    Object asset_entry;
                    bool found=false;
                    int64_t multiple=COIN;
                    if(multiple <= 0)
                    {
                        multiple=1;
                    }
                    asset_entry.push_back(Pair("name", Value::null));
                    asset_entry.push_back(Pair("issuetxid", Value::null));
                    asset_entry.push_back(Pair("assetref", Value::null));
                    asset_entry.push_back(Pair("qty", (double)tx.vout[i].nValue/(double)multiple));                        
                    BOOST_FOREACH(Pair& b, a.value_.get_array()[i].get_obj()) 
                    {
                        if(b.name_ == "assets")
                        {            
                            b.value_.get_array().push_back(asset_entry);
                            found=true;
                        }
                    }
                    if(!found)
                    {
                        Array assets;
                        assets.push_back(asset_entry);
                        a.value_.get_array()[i].get_obj().push_back(Pair("assets", assets));
                    }
                }
            
                a.value_.get_array()[i].get_obj().push_back(Pair("tags", TagEntry(OutputScriptTags[i],false)));
                
                COutPoint txout=COutPoint(hash,i);
                const unsigned char *ptr;
                uint160 subkey_hash160;

                ptr=(unsigned char*)&txout;
                subkey_hash160=Hash160(ptr,ptr+sizeof(COutPoint));
                memcpy(entity.m_EntityID,&subkey_hash160,MC_TDB_ENTITY_ID_SIZE);
                entity.m_EntityType=entStat.m_Entity.m_EntityType | MC_TET_SUBKEY;    
                entity_rows=mc_gState->m_TmpRPCBuffers[rpc_slot]->m_RpcEntityRows;
                entity_rows->Clear();
                WRPCheckWalletError(pwalletTxsMain->WRPGetList(&entity,entStat.m_Generation,1,1,entity_rows),entity.m_EntityType,"",&errCode,&strError);
                Value redeem_entry=Value::null;
                if(entity_rows->GetCount())
                {
                    Object entry;

                    mc_TxEntityRow *lpEntTx;
                    lpEntTx=(mc_TxEntityRow*)entity_rows->GetRow(0);
                    uint256 hash;

                    memcpy(&hash,lpEntTx->m_TxId,MC_TDB_TXID_SIZE);                
                    entry.push_back(Pair("txid", hash.ToString()));
                    entry.push_back(Pair("vin", (int)lpEntTx->m_Flags));   
                    
                    redeem_entry=entry;                    
                }
                a.value_.get_array()[i].get_obj().push_back(Pair("redeem", redeem_entry));
            }
        }
    }

exitlbl:
                
    if(fWRPLocked)
    {
        pwalletTxsMain->WRPReadUnLock();
    }

    if(strError.size())
    {
        throw JSONRPCError(errCode, strError);            
    }
                
    return result;
}

Value explorerlistaddresses(const json_spirit::Array& params, bool fHelp)
{
    if (fHelp || params.size() > 4)
        throw runtime_error("Help message not found\n");
    
    if((mc_gState->m_WalletMode & MC_WMD_EXPLORER_MASK) == 0)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "Explorer APIs are not enabled. To enable them, please run \"multichaind -explorersupport=1 -rescan\" ");        
    }   
           
    mc_TxEntity entity;
    mc_TxEntityStat entStat;
    int errCode;
    string strError;
    Value retArray;
    

    string mode="list";

    int flag=0;
        
    if (params.size() > 1)    
    {
        if(paramtobool_or_flag(params[1],&flag))
        {
            mode="all";            
        }
    }
        
    int count,start;
    count=2147483647;
    if (params.size() > 2)    
    {
        count=paramtoint(params[2],true,0,"Invalid count");
    }
    start=-count;
    if (params.size() > 3)    
    {
        start=paramtoint(params[3],false,0,"Invalid start");
    }
    
    entStat.Zero();
    entStat.m_Entity.m_EntityType=MC_TET_EXP_TX_PUBLISHER;                
    entStat.m_Entity.m_EntityType |= MC_TET_CHAINPOS;
    
    vector<string> inputStrings;
    vector<mc_TxEntity> inputEntities;
    
    if(params.size() > 0)
    {
        inputStrings=ParseStringList(params[0]);
        if(inputStrings.size() == 0)
        {
            return retArray;
        }
    }
    
    pwalletTxsMain->WRPReadLock();
    if(!pwalletTxsMain->WRPFindEntity(&entStat))
    {
        errCode=RPC_NOT_SUBSCRIBED;
        strError="Not subscribed to this service";
        goto exitlbl;
    }

    if( (inputStrings.size() != 1) || (inputStrings[0] != "*") )
    {
        for(int is=0;is<(int)inputStrings.size();is++)
        {
            string str=inputStrings[is];
            entity.Zero();

            WRPSubKeyEntityFromExpAddress(str,entStat,&entity,false,&errCode,&strError);
            if(strError.size())
            {
                goto exitlbl;
            }
            inputEntities.push_back(entity);
        }        
    }
    
    if(flag == 2)
    {
        pwalletTxsMain->WRPReadUnLock();
        
        if(inputStrings.size())
        {
            return (int)inputStrings.size();
        }
        
        return pwalletTxsMain->WRPGetListSize(&entStat.m_Entity,entStat.m_Generation,NULL);
    }
    
    
    retArray=explorerlistmap_operation(&(entStat.m_Entity),inputEntities,inputStrings,count,start,mode,&errCode,&strError);        

exitlbl:
                
    pwalletTxsMain->WRPReadUnLock();

    if(strError.size())
    {
        throw JSONRPCError(errCode, strError);            
    }
    
    return retArray;
}

Value explorerlistaddresstransactions(const json_spirit::Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 4)
        throw runtime_error("Help message not found\n");

    if((mc_gState->m_WalletMode & MC_WMD_EXPLORER_MASK) == 0)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "Explorer APIs are not enabled. To enable them, please run \"multichaind -explorersupport=1 -rescan\" ");        
    }   
           
    mc_TxEntityStat entStat;
    mc_TxEntity entity;
    int errCode;
    string strError;
    vector <mc_QueryCondition> conditions;
    Array retArray;
    mc_Buffer *entity_rows=NULL;
    

    int count,start;
    bool verbose=false;
    
    int flag=0;
    
    if (params.size() > 1)    
    {
        verbose=paramtobool_or_flag(params[1],&flag);
    }

    count=10;
    if (params.size() > 2)    
    {
        count=paramtoint(params[2],true,0,"Invalid count");
    }
    start=-count;
    if (params.size() > 3)    
    {
        start=paramtoint(params[3],false,0,"Invalid start");
    }

    entStat.Zero();
    entStat.m_Entity.m_EntityType=MC_TET_EXP_TX_PUBLISHER;
    entStat.m_Entity.m_EntityType |= MC_TET_CHAINPOS;
    
    bool fWRPLocked=false;
    int chain_height; 
    int rpc_slot=GetRPCSlot();
    if(rpc_slot < 0)
    {
        errCode=RPC_INTERNAL_ERROR;
        strError="Couldn't find RPC Slot";
        goto exitlbl;
    }
    
    fWRPLocked=true;
    pwalletTxsMain->WRPReadLock();
    if(!pwalletTxsMain->WRPFindEntity(&entStat))
    {
        errCode=RPC_NOT_SUBSCRIBED;
        strError="Not subscribed to this service";
        goto exitlbl;
    }

    WRPSubKeyEntityFromExpAddress(params[0].get_str(),entStat,&entity,false,&errCode,&strError);
    if(strError.size())
    {
        goto exitlbl;
    }
    
    entity_rows=mc_gState->m_TmpRPCBuffers[rpc_slot]->m_RpcEntityRows;
    entity_rows->Clear();
    
    if(flag == 2)
    {
        pwalletTxsMain->WRPReadUnLock();
        return pwalletTxsMain->WRPGetListSize(&entity,entStat.m_Generation,NULL);
    }
        
    mc_AdjustStartAndCount(&count,&start,pwalletTxsMain->WRPGetListSize(&entity,entStat.m_Generation,NULL));
    
    WRPCheckWalletError(pwalletTxsMain->WRPGetList(&entity,entStat.m_Generation,start+1,count,entity_rows),entity.m_EntityType,"",&errCode,&strError);
    if(strError.size())
    {
        goto exitlbl;
    }
    
    chain_height=chainActive.Height();
    for(int i=0;i<entity_rows->GetCount();i++)
    {
        mc_TxEntityRow *lpEntTx;
        lpEntTx=(mc_TxEntityRow*)entity_rows->GetRow(i);
        uint256 hash;
        uint64_t tag=lpEntTx->m_Flags;
        
        if(tag & MC_MTX_TAG_EXTENDED_TAGS)
        {
            int err;
            string tag_str=pwalletTxsMain->WRPGetSubKey(lpEntTx->m_TxId,NULL,&err);
            if(tag_str.size() == MC_MTX_TAG_EXTENSION_TOTAL_SIZE)
            {
                memcpy(&hash,tag_str.c_str(),sizeof(uint256));
                tag=mc_GetLE((unsigned char *)tag_str.c_str()+sizeof(uint256),sizeof(uint64_t));
            }                        
        }
        else
        {
            memcpy(&hash,lpEntTx->m_TxId,MC_TDB_TXID_SIZE);                
            hash=mc_ExplorerTxIDHashMap(hash);
        }

        Object entry;
        
        entry.push_back(Pair("txid", hash.ToString()));
        entry.push_back(Pair("tags", TagEntry(tag,true)));
        int block=lpEntTx->m_Block;
        AddBlockInfo(entry,block,chain_height);                        
        retArray.push_back(entry);                                
    }
    
exitlbl:
                
    if(fWRPLocked)
    {
        pwalletTxsMain->WRPReadUnLock();
    }

    if(strError.size())
    {
        throw JSONRPCError(errCode, strError);            
    }
    
    
    return retArray;
}

Value explorerlistblocktransactions(const json_spirit::Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 4)
        throw runtime_error("Help message not found\n");

    if((mc_gState->m_WalletMode & MC_WMD_EXPLORER_MASK) == 0)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "Explorer APIs are not enabled. To enable them, please run \"multichaind -explorersupport=1 -rescan\" ");        
    }   
               
    mc_TxEntityStat entStat;
    mc_TxEntity entity;
    int errCode;
    string strError;
    vector <mc_QueryCondition> conditions;
    Array retArray;
    mc_Buffer *entity_rows=NULL;
    
    uint160 subkey_hash=0;
    uint32_t block_key=params[0].get_int();
    
    
    memcpy(&subkey_hash, &block_key,sizeof(uint32_t));


    int count,start;
    bool verbose=false;
    
    int flag=0;
    
    if (params.size() > 1)    
    {
        verbose=paramtobool_or_flag(params[1],&flag);
    }

    count=10;
    if (params.size() > 2)    
    {
        count=paramtoint(params[2],true,0,"Invalid count");
    }
    start=-count;
    if (params.size() > 3)    
    {
        start=paramtoint(params[3],false,0,"Invalid start");
    }

    entStat.Zero();
    entStat.m_Entity.m_EntityType=MC_TET_EXP_TX_KEY;
    entStat.m_Entity.m_EntityType |= MC_TET_CHAINPOS;
    entity.Zero();
    memcpy(entity.m_EntityID,&subkey_hash,MC_TDB_ENTITY_ID_SIZE);
    entity.m_EntityType=entStat.m_Entity.m_EntityType | MC_TET_SUBKEY;    
    
    bool fWRPLocked=false;
    int chain_height; 
    int rpc_slot=GetRPCSlot();
    if(rpc_slot < 0)
    {
        errCode=RPC_INTERNAL_ERROR;
        strError="Couldn't find RPC Slot";
        goto exitlbl;
    }
    
    fWRPLocked=true;
    pwalletTxsMain->WRPReadLock();
    if(!pwalletTxsMain->WRPFindEntity(&entStat))
    {
        errCode=RPC_NOT_SUBSCRIBED;
        strError="Not subscribed to this service";
        goto exitlbl;
    }
    
    entity_rows=mc_gState->m_TmpRPCBuffers[rpc_slot]->m_RpcEntityRows;
    entity_rows->Clear();
    
    if(flag == 2)
    {
        pwalletTxsMain->WRPReadUnLock();
        return pwalletTxsMain->WRPGetListSize(&entity,entStat.m_Generation,NULL);
    }
    
    mc_AdjustStartAndCount(&count,&start,pwalletTxsMain->WRPGetListSize(&entity,entStat.m_Generation,NULL));
    
    WRPCheckWalletError(pwalletTxsMain->WRPGetList(&entity,entStat.m_Generation,start+1,count,entity_rows),entity.m_EntityType,"",&errCode,&strError);
    if(strError.size())
    {
        goto exitlbl;
    }
    
    chain_height=chainActive.Height();
    for(int i=0;i<entity_rows->GetCount();i++)
    {
        mc_TxEntityRow *lpEntTx;
        lpEntTx=(mc_TxEntityRow*)entity_rows->GetRow(i);
        uint256 hash;
        uint64_t tag=lpEntTx->m_Flags;
        
        if(tag & MC_MTX_TAG_EXTENDED_TAGS)
        {
            int err;
            string tag_str=pwalletTxsMain->WRPGetSubKey(lpEntTx->m_TxId,NULL,&err);
            if(tag_str.size() == MC_MTX_TAG_EXTENSION_TOTAL_SIZE)
            {
                memcpy(&hash,tag_str.c_str(),sizeof(uint256));
                tag=mc_GetLE((unsigned char *)tag_str.c_str()+sizeof(uint256),sizeof(uint64_t));
            }                        
        }
        else
        {
            memcpy(&hash,lpEntTx->m_TxId,MC_TDB_TXID_SIZE);                
            hash=mc_ExplorerTxIDHashMap(hash);
        }

        Object entry;
        
        entry.push_back(Pair("txid", hash.ToString()));
        entry.push_back(Pair("tags", TagEntry(tag,true)));
        int block=lpEntTx->m_Block;
        AddBlockInfo(entry,block,chain_height);                        
        retArray.push_back(entry);                                
    }
    
exitlbl:
                
    if(fWRPLocked)
    {
        pwalletTxsMain->WRPReadUnLock();
    }

    if(strError.size())
    {
        throw JSONRPCError(errCode, strError);            
    }
    
    
    return retArray;
}

Value explorerlistredeemtransactions(const json_spirit::Array& params, bool fHelp)
{
    if (fHelp || params.size() != 2)
        throw runtime_error("Help message not found\n");

    if((mc_gState->m_WalletMode & MC_WMD_EXPLORER_MASK) == 0)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "Explorer APIs are not enabled. To enable them, please run \"multichaind -explorersupport=1 -rescan\" ");        
    }   
           
    mc_TxEntityStat entStat;
    mc_TxEntity entity;
    int errCode;
    string strError;
    vector <mc_QueryCondition> conditions;
    Array retArray;
    mc_Buffer *entity_rows=NULL;
    
    uint256 hash = ParseHashV(params[0], "txid");
    int n=params[1].get_int();
    
    entStat.Zero();
    entStat.m_Entity.m_EntityType=MC_TET_EXP_REDEEM_KEY;
    entStat.m_Entity.m_EntityType |= MC_TET_CHAINPOS;
    
    bool fWRPLocked=false;
    int rpc_slot=GetRPCSlot();
    if(rpc_slot < 0)
    {
        errCode=RPC_INTERNAL_ERROR;
        strError="Couldn't find RPC Slot";
        goto exitlbl;
    }
    
    fWRPLocked=true;
    pwalletTxsMain->WRPReadLock();
    if(!pwalletTxsMain->WRPFindEntity(&entStat))
    {
        errCode=RPC_NOT_SUBSCRIBED;
        strError="Not subscribed to this service";
        goto exitlbl;
    }

    for(int i=0;i<n;i++)
    {
        COutPoint txout=COutPoint(hash,i);
        const unsigned char *ptr;
        uint160 subkey_hash160;

        ptr=(unsigned char*)&txout;
        subkey_hash160=Hash160(ptr,ptr+sizeof(COutPoint));
        memcpy(entity.m_EntityID,&subkey_hash160,MC_TDB_ENTITY_ID_SIZE);
        entity.m_EntityType=entStat.m_Entity.m_EntityType | MC_TET_SUBKEY;    
        entity_rows=mc_gState->m_TmpRPCBuffers[rpc_slot]->m_RpcEntityRows;
        entity_rows->Clear();
        WRPCheckWalletError(pwalletTxsMain->WRPGetList(&entity,entStat.m_Generation,1,1,entity_rows),entity.m_EntityType,"",&errCode,&strError);
        if(entity_rows->GetCount())
        {
            Object entry;

            mc_TxEntityRow *lpEntTx;
            lpEntTx=(mc_TxEntityRow*)entity_rows->GetRow(0);
            uint256 hash;

            memcpy(&hash,lpEntTx->m_TxId,MC_TDB_TXID_SIZE);                
            entry.push_back(Pair("txid", hash.ToString()));
            entry.push_back(Pair("vout", (int)lpEntTx->m_Flags));            
            retArray.push_back(entry);                                
        }
        else
        {
            retArray.push_back(Value::null);                                
        }
    }
    
exitlbl:
                
    if(fWRPLocked)
    {
        pwalletTxsMain->WRPReadUnLock();
    }

    if(strError.size())
    {
        throw JSONRPCError(errCode, strError);            
    }
    
    
    return retArray;
}

Value explorerlistaddressstreams(const json_spirit::Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 4)
        throw runtime_error("Help message not found\n");

    if((mc_gState->m_WalletMode & MC_WMD_EXPLORER_MASK) == 0)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "Explorer APIs are not enabled. To enable them, please run \"multichaind -explorersupport=1 -rescan\" ");        
    }   
           
    mc_TxEntityStat entStat;
    mc_TxEntity entity;
    int errCode;
    string strError;
    vector <mc_QueryCondition> conditions;
    Array retArray;
    mc_Buffer *entity_rows=NULL;
    

    int count,start;
    bool verbose=false;
    
    int flag=0;
    
    if (params.size() > 1)    
    {
        verbose=paramtobool_or_flag(params[1],&flag);
    }

    count=10;
    if (params.size() > 2)    
    {
        count=paramtoint(params[2],true,0,"Invalid count");
    }
    start=-count;
    if (params.size() > 3)    
    {
        start=paramtoint(params[3],false,0,"Invalid start");
    }

    entStat.Zero();
    entStat.m_Entity.m_EntityType=MC_TET_EXP_ADDRESS_STREAMS_KEY;
    entStat.m_Entity.m_EntityType |= MC_TET_CHAINPOS;
    
    bool fWRPLocked=false;
    int rpc_slot=GetRPCSlot();
    if(rpc_slot < 0)
    {
        errCode=RPC_INTERNAL_ERROR;
        strError="Couldn't find RPC Slot";
        goto exitlbl;
    }
    
    fWRPLocked=true;
    pwalletTxsMain->WRPReadLock();
    if(!pwalletTxsMain->WRPFindEntity(&entStat))
    {
        errCode=RPC_NOT_SUBSCRIBED;
        strError="Not subscribed to this service";
        goto exitlbl;
    }

    WRPSubKeyEntityFromExpAddress(params[0].get_str(),entStat,&entity,false,&errCode,&strError);
    if(strError.size())
    {
        goto exitlbl;
    }
    
    entity_rows=mc_gState->m_TmpRPCBuffers[rpc_slot]->m_RpcEntityRows;
    entity_rows->Clear();
    
    if(flag == 2)
    {
        pwalletTxsMain->WRPReadUnLock();
        return pwalletTxsMain->WRPGetListSize(&entity,entStat.m_Generation,NULL);
    }
    
    mc_AdjustStartAndCount(&count,&start,pwalletTxsMain->WRPGetListSize(&entity,entStat.m_Generation,NULL));
    
    WRPCheckWalletError(pwalletTxsMain->WRPGetList(&entity,entStat.m_Generation,start+1,count,entity_rows),entity.m_EntityType,"",&errCode,&strError);
    if(strError.size())
    {
        goto exitlbl;
    }
    
    for(int i=0;i<entity_rows->GetCount();i++)
    {
        mc_TxEntityRow *lpEntTx;
        lpEntTx=(mc_TxEntityRow*)entity_rows->GetRow(i);
        uint256 hash;

        mc_EntityDetails entity_details;
        mc_gState->m_Assets->FindEntityByShortTxID(&entity_details,lpEntTx->m_TxId);
        
        
        Object entry;
        
        entry=StreamEntry(entity_details.GetTxID(),0x02);
        if(verbose)
        {
            entStat.Zero();
            memcpy(&entStat,entity_details.GetTxID()+MC_AST_SHORT_TXID_OFFSET,MC_AST_SHORT_TXID_SIZE);
            entStat.m_Entity.m_EntityType=MC_TET_STREAM_PUBLISHER;                
            entStat.m_Entity.m_EntityType |= MC_TET_CHAINPOS;
            if(!pwalletTxsMain->WRPFindEntity(&entStat))
            {
                entry.push_back(Pair("items", Value::null));
                entry.push_back(Pair("confirmed", Value::null));
            }
            else
            {
                entity.Zero();

                WRPSubKeyEntityFromPublisher(params[0].get_str(),entStat,&entity,false,&errCode,&strError);
                int total,confirmed;
                total=pwalletTxsMain->WRPGetListSize(&entity,entStat.m_Generation,&confirmed);
                entry.push_back(Pair("items", total));
                entry.push_back(Pair("confirmed", confirmed));                
            }            
        }
        retArray.push_back(entry);                                
    }
    
exitlbl:
                
    if(fWRPLocked)
    {
        pwalletTxsMain->WRPReadUnLock();
    }

    if(strError.size())
    {
        throw JSONRPCError(errCode, strError);            
    }
    
    
    return retArray;
}



Value explorerlistaddressassets(const json_spirit::Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 4)
        throw runtime_error("Help message not found\n");

    if((mc_gState->m_WalletMode & MC_WMD_EXPLORER_MASK) == 0)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "Explorer APIs are not enabled. To enable them, please run \"multichaind -explorersupport=1 -rescan\" ");        
    }   
           
    mc_TxEntityStat entStat;
    mc_TxEntity entity;
    int errCode;
    string strError;
    vector <mc_QueryCondition> conditions;
    Array retArray;
    mc_Buffer *entity_rows=NULL;
    

    int count,start;
    bool verbose=false;
    
    int flag=0;
    
    if (params.size() > 1)    
    {
        verbose=paramtobool_or_flag(params[1],&flag);
    }

    count=10;
    if (params.size() > 2)    
    {
        count=paramtoint(params[2],true,0,"Invalid count");
    }
    start=-count;
    if (params.size() > 3)    
    {
        start=paramtoint(params[3],false,0,"Invalid start");
    }

    entStat.Zero();
    entStat.m_Entity.m_EntityType=MC_TET_EXP_ADDRESS_ASSETS_KEY;
    entStat.m_Entity.m_EntityType |= MC_TET_CHAINPOS;
    
    bool fWRPLocked=false;
    int chain_height; 
    int rpc_slot=GetRPCSlot();
    if(rpc_slot < 0)
    {
        errCode=RPC_INTERNAL_ERROR;
        strError="Couldn't find RPC Slot";
        goto exitlbl;
    }
    
    fWRPLocked=true;
    pwalletTxsMain->WRPReadLock();
    if(!pwalletTxsMain->WRPFindEntity(&entStat))
    {
        errCode=RPC_NOT_SUBSCRIBED;
        strError="Not subscribed to this service";
        goto exitlbl;
    }

    WRPSubKeyEntityFromExpAddress(params[0].get_str(),entStat,&entity,false,&errCode,&strError);
    if(strError.size())
    {
        goto exitlbl;
    }
    
    entity_rows=mc_gState->m_TmpRPCBuffers[rpc_slot]->m_RpcEntityRows;
    entity_rows->Clear();
    
    if(flag == 2)
    {
        pwalletTxsMain->WRPReadUnLock();
        return pwalletTxsMain->WRPGetListSize(&entity,entStat.m_Generation,NULL);
    }
    
    mc_AdjustStartAndCount(&count,&start,pwalletTxsMain->WRPGetListSize(&entity,entStat.m_Generation,NULL));
    
    WRPCheckWalletError(pwalletTxsMain->WRPGetList(&entity,entStat.m_Generation,start+1,count,entity_rows),entity.m_EntityType,"",&errCode,&strError);
    if(strError.size())
    {
        goto exitlbl;
    }
    
    chain_height=chainActive.Height();
    for(int i=0;i<entity_rows->GetCount();i++)
    {
        mc_TxEntityRow *lpEntTx;
        mc_TxEntityRow erow;
        lpEntTx=(mc_TxEntityRow*)entity_rows->GetRow(i);
        uint256 hash;

        mc_EntityDetails entity_details;
        entity_details.Zero();
        
//        memcpy(&hash,lpEntTx->m_TxId,MC_TDB_TXID_SIZE);                
        
        erow.Zero();
        int64_t quantity=0;
        mc_TxEntity subkey_entity;
        uint160 balance_subkey_hash160;
        mc_GetCompoundHash160(&balance_subkey_hash160,entity.m_EntityID,lpEntTx->m_TxId);
        subkey_entity.Zero();
        memcpy(subkey_entity.m_EntityID,&balance_subkey_hash160,MC_TDB_ENTITY_ID_SIZE);
        subkey_entity.m_EntityType=MC_TET_SUBKEY_EXP_BALANCE_DETAILS_KEY | MC_TET_CHAINPOS;
        
        if(pwalletTxsMain->WRPGetLastItem(&subkey_entity,entStat.m_Generation,&erow) == 0)
        {
            int err;
            mc_TxAssetBalanceDetails balance_details;
            string assets_str=pwalletTxsMain->WRPGetSubKey(erow.m_TxId,NULL,&err);
            if(assets_str.size() == sizeof(mc_TxAssetBalanceDetails))
            {
                memcpy(&balance_details,assets_str.c_str(),assets_str.size());
                quantity=balance_details.m_Balance;                
            }            
        }
        
        Object entry;
        
        if(*(uint160*)(lpEntTx->m_TxId) != 0)
        {
            mc_gState->m_Assets->FindEntityByShortTxID(&entity_details,lpEntTx->m_TxId);
            entry=AssetEntry(entity_details.GetTxID(),quantity,0x04);
        }
        else
        {
            int64_t multiple=COIN;
            if(multiple <= 0)
            {
                multiple=1;
            }
            entry.push_back(Pair("name", Value::null));
            entry.push_back(Pair("issuetxid", Value::null));
            entry.push_back(Pair("assetref", Value::null));
            entry.push_back(Pair("qty", (double)quantity/(double)multiple));
//            entry.push_back(Pair("raw", quantity));
        }
/*        
        entry.push_back(Pair("txid", hash.ToString()));
        int block=lpEntTx->m_Block;
        if( (block < 0) || (block > chain_height))
        {
            entry.push_back(Pair("block", Value::null));
        }
        else
        {
            entry.push_back(Pair("block", block));            
        }
 */ 
        retArray.push_back(entry);                                
    }
    
exitlbl:
                
    if(fWRPLocked)
    {
        pwalletTxsMain->WRPReadUnLock();
    }

    if(strError.size())
    {
        throw JSONRPCError(errCode, strError);            
    }
    
    
    return retArray;
}

Value explorerlistassetaddresses(const json_spirit::Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 4)
        throw runtime_error("Help message not found\n");

    if((mc_gState->m_WalletMode & MC_WMD_EXPLORER_MASK) == 0)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "Explorer APIs are not enabled. To enable them, please run \"multichaind -explorersupport=1 -rescan\" ");        
    }   
           
    mc_TxEntityStat entStat;
    mc_TxEntity entity;
    int errCode;
    string strError;
    vector <mc_QueryCondition> conditions;
    Array retArray;
    mc_Buffer *entity_rows=NULL;
    int64_t multiple=COIN;
    if(multiple <= 0)
    {
        multiple=1;
    }
    mc_EntityDetails entity_details;
    
    entity_details.Zero();
    if(params[0].type() == str_type)
    {        
        if(params[0].get_str().size())
        {
            ParseEntityIdentifier(params[0],&entity_details, MC_ENT_TYPE_ASSET);           
            multiple=entity_details.GetAssetMultiple();
        }
    }
    else
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid asset identifier");        
    }

    int count,start;
    bool verbose=false;
    
    int flag=0;
    
    if (params.size() > 1)    
    {
        verbose=paramtobool_or_flag(params[1],&flag);
    }

    count=10;
    if (params.size() > 2)    
    {
        count=paramtoint(params[2],true,0,"Invalid count");
    }
    start=-count;
    if (params.size() > 3)    
    {
        start=paramtoint(params[3],false,0,"Invalid start");
    }

    entStat.Zero();
    entStat.m_Entity.m_EntityType=MC_TET_EXP_ASSET_ADDRESSES_KEY;
    entStat.m_Entity.m_EntityType |= MC_TET_CHAINPOS;
    
    bool fWRPLocked=false;
    int rpc_slot=GetRPCSlot();
    if(rpc_slot < 0)
    {
        errCode=RPC_INTERNAL_ERROR;
        strError="Couldn't find RPC Slot";
        goto exitlbl;
    }
    
    fWRPLocked=true;
    pwalletTxsMain->WRPReadLock();
    if(!pwalletTxsMain->WRPFindEntity(&entStat))
    {
        errCode=RPC_NOT_SUBSCRIBED;
        strError="Not subscribed to this service";
        goto exitlbl;
    }

    WRPSubKeyEntityFromExpAsset(&entity_details,entStat,&entity,false,&errCode,&strError);
    if(strError.size())
    {
        goto exitlbl;
    }
    entity_rows=mc_gState->m_TmpRPCBuffers[rpc_slot]->m_RpcEntityRows;
    entity_rows->Clear();
    
    if(flag == 2)
    {
        pwalletTxsMain->WRPReadUnLock();
        return pwalletTxsMain->WRPGetListSize(&entity,entStat.m_Generation,NULL);
    }
    
    mc_AdjustStartAndCount(&count,&start,pwalletTxsMain->WRPGetListSize(&entity,entStat.m_Generation,NULL));
    WRPCheckWalletError(pwalletTxsMain->WRPGetList(&entity,entStat.m_Generation,start+1,count,entity_rows),entity.m_EntityType,"",&errCode,&strError);
    if(strError.size())
    {
        goto exitlbl;
    }
    
    for(int i=0;i<entity_rows->GetCount();i++)
    {
        mc_TxEntityRow *lpEntTx;
        mc_TxEntityRow erow;
        lpEntTx=(mc_TxEntityRow*)entity_rows->GetRow(i);
        uint256 hash;
        
        string address;
        if(lpEntTx->m_Flags & MC_SFL_IS_SCRIPTHASH)
        {            
            CScriptID ScriptID;
            memcpy(&ScriptID,lpEntTx->m_TxId,sizeof(uint160));
            address=CBitcoinAddress(ScriptID).ToString();
        }
        else
        {
            CKeyID KeyID;
            memcpy(&KeyID,lpEntTx->m_TxId,sizeof(uint160));            
            address=CBitcoinAddress(KeyID).ToString();
        }
        
        uint160 asset_hash=0;
        memcpy(&asset_hash, entity_details.GetTxID()+MC_AST_SHORT_TXID_OFFSET,MC_AST_SHORT_TXID_SIZE);
        
        erow.Zero();
        int64_t quantity=0;
        mc_TxEntity subkey_entity;
        uint160 balance_subkey_hash160;
        mc_GetCompoundHash160(&balance_subkey_hash160,lpEntTx->m_TxId,&asset_hash);
        subkey_entity.Zero();
        memcpy(subkey_entity.m_EntityID,&balance_subkey_hash160,MC_TDB_ENTITY_ID_SIZE);
        subkey_entity.m_EntityType=MC_TET_SUBKEY_EXP_BALANCE_DETAILS_KEY | MC_TET_CHAINPOS;
        
        if(pwalletTxsMain->WRPGetLastItem(&subkey_entity,entStat.m_Generation,&erow) == 0)
        {
            int err;
            mc_TxAssetBalanceDetails balance_details;
            string assets_str=pwalletTxsMain->WRPGetSubKey(erow.m_TxId,NULL,&err);
            if(assets_str.size() == sizeof(mc_TxAssetBalanceDetails))
            {
                memcpy(&balance_details,assets_str.c_str(),assets_str.size());
                quantity=balance_details.m_Balance;                
            }            
        }
        
        Object entry;
        
        entry.push_back(Pair("address", address));
        entry.push_back(Pair("qty", (double)quantity/multiple));
        entry.push_back(Pair("raw", quantity));                                    
        
        retArray.push_back(entry);                                
    }
    
exitlbl:
                
    if(fWRPLocked)
    {
        pwalletTxsMain->WRPReadUnLock();
    }

    if(strError.size())
    {
        throw JSONRPCError(errCode, strError);            
    }
    
    
    return retArray;
}

Value explorerlistaddressassettransactions(const json_spirit::Array& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 5)
        throw runtime_error("Help message not found\n");

    if((mc_gState->m_WalletMode & MC_WMD_EXPLORER_MASK) == 0)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "Explorer APIs are not enabled. To enable them, please run \"multichaind -explorersupport=1 -rescan\" ");        
    }   
           
    mc_TxEntityStat entStat;
    mc_TxEntity entity;
    int errCode;
    string strError;
    vector <mc_QueryCondition> conditions;
    Array retArray;
    mc_Buffer *entity_rows=NULL;
    
    uint160 asset_subkey_hash=0;
    uint160 balance_subkey_hash160=0;
    uint160 address_subkey_hash=0;
    CBitcoinAddress address(params[1].get_str());
    if (!address.IsValid())
    {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");        
    }
    
    CTxDestination dest=address.Get();
    CKeyID *lpKeyID=boost::get<CKeyID> (&dest);
    CScriptID *lpScriptID=boost::get<CScriptID> (&dest);
    
    if ((lpKeyID == NULL) && (lpScriptID == NULL) )
    {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");        
    }

    if(lpKeyID)
    {
        memcpy(&address_subkey_hash,lpKeyID,MC_TDB_ENTITY_ID_SIZE);
    }
    else
    {
        memcpy(&address_subkey_hash,lpScriptID,MC_TDB_ENTITY_ID_SIZE);
    }

    int64_t multiple=COIN;
    if(multiple <= 0)
    {
        multiple=1;
    }
    mc_EntityDetails entity_details;
    
    entity_details.Zero();
    if(params[0].type() == str_type)
    {        
        if(params[0].get_str().size())
        {
            ParseEntityIdentifier(params[0],&entity_details, MC_ENT_TYPE_ASSET);           
            multiple=entity_details.GetAssetMultiple();
        }
    }
    else
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid asset identifier");        
    }
    
    memcpy(&asset_subkey_hash, entity_details.GetTxID()+MC_AST_SHORT_TXID_OFFSET,MC_AST_SHORT_TXID_SIZE);
    
    int count,start;
    bool verbose=false;
    
    int flag=0;
    
    if (params.size() > 2)    
    {
        verbose=paramtobool_or_flag(params[2],&flag);
    }

 
    count=10;
    if (params.size() > 3)    
    {
        count=paramtoint(params[3],true,0,"Invalid count");
    }
    start=-count;
    if (params.size() > 4)    
    {
        start=paramtoint(params[4],false,0,"Invalid start");
    }

    entStat.Zero();
    entStat.m_Entity.m_EntityType=MC_TET_EXP_BALANCE_DETAILS_KEY;
    entStat.m_Entity.m_EntityType |= MC_TET_CHAINPOS;
    
    bool fWRPLocked=false;
    int chain_height; 
    int rpc_slot=GetRPCSlot();
    if(rpc_slot < 0)
    {
        errCode=RPC_INTERNAL_ERROR;
        strError="Couldn't find RPC Slot";
        goto exitlbl;
    }
    
    fWRPLocked=true;
    pwalletTxsMain->WRPReadLock();
    if(!pwalletTxsMain->WRPFindEntity(&entStat))
    {
        errCode=RPC_NOT_SUBSCRIBED;
        strError="Not subscribed to this service";
        goto exitlbl;
    }

    
    
    mc_GetCompoundHash160(&balance_subkey_hash160,&address_subkey_hash,&asset_subkey_hash);
    entity.Zero();
    memcpy(entity.m_EntityID,&balance_subkey_hash160,MC_TDB_ENTITY_ID_SIZE);
    entity.m_EntityType=MC_TET_SUBKEY_EXP_BALANCE_DETAILS_KEY | MC_TET_CHAINPOS;
    
    if(strError.size())
    {
        goto exitlbl;
    }
    
    entity_rows=mc_gState->m_TmpRPCBuffers[rpc_slot]->m_RpcEntityRows;
    entity_rows->Clear();
    
    if(flag == 2)
    {
        pwalletTxsMain->WRPReadUnLock();
        return pwalletTxsMain->WRPGetListSize(&entity,entStat.m_Generation,NULL);
    }
    
    mc_AdjustStartAndCount(&count,&start,pwalletTxsMain->WRPGetListSize(&entity,entStat.m_Generation,NULL));
    
    WRPCheckWalletError(pwalletTxsMain->WRPGetList(&entity,entStat.m_Generation,start+1,count,entity_rows),entity.m_EntityType,"",&errCode,&strError);
    if(strError.size())
    {
        goto exitlbl;
    }
    
    chain_height=chainActive.Height();
    for(int i=0;i<entity_rows->GetCount();i++)
    {
        mc_TxEntityRow *lpEntTx;
        lpEntTx=(mc_TxEntityRow*)entity_rows->GetRow(i);
        
        Object entry;
        int err;
        mc_TxAssetBalanceDetails balance_details;
        string assets_str=pwalletTxsMain->WRPGetSubKey(lpEntTx->m_TxId,NULL,&err);
        if(assets_str.size() == sizeof(mc_TxAssetBalanceDetails))
        {
            memcpy(&balance_details,assets_str.c_str(),assets_str.size());
            entry.push_back(Pair("txid",balance_details.m_TxID.ToString()));
            entry.push_back(Pair("amount", (double)balance_details.m_Amount/multiple));
//            entry.push_back(Pair("raw", quantity));                                    
            //entry.push_back(Pair("amount",balance_details.m_Amount));                                
/*            
            if(balance_details.m_Flags & MC_MTX_TFL_IS_INPUT)
            {
                entry.push_back(Pair("isinput",true));
                entry.push_back(Pair("vin",(int)balance_details.m_Vinout));
                entry.push_back(Pair("amount",-balance_details.m_Amount));                
            }
            else
            {
                entry.push_back(Pair("isinput",false));
                entry.push_back(Pair("vout",(int)balance_details.m_Vinout));
                entry.push_back(Pair("amount",balance_details.m_Amount));                                
            }
 */ 
            entry.push_back(Pair("balance", (double)balance_details.m_Balance/multiple));
//            entry.push_back(Pair("balance",balance_details.m_Balance));                                
        }            
        
        int block=lpEntTx->m_Block;
        AddBlockInfo(entry,block,chain_height);                        
        retArray.push_back(entry);                                
    }
    
exitlbl:
                
    if(fWRPLocked)
    {
        pwalletTxsMain->WRPReadUnLock();
    }

    if(strError.size())
    {
        throw JSONRPCError(errCode, strError);            
    }
    
    
    return retArray;
}
