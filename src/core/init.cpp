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
#include "community/community.h"


/* MCHN START */

#include "structs/base58.h"
#include "multichain/multichain.h"
#include "wallet/wallettxs.h"
#include "protocol/relay.h"
#include "filters/filter.h"

std::string BurnAddress(const std::vector<unsigned char>& vchVersion);
std::string SetBannedTxs(std::string txlist);
std::string SetLockedBlock(std::string hash);
bool RecoverAfterCrash();

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

#ifdef ENABLE_WALLET
CWallet* pwalletMain = NULL;
mc_WalletTxs* pwalletTxsMain = NULL;
#endif
mc_RelayManager* pRelayManager = NULL;
mc_FilterEngine* pFilterEngine = NULL;
mc_MultiChainFilterEngine* pMultiChainFilterEngine = NULL;
CInitNodeStatus *pNodeStatus = NULL;
CCriticalSection cs_NodeStatus;


bool fFeeEstimatesInitialized = false;
extern int JSON_DOUBLE_DECIMAL_DIGITS;                             

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

static const char* FEE_ESTIMATES_FILENAME="fee_estimates.dat";
CClientUIInterface uiInterface;

//! -paytxfee will warn if called with a higher fee than this amount (in satoshis) per KB
static const CAmount nHighTransactionFeeWarning = 0.01 * COIN;
//! -maxtxfee will warn if called with a higher fee than this amount (in satoshis)
static const CAmount nHighTransactionMaxFeeWarning = 100 * nHighTransactionFeeWarning;


CInitNodeStatus::CInitNodeStatus()
{
    fInitialized=false;
    sSeedIP="";
    nSeedPort=0;
    sAddress="";
    sLastError="";    
    tStartConnectTime=0;
}

//////////////////////////////////////////////////////////////////////////////
//
// Shutdown
//

//
// Thread management and startup/shutdown:
//
// The network-processing threads are all part of a thread group
// created by AppInit() or the Qt main() function.
//
// A clean exit happens when StartShutdown() or the SIGTERM
// signal handler sets fRequestShutdown, which triggers
// the DetectShutdownThread(), which interrupts the main thread group.
// DetectShutdownThread() then exits, which causes AppInit() to
// continue (it .joins the shutdown thread).
// Shutdown() is then
// called to clean up database connections, and stop other
// threads that should only be stopped after the main network-processing
// threads have exited.
//
// Note that if running -daemon the parent process returns from AppInit2
// before adding any threads to the threadGroup, so .join_all() returns
// immediately and the parent exits from main().
//
// Shutdown for Qt is very similar, only it uses a QTimer to detect
// fRequestShutdown getting set, and then does the normal Qt
// shutdown thing.
//

volatile bool fRequestShutdown = false;
volatile bool fShutdownCompleted = false;

void StartShutdown()
{
    fRequestShutdown = true;
}
bool ShutdownRequested()
{
    return fRequestShutdown;
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
static boost::scoped_ptr<ECCVerifyHandle> globalVerifyHandle;

void Shutdown()
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
    mempool.AddTransactionsUpdated(1);
    StopRPCThreads();
#ifdef ENABLE_WALLET
    if (pwalletMain)
        bitdbwrap.Flush(false);
    GenerateBitcoins(false, NULL, 0);
#endif
    StopNode();
    UnregisterNodeSignals(GetNodeSignals());

    if (fFeeEstimatesInitialized)
    {
        boost::filesystem::path est_path = GetDataDir() / FEE_ESTIMATES_FILENAME;
        CAutoFile est_fileout(fopen(est_path.string().c_str(), "wb"), SER_DISK, CLIENT_VERSION);
        if (!est_fileout.IsNull())
            mempool.WriteFeeEstimates(est_fileout);
        else
            LogPrintf("%s: Failed to write fee estimates to %s\n", __func__, est_path.string());
        fFeeEstimatesInitialized = false;
    }

    {
        LOCK(cs_main);
        if (pcoinsTip != NULL) {
            FlushStateToDisk();
        }
        delete pcoinsTip;
        pcoinsTip = NULL;
        delete pcoinscatcher;
        pcoinscatcher = NULL;
        delete pcoinsdbview;
        pcoinsdbview = NULL;
        delete pblocktree;
        pblocktree = NULL;
    }
#ifdef ENABLE_WALLET
    if (pwalletMain)
        bitdbwrap.Flush(true);
#endif
//#ifndef WIN32
    boost::filesystem::remove(GetPidFile());
//#endif
    UnregisterAllValidationInterfaces();
#ifdef ENABLE_WALLET
    delete pwalletMain;
    pwalletMain = NULL;
/* MCHN START */  
    {
        LOCK(cs_NodeStatus);
        if(pNodeStatus)
        {
            delete pNodeStatus;
            pNodeStatus=NULL;        
        }
    }
    if(pwalletTxsMain)
    {
        delete pwalletTxsMain;
        pwalletTxsMain=NULL;
    }
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
    
/* MCHN END */  
#endif
    globalVerifyHandle.reset();
    ECC_Stop();
    LogPrintf("%s: done\n", __func__);
    fShutdownCompleted = true;
}

/**
 * Signal handlers are very limited in what they are allowed to do, so:
 */
void HandleSIGTERM(int)
{
    fRequestShutdown = true;
    fShutdownCompleted = false;
#ifdef WIN32
    while(!fShutdownCompleted)
    {
        MilliSleep(100);
    }
#endif
}

string mc_ParseIPPort(string strAddr,int *port)
{
    *port=0;
    string s_ip=strAddr;
    size_t last_bracket=strAddr.find_last_of(']');
    size_t last=strAddr.find_last_of(':');
    string s_port;
    
    if( (last != string::npos) && ( (last_bracket == string::npos) || (last_bracket < last) ) )
    {
        s_ip=strAddr.substr(0,last);
        s_port=strAddr.substr(last+1);
        *port=atoi(s_port);
    }
/*    
    stringstream ss(s_ip); 
    string tok;
    if(getline(ss, tok, ':'))
    {
        s_ip=tok;
        if(getline(ss, tok, ':'))
        {
            *port=atoi(tok);
        }
    }            
 */ 
    return s_ip;
}



void HandleSIGHUP(int)
{
    fReopenDebugLog = true;
}

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

bool static Bind(const CService &addr, unsigned int flags) {
    if (!(flags & BF_EXPLICIT) && IsLimited(addr))
        return false;
    std::string strError;
    if (!BindListenPort(addr, strError, (flags & BF_WHITELIST) != 0)) {
        if (flags & BF_REPORT_ERROR)
            return InitError(strError);
        return false;
    }
    return true;
}

std::string HelpMessage(HelpMessageMode mode)                                   // MCHN
{
    // When adding new options to the categories, please keep and ensure alphabetical ordering.
    string strUsage = _("Options:") + "\n";
    strUsage += "  -?                     " + _("This help message") + "\n";
    strUsage += "  -alertnotify=<cmd>     " + _("Execute command when a relevant alert is received or we see a really long fork (%s in cmd is replaced by message)") + "\n";
    strUsage += "  -blocknotify=<cmd>     " + _("Execute command when the best block changes (%s in cmd is replaced by block hash)") + "\n";
    strUsage += "  -checkblocks=<n>       " + strprintf(_("How many blocks to check at startup (default: %u, 0 = all)"), 288) + "\n";
    strUsage += "  -checklevel=<n>        " + strprintf(_("How thorough the block verification of -checkblocks is (0-4, default: %u)"), 3) + "\n";
    strUsage += "  -conf=<file>           " + strprintf(_("Specify configuration file (default: %s)"), "multichain.conf") + "\n";
    if (mode == HMM_BITCOIND)
    {
#if !defined(WIN32)
        strUsage += "  -daemon                " + _("Run in the background as a daemon and accept commands") + "\n";
#endif
    }
    strUsage += "  -datadir=<dir>         " + _("Specify data directory") + "\n";
    strUsage += "  -dbcache=<n>           " + strprintf(_("Set database cache size in megabytes (%d to %d, default: %d)"), nMinDbCache, nMaxDbCache, nDefaultDbCache) + "\n";
    strUsage += "  -loadblock=<file>      " + _("Imports blocks from external blk000??.dat file") + " " + _("on startup") + "\n";
    strUsage += "  -loadblockmaxsize=<n>  " + _("Maximal block size in the files specified in -loadblock") + "\n";
    strUsage += "  -maxorphantx=<n>       " + strprintf(_("Keep at most <n> unconnectable transactions in memory (default: %u)"), DEFAULT_MAX_ORPHAN_TRANSACTIONS) + "\n";
    strUsage += "  -par=<n>               " + strprintf(_("Set the number of script verification threads (%u to %d, 0 = auto, <0 = leave that many cores free, default: %d)"), -(int)boost::thread::hardware_concurrency(), MAX_SCRIPTCHECK_THREADS, DEFAULT_SCRIPTCHECK_THREADS) + "\n";
#ifndef WIN32
    strUsage += "  -pid=<file>            " + strprintf(_("Specify pid file (default: %s)"), "multichain.pid") + "\n";
#endif
    strUsage += "  -reindex               " + _("Rebuild the blockchain and reindex transactions on startup.") + "\n";
#if !defined(WIN32)
    strUsage += "  -shortoutput           " + _("Returns connection string if this node can start or default multichain address otherwise") + "\n";
#endif
/* MCHN START */    
/* Default was 0 */    
    strUsage += "  -txindex=0|1           " + strprintf(_("Maintain a full transaction index, used by the getrawtransaction rpc call (default: %u)"), 1) + "\n";
/* MCHN END */    

    strUsage += "\n" + _("Connection options:") + "\n";
    strUsage += "  -addnode=<ip>          " + _("Add a node to connect to and attempt to keep the connection open") + "\n";
    strUsage += "  -banscore=<n>          " + strprintf(_("Threshold for disconnecting misbehaving peers (default: %u)"), 100) + "\n";
    strUsage += "  -bantime=<n>           " + strprintf(_("Number of seconds to keep misbehaving peers from reconnecting (default: %u)"), 86400) + "\n";
    strUsage += "  -bind=<addr>           " + _("Bind to given address and always listen on it. Use [host]:port notation for IPv6") + "\n";
    strUsage += "  -connect=<ip>          " + _("Connect only to the specified node(s)") + "\n";
    strUsage += "  -discover=0|1          " + _("Discover own IP address (default: 1 when listening and no -externalip)") + "\n";
    strUsage += "  -dns=0|1               " + _("Allow DNS lookups for -addnode, -seednode and -connect") + " " + _("(default: 1)") + "\n";
    strUsage += "  -dnsseed=0|1           " + _("Query for peer addresses via DNS lookup, if low on addresses (default: 1 unless -connect)") + "\n";
    strUsage += "  -externalip=<ip>       " + _("Specify your own public address") + "\n";
    strUsage += "  -forcednsseed          " + strprintf(_("Always query for peer addresses via DNS lookup (default: %u)"), 0) + "\n";
    strUsage += "  -listen=0|1            " + _("Accept connections from outside (default: 1 if no -proxy or -connect)") + "\n";
    strUsage += "  -maxconnections=<n>    " + strprintf(_("Maintain at most <n> connections to peers (default: %u)"), 125) + "\n";
    strUsage += "  -maxoutconnections=<n> " + strprintf(_("Open at most <n> outbound connections to peers (1-32, default: %u)"), 8) + "\n";
    strUsage += "  -maxreceivebuffer=<n>  " + strprintf(_("Maximum per-connection receive buffer, <n>*1000 bytes (default: %u)"), 5000) + "\n";
    strUsage += "  -maxsendbuffer=<n>     " + strprintf(_("Maximum per-connection send buffer, <n>*1000 bytes (default: %u)"), 100000) + "\n";
    strUsage += "  -onion=<ip:port>       " + strprintf(_("Use separate SOCKS5 proxy to reach peers via Tor hidden services (default: %s)"), "-proxy") + "\n";
    strUsage += "  -onlynet=<net>         " + _("Only connect to nodes in network <net> (ipv4, ipv6 or onion)") + "\n";
    strUsage += "  -permitbaremultisig    " + strprintf(_("Relay non-P2SH multisig (default: %u)"), 1) + "\n";
    strUsage += "  -port=<port>           " + _("Listen for connections on <port> ") + "\n";
    strUsage += "  -proxy=<ip:port>       " + _("Connect through SOCKS5 proxy") + "\n";
    strUsage += "  -seednode=<ip>         " + _("Connect to a node to retrieve peer addresses, and disconnect") + "\n";
    strUsage += "  -timeout=<n>           " + strprintf(_("Specify connection timeout in milliseconds (minimum: 1, default: %d)"), DEFAULT_CONNECT_TIMEOUT) + "\n";
    strUsage += "  -retryinittime=<n>     " + _("Number of seconds during which an initial connection is retried before the node quits (default: 0)") + "\n";
    
/*    
#ifdef USE_UPNP
#if USE_UPNP
    strUsage += "  -upnp                  " + _("Use UPnP to map the listening port (default: 1 when listening)") + "\n";
#else
    strUsage += "  -upnp                  " + strprintf(_("Use UPnP to map the listening port (default: %u)"), 0) + "\n";
#endif
#endif
 */ 
    strUsage += "  -whitebind=<addr>      " + _("Bind to given address and whitelist peers connecting to it. Use [host]:port notation for IPv6") + "\n";
    strUsage += "  -whitelist=<netmask>   " + _("Whitelist peers connecting from the given netmask or IP address. Can be specified multiple times.") + "\n";
    strUsage += "                         " + _("Whitelisted peers cannot be DoS banned and their transactions are always relayed, even if they are already in the mempool, useful e.g. for a gateway") + "\n";
    strUsage += "  -onlyencrypted         " + _("Allow only encrypted connection, default: 0. Enterprise Edition only.") + "\n";
    strUsage += "  -allowunencrypted=<netmask>   " + _("Allowed unencrypted connections with peers connecting from the given netmask or IP address. Can be specified multiple times.") + "\n";
    strUsage += "                         " + _("Enterprise Edition only.") + "\n";

#ifdef ENABLE_WALLET
    strUsage += "\n" + _("Wallet options:") + "\n";
//    strUsage += "  -disablewallet         " + _("Do not load the wallet and disable wallet RPC calls") + "\n";
    strUsage += "  -keypool=<n>           " + strprintf(_("Set key pool size to <n> (default: %u)"), 1) + "\n";
    if (GetBoolArg("-help-debug", false))
        strUsage += "  -mintxfee=<amt>        " + strprintf(_("Fees (in BTC/Kb) smaller than this are considered zero fee for transaction creation (default: %s)"), FormatMoney(CWallet::minTxFee.GetFeePerK())) + "\n";
    strUsage += "  -paytxfee=<amt>        " + strprintf(_("Fee (in BTC/kB) to add to transactions you send (default: %s)"), FormatMoney(payTxFee.GetFeePerK())) + "\n";
    strUsage += "  -rescan                " + _("Rescan the block chain for missing wallet transactions") + " " + _("on startup") + "\n";
    strUsage += "  -salvagewallet         " + _("Attempt to recover private keys from a corrupt wallet.dat") + " " + _("on startup") + "\n";
    strUsage += "  -skipwalletchecks      " + _("Skip wallet consistency verification on startup") + "\n";
    strUsage += "  -sendfreetransactions  " + strprintf(_("Send transactions as zero-fee transactions if possible (default: %u)"), 0) + "\n";
    strUsage += "  -spendzeroconfchange=0|1" + strprintf(_("Spend unconfirmed change when sending transactions (default: %u)"), 1) + "\n";
    strUsage += "  -txconfirmtarget=0|1   " + strprintf(_("If paytxfee is not set, include enough fee so transactions begin confirmation on average within n blocks (default: %u)"), 1) + "\n";
    strUsage += "  -maxtxfee=<amt>        " + strprintf(_("Maximum total fees to use in a single wallet transaction, setting too low may abort large transactions (default: %s)"), FormatMoney(maxTxFee)) + "\n";
//    strUsage += "  -upgradewallet         " + _("Upgrade wallet to latest format") + " " + _("on startup") + "\n";
    strUsage += "  -wallet=<file>         " + _("Specify wallet file (within data directory)") + " " + strprintf(_("(default: %s)"), "wallet.dat") + "\n";
    strUsage += "  -walletnotify=<cmd>    " + _("Execute this command when a transaction is first seen or confirmed, if it relates to an address in the wallet or a subscribed asset or stream. ") + "\n";
    strUsage += "  -walletnotifynew=<cmd> " + _("Execute this command when a transaction is first seen, if it relates to an address in the wallet or a subscribed asset or stream. ") + "\n";
    strUsage += "                         " + _("(more details and % substitutions online)") + "\n";
/* MCHN START */    
    strUsage += "  -walletdbversion=2|3   " + _("Specify wallet version, 2 - Berkeley DB, 3 (default) - proprietary") + "\n";
    strUsage += "  -autosubscribe=<params> " + _("Automatically subscribe to new streams and/or assets, as a comma delimited list of subscriptions.") + "\n";
    strUsage += "                         " + _("All editions: assets, streams. Enterprise Edition only: streams-items,streams-items-local,") + "\n";
    strUsage += "                         " + _("streams-keys,streams-keys-local,streams-publishers,streams-publishers-local,streams-retrieve") + "\n";
/* MCHN END */    
    strUsage += "  -zapwallettxes=<mode>  " + _("Delete all wallet transactions and only recover those parts of the blockchain through -rescan on startup") + "\n";
    strUsage += "                         " + _("(1 = keep tx meta data e.g. account owner and payment request information, 2 = drop tx meta data)") + "\n";
#endif

    strUsage += "\n" + _("Debugging/Testing options:") + "\n";
    if (GetBoolArg("-help-debug", false))
    {
        strUsage += "  -checkpoints=0|1       " + strprintf(_("Only accept block chain matching built-in checkpoints (default: %u)"), 1) + "\n";
        strUsage += "  -dblogsize=<n>         " + strprintf(_("Flush database activity from memory pool to disk log every <n> megabytes (default: %u)"), 100) + "\n";
        strUsage += "  -disablesafemode       " + strprintf(_("Disable safemode, override a real safe mode event (default: %u)"), 0) + "\n";
        strUsage += "  -testsafemode          " + strprintf(_("Force safe mode (default: %u)"), 0) + "\n";
        strUsage += "  -dropmessagestest=<n>  " + _("Randomly drop 1 of every <n> network messages") + "\n";
        strUsage += "  -fuzzmessagestest=<n>  " + _("Randomly fuzz 1 of every <n> network messages") + "\n";
        strUsage += "  -flushwallet=0|1       " + strprintf(_("Run a thread to flush wallet periodically (default: %u)"), 1) + "\n";
        strUsage += "  -stopafterblockimport  " + strprintf(_("Stop running after importing blocks from disk (default: %u)"), 0) + "\n";
    }
    strUsage += "  -debug=<category>      " + strprintf(_("Output debugging information (default: %u, supplying <category> is optional)"), 0) + "\n";
    strUsage += "                         " + _("If <category> is not supplied, output all debugging information.") + "\n";
    strUsage += "                         " + _("<category> can be: addrman, alert, bench, coindb, db, lock, rand, rpc, selectcoins, mempool, net,") + "\n";
    strUsage += "                         " + _("                   mchn, mcblock, mcnet, mcminer, mcapi, wallet, filter, v8filter, chunks, offchain") + "\n";
    if (mode == HMM_BITCOIN_QT)
        strUsage += ", qt";
//    strUsage += ".\n";
    strUsage += "  -help-debug            " + _("Show all debugging options (usage: --help -help-debug)") + "\n";
    strUsage += "  -logips                " + strprintf(_("Include IP addresses in debug output (default: %u)"), 0) + "\n";
    strUsage += "  -logtimestamps=0|1     " + strprintf(_("Prepend debug output with timestamp (default: %u)"), 1) + "\n";
    strUsage += "  -limitfreerelay=<n>    " + strprintf(_("Continuously rate-limit free transactions to <n>*1000 bytes per minute (default:%u)"), 0) + "\n";
    if (GetBoolArg("-help-debug", false))
    {
        strUsage += "  -relaypriority=0|1     " + strprintf(_("Require high priority for relaying free or low-fee transactions (default:%u)"), 1) + "\n";
        strUsage += "  -maxsigcachesize=<n>   " + strprintf(_("Limit size of signature cache to <n> entries (default: %u)"), 50000) + "\n";
    }
    strUsage += "  -minrelaytxfee=<amt>   " + strprintf(_("Fees (in BTC/Kb) smaller than this are considered zero fee for relaying (default: %s)"), FormatMoney(::minRelayTxFee.GetFeePerK())) + "\n";
    strUsage += "  -printtoconsole        " + _("Send trace/debug info to console instead of debug.log file") + "\n";
    strUsage += "  -logdir                " + _("Send trace/debug info to specified directory") + "\n";
    if (GetBoolArg("-help-debug", false))
    {
        strUsage += "  -printpriority         " + strprintf(_("Log transaction priority and fee per kB when mining blocks (default: %u)"), 0) + "\n";
        strUsage += "  -privdb=0|1            " + strprintf(_("Sets the DB_PRIVATE flag in the wallet db environment (default: %u)"), 1) + "\n";
/* MCHN START */    
/*    
        strUsage += "  -regtest               " + _("Enter regression test mode, which uses a special chain in which blocks can be solved instantly.") + "\n";
 */ 
/* MCHN END */    
        strUsage += "                         " + _("This is intended for regression testing tools and app development.") + "\n";
        strUsage += "                         " + _("In this mode -genproclimit controls how many blocks are generated immediately.") + "\n";
    }
    strUsage += "  -shrinkdebugfile=0|1   " + _("Shrink debug.log file on client startup (default: 1 when no -debug)") + "\n";
/* MCHN START */    
/*    
    strUsage += "  -testnet               " + _("Use the test network") + "\n";
 */ 
/* MCHN END */    

/*    
    strUsage += "\n" + _("Node relay options:") + "\n";
    strUsage += "  -datacarrier           " + strprintf(_("Relay and mine data carrier transactions (default: %u)"), 1) + "\n";
    strUsage += "  -datacarriersize       " + strprintf(_("Maximum size of data in data carrier transactions we relay and mine (default: %u)"), MAX_OP_RETURN_RELAY) + "\n";
*/
    strUsage += "\n" + _("Block creation options:") + "\n";
#ifdef ENABLE_WALLET
    strUsage += "  -gen=0|1               " + strprintf(_("Generate coins (default: %u)"), 1) + "\n";
    strUsage += "  -genproclimit=<n>      " + strprintf(_("Set the number of threads for coin generation if enabled (-1 = all cores, default: %d)"), 1) + "\n";
#endif
//    strUsage += "  -blockminsize=<n>      " + strprintf(_("Set minimum block size in bytes (default: %u)"), 0) + "\n";
//    strUsage += "  -blockmaxsize=<n>      " + strprintf(_("Set maximum block size in bytes (default: %d)"), DEFAULT_BLOCK_MAX_SIZE) + "\n";
    strUsage += "  -blockmaxsize=<n>      " + _("Set maximum block size in bytes") + "\n";
//    strUsage += "  -blockprioritysize=<n> " + strprintf(_("Set maximum size of high-priority/low-fee transactions in bytes (default: %d)"), DEFAULT_BLOCK_PRIORITY_SIZE) + "\n";

    strUsage += "\n" + _("RPC server options:") + "\n";
    strUsage += "  -server=0|1            " + _("Accept command line and JSON-RPC commands") + "\n";
    strUsage += "  -rest                  " + strprintf(_("Accept public REST requests (default: %u)"), 0) + "\n";
    strUsage += "  -rpcbind=<addr>        " + _("Bind to given address to listen for JSON-RPC connections. Use [host]:port notation for IPv6. This option can be specified multiple times (default: bind to all interfaces)") + "\n";
    strUsage += "  -rpcuser=<user>        " + _("Username for JSON-RPC connections") + "\n";
    strUsage += "  -rpcpassword=<pw>      " + _("Password for JSON-RPC connections") + "\n";
    strUsage += "  -rpcport=<port>        " + _("Listen for JSON-RPC connections on <port>") + "\n";
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
    strUsage += "  -offline                                 " + _("Start multichaind in offline mode, no connections to other nodes.") + "\n";
    strUsage += "  -storeruntimeparams                      " + _("Permanently save modifications to runtime parameters made by setruntimeparam APIs.") + "\n";
    strUsage += "  -initprivkey=<privkey>                   " + _("Manually set the wallet default address and private key when running multichaind for the first time.") + "\n";
    strUsage += "  -handshakelocal=<address>                " + _("Manually override the wallet address which is used for handshaking with other peers in a MultiChain blockchain.") + "\n";
    strUsage += "  -lockadminminerounds=<n>                 " + _("If set overrides lock-admin-mine-rounds blockchain setting.") + "\n";
    strUsage += "  -miningrequirespeers                     " + _("If set overrides mining-requires-peers blockchain setting, values 0/1.") + "\n";
    strUsage += "  -mineemptyrounds=<n>                     " + _("If set overrides mine-empty-rounds blockchain setting, values 0.0-1000.0 or -1.") + "\n";
    strUsage += "  -miningturnover=<n>                      " + _("If set overrides mining-turnover blockchain setting, values 0-1.") + "\n";
    strUsage += "  -shrinkdebugfilesize=<n>                 " + _("If shrinkdebugfile is 1, this controls the size of the debug file. Whenever the debug.log file reaches over 5 times this number of bytes, it is reduced back down to this size.") + "\n";
    strUsage += "  -shortoutput                             " + _("Only show the node address (if connecting was successful) or an address in the wallet (if connect permissions must be granted by another node)") + "\n";
    strUsage += "  -bantx=<txids>                           " + _("Comma delimited list of banned transactions.") + "\n";
    strUsage += "  -lockblock=<hash>                        " + _("Blocks on branches without this block will be rejected") + "\n";
    strUsage += "  -chunkquerytimeout=<n>                   " + _("Timeout, after which undelivered chunk is moved to the end of the chunk queue, default 25s") + "\n";
    strUsage += "  -chunkrequesttimeout=<n>                 " + _("Timeout, after which chunk request is dropped and another source is tried, default 10s") + "\n";
    strUsage += "  -flushsourcechunks=0|1                   " + _("Flush offchain items created by this node to disk immediately when created, default 1") + "\n";
    strUsage += "  -acceptfiltertimeout=<n>                 " + strprintf(_("Timeout, after which filter execution will be aborted, when accepting new txs, in milliseconds, default %u"),DEFAULT_ACCEPT_FILTER_TIMEOUT) + "\n";
    strUsage += "  -sendfiltertimeout=<n>                   " + strprintf(_("Timeout, after which filter execution will be aborted, when tx is sent from this node, in milliseconds, default %u"),DEFAULT_SEND_FILTER_TIMEOUT) + "\n";
    strUsage += "  -lockinlinemetadata=0|1                  " + _("Outputs with inline metadata can be sent only using create/appendrawtransaction, default 1") + "\n";
    strUsage += "  -purgemethod=<method>                    " + _("Overwrite data before purging. Available modes: unlink, simple(=zero, default), one, zeroone, random1-random4, dod, doe, rcmp, gutmann.") + "\n";
    strUsage += "                                           " + _("Can be followed by '-pattern' (up to 6 characters), i.e. random2-mchn makes two random pattern passes followed by 'mchn'. Enterprise Edition only.") + "\n";

    strUsage += "\n" + _("MultiChain API response parameters") + "\n";        
    strUsage += "  -hideknownopdrops      " + strprintf(_("Remove recognized MultiChain OP_DROP metadata from the responses to JSON-RPC calls (default: %u)"), 0) + "\n";
    strUsage += "  -maxshowndata=<n>      " + strprintf(_("The maximum number of bytes to show in the data field of API responses. (default: %u)"), MAX_OP_RETURN_SHOWN) + "\n";
    strUsage += "                         " + _("Pieces of data larger than this will be returned as an object with txid, vout and size fields, for use with the gettxoutdata command.") + "\n";
    strUsage += "  -maxqueryscanitems=<n> " + strprintf(_("The maximum number of txs to be decoded during JSON-RPC querying commands. (default: %u)"), MAX_STREAM_QUERY_ITEMS) + "\n";
    strUsage += "  -v1apicompatible       " + strprintf(_("JSON-RPC calls responses compatible with MultiChain 1.0 (default: %u)"), 0) + "\n";
//    strUsage += "  -apidecimaldigits=<n>  " + _("maximal number of decimal digits in API output (default: auto)") + "\n";
           
    strUsage += "\n" + _("Wallet optimization options:") + "\n";
    strUsage += "  -autocombineminconf=<n>    " + _("Only automatically combine outputs with at least this number of confirmations, default 1") + "\n";
    strUsage += "  -autocombinemininputs=<n>  " + _("Minimum inputs in automatically created combine transaction, default 50") + "\n";
    strUsage += "  -autocombinemaxinputs=<n>  " + _("Maximum inputs in automatically created combine transaction, default 100") + "\n";
    strUsage += "  -autocombinedelay=<n>      " + _("Minimium delay between two auto-combine transactions, in seconds, default 1") + "\n";
    strUsage += "  -autocombinesuspend=<n>    " + _("Auto-combine transaction delay after listunspent API call, in seconds, default 15") + "\n";
    
    
    
    return strUsage;
}

std::string LicenseInfo()
{
    return FormatParagraph(_("Copyright (c) Coin Sciences Ltd - www.multichain.com")) + "\n" +
           "\n" +
           FormatParagraph(_("You are granted a non-exclusive license to use this software for any legal purpose, and to redistribute it unmodified.")) + "\n" +
           "\n" +
           FormatParagraph(_("The software product under this license is provided free of charge. ")) + "\n" +
           "\n" +
           FormatParagraph(_("Full terms are shown at: http://www.multichain.com/terms-of-service/")) +
           "\n";
    
/*    
    return FormatParagraph(strprintf(_("Copyright (C) 2009-%i The Bitcoin Core Developers"), COPYRIGHT_YEAR)) + "\n" +
           "\n" +
           FormatParagraph(_("This is experimental software.")) + "\n" +
           "\n" +
           FormatParagraph(_("Distributed under the MIT software license, see the accompanying file COPYING or <http://www.opensource.org/licenses/mit-license.php>.")) + "\n" +
           "\n" +
           FormatParagraph(_("This product includes software developed by the OpenSSL Project for use in the OpenSSL Toolkit <https://www.openssl.org/> and cryptographic software written by Eric Young and UPnP software written by Thomas Bernard.")) +
           "\n";
 */ 
    
    
}

static void BlockNotifyCallback(const uint256& hashNewTip)
{
    std::string strCmd = GetArg("-blocknotify", "");

    boost::replace_all(strCmd, "%s", hashNewTip.GetHex());
    boost::thread t(runCommand, strCmd); // thread runs free
}

struct CImportingNow
{
    CImportingNow() {
        assert(fImporting == false);
        fImporting = true;
    }

    ~CImportingNow() {
        assert(fImporting == true);
        fImporting = false;
    }
};

void ThreadImport(std::vector<boost::filesystem::path> vImportFiles)
{
    RenameThread("bitcoin-loadblk");

    // -reindex
    if (fReindex) {
        CImportingNow imp;
        int nFile = 0;
        while (true) {
            CDiskBlockPos pos(nFile, 0);
            if (!boost::filesystem::exists(GetBlockPosFilename(pos, "blk")))
                break; // No block files left to reindex
            FILE *file = OpenBlockFile(pos, true);
            if (!file)
                break; // This error is logged in OpenBlockFile
            LogPrintf("Reindexing block file blk%05u.dat...\n", (unsigned int)nFile);
            LoadExternalBlockFile(file, &pos);
            nFile++;
        }
        pblocktree->WriteReindexing(false);
        fReindex = false;
        LogPrintf("Reindexing finished\n");
        // To avoid ending up in a situation without genesis block, re-try initializing (no-op if reindexing worked):
        InitBlockIndex();
    }

    // hardcoded $DATADIR/bootstrap.dat
    filesystem::path pathBootstrap = GetDataDir() / "bootstrap.dat";
    if (filesystem::exists(pathBootstrap)) {
        FILE *file = fopen(pathBootstrap.string().c_str(), "rb");
        if (file) {
            CImportingNow imp;
            filesystem::path pathBootstrapOld = GetDataDir() / "bootstrap.dat.old";
            LogPrintf("Importing bootstrap.dat...\n");
            LoadExternalBlockFile(file);
            RenameOver(pathBootstrap, pathBootstrapOld);
        } else {
            LogPrintf("Warning: Could not open bootstrap file %s\n", pathBootstrap.string());
        }
    }

    // -loadblock=
    BOOST_FOREACH(boost::filesystem::path &path, vImportFiles) {
        FILE *file = fopen(path.string().c_str(), "rb");
        if (file) {
            CImportingNow imp;
            LogPrintf("Importing blocks file %s...\n", path.string());
            LoadExternalBlockFile(file);
        } else {
            LogPrintf("Warning: Could not open blocks file %s\n", path.string());
        }
    }

    if (GetBoolArg("-stopafterblockimport", false)) {
        LogPrintf("Stopping after block import\n");
        StartShutdown();
    }
}

/** Sanity checks
 *  Ensure that Bitcoin is running in a usable environment with all
 *  necessary library support.
 */
bool InitSanityCheck(void)
{
    if(!ECC_InitSanityCheck()) {
        InitError("Elliptic curve cryptography sanity check failure. Aborting.");
        return false;
    }
    if (!glibc_sanity_test() || !glibcxx_sanity_test())
        return false;

    return true;
}

bool GrantMessagePrinted(int OutputPipe,bool failed_seed)
{
    char bufOutput[4096];
    if(mc_gState->m_NetworkParams->m_Status == MC_PRM_STATUS_MINIMAL)
    {
        if(pwalletMain)
        {
            if(pwalletMain->vchDefaultKey.IsValid())
            {
                LogPrintf("mchn: Minimal blockchain parameter set is created, default address: %s\n",
                        CBitcoinAddress(pwalletMain->vchDefaultKey.GetID()).ToString().c_str());
                    
                if(!GetBoolArg("-shortoutput", false))
                {    
                    sprintf(bufOutput,"Blockchain successfully initialized.\n\n");             
                    write(OutputPipe,bufOutput,strlen(bufOutput));
                    
                    if(failed_seed)
                    {
                        sprintf(bufOutput,"Could not connect to seed node, please ensure it is up and available.\n\n");
                        write(OutputPipe,bufOutput,strlen(bufOutput));                        
                    }
                    
                    sprintf(bufOutput,"Please ask blockchain admin or user having activate permission to let you connect and/or transact:\n");
                    write(OutputPipe,bufOutput,strlen(bufOutput));

                    sprintf(bufOutput,"multichain-cli %s grant %s connect\n",mc_gState->m_NetworkParams->Name(),
                         CBitcoinAddress(pwalletMain->vchDefaultKey.GetID()).ToString().c_str());
                    write(OutputPipe,bufOutput,strlen(bufOutput));
                    sprintf(bufOutput,"multichain-cli %s grant %s connect,send,receive\n\n",mc_gState->m_NetworkParams->Name(),
                         CBitcoinAddress(pwalletMain->vchDefaultKey.GetID()).ToString().c_str());
                    write(OutputPipe,bufOutput,strlen(bufOutput));
                }
                else
                {
                    sprintf(bufOutput,"%s\n",CBitcoinAddress(pwalletMain->vchDefaultKey.GetID()).ToString().c_str());                            
                    write(OutputPipe,bufOutput,strlen(bufOutput));
                }
                return true;
            }
        }
    }    
    else
    {
        if(!GetBoolArg("-shortoutput", false))
        {    
            if(failed_seed)
            {
                sprintf(bufOutput,"Could not connect to seed node, please ensure it is up and available.\n\n");
                write(OutputPipe,bufOutput,strlen(bufOutput));                        
            }
        }        
    }
    return false;
}

/** Initialize bitcoin.
 *  @pre Parameters should be parsed and config file should be read.
 */
bool AppInit2(boost::thread_group& threadGroup,int OutputPipe)
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
    fLogIPs = GetBoolArg("-logips", false);
/* MCHN START */    
    fLogTimeMillis = GetBoolArg("-logtimemillis", false);
/* MCHN END */    

    if (mapArgs.count("-bind") || mapArgs.count("-whitebind")) {
        // when specifying an explicit binding address, you want to listen on it
        // even when -connect or -proxy is specified
        if (SoftSetBoolArg("-listen", true))
            LogPrintf("AppInit2 : parameter interaction: -bind or -whitebind set -> setting -listen=1\n");
    }

    if (mapArgs.count("-connect") && mapMultiArgs["-connect"].size() > 0) {
        // when only connecting to trusted nodes, do not seed via DNS, or listen by default
        if (SoftSetBoolArg("-dnsseed", false))
            LogPrintf("AppInit2 : parameter interaction: -connect set -> setting -dnsseed=0\n");
        if (SoftSetBoolArg("-listen", false))
            LogPrintf("AppInit2 : parameter interaction: -connect set -> setting -listen=0\n");
    }

    if (mapArgs.count("-proxy")) {
        // to protect privacy, do not listen by default if a default proxy server is specified
        if (SoftSetBoolArg("-listen", false))
            LogPrintf("AppInit2 : parameter interaction: -proxy set -> setting -listen=0\n");
        // to protect privacy, do not discover addresses by default
        if (SoftSetBoolArg("-discover", false))
            LogPrintf("AppInit2 : parameter interaction: -proxy set -> setting -discover=0\n");
    }

    if (!GetBoolArg("-listen", true)) {
        // do not map ports or try to retrieve public IP when not listening (pointless)
/*        
        if (SoftSetBoolArg("-upnp", false))
            LogPrintf("AppInit2 : parameter interaction: -listen=0 -> setting -upnp=0\n");
 */ 
        mapArgs["-upnp"] = std::string("0");
        if (SoftSetBoolArg("-discover", false))
            LogPrintf("AppInit2 : parameter interaction: -listen=0 -> setting -discover=0\n");
    }

    if (mapArgs.count("-externalip")) {
        // if an explicit public IP is specified, do not try to find others
        if (SoftSetBoolArg("-discover", false))
            LogPrintf("AppInit2 : parameter interaction: -externalip set -> setting -discover=0\n");
    }

    if (GetBoolArg("-salvagewallet", false)) {
        // Rewrite just private keys: rescan to find transactions
        if (SoftSetBoolArg("-rescan", true))
            LogPrintf("AppInit2 : parameter interaction: -salvagewallet=1 -> setting -rescan=1\n");
    }

    // -zapwallettx implies a rescan
    if (GetBoolArg("-zapwallettxes", false)) {
        if (SoftSetBoolArg("-rescan", true))
            LogPrintf("AppInit2 : parameter interaction: -zapwallettxes=<mode> -> setting -rescan=1\n");
    }

    // Make sure enough file descriptors are available
    int nBind = std::max((int)mapArgs.count("-bind") + (int)mapArgs.count("-whitebind"), 1);
    nMaxConnections = GetArg("-maxconnections", 125);
    nMaxConnections = std::max(std::min(nMaxConnections, (int)(FD_SETSIZE - nBind - MIN_CORE_FILEDESCRIPTORS)), 0);
    int nFD = RaiseFileDescriptorLimit(nMaxConnections + MIN_CORE_FILEDESCRIPTORS);
    if (nFD < MIN_CORE_FILEDESCRIPTORS)
        return InitError(_("Not enough file descriptors available."));
    if (nFD - MIN_CORE_FILEDESCRIPTORS < nMaxConnections)
        nMaxConnections = nFD - MIN_CORE_FILEDESCRIPTORS;

    // ********************************************************* Step 3: parameter-to-internal-flags

    fDebug = !mapMultiArgs["-debug"].empty();
    // Special-case: if -debug=0/-nodebug is set, turn off debugging messages
    const vector<string>& categories = mapMultiArgs["-debug"];
    if (GetBoolArg("-nodebug", false) || find(categories.begin(), categories.end(), string("0")) != categories.end())
        fDebug = false;

    // Check for -debugnet
    if (GetBoolArg("-debugnet", false))
        InitWarning(_("Warning: Unsupported argument -debugnet ignored, use -debug=net."));
    // Check for -socks - as this is a privacy risk to continue, exit here
    if (mapArgs.count("-socks"))
        return InitError(_("Error: Unsupported argument -socks found. Setting SOCKS version isn't possible anymore, only SOCKS5 proxies are supported."));
    // Check for -tor - as this is a privacy risk to continue, exit here
    if (GetBoolArg("-tor", false))
        return InitError(_("Error: Unsupported argument -tor found, use -onion."));
    int MaxOutConnections=GetArg("-maxoutconnections",8);
    if ( (MaxOutConnections < 1) || (MaxOutConnections > 32) )
    {
        return InitError(_("Error: -maxoutconnections out of range."));        
    }
    
    
    
    if (GetBoolArg("-benchmark", false))
        InitWarning(_("Warning: Unsupported argument -benchmark ignored, use -debug=bench."));

    // Checkmempool defaults to true in regtest mode
    mempool.setSanityCheck(GetBoolArg("-checkmempool", Params().DefaultCheckMemPool()));
    Checkpoints::fEnabled = GetBoolArg("-checkpoints", true);

    // -par=0 means autodetect, but nScriptCheckThreads==0 means no concurrency
    nScriptCheckThreads = GetArg("-par", DEFAULT_SCRIPTCHECK_THREADS);
    if (nScriptCheckThreads <= 0)
        nScriptCheckThreads += boost::thread::hardware_concurrency();
    if (nScriptCheckThreads <= 1)
        nScriptCheckThreads = 0;
    else if (nScriptCheckThreads > MAX_SCRIPTCHECK_THREADS)
        nScriptCheckThreads = MAX_SCRIPTCHECK_THREADS;

    fServer = GetBoolArg("-server", false);

#ifdef ENABLE_WALLET
    bool fDisableWallet = false;//GetBoolArg("-disablewallet", false);
#endif

    nConnectTimeout = GetArg("-timeout", DEFAULT_CONNECT_TIMEOUT);
    if (nConnectTimeout <= 0)
        nConnectTimeout = DEFAULT_CONNECT_TIMEOUT;

    // Continue to put "/P2SH/" in the coinbase to monitor
    // BIP16 support.
    // This can be removed eventually...
    const char* pszP2SH = "/P2SH/";
    COINBASE_FLAGS << std::vector<unsigned char>(pszP2SH, pszP2SH+strlen(pszP2SH));

    // Fee-per-kilobyte amount considered the same as "free"
    // If you are mining, be careful setting this:
    // if you set it to zero then
    // a transaction spammer can cheaply fill blocks using
    // 1-satoshi-fee transactions. It should be set above the real
    // cost to you of processing a transaction.
    
/* MCHN START */    
    ::minRelayTxFee = CFeeRate(MIN_RELAY_TX_FEE);    
    
/* MCHN END */    
    if (mapArgs.count("-minrelaytxfee"))
    {
        CAmount n = 0;
        if (ParseMoney(mapArgs["-minrelaytxfee"], n) && n > 0)
            ::minRelayTxFee = CFeeRate(n);
        else
            return InitError(strprintf(_("Invalid amount for -minrelaytxfee=<amount>: '%s'"), mapArgs["-minrelaytxfee"]));
    }

#ifdef ENABLE_WALLET
    nTxConfirmTarget = GetArg("-txconfirmtarget", 1);
    bSpendZeroConfChange = GetArg("-spendzeroconfchange", true);
    fSendFreeTransactions = GetArg("-sendfreetransactions", false);

    std::string strWalletFile = GetArg("-wallet", "wallet.dat");
#endif // ENABLE_WALLET

    fIsBareMultisigStd = GetArg("-permitbaremultisig", true) != 0;
    nMaxDatacarrierBytes = GetArg("-datacarriersize", nMaxDatacarrierBytes);

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
    bool crash_recovery_required=CreatePidFile(GetPidFile(), __US_GetPID());
//#endif
    if (GetBoolArg("-shrinkdebugfile", !fDebug))
        ShrinkDebugFile();
    LogPrintf("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
//    LogPrintf("Bitcoin version %s (%s)\n", FormatFullVersion(), CLIENT_DATE);
/* MCHN START */    
    if(mc_gState->m_NetworkParams->IsProtocolMultichain())
    {
        LogPrintf("MultiChain version build %s protocol %s (%s)\n", mc_BuildDescription(mc_gState->GetNumericVersion()), mc_gState->GetProtocolVersion(), CLIENT_DATE);
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
    LogPrintf("Using at most %i connections (%i file descriptors available)\n", nMaxConnections, nFD);
    std::ostringstream strErrors;

    LogPrintf("Using %u threads for script verification\n", nScriptCheckThreads);
    
    if(!GetBoolArg("-offline",false))
    {    
        if (nScriptCheckThreads) {
            for (int i=0; i<nScriptCheckThreads-1; i++)
                threadGroup.create_thread(&ThreadScriptCheck);
        }
    }

    /* Start the RPC server already.  It will be started in "warmup" mode
     * and not really process calls already (but it will signify connections
     * that the server is there and will be ready later).  Warmup mode will
     * be disabled when initialisation is finished.
     */

/* MCHN START */    
    
/* Moved after network initialization
    if (fServer)
    {
        uiInterface.InitMessage.connect(SetRPCWarmupStatus);
        StartRPCThreads();
    }
*/
    int64_t nStart;
    // ********************************************************* Step 5: verify wallet database integrity
#ifdef ENABLE_WALLET
    int currentwalletdatversion=0;
    int foundwalletdatversion=0;
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
            if(currentwalletdatversion != 1)
            {
                if(pEF->ENT_MinWalletDatVersion() > currentwalletdatversion)
                {
                    return InitError(strprintf("Wallet version %d is not supported in this edition of MultiChain.\n"
                            "To upgrade to version %d, run MultiChain Offline Daemon: \n"
                            "multichaind-cold %s -datadir=%s -walletdbversion=3\n"
                            "and restart multichaind or use Community Edition.\n",
                            currentwalletdatversion,pEF->ENT_MinWalletDatVersion(), mc_gState->m_NetworkParams->Name(),mc_gState->m_Params->DataDir(0,0)));                                                            
                }
            }
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
            if(pEF->ENT_MinWalletDatVersion() > currentwalletdatversion)
            {
                return InitError(strprintf("Wallet version %d is not supported in this edition of MultiChain.\n",currentwalletdatversion));                                                            
            }
            
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
            foundwalletdatversion=currentwalletdatversion;
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
    string rpc_threads_error="";
    
    if (fServer)
    {
        JSON_DOUBLE_DECIMAL_DIGITS=GetArg("-apidecimaldigits",-1);        
//        uiInterface.InitMessage.connect(SetRPCWarmupStatus);
        StartRPCThreads(rpc_threads_error);
    }
    if(rpc_threads_error.size())
    {
        if(!GetBoolArg("-shortoutput", false))
        {    
            sprintf(bufOutput,"%s\n",rpc_threads_error.c_str());                            
            bytes_written=write(OutputPipe,bufOutput,strlen(bufOutput));        
        }            
    }
    bool rpc_with_default_port=false;
    if(mc_gState->m_NetworkParams->m_Status == MC_PRM_STATUS_EMPTY)
    {
        rpc_with_default_port=true;
    }
    
    uint32_t SeedStartTime=mc_TimeNowAsUInt();
    int64_t SeedStopTime64=(int64_t)SeedStartTime+GetArg("-retryinittime",-1);
    if(SeedStopTime64 > 0xFFFFFFFF)
    {
        SeedStopTime64=0xFFFFFFFF;
    }
    if(SeedStopTime64 < 0)
    {
        SeedStopTime64=SeedStartTime-1;
    }
    uint32_t SeedStopTime=(uint32_t)SeedStopTime64;
    
    uiInterface.InitMessage(_("Initializing multichain..."));
    RegisterNodeSignals(GetNodeSignals());

    if(GetBoolArg("-v1apicompatible",false))
    {
        mc_gState->m_Compatibility |= MC_VCM_1_0;
    }

    if(GetBoolArg("-offline",false))
    {
        if(mc_gState->m_NetworkParams->m_Status != MC_PRM_STATUS_VALID)
        {
            char fileName[MC_DCT_DB_MAX_PATH];
            mc_GetFullFileName(mc_gState->m_Params->NetworkName(),"params", ".dat",MC_FOM_RELATIVE_TO_DATADIR,fileName);
            string seed_error=strprintf("Couldn't retrieve blockchain parameters from the seed node in offline mode.\n"
                        "The file %s must be copied manually from an existing node.\n",                
                    fileName);
            return InitError(seed_error);                        
        }        
    }    

    
    bool fFirstRunForBuild;
    string init_privkey=GetArg("-initprivkey","");

    mc_gState->m_NetworkParams->m_RelevantProtocolVersion=mc_gState->RelevantParamProtocolVersion(); // Caching relevant protocol version
    
    pwalletMain=NULL;
    if(mc_gState->m_NetworkParams->m_Status != MC_PRM_STATUS_VALID)
    {    
        pwalletMain = new CWallet(strWalletFile);
        DBErrors nLoadWalletRetForBuild = pwalletMain->LoadWallet(fFirstRunForBuild);

        if (nLoadWalletRetForBuild != DB_LOAD_OK)                                   // MCHN-TODO wallet recovery
        {
            if (GetBoolArg("-salvagewallet", false))
            {
                return InitError(_("wallet.dat corrupted. Please remove it and restart."));            
            }
            return InitError(_("wallet.dat is partially corrupted. Please try running MultiChain with -salvagewallet."));                            
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
    
    fNameLookup = GetBoolArg("-dns", true);
    
    string seed_error="";
    const char *seed_node;
    bool zap_wallet_txs=false;
    bool new_wallet_txs=false;
    bool grant_message_printed=false;
    seed_node=mc_gState->GetSeedNode();
    mc_Buffer *rescan_subscriptions=NULL;
    string seed_resolved="";
    int resolved_port = 0;
    string resolved_host = "";
    if(seed_node)
    {        
        seed_resolved=string(seed_node);
        
        if((mc_gState->m_NetworkParams->m_Status == MC_PRM_STATUS_EMPTY) 
           || (mc_gState->m_NetworkParams->m_Status == MC_PRM_STATUS_MINIMAL))
        {        
            SplitHostPort(seed_resolved, resolved_port, resolved_host);
            if(resolved_port == 0)
            {
                return InitError(strprintf("Couldn't connect to the seed node %s - please specify port number explicitly.",seed_node));                                        
            }

            LogPrintf("mchn: Checking seed address %s\n",seed_node);            
            CService resolved_addr;
            if(!Lookup(seed_node, resolved_addr, 0, 1))
            {
                return InitError(strprintf("Couldn't resolve seed address %s.",seed_node));                                                    
            }
            seed_resolved=resolved_addr.ToString();        
        }
        
        mc_gState->SetSeedNode(seed_resolved.c_str());
        LogPrintf("mchn: Seed address resolved: %s\n",mc_gState->GetSeedNode());    
    }
    
    
    if(pEF->Prepare())
    {
        fprintf(stderr,"\nError: Cannot prepare Enterprise features. Exiting...\n\n");
        return false;        
    }

    
    uint32_t retry_log_time=0;
    int seed_attempt=1;
    if(init_privkey.size())
    {
        if(mc_gState->m_NetworkParams->m_Status == MC_PRM_STATUS_EMPTY) 
        {
            seed_attempt=2;            
        }
    }

    bool first_attempt=true;
    while(seed_attempt)
    {        
        
        if((mc_gState->m_NetworkParams->m_Status == MC_PRM_STATUS_EMPTY) 
           || (mc_gState->m_NetworkParams->m_Status == MC_PRM_STATUS_MINIMAL))
        {
            
            if(mc_gState->m_NetworkParams->m_Status == MC_PRM_STATUS_MINIMAL)
            {
                InitializeMultiChainParams();                    
                if(init_privkey.size())
                {
                    if(seed_attempt == 1)
                    {
                        if(mc_gState->m_NetworkParams->GetParam("privatekeyversion",NULL) == NULL)
                        {
                            return InitError(_("The initprivkey runtime parameter can only be used when connecting to MultiChain 1.0 beta 2 or later"));                                                        
                        }
                        string init_privkey_error=pwalletMain->SetDefaultKeyIfInvalid(init_privkey);
                        if(init_privkey_error.size())
                        {
                            return InitError(strprintf("Cannot set initial private key: %s",init_privkey_error));                            
                        }
                        init_privkey="";
                    }
                }
            }

            if(seed_node)
            {
                if(seed_attempt == 1)
                {
                    if(!GetBoolArg("-shortoutput", false))
                    {    
                        if(first_attempt)
                        {
                            sprintf(bufOutput,"Retrieving blockchain parameters from the seed node %s ...\n",seed_node);
                            bytes_written=write(OutputPipe,bufOutput,strlen(bufOutput));
                        }
                    }
                }
            }

            LogPrintf("mchn: Parameter set is not complete - starting paramset discovery thread...\n");
            boost::thread_group seedThreadGroup;

            mc_gState->m_NetworkState=MC_NTS_WAITING_FOR_SEED;

            string seed_ip=seed_node;
            int seed_port=0;
            stringstream ss(seed_ip); 
            string tok;
            int size;

            seed_ip=mc_ParseIPPort(seed_ip,&seed_port);
/*            
            if(seed_port<=0)
            {
                seed_port=GetListenPort();
            }
*/            
/*            
            if(getline(ss, tok, ':'))
            {
                seed_ip=tok;
                if(getline(ss, tok, ':'))
                {
                    seed_port=tok;
                }
            }            
*/
            
            if(pNodeStatus == NULL)
            {
                LOCK(cs_NodeStatus);
                pNodeStatus=new CInitNodeStatus;
                pNodeStatus->fInitialized=false;
                pNodeStatus->sLastError="First connection attempt";
                pNodeStatus->tStartConnectTime=SeedStartTime;
                pNodeStatus->sSeedIP=seed_ip;
                pNodeStatus->nSeedPort=seed_port;
            }
            
            if(mc_QuerySeed(seedThreadGroup,seed_resolved.c_str()))
            {
                LOCK(cs_NodeStatus);
                if((mc_gState->m_NetworkState == MC_NTS_SEED_READY) || (mc_gState->m_NetworkState == MC_NTS_SEED_NO_PARAMS) )
                {
                    seed_error="Couldn't disconnect from the seed node, please restart multichaind";
                    pNodeStatus->sLastError="Disconnect error";
                }
                else
                {
//                    if(seed_port.size() == 0)
                    if(seed_port == 0)
                    {
                        seed_error=strprintf("Couldn't connect to the seed node %s - please specify port number explicitly.",seed_node);                
                        pNodeStatus->sLastError="Couldn't connect to the seed node, missing port";
                    }
                    else
                    {
                        seed_error=strprintf("Couldn't connect to the seed node %s on port %d - please check multichaind is running at that address and that your firewall settings allow incoming connections.",                
                            seed_ip.c_str(),seed_port);
                        pNodeStatus->sLastError="Couldn't connect to the seed node, multichaind is not running or firewall issue";
                    }
                }
            }
            else
            {
                first_attempt=false;
                LOCK(cs_NodeStatus);
                if(mc_gState->m_NetworkParams->m_Status == MC_PRM_STATUS_VALID)
                {
                    pNodeStatus->sLastError="";
                    pNodeStatus->fInitialized=true;                    
                }
                if(mc_gState->m_NetworkParams->m_Status == MC_PRM_STATUS_EMPTY)
                {
                    pNodeStatus->sLastError="Couldn't retrieve blockchain parameters from the seed node";
                }
                if(mc_gState->m_NetworkParams->m_Status == MC_PRM_STATUS_MINIMAL)
                {
                    if(seed_attempt == 1)
                    {
                        pNodeStatus->sLastError="Couldn't initialize node, no connect permission";
                    }
                    else
                    {
                        pNodeStatus->sLastError="Second connection attempt, trying -initprivkey";                        
                    }
                }                
            }

            if(mc_gState->m_NetworkParams->GetParam("protocolversion",&size) != NULL)
            {
                int protocol_version=(int)mc_gState->m_NetworkParams->GetInt64Param("protocolversion");
            
                if(mc_gState->IsSupported(protocol_version) == 0) 
                {
                    if(mc_gState->IsDeprecated(protocol_version))
                    {
                        seed_error=strprintf("The protocol version (%d) for blockchain %s has been deprecated and was last supported in MultiChain %s\n",                
                                protocol_version, mc_gState->m_Params->NetworkName(),
                                mc_BuildDescription(-mc_gState->VersionInfo(protocol_version)));                    
                        return InitError(seed_error);                                
                    }
                    else
                    {
                        seed_error=strprintf("Couldn't connect to the seed node %s on port %d.\n"
                                    "Blockchain was created by multichaind with newer protocol version (%d)\n"                
                                    "Please upgrade to the latest version of MultiChain or connect only to blockchains using protocol version %d or earlier.\n",                
                                seed_ip.c_str(),seed_port,protocol_version, mc_gState->GetProtocolVersion());                        
                    }
                }
            }
                    
            if(mc_gState->m_NetworkState == MC_NTS_SEED_NO_PARAMS)
            {
                char fileName[MC_DCT_DB_MAX_PATH];
                mc_GetFullFileName(mc_gState->m_Params->NetworkName(),"params", ".dat",MC_FOM_RELATIVE_TO_DATADIR,fileName);
                seed_error=strprintf("Couldn't retrieve blockchain parameters from the seed node %s on port %d.\n"
                            "For bitcoin protocol blockchains, the file %s must be copied manually from an existing node.",                
                        seed_ip.c_str(),seed_port,fileName);

            }
            
            LogPrintf("mchn: Exited from paramset discovery thread\n");        

            if(mc_gState->m_NetworkParams->m_Status == MC_PRM_STATUS_VALID)
            {
                SelectMultiChainParams(mc_gState->m_Params->NetworkName());
                delete mc_gState->m_Permissions;
                mc_gState->m_Permissions= new mc_Permissions;
                if(mc_gState->m_Permissions->Initialize(mc_gState->m_Params->NetworkName(),0))                                
                {
                    seed_error="Couldn't initialize permission database with retrieved parameters\n";
                }            
            }

        }
        else
        {
            if(mc_gState->m_NetworkParams->m_Status == MC_PRM_STATUS_GENERATED)
            {
                if(init_privkey.size())
                {
                    if(mc_gState->m_NetworkParams->GetParam("privatekeyversion",NULL) == NULL)
                    {
                        return InitError(_("The initprivkey runtime parameter can only be used when connecting to MultiChain 1.0 beta 2 or later"));                                                        
                    }
                    string init_privkey_error=pwalletMain->SetDefaultKeyIfInvalid(init_privkey);
                    if(init_privkey_error.size())
                    {
                        return InitError(strprintf("Cannot set initial private key: %s",init_privkey_error));                            
                    }
                    init_privkey="";
                }
                const unsigned char *pubKey=pwalletMain->vchDefaultKey.begin();
                int pubKeySize=pwalletMain->vchDefaultKey.size();

                LogPrintf("mchn: Parameter set is new, THIS IS GENESIS NODE - looking for genesis block...\n");
                if(!GetBoolArg("-shortoutput", false))
                {    
                    sprintf(bufOutput,"Looking for genesis block...\n");
                    bytes_written=write(OutputPipe,bufOutput,strlen(bufOutput));                
                }
                mc_gState->m_NetworkParams->SetGlobals();                           // Needed to update IsProtocolMultichain flag in case of bitcoin
                if(mc_gState->m_NetworkParams->Build(pubKey,pubKeySize))
                {
                    return InitError(_("Cannot build new blockchain"));
                }
                LogPrintf("mchn: Genesis block found\n");
                if(!GetBoolArg("-shortoutput", false))
                {    
                    sprintf(bufOutput,"Genesis block found\n\n");
                    bytes_written=write(OutputPipe,bufOutput,strlen(bufOutput));                
                }

                mc_gState->m_NetworkParams->Validate();        

                if(mc_gState->m_NetworkParams->m_Status != MC_PRM_STATUS_VALID)
                {
                    return InitError(_("Invalid parameter set"));
                }                        
            }
            else
            {
                if(seed_node)
                {
                    if(!GetBoolArg("-shortoutput", false))
                    {    
                        sprintf(bufOutput,"Chain %s already exists, adding %s to list of peers\n\n",mc_gState->m_NetworkParams->Name(),seed_node);
                        bytes_written=write(OutputPipe,bufOutput,strlen(bufOutput));
                    }                    
                }
            }            
        }
        seed_attempt--;
        if(seed_attempt)
        {
            if(mc_gState->m_NetworkParams->m_Status == MC_PRM_STATUS_EMPTY)            
            {
                seed_attempt--;                
            }
        }
        if(seed_attempt == 0)
        {
            fPauseLogPrint=false;
            if((mc_gState->m_NetworkParams->m_Status == MC_PRM_STATUS_EMPTY) 
               || (mc_gState->m_NetworkParams->m_Status == MC_PRM_STATUS_MINIMAL))
            {
                uint32_t retry_time=mc_TimeNowAsUInt();
                if(retry_time < SeedStopTime)
                {
                    if(!ShutdownRequested())
                    {
                        if(mc_gState->m_NetworkParams->m_Status == MC_PRM_STATUS_MINIMAL)
                        {
                            InitializeMultiChainParams();        
                        }
                        
                        if(!grant_message_printed)
                        {
                            grant_message_printed=GrantMessagePrinted(OutputPipe,first_attempt);
                        }
                        if(first_attempt)
                        {
                            if(!GetBoolArg("-shortoutput", false))
                            {    
                                sprintf(bufOutput,"You can use getinitstatus API to see current initialization status\n\n");
                                bytes_written=write(OutputPipe,bufOutput,strlen(bufOutput));
                            }                            
                        }
                        if(rpc_with_default_port)
                        {
                            if(mc_gState->m_NetworkParams->m_Status == MC_PRM_STATUS_MINIMAL)
                            {
                                LogPrintf("Restarting RPC server...\n");
                                SelectMultiChainParams(mc_gState->m_Params->NetworkName());
                                StopRPCThreads();
                                if (fServer)
                                {
                                    JSON_DOUBLE_DECIMAL_DIGITS=GetArg("-apidecimaldigits",-1);        
                            //        uiInterface.InitMessage.connect(SetRPCWarmupStatus);
                                    StartRPCThreads(rpc_threads_error);
                                }
                                if(rpc_threads_error.size())
                                {
                                    if(!GetBoolArg("-shortoutput", false))
                                    {    
                                        sprintf(bufOutput,"%s\n",rpc_threads_error.c_str());                            
                                        bytes_written=write(OutputPipe,bufOutput,strlen(bufOutput));        
                                    }            
                                }
                                rpc_with_default_port=false;
                            }     
                        }
                        SetRPCWarmupStatus("Node waiting to successfully initialize, only getinitstatus command is available.");
                        seed_attempt++;
                        __US_Sleep(2000);
                        if(retry_time >= retry_log_time)
                        {
                            retry_log_time=retry_time+60;
                            if(retry_log_time > SeedStopTime-20)
                            {
                                retry_log_time=SeedStopTime-20;
                            }
                        }
                        else
                        {
                            fPauseLogPrint=true;
                        }
                    }
                }
            }
        }
        first_attempt=false;
    }
    fPauseLogPrint=false;

    if(pNodeStatus)
    {
        LOCK(cs_NodeStatus);
        delete pNodeStatus;
        pNodeStatus=NULL;        
    }
    
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
        
        string strBurnAddress=BurnAddress(Params().Base58Prefix(CChainParams::PUBKEY_ADDRESS)); // Caching burn address
        LogPrint("mchn","mchn: Burn address: %s\n",strBurnAddress.c_str());                
        
        wallet_mode=GetArg("-walletdbversion",foundwalletdatversion);
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
                        case MC_TET_ASSET:
                            vSubscribedEntities.push_back(stat->m_Entity);
                            break;
                    }
                }
                __US_Sleep(1000);
                rescan_subscriptions=new mc_Buffer;
                mc_EnterpriseFeatures* pRescanEF = new mc_EnterpriseFeatures;
                if(pRescanEF->Prepare())
                {
                    return InitError(_("Couldn't initialize Enterprise features")); 
                }
                
                if(pRescanEF->Initialize(mc_gState->m_Params->NetworkName(),0))
                {
                    return InitError(_("Couldn't initialize Enterprise features")); 
                }
                pRescanEF->STR_GetSubscriptions(rescan_subscriptions);
                pRescanEF->Destroy();
                delete pRescanEF;                
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
                if(GetBoolArg("-nochunkflush",false))
                {
                    mc_gState->m_WalletMode |= MC_WMD_NO_CHUNK_FLUSH;
                }
                
                mc_gState->m_WalletMode |= mc_AutosubscribeWalletMode(GetArg("-autosubscribe","none"),false);
/*                
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
*/
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
                            pwalletTxsMain->AddEntity(&(entstat.m_Entity),flags);
                            entstat.m_Entity.m_EntityType=MC_TET_PUBKEY_ADDRESS | MC_TET_TIMERECEIVED;
                            pwalletTxsMain->AddEntity(&(entstat.m_Entity),flags);
                        }
                    }
                        
                    if(lpScriptID)
                    {
                        memcpy(entstat.m_Entity.m_EntityID,lpScriptID,MC_TDB_ENTITY_ID_SIZE);
                        entstat.m_Entity.m_EntityType=MC_TET_SCRIPT_ADDRESS | MC_TET_CHAINPOS;
                        if(!pwalletTxsMain->FindEntity(&entstat))
                        {
                            pwalletTxsMain->AddEntity(&(entstat.m_Entity),flags);
                            entstat.m_Entity.m_EntityType=MC_TET_SCRIPT_ADDRESS | MC_TET_TIMERECEIVED;
                            pwalletTxsMain->AddEntity(&(entstat.m_Entity),flags);
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
    else
    {
        if(mc_gState->m_NetworkParams->m_Status == MC_PRM_STATUS_MINIMAL)
        {
            InitializeMultiChainParams();        

            if(seed_node)
            {
                FILE *fileHan;

                fileHan=mc_OpenFile(mc_gState->m_NetworkParams->Name(),"seed",".dat","w",MC_FOM_RELATIVE_TO_DATADIR);
                if(fileHan)
                {
                    fprintf(fileHan,"seed=%s\n",seed_node);
                    mc_CloseFile(fileHan);                    
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
                if(pwalletMain->vchDefaultKey.IsValid())
                {
/*                    
                    LogPrintf("mchn: Minimal blockchain parameter set is created, default address: %s\n",
                            CBitcoinAddress(pwalletMain->vchDefaultKey.GetID()).ToString().c_str());
 */ 
                    if(fFirstRunForBuild)
                    {
                        if (!pwalletMain->SetAddressBook(pwalletMain->vchDefaultKey.GetID(), "", "receive"))
                            strErrors << _("Cannot write default address") << "\n";
                    }
                    
                    if(!grant_message_printed)
                    {
                        grant_message_printed=GrantMessagePrinted(OutputPipe,false);
                    }
/*                    
                    if(!GetBoolArg("-shortoutput", false))
                    {    
                        sprintf(bufOutput,"Blockchain successfully initialized.\n\n");             
                        bytes_written=write(OutputPipe,bufOutput,strlen(bufOutput));
                        sprintf(bufOutput,"Please ask blockchain admin or user having activate permission to let you connect and/or transact:\n");
                        bytes_written=write(OutputPipe,bufOutput,strlen(bufOutput));

                        sprintf(bufOutput,"multichain-cli %s grant %s connect\n",mc_gState->m_NetworkParams->Name(),
                             CBitcoinAddress(pwalletMain->vchDefaultKey.GetID()).ToString().c_str());
                        bytes_written=write(OutputPipe,bufOutput,strlen(bufOutput));
                        sprintf(bufOutput,"multichain-cli %s grant %s connect,send,receive\n\n",mc_gState->m_NetworkParams->Name(),
                             CBitcoinAddress(pwalletMain->vchDefaultKey.GetID()).ToString().c_str());
                        bytes_written=write(OutputPipe,bufOutput,strlen(bufOutput));
                    }
                    else
                    {
                        sprintf(bufOutput,"%s\n",CBitcoinAddress(pwalletMain->vchDefaultKey.GetID()).ToString().c_str());                            
                        bytes_written=write(OutputPipe,bufOutput,strlen(bufOutput));
                    }
 */ 
                    return false;
                }
            }
        }    
        if(seed_error.size())
        {
            return InitError(_(seed_error.c_str()));        
        }
        return InitError(_("Invalid parameter set"));        
    }
    
    mc_gState->m_NodePausedState=GetArg("-paused",0);
    LogPrintf("Node paused state is set to %08X\n",mc_gState->m_NodePausedState);
    
    pwalletMain=NULL;

/*    
    string rpc_threads_error="";
    if (fServer)
    {
        JSON_DOUBLE_DECIMAL_DIGITS=GetArg("-apidecimaldigits",-1);        
        uiInterface.InitMessage.connect(SetRPCWarmupStatus);
        StartRPCThreads(rpc_threads_error);
    }
 */ 
/* MCHN END*/        
    
        ::minRelayTxFee = CFeeRate(MIN_RELAY_TX_FEE); 
        if (mapArgs.count("-mintxfee"))
        {
            CAmount n = 0;
            if (ParseMoney(mapArgs["-mintxfee"], n) && n > 0)
                CWallet::minTxFee = CFeeRate(n);
            else
                return InitError(strprintf(_("Invalid amount for -mintxfee=<amount>: '%s'"), mapArgs["-mintxfee"]));
        }
        if (mapArgs.count("-paytxfee"))
        {
            CAmount nFeePerK = 0;
            if (!ParseMoney(mapArgs["-paytxfee"], nFeePerK))
                return InitError(strprintf(_("Invalid amount for -paytxfee=<amount>: '%s'"), mapArgs["-paytxfee"]));
            if (nFeePerK > nHighTransactionFeeWarning)
                InitWarning(_("Warning: -paytxfee is set very high! This is the transaction fee you will pay if you send a transaction."));
            payTxFee = CFeeRate(nFeePerK, 1000);
            if (payTxFee < ::minRelayTxFee)
            {
                return InitError(strprintf(_("Invalid amount for -paytxfee=<amount>: '%s' (must be at least %s)"),
                                           mapArgs["-paytxfee"], ::minRelayTxFee.ToString()));
            }
        }
        if (mapArgs.count("-maxtxfee"))
        {
            CAmount nMaxFee = 0;
            if (!ParseMoney(mapArgs["-maxtxfee"], nMaxFee))
                return InitError(strprintf(_("Invalid amount for -maxtxfee=<amount>: '%s'"), mapArgs["-maptxfee"]));
            if (nMaxFee > nHighTransactionMaxFeeWarning)
                InitWarning(_("Warning: -maxtxfee is set very high! Fees this large could be paid on a single transaction."));
            maxTxFee = nMaxFee;
            if (CFeeRate(maxTxFee, 1000) < ::minRelayTxFee)
            {
                return InitError(strprintf(_("Invalid amount for -maxtxfee=<amount>: '%s' (must be at least the minrelay fee of %s to prevent stuck transactions)"),
                                           mapArgs["-maxtxfee"], ::minRelayTxFee.ToString()));
            }
        }
        
#endif // ENABLE_WALLET
    // ********************************************************* Step 6: network initialization

//    RegisterNodeSignals(GetNodeSignals());

    if (mapArgs.count("-onlynet")) {
        std::set<enum Network> nets;
        BOOST_FOREACH(std::string snet, mapMultiArgs["-onlynet"]) {
            enum Network net = ParseNetwork(snet);
            if (net == NET_UNROUTABLE)
                return InitError(strprintf(_("Unknown network specified in -onlynet: '%s'"), snet));
            nets.insert(net);
        }
        for (int n = 0; n < NET_MAX; n++) {
            enum Network net = (enum Network)n;
            if (!nets.count(net))
                SetLimited(net);
        }
    }

    if (mapArgs.count("-whitelist")) {
        BOOST_FOREACH(const std::string& net, mapMultiArgs["-whitelist"]) {
            CSubNet subnet(net);
            if (!subnet.IsValid())
                return InitError(strprintf(_("Invalid netmask specified in -whitelist: '%s'"), net));
            CNode::AddWhitelistedRange(subnet);
        }
    }

    CService addrProxy;
    bool fProxy = false;
    if (mapArgs.count("-proxy")) {
        addrProxy = CService(mapArgs["-proxy"], 9050);
        if (!addrProxy.IsValid())
            return InitError(strprintf(_("Invalid -proxy address: '%s'"), mapArgs["-proxy"]));

        SetProxy(NET_IPV4, addrProxy);
        SetProxy(NET_IPV6, addrProxy);
        SetNameProxy(addrProxy);
        fProxy = true;
    }

    // -onion can override normal proxy, -noonion disables tor entirely
    if (!(mapArgs.count("-onion") && mapArgs["-onion"] == "0") &&
        (fProxy || mapArgs.count("-onion"))) {
        CService addrOnion;
        if (!mapArgs.count("-onion"))
            addrOnion = addrProxy;
        else
            addrOnion = CService(mapArgs["-onion"], 9050);
        if (!addrOnion.IsValid())
            return InitError(strprintf(_("Invalid -onion address: '%s'"), mapArgs["-onion"]));
        SetProxy(NET_TOR, addrOnion);
        SetReachable(NET_TOR);
    }

    // see Step 2: parameter interactions for more information about these
    fListen = GetBoolArg("-listen", DEFAULT_LISTEN);
    fDiscover = GetBoolArg("-discover", true);
    fNameLookup = GetBoolArg("-dns", true);

    if(GetBoolArg("-offline",false))
    {
        fListen=false;
    }
    
    bool fBound = false;
    bool fThisBound;
    mc_gState->m_IPv4Address=0;    
    if (fListen) {
        if (mapArgs.count("-bind") || mapArgs.count("-whitebind")) {
            BOOST_FOREACH(std::string strBind, mapMultiArgs["-bind"]) {
                CService addrBind;
                if (!Lookup(strBind.c_str(), addrBind, GetListenPort(), false))
                    return InitError(strprintf(_("Cannot resolve -bind address: '%s'"), strBind));
/* MCHN START */                
                fThisBound = Bind(addrBind, (BF_EXPLICIT | BF_REPORT_ERROR));
                fBound |= fThisBound;
                if(fThisBound)
                {
                    if(mc_gState->m_IPv4Address == 0)
                    {
                        mc_SetIPv4ServerAddress(strBind.c_str());
                    }
                }
/* MCHN END */                
            }
            BOOST_FOREACH(std::string strBind, mapMultiArgs["-whitebind"]) {
                CService addrBind;
                if (!Lookup(strBind.c_str(), addrBind, 0, false))
                    return InitError(strprintf(_("Cannot resolve -whitebind address: '%s'"), strBind));
                if (addrBind.GetPort() == 0)
                    return InitError(strprintf(_("Need to specify a port with -whitebind: '%s'"), strBind));
                fBound |= Bind(addrBind, (BF_EXPLICIT | BF_REPORT_ERROR | BF_WHITELIST));
            }
        }
        else {
            struct in_addr inaddr_any;
            inaddr_any.s_addr = INADDR_ANY;
            fBound |= Bind(CService(in6addr_any, GetListenPort()), BF_NONE);
            fBound |= Bind(CService(inaddr_any, GetListenPort()), !fBound ? BF_REPORT_ERROR : BF_NONE);
        }
        if (!fBound)
            return InitError(_("Failed to listen on any port. Use -listen=0 if you want this."));
    }
/* MCHN START */    
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
    
    
    pRelayManager=new mc_RelayManager;
    
    int max_ips=64;
    uint32_t all_ips[64];
    int found_ips=1;
    if(mc_gState->m_IPv4Address == 0)
    {
        found_ips=mc_FindIPv4ServerAddress(all_ips,max_ips);
    }
    else
    {
        all_ips[0]=mc_gState->m_IPv4Address;
    }
    pRelayManager->SetMyIPs(all_ips,found_ips);
    
    if(!GetBoolArg("-shortoutput", false))
    {
        if(fListen && !GetBoolArg("-offline",false))
        {
            sprintf(bufOutput,"Other nodes can connect to this node using:\n");
            bytes_written=write(OutputPipe,bufOutput,strlen(bufOutput));
            sprintf(bufOutput,"multichaind %s:%d\n\n",MultichainServerAddress(false).c_str(),GetListenPort());
            bytes_written=write(OutputPipe,bufOutput,strlen(bufOutput));
            if(found_ips > 1)
            {
                sprintf(bufOutput,"This host has multiple IP addresses, so from some networks:\n");
                bytes_written=write(OutputPipe,bufOutput,strlen(bufOutput));
                for(int i_ips=0;i_ips<found_ips;i_ips++)
                {
                    if(all_ips[i_ips] != mc_gState->m_IPv4Address)
                    {
                        unsigned char *ptr;
                        ptr=(unsigned char *)(all_ips+i_ips);
                        sprintf(bufOutput,"multichaind %s@%u.%u.%u.%u:%d\n",mc_gState->m_NetworkParams->Name(),ptr[3],ptr[2],ptr[1],ptr[0],GetListenPort());
                        bytes_written=write(OutputPipe,bufOutput,strlen(bufOutput));
                        if(bytes_written != strlen(bufOutput))
                        {
                            found_ips=0;
                        }
                    }                
                }        
                sprintf(bufOutput,"\n");
                bytes_written=write(OutputPipe,bufOutput,strlen(bufOutput));
            }
            if (mapArgs.count("-externalip")) 
            {            
                sprintf(bufOutput,"Based on the -externalip setting, this node is reachable at:\n");
                bytes_written=write(OutputPipe,bufOutput,strlen(bufOutput));
                BOOST_FOREACH(string strAddr, mapMultiArgs["-externalip"]) 
                {
                    int port;
                    string s_ip=mc_ParseIPPort(strAddr,&port);
                    if(port>0)
                    {
                        sprintf(bufOutput,"multichaind %s@%s\n",mc_gState->m_NetworkParams->Name(),strAddr.c_str());
                        bytes_written=write(OutputPipe,bufOutput,strlen(bufOutput));
                        port=GetListenPort();
                    }
                    else
                    {
                        sprintf(bufOutput,"multichaind %s@%s:%d\n",mc_gState->m_NetworkParams->Name(),strAddr.c_str(),GetListenPort());
                        bytes_written=write(OutputPipe,bufOutput,strlen(bufOutput));                    
                    }
                }
                sprintf(bufOutput,"\n");
                bytes_written=write(OutputPipe,bufOutput,strlen(bufOutput));
            }
        }
        else
        {
            if(GetBoolArg("-offline",false))
            {                
                sprintf(bufOutput,"MultiChain started in offline mode, other nodes cannot connect.\n\n");
                bytes_written=write(OutputPipe,bufOutput,strlen(bufOutput));            
            }
            else
            {
                sprintf(bufOutput,"Other nodes cannot connect to this node because the runtime parameter listen=0\n\n");
                bytes_written=write(OutputPipe,bufOutput,strlen(bufOutput));            
            }
        }
    }
    else
    {
        sprintf(bufOutput,"%s:%d\n",MultichainServerAddress(true).c_str(),GetListenPort());                
        bytes_written=write(OutputPipe,bufOutput,strlen(bufOutput));
    }

    
/* MCHN END */    

    if (mapArgs.count("-externalip")) {
        BOOST_FOREACH(string strAddr, mapMultiArgs["-externalip"]) {
            int port;
            string s_ip=mc_ParseIPPort(strAddr,&port);
            if(port<=0)
            {
                port=GetListenPort();
            }
            
//            CService addrLocal(strAddr, GetListenPort(), fNameLookup);
            CService addrLocal(s_ip, port, fNameLookup);
            if (!addrLocal.IsValid())
                return InitError(strprintf(_("Cannot resolve -externalip address: '%s'"), strAddr));
//            AddLocal(CService(strAddr, GetListenPort(), fNameLookup), LOCAL_MANUAL);
            AddLocal(CService(s_ip, port, fNameLookup), LOCAL_MANUAL);
        }
    }

    BOOST_FOREACH(string strDest, mapMultiArgs["-seednode"])
        AddOneShot(strDest);

    // ********************************************************* Step 7: load block chain

/* MCHN START */    
    std::string strBannedTxError=SetBannedTxs(GetArg("-bantx",""));
    if(strBannedTxError.size())    
    {
        return InitError(strBannedTxError);        
    }
/* MCHN END */    
    
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
                    if(strLoadError.size() == 0)
                    {
                        strLoadError = _("Error loading block database");                        
                    }
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
                    return InitError(_("Incorrect or no genesis block found. Wrong datadir for network?"));

                // Initialize the block index (no-op if non-empty database was already loaded)
                if (!InitBlockIndex()) {
                    strLoadError = _("Error initializing block database");
                    break;
                }

                CBlock &genesis_block = const_cast<CBlock&>(Params().GenesisBlock());
                GenesisBlockSize=::GetSerializeSize(genesis_block, SER_DISK, CLIENT_VERSION);
                
                // Check for changed -txindex state
/* MCHN START */    
/* Default was false */    
                if (fTxIndex != GetBoolArg("-txindex", true)) {
                    strLoadError = _("You need to rebuild the database using -reindex to change -txindex");
                    break;
                }
/* MCHN END */    

                uiInterface.InitMessage(_("Verifying blocks..."));
                if (!CVerifyDB().VerifyDB(pcoinsdbview, GetArg("-checklevel", 3),
                              GetArg("-checkblocks", 288))) {
                    strLoadError = _("Corrupted block database detected");
                    break;
                }
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

    pEF->ENT_MaybeStop();
    // As LoadBlockIndex can take several minutes, it's possible the user
    // requested to kill the GUI during the last operation. If so, exit.
    // As the program has not fully started yet, Shutdown() is possibly overkill.
    if (fRequestShutdown)
    {
        LogPrintf("Shutdown requested. Exiting.\n");
        return false;
    }
    LogPrintf(" block index %15dms\n", GetTimeMillis() - nStart);

    boost::filesystem::path est_path = GetDataDir() / FEE_ESTIMATES_FILENAME;
    CAutoFile est_filein(fopen(est_path.string().c_str(), "rb"), SER_DISK, CLIENT_VERSION);
    // Allowed to fail as this file IS missing on first startup.
    if (!est_filein.IsNull())
        mempool.ReadFeeEstimates(est_filein);
    fFeeEstimatesInitialized = true;

    if(mapMultiArgs.count("-rpcallowip") == 0)
    {
        if(!GetBoolArg("-shortoutput", false))
        {    
            sprintf(bufOutput,"Listening for API requests on port %d (local only - see rpcallowip setting)\n\n",(int)GetArg("-rpcport", BaseParams().RPCPort()));                            
            bytes_written=write(OutputPipe,bufOutput,strlen(bufOutput));        
        }
    }
/*    
    if(rpc_threads_error.size())
    {
        if(!GetBoolArg("-shortoutput", false))
        {    
            sprintf(bufOutput,"%s\n",rpc_threads_error.c_str());                            
            bytes_written=write(OutputPipe,bufOutput,strlen(bufOutput));        
        }            
    }
*/    
//    int version=mc_gState->m_NetworkParams->GetInt64Param("protocolversion");
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
    
        
    if(pEF->Initialize(mc_gState->m_Params->NetworkName(),0))
    {
        fprintf(stderr,"\nError: Couldn't initialize Enterprise features, please see debug.log for details. Exiting...\n\n");
        return false;        
    }
    if(rescan_subscriptions)
    {
        pEF->STR_PutSubscriptions(rescan_subscriptions);            
        delete rescan_subscriptions;
    }            

    
    // ********************************************************* Step 8: load wallet
#ifdef ENABLE_WALLET
    if (fDisableWallet) {
        pwalletMain = NULL;
        LogPrintf("Wallet disabled!\n");
    } else {

        // needed to restore wallet transaction meta data after -zapwallettxes
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
/* MCHN START */  
        pwalletTxsMain->BindWallet(pwalletMain);
/* MCHN END */        
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
/* MCHN START */  
        pwalletMain->lpWalletTxs=pwalletTxsMain;
        pwalletMain->InitializeUnspentList();

        {
            LOCK(cs_main);
            uint32_t paused=mc_gState->m_NodePausedState;
            mc_gState->m_NodePausedState=MC_NPS_MINING | MC_NPS_INCOMING;
            std::string strLockBlockError=SetLockedBlock(GetArg("-lockblock",""));
            mc_gState->m_NodePausedState=paused;
            if(strLockBlockError.size())    
            {
                return InitError(strLockBlockError);        
            }
        }
    
        
/* MCHN END */        
    } // (!fDisableWallet)
#else // ENABLE_WALLET
    LogPrintf("No wallet compiled in!\n");
#endif // !ENABLE_WALLET
    // ********************************************************* Step 9: import blocks

    if (mapArgs.count("-blocknotify"))
        uiInterface.NotifyBlockTip.connect(BlockNotifyCallback);

    // scan for better chains in the block chain database, that are not yet connected in the active best chain
    CValidationState state;
    if (!ActivateBestChain(state))
        strErrors << "Failed to connect best block";

    std::vector<boost::filesystem::path> vImportFiles;
    if (mapArgs.count("-loadblock"))
    {
        BOOST_FOREACH(string strFile, mapMultiArgs["-loadblock"])
            vImportFiles.push_back(strFile);
    }
    if(!GetBoolArg("-offline",false))
    {    
        threadGroup.create_thread(boost::bind(&ThreadImport, vImportFiles));
        if (chainActive.Tip() == NULL) {
            LogPrintf("Waiting for genesis block to be imported...\n");
            while (!fRequestShutdown && chainActive.Tip() == NULL)
                MilliSleep(10);
        }
        chainActive.Genesis()->nSize=GenesisBlockSize;
    }

    // ********************************************************* Step 10: start node

    if (!CheckDiskSpace())
        return false;

    if (!strErrors.str().empty())
        return InitError(strErrors.str());

    pRelayManager->Initialize();

    
//    RandAddSeedPerfmon();

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
    
    if(!GetBoolArg("-offline",false))
    {
        StartNode(threadGroup);

#ifdef ENABLE_WALLET
    // Generate coins in the background
        if (pwalletMain)
            GenerateBitcoins(GetBoolArg("-gen", true), pwalletMain, GetArg("-genproclimit", 1));
#endif

    // ********************************************************* Step 11: finished
/*
    SetRPCWarmupFinished();
    uiInterface.InitMessage(_("Done loading"));
*/    
#ifdef ENABLE_WALLET
        if (pwalletMain) {
            // Add wallet transactions that aren't already in a block to mapTransactions
            pwalletMain->ReacceptWalletTransactions();
        }
#endif
    }
    
#ifdef ENABLE_WALLET
    if (pwalletMain) {
        // Run a thread to flush wallet periodically
        threadGroup.create_thread(boost::bind(&ThreadFlushWalletDB, boost::ref(pwalletMain->strWalletFile)));
    }
#endif

    pEF->LIC_VerifyLicenses(0);
    
    vector<string> conflicting_licenses=pEF->LIC_LicensesWithStatus("conflicting");
    
    for(unsigned int l=0;l<conflicting_licenses.size();l++)
    {
        if(!GetBoolArg("-shortoutput", false))
        {    
            sprintf(bufOutput,"The license %s is available to this node but will not be used automatically, because it appears it was not previously in use.\n",
                    conflicting_licenses[l].c_str());
            bytes_written=write(OutputPipe,bufOutput,strlen(bufOutput));        
            sprintf(bufOutput,"To use this license for this node and stop any other from using it, use the 'takelicense %s' command.\n\n",
                    conflicting_licenses[l].c_str());                    
            bytes_written=write(OutputPipe,bufOutput,strlen(bufOutput));        
        }        
    }
    
    if(crash_recovery_required)
    {
        LogPrintf("Node didn't shut down normally, performing recovery\n");
        if(!GetBoolArg("-shortoutput", false))
        {    
            sprintf(bufOutput,"Node didn't shut down normally, performing recovery\n\n");
            bytes_written=write(OutputPipe,bufOutput,strlen(bufOutput));
        }        
        fInRecovery=true;
        bool success=RecoverAfterCrash();
        fInRecovery=false;
        if(success)
        {
            LogPrintf("Recovery completed succesfully\n");            
        }
        else
        {
            LogPrintf("Recovery failed\n");                        
            if(!GetBoolArg("-shortoutput", false))
            {    
                sprintf(bufOutput,"Recovery failed\n\n");
                bytes_written=write(OutputPipe,bufOutput,strlen(bufOutput));
            }        
        }
    }
    
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
