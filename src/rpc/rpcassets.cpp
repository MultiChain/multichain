// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2014-2016 The Bitcoin Core developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.


#include "rpc/rpcwallet.h"

void mergeGenesisWithAssets(mc_Buffer *genesis_amounts, mc_Buffer *asset_amounts)
{
    unsigned char buf[MC_AST_ASSET_FULLREF_BUF_SIZE];
    int64_t quantity;
    memset(buf,0,MC_AST_ASSET_FULLREF_BUF_SIZE);
    
    for(int a=0;a<genesis_amounts->GetCount();a++)
    {
        memcpy(buf+MC_AST_SHORT_TXID_OFFSET,(unsigned char*)genesis_amounts->GetRow(a)+MC_AST_SHORT_TXID_OFFSET,MC_AST_SHORT_TXID_SIZE);
        mc_SetABRefType(buf,MC_AST_ASSET_REF_TYPE_SHORT_TXID);
        quantity=mc_GetABQuantity(genesis_amounts->GetRow(a));
        int row=asset_amounts->Seek(buf);
        if(row >= 0)
        {
            int64_t last=mc_GetABQuantity(asset_amounts->GetRow(row));
            quantity+=last;
            mc_SetABQuantity(asset_amounts->GetRow(row),quantity);
        }
        else
        {
            mc_SetABQuantity(buf,quantity);
            asset_amounts->Add(buf);                        
        }        
    }    
    
    genesis_amounts->Clear();
}

Value issuefromcmd(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 4)
        throw runtime_error("Help message not found\n");

    CBitcoinAddress address(params[1].get_str());
    if (!address.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");

    // Amount
    CAmount nAmount = MCP_MINIMUM_PER_OUTPUT;
    if (params.size() > 5 && params[5].type() != null_type)
    {
        nAmount = AmountFromValue(params[5]);
        if(nAmount < 0)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid amount");
    }
    // Wallet comments
    CWalletTx wtx;
    
    mc_Script *lpScript=mc_gState->m_TmpBuffers->m_RpcScript3;
    lpScript->Clear();

    int64_t quantity;
    int multiple;
    double dQuantity;
    double dUnit;
    
    dQuantity=0.;
    dUnit=1.;
    if (params.size() > 3 && params[3].type() != null_type)
    {
        dQuantity=params[3].get_real();
    }
    
    if (params.size() > 4 && params[4].type() != null_type)
    {
        dUnit=params[4].get_real();
    }
    
    if(dQuantity<0.)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid quantity. Should be positive.");
    if(dUnit<=0.)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid smallest unit. Valid Range [0.00000001 - 1].");
    if(dUnit>1.)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid smallest unit. Valid Range [0.00000001 - 1].");
    
    multiple=(int)((1 + 0.1*dUnit)/dUnit);
    if(fabs((double)multiple*dUnit-1)>0.0001)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid smallest unit. 1 should be divisible by this number.");
    
    quantity=(int64_t)(dQuantity*multiple+0.1);
    if(quantity<0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid quantity or smallest unit. ");
    if(multiple<=0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid quantity or smallest unit.");

    int64_t quantity_to_check=(int64_t)((dQuantity+0.1*dUnit)/dUnit);
    double dDelta;

    dDelta=fabs(dQuantity/dUnit-quantity);
    quantity_to_check=quantity;
    while(quantity_to_check > 1)
    {
        quantity_to_check /= 2;
        dDelta /= 2.;
    }
    
    if(dDelta>1.e-14)
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Quantity should be divisible by smallest unit.");        
        
    }

    

    if(!AddressCanReceive(address.Get()))
    {
        throw JSONRPCError(RPC_INSUFFICIENT_PERMISSIONS, "Destination address doesn't have receive permission");        
    }
    
    lpScript->SetAssetGenesis(quantity);
    
    mc_Script *lpDetailsScript=mc_gState->m_TmpBuffers->m_RpcScript1;   
    lpDetailsScript->Clear();
    mc_Script *lpDetails=mc_gState->m_TmpBuffers->m_RpcScript2;
    lpDetails->Clear();
    
    int ret,type;
    string asset_name="";
    bool is_open=false;
    bool name_is_found=false;
    uint32_t permissions=0;
    
    if (params.size() > 2 && params[2].type() != null_type)// && !params[2].get_str().empty())
    {
        if(params[2].type() == obj_type)
        {
            Object objSpecialParams = params[2].get_obj();
            BOOST_FOREACH(const Pair& s, objSpecialParams) 
            {  
                if(s.name_ == "name")
                {
                    if(!name_is_found)
                    {
                        asset_name=s.value_.get_str().c_str();
                        name_is_found=true;
                    }
                }
                if(s.name_ == "open")
                {
                    if(s.value_.type() == bool_type)
                    {
                        is_open=s.value_.get_bool();
                    }
                    else
                    {
                        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid value for 'open' field, should be boolean");                                                                
                    }
                }
                if(s.name_ == "restrict")
                {
                    if(mc_gState->m_Features->PerAssetPermissions() == 0)
                    {
                        throw JSONRPCError(RPC_NOT_SUPPORTED, "Per-asset permissions not supported for this protocol version");   
                    }
                    if(permissions == 0)
                    {
                        if(s.value_.type() == str_type)
                        {
                            permissions=mc_gState->m_Permissions->GetPermissionType(s.value_.get_str().c_str(),MC_PTP_SEND | MC_PTP_RECEIVE);
                            if(permissions == 0)
                            {
                                throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid value for restrict field");                                                                                                
                            }
                        }
                    }
                }
            }
        }
        else
        {
            if(params[2].get_str().size())
            {
                asset_name=params[2].get_str();
            }
        }
    }
    
    if(asset_name == "*")
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid asset name: *");                                                                                            
    }
    
    unsigned char buf_a[MC_AST_ASSET_REF_SIZE];    
    if(AssetRefDecode(buf_a,asset_name.c_str(),asset_name.size()))
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid asset name, looks like an asset reference");                                                                                                    
    }
    
    if(asset_name.size())
    {
        ret=ParseAssetKey(asset_name.c_str(),NULL,NULL,NULL,NULL,&type,MC_ENT_TYPE_ANY);
        if(ret != MC_ASSET_KEY_INVALID_NAME)
        {
            if(type == MC_ENT_KEYTYPE_NAME)
            {
                throw JSONRPCError(RPC_DUPLICATE_NAME, "Asset or stream with this name already exists");                                    
            }
            else
            {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid asset name");                                    
            }
        }        
    }

    lpDetails->Clear();
    lpDetails->AddElement();
    
    if(asset_name.size())
    {
        lpDetails->SetSpecialParamValue(MC_ENT_SPRM_NAME,(const unsigned char*)(asset_name.c_str()),asset_name.size());//+1);
    }        
    lpDetails->SetSpecialParamValue(MC_ENT_SPRM_ASSET_MULTIPLE,(unsigned char*)&multiple,4);
    
    if(is_open)
    {
        unsigned char b=1;        
        lpDetails->SetSpecialParamValue(MC_ENT_SPRM_FOLLOW_ONS,&b,1);
    }
    
    if(permissions)
    {
        lpDetails->SetSpecialParamValue(MC_ENT_SPRM_PERMISSIONS,(unsigned char*)&permissions,1);                                
    }

/*    
    if (params.size() > 6)
    {
        if(params[6].type() == obj_type)
        {
            Object objParams = params[6].get_obj();
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
 */ 
    int err;
    size_t bytes;
    const unsigned char *script;
    size_t elem_size;
    const unsigned char *elem;
    CScript scriptOpReturn=CScript();
    
    
        
    vector<CTxDestination> addresses;    
    vector<CTxDestination> fromaddresses;        
    int errorCode=RPC_INVALID_PARAMETER;
    string strError;    
    lpDetailsScript->Clear();
    if (params.size() > 6)
    {
        ParseRawDetails(&(params[6]),lpDetails,lpDetailsScript,&errorCode,&strError);        
        if(strError.size())
        {
            goto exitlbl;
        }
    }
    
    script=lpDetails->GetData(0,&bytes);
    lpDetailsScript->Clear();
        
    err=lpDetailsScript->SetNewEntityType(MC_ENT_TYPE_ASSET,0,script,bytes);
    if(err)
    {
        strError= "Invalid custom fields or asset name, too long";
        goto exitlbl;
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid custom fields or asset name, too long");                                                        
    }

    elem = lpDetailsScript->GetData(0,&elem_size);
    scriptOpReturn << vector<unsigned char>(elem, elem + elem_size) << OP_DROP << OP_RETURN;                    
        
    

    addresses.push_back(address.Get());
    
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
//            throw JSONRPCError(RPC_WALLET_ADDRESS_NOT_FOUND, "Private key for from-address is not found in this wallet");                        
        }
        
        set<CTxDestination> thisFromAddresses;

        BOOST_FOREACH(const CTxDestination& fromaddress, fromaddresses)
        {
            thisFromAddresses.insert(fromaddress);
        }

        CPubKey pkey;
        if(!pwalletMain->GetKeyFromAddressBook(pkey,MC_PTP_ISSUE,&thisFromAddresses))
        {
            strError= "from-address doesn't have issue permission";
            errorCode=RPC_INSUFFICIENT_PERMISSIONS;
            goto exitlbl;
//            throw JSONRPCError(RPC_INSUFFICIENT_PERMISSIONS, "from-address doesn't have issue permission");                
        }   
    }
    else
    {
        CPubKey pkey;
        if(!pwalletMain->GetKeyFromAddressBook(pkey,MC_PTP_ISSUE))
        {
            strError= "This wallet doesn't have keys with issue permission";
            errorCode=RPC_INSUFFICIENT_PERMISSIONS;
            goto exitlbl;
//            throw JSONRPCError(RPC_INSUFFICIENT_PERMISSIONS, "This wallet doesn't have keys with issue permission");                
        }        
    }
    
    {
        LOCK (pwalletMain->cs_wallet_send);

        SendMoneyToSeveralAddresses(addresses, nAmount, wtx, lpScript, scriptOpReturn,fromaddresses);
    }

exitlbl:    
    
    
    if(strError.size())
    {
        throw JSONRPCError(errorCode, strError);            
    }
                
    return wtx.GetHash().GetHex();    
}
 


Value issuecmd(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 3)
        throw runtime_error("Help message not found\n");

    Array ext_params;
    ext_params.push_back("*");
    BOOST_FOREACH(const Value& value, params)
    {
        ext_params.push_back(value);
    }
    
    return issuefromcmd(ext_params,fHelp);    
}

Value issuemorefromcmd(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 4 || params.size() > 6)
        throw runtime_error("Help message not found\n");

    CBitcoinAddress address(params[1].get_str());
    if (!address.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");
       
    // Amount
    CAmount nAmount = MCP_MINIMUM_PER_OUTPUT;
    if (params.size() > 4 && params[4].type() != null_type)
    {
        nAmount = AmountFromValue(params[4]);
        if(nAmount < 0)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid amount");
    }
    
    mc_Script *lpScript=mc_gState->m_TmpBuffers->m_RpcScript3;
    lpScript->Clear();
    unsigned char buf[MC_AST_ASSET_FULLREF_BUF_SIZE];
    memset(buf,0,MC_AST_ASSET_FULLREF_BUF_SIZE);
    int multiple=1;
    mc_EntityDetails entity;
    
    if (params.size() > 2 && params[2].type() != null_type && !params[2].get_str().empty())
    {        
        ParseEntityIdentifier(params[2],&entity, MC_ENT_TYPE_ASSET);           
        memcpy(buf,entity.GetFullRef(),MC_AST_ASSET_FULLREF_SIZE);
        multiple=entity.GetAssetMultiple();        
    }
    else
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid asset identifier");        
    }
    
    Value raw_qty=params[3];
    
    int64_t quantity = (int64_t)(raw_qty.get_real() * multiple + 0.499999);
    if(quantity<0)
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid asset quantity");        
    }
        
    mc_SetABQuantity(buf,quantity);
    
    mc_Buffer *lpBuffer=mc_gState->m_TmpBuffers->m_RpcABNoMapBuffer2;
    lpBuffer->Clear();
    
    lpBuffer->Add(buf);
    
    lpScript->SetAssetQuantities(lpBuffer,MC_SCR_ASSET_SCRIPT_TYPE_FOLLOWON);
    
    // Wallet comments
    CWalletTx wtx;
    
    

    if(!AddressCanReceive(address.Get()))
    {
        throw JSONRPCError(RPC_INSUFFICIENT_PERMISSIONS, "Destination address doesn't have receive permission");        
    }
    
    
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
    if (params.size() > 5)
    {
        ParseRawDetails(&(params[5]),lpDetails,lpDetailsScript,&errorCode,&strError);        
        if(strError.size())
        {
            goto exitlbl;
        }
    }
    lpDetailsScript->Clear();

/*    
    if (params.size() > 5)
    {
        if(params[5].type() == obj_type)
        {
            Object objParams = params[5].get_obj();
            BOOST_FOREACH(const Pair& s, objParams) 
            {  
                lpDetails->SetParamValue(s.name_.c_str(),s.name_.size(),(unsigned char*)s.value_.get_str().c_str(),s.value_.get_str().size());                
            }
        }
        else
        {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid extra-params, expecting object");                                        
        }
    }
*/    
    int err;
    size_t bytes;
    const unsigned char *script;
    size_t elem_size;
    const unsigned char *elem;
    
    script=lpDetails->GetData(0,&bytes);
    if(bytes > 0)
    {
        lpDetailsScript->SetEntity(entity.GetTxID()+MC_AST_SHORT_TXID_OFFSET);
        err=lpDetailsScript->SetNewEntityType(MC_ENT_TYPE_ASSET,1,script,bytes);
        if(err)
        {
            strError= "Invalid custom fields, too long";
            goto exitlbl;
        }

        elem = lpDetailsScript->GetData(0,&elem_size);
        scriptOpReturn << vector<unsigned char>(elem, elem + elem_size) << OP_DROP;
        elem = lpDetailsScript->GetData(1,&elem_size);
        scriptOpReturn << vector<unsigned char>(elem, elem + elem_size) << OP_DROP << OP_RETURN;                
    }
        
    

    addresses.push_back(address.Get());
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
//            throw JSONRPCError(RPC_WALLET_ADDRESS_NOT_FOUND, "Private key for from-address is not found in this wallet");                        
        }        
    }
    else
    {
/*        
        CPubKey pkey;
        if(!pwalletMain->GetKeyFromAddressBook(pkey,MC_PTP_ISSUE))
        {
            throw JSONRPCError(RPC_WALLET_ADDRESS_NOT_FOUND, "This wallet doesn't have keys with issue permission");                
        }        
 */ 
    }
    
    if(mc_gState->m_Assets->FindEntityByFullRef(&entity,buf))
    {
        if(entity.AllowedFollowOns())
        {
            if(fromaddresses.size() == 1)
            {
                CKeyID *lpKeyID=boost::get<CKeyID> (&fromaddresses[0]);
                if(lpKeyID != NULL)
                {
                    if(mc_gState->m_Permissions->CanIssue(entity.GetTxID(),(unsigned char*)(lpKeyID)) == 0)
                    {
                        strError= "Issuing more units for this asset is not allowed from this address";
                        errorCode=RPC_INSUFFICIENT_PERMISSIONS;
                        goto exitlbl;
//                        throw JSONRPCError(RPC_INSUFFICIENT_PERMISSIONS, "Issuing more units for this asset is not allowed from this address");                                                                        
                    }                                                 
                }
                else
                {
                    strError= "Issuing more units is allowed only from P2PKH addresses";
                    goto exitlbl;
//                    throw JSONRPCError(RPC_INVALID_PARAMETER, "Issuing more units is allowed only from P2PKH addresses");                                                
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
                        if(mc_gState->m_Permissions->CanIssue(entity.GetTxID(),(unsigned char*)(&keyID)))
                        {
                            issuer_found=true;
                        }
                    }
                }                    
                if(!issuer_found)
                {
                    strError= "Issuing more units for this asset is not allowed from this wallet";
                    errorCode=RPC_INSUFFICIENT_PERMISSIONS;
                    goto exitlbl;
//                    throw JSONRPCError(RPC_INSUFFICIENT_PERMISSIONS, "Issuing more units for this asset is not allowed from this wallet");                                                                                            
                }
            }
        }
        else
        {
            strError= "Issuing more units not allowed for this asset: "+params[2].get_str();
            errorCode=RPC_NOT_ALLOWED;
            goto exitlbl;
//            throw JSONRPCError(RPC_NOT_ALLOWED, "Issuing more units not allowed for this asset: "+params[2].get_str());                            
        }
    }   
    else
    {
        strError= "Asset not found";
        errorCode=RPC_ENTITY_NOT_FOUND;
        goto exitlbl;
//        throw JSONRPCError(RPC_ENTITY_NOT_FOUND, "Asset not found");                
    }
    
    
    {
        LOCK (pwalletMain->cs_wallet_send);

        SendMoneyToSeveralAddresses(addresses, nAmount, wtx, lpScript, scriptOpReturn,fromaddresses);
    }
    
exitlbl:    
    
    if(strError.size())
    {
        throw JSONRPCError(errorCode, strError);            
    }
                
    return wtx.GetHash().GetHex();    
}
 
Value issuemorecmd(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 3)
        throw runtime_error("Help message not found\n");

    Array ext_params;
    ext_params.push_back("*");
    BOOST_FOREACH(const Value& value, params)
    {
        ext_params.push_back(value);
    }
    
    return issuemorefromcmd(ext_params,fHelp);
}    

Value getmultibalances(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 5)
        throw runtime_error("Help message not found\n");

    isminefilter filter = ISMINE_SPENDABLE;
    
    Object balances;

    bool fUnlockedOnly=true;
    bool fIncludeWatchOnly=false;    
    int nMinDepth = 1;
    
    if(params.size() > 2)
    {
        nMinDepth = params[2].get_int();
    }
    
    if(params.size() > 3)
    {
        if(params[3].get_bool())
        {
            fIncludeWatchOnly=true;
        }
    }
    
    if(params.size() > 4)
    {
        if(params[4].get_bool())
        {
            fUnlockedOnly=false;
        }
    }
    
    if(fIncludeWatchOnly)
    {
        filter = filter | ISMINE_WATCH_ONLY;        
    }
    
    set<string> setAddresses;
    set<string> setAddressesWithBalances;
    set<uint160> setAddressUints;
    set<uint160> *lpSetAddressUint=NULL;
    CTxDestination dest;
    
    if(params.size() > 0)
    {
        if( (params[0].type() != str_type) || (params[0].get_str() != "*") )
        {        
            filter = filter | ISMINE_WATCH_ONLY;        
            
            setAddresses=ParseAddresses(params[0],filter);
            if(setAddresses.size() == 0)
            {
                return balances;
            }
        
            BOOST_FOREACH(string str_addr, setAddresses) 
            {
                CBitcoinAddress address(str_addr);
                dest=address.Get();
                const CKeyID *lpKeyID=boost::get<CKeyID> (&dest);
                const CScriptID *lpScriptID=boost::get<CScriptID> (&dest);
                if(lpKeyID)
                {
                    setAddressUints.insert(*(uint160*)lpKeyID);
                }
                else
                {
                    if(lpScriptID)
                    {
                        setAddressUints.insert(*(uint160*)lpScriptID);
                    }
               }
            }

            if(setAddressUints.size())
            {
                lpSetAddressUint=&setAddressUints;
            }            
        }
    }
    
    vector<string> inputStrings;
    
    if (params.size() > 1 && params[1].type() != null_type && ((params[1].type() != str_type) || (params[1].get_str() !="*" ) ) )
    {        
        if(params[1].type() == str_type)
        {
            inputStrings.push_back(params[1].get_str());
            if(params[1].get_str() == "")
            {
                return balances;                
            }
        }
        else
        {
            inputStrings=ParseStringList(params[1]);
            if(inputStrings.size() == 0)
            {
                return balances;
            }
        }
    }
    
    set<uint256> setAssets;        
    if(inputStrings.size())
    {
        for(int is=0;is<(int)inputStrings.size();is++)
        {
            mc_EntityDetails entity;
            ParseEntityIdentifier(inputStrings[is],&entity, MC_ENT_TYPE_ASSET);           
            uint256 hash=*(uint256*)entity.GetTxID();
            if (setAssets.count(hash))
            {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, duplicate asset: " + inputStrings[is]);                        
            }
            setAssets.insert(hash);
        }
    }

    mc_Buffer *asset_amounts=mc_gState->m_TmpBuffers->m_RpcABBuffer1;
    asset_amounts->Clear();
    mc_Buffer *addresstxid_amounts=mc_gState->m_TmpBuffers->m_RpcBuffer1;
    addresstxid_amounts->Clear();
    unsigned char buf[80+MC_AST_ASSET_QUANTITY_SIZE];
    unsigned char totbuf[80+MC_AST_ASSET_QUANTITY_SIZE];
    int64_t quantity;
    int row;
    unsigned char *ptr;  
    
    assert(pwalletMain != NULL);

    
    {
        LOCK(cs_main);
        
        mc_Script *lpScript=mc_gState->m_TmpBuffers->m_RpcScript3;
        lpScript->Clear();    
        
        asset_amounts->Clear();

        addresstxid_amounts->Initialize(80,80+MC_AST_ASSET_QUANTITY_SIZE,MC_BUF_MODE_MAP);
        addresstxid_amounts->Clear();

        memset(totbuf,0,80+MC_AST_ASSET_QUANTITY_SIZE);
        addresstxid_amounts->Add(totbuf,totbuf+80);
        
        
        vector<COutput> vecOutputs;
        pwalletMain->AvailableCoins(vecOutputs, false, NULL, fUnlockedOnly,true, 0, lpSetAddressUint);
        BOOST_FOREACH(const COutput& out, vecOutputs) 
        {        
            if(!out.IsTrustedNoDepth())
            {
                if (out.nDepth < nMinDepth)
                {
                    continue;           
                }
            }
            
            CTxOut txout;
            uint256 hash=out.GetHashAndTxOut(txout);
            
            string str_addr;
            CTxDestination address;
            if (!ExtractDestination(txout.scriptPubKey, address))
            {
                continue;
            }
            
            str_addr=CBitcoinAddress(address).ToString();
            if ( (setAddresses.size()>0) && (setAddresses.count(str_addr) == 0) )
            {
                continue;
            }
            isminetype fIsMine=pwalletMain->IsMine(txout);

            if (!(fIsMine & filter))
            {
                continue;        
            }
         
            memset(totbuf,0,80+MC_AST_ASSET_QUANTITY_SIZE);
            memset(buf,0,80+MC_AST_ASSET_QUANTITY_SIZE);
            memcpy(buf,str_addr.c_str(),str_addr.size());
            
            quantity=txout.nValue;
            if(quantity > 0)
            {
                quantity+=mc_GetLE(addresstxid_amounts->GetRow(0)+80,MC_AST_ASSET_QUANTITY_SIZE);
                mc_PutLE(addresstxid_amounts->GetRow(0)+80,&quantity,MC_AST_ASSET_QUANTITY_SIZE);
                row=addresstxid_amounts->Seek(buf);
                quantity=txout.nValue;
                if(row >= 0)
                {
                    quantity+=mc_GetLE(addresstxid_amounts->GetRow(row)+80,MC_AST_ASSET_QUANTITY_SIZE);
                    mc_PutLE(addresstxid_amounts->GetRow(row)+80,&quantity,MC_AST_ASSET_QUANTITY_SIZE);
                }
                else
                {                             
                    mc_PutLE(buf+80,&quantity,MC_AST_ASSET_QUANTITY_SIZE);
                    addresstxid_amounts->Add(buf,buf+80);                        
                }                
            }
            asset_amounts->Clear();
            if(CreateAssetBalanceList(txout,asset_amounts,lpScript))
            {
                for(int a=0;a<asset_amounts->GetCount();a++)
                {
                    const unsigned char *txid;
                    txid=NULL;
                    ptr=(unsigned char *)asset_amounts->GetRow(a);
                    if(mc_GetABRefType(ptr) == MC_AST_ASSET_REF_TYPE_GENESIS)
//                    if(mc_GetLE(ptr,4) == 0)
                    {
//                        hash=out.tx->GetHash();
                        txid=(unsigned char*)&hash;
                    }
                    else
                    {
                        mc_EntityDetails entity;

                        if(mc_gState->m_Assets->FindEntityByFullRef(&entity,ptr))
                        {
                            txid=entity.GetTxID();
                        }                                
                    }
                    if(txid)
                    {
                        if(setAssets.size())
                        {
//                            hash=*(uint256*)txid;
                            if(setAssets.count(*(uint256*)txid) == 0)
                            {
                                txid=NULL;
                            }
                        }
                    }
                    
                    if(txid)
                    {
                        quantity=mc_GetABQuantity(ptr);
                        memcpy(totbuf+48,txid,32);
                        row=addresstxid_amounts->Seek(totbuf);
                        if(row >= 0)
                        {
                            quantity+=mc_GetLE(addresstxid_amounts->GetRow(row)+80,MC_AST_ASSET_QUANTITY_SIZE);
                            mc_PutLE(addresstxid_amounts->GetRow(row)+80,&quantity,MC_AST_ASSET_QUANTITY_SIZE);
                        }
                        else
                        {                             
                            mc_PutLE(totbuf+80,&quantity,MC_AST_ASSET_QUANTITY_SIZE);
                            addresstxid_amounts->Add(totbuf,totbuf+80);                        
                        }                
                        
                        quantity=mc_GetABQuantity(ptr);
                        memcpy(buf+48,txid,32);
                        row=addresstxid_amounts->Seek(buf);
                        if(row >= 0)
                        {
                            quantity+=mc_GetLE(addresstxid_amounts->GetRow(row)+80,MC_AST_ASSET_QUANTITY_SIZE);
                            mc_PutLE(addresstxid_amounts->GetRow(row)+80,&quantity,MC_AST_ASSET_QUANTITY_SIZE);
                        }
                        else
                        {                             
                            mc_PutLE(buf+80,&quantity,MC_AST_ASSET_QUANTITY_SIZE);
                            addresstxid_amounts->Add(buf,buf+80);                        
                        }                
                    }
                }                
            }        
        }

        bool take_total=false;
        bool have_addr=true;
        
        memset(totbuf,0,80+MC_AST_ASSET_QUANTITY_SIZE);
        while(have_addr)
        {
            memset(buf,0,80+MC_AST_ASSET_QUANTITY_SIZE);
            Array addr_balances;
            set<uint256> setAssetsWithBalances;
            int64_t btc=0;
            for(int i=0;i<addresstxid_amounts->GetCount();i++)
            {
                bool take_it=false;
                ptr=addresstxid_amounts->GetRow(i);
                if(ptr[47] == 0)
                {
                    if(take_total || (memcmp(ptr,totbuf,48) != 0))
                    {
                        if(take_total || (memcmp(buf,totbuf,48) == 0))
                        {
                            take_it=true;
                            if(!take_total)
                            {
                                memcpy(buf,ptr,48);
                            }
                        }
                        else
                        {
                            if(memcmp(ptr,buf,48) == 0)
                            {
                                take_it=true;
                            }
                        }
                    }
                }
                if(take_it)
                {
                    quantity=mc_GetLE(ptr+80,MC_AST_ASSET_QUANTITY_SIZE);
                    if(memcmp(ptr+48,totbuf+48,32) == 0)
                    {
                        btc=quantity;
                    }
                    else
                    {
                        Object asset_entry;
                        asset_entry=AssetEntry(ptr+48,quantity,0x00);
                        addr_balances.push_back(asset_entry);  
                        if(setAssets.size())
                        {
                            uint256 issue_txid=*(uint256*)(ptr+48);
                            setAssetsWithBalances.insert(issue_txid);
                        }
                    }
                    ptr[47]=0x01;
                }
            }
            if(setAssets.size())
            {
                BOOST_FOREACH(const uint256& rem_asset, setAssets) 
                {
                    if(setAssetsWithBalances.count(rem_asset) == 0)
                    {
                        Object asset_entry;
                        asset_entry=AssetEntry((unsigned char*)&rem_asset,0,0x00);
                        addr_balances.push_back(asset_entry);   
                    }
                }                
            }
            if(MCP_WITH_NATIVE_CURRENCY)
            {
                Object asset_entry;
                asset_entry=AssetEntry(NULL,btc,0x00);
                addr_balances.push_back(asset_entry);                        
            }
            
            string out_addr="";
            if(take_total)
            {
                have_addr=false;
                out_addr="total";
                if(setAddresses.size())
                {
                    BOOST_FOREACH(const string& rem_addr, setAddresses) 
                    {
                        if(setAddressesWithBalances.count(rem_addr) == 0)
                        {
                            Array empty_balances;
                            if(setAssets.size())
                            {
                                BOOST_FOREACH(const uint256& rem_asset, setAssets) 
                                {
                                    Object asset_entry;
                                    asset_entry=AssetEntry((unsigned char*)&rem_asset,0,0x00);
                                    empty_balances.push_back(asset_entry);   
                                }                
                            }
                            balances.push_back(Pair(rem_addr, empty_balances));                                    
                        }
                    }
                }
            }
            else
            {
                if(memcmp(buf,totbuf,48) == 0)
                {
                    take_total=true;
                }
                else
                {
                    out_addr=string((char*)buf);
                }
            }       
            if(out_addr.size())
            {
                setAddressesWithBalances.insert(out_addr);
                balances.push_back(Pair(out_addr, addr_balances));                
            }
        }
        
        
    }        
        
    return balances;    
}

Value getaddressbalances(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 3)
        throw runtime_error("Help message not found\n");

    vector<CTxDestination> fromaddresses;        
    fromaddresses=ParseAddresses(params[0].get_str(),false,true);
    
    if(fromaddresses.size() != 1)
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Single from-address should be specified");                        
    }
    
    isminefilter filter = ISMINE_SPENDABLE;
    
    filter = filter | ISMINE_WATCH_ONLY;

    bool fUnlockedOnly=true;
    
    if(params.size() > 2)
        if(params[2].get_bool())
            fUnlockedOnly=false;
    
    
    
    set<CBitcoinAddress> setAddress;
    
    
    bool check_account=true;
    
    BOOST_FOREACH(const CTxDestination& fromaddress, fromaddresses)
    {
        setAddress.insert(fromaddress);
    }
        
    int nMinDepth = 1;
    if (params.size() > 1)
        nMinDepth = params[1].get_int();

    mc_Buffer *asset_amounts=mc_gState->m_TmpBuffers->m_RpcABBuffer1;
    asset_amounts->Clear();
    
    mc_Buffer *genesis_amounts=mc_gState->m_TmpBuffers->m_RpcBuffer1;
    genesis_amounts->Initialize(32,32+MC_AST_ASSET_QUANTITY_SIZE,MC_BUF_MODE_MAP);
    genesis_amounts->Clear();
    
    mc_Script *lpScript=mc_gState->m_TmpBuffers->m_RpcScript3;
    lpScript->Clear();    

    int last_size=0;
    Array assets;
    CAmount totalBTC=0;
    vector<COutput> vecOutputs;        
    assert(pwalletMain != NULL);
    
    uint160 addr=0;
    
    if(fromaddresses.size() == 1)
    {
        CTxDestination addressRet=fromaddresses[0];        
        const CKeyID *lpKeyID=boost::get<CKeyID> (&addressRet);
        const CScriptID *lpScriptID=boost::get<CScriptID> (&addressRet);
        if(lpKeyID)
        {
            addr=*(uint160*)lpKeyID;
        }
        if(lpScriptID)
        {
            addr=*(uint160*)lpScriptID;
        }
    }
    
    pwalletMain->AvailableCoins(vecOutputs, false, NULL, fUnlockedOnly,true,addr);
    BOOST_FOREACH(const COutput& out, vecOutputs) {
        
/*        
        if(out.nDepth == 0)
        {
            if(!out.tx->IsTrusted(out.nDepth))
            {
                continue;
            }
        }
        else
        {
 */ 
        if(!out.IsTrustedNoDepth())
        {
            if (out.nDepth < nMinDepth)
            {
                continue;           
            }
        }
/*
         }
*/
        CTxOut txout;
        uint256 hash=out.GetHashAndTxOut(txout);
        if(check_account)
        {
            CTxDestination address;
            if (!ExtractDestination(txout.scriptPubKey, address))
                continue;
            
            if (!setAddress.count(address))
                continue;
        }
        
        isminetype fIsMine=pwalletMain->IsMine(txout);
        if (!(fIsMine & filter))
        {
            continue;        
        }
        
        
        
        CAmount nValue = txout.nValue;
        totalBTC+=nValue;
        if(CreateAssetBalanceList(txout,asset_amounts,lpScript))
        {
            unsigned char *ptr;
            string assetref="";
            int64_t quantity;
            unsigned char buf[MC_AST_ASSET_FULLREF_BUF_SIZE];
            memset(buf,0,MC_AST_ASSET_FULLREF_BUF_SIZE);
            int n;
            bool is_genesis;
            
            n=asset_amounts->GetCount();
            
            for(int a=last_size;a<n;a++)
            {
                Object asset_entry;
                ptr=(unsigned char *)asset_amounts->GetRow(a);
                is_genesis=false;
                if(mc_GetABRefType(ptr) == MC_AST_ASSET_REF_TYPE_GENESIS)
                {
                    mc_EntityDetails entity;
                    quantity=mc_GetABQuantity(asset_amounts->GetRow(a));
                    if(mc_gState->m_Assets->FindEntityByTxID(&entity,(unsigned char*)&hash))
                    {
                        ptr=(unsigned char *)entity.GetFullRef();
                        memcpy(buf,ptr,MC_AST_ASSET_FULLREF_SIZE);
                        int row=asset_amounts->Seek(buf);
                        if(row >= 0)
                        {
                            int64_t last=mc_GetABQuantity(asset_amounts->GetRow(row));
                            quantity+=last;
                            mc_SetABQuantity(asset_amounts->GetRow(row),quantity);
                        }
                        else
                        {
                            mc_SetABQuantity(buf,quantity);
                            asset_amounts->Add(buf);                        
                        }
                    }                
                    
                    if(is_genesis)
                    {
                        int row=genesis_amounts->Seek(&hash);
                        if(row >= 0)
                        {
                            int64_t last=mc_GetLE(genesis_amounts->GetRow(row)+32,MC_AST_ASSET_QUANTITY_SIZE);
                            quantity+=last;
                            mc_PutLE(genesis_amounts->GetRow(row)+32,&quantity,MC_AST_ASSET_QUANTITY_SIZE);
                        }
                        else
                        {
                            mc_SetABQuantity(buf,quantity);
                            genesis_amounts->Add(&hash,buf+MC_AST_ASSET_QUANTITY_OFFSET);                        
                        }                        
                    }
                }                
            }
            last_size=asset_amounts->GetCount();
        }
    }
    
    
    unsigned char *ptr;
    string assetref="";

    mergeGenesisWithAssets(genesis_amounts, asset_amounts);
    
    for(int a=0;a<asset_amounts->GetCount();a++)
    {
        Object asset_entry;
        ptr=(unsigned char *)asset_amounts->GetRow(a);
        
        mc_EntityDetails entity;
        const unsigned char *txid;
        
        if(mc_gState->m_Assets->FindEntityByFullRef(&entity,ptr))
        {
            txid=entity.GetTxID();
            asset_entry=AssetEntry(txid,mc_GetABQuantity(ptr),0x00);
            assets.push_back(asset_entry);
        }        
    }
                
    for(int a=0;a<genesis_amounts->GetCount();a++)
    {
        Object asset_entry;
        ptr=(unsigned char *)genesis_amounts->GetRow(a);
        
        asset_entry=AssetEntry(ptr,mc_GetLE(ptr+32,MC_AST_ASSET_QUANTITY_SIZE),0x00);
        assets.push_back(asset_entry);
    }
    
    if(MCP_WITH_NATIVE_CURRENCY)
    {
        Object asset_entry;
        asset_entry=AssetEntry(NULL,totalBTC,0x00);
        assets.push_back(asset_entry);        
    }
    

/* MCHN END */        
    return assets;
}



Value getassetbalances(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 4)
        throw runtime_error("Help message not found\n");
    
    bool check_account=false;
    
    isminefilter filter = ISMINE_SPENDABLE;
    if(params.size() > 2)
        if(params[2].get_bool())
            filter = filter | ISMINE_WATCH_ONLY;

    bool fUnlockedOnly=true;
    
    if(params.size() > 3)
        if(params[3].get_bool())
            fUnlockedOnly=false;
    
    set<CBitcoinAddress> setAddress;
        
    if (params.size() > 0)
    {
        if (params[0].get_str() != "*") 
        {
            if (params[0].get_str() != "") 
            {
                if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)
                {
                    throw JSONRPCError(RPC_NOT_SUPPORTED, "Accounts are not supported with scalable wallet - if you need getassetbalances, run multichaind -walletdbversion=1 -rescan, but the wallet will perform worse");        
                }            
            }
            BOOST_FOREACH(const PAIRTYPE(CBitcoinAddress, CAddressBookData)& item, pwalletMain->mapAddressBook)
            {
                const CBitcoinAddress& address = item.first;
                const string& strName = item.second.name;
                    if (strName == params[0].get_str())
                    {
                       setAddress.insert(address);                    
                    }
                }
            check_account=true;
        }
    }

    
    int nMinDepth = 1;
    if (params.size() > 1)
        nMinDepth = params[1].get_int();

    mc_Buffer *asset_amounts=mc_gState->m_TmpBuffers->m_RpcABBuffer1;
    asset_amounts->Clear();
    
    mc_Buffer *genesis_amounts=mc_gState->m_TmpBuffers->m_RpcBuffer1;
    genesis_amounts->Initialize(32,32+MC_AST_ASSET_QUANTITY_SIZE,MC_BUF_MODE_MAP);
    genesis_amounts->Clear();
    
    mc_Script *lpScript=mc_gState->m_TmpBuffers->m_RpcScript3;
    lpScript->Clear();    

    int last_size=0;
    Array assets;
    CAmount totalBTC=0;
    vector<COutput> vecOutputs;
    assert(pwalletMain != NULL);
    pwalletMain->AvailableCoins(vecOutputs, false, NULL, fUnlockedOnly,true);
    BOOST_FOREACH(const COutput& out, vecOutputs) {
        
/*        
        if(out.nDepth == 0)
        {
            if(!out.tx->IsTrusted(out.nDepth))
            {
                continue;
            }
        }
        else
        {
 */ 
            if(!out.IsTrustedNoDepth())
            {
                if (out.nDepth < nMinDepth)
                {
                    continue;           
                }
            }
/*
        }
*/
        CTxOut txout;
        uint256 hash=out.GetHashAndTxOut(txout);
        
        if(check_account)
        {
            CTxDestination address;
            if (!ExtractDestination(txout.scriptPubKey, address))
                continue;

            if (!setAddress.count(address))
                continue;
        }
        
        isminetype fIsMine=pwalletMain->IsMine(txout);

        if (!(fIsMine & filter))
        {
            continue;        
        }
        
        CAmount nValue = txout.nValue;
        totalBTC+=nValue;
        
        if(CreateAssetBalanceList(txout,asset_amounts,lpScript))
        {
            unsigned char *ptr;
            string assetref="";
            int64_t quantity;
            unsigned char buf[MC_AST_ASSET_FULLREF_BUF_SIZE];
            memset(buf,0,MC_AST_ASSET_FULLREF_BUF_SIZE);
            int n;
            bool is_genesis;
            
            n=asset_amounts->GetCount();
            
            for(int a=last_size;a<n;a++)
            {
                Object asset_entry;
                ptr=(unsigned char *)asset_amounts->GetRow(a);
                is_genesis=false;
                if(mc_GetABRefType(ptr) == MC_AST_ASSET_REF_TYPE_GENESIS)
                {
                    mc_EntityDetails entity;
                    quantity=mc_GetABQuantity(asset_amounts->GetRow(a));
                    if(mc_gState->m_Assets->FindEntityByTxID(&entity,(unsigned char*)&hash))
                    {
                        ptr=(unsigned char *)entity.GetFullRef();
                        memcpy(buf,ptr,MC_AST_ASSET_FULLREF_SIZE);
                        int row=asset_amounts->Seek(buf);
                        if(row >= 0)
                        {
                            int64_t last=mc_GetABQuantity(asset_amounts->GetRow(row));
                            quantity+=last;
                            mc_SetABQuantity(asset_amounts->GetRow(row),quantity);
                        }
                        else
                        {
                            mc_SetABQuantity(buf,quantity);
                            asset_amounts->Add(buf);                        
                        }
                    }                
                    
                    if(is_genesis)
                    {
                        int row=genesis_amounts->Seek(&hash);
                        if(row >= 0)
                        {
                            int64_t last=mc_GetLE(genesis_amounts->GetRow(row)+32,MC_AST_ASSET_QUANTITY_SIZE);
                            quantity+=last;
                            mc_PutLE(genesis_amounts->GetRow(row)+32,&quantity,MC_AST_ASSET_QUANTITY_SIZE);
                        }
                        else
                        {
                            mc_SetABQuantity(buf,quantity);
                            genesis_amounts->Add(&hash,buf+MC_AST_ASSET_QUANTITY_OFFSET);                        
                        }                        
                    }
                }                
            }
            last_size=asset_amounts->GetCount();
        }
    }
    
    
    unsigned char *ptr;
    string assetref="";

    mergeGenesisWithAssets(genesis_amounts, asset_amounts);   
    
    for(int a=0;a<asset_amounts->GetCount();a++)
    {
        Object asset_entry;
        ptr=(unsigned char *)asset_amounts->GetRow(a);
        
        mc_EntityDetails entity;
        const unsigned char *txid;
        
        if(mc_gState->m_Assets->FindEntityByFullRef(&entity,ptr))
        {
            txid=entity.GetTxID();
            asset_entry=AssetEntry(txid,mc_GetABQuantity(ptr),0x00);
            assets.push_back(asset_entry);
        }        
    }
    
    for(int a=0;a<genesis_amounts->GetCount();a++)
    {
        Object asset_entry;
        ptr=(unsigned char *)genesis_amounts->GetRow(a);
        
        asset_entry=AssetEntry(ptr,mc_GetLE(ptr+32,MC_AST_ASSET_QUANTITY_SIZE),0x00);
        assets.push_back(asset_entry);
    }
        
/* MCHN END */        
    return assets;
}


Value gettotalbalances(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 3)
        throw runtime_error("Help message not found\n");

    Array new_params;
    
    new_params.push_back("*");
    
    for(int i=0;i<(int)params.size();i++)
    {
        new_params.push_back(params[i]);
    }

    return  getassetbalances(new_params,fHelp);
}

Value getassetinfo(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        mc_ThrowHelpMessage("getassetinfo");        
//       throw runtime_error("Help message not found\n");
    
    if(params[0].type() != str_type)
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid asset identifier, expected string");                                                        
    }
    
    uint32_t output_level;
    mc_EntityDetails entity;
    ParseEntityIdentifier(params[0].get_str(),&entity, MC_ENT_TYPE_ASSET);           

    output_level=0x07;
    
    if (params.size() > 1)    
    {
        if(paramtobool(params[1]))
        {
            output_level|=0x20;
        }
    }
    
    return AssetEntry(entity.GetTxID(),-1,output_level);    
}

Value listassets(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 4)
       throw runtime_error("Help message not found\n");

    mc_Buffer *assets;
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
    
    assets=NULL;
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
                ParseEntityIdentifier(inputStrings[is],&entity, MC_ENT_TYPE_ASSET);           
                uint256 hash=*(uint256*)entity.GetTxID();

                assets=mc_gState->m_Assets->GetEntityList(assets,(unsigned char*)&hash,MC_ENT_TYPE_ASSET);
            }
        }
    }
    else
    {        
        {
            LOCK(cs_main);
            assets=mc_gState->m_Assets->GetEntityList(assets,NULL,MC_ENT_TYPE_ASSET);
        }
    }
    
    if(assets == NULL)
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Cannot open asset database");

    output_level=0x0F;
    
    if (params.size() > 1)    
    {
        if(paramtobool(params[1]))
        {
            output_level|=0x20;
        }
    }
    
    mc_AdjustStartAndCount(&count,&start,assets->GetCount());
    
    Array partial_results;
    int unconfirmed_count=0;
    if(count > 0)
    {
        for(int i=0;i<assets->GetCount();i++)
        {
            Object entry;

            txid=assets->GetRow(i);
            entry=AssetEntry(txid,-1,output_level);
            if(entry.size()>0)
            {
                BOOST_FOREACH(const Pair& p, entry) 
                {
                    if(p.name_ == "assetref")
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
        
        for(int i=0;i<assets->GetCount();i++)
        {
            Object entry;

            txid=assets->GetRow(i);

            entry=AssetEntry(txid,-1,output_level);
            if(entry.size()>0)
            {
                BOOST_FOREACH(const Pair& p, entry) 
                {
                    if(p.name_ == "assetref")
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
    if(count != assets->GetCount())
    {
        return_partial=true;
    }
    mc_gState->m_Assets->FreeEntityList(assets);
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
Object ListAssetTransactions(const CWalletTx& wtx, mc_EntityDetails *entity, bool fLong, mc_Buffer *amounts,mc_Script *lpScript)
{
    Object entry;
    unsigned char bufEmptyAssetRef[MC_AST_ASSET_QUANTITY_OFFSET];
    uint32_t new_entity_type;
    set<uint256> streams_already_seen;
//    Array aMetaData;
    Array aItems;
    uint32_t format;
    unsigned char *chunk_hashes;
    int chunk_count;   
    int64_t total_chunk_size,out_size;
    uint32_t retrieve_status;
    Array aFormatMetaData;
    vector<Array> aFormatMetaDataPerOutput;
//    string format_text_str;
    
    double units=1.;
    units= 1./(double)(entity->GetAssetMultiple());
    
    uint256 hash=wtx.GetHash();
    
    map <CTxDestination,int64_t> mAddresses;
    
    memset(bufEmptyAssetRef,0,MC_AST_ASSET_QUANTITY_OFFSET);
    
    BOOST_FOREACH(const CTxIn& txin, wtx.vin)
    {
        CTxOut prevout;
                
        int err;
        const CWalletTx& prev=pwalletTxsMain->GetWalletTx(txin.prevout.hash,NULL,&err);
        if(err == MC_ERR_NOERROR)
        {
            if (txin.prevout.n < prev.vout.size())
            {
                prevout=prev.vout[txin.prevout.n];
                CTxDestination address;
                int required=0;
                
                ExtractDestination(prevout.scriptPubKey, address);
                
                string strFailReason;
                amounts->Clear();
                if(CreateAssetBalanceList(prevout,amounts,lpScript,&required))
                {
                    for(int i=0;i<amounts->GetCount();i++)
                    {
                        int64_t quantity=-1;
                        if(memcmp(entity->GetFullRef(),amounts->GetRow(i),MC_AST_ASSET_FULLREF_SIZE) == 0)
                        {
                            quantity=mc_GetABQuantity(amounts->GetRow(i));
                        }
                        else
                        {
                            if(memcmp(entity->GetTxID(),&(txin.prevout.hash),sizeof(uint256)) == 0)
                            {
                                if( mc_GetABRefType(amounts->GetRow(i)) == MC_AST_ASSET_REF_TYPE_GENESIS )                    
//                                if(memcmp(bufEmptyAssetRef,amounts->GetRow(i),MC_AST_ASSET_QUANTITY_OFFSET) == 0)
                                {
                                    quantity=mc_GetABQuantity(amounts->GetRow(i));
                                }                            
                            }
                        }

                        if(quantity >= 0)
                        {                        
                            map<CTxDestination, int64_t>::iterator itold = mAddresses.find(address);
                            if (itold == mAddresses.end())
                            {
                                mAddresses.insert(make_pair(address, -quantity));
                            }
                            else
                            {
                                itold->second-=quantity;
                            }                            
                        }
                    }
                }
            }
        }
    }    

    aFormatMetaDataPerOutput.resize(wtx.vout.size());

    for (int j = 0; j < (int)wtx.vout.size(); ++j)
    {
        const CTxOut& txout = wtx.vout[j];
        if(!txout.scriptPubKey.IsUnspendable())
        {
            string strFailReason;
            CTxDestination address;
            int required=0;

            ExtractDestination(txout.scriptPubKey, address);

            amounts->Clear();
            if(CreateAssetBalanceList(txout,amounts,lpScript,&required))
            {
                for(int i=0;i<amounts->GetCount();i++)
                {
                    int64_t quantity=-1;
                    if(memcmp(entity->GetFullRef(),amounts->GetRow(i),MC_AST_ASSET_FULLREF_SIZE) == 0)
                    {
                        quantity=mc_GetABQuantity(amounts->GetRow(i));
                    }
                    else
                    {
                        if(memcmp(entity->GetTxID(),&hash,sizeof(uint256)) == 0)
                        {
                            if( mc_GetABRefType(amounts->GetRow(i)) == MC_AST_ASSET_REF_TYPE_GENESIS )                    
//                            if(memcmp(bufEmptyAssetRef,amounts->GetRow(i),MC_AST_ASSET_QUANTITY_OFFSET) == 0)
                            {
                                quantity=mc_GetABQuantity(amounts->GetRow(i));
                            }                            
                        }
                    }
                            
                    if(quantity >= 0)
                    {
                        map<CTxDestination, int64_t>::iterator itold = mAddresses.find(address);
                        if (itold == mAddresses.end())
                        {
                            mAddresses.insert(make_pair(address, +quantity));
                        }
                        else
                        {
                            itold->second+=quantity;
                        }                            
                    }
                }
            }            
        }
        else
        {            
            const CScript& script2 = wtx.vout[j].scriptPubKey;        
            CScript::const_iterator pc2 = script2.begin();

            lpScript->Clear();
            lpScript->SetScript((unsigned char*)(&pc2[0]),(size_t)(script2.end()-pc2),MC_SCR_TYPE_SCRIPTPUBKEY);
            
//            lpScript->ExtractAndDeleteDataFormat(&format);
            lpScript->ExtractAndDeleteDataFormat(&format,&chunk_hashes,&chunk_count,&total_chunk_size);
            const unsigned char *elem;

            if(lpScript->GetNumElements()<=1)
            {
                if(lpScript->GetNumElements()==1)
                {
//                    elem = lpScript->GetData(lpScript->GetNumElements()-1,&elem_size);
                    retrieve_status = GetFormattedData(lpScript,&elem,&out_size,chunk_hashes,chunk_count,total_chunk_size);
                    Value metadata=OpReturnFormatEntry(elem,out_size,wtx.GetHash(),j,format,NULL,retrieve_status);
                    aFormatMetaData.push_back(metadata);
                    aFormatMetaDataPerOutput[j].push_back(metadata);
                }                        
            }
            else
            {
                if(mc_gState->m_Compatibility & MC_VCM_1_0)
                {
//                    elem = lpScript->GetData(lpScript->GetNumElements()-1,&elem_size);
                    retrieve_status = GetFormattedData(lpScript,&elem,&out_size,chunk_hashes,chunk_count,total_chunk_size);
                    if(out_size)
                    {
//                        aMetaData.push_back(OpReturnEntry(elem,elem_size,wtx.GetHash(),i));
                        aFormatMetaData.push_back(OpReturnFormatEntry(elem,out_size,wtx.GetHash(),j,format,NULL,retrieve_status));
                    }
                }
                
                lpScript->SetElement(0);
                lpScript->GetNewEntityType(&new_entity_type);
            }
        }
    }
    
    if(mAddresses.size() == 0)
    {
        return entry;            
    }
    
    Array vin;
    Array vout;
    
    if(fLong)
    {
        BOOST_FOREACH(const CTxIn& txin, wtx.vin)
        {
            CTxOut TxOutIn;
            vin.push_back(TxOutEntry(TxOutIn,-1,txin,txin.prevout.hash,amounts,lpScript));
        }
    }
    for (int i = 0; i < (int)wtx.vout.size(); ++i)
    {
        CTxIn TxIn;
        Value data_item_entry=DataItemEntry(wtx,i,streams_already_seen,0x01);
        if(!data_item_entry.is_null())
        {
            aItems.push_back(data_item_entry);
        }
        if(fLong)
        {
            Array aTxOutItems;
            if(!data_item_entry.is_null())
            {
                aTxOutItems.push_back(data_item_entry);
            }
            Object txout_entry=TxOutEntry(wtx.vout[i],i,TxIn,wtx.GetHash(),amounts,lpScript);
            if( (aTxOutItems.size() > 0) || (mc_gState->m_Compatibility & MC_VCM_1_0) )
            {
                txout_entry.push_back(Pair("items", aTxOutItems));
            }
            if( (mc_gState->m_Compatibility & MC_VCM_1_0) == 0)
            {
                if(aFormatMetaDataPerOutput[i].size())
                {
                    txout_entry.push_back(Pair("data", aFormatMetaDataPerOutput[i]));                    
                }
            }
            
            vout.push_back(txout_entry);
        }
    }        
    
    int64_t other_amount=0;
    
    Object oBalance;
    BOOST_FOREACH(const PAIRTYPE(CTxDestination, int64_t)& item, mAddresses)
    {
        const CTxDestination dest=item.first;
        const CKeyID *lpKeyID=boost::get<CKeyID> (&dest);
        const CScriptID *lpScript=boost::get<CScriptID> (&dest);
        if( (lpKeyID == NULL) && (lpScript == NULL) )
        {
            other_amount=item.second;
        }
        else            
        {
            oBalance.push_back(Pair(CBitcoinAddress(item.first).ToString(), units*item.second));            
        }
    }
    if(other_amount != 0)
    {
        oBalance.push_back(Pair("", other_amount));                    
    }
    
    entry.push_back(Pair("addresses", oBalance));
    entry.push_back(Pair("items", aItems));
    entry.push_back(Pair("data", aFormatMetaData));
    
    WalletTxToJSON(wtx, entry, true);

    if(fLong)
    {
        entry.push_back(Pair("vin", vin));
        entry.push_back(Pair("vout", vout));
        string strHex = EncodeHexTx(static_cast<CTransaction>(wtx));
        entry.push_back(Pair("hex", strHex));
    }    
    
    return entry;    
}

Value getfilterassetbalances(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)                        
        mc_ThrowHelpMessage("getfilterassetbalances");        
    
    mc_EntityDetails asset_entity;
    mc_EntityDetails* lpAsset=NULL;
    double multiple=1.;
    if(COIN > 0)
    {
        multiple=(double)COIN;
    }
    
    int64_t quantity;
    bool raw_value=false;

    if (params.size() > 1)    
    {
        raw_value=paramtobool(params[1]);
    }
    
    if(params[0].type() != str_type)
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid asset identifier, expected string");                                                        
    }
    
    if(params[0].get_str().size())
    {
        ParseEntityIdentifier(params[0],&asset_entity,MC_ENT_TYPE_ASSET);
        lpAsset=&asset_entity;
        multiple=(double)(lpAsset->GetAssetMultiple());
    }
    
    map <CTxDestination,int64_t> mAddresses;
    
    mc_Buffer *asset_amounts=mc_gState->m_TmpBuffers->m_RpcABBuffer1;
    asset_amounts->Clear();
    
    mc_Script *lpScript=mc_gState->m_TmpBuffers->m_RpcScript3;
    lpScript->Clear();    
    
    for(int j=0;j<(int)pMultiChainFilterEngine->m_Tx.vin.size();j++)
    {
        int n=pMultiChainFilterEngine->m_Tx.vin[j].prevout.n;
        CCoins coins;
        {
            LOCK(mempool.cs);
            CCoinsViewMemPool view(pcoinsTip, mempool);
            if (!view.GetCoins(pMultiChainFilterEngine->m_Tx.vin[j].prevout.hash, coins))
            {
                return Value::null;                                                                 
            }
            if (n<0 || (unsigned int)n>=coins.vout.size() || coins.vout[n].IsNull())
                return Value::null;

            CTxDestination address;
            int required=0;

            ExtractDestination(coins.vout[n].scriptPubKey, address);
    
            quantity=-1;
            
            if(lpAsset)
            {
                asset_amounts->Clear();
                if(CreateAssetBalanceList(coins.vout[n],asset_amounts,lpScript,&required))
                {
                    for(int i=0;i<asset_amounts->GetCount();i++)
                    {
                        if(memcmp(lpAsset->GetFullRef(),asset_amounts->GetRow(i),MC_AST_ASSET_FULLREF_SIZE) == 0)
                        {
                            quantity=mc_GetABQuantity(asset_amounts->GetRow(i));
                        }
                        else
                        {
                            int gin=j;
                            if(mc_gState->m_Features->FixedIn20006() == 0)
                            {
                                gin=i;
                            }
                            if(memcmp(lpAsset->GetTxID(),&(pMultiChainFilterEngine->m_Tx.vin[gin].prevout.hash),sizeof(uint256)) == 0)
                            {
                                if( mc_GetABRefType(asset_amounts->GetRow(i)) == MC_AST_ASSET_REF_TYPE_GENESIS )                    
                                {
                                    quantity=mc_GetABQuantity(asset_amounts->GetRow(i));
                                }                            
                            }
                        }

                    }
                }
            }
            else
            {
                quantity=coins.vout[n].nValue;
            }    
            if(quantity >= 0)
            {                        
                map<CTxDestination, int64_t>::iterator itold = mAddresses.find(address);
                if (itold == mAddresses.end())
                {
                    mAddresses.insert(make_pair(address, -quantity));
                }
                else
                {
                    itold->second-=quantity;
                }                            
            }
        }        
    }
    for (int j = 0; j < (int)pMultiChainFilterEngine->m_Tx.vout.size(); ++j)
    {
        const CTxOut& txout = pMultiChainFilterEngine->m_Tx.vout[j];
        if(!txout.scriptPubKey.IsUnspendable())
        {
            CTxDestination address;
            int required=0;

            ExtractDestination(txout.scriptPubKey, address);
    
            quantity=-1;
            if(lpAsset)
            {
                asset_amounts->Clear();
                if(CreateAssetBalanceList(txout,asset_amounts,lpScript,&required))
                {
                    for(int i=0;i<asset_amounts->GetCount();i++)
                    {
                        if(memcmp(lpAsset->GetFullRef(),asset_amounts->GetRow(i),MC_AST_ASSET_FULLREF_SIZE) == 0)
                        {
                            quantity=mc_GetABQuantity(asset_amounts->GetRow(i));
                        }
                        else
                        {
                            if(memcmp(lpAsset->GetTxID(),&(pMultiChainFilterEngine->m_TxID),sizeof(uint256)) == 0)
                            {
                                if( mc_GetABRefType(asset_amounts->GetRow(i)) == MC_AST_ASSET_REF_TYPE_GENESIS )                    
                                {
                                    quantity=mc_GetABQuantity(asset_amounts->GetRow(i));
                                }                            
                            }
                        }
                    }
                }
            }         
            else
            {
                quantity=txout.nValue;                
            }
            if(quantity >= 0)
            {
                map<CTxDestination, int64_t>::iterator itold = mAddresses.find(address);
                if (itold == mAddresses.end())
                {
                    mAddresses.insert(make_pair(address, +quantity));
                }
                else
                {
                    itold->second+=quantity;
                }                            
            }
        }        
    }
    
    int64_t other_amount=0;
    
    Object oBalance;
    BOOST_FOREACH(const PAIRTYPE(CTxDestination, int64_t)& item, mAddresses)
    {
        const CTxDestination dest=item.first;
        const CKeyID *lpKeyID=boost::get<CKeyID> (&dest);
        const CScriptID *lpScript=boost::get<CScriptID> (&dest);
        if( (lpKeyID == NULL) && (lpScript == NULL) )
        {
            other_amount=item.second;
        }
        else            
        {
            oBalance.push_back(Pair(CBitcoinAddress(item.first).ToString(), raw_value ? item.second : item.second/multiple));            
        }
    }
    if(other_amount != 0)
    {
        oBalance.push_back(Pair("", raw_value ? other_amount : other_amount/multiple));                    
    }

    return oBalance;
}

Value getassettransaction(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 3)
        throw runtime_error("Help message not found\n");
    
    if((mc_gState->m_WalletMode & MC_WMD_TXS) == 0)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "API is not supported with this wallet version. To get this functionality, run \"multichaind -walletdbversion=2 -rescan\" ");        
    }   
           
    mc_EntityDetails asset_entity;
    ParseEntityIdentifier(params[0],&asset_entity,MC_ENT_TYPE_ASSET);
    
    mc_TxEntityStat entStat;
    entStat.Zero();
    memcpy(&entStat,asset_entity.GetShortRef(),mc_gState->m_NetworkParams->m_AssetRefSize);
    entStat.m_Entity.m_EntityType=MC_TET_ASSET;
    entStat.m_Entity.m_EntityType |= MC_TET_CHAINPOS;
    
    if(!pwalletTxsMain->FindEntity(&entStat))
    {
        throw JSONRPCError(RPC_NOT_SUBSCRIBED, "Not subscribed to this asset");                                
    }
    
    uint256 hash = ParseHashV(params[1], "parameter 2");
    
    bool verbose=false;
    
    if (params.size() > 2)    
    {
        verbose=paramtobool(params[2]);
    }
    
    const CWalletTx& wtx=pwalletTxsMain->GetWalletTx(hash,NULL,NULL);
    mc_Buffer *asset_amounts=mc_gState->m_TmpBuffers->m_RpcABBuffer1;
    asset_amounts->Clear();
    
    mc_Script *lpScript=mc_gState->m_TmpBuffers->m_RpcScript3;
    lpScript->Clear();    
    
    Object entry=ListAssetTransactions(wtx, &asset_entity, verbose, asset_amounts, lpScript);
    
    if(entry.size() == 0)
    {
        throw JSONRPCError(RPC_TX_NOT_FOUND, "This transaction was not found for this asset");                
    }
    
    
    return entry;
}


Value listassettransactions(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 5)
        throw runtime_error("Help message not found\n");

    if((mc_gState->m_WalletMode & MC_WMD_TXS) == 0)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "API is not supported with this wallet version. To get this functionality, run \"multichaind -walletdbversion=2 -rescan\" ");        
    }   

    Array retArray;

    mc_TxEntityStat entStat;
    mc_TxEntityRow *lpEntTx;
    
    mc_EntityDetails asset_entity;
    ParseEntityIdentifier(params[0],&asset_entity,MC_ENT_TYPE_ASSET);

    int count,start,shift;
    bool verbose=false;
    bool fGenesis;
    
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
    memcpy(&entStat,asset_entity.GetShortRef(),mc_gState->m_NetworkParams->m_AssetRefSize);
    entStat.m_Entity.m_EntityType=MC_TET_ASSET;
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
        throw JSONRPCError(RPC_NOT_SUBSCRIBED, "Not subscribed to this asset");                                
    }
    
    mc_Buffer *entity_rows=mc_gState->m_TmpBuffers->m_RpcEntityRows;
    entity_rows->Clear();

    mc_Buffer *asset_amounts=mc_gState->m_TmpBuffers->m_RpcABBuffer1;
    asset_amounts->Clear();
    
    mc_Script *lpScript=mc_gState->m_TmpBuffers->m_RpcScript3;
    lpScript->Clear();    

    CheckWalletError(pwalletTxsMain->GetList(&entStat.m_Entity,1,1,entity_rows),entStat.m_Entity.m_EntityType,"");
    shift=1;
    if(entity_rows->GetCount())
    {
        lpEntTx=(mc_TxEntityRow*)entity_rows->GetRow(0);
        if(memcmp(lpEntTx->m_TxId,asset_entity.GetTxID(),MC_TDB_TXID_SIZE) == 0)
        {
            shift=0;
        }
    }
    
    mc_AdjustStartAndCount(&count,&start,entStat.m_LastPos+shift);
    
    fGenesis=false;
    if(shift)
    {
        if(start)
        {
            start-=shift;
            shift=0;
        }
        else
        {
            count-=shift;
            fGenesis=true;
        }
    }
    
    CheckWalletError(pwalletTxsMain->GetList(&entStat.m_Entity,start+1,count,entity_rows),entStat.m_Entity.m_EntityType,"");
    
    
    for(int i=0;i<entity_rows->GetCount()+shift;i++)
    {
        uint256 hash;
        if(fGenesis && (i == 0))
        {
            memcpy(&hash,asset_entity.GetTxID(),MC_TDB_TXID_SIZE);            
        }
        else
        {
            lpEntTx=(mc_TxEntityRow*)entity_rows->GetRow(i-shift);
            memcpy(&hash,lpEntTx->m_TxId,MC_TDB_TXID_SIZE);            
        }
        const CWalletTx& wtx=pwalletTxsMain->GetWalletTx(hash,NULL,NULL);
        Object entry=ListAssetTransactions(wtx, &asset_entity, verbose, asset_amounts, lpScript);

                
        if(entry.size())
        {
            retArray.push_back(entry);                                
        }
    }
    
    return retArray;
}
