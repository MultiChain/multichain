// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2014-2016 The Bitcoin Core developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "structs/amount.h"
#include "structs/base58.h"
#include "utils/core_io.h"
#include "rpc/rpcserver.h"
#include "core/init.h"
#include "net/net.h"
#include "net/netbase.h"
#include "utils/timedata.h"
#include "utils/util.h"
#include "utils/utilmoneystr.h"
#include "wallet/wallet.h"
#include "wallet/walletdb.h"
#include "rpc/rpcwallet.h"
#include "community/community.h"

#include <stdint.h>

#include <boost/assign/list_of.hpp>
#include "json/json_spirit_utils.h"
#include "json/json_spirit_value.h"

using namespace std;
using namespace boost;
using namespace boost::assign;
using namespace json_spirit;

/* MCHN START */
#include "script/sign.h"
#include "multichain/multichain.h"
#include "wallet/wallettxs.h"

#include "rpc/rpcutils.h"

/* MCHN END */


int64_t nWalletUnlockTime;
static CCriticalSection cs_nWalletUnlockTime;



std::string HelpRequiringPassphrase()
{
    return pwalletMain && pwalletMain->IsCrypted()
        ? "\nRequires wallet passphrase to be set with walletpassphrase call.\n"
        : "";
}

void EnsureWalletIsUnlocked()
{
    if (pwalletMain->IsLocked())
        throw JSONRPCError(RPC_WALLET_UNLOCK_NEEDED, "Error: Please enter the wallet passphrase with walletpassphrase first.");
}

string AccountFromValue(const Value& value)
{
    string strAccount = value.get_str();
    if (strAccount == "*")
        throw JSONRPCError(RPC_WALLET_INVALID_ACCOUNT_NAME, "Invalid account name");
    return strAccount;
}

Value getnewaddress(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)                                             // MCHN
        throw runtime_error("Help message not found\n");

    // Parse the account first so we don't generate a key if there's an error
    string strAccount;
    if (params.size() > 0)
    {
        if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)
        {
            throw JSONRPCError(RPC_NOT_SUPPORTED, "Accounts are not supported with scalable wallet - if you need accounts, run multichaind -walletdbversion=1 -rescan, but the wallet will perform worse");        
        }            
        strAccount = AccountFromValue(params[0]);
    }

    if (!pwalletMain->IsLocked())
        pwalletMain->TopUpKeyPool();

    // Generate a new key that is added to wallet
    CPubKey newKey;
    if (!pwalletMain->GetKeyFromPool(newKey))
        throw JSONRPCError(RPC_WALLET_KEYPOOL_RAN_OUT, "Error: Keypool ran out, please call keypoolrefill first");
    CKeyID keyID = newKey.GetID();

    pwalletMain->SetAddressBook(keyID, strAccount, "receive");

    if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)
    {
        mc_TxEntity entity;
        entity.Zero();
        memcpy(entity.m_EntityID,&keyID,MC_TDB_ENTITY_ID_SIZE);
        entity.m_EntityType=MC_TET_PUBKEY_ADDRESS | MC_TET_CHAINPOS;
        pwalletTxsMain->AddEntity(&entity,0);
        entity.m_EntityType=MC_TET_PUBKEY_ADDRESS | MC_TET_TIMERECEIVED;
        pwalletTxsMain->AddEntity(&entity,0);
    }
    
    return CBitcoinAddress(keyID).ToString();
}


CBitcoinAddress GetAccountAddress(string strAccount, bool bForceNew=false)
{
    CWalletDB walletdb(pwalletMain->strWalletFile);

    CAccount account;
    walletdb.ReadAccount(strAccount, account);

    bool bKeyUsed = false;

    // Check if the current key has been used
    if (account.vchPubKey.IsValid())
    {
        CScript scriptPubKey = GetScriptForDestination(account.vchPubKey.GetID());
        for (map<uint256, CWalletTx>::iterator it = pwalletMain->mapWallet.begin();
             it != pwalletMain->mapWallet.end() && account.vchPubKey.IsValid();
             ++it)
        {
            const CWalletTx& wtx = (*it).second;
            BOOST_FOREACH(const CTxOut& txout, wtx.vout)
                if (txout.scriptPubKey == scriptPubKey)
                    bKeyUsed = true;
        }
    }

    // Generate a new key
    if (!account.vchPubKey.IsValid() || bForceNew || bKeyUsed)
    {
        if (!pwalletMain->GetKeyFromPool(account.vchPubKey))
            throw JSONRPCError(RPC_WALLET_KEYPOOL_RAN_OUT, "Error: Keypool ran out, please call keypoolrefill first");

        pwalletMain->SetAddressBook(account.vchPubKey.GetID(), strAccount, "receive");
        walletdb.WriteAccount(strAccount, account);
    }

    return CBitcoinAddress(account.vchPubKey.GetID());
}

Value getaccountaddress(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)                                            // MCHN
        throw runtime_error("Help message not found\n");

    if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "Accounts are not supported with scalable wallet - if you need accounts, run multichaind -walletdbversion=1 -rescan, but the wallet will perform worse");        
    }            
    
    // Parse the account first so we don't generate a key if there's an error
    string strAccount = AccountFromValue(params[0]);

    Value ret;

    ret = GetAccountAddress(strAccount).ToString();

    return ret;
}


Value getrawchangeaddress(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)                                             // MCHN
        throw runtime_error("Help message not found\n");

    if (!pwalletMain->IsLocked())
        pwalletMain->TopUpKeyPool();

    CReserveKey reservekey(pwalletMain);
    CPubKey vchPubKey;
    if (!reservekey.GetReservedKey(vchPubKey))
        throw JSONRPCError(RPC_WALLET_KEYPOOL_RAN_OUT, "Error: Keypool ran out, please call keypoolrefill first");

    reservekey.KeepKey();

    CKeyID keyID = vchPubKey.GetID();

    return CBitcoinAddress(keyID).ToString();
}


Value setaccount(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)                        // MCHN
        throw runtime_error("Help message not found\n");

    if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "Accounts are not supported with scalable wallet - if you need accounts, run multichaind -walletdbversion=1 -rescan, but the wallet will perform worse");        
    }            
    
    CBitcoinAddress address(params[0].get_str());
    if (!address.IsValid())                                                     // MCHN
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");


    string strAccount;
    if (params.size() > 1)
        strAccount = AccountFromValue(params[1]);

    // Only add the account if the address is yours.
    if (IsMine(*pwalletMain, address.Get()))
    {
        // Detect when changing the account of an address that is the 'unused current key' of another account:
        if (pwalletMain->mapAddressBook.count(address.Get()))
        {
            string strOldAccount = pwalletMain->mapAddressBook[address.Get()].name;
            if (address == GetAccountAddress(strOldAccount))
                GetAccountAddress(strOldAccount, true);
        }
        pwalletMain->SetAddressBook(address.Get(), strAccount, "receive");
    }
    else
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "setaccount can only be used with own address");

    return Value::null;
}


Value getaccount(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)                                            // MCHN
        throw runtime_error("Help message not found\n");

    if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "Accounts are not supported with scalable wallet - if you need accounts, run multichaind -walletdbversion=1 -rescan, but the wallet will perform worse");        
    }            
    
    CBitcoinAddress address(params[0].get_str());
    if (!address.IsValid())                                                     // MCHN
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");

    string strAccount;
    map<CTxDestination, CAddressBookData>::iterator mi = pwalletMain->mapAddressBook.find(address.Get());
    if (mi != pwalletMain->mapAddressBook.end() && !(*mi).second.name.empty())
        strAccount = (*mi).second.name;
    return strAccount;
}


Value getaddressesbyaccount(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)                                            // MCHN
        throw runtime_error("Help message not found\n");

    string strAccount = AccountFromValue(params[0]);

    if(strAccount != "") 
    {
        if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)
        {
            throw JSONRPCError(RPC_NOT_SUPPORTED, "Accounts are not supported with scalable wallet - if you need accounts, run multichaind -walletdbversion=1 -rescan, but the wallet will perform worse");        
        }            
    }
    
    // Find all addresses that have the given account
    Array ret;
    BOOST_FOREACH(const PAIRTYPE(CBitcoinAddress, CAddressBookData)& item, pwalletMain->mapAddressBook)
    {
        const CBitcoinAddress& address = item.first;
        const string& strName = item.second.name;
        if(item.second.purpose != "license")
        {
            if (strName == strAccount)
                ret.push_back(address.ToString());
        }
    }
    return ret;
}


/* MCHN START */    

Value resendwallettransactions(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 0)
        throw runtime_error("Help message not found\n");
    pwalletMain->ResendWalletTransactions(true);
    return "Wallet transactions resent";
}



/* MCHN END */    

void SendMoney(const CTxDestination &address, CAmount nValue, CWalletTx& wtxNew,mc_Script *dropscript,mc_Script *opreturnscript) // MCHN
{
    // Check amount
/* MCHN START */    
//    if (nValue <= 0)
    if (nValue < 0)
/* MCHN START */    
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid amount");

    
    if (nValue > pwalletMain->GetBalance())
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, "Insufficient funds");
   
    string strError;
    if (pwalletMain->IsLocked())
    {
        strError = "Error: Wallet locked, unable to create transaction!";
        LogPrintf("SendMoney() : %s", strError);
        throw JSONRPCError(RPC_WALLET_UNLOCK_NEEDED, strError);
    }

    // Parse Bitcoin address

/* MCHN START */
// Pay-To_PubKey testing code
    
/*    
    CKey key;
    
    CKeyID lpKeyID=boost::get<CKeyID> (address);
    if(!pwalletMain->GetKey(lpKeyID, key))
    {
        strError = "Error: Cannot find private key!";
        LogPrintf("SendMoney() : %s", strError);
        throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }
    
    CPubKey pkey=key.GetPubKey();            
    
    CScript scriptPubKey;
    scriptPubKey << ToByteVector(pkey) << OP_CHECKSIG;
 */ 
/* MCHN END */    
    
    CScript scriptPubKey = GetScriptForDestination(address);

/* MCHN START */    
    size_t elem_size;
    const unsigned char *elem;
    
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
    
    CScript scriptOpReturn=CScript();
    
    if(opreturnscript)
    {
        elem = opreturnscript->GetData(0,&elem_size);
        if(elem_size > 0)
        {
            scriptOpReturn << OP_RETURN << vector<unsigned char>(elem, elem + elem_size);
        }
    }
/* MCHN END */    
    
   
    // Create and send the transaction
    CReserveKey reservekey(pwalletMain);
    CAmount nFeeRequired;
    if (!pwalletMain->CreateTransaction(scriptPubKey, nValue, scriptOpReturn, wtxNew, reservekey, nFeeRequired, strError))
    {
        if (nValue + nFeeRequired > pwalletMain->GetBalance())
            strError = strprintf("Error: This transaction requires a transaction fee of at least %s because of its amount, complexity, or use of recently received funds!", FormatMoney(nFeeRequired));
        LogPrintf("SendMoney() : %s\n", strError);
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, strError);
    }
/* MCHN START */
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
    
/* MCHN END */
}

/* MCHN START */





Value listaddresses(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 5)
        throw runtime_error("Help message not found\n");

    if((mc_gState->m_WalletMode & MC_WMD_TXS) == 0)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "API is not supported with this wallet version. To get this functionality, run \"multichaind -walletdbversion=2 -rescan\" ");        
    }   

    Array result;
    int entity_count,address_count;
    mc_TxEntityStat *lpEntity;
    
    entity_count=pwalletTxsMain->GetEntityListCount();
    
    set<string> setAddresses;
    set<string> setAddressesWithBalances;
    if(params.size() > 0)
    {
        if( (params[0].type() != str_type) || (params[0].get_str() != "*") )
        {        
            setAddresses=ParseAddresses(params[0],ISMINE_NO);
            if(setAddresses.size() == 0)
            {
                return result;
            }
        }
    }
    
    address_count=0;
    for(int i=0;i<entity_count;i++)
    {
        lpEntity=pwalletTxsMain->GetEntity(i);
        if( ((lpEntity->m_Entity.m_EntityType == (MC_TET_PUBKEY_ADDRESS | MC_TET_CHAINPOS)) || 
            (lpEntity->m_Entity.m_EntityType == (MC_TET_SCRIPT_ADDRESS | MC_TET_CHAINPOS))) &&
            ((lpEntity->m_Flags & MC_EFL_NOT_IN_LISTS) == 0 ) )               
        {
            if(setAddresses.size())
            {
                CBitcoinAddress address;
                if(CBitcoinAddressFromTxEntity(address,&(lpEntity->m_Entity)))
                {
                    if(setAddresses.count(address.ToString()) > 0)
                    {
                        address_count++;
                    }
                }
            }
            else
            {
                address_count++;
            }
        }
    }
    
    uint32_t verbose=0x00;
    
    if (params.size() > 1)
    {
        if(params[1].type() == bool_type)
        {
            if(params[1].get_bool())
            {
                verbose=0x02;
            }        
        }
        else
        {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid value for 'verbose' parameter, should be boolean");                                                                
        }        
    }
        
    int count,start;
    count=entity_count;
    if (params.size() > 2)    
    {
        count=paramtoint(params[2],true,0,"Invalid count");
    }
    start=-count;
    if (params.size() > 3)    
    {
        start=paramtoint(params[3],false,0,"Invalid start");
    }
    
    mc_AdjustStartAndCount(&count,&start,address_count);
    
    address_count=0;
    for(int i=0;i<entity_count;i++)
    {
        lpEntity=pwalletTxsMain->GetEntity(i);        
        CBitcoinAddress address;
        if( (lpEntity->m_Entity.m_EntityType & MC_TET_ORDERMASK) == MC_TET_CHAINPOS)
        {
            if(CBitcoinAddressFromTxEntity(address,&(lpEntity->m_Entity)))
            {
                if( (setAddresses.size() == 0) || (setAddresses.count(address.ToString()) > 0) )
                {
                    if((address_count >= start) && (address_count < start+count))
                    {
                        result.push_back(AddressEntry(address,verbose));
                    }
                    address_count++;
                }
            }
        }
    }
    
    return result;
}



Value storechunk(const Array& params, bool fHelp)
{
    int err;
    
    if (fHelp || params.size() != 1) 
        throw runtime_error("Help message not found\n");
    
    if((mc_gState->m_WalletMode & MC_WMD_TXS) == 0)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "API is not supported with this wallet version. To get this functionality, run \"multichaind -walletdbversion=2 -rescan\" ");        
    }   
    
    vector<unsigned char> vValue;
    if(params[0].type() != str_type)
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "data should be hexadecimal string");                                                                                                                
    }
    
    bool fIsHex;
    vValue=ParseHex(params[0].get_str().c_str(),fIsHex);    
    if(!fIsHex)
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "data should be hexadecimal string");                                                                                                                
    }        
    
    if(vValue.size() == 0)
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "data should be non-empty hexadecimal string");                                                                                                                        
    }

    if((int)vValue.size() > MAX_CHUNK_SIZE)
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "data is too long");                                                                                                                        
    }
    
    uint256 hash;
    mc_gState->m_TmpBuffers->m_RpcHasher1->DoubleHash(&vValue[0],(int)vValue.size(),&hash);
/*    
    mc_SHA256 *hasher;
    
    hasher=new mc_SHA256;    
    hasher->Reset();
    hasher->Write(&vValue[0],(int)vValue.size());
    hasher->GetHash((unsigned char*)&hash);
    hasher->Reset();
    hasher->Write((unsigned char*)&hash,32);
    hasher->GetHash((unsigned char*)&hash);
    
    delete hasher;
*/    
    mc_TxEntity entity;
    entity.Zero();
    entity.m_EntityType=MC_TET_AUTHOR;
    
    err=pwalletTxsMain->m_ChunkDB->AddChunk((unsigned char*)&hash,&entity,NULL,-1,(unsigned char*)&vValue[0],NULL,NULL,(int)vValue.size(),0,0,0);
    
    if(err)
    {
        switch(err)
        {
            case MC_ERR_FOUND:
                break;
            default:
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Internal error: couldn't store chunk");                                                                                                                                
                break;
        }
    }
    
//    pwalletTxsMain->m_ChunkDB->FlushSourceChunks(GetArg("-chunkflushmode",MC_CDB_FLUSH_MODE_COMMIT));
    if(pwalletTxsMain->m_ChunkDB->FlushSourceChunks(GetArg("-flushsourcechunks",true) ? (MC_CDB_FLUSH_MODE_FILE | MC_CDB_FLUSH_MODE_DATASYNC) : MC_CDB_FLUSH_MODE_NONE))
    {
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Couldn't store offchain items, probably chunk database is corrupted");                                                
    }
    pwalletTxsMain->m_ChunkDB->Dump("storechunk");
    
    return hash.GetHex();
}



Value txoutdata_operation(const Array& params,int fHan)
{
    CScript txout_script;
    uint32_t format;
    unsigned char *chunk_hashes;
    int chunk_count=0;   
    int64_t total_chunk_size,out_size;
    uint32_t retrieve_status;
    size_t elem_size;
    const unsigned char *elem;
    string error_str;
    int errorCode;
    
    out_size=0;
    format=MC_SCR_DATA_FORMAT_UNKNOWN;
    elem=NULL;
    
    if(!pEF->DRF_GetData(params[0].get_str(),txout_script,&elem,&out_size,&format,error_str))
    {
        if(error_str.size())
        {
            throw JSONRPCError(RPC_INVALID_PARAMETER, error_str);                    
        }
        if(params[1].type() == str_type)
        {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid data reference");                                
        }
        
        uint256 hash;
        hash=uint256(params[0].get_str());
    //    uint256 hash(params[0].get_str());
        int n = params[1].get_int();

        CTransaction tx;
        bool found=false;
        if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)
        {
            if(pwalletTxsMain->FindWalletTx(hash,NULL) == 0)
            {
                const CWalletTx& wtx=pwalletTxsMain->GetWalletTx(hash,NULL,NULL);
                tx=CTransaction(wtx);
                found=true;
            }
        }
        else
        {
            if (pwalletMain->mapWallet.count(hash))
            {
                const CWalletTx& wtx = pwalletMain->mapWallet[hash];
                tx=CTransaction(wtx);
                found=true;

            }
        }

        if(!found)
        {
            uint256 hashBlock = 0;
            if (!GetTransaction(hash, tx, hashBlock, true))
            {
                throw JSONRPCError(RPC_TX_NOT_FOUND, "No information available about transaction");
            }
        }

        if( (n<0) || (n >= (int)tx.vout.size()) ) 
        {
            throw JSONRPCError(RPC_OUTPUT_NOT_FOUND, "Invalid vout");        
        }
        
        txout_script = tx.vout[n].scriptPubKey;     
    }
            
    if(txout_script.size())
    {
        const CScript& script1 = txout_script;        
        CScript::const_iterator pc1 = script1.begin();

        mc_gState->m_TmpScript->Clear();
        mc_gState->m_TmpScript->SetScript((unsigned char*)(&pc1[0]),(size_t)(script1.end()-pc1),MC_SCR_TYPE_SCRIPTPUBKEY);

        if(mc_gState->m_TmpScript->IsOpReturnScript() == 0)                      
        {
            unsigned char *ptr;
            int size;
            elem=NULL;

            for (int e = 0; e < mc_gState->m_TmpScript->GetNumElements(); e++)
            {
                mc_gState->m_TmpScript->SetElement(e);
                if(mc_gState->m_TmpScript->GetRawData(&ptr,&size) == 0)      
                {
                    if(elem)
                    {
                        throw JSONRPCError(RPC_NOT_ALLOWED, "This output has more than one data item");                                
                    }
                    format=MC_SCR_DATA_FORMAT_UNKNOWN;
                    if(e > 0)
                    {
                        mc_gState->m_TmpScript->SetElement(e-1);
                        mc_gState->m_TmpScript->GetDataFormat(&format);
                    }
                    elem=ptr;
                    elem_size=size;
                    out_size=elem_size;
                }        
            }
            if(elem == NULL)
            {
                throw JSONRPCError(RPC_OUTPUT_NOT_DATA, "Output without metadata");        
            }
        }
        else
        {
    //        mc_gState->m_TmpScript->ExtractAndDeleteDataFormat(&format);
            mc_gState->m_TmpScript->ExtractAndDeleteDataFormat(&format,&chunk_hashes,&chunk_count,&total_chunk_size);
            retrieve_status = GetFormattedData(mc_gState->m_TmpScript,&elem,&out_size,chunk_hashes,chunk_count,total_chunk_size);
            if(retrieve_status & MC_OST_ERROR_MASK)
            {
                error_str=OffChainError(retrieve_status,&errorCode);
                throw JSONRPCError(errorCode, error_str);                    
            }

            elem_size=(size_t)out_size;
            if( ( (retrieve_status & MC_OST_STATUS_MASK) != MC_OST_RETRIEVED ) && 
                ( (retrieve_status & MC_OST_STORAGE_MASK) != MC_OST_ON_CHAIN ) )
            {
                    throw JSONRPCError(RPC_OUTPUT_NOT_FOUND, "Data for this output is not available");        
            }            
    //        elem = mc_gState->m_TmpScript->GetData(mc_gState->m_TmpScript->GetNumElements()-1,&elem_size);
        }
    }

    int64_t count,start;
    count=out_size;
    start=0;
    
    if (params.size() > 2)    
    {
        count=paramtoint64(params[2],true,0,"Invalid count");
    }
    if (params.size() > 3)    
    {
        start=paramtoint64(params[3],false,0,"Invalid start");
    }

    if(start < 0)
    {
        start=out_size+start;
        if(start<0)
        {
            start=0;
        }        
    }

    if(start > out_size)
    {
        start=out_size;
    }
    if(start+count > out_size)
    {
        count=out_size-start;
    }

    if( (format == MC_SCR_DATA_FORMAT_UBJSON) || (format == MC_SCR_DATA_FORMAT_UTF8) )
    {
        if(fHan)
        {
            throw JSONRPCError(RPC_NOT_SUPPORTED, "This API is not supported for text and JSON data");                                                                                        
        }
        if(start != 0)
        {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid start, must be 0 for text and JSON data");                                                                            
        }
        if(count != out_size)
        {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid count, must include all text or JSON data");                                                                            
        }
    }
    
    if(fHan)
    {
        if(chunk_count > 1)
        {
            if(elem == NULL)
            {
                elem=GetChunkDataInRange(&out_size,chunk_hashes,chunk_count,start,count,fHan);
                if(elem == NULL)
                {
                    throw JSONRPCError(RPC_INTERNAL_ERROR, "Couldn't retrieve data for this output");                                                                                            
                }
                return count;
            }
        }        
        else
        {
            if(write(fHan,elem+start,count) != count)
            {
                throw JSONRPCError(RPC_INTERNAL_ERROR, "Cannot store binary cache item");                                                                                                                                    
            }            
            return count;
        }
    }
    
    if(count > 0x4000000)
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid count, must be below 64MB");                                                                            
    }
    
    if(chunk_count > 1)
    {
        if(elem == NULL)
        {
            elem=GetChunkDataInRange(&out_size,chunk_hashes,chunk_count,start,count,0);
            if(elem == NULL)
            {
                throw JSONRPCError(RPC_INTERNAL_ERROR, "Couldn't retrieve data for this output");                                                                                            
            }
            return OpReturnFormatEntry(elem,count,0,0,format,NULL);        
        }
    }
    return OpReturnFormatEntry(elem+start,count,0,0,format,NULL);            
}

Value txouttobinarycache(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 3 || params.size() > 5)                        // MCHN
        throw runtime_error("Help message not found\n");    
 
    int64_t size;
    
    int fHan=mc_BinaryCacheFile(params[0].get_str(),2);
    if(fHan <= 0)
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Binary cache item with this identifier not found");                                                                                                                        
    }
    
    size=lseek64(fHan,0,SEEK_END);
    
    if(size)
    {
        close(fHan);
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Binary cache item is not empty");                                                                                                                                            
    }

    Array ext_params;
    int param_count=0;
    BOOST_FOREACH(const Value& value, params)
    {
        if(param_count)
        {
            ext_params.push_back(value);
        }
        param_count++;
    }
    
    size=txoutdata_operation(ext_params,fHan).get_int64();    

    close(fHan);
        
    return size;
    
}

Value gettxoutdata(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 4)                        // MCHN
        throw runtime_error("Help message not found\n");
    
    return txoutdata_operation(params,0);    
}

/* MCHN END */

Value listaddressgroupings(const Array& params, bool fHelp)
{
    if (fHelp)                                                                  // MCHN
        throw runtime_error("Help message not found\n");

    Array jsonGroupings;
    map<CTxDestination, CAmount> balances = pwalletMain->GetAddressBalances();
    BOOST_FOREACH(set<CTxDestination> grouping, pwalletMain->GetAddressGroupings())
    {
        Array jsonGrouping;
        BOOST_FOREACH(CTxDestination address, grouping)
        {
            Array addressInfo;
            addressInfo.push_back(CBitcoinAddress(address).ToString());
            addressInfo.push_back(ValueFromAmount(balances[address]));
            {
                LOCK(pwalletMain->cs_wallet);
                if (pwalletMain->mapAddressBook.find(CBitcoinAddress(address).Get()) != pwalletMain->mapAddressBook.end())
                    addressInfo.push_back(pwalletMain->mapAddressBook.find(CBitcoinAddress(address).Get())->second.name);
            }
            jsonGrouping.push_back(addressInfo);
        }
        jsonGroupings.push_back(jsonGrouping);
    }
    return jsonGroupings;
}

Value signmessage(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 2)                                            // MCHN
        throw runtime_error("Help message not found\n");

    EnsureWalletIsUnlocked();

    string strAddress = params[0].get_str();
    string strMessage = params[1].get_str();

    CKey key;
    CBitcoinAddress addr(strAddress);
    if (!addr.IsValid())
    {
        CBitcoinSecret vchSecret;
        bool fGood = vchSecret.SetString(strAddress);

        if (fGood)
        {
            key = vchSecret.GetKey();
            if (!key.IsValid()) 
            {
                fGood=false;
            }            
            else
            {
                CPubKey pubkey = key.GetPubKey();
                assert(key.VerifyPubKey(pubkey));
                
            }
        }
        if(!fGood)
        {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address or private key");            
        }
    }
    else
    {
        CKeyID keyID;
        if (!addr.GetKeyID(keyID))
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Address does not refer to key");

        if (!pwalletMain->GetKey(keyID, key))
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Private key not available");        
    }


    CHashWriter ss(SER_GETHASH, 0);
    ss << strMessageMagic;
    ss << strMessage;

    vector<unsigned char> vchSig;
    if (!key.SignCompact(ss.GetHash(), vchSig))
        throw JSONRPCError(RPC_WALLET_ERROR, "Sign failed");

    return EncodeBase64(&vchSig[0], vchSig.size());
}

Value getreceivedbyaddress(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)                        // MCHN
        throw runtime_error("Help message not found\n");

    if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "Not supported with scalable wallet - if you need accounts, run multichaind -walletdbversion=1 -rescan, but the wallet will perform worse");        
    }            
    
    // Bitcoin address
    CBitcoinAddress address = CBitcoinAddress(params[0].get_str());
    if (!address.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");
    CScript scriptPubKey = GetScriptForDestination(address.Get());
    if (!IsMine(*pwalletMain,scriptPubKey))
        return (double)0.0;

    // Minimum confirmations
    int nMinDepth = 1;
    if (params.size() > 1)
        nMinDepth = params[1].get_int();

    // Tally
    CAmount nAmount = 0;
    for (map<uint256, CWalletTx>::iterator it = pwalletMain->mapWallet.begin(); it != pwalletMain->mapWallet.end(); ++it)
    {
        const CWalletTx& wtx = (*it).second;
        if (wtx.IsCoinBase() || !IsFinalTx(wtx))
            continue;

        BOOST_FOREACH(const CTxOut& txout, wtx.vout)
            if (txout.scriptPubKey == scriptPubKey)
                if (wtx.GetDepthInMainChain() >= nMinDepth)
                    nAmount += txout.nValue;
    }

    return  ValueFromAmount(nAmount);
}


Value getreceivedbyaccount(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error("Help message not found\n");

    if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "Accounts are not supported with scalable wallet - if you need accounts, run multichaind -walletdbversion=1 -rescan, but the wallet will perform worse");        
    }            
    
    // Minimum confirmations
    int nMinDepth = 1;
    if (params.size() > 1)
        nMinDepth = params[1].get_int();

    // Get the set of pub keys assigned to account
    string strAccount = AccountFromValue(params[0]);
    set<CTxDestination> setAddress = pwalletMain->GetAccountAddresses(strAccount);

    // Tally
    CAmount nAmount = 0;
    for (map<uint256, CWalletTx>::iterator it = pwalletMain->mapWallet.begin(); it != pwalletMain->mapWallet.end(); ++it)
    {
        const CWalletTx& wtx = (*it).second;
        if (wtx.IsCoinBase() || !IsFinalTx(wtx))
            continue;

        BOOST_FOREACH(const CTxOut& txout, wtx.vout)
        {
            CTxDestination address;
            if (ExtractDestination(txout.scriptPubKey, address) && IsMine(*pwalletMain, address) && setAddress.count(address))
                if (wtx.GetDepthInMainChain() >= nMinDepth)
                    nAmount += txout.nValue;
        }
    }

/*  MCHN START */   
    if(COIN == 0)
    {
        return (double)nAmount;
    }
/*  MCHN END */   
    return (double)nAmount / (double)COIN;
}


CAmount GetAccountBalance(CWalletDB& walletdb, const string& strAccount, int nMinDepth, const isminefilter& filter)
{
    CAmount nBalance = 0;

    // Tally wallet transactions
    for (map<uint256, CWalletTx>::iterator it = pwalletMain->mapWallet.begin(); it != pwalletMain->mapWallet.end(); ++it)
    {
        const CWalletTx& wtx = (*it).second;
        if (!IsFinalTx(wtx) || wtx.GetBlocksToMaturity() > 0 || wtx.GetDepthInMainChain() < 0)
            continue;

        CAmount nReceived, nSent, nFee;
        wtx.GetAccountAmounts(strAccount, nReceived, nSent, nFee, filter);

        if (nReceived != 0 && wtx.GetDepthInMainChain() >= nMinDepth)
            nBalance += nReceived;
        nBalance -= nSent + nFee;
    }

    // Tally internal accounting entries
    nBalance += walletdb.GetAccountCreditDebit(strAccount);

    return nBalance;
}

CAmount GetAccountBalance(const string& strAccount, int nMinDepth, const isminefilter& filter)
{
    CWalletDB walletdb(pwalletMain->strWalletFile);
    return GetAccountBalance(walletdb, strAccount, nMinDepth, filter);
}


Value getbalance(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 3)                                             // MCHN
        throw runtime_error("Help message not found\n");

    if (params.size() == 0)
        return  ValueFromAmount(pwalletMain->GetBalance());

    int nMinDepth = 1;
    if (params.size() > 1)
        nMinDepth = params[1].get_int();
    isminefilter filter = ISMINE_SPENDABLE;
    if(params.size() > 2)
        if(params[2].get_bool())
            filter = filter | ISMINE_WATCH_ONLY;

    if (params[0].get_str() == "*") {
        // Calculate total balance a different way from GetBalance()
        // (GetBalance() sums up all unspent TxOuts)
        // getbalance and getbalance '*' 0 should return the same number
        CAmount nBalance = 0;
        if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)
        {
            vector<COutput> vecOutputs;        
            pwalletMain->AvailableCoins(vecOutputs, false, NULL, false,true);
            BOOST_FOREACH(const COutput& out, vecOutputs) 
            {
                CTxOut txout;
                out.GetHashAndTxOut(txout);

                isminetype fIsMine=pwalletMain->IsMine(txout);
                
                if(fIsMine & filter)
                {
                    if(out.IsTrustedNoDepth() || (out.nDepth >= nMinDepth))
                    {
                        nBalance+=txout.nValue;
                    }
                }
            }            
        }
        else
        {
            for (map<uint256, CWalletTx>::iterator it = pwalletMain->mapWallet.begin(); it != pwalletMain->mapWallet.end(); ++it)
            {
                const CWalletTx& wtx = (*it).second;
                if (!IsFinalTx(wtx) || wtx.GetBlocksToMaturity() > 0 || wtx.GetDepthInMainChain() < 0)
                    continue;

                CAmount allFee;
                string strSentAccount;
                list<COutputEntry> listReceived;
                list<COutputEntry> listSent;
                wtx.GetAmounts(listReceived, listSent, allFee, strSentAccount, filter);
                if (wtx.GetDepthInMainChain() >= nMinDepth)
                {
                    BOOST_FOREACH(const COutputEntry& r, listReceived)
                        nBalance += r.amount;
                }
                BOOST_FOREACH(const COutputEntry& s, listSent)
                    nBalance -= s.amount;
                nBalance -= allFee;
            }
        }
        return  ValueFromAmount(nBalance);
    }

    if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "Accounts are not supported with scalable wallet - if you need getbalance, run multichaind -walletdbversion=1 -rescan, but the wallet will perform worse");        
    }
    string strAccount = AccountFromValue(params[0]);

    CAmount nBalance = GetAccountBalance(strAccount, nMinDepth, filter);

    return ValueFromAmount(nBalance);
}

Value getunconfirmedbalance(const Array &params, bool fHelp)
{
    if (fHelp || params.size() > 0)
        throw runtime_error("Help message not found\n");
    return ValueFromAmount(pwalletMain->GetUnconfirmedBalance());
}


Value movecmd(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 3 || params.size() > 5)
        throw runtime_error("Help message not found\n");

    if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "Accounts are not supported with scalable wallet - if you need move, run multichaind -walletdbversion=1 -rescan, but the wallet will perform worse");        
    }
    
    string strFrom = AccountFromValue(params[0]);
    string strTo = AccountFromValue(params[1]);
    CAmount nAmount = AmountFromValue(params[2]);
    if (params.size() > 3)
        // unused parameter, used to be nMinDepth, keep type-checking it though
        (void)params[3].get_int();
    string strComment;
    if (params.size() > 4)
        strComment = params[4].get_str();

    CWalletDB walletdb(pwalletMain->strWalletFile);
    if (!walletdb.TxnBegin())
        throw JSONRPCError(RPC_DATABASE_ERROR, "database error");

    int64_t nNow = GetAdjustedTime();

    // Debit
    CAccountingEntry debit;
    debit.nOrderPos = pwalletMain->IncOrderPosNext(&walletdb);
    debit.strAccount = strFrom;
    debit.nCreditDebit = -nAmount;
    debit.nTime = nNow;
    debit.strOtherAccount = strTo;
    debit.strComment = strComment;
    walletdb.WriteAccountingEntry(debit);

    // Credit
    CAccountingEntry credit;
    credit.nOrderPos = pwalletMain->IncOrderPosNext(&walletdb);
    credit.strAccount = strTo;
    credit.nCreditDebit = nAmount;
    credit.nTime = nNow;
    credit.strOtherAccount = strFrom;
    credit.strComment = strComment;
    walletdb.WriteAccountingEntry(credit);

    if (!walletdb.TxnCommit())
        throw JSONRPCError(RPC_DATABASE_ERROR, "database error");

    return true;
}


Value sendfrom(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 3 || params.size() > 6)                        // MCHN 
        throw runtime_error("Help message not found\n");

    if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "Accounts are not supported with scalable wallet - if you need sendfrom, run multichaind -walletdbversion=1 -rescan, but the wallet will perform worse");        
    }
    
    string strAccount = AccountFromValue(params[0]);
    CBitcoinAddress address(params[1].get_str());
    if (!address.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");
    CAmount nAmount = AmountFromValue(params[2]);
    int nMinDepth = 1;
    if (params.size() > 3)
        nMinDepth = params[3].get_int();

    CWalletTx wtx;
    wtx.strFromAccount = strAccount;
    if (params.size() > 4 && params[4].type() != null_type && !params[4].get_str().empty())
        wtx.mapValue["comment"] = params[4].get_str();
    if (params.size() > 5 && params[5].type() != null_type && !params[5].get_str().empty())
        wtx.mapValue["to"]      = params[5].get_str();

    EnsureWalletIsUnlocked();

    // Check funds
    CAmount nBalance = GetAccountBalance(strAccount, nMinDepth, ISMINE_SPENDABLE);
    if (nAmount > nBalance)
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, "Account has insufficient funds");

    LOCK (pwalletMain->cs_wallet_send);
    
    SendMoney(address.Get(), nAmount, wtx, NULL, NULL);

    return wtx.GetHash().GetHex();
}


Value sendmany(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 4)                        // MCHN
        throw runtime_error("Help message not found\n");

    if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "Accounts are not supported with scalable wallet - if you need sendmany, run multichaind -walletdbversion=1 -rescan, but the wallet will perform worse");        
    }
    
    string strAccount = AccountFromValue(params[0]);
    Object sendTo = params[1].get_obj();
    int nMinDepth = 1;
    if (params.size() > 2)
        nMinDepth = params[2].get_int();

    CWalletTx wtx;
    wtx.strFromAccount = strAccount;
    if (params.size() > 3 && params[3].type() != null_type && !params[3].get_str().empty())
        wtx.mapValue["comment"] = params[3].get_str();

    set<CBitcoinAddress> setAddress;
    vector<pair<CScript, CAmount> > vecSend;

    CAmount totalAmount = 0;
    BOOST_FOREACH(const Pair& s, sendTo)
    {
        CBitcoinAddress address(s.name_);
        if (!address.IsValid())
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, string("Invalid address: ")+s.name_);

        if (setAddress.count(address))
            throw JSONRPCError(RPC_INVALID_PARAMETER, string("Invalid parameter, duplicated address: ")+s.name_);
        setAddress.insert(address);

        CScript scriptPubKey = GetScriptForDestination(address.Get());
        CAmount nAmount = AmountFromValue(s.value_);
        totalAmount += nAmount;

        vecSend.push_back(make_pair(scriptPubKey, nAmount));
    }

    EnsureWalletIsUnlocked();

    // Check funds
    CAmount nBalance = GetAccountBalance(strAccount, nMinDepth, ISMINE_SPENDABLE);
    if (totalAmount > nBalance)
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, "Account has insufficient funds");

/* MCHN START */
    LOCK(pwalletMain->cs_wallet_send);
/* MCHN END */
    
    // Send
    CReserveKey keyChange(pwalletMain);
    CAmount nFeeRequired = 0;
    string strFailReason;
    bool fCreated = pwalletMain->CreateTransaction(vecSend, wtx, keyChange, nFeeRequired, strFailReason);
    if (!fCreated)
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, strFailReason);
/* MCHN START */    
    string strRejectReason;
    if (!pwalletMain->CommitTransaction(wtx, keyChange,strRejectReason))
        throw JSONRPCError(RPC_TRANSACTION_REJECTED, "Transaction commit failed: " + strRejectReason);
/* MCHN END */    

    return wtx.GetHash().GetHex();
}

// Defined in rpcmisc.cpp
extern CScript _createmultisig_redeemScript(const Array& params);

Value addmultisigaddress(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 3)                        // MCHN
    {
        throw runtime_error("Help message not found\n");
    }

    string strAccount;
    if (params.size() > 2)
    {
        if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)
        {
            throw JSONRPCError(RPC_NOT_SUPPORTED, "Accounts are not supported with scalable wallet - if you need accounts, run multichaind -walletdbversion=1 -rescan, but the wallet will perform worse");        
        }            
        strAccount = AccountFromValue(params[2]);
    }
    // Construct using pay-to-script-hash:
    CScript inner = _createmultisig_redeemScript(params);
    CScriptID innerID(inner);
    pwalletMain->AddCScript(inner);

    pwalletMain->SetAddressBook(innerID, strAccount, "send");

    if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)
    {
        mc_TxEntity entity;
        entity.Zero();
        memcpy(entity.m_EntityID,&innerID,MC_TDB_ENTITY_ID_SIZE);
        entity.m_EntityType=MC_TET_SCRIPT_ADDRESS | MC_TET_CHAINPOS;
        pwalletTxsMain->AddEntity(&entity,MC_EFL_NOT_IN_SYNC);
        entity.m_EntityType=MC_TET_SCRIPT_ADDRESS | MC_TET_TIMERECEIVED;
        pwalletTxsMain->AddEntity(&entity,MC_EFL_NOT_IN_SYNC);
    }

    CScript outer = GetScriptForDestination(innerID);
    
    if(IsMine(*pwalletMain, outer) == ISMINE_NO)
    {
        if (!pwalletMain->HaveWatchOnly(outer))
        {
            if (!pwalletMain->AddWatchOnly(outer))
                throw JSONRPCError(RPC_WALLET_ERROR, "Error adding address to wallet");            
        }
    }
    
    return CBitcoinAddress(innerID).ToString();
}


struct tallyitem
{
    CAmount nAmount;
    int nConf;
    vector<uint256> txids;
    bool fIsWatchonly;
    tallyitem()
    {
        nAmount = 0;
        nConf = std::numeric_limits<int>::max();
        fIsWatchonly = false;
    }
};

Value ListReceived(const Array& params, bool fByAccounts)
{
    // Minimum confirmations
    int nMinDepth = 1;
    if (params.size() > 0)
        nMinDepth = params[0].get_int();

    // Whether to include empty accounts
    bool fIncludeEmpty = false;
    if (params.size() > 1)
        fIncludeEmpty = params[1].get_bool();

    isminefilter filter = ISMINE_SPENDABLE;
    if(params.size() > 2)
        if(params[2].get_bool())
            filter = filter | ISMINE_WATCH_ONLY;

    // Tally
    map<CBitcoinAddress, tallyitem> mapTally;
    for (map<uint256, CWalletTx>::iterator it = pwalletMain->mapWallet.begin(); it != pwalletMain->mapWallet.end(); ++it)
    {
        const CWalletTx& wtx = (*it).second;

        if (wtx.IsCoinBase() || !IsFinalTx(wtx))
            continue;

        int nDepth = wtx.GetDepthInMainChain();
        if (nDepth < nMinDepth)
            continue;

        BOOST_FOREACH(const CTxOut& txout, wtx.vout)
        {
            CTxDestination address;
            if (!ExtractDestination(txout.scriptPubKey, address))
                continue;

            isminefilter mine = IsMine(*pwalletMain, address);
            if(!(mine & filter))
                continue;

            tallyitem& item = mapTally[address];
            item.nAmount += txout.nValue;
            item.nConf = min(item.nConf, nDepth);
            item.txids.push_back(wtx.GetHash());
            if (mine & ISMINE_WATCH_ONLY)
                item.fIsWatchonly = true;
        }
    }

    // Reply
    Array ret;
    map<string, tallyitem> mapAccountTally;
    BOOST_FOREACH(const PAIRTYPE(CBitcoinAddress, CAddressBookData)& item, pwalletMain->mapAddressBook)
    {
        const CBitcoinAddress& address = item.first;
        const string& strAccount = item.second.name;
        map<CBitcoinAddress, tallyitem>::iterator it = mapTally.find(address);
        if (it == mapTally.end() && !fIncludeEmpty)
            continue;

        CAmount nAmount = 0;
        int nConf = std::numeric_limits<int>::max();
        bool fIsWatchonly = false;
        if (it != mapTally.end())
        {
            nAmount = (*it).second.nAmount;
            nConf = (*it).second.nConf;
            fIsWatchonly = (*it).second.fIsWatchonly;
        }

        if (fByAccounts)
        {
            tallyitem& item = mapAccountTally[strAccount];
            item.nAmount += nAmount;
            item.nConf = min(item.nConf, nConf);
            item.fIsWatchonly = fIsWatchonly;
        }
        else
        {
            Object obj;
            if(fIsWatchonly)
                obj.push_back(Pair("involvesWatchonly", true));
            obj.push_back(Pair("address",       address.ToString()));
            obj.push_back(Pair("account",       strAccount));
            obj.push_back(Pair("amount",        ValueFromAmount(nAmount)));
            obj.push_back(Pair("confirmations", (nConf == std::numeric_limits<int>::max() ? 0 : nConf)));
            Array transactions;
            if (it != mapTally.end())
            {
                BOOST_FOREACH(const uint256& item, (*it).second.txids)
                {
                    transactions.push_back(item.GetHex());
                }
            }
            obj.push_back(Pair("txids", transactions));
            ret.push_back(obj);
        }
    }

    if (fByAccounts)
    {
        for (map<string, tallyitem>::iterator it = mapAccountTally.begin(); it != mapAccountTally.end(); ++it)
        {
            CAmount nAmount = (*it).second.nAmount;
            int nConf = (*it).second.nConf;
            Object obj;
            if((*it).second.fIsWatchonly)
                obj.push_back(Pair("involvesWatchonly", true));
            obj.push_back(Pair("account",       (*it).first));
            obj.push_back(Pair("amount",        ValueFromAmount(nAmount)));
            obj.push_back(Pair("confirmations", (nConf == std::numeric_limits<int>::max() ? 0 : nConf)));
            ret.push_back(obj);
        }
    }

    return ret;
}

Value listreceivedbyaddress(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 3)
        throw runtime_error("Help message not found\n");

    if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "Not supported with scalable wallet - if you need accounts, run multichaind -walletdbversion=1 -rescan, but the wallet will perform worse");        
    }            
    
    return ListReceived(params, false);
}

Value listreceivedbyaccount(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 3)
        throw runtime_error("Help message not found\n");

    if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "Accounts are not supported with scalable wallet - if you need accounts, run multichaind -walletdbversion=1 -rescan, but the wallet will perform worse");        
    }            

    return ListReceived(params, true);
}

static void MaybePushAddress(Object & entry, const CTxDestination &dest)
{
    CBitcoinAddress addr;
    if (addr.Set(dest))
        entry.push_back(Pair("address", addr.ToString()));
}

/* MCHN START */




/* MCHN END */

void ListTransactions(const CWalletTx& wtx, const string& strAccount, int nMinDepth, bool fLong, Array& ret, const isminefilter& filter, mc_TxDefRow *txdef)
{
    CAmount nFee;
    string strSentAccount;
    list<COutputEntry> listReceived;
    list<COutputEntry> listSent;

    wtx.GetAmounts(listReceived, listSent, nFee, strSentAccount, filter);

    bool fAllAccounts = (strAccount == string("*"));
    bool involvesWatchonly = wtx.IsFromMe(ISMINE_WATCH_ONLY);

    // Sent
    if ((!listSent.empty() || nFee != 0) && (fAllAccounts || strAccount == strSentAccount))
    {
        BOOST_FOREACH(const COutputEntry& s, listSent)
        {
            Object entry;
            if(involvesWatchonly || (::IsMine(*pwalletMain, s.destination) & ISMINE_WATCH_ONLY))
                entry.push_back(Pair("involvesWatchonly", true));
            entry.push_back(Pair("account", strSentAccount));
            MaybePushAddress(entry, s.destination);
            entry.push_back(Pair("category", "send"));
            entry.push_back(Pair("amount", ValueFromAmount(-s.amount)));
            entry.push_back(Pair("vout", s.vout));
            entry.push_back(Pair("fee", ValueFromAmount(-nFee)));
            if (fLong)
                WalletTxToJSON(wtx, entry);
            ret.push_back(entry);
        }
    }

    int nDepth=0;
    if(txdef)
    {
        if(txdef->m_Block >= 0)
        {
            nDepth= chainActive.Height()-txdef->m_Block+1;
            if(nDepth<0)
            {
                nDepth=0;
            }
        }        
    }
    else
    {
        nDepth=wtx.GetDepthInMainChain();
    }
    
    // Received
    if (listReceived.size() > 0 && (nDepth >= nMinDepth) )
    {
        BOOST_FOREACH(const COutputEntry& r, listReceived)
        {
            string account;
            if (pwalletMain->mapAddressBook.count(r.destination))
                account = pwalletMain->mapAddressBook[r.destination].name;
            if (fAllAccounts || (account == strAccount))
            {
                Object entry;
                if(involvesWatchonly || (::IsMine(*pwalletMain, r.destination) & ISMINE_WATCH_ONLY))
                    entry.push_back(Pair("involvesWatchonly", true));
                entry.push_back(Pair("account", account));
                MaybePushAddress(entry, r.destination);
                if (wtx.IsCoinBase())
                {
                    if (nDepth < 1)
                        entry.push_back(Pair("category", "orphan"));
                    else if (max(0, (COINBASE_MATURITY+1) - nDepth) > 0)
                        entry.push_back(Pair("category", "immature"));
                    else
                        entry.push_back(Pair("category", "generate"));
                }
                else
                {
                    entry.push_back(Pair("category", "receive"));
                }
                entry.push_back(Pair("amount", ValueFromAmount(r.amount)));
                entry.push_back(Pair("vout", r.vout));
                if (fLong)
                    WalletTxToJSON(wtx, entry);
/* MCHN START */         
/*                
                Object full_entry;
                TxToJSON(wtx,wtx.hashBlock,full_entry);
                entry.push_back(Pair("details",full_entry));                
 */ 
/* MCHN END */                
                ret.push_back(entry);
            }
        }
    }
}

void ListTransactions(const CWalletTx& wtx, const string& strAccount, int nMinDepth, bool fLong, Array& ret, const isminefilter& filter)
{
    ListTransactions(wtx,strAccount,nMinDepth,fLong,ret,filter,NULL);
}

void AcentryToJSON(const CAccountingEntry& acentry, const string& strAccount, Array& ret)
{
    bool fAllAccounts = (strAccount == string("*"));

    if (fAllAccounts || acentry.strAccount == strAccount)
    {
        Object entry;
        entry.push_back(Pair("account", acentry.strAccount));
        entry.push_back(Pair("category", "move"));
        entry.push_back(Pair("time", acentry.nTime));
        entry.push_back(Pair("amount", ValueFromAmount(acentry.nCreditDebit)));
        entry.push_back(Pair("otheraccount", acentry.strOtherAccount));
        entry.push_back(Pair("comment", acentry.strComment));
        ret.push_back(entry);
    }
}

Value listtransactions(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 4)                                             // MCHN
        throw runtime_error("Help message not found\n");

    string strAccount = "*";
    if (params.size() > 0)
        strAccount = params[0].get_str();
    int nCount = 10;
    if (params.size() > 1)
        nCount = params[1].get_int();
    int nFrom = 0;
    if (params.size() > 2)
        nFrom = params[2].get_int();
    isminefilter filter = ISMINE_SPENDABLE;
    if(params.size() > 3)
        if(params[3].get_bool())
            filter = filter | ISMINE_WATCH_ONLY;

    if (nCount < 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Negative count");
    if (nFrom < 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Negative from");

    Array ret;
    if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "Not supported with scalable wallet - if you need listtransactions, run multichaind -walletdbversion=1 -rescan, but the wallet will perform worse");        
    }
    else
    {
        std::list<CAccountingEntry> acentries;
        CWallet::TxItems txOrdered = pwalletMain->OrderedTxItems(acentries, strAccount);

        // iterate backwards until we have nCount items to return:
        for (CWallet::TxItems::reverse_iterator it = txOrdered.rbegin(); it != txOrdered.rend(); ++it)
        {
            CWalletTx *const pwtx = (*it).second.first;
            if (pwtx != 0)
                ListTransactions(*pwtx, strAccount, 0, true, ret, filter);

            CAccountingEntry *const pacentry = (*it).second.second;
            if (pacentry != 0)
                AcentryToJSON(*pacentry, strAccount, ret);

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

/* MCHN START */    


/* MCHN END */    

Value listaccounts(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 2)
        throw runtime_error("Help message not found\n");

    if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "Accounts are not supported with scalable wallet - if you need listaccounts, run multichaind -walletdbversion=1 -rescan, but the wallet will perform worse");        
    }   

    int nMinDepth = 1;
    if (params.size() > 0)
        nMinDepth = params[0].get_int();
    isminefilter includeWatchonly = ISMINE_SPENDABLE;
    if(params.size() > 1)
        if(params[1].get_bool())
            includeWatchonly = includeWatchonly | ISMINE_WATCH_ONLY;

    map<string, CAmount> mapAccountBalances;
    BOOST_FOREACH(const PAIRTYPE(CTxDestination, CAddressBookData)& entry, pwalletMain->mapAddressBook) {
        if (IsMine(*pwalletMain, entry.first) & includeWatchonly) // This address belongs to me
            mapAccountBalances[entry.second.name] = 0;
    }

    for (map<uint256, CWalletTx>::iterator it = pwalletMain->mapWallet.begin(); it != pwalletMain->mapWallet.end(); ++it)
    {
        const CWalletTx& wtx = (*it).second;
        CAmount nFee;
        string strSentAccount;
        list<COutputEntry> listReceived;
        list<COutputEntry> listSent;
        int nDepth = wtx.GetDepthInMainChain();
        if (wtx.GetBlocksToMaturity() > 0 || nDepth < 0)
            continue;
        wtx.GetAmounts(listReceived, listSent, nFee, strSentAccount, includeWatchonly);
        mapAccountBalances[strSentAccount] -= nFee;
        BOOST_FOREACH(const COutputEntry& s, listSent)
            mapAccountBalances[strSentAccount] -= s.amount;
        if (nDepth >= nMinDepth)
        {
            BOOST_FOREACH(const COutputEntry& r, listReceived)
                if (pwalletMain->mapAddressBook.count(r.destination))
                    mapAccountBalances[pwalletMain->mapAddressBook[r.destination].name] += r.amount;
                else
                    mapAccountBalances[""] += r.amount;
        }
    }

    list<CAccountingEntry> acentries;
    CWalletDB(pwalletMain->strWalletFile).ListAccountCreditDebit("*", acentries);
    BOOST_FOREACH(const CAccountingEntry& entry, acentries)
        mapAccountBalances[entry.strAccount] += entry.nCreditDebit;

    Object ret;
    BOOST_FOREACH(const PAIRTYPE(string, CAmount)& accountBalance, mapAccountBalances) {
        ret.push_back(Pair(accountBalance.first, ValueFromAmount(accountBalance.second)));
    }
    return ret;
}

Value listsinceblock(const Array& params, bool fHelp)
{
    if (fHelp)                                                                  // MCHN
        throw runtime_error("Help message not found\n");

    CBlockIndex *pindex = NULL;
    int target_confirms = 1;
    isminefilter filter = ISMINE_SPENDABLE;

    if (params.size() > 0)
    {
        uint256 blockId = 0;

        blockId.SetHex(params[0].get_str());
        BlockMap::iterator it = mapBlockIndex.find(blockId);
        if (it != mapBlockIndex.end())
            pindex = it->second;
    }

    if (params.size() > 1)
    {
        target_confirms = params[1].get_int();

        if (target_confirms < 1)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter");
    }

    if(params.size() > 2)
        if(params[2].get_bool())
            filter = filter | ISMINE_WATCH_ONLY;

    int depth = pindex ? (1 + chainActive.Height() - pindex->nHeight) : -1;

    Array transactions;

    if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)
    {
        int first_block=0;
        int tx_count;
        int this_tx, up_tx, down_tx,chunk_size;
        int err;
        mc_Buffer *lpEntRowBuffer;
        mc_TxEntityRow *entrow;
        mc_TxEntity wallet_entity;
        set <uint256> hashes;
        if(pindex)
        {
            first_block=pindex->nHeight+1;
        }
    
        wallet_entity.Zero();
        wallet_entity.m_EntityType=MC_TET_WALLET_SPENDABLE | MC_TET_CHAINPOS;
        if(filter & ISMINE_WATCH_ONLY)
        {
            wallet_entity.m_EntityType=MC_TET_WALLET_ALL | MC_TET_CHAINPOS;            
        }
        tx_count=pwalletTxsMain->GetListSize(&wallet_entity,NULL);
        
        if(tx_count)
        {
            lpEntRowBuffer=new mc_Buffer;
            lpEntRowBuffer->Initialize(MC_TDB_ENTITY_KEY_SIZE,sizeof(mc_TxEntityRow),MC_BUF_MODE_DEFAULT);
            
            up_tx=tx_count;
            down_tx=1;
            this_tx=(up_tx+down_tx)/2;
            chunk_size=10;
            while(up_tx-down_tx>chunk_size)
            {
                err=pwalletTxsMain->GetList(&wallet_entity,this_tx,1,lpEntRowBuffer);
                CheckWalletError(err,wallet_entity.m_EntityType,"");
                if( (err == MC_ERR_NOERROR) && (lpEntRowBuffer->GetCount() > 0) )
                {
                    entrow=(mc_TxEntityRow *)(lpEntRowBuffer->GetRow(0));
                    if(entrow->m_Block >= first_block)
                    {
                        up_tx=this_tx;                        
                    }
                    else
                    {
                        down_tx=this_tx;
                    }
                    this_tx=(up_tx+down_tx)/2;
                }                
                else                                                            // We are probably in reorg, start from what we have now
                {
                    up_tx=down_tx;
                }
            }
            
            this_tx=down_tx;
            while(this_tx<=tx_count)
            {
                err=pwalletTxsMain->GetList(&wallet_entity,this_tx,chunk_size,lpEntRowBuffer);
                CheckWalletError(err,wallet_entity.m_EntityType,"");
                if( (err == MC_ERR_NOERROR) && (lpEntRowBuffer->GetCount() > 0) )
                {
                    for(int i=0;i<lpEntRowBuffer->GetCount();i++)
                    {
                        entrow=(mc_TxEntityRow*)lpEntRowBuffer->GetRow(i);
                        if( (entrow->m_Block < 0) || (entrow->m_Block >= first_block) )
                        {
                            uint256 hash;
                            memcpy(&hash,entrow->m_TxId,MC_TDB_TXID_SIZE);                            
                            hashes.insert(hash);
                        }
                    }
                }           
                this_tx+=chunk_size;
            }        
            
            BOOST_FOREACH(uint256 hash, hashes)
            {
                mc_TxDefRow txdef;
                const CWalletTx& wtx=pwalletTxsMain->GetWalletTx(hash,&txdef,NULL);
                ListTransactions(wtx, "*", 0, true, transactions, filter,&txdef);                
            }
            
            delete lpEntRowBuffer;
        }
    }
    else
    {        
        for (map<uint256, CWalletTx>::iterator it = pwalletMain->mapWallet.begin(); it != pwalletMain->mapWallet.end(); it++)
        {
            CWalletTx tx = (*it).second;
            if (depth == -1 || tx.GetDepthInMainChain() < depth)
                ListTransactions(tx, "*", 0, true, transactions, filter);
        }
    }

    CBlockIndex *pblockLast = chainActive[chainActive.Height() + 1 - target_confirms];
    uint256 lastblock = pblockLast ? pblockLast->GetBlockHash() : 0;

    Object ret;
    ret.push_back(Pair("transactions", transactions));
    ret.push_back(Pair("lastblock", lastblock.GetHex()));

    return ret;
}

Value gettransaction(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)                        // MCHN
        throw runtime_error("Help message not found\n");

    uint256 hash;
    hash.SetHex(params[0].get_str());

    isminefilter filter = ISMINE_SPENDABLE;
    if(params.size() > 1)
        if(params[1].get_bool())
            filter = filter | ISMINE_WATCH_ONLY;

    Object entry;
    if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)
    {
        int err;
        const CWalletTx& wtx=pwalletTxsMain->GetWalletTx(hash,NULL,&err);
        if(err)
        {
            throw JSONRPCError(RPC_TX_NOT_FOUND, "Invalid or non-wallet transaction id");            
        }            
        CAmount nCredit = wtx.GetCredit(filter);
        CAmount nDebit = wtx.GetDebit(filter);
        CAmount nNet = nCredit - nDebit;
        CAmount nFee = (wtx.IsFromMe(filter) ? wtx.GetValueOut() - nDebit : 0);

        entry.push_back(Pair("amount", ValueFromAmount(nNet - nFee)));
        if (wtx.IsFromMe(filter))
            entry.push_back(Pair("fee", ValueFromAmount(nFee)));

        WalletTxToJSON(wtx, entry);

        Array details;
        ListTransactions(wtx, "*", 0, false, details, filter);
        entry.push_back(Pair("details", details));

        string strHex = EncodeHexTx(static_cast<CTransaction>(wtx));
        entry.push_back(Pair("hex", strHex));
    }
    else
    {
        if (!pwalletMain->mapWallet.count(hash))
            throw JSONRPCError(RPC_TX_NOT_FOUND, "Invalid or non-wallet transaction id");
        const CWalletTx& wtx = pwalletMain->mapWallet[hash];

        CAmount nCredit = wtx.GetCredit(filter);
        CAmount nDebit = wtx.GetDebit(filter);
        CAmount nNet = nCredit - nDebit;
        CAmount nFee = (wtx.IsFromMe(filter) ? wtx.GetValueOut() - nDebit : 0);

        entry.push_back(Pair("amount", ValueFromAmount(nNet - nFee)));
        if (wtx.IsFromMe(filter))
            entry.push_back(Pair("fee", ValueFromAmount(nFee)));

        WalletTxToJSON(wtx, entry);

        Array details;
        ListTransactions(wtx, "*", 0, false, details, filter);
        entry.push_back(Pair("details", details));

        string strHex = EncodeHexTx(static_cast<CTransaction>(wtx));
        entry.push_back(Pair("hex", strHex));
    }

    return entry;
}


Value backupwallet(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error("Help message not found\n");

    char bufOutput[MC_DCT_DB_MAX_PATH+1];
    char *full_path=__US_FullPath(params[0].get_str().c_str(),bufOutput,MC_DCT_DB_MAX_PATH+1);
    
    if(full_path)
    {
        char bufWallet[MC_DCT_DB_MAX_PATH+1];
        mc_GetFullFileName(mc_gState->m_NetworkParams->Name(),"wallet",".dat",MC_FOM_RELATIVE_TO_DATADIR,bufWallet);

        if(strcmp(full_path,bufWallet) == 0)
        {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Cannot backup wallet file to itself");        
        }
    }
    
    
//    string strDest = params[0].get_str();
    string strDest = strprintf("%s",full_path);
    if (!BackupWallet(*pwalletMain, strDest))
        throw JSONRPCError(RPC_WALLET_ERROR, "Error: Wallet backup failed!");

    return Value::null;
}


Value keypoolrefill(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error("Help message not found\n");

    // 0 is interpreted by TopUpKeyPool() as the default keypool size given by -keypool
    unsigned int kpSize = 0;
    if (params.size() > 0) {
        if (params[0].get_int() < 0)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, expected valid size.");
        kpSize = (unsigned int)params[0].get_int();
    }

    EnsureWalletIsUnlocked();
    pwalletMain->TopUpKeyPool(kpSize);

    if (pwalletMain->GetKeyPoolSize() < kpSize)
        throw JSONRPCError(RPC_WALLET_ERROR, "Error refreshing keypool.");

    return Value::null;
}


static void LockWallet(CWallet* pWallet)
{
    LOCK(cs_nWalletUnlockTime);
    nWalletUnlockTime = 0;
    pWallet->Lock();
}

Value walletpassphrase(const Array& params, bool fHelp)
{
    if (pwalletMain->IsCrypted() && (fHelp || params.size() != 2))              // MCHN
        throw runtime_error("Help message not found\n");

    if (fHelp)
        return true;
    if (!pwalletMain->IsCrypted())
        throw JSONRPCError(RPC_WALLET_WRONG_ENC_STATE, "Error: running with an unencrypted wallet, but walletpassphrase was called.");

    // Note that the walletpassphrase is stored in params[0] which is not mlock()ed
    SecureString strWalletPass;
    strWalletPass.reserve(100);
    // TODO: get rid of this .c_str() by implementing SecureString::operator=(std::string)
    // Alternately, find a way to make params[0] mlock()'d to begin with.
    strWalletPass = params[0].get_str().c_str();

    if (strWalletPass.length() > 0)
    {
        if (!pwalletMain->Unlock(strWalletPass))
            throw JSONRPCError(RPC_WALLET_PASSPHRASE_INCORRECT, "Error: The wallet passphrase entered was incorrect.");
    }
    else
        throw runtime_error(
            "walletpassphrase <passphrase> <timeout>\n"
            "Stores the wallet decryption key in memory for <timeout> seconds.");

    pwalletMain->TopUpKeyPool();

    int64_t nSleepTime = params[1].get_int64();
    LOCK(cs_nWalletUnlockTime);
    nWalletUnlockTime = GetTime() + nSleepTime;
    RPCRunLater("lockwallet", boost::bind(LockWallet, pwalletMain), nSleepTime);

    return Value::null;
}


Value walletpassphrasechange(const Array& params, bool fHelp)
{
    if (pwalletMain->IsCrypted() && (fHelp || params.size() != 2))
        throw runtime_error("Help message not found\n");

    if (fHelp)
        return true;
    if (!pwalletMain->IsCrypted())
        throw JSONRPCError(RPC_WALLET_WRONG_ENC_STATE, "Error: running with an unencrypted wallet, but walletpassphrasechange was called.");

    // TODO: get rid of these .c_str() calls by implementing SecureString::operator=(std::string)
    // Alternately, find a way to make params[0] mlock()'d to begin with.
    SecureString strOldWalletPass;
    strOldWalletPass.reserve(100);
    strOldWalletPass = params[0].get_str().c_str();

    SecureString strNewWalletPass;
    strNewWalletPass.reserve(100);
    strNewWalletPass = params[1].get_str().c_str();

    if (strOldWalletPass.length() < 1 || strNewWalletPass.length() < 1)
        throw runtime_error(
            "walletpassphrasechange <oldpassphrase> <newpassphrase>\n"
            "Changes the wallet passphrase from <oldpassphrase> to <newpassphrase>.");

    if (!pwalletMain->ChangeWalletPassphrase(strOldWalletPass, strNewWalletPass))
        throw JSONRPCError(RPC_WALLET_PASSPHRASE_INCORRECT, "Error: The wallet passphrase entered was incorrect.");

    return Value::null;
}


Value walletlock(const Array& params, bool fHelp)
{
    if (pwalletMain->IsCrypted() && (fHelp || params.size() != 0))
        throw runtime_error("Help message not found\n");

    if (fHelp)
        return true;
    if (!pwalletMain->IsCrypted())
        throw JSONRPCError(RPC_WALLET_WRONG_ENC_STATE, "Error: running with an unencrypted wallet, but walletlock was called.");

    {
        LOCK(cs_nWalletUnlockTime);
        pwalletMain->Lock();
        nWalletUnlockTime = 0;
    }

    return Value::null;
}


Value encryptwallet(const Array& params, bool fHelp)
{
    if (!pwalletMain->IsCrypted() && (fHelp || params.size() != 1))             // MCHN
        throw runtime_error("Help message not found\n");

    if (fHelp)
        return true;
    if (pwalletMain->IsCrypted())
        throw JSONRPCError(RPC_WALLET_WRONG_ENC_STATE, "Error: running with an encrypted wallet, but encryptwallet was called.");

    // TODO: get rid of this .c_str() by implementing SecureString::operator=(std::string)
    // Alternately, find a way to make params[0] mlock()'d to begin with.
    SecureString strWalletPass;
    strWalletPass.reserve(100);
    strWalletPass = params[0].get_str().c_str();

    if (strWalletPass.length() < 1)
        throw runtime_error(
            "encryptwallet <passphrase>\n"
            "Encrypts the wallet with <passphrase>.");

    if (!pwalletMain->EncryptWallet(strWalletPass))
        throw JSONRPCError(RPC_WALLET_ENCRYPTION_FAILED, "Error: Failed to encrypt the wallet.");

    // BDB seems to have a bad habit of writing old data into
    // slack space in .dat files; that is bad if the old data is
    // unencrypted private keys. So:
    StartShutdown();                                                            // MCHN
    return "wallet encrypted; server stopping, restart to run with encrypted wallet. The keypool has been flushed, you need to make a new backup.";
}

Value lockunspent(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)                        // MCHN
        throw runtime_error("Help message not found\n");

    if (params.size() == 1)
        RPCTypeCheck(params, list_of(bool_type));
    else
        RPCTypeCheck(params, list_of(bool_type)(array_type));

    bool fUnlock = params[0].get_bool();

    if (params.size() == 1) {
        if (fUnlock)
            pwalletMain->UnlockAllCoins();
        return true;
    }

    Array outputs = params[1].get_array();
    BOOST_FOREACH(Value& output, outputs)
    {
        if (output.type() != obj_type)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, expected object");
        const Object& o = output.get_obj();

        RPCTypeCheck(o, map_list_of("txid", str_type)("vout", int_type));

        string txid = find_value(o, "txid").get_str();
        if (!IsHex(txid))
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, expected hex txid");

        int nOutput = find_value(o, "vout").get_int();
        if (nOutput < 0)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, vout must be positive");

        COutPoint outpt(uint256(txid), nOutput);

        if (fUnlock)
            pwalletMain->UnlockCoin(outpt);
        else
            pwalletMain->LockCoin(outpt);
    }

    return true;
}

Value listlockunspent(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 0)
        throw runtime_error("Help message not found\n");

    vector<COutPoint> vOutpts;
    pwalletMain->ListLockedCoins(vOutpts);

    Array ret;

    BOOST_FOREACH(COutPoint &outpt, vOutpts) {
        Object o;

        o.push_back(Pair("txid", outpt.hash.GetHex()));
        o.push_back(Pair("vout", (int)outpt.n));
        ret.push_back(o);
    }

    return ret;
}

Value settxfee(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 1)                        // MCHN
        throw runtime_error("Help message not found\n");

    // Amount
    CAmount nAmount = 0;
    if (params[0].get_real() != 0.0)
        nAmount = AmountFromValue(params[0]);        // rejects 0.0 amounts

    payTxFee = CFeeRate(nAmount, 1000);
    return true;
}

Value getwalletinfo(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)                                            // MCHN
        throw runtime_error("Help message not found\n");

    Object obj;
    obj.push_back(Pair("walletversion", pwalletMain->GetVersion()));
    obj.push_back(Pair("balance",       ValueFromAmount(pwalletMain->GetBalance())));
    obj.push_back(Pair("walletdbversion", mc_gState->GetWalletDBVersion()));                
    if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS) 
    {
        obj.push_back(Pair("txcount",(int)pwalletTxsMain->m_Database->m_DBStat.m_Count+pwalletTxsMain->m_Database->m_RawMemPools[0]->GetCount()));        
    }
    else
    {
        obj.push_back(Pair("txcount",       (int)pwalletMain->mapWallet.size()));
    }
    vector<COutput> vecOutputs;
    pwalletMain->AvailableCoins(vecOutputs, false, NULL, false,true);
    obj.push_back(Pair("utxocount",  (int)vecOutputs.size()));                
    obj.push_back(Pair("keypoololdest", pwalletMain->GetOldestKeyPoolTime()));
    obj.push_back(Pair("keypoolsize",   (int)pwalletMain->GetKeyPoolSize()));
    if (pwalletMain->IsCrypted())
        obj.push_back(Pair("unlocked_until", nWalletUnlockTime));
    return obj;
}
