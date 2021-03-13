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

Value listexpmap_operation(mc_TxEntity *parent_entity,vector<mc_TxEntity>& inputEntities,vector<string>& inputStrings,int count, int start, string mode,int *errCode,string *strError)
{
    mc_TxEntity entity;
    mc_TxEntityStat entStat;
    Array retArray;
    mc_Buffer *entity_rows;
    mc_TxEntityRow erow;
    uint160 subkey_hash;    
    int row,enitity_count;
    
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
                        if( (block < 0) || (block > chain_height))
                        {
                            entry.push_back(Pair("block", Value::null));
                        }
                        else
                        {
                            entry.push_back(Pair("block", block));            
                        }
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

Value listexptxs(const json_spirit::Array& params, bool fHelp)
{
    if (fHelp || params.size() > 4)
        throw runtime_error("Help message not found\n");

    if((mc_gState->m_WalletMode & MC_WMD_EXPLORER) == 0)
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
    
    if (params.size() > 0)    
    {
        verbose=paramtobool(params[0]);
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
    
    mc_AdjustStartAndCount(&count,&start,pwalletTxsMain->WRPGetListSize(&entStat.m_Entity,entStat.m_Generation,NULL));
    
//    CheckWalletError(pwalletTxsMain->GetList(&entStat.m_Entity,start+1,count,entity_rows),entStat.m_Entity.m_EntityType,"");
    WRPCheckWalletError(pwalletTxsMain->WRPGetList(&entStat.m_Entity,entStat.m_Generation,start+1,count,entity_rows),entStat.m_Entity.m_EntityType,"",&errCode,&strError);

    chain_height=chainActive.Height();
    for(int i=0;i<entity_rows->GetCount();i++)
    {
        mc_TxEntityRow *lpEntTx;
        lpEntTx=(mc_TxEntityRow*)entity_rows->GetRow(i);
        uint256 hash;
        
        memcpy(&hash,lpEntTx->m_TxId,MC_TDB_TXID_SIZE);                

        Object entry;
        
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

Value listexpaddresses(const json_spirit::Array& params, bool fHelp)
{
    if (fHelp || params.size() > 5)
        throw runtime_error("Help message not found\n");
    
    if((mc_gState->m_WalletMode & MC_WMD_EXPLORER) == 0)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "Explorer APIs are not enabled. To enable them, please run \"multichaind -explorersupport=1 -rescan\" ");        
    }   
           
    mc_TxEntity entity;
    mc_TxEntityStat entStat;
    int errCode;
    string strError;
    Value retArray;
    

    string mode="list";
    
    if (params.size() > 1)    
    {
        if(paramtobool(params[1]))
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
        strError="Not subscribed to this stream";
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
    
    
    retArray=listexpmap_operation(&(entStat.m_Entity),inputEntities,inputStrings,count,start,mode,&errCode,&strError);        

exitlbl:
                
    pwalletTxsMain->WRPReadUnLock();

    if(strError.size())
    {
        throw JSONRPCError(errCode, strError);            
    }
    
    return retArray;
}

Value listexpaddresstxs(const json_spirit::Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 5)
        throw runtime_error("Help message not found\n");

    if((mc_gState->m_WalletMode & MC_WMD_EXPLORER) == 0)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "Explorer APIs are not enabled. To enable them, please run \"multichaind -explorersupport=1 -rescan\" ");        
    }   
           
    if(params[0].get_str() == "*")
    {
        int count=0;
        Array ext_params;
        BOOST_FOREACH(const Value& value, params)
        {
            if(count != 1)
            {
                ext_params.push_back(value);
            }
            count++;
        }
    
        return listexptxs(ext_params,fHelp);            
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
    
    if (params.size() > 1)    
    {
        verbose=paramtobool(params[1]);
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
        strError="Not subscribed to this stream";
        goto exitlbl;
    }

    WRPSubKeyEntityFromExpAddress(params[0].get_str(),entStat,&entity,false,&errCode,&strError);
    if(strError.size())
    {
        goto exitlbl;
    }
    
    entity_rows=mc_gState->m_TmpRPCBuffers[rpc_slot]->m_RpcEntityRows;
    entity_rows->Clear();
    
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
        
        memcpy(&hash,lpEntTx->m_TxId,MC_TDB_TXID_SIZE);                

        Object entry;
        
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

Value listexpblocktxs(const json_spirit::Array& params, bool fHelp)
{
    return Value::null;    
}
