// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2014-2016 The Bitcoin Core developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2017 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.


#include "rpc/rpcwallet.h"
#include "protocol/multichainfilter.h"

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
    
    uint32_t filter_type=MC_FLT_TYPE_TX;
    
    lpDetails->SetSpecialParamValue(MC_ENT_SPRM_FILTER_TYPE,(unsigned char*)&filter_type,4);
    
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
        if(params[3].type() == bool_type)
        {
            if(params[3].get_bool())
            {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid restrictions, should be object or boolean false");                                                        
            }
        }
        else
        {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid restrictions, should be object or boolean false");                                        
        }
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
        
        mc_Filter *worker=new mc_Filter;
        string strError="";        
        int err=pFilterEngine->CreateFilter(js.c_str(),MC_FLT_MAIN_NAME_TX,worker,strError);
        delete worker;
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

    
    lpDetails->SetSpecialParamValue(MC_ENT_SPRM_FILTER_CODE,(unsigned char*)js.c_str(),js.size());                                                        
    
    
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
    if (fHelp || params.size() > 2)
        throw runtime_error("Help message not found\n");
    
    Array results;
    uint32_t output_level;

    if(mc_gState->m_Features->Filters() == 0)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "API is not supported with this protocol version.");        
    }   
    
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
    
    set <uint256> filter_list;
    
    if(inputStrings.size())
    {
        {
            LOCK(cs_main);
            for(int is=0;is<(int)inputStrings.size();is++)
            {
                string param=inputStrings[is];

                mc_EntityDetails filter_entity;
                ParseEntityIdentifier(param,&filter_entity,MC_ENT_TYPE_FILTER);
                filter_list.insert(*(uint256*)filter_entity.GetTxID());
            }
        }
    }
    
    
    bool verbose=false;
    if (params.size() > 1)    
    {
        if(paramtobool(params[1]))
        {
            verbose=true;            
        }
    }
    
    output_level=0x07;
    
    if (verbose)    
    {
        output_level=0x27;            
    }
    
    int unconfirmed_count=0;
    for(int i=0;i<(int)pMultiChainFilterEngine->m_Filters.size();i++)
    {
        Object entry;

        if((filter_list.size() == 0) || 
           (filter_list.find(*(uint256*)pMultiChainFilterEngine->m_Filters[i].m_Details.GetTxID()) != filter_list.end()) )
        {
            entry=FilterEntry(pMultiChainFilterEngine->m_Filters[i].m_Details.GetTxID(),output_level);
            if(entry.size()>0)
            {
                bool take_it=false;
                BOOST_FOREACH(const Pair& p, entry) 
                {
                    if(p.name_ == "filterref")
                    {
                        if(p.value_.type() == str_type)
                        {
                            take_it=true;
                        }
                        else
                        {
                            unconfirmed_count++;
                        }
                    }
                }            
                if(take_it)
                {
                    bool valid=pMultiChainFilterEngine->m_Filters[i].m_CreateError.size() == 0;
                    entry.push_back(Pair("valid",valid));
                    if(!valid)
                    {
                        entry.push_back(Pair("error",pMultiChainFilterEngine->m_Filters[i].m_CreateError));                            
                    }
                    entry.push_back(Pair("approved",mc_gState->m_Permissions->FilterApproved(NULL,&(pMultiChainFilterEngine->m_Filters[i].m_FilterAddress)) !=0 ));
                    entry.push_back(Pair("address",pMultiChainFilterEngine->m_Filters[i].m_FilterAddress.ToString()));
                    results.push_back(entry);                                            
                }
            }            
        }
    }

    sort(results.begin(), results.end(), AssetCompareByRef);
        
    for(int i=0;i<(int)pMultiChainFilterEngine->m_Filters.size();i++)
    {
        if((filter_list.size() == 0) || 
           (filter_list.find(*(uint256*)pMultiChainFilterEngine->m_Filters[i].m_Details.GetTxID()) != filter_list.end()) )
        {
            Object entry;

            entry=FilterEntry(pMultiChainFilterEngine->m_Filters[i].m_Details.GetTxID(),output_level);
            if(entry.size()>0)
            {
                bool take_it=false;
                BOOST_FOREACH(const Pair& p, entry) 
                {
                    if(p.name_ == "filterref")
                    {
                        if(p.value_.type() != str_type)
                        {
                            take_it=true;
                        }
                    }
                }            
                if(take_it)
                {
                    bool valid=pMultiChainFilterEngine->m_Filters[i].m_CreateError.size() == 0;
                    entry.push_back(Pair("valid",valid));
                    if(!valid)
                    {
                        entry.push_back(Pair("error",pMultiChainFilterEngine->m_Filters[i].m_CreateError));                            
                    }
                    entry.push_back(Pair("approved",mc_gState->m_Permissions->FilterApproved(NULL,&(pMultiChainFilterEngine->m_Filters[i].m_FilterAddress)) !=0 ));
                    entry.push_back(Pair("address",pMultiChainFilterEngine->m_Filters[i].m_FilterAddress.ToString()));
                    results.push_back(entry);                                            
                }
            }            
        }
    }

    for(int i=0;i<(int)results.size();i++)
    {
        uint160 filter_address=0;
        BOOST_FOREACH(const Pair& p, results[i].get_obj()) 
        {
            if(p.name_ == "address")
            {
                filter_address.SetHex(p.value_.get_str());
            }
        }
        results[i].get_obj().pop_back();
        
        if(verbose)
        {
            mc_PermissionDetails *plsRow;
            mc_PermissionDetails *plsDet;
            mc_PermissionDetails *plsPend;
            int flags,consensus,remaining;
            Array admins;
            Array pending;

            mc_Buffer *permissions=NULL;
            permissions=mc_gState->m_Permissions->GetPermissionList(NULL,(unsigned char*)&filter_address,MC_PTP_FILTER,permissions);

            if(permissions->GetCount())
            {
                plsRow=(mc_PermissionDetails *)(permissions->GetRow(0));

                flags=plsRow->m_Flags;
                consensus=plsRow->m_RequiredAdmins;
                mc_Buffer *details;

                if((flags & MC_PFL_HAVE_PENDING) || (consensus>1))
                {
                    details=mc_gState->m_Permissions->GetPermissionDetails(plsRow);                            
                }
                else
                {
                    details=NULL;
                }

                if(details)
                {
                    for(int j=0;j<details->GetCount();j++)
                    {
                        plsDet=(mc_PermissionDetails *)(details->GetRow(j));
                        remaining=plsDet->m_RequiredAdmins;
                        if(remaining > 0)
                        {
                            uint160 addr;
                            memcpy(&addr,plsDet->m_LastAdmin,sizeof(uint160));
                            CKeyID lpKeyID=CKeyID(addr);
                            admins.push_back(CBitcoinAddress(lpKeyID).ToString());                                                
                        }
                    }                    
                    for(int j=0;j<details->GetCount();j++)
                    {
                        plsDet=(mc_PermissionDetails *)(details->GetRow(j));
                        remaining=plsDet->m_RequiredAdmins;
                        if(remaining == 0)
                        {
                            Object pend_obj;
                            Array pend_admins;
                            uint32_t block_from=plsDet->m_BlockFrom;
                            uint32_t block_to=plsDet->m_BlockTo;
                            for(int k=j;k<details->GetCount();k++)
                            {
                                plsPend=(mc_PermissionDetails *)(details->GetRow(k));
                                remaining=plsPend->m_RequiredAdmins;

                                if(remaining == 0)
                                {
                                    if(block_from == plsPend->m_BlockFrom)
                                    {
                                        if(block_to == plsPend->m_BlockTo)
                                        {
                                            uint160 addr;
                                            memcpy(&addr,plsPend->m_LastAdmin,sizeof(uint160));
                                            CKeyID lpKeyID=CKeyID(addr);
    //                                        CKeyID lpKeyID=CKeyID(*(uint160*)((void*)(plsPend->m_LastAdmin)));
                                            pend_admins.push_back(CBitcoinAddress(lpKeyID).ToString());                                                
                                            plsPend->m_RequiredAdmins=0x01010101;
                                        }                                    
                                    }
                                }
                            }          
                            pend_obj.push_back(Pair("approve", block_from < block_to));                        
                            pend_obj.push_back(Pair("admins", pend_admins));
                            pend_obj.push_back(Pair("required", (int64_t)(consensus-pend_admins.size())));
                            pending.push_back(pend_obj);                            
                        }
                    }                    
                    mc_gState->m_Permissions->FreePermissionList(details);                    
                }
                else
                {
                    uint160 addr;
                    memcpy(&addr,plsRow->m_LastAdmin,sizeof(uint160));
                    CKeyID lpKeyID=CKeyID(addr);
                    admins.push_back(CBitcoinAddress(lpKeyID).ToString());                    
                }

                results[i].get_obj().push_back(Pair("admins", admins));
                results[i].get_obj().push_back(Pair("pending", pending));                                            
            }
            else
            {
                results[i].get_obj().push_back(Pair("admins", admins));
                results[i].get_obj().push_back(Pair("pending", pending));                                                            
            }
            mc_gState->m_Permissions->FreePermissionList(permissions);                    
        }    
    }
    
    return results;
}

Value getfiltercode(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error("Help message not found\n");
    
    if(mc_gState->m_Features->Filters() == 0)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "API is not supported with this protocol version.");        
    }   
    
    mc_EntityDetails filter_entity;
    ParseEntityIdentifier(params[0],&filter_entity,MC_ENT_TYPE_FILTER);
    
    char *ptr;
    size_t value_size;
    char filter_code[MC_ENT_MAX_SCRIPT_SIZE+1];
    
    ptr=(char *)filter_entity.GetSpecialParam(MC_ENT_SPRM_FILTER_CODE,&value_size);

    if(ptr == NULL)
    {
        return Value::null;
    }

    if(value_size)
    {
        memcpy(filter_code,ptr,value_size);
        
    }
    filter_code[value_size]=0x00;

    return string(filter_code);
}

Value getfiltertxid(const Array& params, bool fHelp)
{
    return pMultiChainFilterEngine->m_TxID.ToString();
}