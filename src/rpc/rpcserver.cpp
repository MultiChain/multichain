// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2014-2016 The Bitcoin Core developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2017 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "rpc/rpcserver.h"

#include "structs/base58.h"
#include "core/init.h"
#include "core/main.h"
#include "ui/ui_interface.h"
#include "utils/util.h"
#ifdef ENABLE_WALLET
#include "wallet/wallet.h"
#endif

#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/bind.hpp>
#include <boost/filesystem.hpp>
#include <boost/foreach.hpp>
#include <boost/iostreams/concepts.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include "json/json_spirit_writer_template.h"

using namespace boost;
using namespace boost::asio;
using namespace json_spirit;
using namespace std;

static std::string strRPCUserColonPass;

static bool fRPCRunning = false;
static bool fRPCInWarmup = true;
static std::string rpcWarmupStatus("RPC server started");
static CCriticalSection cs_rpcWarmup;

//! These are created by StartRPCThreads, destroyed in StopRPCThreads
static asio::io_service* rpc_io_service = NULL;
static map<string, boost::shared_ptr<deadline_timer> > deadlineTimers;
static ssl::context* rpc_ssl_context = NULL;
static boost::thread_group* rpc_worker_group = NULL;
static boost::asio::io_service::work *rpc_dummy_work = NULL;
static std::vector<CSubNet> rpc_allow_subnets; //!< List of subnets to allow RPC connections from
static std::vector< boost::shared_ptr<ip::tcp::acceptor> > rpc_acceptors;

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
/*    
    request.push_back(Pair("chain_name", string(mc_gState->m_Params->NetworkName())));
 */ 
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
/* MCHN START */    
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
    
//    if (dAmount < 0.0 || dAmount > 21000000.0)                                  // MCHN - was <=
//        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid amount");
/* MCHN END */    
    CAmount nAmount = roundint64(dAmount * COIN);
    if (!MoneyRange(nAmount))
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid amount");
    return nAmount;
}

Value ValueFromAmount(const CAmount& amount)
{
/*  MCHN START */   
    if(COIN == 0)
    {
        return (double)amount;
    }
/*  MCHN END */   
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
        if (pcmd->reqWallet && !pwalletMain)
            continue;
#endif

/* MCHN START */        
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
                strHelp = string(e.what());
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
/*        
        try
        {
            Array params;
            rpcfn_type pfn = pcmd->actor;
            if (setDone.insert(pfn).second)
                (*pfn)(params, true);
        }
        catch (std::exception& e)
        {
            // Help text is returned in an exception
            string strHelp = string(e.what());
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
 */ 
    }
/* MCHN END */        
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

/* MCHN START */

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

/* END */

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


bool HTTPAuthorized(map<string, string>& mapHeaders)
{
    string strAuth = mapHeaders["authorization"];
    if (strAuth.substr(0,6) != "Basic ")
        return false;
    string strUserPass64 = strAuth.substr(6); boost::trim(strUserPass64);
    string strUserPass = DecodeBase64(strUserPass64);
    return TimingResistantEqual(strUserPass, strRPCUserColonPass);
}

void ErrorReply(std::ostream& stream, const Object& objError, const Value& id)
{
    // Send error reply from json-rpc error object
    int nStatus = HTTP_INTERNAL_SERVER_ERROR;
    int code = find_value(objError, "code").get_int();
    if (code == RPC_INVALID_REQUEST) nStatus = HTTP_BAD_REQUEST;
    else if (code == RPC_METHOD_NOT_FOUND) nStatus = HTTP_NOT_FOUND;
    string strReply = JSONRPCReply(Value::null, objError, id);
    stream << HTTPReply(nStatus, strReply, false) << std::flush;
}

CNetAddr BoostAsioToCNetAddr(boost::asio::ip::address address)
{
    CNetAddr netaddr;
    // Make sure that IPv4-compatible and IPv4-mapped IPv6 addresses are treated as IPv4 addresses
    if (address.is_v6()
     && (address.to_v6().is_v4_compatible()
      || address.to_v6().is_v4_mapped()))
        address = address.to_v6().to_v4();

    if(address.is_v4())
    {
        boost::asio::ip::address_v4::bytes_type bytes = address.to_v4().to_bytes();
        netaddr.SetRaw(NET_IPV4, &bytes[0]);
    }
    else
    {
        boost::asio::ip::address_v6::bytes_type bytes = address.to_v6().to_bytes();
        netaddr.SetRaw(NET_IPV6, &bytes[0]);
    }
    return netaddr;
}

bool ClientAllowed(const boost::asio::ip::address& address)
{
    CNetAddr netaddr = BoostAsioToCNetAddr(address);
    BOOST_FOREACH(const CSubNet &subnet, rpc_allow_subnets)
        if (subnet.Match(netaddr))
            return true;
    return false;
}

template <typename Protocol>
class AcceptedConnectionImpl : public AcceptedConnection
{
public:
    AcceptedConnectionImpl(
            asio::io_service& io_service,
            ssl::context &context,
            bool fUseSSL) :
        sslStream(io_service, context),
        _d(sslStream, fUseSSL),
        _stream(_d)
    {
    }

    virtual std::iostream& stream()
    {
        return _stream;
    }

    virtual std::string peer_address_to_string() const
    {
        return peer.address().to_string();
    }

    virtual void close()
    {
        _stream.close();
    }

    typename Protocol::endpoint peer;
    asio::ssl::stream<typename Protocol::socket> sslStream;

private:
    SSLIOStreamDevice<Protocol> _d;
    iostreams::stream< SSLIOStreamDevice<Protocol> > _stream;
};

void ServiceConnection(AcceptedConnection *conn);

//! Forward declaration required for RPCListen
template <typename Protocol, typename SocketAcceptorService>
static void RPCAcceptHandler(boost::shared_ptr< basic_socket_acceptor<Protocol, SocketAcceptorService> > acceptor,
                             ssl::context& context,
                             bool fUseSSL,
                             boost::shared_ptr< AcceptedConnection > conn,
                             const boost::system::error_code& error);

/**
 * Sets up I/O resources to accept and handle a new connection.
 */
template <typename Protocol, typename SocketAcceptorService>
static void RPCListen(boost::shared_ptr< basic_socket_acceptor<Protocol, SocketAcceptorService> > acceptor,
                   ssl::context& context,
                   const bool fUseSSL)
{
    // Accept connection
    boost::shared_ptr< AcceptedConnectionImpl<Protocol> > conn(new AcceptedConnectionImpl<Protocol>(acceptor->get_io_service(), context, fUseSSL));

    acceptor->async_accept(
            conn->sslStream.lowest_layer(),
            conn->peer,
            boost::bind(&RPCAcceptHandler<Protocol, SocketAcceptorService>,
                acceptor,
                boost::ref(context),
                fUseSSL,
                conn,
                _1));
}


/**
 * Accept and handle incoming connection.
 */
template <typename Protocol, typename SocketAcceptorService>
static void RPCAcceptHandler(boost::shared_ptr< basic_socket_acceptor<Protocol, SocketAcceptorService> > acceptor,
                             ssl::context& context,
                             const bool fUseSSL,
                             boost::shared_ptr< AcceptedConnection > conn,
                             const boost::system::error_code& error)
{
    // Immediately start accepting new connections, except when we're cancelled or our socket is closed.
    if (error != asio::error::operation_aborted && acceptor->is_open())
        RPCListen(acceptor, context, fUseSSL);

    AcceptedConnectionImpl<ip::tcp>* tcp_conn = dynamic_cast< AcceptedConnectionImpl<ip::tcp>* >(conn.get());

    if (error)
    {
        // TODO: Actually handle errors
        LogPrintf("%s: Error: %s\n", __func__, error.message());
    }
    // Restrict callers by IP.  It is important to
    // do this before starting client thread, to filter out
    // certain DoS and misbehaving clients.
    else if (tcp_conn && !ClientAllowed(tcp_conn->peer.address()))
    {
        // Only send a 403 if we're not using SSL to prevent a DoS during the SSL handshake.
        if (!fUseSSL)
            conn->stream() << HTTPError(HTTP_FORBIDDEN, false) << std::flush;
        conn->close();
    }
    else {
        ServiceConnection(conn.get());
        conn->close();
    }
}

static ip::tcp::endpoint ParseEndpoint(const std::string &strEndpoint, int defaultPort)
{
    std::string addr;
    int port = defaultPort;
    SplitHostPort(strEndpoint, port, addr);
    return ip::tcp::endpoint(asio::ip::address::from_string(addr), port);
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

void StartRPCThreads()
{
    mc_InitRPCList(vStaticRPCCommands,vStaticRPCWalletReadCommands);
    mc_InitRPCListIfLimited();
    tableRPC.initialize();

    rpc_allow_subnets.clear();
    rpc_allow_subnets.push_back(CSubNet("127.0.0.0/8")); // always allow IPv4 local subnet
    rpc_allow_subnets.push_back(CSubNet("::1")); // always allow IPv6 localhost
    if (mapMultiArgs.count("-rpcallowip"))
    {
        const vector<string>& vAllow = mapMultiArgs["-rpcallowip"];
        BOOST_FOREACH(string strAllow, vAllow)
        {
            CSubNet subnet(strAllow);
            if(!subnet.IsValid())
            {
                uiInterface.ThreadSafeMessageBox(
                    strprintf("Invalid -rpcallowip subnet specification: %s. Valid are a single IP (e.g. 1.2.3.4), a network/netmask (e.g. 1.2.3.4/255.255.255.0) or a network/CIDR (e.g. 1.2.3.4/24).", strAllow),
                    "", CClientUIInterface::MSG_ERROR);
                StartShutdown();
                return;
            }
            rpc_allow_subnets.push_back(subnet);
        }
    }
    std::string strAllowed;
    BOOST_FOREACH(const CSubNet &subnet, rpc_allow_subnets)
        strAllowed += subnet.ToString() + " ";
    if(fDebug)LogPrint("rpc", "Allowing RPC connections from: %s\n", strAllowed);

    strRPCUserColonPass = mapArgs["-rpcuser"] + ":" + mapArgs["-rpcpassword"];
    if (((mapArgs["-rpcpassword"] == "") ||
         (mapArgs["-rpcuser"] == mapArgs["-rpcpassword"])) && Params().RequireRPCPassword())
    {
        unsigned char rand_pwd[32];
        GetRandBytes(rand_pwd, 32);
        uiInterface.ThreadSafeMessageBox(strprintf(
            _("To use multichaind, you must set an rpcpassword in the configuration file:\n"
              "%s\n"
              "It is recommended you use the following random password:\n"
              "rpcuser=multichainrpc\n"
              "rpcpassword=%s\n"
              "(you do not need to remember this password)\n"
              "The username and password MUST NOT be the same.\n"
              "If the file does not exist, create it with owner-readable-only file permissions.\n"
              "It is also recommended to set alertnotify so you are notified of problems;\n"
              "for example: alertnotify=echo %%s | mail -s \"MultiChain Alert\" admin@foo.com\n"),
                GetConfigFile().string(),
                EncodeBase58(&rand_pwd[0],&rand_pwd[0]+32)),
                "", CClientUIInterface::MSG_ERROR | CClientUIInterface::SECURE);
        StartShutdown();
        return;
    }

    assert(rpc_io_service == NULL);
    rpc_io_service = new asio::io_service();
    rpc_ssl_context = new ssl::context(*rpc_io_service, ssl::context::sslv23);

    const bool fUseSSL = GetBoolArg("-rpcssl", false);

    if (fUseSSL)
    {
        rpc_ssl_context->set_options(ssl::context::no_sslv2 | ssl::context::no_sslv3);

        filesystem::path pathCertFile(GetArg("-rpcsslcertificatechainfile", "server.cert"));
        if (!pathCertFile.is_complete()) pathCertFile = filesystem::path(GetDataDir()) / pathCertFile;
        if (filesystem::exists(pathCertFile)) rpc_ssl_context->use_certificate_chain_file(pathCertFile.string());
        else LogPrintf("ThreadRPCServer ERROR: missing server certificate file %s\n", pathCertFile.string());

        filesystem::path pathPKFile(GetArg("-rpcsslprivatekeyfile", "server.pem"));
        if (!pathPKFile.is_complete()) pathPKFile = filesystem::path(GetDataDir()) / pathPKFile;
        if (filesystem::exists(pathPKFile)) rpc_ssl_context->use_private_key_file(pathPKFile.string(), ssl::context::pem);
        else LogPrintf("ThreadRPCServer ERROR: missing server private key file %s\n", pathPKFile.string());

        string strCiphers = GetArg("-rpcsslciphers", "TLSv1.2+HIGH:TLSv1+HIGH:!SSLv2:!aNULL:!eNULL:!3DES:@STRENGTH");
        SSL_CTX_set_cipher_list(rpc_ssl_context->impl(), strCiphers.c_str());
    }

    std::vector<ip::tcp::endpoint> vEndpoints;
    bool bBindAny = false;
    int defaultPort = GetArg("-rpcport", BaseParams().RPCPort());
    if (!mapArgs.count("-rpcallowip")) // Default to loopback if not allowing external IPs
    {
        vEndpoints.push_back(ip::tcp::endpoint(asio::ip::address_v6::loopback(), defaultPort));
        vEndpoints.push_back(ip::tcp::endpoint(asio::ip::address_v4::loopback(), defaultPort));
        if (mapArgs.count("-rpcbind"))
        {
            LogPrintf("WARNING: option -rpcbind was ignored because -rpcallowip was not specified, refusing to allow everyone to connect\n");
        }
    } else if (mapArgs.count("-rpcbind")) // Specific bind address
    {
        BOOST_FOREACH(const std::string &addr, mapMultiArgs["-rpcbind"])
        {
            try {
                vEndpoints.push_back(ParseEndpoint(addr, defaultPort));
            }
            catch(const boost::system::system_error &)
            {
                uiInterface.ThreadSafeMessageBox(
                    strprintf(_("Could not parse -rpcbind value %s as network address"), addr),
                    "", CClientUIInterface::MSG_ERROR);
                StartShutdown();
                return;
            }
        }
    } else { // No specific bind address specified, bind to any
        vEndpoints.push_back(ip::tcp::endpoint(asio::ip::address_v6::any(), defaultPort));
        vEndpoints.push_back(ip::tcp::endpoint(asio::ip::address_v4::any(), defaultPort));
        // Prefer making the socket dual IPv6/IPv4 instead of binding
        // to both addresses seperately.
        bBindAny = true;
    }

    bool fListening = false;
    std::string strerr;
    std::string straddress;
    BOOST_FOREACH(const ip::tcp::endpoint &endpoint, vEndpoints)
    {
        try {
            asio::ip::address bindAddress = endpoint.address();
            straddress = bindAddress.to_string();
            LogPrintf("Binding RPC on address %s port %i (IPv4+IPv6 bind any: %i)\n", straddress, endpoint.port(), bBindAny);
            boost::system::error_code v6_only_error;
            boost::shared_ptr<ip::tcp::acceptor> acceptor(new ip::tcp::acceptor(*rpc_io_service));

            acceptor->open(endpoint.protocol());
            acceptor->set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));

            // Try making the socket dual IPv6/IPv4 when listening on the IPv6 "any" address
            acceptor->set_option(boost::asio::ip::v6_only(
                !bBindAny || bindAddress != asio::ip::address_v6::any()), v6_only_error);

            acceptor->bind(endpoint);
            acceptor->listen(socket_base::max_connections);

            RPCListen(acceptor, *rpc_ssl_context, fUseSSL);

            fListening = true;
            rpc_acceptors.push_back(acceptor);
            // If dual IPv6/IPv4 bind successful, skip binding to IPv4 separately
            if(bBindAny && bindAddress == asio::ip::address_v6::any() && !v6_only_error)
                break;
        }
        catch(boost::system::system_error &e)
        {
            LogPrintf("ERROR: Binding RPC on address %s port %i failed: %s\n", straddress, endpoint.port(), e.what());
            strerr = strprintf(_("An error occurred while setting up the RPC address %s port %u for listening: %s"), straddress, endpoint.port(), e.what());
        }
    }

    if (!fListening) {
        uiInterface.ThreadSafeMessageBox(strerr, "", CClientUIInterface::MSG_ERROR);
        StartShutdown();
        return;
    }

    rpc_worker_group = new boost::thread_group();
    for (int i = 0; i < GetArg("-rpcthreads", 4); i++)
        rpc_worker_group->create_thread(boost::bind(&asio::io_service::run, rpc_io_service));
    
    fRPCRunning = true;
}

void StartDummyRPCThread()
{
    if(rpc_io_service == NULL)
    {
        rpc_io_service = new asio::io_service();
        /* Create dummy "work" to keep the thread from exiting when no timeouts active,
         * see http://www.boost.org/doc/libs/1_51_0/doc/html/boost_asio/reference/io_service.html#boost_asio.reference.io_service.stopping_the_io_service_from_running_out_of_work */
        rpc_dummy_work = new asio::io_service::work(*rpc_io_service);
        rpc_worker_group = new boost::thread_group();
        rpc_worker_group->create_thread(boost::bind(&asio::io_service::run, rpc_io_service));
        fRPCRunning = true;
    }
}

void StopRPCThreads()
{
    if (rpc_io_service == NULL) return;
    // Set this to false first, so that longpolling loops will exit when woken up
    fRPCRunning = false;

    // First, cancel all timers and acceptors
    // This is not done automatically by ->stop(), and in some cases the destructor of
    // asio::io_service can hang if this is skipped.
    boost::system::error_code ec;
    BOOST_FOREACH(const boost::shared_ptr<ip::tcp::acceptor> &acceptor, rpc_acceptors)
    {
        acceptor->cancel(ec);
        if (ec)
            LogPrintf("%s: Warning: %s when cancelling acceptor", __func__, ec.message());
    }
    rpc_acceptors.clear();
    BOOST_FOREACH(const PAIRTYPE(std::string, boost::shared_ptr<deadline_timer>) &timer, deadlineTimers)
    {
        timer.second->cancel(ec);
        if (ec)
            LogPrintf("%s: Warning: %s when cancelling timer", __func__, ec.message());
    }
    deadlineTimers.clear();

    rpc_io_service->stop();
    cvBlockChange.notify_all();
    if (rpc_worker_group != NULL)
        rpc_worker_group->join_all();
    delete rpc_dummy_work; rpc_dummy_work = NULL;
    delete rpc_worker_group; rpc_worker_group = NULL;
    delete rpc_ssl_context; rpc_ssl_context = NULL;
    delete rpc_io_service; rpc_io_service = NULL;
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

void RPCRunHandler(const boost::system::error_code& err, boost::function<void(void)> func)
{
    if (!err)
        func();
}

void RPCRunLater(const std::string& name, boost::function<void(void)> func, int64_t nSeconds)
{
    assert(rpc_io_service != NULL);

    if (deadlineTimers.count(name) == 0)
    {
        deadlineTimers.insert(make_pair(name,
                                        boost::shared_ptr<deadline_timer>(new deadline_timer(*rpc_io_service))));
    }
    deadlineTimers[name]->expires_from_now(posix_time::seconds(nSeconds));
    deadlineTimers[name]->async_wait(boost::bind(RPCRunHandler, _1, func));
}

class JSONRequest
{
public:
    Value id;
    string strMethod;
    Array params;

    JSONRequest() { id = Value::null; }
    void parse(const Value& valRequest);
};

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

/* MCHN START */    
    Value valChainName = find_value(request, "chain_name");
    if (valChainName.type() != null_type)
    {
        if (valChainName.type() != str_type)
            throw JSONRPCError(RPC_INVALID_REQUEST, "Chain name must be a string");
        if (strcmp(valChainName.get_str().c_str(),mc_gState->m_Params->NetworkName()))
            throw JSONRPCError(RPC_INVALID_REQUEST, "Wrong chain name");
    }
/* MCHN END */    
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
/* MCHN START */    
    uint32_t wallet_mode=mc_gState->m_WalletMode;
/* MCHN END */    
    try {
        jreq.parse(req);

        Value result = tableRPC.execute(jreq.strMethod, jreq.params,jreq.id);
        rpc_result = JSONRPCReplyObj(result, Value::null, jreq.id);
    }
    catch (Object& objError)
    {
/* MCHN START */    
        mc_gState->m_WalletMode=wallet_mode;
        if(fDebug)LogPrint("mcapi","mcapi: API request failure A: %s\n",JSONRPCMethodIDForLog(jreq.strMethod,jreq.id).c_str());        
/* MCHN END */    
        rpc_result = JSONRPCReplyObj(Value::null, objError, jreq.id);
    }
    catch (std::exception& e)
    {
/* MCHN START */    
        mc_gState->m_WalletMode=wallet_mode;
        if(fDebug)LogPrint("mcapi","mcapi: API request failure B: %s\n",JSONRPCMethodIDForLog(jreq.strMethod,jreq.id).c_str());        
/* MCHN END */    
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

static bool HTTPReq_JSONRPC(AcceptedConnection *conn,
                            string& strRequest,
                            map<string, string>& mapHeaders,
                            bool fRun)
{
    // Check authorization
    if (mapHeaders.count("authorization") == 0)
    {
        conn->stream() << HTTPError(HTTP_UNAUTHORIZED, false) << std::flush;
        return false;
    }

    if (!HTTPAuthorized(mapHeaders))
    {
        LogPrintf("ThreadRPCServer incorrect password attempt from %s\n", conn->peer_address_to_string());
        /* Deter brute-forcing
           If this results in a DoS the user really
           shouldn't have their RPC port exposed. */
        MilliSleep(250);

        conn->stream() << HTTPError(HTTP_UNAUTHORIZED, false) << std::flush;
        return false;
    }

    JSONRequest jreq;
/* MCHN START */    
    uint32_t wallet_mode=mc_gState->m_WalletMode;
    if(fDebug)LogPrint("mcapi","mcapi: API request from %s\n",conn->peer_address_to_string().c_str());
/* MCHN END */    
    try
    {
        // Parse request
        Value valRequest;
        if (!read_string(strRequest, valRequest))
            throw JSONRPCError(RPC_PARSE_ERROR, "Parse error");

        // Return immediately if in warmup
        {
            LOCK(cs_rpcWarmup);
            if (fRPCInWarmup)
                throw JSONRPCError(RPC_IN_WARMUP, rpcWarmupStatus);
        }

        string strReply;

        // singleton request
        if (valRequest.type() == obj_type) {
            jreq.parse(valRequest);

            Value result = tableRPC.execute(jreq.strMethod, jreq.params,jreq.id);

            // Send reply
            strReply = JSONRPCReply(result, Value::null, jreq.id);

        // array of requests
        } else if (valRequest.type() == array_type)
            strReply = JSONRPCExecBatch(valRequest.get_array());
        else
            throw JSONRPCError(RPC_PARSE_ERROR, "Top-level object parse error");

        conn->stream() << HTTPReplyHeader(HTTP_OK, fRun, strReply.size()) << strReply << std::flush;
    }
    catch (Object& objError)
    {
/* MCHN START */    
        mc_gState->m_WalletMode=wallet_mode;
        if(fDebug)LogPrint("mcapi","mcapi: API request failure: %s, code: %d\n",JSONRPCMethodIDForLog(jreq.strMethod,jreq.id).c_str(),find_value(objError, "code").get_int());
        
//        if(fDebug)LogPrint("mcapi","mcapi: API request failure C\n");        
/* MCHN END */    
        ErrorReply(conn->stream(), objError, jreq.id);
        return false;
    }
    catch (std::exception& e)
    {
/* MCHN START */    
        mc_gState->m_WalletMode=wallet_mode;
        if(fDebug)LogPrint("mcapi","mcapi: API request failure D: %s\n",JSONRPCMethodIDForLog(jreq.strMethod,jreq.id).c_str());        
/* MCHN END */    
        ErrorReply(conn->stream(), JSONRPCError(RPC_PARSE_ERROR, e.what()), jreq.id);
        return false;
    }
    return true;
}

void ServiceConnection(AcceptedConnection *conn)
{
    bool fRun = true;
    while (fRun && !ShutdownRequested())
    {
        int nProto = 0;
        map<string, string> mapHeaders;
        string strRequest, strMethod, strURI;

        // Read HTTP request line
        if (!ReadHTTPRequestLine(conn->stream(), nProto, strMethod, strURI))
            break;

        // Read HTTP message headers and body
        ReadHTTPMessage(conn->stream(), mapHeaders, strRequest, nProto, MAX_SIZE);

        // HTTP Keep-Alive is false; close connection immediately
        if ((mapHeaders["connection"] == "close") || (!GetBoolArg("-rpckeepalive", false)))
            fRun = false;

        // Process via JSON-RPC API
        if (strURI == "/") {
            if (!HTTPReq_JSONRPC(conn, strRequest, mapHeaders, fRun))
                break;

        // Process via HTTP REST API
        } else if (strURI.substr(0, 6) == "/rest/" && GetBoolArg("-rest", false)) {
            if (!HTTPReq_REST(conn, strURI, mapHeaders, fRun))
                break;

        } else {
            conn->stream() << HTTPError(HTTP_NOT_FOUND, false) << std::flush;
            break;
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
    if (pcmd->reqWallet && !pwalletMain)
        throw JSONRPCError(RPC_METHOD_NOT_FOUND, "Method not found (disabled)");
#endif

//    if(mc_gState->m_ProtocolVersionToUpgrade > mc_gState->m_NetworkParams->ProtocolVersion())
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
            LogPrint("mcapi","mcapi: API request: %s\n",strRequest.c_str());
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
/* MCHN START */                
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
/* MCHN END */                
                result = pcmd->actor(params, false);
/* MCHN START */ 
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
/* MCHN END */                
                
            }
#else // ENABLE_WALLET
            else {
                LOCK(cs_main);
                result = pcmd->actor(params, false);
            }
#endif // !ENABLE_WALLET
        }
/* MCHN START */        
        if(fDebug)LogPrint("mcapi","mcapi: API request successful: %s\n",JSONRPCMethodIDForLog(strMethod,req_id).c_str());
/* MCHN END */        
        return result;
    }
    catch (std::exception& e)
    {
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

