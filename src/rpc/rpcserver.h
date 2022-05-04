// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2014-2016 The Bitcoin Core developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#ifndef BITCOIN_RPCSERVER_H
#define BITCOIN_RPCSERVER_H

#include "structs/amount.h"
#include "rpc/rpcprotocol.h"
#include "structs/uint256.h"

#include <list>
#include <map>
#include <stdint.h>
#include <string>

#include "json/json_spirit_reader_template.h"
#include "json/json_spirit_utils.h"
#include "json/json_spirit_writer_template.h"

class CBlockIndex;
class CNetAddr;

class AcceptedConnection
{
public:
    virtual ~AcceptedConnection() {}

    virtual std::iostream& stream() = 0;
    virtual std::string peer_address_to_string() const = 0;
    virtual void close() = 0;
    virtual uint32_t get_flags() = 0;
};

class JSONRequest
{
public:
    json_spirit::Value id;
    std::string strMethod;
    json_spirit::Array params;

    JSONRequest() { id = json_spirit::Value::null; }
    void parse(const json_spirit::Value& valRequest);
};

class RPCThreadLoad
{
public:
    int64_t size;
    int64_t start;
    int64_t end;
    int64_t last;
    int64_t total;
    int64_t load[10];
    RPCThreadLoad()
    {
        Zero();
    }
    
    void Zero();
    int64_t Calculate(int64_t *l,int64_t *s,int64_t *e);
    void Update();
    double ThreadLoad();
};
        
        
/** Start RPC threads */
void StartRPCThreads(std::string& strError);
/**
 * Alternative to StartRPCThreads for the GUI, when no server is
 * used. The RPC thread in this case is only used to handle timeouts.
 * If real RPC threads have already been started this is a no-op.
 */
void StartDummyRPCThread();
/** Stop RPC threads */
void StopRPCThreads();
/** Query whether RPC is running */
bool IsRPCRunning();

/** 
 * Set the RPC warmup status.  When this is done, all RPC calls will error out
 * immediately with RPC_IN_WARMUP.
 */
void SetRPCWarmupStatus(const std::string& newStatus);
/* Mark warmup as done.  RPC calls will be processed from now on.  */
void SetRPCWarmupFinished();

/* returns the current warmup state.  */
bool RPCIsInWarmup(std::string *statusOut);

/**
 * Type-check arguments; throws JSONRPCError if wrong type given. Does not check that
 * the right number of arguments are passed, just that any passed are the correct type.
 * Use like:  RPCTypeCheck(params, boost::assign::list_of(str_type)(int_type)(obj_type));
 */
void RPCTypeCheck(const json_spirit::Array& params,
                  const std::list<json_spirit::Value_type>& typesExpected, bool fAllowNull=false);
/**
 * Check for expected keys/value types in an Object.
 * Use like: RPCTypeCheck(object, boost::assign::map_list_of("name", str_type)("value", int_type));
 */
void RPCTypeCheck(const json_spirit::Object& o,
                  const std::map<std::string, json_spirit::Value_type>& typesExpected, bool fAllowNull=false);

/**
 * Run func nSeconds from now. Uses boost deadline timers.
 * Overrides previous timer <name> (if any).
 */
void RPCRunLater(const std::string& name, boost::function<void(void)> func, int64_t nSeconds);

/**
 * Returns RPC slot of this thread
 */

int GetRPCSlot();

typedef json_spirit::Value(*rpcfn_type)(const json_spirit::Array& params, bool fHelp);

class CRPCCommand
{
public:
    std::string category;
    std::string name;
    rpcfn_type actor;
    bool okSafeMode;
    bool threadSafe;
    bool reqWallet;
};

/**
 * Bitcoin RPC command dispatcher.
 */
class CRPCTable
{
private:
    std::map<std::string, const CRPCCommand*> mapCommands;
    std::map<std::string, const CRPCCommand*> mapWalletReadCommands;
public:
    CRPCTable();
    const CRPCCommand* operator[](std::string name) const;
    std::string help(std::string name) const;

    /**
     * Execute a method.
     * @param method   Method to execute
     * @param params   Array of arguments (JSON objects)
     * @returns Result of the call.
     * @throws an exception (json_spirit::Value) when an error happens.
     */
    void initialize();
    json_spirit::Value execute(const std::string &method, const json_spirit::Array &params, const json_spirit::Value& req_id) const;
};

extern CRPCTable tableRPC;

/* MCHN START */
extern std::map<std::string, std::string> mapHelpStrings;
extern std::map<std::string, int> mapLogParamCounts;
extern std::set<std::string> setAllowedWhenWaitingForUpgrade;
extern std::set<std::string> setAllowedWhenOffline;
extern std::set<std::string> setAllowedWhenLimited;
extern std::vector<CRPCCommand> vStaticRPCCommands;
extern std::vector<CRPCCommand> vStaticRPCWalletReadCommands;
void mc_InitRPCHelpMap();
std::string mc_RPCHelpString(std::string strMethod);
void mc_ThrowHelpMessage(std::string strMethod);
void mc_InitRPCList(std::vector<CRPCCommand>& vStaticRPCCommands,std::vector<CRPCCommand>& vStaticRPCWalletReadCommands);


/* MCHN END */

/**
 * Utilities: convert hex-encoded Values
 * (throws error if not hex).
 */
extern uint256 ParseHashV(const json_spirit::Value& v, std::string strName);
extern uint256 ParseHashO(const json_spirit::Object& o, std::string strKey);
extern std::vector<unsigned char> ParseHexV(const json_spirit::Value& v, std::string strName);
extern std::vector<unsigned char> ParseHexO(const json_spirit::Object& o, std::string strKey);

extern void InitRPCMining();
extern void ShutdownRPCMining();

extern int64_t nWalletUnlockTime;
extern CAmount AmountFromValue(const json_spirit::Value& value);
extern json_spirit::Value ValueFromAmount(const CAmount& amount);
extern double GetDifficulty(const CBlockIndex* blockindex = NULL);
extern std::string HelpRequiringPassphrase();
extern std::string HelpExampleCli(std::string methodname, std::string args);
extern std::string HelpExampleRpc(std::string methodname, std::string args);

extern void EnsureWalletIsUnlocked();

extern json_spirit::Value help(const json_spirit::Array& params, bool fHelp); 
extern json_spirit::Value stop(const json_spirit::Array& params, bool fHelp); 


extern json_spirit::Value getconnectioncount(const json_spirit::Array& params, bool fHelp); // in rpcnet.cpp
extern json_spirit::Value getpeerinfo(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value liststorednodes(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value ping(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value addnode(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value storenode(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getaddednodeinfo(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getnettotals(const json_spirit::Array& params, bool fHelp);

extern json_spirit::Value dumpprivkey(const json_spirit::Array& params, bool fHelp); // in rpcdump.cpp
extern json_spirit::Value importprivkey(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value importaddress(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value dumpwallet(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value importwallet(const json_spirit::Array& params, bool fHelp);

extern json_spirit::Value getgenerate(const json_spirit::Array& params, bool fHelp); // in rpcmining.cpp
extern json_spirit::Value setgenerate(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getnetworkhashps(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value gethashespersec(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getmininginfo(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value prioritisetransaction(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getblocktemplate(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value submitblock(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value estimatefee(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value estimatepriority(const json_spirit::Array& params, bool fHelp);

extern json_spirit::Value getnewaddress(const json_spirit::Array& params, bool fHelp); // in rpcwallet.cpp
extern json_spirit::Value getaccountaddress(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getrawchangeaddress(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value setaccount(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getaccount(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getaddressesbyaccount(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value sendtoaddress(const json_spirit::Array& params, bool fHelp);
/* MCHN START */    
extern json_spirit::Value debug(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getchunkqueueinfo(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getchunkqueuetotals(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value createkeypairs(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getaddresses(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value createbinarycache(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value appendbinarycache(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value deletebinarycache(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value combineunspent(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value grantcmd(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value revokecmd(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value issuecmd(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value issuemore(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value issuetoken(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value listassets(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value listpermissions(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value listminers(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getassetbalances(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value gettotalbalances(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value sendassettoaddress(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getblockchainparams(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getruntimeparams(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value setruntimeparam(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value preparelockunspent(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value createrawexchange(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value appendrawexchange(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value completerawexchange(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value decoderawexchange(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value appendrawmetadata(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value approvefrom(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value grantfromcmd(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value revokefromcmd(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value issuefromcmd(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value issuemorefrom(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value issuetokenfrom(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value sendassetfrom(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value preparelockunspentfrom(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value sendfromaddress(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value createrawsendfrom(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getmultibalances(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value gettokenbalances(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getaddressbalances(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value disablerawtransaction(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value sendwithmetadata(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value sendwithmetadatafrom(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value grantwithmetadata(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value grantwithmetadatafrom(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value listwallettransactions(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value listaddresstransactions(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getwallettransaction(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getaddresstransaction(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value appendrawchange(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value pausecmd(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value resumecmd(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value clearmempool(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value setlastblock(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value resendwallettransactions(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value listaddresses(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value liststreams(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value listupgrades(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value listtxfilters(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value liststreamfilters(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getfiltercode(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getfiltertxid(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value setfilterparam(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value testtxfilter(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value runtxfilter(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value teststreamfilter(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value runstreamfilter(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getassetinfo(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getstreaminfo(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value verifypermission(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getfiltertxinput(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value createcmd(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value createfromcmd(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value updatecmd(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value updatefromcmd(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value publish(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value publishfrom(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value publishmulti(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value publishmultifrom(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value subscribe(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value unsubscribe(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value trimsubscribe(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value retrievestreamitems(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value purgestreamitems(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value purgepublisheditems(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value listassettransactions(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getassettransaction(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getstreamitem(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value liststreamtxitems(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value liststreamitems(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value liststreamkeyitems(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value liststreamqueryitems(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value liststreampublisheritems(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value liststreamkeys(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value liststreampublishers(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value gettxoutdata(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value txouttobinarycache(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value listblocks(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value liststreamblockitems(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getstreamkeysummary(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getstreampublishersummary(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value storechunk(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getlicenserequest(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value decodelicenserequest(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value decodelicenseconfirmation(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value activatelicense(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value activatelicensefrom(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value transferlicense(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value takelicense(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value listlicenses(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getlicenseconfirmation(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value importlicenserequest(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getinitstatus(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getchaintotals(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value gethealthcheck(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value listvariables(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getvariableinfo(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value setvariablevalue(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value setvariablevaluefrom(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getvariablevalue(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getvariablehistory(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value listassetissues(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value gettokeninfo(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value listlibraries(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value addlibraryupdate(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value addlibraryupdatefrom(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getlibrarycode(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value testlibrary(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getdiagnostics(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value applycommands(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value decodehexubjson(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value encodehexubjson(const json_spirit::Array& params, bool fHelp);

extern json_spirit::Value explorerlisttransactions(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value explorerlistaddresses(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value explorerlistaddresstransactions(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value explorerlistblocktransactions(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value explorerlistredeemtransactions(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value explorerlistaddressassets(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value explorerlistaddressstreams(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value explorerlistassetaddresses(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value explorerlistaddressassettransactions(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value explorergetrawtransaction(const json_spirit::Array& params, bool fHelp);



extern json_spirit::Value purehelpitem(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value purehelpitem_nomethod(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value purehelpitem_onlyfilter(const json_spirit::Array& params, bool fHelp);

extern json_spirit::Value createfeed(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value deletefeed(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value pausefeed(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value resumefeed(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value addtofeed(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value updatefeed(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value purgefeed(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value listfeeds(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getdatarefdata(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value datareftobinarycache(const json_spirit::Array& params, bool fHelp);

/* MCHN END */    
extern json_spirit::Value signmessage(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value verifymessage(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getreceivedbyaddress(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getreceivedbyaccount(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getbalance(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getunconfirmedbalance(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value movecmd(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value sendfrom(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value sendmany(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value addmultisigaddress(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value createmultisig(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value listreceivedbyaddress(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value listreceivedbyaccount(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value listtransactions(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value listaddressgroupings(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value listaccounts(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value listsinceblock(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value gettransaction(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value backupwallet(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value keypoolrefill(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value walletpassphrase(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value walletpassphrasechange(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value walletlock(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value encryptwallet(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value validateaddress(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getinfo(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getwalletinfo(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getblockchaininfo(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getnetworkinfo(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value setmocktime(const json_spirit::Array& params, bool fHelp);

extern json_spirit::Value getrawtransaction(const json_spirit::Array& params, bool fHelp); // in rcprawtransaction.cpp
extern json_spirit::Value getfiltertransaction(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getfilterstreamitem(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getfilterstream(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getfilterassetbalances(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getfiltertokenbalances(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value listunspent(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value lockunspent(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value listlockunspent(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value appendrawtransaction(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value createrawtransaction(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value decoderawtransaction(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value decodescript(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value signrawtransaction(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value sendrawtransaction(const json_spirit::Array& params, bool fHelp);

extern json_spirit::Value getblockcount(const json_spirit::Array& params, bool fHelp); // in rpcblockchain.cpp
extern json_spirit::Value getbestblockhash(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getdifficulty(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value settxfee(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getmempoolinfo(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getrawmempool(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getblockhash(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getblock(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getlastblockinfo(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value gettxoutsetinfo(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value gettxout(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value verifychain(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getchaintips(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value invalidateblock(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value reconsiderblock(const json_spirit::Array& params, bool fHelp);

// in rest.cpp
extern bool HTTPReq_REST(AcceptedConnection *conn,
                  std::string& strURI,
                  std::map<std::string, std::string>& mapHeaders,
                  bool fRun);

#endif // BITCOIN_RPCSERVER_H
