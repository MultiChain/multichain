// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#if defined(HAVE_CONFIG_H)
#include "config/bitcoin-config.h"
#endif

#include "net/net.h"

#include "storage/addrman.h"
#include "chainparams/chainparams.h"
#include "version/clientversion.h"
#include "primitives/transaction.h"
#include "ui/ui_interface.h"

#include "structs/base58.h"
#include "keys/key.h"
#include "keys/pubkey.h"
#include "wallet/wallet.h"
extern CWallet* pwalletMain;
#include "multichain/multichain.h"
#include "community/community.h"

#ifdef WIN32
#include <string.h>
#else
#include <fcntl.h>
#endif

#ifdef USE_UPNP
#include <miniupnpc/miniupnpc.h>
#include <miniupnpc/miniwget.h>
#include <miniupnpc/upnpcommands.h>
#include <miniupnpc/upnperrors.h>
#endif

#include <boost/filesystem.hpp>
#include <boost/thread.hpp>

// Dump addresses to peers.dat every 15 minutes (900s)
#define DUMP_ADDRESSES_INTERVAL 900

#if !defined(HAVE_MSG_NOSIGNAL) && !defined(MSG_NOSIGNAL)
#define MSG_NOSIGNAL 0
#endif

// Fix for ancient MinGW versions, that don't have defined these in ws2tcpip.h.
// Todo: Can be removed when our pull-tester is upgraded to a modern MinGW version.
#ifdef WIN32
#ifndef PROTECTION_LEVEL_UNRESTRICTED
#define PROTECTION_LEVEL_UNRESTRICTED 10
#endif
#ifndef IPV6_PROTECTION_LEVEL
#define IPV6_PROTECTION_LEVEL 23
#endif
#endif

using namespace boost;
using namespace std;

namespace {
    const int MAX_OUTBOUND_CONNECTIONS = 8;

    struct ListenSocket {
        SOCKET socket;
        bool whitelisted;

        ListenSocket(SOCKET socket, bool whitelisted) : socket(socket), whitelisted(whitelisted) {}
    };
}

//
// Global state variables
//
bool fDiscover = true;
bool fListen = true;
uint64_t nLocalServices = NODE_NETWORK;
uint64_t nLocalMultiChainServices = 0;
CCriticalSection cs_mapLocalHost;
map<CNetAddr, LocalServiceInfo> mapLocalHost;
CCriticalSection cs_setLocalAddr;
set<CService> setLocalAddr;

static bool vfReachable[NET_MAX] = {};
static bool vfLimited[NET_MAX] = {};
static CNode* pnodeLocalHost = NULL;
uint64_t nLocalHostNonce = 0;
static std::vector<ListenSocket> vhListenSocket;
CAddrMan addrman;
int nMaxConnections = 125;
int nMaxOutConnections = 8;
int OutConnectionsAlgoritm=0;
int InitialNetLogTime=0;
bool fAddressesInitialized = false;

vector<CNode*> vNodes;
CCriticalSection cs_vNodes;
map<CInv, CDataStream> mapRelay;
deque<pair<int64_t, CInv> > vRelayExpiration;
CCriticalSection cs_mapRelay;
limitedmap<CInv, int64_t> mapAlreadyAskedFor(MAX_INV_SZ);

static deque<string> vOneShots;
CCriticalSection cs_vOneShots;

set<CNetAddr> setservAddNodeAddresses;
CCriticalSection cs_setservAddNodeAddresses;

vector<std::string> vAddedNodes;
CCriticalSection cs_vAddedNodes;

NodeId nLastNodeId = 0;
CCriticalSection cs_nLastNodeId;

static CSemaphore *semOutbound = NULL;

void FlushBlockIndexCache();

// Signals for message handling
static CNodeSignals g_signals;
CNodeSignals& GetNodeSignals() { return g_signals; }

void AddOneShot(string strDest)
{
    LOCK(cs_vOneShots);
    vOneShots.push_back(strDest);
}

unsigned short GetListenPort()
{
    return (unsigned short)(GetArg("-port", Params().GetDefaultPort()));
}

// find 'best' local address for a particular peer
bool GetLocal(CService& addr, const CNetAddr *paddrPeer)
{
    if (!fListen)
        return false;

    int nBestScore = -1;
    int nBestReachability = -1;
    {
        LOCK(cs_mapLocalHost);
        for (map<CNetAddr, LocalServiceInfo>::iterator it = mapLocalHost.begin(); it != mapLocalHost.end(); it++)
        {
            int nScore = (*it).second.nScore;
            int nReachability = (*it).first.GetReachabilityFrom(paddrPeer);
            if (nReachability > nBestReachability || (nReachability == nBestReachability && nScore > nBestScore))
            {
                addr = CService((*it).first, (*it).second.nPort);
                nBestReachability = nReachability;
                nBestScore = nScore;
            }
        }
    }
    return nBestScore >= 0;
}

// get best local address for a particular peer as a CAddress
// Otherwise, return the unroutable 0.0.0.0 but filled in with
// the normal parameters, since the IP may be changed to a useful
// one by discovery.
CAddress GetLocalAddress(const CNetAddr *paddrPeer)
{
    CAddress ret(CService("0.0.0.0",GetListenPort()),0);
    CService addr;
    if (GetLocal(addr, paddrPeer))
    {
        ret = CAddress(addr);
    }
    ret.nServices = nLocalServices;
    ret.nTime = GetAdjustedTime();
    return ret;
}

bool RecvLine(SOCKET hSocket, string& strLine)
{
    strLine = "";
    while (true)
    {
        char c;
        int nBytes = recv(hSocket, &c, 1, 0);
        if (nBytes > 0)
        {
            if (c == '\n')
                continue;
            if (c == '\r')
                return true;
            strLine += c;
            if (strLine.size() >= 9000)
                return true;
        }
        else if (nBytes <= 0)
        {
            boost::this_thread::interruption_point();
            if (nBytes < 0)
            {
                int nErr = WSAGetLastError();
                if (nErr == WSAEMSGSIZE)
                    continue;
                if (nErr == WSAEWOULDBLOCK || nErr == WSAEINTR || nErr == WSAEINPROGRESS)
                {
                    MilliSleep(10);
                    continue;
                }
            }
            if (!strLine.empty())
                return true;
            if (nBytes == 0)
            {
                // socket closed
                if(fDebug)LogPrint("net", "socket closed\n");
                return false;
            }
            else
            {
                // socket error
                int nErr = WSAGetLastError();
                if(fDebug)LogPrint("net", "recv failed: %s\n", NetworkErrorString(nErr));
                return false;
            }
        }
    }
}

int GetnScore(const CService& addr)
{
    LOCK(cs_mapLocalHost);
    if (mapLocalHost.count(addr) == LOCAL_NONE)
        return 0;
    return mapLocalHost[addr].nScore;
}

// Is our peer's addrLocal potentially useful as an external IP source?
bool IsPeerAddrLocalGood(CNode *pnode)
{
    return fDiscover && pnode->addr.IsRoutable() && pnode->addrLocal.IsRoutable() &&
           !IsLimited(pnode->addrLocal.GetNetwork());
}

// pushes our own address to a peer
void AdvertizeLocal(CNode *pnode)
{
    if (fListen && pnode->fSuccessfullyConnected)
    {
        CAddress addrLocal = GetLocalAddress(&pnode->addr);
        // If discovery is enabled, sometimes give our peer the address it
        // tells us that it sees us as in case it has a better idea of our
        // address than we do.
        if (IsPeerAddrLocalGood(pnode) && (!addrLocal.IsRoutable() ||
             GetRand((GetnScore(addrLocal) > LOCAL_MANUAL) ? 8:2) == 0))
        {
            addrLocal.SetIP(pnode->addrLocal);
        }
        if (addrLocal.IsRoutable())
        {
            pnode->PushAddress(addrLocal);
        }
    }
}

void SetReachable(enum Network net, bool fFlag)
{
    LOCK(cs_mapLocalHost);
    vfReachable[net] = fFlag;
    if (net == NET_IPV6 && fFlag)
        vfReachable[NET_IPV4] = true;
}

// learn a new local address
bool AddLocal(const CService& addr, int nScore)
{
    if (!addr.IsRoutable())
        return false;

    if (!fDiscover && nScore < LOCAL_MANUAL)
        return false;

    if (IsLimited(addr))
        return false;

    LogPrintf("AddLocal(%s,%i)\n", addr.ToString(), nScore);

    {
        LOCK(cs_mapLocalHost);
        bool fAlready = mapLocalHost.count(addr) > 0;
        LocalServiceInfo &info = mapLocalHost[addr];
        if (!fAlready || nScore >= info.nScore) {
            info.nScore = nScore + (fAlready ? 1 : 0);
            info.nPort = addr.GetPort();
        }
        SetReachable(addr.GetNetwork());
    }

    return true;
}

bool AddLocal(const CNetAddr &addr, int nScore)
{
    return AddLocal(CService(addr, GetListenPort()), nScore);
}

/** Make a particular network entirely off-limits (no automatic connects to it) */
void SetLimited(enum Network net, bool fLimited)
{
    if (net == NET_UNROUTABLE)
        return;
    LOCK(cs_mapLocalHost);
    vfLimited[net] = fLimited;
}

bool IsLimited(enum Network net)
{
    LOCK(cs_mapLocalHost);
    return vfLimited[net];
}

bool IsLimited(const CNetAddr &addr)
{
    return IsLimited(addr.GetNetwork());
}

/** vote for a local address */
bool SeenLocal(const CService& addr)
{
    {
        LOCK(cs_mapLocalHost);
        if (mapLocalHost.count(addr) == 0)
            return false;
        mapLocalHost[addr].nScore++;
    }
    return true;
}

bool SeenLocalAddr(const CService& addr)
{
    {
        LOCK(cs_setLocalAddr);        
        if (setLocalAddr.find(addr) == setLocalAddr.end())
        {
            setLocalAddr.insert(addr);
        }
    }
    return true;
}

/** check whether a given address is potentially local */
bool IsLocal(const CService& addr)
{
    if(addr.IsLocal())
    {
        return 1;        
    }
            
    LOCK(cs_mapLocalHost);
    return mapLocalHost.count(addr) > 0;
}

/** check whether a given network is one we can probably connect to */
bool IsReachable(enum Network net)
{
    LOCK(cs_mapLocalHost);
    return vfReachable[net] && !vfLimited[net];
}

/** check whether a given address is in a network we can probably connect to */
bool IsReachable(const CNetAddr& addr)
{
    enum Network net = addr.GetNetwork();
    return IsReachable(net);
}

void AddressCurrentlyConnected(const CService& addr)
{
    addrman.Connected(addr);
}


uint64_t CNode::nTotalBytesRecv = 0;
uint64_t CNode::nTotalBytesSent = 0;
CCriticalSection CNode::cs_totalBytesRecv;
CCriticalSection CNode::cs_totalBytesSent;

CNode* FindNode(const CNetAddr& ip)
{
    LOCK(cs_vNodes);
    BOOST_FOREACH(CNode* pnode, vNodes)
        if ((CNetAddr)pnode->addr == ip)
            return (pnode);
    return NULL;
}

CNode* FindNode(const std::string& addrName)
{
    LOCK(cs_vNodes);
    BOOST_FOREACH(CNode* pnode, vNodes)
        if (pnode->addrName == addrName)
            return (pnode);
    return NULL;
}

CNode* FindNode(const CService& addr)
{
    LOCK(cs_vNodes);
    BOOST_FOREACH(CNode* pnode, vNodes)
        if ((CService)pnode->addr == addr)
            return (pnode);
    return NULL;
}

CNode* ConnectNode(CAddress addrConnect, const char *pszDest)
{
    if (pszDest == NULL) {
//        if (IsLocal(addrConnect))

        if (IsLocal(addrConnect) && (addrConnect.GetPort() == GetListenPort()))
        return NULL;

        // Look for an existing connection
        CNode* pnode = FindNode((CService)addrConnect);
        if (pnode)
        {
            pnode->AddRef();
            return pnode;
        }
    }

    /// debug print
    if(fDebug)LogPrint("net", "net: trying connection %s lastseen=%.1fhrs\n",
        pszDest ? pszDest : addrConnect.ToString(),
        pszDest ? 0.0 : (double)(GetAdjustedTime() - addrConnect.nTime)/3600.0);

    // Connect
    SOCKET hSocket;
    bool proxyConnectionFailed = false;
    if (pszDest ? ConnectSocketByName(addrConnect, hSocket, pszDest, Params().GetDefaultPort(), nConnectTimeout, &proxyConnectionFailed) :
                  ConnectSocket(addrConnect, hSocket, nConnectTimeout, &proxyConnectionFailed))
    {
/* MCHN START */        
        SetReachable(addrConnect.GetNetwork());
/* MCHN END */        
        addrman.Attempt(addrConnect);

        // Add node
        CNode* pnode = new CNode(hSocket, addrConnect, pszDest ? pszDest : "", false);
        pnode->AddRef();

/* MCHN START */        
        if(pszDest)
        {
            if(mc_gState->GetSeedNode())
            {
                if(strcmp(pszDest,mc_gState->GetSeedNode()) == 0)
                {
                    mc_gState->m_pSeedNode=pnode;
                    if(fDebug)LogPrint("mchn","mchn: Connected to seed node %s\n",pszDest);
                }
            }
        }
        
/* MCHN END */        
        {
            LOCK(cs_vNodes);
            vNodes.push_back(pnode);
        }

        pnode->nTimeConnected = GetTime();
        pnode->nTimeToDisconnect = pnode->nTimeConnected + 86400 - 3600 + GetRand(7200);

        return pnode;
    } else if (!proxyConnectionFailed) {
        // If connecting to the node failed, and failure is not caused by a problem connecting to
        // the proxy, mark this as an attempt.
        addrman.Attempt(addrConnect);
    }

    return NULL;
}

void CNode::CloseSocketDisconnect()
{
    fDisconnect = true;
    if (hSocket != INVALID_SOCKET)
    {
//        if(fDebug)LogPrint("net", "disconnecting peer=%d\n", id);
         LogPrintf("Peer %6d: Disconnecting\n", id);
        CloseSocket(hSocket);
    }

    // in case this fails, we'll empty the recv buffer when the CNode is deleted
    {
        TRY_LOCK(cs_vRecvMsg, lockRecv);
        if (lockRecv)
            vRecvMsg.clear();
    }
    {
        TRY_LOCK(cs_vRecvDataMsg, lockDataRecv);
        if (lockDataRecv)
            vRecvDataMsg.clear();
    }
    {
        TRY_LOCK(cs_vRecvTxDataMsg, lockTxDataRecv);
        if (lockTxDataRecv)
            vRecvDataMsg.clear();
    }
    {
        TRY_LOCK(cs_vRecvOffchainMsg, lockOffchainRecv);
        if (lockOffchainRecv)
            vRecvOffchainMsg.clear();
    }
}

void CNode::PushVersion()
{
//    int nBestHeight = g_signals.GetHeight().get_value_or(0);
    int nBestHeight=chainActive.Height();
    /// when NTP implemented, change to just nTime = GetAdjustedTime()
    int64_t nTime = (fInbound ? GetAdjustedTime() : GetTime());
    CAddress addrYou = (addr.IsRoutable() && !IsProxy(addr) ? addr : CAddress(CService("0.0.0.0",0)));
    CAddress addrMe = GetLocalAddress(&addr);
    GetRandBytes((unsigned char*)&nLocalHostNonce, sizeof(nLocalHostNonce));
    nVersionNonceSent=nLocalHostNonce;                                              // MCHN
    if (fLogIPs)
    {
        if(fDebug)LogPrint("net", "send version message: version %d, blocks=%d, us=%s, them=%s, peer=%d\n", PROTOCOL_VERSION, nBestHeight, addrMe.ToString(), addrYou.ToString(), id);
    }
    else
    {
        if(fDebug)LogPrint("net", "send version message: version %d, blocks=%d, us=%s, peer=%d\n", PROTOCOL_VERSION, nBestHeight, addrMe.ToString(), id);
    }
    
/* MCHN START */
    std::string subver=FormatSubVersion(CLIENT_NAME, CLIENT_VERSION, std::vector<string>());
    nLocalMultiChainServices |= pEF->NET_Services();
    if(mc_gState->m_NetworkParams->IsProtocolMultichain())
    {
        subver=FormatSubVersion("MultiChain", mc_gState->GetProtocolVersion(), std::vector<string>());
    }
    PushMessage("version", PROTOCOL_VERSION, nLocalServices, nTime, addrYou, addrMe,
                nLocalHostNonce, subver, nBestHeight, true, nLocalMultiChainServices);
/* MCHN END */
}





std::map<CNetAddr, int64_t> CNode::setBanned;
CCriticalSection CNode::cs_setBanned;

void CNode::ClearBanned()
{
    setBanned.clear();
}

bool CNode::IsBanned(CNetAddr ip)
{
    bool fResult = false;
    {
        LOCK(cs_setBanned);
        std::map<CNetAddr, int64_t>::iterator i = setBanned.find(ip);
        if (i != setBanned.end())
        {
            int64_t t = (*i).second;
            if (GetTime() < t)
                fResult = true;
        }
    }
    return fResult;
}

bool CNode::Ban(const CNetAddr &addr) {
    int64_t banTime = GetTime()+GetArg("-bantime", 60*60*24);  // Default 24-hour ban
    {
        LOCK(cs_setBanned);
        if (setBanned[addr] < banTime)
            setBanned[addr] = banTime;
    }
    return true;
}

bool CNode::IsTxInFlight(uint256 txid)
{
    LOCK(cs_sTxsInFlight);
//    return ( sTxsInFlight.find(txid) != sTxsInFlight.end() );
    if ( sTxsInFlight.count(txid) > 0 )
    {
        return true;
    }
    return false;
}

void CNode::AddTxsInFlight(std::vector<uint256> txids)
{
    LOCK(cs_sTxsInFlight);
    for(unsigned int i=0;i<txids.size();i++)
    {
//        if( sTxsInFlight.find(txids[i]) == sTxsInFlight.end() )
        if(sTxsInFlight.count(txids[i]) == 0)
        {
            sTxsInFlight.insert(txids[i]);
        }
    }
}

void CNode::RemoveTxsInFlight(std::vector<uint256> txids)
{
    LOCK(cs_sTxsInFlight);
    for(unsigned int i=0;i<txids.size();i++)
    {
        set <uint256>::iterator it=sTxsInFlight.find(txids[i]);
        if( it != sTxsInFlight.end() )
        {
            sTxsInFlight.erase(it);
        }
    }    
}

void CNode::RemoveTxInFlight(uint256 txid)
{
    LOCK(cs_sTxsInFlight);    
    set <uint256>::iterator it=sTxsInFlight.find(txid);
    if( it != sTxsInFlight.end() )
    {
        sTxsInFlight.erase(it);
    }
}

size_t CNode::TotalBuffersSize()
{
    size_t total=0;
    if(nMessageHandlerThreads != ( MC_MHT_GETDATA | MC_MHT_PROCESSDATA | MC_MHT_PROCESSTXDATA ) )                
    {
        return nTotalBuffersSize;
    }
    int64_t nNow=GetTime();
    if(nNow < nNextSizeCalcTimestamp)
    {
        return nTotalBuffersSize;
    }
    nNextSizeCalcTimestamp=nNow+10;
    
    {
        LOCK(cs_vSend);
        total+=ssSend.size();
    }
    {
        LOCK(cs_vRecvGetData);
        total+=vRecvGetDataBuf.size()*sizeof(CInv);
    }

    {
        LOCK(cs_vRecvMsg);
        total+=vRecvMsg.size()*sizeof(CNetMessage);
        std::deque<CNetMessage>::iterator it = vRecvMsg.begin();
        while (it != vRecvMsg.end()) 
        {
            total+=it->vRecv.size();
            it++;
        }                
    }

    {
        LOCK(cs_vRecvDataMsg);
        total+=vRecvDataMsg.size()*sizeof(CNetMessage);
        std::deque<CNetMessage>::iterator it = vRecvDataMsg.begin();
        while (it != vRecvDataMsg.end()) 
        {
            total+=it->vRecv.size();
            it++;
        }                
    }
    {
        LOCK(cs_vRecvTxDataMsg);
        total+=vRecvTxDataMsg.size()*sizeof(CNetMessage);
        std::deque<CNetMessage>::iterator it = vRecvTxDataMsg.begin();
        while (it != vRecvTxDataMsg.end()) 
        {
            total+=it->vRecv.size();
            it++;
        }                
    }
    {
        LOCK(cs_vRecvOffchainMsg);
        total+=vRecvOffchainMsg.size()*sizeof(CNetMessage);
        std::deque<CNetMessage>::iterator it = vRecvOffchainMsg.begin();
        while (it != vRecvOffchainMsg.end()) 
        {
            total+=it->vRecv.size();
            it++;
        }                
    }
    {
        LOCK(cs_sTxsInFlight);
        total+=sTxsInFlight.size()*sizeof(uint256);
    }
    {
        LOCK(cs_inventory);
        total+=setInventoryKnown.size()*sizeof(CInv);
        total+=vInventoryToSend.size()*sizeof(CInv);
    }
    {
        LOCK(cs_askfor);
        total+=mapAskFor.size()*sizeof(CInv);
    }
    nTotalBuffersSize=total;
    return total;
}

std::vector<CSubNet> CNode::vWhitelistedRange;
CCriticalSection CNode::cs_vWhitelistedRange;

bool CNode::IsWhitelistedRange(const CNetAddr &addr) {
    LOCK(cs_vWhitelistedRange);
    BOOST_FOREACH(const CSubNet& subnet, vWhitelistedRange) {
        if (subnet.Match(addr))
            return true;
    }
    return false;
}

void CNode::AddWhitelistedRange(const CSubNet &subnet) {
    LOCK(cs_vWhitelistedRange);
    vWhitelistedRange.push_back(subnet);
}

#undef X
#define X(name) stats.name = name
void CNode::copyStats(CNodeStats &stats)
{
    stats.nodeid = this->GetId();
    X(nServices);
    X(nLastSend);
    X(nLastRecv);
    X(nTimeConnected);
    X(addrName);
    X(nVersion);
    X(cleanSubVer);
    X(fInbound);
    X(nStartingHeight);
    X(nSendBytes);
    X(nRecvBytes);
    X(fWhitelisted);

    // It is common for nodes with good ping times to suddenly become lagged,
    // due to a new block arriving or other large transfer.
    // Merely reporting pingtime might fool the caller into thinking the node was still responsive,
    // since pingtime does not update until the ping is complete, which might take a while.
    // So, if a ping is taking an unusually long time in flight,
    // the caller can immediately detect that this is happening.
    int64_t nPingUsecWait = 0;
    if ((0 != nPingNonceSent) && (0 != nPingUsecStart)) {
        nPingUsecWait = GetTimeMicros() - nPingUsecStart;
    }

    // Raw ping time is in microseconds, but show it to user as whole seconds (Bitcoin users should be well used to small numbers with many decimal places by now :)
    stats.dPingTime = (((double)nPingUsecTime) / 1e6);
    stats.dPingWait = (((double)nPingUsecWait) / 1e6);

    // Leave string empty if addrLocal invalid (not filled in yet)
    stats.addrLocal = addrLocal.IsValid() ? addrLocal.ToString() : "";
    
/* MCHN START */    
    stats.kAddrLocal=kAddrLocal;
    stats.kAddrRemote=kAddrRemote;
    stats.fSuccessfullyConnected=fSuccessfullyConnected;
    stats.fEncrypted=pEF->NET_IsEncrypted(this);
/* MCHN END */    
    
}
#undef X

bool CNode::DelayedSend()
{
    if(nNextSendTime)
    {
        if(GetTime() < nNextSendTime)
        {
            return true;
        }
    }
    
    nNextSendTime=0;
    return false;
}

// requires LOCK(cs_vRecvMsg)
bool CNode::ReceiveMsgBytes(const char *pch, unsigned int nBytes)
{
    if(!pEF->NET_RestoreFromCache(this))
    {
        return false;
    }
    
    if(!vRecvMsg.empty() && vRecvMsg.back().complete())
    {
        int handled;
        CNetMessage& msg = vRecvMsg.back();
        handled=pEF->NET_StoreInCache(pEntData,msg,pch,nBytes);
        pch += handled;
        nBytes -= handled;
    }
    
    while (nBytes > 0) {

        // get current incomplete message, or create a new one
        if (vRecvMsg.empty() ||
            vRecvMsg.back().complete())
            vRecvMsg.push_back(CNetMessage(SER_NETWORK, nRecvVersion));

        CNetMessage& msg = vRecvMsg.back();

        // absorb network data
        int handled;
        if (!msg.in_data)
        {
            if(pEntData)
            {
                handled = pEF->NET_ReadHeader(pEntData,msg,pch,nBytes);
            }
            else
            {
                handled = msg.readHeader(pch, nBytes);
            }
        }
        else
            handled = msg.readData(pch, nBytes);

        if (handled < 0)        
        {
            LogPrintf("Error reading header, size: %u, peer=%d\n",msg.hdr.nMessageSize,this->id);            
            return false;
        }
        
        pch += handled;
        nBytes -= handled;

        if (msg.complete())
        {
            pEF->NET_ProcessMsgData(pEntData,msg);
            msg.nTime = GetTimeMicros();
            if(fDebug)LogPrint("mcnet","mcnet: complete message: %s, peer=%d\n", msg.hdr.GetCommand(),id);
            
            handled=pEF->NET_StoreInCache(pEntData,msg,pch,nBytes);
            pch += handled;
            nBytes -= handled;
        }
    }

    return true;
}

int CNetMessage::readHeader(const char *pch, unsigned int nBytes)
{
    // copy data to temporary parsing buffer
    unsigned int nRemaining = 24 - nHdrPos;
    unsigned int nCopy = std::min(nRemaining, nBytes);

    memcpy(&hdrbuf[nHdrPos], pch, nCopy);
    nHdrPos += nCopy;
    
//    mc_Dump("HEAD",pch,nCopy);
//    mc_Dump("HEAD",pch,nBytes);

    // if header incomplete, exit
    if (nHdrPos < 24)
        return nCopy;

    // deserialize to CMessageHeader
    try {
        hdrbuf >> hdr;
    }
    catch (const std::exception &) {
        return -1;
    }

    // reject messages larger than MAX_SIZE
    if (hdr.nMessageSize > MAX_SIZE)
            return -1;

    if(fDebug)LogPrint("mcnet","mcnet: received header: %s\n", hdr.GetCommand());
    
    // switch state to reading message data
    in_data = true;

    return nCopy;
}

int CNetMessage::readData(const char *pch, unsigned int nBytes)
{
    unsigned int nRemaining = hdr.nMessageSize - nDataPos;
    unsigned int nCopy = std::min(nRemaining, nBytes);

    if (vRecv.size() < nDataPos + nCopy) {
        // Allocate up to 256 KiB ahead, but never more than the total message size.
        vRecv.resize(std::min(hdr.nMessageSize, nDataPos + nCopy + 256 * 1024));
    }

    memcpy(&vRecv[nDataPos], pch, nCopy);
    
//    mc_Dump("RECV",pch,nCopy);
    
    nDataPos += nCopy;

    return nCopy;
}










// requires LOCK(cs_vSend)
void SocketSendData(CNode *pnode)
{
    if(pnode->DelayedSend())
    {
        return;
    }
    double start_time=0;
    size_t start_size=0;
    if(InitialNetLogTime)
    {
        start_time=mc_TimeNowAsDouble();
        start_size=pnode->nSendSize;
    }
    std::deque<CSerializeData>::iterator it = pnode->vSendMsg.begin();

    while (it != pnode->vSendMsg.end()) {
        const CSerializeData &data = *it;
        assert(data.size() > pnode->nSendOffset);
        
//        mc_Dump("SEND",(const void*)(&data[pnode->nSendOffset]),data.size()- pnode->nSendOffset);
        
        int nBytes = send(pnode->hSocket, &data[pnode->nSendOffset], data.size() - pnode->nSendOffset, MSG_NOSIGNAL | MSG_DONTWAIT);
        if (nBytes > 0) {
            pnode->nLastSend = GetTime();
            pnode->nSendBytes += nBytes;
            pnode->nSendOffset += nBytes;
            pnode->RecordBytesSent(nBytes);
            if (pnode->nSendOffset == data.size()) {
                pnode->nSendOffset = 0;
                pnode->nSendSize -= data.size();
                it++;
            } else {
                // could not send full message; stop sending more
                break;
            }
        } else {
            if (nBytes < 0) {
                // error
                int nErr = WSAGetLastError();
                if (nErr != WSAEWOULDBLOCK && nErr != WSAEMSGSIZE && nErr != WSAEINTR && nErr != WSAEINPROGRESS)
                {
                    LogPrintf("socket send error %s\n", NetworkErrorString(nErr));
                    pnode->CloseSocketDisconnect();
                }
            }
            // couldn't send anything at all
            break;
        }
    }

    if (it == pnode->vSendMsg.end()) {
        assert(pnode->nSendOffset == 0);
        assert(pnode->nSendSize == 0);
    }
    pnode->vSendMsg.erase(pnode->vSendMsg.begin(), it);
    
    if(InitialNetLogTime)
    {
        if(start_size>pnode->nSendSize)
        {
            if(GetTime()-pnode->nTimeConnected<InitialNetLogTime)LogPrintf("mchn-inl: NSNT: sent %d bytes in %8.3fs\n",start_size-pnode->nSendSize,mc_TimeNowAsDouble()-start_time);            
        }
    }
}

static list<CNode*> vNodesDisconnected;

void ThreadSocketHandler()
{
    unsigned int nPrevNodeCount = 0;
    while (true)
    {
        //
        // Disconnect nodes
        //
        {
            LOCK(cs_vNodes);
            // Disconnect unused nodes
            vector<CNode*> vNodesCopy = vNodes;
            BOOST_FOREACH(CNode* pnode, vNodesCopy)
            {
                if ( (pnode->fDisconnect && !pnode->DelayedSend()) ||
                    (pnode->GetRefCount() <= 0 && pnode->vRecvMsg.empty() && pnode->nSendSize == 0 && pnode->ssSend.empty()))
                {
                    // remove from vNodes
                    vNodes.erase(remove(vNodes.begin(), vNodes.end(), pnode), vNodes.end());

                    // release outbound grant (if any)
                    pnode->grantOutbound.Release();

                    // close socket and cleanup
/* MCHN START */                    
                    if(fDebug)LogPrint("net","disconnect flag set\n");
                    if(!pnode->fInbound)                                        // Refresh connection time for good nodes we are disconnecting from
                    {
                        if(pnode->fParameterSetVerified)
                        {
                            addrman.Good(pnode->addr);
                        }
                    }
/* MCHN END */                    
                    pnode->CloseSocketDisconnect();

                    // hold in disconnected pool until all refs are released
                    if (pnode->fNetworkNode || pnode->fInbound)
                        pnode->Release();
                    vNodesDisconnected.push_back(pnode);
                }
            }
        }
        {
            // Delete disconnected nodes
            list<CNode*> vNodesDisconnectedCopy = vNodesDisconnected;
            BOOST_FOREACH(CNode* pnode, vNodesDisconnectedCopy)
            {
                // wait until threads are done using it
                if (pnode->GetRefCount() <= 0)
                {
                    bool fDelete = false;
                    {
                        TRY_LOCK(pnode->cs_vSend, lockSend);
                        if (lockSend)
                        {
                            TRY_LOCK(pnode->cs_vRecvMsg, lockRecv);
                            if (lockRecv)
                            {
                                TRY_LOCK(pnode->cs_vRecvDataMsg, lockDataRecv);
                                if (lockDataRecv)
                                {
                                    TRY_LOCK(pnode->cs_vRecvTxDataMsg, lockTxDataRecv);
                                    if (lockTxDataRecv)
                                    {
                                        TRY_LOCK(pnode->cs_vRecvOffchainMsg, lockOffchainRecv);
                                        if (lockOffchainRecv)
                                        {
                                            TRY_LOCK(pnode->cs_inventory, lockInv);
                                            if (lockInv)
                                                fDelete = true;
                                        }
                                    }
                                }
                            }
                        }
                    }
                                        
                    if (fDelete)
                    {
                        vNodesDisconnected.remove(pnode);

                        if(mc_gState->m_pSeedNode == (void*)pnode)
                        {
                            mc_gState->m_pSeedNode=NULL;
                            if(fDebug)LogPrint("mchn","mchn: Disconnected from seed node\n");
                        }

                        delete pnode;
                    }
                }
            }
        }
        if(vNodes.size() != nPrevNodeCount) {
            nPrevNodeCount = vNodes.size();
            uiInterface.NotifyNumConnectionsChanged(nPrevNodeCount);
        }

        //
        // Find which sockets have data to receive
        //
        struct timeval timeout;
        timeout.tv_sec  = 0;
        timeout.tv_usec = 50000; // frequency to poll pnode->vSend

        fd_set fdsetRecv;
        fd_set fdsetSend;
        fd_set fdsetError;
        FD_ZERO(&fdsetRecv);
        FD_ZERO(&fdsetSend);
        FD_ZERO(&fdsetError);
        SOCKET hSocketMax = 0;
        bool have_fds = false;

        BOOST_FOREACH(const ListenSocket& hListenSocket, vhListenSocket) {
            FD_SET(hListenSocket.socket, &fdsetRecv);
            hSocketMax = max(hSocketMax, hListenSocket.socket);
            have_fds = true;
        }

        {
            LOCK(cs_vNodes);
            BOOST_FOREACH(CNode* pnode, vNodes)
            {
                if (pnode->hSocket == INVALID_SOCKET)
                    continue;
                FD_SET(pnode->hSocket, &fdsetError);
                hSocketMax = max(hSocketMax, pnode->hSocket);
                have_fds = true;

                // Implement the following logic:
                // * If there is data to send, select() for sending data. As this only
                //   happens when optimistic write failed, we choose to first drain the
                //   write buffer in this case before receiving more. This avoids
                //   needlessly queueing received data, if the remote peer is not themselves
                //   receiving data. This means properly utilizing TCP flow control signalling.
                // * Otherwise, if there is no (complete) message in the receive buffer,
                //   or there is space left in the buffer, select() for receiving data.
                // * (if neither of the above applies, there is certainly one message
                //   in the receiver buffer ready to be processed).
                // Together, that means that at least one of the following is always possible,
                // so we don't deadlock:
                // * We send some data.
                // * We wait for data to be received (and disconnect after timeout).
                // * We process a message in the buffer (message handler thread).
                {
                    TRY_LOCK(pnode->cs_vSend, lockSend);
                    if (lockSend && !pnode->vSendMsg.empty()) {
                        FD_SET(pnode->hSocket, &fdsetSend);
                        continue;
                    }
                }
                {
                    TRY_LOCK(pnode->cs_vRecvMsg, lockRecv);
                    if (lockRecv && (
                        pnode->vRecvMsg.empty() || !pnode->vRecvMsg.front().complete() ||
                        pnode->GetTotalRecvSize() <= ReceiveFloodSize()))
                        FD_SET(pnode->hSocket, &fdsetRecv);
                }
            }
        }

        int nSelect = select(have_fds ? hSocketMax + 1 : 0,
                             &fdsetRecv, &fdsetSend, &fdsetError, &timeout);
        boost::this_thread::interruption_point();

        if (nSelect == SOCKET_ERROR)
        {
            if (have_fds)
            {
                int nErr = WSAGetLastError();
                LogPrintf("socket select error %s\n", NetworkErrorString(nErr));
                for (unsigned int i = 0; i <= hSocketMax; i++)
                    FD_SET(i, &fdsetRecv);
            }
            FD_ZERO(&fdsetSend);
            FD_ZERO(&fdsetError);
            MilliSleep(timeout.tv_usec/1000);
        }

        //
        // Accept new connections
        //
        BOOST_FOREACH(const ListenSocket& hListenSocket, vhListenSocket)
        {
            if (hListenSocket.socket != INVALID_SOCKET && FD_ISSET(hListenSocket.socket, &fdsetRecv))
            {
                struct sockaddr_storage sockaddr;
                socklen_t len = sizeof(sockaddr);
                SOCKET hSocket = accept(hListenSocket.socket, (struct sockaddr*)&sockaddr, &len);
                CAddress addr;
                int nInbound = 0;

                if (hSocket != INVALID_SOCKET)
                    if (!addr.SetSockAddr((const struct sockaddr*)&sockaddr))
                        LogPrintf("Warning: Unknown socket family\n");
                
                if(fDebug)LogPrint("net","Connection attempt from: %s\n", addr.ToString().c_str());

                bool whitelisted = hListenSocket.whitelisted || CNode::IsWhitelistedRange(addr);
                {
                    LOCK(cs_vNodes);
                    BOOST_FOREACH(CNode* pnode, vNodes)
                        if (pnode->fInbound)
                            nInbound++;
                }

                if (hSocket == INVALID_SOCKET)
                {
                    int nErr = WSAGetLastError();
                    if (nErr != WSAEWOULDBLOCK)
                        LogPrintf("socket error accept failed: %s\n", NetworkErrorString(nErr));
                }
                else if (nInbound >= nMaxConnections - nMaxOutConnections) // (nInbound >= nMaxConnections - GetArg("-maxoutconnections",MAX_OUTBOUND_CONNECTIONS))
                {
                    CloseSocket(hSocket);
                }
                else if (CNode::IsBanned(addr) && !whitelisted)
                {
                    LogPrintf("connection from %s dropped (banned)\n", addr.ToString());
                    CloseSocket(hSocket);
                }
                else
                {
                    CNode* pnode = new CNode(hSocket, addr, "", true);
                    pnode->AddRef();
                    pnode->fWhitelisted = whitelisted;

                    {
                        LOCK(cs_vNodes);
                        vNodes.push_back(pnode);
                    }
                }
            }
        }

        //
        // Service each socket
        //
        vector<CNode*> vNodesCopy;
        {
            LOCK(cs_vNodes);
            vNodesCopy = vNodes;
            BOOST_FOREACH(CNode* pnode, vNodesCopy)
                pnode->AddRef();
        }
        BOOST_FOREACH(CNode* pnode, vNodesCopy)
        {
            boost::this_thread::interruption_point();

            //
            // Receive
            //
            if (pnode->hSocket == INVALID_SOCKET)
                continue;
            if (FD_ISSET(pnode->hSocket, &fdsetRecv) || FD_ISSET(pnode->hSocket, &fdsetError))
            {
                TRY_LOCK(pnode->cs_vRecvMsg, lockRecv);
                if (lockRecv)
                {
                    {
                        // typical socket buffer is 8K-64K
                        char pchBuf[0x10000];
                        int nBytes = recv(pnode->hSocket, pchBuf, sizeof(pchBuf), MSG_DONTWAIT);
                        if (nBytes > 0)
                        {
                            if (!pnode->ReceiveMsgBytes(pchBuf, nBytes))
                            {
/* MCHN START */                    
                                if(fDebug)LogPrint("net","receive error\n");
/* MCHN END */                    
                                pnode->CloseSocketDisconnect();
                            }
                            pnode->nLastRecv = GetTime();
                            pnode->nRecvBytes += nBytes;
                            pnode->RecordBytesRecv(nBytes);
                        }
                        else if (nBytes == 0)
                        {
                            // socket closed gracefully
                            if (!pnode->fDisconnect)
                            {
                                if(fDebug)LogPrint("net", "socket closed\n");
                            }
                            else
                            {
                                if(fDebug)LogPrint("net","socket closed, disconnect flag is set\n");
                            }
                            pnode->CloseSocketDisconnect();
                        }
                        else if (nBytes < 0)
                        {
                            // error
                            int nErr = WSAGetLastError();
                            if (nErr != WSAEWOULDBLOCK && nErr != WSAEMSGSIZE && nErr != WSAEINTR && nErr != WSAEINPROGRESS)
                            {
                                if (!pnode->fDisconnect)
                                    LogPrintf("socket recv error %s\n", NetworkErrorString(nErr));
/* MCHN START */                    
                                else
                                    LogPrintf("socket recv error %s, disconnect flag is set\n", NetworkErrorString(nErr));
/* MCHN END */                    
                                pnode->CloseSocketDisconnect();
                            }
                        }
                    }
                }
            }

            //
            // Send
            //
            if (pnode->hSocket == INVALID_SOCKET)
                continue;
            if (FD_ISSET(pnode->hSocket, &fdsetSend))
            {
                TRY_LOCK(pnode->cs_vSend, lockSend);
                if (lockSend)
                    SocketSendData(pnode);
            }

            //
            // Inactivity checking
            //
            int64_t nTime = GetTime();
            if (nTime - pnode->nTimeConnected > 60)
            {
                if (pnode->nLastRecv == 0 || pnode->nLastSend == 0)
                {
                    if(fDebug)LogPrint("net", "socket no message in first 60 seconds, %d %d from %d\n", pnode->nLastRecv != 0, pnode->nLastSend != 0, pnode->id);
                    pnode->fDisconnect = true;
                }
                else if (nTime - pnode->nLastSend > TIMEOUT_INTERVAL)
                {
                    LogPrintf("socket sending timeout: %is, peer %d\n", nTime - pnode->nLastSend,pnode->id);
                    pnode->fDisconnect = true;
                }
                else if (nTime - pnode->nLastRecv > (pnode->nVersion > BIP0031_VERSION ? TIMEOUT_INTERVAL : 90*60))
                {
                    LogPrintf("socket receive timeout: %is, peer %d\n", nTime - pnode->nLastRecv,pnode->id);
                    pnode->fDisconnect = true;
                }
                else if (pnode->nPingNonceSent && pnode->nPingUsecStart + TIMEOUT_INTERVAL * 1000000 < GetTimeMicros())
                {
                    LogPrintf("ping timeout: %fs, peer %d\n", 0.000001 * (GetTimeMicros() - pnode->nPingUsecStart),pnode->id);
                    pnode->fDisconnect = true;
                }
            }
        }
        {
            LOCK(cs_vNodes);
            BOOST_FOREACH(CNode* pnode, vNodesCopy)
                pnode->Release();
        }
    }
}









#ifdef USE_UPNP
void ThreadMapPort()
{
    std::string port = strprintf("%u", GetListenPort());
    const char * multicastif = 0;
    const char * minissdpdpath = 0;
    struct UPNPDev * devlist = 0;
    char lanaddr[64];

#ifndef UPNPDISCOVER_SUCCESS
    /* miniupnpc 1.5 */
    devlist = upnpDiscover(2000, multicastif, minissdpdpath, 0);
#else
    /* miniupnpc 1.6 */
    int error = 0;
    devlist = upnpDiscover(2000, multicastif, minissdpdpath, 0, 0, &error);
#endif

    struct UPNPUrls urls;
    struct IGDdatas data;
    int r;

    r = UPNP_GetValidIGD(devlist, &urls, &data, lanaddr, sizeof(lanaddr));
    if (r == 1)
    {
        if (fDiscover) {
            char externalIPAddress[40];
            r = UPNP_GetExternalIPAddress(urls.controlURL, data.first.servicetype, externalIPAddress);
            if(r != UPNPCOMMAND_SUCCESS)
                LogPrintf("UPnP: GetExternalIPAddress() returned %d\n", r);
            else
            {
                if(externalIPAddress[0])
                {
                    LogPrintf("UPnP: ExternalIPAddress = %s\n", externalIPAddress);
                    AddLocal(CNetAddr(externalIPAddress), LOCAL_UPNP);
                }
                else
                    LogPrintf("UPnP: GetExternalIPAddress failed.\n");
            }
        }

        string strDesc = "MultiChain " + FormatFullVersion();

        try {
            while (true) {
#ifndef UPNPDISCOVER_SUCCESS
                /* miniupnpc 1.5 */
                r = UPNP_AddPortMapping(urls.controlURL, data.first.servicetype,
                                    port.c_str(), port.c_str(), lanaddr, strDesc.c_str(), "TCP", 0);
#else
                /* miniupnpc 1.6 */
                r = UPNP_AddPortMapping(urls.controlURL, data.first.servicetype,
                                    port.c_str(), port.c_str(), lanaddr, strDesc.c_str(), "TCP", 0, "0");
#endif

                if(r!=UPNPCOMMAND_SUCCESS)
                    LogPrintf("AddPortMapping(%s, %s, %s) failed with code %d (%s)\n",
                        port, port, lanaddr, r, strupnperror(r));
                else
                    LogPrintf("UPnP Port Mapping successful.\n");;

                MilliSleep(20*60*1000); // Refresh every 20 minutes
            }
        }
        catch (boost::thread_interrupted)
        {
            r = UPNP_DeletePortMapping(urls.controlURL, data.first.servicetype, port.c_str(), "TCP", 0);
            LogPrintf("UPNP_DeletePortMapping() returned : %d\n", r);
            freeUPNPDevlist(devlist); devlist = 0;
            FreeUPNPUrls(&urls);
            throw;
        }
    } else {
        LogPrintf("No valid UPnP IGDs found\n");
        freeUPNPDevlist(devlist); devlist = 0;
        if (r != 0)
            FreeUPNPUrls(&urls);
    }
}

void MapPort(bool fUseUPnP)
{
    static boost::thread* upnp_thread = NULL;

    if (fUseUPnP)
    {
        if (upnp_thread) {
            upnp_thread->interrupt();
            upnp_thread->join();
            delete upnp_thread;
        }
        upnp_thread = new boost::thread(boost::bind(&TraceThread<void (*)()>, "upnp", &ThreadMapPort));
    }
    else if (upnp_thread) {
        upnp_thread->interrupt();
        upnp_thread->join();
        delete upnp_thread;
        upnp_thread = NULL;
    }
}

#else
void MapPort(bool)
{
    // Intentionally left blank.
}
#endif






void ThreadDNSAddressSeed()
{
    // goal: only query DNS seeds if address need is acute
    if ((addrman.size() > 0) &&
        (!GetBoolArg("-forcednsseed", false))) {
        MilliSleep(11 * 1000);

        LOCK(cs_vNodes);
        if (vNodes.size() >= 2) {
            LogPrintf("P2P peers available. Skipped DNS seeding.\n");
            return;
        }
    }

    const vector<CDNSSeedData> &vSeeds = Params().DNSSeeds();
    int found = 0;

    LogPrintf("Loading addresses from DNS seeds (could take a while)\n");

    BOOST_FOREACH(const CDNSSeedData &seed, vSeeds) {
        if (HaveNameProxy()) {
            AddOneShot(seed.host);
        } else {
            vector<CNetAddr> vIPs;
            vector<CAddress> vAdd;
            if (LookupHost(seed.host.c_str(), vIPs))
            {
                BOOST_FOREACH(CNetAddr& ip, vIPs)
                {
                    int nOneDay = 24*3600;
                    CAddress addr = CAddress(CService(ip, Params().GetDefaultPort()));
                    addr.nTime = GetTime() - 3*nOneDay - GetRand(4*nOneDay); // use a random age between 3 and 7 days old
                    vAdd.push_back(addr);
                    found++;
                }
            }
            addrman.Add(vAdd, CNetAddr(seed.name, true));
        }
    }

    LogPrintf("%d addresses found from DNS seeds\n", found);
}












void DumpAddresses()
{
    int64_t nStart = GetTimeMillis();

/* MCHN START */
    if(mc_gState->m_NetworkParams->m_Status != MC_PRM_STATUS_EMPTY) 
    {
/* MCHN END */            
    
    CAddrDB adb;
    adb.Write(addrman);

    if(fDebug)LogPrint("net", "Flushed %d addresses to peers.dat  %dms\n",
           addrman.size(), GetTimeMillis() - nStart);
/* MCHN START */
    }
/* MCHN END */            
}

void static ProcessOneShot()
{
    string strDest;
    {
        LOCK(cs_vOneShots);
        if (vOneShots.empty())
            return;
        strDest = vOneShots.front();
        vOneShots.pop_front();
    }
    CAddress addr;
    CSemaphoreGrant grant(*semOutbound, true);
    if (grant) {
        if (!OpenNetworkConnection(addr, &grant, strDest.c_str(), true))
            AddOneShot(strDest);
    }
}

void ThreadOpenConnections()
{
    // Connect to specific addresses
    if (mapArgs.count("-connect") && mapMultiArgs["-connect"].size() > 0)
    {
        for (int64_t nLoop = 0;; nLoop++)
        {
            ProcessOneShot();
            BOOST_FOREACH(string strAddr, mapMultiArgs["-connect"])
            {
                CAddress addr;
                OpenNetworkConnection(addr, NULL, strAddr.c_str());
                for (int i = 0; i < 10 && i < nLoop; i++)
                {
                    MilliSleep(500);
                }
            }
            MilliSleep(500);
        }
    }

/* MCHN START */    
    
    if(mc_gState->GetSeedNode())                                                // MCHN-TODO. Connection to seed node, find later how to disconnect
    {
        
        CAddress addr1=CAddress(CService(mc_gState->GetSeedNode()));
        if(addr1.IsValid())
        {
            CMCAddrInfo *mcaddrinfo=addrman.GetMCAddrMan()->Find(addr1,0);
            if(mcaddrinfo == NULL)
            {
                addrman.Add(addr1, addr1);
                addrman.Good(addr1);
                mcaddrinfo=addrman.GetMCAddrMan()->Find(addr1,0);
                if(mcaddrinfo)
                {
                    mcaddrinfo->SetFlag(MC_AMF_SOURCE_SEED,1);
                    if(fDebug)LogPrint("addrman", "Remember seed address %s\n", addr1.ToString());                
                }
            }        
        }
        
        LogPrintf("Seed node is set, trying to connect to %s...\n",mc_gState->GetSeedNode());
        CAddress addr;
        bool outcome=OpenNetworkConnection(addr, NULL, mc_gState->GetSeedNode());
        if(outcome)
        {
            addrman.GetMCAddrMan()->SetOutcome(addr1,0,outcome);
        }
        else
        {
            if(addr1.IsValid())
            {
                addrman.GetMCAddrMan()->SetOutcome(addr1,0,outcome);                            
            }
        }
    }

/* MCHN END */    
    
    set<CService> setLocalAddrCopy;
    uint32_t mcaddr_count[MC_AMM_MODE_COUNT];
    if(MCP_ANYONE_CAN_CONNECT)
    {
        OutConnectionsAlgoritm=0;
    }
    while(OutConnectionsAlgoritm == 1)
    {
        if(!GetBoolArg("-addnodeonly",false))
        {
            {
                LOCK(cs_setLocalAddr);        
                BOOST_FOREACH(CService laddr, setLocalAddr) 
                {                
                    setLocalAddrCopy.insert(laddr);
                }
            }
            int nOutBound=addrman.GetMCAddrMan()->PrepareSelect(setLocalAddrCopy,mcaddr_count);
            int nRemaining=nMaxOutConnections-nOutBound;
            
            if( nRemaining * 2 < (int)mcaddr_count[MC_AMM_RECENT_SUCCESS] )
            {
                LogPrintf("Too many addresses to connect (%d) for remaining %d slots, falling back to stochastic algorithm\n",mcaddr_count[MC_AMM_RECENT_SUCCESS],nRemaining);
                OutConnectionsAlgoritm=0;
            }
            if(MCP_ANYONE_CAN_CONNECT)
            {
                LogPrintf("anyone-can-connect=true, falling back to stochastic algorithm\n");
                OutConnectionsAlgoritm=0;
            }
            
            CMCAddrInfo *addr;
            int nTotal=0;
            for(uint32_t mode=MC_AMM_MIN_MODE;mode<MC_AMM_MODE_COUNT;mode++)
            {
                nTotal+=mcaddr_count[mode];
            }

            for(uint32_t mode=MC_AMM_MIN_MODE;mode<MC_AMM_MODE_COUNT;mode++)
            {
                double ratio=1.;
                int sleep_time=500;
                if(mode == MC_AMM_RECENT_SUCCESS)
                {
                    sleep_time=100;
                }
                else
                {
                    if(nRemaining * 4 < nMaxOutConnections)
                    {
                        sleep_time=2000;                        
                    }
                }
                
                if(nRemaining > 0)
                {
                    if(nRemaining < nTotal)
                    {
                        if(nRemaining < (int)mcaddr_count[mode])
                        {
                            ratio=(double)nRemaining/(double)mcaddr_count[mode];
                        }
                        if((mode == MC_AMM_OLD_FAIL) || (mode == MC_AMM_TRIED_NET))
                        {
                            ratio /= 2.;
                        }
                    }

                    while((addr = addrman.GetMCAddrMan()->Select(mode)))
                    {
                        if(mc_RandomDouble() <= ratio)
                        {
                            CSemaphoreGrant grant(*semOutbound);
                            CAddress addrConnect(addr->GetNetAddress());
                                
                            const char *pszDest=NULL;
                            if( (mc_gState->m_pSeedNode == NULL) && !fSeedAbandoned )
                            {
                                if(mc_gState->GetSeedNode())
                                {
                                    if(strcmp(addrConnect.ToStringIPPort().c_str(),mc_gState->GetSeedNode()) == 0)
                                    {
                                        pszDest=mc_gState->GetSeedNode();
                                    }                                    
                                }
                            }
                            
                            bool outcome=OpenNetworkConnection(addrConnect, &grant, pszDest);
                            if(outcome)
                            {
                                addrman.GetMCAddrMan()->SetOutcome(addr->GetNetAddress(),0,outcome);
                                nRemaining--;
                            }                                
                            else
                            {
                                addrman.GetMCAddrMan()->SetOutcome(addr->GetNetAddress(),0,outcome);                            
                                addrman.GetMCAddrMan()->SetOutcome(addr->GetNetAddress(),addr->GetMCAddress(),outcome);                            
                            }
                            
//                            addrman.SCSetSelected(addrConnect);                 
//                            addrman.SetSC(false,GetAdjustedTime());
                            
                            boost::this_thread::interruption_point();                
                            MilliSleep(sleep_time);
                        }
                        nTotal--;
                    }
                }
            }
        }   
        
        boost::this_thread::interruption_point();                
        MilliSleep(500);
    }
    
    
    // Initiate network connections
    int64_t nStart = GetTime();
    while (true)
    {
        ProcessOneShot();

        MilliSleep(500);

        CSemaphoreGrant grant(*semOutbound);
        boost::this_thread::interruption_point();

        
       
        // Add seed nodes if DNS seeds are all down (an infrastructure attack?).
        if (addrman.size() == 0 && (GetTime() - nStart > 60)) {
            static bool done = false;
            if (!done) {
                LogPrintf("Adding fixed seed nodes as DNS doesn't seem to be available.\n");
                addrman.Add(Params().FixedSeeds(), CNetAddr("127.0.0.1"));
                done = true;
            }
        }

        //
        // Choose an address to connect to based on most recently seen
        //
        CAddress addrConnect;

        
        // Only connect out to one peer per network group (/16 for IPv4).
        // Do this here so we don't have to critsect vNodes inside mapAddresses critsect.
        int nOutbound = 0;
        int nNodes = 0;
        set<vector<unsigned char> > setConnected;
        set<string> setConnectedVerifiedAddresses;
        set<string> setConnectedFromAddresses;
        set<string> setConnectedToAddresses;
        {
            LOCK(cs_vNodes);
            nNodes=(int)vNodes.size();
            BOOST_FOREACH(CNode* pnode, vNodes) {                
                if (!pnode->fInbound) {
                    setConnected.insert(pnode->addr.GetGroup());
                    nOutbound++;
                }
/* MCHN START */                
                if (pnode->fInbound) 
                {                
                    if (((CNetAddr)pnode->addr) == (CNetAddr)pnode->addrFromVersion)
                    {
                        setConnectedVerifiedAddresses.insert(pnode->addrFromVersion.ToStringIPPort());
                    }
                    setConnectedFromAddresses.insert(pnode->addrFromVersion.ToStringIPPort());
                }
                else
                {
                    setConnectedToAddresses.insert(pnode->addr.ToStringIPPort());                    
                }
/* MCHN END */                
            }
        }

        int64_t nANow = GetAdjustedTime();

        addrman.SCRecalculate(nANow);
        
        int nTries = 0;
        while (true)
        {
            // use an nUnkBias between 10 (no outgoing connections) and 90 (8 outgoing connections)
            CAddress addr = addrman.Select(10 + min(nOutbound,8)*10,nNodes);

            // if we selected an invalid address, restart
/* MCHN START */            
            if(mc_gState->m_NetworkParams->IsProtocolMultichain())
            {
                if (!addr.IsValid() || (IsLocal(addr) && (addr.GetPort() == GetListenPort())))
                {
                    addrman.SetSC(true,nANow);
                    break;                
                }
                
                LOCK(cs_setLocalAddr);        
                if(setLocalAddr.find(addr) != setLocalAddr.end())
                {
                    addrman.SetSC(true,nANow);
                    break;
                }
                MilliSleep(100);
            }
            else
            {
/* MCHN END */            
                if (!addr.IsValid() || setConnected.count(addr.GetGroup()) || IsLocal(addr))
                {
                    addrman.SetSC(true,nANow);
                    break;
                }
/* MCHN START */            
            }
/* MCHN END */            

            // If we didn't find an appropriate destination after trying 100 addresses fetched from addrman,
            // stop this loop, and let the outer loop run again (which sleeps, adds seed nodes, recalculates
            // already-connected network ranges, ...) before trying new addrman addresses.
            nTries++;
            if(mc_gState->m_NetworkParams->IsProtocolMultichain())
            {
                if (nTries > 20)                
                    break;
            }
            else
            {
                if (nTries > 100)                
                    break;                
            }
            
            if (IsLimited(addr))
            {
                continue;
            }

            // only consider very recently tried nodes after 30 failed attempts
/* MCHN START */            
            if(mc_gState->m_NetworkParams->IsProtocolMultichain() == 0)
            {
/* MCHN END */            
            if (nANow - addr.nLastTry < 600 && nTries < 30)
            {
                continue;
            }
/* MCHN START */            
            }
/* MCHN END */            

            
            // do not allow non-default ports, unless after 50 invalid addresses selected already
/* MCHN START */            
            if(mc_gState->m_NetworkParams->IsProtocolMultichain() == 0)
            {
/* MCHN END */            
                if (addr.GetPort() != Params().GetDefaultPort() && nTries < 50)
                {
                    continue;
                }
/* MCHN START */            
            }


            if(mc_gState->m_NetworkParams->IsProtocolMultichain())
            {
                
                if(setConnectedVerifiedAddresses.count(addr.ToStringIPPort()))
                {
                    addrman.SetSC(false,nANow);
                    continue;
                }
                if(setConnectedToAddresses.count(addr.ToStringIPPort()))
                {
                    addrman.SetSC(false,nANow);
                    continue;
                }
                if (nANow - addr.nLastTry < 600)
                {
                    if(setConnectedFromAddresses.count(addr.ToStringIPPort()))
                    {
                        addrman.SetSC(false,nANow);
                        continue;
                    }
                }
            }
            
/* MCHN END */            

            addrConnect = addr;
            break;
        }

/* MCHN START */        
        if(!GetBoolArg("-addnodeonly",false))
        {
            if (addrConnect.IsValid())
            {
                OpenNetworkConnection(addrConnect, &grant);
                addrman.SetSC(false,nANow);
            }
        }
/* MCHN END */        
    }
}

void ThreadOpenAddedConnections()
{
    {
        LOCK(cs_vAddedNodes);
        vAddedNodes = mapMultiArgs["-addnode"];
    }

    if (HaveNameProxy()) {
        while(true) {
            list<string> lAddresses(0);
            {
                LOCK(cs_vAddedNodes);
                BOOST_FOREACH(string& strAddNode, vAddedNodes)
                    lAddresses.push_back(strAddNode);
            }
            BOOST_FOREACH(string& strAddNode, lAddresses) {
                CAddress addr;
                CSemaphoreGrant grant(*semOutbound);
                OpenNetworkConnection(addr, &grant, strAddNode.c_str());
                MilliSleep(500);
            }
            MilliSleep(120000); // Retry every 2 minutes
        }
    }

    for (unsigned int i = 0; true; i++)
    {
        list<string> lAddresses(0);
        {
            LOCK(cs_vAddedNodes);
            BOOST_FOREACH(string& strAddNode, vAddedNodes)
                lAddresses.push_back(strAddNode);
        }

        list<vector<CService> > lservAddressesToAdd(0);
        BOOST_FOREACH(string& strAddNode, lAddresses)
        {
            vector<CService> vservNode(0);
            if(Lookup(strAddNode.c_str(), vservNode, Params().GetDefaultPort(), fNameLookup, 0))
            {
                lservAddressesToAdd.push_back(vservNode);
                {
                    LOCK(cs_setservAddNodeAddresses);
                    BOOST_FOREACH(CService& serv, vservNode)
                        setservAddNodeAddresses.insert(serv);
                }
            }
        }
        // Attempt to connect to each IP for each addnode entry until at least one is successful per addnode entry
        // (keeping in mind that addnode entries can have many IPs if fNameLookup)
        {
            LOCK(cs_vNodes);
            BOOST_FOREACH(CNode* pnode, vNodes)
                for (list<vector<CService> >::iterator it = lservAddressesToAdd.begin(); it != lservAddressesToAdd.end(); it++)
                    BOOST_FOREACH(CService& addrNode, *(it))
                        if (pnode->addr == addrNode)
                        {
                            it = lservAddressesToAdd.erase(it);
                            it--;
                            break;
                        }
        }
        BOOST_FOREACH(vector<CService>& vserv, lservAddressesToAdd)
        {
            CSemaphoreGrant grant(*semOutbound);
            OpenNetworkConnection(CAddress(vserv[i % vserv.size()]), &grant, NULL, false, true);
            MilliSleep(500);
        }
        if(GetBoolArg("-addnodeonly",false))
        {
            MilliSleep(1000); // Retry every 1 second
        }
        else
        {
            MilliSleep(120000); // Retry every 2 minutes
        }
    }
}

// if successful, this moves the passed grant to the constructed node
bool OpenNetworkConnection(const CAddress& addrConnect, CSemaphoreGrant *grantOutbound, const char *pszDest, bool fOneShot, bool fAllowSameIP)
{
    //
    // Initiate outbound network connection
    //
    boost::this_thread::interruption_point();
    if (!pszDest) {
//        if (IsLocal(addrConnect)  ||
          if ((IsLocal(addrConnect) && (addrConnect.GetPort() == GetListenPort())) ||                
//            FindNode((CNetAddr)addrConnect) || CNode::IsBanned(addrConnect) ||
            (FindNode((CNetAddr)addrConnect) && !IsLocal(addrConnect) && !fAllowSameIP) || CNode::IsBanned(addrConnect) ||
            FindNode(addrConnect.ToStringIPPort()))
          {
            if(fDebug)LogPrint("net","net: Node found: %s\n",addrConnect.ToStringIPPort().c_str());
             return false;
          }
    } else if (FindNode(pszDest))
    {
        if(fDebug)LogPrint("net","net: Node found: %s\n",pszDest);
        return false;
    }
/* MCHN START */    
    if(pszDest)
    {
        if(fDebug)LogPrint("net","net: Trying to connect to %s (by address)\n",pszDest);
    }
    else
    {
        if(fDebug)LogPrint("net","net: Trying to connect to %s\n",addrConnect.ToStringIPPort().c_str());
    }
/* MCHN END */    
    
    CNode* pnode = ConnectNode(addrConnect, pszDest);
    boost::this_thread::interruption_point();

    FlushBlockIndexCache();    
    
    if (!pnode)
    {
/* MCHN START */    
        if(fDebug)LogPrint("net","net: Connection not established\n");
/* MCHN END */    
        return false;
    }
    if (grantOutbound)
        grantOutbound->MoveTo(pnode->grantOutbound);
    pnode->fNetworkNode = true;
    if (fOneShot)
        pnode->fOneShot = true;

    if(fDebug)LogPrint("net","net: Connection to %s established\n",addrConnect.ToStringIPPort().c_str());
    return true;
}


void ThreadMessageHandler()
{
    SetThreadPriority(THREAD_PRIORITY_BELOW_NORMAL);
    while (true)
    {
        vector<CNode*> vNodesCopy;
        {
            LOCK(cs_vNodes);
            vNodesCopy = vNodes;
            BOOST_FOREACH(CNode* pnode, vNodesCopy) {
                pnode->AddRef();
            }
        }

        // Poll the connected nodes for messages
        CNode* pnodeTrickle = NULL;
        if (!vNodesCopy.empty())
            pnodeTrickle = vNodesCopy[GetRand(vNodesCopy.size())];

        bool fSleep = true;

        BOOST_FOREACH(CNode* pnode, vNodesCopy)
        {            
            if (pnode->fDisconnect)
                continue;

            if(GetTime() > pnode->nTimeToDisconnect)
            {
                LogPrintf("mchn: Connection was established for too long, disconnecting, peer=%d\n",pnode->id);        
                pnode->fDisconnect=true;
                continue;
            }
            
            // Receive messages
            {
                TRY_LOCK(pnode->cs_vRecvMsg, lockRecv);
                if (lockRecv)
                {
                    if (!g_signals.ProcessMessages(pnode))
                    {
/* MCHN START */                    
                        if(fDebug)LogPrint("net","socket closed because of error in message processing\n");
/* MCHN END */                    
                        pnode->CloseSocketDisconnect();
                    }                    
                    if (pnode->nSendSize < SendBufferSize())
                    {
                        if (!pnode->vRecvGetData.empty() || (!pnode->vRecvMsg.empty() && pnode->vRecvMsg[0].complete()))
                        {
                            fSleep = false;
                        }
                    }
                }
            }
                
            pnode->TotalBuffersSize();
            FlushBlockIndexCache();
            boost::this_thread::interruption_point();

/* MCHN START */
/* Avoid communication with the node before connection is properly established with verackack and the network paramset is valid*/            
            if (pnode->fDisconnect)
                continue;
            
        if((mc_gState->m_NetworkParams->m_Status == MC_PRM_STATUS_VALID) && pnode->fParameterSetVerified)
        {
/* MCHN END */
            // Send messages
            if(nMessageHandlerThreads == 0)                
            {
                TRY_LOCK(pnode->cs_vSend, lockSend);
                if (lockSend)
                    g_signals.SendMessages(pnode, pnode == pnodeTrickle);
            }
            else                                                                // We cannot take this lock, possible deadlock 
            {
                g_signals.SendMessages(pnode, pnode == pnodeTrickle);                
            }
            FlushBlockIndexCache();                    
            boost::this_thread::interruption_point();
/* MCHN START */            
        }

/* MCHN END */
        }

        {
            LOCK(cs_vNodes);
            BOOST_FOREACH(CNode* pnode, vNodesCopy)
                pnode->Release();
        }

        if (fSleep)
            MilliSleep(100);
    }
}

void ThreadDataMessageHandler()
{
    SetThreadPriority(THREAD_PRIORITY_BELOW_NORMAL);
    while (true)
    {
        boost::this_thread::interruption_point();
        vector<CNode*> vNodesCopy;
        {
            LOCK(cs_vNodes);
            vNodesCopy = vNodes;
            BOOST_FOREACH(CNode* pnode, vNodesCopy) {
                pnode->AddRef();
            }
        }

        bool fSleep = true;

        BOOST_FOREACH(CNode* pnode, vNodesCopy)
        {
            if (pnode->fDisconnect)
                continue;

            CNetMessage msg(SER_NETWORK, pnode->nRecvVersion);
            bool fFound=false;
            // Receive messages
            {
                LOCK(pnode->cs_vRecvDataMsg);
                std::deque<CNetMessage>::iterator it = pnode->vRecvDataMsg.begin();
                if (!pnode->fDisconnect && it != pnode->vRecvDataMsg.end())
                {
                    msg = *it;
                    fFound=true;
                }
            }
            if(fFound)
            {
                if(g_signals.ProcessDataMessage(pnode,msg))
                {
                    if (!pnode->fDisconnect)
                    {
                        LOCK(pnode->cs_vRecvDataMsg);
                        pnode->vRecvDataMsg.pop_front();
                        if (!pnode->vRecvDataMsg.empty())
                        {
                            fSleep = false;
                        }
                    }
                }
            }
            
            FlushBlockIndexCache();                    
            boost::this_thread::interruption_point();

            if (pnode->fDisconnect)
                continue;
        }

        boost::this_thread::interruption_point();
        
        {
            LOCK(cs_vNodes);
            BOOST_FOREACH(CNode* pnode, vNodesCopy)
                pnode->Release();
        }

        boost::this_thread::interruption_point();
        
        if (fSleep)
            MilliSleep(100);
    }
}

void ThreadTxDataMessageHandler()
{
    SetThreadPriority(THREAD_PRIORITY_BELOW_NORMAL);
    while (true)
    {
        boost::this_thread::interruption_point();
        vector<CNode*> vNodesCopy;
        {
            LOCK(cs_vNodes);
            vNodesCopy = vNodes;
            BOOST_FOREACH(CNode* pnode, vNodesCopy) {
                pnode->AddRef();
            }
        }

        bool fSleep = true;

        BOOST_FOREACH(CNode* pnode, vNodesCopy)
        {
            if (pnode->fDisconnect)
                continue;

            CNetMessage msg(SER_NETWORK, pnode->nRecvVersion);
            bool fFound=false;
            // Receive messages
            {
                LOCK(pnode->cs_vRecvTxDataMsg);
                std::deque<CNetMessage>::iterator it = pnode->vRecvTxDataMsg.begin();
                if (!pnode->fDisconnect && it != pnode->vRecvTxDataMsg.end())
                {
                    msg = *it;
                    fFound=true;
                }
            }
            if(fFound)
            {
                if(g_signals.ProcessDataMessage(pnode,msg))
                {
                    if (!pnode->fDisconnect)
                    {
                        LOCK(pnode->cs_vRecvTxDataMsg);
                        pnode->vRecvTxDataMsg.pop_front();
                        if (!pnode->vRecvTxDataMsg.empty())
                        {
                            fSleep = false;
                        }
                    }
                }
            }
            
            FlushBlockIndexCache();                    
            boost::this_thread::interruption_point();

            if (pnode->fDisconnect)
                continue;
        }

        boost::this_thread::interruption_point();
        
        {
            LOCK(cs_vNodes);
            BOOST_FOREACH(CNode* pnode, vNodesCopy)
                pnode->Release();
        }

        boost::this_thread::interruption_point();
        
        if (fSleep)
            MilliSleep(100);
    }
}

void ThreadOffchainMessageHandler()
{
    SetThreadPriority(THREAD_PRIORITY_BELOW_NORMAL);
    while (true)
    {
        boost::this_thread::interruption_point();
        vector<CNode*> vNodesCopy;
        {
            LOCK(cs_vNodes);
            vNodesCopy = vNodes;
            BOOST_FOREACH(CNode* pnode, vNodesCopy) {
                pnode->AddRef();
            }
        }

        bool fSleep = true;

        BOOST_FOREACH(CNode* pnode, vNodesCopy)
        {
            if (pnode->fDisconnect)
                continue;

            CNetMessage msg(SER_NETWORK, pnode->nRecvVersion);
            bool fFound=false;
            // Receive messages
            {
                LOCK(pnode->cs_vRecvOffchainMsg);
                std::deque<CNetMessage>::iterator it = pnode->vRecvOffchainMsg.begin();
                if (!pnode->fDisconnect && it != pnode->vRecvOffchainMsg.end())
                {
                    msg = *it;
                    fFound=true;
                }
            }
            if(fFound)
            {
                if(g_signals.ProcessDataMessage(pnode,msg))
                {
                    if (!pnode->fDisconnect)
                    {
                        LOCK(pnode->cs_vRecvOffchainMsg);
                        pnode->vRecvOffchainMsg.pop_front();
                        if (!pnode->vRecvOffchainMsg.empty())
                        {
                            fSleep = false;
                        }
                    }
                }
            }
            
            boost::this_thread::interruption_point();

            if (pnode->fDisconnect)
                continue;
        }

        boost::this_thread::interruption_point();
        
        {
            LOCK(cs_vNodes);
            BOOST_FOREACH(CNode* pnode, vNodesCopy)
                pnode->Release();
        }

        boost::this_thread::interruption_point();
        
        if (fSleep)
            MilliSleep(100);
    }
}


void ThreadGetDataMessageHandler()
{
    SetThreadPriority(THREAD_PRIORITY_BELOW_NORMAL);
    while (true)
    {
        boost::this_thread::interruption_point();
        vector<CNode*> vNodesCopy;
        {
            LOCK(cs_vNodes);
            vNodesCopy = vNodes;
            BOOST_FOREACH(CNode* pnode, vNodesCopy) {
                pnode->AddRef();
            }
        }

        bool fSleep = true;

        BOOST_FOREACH(CNode* pnode, vNodesCopy)
        {
            if (pnode->fDisconnect)
                continue;

            {
                LOCK(pnode->cs_vRecvGetData);
                pnode->vRecvGetData.insert(pnode->vRecvGetData.end(), pnode->vRecvGetDataBuf.begin(), pnode->vRecvGetDataBuf.end());    
                pnode->vRecvGetDataBuf.clear();
            }
                
            if (!g_signals.ProcessGetData(pnode))
            {
                if(fDebug)LogPrint("net","socket closed because of error in message processing\n");
                pnode->CloseSocketDisconnect();
            }

            if (!pnode->vRecvGetData.empty())
            {
                fSleep = false;
            }
        
            FlushBlockIndexCache();                    
            boost::this_thread::interruption_point();

            if (pnode->fDisconnect)
                continue;
        }


        boost::this_thread::interruption_point();
        
        {
            LOCK(cs_vNodes);
            BOOST_FOREACH(CNode* pnode, vNodesCopy)
                pnode->Release();
        }

        boost::this_thread::interruption_point();
        
        if (fSleep)
            MilliSleep(100);
    }
}


bool BindListenPort(const CService &addrBind, string& strError, bool fWhitelisted)
{
    strError = "";
    int nOne = 1;

    // Create socket for listening for incoming connections
    struct sockaddr_storage sockaddr;
    socklen_t len = sizeof(sockaddr);
    if (!addrBind.GetSockAddr((struct sockaddr*)&sockaddr, &len))
    {
        strError = strprintf("Error: Bind address family for %s not supported", addrBind.ToString());
        LogPrintf("%s\n", strError);
        return false;
    }

    SOCKET hListenSocket = socket(((struct sockaddr*)&sockaddr)->sa_family, SOCK_STREAM, IPPROTO_TCP);
    if (hListenSocket == INVALID_SOCKET)
    {
        strError = strprintf("Error: Couldn't open socket for incoming connections (socket returned error %s)", NetworkErrorString(WSAGetLastError()));
        LogPrintf("%s\n", strError);
        return false;
    }

#ifndef WIN32
#ifdef SO_NOSIGPIPE
    // Different way of disabling SIGPIPE on BSD
    setsockopt(hListenSocket, SOL_SOCKET, SO_NOSIGPIPE, (void*)&nOne, sizeof(int));
#endif
    // Allow binding if the port is still in TIME_WAIT state after
    // the program was closed and restarted. Not an issue on windows!
    setsockopt(hListenSocket, SOL_SOCKET, SO_REUSEADDR, (void*)&nOne, sizeof(int));
#endif

    // Set to non-blocking, incoming connections will also inherit this
    if (!SetSocketNonBlocking(hListenSocket, true)) {
        strError = strprintf("BindListenPort: Setting listening socket to non-blocking failed, error %s\n", NetworkErrorString(WSAGetLastError()));
        LogPrintf("%s\n", strError);
        return false;
    }

    // some systems don't have IPV6_V6ONLY but are always v6only; others do have the option
    // and enable it by default or not. Try to enable it, if possible.
    if (addrBind.IsIPv6()) {
#ifdef IPV6_V6ONLY
#ifdef WIN32
        setsockopt(hListenSocket, IPPROTO_IPV6, IPV6_V6ONLY, (const char*)&nOne, sizeof(int));
#else
        setsockopt(hListenSocket, IPPROTO_IPV6, IPV6_V6ONLY, (void*)&nOne, sizeof(int));
#endif
#endif
#ifdef WIN32
        int nProtLevel = PROTECTION_LEVEL_UNRESTRICTED;
        setsockopt(hListenSocket, IPPROTO_IPV6, IPV6_PROTECTION_LEVEL, (const char*)&nProtLevel, sizeof(int));
#endif
    }

    if (::bind(hListenSocket, (struct sockaddr*)&sockaddr, len) == SOCKET_ERROR)
    {
        int nErr = WSAGetLastError();
        if (nErr == WSAEADDRINUSE)
            strError = strprintf(_("Unable to bind to %s on this computer. MultiChain Core is probably already running."), addrBind.ToString());
        else
            strError = strprintf(_("Unable to bind to %s on this computer (bind returned error %s)"), addrBind.ToString(), NetworkErrorString(nErr));
        LogPrintf("%s\n", strError);
        CloseSocket(hListenSocket);
        return false;
    }
    LogPrintf("Bound to %s\n", addrBind.ToString());

    // Listen for incoming connections
    if (listen(hListenSocket, SOMAXCONN) == SOCKET_ERROR)
    {
        strError = strprintf(_("Error: Listening for incoming connections failed (listen returned error %s)"), NetworkErrorString(WSAGetLastError()));
        LogPrintf("%s\n", strError);
        CloseSocket(hListenSocket);
        return false;
    }

    vhListenSocket.push_back(ListenSocket(hListenSocket, fWhitelisted));

    if (addrBind.IsRoutable() && fDiscover && !fWhitelisted)
        AddLocal(addrBind, LOCAL_BIND);

    return true;
}

void static Discover(boost::thread_group& threadGroup)
{
    if (!fDiscover)
        return;

#ifdef WIN32
    // Get local host IP
    char pszHostName[256] = "";
    if (gethostname(pszHostName, sizeof(pszHostName)) != SOCKET_ERROR)
    {
        vector<CNetAddr> vaddr;
        if (LookupHost(pszHostName, vaddr))
        {
            BOOST_FOREACH (const CNetAddr &addr, vaddr)
            {
                if (AddLocal(addr, LOCAL_IF))
                    LogPrintf("%s: %s - %s\n", __func__, pszHostName, addr.ToString());
            }
        }
    }
#else
    // Get local host ip
    struct ifaddrs* myaddrs;
    if (getifaddrs(&myaddrs) == 0)
    {
        for (struct ifaddrs* ifa = myaddrs; ifa != NULL; ifa = ifa->ifa_next)
        {
            if (ifa->ifa_addr == NULL) continue;
            if ((ifa->ifa_flags & IFF_UP) == 0) continue;
            if (strcmp(ifa->ifa_name, "lo") == 0) continue;
            if (strcmp(ifa->ifa_name, "lo0") == 0) continue;
            if (ifa->ifa_addr->sa_family == AF_INET)
            {
                struct sockaddr_in* s4 = (struct sockaddr_in*)(ifa->ifa_addr);
                CNetAddr addr(s4->sin_addr);
                if (AddLocal(addr, LOCAL_IF))
                    LogPrintf("%s: IPv4 %s: %s\n", __func__, ifa->ifa_name, addr.ToString());
            }
            else if (ifa->ifa_addr->sa_family == AF_INET6)
            {
                struct sockaddr_in6* s6 = (struct sockaddr_in6*)(ifa->ifa_addr);
                CNetAddr addr(s6->sin6_addr);
                if (AddLocal(addr, LOCAL_IF))
                    LogPrintf("%s: IPv6 %s: %s\n", __func__, ifa->ifa_name, addr.ToString());
            }
        }
        freeifaddrs(myaddrs);
    }
#endif
}


void StartNode(boost::thread_group& threadGroup)
{
    uiInterface.InitMessage(_("Loading addresses..."));
    // Load addresses for peers.dat
    int64_t nStart = GetTimeMillis();
    {
/* MCHN START */
        if(mc_gState->m_NetworkParams->m_Status != MC_PRM_STATUS_EMPTY) 
        {
/* MCHN END */            
            CAddrDB adb;
            if (!adb.Read(addrman))
                LogPrintf("Invalid or missing peers.dat; recreating\n");
/* MCHN START */
        }
/* MCHN END */
    }
    LogPrintf("Loaded %i addresses from peers.dat  %dms\n",
           addrman.size(), GetTimeMillis() - nStart);
    fAddressesInitialized = true;

    if (semOutbound == NULL) {
        // initialize semaphore
//        int nMaxOutbound = min((int)GetArg("-maxoutconnections",MAX_OUTBOUND_CONNECTIONS), nMaxConnections);
        int nMaxOutbound = min(nMaxOutConnections, nMaxConnections);
        semOutbound = new CSemaphore(nMaxOutbound);
    }

    if (pnodeLocalHost == NULL)
        pnodeLocalHost = new CNode(INVALID_SOCKET, CAddress(CService("127.0.0.1", 0), nLocalServices));

    Discover(threadGroup);

    //
    // Start threads
    //

    if (!GetBoolArg("-dnsseed", true))
        LogPrintf("DNS seeding disabled\n");
    else
        threadGroup.create_thread(boost::bind(&TraceThread<void (*)()>, "dnsseed", &ThreadDNSAddressSeed));

    // Map ports with UPnP
//    MapPort(GetBoolArg("-upnp", DEFAULT_UPNP));
    MapPort(GetBoolArg("-upnp", false));

    // Send and receive from sockets, accept connections
    threadGroup.create_thread(boost::bind(&TraceThread<void (*)()>, "net", &ThreadSocketHandler));

    // Initiate outbound connections from -addnode
    threadGroup.create_thread(boost::bind(&TraceThread<void (*)()>, "addcon", &ThreadOpenAddedConnections));

    // Initiate outbound connections
    threadGroup.create_thread(boost::bind(&TraceThread<void (*)()>, "opencon", &ThreadOpenConnections));

    // Process messages
    threadGroup.create_thread(boost::bind(&TraceThread<void (*)()>, "msghand", &ThreadMessageHandler));

    if(nMessageHandlerThreads & MC_MHT_GETDATA)
    {
        // Process getdata messages
        threadGroup.create_thread(boost::bind(&TraceThread<void (*)()>, "gmsghand", &ThreadGetDataMessageHandler));        
    }
    if(nMessageHandlerThreads & MC_MHT_PROCESSDATA)
    {
        // Process block data messages
        threadGroup.create_thread(boost::bind(&TraceThread<void (*)()>, "dmsghand", &ThreadDataMessageHandler));

        if(nMessageHandlerThreads & MC_MHT_PROCESSTXDATA)
        {
            // Process tx data messages
            threadGroup.create_thread(boost::bind(&TraceThread<void (*)()>, "tmsghand", &ThreadTxDataMessageHandler));
        }
    }
    if(nMessageHandlerThreads & MC_MHT_OFFCHAIN)
    {
        // Process offchain messages
        threadGroup.create_thread(boost::bind(&TraceThread<void (*)()>, "omsghand", &ThreadOffchainMessageHandler));
    }
    // Dump network addresses
    threadGroup.create_thread(boost::bind(&LoopForever<void (*)()>, "dumpaddr", &DumpAddresses, DUMP_ADDRESSES_INTERVAL * 1000));
}

void EncNetCleanup()
{
    {
        LOCK(cs_vNodes);
        if(pEF)
        {
            BOOST_FOREACH(CNode *pnode, vNodes)
            {
                if(pnode->pEntData)
                {
                    pEF->NET_FreeNodeData(pnode->pEntData);
                    pnode->pEntData=NULL;
                }
            }                    
        }
    }    
}

bool StopNode()
{
    LogPrintf("StopNode()\n");
    MapPort(false);
    if (semOutbound)
        for (int i=0; i< nMaxOutConnections; i++)// i<GetArg("-maxoutconnections",MAX_OUTBOUND_CONNECTIONS); i++)
            semOutbound->post();

    if (fAddressesInitialized)
    {
        DumpAddresses();
        fAddressesInitialized = false;
    }

/* MCHN START */
    EncNetCleanup();
    LogPrintf("Node stopped\n");
/* MCHN END */
    return true;
}

class CNetCleanup
{
public:
    CNetCleanup() {}

    ~CNetCleanup()
    {
        // Close sockets
        BOOST_FOREACH(CNode* pnode, vNodes)
            if (pnode->hSocket != INVALID_SOCKET)
                CloseSocket(pnode->hSocket);
        BOOST_FOREACH(ListenSocket& hListenSocket, vhListenSocket)
            if (hListenSocket.socket != INVALID_SOCKET)
                if (!CloseSocket(hListenSocket.socket))
                    LogPrintf("CloseSocket(hListenSocket) failed with error %s\n", NetworkErrorString(WSAGetLastError()));

        // clean up some globals (to help leak detection)
        BOOST_FOREACH(CNode *pnode, vNodes)
            delete pnode;
        BOOST_FOREACH(CNode *pnode, vNodesDisconnected)
            delete pnode;
        vNodes.clear();
        vNodesDisconnected.clear();
        vhListenSocket.clear();
        delete semOutbound;
        semOutbound = NULL;
        delete pnodeLocalHost;
        pnodeLocalHost = NULL;

#ifdef WIN32
        // Shutdown Windows Sockets
        WSACleanup();
#endif
    }
}
instance_of_cnetcleanup;







void RelayTransaction(const CTransaction& tx)
{
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss.reserve(10000);
    ss << tx;
    RelayTransaction(tx, ss);
}

void RelayTransaction(const CTransaction& tx, const CDataStream& ss)
{
    CInv inv(MSG_TX, tx.GetHash());
    {
        LOCK(cs_mapRelay);
        // Expire old relay messages
        while (!vRelayExpiration.empty() && vRelayExpiration.front().first < GetTime())
        {
            mapRelay.erase(vRelayExpiration.front().second);
            vRelayExpiration.pop_front();
        }

        // Save original serialized message so newer versions are preserved
        mapRelay.insert(std::make_pair(inv, ss));
//        vRelayExpiration.push_back(std::make_pair(GetTime() + 15 * 60, inv));
        vRelayExpiration.push_back(std::make_pair(GetTime() + 2 * Params().TargetSpacing(), inv));
    }
    LOCK(cs_vNodes);
    BOOST_FOREACH(CNode* pnode, vNodes)
    {
        if(!pnode->fRelayTxes)
            continue;
        {
            LOCK(pnode->cs_inventory);
            if(!pnode->fReadyForTxInv && (OrphanHandlerVersion == 1))
                continue;
        }
        LOCK(pnode->cs_filter);
        if (pnode->pfilter)
        {
            if (pnode->pfilter->IsRelevantAndUpdate(tx))
                pnode->PushInventory(inv);
        } else
            pnode->PushInventory(inv);
    }
}

void CNode::RecordBytesRecv(uint64_t bytes)
{
    LOCK(cs_totalBytesRecv);
    nTotalBytesRecv += bytes;
}

void CNode::RecordBytesSent(uint64_t bytes)
{
    LOCK(cs_totalBytesSent);
    nTotalBytesSent += bytes;
}

uint64_t CNode::GetTotalBytesRecv()
{
    LOCK(cs_totalBytesRecv);
    return nTotalBytesRecv;
}

uint64_t CNode::GetTotalBytesSent()
{
    LOCK(cs_totalBytesSent);
    return nTotalBytesSent;
}

void CNode::Fuzz(int nChance)
{
    if (!fSuccessfullyConnected) return; // Don't fuzz initial handshake
    if (GetRand(nChance) != 0) return; // Fuzz 1 of every nChance messages

    switch (GetRand(3))
    {
    case 0:
        // xor a random byte with a random value:
        if (!ssSend.empty()) {
            CDataStream::size_type pos = GetRand(ssSend.size());
            ssSend[pos] ^= (unsigned char)(GetRand(256));
        }
        break;
    case 1:
        // delete a random byte:
        if (!ssSend.empty()) {
            CDataStream::size_type pos = GetRand(ssSend.size());
            ssSend.erase(ssSend.begin()+pos);
        }
        break;
    case 2:
        // insert a random byte at a random position
        {
            CDataStream::size_type pos = GetRand(ssSend.size());
            char ch = (char)GetRand(256);
            ssSend.insert(ssSend.begin()+pos, ch);
        }
        break;
    }
    // Chance of more than one change half the time:
    // (more changes exponentially less likely):
    Fuzz(2);
}

//
// CAddrDB
//

CAddrDB::CAddrDB()
{
    pathAddr = GetDataDir() / "peers.dat";
}

bool CAddrDB::Write(const CAddrMan& addr)
{
    // Generate random temporary filename
    unsigned short randv = 0;
    GetRandBytes((unsigned char*)&randv, sizeof(randv));
    std::string tmpfn = strprintf("peers.dat.%04x", randv);

    // serialize addresses, checksum data up to that point, then append csum
    CDataStream ssPeers(SER_DISK, CLIENT_VERSION);
    ssPeers << FLATDATA(Params().MessageStart());
    ssPeers << addr;
    uint256 hash = Hash(ssPeers.begin(), ssPeers.end());
    ssPeers << hash;

    // open temp output file, and associate with CAutoFile
    boost::filesystem::path pathTmp = GetDataDir() / tmpfn;
    FILE *file = fopen(pathTmp.string().c_str(), "wb");
    CAutoFile fileout(file, SER_DISK, CLIENT_VERSION);
    if (fileout.IsNull())
        return error("%s : Failed to open file %s", __func__, pathTmp.string());

    // Write and commit header, data
    try {
        fileout << ssPeers;
    }
    catch (std::exception &e) {
        return error("%s : Serialize or I/O error - %s", __func__, e.what());
    }
    FileCommit(fileout.Get());
    fileout.fclose();

    // replace existing peers.dat, if any, with new peers.dat.XXXX
    if (!RenameOver(pathTmp, pathAddr))
        return error("%s : Rename-into-place failed", __func__);

    return true;
}

bool CAddrDB::Read(CAddrMan& addr)
{
    // open input file, and associate with CAutoFile
    FILE *file = fopen(pathAddr.string().c_str(), "rb");
    CAutoFile filein(file, SER_DISK, CLIENT_VERSION);
    if (filein.IsNull())
    {
        if(fDebug)LogPrint("addrman","%s : Failed to open file %s", __func__, pathAddr.string().c_str());
        return false;
//        return error("%s : Failed to open file %s", __func__, pathAddr.string());
    }
    // use file size to size memory buffer
    int fileSize = boost::filesystem::file_size(pathAddr);
    int dataSize = fileSize - sizeof(uint256);
    // Don't try to resize to a negative number if file is small
    if (dataSize < 0)
        dataSize = 0;
    vector<unsigned char> vchData;
    vchData.resize(dataSize);
    uint256 hashIn;

    // read data and checksum from file
    try {
        filein.read((char *)&vchData[0], dataSize);
        filein >> hashIn;
    }
    catch (std::exception &e) {
        return error("%s : Deserialize or I/O error - %s", __func__, e.what());
    }
    filein.fclose();

    CDataStream ssPeers(vchData, SER_DISK, CLIENT_VERSION);

    // verify stored checksum matches input data
    uint256 hashTmp = Hash(ssPeers.begin(), ssPeers.end());
    if (hashIn != hashTmp)
        return error("%s : Checksum mismatch, data corrupted", __func__);

    unsigned char pchMsgTmp[4];
    try {
        // de-serialize file header (network specific magic number) and ..
        ssPeers >> FLATDATA(pchMsgTmp);

        // ... verify the network matches ours
        if (memcmp(pchMsgTmp, Params().MessageStart(), sizeof(pchMsgTmp)))
            return error("%s : Invalid network magic number", __func__);

        // de-serialize address data into one CAddrMan object
        ssPeers >> addr;
    }
    catch (std::exception &e) {
        return error("%s : Deserialize or I/O error - %s", __func__, e.what());
    }

    return true;
}

unsigned int ReceiveFloodSize() { return 1000*GetArg("-maxreceivebuffer", 5*1000); }
unsigned int SendBufferSize() { return 1000*GetArg("-maxsendbuffer", 1*100000); }

CNode::CNode(SOCKET hSocketIn, CAddress addrIn, std::string addrNameIn, bool fInboundIn) : ssSend(SER_NETWORK, INIT_PROTO_VERSION), setAddrKnown(5000)
{
    nServices = 0;
    hSocket = hSocketIn;
    nRecvVersion = INIT_PROTO_VERSION;
    nLastSend = 0;
    nLastRecv = 0;
    nSendBytes = 0;
    nRecvBytes = 0;
    nTimeConnected = GetTime();
    nTimeToDisconnect = nTimeConnected + 86400 - 3600 + GetRand(7200);
    addr = addrIn;
    addrName = addrNameIn == "" ? addr.ToStringIPPort() : addrNameIn;
    nVersion = 0;
    strSubVer = "";
    fWhitelisted = false;
    fOneShot = false;
    fClient = false; // set by version message
    fInbound = fInboundIn;
    fNetworkNode = false;
    fSuccessfullyConnected = false;
    fDisconnect = false;
    nRefCount = 0;
    nSendSize = 0;
    nSendOffset = 0;
    hashContinue = 0;
    nStartingHeight = -1;
    fGetAddr = false;
    fRelayTxes = false;
    fReadyForTxInv = false;
    setInventoryKnown.max_size(SendBufferSize() / 1000);
    pfilter = new CBloomFilter();
    nPingNonceSent = 0;
    nPingUsecStart = 0;
    nPingUsecTime = 0;
    fPingQueued = false;

/* MCHN START */    
    nMultiChainServices=0;
    fDefaultMessageStart=false;
    fVerackackReceived=false;
    fVerackackSent=false;
    fParameterSetVerified=false;    
    fSyncedOnce=false;
    fCanConnectLocal=false;
    fCanConnectRemote=false;
    fLastIgnoreIncoming=false;
    fEmptyHeaders=false;
    nLastKBPerDestinationChangeTimestamp=0;
    nMaxKBPerDestination=0;
    nTotalBuffersSize=0;
    nNextSizeCalcTimestamp=0;
    
    pEntData=NULL;
    nNextSendTime=0;    
    
    vChainPieces.clear();
    nChainBanTimestamp=0;
    
/* MCHN END */    
    
    {
        LOCK(cs_nLastNodeId);
        id = nLastNodeId++;
    }

    if (fLogIPs)
    {
        if(fDebug)LogPrint("net", "Added connection to %s peer=%d\n", addrName, id);
    }
    else
    {
        if(fDebug)LogPrint("net", "Added connection peer=%d\n", id);
    }

    // Be shy and don't send version until we hear
    if (hSocket != INVALID_SOCKET && !fInbound)
        PushVersion();

    GetNodeSignals().InitializeNode(GetId(), this);
}

CNode::~CNode()
{
    CloseSocket(hSocket);

    if (pfilter)
        delete pfilter;
    
    if(pEntData)
    {
        pEF->NET_FreeNodeData(pEntData);
        pEntData=NULL;
    }

    GetNodeSignals().FinalizeNode(GetId());
}

void CNode::AskFor(const CInv& inv)
{
//    if (mapAskFor.size() > MAPASKFOR_MAX_SZ)
    LOCK(cs_askfor);
    if (mapAskFor.size() > 2 * MAX_INV_SZ)
    {
        if(!fDisconnect)
        {
            LogPrintf("mchn: Too many inv messages from single node, disconnecting, peer=%d\n",id);        
        }
        fDisconnect=true;
        return;
    }
/* MCHN START */
    if(mc_gState->m_NodePausedState & MC_NPS_INCOMING)                          
    {
        if(inv.type != MSG_BLOCK)
        {
            return;
        }
    }
/* MCHN END */
    // We're using mapAskFor as a priority queue,
    // the key is the earliest time the request can be sent
    int64_t nRequestTime;
    limitedmap<CInv, int64_t>::const_iterator it = mapAlreadyAskedFor.find(inv);
    if (it != mapAlreadyAskedFor.end())
        nRequestTime = it->second;
    else
        nRequestTime = 0;
    if(fDebug)LogPrint("net", "askfor %s  %d (%s) peer=%d\n", inv.ToString(), nRequestTime, DateTimeStrFormat("%H:%M:%S", nRequestTime/1000000), id);

    // Make sure not to reuse time indexes to keep things in the same order
    int64_t nNow = GetTimeMicros() - 1000000;
    static int64_t nLastTime;
    ++nLastTime;
    nNow = std::max(nNow, nLastTime);
    nLastTime = nNow;
    // Each retry is 2 minutes after the last
//    nRequestTime = std::max(nRequestTime + 2 * 60 * 1000000, nNow);
    nRequestTime = std::max(nRequestTime + Params().TargetSpacing() * 500000, nNow);
    
    if (it != mapAlreadyAskedFor.end())
        mapAlreadyAskedFor.update(it, nRequestTime);
    else
        mapAlreadyAskedFor.insert(std::make_pair(inv, nRequestTime));
    mapAskFor.insert(std::make_pair(nRequestTime, inv));
}

void CNode::BeginMessage(const char* pszCommand) EXCLUSIVE_LOCK_FUNCTION(cs_vSend)
{
    ENTER_CRITICAL_SECTION(cs_vSend);
    assert(ssSend.size() == 0);
    ssSend << CMessageHeader(pszCommand, 0);
    if(fDebug)LogPrint("net", "sending: %s ", SanitizeString(pszCommand));
    if(fDebug)LogPrint("mchnminor","mchn: SEND: %s\n",SanitizeString(pszCommand));
    if(InitialNetLogTime)if(GetTime()-nTimeConnected<InitialNetLogTime)LogPrintf("mchn-inl: SEND: %s",SanitizeString(pszCommand));
}

void CNode::AbortMessage() UNLOCK_FUNCTION(cs_vSend)
{
    ssSend.clear();

    LEAVE_CRITICAL_SECTION(cs_vSend);

    if(fDebug)LogPrint("net", "(aborted)\n");
}

void CNode::EndMessage() UNLOCK_FUNCTION(cs_vSend)
{
    // The -*messagestest options are intentionally not documented in the help message,
    // since they are only used during development to debug the networking code and are
    // not intended for end-users.
    if (mapArgs.count("-dropmessagestest") && (atoi(mapArgs["-dropmessagestest"]) > 0) && (GetRand(GetArg("-dropmessagestest", 2)) == 0) )
    {
        if(fDebug)LogPrint("net", "dropmessages DROPPING SEND MESSAGE\n");
        AbortMessage();
        return;
    }
    if (mapArgs.count("-fuzzmessagestest"))
        Fuzz(GetArg("-fuzzmessagestest", 10));

    if (ssSend.size() == 0)
        return;

    // Set the size
    unsigned int nSize = ssSend.size() - CMessageHeader::HEADER_SIZE;
    memcpy((char*)&ssSend[CMessageHeader::MESSAGE_SIZE_OFFSET], &nSize, sizeof(nSize));

    // Set the checksum
    uint256 hash = Hash(ssSend.begin() + CMessageHeader::HEADER_SIZE, ssSend.end());
    unsigned int nChecksum = 0;
    memcpy(&nChecksum, &hash, sizeof(nChecksum));
    assert(ssSend.size () >= CMessageHeader::CHECKSUM_OFFSET + sizeof(nChecksum));
    memcpy((char*)&ssSend[CMessageHeader::CHECKSUM_OFFSET], &nChecksum, sizeof(nChecksum));

    if(fDebug)LogPrint("net", "(%d bytes) peer=%d\n", nSize, id);
    if(InitialNetLogTime)if(GetTime()-nTimeConnected<InitialNetLogTime)LogPrintf("(%d bytes) peer=%d\n", nSize, id);
    if(pEntData)
    {
        pEF->NET_PushMsg(pEntData,ssSend);
    }
    std::deque<CSerializeData>::iterator it = vSendMsg.insert(vSendMsg.end(), CSerializeData());
    ssSend.GetAndClear(*it);
    nSendSize += (*it).size();

    // If write queue empty, attempt "optimistic write"
    if (it == vSendMsg.begin())
        SocketSendData(this);

    LEAVE_CRITICAL_SECTION(cs_vSend);
}

/* MCHN START */

int mc_QuerySeed(boost::thread_group& threadGroup,const char *seedAddr)
{
    int err;
//    AddOneShot(seedAddr);
    StartNode(threadGroup);

    unsigned int StartTime=mc_TimeNowAsUInt();
    
    if(fDebug)LogPrint("mchn","mchn: Sending query to seed node %s\n",seedAddr);
        
    err=MC_ERR_NOERROR;
    
    while( (mc_gState->m_NetworkState != MC_NTS_SEED_READY) && (mc_gState->m_NetworkState != MC_NTS_SEED_NO_PARAMS))                       // MCHN-TODO add timeout
    {        
        __US_Sleep(1);
        if(mc_TimeNowAsUInt() - StartTime > 10)
        {
            LogPrintf("mchn: Could not establish connection with seed node - timeout\n");
            mc_gState->m_NetworkState=MC_NTS_SEED_READY;
            err=MC_ERR_CONNECTION_ERROR;
        }
    }
    
    if(err)
    {
        mc_gState->m_NetworkState=MC_NTS_NOT_READY;
    }
    
    if(fDebug)LogPrint("mchn","mchn: Query completed, waiting for seed node to disconnect\n");
    
    StartTime=mc_TimeNowAsUInt();
    bool fCheckDisconnect=true;
    while(fCheckDisconnect)
    {
        {
            {
                LOCK(cs_vNodes);
                if(vNodes.size() == 0)
                {
                    if(fDebug)LogPrint("mchn","mchn: Successfully disconnected from seed node\n");                    
                    fCheckDisconnect=false;
                }
            }
            if(fCheckDisconnect)
            {
                __US_Sleep(50);
                if(mc_TimeNowAsUInt() - StartTime > 10)
                {
                    if(fDebug)LogPrint("mchn","mchn: Could not disconnect from seed node - timeout\n");                    
                    fCheckDisconnect=false;
                    err=MC_ERR_CONNECTION_ERROR;
                }
                
            }
        }        
    }
    
    threadGroup.interrupt_all();
    threadGroup.join_all();    
    
    return err;
}

/* MCHN END */
