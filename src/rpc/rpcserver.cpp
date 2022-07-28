// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2014-2016 The Bitcoin Core developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include <rpc/rpchttpserver.h>
#include "rpc/rpcprotocol.h"
#include <chainparams/chainparamsbase.h>
#include <utils/util.h>
#include <net/netbase.h>
#include <core/init.h>

#include "rpc/rpcserver.h"
#include "structs/base58.h"
#include "core/main.h"
#include "ui/ui_interface.h"
#ifdef ENABLE_WALLET
#include "wallet/wallet.h"
#endif
#include "community/community.h"
#include "json/json_spirit_writer_template.h"

#include <boost/algorithm/string.hpp>

#include <deque>
#include <memory>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <condition_variable>
#include <mutex>
#include <thread>

#include <sys/types.h>
#include <sys/stat.h>

#include <event2/thread.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/util.h>
#include <event2/keyvalq_struct.h>

#include <rpc/rpcevents.h>


using namespace json_spirit;
using namespace std;

static std::string strRPCUserColonPass;

static bool fRPCRunning = false;
static bool fRPCInterrupted = false;
static bool fRPCInWarmup = true;
static std::string rpcWarmupStatus("RPC server started");
static CCriticalSection cs_rpcWarmup;

static map<uint64_t, RPCThreadLoad> rpc_loads;
static map<uint64_t, int> rpc_slots;
static uint32_t rpc_thread_flags[MC_PRM_MAX_THREADS];

void LockWallet(CWallet* pWallet);
int TxThrottlingDelay(bool print);

#define MC_ACF_NONE              0x00000000 
#define MC_ACF_ENTERPRISE        0x00000001 

#define MC_RPC_FLAG_NONE              0x00000000 
#define MC_RPC_FLAG_WRP_READ_LOCK     0x00000001 
#define MC_RPC_FLAG_NEW_TX            0x00000002 

namespace std {
    template<class T> struct _Unique_if {
        typedef unique_ptr<T> _Single_object;
    };

    template<class T> struct _Unique_if<T[]> {
        typedef unique_ptr<T[]> _Unknown_bound;
    };

    template<class T, size_t N> struct _Unique_if<T[N]> {
        typedef void _Known_bound;
    };

    template<class T, class... Args>
        typename _Unique_if<T>::_Single_object
        make_unique(Args&&... args) {
            return unique_ptr<T>(new T(std::forward<Args>(args)...));
        }

    template<class T>
        typename _Unique_if<T>::_Unknown_bound
        make_unique(size_t n) {
            typedef typename remove_extent<T>::type U;
            return unique_ptr<T>(new U[n]());
        }

    template<class T, class... Args>
        typename _Unique_if<T>::_Known_bound
        make_unique(Args&&...) = delete;
}
/*
template<typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&&... args) {
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}
*/

/** WWW-Authenticate to present with 401 Unauthorized response */
static const char* WWW_AUTH_HEADER_DATA = "Basic realm=\"jsonrpc\"";

/** Maximum size of http request (request line + headers) */
static const size_t MAX_HEADERS_SIZE = 8192;

/** HTTP request work item */
class HTTPWorkItem final : public HTTPClosure
{
public:
    HTTPWorkItem(std::unique_ptr<HTTPRequest> _req, const std::string &_path, const HTTPRequestHandler& _func):
        req(std::move(_req)), path(_path), func(_func)
    {
    }
    void operator()() override
    {
        func(req.get(), path);
    }

    std::unique_ptr<HTTPRequest> req;

private:
    std::string path;
    HTTPRequestHandler func;
};

/** Simple work queue for distributing work over multiple threads.
 * Work items are simply callable objects.
 */
template <typename WorkItem>
class WorkQueue
{
private:
    boost::condition_variable cond;
    boost::mutex mutex;
    
    std::deque<std::unique_ptr<WorkItem>> queue;
    bool running;
    const size_t maxDepth;

public:
    explicit WorkQueue(size_t _maxDepth) : running(true),
                                 maxDepth(_maxDepth)
    {
    }
    /** Precondition: worker threads have all stopped (they have been joined).
     */
    ~WorkQueue()
    {
    }
    /** Enqueue a work item */
    bool Enqueue(WorkItem* item)
    {
        boost::unique_lock<boost::mutex> lock(mutex);
        if (!running || queue.size() >= maxDepth) {
            return false;
        }
        queue.emplace_back(std::unique_ptr<WorkItem>(item));
        cond.notify_one();
        return true;
    }
    
    /** Thread function */
    void Run()
    {
        uint64_t thread_id=__US_ThreadID();
        while (true) {
            std::unique_ptr<WorkItem> i;
            {
                boost::unique_lock<boost::mutex> lock(mutex);
                
                while (running && queue.empty())
                    cond.wait(lock);
                if (!running && queue.empty())
                    break;
                i = std::move(queue.front());
                queue.pop_front();
            }
            map<uint64_t,RPCThreadLoad>::iterator load_it=rpc_loads.find(thread_id);
            if(load_it != rpc_loads.end())
            {
                if(nWalletUnlockTime)
                {
                    if(GetTime() > nWalletUnlockTime)
                    {
                        LockWallet(pwalletMain);
                    }
                }
                load_it->second.start=GetTimeMicros();
            }
            (*i)();
            if(load_it != rpc_loads.end())
            {
                load_it->second.end=GetTimeMicros();
                load_it->second.Update();
            }
        }
    }

    /** Interrupt and exit loops */
    void Interrupt()
    {
        boost::unique_lock<boost::mutex> lock(mutex);
        running = false;
        cond.notify_all();
    }
};

struct HTTPPathHandler
{
    HTTPPathHandler(std::string _prefix, bool _exactMatch, HTTPRequestHandler _handler):
        prefix(_prefix), exactMatch(_exactMatch), handler(_handler)
    {
    }
    std::string prefix;
    bool exactMatch;
    HTTPRequestHandler handler;
};


void RPCThreadLoad::Zero()
{
    size=10;
    last=GetTimeMicros();
    start=0;
    end=0;
    total=0;
    
    memset(load,0, size*sizeof(double));    
}

void RPCThreadLoad::Update()
{
    int64_t m=1000000;
    if(start == 0)
    {
        return;
    }
    
    int64_t last_sec=last/m;
    int64_t start_sec=start/m;
    int64_t end_sec=end/m;
    
    if(last+size*m <= start)
    {
        memset(load,0, size*sizeof(double));    
    }
    else
    {
        for(int64_t i=last_sec+1;i<=start_sec;i++)
        {
            load[i%size]=0;
        }
    }
    
    if(start+size*m <= end)
    {
        for(int64_t i=0;i<size;i++)
        {
            load[i]=m;
        }        
    }
    else
    {
        if(start_sec == end_sec)
        {
            load[start_sec%size]+=end-start;
        }
        else
        {
            for(int64_t i=start_sec+1;i<=end_sec-1;i++)
            {
                load[i%size]=m;
            }        
            load[start_sec%size]+=m-(start%m);
            load[end_sec%size]=end%m;       
        }
    }
    total=0;
    for(int64_t i=0;i<size;i++)
    {            
        total+=load[i];
    }                
    
    start=0;    
    last=end;
    end=0;
}

double RPCThreadLoad::ThreadLoad()
{    
    int64_t now=GetTimeMicros();
    
    RPCThreadLoad test;
    memcpy(&test,this,sizeof(RPCThreadLoad));
    if(test.start == 0)
    {        
        test.start=now;
    }
    test.end=now;
    test.Update();

    return (double)test.total/(double)(test.size*1000000.);
}

double TotalRPCLoad(int *free_threads,int *total_threads)
{   
    double result=0.;
    if(total_threads)
    {
        *total_threads=(int)rpc_loads.size();
    }
    if(free_threads)
    {
        *free_threads=0;
    }
    
    for (map<uint64_t,RPCThreadLoad>::iterator it = rpc_loads.begin(); it != rpc_loads.end(); ++it)
    {
        result+=it->second.ThreadLoad();
        if(free_threads)
        {
            if(it->second.start == 0)
            {
                *free_threads+=1;
            }
        }
    }   
    
    if(rpc_loads.size())
    {
        result /= rpc_loads.size();
    }
    
    return result;
}

string JSONRPCRequestForLog(const string& strMethod, const Array& params, const Value& id)
{
    Object request;
    request.push_back(Pair("method", strMethod));
    map<string, int>::iterator it = mapLogParamCounts.find(strMethod);
    if (it != mapLogParamCounts.end())
    {
        Array visible_params;
        if(it->second != 0)
        {
            if(strMethod == "signrawtransaction")
            {
                visible_params=params;
                if (visible_params.size() > 2 && visible_params[2].type() != null_type) 
                {
                    visible_params[2]="[<PRIVATE KEYS>]";
                }
            }
        }
        request.push_back(Pair("params", visible_params));
    }
    else
    {
        request.push_back(Pair("params", params));
    }
    request.push_back(Pair("id", id));
    return write_string(Value(request), false);// + "\n";
}

string JSONRPCMethodIDForLog(const string& strMethod, const Value& id)
{
    Object request;
    request.push_back(Pair("method", strMethod));
    request.push_back(Pair("id", id));
    return write_string(Value(request), false);// + "\n";
}


void RPCTypeCheck(const Array& params,
                  const list<Value_type>& typesExpected,
                  bool fAllowNull)
{
    unsigned int i = 0;
    BOOST_FOREACH(Value_type t, typesExpected)
    {
        if (params.size() <= i)
            break;

        const Value& v = params[i];
        if (!((v.type() == t) || (fAllowNull && (v.type() == null_type))))
        {
            string err = strprintf("Expected type %s, got %s",
                                   Value_type_name[t], Value_type_name[v.type()]);
            throw JSONRPCError(RPC_TYPE_ERROR, err);
        }
        i++;
    }
}

void RPCTypeCheck(const Object& o,
                  const map<string, Value_type>& typesExpected,
                  bool fAllowNull)
{
    BOOST_FOREACH(const PAIRTYPE(string, Value_type)& t, typesExpected)
    {
        const Value& v = find_value(o, t.first);
        if (!fAllowNull && v.type() == null_type)
            throw JSONRPCError(RPC_TYPE_ERROR, strprintf("Missing %s", t.first));

        if (!((v.type() == t.second) || (fAllowNull && (v.type() == null_type))))
        {
            string err = strprintf("Expected type %s for %s, got %s",
                                   Value_type_name[t.second], t.first, Value_type_name[v.type()]);
            throw JSONRPCError(RPC_TYPE_ERROR, err);
        }
    }
}

static inline int64_t roundint64(double d)
{
    return (int64_t)(d > 0 ? d + 0.5 : d - 0.5);
}

CAmount AmountFromValue(const Value& value)
{
    double dAmount = value.get_real();
    if(COIN == 0)
    {
        if(dAmount != 0)
        {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid amount");            
        }
    }
    else
    {
        if (dAmount < 0.0 || dAmount > (double)MAX_MONEY/(double)COIN)                                  // MCHN - was <=
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid amount");        
    }
    
    CAmount nAmount = roundint64(dAmount * COIN);
    if (!MoneyRange(nAmount))
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid amount");
    return nAmount;
}

Value ValueFromAmount(const CAmount& amount)
{
    if(COIN == 0)
    {
        return (double)amount;
    }
    return (double)amount / (double)COIN;
}

uint256 ParseHashV(const Value& v, string strName)
{
    string strHex;
    if (v.type() == str_type)
        strHex = v.get_str();
    if (!IsHex(strHex)) // Note: IsHex("") is false
        throw JSONRPCError(RPC_INVALID_PARAMETER, strName+" must be hexadecimal string (not '"+strHex+"')");
    if (64 != strHex.length())
        throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("%s must be of length %d (not %d)", strName, 64, strHex.length()));
     uint256 result;
    result.SetHex(strHex);
    return result;
}
uint256 ParseHashO(const Object& o, string strKey)
{
    return ParseHashV(find_value(o, strKey), strKey);
}
vector<unsigned char> ParseHexV(const Value& v, string strName)
{
    string strHex;
    if (v.type() == str_type)
        strHex = v.get_str();
    if (!IsHex(strHex))
        throw JSONRPCError(RPC_INVALID_PARAMETER, strName+" must be hexadecimal string (not '"+strHex+"')");
    return ParseHex(strHex);
}
vector<unsigned char> ParseHexO(const Object& o, string strKey)
{
    return ParseHexV(find_value(o, strKey), strKey);
}


/**
 * Note: This interface may still be subject to change.
 */

string CRPCTable::help(string strCommand) const
{
    string strRet;
    string category;
    set<rpcfn_type> setDone;
    vector<pair<string, const CRPCCommand*> > vCommands;

    for (map<string, const CRPCCommand*>::const_iterator mi = mapCommands.begin(); mi != mapCommands.end(); ++mi)
        vCommands.push_back(make_pair(mi->second->category + mi->first, mi->second));
    sort(vCommands.begin(), vCommands.end());

    BOOST_FOREACH(const PAIRTYPE(string, const CRPCCommand*)& command, vCommands)
    {
        const CRPCCommand *pcmd = command.second;
        string strMethod = pcmd->name;
        // We already filter duplicates, but these deprecated screw up the sort order
        if (strMethod.find("label") != string::npos)
            continue;
        if ((strCommand != "" || pcmd->category == "hidden") && strMethod != strCommand)
            continue;
#ifdef ENABLE_WALLET
        if (pcmd->reqWallet && !pwalletMain)                                    // Never happens, reqWallet is changed to require Wallet read lock 
            continue;
#endif

        string strHelp="";
        map<string, string>::iterator it = mapHelpStrings.find(strMethod);
        if (it == mapHelpStrings.end())
        {
            try
            {
                Array params;
                rpcfn_type pfn = pcmd->actor;
                if (setDone.insert(pfn).second)
                    (*pfn)(params, true);
            }
            catch (std::exception& e)
            {
                if(strCommand != "")
                {
                    strHelp = string(e.what());
                }
            }
        }
        else
        {
            strHelp=it->second;
        }                            
        
        if(strHelp != "")
        {
            if (strCommand == "")
            {
                if (strHelp.find('\n') != string::npos)
                    strHelp = strHelp.substr(0, strHelp.find('\n'));

                if (category != pcmd->category)
                {
                    if (!category.empty())
                        strRet += "\n";
                    category = pcmd->category;
                    string firstLetter = category.substr(0,1);
                    boost::to_upper(firstLetter);
                    strRet += "== " + firstLetter + category.substr(1) + " ==\n";
                }
            }
            strRet += strHelp + "\n";            
        }
    }
    
    if (strRet == "")
        strRet = strprintf("help: unknown command: %s\n", strCommand);
    strRet = strRet.substr(0,strRet.size()-1);
    return strRet;
}

Value help(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error("Help message not found\n");

    string strCommand;
    if (params.size() > 0)
        strCommand = params[0].get_str();
    
    if(strCommand.size())
    {
        if(setAllowedWhenLimited.size())
        {
            if( setAllowedWhenLimited.count(strCommand) == 0 )
            {
                throw JSONRPCError(RPC_NOT_ALLOWED, "Method not allowed with current setting of -rpcallowmethod runtime parameter");                
            }        
        }
    }
    
    return tableRPC.help(strCommand);
}


Value stop(const Array& params, bool fHelp)
{
    // Accept the deprecated and ignored 'detach' boolean argument
    if (fHelp || params.size() > 1)
        throw runtime_error("Help message not found\n");
    // Shutdown will take long enough that the response should get back
    StartShutdown();
    return "MultiChain server stopping";
}

string AllowedPausedServices()
{
    string ret="incoming,mining,offchain";
    
    return ret;
}

uint32_t GetPausedServices(const char *str)
{
    uint32_t result,type;
    char* ptr;
    char* start;
    char* ptrEnd;
    char c;
    
    ptr=(char*)str;
    ptrEnd=ptr+strlen(ptr);
    start=ptr;
    
    result=0;
    
    while(ptr<=ptrEnd)
    {
        c=*ptr;
        if( (c == ',') || (c ==0x00))
        {
            if(ptr > start+4)
            {
                type=0;
                if(memcmp(start,"incoming",    ptr-start) == 0)type = MC_NPS_INCOMING;
                if(memcmp(start,"mining",      ptr-start) == 0)type = MC_NPS_MINING;
                if(memcmp(start,"reaccepting", ptr-start) == 0)type = MC_NPS_REACCEPT;
                if(memcmp(start,"offchain",    ptr-start) == 0)type = MC_NPS_OFFCHAIN;
                if(memcmp(start,"chunks",      ptr-start) == 0)type = MC_NPS_CHUNKS;
                
                if(type == 0)
                {
                    return 0;
                }
                result |= type;
                start=ptr+1;
            }
        }
        ptr++;
    }
    
    return  result;
}


Value pausecmd(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 1)
        throw runtime_error("Help message not found\n");
    
    uint32_t type=0;
    if (params.size() > 0 && params[0].type() != null_type && !params[0].get_str().empty())
    {
        type=GetPausedServices(params[0].get_str().c_str());
    }
    
    if(type == 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid task");
    
    LOCK(cs_main);
    
    mc_gState->m_NodePausedState |= type;
    
    LogPrintf("Node paused state is set to %08X\n",mc_gState->m_NodePausedState);
    
    return "Paused";
}

Value resumecmd(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 1)
        throw runtime_error("Help message not found\n");
    
    uint32_t type=0;
    if (params.size() > 0 && params[0].type() != null_type && !params[0].get_str().empty())
    {
        type=GetPausedServices(params[0].get_str().c_str());
    }
    
    if(type == 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid task");
    
    LOCK(cs_main);
    
    mc_gState->m_NodePausedState &= (MC_NPS_ALL ^ type);
    
    if( type & MC_NPS_REACCEPT )
    {
        pwalletMain->ReacceptWalletTransactions();                                                                                            
    }
    
    LogPrintf("Node paused state is set to %08X\n",mc_gState->m_NodePausedState);
    
    return "Resumed";
}


CRPCTable::CRPCTable()
{
}

void CRPCTable::initialize()
{
    unsigned int vcidx;
    for (vcidx = 0; vcidx < vStaticRPCCommands.size(); vcidx++)
    {
        const CRPCCommand *pcmd;

        pcmd = &vStaticRPCCommands[vcidx];
        mapCommands[pcmd->name] = pcmd;
    }
    for (vcidx = 0; vcidx < vStaticRPCWalletReadCommands.size(); vcidx++)
    {
        const CRPCCommand *pcmd;

        pcmd = &vStaticRPCWalletReadCommands[vcidx];
        mapWalletReadCommands[pcmd->name] = pcmd;
    }
}

const CRPCCommand *CRPCTable::operator[](string name) const
{
    map<string, const CRPCCommand*>::const_iterator it = mapCommands.find(name);
    if (it == mapCommands.end())
        return NULL;
    return (*it).second;
}

void mc_InitRPCListIfLimited()
{
    if (mapArgs.count("-rpcallowmethod")) 
    {
        setAllowedWhenLimited.insert("help");
        BOOST_FOREACH(const std::string& methods, mapMultiArgs["-rpcallowmethod"]) 
        {
            stringstream ss(methods); 
            string tok;
            while(getline(ss, tok, ',')) 
            {
                setAllowedWhenLimited.insert(tok);    
            }
        }
    }
}

int IsRPCWRPReadLockFlagSet() 
{
    uint64_t thread_id=__US_ThreadID();
    map<uint64_t,int>::iterator slot_it=rpc_slots.find(thread_id);
    if(slot_it != rpc_slots.end())
    {
        return (rpc_thread_flags[slot_it->second] & MC_RPC_FLAG_WRP_READ_LOCK);
    }    
    return 0;
}

void CheckFlagsOnException(const string& strMethod,const Value& req_id,const string& message)
{
    if(IsRPCWRPReadLockFlagSet())
    {
        LogPrintf("WARNING: Unlocking wallet after failure: method: %s, error: %s\n",JSONRPCMethodIDForLog(strMethod,req_id).c_str(),message);
        pwalletTxsMain->WRPReadUnLock();
    }   
    {
        LOCK(cs_rpcWarmup);
    
        if(!fRPCInWarmup)
        {
            mc_gState->m_Assets->ThreadCleanse(__US_ThreadID());
        }
    }
}




bool IsRPCRunning()
{
    return fRPCRunning;
}

void SetRPCWarmupStatus(const std::string& newStatus)
{
    LOCK(cs_rpcWarmup);
    rpcWarmupStatus = newStatus;
}

void SetRPCWarmupFinished()
{
    LOCK(cs_rpcWarmup);
    assert(fRPCInWarmup);
    fRPCInWarmup = false;
}

bool RPCIsInWarmup(std::string *outStatus)
{
    LOCK(cs_rpcWarmup);
    if (outStatus)
        *outStatus = rpcWarmupStatus;
    return fRPCInWarmup;
}

void RPCRunLater(const std::string& name, boost::function<void(void)> func, int64_t nSeconds)
{
/* TODO */    
}

void JSONRequest::parse(const Value& valRequest)
{
    // Parse request
    if (valRequest.type() != obj_type)
        throw JSONRPCError(RPC_INVALID_REQUEST, "Invalid Request object");
    const Object& request = valRequest.get_obj();

    // Parse id now so errors from here on will have the id
    id = find_value(request, "id");

    // Parse method
    Value valMethod = find_value(request, "method");
    if (valMethod.type() == null_type)
        throw JSONRPCError(RPC_INVALID_REQUEST, "Missing method");
    if (valMethod.type() != str_type)
        throw JSONRPCError(RPC_INVALID_REQUEST, "Method must be a string");
    strMethod = valMethod.get_str();
    if (strMethod != "getblocktemplate")
        if(fDebug)LogPrint("rpc", "ThreadRPCServer method=%s\n", SanitizeString(strMethod));

    Value valChainName = find_value(request, "chain_name");
    if (valChainName.type() != null_type)
    {
        if (valChainName.type() != str_type)
            throw JSONRPCError(RPC_INVALID_REQUEST, "Chain name must be a string");
        if (strcmp(valChainName.get_str().c_str(),mc_gState->m_Params->NetworkName()))
            throw JSONRPCError(RPC_INVALID_REQUEST, "Wrong chain name");
    }
    // Parse params
    Value valParams = find_value(request, "params");
    if (valParams.type() == array_type)
        params = valParams.get_array();
    else if (valParams.type() == null_type)
        params = Array();
    else
        throw JSONRPCError(RPC_INVALID_REQUEST, "Params must be an array");
}

static Object JSONRPCExecOne(const Value& req)
{
    Object rpc_result;

    JSONRequest jreq;
    uint32_t wallet_mode=mc_gState->m_WalletMode;
    try {
        jreq.parse(req);

        Value result = tableRPC.execute(jreq.strMethod, jreq.params,jreq.id);
        rpc_result = JSONRPCReplyObj(result, Value::null, jreq.id);
    }
    catch (Object& objError)
    {
        mc_gState->m_WalletMode=wallet_mode;
        string strReply = JSONRPCReply(Value::null, objError, jreq.id);
        CheckFlagsOnException(jreq.strMethod,jreq.id,strReply);
        
        if(fDebug)LogPrint("mcapi","mcapi: API request failure A: %s\n",JSONRPCMethodIDForLog(jreq.strMethod,jreq.id).c_str());        
        rpc_result = JSONRPCReplyObj(Value::null, objError, jreq.id);
    }
    catch (std::exception& e)
    {
        mc_gState->m_WalletMode=wallet_mode;
        CheckFlagsOnException(jreq.strMethod,jreq.id,e.what());
        if(fDebug)LogPrint("mcapi","mcapi: API request failure B: %s\n",JSONRPCMethodIDForLog(jreq.strMethod,jreq.id).c_str());        
        rpc_result = JSONRPCReplyObj(Value::null,
                                     JSONRPCError(RPC_PARSE_ERROR, e.what()), jreq.id);
    }

    return rpc_result;
}

static string JSONRPCExecBatch(const Array& vReq)
{
    Array ret;
    for (unsigned int reqIdx = 0; reqIdx < vReq.size(); reqIdx++)
        ret.push_back(JSONRPCExecOne(vReq[reqIdx]));

    return write_string(Value(ret), false) + "\n";
}

Array JSONRPCExecInternalBatch(const Array& vReq)
{
    Array ret;
    for (unsigned int reqIdx = 0; reqIdx < vReq.size(); reqIdx++)
    {
        bool take_it=true;
        Value defaultaddress=find_value(vReq[reqIdx].get_obj(), "defaultaddress");
        if( (defaultaddress.type() == str_type) && (defaultaddress.get_str().size() > 0) )
        {
            take_it=false;
            if(defaultaddress == CBitcoinAddress(pwalletMain->vchDefaultKey.GetID()).ToString())
            {
                take_it=true;
            }
        }        
        if(take_it)
        {
            Object result=JSONRPCExecOne(vReq[reqIdx]);
            Value error=find_value(result, "error");
            ret.push_back(result);
            Value ignore_error=true;
            if( (error.type() == str_type) && (error.get_str().size() > 0) )
            {
                ignore_error=find_value(vReq[reqIdx].get_obj(), "ignoreerror");
                if(ignore_error.type() != bool_type)
                {
                    ignore_error=false;
                }
            }
            if(!ignore_error.get_bool())
            {
                return ret;
            }            
        }
        else
        {
            JSONRequest jreq;
            jreq.parse(vReq[reqIdx]);
            Object result=JSONRPCReplyObj(Value::null,JSONRPCError(RPC_WALLET_ADDRESS_NOT_FOUND, "Ignored for this default address"), jreq.id);
            ret.push_back(result);
        }
    }

    return ret;
}

bool  HTTPReq_JSONRPC(string& strRequest, uint32_t flags, string& strReply, string& strHeader, map<string, string>& mapHeaders,int& http_code,json_spirit::Value& valError,json_spirit::Value& req_id)
{
    JSONRequest jreq;
    bool jreq_parsed=false;
    req_id=0;
    uint32_t wallet_mode=mc_gState->m_WalletMode;
    
    try
    {
        if(flags & MC_ACF_ENTERPRISE)
        {
            {
                LOCK(cs_rpcWarmup);
                if (fRPCInWarmup)
                {
                    throw JSONRPCError(RPC_IN_WARMUP, rpcWarmupStatus);                    
                }
            }
            strReply=pEF->HCH_ProcessRequest(strRequest,&strHeader,mapHeaders,http_code);
            string reason; 
            if(http_code != HTTP_OK)
            {
                if(pEF->LIC_VerifyFeature(MC_EFT_HEALTH_CHECK,reason) == 0)
                {
                    for(int s=0;s<20;s++)
                    {
                        if(!fRPCInterrupted)
                        {
                            MilliSleep(1000);        
                        }
                    }
                }
            }
        }
        else
        {            
            // Parse request
            Value valRequest;
            if (!read_string(strRequest, valRequest))
                throw JSONRPCError(RPC_PARSE_ERROR, "Parse error");
            // Return immediately if in warmup
            {
                LOCK(cs_rpcWarmup);
                if (fRPCInWarmup)
                {
                    jreq.parse(valRequest);
                    jreq_parsed=true;
                    if( (jreq.strMethod != "stop") && (jreq.strMethod != "getinitstatus") )
                    {
                        throw JSONRPCError(RPC_IN_WARMUP, rpcWarmupStatus);                    
                    }
                }
            }

            // singleton request
            if (valRequest.type() == obj_type) {
                if(!jreq_parsed)
                {
                    jreq.parse(valRequest);
                }

                req_id=jreq.id;
                Value result = tableRPC.execute(jreq.strMethod, jreq.params,jreq.id);

                strReply = JSONRPCReply(result, Value::null, jreq.id);

            // array of requests
            } else if (valRequest.type() == array_type)
                strReply = JSONRPCExecBatch(valRequest.get_array());
            else
                throw JSONRPCError(RPC_PARSE_ERROR, "Top-level object parse error");
            
            strHeader=HTTPReplyHeader(HTTP_OK, false, strReply.size());
            mapHeaders.insert(make_pair("Content-Length",strprintf("%u",strReply.size())));
            mapHeaders.insert(make_pair("Content-Type","application/json"));
        }
    }
    catch (Object& objError)
    {
        mc_gState->m_WalletMode=wallet_mode;
        string strReply = JSONRPCReply(Value::null, objError, jreq.id);
        CheckFlagsOnException(jreq.strMethod,jreq.id,strReply);
        
        if(fDebug)LogPrint("mcapi","mcapi: API request failure: %s, code: %d\n",JSONRPCMethodIDForLog(jreq.strMethod,jreq.id).c_str(),find_value(objError, "code").get_int());
        
        valError=objError;
        return false;
    }
    catch (std::exception& e)
    {
        mc_gState->m_WalletMode=wallet_mode;
        CheckFlagsOnException(jreq.strMethod,jreq.id,e.what());
        if(fDebug)LogPrint("mcapi","mcapi: API request failure D: %s\n",JSONRPCMethodIDForLog(jreq.strMethod,jreq.id).c_str());        
        valError=JSONRPCError(RPC_PARSE_ERROR, e.what());
        return false;
    }
    
    mapHeaders.insert(make_pair("Server",strprintf("multichain-json-rpc/%s",FormatFullMultiChainVersion())));

    return true;    
}

int GetRPCSlot()
{
    uint64_t thread_id=__US_ThreadID();
    map<uint64_t,int>::iterator slot_it=rpc_slots.find(thread_id);
    if(slot_it != rpc_slots.end())
    {
        return slot_it->second;
    }
    
    return -1;
}

void SetRPCWRPReadLockFlag(int lock)
{
    uint64_t thread_id=__US_ThreadID();
    map<uint64_t,int>::iterator slot_it=rpc_slots.find(thread_id);
    if(slot_it != rpc_slots.end())
    {
        if(lock)
        {
            rpc_thread_flags[slot_it->second] |= MC_RPC_FLAG_WRP_READ_LOCK;
        }
        else
        {
            if(rpc_thread_flags[slot_it->second] & MC_RPC_FLAG_WRP_READ_LOCK)rpc_thread_flags[slot_it->second]-=MC_RPC_FLAG_WRP_READ_LOCK;
        }
    }    
}

void SetRPCNewTxFlag()
{
    uint64_t thread_id=__US_ThreadID();
    map<uint64_t,int>::iterator slot_it=rpc_slots.find(thread_id);
    if(slot_it != rpc_slots.end())
    {
        rpc_thread_flags[slot_it->second] |= MC_RPC_FLAG_NEW_TX;
    }    
}

void ThrottleTxFlow()
{
    uint64_t thread_id=__US_ThreadID();
    map<uint64_t,int>::iterator slot_it=rpc_slots.find(thread_id);
    if(slot_it != rpc_slots.end())
    {
        if(rpc_thread_flags[slot_it->second] & MC_RPC_FLAG_NEW_TX)
        {
            int tx_throttling_delay=TxThrottlingDelay(false);
            if(tx_throttling_delay > 0)
            {
                MilliSleep(tx_throttling_delay);
            }
        }
    }        
}

json_spirit::Value CRPCTable::execute(const std::string &strMethod, const json_spirit::Array &params, const Value& req_id) const
{
    // Find method
    const CRPCCommand *pcmd = tableRPC[strMethod];
    if (!pcmd)
    {
        if( ((mc_gState->m_SessionFlags & MC_SSF_COLD) == 0) || (mapHelpStrings.count(strMethod) == 0) )
        {
            throw JSONRPCError(RPC_METHOD_NOT_FOUND, "Method not found");
        }
        else
        {
            throw JSONRPCError(RPC_NOT_SUPPORTED, "Method not available in cold version of MultiChain");            
        }
    }
#ifdef ENABLE_WALLET
    if (pcmd->reqWallet && !pwalletMain)                                        // Never happens, reqWallet is changed to require Wallet read lock 
        throw JSONRPCError(RPC_METHOD_NOT_FOUND, "Method not found (disabled)");
#endif

    if( (mc_gState->m_ProtocolVersionToUpgrade > 0) && (mc_gState->IsSupported(mc_gState->m_ProtocolVersionToUpgrade) == 0) )
    {
        if( setAllowedWhenWaitingForUpgrade.count(strMethod) == 0 )
        {
            throw JSONRPCError(RPC_UPGRADE_REQUIRED, strprintf("BlockChain was upgraded to protocol version %d, please upgrade MultiChain",mc_gState->m_ProtocolVersionToUpgrade));
        }
    }
    
    if(GetBoolArg("-offline",false))
    {
        if( setAllowedWhenOffline.count(strMethod) == 0 )
        {
            throw JSONRPCError(RPC_NOT_SUPPORTED, "Method not available with -offline runtime parameter");                
        }        
    }
    
    if(setAllowedWhenLimited.size())
    {
        if( setAllowedWhenLimited.count(strMethod) == 0 )
        {
            throw JSONRPCError(RPC_NOT_ALLOWED, "Method not allowed with current setting of -rpcallowmethod runtime parameter");                
        }        
    }
    
    // Observe safe mode
    string strWarning = GetWarnings("rpc");
    if (strWarning != "" && !GetBoolArg("-disablesafemode", false) &&
        !pcmd->okSafeMode)
        throw JSONRPCError(RPC_FORBIDDEN_BY_SAFE_MODE, string("Safe mode: ") + strWarning);

    try
    {
        // Execute
        if(fDebug)
        {
            string strRequest = JSONRPCRequestForLog(strMethod, params, req_id);
            if(fDebug)LogPrint("mcapi","mcapi: API request: %s, worker: %lu\n",strRequest.c_str(),__US_ThreadID());
            if(fDebug)LogPrint("drsrv01","drsrv01: %d: --> %s\n",GetRPCSlot(),strMethod.c_str());            
        }
        
        Value result;
        {
            if (pcmd->threadSafe)
                result = pcmd->actor(params, false);
#ifdef ENABLE_WALLET
            else if (!pwalletMain) {
                LOCK(cs_main);
                result = pcmd->actor(params, false);
            } else {
                LOCK2(cs_main, pwalletMain->cs_wallet);

                uint32_t wallet_mode=mc_gState->m_WalletMode;
                string strResultNone;
                string strResult;
                if(LogAcceptCategory("walletcompare"))
                {
                    if(wallet_mode & MC_WMD_MAP_TXS)
                    {
                        if(mapWalletReadCommands.count(strMethod))
                        {
                            mc_gState->m_WalletMode=MC_WMD_NONE;
                            result = pcmd->actor(params, false);
                            strResultNone=JSONRPCReply(result, Value::null, 1);
                            mc_gState->m_WalletMode=wallet_mode;
                        }
                    }
                }                

                result = pcmd->actor(params, false);

                if(LogAcceptCategory("walletcompare"))
                {
                    if(wallet_mode & MC_WMD_MAP_TXS)
                    {
                        if(mapWalletReadCommands.count(strMethod))
                        {
                            strResult=JSONRPCReply(result, Value::null, 1);       
                            if(strcmp(strResultNone.c_str(),strResult.c_str()))
                            {
                                string strRequestBad = JSONRPCRequestForLog(strMethod, params, req_id);
                                if(fDebug)LogPrint("walletcompare","walletcompare: ERROR: Result mismatch on API request: %s\n",strRequestBad.c_str());
                                if(fDebug)LogPrint("walletcompare","walletcompare: %s\n",strResultNone.c_str());
                                if(fDebug)LogPrint("walletcompare","walletcompare: %s\n",strResult.c_str());
                            }
                            else
                            {
                                if(fDebug)LogPrint("walletcompare","walletcompare: match: %s \n",strMethod.c_str());                                
                            }
                        }
                    }
                }
                
            }
#else // ENABLE_WALLET
            else {
                LOCK(cs_main);
                result = pcmd->actor(params, false);
            }
#endif // !ENABLE_WALLET
        }
        
        ThrottleTxFlow();
        
        if(fDebug)LogPrint("mcapi","mcapi: API request successful: %s\n",JSONRPCMethodIDForLog(strMethod,req_id).c_str());
        if(fDebug)LogPrint("drsrv01","drsrv01: %d: <-- %s\n",GetRPCSlot(),strMethod.c_str());            
        return result;
    }
    catch (std::exception& e)
    {
        CheckFlagsOnException(strMethod,req_id,e.what());
        if(fDebug)LogPrint("mcapi","mcapi: API request failure: %s\n",JSONRPCMethodIDForLog(strMethod,req_id).c_str());//strMethod.c_str());
        if(strcmp(e.what(),"Help message not found\n") == 0)
        {
            throw JSONRPCError(RPC_MISC_ERROR, mc_RPCHelpString(strMethod).c_str());
        }
        throw JSONRPCError(RPC_MISC_ERROR, e.what());
    }
}

std::string HelpExampleCli(string methodname, string args){
    return "> multichain-cli " + std::string(mc_gState->m_NetworkParams->Name()) + " " + methodname + " " + args + "\n";
}

std::string HelpExampleRpc(string methodname, string args){
    return "> curl --user myusername --data-binary '{\"jsonrpc\": \"1.0\", \"id\":\"curltest\", "
        "\"method\": \"" + methodname + "\", \"params\": [" + args + "] }' -H 'content-type: text/plain;' http://127.0.0.1:"+
            strprintf("%d",(int)mc_gState->m_NetworkParams->GetInt64Param("defaultrpcport")) + "\n";// MCHN was hard-coded 8332 before
}

CRPCTable tableRPC;
std::map<std::string, std::string> mapHelpStrings;
std::map<std::string, int> mapLogParamCounts;
std::set<std::string> setAllowedWhenWaitingForUpgrade;
std::set<std::string> setAllowedWhenOffline;
std::set<std::string> setAllowedWhenLimited;

std::vector<CRPCCommand> vStaticRPCCommands;
std::vector<CRPCCommand> vStaticRPCWalletReadCommands;

/** HTTP module state */

//! libevent event loop
static struct event_base* eventBase = nullptr;
//! HTTP server
static struct evhttp* eventHTTP = nullptr;
//! HTTP Health check server
static struct evhttp* eventHTTPHC = nullptr;
//! List of subnets to allow RPC connections from
static std::vector<CSubNet> rpc_allow_subnets;
//! Work queue for handling longer requests off the event loop thread
static std::unique_ptr<WorkQueue<HTTPClosure>> g_work_queue{nullptr};
//! Work queue for handling health checker requests
static std::unique_ptr<WorkQueue<HTTPClosure>> g_work_queue_hc{nullptr};
//! Handlers for (sub)paths
static std::vector<HTTPPathHandler> pathHandlers;
//! Bound listening sockets
static std::vector<evhttp_bound_socket *> boundSockets;
//! Bound listening sockets
static std::vector<evhttp_bound_socket *> boundSocketsHC;

bool LookupHost(const std::string& name, CNetAddr& addr, bool fAllowLookup)
{
    std::vector<CNetAddr> vIP;
    if (LookupHost(name.c_str(), vIP, 1, fAllowLookup))
    {
        addr = vIP[0];
        return true;
    }
    return false;
}


/** Check if a network address is allowed to access the HTTP server */
static bool ClientAllowed(const CNetAddr& netaddr)
{
    if (!netaddr.IsValid())
        return false;
    for(const CSubNet& subnet : rpc_allow_subnets)
        if (subnet.Match(netaddr))
            return true;
    return false;
}

/** Initialize ACL list for HTTP server */
static bool InitHTTPAllowList()
{
    
    rpc_allow_subnets.clear();
    
    rpc_allow_subnets.clear();
    rpc_allow_subnets.push_back(CSubNet("127.0.0.0/8")); // always allow IPv4 local subnet
    rpc_allow_subnets.push_back(CSubNet("::1")); // always allow IPv6 localhost
    if (mapMultiArgs.count("-rpcallowip"))
    {
        for (const std::string& strAllow : mapMultiArgs["-rpcallowip"]) 
        {
            CSubNet subnet(strAllow);
            if(!subnet.IsValid())
            {
                LogPrintf("ERROR: RPC HTTP Server: Invalid -rpcallowip subnet specification: %s. Valid are a single IP (e.g. 1.2.3.4), a network/netmask (e.g. 1.2.3.4/255.255.255.0) or a network/CIDR (e.g. 1.2.3.4/24).",strAllow.c_str());
                return false;
            }
            rpc_allow_subnets.push_back(subnet);
        }
    }
    
    std::string strAllowed;
    for (const CSubNet& subnet : rpc_allow_subnets)
        strAllowed += subnet.ToString() + " ";
    
    if(fDebug)LogPrint("rpc", "Allowing HTTP connections from: %s\n", strAllowed.c_str());
    return true;
}

/** HTTP request method as string - use for logging only */
std::string RequestMethodString(HTTPRequest::RequestMethod m)
{
    switch (m) {
    case HTTPRequest::GET:
        return "GET";
        break;
    case HTTPRequest::POST:
        return "POST";
        break;
    case HTTPRequest::HEAD:
        return "HEAD";
        break;
    case HTTPRequest::PUT:
        return "PUT";
        break;
    default:
        return "unknown";
    }
}

/** HTTP request callback */
static void http_request_cb(struct evhttp_request* req, void* arg)
{
    // Disable reading to work around a libevent bug, fixed in 2.2.0.
    if (event_get_version_number() >= 0x02010600 && event_get_version_number() < 0x02020001) {
        evhttp_connection* conn = evhttp_request_get_connection(req);
        if (conn) {
            bufferevent* bev = evhttp_connection_get_bufferevent(conn);
            if (bev) {
                bufferevent_disable(bev, EV_READ);
            }
        }
    }
    std::unique_ptr<HTTPRequest> hreq(new HTTPRequest(req));

    // Early address-based allow check
    if (!ClientAllowed(hreq->GetPeer())) {
        LogPrintf( "HTTP request from %s rejected: Client network is not allowed RPC access\n",
                 hreq->GetPeer().ToString());
        
        hreq->WriteReply(HTTP_FORBIDDEN);
        return;
    }

    // Early reject unknown HTTP methods
    if (hreq->GetRequestMethod() == HTTPRequest::UNKNOWN) {
        LogPrintf("HTTP request from %s rejected: Unknown HTTP request method\n",
                 hreq->GetPeer().ToString());
        hreq->WriteReply(HTTP_BAD_METHOD);
 
        return;
    }

    if(fDebug)LogPrint("rpc", "Received a %s request for %s from %s\n",
             RequestMethodString(hreq->GetRequestMethod()), SanitizeString(hreq->GetURI()).substr(0, 100), hreq->GetPeer().ToString().c_str());

    // Find registered handler for prefix
    std::string strURI = hreq->GetURI();
    std::string path;
    std::vector<HTTPPathHandler>::const_iterator i = pathHandlers.begin();
    std::vector<HTTPPathHandler>::const_iterator iend = pathHandlers.end();
    for (; i != iend; ++i) {
        bool match = false;
        if (i->exactMatch)
            match = (strURI == i->prefix);
        else
            match = (strURI.substr(0, i->prefix.size()) == i->prefix);
        if (match) {
            path = strURI.substr(i->prefix.size());
            break;
        }
    }

    // Dispatch to worker thread
    if (i != iend) {
        hreq->SetFlags(MC_ACF_NONE);
        std::unique_ptr<HTTPWorkItem> item(new HTTPWorkItem(std::move(hreq), path, i->handler));
        assert(g_work_queue);
        if (g_work_queue->Enqueue(item.get())) {
            item.release(); /* if true, queue took ownership */
        } else {

            LogPrintf("WARNING: request rejected because http work queue depth exceeded, it can be increased with the -rpcworkqueue= setting\n");
            item->req->WriteReply(HTTP_SERVICE_UNAVAILABLE, "Work queue depth exceeded");
        }
    } else {
        hreq->WriteReply(HTTP_NOT_FOUND);
    }
}



/** HTTP health checker request callback */
static void http_hc_request_cb(struct evhttp_request* req, void* arg)
{
    // Disable reading to work around a libevent bug, fixed in 2.2.0.
    if (event_get_version_number() >= 0x02010600 && event_get_version_number() < 0x02020001) {
        evhttp_connection* conn = evhttp_request_get_connection(req);
        if (conn) {
            bufferevent* bev = evhttp_connection_get_bufferevent(conn);
            if (bev) {
                bufferevent_disable(bev, EV_READ);
            }
        }
    }
    std::unique_ptr<HTTPRequest> hreq(new HTTPRequest(req));

    // Early address-based allow check
    if (!ClientAllowed(hreq->GetPeer())) {
        LogPrintf( "HTTP request from %s rejected: Client network is not allowed RPC access\n",
                 hreq->GetPeer().ToString());
        
        hreq->WriteReply(HTTP_FORBIDDEN);
        return;
    }

    // Early reject unknown HTTP methods
    if (hreq->GetRequestMethod() == HTTPRequest::UNKNOWN) {
        LogPrintf("HTTP request from %s rejected: Unknown HTTP request method\n",
                 hreq->GetPeer().ToString());
        hreq->WriteReply(HTTP_BAD_METHOD);
 
        return;
    }

    if(fDebug)LogPrint("rpc", "Received a health checker %s request for %s from %s\n",
             RequestMethodString(hreq->GetRequestMethod()), SanitizeString(hreq->GetURI()).substr(0, 100), hreq->GetPeer().ToString().c_str());

    // Find registered handler for prefix
    std::string strURI = hreq->GetURI();
    std::string path;
    std::vector<HTTPPathHandler>::const_iterator i = pathHandlers.begin();
    std::vector<HTTPPathHandler>::const_iterator iend = pathHandlers.end();
    for (; i != iend; ++i) {
        bool match = false;
        if (i->exactMatch)
            match = (strURI == i->prefix);
        else
            match = (strURI.substr(0, i->prefix.size()) == i->prefix);
        if (match) {
            path = strURI.substr(i->prefix.size());
            break;
        }
    }

    // Dispatch to worker thread
    if (i != iend) {
        hreq->SetFlags(MC_ACF_ENTERPRISE);        
        std::unique_ptr<HTTPWorkItem> item(new HTTPWorkItem(std::move(hreq), path, i->handler));
        assert(g_work_queue_hc);
        if (g_work_queue_hc->Enqueue(item.get())) {
            item.release(); /* if true, queue took ownership */
        } else {

            LogPrintf("WARNING: health checker request rejected because http work queue depth exceeded\n");
            item->req->WriteHeader("Connection", "close");
            item->req->WriteReply(HTTP_SERVICE_UNAVAILABLE, "Health checker work queue depth exceeded");
        }
        
//        (*item)();
    } else {
        hreq->WriteReply(HTTP_NOT_FOUND);
    }
}

/** Callback to reject HTTP requests after shutdown. */
static void http_reject_request_cb(struct evhttp_request* req, void*)
{
    if(fDebug)LogPrint("rpc", "Rejecting request while shutting down\n");
    evhttp_send_error(req, HTTP_SERVUNAVAIL, NULL);
}

/** Event dispatcher thread */
static bool ThreadHTTP(struct event_base* base)
{
/* TODO 
    util::ThreadRename("http");
 */ 
    
/* TODO 
    SetSyscallSandboxPolicy(SyscallSandboxPolicy::NET_HTTP_SERVER);
 */ 
    if(fDebug)LogPrint("rpc", "Entering http event loop\n");
    event_base_dispatch(base);
    // Event loop will be interrupted by InterruptHTTPServer()
    if(fDebug)LogPrint("rpc", "Exited http event loop\n");
    return event_base_got_break(base) == 0;
}

/** Bind HTTP server to specified addresses */
static bool HTTPBindAddresses(struct evhttp* http,struct evhttp* http_hc)
{
    uint16_t http_port{static_cast<uint16_t>(GetArg("-rpcport", BaseParams().RPCPort()))};
    std::vector<std::pair<std::string, uint16_t>> endpoints;

    int ihcPort=pEF->HCH_GetPort();
    
    if((ihcPort < 0) || (ihcPort > 65535))
    {
        uiInterface.ThreadSafeMessageBox("Invalid health checker port", "", CClientUIInterface::MSG_ERROR);
        return false;        
    }
    
    uint16_t hcPort = (uint16_t)ihcPort;
    if(hcPort == http_port)
    {
        uiInterface.ThreadSafeMessageBox("Health checker and RPC ports should be different", "", CClientUIInterface::MSG_ERROR);
        return false;
    }
    
    // Determine what addresses to bind to
    if (mapMultiArgs.count("-rpcallowip") == 0) { // Default to loopback if not allowing external IPs
//    if (!((mapMultiArgs.count("-rpcallowip") > 0) && (mapMultiArgs.count("-rpcbind") > 0))) { // Default to loopback if not allowing external IPs
        endpoints.push_back(std::make_pair("::1", http_port));
        endpoints.push_back(std::make_pair("127.0.0.1", http_port));
        if(hcPort)
        {
            endpoints.push_back(std::make_pair("::1", hcPort));
            endpoints.push_back(std::make_pair("127.0.0.1", hcPort));
        }
/*        
        if (mapMultiArgs.count("-rpcallowip") > 0) {
            LogPrintf("WARNING: option -rpcallowip was specified without -rpcbind; this doesn't usually make sense\n");
        }
*/
        if (mapMultiArgs.count("-rpcbind") > 0) {
            LogPrintf("WARNING: option -rpcbind was ignored because -rpcallowip was not specified, refusing to allow everyone to connect\n");
        }
    } else if (mapMultiArgs.count("-rpcbind") > 0) { // Specific bind address
        for (const std::string& strRPCBind : mapMultiArgs["-rpcbind"]) {
            int iport{http_port};
            std::string host;
            SplitHostPort(strRPCBind, iport, host);
            uint16_t port=(uint16_t)iport;
            endpoints.push_back(std::make_pair(host, port));
            if(hcPort)
            {
                endpoints.push_back(std::make_pair(host, hcPort));
            }
        }
    } else { // No specific bind address specified, bind to any
        endpoints.push_back(std::make_pair("::", http_port));
        endpoints.push_back(std::make_pair("0.0.0.0", http_port));
    }
    
    bool hcOK=hcPort ? false : true;

    // Bind addresses
    for (std::vector<std::pair<std::string, uint16_t> >::iterator i = endpoints.begin(); i != endpoints.end(); ++i) {
        if(i->second == hcPort)
        {
            if(fDebug)LogPrint("rpc", "Binding health checker on address %s port %i\n", i->first, i->second);
            evhttp_bound_socket *bind_handle = evhttp_bind_socket_with_handle(http_hc, i->first.empty() ? NULL : i->first.c_str(), i->second);
            if (bind_handle) {
                hcOK=true;
                boundSocketsHC.push_back(bind_handle);
            } else {
                LogPrintf("Binding health checker on address %s port %i failed.\n", i->first, i->second);
            }
        }
        else    
        {
            if(fDebug)LogPrint("rpc", "Binding RPC on address %s port %i\n", i->first, i->second);
            evhttp_bound_socket *bind_handle = evhttp_bind_socket_with_handle(http, i->first.empty() ? NULL : i->first.c_str(), i->second);
            if (bind_handle) {
/*               
                CNetAddr addr;
                if (i->first.empty() || (LookupHost(i->first, addr, false))) {
                    LogPrintf("WARNING: the RPC server is not safe to expose to untrusted networks such as the public internet\n");
                }
 */ 
                boundSockets.push_back(bind_handle);
            } else {
                LogPrintf("Binding RPC on address %s port %i failed.\n", i->first, i->second);
            }
        }
    }
    
    if(!hcOK)
    {
        return false;        
    }
    
    return !boundSockets.empty();
}

/** Simple wrapper to set thread name and run work queue */
static void HTTPWorkQueueRun(WorkQueue<HTTPClosure>* queue, int worker_num)
{
/* TODO 
    util::ThreadRename(strprintf("httpworker.%i", worker_num));
 */ 
/* TODO
    SetSyscallSandboxPolicy(SyscallSandboxPolicy::NET_HTTP_SERVER_WORKER);
 */ 
    uint64_t thread_id=__US_ThreadID();
    
    if(worker_num >= 0)
    {
        if(fDebug)LogPrint("mcapi", "Starting RPC worker thread %d, id: %lu\n",worker_num,thread_id);
        RPCThreadLoad load;
        load.Zero();
        rpc_loads.insert(make_pair(thread_id,load));
        rpc_slots.insert(make_pair(thread_id,worker_num));        
    }
    queue->Run();
}

/** libevent event log callback */
static void libevent_log_cb(int severity, const char *msg)
{
    if (severity >= EVENT_LOG_WARN) // Log warn messages and higher without debug category
        LogPrintf("libevent: %s\n", msg);
    else
    {
        if(fDebug)LogPrint("libevent", "libevent: %s\n", msg);
    }
}

bool InitHTTPServer()
{
    if (!InitHTTPAllowList())
        return false;

    // Redirect libevent's logging to our own log
    event_set_log_callback(&libevent_log_cb);
    // Update libevent's log handling. Returns false if our version of
    // libevent doesn't support debug logging, in which case we should
    // clear the BCLog::LIBEVENT flag.
/* TODO
    if (!UpdateHTTPServerLogging(LogInstance().WillLogCategory(BCLog::LIBEVENT))) {
        LogInstance().DisableCategory(BCLog::LIBEVENT);
    }
 */ 

#ifdef WIN32
    evthread_use_windows_threads();
#else
    evthread_use_pthreads();
#endif

    raii_event_base base_ctr = obtain_event_base();

    /* Create a new evhttp object to handle requests. */
    raii_evhttp http_ctr = obtain_evhttp(base_ctr.get());
    struct evhttp* http = http_ctr.get();
    if (!http) {
        LogPrintf("couldn't create evhttp. Exiting.\n");
        return false;
    }

//    raii_event_base base_hc_ctr = obtain_event_base();
    
    raii_evhttp http_hc_ctr = obtain_evhttp(base_ctr.get());
    struct evhttp* http_hc = http_hc_ctr.get();
    if (!http_hc) {
        LogPrintf("couldn't create evhttp. Exiting.\n");
        return false;
    }
    
    evhttp_set_timeout(http, GetArg("-rpcservertimeout", DEFAULT_HTTP_SERVER_TIMEOUT));
    evhttp_set_max_headers_size(http, MAX_HEADERS_SIZE);
    evhttp_set_max_body_size(http, MAX_SIZE);
    evhttp_set_gencb(http, http_request_cb, NULL);

    evhttp_set_timeout(http_hc, DEFAULT_HTTP_SERVER_TIMEOUT);
    evhttp_set_max_headers_size(http_hc, MAX_HEADERS_SIZE);
    evhttp_set_max_body_size(http_hc, MAX_SIZE);
    evhttp_set_gencb(http_hc, http_hc_request_cb, NULL);
    
    if (!HTTPBindAddresses(http,http_hc)) {
        LogPrintf("Unable to bind any endpoint for RPC server\n");
        return false;
    }


    if(fDebug)LogPrint("rpc", "Initialized RPC HTTP server\n");
    int workQueueDepth = std::max((long)GetArg("-rpcworkqueue", DEFAULT_HTTP_WORKQUEUE), 1L);
    LogPrintf("HTTP: creating work queue of depth %d\n", workQueueDepth);

    g_work_queue = std::make_unique<WorkQueue<HTTPClosure>>(workQueueDepth);
    g_work_queue_hc = std::make_unique<WorkQueue<HTTPClosure>>(DEFAULT_HTTP_WORKQUEUE);
    
    // transfer ownership to eventBase/HTTP via .release()
    eventBase = base_ctr.release();
//    eventBaseHC = base_hc_ctr.release();
    eventHTTP = http_ctr.release();
    eventHTTPHC = http_hc_ctr.release();
    return true;
}

bool UpdateHTTPServerLogging(bool enable) {
#if LIBEVENT_VERSION_NUMBER >= 0x02010100
    if (enable) {
        event_enable_debug_logging(EVENT_DBG_ALL);
    } else {
        event_enable_debug_logging(EVENT_DBG_NONE);
    }
    return true;
#else
    // Can't update libevent logging if version < 02010100
    return false;
#endif
}

static std::thread g_thread_http;
static std::vector<std::thread> g_thread_http_workers;


struct event_base* EventBase()
{
    return eventBase;
}

static void httpevent_callback_fn(evutil_socket_t, short, void* data)
{
    // Static handler: simply call inner handler
    HTTPEvent *self = static_cast<HTTPEvent*>(data);
    self->handler();
    if (self->deleteWhenTriggered)
        delete self;
}

HTTPEvent::HTTPEvent(struct event_base* base, bool _deleteWhenTriggered, const std::function<void()>& _handler):
    deleteWhenTriggered(_deleteWhenTriggered), handler(_handler)
{
    ev = event_new(base, -1, 0, httpevent_callback_fn, this);
    assert(ev);
}
HTTPEvent::~HTTPEvent()
{
    event_free(ev);
}
void HTTPEvent::trigger(struct timeval* tv)
{
    if (tv == NULL)
        event_active(ev, 0, 0); // immediately trigger event in main thread
    else
        evtimer_add(ev, tv); // trigger after timeval passed
}
HTTPRequest::HTTPRequest(struct evhttp_request* _req, bool _replySent) : req(_req), replySent(_replySent)
{
}

HTTPRequest::~HTTPRequest()
{
    if (!replySent) {
        // Keep track of whether reply was sent to avoid request leaks
        LogPrintf("%s: Unhandled request\n", __func__);
        WriteReply(HTTP_INTERNAL_SERVER_ERROR, "Unhandled request");

    }
    // evhttpd cleans up the request, as long as a reply was sent.
}

std::pair<bool, std::string> HTTPRequest::GetHeader(const std::string& hdr) const
{
    const struct evkeyvalq* headers = evhttp_request_get_input_headers(req);
    assert(headers);
    const char* val = evhttp_find_header(headers, hdr.c_str());
    if (val)
        return std::make_pair(true, val);
    else
        return std::make_pair(false, "");
}

std::string HTTPRequest::ReadBody()
{
    struct evbuffer* buf = evhttp_request_get_input_buffer(req);
    if (!buf)
        return "";
    size_t size = evbuffer_get_length(buf);
    /** Trivial implementation: if this is ever a performance bottleneck,
     * internal copying can be avoided in multi-segment buffers by using
     * evbuffer_peek and an awkward loop. Though in that case, it'd be even
     * better to not copy into an intermediate string but use a stream
     * abstraction to consume the evbuffer on the fly in the parsing algorithm.
     */
    const char* data = (const char*)evbuffer_pullup(buf, size);
    if (!data) // returns NULL in case of empty buffer
        return "";
    std::string rv(data, size);
    evbuffer_drain(buf, size);
    return rv;
}

void HTTPRequest::WriteHeader(const std::string& hdr, const std::string& value)
{
    struct evkeyvalq* headers = evhttp_request_get_output_headers(req);
    assert(headers);
    evhttp_add_header(headers, hdr.c_str(), value.c_str());
}

/** Closure sent to main thread to request a reply to be sent to
 * a HTTP request.
 * Replies must be sent in the main loop in the main http thread,
 * this cannot be done from worker threads.
 */
void HTTPRequest::WriteReply(int nStatus, const std::string& strReply)
{
    assert(!replySent && req);
    if (ShutdownRequested()) {
        WriteHeader("Connection", "close");
    }
    // Send event to main http thread to send reply message
    struct evbuffer* evb = evhttp_request_get_output_buffer(req);
    assert(evb);
    evbuffer_add(evb, strReply.data(), strReply.size());
    auto req_copy = req;
    HTTPEvent* ev = new HTTPEvent(eventBase, true, [req_copy, nStatus]{
        evhttp_send_reply(req_copy, nStatus, NULL, NULL);
        // Re-enable reading from the socket. This is the second part of the libevent
        // workaround above.
        if (event_get_version_number() >= 0x02010600 && event_get_version_number() < 0x02020001) {
            evhttp_connection* conn = evhttp_request_get_connection(req_copy);
            if (conn) {
                bufferevent* bev = evhttp_connection_get_bufferevent(conn);
                if (bev) {
                    bufferevent_enable(bev, EV_READ | EV_WRITE);
                }
            }
        }
    });
    ev->trigger(NULL);
    replySent = true;
    req = NULL; // transferred back to main thread
}

CService HTTPRequest::GetPeer() const
{
    evhttp_connection* con = evhttp_request_get_connection(req);
    CService peer;
    if (con) {
        // evhttp retains ownership over returned address string
        const char* address = "";
        uint16_t port = 0;
        evhttp_connection_get_peer(con, (char**)&address, &port);
        LookupNumeric(address, peer, port);        
    }
    return peer;
}

void HTTPRequest::SetFlags(uint32_t flags_in)
{
    flags=flags_in;
}

uint32_t HTTPRequest::GetFlags()
{
    return flags;
}

std::string HTTPRequest::GetURI() const
{
    return evhttp_request_get_uri(req);
}

HTTPRequest::RequestMethod HTTPRequest::GetRequestMethod() const
{
    switch (evhttp_request_get_command(req)) {
    case EVHTTP_REQ_GET:
        return GET;
        break;
    case EVHTTP_REQ_POST:
        return POST;
        break;
    case EVHTTP_REQ_HEAD:
        return HEAD;
        break;
    case EVHTTP_REQ_PUT:
        return PUT;
        break;
    default:
        return UNKNOWN;
        break;
    }
}

void RegisterHTTPHandler(const std::string &prefix, bool exactMatch, const HTTPRequestHandler &handler)
{
/* TODO    
    if(fDebug)LogPrint(BCLog::HTTP, "Registering HTTP handler for %s (exactmatch %d)\n", prefix, exactMatch);
 */ 
    pathHandlers.push_back(HTTPPathHandler(prefix, exactMatch, handler));
}

void UnregisterHTTPHandler(const std::string &prefix, bool exactMatch)
{
    std::vector<HTTPPathHandler>::iterator i = pathHandlers.begin();
    std::vector<HTTPPathHandler>::iterator iend = pathHandlers.end();
    for (; i != iend; ++i)
        if (i->prefix == prefix && i->exactMatch == exactMatch)
            break;
    if (i != iend)
    {
/* TODO    
        if(fDebug)LogPrint(BCLog::HTTP, "Unregistering HTTP handler for %s (exactmatch %d)\n", prefix, exactMatch);
 */ 
        pathHandlers.erase(i);
    }
}


static void JSONErrorReply(HTTPRequest* req, const Value& objError, const Value& id)
{
    // Send error reply from json-rpc error object
    int nStatus = HTTP_INTERNAL_SERVER_ERROR;
    int code = find_value(objError.get_obj(), "code").get_int();

    if (code == RPC_INVALID_REQUEST)
        nStatus = HTTP_BAD_REQUEST;
    else if (code == RPC_METHOD_NOT_FOUND)
        nStatus = HTTP_NOT_FOUND;

    std::string strReply = JSONRPCReply(Value::null, objError, id);

    req->WriteHeader("Content-Type", "application/json");
    req->WriteReply(nStatus, strReply);
}

std::string TrimString(const std::string& str, const std::string& pattern = " \f\n\r\t\v")
{
    std::string::size_type front = str.find_first_not_of(pattern);
    if (front == std::string::npos) {
        return std::string();
    }
    std::string::size_type end = str.find_last_not_of(pattern);
    return str.substr(front, end - front + 1);
}

static bool RPCAuthorized(const std::string& strAuth, std::string& strAuthUsernameOut)
{
    if (strRPCUserColonPass.empty()) // Belt-and-suspenders measure if InitRPCAuthentication was not called
        return false;
    if (strAuth.substr(0, 6) != "Basic ")
        return false;
    std::string strUserPass64 = TrimString(strAuth.substr(6));
    std::string strUserPass = DecodeBase64(strUserPass64);

    if (strUserPass.find(':') != std::string::npos)
        strAuthUsernameOut = strUserPass.substr(0, strUserPass.find(':'));

    //Check if authorized under single-user field
    if (TimingResistantEqual(strUserPass, strRPCUserColonPass)) {
        return true;
    }
    
    return false;
}

static bool HTTPReq_JSONRPC(HTTPRequest* req)
{
    // JSONRPC handles only POST
    if((req->GetFlags() & MC_ACF_ENTERPRISE) == 0)
    {
        if (req->GetRequestMethod() != HTTPRequest::POST) 
        {
            req->WriteReply(HTTP_BAD_METHOD, "JSONRPC server handles only POST requests");
            return false;
        }
    }
    // Check authorization
    std::pair<bool, std::string> authHeader = req->GetHeader("authorization");
    if (!authHeader.first) {
        req->WriteHeader("WWW-Authenticate", WWW_AUTH_HEADER_DATA);
        req->WriteReply(HTTP_UNAUTHORIZED);
        return false;
    }

    std::string strAuthUsernameOut;
    if (!RPCAuthorized(authHeader.second, strAuthUsernameOut)) {
        LogPrintf("ThreadRPCServer incorrect password attempt from %s\n", req->GetPeer().ToStringIPPort().c_str());

        // Deter brute-forcing           If this results in a DoS the user really           shouldn't have their RPC port exposed. 
        MilliSleep(250);

        req->WriteHeader("WWW-Authenticate", WWW_AUTH_HEADER_DATA);
        req->WriteReply(HTTP_UNAUTHORIZED);
        return false;
    }

    std::string strRequest;
    std::string strReply;
    std::string strHeaderOut;
    std::map<std::string, std::string> mapHeadersOut;
    Value valError;
    Value req_id;
    int http_code=HTTP_OK;
    strRequest=req->ReadBody();
    bool result=HTTPReq_JSONRPC(strRequest,req->GetFlags(),strReply,strHeaderOut,mapHeadersOut,http_code,valError,req_id);
    if(result)
    {
        for (std::map<std::string, std::string>::const_iterator it = mapHeadersOut.begin(); it != mapHeadersOut.end(); ++it)
        {
            req->WriteHeader(it->first, it->second);            
        }
        req->WriteReply(http_code, strReply);        
        if(http_code != HTTP_OK)
        {
            result=false;
        }
    }
    else
    {
        JSONErrorReply(req, valError, req_id);        
    }
    return result;
}

void StartHTTPServer()
{    
    mc_InitRPCList(vStaticRPCCommands,vStaticRPCWalletReadCommands);
    mc_InitRPCListIfLimited();
    tableRPC.initialize();
    
    strRPCUserColonPass = mapArgs["-rpcuser"] + ":" + mapArgs["-rpcpassword"];

    auto handle_rpc = [](HTTPRequest* req, const std::string&) { return HTTPReq_JSONRPC( req); };
    RegisterHTTPHandler("/", true, handle_rpc);
    
    struct event_base* eventBase = EventBase();
    assert(eventBase);
    
    if(fDebug)LogPrint("rpc", "Starting RPC HTTP server\n");
    int rpcThreads = std::max((long)GetArg("-rpcthreads", DEFAULT_HTTP_THREADS), 1L);
    LogPrintf("HTTP: starting %d worker threads\n", rpcThreads);
    g_thread_http = std::thread(ThreadHTTP, eventBase);

    for (int i = 0; i < rpcThreads; i++) {
        g_thread_http_workers.emplace_back(HTTPWorkQueueRun, g_work_queue.get(), i);
        MilliSleep(10);
        while((int)rpc_slots.size() < i+1)
        {
            MilliSleep(10);
        }
    }
    g_thread_http_workers.emplace_back(HTTPWorkQueueRun, g_work_queue_hc.get(), -1);
    
    mc_gState->InitRPCThreads(rpcThreads);
    fRPCRunning = true;
}

void InterruptHTTPServer()
{
    if(fDebug)LogPrint("rpc", "Interrupting HTTP server\n");
    if (eventHTTP) {
        // Reject requests on current connections
        evhttp_set_gencb(eventHTTP, http_reject_request_cb, NULL);
    }
    if (eventHTTPHC) {
        // Reject requests on current connections
        evhttp_set_gencb(eventHTTPHC, http_reject_request_cb, NULL);
    }
    fRPCInterrupted=true;
    if (g_work_queue) {
        g_work_queue->Interrupt();
    }
    if (g_work_queue_hc) {
        g_work_queue_hc->Interrupt();
    }
}

void StopHTTPServer()
{
    UnregisterHTTPHandler("/", true);
    if(fDebug)LogPrint("rpc", "Stopping RPC HTTP server\n");
    if (g_work_queue) {

        if(fDebug)LogPrint("rpc", "Waiting for RPC HTTP worker threads to exit\n");
        for (auto& thread : g_thread_http_workers) {
            thread.join();
        }
        g_thread_http_workers.clear();
    }
    // Unlisten sockets, these are what make the event loop running, which means
    // that after this and all connections are closed the event loop will quit.
    for (evhttp_bound_socket *socket : boundSocketsHC) {
        evhttp_del_accept_socket(eventHTTPHC, socket);
    }
    boundSocketsHC.clear();
    for (evhttp_bound_socket *socket : boundSockets) {
        evhttp_del_accept_socket(eventHTTP, socket);
    }
    boundSockets.clear();
    if (eventBase) {
        if(fDebug)LogPrint("rpc", "Waiting for RPC HTTP event thread to exit\n");
        if (g_thread_http.joinable()) g_thread_http.join();
    }
    if (eventHTTPHC) {
        evhttp_free(eventHTTPHC);
        eventHTTPHC = NULL;
    }
    if (eventHTTP) {
        evhttp_free(eventHTTP);
        eventHTTP = NULL;
    }
    if (eventBase) {
        event_base_free(eventBase);
        eventBase = NULL;
    }
    g_work_queue.reset();
    g_work_queue_hc.reset();
    if(fDebug)LogPrint("rpc", "Stopped RPC HTTP server\n");
}


/*
bool StartHTTPRPC()
{
    mc_InitRPCList(vStaticRPCCommands,vStaticRPCWalletReadCommands);
    mc_InitRPCListIfLimited();
    tableRPC.initialize();
    fRPCRunning = true;
    
    strRPCUserColonPass = mapArgs["-rpcuser"] + ":" + mapArgs["-rpcpassword"];

    auto handle_rpc = [](HTTPRequest* req, const std::string&) { return HTTPReq_JSONRPC( req); };
    RegisterHTTPHandler("/", true, handle_rpc);
    
    struct event_base* eventBase = EventBase();
    assert(eventBase);
    return true;
}
void InterruptHTTPRPC()
{
    if(fDebug)LogPrint(BCLog::RPC, "Interrupting HTTP RPC server\n");
}

void StopHTTPRPC()
{
    if(fDebug)LogPrint(BCLog::RPC, "Stopping HTTP RPC server\n");
    UnregisterHTTPHandler("/", true);
    if (g_wallet_init_interface.HasWalletSupport()) {
        UnregisterHTTPHandler("/wallet/", false);
    }
    
    if (httpRPCTimerInterface) {
        RPCUnsetTimerInterface(httpRPCTimerInterface.get());
        httpRPCTimerInterface.reset();
    }
}
*/