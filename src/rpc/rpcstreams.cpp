// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2014-2016 The Bitcoin Core developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2017 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.


#include "rpc/rpcwallet.h"

Value createupgradefromcmd(const Array& params, bool fHelp);

void parseStreamIdentifier(Value stream_identifier,mc_EntityDetails *entity)
{
    unsigned char buf[32];
    unsigned char buf_a[MC_AST_ASSET_REF_SIZE];
    unsigned char buf_n[MC_AST_ASSET_REF_SIZE];
    int ret;
    
    if (stream_identifier.type() != null_type && !stream_identifier.get_str().empty())
    {        
        string str=stream_identifier.get_str();
        
        if(AssetRefDecode(buf_a,str.c_str(),str.size()))
        {
            memset(buf_n,0,MC_AST_ASSET_REF_SIZE);
            if(memcmp(buf_a,buf_n,4) == 0)
            {
                unsigned char *root_stream_name;
                int root_stream_name_size;
                root_stream_name=(unsigned char *)mc_gState->m_NetworkParams->GetParam("rootstreamname",&root_stream_name_size);        
                if(mc_gState->m_NetworkParams->IsProtocolMultichain() == 0)
                {
                    root_stream_name_size=0;
                }    
                if( (root_stream_name_size > 1) && (memcmp(buf_a,buf_n,MC_AST_ASSET_REF_SIZE) == 0) )
                {
                    str=strprintf("%s",root_stream_name);
                }
                else
                {
                    throw JSONRPCError(RPC_ENTITY_NOT_FOUND, "Stream with this stream reference not found: "+str);                    
                }
            }
        }
        
        ret=ParseAssetKey(str.c_str(),buf,NULL,NULL,NULL,NULL,MC_ENT_TYPE_STREAM);
        switch(ret)
        {
            case MC_ASSET_KEY_INVALID_TXID:
                throw JSONRPCError(RPC_ENTITY_NOT_FOUND, "Stream with this txid not found: "+str);
                break;
            case MC_ASSET_KEY_INVALID_REF:
                throw JSONRPCError(RPC_ENTITY_NOT_FOUND, "Stream with this stream reference not found: "+str);
                break;
            case MC_ASSET_KEY_INVALID_NAME:
                throw JSONRPCError(RPC_ENTITY_NOT_FOUND, "Stream with this name not found: "+str);
                break;
            case MC_ASSET_KEY_INVALID_SIZE:
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Could not parse stream key: "+str);
                break;
/*                
            case 1:
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Unconfirmed stream: "+str);
                break;
 */ 
        }
    }
    else
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid stream identifier");        
    }
           
    if(entity)
    {
        if(mc_gState->m_Assets->FindEntityByTxID(entity,buf))
        {
            if(entity->GetEntityType() != MC_ENT_TYPE_STREAM)
            {
                throw JSONRPCError(RPC_ENTITY_NOT_FOUND, "Invalid stream identifier, not stream");                        
            }
        }    
    }    
}

Value liststreams(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 4)
        throw runtime_error("Help message not found\n");

    Array results;
    mc_Buffer *streams;
    unsigned char *txid;
    uint32_t output_level;
    
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
    
    streams=NULL;
    
    vector<string> inputStrings;
    if (params.size() > 0 && params[0].type() != null_type && ((params[0].type() != str_type) || (params[0].get_str() !="*" ) ) )
    {        
        if(params[0].type() == str_type)
        {
            inputStrings.push_back(params[0].get_str());
            if(params[0].get_str() == "")
            {
                return results;                
            }
        }
        else
        {
            inputStrings=ParseStringList(params[0]);        
            if(inputStrings.size() == 0)
            {
                return results;
            }
        }
    }
    if(inputStrings.size())
    {
        {
            LOCK(cs_main);
            for(int is=0;is<(int)inputStrings.size();is++)
            {
                string param=inputStrings[is];

                mc_EntityDetails stream_entity;
                parseStreamIdentifier(param,&stream_entity);           

                streams=mc_gState->m_Assets->GetEntityList(streams,stream_entity.GetTxID(),MC_ENT_TYPE_STREAM);
            }
        }
    }
    else
    {        
        {
            LOCK(cs_main);
            streams=mc_gState->m_Assets->GetEntityList(streams,NULL,MC_ENT_TYPE_STREAM);
        }
    }
    
    if(streams == NULL)
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Cannot open entity database");

    output_level=0x1E;
    
    if (params.size() > 1)    
    {
        if(paramtobool(params[1]))
        {
            output_level=0x3E;            
        }
    }
    
    
    int root_stream_name_size;
    mc_gState->m_NetworkParams->GetParam("rootstreamname",&root_stream_name_size);        
    if( (root_stream_name_size <= 1) && (inputStrings.size() == 0) && (mc_gState->m_Features->FixedIn10008() == 0) )            // Patch, to be removed in 10008
    {
        mc_AdjustStartAndCount(&count,&start,streams->GetCount()-1);        
        start++;            
    }
    else
    {
        mc_AdjustStartAndCount(&count,&start,streams->GetCount());        
    }
    
    
    Array partial_results;
    int unconfirmed_count=0;
    if(count > 0)
    {
        for(int i=0;i<streams->GetCount();i++)
        {
            Object entry;

            txid=streams->GetRow(i);
            entry=StreamEntry(txid,output_level);
            if(entry.size()>0)
            {
                BOOST_FOREACH(const Pair& p, entry) 
                {
                    if(p.name_ == "streamref")
                    {
                        if(p.value_.type() == str_type)
                        {
                            results.push_back(entry);                        
                        }
                        else
                        {
                            unconfirmed_count++;
                        }
                    }
                }            
            }            
        }

        sort(results.begin(), results.end(), AssetCompareByRef);
        
        for(int i=0;i<streams->GetCount();i++)
        {
            Object entry;

            txid=streams->GetRow(i);

            entry=StreamEntry(txid,output_level);
            if(entry.size()>0)
            {
                BOOST_FOREACH(const Pair& p, entry) 
                {
                    if(p.name_ == "streamref")
                    {
                        if(p.value_.type() != str_type)
                        {
                            results.push_back(entry);                        
                        }
                    }
                }            
            }            
        }
    }
        
    bool return_partial=false;
    if(count != streams->GetCount()-1)
    {
        return_partial=true;
    }
    if( (root_stream_name_size <= 1) && (inputStrings.size() == 0)  && (mc_gState->m_Features->FixedIn10008() == 0) )            // Patch, to be removed in 10008
    {
        return_partial=true;        
    }
    mc_gState->m_Assets->FreeEntityList(streams);
    if(return_partial)
    {
        for(int i=start;i<start+count;i++)
        {
            partial_results.push_back(results[i]);                                                                
        }
        return partial_results;
    }
     
    return results;
}

Value createstreamfromcmd(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 4)
        throw runtime_error("Help message not found\n");

    if(mc_gState->m_Features->Streams() == 0)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "API is not supported for this protocol version");        
    }
    
    if (strcmp(params[1].get_str().c_str(),"stream"))
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid entity type, should be stream");

    CWalletTx wtx;
    
    mc_Script *lpScript;
    
    mc_Script *lpDetailsScript;
    lpDetailsScript=NULL;
    
    mc_Script *lpDetails;
    
    int ret,type;
    string stream_name="";

    if (params[2].type() != str_type)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid stream name, should be string");
            
    if(!params[2].get_str().empty())
    {        
        stream_name=params[2].get_str();
    }
    
    if(params[3].type() != bool_type)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid open flag, should be boolean");
    
    if(mc_gState->m_Features->Streams())
    {
        if(stream_name == "*")
        {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid stream name: *");                                                                                            
        }
    }

    unsigned char buf_a[MC_AST_ASSET_REF_SIZE];    
    if(AssetRefDecode(buf_a,stream_name.c_str(),stream_name.size()))
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid stream name, looks like a stream reference");                                                                                                    
    }
            
    
    if(stream_name.size())
    {
        ret=ParseAssetKey(stream_name.c_str(),NULL,NULL,NULL,NULL,&type,MC_ENT_TYPE_ANY);
        if(ret != MC_ASSET_KEY_INVALID_NAME)
        {
            if(type == MC_ENT_KEYTYPE_NAME)
            {
                throw JSONRPCError(RPC_DUPLICATE_NAME, "Stream or asset with this name already exists");                                    
            }
            else
            {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid stream name");                                    
            }
        }        
    }

    lpScript=new mc_Script;
    
    lpDetails=new mc_Script;
    lpDetails->AddElement();
    if(params[3].get_bool())
    {
        unsigned char b=1;        
        lpDetails->SetSpecialParamValue(MC_ENT_SPRM_ANYONE_CAN_WRITE,&b,1);        
    }
    if(stream_name.size())
    {        
        lpDetails->SetSpecialParamValue(MC_ENT_SPRM_NAME,(unsigned char*)(stream_name.c_str()),stream_name.size());//+1);
    }
    
    

    if (params.size() > 4)
    {
        if(params[4].type() == obj_type)
        {
            Object objParams = params[4].get_obj();
            BOOST_FOREACH(const Pair& s, objParams) 
            {  
                lpDetails->SetParamValue(s.name_.c_str(),s.name_.size(),(unsigned char*)s.value_.get_str().c_str(),s.value_.get_str().size());                
            }
        }
        else
        {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid custom fields, expecting object");                                        
        }
    }
    
    int err;
    size_t bytes;
    const unsigned char *script;
    script=lpDetails->GetData(0,&bytes);
    
    lpDetailsScript=new mc_Script;

    size_t elem_size;
    const unsigned char *elem;
    CScript scriptOpReturn=CScript();
    
    if(mc_gState->m_Features->OpDropDetailsScripts())
    {
        err=lpDetailsScript->SetNewEntityType(MC_ENT_TYPE_STREAM,0,script,bytes);
        if(err)
        {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid custom fields or stream name, too long");                                                        
        }
        
        elem = lpDetailsScript->GetData(0,&elem_size);
        scriptOpReturn << vector<unsigned char>(elem, elem + elem_size) << OP_DROP << OP_RETURN;        
    }
    else
    {
        lpDetailsScript->SetNewEntityType(MC_ENT_TYPE_STREAM);

        err=lpDetailsScript->SetGeneralDetails(script,bytes);
        if(err)
        {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid custom fields or stream name, too long");                                                    
        }

        for(int e=0;e<lpDetailsScript->GetNumElements();e++)
        {
            elem = lpDetailsScript->GetData(e,&elem_size);
            if(e == (lpDetailsScript->GetNumElements() - 1) )
            {
                if(elem_size > 0)
                {
                    scriptOpReturn << OP_RETURN << vector<unsigned char>(elem, elem + elem_size);
                }
                else
                {
                    scriptOpReturn << OP_RETURN;
                }
            }
            else
            {
                if(elem_size > 0)
                {
                    scriptOpReturn << vector<unsigned char>(elem, elem + elem_size) << OP_DROP;
                }                
            }
        }    
    }
    
    vector<CTxDestination> addresses;    
    
    vector<CTxDestination> fromaddresses;        
    
    if(params[0].get_str() != "*")
    {
        fromaddresses=ParseAddresses(params[0].get_str(),false,false);

        if(fromaddresses.size() != 1)
        {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Single from-address should be specified");                        
        }

        if( (IsMine(*pwalletMain, fromaddresses[0]) & ISMINE_SPENDABLE) != ISMINE_SPENDABLE )
        {
            throw JSONRPCError(RPC_WALLET_ADDRESS_NOT_FOUND, "Private key for from-address is not found in this wallet");                        
        }
        
        set<CTxDestination> thisFromAddresses;

        BOOST_FOREACH(const CTxDestination& fromaddress, fromaddresses)
        {
            thisFromAddresses.insert(fromaddress);
        }

        CPubKey pkey;
        if(!pwalletMain->GetKeyFromAddressBook(pkey,MC_PTP_CREATE,&thisFromAddresses))
        {
            throw JSONRPCError(RPC_INSUFFICIENT_PERMISSIONS, "from-address doesn't have create permission");                
        }   
    }
    else
    {
        CPubKey pkey;
        if(!pwalletMain->GetKeyFromAddressBook(pkey,MC_PTP_CREATE))
        {
            throw JSONRPCError(RPC_INSUFFICIENT_PERMISSIONS, "This wallet doesn't have keys with create permission");                
        }        
    }
    
    
    EnsureWalletIsUnlocked();
    LOCK (pwalletMain->cs_wallet_send);
    
    SendMoneyToSeveralAddresses(addresses, 0, wtx, lpScript, scriptOpReturn,fromaddresses);

    if(lpDetailsScript)
    {
        delete lpDetailsScript;
    }
    delete lpDetails;
    delete lpScript;
  
    return wtx.GetHash().GetHex();    
}

Value createfromcmd(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 4)
        throw runtime_error("Help message not found\n");
    
    if(mc_gState->m_Features->Streams() == 0)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "API is not supported for this protocol version");        
    }
    if (strcmp(params[1].get_str().c_str(),"stream") == 0)
    {
        return createstreamfromcmd(params,fHelp);    
    }
    
    if (strcmp(params[1].get_str().c_str(),"upgrade") == 0)
    {
        return createupgradefromcmd(params,fHelp);    
    }
    
    throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid entity type, should be stream");
}

Value createcmd(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 3)
        throw runtime_error("Help message not found\n");

    if(mc_gState->m_Features->Streams() == 0)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "API is not supported for this protocol version");        
    }
    
    Array ext_params;
    ext_params.push_back("*");
    BOOST_FOREACH(const Value& value, params)
    {
        ext_params.push_back(value);
    }
    
    return createfromcmd(ext_params,fHelp);    
}

Value publish(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 3)
        throw runtime_error("Help message not found\n");
    
    if(mc_gState->m_Features->Streams() == 0)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "API is not supported for this protocol version");        
    }
    
    Array ext_params;
    ext_params.push_back("*");
    BOOST_FOREACH(const Value& value, params)
    {
        ext_params.push_back(value);
    }
    
    return publishfrom(ext_params,fHelp);    
}

Value publishfrom(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 4)
        throw runtime_error("Help message not found\n");

    if(mc_gState->m_Features->Streams() == 0)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "API is not supported for this protocol version");        
    }
       
    if(params[2].get_str() == "*")
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid item-key-string: *");                
    }
    
    mc_Script *lpScript;
    mc_EntityDetails stream_entity;
    parseStreamIdentifier(params[1],&stream_entity);           
               
    
    // Wallet comments
    CWalletTx wtx;
            
    vector<CTxDestination> addresses;    
    
    vector<CTxDestination> fromaddresses;        
    
    if(params[0].get_str() != "*")
    {
        fromaddresses=ParseAddresses(params[0].get_str(),false,false);

        if(fromaddresses.size() != 1)
        {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Single from-address should be specified");                        
        }
        if( (IsMine(*pwalletMain, fromaddresses[0]) & ISMINE_SPENDABLE) != ISMINE_SPENDABLE )
        {
            throw JSONRPCError(RPC_WALLET_ADDRESS_NOT_FOUND, "Private key for from-address is not found in this wallet");                        
        }
    }

    FindAddressesWithPublishPermission(fromaddresses,&stream_entity);
        
    if(params[2].get_str().size() > MC_ENT_MAX_ITEM_KEY_SIZE)
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Item key is too long");                                                                                                    
    }
    
    mc_Script *lpDetailsScript;
    lpDetailsScript=NULL;
        

    bool fIsHex;
    vector<unsigned char> dataData(ParseHex(params[3].get_str().c_str(),fIsHex));    
    if(!fIsHex)
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Item data should be hexadecimal string");                                                                                                    
    }
    
    lpDetailsScript=new mc_Script;
    lpDetailsScript->SetEntity(stream_entity.GetTxID()+MC_AST_SHORT_TXID_OFFSET);
    lpDetailsScript->SetItemKey((unsigned char*)params[2].get_str().c_str(),params[2].get_str().size());

    lpDetailsScript->AddElement();
    if(dataData.size())
    {
        lpDetailsScript->SetData(&dataData[0],dataData.size());
    }

    size_t elem_size;
    const unsigned char *elem;
    CScript scriptOpReturn=CScript();
    
    for(int e=0;e<lpDetailsScript->GetNumElements();e++)
    {
        elem = lpDetailsScript->GetData(e,&elem_size);
        if(e == (lpDetailsScript->GetNumElements() - 1) )
        {
            if(elem_size > 0)
            {
                scriptOpReturn << OP_RETURN << vector<unsigned char>(elem, elem + elem_size);
            }
            else
            {
                scriptOpReturn << OP_RETURN;
            }
        }
        else
        {
            if(elem_size > 0)
            {
                scriptOpReturn << vector<unsigned char>(elem, elem + elem_size) << OP_DROP;
            }                
        }
    }    
    
    
    lpScript=new mc_Script;
         
    EnsureWalletIsUnlocked();
    LOCK (pwalletMain->cs_wallet_send);
    
    SendMoneyToSeveralAddresses(addresses, 0, wtx, lpScript, scriptOpReturn,fromaddresses);

    delete lpDetailsScript;
    delete lpScript;
  
    return wtx.GetHash().GetHex();    
}

Value subscribe(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error("Help message not found\n");

    if(mc_gState->m_Features->Streams() == 0)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "API is not supported for this protocol version");        
    }
    if((mc_gState->m_WalletMode & MC_WMD_TXS) == 0)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "API is not supported with this wallet version. To get this functionality, run \"multichaind -walletdbversion=2 -rescan\" ");        
    }   
       
    // Whether to perform rescan after import
    bool fRescan = true;
    if (params.size() > 1)
        fRescan = params[1].get_bool();

    vector<mc_EntityDetails> inputEntities;
    vector<string> inputStrings;
    if(params[0].type() == str_type)
    {
        inputStrings.push_back(params[0].get_str());
    }
    else
    {    
        inputStrings=ParseStringList(params[0]);
    }
    
    for(int is=0;is<(int)inputStrings.size();is++)
    {
        mc_EntityDetails entity_to_subscribe;
        Value param=inputStrings[is];
        ParseEntityIdentifier(param,&entity_to_subscribe, MC_ENT_TYPE_ANY);           
        inputEntities.push_back(entity_to_subscribe);
    }
    
    bool fNewFound=false;
    for(int is=0;is<(int)inputStrings.size();is++)
    {
        mc_EntityDetails* lpEntity;
        lpEntity=&inputEntities[is];
        
        mc_TxEntity entity;
        if(lpEntity->GetEntityType() == MC_ENT_TYPE_STREAM)
        {
            entity.Zero();
            memcpy(entity.m_EntityID,lpEntity->GetTxID()+MC_AST_SHORT_TXID_OFFSET,MC_AST_SHORT_TXID_SIZE);
            entity.m_EntityType=MC_TET_STREAM | MC_TET_CHAINPOS;
            if(pwalletTxsMain->AddEntity(&entity,MC_EFL_NOT_IN_SYNC) != MC_ERR_FOUND)
            {
                entity.m_EntityType=MC_TET_STREAM | MC_TET_TIMERECEIVED;
                pwalletTxsMain->AddEntity(&entity,MC_EFL_NOT_IN_SYNC);
                entity.m_EntityType=MC_TET_STREAM_KEY | MC_TET_CHAINPOS;
                pwalletTxsMain->AddEntity(&entity,MC_EFL_NOT_IN_SYNC);
                entity.m_EntityType=MC_TET_STREAM_KEY | MC_TET_TIMERECEIVED;
                pwalletTxsMain->AddEntity(&entity,MC_EFL_NOT_IN_SYNC);
                entity.m_EntityType=MC_TET_STREAM_PUBLISHER | MC_TET_CHAINPOS;
                pwalletTxsMain->AddEntity(&entity,MC_EFL_NOT_IN_SYNC);
                entity.m_EntityType=MC_TET_STREAM_PUBLISHER | MC_TET_TIMERECEIVED;
                pwalletTxsMain->AddEntity(&entity,MC_EFL_NOT_IN_SYNC);
                fNewFound=true;
            }
        }

        if(lpEntity->GetEntityType() == MC_ENT_TYPE_ASSET)
        {
            entity.Zero();
            memcpy(entity.m_EntityID,lpEntity->GetShortRef(),mc_gState->m_NetworkParams->m_AssetRefSize);
            entity.m_EntityType=MC_TET_ASSET | MC_TET_CHAINPOS;
            if(pwalletTxsMain->AddEntity(&entity,MC_EFL_NOT_IN_SYNC) != MC_ERR_FOUND)
            {
                entity.m_EntityType=MC_TET_ASSET | MC_TET_TIMERECEIVED;
                pwalletTxsMain->AddEntity(&entity,MC_EFL_NOT_IN_SYNC);
                fNewFound=true;
            }
        }
    }
    
    if (fRescan && fNewFound)
    {
        pwalletMain->ScanForWalletTransactions(chainActive.Genesis(), true, true);
    }

    return Value::null;
}


Value unsubscribe(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error("Help message not found\n");

    if(mc_gState->m_Features->Streams() == 0)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "API is not supported for this protocol version");        
    }
    if((mc_gState->m_WalletMode & MC_WMD_TXS) == 0)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "API is not supported with this wallet version. To get this functionality, run \"multichaind -walletdbversion=2 -rescan\" ");        
    }   
       

    vector<mc_EntityDetails> inputEntities;
    vector<string> inputStrings;
    if(params[0].type() == str_type)
    {
        inputStrings.push_back(params[0].get_str());
    }
    else
    {    
        inputStrings=ParseStringList(params[0]);
    }
    
    for(int is=0;is<(int)inputStrings.size();is++)
    {
        mc_EntityDetails entity_to_subscribe;
        Value param=inputStrings[is];
        ParseEntityIdentifier(param,&entity_to_subscribe, MC_ENT_TYPE_ANY);           
        inputEntities.push_back(entity_to_subscribe);
    }
        
    mc_Buffer *streams;
    streams=new mc_Buffer;
    streams->Initialize(sizeof(mc_TxEntity),sizeof(mc_TxEntity),MC_BUF_MODE_DEFAULT);
    
    
    bool fNewFound=false;
    for(int is=0;is<(int)inputStrings.size();is++)
    {
        mc_EntityDetails* lpEntity;
        lpEntity=&inputEntities[is];
    
        mc_TxEntity entity;
        if(lpEntity->GetEntityType() == MC_ENT_TYPE_STREAM)
        {
            entity.Zero();
            memcpy(entity.m_EntityID,lpEntity->GetTxID()+MC_AST_SHORT_TXID_OFFSET,MC_AST_SHORT_TXID_SIZE);
            entity.m_EntityType=MC_TET_STREAM | MC_TET_CHAINPOS;
            streams->Add(&entity,NULL);
            entity.m_EntityType=MC_TET_STREAM | MC_TET_TIMERECEIVED;
            streams->Add(&entity,NULL);
            entity.m_EntityType=MC_TET_STREAM_KEY | MC_TET_CHAINPOS;
            streams->Add(&entity,NULL);
            entity.m_EntityType=MC_TET_STREAM_KEY | MC_TET_TIMERECEIVED;
            streams->Add(&entity,NULL);
            entity.m_EntityType=MC_TET_STREAM_PUBLISHER | MC_TET_CHAINPOS;
            streams->Add(&entity,NULL);
            entity.m_EntityType=MC_TET_STREAM_PUBLISHER | MC_TET_TIMERECEIVED;
            streams->Add(&entity,NULL);
            fNewFound=true;
        }

        if(lpEntity->GetEntityType() == MC_ENT_TYPE_ASSET)
        {
            entity.Zero();
            memcpy(entity.m_EntityID,lpEntity->GetShortRef(),mc_gState->m_NetworkParams->m_AssetRefSize);
            entity.m_EntityType=MC_TET_ASSET | MC_TET_CHAINPOS;
            streams->Add(&entity,NULL);
            entity.m_EntityType=MC_TET_ASSET | MC_TET_TIMERECEIVED;
            streams->Add(&entity,NULL);
            fNewFound=true;
        }
    }

    if(fNewFound)
    {
        if(pwalletTxsMain->Unsubscribe(streams))
        {
            delete streams;
            throw JSONRPCError(RPC_INTERNAL_ERROR, "Couldn't unsubscribe from stream");                                    
        }
    }

    delete streams;
    return Value::null;
}

Value getstreamitem(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 3)
        throw runtime_error("Help message not found\n");
   
    if(mc_gState->m_Features->Streams() == 0)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "API is not supported for this protocol version");        
    }
    if((mc_gState->m_WalletMode & MC_WMD_TXS) == 0)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "API is not supported with this wallet version. For full streams functionality, run \"multichaind -walletdbversion=2 -rescan\" ");        
    }   
           
    mc_EntityDetails stream_entity;
    parseStreamIdentifier(params[0],&stream_entity);           
    
    mc_TxEntityStat entStat;
    entStat.Zero();
    memcpy(&entStat,stream_entity.GetTxID()+MC_AST_SHORT_TXID_OFFSET,MC_AST_SHORT_TXID_SIZE);
    entStat.m_Entity.m_EntityType=MC_TET_STREAM;
    entStat.m_Entity.m_EntityType |= MC_TET_CHAINPOS;

    if(!pwalletTxsMain->FindEntity(&entStat))
    {
        throw JSONRPCError(RPC_NOT_SUBSCRIBED, "Not subscribed to this stream");                                
    }
    
    
    uint256 hash = ParseHashV(params[1], "parameter 2");
    
    bool verbose=false;
    
    if (params.size() > 2)    
    {
        verbose=paramtobool(params[2]);
    }
    
    const CWalletTx& wtx=pwalletTxsMain->GetWalletTx(hash,NULL,NULL);
    
    Object entry=StreamItemEntry(wtx,stream_entity.GetTxID()+MC_AST_SHORT_TXID_OFFSET,verbose);    
    
    if(entry.size() == 0)
    {
        throw JSONRPCError(RPC_TX_NOT_FOUND, "This transaction was not found in this stream");                
    }
    
    return entry;
}

Value liststreamitems(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 5)
        throw runtime_error("Help message not found\n");

    if(mc_gState->m_Features->Streams() == 0)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "API is not supported for this protocol version");        
    }
    if((mc_gState->m_WalletMode & MC_WMD_TXS) == 0)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "API is not supported with this wallet version. For full streams functionality, run \"multichaind -walletdbversion=2 -rescan\" ");        
    }   
           
    mc_TxEntityStat entStat;
    
    mc_EntityDetails stream_entity;
    parseStreamIdentifier(params[0],&stream_entity);           

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
    
    bool fLocalOrdering = false;
    if (params.size() > 4)
        fLocalOrdering = params[4].get_bool();
    
    entStat.Zero();
    memcpy(&entStat,stream_entity.GetTxID()+MC_AST_SHORT_TXID_OFFSET,MC_AST_SHORT_TXID_SIZE);
    entStat.m_Entity.m_EntityType=MC_TET_STREAM;
    if(fLocalOrdering)
    {
        entStat.m_Entity.m_EntityType |= MC_TET_TIMERECEIVED;
    }
    else
    {
        entStat.m_Entity.m_EntityType |= MC_TET_CHAINPOS;
    }
    if(!pwalletTxsMain->FindEntity(&entStat))
    {
        throw JSONRPCError(RPC_NOT_SUBSCRIBED, "Not subscribed to this stream");                                
    }
    
    mc_Buffer *entity_rows;
    entity_rows=new mc_Buffer;
    entity_rows->Initialize(MC_TDB_ENTITY_KEY_SIZE,MC_TDB_ROW_SIZE,MC_BUF_MODE_DEFAULT);
    
    mc_AdjustStartAndCount(&count,&start,entStat.m_LastPos);
    
    Array retArray;
    pwalletTxsMain->GetList(&entStat.m_Entity,start+1,count,entity_rows);
    
    for(int i=0;i<entity_rows->GetCount();i++)
    {
        mc_TxEntityRow *lpEntTx;
        lpEntTx=(mc_TxEntityRow*)entity_rows->GetRow(i);
        uint256 hash;
        memcpy(&hash,lpEntTx->m_TxId,MC_TDB_TXID_SIZE);
        const CWalletTx& wtx=pwalletTxsMain->GetWalletTx(hash,NULL,NULL);
        Object entry=StreamItemEntry(wtx,stream_entity.GetTxID()+MC_AST_SHORT_TXID_OFFSET,verbose);
        if(entry.size())
        {
            retArray.push_back(entry);                                
        }
    }
    
    delete entity_rows;
    
    return retArray;
}

void getTxsForBlockRange(vector <uint256>& txids,mc_TxEntity *entity,int height_from,int height_to,mc_Buffer *entity_rows)
{
    int first_item,last_item,count,i;
    
    last_item=pwalletTxsMain->GetBlockItemIndex(entity,height_to);
    if(last_item)
    {
        first_item=pwalletTxsMain->GetBlockItemIndex(entity,height_from-1)+1;
        count=last_item-first_item+1;
        if(count > 0)
        {
            pwalletTxsMain->GetList(entity,first_item,count,entity_rows);
            
            mc_TxEntityRow *lpEntTx;
            uint256 hash;
            for(i=0;i<count;i++)
            {
                lpEntTx=(mc_TxEntityRow*)entity_rows->GetRow(i);
                memcpy(&hash,lpEntTx->m_TxId,MC_TDB_TXID_SIZE);
                txids.push_back(hash);
            }
        }        
    }
}

Value liststreamblockitems(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 5)
        throw runtime_error("Help message not found\n");

    if(mc_gState->m_Features->Streams() == 0)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "API is not supported for this protocol version");        
    }
    if((mc_gState->m_WalletMode & MC_WMD_TXS) == 0)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "API is not supported with this wallet version. For full streams functionality, run \"multichaind -walletdbversion=2 -rescan\" ");        
    }   
           
    mc_TxEntityStat entStat;
    
    mc_EntityDetails stream_entity;
    parseStreamIdentifier(params[0],&stream_entity);           

    int count,start;
    bool verbose=false;
    
    if (params.size() > 2)    
    {
        verbose=paramtobool(params[2]);
    }
    
    count=2147483647;
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
    memcpy(&entStat,stream_entity.GetTxID()+MC_AST_SHORT_TXID_OFFSET,MC_AST_SHORT_TXID_SIZE);
    entStat.m_Entity.m_EntityType=MC_TET_STREAM;
    entStat.m_Entity.m_EntityType |= MC_TET_CHAINPOS;
    if(!pwalletTxsMain->FindEntity(&entStat))
    {
        throw JSONRPCError(RPC_NOT_SUBSCRIBED, "Not subscribed to this stream");                                
    }
    
    
    vector <int> heights=ParseBlockSetIdentifier(params[1]);
    vector <uint256> txids;
    
    Array retArray;
    if(heights.size() == 0)
    {
        return retArray;
    }
    
    int height_from,height_to;
    height_from=heights[0];
    height_to=heights[0];

    mc_Buffer *entity_rows;
    entity_rows=new mc_Buffer;
    entity_rows->Initialize(MC_TDB_ENTITY_KEY_SIZE,MC_TDB_ROW_SIZE,MC_BUF_MODE_DEFAULT);
    
    for(unsigned int i=1;i<heights.size();i++)
    {
        if(heights[i] > height_to + 1)
        {
            getTxsForBlockRange(txids,&entStat.m_Entity,height_from,height_to,entity_rows);
            height_from=heights[i];
        }
        height_to=heights[i];
    }
    
    
    getTxsForBlockRange(txids,&entStat.m_Entity,height_from,height_to,entity_rows);
    delete entity_rows;
    
    mc_AdjustStartAndCount(&count,&start,txids.size());
    
    for(int i=start;i<start+count;i++)
    {
        const CWalletTx& wtx=pwalletTxsMain->GetWalletTx(txids[i],NULL,NULL);
        Object entry=StreamItemEntry(wtx,stream_entity.GetTxID()+MC_AST_SHORT_TXID_OFFSET,verbose);
        if(entry.size())
        {
            retArray.push_back(entry);                                
        }
    }
    
    return retArray;
}


void getSubKeyEntityFromKey(string str,mc_TxEntityStat entStat,mc_TxEntity *entity)
{
    if(str == "*")
    {
        return;
    }
    uint160 key_string_hash;
    uint160 stream_subkey_hash;
    key_string_hash=Hash160(str.begin(),str.end());
    mc_GetCompoundHash160(&stream_subkey_hash,entStat.m_Entity.m_EntityID,&key_string_hash);
    memcpy(entity->m_EntityID,&stream_subkey_hash,MC_TDB_ENTITY_ID_SIZE);
    entity->m_EntityType=entStat.m_Entity.m_EntityType | MC_TET_SUBKEY;    
}

void getSubKeyEntityFromPublisher(string str,mc_TxEntityStat entStat,mc_TxEntity *entity)
{
    if(str == "*")
    {
        return;
    }
    uint160 stream_subkey_hash;
    CBitcoinAddress address(str);
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
        mc_GetCompoundHash160(&stream_subkey_hash,entStat.m_Entity.m_EntityID,lpKeyID);        
    }
    else
    {
        mc_GetCompoundHash160(&stream_subkey_hash,entStat.m_Entity.m_EntityID,lpScriptID);                
    }

    memcpy(entity->m_EntityID,&stream_subkey_hash,MC_TDB_ENTITY_ID_SIZE);
    entity->m_EntityType=entStat.m_Entity.m_EntityType | MC_TET_SUBKEY;    
}

Value liststreamkeyitems(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 6)
        throw runtime_error("Help message not found\n");

    if(mc_gState->m_Features->Streams() == 0)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "API is not supported for this protocol version");        
    }
    if((mc_gState->m_WalletMode & MC_WMD_TXS) == 0)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "API is not supported with this wallet version. For full streams functionality, run \"multichaind -walletdbversion=2 -rescan\" ");        
    }   
    
    if(params[1].get_str() == "*")
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
    
        return liststreamitems(ext_params,fHelp);            
    }
           
    mc_TxEntityStat entStat;
    mc_TxEntity entity;
    
    mc_EntityDetails stream_entity;
    parseStreamIdentifier(params[0],&stream_entity);           

    int count,start;
    bool verbose=false;
    
    if (params.size() > 2)    
    {
        verbose=paramtobool(params[2]);
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
        
    bool fLocalOrdering = false;
    if (params.size() > 5)
        fLocalOrdering = params[5].get_bool();
    
    entStat.Zero();
    memcpy(&entStat,stream_entity.GetTxID()+MC_AST_SHORT_TXID_OFFSET,MC_AST_SHORT_TXID_SIZE);
    entStat.m_Entity.m_EntityType=MC_TET_STREAM_KEY;
    if(fLocalOrdering)
    {
        entStat.m_Entity.m_EntityType |= MC_TET_TIMERECEIVED;
    }
    else
    {
        entStat.m_Entity.m_EntityType |= MC_TET_CHAINPOS;
    }
    if(!pwalletTxsMain->FindEntity(&entStat))
    {
        throw JSONRPCError(RPC_NOT_SUBSCRIBED, "Not subscribed to this stream");                                
    }

    getSubKeyEntityFromKey(params[1].get_str(),entStat,&entity);
    
    
    mc_Buffer *entity_rows;
    entity_rows=new mc_Buffer;
    entity_rows->Initialize(MC_TDB_ENTITY_KEY_SIZE,MC_TDB_ROW_SIZE,MC_BUF_MODE_DEFAULT);
    
    mc_AdjustStartAndCount(&count,&start,pwalletTxsMain->GetListSize(&entity,entStat.m_Generation,NULL));
    
    Array retArray;
    pwalletTxsMain->GetList(&entity,entStat.m_Generation,start+1,count,entity_rows);
    
    for(int i=0;i<entity_rows->GetCount();i++)
    {
        mc_TxEntityRow *lpEntTx;
        lpEntTx=(mc_TxEntityRow*)entity_rows->GetRow(i);
        uint256 hash;
        memcpy(&hash,lpEntTx->m_TxId,MC_TDB_TXID_SIZE);
        const CWalletTx& wtx=pwalletTxsMain->GetWalletTx(hash,NULL,NULL);
        Object entry=StreamItemEntry(wtx,stream_entity.GetTxID()+MC_AST_SHORT_TXID_OFFSET,verbose);
        if(entry.size())
        {
            retArray.push_back(entry);                                
        }
    }
    
    delete entity_rows;
    
    return retArray;
}


Value liststreampublisheritems(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 6)
        throw runtime_error("Help message not found\n");

    if(mc_gState->m_Features->Streams() == 0)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "API is not supported for this protocol version");        
    }
    if((mc_gState->m_WalletMode & MC_WMD_TXS) == 0)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "API is not supported with this wallet version. For full streams functionality, run \"multichaind -walletdbversion=2 -rescan\" ");        
    }   
           
    if(params[1].get_str() == "*")
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
    
        return liststreamitems(ext_params,fHelp);            
    }
    
    mc_TxEntityStat entStat;
    mc_TxEntity entity;
    uint160 stream_subkey_hash;
    
    mc_EntityDetails stream_entity;
    parseStreamIdentifier(params[0],&stream_entity);           

    int count,start;
    bool verbose=false;
    
    if (params.size() > 2)    
    {
        verbose=paramtobool(params[2]);
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

    bool fLocalOrdering = false;
    if (params.size() > 5)
        fLocalOrdering = params[5].get_bool();
    
    entStat.Zero();
    memcpy(&entStat,stream_entity.GetTxID()+MC_AST_SHORT_TXID_OFFSET,MC_AST_SHORT_TXID_SIZE);
    entStat.m_Entity.m_EntityType=MC_TET_STREAM_PUBLISHER;
    if(fLocalOrdering)
    {
        entStat.m_Entity.m_EntityType |= MC_TET_TIMERECEIVED;
    }
    else
    {
        entStat.m_Entity.m_EntityType |= MC_TET_CHAINPOS;
    }
    if(!pwalletTxsMain->FindEntity(&entStat))
    {
        throw JSONRPCError(RPC_NOT_SUBSCRIBED, "Not subscribed to this stream");                                
    }

    getSubKeyEntityFromPublisher(params[1].get_str(),entStat,&entity);
    
    mc_Buffer *entity_rows;
    entity_rows=new mc_Buffer;
    entity_rows->Initialize(MC_TDB_ENTITY_KEY_SIZE,MC_TDB_ROW_SIZE,MC_BUF_MODE_DEFAULT);
    
    mc_AdjustStartAndCount(&count,&start,pwalletTxsMain->GetListSize(&entity,entStat.m_Generation,NULL));
    
    Array retArray;
    pwalletTxsMain->GetList(&entity,entStat.m_Generation,start+1,count,entity_rows);
    
    for(int i=0;i<entity_rows->GetCount();i++)
    {
        mc_TxEntityRow *lpEntTx;
        lpEntTx=(mc_TxEntityRow*)entity_rows->GetRow(i);
        uint256 hash;
        memcpy(&hash,lpEntTx->m_TxId,MC_TDB_TXID_SIZE);
        const CWalletTx& wtx=pwalletTxsMain->GetWalletTx(hash,NULL,NULL);
        Object entry=StreamItemEntry(wtx,stream_entity.GetTxID()+MC_AST_SHORT_TXID_OFFSET,verbose);
        if(entry.size())
        {
            retArray.push_back(entry);                                
        }
    }
    
    delete entity_rows;
    
    return retArray;
}

bool IsAllowedMapMode(string mode)
{
    if(mode == "list")        return true;
    if(mode == "all")        return true;
    return false;
}

//Value liststreammap_operation(mc_TxEntity *parent_entity,mc_TxEntity *subkey_entity,string subkey_string,int count, int start, string mode)
Value liststreammap_operation(mc_TxEntity *parent_entity,vector<mc_TxEntity>& inputEntities,vector<string>& inputStrings,int count, int start, string mode)
{
    mc_TxEntity entity;
    mc_TxEntityStat entStat;
    Array retArray;
    mc_Buffer *entity_rows;
    mc_TxEntityRow erow;
    uint160 stream_subkey_hash;    
    int row,enitity_count;
    
    entity_rows=NULL;
    enitity_count=inputEntities.size();
    if(enitity_count == 0)
    {
        mc_AdjustStartAndCount(&count,&start,pwalletTxsMain->GetListSize(parent_entity,NULL));
        entity_rows=new mc_Buffer;
        entity_rows->Initialize(MC_TDB_ENTITY_KEY_SIZE,MC_TDB_ROW_SIZE,MC_BUF_MODE_DEFAULT);
        pwalletTxsMain->GetList(parent_entity,start+1,count,entity_rows);
        enitity_count=entity_rows->GetCount();
    }
    else
    {
        mc_AdjustStartAndCount(&count,&start,enitity_count);       
        enitity_count=count;
    }
    
    entStat.Zero();
    if(enitity_count)
    {
        memcpy(&entStat,parent_entity,sizeof(mc_TxEntity));
        pwalletTxsMain->FindEntity(&entStat);
    }
    
    for(int i=0;i<enitity_count;i++)
    {
        mc_TxEntityRow *lpEntTx;
        string key_string;
        if(entity_rows)
        {
            lpEntTx=(mc_TxEntityRow*)entity_rows->GetRow(i);
            key_string=pwalletTxsMain->GetSubKey(lpEntTx->m_TxId, NULL,NULL);
            entity.Zero();
            mc_GetCompoundHash160(&stream_subkey_hash,parent_entity->m_EntityID,lpEntTx->m_TxId);
            memcpy(entity.m_EntityID,&stream_subkey_hash,MC_TDB_ENTITY_ID_SIZE);
            entity.m_EntityType=parent_entity->m_EntityType | MC_TET_SUBKEY;
        }
        else
        {
            memcpy(&entity,&(inputEntities[i+start]),sizeof(mc_TxEntity));
            key_string=inputStrings[i+start];
        }
        
        int total,confirmed;
        total=pwalletTxsMain->GetListSize(&entity,entStat.m_Generation,&confirmed);
        
        Object all_entry;
        int shift=total-1;
        if(shift == 0)
        {
            shift=1;
        }
        if((parent_entity->m_EntityType & MC_TET_TYPE_MASK) == MC_TET_STREAM_PUBLISHER)
        {
            all_entry.push_back(Pair("publisher", key_string));                                                                                                                
        }
        else
        {
            all_entry.push_back(Pair("key", key_string));                                                                                            
        }
        all_entry.push_back(Pair("items", total));                                                                        
        all_entry.push_back(Pair("confirmed", confirmed));                                                                        
        
        if(mode == "all")
        {
            for(row=1;row<=total;row+=shift)
            {
                if( ( (row == 1) && (mode != "last") ) || ( (row == total) && (mode != "first") ) )
                {                    
                    erow.Zero();
                    memcpy(&erow.m_Entity,&entity,sizeof(mc_TxEntity));
                    erow.m_Generation=entStat.m_Generation;
                    erow.m_Pos=row;

                    if(pwalletTxsMain->GetRow(&erow) == 0)
                    {
                        uint256 hash;
                        memcpy(&hash,erow.m_TxId,MC_TDB_TXID_SIZE);
                        const CWalletTx& wtx=pwalletTxsMain->GetWalletTx(hash,NULL,NULL);

                        Value item_value;

                        item_value=StreamItemEntry(wtx,parent_entity->m_EntityID,true);
                        if(row == 1)
                        {
                            all_entry.push_back(Pair("first", item_value));                                                                        
                        }
                        if(row == total)
                        {
                            all_entry.push_back(Pair("last", item_value));                                                                        
                        }
                    }
                }
            }
        }
        retArray.push_back(all_entry);                                
    }

    if(entity_rows)
    {
        delete entity_rows;
    }
    
    return retArray;
}

Value liststreamkeys_or_publishers(const Array& params,bool is_publishers)
{
    mc_TxEntity entity;
    mc_TxEntityStat entStat;
    
    mc_EntityDetails stream_entity;
    
    parseStreamIdentifier(params[0],&stream_entity);           

    string mode="list";
    
    if (params.size() > 2)    
    {
        if(paramtobool(params[2]))
        {
            mode="all";            
        }
    }
        
    int count,start;
    count=2147483647;
    if (params.size() > 3)    
    {
        count=paramtoint(params[3],true,0,"Invalid count");
    }
    start=-count;
    if (params.size() > 4)    
    {
        start=paramtoint(params[4],false,0,"Invalid start");
    }
    
    bool fLocalOrdering = false;
    if (params.size() > 5)
        fLocalOrdering = params[5].get_bool();
    
    entStat.Zero();
    memcpy(&entStat,stream_entity.GetTxID()+MC_AST_SHORT_TXID_OFFSET,MC_AST_SHORT_TXID_SIZE);
    if(is_publishers)
    {
        entStat.m_Entity.m_EntityType=MC_TET_STREAM_PUBLISHER;                
    }
    else
    {
        entStat.m_Entity.m_EntityType=MC_TET_STREAM_KEY;        
    }
    if(fLocalOrdering)
    {
        entStat.m_Entity.m_EntityType |= MC_TET_TIMERECEIVED;
    }
    else
    {
        entStat.m_Entity.m_EntityType |= MC_TET_CHAINPOS;
    }
    
    if(!pwalletTxsMain->FindEntity(&entStat))
    {
        throw JSONRPCError(RPC_NOT_SUBSCRIBED, "Not subscribed to this stream");                                
    }

    vector<string> inputStrings;
    vector<mc_TxEntity> inputEntities;
    
    if(params.size() > 1)
    {
        if(!is_publishers && (params[1].type() == str_type) )
        {
            inputStrings.push_back(params[1].get_str());
        }
        else
        {
            inputStrings=ParseStringList(params[1]);
            if(inputStrings.size() == 0)
            {
                Array retArray;                
                return retArray;
            }
        }
        bool take_it=true;
        if( (inputStrings.size() == 1) && (inputStrings[0] == "*") )
        {
            take_it=false;
        }
        if(take_it)
        {            
            for(int is=0;is<(int)inputStrings.size();is++)
            {
                string str=inputStrings[is];
                entity.Zero();

                if(is_publishers)
                {
                    getSubKeyEntityFromPublisher(str,entStat,&entity);
                }
                else
                {
                    getSubKeyEntityFromKey(str,entStat,&entity);        
                }
                inputEntities.push_back(entity);
            }
        }
    }
    
    return liststreammap_operation(&(entStat.m_Entity),inputEntities,inputStrings,count,start,mode);        
}

Value liststreamkeys(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 6)
        throw runtime_error("Help message not found\n");
    
    if(mc_gState->m_Features->Streams() == 0)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "API is not supported for this protocol version");        
    }
    if((mc_gState->m_WalletMode & MC_WMD_TXS) == 0)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "API is not supported with this wallet version. For full streams functionality, run \"multichaind -walletdbversion=2 -rescan\" ");        
    }   
           
    return liststreamkeys_or_publishers(params,false);
}

Value liststreampublishers(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 6)
        throw runtime_error("Help message not found\n");
    if((mc_gState->m_WalletMode & MC_WMD_TXS) == 0)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "API is not supported with this wallet version. For full streams functionality, run \"multichaind -walletdbversion=2 -rescan\" ");        
    }   
    
    if(mc_gState->m_Features->Streams() == 0)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "API is not supported for this protocol version");        
    }
           
    return liststreamkeys_or_publishers(params,true);
}

