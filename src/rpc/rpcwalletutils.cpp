// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2014-2016 The Bitcoin Core developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.


#include "rpc/rpcwallet.h"
int VerifyNewTxForStreamFilters(const CTransaction& tx,std::string &strResult,mc_MultiChainFilter **lppFilter,int *applied);            
string OpReturnFormatToText(int format);

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
    
        
    mc_MultiChainFilter* lpFilter;
    int applied=0;
    string filter_error="";

    if(VerifyNewTxForStreamFilters(wtxNew,filter_error,&lpFilter,&applied) == MC_ERR_NOT_ALLOWED)
    {
        throw JSONRPCError(RPC_NOT_ALLOWED, "Transaction didn't pass stream filter " + lpFilter->m_FilterCaption + ": " + filter_error);                            
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

Object StreamItemEntry1(const CWalletTx& wtx,int first_output,const unsigned char *stream_id, bool verbose, const char** given_key,const char ** given_publisher,int *output)
{
    Object entry;
    Array publishers;
    set<uint160> publishers_set;
    Array keys;    
    int stream_output;
    const unsigned char *ptr;
    unsigned char item_key[MC_ENT_MAX_ITEM_KEY_SIZE+1];
    int item_key_size;
//    Value item_value;
    uint32_t format;
    unsigned char *chunk_hashes;
    int chunk_count;   
    int64_t total_chunk_size,out_size;
    uint32_t retrieve_status=0;
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
                        mc_gState->m_TmpScript->ExtractAndDeleteDataFormat(&format,&chunk_hashes,&chunk_count,&total_chunk_size);
//                        chunk_hashes=NULL;
//                        mc_gState->m_TmpScript->ExtractAndDeleteDataFormat(&format);

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

                                const unsigned char *elem;

//                                elem = mc_gState->m_TmpScript->GetData(mc_gState->m_TmpScript->GetNumElements()-1,&elem_size);
                                retrieve_status = GetFormattedData(mc_gState->m_TmpScript,&elem,&out_size,chunk_hashes,chunk_count,total_chunk_size);
//                                item_value=OpReturnEntry(elem,elem_size,wtx.GetHash(),j);
                                format_item_value=OpReturnFormatEntry(elem,out_size,wtx.GetHash(),j,format,&format_text_str,retrieve_status);
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
    entry.push_back(Pair("offchain", (retrieve_status & MC_OST_STORAGE_MASK) == MC_OST_OFF_CHAIN));        
    if( ( retrieve_status & MC_OST_CONTROL_NO_DATA ) == 0)
    {
        entry.push_back(Pair("available", AvailableFromStatus(retrieve_status)));        
        if(retrieve_status & MC_OST_ERROR_MASK)
        {
            string error_str;
            int errorCode;
            error_str=OffChainError(retrieve_status,&errorCode);
            entry.push_back(Pair("error", error_str));        
        }
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

Object StreamItemEntry(const CWalletTx& wtx,int first_output,const unsigned char *stream_id, bool verbose, vector<mc_QueryCondition> *given_conditions,int *output)
{
    Object entry;
    Array publishers;
    set<uint160> publishers_set;
    Array keys;    
    int stream_output;
    const unsigned char *ptr;
    unsigned char item_key[MC_ENT_MAX_ITEM_KEY_SIZE+1];
    int item_key_size;
//    Value item_value;
    uint32_t format;
    unsigned char *chunk_hashes;
    int chunk_count;   
    int64_t total_chunk_size,out_size;
    uint32_t retrieve_status=0;
    Value format_item_value;
    string format_text_str;
    int start_from=first_output;
    
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
                if(given_conditions)
                {
                    for(int c=0;c<(int)(*given_conditions).size();c++)
                    {
                        (*given_conditions)[c].m_TmpMatch=false;
                    }
                }
                keys.clear();
                const CScript& script1 = wtx.vout[j].scriptPubKey;        
                CScript::const_iterator pc1 = script1.begin();

                mc_gState->m_TmpScript->Clear();
                mc_gState->m_TmpScript->SetScript((unsigned char*)(&pc1[0]),(size_t)(script1.end()-pc1),MC_SCR_TYPE_SCRIPTPUBKEY);

                if(mc_gState->m_TmpScript->IsOpReturnScript())                      
                {
                    if(mc_gState->m_TmpScript->GetNumElements()) // 2 OP_DROPs + OP_RETURN - item key
                    {
                        mc_gState->m_TmpScript->ExtractAndDeleteDataFormat(&format,&chunk_hashes,&chunk_count,&total_chunk_size);
//                        chunk_hashes=NULL;
//                        mc_gState->m_TmpScript->ExtractAndDeleteDataFormat(&format);

                        unsigned char short_txid[MC_AST_SHORT_TXID_SIZE];
                        mc_gState->m_TmpScript->SetElement(0);

                        if(mc_gState->m_TmpScript->GetEntity(short_txid) == 0)           
                        {
                            if(memcmp(short_txid,stream_id,MC_AST_SHORT_TXID_SIZE) == 0)
                            {
                                stream_output=j;
                                for(int e=1;e<mc_gState->m_TmpScript->GetNumElements()-1;e++)
                                {
                                    mc_gState->m_TmpScript->SetElement(e);
                                                                                                // Should be spkk
                                    if(mc_gState->m_TmpScript->GetItemKey(item_key,&item_key_size))   // Item key
                                    {
                                        return entry;
                                    }                                            
                                    item_key[item_key_size]=0;
                                    
                                    if(given_conditions)
                                    {
                                        for(int c=0;c<(int)(*given_conditions).size();c++)
                                        {
                                            if((*given_conditions)[c].m_Type == MC_QCT_KEY)
                                            {
                                                if(strcmp((char*)item_key,(*given_conditions)[c].m_Value.c_str()) == 0)
                                                {
                                                    (*given_conditions)[c].m_TmpMatch=true;
                                                }                                                
                                            }
                                        }
                                    }
                                    
                                    keys.push_back(string(item_key,item_key+item_key_size));
                                }
                                
                                if(given_conditions)
                                {
                                    for(int c=0;c<(int)(*given_conditions).size();c++)
                                    {
                                        if((*given_conditions)[c].m_Type == MC_QCT_KEY)
                                        {
                                            if(!(*given_conditions)[c].m_TmpMatch)
                                            {
                                                stream_output=-1;                                                
                                            }
                                        }
                                    }
                                }

                                const unsigned char *elem;

//                                elem = mc_gState->m_TmpScript->GetData(mc_gState->m_TmpScript->GetNumElements()-1,&elem_size);
                                retrieve_status = GetFormattedData(mc_gState->m_TmpScript,&elem,&out_size,chunk_hashes,chunk_count,total_chunk_size);
//                                item_value=OpReturnEntry(elem,elem_size,wtx.GetHash(),j);
                                format_item_value=OpReturnFormatEntry(elem,out_size,wtx.GetHash(),j,format,&format_text_str,retrieve_status);
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
                        
                        if(given_conditions)
                        {
                            for(int c=0;c<(int)(*given_conditions).size();c++)
                            {
                                if((*given_conditions)[c].m_Type == MC_QCT_PUBLISHER)
                                {
                                    if(strcmp(publisher_str.c_str(),(*given_conditions)[c].m_Value.c_str()) == 0)
                                    {
                                        (*given_conditions)[c].m_TmpMatch=true;
                                    }                                                
                                }
                            }
                        }
                        
                        publishers.push_back(publisher_str);
                    }
                }
            }        
        }

        if(given_conditions)
        {
            for(int c=0;c<(int)(*given_conditions).size();c++)
            {
                if((*given_conditions)[c].m_Type == MC_QCT_PUBLISHER)
                {
                    if(!(*given_conditions)[c].m_TmpMatch)
                    {
                        stream_output=-1;                                                
                    }
                }
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
    entry.push_back(Pair("offchain", (retrieve_status & MC_OST_STORAGE_MASK) == MC_OST_OFF_CHAIN));        
    string full_error="";
    if( ( retrieve_status & MC_OST_CONTROL_NO_DATA ) == 0)
    {        
        if(retrieve_status & MC_OST_ERROR_MASK)
        {
            int errorCode;
            full_error=OffChainError(retrieve_status,&errorCode);
            entry.push_back(Pair("available", false));        
            entry.push_back(Pair("error", full_error));        
        }
        else
        {
//            if( AvailableFromStatus(retrieve_status) && ((retrieve_status & MC_OST_STORAGE_MASK) == MC_OST_OFF_CHAIN ))
            if( AvailableFromStatus(retrieve_status) )
            {
                mc_MultiChainFilter* lpFilter;
                int applied=0;
                string filter_error="";
                mc_TxDefRow txdef;
                if(pwalletTxsMain->FindWalletTx(wtx.GetHash(),&txdef))
                {
                    full_error="Error while retreiving tx from the wallet";
                }
                else
                {        
                    int filter_block=-1;
                    int filter_offset=0;
                    if( (txdef.m_Block >= 0) && (txdef.m_Block <= chainActive.Height()))
                    {
                        filter_block=txdef.m_Block;
                        filter_offset=txdef.m_BlockTxOffset;
                    }
                    else
                    {
                        filter_offset=-1;                                    
                    }
                    
                    if(pMultiChainFilterEngine->RunStreamFilters(wtx,stream_output,(unsigned char *)stream_id,filter_block,filter_offset, 
                            filter_error,&lpFilter,&applied) != MC_ERR_NOERROR)
                    {
                        full_error="Error while running stream filters";
                    }
                    else
                    {
                        if(filter_error.size())
                        {
                            full_error=strprintf("Stream item did not pass filter %s: %s",lpFilter->m_FilterCaption.c_str(),filter_error.c_str());
                        }
                    }                                
                }
                if(full_error.size())
                {    
                    entry.push_back(Pair("available", false));                            
                    entry.push_back(Pair("error", full_error));  
                    Object metadata_object;
                    metadata_object.push_back(Pair("txid", wtx.GetHash().ToString()));
                    metadata_object.push_back(Pair("vout", stream_output));
                    metadata_object.push_back(Pair("format", OpReturnFormatToText(format)));
                    metadata_object.push_back(Pair("size", out_size));
                    format_item_value=metadata_object;
                }                
                else
                {
                    entry.push_back(Pair("available", AvailableFromStatus(retrieve_status)));                            
                }
            }
            else
            {
                entry.push_back(Pair("available", AvailableFromStatus(retrieve_status)));                        
            }
        }
    }
    
    if(full_error.size())
    {    
        entry.push_back(Pair("data", Value::null));        
    }
    else
    {    
        entry.push_back(Pair("data", format_item_value));        
    }
    
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

void AppendOffChainFormatData(uint32_t data_format,
                              uint32_t out_options,
                              mc_Script *lpDetailsScript,
                              vector<unsigned char>& vValue,
                              vector<uint256>* vChunkHashes,
                              int *errorCode,
                              string *strError)
{
    if((mc_gState->m_WalletMode & MC_WMD_TXS) == 0)
    {
        *strError="Offchain data is not supported with this wallet version. To get this functionality, run \"multichaind -walletdbversion=2 -rescan\"";
        *errorCode=RPC_NOT_SUPPORTED;
        return;
    }   
    mc_ChunkDBRow chunk_def;
    lpDetailsScript->Clear();
    
    int chunk_count;
    int tail_size,size,max_chunk_size;
    int64_t total_size;
    int fHan;
    int err;
    unsigned char *ptr;
    uint256 hash;
    mc_TxEntity entity;
    unsigned char salt[MC_CDB_CHUNK_SALT_SIZE];
    uint32_t salt_size;
        
    salt_size=0;
    if(mc_gState->m_Features->SaltedChunks())
    {
        if(out_options & MC_RFD_OPTION_SALTED)
        {
            salt_size=16;
        }
    }
    
    entity.Zero();
    entity.m_EntityType=MC_TET_AUTHOR;    

    if(out_options & MC_RFD_OPTION_OFFCHAIN)
    {
        if(salt_size)
        {
            *errorCode=RPC_NOT_ALLOWED;
            *strError="chunks option is not allowed for salted items";
            return;             
        }
        chunk_count=(int)vValue.size()/MC_CDB_CHUNK_HASH_SIZE;
        if(chunk_count > MAX_CHUNK_COUNT)
        {
            *strError="Too many chunks in the script";
            return; 
        }

        lpDetailsScript->SetChunkDefHeader(data_format,chunk_count,0);
        for(int i=0;i<chunk_count;i++)
        {
            err=pwalletTxsMain->m_ChunkDB->GetChunkDef(&chunk_def,(unsigned char*)&vValue[i*MC_CDB_CHUNK_HASH_SIZE],&entity,NULL,-1);
            if(err)
            {
                *strError="Chunk not found in this wallet";
                return; 
            }
            lpDetailsScript->SetChunkDefHash((unsigned char*)&vValue[i*MC_CDB_CHUNK_HASH_SIZE],chunk_def.m_Size);
            if(vChunkHashes)
            {
                vChunkHashes->push_back(*(uint256*)&vValue[i*MC_CDB_CHUNK_HASH_SIZE]);
            }
        }
    }
    else
    {
        chunk_count=0;
        tail_size=0;
        ptr=NULL;
        fHan=-1;
        if(vValue.size())
        {
            if(out_options & MC_RFD_OPTION_CACHE)
            {            
                fHan=mc_BinaryCacheFile((char*)&vValue[0],0);
                if(fHan <= 0)
                {
                    *strError="Binary cache item with this identifier not found";
                    return;                     
                }
                total_size=lseek64(fHan,0,SEEK_END);
                if(lseek64(fHan,0,SEEK_SET) != 0)
                {
                    *strError="Cannot read binary cache item";
                    close(fHan);
                    return;                                         
                }
                if(total_size)
                {
                    chunk_count=(int)((total_size-1)/MAX_CHUNK_SIZE)+1;                    
                }
                tail_size=(int)(total_size-(int64_t)(chunk_count-1)*MAX_CHUNK_SIZE);            
            }
            else
            {
                chunk_count=((int)vValue.size()-1)/MAX_CHUNK_SIZE+1;            
                tail_size=(int)vValue.size()-(chunk_count-1)*MAX_CHUNK_SIZE;            
            }
            max_chunk_size=tail_size;
            if(chunk_count > 1)
            {
                max_chunk_size=MAX_CHUNK_SIZE;
            }
            mc_gState->m_TmpBuffers->m_RpcChunkScript1->Clear();
            mc_gState->m_TmpBuffers->m_RpcChunkScript1->Resize(max_chunk_size,1);
            ptr=mc_gState->m_TmpBuffers->m_RpcChunkScript1->m_lpData;
            
            lpDetailsScript->SetChunkDefHeader(data_format,chunk_count,salt_size);
            for(int i=0;i<chunk_count;i++)
            {
                size=MAX_CHUNK_SIZE;
                if(i == chunk_count-1)
                {
                    size=tail_size;
                }
                if(fHan > 0)
                {
                    if(read(fHan,ptr,size) != size)
                    {
                        *errorCode=RPC_INTERNAL_ERROR;
                        *strError="Cannot read binary cache item";
                        close(fHan);
                        return;                     
                    }
                }
                else
                {
                    ptr=(unsigned char*)&vValue[i*MAX_CHUNK_SIZE];
                }
                
//                mc_gState->m_TmpBuffers->m_RpcHasher1->DoubleHash(ptr,size,&hash);
                if(salt_size)
                {
                    GetRandBytes(salt, salt_size);
                }
                
                mc_gState->m_TmpBuffers->m_RpcHasher1->DoubleHash(salt,salt_size,ptr,size,&hash);
                
                err=pwalletTxsMain->m_ChunkDB->AddChunk((unsigned char*)&hash,&entity,NULL,-1,ptr,NULL,salt,size,0,salt_size,0);   
                if(err)
                {
                    switch(err)
                    {
                        case MC_ERR_FOUND:
                            break;
                        default:
                            *strError="Internal error: couldn't store chunk";
                            if(fHan > 0)
                            {
                                close(fHan);
                            }
                            return; 
                    }
                }
    
                lpDetailsScript->SetChunkDefHash((unsigned char*)&hash,size);
                if(vChunkHashes)
                {
                    vChunkHashes->push_back(*(uint256*)&hash);
                }
            }
        }
        else
        {
            lpDetailsScript->SetChunkDefHeader(data_format,0,salt_size);            
        }
        if(fHan > 0)
        {
            close(fHan);
            fHan=0;
        }
    }
    
//            err=pwalletTxsMain->m_ChunkDB->AddChunk((unsigned char*)&hash,&entity,NULL,-1,(unsigned char*)&vValue[0],NULL,(int)vValue.size(),0,0);
}                             

