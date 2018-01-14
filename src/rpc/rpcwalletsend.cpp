// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2014-2016 The Bitcoin Core developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2017 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.


#include "rpc/rpcwallet.h"
bool CreateAssetGroupingTransaction(CWallet *lpWallet, const vector<pair<CScript, CAmount> >& vecSend,
                                CWalletTx& wtxNew, CReserveKey& reservekey, CAmount& nFeeRet, std::string& strFailReason, const CCoinControl* coinControl,
                                const set<CTxDestination>* addresses,int min_conf,int min_inputs,int max_inputs,const vector<COutPoint>* lpCoinsToUse,uint32_t flags, int *eErrorCode);




Value createrawsendfrom(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 4)                                            // MCHN
        throw runtime_error("Help message not found\n");

    vector<CTxDestination> fromaddresses;        
    set<CTxDestination> thisFromAddresses;
    
    fromaddresses=ParseAddresses(params[0].get_str(),false,true);

    if(fromaddresses.size() != 1)
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Single from-address should be specified");                        
    }

    if( IsMine(*pwalletMain, fromaddresses[0]) == ISMINE_NO )
    {
        throw JSONRPCError(RPC_WALLET_ADDRESS_NOT_FOUND, "from-address is not found in this wallet");                        
    }
    

    BOOST_FOREACH(const CTxDestination& fromaddress, fromaddresses)
    {
        thisFromAddresses.insert(fromaddress);
    }    
    
    Object sendTo = params[1].get_obj();

    CWalletTx rawTx;
    mc_EntityDetails found_entity;
   
    vector <pair<CScript, CAmount> > vecSend;
    vecSend=ParseRawOutputMultiObject(sendTo,NULL);

    mc_EntityDetails entity;
    mc_gState->m_TmpAssetsOut->Clear();
    for(int i=0;i<(int)vecSend.size();i++)
    {
        FindFollowOnsInScript(vecSend[i].first,mc_gState->m_TmpAssetsOut,mc_gState->m_TmpScript);
    }
    entity.Zero();
    if(mc_gState->m_TmpAssetsOut->GetCount())
    {
/*        
        if(mc_gState->m_TmpAssetsOut->GetCount() > 1)
        {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Follow-on script rejected - follow-on for several assets");                                    
        }        
 */ 
        if(!mc_gState->m_Assets->FindEntityByFullRef(&entity,mc_gState->m_TmpAssetsOut->GetRow(0)))
        {
            throw JSONRPCError(RPC_ENTITY_NOT_FOUND, "Follow-on script rejected - asset not found");                                                
        }
    }
    

    if (params.size() > 2 && params[2].type() != null_type) 
    {
        BOOST_FOREACH(const Value& data, params[2].get_array()) 
        {
            CScript scriptOpReturn=ParseRawMetadata(data,MC_DATA_API_PARAM_TYPE_ALL-MC_DATA_API_PARAM_TYPE_CIS,&entity,&found_entity);
            if(found_entity.GetEntityType() == MC_ENT_TYPE_STREAM)
            {
                FindAddressesWithPublishPermission(fromaddresses,&found_entity);
            }
            vecSend.push_back(make_pair(scriptOpReturn, 0));
        }
    }

    string action="";
    string hex;
    Value signedTx;    
    Value txid;
    bool sign_it=false;
    bool lock_it=false;
    bool send_it=false;
    if (params.size() > 3 && params[3].type() != null_type) 
    {
        ParseRawAction(params[3].get_str(),lock_it,sign_it,send_it);
    }
    
    
    CReserveKey reservekey(pwalletMain);
    CAmount nFeeRequired;
    string strError;
    uint32_t flags=MC_CSF_ALLOW_NOT_SPENDABLE_P2SH | MC_CSF_ALLOW_SPENDABLE_P2SH | MC_CSF_ALLOW_NOT_SPENDABLE;
    
    if(!sign_it)
    {
        flags |= MC_CSF_ALLOWED_COINS_ARE_MINE;
    }
    
    if(vecSend.size() == 0)
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Either addresses object or data array should not be empty");                                                        
    }
    
    EnsureWalletIsUnlocked();
    {
        LOCK (pwalletMain->cs_wallet_send);
        int eErrorCode;
        if(!CreateAssetGroupingTransaction(pwalletMain, vecSend, rawTx, reservekey, nFeeRequired, strError, NULL, &thisFromAddresses, 1, -1, -1, NULL, flags, &eErrorCode))
        {
            LogPrintf("createrawsendfrom : %s\n", strError);
            throw JSONRPCError(eErrorCode, strError);
        }
    }
    
    hex=EncodeHexTx(rawTx);


    if(sign_it)
    {
        Array signrawtransaction_params;
        signrawtransaction_params.push_back(hex);
        signedTx=signrawtransaction(signrawtransaction_params,false);
    }
    if(lock_it)
    {
        BOOST_FOREACH(const CTxIn& txin, rawTx.vin)
        {
            COutPoint outpt(txin.prevout.hash, txin.prevout.n);
            pwalletMain->LockCoin(outpt);
        }
    }    
    if(send_it)
    {
        Array sendrawtransaction_params;
        BOOST_FOREACH(const Pair& s, signedTx.get_obj()) 
        {        
            if(s.name_=="complete")
            {
                if(!s.value_.get_bool())
                {
                    throw JSONRPCError(RPC_TRANSACTION_ERROR, "Transaction was not signed properly");                    
                }
            }
            if(s.name_=="hex")
            {
                sendrawtransaction_params.push_back(s.value_.get_str());                
            }
        }
        txid=sendrawtransaction(sendrawtransaction_params,false);
    }
    
    if(send_it)
    {
        return txid;
    }
    
    if(sign_it)
    {
        return signedTx;
    }
    
    return hex;
}



Value sendfromaddress(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 3 || params.size() > 5)                        // MCHN 
        throw runtime_error("Help message not found\n");

    CBitcoinAddress address(params[1].get_str());                               
    if (!address.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");

    // Wallet comments
    CWalletTx wtx;
    if (params.size() > 3 && params[3].type() != null_type && !params[3].get_str().empty())
        wtx.mapValue["comment"] = params[3].get_str();
    if (params.size() > 4 && params[4].type() != null_type && !params[4].get_str().empty())
        wtx.mapValue["to"]      = params[4].get_str();

    
    if(!AddressCanReceive(address.Get()))
    {
        throw JSONRPCError(RPC_INSUFFICIENT_PERMISSIONS, "Destination address doesn't have receive permission");        
    }
 
    
    CAmount nAmount=0;
    
    vector<CTxDestination> addresses;    
    addresses.push_back(address.Get());
        
    mc_Script *lpScript=mc_gState->m_TmpBuffers->m_RpcScript3;
    lpScript->Clear();
    
    if (params[2].type() == obj_type)
    {
        lpScript->Clear();
        uint256 offer_hash;

        if (params[2].type() != obj_type)
        {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid asset quantities object");        
        }
        else
        {
            int eErrorCode;
            string strError=ParseRawOutputObject(params[2],nAmount,lpScript,&eErrorCode);
            if(strError.size())
            {
                throw JSONRPCError(eErrorCode, strError);                            
            }
        }
    }
    else
    {
      nAmount = AmountFromValue(params[2]);     
    }
    

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
        if(!pwalletMain->GetKeyFromAddressBook(pkey,MC_PTP_SEND,&thisFromAddresses))
        {
            throw JSONRPCError(RPC_INSUFFICIENT_PERMISSIONS, "from-address doesn't have send permission");                
        }  
    }
    else
    {
        CPubKey pkey;
        if(!pwalletMain->GetKeyFromAddressBook(pkey,MC_PTP_SEND))
        {
            throw JSONRPCError(RPC_INSUFFICIENT_PERMISSIONS, "This wallet doesn't have keys with send permission");                
        }        
    }

    
    EnsureWalletIsUnlocked();
    LOCK (pwalletMain->cs_wallet_send);
    
    SendMoneyToSeveralAddresses(addresses, nAmount, wtx, lpScript, CScript(), fromaddresses);
    
    return wtx.GetHash().GetHex();
}


Value sendwithmetadatafrom(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 4)                        // MCHN 
        throw runtime_error("Help message not found\n");

    CBitcoinAddress address(params[1].get_str());                               
    if (!address.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");

    // Wallet comments
    CWalletTx wtx;
    
    if(!AddressCanReceive(address.Get()))
    {
        throw JSONRPCError(RPC_INSUFFICIENT_PERMISSIONS, "Destination address doesn't have receive permission");        
    }
 
    
    CAmount nAmount=0;
    
    vector<CTxDestination> addresses;    
    addresses.push_back(address.Get());
        
    mc_Script *lpScript=mc_gState->m_TmpBuffers->m_RpcScript3;    
    lpScript->Clear();
    
    if (params[2].type() == obj_type)
    {
        lpScript->Clear();
        uint256 offer_hash;

        if (params[2].type() != obj_type)
        {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid asset quantities object");        
        }
        else
        {
            int eErrorCode;
            string strError=ParseRawOutputObject(params[2],nAmount,lpScript,&eErrorCode);
            if(strError.size())
            {
                throw JSONRPCError(eErrorCode, strError);                            
            }
        }
    }
    else
    {
      nAmount = AmountFromValue(params[2]);     
    }
    
    mc_EntityDetails found_entity;
    CScript scriptOpReturn=ParseRawMetadata(params[3],MC_DATA_API_PARAM_TYPE_SIMPLE,NULL,&found_entity);
    
    vector<CTxDestination> fromaddresses;        
    set<CTxDestination> thisFromAddresses;

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
        

        BOOST_FOREACH(const CTxDestination& fromaddress, fromaddresses)
        {
            thisFromAddresses.insert(fromaddress);
        }

        CPubKey pkey;
        if(!pwalletMain->GetKeyFromAddressBook(pkey,MC_PTP_SEND,&thisFromAddresses))
        {
            throw JSONRPCError(RPC_INSUFFICIENT_PERMISSIONS, "from-address doesn't have send permission");                
        }   
        if(found_entity.GetEntityType() == MC_ENT_TYPE_STREAM)
        {
            FindAddressesWithPublishPermission(fromaddresses,&found_entity);
        }
    }
    else
    {
        CPubKey pkey;
        if(found_entity.GetEntityType() == MC_ENT_TYPE_STREAM)
        {
            FindAddressesWithPublishPermission(fromaddresses,&found_entity);
            BOOST_FOREACH(const CTxDestination& fromaddress, fromaddresses)
            {
                thisFromAddresses.insert(fromaddress);
            }
            if(!pwalletMain->GetKeyFromAddressBook(pkey,MC_PTP_SEND,&thisFromAddresses))
            {
                throw JSONRPCError(RPC_INSUFFICIENT_PERMISSIONS, "This wallet doesn't have keys with write permission for given stream and/or global send permission");                
            }        
        }
        else
        {
            if(!pwalletMain->GetKeyFromAddressBook(pkey,MC_PTP_SEND))
            {
                throw JSONRPCError(RPC_INSUFFICIENT_PERMISSIONS, "This wallet doesn't have keys with send permission");                
            }        
        }
    }
    


    EnsureWalletIsUnlocked();
    LOCK (pwalletMain->cs_wallet_send);
    
    SendMoneyToSeveralAddresses(addresses, nAmount, wtx, lpScript, scriptOpReturn, fromaddresses);
    
    return wtx.GetHash().GetHex();
}

Value sendwithmetadata(const Array& params, bool fHelp)
{
    if (fHelp || params.size() !=3 )
        throw runtime_error("Help message not found\n");

    Array ext_params;
    ext_params.push_back("*");
    BOOST_FOREACH(const Value& value, params)
    {
        ext_params.push_back(value);
    }
    
    return sendwithmetadatafrom(ext_params,fHelp);    
}

Value sendtoaddress(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 4)                        // MCHN 
        throw runtime_error("Help message not found\n");

    Array ext_params;
    ext_params.push_back("*");
    BOOST_FOREACH(const Value& value, params)
    {
        ext_params.push_back(value);
    }
    
    return sendfromaddress(ext_params,fHelp);       
}



Value combineunspent(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 6)
        throw runtime_error("Help message not found\n");
    
    int nMinConf = 1;
    if (params.size() > 1)
        nMinConf = params[1].get_int();
    
    int nMinInputs = 2;
    if (params.size() > 3)
        nMinInputs = params[3].get_int();
    
    if((nMinInputs < 2) || (nMinInputs > 1000))
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid mininputs. Valid Range [2 - 1000].");        
    }
    
    int nMaxInputs = 100;
    if (params.size() > 4)
        nMaxInputs = params[4].get_int();
    
    if((nMaxInputs < 2) || (nMaxInputs > 1000))
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid maxinputs. Valid Range [2 - 1000].");        
    }
    
    if(nMaxInputs < nMinInputs)
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "maxinputs below mininouts.");                
    }
    
    int nMaxTransactions = 100;
    if (params.size() > 2)
        nMaxTransactions = params[2].get_int();

    if((nMaxTransactions < 1) || (nMaxTransactions > 100))
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid maximum-transactions. Valid Range [1 - 100].");        
    }
    
    int nMaxTime = 30;
    if (params.size() > 5)
        nMaxTime = params[5].get_int();

    vector<CTxDestination> addresses;
    set<CTxDestination> thisAddresses;
    set<CBitcoinAddress> setAddress;
    set<CTxDestination>* lpAddresses; 
    
    string tok;

    lpAddresses=NULL;
    
    if( (params.size() == 0) || (params[0].get_str() == "*") )
    {
        BOOST_FOREACH(const PAIRTYPE(CBitcoinAddress, CAddressBookData)& item, pwalletMain->mapAddressBook)
        {
            const CBitcoinAddress& address = item.first;
            CTxDestination dest=address.Get();            
            CKeyID *lpKeyID=boost::get<CKeyID> (&dest);
            
            if(lpKeyID)
            {
                addresses.push_back(address.Get());                
            }
        }
    }
    else
    {
        stringstream ss(params[0].get_str()); 
        while(getline(ss, tok, ',')) 
        {
            CBitcoinAddress address(tok);
            if (!address.IsValid())
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address: "+tok);            
            if (setAddress.count(address))
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, duplicated address: "+tok);
            CTxDestination dest=address.Get();            
            CKeyID *lpKeyID=boost::get<CKeyID> (&dest);
            if(lpKeyID)
            {        
                addresses.push_back(address.Get());
                setAddress.insert(address);
            }
            else
            {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address (only pubkeyhash addresses are supported) : "+tok);                
            }
        }
    }

    EnsureWalletIsUnlocked();
    
    string strError;
    if (pwalletMain->IsLocked())
    {
        strError = "Error: Wallet locked, unable to create transaction!";
        LogPrintf("CombineUnspent() : %s", strError);
        throw JSONRPCError(RPC_WALLET_UNLOCK_NEEDED, strError);
    }

    
    // Create and send the transaction
    CReserveKey reservekey(pwalletMain);
    
    double start_time=mc_TimeNowAsDouble();
    int txcount=0;
    int pos=0;
    std::vector <CScript> scriptPubKeys;
    CScript scriptOpReturn=CScript();
    Array results;
    
    
    while((pos<(int)addresses.size()) && (txcount < nMaxTransactions) && ((mc_TimeNowAsDouble()-start_time < (double)nMaxTime) || (txcount == 0)))
    {    
        thisAddresses.clear();
        thisAddresses.insert(addresses[pos]);
        lpAddresses=&thisAddresses;
        
        CKeyID *lpKeyID=boost::get<CKeyID> (&addresses[pos]);        
        CBitcoinAddress bitcoin_address=CBitcoinAddress(*lpKeyID);
        
        
        CWalletTx wtx;
        if(!pwalletMain->CreateAndCommitOptimizeTransaction(wtx,strError,lpAddresses,nMinConf,nMinInputs,nMaxInputs))
        {
            pos++;
        }
        else
        {
            txcount++;
            results.push_back(wtx.GetHash().GetHex());            
        }
        
    }    
    
    if(results.size() == 0)
    {
        strError="Not enough inputs";
        LogPrintf("CombineUnspent() : %s\n", strError);
//        throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }
    
    return  results;
}


Value preparelockunspentfrom(const json_spirit::Array& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 3)
        throw runtime_error("Help message not found\n");
    
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

    set<CTxDestination> thisFromAddresses;
    
    BOOST_FOREACH(const CTxDestination& fromaddress, fromaddresses)
    {
        thisFromAddresses.insert(fromaddress);
    }
    
    CPubKey pkey;
    if(!pwalletMain->GetKeyFromAddressBook(pkey,MC_PTP_SEND | MC_PTP_RECEIVE,&thisFromAddresses))
    {
        throw JSONRPCError(RPC_INSUFFICIENT_PERMISSIONS, "from-address must have send and receive permission");                
    }   
    vector<CTxDestination> addresses;    
    addresses.push_back(CTxDestination(pkey.GetID()));
    
    mc_Script *lpScript=mc_gState->m_TmpBuffers->m_RpcScript3;
    lpScript->Clear();
    
    CAmount nAmount=0;
    uint256 offer_hash;
    bool lock_it=true;
    
    
    if (params[1].type() != obj_type)
    {
        nAmount = AmountFromValue(params[1]);             
    }
    else
    {
        int eErrorCode;
        string strError=ParseRawOutputObject(params[1],nAmount,lpScript,&eErrorCode);
        if(strError.size())
        {
            throw JSONRPCError(eErrorCode, strError);                            
        }
    }
    
    if ((params.size() > 2) && (params[2].type() == bool_type))
    {
        if(!params[2].get_bool())
        {
            lock_it=false;
        }        
    }

    CWalletTx wtx;
    

    
    EnsureWalletIsUnlocked();
    LOCK (pwalletMain->cs_wallet_send);
    
    SendMoneyToSeveralAddresses(addresses, nAmount, wtx, lpScript, CScript(), fromaddresses);
    
    int vout=-1;
    
    CScript scriptPubKey = GetScriptForDestination(addresses[0]);
    
    for(int element=0;element < lpScript->GetNumElements();element++)
    {
        size_t elem_size;    
        const unsigned char *elem;
        elem = lpScript->GetData(element,&elem_size);
        if(elem)
        {
            scriptPubKey << vector<unsigned char>(elem, elem + elem_size) << OP_DROP;
        }
        else
            throw JSONRPCError(RPC_INTERNAL_ERROR, "Invalid script");
    }
    
    CScript::const_iterator pc0 = scriptPubKey.begin();
    
    for (unsigned int j = 0; j < wtx.vout.size(); j++)
    {
        CTxOut txout=wtx.vout[j];
        
        const CScript& script1 = txout.scriptPubKey;        
        CScript::const_iterator pc1 = script1.begin();
        
        int script_size=(int)(script1.end()-script1.begin());

        if(nAmount == txout.nValue)            
        {
            if(script_size == (int)(scriptPubKey.end()-scriptPubKey.begin()))
            {
                if(memcmp((&pc1[0]),(&pc0[0]),script_size) == 0)
                {
                    LOCK(pwalletMain->cs_wallet);
                    if(fDebug)LogPrint("mchn","mchn: New lockunspent (%s,%d), Offer hash: %s\n",wtx.GetHash().GetHex().c_str(),j,offer_hash.GetHex().c_str());
                    vout=j;                    
                }
            }
        }        
    }
    
    
    if(lock_it)
    {
        COutPoint outpt(wtx.GetHash(),vout);

        pwalletMain->LockCoin(outpt);
    }
    
    Object result;
    
    result.push_back(Pair("txid", wtx.GetHash().GetHex()));
    result.push_back(Pair("vout", vout));
    
    return result;
}



Value preparelockunspent(const json_spirit::Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error("Help message not found\n");
    
    CPubKey pkey;
    if(!pwalletMain->GetKeyFromAddressBook(pkey,MC_PTP_SEND | MC_PTP_RECEIVE))
    {
        throw JSONRPCError(RPC_INSUFFICIENT_PERMISSIONS, "This wallet doesn't have keys with send and receive permission");                
    }

    vector<CTxDestination> addresses;    
    addresses.push_back(CTxDestination(pkey.GetID()));
    
    mc_Script *lpScript=mc_gState->m_TmpBuffers->m_RpcScript3;
    lpScript->Clear();
    
    CAmount nAmount=0;
    uint256 offer_hash;
    bool lock_it=true;
    
    
    if (params[0].type() != obj_type)
    {
        nAmount = AmountFromValue(params[0]);             
//        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid asset quantities object");        
    }
    else
    {
        int eErrorCode;
        string strError=ParseRawOutputObject(params[0],nAmount,lpScript,&eErrorCode);
        if(strError.size())
        {
            throw JSONRPCError(eErrorCode, strError);                            
        }
    }
    
    if ((params.size() > 1) && (params[1].type() == bool_type))
    {
        if(!params[1].get_bool())
        {
            lock_it=false;
        }        
    }

    CWalletTx wtx;
    

    vector<CTxDestination> fromaddresses;    
    
    EnsureWalletIsUnlocked();
    LOCK (pwalletMain->cs_wallet_send);
       
    SendMoneyToSeveralAddresses(addresses, nAmount, wtx, lpScript, CScript(), fromaddresses);
    
    int vout=-1;
    
    CScript scriptPubKey = GetScriptForDestination(addresses[0]);
    
    for(int element=0;element < lpScript->GetNumElements();element++)
    {
        size_t elem_size;    
        const unsigned char *elem;
        elem = lpScript->GetData(element,&elem_size);
        if(elem)
        {
            scriptPubKey << vector<unsigned char>(elem, elem + elem_size) << OP_DROP;
        }
        else
            throw JSONRPCError(RPC_INTERNAL_ERROR, "Invalid script");
    }
    
    CScript::const_iterator pc0 = scriptPubKey.begin();
    
    for (unsigned int j = 0; j < wtx.vout.size(); j++)
    {
        CTxOut txout=wtx.vout[j];
        
        const CScript& script1 = txout.scriptPubKey;        
        CScript::const_iterator pc1 = script1.begin();
        
        int script_size=(int)(script1.end()-script1.begin());

        if(nAmount == txout.nValue)            
        {
            if(script_size == (int)(scriptPubKey.end()-scriptPubKey.begin()))
            {
                if(memcmp((&pc1[0]),(&pc0[0]),script_size) == 0)
                {
                    LOCK(pwalletMain->cs_wallet);
                    if(fDebug)LogPrint("mchn","mchn: New lockunspent (%s,%d), Offer hash: %s\n",wtx.GetHash().GetHex().c_str(),j,offer_hash.GetHex().c_str());
                    vout=j;                    
//                    pwalletMain->mapExchanges.insert(make_pair(COutPoint(wtx.GetHash(),j),CExchangeStatus(offer_hash,0,mc_TimeNowAsUInt())));                    
                }
            }
        }        
    }
    
    if(lock_it)
    {
        COutPoint outpt(wtx.GetHash(),vout);

        pwalletMain->LockCoin(outpt);
    }
    
    Object result;
    
    result.push_back(Pair("txid", wtx.GetHash().GetHex()));
    result.push_back(Pair("vout", vout));
    
    return result;
}

Value sendassetfrom(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 4 || params.size() > 7)
        throw runtime_error("Help message not found\n");

    CBitcoinAddress address(params[1].get_str());
    if (!address.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");

    // Amount    
    CAmount nAmount = MCP_MINIMUM_PER_OUTPUT;
    if (params.size() > 4 && params[4].type() != null_type)
    {
        nAmount = AmountFromValue(params[4]);
    }

    // Wallet comments
    CWalletTx wtx;
    if (params.size() > 5 && params[5].type() != null_type && !params[5].get_str().empty())
        wtx.mapValue["comment"] = params[5].get_str();
    if (params.size() > 6 && params[6].type() != null_type && !params[6].get_str().empty())
        wtx.mapValue["to"]      = params[6].get_str();

    if(!AddressCanReceive(address.Get()))
    {
        throw JSONRPCError(RPC_INSUFFICIENT_PERMISSIONS, "Destination address doesn't have receive permission");        
    }
    
    mc_Script *lpScript=mc_gState->m_TmpBuffers->m_RpcScript3;
    lpScript->Clear();
    
    unsigned char buf[MC_AST_ASSET_FULLREF_BUF_SIZE];
    memset(buf,0,MC_AST_ASSET_FULLREF_BUF_SIZE);
    int multiple=1;
    
    if (params.size() > 2 && params[2].type() != null_type && !params[2].get_str().empty())
    {        
        mc_EntityDetails entity;
        ParseEntityIdentifier(params[2],&entity, MC_ENT_TYPE_ASSET);           
        memcpy(buf,entity.GetFullRef(),MC_AST_ASSET_FULLREF_SIZE);
        if(mc_gState->m_Features->ShortTxIDInTx() == 0)
        {
            if(entity.IsUnconfirmedGenesis())
            {
                throw JSONRPCError(RPC_UNCONFIRMED_ENTITY, "Unconfirmed asset: "+params[2].get_str());            
            }
        }
        multiple=entity.GetAssetMultiple();
    }
    else
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid asset reference");        
    }
    Value raw_qty=params[3];
    
    int64_t quantity = (int64_t)(raw_qty.get_real() * multiple + 0.499999);
    if(quantity<0)
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid asset quantity");        
    }
    
    
    mc_SetABQuantity(buf,quantity);
    
    mc_Buffer *lpBuffer=mc_gState->m_TmpBuffers->m_RpcABNoMapBuffer1;
    lpBuffer->Clear();
    
    lpBuffer->Add(buf);
    
    lpScript->SetAssetQuantities(lpBuffer,MC_SCR_ASSET_SCRIPT_TYPE_TRANSFER);
    
    vector<CTxDestination> addresses;    
    addresses.push_back(address.Get());
    
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
        if(!pwalletMain->GetKeyFromAddressBook(pkey,MC_PTP_SEND,&thisFromAddresses))
        {
            throw JSONRPCError(RPC_INSUFFICIENT_PERMISSIONS, "from-address doesn't have send permission");                
        }   
    }
    else
    {
        CPubKey pkey;
        if(!pwalletMain->GetKeyFromAddressBook(pkey,MC_PTP_SEND))
        {
            throw JSONRPCError(RPC_INSUFFICIENT_PERMISSIONS, "This wallet doesn't have keys with send permission");                
        }        
    }

    EnsureWalletIsUnlocked();
    LOCK (pwalletMain->cs_wallet_send);
    
    SendMoneyToSeveralAddresses(addresses, nAmount, wtx, lpScript, CScript(),fromaddresses);

    return wtx.GetHash().GetHex();
}


Value sendassettoaddress(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 3 || params.size() > 6)
        throw runtime_error("Help message not found\n");

    Array ext_params;
    ext_params.push_back("*");
    BOOST_FOREACH(const Value& value, params)
    {
        ext_params.push_back(value);
    }
    
    return sendassetfrom(ext_params,fHelp);
    
    CBitcoinAddress address(params[0].get_str());
    if (!address.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");

    // Amount    
    CAmount nAmount = MCP_MINIMUM_PER_OUTPUT;
    if (params.size() > 3 && params[3].type() != null_type)
    {
        nAmount = AmountFromValue(params[3]);
    }

    // Wallet comments
    CWalletTx wtx;
    if (params.size() > 4 && params[4].type() != null_type && !params[4].get_str().empty())
        wtx.mapValue["comment"] = params[4].get_str();
    if (params.size() > 5 && params[5].type() != null_type && !params[5].get_str().empty())
        wtx.mapValue["to"]      = params[5].get_str();

    if(!AddressCanReceive(address.Get()))
    {
        throw JSONRPCError(RPC_INSUFFICIENT_PERMISSIONS, "Destination address doesn't have receive permission");        
    }
    
    mc_Script *lpScript=mc_gState->m_TmpBuffers->m_RpcScript3;
    lpScript->Clear();
    
    unsigned char buf[MC_AST_ASSET_FULLREF_BUF_SIZE];
    memset(buf,0,MC_AST_ASSET_FULLREF_BUF_SIZE);
    int multiple=1;
    
    if (params.size() > 1 && params[1].type() != null_type && !params[1].get_str().empty())
    {        
        mc_EntityDetails entity;
        ParseEntityIdentifier(params[1],&entity, MC_ENT_TYPE_ASSET);           
        memcpy(buf,entity.GetFullRef(),MC_AST_ASSET_FULLREF_SIZE);
        if(mc_gState->m_Features->ShortTxIDInTx() == 0)
        {
            if(entity.IsUnconfirmedGenesis())
            {
                throw JSONRPCError(RPC_UNCONFIRMED_ENTITY, "Unconfirmed asset: "+params[1].get_str());            
            }
        }
        multiple=entity.GetAssetMultiple();
    }
    else
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid asset reference");        
    }
    Value raw_qty=params[2];
    
    int64_t quantity = (int64_t)(raw_qty.get_real() * multiple + 0.499999);
    if(quantity<=0)
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid asset quantity");        
    }
    
    
    mc_SetABQuantity(buf,quantity);
    
    mc_Buffer *lpBuffer=mc_gState->m_TmpBuffers->m_RpcABNoMapBuffer1;
    lpBuffer->Clear();
    
    mc_InitABufferDefault(lpBuffer);
    
    lpBuffer->Add(buf);
    
    lpScript->SetAssetQuantities(lpBuffer,MC_SCR_ASSET_SCRIPT_TYPE_TRANSFER);
    
    vector<CTxDestination> addresses;    
    addresses.push_back(address.Get());
    
    vector<CTxDestination> fromaddresses;    
    
    EnsureWalletIsUnlocked();
    LOCK (pwalletMain->cs_wallet_send);
    
    SendMoneyToSeveralAddresses(addresses, nAmount, wtx, lpScript, CScript(),fromaddresses);
    
    return wtx.GetHash().GetHex();
}
