// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2014-2016 The Bitcoin Core developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.


#include "rpc/rpcwallet.h"
bool AddParamNameValueToScript(const string  param_name,const Value param_value,mc_Script *lpDetailsScript,int version,int *errorCode,string *strError);
int CreateUpgradeLists(int current_height,vector<mc_UpgradedParameter> *vParams,vector<mc_UpgradeStatus> *vUpgrades);

Value createupgradefromcmd(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 5)
        throw runtime_error("Help message not found\n");

    if (strcmp(params[1].get_str().c_str(),"upgrade"))
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid entity type, should be stream");

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
                throw JSONRPCError(RPC_DUPLICATE_NAME, "Upgrade, stream or asset with this name already exists");                                    
            }
            else
            {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid upgrade name");                                    
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
    
    if(params[3].type() != bool_type)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid open flag, should be boolean");
    if(params[3].get_bool())
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid open flag, should be false");
    
    
    bool protocol_version_found=false;
    int protocol_version;
    int start_block;
    CScript scriptOpReturn=CScript();
    
    lpDetailsScript->Clear();
    lpDetailsScript->AddElement();                   

    if(params[4].type() == obj_type)
    {
        Object objParams = params[4].get_obj();
        BOOST_FOREACH(const Pair& s, objParams) 
        {  
            if(s.name_ == "protocol-version")
            {
                if(s.value_.type() != int_type)
                {
                    strError="Invalid protocol version, expecting integer";
                    goto exitlbl;
                }
                protocol_version_found=true;
                protocol_version=s.value_.get_int();
                if( (protocol_version < mc_gState->MinProtocolVersion()) || 
                    ( -mc_gState->VersionInfo(protocol_version) != mc_gState->GetNumericVersion() ) )
                {
                    strError=strprintf("Invalid value for protocol version. Valid range: %s\n",mc_SupportedProtocols().c_str());
                    errorCode=RPC_NOT_ALLOWED;
                    goto exitlbl;
                }
                
                if( protocol_version < mc_gState->MinProtocolDowngradeVersion() )
                {
                    strError="Invalid protocol version, cannot downgrade to this version";
                    errorCode=RPC_NOT_ALLOWED;
                    goto exitlbl;
                }
                if( mc_gState->m_NetworkParams->ProtocolVersion() >= mc_gState->MinProtocolForbiddenDowngradeVersion() )
                {
                    if(protocol_version < mc_gState->m_NetworkParams->ProtocolVersion())
                    {
                        strError="Invalid protocol version, cannot downgrade from current version";
                        errorCode=RPC_NOT_ALLOWED;
                        goto exitlbl;
                    }                    
                }
                lpDetails->SetSpecialParamValue(MC_ENT_SPRM_UPGRADE_PROTOCOL_VERSION,(unsigned char*)&protocol_version,4);        
            }
            else
            {
                if(s.name_ == "startblock")
                {
                    if(s.value_.type() != int_type)
                    {
                        strError="Invalid start block, expecting integer";
                        goto exitlbl;
                    }
                    start_block=s.value_.get_int();
                    lpDetails->SetSpecialParamValue(MC_ENT_SPRM_UPGRADE_START_BLOCK,(unsigned char*)&start_block,4);        
                }                    
                else
                {
                    if(mc_gState->m_Features->ParameterUpgrades())
                    {                        
                        if(!AddParamNameValueToScript(s.name_,s.value_,lpDetailsScript,0,&errorCode,&strError))
                        {
                            goto exitlbl;
                        }
                    }
                    else
                    {
                        strError="Some upgrade parameters are not supported by the current protocol, please upgrade protocol separately first.";
                        errorCode=RPC_NOT_SUPPORTED;
                        goto exitlbl;
                    }
//                    lpDetails->SetParamValue(s.name_.c_str(),s.name_.size(),(unsigned char*)s.value_.get_str().c_str(),s.value_.get_str().size());                                        
                }
            }                
        }
        
    }
    else
    {
        strError="Invalid custom fields, expecting object";
        goto exitlbl;
    }

    size_t bytes;
    const unsigned char *script;
    
    script = lpDetailsScript->GetData(0,&bytes);
    if( !protocol_version_found && (bytes == 0) )
    {        
        strError="Missing protocol-version";
        if(mc_gState->m_Features->ParameterUpgrades())
        {
            strError+=" or other parameters";
        }
        
        goto exitlbl;
    }
    
    if(bytes)
    {
        lpDetails->SetSpecialParamValue(MC_ENT_SPRM_UPGRADE_CHAIN_PARAMS,script,bytes);                                                        
    }
    
    
    script=lpDetails->GetData(0,&bytes);
    

    int err;
    size_t elem_size;
    const unsigned char *elem;
    
    lpDetailsScript->Clear();
    err=lpDetailsScript->SetNewEntityType(MC_ENT_TYPE_UPGRADE,0,script,bytes);
    if(err)
    {
        strError="Invalid custom fields or upgrade name, too long";
        goto exitlbl;
    }

    elem = lpDetailsScript->GetData(0,&elem_size);
    scriptOpReturn << vector<unsigned char>(elem, elem + elem_size) << OP_DROP << OP_RETURN;        
    
    
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

bool ParseStreamFilterApproval(Value param,mc_EntityDetails *stream_entity)
{
    bool field_parsed,for_found,approve_found;
    bool approval=false;
    
    if(param.type() == obj_type)
    {
        Object objParams = param.get_obj();
        for_found=false;
        approve_found=false;
        BOOST_FOREACH(const Pair& s, objParams) 
        {
            field_parsed=false;
            if(s.name_ == "for")
            {
                if(for_found)
                {
                    throw JSONRPCError(RPC_INVALID_PARAMETER,"for field can appear only once in the object");                               
                }
                if(s.value_.type() == str_type)
                {
                    ParseEntityIdentifier(s.value_.get_str(),stream_entity, MC_ENT_TYPE_STREAM);           
                }
                else
                {
                    throw JSONRPCError(RPC_INVALID_PARAMETER,"stream identifier should be string");                               
                }
                field_parsed=true;
                for_found=true;
            }            
            if(s.name_ == "approve")
            {
                if(approve_found)
                {
                    throw JSONRPCError(RPC_INVALID_PARAMETER,"for field can appear only once in the object");                               
                }
                if(s.value_.type() == bool_type)
                {
                    approval=s.value_.get_bool();
                }
                else
                {
                    throw JSONRPCError(RPC_INVALID_PARAMETER,"approve should be boolean");                               
                }       
                field_parsed=true;
                approve_found=true;
            }
            if(!field_parsed)
            {
                throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Invalid field: %s",s.name_.c_str()));                           
            }
        }
        if(!for_found)
        {
            throw JSONRPCError(RPC_INVALID_PARAMETER,"missing stream identifier");                                           
        }
        if(!approve_found)
        {
            throw JSONRPCError(RPC_INVALID_PARAMETER,"missing approve filed");                                           
        }
    }
    else
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid stream approval, should be object");                                        
    }        
    
    return approval;
}

Value approvefrom(const json_spirit::Array& params, bool fHelp)
{
    if (fHelp || params.size() < 3)
        throw runtime_error("Help message not found\n");

    vector<CTxDestination> addresses;
    set<CBitcoinAddress> setAddress;
    
    uint32_t approval,timestamp;

    string entity_identifier;
    entity_identifier=params[1].get_str();
    
    timestamp=mc_TimeNowAsUInt();

    mc_EntityDetails entity;
    entity.Zero();
    ParseEntityIdentifier(entity_identifier,&entity, MC_ENT_TYPE_ANY);           
    
    string entity_nameU; 
    string entity_nameL; 
    bool fIsUpgrade=true;
    bool fIsStreamFilter=false;
    
    switch(entity.GetEntityType())
    {
        case MC_ENT_TYPE_UPGRADE:
            entity_nameU="Upgrade";
            entity_nameL="upgrade";
            break;
        case MC_ENT_TYPE_FILTER:
            if(mc_gState->m_Features->Filters() == 0)
            {
                throw JSONRPCError(RPC_NOT_SUPPORTED, "API is not supported with this protocol version.");        
            }   
            if(entity.GetFilterType() != MC_FLT_TYPE_TX)
            {
                if(mc_gState->m_Features->StreamFilters() == 0)
                {
                    throw JSONRPCError(RPC_NOT_SUPPORTED, "Only Tx filters can be approved/disapproved in this protocol version.");        
                }        
                if(entity.GetFilterType() == MC_FLT_TYPE_STREAM)
                {
                    fIsStreamFilter=true;
                }
            }
            entity_nameU="Filter";
            entity_nameL="filter";
            fIsUpgrade=false;
            break;
        default:
            throw JSONRPCError(RPC_ENTITY_NOT_FOUND, "Invalid identifier, should be upgrade or filter");                                
            break;
    }

    mc_EntityDetails stream_entity;
    stream_entity.Zero();
    
    if(fIsStreamFilter)
    {
        if (params.size() <= 2)    
        {
            throw JSONRPCError(RPC_INVALID_PARAMS, "Missing stream identifier");                    
        }        
        approval=ParseStreamFilterApproval(params[2],&stream_entity);
    }
    else
    {
        approval=1;
        if (params.size() > 2)    
        {
            if(!paramtobool(params[2]))
            {
                approval=0;
            }
        }
    }
    

    if(fIsUpgrade)
    {
        if( mc_gState->m_NetworkParams->ProtocolVersion() >= mc_gState->MinProtocolForbiddenDowngradeVersion() )
        {
            if(entity.UpgradeProtocolVersion())
            {
                if(entity.UpgradeProtocolVersion() < mc_gState->m_NetworkParams->ProtocolVersion())
                {
                    throw JSONRPCError(RPC_NOT_ALLOWED, "Invalid protocol version, cannot downgrade from current version");
                }                    
            }
        }
    }
    
    if(fIsUpgrade)
    {
        if(mc_gState->m_Permissions->IsApproved(entity.GetTxID()+MC_AST_SHORT_TXID_OFFSET,0))
        {
            throw JSONRPCError(RPC_NOT_ALLOWED, "Upgrade already approved");                                
        }
    }
    
    vector<CTxDestination> fromaddresses;       
    fromaddresses=ParseAddresses(params[0].get_str(),false,false);
    EnsureWalletIsUnlocked();

    if(fromaddresses.size() != 1)
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Single from-address should be specified");                        
    }
    if( (IsMine(*pwalletMain, fromaddresses[0]) & ISMINE_SPENDABLE) != ISMINE_SPENDABLE )
    {
        throw JSONRPCError(RPC_WALLET_ADDRESS_NOT_FOUND, "Private key for from-address is not found in this wallet");                        
    }
            
    CKeyID *lpKeyID=boost::get<CKeyID> (&fromaddresses[0]);
    if(fIsStreamFilter)
    {
        if(lpKeyID != NULL)
        {
            if(mc_gState->m_Permissions->CanAdmin(stream_entity.GetTxID(),(unsigned char*)(lpKeyID)) == 0)
            {
                throw JSONRPCError(RPC_INSUFFICIENT_PERMISSIONS, "from-address doesn't have admin permission for specified stream");                                                                        
            }                                                                     
        }
        else
        {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Please use raw transactions to approve filters from P2SH addresses");                                                
        }
    }
    else
    {
        if(lpKeyID != NULL)
        {
            if(mc_gState->m_Permissions->CanAdmin(NULL,(unsigned char*)(lpKeyID)) == 0)
            {
                throw JSONRPCError(RPC_INSUFFICIENT_PERMISSIONS, "from-address doesn't have admin permission");                                                                        
            }                                                                     
        }
        else
        {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Please use raw transactions to approve upgrades from P2SH addresses");                                                
        }
    }
    
    mc_Script *lpScript=mc_gState->m_TmpBuffers->m_RpcScript3;
    lpScript->Clear();
    
    CScript scriptOpReturn=CScript();
    
    if(fIsUpgrade)
    {
        lpScript->SetEntity(entity.GetTxID()+MC_AST_SHORT_TXID_OFFSET);        
        lpScript->SetApproval(approval, timestamp);
        size_t elem_size;
        const unsigned char *elem;

        for(int e=0;e<lpScript->GetNumElements();e++)
        {
            elem = lpScript->GetData(e,&elem_size);
            scriptOpReturn << vector<unsigned char>(elem, elem + elem_size) << OP_DROP;
        }    
        scriptOpReturn << OP_RETURN;
    }
    else
    {
        if(fIsStreamFilter)
        {
            lpScript->SetEntity(stream_entity.GetTxID()+MC_AST_SHORT_TXID_OFFSET);                    
        }
        lpScript->SetPermission(MC_PTP_FILTER,0,approval ? 4294967295U : 0,timestamp);
        uint160 filter_address;
        filter_address=0;
        memcpy(&filter_address,entity.GetTxID()+MC_AST_SHORT_TXID_OFFSET,MC_AST_SHORT_TXID_SIZE);
        addresses.push_back(CKeyID(filter_address));
    }
    
    
    LogPrintf("mchn: %s %s %s (%s) from address %s\n",(approval != 0) ? "Approving" : "Disapproving",entity_nameL.c_str(),
            ((uint256*)entity.GetTxID())->ToString().c_str(),entity.GetName(),CBitcoinAddress(fromaddresses[0]).ToString().c_str());
        
    
    CWalletTx wtx;
    LOCK (pwalletMain->cs_wallet_send);    

    SendMoneyToSeveralAddresses(addresses, 0, wtx, lpScript, scriptOpReturn, fromaddresses);

    return wtx.GetHash().GetHex();
    
}

Value listupgrades(const json_spirit::Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error("Help message not found\n");

    Array results;
    mc_Buffer *upgrades;
    
    set<uint160> inputUpgrades;
    
    upgrades=NULL;
    
   
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
    
    for(int is=0;is<(int)inputStrings.size();is++)
    {
        string param=inputStrings[is];

        mc_EntityDetails upgrade_entity;
        ParseEntityIdentifier(param,&upgrade_entity, MC_ENT_TYPE_UPGRADE);           

        uint160 hash=0;
        memcpy(&hash,upgrade_entity.GetTxID() + MC_AST_SHORT_TXID_OFFSET,MC_AST_SHORT_TXID_SIZE);
        inputUpgrades.insert(hash);
    }

    vector<mc_UpgradedParameter> vParams;
    vector<mc_UpgradeStatus> vUpgrades;
    uint32_t current_height=chainActive.Height();
    CreateUpgradeLists(chainActive.Height(),&vParams,&vUpgrades);

    for(int u=0;u<(int)vUpgrades.size();u++)
    {
        Object entry;
        Object applied_params;
        Object skipped_params;
        mc_PermissionDetails *plsRow;
        mc_PermissionDetails *plsDet;
        mc_EntityDetails upgrade_entity;
        int consensus;
        Value null_value;
        string param_name;
        
        uint160 hash=0;
        memcpy(&hash,vUpgrades[u].m_EntityShortTxID,sizeof(uint160));
        
        if( (inputUpgrades.size() == 0) || (inputUpgrades.count(hash) > 0) )
        {
            upgrade_entity.Zero();
            mc_gState->m_Assets->FindEntityByShortTxID(&upgrade_entity,vUpgrades[u].m_EntityShortTxID);
            entry=UpgradeEntry(upgrade_entity.GetTxID());

            entry.push_back(Pair("approved", (vUpgrades[u].m_ApprovedBlock <= current_height+1)));            

            for(uint32_t p=vUpgrades[u].m_FirstParam;p<vUpgrades[u].m_LastParam;p++)
            {
                param_name=string(vParams[p].m_Param->m_DisplayName);
                if(vParams[p].m_Skipped == MC_PSK_APPLIED)
                {                
                    if(vParams[p].m_Param->m_Type & MC_PRM_DECIMAL)
                    {
                        applied_params.push_back(Pair(param_name,mc_gState->m_NetworkParams->Int64ToDecimal(vParams[p].m_Value)));            
                    }
                    else
                    {
                        switch(vParams[p].m_Param->m_Type & MC_PRM_DATA_TYPE_MASK)
                        {
                            case MC_PRM_BOOLEAN:
                                applied_params.push_back(Pair(param_name,(vParams[p].m_Value != 0)));            
                                break;
                            case MC_PRM_INT32:
                                applied_params.push_back(Pair(param_name,(int)vParams[p].m_Value));            
                            case MC_PRM_UINT32:
                            case MC_PRM_INT64:
                                applied_params.push_back(Pair(param_name,vParams[p].m_Value));            
                                break;
                        }                                
                    }
                }
                else
                {
                    string param_err;
                    switch(vParams[p].m_Skipped)
                    {
                        case MC_PSK_INTERNAL_ERROR:         param_err="Parameter not applied because of internal error, please report this"; break;
                        case MC_PSK_NOT_FOUND:              param_err="Parameter name not recognized"; break;
                        case MC_PSK_WRONG_SIZE:             param_err="Parameter is encoded with wrong size"; break;
                        case MC_PSK_OUT_OF_RANGE:           param_err="Parameter value is out of range"; break;
                        case MC_PSK_FRESH_UPGRADE:          param_err=strprintf("Parameter is upgraded less than %d blocks ago",MIN_BLOCKS_BETWEEN_UPGRADES); break;
                        case MC_PSK_DOUBLE_RANGE:           param_err="New parameter value must be between half and double previous value"; break;
                        case MC_PSK_NOT_SUPPORTED:          param_err="This parameter cannot be upgraded in this protocol version"; break;
                        case MC_PSK_NEW_NOT_DOWNGRADABLE:   param_err="Cannot downgrade to this version"; break;
                        case MC_PSK_OLD_NOT_DOWNGRADABLE:   param_err="Downgrades are not allowed in this protocol version"; break;
                        default:                            param_err="Parameter not applied because of internal error, please report this"; break;
                    }
                    skipped_params.push_back(Pair(param_name,param_err));            
                }
            }

            if(vUpgrades[u].m_AppliedBlock <= current_height)
            {
                entry.push_back(Pair("appliedblock", (int64_t)vUpgrades[u].m_AppliedBlock));                  
                entry.push_back(Pair("appliedparams", applied_params));                  
                entry.push_back(Pair("skippedparams", skipped_params));                  
            }
            else
            {
                entry.push_back(Pair("appliedblock", null_value));                  
                entry.push_back(Pair("appliedparams", null_value));                              
                entry.push_back(Pair("skippedparams", null_value));                  
            }

            upgrades=mc_gState->m_Permissions->GetUpgradeList(upgrade_entity.GetTxID() + MC_AST_SHORT_TXID_OFFSET,upgrades);
            if(upgrades->GetCount())
            {
                plsRow=(mc_PermissionDetails *)(upgrades->GetRow(upgrades->GetCount()-1));
                consensus=plsRow->m_RequiredAdmins;
                if(plsRow->m_Type != MC_PTP_UPGRADE)
                {
                    plsRow->m_BlockTo=0;
                }

                Array admins;
                Array pending;
                mc_Buffer *details;

                if(plsRow->m_Type == MC_PTP_UPGRADE)
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
                        if(plsDet->m_BlockFrom < plsDet->m_BlockTo)
                        {
                            uint160 addr;
                            memcpy(&addr,plsDet->m_LastAdmin,sizeof(uint160));
                            CKeyID lpKeyID=CKeyID(addr);
                            admins.push_back(CBitcoinAddress(lpKeyID).ToString());                                                
                        }
                    }                    
                    consensus=plsRow->m_RequiredAdmins;
                }
                if(admins.size() == 0)
                {
                    if(plsRow->m_BlockFrom < plsRow->m_BlockTo)
                    {
                        uint160 addr;
                        memcpy(&addr,plsRow->m_LastAdmin,sizeof(uint160));
                        CKeyID lpKeyID=CKeyID(addr);
                        admins.push_back(CBitcoinAddress(lpKeyID).ToString());                                                                    
                    }                
                }

                entry.push_back(Pair("admins", admins));
                entry.push_back(Pair("required", (int64_t)(consensus-admins.size())));
            }
            upgrades->Clear();
            results.push_back(entry);
        }
    }
    
    mc_gState->m_Permissions->FreePermissionList(upgrades);
     
    return results;
}


