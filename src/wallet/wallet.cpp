// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2014-2016 The Bitcoin Core developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "wallet/wallet.h"
/* MCHN START */
#include "wallet/wallettxs.h"
extern mc_WalletTxs* pwalletTxsMain;
/* MCHN END */

#include "structs/base58.h"
#include "chain/checkpoints.h"
#include "wallet/coincontrol.h"
#include "net/net.h"
#include "script/script.h"
#include "script/sign.h"
#include "utils/timedata.h"
#include "utils/util.h"
#include "utils/utilmoneystr.h"
#include "community/community.h"

#include <assert.h>

#include <boost/algorithm/string/replace.hpp>
#include <boost/thread.hpp>

using namespace std;

/**
 * Settings
 */

//! -maxtxfee default
static const CAmount DEFAULT_TRANSACTION_MAXFEE = 0.1 * COIN;

CFeeRate payTxFee(DEFAULT_TRANSACTION_FEE);
CAmount maxTxFee = DEFAULT_TRANSACTION_MAXFEE;
unsigned int nTxConfirmTarget = 1;
bool bSpendZeroConfChange = true;
bool fSendFreeTransactions = false;
bool fPayAtLeastCustomFee = true;

/** 
 * Fees smaller than this (in satoshi) are considered zero fee (for transaction creation) 
 * Override with -mintxfee
 */

/* MCHN START */
//CFeeRate CWallet::minTxFee = CFeeRate(1000);
CFeeRate CWallet::minTxFee = CFeeRate(MIN_RELAY_TX_FEE);
bool OutputCanSend(COutput out);

struct CompareValueOnlyIntDesc
{
    bool operator()(const pair<int, pair<const CWalletTx*, unsigned int> >& t1,
                    const pair<int, pair<const CWalletTx*, unsigned int> >& t2) const
    {
        return t1.first > t2.first;
    }
};

int64_t mc_GetABCoinQuantity(void *ptr,int coin_id);
void mc_SetABCoinQuantity(void *ptr,int coin_id,int64_t quantity);

/* MCHN END */




/** @defgroup mapWallet
 *
 * @{
 */

struct CompareValueOnly
{
    bool operator()(const pair<CAmount, pair<const CWalletTx*, unsigned int> >& t1,
                    const pair<CAmount, pair<const CWalletTx*, unsigned int> >& t2) const
    {
        return t1.first < t2.first;
    }
};
struct CompareValueOnlyHash
{
    bool operator()(const pair<CAmount, pair<uint256, unsigned int> >& t1,
                    const pair<CAmount, pair<uint256, unsigned int> >& t2) const
    {
        return t1.first < t2.first;
    }
};


const CWalletTx* CWallet::GetWalletTx(const uint256& hash) const
{
    LOCK(cs_wallet);
//    if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)                          // Not supported as returns pointer
    
    std::map<uint256, CWalletTx>::const_iterator it = mapWallet.find(hash);
    if (it == mapWallet.end())
        return NULL;
    return &(it->second);
}

CPubKey CWallet::GenerateNewKey()
{
    AssertLockHeld(cs_wallet); // mapKeyMetadata
    bool fCompressed = CanSupportFeature(FEATURE_COMPRPUBKEY); // default to compressed public keys if we want 0.6.0 wallets

//    RandAddSeedPerfmon();                                                     // MCHN removed in 146c0a7 when merging sec256k1
    CKey secret;
    secret.MakeNewKey(fCompressed);

    // Compressed public keys were introduced in version 0.6.0
    if (fCompressed)
        SetMinVersion(FEATURE_COMPRPUBKEY);

    CPubKey pubkey = secret.GetPubKey();
    assert(secret.VerifyPubKey(pubkey));

    // Create new metadata
    int64_t nCreationTime = GetTime();
    mapKeyMetadata[pubkey.GetID()] = CKeyMetadata(nCreationTime);
    if (!nTimeFirstKey || nCreationTime < nTimeFirstKey)
        nTimeFirstKey = nCreationTime;

    if (!AddKeyPubKey(secret, pubkey))
        throw std::runtime_error("CWallet::GenerateNewKey() : AddKey failed");
    return pubkey;
}

bool CWallet::AddKeyPubKey(const CKey& secret, const CPubKey &pubkey)
{
    AssertLockHeld(cs_wallet); // mapKeyMetadata
    if (!CCryptoKeyStore::AddKeyPubKey(secret, pubkey))
        return false;

    // check if we need to remove from watch-only
    CScript script;
    script = GetScriptForDestination(pubkey.GetID());
    if (HaveWatchOnly(script))
        RemoveWatchOnly(script);

    if (!fFileBacked)
        return true;
    if (!IsCrypted()) {
        return CWalletDB(strWalletFile).WriteKey(pubkey,
                                                 secret.GetPrivKey(),
                                                 mapKeyMetadata[pubkey.GetID()]);
    }
    return true;
}

string CWallet::SetDefaultKeyIfInvalid(std::string init_privkey)
{
    if(vchDefaultKey.IsValid())
    {
        return "Wallet already has default key";
    }
    
    CBitcoinSecret vchSecret;
    if(!vchSecret.SetString(init_privkey))
    {
        if(mc_gState->m_NetworkParams->GetParam("privatekeyversion",NULL) == NULL)
        {
            return "Private key version is not set, please chose seed node running at least MultiChain 1.0 beta 2";            
        }
        return "Invalid private key encoding";
    }

    CKey key = vchSecret.GetKey();
    if (!key.IsValid())
    {
        return "Private key outside allowed range";        
    }

    CPubKey pubkey = key.GetPubKey();
    assert(key.VerifyPubKey(pubkey));
    CKeyID vchAddress = pubkey.GetID();

    if(!HaveKey(vchAddress))
    {
        mapKeyMetadata[vchAddress].nCreateTime = 1;

        if (!AddKeyPubKey(key, pubkey))
        {
            return "Error adding key to wallet";                    
        }

        // whenever a key is imported, we need to scan the whole chain
        nTimeFirstKey = 1; // 0 would be considered 'no value'        
    }
    
    if(!SetDefaultKey(pubkey))
    {
        return "Error setting default key";                            
    }
    
    return "";
}


bool CWallet::AddCryptedKey(const CPubKey &vchPubKey,
                            const vector<unsigned char> &vchCryptedSecret)
{
    if (!CCryptoKeyStore::AddCryptedKey(vchPubKey, vchCryptedSecret))
        return false;
    if (!fFileBacked)
        return true;
    {
        LOCK(cs_wallet);
        if (pwalletdbEncryption)
            return pwalletdbEncryption->WriteCryptedKey(vchPubKey,
                                                        vchCryptedSecret,
                                                        mapKeyMetadata[vchPubKey.GetID()]);
        else
            return CWalletDB(strWalletFile).WriteCryptedKey(vchPubKey,
                                                            vchCryptedSecret,
                                                            mapKeyMetadata[vchPubKey.GetID()]);
    }
    return false;
}

bool CWallet::LoadKeyMetadata(const CPubKey &pubkey, const CKeyMetadata &meta)
{
    AssertLockHeld(cs_wallet); // mapKeyMetadata
    if (meta.nCreateTime && (!nTimeFirstKey || meta.nCreateTime < nTimeFirstKey))
        nTimeFirstKey = meta.nCreateTime;

    mapKeyMetadata[pubkey.GetID()] = meta;
    return true;
}

bool CWallet::LoadCryptedKey(const CPubKey &vchPubKey, const std::vector<unsigned char> &vchCryptedSecret)
{
    return CCryptoKeyStore::AddCryptedKey(vchPubKey, vchCryptedSecret);
}

bool CWallet::AddCScript(const CScript& redeemScript)
{
    if (!CCryptoKeyStore::AddCScript(redeemScript))
        return false;
    if (!fFileBacked)
        return true;
    return CWalletDB(strWalletFile).WriteCScript(Hash160(redeemScript), redeemScript);
}

bool CWallet::LoadCScript(const CScript& redeemScript)
{
    /* A sanity check was added in pull #3843 to avoid adding redeemScripts
     * that never can be redeemed. However, old wallets may still contain
     * these. Do not add them to the wallet and warn. */
    if (redeemScript.size() > MAX_SCRIPT_ELEMENT_SIZE)
    {
        std::string strAddr = CBitcoinAddress(CScriptID(redeemScript)).ToString();
        LogPrintf("%s: Warning: This wallet contains a redeemScript of size %i which exceeds maximum size %i thus can never be redeemed. Do not use address %s.\n",
            __func__, redeemScript.size(), MAX_SCRIPT_ELEMENT_SIZE, strAddr);
        return true;
    }

    return CCryptoKeyStore::AddCScript(redeemScript);
}

bool CWallet::AddWatchOnly(const CScript &dest)
{
    if (!CCryptoKeyStore::AddWatchOnly(dest))
        return false;
    nTimeFirstKey = 1; // No birthday information for watch-only keys.
    NotifyWatchonlyChanged(true);
    if (!fFileBacked)
        return true;
    return CWalletDB(strWalletFile).WriteWatchOnly(dest);
}

bool CWallet::RemoveWatchOnly(const CScript &dest)
{
    AssertLockHeld(cs_wallet);
    if (!CCryptoKeyStore::RemoveWatchOnly(dest))
        return false;
    if (!HaveWatchOnly())
        NotifyWatchonlyChanged(false);
    if (fFileBacked)
        if (!CWalletDB(strWalletFile).EraseWatchOnly(dest))
            return false;

    return true;
}

bool CWallet::LoadWatchOnly(const CScript &dest)
{
    return CCryptoKeyStore::AddWatchOnly(dest);
}

bool CWallet::Unlock(const SecureString& strWalletPassphrase)
{
    CCrypter crypter;
    CKeyingMaterial vMasterKey;

    {
        LOCK(cs_wallet);
        BOOST_FOREACH(const MasterKeyMap::value_type& pMasterKey, mapMasterKeys)
        {
            if(!crypter.SetKeyFromPassphrase(strWalletPassphrase, pMasterKey.second.vchSalt, pMasterKey.second.nDeriveIterations, pMasterKey.second.nDerivationMethod))
                return false;
            if (!crypter.Decrypt(pMasterKey.second.vchCryptedKey, vMasterKey))
                continue; // try another master key
            if (CCryptoKeyStore::Unlock(vMasterKey))
                return true;
        }
    }
    return false;
}

bool CWallet::ChangeWalletPassphrase(const SecureString& strOldWalletPassphrase, const SecureString& strNewWalletPassphrase)
{
    bool fWasLocked = IsLocked();

    {
        LOCK(cs_wallet);
        Lock();

        CCrypter crypter;
        CKeyingMaterial vMasterKey;
        BOOST_FOREACH(MasterKeyMap::value_type& pMasterKey, mapMasterKeys)
        {
            if(!crypter.SetKeyFromPassphrase(strOldWalletPassphrase, pMasterKey.second.vchSalt, pMasterKey.second.nDeriveIterations, pMasterKey.second.nDerivationMethod))
                return false;
            if (!crypter.Decrypt(pMasterKey.second.vchCryptedKey, vMasterKey))
                return false;
            if (CCryptoKeyStore::Unlock(vMasterKey))
            {
                int64_t nStartTime = GetTimeMillis();
                crypter.SetKeyFromPassphrase(strNewWalletPassphrase, pMasterKey.second.vchSalt, pMasterKey.second.nDeriveIterations, pMasterKey.second.nDerivationMethod);
                pMasterKey.second.nDeriveIterations = pMasterKey.second.nDeriveIterations * (100 / ((double)(GetTimeMillis() - nStartTime)));

                nStartTime = GetTimeMillis();
                crypter.SetKeyFromPassphrase(strNewWalletPassphrase, pMasterKey.second.vchSalt, pMasterKey.second.nDeriveIterations, pMasterKey.second.nDerivationMethod);
                pMasterKey.second.nDeriveIterations = (pMasterKey.second.nDeriveIterations + pMasterKey.second.nDeriveIterations * 100 / ((double)(GetTimeMillis() - nStartTime))) / 2;

                if (pMasterKey.second.nDeriveIterations < 25000)
                    pMasterKey.second.nDeriveIterations = 25000;

                LogPrintf("Wallet passphrase changed to an nDeriveIterations of %i\n", pMasterKey.second.nDeriveIterations);

                if (!crypter.SetKeyFromPassphrase(strNewWalletPassphrase, pMasterKey.second.vchSalt, pMasterKey.second.nDeriveIterations, pMasterKey.second.nDerivationMethod))
                    return false;
                if (!crypter.Encrypt(vMasterKey, pMasterKey.second.vchCryptedKey))
                    return false;
                CWalletDB(strWalletFile).WriteMasterKey(pMasterKey.first, pMasterKey.second);
                if (fWasLocked)
                    Lock();
                return true;
            }
        }
    }

    return false;
}

void CWallet::SetBestChain(const CBlockLocator& loc)
{
    if( (mc_gState->m_WalletMode & MC_WMD_TXS) == 0 )
    {                
        CWalletDB walletdb(strWalletFile);
        walletdb.WriteBestBlock(loc);
    }
}

bool CWallet::SetMinVersion(enum WalletFeature nVersion, CWalletDB* pwalletdbIn, bool fExplicit)
{
    LOCK(cs_wallet); // nWalletVersion
    if (nWalletVersion >= nVersion)
        return true;

    // when doing an explicit upgrade, if we pass the max version permitted, upgrade all the way
    if (fExplicit && nVersion > nWalletMaxVersion)
            nVersion = FEATURE_LATEST;

    nWalletVersion = nVersion;

    if (nVersion > nWalletMaxVersion)
        nWalletMaxVersion = nVersion;

    if (fFileBacked)
    {
        CWalletDB* pwalletdb = pwalletdbIn ? pwalletdbIn : new CWalletDB(strWalletFile);
        if (nWalletVersion > 40000)
            pwalletdb->WriteMinVersion(nWalletVersion);
        if (!pwalletdbIn)
            delete pwalletdb;
    }

    return true;
}

bool CWallet::SetMaxVersion(int nVersion)
{
    LOCK(cs_wallet); // nWalletVersion, nWalletMaxVersion
    // cannot downgrade below current version
    if (nWalletVersion > nVersion)
        return false;

    nWalletMaxVersion = nVersion;

    return true;
}

set<uint256> CWallet::GetConflicts(const uint256& txid) const
{
    set<uint256> result;
//    if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)                            // Not supported as Spent array is no longer maintained
    
    AssertLockHeld(cs_wallet);

    std::map<uint256, CWalletTx>::const_iterator it = mapWallet.find(txid);
    if (it == mapWallet.end())
        return result;
    const CWalletTx& wtx = it->second;

    std::pair<TxSpends::const_iterator, TxSpends::const_iterator> range;

    BOOST_FOREACH(const CTxIn& txin, wtx.vin)
    {
        if (mapTxSpends.count(txin.prevout) <= 1)
            continue;  // No conflict if zero or one spends
        range = mapTxSpends.equal_range(txin.prevout);
        for (TxSpends::const_iterator it = range.first; it != range.second; ++it)
            result.insert(it->second);
    }
    return result;
}

void CWallet::SyncMetaData(pair<TxSpends::iterator, TxSpends::iterator> range)
{
    // We want all the wallet transactions in range to have the same metadata as
    // the oldest (smallest nOrderPos).
    // So: find smallest nOrderPos:
//    if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)                          // Not supported as Spent array is no longer maintained
    
    int nMinOrderPos = std::numeric_limits<int>::max();
    const CWalletTx* copyFrom = NULL;
    for (TxSpends::iterator it = range.first; it != range.second; ++it)
    {
        const uint256& hash = it->second;
        int n = mapWallet[hash].nOrderPos;
        if (n < nMinOrderPos)
        {
            nMinOrderPos = n;
            copyFrom = &mapWallet[hash];
        }
    }
    // Now copy data from copyFrom to rest:
    for (TxSpends::iterator it = range.first; it != range.second; ++it)
    {
        const uint256& hash = it->second;
        CWalletTx* copyTo = &mapWallet[hash];
        if (copyFrom == copyTo) continue;
        copyTo->mapValue = copyFrom->mapValue;
        copyTo->vOrderForm = copyFrom->vOrderForm;
        // fTimeReceivedIsTxTime not copied on purpose
        // nTimeReceived not copied on purpose
        copyTo->nTimeSmart = copyFrom->nTimeSmart;
        copyTo->fFromMe = copyFrom->fFromMe;
        copyTo->strFromAccount = copyFrom->strFromAccount;
        // nOrderPos not copied on purpose
        // cached members not copied on purpose
    }
}

/**
 * Outpoint is spent if any non-conflicted transaction
 * spends it:
 */
bool CWallet::IsSpent(const uint256& hash, unsigned int n) const
{
//    if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)                          // Not supported as Spends array not maintained 
    const COutPoint outpoint(hash, n);
    pair<TxSpends::const_iterator, TxSpends::const_iterator> range;
    range = mapTxSpends.equal_range(outpoint);

    for (TxSpends::const_iterator it = range.first; it != range.second; ++it)
    {
        const uint256& wtxid = it->second;
/* MCHN START */        
        if(setPurged.count(wtxid))
            return true;
/* MCHN END */        
        std::map<uint256, CWalletTx>::const_iterator mit = mapWallet.find(wtxid);
        if (mit != mapWallet.end() && mit->second.GetDepthInMainChain() >= 0)
            return true; // Spent
    }
    return false;
}

void CWallet::AddToSpends(const COutPoint& outpoint, const uint256& wtxid)
{
    mapTxSpends.insert(make_pair(outpoint, wtxid));

    pair<TxSpends::iterator, TxSpends::iterator> range;
    range = mapTxSpends.equal_range(outpoint);
    SyncMetaData(range);
}


void CWallet::AddToSpends(const uint256& wtxid)
{
//    if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)                          // Not supported as Spends array not maintained 
    assert(mapWallet.count(wtxid));
    CWalletTx& thisTx = mapWallet[wtxid];
    if (thisTx.IsCoinBase()) // Coinbases don't spend anything!
        return;

    BOOST_FOREACH(const CTxIn& txin, thisTx.vin)
        AddToSpends(txin.prevout, wtxid);
}

bool CWallet::EncryptWallet(const SecureString& strWalletPassphrase)
{
    if (IsCrypted())
        return false;

    CKeyingMaterial vMasterKey;
//    RandAddSeedPerfmon();

    vMasterKey.resize(WALLET_CRYPTO_KEY_SIZE);
//    GetRandBytes(&vMasterKey[0], WALLET_CRYPTO_KEY_SIZE);
    GetStrongRandBytes(&vMasterKey[0], WALLET_CRYPTO_KEY_SIZE);

    CMasterKey kMasterKey;
//    RandAddSeedPerfmon();

    kMasterKey.vchSalt.resize(WALLET_CRYPTO_SALT_SIZE);
//    GetRandBytes(&kMasterKey.vchSalt[0], WALLET_CRYPTO_SALT_SIZE);
    GetStrongRandBytes(&kMasterKey.vchSalt[0], WALLET_CRYPTO_SALT_SIZE);

    CCrypter crypter;
    int64_t nStartTime = GetTimeMillis();
    crypter.SetKeyFromPassphrase(strWalletPassphrase, kMasterKey.vchSalt, 25000, kMasterKey.nDerivationMethod);
    kMasterKey.nDeriveIterations = 2500000 / ((double)(GetTimeMillis() - nStartTime));

    nStartTime = GetTimeMillis();
    crypter.SetKeyFromPassphrase(strWalletPassphrase, kMasterKey.vchSalt, kMasterKey.nDeriveIterations, kMasterKey.nDerivationMethod);
    kMasterKey.nDeriveIterations = (kMasterKey.nDeriveIterations + kMasterKey.nDeriveIterations * 100 / ((double)(GetTimeMillis() - nStartTime))) / 2;

    if (kMasterKey.nDeriveIterations < 25000)
        kMasterKey.nDeriveIterations = 25000;

    LogPrintf("Encrypting Wallet with an nDeriveIterations of %i\n", kMasterKey.nDeriveIterations);

    if (!crypter.SetKeyFromPassphrase(strWalletPassphrase, kMasterKey.vchSalt, kMasterKey.nDeriveIterations, kMasterKey.nDerivationMethod))
        return false;
    if (!crypter.Encrypt(vMasterKey, kMasterKey.vchCryptedKey))
        return false;

    {
        LOCK(cs_wallet);
        mapMasterKeys[++nMasterKeyMaxID] = kMasterKey;
        if (fFileBacked)
        {
            assert(!pwalletdbEncryption);
            pwalletdbEncryption = new CWalletDB(strWalletFile);
            if (!pwalletdbEncryption->TxnBegin()) {
                delete pwalletdbEncryption;
                pwalletdbEncryption = NULL;
                return false;
            }
            pwalletdbEncryption->WriteMasterKey(nMasterKeyMaxID, kMasterKey);
        }

        if (!EncryptKeys(vMasterKey))
        {
            if (fFileBacked) {
                pwalletdbEncryption->TxnAbort();
                delete pwalletdbEncryption;
            }
            // We now probably have half of our keys encrypted in memory, and half not...
            // die and let the user reload their unencrypted wallet.
            assert(false);
        }

        // Encryption was introduced in version 0.4.0
        SetMinVersion(FEATURE_WALLETCRYPT, pwalletdbEncryption, true);

        if (fFileBacked)
        {
            if (!pwalletdbEncryption->TxnCommit()) {
                delete pwalletdbEncryption;
                // We now have keys encrypted in memory, but not on disk...
                // die to avoid confusion and let the user reload their unencrypted wallet.
                assert(false);
            }

            delete pwalletdbEncryption;
            pwalletdbEncryption = NULL;
        }

        Lock();
        Unlock(strWalletPassphrase);
        NewKeyPool();
        Lock();

        // Need to completely rewrite the wallet file; if we don't, bdb might keep
        // bits of the unencrypted private key in slack space in the database file.
        RewriteWalletDB(strWalletFile);

    }
    NotifyStatusChanged(this);

    return true;
}

int64_t CWallet::IncOrderPosNext(CWalletDB *pwalletdb)
{
    AssertLockHeld(cs_wallet); // nOrderPosNext
    int64_t nRet = nOrderPosNext++;
    if (pwalletdb) {
        pwalletdb->WriteOrderPosNext(nOrderPosNext);
    } else {
        CWalletDB(strWalletFile).WriteOrderPosNext(nOrderPosNext);
    }
    return nRet;
}

CWallet::TxItems CWallet::OrderedTxItems(std::list<CAccountingEntry>& acentries, std::string strAccount)
{
    AssertLockHeld(cs_wallet); // mapWallet
    CWalletDB walletdb(strWalletFile);

    // First: get all CWalletTx and CAccountingEntry into a sorted-by-order multimap.
    TxItems txOrdered;

//    if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)                          // Not supported, not used in this case
    
    // Note: maintaining indices in the database of (account,time) --> txid and (account, time) --> acentry
    // would make this much faster for applications that do this a lot.
    for (map<uint256, CWalletTx>::iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
    {
        CWalletTx* wtx = &((*it).second);
        txOrdered.insert(make_pair(wtx->nOrderPos, TxPair(wtx, (CAccountingEntry*)0)));
    }
    acentries.clear();
    walletdb.ListAccountCreditDebit(strAccount, acentries);
    BOOST_FOREACH(CAccountingEntry& entry, acentries)
    {
        txOrdered.insert(make_pair(entry.nOrderPos, TxPair((CWalletTx*)0, &entry)));
    }

    return txOrdered;
}

void CWallet::MarkDirty()
{
//    if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)                            // Not supported, not used in this case
    {
        LOCK(cs_wallet);
        BOOST_FOREACH(PAIRTYPE(const uint256, CWalletTx)& item, mapWallet)
            item.second.MarkDirty();
    }
}

bool CWallet::AddToWallet(const CWalletTx& wtxIn, bool fFromLoadWallet)
{
    uint256 hash = wtxIn.GetHash();
//    if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)                          // Not supported, not used in this case

    if (fFromLoadWallet)
    {
        mapWallet[hash] = wtxIn;
        mapWallet[hash].BindWallet(this);
        AddToSpends(hash);
    }
    else
    {
        LOCK(cs_wallet);
        // Inserts only if not already there, returns tx inserted or tx found
        pair<map<uint256, CWalletTx>::iterator, bool> ret = mapWallet.insert(make_pair(hash, wtxIn));
        CWalletTx& wtx = (*ret.first).second;
        wtx.BindWallet(this);
        bool fInsertedNew = ret.second;
        if (fInsertedNew)
        {
            wtx.nTimeReceived = GetAdjustedTime();
/* MCHN START */            
            if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)
            {
                mc_TxDefRow txdef;
                if(pwalletTxsMain->FindWalletTx(wtx.GetHash(),&txdef) == MC_ERR_NOERROR)
                {
                    wtx.nTimeReceived=txdef.m_TimeReceived;
                }
            }
/* MCHN END */            
            wtx.nOrderPos = IncOrderPosNext();

            wtx.nTimeSmart = wtx.nTimeReceived;
            if (wtxIn.hashBlock != 0)
            {
                if (mapBlockIndex.count(wtxIn.hashBlock))
                {
                    int64_t latestNow = wtx.nTimeReceived;
                    int64_t latestEntry = 0;
/* MCHN START */                    
/*                    
                    {
                        // Tolerate times up to the last timestamp in the wallet not more than 5 minutes into the future
                        int64_t latestTolerated = latestNow + 300;
                        std::list<CAccountingEntry> acentries;
                        TxItems txOrdered = OrderedTxItems(acentries);
                        for (TxItems::reverse_iterator it = txOrdered.rbegin(); it != txOrdered.rend(); ++it)
                        {
                            CWalletTx *const pwtx = (*it).second.first;
                            if (pwtx == &wtx)
                                continue;
                            CAccountingEntry *const pacentry = (*it).second.second;
                            int64_t nSmartTime;
                            if (pwtx)
                            {
                                nSmartTime = pwtx->nTimeSmart;
                                if (!nSmartTime)
                                    nSmartTime = pwtx->nTimeReceived;
                            }
                            else
                                nSmartTime = pacentry->nTime;
                            if (nSmartTime <= latestTolerated)
                            {
                                latestEntry = nSmartTime;
                                if (nSmartTime > latestNow)
                                    latestNow = nSmartTime;
                                break;
                            }
                        }
                    }
 */ 
/* MCHN END */                    
                    int64_t blocktime = mapBlockIndex[wtxIn.hashBlock]->GetBlockTime();
                    wtx.nTimeSmart = std::max(latestEntry, std::min(blocktime, latestNow));
                }
                else
                    LogPrintf("AddToWallet() : found %s in block %s not in index\n",
                             wtxIn.GetHash().ToString(),
                             wtxIn.hashBlock.ToString());
            }
            AddToSpends(hash);
        }

        bool fUpdated = false;
        if (!fInsertedNew)
        {
            // Merge
            if (wtxIn.hashBlock != 0 && wtxIn.hashBlock != wtx.hashBlock)
            {
                wtx.hashBlock = wtxIn.hashBlock;
                fUpdated = true;
            }
            if (wtxIn.nIndex != -1 && (wtxIn.vMerkleBranch != wtx.vMerkleBranch || wtxIn.nIndex != wtx.nIndex))
            {
                wtx.vMerkleBranch = wtxIn.vMerkleBranch;
                wtx.nIndex = wtxIn.nIndex;
                fUpdated = true;
            }
            if (wtxIn.fFromMe && wtxIn.fFromMe != wtx.fFromMe)
            {
                wtx.fFromMe = wtxIn.fFromMe;
                fUpdated = true;
            }
/* MCHN START */            
            if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)
            {
                mc_TxDefRow txdef;
                if(pwalletTxsMain->FindWalletTx(wtx.GetHash(),&txdef) == MC_ERR_NOERROR)
                {
                    if(wtx.nTimeReceived != txdef.m_TimeReceived)
                    {
                        wtx.nTimeReceived=txdef.m_TimeReceived;
                        wtx.nTimeSmart=txdef.m_TimeReceived;
                        fUpdated = true;
                    }                    
                }
            }
/* MCHN END */            
        }

/* MCHN START */        
//        LogPrintf("AddToWallet %s  %s%s; Time \n", wtxIn.GetHash().ToString(), (fInsertedNew ? "new" : ""), (fUpdated ? "update" : ""));
/* MCHN END */        

        // Write to disk
        if (fInsertedNew || fUpdated)
            if (!wtx.WriteToDisk())
                return false;

    
        // Break debit/credit balance caches:
        wtx.MarkDirty();

        // Notify UI of new or updated transaction
        NotifyTransactionChanged(this, hash, fInsertedNew ? CT_NEW : CT_UPDATED);

        // notify an external script when a wallet transaction comes in or is updated
        std::string strCmd = GetArg("-walletnotify", "");

        if ( !strCmd.empty())
        {
            boost::replace_all(strCmd, "%s", wtxIn.GetHash().GetHex());
            boost::thread t(runCommand, strCmd); // thread runs free
        }

        UpdateUnspentList(wtx,true);
    }
    

    return true;
}

/**
 * Add a transaction to the wallet, or update it.
 * pblock is optional, but should be provided if the transaction is known to be in a block.
 * If fUpdate is true, existing transactions will be updated.
 */
bool CWallet::AddToWalletIfInvolvingMe(const CTransaction& tx, const CBlock* pblock, bool fUpdate)
{
    {
        AssertLockHeld(cs_wallet);
/* MCHN START */        
        if(pblock)
        {
            if(pblock->GetHash() ==  Params().HashGenesisBlock())
            {
                return false;
            }            
        }
/* MCHN END */                
        bool fExisted = mapWallet.count(tx.GetHash()) != 0;
        if (fExisted && !fUpdate) return false;
        if (fExisted || IsMine(tx) || IsFromMe(tx))
        {
            CWalletTx wtx(this,tx);
            // Get merkle branch if transaction was found in a block
            if (pblock)
                wtx.SetMerkleBranch(*pblock);
            return AddToWallet(wtx);
        }
    }
    return false;
}

void CWallet::SyncTransaction(const CTransaction& tx, const CBlock* pblock)
{
    LOCK2(cs_main, cs_wallet);

    /* MCHN START */
    bool fEmpty=false;
    if((tx.vin.size() == 0) && ((tx.vout.size() == 0)))
    {
        LogPrint("mchn","mchn: Wallet optimization after block\n");
        fEmpty=true;
    }

    if(!fEmpty)
    {
        if(((mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS) == 0) || (mc_gState->m_WalletMode & MC_WMD_MAP_TXS))
        {
/* MCHN END */    
            if (!AddToWalletIfInvolvingMe(tx, pblock, true))
                return; // Not one of ours

            // If a transaction changes 'conflicted' state, that changes the balance
            // available of the outputs it spends. So force those to be
            // recomputed, also:
            BOOST_FOREACH(const CTxIn& txin, tx.vin)
            {
                if (mapWallet.count(txin.prevout.hash))
                    mapWallet[txin.prevout.hash].MarkDirty();
            }
        
/* MCHN START */   
        }
    }
    if(fEmpty)// || (!fExisted && (pblock == NULL)))
    {
        OptimizeUnspentList();
    }
    
/* MCHN END */    
    
}

void CWallet::EraseFromWallet(const uint256 &hash)
{
//    if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)                          // Not supported, not used in this case
    if (!fFileBacked)
        return;
    {
        LOCK(cs_wallet);
        if (mapWallet.erase(hash))
        {
            CWalletDB(strWalletFile).EraseTx(hash);
        }
    }
    return;
}


isminetype CWallet::IsMine(const CTxIn &txin) const
{
    {
        LOCK(cs_wallet);
        if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)
        {
            int err;
            const CWalletTx& prev=pwalletTxsMain->GetInternalWalletTx(txin.prevout.hash,NULL,&err);
            if(err == MC_ERR_NOERROR)
            {
                if (txin.prevout.n < prev.vout.size())
                    return IsMine(prev.vout[txin.prevout.n]);
            }            
        }
        else
        {
            map<uint256, CWalletTx>::const_iterator mi = mapWallet.find(txin.prevout.hash);
            if (mi != mapWallet.end())
            {
                const CWalletTx& prev = (*mi).second;
                if (txin.prevout.n < prev.vout.size())
                    return IsMine(prev.vout[txin.prevout.n]);
            }
        }
    }
    return ISMINE_NO;
}

CAmount CWallet::GetDebit(const CTxIn &txin, const isminefilter& filter) const
{
    {        
        LOCK(cs_wallet);
        if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)
        {
            int err;
            const CWalletTx& prev=pwalletTxsMain->GetInternalWalletTx(txin.prevout.hash,NULL,&err);
            if(err == MC_ERR_NOERROR)
            {
                if (txin.prevout.n < prev.vout.size())
                    if (IsMine(prev.vout[txin.prevout.n]) & filter)
                        return prev.vout[txin.prevout.n].nValue;           
            }            
        }
        else
        {
            map<uint256, CWalletTx>::const_iterator mi = mapWallet.find(txin.prevout.hash);
            if (mi != mapWallet.end())
            {
                const CWalletTx& prev = (*mi).second;
                if (txin.prevout.n < prev.vout.size())
                    if (IsMine(prev.vout[txin.prevout.n]) & filter)
                        return prev.vout[txin.prevout.n].nValue;
            }
        }
    }
    return 0;
}

bool CWallet::IsFromMe(const CTxIn &txin, const isminefilter& filter) const
{
    {
        LOCK(cs_wallet);
        
        if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)
        {
            int err;
            if(txin.prevout.hash == 0)
            {
                return false;
            }
            
            const CWalletTx& prev=pwalletTxsMain->GetInternalWalletTx(txin.prevout.hash,NULL,&err);
            
            if(err == MC_ERR_NOERROR)
            {
                if (txin.prevout.n < prev.vout.size())
                    if (IsMine(prev.vout[txin.prevout.n]) & filter)
                        return true;                
            }            
        }
        else
        {
            map<uint256, CWalletTx>::const_iterator mi = mapWallet.find(txin.prevout.hash);
            if (mi != mapWallet.end())
            {
                const CWalletTx& prev = (*mi).second;
                if (txin.prevout.n < prev.vout.size())
                    if (IsMine(prev.vout[txin.prevout.n]) & filter)
                        return true;
            }
        }
    }
    return false;
}

bool CWalletTx::IsTrusted(int nDepth) const
{
    // Quick answer in most cases
    if (!IsFinalTx(*this))
        return false;
    if (nDepth >= 1)
        return true;
    if (nDepth < 0)
        return false;
    if (!bSpendZeroConfChange || !IsFromMe(ISMINE_ALL)) // using wtx's cached debit
        return false;

    // Trusted if all inputs are from us and are in the mempool:
    BOOST_FOREACH(const CTxIn& txin, vin)
    {
        // Transactions not sent by us: not trusted
/* MCHN START */            
        if(pwallet->setPurged.count(txin.prevout.hash) == 0)
        {
/* MCHN END */            
            if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)
            {
                int err;
                const CWalletTx& prev=pwalletTxsMain->GetInternalWalletTx(txin.prevout.hash,NULL,&err);
                if(err)
                {
                    return false;
                }
                const CTxOut& parentOut = prev.vout[txin.prevout.n];
                if (pwallet->IsMine(parentOut) != ISMINE_SPENDABLE)
                {
                    return false;
                }   
            }
            else
            {
                const CWalletTx* parent = pwallet->GetWalletTx(txin.prevout.hash);
                if (parent == NULL)
                    return false;
                const CTxOut& parentOut = parent->vout[txin.prevout.n];
                if (pwallet->IsMine(parentOut) != ISMINE_SPENDABLE)
                    return false;
            }
/* MCHN START */            
        }
/* MCHN END */            
    }
    return true;
}

bool CWallet::IsChange(const CTxOut& txout) const
{
    // TODO: fix handling of 'change' outputs. The assumption is that any
    // payment to a script that is ours, but is not in the address book
    // is change. That assumption is likely to break when we implement multisignature
    // wallets that return change back into a multi-signature-protected address;
    // a better way of identifying which outputs are 'the send' and which are
    // 'the change' will need to be implemented (maybe extend CWalletTx to remember
    // which output, if any, was change).
    if (::IsMine(*this, txout.scriptPubKey))
    {
        CTxDestination address;
        if (!ExtractDestination(txout.scriptPubKey, address))
            return true;

        LOCK(cs_wallet);
        if (!mapAddressBook.count(address))
            return true;
    }
    return false;
}

int64_t CWalletTx::GetTxTime() const
{
    int64_t n = nTimeSmart;
    return n ? n : nTimeReceived;
}

int CWalletTx::GetRequestCount() const
{
    // Returns -1 if it wasn't being tracked
    int nRequests = -1;
    {
        LOCK(pwallet->cs_wallet);
        if (IsCoinBase())
        {
            // Generated block
            if (hashBlock != 0)
            {
                map<uint256, int>::const_iterator mi = pwallet->mapRequestCount.find(hashBlock);
                if (mi != pwallet->mapRequestCount.end())
                    nRequests = (*mi).second;
            }
        }
        else
        {
            // Did anyone request this transaction?
            map<uint256, int>::const_iterator mi = pwallet->mapRequestCount.find(GetHash());
            if (mi != pwallet->mapRequestCount.end())
            {
                nRequests = (*mi).second;

                // How about the block it's in?
                if (nRequests == 0 && hashBlock != 0)
                {
                    map<uint256, int>::const_iterator mi = pwallet->mapRequestCount.find(hashBlock);
                    if (mi != pwallet->mapRequestCount.end())
                        nRequests = (*mi).second;
                    else
                        nRequests = 1; // If it's in someone else's block it must have got out
                }
            }
        }
    }
    return nRequests;
}

void CWalletTx::GetAmounts(list<COutputEntry>& listReceived,
                           list<COutputEntry>& listSent, CAmount& nFee, string& strSentAccount, const isminefilter& filter) const
{
    nFee = 0;
    listReceived.clear();
    listSent.clear();
    strSentAccount = strFromAccount;

    // Compute fee:
    CAmount nDebit = GetDebit(filter);
    if (nDebit > 0) // debit>0 means we signed/sent this transaction
    {
        CAmount nValueOut = GetValueOut();
        nFee = nDebit - nValueOut;
    }
        
/* MCHN START */
    bool isFromMe=false;
    if(nDebit > 0)
    {
        isFromMe=true;
    }
    else
    {
        if(mc_gState->m_NetworkParams->IsProtocolMultichain())
        {
            if(IsFromMe(filter))
            {
                isFromMe=true;
            }
        }
    }

    bool isExchange=false;
    if(isFromMe)
    {
        BOOST_FOREACH(const CTxIn& txin, vin)
        {
            if(!pwallet->IsFromMe(txin, filter))
            {
                isExchange=true;
            }
        }
    }

    bool isSelf=false;

    if(isFromMe && !isExchange)
    {
        isSelf=true;
        for (unsigned int i = 0; i < vout.size(); ++i)
        {
            const CTxOut& txout = vout[i];
            if(!txout.scriptPubKey.IsUnspendable())
            {
                isminetype fIsMine = pwallet->IsMine(txout);
                if ((fIsMine & filter) == 0)
                {
                    isSelf=false;
                }
            }
        }        
    }
    
/* MCHN END */    
    // Sent/received.
    for (unsigned int i = 0; i < vout.size(); ++i)
    {
        const CTxOut& txout = vout[i];
        isminetype fIsMine = pwallet->IsMine(txout);
        // Only need to handle txouts if AT LEAST one of these is true:
        //   1) they debit from us (sent)
        //   2) the output is to us (received)
/* MCHN START */        
//        if (nDebit > 0)
        if(txout.scriptPubKey.IsUnspendable())
            continue; 
        
        if(isFromMe)
        {
            // Don't report 'change' txouts
            bool isChange=false;
            if(mc_gState->m_NetworkParams->IsProtocolMultichain())
            {
                if(!isSelf)
                {                    
                    if(fIsMine & filter)
                    {
                        isChange = true;
                    }
                }
            }
            else
            {
                isChange=pwallet->IsChange(txout);
            }
            if (isChange)
                continue;
/* MCHN END */                    
        }
        else if (!(fIsMine & filter))
            continue;

        // In either case, we need to get the destination address
        CTxDestination address;
        if (!ExtractDestination(txout.scriptPubKey, address))
        {
            LogPrintf("CWalletTx::GetAmounts: Unknown transaction type found, txid %s\n",
                     this->GetHash().ToString());
            address = CNoDestination();
        }

        COutputEntry output = {address, txout.nValue, (int)i};

        // If we are debited by the transaction, add the output as a "sent" entry
/* MCHN START */        
//        if (nDebit > 0)
        if(isFromMe)
/* MCHN END */                    
            listSent.push_back(output);

        // If we are receiving the output, add it as a "received" entry
        if (fIsMine & filter)
            listReceived.push_back(output);
    }

}

void CWalletTx::GetAccountAmounts(const string& strAccount, CAmount& nReceived,
                                  CAmount& nSent, CAmount& nFee, const isminefilter& filter) const
{
    nReceived = nSent = nFee = 0;

    CAmount allFee;
    string strSentAccount;
    list<COutputEntry> listReceived;
    list<COutputEntry> listSent;
    GetAmounts(listReceived, listSent, allFee, strSentAccount, filter);

    if (strAccount == strSentAccount)
    {
        BOOST_FOREACH(const COutputEntry& s, listSent)
            nSent += s.amount;
        nFee = allFee;
    }
    {
        LOCK(pwallet->cs_wallet);
        BOOST_FOREACH(const COutputEntry& r, listReceived)
        {
            if (pwallet->mapAddressBook.count(r.destination))
            {
                map<CTxDestination, CAddressBookData>::const_iterator mi = pwallet->mapAddressBook.find(r.destination);
                if (mi != pwallet->mapAddressBook.end() && (*mi).second.name == strAccount)
                    nReceived += r.amount;
            }
            else if (strAccount.empty())
            {
                nReceived += r.amount;
            }
        }
    }
}


bool CWalletTx::WriteToDisk()
{
    return CWalletDB(pwallet->strWalletFile).WriteTx(GetHash(), *this);
}

/* MCHN START */

mc_TxImport *StartImport(CWallet *lpWallet,bool fOnlyUnsynced, bool fOnlySubscriptions, int block, int *err)
{
    vector <CBitcoinAddress> vAddressesToImport;
    vector <mc_TxEntity> vStreamsToImport;
    mc_TxEntity entity;
    mc_TxImport *imp;
    mc_Buffer *m_ChainEntities;
    mc_TxEntityStat *lpent;
    mc_EntityDetails stream_entity;
    unsigned char *ptr;
    int block_to_start_from,i,b;

    imp=NULL;
    
    *err=MC_ERR_NOERROR;
    if( (mc_gState->m_WalletMode & MC_WMD_TXS) == 0)
    {
        return NULL;
    }            
    
    block_to_start_from=chainActive.Height()-1;    
    
    if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)
    {
        if(!fOnlySubscriptions)
        {
            BOOST_FOREACH(const PAIRTYPE(CBitcoinAddress, CAddressBookData)& item, lpWallet->mapAddressBook)
            {
                const CBitcoinAddress& address = item.first;
                mc_TxEntityStat entStat;
                CTxDestination addressRet=address.Get();        
                const CKeyID *lpKeyID=boost::get<CKeyID> (&addressRet);
                const CScriptID *lpScriptID=boost::get<CScriptID> (&addressRet);

                entity.Zero();
                if(lpKeyID)
                {
                    memcpy(entity.m_EntityID,lpKeyID,MC_TDB_ENTITY_ID_SIZE);
                    entity.m_EntityType=MC_TET_PUBKEY_ADDRESS | MC_TET_CHAINPOS;
                }
                if(lpScriptID)
                {
                    memcpy(entity.m_EntityID,lpScriptID,MC_TDB_ENTITY_ID_SIZE);
                    entity.m_EntityType=MC_TET_SCRIPT_ADDRESS | MC_TET_CHAINPOS;
                }

                if(entity.m_EntityType)
                {
                    entStat.Zero();
                    memcpy(&entStat,&entity,sizeof(mc_TxEntity));
                    if(pwalletTxsMain->FindEntity(&entStat))
                    {                        
                        if(!fOnlyUnsynced || 
                           (((entStat.m_Flags & MC_EFL_NOT_IN_SYNC) != 0) && (entStat.m_LastImportedBlock > block) ) )
                        {
                            vAddressesToImport.push_back(address);
                        }
                    }
                    else
                    {
                        vAddressesToImport.push_back(address);                    
                    }
                }
            }
        }
    }
    
    pwalletTxsMain->Lock();    
    m_ChainEntities=pwalletTxsMain->GetEntityList();
    if(m_ChainEntities)
    {
        for(i=0;i<m_ChainEntities->GetCount();i++)
        {
            lpent=(mc_TxEntityStat*)m_ChainEntities->GetRow(i);
            if(lpent->m_Entity.IsSubscription())
            {
//                if(lpent->m_Entity.m_EntityType & MC_TET_CHAINPOS)
                {
                    if( ((lpent->m_Flags & MC_EFL_NOT_IN_SYNC) != 0 ) ||
                        (pEF->STR_IsOutOfSync(&(lpent->m_Entity)) != 0) )    
                    {
                        vStreamsToImport.push_back(lpent->m_Entity);
                    }
                }
            }
        }
    }
    pwalletTxsMain->UnLock();
    if( (vAddressesToImport.size() == 0) && (vStreamsToImport.size() == 0) )
    {
        return NULL;
    }
    
    mc_Buffer *lpEntities;
    lpEntities=new mc_Buffer();
    lpEntities->Initialize(sizeof(mc_TxEntity),sizeof(mc_TxEntity),MC_BUF_MODE_DEFAULT);
    if(vAddressesToImport.size())
    {
        entity.Zero();
        entity.m_EntityType=MC_TET_WALLET_ALL | MC_TET_CHAINPOS;
        lpEntities->Add(&entity,NULL);
        entity.m_EntityType=MC_TET_WALLET_ALL | MC_TET_TIMERECEIVED;
        lpEntities->Add(&entity,NULL);
        entity.m_EntityType=MC_TET_WALLET_SPENDABLE | MC_TET_CHAINPOS;
        lpEntities->Add(&entity,NULL);
        entity.m_EntityType=MC_TET_WALLET_SPENDABLE | MC_TET_TIMERECEIVED;
        lpEntities->Add(&entity,NULL);

        for(unsigned int i=0;i<vAddressesToImport.size();i++)
        {
            const CBitcoinAddress& address = vAddressesToImport[i];
            CTxDestination addressRet=address.Get();        
            const CKeyID *lpKeyID=boost::get<CKeyID> (&addressRet);
            const CScriptID *lpScriptID=boost::get<CScriptID> (&addressRet);

            entity.Zero();
            if(lpKeyID)
            {
                memcpy(entity.m_EntityID,lpKeyID,MC_TDB_ENTITY_ID_SIZE);
                entity.m_EntityType=MC_TET_PUBKEY_ADDRESS | MC_TET_CHAINPOS;
                lpEntities->Add(&entity,NULL);
                entity.m_EntityType=MC_TET_PUBKEY_ADDRESS | MC_TET_TIMERECEIVED;
                lpEntities->Add(&entity,NULL);
            }
            if(lpScriptID)
            {
                memcpy(entity.m_EntityID,lpScriptID,MC_TDB_ENTITY_ID_SIZE);
                entity.m_EntityType=MC_TET_SCRIPT_ADDRESS | MC_TET_CHAINPOS;
                lpEntities->Add(&entity,NULL);
                entity.m_EntityType=MC_TET_SCRIPT_ADDRESS | MC_TET_TIMERECEIVED;
                lpEntities->Add(&entity,NULL);
            }        
        }
    }
    
    if(vStreamsToImport.size())
    {
        for(unsigned int i=0;i<vStreamsToImport.size();i++)
        {     
            bool take_it=false;
            if(mc_gState->m_Assets->FindEntityByShortTxID(&stream_entity,vStreamsToImport[i].m_EntityID))
            {
                take_it=true;
            }                
            if(take_it)
            {
                ptr=(unsigned char *)stream_entity.GetRef();
                b=chainActive.Height();
                if(stream_entity.IsUnconfirmedGenesis() == 0)
                {
                    b=(int)mc_GetLE(ptr,4)-1;
                }
                
                if(b > block)
                {
                    if(b < block_to_start_from)
                    {
                        block_to_start_from=b;
                    }
                    entity.Zero();            
                    memcpy(entity.m_EntityID,vStreamsToImport[i].m_EntityID,MC_TDB_ENTITY_ID_SIZE);
                    entity.m_EntityType=vStreamsToImport[i].m_EntityType;
                    lpEntities->Add(&entity,NULL);
/*                    
                    entity.m_EntityType=(vStreamsToImport[i].m_EntityType - MC_TET_CHAINPOS) | MC_TET_TIMERECEIVED;
                    lpEntities->Add(&entity,NULL);
 */ 
                }
            }            
        }
    }
    
    
    if(vAddressesToImport.size())
    {
        block_to_start_from=block;        
    }    
    
    if(lpEntities->GetCount())
    {
        imp=pwalletTxsMain->StartImport(lpEntities,block_to_start_from,err);
    }
    
    
    delete lpEntities;
    
    return imp;
}

/* MCHN END*/

/**
 * Scan the block chain (starting in pindexStart) for transactions
 * from or to us. If fUpdate is true, found transactions that already
 * exist in the wallet will be updated.
 */
int CWallet::ScanForWalletTransactions(CBlockIndex* pindexStart, bool fUpdate,bool fOnlyUnsynced,bool fOnlySubscriptions)
{
    int ret = 0;
    int64_t nNow = GetTime();

    CBlockIndex* pindex = pindexStart;
    {
        LOCK2(cs_main, cs_wallet);

/* MCHN START */        
        mc_TxImport *imp;
        int err;
        imp=StartImport(this,fOnlyUnsynced,fOnlySubscriptions,pindex->nHeight-1,&err);
/* MCHN END */      
        
/* MCHN START */        
        // no need to read and scan block, if block was created before
        // our wallet birthday (as adjusted for block time variability)

/* Bad idea, imported address may be older than our wallet        
        while (pindex && nTimeFirstKey && (pindex->GetBlockTime() < (nTimeFirstKey - 7200)))
            pindex = chainActive.Next(pindex);
 */ 
/* MCHN END */      
    
        if(imp)
        {
            while(pindex && (pindex->nHeight <= imp->m_Block))
            {
                pindex = chainActive.Next(pindex);            
            }
        }        

        ShowProgress(_("Rescanning..."), 0); // show rescan progress in GUI as dialog or on splashscreen, if -rescan on startup
        LogPrint("wallet","Rescanning for wallet transactions\n");
        double dProgressStart = Checkpoints::GuessVerificationProgress(pindex, false);
        double dProgressTip = Checkpoints::GuessVerificationProgress(chainActive.Tip(), false);
        if(mc_gState->m_WalletMode & MC_WMD_TXS)
        {
            if(imp == NULL)
            {
                LogPrint("wallet","No new entities, rescanning skipped\n");
                pindex=NULL;                
            }
        }
        
        while (pindex)
        {
            if (pindex->nHeight % 100 == 0 && dProgressTip - dProgressStart > 0.0)
                ShowProgress(_("Rescanning..."), std::max(1, std::min(99, (int)((Checkpoints::GuessVerificationProgress(pindex, false) - dProgressStart) / (dProgressTip - dProgressStart) * 100))));
            
            CBlock block;
            ReadBlockFromDisk(block, pindex);
            
/* MCHN START */            
            CDiskTxPos pos(pindex->GetBlockPos(), GetSizeOfCompactSize(block.vtx.size()));
            int block_tx_index=0;
            
            BOOST_FOREACH(CTransaction& tx, block.vtx)
            {
                if(imp)
                {
                    if(err == MC_ERR_NOERROR)
                    {
                        if(pindex->nHeight)                                     // Skip 0-block coinbase
                        {
                            err=pwalletTxsMain->AddTx(imp,tx,pindex->nHeight,&pos,block_tx_index,pindex->GetBlockHash());
                        }
                    }
                }
                if(((mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS) == 0) || (mc_gState->m_WalletMode & MC_WMD_MAP_TXS))
                {
                    if (AddToWalletIfInvolvingMe(tx, &block, fUpdate))
                        ret++;
                }
                pos.nTxOffset += ::GetSerializeSize(tx, SER_DISK, CLIENT_VERSION);
                block_tx_index++;
            }
            if(imp)
            {
                if(err == MC_ERR_NOERROR)
                {
                    err=pEF->FED_EventChunksAvailable();
                    if(err)
                    {
                        LogPrintf("ERROR: Cannot write offchain items in block, error %d\n",err);
                    }
                    err=MC_ERR_NOERROR;
                }
                if(err == MC_ERR_NOERROR)
                {
                    err=pwalletTxsMain->Commit(imp);
                    
                }   
                if(err == MC_ERR_NOERROR)
                {
                    pwalletTxsMain->CleanUpAfterBlock(imp,pindex->nHeight,pindex->nHeight-1);
                }                
            }
/* MCHN END */            
            if(!fOnlyUnsynced)
            {
                if((pindex->nHeight % 1000) == 0)
                {
                    printf("%d of %d blocks rescanned\n",pindex->nHeight,chainActive.Height());
                }
            }
            pindex = chainActive.Next(pindex);
            if (GetTime() >= nNow + 60) {
                nNow = GetTime();
                LogPrintf("Still rescanning. At block %d. Progress=%f\n", pindex->nHeight, Checkpoints::GuessVerificationProgress(pindex));
            }
        }
        ShowProgress(_("Rescanning..."), 100); // hide progress dialog in GUI
        if(imp)
        {
            if(err == MC_ERR_NOERROR)
            {
                LogPrint("wallet","wtxs: Replaying import mempool, %d items\n",mempool.hashList->m_Count);
                for(int pos=0;pos<mempool.hashList->m_Count;pos++)
                {
                    uint256 hash=*(uint256*)mempool.hashList->GetRow(pos);
                    if(mempool.exists(hash))
                    {
                        const CTransaction& tx = mempool.mapTx[hash].GetTx();
                        LogPrint("wallet","wtxs: Mempool tx: %s\n",hash.ToString().c_str());
                        pwalletTxsMain->AddTx(imp,tx,-1,NULL,-1,0);            
                    }
                }
                if(err == MC_ERR_NOERROR)
                {
                    err=pEF->FED_EventChunksAvailable();
                    if(err)
                    {
                        LogPrintf("ERROR: Cannot write offchain items after mempool, error %d\n",err);
                    }
                    err=MC_ERR_NOERROR;
                }
                
                err=pwalletTxsMain->CompleteImport(imp,((pindexStart->nHeight > 0) && !fOnlySubscriptions) ? MC_EFL_NOT_IN_SYNC_AFTER_IMPORT : 0);
            }
            else
            {
                LogPrintf("Rescan failed with error %d\n",err);            
                err=pwalletTxsMain->DropImport(imp);
            }
        }
        
        if(err)
        {
            LogPrintf("Rescan failed with error %d\n",err);            
            ret=-1;
        }
        else
        {
            LogPrint("wallet","Rescan completed successfully\n");            
        }
    }
    
    return ret;
}

void CWallet::ReacceptWalletTransactions()
{
    LOCK2(cs_main, cs_wallet);
    if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)
    {        
        pwalletTxsMain->Lock();
        LogPrint("mchn","ReacceptWalletTransactions: %ld txs in unconfirmed pool \n", pwalletTxsMain->m_UnconfirmedSends.size());
//        BOOST_FOREACH(PAIRTYPE(const uint256, CWalletTx)& item, pwalletTxsMain->m_UnconfirmedSends)
        BOOST_FOREACH(const uint256& wtxid, pwalletTxsMain->m_UnconfirmedSendsHashes) 
        {
            map<uint256,CWalletTx>::iterator item = pwalletTxsMain->m_UnconfirmedSends.find(wtxid);
            if(item != pwalletTxsMain->m_UnconfirmedSends.end())
            {                
//                const uint256& wtxid = item.first;
                CWalletTx& wtx = item->second;

                if (!wtx.IsCoinBase())// && nDepth < 0)
                {
                    LOCK(mempool.cs);

                    if (!mempool.exists(wtxid))
                    {
                        int nDepth = wtx.GetDepthInMainChain();
                        LogPrint("wallet","Unconfirmed wtx: %s, depth: %d\n", wtxid.ToString(),nDepth);
                        if(nDepth < 0)
                        {
                            LogPrint("wallet","Reaccepting wtx %s\n", wtxid.ToString());
                            if(!wtx.AcceptToMemoryPool(false))
                            {
                                LogPrintf("Tx %s was not accepted to mempool, setting INVALID flag\n", wtxid.ToString());
                                pwalletTxsMain->SaveTxFlag((unsigned char*)&wtxid,MC_TFL_INVALID,1);
                            }
                        }
                        else
                        {
                            LogPrintf("wtxs: Internal error! Unconfirmed wtx %s already in the chain\n", wtxid.ToString());                                                                
                        }
                    }
                    else
                    {
                        LogPrint("wallet","Unconfirmed wtx %s already in mempool, ignoring\n", wtxid.ToString());                    
                    }
                }            
            }
            else
            {
                LogPrintf("wtxs: Internal error! Unconfirmed wtx %s details not found\n", wtxid.ToString());                                    
            }
        }            
        pwalletTxsMain->UnLock();
    }
    else
    {
        BOOST_FOREACH(PAIRTYPE(const uint256, CWalletTx)& item, mapWallet)
        {
            const uint256& wtxid = item.first;
            CWalletTx& wtx = item.second;
            assert(wtx.GetHash() == wtxid);

            int nDepth = wtx.GetDepthInMainChain();

            if (!wtx.IsCoinBase() && nDepth < 0)
            {
                // Try to add to memory pool
                LOCK(mempool.cs);
                wtx.AcceptToMemoryPool(false);
            }
        }
    }
}

void CWalletTx::RelayWalletTransaction()
{
    if (!IsCoinBase())
    {
        if (GetDepthInMainChain() == 0) {
            LogPrint("wallet","Relaying wtx %s\n", GetHash().ToString());
            RelayTransaction((CTransaction)*this);
        }
    }
}

set<uint256> CWalletTx::GetConflicts() const
{
    set<uint256> result;
    if (pwallet != NULL)
    {
        uint256 myHash = GetHash();
        result = pwallet->GetConflicts(myHash);
        result.erase(myHash);
    }
    return result;
}

void CWallet::ResendWalletTransactions(bool fForce)
{
    // Do this infrequently and randomly to avoid giving away
    // that these are our transactions.
    if (!fForce && (GetTime() < nNextResend))
        return;
    bool fFirst = (nNextResend == 0);
/* MCHN START */    
//    nNextResend = GetTime() + GetRand(30 * 60);
    nNextResend = GetTime() + GetRand(3 * MCP_TARGET_BLOCK_TIME);
/* MCHN END */    
    if (!fForce && fFirst)
        return;

    // Only do it if there's been a new block since last time
    if(GetTime() < nNextResend + 3 * MCP_TARGET_BLOCK_TIME )
    {
        if (!fForce && (nTimeBestReceived < nLastResend))
            return;
    }
    nLastResend = GetTime();

    // Rebroadcast any of our txes that aren't in a block yet
    LogPrintf("ResendWalletTransactions()\n");
    {
        LOCK(cs_wallet);
        // Sort them in chronological order
        
        if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)
        {
            pwalletTxsMain->Lock();
//            BOOST_FOREACH(PAIRTYPE(const uint256, CWalletTx)& item, pwalletTxsMain->m_UnconfirmedSends)
            BOOST_FOREACH(const uint256& wtxid, pwalletTxsMain->m_UnconfirmedSendsHashes) 
            {
                map<uint256,CWalletTx>::iterator item = pwalletTxsMain->m_UnconfirmedSends.find(wtxid);
                if(item != pwalletTxsMain->m_UnconfirmedSends.end())
                {                
                    CWalletTx& wtx = item->second;
                    // Don't rebroadcast until it's had plenty of time that
                    // it should have gotten in already by now.
                    if ( fForce || (nTimeBestReceived - (int64_t)wtx.nTimeReceived > MCP_TARGET_BLOCK_TIME) )
                    {
                        LogPrint("wallet","Wallet tx %s resent\n",wtx.GetHash().ToString().c_str());
                        wtx.RelayWalletTransaction();                        
                    }
//                        mapSorted.insert(make_pair(wtx.nTimeReceived, &wtx));
                }
                
            }            
            pwalletTxsMain->UnLock();
        }
        else
        {
            multimap<unsigned int, CWalletTx*> mapSorted;
            BOOST_FOREACH(PAIRTYPE(const uint256, CWalletTx)& item, mapWallet)
            {
                CWalletTx& wtx = item.second;
                // Don't rebroadcast until it's had plenty of time that
                // it should have gotten in already by now.
    /* MCHNS START */    
    //            if (nTimeBestReceived - (int64_t)wtx.nTimeReceived > 5 * 60)
                if (nTimeBestReceived - (int64_t)wtx.nTimeReceived > MCP_TARGET_BLOCK_TIME)
    /* MCHN END */    
                    mapSorted.insert(make_pair(wtx.nTimeReceived, &wtx));
            }
            BOOST_FOREACH(PAIRTYPE(const unsigned int, CWalletTx*)& item, mapSorted)
            {
                CWalletTx& wtx = *item.second;
                LogPrint("wallet","Wallet tx %s resent\n",wtx.GetHash().ToString().c_str());
                wtx.RelayWalletTransaction();
            }
        }
    }
}

/** @} */ // end of mapWallet




/** @defgroup Actions
 *
 * @{
 */


CAmount CWallet::GetBalance() const
{
    CAmount nTotal = 0;
/* MCHN START */
    if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)
    {    
        vector<COutput> vecOutputs;        
        AvailableCoins(vecOutputs, false, NULL, false,true);
        BOOST_FOREACH(const COutput& out, vecOutputs) 
        {
            if(out.coin.IsTrusted())
            {
                nTotal+=out.coin.m_TXOut.nValue;
            }
        }
        return nTotal;
    }
/* MCHN END*/
    
    {
        LOCK2(cs_main, cs_wallet);
        
/* MCHN START */        
//        for (map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
//        {

        for (map<uint256, int>::const_iterator itUnspent = mapUnspent.begin(); itUnspent != mapUnspent.end(); ++itUnspent)
        {
            const uint256& wtxid = itUnspent->first;
            std::map<uint256, CWalletTx>::const_iterator it = mapWallet.find(wtxid);
            
            if (it == mapWallet.end())
                continue;

/* MCHN END */                
        
            const CWalletTx* pcoin = &(*it).second;
            if (pcoin->IsTrusted())
                nTotal += pcoin->GetAvailableCredit();
        }
    }

    return nTotal;
}

CAmount CWallet::GetUnconfirmedBalance() const
{
    CAmount nTotal = 0;
/* MCHN START */
    if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)
    {    
        vector<COutput> vecOutputs;        
        AvailableCoins(vecOutputs, false, NULL, false,true);
        BOOST_FOREACH(const COutput& out, vecOutputs) 
        {
            if (!out.coin.IsFinal() || (!out.coin.IsTrusted() && out.nDepth == 0))
            {
                nTotal+=out.coin.m_TXOut.nValue;
            }
        }
        return nTotal;
    }
/* MCHN END*/
    {
        LOCK2(cs_main, cs_wallet);
        for (map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
        {
            const CWalletTx* pcoin = &(*it).second;
            if (!IsFinalTx(*pcoin) || (!pcoin->IsTrusted() && pcoin->GetDepthInMainChain() == 0))
                nTotal += pcoin->GetAvailableCredit();
        }
    }
    return nTotal;
}

CAmount CWallet::GetImmatureBalance() const
{
//    if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)                          // Not supported, used only in QT
    CAmount nTotal = 0;
    {
        LOCK2(cs_main, cs_wallet);
        for (map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
        {
            const CWalletTx* pcoin = &(*it).second;
            nTotal += pcoin->GetImmatureCredit();
        }
    }
    return nTotal;
}

CAmount CWallet::GetWatchOnlyBalance() const
{
//    if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)                          // Not supported, used only in QT
    CAmount nTotal = 0;
    {
        LOCK2(cs_main, cs_wallet);
        for (map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
        {
            const CWalletTx* pcoin = &(*it).second;
            if (pcoin->IsTrusted())
                nTotal += pcoin->GetAvailableWatchOnlyCredit();
        }
    }

    return nTotal;
}

CAmount CWallet::GetUnconfirmedWatchOnlyBalance() const
{
//    if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)                          // Not supported, used only in QT
    CAmount nTotal = 0;
    {
        LOCK2(cs_main, cs_wallet);
        for (map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
        {
            const CWalletTx* pcoin = &(*it).second;
            if (!IsFinalTx(*pcoin) || (!pcoin->IsTrusted() && pcoin->GetDepthInMainChain() == 0))
                nTotal += pcoin->GetAvailableWatchOnlyCredit();
        }
    }
    return nTotal;
}

CAmount CWallet::GetImmatureWatchOnlyBalance() const
{
//    if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)                          // Not supported, used only in QT
    CAmount nTotal = 0;
    {
        LOCK2(cs_main, cs_wallet);
        for (map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
        {
            const CWalletTx* pcoin = &(*it).second;
            nTotal += pcoin->GetImmatureWatchOnlyCredit();
        }
    }
    return nTotal;
}


/**
 * populate vCoins with vector of available COutputs.
 */
void CWallet::AvailableCoins(vector<COutput>& vCoins, bool fOnlyConfirmed, const CCoinControl *coinControl, bool fOnlyUnlocked, bool fOnlyCoinsNoTxs, 
                             uint160 addr, const set<uint160>* addresses, uint32_t flags) const
{
    vCoins.clear();

/* MCHN START */        
    if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)
//    if(false)        
    {
        LOCK2(cs_main, cs_wallet);
//    std::map<COutPoint, mc_Coin> m_UTXOs[MC_TDB_MAX_IMPORTS];    
        pwalletTxsMain->Lock();
        if(fOnlyCoinsNoTxs)
        {
            for (map<COutPoint, mc_Coin>::const_iterator it = pwalletTxsMain->m_UTXOs[0].begin(); it != pwalletTxsMain->m_UTXOs[0].end(); ++it)
            {
                const mc_Coin& coin = it->second;
                if( ( (addresses == NULL) && (addr == 0) ) || 
                    (addr == coin.m_EntityID) || 
                    ( (addresses != NULL) && (addresses->count(coin.m_EntityID) != 0)) )
                {
                    isminetype mine;
                    bool is_p2sh=false;
                    if(coin.m_EntityType)
                    {
                        if(coin.m_Flags & MC_TFL_IS_SPENDABLE)
                        {
                            mine=ISMINE_SPENDABLE;
                        }
                        else
                        {
                            mine=ISMINE_WATCH_ONLY;
                        }
                        if( (coin.m_EntityType & MC_TET_TYPE_MASK) == MC_TET_SCRIPT_ADDRESS )
                        {
                            is_p2sh=true;
                        }
                    
                    }
                    else
                    {
                        mine=IsMine(coin.m_TXOut);
                        is_p2sh=coin.m_TXOut.scriptPubKey.IsPayToScriptHash();
                    }
                    if(flags & MC_CSF_ALLOW_NOT_SPENDABLE)
                    {
                        mine=ISMINE_SPENDABLE;                                
                    }
                    else
                    {
                        if(is_p2sh)
                        {
                            if((flags & MC_CSF_ALLOW_SPENDABLE_P2SH) == 0)
                            {
                                mine=ISMINE_NO;
                            }
                            else
                            {
                                if(flags & MC_CSF_ALLOW_NOT_SPENDABLE_P2SH)
                                {
                                    mine=ISMINE_SPENDABLE;
                                }
                            }
                        }                            
                    }
                    
                    mc_Coin coin_fixed;
                    bool use_fixed_coin=false;
                    if(flags & MC_CSF_ALLOWED_COINS_ARE_MINE)
                    {
                        if((mine & ISMINE_SPENDABLE) != ISMINE_NO)
                        {
                            coin_fixed=coin;
                            coin_fixed.m_Flags |= MC_TFL_IS_MINE_FOR_THIS_SEND;
                            use_fixed_coin=true;
                        }
                    }
                    
                    uint256 txid=coin.m_OutPoint.hash;
                    uint32_t vout=coin.m_OutPoint.n;
                    int nDepth=coin.GetDepthInMainChain();
//                    LogPrintf("DEBUG: %s %d %d %d\n",coin.m_OutPoint.ToString().c_str(),coin.GetDepthInMainChain(),coin.IsFinal(),coin.BlocksToMaturity());
//                    LogPrintf("DEBUG: %s\n",coin.ToString().c_str());
                    if ( (coin.IsFinal()) && 
                         ((coin.m_Flags & MC_TFL_IS_LICENSE_TOKEN) == 0) &&   
                         (coin.BlocksToMaturity() <= 0) &&
                         (mine != ISMINE_NO) &&
                         (!fOnlyUnlocked || !IsLockedCoin(txid, vout)) && 
                         (!coinControl || !coinControl->HasSelected() || coinControl->IsSelected(txid, vout)))
                    {
                        vCoins.push_back(COutput(NULL, vout, nDepth, (mine & ISMINE_SPENDABLE) != ISMINE_NO, use_fixed_coin ? coin_fixed : coin));                
                    }            
                }
            }                            
        }
        else
        {
            vector<mc_Coin> vCoinsToTake;
            pwalletTxsMain->vAvailableCoins.clear();

            for (map<COutPoint, mc_Coin>::const_iterator it = pwalletTxsMain->m_UTXOs[0].begin(); it != pwalletTxsMain->m_UTXOs[0].end(); ++it)
            {
                const mc_Coin& coin = it->second;
                isminetype mine = IsMine(coin.m_TXOut);
                bool is_p2sh=coin.m_TXOut.scriptPubKey.IsPayToScriptHash();
                if(flags & MC_CSF_ALLOW_NOT_SPENDABLE)
                {
                    mine=ISMINE_SPENDABLE;                                
                }
                else
                {
                    if(is_p2sh)
                    {
                        if((flags & MC_CSF_ALLOW_SPENDABLE_P2SH) == 0)
                        {
                            mine=ISMINE_NO;
                        }
                        else
                        {
                            if(flags & MC_CSF_ALLOW_NOT_SPENDABLE_P2SH)
                            {
                                mine=ISMINE_SPENDABLE;
                            }
                        }
                    }                            
                }
                
                uint256 txid=coin.m_OutPoint.hash;
                uint32_t vout=coin.m_OutPoint.n;
                if ( (coin.IsFinal()) && 
                     (coin.BlocksToMaturity() <= 0) &&
                     (mine != ISMINE_NO) &&
                     (!fOnlyUnlocked || !IsLockedCoin(txid, vout)) && 
                     (!coinControl || !coinControl->HasSelected() || coinControl->IsSelected(txid, vout)))
                {
                    const CWalletTx& wtx=pwalletTxsMain->GetInternalWalletTx(txid,NULL,NULL);
                    std::map<uint256, CWalletTx>::const_iterator itold = pwalletTxsMain->vAvailableCoins.find(txid);
                    if (itold == pwalletTxsMain->vAvailableCoins.end())
                    {
                        pwalletTxsMain->vAvailableCoins.insert(make_pair(txid, wtx));
                    }                    
                    vCoinsToTake.push_back(coin);
                }            
            }                
            for (unsigned int i=0;i<vCoinsToTake.size();i++)
            {
                const mc_Coin& coin = vCoinsToTake[i];
                isminetype mine = IsMine(coin.m_TXOut);
                int nDepth=coin.GetDepthInMainChain();
                uint256 txid=coin.m_OutPoint.hash;
                uint32_t vout=coin.m_OutPoint.n;
                std::map<uint256, CWalletTx>::const_iterator itold = pwalletTxsMain->vAvailableCoins.find(txid);
                const CWalletTx* pwtx=&(*itold).second;
                vCoins.push_back(COutput(pwtx, vout, nDepth, (mine & ISMINE_SPENDABLE) != ISMINE_NO));                
            }                
        }
        pwalletTxsMain->UnLock();
    }    
    else
    {
/* MCHN END */        
        LOCK2(cs_main, cs_wallet);
/* MCHN START */        
        LogPrint("mchn","mchn: Wallet coins: Total: %d, Unspent: %d\n",mapWallet.size(),mapUnspent.size());
//        for (map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
//        {
//            const uint256& wtxid = it->first;
            
        for (map<uint256, int>::const_iterator itUnspent = mapUnspent.begin(); itUnspent != mapUnspent.end(); ++itUnspent)
        {
            const uint256& wtxid = itUnspent->first;
            std::map<uint256, CWalletTx>::const_iterator it = mapWallet.find(wtxid);
            
            if (it == mapWallet.end())
                continue;
/* MCHN END */                
            const CWalletTx* pcoin = &(*it).second;

            if (!IsFinalTx(*pcoin))
            {
//                printf("A\n");
                continue;
            }

            if (fOnlyConfirmed && !pcoin->IsTrusted())
            {
//                printf("B\n");
                continue;
            }

            if (pcoin->IsCoinBase() && pcoin->GetBlocksToMaturity() > 0)
            {
//                printf("C\n");
                continue;
            }

            int nDepth = pcoin->GetDepthInMainChain();
            if (nDepth < 0)
            {
//                printf("D %s\n",pcoin->GetHash().GetHex().c_str());
                continue;
            }

/* MCHN START */            
            if(mc_gState->m_NetworkParams->IsProtocolMultichain())
            {
                for (unsigned int i = 0; i < pcoin->vout.size(); i++) {
                    isminetype mine = IsMine(pcoin->vout[i]);
                    if (!(IsSpent(wtxid, i)) && mine != ISMINE_NO &&
                        (!fOnlyUnlocked || !IsLockedCoin((*it).first, i)) && pcoin->vout[i].nValue >= 0 && 
                        (!coinControl || !coinControl->HasSelected() || coinControl->IsSelected((*it).first, i)))
                            vCoins.push_back(COutput(pcoin, i, nDepth, (mine & ISMINE_SPENDABLE) != ISMINE_NO));
                }
            }
            else
            {
/* MCHN END */            
            
            for (unsigned int i = 0; i < pcoin->vout.size(); i++) {
                isminetype mine = IsMine(pcoin->vout[i]);
                if (!(IsSpent(wtxid, i)) && mine != ISMINE_NO &&
                    (!fOnlyUnlocked || !IsLockedCoin((*it).first, i)) && pcoin->vout[i].nValue > 0 && 
                    (!coinControl || !coinControl->HasSelected() || coinControl->IsSelected((*it).first, i)))
                        vCoins.push_back(COutput(pcoin, i, nDepth, (mine & ISMINE_SPENDABLE) != ISMINE_NO));
            }
/* MCHN START */            
            }
/* MCHN END */            
        }
    }
}

static void ApproximateBestSubset(vector<pair<CAmount, pair<const CWalletTx*,unsigned int> > >vValue, const CAmount& nTotalLower, const CAmount& nTargetValue,
                                  vector<char>& vfBest, CAmount& nBest, int iterations = 1000)
{
    vector<char> vfIncluded;

    vfBest.assign(vValue.size(), true);
    nBest = nTotalLower;

    seed_insecure_rand();

    for (int nRep = 0; nRep < iterations && nBest != nTargetValue; nRep++)
    {
        vfIncluded.assign(vValue.size(), false);
        CAmount nTotal = 0;
        bool fReachedTarget = false;
        for (int nPass = 0; nPass < 2 && !fReachedTarget; nPass++)
        {
            for (unsigned int i = 0; i < vValue.size(); i++)
            {
                //The solver here uses a randomized algorithm,
                //the randomness serves no real security purpose but is just
                //needed to prevent degenerate behavior and it is important
                //that the rng is fast. We do not use a constant random sequence,
                //because there may be some privacy improvement by making
                //the selection random.
                if (nPass == 0 ? insecure_rand()&1 : !vfIncluded[i])
                {
                    nTotal += vValue[i].first;
                    vfIncluded[i] = true;
                    if (nTotal >= nTargetValue)
                    {
                        fReachedTarget = true;
                        if (nTotal < nBest)
                        {
                            nBest = nTotal;
                            vfBest = vfIncluded;
                        }
                        nTotal -= vValue[i].first;
                        vfIncluded[i] = false;
                    }
                }
            }
        }
    }
}

static void ApproximateBestSubset(vector<pair<CAmount, pair<uint256,unsigned int> > >vValue, const CAmount& nTotalLower, const CAmount& nTargetValue,
                                  vector<char>& vfBest, CAmount& nBest, int iterations = 1000)
{
    vector<char> vfIncluded;

    vfBest.assign(vValue.size(), true);
    nBest = nTotalLower;

    seed_insecure_rand();

    for (int nRep = 0; nRep < iterations && nBest != nTargetValue; nRep++)
    {
        vfIncluded.assign(vValue.size(), false);
        CAmount nTotal = 0;
        bool fReachedTarget = false;
        for (int nPass = 0; nPass < 2 && !fReachedTarget; nPass++)
        {
            for (unsigned int i = 0; i < vValue.size(); i++)
            {
                //The solver here uses a randomized algorithm,
                //the randomness serves no real security purpose but is just
                //needed to prevent degenerate behavior and it is important
                //that the rng is fast. We do not use a constant random sequence,
                //because there may be some privacy improvement by making
                //the selection random.
                if (nPass == 0 ? insecure_rand()&1 : !vfIncluded[i])
                {
                    nTotal += vValue[i].first;
                    vfIncluded[i] = true;
                    if (nTotal >= nTargetValue)
                    {
                        fReachedTarget = true;
                        if (nTotal < nBest)
                        {
                            nBest = nTotal;
                            vfBest = vfIncluded;
                        }
                        nTotal -= vValue[i].first;
                        vfIncluded[i] = false;
                    }
                }
            }
        }
    }
}


bool CWallet::SelectCoinsMinConf(const CAmount& nTargetValue, int nConfMine, int nConfTheirs, vector<COutput> vCoins,
                                 set<pair<const CWalletTx*,unsigned int> >& setCoinsRet, CAmount& nValueRet) const
{
    setCoinsRet.clear();
    nValueRet = 0;

    // List of values less than target
    pair<CAmount, pair<const CWalletTx*,unsigned int> > coinLowestLarger;
    coinLowestLarger.first = std::numeric_limits<CAmount>::max();
    coinLowestLarger.second.first = NULL;
    vector<pair<CAmount, pair<const CWalletTx*,unsigned int> > > vValue;
    CAmount nTotalLower = 0;

    random_shuffle(vCoins.begin(), vCoins.end(), GetRandInt);

    BOOST_FOREACH(const COutput &output, vCoins)
    {
        if (!output.fSpendable)
            continue;

/* MCHN START */        
        if(!OutputCanSend(output))
        {
            continue;
        }
/* MCHN END */        
        
        const CWalletTx *pcoin = output.tx;

        if (output.nDepth < (pcoin->IsFromMe(ISMINE_ALL) ? nConfMine : nConfTheirs))
            continue;

        int i = output.i;
        CAmount n = pcoin->vout[i].nValue;

        pair<CAmount,pair<const CWalletTx*,unsigned int> > coin = make_pair(n,make_pair(pcoin, i));

        if (n == nTargetValue)
        {
            setCoinsRet.insert(coin.second);
            nValueRet += coin.first;
            return true;
        }
        else if (n < nTargetValue + CENT)
        {
            vValue.push_back(coin);
            nTotalLower += n;
        }
        else if (n < coinLowestLarger.first)
        {
            coinLowestLarger = coin;
        }
    }

    if (nTotalLower == nTargetValue)
    {
        for (unsigned int i = 0; i < vValue.size(); ++i)
        {
            setCoinsRet.insert(vValue[i].second);
            nValueRet += vValue[i].first;
        }
        return true;
    }

    if (nTotalLower < nTargetValue)
    {
        if (coinLowestLarger.second.first == NULL)
            return false;
        setCoinsRet.insert(coinLowestLarger.second);
        nValueRet += coinLowestLarger.first;
        return true;
    }

    // Solve subset sum by stochastic approximation
    sort(vValue.rbegin(), vValue.rend(), CompareValueOnly());
    vector<char> vfBest;
    CAmount nBest;

    ApproximateBestSubset(vValue, nTotalLower, nTargetValue, vfBest, nBest, 1000);
    if (nBest != nTargetValue && nTotalLower >= nTargetValue + CENT)
        ApproximateBestSubset(vValue, nTotalLower, nTargetValue + CENT, vfBest, nBest, 1000);

    // If we have a bigger coin and (either the stochastic approximation didn't find a good solution,
    //                                   or the next bigger coin is closer), return the bigger coin
    if (coinLowestLarger.second.first &&
        ((nBest != nTargetValue && nBest < nTargetValue + CENT) || coinLowestLarger.first <= nBest))
    {
        setCoinsRet.insert(coinLowestLarger.second);
        nValueRet += coinLowestLarger.first;
    }
    else {
        for (unsigned int i = 0; i < vValue.size(); i++)
            if (vfBest[i])
            {
                setCoinsRet.insert(vValue[i].second);
                nValueRet += vValue[i].first;
            }

        LogPrint("selectcoins", "SelectCoins() best subset: ");
        for (unsigned int i = 0; i < vValue.size(); i++)
            if (vfBest[i])
                LogPrint("selectcoins", "%s ", FormatMoney(vValue[i].first));
        LogPrint("selectcoins", "total %s\n", FormatMoney(nBest));
    }

    return true;
}

bool CWallet::SelectCoins(const CAmount& nTargetValue, set<pair<const CWalletTx*,unsigned int> >& setCoinsRet, CAmount& nValueRet, const CCoinControl* coinControl) const
{
    vector<COutput> vCoins;
    AvailableCoins(vCoins, true, coinControl);

    // coin control -> return all selected outputs (we want all selected to go into the transaction for sure)
    if (coinControl && coinControl->HasSelected())
    {
        BOOST_FOREACH(const COutput& out, vCoins)
        {
            if(!out.fSpendable)
                continue;
            nValueRet += out.tx->vout[out.i].nValue;
            setCoinsRet.insert(make_pair(out.tx, out.i));
        }
        return (nValueRet >= nTargetValue);
    }

    return (SelectCoinsMinConf(nTargetValue, 1, 6, vCoins, setCoinsRet, nValueRet) ||
            SelectCoinsMinConf(nTargetValue, 1, 1, vCoins, setCoinsRet, nValueRet) ||
            (bSpendZeroConfChange && SelectCoinsMinConf(nTargetValue, 0, 1, vCoins, setCoinsRet, nValueRet)));
}

/**
 * This is not used anymore
 */


bool CWallet::CreateTransaction(const vector<pair<CScript, CAmount> >& vecSend,
                                CWalletTx& wtxNew, CReserveKey& reservekey, CAmount& nFeeRet, std::string& strFailReason, const CCoinControl* coinControl)
{
    CAmount nValue = 0;
    BOOST_FOREACH (const PAIRTYPE(CScript, CAmount)& s, vecSend)
    {
        if (nValue < 0)
        {
            strFailReason = _("Transaction amounts must be positive");
            return false;
        }
        nValue += s.second;
    }
    if (vecSend.empty() || nValue < 0)
    {
        strFailReason = _("Transaction amounts must be positive");
        return false;
    }
    
    wtxNew.fTimeReceivedIsTxTime = true;
    wtxNew.BindWallet(this);
    CMutableTransaction txNew;

    {
        LOCK2(cs_main, cs_wallet);
        {
            nFeeRet = 0;
            while (true)
            {
                txNew.vin.clear();
                txNew.vout.clear();
                wtxNew.fFromMe = true;

                CAmount nTotalValue = nValue + nFeeRet;
                double dPriority = 0;
                // vouts to the payees
                BOOST_FOREACH (const PAIRTYPE(CScript, CAmount)& s, vecSend)
                {
                    CTxOut txout(s.second, s.first);
/* MCHN START */                    

                    uint32_t type,from,to,timestamp,type_ored,no_dust_check;
                    
                    mc_gState->m_TmpScript->Clear();
        
                    const CScript& script1 = txout.scriptPubKey;        
                    CScript::const_iterator pc1 = script1.begin();

                    mc_gState->m_TmpScript->SetScript((unsigned char*)(&pc1[0]),(size_t)(script1.end()-pc1),MC_SCR_TYPE_SCRIPTPUBKEY);
    
                    type_ored=0;
                    no_dust_check=mc_gState->m_TmpScript->IsOpReturnScript();
                    for (int e = 0; e < mc_gState->m_TmpScript->GetNumElements(); e++)
                    {
                        mc_gState->m_TmpScript->SetElement(e);
                        if(mc_gState->m_TmpScript->GetPermission(&type,&from,&to,&timestamp) == 0)
                        {
                            if(from >= to)
                            {
                                no_dust_check=1;
                            }
                            type_ored |= type;
                        }
                    }
                    
                    
                    if((type_ored == 0))// || (type_ored & MC_PTP_RECEIVE))
                    {
                        if(no_dust_check == 0)
                        {
                            ::minRelayTxFee = CFeeRate(MIN_RELAY_TX_FEE);    
                            
/* MCHN END */                    
                    
                    if (txout.IsDust(::minRelayTxFee))
                    {
                        strFailReason = _("Transaction amount too small");
                        return false;
                    }
/* MCHN START */                    
                        }
                    }
/* MCHN END */                    
                    txNew.vout.push_back(txout);
                }

                // Choose coins to use
                set<pair<const CWalletTx*,unsigned int> > setCoins;
                CAmount nValueIn = 0;
                if (!SelectCoins(nTotalValue, setCoins, nValueIn, coinControl))
                {
                    strFailReason = _("Insufficient funds");
                    return false;
                }
                BOOST_FOREACH(PAIRTYPE(const CWalletTx*, unsigned int) pcoin, setCoins)
                {
                    CAmount nCredit = pcoin.first->vout[pcoin.second].nValue;
                    //The coin age after the next block (depth+1) is used instead of the current,
                    //reflecting an assumption the user would accept a bit more delay for
                    //a chance at a free transaction.
                    //But mempool inputs might still be in the mempool, so their age stays 0
                    int age = pcoin.first->GetDepthInMainChain();
                    if (age != 0)
                        age += 1;
                    dPriority += (double)nCredit * age;
                }

                CAmount nChange = nValueIn - nValue - nFeeRet;

                if (nChange > 0)
                {
                    // Fill a vout to ourself
                    // TODO: pass in scriptChange instead of reservekey so
                    // change transaction isn't always pay-to-bitcoin-address
                    CScript scriptChange;

                    // coin control: send change to custom address
                    if (coinControl && !boost::get<CNoDestination>(&coinControl->destChange))
                        scriptChange = GetScriptForDestination(coinControl->destChange);

                    // no coin control: send change to newly generated address
                    else
                    {
                        // Note: We use a new key here to keep it from being obvious which side is the change.
                        //  The drawback is that by not reusing a previous key, the change may be lost if a
                        //  backup is restored, if the backup doesn't have the new private key for the change.
                        //  If we reused the old key, it would be possible to add code to look for and
                        //  rediscover unknown transactions that were written with keys of ours to recover
                        //  post-backup change.

                        // Reserve a new key pair from key pool
                        CPubKey vchPubKey;
                        bool ret;
                        ret = reservekey.GetReservedKey(vchPubKey);
                        assert(ret); // should never fail, as we just unlocked
/* MCHN START */                        
// Using default key - cannot use other keys from pool as they don't have permission                       
//                        vchPubKey=vchDefaultKey;
                        
                        if(!GetKeyFromAddressBook(vchPubKey,MC_PTP_RECEIVE))
                        {
                            LogPrintf("mchn: Internal error: Cannot find address for change having receive permission\n");
                            strFailReason = _("Change address not found");
                            return false;
                        }
                        
/* MCHN END */                        
                        scriptChange = GetScriptForDestination(vchPubKey.GetID());
                    }

                    CTxOut newTxOut(nChange, scriptChange);

                    // Never create dust outputs; if we would, just
                    // add the dust to the fee.
                    if (newTxOut.IsDust(::minRelayTxFee))
                    {
                        nFeeRet += nChange;
                        reservekey.ReturnKey();
                    }
                    else
                    {
                        // Insert change txn at random position:
                        vector<CTxOut>::iterator position = txNew.vout.begin()+GetRandInt(txNew.vout.size()+1);
                        txNew.vout.insert(position, newTxOut);
                    }
                }
                else
                    reservekey.ReturnKey();

                // Fill vin
                BOOST_FOREACH(const PAIRTYPE(const CWalletTx*,unsigned int)& coin, setCoins)
                    txNew.vin.push_back(CTxIn(coin.first->GetHash(),coin.second));

                // Sign
                int nIn = 0;
                BOOST_FOREACH(const PAIRTYPE(const CWalletTx*,unsigned int)& coin, setCoins)
                    if (!SignSignature(*this, *coin.first, txNew, nIn++))
                    {                        
                        strFailReason = _("Signing transaction failed");
                        return false;
                    }

                // Embed the constructed transaction data in wtxNew.
                *static_cast<CTransaction*>(&wtxNew) = CTransaction(txNew);

                // Limit size
                unsigned int nBytes = ::GetSerializeSize(*(CTransaction*)&wtxNew, SER_NETWORK, PROTOCOL_VERSION);
                if (nBytes >= MAX_STANDARD_TX_SIZE)
                {
                    strFailReason = _("Transaction too large");
                    return false;
                }
                dPriority = wtxNew.ComputePriority(dPriority, nBytes);

                // Can we complete this as a free transaction?
                if (fSendFreeTransactions && nBytes <= MAX_FREE_TRANSACTION_CREATE_SIZE)
                {
                    // Not enough fee: enough priority?
                    double dPriorityNeeded = mempool.estimatePriority(nTxConfirmTarget);
                    // Not enough mempool history to estimate: use hard-coded AllowFree.
                    if (dPriorityNeeded <= 0 && AllowFree(dPriority))
                        break;

                    // Small enough, and priority high enough, to send for free
                    if (dPriorityNeeded > 0 && dPriority >= dPriorityNeeded)
                        break;
                }

                CAmount nFeeNeeded = GetMinimumFee(nBytes, nTxConfirmTarget, mempool);

                // If we made it here and we aren't even able to meet the relay fee on the next pass, give up
                // because we must be at the maximum allowed fee.
                if (nFeeNeeded < ::minRelayTxFee.GetFee(nBytes))
                {
                    strFailReason = _("Transaction too large for fee policy");
                    return false;
                }

                if (nFeeRet >= nFeeNeeded)
                    break; // Done, enough fee included.

                // Include more fee and try again.
                nFeeRet = nFeeNeeded;
                continue;
            }
        }
    }
    return true;
}

bool CWallet::CreateTransaction(CScript scriptPubKey, const CAmount& nValue,
                                CWalletTx& wtxNew, CReserveKey& reservekey, CAmount& nFeeRet, std::string& strFailReason, const CCoinControl* coinControl)
{
    vector< pair<CScript, CAmount> > vecSend;
    vecSend.push_back(make_pair(scriptPubKey, nValue));
    return CreateTransaction(vecSend, wtxNew, reservekey, nFeeRet, strFailReason, coinControl);
}

/* MCHN START */

bool CWallet::CreateTransaction(CScript scriptPubKey, const CAmount& nValue, CScript scriptOpReturn,
                                CWalletTx& wtxNew, CReserveKey& reservekey, CAmount& nFeeRet, std::string& strFailReason, const CCoinControl* coinControl,
                                const set<CTxDestination>* addresses,int min_conf,int min_inputs,int max_inputs,const vector<COutPoint>* lpCoinsToUse, int *eErrorCode)
{
    vector< pair<CScript, CAmount> > vecSend;
    CAmount nAmount=nValue;
    int eErrorCode1=0; 
    if(nAmount < 0)
    {
        minRelayTxFee = CFeeRate(MIN_RELAY_TX_FEE);    
        CTxOut txout(nAmount, scriptPubKey);
        nAmount=txout.GetDustThreshold(minRelayTxFee);
    }
    vecSend.push_back(make_pair(scriptPubKey, nAmount));
    if(scriptOpReturn.size())
    {
        vecSend.push_back(make_pair(scriptOpReturn, 0));
    }
    return CreateMultiChainTransaction(vecSend, wtxNew, reservekey, nFeeRet, strFailReason, coinControl, addresses, min_conf, min_inputs, max_inputs, lpCoinsToUse, 
    (eErrorCode != 0) ? eErrorCode : & eErrorCode1);
}

bool CWallet::CreateTransaction(std::vector<CScript> scriptPubKeys, const CAmount& nValue, CScript scriptOpReturn,
                                CWalletTx& wtxNew, CReserveKey& reservekey, CAmount& nFeeRet, std::string& strFailReason, const CCoinControl* coinControl,
                                const set<CTxDestination>* addresses,int min_conf,int min_inputs,int max_inputs,const vector<COutPoint>* lpCoinsToUse, int *eErrorCode)
{
    vector< pair<CScript, CAmount> > vecSend;
    BOOST_FOREACH (const CScript& scriptPubKey, scriptPubKeys)
    {
        CAmount nAmount=nValue;
        if(nAmount < 0)
        {
            minRelayTxFee = CFeeRate(MIN_RELAY_TX_FEE);    
            CTxOut txout(nAmount, scriptPubKey);
            nAmount=txout.GetDustThreshold(minRelayTxFee);
        }
        vecSend.push_back(make_pair(scriptPubKey, nAmount));
    }

    if(scriptOpReturn.size())
    {
        vecSend.push_back(make_pair(scriptOpReturn, 0));
    }
    return CreateMultiChainTransaction(vecSend, wtxNew, reservekey, nFeeRet, strFailReason, coinControl, addresses, min_conf, min_inputs, max_inputs,lpCoinsToUse,eErrorCode);
}

int CWallet::SelectMultiChainCombineCoinsMinConf(int nConfMine, int nConfTheirs, vector<COutput> vCoins, mc_Buffer *in_map, mc_Buffer *in_amounts,
                                int in_selected_row,int in_asset_row,
                                set<pair<const CWalletTx*,unsigned int> >& setCoinsRet,int max_inputs) const
{
    setCoinsRet.clear();
    vector<pair<int, pair<const CWalletTx*,unsigned int> > > vValue;
    bool take_it;
    int coin_id=-1;
    unsigned char buf_map[40];
    
    BOOST_FOREACH(const COutput &output, vCoins)
    {
        take_it=true;
        
        if(take_it)
        {
            if (!output.fSpendable)
            {
                take_it=false;
            }
        }
        
        const CWalletTx *pcoin = output.tx;

        if(take_it)
        {
            if (output.nDepth < (pcoin->IsFromMe(ISMINE_ALL) ? nConfMine : nConfTheirs))
            {
                take_it=false;
            }
        }
        
                                                                                // Retrieving coin index in the matrix
        if(take_it)
        {
            uint256 hash=output.tx->GetHash();
            int out_i;
            out_i=output.i;
            
            memcpy(buf_map,&hash,32);
            mc_PutLE(buf_map+32,&out_i,4);
            int row=in_map->Seek(buf_map);
            if(row >= 0)
            {
                coin_id=mc_GetLE(in_map->GetRow(row)+36,4);
            }
            else
            {
                take_it=false;                
            }
        }
        
                                                                                // Checking that the coin is not selected yet
        if(take_it)
        {
            if(mc_GetABCoinQuantity(in_amounts->GetRow(in_selected_row),coin_id))
            {
                take_it=false;                
            }            
        }
        

        int i = output.i;

        if(take_it)
        {
                                                                                // No changes below this line
            pair<int,pair<const CWalletTx*,unsigned int> > coin = make_pair(output.nDepth,make_pair(pcoin, i));

            vValue.push_back(coin);
        }
    }
    
    
    if((int)vValue.size() <= max_inputs)
    {
        for (unsigned int i = 0; i < vValue.size(); i++)
        {
            setCoinsRet.insert(vValue[i].second);
        }
        return vValue.size();
    }
    
    sort(vValue.rbegin(), vValue.rend(), CompareValueOnlyIntDesc());
    
    for (int i = 0; i < max_inputs; i++)
    {
        setCoinsRet.insert(vValue[i].second);
    }

    return vValue.size();
}


bool CWallet::SelectMultiChainCoinsMinConf(const CAmount& nTargetValue, int nConfMine, int nConfTheirs, vector<COutput> vCoins, mc_Buffer *in_map, mc_Buffer *in_amounts,
                                int in_selected_row,int in_asset_row,int in_preferred_row,
                                set<pair<uint256,unsigned int> >& setCoinsRet, CAmount& nValueRet) const
{
//    printf("SelectMultiChainCoinsMinConf\n");
    setCoinsRet.clear();
    nValueRet = 0;

    // List of values less than target
    pair<CAmount, pair<uint256,unsigned int> > coinLowestLarger;
    coinLowestLarger.first = std::numeric_limits<CAmount>::max();
    coinLowestLarger.second.first = 0;
    vector<pair<CAmount, pair<uint256,unsigned int> > > vValue;
    CAmount nTotalLower = 0;
    bool take_it;
    CAmount local_cent=CENT;
    if(in_asset_row != 3)
    {
        local_cent=0;
    }
    int coin_id=-1;
    unsigned char buf_map[40];
    int64_t quantity;
    
    random_shuffle(vCoins.begin(), vCoins.end(), GetRandInt);
    
    BOOST_FOREACH(const COutput &output, vCoins)
    {
        take_it=true;
        quantity=0;
        
//        printf("Coin: %s-%d\n",output.tx->GetHash().ToString().c_str(),output.i);
        if(take_it)
        {
            if (!output.fSpendable)
            {
//                printf("Not spendable\n");
                take_it=false;
            }
        }
        
        const CWalletTx *pcoin = output.tx;

        if(take_it)
        {
            if(pcoin)
            {
                if (output.nDepth < (pcoin->IsFromMe(ISMINE_ALL) ? nConfMine : nConfTheirs))
                {
    //                printf("Not enough confitmations\n");
                    take_it=false;
                }
            }
            else
            {
                if(output.coin.m_Flags & MC_TFL_IS_MINE_FOR_THIS_SEND)
                {
                    if (output.nDepth < nConfMine)
                    {
                        take_it=false;
                    }                                    
                }
                else
                {
                    if (!output.coin.IsTrusted())
                    {
        //                printf("Not spendable\n");
                        take_it=false;
                    }
                    if (output.nDepth < (((output.coin.m_Flags & MC_TFL_FROM_ME) > 0) ? nConfMine : nConfTheirs))
                    {
        //                printf("Not enough confitmations\n");
                        take_it=false;
                    }                
                }
            }
        }
        
                                                                                // Retrieving coin index in the matrix
        uint256 hash;
        if(pcoin)
        {
            hash=pcoin->GetHash();
        }
        else
        {
            hash=output.coin.m_OutPoint.hash;
        }
        
        if(take_it)
        {
            int out_i;
            out_i=output.i;
            
            memcpy(buf_map,&hash,32);
            mc_PutLE(buf_map+32,&out_i,4);
            int row=in_map->Seek(buf_map);
            if(row >= 0)
            {
                coin_id=mc_GetLE(in_map->GetRow(row)+36,4);
//                printf("Coin ID: %d\n",coin_id);
            }
            else
            {
//                printf("Not found in map\n");
                take_it=false;                
            }
        }
        
                                                                                // Checking that the coin is not selected yet
        if(take_it)
        {
            if(mc_GetABCoinQuantity(in_amounts->GetRow(in_selected_row),coin_id))
            {
//                printf("Already selected\n");
                take_it=false;                
            }            
        }
        
        if(take_it && (in_preferred_row > 0))                                   // Checking that this coin have positive value in "preferred row" - 
                                                                                // relevant asset or "pure native currency" flag
        {
            if(mc_GetABCoinQuantity(in_amounts->GetRow(in_preferred_row),coin_id) == 0)
            {
                take_it=false;                
            }            
        }
                                                                                // Retrieving quantity from matrix
        if(take_it)
        {
            quantity=mc_GetABCoinQuantity(in_amounts->GetRow(in_asset_row),coin_id);
            if(quantity <= 0)
            {
//                printf("Bad quantity: %ld \n",quantity);
                take_it=false;
            }             
            if(quantity == 0)
            {
                if(nTargetValue == 0)
                {
                    pair<CAmount,pair<uint256,unsigned int> > coin = make_pair(quantity,make_pair(hash, output.i));

                    setCoinsRet.insert(coin.second);
                    nValueRet += coin.first;
                    return true;                    
                }
            }
        }
        

        int i = output.i;
        CAmount n = quantity;

        if(take_it)
        {
                                                                                
            pair<CAmount,pair<uint256,unsigned int> > coin = make_pair(n,make_pair(hash, i));

            if (n == nTargetValue)
            {
                setCoinsRet.insert(coin.second);
                nValueRet += coin.first;
                return true;
            }
            else if (n < nTargetValue + local_cent)
            {
                vValue.push_back(coin);
                nTotalLower += n;
            }
            else if (n < coinLowestLarger.first)
            {
                coinLowestLarger = coin;
            }
        }
    }

    if (nTotalLower == nTargetValue)
    {
        for (unsigned int i = 0; i < vValue.size(); ++i)
        {
            setCoinsRet.insert(vValue[i].second);
            nValueRet += vValue[i].first;
        }
        if(setCoinsRet.size() == 0)
        {
            if(nTargetValue == 0)
            {
                if (coinLowestLarger.second.first == 0)
                    return false;
                setCoinsRet.insert(coinLowestLarger.second);
                nValueRet += coinLowestLarger.first;                
                return true;
            }
            return false;
        }
        return true;
    }

    if (nTotalLower < nTargetValue)
    {
        if (coinLowestLarger.second.first == 0)
            return false;
        setCoinsRet.insert(coinLowestLarger.second);
        nValueRet += coinLowestLarger.first;
        return true;
    }
    

    // Solve subset sum by stochastic approximation
    sort(vValue.rbegin(), vValue.rend(), CompareValueOnlyHash());
    vector<char> vfBest;
    CAmount nBest;

    ApproximateBestSubset(vValue, nTotalLower, nTargetValue, vfBest, nBest, 1000);
    if (nBest != nTargetValue && nTotalLower >= nTargetValue + local_cent)
        ApproximateBestSubset(vValue, nTotalLower, nTargetValue + local_cent, vfBest, nBest, 1000);

    // If we have a bigger coin and (either the stochastic approximation didn't find a good solution,
    //                                   or the next bigger coin is closer), return the bigger coin
    if ((coinLowestLarger.second.first != 0) &&
        ((nBest != nTargetValue && nBest < nTargetValue + local_cent) || coinLowestLarger.first <= nBest))
    {
        setCoinsRet.insert(coinLowestLarger.second);
        nValueRet += coinLowestLarger.first;
    }
    else {
        for (unsigned int i = 0; i < vValue.size(); i++)
            if (vfBest[i])
            {
                setCoinsRet.insert(vValue[i].second);
                nValueRet += vValue[i].first;
            }

        LogPrint("selectcoins", "SelectCoins() best subset: ");
        for (unsigned int i = 0; i < vValue.size(); i++)
            if (vfBest[i])
                LogPrint("selectcoins", "%s ", FormatMoney(vValue[i].first));
        LogPrint("selectcoins", "total %s\n", FormatMoney(nBest));
    }

    if(nTargetValue == 0)
    {
        if(setCoinsRet.size() == 0)
        {
            return false;
        }
    }
    
    return true;
}

bool CWallet::SelectMultiChainCoins(const CAmount& nTargetValue, vector<COutput> &vCoins, mc_Buffer *in_map, mc_Buffer *in_amounts, 
                                int in_selected_row,int in_asset_row,int in_preferred_row,
                                set<pair<uint256,unsigned int> >& setCoinsRet, CAmount& nValueRet, const CCoinControl* coinControl) const
{
    int coin_id=0;
    int buf_map[40];
    
    // coin control -> return all selected outputs (we want all selected to go into the transaction for sure)
    if (coinControl && coinControl->HasSelected())
    {
        BOOST_FOREACH(const COutput& out, vCoins)
        {
            if(out.fSpendable)
            {
                uint256 hash=out.tx->GetHash();
                int out_i;
                out_i=out.i;

                memcpy(buf_map,&hash,32);                                       // Retrieving coin index in the matrix
                mc_PutLE(buf_map+32,&out_i,4);
                int row=in_map->Seek(buf_map);
                
                if(row >= 0)
                {
                    coin_id=mc_GetLE(in_map->GetRow(row)+36,4);
                                                                                // Coin is not selected yet
//                    if(mc_GetLE(in_amounts->GetRow(in_selected_row)+coin_id*MC_AST_ASSET_REF_SIZE,MC_AST_ASSET_QUANTITY_SIZE) == 0)// BUG??? 
                    if(mc_GetABCoinQuantity(in_amounts->GetRow(in_selected_row),coin_id) == 0)
                    {
                                                                                // Value is take  from the matrix
                        nValueRet += mc_GetABCoinQuantity(in_amounts->GetRow(in_asset_row),coin_id);
                        setCoinsRet.insert(make_pair(hash, out.i));
                    }
                }
            }
            coin_id++;
        }
        return (nValueRet >= nTargetValue);
    }

    int preferred_row=in_preferred_row;
    for(int attempt=0;attempt<2;attempt++)
    {
        if((SelectMultiChainCoinsMinConf(nTargetValue, 1, 6, vCoins, in_map, in_amounts, in_selected_row, in_asset_row, preferred_row, setCoinsRet, nValueRet) ||
            SelectMultiChainCoinsMinConf(nTargetValue, 1, 1, vCoins, in_map, in_amounts, in_selected_row, in_asset_row, preferred_row, setCoinsRet, nValueRet) ||
            (bSpendZeroConfChange && SelectMultiChainCoinsMinConf(nTargetValue, 0, 1, vCoins, in_map, in_amounts, in_selected_row, in_asset_row, preferred_row, setCoinsRet, nValueRet))))
        {
            return true;
        }
        if(in_preferred_row == 0)
        {
            return false;
        }
        preferred_row=0;
    }
    
    return false;
}




/* MCHN END */

/**
 * Call after CreateTransaction unless you want to abort
 */

bool CWallet::CommitTransaction(CWalletTx& wtxNew, CReserveKey& reservekey, string& reject_reason)
{
    {
        LOCK2(cs_main, cs_wallet);
        LogPrintf("CommitTransaction: %s, vin: %d, vout: %d\n",wtxNew.GetHash().ToString().c_str(),(int)wtxNew.vin.size(),(int)wtxNew.vout.size());
        if(fDebug)LogPrint("wallet","CommitTransaction:\n%s", wtxNew.ToString());
        {
/* MCHN START */            
            if(((mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS) == 0) || (mc_gState->m_WalletMode & MC_WMD_MAP_TXS))
            {
/* MCHN END */            
                // This is only to keep the database open to defeat the auto-flush for the
                // duration of this scope.  This is the only place where this optimization
                // maybe makes sense; please don't do it anywhere else.
                CWalletDB* pwalletdb = fFileBacked ? new CWalletDB(strWalletFile,"r") : NULL;

                // Take key pair from key pool so it won't be used again
                reservekey.KeepKey();

                // Add tx to wallet, because if it has change it's also ours,
                // otherwise just for transaction history.
                AddToWallet(wtxNew);

                // Notify that old coins are spent
                set<CWalletTx*> setCoins;
                BOOST_FOREACH(const CTxIn& txin, wtxNew.vin)
                {
                    CWalletTx &coin = mapWallet[txin.prevout.hash];
                    coin.BindWallet(this);
                    NotifyTransactionChanged(this, coin.GetHash(), CT_UPDATED);
                }

                if (fFileBacked)
                    delete pwalletdb;
/* MCHN START */            
            }
/* MCHN END */            
        }

        // Track how many getdata requests our transaction gets
        // mapRequestCount[wtxNew.GetHash()] = 0;

        // Broadcast

        if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)
        {
//            pwalletTxsMain->m_ChunkDB->FlushSourceChunks(GetArg("-chunkflushmode",MC_CDB_FLUSH_MODE_COMMIT));
            if(pwalletTxsMain->m_ChunkDB->FlushSourceChunks(GetArg("-flushsourcechunks",true) ? (MC_CDB_FLUSH_MODE_FILE | MC_CDB_FLUSH_MODE_DATASYNC) : MC_CDB_FLUSH_MODE_NONE))
            {
                reject_reason="Couldn't store offchain items, probably chunk database is corrupted";                                        
                return false;
            }
        }
        
        if (!wtxNew.AcceptToMemoryPoolReturnReason(false,true,reject_reason))   // MCHN
        {
            // This must not fail. The transaction has already been signed and recorded.
            LogPrintf("CommitTransaction() : Error: Transaction not valid: %s\n",reject_reason.c_str());  // MCHN
            return false;
        }
/*        
        else
        {
            pwalletTxsMain->AddTx(NULL,wtxNew,-1,NULL,-1,0);   
            if(((mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS) == 0) || (mc_gState->m_WalletMode & MC_WMD_MAP_TXS))
            {
                SyncWithWallets(wtxNew, NULL);            
            }
        }
*/    
        for (unsigned int i = 0; i < wtxNew.vin.size(); i++) 
        {
            COutPoint outp=wtxNew.vin[i].prevout;
            UnlockCoin(outp);
        }
        wtxNew.RelayWalletTransaction();

    }
    return true;
}

CAmount CWallet::GetMinimumFee(unsigned int nTxBytes, unsigned int nConfirmTarget, const CTxMemPool& pool)
{
    if(MCP_WITH_NATIVE_CURRENCY == 0)
    {
        return 0;
    }
    // payTxFee is user-set "I want to pay this much"
    CAmount nFeeNeeded = payTxFee.GetFee(nTxBytes);
    // user selected total at least (default=true)
    if (fPayAtLeastCustomFee && nFeeNeeded > 0 && nFeeNeeded < payTxFee.GetFeePerK())
        nFeeNeeded = payTxFee.GetFeePerK();
    // User didn't set: use -txconfirmtarget to estimate...
    if (nFeeNeeded == 0)
        nFeeNeeded = pool.estimateFee(nConfirmTarget).GetFee(nTxBytes);
    // ... unless we don't have enough mempool data, in which case fall
    // back to a hard-coded fee
    if (nFeeNeeded == 0)
        nFeeNeeded = minTxFee.GetFee(nTxBytes);
    // prevent user from paying a non-sense fee (like 1 satoshi): 0 < fee < minRelayFee
    if (nFeeNeeded < ::minRelayTxFee.GetFee(nTxBytes))
        nFeeNeeded = ::minRelayTxFee.GetFee(nTxBytes);
    // But always obey the maximum
    if (nFeeNeeded > maxTxFee)
        nFeeNeeded = maxTxFee;
    return nFeeNeeded;
}




DBErrors CWallet::LoadWallet(bool& fFirstRunRet)
{
    if (!fFileBacked)
        return DB_LOAD_OK;
    fFirstRunRet = false;
    DBErrors nLoadWalletRet = CWalletDB(strWalletFile,"cr+").LoadWallet(this);
    if (nLoadWalletRet == DB_NEED_REWRITE)
    {
        if (RewriteWalletDB(strWalletFile, "\x04pool"))
        {
            LOCK(cs_wallet);
            setKeyPool.clear();
            // Note: can't top-up keypool here, because wallet is locked.
            // User will be prompted to unlock wallet the next operation
            // the requires a new key.
        }
    }

    if (nLoadWalletRet != DB_LOAD_OK)
        return nLoadWalletRet;
    fFirstRunRet = !vchDefaultKey.IsValid();

    uiInterface.LoadWallet(this);

    return DB_LOAD_OK;
}


DBErrors CWallet::ZapWalletTx(std::vector<CWalletTx>& vWtx)
{
    if (!fFileBacked)
        return DB_LOAD_OK;
    DBErrors nZapWalletTxRet = CWalletDB(strWalletFile,"cr+").ZapWalletTx(this, vWtx);
    if (nZapWalletTxRet == DB_NEED_REWRITE)
    {
        if (RewriteWalletDB(strWalletFile, "\x04pool"))
        {
            LOCK(cs_wallet);
            setKeyPool.clear();
            // Note: can't top-up keypool here, because wallet is locked.
            // User will be prompted to unlock wallet the next operation
            // that requires a new key.
        }
    }

    if (nZapWalletTxRet != DB_LOAD_OK)
        return nZapWalletTxRet;

    return DB_LOAD_OK;
}


bool CWallet::SetAddressBook(const CTxDestination& address, const string& strName, const string& strPurpose)
{
    bool fUpdated = false;
    {
        LOCK(cs_wallet); // mapAddressBook
        std::map<CTxDestination, CAddressBookData>::iterator mi = mapAddressBook.find(address);
        fUpdated = mi != mapAddressBook.end();
        mapAddressBook[address].name = strName;
        if (!strPurpose.empty()) /* update purpose only if requested */
            mapAddressBook[address].purpose = strPurpose;
    }
    NotifyAddressBookChanged(this, address, strName, ::IsMine(*this, address) != ISMINE_NO,
                             strPurpose, (fUpdated ? CT_UPDATED : CT_NEW) );
    if (!fFileBacked)
        return false;    
    if (!strPurpose.empty() && !CWalletDB(strWalletFile).WritePurpose(CBitcoinAddress(address).ToString(), strPurpose))
        return false;
    LogPrint("mchn","Stored address %s in address book, account: %s, purpose: %s.\n",CBitcoinAddress(address).ToString().c_str(),strName.c_str(),strPurpose.c_str());
    return CWalletDB(strWalletFile).WriteName(CBitcoinAddress(address).ToString(), strName);
}

bool CWallet::DelAddressBook(const CTxDestination& address)
{
    {
        LOCK(cs_wallet); // mapAddressBook

        if(fFileBacked)
        {
            // Delete destdata tuples associated with address
            std::string strAddress = CBitcoinAddress(address).ToString();
            BOOST_FOREACH(const PAIRTYPE(string, string) &item, mapAddressBook[address].destdata)
            {
                CWalletDB(strWalletFile).EraseDestData(strAddress, item.first);
            }
        }
        mapAddressBook.erase(address);
    }

    NotifyAddressBookChanged(this, address, "", ::IsMine(*this, address) != ISMINE_NO, "", CT_DELETED);

    if (!fFileBacked)
        return false;
    CWalletDB(strWalletFile).ErasePurpose(CBitcoinAddress(address).ToString());
    return CWalletDB(strWalletFile).EraseName(CBitcoinAddress(address).ToString());
}

bool CWallet::SetEKey(const uint256& hashEKey, const CEncryptionKey& ekey)
{
    {
        LOCK(cs_wallet); // mapEKeys
        std::map<uint256, CEncryptionKey>::iterator mi = mapEKeys.find(hashEKey);
        if(mi != mapEKeys.end())
        {
            return false;
        }
        
        mapEKeys.insert(make_pair(hashEKey,ekey));
    }
    LogPrint("mchn","Stored ekey %s in the wallet, type: %2X, purpose: %2x.\n",hashEKey.ToString().c_str(),ekey.m_Type,ekey.m_Purpose);
    return CWalletDB(strWalletFile).WriteEKey(hashEKey, ekey);    
}
    
bool CWallet::DelEKey(const uint256& hashEKey)
{
    {
        LOCK(cs_wallet); 

        mapEKeys.erase(hashEKey);
    }
    return CWalletDB(strWalletFile).EraseEKey(hashEKey);    
}

bool CWallet::SetLicenseRequest(const uint256& hash, const CLicenseRequest& license_request,const uint256& full_hash)
{
    {
        LOCK(cs_wallet); // mapLicenseRequests
        std::map<uint256, CLicenseRequest>::iterator mi = mapLicenseRequests.find(hash);
        if(mi != mapLicenseRequests.end())
        {
            mi->second=license_request;
        }
        else
        {
            mapLicenseRequests.insert(make_pair(hash,license_request));
        }
    }
    if(full_hash == 0)
    {
        LogPrintf("Stored license request %s in the wallet.\n",hash.ToString().c_str());
    }
    else
    {
        LogPrintf("Stored license request %s-%s in the wallet.\n",hash.ToString().c_str(),full_hash.ToString().c_str());        
    }
    return CWalletDB(strWalletFile).WriteLicenseRequest(hash, license_request);        
}
    
bool CWallet::SetLicenseRequestRefCount(const uint256& hash, uint32_t count)
{
    {
        LOCK(cs_wallet); // mapLicenseRequests
        std::map<uint256, CLicenseRequest>::iterator mi = mapLicenseRequests.find(hash);
        if(mi == mapLicenseRequests.end())
        {
            return false;
        }        

        if( (mi->second.m_ReferenceCount == 0) || (count == 0) )
        {
            mi->second.m_ReferenceCount=count;
        }
        
        if(mi->second.m_ReferenceCount)
        {
            mi->second.m_ReferenceCount-=1;
        }
        if(mi->second.m_ReferenceCount)
        {
            LogPrintf("License request %s in the wallet modified, ref count: %d.\n",hash.ToString().c_str(),mi->second.m_ReferenceCount);
            return CWalletDB(strWalletFile).WriteLicenseRequest(hash, mi->second);        
        }
    }    
    return DelLicenseRequest(hash);
}
    
bool CWallet::DelLicenseRequest(const uint256& hash)
{
    {
        LOCK(cs_wallet); 

        mapLicenseRequests.erase(hash);
    }
    LogPrintf("License request %s deleted from wallet.\n",hash.ToString().c_str());
    return CWalletDB(strWalletFile).EraseLicenseRequest(hash);        
}

bool CWallet::SetDefaultKey(const CPubKey &vchPubKey)
{
    if (fFileBacked)
    {
        if (!CWalletDB(strWalletFile).WriteDefaultKey(vchPubKey))
            return false;
    }
    vchDefaultKey = vchPubKey;
    return true;
}

/**
 * Mark old keypool keys as used,
 * and generate all new keys 
 */
bool CWallet::NewKeyPool()
{
    {
        LOCK(cs_wallet);
        CWalletDB walletdb(strWalletFile);
        BOOST_FOREACH(int64_t nIndex, setKeyPool)
            walletdb.ErasePool(nIndex);
        setKeyPool.clear();

        if (IsLocked())
            return false;

        int64_t nKeys = max(GetArg("-keypool", mc_gState->m_NetworkParams->IsProtocolMultichain() ? 1 : 100), (int64_t)0);
        for (int i = 0; i < nKeys; i++)
        {
            int64_t nIndex = i+1;
            walletdb.WritePool(nIndex, CKeyPool(GenerateNewKey()));
            setKeyPool.insert(nIndex);
        }
        LogPrintf("CWallet::NewKeyPool wrote %d new keys\n", nKeys);
    }
    return true;
}

bool CWallet::TopUpKeyPool(unsigned int kpSize)
{
    {
        LOCK(cs_wallet);
        if (IsLocked())
            return false;

        CWalletDB walletdb(strWalletFile);

        // Top up key pool
        unsigned int nTargetSize;
        if (kpSize > 0)
            nTargetSize = kpSize;
        else
/* MCHN START */            
        {
            if(mc_gState->m_NetworkParams->IsProtocolMultichain())
            {
                nTargetSize = max(GetArg("-keypool", 1), (int64_t) 0);
            }
            else
            {
                nTargetSize = max(GetArg("-keypool", 100), (int64_t) 0);
            }
        }
/* MCHN END */            

        while (setKeyPool.size() < (nTargetSize + 1))
        {
            int64_t nEnd = 1;
            if (!setKeyPool.empty())
                nEnd = *(--setKeyPool.end()) + 1;
            if (!walletdb.WritePool(nEnd, CKeyPool(GenerateNewKey())))
                throw runtime_error("TopUpKeyPool() : writing generated key failed");
            setKeyPool.insert(nEnd);
            LogPrintf("keypool added key %d, size=%u\n", nEnd, setKeyPool.size());
        }
    }
    return true;
}

void CWallet::ReserveKeyFromKeyPool(int64_t& nIndex, CKeyPool& keypool)
{
    nIndex = -1;
    keypool.vchPubKey = CPubKey();
    {
        LOCK(cs_wallet);

        if (!IsLocked())
            TopUpKeyPool();

        // Get the oldest key
        if(setKeyPool.empty())
            return;

        CWalletDB walletdb(strWalletFile);

        nIndex = *(setKeyPool.begin());
        setKeyPool.erase(setKeyPool.begin());
        if (!walletdb.ReadPool(nIndex, keypool))
            throw runtime_error("ReserveKeyFromKeyPool() : read failed");
        if (!HaveKey(keypool.vchPubKey.GetID()))
            throw runtime_error("ReserveKeyFromKeyPool() : unknown key in key pool");
        assert(keypool.vchPubKey.IsValid());
//        LogPrintf("keypool reserve %d\n", nIndex);
    }
}

void CWallet::KeepKey(int64_t nIndex)
{
    // Remove from key pool
    if (fFileBacked)
    {
        CWalletDB walletdb(strWalletFile);
        walletdb.ErasePool(nIndex);
    }
//    LogPrintf("keypool keep %d\n", nIndex);
}

void CWallet::ReturnKey(int64_t nIndex)
{
    // Return to key pool
    {
        LOCK(cs_wallet);
        setKeyPool.insert(nIndex);
    }
//    LogPrintf("keypool return %d\n", nIndex);
}

bool CWallet::GetKeyFromPool(CPubKey& result)
{
    int64_t nIndex = 0;
    CKeyPool keypool;
    {
        LOCK(cs_wallet);
        ReserveKeyFromKeyPool(nIndex, keypool);
        if (nIndex == -1)
        {
            if (IsLocked()) return false;
            result = GenerateNewKey();
            return true;
        }
        KeepKey(nIndex);
        result = keypool.vchPubKey;
    }
    return true;
}

/* MCHN START */
bool CWallet::GetKeyFromAddressBook(CPubKey& result,uint32_t type,const set<CTxDestination>* addresses,map<uint32_t, uint256>* mapSpecialEntity)
{
    if((mc_gState->m_NetworkParams->IsProtocolMultichain() == 0) || 
       (type == 0))
    {
        result=vchDefaultKey;
        return true;
    }
    
    CKeyID keyID;
    uint32_t perm;
            
    if((addresses == NULL) && (mapSpecialEntity == NULL))
    {
        keyID=vchDefaultKey.GetID();
        perm=mc_gState->m_Permissions->GetAllPermissions(NULL,(unsigned char*)(&keyID),type);
        if(perm == type)
        {
            result=vchDefaultKey;
            return true;        
        }
    }
    
    BOOST_FOREACH(const PAIRTYPE(CBitcoinAddress, CAddressBookData)& item, mapAddressBook)
    {
        const CBitcoinAddress& address = item.first;

        if((addresses == NULL) || (addresses->count(address.Get())))
        {
            if(address.GetKeyID(keyID))
            {
                perm=mc_gState->m_Permissions->GetAllPermissions(NULL,(unsigned char*)(&keyID),type);
                if(perm == type)
                {
                    bool take_it=true;
                    if(mapSpecialEntity)
                    {
                        if(type & MC_PTP_ISSUE)
                        {
                            unsigned char *lpEntity=NULL;

                            std::map<uint32_t,uint256>::const_iterator it = mapSpecialEntity->find(MC_PTP_ISSUE);
                            if (it != mapSpecialEntity->end())
                            {
                                lpEntity=(unsigned char*)(&(it->second));
                            }
                            take_it=false;
                            if(mc_gState->m_Permissions->CanIssue(lpEntity,(unsigned char*)(&keyID)))
                            {
                                take_it=true;
                            }                                                 
                        }
                        if(type & MC_PTP_WRITE)
                        {
                            unsigned char *lpEntity=NULL;

                            std::map<uint32_t,uint256>::const_iterator it = mapSpecialEntity->find(MC_PTP_WRITE);
                            if (it != mapSpecialEntity->end())
                            {
                                lpEntity=(unsigned char*)(&(it->second));
                            }
                            take_it=false;
                            if(mc_gState->m_Permissions->CanWrite(lpEntity,(unsigned char*)(&keyID)))
                            {
                                take_it=true;
                            }                                                 
                        }
                    }
                        
                    CKey key;
                    if(take_it)
                    {
                        if(GetKey(keyID, key))
                        {
                            result=key.GetPubKey();
                            return true;
                        }
                    }
                }
            }
        }
    }    
    return false;
}
/* MCHN END */

int64_t CWallet::GetOldestKeyPoolTime()
{
    int64_t nIndex = 0;
    CKeyPool keypool;
    ReserveKeyFromKeyPool(nIndex, keypool);
    if (nIndex == -1)
        return GetTime();
    ReturnKey(nIndex);
    return keypool.nTime;
}

std::map<CTxDestination, CAmount> CWallet::GetAddressBalances()
{
    map<CTxDestination, CAmount> balances;

    if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)
    {
        LOCK(cs_wallet);
        vector<COutput> vecOutputs;        
        AvailableCoins(vecOutputs, false, NULL, false,true);
        BOOST_FOREACH(const COutput& out, vecOutputs) 
        {
            if(out.coin.IsTrusted() && 
               out.coin.IsFinal() && 
               (out.coin.BlocksToMaturity() <=0) )
            {
                if(out.coin.m_EntityType)
                {
                    CTxOut txout;
                    out.GetHashAndTxOut(txout);

                    CTxDestination address;
                    if (ExtractDestination(txout.scriptPubKey, address))
                    {
                        if (!balances.count(address))
                            balances[address] = 0;
                        balances[address] += txout.nValue;
                    }                    
                }
            }
        }        
    }
    else
    {
        LOCK(cs_wallet);
        BOOST_FOREACH(PAIRTYPE(uint256, CWalletTx) walletEntry, mapWallet)
        {
            CWalletTx *pcoin = &walletEntry.second;

            if (!IsFinalTx(*pcoin) || !pcoin->IsTrusted())
                continue;

            if (pcoin->IsCoinBase() && pcoin->GetBlocksToMaturity() > 0)
                continue;

            int nDepth = pcoin->GetDepthInMainChain();
            if (nDepth < (pcoin->IsFromMe(ISMINE_ALL) ? 0 : 1))
                continue;

            for (unsigned int i = 0; i < pcoin->vout.size(); i++)
            {
                CTxDestination addr;
                if (!IsMine(pcoin->vout[i]))
                    continue;
                if(!ExtractDestination(pcoin->vout[i].scriptPubKey, addr))
                    continue;

                CAmount n = IsSpent(walletEntry.first, i) ? 0 : pcoin->vout[i].nValue;

                if (!balances.count(addr))
                    balances[addr] = 0;
                balances[addr] += n;
            }
        }
    }

    return balances;
}

set< set<CTxDestination> > CWallet::GetAddressGroupings()
{
    AssertLockHeld(cs_wallet); // mapWallet
    set< set<CTxDestination> > groupings;
    set<CTxDestination> grouping;
    set< set<CTxDestination> > ret;

    if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)
    {
        LOCK(cs_wallet);
        set<CTxDestination> addresses_with_outputs;
        vector<COutput> vecOutputs;        
        AvailableCoins(vecOutputs, false, NULL, false,true);
        BOOST_FOREACH(const COutput& out, vecOutputs) 
        {
            if(out.coin.IsTrusted() && 
               out.coin.IsFinal() && 
               (out.coin.BlocksToMaturity() <=0) )
            {
                if(out.coin.m_EntityType)
                {
                    CTxOut txout;
                    out.GetHashAndTxOut(txout);

                    CTxDestination address;
                    if (ExtractDestination(txout.scriptPubKey, address))
                    {
                        if (!addresses_with_outputs.count(address))
                            addresses_with_outputs.insert(address);
                    }                    
                }
            }            
        }
        BOOST_FOREACH(const CTxDestination& address, addresses_with_outputs) 
        {
            grouping.clear();
            grouping.insert(address);
            groupings.insert(grouping);
        }
        return groupings;
    }
    else
    {
        BOOST_FOREACH(PAIRTYPE(uint256, CWalletTx) walletEntry, mapWallet)
        {
            CWalletTx *pcoin = &walletEntry.second;

            if (pcoin->vin.size() > 0)
            {
                bool any_mine = false;
                // group all input addresses with each other
                BOOST_FOREACH(CTxIn txin, pcoin->vin)
                {
                    CTxDestination address;
                    if(!IsMine(txin)) /* If this input isn't mine, ignore it */
                        continue;
                    if(!ExtractDestination(mapWallet[txin.prevout.hash].vout[txin.prevout.n].scriptPubKey, address))
                        continue;
                    grouping.insert(address);
                    any_mine = true;
                }

                // group change with input addresses
                if (any_mine)
                {
                   BOOST_FOREACH(CTxOut txout, pcoin->vout)
                       if (IsChange(txout))
                       {
                           CTxDestination txoutAddr;
                           if(!ExtractDestination(txout.scriptPubKey, txoutAddr))
                               continue;
                           grouping.insert(txoutAddr);
                       }
                }
                if (grouping.size() > 0)
                {
                    groupings.insert(grouping);
                    grouping.clear();
                }
            }

            // group lone addrs by themselves
            for (unsigned int i = 0; i < pcoin->vout.size(); i++)
                if (IsMine(pcoin->vout[i]))
                {
                    CTxDestination address;
                    if(!ExtractDestination(pcoin->vout[i].scriptPubKey, address))
                        continue;
                    grouping.insert(address);
                    groupings.insert(grouping);
                    grouping.clear();
                }
        }

        set< set<CTxDestination>* > uniqueGroupings; // a set of pointers to groups of addresses
        map< CTxDestination, set<CTxDestination>* > setmap;  // map addresses to the unique group containing it
        BOOST_FOREACH(set<CTxDestination> grouping, groupings)
        {
            // make a set of all the groups hit by this new group
            set< set<CTxDestination>* > hits;
            map< CTxDestination, set<CTxDestination>* >::iterator it;
            BOOST_FOREACH(CTxDestination address, grouping)
                if ((it = setmap.find(address)) != setmap.end())
                    hits.insert((*it).second);

            // merge all hit groups into a new single group and delete old groups
            set<CTxDestination>* merged = new set<CTxDestination>(grouping);
            BOOST_FOREACH(set<CTxDestination>* hit, hits)
            {
                merged->insert(hit->begin(), hit->end());
                uniqueGroupings.erase(hit);
                delete hit;
            }
            uniqueGroupings.insert(merged);

            // update setmap
            BOOST_FOREACH(CTxDestination element, *merged)
                setmap[element] = merged;
        }

        BOOST_FOREACH(set<CTxDestination>* uniqueGrouping, uniqueGroupings)
        {
            ret.insert(*uniqueGrouping);
            delete uniqueGrouping;
        }
    }
    
    return ret;
}

set<CTxDestination> CWallet::GetAccountAddresses(string strAccount) const
{
    LOCK(cs_wallet);
    set<CTxDestination> result;
    BOOST_FOREACH(const PAIRTYPE(CTxDestination, CAddressBookData)& item, mapAddressBook)
    {
        const CTxDestination& address = item.first;
        const string& strName = item.second.name;
        if(item.second.purpose != "license")
        {        
            if (strName == strAccount)
                result.insert(address);
        }
    }
    return result;
}

bool CReserveKey::GetReservedKey(CPubKey& pubkey)
{
    if (nIndex == -1)
    {
        CKeyPool keypool;
        pwallet->ReserveKeyFromKeyPool(nIndex, keypool);
        if (nIndex != -1)
            vchPubKey = keypool.vchPubKey;
        else {
            return false;
        }
    }
    assert(vchPubKey.IsValid());
    pubkey = vchPubKey;
    return true;
}

void CReserveKey::KeepKey()
{
    if (nIndex != -1)
        pwallet->KeepKey(nIndex);
    nIndex = -1;
    vchPubKey = CPubKey();
}

void CReserveKey::ReturnKey()
{
    if (nIndex != -1)
        pwallet->ReturnKey(nIndex);
    nIndex = -1;
    vchPubKey = CPubKey();
}

void CWallet::GetAllReserveKeys(set<CKeyID>& setAddress) const
{
    setAddress.clear();

    CWalletDB walletdb(strWalletFile);

    LOCK2(cs_main, cs_wallet);
    BOOST_FOREACH(const int64_t& id, setKeyPool)
    {
        CKeyPool keypool;
        if (!walletdb.ReadPool(id, keypool))
            throw runtime_error("GetAllReserveKeyHashes() : read failed");
        assert(keypool.vchPubKey.IsValid());
        CKeyID keyID = keypool.vchPubKey.GetID();
        if (!HaveKey(keyID))
            throw runtime_error("GetAllReserveKeyHashes() : unknown key in key pool");
        setAddress.insert(keyID);
    }
}

void CWallet::UpdatedTransaction(const uint256 &hashTx)
{
//    if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)                          // Not supported, used only in QT
    {
        LOCK(cs_wallet);
        // Only notify UI if this transaction is in this wallet
        map<uint256, CWalletTx>::const_iterator mi = mapWallet.find(hashTx);
        if (mi != mapWallet.end())
            NotifyTransactionChanged(this, hashTx, CT_UPDATED);
    }
}

void CWallet::LockCoin(COutPoint& output)
{
    AssertLockHeld(cs_wallet); // setLockedCoins
    setLockedCoins.insert(output);
}

void CWallet::UnlockCoin(COutPoint& output)
{
    AssertLockHeld(cs_wallet); // setLockedCoins
    setLockedCoins.erase(output);
}

void CWallet::UnlockAllCoins()
{
    AssertLockHeld(cs_wallet); // setLockedCoins
    setLockedCoins.clear();
}

bool CWallet::IsLockedCoin(uint256 hash, unsigned int n) const
{
    AssertLockHeld(cs_wallet); // setLockedCoins
    COutPoint outpt(hash, n);

    return (setLockedCoins.count(outpt) > 0);
}

void CWallet::ListLockedCoins(std::vector<COutPoint>& vOutpts)
{
    AssertLockHeld(cs_wallet); // setLockedCoins
    for (std::set<COutPoint>::iterator it = setLockedCoins.begin();
         it != setLockedCoins.end(); it++) {
        COutPoint outpt = (*it);
        vOutpts.push_back(outpt);
    }
}

/** @} */ // end of Actions

class CAffectedKeysVisitor : public boost::static_visitor<void> {
private:
    const CKeyStore &keystore;
    std::vector<CKeyID> &vKeys;

public:
    CAffectedKeysVisitor(const CKeyStore &keystoreIn, std::vector<CKeyID> &vKeysIn) : keystore(keystoreIn), vKeys(vKeysIn) {}

    void Process(const CScript &script) {
        txnouttype type;
        std::vector<CTxDestination> vDest;
        int nRequired;
        if (ExtractDestinations(script, type, vDest, nRequired)) {
            BOOST_FOREACH(const CTxDestination &dest, vDest)
                boost::apply_visitor(*this, dest);
        }
    }

    void operator()(const CKeyID &keyId) {
        if (keystore.HaveKey(keyId))
            vKeys.push_back(keyId);
    }

    void operator()(const CScriptID &scriptId) {
        CScript script;
        if (keystore.GetCScript(scriptId, script))
            Process(script);
    }

    void operator()(const CNoDestination &none) {}
};

void CWallet::GetKeyBirthTimes(std::map<CKeyID, int64_t> &mapKeyBirth) const {
    AssertLockHeld(cs_wallet); // mapKeyMetadata
    mapKeyBirth.clear();

    // get birth times for keys with metadata

    for (std::map<CKeyID, CKeyMetadata>::const_iterator it = mapKeyMetadata.begin(); it != mapKeyMetadata.end(); it++)
        if (it->second.nCreateTime)
            mapKeyBirth[it->first] = it->second.nCreateTime;

    // map in which we'll infer heights of other keys
    CBlockIndex *pindexMax = chainActive[std::max(0, chainActive.Height() - 144)]; // the tip can be reorganised; use a 144-block safety margin
    std::map<CKeyID, CBlockIndex*> mapKeyFirstBlock;
    std::set<CKeyID> setKeys;
    GetKeys(setKeys);
    BOOST_FOREACH(const CKeyID &keyid, setKeys) {
        if (mapKeyBirth.count(keyid) == 0)
            mapKeyFirstBlock[keyid] = pindexMax;
    }
    setKeys.clear();

    // if there are no such keys, we're done
    if (mapKeyFirstBlock.empty())
        return;

    // find first block that affects those keys, if there are any left
/* MCHN START */    
    if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)
    {
        mc_Buffer *entity_rows;
        entity_rows=new mc_Buffer;
        entity_rows->Initialize(MC_TDB_ENTITY_KEY_SIZE,MC_TDB_ROW_SIZE,MC_BUF_MODE_DEFAULT);
        
        mc_TxEntity entity;
        mc_TxEntityRow *lpEntTx;
        int first_block;
        int tx_count;
        
        entity.Zero();
        for (std::map<CKeyID, CBlockIndex*>::const_iterator it = mapKeyFirstBlock.begin(); it != mapKeyFirstBlock.end(); it++)
        {
            memcpy(entity.m_EntityID,&(it->first),MC_TDB_ENTITY_ID_SIZE);
            entity.m_EntityType=MC_TET_PUBKEY_ADDRESS | MC_TET_CHAINPOS;
            
            first_block=chainActive.Height();
            tx_count=pwalletTxsMain->GetListSize(&entity,NULL);
            if(tx_count)
            {
                pwalletTxsMain->GetList(&entity,1,1,entity_rows);
                lpEntTx=(mc_TxEntityRow*)entity_rows->GetRow(0);
                first_block=lpEntTx->m_Block;
                if( (first_block < 0) || (first_block > chainActive.Height()) )
                {
                    first_block=chainActive.Height();                    
                }
            }
            CBlockIndex* pindex=chainActive[first_block];
            mapKeyBirth[it->first] = pindex->GetBlockTime() - 12 * Params().TargetSpacing(); // block times can be 2h off            
        }

        delete entity_rows;
    }
    else
    {
/* MCHN END */    
        std::vector<CKeyID> vAffected;
        for (std::map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); it++) {
            // iterate over all wallet transactions...
            const CWalletTx &wtx = (*it).second;
            BlockMap::const_iterator blit = mapBlockIndex.find(wtx.hashBlock);
            if (blit != mapBlockIndex.end() && chainActive.Contains(blit->second)) {
                // ... which are already in a block
                int nHeight = blit->second->nHeight;
                BOOST_FOREACH(const CTxOut &txout, wtx.vout) {
                    // iterate over all their outputs
                    CAffectedKeysVisitor(*this, vAffected).Process(txout.scriptPubKey);
                    BOOST_FOREACH(const CKeyID &keyid, vAffected) {
                        // ... and all their affected keys
                        std::map<CKeyID, CBlockIndex*>::iterator rit = mapKeyFirstBlock.find(keyid);
                        if (rit != mapKeyFirstBlock.end() && nHeight < rit->second->nHeight)
                            rit->second = blit->second;
                    }
                    vAffected.clear();
                }
            }
        }
        // Extract block timestamps for those keys
        for (std::map<CKeyID, CBlockIndex*>::const_iterator it = mapKeyFirstBlock.begin(); it != mapKeyFirstBlock.end(); it++)
            mapKeyBirth[it->first] = it->second->GetBlockTime() - 12 * Params().TargetSpacing(); // MCHN block times can be 2h off
    }

}

bool CWallet::AddDestData(const CTxDestination &dest, const std::string &key, const std::string &value)
{
    if (boost::get<CNoDestination>(&dest))
        return false;

    mapAddressBook[dest].destdata.insert(std::make_pair(key, value));
    if (!fFileBacked)
        return true;
    return CWalletDB(strWalletFile).WriteDestData(CBitcoinAddress(dest).ToString(), key, value);
}

bool CWallet::EraseDestData(const CTxDestination &dest, const std::string &key)
{
    if (!mapAddressBook[dest].destdata.erase(key))
        return false;
    if (!fFileBacked)
        return true;
    return CWalletDB(strWalletFile).EraseDestData(CBitcoinAddress(dest).ToString(), key);
}

bool CWallet::LoadDestData(const CTxDestination &dest, const std::string &key, const std::string &value)
{
    mapAddressBook[dest].destdata.insert(std::make_pair(key, value));
    return true;
}

bool CWallet::GetDestData(const CTxDestination &dest, const std::string &key, std::string *value) const
{
    std::map<CTxDestination, CAddressBookData>::const_iterator i = mapAddressBook.find(dest);
    if(i != mapAddressBook.end())
    {
        CAddressBookData::StringMap::const_iterator j = i->second.destdata.find(key);
        if(j != i->second.destdata.end())
        {
            if(value)
                *value = j->second;
            return true;
        }
    }
    return false;
}

CKeyPool::CKeyPool()
{
    nTime = GetTime();
}

CKeyPool::CKeyPool(const CPubKey& vchPubKeyIn)
{
    nTime = GetTime();
    vchPubKey = vchPubKeyIn;
}

CWalletKey::CWalletKey(int64_t nExpires)
{
    nTimeCreated = (nExpires ? GetTime() : 0);
    nTimeExpires = nExpires;
}

int CMerkleTx::SetMerkleBranch(const CBlock& block)
{
    AssertLockHeld(cs_main);
    CBlock blockTmp;

    // Update the tx's hashBlock
    hashBlock = block.GetHash();

    // Locate the transaction
    for (nIndex = 0; nIndex < (int)block.vtx.size(); nIndex++)
        if (block.vtx[nIndex] == *(CTransaction*)this)
            break;
    if (nIndex == (int)block.vtx.size())
    {
        vMerkleBranch.clear();
        nIndex = -1;
        LogPrintf("ERROR: SetMerkleBranch() : couldn't find tx in block\n");
        return 0;
    }
    // Fill in merkle branch
    vMerkleBranch = block.GetMerkleBranch(nIndex);
 
    // Is the tx in a block that's in the main chain
    BlockMap::iterator mi = mapBlockIndex.find(hashBlock);
    if (mi == mapBlockIndex.end())
        return 0;
    const CBlockIndex* pindex = (*mi).second;
    if (!pindex || !chainActive.Contains(pindex))
        return 0;

    return chainActive.Height() - pindex->nHeight + 1;
}

int CMerkleTx::GetDepthInMainChainINTERNAL(const CBlockIndex* &pindexRet) const
{
    if((mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS) == 0)
    {
        if (hashBlock == 0 || nIndex == -1)
            return 0;
    }
    AssertLockHeld(cs_main);

    CBlockIndex* pindex = NULL;
    if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)
    {
        mc_TxDefRow txdef;
        if(pwalletTxsMain->FindWalletTx(GetHash(),&txdef) == MC_ERR_NOERROR)
        {
            if((txdef.m_Flags & MC_TFL_INVALID) == 0)
            {
                if(txdef.m_Block >= 0)
                {
                    if(txdef.m_Block <= chainActive.Height())
                    {
                        pindex=chainActive[txdef.m_Block];
                    }
                }        
            }
        }
        if (!pindex || !chainActive.Contains(pindex))
            return 0;
    }
    else
    {
        // Find the block it claims to be in
        BlockMap::iterator mi = mapBlockIndex.find(hashBlock);
        if (mi == mapBlockIndex.end())
            return 0;
//        CBlockIndex* pindex = (*mi).second;
        pindex = (*mi).second;
        if (!pindex || !chainActive.Contains(pindex))
            return 0;

        // Make sure the merkle branch connects to this block
        if (!fMerkleVerified)
        {
            if (CBlock::CheckMerkleBranch(GetHash(), vMerkleBranch, nIndex) != pindex->hashMerkleRoot)
                return 0;
            fMerkleVerified = true;
        }
    }
    pindexRet = pindex;
    return chainActive.Height() - pindex->nHeight + 1;
}

int CMerkleTx::GetDepthInMainChain(const CBlockIndex* &pindexRet) const
{
    AssertLockHeld(cs_main);
    int nResult = GetDepthInMainChainINTERNAL(pindexRet);
    if (nResult == 0 && !mempool.exists(GetHash()))
        return -1; // Not in chain, not in mempool

    return nResult;
}

int CMerkleTx::GetBlocksToMaturity() const
{
    if (!IsCoinBase())
        return 0;
    return max(0, (COINBASE_MATURITY+1) - GetDepthInMainChain());
}


bool CMerkleTx::AcceptToMemoryPool(bool fLimitFree, bool fRejectInsaneFee)
{
    CValidationState state;
    return ::AcceptToMemoryPool(mempool, state, *this, fLimitFree, NULL, fRejectInsaneFee);
}

/* MCHN START */
bool CWalletTx::AcceptToMemoryPoolReturnReason(bool fLimitFree, bool fRejectInsaneFee,string& reject_reason)
{
    CValidationState state;
    
    if(pMultiChainFilterEngine)
    {
        pMultiChainFilterEngine->SetTimeout(pMultiChainFilterEngine->GetSendTimeout());
    }

    bool result=::AcceptToMemoryPool(mempool, state, *this, fLimitFree, NULL, fRejectInsaneFee,this);

    if(pMultiChainFilterEngine)
    {
        pMultiChainFilterEngine->SetTimeout(pMultiChainFilterEngine->GetAcceptTimeout());
    }

    if(!result)
    {
        if(state.IsInvalid())
            reject_reason = strprintf("%i: %s", state.GetRejectCode(), state.GetRejectReason());
        else
            reject_reason = state.GetRejectReason();
    }
    
    return result;
}
/* MCHN END */

