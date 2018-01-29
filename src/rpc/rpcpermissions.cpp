// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2014-2016 The Bitcoin Core developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2017 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.


#include "rpc/rpcwallet.h"


string AllowedPermissions()
{
    string ret="connect,send,receive,issue,mine,admin,activate";
    if(mc_gState->m_Features->Streams())
    {
        ret += ",create";
    }
    
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
    
    if(((type & MC_PTP_RECEIVE) == 0) || (from >= to))
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
    timestamp=mc_TimeNowAsUInt();

    mc_EntityDetails entity;
    entity.Zero();
    if (entity_identifier.size())
    {        
        ParseEntityIdentifier(entity_identifier,&entity, MC_ENT_TYPE_ANY);           
        LogPrintf("mchn: Granting %s permission(s) to address %s (%ld-%ld), Entity TxID: %s, Name: %s\n",permission_type,params[1].get_str(),from,to,
                ((uint256*)entity.GetTxID())->ToString().c_str(),entity.GetName());
        
//        type=mc_gState->m_Permissions->GetPermissionType(permission_type.c_str(),entity.GetEntityType());
        type=mc_gState->m_Permissions->GetPermissionType(permission_type.c_str(),&entity);
        
        if(type == 0)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid permission");
        
        lpScript->SetEntity(entity.GetTxID()+MC_AST_SHORT_TXID_OFFSET);        
    }
    else
    {
        type=mc_gState->m_Permissions->GetPermissionType(permission_type.c_str(),&entity);
//        type=mc_gState->m_Permissions->GetPermissionType(permission_type.c_str(),MC_ENT_TYPE_NONE);
        
        if(type == 0)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid permission");
        
        LogPrintf("mchn: Granting %s permission(s) to address %s (%ld-%ld)\n",permission_type,params[1].get_str(),from,to);
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
    
    
    EnsureWalletIsUnlocked();
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

Value listpermissions(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 3)
        throw runtime_error("Help message not found\n");
    
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
    
//    type=mc_gState->m_Permissions->GetPermissionType(permission_type.c_str(),entity.GetEntityType());
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
            case MC_PTP_CREATE :entry.push_back(Pair("type", "create"));break;
            case MC_PTP_ISSUE  :entry.push_back(Pair("type", "issue"));break;
            case MC_PTP_MINE   :entry.push_back(Pair("type", "mine"));break;
            case MC_PTP_ADMIN  :entry.push_back(Pair("type", "admin"));break;
            case MC_PTP_ACTIVATE  :entry.push_back(Pair("type", "activate"));break;                
            default:take_it=false;
        }
        entry.push_back(Pair("startblock", (int64_t)plsRow->m_BlockFrom));
        entry.push_back(Pair("endblock", (int64_t)plsRow->m_BlockTo));                        
        if( (plsRow->m_BlockFrom >= plsRow->m_BlockTo) && 
                (((flags & MC_PFL_HAVE_PENDING) == 0) || !verbose) )
        {
            take_it=false;
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

