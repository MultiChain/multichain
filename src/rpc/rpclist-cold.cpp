// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2014-2016 The Bitcoin Core developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2017 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#if defined(HAVE_CONFIG_H)
#include "config/bitcoin-config.h"
#endif

#include "rpc/rpcserver.h"


/* MCHN START */
static const CRPCCommand vRPCWalletReadCommands[] =
{ //  category              name                      actor (function)         okSafeMode threadSafe reqWallet
  //  --------------------- ------------------------  -----------------------  ---------- ---------- ---------
};

/* MCHN END */

/**
 * Call Table
 */
static const CRPCCommand vRPCCommands[] =
{ //  category              name                      actor (function)         okSafeMode threadSafe reqWallet
  //  --------------------- ------------------------  -----------------------  ---------- ---------- ---------
    /* Overall control/query calls */
    { "control",            "getinfo",                &getinfo,                true,      false,      false }, /* uses wallet if enabled */
    { "control",            "help",                   &help,                   true,      true,       false },
    { "control",            "stop",                   &stop,                   true,      true,       false },
    { "control",            "getblockchainparams",    &getblockchainparams,    true,      false,      false }, 

    /* Raw transactions */
    { "rawtransactions",    "decodescript",           &decodescript,           true,      false,      false },
    { "rawtransactions",    "signrawtransaction",     &signrawtransaction,     false,     false,      false }, /* uses wallet if enabled */

    /* Utility functions */
    { "util",               "createkeypairs",         &createkeypairs,         true,      true ,      false },
    { "util",               "createmultisig",         &createmultisig,         true,      true ,      false },
    { "util",               "validateaddress",        &validateaddress,        true,      false,      false }, /* uses wallet if enabled */
    { "util",               "verifymessage",          &verifymessage,          true,      false,      false },

#ifdef ENABLE_WALLET
    /* Wallet */
    { "wallet",             "addmultisigaddress",     &addmultisigaddress,     true,      false,      true },
    { "wallet",             "backupwallet",           &backupwallet,           true,      false,      true },
    { "wallet",             "dumpprivkey",            &dumpprivkey,            true,      false,      true },
    { "wallet",             "dumpwallet",             &dumpwallet,             true,      false,      true },
    { "wallet",             "encryptwallet",          &encryptwallet,          true,      false,      true },
    { "wallet",             "importprivkey",          &importprivkey,          true,      false,      true },
    { "wallet",             "importwallet",           &importwallet,           true,      false,      true },
    { "wallet",             "importaddress",          &importaddress,          true,      false,      true },
    { "wallet",             "getaddresses",           &getaddresses,           true,      false,      true },
    { "wallet",             "getnewaddress",          &getnewaddress,          true,      false,      true },
    
    { "wallet",             "listaddresses",          &listaddresses,          false,     false,      true },
    
    { "wallet",             "signmessage",            &signmessage,            true,      false,      true },
    { "wallet",             "walletlock",             &walletlock,             true,      false,      true },
    { "wallet",             "walletpassphrasechange", &walletpassphrasechange, true,      false,      true },
    { "wallet",             "walletpassphrase",       &walletpassphrase,       true,      false,      true },
#endif // ENABLE_WALLET
};

void mc_InitRPCList(std::vector<CRPCCommand>& vStaticRPCCommands,std::vector<CRPCCommand>& vStaticRPCWalletReadCommands)
{
    unsigned int vcidx;
    vStaticRPCCommands.clear();
    vStaticRPCWalletReadCommands.clear();
    for (vcidx = 0; vcidx < (sizeof(vRPCCommands) / sizeof(vRPCCommands[0])); vcidx++)
    {
        vStaticRPCCommands.push_back(vRPCCommands[vcidx]);
    }    
/*    
    for (vcidx = 0; vcidx < (sizeof(vRPCWalletReadCommands) / sizeof(vRPCWalletReadCommands[0])); vcidx++)
    {
        vStaticRPCWalletReadCommands.push_back(vRPCCommands[vcidx]);
    }    
 */ 
}

