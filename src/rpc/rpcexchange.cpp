// Copyright (c) 2014-2016 The Bitcoin Core developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.


#include "rpc/rpcwallet.h"
#include "script/sign.h"

bool AcceptExchange(const CTransaction& tx, vector <CTxOut>& inputs, vector <string>& errors,string& reason)
{
    if(tx.vout.size() < tx.vin.size())
    {
        reason="Too few outputs";
        return false;        
    }

    return GetTxInputsAsTxOuts(tx, inputs, errors, reason);
}

Object ExchangeAssetEntry(uint256 hash,const CTxOut txout,mc_Script *lpScript,mc_Buffer *asset_amounts,mc_Buffer *total_amounts,bool& can_disable,bool allow_sighashall,string& strError)
{
    Object result;
    CBitcoinAddress address;
    unsigned char buf[MC_AST_ASSET_FULLREF_BUF_SIZE];
    memset(buf,0,MC_AST_ASSET_FULLREF_BUF_SIZE);
    bool is_input=false;
    bool address_found=false;
    
    if(lpScript->m_Size)
    {
        is_input=true;
    }

    if(is_input)
    {
        if(lpScript->GetNumElements() == 2)
        {
            size_t elem_size;
            const unsigned char *elem;
            unsigned char hash_type;

            elem = lpScript->GetData(0,&elem_size);
            
            if(elem_size>1)
            {
                hash_type=elem[elem_size-1];

                if(hash_type != (SIGHASH_SINGLE | SIGHASH_ANYONECANPAY))
                {
                    if(allow_sighashall)
                    {
                        if(hash_type != SIGHASH_ALL)
                        {                        
                            strError="Signature hash type should be SINGLE | SIGHASH_ANYONECANPAY or SIGHASH_ALL";                                            
                        }
                    }
                    else
                    {
                        strError="Signature hash type should be SINGLE | SIGHASH_ANYONECANPAY";                    
                    }
                }          

                elem = lpScript->GetData(1,&elem_size);
                uint160 addr=Hash160(elem,elem+elem_size);
                const unsigned char *pubkey_hash=(unsigned char *)&addr;
                address=CBitcoinAddress(*(CKeyID*)pubkey_hash);
                address_found=true;

                if( (pwalletMain->IsMine(txout) & ISMINE_SPENDABLE) != ISMINE_NO )
                {
                    can_disable=true;
                }

                if(mc_gState->m_Permissions->CanSend(NULL,pubkey_hash) == 0)
                {
                    if(strError.size() == 0)
                    {
                        strError="Address doesn't have send permission";                                                        
                    }
                }
            }
            else                                                                // Multisig with one signature
            {
                if(strError.size() == 0)
                {
                    strError="Only pay-to-keyhash addresses are supported in exchange";                                    
                }                
            }
        }
        else
        {
            if(strError.size() == 0)
            {
                strError="Only pay-to-keyhash addresses are supported in exchange";                                    
            }
        }
    }
    else
    {
        CTxDestination addressRet;        
        
        if(!ExtractDestination(txout.scriptPubKey, addressRet))
        {
            if(strError.size() == 0)
            {
                strError="Only pay-to-keyhash addresses are supported in exchange";                                    
            }
        }
        else
        {
            CKeyID *lpKeyID=boost::get<CKeyID> (&addressRet);
            if(lpKeyID != NULL)
            {
                address=CBitcoinAddress(*lpKeyID);
                address_found=true;
                if(mc_gState->m_Permissions->CanReceive(NULL,(unsigned char*)(lpKeyID)) == 0)
                {
                    if(strError.size() == 0)
                    {
                        strError="Address doesn't have receive permission";                                                        
                    }
                }
            }
            else
            {
                if(strError.size() == 0)
                {
                    strError="Only pay-to-keyhash addresses are supported in exchange";                                    
                }                
            }
        }
    }
    
    if(strError.size())
    {
        return result;
    }
    
    Array assets;
    
    asset_amounts->Clear();
    if(!CreateAssetBalanceList(txout,asset_amounts,lpScript))
    {
        if(strError.size() == 0)
        {
            strError="Wrong asset transfer script";
        }
    }        
    else
    {
        for(int a=0;a<asset_amounts->GetCount();a++)
        {
            Object asset_entry;
            unsigned char *ptr=(unsigned char *)asset_amounts->GetRow(a);

            mc_EntityDetails entity;
            const unsigned char *txid;
            
            int64_t quantity,total;
            
            quantity=(int64_t)mc_GetABQuantity(ptr);
            
            total=quantity;
            if(!is_input)
            {
                total=-quantity;
            }
            
            if(total_amounts)
            {
                int row=total_amounts->Seek(ptr);
                if(row >= 0)
                {
                    total+=(int64_t)mc_GetABQuantity(total_amounts->GetRow(row));
                    mc_SetABQuantity(total_amounts->GetRow(row),total);
                }
                else
                {
                    memcpy(buf,ptr,MC_AST_ASSET_QUANTITY_OFFSET);
                    mc_SetABQuantity(buf,total);
                    total_amounts->Add(buf);                                        
                }
            }
                    
            
            if(mc_gState->m_Assets->FindEntityByFullRef(&entity,ptr))
            {
                txid=entity.GetTxID();
                asset_entry=AssetEntry(txid,quantity,0x00);
                assets.push_back(asset_entry);
            }        
        }
    }
        
    result.push_back(Pair("amount", ValueFromAmount(txout.nValue)));    
    result.push_back(Pair("assets", assets));    
    if(address_found)
    {
        result.push_back(Pair("address", address.ToString()));
    }
    else
    {
        result.push_back(Pair("address", ""));        
    }
    
//    result.push_back(Pair("error", strError));
    return result;
}

Object DecodeExchangeTransaction(const CTransaction tx,int verbose,int64_t& native_balance,mc_Buffer *lpAssets,bool& is_complete,bool allow_last_sighashall,string& strError)
{
    Object result;
    strError="";
    native_balance=0;
    bool can_disable=false;
    uint32_t format;
    
    vector <CTxOut> input_txouts;
    vector <string> input_errors;

    mc_Buffer *asset_amounts=mc_gState->m_TmpBuffers->m_RpcABBuffer2;
    asset_amounts->Clear();
    
    mc_Script *lpScript=mc_gState->m_TmpBuffers->m_RpcScript4;
    lpScript->Clear();    

    AcceptExchange(tx, input_txouts, input_errors,strError);

    if(strError.size())
    {
        throw JSONRPCError(RPC_EXCHANGE_ERROR, strError);        
    }
    
    native_balance=0;
    Array exchanges;
    
    for(int i=0;i<(int)input_txouts.size();i++)
    {
        Object exchange;
        string entry_error;
        
        
        const CScript& script2 = tx.vin[i].scriptSig;        
        CScript::const_iterator pc2 = script2.begin();

        lpScript->Clear();
        lpScript->SetScript((unsigned char*)(&pc2[0]),(size_t)(script2.end()-pc2),MC_SCR_TYPE_SCRIPTSIG);
        
        if((input_errors[i].size() > 0) && (input_txouts[i].nValue < 0) )
        {
            input_txouts[i].nValue=0;
        }
        
        native_balance+=(int64_t)input_txouts[i].nValue-(int64_t)tx.vout[i].nValue;
        
        entry_error=input_errors[i];
        
        Object offer=ExchangeAssetEntry(tx.vin[i].prevout.hash,input_txouts[i],lpScript,asset_amounts,lpAssets,can_disable,
                (i==(int)input_txouts.size()-1) ? allow_last_sighashall : false,entry_error);
        
        
        
        offer.push_back(Pair("txid",  tx.vin[i].prevout.hash.ToString()));    
        offer.push_back(Pair("vout",  (uint64_t)(tx.vin[i].prevout.n)));    
        
        exchange.push_back(Pair("offer", offer));
        if(entry_error.size() == 0)
        {
            if (!VerifyScript(script2, input_txouts[i].scriptPubKey, STANDARD_SCRIPT_VERIFY_FLAGS, TransactionSignatureChecker(&tx, i)))
            {
                entry_error="Wrong signature";
            }
        }
        
        if(entry_error.size())
        {
            throw JSONRPCError(RPC_EXCHANGE_ERROR, entry_error + strprintf("; Input: %d, txid: %s, vout: %d",i,tx.vin[i].prevout.hash.GetHex().c_str(),tx.vin[i].prevout.n));        
        }
        
        entry_error="";
        lpScript->Clear();
        exchange.push_back(Pair("ask", ExchangeAssetEntry(0,tx.vout[i],lpScript,asset_amounts,lpAssets,can_disable,true,entry_error)));
        if(entry_error.size())
        {
            throw JSONRPCError(RPC_EXCHANGE_ERROR, entry_error+ strprintf("; Output: %d,",i));        
        }
                
//        exchange.push_back(Pair("amount",ValueFromAmount((int64_t)input_txouts[i].nValue-(int64_t)tx.vout[i].nValue)));
        exchanges.push_back(exchange);
    }
    
    Array aMetaData;
    set<uint256> streams_already_seen;
    
    if(tx.vout.size() > tx.vin.size())
    {
        for(int i=(int)input_txouts.size();i<(int)tx.vout.size();i++)
        {
            const CTxOut& txout = tx.vout[i];
            if(!txout.scriptPubKey.IsUnspendable())
            {
                throw JSONRPCError(RPC_EXCHANGE_ERROR, "Too many outputs");                        
            }            
            else
            {
                const CScript& script1 = tx.vout[i].scriptPubKey;        
                CScript::const_iterator pc1 = script1.begin();
                size_t elem_size;
                const unsigned char *elem;

                lpScript->Clear();
                lpScript->SetScript((unsigned char*)(&pc1[0]),(size_t)(script1.end()-pc1),MC_SCR_TYPE_SCRIPTPUBKEY);                
                
                lpScript->ExtractAndDeleteDataFormat(&format);
                
                if(lpScript->GetNumElements()<=1)
                {
                    if(lpScript->GetNumElements()==1)
                    {
                        elem = lpScript->GetData(lpScript->GetNumElements()-1,&elem_size);
                        aMetaData.push_back(OpReturnEntry(elem,elem_size,tx.GetHash(),i));
                    }                        
                }
                else
                {
                    Value data_item_entry=DataItemEntry(tx,i,streams_already_seen, 0x03);
                    if(!data_item_entry.is_null())
                    {
                        aMetaData.push_back(data_item_entry);
                    }
                    else
                    {
                        elem = lpScript->GetData(lpScript->GetNumElements()-1,&elem_size);
                        if(elem_size)
                        {
                            aMetaData.push_back(OpReturnEntry(elem,elem_size,tx.GetHash(),i));
                        }
                    }
                }                
            }
        }        
    }

    
    is_complete=true;
    if(lpAssets != NULL)
    {
        Object tocomplete;
        Array offer;
        Array ask;
        unsigned char buf[MC_AST_ASSET_FULLREF_BUF_SIZE];
        memset(buf,0,MC_AST_ASSET_FULLREF_BUF_SIZE);
        int multiple=1;
        
        for(int a=0;a<lpAssets->GetCount();a++)
        {
            Object asset_entry;
            unsigned char *ptr=(unsigned char *)lpAssets->GetRow(a);

            mc_EntityDetails entity;
            const unsigned char *txid;
            
            int64_t quantity;
            
            quantity=(int64_t)mc_GetABQuantity(ptr);
            
            if(quantity != 0)
            {
                is_complete=false;
                if(mc_gState->m_Assets->FindEntityByFullRef(&entity,ptr))
                {
                    txid=entity.GetTxID();
                    if(quantity > 0)
                    {
                        asset_entry=AssetEntry(txid,quantity,0x00);
                        offer.push_back(asset_entry);
                    }
                    else
                    {
                        quantity=-quantity;
                        asset_entry=AssetEntry(txid,quantity,0x00);
                        ask.push_back(asset_entry);  
                        uint256 hash;
                        hash=*(uint256*)txid;
                        
//                        ParseAssetKey(hash.GetHex().c_str(),NULL,buf,NULL,&multiple,NULL, MC_ENT_TYPE_ASSET);
                        ParseAssetKeyToFullAssetRef(hash.GetHex().c_str(),buf,&multiple,NULL, MC_ENT_TYPE_ASSET);
                        tocomplete.push_back(Pair(hash.GetHex(),(double)quantity/double(multiple)));
                    }
                }        
            }
        }
        
/*        
        tocomplete.push_back(Pair("offer", offer));
        tocomplete.push_back(Pair("ask", ask));
        tocomplete.push_back(Pair("amount", ValueFromAmount(-native_balance)));
*/
        Object obj_offer;
        Object obj_ask;
        
        CAmount required_fee=0;
        int64_t dust=0;
        
        unsigned int nBytes = ::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION);
        
        CWallet::minTxFee = CFeeRate(MIN_RELAY_TX_FEE); 
        ::minRelayTxFee = CFeeRate(MIN_RELAY_TX_FEE); 
        
        required_fee=pwalletMain->GetMinimumFee(nBytes,1,mempool);
        
        if(!is_complete || (native_balance<required_fee))
        {
            nBytes+=4+36+1+73+1+65+4+8+1+24+4+ask.size()*(mc_gState->m_NetworkParams->m_AssetRefSize+MC_AST_ASSET_QUANTITY_SIZE)+1+32;
            required_fee=pwalletMain->GetMinimumFee(nBytes,1,mempool);
            dust=-1;
            if(mc_gState->m_NetworkParams->IsProtocolMultichain())
            {
                dust=MCP_MINIMUM_PER_OUTPUT;
            }            
            if(dust<0)
            {
                size_t nSize = GetSerializeSize(SER_DISK,0)+148u;
                dust=3*minRelayTxFee.GetFee(nSize);
            }
        }
        
//        native_balance-=required_fee;
        
        if(native_balance >= 0)
        {
            obj_offer.push_back(Pair("amount", ValueFromAmount(native_balance)));
            obj_ask.push_back(Pair("amount", ValueFromAmount(0)));
        }
        else
        {
            is_complete=false;
            obj_offer.push_back(Pair("amount", ValueFromAmount(0)));
            obj_ask.push_back(Pair("amount", ValueFromAmount(-native_balance)));            
        }
        
        obj_offer.push_back(Pair("assets", offer));
        obj_ask.push_back(Pair("assets", ask));
        
        result.push_back(Pair("offer", obj_offer));
        result.push_back(Pair("ask", obj_ask));
        
        
        if(is_complete)
        {
            result.push_back(Pair("completedfee", ValueFromAmount(native_balance)));
        }
        result.push_back(Pair("requiredfee", ValueFromAmount(required_fee)));                
        
//        result.push_back(Pair("tocomplete", tocomplete));
        
        result.push_back(Pair("candisable", can_disable));
        
//        result.push_back(Pair("tocomplete", tocomplete));
        if(is_complete)
        {
            if(native_balance<required_fee)
            {
                if(pwalletMain->GetBalance()>=required_fee-native_balance)
                {
                    result.push_back(Pair("cancomplete", true));                                
                }
                else
                {
                    result.push_back(Pair("cancomplete", false));                                                    
                }
            }
            else
            {                
                result.push_back(Pair("cancomplete", true));            
            }
        }
        else
        {    
            CPubKey pkey;
            if(!pwalletMain->GetKeyFromAddressBook(pkey,MC_PTP_SEND | MC_PTP_RECEIVE))
            {
                result.push_back(Pair("cancomplete", false));            
            }
            else
            {
                CTxDestination address=CTxDestination(pkey.GetID());    

                mc_Script *lpScript=mc_gState->m_TmpBuffers->m_RpcScript4;
                lpScript->Clear();

                Value param=tocomplete;
                uint256 offer_hash;            
                CAmount nAmount;
                int eErrorCode;
                string strError=ParseRawOutputObject(param,nAmount,lpScript,&eErrorCode);
                if(strError.size())
                {
                    throw JSONRPCError(eErrorCode, strError);                            
                }

                nAmount=-native_balance+dust+required_fee;
                if(nAmount<2*dust)
                {
                    nAmount=2*dust;
                }    

                CScript scriptPubKey = GetScriptForDestination(address);

                size_t elem_size;
                const unsigned char *elem;

                for(int element=0;element < lpScript->GetNumElements();element++)
                {
                    elem = lpScript->GetData(element,&elem_size);
                    if(elem)
                    {
                        scriptPubKey << vector<unsigned char>(elem, elem + elem_size) << OP_DROP;
                    }
                }

                CScript scriptOpReturn=CScript();

                CReserveKey reservekey(pwalletMain);
                CAmount nFeeRequired;
                CWalletTx wtxNew;

                {
                    LOCK (pwalletMain->cs_wallet_send);
                    if (!pwalletMain->CreateTransaction(scriptPubKey, nAmount, scriptOpReturn, wtxNew, reservekey, nFeeRequired, strError))
                    {
                        result.push_back(Pair("cancomplete", false));            
                    }
                    else
                    {
                        result.push_back(Pair("cancomplete", true));                            
                    }
                }
            }
        }
            
        result.push_back(Pair("complete", is_complete));
    }
    
    

    if(verbose)
    {
        result.push_back(Pair("exchanges", exchanges));
//        result.push_back(Pair("data", aMetaData));
    }    
    
//    result.push_back(Pair("error", strError));
//    result.push_back(Pair("valid", is_valid));
    return result;
}



string FindExchangeOutPoint(const json_spirit::Array& params,int first_param,COutPoint& offer_input,CAmount& nAmount,mc_Script *lpScript,int *eErrorCode)
{
    string strError="";
    
    uint256 offer_hash;
    
//    bool offer_input_found=false;
    
//    uint32_t ts=0;
    uint256 given_hash=0;
    int given_vout=-1;
//    if(params.size() >= 4)
    {
        given_hash.SetHex(params[first_param+0].get_str());
        given_vout=params[first_param+1].get_int();
    }
  
    offer_input=COutPoint(given_hash,given_vout);
    
    uint256 ask_hash;
    nAmount=0;
    lpScript->Clear();
    
    *eErrorCode=RPC_INVALID_PARAMETER;
    if (params[first_param+2].type() != obj_type)
    {
        strError= "Invalid ask assets object";        
        return strError;
    }
    else
    {
        string strError=ParseRawOutputObject(params[first_param+2],nAmount,lpScript,eErrorCode);
        if(strError.size())
        {
            return strError;
        }
        CTxOut tmpTxOut=CTxOut(nAmount,CScript());
        if (tmpTxOut.IsDust(::minRelayTxFee))
        {
            strError="Ask amount too small";
            return strError;
        }
        
    }
    
    return strError;
}

Value createrawexchange(const json_spirit::Array& params, bool fHelp)
{
    if (fHelp || params.size() != 3)
        throw runtime_error("Help message not found\n");
    
    
    COutPoint offer_input;
    mc_Script *lpScript=mc_gState->m_TmpBuffers->m_RpcScript3;
    lpScript->Clear();
    CAmount nAmount=0;
    int eErrorCode;
    string strError=FindExchangeOutPoint(params,0,offer_input,nAmount,lpScript,&eErrorCode);
    if(strError.size())
    {
        throw JSONRPCError(eErrorCode, strError);                            
    }

    CTxOut preparedTxOut;
    if(!FindPreparedTxOut(preparedTxOut,offer_input,strError))
    {
        throw JSONRPCError(RPC_WALLET_OUTPUT_NOT_FOUND, strError);                            
    }
    
    const CScript& script1 = preparedTxOut.scriptPubKey;        
    CTxDestination addressRet;        
    if(!ExtractDestination(script1, addressRet))
    {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Cannot extract address from prepared output");                                    
    }

    CKeyID *lpKeyID=boost::get<CKeyID> (&addressRet);
    if(lpKeyID == NULL)
    {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Prepared output should be pay-to-pubkeyhash");                                    
    }
    
    if(mc_gState->m_Permissions->CanSend(NULL,(unsigned char*)(lpKeyID)) == 0)
    {
        throw JSONRPCError(RPC_INSUFFICIENT_PERMISSIONS, "Address of prepared output doesn't have send permission");                                    
    }
    
    if(mc_gState->m_Permissions->CanReceive(NULL,(unsigned char*)(lpKeyID)) == 0)
    {
        throw JSONRPCError(RPC_INSUFFICIENT_PERMISSIONS, "Address of prepared output doesn't have receive permission");                                    
    }
    
    vector<CTxDestination> addresses;    
    addresses.push_back(addressRet);        
        
    
    
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
    
    CMutableTransaction tx;
    
    CTxOut txout(nAmount, scriptPubKey);

    tx.vout.push_back(txout);
    tx.vin.push_back(CTxIn(offer_input));
    
    if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)
    {
        int err;
        const CWalletTx& wtx=pwalletTxsMain->GetWalletTx(offer_input.hash,NULL,&err);
        if(err)
        {
            throw JSONRPCError(RPC_WALLET_OUTPUT_NOT_FOUND, "Output not found in wallet");                                        
        }
        if(!SignSignature(*pwalletMain, wtx.vout[offer_input.n].scriptPubKey, tx, tx.vin.size()-1, SIGHASH_SINGLE | SIGHASH_ANYONECANPAY ))
        {
            throw JSONRPCError(RPC_WALLET_ERROR, "Signing transaction failed");                                    
        }
    }
    else
    {
        std::map<uint256, CWalletTx>::const_iterator it = pwalletMain->mapWallet.find(offer_input.hash);

        if (it == pwalletMain->mapWallet.end())
        {
            throw JSONRPCError(RPC_WALLET_OUTPUT_NOT_FOUND, "Output not found in wallet");                            
        }

        const CWalletTx* pcoin = &(*it).second;


        if(!SignSignature(*pwalletMain, pcoin->vout[offer_input.n].scriptPubKey, tx, tx.vin.size()-1, SIGHASH_SINGLE | SIGHASH_ANYONECANPAY ))
        {
            throw JSONRPCError(RPC_WALLET_ERROR, "Signing transaction failed");                                    
        }
    }
        
    return EncodeHexTx(tx);
}


Value appendrawexchange(const json_spirit::Array& params, bool fHelp)
{
    if (fHelp || params.size() != 4 )
        throw runtime_error("Help message not found\n");

    COutPoint offer_input;
    mc_Script *lpScript=mc_gState->m_TmpBuffers->m_RpcScript3;
    lpScript->Clear();
    CAmount nAmount=0;
    int eErrorCode;
    
    string strError=FindExchangeOutPoint(params,1,offer_input,nAmount,lpScript,&eErrorCode);
    if(strError.size())
    {
        throw JSONRPCError(eErrorCode, strError);                            
    }
  
    CTxOut preparedTxOut;
    if(!FindPreparedTxOut(preparedTxOut,offer_input,strError))
    {
        throw JSONRPCError(RPC_WALLET_OUTPUT_NOT_FOUND, strError);                            
    }
    
    const CScript& script1 = preparedTxOut.scriptPubKey;        
    CTxDestination addressRet;        
    if(!ExtractDestination(script1, addressRet))
    {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Cannot extract address from prepared output");                                    
    }

    CKeyID *lpKeyID=boost::get<CKeyID> (&addressRet);
    if(lpKeyID == NULL)
    {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Prepared output should be pay-to-pubkeyhash");                                    
    }
    
    if(mc_gState->m_Permissions->CanSend(NULL,(unsigned char*)(lpKeyID)) == 0)
    {
        throw JSONRPCError(RPC_INSUFFICIENT_PERMISSIONS, "Address of prepared output doesn't have send permission");                                    
    }
    
    if(mc_gState->m_Permissions->CanReceive(NULL,(unsigned char*)(lpKeyID)) == 0)
    {
        throw JSONRPCError(RPC_INSUFFICIENT_PERMISSIONS, "Address of prepared output doesn't have receive permission");                                    
    }
    
    vector<CTxDestination> addresses;    
    addresses.push_back(addressRet);        
    
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
    
    CMutableTransaction tx;
    
    vector<unsigned char> txData(ParseHex(params[0].get_str()));
    
    CDataStream ssData(txData, SER_NETWORK, PROTOCOL_VERSION);
    ssData >> tx;
    
    CTxOut txout(nAmount, scriptPubKey);

    tx.vout.push_back(txout);
    tx.vin.push_back(CTxIn(offer_input));
    
    if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)
    {
        int err;
        const CWalletTx& wtx=pwalletTxsMain->GetWalletTx(offer_input.hash,NULL,&err);
        if(err)
        {
            throw JSONRPCError(RPC_WALLET_OUTPUT_NOT_FOUND, "Output not found in wallet");                                        
        }
        if(!SignSignature(*pwalletMain, wtx.vout[offer_input.n].scriptPubKey, tx, tx.vin.size()-1, SIGHASH_SINGLE | SIGHASH_ANYONECANPAY ))
        {
            throw JSONRPCError(RPC_WALLET_ERROR, "Signing transaction failed");                                    
        }
    }
    else
    {
        std::map<uint256, CWalletTx>::const_iterator it = pwalletMain->mapWallet.find(offer_input.hash);

        if (it == pwalletMain->mapWallet.end())
        {
            throw JSONRPCError(RPC_WALLET_OUTPUT_NOT_FOUND, "Output not found in wallet");                            
        }

        const CWalletTx* pcoin = &(*it).second;


        if(!SignSignature(*pwalletMain, pcoin->vout[offer_input.n].scriptPubKey, tx, tx.vin.size()-1, SIGHASH_SINGLE | SIGHASH_ANYONECANPAY ))
        {
            throw JSONRPCError(RPC_WALLET_ERROR, "Signing transaction failed");                                    
        }
    }        
    
    bool is_complete=true;
    int64_t native_balance;
    
    mc_Buffer *asset_amounts=mc_gState->m_TmpBuffers->m_RpcABBuffer1;
    asset_amounts->Clear();
    
    {
        LOCK(cs_main);
        Object decode_result;
        
        decode_result=DecodeExchangeTransaction(tx,0,native_balance,asset_amounts,is_complete,false,strError);
    }

    Object result;
    result.push_back(Pair("hex", EncodeHexTx(tx)));
    result.push_back(Pair("complete", is_complete));

    return result;
}

Value completerawexchange(const json_spirit::Array& params, bool fHelp)
{
    if (fHelp || params.size() < 4 || params.size() > 5)
        throw runtime_error("Help message not found\n");

    COutPoint offer_input;
    mc_Script *lpScript=mc_gState->m_TmpBuffers->m_RpcScript3;
    lpScript->Clear();
    CAmount nAmount=0;
    int eErrorCode;
    
    string strError=FindExchangeOutPoint(params,1,offer_input,nAmount,lpScript,&eErrorCode);
    if(strError.size())
    {
        throw JSONRPCError(eErrorCode, strError);                            
    }
  
    CTxOut preparedTxOut;
    if(!FindPreparedTxOut(preparedTxOut,offer_input,strError))
    {
        throw JSONRPCError(RPC_WALLET_OUTPUT_NOT_FOUND, strError);                            
    }
    
    const CScript& script1 = preparedTxOut.scriptPubKey;        
    CTxDestination addressRet;        
    if(!ExtractDestination(script1, addressRet))
    {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Cannot extract address from prepared output");                                    
    }

    CKeyID *lpKeyID=boost::get<CKeyID> (&addressRet);
    if(lpKeyID == NULL)
    {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Prepared output should be pay-to-pubkeyhash");                                    
    }
    
    if(mc_gState->m_Permissions->CanSend(NULL,(unsigned char*)(lpKeyID)) == 0)
    {
        throw JSONRPCError(RPC_INSUFFICIENT_PERMISSIONS, "Address of prepared output doesn't have send permission");                                    
    }
    
    if(mc_gState->m_Permissions->CanReceive(NULL,(unsigned char*)(lpKeyID)) == 0)
    {
        throw JSONRPCError(RPC_INSUFFICIENT_PERMISSIONS, "Address of prepared output doesn't have receive permission");                                    
    }
    
    vector<CTxDestination> addresses;    
    addresses.push_back(addressRet);        
    
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
    
    CMutableTransaction tx;
    
    vector<unsigned char> txData(ParseHex(params[0].get_str()));
    
    CDataStream ssData(txData, SER_NETWORK, PROTOCOL_VERSION);
    ssData >> tx;
    
    CTxOut txout(nAmount, scriptPubKey);

    tx.vout.push_back(txout);
    tx.vin.push_back(CTxIn(offer_input));
    
    if(params.size() > 4)
    {    
        mc_EntityDetails found_entity;
        CScript scriptOpReturn=ParseRawMetadata(params[4],MC_DATA_API_PARAM_TYPE_SIMPLE,NULL,&found_entity);
        
        if(found_entity.GetEntityType() == MC_ENT_TYPE_STREAM)
        {        
            const unsigned char *aptr;

            aptr=GetAddressIDPtr(addresses[0]);
            if(aptr)
            {
                if((found_entity.AnyoneCanWrite() == 0) && (mc_gState->m_Permissions->CanWrite(found_entity.GetTxID(),aptr) == 0))
                {
                    throw JSONRPCError(RPC_INSUFFICIENT_PERMISSIONS, "Publishing in this stream is not allowed from this address");                                                                        
                }                                                 
                if(mc_gState->m_Permissions->CanSend(NULL,aptr) == 0)
                {
                    throw JSONRPCError(RPC_INSUFFICIENT_PERMISSIONS, "Address doesn't have send permission");                                                                        
                }                                                 
            }
        }
        
        CTxOut txoutdata(0, scriptOpReturn);
        tx.vout.push_back(txoutdata);        
    }    
    
    if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)
    {
        int err;
        const CWalletTx& wtx=pwalletTxsMain->GetWalletTx(offer_input.hash,NULL,&err);
        if(err)
        {
            throw JSONRPCError(RPC_WALLET_OUTPUT_NOT_FOUND, "Output not found in wallet");                                        
        }
        if(!SignSignature(*pwalletMain, wtx.vout[offer_input.n].scriptPubKey, tx, tx.vin.size()-1, SIGHASH_ALL ))
        {
            throw JSONRPCError(RPC_WALLET_ERROR, "Signing transaction failed");                                    
        }
    }
    else
    {
        std::map<uint256, CWalletTx>::const_iterator it = pwalletMain->mapWallet.find(offer_input.hash);

        if (it == pwalletMain->mapWallet.end())
        {
            throw JSONRPCError(RPC_WALLET_OUTPUT_NOT_FOUND, "Output not found in wallet");                            
        }

        const CWalletTx* pcoin = &(*it).second;


        if(!SignSignature(*pwalletMain, pcoin->vout[offer_input.n].scriptPubKey, tx, tx.vin.size()-1, SIGHASH_ALL ))
        {
            throw JSONRPCError(RPC_WALLET_ERROR, "Signing transaction failed");                                    
        }
    }        
    
    bool is_complete=true;
    int64_t native_balance;
    
    mc_Buffer *asset_amounts=mc_gState->m_TmpBuffers->m_RpcABBuffer1;
    asset_amounts->Clear();
    
    {
        LOCK(cs_main);
        Object decode_result;
        
        decode_result=DecodeExchangeTransaction(tx,0,native_balance,asset_amounts,is_complete,true,strError);
    }
    
    if(!is_complete)
    {
        throw JSONRPCError(RPC_EXCHANGE_ERROR, "Incomplete exchange");                                            
    }
    
    return EncodeHexTx(tx);
}

Value decoderawexchange(const json_spirit::Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1)
        throw runtime_error("Help message not found\n");

    CTransaction tx;
    
    if (!DecodeHexTx(tx, params[0].get_str()))
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "TX decode failed");

    int verbose=0;
    if (params.size() > 1)    
    {
        if(paramtobool(params[1]))
        {
            verbose=1;
        }        
    }
    

    Object result;
    bool is_complete=true;
    int64_t native_balance;
    string strError;
    
    mc_Buffer *asset_amounts=mc_gState->m_TmpBuffers->m_RpcABBuffer1;
    asset_amounts->Clear();
    
    {
        LOCK(cs_main);
        
        result=DecodeExchangeTransaction(tx,verbose,native_balance,asset_amounts,is_complete,true,strError);
    }
    
    return result;
}


