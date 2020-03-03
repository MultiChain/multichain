// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#if defined(HAVE_CONFIG_H)
#include "config/bitcoin-config.h"
#endif

#include "core/init.h"

#include "storage/addrman.h"
#include "structs/amount.h"
#include "chain/checkpoints.h"
#include "compat/sanity.h"
#include "keys/key.h"
#include "core/main.h"
#include "miner/miner.h"
#include "net/net.h"
#include "rpc/rpcserver.h"
#include "script/standard.h"
#include "storage/txdb.h"
#include "ui/ui_interface.h"
#include "utils/util.h"
#include "utils/utilmoneystr.h"
#ifdef ENABLE_WALLET
#include "wallet/dbwrap.h"
#include "wallet/wallet.h"
#include "wallet/walletdb.h"
#endif


/* MCHN START */

#include "structs/base58.h"
#include "multichain/multichain.h"
#include "wallet/wallettxs.h"
#include "filters/filter.h"
std::string BurnAddress(const std::vector<unsigned char>& vchVersion);

void HandleSIGTERM(int);
void HandleSIGHUP(int);
bool InitSanityCheck(void);



/* MCHN END */

#include <stdint.h>
#include <stdio.h>

#ifndef WIN32
#include <signal.h>
#endif

#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/filesystem.hpp>
#include <boost/interprocess/sync/file_lock.hpp>
#include <boost/thread.hpp>
#include <openssl/crypto.h>

using namespace boost;
using namespace std;


#ifdef WIN32
// Win32 LevelDB doesn't use filedescriptors, and the ones used for
// accessing block files, don't count towards to fd_set size limit
// anyway.
#define MIN_CORE_FILEDESCRIPTORS 0
#else
#define MIN_CORE_FILEDESCRIPTORS 150
#endif

/** Used to pass flags to the Bind() function */
enum BindFlags {
    BF_NONE         = 0,
    BF_EXPLICIT     = (1U << 0),
    BF_REPORT_ERROR = (1U << 1),
    BF_WHITELIST    = (1U << 2),
};

static boost::scoped_ptr<ECCVerifyHandle> globalVerifyHandle;
extern bool fRequestShutdown;
extern bool fShutdownCompleted;



bool static InitError(const std::string &str)
{
    uiInterface.ThreadSafeMessageBox(str, "", CClientUIInterface::MSG_ERROR);
    return false;
}

bool static InitWarning(const std::string &str)
{
    uiInterface.ThreadSafeMessageBox(str, "", CClientUIInterface::MSG_WARNING);
    return true;
}

class CCoinsViewErrorCatcher : public CCoinsViewBacked
{
public:
    CCoinsViewErrorCatcher(CCoinsView* view) : CCoinsViewBacked(view) {}
    bool GetCoins(const uint256 &txid, CCoins &coins) const {
        try {
            return CCoinsViewBacked::GetCoins(txid, coins);
        } catch(const std::runtime_error& e) {
            uiInterface.ThreadSafeMessageBox(_("Error reading from database, shutting down."), "", CClientUIInterface::MSG_ERROR);
            LogPrintf("Error reading from database: %s\n", e.what());
            // Starting the shutdown sequence and returning false to the caller would be
            // interpreted as 'entry not found' (as opposed to unable to read data), and
            // could lead to invalid interpration. Just exit immediately, as we can't
            // continue anyway, and all writes should be atomic.
            abort();
        }
    }
    // Writes do not need similar protection, as failure to write is handled by the caller.
};

static CCoinsViewDB *pcoinsdbview = NULL;
static CCoinsViewErrorCatcher *pcoinscatcher = NULL;


void Shutdown_Cold()
{
    LogPrintf("%s: In progress...\n", __func__);
    static CCriticalSection cs_Shutdown;
    TRY_LOCK(cs_Shutdown, lockShutdown);
    if (!lockShutdown)
        return;

    /// Note: Shutdown() must be able to handle cases in which AppInit2() failed part of the way,
    /// for example if the data directory was found to be locked.
    /// Be sure that anything that writes files or flushes caches only does this if the respective
    /// module was initialized.
    RenameThread("bitcoin-shutoff");
    StopRPCThreads();
#ifdef ENABLE_WALLET
    if (pwalletMain)
        bitdbwrap.Flush(false);
#endif

#ifdef ENABLE_WALLET
    if (pwalletMain)
        bitdbwrap.Flush(true);
#endif
//#ifndef WIN32
    boost::filesystem::remove(GetPidFile());
//#endif
#ifdef ENABLE_WALLET
    delete pwalletMain;
    pwalletMain = NULL;
/* MCHN START */  
    if(pwalletTxsMain)
    {
        delete pwalletTxsMain;
        pwalletTxsMain=NULL;
    }
/* MCHN END */  
#endif
    if(pRelayManager)
    {
        delete pRelayManager;
        pRelayManager=NULL;        
    }
    
    if(pMultiChainFilterEngine)
    {
        delete pMultiChainFilterEngine;
        pMultiChainFilterEngine=NULL;        
    }
    
    if(pFilterEngine)
    {
        delete pFilterEngine;
        pFilterEngine=NULL;        
    }
    
    globalVerifyHandle.reset();
    ECC_Stop();
    LogPrintf("%s: done\n", __func__);
    fShutdownCompleted = true;
}

std::string HelpMessage_Cold()
{
    // When adding new options to the categories, please keep and ensure alphabetical ordering.
    string strUsage = _("Options:") + "\n";
    strUsage += "  -?                     " + _("This help message") + "\n";
    strUsage += "  -conf=<file>           " + strprintf(_("Specify configuration file (default: %s)"), "multichain.conf") + "\n";
#if !defined(WIN32)
        strUsage += "  -daemon                " + _("Run in the background as a daemon and accept commands") + "\n";
#endif
        
    strUsage += "  -datadir=<dir>         " + _("Specify data directory") + "\n";
#ifndef WIN32
    strUsage += "  -pid=<file>            " + strprintf(_("Specify pid file (default: %s)"), "multichain.pid") + "\n";
#endif
#if !defined(WIN32)
    strUsage += "  -shortoutput           " + _("Returns connection string if this node can start or default multichain address otherwise") + "\n";
#endif


#ifdef ENABLE_WALLET
    strUsage += "\n" + _("Wallet options:") + "\n";
    strUsage += "  -disablewallet         " + _("Do not load the wallet and disable wallet RPC calls") + "\n";
    strUsage += "  -keypool=<n>           " + strprintf(_("Set key pool size to <n> (default: %u)"), 1) + "\n";
    if (GetBoolArg("-help-debug", false))
        strUsage += "  -mintxfee=<amt>        " + strprintf(_("Fees (in BTC/Kb) smaller than this are considered zero fee for transaction creation (default: %s)"), FormatMoney(CWallet::minTxFee.GetFeePerK())) + "\n";
    strUsage += "  -salvagewallet         " + _("Attempt to recover private keys from a corrupt wallet.dat") + " " + _("on startup") + "\n";
    strUsage += "  -wallet=<file>         " + _("Specify wallet file (within data directory)") + " " + strprintf(_("(default: %s)"), "wallet.dat") + "\n";
/* MCHN START */    
    strUsage += "  -walletdbversion=2|3   " + _("Specify wallet version, 2 - Berkeley DB, 3 (default) - proprietary") + "\n";
/* MCHN END */    
#endif

    strUsage += "\n" + _("Debugging/Testing options:") + "\n";
    strUsage += "  -debug=<category>      " + strprintf(_("Output debugging information (default: %u, supplying <category> is optional)"), 0) + "\n";
    strUsage += "                         " + _("If <category> is not supplied, output all debugging information.") + "\n";
    strUsage += "                         " + _("<category> can be:");
    strUsage +=                                 " addrman, alert, bench, coindb, db, lock, rand, rpc, selectcoins, mempool, net"; // Don't translate these and qt below
    strUsage += ".\n";

    strUsage += "  -printtoconsole        " + _("Send trace/debug info to console instead of debug.log file") + "\n";
    strUsage += "  -logdir                " + _("Send trace/debug info to specified directory") + "\n";
    strUsage += "  -shrinkdebugfile       " + _("Shrink debug.log file on client startup (default: 1 when no -debug)") + "\n";

    strUsage += "\n" + _("RPC server options:") + "\n";
    strUsage += "  -server                " + _("Accept command line and JSON-RPC commands") + "\n";
    strUsage += "  -rest                  " + strprintf(_("Accept public REST requests (default: %u)"), 0) + "\n";
    strUsage += "  -rpcbind=<addr>        " + _("Bind to given address to listen for JSON-RPC connections. Use [host]:port notation for IPv6. This option can be specified multiple times (default: bind to all interfaces)") + "\n";
    strUsage += "  -rpcuser=<user>        " + _("Username for JSON-RPC connections") + "\n";
    strUsage += "  -rpcpassword=<pw>      " + _("Password for JSON-RPC connections") + "\n";
    strUsage += "  -rpcport=<port>        " + strprintf(_("Listen for JSON-RPC connections on <port> (default: %u or testnet: %u)"), 8332, 18332) + "\n";
    strUsage += "  -rpcallowip=<ip>       " + _("Allow JSON-RPC connections from specified source. Valid for <ip> are a single IP (e.g. 1.2.3.4), a network/netmask (e.g. 1.2.3.4/255.255.255.0) or a network/CIDR (e.g. 1.2.3.4/24).") + "\n";
    strUsage += "                         " + _("This option can be specified multiple times") + "\n";
    strUsage += "  -rpcallowmethod=<methods> " + _("If specified, allow only comma delimited list of JSON-RPC <methods>. This option can be specified multiple times.") + "\n";
    strUsage += "  -rpcthreads=<n>        " + strprintf(_("Set the number of threads to service RPC calls (default: %d)"), 4) + "\n";
    strUsage += "  -rpckeepalive          " + strprintf(_("RPC support for HTTP persistent connections (default: %d)"), 0) + "\n";

    strUsage += "\n" + _("RPC SSL options") + "\n";
    strUsage += "  -rpcssl                                  " + _("Use OpenSSL (https) for JSON-RPC connections") + "\n";
    strUsage += "  -rpcsslcertificatechainfile=<file.cert>  " + strprintf(_("Server certificate file (default: %s)"), "server.cert") + "\n";
    strUsage += "  -rpcsslprivatekeyfile=<file.pem>         " + strprintf(_("Server private key (default: %s)"), "server.pem") + "\n";
    strUsage += "  -rpcsslciphers=<ciphers>                 " + strprintf(_("Acceptable ciphers (default: %s)"), "TLSv1.2+HIGH:TLSv1+HIGH:!SSLv2:!aNULL:!eNULL:!3DES:@STRENGTH") + "\n";

    strUsage += "\n" + _("MultiChain runtime parameters") + "\n";    
    strUsage += "  -initprivkey=<privkey>                   " + _("Manually set the wallet default address and private key when running multichaind for the first time.") + "\n";
    strUsage += "  -hideknownopdrops=<n>                    " + strprintf(_("Remove recognized MultiChain OP_DROP metadata from the responses to JSON_RPC calls (default: %u)"), 0) + "\n";
    strUsage += "  -shrinkdebugfilesize=<n>                 " + _("If shrinkdebugfile is 1, this controls the size of the debug file. Whenever the debug.log file reaches over 5 times this number of bytes, it is reduced back down to this size.") + "\n";
    
    
    return strUsage;
}


/** Initialize bitcoin.
 *  @pre Parameters should be parsed and config file should be read.
 */
bool AppInit2_Cold(boost::thread_group& threadGroup,int OutputPipe)
{
/* MCHN START */    
    char bufOutput[4096];
    size_t bytes_written;
/* MCHN END */        
    // ********************************************************* Step 1: setup
#ifdef _MSC_VER
    // Turn off Microsoft heap dump noise
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_WARN, CreateFileA("NUL", GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, 0));
#endif
#if _MSC_VER >= 1400
    // Disable confusing "helpful" text message on abort, Ctrl-C
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
#ifdef WIN32
    // Enable Data Execution Prevention (DEP)
    // Minimum supported OS versions: WinXP SP3, WinVista >= SP1, Win Server 2008
    // A failure is non-critical and needs no further attention!
#ifndef PROCESS_DEP_ENABLE
    // We define this here, because GCCs winbase.h limits this to _WIN32_WINNT >= 0x0601 (Windows 7),
    // which is not correct. Can be removed, when GCCs winbase.h is fixed!
#define PROCESS_DEP_ENABLE 0x00000001
#endif
    typedef BOOL (WINAPI *PSETPROCDEPPOL)(DWORD);
    PSETPROCDEPPOL setProcDEPPol = (PSETPROCDEPPOL)GetProcAddress(GetModuleHandleA("Kernel32.dll"), "SetProcessDEPPolicy");
    if (setProcDEPPol != NULL) setProcDEPPol(PROCESS_DEP_ENABLE);

    // Initialize Windows Sockets
    WSADATA wsadata;
    int ret = WSAStartup(MAKEWORD(2,2), &wsadata);
    if (ret != NO_ERROR || LOBYTE(wsadata.wVersion ) != 2 || HIBYTE(wsadata.wVersion) != 2)
    {
        return InitError(strprintf("Error: Winsock library failed to start (WSAStartup returned error %d)", ret));
    }
#endif
#ifndef WIN32

    if (GetBoolArg("-sysperms", false)) {
#ifdef ENABLE_WALLET
        if (!GetBoolArg("-disablewallet", false))
            return InitError("Error: -sysperms is not allowed in combination with enabled wallet functionality");
#endif
    } else {
        umask(077);
    }

    fRequestShutdown=false;
    
    // Clean shutdown on SIGTERM
    struct sigaction sa;
    sa.sa_handler = HandleSIGTERM;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    // Reopen debug.log on SIGHUP
    struct sigaction sa_hup;
    sa_hup.sa_handler = HandleSIGHUP;
    sigemptyset(&sa_hup.sa_mask);
    sa_hup.sa_flags = 0;
    sigaction(SIGHUP, &sa_hup, NULL);

#if defined (__SVR4) && defined (__sun)
    // ignore SIGPIPE on Solaris
    signal(SIGPIPE, SIG_IGN);
#endif
    
#else
    
    SetConsoleCtrlHandler( (PHANDLER_ROUTINE) HandleSIGTERM, TRUE );

#endif

    // ********************************************************* Step 2: parameter interactions
    // Set this early so that parameter interactions go to console
    fPrintToConsole = GetBoolArg("-printtoconsole", false);
    fLogTimestamps = GetBoolArg("-logtimestamps", true);
/* MCHN START */    
    fLogTimeMillis = GetBoolArg("-logtimemillis", false);
/* MCHN END */    

    // ********************************************************* Step 3: parameter-to-internal-flags

    fDebug = !mapMultiArgs["-debug"].empty();
    // Special-case: if -debug=0/-nodebug is set, turn off debugging messages
    const vector<string>& categories = mapMultiArgs["-debug"];
    if (GetBoolArg("-nodebug", false) || find(categories.begin(), categories.end(), string("0")) != categories.end())
        fDebug = false;

    fServer = GetBoolArg("-server", false);
#ifdef ENABLE_WALLET
    bool fDisableWallet = GetBoolArg("-disablewallet", false);
#endif

#ifdef ENABLE_WALLET
    std::string strWalletFile = GetArg("-wallet", "wallet.dat");
#endif // ENABLE_WALLET

    // ********************************************************* Step 4: application initialization: dir lock, daemonize, pidfile, debug log

    // Initialize elliptic curve code
    ECC_Start();
    globalVerifyHandle.reset(new ECCVerifyHandle());

    // Sanity check
    if (!InitSanityCheck())
        return InitError(_("Initialization sanity check failed. MultiChain Core is shutting down."));

    std::string strDataDir = GetDataDir().string();
    LogPrint("mchn","mchn: Data directory: %s\n",strDataDir.c_str());
#ifdef ENABLE_WALLET
    // Wallet file must be a plain filename without a directory
    if (strWalletFile != boost::filesystem::basename(strWalletFile) + boost::filesystem::extension(strWalletFile))
        return InitError(strprintf(_("Wallet %s resides outside data directory %s"), strWalletFile, strDataDir));
#endif
    // Make sure only a single Bitcoin process is using the data directory.
    boost::filesystem::path pathLockFile = GetDataDir() / ".lock";
    FILE* file = fopen(pathLockFile.string().c_str(), "a"); // empty lock file; created if it doesn't exist.
    if (file) fclose(file);

/* MCHN START ifndef */
// In windows .lock file cannot be removed by remove_all if needed
// Anyway, we don't need this lock as we have permission db lock    
#ifndef WIN32
    static boost::interprocess::file_lock lock(pathLockFile.string().c_str());
    if (!lock.try_lock())
        return InitError(strprintf(_("Cannot obtain a lock on data directory %s. MultiChain Core is probably already running."), strDataDir));
#endif
/* MCHN END */
    
//#ifndef WIN32
    CreatePidFile(GetPidFile(), __US_GetPID());
//#endif
    if (GetBoolArg("-shrinkdebugfile", !fDebug))
        ShrinkDebugFile();
    LogPrintf("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
//    LogPrintf("Bitcoin version %s (%s)\n", FormatFullVersion(), CLIENT_DATE);
/* MCHN START */    
    if(mc_gState->m_NetworkParams->IsProtocolMultichain())
    {
        LogPrintf("MultiChain offline version build %s protocol %s (%s)\n", mc_BuildDescription(mc_gState->GetNumericVersion()), mc_gState->GetProtocolVersion(), CLIENT_DATE);
    }

/* MCHN END */    
    LogPrintf("Using OpenSSL version %s\n", SSLeay_version(SSLEAY_VERSION));
#ifdef ENABLE_WALLET
    WalletDBLogVersionString();
#endif
    if (!fLogTimestamps)
        LogPrintf("Startup time: %s\n", DateTimeStrFormat("%Y-%m-%d %H:%M:%S", GetTime()));
    LogPrintf("Default data directory %s\n", GetDefaultDataDir().string());
    LogPrintf("Using data directory %s\n", strDataDir);
    LogPrintf("Using config file %s\n", GetConfigFile().string());
    std::ostringstream strErrors;

    LogPrintf("Using %u threads for script verification\n", nScriptCheckThreads);
    
    int64_t nStart;

    // ********************************************************* Step 5: verify wallet database integrity
#ifdef ENABLE_WALLET
    int currentwalletdatversion=0;
    int64_t wallet_mode=GetArg("-walletdbversion",MC_TDB_WALLET_VERSION);
    mc_gState->m_WalletMode=MC_WMD_NONE;
    std::vector<CDBConstEnv::KeyValPair> salvagedData;
    bool wallet_upgrade=false;
    if (!fDisableWallet) {
        LogPrintf("Using wallet %s\n", strWalletFile);
        uiInterface.InitMessage(_("Verifying wallet..."));

        
        boost::filesystem::path pathWalletDat=GetDataDir() / strWalletFile;
        if (filesystem::exists(pathWalletDat))
        {
            currentwalletdatversion=GetWalletDatVersion(pathWalletDat.string());
            boost::filesystem::path pathWallet=GetDataDir() / "wallet";

            LogPrintf("Wallet file exists. WalletDBVersion: %d.\n", currentwalletdatversion);
            if( (currentwalletdatversion == 3) && (GetArg("-walletdbversion",MC_TDB_WALLET_VERSION) != 3) )
            {
                return InitError(_("Wallet downgrade is not allowed"));                                                        
            }
            if( (currentwalletdatversion == 2) && (GetArg("-walletdbversion",0) == 3) )
            {
                if(!boost::filesystem::exists(pathWallet))
                {
                    currentwalletdatversion=1;
                }
                else
                {
                    CDBWrapEnv env2;
                    if (!env2.Open(GetDataDir()))
                    {
                        return InitError(_("Error initializing wallet database environment for upgrade"));                                        
                    }                
                    bool allOK = env2.Salvage(strWalletFile, false, salvagedData);
                    if(!allOK)
                    {
                        return InitError(_("wallet.dat corrupt, cannot upgrade, you should repair it first.\n Run multichaind normally or with -salvagewallet flag"));                    
                    }

                    currentwalletdatversion=3;
                    wallet_upgrade=true;                
                }
            }
        }
        else
        {
            currentwalletdatversion=wallet_mode;
            LogPrintf("Wallet file doesn't exist. New file will be created with version %d.\n", currentwalletdatversion);
        }      
        switch(currentwalletdatversion)
        {
            case 3:
                mc_gState->m_WalletMode |= MC_WMD_FLAT_DAT_FILE;
                break;
            case 2:
                break;
            case 1:
                return InitError(strprintf("Wallet version 1 is not supported in this version of MultiChain. "
                        "To upgrade to version 2, run MultiChain 1.0: \n"
                        "multichaind %s -walletdbversion=2 -rescan\n",mc_gState->m_NetworkParams->Name()));                                        
            default:
                return InitError(_("Invalid wallet version, possible values 2, 3.\n"));                                                                    
        }
        
        if (!bitdbwrap.Open(GetDataDir()))
        {
            // try moving the database env out of the way
            boost::filesystem::path pathDatabase = GetDataDir() / "database";
            boost::filesystem::path pathDatabaseBak = GetDataDir() / strprintf("database.%d.bak", GetTime());
            try {
                boost::filesystem::rename(pathDatabase, pathDatabaseBak);
                LogPrintf("Moved old %s to %s. Retrying.\n", pathDatabase.string(), pathDatabaseBak.string());
            } catch(boost::filesystem::filesystem_error &error) {
                 // failure is ok (well, not really, but it's not worse than what we started with)
            }

            // try again
            if (!bitdbwrap.Open(GetDataDir())) {
                // if it still fails, it probably means we can't even create the database env
                string msg = strprintf(_("Error initializing wallet database environment %s!"), strDataDir);
                return InitError(msg);
            }
        }

        
        if(wallet_upgrade)
        {
            LogPrintf("Wallet file will be upgraded to version %d.\n", currentwalletdatversion);
            if(!bitdbwrap.Recover(strWalletFile,salvagedData))
            {
                return InitError(_("Couldn't upgrade wallet.dat"));                                    
            }
        }
        
        if (filesystem::exists(pathWalletDat))
        {
            currentwalletdatversion=GetWalletDatVersion(pathWalletDat.string());
        }
        else
        {
            currentwalletdatversion=wallet_mode;
        }      
        
        if (GetBoolArg("-salvagewallet", false))
        {
            // Recover readable keypairs:
//            if (!CWalletDB::Recover(bitdbwrap, strWalletFile, true))
            if(!WalletDBRecover(bitdbwrap,strWalletFile,true))
                return false;
            sprintf(bufOutput,"\nTo work properly with salvaged addresses, you have to call importaddress API and restart MultiChain with -rescan\n\n");
            bytes_written=write(OutputPipe,bufOutput,strlen(bufOutput));                
        }

        if (filesystem::exists(GetDataDir() / strWalletFile))
        {
//            CDBConstEnv::VerifyResult r = bitdbwrap.Verify(strWalletFile, CWalletDB::Recover);
            CDBConstEnv::VerifyResult r = bitdbwrap.Verify(strWalletFile);
            if(r != CDBConstEnv::VERIFY_OK)
            {
                r=WalletDBRecover(bitdbwrap,strWalletFile) ? CDBConstEnv::RECOVER_OK : CDBConstEnv::RECOVER_FAIL;
            }
            if (r == CDBConstEnv::RECOVER_OK)
            {
                string msg = strprintf(_("Warning: wallet.dat corrupt, data salvaged!"
                                         " Original wallet.dat saved as wallet.{timestamp}.bak in %s; if"
                                         " your balance or transactions are incorrect you should"
                                         " restore from a backup."), strDataDir);
                InitWarning(msg);
            }
            if (r == CDBConstEnv::RECOVER_FAIL)
                return InitError(_("wallet.dat corrupt, salvage failed"));
        }
        else
        {
            bitdbwrap.SetSeekDBName(strWalletFile);
        }        
    } // (!fDisableWallet)

/* MCHN START*/    
    uiInterface.InitMessage(_("Initializing multichain..."));

    if(GetBoolArg("-offline",false))
    {
        if(mc_gState->m_NetworkParams->m_Status != MC_PRM_STATUS_VALID)
        {
            char fileName[MC_DCT_DB_MAX_PATH];
            mc_GetFullFileName(mc_gState->m_Params->NetworkName(),"params", ".dat",MC_FOM_RELATIVE_TO_DATADIR,fileName);
            string seed_error=strprintf("Couldn't retrieve blockchain parameters from the seed node in offline mode.\n"
                        "The file %s must be copied manually from an existing node into empty blockchain directory.\n",                
                    fileName);
            return InitError(seed_error);                        
        }        
    }    

    
    bool fFirstRunForBuild;
    string init_privkey=GetArg("-initprivkey","");
    
    pwalletMain=NULL;
    
    string seed_error="";
    bool zap_wallet_txs=false;
    bool new_wallet_txs=false;
    
    if(mc_gState->m_NetworkParams->m_Status == MC_PRM_STATUS_VALID)
    {
        LogPrintf("mchn: Parameter set is valid - initializing blockchain parameters...\n");
        mc_gState->m_NetworkParams->SetGlobals();
        InitializeMultiChainParams();        

        if(GetBoolArg("-reindex", false))
        {
            mc_RemoveDir(mc_gState->m_Params->NetworkName(),"entities.db");
            mc_RemoveFile(mc_gState->m_Params->NetworkName(),"entities",".dat",MC_FOM_RELATIVE_TO_DATADIR);
        }
        
        mc_gState->m_Assets= new mc_AssetDB;
        if(mc_gState->m_Assets->Initialize(mc_gState->m_Params->NetworkName(),0))                                
        {
            seed_error=strprintf("ERROR: Couldn't initialize asset database for blockchain %s. Please restart multichaind with reindex=1.\n",mc_gState->m_Params->NetworkName());
            return InitError(_(seed_error.c_str()));        
        }
        
        wallet_mode=GetArg("-walletdbversion",0);
        if(wallet_mode == 0)
        {
            mc_gState->m_WalletMode=MC_WMD_AUTO;
        }
        if(wallet_mode == 3)
        {
            mc_gState->m_WalletMode=MC_WMD_TXS | MC_WMD_ADDRESS_TXS | MC_WMD_FLAT_DAT_FILE; 
        }
        if(wallet_mode == 2)
        {
            mc_gState->m_WalletMode=MC_WMD_TXS | MC_WMD_ADDRESS_TXS; 
        }
        if(wallet_mode == 1)
        {
            mc_gState->m_WalletMode=MC_WMD_NONE;
            zap_wallet_txs=false;
        }
        if(wallet_mode == -1)
        {
            mc_gState->m_WalletMode=MC_WMD_TXS | MC_WMD_ADDRESS_TXS | MC_WMD_MAP_TXS;            
            zap_wallet_txs=false;
        }
 

        vector <mc_TxEntity> vSubscribedEntities;
        if(GetBoolArg("-reindex", false) || GetBoolArg("-rescan", false))
        {
            pwalletTxsMain=new mc_WalletTxs;
            if(pwalletTxsMain->Initialize(mc_gState->m_NetworkParams->Name(),MC_WMD_TXS | MC_WMD_ADDRESS_TXS) == MC_ERR_NOERROR)
            {
                mc_Buffer *entity_list;
                entity_list=pwalletTxsMain->GetEntityList();
                for(int e=0;e<entity_list->GetCount();e++)
                {
                    mc_TxEntityStat *stat;
                    stat=(mc_TxEntityStat *)entity_list->GetRow(e);
                    switch(stat->m_Entity.m_EntityType & MC_TET_TYPE_MASK)
                    {
                        case MC_TET_PUBKEY_ADDRESS:
                        case MC_TET_SCRIPT_ADDRESS:
                        case MC_TET_STREAM:
                        case MC_TET_STREAM_KEY:
                        case MC_TET_STREAM_PUBLISHER:
                            vSubscribedEntities.push_back(stat->m_Entity);
                            break;
                    }
                }
                __US_Sleep(1000);
            }
            pwalletTxsMain->Destroy();
            delete pwalletTxsMain;            

            mc_RemoveDir(mc_gState->m_Params->NetworkName(),"wallet");            
            zap_wallet_txs=true;
        }


        
        pwalletTxsMain=new mc_WalletTxs;
        mc_TxEntity entity;
        boost::filesystem::path pathWallet=GetDataDir() / "wallet";
        if(mc_gState->m_WalletMode == MC_WMD_NONE)
        {
            if(boost::filesystem::exists(pathWallet))
            {
                return InitError(strprintf("Wallet was created in version 2 or higher. To switch to version 1, with worse performance and scalability, run: \nmultichaind %s -walletdbversion=1 -rescan\n",mc_gState->m_NetworkParams->Name()));                                        
            }
        }
        else
        {
            if(!boost::filesystem::exists(pathWallet))
            {
                if((mc_gState->m_Permissions->m_Block >= 0) && !GetBoolArg("-rescan", false))
                {
                    if(mc_gState->m_WalletMode == MC_WMD_AUTO)
                    {
                        mc_gState->m_WalletMode=MC_WMD_NONE;
                    }
                    if(mc_gState->m_WalletMode != MC_WMD_NONE)
                    {
                        return InitError(strprintf("Wallet was created in version 1. To switch to version %d, with better performance and scalability, run: \nmultichaind %s -walletdbversion=%d -rescan\n",
                                MC_TDB_WALLET_VERSION,mc_gState->m_NetworkParams->Name(),MC_TDB_WALLET_VERSION));                                        
                    }                    
                }
                else
                {
                    new_wallet_txs=true;
                    if(mc_gState->m_WalletMode == MC_WMD_AUTO)
                    {                        
                        mc_gState->m_WalletMode = MC_WMD_TXS | MC_WMD_ADDRESS_TXS;
                        wallet_mode=MC_TDB_WALLET_VERSION;
                        if(wallet_mode > 2)
                        {
                            mc_gState->m_WalletMode |= MC_WMD_FLAT_DAT_FILE;
                        }
                    }
                }
            }
            
            if(mc_gState->m_WalletMode != MC_WMD_NONE)
            {
                boost::filesystem::create_directories(pathWallet);
                if(LogAcceptCategory("walletdump"))
                {
                    mc_gState->m_WalletMode |= MC_WMD_DEBUG;
                }
                string autosubscribe=GetArg("-autosubscribe","none");
                
                if(autosubscribe=="streams")
                {
                    mc_gState->m_WalletMode |= MC_WMD_AUTOSUBSCRIBE_STREAMS;
                }
                if(autosubscribe=="assets")
                {
                    mc_gState->m_WalletMode |= MC_WMD_AUTOSUBSCRIBE_ASSETS;
                }
                if( (autosubscribe=="assets,streams") || (autosubscribe=="streams,assets"))
                {
                    mc_gState->m_WalletMode |= MC_WMD_AUTOSUBSCRIBE_STREAMS;
                    mc_gState->m_WalletMode |= MC_WMD_AUTOSUBSCRIBE_ASSETS;
                }                

                if(pwalletTxsMain->Initialize(mc_gState->m_NetworkParams->Name(),mc_gState->m_WalletMode))
                {
                    return InitError("Wallet tx database corrupted. Please restart multichaind with -rescan\n");                        
                }

                if(mc_gState->m_WalletMode & MC_WMD_AUTO)
                {
                    mc_gState->m_WalletMode=pwalletTxsMain->m_Database->m_DBStat.m_InitMode;
                    wallet_mode=pwalletTxsMain->m_Database->m_DBStat.m_WalletVersion;
                }
                if(wallet_mode == -1)
                {
                    wallet_mode=pwalletTxsMain->m_Database->m_DBStat.m_WalletVersion;
                }

                if( (pwalletTxsMain->m_Database->m_DBStat.m_WalletVersion == 2) && (wallet_mode == 3) )
                {
                    if(wallet_upgrade)
                    {
                        if(pwalletTxsMain->UpdateMode(MC_WMD_FLAT_DAT_FILE))
                        {
                            return InitError(_("Couldn't update wallet mode"));                                    
                        }                        
                    }
                }
                
                if((pwalletTxsMain->m_Database->m_DBStat.m_WalletVersion) != wallet_mode)
                {
                    return InitError(strprintf("Wallet tx database was created with different wallet version (%d). Please restart multichaind with reindex=1 \n",pwalletTxsMain->m_Database->m_DBStat.m_WalletVersion));                        
                }        

                if((pwalletTxsMain->m_Database->m_DBStat.m_InitMode & MC_WMD_MODE_MASK) != (mc_gState->m_WalletMode & MC_WMD_MODE_MASK))
                {
                    return InitError(strprintf("Wallet tx database was created in different mode (%08X). Please restart multichaind with reindex=1 \n",pwalletTxsMain->m_Database->m_DBStat.m_InitMode));                        
                }        
            }
        }
        LogPrint("mcblockperf","mchn-block-perf: Wallet initialization completed (%s)\n",(mc_gState->m_WalletMode & MC_WMD_TXS) ? pwalletTxsMain->Summary() : "");
        LogPrintf("Wallet mode: %08X\n",mc_gState->m_WalletMode);
        if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)
        {
            if(GetBoolArg("-zapwallettxes", false) && !GetBoolArg("-reindex", false))
            {
                return InitError(_("-zapwallettxes is not supported with scalable wallet.\n"));                                        
            }
        }

        if(pwalletMain == NULL)                                                 // Opening wallet only after multichain parameters were initizalized
        {
            pwalletMain = new CWallet(strWalletFile);
            DBErrors nLoadWalletRetForBuild = pwalletMain->LoadWallet(fFirstRunForBuild);

            if (nLoadWalletRetForBuild != DB_LOAD_OK)                                   // MCHN-TODO wallet recovery
            {
                if (GetBoolArg("-salvagewallet", false))
                {
                    return InitError(_("wallet.dat corrupted. Please remove it and restart."));            
                }
                return InitError(_("wallet.dat corrupted. Please try running MultiChain with -salvagewallet."));                            
            }

            if(!pwalletMain->vchDefaultKey.IsValid())
            {
                if(init_privkey.size())
                {
                    LogPrintf("mchn: Default key is specified using -initprivkey - not created\n");                
                }
                else
                {
                    LogPrintf("mchn: Default key is not found - creating new... \n");
                    // Create new keyUser and set as default key
        //            RandAddSeedPerfmon();

                    pwalletMain->SetMinVersion(FEATURE_LATEST); // permanently upgrade the wallet immediately
                    CPubKey newDefaultKey;
                    if (pwalletMain->GetKeyFromPool(newDefaultKey)) {
                        pwalletMain->SetDefaultKey(newDefaultKey);
                    }
                }
            }
            else
            {
                if(init_privkey.size())
                {
                    LogPrintf("mchn: Wallet already has default key, -initprivkey is ignored\n");                
                    if(!GetBoolArg("-shortoutput", false))
                    {    
                        sprintf(bufOutput,"Wallet already has default key, -initprivkey is ignored\n\n");
                        bytes_written=write(OutputPipe,bufOutput,strlen(bufOutput));
                    }
                    init_privkey="";
                }            
            }            
        }
        
        if(pwalletMain)
        {
            if(init_privkey.size())
            {
                string init_privkey_error=pwalletMain->SetDefaultKeyIfInvalid(init_privkey);
                if(init_privkey_error.size())
                {
                    return InitError(strprintf("Cannot set initial private key: %s",init_privkey_error));                            
                }
                init_privkey="";
            }
            
            if(fFirstRunForBuild || (pwalletMain->mapAddressBook.size() == 0))
            {
                if (!pwalletMain->SetAddressBook(pwalletMain->vchDefaultKey.GetID(), "", "receive"))
                    strErrors << _("Cannot write default address") << "\n";
            }

            if(new_wallet_txs)
            {
                entity.Zero();
                entity.m_EntityType=MC_TET_WALLET_ALL | MC_TET_CHAINPOS;
                pwalletTxsMain->AddEntity(&entity,0);
                entity.m_EntityType=MC_TET_WALLET_ALL | MC_TET_TIMERECEIVED;
                pwalletTxsMain->AddEntity(&entity,0);        
                entity.m_EntityType=MC_TET_WALLET_SPENDABLE | MC_TET_CHAINPOS;
                pwalletTxsMain->AddEntity(&entity,0);
                entity.m_EntityType=MC_TET_WALLET_SPENDABLE | MC_TET_TIMERECEIVED;
                pwalletTxsMain->AddEntity(&entity,0);

                for(int e=0;e<(int)vSubscribedEntities.size();e++)
                {
                    pwalletTxsMain->AddEntity(&(vSubscribedEntities[e]),MC_EFL_NOT_IN_SYNC);                    
                }                                
                
                mc_TxEntityStat entstat;
                BOOST_FOREACH(const PAIRTYPE(CBitcoinAddress, CAddressBookData)& item, pwalletMain->mapAddressBook)
                {
                    const CBitcoinAddress& address = item.first;
                    CTxDestination addressRet=address.Get();        
                    const CKeyID *lpKeyID=boost::get<CKeyID> (&addressRet);
                    const CScriptID *lpScriptID=boost::get<CScriptID> (&addressRet);
                    uint32_t flags=0;
                    if(item.second.purpose == "license")
                    {
                        flags |= MC_EFL_NOT_IN_LISTS;
                    }

                    entstat.Zero();
                    if(lpKeyID)
                    {
                        memcpy(entstat.m_Entity.m_EntityID,lpKeyID,MC_TDB_ENTITY_ID_SIZE);
                        entstat.m_Entity.m_EntityType=MC_TET_PUBKEY_ADDRESS | MC_TET_CHAINPOS;
                        if(!pwalletTxsMain->FindEntity(&entstat))
                        {
                            pwalletTxsMain->AddEntity(&(entstat.m_Entity),0);
                            entstat.m_Entity.m_EntityType=MC_TET_PUBKEY_ADDRESS | MC_TET_TIMERECEIVED;
                            pwalletTxsMain->AddEntity(&(entstat.m_Entity),0);
                        }
                    }
                        
                    if(lpScriptID)
                    {
                        memcpy(entstat.m_Entity.m_EntityID,lpScriptID,MC_TDB_ENTITY_ID_SIZE);
                        entstat.m_Entity.m_EntityType=MC_TET_SCRIPT_ADDRESS | MC_TET_CHAINPOS;
                        if(!pwalletTxsMain->FindEntity(&entstat))
                        {
                            pwalletTxsMain->AddEntity(&(entstat.m_Entity),0);
                            entstat.m_Entity.m_EntityType=MC_TET_SCRIPT_ADDRESS | MC_TET_TIMERECEIVED;
                            pwalletTxsMain->AddEntity(&(entstat.m_Entity),0);
                        }
                    }
                }

            }
            
            CPubKey pkey;
            if(mc_gState->m_NetworkParams->IsProtocolMultichain())
            {
                if(pwalletMain->GetKeyFromAddressBook(pkey,MC_PTP_CONNECT))
                {
                    LogPrint("mchn","mchn: Default connect  address: %s\n",CBitcoinAddress(pkey.GetID()).ToString().c_str());                
                }
                if(pwalletMain->GetKeyFromAddressBook(pkey,MC_PTP_SEND))
                {
                    LogPrint("mchn","mchn: Default send     address: %s\n",CBitcoinAddress(pkey.GetID()).ToString().c_str());                
                }
                if(pwalletMain->GetKeyFromAddressBook(pkey,MC_PTP_RECEIVE))
                {
                    LogPrint("mchn","mchn: Default receive  address: %s\n",CBitcoinAddress(pkey.GetID()).ToString().c_str());                
                }
                if(pwalletMain->GetKeyFromAddressBook(pkey,MC_PTP_CREATE))
                {
                    LogPrint("mchn","mchn: Default create   address: %s\n",CBitcoinAddress(pkey.GetID()).ToString().c_str());                
                }
                if(pwalletMain->GetKeyFromAddressBook(pkey,MC_PTP_ISSUE))
                {
                    LogPrint("mchn","mchn: Default issue    address: %s\n",CBitcoinAddress(pkey.GetID()).ToString().c_str());                
                }
                if(pwalletMain->GetKeyFromAddressBook(pkey,MC_PTP_MINE))
                {
                    LogPrint("mchn","mchn: Default mine     address: %s\n",CBitcoinAddress(pkey.GetID()).ToString().c_str());                
                }
                if(pwalletMain->GetKeyFromAddressBook(pkey,MC_PTP_ADMIN))
                {
                    LogPrint("mchn","mchn: Default admin    address: %s\n",CBitcoinAddress(pkey.GetID()).ToString().c_str());                
                }
                if(pwalletMain->GetKeyFromAddressBook(pkey,MC_PTP_ACTIVATE))
                {
                    LogPrint("mchn","mchn: Default activate address: %s\n",CBitcoinAddress(pkey.GetID()).ToString().c_str());                
                }
            }
            else
            {
                if(pwalletMain->GetKeyFromAddressBook(pkey,0))
                {
                    LogPrint("mchn","mchn: Default address: %s\n",CBitcoinAddress(pkey.GetID()).ToString().c_str());                
                }                
            }
            delete pwalletMain;
            pwalletMain=NULL;
        }
        
        if(GetBoolArg("-reindex", false))
        {
            mc_gState->m_Assets->RollBack(-1);
            mc_gState->m_Permissions->RollBack(-1);
        }
    }
    
    pwalletMain=NULL;

    string rpc_threads_error="";
    if (fServer)
    {
        uiInterface.InitMessage.connect(SetRPCWarmupStatus);
        StartRPCThreads(rpc_threads_error);
    }
/* MCHN END*/        
    

#endif // ENABLE_WALLET
    // ********************************************************* Step 6: network initialization
/*
    int version=mc_gState->m_NetworkParams->GetInt64Param("protocolversion");
    LogPrintf("MultiChain protocol version: %d\n",version);
    if(version != mc_gState->GetProtocolVersion())
    {
        if(!GetBoolArg("-shortoutput", false))
        {    
            sprintf(bufOutput,"Protocol version %d\n\n",version);            
            bytes_written=write(OutputPipe,bufOutput,strlen(bufOutput));
        }
    }
*/    
/* MCHN END */    

    std::string strResult;
    
    pFilterEngine=new mc_FilterEngine();
    if (pFilterEngine->Initialize(strResult) != MC_ERR_NOERROR)
    {
        return InitError(strprintf(_("Couldn't initialize filter engine: '%s'"), strResult));
    }
    
    pMultiChainFilterEngine=new mc_MultiChainFilterEngine;
    if(pMultiChainFilterEngine->Initialize())
    {
        return InitError(_("Couldn't initialize filter engine."));        
    }
    
    // ********************************************************* Step 7: load block chain

    fReindex = GetBoolArg("-reindex", false);

    // Upgrading to 0.8; hard-link the old blknnnn.dat files into /blocks/
    filesystem::path blocksDir = GetDataDir() / "blocks";
    if (!filesystem::exists(blocksDir))
    {
        filesystem::create_directories(blocksDir);
        bool linked = false;
        for (unsigned int i = 1; i < 10000; i++) {
            filesystem::path source = GetDataDir() / strprintf("blk%04u.dat", i);
            if (!filesystem::exists(source)) break;
            filesystem::path dest = blocksDir / strprintf("blk%05u.dat", i-1);
            try {
                filesystem::create_hard_link(source, dest);
                LogPrintf("Hardlinked %s -> %s\n", source.string(), dest.string());
                linked = true;
            } catch (filesystem::filesystem_error & e) {
                // Note: hardlink creation failing is not a disaster, it just means
                // blocks will get re-downloaded from peers.
                LogPrintf("Error hardlinking blk%04u.dat : %s\n", i, e.what());
                break;
            }
        }
        if (linked)
        {
            fReindex = true;
        }
    }

    // cache size calculations
    size_t nTotalCache = (GetArg("-dbcache", nDefaultDbCache) << 20);
    if (nTotalCache < (nMinDbCache << 20))
        nTotalCache = (nMinDbCache << 20); // total cache cannot be less than nMinDbCache
    else if (nTotalCache > (nMaxDbCache << 20))
        nTotalCache = (nMaxDbCache << 20); // total cache cannot be greater than nMaxDbCache
    size_t nBlockTreeDBCache = nTotalCache / 8;
/* MCHN START */    
/* Default was false */    
    if (nBlockTreeDBCache > (1 << 21) && !GetBoolArg("-txindex", true))
        nBlockTreeDBCache = (1 << 21); // block tree db cache shouldn't be larger than 2 MiB
/* MCHN END */    
    nTotalCache -= nBlockTreeDBCache;
    size_t nCoinDBCache = nTotalCache / 2; // use half of the remaining cache for coindb cache
    nTotalCache -= nCoinDBCache;
    nCoinCacheSize = nTotalCache / 300; // coins in memory require around 300 bytes

    bool fLoaded = false;
    while (!fLoaded) {
        bool fReset = fReindex;
        std::string strLoadError;

        uiInterface.InitMessage(_("Loading block index..."));

        nStart = GetTimeMillis();
        do {
            try {
                UnloadBlockIndex();
                delete pcoinsTip;
                delete pcoinsdbview;
                delete pcoinscatcher;
                delete pblocktree;

                pblocktree = new CBlockTreeDB(nBlockTreeDBCache, false, fReindex);
                pcoinsdbview = new CCoinsViewDB(nCoinDBCache, false, fReindex);
                pcoinscatcher = new CCoinsViewErrorCatcher(pcoinsdbview);
                pcoinsTip = new CCoinsViewCache(pcoinscatcher);

                if (fReindex)
                    pblocktree->WriteReindexing(true);

                if(mc_gState->m_WalletMode & MC_WMD_TXS)
                {
                    bool fFirstRunForLoadChain = true;
                    pwalletMain = new CWallet(strWalletFile);
                    DBErrors nLoadWalletRetForLoadChain = pwalletMain->LoadWallet(fFirstRunForLoadChain);
                    if (nLoadWalletRetForLoadChain != DB_LOAD_OK)
                    {
                        strLoadError = _("Error loading wallet before loading chain");
                        break;
                    }
                    pwalletTxsMain->BindWallet(pwalletMain);
                }
    
                if (!LoadBlockIndex(strLoadError)) {
                    strLoadError = _("Error loading block database");
                    break;
                }

                if(pwalletMain)
                {
                    pwalletTxsMain->BindWallet(NULL);
                    delete pwalletMain;
                    pwalletMain=NULL;
                }
                
                // If the loaded chain has a wrong genesis, bail out immediately
                // (we're likely using a testnet datadir, or the other way around).
                if (!mapBlockIndex.empty() && mapBlockIndex.count(Params().HashGenesisBlock()) == 0)
                    return InitError(_("This blockchain was created with a different params.dat file, please restore the original."));
//                    return InitError(_("Incorrect or no genesis block found. Wrong datadir for network?"));

                // Initialize the block index (no-op if non-empty database was already loaded)
                if (!InitBlockIndex()) {
                    strLoadError = _("Error initializing block database");
                    break;
                }

                // Check for changed -txindex state
/* MCHN START */    
/* Default was false */    
                if (fTxIndex != GetBoolArg("-txindex", true)) {
                    strLoadError = _("You need to rebuild the database using -reindex to change -txindex");
                    break;
                }
/* MCHN END */    

                uiInterface.InitMessage(_("Verifying blocks..."));
            } catch(std::exception &e) {
                if (fDebug) LogPrintf("%s\n", e.what());
                strLoadError = _("Error opening block database");
                break;
            }

            fLoaded = true;
        } while(false);

        if (!fLoaded) {
            // first suggest a reindex
            if (!fReset) {
/* MCHN START */                
                bool fRet = uiInterface.ThreadSafeMessageBox(
                    strLoadError + ".\n\n" + _("Please restart multichaind with reindex=1."),
                    "", CClientUIInterface::BTN_ABORT);
                
/* MCHN END */                
                if (fRet) {
                    fReindex = true;
                    fRequestShutdown = false;
                } else {
                    LogPrintf("Aborted block database rebuild. Exiting.\n");
                    return false;
                }
            } else {
                return InitError(strLoadError);
            }
        }
    }
    

    // As LoadBlockIndex can take several minutes, it's possible the user
    // requested to kill the GUI during the last operation. If so, exit.
    // As the program has not fully started yet, Shutdown() is possibly overkill.
    if (fRequestShutdown)
    {
        LogPrintf("Shutdown requested. Exiting.\n");
        return false;
    }
    LogPrintf(" block index %15dms\n", GetTimeMillis() - nStart);
    
    if(mapMultiArgs.count("-rpcallowip") == 0)
    {
        if(!GetBoolArg("-shortoutput", false))
        {    
            sprintf(bufOutput,"Listening for API requests on port %d (local only - see rpcallowip setting)\n\n",(int)GetArg("-rpcport", BaseParams().RPCPort()));                            
            bytes_written=write(OutputPipe,bufOutput,strlen(bufOutput));        
        }
    }
    if(rpc_threads_error.size())
    {
        if(!GetBoolArg("-shortoutput", false))
        {    
            sprintf(bufOutput,"%s\n",rpc_threads_error.c_str());                            
            bytes_written=write(OutputPipe,bufOutput,strlen(bufOutput));        
        }            
    }
    

    int version=mc_gState->m_NetworkParams->ProtocolVersion();
    LogPrintf("MultiChain protocol version: %d\n",version);
    if(version != mc_gState->GetProtocolVersion())
    {
        if(!GetBoolArg("-shortoutput", false))
        {    
            int original_protocol_version=(int)mc_gState->m_NetworkParams->GetInt64Param("protocolversion");

            if(version != original_protocol_version)
            {
                sprintf(bufOutput,"Chain running protocol version %d (chain created with %d)\n\n",version,original_protocol_version);                            
            }
            else
            {
                sprintf(bufOutput,"Chain running protocol version %d\n\n",version);            
            }
            bytes_written=write(OutputPipe,bufOutput,strlen(bufOutput));
        }
    }
    
    // ********************************************************* Step 8: load wallet
#ifdef ENABLE_WALLET
    if (fDisableWallet) {
        pwalletMain = NULL;
        LogPrintf("Wallet disabled!\n");
    } else {

        // needed to restore wallet transaction meta data after -zapwallettxes
/*        
        std::vector<CWalletTx> vWtx;

        if (GetBoolArg("-zapwallettxes", false) || zap_wallet_txs) {
            uiInterface.InitMessage(_("Zapping all transactions from wallet..."));

            pwalletMain = new CWallet(strWalletFile);
            DBErrors nZapWalletRet = pwalletMain->ZapWalletTx(vWtx);
            if (nZapWalletRet != DB_LOAD_OK) {
                uiInterface.InitMessage(_("Error loading wallet.dat: Wallet corrupted"));
                return false;
            }

            delete pwalletMain;
            pwalletMain = NULL;
        }
*/
        
        uiInterface.InitMessage(_("Loading wallet..."));

        nStart = GetTimeMillis();
        bool fFirstRun = true;
        pwalletMain = new CWallet(strWalletFile);
        DBErrors nLoadWalletRet = pwalletMain->LoadWallet(fFirstRun);
        if (nLoadWalletRet != DB_LOAD_OK)
        {
            if (nLoadWalletRet == DB_CORRUPT)
                strErrors << _("Error loading wallet.dat: Wallet corrupted") << "\n";
            else if (nLoadWalletRet == DB_NONCRITICAL_ERROR)
            {
                string msg(_("Warning: error reading wallet.dat! All keys read correctly, but transaction data"
                             " or address book entries might be missing or incorrect."));
                InitWarning(msg);
            }
            else if (nLoadWalletRet == DB_TOO_NEW)                              // MCHN
                strErrors << _("Error loading wallet.dat: Wallet requires newer version of MultiChain Core") << "\n";
            else if (nLoadWalletRet == DB_NEED_REWRITE)                         // MCHN
            {
                strErrors << _("Wallet needed to be rewritten: restart MultiChain Core to complete") << "\n";
                LogPrintf("%s", strErrors.str());
                return InitError(strErrors.str());
            }
            else
                strErrors << _("Error loading wallet.dat") << "\n";
        }

        if (GetBoolArg("-upgradewallet", fFirstRun))
        {
            int nMaxVersion = GetArg("-upgradewallet", 0);
            if (nMaxVersion == 0) // the -upgradewallet without argument case
            {
                LogPrintf("Performing wallet upgrade to %i\n", FEATURE_LATEST);
                nMaxVersion = CLIENT_VERSION;
                pwalletMain->SetMinVersion(FEATURE_LATEST); // permanently upgrade the wallet immediately
            }
            else
                LogPrintf("Allowing wallet upgrade up to %i\n", nMaxVersion);
            if (nMaxVersion < pwalletMain->GetVersion())
                strErrors << _("Cannot downgrade wallet") << "\n";
            pwalletMain->SetMaxVersion(nMaxVersion);
        }

        if (fFirstRun)
        {
            // Create new keyUser and set as default key
//            RandAddSeedPerfmon();

            CPubKey newDefaultKey;
            if (pwalletMain->GetKeyFromPool(newDefaultKey)) {
                pwalletMain->SetDefaultKey(newDefaultKey);
//            printf("Writing default address on open \n");
                if (!pwalletMain->SetAddressBook(pwalletMain->vchDefaultKey.GetID(), "", "receive"))
                    strErrors << _("Cannot write default address") << "\n";
            }

            pwalletMain->SetBestChain(chainActive.GetLocator());
        }
        
        LogPrintf("%s", strErrors.str());
        LogPrintf(" wallet      %15dms\n", GetTimeMillis() - nStart);

/*        
        RegisterValidationInterface(pwalletMain);

        CBlockIndex *pindexRescan = chainActive.Tip();
        if (GetBoolArg("-rescan", false))
            pindexRescan = chainActive.Genesis();
        else
        {
            if( (mc_gState->m_WalletMode & MC_WMD_TXS) == 0 )
            {            
                CWalletDB walletdb(strWalletFile);
                CBlockLocator locator;
                if (walletdb.ReadBestBlock(locator))
                    pindexRescan = FindForkInGlobalIndex(chainActive, locator);
                else
                    pindexRescan = chainActive.Genesis();
            }
        }
 */ 
/* MCHN START */  
        pwalletTxsMain->BindWallet(pwalletMain);
/* MCHN END */        
/*        
        if (chainActive.Tip() && chainActive.Tip() != pindexRescan)
        {
            uiInterface.InitMessage(_("Rescanning..."));
            LogPrintf("Rescanning last %i blocks (from block %i)...\n", chainActive.Height() - pindexRescan->nHeight, pindexRescan->nHeight);
            nStart = GetTimeMillis();
            pwalletMain->ScanForWalletTransactions(pindexRescan, true, false);
            LogPrintf(" rescan      %15dms\n", GetTimeMillis() - nStart);
            pwalletMain->SetBestChain(chainActive.GetLocator());
            nWalletDBUpdated++;

            // Restore wallet transaction metadata after -zapwallettxes=1
            if (GetBoolArg("-zapwallettxes", false) && GetArg("-zapwallettxes", "1") != "2")
            {
                BOOST_FOREACH(const CWalletTx& wtxOld, vWtx)
                {
                    uint256 hash = wtxOld.GetHash();
                    std::map<uint256, CWalletTx>::iterator mi = pwalletMain->mapWallet.find(hash);
                    if (mi != pwalletMain->mapWallet.end())
                    {
                        const CWalletTx* copyFrom = &wtxOld;
                        CWalletTx* copyTo = &mi->second;
                        copyTo->mapValue = copyFrom->mapValue;
                        copyTo->vOrderForm = copyFrom->vOrderForm;
                        copyTo->nTimeReceived = copyFrom->nTimeReceived;
                        copyTo->nTimeSmart = copyFrom->nTimeSmart;
                        copyTo->fFromMe = copyFrom->fFromMe;
                        copyTo->strFromAccount = copyFrom->strFromAccount;
                        copyTo->nOrderPos = copyFrom->nOrderPos;
                        copyTo->WriteToDisk();
                    }
                }
            }
        }
 */ 
/* MCHN START */  
        pwalletMain->lpWalletTxs=pwalletTxsMain;
        pwalletMain->InitializeUnspentList();
    
        
/* MCHN END */        
    } // (!fDisableWallet)
#else // ENABLE_WALLET
    LogPrintf("No wallet compiled in!\n");
#endif // !ENABLE_WALLET
    // ********************************************************* Step 9: import blocks

    // scan for better chains in the block chain database, that are not yet connected in the active best chain
    CValidationState state;
    if (!ActivateBestChain(state))
        strErrors << "Failed to connect best block";

    // ********************************************************* Step 10: start node

    if (!CheckDiskSpace())
        return false;

    if (!strErrors.str().empty())
        return InitError(strErrors.str());


    //// debug print
    LogPrintf("mapBlockIndex.size() = %u\n",   mapBlockIndex.size());
    LogPrintf("nBestHeight = %d\n",                   chainActive.Height());
#ifdef ENABLE_WALLET
    LogPrintf("setKeyPool.size() = %u\n",      pwalletMain ? pwalletMain->setKeyPool.size() : 0);
    if(mc_gState->m_WalletMode & MC_WMD_TXS)
    {
        LogPrintf("oldWallet.size() = %u\n",       pwalletMain ? pwalletMain->mapWallet.size() : 0);        
        LogPrintf("mapWallet.size() = %u\n",       pwalletTxsMain->m_Database->m_DBStat.m_Count);        
    }
    else
    {
        LogPrintf("mapWallet.size() = %u\n",       pwalletMain ? pwalletMain->mapWallet.size() : 0);
    }
    LogPrintf("mapAddressBook.size() = %u\n",  pwalletMain ? pwalletMain->mapAddressBook.size() : 0);
#endif

    if (pwalletMain)
        bitdbwrap.Flush(false);
    
#ifdef ENABLE_WALLET
    if (pwalletMain) {
        // Run a thread to flush wallet periodically
        threadGroup.create_thread(boost::bind(&ThreadFlushWalletDB, boost::ref(pwalletMain->strWalletFile)));
    }
#endif

    SetRPCWarmupFinished();                                                     // Should be here, otherwise wallet can double spend
    uiInterface.InitMessage(_("Done loading"));

/* MCHN START */    
    if(!GetBoolArg("-shortoutput", false))
    {    
        sprintf(bufOutput,"Node ready.\n\n");
        bytes_written=write(OutputPipe,bufOutput,strlen(bufOutput));
    }
    mc_InitRPCHelpMap();

    LogPrintf("Node started\n");
/* MCHN END */    
    return !fRequestShutdown;
}
