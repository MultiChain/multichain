// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2014-2016 The Bitcoin Core developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.


#include "rpc/rpcwallet.h"
#include "filters/filter.h"

bool mc_JSInExtendedScript(size_t size);

Value createlibraryfromcmd(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 5)
        throw runtime_error("Help message not found\n");

    if (strcmp(params[1].get_str().c_str(),"library"))
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid entity type, should be stream");

    if(mc_gState->m_Features->Libraries() == 0)
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
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid library name, should be string");
            
    if(!params[2].get_str().empty())
    {        
        variable_name=params[2].get_str();
    }
        
    if(variable_name == "*")
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid library name: *");                                                                                            
    }

    unsigned char buf_a[MC_AST_ASSET_REF_SIZE];    
    if(AssetRefDecode(buf_a,variable_name.c_str(),variable_name.size()))
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid library name, looks like a library reference");                                                                                                    
    }
            
    
    if(variable_name.size())
    {
        ret=ParseAssetKey(variable_name.c_str(),NULL,NULL,NULL,NULL,&type,MC_ENT_TYPE_ANY);
        if(ret != MC_ASSET_KEY_INVALID_NAME)
        {
            if(type == MC_ENT_KEYTYPE_NAME)
            {
                throw JSONRPCError(RPC_DUPLICATE_NAME, "Variable, library, stream or asset with this name already exists");                                    
            }
            else
            {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid library name");                                    
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
    
    unsigned char b=255;        
    if(params[3].type() != obj_type)
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid options flag, should be object");
    }
    BOOST_FOREACH(const Pair& d, params[3].get_obj()) 
    {
        if(d.name_ == "updatemode")
        {
            if(d.value_.type() != str_type)
            {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid updatemode field");
            }
            if(d.value_.get_str() == "none")b=0x00;
            if(d.value_.get_str() == "instant")b=0x01;
            if(d.value_.get_str() == "approve")b=0x04;
            if(b == 255)
            {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid updatemode field");                        
            }
        }
        else
        {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid option: "+d.name_);                    
        }
    }
    
    if(b == 255)
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Missing updatemode field");                        
    }
    
    lpDetails->SetSpecialParamValue(MC_ENT_SPRM_FOLLOW_ONS,&b,1);
    
    
    CScript scriptOpReturn=CScript();
    string js;
    lpDetailsScript->Clear();
    
    if(params[4].type() != str_type)
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid library code, expecting string");                            
    }
    
    js=params[4].get_str();
    
    mc_Filter *worker=new mc_Filter;
    std::vector <std::string> callback_names;
    int err;
    
    string dummy_main_function="_multichain_library_test_";
    string test_code=js+"\n\n"+"function "+dummy_main_function+"(){} ";
    
    err=pFilterEngine->CreateFilter(test_code,dummy_main_function,callback_names,worker,strError);
    delete worker;
    if(err)
    {
        throw JSONRPCError(RPC_INTERNAL_ERROR,"Couldn't create library");                                                                   
    }
    if(strError.size())
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER,strprintf("Couldn't compile library code: %s",strError.c_str()));                                                                               
    }
    
    bool js_extended=mc_JSInExtendedScript(js.size());
    if(!js_extended)
    {
        lpDetails->SetSpecialParamValue(MC_ENT_SPRM_FILTER_CODE,(unsigned char*)js.c_str(),js.size());                                                        
    }
    
    lpDetailsScript->Clear();
    
    size_t bytes;
    const unsigned char *script;
    script=lpDetails->GetData(0,&bytes);
    

    size_t elem_size;
    const unsigned char *elem;
    
    err=lpDetailsScript->SetNewEntityType(MC_ENT_TYPE_LIBRARY,0,script,bytes);
    if(err)
    {
        strError= "Invalid value or library name, too long";
        goto exitlbl;
    }

    elem = lpDetailsScript->GetData(0,&elem_size);
    scriptOpReturn << vector<unsigned char>(elem, elem + elem_size) << OP_DROP << OP_RETURN;        
    
    if(js_extended)
    {
        lpDetails->Clear();
        lpDetails->AddElement();
        lpDetails->SetSpecialParamValue(MC_ENT_SPRM_FILTER_CODE,(unsigned char*)js.c_str(),js.size());                                                        
        
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


Value addlibraryupdatefrom(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 4)
        throw runtime_error("Help message not found\n");

    if(mc_gState->m_Features->Libraries() == 0)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "API is not supported with this protocol version.");        
    }   
    
    unsigned char buf[MC_AST_ASSET_FULLREF_BUF_SIZE];
    memset(buf,0,MC_AST_ASSET_FULLREF_BUF_SIZE);
    mc_EntityDetails entity;
    mc_EntityDetails update_entity;
    
    if (params.size() > 1 && params[1].type() != null_type && !params[1].get_str().empty())
    {        
        ParseEntityIdentifier(params[1],&entity, MC_ENT_TYPE_LIBRARY);           
        if(entity.IsFollowOn())
        {
            throw JSONRPCError(RPC_ENTITY_NOT_FOUND, "Library with this identifier not found");                                                                
        }
        memcpy(buf,entity.GetFullRef(),MC_AST_ASSET_FULLREF_SIZE);
    }
    else
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid library identifier");        
    }

    if(entity.AllowedFollowOns() == 0)
    {
        throw JSONRPCError(RPC_NOT_ALLOWED, "Library update not allowed for this library: "+params[1].get_str());        
    }
    
    if(params[2].get_str().size() == 0)
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid update name");                
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
    string js;
    lpDetailsScript->Clear();
    
    string update_name=params[2].get_str();
    
    if(update_name.size())
    {        
        lpDetails->SetSpecialParamValue(MC_ENT_SPRM_UPDATE_NAME,(unsigned char*)(update_name.c_str()),update_name.size());
    }
    
    if(mc_gState->m_Assets->FindUpdateByName(&update_entity,entity.GetTxID(),update_name.c_str()))
    {
        throw JSONRPCError(RPC_DUPLICATE_NAME, "Update with this name already exists");                                    
    }
    
    if(params[3].type() != str_type)
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid library code, expecting string");                            
    }
    
    js=params[3].get_str();
    
    mc_Filter *worker=new mc_Filter;
    std::vector <std::string> callback_names;
    int err;
    
    string dummy_main_function="_multichain_library_test_";
    string test_code=js+"\n\n"+"function "+dummy_main_function+"(){} ";

    err=pFilterEngine->CreateFilter(test_code,dummy_main_function,callback_names,worker,strError);
    delete worker;
    if(err)
    {
        throw JSONRPCError(RPC_INTERNAL_ERROR,"Couldn't create library");                                                                   
    }
    if(strError.size())
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER,strprintf("Couldn't compile library code: %s",strError.c_str()));                                                                               
    }
    
    bool js_extended=mc_JSInExtendedScript(js.size());
    if(!js_extended)
    {
        lpDetails->SetSpecialParamValue(MC_ENT_SPRM_FILTER_CODE,(unsigned char*)js.c_str(),js.size());                                                        
    }
    
    size_t bytes;
    const unsigned char *script;
    size_t elem_size;
    const unsigned char *elem;
    
    script=lpDetails->GetData(0,&bytes);
    lpDetailsScript->SetEntity(entity.GetTxID()+MC_AST_SHORT_TXID_OFFSET);
    err=lpDetailsScript->SetNewEntityType(MC_ENT_TYPE_LIBRARY,1,script,bytes);
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
        lpDetails->SetSpecialParamValue(MC_ENT_SPRM_FILTER_CODE,(unsigned char*)js.c_str(),js.size());                                                        

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
        if(fromaddresses.size() == 1)
        {
            CKeyID *lpKeyID=boost::get<CKeyID> (&fromaddresses[0]);
            if(lpKeyID != NULL)
            {
                if(mc_gState->m_Permissions->CanWrite(entity.GetTxID(),(unsigned char*)(lpKeyID)) == 0)
                {
                    strError= "Library update is not allowed from this address";
                    errorCode=RPC_INSUFFICIENT_PERMISSIONS;
                    goto exitlbl;
                }                                                 
            }
            else
            {
                strError= "Library update is allowed only from P2PKH addresses";
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
                strError= "Library update is not allowed from this wallet";
                errorCode=RPC_INSUFFICIENT_PERMISSIONS;
                goto exitlbl;
            }
        }
    }   
    else
    {
        strError= "Library not found";
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

Value addlibraryupdate(const Array& params, bool fHelp)
{
    if (fHelp || params.size() !=  3)
        throw runtime_error("Help message not found\n");

    if(mc_gState->m_Features->Libraries() == 0)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "API is not supported with this protocol version.");        
    }   
    
    Array ext_params;
    ext_params.push_back("*");
    BOOST_FOREACH(const Value& value, params)
    {
        ext_params.push_back(value);
    }
    
    return addlibraryupdatefrom(ext_params,fHelp);    
}

Value getlibrarycode(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error("Help message not found\n");
    
    if(mc_gState->m_Features->Filters() == 0)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "API is not supported with this protocol version.");        
    }   
    
    mc_EntityDetails filter_entity;
    mc_EntityDetails update_entity;
    mc_EntityDetails *entity_to_use;
    
    
    ParseEntityIdentifier(params[0],&filter_entity,MC_ENT_TYPE_LIBRARY);
    

    if(filter_entity.IsFollowOn())
    {
        throw JSONRPCError(RPC_ENTITY_NOT_FOUND, "Library with this create txid not found");                                                                
    }
    
    entity_to_use=&filter_entity;
    
    if(params.size() == 1)
    {
        if(mc_gState->m_Assets->FindLastEntity(&update_entity,&filter_entity) == 0)
        {
            throw JSONRPCError(RPC_ENTITY_NOT_FOUND, "Library not found");                
        }
        entity_to_use=&update_entity;
    }
    else
    {
        if(params[1].get_str().size())
        {
            if(mc_gState->m_Assets->FindUpdateByName(&update_entity,filter_entity.GetTxID(),params[1].get_str().c_str()) == 0)
            {
                throw JSONRPCError(RPC_ENTITY_NOT_FOUND, "Library update not found");                
            }            
            entity_to_use=&update_entity;
        }
    }

    char *ptr;
    size_t value_size;
    string filter_code="";
    
    
    ptr=(char *)entity_to_use->GetSpecialParam(MC_ENT_SPRM_FILTER_CODE,&value_size,1);

    if(ptr == NULL)
    {
        return Value::null;
    }

    if(value_size)
    {
        filter_code.assign(ptr,value_size);        
    }
    
    return filter_code;
}

Value listlibraries(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 4)
       throw runtime_error("Help message not found\n");

    if(mc_gState->m_Features->Libraries() == 0)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "API is not supported with this protocol version.");        
    }   
    

    mc_Buffer *libraries;
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
    
    libraries=NULL;
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
                ParseEntityIdentifier(inputStrings[is],&entity, MC_ENT_TYPE_LIBRARY);           
                if(entity.IsFollowOn())
                {
                    throw JSONRPCError(RPC_ENTITY_NOT_FOUND, "Library with this identifier " + inputStrings[is] + "not found");                                                                
                }
                uint256 hash=*(uint256*)entity.GetTxID();

                libraries=mc_gState->m_Assets->GetEntityList(libraries,(unsigned char*)&hash,MC_ENT_TYPE_LIBRARY);
            }
        }
    }
    else
    {        
        {
            LOCK(cs_main);
            libraries=mc_gState->m_Assets->GetEntityList(libraries,NULL,MC_ENT_TYPE_LIBRARY);
        }
    }
    
    if(libraries == NULL)
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Cannot open libraries database");

    output_level=0x06;
    
    if (params.size() > 1)    
    {
        if(paramtobool(params[1]))
        {
            output_level|=0x20;
        }
    }
    
    mc_AdjustStartAndCount(&count,&start,libraries->GetCount());
    
    Array partial_results;
    int unconfirmed_count=0;
    if(count > 0)
    {
        for(int i=0;i<libraries->GetCount();i++)
        {
            Object entry;

            txid=libraries->GetRow(i);
            entry=LibraryEntry(txid,output_level);
            if(entry.size()>0)
            {
                BOOST_FOREACH(const Pair& p, entry) 
                {
                    if(p.name_ == "libraryref")
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
        
        for(int i=0;i<libraries->GetCount();i++)
        {
            Object entry;

            txid=libraries->GetRow(i);

            entry=LibraryEntry(txid,output_level);
            if(entry.size()>0)
            {
                BOOST_FOREACH(const Pair& p, entry) 
                {
                    if(p.name_ == "libraryref")
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
    if(count != libraries->GetCount())
    {
        return_partial=true;
    }
    mc_gState->m_Assets->FreeEntityList(libraries);
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

