// Copyright (c) 2014-2016 The Bitcoin Core developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "structs/base58.h"
#include "rpc/rpcserver.h"
#include "core/init.h"
#include "core/main.h"
#include "script/script.h"
#include "script/standard.h"
#include "utils/sync.h"
#include "utils/util.h"
#include "utils/utiltime.h"
#include "wallet/wallet.h"
/* MCHN START */
#include "wallet/wallettxs.h"
#include "rpc/rpcutils.h"
/* MCHN END */

#include <fstream>
#include <stdint.h>

#include <boost/algorithm/string.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include "json/json_spirit_value.h"

using namespace json_spirit;
using namespace std;

void EnsureWalletIsUnlocked();
/* MCHN START */
vector<string> ParseStringList(Value param);
char * __US_FullPath(const char* path, char *full_path, int len);
/* MCHN END */

std::string static EncodeDumpTime(int64_t nTime) {
    return DateTimeStrFormat("%Y-%m-%dT%H:%M:%SZ", nTime);
}

int64_t static DecodeDumpTime(const std::string &str) {
    static const boost::posix_time::ptime epoch = boost::posix_time::from_time_t(0);
    static const std::locale loc(std::locale::classic(),
        new boost::posix_time::time_input_facet("%Y-%m-%dT%H:%M:%SZ"));
    std::istringstream iss(str);
    iss.imbue(loc);
    boost::posix_time::ptime ptime(boost::date_time::not_a_date_time);
    iss >> ptime;
    if (ptime.is_not_a_date_time())
        return 0;
    return (ptime - epoch).total_seconds();
}

std::string static EncodeDumpString(const std::string &str) {
    std::stringstream ret;
    BOOST_FOREACH(unsigned char c, str) {
        if (c <= 32 || c >= 128 || c == '%') {
            ret << '%' << HexStr(&c, &c + 1);
        } else {
            ret << c;
        }
    }
    return ret.str();
}

std::string DecodeDumpString(const std::string &str) {
    std::stringstream ret;
    for (unsigned int pos = 0; pos < str.length(); pos++) {
        unsigned char c = str[pos];
        if (c == '%' && pos+2 < str.length()) {
            c = (((str[pos+1]>>6)*9+((str[pos+1]-'0')&15)) << 4) | 
                ((str[pos+2]>>6)*9+((str[pos+2]-'0')&15));
            pos += 2;
        }
        ret << c;
    }
    return ret.str();
}

Value importprivkey(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 3)
        throw runtime_error("Help message not found\n");

    EnsureWalletIsUnlocked();

    string strLabel = "";
    if (params.size() > 1)
        strLabel = params[1].get_str();

    // Whether to perform rescan after import
    bool fRescan = true;
    int start_block=0;
    if (params.size() > 2)
    {        
        start_block=ParseRescanParameter(params[2],&fRescan);
    }

    
    bool fNewFound=false;
    vector<string> inputStrings=ParseStringList(params[0]);
    vector<CKey> inputKeys;
    vector<CPubKey> inputPubKeys;
    vector<CKeyID> inputKeyIDs;
    
    for(int is=0;is<(int)inputStrings.size();is++)
    {
        string strSecret=inputStrings[is];
        
        CBitcoinSecret vchSecret;
        bool fGood = vchSecret.SetString(strSecret);

        if (!fGood) throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid private key encoding");

        CKey key = vchSecret.GetKey();
        if (!key.IsValid()) throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Private key outside allowed range");                
        
        CPubKey pubkey = key.GetPubKey();
        assert(key.VerifyPubKey(pubkey));
        CKeyID vchAddress = pubkey.GetID();
        
        inputKeys.push_back(key);
        inputPubKeys.push_back(pubkey);
        inputKeyIDs.push_back(vchAddress);
        
        if (!pwalletMain->HaveKey(vchAddress))
        {
            fNewFound=true;
        }
    }   
    
    if(!fNewFound)
    {
        return Value::null;        
    }
    
    pwalletMain->MarkDirty();
    
    for(int is=0;is<(int)inputStrings.size();is++)
    {
        CKey key = inputKeys[is];
        CPubKey pubkey = inputPubKeys[is];
        CKeyID vchAddress = inputKeyIDs[is];

        pwalletMain->SetAddressBook(vchAddress, strLabel, "receive");

        // Don't throw error in case a key is already there
        if (!pwalletMain->HaveKey(vchAddress))
        {
            pwalletMain->mapKeyMetadata[vchAddress].nCreateTime = 1;

            if (!pwalletMain->AddKeyPubKey(key, pubkey))
                throw JSONRPCError(RPC_WALLET_ERROR, "Error adding key to wallet");

            // whenever a key is imported, we need to scan the whole chain
            pwalletMain->nTimeFirstKey = 1; // 0 would be considered 'no value'

            if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)
            {
                mc_TxEntity entity;
                const CKeyID& KeyID=pubkey.GetID();

                memcpy(entity.m_EntityID,&KeyID,MC_TDB_ENTITY_ID_SIZE);
                entity.m_EntityType=MC_TET_PUBKEY_ADDRESS | MC_TET_CHAINPOS;
                pwalletTxsMain->AddEntity(&entity,MC_EFL_NOT_IN_SYNC);
                entity.m_EntityType=MC_TET_PUBKEY_ADDRESS | MC_TET_TIMERECEIVED;
                pwalletTxsMain->AddEntity(&entity,MC_EFL_NOT_IN_SYNC);
            }            
        }    
    }    
    
    if (fRescan) {
        pwalletMain->ScanForWalletTransactions(chainActive[start_block], true, true);
    }
    
    return Value::null;
}

Value importaddress(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 3)
        throw runtime_error("Help message not found\n");

    CScript script;
    
    string strLabel = "";
    if (params.size() > 1)
        strLabel = params[1].get_str();

    // Whether to perform rescan after import
    bool fRescan = true;
    int start_block=0;
    if (params.size() > 2)
    {        
        start_block=ParseRescanParameter(params[2],&fRescan);
    }

    bool fNewFound=false;
    vector<string> inputStrings=ParseStringList(params[0]);
    vector<CBitcoinAddress> inputAddresses;
    vector<CScript> inputScripts;
    
    for(int is=0;is<(int)inputStrings.size();is++)
    {
        string param=inputStrings[is];
        
        CBitcoinAddress address(param);
        if (address.IsValid()) {
            script = GetScriptForDestination(address.Get());
        } else if (IsHex(param)) {
            std::vector<unsigned char> data(ParseHex(param));
            script = CScript(data.begin(), data.end());
        } else {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid MultiChain address or script");
        }

        inputAddresses.push_back(address);
        inputScripts.push_back(script);
        
        if( (mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS) == 0 )
        {
            if (::IsMine(*pwalletMain, script) == ISMINE_SPENDABLE)
            {
    //            throw JSONRPCError(RPC_WALLET_ERROR, "The wallet already contains the private key for this address or script");
                return Value::null;    
            }
        }
        
        if (!pwalletMain->HaveWatchOnly(script))
        {
            fNewFound=true;
        }
    }

    if( (mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS) == 0 )
    {
        if(!fNewFound)
        {
            return Value::null;        
        }
    }
    
    pwalletMain->MarkDirty();
    
    for(int is=0;is<(int)inputStrings.size();is++)
    {
        CBitcoinAddress address=inputAddresses[is];
        script=inputScripts[is];
        
        // add to address book or update label
        if (address.IsValid())
        {
            pwalletMain->SetAddressBook(address.Get(), strLabel, "receive");
        }
        else
        {
            CScriptID innerID(script);
            address=CBitcoinAddress(innerID);
        }
        
        // Don't throw error in case an address is already there
        if (!pwalletMain->HaveWatchOnly(script))
        {
            if (!pwalletMain->AddWatchOnly(script))
                throw JSONRPCError(RPC_WALLET_ERROR, "Error adding address to wallet");            

            if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)
            {
                mc_TxEntity entity;
                CTxDestination addressRet=address.Get();        
                const CKeyID *lpKeyID=boost::get<CKeyID> (&addressRet);
                const CScriptID *lpScriptID=boost::get<CScriptID> (&addressRet);

                if(lpKeyID)
                {
                    memcpy(entity.m_EntityID,lpKeyID,MC_TDB_ENTITY_ID_SIZE);
                    entity.m_EntityType=MC_TET_PUBKEY_ADDRESS | MC_TET_CHAINPOS;
                    pwalletTxsMain->AddEntity(&entity,MC_EFL_NOT_IN_SYNC);
                    entity.m_EntityType=MC_TET_PUBKEY_ADDRESS | MC_TET_TIMERECEIVED;
                    pwalletTxsMain->AddEntity(&entity,MC_EFL_NOT_IN_SYNC);
                }
                if(lpScriptID)
                {
                    memcpy(entity.m_EntityID,lpScriptID,MC_TDB_ENTITY_ID_SIZE);
                    entity.m_EntityType=MC_TET_SCRIPT_ADDRESS | MC_TET_CHAINPOS;
                    pwalletTxsMain->AddEntity(&entity,MC_EFL_NOT_IN_SYNC);
                    entity.m_EntityType=MC_TET_SCRIPT_ADDRESS | MC_TET_TIMERECEIVED;
                    pwalletTxsMain->AddEntity(&entity,MC_EFL_NOT_IN_SYNC);                    
                }
            }
        }
    }        
        
    {
        if (fRescan)
        {
            pwalletMain->ScanForWalletTransactions(chainActive[start_block], true, true);
            pwalletMain->ReacceptWalletTransactions();
        }
    }

    return Value::null;
}

Value importwallet(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error("Help message not found\n");

    EnsureWalletIsUnlocked();

    bool fRescan = true;
    int start_block=0;
    if (params.size() > 1)
    {        
        start_block=ParseRescanParameter(params[1],&fRescan);
    }
    
    ifstream file;
    file.open(params[0].get_str().c_str(), std::ios::in | std::ios::ate);
    if (!file.is_open())
        throw JSONRPCError(RPC_GENERAL_FILE_ERROR, "Cannot open wallet dump file");

    int64_t nTimeBegin = chainActive.Tip()->GetBlockTime();

    bool fGood = true;

    int64_t nFilesize = std::max((int64_t)1, (int64_t)file.tellg());
    file.seekg(0, file.beg);

    pwalletMain->ShowProgress(_("Importing..."), 0); // show progress dialog in GUI
    while (file.good()) {
        pwalletMain->ShowProgress("", std::max(1, std::min(99, (int)(((double)file.tellg() / (double)nFilesize) * 100))));
        std::string line;
        std::getline(file, line);
        if (line.empty() || line[0] == '#')
            continue;

        std::vector<std::string> vstr;
        boost::split(vstr, line, boost::is_any_of(" "));
        if (vstr.size() < 2)
            continue;
        CBitcoinSecret vchSecret;
        if (!vchSecret.SetString(vstr[0]))
            continue;
        CKey key = vchSecret.GetKey();
        CPubKey pubkey = key.GetPubKey();
        assert(key.VerifyPubKey(pubkey));
        CKeyID keyid = pubkey.GetID();
        bool fAlreadyHave=false;
        if (pwalletMain->HaveKey(keyid)) {
            LogPrintf("Skipping import of %s (key already present)\n", CBitcoinAddress(keyid).ToString());
//                continue;
            fAlreadyHave=true;
        }
        int64_t nTime = DecodeDumpTime(vstr[1]);
        std::string strLabel;
        bool fLabel = true;
        for (unsigned int nStr = 2; nStr < vstr.size(); nStr++) {
            if (boost::algorithm::starts_with(vstr[nStr], "#"))
                break;
            if (vstr[nStr] == "change=1")
                fLabel = false;
            if (vstr[nStr] == "reserve=1")
                fLabel = false;
            if (boost::algorithm::starts_with(vstr[nStr], "label=")) {
                strLabel = DecodeDumpString(vstr[nStr].substr(6));
                fLabel = true;
            }
        }
        LogPrintf("Importing %s...\n", CBitcoinAddress(keyid).ToString());
        if(!fAlreadyHave)
        {
            if (!pwalletMain->AddKeyPubKey(key, pubkey)) {
                fGood = false;
                continue;
            }
            pwalletMain->mapKeyMetadata[keyid].nCreateTime = nTime;
        }
        if (fLabel)
            pwalletMain->SetAddressBook(keyid, strLabel, "receive");
        
/* MCHN START */        
        if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)
        {
            mc_TxEntity entity;
            const CKeyID& KeyID=pubkey.GetID();

            memcpy(entity.m_EntityID,&KeyID,MC_TDB_ENTITY_ID_SIZE);
            entity.m_EntityType=MC_TET_PUBKEY_ADDRESS | MC_TET_CHAINPOS;
            pwalletTxsMain->AddEntity(&entity,MC_EFL_NOT_IN_SYNC);
            entity.m_EntityType=MC_TET_PUBKEY_ADDRESS | MC_TET_TIMERECEIVED;
            pwalletTxsMain->AddEntity(&entity,MC_EFL_NOT_IN_SYNC);
        }
/* MCHN END */        
        
        nTimeBegin = std::min(nTimeBegin, nTime);
    }
    file.close();
    pwalletMain->ShowProgress("", 100); // hide progress dialog in GUI

    CBlockIndex *pindex = chainActive.Tip();
    while (pindex && pindex->pprev && pindex->GetBlockTime() > nTimeBegin - 12 * Params().TargetSpacing()) // MCHN
        pindex = pindex->pprev;

    if (!pwalletMain->nTimeFirstKey || nTimeBegin < pwalletMain->nTimeFirstKey)
        pwalletMain->nTimeFirstKey = nTimeBegin;

/* MCHN START */        
    if(fRescan)
    {
        if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)
        {
            if(start_block)
            {
                LogPrintf("Rescanning last %i blocks\n", chainActive.Height()-start_block+1);
            }
            else
            {
                LogPrintf("Rescanning all %i blocks\n", chainActive.Height());
            }
            pwalletMain->ScanForWalletTransactions(chainActive[start_block],false,true);
        }
        else
        {
            LogPrintf("Rescanning last %i blocks\n", chainActive.Height() - pindex->nHeight + 1);
            pwalletMain->ScanForWalletTransactions(pindex,false,true);
        }
    }
    pwalletMain->MarkDirty();        

    if (!fGood)
        throw JSONRPCError(RPC_WALLET_ERROR, "Error adding some keys to wallet");

    return Value::null;
}

Value dumpprivkey(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error("Help message not found\n");

    EnsureWalletIsUnlocked();

    string strAddress = params[0].get_str();
    CBitcoinAddress address;
    if (!address.SetString(strAddress))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid MultiChain address");
    CKeyID keyID;
    if (!address.GetKeyID(keyID))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Address does not refer to a key");
    CKey vchSecret;
    if (!pwalletMain->GetKey(keyID, vchSecret))
        throw JSONRPCError(RPC_WALLET_ADDRESS_NOT_FOUND, "Private key for address " + strAddress + " is not known");
    return CBitcoinSecret(vchSecret).ToString();
}


Value dumpwallet(const Array& params, bool fHelp)
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
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Cannot dump wallet file to itself");        
        }
    }
    
    EnsureWalletIsUnlocked();

    ofstream file;
    file.open(params[0].get_str().c_str());
    if (!file.is_open())
        throw JSONRPCError(RPC_GENERAL_FILE_ERROR, "Cannot open wallet dump file");

    std::map<CKeyID, int64_t> mapKeyBirth;
    std::set<CKeyID> setKeyPool;
    pwalletMain->GetKeyBirthTimes(mapKeyBirth);
    pwalletMain->GetAllReserveKeys(setKeyPool);

    // sort time/key pairs
    std::vector<std::pair<int64_t, CKeyID> > vKeyBirth;
    for (std::map<CKeyID, int64_t>::const_iterator it = mapKeyBirth.begin(); it != mapKeyBirth.end(); it++) {
        bool take_it=true;
        std::map<CTxDestination, CAddressBookData>::iterator mi = pwalletMain->mapAddressBook.find(it->first);
        if(mi != pwalletMain->mapAddressBook.end())
        {
            if(mi->second.purpose == "license")
            {
                take_it=false;
            }
        }
        if(take_it)
        {
            vKeyBirth.push_back(std::make_pair(it->second, it->first));
        }
    }
    mapKeyBirth.clear();
    std::sort(vKeyBirth.begin(), vKeyBirth.end());

    // produce output
    file << strprintf("# Wallet dump created by MultiChain %s (%s)\n", CLIENT_BUILD, CLIENT_DATE);
    file << strprintf("# * Created on %s\n", EncodeDumpTime(GetTime()));
    file << strprintf("# * Best block at time of backup was %i (%s),\n", chainActive.Height(), chainActive.Tip()->GetBlockHash().ToString());
    file << strprintf("#   mined on %s\n", EncodeDumpTime(chainActive.Tip()->GetBlockTime()));
    file << "\n";
    for (std::vector<std::pair<int64_t, CKeyID> >::const_iterator it = vKeyBirth.begin(); it != vKeyBirth.end(); it++) {
        const CKeyID &keyid = it->second;
        std::string strTime = EncodeDumpTime(it->first);
        std::string strAddr = CBitcoinAddress(keyid).ToString();
        CKey key;
        if (pwalletMain->GetKey(keyid, key)) {
            if (pwalletMain->mapAddressBook.count(keyid)) {
                file << strprintf("%s %s label=%s # addr=%s\n", CBitcoinSecret(key).ToString(), strTime, EncodeDumpString(pwalletMain->mapAddressBook[keyid].name), strAddr);
            } else if (setKeyPool.count(keyid)) {
                file << strprintf("%s %s reserve=1 # addr=%s\n", CBitcoinSecret(key).ToString(), strTime, strAddr);
            } else {
                file << strprintf("%s %s change=1 # addr=%s\n", CBitcoinSecret(key).ToString(), strTime, strAddr);
            }
        }
    }
    file << "\n";
    file << "# End of dump\n";
    file.close();
    return Value::null;
}
