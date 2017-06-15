// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2014-2016 The Bitcoin Core developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2017 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.


#include "rpc/rpcwallet.h"

Value createupgradefromcmd(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 5)
        throw runtime_error("Help message not found\n");

    if(mc_gState->m_Features->Upgrades() == 0)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "API is not supported for this protocol version");        
    }

    if (strcmp(params[1].get_str().c_str(),"upgrade"))
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid entity type, should be stream");

    CWalletTx wtx;
    
    mc_Script *lpScript;
    
    mc_Script *lpDetailsScript;
    lpDetailsScript=NULL;
    
    mc_Script *lpDetails;
    
    int ret,type;
    string upgrade_name="";

    if (params[2].type() != str_type)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid upgrade name, should be string");
            
    if(!params[2].get_str().empty())
    {        
        upgrade_name=params[2].get_str();
    }
        
    if(mc_gState->m_Features->Streams())
    {
        if(upgrade_name == "*")
        {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid upgrade name: *");                                                                                            
        }
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

    lpScript=new mc_Script;
    
    lpDetails=new mc_Script;
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

    if(params[4].type() == obj_type)
    {
        Object objParams = params[4].get_obj();
        BOOST_FOREACH(const Pair& s, objParams) 
        {  
            if(s.name_ == "protocol-version")
            {
                if(s.value_.type() != int_type)
                {
                    throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid protocol version, expecting integer");                                                                
                }
                protocol_version_found=true;
                protocol_version=s.value_.get_int();
                if(protocol_version < 0)
                {
                    throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid protocol version, should be non-negative");                                                                                    
                }
                if(protocol_version > mc_gState->GetProtocolVersion())
                {
                    throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid protocol version, cannot upgrade to future version");                                                                                    
                }
                lpDetails->SetSpecialParamValue(MC_ENT_SPRM_UPGRADE_PROTOCOL_VERSION,(unsigned char*)&protocol_version,4);        
            }
            else
            {
                if(s.name_ == "start-block")
                {
                    if(s.value_.type() != int_type)
                    {
                        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid start block, expecting integer");                                                                
                    }
                    start_block=s.value_.get_int();
                    lpDetails->SetSpecialParamValue(MC_ENT_SPRM_UPGRADE_START_BLOCK,(unsigned char*)&start_block,4);        
                }                    
                else
                {
                    lpDetails->SetParamValue(s.name_.c_str(),s.name_.size(),(unsigned char*)s.value_.get_str().c_str(),s.value_.get_str().size());                                        
                }
            }                
        }
    }
    else
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid custom fields, expecting object");                                        
    }

    if(!protocol_version_found)
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "protocol-version is required");                                                
    }
    
    
    size_t bytes;
    const unsigned char *script;
    script=lpDetails->GetData(0,&bytes);
    
    lpDetailsScript=new mc_Script;

    int err;
    size_t elem_size;
    const unsigned char *elem;
    CScript scriptOpReturn=CScript();
    
    err=lpDetailsScript->SetNewEntityType(MC_ENT_TYPE_UPGRADE,0,script,bytes);
    if(err)
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid custom fields or upgrade name, too long");                                                        
    }

    elem = lpDetailsScript->GetData(0,&elem_size);
    scriptOpReturn << vector<unsigned char>(elem, elem + elem_size) << OP_DROP << OP_RETURN;        
    
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

Value approvefrom(const json_spirit::Array& params, bool fHelp)
{
    if (fHelp || params.size() < 2)
        throw runtime_error("Help message not found\n");

    vector<CTxDestination> addresses;
    set<CBitcoinAddress> setAddress;
    
    uint32_t approval,timestamp;

    string entity_identifier;
    entity_identifier=params[1].get_str();
    
    approval=1;
    if (params.size() > 2)    
    {
        if(!paramtobool(params[2]))
        {
            approval=0;
        }
    }
    
    timestamp=mc_TimeNowAsUInt();

    mc_EntityDetails entity;
    entity.Zero();
    ParseEntityIdentifier(entity_identifier,&entity, MC_ENT_TYPE_UPGRADE);           

    vector<CTxDestination> fromaddresses;       
    fromaddresses=ParseAddresses(params[0].get_str(),false,false);

    if(fromaddresses.size() != 1)
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Single from-address should be specified");                        
    }
    if( (IsMine(*pwalletMain, fromaddresses[0]) & ISMINE_SPENDABLE) != ISMINE_SPENDABLE )
    {
        throw JSONRPCError(RPC_WALLET_ADDRESS_NOT_FOUND, "Private key for from-address is not found in this wallet");                        
    }
        
    CKeyID *lpKeyID=boost::get<CKeyID> (&fromaddresses[0]);
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
    
    mc_Script *lpScript;
    lpScript=new mc_Script;
    
    lpScript->SetEntity(entity.GetTxID()+MC_AST_SHORT_TXID_OFFSET);        
    lpScript->SetApproval(approval, timestamp);
    
    size_t elem_size;
    const unsigned char *elem;
    CScript scriptOpReturn=CScript();
    
    for(int e=0;e<lpScript->GetNumElements();e++)
    {
        elem = lpScript->GetData(e,&elem_size);
        scriptOpReturn << vector<unsigned char>(elem, elem + elem_size) << OP_DROP;
    }    
    scriptOpReturn << OP_RETURN;
    
    LogPrintf("mchn: %s upgrade %s (%s) from address %s\n",(approval != 0) ? "Approving" : "Disapproving",
            ((uint256*)entity.GetTxID())->ToString().c_str(),entity.GetName(),CBitcoinAddress(fromaddresses[0]).ToString().c_str());
        
    
    CWalletTx wtx;
    EnsureWalletIsUnlocked();
    LOCK (pwalletMain->cs_wallet_send);    

    SendMoneyToSeveralAddresses(addresses, 0, wtx, lpScript, scriptOpReturn, fromaddresses);

    delete lpScript;
    return wtx.GetHash().GetHex();
    
}

Value listupgrades(const json_spirit::Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error("Help message not found\n");

    Array results;
    mc_Buffer *upgrades;
    
    upgrades=NULL;
    
/*    
    int verbose=0;
    if (params.size() > 1)    
    {
        if(paramtobool(params[1]))
        {
            verbose=1;
        }        
    }
*/
    
    int verbose=1;
    
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

                mc_EntityDetails upgrade_entity;
                ParseEntityIdentifier(param,&upgrade_entity, MC_ENT_TYPE_UPGRADE);           
                
                upgrades=mc_gState->m_Permissions->GetUpgradeList(upgrade_entity.GetTxID() + MC_AST_SHORT_TXID_OFFSET,upgrades);
            }
        }
    }
    else
    {        
        {
            LOCK(cs_main);
            upgrades=mc_gState->m_Permissions->GetUpgradeList(NULL,upgrades);
        }
    }
    
    if(upgrades == NULL)
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Cannot open entity database");

    set <uint160> stored_upgrades;
    map <uint64_t,int> map_sorted;
    uint160 hash;
    
    for(int i=0;i<upgrades->GetCount();i++)
    {
        mc_PermissionDetails *plsRow;
        plsRow=(mc_PermissionDetails *)(upgrades->GetRow(i));
        if(plsRow->m_Type == MC_PTP_UPGRADE)
        {
            memcpy(&hash,plsRow->m_Address,sizeof(uint160));
            stored_upgrades.insert(hash);
            map_sorted.insert(std::make_pair(plsRow->m_LastRow,i));
        }        
    }   
    for(int i=0;i<upgrades->GetCount();i++)
    {
        mc_PermissionDetails *plsRow;
        plsRow=(mc_PermissionDetails *)(upgrades->GetRow(i));
        if(plsRow->m_Type != MC_PTP_UPGRADE)
        {
            memcpy(&hash,plsRow->m_Address,sizeof(uint160));
            if(stored_upgrades.count(hash) == 0)
            {
                plsRow->m_Type = MC_PTP_UPGRADE;
                plsRow->m_BlockTo = 0;
                map_sorted.insert(std::make_pair(plsRow->m_LastRow,i));
            }
        }
    }    
    
    BOOST_FOREACH(PAIRTYPE(const uint64_t, int)& item, map_sorted)
//    for(int i=0;i<upgrades->GetCount();i++)
    {
        int i=item.second;
        Object entry;
        mc_PermissionDetails *plsRow;
        mc_PermissionDetails *plsDet;
        mc_PermissionDetails *plsPend;
        mc_EntityDetails upgrade_entity;
        bool take_it,approved;
        int flags,consensus,remaining;
        plsRow=(mc_PermissionDetails *)(upgrades->GetRow(i));
        
        upgrade_entity.Zero();
        mc_gState->m_Assets->FindEntityByShortTxID(&upgrade_entity,plsRow->m_Address);
        
        entry=UpgradeEntry(upgrade_entity.GetTxID());
        approved=true;
        if(plsRow->m_BlockFrom >= plsRow->m_BlockTo)
        {
            approved=false;  
            entry.push_back(Pair("approved", false));            
        }
        else
        {
            entry.push_back(Pair("approved", true));                        
        }
        
        take_it=true;
        flags=plsRow->m_Flags;
        consensus=plsRow->m_RequiredAdmins;

//        entry.push_back(Pair("startblock", (int64_t)upgrade_entity->UpgradeStartBlock()));
        if(plsRow->m_Type != MC_PTP_UPGRADE)
        {
            take_it=false;
        }
        if(take_it)
        {
            if( ( (plsRow->m_BlockFrom >= plsRow->m_BlockTo) && (inputStrings.size() == 0)) && 
                    (((flags & MC_PFL_HAVE_PENDING) == 0) || !verbose) )
            {
                if(verbose)
                {
                    results.push_back(entry);                    
                }
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
                        bool take_pend=true;
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
                        if(block_from >= block_to)
                        {
                            if(!approved)
                            {
                                take_pend=false;
                            }
                            pend_obj.push_back(Pair("approve", false));                            
                        }
                        else
                        {
                            if(!approved)
                            {
                                take_pend=true;
                            }
                            pend_obj.push_back(Pair("approve", true));                                                        
                        }
//                        pend_obj.push_back(Pair("startblock", (int64_t)block_from));
//                        pend_obj.push_back(Pair("endblock", (int64_t)block_to));                        
                        pend_obj.push_back(Pair("admins", pend_admins));
                        pend_obj.push_back(Pair("required", (int64_t)(consensus-pend_admins.size())));
                        if(take_pend)
                        {
                            pending.push_back(pend_obj);                            
                        }
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
    
    mc_gState->m_Permissions->FreePermissionList(upgrades);
     
    return results;
}

