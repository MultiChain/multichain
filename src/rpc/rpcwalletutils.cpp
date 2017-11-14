// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2014-2016 The Bitcoin Core developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2017 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.


#include "rpc/rpcwallet.h"

void MinimalWalletTxToJSON(const CWalletTx& wtx, Object& entry)
{
    if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)
    {
        uint256 blockHash;
        int block=wtx.txDef.m_Block;
        int confirms = wtx.GetDepthInMainChain();
        entry.push_back(Pair("confirmations", confirms));
        if (confirms > 0)
        {
            blockHash=chainActive[block]->GetBlockHash();
            entry.push_back(Pair("blocktime", mapBlockIndex[blockHash]->GetBlockTime()));
        }
        uint256 hash = wtx.GetHash();
        entry.push_back(Pair("txid", hash.GetHex()));
    }
    else
    {
        int confirms = wtx.GetDepthInMainChain();
        entry.push_back(Pair("confirmations", confirms));
        if (confirms > 0)
        {
            entry.push_back(Pair("blocktime", mapBlockIndex[wtx.hashBlock]->GetBlockTime()));
        }
        uint256 hash = wtx.GetHash();
        entry.push_back(Pair("txid", hash.GetHex()));
    }    
}

void WalletTxToJSON(const CWalletTx& wtx, Object& entry,bool skipWalletConflicts, int vout)
{
    /* MCHN START */        
    if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)
    {
        int block=wtx.txDef.m_Block;
        uint256 blockHash;
        int confirms = wtx.GetDepthInMainChain();
        entry.push_back(Pair("confirmations", confirms));
        if (wtx.IsCoinBase())
            entry.push_back(Pair("generated", true));
        int64_t nTimeSmart=(int64_t)wtx.txDef.m_TimeReceived;
        if (confirms > 0)
        {
            blockHash=chainActive[block]->GetBlockHash();
            entry.push_back(Pair("blockhash", chainActive[block]->GetBlockHash().GetHex()));
            entry.push_back(Pair("blockindex", (int64_t)wtx.txDef.m_BlockIndex));
            entry.push_back(Pair("blocktime", mapBlockIndex[blockHash]->GetBlockTime()));
            if(mapBlockIndex[blockHash]->GetBlockTime()<nTimeSmart)
            {
                nTimeSmart=mapBlockIndex[blockHash]->GetBlockTime();
            }
        }
        uint256 hash = wtx.GetHash();
        entry.push_back(Pair("txid", hash.GetHex()));
        if(vout >= 0)
        {
            entry.push_back(Pair("vout", vout));            
        }
        Array conflicts;
        if(!skipWalletConflicts)
        {
            entry.push_back(Pair("walletconflicts", conflicts));
        }
        if(wtx.txDef.m_Flags & MC_TFL_INVALID)
        {
            entry.push_back(Pair("valid", false));            
        }
        else
        {
            entry.push_back(Pair("valid", true));            
        }
        entry.push_back(Pair("time", nTimeSmart));
        entry.push_back(Pair("timereceived", (int64_t)wtx.txDef.m_TimeReceived));
        BOOST_FOREACH(const PAIRTYPE(string,string)& item, wtx.mapValue)
        {
            if((item.first.size() <13) || (memcmp(item.first.c_str(),"fullsize",8) != 0))
            {
                entry.push_back(Pair(item.first, item.second));
            }
        }
    }
    else
    {
    /* MCHN END */
        int confirms = wtx.GetDepthInMainChain();
        entry.push_back(Pair("confirmations", confirms));
        if (wtx.IsCoinBase())
            entry.push_back(Pair("generated", true));
        if (confirms > 0)
        {
            entry.push_back(Pair("blockhash", wtx.hashBlock.GetHex()));
            entry.push_back(Pair("blockindex", wtx.nIndex));
            entry.push_back(Pair("blocktime", mapBlockIndex[wtx.hashBlock]->GetBlockTime()));
        }
        uint256 hash = wtx.GetHash();
        entry.push_back(Pair("txid", hash.GetHex()));
        Array conflicts;
        BOOST_FOREACH(const uint256& conflict, wtx.GetConflicts())
            conflicts.push_back(conflict.GetHex());
    /* MCHN START */
        if(!skipWalletConflicts)
        {
    /* MCHN END */
        entry.push_back(Pair("walletconflicts", conflicts));
    /* MCHN START */        
        }
        if(conflicts.size())
        {
            if(confirms <= 0)
            {
                entry.push_back(Pair("valid", false));            
            }
            else
            {
                entry.push_back(Pair("valid", true));                            
            }
        }
        else
        {
            entry.push_back(Pair("valid", true));            
        }
    /* MCHN END */
        entry.push_back(Pair("time", wtx.GetTxTime()));
        entry.push_back(Pair("timereceived", (int64_t)wtx.nTimeReceived));
        BOOST_FOREACH(const PAIRTYPE(string,string)& item, wtx.mapValue)
            entry.push_back(Pair(item.first, item.second));
    }
}


//void SendMoneyToSeveralAddresses(const std::vector<CTxDestination> addresses, CAmount nValue, CWalletTx& wtxNew,mc_Script *dropscript,mc_Script *opreturnscript,const std::vector<CTxDestination>& fromaddresses)
void SendMoneyToSeveralAddresses(const std::vector<CTxDestination> addresses, CAmount nValue, CWalletTx& wtxNew,mc_Script *dropscript,CScript scriptOpReturn,const std::vector<CTxDestination>& fromaddresses)
{
    // Check amount
    
/*    
    if (nValue < 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid amount");
*/
    string strError;
    if (pwalletMain->IsLocked())
    {
        strError = "Error: Wallet locked, unable to create transaction!";
        LogPrintf("SendMoney() : %s", strError);
        throw JSONRPCError(RPC_WALLET_UNLOCK_NEEDED, strError);
    }

    set<CTxDestination> thisFromAddresses;
    set<CTxDestination> *lpFromAddresses;
    
    BOOST_FOREACH(const CTxDestination& fromaddress, fromaddresses)
    {
        thisFromAddresses.insert(fromaddress);
    }
    
    lpFromAddresses=NULL;
    if(thisFromAddresses.size())
    {
        lpFromAddresses=&thisFromAddresses;
    }
    
    // Parse Bitcoin address

    
    std::vector <CScript> scriptPubKeys;
    size_t elem_size;
    const unsigned char *elem;
    
    BOOST_FOREACH (const CTxDestination& address, addresses)
    {
    
        CScript scriptPubKey = GetScriptForDestination(address);


        if(dropscript)
        {
            if(fDebug)LogPrint("mchnminor","mchn: Sending script with %d OP_DROP element(s)",dropscript->GetNumElements());
            if(dropscript->GetNumElements() > MCP_STD_OP_DROP_COUNT )
                throw JSONRPCError(RPC_INTERNAL_ERROR, "Invalid number of elements in script");

            for(int element=0;element < dropscript->GetNumElements();element++)
            {
                elem = dropscript->GetData(element,&elem_size);
                if(elem)
                {
                    scriptPubKey << vector<unsigned char>(elem, elem + elem_size) << OP_DROP;
                }
                else
                    throw JSONRPCError(RPC_INTERNAL_ERROR, "Invalid script");
            }
        }        
        
        scriptPubKeys.push_back(scriptPubKey);
    }
    
    // Create and send the transaction
    
    CReserveKey reservekey(pwalletMain);
    CAmount nFeeRequired;
    int eErrorCode;
    if (!pwalletMain->CreateTransaction(scriptPubKeys, nValue, scriptOpReturn, wtxNew, reservekey, nFeeRequired, strError, NULL, lpFromAddresses, 1,-1,-1, NULL, &eErrorCode))
    {
/*        
        if (nValue + nFeeRequired > pwalletMain->GetBalance())
            strError = strprintf("Error: This transaction requires a transaction fee of at least %s because of its amount, complexity, or use of recently received funds!", FormatMoney(nFeeRequired));
 */ 
        LogPrintf("SendMoney() : %s\n", strError);
        throw JSONRPCError(eErrorCode, strError);
    }
    
    string strRejectReason;
    if (!pwalletMain->CommitTransaction(wtxNew, reservekey, strRejectReason))
    {
        if(strRejectReason.size())
        {
            throw JSONRPCError(RPC_TRANSACTION_REJECTED, "Error: The transaction was rejected: " + strRejectReason);
        }
        else
        {
            throw JSONRPCError(RPC_TRANSACTION_REJECTED, "Error: this transaction was rejected. This may be because you are sharing private keys between nodes, and another node has spent the funds used by this transaction.");
        }                        
    }            
}

vector<CTxDestination> ParseAddresses(string param, bool create_full_list, bool allow_scripthash)
{
    vector<CTxDestination> addresses;
    set<CTxDestination> thisAddresses;
    set<CBitcoinAddress> setAddress;

    string tok;

    CKeyID *lpKeyID;
    if( param == "*" )
    {
        if(create_full_list)
        {
            BOOST_FOREACH(const PAIRTYPE(CBitcoinAddress, CAddressBookData)& item, pwalletMain->mapAddressBook)
            {
                const CBitcoinAddress& address = item.first; 
                CTxDestination dest=address.Get();
                lpKeyID=boost::get<CKeyID> (&dest);

                if(lpKeyID)
                {
                    addresses.push_back(address.Get());                
                }
            }
        }
    }
    else
    {
        stringstream ss(param); 
        while(getline(ss, tok, ',')) 
        {
            CBitcoinAddress address(tok);
            if (!address.IsValid())
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address: "+tok);            
            if (setAddress.count(address))
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, duplicated address: "+tok);
                CTxDestination dest=address.Get();
            CKeyID *lpKeyID=boost::get<CKeyID> (&dest);
            CScriptID *lpScriptID=boost::get<CScriptID> (&dest);
            
            if( (lpKeyID != NULL) || (( lpScriptID != NULL) && allow_scripthash ))
            {        
                addresses.push_back(address.Get());
                setAddress.insert(address);
            }
            else
            {
                if(allow_scripthash)
                {
                    throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address (only pubkeyhash and scripthash addresses are supported) : "+tok);                
                }
                else
                {
                    throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address (only pubkeyhash addresses are supported) : "+tok);                                    
                }
            }
        }
    }
    
    return addresses;
}

void FindAddressesWithPublishPermission(vector<CTxDestination>& fromaddresses,mc_EntityDetails *stream_entity)
{
    if(fromaddresses.size() == 1)
    {
        const unsigned char *aptr;

        aptr=GetAddressIDPtr(fromaddresses[0]);
        if(aptr)
        {
            if((stream_entity->AnyoneCanWrite() == 0) && (mc_gState->m_Permissions->CanWrite(stream_entity->GetTxID(),aptr) == 0))
            {
                throw JSONRPCError(RPC_INSUFFICIENT_PERMISSIONS, "Publishing in this stream is not allowed from this address");                                                                        
            }                                                 
            if(mc_gState->m_Permissions->CanSend(NULL,aptr) == 0)
            {
                throw JSONRPCError(RPC_INSUFFICIENT_PERMISSIONS, "from-address doesn't have send permission");                                                                        
            }                                                 
        }
    }
    else
    {
        bool publisher_found=false;
        BOOST_FOREACH(const PAIRTYPE(CBitcoinAddress, CAddressBookData)& item, pwalletMain->mapAddressBook)
        {
            const CBitcoinAddress& address = item.first;
            CKeyID keyID;

            if(address.GetKeyID(keyID))
            {
                if((stream_entity->AnyoneCanWrite() != 0) || (mc_gState->m_Permissions->CanWrite(stream_entity->GetTxID(),(unsigned char*)(&keyID)) != 0))
                {
                    if(mc_gState->m_Permissions->CanSend(NULL,(unsigned char*)(&keyID)))
                    {
                        publisher_found=true;
                        fromaddresses.push_back(address.Get());
                    }
                }
            }
        }                    
        if(!publisher_found)
        {
            throw JSONRPCError(RPC_INSUFFICIENT_PERMISSIONS, "This wallet contains no addresses with permission to write to this stream and global send permission.");                                                                                            
        }
    }    
}

set<string> ParseAddresses(Value param, isminefilter filter)
{
    set<string> setAddresses;
    
    if(param.type() == array_type)
    {
        BOOST_FOREACH(const Value& vtok, param.get_array())
        {
            if(vtok.type() == str_type)
            {
                string tok=vtok.get_str();
                CBitcoinAddress address(tok);
                if (!address.IsValid())
                    throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address: "+tok);            
                if (setAddresses.count(address.ToString()))
                    throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, duplicate address: "+tok);
                const CTxDestination dest=address.Get();
                const CKeyID *lpKeyID=boost::get<CKeyID> (&dest);
                const CScriptID *lpScriptID=boost::get<CScriptID> (&dest);

                if( (lpKeyID != NULL) || ( lpScriptID != NULL))         
                {        
                    if(filter != ISMINE_NO)
                    {
                        isminetype fIsMine=IsMine(*pwalletMain,dest);
                        if (!(fIsMine & filter))
                        {
                            throw JSONRPCError(RPC_WALLET_ADDRESS_NOT_FOUND, "Non-wallet address : "+tok);                                                                    
                        }
                    }

                    setAddresses.insert(address.ToString());
                }
                else
                {
                    throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address (only pubkeyhash and scripthash addresses are supported) : "+tok);                                    
                }
            }
            else
            {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address, expected string");                                                        
            }
        }            
    }
    else
    {
        vector<CTxDestination> fromaddresses;        
        fromaddresses=ParseAddresses(param.get_str(),false,true);            
        BOOST_FOREACH(const CTxDestination& dest, fromaddresses)
        {
            isminetype fIsMine=IsMine(*pwalletMain,dest);
            if(filter != ISMINE_NO)
            {
                if (!(fIsMine & filter))
                {
                    CBitcoinAddress address(dest);
                    throw JSONRPCError(RPC_WALLET_ADDRESS_NOT_FOUND, "Non-wallet address: "+address.ToString());                                                                    
                }
            }
            setAddresses.insert(CBitcoinAddress(dest).ToString());
        }
    }

    return setAddresses;
}

bool CBitcoinAddressFromTxEntity(CBitcoinAddress &address,mc_TxEntity *lpEntity)
{
    bool is_address=false;
    CTxDestination dest=CTxDestination();
    address=CBitcoinAddress();
    if((lpEntity->m_EntityType & MC_TET_TYPE_MASK) == MC_TET_PUBKEY_ADDRESS)
    {            
        unsigned char *ptr=lpEntity->m_EntityID;
        dest=CKeyID(*(uint160*)ptr);
        is_address=true; 
    }
    if((lpEntity->m_EntityType & MC_TET_TYPE_MASK) == MC_TET_SCRIPT_ADDRESS)
    {
        unsigned char *ptr=lpEntity->m_EntityID;
        dest=CScriptID(*(uint160*)ptr);
        is_address=true;
    }
    if(is_address)
    {
        address=CBitcoinAddress(dest);
        return true;
    }    
    return false;
}

Object StreamItemEntry(const CWalletTx& wtx,int first_output,const unsigned char *stream_id, bool verbose, const char** given_key,const char ** given_publisher,int *output)
{
    Object entry;
    Array publishers;
    set<uint160> publishers_set;
    Array keys;    
    int stream_output;
    const unsigned char *ptr;
    unsigned char item_key[MC_ENT_MAX_ITEM_KEY_SIZE+1];
    int item_key_size;
    Value item_value;
    uint32_t format;
    Value format_item_value;
    string format_text_str;
    int start_from=first_output;
    bool key_found,publisher_found;
    
    stream_output=-1;
    if(output)
    {
        *output=(int)wtx.vout.size();
    }
    while( (stream_output < 0) && (start_from<(int)wtx.vout.size()) )
    {
        stream_output=-1;
        for (int j = start_from; j < (int)wtx.vout.size(); ++j)
        {
            if(stream_output < 0)
            {
                keys.clear();
                const CScript& script1 = wtx.vout[j].scriptPubKey;        
                CScript::const_iterator pc1 = script1.begin();

                mc_gState->m_TmpScript->Clear();
                mc_gState->m_TmpScript->SetScript((unsigned char*)(&pc1[0]),(size_t)(script1.end()-pc1),MC_SCR_TYPE_SCRIPTPUBKEY);

                if(mc_gState->m_TmpScript->IsOpReturnScript())                      
                {
                    if(mc_gState->m_TmpScript->GetNumElements()) // 2 OP_DROPs + OP_RETURN - item key
                    {
                        mc_gState->m_TmpScript->ExtractAndDeleteDataFormat(&format);

                        unsigned char short_txid[MC_AST_SHORT_TXID_SIZE];
                        mc_gState->m_TmpScript->SetElement(0);

                        if(mc_gState->m_TmpScript->GetEntity(short_txid) == 0)           
                        {
                            if(memcmp(short_txid,stream_id,MC_AST_SHORT_TXID_SIZE) == 0)
                            {
                                stream_output=j;
                                key_found=false;
                                for(int e=1;e<mc_gState->m_TmpScript->GetNumElements()-1;e++)
                                {
                                    mc_gState->m_TmpScript->SetElement(e);
                                                                                                // Should be spkk
                                    if(mc_gState->m_TmpScript->GetItemKey(item_key,&item_key_size))   // Item key
                                    {
                                        return entry;
                                    }                                            
                                    item_key[item_key_size]=0;
                                    if(given_key)
                                    {
                                        if(strcmp((char*)item_key,*given_key) == 0)
                                        {
                                            key_found=true;                                   
                                        }
                                    }
                                    keys.push_back(string(item_key,item_key+item_key_size));
                                }
                                
                                if(given_key)
                                {
                                    if(!key_found)
                                    {
                                        stream_output=-1;
                                    }
                                }

                                size_t elem_size;
                                const unsigned char *elem;

                                elem = mc_gState->m_TmpScript->GetData(mc_gState->m_TmpScript->GetNumElements()-1,&elem_size);
                                item_value=OpReturnEntry(elem,elem_size,wtx.GetHash(),j);
                                format_item_value=OpReturnFormatEntry(elem,elem_size,wtx.GetHash(),j,format,&format_text_str);
                            }
                        }
                    }                        
                }
            }
        }

        if(stream_output < 0)
        {
            return entry;
        }
        if(output)
        {
            *output=stream_output;
        }

        publishers.clear();
        publishers_set.clear();
        publisher_found=false;
        for (int i = 0; i < (int)wtx.vin.size(); ++i)
        {
            int op_addr_offset,op_addr_size,is_redeem_script,sighash_type;

            const CScript& script2 = wtx.vin[i].scriptSig;        
            CScript::const_iterator pc2 = script2.begin();

            ptr=mc_ExtractAddressFromInputScript((unsigned char*)(&pc2[0]),(int)(script2.end()-pc2),&op_addr_offset,&op_addr_size,&is_redeem_script,&sighash_type,0);
            if(ptr)
            {
                if( (sighash_type == SIGHASH_ALL) || ( (sighash_type == SIGHASH_SINGLE) && (i == stream_output) ) )
                {
                    uint160 publisher_hash=Hash160(ptr+op_addr_offset,ptr+op_addr_offset+op_addr_size);
                    if(publishers_set.count(publisher_hash) == 0)
                    {
                        publishers_set.insert(publisher_hash);
                        string publisher_str;
                        if(is_redeem_script)
                        {
                            publisher_str=CBitcoinAddress((CScriptID)publisher_hash).ToString();
                        }
                        else
                        {
                            publisher_str=CBitcoinAddress((CKeyID)publisher_hash).ToString();                    
                        }
                        if(given_publisher)
                        {
                            if(strcmp(publisher_str.c_str(),*given_publisher) == 0)
                            {
                                publisher_found=true;                                   
                            }
                        }
                        publishers.push_back(publisher_str);
                    }
                }
            }        
        }
        if(given_publisher)
        {
            if(!publisher_found)
            {
                stream_output=-1;
            }
        }
                
        if(stream_output < 0)
        {
            start_from++;
        }
    }

    if(stream_output < 0)
    {
        return entry;
    }

    
    entry.push_back(Pair("publishers", publishers));
    entry.push_back(Pair("keys", keys));
    if(mc_gState->m_Compatibility & MC_VCM_1_0)
    {
        entry.push_back(Pair("key", keys[0]));        
    }
    entry.push_back(Pair("data", format_item_value));        
    if(verbose)
    {
        WalletTxToJSON(wtx, entry, true, stream_output);
    }
    else
    {
        MinimalWalletTxToJSON(wtx, entry);
    }
    
    return entry;
}


Object TxOutEntry(const CTxOut& TxOutIn,int vout,const CTxIn& TxIn,uint256 hash,mc_Buffer *amounts,mc_Script *lpScript)
{
    bool fIsInput=false;
    bool fIsFound=false;
    if(TxIn.prevout.hash != 0)
    {
        fIsInput=true;
    }

    Object txout_entry;
    Array permissions;
    Array peroutputdata;
    isminetype fIsMine=ISMINE_NO;
    amounts->Clear();
//    int iad=-1;
    CTxOut txout;
    if(fIsInput)
    {
        if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)
        {
            int err;
            const CWalletTx& prev=pwalletTxsMain->GetWalletTx(TxIn.prevout.hash,NULL,&err);
            if(err == MC_ERR_NOERROR)
            {
                if (TxIn.prevout.n < prev.vout.size())
                {
                    txout=prev.vout[TxIn.prevout.n];
                    fIsFound=true;
                }
            }
        }
        else
        {
            map<uint256, CWalletTx>::const_iterator mi = pwalletMain->mapWallet.find(TxIn.prevout.hash);
            if (mi != pwalletMain->mapWallet.end())
            {
                const CWalletTx& prev = (*mi).second;
                if (TxIn.prevout.n < prev.vout.size())
                {
                    txout=prev.vout[TxIn.prevout.n];
                    fIsFound=true;
                }
            }
        }
    }
    else
    {
        txout=TxOutIn;
        fIsFound=true;        
    }
    
    if(fIsInput)
    {
        txout_entry.push_back(Pair("txid", TxIn.prevout.hash.ToString()));
        txout_entry.push_back(Pair("vout", (int)TxIn.prevout.n));
    }
    else
    {
        txout_entry.push_back(Pair("n", vout));        
    }
    
    if(fIsFound)
    {
        txnouttype typeRet;
        txout_entry.push_back(Pair("addresses", AddressEntries(txout,typeRet)));
        txout_entry.push_back(Pair("type", GetTxnOutputType(typeRet)));
        
        fIsMine=pwalletMain->IsMine(txout);
        string strFailReason;
        int required=0;
        if(ParseMultichainTxOutToBuffer(hash,txout,amounts,lpScript,NULL,&required,strFailReason))
        {
            if(required & (MC_PTP_ADMIN | MC_PTP_ACTIVATE) )
            {
                if(!fIsInput)
                {
                    permissions=PermissionEntries(txout,lpScript,false);
                }
            }
        }
    }
    else
    {
        if(fIsInput)
        {
            txnouttype typeRet;
            txout_entry.push_back(Pair("addresses", AddressEntries(TxIn,typeRet,lpScript)));
            txout_entry.push_back(Pair("type", GetTxnOutputType(typeRet)));
        }
    }
    
    txout_entry.push_back(Pair("ismine", (fIsMine & ISMINE_SPENDABLE) ? true : false));
    txout_entry.push_back(Pair("iswatchonly", (fIsMine & ISMINE_WATCH_ONLY) ? true : false));
            
    if(fIsFound)
    {
        txout_entry.push_back(Pair("amount", ValueFromAmount(txout.nValue)));
        Array assets;
        assets=AssetArrayFromAmounts(amounts,-1,hash,fIsInput ? 0 : 1);
        if( (assets.size() > 0) || (mc_gState->m_Compatibility & MC_VCM_1_0) )
        {
            txout_entry.push_back(Pair("assets", AssetArrayFromAmounts(amounts,-1,hash,fIsInput ? 0 : 1)));                
        }
        
        if(!fIsInput)
        {
            if( (permissions.size() > 0) || (mc_gState->m_Compatibility & MC_VCM_1_0) )
            {
                txout_entry.push_back(Pair("permissions", permissions));                            
            }
            
            peroutputdata=PerOutputDataEntries(txout,lpScript,hash,vout);
            if(peroutputdata.size())
            {
                txout_entry.push_back(Pair("data", peroutputdata));
            }            
        }
    }

    return txout_entry;
}

