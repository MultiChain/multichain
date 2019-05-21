// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2014-2016 The Bitcoin Core developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.


#include "rpc/rpcwallet.h"

bool IsTxOutMineAndInList(CWallet *lpWallet,const CTxOut& txout,const set<CTxDestination>* addresses,const isminefilter& filter)
{
    isminetype fIsMine=lpWallet->IsMine(txout);

    if(fIsMine & filter)
    {
        if(addresses)
        {
            CTxDestination addressRet;        
            if(ExtractDestination(txout.scriptPubKey, addressRet))
            {
                if(addresses->count(addressRet))
                {
                    return true;
                }
            }                                    
        }
        else
        {
            return true;
        }
    }
    
    return false;
}
   
int GetInputOffer(const CWalletTx& wtx, const set<CTxDestination>* addresses,const isminefilter& filter,CAmount& nAmount,mc_Buffer *amounts,mc_Script *lpScript,set<CTxDestination>* from_addresses,set<CTxDestination>* my_addresses)
{
    int nIsMineCount=0;
    amounts->Clear();
    nAmount=0;
    bool fIsMine;
    
    BOOST_FOREACH(const CTxIn& txin, wtx.vin)
    {
        CTxDestination addressFrom;
        bool take_it=false;
        fIsMine=false;
        
        CTxOut prevout;
        bool prevout_found=false;
                
        if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)
        {
            int err;
            const CWalletTx& prev=pwalletTxsMain->GetWalletTx(txin.prevout.hash,NULL,&err);
            if(err == MC_ERR_NOERROR)
            {
                if (txin.prevout.n < prev.vout.size())
                {
                    prevout=prev.vout[txin.prevout.n];
                    prevout_found=true;                    
                }
            }
        }
        else
        {
            map<uint256, CWalletTx>::const_iterator mi = pwalletMain->mapWallet.find(txin.prevout.hash);
            if (mi != pwalletMain->mapWallet.end())
            {
                const CWalletTx& prev = (*mi).second;
                if (txin.prevout.n < prev.vout.size())
                {
                    prevout=prev.vout[txin.prevout.n];
                    prevout_found=true;                    
                }
            }
        }
        
        if (prevout_found)
        {
            if(IsTxOutMineAndInList(pwalletMain,prevout,addresses,filter))
            {
                string strFailReason;
                if(ParseMultichainTxOutToBuffer(txin.prevout.hash,prevout,amounts,lpScript,NULL,NULL,strFailReason))
                {
                    nAmount+=prevout.nValue;
                    nIsMineCount++;
                    fIsMine=true;
                    if(ExtractDestination(prevout.scriptPubKey, addressFrom))
                    {
                        take_it=true;
                    }
                }
            }
        }                
        if( (from_addresses != NULL) && !wtx.IsCoinBase())
        {
            const CScript& script2 = txin.scriptSig;        
            CScript::const_iterator pc2 = script2.begin();

            lpScript->Clear();
            lpScript->SetScript((unsigned char*)(&pc2[0]),(size_t)(script2.end()-pc2),MC_SCR_TYPE_SCRIPTSIGRAW);

            size_t elem_size;
            const unsigned char *elem;
            if(lpScript->GetNumElements() >= 2)
            {
                elem = lpScript->GetData(0,&elem_size);
                if(elem_size == 1)
                {
                    elem = lpScript->GetData(lpScript->GetNumElements()-1,&elem_size);
                    if( (elem[0]>0x50) && (elem[0]<0x60))                             
                    {
                        uint160 hash=Hash160(elem,elem+elem_size);
                        addressFrom=CScriptID(hash);
                        take_it=true;                        
                    }
                }    
                else
                {
                    elem = lpScript->GetData(lpScript->GetNumElements()-1,&elem_size);        
                    uint160 hash=Hash160(elem,elem+elem_size);
                    addressFrom=CKeyID(hash);
                    take_it=true;
                }                
            }    

            if(take_it)
            {
                if( (addresses == NULL) || (addresses->find(addressFrom) == addresses->end()) )
                {
                    if(fIsMine)
                    {
                        if(my_addresses->find(addressFrom) == my_addresses->end())
                        {
                            my_addresses->insert(addressFrom);
                        }                                                                
                    }
                    else
                    {
                        if(from_addresses->find(addressFrom) == from_addresses->end())
                        {
                            from_addresses->insert(addressFrom);
                        }                    
                    }
                }
                else
                {
                    if(my_addresses->find(addressFrom) == my_addresses->end())
                    {
                        my_addresses->insert(addressFrom);
                    }                                        
                }
            }
        }
    }
    
    return nIsMineCount;
}

Object ListWalletTransactions(const CWalletTx& wtx, bool fLong, const isminefilter& filter,const set<CTxDestination>* addresses,mc_Buffer *amounts,mc_Script *lpScript)
{
    Object entry;
    int nIsFromMeCount,nIsToMeCount;
    uint32_t new_entity_type;
    CAmount nAmount;
    Array issue;
    Array assets;
    set<CTxDestination> my_addresses;
    set<CTxDestination> to_addresses;
    set<CTxDestination> from_addresses;
    set<uint256> streams_already_seen;
    Array aToAddresses;
    Array aFromAddresses;
    Array aMyAddresses;
    Array aPermissions;
    Array aMetaData;
    Array aItems;
    uint32_t format;
    unsigned char *chunk_hashes;
    int chunk_count;   
    int64_t total_chunk_size,out_size;
    uint32_t retrieve_status;
    Array aFormatMetaData;
    vector<Array> aFormatMetaDataPerOutput;

    nIsFromMeCount=GetInputOffer(wtx,addresses,filter,nAmount,amounts,lpScript,&from_addresses,&my_addresses);
    nIsToMeCount=0;
    
    nAmount=-nAmount;
    for(int i=0;i<amounts->GetCount();i++)
    {
        uint64_t quantity=mc_GetABQuantity(amounts->GetRow(i));
        quantity=-quantity;
        mc_SetABQuantity(amounts->GetRow(i),quantity);        
    }
    
    aFormatMetaDataPerOutput.resize(wtx.vout.size());
    
    new_entity_type=MC_ENT_TYPE_NONE;
    nIsToMeCount=0;
    for (int i = 0; i < (int)wtx.vout.size(); ++i)
    {
        const CTxOut& txout = wtx.vout[i];
        if(!txout.scriptPubKey.IsUnspendable())
        {
            string strFailReason;
            int required=0;
            
            txnouttype typeRet;
            int nRequiredRet;
            vector<CTxDestination> addressRets;
            bool fIsMine=false;
                
            if(IsTxOutMineAndInList(pwalletMain,txout,addresses,filter))
            {
                fIsMine=true;
                nIsToMeCount++;
                ParseMultichainTxOutToBuffer(wtx.GetHash(),txout,amounts,lpScript,NULL,&required,strFailReason);
//                CreateAssetBalanceList(txout,amounts,lpScript,&required);
                nAmount+=txout.nValue;
            }
           if(ExtractDestinations(txout.scriptPubKey,typeRet,addressRets,nRequiredRet))
            {
                if(addressRets.size() == 1)                                     // Details of multisig should not appear top-level myaddresses/addresses
                {
                    for (int j = 0; j < (int)addressRets.size(); j++)
                    {    
                        if( !fIsMine )
                        {
                            if(to_addresses.find(addressRets[j]) == to_addresses.end())
                            {
                                to_addresses.insert(addressRets[j]);
                            }
                        }
                        else
                        {
                            if(my_addresses.find(addressRets[j]) == my_addresses.end())
                            {
                                my_addresses.insert(addressRets[j]);
                            }                        
                        }
                    }
                }
            }
        }
        else
        {            
            const CScript& script2 = wtx.vout[i].scriptPubKey;        
            CScript::const_iterator pc2 = script2.begin();

            lpScript->Clear();
            lpScript->SetScript((unsigned char*)(&pc2[0]),(size_t)(script2.end()-pc2),MC_SCR_TYPE_SCRIPTPUBKEY);
            
//        	lpScript->ExtractAndDeleteDataFormat(&format);
            lpScript->ExtractAndDeleteDataFormat(&format,&chunk_hashes,&chunk_count,&total_chunk_size);
            
            const unsigned char *elem;

            if(lpScript->GetNumElements()<=1)
            {
                if(lpScript->GetNumElements()==1)
                {
//                    elem = lpScript->GetData(lpScript->GetNumElements()-1,&elem_size);
//                    aMetaData.push_back(OpReturnEntry(elem,elem_size,wtx.GetHash(),i));
                    retrieve_status = GetFormattedData(lpScript,&elem,&out_size,chunk_hashes,chunk_count,total_chunk_size);
                    Value metadata=OpReturnFormatEntry(elem,out_size,wtx.GetHash(),i,format,NULL,retrieve_status);
                    aFormatMetaData.push_back(metadata);
                    aFormatMetaDataPerOutput[i].push_back(metadata);
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
    //                    aMetaData.push_back(OpReturnEntry(elem,elem_size,wtx.GetHash(),i));
                        aFormatMetaData.push_back(OpReturnFormatEntry(elem,out_size,wtx.GetHash(),i,format,NULL,retrieve_status));
                    }
                }
                
                lpScript->SetElement(0);
                if(lpScript->GetNewEntityType(&new_entity_type))
                {
                    if(lpScript->GetNumElements()==3)
                    {
                        unsigned char short_txid[MC_AST_SHORT_TXID_SIZE];
                        if(lpScript->GetEntity(short_txid) == 0)
                        {
                            unsigned char details_script[MC_ENT_MAX_SCRIPT_SIZE];
                            int details_script_size,entity_update;
                            
                            lpScript->SetElement(1);;
                            lpScript->GetNewEntityType(&new_entity_type,&entity_update,details_script,&details_script_size);                        
                        }                        
                    }
                }
                
            }
        }
    }

    assets=AssetArrayFromAmounts(amounts,-1,wtx.GetHash(),0);
    
    uint64_t total=0;
    Array aIssueAddresses;
    set<CTxDestination> issue_addresses;
    Object asset_entry;
    
    for (int i = 0; i < (int)wtx.vout.size(); ++i)
    {
        const CTxOut& txout = wtx.vout[i];
        if(!txout.scriptPubKey.IsUnspendable())
        {
            string strFailReason;
            int required=0;
            amounts->Clear();
//            ParseMultichainTxOutToBuffer(wtx.GetHash(),txout,amounts,lpScript,NULL,&required,strFailReason);
            CreateAssetBalanceList(txout,amounts,lpScript,&required);
            if(required & (MC_PTP_ADMIN | MC_PTP_ACTIVATE))
            {
                Array this_permissions=PermissionEntries(txout,lpScript,true);
                for(int j=0;j<(int)this_permissions.size();j++)
                {
                    aPermissions.push_back(this_permissions[j]);
                }
            }
            if(required & MC_PTP_ISSUE)
            {
                total=1;
                uint256 txid=wtx.GetHash();
                asset_entry=AssetEntry((unsigned char*)&txid,-total,0x83);                        
                txnouttype typeRet;
                int nRequiredRet;
                vector<CTxDestination> addressRets;
                
                if(ExtractDestinations(txout.scriptPubKey,typeRet,addressRets,nRequiredRet))
                {
                    for (int j = 0; j < (int)addressRets.size(); j++)
                    {    
                        if(issue_addresses.find(addressRets[j]) == issue_addresses.end())
                        {
                            issue_addresses.insert(addressRets[j]);
                        }
                    }
                }

            }
        }
    }
    
    if(total)
    {
        BOOST_FOREACH(const CTxDestination& addr, issue_addresses)
        {
            aIssueAddresses.push_back(CBitcoinAddress(addr).ToString());
        }
        asset_entry.push_back(Pair("addresses", aIssueAddresses));
    }
    
    //aIssue.push_back(asset_entry);
    
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
    
    if( (nIsFromMeCount+nIsToMeCount) == 0 )
    {
        return entry;
    }
    
    
    
    
    BOOST_FOREACH(const CTxDestination& addr, my_addresses)
    {
        aMyAddresses.push_back(CBitcoinAddress(addr).ToString());
    }
    
    BOOST_FOREACH(const CTxDestination& addr, from_addresses)
    {
        aFromAddresses.push_back(CBitcoinAddress(addr).ToString());
    }
    
    BOOST_FOREACH(const CTxDestination& addr, to_addresses)
    {
//        aToAddresses.push_back(CBitcoinAddress(addr).ToString());
        if(from_addresses.find(addr) == from_addresses.end())
        {
            aFromAddresses.push_back(CBitcoinAddress(addr).ToString());            
        }
    }
        
    Object oBalance;
    oBalance.push_back(Pair("amount", ValueFromAmount(nAmount)));
    oBalance.push_back(Pair("assets", assets));
    entry.push_back(Pair("balance", oBalance));
    entry.push_back(Pair("myaddresses", aMyAddresses));
    entry.push_back(Pair("addresses", aFromAddresses));
//    entry.push_back(Pair("fromaddresses", aFromAddresses));
//    entry.push_back(Pair("toaddresses", aToAddresses));
    entry.push_back(Pair("permissions", aPermissions));
    if(new_entity_type == MC_ENT_TYPE_STREAM)
    {
        uint256 txid=wtx.GetHash();
        entry.push_back(Pair("create", StreamEntry((unsigned char*)&txid,0x05)));
    }
    if(asset_entry.size())
    {
        entry.push_back(Pair("issue", asset_entry));
    }
    else
    {
        if(new_entity_type == MC_ENT_TYPE_ASSET)
        {
            uint256 txid=wtx.GetHash();
            entry.push_back(Pair("issue", AssetEntry((unsigned char*)&txid,-total,0x83)));
        }        
    }
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
//    ret.push_back(entry);                                
    
    return entry;
}

Value listwallettransactions(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 4)                                             // MCHN
        throw runtime_error("Help message not found\n");

    int nCount = 10;
    if (params.size() > 0)
        nCount = params[0].get_int();
    int nFrom = 0;
    if (params.size() > 1)
        nFrom = params[1].get_int();
    isminefilter filter = ISMINE_SPENDABLE;
    if(params.size() > 2)
        if(params[2].get_bool())
            filter = filter | ISMINE_WATCH_ONLY;
    bool fLong=false;
    if(params.size() > 3)
        if(params[3].get_bool())
            fLong=true;

    if (nCount < 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Negative count");
    if (nFrom < 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Negative from");

    
    mc_Buffer *asset_amounts=mc_gState->m_TmpBuffers->m_RpcABBuffer1;
    asset_amounts->Clear();
    
    mc_Script *lpScript=mc_gState->m_TmpBuffers->m_RpcScript3;
    lpScript->Clear();    
    
    mc_Buffer *entity_rows=mc_gState->m_TmpBuffers->m_RpcEntityRows;
    entity_rows->Clear();
    
    Array ret;
    if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)
    {
        mc_TxEntity wallet_by_time;
        mc_TxEntityRow *lpEntTx;
        wallet_by_time.Zero();
        wallet_by_time.m_EntityType=MC_TET_TIMERECEIVED;
        if(filter & ISMINE_WATCH_ONLY)
        {
            wallet_by_time.m_EntityType |= MC_TET_WALLET_ALL;
        }
        else
        {
            wallet_by_time.m_EntityType |= MC_TET_WALLET_SPENDABLE;            
        }
        CheckWalletError(pwalletTxsMain->GetList(&wallet_by_time,-nFrom,nCount,entity_rows),wallet_by_time.m_EntityType,"");
        for(int i=0;i<entity_rows->GetCount();i++)
        {
            lpEntTx=(mc_TxEntityRow*)entity_rows->GetRow(i);
            uint256 hash;
            memcpy(&hash,lpEntTx->m_TxId,MC_TDB_TXID_SIZE);
            const CWalletTx& wtx=pwalletTxsMain->GetWalletTx(hash,NULL,NULL);
            Object entry=ListWalletTransactions(wtx,fLong,filter,NULL,asset_amounts,lpScript);
            if(entry.size())
            {
                ret.push_back(entry);                                
            }
        }
    }
    else
    {
        std::list<CAccountingEntry> acentries;
        CWallet::TxItems txOrdered = pwalletMain->OrderedTxItems(acentries, "*");
        // iterate backwards until we have nCount items to return:
        for (CWallet::TxItems::reverse_iterator it = txOrdered.rbegin(); it != txOrdered.rend(); ++it)
        {
            CWalletTx *const pwtx = (*it).second.first;
            if (pwtx != 0)
            {
                Object entry=ListWalletTransactions(*pwtx,fLong,filter,NULL,asset_amounts,lpScript);
                if(entry.size())
                {
                    ret.push_back(entry);                                
                }
            }

            if ((int)ret.size() >= (nCount+nFrom)) break;
        }
        // ret is newest to oldest

        if (nFrom > (int)ret.size())
            nFrom = ret.size();
        if ((nFrom + nCount) > (int)ret.size())
            nCount = ret.size() - nFrom;
        Array::iterator first = ret.begin();
        std::advance(first, nFrom);
        Array::iterator last = ret.begin();
        std::advance(last, nFrom+nCount);

        if (last != ret.end()) ret.erase(last, ret.end());
        if (first != ret.begin()) ret.erase(ret.begin(), first);

        std::reverse(ret.begin(), ret.end()); // Return oldest to newest        
    }

    
    return ret;
}

Value listaddresstransactions(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 5)                                   
        throw runtime_error("Help message not found\n");

    int nCount = 10;
    if (params.size() > 1)
        nCount = params[1].get_int();
    int nFrom = 0;
    if (params.size() > 2)
        nFrom = params[2].get_int();
    isminefilter filter = ISMINE_ALL;
    bool fLong=false;
    if(params.size() > 3)
        if(params[3].get_bool())
            fLong=true;
    
    if (nCount < 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Negative count");
    if (nFrom < 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Negative from");

    
    mc_Buffer *asset_amounts=mc_gState->m_TmpBuffers->m_RpcABBuffer1;
    asset_amounts->Clear();
    
    mc_Script *lpScript=mc_gState->m_TmpBuffers->m_RpcScript3;
    lpScript->Clear();    
    
    vector<CTxDestination> fromaddresses;        
    
    if (params.size() > 0)
    {
        fromaddresses=ParseAddresses(params[0].get_str(),false,true);        
    }
    
    if(fromaddresses.size() != 1)
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Single address should be specified");                        
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
    
    
    mc_Buffer *entity_rows=mc_gState->m_TmpBuffers->m_RpcEntityRows;
    entity_rows->Clear();

    Array ret;
    if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)
    {
        mc_TxEntity address_by_time;
        mc_TxEntityRow *lpEntTx;
        address_by_time.Zero();
        const CKeyID *lpKeyID=boost::get<CKeyID> (&fromaddresses[0]);
        const CScriptID *lpScriptID=boost::get<CScriptID> (&fromaddresses[0]);
        if(lpKeyID)
        {
            memcpy(address_by_time.m_EntityID,lpKeyID,MC_TDB_ENTITY_ID_SIZE);
            address_by_time.m_EntityType=MC_TET_PUBKEY_ADDRESS | MC_TET_TIMERECEIVED;
        }
        if(lpScriptID)
        {
            memcpy(address_by_time.m_EntityID,lpScriptID,MC_TDB_ENTITY_ID_SIZE);
            address_by_time.m_EntityType=MC_TET_SCRIPT_ADDRESS | MC_TET_TIMERECEIVED;
        }
        CheckWalletError(pwalletTxsMain->GetList(&address_by_time,-nFrom,nCount,entity_rows),address_by_time.m_EntityType,"");
        for(int i=0;i<entity_rows->GetCount();i++)
        {
            lpEntTx=(mc_TxEntityRow*)entity_rows->GetRow(i);
            uint256 hash;
            memcpy(&hash,lpEntTx->m_TxId,MC_TDB_TXID_SIZE);
            const CWalletTx& wtx=pwalletTxsMain->GetWalletTx(hash,NULL,NULL);
            Object entry=ListWalletTransactions(wtx,fLong,filter,lpFromAddresses,asset_amounts,lpScript);
            if(entry.size())
            {
                ret.push_back(entry);                                
            }
        }
    }
    else
    {
        std::list<CAccountingEntry> acentries;
        CWallet::TxItems txOrdered = pwalletMain->OrderedTxItems(acentries, "*");

        // iterate backwards until we have nCount items to return:
        for (CWallet::TxItems::reverse_iterator it = txOrdered.rbegin(); it != txOrdered.rend(); ++it)
        {
            CWalletTx *const pwtx = (*it).second.first;
            if (pwtx != 0)
            {
                Object entry=ListWalletTransactions(*pwtx,fLong,filter,lpFromAddresses,asset_amounts,lpScript);
                if(entry.size())
                {
                    ret.push_back(entry);                                
                }
            }

            if ((int)ret.size() >= (nCount+nFrom)) break;
        }
        // ret is newest to oldest

        if (nFrom > (int)ret.size())
            nFrom = ret.size();
        if ((nFrom + nCount) > (int)ret.size())
            nCount = ret.size() - nFrom;
        Array::iterator first = ret.begin();
        std::advance(first, nFrom);
        Array::iterator last = ret.begin();
        std::advance(last, nFrom+nCount);

        if (last != ret.end()) ret.erase(last, ret.end());
        if (first != ret.begin()) ret.erase(ret.begin(), first);

        std::reverse(ret.begin(), ret.end()); // Return oldest to newest
    }
    
    return ret;
}

Value getwallettransaction(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 3)                        // MCHN
        throw runtime_error("Help message not found\n");

    uint256 hash;
    hash.SetHex(params[0].get_str());

    isminefilter filter = ISMINE_SPENDABLE;
    if(params.size() > 1)
        if(params[1].get_bool())
            filter = filter | ISMINE_WATCH_ONLY;
    
    bool fLong=false;
    if(params.size() > 2)
        if(params[2].get_bool())
            fLong=true;

    
    if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)
    {
        if(pwalletTxsMain->FindWalletTx(hash,NULL))
            throw JSONRPCError(RPC_TX_NOT_FOUND, "Invalid or non-wallet transaction id");
    }
    else
    {
        if (!pwalletMain->mapWallet.count(hash))
            throw JSONRPCError(RPC_TX_NOT_FOUND, "Invalid or non-wallet transaction id");
    }

    mc_Buffer *asset_amounts=mc_gState->m_TmpBuffers->m_RpcABBuffer1;
    asset_amounts->Clear();
    
    mc_Script *lpScript=mc_gState->m_TmpBuffers->m_RpcScript3;
    lpScript->Clear();
    
    Object entry;
    
    if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)
    {
        const CWalletTx& wtx=pwalletTxsMain->GetWalletTx(hash,NULL,NULL);
        entry=ListWalletTransactions(wtx,fLong,filter,NULL,asset_amounts,lpScript);
    }
    else
    {
        const CWalletTx& wtx = pwalletMain->mapWallet[hash];
        entry=ListWalletTransactions(wtx,fLong,filter,NULL,asset_amounts,lpScript);
    }
        
    if(entry.size() == 0)
    {
        throw JSONRPCError(RPC_TX_NOT_FOUND, "Wallet addresses with specified criteria are not involved in transaction");                                
    }
    
    
    return entry;
}

Value getaddresstransaction(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 3)                        // MCHN
        throw runtime_error("Help message not found\n");

    uint256 hash;
    hash.SetHex(params[1].get_str());

    isminefilter filter = ISMINE_ALL;
    
    bool fLong=false;
    if(params.size() > 2)
        if(params[2].get_bool())
            fLong=true;

    if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)
    {
        if(pwalletTxsMain->FindWalletTx(hash,NULL))
            throw JSONRPCError(RPC_TX_NOT_FOUND, "Invalid or non-wallet transaction id");
    }
    else
    {
        if (!pwalletMain->mapWallet.count(hash))
            throw JSONRPCError(RPC_TX_NOT_FOUND, "Invalid or non-wallet transaction id");
    }

    
    mc_Buffer *asset_amounts=mc_gState->m_TmpBuffers->m_RpcABBuffer1;
    asset_amounts->Clear();
    
    mc_Script *lpScript=mc_gState->m_TmpBuffers->m_RpcScript3;
    lpScript->Clear();    
    
    vector<CTxDestination> fromaddresses;        
    
    if (params.size() > 0)
    {
        fromaddresses=ParseAddresses(params[0].get_str(),false,true);        
    }
    
    if(fromaddresses.size() != 1)
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Single address should be specified");                        
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
    
    Object entry;
    if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)
    {
        const CWalletTx& wtx=pwalletTxsMain->GetWalletTx(hash,NULL,NULL);
        entry=ListWalletTransactions(wtx,fLong,filter,lpFromAddresses,asset_amounts,lpScript);
    }
    else
    {
        const CWalletTx& wtx = pwalletMain->mapWallet[hash];
        entry=ListWalletTransactions(wtx,fLong,filter,lpFromAddresses,asset_amounts,lpScript);
    }
    
    if(entry.size() == 0)
    {
        throw JSONRPCError(RPC_TX_NOT_FOUND, "This transaction was not found for this address");                                
    }
    
    return entry;
}

