// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2014-2016 The Bitcoin Core developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#if defined(HAVE_CONFIG_H)
#include "config/bitcoin-config.h"
#endif


#include "rpc/rpcserver.h"


/* MCHN START */
static const CRPCCommand vRPCWalletReadCommands[] =
{ //  category              name                      actor (function)         okSafeMode threadSafe reqWallet
  //  --------------------- ------------------------  -----------------------  ---------- ---------- ---------
    { "wallet",             "getbalance",             &getbalance,             false,     false,      true },
    { "wallet",             "getreceivedbyaccount",   &getreceivedbyaccount,   false,     false,      true },
    { "wallet",             "getreceivedbyaddress",   &getreceivedbyaddress,   false,     false,      true },
    { "wallet",             "gettransaction",         &gettransaction,         false,     false,      true },
    { "wallet",             "getunconfirmedbalance",  &getunconfirmedbalance,  false,     false,      true },
    { "wallet",             "getwalletinfo",          &getwalletinfo,          false,     false,      true },
    { "wallet",             "listlockunspent",        &listlockunspent,        false,     false,      true },
    { "wallet",             "listreceivedbyaccount",  &listreceivedbyaccount,  false,     false,      true },
    { "wallet",             "listreceivedbyaddress",  &listreceivedbyaddress,  false,     false,      true },
    { "wallet",             "listsinceblock",         &listsinceblock,         false,     false,      true },
    { "wallet",             "listtransactions",       &listtransactions,       false,     false,      true },
    { "wallet",             "listunspent",            &listunspent,            false,     false,      true },
    { "wallet",             "getassetbalances",       &getassetbalances,       false,     false,      true },
    { "wallet",             "gettotalbalances",       &gettotalbalances,       false,     false,      true },
    
    { "wallet",             "getmultibalances",       &getmultibalances,       false,     false,      true },
    { "wallet",             "gettokenbalances",       &gettokenbalances,       false,     false,      true },
    { "wallet",             "getaddressbalances",     &getaddressbalances,     false,     false,      true },
    
    { "wallet",             "listwallettransactions", &listwallettransactions, false,     false,      true },
    { "wallet",             "listaddresstransactions",&listaddresstransactions,false,     false,      true },
    { "wallet",             "getwallettransaction",   &getwallettransaction,   false,     false,      true },
    { "wallet",             "getaddresstransaction",  &getaddresstransaction,  false,     false,      true },
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
    { "control",            "getchaintotals",         &getchaintotals,        true,      true,       false },
    { "control",            "getinitstatus",          &getinitstatus,          true,      true,       false },
    { "control",            "gethealthcheck",         &gethealthcheck,         true,      true,       false },
    { "control",            "help",                   &help,                   true,      true,       false },
    { "control",            "stop",                   &stop,                   true,      true,       false },
/* MCHN START */    
    { "control",            "pause",                  &pausecmd,               true,      false,      false },
    { "control",            "resume",                 &resumecmd,              true,      false,      false },
    { "control",            "clearmempool",           &clearmempool,           true,      false,      false },
    { "control",            "setlastblock",           &setlastblock,           true,      false,      false },
    { "control",            "getblockchainparams",    &getblockchainparams,    true,      false,      false }, 
    { "control",            "getruntimeparams",       &getruntimeparams,       true,      false,      false }, 
    { "control",            "setruntimeparam",        &setruntimeparam,        true,      false,      false }, 
    { "control",            "getdiagnostics",         &getdiagnostics,         true,      false,      false }, 
    { "control",            "applycommands",          &applycommands,          true,      false,      false }, 
/* MCHN END */    

    /* P2P networking */
    { "network",            "getnetworkinfo",         &getnetworkinfo,         true,      false,      false },
    { "network",            "addnode",                &addnode,                true,      true,       false },
    { "network",            "storenode",              &storenode,              true,      true,       false },
    { "network",            "getaddednodeinfo",       &getaddednodeinfo,       true,      true,       false },
    { "network",            "getconnectioncount",     &getconnectioncount,     true,      false,      false },
    { "network",            "getnettotals",           &getnettotals,           true,      true,       false },
    { "network",            "getpeerinfo",            &getpeerinfo,            true,      false,      false },
    { "network",            "liststorednodes",        &liststorednodes,        true,      false,      false },
    { "network",            "ping",                   &ping,                   true,      false,      false },
    { "network",            "getchunkqueueinfo",      &getchunkqueueinfo,      true,      true,       false },
    { "network",            "getchunkqueuetotals",    &getchunkqueuetotals,    true,      true,       false },

    /* Block chain and UTXO */
    { "blockchain",         "getblockchaininfo",      &getblockchaininfo,      true,      false,      false },
    { "blockchain",         "getbestblockhash",       &getbestblockhash,       true,      false,      false },
    { "blockchain",         "getblockcount",          &getblockcount,          true,       true,      false },
    { "blockchain",         "getlastblockinfo",       &getlastblockinfo,       true,      false,      false },
    { "blockchain",         "getblock",               &getblock,               true,      false,      false },
    { "blockchain",         "getblockhash",           &getblockhash,           true,      false,      false },
    { "blockchain",         "getchaintips",           &getchaintips,           true,      false,      false },
    { "blockchain",         "getdifficulty",          &getdifficulty,          true,      false,      false },
    { "blockchain",         "getmempoolinfo",         &getmempoolinfo,         true,      true,       false },
    { "blockchain",         "getrawmempool",          &getrawmempool,          true,      false,      false },
    { "blockchain",         "gettxout",               &gettxout,               true,      false,      false },
    { "blockchain",         "gettxoutsetinfo",        &gettxoutsetinfo,        true,      false,      false },
    { "blockchain",         "verifychain",            &verifychain,            true,      false,      false },
    { "blockchain",         "invalidateblock",        &invalidateblock,        true,      true,       false },
    { "blockchain",         "reconsiderblock",        &reconsiderblock,        true,      true,       false },

/* MCHN START */    
    { "blockchain",         "listassets",             &listassets,             true,      false,      false },
    { "blockchain",         "listpermissions",        &listpermissions,        true,      false,      false },
    { "blockchain",         "listminers",             &listminers,             true,      false,      false },
    { "blockchain",         "liststreams",            &liststreams,            true,      false,      false },
    { "blockchain",         "listupgrades",           &listupgrades,           true,      false,      false },
    { "blockchain",         "listtxfilters",          &listtxfilters,          true,      false,      false },
    { "blockchain",         "liststreamfilters",      &liststreamfilters,      true,      false,      false },
    { "blockchain",         "getfiltercode",          &getfiltercode,          true,      false,      false },
    { "blockchain",         "testtxfilter",           &testtxfilter,           true,      false,      false },
    { "blockchain",         "runtxfilter",            &runtxfilter,            true,      false,      false },
    { "blockchain",         "teststreamfilter",       &teststreamfilter,       true,      false,      false },
    { "blockchain",         "runstreamfilter",        &runstreamfilter,        true,      false,      false },
    { "blockchain",         "listblocks",             &listblocks,             true,      false,      false },
    { "blockchain",         "getassetinfo",           &getassetinfo,           true,      false,      false },
    { "blockchain",         "getstreaminfo",          &getstreaminfo,          true,      false,      false },
    { "blockchain",         "verifypermission",       &verifypermission,       true,      false,      false },

    { "blockchain",         "listvariables",          &listvariables,          true,      false,      false },
    { "blockchain",         "getvariableinfo",        &getvariableinfo,        true,      false,      false },
    { "blockchain",         "setvariablevalue",       &setvariablevalue,       true,      false,      false },
    { "blockchain",         "setvariablevaluefrom",   &setvariablevaluefrom,   true,      false,      false },
    { "blockchain",         "getvariablevalue",       &getvariablevalue,       true,      false,      false },
    { "blockchain",         "getvariablehistory",     &getvariablehistory,     true,      false,      false },
    { "blockchain",         "listassetissues",        &listassetissues,        true,      false,      false },
    { "blockchain",         "gettokeninfo",           &gettokeninfo,        true,      false,      false },

    { "blockchain",         "listlibraries",          &listlibraries,          true,      false,      false },
    { "blockchain",         "addlibraryupdate",       &addlibraryupdate,       true,      false,      false },
    { "blockchain",         "addlibraryupdatefrom",   &addlibraryupdatefrom,   true,      false,      false },
    { "blockchain",         "getlibrarycode",         &getlibrarycode,         true,      false,      false },
    { "blockchain",         "testlibrary",            &testlibrary,            true,      false,      false },
    
/* MCHN END */    
    
    /* Mining */
    { "mining",             "getblocktemplate",       &getblocktemplate,       true,      false,      false },
    { "mining",             "getmininginfo",          &getmininginfo,          true,      false,      false },
    { "mining",             "getnetworkhashps",       &getnetworkhashps,       true,      false,      false },
    { "mining",             "prioritisetransaction",  &prioritisetransaction,  true,      false,      false },
    { "mining",             "submitblock",            &submitblock,            true,      true,       false },

#ifdef ENABLE_WALLET
    /* Coin generation */
    { "generating",         "getgenerate",            &getgenerate,            true,      false,      false },
    { "generating",         "gethashespersec",        &gethashespersec,        true,      false,      false },
    { "generating",         "setgenerate",            &setgenerate,            true,      true,       false },
#endif

    /* Raw transactions */
    { "rawtransactions",    "appendrawtransaction",   &appendrawtransaction,   true,      false,      false },
    { "rawtransactions",    "createrawtransaction",   &createrawtransaction,   true,      false,      false },
    { "rawtransactions",    "decoderawtransaction",   &decoderawtransaction,   true,      false,      false },
    { "rawtransactions",    "decodescript",           &decodescript,           true,      false,      false },
    { "rawtransactions",    "getrawtransaction",      &getrawtransaction,      true,      false,      false },
    { "rawtransactions",    "sendrawtransaction",     &sendrawtransaction,     false,     false,      false },
    { "rawtransactions",    "signrawtransaction",     &signrawtransaction,     false,     false,      false }, /* uses wallet if enabled */
/* MCHN START */    
    { "rawtransactions",    "appendrawchange",        &appendrawchange,        false,     false,      false },
    { "hidden",             "appendrawmetadata",      &appendrawmetadata,      false,     false,      false },
    { "rawtransactions",    "appendrawdata",          &appendrawmetadata,      false,     false,      false },
    { "hidden",             "debug",                  &debug,                  false,     true,       false },
/* MCHN END */    

    /* Utility functions */
    { "util",               "createkeypairs",         &createkeypairs,         true,      true ,      false },
    { "util",               "createmultisig",         &createmultisig,         true,      true ,      false },
    { "util",               "validateaddress",        &validateaddress,        true,      false,      false }, /* uses wallet if enabled */
    { "util",               "verifymessage",          &verifymessage,          true,      false,      false },
    { "util",               "estimatefee",            &estimatefee,            true,      true,       false },
    { "util",               "estimatepriority",       &estimatepriority,       true,      true,       false },
    
    { "util",               "createbinarycache",      &createbinarycache,      true,      true,      false },
    { "util",               "appendbinarycache",      &appendbinarycache,      true,      true,      false },
    { "util",               "deletebinarycache",      &deletebinarycache,      true,      true,      false },
    { "util",               "decodehexubjson",        &decodehexubjson,        true,      true,      false },
    { "util",               "encodehexubjson",        &encodehexubjson,        true,      true,      false },

    /* Not shown in help */
    { "hidden",             "invalidateblock",        &invalidateblock,        true,      true,       false },
    { "hidden",             "reconsiderblock",        &reconsiderblock,        true,      true,       false },
    { "hidden",             "setmocktime",            &setmocktime,            true,      false,      false },
    { "hidden",             "data-all",               &purehelpitem_nomethod,  true,      true,       false },
    { "hidden",             "data-with",              &purehelpitem_nomethod,  true,      true,       false },
    { "hidden",             "addresses-all",          &purehelpitem_nomethod,  true,      true,       false },
    { "hidden",             "amounts-all",            &purehelpitem_nomethod,  true,      true,       false },
    { "hidden",             "feed-options",           &purehelpitem_nomethod,  true,      true,       false },
    
    { "hidden",             "getfilterstreamitem",    &purehelpitem_onlyfilter,true,      false,      false },
    { "hidden",             "getfilterstream",        &purehelpitem_onlyfilter,true,      false,      false },
    { "hidden",             "getfiltertransaction",   &purehelpitem_onlyfilter,true,      false,      false },
    { "hidden",             "getfilterassetbalances", &purehelpitem_onlyfilter,true,      false,      false },
    { "hidden",             "getfiltertokenbalances", &purehelpitem_onlyfilter,true,      false,      false },
    { "hidden",             "getfiltertxid",          &purehelpitem_onlyfilter,true,      true,       false },
    { "hidden",             "getfiltertxinput",       &purehelpitem_onlyfilter,true,      true,       false },
    { "hidden",             "setfilterparam",         &purehelpitem_onlyfilter,true,      true,       false },
    { "hidden",             "filters",                &purehelpitem_nomethod,  true,      true,       false },

#ifdef ENABLE_WALLET
    /* Licensing */
    { "hidden",             "getlicenserequest",      &getlicenserequest,       true,      false,      true },
    { "hidden",             "decodelicenserequest",   &decodelicenserequest,    true,      false,      true },
    { "hidden",             "decodelicenseconfirmation",   &decodelicenseconfirmation,    true,      false,      true },
    { "hidden",             "activatelicense",        &activatelicense,         true,      false,      true },
    { "hidden",             "activatelicensefrom",    &activatelicensefrom,     true,      false,      true },
    { "hidden",             "transferlicense",        &transferlicense,         true,      false,      true },
    { "hidden",             "takelicense",            &takelicense,             true,      false,      true },
    { "hidden",             "listlicenses",           &listlicenses,            true,      false,      true },
    { "hidden",             "getlicenseconfirmation", &getlicenseconfirmation,  true,      false,      true },
    { "hidden",             "importlicenserequest",   &importlicenserequest,       true,      false,      true },
    
    { "feeds",              "createfeed",             &createfeed,              true,      false,      false },
    { "feeds",              "deletefeed",             &deletefeed,              true,      true,       false },
    { "feeds",              "pausefeed",              &pausefeed,               true,      false,      false },
    { "feeds",              "resumefeed",             &resumefeed,              true,      false,      false },
    { "feeds",              "addtofeed",              &addtofeed,               true,      false,      false },
    { "feeds",              "updatefeed",             &updatefeed,              true,      false,      false },
    { "feeds",              "purgefeed",              &purgefeed,               true,      true,       false },
    { "feeds",              "listfeeds",              &listfeeds,               true,      false,      false },
    { "feeds",              "getdatarefdata",         &getdatarefdata,          true,      true,      false },
    { "feeds",              "datareftobinarycache",   &datareftobinarycache,    true,      true,      false },

    /* Wallet */
    { "wallet",             "addmultisigaddress",     &addmultisigaddress,     true,      false,      true },
    { "wallet",             "backupwallet",           &backupwallet,           true,      false,      true },
    { "wallet",             "dumpprivkey",            &dumpprivkey,            true,      false,      true },
    { "wallet",             "dumpwallet",             &dumpwallet,             true,      false,      true },
    { "wallet",             "encryptwallet",          &encryptwallet,          true,      false,      true },
    { "wallet",             "getaccountaddress",      &getaccountaddress,      true,      false,      true },
    { "wallet",             "getaccount",             &getaccount,             true,      false,      true },
    { "wallet",             "getaddressesbyaccount",  &getaddressesbyaccount,  true,      false,      true },
    { "wallet",             "getbalance",             &getbalance,             false,     false,      true },
    { "wallet",             "getnewaddress",          &getnewaddress,          true,      false,      true },
    { "wallet",             "getrawchangeaddress",    &getrawchangeaddress,    true,      false,      true },
    { "wallet",             "getreceivedbyaccount",   &getreceivedbyaccount,   false,     false,      true },
    { "wallet",             "getreceivedbyaddress",   &getreceivedbyaddress,   false,     false,      true },
    { "wallet",             "gettransaction",         &gettransaction,         false,     false,      true },
    { "wallet",             "getunconfirmedbalance",  &getunconfirmedbalance,  false,     false,      true },
    { "wallet",             "getwalletinfo",          &getwalletinfo,          false,     false,      true },
    { "wallet",             "importprivkey",          &importprivkey,          true,      false,      true },
    { "wallet",             "importwallet",           &importwallet,           true,      false,      true },
    { "wallet",             "importaddress",          &importaddress,          true,      false,      true },
    { "wallet",             "keypoolrefill",          &keypoolrefill,          true,      false,      true },
    { "wallet",             "listaccounts",           &listaccounts,           false,     false,      true },
    { "wallet",             "listaddressgroupings",   &listaddressgroupings,   false,     false,      true },
    { "wallet",             "listlockunspent",        &listlockunspent,        false,     false,      true },
    { "wallet",             "listreceivedbyaccount",  &listreceivedbyaccount,  false,     false,      true },
    { "wallet",             "listreceivedbyaddress",  &listreceivedbyaddress,  false,     false,      true },
    { "wallet",             "listsinceblock",         &listsinceblock,         false,     false,      true },
    { "wallet",             "listtransactions",       &listtransactions,       false,     false,      true },
    { "wallet",             "listunspent",            &listunspent,            false,     false,      true },
    { "wallet",             "lockunspent",            &lockunspent,            true,      false,      true },
    { "wallet",             "move",                   &movecmd,                false,     false,      true },
//    { "wallet",             "sendfrom",               &sendfrom,               false,     false,      true },
    { "wallet",             "sendfromaccount",        &sendfrom,               false,     false,      true },
    { "wallet",             "sendmany",               &sendmany,               false,     false,      true },
    { "hidden",             "sendtoaddress",          &sendtoaddress,          false,     false,      true },
    { "wallet",             "send",                   &sendtoaddress,          false,     false,      true },
/* MCHN START */    
    { "wallet",             "getaddresses",           &getaddresses,           true,      false,      true },
    { "wallet",             "combineunspent",         &combineunspent,         false,     false,      true }, 
    { "wallet",             "grant",                  &grantcmd,               false,     false,      true }, 
    { "wallet",             "revoke",                 &revokecmd,              false,     false,      true },
    { "wallet",             "issue",                  &issuecmd,               false,     false,      true },
    { "wallet",             "issuemore",              &issuemore,              false,     false,      true },
    { "wallet",             "issuetoken",             &issuetoken,             false,     false,      true },
    { "wallet",             "getassetbalances",       &getassetbalances,       false,     false,      true },
    { "wallet",             "gettotalbalances",       &gettotalbalances,       false,     false,      true },
    { "hidden",             "sendassettoaddress",     &sendassettoaddress,     false,     false,      true },
    { "wallet",             "sendasset",              &sendassettoaddress,     false,     false,      true },
    { "wallet",             "preparelockunspent",     &preparelockunspent,     false,     false,      true },
    { "wallet",             "createrawexchange",      &createrawexchange,      false,     false,      true },
    { "wallet",             "appendrawexchange",      &appendrawexchange,      false,     false,      true },
    { "wallet",             "completerawexchange",    &completerawexchange,    false,     false,      true },
    { "wallet",             "decoderawexchange",      &decoderawexchange,      false,     false,      true },
    
    { "wallet",             "grantfrom",              &grantfromcmd,           false,     false,      true }, 
    { "wallet",             "approvefrom",            &approvefrom,            false,     false,      true }, 
    { "wallet",             "revokefrom",             &revokefromcmd,          false,     false,      true },
    { "wallet",             "issuefrom",              &issuefromcmd,           false,     false,      true },
    { "wallet",             "issuemorefrom",          &issuemorefrom,          false,     false,      true },
    { "wallet",             "issuetokenfrom",         &issuetokenfrom,         false,     false,      true },
    { "wallet",             "preparelockunspentfrom", &preparelockunspentfrom, false,     false,      true },
    { "wallet",             "sendassetfrom",          &sendassetfrom,          false,     false,      true },
    { "hidden",             "sendfromaddress",        &sendfromaddress,        false,     false,      true },
    { "wallet",             "sendfrom",               &sendfromaddress,        false,     false,      true },
    { "wallet",             "getmultibalances",       &getmultibalances,       false,     false,      true },
    { "wallet",             "gettokenbalances",       &gettokenbalances,       false,     false,      true },
    { "wallet",             "getaddressbalances",     &getaddressbalances,     false,     false,      true },
    { "wallet",             "disablerawtransaction",  &disablerawtransaction,  false,     false,      true },
    { "hidden",             "sendwithmetadata",       &sendwithmetadata,       false,     false,      true },
    { "wallet",             "sendwithdata",           &sendwithmetadata,       false,     false,      true },
    { "hidden",             "sendwithmetadatafrom",   &sendwithmetadatafrom,   false,     false,      true },
    { "wallet",             "sendwithdatafrom",       &sendwithmetadatafrom,   false,     false,      true },
    { "hidden",             "grantwithmetadata",      &grantwithmetadata,      false,     false,      true },
    { "wallet",             "grantwithdata",          &grantwithmetadata,      false,     false,      true },
    { "hidden",             "grantwithmetadatafrom",  &grantwithmetadatafrom,  false,     false,      true },
    { "wallet",             "grantwithdatafrom",      &grantwithmetadatafrom,  false,     false,      true },
    { "wallet",             "createrawsendfrom",      &createrawsendfrom,      false,     false,      true },
    
    { "wallet",             "listaddresses",          &listaddresses,          false,     false,      true },
    { "wallet",             "listwallettransactions", &listwallettransactions, false,     false,      true },
    { "wallet",             "listaddresstransactions",&listaddresstransactions,false,     false,      true },
    { "wallet",             "getwallettransaction",   &getwallettransaction,   false,     false,      true },
    { "wallet",             "getaddresstransaction",  &getaddresstransaction,  false,     false,      true },
    { "wallet",             "resendwallettransactions",&resendwallettransactions,  false,     false,      true },
    
    { "wallet",             "create",                 &createcmd,               false,     false,      true },
    { "wallet",             "createfrom",             &createfromcmd,           false,     false,      true },
    { "wallet",             "update",                 &updatecmd,               false,     false,      true }, 
    { "wallet",             "updatefrom",             &updatefromcmd,           false,     false,      true }, 
    { "wallet",             "publish",                &publish,                 false,     false,      true },
    { "wallet",             "publishfrom",            &publishfrom,             false,     false,      true },
    { "wallet",             "publishmulti",           &publishmulti,            false,     false,      true },
    { "wallet",             "publishmultifrom",       &publishmultifrom,        false,     false,      true },
    { "wallet",             "subscribe",              &subscribe,               false,     false,      true },
    { "wallet",             "unsubscribe",            &unsubscribe,             false,     false,      true },
    { "wallet",             "trimsubscribe",          &trimsubscribe,           false,     false,      true },
    { "wallet",             "retrievestreamitems",    &retrievestreamitems,     false,     false,      true },
    { "wallet",             "purgestreamitems",       &purgestreamitems,        false,     false,      true },
    { "wallet",             "purgepublisheditems",    &purgepublisheditems,     false,     false,      true },
    { "wallet",             "listassettransactions",  &listassettransactions,   false,     false,      true },
    { "wallet",             "getassettransaction",    &getassettransaction,     false,     false,      true },
    { "wallet",             "getstreamitem",          &getstreamitem,           false,      true,      true },
    { "wallet",             "liststreamtxitems",      &liststreamtxitems,       false,      true,      true },
    { "wallet",             "liststreamitems",        &liststreamitems,         false,      true,      false },
    { "wallet",             "liststreamqueryitems",   &liststreamqueryitems,    false,     true,      true },
    { "wallet",             "liststreamkeyitems",     &liststreamkeyitems,      false,      true,      true },
    { "wallet",             "liststreampublisheritems",&liststreampublisheritems,false,     true,      true },
    { "wallet",             "liststreamkeys",         &liststreamkeys,          false,      true,      true },
    { "wallet",             "liststreampublishers",   &liststreampublishers,    false,      true,      true },
    { "wallet",             "gettxoutdata",           &gettxoutdata,            false,     true,      true },
    { "wallet",             "txouttobinarycache",     &txouttobinarycache,      false,     true,      true },
    { "wallet",             "liststreamblockitems",   &liststreamblockitems,    false,     true,      false },
    { "wallet",             "getstreamkeysummary",    &getstreamkeysummary,     false,     true,      true },
    { "wallet",             "getstreampublishersummary",&getstreampublishersummary,false,  true,      true },
    { "hidden",             "storechunk",             &storechunk,              false,     false,      true },
    
    { "wallet",             "explorerlisttransactions",             &explorerlisttransactions,              false,      true,      false },
    { "wallet",             "explorerlistaddresses",       &explorerlistaddresses,        false,      true,      false },
    { "wallet",             "explorerlistaddresstransactions",      &explorerlistaddresstransactions,       false,      true,      false },
    { "wallet",             "explorerlistblocktransactions",        &explorerlistblocktransactions,         false,      true,      false },
    { "wallet",             "explorerlistredeemtransactions",       &explorerlistredeemtransactions,        false,      true,      false },
    { "wallet",             "explorerlistaddressassets",   &explorerlistaddressassets,    false,      true,      false },
    { "wallet",             "explorerlistaddressstreams",  &explorerlistaddressstreams,   false,      true,      false },
    { "wallet",             "explorerlistassetaddresses",  &explorerlistassetaddresses,   false,      true,      false },
    { "wallet",             "explorerlistaddressassettransactions", &explorerlistaddressassettransactions,  false,      true,      false },
    { "wallet",             "explorergetrawtransaction", &explorergetrawtransaction,                false,      false,     false },
    
/* MCHN END */    
    { "wallet",             "setaccount",             &setaccount,             true,      false,      true },
    { "wallet",             "settxfee",               &settxfee,               true,      false,      true },
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
    for (vcidx = 0; vcidx < (sizeof(vRPCWalletReadCommands) / sizeof(vRPCWalletReadCommands[0])); vcidx++)
    {
        vStaticRPCWalletReadCommands.push_back(vRPCCommands[vcidx]);
    }    
}
