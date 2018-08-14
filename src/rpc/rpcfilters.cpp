// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2014-2016 The Bitcoin Core developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2017 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.


#include "rpc/rpcwallet.h"
#include "protocol/filter.h"

Value createtxfilterfromcmd(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 5)
        throw runtime_error("Help message not found\n");

    if (strcmp(params[1].get_str().c_str(),"txfilter"))
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid entity type, should be stream");

    if(mc_gState->m_Features->Filters() == 0)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "API is not supported with this protocol version.");        
    }   
    
    if(MCP_ALLOW_FILTERS == 0)
    {
        throw JSONRPCError(RPC_NOT_ALLOWED, "Filters are not allowed in this chain.");        
    }   
    
    CWalletTx wtx;
    
    mc_Script *lpScript=mc_gState->m_TmpBuffers->m_RpcScript3;    
    lpScript->Clear();
    mc_Script *lpDetailsScript=mc_gState->m_TmpBuffers->m_RpcScript1;
    lpDetailsScript->Clear();
    mc_Script *lpDetails=mc_gState->m_TmpBuffers->m_RpcScript2;
    lpDetails->Clear();
    
    int ret,type;
    string upgrade_name="";
    string strError="";
    int errorCode=RPC_INVALID_PARAMETER;
    
    if (params[2].type() != str_type)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid upgrade name, should be string");
            
    if(!params[2].get_str().empty())
    {        
        upgrade_name=params[2].get_str();
    }
        
    if(upgrade_name == "*")
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid upgrade name: *");                                                                                            
    }

    unsigned char buf_a[MC_AST_ASSET_REF_SIZE];    
    if(AssetRefDecode(buf_a,upgrade_name.c_str(),upgrade_name.size()))
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid upgrade name, looks like a upgrade reference");                                                                                                    
    }
            
    
    if(upgrade_name.size())
    {
        ret=ParseAssetKey(upgrade_name.c_str(),NULL,NULL,NULL,NULL,&type,MC_ENT_TYPE_ANY);
        if(ret != MC_ASSET_KEY_INVALID_NAME)
        {
            if(type == MC_ENT_KEYTYPE_NAME)
            {
                throw JSONRPCError(RPC_DUPLICATE_NAME, "Filter, upgrade, stream or asset with this name already exists");                                    
            }
            else
            {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid upgrade name");                                    
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
        if(!pwalletMain->GetKeyFromAddressBook(pkey,MC_PTP_CREATE | MC_PTP_ADMIN,&thisFromAddresses))
        {
            throw JSONRPCError(RPC_INSUFFICIENT_PERMISSIONS, "from-address doesn't have create or admin permission");                
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
                        if(mc_gState->m_Permissions->CanAdmin(NULL,(unsigned char*)(&keyID)))
                        {
                            fromaddresses.push_back(keyID);
                        }
                    }
                }
            }
        }                    
        CPubKey pkey;
        if(fromaddresses.size() == 0)
        {
            throw JSONRPCError(RPC_INSUFFICIENT_PERMISSIONS, "This wallet doesn't have keys with create and admin permission");                
        }        
    }
    
    lpScript->Clear();
    
    lpDetails->Clear();
    lpDetails->AddElement();
    if(upgrade_name.size())
    {        
        lpDetails->SetSpecialParamValue(MC_ENT_SPRM_NAME,(unsigned char*)(upgrade_name.c_str()),upgrade_name.size());//+1);
    }
    
    bool field_parsed,already_found;
    size_t bytes;
    string js;
    const unsigned char *script;
    CScript scriptOpReturn=CScript();
    

    lpDetailsScript->Clear();
    lpDetailsScript->AddElement();            
    if(params[3].type() == obj_type)
    {
        Object objParams = params[4].get_obj();
        already_found=false;
        BOOST_FOREACH(const Pair& s, objParams) 
        {
            field_parsed=false;
            if(s.name_ == "for")
            {
                if(already_found)
                {
                    throw JSONRPCError(RPC_INVALID_PARAMETER,"for field can appear only once in the object");                               
                }
                lpDetailsScript->Clear();
                lpDetailsScript->AddElement();                   
                vector<string> inputStrings;
                inputStrings=ParseStringList(params[0]);        
                for(int is=0;is<(int)inputStrings.size();is++)
                {
                    mc_EntityDetails entity;
                    ParseEntityIdentifier(inputStrings[is],&entity, MC_ENT_TYPE_ANY);           
                    if(entity.GetEntityType() > MC_ENT_TYPE_STREAM_MAX)
                    {
                        throw JSONRPCError(RPC_INVALID_PARAMETER, "Filter can be created only for streams and assets");           
                    }
                    lpDetailsScript->SetData(entity.GetShortRef(),MC_AST_SHORT_TXID_SIZE);
                }
                field_parsed=true;
                already_found=true;
            }
            if(s.name_ == "js")
            {
                field_parsed=true;                
            }            
            if(!field_parsed)
            {
                throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Invalid field: %s",s.name_.c_str()));                           
            }
        }
        script = lpDetailsScript->GetData(0,&bytes);

        if(bytes)
        {
            lpDetails->SetSpecialParamValue(MC_ENT_SPRM_FILTER_ENTITY,script,bytes);
        }
    }
    else
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid restrictions, should be object");                            
    }
    
    if(params[4].type() == obj_type)
    {
        Object objParams = params[4].get_obj();
        already_found=false;
        BOOST_FOREACH(const Pair& s, objParams) 
        {
            field_parsed=false;
            if(s.name_ == "js")
            {
                if(s.value_.type() == str_type)
                {
                    if(already_found)
                    {
                        throw JSONRPCError(RPC_INVALID_PARAMETER,"js field can appear only once in the object");                               
                    }
                    js=s.value_.get_str();
                    already_found=true;
                    field_parsed=true;
                }
                else
                {
                    throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid js field, should be string");                    
                }
            }            
            if(!field_parsed)
            {
                throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Invalid field: %s",s.name_.c_str()));                           
            }
        }
        if(!already_found)
        {
            throw JSONRPCError(RPC_INVALID_PARAMETER,"js field is required");                                           
        }
        if(js.size() == 0)
        {
            throw JSONRPCError(RPC_INVALID_PARAMETER,"js cannot be empty");                                                       
        }        
        
        mc_Filter filter;
        string strError="";
        int err=pFilterEngine->CreateFilter(js.c_str(),"filtertransaction",&filter,strError);
        if(err)
        {
            throw JSONRPCError(RPC_INTERNAL_ERROR,"Couldn't create filter");                                                                   
        }
        if(strError.size())
        {
            throw JSONRPCError(RPC_INVALID_PARAMETER,strprintf("Couldn't create filter: %s",strError.c_str()));                                                                               
        }
    }
    else
    {
        strError="Invalid details, expecting object";
        goto exitlbl;
    }

    
    lpDetails->SetSpecialParamValue(MC_ENT_SPRM_FILTER_JS,(unsigned char*)js.c_str(),js.size());                                                        
    
    
    script=lpDetails->GetData(0,&bytes);
    

    int err;
    size_t elem_size;
    const unsigned char *elem;
    
    lpDetailsScript->Clear();
    err=lpDetailsScript->SetNewEntityType(MC_ENT_TYPE_FILTER,0,script,bytes);
    if(err)
    {
        strError="Invalid custom fields or filter name, too long";
        goto exitlbl;
    }

    elem = lpDetailsScript->GetData(0,&elem_size);
    scriptOpReturn << vector<unsigned char>(elem, elem + elem_size) << OP_DROP << OP_RETURN;        
    
    
    EnsureWalletIsUnlocked();
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

Value listfilters(const Array& params, bool fHelp)
{
    return Value::null;
}
