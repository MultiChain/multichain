// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2014-2016 The Bitcoin Core developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.


#include "rpc/rpcwallet.h"
#include "miner/miner.h"


string AllowedPermissions()
{
    string ret="connect,send,receive,issue,mine,admin,activate,create";
    
    return ret;
}

Value grantoperation(const Array& params)
{
    vector<CTxDestination> addresses;
    set<CBitcoinAddress> setAddress;
    
    stringstream ss(params[1].get_str()); 
    string tok;

    while(getline(ss, tok, ',')) 
    {
        CBitcoinAddress address(tok);
        if (!address.IsValid())
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address: "+tok);            
        if (setAddress.count(address))
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, duplicated address: "+tok);
        addresses.push_back(address.Get());
        setAddress.insert(address);
    }

    // Amount
    CAmount nAmount = 0;
    if (params.size() > 4 && params[4].type() != null_type)
    {
        nAmount = AmountFromValue(params[4]);
    }

    // Wallet comments
    CWalletTx wtx;
    if (params.size() > 7 && params[7].type() != null_type && !params[7].get_str().empty())
        wtx.mapValue["comment"] = params[7].get_str();
    if (params.size() > 8 && params[8].type() != null_type && !params[8].get_str().empty())
        wtx.mapValue["to"]      = params[8].get_str();

    mc_Script *lpScript=mc_gState->m_TmpBuffers->m_RpcScript3;
    lpScript->Clear();
    
    uint32_t type,from,to,timestamp;

    type=0;
    string entity_identifier, permission_type;
    entity_identifier="";
    if (params.size() > 2 && params[2].type() != null_type && !params[2].get_str().empty())
    {
        permission_type=params[2].get_str();
        int period_pos=permission_type.find_last_of(".");
        
        if(period_pos >= 0)
        {
            entity_identifier=permission_type.substr(0,period_pos);
            permission_type=permission_type.substr(period_pos+1,permission_type.size());
        }
    }
    

    
    from=0;
    if (params.size() > 5 && params[5].type() != null_type)
    {
        if(params[5].get_int64()<0)
        {
            throw JSONRPCError(RPC_INVALID_PARAMETER,  "start_block should be non-negative");                            
        }
        from=(uint32_t)params[5].get_int64();
    }
             
    to=4294967295U;
    if (params.size() > 6 && params[6].type() != null_type && (params[6].get_int64() != -1))
    {
        if(params[6].get_int64()<0)
        {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "end_block should be non-negative or -1");                            
        }
        to=(uint32_t)params[6].get_int64();
    }
    
    bool require_receive=true;
    
/*    
    if( ((type & MC_PTP_RECEIVE) == 0) || (from >= to))
    {        
        if(nAmount > 0)
        {
            BOOST_FOREACH(CTxDestination& txdest, addresses) 
            {
                if(!AddressCanReceive(txdest))
                {
                    throw JSONRPCError(RPC_INSUFFICIENT_PERMISSIONS, "Destination address doesn't have receive permission");        
                }
            }
        }
    }
*/    
    timestamp=mc_TimeNowAsUInt();

    mc_EntityDetails entity;
    entity.Zero();
    if (entity_identifier.size())
    {        
        ParseEntityIdentifier(entity_identifier,&entity, MC_ENT_TYPE_ANY);           
        LogPrintf("mchn: Granting %s permission(s) to address %s (%ld-%ld), Entity TxID: %s, Name: %s\n",permission_type,params[1].get_str(),from,to,
                ((uint256*)entity.GetTxID())->ToString().c_str(),entity.GetName());
        
        type=mc_gState->m_Permissions->GetPermissionType(permission_type.c_str(),&entity);
        
        if(type == 0)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid permission");
        
        lpScript->SetEntity(entity.GetTxID()+MC_AST_SHORT_TXID_OFFSET);        
    }
    else
    {
        type=mc_gState->m_Permissions->GetPermissionType(permission_type.c_str(),&entity);
        
        if(type == 0)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid permission");
        
        if(type & MC_PTP_RECEIVE)
        {
            if(from < to)
            {
                require_receive=false;
            }
        }
        
        LogPrintf("mchn: Granting %s permission(s) to address %s (%ld-%ld)\n",permission_type,params[1].get_str(),from,to);
    }
    
    if(require_receive)
    {
        if(nAmount > 0)
        {
            BOOST_FOREACH(CTxDestination& txdest, addresses) 
            {
                if(!AddressCanReceive(txdest))
                {
                    throw JSONRPCError(RPC_INSUFFICIENT_PERMISSIONS, "Destination address doesn't have receive permission");        
                }
            }
        }        
    }
    if(type & MC_PTP_MINE)
    {
        if(from<to)
        {
            BOOST_FOREACH(CTxDestination& txdest, addresses) 
            {
                CScriptID *lpScriptID=boost::get<CScriptID> (&txdest);
                if(lpScriptID != NULL)
                {
                    throw JSONRPCError(RPC_NOT_ALLOWED, "Granting mine permission to P2SH addresses is not allowed");                                    
                }
            }        
        }
    }
    
    lpScript->SetPermission(type,from,to,timestamp);
    
    vector<CTxDestination> fromaddresses;       
    if(params[0].get_str() != "*")
    {
        fromaddresses=ParseAddresses(params[0].get_str(),false,false);
    }

    if(fromaddresses.size() > 1)
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Single from-address should be specified");                        
    }
    
    mc_EntityDetails found_entity;
    found_entity.Zero();
    CScript scriptOpReturn=CScript();
    if (params.size() > 3)
    {
        scriptOpReturn=ParseRawMetadata(params[3],MC_DATA_API_PARAM_TYPE_SIMPLE,NULL,&found_entity);
    }
    EnsureWalletIsUnlocked();
    
    if(fromaddresses.size() == 1)
    {
        if( (IsMine(*pwalletMain, fromaddresses[0]) & ISMINE_SPENDABLE) != ISMINE_SPENDABLE )
        {
            throw JSONRPCError(RPC_WALLET_ADDRESS_NOT_FOUND, "Private key for from-address is not found in this wallet");                        
        }
        
        CKeyID *lpKeyID=boost::get<CKeyID> (&fromaddresses[0]);
        if(lpKeyID != NULL)
        {
            if(entity.GetEntityType())
            {
                if(mc_gState->m_Permissions->IsActivateEnough(type))
                {
                    if(mc_gState->m_Permissions->CanActivate(entity.GetTxID(),(unsigned char*)(lpKeyID)) == 0)
                    {
                        throw JSONRPCError(RPC_INSUFFICIENT_PERMISSIONS, "from-address doesn't have activate or admin permission for this entity");                                                                        
                    }                                                 
                }
                else
                {
                    if(mc_gState->m_Permissions->CanAdmin(entity.GetTxID(),(unsigned char*)(lpKeyID)) == 0)
                    {
                        throw JSONRPCError(RPC_INSUFFICIENT_PERMISSIONS, "from-address doesn't have admin permission for this entity");                                                                        
                    }                                                                     
                }
            }
            else
            {
                if(mc_gState->m_Permissions->IsActivateEnough(type))
                {
                    if(mc_gState->m_Permissions->CanActivate(NULL,(unsigned char*)(lpKeyID)) == 0)
                    {
                        throw JSONRPCError(RPC_INSUFFICIENT_PERMISSIONS, "from-address doesn't have activate or admin permission");                                                                        
                    }                                                 
                }
                else
                {
                    if(mc_gState->m_Permissions->CanAdmin(NULL,(unsigned char*)(lpKeyID)) == 0)
                    {
                        throw JSONRPCError(RPC_INSUFFICIENT_PERMISSIONS, "from-address doesn't have admin permission");                                                                        
                    }                                                                     
                }                
            }
        }
        else
        {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Please use raw transactions to grant/revoke from P2SH addresses");                                                
        }
        if(found_entity.GetEntityType() == MC_ENT_TYPE_STREAM)
        {
            FindAddressesWithPublishPermission(fromaddresses,&found_entity);
        }
    }
    else
    {
        bool admin_found=false;
        
        BOOST_FOREACH(const PAIRTYPE(CBitcoinAddress, CAddressBookData)& item, pwalletMain->mapAddressBook)
        {
            const CBitcoinAddress& address = item.first;
            CKeyID keyID;

            if(address.GetKeyID(keyID))
            {
                bool valid_publisher=true;
                if(found_entity.GetEntityType() == MC_ENT_TYPE_STREAM)
                {
                    if((found_entity.AnyoneCanWrite() == 0) && (mc_gState->m_Permissions->CanWrite(found_entity.GetTxID(),(unsigned char*)(&keyID)) == 0))
                    {
                        valid_publisher=false;
                    }
                }
                
                if(valid_publisher)
                {
                    if(entity.GetEntityType())
                    {
                        if(mc_gState->m_Permissions->IsActivateEnough(type))
                        {
                            if(mc_gState->m_Permissions->CanActivate(entity.GetTxID(),(unsigned char*)(&keyID)))
                            {
                                admin_found=true;
                            }                                                 
                        }
                        else
                        {
                            if(mc_gState->m_Permissions->CanAdmin(entity.GetTxID(),(unsigned char*)(&keyID)))
                            {
                                admin_found=true;
                            }                                                                     
                        }
                    }
                    else
                    {
                        if(mc_gState->m_Permissions->IsActivateEnough(type))
                        {
                            if(mc_gState->m_Permissions->CanActivate(NULL,(unsigned char*)(&keyID)))
                            {
                                admin_found=true;
                            }                                                 
                        }
                        else
                        {
                            if(mc_gState->m_Permissions->CanAdmin(NULL,(unsigned char*)(&keyID)))
                            {
                                admin_found=true;
                            }                                                                     
                        }                
                    }
                }
            }
        }        
        
        if(!admin_found)
        {
            string strErrorMessage="This wallet doesn't have addresses with ";
            if(found_entity.GetEntityType() == MC_ENT_TYPE_STREAM)
            {
                strErrorMessage+="write permission for given stream and ";
            }
            if(mc_gState->m_Permissions->IsActivateEnough(type))
            {
                strErrorMessage+="activate or admin permission";                
            }            
            else
            {
                strErrorMessage+="admin permission";                                
            }
            if(entity.GetEntityType())
            {
                strErrorMessage+=" for this entity";                                
            }
            throw JSONRPCError(RPC_INSUFFICIENT_PERMISSIONS, strErrorMessage);                                                                        
        }
    }
    
    
    LOCK (pwalletMain->cs_wallet_send);    

    SendMoneyToSeveralAddresses(addresses, nAmount, wtx, lpScript, scriptOpReturn, fromaddresses);

    return wtx.GetHash().GetHex();
    
}

Value grantwithmetadatafrom(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 4 || params.size() > 7)
        throw runtime_error("Help message not found\n");
    
    return grantoperation(params);
}

Value grantwithmetadata(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 3 || params.size() > 6)
        throw runtime_error("Help message not found\n");
    
    Array ext_params;
    ext_params.push_back("*");
    BOOST_FOREACH(const Value& value, params)
    {
        ext_params.push_back(value);
    }
    return grantoperation(ext_params);
}

Value grantfromcmd(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 3 || params.size() > 8)
        throw runtime_error("Help message not found\n");

    Array ext_params;
    int param_count=0;
    BOOST_FOREACH(const Value& value, params)
    {
        ext_params.push_back(value);
        param_count++;
        if(param_count==3)
        {
            ext_params.push_back("");            
        }
    }
    return grantoperation(ext_params);    
}


Value grantcmd(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 7)
        throw runtime_error("Help message not found\n");

    Array ext_params;
    ext_params.push_back("*");
    int param_count=1;
    BOOST_FOREACH(const Value& value, params)
    {
        ext_params.push_back(value);
        param_count++;
        if(param_count==3)
        {
            ext_params.push_back("");            
        }
    }
    return grantoperation(ext_params);
    
}

Value revokefromcmd(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 3 || params.size() > 6)
        throw runtime_error("Help message not found\n");

    Array ext_params;
    int param_count=0;
    BOOST_FOREACH(const Value& value, params)
    {
        ext_params.push_back(value);
        param_count++;
        if(param_count==3)
        {
            ext_params.push_back("");            
        }
        if(param_count==4)
        {
            ext_params.push_back(0);            
            ext_params.push_back(0);            
        }
    }
    if(param_count < 4)
    {
        ext_params.push_back(0);            
        ext_params.push_back(0);            
        ext_params.push_back(0);                    
    }
    return grantoperation(ext_params);
    
}



Value revokecmd(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 5)
        throw runtime_error("Help message not found\n");

    Array ext_params;
    ext_params.push_back("*");
    int param_count=1;
    BOOST_FOREACH(const Value& value, params)
    {
        ext_params.push_back(value);
        param_count++;
        if(param_count==3)
        {
            ext_params.push_back("");            
        }
        if(param_count==4)
        {
            ext_params.push_back(0);            
            ext_params.push_back(0);            
        }
    }
    if(param_count < 4)
    {
        ext_params.push_back(0);            
        ext_params.push_back(0);            
        ext_params.push_back(0);                    
    }
    return grantoperation(ext_params);
    
}

Value updatefromcmd(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 3)
        throw runtime_error("Help message not found\n");
    
    mc_EntityDetails entity;
    uint32_t type,from,to,timestamp;
    vector<CTxDestination> addresses;
    uint160 details_address;
    CWalletTx wtx;
    CScript scriptOpReturn=CScript();
    
    
    if (params[1].type() != null_type && !params[1].get_str().empty())
    {        
        ParseEntityIdentifier(params[1],&entity, MC_ENT_TYPE_ANY);           
    }
    else
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid entity identifier");        
    }

    switch(entity.GetEntityType())
    {
        case MC_ENT_TYPE_ASSET:
            if ( (params[2].type() != obj_type) || 
                 (params[2].get_obj().size() != 1) || 
                 (params[2].get_obj()[0].name_ != "open") )
            {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Asset update should be object with single field - open");                
            }
            to=0;
            if(paramtobool(params[2].get_obj()[0].value_))
            {
                to=4294967295U;
                if(entity.AdminCanOpen() == 0)
                {
                    throw JSONRPCError(RPC_NOT_ALLOWED, "Opening is not allowed for this asset");                                            
                }
            }
            else
            {
                if(entity.AdminCanClose() == 0)
                {
                    throw JSONRPCError(RPC_NOT_ALLOWED, "Closing is not allowed for this asset");                                            
                }                        
            }
            type=MC_PTP_DETAILS;
            timestamp=mc_TimeNowAsUInt();
            from=0;
            mc_gState->m_Permissions->DetailsAddress((unsigned char*)&details_address, MC_PDF_ASSET_OPEN);                 
            addresses.push_back(CKeyID(details_address));
            
            break;
        default:
            throw JSONRPCError(RPC_NOT_SUBSCRIBED, "This entity cannot be updated");        
            break;
    }
    
    mc_Script *lpScript=mc_gState->m_TmpBuffers->m_RpcScript3;
    lpScript->Clear();    
    lpScript->SetEntity(entity.GetTxID()+MC_AST_SHORT_TXID_OFFSET);        
    lpScript->SetPermission(type,from,to,timestamp);
    
    vector<CTxDestination> fromaddresses;       
    if(params[0].get_str() != "*")
    {
        fromaddresses=ParseAddresses(params[0].get_str(),false,false);
    }

    if(fromaddresses.size() > 1)
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Single from-address should be specified");                        
    }
    
    EnsureWalletIsUnlocked();
    
    if(fromaddresses.size() == 1)
    {
        if( (IsMine(*pwalletMain, fromaddresses[0]) & ISMINE_SPENDABLE) != ISMINE_SPENDABLE )
        {
            throw JSONRPCError(RPC_WALLET_ADDRESS_NOT_FOUND, "Private key for from-address is not found in this wallet");                        
        }
        
        CKeyID *lpKeyID=boost::get<CKeyID> (&fromaddresses[0]);
        if(lpKeyID != NULL)
        {
            if(entity.GetEntityType())
            {
                if(mc_gState->m_Permissions->CanAdmin(entity.GetTxID(),(unsigned char*)(lpKeyID)) == 0)
                {
                    throw JSONRPCError(RPC_INSUFFICIENT_PERMISSIONS, "from-address doesn't have admin permission for this entity");                                                                        
                }                                                                     
            }
        }
        else
        {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Please use raw transactions to grant/revoke from P2SH addresses");                                                
        }
    }
    else
    {
        bool admin_found=false;
        
        BOOST_FOREACH(const PAIRTYPE(CBitcoinAddress, CAddressBookData)& item, pwalletMain->mapAddressBook)
        {
            const CBitcoinAddress& address = item.first;
            CKeyID keyID;

            if(address.GetKeyID(keyID))
            {
                if(entity.GetEntityType())
                {
                    if(mc_gState->m_Permissions->CanAdmin(entity.GetTxID(),(unsigned char*)(&keyID)))
                    {
                        admin_found=true;
                    }                                                                     
                }
            }
        }        
        
        if(!admin_found)
        {
            string strErrorMessage="This wallet doesn't have addresses with ";
            strErrorMessage+="admin permission";                                
            if(entity.GetEntityType())
            {
                strErrorMessage+=" for this entity";                                
            }
            throw JSONRPCError(RPC_INSUFFICIENT_PERMISSIONS, strErrorMessage);                                                                        
        }
    }
    
    
    LOCK (pwalletMain->cs_wallet_send);    

    SendMoneyToSeveralAddresses(addresses, 0, wtx, lpScript, scriptOpReturn, fromaddresses);

    return wtx.GetHash().GetHex();    
}


Value updatecmd(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 2 )
        throw runtime_error("Help message not found\n");

    Array ext_params;
    ext_params.push_back("*");
    BOOST_FOREACH(const Value& value, params)
    {
        ext_params.push_back(value);
    }
    return updatefromcmd(ext_params,fHelp);
    
}


Value verifypermission(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 2)
        mc_ThrowHelpMessage("verifypermission");        
//        throw runtime_error("Help message not found\n");
    
    if(params[1].type() != str_type)
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid permission, expected string");                                                        
    }
    
    if(params[0].type() != str_type)
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid address, expected string");                                                        
    }
    
    uint32_t type;
    string entity_identifier, permission_type;
    entity_identifier="";
    permission_type="all";
    if (params.size() > 0 && params[1].type() != null_type)// && !params[0].get_str().empty())
    {
        permission_type=params[1].get_str();
//        int period_pos=permission_type.find_last_of(".",permission_type.size());
        int period_pos=permission_type.find_last_of(".");
        
        if(period_pos >= 0)
        {
            entity_identifier=permission_type.substr(0,period_pos);
            permission_type=permission_type.substr(period_pos+1,permission_type.size());
        }
    }
        
    mc_EntityDetails entity;
    const unsigned char *lpEntity;
    lpEntity=NULL;
    entity.Zero();
    if (entity_identifier.size())
    {        
        ParseEntityIdentifier(entity_identifier,&entity, MC_ENT_TYPE_ANY);           
        lpEntity=entity.GetTxID();
    }
    
    type=mc_gState->m_Permissions->GetPermissionType(permission_type.c_str(),&entity);
    if(type == 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid permission");

    CBitcoinAddress address(params[0].get_str());
    if (!address.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address: "+params[0].get_str());            

    CTxDestination dest=address.Get();
    CKeyID *lpKeyID=boost::get<CKeyID> (&dest);
    CScriptID *lpScriptID=boost::get<CScriptID> (&dest);
    
    void* lpAddress=NULL;

    if(((lpKeyID == NULL) && (lpScriptID == NULL)))
    {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address: "+params[1].get_str());            
        return false;                
    }
            
    if(lpKeyID != NULL)
    {
        lpAddress=lpKeyID;
    }
    else
    {
        lpAddress=lpScriptID;
    }
    
    
    int result=0;
    switch(type)
    {
        case MC_PTP_CONNECT : result = mc_gState->m_Permissions->CanConnectForVerify   (lpEntity,lpAddress); break;
        case MC_PTP_SEND:     result = mc_gState->m_Permissions->CanSend      (lpEntity,lpAddress); break;
        case MC_PTP_RECEIVE:  result = mc_gState->m_Permissions->CanReceive   (lpEntity,lpAddress); break;
        case MC_PTP_WRITE:    result = mc_gState->m_Permissions->CanWrite     (lpEntity,lpAddress); break;
        case MC_PTP_CREATE:   result = mc_gState->m_Permissions->CanCreate    (lpEntity,lpAddress); break;
        case MC_PTP_ISSUE:    result = mc_gState->m_Permissions->CanIssue     (lpEntity,lpAddress); break;
        case MC_PTP_ACTIVATE: result = mc_gState->m_Permissions->CanActivate  (lpEntity,lpAddress); break;
        case MC_PTP_MINE:     result = mc_gState->m_Permissions->CanMine      (lpEntity,lpAddress); break;
        case MC_PTP_ADMIN:    result = mc_gState->m_Permissions->CanAdmin     (lpEntity,lpAddress); break;
        case MC_PTP_CUSTOM1:
        case MC_PTP_CUSTOM2:
        case MC_PTP_CUSTOM3:
        case MC_PTP_CUSTOM4:
        case MC_PTP_CUSTOM5:
        case MC_PTP_CUSTOM6:
            result = mc_gState->m_Permissions->CanCustom(lpEntity,lpAddress,type);
            break;
        case MC_PTP_READ:    
            if(mc_gState->m_Features->ReadPermissions() == 0)
            {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid permission");
            }
            result = mc_gState->m_Permissions->CanRead      (lpEntity,lpAddress); 
            break;
        default:
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid permission");
    }    
    
    if(result == 0)                                                         // No entity restrictions
    {
        if(entity.GetEntityType())
        {
            if(mc_gState->m_Features->FixedIn20005())
            {
                if(entity.GetEntityType() == MC_ENT_TYPE_ASSET)
                {
                    if( (entity.Permissions() & type) == 0)
                    {
                        result=1;
                    }
                }
                else
                {
                    if(entity.GetEntityType() <= MC_ENT_TYPE_STREAM_MAX)
                    {
                        if(entity.AnyoneCanWrite())
                        {
                            result=1;                            
                        }
                    }                    
                }
            }
        }
    }
    
    return (result != 0);
}

Value listpermissions_operation(const Array& params, bool fOnlyPermittedOrPending)
{
    mc_Buffer *permissions;
    
    Array results;
    uint32_t type;
    
    type=0;

    string entity_identifier, permission_type;
    entity_identifier="";
    permission_type="all";
    if (params.size() > 0 && params[0].type() != null_type)// && !params[0].get_str().empty())
    {
        permission_type=params[0].get_str();
//        int period_pos=permission_type.find_last_of(".",permission_type.size());
        int period_pos=permission_type.find_last_of(".");
        
        if(period_pos >= 0)
        {
            entity_identifier=permission_type.substr(0,period_pos);
            permission_type=permission_type.substr(period_pos+1,permission_type.size());
        }
    }
    
    
    mc_EntityDetails entity;
    const unsigned char *lpEntity;
    lpEntity=NULL;
    entity.Zero();
    if (entity_identifier.size())
    {        
        ParseEntityIdentifier(entity_identifier,&entity, MC_ENT_TYPE_ANY);           
        lpEntity=entity.GetTxID();
    }
    
    type=mc_gState->m_Permissions->GetPermissionType(permission_type.c_str(),&entity);
    if(type == 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid permission");
    
    
    int verbose=0;
    if (params.size() > 2)    
    {
        if(paramtobool(params[2]))
        {
            verbose=1;
        }        
    }
    
    permissions=NULL;
    if (params.size() > 1 && params[1].type() != null_type && 
            ((params[1].type() != str_type) || ((params[1].get_str() !="*"))))// && (params[1].get_str() !="") )) )
    {
        vector<CTxDestination> addresses;
        vector<string> inputStrings=ParseStringList(params[1]);
        for(int is=0;is<(int)inputStrings.size();is++)
        {
            string tok=inputStrings[is];
            CBitcoinAddress address(tok);
            if (!address.IsValid())
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address: "+tok);            
            addresses.push_back(address.Get());            
        }        
        
        if(addresses.size() == 0)
        {
            return results;
        }
        BOOST_FOREACH(CTxDestination& dest, addresses) 
        {
            CKeyID *lpKeyID=boost::get<CKeyID> (&dest);
            CScriptID *lpScriptID=boost::get<CScriptID> (&dest);
            if(((lpKeyID == NULL) && (lpScriptID == NULL)))
            {
                mc_gState->m_Permissions->FreePermissionList(permissions);
                LogPrintf("mchn: Invalid address");
                return false;                
            }
            
            {
                LOCK(cs_main);
                if(lpKeyID != NULL)
                {
                    permissions=mc_gState->m_Permissions->GetPermissionList(lpEntity,(unsigned char*)(lpKeyID),type,permissions);
                }
                else
                {
                    permissions=mc_gState->m_Permissions->GetPermissionList(lpEntity,(unsigned char*)(lpScriptID),type,permissions);                
                }
            }
        }
    }
    else
    {        
        {
            LOCK(cs_main);
            permissions=mc_gState->m_Permissions->GetPermissionList(lpEntity,NULL,type,NULL);
        }
    }
    
    if(permissions == NULL)
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Cannot open permission database");

    for(int i=0;i<permissions->GetCount();i++)
    {
        Object entry;
        mc_PermissionDetails *plsRow;
        mc_PermissionDetails *plsDet;
        mc_PermissionDetails *plsPend;
        bool take_it;
        int flags,consensus,remaining;
        plsRow=(mc_PermissionDetails *)(permissions->GetRow(i));
        
        take_it=true;
        flags=plsRow->m_Flags;
        consensus=plsRow->m_RequiredAdmins;
        if(flags & MC_PFL_IS_SCRIPTHASH)
        {
            uint160 addr;
            memcpy(&addr,plsRow->m_Address,sizeof(uint160));
            CScriptID lpScriptID=CScriptID(addr);
            entry.push_back(Pair("address", CBitcoinAddress(lpScriptID).ToString()));                
            entry.push_back(Pair("isp2shaddress", true));                
        }
        else
        {
            uint160 addr;
            memcpy(&addr,plsRow->m_Address,sizeof(uint160));
            CKeyID lpKeyID=CKeyID(addr);
            entry.push_back(Pair("address", CBitcoinAddress(lpKeyID).ToString()));
        }
        entry.push_back(Pair("for", PermissionForFieldEntry(&entity)));            
        switch(plsRow->m_Type)
        {
            case MC_PTP_CONNECT:entry.push_back(Pair("type", "connect"));break;
            case MC_PTP_SEND   :entry.push_back(Pair("type", "send"));break;
            case MC_PTP_RECEIVE:entry.push_back(Pair("type", "receive"));break;
            case MC_PTP_WRITE  :entry.push_back(Pair("type", "write"));break;
            case MC_PTP_READ   :if(mc_gState->m_Features->ReadPermissions())entry.push_back(Pair("type", "read"));break;
            case MC_PTP_CREATE :entry.push_back(Pair("type", "create"));break;
            case MC_PTP_ISSUE  :entry.push_back(Pair("type", "issue"));break;
            case MC_PTP_MINE   :entry.push_back(Pair("type", "mine"));break;
            case MC_PTP_ADMIN  :entry.push_back(Pair("type", "admin"));break;
            case MC_PTP_ACTIVATE  :entry.push_back(Pair("type", "activate"));break;                
            default:take_it=false;
        }
        if(!take_it)
        {
            take_it=true;
            if(mc_gState->m_Features->CustomPermissions())
            {
                switch(plsRow->m_Type)
                {
                    case MC_PTP_CUSTOM1  :entry.push_back(Pair("type", MC_PTN_CUSTOM1));break;
                    case MC_PTP_CUSTOM2  :entry.push_back(Pair("type", MC_PTN_CUSTOM2));break;
                    case MC_PTP_CUSTOM3  :entry.push_back(Pair("type", MC_PTN_CUSTOM3));break;
                    case MC_PTP_CUSTOM4  :entry.push_back(Pair("type", MC_PTN_CUSTOM4));break;
                    case MC_PTP_CUSTOM5  :entry.push_back(Pair("type", MC_PTN_CUSTOM5));break;
                    case MC_PTP_CUSTOM6  :entry.push_back(Pair("type", MC_PTN_CUSTOM6));break;
                    default:take_it=false;
                }
            }
        }
        entry.push_back(Pair("startblock", (int64_t)plsRow->m_BlockFrom));
        entry.push_back(Pair("endblock", (int64_t)plsRow->m_BlockTo));           
        if(fOnlyPermittedOrPending)
        {
            if( (plsRow->m_BlockFrom >= plsRow->m_BlockTo) && 
                    (((flags & MC_PFL_HAVE_PENDING) == 0) || !verbose) )
            {
                take_it=false;
            }
        }
        if(take_it)
        {
            Array admins;
            Array pending;
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
                        if(plsDet->m_Flags & MC_PFL_IS_SCRIPTHASH)
                        {
                            CScriptID lpScriptID=CScriptID(addr);
                            admins.push_back(CBitcoinAddress(lpScriptID).ToString());                                                                            
                        }
                        else
                        {
                            CKeyID lpKeyID=CKeyID(addr);
                            admins.push_back(CBitcoinAddress(lpKeyID).ToString());                                                
                        }
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
                                        if(plsPend->m_Flags & MC_PFL_IS_SCRIPTHASH)
                                        {
                                            CScriptID lpScriptID=CScriptID(addr);
                                            pend_admins.push_back(CBitcoinAddress(lpScriptID).ToString());                                                                            
                                        }
                                        else
                                        {
                                            CKeyID lpKeyID=CKeyID(addr);
    //                                        CKeyID lpKeyID=CKeyID(*(uint160*)((void*)(plsPend->m_LastAdmin)));
                                            pend_admins.push_back(CBitcoinAddress(lpKeyID).ToString());                                                
                                        }
                                        plsPend->m_RequiredAdmins=0x01010101;
                                    }                                    
                                }
                            }
                        }          
                        pend_obj.push_back(Pair("startblock", (int64_t)block_from));
                        pend_obj.push_back(Pair("endblock", (int64_t)block_to));                        
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
            if(verbose)
            {
                entry.push_back(Pair("admins", admins));
                entry.push_back(Pair("pending", pending));                        
            }
            results.push_back(entry);
        }
    }
    
    mc_gState->m_Permissions->FreePermissionList(permissions);
     
    return results;
}

Value listpermissions(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 3)
        throw runtime_error("Help message not found\n");
    
    return listpermissions_operation(params,true);
}

typedef struct mc_MinerInfo
{
    uint160 m_Address;
    uint32_t m_StartBlock;
    uint32_t m_EndBlock;
    uint32_t m_BlockReceived;
    uint32_t m_ConfirmedStartBlock;
    uint32_t m_ConfirmedEndBlock;
    uint32_t m_LastMined;
    uint32_t m_Flags;
    uint256 m_LastTxID;
    uint256 m_ConfirmedLastTxID;
    bool m_Active;
    Value m_Pending;
    int m_Recent;
    uint32_t m_WaitBlocks;
    uint32_t m_NextAllowed;
    Array m_Grants;
    
    void Zero();
    
    mc_MinerInfo()
    {
        Zero();
    }
    
} mc_MinerInfo;

void mc_MinerInfo::Zero()
{
    m_Address=0;
    m_StartBlock=0;
    m_EndBlock=0;
    m_BlockReceived=0;
    m_ConfirmedStartBlock=0;
    m_ConfirmedEndBlock=0;
    m_LastTxID=0;
    m_ConfirmedLastTxID=0;
    m_LastMined=0;
    m_Flags=0;
    m_Active=false;
    m_Pending=Value::null;
    m_Recent=-1;
    m_WaitBlocks=INT_MAX;    
    m_NextAllowed=INT_MAX;
    m_Grants.clear();
}

bool ListMinersSort(mc_MinerInfo a,mc_MinerInfo b)
{ 
    if(a.m_WaitBlocks == b.m_WaitBlocks)
    {
        return (a.m_LastMined > b.m_LastMined);
    }
    
    return (a.m_WaitBlocks<b.m_WaitBlocks);
}

Array mc_GetRecentGrants(mc_PermissionDetails *plsRow,uint32_t from_block)
{
    Array result;
    mc_Buffer *details;
    uint32_t last_block=INT_MAX;
    int last_offset=-1;
    int last_tx=-1;
    CBlock block;
      
    details=mc_gState->m_Permissions->GetPermissionRows(plsRow,from_block);                            
    if(details)
    {
        for(int j=details->GetCount()-1;j>=0;j--)
        {
            mc_PermissionDetails *plsDet;
            Object entry;
            plsDet=(mc_PermissionDetails *)(details->GetRow(j));

            entry.push_back(Pair("grants", strprintf("%d-%d",plsDet->m_GrantFrom,plsDet->m_GrantTo)));
            entry.push_back(Pair("blocks", strprintf("%d-%d",plsDet->m_BlockFrom,plsDet->m_BlockTo)));
            uint160 addr;
            memcpy(&addr,plsDet->m_LastAdmin,sizeof(uint160));
            if(plsDet->m_Flags & MC_PFL_IS_SCRIPTHASH)
            {
                CScriptID lpScriptID=CScriptID(addr);
                entry.push_back(Pair("admin",CBitcoinAddress(lpScriptID).ToString()));                
            }
            else
            {
                CKeyID lpKeyID=CKeyID(addr);
                entry.push_back(Pair("admin",CBitcoinAddress(lpKeyID).ToString()));
            }
            if((int)plsDet->m_BlockReceived <= chainActive.Height())
            {
                entry.push_back(Pair("block", (int64_t)plsDet->m_BlockReceived));
                if(plsDet->m_BlockReceived != last_block)
                {
                    if(ReadBlockFromDisk(block, chainActive[plsDet->m_BlockReceived]))
                    {
                        last_block=plsDet->m_BlockReceived;
                        last_offset=80 + 1;
                        if(block.vtx.size() >= 0xfd)
                        {
                            last_offset+=2;
                        }
                        if(block.vtx.size() > 0xffff)
                        {
                            last_offset+=2;
                        }    
                        last_tx=0;
                    }
                }
                int offset=last_offset;
                int t=last_tx;
                while( (t<(int)block.vtx.size()) && (offset < plsDet->m_Offset))
                {   
                    offset+=block.vtx[t].GetSerializeSize(SER_NETWORK,block.vtx[t].nVersion);
                    t++;
                }
                last_offset=offset;
                last_tx=t;
                if((t<(int)block.vtx.size()) && (offset == plsDet->m_Offset))
                {
                    entry.push_back(Pair("txid", block.vtx[t].GetHash().ToString()));                                                                
                }
                else
                {
                    entry.push_back(Pair("txid", Value::null));                                                                
                    entry.push_back(Pair("offset", (int64_t)plsDet->m_Offset));                                            
                }
            }
            else
            {
                entry.push_back(Pair("block", Value::null));                
                const void *txid=mc_gState->m_Permissions->GetMempoolTxID(plsDet->m_Offset);
                if(txid)
                {
                    entry.push_back(Pair("txid", ((uint256*)txid)->ToString()));                                                                                    
                }
                else
                {
                    entry.push_back(Pair("txid", Value::null));                                                                
                    entry.push_back(Pair("offset", (int64_t)plsDet->m_Offset));                                                                
                }                    
            }
            result.push_back(entry);
        }
        
        mc_gState->m_Permissions->FreePermissionList(details);                      
    }            
    return result;
}

Array listconnect_operation(const Array& params)
{
    Array result;
    mc_Buffer *permissions;
    permissions=mc_gState->m_Permissions->GetPermissionList(NULL,NULL,MC_PTP_CONNECT,NULL);
    for(int i=0;i<permissions->GetCount();i++)
    {
        Object entry;
        mc_PermissionDetails *plsRow;
        plsRow=(mc_PermissionDetails *)(permissions->GetRow(i));
        mc_MinerInfo admin;
        if(plsRow->m_BlockFrom < plsRow->m_BlockTo)
        {
            if(plsRow->m_Flags & MC_PFL_IS_SCRIPTHASH)
            {
                CScriptID address=(CScriptID)(*(uint160*)(plsRow->m_Address));
                entry.push_back(Pair("address",CBitcoinAddress(address).ToString()));
                entry.push_back(Pair("islocal",false));                            
            }
            else
            {
                CKeyID address=(CKeyID)(*(uint160*)(plsRow->m_Address));
                bool is_mine=(IsMine(*pwalletMain, address)  == ISMINE_SPENDABLE);
                entry.push_back(Pair("address",CBitcoinAddress(address).ToString()));
                entry.push_back(Pair("islocal",is_mine));            
            }
            result.push_back(entry);
        }
    }    
    return  result;    
}


Array listadmins_operation(const Array& params)
{
    mc_Buffer *permissions;
    permissions=mc_gState->m_Permissions->GetPermissionList(NULL,NULL,MC_PTP_ADMIN,NULL);

    int verbose=0;
    if (params.size() > 0)    
    {
        if(paramtobool(params[0]))
        {
            verbose=1;
        }        
    }

    int details_level=0x00;
    
    if (params.size() > 1)    
    {
        details_level=params[1].get_int();
// 0x01 - hide nice details       
// 0x02 - check last changes
    }    
    
    
    uint32_t next_block=chainActive.Height()+1;
    uint32_t from_block=chainActive.Height()+2;
    if(params.size() > 2)    
    {
        from_block=params[2].get_int();
    }
    
    vector <mc_MinerInfo> admins;
    vector <int> block_admin_counts;
    int admin_count=0;
    for(int i=0;i<permissions->GetCount();i++)
    {
        mc_PermissionDetails *plsRow;
        plsRow=(mc_PermissionDetails *)(permissions->GetRow(i));
        mc_MinerInfo admin;
        admin.Zero();
        admin.m_StartBlock=plsRow->m_BlockFrom;
        admin.m_EndBlock=plsRow->m_BlockTo;
        admin.m_BlockReceived=plsRow->m_BlockReceived;
        admin.m_Address=*(uint160*)(plsRow->m_Address);
        admin.m_Flags=plsRow->m_Flags;
        if(details_level & 0x02)    
        {
            admin.m_Grants=mc_GetRecentGrants(plsRow,from_block);
        }
        
        admin.m_Active=false;
        if(next_block >= admin.m_StartBlock)
        {
            if(next_block < admin.m_EndBlock)
            {
                admin.m_Active=true;
                admin_count++;
            }                                
        }
        if(admin.m_BlockReceived == next_block)
        {
            admin.m_Pending=false;
            if(admin.m_StartBlock < admin.m_EndBlock)
            {
                admin.m_Pending=true;                
            }        
        }
        admins.push_back(admin);
    }    
        
    block_admin_counts.push_back(admin_count);
    
    mc_gState->m_Permissions->FreePermissionList(permissions);
    
    Array result;
    
    for(int i=0;i<(int)admins.size();i++)
    {
        Object entry;
        if(admins[i].m_Flags & MC_PFL_IS_SCRIPTHASH)
        {
            entry.push_back(Pair("address",CBitcoinAddress((CScriptID)(admins[i].m_Address)).ToString()));            
            entry.push_back(Pair("islocal",false));
        }
        else
        {
            bool is_mine=(IsMine(*pwalletMain, (CKeyID)(admins[i].m_Address))  == ISMINE_SPENDABLE);
            entry.push_back(Pair("address",CBitcoinAddress((CKeyID)(admins[i].m_Address)).ToString()));
            entry.push_back(Pair("islocal",is_mine));
        }
        entry.push_back(Pair("permitted",admins[i].m_Active));
        if(verbose)
        {
            entry.push_back(Pair("startblock",(int64_t)admins[i].m_StartBlock));
            entry.push_back(Pair("endblock",(int64_t)admins[i].m_EndBlock));            
        }
        string chain_state= "no-admin-permissions" ;
        if(admins[i].m_Active)
        {
            chain_state="admin-permitted";
        }
        else
        {
            if(!admins[i].m_Pending.is_null() &&  admins[i].m_Pending.get_bool())
            {
                chain_state="pending-admin-permissions";                                
            }            
        }
        entry.push_back(Pair("chainstate",chain_state));            
        
        if(details_level & 0x02)
        {
            entry.push_back(Pair("lastgrants",admins[i].m_Grants));                                    
        }
        
        result.push_back(entry);
    }   
    
    return  result;    
}

Array listminers_operation(const Array& params)
{
    mc_Buffer *permissions;
    permissions=mc_gState->m_Permissions->GetPermissionList(NULL,NULL,MC_PTP_MINE,NULL);

    int verbose=0;
    if (params.size() > 0)    
    {
        if(paramtobool(params[0]))
        {
            verbose=1;
        }        
    }

    int details_level=0x00;
    
    if (params.size() > 1)    
    {
        details_level=params[1].get_int();
// 0x01 - hide nice details       
// 0x02 - check last changes
    }    
    
    
    uint32_t next_block=chainActive.Height()+1;
    uint32_t from_block=chainActive.Height()+2;
    if(params.size() > 2)    
    {
        from_block=params[2].get_int();
    }
    
    vector <mc_MinerInfo> miners;
    vector <int> block_miner_counts;
    int miner_count=0;
    for(int i=0;i<permissions->GetCount();i++)
    {
        mc_PermissionDetails *plsRow;
        plsRow=(mc_PermissionDetails *)(permissions->GetRow(i));
        mc_MinerInfo miner;
        miner.Zero();
        miner.m_StartBlock=plsRow->m_BlockFrom;
        miner.m_EndBlock=plsRow->m_BlockTo;
        miner.m_BlockReceived=plsRow->m_BlockReceived;
        miner.m_Address=*(uint160*)(plsRow->m_Address);
        miner.m_Flags=plsRow->m_Flags;
        if(details_level & 0x02)    
        {
            miner.m_Grants=mc_GetRecentGrants(plsRow,from_block);
        }
        
        mc_gState->m_Permissions->GetMinerInfo(plsRow->m_Address,&miner.m_ConfirmedStartBlock,&miner.m_ConfirmedEndBlock,&miner.m_LastMined);
        miner.m_Active=false;
        if(next_block >= miner.m_ConfirmedStartBlock)
        {
            if(next_block < miner.m_ConfirmedEndBlock)
            {
                miner.m_Active=true;
                miner_count++;
            }                                
        }
        if(miner.m_BlockReceived == next_block)
        {
            miner.m_Pending=false;
            if(miner.m_StartBlock < miner.m_EndBlock)
            {
                miner.m_Pending=true;                
            }        
        }
        miners.push_back(miner);
    }    
        
    block_miner_counts.push_back(miner_count);
    for(int i=0;i<(int)miners.size();i++)
    {
        uint32_t try_block=next_block;
        if(miners[i].m_Active)
        {
            uint32_t shift=try_block-next_block;
            bool in_range=(try_block >= miners[i].m_ConfirmedStartBlock) && (try_block < miners[i].m_ConfirmedEndBlock);
            while((mc_gState->m_Permissions->IsBarredByDiversity(try_block,miners[i].m_LastMined,block_miner_counts[shift])) && in_range)                  
            {
                shift++;
                try_block++;
                in_range=(try_block >= miners[i].m_ConfirmedStartBlock) && (try_block < miners[i].m_ConfirmedEndBlock);
                if(in_range)
                {
                    if(shift>=block_miner_counts.size())
                    {
                        int miner_count=0;
                        for(int j=0;j<(int)miners.size();j++)
                        {
                            if((try_block >= miners[j].m_ConfirmedStartBlock) && (try_block < miners[j].m_ConfirmedEndBlock))
                            {
                                miner_count++;
                            }
                        }    
                        block_miner_counts.push_back(miner_count);
                    }
                }
            }
            if(in_range)
            {
                miners[i].m_WaitBlocks=shift;
                miners[i].m_NextAllowed=try_block;
            }
        }
        else
        {
            if(miners[i].m_ConfirmedStartBlock < miners[i].m_ConfirmedEndBlock)
            {
                if(miners[i].m_ConfirmedStartBlock > next_block)
                {
                    miners[i].m_WaitBlocks=miners[i].m_ConfirmedStartBlock-next_block;
                }
            }
        }
    }
    
    mc_gState->m_Permissions->FreePermissionList(permissions);
    
    sort(miners.begin(), miners.end(), ListMinersSort);
    CPubKey active_miner;    
    uint32_t status=mc_GetMiningStatus(active_miner);
    uint160 miner_hash=0;
    if(active_miner.IsValid())
    {
        miner_hash=active_miner.GetID();
    }
            
    Array result;
    
    for(int i=0;i<(int)miners.size();i++)
    {
        Object entry;
        bool is_mine=false;
        if(miners[i].m_Flags & MC_PFL_IS_SCRIPTHASH)
        {
            entry.push_back(Pair("address",CBitcoinAddress((CScriptID)(miners[i].m_Address)).ToString()));            
            entry.push_back(Pair("islocal",is_mine));
        }
        else
        {
            is_mine=(IsMine(*pwalletMain, (CKeyID)(miners[i].m_Address))  == ISMINE_SPENDABLE);
            entry.push_back(Pair("address",CBitcoinAddress((CKeyID)(miners[i].m_Address)).ToString()));
            entry.push_back(Pair("islocal",is_mine));
        }
        entry.push_back(Pair("permitted",miners[i].m_Active));
        if(miners[i].m_WaitBlocks != INT_MAX)
        {
            entry.push_back(Pair("diversitywaitblocks",(int64_t)miners[i].m_WaitBlocks));
//            entry.push_back(Pair("nextallowed",(int64_t)miners[i].m_NextAllowed));
        }
        else
        {
            entry.push_back(Pair("diversitywaitblocks",Value::null));
//            entry.push_back(Pair("nextallowed",Value::null));            
        }
        if(verbose)
        {
            entry.push_back(Pair("startblock",(int64_t)miners[i].m_ConfirmedStartBlock));
            entry.push_back(Pair("endblock",(int64_t)miners[i].m_ConfirmedEndBlock));            
        }
        if(miners[i].m_LastMined > 0)
        {
            entry.push_back(Pair("lastmined",(int64_t)miners[i].m_LastMined));
        }
        else
        {
            entry.push_back(Pair("lastmined",Value::null));            
        }
        string chain_state= "no-mine-permissions" ;
        if(miners[i].m_Active)
        {
            chain_state="mining-permitted";
            if(miners[i].m_WaitBlocks > 0)
            {
                chain_state="waiting-mining-diversity";                
            }
            else
            {
                if(!miners[i].m_Pending.is_null() && !miners[i].m_Pending.get_bool())
                {
                    if(!miners[i].m_Pending.get_bool())
                    {
                        chain_state="mining-permitted-pending-revoke";                
                    }
                }                
            }
        }
        else
        {
            if(!miners[i].m_Pending.is_null() &&  miners[i].m_Pending.get_bool())
            {
                chain_state="pending-mine-permissions";                                
            }            
        }
        entry.push_back(Pair("chainstate",chain_state));            
        
        if(is_mine)
        {
            string node_state=chain_state;
//            if(chain_state=="mining-permitted" )
            if(miners[i].m_Active && (miners[i].m_WaitBlocks == 0))            
            {
                node_state="";
                if(miner_hash == 0)                                             
                {
                    node_state="selecting-local-address";
                }
                if(node_state.size() == 0)if( (status & MC_MST_MINER_READY) == 0 )  node_state="disabled-by-gen-parameter";
                if(node_state.size() == 0)if( status & MC_MST_PAUSED )              node_state="mining-paused";
                if(node_state.size() == 0)if( status & MC_MST_NO_PEERS )            node_state="mining-requires-peers";
                if(node_state.size() == 0)if( status & MC_MST_NO_TXS )              node_state="waiting-new-transactions";
                if(node_state.size() == 0)if( status & MC_MST_NO_LOCKED_BLOCK )     node_state="waiting-for-lockblock";
                if(node_state.size() == 0)if( status & MC_MST_BAD_VERSION )         node_state="unsupported-protocol-version";
                if(node_state.size() == 0)if( status & MC_MST_REINDEX )             node_state="waiting-reindex-finish";
                if(node_state.size() == 0)
                {
                    if(status & MC_MST_MINING)
                    {
                        if(miners[i].m_Address == miner_hash)                   node_state="mining-block-now";
                        else                                                    node_state="other-local-address-mining";
                    }
                    else
                    {
                        if(miners[i].m_Address == miner_hash)
                        {
                            if(status & MC_MST_RECENT)
                            {
                                node_state="waiting-block-time";
                            }
                            else
                            {
                                if(status & MC_MST_DRIFT)
                                {
                                    node_state="waiting-block-time";
                                }
                                else
                                {
                                    node_state="waiting-mining-turnover";
                                }                                
                            }
                        }
                        else
                        {
                            node_state="other-local-address-preferred";
                        }
                    }
                }
            }
            entry.push_back(Pair("localstate",node_state));                                    
        }
        else
        {
            entry.push_back(Pair("localstate",Value::null));                                    
        }
        if(details_level & 0x02)
        {
            entry.push_back(Pair("lastgrants",miners[i].m_Grants));                                    
        }
        
        result.push_back(entry);
    }   
    
    return  result;
}



Value listminers(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error("Help message not found\n");

    return listminers_operation(params);
}
