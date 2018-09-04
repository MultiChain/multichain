// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2014-2016 The Bitcoin Core developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2017 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.


#include "rpc/rpcwallet.h"
#include "protocol/multichainfilter.h"

void TxToJSON(const CTransaction& tx, const uint256 hashBlock, Object& entry);

void ParseFilterRestrictions(Value param,mc_Script *lpDetailsScript)
{
    bool field_parsed,already_found;
    
    lpDetailsScript->Clear();
    lpDetailsScript->AddElement();            
    if(param.type() == obj_type)
    {
        Object objParams = param.get_obj();
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
                if(s.value_.type() == str_type)
                {
                    inputStrings.push_back(s.value_.get_str());
                }
                else
                {
                    inputStrings=ParseStringList(s.value_);        
                }
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
            if(!field_parsed)
            {
                throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Invalid field: %s",s.name_.c_str()));                           
            }
        }
    }
    else
    {
        if(param.type() == bool_type)
        {
            if(param.get_bool())
            {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid restrictions, should be object or boolean false");                                                        
            }
        }
        else
        {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid restrictions, should be object or boolean false");                                        
        }
    }    
}

string ParseFilterDetails(Value param)
{
    if(param.type() != str_type)
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid filter code, expecting string");                            
    }
    
    return param.get_str();
}

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
    
    CWalletTx wtx;
    
    mc_Script *lpScript=mc_gState->m_TmpBuffers->m_RpcScript3;    
    lpScript->Clear();
    mc_Script *lpDetailsScript=mc_gState->m_TmpBuffers->m_RpcScript1;
    lpDetailsScript->Clear();
    mc_Script *lpDetails=mc_gState->m_TmpBuffers->m_RpcScript2;
    lpDetails->Clear();
    
    int ret,type;
    string filter_name="";
    string strError="";
    int err;
    int errorCode=RPC_INVALID_PARAMETER;
    
    if (params[2].type() != str_type)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid filter name, should be string");
            
    if(!params[2].get_str().empty())
    {        
        filter_name=params[2].get_str();
    }
        
    if(filter_name == "*")
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid filter name: *");                                                                                            
    }

    unsigned char buf_a[MC_AST_ASSET_REF_SIZE];    
    if(AssetRefDecode(buf_a,filter_name.c_str(),filter_name.size()))
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid filter name, looks like a filter reference");                                                                                                    
    }
            
    
    if(filter_name.size())
    {
        ret=ParseAssetKey(filter_name.c_str(),NULL,NULL,NULL,NULL,&type,MC_ENT_TYPE_ANY);
        if(ret != MC_ASSET_KEY_INVALID_NAME)
        {
            if(type == MC_ENT_KEYTYPE_NAME)
            {
                throw JSONRPCError(RPC_DUPLICATE_NAME, "Filter, upgrade, stream or asset with this name already exists");                                    
            }
            else
            {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid filter name");                                    
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
    if(filter_name.size())
    {        
        lpDetails->SetSpecialParamValue(MC_ENT_SPRM_NAME,(unsigned char*)(filter_name.c_str()),filter_name.size());//+1);
    }
    
    size_t bytes;
    string js;
    const unsigned char *script;
    CScript scriptOpReturn=CScript();
    
    uint32_t filter_type=MC_FLT_TYPE_TX;
    
    lpDetails->SetSpecialParamValue(MC_ENT_SPRM_FILTER_TYPE,(unsigned char*)&filter_type,4);

    ParseFilterRestrictions(params[3],lpDetailsScript);
    
    script = lpDetailsScript->GetData(0,&bytes);

    if(bytes)
    {
        lpDetails->SetSpecialParamValue(MC_ENT_SPRM_FILTER_ENTITY,script,bytes);
    }

    js=ParseFilterDetails(params[4]);

/*    
    mc_Filter *worker=new mc_Filter;
    err=pFilterEngine->CreateFilter(js.c_str(),MC_FLT_MAIN_NAME_TX,worker,strError);
    delete worker;
    if(err)
    {
        throw JSONRPCError(RPC_INTERNAL_ERROR,"Couldn't create filter");                                                                   
    }
    if(strError.size())
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER,strprintf("Couldn't create filter: %s",strError.c_str()));                                                                               
    }
*/    
    lpDetails->SetSpecialParamValue(MC_ENT_SPRM_FILTER_CODE,(unsigned char*)js.c_str(),js.size());                                                        
    
    
    script=lpDetails->GetData(0,&bytes);
    

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

Value listtxfilters(const Array& params, bool fHelp)
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
            entry=TxFilterEntry(pMultiChainFilterEngine->m_Filters[i].m_Details.GetTxID(),output_level);
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
                    entry.push_back(Pair("compiled",valid));
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

            entry=TxFilterEntry(pMultiChainFilterEngine->m_Filters[i].m_Details.GetTxID(),output_level);
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
                    entry.push_back(Pair("compiled",valid));
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
    if (fHelp || params.size() != 0)
        throw JSONRPCError(RPC_INVALID_PARAMS, "Wrong number of parameters");                    
    
    return pMultiChainFilterEngine->m_TxID.ToString();
}

Value setfilterparam(const json_spirit::Array& params, bool fHelp)
{
    if (fHelp || params.size() != 2)                                            
        throw JSONRPCError(RPC_INVALID_PARAMS, "Wrong number of parameters");                    
    
    string param_name=params[0].get_str();
    bool fFound=false;
    
    if( (param_name == "maxshowndata") )
    {
        if( (params[1].type() == int_type) || (params[1].type() == str_type) )
        {
            int nValue;
            if(params[1].type() == int_type)
            {
                nValue=params[1].get_int();
            }
            else
            {
                nValue=atoi(params[1].get_str().c_str());
            }
            pMultiChainFilterEngine->m_Params.m_MaxShownData=nValue;
        }
        else
        {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter value type");                                            
        }
        fFound=true;
    }

    if(!fFound)
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Unsupported runtime parameter: " + param_name);                                                    
    }
    
    return "Set";
}

Value testtxfilter(const vector <uint160>& entities,const  char *filter_code, string txhex)
{
    Object result;
    int err;
    string strError;
    string strReason="";
    int errorCode=RPC_INVALID_PARAMETER;
    string strFatal="";
    bool relevant_filter=true;
    int64_t nStart;
    
    CTransaction tx;

    if(txhex.size())
    {
        if (!DecodeHexTx(tx,txhex))
            throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "TX decode failed");
    }
    
    mc_Filter *worker=new mc_Filter;
    err=pFilterEngine->CreateFilter(filter_code,MC_FLT_MAIN_NAME_TX,worker,strError);
    if(err)
    {
        errorCode=RPC_INTERNAL_ERROR;
        strFatal="Couldn't create filter";
        goto exitlbl;
    }
    if(strError.size())   
    {
        result.push_back(Pair("compiled", false));
        strReason=strError;
        relevant_filter=false;
    }
    else
    {
        result.push_back(Pair("compiled", true));
    }

    nStart = GetTimeMicros();
    if(txhex.size())
    {
        if(relevant_filter)
        {
            if(entities.size())
            {
                uint160 hash=0;
                mc_gState->m_Assets->m_TmpRelevantEntities->Clear();
                mc_gState->m_Assets->m_TmpRelevantEntities->Add(&hash);

                Object result;
                set <uint160> TxEntities;                                        
                TxToJSON(tx, 0, result);


                for(int i=1;i<mc_gState->m_Assets->m_TmpRelevantEntities->GetCount();i++)
                {
                    uint160 hash=0;
                    memcpy(&hash,mc_gState->m_Assets->m_TmpRelevantEntities->GetRow(i),MC_AST_SHORT_TXID_SIZE);
                    if(TxEntities.find(hash) == TxEntities.end())
                    {
                        TxEntities.insert(hash);
                    }
                }
                mc_gState->m_Assets->m_TmpRelevantEntities->Clear();

                relevant_filter=false;
                for(int i=0;i<(int)entities.size();i++)
                {
                    if(TxEntities.find(entities[i]) != TxEntities.end())
                    {
                        relevant_filter=true;
                    }
                }
            }
        }        
        
        Array callbacks;
        
        strError="";
        if(relevant_filter)
        {
            err=pMultiChainFilterEngine->RunFilterWithCallbackLog(tx,worker,strError,callbacks);
            if(err)
            {
                errorCode=RPC_INTERNAL_ERROR;
                strFatal="Couldn't run filter";
            }
        }
        
        if(strError.size())   
        {
            result.push_back(Pair("passed", false));
            strReason=strError;
        }
        else
        {
            result.push_back(Pair("passed", true));
        }                
        result.push_back(Pair("callbacks", callbacks));
    }

    if(strReason.size())
    {
        result.push_back(Pair("reason", strReason));        
    }
    else
    {
        result.push_back(Pair("reason", Value::null));                
    }
    
    if(txhex.size())
    {
        result.push_back(Pair("time", ((double)GetTimeMicros()-nStart)/1000000.));                
    }
    
exitlbl:    

    delete worker;
    
    if(strFatal.size())
    {
        throw JSONRPCError(errorCode,strFatal);                                                                                       
    }

    return result;    
}

Value runtxfilter(const json_spirit::Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error("Help message not found\n");
    
    mc_EntityDetails filter_entity;
    ParseEntityIdentifier(params[0],&filter_entity,MC_ENT_TYPE_FILTER);
    
    char filter_code[MC_ENT_MAX_SCRIPT_SIZE+1];
    std::vector <uint160> entities;   
    unsigned char *ptr;
    size_t value_size;
    string txhex="";
    
    ptr=(unsigned char *)filter_entity.GetSpecialParam(MC_ENT_SPRM_FILTER_ENTITY,&value_size);
    
    if(ptr)
    {
        if(value_size % MC_AST_SHORT_TXID_SIZE)
        {
            throw JSONRPCError(RPC_NOT_ALLOWED, "Specified filter has invalid restriction value");        
        }
    }    
    
    entities=mc_FillRelevantFilterEntitities(ptr, value_size);
    
    ptr=(unsigned char *)filter_entity.GetSpecialParam(MC_ENT_SPRM_FILTER_CODE,&value_size);
    
    if(ptr)
    {
        memcpy(filter_code,ptr,value_size);
        filter_code[value_size]=0x00;    
    }                                    
    
    if (params.size() > 1)    
    {
        if(params[1].type() != str_type)
        {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid tx-hex, should be string");            
        }
        txhex=params[1].get_str();
        if(txhex.size() == 0)
        {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Empty tx-hex");                        
        }
    }
    
    return testtxfilter(entities, (char *)filter_code, txhex);
}

Value testtxfilter(const json_spirit::Array& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 3)
        throw runtime_error("Help message not found\n");
    
    size_t bytes;
    string js;
    const unsigned char *script;
    std::vector <uint160> entities;   
    string txhex="";
    
    uint32_t filter_type=MC_FLT_TYPE_TX;
    
    mc_Script *lpDetailsScript=mc_gState->m_TmpBuffers->m_RpcScript1;
    lpDetailsScript->Clear();
    
    ParseFilterRestrictions(params[0],lpDetailsScript);
    
    script = lpDetailsScript->GetData(0,&bytes);

    if(bytes)
    {
        entities=mc_FillRelevantFilterEntitities(script, bytes);
    }

    js=ParseFilterDetails(params[1]);
    
    if (params.size() > 2)    
    {
        if(params[2].type() != str_type)
        {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid tx-hex, should be string");            
        }
        txhex=params[2].get_str();
        if(txhex.size() == 0)
        {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Empty tx-hex");                        
        }
    }
    
    return testtxfilter(entities, js.c_str(), txhex);
}
