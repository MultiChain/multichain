// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2014-2016 The Bitcoin Core developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "rpc/rpcclient.h"

#include "rpc/rpcprotocol.h"
#include "utils/util.h"
#include "ui/ui_interface.h"

#include <set>
#include <stdint.h>

using namespace std;
using namespace json_spirit;

class CRPCConvertParam
{
public:
    std::string methodName;            //! method whose params want conversion
    int paramIdx;                      //! 0-based idx of param to convert
};

static const std::string vAPINames[] =
{
"addmultisigaddress",
"addnode",
"storenode",
"addresses-all",
"appendbinarycache",
"appendrawchange",
"appendrawdata",
"appendrawexchange",
"appendrawmetadata",
"appendrawtransaction",
"approvefrom",
"backupwallet",
"clearmempool",
"combineunspent",
"completerawexchange",
"create",
"createbinarycache",
"createfrom",
"createkeypairs",
"createmultisig",
"createrawexchange",
"createrawsendfrom",
"createrawtransaction",
"data-all",
"data-with",
"debug",
"decoderawexchange",
"decoderawtransaction",
"decodescript",
"deletebinarycache",
"disablerawtransaction",
"dumpprivkey",
"dumpwallet",
"encryptwallet",
"estimatefee",
"estimatepriority",
"filters",
"getaccount",
"getaccountaddress",
"getaddednodeinfo",
"getaddressbalances",
"getaddresses",
"getaddressesbyaccount",
"getaddresstransaction",
"getassetbalances",
"getassetinfo",
"getassettransaction",
"getbalance",
"getbestblockhash",
"getblock",
"getblockchaininfo",
"getblockchainparams",
"getblockcount",
"getblockhash",
"getblocktemplate",
"getchaintips",
"getchunkqueueinfo",
"getchunkqueuetotals",
"getconnectioncount",
"getdifficulty",
"getfilterassetbalances",
"getfiltertokenbalances",
"getfiltercode",
"getfilterstreamitem",
"getfilterstream",
"getfiltertransaction",
"getfiltertxid",
"getfiltertxinput",
"getgenerate",
"gethashespersec",
"gethealthcheck",
"getinfo",
"getentitycount",
"getinitstatus",
"getlastblockinfo",
"getmempoolinfo",
"getmininginfo",
"getmultibalances",
"gettokenbalances",
"getnettotals",
"getnetworkhashps",
"getnetworkinfo",
"getnewaddress",
"getpeerinfo",
"liststorednodes",
"getrawchangeaddress",
"getrawmempool",
"getrawtransaction",
"getreceivedbyaccount",
"getreceivedbyaddress",
"getruntimeparams",
"getstreaminfo",
"getstreamitem",
"getstreamkeysummary",
"getstreampublishersummary",
"gettotalbalances",
"gettransaction",
"gettxout",
"gettxoutdata",
"gettxoutsetinfo",
"getunconfirmedbalance",
"getwalletinfo",
"getwallettransaction",
"grant",
"grantfrom",
"grantwithdata",
"grantwithdatafrom",
"grantwithmetadata",
"grantwithmetadatafrom",
"help",
"importaddress",
"importprivkey",
"importwallet",
"invalidateblock",
"issue",
"issuefrom",
"issuemore",
"issuemorefrom",
"issuetoken",
"issuetokenfrom",
"keypoolrefill",
"listaccounts",
"listaddresses",
"listaddressgroupings",
"listaddresstransactions",
"listassets",
"listassettransactions",
"listblocks",
"listlockunspent",
"listpermissions",
"listminers",
"listreceivedbyaccount",
"listreceivedbyaddress",
"listsinceblock",
"liststreamblockitems",
"liststreamfilters",
"liststreamitems",
"liststreamkeyitems",
"liststreamkeys",
"liststreampublisheritems",
"liststreampublishers",
"liststreamqueryitems",
"liststreams",
"liststreamtxitems",
"listtransactions",
"listtxfilters",
"listunspent",
"listupgrades",
"listwallettransactions",
"lockunspent",
"move",
"pause",
"ping",
"preparelockunspent",
"preparelockunspentfrom",
"prioritisetransaction",
"publish",
"publishfrom",
"publishmulti",
"publishmultifrom",
"purgestreamitems",
"purgepublisheditems",
"reconsiderblock",
"resendwallettransactions",
"resume",
"retrievestreamitems",
"revoke",
"revokefrom",
"runstreamfilter",
"runtxfilter",
"send",
"sendasset",
"sendassetfrom",
"sendassettoaddress",
"sendfrom",
"sendfromaccount",
"sendfromaddress",
"sendmany",
"sendrawtransaction",
"sendtoaddress",
"sendwithdata",
"sendwithdatafrom",
"sendwithmetadata",
"sendwithmetadatafrom",
"setaccount",
"setfilterparam",
"setgenerate",
"setlastblock",
"setmocktime",
"setruntimeparam",
"settxfee",
"signmessage",
"signrawtransaction",
"stop",
"storechunk",
"submitblock",
"subscribe",
"teststreamfilter",
"testtxfilter",
"trimsubscribe",
"unsubscribe",
"validateaddress",
"verifychain",
"verifymessage",
"verifypermission",
"walletlock",
"walletpassphrase",
"walletpassphrasechange",
"txouttobinarycache",    
"trimsubscribe",
"retrievestreamitems",
"purgestreamitems",
"purgepublisheditems",
"getlicenserequest",
"decodelicenserequest",
"decodelicenseconfirmation",
"listlicenses",
"getlicenseconfirmation",
"activatelicense",
"transferlicense",
"takelicense",
"importlicenserequest",
"createfeed",
"deletefeed",
"pausefeed",
"resumefeed",
"addtofeed",
"updatefeed",
"purgefeed",       
"listfeeds",
"getdatarefdata", 
"datareftobinarycache", 
"listvariables", 
"getvariableinfo", 
"setvariablevalue", 
"setvariablevaluefrom", 
"getvariablevalue", 
"getvariablehistory", 
"listassetissues", 
"gettokeninfo", 
"listlibraries", 
"addlibraryupdate", 
"addlibraryupdatefrom", 
"getlibrarycode"
"explorerlisttransactions",
"explorerlistblocktransactions",
"explorerlistaddresstransactions",
"explorerlistaddresses",
"explorerlistredeemtransactions",
"explorerlistaddressassets",
"explorerlistaddressstreams",
"explorerlistassetaddresses",
"explorerlistaddressassettransactions",
"explorergetrawtransaction",
"getchaintotals",
"update",
"updatefrom",
"getdiagnostics",
"applycommands",
"decodehexubjson",
"encodehexubjson",
};

static const CRPCConvertParam vRPCConvertParams[] =
{
    { "stop", 0 },
    { "setmocktime", 0 },
    { "getaddednodeinfo", 0 },
    { "setgenerate", 0 },
    { "setgenerate", 1 },
    { "getnetworkhashps", 0 },
    { "getnetworkhashps", 1 },
    { "sendtoaddress", 1 },
    { "send", 1 },
/* MCHN START */    
//    { "setruntimeparam", 1 },                                                             
    { "debug", 1 },                                                             
    { "debug", 2 },                                                             
    { "createkeypairs", 0 },                                                             
    { "combineunspent", 1 },                                                             
    { "combineunspent", 2 },                                                             
    { "combineunspent", 3 },                                                             
    { "combineunspent", 4 },                                                             
    { "combineunspent", 5 },                                                             
    { "grant", 2 },                                                             
    { "grant", 3 },                                                             
    { "grant", 4 },                                                             
    { "grantwithmetadata", 2 },                                                             
    { "grantwithmetadata", 3 },                                                             
    { "grantwithmetadata", 4 },                                                             
    { "grantwithmetadata", 5 },                                                             
    { "grantwithdata", 2 },                                                             
    { "grantwithdata", 3 },                                                             
    { "grantwithdata", 4 },                                                             
    { "grantwithdata", 5 },                                                             
    { "revoke", 2 },                                                            
    { "issue", 1 },                                                            
    { "issue", 2 },                                                            
    { "issue", 3 },                                                            
    { "issue", 4 },                                                            
    { "issue", 5 },                                                            
    { "issuemore", 2 },                                                            
    { "issuemore", 3 },                                                            
    { "issuemore", 4 },                                                            
    { "getassetinfo", 1 },                                                            
    { "getstreaminfo", 1 },                                                            
    { "getvariableinfo", 1 },                                                            
    { "getfiltertxinput", 0 },                                                            
    { "listassets", 0 },
    { "listassets", 1 },                                                            
    { "listassets", 2 },                                                            
    { "listassets", 3 },                                                            
    { "liststreams", 0 },
    { "liststreams", 1 },                                                            
    { "liststreams", 2 },                                                            
    { "liststreams", 3 },                                                            
    { "listvariables", 0 },
    { "listvariables", 1 },                                                            
    { "listvariables", 2 },                                                            
    { "listvariables", 3 },                                                            
    { "listlibraries", 0 },
    { "listlibraries", 1 },                                                            
    { "listlibraries", 2 },                                                            
    { "listlibraries", 3 },                                                            
    { "getvariablehistory", 1 },                                                            
    { "getvariablehistory", 2 },                                                            
    { "getvariablehistory", 3 },         
    { "listassetissues", 1 },                                                            
    { "listassetissues", 2 },                                                            
    { "listassetissues", 3 },         
    { "gettokeninfo", 2 },                                                            
    { "listupgrades", 0 },
    { "listupgrades", 1 },                                                            
    { "listupgrades", 2 },                                                            
    { "listupgrades", 3 },                                                            
    { "listtxfilters", 0 },
    { "listtxfilters", 1 },                                                            
    { "liststreamfilters", 0 },
    { "liststreamfilters", 1 },                                                            
    { "testtxfilter", 0 },                                                            
    { "teststreamfilter", 0 },                                                            
    { "teststreamfilter", 3 },                                                            
    { "runstreamfilter", 2 },                                                            
    { "publishfrom", 2 },                                                            
    { "publishfrom", 3 },                                                            
    { "publish", 1 },
    { "publish", 2 },                                                            
    { "publishmultifrom", 2 },                                                            
    { "publishmulti", 1 },                                                            
    { "getassetbalances", 1 },
    { "getassetbalances", 2 },
    { "getassetbalances", 3 },
    { "gettotalbalances", 0 },
    { "gettotalbalances", 1 },
    { "gettotalbalances", 2 },
    { "sendassettoaddress", 2 },
    { "sendassettoaddress", 3 },
    { "sendasset", 2 },
    { "sendasset", 3 },
    { "getlastblockinfo", 0 },
    { "getblockchainparams", 0 },
    { "getblockchainparams", 1 },
    { "preparelockunspent", 0 },
    { "preparelockunspent", 1 },
    { "createrawexchange", 1 },
    { "createrawexchange", 2 },
    { "appendrawexchange", 2 },
    { "appendrawexchange", 3 },    
    { "completerawexchange", 2 },
    { "completerawexchange", 3 },    
    { "completerawexchange", 4 },    
    { "decoderawexchange", 1 },    
    { "appendrawmetadata", 1 },
    { "appendrawdata", 1 },
    { "sendfromaddress", 2 },
    { "sendfrom", 2 },
    { "sendassetfrom", 3 },
    { "sendassetfrom", 4 },
    { "grantwithmetadatafrom", 3 },                                                             
    { "grantwithmetadatafrom", 4 },                                                             
    { "grantwithmetadatafrom", 5 },                                                             
    { "grantwithmetadatafrom", 6 },                                                             
    { "grantwithdatafrom", 3 },                                                             
    { "grantwithdatafrom", 4 },                                                             
    { "grantwithdatafrom", 5 },                                                             
    { "grantwithdatafrom", 6 },                                                             
    { "grantfrom", 3 },                                                             
    { "grantfrom", 4 },                                                             
    { "grantfrom", 5 },                                                             
    { "approvefrom", 2 },                                                             
    { "revokefrom", 3 },                                                            
    { "issuefrom", 2 },                                                            
    { "issuefrom", 3 },                                                            
    { "issuefrom", 4 },                                                            
    { "issuefrom", 5 },                                                            
    { "issuefrom", 6 },                                                            
    { "issuemorefrom", 3 },                                                            
    { "issuemorefrom", 4 },                                                            
    { "issuemorefrom", 5 },                                                            
    { "issuetoken", 3},                                                            
    { "issuetoken", 4},                                                            
    { "issuetoken", 5},                                                            
    { "issuetokenfrom", 4},                                                            
    { "issuetokenfrom", 5},                                                            
    { "issuetokenfrom", 6},                                                                
    { "preparelockunspentfrom", 1 },
    { "preparelockunspentfrom", 2 },
    { "getaddressbalances", 1 },
    { "getaddressbalances", 2 },
    { "getmultibalances", 0 },
    { "getmultibalances", 1 },
    { "getmultibalances", 2 },
    { "getmultibalances", 3 },
    { "getmultibalances", 4 },
    { "gettokenbalances", 0 },
    { "gettokenbalances", 1 },
    { "gettokenbalances", 2 },
    { "gettokenbalances", 3 },
    { "gettokenbalances", 4 },
    { "listaddresses", 0 },
    { "listaddresses", 1 },
    { "listaddresses", 2 },
    { "listaddresses", 3 },
    { "listpermissions", 1 },
    { "listpermissions", 2 },
    { "listminers", 0 },
    { "sendwithmetadata", 1 },
    { "sendwithmetadata", 2 },
    { "sendwithdata", 1 },
    { "sendwithdata", 2 },
    { "sendwithmetadatafrom", 2 },
    { "sendwithmetadatafrom", 3 },
    { "sendwithdatafrom", 2 },
    { "sendwithdatafrom", 3 },
    { "getaddresses", 0 },
    { "listwallettransactions", 0 },
    { "listwallettransactions", 1 },
    { "listwallettransactions", 2 },
    { "listwallettransactions", 3 },
    { "listaddresstransactions", 1 },
    { "listaddresstransactions", 2 },
    { "listaddresstransactions", 3 },
    { "getwallettransaction", 1 },
    { "getwallettransaction", 2 },
    { "getaddresstransaction",2 },
    { "appendrawchange",2 },
    { "createfrom", 3 },                                                            
    { "createfrom", 4 },                                                            
    { "create", 2 },                                                            
    { "create", 3 },                                                            
    { "updatefrom", 2 },                                                            
    { "update", 1 },                                                            
    { "subscribe", 0 },
    { "subscribe", 1 },
    { "trimsubscribe", 0 },
    { "retrievestreamitems", 1 },
    { "purgestreamitems", 1 },
    { "purgepublisheditems", 0 },
    { "unsubscribe", 0 },
    { "unsubscribe", 1 },
    { "listassettransactions", 1 },
    { "listassettransactions", 2 },
    { "listassettransactions", 3 },
    { "listassettransactions", 4 },
    { "getassettransaction", 2 },
    { "getstreamitem", 2 },
    { "liststreamtxitems", 1 },
    { "liststreamtxitems", 2 },
    { "liststreamitems", 1 },
    { "liststreamitems", 2 },
    { "liststreamitems", 3 },
    { "liststreamitems", 4 },
    { "gettxoutdata", 1 },
    { "gettxoutdata", 2 },
    { "gettxoutdata", 3 },
    { "txouttobinarycache", 2 },
    { "txouttobinarycache", 3 },
    { "txouttobinarycache", 4 },
    { "liststreamkeys", 1 },
    { "liststreamkeys", 2 },
    { "liststreamkeys", 3 },
    { "liststreamkeys", 4 },
    { "liststreamkeys", 5 },
    { "liststreampublishers", 1 },
    { "liststreampublishers", 2 },
    { "liststreampublishers", 3 },
    { "liststreampublishers", 4 },
    { "liststreampublishers", 5 },
    { "liststreamqueryitems", 1 },
    { "liststreamqueryitems", 2 },
    { "liststreamkeyitems", 2 },
    { "liststreamkeyitems", 3 },
    { "liststreamkeyitems", 4 },
    { "liststreamkeyitems", 5 },
    { "liststreampublisheritems", 2 },
    { "liststreampublisheritems", 3 },
    { "liststreampublisheritems", 4 },
    { "liststreampublisheritems", 5 },
    { "liststreamblockitems", 1 },
    { "liststreamblockitems", 2 },
    { "liststreamblockitems", 3 },
    { "liststreamblockitems", 4 },
    { "listblocks", 0 },
    { "listblocks", 1 },
/* MCHN END */    
    { "settxfee", 0 },
    { "getreceivedbyaddress", 1 },
    { "getreceivedbyaccount", 1 },
    { "listreceivedbyaddress", 0 },
    { "listreceivedbyaddress", 1 },
    { "listreceivedbyaddress", 2 },
    { "listreceivedbyaccount", 0 },
    { "listreceivedbyaccount", 1 },
    { "listreceivedbyaccount", 2 },
    { "getbalance", 1 },
    { "getbalance", 2 },
    { "getblockhash", 0 },
    { "move", 2 },
    { "move", 3 },
//    { "sendfrom", 2 },
//    { "sendfrom", 3 },
    { "sendfromaccount", 2 },
    { "sendfromaccount", 3 },
    { "listtransactions", 1 },
    { "listtransactions", 2 },
    { "listtransactions", 3 },
    { "listaccounts", 0 },
    { "listaccounts", 1 },
    { "walletpassphrase", 1 },
    { "getblocktemplate", 0 },
    { "listsinceblock", 1 },
    { "listsinceblock", 2 },
    { "sendmany", 1 },
    { "sendmany", 2 },
    { "addmultisigaddress", 0 },
    { "addmultisigaddress", 1 },
    { "createmultisig", 0 },
    { "createmultisig", 1 },
    { "listunspent", 0 },
    { "listunspent", 1 },
    { "listunspent", 2 },
    { "getblock", 1 },
    { "gettransaction", 1 },
    { "getrawtransaction", 1 },
    { "appendrawtransaction", 1 },
    { "appendrawtransaction", 2 },
    { "appendrawtransaction", 3 },
    { "appendrawtransaction", 5 },
    { "createrawtransaction", 0 },
    { "createrawtransaction", 1 },
    { "createrawtransaction", 2 },
    { "createrawtransaction", 4 },
    { "createrawsendfrom", 1 },
    { "createrawsendfrom", 2 },
    { "createrawsendfrom", 4 },
    { "signrawtransaction", 1 },
    { "signrawtransaction", 2 },
    { "sendrawtransaction", 1 },
    { "gettxout", 1 },
    { "gettxout", 2 },
    { "lockunspent", 0 },
    { "lockunspent", 1 },
    { "importprivkey", 0 },
    { "importprivkey", 2 },
    { "importaddress", 0 },
    { "importaddress", 2 },
    { "importwallet", 1 },
    { "verifychain", 0 },
    { "verifychain", 1 },
    { "keypoolrefill", 0 },
    { "getrawmempool", 0 },
    { "estimatefee", 0 },
    { "estimatepriority", 0 },
    { "prioritisetransaction", 1 },
    { "prioritisetransaction", 2 },
    { "getlicenseconfirmation", 1 },
    { "listlicenses", 0 },
    { "listlicenses", 1 },
    { "getlicenserequest", 0 },
    { "createfeed", 1 },
    { "deletefeed", 1 },
    { "pausefeed", 1 },
    { "resumefeed", 1 },
    { "addtofeed", 1 },
    { "addtofeed", 4 },
    { "updatefeed", 1 },
    { "updatefeed", 4 },
    { "listfeeds", 0 },
    { "listfeeds", 1 },                                                            
    { "getdatarefdata", 1 },
    { "getdatarefdata", 2 },
    { "datareftobinarycache", 2 },
    { "datareftobinarycache", 3 },
    { "setvariablevaluefrom", 2 },                                                            
    { "setvariablevalue", 1 },                                                            
    { "explorerlisttransactions", 0 },
    { "explorerlisttransactions", 1 },
    { "explorerlisttransactions", 2 },
    { "explorerlistaddresses", 0 },
    { "explorerlistaddresses", 1 },
    { "explorerlistaddresses", 2 },
    { "explorerlistaddresses", 3 },
    { "explorerlistaddresstransactions", 1 },
    { "explorerlistaddresstransactions", 2 },
    { "explorerlistaddresstransactions", 3 },
    { "explorerlistaddressassets", 1 },
    { "explorerlistaddressassets", 2 },
    { "explorerlistaddressassets", 3 },
    { "explorerlistaddressstreams", 1 },
    { "explorerlistaddressstreams", 2 },
    { "explorerlistaddressstreams", 3 },
    { "explorerlistassetaddresses", 1 },
    { "explorerlistassetaddresses", 2 },
    { "explorerlistassetaddresses", 3 },
    { "explorerlistblocktransactions", 0 },
    { "explorerlistblocktransactions", 1 },
    { "explorerlistblocktransactions", 2 },
    { "explorerlistblocktransactions", 3 },
    { "explorerlistredeemtransactions", 1 },
    { "explorerlistaddressassettransactions", 2 },
    { "explorerlistaddressassettransactions", 3 },
    { "explorerlistaddressassettransactions", 4 },
    { "encodehexubjson", 0 },
    { "getdiagnostics", 0 },
    { "liststorednodes", 0 },
};

class CRPCConvertTable
{
private:
    std::set<std::pair<std::string, int> > members;

public:
    CRPCConvertTable();

    bool convert(const std::string& method, int idx) {
        return (members.count(std::make_pair(method, idx)) > 0);
    }
};

class CRPCNameTable
{
private:
    std::set<std::string > members;

public:
    CRPCNameTable();

    bool found(const std::string& method) {
        return (members.count(method) > 0);
    }
};

CRPCNameTable::CRPCNameTable()
{
    const unsigned int n_elem =
        (sizeof(vAPINames) / sizeof(vAPINames[0]));

    for (unsigned int i = 0; i < n_elem; i++) {
        members.insert(vAPINames[i]);
    }
}


CRPCConvertTable::CRPCConvertTable()
{
    const unsigned int n_elem =
        (sizeof(vRPCConvertParams) / sizeof(vRPCConvertParams[0]));

    for (unsigned int i = 0; i < n_elem; i++) {
        members.insert(std::make_pair(vRPCConvertParams[i].methodName,
                                      vRPCConvertParams[i].paramIdx));
    }
}

static CRPCConvertTable rpcCvtTable;
static CRPCNameTable rpcNamTable;

/* MCHN START */

class CRPCConvertParamMayBeString
{
public:
    std::string methodName;            //! method whose params want conversion
    int paramIdx;                      //! 0-based idx of param to convert
};

static const CRPCConvertParamMayBeString vRPCConvertParamsMayBeString[] =
{
    { "issue", 1 },
    { "issuefrom", 2 },
    { "appendrawmetadata", 1 },
    { "appendrawdata", 1 },
    { "getmultibalances", 0 },
    { "getmultibalances", 1 },
    { "gettokenbalances", 0 },
    { "gettokenbalances", 1 },
    { "listaddresses", 0 },
    { "grantwithmetadata", 2 },                                                             
    { "grantwithdata", 2 },                                                             
    { "grantwithmetadatafrom", 3 },                                                             
    { "grantwithdatafrom", 3 },                                                             
    { "sendwithmetadata", 2 },
    { "sendwithdata", 2 },
    { "sendwithmetadatafrom", 3 },
    { "sendwithdatafrom", 3 },
    { "completerawexchange", 4 },    
    { "importaddress", 0 },
    { "importprivkey", 0 },
    { "subscribe", 0 },
    { "trimsubscribe", 0 },
    { "retrievestreamitems", 1 },
    { "purgestreamitems", 1 },
    { "purgepublisheditems", 0 },
    { "unsubscribe", 0 },
    { "liststreamkeys", 1 },
    { "liststreampublishers", 1 },
    { "liststreamtxitems", 1 },
    { "listassets", 0 },
    { "liststreams", 0 },
    { "listvariables", 0 },
    { "listlibraries", 0 },
    { "listupgrades", 0 },
    { "listtxfilters", 0 },                                                            
    { "liststreamfilters", 0 },                                                            
    { "listpermissions", 1 },
    { "publishfrom", 2 },                                                            
    { "publishfrom", 3 },                                                            
    { "publish", 1 },
    { "publish", 2 },                                                            
    { "setgenerate", 0 },
    { "liststreamblockitems", 1 },
    { "listblocks", 0 },
    { "createfrom", 4 },                                                            
    { "create", 3 },                                                            
    { "listlicenses", 0 },
    { "addtofeed", 1 },
    { "updatefeed", 1 },
    { "listfeeds", 0 },
    { "setvariablevaluefrom", 2 },                                                            
    { "setvariablevalue", 1 },                                                                
    { "explorerlistaddresses", 0 },
    { "explorerlisttransactions", 0 },
    { "explorerlistaddresses", 1 },
    { "explorerlistaddresstransactions", 1 },
    { "explorerlistblocktransactions", 1 },
    { "explorerlistaddressstreams", 1 },
    { "explorerlistaddressassets", 1 },
    { "explorerlistassetaddresses", 1 },
    { "explorerlistaddressassettransactions", 1 },
};

class CRPCConvertTableMayBeString
{
private:
    std::set<std::pair<std::string, int> > members;

public:
    CRPCConvertTableMayBeString();

    bool maybestring(const std::string& method, int idx) {
        return (members.count(std::make_pair(method, idx)) > 0);
    }
};

CRPCConvertTableMayBeString::CRPCConvertTableMayBeString()
{
    const unsigned int n_elem =
        (sizeof(vRPCConvertParamsMayBeString) / sizeof(vRPCConvertParamsMayBeString[0]));

    for (unsigned int i = 0; i < n_elem; i++) {
        members.insert(std::make_pair(vRPCConvertParamsMayBeString[i].methodName,
                                      vRPCConvertParamsMayBeString[i].paramIdx));
    }
}

static CRPCConvertTableMayBeString rpcCvtTableMayBeString;

class CRPCConvertParamAnyType
{
public:
    std::string methodName;            //! method whose params want conversion
    int paramIdx;                      //! 0-based idx of param to convert
};

static const CRPCConvertParamAnyType vRPCConvertParamsAnyType[] =
{
    { "createfrom", 4 },                                                            
    { "create", 3 },                                                            
    { "setvariablevaluefrom", 2 },                                                            
    { "setvariablevalue", 1 },                                                                
};

class CRPCConvertTableAnyType
{
private:
    std::set<std::pair<std::string, int> > members;

public:
    CRPCConvertTableAnyType();

    bool anytype(const std::string& method, int idx) {
        return (members.count(std::make_pair(method, idx)) > 0);
    }
};

CRPCConvertTableAnyType::CRPCConvertTableAnyType()
{
    const unsigned int n_elem =
        (sizeof(vRPCConvertParamsAnyType) / sizeof(vRPCConvertParamsAnyType[0]));

    for (unsigned int i = 0; i < n_elem; i++) {
        members.insert(std::make_pair(vRPCConvertParamsAnyType[i].methodName,
                                      vRPCConvertParamsAnyType[i].paramIdx));
    }
}

static CRPCConvertTableAnyType rpcCvtTableAnyType;

bool HaveAPIWithThisName(const std::string &strMethod)
{
    return rpcNamTable.found(strMethod);
}

/* MCHN END */

/** Convert strings to command-specific RPC representation */
Array RPCConvertValues(const std::string &strMethod, const std::vector<std::string> &strParams)
{
    Array params;

    for (unsigned int idx = 0; idx < strParams.size(); idx++) {
        const std::string& strVal = strParams[idx];

        // insert string value directly
        if (!rpcCvtTable.convert(strMethod, idx)) {
//            params.push_back(strVal);
            std::string strConverted=convert_string_to_utf8(strVal);
            params.push_back(strConverted);
        }

        // parse string as JSON, insert bool/number/object/etc. value
        else {
            Value jVal;
            if (!read_string(strVal, jVal))
/* MCHN START */
            {
                if (!rpcCvtTableMayBeString.maybestring(strMethod, idx)) 
                {
                    throw runtime_error(string("Error parsing JSON:")+strVal);
                }
                else
                {
//                    params.push_back(strVal);                    
                    std::string strConverted=convert_string_to_utf8(strVal);
                    params.push_back(strConverted);
                }
            }
            else
            {
                if (!rpcCvtTableMayBeString.maybestring(strMethod, idx) ||
                     rpcCvtTableAnyType.anytype(strMethod, idx)) 
                {
                    params.push_back(jVal);                    
                }
                else
                {
                    if(jVal.type() == obj_type)
                    {
                        params.push_back(jVal);                                            
                    }
                    else
                    {
                        if(jVal.type() == array_type)
                        {
                            params.push_back(jVal);                                            
                        }
                        else
                        {
                            if(jVal.type() == bool_type)
                            {
                                params.push_back(jVal);                                            
                            }
                            else
                            {
                                std::string strConverted=convert_string_to_utf8(strVal);
                                params.push_back(strConverted);
    //                            params.push_back(strVal);                                            
                            }
                        }
                    }
                }
            }
/* MCHN END */
        }
    }

    return params;
}

