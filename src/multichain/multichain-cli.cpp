// Copyright (c) 2014-2016 The Bitcoin Core developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "chainparams/chainparamsbase.h"
#include "version/clientversion.h"
#include "rpc/rpcclient.h"
#include "rpc/rpcprotocol.h"
#include "rpc/rpcasio.h"
#include "utils/util.h"
#include "utils/utilstrencodings.h"

/* MCHN START */
#include "multichain/multichain.h"                                              
#include "chainparams/globals.h"
/* MCHN END */

#include <boost/filesystem.hpp>
//#include <boost/filesystem/fstream.hpp>
#include <boost/filesystem/operations.hpp>

#define _(x) std::string(x) /* Keep the _() around in case gettext or such will be used later to translate non-UI */

using namespace std;
using namespace boost;
using namespace boost::asio;
using namespace json_spirit;

static const int CONTINUE_EXECUTION=-1;
extern unsigned int JSON_NO_DOUBLE_FORMATTING;  
extern int JSON_DOUBLE_DECIMAL_DIGITS;                             

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

Object CallRPC(const string& strMethod, const Array& params)
{
    if (mapArgs["-rpcuser"] == "" && mapArgs["-rpcpassword"] == "")
        throw runtime_error(strprintf(
            _("No credentials found for chain \"%s\"\n\n"
              "You must set rpcpassword=<password> in the configuration file:\n%s/multichain.conf\n"
              "If the file does not exist, create it with owner-readable-only file permissions."),
                mc_gState->m_Params->NetworkName(),mc_gState->m_Params->DataDir(1,0)));

    // Connect to localhost
    bool fUseSSL = GetBoolArg("-rpcssl", false);
    asio::io_service io_service;
    ssl::context context(io_service, ssl::context::sslv23);
    context.set_options(ssl::context::no_sslv2 | ssl::context::no_sslv3);
    asio::ssl::stream<asio::ip::tcp::socket> sslStream(io_service, context);
    SSLIOStreamDevice<asio::ip::tcp> d(sslStream, fUseSSL);
    iostreams::stream< SSLIOStreamDevice<asio::ip::tcp> > stream(d);

    const bool fConnected = d.connect(GetArg("-rpcconnect", "127.0.0.1"), GetArg("-rpcport", itostr(BaseParams().RPCPort())));
    if (!fConnected)
        throw CConnectionFailed("couldn't connect to server");

    // HTTP basic authentication
    
    string strUserPass64 = EncodeBase64(mapArgs["-rpcuser"] + ":" + mapArgs["-rpcpassword"]);
    map<string, string> mapRequestHeaders;
    mapRequestHeaders["Authorization"] = string("Basic ") + strUserPass64;
    // Send request
//    JSON_NO_DOUBLE_FORMATTING=1;    
    
    int32_t id_nonce;
    id_nonce=mc_RandomInRange(10000000,99999999);
    Value req_id=strprintf("%08d-%u",id_nonce,mc_TimeNowAsUInt());
    
    string strRequest = JSONRPCRequest(strMethod, params, req_id);
//    JSON_NO_DOUBLE_FORMATTING=0;    
    JSON_DOUBLE_DECIMAL_DIGITS=GetArg("-apidecimaldigits",-1);        
    string strPost = HTTPPost(strRequest, mapRequestHeaders);
    stream << strPost << std::flush;

    string requestout=GetArg("-requestout","stderr");
    if(requestout == "stdout")
    {
        fprintf(stdout, "%s\n", strRequest.c_str());        
    }
    if(requestout == "stderr")
    {
        fprintf(stderr, "%s\n", strRequest.c_str());        
    }
    
    // Receive HTTP reply status
    int nProto = 0;
    int nStatus = ReadHTTPStatus(stream, nProto);

    // Receive HTTP reply message headers and body
    map<string, string> mapHeaders;
    string strReply;
    ReadHTTPMessage(stream, mapHeaders, strReply, nProto, std::numeric_limits<size_t>::max());

    if (nStatus == HTTP_UNAUTHORIZED)
        throw runtime_error("incorrect rpcuser or rpcpassword (authorization failed)");
    else if (nStatus >= 400 && nStatus != HTTP_BAD_REQUEST && nStatus != HTTP_NOT_FOUND && nStatus != HTTP_INTERNAL_SERVER_ERROR)
        throw runtime_error(strprintf("server returned HTTP error %d", nStatus));
    else if (strReply.empty())
        throw runtime_error("no response from server");

    // Parse reply
    Value valReply;
    if (!read_string(strReply, valReply))
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

