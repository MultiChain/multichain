// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2014-2016 The Bitcoin Core developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2017 Coin Sciences Ltd
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
    { "listassets", 0 },
    { "listassets", 1 },                                                            
    { "listassets", 2 },                                                            
    { "listassets", 3 },                                                            
    { "liststreams", 0 },
    { "liststreams", 1 },                                                            
    { "liststreams", 2 },                                                            
    { "liststreams", 3 },                                                            
    { "listupgrades", 0 },
    { "listupgrades", 1 },                                                            
    { "listupgrades", 2 },                                                            
    { "listupgrades", 3 },                                                            
    { "publishfrom", 2 },                                                            
    { "publishfrom", 3 },                                                            
    { "publish", 1 },
    { "publish", 2 },                                                            
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
    { "preparelockunspentfrom", 1 },
    { "preparelockunspentfrom", 2 },
    { "getaddressbalances", 1 },
    { "getaddressbalances", 2 },
    { "getmultibalances", 0 },
    { "getmultibalances", 1 },
    { "getmultibalances", 2 },
    { "getmultibalances", 3 },
    { "getmultibalances", 4 },
    { "listaddresses", 0 },
    { "listaddresses", 1 },
    { "listaddresses", 2 },
    { "listaddresses", 3 },
    { "listpermissions", 1 },
    { "listpermissions", 2 },
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
    { "subscribe", 0 },
    { "subscribe", 1 },
    { "unsubscribe", 0 },
    { "listassettransactions", 1 },
    { "listassettransactions", 2 },
    { "listassettransactions", 3 },
    { "listassettransactions", 4 },
    { "getassettransaction", 2 },
    { "getstreamitem", 2 },
    { "liststreamtxitems", 2 },
    { "liststreamitems", 1 },
    { "liststreamitems", 2 },
    { "liststreamitems", 3 },
    { "liststreamitems", 4 },
    { "gettxoutdata", 1 },
    { "gettxoutdata", 2 },
    { "gettxoutdata", 3 },
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
    { "createrawtransaction", 0 },
    { "createrawtransaction", 1 },
    { "createrawtransaction", 2 },
    { "createrawsendfrom", 1 },
    { "createrawsendfrom", 2 },
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
    { "verifychain", 0 },
    { "verifychain", 1 },
    { "keypoolrefill", 0 },
    { "getrawmempool", 0 },
    { "estimatefee", 0 },
    { "estimatepriority", 0 },
    { "prioritisetransaction", 1 },
    { "prioritisetransaction", 2 },
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
    { "unsubscribe", 0 },
    { "liststreamkeys", 1 },
    { "liststreampublishers", 1 },
    { "listassets", 0 },
    { "liststreams", 0 },
    { "listupgrades", 0 },
    { "listpermissions", 1 },
    { "publishfrom", 2 },                                                            
    { "publishfrom", 3 },                                                            
    { "publish", 1 },
    { "publish", 2 },                                                            
    { "setgenerate", 0 },
    { "liststreamblockitems", 1 },
    { "listblocks", 0 },
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
                if (!rpcCvtTableMayBeString.maybestring(strMethod, idx)) 
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

