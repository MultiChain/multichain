// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2014-2016 The Bitcoin Core developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.


#include "rpc/rpcwallet.h"

Value createvariablefromcmd(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 5)
        throw runtime_error("Help message not found\n");

    if (strcmp(params[1].get_str().c_str(),"variable"))
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid entity type, should be stream");

    if(mc_gState->m_Features->Variables() == 0)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "API is not supported with this protocol version.");        
    }   
    
    CWalletTx wtx;
    
    mc_Script *lpScript=mc_gState->m_TmpBuffers->m_RpcScript3;    
    lpScript->Clear();
    mc_Script *lpDetailsScript=mc_gState->m_TmpBuffers->m_RpcScript1;
    lpDetailsScript->Clear();
    mc_Script *lpDetails=mc_gState->m_TmpBuffers->m_RpcScript2;
    lpDetails->Clear();
    
    int ret,type;
    string variable_name="";
    string strError="";
    int errorCode=RPC_INVALID_PARAMETER;
    
    if (params[2].type() != str_type)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid variable name, should be string");
            
    if(!params[2].get_str().empty())
    {        
        variable_name=params[2].get_str();
    }
        
    if(variable_name == "*")
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid variable name: *");                                                                                            
    }

    unsigned char buf_a[MC_AST_ASSET_REF_SIZE];    
    if(AssetRefDecode(buf_a,variable_name.c_str(),variable_name.size()))
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid variable name, looks like a variable reference");                                                                                                    
    }
            
    
    if(variable_name.size())
    {
        ret=ParseAssetKey(variable_name.c_str(),NULL,NULL,NULL,NULL,&type,MC_ENT_TYPE_ANY);
        if(ret != MC_ASSET_KEY_INVALID_NAME)
        {
            if(type == MC_ENT_KEYTYPE_NAME)
            {
                throw JSONRPCError(RPC_DUPLICATE_NAME, "Variable, stream or asset with this name already exists");                                    
            }
            else
            {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid variable name");                                    
            }
        }        
    }

    vector<CTxDestination> addresses;    
    
    vector<CTxDestination> fromaddresses;        
    EnsureWalletIsUnlocked();
    
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
        BOOST_FOREACH(const PAIRTYPE(CBitcoinAddress, CAddressBookData)& item, pwalletMain->mapAddressBook)
        {
            const CBitcoinAddress& address = item.first;
            CKeyID keyID;

            if(address.GetKeyID(keyID))
            {
                if( IsMine(*pwalletMain, keyID) & ISMINE_SPENDABLE )
                {
                    if(mc_gState->m_Permissions->CanCreate(NULL,(unsigned char*)(&keyID)))
                    {
                        fromaddresses.push_back(keyID);
                    }
                }
            }
        }                    
        CPubKey pkey;
        if(fromaddresses.size() == 0)
        {
            throw JSONRPCError(RPC_INSUFFICIENT_PERMISSIONS, "This wallet doesn't have keys with create permission");                
        }        
    }
    
    lpScript->Clear();
    
    lpDetails->Clear();
    lpDetails->AddElement();
    if(variable_name.size())
    {        
        lpDetails->SetSpecialParamValue(MC_ENT_SPRM_NAME,(unsigned char*)(variable_name.c_str()),variable_name.size());
    }
    unsigned char b=1;        
    lpDetails->SetSpecialParamValue(MC_ENT_SPRM_FOLLOW_ONS,&b,1);
    
    
    if (params.size() > 3)
    {
        if(params[3].type() != bool_type)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid open flag, should be boolean");
        if(params[3].get_bool())
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid open flag, should be false");
    }
    
    
    CScript scriptOpReturn=CScript();
    bool js_extended=false;
    size_t max_size=MC_AST_MAX_NOT_EXTENDED_VARIABLE_SIZE;
    lpDetailsScript->Clear();
    if (params.size() > 4)
    {
        ParseRawValue(&(params[4]),lpDetails,lpDetailsScript,&max_size,&errorCode,&strError);        
        if(strError.size())
        {
            goto exitlbl;
        }
        if(max_size > MC_AST_MAX_NOT_EXTENDED_VARIABLE_SIZE)
        {
            js_extended=true;
        }
    }
    lpDetailsScript->Clear();
    
    int err;
    size_t bytes;
    const unsigned char *script;
    script=lpDetails->GetData(0,&bytes);
    

    size_t elem_size;
    const unsigned char *elem;
    
    err=lpDetailsScript->SetNewEntityType(MC_ENT_TYPE_VARIABLE,0,script,bytes);
    if(err)
    {
        strError= "Invalid value or variable name, too long";
        goto exitlbl;
    }

    elem = lpDetailsScript->GetData(0,&elem_size);
    scriptOpReturn << vector<unsigned char>(elem, elem + elem_size) << OP_DROP << OP_RETURN;        
    
    if(js_extended)
    {
        lpDetails->Clear();
        lpDetails->AddElement();
        ParseRawValue(&(params[4]),lpDetails,lpDetailsScript,NULL,&errorCode,&strError);        
        if(strError.size())
        {
            goto exitlbl;
        }

        elem=lpDetails->GetData(0,&elem_size);
        lpDetailsScript->Clear();
        lpDetailsScript->SetExtendedDetails(elem,elem_size);
        elem = lpDetailsScript->GetData(0,&elem_size);
        scriptOpReturn << vector<unsigned char>(elem, elem + elem_size);
    }        

    {
        LOCK (pwalletMain->cs_wallet_send);

        SendMoneyToSeveralAddresses(addresses, 0, wtx, lpScript, scriptOpReturn,fromaddresses);
    }
        
exitlbl:

    if(strError.size())
    {
        throw JSONRPCError(errorCode, strError);                        
    }
    return wtx.GetHash().GetHex();    
}

Value setvariablevaluefrom(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 3)
        throw runtime_error("Help message not found\n");

    if(mc_gState->m_Features->Variables() == 0)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "API is not supported with this protocol version.");        
    }   
    
    unsigned char buf[MC_AST_ASSET_FULLREF_BUF_SIZE];
    memset(buf,0,MC_AST_ASSET_FULLREF_BUF_SIZE);
    mc_EntityDetails entity;
    
    if (params.size() > 1 && params[1].type() != null_type && !params[1].get_str().empty())
    {        
        ParseEntityIdentifier(params[1],&entity, MC_ENT_TYPE_VARIABLE);           
        memcpy(buf,entity.GetFullRef(),MC_AST_ASSET_FULLREF_SIZE);
    }
    else
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid variable identifier");        
    }

    CWalletTx wtx;
    
    mc_Script *lpScript=mc_gState->m_TmpBuffers->m_RpcScript3;    
    lpScript->Clear();
    
    mc_Script *lpDetailsScript=mc_gState->m_TmpBuffers->m_RpcScript1;
    lpDetailsScript->Clear();
    
    mc_Script *lpDetails=mc_gState->m_TmpBuffers->m_RpcScript2;
    lpDetails->Clear();
    
    lpDetails->AddElement();
        
    vector<CTxDestination> addresses;    
    vector<CTxDestination> fromaddresses;        
    
    
    CScript scriptOpReturn=CScript();
    int errorCode=RPC_INVALID_PARAMETER;
    string strError;    
    bool js_extended=false;
    size_t max_size=MC_AST_MAX_NOT_EXTENDED_VARIABLE_SIZE;
    lpDetailsScript->Clear();
    if (params.size() > 2)
    {
        ParseRawValue(&(params[2]),lpDetails,lpDetailsScript,&max_size,&errorCode,&strError);        
        if(strError.size())
        {
            goto exitlbl;
        }
        if(max_size > MC_AST_MAX_NOT_EXTENDED_VARIABLE_SIZE)
        {
            js_extended=true;
        }
    }
    lpDetailsScript->Clear();
    
    int err;
    size_t bytes;
    const unsigned char *script;
    size_t elem_size;
    const unsigned char *elem;
    
    script=lpDetails->GetData(0,&bytes);
    lpDetailsScript->SetEntity(entity.GetTxID()+MC_AST_SHORT_TXID_OFFSET);
    err=lpDetailsScript->SetNewEntityType(MC_ENT_TYPE_VARIABLE,1,script,bytes);
    if(err)
    {
        strError= "Invalid value, too long";
        goto exitlbl;
    }

    elem = lpDetailsScript->GetData(0,&elem_size);
    scriptOpReturn << vector<unsigned char>(elem, elem + elem_size) << OP_DROP;
    elem = lpDetailsScript->GetData(1,&elem_size);
    scriptOpReturn << vector<unsigned char>(elem, elem + elem_size) << OP_DROP << OP_RETURN;                
    
    if(js_extended)
    {
        lpDetails->Clear();
        lpDetails->AddElement();
        ParseRawValue(&(params[2]),lpDetails,lpDetailsScript,NULL,&errorCode,&strError);        
        if(strError.size())
        {
            goto exitlbl;
        }

        elem=lpDetails->GetData(0,&elem_size);
        lpDetailsScript->Clear();
        lpDetailsScript->SetExtendedDetails(elem,elem_size);
        elem = lpDetailsScript->GetData(0,&elem_size);
        scriptOpReturn << vector<unsigned char>(elem, elem + elem_size);
    }        

    EnsureWalletIsUnlocked();
       
    if(params[0].get_str() != "*")
    {
        fromaddresses=ParseAddresses(params[0].get_str(),false,false);

        if(fromaddresses.size() != 1)
        {
            strError= "Single from-address should be specified";
            goto exitlbl;
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Single from-address should be specified");                        
        }
        
        if( (IsMine(*pwalletMain, fromaddresses[0]) & ISMINE_SPENDABLE) != ISMINE_SPENDABLE )
        {
            strError= "Private key for from-address is not found in this wallet";
            errorCode=RPC_WALLET_ADDRESS_NOT_FOUND;
            goto exitlbl;
        }        
    }
    
    FindAddressesWithPublishPermission(fromaddresses,&entity);
    
    if(mc_gState->m_Assets->FindEntityByFullRef(&entity,buf))
    {
        if(entity.AllowedFollowOns())
        {
            if(fromaddresses.size() == 1)
            {
                CKeyID *lpKeyID=boost::get<CKeyID> (&fromaddresses[0]);
                if(lpKeyID != NULL)
                {
                    if(mc_gState->m_Permissions->CanWrite(entity.GetTxID(),(unsigned char*)(lpKeyID)) == 0)
                    {
                        strError= "Setting variable value is not allowed from this address";
                        errorCode=RPC_INSUFFICIENT_PERMISSIONS;
                        goto exitlbl;
                    }                                                 
                }
                else
                {
                    strError= "Setting variable value is allowed only from P2PKH addresses";
                    goto exitlbl;
                }
            }
            else
            {
                bool issuer_found=false;
                BOOST_FOREACH(const PAIRTYPE(CBitcoinAddress, CAddressBookData)& item, pwalletMain->mapAddressBook)
                {
                    const CBitcoinAddress& address = item.first;
                    CKeyID keyID;

                    if(address.GetKeyID(keyID))
                    {
                        if(mc_gState->m_Permissions->CanWrite(entity.GetTxID(),(unsigned char*)(&keyID)))
                        {
                            issuer_found=true;
                        }
                    }
                }                    
                if(!issuer_found)
                {
                    strError= "Setting variable value is not allowed from this wallet";
                    errorCode=RPC_INSUFFICIENT_PERMISSIONS;
                    goto exitlbl;
                }
            }
        }
        else
        {
            strError= "Setting variable value not allowed for this variable: "+params[1].get_str();
            errorCode=RPC_NOT_ALLOWED;
            goto exitlbl;
        }
    }   
    else
    {
        strError= "Variable not found";
        errorCode=RPC_ENTITY_NOT_FOUND;
        goto exitlbl;
    }
    
    {
        LOCK (pwalletMain->cs_wallet_send);

        SendMoneyToSeveralAddresses(addresses, 0, wtx, lpScript, scriptOpReturn,fromaddresses);
    }
        
exitlbl:

    if(strError.size())
    {
        throw JSONRPCError(errorCode, strError);                        
    }
    return wtx.GetHash().GetHex();    
}

Value setvariablevalue(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error("Help message not found\n");

    if(mc_gState->m_Features->Variables() == 0)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "API is not supported with this protocol version.");        
    }   
    
    Array ext_params;
    ext_params.push_back("*");
    BOOST_FOREACH(const Value& value, params)
    {
        ext_params.push_back(value);
    }
    
    return setvariablevaluefrom(ext_params,fHelp);    
}

Value getvariablevalue(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        mc_ThrowHelpMessage("getvariablevalue");        

    if(mc_gState->m_Features->Variables() == 0)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "API is not supported with this protocol version.");        
    }   
    
    mc_EntityDetails variable_entity;
    mc_EntityDetails last_entity;
    ParseEntityIdentifier(params[0],&variable_entity,MC_ENT_TYPE_VARIABLE);

    
    if(mc_gState->m_Assets->FindLastEntity(&last_entity,&variable_entity) == 0)
    {
        throw JSONRPCError(RPC_ENTITY_NOT_FOUND, "Variable not found");                
    }
    return mc_ExtractValueJSONObject(&last_entity);
}

Value getvariablehistory(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 4)
        throw runtime_error("Help message not found\n");

    if(mc_gState->m_Features->Variables() == 0)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "API is not supported with this protocol version.");        
    }   
    
    uint32_t output_level;
    
    int count,start;
    int history_items;
    
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
    
    mc_EntityDetails variable_entity;
    mc_EntityDetails last_entity;
    ParseEntityIdentifier(params[0],&variable_entity,MC_ENT_TYPE_VARIABLE);
    
    if(mc_gState->m_Assets->FindLastEntity(&last_entity,&variable_entity) == 0)
    {
        throw JSONRPCError(RPC_ENTITY_NOT_FOUND, "Variable not found");                
    }
    
    history_items=mc_GetEntityIndex(&last_entity)+1;
    output_level=0x40;
    
    if (params.size() > 1)    
    {
        if(paramtobool(params[1]))
        {
            output_level|=0x80;
        }
    }
    
    mc_AdjustStartAndCount(&count,&start,history_items);
    
    return VariableHistory(&last_entity,count,start,output_level);
}

Value getvariableinfo(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        mc_ThrowHelpMessage("getvariableinfo");        
    
    if(mc_gState->m_Features->Variables() == 0)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "API is not supported with this protocol version.");        
    }   
    
    if(params[0].type() != str_type)
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid variable identifier, expected string");                                                        
    }
    
    uint32_t output_level;
    mc_EntityDetails entity;
    ParseEntityIdentifier(params[0].get_str(),&entity, MC_ENT_TYPE_VARIABLE);           

    output_level=0x06;
    
    if (params.size() > 1)    
    {
        if(paramtobool(params[1]))
        {
            output_level|=0x20;
        }
    }
    
    return VariableEntry(entity.GetTxID(),output_level);        
}

Value listvariables(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 4)
       throw runtime_error("Help message not found\n");

    if(mc_gState->m_Features->Variables() == 0)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "API is not supported with this protocol version.");        
    }   
    

    mc_Buffer *variables;
    unsigned char *txid;
    uint32_t output_level;
    Array results;
    
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
    
    variables=NULL;
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
                mc_EntityDetails entity;
                ParseEntityIdentifier(inputStrings[is],&entity, MC_ENT_TYPE_VARIABLE);           
                uint256 hash=*(uint256*)entity.GetTxID();

                variables=mc_gState->m_Assets->GetEntityList(variables,(unsigned char*)&hash,MC_ENT_TYPE_VARIABLE);
            }
        }
    }
    else
    {        
        {
            LOCK(cs_main);
            variables=mc_gState->m_Assets->GetEntityList(variables,NULL,MC_ENT_TYPE_VARIABLE);
        }
    }
    
    if(variables == NULL)
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Cannot open variable database");

    output_level=0x06;
    
    if (params.size() > 1)    
    {
        if(paramtobool(params[1]))
        {
            output_level|=0x20;
        }
    }
    
    mc_AdjustStartAndCount(&count,&start,variables->GetCount());
    
    Array partial_results;
    int unconfirmed_count=0;
    if(count > 0)
    {
        for(int i=0;i<variables->GetCount();i++)
        {
            Object entry;

            txid=variables->GetRow(i);
            entry=VariableEntry(txid,output_level);
            if(entry.size()>0)
            {
                BOOST_FOREACH(const Pair& p, entry) 
                {
                    if(p.name_ == "variableref")
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
        
        for(int i=0;i<variables->GetCount();i++)
        {
            Object entry;

            txid=variables->GetRow(i);

            entry=VariableEntry(txid,output_level);
            if(entry.size()>0)
            {
                BOOST_FOREACH(const Pair& p, entry) 
                {
                    if(p.name_ == "variableref")
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
    if(count != variables->GetCount())
    {
        return_partial=true;
    }
    mc_gState->m_Assets->FreeEntityList(variables);
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

