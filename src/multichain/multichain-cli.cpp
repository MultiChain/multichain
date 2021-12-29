// Copyright (c) 2014-2016 The Bitcoin Core developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "chainparams/chainparamsbase.h"
#include "version/clientversion.h"
#include "rpc/rpcclient.h"
#include "rpc/rpcprotocol.h"
#include "utils/random.h"
#include "utils/util.h"
#include "utils/utilstrencodings.h"

/* MCHN START */
#include "multichain/multichain.h"                                              
#include "chainparams/globals.h"
/* MCHN END */

#include <boost/filesystem.hpp>
//#include <boost/filesystem/fstream.hpp>
#include <boost/filesystem/operations.hpp>

#include <event2/buffer.h>
#include <event2/keyvalq_struct.h>
#include <rpc/rpcevents.h>


#define _(x) std::string(x) /* Keep the _() around in case gettext or such will be used later to translate non-UI */

using namespace std;
using namespace boost;
using namespace json_spirit;

static const int CONTINUE_EXECUTION=-1;
extern unsigned int JSON_NO_DOUBLE_FORMATTING;  
extern int JSON_DOUBLE_DECIMAL_DIGITS;                             
static const int DEFAULT_HTTP_CLIENT_TIMEOUT=900;

string FormatFullMultiChainVersion();

std::string HelpMessageCli()
{
    string strUsage;
    strUsage += _("Options:") + "\n";
    strUsage += "  -?                       " + _("This help message") + "\n";
    strUsage += "  -conf=<file>             " + strprintf(_("Specify configuration file (default: %s)"), "multichain.conf") + "\n";
    strUsage += "  -datadir=<dir>           " + _("Specify data directory") + "\n";
    strUsage += "  -cold                    " + _("Connect to multichaind-cold: use multichaind-cold default directory if -datadir is not set") + "\n";
/* MCHN START */    
    strUsage += "  -requestout=<requestout> " + _("Send request to stderr, stdout or null (not print it at all), default stderr") + "\n"; 
    strUsage += "  -saveclilog=<n>          " + _("If <n>=0 multichain-cli history is not saved, default 1") + "\n";
/*    
    strUsage += "  -testnet               " + _("Use the test network") + "\n";
    strUsage += "  -regtest               " + _("Enter regression test mode, which uses a special chain in which blocks can be "
                                                "solved instantly. This is intended for regression testing tools and app development.") + "\n";
 */ 
/* MCHN END */    
    strUsage += "  -rpcconnect=<ip>         " + strprintf(_("Send commands to node running on <ip> (default: %s)"), "127.0.0.1") + "\n";
    strUsage += "  -rpcport=<port>          " + _("Connect to JSON-RPC on <port> ") + "\n";
    strUsage += "  -rpcwait                 " + _("Wait for RPC server to start") + "\n";
    strUsage += "  -rpcuser=<user>          " + _("Username for JSON-RPC connections") + "\n";
    strUsage += "  -rpcpassword=<pw>        " + _("Password for JSON-RPC connections") + "\n";

    strUsage += "\n" + _("SSL options: ") + "\n";
    strUsage += "  -rpcssl                  " + _("Use OpenSSL (https) for JSON-RPC connections") + "\n";

    return strUsage;
}

//////////////////////////////////////////////////////////////////////////////
//
// Start
//

//
// Exception thrown on connection error.  This error is used to determine
// when to wait if -rpcwait is given.
//
class CConnectionFailed : public std::runtime_error
{
public:

    explicit inline CConnectionFailed(const std::string& msg) :
        std::runtime_error(msg)
    {}

};

//
// This function returns either one of EXIT_ codes when it's expected to stop the process or
// CONTINUE_EXECUTION when it's expected to continue further.
//
static int AppInitRPC(int argc, char* argv[])
{
    //
    // Parameters
    //
    int err = MC_ERR_NOERROR;
    int minargs=2;

#ifndef WIN32
    minargs=1;
#endif
    
    RandomInit();
    //
    // Parameters
    //
    // If Qt is used, parameters/bitcoin.conf are parsed in qt/bitcoin.cpp's main()

    mc_gState=new mc_State;
    
    mc_gState->m_Params->Parse(argc, argv, MC_ETP_CLI);

    if(GetBoolArg("-cold",false))
    {
        mc_gState->m_SessionFlags |= MC_SSF_COLD;
    }
                
    mc_CheckDataDirInConfFile();
   
    if(mc_gState->m_Params->NetworkName())
    {
        if(strlen(mc_gState->m_Params->NetworkName()) > MC_PRM_NETWORK_NAME_MAX_SIZE)
        {
            fprintf(stderr, "ERROR: invalid chain name: %s\n",mc_gState->m_Params->NetworkName());
            return EXIT_FAILURE;
        }
    }
    
//    ParseParameters(argc, argv);
      if (mc_gState->m_Params->HasOption("-?") || 
        mc_gState->m_Params->HasOption("-help") || 
        mc_gState->m_Params->HasOption("-version") || 
        (mc_gState->m_Params->NetworkName() == NULL) ||
        mc_gState->m_Params->m_NumArguments<minargs)
      {
        fprintf(stdout,"\nMultiChain %s RPC client\n\n",mc_BuildDescription(mc_gState->GetNumericVersion()).c_str());
        
        std::string strUsage = "";
        if (mc_gState->m_Params->HasOption("-version"))
        {
        }
        else
        {
            strUsage += "\n" + _("Usage:") + "\n" +
                  "  multichain-cli <blockchain-name> [options] <command> [params]  " + _("Send command to MultiChain Core") + "\n" +
                  "  multichain-cli <blockchain-name> [options] help                " + _("List commands") + "\n" +
                  "  multichain-cli <blockchain-name> [options] help <command>      " + _("Get help for a command") + "\n";

            strUsage += "\n" + HelpMessageCli();                                // MCHN-TODO Edit help message
        }

        fprintf(stdout, "%s", strUsage.c_str());
        return EXIT_SUCCESS;
    }

    
    
/*    
    if (!boost::filesystem::is_directory(GetDataDir(false))) {
        fprintf(stderr, "Error: Specified data directory \"%s\" does not exist.\n", mapArgs["-datadir"].c_str());
        return false;
    }
*/
    int RPCPort=MC_DEFAULT_RPC_PORT;        
    int read_err;
    err=MC_ERR_NOERROR;
    
    boost::filesystem::path path=boost::filesystem::path(string(mc_gState->m_Params->DataDir(0,0)));
    if(boost::filesystem::is_directory(path))
    {
        path=boost::filesystem::path(string(mc_gState->m_Params->DataDir(1,0)));
    
        if (!boost::filesystem::is_directory(path)) 
        {
            err=mc_gState->m_Params->ReadConfig(NULL);
        }
        else
        {
            read_err=mc_gState->m_NetworkParams->Read(mc_gState->m_Params->NetworkName());
            if(read_err)
            {
                err=mc_gState->m_Params->ReadConfig(NULL);     
            
                if (mapArgs["-rpcuser"] == "" && mapArgs["-rpcpassword"] == "")
                {
                    if(read_err != MC_ERR_FILE_READ_ERROR)
                    {
                        fprintf(stderr,"ERROR: Couldn't read configuration file for blockchain %s. Please try upgrading MultiChain. Exiting...\n",mc_gState->m_Params->NetworkName());
                        return EXIT_FAILURE;
                    }
                }
            }
            else
            {
                err=mc_gState->m_Params->ReadConfig(mc_gState->m_Params->NetworkName());            
                RPCPort=mc_gState->m_NetworkParams->GetInt64Param("defaultrpcport");
            }
        }
    }    
    
    if(err)
    {
        fprintf(stderr,"ERROR: Couldn't read parameter file for blockchain %s. Exiting...\n",mc_gState->m_Params->NetworkName());
        return EXIT_FAILURE;
    }

    RPCPort=mc_gState->m_Params->GetOption("-rpcport",RPCPort);
    
    SelectMultiChainBaseParams(mc_gState->m_Params->NetworkName(),RPCPort);
    
    // Check for -testnet or -regtest parameter (BaseParams() calls are only valid after this clause)
/*    
    if (!SelectBaseParamsFromCommandLine()) {
        fprintf(stderr, "Error: Invalid combination of -regtest and -testnet.\n");
        return false;
    }
 */ 

    return CONTINUE_EXECUTION;
}

/** Reply structure for request_done to fill in */
struct RPCHTTPReply
{
    RPCHTTPReply(): status(0), error(-1) {}

    int status;
    int error;
    std::string body;
};

static std::string http_errorstring(int code)
{
    switch(code) {
#if LIBEVENT_VERSION_NUMBER >= 0x02010300
    case EVREQ_HTTP_TIMEOUT:
        return "timeout reached";
    case EVREQ_HTTP_EOF:
        return "EOF reached";
    case EVREQ_HTTP_INVALID_HEADER:
        return "error while reading header, or invalid header";
    case EVREQ_HTTP_BUFFER_ERROR:
        return "error encountered while reading or writing";
    case EVREQ_HTTP_REQUEST_CANCEL:
        return "request was canceled";
    case EVREQ_HTTP_DATA_TOO_LONG:
        return "response body is larger than allowed";
#endif
    default:
        return "unknown";
    }
}

static void http_request_done(struct evhttp_request *req, void *ctx)
{
    RPCHTTPReply *reply = static_cast<RPCHTTPReply*>(ctx);

    if (req == nullptr) {
        /* If req is nullptr, it means an error occurred while connecting: the
         * error code will have been passed to http_error_cb.
         */
        reply->status = 0;
        return;
    }

    reply->status = evhttp_request_get_response_code(req);

    struct evbuffer *buf = evhttp_request_get_input_buffer(req);
    if (buf)
    {
        size_t size = evbuffer_get_length(buf);
        const char *data = (const char*)evbuffer_pullup(buf, size);
        if (data)
            reply->body = std::string(data, size);
        evbuffer_drain(buf, size);
    }
}

#if LIBEVENT_VERSION_NUMBER >= 0x02010300
static void http_error_cb(enum evhttp_request_error err, void *ctx)
{
    RPCHTTPReply *reply = static_cast<RPCHTTPReply*>(ctx);
    reply->error = err;
}
#endif

Object CallRPC(const string& strMethod, const Array& params)
{
    if (mapArgs["-rpcuser"] == "" && mapArgs["-rpcpassword"] == "")
        throw runtime_error(strprintf(
            _("No credentials found for chain \"%s\"\n\n"
              "You must set rpcpassword=<password> in the configuration file:\n%s/multichain.conf\n"
              "If the file does not exist, create it with owner-readable-only file permissions."),
                mc_gState->m_Params->NetworkName(),mc_gState->m_Params->DataDir(1,0)));

    std::string host;
    // In preference order, we choose the following for the port:
    //     1. -rpcport
    //     2. port in -rpcconnect (ie following : in ipv4 or ]: in ipv6)
    //     3. default port for chain
    int port;
    SplitHostPort(GetArg("-rpcconnect", "127.0.0.1"), port, host);
    port = GetArg("-rpcport", BaseParams().RPCPort());
    
    // Obtain event base
    raii_event_base base = obtain_event_base();

    // Synchronously look up hostname
    raii_evhttp_connection evcon = obtain_evhttp_connection_base(base.get(), host, port);

    // Set connection timeout
    {
        const int timeout = GetArg("-rpcclienttimeout", DEFAULT_HTTP_CLIENT_TIMEOUT);
        if (timeout > 0) {
            evhttp_connection_set_timeout(evcon.get(), timeout);
        } else {
            // Indefinite request timeouts are not possible in libevent-http, so we
            // set the timeout to a very long time period instead.

            constexpr int YEAR_IN_SECONDS = 31556952; // Average length of year in Gregorian calendar
            evhttp_connection_set_timeout(evcon.get(), 5 * YEAR_IN_SECONDS);
        }
    }

    RPCHTTPReply response;
    raii_evhttp_request req = obtain_evhttp_request(http_request_done, (void*)&response);
    if (req == nullptr)
        throw std::runtime_error("create http request failed");
#if LIBEVENT_VERSION_NUMBER >= 0x02010300
    evhttp_request_set_error_cb(req.get(), http_error_cb);
#endif
    
    // Get credentials
    string strUserPass64 = EncodeBase64(mapArgs["-rpcuser"] + ":" + mapArgs["-rpcpassword"]);
    string strAuthorization=string("Basic ") + strUserPass64;
    int32_t id_nonce;
    id_nonce=mc_RandomInRange(10000000,99999999);
    Value req_id=strprintf("%08d-%u",id_nonce,mc_TimeNowAsUInt());
    
    string strRequest = JSONRPCRequest(strMethod, params, req_id);
    string strRequestLength=strprintf("%ld",strRequest.size());

    struct evkeyvalq* output_headers = evhttp_request_get_output_headers(req.get());
    assert(output_headers);
    evhttp_add_header(output_headers, "User-Agent", ("multichain-json-rpc/" + FormatFullMultiChainVersion()).c_str());
    evhttp_add_header(output_headers, "Host", host.c_str());
    evhttp_add_header(output_headers, "Connection", "close");
    evhttp_add_header(output_headers, "Content-Type", "application/json");
    evhttp_add_header(output_headers, "Content-Length", strRequestLength.c_str());
    evhttp_add_header(output_headers, "Accept", "application/json");
    evhttp_add_header(output_headers, "Authorization", strAuthorization.c_str());

    struct evbuffer* output_buffer = evhttp_request_get_output_buffer(req.get());
    assert(output_buffer);
    evbuffer_add(output_buffer, strRequest.data(), strRequest.size());

    std::string endpoint = "/";

    int r = evhttp_make_request(evcon.get(), req.get(), EVHTTP_REQ_POST, endpoint.c_str());
    req.release(); // ownership moved to evcon in above call
    if (r != 0) {
        throw CConnectionFailed("send http request failed");
    }
    
    event_base_dispatch(base.get());

    if (response.status == 0) {
        std::string responseErrorMessage;
        if (response.error != -1) {
            responseErrorMessage = strprintf(" (error code %d - \"%s\")", response.error, http_errorstring(response.error));
        }
        throw CConnectionFailed(strprintf("Could not connect to the server %s:%d%s\n\nMake sure the multichaind server is running and that you are connecting to the correct RPC port.", host, port, responseErrorMessage));
    } else if (response.status == HTTP_UNAUTHORIZED) {
        throw std::runtime_error("Authorization failed: Incorrect rpcuser or rpcpassword");
    } else if (response.status == HTTP_SERVICE_UNAVAILABLE) {
        throw std::runtime_error(strprintf("Server response: %s", response.body));
    } else if (response.status >= 400 && response.status != HTTP_BAD_REQUEST && response.status != HTTP_NOT_FOUND && response.status != HTTP_INTERNAL_SERVER_ERROR)
        throw std::runtime_error(strprintf("server returned HTTP error %d", response.status));
    else if (response.body.empty())
        throw std::runtime_error("no response from server");

    
    string requestout=GetArg("-requestout","stderr");
    if(requestout == "stdout")
    {
        fprintf(stdout, "%s\n", strRequest.c_str());        
    }
    if(requestout == "stderr")
    {
        fprintf(stderr, "%s\n", strRequest.c_str());        
    }
    
    // Parse reply
    Value valReply;
    if (!read_string(response.body, valReply))
        throw runtime_error("couldn't parse reply from server");
    const Object& reply = valReply.get_obj();
    if (reply.empty())
        throw runtime_error("expected reply to have result, error and id properties");

    return reply;
}

int CommandLineRPC(int argc, char *argv[])
{
    string strPrint;
    int nRet = 0;
    
    bool fWaitForNetworkName=true; 
    try {
        // Skip switches
        while (argc > 1 && (IsSwitchChar(argv[1][0]) || fWaitForNetworkName)) {
            if(!IsSwitchChar(argv[1][0]))
            {
                fWaitForNetworkName=false;
            }
            argc--;
            argv++;
        }

        // Method
        if (argc < 2)
            throw runtime_error("too few parameters");
        string strMethod = argv[1];

        // Parameters default to strings
        std::vector<std::string> strParams(&argv[2], &argv[argc]);
        Array params = RPCConvertValues(strMethod, strParams);

        // Execute and handle connection failures with -rpcwait
        const bool fWait = GetBoolArg("-rpcwait", false);
        do {
            try {
                const Object reply = CallRPC(strMethod, params);

                // Parse reply
                const Value& result = find_value(reply, "result");
                const Value& error  = find_value(reply, "error");

                if (error.type() != null_type) {
                    // Error
                    const int code = find_value(error.get_obj(), "code").get_int();
                    if (fWait && code == RPC_IN_WARMUP)
                        throw CConnectionFailed("server in warmup");
                    strPrint = "error: " + write_string(error, false);
                    nRet = abs(code);
                    
                    if (error.type() == obj_type)
                    {
                        Value errCode = find_value(error.get_obj(), "code");
                        Value errMsg  = find_value(error.get_obj(), "message");
                        strPrint = (errCode.type() == null_type) ? "" : "error code: "+strprintf("%s",errCode.get_int())+"\n";

                        if (errMsg.type() == str_type)
                            strPrint += "error message:\n"+errMsg.get_str();
                    }
                    
                } else {
                    // Result
                    if (result.type() == null_type)
                        strPrint = "";
                    else if (result.type() == str_type)
                        strPrint = result.get_str();
                    else
                        strPrint = write_string(result, true);
                }

                // Connection succeeded, no need to retry.
                break;
            }
            catch (const CConnectionFailed& e) {
                if (fWait)
                    MilliSleep(1000);
                else
                    throw;
            }
        } while (fWait);
    }
    catch (boost::thread_interrupted) {
        throw;
    }
    catch (std::exception& e) {
        strPrint = string("error: ") + e.what();
        nRet = EXIT_FAILURE;
    }
    catch (...) {
        PrintExceptionContinue(NULL, "CommandLineRPC()");
        throw;
    }

    if (strPrint != "") {
        fprintf((nRet == 0 ? stdout : stderr), "%s\n", strPrint.c_str());
    }
    return nRet;
}

int main(int argc, char* argv[])
{
    SetupEnvironment();
    if (!SetupNetworking()) {
        printf("Error: Initializing networking failed\n");
        return EXIT_FAILURE;
    }
    
    try {
        int ret = AppInitRPC(argc, argv);
        if (ret != CONTINUE_EXECUTION)
            return ret;
     }
    catch (std::exception& e) {
        PrintExceptionContinue(&e, "AppInitRPC()");
        return EXIT_FAILURE;
    } catch (...) {
        PrintExceptionContinue(NULL, "AppInitRPC()");
        return EXIT_FAILURE;
    }
    
#ifndef WIN32
    umask(077);        
#endif

    boost::filesystem::path path_cli_log;
    if (mapArgs.count("-datadir")) 
    {
        path_cli_log = boost::filesystem::system_complete(mapArgs["-datadir"]);
    } 
    else 
    {
        path_cli_log = string(mc_gState->m_Params->DataDir(0,0));
    }
    path_cli_log /= string(".cli_history");
    boost::filesystem::create_directories(path_cli_log);
    path_cli_log /= string(mc_gState->m_Params->NetworkName() + string(".log"));
    
    if (mapArgs["-rpcuser"] == "" && mapArgs["-rpcpassword"] == "")
    {
        string strMethod=strprintf("%s",mc_gState->m_Params->NetworkName());
        if(HaveAPIWithThisName(strMethod))
        {
            fprintf(stdout,"\nMultiChain %s RPC client\n\n",mc_BuildDescription(mc_gState->GetNumericVersion()).c_str());
            printf("ERROR: Couldn't read configuration file for blockchain %s. \n\n"
                    "Be sure include the blockchain name before the command name, e.g.:\n\n"
                    "multichain-cli chain1 %s\n\n",mc_gState->m_Params->NetworkName(),mc_gState->m_Params->NetworkName());
            return EXIT_FAILURE;                        
        }
    }
    
 #ifndef WIN32   
    if(mc_gState->m_Params->m_NumArguments == 1)                                // Interactive mode
    {
        fprintf(stdout,"\nMultiChain %s RPC client\n\n",mc_BuildDescription(mc_gState->GetNumericVersion()).c_str());
        if (mapArgs["-rpcuser"] == "" && mapArgs["-rpcpassword"] == "")
        {
            string str=strprintf(
                _("No credentials found for chain \"%s\"\n\n"
                  "You must set rpcpassword=<password> in the configuration file:\n%s/multichain.conf\n"
                  "If the file does not exist, create it with owner-readable-only file permissions."),
                    mc_gState->m_Params->NetworkName(),mc_gState->m_Params->DataDir(1,0));
            printf("error: %s\n",str.c_str());
            return EXIT_FAILURE;
        }
        
        fprintf(stdout,"\nInteractive mode\n\n");
        
        mc_TerminalInput *term=new mc_TerminalInput;
        term->SetPrompt(mc_gState->m_Params->NetworkName());
        char *command;
        char *commandEnd;
        
        char *dest;
        dest=NULL;
        dest=(char*)mc_New(MC_DCT_TERM_BUFFER_SIZE+16384);
        char *argv_p[1024];
        int argc_p,offset,shift;
        bool exitnow=false;

        term->LoadDataFromLog(path_cli_log.string().c_str());
        
        term->Prompt();
        while(!exitnow && ((command=term->GetLine()) != NULL))
        {
            if((strcmp(command,"exit")==0) || 
               (strcmp(command,"quit")==0) || 
               (strcmp(command,"bye")==0))
            {
                printf("\nBye\n");
                exitnow=1;    
            }
            else
            {
                printf("\n");
                commandEnd=command+strlen(command);
                offset=0;
                argc_p=0;
                strcpy(dest+offset,"multichain-cli");
                argv_p[argc_p]=dest+offset;
                argc_p++;
                offset+=strlen(dest+offset)+1;
                strcpy(dest+offset,mc_gState->m_Params->NetworkName());
                argv_p[argc_p]=dest+offset;
                argc_p++;
                offset+=strlen(dest+offset)+1;
                shift=mc_StringToArg(command,dest+offset);
                while(shift>0)
                {                    
                    argv_p[argc_p]=dest+offset;
                    argc_p++;
                    offset+=strlen(dest+offset)+1;                    
                    command+=shift;
                    shift=mc_StringToArg(command,dest+offset);
                    if(argc>=1024)
                    {
                        shift=0;
                    }
                    if(command>=commandEnd)
                    {
                        shift=0;                        
                    }
                }
                
                if(shift<0)
                {
                    printf("\nParsing error: %s\n",command);
                }
                else
                {
                    if(argc_p>2)
                    {
                        try {
                            if(GetBoolArg("-saveclilog",true))
                            {
                                mc_SaveCliCommandToLog(path_cli_log.string().c_str(), argc_p, argv_p);
                            }
                            CommandLineRPC(argc_p, argv_p);
                        }
                        catch (std::exception& e) {
                            PrintExceptionContinue(&e, "CommandLineRPC()");
                        } catch (...) {
                            PrintExceptionContinue(NULL, "CommandLineRPC()");
                        }
                    }
                }

                term->Prompt();                
            }
        }
        
        mc_Delete(dest);
        delete term;
        
        return 0;
    }
#endif

    int ret = EXIT_FAILURE;
    try {
        mc_SaveCliCommandToLog(path_cli_log.string().c_str(), argc, argv);
        ret = CommandLineRPC(argc, argv);
    }
    catch (std::exception& e) {
        PrintExceptionContinue(&e, "CommandLineRPC()");
    } catch (...) {
        PrintExceptionContinue(NULL, "CommandLineRPC()");
    }
    return ret;
}

