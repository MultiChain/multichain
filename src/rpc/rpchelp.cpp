// Copyright (c) 2014-2016 The Bitcoin Core developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.


#include "core/main.h"
#include "rpc/rpcserver.h"
#include "rpc/rpcutils.h"
#include "community/community.h"

std::string HelpRequiringPassphraseWrapper()
{
#ifdef ENABLE_WALLET
     return HelpRequiringPassphrase();
#else
     return "";
#endif    
}

std::string mc_RPCHelpString(std::string strMethod)
{
    string strHelp="";
    map<string, string>::iterator it = mapHelpStrings.find(strMethod);
    if (it != mapHelpStrings.end())
    {
        strHelp=it->second;
    }
    else
    {
        throw runtime_error("Help message not found\n");        
    }                            
    
    return strHelp;
}

void mc_ThrowHelpMessage(std::string strMethod)
{
    if(pMultiChainFilterEngine->m_TxID != 0)
    {
        throw JSONRPCError(RPC_INVALID_PARAMS, "Wrong number of parameters");          
        //throw JSONRPCError(RPC_MISC_ERROR, mc_RPCHelpString(strMethod));
    }
    throw runtime_error("Help message not found\n");
}

void mc_InitRPCHelpMap01()
{
    mapHelpStrings.insert(std::make_pair("getbestblockhash",
            "getbestblockhash\n"
            "\nReturns the hash of the best (tip) block in the longest block chain.\n"
            "\nResult\n"
            "\"hex\"                               (string) the block hash hex encoded\n"
            "\nExamples\n"
            + HelpExampleCli("getbestblockhash", "")
            + HelpExampleRpc("getbestblockhash", "")
        ));
    
    mapHelpStrings.insert(std::make_pair("getblock",
            "getblock \"hash\"|height ( verbose )\n"
            "\nReturns hex-encoded data or json object for block.\n"
            "\nArguments:\n"
            "1. \"hash\"                           (string, required) The block hash\n"
            " or\n"
            "1. height                           (numeric, required) The block height in active chain\n"
            "2. verbose                          (numeric or boolean, optional, default=1) 0(or false) - encoded data, 1(or true) - json object,\n"
            "                                                                              2 - with tx encoded data, 4 - with tx json object\n"
            "\nResult (for verbose = 1, see help getrawtransaction for details about transactions - verbose = 4):\n"
            "{\n"
            "  \"hash\" : \"hash\",                  (string) the block hash (same as provided)\n"
            "  \"miner\" : \"miner\",                (string) the address of the miner\n"
            "  \"confirmations\" : n,              (numeric) The number of confirmations, or -1 if the block is not on the main chain\n"
            "  \"size\" : n,                       (numeric) The block size\n"
            "  \"height\" : n,                     (numeric) The block height or index\n"
            "  \"version\" : n,                    (numeric) The block version\n"
            "  \"merkleroot\" : \"xxxx\",            (string) The merkle root\n"
            "  \"tx\" : [                          (array of strings) The transaction ids\n"
            "     \"transactionid\"                (string) The transaction id\n"
            "     ,...\n"
            "  ],\n"
            "  \"time\" : ttt,                     (numeric) The block time in seconds since epoch (Jan 1 1970 GMT)\n"
            "  \"nonce\" : n,                      (numeric) The nonce\n"
            "  \"bits\" : \"1d00ffff\",              (string) The bits\n"
            "  \"difficulty\" : x.xxx,             (numeric) The difficulty\n"
            "  \"previousblockhash\" : \"hash\",     (string) The hash of the previous block\n"
            "  \"nextblockhash\" : \"hash\"          (string) The hash of the next block\n"
            "}\n"
            "\nResult (for verbose=false):\n"
            "\"data\"                              (string) A string that is serialized, hex-encoded data for block 'hash'.\n"
            "\nExamples:\n"
            + HelpExampleCli("getblock", "\"00000000c937983704a73af28acdec37b049d214adbda81d7e2a3dd146f6ed09\"")
            + HelpExampleRpc("getblock", "\"00000000c937983704a73af28acdec37b049d214adbda81d7e2a3dd146f6ed09\"")
        ));
    
    mapHelpStrings.insert(std::make_pair("getblockchaininfo",
            "getblockchaininfo\n"
            "Returns an object containing various state info regarding block chain processing.\n"
            "\nResult:\n"
            "{\n"
            "  \"chain\": \"xxxx\",                  (string) current network name as defined in BIP70 (main, test, regtest)\n"
            "  \"chainname\": \"xxxx\",              (string) multichain network name\n"
            "  \"description\": \"xxxx\",            (string) network desctription\n"
            "  \"protocol\": \"xxxx\",               (string) protocol - multichain or bitcoin\n"
            "  \"setupblocks\": \"xxxx\",            (string) number of network setup blocks\n"
            "  \"blocks\": xxxxxx,                 (numeric) the current number of blocks processed in the server\n"
            "  \"headers\": xxxxxx,                (numeric) the current number of headers we have validated\n"
            "  \"bestblockhash\": \"...\",           (string) the hash of the currently best block\n"
            "  \"difficulty\": xxxxxx,             (numeric) the current difficulty\n"
            "  \"verificationprogress\": xxxx,     (numeric) estimate of verification progress [0..1]\n"
            "  \"chainwork\": \"xxxx\"               (string) total amount of work in active chain, in hexadecimal\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("getblockchaininfo", "")
            + HelpExampleRpc("getblockchaininfo", "")
        ));
    
    mapHelpStrings.insert(std::make_pair("getblockcount",
            "getblockcount\n"
            "\nReturns the number of blocks in the longest block chain.\n"
            "\nResult:\n"
            "n                                   (numeric) The current block count\n"
            "\nExamples:\n"
            + HelpExampleCli("getblockcount", "")
            + HelpExampleRpc("getblockcount", "")
        ));
    
    mapHelpStrings.insert(std::make_pair("getblockhash",
            "getblockhash index\n"
            "\nReturns hash of block in best-block-chain at index provided.\n"
            "\nArguments:\n"
            "1. index                            (numeric, required) The block index\n"
            "\nResult:\n"
            "\"hash\"                              (string) The block hash\n"
            "\nExamples:\n"
            + HelpExampleCli("getblockhash", "1000")
            + HelpExampleRpc("getblockhash", "1000")
        ));
    
    mapHelpStrings.insert(std::make_pair("getchaintips",
            "getchaintips\n"
            "Return information about all known tips in the block tree,"
            " including the main chain as well as orphaned branches.\n"
            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"height\": xxxx,                 (numeric) height of the chain tip\n"
            "    \"hash\": \"xxxx\",                 (string) block hash of the tip\n"
            "    \"branchlen\": 0                  (numeric) zero for main chain\n"
            "    \"status\": \"active\"              (string) \"active\" for the main chain\n"
            "  },\n"
            "  {\n"
            "    \"height\": xxxx,\n"
            "    \"hash\": \"xxxx\",\n"
            "    \"branchlen\": 1                  (numeric) length of branch connecting the tip to the main chain\n"
            "    \"status\": \"xxxx\"                (string) status of the chain (active, valid-fork, valid-headers, headers-only, invalid)\n"
            "  }\n"
            "]\n"
            "Possible values for status:\n"
            "1.  \"invalid\"                       This branch contains at least one invalid block\n"
            "2.  \"headers-only\"                  Not all blocks for this branch are available, but the headers are valid\n"
            "3.  \"valid-headers\"                 All blocks are available for this branch, but they were never fully validated\n"
            "4.  \"valid-fork\"                    This branch is not part of the active chain, but is fully validated\n"
            "5.  \"active\"                        This is the tip of the active main chain, which is certainly valid\n"
            "\nExamples:\n"
            + HelpExampleCli("getchaintips", "")
            + HelpExampleRpc("getchaintips", "")
        ));
    
    mapHelpStrings.insert(std::make_pair("getdifficulty",
            "getdifficulty\n"
            "\nReturns the proof-of-work difficulty as a multiple of the minimum difficulty.\n"
            "\nResult:\n"
            "n.nnn                                 (numeric) the proof-of-work difficulty as a multiple of the minimum difficulty.\n"
            "\nExamples:\n"
            + HelpExampleCli("getdifficulty", "")
            + HelpExampleRpc("getdifficulty", "")
        ));
    
    mapHelpStrings.insert(std::make_pair("getmempoolinfo",
            "getmempoolinfo\n"
            "\nReturns details on the active state of the TX memory pool.\n"
            "\nResult:\n"
            "{\n"
            "  \"size\": xxxxx                     (numeric) Current tx count\n"
            "  \"bytes\": xxxxx                    (numeric) Sum of all tx sizes\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("getmempoolinfo", "")
            + HelpExampleRpc("getmempoolinfo", "")
        ));
    
    mapHelpStrings.insert(std::make_pair("getrawmempool",
            "getrawmempool ( verbose )\n"
            "\nReturns all transaction ids in memory pool as a json array of string transaction ids.\n"
            "\nArguments:\n"
            "1. verbose                          (boolean, optional, default=false) true for a json object, false for array of transaction ids\n"
            "\nResult: (for verbose = false):\n"
            "[                                   (json array of string)\n"
            "  \"transactionid\"                   (string) The transaction id\n"
            "  ,...\n"
            "]\n"
            "\nResult: (for verbose = true):\n"
            "{                                   (json object)\n"
            "  \"transactionid\" : {               (json object)\n"
            "    \"size\" : n,                     (numeric) transaction size in bytes\n"
            "    \"fee\" : n,                      (numeric) transaction fee in native currency units\n"
            "    \"time\" : n,                     (numeric) local time transaction entered pool in seconds since 1 Jan 1970 GMT\n"
            "    \"height\" : n,                   (numeric) block height when transaction entered pool\n"
            "    \"startingpriority\" : n,         (numeric) priority when transaction entered pool\n"
            "    \"currentpriority\" : n,          (numeric) transaction priority now\n"
            "    \"depends\" : [                   (array) unconfirmed transactions used as inputs for this transaction\n"
            "        \"transactionid\",            (string) parent transaction id\n"
            "       ... ]\n"
            "  }, ...\n"
            "]\n"
            "\nExamples\n"
            + HelpExampleCli("getrawmempool", "true")
            + HelpExampleRpc("getrawmempool", "true")
        ));
    
    mapHelpStrings.insert(std::make_pair("gettxout",
            "gettxout \"txid\" n ( includemempool )\n"
            "\nReturns details about an unspent transaction output.\n"
            "\nArguments:\n"
            "1. \"txid\"                           (string, required) The transaction id\n"
            "2. n                                (numeric, required) vout value\n"
            "3. includemempool                   (boolean, optional, default true) Whether to included the mem pool\n"
            "\nResult:\n"
            "{\n"
            "  \"bestblock\" : \"hash\",             (string) the block hash\n"
            "  \"confirmations\" : n,              (numeric) The number of confirmations\n"
            "  \"value\" : x.xxx,                  (numeric) The transaction value in btc\n"
            "  \"scriptPubKey\" : {                (json object)\n"
            "     \"asm\" : \"code\",                (string) \n"
            "     \"hex\" : \"hex\",                 (string) \n"
            "     \"reqSigs\" : n,                 (numeric) Number of required signatures\n"
            "     \"type\" : \"pubkeyhash\",         (string) The type, eg pubkeyhash\n"
            "     \"addresses\" : [                (array of string) array of addresses\n"
            "        \"address\"                   (string) address\n"
            "        ,...\n"
            "     ]\n"
            "  },\n"
            "  \"version\" : n,                    (numeric) The version\n"
            "  \"coinbase\" : true|false           (boolean) Coinbase or not\n"
            "}\n"

            "\nExamples:\n"
            "\nGet unspent transactions\n"
            + HelpExampleCli("listunspent", "") +
            "\nView the details\n"
            + HelpExampleCli("gettxout", "\"txid\" 1") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("gettxout", "\"txid\", 1")
        ));
}

void mc_InitRPCHelpMap02()
{
    mapHelpStrings.insert(std::make_pair("gettxoutsetinfo",
            "gettxoutsetinfo\n"
            "\nReturns statistics about the unspent transaction output set.\n"
            "Note this call may take some time.\n"
            "\nResult:\n"
            "{\n"
            "  \"height\":n,                       (numeric) The current block height (index)\n"
            "  \"bestblock\": \"hex\",               (string) the best block hash hex\n"
            "  \"transactions\": n,                (numeric) The number of transactions\n"
            "  \"txouts\": n,                      (numeric) The number of output transactions\n"
            "  \"bytes_serialized\": n,            (numeric) The serialized size\n"
            "  \"hash_serialized\": \"hash\",        (string) The serialized hash\n"
            "  \"total_amount\": x.xxx             (numeric) The total amount\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("gettxoutsetinfo", "")
            + HelpExampleRpc("gettxoutsetinfo", "")
        ));
    
    mapHelpStrings.insert(std::make_pair("listassets",
            "listassets ( asset-identifier(s) verbose count start )\n"
            "\nReturns list of defined assets\n"
            "\nArguments:\n"
            "1. \"asset-identifier\"               (string, optional, default=*) Asset identifier - one of: issue txid, asset reference, asset name.\n"
            " or\n"
            "1. asset-identifier(s)              (array, optional) A json array of asset identifiers \n"                
            "2. verbose                          (boolean, optional, default=false) If true, returns list of all issue transactions, including follow-ons \n"
            "3. count                            (number, optional, default=INT_MAX - all) The number of assets to display\n"
            "4. start                            (number, optional, default=-count - last) Start from specific asset, 0 based, if negative - from the end\n"
            "\nResult:\n"
            "An array containing list of defined assets\n"            
            "\nExamples:\n"
            + HelpExampleCli("listassets", "")
            + HelpExampleRpc("listassets", "")
        ));
    
    mapHelpStrings.insert(std::make_pair("listpermissions",
            "listpermissions ( \"permission(s)\" address(es) verbose )\n"
            "\nReturns a list of all permissions which have been explicitly granted to addresses.\n"
            "\nArguments:\n"
            "1. \"permission(s)\"                  (string, optional, default=*) Permission strings, comma delimited. Possible values: " + AllowedPermissions() + "\n"                
            "2. \"address(es)\"                    (string, optional, default=*) The addresses to retrieve permissions for. \"*\" for all addresses\n"
            " or\n"
            "2. address(es)                      (array, optional) A json array of addresses to return permissions for\n"                
            "3. verbose                          (boolean, optional, default=false) If true, returns list of pending grants \n"
            "\nResult:\n"
            "An array containing list of permissions\n"            
            "\nExamples:\n"
            + HelpExampleCli("listpermissions", "connect,send,receive")
            + HelpExampleCli("listpermissions", "all \"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\"" )
            + HelpExampleRpc("listpermissions", "connect,send,receive")
        ));
    
    mapHelpStrings.insert(std::make_pair("liststreams",
            "liststreams ( stream-identifier(s) verbose count start )\n"
            "\nReturns list of defined streams\n"
            "\nArguments:\n"
            "1. \"stream-identifier(s)\"           (string, optional, default=*) Stream identifier - one of: create txid, stream reference, stream name.\n"
            " or\n"
            "1. stream-identifier(s)             (array, optional) A json array of stream identifiers \n"                
            "2. verbose                          (boolean, optional, default=false) If true, returns list of stream creators \n"
            "3. count                            (number, optional, default=INT_MAX - all) The number of streams to display\n"
            "4. start                            (number, optional, default=-count - last) Start from specific stream, 0 based, if negative - from the end\n"
            "\nResult:\n"
            "An array containing list of defined streams\n"            
            "\nExamples:\n"
            + HelpExampleCli("liststreams", "")
            + HelpExampleRpc("liststreams", "")
        ));
    
    mapHelpStrings.insert(std::make_pair("verifychain",
            "verifychain ( checklevel numblocks )\n"
            "\nVerifies blockchain database.\n"
            "\nArguments:\n"
            "1. checklevel                       (numeric, optional, 0-4, default=3) How thorough the block verification is.\n"
            "2. numblocks                        (numeric, optional, default=288, 0=all) The number of blocks to check.\n"
            "\nResult:\n"
            "true|false                          (boolean) Verified or not\n"
            "\nExamples:\n"
            + HelpExampleCli("verifychain", "")
            + HelpExampleRpc("verifychain", "")
        ));
    
    mapHelpStrings.insert(std::make_pair("clearmempool",
            "clearmempool \n"
            "\nRemoves all transactions from the TX memory pool.\n"
            "Local mining and the processing of incoming transactions and blocks should be paused.\n"
            "\nExamples:\n"
            + HelpExampleCli("clearmempool", "")
            + HelpExampleRpc("clearmempool", "")
        ));
    
    mapHelpStrings.insert(std::make_pair("getblockchainparams",
            "getblockchainparams ( displaynames with-upgrades )\n"    
            "\nReturns a list of values of this blockchain’s parameters\n"
            "\nArguments:\n"
            "1. displaynames                     (boolean, optional, default=true) use display names instead of internal\n"
//            "2. height                           (numeric or boolean, optional, default true) The block height in active chain or height before current tip (if negative)\n"
//            "                                    false - original configuration (height=0), true - current configuration\n"
            "2. with-upgrades                    (boolean, optional, default=true) Take upgrades into account \n"
            "\nResult:\n"
            "An object containing various blockchain parameters.\n"
            "\nExamples:\n"
            + HelpExampleCli("getblockchainparams", "")
            + HelpExampleRpc("getblockchainparams", "")
        ));
    
    mapHelpStrings.insert(std::make_pair("getinfo",
            "getinfo\n"
            "\nReturns general information about this node and blockchain.\n"
            "\nResult:\n"
            "{\n"
            "  \"version\": xxxxx,                 (numeric) the server version\n"
            "  \"protocolversion\": xxxxx,         (numeric) the protocol version\n"
            "  \"chainname\": \"xxxx\",              (string) multichain network name\n"
            "  \"description\": \"xxxx\",            (string) network desctription\n"
            "  \"protocol\": \"xxxx\",               (string) protocol - multichain or bitcoin\n"
            "  \"port\": xxxx,                     (numeric) network port\n"
            "  \"setupblocks\": \"xxxx\",            (string) number of network setup blocks\n"
            "  \"walletversion\": xxxxx,           (numeric) the wallet version\n"
            "  \"balance\": xxxxxxx,               (numeric) the total native currency balance of the wallet\n"
            "  \"walletdbversion\": xxxxx,         (numeric) the wallet database version\n"
            "  \"blocks\": xxxxxx,                 (numeric) the current number of blocks processed in the server\n"
            "  \"timeoffset\": xxxxx,              (numeric) the time offset\n"
            "  \"connections\": xxxxx,             (numeric) the number of connections\n"
            "  \"proxy\": \"host:port\",             (string, optional) the proxy used by the server\n"
            "  \"difficulty\": xxxxxx,             (numeric) the current difficulty\n"
            "  \"testnet\": true|false,            (boolean) if the server is using testnet or not\n"
            "  \"keypoololdest\": xxxxxx,          (numeric) the timestamp (seconds since GMT epoch) of the oldest pre-generated key in the key pool\n"
            "  \"keypoolsize\": xxxx,              (numeric) how many new keys are pre-generated\n"
            "  \"unlocked_until\": ttt,            (numeric) the timestamp in seconds since epoch (midnight Jan 1 1970 GMT) that the wallet is unlocked for transfers, or 0 if the wallet is locked\n"
            "  \"paytxfee\": x.xxxx,               (numeric) the transaction fee set in btc/kb\n"
            "  \"relayfee\": x.xxxx,               (numeric) minimum relay fee for non-free transactions in btc/kb\n"
            "  \"errors\": \"...\"                   (string) any error messages\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("getinfo", "")
            + HelpExampleRpc("getinfo", "")
        ));
    
    mapHelpStrings.insert(std::make_pair("help",
            "help ( command )\n"
            "\nList all commands, or get help for a specified command.\n"
            "\nArguments:\n"
            "1. \"command\"                        (string, optional) The command to get help on\n"
            "\nResult:\n"
            "\"text\"                              (string) The help text\n"
        ));
    
    mapHelpStrings.insert(std::make_pair("pause",
            "pause \"task(s)\" \n"
            "\nPauses local mining or the processing of incoming transactions and blocks.\n"
            "\nArguments:\n"
            "1. \"task(s)\"                        (string, required) Task(s) to be paused. Possible values: " + AllowedPausedServices() + " \n"
            "\nExamples:\n"
            + HelpExampleCli("pause", "incoming,mining")
            + HelpExampleRpc("pause", "incoming")
        ));
    
}

void mc_InitRPCHelpMap03()
{
    mapHelpStrings.insert(std::make_pair("resume",
            "resume \"task(s)\" \n"
            "\nResumes local mining or the processing of incoming transactions and blocks\n"
            "\nArguments:\n"
            "1. \"task(s)\"                        (string, required) Task(s) to be resumed. Possible values: " + AllowedPausedServices() + " \n"
            "\nExamples:\n"
            + HelpExampleCli("resume", "incoming,mining")
            + HelpExampleRpc("resume", "mining")
        ));
    
    mapHelpStrings.insert(std::make_pair("setlastblock",
            "setlastblock ( \"hash\"|height )\n"
            "\nSets last block in the chain.\n"
            "Local mining and the processing of incoming transactions and blocks should be paused.\n"
            "\nArguments:\n"
            "1. \"hash\"                           (string, optional) The block hash, if omitted - best chain is activated\n"
            " or\n"
            "1. height                           (numeric, optional) The block height in active chain or height before current tip (if negative)\n"
            "\nResult:\n"
            "\"hash\"                              (string) The block hash of the chain tip\n"
            "\nExamples:\n"
            + HelpExampleCli("setlastblock", "\"00000000c937983704a73af28acdec37b049d214adbda81d7e2a3dd146f6ed09\"")
            + HelpExampleRpc("setlastblock", "\"00000000c937983704a73af28acdec37b049d214adbda81d7e2a3dd146f6ed09\"")
        ));
    
    mapHelpStrings.insert(std::make_pair("stop",
             "stop\n"
            "\nShuts down the this blockchain node. Sends stop signal to MultiChain server."
        ));
    
    mapHelpStrings.insert(std::make_pair("getgenerate",
            "getgenerate\n"
            "\nReturn if the server is set to generate coins or not. The default is false.\n"
            "It is set with the command line argument -gen (or bitcoin.conf setting gen)\n"
            "It can also be set with the setgenerate call.\n"
            "\nResult\n"
            "true|false                          (boolean) If the server is set to generate coins or not\n"
            "\nExamples:\n"
            + HelpExampleCli("getgenerate", "")
            + HelpExampleRpc("getgenerate", "")
        ));
    
    mapHelpStrings.insert(std::make_pair("gethashespersec",
            "gethashespersec\n"
            "\nReturns a recent hashes per second performance measurement while generating.\n"
            "See the getgenerate and setgenerate calls to turn generation on and off.\n"
            "\nResult:\n"
            "n                                   (numeric) The recent hashes per second when generation is on (will return 0 if generation is off)\n"
            "\nExamples:\n"
            + HelpExampleCli("gethashespersec", "")
            + HelpExampleRpc("gethashespersec", "")
        ));
    
    mapHelpStrings.insert(std::make_pair("setgenerate",
            "setgenerate generate ( genproclimit )\n"
            "\nSet 'generate' true or false to turn generation on or off.\n"
            "Generation is limited to 'genproclimit' processors, -1 is unlimited.\n"
            "See the getgenerate call for the current setting.\n"
            "\nArguments:\n"
            "1. generate                         (boolean, required) Set to true to turn on generation, off to turn off.\n"
            "2. genproclimit                     (numeric, optional, default = 1) Set the processor limit for when generation is on. Can be -1 for unlimited.\n"
            "\nResult\n"
            "[ blockhashes ]                     (array, -regtest only) hashes of blocks generated\n"
            "\nExamples:\n"
            "\nSet the generation on with a limit of one processor\n"
            + HelpExampleCli("setgenerate", "true 1") +
            "\nCheck the setting\n"
            + HelpExampleCli("getgenerate", "") +
            "\nTurn off generation\n"
            + HelpExampleCli("setgenerate", "false") +
            "\nUsing json rpc\n"
            + HelpExampleRpc("setgenerate", "true, 1")
        ));
    
    mapHelpStrings.insert(std::make_pair("getblocktemplate",
            "getblocktemplate ( \"jsonrequestobject\" )\n"
            "\nIf the request parameters include a 'mode' key, that is used to explicitly select between the default 'template' request or a 'proposal'.\n"
            "It returns data needed to construct a block to work on.\n"
            "See https://en.bitcoin.it/wiki/BIP_0022 for full specification.\n"

            "\nArguments:\n"
            "1. \"jsonrequestobject\"              (string, optional) A json object in the following spec\n"
            "     {\n"
            "       \"mode\":\"template\"            (string, optional) This must be set to \"template\" or omitted\n"
            "       \"capabilities\":[             (array, optional) A list of strings\n"
            "           \"support\"                (string) client side supported feature, 'longpoll', 'coinbasetxn', 'coinbasevalue', 'proposal', 'serverlist', 'workid'\n"
            "           ,...\n"
            "         ]\n"
            "     }\n"
            "\n"

            "\nResult:\n"
            "{\n"
            "  \"version\" : n,                    (numeric) The block version\n"
            "  \"previousblockhash\" : \"xxxx\",     (string) The hash of current highest block\n"
            "  \"transactions\" : [                (array) contents of non-coinbase transactions that should be included in the next block\n"
            "      {\n"
            "         \"data\" : \"xxxx\",           (string) transaction data encoded in hexadecimal (byte-for-byte)\n"
            "         \"hash\" : \"xxxx\",           (string) hash/id encoded in little-endian hexadecimal\n"
            "         \"depends\" : [              (array) array of numbers \n"
            "             n                      (numeric) transactions before this one (by 1-based index in 'transactions' list) that must be present in the final block if this one is\n"
            "             ,...\n"
            "         ],\n"
            "         \"fee\": n,                  (numeric) difference in value between transaction inputs and outputs (in Satoshis); for coinbase transactions, this is a negative Number of the total collected block fees (ie, not including the block subsidy); if key is not present, fee is unknown and clients MUST NOT assume there isn't one\n"
            "         \"sigops\" : n,              (numeric) total number of SigOps, as counted for purposes of block limits; if key is not present, sigop count is unknown and clients MUST NOT assume there aren't any\n"
            "         \"required\" : true|false    (boolean) if provided and true, this transaction must be in the final block\n"
            "      }\n"
            "      ,...\n"
            "  ],\n"
            "  \"coinbaseaux\" : {                 (json object) data that should be included in the coinbase's scriptSig content\n"
            "      \"flags\" : \"flags\"             (string) \n"
            "  },\n"
            "  \"coinbasevalue\" : n,              (numeric) maximum allowable input to coinbase transaction, including the generation award and transaction fees (in Satoshis)\n"
            "  \"coinbasetxn\" : { ... },          (json object) information for coinbase transaction\n"
            "  \"target\" : \"xxxx\",                (string) The hash target\n"
            "  \"mintime\" : xxx,                  (numeric) The minimum timestamp appropriate for next block time in seconds since epoch (Jan 1 1970 GMT)\n"
            "  \"mutable\" : [                     (array of string) list of ways the block template may be changed \n"
            "     \"value\"                        (string) A way the block template may be changed, e.g. 'time', 'transactions', 'prevblock'\n"
            "     ,...\n"
            "  ],\n"
            "  \"noncerange\" : \"00000000ffffffff\",(string) A range of valid nonces\n"
            "  \"sigoplimit\" : n,                 (numeric) limit of sigops in blocks\n"
            "  \"sizelimit\" : n,                  (numeric) limit of block size\n"
            "  \"curtime\" : ttt,                  (numeric) current timestamp in seconds since epoch (Jan 1 1970 GMT)\n"
            "  \"bits\" : \"xxx\",                   (string) compressed target of next block\n"
            "  \"height\" : n                      (numeric) The height of the next block\n"
            "}\n"

            "\nExamples:\n"
            + HelpExampleCli("getblocktemplate", "")
            + HelpExampleRpc("getblocktemplate", "")
        ));
    
    mapHelpStrings.insert(std::make_pair("getmininginfo",
            "getmininginfo\n"
            "\nReturns a json object containing mining-related information."
            "\nResult:\n"
            "{\n"
            "  \"blocks\": nnn,                    (numeric) The current block\n"
            "  \"currentblocksize\": nnn,          (numeric) The last block size\n"
            "  \"currentblocktx\": nnn,            (numeric) The last block transaction\n"
            "  \"difficulty\": xxx.xxxxx           (numeric) The current difficulty\n"
            "  \"errors\": \"...\"                   (string) Current errors\n"
            "  \"generate\": true|false            (boolean) If the generation is on or off (see getgenerate or setgenerate calls)\n"
            "  \"genproclimit\": n                 (numeric) The processor limit for generation. -1 if no generation. (see getgenerate or setgenerate calls)\n"
            "  \"hashespersec\": n                 (numeric) The hashes per second of the generation, or 0 if no generation.\n"
            "  \"pooledtx\": n                     (numeric) The size of the mem pool\n"
            "  \"testnet\": true|false             (boolean) If using testnet or not\n"
            "  \"chain\": \"xxxx\",                  (string) current network name as defined in BIP70 (main, test, regtest)\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("getmininginfo", "")
            + HelpExampleRpc("getmininginfo", "")
        ));
    
    mapHelpStrings.insert(std::make_pair("getnetworkhashps",
            "getnetworkhashps ( blocks height )\n"
            "\nReturns the estimated network hashes per second based on the last n blocks.\n"
            "Pass in [blocks] to override # of blocks, -1 specifies since last difficulty change.\n"
            "Pass in [height] to estimate the network speed at the time when a certain block was found.\n"
            "\nArguments:\n"
            "1. blocks                           (numeric, optional, default=120) The number of blocks, or -1 for blocks since last difficulty change.\n"
            "2. height                           (numeric, optional, default=-1) To estimate at the time of the given height.\n"
            "\nResult:\n"
            "x                                   (numeric) Hashes per second estimated\n"
            "\nExamples:\n"
            + HelpExampleCli("getnetworkhashps", "")
            + HelpExampleRpc("getnetworkhashps", "")
        ));
    
    mapHelpStrings.insert(std::make_pair("prioritisetransaction",
            "prioritisetransaction txid priority-delta fee-delta\n"
            "Accepts the transaction into mined blocks at a higher (or lower) priority\n"
            "\nArguments:\n"
            "1. txid                             (string, required) The transaction id.\n"
            "2. priority-delta                   (numeric, required) The priority to add or subtract.\n"
            "                                    The transaction selection algorithm considers the tx as it would have a higher priority.\n"
            "                                    (priority of a transaction is calculated: coinage * value_in_satoshis / txsize) \n"
            "3. fee-delta                        (numeric, required) The fee value (in satoshis) to add (or subtract, if negative).\n"
            "                                    The fee is not actually paid, only the algorithm for selecting transactions into a block\n"
            "                                    considers the transaction as it would have paid a higher (or lower) fee.\n"
            "\nResult\n"
            "true                                (boolean) Returns true\n"
            "\nExamples:\n"
            + HelpExampleCli("prioritisetransaction", "\"txid\" 0.0 10000")
            + HelpExampleRpc("prioritisetransaction", "\"txid\", 0.0, 10000")
        ));
    
}

void mc_InitRPCHelpMap04()
{
    mapHelpStrings.insert(std::make_pair("submitblock",
            "submitblock hexdata ( \"jsonparametersobject\" )\n"
            "\nAttempts to submit new block to network.\n"
            "The 'jsonparametersobject' parameter is currently ignored.\n"
            "See https://en.bitcoin.it/wiki/BIP_0022 for full specification.\n"

            "\nArguments\n"
            "1. hexdata                          (string, required) the hex-encoded block data to submit\n"
            "2. \"jsonparametersobject\"           (string, optional) object of optional parameters\n"
            "    {\n"
            "      \"workid\" : \"id\"               (string, optional) if the server provided a workid, it MUST be included with submissions\n"
            "    }\n"
            "\nResult:\n"
            "\nExamples:\n"
            + HelpExampleCli("submitblock", "\"mydata\"")
            + HelpExampleRpc("submitblock", "\"mydata\"")
        ));
    
    mapHelpStrings.insert(std::make_pair("addnode",
            "addnode \"node\" \"add\"|\"remove\"|\"onetry\"\n"
            "\nAttempts add or remove a node from the addnode list.\n"
            "Or try a connection to a node once.\n"
            "\nArguments:\n"
            "1. \"node\"                           (string, required) The node (see getpeerinfo for nodes)\n"
            "2. \"command\"                        (string, required) 'add' to add a node to the list, 'remove' to remove a node from the list,\n"
            "                                                       'onetry' to try a connection to the node once\n"
            "\nExamples:\n"
            + HelpExampleCli("addnode", "\"192.168.0.6:8333\" \"onetry\"")
            + HelpExampleRpc("addnode", "\"192.168.0.6:8333\", \"onetry\"")
        ));
    
    mapHelpStrings.insert(std::make_pair("getaddednodeinfo",
            "getaddednodeinfo dns ( \"node\" )\n"
            "\nReturns information about the given added node, or all added nodes\n"
            "(note that onetry addnodes are not listed here)\n"
            "If dns is false, only a list of added nodes will be provided,\n"
            "otherwise connected information will also be available.\n"
            "\nArguments:\n"
            "1. dns                                      (boolean, required) If false, only a list of added nodes will be provided,\n "
             "                                                               otherwise connected information will also be available.\n"
            "2. \"node\"                                   (string, optional)  If provided, return information about this specific node,\n "
            "                                                                otherwise all nodes are returned.\n"
            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"addednode\" : \"192.168.0.201\",          (string) The node ip address\n"
            "    \"connected\" : true|false,               (boolean) If connected\n"
            "    \"addresses\" : [\n"
            "       {\n"
            "         \"address\" : \"192.168.0.201:8333\",  (string) The MultiChain server host and port\n"
            "         \"connected\" : \"outbound\"           (string) connection, inbound or outbound\n"
            "       }\n"
            "       ,...\n"
            "     ]\n"
            "  }\n"
            "  ,...\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("getaddednodeinfo", "true")
            + HelpExampleCli("getaddednodeinfo", "true \"192.168.0.201\"")
            + HelpExampleRpc("getaddednodeinfo", "true, \"192.168.0.201\"")
        ));
    
    mapHelpStrings.insert(std::make_pair("getconnectioncount",
            "getconnectioncount\n"
            "\nReturns the number of connections to other nodes.\n"
            "\nbResult:\n"
            "n                                   (numeric) The connection count\n"
            "\nExamples:\n"
            + HelpExampleCli("getconnectioncount", "")
            + HelpExampleRpc("getconnectioncount", "")
         ));
    
    mapHelpStrings.insert(std::make_pair("getnettotals",
            "getnettotals\n"
            "\nReturns information about network traffic, including bytes in, bytes out,\n"
            "and current time.\n"
            "\nResult:\n"
            "{\n"
            "  \"totalbytesrecv\": n,              (numeric) Total bytes received\n"
            "  \"totalbytessent\": n,              (numeric) Total bytes sent\n"
            "  \"timemillis\": t                   (numeric) Total cpu time\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("getnettotals", "")
            + HelpExampleRpc("getnettotals", "")
        ));
    
    mapHelpStrings.insert(std::make_pair("getnetworkinfo",
            "getnetworkinfo\n"
            "Returns an object containing various state info regarding P2P networking.\n"
            "\nResult:\n"
            "{\n"
            "  \"version\": xxxxx,                      (numeric) the server version\n"
            "  \"subversion\": \"/Satoshi:x.x.x/\",       (string) the server subversion string\n"
            "  \"protocolversion\": xxxxx,              (numeric) the protocol version\n"
            "  \"localservices\": \"xxxxxxxxxxxxxxxx\",   (string) the services we offer to the network\n"
            "  \"timeoffset\": xxxxx,                   (numeric) the time offset\n"
            "  \"connections\": xxxxx,                  (numeric) the number of connections\n"
            "  \"networks\": [                          (array) information per network\n"
            "  {\n"
            "    \"name\": \"xxx\",                       (string) network (ipv4, ipv6 or onion)\n"
            "    \"limited\": true|false,               (boolean) is the network limited using -onlynet?\n"
            "    \"reachable\": true|false,             (boolean) is the network reachable?\n"
            "    \"proxy\": \"host:port\"                 (string) the proxy that is used for this network, or empty if none\n"
            "  }\n"
            "  ,...\n"
            "  ],\n"
            "  \"relayfee\": x.xxxxxxxx,                (numeric) minimum relay fee for non-free transactions in btc/kb\n"
            "  \"localaddresses\": [                    (array) list of local addresses\n"
            "  {\n"
            "    \"address\": \"xxxx\",                   (string) network address\n"
            "    \"port\": xxx,                         (numeric) network port\n"
            "    \"score\": xxx                         (numeric) relative score\n"
            "  }\n"
            "  ,...\n"
            "  ]\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("getnetworkinfo", "")
            + HelpExampleRpc("getnetworkinfo", "")
        ));
    
    mapHelpStrings.insert(std::make_pair("getpeerinfo",
            "getpeerinfo\n"
            "\nReturns data about each connected network node as a json array of objects.\n"
            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"id\": n,                        (numeric) Peer index\n"
            "    \"addr\":\"host:port\",             (string) The ip address and port of the peer\n"
            "    \"addrlocal\":\"ip:port\",          (string) local address\n"
            "    \"services\":\"xxxxxxxxxxxxxxxx\",  (string) The services offered\n"
            "    \"lastsend\": ttt,                (numeric) The time in seconds since epoch (Jan 1 1970 GMT) of the last send\n"
            "    \"lastrecv\": ttt,                (numeric) The time in seconds since epoch (Jan 1 1970 GMT) of the last receive\n"
            "    \"bytessent\": n,                 (numeric) The total bytes sent\n"
            "    \"bytesrecv\": n,                 (numeric) The total bytes received\n"
            "    \"conntime\": ttt,                (numeric) The connection time in seconds since epoch (Jan 1 1970 GMT)\n"
            "    \"pingtime\": n,                  (numeric) ping time\n"
            "    \"pingwait\": n,                  (numeric) ping wait\n"
            "    \"version\": v,                   (numeric) The peer version, such as 7001\n"
            "    \"subver\": \"/Satoshi:0.8.5/\",    (string) The string version\n"
            "    \"handshakelocal\": n,            (string) If protocol is Multichain. Address used by local node for handshake.\n"
            "    \"handshake\": n,                 (string) If protocol is Multichain. Address used by remote node for handshake.\n"
            "    \"inbound\": true|false,          (boolean) Inbound (true) or Outbound (false)\n"
            "    \"startingheight\": n,            (numeric) The starting height (block) of the peer\n"
            "    \"banscore\": n,                  (numeric) The ban score\n"
            "    \"synced_headers\": n,            (numeric) The last header we have in common with this peer\n"
            "    \"synced_blocks\": n,             (numeric) The last block we have in common with this peer\n"
            "    \"inflight\": [\n"
            "       n,                           (numeric) The heights of blocks we're currently asking from this peer\n"
            "       ...\n"
            "    ]\n"
            "  }\n"
            "  ,...\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("getpeerinfo", "")
            + HelpExampleRpc("getpeerinfo", "")
        ));
    
    mapHelpStrings.insert(std::make_pair("ping",
            "ping\n"
            "\nRequests that a ping be sent to all other nodes, to measure ping time.\n"
            "Results provided in getpeerinfo, pingtime and pingwait fields are decimal seconds.\n"
            "Ping command is handled in queue with all other commands, so it measures processing backlog, not just network ping.\n"
            "\nResult:\n"
            "\nExamples:\n"
            + HelpExampleCli("ping", "")
            + HelpExampleRpc("ping", "")
        ));
    
    mapHelpStrings.insert(std::make_pair("appendrawchange",
            "appendrawchange \"tx-hex\" \"address\" ( native-fee )\n"
            "\nAppends change output to raw transaction, containing any remaining assets / \n"
            "native currency in the inputs that are not already sent to other outputs.\n"
            "\nArguments:\n"
            "1. \"tx-hex\"                         (string, required) The hex string of the raw transaction)\n"
            "2. \"address\"                        (string, required) The address to send the change to.\n"
            "3. native-fee                       (numeric, optional) Native currency value deducted from that amount so it becomes a transaction fee.\n"
            "                                                        Default - calculated automatically\n"
            "\nResult:\n"
            "\"transaction\"                       (string) hex string of the transaction\n"
            "\nExamples:\n"
            + HelpExampleCli("appendrawchange", "\"hex\"" "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" ")
            + HelpExampleCli("appendrawchange", "\"hex\"" "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" 0.01")
            + HelpExampleRpc("appendrawchange", "\"hex\" \"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\"")
        ));
    
    mapHelpStrings.insert(std::make_pair("appendrawmetadata",
            "appendrawmetadata \"tx-hex\" data \n"
            "\nAppends new OP_RETURN output to existing raw transaction\n"
            "Returns hex-encoded raw transaction.\n"
            "\nArguments:\n"
            "1. \"tx-hex\"                           (string, required) The transaction hex string\n"
            "2. data                               (string or object, required) Data, see help data-all for details.\n"
            "\nResult:\n"
            "\"transaction\"                         (string) hex string of the transaction\n"
            "\nExamples:\n"
            + HelpExampleCli("appendrawmetadata", "\"tx-hexstring\" 48656C6C6F20576F726C64210A" )
            + HelpExampleRpc("appendrawmetadata", "\"tx-hexstring\",\"48656C6C6F20576F726C64210A\"")
        ));
    
}

void mc_InitRPCHelpMap05()
{
    mapHelpStrings.insert(std::make_pair("appendrawdata",
            "appendrawdata tx-hex data \n"
            "\nAppends new OP_RETURN output to existing raw transaction\n"
            "Returns hex-encoded raw transaction.\n"
            "\nArguments:\n"
            "1. \"tx-hex\"                           (string, required) The transaction hex string\n"
            "2. data                               (string or object, required) Data, see help data-all for details.\n"
/*    
            "2. \"data-hex\"                         (string, required) Data hex string\n"
            " or\n"
            "2. data-json                          (object, required) JSON data object\n"
            "    {\n"
            "      \"json\" : json-data              (object, required) Valid JSON object\n" 
            "    }\n"                                
            " or\n"
            "2. data-text                          (object, required) Text data object\n"
            "    {\n"
            "      \"text\" : text                   (string, required) Data string\n" 
            "    }\n"                                
            " or\n"
            "2. issue-details                      (object, required) A json object with issue metadata\n"
            "    {\n"
            "      \"create\" : asset                (string,required) asset\n" 
            "      \"name\" : asset-name             (string,optional) Asset name\n"
            "      \"multiple\" : n                  (numeric,optional, default 1) Number of raw units in one displayed unit\n"
            "      \"open\" : true|false             (boolean, optional, default false) True if follow-on issues are allowed\n"                
            "      \"details\" :                     (object, optional)  a json object with custom fields\n"           
            "        {\n"
            "          \"param-name\": \"param-value\" (strings, required) The key is the parameter name, the value is parameter value\n"
            "          ,...\n"
            "        }\n"
            "    }\n"                                
            " or\n"
            "2. issuemore-details                  (object, required) A json object with issuemore metadata\n"
            "    {\n"
            "      \"update\" : asset-identifier     (string,required) Asset identifier - one of: issue txid, asset reference, asset name.\n"
            "      \"details\" :                     (object, optional)  a json object with custom fields\n"           
            "        {\n"
            "          \"param-name\": \"param-value\" (strings, required) The key is the parameter name, the value is parameter value\n"
            "          ,...\n"
            "        }\n"
            "    }\n"                                
            " or\n"
            "2. create-new-stream                  (object, required) A json object with new stream details\n"
            "    {\n"                
            "      \"create\" : stream               (string,required) stream\n"
            "      \"name\" : stream-name            (string,optional) Stream name\n"
            "      \"open\" : true|false             (string,optional, default: false) If true, anyone can publish\n"
            "      \"details\" :                     (object,optional) a json object with custom fields\n"           
            "        {\n"
            "          \"param-name\": \"param-value\" (strings, required) The key is the parameter name, the value is parameter value\n"
            "          ,...\n"
            "        }\n"
            "    }\n"                                
            " or\n"
            "2. publish-new-stream-item            (object, required) A json object with stream item\n"
            "    {\n"                
            "      \"for\" : stream-identifier       (string,required) Stream identifier - one of: create txid, stream reference, stream name.\n"
            "      \"key\" : key                     (string,optional, default: \"\") Item key\n"
            "      \"keys\" : keys                   (array,optional) Item keys, array of strings\n"
            "      \"data\" : data-hex               (string,optional, default: \"\") Data hex string\n"
            "        or\n"
            "      \"data\" :                        (object, required) JSON data object\n"
            "        {\n"
            "          \"json\" : json-data          (object, required) Valid JSON string\n" 
            "        }\n"                                
            "        or\n"
            "      \"data\" :                        (object, required) JSON data object\n"
            "        {\n"
            "          \"text\" : \"text\"             (string, required) Data string\n" 
            "        }\n"                                
            "    }\n"                                
            " or\n"
            "2. create-new-upgrade                 (object, required) A json object with new upgrade details\n"
            "    {\n"                
            "      \"create\" : upgrade              (string,required) upgrade\n"
            "      \"name\" : upgrade-name           (string,optional) Upgrade name\n"
            "      \"startblock\" : n                (numeric,optional, default: 0) Block to apply upgrade from (inclusive).\n"
            "      \"details\" :                     (object,optional) a json object with custom fields\n"           
            "        {\n"
            "          \"protocol-version\": version (numeric, required) Protocol version to upgrade to \n"
            "        }\n"
            "    }\n"                                
            " or\n"
            "2. approve-upgrade                    (object, required) A json object with approval details\n"
            "    {\n"                
            "      \"approve\" : approve             (boolean,required) Approve or disapprove\n"
            "      \"for\" : upgrade-identifier      (string,required)  Upgrade identifier - one of: create txid, upgrade name.\n"
            "    }\n"                                
 */ 
            "\nResult:\n"
            "\"transaction\"                         (string) hex string of the transaction\n"
            "\nExamples:\n"
            + HelpExampleCli("appendrawdata", "\"tx-hexstring\" 48656C6C6F20576F726C64210A" )
            + HelpExampleRpc("appendrawdata", "\"tx-hexstring\",\"48656C6C6F20576F726C64210A\"")
        ));
    
    mapHelpStrings.insert(std::make_pair("createrawtransaction",
            "createrawtransaction [{\"txid\":\"id\",\"vout\":n},...] {\"address\":amount,...} ( [data] \"action\" ) \n"
            "\nCreate a transaction spending the given inputs.\n"

            "\nArguments:\n"
            "1. transactions                           (array, required) A json array of json objects\n"
            "     [\n"
            "       {\n"
            "         \"txid\":\"id\",                     (string, required) The transaction id\n"
            "         \"vout\":n                         (numeric, required) The output number\n"
            "         \"scriptPubKey\": \"hex\",           (string, optional) script key, used if cache=true or action=sign\n"
            "         \"redeemScript\": \"hex\"            (string, optional) redeem script, used if action=sign\n"
            "         \"cache\":true|false               (boolean, optional) If true - add cached script to tx, if omitted - add automatically if needed\n"    
            "       }\n"
            "       ,...\n"
            "     ]\n"
            "2. addresses                              (object, required) Object with addresses as keys, see help addresses-all for details.\n"
/*    
            "2. addresses                              (object, required) a json object with addresses as keys and amounts as values\n"
            "    {\n"
            "      \"address\": \n"
            "        x.xxx                             (numeric, required) The key is the address, the value is the native currency amount\n"
            "          or \n"
            "        {                                 (object) A json object of assets to send\n"
            "          \"asset-identifier\" : asset-quantity \n"
            "          ,...\n"
            "        }\n"                                
            "          or \n"
            "        {                                 (object) A json object describing new asset issue\n"
            "          \"issue\" : \n"
            "            {\n"
            "              \"raw\" : n                   (numeric, required) The asset total amount in raw units \n"
            "              ,...\n"
            "            }\n"                                
            "          ,...\n"
            "        }\n"                                
            "          or \n"
            "        {                                 (object) A json object describing follow-on asset issue\n"
            "          \"issuemore\" : \n"
            "            {\n"
            "              \"asset\" : \"asset-identifier\"(string, required) Asset identifier - one of: issue txid. asset reference, asset name.\n"
            "              \"raw\" : n                   (numeric, required) The asset total amount in raw units \n"
            "              ,...\n"
            "            }\n"                                
            "          ,...\n"
            "        }\n"                                
            "          or \n"
            "        {                                 (object) A json object describing permission change\n"
            "          \"permissions\" : \n"
            "            {\n"
            "              \"type\" : \"permission(s)\"    (string,required) Permission strings, comma delimited. Possible values:\n"
            "                                                              " + AllowedPermissions() + " \n"
            "              \"startblock\" : n            (numeric, optional) Block to apply permissions from (inclusive). Default - 0\n"
            "              \"endblock\"  : n             (numeric, optional) Block to apply permissions to (exclusive). Default - 4294967295\n"
            "              \"timestamp\" : n             (numeric, optional) This helps resolve conflicts between\n"
            "                                                                permissions assigned by the same administrator. Default - current time\n"
            "              ,...\n"
            "            }\n"                                
            "          ,...\n"
            "        }\n"                                
            "      ,...\n"
            "    }\n"
 */ 
            "3. data                                   (array, optional) Array of hexadecimal strings or data objects, see help data-all for details.\n"
            "4. \"action\"                               (string, optional, default \"\") Additional actions: \"lock\", \"sign\", \"lock,sign\", \"sign,lock\", \"send\". \n"
                

            "\nResult:\n"
            "\"transaction\"                             (string) hex string of the transaction (if action= \"\" or \"lock\")\n"
            "  or \n"
            "{                                         (object) A json object (if action= \"sign\" or \"lock,sign\" or \"sign,lock\")\n"
            "  \"hex\": \"value\",                         (string) The raw transaction with signature(s) (hex-encoded string)\n"
            "  \"complete\": true|false                  (boolean) if transaction has a complete set of signature (0 if not)\n"
            "}\n"
            "  or \n"
            "\"hex\"                                     (string) The transaction hash in hex (if action= \"send\")\n"

            "\nExamples\n"
            + HelpExampleCli("createrawtransaction", "\"[{\\\"txid\\\":\\\"myid\\\",\\\"vout\\\":0}]\" \"{\\\"address\\\":0.01}\"")
            + HelpExampleRpc("createrawtransaction", "\"[{\\\"txid\\\":\\\"myid\\\",\\\"vout\\\":0}]\", \"{\\\"address\\\":0.01}\"")
        ));
    
    mapHelpStrings.insert(std::make_pair("decoderawtransaction",
            "decoderawtransaction \"tx-hex\"\n"
            "\nReturn a JSON object representing the serialized, hex-encoded transaction.\n"

            "\nArguments:\n"
            "1. \"tx-hex\"                                     (string, required) The transaction hex string\n"

            "\nResult:\n"
            "{\n"
            "  \"txid\" : \"id\",                                (string) The transaction id\n"
            "  \"version\" : n,                                (numeric) The version\n"
            "  \"locktime\" : ttt,                             (numeric) The lock time\n"
            "  \"vin\" : [                                     (array of json objects)\n"
            "     {\n"
            "       \"txid\": \"id\",                            (string) The transaction id\n"
            "       \"vout\": n,                               (numeric) The output number\n"
            "       \"scriptSig\": {                           (json object) The script\n"
            "         \"asm\": \"asm\",                          (string) asm\n"
            "         \"hex\": \"hex\"                           (string) hex\n"
            "       },\n"
            "       \"sequence\": n                            (numeric) The script sequence number\n"
            "     }\n"
            "     ,...\n"
            "  ],\n"
            "  \"vout\" : [                                    (array of json objects)\n"
            "     {\n"
            "       \"value\" : x.xxx,                         (numeric) The value in btc\n"
            "       \"n\" : n,                                 (numeric) index\n"
            "       \"scriptPubKey\" : {                       (json object)\n"
            "         \"asm\" : \"asm\",                         (string) the asm\n"
            "         \"hex\" : \"hex\",                         (string) the hex\n"
            "         \"reqSigs\" : n,                         (numeric) The required sigs\n"
            "         \"type\" : \"pubkeyhash\",                 (string) The type, eg 'pubkeyhash'\n"
            "         \"addresses\" : [                        (json array of string)\n"
            "           \"12tvKAXCxZjSmdNbao16dKXC8tRWfcF5oc\" (string) address\n"
            "           ,...\n"
            "         ]\n"
            "       }\n"
            "     }\n"
            "     ,...\n"
            "  ],\n"
            "}\n"

            "\nExamples:\n"
            + HelpExampleCli("decoderawtransaction", "\"hexstring\"")
            + HelpExampleRpc("decoderawtransaction", "\"hexstring\"")
        ));
    
    mapHelpStrings.insert(std::make_pair("decodescript",
            "decodescript script-hex\n"
            "\nDecode a hex-encoded script.\n"
            "\nArguments:\n"
            "1. script-hex                       (string) the hex encoded script\n"
            "\nResult:\n"
            "{\n"
            "  \"asm\":\"asm\",                      (string) Script public key\n"
            "  \"hex\":\"hex\",                      (string) hex encoded public key\n"
            "  \"type\":\"type\",                    (string) The output type\n"
            "  \"reqSigs\": n,                     (numeric) The required signatures\n"
            "  \"addresses\": [                    (json array of string)\n"
            "     \"address\"                      (string) address\n"
            "     ,...\n"
            "  ],\n"
            "  \"p2sh\",\"address\"                  (string) script address\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("decodescript", "\"hexstring\"")
            + HelpExampleRpc("decodescript", "\"hexstring\"")
        ));
    
    mapHelpStrings.insert(std::make_pair("getrawtransaction",
            "getrawtransaction \"txid\" ( verbose )\n"
            "\nNOTE: By default this function only works sometimes. This is when the tx is in the mempool\n"
            "or there is an unspent output in the utxo for this transaction. To make it always work,\n"
            "you need to maintain a transaction index, using the -txindex command line option.\n"
            "\nReturn the raw transaction data.\n"
            "\nIf verbose=0, returns a string that is serialized, hex-encoded data for 'txid'.\n"
            "If verbose is non-zero, returns an Object with information about 'txid'.\n"

            "\nArguments:\n"
            "1. \"txid\"                           (string, required) The transaction id\n"
            "2. verbose                          (numeric or boolean, optional, default=0(false)) If 0, return a string, other return a json object\n"

            "\nResult (if verbose is not set or set to 0):\n"
            "\"data\"                              (string) The serialized, hex-encoded data for 'txid'\n"

            "\nResult (if verbose > 0):\n"
            "{\n"
            "  \"hex\" : \"data\",                   (string) The serialized, hex-encoded data for 'txid'\n"
            "  \"txid\" : \"id\",                    (string) The transaction id (same as provided)\n"
            "  \"version\" : n,                    (numeric) The version\n"
            "  \"locktime\" : ttt,                 (numeric) The lock time\n"
            "  \"vin\" : [                         (array of json objects)\n"
            "     {\n"
            "       \"txid\": \"id\",                (string) The transaction id\n"
            "       \"vout\": n,                   (numeric) \n"
            "       \"scriptSig\": {               (json object) The script\n"
            "         \"asm\": \"asm\",              (string) asm\n"
            "         \"hex\": \"hex\"               (string) hex\n"
            "       },\n"
            "       \"sequence\": n                (numeric) The script sequence number\n"
            "     }\n"
            "     ,...\n"
            "  ],\n"
            "  \"vout\" : [                        (array of json objects)\n"
            "     {\n"
            "       \"value\" : x.xxx,             (numeric) The value in btc\n"
            "       \"n\" : n,                     (numeric) index\n"
            "       \"scriptPubKey\" : {           (json object)\n"
            "         \"asm\" : \"asm\",             (string) the asm\n"
            "         \"hex\" : \"hex\",             (string) the hex\n"
            "         \"reqSigs\" : n,             (numeric) The required sigs\n"
            "         \"type\" : \"pubkeyhash\",     (string) The type, eg 'pubkeyhash'\n"
            "         \"addresses\" : [            (json array of string)\n"
            "           \"address\"                (string) address\n"
            "           ,...\n"
            "         ]\n"
            "       }\n"
            "     }\n"
            "     ,...\n"
            "  ],\n"
            "  \"blockhash\" : \"hash\",             (string) the block hash\n"
            "  \"confirmations\" : n,              (numeric) The confirmations\n"
            "  \"time\" : ttt,                     (numeric) The transaction time in seconds since epoch (Jan 1 1970 GMT)\n"
            "  \"blocktime\" : ttt                 (numeric) The block time in seconds since epoch (Jan 1 1970 GMT)\n"
            "}\n"

            "\nExamples:\n"
            + HelpExampleCli("getrawtransaction", "\"mytxid\"")
            + HelpExampleCli("getrawtransaction", "\"mytxid\" 1")
            + HelpExampleRpc("getrawtransaction", "\"mytxid\", 1")
        ));
    
    mapHelpStrings.insert(std::make_pair("sendrawtransaction",
            "sendrawtransaction \"tx-hex\" ( allowhighfees )\n"
            "\nSubmits raw transaction (serialized, hex-encoded) to local node and network.\n"
            "\nAlso see createrawtransaction and signrawtransaction calls.\n"
            "\nArguments:\n"
            "1. \"tx-hex\"                         (string, required) The hex string of the raw transaction)\n"
            "2. allowhighfees                    (boolean, optional, default=false) Allow high fees\n"
            "\nResult:\n"
            "\"hex\"                               (string) The transaction hash in hex\n"
            "\nExamples:\n"
            "\nCreate a transaction\n"
            + HelpExampleCli("createrawtransaction", "\"[{\\\"txid\\\" : \\\"mytxid\\\",\\\"vout\\\":0}]\" \"{\\\"myaddress\\\":0.01}\"") +
            "Sign the transaction, and get back the hex\n"
            + HelpExampleCli("sendrawtransaction", "\"myhex\"") +
            "\nSend the transaction (signed hex)\n"
            + HelpExampleCli("sendrawtransaction", "\"signedhex\"") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("sendrawtransaction", "\"signedhex\"")
        ));
    
    mapHelpStrings.insert(std::make_pair("signrawtransaction",
            "signrawtransaction \"tx-hex\" ( [{\"txid\":\"id\",\"vout\":n,\"scriptPubKey\":\"hex\",\"redeemScript\":\"hex\"},...] [\"privatekey1\",...] sighashtype )\n"
            "\nSign inputs for raw transaction (serialized, hex-encoded).\n"
            "The second optional argument (may be null) is an array of previous transaction outputs that\n"
            "this transaction depends on but may not yet be in the block chain.\n"
            "The third optional argument (may be null) is an array of base58-encoded private\n"
            "keys that, if given, will be the only keys used to sign the transaction.\n"
            + HelpRequiringPassphraseWrapper() +
            "\nArguments:\n"
            "1. \"tx-hex\"                         (string, required) The transaction hex string\n"
            "2. prevtxs                          (array, optional) An json array of previous dependent transaction outputs\n"
            "     [                              (json array of json objects, or 'null' if none provided)\n"
            "       {\n"
            "         \"txid\":\"id\",               (string, required) The transaction id\n"
            "         \"vout\":n,                  (numeric, required) The output number\n"
            "         \"scriptPubKey\": \"hex\",     (string, required) script key\n"
            "         \"redeemScript\": \"hex\"      (string, required for P2SH) redeem script\n"
            "       }\n"
            "       ,...\n"
            "    ]\n"
            "3.privatekeys                       (array, optional) A json array of base58-encoded private keys for signing\n"
            "    [                               (json array of strings, or 'null' if none provided)\n"
            "      \"privatekey\"                  (string) private key in base58-encoding\n"
            "      ,...\n"
            "    ]\n"
            "4. \"sighashtype\"                    (string, optional, default=ALL) The signature hash type. Must be one of\n"
            "       \"ALL\"\n"
            "       \"NONE\"\n"
            "       \"SINGLE\"\n"
            "       \"ALL|ANYONECANPAY\"\n"
            "       \"NONE|ANYONECANPAY\"\n"
            "       \"SINGLE|ANYONECANPAY\"\n"

            "\nResult:\n"
            "{\n"
            "  \"hex\": \"value\",                   (string) The raw transaction with signature(s) (hex-encoded string)\n"
            "  \"complete\": true|false            (boolean) if transaction has a complete set of signature (0 if not)\n"
            "}\n"

            "\nExamples:\n"
            + HelpExampleCli("signrawtransaction", "\"myhex\"")
            + HelpExampleRpc("signrawtransaction", "\"myhex\"")
        ));
    
    mapHelpStrings.insert(std::make_pair("createkeypairs",
            "createkeypairs ( count )\n"
            "\nCreates public/private key pairs. These key pairs are not stored in the wallet.\n"
            "\nArguments: \n"
            "1. count                            (number, optional, default=1) Number of pairs to create.\n"
            "\nResult:\n"
            "[                                   (json array of )\n"
            "   {\n"
            "      \"address\" : \"address\",        (string) Pay-to-pubkeyhash address\n"
            "      \"pubkey\"  : \"pubkey\",         (string) Public key (hexadecimal)\n"
            "      \"privkey\" : \"privatekey\",     (string) Private key, base58-encoded as required for signrawtransaction\n"
            "  }\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("createkeypairs", "")
            + HelpExampleRpc("createkeypairs", "")
        ));
    
    mapHelpStrings.insert(std::make_pair("createmultisig",
            "createmultisig nrequired keys\n"
            "\nCreates a multi-signature address with n signature of m keys required.\n"
            "It returns a json object with the address and redeemScript.\n"

            "\nArguments:\n"
            "1. nrequired                        (numeric, required) The number of required signatures out of the n keys or addresses.\n"
            "2. keys                             (array, required) A json array of keys which are addresses or hex-encoded public keys\n"
            "     [\n"
            "       \"key\"                        (string) address or hex-encoded public key\n"
            "       ,...\n"
            "     ]\n"

            "\nResult:\n"
            "{\n"
            "  \"address\":\"multisigaddress\",      (string) The value of the new multisig address.\n"
            "  \"redeemScript\":\"script\"           (string) The string value of the hex-encoded redemption script.\n"
            "}\n"

            "\nExamples:\n"
            "\nCreate a multisig address from 2 addresses\n"
            + HelpExampleCli("createmultisig", "2 \"[\\\"16sSauSf5pF2UkUwvKGq4qjNRzBZYqgEL5\\\",\\\"171sgjn4YtPu27adkKGrdDwzRTxnRkBfKV\\\"]\"") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("createmultisig", "2, \"[\\\"16sSauSf5pF2UkUwvKGq4qjNRzBZYqgEL5\\\",\\\"171sgjn4YtPu27adkKGrdDwzRTxnRkBfKV\\\"]\"")
        ));
    
    mapHelpStrings.insert(std::make_pair("estimatefee",
            "estimatefee nblocks\n"
            "\nEstimates the approximate fee per kilobyte\n"
            "needed for a transaction to begin confirmation\n"
            "within nblocks blocks.\n"
            "\nArguments:\n"
            "1. nblocks                          (numeric)\n"
            "\nResult:\n"
            "n :                                 (numeric) estimated fee-per-kilobyte\n"
            "\n"
            "-1.0 is returned if not enough transactions and\n"
            "blocks have been observed to make an estimate.\n"
            "\nExample:\n"
            + HelpExampleCli("estimatefee", "6")
        ));
    
}

void mc_InitRPCHelpMap06()
{
    mapHelpStrings.insert(std::make_pair("estimatepriority",
            "estimatepriority nblocks\n"
            "\nEstimates the approximate priority\n"
            "a zero-fee transaction needs to begin confirmation\n"
            "within nblocks blocks.\n"
            "\nArguments:\n"
            "1. nblocks                          (numeric)\n"
            "\nResult:\n"
            "n :                                 (numeric) estimated priority\n"
            "\n"
            "-1.0 is returned if not enough transactions and\n"
            "blocks have been observed to make an estimate.\n"
            "\nExample:\n"
            + HelpExampleCli("estimatepriority", "6")
        ));
    
    mapHelpStrings.insert(std::make_pair("validateaddress",
            "validateaddress \"address\"|\"pubkey\"|\"privkey\"\n"
            "\nReturn information about the given address or public key or private key.\n"
            "\nArguments:\n"
            "1. \"address\"                        (string, required) The address to validate\n"
            "  or \n"
            "1. \"pubkey\"                         (string, required) The public key (hexadecimal) to validate\n"
            "  or \n"
            "1. \"privkey\"                        (string, required) The private key (see dumpprivkey) to validate\n"
            "\nResult:\n"
            "{\n"
            "  \"isvalid\" : true|false,           (boolean) If the address is valid or not. If not, this is the only property returned.\n"
            "  \"address\" : \"address\",            (string) The address validated\n"
            "  \"ismine\" : true|false,            (boolean) If the address is yours or not\n"
            "  \"isscript\" : true|false,          (boolean) If the key is a script\n"
            "  \"pubkey\" : \"publickeyhex\",        (string) The hex value of the raw public key\n"
            "  \"iscompressed\" : true|false,      (boolean) If the address is compressed\n"
            "  \"account\" : \"account\"             (string) The account associated with the address, \"\" is the default account\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("validateaddress", "\"1PSSGeFHDnKNxiEyFrD1wcEaHr9hrQDDWc\"")
            + HelpExampleRpc("validateaddress", "\"1PSSGeFHDnKNxiEyFrD1wcEaHr9hrQDDWc\"")
        ));
    
    mapHelpStrings.insert(std::make_pair("verifymessage",
            "verifymessage \"address\" \"signature\" \"message\"\n"
            "\nVerify a signed message\n"
            "\nArguments:\n"
            "1. \"address\"                        (string, required) The address to use for the signature.\n"
            "2. \"signature\"                      (string, required) The signature provided by the signer in base 64 encoding (see signmessage).\n"
            "3. \"message\"                        (string, required) The message that was signed.\n"
            "\nResult:\n"
            "true|false                          (boolean) If the signature is verified or not.\n"
            "\nExamples:\n"
            "\nUnlock the wallet for 30 seconds\n"
            + HelpExampleCli("walletpassphrase", "\"mypassphrase\" 30") +
            "\nCreate the signature\n"
            + HelpExampleCli("signmessage", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XZ\" \"my message\"") +
            "\nVerify the signature\n"
            + HelpExampleCli("verifymessage", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XZ\" \"signature\" \"my message\"") +
            "\nAs json rpc\n"
            + HelpExampleRpc("verifymessage", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XZ\", \"signature\", \"my message\"")
        ));
    
    mapHelpStrings.insert(std::make_pair("addmultisigaddress",
            "addmultisigaddress nrequired keys ( \"account\" )\n"
            "\nAdd a nrequired-to-sign multisignature address to the wallet.\n"
            "Each key is a address or hex-encoded public key.\n"
            "If 'account' is specified, assign address to that account.\n"

            "\nArguments:\n"
            "1. nrequired                        (numeric, required) The number of required signatures out of the n keys or addresses.\n"
            "2. keys                             (array, required) A json array of addresses or hex-encoded public keys\n"
            "     [\n"
            "       \"address\"                    (string) address or hex-encoded public key\n"
            "       ...,\n"
            "     ]\n"
            "3. \"account\"                        (string, optional) An account to assign the addresses to.\n"

            "\nResult:\n"
            "\"address\"                           (string) A address associated with the keys.\n"

            "\nExamples:\n"
            "\nAdd a multisig address from 2 addresses\n"
            + HelpExampleCli("addmultisigaddress", "2 \"[\\\"16sSauSf5pF2UkUwvKGq4qjNRzBZYqgEL5\\\",\\\"171sgjn4YtPu27adkKGrdDwzRTxnRkBfKV\\\"]\"") +
            "\nAs json rpc call\n"
            + HelpExampleRpc("addmultisigaddress", "2, \"[\\\"16sSauSf5pF2UkUwvKGq4qjNRzBZYqgEL5\\\",\\\"171sgjn4YtPu27adkKGrdDwzRTxnRkBfKV\\\"]\"")
        ));
    
    mapHelpStrings.insert(std::make_pair("appendrawexchange",
            "appendrawexchange \"hex\" \"txid\" vout ask-assets \n"
            "\nAdds to the raw atomic exchange transaction in tx-hex given by a previous call to createrawexchange or appendrawexchange. \n"
            + HelpRequiringPassphraseWrapper() +
            "\nArguments:\n"
            "1. \"hex\"                            (string, required) The transaction hex string\n"
            "2. \"txid\"                           (string, required) Transaction ID of the output prepared by preparelockunspent.\n"
            "3. vout                             (numeric, required) Output index\n"
            "4. ask-assets                       (object, required) A json object of assets to ask\n"
            "    {\n"
            "      \"asset-identifier\" : asset-quantity\n"
            "      ,...\n"
            "    }\n"                
            "\nResult:\n"
            "{\n"
            "  \"hex\": \"value\",                   (string) The raw transaction with signature(s) (hex-encoded string)\n"
            "  \"complete\": true|false            (boolean) if exchange is completed and can be sent \n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("appendrawexchange", "\"hexstring\" f4c3dd510dd55761015c9d96bff7793b0d501dd6f01a959fd7dd02478fb47dfb 1 \"{\\\"1234-5678-1234\\\":200}\"" )
            + HelpExampleRpc("appendrawexchange", "\"hexstring\",\"f4c3dd510dd55761015c9d96bff7793b0d501dd6f01a959fd7dd02478fb47dfb\",1,\"{\\\"1234-5678-1234\\\":200}\\\"")
        ));
    
    mapHelpStrings.insert(std::make_pair("backupwallet",
            "backupwallet \"destination\"\n"
            "\nSafely copies wallet.dat to destination, which can be a directory or a path with filename.\n"
            "\nArguments:\n"
            "1. \"destination\"                  (string) The destination directory or file\n"
            "\nExamples:\n"
            + HelpExampleCli("backupwallet", "\"backup.dat\"")
            + HelpExampleRpc("backupwallet", "\"backup.dat\"")
        ));
    
    mapHelpStrings.insert(std::make_pair("combineunspent",
            "combineunspent ( \"address(es)\" minconf maxcombines mininputs maxinputs maxtime )\n"
            "\nOptimizes wallet performance by combining unspent txouts.\n"
            + HelpRequiringPassphraseWrapper() +
            "\nArguments:\n"
            "1. \"address(es)\"                    (string, optional) Addresses to optimize (comma delimited). Default - \"*\", all.\n"
            "2. minconf                          (numeric, optional) The minimum confirmations to filter. Default - 1\n"
            "3. maxcombines                      (numeric, optional) Maximal number of transactions to send. Default - 100\n"
            "4. mininputs                        (numeric, optional) Minimal number of txouts to combine in one transaction. Default - 2\n"
            "5. maxinputs                        (numeric, optional) Maximal number of txouts to combine in one transaction. Default - 100\n"
            "6. maxtime                          (numeric, optional) Maximal time for creating combining transactions, at least one transaction will be sent. Default - 15s\n"
            "\nResult:\n"
            "\"transactionids\"                    (array) Array of transaction ids.\n"
            "\nExamples:\n"
            + HelpExampleCli("combineunspent", "\"*\" 1 100 5 20 120")
            + HelpExampleCli("combineunspent", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" ")
            + HelpExampleRpc("combineunspent", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\",1,100, 5, 20, 120" )
        ));
    
    mapHelpStrings.insert(std::make_pair("create",
            "create \"entity-type\" \"entity-name\" open ( custom-fields )\n"
            "\nCreates stream, upgrade, filter or variable\n"
            + HelpRequiringPassphraseWrapper() +
            "\nArguments:\n"
            "1. \"entity-type\"                    (string, required) stream\n"
            "2. \"stream-name\"                    (string, required) Stream name, if not \"\" should be unique.\n"
            "3. open                             (boolean, required) Allow anyone to publish in this stream\n"
            "  or \n"
            "3. restrictions                     (object, optional) Stream restrictions\n"
            "    {\n"
            "      \"restrict\" : \"restrictions\"   (string, optional) Stream restrictions, comma delimited. Possible values: write,read,offchain,onchain\n"
            "      \"salted\" : true|false         (boolean, optional) Indicates whether offchain item chunk hashes should be salted\n"
            "    }\n"
            "4  custom-fields                    (object, optional)  a json object with custom fields\n"
            "    {\n"
            "      \"param-name\": \"param-value\"   (strings, required) The key is the parameter name, the value is parameter value\n"
            "      ,...\n"
            "    }\n"
            "  or \n"
            "1. \"entity-type\"                    (string, required) upgrade\n"
            "2. \"upgrade-name\"                   (string, required) Upgrade name, if not \"\" should be unique.\n"
            "3. open                             (boolean, required) Should be false\n"
            "4. custom-fields                    (object, required) A json object with custom fields\n"
            "    {\n"
            "      \"protocol-version\": version   (numeric, optional) Protocol version to upgrade to\n"
            "      \"parameter-name\": value       (numeric, optional) New value for upgradable parameter, one of: \n"
            "                                                        target-block-time,\n"
            "                                                        maximum-block-size,\n"
            "                                                        max-std-tx-size,\n"
            "                                                        max-std-op-returns-count,\n"
            "                                                        max-std-op-return-size,\n"
            "                                                        max-std-op-drops-count,\n"
            "                                                        max-std-element-size,\n"
            "                                                        anyone-can-connect,\n"
            "                                                        anyone-can-send,\n"
            "                                                        anyone-can-receive,\n"
            "                                                        anyone-can-receiveempty,\n"
            "                                                        anyone-can-issue,\n"
            "                                                        anyone-can-create,\n"
            "                                                        anyone-can-activate\n"
            "      \"startblock\": block           (numeric, optional, default 0) Block to apply from \n"
//            "      \"param-name\": \"param-value\"   (strings, required) The key is the parameter name, the value is parameter value\n"
            "      ,...\n"
            "    }\n"
            "  or \n"
            "1. \"entity-type\"                    (string, required) txfilter\n"
            "2. \"txfilter-name\"                  (string, required) Tx filter name, if not \"\" should be unique.\n"
            "3. restrictions                     (object, required) A json object with optional filter restrictions\n"
            "    {\n"
            "      \"for\": \"entity-identifier\"    (string, optional) Asset/stream identifier - one of: create txid, stream reference, stream name.\n"
            "        or\n"
            "      \"for\": entity-identifier(s)   (array, optional) A json array of asset/stream identifiers.\n"    
            "    }\n"
            "4. \"javascript-code\"                (string, required) JavaScript filter code, see help filters. Example:\n"
            "                                                       function filtertransaction()\n"
            "                                                       {\n"
            "                                                           var tx=getfiltertransaction();\n"
            "                                                                                        \n"
            "                                                           if (tx.vout.length<1)\n"
            "                                                               return \"One output required\";\n"
            "                                                       }\n"
            "  or \n"
            "1. \"entity-type\"                    (string, required) streamfilter\n"
            "2. \"streamfilter-name\"              (string, required) Stream filter name, if not \"\" should be unique.\n"
            "3. restrictions                     (object, required) A json object with filter restrictions\n"
            "    {\n"
            "    }\n"
            "4. \"javascript-code\"                (string, required) JavaScript filter code, see help filters. Example:\n"
            "                                                       function filterstreamitem()\n"
            "                                                       {\n"
            "                                                           var item=getfilterstreamitem();\n"
            "                                                                                        \n"
            "                                                           if (item.keys.length<2)\n"
            "                                                               return \"At least two keys required\";\n"
            "                                                       }   \n"
/*    
            "  or \n"
            "1. entity-type                      (string, required) variable\n"
            "2. \"variable-name\"                  (string, required) Variable name, if not \"\" should be unique.\n"
            "3. open                             (boolean, optional) Should be false\n"
            "4. value                            (any data, optional, default null) Variable value (JSON objects and arrays allowed).\n"
*/

            "\nResult:\n"
            "\"transactionid\"  (string) The transaction id.\n"
            "\nExamples:\n"
            + HelpExampleCli("create", "stream test false ")
            + HelpExampleCli("create", "stream test false '{\"Description\":\"Test stream\"}'")
            + HelpExampleRpc("create", "\"stream\", \"test\", false")
        ));
    
    mapHelpStrings.insert(std::make_pair("createfrom",
            "createfrom \"from-address\" \"entity-type\" \"entity-name\" open ( custom-fields )\n"
            "\nCreates stream, upgrade, filter or variable using specific address\n"
            + HelpRequiringPassphraseWrapper() +
            "\nArguments:\n"
            "1. \"from-address\"                   (string, required) Address used for creating.\n"
            "2. entity-type                      (string, required) stream\n"
            "3. \"stream-name\"                    (string, required) Stream name, if not \"\" should be unique.\n"
            "4. open                             (boolean, required) Allow anyone to publish in this stream\n"
            "  or \n"
            "4. restrictions                     (object, optional) Stream restrictions\n"
            "    {\n"
            "      \"restrict\" : \"restrictions\"   (string, optional) Stream restrictions, comma delimited. Possible values: write,offchain,onchain\n"
            "      \"salted\" : true|false         (boolean, optional) Indicates whether offchain item chunk hashes should be salted\n"
            "    }\n"
            "5  custom-fields                    (object, optional)  a json object with custom fields\n"
            "    {\n"
            "      \"param-name\": \"param-value\"   (strings, required) The key is the parameter name, the value is parameter value\n"
            "      ,...\n"
            "    }\n"
            "  or \n"
            "1. \"from-address\"                   (string, required) Address used for creating.\n"
            "2. entity-type                      (string, required) upgrade\n"
            "3. \"upgrade-name\"                   (string, required) Upgrade name, if not \"\" should be unique.\n"
            "4. open                             (boolean, required) Should be false\n"
            "5. custom-fields                    (object, required) A json object with custom fields\n"
            "    {\n"
            "      \"protocol-version\": version   (numeric, optional) Protocol version to upgrade to \n"
            "      \"parameter-name\": value       (numeric, optional) New value for upgradable parameter, one of: \n"
            "                                                        target-block-time,\n"
            "                                                        maximum-block-size,\n"
            "                                                        max-std-tx-size,\n"
            "                                                        max-std-op-returns-count,\n"
            "                                                        max-std-op-return-size,\n"
            "                                                        max-std-op-drops-count,\n"
            "                                                        max-std-element-size,\n"
            "                                                        anyone-can-connect,\n"
            "                                                        anyone-can-send,\n"
            "                                                        anyone-can-receive,\n"
            "                                                        anyone-can-receiveempty,\n"
            "                                                        anyone-can-issue,\n"
            "                                                        anyone-can-create,\n"
            "                                                        anyone-can-activate\n"
            "      \"start-block\": block          (numeric, optional, default 0) Block to apply from \n"
//            "      \"param-name\": \"param-value\"   (strings, required) The key is the parameter name, the value is parameter value\n"
            "      ,...\n"
            "    }\n"
            "  or \n"
            "1. \"from-address\"                   (string, required) Address used for creating.\n"
            "2. \"entity-type\"                    (string, required) txfilter\n"
            "3. \"txfilter-name\"                  (string, required) Tx filter name, if not \"\" should be unique.\n"
            "4. restrictions                     (object, required)  a json object with filter restrictions\n"
            "    {\n"
            "      \"for\": \"entity-identifier\"    (string, optional) Asset/stream identifier - one of: create txid, stream reference, stream name.\n"
            "        or\n"
            "      \"for\": entity-identifier(s)   (array, optional) A json array of asset/stream identifiers .\n"    
            "    }\n"
            "5. \"javascript-code\"                (string, required) JavaScript filter code, see help filters. Example:\n"
            "                                                         function filtertransaction()\n"
            "                                                         {\n"
            "                                                             var tx=getfiltertransaction();\n"
            "                                                                                          \n"
            "                                                             if (tx.vout.length<1)\n"
            "                                                                 return \"One output required\";\n"
            "                                                         }   \n "
            "  or \n"
            "1. \"from-address\"                   (string, required) Address used for creating.\n"
            "2. \"entity-type\"                    (string, required) streamfilter\n"
            "3. \"streamfilter-name\"              (string, required) Stream filter name, if not \"\" should be unique.\n"
            "4. restrictions                     (object, required) A json object with filter restrictions\n"
            "    {\n"
            "    }\n"
            "5. \"javascript-code\"                (string, required) JavaScript filter code, see help filters. Example:\n"
            "                                                         function filterstreamitem()\n"
            "                                                         {\n"
            "                                                             var item=getfilterstreamitem();\n"
            "                                                                                          \n"
            "                                                             if (item.keys.length<2)\n"
            "                                                                 return \"At least two keys required\";\n"
            "                                                         }   \n"
/*    
            "  or \n"
            "1. \"from-address\"                   (string, required) Address used for creating.\n"
            "2. entity-type                      (string, required) variable\n"
            "3. \"variable-name\"                  (string, required) Variable name, if not \"\" should be unique.\n"
            "4. open                             (boolean, optional) Should be false\n"
            "5. value                            (any data, optional, default null) Variable value (JSON objects and arrays allowed).\n"
*/
            "\nResult:\n"
            "\"transactionid\"                     (string) The transaction id.\n"
            "\nExamples:\n"
            + HelpExampleCli("createfrom", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" stream test false ")
            + HelpExampleCli("createfrom", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" stream test false '{\"Description\":\"Test stream\"}'")
            + HelpExampleRpc("createfrom", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\", \"stream\", \"test\", false")
        ));
    
    mapHelpStrings.insert(std::make_pair("createrawexchange",
            "createrawexchange \"txid\" vout ask-assets\n"
            "\nCreates new exchange transaction\n"
            "Note that the transaction should be completed by appendrawexchange\n"
            + HelpRequiringPassphraseWrapper() +
            "\nArguments:\n"
            "1. \"txid\"                           (string, required) Transaction ID of the output prepared by preparelockunspent.\n"
            "2. vout                             (numeric, required) Output index\n"
            "3. ask-assets                       (object, required) A json object of assets to ask\n"
            "    {\n"
            "      \"asset-identifier\" : asset-quantity\n"
            "      ,...\n"
            "    }\n"                
            "\nResult:\n"
            "\"transaction\"                       (string) hex string of the transaction\n"
            "\nExamples:\n"
            + HelpExampleCli("createrawexchange", "f4c3dd510dd55761015c9d96bff7793b0d501dd6f01a959fd7dd02478fb47dfb 1 \"{\\\"1234-5678-1234\\\":200}\"")
            + HelpExampleRpc("createrawexchange", "\"f4c3dd510dd55761015c9d96bff7793b0d501dd6f01a959fd7dd02478fb47dfb\",1,\"{\\\"1234-5678-1234\\\":200}\\\"")
        ));
    
}

void mc_InitRPCHelpMap07()
{
    mapHelpStrings.insert(std::make_pair("createrawsendfrom",
            "createrawsendfrom \"from-address\" {\"address\":amount,...} ( [data] \"action\" ) \n"
            "\nCreate a transaction using the given sending address.\n"

            "\nArguments:\n"
            "1. \"from-address\"                           (string, required) Address to send from.\n"
            "2. addresses                                (object, required) Object with addresses as keys, see help addresses-all for details.\n"
            "3. data                                     (array, optional) Array of hexadecimal strings or data objects, see help data-all for details.\n"
            "4. \"action\"                                 (string, optional, default \"\") Additional actions: \"lock\", \"sign\", \"lock,sign\", \"sign,lock\", \"send\". \n"
                

            "\nResult:\n"
            "\"transaction\"                               (string) hex string of the transaction (if action= \"\" or \"lock\")\n"
            "  or \n"
            "{                                           (object) A json object (if action= \"sign\" or \"lock,sign\" or \"sign,lock\")\n"
            "  \"hex\": \"value\",                           (string) The raw transaction with signature(s) (hex-encoded string)\n"
            "  \"complete\": true|false                    (boolean) if transaction has a complete set of signature (0 if not)\n"
            "}\n"
            "  or \n"
            "\"hex\"                                       (string) The transaction hash in hex (if action= \"send\")\n"

            "\nExamples\n"
            + HelpExampleCli("createrawsendfrom", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" \"{\\\"address\\\":0.01}\"")
            + HelpExampleRpc("createrawsendfrom", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\", \"{\\\"address\\\":0.01}\"")
        ));
    
    mapHelpStrings.insert(std::make_pair("decoderawexchange",
            "decoderawexchange \"tx-hex\" ( verbose )\n"
            "\nReturn a JSON object representing the serialized, hex-encoded exchange transaction.\n"

            "\nArguments:\n"
            "1. \"tx-hex\"                         (string, required) The exchange transaction hex string\n"
            "2. verbose                          (boolean, optional, default=false) If true, returns array of all exchanges\n"
            "                                                                       created by createrawexchange or appendrawexchange\n"

            "\nResults is an object with exchange details\n"
            "\nExamples:\n"
            + HelpExampleCli("decoderawexchange", "\"hexstring\"")
            + HelpExampleRpc("decoderawexchange", "\"hexstring\"")
       ));
    
    mapHelpStrings.insert(std::make_pair("disablerawtransaction",
            "disablerawtransaction \"tx-hex\"\n"
            "\nDisable raw transaction by spending one of its inputs and sending it back to the wallet.\n"

            "\nArguments:\n"
            "1. \"tx-hex\"                         (string, required) The transaction hex string\n"

            "\nResult:\n"
            "\"transactionid\"                     (string) The transaction id.\n"
            "\nExamples:\n"
            + HelpExampleCli("disablerawtransaction", "\"hexstring\"")
            + HelpExampleRpc("disablerawtransaction", "\"hexstring\"")
        ));
    
    mapHelpStrings.insert(std::make_pair("dumpprivkey",
            "dumpprivkey \"address\"\n"
            "\nReveals the private key corresponding to 'address'.\n"
            "Then the importprivkey can be used with this output\n"
            "\nArguments:\n"
            "1. \"address\"                        (string, required) The MultiChain address for the private key\n"
            "\nResult:\n"
            "\"key\"                               (string) The private key\n"
            "\nExamples:\n"
            + HelpExampleCli("dumpprivkey", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XZ\"")
            + HelpExampleCli("importprivkey", "\"mykey\"")
            + HelpExampleRpc("dumpprivkey", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XZ\"")
        ));
    
    mapHelpStrings.insert(std::make_pair("dumpwallet",
            "dumpwallet \"filename\"\n"
            "\nDumps all wallet keys in a human-readable format.\n"
            "\nArguments:\n"
            "1. \"filename\"                       (string, required) The filename\n"
            "\nExamples:\n"
            + HelpExampleCli("dumpwallet", "\"test\"")
            + HelpExampleRpc("dumpwallet", "\"test\"")
        ));
    
    mapHelpStrings.insert(std::make_pair("encryptwallet",
            "encryptwallet \"passphrase\"\n"
            "\nEncrypts the wallet with 'passphrase'. This is for first time encryption.\n"
            "After this, any calls that interact with private keys such as sending or signing \n"
            "will require the passphrase to be set prior the making these calls.\n"
            "Use the walletpassphrase call for this, and then walletlock call.\n"
            "If the wallet is already encrypted, use the walletpassphrasechange call.\n"
            "Note that this will shutdown the server.\n"
            "\nArguments:\n"
            "1. \"passphrase\"                     (string) The pass phrase to encrypt the wallet with. "
            "                                             It must be at least 1 character, but should be long.\n"
            "\nExamples:\n"
            "\nEncrypt you wallet\n"
            + HelpExampleCli("encryptwallet", "\"my pass phrase\"") +
            "\nNow set the passphrase to use the wallet, such as for signing or sending assets\n"
            + HelpExampleCli("walletpassphrase", "\"my pass phrase\"") +
            "\nNow we can so something like sign\n"
            + HelpExampleCli("signmessage", "\"address\" \"test message\"") +
            "\nNow lock the wallet again by removing the passphrase\n"
            + HelpExampleCli("walletlock", "") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("encryptwallet", "\"my pass phrase\"")
        ));
    
    mapHelpStrings.insert(std::make_pair("getaccount",
            "getaccount \"address\"\n"
            "\nReturns the account associated with the given address.\n"
            "\nArguments:\n"
            "1. \"address\"                        (string, required) The address for account lookup.\n"
            "\nResult:\n"
            "\"accountname\"                       (string) the account address\n"
            "\nExamples:\n"
            + HelpExampleCli("getaccount", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XZ\"")
            + HelpExampleRpc("getaccount", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XZ\"")
        ));
    
    mapHelpStrings.insert(std::make_pair("getaccountaddress",
            "getaccountaddress \"account\"\n"
            "\nReturns the current address for receiving payments to this account.\n" 
            "\nArguments:\n"
            "1. \"account\"                        (string, required) The account name for the address.\n"
            "                                                       It can also be set to the empty string \"\" to represent the default account.\n"
            "                                                       The account does not need to exist, it will be created and a new address created\n"
            "                                                       if there is no account by the given name.\n"
            "\nResult:\n"
            "\"address\"                           (string) The account address\n"
            "\nExamples:\n"
            + HelpExampleCli("getaccountaddress", "")
            + HelpExampleCli("getaccountaddress", "\"\"")
            + HelpExampleCli("getaccountaddress", "\"myaccount\"")
            + HelpExampleRpc("getaccountaddress", "\"myaccount\"")
        ));
    
    mapHelpStrings.insert(std::make_pair("getaddressbalances",
            "getaddressbalances \"address\" ( minconf includeLocked ) \n"
            "\nReturns asset balances for specified address\n"
            "\nArguments:\n"
            "1. \"address\"                        (string, required) Address to return balance for.\n"
            "2. minconf                          (numeric, optional, default=1) Only include transactions confirmed at least this many times.\n"
            "3. includeLocked                    (bool, optional, default=false) Also take locked outputs into account\n"
            "\nResult:\n"
            "An array of Objects with totals and details for each asset.\n"
            "\nExamples:\n"
            "\nThe total amount in the server across all accounts\n"
            + HelpExampleCli("getaddressbalances", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\"")
            + HelpExampleCli("getaddressbalances", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" 0 true")
            + HelpExampleRpc("getaddressbalances", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\"")
        ));
    
    mapHelpStrings.insert(std::make_pair("getaddresses",
            "getaddresses ( verbose )\n"
            "\nReturns the list of all addresses in the wallet.\n"
            "\nArguments: \n"
            "1. verbose                          (boolean, optional, default=false) The account name.\n"
            "\nResult:\n"
            "[                                   (json array of )\n"
            "  \"address\"                         (string) an address \n"
            "  or \n"
            "  address-datails                   (object) address details if verbose=true\n"
            "  ,...\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("getaddresses", "")
            + HelpExampleRpc("getaddresses", "")
        ));
    
}

void mc_InitRPCHelpMap08()
{
    mapHelpStrings.insert(std::make_pair("getaddressesbyaccount",
            "getaddressesbyaccount \"account\"\n"
            "\nReturns the list of addresses for the given account.\n"
            "\nArguments:\n"
            "1. \"account\"                        (string, required) The account name.\n"
            "\nResult:\n"
            "[                                   (json array of string)\n"
            "  \"address\"                         (string) an address associated with the given account\n"
            "  ,...\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("getaddressesbyaccount", "\"tabby\"")
            + HelpExampleRpc("getaddressesbyaccount", "\"tabby\"")
        ));
    
    mapHelpStrings.insert(std::make_pair("getaddresstransaction",
            "getaddresstransaction \"address\" \"txid\" ( verbose )\n"
            "\nProvides information about transaction txid related to address in this node’s wallet\n"
            "\nArguments:\n"
            "1. \"address\"                        (string, required) Address used for balance calculation.\n"
            "2. \"txid\"                           (string, required) The transaction id\n"
            "3. verbose                          (bool, optional, default=false) If true, returns detailed array of inputs and outputs and raw hex of transactions\n"
            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"balance\": {...},               (object)  Changes in address balance. \n"
            "    {\n"
            "      \"amount\": x.xxx,              (numeric) The amount in native currency. Negative value means amount was send by the wallet, positive - received\n"
            "      \"assets\": {...},              (object)  Changes in asset amounts. \n"
            "    }\n"
            "    \"myaddresses\": [...],           (array)   Address passed as parameter\n"
            "    \"addresses\": [...],             (array)   Array of counterparty addresses  involved in transaction  \n"
            "    \"permissions\": [...],           (array)   Changes in permissions \n"
            "    \"issue\": {...},                 (object)  Issue details  \n"
            "    \"data\" : \"metadata\",            (array)   Hexadecimal representation of metadata appended to the transaction\n"
            "    \"confirmations\": n,             (numeric)  The number of confirmations for the transaction. Available for 'send' and \n"
            "                                         'receive' category of transactions.\n"
            "    \"blockhash\": \"hashvalue\",       (string) The block hash containing the transaction. Available for 'send' and 'receive'\n"
            "                                        category of transactions.\n"
            "    \"blockindex\": n,                (numeric) The block index containing the transaction. Available for 'send' and 'receive'\n"
            "                                      category of transactions.\n"
            "    \"txid\": \"transactionid\",        (string) The transaction id. Available for 'send' and 'receive' category of transactions.\n"
            "    \"time\": xxx,                    (numeric) The transaction time in seconds since epoch (midnight Jan 1 1970 GMT).\n"
            "    \"timereceived\": xxx,            (numeric) The time received in seconds since epoch (midnight Jan 1 1970 GMT). Available \n"
            "                                          for 'send' and 'receive' category of transactions.\n"
            "    \"comment\": \"...\",               (string) If a comment is associated with the transaction.\n"
            "    \"vin\": [...],                   (array)  If verbose=true. Array of input details\n"
            "    \"vout\": [...],                  (array)  If verbose=true. Array of output details\n"
            "    \"hex\" : \"data\"                  (string) If verbose=true. Raw data for transaction\n"
            "  }\n"
            "]\n"

            "\nExamples:\n"
            + HelpExampleCli("getaddresstransaction", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" \"1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d\"")
            + HelpExampleCli("getaddresstransaction", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" \"1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d\" true")
            + HelpExampleRpc("getaddresstransaction", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" \"1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d\"")
        ));
    
    mapHelpStrings.insert(std::make_pair("getassetbalances",
            "getassetbalances ( \"account\" minconf includeWatchonly includeLocked )\n"
            "\nIf account is not specified, returns the server's total available asset balances.\n"
            "If account is specified, returns the balances in the account.\n"
            "Note that the account \"\" is not the same as leaving the parameter out.\n"
            "The server total may be different to the balance in the default \"\" account.\n"
            "\nArguments:\n"
            "1. \"account\"                        (string, optional) The selected account, or \"*\" for entire wallet. It may be the default account using \"\".\n"
            "2. minconf                          (numeric, optional, default=1) Only include transactions confirmed at least this many times.\n"
            "3. includeWatchonly                 (bool, optional, default=false) Also include balance in watchonly addresses (see 'importaddress')\n"
            "4. includeLocked                    (bool, optional, default=false) Also take locked outputs into account\n"
            "Results are an array of Objects with totals and details for each asset.\n"
            "\nExamples:\n"
            "\nThe total amount in the server across all accounts\n"
            + HelpExampleCli("getassetbalances", "") +
            "\nThe total amount in the server across all accounts, with at least 5 confirmations\n"
            + HelpExampleCli("getassetbalances", "\"*\" 6") +
            "\nThe total amount in the default account with at least 1 confirmation\n"
            + HelpExampleCli("getassetbalances", "\"\"") +
            "\nThe total amount in the account named tabby with at least 6 confirmations\n"
            + HelpExampleCli("getassetbalances", "\"tabby\" 6 true") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("getassetbalances", "\"tabby\", 6")
        ));
    
    mapHelpStrings.insert(std::make_pair("getassettransaction",
            "getassettransaction \"asset-identifier\" \"txid\" ( verbose )\n"
            "\nRetrieves a specific transaction txid involving asset.\n"
            "\nArguments:\n"
            "1. \"asset-identifier\"               (string, required) Asset identifier - one of: issue txid, asset reference, asset name.\n"
            "2. \"txid\"                           (string, required) The transaction id\n"
            "3. verbose                          (boolean, optional, default=false) If true, returns information about item transaction \n"
            "\nResult:\n"
            "\"transaction\"                       (object) Information about an individual transaction from the perspective of a particular asset.\n"
            "\nExamples:\n"
            + HelpExampleCli("getassettransaction", "\"myasset\" \"mytxid\"") 
            + HelpExampleCli("getassettransaction", "\"myasset\" \"mytxid\"  true") 
            + HelpExampleRpc("getassettransaction", "\"myasset\", \"mytxid\", false")
        ));
    
    mapHelpStrings.insert(std::make_pair("getbalance",
            "getbalance ( \"account\" minconf includeWatchonly )\n"
            "\nIf account is not specified, returns the server's total available balance.\n"
            "If account is specified, returns the balance in the account.\n"
            "Note that the account \"\" is not the same as leaving the parameter out.\n"
            "The server total may be different to the balance in the default \"\" account.\n"
            "\nArguments:\n"
            "1. \"account\"                        (string, optional) The selected account, or \"*\" for entire wallet. It may be the default account using \"\".\n"
            "2. minconf                          (numeric, optional, default=1) Only include transactions confirmed at least this many times.\n"
            "3. includeWatchonly                 (bool, optional, default=false) Also include balance in watchonly addresses (see 'importaddress')\n"
            "\nResult:\n"
            "amount                              (numeric) The total amount in native currency received for this account.\n"
            "\nExamples:\n"
            "\nThe total amount in the server across all accounts\n"
            + HelpExampleCli("getbalance", "") +
            "\nThe total amount in the server across all accounts, with at least 5 confirmations\n"
            + HelpExampleCli("getbalance", "\"*\" 6") +
            "\nThe total amount in the default account with at least 1 confirmation\n"
            + HelpExampleCli("getbalance", "\"\"") +
            "\nThe total amount in the account named tabby with at least 6 confirmations\n"
            + HelpExampleCli("getbalance", "\"tabby\" 6") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("getbalance", "\"tabby\", 6")
        ));
    
    mapHelpStrings.insert(std::make_pair("getmultibalances",
            "getmultibalances ( address(es) assets minconf includeLocked includeWatchonly ) \n"
            "\nReturns asset balances for specified address\n"
            "\nArguments:\n"
            "1. \"address(es)\"                    (string, optional, default=*) Address(es) to return balance for, comma delimited\n"
            " or\n"
            "1. address(es)                      (array, optional) A json array of addresses to return balance for\n"                
            "2. \"asset\"                          (string, optional, default=*) Single asset identifier to return balance for\n"                
            " or\n"
            "2. assets                           (array, optional) A json array of asset identifiers to return balance for\n"                
            "3. minconf                          (numeric, optional, default=1) Only include transactions confirmed at least this many times.\n"
            "4. includeWatchonly                 (bool, optional, default=false) Include transactions to watchonly addresses (see 'importaddress')\n"
            "5. includeLocked                    (bool, optional, default=false) Also take locked outputs into account\n"
            "\nResult:\n"
            "An object of balance arrays with totals and details for each address.\n"
            "\nExamples:\n"
            + HelpExampleCli("getmultibalances", "")
            + HelpExampleCli("getmultibalances", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\"")
            + HelpExampleRpc("getmultibalances", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\"")
        ));
    
    mapHelpStrings.insert(std::make_pair("getnewaddress",
            "getnewaddress ( \"account\" )\n"
            "\nReturns a new address for receiving payments.\n"      
            "If 'account' is specified (deprecated), it is added to the address book \n"
            "so payments received with the address will be credited to 'account'.\n"
            "\nArguments:\n"
            "1. \"account\"                        (string, optional) The account name for the address to be linked to.\n "
            "                                                       If not provided, the default account \"\" is used.\n "
            "                                                       It can also be set to the empty string \"\" to represent the default account.\n "
            "                                                       The account does not need to exist, it will be created if there is no account by the given name.\n"
            "\nResult:\n"
            "\"address\"                           (string) The new address\n"    
            "\nExamples:\n"
            + HelpExampleCli("getnewaddress", "")
            + HelpExampleCli("getnewaddress", "\"\"")
            + HelpExampleCli("getnewaddress", "\"myaccount\"")
            + HelpExampleRpc("getnewaddress", "\"myaccount\"")
        ));
    
    mapHelpStrings.insert(std::make_pair("getrawchangeaddress",
            "getrawchangeaddress\n"
            "\nReturns a new  address, for receiving change.\n"
            "This is for use with raw transactions, NOT normal use.\n"
            "\nResult:\n"
            "\"address\"                           (string) The address\n"
            "\nExamples:\n"
            + HelpExampleCli("getrawchangeaddress", "")
            + HelpExampleRpc("getrawchangeaddress", "")
        ));
    
    mapHelpStrings.insert(std::make_pair("getreceivedbyaccount",
            "getreceivedbyaccount \"account\" ( minconf )\n"
            "\nReturns the total amount received by addresses with <account> in transactions with at least [minconf] confirmations.\n"
            "\nArguments:\n"
            "1. \"account\"                        (string, required) The selected account, may be the default account using \"\".\n"
            "2. minconf                          (numeric, optional, default=1) Only include transactions confirmed at least this many times.\n"
            "\nResult:\n"
            "amount                              (numeric) The total amount in native currency received for this account.\n"
            "\nExamples:\n"
            "\nAmount received by the default account with at least 1 confirmation\n"
            + HelpExampleCli("getreceivedbyaccount", "\"\"") +
            "\nAmount received at the tabby account including unconfirmed amounts with zero confirmations\n"
            + HelpExampleCli("getreceivedbyaccount", "\"tabby\" 0") +
            "\nThe amount with at least 6 confirmation, very safe\n"
            + HelpExampleCli("getreceivedbyaccount", "\"tabby\" 6") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("getreceivedbyaccount", "\"tabby\", 6")
        ));
    
    mapHelpStrings.insert(std::make_pair("getreceivedbyaddress",
            "getreceivedbyaddress \"address\" ( minconf )\n"
            "\nReturns the total amount received by the given address in transactions with at least minconf confirmations.\n"
            "\nArguments:\n"
            "1. \"address\"                        (string, required) The address for transactions.\n"
            "2. minconf                          (numeric, optional, default=1) Only include transactions confirmed at least this many times.\n"
            "\nResult:\n"
            "amount                              (numeric) The total amount in native currency received at this address.\n"
            "\nExamples:\n"
            "\nThe amount from transactions with at least 1 confirmation\n"
            + HelpExampleCli("getreceivedbyaddress", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XZ\"") +
            "\nThe amount including unconfirmed transactions, zero confirmations\n"
            + HelpExampleCli("getreceivedbyaddress", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XZ\" 0") +
            "\nThe amount with at least 6 confirmation, very safe\n"
            + HelpExampleCli("getreceivedbyaddress", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XZ\" 6") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("getreceivedbyaddress", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XZ\", 6")
        ));
    
}

void mc_InitRPCHelpMap09()
{
    mapHelpStrings.insert(std::make_pair("getstreamitem",
            "getstreamitem \"stream-identifier\" \"txid\" ( verbose )\n"
            "\nReturns stream item.\n"
            "\nArguments:\n"
            "1. \"stream-identifier\"              (string, required) Stream identifier - one of: create txid, stream reference, stream name.\n"
            "2. \"txid\"                           (string, required) The transaction id\n"
            "3. verbose                          (boolean, optional, default=false) If true, returns information about item transaction \n"
            "\nResult:\n"
            "\"stream-item\"                       (object) Stream item.\n"
            "\nExamples:\n"
            + HelpExampleCli("getstreamitem", "\"mytxid\"") 
            + HelpExampleCli("getstreamitem", "\"mytxid\"  true") 
            + HelpExampleRpc("getstreamitem", "\"mytxid\", false")
        ));
    
    mapHelpStrings.insert(std::make_pair("gettotalbalances",
            "gettotalbalances ( minconf includeWatchonly includeLocked )\n"
            "\nReturns a list of all the asset balances in this node’s wallet, with at least minconf confirmations.\n"
            "\nArguments:\n"
            "1. minconf                          (numeric, optional, default=1) Only include transactions confirmed at least this many times.\n"
            "2. includeWatchonly                 (bool, optional, default=false) Also include balance in watchonly addresses (see 'importaddress')\n"
            "3. includeLocked                    (bool, optional, default=false) Also take locked outputs into account\n"
            "\nResult:\n"
            "An array of Objects with totals and details for each asset.\n"
            "\nExamples:\n"
            "\nThe total amount in the server across all accounts\n"
            + HelpExampleCli("gettotalbalances", "") +
            "\nThe total amount in the server across all accounts, with at least 5 confirmations\n"
            + HelpExampleCli("gettotalbalances", "6") +
            "\nThe total amount in the default account with at least 1 confirmation\n"
            + HelpExampleCli("gettotalbalances", "") +
            "\nThe total amount in the account named tabby with at least 6 confirmations\n"
            + HelpExampleCli("gettotalbalances", "6 true") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("gettotalbalances", "\"tabby\", 6")
        ));
    
    mapHelpStrings.insert(std::make_pair("gettransaction",
            "gettransaction \"txid\" ( includeWatchonly )\n"
            "\nGet detailed information about in-wallet transaction <txid>\n"
            "\nArguments:\n"
            "1. \"txid\"                           (string, required) The transaction id\n"
            "2. includeWatchonly                 (bool, optional, default=false) Whether to include watchonly addresses in balance calculation and details[]\n"
            "\nResult:\n"
            "{\n"
            "  \"amount\" : x.xxx,                 (numeric) The transaction amount in native currency\n"
            "  \"confirmations\" : n,              (numeric) The number of confirmations\n"
            "  \"blockhash\" : \"hash\",             (string) The block hash\n"
            "  \"blockindex\" : xx,                (numeric) The block index\n"
            "  \"blocktime\" : ttt,                (numeric) The time in seconds since epoch (1 Jan 1970 GMT)\n"
            "  \"txid\" : \"transactionid\",         (string) The transaction id.\n"
            "  \"time\" : ttt,                     (numeric) The transaction time in seconds since epoch (1 Jan 1970 GMT)\n"
            "  \"timereceived\" : ttt,             (numeric) The time received in seconds since epoch (1 Jan 1970 GMT)\n"
            "  \"details\" : [\n"
            "    {\n"
            "      \"account\" : \"accountname\",    (string) The account name involved in the transaction, can be \"\" for the default account.\n"
            "      \"address\" : \"address\",        (string) The address involved in the transaction\n"
            "      \"category\" : \"send|receive\",  (string) The category, either 'send' or 'receive'\n"
            "      \"amount\" : x.xxx              (numeric) The amount in native currency\n"
            "      \"vout\" : n,                   (numeric) the vout value\n"
            "    }\n"
            "    ,...\n"
            "  ],\n"
            "  \"hex\" : \"data\"                    (string) Raw data for transaction\n"
            "}\n"

            "\nExamples:\n"
            + HelpExampleCli("gettransaction", "\"1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d\"")
            + HelpExampleCli("gettransaction", "\"1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d\" true")
            + HelpExampleRpc("gettransaction", "\"1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d\"")
        ));
    
    mapHelpStrings.insert(std::make_pair("gettxoutdata",
            "gettxoutdata \"txid\" vout ( count-bytes start-byte )\n"
            "\nReturns metadata of transaction output.\n"
            "\nArguments:\n"
            "1. \"txid\"                           (string, required) The transaction id\n"
            "2. vout                             (numeric, required) vout value\n"
            "3. count-bytes                      (numeric, optional, default=INT_MAX) Number of bytes to return\n"
            "4. start-byte                       (numeric, optional, default=0) start from specific byte \n"
            "\nResult:\n"
            "data                                  (string) transaction output metadata.\n"
            "\nExamples:\n"
            "\nView the data\n"
            + HelpExampleCli("gettxoutdata", "\"txid\" 1") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("gettxoutdata", "\"txid\", 1")
        ));
    
    mapHelpStrings.insert(std::make_pair("getunconfirmedbalance",
                "getunconfirmedbalance\n"
                "Returns the server's total unconfirmed balance\n"
        ));
    
    mapHelpStrings.insert(std::make_pair("getwalletinfo",
            "getwalletinfo\n"
            "Returns an object containing various wallet state info.\n"
            "\nResult:\n"
            "{\n"
            "  \"walletversion\": xxxxx,           (numeric) the wallet version\n"
            "  \"balance\": xxxxxxx,               (numeric) the total native currency balance of the wallet\n"
            "  \"txcount\": xxxxxxx,               (numeric) the total number of transactions and stream keys in the wallet\n"
            "  \"walletdbversion\": xxxxx,         (numeric) the wallet database version\n"
            "  \"keypoololdest\": xxxxxx,          (numeric) the timestamp (seconds since GMT epoch) of the oldest pre-generated key in the key pool\n"
            "  \"keypoolsize\": xxxx,              (numeric) how many new keys are pre-generated\n"
            "  \"unlocked_until\": ttt,            (numeric) the timestamp in seconds since epoch (midnight Jan 1 1970 GMT)\n"
            "                                              that the wallet is unlocked for transfers, or 0 if the wallet is locked\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("getwalletinfo", "")
            + HelpExampleRpc("getwalletinfo", "")
        ));
    
    mapHelpStrings.insert(std::make_pair("getwallettransaction",
            "getwallettransaction \"txid\" ( includeWatchonly verbose )\n"
            "\nGet detailed information about in-wallet transaction <txid>\n"
            "\nArguments:\n"
            "1. \"txid\"    (string, required) The transaction id\n"
            "2. includeWatchonly    (bool, optional, default=false) Whether to include watchonly addresses in balance calculation and details[]\n"
            "3. verbose (bool, optional, default=false) If true, returns detailed array of inputs and outputs and raw hex of transactions\n"
            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"balance\": {...},               (object)  Changes in wallet balance. \n"
            "    {\n"
            "      \"amount\": x.xxx,              (numeric) The amount in native currency. Negative value means amount was send by the wallet, positive - received\n"
            "      \"assets\": {...},              (object)  Changes in asset amounts. \n"
            "    }\n"
            "    \"myaddresses\": [...],           (array)   Array of wallet addresses involved in transaction   \n"
            "    \"addresses\": [...],             (array)   Array of counterparty addresses  involved in transaction  \n"
            "    \"permissions\": [...],           (array)   Changes in permissions \n"
            "    \"issue\": {...},                 (object)  Issue details  \n"
            "    \"data\" : \"metadata\",            (array)   Hexadecimal representation of metadata appended to the transaction\n"
            "    \"confirmations\": n,             (numeric)  The number of confirmations for the transaction. Available for 'send' and \n"
            "                                               'receive' category of transactions.\n"
            "    \"blockhash\": \"hashvalue\",       (string) The block hash containing the transaction. Available for 'send' and 'receive'\n"
            "                                               category of transactions.\n"
            "    \"blockindex\": n,                (numeric) The block index containing the transaction. Available for 'send' and 'receive'\n"
            "                                              category of transactions.\n"
            "    \"txid\": \"transactionid\",        (string) The transaction id. Available for 'send' and 'receive' category of transactions.\n"
            "    \"time\": xxx,                    (numeric) The transaction time in seconds since epoch (midnight Jan 1 1970 GMT).\n"
            "    \"timereceived\": xxx,            (numeric) The time received in seconds since epoch (midnight Jan 1 1970 GMT). Available \n"
            "                                              for 'send' and 'receive' category of transactions.\n"
            "    \"comment\": \"...\",               (string) If a comment is associated with the transaction.\n"
            "    \"vin\": [...],                   (array)  If verbose=true. Array of input details\n"
            "    \"vout\": [...],                  (array)  If verbose=true. Array of output details\n"
            "    \"hex\" : \"data\"                  (string) If verbose=true. Raw data for transaction\n"
            "  }\n"
            "]\n"

            "\nExamples:\n"
            + HelpExampleCli("getwallettransaction", "\"1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d\"")
            + HelpExampleCli("getwallettransaction", "\"1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d\" true")
            + HelpExampleRpc("getwallettransaction", "\"1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d\"")
        ));
    
    mapHelpStrings.insert(std::make_pair("grant",
            "grant \"address(es)\" \"permission(s)\" ( native-amount startblock endblock \"comment\" \"comment-to\" )\n"
            "\nGrant permission(s) to a given address. \n"
            + HelpRequiringPassphraseWrapper() +
            "\nArguments:\n"
            "1. \"address(es)\"                    (string, required)  The multichain addresses to send to (comma delimited)\n"
            "2. \"permission(s)\"                  (string, required)  Permission strings, comma delimited. \n"
            "                                                        Global: " + AllowedPermissions() + " \n"
            "                                                        or per-asset: asset-identifier.issue,admin,activate,send,receive \n"
            "                                                        or per-stream: stream-identifier.write,read,activate,admin \n"
            "3. native-amount                    (numeric, optional) Native currency amount to send. eg 0.1. Default - 0.0\n"
            "4. startblock                       (numeric, optional) Block to apply permissions from (inclusive). Default - 0\n"
            "5. endblock                         (numeric, optional) Block to apply permissions to (exclusive). Default - 4294967295\n"
            "                                                        If -1 is specified default value is used.\n"
            "6. \"comment\"                        (string, optional)  A comment used to store what the transaction is for. \n"
            "                                                        This is not part of the transaction, just kept in your wallet.\n"
            "7. \"comment-to\"                     (string, optional)  A comment to store the name of the person or organization \n"
            "                                                        to which you're sending the transaction. This is not part of the \n"
            "                                                        transaction, just kept in your wallet.\n"
            "\nResult:\n"
            "\"transactionid\"                     (string) The transaction id.\n"
            "\nExamples:\n"
            + HelpExampleCli("grant", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" 0.1 connect,send,receive")
            + HelpExampleCli("grant", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" 0.1 mystream.admin,write")
            + HelpExampleCli("grant", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" 0.1 mine \"permission to mine\" \"Miners Ltd.\"")
            + HelpExampleRpc("grant", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\", 0.1, admin \"temporary admin\", \"Admins Ltd.\" 20000 30000")
        ));
    
    mapHelpStrings.insert(std::make_pair("grantfrom",
            "grantfrom \"from-address\" \"to-address(es)\" \"permission(s)\" ( native-amount startblock endblock \"comment\" \"comment-to\" )\n"
            "\nGrant permission using specific address.\n"
            + HelpRequiringPassphraseWrapper() +
            "\nArguments:\n"
            "1. \"from-address\"                   (string, required) Address used for grant.\n"
            "2. \"to-address(es)\"                 (string, required) The multichain addresses to grant permissions to\n"
            "3. \"permission(s)\"                  (string, required)  Permission strings, comma delimited. \n"
            "                                                        Global: " + AllowedPermissions() + " \n"
            "                                                        or per-asset: asset-identifier.issue,admin,activate,send,receive \n"
            "                                                        or per-stream: stream-identifier.write,read,activate,admin \n"
            "4. native-amount                    (numeric, optional) Native currency amount to send. eg 0.1. Default - 0.0\n"
            "5. startblock                       (numeric, optional) Block to apply permissions from (inclusive). Default - 0\n"
            "6. endblock                         (numeric, optional) Block to apply permissions to (exclusive). Default - 4294967295\n"
            "                                                        If -1 is specified default value is used.\n"
            "7. \"comment\"                        (string, optional)  A comment used to store what the transaction is for. \n"
            "                                                        This is not part of the transaction, just kept in your wallet.\n"
            "8. \"comment-to\"                     (string, optional)  A comment to store the name of the person or organization \n"
            "                                                        to which you're sending the transaction. This is not part of the \n"
            "                                                        transaction, just kept in your wallet.\n"
            "\nResult:\n"
            "\"transactionid\"                     (string) The transaction id.\n"
            "\nExamples:\n"
            + HelpExampleCli("grantfrom", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" \"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" 0.1 connect,send,receive")
            + HelpExampleCli("grantfrom", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" \"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" 0.1 mine \"permission to mine\" \"Miners Ltd.\"")
            + HelpExampleRpc("grantfrom", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\", \"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\", 0.1, admin \"temporary admin\", \"Admins Ltd.\" 20000 30000")
        ));
    
    mapHelpStrings.insert(std::make_pair("grantwithdata",
            "grantwithdata \"address(es)\" \"permission(s)\" data|publish-new-stream-item ( native-amount startblock endblock )\n"
            "\nGrant permission(s) with metadata to a given address. \n"
            "\nArguments:\n"
            "1. \"address(es)\"                    (string, required) The multichain addresses to send to (comma delimited)\n"
            "2. \"permission(s)\"                  (string, required) Permission strings, comma delimited. \n"
            "                                                       Global: " + AllowedPermissions() + " \n"
            "                                                       or per-asset: asset-identifier.issue,admin,activate,send,receive \n"
            "                                                       or per-stream: stream-identifier.write,activate,admin \n"
            "3. data|publish-new-stream-item     (string or object, required) Data, see help data-with for details. \n"
            "4. native-amount                    (numeric, optional)  Native currency amount to send. eg 0.1. Default - 0.0\n"
            "5. startblock                       (numeric, optional)  Block to apply permissions from (inclusive). Default - 0\n"
            "6. endblock                         (numeric, optional)  Block to apply permissions to (exclusive). Default - 4294967295\n"
            "                                                         If -1 is specified default value is used.\n"
            "\nResult:\n"
            "\"transactionid\"                     (string) The transaction id.\n"
            "\nExamples:\n"
            + HelpExampleCli("grantwithdata", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" connect,send,receive 48656C6C6F20576F726C64210A")
            + HelpExampleCli("grantwithdata", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" mine 48656C6C6F20576F726C64210A 0.1")
            + HelpExampleRpc("grantwithdata", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\", admin, 48656C6C6F20576F726C64210A")
        ));
    
}

void mc_InitRPCHelpMap10()
{
    mapHelpStrings.insert(std::make_pair("grantwithmetadata",
            "grantwithdata \"address(es)\" \"permission(s)\" data|publish-new-stream-item ( native-amount startblock endblock )\n"
            "\nGrant permission(s) with metadata to a given address. \n"
            "\nArguments:\n"
            "1. \"address(es)\"                    (string, required) The multichain addresses to send to (comma delimited)\n"
            "2. \"permission(s)\"                  (string, required)  Permission strings, comma delimited. \n"
            "                                                        Global: " + AllowedPermissions() + " \n"
            "                                                        or per-asset: asset-identifier.issue,admin,activate,send,receive \n"
            "                                                        or per-stream: stream-identifier.write,activate,admin \n"
            "3. data|publish-new-stream-item     (string or object, required) Data, see help data-with for details. \n"
            "4. native-amount                    (numeric, optional)  Native currency amount to send. eg 0.1. Default - 0.0\n"
            "5. startblock                       (numeric, optional)  Block to apply permissions from (inclusive). Default - 0\n"
            "6. endblock                         (numeric, optional)  Block to apply permissions to (exclusive). Default - 4294967295\n"
            "                                                         If -1 is specified default value is used.\n"
            "\nResult:\n"
            "\"transactionid\"  (string) The transaction id.\n"
            "\nExamples:\n"
            + HelpExampleCli("grantwithdata", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" connect,send,receive 48656C6C6F20576F726C64210A")
            + HelpExampleCli("grantwithdata", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" mine 48656C6C6F20576F726C64210A 0.1")
            + HelpExampleRpc("grantwithdata", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\", admin, 48656C6C6F20576F726C64210A")
        ));
    
    mapHelpStrings.insert(std::make_pair("grantwithdatafrom",
            "grantwithdatafrom \"from-address\" \"to-address(es)\" \"permission(s)\" data|publish-new-stream-item ( native-amount startblock endblock )\n"
            "\nGrant permission with metadata using specific address.\n"
            + HelpRequiringPassphraseWrapper() +
            "\nArguments:\n"
            "1. \"from-address\"                   (string, required) Address used for grant.\n"
            "2. \"address(es)\"                    (string, required) The multichain addresses to send to (comma delimited)\n"
            "3. \"permission(s)\"                  (string, required) Permission strings, comma delimited. \n"
            "                                                       Global: " + AllowedPermissions() + " \n"
            "                                                       or per-asset: asset-identifier.issue,admin,activate,send,receive \n"
            "                                                       or per-stream: stream-identifier.write,activate,admin \n"
            "4. data|publish-new-stream-item     (string or object, required) Data, see help data-with for details. \n"
            "5. native-amount                    (numeric, optional)  Native currency amount to send. eg 0.1. Default - 0.0\n"
            "6. startblock                       (numeric, optional)  Block to apply permissions from (inclusive). Default - 0\n"
            "7. endblock                         (numeric, optional)  Block to apply permissions to (exclusive). Default - 4294967295\n"
            "                                                         If -1 is specified default value is used.\n"
            "\nResult:\n"
            "\"transactionid\"                     (string) The transaction id.\n"
            "\nExamples:\n"
            + HelpExampleCli("grantwithdatafrom", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" \"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" connect,send,receive 48656C6C6F20576F726C64210A")
            + HelpExampleCli("grantwithdatafrom", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" \"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" mine 48656C6C6F20576F726C64210A 0.1 ")
            + HelpExampleRpc("grantwithdatafrom", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\", \"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\", admin, 48656C6C6F20576F726C64210A")
        ));
    
    mapHelpStrings.insert(std::make_pair("grantwithmetadatafrom",
            "grantwithmetadatafrom \"from-address\" \"to-address(es)\" \"permission(s)\" data|publish-new-stream-item ( native-amount startblock endblock )\n"
            "\nGrant permission with metadata using specific address.\n"
            + HelpRequiringPassphraseWrapper() +
            "\nArguments:\n"
            "1. \"from-address\"                   (string, required) Address used for grant.\n"
            "2. \"address(es)\"                    (string, required) The multichain addresses to send to (comma delimited)\n"
            "3. \"permission(s)\"                  (string, required) Permission strings, comma delimited. \n"
            "                                                       Global: " + AllowedPermissions() + " \n"
            "                                                       or per-asset: asset-identifier.issue,admin,activate,send,receive \n"
            "                                                       or per-stream: stream-identifier.write,activate,admin \n"
            "4. data|publish-new-stream-item     (string or object, required) Data, see help data-with for details. \n"
            "5. native-amount                    (numeric, optional)  Native currency amount to send. eg 0.1. Default - 0.0\n"
            "6. startblock                       (numeric, optional)  Block to apply permissions from (inclusive). Default - 0\n"
            "7. endblock                         (numeric, optional)  Block to apply permissions to (exclusive). Default - 4294967295\n"
            "                                                         If -1 is specified default value is used.\n"
            "\nResult:\n"
            "\"transactionid\"                     (string) The transaction id.\n"
            "\nExamples:\n"
            + HelpExampleCli("grantwithmetadatafrom", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" \"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" connect,send,receive 48656C6C6F20576F726C64210A")
            + HelpExampleCli("grantwithmetadatafrom", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" \"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" mine 48656C6C6F20576F726C64210A 0.1 ")
            + HelpExampleRpc("grantwithmetadatafrom", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\", \"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\", admin, 48656C6C6F20576F726C64210A")
        ));
    
    mapHelpStrings.insert(std::make_pair("importaddress",
            "importaddress address(es) ( \"label\" rescan )\n"
            "\nAdds an address or script (in hex) that can be watched as if it were in your wallet but cannot be used to spend.\n"
            "\nArguments:\n"
            "1. \"address(es)\"                    (string, required) The addresses, comma delimited\n"
            " or\n"
            "1. address(es)                      (array, optional) A json array of addresses \n"                
            "2. \"label\"                          (string, optional, default=\"\") An optional label\n"
            "3. rescan                           (boolean or integer, optional, default=true) Rescan the wallet for transactions. \n"
            "                                                       If integer rescan from block, if negative - from the end.\n"
            "\nNote: This call can take minutes to complete if rescan is true.\n"
            "\nResult:\n"
            "\nExamples:\n"
            "\nImport an address with rescan\n"
            + HelpExampleCli("importaddress", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\"") +
            "\nImport using a label without rescan\n"
            + HelpExampleCli("importaddress", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" \"testing\" false") +
            "\nAs a JSON-RPC call\n"
            + HelpExampleRpc("importaddress", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\", \"testing\", false")
        ));
    
    mapHelpStrings.insert(std::make_pair("importprivkey",
            "importprivkey privkey(s) ( \"label\" rescan )\n"
            "\nAdds a private key (as returned by dumpprivkey) to your wallet.\n"
            "\nArguments:\n"
            "1. \"privkey(s)\"                     (string, required) The private key (see dumpprivkey), comma delimited\n"
            " or\n"
            "1. privkey(s)                       (array, optional) A json array of private keys \n"                
            "2. \"label\"                          (string, optional, default=\"\") An optional label\n"
            "3. rescan                           (boolean or integer, optional, default=true) Rescan the wallet for transactions. \n"
            "                                                       If integer rescan from block, if negative - from the end.\n"
            "\nNote: This call can take minutes to complete if rescan is true.\n"
            "\nResult:\n"
            "\nExamples:\n"
            "\nDump a private key\n"
            + HelpExampleCli("dumpprivkey", "\"myaddress\"") +
            "\nImport the private key with rescan\n"
            + HelpExampleCli("importprivkey", "\"mykey\"") +
            "\nImport using a label and without rescan\n"
            + HelpExampleCli("importprivkey", "\"mykey\" \"testing\" false") +
            "\nAs a JSON-RPC call\n"
            + HelpExampleRpc("importprivkey", "\"mykey\", \"testing\", false")
        ));
    
    mapHelpStrings.insert(std::make_pair("importwallet",
            "importwallet \"filename\" ( rescan )\n"
            "\nImports keys from a wallet dump file (see dumpwallet).\n"
            "\nArguments:\n"
            "1. \"filename\"                       (string, required) The wallet file\n"
            "2. rescan                           (boolean or integer, optional, default=true) Rescan the wallet for transactions. \n"
            "                                                       If integer rescan from block, if negative - from the end.\n"
            "\nExamples:\n"
            "\nDump the wallet\n"
            + HelpExampleCli("dumpwallet", "\"test\"") +
            "\nImport the wallet\n"
            + HelpExampleCli("importwallet", "\"test\"") +
            "\nImport using the json rpc call\n"
            + HelpExampleRpc("importwallet", "\"test\"")
        ));
    
    mapHelpStrings.insert(std::make_pair("issue",
            "issue \"address\" \"asset-name\"|asset-params quantity ( smallest-unit native-amount custom-fields )\n"
            "\nIssue new asset\n"
            + HelpRequiringPassphraseWrapper() +
            "\nArguments:\n"
            "1. \"address\"                        (string, required) The address to send newly created asset to.\n"
            "2. \"asset-name\"                     (string, required) Asset name, if not \"\" should be unique.\n"
            " or\n"
            "2. asset-params                     (object, required) A json object of with asset params\n"
            "    {\n"
            "      \"name\" : \"asset-name\"         (string, optional) Asset name\n"
            "      \"open\" : true|false           (boolean, optional, default false) True if follow-on issues are allowed\n"
            "      \"restrict\" : \"restrictions\"   (string, optional) Permission strings, comma delimited. Possible values: send,receive\n"
            "    }\n"                                
            "3. quantity                         (numeric, required) The asset total amount in display units. eg. 1234.56\n"
            "4. smallest-unit                    (numeric, optional, default=1) Number of raw units in one displayed unit, eg 0.01 for cents\n"
            "5. native-amount                    (numeric, optional) native currency amount to send. eg 0.1, Default: minimum-per-output.\n"
            "6  custom-fields                    (object, optional)  a json object with custom fields\n"
            "    {\n"
            "      \"param-name\": \"param-value\"   (strings, required) The key is the parameter name, the value is parameter value\n"
            "      ,...\n"
            "    }\n"
            "\nResult:\n"
            "\"transactionid\"                     (string) The transaction id.\n"
            "\nExamples:\n"
            + HelpExampleCli("issue", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" \"Dollar\" 1000000")
            + HelpExampleCli("issue", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" Dollar 1000000 0.01 0.01 ")
            + HelpExampleRpc("issue", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\", \"Dollar\", 1000000, 0.01, 0.01 \"description=1 Million dollars\"")
        ));
    
    mapHelpStrings.insert(std::make_pair("issuefrom",
            "issuefrom \"from-address\" \"to-address\" \"asset-name\"|asset-params quantity ( smallest-unit native-amount custom-fields )\n"
            "\nIssue asset using specific address\n"
            + HelpRequiringPassphraseWrapper() +
            "\nArguments:\n"
            "1. \"from-address\"                   (string, required) Address used for issuing.\n"
            "2. \"to-address\"                     (string, required) The  address to send newly created asset to.\n"
            "3. \"asset-name\"                     (string, required) Asset name, if not \"\" should be unique.\n"
            " or\n"
            "3. asset-params                     (object, required) A json object of with asset params\n"
            "    {\n"
            "      \"name\" : \"asset-name\"         (string, optional) Asset name\n"
            "      \"open\" : true|false           (boolean, optional, default false) True if follow-on issues are allowed\n"
            "      \"restrict\" : \"restrictions\"   (string, optional) Permission strings, comma delimited. Possible values: send,receive\n"
            "    }\n"                                
            "4. quantity                         (numeric, required) The asset total amount in display units. eg. 1234.56\n"
            "5. smallest-unit                    (numeric, optional, default=1) Number of raw units in one displayed unit, eg 0.01 for cents\n"
            "6. native-amount                    (numeric, optional) native currency amount to send. eg 0.1, Default: minimum-per-output.\n"
            "7  custom-fields                    (object, optional)  a json object with custom fields\n"
            "    {\n"
            "      \"param-name\": \"param-value\"   (strings, required) The key is the parameter name, the value is parameter value\n"
            "      ,...\n"
            "    }\n"
            "\nResult:\n"
            "\"transactionid\"                     (string) The transaction id.\n"
            "\nExamples:\n"
            + HelpExampleCli("issuefrom", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" \"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" \"Dollar\" 1000000")
            + HelpExampleCli("issuefrom", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" \"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" Dollar 1000000 0.01 0.01 ")
            + HelpExampleRpc("issuefrom", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\", \"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\", \"Dollar\", 1000000, 0.01, 0.01 \"description=1 Million dollars\"")
        ));
    
    mapHelpStrings.insert(std::make_pair("issuemore",
            "issuemore \"address\" \"asset-identifier\" quantity ( native-amount custom-fields )\n"
            "\nCreate more units for asset\n"
            + HelpRequiringPassphraseWrapper() +
            "\nArguments:\n"
            "1. \"address\"                        (string, required) The address to send newly created asset to.\n"
            "2. \"asset-identifier\"               (string, required) Asset identifier - one of: issue txid, asset reference, asset name.\n"
            "3. quantity                         (numeric, required) The asset total amount in display units. eg. 1234.56\n"
            "4. native-amount                    (numeric, optional) native currency amount to send. eg 0.1, Default: minimum-per-output.\n"
            "5  custom-fields                    (object, optional)  a json object with custom fields\n"
            "    {\n"
            "      \"param-name\": \"param-value\"   (strings, required) The key is the parameter name, the value is parameter value\n"
            "      ,...\n"
            "    }\n"
            "\nResult:\n"
            "\"transactionid\"                     (string) The transaction id.\n"
            "\nExamples:\n"
            + HelpExampleCli("issuemore", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" \"Dollar\" 1000000")
            + HelpExampleCli("issuemore", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" Dollar 1000000 0.01 ")
            + HelpExampleRpc("issuemore", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\", \"Dollar\", 1000000,  0.01 \"description=1 Million dollars\"")
        ));
    
    mapHelpStrings.insert(std::make_pair("issuemorefrom",
            "issuemorefrom \"from-address\" \"to-address\" \"asset-identifier\" quantity ( native-amount custom-fields )\n"
            "\nCreate more units for asset from specific address\n"
            + HelpRequiringPassphraseWrapper() +
            "\nArguments:\n"
            "1. \"from-address\"                   (string, required) Address used for issuing.\n"
            "2. \"to-address\"                     (string, required) The  address to send newly created asset to.\n"
            "3. \"asset-identifier\"               (string, required) Asset identifier - one of: issue txid, asset reference, asset name.\n"
            "4. quantity                         (numeric, required) The asset total amount in display units. eg. 1234.56\n"
            "5. native-amount                    (numeric, optional) native currency amount to send. eg 0.1, Default: minimum-per-output.\n"
            "6  custom-fields                    (object, optional)  a json object with custom fields\n"
            "    {\n"
            "      \"param-name\": \"param-value\"   (strings, required) The key is the parameter name, the value is parameter value\n"
            "      ,...\n"
            "    }\n"
            "\nResult:\n"
            "\"transactionid\"                     (string) The transaction id.\n"
            "\nExamples:\n"
            + HelpExampleCli("issuemorefrom", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" \"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" \"Dollar\" 1000000")
            + HelpExampleCli("issuemorefrom", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" \"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" Dollar 1000000 0.01 ")
            + HelpExampleRpc("issuemorefrom", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\", \"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\", \"Dollar\", 1000000, 0.01 \"description=1 Million dollars\"")
        ));
    
}

void mc_InitRPCHelpMap11()
{
    mapHelpStrings.insert(std::make_pair("keypoolrefill",
            "keypoolrefill ( newsize )\n"
            "\nFills the keypool."
            + HelpRequiringPassphraseWrapper() + 
            "\nArguments\n"
            "1. newsize                          (numeric, optional, default=100) The new keypool size\n"
            "\nExamples:\n"
            + HelpExampleCli("keypoolrefill", "")
            + HelpExampleRpc("keypoolrefill", "")
        ));
    
    mapHelpStrings.insert(std::make_pair("listaccounts",
            "listaccounts ( minconf includeWatchonly)\n"
            "\nReturns Object that has account names as keys, account balances as values.\n"
            "\nArguments:\n"
            "1. minconf                          (numeric, optional, default=1) Only include transactions with at least this many confirmations\n"
            "2. includeWatchonly                 (bool, optional, default=false) Include balances in watchonly addresses (see 'importaddress')\n"
            "\nResult:\n"
            "{                                   (json object where keys are account names, and values are numeric balances\n"
            "  \"account\": x.xxx,                 (numeric) The property name is the account name, and the value is the total balance for the account.\n"
            "  ...\n"
            "}\n"
            "\nExamples:\n"
            "\nList account balances where there at least 1 confirmation\n"
            + HelpExampleCli("listaccounts", "") +
            "\nList account balances including zero confirmation transactions\n"
            + HelpExampleCli("listaccounts", "0") +
            "\nList account balances for 6 or more confirmations\n"
            + HelpExampleCli("listaccounts", "6") +
            "\nAs json rpc call\n"
            + HelpExampleRpc("listaccounts", "6")
        ));
    
    mapHelpStrings.insert(std::make_pair("listaddresses",
            "listaddresses ( address(es) verbose count start ) \n"
            "\nReturns asset balances for specified address\n"
            "\nArguments:\n"
            "1. \"address(es)\"                    (string, optional, default=*) Address(es) to return information for, comma delimited. Default - all\n"
            " or\n"
            "1. address(es)                      (array, optional) A json array of addresses to return information for\n"                
            "2. verbose                          (boolean, optional, default=false) If true return more information about address.\n"
            "3. count                            (number, optional, default=INT_MAX - all) The number of addresses to display\n"
            "4. start                            (number, optional, default=-count - last) Start from specific address, 0 based, if negative - from the end\n"
            "\nResult:\n"
            "An array of address Objects.\n"
            "\nExamples:\n"
            + HelpExampleCli("listaddresses", "")
            + HelpExampleCli("listaddresses", "\"*\" true")
            + HelpExampleCli("listaddresses", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" true")
            + HelpExampleRpc("listaddresses", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\"")
        ));
    
    mapHelpStrings.insert(std::make_pair("listaddressgroupings",
            "listaddressgroupings\n"
            "\nLists groups of addresses which have had their common ownership\n"
            "made public by common use as inputs or as the resulting change\n"
            "in past transactions\n"
            "\nResult:\n"
            "[\n"
            "  [\n"
            "    [\n"
            "      \"address\",                    (string) The address\n"
            "      amount,                       (numeric) The amount in native currency\n"
            "      \"account\"                     (string, optional) The account\n"
            "    ]\n"
            "    ,...\n"
            "  ]\n"
            "  ,...\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("listaddressgroupings", "")
            + HelpExampleRpc("listaddressgroupings", "")
        ));
    
    mapHelpStrings.insert(std::make_pair("listaddresstransactions",
            "listaddresstransactions \"address\" ( count skip verbose )\n"
            "\nLists information about the <count> most recent transactions related to address in this node’s wallet.\n"
            "\nArguments:\n" 
            "1. \"address\"                        (string, required)  Address to list transactions for.\n"
            "2. count                            (numeric, optional, default=10) The number of transactions to return\n"
            "3. skip                             (numeric, optional, default=0) The number of transactions to skip\n"
            "4. verbose                          (boolean, optional, default=false) If true, returns detailed array of inputs and outputs and raw hex of transactions\n"
            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"balance\": {...},               (object)  Changes in address balance. \n"
            "    {\n"
            "      \"amount\": x.xxx,              (numeric) The amount in native currency. Negative value means amount was send by the wallet, positive - received\n"
            "      \"assets\": {...},              (object)  Changes in asset amounts. \n"
            "    }\n"
            "    \"myaddresses\": [...],           (array)   Address passed as parameter.   \n"
            "    \"addresses\": [...],             (array)   Array of counterparty addresses  involved in transaction  \n"
            "    \"permissions\": [...],           (array)   Changes in permissions \n"
            "    \"issue\": {...},                 (object)  Issue details  \n"
            "    \"data\" : \"metadata\",            (array)   Hexadecimal representation of metadata appended to the transaction\n"
            "    \"confirmations\": n,             (numeric)  The number of confirmations for the transaction. Available for 'send' and \n"
            "                                         'receive' category of transactions.\n"
            "    \"blockhash\": \"hashvalue\",       (string) The block hash containing the transaction. Available for 'send' and 'receive'\n"
            "                                          category of transactions.\n"
            "    \"blockindex\": n,                (numeric) The block index containing the transaction. Available for 'send' and 'receive'\n"
            "                                          category of transactions.\n"
            "    \"txid\": \"transactionid\",        (string) The transaction id. Available for 'send' and 'receive' category of transactions.\n"
            "    \"time\": xxx,                    (numeric) The transaction time in seconds since epoch (midnight Jan 1 1970 GMT).\n"
            "    \"timereceived\": xxx,            (numeric) The time received in seconds since epoch (midnight Jan 1 1970 GMT). Available \n"
            "                                          for 'send' and 'receive' category of transactions.\n"
            "    \"comment\": \"...\",               (string) If a comment is associated with the transaction.\n"
            "    \"vin\": [...],                   (array)  If verbose=true. Array of input details\n"
            "    \"vout\": [...],                  (array)  If verbose=true. Array of output details\n"
            "    \"hex\" : \"data\"                  (string) If verbose=true. Raw data for transaction\n"
            "  }\n"
            "]\n"

            "\nExamples:\n"
            "\nList the most recent 10 transactions in the systems\n"
            + HelpExampleCli("listaddresstransactions", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\"") +
            "\nList transactions 100 to 120 \n"
            + HelpExampleCli("listaddresstransactions", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" 20 100") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("listaddresstransactions", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\", 20, 100")
        ));
    
    mapHelpStrings.insert(std::make_pair("listassettransactions",
             "listassettransactions \"asset-identifier\" ( verbose count start local-ordering )\n"
            "\nLists transactions involving asset.\n"
            "\nArguments:\n"
            "1. \"asset-identifier\"               (string, required) Asset identifier - one of: issue txid, asset reference, asset name.\n"
            "2. verbose                          (boolean, optional, default=false) If true, returns information about transaction \n"
            "3. count                            (number, optional, default=10) The number of transactions to display\n"
            "4. start                            (number, optional, default=-count - last) Start from specific transaction, 0 based, if negative - from the end\n"
            "5. local-ordering                   (boolean, optional, default=false) If true, transactions appear in the order they were processed by the wallet,\n"
            "                                                                       if false - in the order they appear in blockchain\n"
            "\nResult:\n"
            "\"stream-items\"                      (array) List of transactions.\n"
            "\nExamples:\n"
            + HelpExampleCli("listassettransactions", "\"test-asset\"") 
            + HelpExampleCli("listassettransactions", "\"test-asset\" true 10 100") 
            + HelpExampleRpc("listassettransactions", "\"test-asset\", false, 20")
       ));
    
    mapHelpStrings.insert(std::make_pair("listlockunspent",
            "listlockunspent\n"
            "\nReturns list of temporarily unspendable outputs.\n"
            "See the lockunspent call to lock and unlock transactions for spending.\n"
            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"txid\" : \"transactionid\",       (string) The transaction id locked\n"
            "    \"vout\" : n                      (numeric) The vout value\n"
            "  }\n"
            "  ,...\n"
            "]\n"
            "\nExamples:\n"
            "\nList the unspent transactions\n"
            + HelpExampleCli("listunspent", "") +
            "\nLock an unspent transaction\n"
            + HelpExampleCli("lockunspent", "false \"[{\\\"txid\\\":\\\"a08e6907dbbd3d809776dbfc5d82e371b764ed838b5655e72f463568df1aadf0\\\",\\\"vout\\\":1}]\"") +
            "\nList the locked transactions\n"
            + HelpExampleCli("listlockunspent", "") +
            "\nUnlock the transaction again\n"
            + HelpExampleCli("lockunspent", "true \"[{\\\"txid\\\":\\\"a08e6907dbbd3d809776dbfc5d82e371b764ed838b5655e72f463568df1aadf0\\\",\\\"vout\\\":1}]\"") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("listlockunspent", "")
        ));
    
    mapHelpStrings.insert(std::make_pair("listreceivedbyaccount",
            "listreceivedbyaccount ( minconf includeempty includeWatchonly )\n"
            "\nList balances by account.\n"
            "\nArguments:\n"
            "1. minconf                          (numeric, optional, default=1) The minimum number of confirmations before payments are included.\n"
            "2. includeempty                     (boolean, optional, default=false) Whether to include accounts that haven't received any payments.\n"
            "3. includeWatchonly                 (bool, optional, default=false) Whether to include watchonly addresses (see 'importaddress').\n"

            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"involvesWatchonly\" : \"true\",   (bool) Only returned if imported addresses were involved in transaction\n"
            "    \"account\" : \"accountname\",      (string) The account name of the receiving account\n"
            "    \"amount\" : x.xxx,               (numeric) The total amount received by addresses with this account\n"
            "    \"confirmations\" : n             (numeric) The number of confirmations of the most recent transaction included\n"
            "  }\n"
            "  ,...\n"
            "]\n"

            "\nExamples:\n"
            + HelpExampleCli("listreceivedbyaccount", "")
            + HelpExampleCli("listreceivedbyaccount", "6 true")
            + HelpExampleRpc("listreceivedbyaccount", "6, true, true")
        ));
    
    mapHelpStrings.insert(std::make_pair("listreceivedbyaddress",
            "listreceivedbyaddress ( minconf includeempty includeWatchonly )\n"
            "\nList balances by receiving address.\n"
            "\nArguments:\n"
            "1. minconf                          (numeric, optional, default=1) The minimum number of confirmations before payments are included.\n"
            "2. includeempty                     (numeric, optional, default=false) Whether to include addresses that haven't received any payments.\n"
            "3. includeWatchonly                 (bool, optional, default=false) Whether to include watchonly addresses (see 'importaddress').\n"

            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"involvesWatchonly\" : \"true\",   (bool) Only returned if imported addresses were involved in transaction\n"
            "    \"address\" : \"receivingaddress\", (string) The receiving address\n"
            "    \"account\" : \"accountname\",      (string) The account of the receiving address. The default account is \"\".\n"
            "    \"amount\" : x.xxx,               (numeric) The total amount in native currency received by the address\n"
            "    \"confirmations\" : n             (numeric) The number of confirmations of the most recent transaction included\n"
            "  }\n"
            "  ,...\n"
            "]\n"

            "\nExamples:\n"
            + HelpExampleCli("listreceivedbyaddress", "")
            + HelpExampleCli("listreceivedbyaddress", "6 true")
            + HelpExampleRpc("listreceivedbyaddress", "6, true, true")
        ));
    
    mapHelpStrings.insert(std::make_pair("listsinceblock",
            "listsinceblock ( blockhash target-confirmations includeWatchonly )\n"
            "\nGet all transactions in blocks since block [blockhash], or all transactions if omitted\n"
            "\nArguments:\n"
            "1. blockhash                        (string, optional) The block hash to list transactions since\n"
            "2. target-confirmations:            (numeric, optional) The confirmations required, must be 1 or more\n"
            "3. includeWatchonly:                (bool, optional, default=false) Include transactions to watchonly addresses (see 'importaddress')"
            "\nResult:\n"
            "{\n"
            "  \"transactions\": [\n"
            "    \"account\":\"accountname\",        (string) The account name associated with the transaction. Will be \"\" for the default account.\n"
            "    \"address\":\"address\",            (string) The  address of the transaction. Not present for move transactions (category = move).\n"
            "    \"category\":\"send|receive\",      (string) The transaction category. 'send' has negative amounts, 'receive' has positive amounts.\n"
            "    \"amount\": x.xxx,                (numeric) The amount in btc. This is negative for the 'send' category, and for the 'move' category for moves \n"
            "                                          outbound. It is positive for the 'receive' category, and for the 'move' category for inbound funds.\n"
            "    \"vout\" : n,                     (numeric) the vout value\n"
            "    \"fee\": x.xxx,                   (numeric) The amount of the fee in btc. This is negative and only available for the 'send' category of transactions.\n"
            "    \"confirmations\": n,             (numeric) The number of confirmations for the transaction.\n"
            "                                              Available for 'send' and 'receive' category of transactions.\n"
            "    \"blockhash\": \"hashvalue\",       (string) The block hash containing the transaction. Available for 'send' and 'receive' category of transactions.\n"
            "    \"blockindex\": n,                (numeric) The block index containing the transaction. Available for 'send' and 'receive' category of transactions.\n"
            "    \"blocktime\": xxx,               (numeric) The block time in seconds since epoch (1 Jan 1970 GMT).\n"
            "    \"txid\": \"transactionid\",        (string) The transaction id. Available for 'send' and 'receive' category of transactions.\n"
            "    \"time\": xxx,                    (numeric) The transaction time in seconds since epoch (Jan 1 1970 GMT).\n"
            "    \"timereceived\": xxx,            (numeric) The time received in seconds since epoch (Jan 1 1970 GMT).\n"
            "                                              Available for 'send' and 'receive' category of transactions.\n"
            "    \"comment\": \"...\",               (string) If a comment is associated with the transaction.\n"
            "    \"to\": \"...\",                    (string) If a comment to is associated with the transaction.\n"
             "  ],\n"
            "  \"lastblock\": \"lastblockhash\"      (string) The hash of the last block\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("listsinceblock", "")
            + HelpExampleCli("listsinceblock", "\"000000000000000bacf66f7497b7dc45ef753ee9a7d38571037cdb1a57f663ad\" 6")
            + HelpExampleRpc("listsinceblock", "\"000000000000000bacf66f7497b7dc45ef753ee9a7d38571037cdb1a57f663ad\", 6")
        ));
    
}

void mc_InitRPCHelpMap12()
{
    mapHelpStrings.insert(std::make_pair("liststreamitems",
            "liststreamitems \"stream-identifier\" ( verbose count start local-ordering )\n"
            "\nReturns stream items.\n"
            "\nArguments:\n"
            "1. \"stream-identifier\"              (string, required) Stream identifier - one of: create txid, stream reference, stream name.\n"
            "2. verbose                          (boolean, optional, default=false) If true, returns information about item transaction \n"
            "3. count                            (number, optional, default=10) The number of items to display\n"
            "4. start                            (number, optional, default=-count - last) Start from specific item, 0 based, if negative - from the end\n"
            "5. local-ordering                   (boolean, optional, default=false) If true, items appear in the order they were processed by the wallet,\n"
            "                                                                       if false - in the order they appear in blockchain\n"
            "\nResult:\n"
            "\"stream-items\"                      (array) List of stream items.\n"
            "\nExamples:\n"
            + HelpExampleCli("liststreamitems", "\"test-stream\"") 
            + HelpExampleCli("liststreamitems", "\"test-stream\" true 10 100") 
            + HelpExampleRpc("liststreamitems", "\"test-stream\", false, 20")
        ));
    
    mapHelpStrings.insert(std::make_pair("liststreamkeyitems",
            "liststreamkeyitems \"stream-identifier\" \"key\" ( verbose count start local-ordering )\n"
            "\nReturns stream items for specific key.\n"
            "\nArguments:\n"
            "1. \"stream-identifier\"              (string, required) Stream identifier - one of: create txid, stream reference, stream name.\n"
            "2. \"key\"                            (string, required) Stream key\n"
            "3. verbose                          (boolean, optional, default=false) If true, returns information about item transaction \n"
            "4. count                            (number, optional, default=10) The number of items to display\n"
            "5. start                            (number, optional, default=-count - last) Start from specific item, 0 based, if negative - from the end\n"
            "6. local-ordering                   (boolean, optional, default=false) If true, items appear in the order they were processed by the wallet,\n"
            "                                                                       if false - in the order they appear in blockchain\n"
            "\nResult:\n"
            "\"stream-items\"                      (array) List of stream items for specific key.\n"
            "\nExamples:\n"
            + HelpExampleCli("liststreamkeyitems", "\"test-stream\" \"key01\"") 
            + HelpExampleCli("liststreamkeyitems", "\"test-stream\" \"key01\" true 10 100") 
            + HelpExampleRpc("liststreamkeyitems", "\"test-stream\", \"key01\", false 20")
        ));
    
    mapHelpStrings.insert(std::make_pair("liststreamkeys",
            "liststreamkeys \"stream-identifier\" ( key(s) verbose count start local-ordering )\n"
            "\nReturns stream keys.\n"
            "\nArguments:\n"
            "1. \"stream-identifier\"              (string, required) Stream identifier - one of: create txid, stream reference, stream name.\n"
            "2. \"key\"                            (string, optional, default=*) Stream key\n"
            " or\n"
            "2. key(s)                           (array, optional) A json array of stream keys \n"                
            "3. verbose                          (boolean, optional, default=false) If true, returns extended information about key \n"
            "4. count                            (number, optional, default=INT_MAX - all) The number of items to display\n"
            "5. start                            (number, optional, default=-count - last) Start from specific item, 0 based, if negative - from the end\n"
            "6. local-ordering                   (boolean, optional, default=false) If true, items appear in the order they were processed by the wallet,\n"
            "                                                                       if false - in the order they apppear in blockchain\n"
            "\nResult:\n"
            "\"stream-keys\"                       (array) List of stream keys.\n"
            "\nExamples:\n"
            + HelpExampleCli("liststreamkeys", "\"test-stream\" ") 
            + HelpExampleCli("liststreamkeys", "\"test-stream\" \"key01\"") 
            + HelpExampleCli("liststreamkeys", "\"test-stream\" \"*\" true 10 100") 
            + HelpExampleRpc("liststreamkeys", "\"test-stream\", \"key01\"")
        ));
    
    mapHelpStrings.insert(std::make_pair("liststreampublisheritems",
            "liststreampublisheritems \"stream-identifier\" \"address\" ( verbose count start local-ordering )\n"
            "\nReturns stream items for specific publisher.\n"
            "\nArguments:\n"
            "1. \"stream-identifier\"              (string, required) Stream identifier - one of: create txid, stream reference, stream name.\n"
            "2. \"address\"                        (string, required) Publisher address\n"
            "3. verbose                          (boolean, optional, default=false) If true, returns information about item transaction \n"
            "4. count                            (number, optional, default=10) The number of items to display\n"
            "5. start                            (number, optional, default=-count - last) Start from specific item, 0 based, if negative - from the end\n"
            "6. local-ordering                   (boolean, optional, default=false) If true, items appear in the order they were processed by the wallet,\n"
            "                                                                       if false - in the order they appear in blockchain\n"
            "\nResult:\n"
            "\"stream-items\"                      (array) List of stream items for specific publisher.\n"
            "\nExamples:\n"
            + HelpExampleCli("liststreampublisheritems", "\"test-stream\" \"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\"") 
            + HelpExampleCli("liststreampublisheritems", "\"test-stream\" \"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" true 10 100") 
            + HelpExampleRpc("liststreampublisheritems", "\"test-stream\", \"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\", false, 20")
        ));
    
    mapHelpStrings.insert(std::make_pair("liststreampublishers",
            "liststreampublishers \"stream-identifier\" ( address(es) verbose count start local-ordering )\n"
            "\nReturns stream publishers.\n"
            "\nArguments:\n"
            "1. \"stream-identifier\"              (string, required) Stream identifier - one of: create txid, stream reference, stream name.\n"
            "2. \"address(es)\"                    (string, optional, default=*) Publisher addresses, comma delimited\n"
            " or\n"
            "2. address(es)                      (array, optional) A json array of publisher addresses \n"                
            "3. verbose                          (boolean, optional, default=false) If true, returns extended information about publisher \n"
            "4. count                            (number, optional, default=INT_MAX - all) The number of items to display\n"
            "5. start                            (number, optional, default=-count - last) Start from specific item, 0 based, if negative - from the end\n"
            "6. local-ordering                   (boolean, optional, default=false) If true, items appear in the order they were processed by the wallet,\n"
            "                                                                       if false - in the order they appear in blockchain\n"
            "\nResult:\n"
            "\"stream-publishers\"                 (array) List of stream publishers.\n"
            "\nExamples:\n"
            + HelpExampleCli("liststreampublishers", "\"test-stream\" ") 
            + HelpExampleCli("liststreampublishers", "\"test-stream\" 1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd") 
            + HelpExampleCli("liststreampublishers", "\"test-stream\" \"*\" true 10 100") 
            + HelpExampleRpc("liststreampublishers", "\"test-stream\", \"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\"")
        ));
    
    mapHelpStrings.insert(std::make_pair("listtransactions",
            "listtransactions ( \"account\" count from includeWatchonly )\n"
            "\nReturns up to 'count' most recent transactions skipping the first 'from' transactions for account 'account'.\n"
            "\nArguments:\n"
            "1. \"account\"                        (string, optional) The account name. If not included, it will list all transactions for all accounts.\n"
            "                                                       If \"\" is set, it will list transactions for the default account.\n"
            "2. count                            (numeric, optional, default=10) The number of transactions to return\n"
            "3. from                             (numeric, optional, default=0) The number of transactions to skip\n"
            "4. includeWatchonly                 (bool, optional, default=false) Include transactions to watchonly addresses (see 'importaddress')\n"
            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"account\":\"accountname\",        (string) The account name associated with the transaction. \n"
            "                                              It will be \"\" for the default account.\n"
            "    \"address\":\"address\",            (string) The address of the transaction. Not present for \n"
            "                                              move transactions (category = move).\n"
            "    \"category\":\"send|receive|move\", (string) The transaction category. 'move' is a local (off blockchain)\n"
            "                                              transaction between accounts, and not associated with an address,\n"
            "                                              transaction id or block. 'send' and 'receive' transactions are \n"
            "                                              associated with an address, transaction id and block details\n"
            "    \"amount\": x.xxx,                (numeric)  The amount in btc. This is negative for the 'send' category, and for the\n"
            "                                              'move' category for moves outbound. It is positive for the 'receive' category,\n"
            "                                              and for the 'move' category for inbound funds.\n"
            "    \"vout\" : n,                     (numeric) the vout value\n"
            "    \"fee\": x.xxx,                   (numeric) The amount of the fee in btc. This is negative and only available for the \n"
            "                                              'send' category of transactions.\n"
            "    \"confirmations\": n,             (numeric) The number of confirmations for the transaction. Available for 'send' and \n"
            "                                              'receive' category of transactions.\n"
            "    \"blockhash\": \"hashvalue\",       (string) The block hash containing the transaction. Available for 'send' and 'receive'\n"
            "                                              category of transactions.\n"
            "    \"blockindex\": n,                (numeric) The block index containing the transaction. Available for 'send' and 'receive'\n"
            "                                              category of transactions.\n"
            "    \"txid\": \"transactionid\",        (string) The transaction id. Available for 'send' and 'receive' category of transactions.\n"
            "    \"time\": xxx,                    (numeric) The transaction time in seconds since epoch (midnight Jan 1 1970 GMT).\n"
            "    \"timereceived\": xxx,            (numeric) The time received in seconds since epoch (midnight Jan 1 1970 GMT). Available \n"
            "                                              for 'send' and 'receive' category of transactions.\n"
            "    \"comment\": \"...\",               (string) If a comment is associated with the transaction.\n"
            "    \"otheraccount\": \"accountname\",  (string) For the 'move' category of transactions, the account the funds came \n"
            "                                              from (for receiving funds, positive amounts), or went to (for sending funds,\n"
            "                                              negative amounts).\n"
            "  }\n"
            "]\n"

            "\nExamples:\n"
            "\nList the most recent 10 transactions in the systems\n"
            + HelpExampleCli("listtransactions", "") +
            "\nList the most recent 10 transactions for the tabby account\n"
            + HelpExampleCli("listtransactions", "\"tabby\"") +
            "\nList transactions 100 to 120 from the tabby account\n"
            + HelpExampleCli("listtransactions", "\"tabby\" 20 100") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("listtransactions", "\"tabby\", 20, 100")
        ));
    
    mapHelpStrings.insert(std::make_pair("listunspent",
            "listunspent ( minconf maxconf addresses )\n"
            "\nReturns array of unspent transaction outputs\n"
            "with between minconf and maxconf (inclusive) confirmations.\n"
            "Optionally filter to only include txouts paid to specified addresses.\n"
            "Results are an array of Objects, each of which has:\n"
            "{txid, vout, scriptPubKey, amount, confirmations}\n"
            "\nArguments:\n"
            "1. minconf                          (numeric, optional, default=1) The minimum confirmations to filter\n"
            "2. maxconf                          (numeric, optional, default=9999999) The maximum confirmations to filter\n"
            "3. addresses                        (array, optional) A json array of addresses to filter\n"
            "    [\n"
            "      \"address\"                     (string) address\n"
            "      ,...\n"
            "    ]\n"
            "\nResult\n"
            "[                                   (array of json object)\n"
            "  {\n"
            "    \"txid\" : \"txid\",                (string) the transaction id \n"
            "    \"vout\" : n,                     (numeric) the vout value\n"
            "    \"address\" : \"address\",          (string) the address\n"
            "    \"account\" : \"account\",          (string) The associated account, or \"\" for the default account\n"
            "    \"scriptPubKey\" : \"key\",         (string) the script key\n"
            "    \"amount\" : x.xxx,               (numeric) the transaction amount in btc\n"
            "    \"confirmations\" : n             (numeric) The number of confirmations\n"
            "  }\n"
            "  ,...\n"
            "]\n"

            "\nExamples\n"
            + HelpExampleCli("listunspent", "")
            + HelpExampleCli("listunspent", "6 9999999 \"[\\\"1PGFqEzfmQch1gKD3ra4k18PNj3tTUUSqg\\\",\\\"1LtvqCaApEdUGFkpKMM4MstjcaL4dKg8SP\\\"]\"")
            + HelpExampleRpc("listunspent", "6, 9999999 \"[\\\"1PGFqEzfmQch1gKD3ra4k18PNj3tTUUSqg\\\",\\\"1LtvqCaApEdUGFkpKMM4MstjcaL4dKg8SP\\\"]\"")
        ));
    
    mapHelpStrings.insert(std::make_pair("listwallettransactions",
            "listwallettransactions ( count skip includeWatchonly verbose )\n"
            "\nLists information about the <count> most recent transactions in this node’s wallet.\n"
            "\nArguments:\n"
            "1. count                            (numeric, optional, default=10) The number of transactions to return\n"
            "2. skip                             (numeric, optional, default=0) The number of transactions to skip\n"
            "3. includeWatchonly                 (bool, optional, default=false) Include transactions to watchonly addresses (see 'importaddress')\n"
            "4. verbose                          (bool, optional, default=false) If true, returns detailed array of inputs and outputs and raw hex of transactions\n"
            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"balance\": {...},               (object)  Changes in wallet balance. \n"
            "    {\n"
            "      \"amount\": x.xxx,              (numeric) The amount in native currency. Negative value means amount was send by the wallet, positive - received\n"
            "      \"assets\": {...},              (object)  Changes in asset amounts. \n"
            "    }\n"
            "    \"myaddresses\": [...],           (array)   Array of wallet addresses involved in transaction   \n"
            "    \"addresses\": [...],             (array)   Array of counterparty addresses  involved in transaction  \n"
            "    \"permissions\": [...],           (array)   Changes in permissions \n"
            "    \"issue\": {...},                 (object)  Issue details  \n"
            "    \"data\" : \"metadata\",            (array)   Hexadecimal representation of metadata appended to the transaction\n"
            "    \"confirmations\": n,             (numeric) The number of confirmations for the transaction. Available for 'send' and \n"
            "                                              'receive' category of transactions.\n"
            "    \"blockhash\": \"hashvalue\",       (string)   The block hash containing the transaction. Available for 'send' and 'receive'\n"
            "                                               category of transactions.\n"
            "    \"blockindex\": n,                (numeric)  The block index containing the transaction. Available for 'send' and 'receive'\n"
            "                                               category of transactions.\n"
            "    \"txid\": \"transactionid\",        (string) The transaction id. Available for 'send' and 'receive' category of transactions.\n"
            "    \"time\": xxx,                    (numeric)  The transaction time in seconds since epoch (midnight Jan 1 1970 GMT).\n"
            "    \"timereceived\": xxx,            (numeric)  The time received in seconds since epoch (midnight Jan 1 1970 GMT). Available \n"
            "                                               for 'send' and 'receive' category of transactions.\n"
            "    \"comment\": \"...\",               (string) If a comment is associated with the transaction.\n"
            "    \"vin\": [...],                   (array)  If verbose=true. Array of input details\n"
            "    \"vout\": [...],                  (array)  If verbose=true. Array of output details\n"
            "    \"hex\" : \"data\"                  (string) If verbose=true. Raw data for transaction\n"
            "  }\n"
            "]\n"

            "\nExamples:\n"
            "\nList the most recent 10 transactions in the systems\n"
            + HelpExampleCli("listwallettransactions", "") +
            "\nList transactions 100 to 120 \n"
            + HelpExampleCli("listwallettransactions", "20 100") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("listwallettransactions", "20, 100")
        ));
    
    mapHelpStrings.insert(std::make_pair("lockunspent",
            "lockunspent unlock [{\"txid\":\"txid\",\"vout\":n},...]\n"
            "\nUpdates list of temporarily unspendable outputs.\n"
            "Temporarily lock (unlock=false) or unlock (unlock=true) specified transaction outputs.\n"
            "A locked transaction output will not be chosen by automatic coin selection, when spending assetss.\n"
            "Locks are stored in memory only. Nodes start with zero locked outputs, and the locked output list\n"
            "is always cleared (by virtue of process exit) when a node stops or fails.\n"
            "Also see the listunspent call\n"
            "\nArguments:\n"
            "1. unlock                           (boolean, required) Whether to unlock (true) or lock (false) the specified transactions\n"
            "2. transactions                     (array, required) A json array of objects. Each object the txid (string) vout (numeric)\n"
            "     [                              (json array of json objects)\n"
            "       {\n"
            "         \"txid\":\"id\",               (string) The transaction id\n"
            "         \"vout\": n                  (numeric) The output number\n"
            "       }\n"
            "       ,...\n"
            "     ]\n"

            "\nResult:\n"
            "true|false                          (boolean) Whether the command was successful or not\n"

            "\nExamples:\n"
            "\nList the unspent transactions\n"
            + HelpExampleCli("listunspent", "") +
            "\nLock an unspent transaction\n"
            + HelpExampleCli("lockunspent", "false \"[{\\\"txid\\\":\\\"a08e6907dbbd3d809776dbfc5d82e371b764ed838b5655e72f463568df1aadf0\\\",\\\"vout\\\":1}]\"") +
            "\nList the locked transactions\n"
            + HelpExampleCli("listlockunspent", "") +
            "\nUnlock the transaction again\n"
            + HelpExampleCli("lockunspent", "true \"[{\\\"txid\\\":\\\"a08e6907dbbd3d809776dbfc5d82e371b764ed838b5655e72f463568df1aadf0\\\",\\\"vout\\\":1}]\"") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("lockunspent", "false, \"[{\\\"txid\\\":\\\"a08e6907dbbd3d809776dbfc5d82e371b764ed838b5655e72f463568df1aadf0\\\",\\\"vout\\\":1}]\"")
        ));
    
    mapHelpStrings.insert(std::make_pair("move",
            "move \"fromaccount\" \"toaccount\" amount ( minconf \"comment\" )\n"
            "\nMove a specified amount from one account in your wallet to another.\n"
            "\nArguments:\n"
            "1. \"fromaccount\"                    (string, required) The name of the account to move funds from. May be the default account using \"\".\n"
            "2. \"toaccount\"                      (string, required) The name of the account to move funds to. May be the default account using \"\".\n"
            "3. minconf                          (numeric, optional, default=1) Only use funds with at least this many confirmations.\n"
            "4. \"comment\"                        (string, optional) An optional comment, stored in the wallet only.\n"
            "\nResult:\n"
            "true|false                          (boolean) true if successfull.\n"
            "\nExamples:\n"
            "\nMove 0.01 btc from the default account to the account named tabby\n"
            + HelpExampleCli("move", "\"\" \"tabby\" 0.01") +
            "\nMove 0.01 btc timotei to akiko with a comment and funds have 6 confirmations\n"
            + HelpExampleCli("move", "\"timotei\" \"akiko\" 0.01 6 \"happy birthday!\"") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("move", "\"timotei\", \"akiko\", 0.01, 6, \"happy birthday!\"")
        ));
    
}

void mc_InitRPCHelpMap13()
{
    mapHelpStrings.insert(std::make_pair("preparelockunspent",
            "preparelockunspent asset-quantities ( lock )\n"
            "\nPrepares exchange transaction output for createrawexchange, appendrawexchange\n"
            + HelpRequiringPassphraseWrapper() +
            "\nArguments:\n"
            "1. asset-quantities                 (object, required) A json object of assets to send\n"
            "    {\n"
            "      \"asset-identifier\" : asset-quantity\n"
            "      ,...\n"
            "    }\n"                
            "2. lock                             (boolean, optional, default=true) Lock prepared unspent output\n"
            "\nResult:\n"
            "{\n"
            "  \"txid\": \"transactionid\",          (string) Transaction ID of the output which can be spent in createrawexchange or createrawexchange\n"
            "  \"vout\": n                         (numeric) Output index\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("preparelockunspent", "\"{\\\"12345-6789-1234\\\":100}\"")
            + HelpExampleCli("preparelockunspent", "\"{\\\"12345-6789-1234\\\":100,\\\"1234-5678-1234\\\":200}\"")
            + HelpExampleRpc("preparelockunspent",  "\"{\\\"12345-6789-1234\\\":100,\\\"1234-5678-1234\\\":200}\"")
        ));
    
    mapHelpStrings.insert(std::make_pair("preparelockunspentfrom",
            "preparelockunspentfrom \"from-address\" asset-quantities ( lock )\n"
            "\nPrepares exchange transaction output for createrawexchange, appendrawexchange using specific address\n"
            + HelpRequiringPassphraseWrapper() +
            "\nArguments:\n"
            "1. \"from-address\"                   (string, required) Address to send from .\n"
            "2. asset-quantities                 (object, required) A json object of assets to send\n"
            "    {\n"
            "      \"asset-identifier\" : asset-quantity\n"
            "      ,...\n"
            "    }\n"                
            "3. lock                             (boolean, optiona, default=true) Lock prepared unspent output\n"
            "\nResult:\n"
            "{\n"
            "  \"txid\": \"transactionid\",          (string) Transaction ID of the output which can be spent in createrawexchange or createrawexchange\n"
            "  \"vout\": n                         (numeric) Output index\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("preparelockunspentfrom", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" \"{\\\"12345-6789-1234\\\":100}\"")
            + HelpExampleCli("preparelockunspentfrom", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" \"{\\\"12345-6789-1234\\\":100,\\\"1234-5678-1234\\\":200}\"")
            + HelpExampleRpc("preparelockunspentfrom",  "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" \"{\\\"12345-6789-1234\\\":100,\\\"1234-5678-1234\\\":200}\"")
        ));
    
    mapHelpStrings.insert(std::make_pair("publish",
            "publish \"stream-identifier\" \"key\"|keys \"data-hex\"|data-obj \"options\" \n"
            "\nPublishes stream item\n"
            + HelpRequiringPassphraseWrapper() +
            "\nArguments:\n"
            "1. \"stream-identifier\"              (string, required) Stream identifier - one of: create txid, stream reference, stream name.\n"
            "2. \"key\"                            (string, required) Item key\n"
            " or\n"    
            "2. keys                             (array, required) Array of item keys\n"
            "3. \"data-hex\"                       (string, required) Data hex string\n"
            " or\n"
            "3. data-json                        (object, required) JSON data object\n"
            "    {\n"
            "      \"json\" : data-json            (object, required) Valid JSON object\n" 
            "    }\n"                                
            " or\n"
            "3. data-text                        (object, required) Text data object\n"
            "    {\n"
            "      \"text\" : \"data-text\"          (string, required) Data string\n" 
            "    }\n"                                
            " or\n"
            "3. data-cached                      (object, required) Binary raw data created with appendbinarycache\n"
            "    {\n"
            "      \"cache\" : \"identifier\"        (string, required) Binary cache identifier\n" 
            "    }\n"                                    
/*    
            "3. data-chunks                      (object, required) Offchain chunk hashes\n"
            "    {\n"
            "      \"chunks\" : chunk-hashes       (array, required) Array of chunk hashes created by storechunk\n" 
            "    }\n"                                
 */ 
            "4. \"options\"                        (string, optional) Should be \"offchain\" or omitted\n"
            "\nResult:\n"
            "\"transactionid\"                     (string) The transaction id.\n"
            "\nExamples:\n"
            + HelpExampleCli("publish", "test \"hello world\" 48656C6C6F20576F726C64210A")
            + HelpExampleRpc("publish", "\"test\", \"hello world\", \"48656C6C6F20576F726C64210A\"")
        ));
    
    mapHelpStrings.insert(std::make_pair("publishfrom",
            "publishfrom \"from-address\" \"stream-identifier\" \"key\"|keys \"data-hex\"|data-obj \"options\" \n"
            "\nPublishes stream item from specific address\n"
            + HelpRequiringPassphraseWrapper() +
            "\nArguments:\n"
            "1. \"from-address\"                   (string, required) Address used for publishsing.\n"
            "2. \"stream-identifier\"              (string, required) Stream identifier - one of: create txid, stream reference, stream name.\n"
            "3. \"key\"                            (string, required) Item key\n"
            " or\n"    
            "3. keys                             (array, required) Array of item keys\n"
            "4. \"data-hex\"                       (string, required) Data hex string\n"
            " or\n"
            "4. data-json                        (object, required) JSON data object\n"
            "    {\n"
            "      \"json\" : data-json            (object, required) Valid JSON object\n" 
            "    }\n"                                
            " or\n"
            "4. data-text                        (object, required) Text data object\n"
            "    {\n"
            "      \"text\" : \"data-text\"          (string, required) Data string\n" 
            "    }\n"                                
            " or\n"
            "4. data-cached                      (object, required) Binary raw data created with appendbinarycache\n"
            "    {\n"
            "      \"cache\" : \"identifier\"        (string, required) Binary cache identifier\n" 
            "    }\n"                                
/*    
            "4. data-chunks                      (object, required) Offchain chunk hashes\n"
            "    {\n"
            "      \"chunks\" : chunk-hashes       (array, required) Array of chunk hashes created by storechunk\n" 
            "    }\n"                                
 */ 
            "5. \"options\"                        (string, optional) Should be \"offchain\" or omitted\n"
            "\nResult:\n"
            "\"transactionid\"                     (string) The transaction id.\n"
            "\nExamples:\n"
            + HelpExampleCli("publishfrom", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" test \"hello world\" 48656C6C6F20576F726C64210A")
            + HelpExampleRpc("publishfrom", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\", \"test\", \"hello world\", \"48656C6C6F20576F726C64210A\"")
        ));
    
    mapHelpStrings.insert(std::make_pair("resendwallettransactions",
            "resendwallettransactions\n"
            "\nStop Resends wallet transactions."
        ));
    
    mapHelpStrings.insert(std::make_pair("revoke",
            "revoke \"address(es)\" \"permission(s)\" ( native-amount \"comment\" \"comment-to\" )\n"
            "\nRevoke permission from a given address. The amount is a real\n"
            + HelpRequiringPassphraseWrapper() +
            "\nArguments:\n"
            "1. \"address(es)\"                    (string, required) The addresses(es) to revoke permissions from\n"
            "2. \"permission(s)\"                  (string, required) Permission strings, comma delimited. \n"
            "                                                       Global: " + AllowedPermissions() + " \n"
            "                                                       or per-asset: asset-identifier.issue,admin,activate,send,receive \n"
            "                                                       or per-stream: stream-identifier.write,activate,admin \n"
            "3. native-amount                    (numeric, optional) native currency amount to send. eg 0.1. Default - 0\n"
            "4. \"comment\"                        (string, optional) A comment used to store what the transaction is for. \n"
            "                                                       This is not part of the transaction, just kept in your wallet.\n"
            "5. \"comment-to\"                     (string, optional) A comment to store the name of the person or organization \n"
            "                                                       to which you're sending the transaction. This is not part of the \n"
            "                                                       transaction, just kept in your wallet.\n"
            "\nResult:\n"
            "\"transactionid\"                     (string) The transaction id.\n"
            "\nExamples:\n"
            + HelpExampleCli("revoke", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" 0.1 connect,send,receive")
            + HelpExampleCli("revoke", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" 0.1 mine \"permission to mine\" \"Rogue Miner\"")
            + HelpExampleRpc("revoke", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\", 0.1, admin \"disabling temporary admin\", \"Admins Ltd.\"")
        ));
    
    mapHelpStrings.insert(std::make_pair("revokefrom",
            "revokefrom \"from-address\" \"to-address(es)\" \"permission(s)\" ( native-amount \"comment\" \"comment-to\" )\n"
            "\nRevoke permissions using specific address.\n"
            + HelpRequiringPassphraseWrapper() +
            "\nArguments:\n"
            "1. \"from-address\"                   (string, required) Addresses used for revoke.\n"
            "2. \"to-address(es)\"                 (string, required) The addresses(es) to revoke permissions from. Comma delimited\n"
            "3. \"permission(s)\"                  (string, required) Permission strings, comma delimited. \n"
            "                                                       Global: " + AllowedPermissions() + " \n"
            "                                                       or per-asset: asset-identifier.issue,admin,activate,send,receive \n"
            "                                                       or per-stream: stream-identifier.write,activate,admin \n"
            "4. native-amount                    (numeric, optional) native currency amount to send. eg 0.1. Default - 0\n"
            "5. \"comment\"                        (string, optional) A comment used to store what the transaction is for. \n"
            "                                                       This is not part of the transaction, just kept in your wallet.\n"
            "6. \"comment-to\"                     (string, optional) A comment to store the name of the person or organization \n"
            "                                                       to which you're sending the transaction. This is not part of the \n"
            "                                                       transaction, just kept in your wallet.\n"
            "\nResult:\n"
            "\"transactionid\"                     (string) The transaction id.\n"
            "\nExamples:\n"
            + HelpExampleCli("revokefrom", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" \"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" 0.1 connect,send,receive")
            + HelpExampleCli("revokefrom", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" \"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" 0.1 mine \"permission to mine\" \"Rogue Miner\"")
            + HelpExampleRpc("revokefrom", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" \"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\", 0.1, admin \"disabling temporary admin\", \"Admins Ltd.\"")
        ));
    
    mapHelpStrings.insert(std::make_pair("send",
            "send \"address\" amount|asset-quantities ( \"comment\" \"comment-to\" )\n"
            "\nSend an amount (or several asset amounts) to a given address. The amount is a real and is rounded to the nearest 0.00000001\n"
            + HelpRequiringPassphraseWrapper() +
            "\nArguments:\n"
            "1. address                          (string, required) The address to send to.\n"
            "2. amount                           (numeric, required) The amount in native currency to send. eg 0.1\n"
            " or\n"
            "2. asset-quantities                 (object, required) A json object of assets to send\n"
            "    {\n"
            "      \"asset-identifier\" : asset-quantity\n"
            "      ,...\n"
            "    }\n"                                
            "3. \"comment\"                        (string, optional) A comment used to store what the transaction is for. \n"
            "                                                       This is not part of the transaction, just kept in your wallet.\n"
            "4. \"comment-to\"                     (string, optional) A comment to store the name of the person or organization \n"
            "                                                       to which you're sending the transaction. This is not part of the \n"
            "                                                       transaction, just kept in your wallet.\n"
            "\nResult:\n"
            "\"transactionid\"                     (string) The transaction id.\n"
            "\nExamples:\n"
            + HelpExampleCli("send", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" \"{\\\"12345-6789-1234\\\":100,\\\"1234-5678-1234\\\":200}\"")
            + HelpExampleCli("send", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" 0.1 \"donation\" \"seans outpost\"")
            + HelpExampleRpc("send", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\", 0.1, \"donation\", \"seans outpost\"")
        ));
    
    mapHelpStrings.insert(std::make_pair("sendtoaddress",
            "sendtoaddress \"address\" amount|asset-quantities ( \"comment\" \"comment-to\" )\n"
            "\nSend an amount (or several asset amounts) to a given address. The amount is a real and is rounded to the nearest 0.00000001\n"
            + HelpRequiringPassphraseWrapper() +
            "\nArguments:\n"
            "1. \"address\"                        (string, required) The address to send to.\n"
            "2. amount                           (numeric, required) The amount in native currency to send. eg 0.1\n"
            " or\n"
            "2. asset-quantities                 (object, required) A json object of assets to send\n"
            "    {\n"
            "      \"asset-identifier\" : asset-quantity\n"
            "      ,...\n"
            "    }\n"                                
            "3. \"comment\"                        (string, optional) A comment used to store what the transaction is for. \n"
            "                                                       This is not part of the transaction, just kept in your wallet.\n"
            "4. \"comment-to\"                     (string, optional) A comment to store the name of the person or organization \n"
            "                                                       to which you're sending the transaction. This is not part of the \n"
            "                                                       transaction, just kept in your wallet.\n"
            "\nResult:\n"
            "\"transactionid\"                     (string) The transaction id.\n"
            "\nExamples:\n"
            + HelpExampleCli("sendtoaddress", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" \"{\\\"12345-6789-1234\\\":100,\\\"1234-5678-1234\\\":200}\"")
            + HelpExampleCli("sendtoaddress", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" 0.1 \"donation\" \"seans outpost\"")
            + HelpExampleRpc("sendtoaddress", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\", 0.1, \"donation\", \"seans outpost\"")
        ));
    
    mapHelpStrings.insert(std::make_pair("sendasset",
            "sendasset \"address\" \"asset-identifier\" asset-qty ( native-amount \"comment\" \"comment-to\" )\n"
            "\nSend asset amount to a given address. The amounts are real.\n"
            + HelpRequiringPassphraseWrapper() +
            "\nArguments:\n"
            "1. \"address\"                        (string, required) The address to send to.\n"
            "2. \"asset-identifier\"               (string, required) Asset identifier - one of: issue txid, asset reference, asset name.\n"
            "3. asset-qty                        (numeric, required) Asset quantity to send. eg 0.1\n"
            "4. native-amount                    (numeric, optional) native currency amount to send. eg 0.1, Default: minimum-per-output.\n"
            "5. \"comment\"                        (string, optional) A comment used to store what the transaction is for. \n"
            "                                                       This is not part of the transaction, just kept in your wallet.\n"
            "6. \"comment-to\"                     (string, optional) A comment to store the name of the person or organization \n"
            "                                                       to which you're sending the transaction. This is not part of the \n"
            "                                                       transaction, just kept in your wallet.\n"
            "\nResult:\n"
            "\"transactionid\"                     (string) The transaction id.\n"
            "\nExamples:\n"
            + HelpExampleCli("sendasset", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" 12345-6789-1234 100")
            + HelpExampleCli("sendasset", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" 12345-6789-1234 100 0.1 \"donation\" \"seans outpost\"")
            + HelpExampleRpc("sendasset", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\", 12345-6789-1234, 100, 0.1, \"donation\", \"seans outpost\"")
        ));
    
}

void mc_InitRPCHelpMap14()
{
    mapHelpStrings.insert(std::make_pair("sendassettoaddress",
            "sendassettoaddress \"address\" \"asset-identifier\" asset-qty ( native-amount \"comment\" \"comment-to\" )\n"
            "\nSend asset amount to a given address. The amounts are real.\n"
            + HelpRequiringPassphraseWrapper() +
            "\nArguments:\n"
            "1. \"address\"                        (string, required) The address to send to.\n"
            "2. \"asset-identifier\"               (string, required) Asset identifier - one of: issue txid, asset reference, asset name.\n"
            "3. asset-qty                        (numeric, required) Asset quantity to send. eg 0.1\n"
            "4. native-amount                    (numeric, optional) native currency amount to send. eg 0.1, Default: minimum-per-output.\n"
            "5. \"comment\"                        (string, optional) A comment used to store what the transaction is for. \n"
            "                                                       This is not part of the transaction, just kept in your wallet.\n"
            "6. \"comment-to\"                     (string, optional) A comment to store the name of the person or organization \n"
            "                                                       to which you're sending the transaction. This is not part of the \n"
            "                                                       transaction, just kept in your wallet.\n"
            "\nResult:\n"
            "\"transactionid\"                     (string) The transaction id.\n"
            "\nExamples:\n"
            + HelpExampleCli("sendassettoaddress", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" 12345-6789-1234 100")
            + HelpExampleCli("sendassettoaddress", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" 12345-6789-1234 100 0.1 \"donation\" \"seans outpost\"")
            + HelpExampleRpc("sendassettoaddress", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\", 12345-6789-1234, 100, 0.1, \"donation\", \"seans outpost\"")
        ));
    
    mapHelpStrings.insert(std::make_pair("sendassetfrom",
            "sendassetfrom \"from-address\" \"to-address\" \"asset-identifier\" asset-qty ( native-amount \"comment\" \"comment-to\" )\n"
            "\nSend an asset amount using specific address. \n" 
            + HelpRequiringPassphraseWrapper() + 
            "\nArguments:\n"
            "1. \"from-address\"                   (string, required) Address to send from. \n"
            "2. \"to-address\"                     (string, required) The address to send to.\n"
            "3. \"asset-identifier\"               (string, required) Asset identifier - one of: issue txid, asset reference, asset name.\n"
            "4. asset-qty                        (numeric, required) Asset quantity to send. eg 0.1\n"
            "5. native-amount                    (numeric, optional) native currency amount to send. eg 0.1, Default: minimum-per-output.\n"
            "6. \"comment\"                        (string, optional) A comment used to store what the transaction is for. \n"
            "                                                       This is not part of the transaction, just kept in your wallet.\n"
            "7. \"comment-to\"                     (string, optional) A comment to store the name of the person or organization \n"
            "                                                       to which you're sending the transaction. This is not part of the \n"
            "                                                       transaction, just kept in your wallet.\n"
            "\nResult:\n"
            "\"transactionid\"                     (string) The transaction id.\n"
            "\nExamples:\n"
            + HelpExampleCli("sendassetfrom", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" \"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" 0.1 12345-6789-1234 100")
            + HelpExampleCli("sendassetfrom", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" \"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" 0.1 12345-6789-1234 100 \"donation\" \"seans outpost\"")
            + HelpExampleRpc("sendassetfrom", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\", \"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\", 0.1, 12345-6789-1234, 100, \"donation\", \"seans outpost\"")
        ));
    
    mapHelpStrings.insert(std::make_pair("sendfrom",
            "sendfrom \"from-address\" \"to-address\" amount|asset-quantities ( \"comment\" \"comment-to\" )\n"
            "\nSend an amount (or several asset amounts) using specific address.\n"
            + HelpRequiringPassphraseWrapper() +
            "\nArguments:\n"
            "1. \"from-address\"                   (string, required) Address to send from.\n"
            "2. \"to-address\"                     (string, required) The address to send to.\n"
            "3. amount                           (numeric, required) The amount in native currency to send. eg 0.1\n"
            " or\n"
            "3. asset-quantities                 (object, required) A json object of assets to send\n"
            "    {\n"
            "      \"asset-identifier\" : asset-quantity\n"
            "      ,...\n"
            "    }\n"                                
            "4. \"comment\"                        (string, optional) A comment used to store what the transaction is for. \n"
            "                                                       This is not part of the transaction, just kept in your wallet.\n"
            "5. \"comment-to\"                     (string, optional) A comment to store the name of the person or organization \n"
            "                                                       to which you're sending the transaction. This is not part of the \n"
            "                                                       transaction, just kept in your wallet.\n"
            "\nResult:\n"
            "\"transactionid\"  (string) The transaction id.\n"
            "\nExamples:\n"
            + HelpExampleCli("sendfrom", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" \"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" \"{\\\"12345-6789-1234\\\":100,\\\"1234-5678-1234\\\":200}\"")
            + HelpExampleCli("sendfrom", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" \"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" 0.1 \"donation\" \"seans outpost\"")
            + HelpExampleRpc("sendfrom", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\", \"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\", 0.1, \"donation\", \"seans outpost\"")
         ));
    
    mapHelpStrings.insert(std::make_pair("sendfromaddress",
            "sendfrom \"from-address\" \"to-address\" amount|asset-quantities ( \"comment\" \"comment-to\" )\n"
            "\nSend an amount (or several asset amounts) using specific address.\n"
            + HelpRequiringPassphraseWrapper() +
            "\nArguments:\n"
            "1. \"from-address\"                   (string, required) Address to send from.\n"
            "2. \"to-address\"                     (string, required) The address to send to.\n"
            "3. amount                           (numeric, required) The amount in native currency to send. eg 0.1\n"
            " or\n"
            "3. asset-quantities                 (object, required) A json object of assets to send\n"
            "    {\n"
            "      \"asset-identifier\" : asset-quantity\n"
            "      ,...\n"
            "    }\n"                                
            "4. \"comment\"                        (string, optional) A comment used to store what the transaction is for. \n"
            "                                                       This is not part of the transaction, just kept in your wallet.\n"
            "5. \"comment-to\"                     (string, optional) A comment to store the name of the person or organization \n"
            "                                                       to which you're sending the transaction. This is not part of the \n"
            "                                                       transaction, just kept in your wallet.\n"
            "\nResult:\n"
            "\"transactionid\"                     (string) The transaction id.\n"
            "\nExamples:\n"
            + HelpExampleCli("sendfromaddress", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" \"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" \"{\\\"12345-6789-1234\\\":100,\\\"1234-5678-1234\\\":200}\"")
            + HelpExampleCli("sendfromaddress", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" \"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" 0.1 \"donation\" \"seans outpost\"")
            + HelpExampleRpc("sendfromaddress", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\", \"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\", 0.1, \"donation\", \"seans outpost\"")
         ));
    
    mapHelpStrings.insert(std::make_pair("sendfromaccount",
            "sendfromaccount \"fromaccount\" toaddress amount ( minconf \"comment\" \"comment-to\" )\n"
            "\nSent an amount from an account to a address.\n"
            "The amount is a real and is rounded to the nearest 0.00000001."
            + HelpRequiringPassphraseWrapper() +
            "\nArguments:\n"
            "1. \"fromaccount\"                    (string, required) The name of the account to send funds from. May be the default account using \"\".\n"
            "2. toaddress                        (string, required) The address to send funds to.\n"
            "3. amount                           (numeric, required) The amount in native currency. (transaction fee is added on top).\n"
            "4. minconf                          (numeric, optional, default=1) Only use funds with at least this many confirmations.\n"
            "5. \"comment\"                        (string, optional) A comment used to store what the transaction is for. \n"
            "                                                       This is not part of the transaction, just kept in your wallet.\n"
            "6. \"comment-to\"                     (string, optional) An optional comment to store the name of the person or organization \n"
            "                                                       to which you're sending the transaction. This is not part of the transaction, \n"
            "                                                       it is just kept in your wallet.\n"
            "\nResult:\n"
            "\"transactionid\"                     (string) The transaction id.\n"
            "\nExamples:\n"
            "\nSend 0.01 btc from the default account to the address, must have at least 1 confirmation\n"
            + HelpExampleCli("sendfromaccount", "\"\" \"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" 0.01") +
            "\nSend 0.01 from the tabby account to the given address, funds must have at least 6 confirmations\n"
            + HelpExampleCli("sendfromaccount", "\"tabby\" \"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" 0.01 6 \"donation\" \"seans outpost\"") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("sendfromaccount", "\"tabby\", \"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\", 0.01, 6, \"donation\", \"seans outpost\"")
        ));
    
    mapHelpStrings.insert(std::make_pair("sendmany",
            "sendmany \"fromaccount\" {\"address\":amount,...} ( minconf \"comment\" )\n"
            "\nSend multiple times. Amounts are double-precision floating point numbers."
            + HelpRequiringPassphraseWrapper() + 
            "\nArguments:\n"
            "1. \"fromaccount\"                    (string, required) The account to send the funds from, can be \"\" for the default account\n"
            "2. \"amounts\"                        (string, required) A json object with addresses and amounts\n"
            "    {\n"
            "      \"address\":amount              (numeric) The address is the key, the numeric amount in btc is the value\n"
            "      ,...\n"
            "    }\n"
            "3. minconf                          (numeric, optional, default=1) Only use the balance confirmed at least this many times.\n"
            "4. \"comment\"                        (string, optional) A comment\n"
            "\nResult:\n"
            "\"transactionid\"                     (string) The transaction id for the send. Only 1 transaction is created regardless of \n"
            "                                    the number of addresses.\n"
            "\nExamples:\n"
            "\nSend two amounts to two different addresses:\n"
            + HelpExampleCli("sendmany", "\"tabby\" \"{\\\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XZ\\\":0.01,\\\"1353tsE8YMTA4EuV7dgUXGjNFf9KpVvKHz\\\":0.02}\"") +
            "\nSend two amounts to two different addresses setting the confirmation and comment:\n"
            + HelpExampleCli("sendmany", "\"tabby\" \"{\\\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XZ\\\":0.01,\\\"1353tsE8YMTA4EuV7dgUXGjNFf9KpVvKHz\\\":0.02}\" 6 \"testing\"") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("sendmany", "\"tabby\", \"{\\\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XZ\\\":0.01,\\\"1353tsE8YMTA4EuV7dgUXGjNFf9KpVvKHz\\\":0.02}\", 6, \"testing\"")
        ));
    
    mapHelpStrings.insert(std::make_pair("sendwithdata",
            "sendwithdata \"address\" amount|asset-quantities data|publish-new-stream-item\n"
            "\nSend an amount (or several asset amounts) to a given address with appended metadata. \n"
            + HelpRequiringPassphraseWrapper() +
            "\nArguments:\n"
            "1. \"address\"                        (string, required) The address to send to.\n"
            "2. amount                           (numeric, required) The amount in native currency to send. eg 0.1\n"
            " or\n"
            "2. asset-quantities                 (object, required) A json object of assets to send\n"
            "    {\n"
            "      \"asset-identifier\" : asset-quantity\n"
            "      ,...\n"
            "    }\n"                                
            "3. data|publish-new-stream-item     (string or object, required) Data, see help data-with for details. \n"
            "\nResult:\n"
            "\"transactionid\"                     (string) The transaction id.\n"
            "\nExamples:\n"
            + HelpExampleCli("sendwithdata", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" \"{\\\"12345-6789-1234\\\":100,\\\"1234-5678-1234\\\":200}\" 48656C6C6F20576F726C64210A")
            + HelpExampleCli("sendwithdata", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" 0.1 48656C6C6F20576F726C64210A")
            + HelpExampleRpc("sendwithdata", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\", 0.1, 48656C6C6F20576F726C64210A")
        ));
    
    mapHelpStrings.insert(std::make_pair("sendwithmetadata",
            "sendwithmetadata \"address\" amount|asset-quantities data|publish-new-stream-item\n"
            "\nSend an amount (or several asset amounts) to a given address with appended metadata. \n"
            + HelpRequiringPassphraseWrapper() +
            "\nArguments:\n"
            "1. \"address\"                        (string, required) The address to send to.\n"
            "2. amount                           (numeric, required) The amount in native currency to send. eg 0.1\n"
            " or\n"
            "2. asset-quantities                 (object, required) A json object of assets to send\n"
            "    {\n"
            "      \"asset-identifier\" : asset-quantity\n"
            "      ,...\n"
            "    }\n"                                
            "3. data|publish-new-stream-item     (string or object, required) Data, see help data-with for details. \n"
            "\nResult:\n"
            "\"transactionid\"                     (string) The transaction id.\n"
            "\nExamples:\n"
            + HelpExampleCli("sendwithmetadata", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" \"{\\\"12345-6789-1234\\\":100,\\\"1234-5678-1234\\\":200}\" 48656C6C6F20576F726C64210A")
            + HelpExampleCli("sendwithmetadata", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" 0.1 48656C6C6F20576F726C64210A")
            + HelpExampleRpc("sendwithmetadata", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\", 0.1, 48656C6C6F20576F726C64210A")
        ));
    
    mapHelpStrings.insert(std::make_pair("sendwithdatafrom",
            "sendwithdatafrom \"from-address\" \"to-address\" amount|asset-quantities data|publish-new-stream-item\n"
            "\nSend an amount (or several asset amounts) using specific address.\n"
            + HelpRequiringPassphraseWrapper() +
            "\nArguments:\n"
            "1. \"from-address\"                   (string, required) Address to send from.\n"
            "2. \"to-address\"                     (string, required) The address to send to.\n"
            "3. amount                           (numeric, required) The amount in native currency to send. eg 0.1\n"
            " or\n"
            "3. asset-quantities                 (object, required) A json object of assets to send\n"
            "    {\n"
            "      \"asset-identifier\" : asset-quantity\n"
            "      ,...\n"
            "    }\n"                                
            "4. data|publish-new-stream-item     (string or object, required) Data, see help data-with for details. \n"
            "\nResult:\n"
            "\"transactionid\"                     (string) The transaction id.\n"
            "\nExamples:\n"
            + HelpExampleCli("sendwithdatafrom", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" \"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" \"{\\\"12345-6789-1234\\\":100,\\\"1234-5678-1234\\\":200}\" 48656C6C6F20576F726C64210A")
            + HelpExampleCli("sendwithdatafrom", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" \"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" 0.1 48656C6C6F20576F726C64210A")
            + HelpExampleRpc("sendwithdatafrom", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\", \"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\", 0.1, 48656C6C6F20576F726C64210A")
        ));
    
    mapHelpStrings.insert(std::make_pair("sendwithmetadatafrom",
            "sendwithmetadatafrom \"from-address\" \"to-address\" amount|asset-quantities data|publish-new-stream-item\n"
            "\nSend an amount (or several asset amounts) using specific address.\n"
            + HelpRequiringPassphraseWrapper() +
            "\nArguments:\n"
            "1. \"from-address\"                   (string, required) Address to send from.\n"
            "2. \"to-address\"                     (string, required) The address to send to.\n"
            "3. amount                           (numeric, required) The amount in native currency to send. eg 0.1\n"
            " or\n"
            "3. asset-quantities                 (object, required) A json object of assets to send\n"
            "    {\n"
            "      \"asset-identifier\" : asset-quantity\n"
            "      ,...\n"
            "    }\n"                                
            "4. data|publish-new-stream-item     (string or object, required) Data, see help data-with for details. \n"
            "\nResult:\n"
            "\"transactionid\"                     (string) The transaction id.\n"
            "\nExamples:\n"
            + HelpExampleCli("sendwithmetadatafrom", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" \"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" \"{\\\"12345-6789-1234\\\":100,\\\"1234-5678-1234\\\":200}\" 48656C6C6F20576F726C64210A")
            + HelpExampleCli("sendwithmetadatafrom", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" \"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" 0.1 48656C6C6F20576F726C64210A")
            + HelpExampleRpc("sendwithmetadatafrom", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\", \"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\", 0.1, 48656C6C6F20576F726C64210A")
        ));
    
}

void mc_InitRPCHelpMap15()
{
    mapHelpStrings.insert(std::make_pair("setaccount",
            "setaccount \"address\" \"account\"\n"
            "\nSets the account associated with the given address.\n"
            "\nArguments:\n"
            "1. \"address\"                        (string, required) The address to be associated with an account.\n"
            "2. \"account\"                        (string, required) The account to assign the address to.\n"
            "\nExamples:\n"
            + HelpExampleCli("setaccount", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XZ\" \"tabby\"")
            + HelpExampleRpc("setaccount", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XZ\", \"tabby\"")
        ));
    
    mapHelpStrings.insert(std::make_pair("settxfee",
            "settxfee amount\n"
            "\nSet the transaction fee per kB.\n"
            "\nArguments:\n"
            "1. amount                           (numeric, required) The transaction fee in <native currency>/kB rounded to the nearest 0.00000001\n"
            "\nResult\n"
            "true|false                          (boolean) Returns true if successful\n"
            "\nExamples:\n"
            + HelpExampleCli("settxfee", "0.00001")
            + HelpExampleRpc("settxfee", "0.00001")
        ));
    
    mapHelpStrings.insert(std::make_pair("signmessage",
            "signmessage \"address\"|\"privkey\" \"message\"\n"
            "\nSign a message with the private key of an address"
            + HelpRequiringPassphraseWrapper() + 
            "\nArguments:\n"
            "1. \"address\"                        (string, required) The address to use for the private key.\n"
            " or\n"    
            "1. \"privkey\"                        (string, required) The private key (see dumpprivkey and createkeypairs)\n"
            "2. \"message\"                        (string, required) The message to create a signature of.\n"
            "\nResult:\n"
            "\"signature\"                         (string) The signature of the message encoded in base 64\n"
            "\nExamples:\n"
            "\nUnlock the wallet for 30 seconds\n"
            + HelpExampleCli("walletpassphrase", "\"mypassphrase\" 30") +
            "\nCreate the signature\n"
            + HelpExampleCli("signmessage", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XZ\" \"my message\"") +
            "\nVerify the signature\n"
            + HelpExampleCli("verifymessage", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XZ\" \"signature\" \"my message\"") +
            "\nAs json rpc\n"
            + HelpExampleRpc("signmessage", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XZ\", \"my message\"")
        ));
    
    mapHelpStrings.insert(std::make_pair("subscribe",
            "subscribe entity-identifier(s) ( rescan parameters )\n"
            "\nSubscribes to the stream.\n"
            "\nArguments:\n"
            "1. \"stream-identifier\"              (string, required) Stream identifier - one of: create txid, stream reference, stream name.\n"
            " or\n"
            "1. \"asset-identifier\"               (string, required) Asset identifier - one of: issue txid, asset reference, asset name.\n"
            " or\n"
            "1. entity-identifier(s)             (array, optional) A json array of stream or asset identifiers \n"                
            "2. rescan                           (boolean, optional, default=true) Rescan the wallet for transactions\n"
            "3. \"parameters\"                     (string, optional) Available only in Enterprise Edition.\n"
            "                                                       Comma-delimited subset of: \n"
            "                                                         retrieve - automatically retrieve offchain items, \n"
            "                                                         items - build index for liststreamitems,\n"
            "                                                         keys - build index for liststreamkeys, liststreamkeyitems, getstreamkeysummary,\n"
            "                                                         publishers - build index for liststreampublishers, liststreampublisheritems, getstreampublishersummary,\n"
            "                                                         items-local - same as items, for local-ordering=true,\n"
            "                                                         keys-local - same as keys, for local-ordering=true,\n"
            "                                                         publishers-local - same as publishers, for local-ordering=true\n"
            "\nNote: This call can take minutes to complete if rescan is true.\n"
            "\nResult:\n"
            "\nExamples:\n"
            "\nSubscribe to the stream with rescan\n"
            + HelpExampleCli("subscribe", "\"test-stream\"") +
            "\nSubscribe to the stream without rescan\n"
            + HelpExampleCli("subscribe", "\"test-stream\" false") +
            "\nAs a JSON-RPC call\n"
            + HelpExampleRpc("subscribe", "\"test-stream\", false")
         ));
    
    mapHelpStrings.insert(std::make_pair("unsubscribe",
            "unsubscribe entity-identifier(s) ( purge )\n"
            "\nUnsubscribes from the stream.\n"
            "\nArguments:\n"
            "1. \"stream-identifier\"              (string, required) Stream identifier - one of: create txid, stream reference, stream name.\n"
            " or\n"
            "1. \"asset-identifier\"               (string, required) Asset identifier - one of: issue txid, asset reference, asset name.\n"
            " or\n"
            "1. entity-identifier(s)             (array, optional) A json array of stream or asset identifiers \n"                
            "2. purge                            (boolean, optional, default=false) Purge all offchain data for the stream\n"
            "\nResult:\n"
            "\nExamples:\n"
            + HelpExampleCli("unsubscribe", "\"test-stream\"")
            + HelpExampleRpc("unsubscribe", "\"test-stream\", false")
        ));
    
    mapHelpStrings.insert(std::make_pair("invalidateblock",
            "invalidateblock \"hash\"\n"
            "\nPermanently marks a block as invalid, as if it violated a consensus rule.\n"
            "\nArguments:\n"
            "1. hash                             (string, required) the hash of the block to mark as invalid\n"
            "\nResult:\n"
            "\nExamples:\n"
            + HelpExampleCli("invalidateblock", "\"blockhash\"")
            + HelpExampleRpc("invalidateblock", "\"blockhash\"")
        ));
    
    mapHelpStrings.insert(std::make_pair("reconsiderblock",
            "reconsiderblock \"hash\"\n"
            "\nRemoves invalidity status of a block and its descendants, reconsider them for activation.\n"
            "This can be used to undo the effects of invalidateblock.\n"
            "\nArguments:\n"
            "1. hash                             (string, required) the hash of the block to reconsider\n"
            "\nResult:\n"
            "\nExamples:\n"
            + HelpExampleCli("reconsiderblock", "\"blockhash\"")
            + HelpExampleRpc("reconsiderblock", "\"blockhash\"")
        ));
       
    mapHelpStrings.insert(std::make_pair("setmocktime",
            "setmocktime timestamp\n"
            "\nSet the local time to given timestamp (-regtest only)\n"
            "\nArguments:\n"
            "1. timestamp                        (integer, required) Unix seconds-since-epoch timestamp\n"
            "   Pass 0 to go back to using the system time."
        ));
       
    mapHelpStrings.insert(std::make_pair("getruntimeparams",
            "getruntimeparams \n"
            "\nReturns a selection of this node’s runtime parameters.\n"
            "\nResult:\n"
            "An object containing various runtime parameters\n"            
            "\nExamples:\n"
            + HelpExampleCli("getruntimeparams", "")
            + HelpExampleRpc("getruntimeparams", "")
        ));
    
    mapHelpStrings.insert(std::make_pair("setruntimeparam",
            "setruntimeparam \"parameter-name\" parameter-value \n"
            "\nSets value for runtime parameter\n"
            "\nArguments:\n"
            "1. \"parameter-name\"                 (string, required) Parameter name, one of:\n"
            "                                                       miningrequirespeers,\n"
            "                                                       mineemptyrounds,\n"
            "                                                       miningturnover,\n"
            "                                                       lockadminminerounds,\n"
            "                                                       maxshowndata, \n"
            "                                                       maxqueryscanitems, \n"
            "                                                       bantx,\n"
            "                                                       lockblock,\n"
            "                                                       autosubscribe,\n"
            "                                                       handshakelocal,\n"
            "                                                       hideknownopdrops\n"
            "                                                       acceptfiltertimeout\n"
            "                                                       sendfiltertimeout\n"
            "                                                       lockinlinemetadata\n"
            "2. parameter-value                  (required) parameter value\n"
            "\nResult:\n"
            "\nExamples:\n"
            + HelpExampleCli("setruntimeparam", "\"miningturnover\" 0.3")
            + HelpExampleRpc("setruntimeparam", "\"miningturnover\", 0.3")
        ));
    
}

void mc_InitRPCHelpMap16()
{
    mapHelpStrings.insert(std::make_pair("completerawexchange",
            "completerawexchange hex txid vout ask-assets ( data|publish-new-stream-item ) \n"
            "\nCompletes existing exchange transaction, adds fee if needed\n"
            "Returns hex-encoded raw transaction.\n"
            + HelpRequiringPassphraseWrapper() +
            "\nArguments:\n"
            "1. \"hex\"                            (string, required) The transaction hex string\n"
            "2. \"txid\"                           (string, required) Transaction ID of the output prepared by preparelockunspent.\n"
            "3. \"vout\"                           (numeric, required) Output index\n"
            "4. \"ask-assets\"                     (object, required) A json object of assets to ask\n"
            "    {\n"
            "      \"asset-identifier\" : asset-quantity\n"
            "      ,...\n"
            "    }\n"                
            "5. data|publish-new-stream-item     (string or object, optional) Data, see help data-with for details. \n"
            "\nResult:\n"
            "\"transaction\"                       (string) hex string of the transaction\n"
            "\nExamples:\n"
            + HelpExampleCli("completerawexchange", "\"hexstring\" f4c3dd510dd55761015c9d96bff7793b0d501dd6f01a959fd7dd02478fb47dfb 1 \"{\\\"1234-5678-1234\\\":200}\"" )
            + HelpExampleRpc("completerawexchange", "\"hexstring\",\"f4c3dd510dd55761015c9d96bff7793b0d501dd6f01a959fd7dd02478fb47dfb\",1,\"{\\\"1234-5678-1234\\\":200}\\\"")
        ));
    
    mapHelpStrings.insert(std::make_pair("approvefrom",
            "approvefrom \"from-address\" \"upgrade-identifier\"|\"filter-identifier\" ( approve )\n"
            "\nApprove upgrade using specific address.\n"
            + HelpRequiringPassphraseWrapper() +
            "\nArguments:\n"
            "1. \"from-address\"                   (string, required) Address used for approval.\n"
            "2. \"upgrade-identifier\"             (string, required) Upgrade identifier - one of: create txid, upgrade name.\n"
            "3. approve                          (boolean, required)  Approve or disapprove\n"
            " or\n"
            "1. \"from-address\"                   (string, required) Address used for approval.\n"
            "2. \"tx-filter-identifier\"           (string, required) Tx Filter identifier - one of: create txid, filter reference, filter name.\n"
            "3. approve                          (boolean, required)  Approve or disapprove\n"
            " or\n"
            "1. \"from-address\"                   (string, required) Address used for approval.\n"
            "2. \"stream-filter-identifier\"       (string, required) Stream Filter identifier - one of: create txid, filter reference, filter name.\n"
            "3. approve                          (object, required)  Approve or disapprove\n"
            "    {\n"                
            "      \"approve\" : approve           (boolean, required) Approve or disapprove\n"
            "      \"for\" : \"stream-identifier\"   (string, required)  Stream identifier - one of: create txid, stream reference, stream name.\n"
            "    }\n"                                
            "\nResult:\n"
            "\"transactionid\"                     (string) The transaction id.\n"
            "\nExamples:\n"
            + HelpExampleCli("approvefrom", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" \"upgrade1\"")
            + HelpExampleCli("approvefrom", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" \"upgrade1\" false")
            + HelpExampleRpc("approvefrom", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\", \"upgrade1\", true")
        ));
    
    mapHelpStrings.insert(std::make_pair("listupgrades",
            "listupgrades (upgrade-identifier(s))\n"
            "1. \"upgrade-identifier(s)\"          (string, optional, default=*) Upgrade identifier - one of:\n"
            "                                                                                create txid, upgrade name.\n"
            " or\n"
            "1. upgrade-identifier(s)            (array, optional) A json array of upgrade identifiers \n"                
            "\nReturns list of defined upgrades\n"
            "\nExamples:\n"
            + HelpExampleCli("listupgrades", "")
            + HelpExampleRpc("listupgrades", "")
        ));
    
    mapHelpStrings.insert(std::make_pair("appendrawtransaction",
            "appendrawtransaction \"tx-hex\" [{\"txid\":\"id\",\"vout\":n},...] ( {\"address\":amount,...} [data] \"action\" ) \n"
            "\nAppend inputs and outputs to raw transaction\n"

            "\nArguments:\n"
            "1. \"tx-hex\"                               (string, required) Source transaction hex string\n"
            "2. transactions                           (array, required) A json array of json objects\n"
            "     [\n"
            "       {\n"
            "         \"txid\":\"id\",                     (string, required) The transaction id\n"
            "         \"vout\":n                         (numeric, required) The output number\n"
            "         \"scriptPubKey\": \"hex\",           (string, optional) script key, used if cache=true or action=sign\n"
            "         \"redeemScript\": \"hex\"            (string, optional) redeem script, used if action=sign\n"
            "         \"cache\":true|false               (boolean, optional) If true - add cached script to tx, if omitted - add automatically if needed\n"    
            "       }\n"
            "       ,...\n"
            "     ]\n"
            "3. addresses                              (object, required) Object with addresses as keys, see help addresses-all for details.\n"
            "4. data                                   (array, optional) Array of hexadecimal strings or data objects, see help data-all for details.\n"
            "5.\"action\"                                (string, optional, default \"\") Additional actions: \"lock\", \"sign\", \"lock,sign\", \"sign,lock\", \"send\". \n"
                

            "\nResult:\n"
            "\"transaction\"                             (string) hex string of the transaction (if action= \"\" or \"lock\")\n"
            "  or \n"
            "{                                         (object) A json object (if action= \"sign\" or \"lock,sign\" or \"sign,lock\")\n"
            "  \"hex\": \"value\",                         (string) The raw transaction with signature(s) (hex-encoded string)\n"
            "  \"complete\": true|false                  (boolean) if transaction has a complete set of signature (0 if not)\n"
            "}\n"
            "  or \n"
            "\"hex\"                                     (string) The transaction hash in hex (if action= \"send\")\n"

            "\nExamples\n"
            + HelpExampleCli("appendrawtransaction", "\"hexstring\" \"[{\\\"txid\\\":\\\"myid\\\",\\\"vout\\\":0}]\" \"{\\\"address\\\":0.01}\"")
            + HelpExampleRpc("appendrawtransaction", "\"hexstring\", \"[{\\\"txid\\\":\\\"myid\\\",\\\"vout\\\":0}]\", \"{\\\"address\\\":0.01}\"")
        ));
    
    mapHelpStrings.insert(std::make_pair("listblocks",
            "listblocks block-set-identifier ( verbose )\n"
            "\nReturns list of block information objects\n"
            "\nArguments:\n"
            "1. \"block-set-identifier\"           (string, required) Comma delimited list of block identifiers: \n"
            "                                                       block height,\n"
            "                                                       block hash,\n"
            "                                                       block height range, e.g. <block-from>-<block-to>,\n"
            "                                                       number of last blocks in the active chain (if negative),\n"
            " or\n"
            "1. block-set-identifier             (array, required)  A json array of block identifiers \n"                
            " or\n"
            "1. block-set-identifier             (object, required) A json object with time range\n"
            "    {\n"                
            "      \"starttime\" : start-time      (numeric,required) Start time.\n"
            "      \"endtime\" : end-time          (numeric,required) End time.\n"
            "    }\n"                                
            "2. verbose                          (boolean, optional, default=false) If true, returns more information\n"
            "\nResult:\n"
            "An array containing list of block information objects\n"            
            "\nExamples:\n"
            + HelpExampleCli("listblocks", "\"1000,1100-1120\"")
            + HelpExampleCli("listblocks", "\"00000000c937983704a73af28acdec37b049d214adbda81d7e2a3dd146f6ed09\"")
            + HelpExampleRpc("listblocks", "1000")
        ));
    
    mapHelpStrings.insert(std::make_pair("liststreamblockitems",
            "liststreamblockitems \"stream-identifier\" block-set-identifier ( verbose count start )\n"
            "\nReturns stream items in certain block range.\n"
            "\nArguments:\n"
            "1. \"stream-identifier\"              (string, required) Stream identifier - one of: create txid, stream reference, stream name.\n"
            "2. \"block-set-identifier\"           (string, required) Comma delimited list of block identifiers: \n"
            "                                                       block height,\n"
            "                                                       block hash,\n"
            "                                                       block height range, e.g. <block-from>-<block-to>,\n"
            "                                                       number of last blocks in the active chain (if negative),\n"
            " or\n"
            "2. block-set-identifier             (array, required)  A json array of block identifiers \n"                
            " or\n"
            "2. block-set-identifier             (object, required) A json object with time range\n"
            "    {\n"                
            "      \"starttime\" : start-time      (numeric,required) Start time.\n"
            "      \"endtime\" : end-time          (numeric,required) End time.\n"
            "    }\n"                                
            "3. verbose                          (boolean, optional, default=false) If true, returns information about item transaction \n"
            "4. count                            (number, optional, default==INT_MAX) The number of items to display\n"
            "5. start                            (number, optional, default=-count - last) Start from specific item, 0 based, if negative - from the end\n"
            "\nResult:\n"
            "stream-items                        (array) List of stream items.\n"
            "\nExamples:\n"
            + HelpExampleCli("liststreamblockitems", "\"test-stream\" 1000,1100-1120 ") 
            + HelpExampleCli("liststreamblockitems", "\"test-stream\" 1000 true 10 100") 
            + HelpExampleRpc("liststreamblockitems", "\"test-stream\", 1000, false, 20")
        ));
    
    mapHelpStrings.insert(std::make_pair("liststreamtxitems",
            "liststreamtxitems \"stream-identifier\" txids ( verbose )\n"
            "\nReturns stream items.\n"
            "\nArguments:\n"
            "1. \"stream-identifier\"              (string, required) Stream identifier - one of: create txid, stream reference, stream name.\n"
            "2. \"txids\"                          (string, required) Transaction IDs, comma delimited\n"
            " or\n"
            "2. txids                            (array, required) Array of transaction IDs\n"
            "3. verbose                          (boolean, optional, default=false) If true, returns information about item transaction \n"
            "\nResult:\n"
            "\"stream-items\"                      (array) Array of stream items.\n"
            "\nExamples:\n"
            + HelpExampleCli("liststreamtxitems", "\"mytxid\"") 
            + HelpExampleCli("liststreamtxitems", "\"mytxid\"  true") 
            + HelpExampleRpc("liststreamtxitems", "\"mytxid\", false")
        ));
    
    mapHelpStrings.insert(std::make_pair("data-all",
            "Data parameter(s) appearing in appendrawdata, appendrawtransaction, createrawtransaction, createrawsendfrom \n\n"
            "\"data-hex\"                            (string, required) Data hex string\n"
            " or\n"
            "data-json                             (object, required) JSON data object\n"
            "    {\n"
            "      \"json\" : data-json              (object, required) Valid JSON object\n" 
            "    }\n"                                
            " or\n"
            "data-text                             (object, required) Text data object\n"
            "    {\n"
            "      \"text\" : \"data-text\"            (string, required) Data string\n" 
            "    }\n"                                
            " or\n"
            "data-cached                           (object, required) Binary raw data created with appendbinarycache\n"
            "    {\n"
            "      \"cache\" : \"identifier\"          (string, required) Binary cache identifier\n" 
            "    }\n"                                
            " or\n"
            "issue-details                         (object, required) A json object with issue metadata\n"
            "    {\n"
            "      \"create\" : \"asset\"              (string, required) asset\n" 
            "      \"name\" : \"asset-name\"           (string, optional) Asset name\n"
            "      \"multiple\" : n                  (numeric, optional, default 1) Number of raw units in one displayed unit\n"
            "      \"open\" : true|false             (boolean, optional, default false) True if follow-on issues are allowed\n"                
            "      \"restrict\" : \"restrictions\"     (string, optional) Permission strings, comma delimited. Possible values: send,receive\n"
            "      \"details\" :                     (object, optional) A json object with custom fields\n"           
            "        {\n"
            "          \"param-name\": \"param-value\" (strings, required) The key is the parameter name, the value is parameter value\n"
            "          ,...\n"
            "        }\n"
            "    }\n"                                
            " or\n"
            "issuemore-details                     (object, required) A json object with issuemore metadata\n"
            "    {\n"
            "      \"update\" : \"asset-identifier\"   (string, required) Asset identifier - one of: issue txid, asset reference, asset name.\n"
            "      \"details\" :                     (object, optional) A json object with custom fields\n"           
            "        {\n"
            "          \"param-name\": \"param-value\" (strings, required) The key is the parameter name, the value is parameter value\n"
            "          ,...\n"
            "        }\n"
            "    }\n"                                
            " or\n"
            "create-new-stream                     (object, required) A json object with new stream details\n"
            "    {\n"                
            "      \"create\" : \"stream\"             (string, required) stream\n"
            "      \"name\" : \"stream-name\"          (string, optional) Stream name\n"
            "      \"open\" : true|false             (boolean, optional, default: false) If true, anyone can publish\n"
            "      \"restrict\" : \"restrictions\"     (string, optional) Stream restrictions, comma delimited. Possible values: write,offchain,onchain\n"
            "      \"salted\" : true|false           (boolean, optional) Indicates whether offchain item chunk hashes should be salted\n"
            "      \"details\" :                     (object, optional) A json object with custom fields\n"           
            "        {\n"
            "          \"param-name\": \"param-value\" (strings, required) The key is the parameter name, the value is parameter value\n"
            "          ,...\n"
            "        }\n"
            "    }\n"                                
            " or\n"
            "publish-new-stream-item               (object, required) A json object with stream item\n"
            "    {\n"                
            "      \"for\" : \"stream-identifier\"     (string, required) Stream identifier - one of: create txid, stream reference, stream name.\n"
            "      \"options\" : \"options\"           (string, optional) Should be \"offchain\" or omitted\n"
            "      \"key\" : \"key\"                   (string, optional, default: \"\") Item key\n"
            "        or\n"
            "      \"keys\" : keys                   (array, optional) Item keys, array of strings\n"
            "      \"data\" : \"data-hex\"             (string, optional, default: \"\") Data hex string\n"
            "        or\n"
            "      \"data\" :                        (object, required) JSON data object\n"
            "        {\n"
            "          \"json\" : data-json          (object, required) Valid JSON string\n" 
            "        }\n"                                
            "        or\n"
            "      \"data\" :                        (object, required) Text data object\n"
            "        {\n"
            "          \"text\" : \"data-text\"        (string, required) Data string\n" 
            "        }\n"                                
            "        or\n"
            "      \"data\"                          (object, required) Binary raw data created with appendbinarycache\n"
            "        {\n"
            "          \"cache\" : \"identifier\"      (string, required) Binary cache identifier\n" 
            "        }\n"                              
/*    
            "      \"data\" :                        (object, required) Offchain chunk hashes\n"
            "        {\n"
            "          \"chunks\" : chunk-hashes     (array, required) Array of chunk hashes created by storechunk\n" 
            "        }\n"                                
 */ 
            "    }\n"                                
            " or\n"
            "create-new-upgrade                    (object, required) A json object with new upgrade details\n"
            "    {\n"                
            "      \"create\" : \"upgrade\"            (string, required) upgrade\n"
            "      \"name\" : \"upgrade-name\"         (string, optional) Upgrade name\n"
            "      \"startblock\" : n                (numeric, optional, default: 0) Block to apply upgrade from (inclusive).\n"
            "      \"details\" :                     (object, optional) A json object with custom fields\n"           
            "        {\n"
            "          \"protocol-version\": version (numeric, optional) Protocol version to upgrade to \n"
            "          \"parameter-name\": value     (numeric, optional) New value for upgradable parameter, one of: \n"
            "                                                          target-block-time,\n"
            "                                                          maximum-block-size,\n"
            "                                                          max-std-tx-size,\n"
            "                                                          max-std-op-returns-count,\n"
            "                                                          max-std-op-return-size,\n"
            "                                                          max-std-op-drops-count,\n"
            "                                                          max-std-element-size,\n"
            "                                                          anyone-can-connect,\n"
            "                                                          anyone-can-send,\n"
            "                                                          anyone-can-receive,\n"
            "                                                          anyone-can-receiveempty,\n"
            "                                                          anyone-can-issue,\n"
            "                                                          anyone-can-create,\n"
            "                                                          anyone-can-activate\n"
            "        }\n"
            "    }\n"                                
            " or\n"
            "approve-upgrade                       (object, required) A json object with approval details\n"
            "    {\n"                
            "      \"approve\" : approve             (boolean, required) Approve or disapprove\n"
            "      \"for\" : \"upgrade-identifier\"    (string, required)  Upgrade identifier - one of: create txid, upgrade name.\n"
            "    }\n"                                
            " or\n"
            "create-new-filter                     (object, required) A json object with new filter details\n"
            "    {\n"                
            "      \"create\" : \"filter-type\"        (string, required) txfilter or streamfilter\n"
            "      \"name\" : \"filter-name\"          (string, optional) Filter name\n"
            "      \"for\": \"entity-identifier\"      (string, optional) Only for tx filters, Asset/stream identifier - one of: create txid, stream reference, stream name.\n"
            "        or\n"
            "      \"for\": entity-identifier(s)     (array, optional) Only for tx filters, A json array of asset/stream identifiers .\n"    
            "      \"code\": \"filter-code\"           (string, required) JavaScript filter code, see help filters.\n"
            "    }\n"    
/*    
            " or\n"
            "create-new-variable                   (object, required) A json object with new variable details\n"
            "    {\n"                
            "      \"create\" : \"variable\"           (string, required) variable\n"
            "      \"name\" : \"variable-name\"        (string, optional) Variable name\n"
            "      \"value\": value                  (any data, optional, default null) Variable value (JSON objects and arrays allowed).\n"
            "    }\n"                                
            " or\n"
            "set-variable-value                    (object, required) A json object with new variable value\n"
            "    {\n"
            "      \"update\" : \"variable-identifier\" (string, required) Variable identifier - one of: create txid, variable reference, variable name.\n"
            "      \"value\": value                  (any data, optional, default null) Variable value (JSON objects and arrays allowed).\n"
            "    }\n"                                
 */ 
        ));

    mapHelpStrings.insert(std::make_pair("data-with",
            "Data parameter(s) appearing in completerawexchange, grantwithdata, grantwithdatafrom, sendwithdata, sendwithdatafrom\n\n"
            "\"data-hex\"                            (string, required) Data hex string\n"
            " or\n"
            "data-json                             (object, required) JSON data object\n"
            "    {\n"
            "      \"json\" : data-json              (object, required) Valid JSON object\n" 
            "    }\n"                                
            " or\n"
            "data-text                             (object, required) Text data object\n"
            "    {\n"
            "      \"text\" : \"data-text\"            (string, required) Data string\n" 
            "    }\n"                                
            " or\n"
            "data-cached                           (object, required) Binary raw data created with appendbinarycache\n"
            "    {\n"
            "      \"cache\" : \"identifier\"          (string, required) Binary cache identifier\n" 
            "    }\n"                                    
            " or\n"
            "publish-new-stream-item               (object, required) A json object with stream item\n"
            "    {\n"                
            "      \"for\" : \"stream-identifier\"     (string, required) Stream identifier - one of: create txid, stream reference, stream name.\n"
            "      \"options\" : \"options\"           (string, optional) Should be \"offchain\" or omitted\n"
            "      \"key\" : \"key\"                   (string, optional, default: \"\") Item key\n"
            "        or\n"
            "      \"keys\" : keys                   (array, optional) Item keys, array of strings\n"
            "      \"data\" : \"data-hex\"             (string, optional, default: \"\") Data hex string\n"
            "        or\n"
            "      \"data\" :                        (object, required) JSON data object\n"
            "        {\n"
            "          \"json\" : data-json          (object, required) Valid JSON string\n" 
            "        }\n"                                
            "        or\n"
            "      \"data\" :                        (object, required) JSON data object\n"
            "        {\n"
            "          \"text\" : \"data-text\"        (string, required) Data string\n" 
            "        }\n"                                
            "        or\n"
            "     \"data\" :                         (object, required) Binary raw data created with appendbinarycache\n"
            "        {\n"
            "          \"cache\" : \"identifier\"      (string, required) Binary cache identifier\n" 
            "        }\n"                                    
/*    
            "      \"data\" :                        (object, required) Text chunk hashes\n"
            "        {\n"
            "          \"chunks\" : chunk-hashes     (array, required) Array of chunk hashes created by storechunk\n" 
            "        }\n"                                
 */ 
            "    }\n"                                
        ));

     mapHelpStrings.insert(std::make_pair("addresses-all",
            "Addresses parameter(s) appearing in appendrawtransaction, createrawtransaction, createrawsendfrom,  \n\n"

            "{\n"
            "  \"address\":                           (string, required) Destination address\n"
            "   x.xxx                               (numeric, required) The value is the native currency amount\n"
            "     or \n"
            "   {                                   (object) A json object of assets to send\n"
            "      \"asset-identifier\" :             (string, required) Asset identifier - one of: issue txid, asset reference, asset name. \"\" for native currency.\n"
            "       asset-quantity                  (numeric, required) The asset value. \n"
            "     ,...\n"
            "   }\n"                                
            "      or \n"
            "   {                                   (object) A json object describing new asset issue\n"
            "     \"issue\" : \n"
            "       {\n"
            "          \"raw\" : n                    (numeric, required) The asset total amount in raw units \n"
            "       }\n"                                
            "   }\n"                                
            "      or \n"
            "   {                                   (object) A json object describing follow-on asset issue\n"
            "     \"issuemore\" : \n"
            "       {\n"
            "          \"asset\" : \"asset-identifier\" (string, required) Asset identifier - one of: issue txid. asset reference, asset name.\n"
            "          \"raw\" : n                    (numeric, required) The asset total amount in raw units \n"
            "       }\n"                                
            "   }\n"                                
            "      or \n"
            "   {                                   (object) A json object describing permission change\n"
            "      \"permissions\" : \n"
            "        {\n"
            "          \"type\" : \"permission(s)\"     (string, required) Permission strings, comma delimited. Possible values:\n"
            "                                                          " + AllowedPermissions() + " \n"
            "          \"for\": \"entity-identifier\"   (string, optional) Asset/stream identifier - one of: create txid, stream reference, stream name.\n"
            "          \"startblock\" : n             (numeric, optional) Block to apply permissions from (inclusive). Default - 0\n"
            "          \"endblock\"  : n              (numeric, optional) Block to apply permissions to (exclusive). Default - 4294967295\n"
            "          \"timestamp\" : n              (numeric, optional) This helps resolve conflicts between\n"
            "                                                           permissions assigned by the same administrator. Default - current time\n"
            "        }\n"                                
            "   }\n"                    
            "      or \n"
            "   {                                   (object) A json object describing inline data\n"
            "      \"data\" : \n"
            "        {\n"     
            "          \"data-hex\"                   (string, required) Data hex string\n"
            "             or\n"
            "          data-json                    (object, required) JSON data object\n"
            "            {\n"
            "              \"json\" : data-json       (object, required) Valid JSON object\n" 
            "            }\n"                                
            "             or\n"
            "          data-text                    (object, required) Text data object\n"
            "            {\n"
            "              \"text\" : \"data-text\"     (string, required) Data string\n" 
            "            }\n"                                
            "             or\n"
            "          data-cached                  (object, required) Binary raw data created with appendbinarycache\n"
            "            {\n"
            "              \"cache\" : \"identifier\"   (string, required) Binary cache identifier\n" 
            "            }\n"                                    
            "        }\n"                    
            "   }\n"                    
     
            " ,...\n"
            "}\n"
            "  or \n"
            "{\n"
            "  \"tx-filter-identifier\":              (string, required) Tx Filter identifier - one of: create txid, filter reference, filter name.\n"
            "   {\n"
            "     \"approve\" : approve               (boolean, required)  Approve or disapprove\n"
            "   }\n"                                
            "}\n"
            "  or \n"
            "{\n"
            "  \"stream-filter-identifier\":          (string, required) Stream Filter identifier - one of: create txid, filter reference, filter name.\n"
            "   {\n"
            "     \"approve\" : approve               (boolean, required) Approve or disapprove\n"
            "     \"for\" : \"stream-identifier\"       (string, required)  Stream identifier - one of: create txid, stream reference, stream name.\n"
            "   }\n"                                
            "}\n"
        ));
    
}

void mc_InitRPCHelpMap17()
{
     mapHelpStrings.insert(std::make_pair("getstreamkeysummary",
            "getstreamkeysummary \"stream-identifier\" \"key\" \"mode\"\n"
            "\nReturns stream json object items summary for specific key.\n"
            "\nArguments:\n"
            "1. \"stream-identifier\"              (string, required) Stream identifier - one of: create txid, stream reference, stream name.\n"
            "2. \"key\"                            (string, required) Stream key\n"
            "3. \"mode\"                           (string, required) Comma delimited list of the following:\n"
            "                                                       jsonobjectmerge (required) - merge json objects\n"
            "                                                       recursive - merge json sub-objects recursively\n"
            "                                                       noupdate -  preserve first value for each key instead of taking the last\n"
            "                                                       omitnull - omit keys with null values\n"
            "                                                       ignoreother - ignore items that cannot be included in summary (otherwise returns an error)\n"
            "                                                       ignoremissing - ignore missing offchain items (otherwise returns an error)\n"
            "                                                       firstpublishersany - only summarize items by a publisher of first item with this key\n"
            "                                                       firstpublishersall - only summarize items by all publishers of first item with this key\n"
            "\nResult:\n"
            "summary-object                      (object) Summary object for specific key.\n"
            "\nExamples:\n"
            + HelpExampleCli("getstreamkeysummary", "\"test-stream\" \"key01\" \"jsonobjectmerge\"") 
            + HelpExampleCli("getstreamkeysummary", "\"test-stream\" \"key01\" \"jsonobjectmerge,ignore,recursive\"") 
            + HelpExampleRpc("getstreamkeysummary", "\"test-stream\", \"key01\", \"jsonobjectmerge,ignore,recursive\"")
        ));
    
   
     mapHelpStrings.insert(std::make_pair("getstreampublishersummary",
            "getstreampublishersummary \"stream-identifier\" \"address\" \"mode\"\n"
            "\nReturns stream json object items summary for specific publisher.\n"
            "\nArguments:\n"
            "1. \"stream-identifier\"              (string, required) Stream identifier - one of: create txid, stream reference, stream name.\n"
            "2. \"address\"                        (string, required) Publisher address\n"
            "3. \"mode\"                           (string, required) Comma delimited list of the following:\n"
            "                                                       jsonobjectmerge (required) - merge json objects\n"
            "                                                       recursive - merge json sub-objects recursively\n"
            "                                                       noupdate -  preserve first value for each key instead of taking the last\n"
            "                                                       omitnull - omit keys with null values\n"
            "                                                       ignoreother - ignore items that cannot be included in summary (otherwise returns an error)\n"
            "                                                       ignoremissing - ignore missing offchain items (otherwise returns an error)\n"
            "\nResult:\n"
            "summary-object                      (object) Summary object for specific publisher.\n"
            "\nExamples:\n"
            + HelpExampleCli("liststreampublisheritems", "\"test-stream\" \"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" \"jsonobjectmerge\"") 
            + HelpExampleCli("liststreampublisheritems", "\"test-stream\" \"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" \"jsonobjectmerge,ignore,recursive\"") 
            + HelpExampleRpc("liststreampublisheritems", "\"test-stream\", \"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\", \"jsonobjectmerge,ignore,recursive\"")
        ));
    
     mapHelpStrings.insert(std::make_pair("storechunk",
            "storechunk \"data-hex\" \n"
            "\nStores chunk of data in local wallet. Returns hash of the data, which can be used later when publishing offchain stream items.\n"
            "\nArguments:\n"
            "1. \"data-hex\"                       (string, required) The hex string of the data chunk\n"
            "\nResult:\n"
            "\"hex\"                               (string) The chunk hash in hex\n"
            "\nExamples:\n"
            + HelpExampleCli("storechunk", "\"f4c3dd510dd55761015c9d96bff7793b0d501dd6f01a959fd7\"") 
            + HelpExampleRpc("storechunk", "\"f4c3dd510dd55761015c9d96bff7793b0d501dd6f01a959fd7\"")
        ));
    
     mapHelpStrings.insert(std::make_pair("createbinarycache",
            "createbinarycache \n"
            "\nReturns random string, which can be used as binary cache item identifier\n"
            "\nArguments:\n"
            "\nResult:\n"
            "\"identifier\"                               (string) Binary cache item identifier\n"
            "\nExamples:\n"
            + HelpExampleCli("createbinarycache","") 
            + HelpExampleRpc("createbinarycache","")
        ));
    
     mapHelpStrings.insert(std::make_pair("appendbinarycache",
            "appendbinarycache \"identifier\" \"data-hex\" \n"
            "\nAppends data to binary cache.\n"
            "\nArguments:\n"
            "1. \"identifier\"                     (string, required) Binary cache item identifier\n"
            "2. \"data-hex\"                       (string, required) The hex string to be added to binary cache item\n"
            "\nResult:\n"
            "size                                (numeric) Size of the binary cache item\n"
            "\nExamples:\n"
            + HelpExampleCli("appendbinarycache", "\"TjnVWwHYEg4\" \"f4c3dd510dd55761015c9d96bff7793b0d501dd6f01a959fd7\"") 
            + HelpExampleRpc("appendbinarycache", "\"TjnVWwHYEg4\",\"f4c3dd510dd55761015c9d96bff7793b0d501dd6f01a959fd7\"")
        ));
    
     mapHelpStrings.insert(std::make_pair("deletebinarycache",
            "deletebinarycache \"identifier\"  \n"
            "\nClear binary cache item\n"
            "\nArguments:\n"
            "1. \"identifier\"                     (string, required) Binary cache item identifier, \"*\" - to clear all items\n"
            "\nResult:\n"
            "\nExamples:\n"
            + HelpExampleCli("deletebinarycache", "\"TjnVWwHYEg4\"") 
            + HelpExampleRpc("deletebinarycache", "\"TjnVWwHYEg4\"")
        ));
    
    mapHelpStrings.insert(std::make_pair("walletpassphrase",
            "walletpassphrase \"passphrase\" timeout\n"
            "\nStores the wallet decryption key in memory for 'timeout' seconds.\n"
            "This is needed prior to performing transactions related to private keys such as sending assets\n"
            "\nArguments:\n"
            "1. \"passphrase\"                     (string, required) The wallet passphrase\n"
            "2. timeout                          (numeric, required) The time to keep the decryption key in seconds.\n"
            "\nNote:\n"
            "Issuing the walletpassphrase command while the wallet is already unlocked will set a new unlock\n"
            "time that overrides the old one.\n"
            "\nExamples:\n"
            "\nunlock the wallet for 60 seconds\n"
            + HelpExampleCli("walletpassphrase", "\"my pass phrase\" 60") +
            "\nLock the wallet again (before 60 seconds)\n"
            + HelpExampleCli("walletlock", "") +
            "\nAs json rpc call\n"
            + HelpExampleRpc("walletpassphrase", "\"my pass phrase\", 60")
        ));
    
    
    mapHelpStrings.insert(std::make_pair("walletpassphrasechange",
            "walletpassphrasechange \"oldpassphrase\" \"newpassphrase\"\n"
            "\nChanges the wallet passphrase from 'oldpassphrase' to 'newpassphrase'.\n"
            "\nArguments:\n"
            "1. \"oldpassphrase\"                  (string, required) The current passphrase\n"
            "2. \"newpassphrase\"                  (string, required) The new passphrase\n"
            "\nExamples:\n"
            + HelpExampleCli("walletpassphrasechange", "\"old one\" \"new one\"")
            + HelpExampleRpc("walletpassphrasechange", "\"old one\", \"new one\"")
        ));
    
    mapHelpStrings.insert(std::make_pair("walletlock",
            "walletlock\n"
            "\nRemoves the wallet encryption key from memory, locking the wallet.\n"
            "After calling this method, you will need to call walletpassphrase again\n"
            "before being able to call any methods which require the wallet to be unlocked.\n"
            "\nExamples:\n"
            "\nSet the passphrase for 2 minutes to perform a transaction\n"
            + HelpExampleCli("walletpassphrase", "\"my pass phrase\" 120") +
            "\nPerform a send (requires passphrase set)\n"
            + HelpExampleCli("sendtoaddress", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" 1.0") +
            "\nClear the passphrase since we are done before 2 minutes is up\n"
            + HelpExampleCli("walletlock", "") +
            "\nAs json rpc call\n"
            + HelpExampleRpc("walletlock", "")
        ));

     
    mapHelpStrings.insert(std::make_pair("getchunkqueueinfo",
            "getchunkqueueinfo\n"
            "\nReturns data about each current chunk queue status.\n"
            "\nExamples:\n"
            + HelpExampleCli("getchunkqueueinfo", "")
            + HelpExampleRpc("getchunkqueueinfo", "")
        ));
    
}

void mc_InitRPCHelpMap18()
{
    mapHelpStrings.insert(std::make_pair("getchunkqueuetotals",
            "getchunkqueuetotals\n"
            "\nReturns chunks delivery statistics.\n"
            "\nExamples:\n"
            + HelpExampleCli("getchunkqueuetotals", "")
            + HelpExampleRpc("getchunkqueuetotals", "")
        ));
    
    
     mapHelpStrings.insert(std::make_pair("liststreamqueryitems",
            "liststreamqueryitems \"stream-identifier\" query ( verbose )\n"
            "\nReturns stream items for specific query.\n"
            "\nArguments:\n"
            "1. \"stream-identifier\"              (string, required) Stream identifier - one of: create txid, stream reference, stream name.\n"
            "2. query                            (object, required) Query\n"
            "    {\n"                
            "      \"key\" : \"key\"                   (string, optional, default: \"\") Item key\n"
            "        or\n"
            "      \"keys\" : keys                   (array, optional) Item keys, array of strings\n"
            "        and/or\n"
            "      \"publisher\" : \"publisher\"       (string, optional, default: \"\") Publisher\n"
            "        or\n"
            "      \"publishers\" : publishers       (array, optional) Publishers, array of strings\n"
            "    }\n"                                
            "3. verbose                          (boolean, optional, default=false) If true, returns information about item transaction \n"
            "\nResult:\n"
            "\"stream-items\"                      (array) List of stream items for specific query.\n"
            "\nExamples:\n"
            + HelpExampleCli("liststreamqueryitems", "\"test-stream\" \"{\\\"keys\\\":[\\\"key01\\\",\"key02\"]}\"") 
            + HelpExampleCli("liststreamqueryitems", "\"test-stream\" \"{\\\"keys\\\":[\\\"key01\\\",\"key02\"],\\\"publisher\\\":\\\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XZ\\\"}\" true ") 
            + HelpExampleRpc("liststreamqueryitems", "\"test-stream\", \"{\\\"keys\\\":[\\\"key01\\\",\"key02\"],\\\"publisher\\\":\\\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XZ\\\"}\", false")
        ));
    
     mapHelpStrings.insert(std::make_pair("listtxfilters",
            "listtxfilters ( filter-identifier(s) verbose )\n"
            "\nReturns list of defined tx filters\n"
            "\nArguments:\n"
            "1. \"filter-identifier\"              (string, optional, default=*) Filter identifier - one of: create txid, filter reference, filter name.\n"
            " or\n"
            "1. filter-identifier(s)             (array, optional) A json array of filter identifiers \n"                
            "2. verbose                          (boolean, optional, default=false) If true, returns list of creators and approval details \n"
            "\nResult:\n"
            "An array containing list of defined tx filters\n"            
            "\nExamples:\n"
            + HelpExampleCli("listtxfilters", "")
            + HelpExampleRpc("listtxfilters", "")
        ));
     
     mapHelpStrings.insert(std::make_pair("liststreamfilters",
            "liststreamfilters ( filter-identifier(s) verbose )\n"
            "\nReturns list of defined stream filters\n"
            "\nArguments:\n"
            "1. \"filter-identifier\"              (string, optional, default=*) Filter identifier - one of: create txid, filter reference, filter name.\n"
            " or\n"
            "1. filter-identifier(s)             (array, optional) A json array of filter identifiers \n"                
            "2. verbose                          (boolean, optional, default=false) If true, returns list of creators and approval details \n"
            "\nResult:\n"
            "An array containing list of defined stream filters\n"            
            "\nExamples:\n"
            + HelpExampleCli("liststreamfilters", "")
            + HelpExampleRpc("liststreamfilters", "")
        ));
     
     mapHelpStrings.insert(std::make_pair("getfiltercode",
            "getfiltercode \"filter-identifier\"\n"
            "\nReturns code for specified filter\n"
            "\nArguments:\n"
            "1. \"filter-identifier\"              (string, required) Filter identifier - one of: create txid, filter reference, filter name.\n"
            "\nResult:\n"
            "Filter code in plain text\n"            
           "\nExamples:\n"
            + HelpExampleCli("getfiltercode", "filter1")
            + HelpExampleRpc("getfiltercode", "filter1")
        ));
     
     mapHelpStrings.insert(std::make_pair("runtxfilter",
            "runtxfilter \"filter-identifier\" ( \"tx-hex\"|\"txid\" )\n"
            "\nCompile an existing filter and optionally test it on a transaction\n"
            "\nArguments:\n"
            "1. \"filter-identifier\"              (string, required) Filter identifier - one of: create txid, filter reference, filter name.\n"
            "2. \"tx-hex\"                         (string, optional) The transaction hex string to filter, otherwise filter compiled only\n"
            " or\n"     
            "2. \"txid\"                           (string, optional) The transaction id\n"
            "{\n"
            "  \"compiled\": true|false,           (boolean) Filter passed compilation\n"
            "  \"passed\": true|false,             (boolean) Transaction passed the filter\n"
            "  \"callbacks\": callbacks,           (array of objects) Information about callback calls by filter\n"
            "  \"reason\": \"rejection reason\",     (boolean) Reason for rejection, null if passed\n"
            "  \"time\": x.xxxxxx,                 (numeric) Seconds to run transaction through the filter\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("runtxfilter", "filter1")
            + HelpExampleRpc("runtxfilter", "filter1")
        ));
     
     mapHelpStrings.insert(std::make_pair("testtxfilter",
            "testtxfilter restrictions \"javascript-code\" ( \"tx-hex\"|\"txid\" )\n"
            "\nCompile a test filter and optionally test it on a transaction\n"
            "\nArguments:\n"
            "1. restrictions                     (object, required)  a json object with filter restrictions\n"
            "    {\n"
            "      \"for\": \"entity-identifier\"    (string, optional) Asset/stream identifier - one of: create txid, stream reference, stream name.\n"
            "        or\n"
            "      \"for\": entity-identifier(s)   (array, optional) A json array of asset/stream identifiers .\n"    
            "    }\n"
            "2. \"javascript-code\"                (string, required) JavaScript filter code, see help filters. Example:\n"
            "                                                         function filtertransaction()\n"
            "                                                         {\n"
            "                                                             var tx=getfiltertransaction();\n"
            "                                                                                          \n"
            "                                                             if (tx.vout.length<1)\n"
            "                                                                 return \"One output required\";\n"
            "                                                         }   \n "
            "3. \"tx-hex\"                         (string, optional) The transaction hex string to filter, otherwise filter compiled only\n"
            " or\n"     
            "3. \"txid\"                           (string, optional) The transaction id\n"
            "\nResult:\n"
            "{\n"
            "  \"compiled\": true|false,           (boolean) Filter passed compilation\n"
            "  \"passed\": true|false,             (boolean) Transaction passed the filter\n"
            "  \"callbacks\": callbacks,           (array of objects) Information about callback calls by filter\n"
            "  \"reason\": \"rejection reason\",     (boolean) Reason for rejection, null if passed\n"
            "  \"time\": x.xxxxxx,                 (numeric) Seconds to run transaction through the filter\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("testtxfilter", "\"{}\" \"javascript-code\"")
            + HelpExampleRpc("testtxfilter", "\"{}\", \"javascript-code\"")
        ));
     
    mapHelpStrings.insert(std::make_pair("getfiltertransaction",
            "getfiltertransaction\n"
            "\nReturns an object with information about the filtered transaction.\n"
        ));
    
    mapHelpStrings.insert(std::make_pair("getfilterstreamitem",
            "getfilterstreamitem\n"
            "\nReturns an object with information about the filtered stream item.\n"
        ));
    
    mapHelpStrings.insert(std::make_pair("getlastblockinfo",
            "getlastblockinfo ( skip )\n"
            "\nReturns information about the last or recent blocks in the active chain.\n"
            "\nArguments:\n"
            "1. skip                            (numeric, optional) The number of blocks back to skip. Default 0.\n"
            "\nResult:\n"
            "{\n"
            "  \"hash\" : \"hash\",                  (string) The block hash\n"
            "  \"height\" : n,                     (numeric) The block height or index\n"
            "  \"time\" : ttt,                     (numeric) The block time in seconds since epoch (Jan 1 1970 GMT)\n"
            "  \"txcount\" : n,                    (numeric) Number of transactions in block\n"
            "  \"miner\" : n,                       (string) The address of the miner\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("getlastblockinfo", "")
            + HelpExampleRpc("getlastblockinfo", "")
        ));
    
    
   
}

void mc_InitRPCHelpMap19()
{
    mapHelpStrings.insert(std::make_pair("verifypermission",
            "verifypermission \"address\" \"permission\" \n"
            "\nChecks whether the address has a specified permission.\n"
            "\nArguments:\n"
            "1. \"address\"                      (string, required) The address to verify permission for. \n"
            "2. \"permission\"                   (string, required) Permission string. Possible values: " + AllowedPermissions() + ". \n"                
            "\nResult:\n"
            "True if address has specified permission, false otherwise\n"            
            "\nExamples:\n"
            + HelpExampleCli("verifypermission", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" send" )
            + HelpExampleRpc("verifypermission", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\", \"send\"")
        ));
    
     mapHelpStrings.insert(std::make_pair("getfiltertxinput",
            "getfiltertxinput(n) \n"
            "\nReturns details about an unspent transaction output spent in the filtered transaction.\n"
            "\nArguments:\n"
            "1. vin                                (numeric, required) Transaction input index\n"
        ));
     
     mapHelpStrings.insert(std::make_pair("getfiltertxid",
            "getfiltertxid  \n"
            "\nReturns the transaction id of the filtered transaction.\n"
        ));
     
     mapHelpStrings.insert(std::make_pair("setfilterparam",
             "setfilterparam(\"parameter-name\",parameter-value) \n"
            "\nSets value for runtime parameter, used in callbacks while this filter is running\n"
            "\nArguments:\n"
            "1. \"parameter-name\"                 (string, required) Parameter name, one of:\n"
            "                                                       maxshowndata\n"
            "2. parameter-value                  (required) parameter value\n"
        ));
     mapHelpStrings.insert(std::make_pair("filters",
            "\nThe following APIs are allowed to be called from filter code:\n"
            "                         getlastblockinfo \n"
            "                         getassetinfo \n"
            "                         getstreaminfo \n"
            "                         verifypermission \n"
            "                         verifymessage \n"
            "\nThe following additional methods can be called from filter code (see help <method> for details):\n"
            "                         getfiltertransaction \n"
            "                         getfiltertxid \n"
            "                         getfilterstreamitem (only for stream filters)\n"
            "                         getfiltertxinput (only for tx filters)\n"
            "                         getfilterassetbalances (only for tx filters)\n"
            "                         setfilterparam \n"             
        ));
    
     mapHelpStrings.insert(std::make_pair("getassetinfo",
            "getassetinfo \"asset-identifier\" ( verbose )\n"
            "\nReturns information about a single asset\n"
            "\nArguments:\n"
            "1. \"asset-identifier\"               (string, required) Asset identifier - one of: issue txid, asset reference, asset name.\n"
            "2. verbose                          (boolean, optional, default=false) If true, returns list of all issue transactions, including follow-ons \n"
            "\nResult:\n"
            "Object with asset details\n"            
            "\nExamples:\n"
            + HelpExampleCli("getassetinfo", "myasset")
            + HelpExampleRpc("getassetinfo", "myasset")
        ));
      
     mapHelpStrings.insert(std::make_pair("getstreaminfo",
            "getstreaminfo \"stream-identifier\" ( verbose )\n"
            "\nReturns information about a single stream\n"
            "\nArguments:\n"
            "1. \"stream-identifier\"              (string, required) Stream identifier - one of: create txid, stream reference, stream name.\n"
            "2. verbose                          (boolean, optional, default=false) If true, returns list of creators \n"
            "\nResult:\n"
            "Object with stream details\n"            
            "\nExamples:\n"
            + HelpExampleCli("getstreaminfo", "mystream")
            + HelpExampleRpc("getstreaminfo", "mystream")
        ));
      
     mapHelpStrings.insert(std::make_pair("runstreamfilter",
            "runstreamfilter \"filter-identifier\" ( \"tx-hex\"|\"txid\" vout )\n"
            "\nCompile an existing filter and optionally test it on a transaction\n"
            "\nArguments:\n"
            "1. \"filter-identifier\"              (string, required) Filter identifier - one of: create txid, filter reference, filter name.\n"
            "2. \"tx-hex\"                         (string, optional) The transaction hex string to filter, otherwise filter compiled only\n"
            " or\n"     
            "2. \"txid\"                           (string, optional) The transaction id\n"
            "3. vout:n                           (numeric, optional) The output number, if omitted and txid/tx-hex is specified, found automatically\n"
            "\nResult:\n"
            "{\n"
            "  \"compiled\": true|false,           (boolean) Filter passed compilation\n"
            "  \"passed\": true|false,             (boolean) Transaction passed the filter\n"
            "  \"callbacks\": callbacks,           (array of objects) Information about callback calls by filter\n"
            "  \"reason\": \"rejection reason\",     (boolean) Reason for rejection, null if passed\n"
            "  \"time\": x.xxxxxx,                 (numeric) Seconds to run transaction through the filter\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("runstreamfilter", "filter1")
            + HelpExampleRpc("runstreamfilter", "filter1")
        ));
     
     mapHelpStrings.insert(std::make_pair("teststreamfilter",
            "teststreamfilter restrictions \"javascript-code\" ( \"tx-hex\"|\"txid\" vout )\n"
            "\nCompile a test filter and optionally test it on a transaction\n"
            "\nArguments:\n"
            "1. restrictions                     (object, required)  a json object with filter restrictions\n"
            "    {\n"
            "    }\n"
            "2. \"javascript-code\"                (string, required) JavaScript filter code, see help filters. Example:\n"
            "                                                         function filterstreamitem()\n"
            "                                                         {\n"
            "                                                             var item=getfilterstreamitem();\n"
            "                                                                                          \n"
            "                                                             if (item.keys.length<2)\n"
            "                                                                 return \"At least two keys required\";\n"
            "                                                         }   \n"
            "3. \"tx-hex\"                         (string, optional) The transaction hex string to filter, otherwise filter compiled only\n"
            " or\n"     
            "3. \"txid\"                           (string, optional) The transaction id\n"
            "4. vout:n                           (numeric, optional) The output number, if omitted and txid/tx-hex is specified, found automatically\n"
            "\nResult:\n"
            "{\n"
            "  \"compiled\": true|false,           (boolean) Filter passed compilation\n"
            "  \"passed\": true|false,             (boolean) Transaction passed the filter\n"
            "  \"callbacks\": callbacks,           (array of objects) Information about callback calls by filter\n"
            "  \"reason\": \"rejection reason\",     (boolean) Reason for rejection, null if passed\n"
            "  \"time\": x.xxxxxx,                 (numeric) Seconds to run transaction through the filter\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("teststreamfilter", "\"{}\" \"javascript-code\"")
            + HelpExampleRpc("teststreamfilter", "\"{}\", \"javascript-code\"")
        ));
     
     mapHelpStrings.insert(std::make_pair("getfilterassetbalances",
            "getfilterassetbalances \"asset-identifier\" ( raw )\n"
            "\nReturns information about the last or recent blocks in the active chain.\n"
            "\nArguments:\n"
            "1. \"asset-identifier\"               (string, required) Asset identifier - one of: issue txid, asset reference, asset name. \"\" for native currency.\n"
            "2. raw                              (boolean, optional, default false) Return raw value\n"
            "\nResult:\n"
            "Object with balances for every address involved in transaction\n"            
        ));
     
}

void mc_InitRPCHelpMap20()
{
     mapHelpStrings.insert(std::make_pair("publishmulti",
            "publishmulti \"stream-identifier\" items \"options\" \n"
            "\nPublishes several stream items\n"
            + HelpRequiringPassphraseWrapper() +
            "\nArguments:\n"
            "1. \"stream-identifier\"                (string, required) Stream identifier - one of: create txid, stream reference, stream name. Default for items if \"for\" field is omitted\n"     
            "2. items                              (array, required) Array of stream items. \n"
            "  [\n"                
            "    {\n"                
            "      \"for\" : \"stream-identifier\"     (string, optional) Stream identifier, uses default if omitted.\n"
            "      \"options\" : \"options\"           (string, optional) Should be \"offchain\" or omitted\n"
            "      \"key\" : \"key\"                   (string, optional, default: \"\") Item key\n"
            "        or\n"
            "      \"keys\" : keys                   (array, optional) Item keys, array of strings\n"
            "      \"data\" : \"data-hex\"             (string, optional, default: \"\") Data hex string\n"
            "        or\n"
            "      \"data\" :                        (object, required) JSON data object\n"
            "        {\n"
            "          \"json\" : data-json          (object, required) Valid JSON string\n" 
            "        }\n"                                
            "        or\n"
            "      \"data\" :                        (object, required) Text data object\n"
            "        {\n"
            "          \"text\" : \"data-text\"        (string, required) Data string\n" 
            "        }\n"                                
            "        or\n"
            "      \"data\"                          (object, required) Binary raw data created with appendbinarycache\n"
            "        {\n"
            "          \"cache\" : \"identifier\"      (string, required) Binary cache identifier\n" 
            "        }\n"                              
            "    }\n"                                
            "  ]\n"                                
            "3. \"options\"                          (string, optional) Should be \"offchain\" or omitted. Default for items if \"options\" field is omitted\n"
            "\nResult:\n"
            "\"transactionid\"                       (string) The transaction id.\n"
            "\nExamples:\n"
            + HelpExampleCli("publishmulti", "test [{\"key\":\"hello world\",\"data\":\"48656C6C6F20576F726C64210A\"}]")
            + HelpExampleRpc("publishmulti", "\"test\", [{\"key\":\"hello world\",\"data\":\"48656C6C6F20576F726C64210A\"}]")
        ));

     mapHelpStrings.insert(std::make_pair("publishmultifrom",
            "publishmultifrom \"from-address\" \"stream-identifier\" items \"options\" \n"
            "\nPublishes several stream items\n"
            + HelpRequiringPassphraseWrapper() +
            "\nArguments:\n"
            "1. \"from-address\"                     (string, required) Address used for publishing.\n"
            "2. \"stream-identifier\"                (string, required) Stream identifier - one of: create txid, stream reference, stream name. Default for items if \"for\" field is omitted\n"     
            "3. items                              (array, required) Array of stream items. \n"
            "  [\n"                
            "    {\n"                
            "      \"for\" : \"stream-identifier\"     (string, optional) Stream identifier, uses default if omitted.\n"
            "      \"options\" : \"options\"           (string, optional) Should be \"offchain\" or omitted\n"
            "      \"key\" : \"key\"                   (string, optional, default: \"\") Item key\n"
            "        or\n"
            "      \"keys\" : keys                   (array, optional) Item keys, array of strings\n"
            "      \"data\" : \"data-hex\"             (string, optional, default: \"\") Data hex string\n"
            "        or\n"
            "      \"data\" :                        (object, required) JSON data object\n"
            "        {\n"
            "          \"json\" : data-json          (object, required) Valid JSON string\n" 
            "        }\n"                                
            "        or\n"
            "      \"data\" :                        (object, required) Text data object\n"
            "        {\n"
            "          \"text\" : \"data-text\"        (string, required) Data string\n" 
            "        }\n"                                
            "        or\n"
            "      \"data\"                          (object, required) Binary raw data created with appendbinarycache\n"
            "        {\n"
            "          \"cache\" : \"identifier\"      (string, required) Binary cache identifier\n" 
            "        }\n"                              
            "    }\n"                                
            "  ]\n"                                
            "4. \"options\"                          (string, optional) Should be \"offchain\" or omitted. Default for items if \"options\" field is omitted\n"
            "\nResult:\n"
            "\"transactionid\"                       (string) The transaction id.\n"
            "\nExamples:\n"
            + HelpExampleCli("publishmultifrom", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" test [{\"key\":\"hello world\",\"data\":\"48656C6C6F20576F726C64210A\"}]")
            + HelpExampleRpc("publishmultifrom", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\", \"test\", [{\"key\":\"hello world\",\"data\":\"48656C6C6F20576F726C64210A\"}]")
        ));
     
    mapHelpStrings.insert(std::make_pair("txouttobinarycache",
            "txouttobinarycache \"identifier\" \"txid\" vout ( count-bytes start-byte )\n"
            "\nStores metadata of transaction output in binary cache.\n"    
            "\nArguments:\n"
            "1. \"identifier\"                     (string, required) Binary cache item identifier\n"
            "2. \"txid\"                           (string, required) The transaction id\n"
            "3. vout                             (numeric, required) vout value\n"
            "4. count-bytes                      (numeric, optional, default=INT_MAX) Number of bytes to return\n"
            "5. start-byte                       (numeric, optional, default=0) start from specific byte \n"
            "\nResult:\n"
            "size                                (numeric) Size of the binary cache item\n"
            "\nExamples:\n"
            "\nView the data\n"
            + HelpExampleCli("txouttobinarycache", "\"TjnVWwHYEg4\" \"txid\" 1") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("txouttobinarycache", "\"TjnVWwHYEg4\", \"txid\", 1")
        ));
    
    mapHelpStrings.insert(std::make_pair("trimsubscribe",
            "trimsubscribe stream-identifier(s) \"parameters\"\n"
            "\nAvailable only in Enterprise Edition.\n"
            "\nRemoves indexes from subscriptions to the stream.\n"
            "\nArguments:\n"
            "1. \"stream-identifier\"              (string, required) Stream identifier - one of: create txid, stream reference, stream name.\n"
            " or\n"
            "1. stream-identifier(s)             (array, optional) A json array of stream identifiers \n"                
            "2. \"parameters\"                     (string, required) Comma-delimited subset of: \n"
            "                                                         retrieve - stop automatic retrieval of offchain items, \n"
            "                                                         keys - trim index for liststreamkeys, liststreamkeyitems, getstreamkeysummary,\n"
            "                                                         publishers - trim index for liststreampublishers, liststreampublisheritems, getstreampublishersummary,\n"
            "                                                         items-local - trim index for liststreamitems for local-ordering=true,\n"
            "                                                         keys-local - same as keys, for local-ordering=true,\n"
            "                                                         publishers-local - same as publishers, for local-ordering=true\n"
            "\nResult:\n"
            "\nExamples:\n"
            + HelpExampleCli("trimsubscribe", "\"test-stream\" \"retrieve,publishers\"") 
            + HelpExampleRpc("trimsubscribe", "\"test-stream\", \"retrieve,publishers\"")
         ));
    
    mapHelpStrings.insert(std::make_pair("retrievestreamitems",
            "retrievestreamitems stream-identifier \"txids\"|txouts|blocks|query\n"
            "\nAvailable only in Enterprise Edition.\n"
            "\nSchedules retrieval of offchain data for specific items in the stream\n"
            "\nArguments:\n"
            "1. \"stream-identifier\"              (string, required) Stream identifier - one of: create txid, stream reference, stream name.\n"
            "2. \"txids\"                          (string, required) \"all\" or list of transactions, comma delimited\n"
            " or\n"         
            "2. txids                            (array, required) Array of transactions IDs\n"
            " or\n"             
            "2. txouts                           (array, required) Array of transaction outputs\n"
            " [\n"
            "   {\n"
            "     \"txid\":\"id\",                   (string, required) The transaction id\n"
            "     \"vout\":n                       (numeric, required) The output number\n"
            "   }\n"
            "  ,...\n"
            " ]\n"
            " or\n"         
            "2. blocks                           (object, required) List of transactions in block range\n"
            "  { \"blocks\":\n"
            "    \"block-set-identifier\"          (string, required) Comma delimited list of block identifiers: \n"
            "                                                       block height,\n"
            "                                                       block hash,\n"
            "                                                       block height range, e.g. <block-from>-<block-to>,\n"
            "                                                       number of last blocks in the active chain (if negative),\n"
            "  or\n"
            "    block-set-identifier            (array, required)  A json array of block identifiers \n"                
            "  or\n"
            "    block-set-identifier            (object, required) A json object with time range\n"
            "    {\n"                
            "      \"starttime\" : start-time      (numeric,required) Start time.\n"
            "      \"endtime\" : end-time          (numeric,required) End time.\n"
            "    }\n"                                
            "  }\n"                                
            " or\n"         
            "2. query                            (object, required) Query (AND logic)\n"
            "  {\n"                
            "    \"key\" : \"key\"                   (string, optional, default: \"\") Item key\n"
            "      or\n"
            "    \"keys\" : keys                   (array, optional) Item keys, array of strings\n"
            "      and/or\n"
            "    \"publisher\" : \"publisher\"       (string, optional, default: \"\") Publisher\n"
            "      or\n"
            "    \"publishers\" : publishers       (array, optional) Publishers, array of strings\n"
            "  }\n"                                
            "\nReturns statistics about changes in chunk queue.\n"
            "\nExamples:\n"
            + HelpExampleCli("retrievestreamitems", "\"test-stream\" \"{\\\"keys\\\":[\\\"key01\\\",\"key02\"]}\"") 
            + HelpExampleCli("retrievestreamitems", "\"test-stream\" \"{\\\"keys\\\":[\\\"key01\\\",\"key02\"],\\\"publisher\\\":\\\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XZ\\\"}\" true ") 
            + HelpExampleRpc("retrievestreamitems", "\"test-stream\", \"{\\\"keys\\\":[\\\"key01\\\",\"key02\"],\\\"publisher\\\":\\\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XZ\\\"}\", false")
         ));
    
    mapHelpStrings.insert(std::make_pair("purgestreamitems",
            "purgestreamitems stream-identifier \"txids\"|txouts|blocks|query\n"
            "\nAvailable only in Enterprise Edition.\n"
            "\nPurges offchain data for specific items in the stream.\n"
            "\nArguments:\n"
            "1. \"stream-identifier\"              (string, required) Stream identifier - one of: create txid, stream reference, stream name.\n"
            "2. \"txids\"                          (string, required) \"all\" or list of transactions, comma delimited\n"
            " or\n"         
            "2. txids                            (array, required) Array of transactions IDs\n"
            " or\n"             
            "2. txouts                           (array, required) Array of transaction outputs\n"
            " [\n"
            "   {\n"
            "     \"txid\":\"id\",                   (string, required) The transaction id\n"
            "     \"vout\":n                       (numeric, required) The output number\n"
            "   }\n"
            "  ,...\n"
            " ]\n"
            " or\n"         
            "2. blocks                           (object, required) List of transactions in block range\n"
            "  { \"blocks\":\n"
            "    \"block-set-identifier\"          (string, required) Comma delimited list of block identifiers: \n"
            "                                                       block height,\n"
            "                                                       block hash,\n"
            "                                                       block height range, e.g. <block-from>-<block-to>,\n"
            "                                                       number of last blocks in the active chain (if negative),\n"
            "  or\n"
            "    block-set-identifier            (array, required)  A json array of block identifiers \n"                
            "  or\n"
            "    block-set-identifier            (object, required) A json object with time range\n"
            "    {\n"                
            "      \"starttime\" : start-time      (numeric,required) Start time.\n"
            "      \"endtime\" : end-time          (numeric,required) End time.\n"
            "    }\n"                                
            "  }\n"                                
            " or\n"         
            "2. query                            (object, required) Query (AND logic)\n"
            "  {\n"                
            "    \"key\" : \"key\"                   (string, optional, default: \"\") Item key\n"
            "      or\n"
            "    \"keys\" : keys                   (array, optional) Item keys, array of strings\n"
            "      and/or\n"
            "    \"publisher\" : \"publisher\"       (string, optional, default: \"\") Publisher\n"
            "      or\n"
            "    \"publishers\" : publishers       (array, optional) Publishers, array of strings\n"
            "  }\n"                                
            "\nReturns statistics about purged items.\n"
            "\nExamples:\n"
            + HelpExampleCli("purgestreamitems", "\"test-stream\" \"{\\\"keys\\\":[\\\"key01\\\",\"key02\"]}\"") 
            + HelpExampleCli("purgestreamitems", "\"test-stream\" \"{\\\"keys\\\":[\\\"key01\\\",\"key02\"],\\\"publisher\\\":\\\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XZ\\\"}\" true ") 
            + HelpExampleRpc("purgestreamitems", "\"test-stream\", \"{\\\"keys\\\":[\\\"key01\\\",\"key02\"],\\\"publisher\\\":\\\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XZ\\\"}\", false")
         ));
    
    mapHelpStrings.insert(std::make_pair("purgepublisheditems",
            "purgepublisheditems \"txids\"|txouts|blocks\n"
            "\nAvailable only in Enterprise Edition.\n"
            "\nPurges offchain items published by this node\n"
            "\nArguments:\n"
            "1. \"txids\"                          (string, required) \"all\" or list of transactions, comma delimited\n"
            " or\n"         
            "1. txids                            (array, required) Array of transactions IDs\n"
            " or\n"             
            "1. txouts                           (array, required) Array of transaction outputs\n"
            " [\n"
            "   {\n"
            "     \"txid\":\"id\",                   (string, required) The transaction id\n"
            "     \"vout\":n                       (numeric, required) The output number\n"
            "   }\n"
            "  ,...\n"
            " ]\n"
            " or\n"         
            "1. blocks                           (object, required) List of transactions in block range\n"
            "  { \"blocks\":\n"
            "    \"block-set-identifier\"          (string, required) Comma delimited list of block identifiers: \n"
            "                                                       block height,\n"
            "                                                       block hash,\n"
            "                                                       block height range, e.g. <block-from>-<block-to>,\n"
            "                                                       number of last blocks in the active chain (if negative),\n"
            "  or\n"
            "    block-set-identifier            (array, required)  A json array of block identifiers \n"                
            "  or\n"
            "    block-set-identifier            (object, required) A json object with time range\n"
            "    {\n"                
            "      \"starttime\" : start-time      (numeric,required) Start time.\n"
            "      \"endtime\" : end-time          (numeric,required) End time.\n"
            "    }\n"                                
            "  }\n"                                
            "\nReturns statistics about purged items.\n"
            "\nExamples:\n"
            + HelpExampleCli("purgepublisheditems", "\"mytxid\"") 
            + HelpExampleRpc("purgepublisheditems", "\"mytxid\"")
         ));
    
    mapHelpStrings.insert(std::make_pair("getlicenserequest",
            "getlicenserequest \n"
            "\nReturns license request.\n"
            "\nExamples:\n"
            + HelpExampleCli("getlicenserequest", "") 
            + HelpExampleRpc("getlicenserequest", "")    
       ));
   
    mapHelpStrings.insert(std::make_pair("decodelicenserequest",
            "decodelicenserequest \"license-request-hex\"\n"
            "\nAvailable only in Enterprise Edition.\n"
            "\nReturns a JSON object representing the serialized, hex-encoded license request.\n"

            "\nArguments:\n"
            "1. \"license-request-hex\"                          (string, required) The license request hex string (output of getlicenserequest)\n"

            "\nExamples:\n"
            + HelpExampleCli("decodelicenserequest", "\"hexstring\"")
            + HelpExampleRpc("decodelicenserequest", "\"hexstring\"")
      ));
   
    mapHelpStrings.insert(std::make_pair("decodelicenseconfirmation",
            "decodelicenseconfirmation \"license-confirmation-hex\"\n"
            "\nAvailable only in Enterprise Edition.\n"
            "\nReturns a JSON object representing the serialized, hex-encoded license confirmation.\n"

            "\nArguments:\n"
            "1. \"license-confirmation-hex\"                     (string, required) The license confirmation hex string (input of activatelicense)\n"

            "\nExamples:\n"
            + HelpExampleCli("decodelicenseconfirmation", "\"hexstring\"")
            + HelpExampleRpc("decodelicenseconfirmation", "\"hexstring\"")
      ));
   
}

void mc_InitRPCHelpMap21()
{
    mapHelpStrings.insert(std::make_pair("listlicenses",
            "listlicenses ( license-identifier(s) verbose ) \n"
            "\nAvailable only in Enterprise Edition.\n"
            "\nReturns list of licenses owned by this node\n"
            "\nArguments:\n"
            "1. \"license-identifier(s)\"          (string, optional, default=*) License identifier - one of the following:\n"
            "                                                                           (license) name\n"
            "                                                                           confirmation->licensehash\n"
            "                                                                           transactions->issuetxid\n"
            "                                                                           transactions->assetref\n"
            "                                                                           transactions->lasttxid\n"
            " or\n"
            "1. license-identifier(s)            (array, optional) A json array of license identifiers \n"                
            "2. verbose                          (boolean, optional, default=false) If true, returns extended license information \n"
            "\nResult:\n"
            "An array containing list of licenses\n"            
            "\nExamples:\n"
            + HelpExampleCli("listlicenses", "")
            + HelpExampleRpc("listlicenses", "")
        ));
    
    mapHelpStrings.insert(std::make_pair("getlicenseconfirmation",
            "getlicenseconfirmation \"license-request-hex\" ( confirmation-settings )\n"
            "\nAvailable only in Enterprise Edition.\n"
            "\nReturns license confirmation.\n"

            "\nArguments:\n"
            "1. \"license-request-hex\"                          (string, required) The license request hex string (output of getlicenserequest)\n"
            "2. confirmation-settings                          (integer, optional) Number of confirmations with default settings to return\n"
            "  or\n"
            "2. confirmation-settings                          (object, optional) Confirmation settings for new request. Possible fields:\n"
            "                                                       count, integer, number of confirmations with these settings to return\n"
            "                                                       starttime, integer\n"
            "                                                       endtime,  integer\n"
            "                                                       interval, integer (in case of conflict with endtime, the last field counts)\n"
            "                                                       features, integer or hexadecimal string\n"
            "                                                       single feature, as it appears in the output of listlicenses, boolean\n"
            "                                                                         (in case of conflict with features, the last field counts)\n"
            "                                                       flags,  integer\n"
            "                                                       params,  a json object with custom parameters\n"
            "                                                       details,  a json object with custom details\n"
            "  or\n"
            "2. confirmation-settings                          (object, optional) Confirmation settings for request extension. Possible fields:\n"
            "                                                       extension, required, boolean, should be true\n"
            "                                                       interval, integer, optional, default - like in previous license\n"
            "                                                       delay, integer, optional, delay after last license end time, default 0  \n"
            "  or\n"
            "2. confirmation-settings                          (array, optional) Array of objects as described above, \"count\" field is ignored\n"

            "\nReturns array of license confirmations.\n"
            "\nExamples:\n"
            + HelpExampleCli("getlicenseconfirmation", "\"hexstring\"")
            + HelpExampleRpc("getlicenseconfirmation", "\"hexstring\"")
      ));
   
    mapHelpStrings.insert(std::make_pair("activatelicense",
            "activatelicense ( \"license-confirmation-hex\" )\n"
            "\nAvailable only in Enterprise Edition.\n"
            "\nActivates Enterprise license.\n"

            "\nArguments:\n"
            "1. \"license-confirmation-hex\"                     (string, optional) The license confirmation hex string\n"
            "                                                       If omitted, empty, self-signed license is activated.\n"

            "\nResult:\n"
            "\"transactionid\"                     (string) The transaction id.\n"
    
            "\nExamples:\n"
            + HelpExampleCli("activatelicense", "\"hexstring\"")
            + HelpExampleRpc("activatelicense", "\"hexstring\"")
      ));
   
    mapHelpStrings.insert(std::make_pair("activatelicensefrom",
            "activatelicensefrom \"from-address\" ( \"license-confirmation-hex\" )\n"
            "\nAvailable only in Enterprise Edition.\n"
            "\nActivates Enterprise license.\n"

            "\nArguments:\n"
            "1. \"from-address\"                     (string, required) Address used for publishing.\n"
            "2. \"license-confirmation-hex\"                     (string, optional) The license confirmation hex string\n"
            "                                                       If omitted, empty, self-signed license is activated.\n"

            "\nResult:\n"
            "\"transactionid\"                     (string) The transaction id.\n"
    
            "\nExamples:\n"
            + HelpExampleCli("activatelicensefrom", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" \"hexstring\"")
            + HelpExampleRpc("activatelicensefrom", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\", \"hexstring\"")
      ));
   
    mapHelpStrings.insert(std::make_pair("transferlicense",
            "transferlicense \"license-identifier\" \"license-request-hex\" \n"
            "\nAvailable only in Enterprise Edition.\n"
            "\nTransfers Enterprise license.\n"

            "\nArguments:\n"
            "1. \"license-identifier\"                  (string, required) License identifier - one of the following (see output of listlicenses):\n"
            "                                                                           (license) name\n"
            "                                                                           confirmation->licensehash\n"
            "                                                                           transactions->issuetxid\n"
            "                                                                           transactions->assetref\n"
            "                                                                           transactions->lasttxid\n"
            "2. \"license-request-hex\"                 (string, required) The license confirmation hex string\n"
            "\nResult:\n"
            "\"transactionid\"                          (string) The transaction id.\n"
            "\nExamples:\n"
            + HelpExampleCli("transferlicense", "\"license-7952-5b4c-fe80-1667\" \"hexstring\"")
            + HelpExampleRpc("transferlicense", "\"license-7952-5b4c-fe80-1667\",\"hexstring\"")
      ));
   
    mapHelpStrings.insert(std::make_pair("takelicense",
            "takelicense \"license-identifier\"  \n"
            "\nAvailable only in Enterprise Edition.\n"
            "\nTake Enterprise license to this node and stop any other from using it.\n"

            "\nArguments:\n"
            "1. \"license-identifier\"                  (string, required) License identifier - one of the following (see output of listlicenses):\n"
            "                                                                           (license) name\n"
            "                                                                           confirmation->licensehash\n"
            "                                                                           transactions->issuetxid\n"
            "                                                                           transactions->assetref\n"
            "                                                                           transactions->lasttxid\n"
            "\nResult:\n"
            "\"transactionid\"                          (string) The transaction id.\n"
            "\nExamples:\n"
            + HelpExampleCli("takelicense", "\"license-7952-5b4c-fe80-1667\"")
            + HelpExampleRpc("takelicense", "\"license-7952-5b4c-fe80-1667\"")
      ));
   
    mapHelpStrings.insert(std::make_pair("importlicenserequest",
            "importlicenserequest \"license-request-hex\" \n"
            "\nAvailable only in Enterprise Edition.\n"
            "\nImports license request.\n"

            "\nArguments:\n"
            "1. \"license-request-hex\"                          (string, required) The license request hex string (output of getlicenserequest)\n"

            "\nExamples:\n"
            + HelpExampleCli("importlicenserequest", "\"hexstring\"")
            + HelpExampleRpc("importlicenserequest", "\"hexstring\"")
      ));
   
     mapHelpStrings.insert(std::make_pair("getinitstatus",
            "getinitstatus\n"
            "\nReturns information about initialization status of this node\n"
            "\nExamples:\n"
            + HelpExampleCli("getinitstatus", "")
            + HelpExampleRpc("getinitstatus", "")
        ));
  
    
    mapHelpStrings.insert(std::make_pair("createfeed",
            "createfeed \"feed-name\" ( parameters ) \n"
            "\nAvailable only in Enterprise Edition.\n"
            "\nCreates feed\n"
            "\nArguments:\n"
            "1. \"feed-name\"                      (string, required) Feed name\n"
            "2. parameters                       (object, optional) Feed parameters. Supported parameters: \n"
            "                                                         directory - feed output directory, MultiChain will attempt to create it.\n"
            "\nExamples:\n"
            + HelpExampleCli("createfeed", "feed1")
            + HelpExampleRpc("createfeed", "feed1")
        ));
    
    mapHelpStrings.insert(std::make_pair("deletefeed",
            "deletefeed \"feed-name\" ( force )\n"
            "\nAvailable only in Enterprise Edition.\n"
            "\nDeletes feed. \n"
            "\nArguments:\n"
            "1. \"feed-name\"                      (string, required) Feed name\n"
            "2. force                            (string, optional, default false) Delete feed even with unsuspended subscriptions and not purged files.\n"
            "                                                         Feed directory is not deleted.\n"
            "\nExamples:\n"
            + HelpExampleCli("deletefeedfeed", "feed1")
            + HelpExampleRpc("deletefeedfeed", "feed1")
        ));
    
    mapHelpStrings.insert(std::make_pair("pausefeed",
            "pausefeed \"feed-name\" ( buffer )\n"
            "\nAvailable only in Enterprise Edition.\n"
            "\nPauses output to feed\n"
            "\nArguments:\n"
            "1. \"feed-name\"                      (string, required) Feed name\n"
            "2. buffer                           (optional, default=true) Store feed events in temporary buffer while paused\n"
            "\nExamples:\n"
            + HelpExampleCli("pausefeed", "feed1")
            + HelpExampleRpc("pausefeed", "feed1")
        ));
    
    mapHelpStrings.insert(std::make_pair("resumefeed",
            "resumefeed \"feed-name\" ( buffer )\n"
            "\nAvailable only in Enterprise Edition.\n"
            "\nResumes output to feed\n"
            "\nArguments:\n"
            "1. \"feed-name\"                      (string, required) Feed name\n"
            "2. buffer                           (optional, default=true) Try to restore events from temporary buffer\n"
            "\nExamples:\n"
            + HelpExampleCli("resumefeed", "feed1")
            + HelpExampleRpc("resumefeed", "feed1")
        ));
    
    mapHelpStrings.insert(std::make_pair("listfeeds",
            "listfeeds ( feed-name(s) verbose ) \n"
            "\nAvailable only in Enterprise Edition.\n"
            "\nReturns list of feeds defined on this node\n"
            "\nArguments:\n"
            "1. \"feed-name\"                      (string, optional, default=*) Feed name\n"
            " or\n"
            "1. feed-name(s)                     (array, optional) A json array of feed names \n"                
            "2. verbose                          (boolean, optional, default=false) If true, returns extended feed information \n"
            "\nResult:\n"
            "An array containing list of feeds\n"            
            "\nExamples:\n"
            + HelpExampleCli("listfeeds", "")
            + HelpExampleRpc("listfeeds", "")
        ));
    
    
}
void mc_InitRPCHelpMap22()
{
     mapHelpStrings.insert(std::make_pair("addtofeed",
            "addtofeed \"feed-name\" entities ( \"globals\" \"action\" options )\n"
            "\nAvailable only in Enterprise Edition.\n"
            "\nAdds subscriptions to feed.\n"
            "\nArguments:\n"
            "1. \"feed-name\"                      (string, required) Feed name\n"
            "2. \"entities\"                       (string, required) Stream identifier - one of: create txid, stream reference, stream name.\n"
            " or\n"
            "2. entities                         (array, optional) A json array of stream identifiers \n"                
            "3. \"globals\"                        (string, optional, default \"\") One of the following: \"blocks\",\"none\",\"\".\n"
            "4. \"action\"                         (string, optional, default=rescan) Immediate action:\n"
            "                                                         rescan - rescan and start processing, \n"
            "                                                         start - start processing without rescanning,\n"
            "                                                         suspend - suspend subscription,\n"
            "5. options                          (object, optional) JSON object of subscription options, see help feed-options for details.\n"
            "\nNote: This call can take minutes to complete if action=rescan.\n"
            "\nResult:\n"
            "\nExamples:\n"
            + HelpExampleCli("addtofeed", "\"feed1\" \"test-stream\" \"\" \"rescan\" \"{\\\"maxshowndata\\\":256}\"") 
            + HelpExampleCli("addtofeed", "\"feed1\" \"test-stream\" blocks suspend") 
            + HelpExampleRpc("addtofeed", "\"feed1\", \"test-stream\"")
         ));
   
     mapHelpStrings.insert(std::make_pair("updatefeed",
            "updatefeed \"feed-name\" entities ( \"globals\" \"action\" options )\n"
            "\nAvailable only in Enterprise Edition.\n"
            "\nUpdates subscriptions in feed.\n"
            "\nArguments:\n"
            "1. \"feed-name\"                      (string, required) Feed name\n"
            "2. \"entities\"                       (string, required) Stream identifier - one of: create txid, stream reference, stream name. Or \"*\" or \"none\".\n"
            " or\n"
            "2. entities                         (array, optional) A json array of stream identifiers \n"                
            "3. \"globals\"                        (string, optional, default \"\") One of the following: \"blocks\",\"*\",\"none\",\"\".\n"
            "4. \"action\"                         (string, optional, default=none) Modification action:\n"
            "                                                         none - don't change current state, \n"
            "                                                         rescan - rescan for non-suspended subscriptions, \n"
            "                                                         sync - rescan for non-suspended subscriptions that are not in sync, \n"
            "                                                         start - unsuspend subscriptions that are suspended,\n"
            "                                                         start,rescan - start followed by rescan,\n"
            "                                                         start,sync - start followed by sync,\n"
            "                                                         suspend - suspend subscription(s),\n"
            "                                                         delete - delete subscription(s)\n"
            "5. options                          (object, optional) JSON object of subscription options, see help feed-options for details.\n"
            "\nNote: This call can take minutes to complete if action=rescan.\n"
            "\nResult:\n"
            "\nExamples:\n"
            + HelpExampleCli("updatefeed", "\"feed1\" \"*\" \"\" \"none\" \"{\\\"maxshowndata\\\":256}\"") 
            + HelpExampleCli("updatefeed", "\"feed1\" \"test-stream\" blocks suspend") 
            + HelpExampleRpc("updatefeed", "\"feed1\", \"test-stream\"")
         ));
   
     mapHelpStrings.insert(std::make_pair("getdatarefdata",
            "getdatarefdata \"dataref\" ( count-bytes start-byte )\n"
            "\nReturns metadata referenced by dataref.\n"
            "\nArguments:\n"
            "1. \"dataref\"                        (string, required) Dataref\n"
            "2. count-bytes                      (numeric, optional, default=INT_MAX) Number of bytes to return\n"
            "3. start-byte                       (numeric, optional, default=0) start from specific byte \n"
            "\nResult:\n"
            "data                                (string) transaction output metadata.\n"
            "\nExamples:\n"
            "\nView the data\n"
            + HelpExampleCli("getdatarefdata", "\"dataref\"") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("getdatarefdata", "\"dataref\"")
        ));
    
     mapHelpStrings.insert(std::make_pair("datareftobinarycache",
            "datareftobinarycache \"identifier\" \"dataref\" ( count-bytes start-byte )\n"
            "\nStores metadata referenced by dataref in binary cache.\n"    
            "\nArguments:\n"
            "1. \"identifier\"                     (string, required) Binary cache item identifier\n"
            "2. \"dataref\"                        (string, required) Dataref\n"
            "3. count-bytes                      (numeric, optional, default=INT_MAX) Number of bytes to return\n"
            "4. start-byte                       (numeric, optional, default=0) start from specific byte \n"
            "\nResult:\n"
            "size                                (numeric) Size of the binary cache item\n"
            "\nExamples:\n"
            "\nView the data\n"
            + HelpExampleCli("datareftobinarycache", "\"TjnVWwHYEg4\" \"dataref\"") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("datareftobinarycache", "\"TjnVWwHYEg4\", \"dataref\"")
        ));
     
    mapHelpStrings.insert(std::make_pair("purgefeed",
            "purgefeed \"feed-name\" file|days|\"*\"\n"
            "\nAvailable only in Enterprise Edition.\n"
            "\nPurges old feed files\n"
            "\nArguments:\n"
            "1. \"feed-name\"                      (string, required) Feed name\n"
            "2. file                             (integer, required) >= 0 Purge files before this file, normally, adapter read file.\n"
            " or\n"    
            "2. days                             (integer, required) <0 Purge only events more than this number of days ago.\n"
            " or\n"    
    
            "2. \"*\"                              (string, required) Purge all files and reset feed pointer \n"
            "\nExamples:\n"
            + HelpExampleCli("purgefeed", "feed1 1000")
            + HelpExampleRpc("purgefeed", "feed1, 1000")
        ));
    
     mapHelpStrings.insert(std::make_pair("feed-options",
            "The following options can be specified in 'options' parameter of updatefeed API:\n"
            "      override (boolean, default false) - when used with entities='*', override previously defined options for each entity\n"
            "\nThe following parameters can be specified in 'options' parameter of addtofeed, updatefeed APIs:\n"
            "      maxshowndata (integer) - maximal size of the data which can be written in the feed file\n"
            "      nostreamfilters (boolean, default false) - skip running of stream filters on feed items\n"
            "\nThe following events can be enabled/disabled in addtofeed, updatefeed APIs, boolean, default true \n"     
            "      blockaddend, blockremoveend  \n"
            "      itemconfirmed, itemunconfirmed  \n"
            "      offchainavailable, offchainpurged \n"
            "\nThe following fields can be enabled/disabled in addtofeed, updatefeed APIs, boolean, default true, \n"
            "      blockaddstart-height,blockaddstart-txcount,blockaddstart-time,blockaddstart-miner, blockaddstart-size,\n"
            "      blockaddend-height,blockaddend-txcount,blockaddend-time,blockaddend-miner, blockaddend-size,\n"
            "      blockremovestart-height,\n"
            "      blockremoveend-height,\n"
            "      itemreceived-txid,itemreceived-vout,itemreceived-stream,itemreceived-publisher,itemreceived-key,itemreceived-format,itemreceived-size, \n"
            "      itemreceived-flags,itemreceived-binary,itemreceived-text,itemreceived-json,itemreceived-dataref,itemreceived-timereceived, \n"
            "      itemconfirmed-stream,itemconfirmed-blockheight,itemconfirmed-blockhash,itemconfirmed-blocktime,itemconfirmed-dataref,itemconfirmed-offsetinblock, \n"
            "      itemunconfirmed-stream,\n"
            "      iteminvalid-stream, \n"
            "      offchainavailable-stream,offchainavailable-format,offchainavailable-size,offchainavailable-flags,\n"
            "      offchainavailable-binary,offchainavailable-text,offchainavailable-json,offchainavailable-dataref,offchainavailable-timereceived, \n"
            "      offchainpurged-stream \n"
            "\nThe following events CAN NOT be disabled in addtofeed, updatefeed APIs, but still included in the feed\n"     
            "      blockaddstart,blockremovestart,itemreceived,iteminvalid\n"
            "\nThe following fields CAN NOT be disabled in addtofeed, updatefeed APIs, but still included in the feed\n"     
            "      blockaddstart-hash,blockaddend-hash,blockremovestart-hash,blockremoveend-hash,\n"
            "      itemreceived-id,itemconfirmed-id,itemunconfirmed-id,iteminvalid-id,offchainavailable-id,offchainpurged-id\n"
        ));
    
}

void mc_InitRPCHelpMap23()
{
    mapHelpStrings.insert(std::make_pair("setvariablevaluefrom",
            "setvariablevaluefrom \"from-address\" \"variable-identifier\" value \n"
            "\nSets variable value\n"
            + HelpRequiringPassphraseWrapper() +
            "\nArguments:\n"
            "1. \"from-address\"                   (string, required) Address used for setting variable value.\n"
            "2. \"variable-identifier\"            (string, required) Variable identifier - one of: create txid, variable reference, variable name.\n"
            "3. value                            (any data, optional, default null) Variable value (JSON objects and arrays allowed).\n"
            "\nResult:\n"
            "\"transactionid\"                     (string) The transaction id.\n"
            "\nExamples:\n"
            + HelpExampleCli("setvariablevaluefrom", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" \"var1\" \"{\\\"count\\\":8}\"")
            + HelpExampleRpc("setvariablevaluefrom", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\", \"var1\", \"{\\\"count\\\":8}\"")
        ));
    
    mapHelpStrings.insert(std::make_pair("setvariablevalue",
            "setvariablevalue \"variable-identifier\" value \n"
            "\nChanges variable value\n"
            + HelpRequiringPassphraseWrapper() +
            "\nArguments:\n"
            "1. \"variable-identifier\"            (string, required) Variable identifier - one of: create txid, variable reference, variable name.\n"
            "2. value                            (any data, optional, default null) Variable value (JSON objects and arrays allowed).\n"
            "\nResult:\n"
            "\"transactionid\"                     (string) The transaction id.\n"
            "\nExamples:\n"
            + HelpExampleCli("setvariablevalue", "\"var1\" \"{\\\"count\\\":8}\"")
            + HelpExampleRpc("setvariablevalue", "\"var1\", \"{\\\"count\\\":8}\"")
        ));
    
     mapHelpStrings.insert(std::make_pair("listvariables",
            "listvariables ( variable-identifier(s) verbose count start )\n"
            "\nReturns list of defined variables\n"
            "\nArguments:\n"
            "1. \"variable-identifier(s)\"           (string, optional, default=*) Variable identifier - one of: create txid, variable reference, variable name.\n"
            " or\n"
            "1. variable-identifier(s)             (array, optional) A json array of variable identifiers \n"                
            "2. verbose                          (boolean, optional, default=false) If true, last txid and writers \n"
            "3. count                            (number, optional, default=INT_MAX - all) The number of variables to display\n"
            "4. start                            (number, optional, default=-count - last) Start from specific variable, 0 based, if negative - from the end\n"
            "\nResult:\n"
            "An array containing list of defined variables\n"            
            "\nExamples:\n"
            + HelpExampleCli("listvariables", "")
            + HelpExampleRpc("listvariables", "")
        ));
    
     mapHelpStrings.insert(std::make_pair("getvariablehistory",
            "getvariablehistory ( \"variable-identifier\" verbose count start )\n"
            "\nReturns variable value changes history\n"
            "\nArguments:\n"
            "1. \"variable-identifier\"            (string, required) Variable identifier - one of: create txid, variable reference, variable name.\n"
            "2. verbose                          (boolean, optional, default=false) If true, returns txid and writer address\n"
            "3. count                            (number, optional, default=INT_MAX - all) The number of value changes to display\n"
            "4. start                            (number, optional, default=-count - last) Start from specific value change, 0 based, if negative - from the end\n"
            "\nResult:\n"
            "An array containing variable value changes history\n"            
            "\nExamples:\n"
            + HelpExampleCli("getvariablehistory", "\"var1\"")
            + HelpExampleRpc("getvariablehistory", "\"var1\"")
        ));
    
     mapHelpStrings.insert(std::make_pair("getvariableinfo",
            "getvariableinfo ( \"variable-identifier\" verbose )\n"
            "\nReturns information about defined variable\n"
            "\nArguments:\n"
            "1. \"variable-identifier\"            (string, required) Variable identifier - one of: create txid, variable reference, variable name.\n"
            "2. verbose                          (boolean, optional, default=false) If true, last txid and writers \n"
            "\nResult:\n"
            "An object containing information about defined variable\n"            
            "\nExamples:\n"
            + HelpExampleCli("getvariableinfo", "\"var1\"")
            + HelpExampleRpc("getvariableinfo", "\"var1\"")
        ));
    
     mapHelpStrings.insert(std::make_pair("getvariablevalue",
            "getvariablevalue ( \"variable-identifier\" )\n"
            "\nReturns current variable value\n"
            "\nArguments:\n"
            "1. \"variable-identifier\"            (string, required) Variable identifier - one of: create txid, variable reference, variable name.\n"
            "\nResult:\n"
            "Current variable value\n"            
            "\nExamples:\n"
            + HelpExampleCli("getvariablevalue", "\"var1\"")
            + HelpExampleRpc("getvariablevalue", "\"var1\"")
        ));
    
     mapHelpStrings.insert(std::make_pair("AAAAAAA",
            ""
        ));
    
    
}

void mc_InitRPCLogParamCountMap()
{
    mapLogParamCounts.insert(std::make_pair("encryptwallet",0));
    mapLogParamCounts.insert(std::make_pair("walletpassphrase",0));
    mapLogParamCounts.insert(std::make_pair("walletpassphrasechange",0));
    mapLogParamCounts.insert(std::make_pair("importprivkey",0));
    mapLogParamCounts.insert(std::make_pair("signrawtransaction",-1));
}

void mc_InitRPCAllowedWhenWaitingForUpgradeSet()
{
    setAllowedWhenWaitingForUpgrade.insert("getinfo");    
    setAllowedWhenWaitingForUpgrade.insert("help");    
    setAllowedWhenWaitingForUpgrade.insert("stop");    
    setAllowedWhenWaitingForUpgrade.insert("pause");    
    setAllowedWhenWaitingForUpgrade.insert("resume");    
    setAllowedWhenWaitingForUpgrade.insert("clearmempool");    
    setAllowedWhenWaitingForUpgrade.insert("setlastblock");    
    setAllowedWhenWaitingForUpgrade.insert("getblockchainparams");    
    setAllowedWhenWaitingForUpgrade.insert("getruntimeparams");    
    setAllowedWhenWaitingForUpgrade.insert("setruntimeparam");    
    setAllowedWhenWaitingForUpgrade.insert("getblockchaininfo");    
    setAllowedWhenWaitingForUpgrade.insert("getblockcount");    
    setAllowedWhenWaitingForUpgrade.insert("getblock");    
    setAllowedWhenWaitingForUpgrade.insert("getblockhash");    
    setAllowedWhenWaitingForUpgrade.insert("getmempoolinfo");    
    setAllowedWhenWaitingForUpgrade.insert("listupgrades");    
    setAllowedWhenWaitingForUpgrade.insert("decoderawtransaction");    
    setAllowedWhenWaitingForUpgrade.insert("getrawtransaction");    
    setAllowedWhenWaitingForUpgrade.insert("dumpprivkey");    
    setAllowedWhenWaitingForUpgrade.insert("getaddresses");    
    setAllowedWhenWaitingForUpgrade.insert("listaddresses");    
    setAllowedWhenWaitingForUpgrade.insert("createkeypairs");    
    setAllowedWhenWaitingForUpgrade.insert("createmultisig");    
    setAllowedWhenWaitingForUpgrade.insert("validateaddress");    
    setAllowedWhenWaitingForUpgrade.insert("addnode");    
    setAllowedWhenWaitingForUpgrade.insert("getpeerinfo");    
    setAllowedWhenWaitingForUpgrade.insert("signmessage");    
    setAllowedWhenWaitingForUpgrade.insert("verifymessage");    
}

void mc_InitRPCAllowedWhenOffline()
{
    setAllowedWhenOffline.insert("getblockchainparams");    
    setAllowedWhenOffline.insert("getinfo");    
    setAllowedWhenOffline.insert("help");    
    setAllowedWhenOffline.insert("stop");    
    setAllowedWhenOffline.insert("decodescript");    
    setAllowedWhenOffline.insert("signrawtransaction");    
    setAllowedWhenOffline.insert("createkeypairs");    
    setAllowedWhenOffline.insert("verifymessage");    
    setAllowedWhenOffline.insert("signmessage");    
    
    
    setAllowedWhenOffline.insert("addmultisigaddress");    
    setAllowedWhenOffline.insert("getaddresses");    
    setAllowedWhenOffline.insert("getnewaddress");    
    setAllowedWhenOffline.insert("importaddress");    
    setAllowedWhenOffline.insert("listaddresses");    
    setAllowedWhenOffline.insert("validateaddress");    
    setAllowedWhenOffline.insert("createmultisig");    
    setAllowedWhenOffline.insert("backupwallet");    
    setAllowedWhenOffline.insert("dumpprivkey");    
    setAllowedWhenOffline.insert("dumpwallet");    
    setAllowedWhenOffline.insert("encryptwallet");    
    setAllowedWhenOffline.insert("importprivkey");    
    setAllowedWhenOffline.insert("importwallet");    
    setAllowedWhenOffline.insert("walletlock");    
    setAllowedWhenOffline.insert("walletpassphrase");    
    setAllowedWhenOffline.insert("walletpassphrasechange");    
    
    setAllowedWhenOffline.insert("decodelicenserequest");    
    setAllowedWhenOffline.insert("decodelicenseconfirmation");    
    setAllowedWhenOffline.insert("getlicenseconfirmation");    
    
}

void mc_InitRPCHelpMap()
{
    mc_InitRPCHelpMap01();
    mc_InitRPCHelpMap02();
    mc_InitRPCHelpMap03();
    mc_InitRPCHelpMap04();
    mc_InitRPCHelpMap05();
    mc_InitRPCHelpMap06();
    mc_InitRPCHelpMap07();
    mc_InitRPCHelpMap08();
    mc_InitRPCHelpMap09();
    mc_InitRPCHelpMap10();
    mc_InitRPCHelpMap11();
    mc_InitRPCHelpMap12();
    mc_InitRPCHelpMap13();
    mc_InitRPCHelpMap14();
    mc_InitRPCHelpMap15();
    mc_InitRPCHelpMap16();
    mc_InitRPCHelpMap17();
    mc_InitRPCHelpMap18();
    mc_InitRPCHelpMap19();
    mc_InitRPCHelpMap20();
    mc_InitRPCHelpMap21();
    mc_InitRPCHelpMap22();
//    mc_InitRPCHelpMap23();
    
    pEF->ENT_InitRPCHelpMap();
    
    mc_InitRPCLogParamCountMap();
    mc_InitRPCAllowedWhenWaitingForUpgradeSet();    
    mc_InitRPCAllowedWhenOffline();    
}

Value purehelpitem(const Array& params, bool fHelp)
{
    if (fHelp)
        throw runtime_error("Help message not found\n");
    
    return Value::null; 
}

Value purehelpitem_nomethod(const Array& params, bool fHelp)
{
    if (fHelp)
        throw runtime_error("Help message not found\n");
    
    throw JSONRPCError(RPC_METHOD_NOT_FOUND, "Method not found");

    return Value::null; 
}

Value purehelpitem_onlyfilter(const Array& params, bool fHelp)
{
    if (fHelp)
        throw runtime_error("Help message not found\n");
    
    throw JSONRPCError(RPC_METHOD_NOT_FOUND, "This method can only be called from within a filter");

    return Value::null; 
}