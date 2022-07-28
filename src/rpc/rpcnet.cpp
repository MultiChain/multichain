// Copyright (c) 2014-2016 The Bitcoin Core developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "rpc/rpcserver.h"

#include "version/clientversion.h"
#include "core/main.h"
#include "net/net.h"
#include "net/netbase.h"
#include "protocol/netprotocol.h"
#include "utils/sync.h"
#include "utils/timedata.h"
#include "utils/util.h"
#include "version/bcversion.h"
#include "storage/addrman.h"

#ifdef HAVE_GETADDRINFO_A
#include <netdb.h>
#endif

#ifndef WIN32
#if HAVE_INET_PTON
#include <arpa/inet.h>
#endif
#include <fcntl.h>
#endif


/* MCHN START */
#include "structs/base58.h"
/* MCHN END */

#include <boost/foreach.hpp>

#include "json/json_spirit_value.h"

using namespace json_spirit;
using namespace std;
void PushMultiChainRelay(CNode* pto, uint32_t msg_type,vector<CAddress>& path,vector<CAddress>& path_to_follow,vector<unsigned char>& payload);
int64_t mc_PeerStatusDelayForget();
bool paramtobool(Value param);

Value getconnectioncount(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error("Help message not found\n");

    LOCK(cs_vNodes);
    return (int)vNodes.size();
}

Value ping(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error("Help message not found\n");

    // Request that each node send a ping during next message processing pass
    LOCK(cs_vNodes);
    BOOST_FOREACH(CNode* pNode, vNodes) {
        pNode->fPingQueued = true;
    }

    return Value::null;
}

static void CopyNodeStats(std::vector<CNodeStats>& vstats)
{
    vstats.clear();

    LOCK(cs_vNodes);
    vstats.reserve(vNodes.size());
    BOOST_FOREACH(CNode* pnode, vNodes) {
        CNodeStats stats;
        pnode->copyStats(stats);
        vstats.push_back(stats);
    }
}

Value getpeerinfo(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error("Help message not found\n");

    vector<CNodeStats> vstats;
    CopyNodeStats(vstats);

    Array ret;

    BOOST_FOREACH(const CNodeStats& stats, vstats) {
/* MCHN START */        
        if(stats.fSuccessfullyConnected)
        {
/* MCHN END */        
        Object obj;
        CNodeStateStats statestats;
        bool fStateStats = GetNodeStateStats(stats.nodeid, statestats);
        obj.push_back(Pair("id", stats.nodeid));
        obj.push_back(Pair("addr", stats.addrName));
        if (!(stats.addrLocal.empty()))
            obj.push_back(Pair("addrlocal", stats.addrLocal));
        obj.push_back(Pair("services", strprintf("%016x", stats.nServices)));
        obj.push_back(Pair("lastsend", stats.nLastSend));
        obj.push_back(Pair("lastrecv", stats.nLastRecv));
        obj.push_back(Pair("bytessent", stats.nSendBytes));
        obj.push_back(Pair("bytesrecv", stats.nRecvBytes));
        obj.push_back(Pair("conntime", stats.nTimeConnected));
        obj.push_back(Pair("pingtime", stats.dPingTime));
        if (stats.dPingWait > 0.0)
            obj.push_back(Pair("pingwait", stats.dPingWait));
        obj.push_back(Pair("version", stats.nVersion));
        // Use the sanitized form of subver here, to avoid tricksy remote peers from
        // corrupting or modifiying the JSON output by putting special characters in
        // their ver message.
        obj.push_back(Pair("subver", stats.cleanSubVer));
/* MCHN START */        
        if(mc_gState->m_NetworkParams->IsProtocolMultichain())
        {
            if(MCP_ANYONE_CAN_CONNECT)
            {
                Value null_value;
                obj.push_back(Pair("handshakelocal", null_value));                
                obj.push_back(Pair("handshake", null_value));                
            }            
            else
            {
                obj.push_back(Pair("handshakelocal", CBitcoinAddress(stats.kAddrLocal).ToString()));                
                obj.push_back(Pair("handshake", CBitcoinAddress(stats.kAddrRemote).ToString()));                
            }
        }
/* MCHN END */        
        obj.push_back(Pair("inbound", stats.fInbound));
        obj.push_back(Pair("encrypted", stats.fEncrypted));
        obj.push_back(Pair("startingheight", stats.nStartingHeight));
        if (fStateStats) {
            obj.push_back(Pair("banscore", statestats.nMisbehavior));
            obj.push_back(Pair("synced_headers", statestats.nSyncHeight));
            obj.push_back(Pair("synced_blocks", statestats.nCommonHeight));
            Array heights;
            BOOST_FOREACH(int height, statestats.vHeightInFlight) {
                heights.push_back(height);
            }
            obj.push_back(Pair("inflight", heights));
        }
        obj.push_back(Pair("whitelisted", stats.fWhitelisted));
       
        

        ret.push_back(obj);
/* MCHN START */        
        }
/* MCHN END */        
    }

    return ret;
}

bool PeersCompareByTime(Value a,Value b)
{ 
    int64_t time_a=0;
    int64_t time_b=0;

    BOOST_FOREACH(const Pair& p, a.get_obj()) 
    {
        if(p.name_ == "lastsuccess")
        {
            time_a=p.value_.get_int64();
        }
    }

    BOOST_FOREACH(const Pair& p, b.get_obj()) 
    {
        if(p.name_ == "lastsuccess")
        {
            time_b=p.value_.get_int64();
        }
    }

    return (time_a >= time_b);
}


Value liststorednodes(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error("Help message not found\n");

    bool take_ignored=false;
    if (params.size() == 1)
        take_ignored = paramtobool(params[0]);
    
    
    vector<CNodeStats> vstats;
    CopyNodeStats(vstats);

    map<CService, bool> mConnected;

    {
        LOCK(cs_vNodes);
        BOOST_FOREACH(CNode* pnode, vNodes) {
            if(pnode->fInbound)
            {
                mConnected.insert(make_pair(pnode->addrFromVersion,true));
            }
            else
            {
                mConnected.insert(make_pair(pnode->addr,false));                
            }
        }
    }
    
    vector <CMCAddrInfo> empty;
    map<CService, vector<CMCAddrInfo> > mNetAddresses;
    CMCAddrInfo *info;
    
    addrman.GetMCAddrMan()->Reset();
    while((info=addrman.GetMCAddrMan()->Next()))
    {
        CService naddr=info->GetNetAddress();
        if(!naddr.IsZero())
        {        
            if (naddr.IsValid() && (!IsLocal(naddr) || (naddr.GetPort() != GetListenPort())))
            {
                map<CService, vector<CMCAddrInfo> >::iterator nit = mNetAddresses.find(naddr);
                if(nit == mNetAddresses.end())
                {
                    mNetAddresses.insert(make_pair(naddr,empty));
                    nit = mNetAddresses.find(naddr);
                }
                nit->second.push_back(info);
            }            
        }
    }    
    
    Array netaddresses;
    BOOST_FOREACH(PAIRTYPE(CService, vector<CMCAddrInfo>) naddr, mNetAddresses)
    {
        Object entry;
        int64_t lastattempt=0;
        int netattempts=0;
        uint32_t flags=0;
        Array handshakes;
        BOOST_FOREACH(CMCAddrInfo info, naddr.second)
        {
            Object handshake;
            int64_t lastsuccess,lasttry;
            int attempts;
            attempts=info.GetLastTryInfo(&lastsuccess,&lasttry,NULL);
            if(info.GetMCAddress() != 0)
            {
                handshake.push_back(Pair("address",CBitcoinAddress((CKeyID)info.GetMCAddress()).ToString()));
                handshake.push_back(Pair("lastsuccess",lastsuccess));
                handshakes.push_back(handshake);
            }
            else
            {
                lastattempt=lasttry;
                netattempts=attempts;
                flags=info.GetFlags();
            }
        }        
        
        sort(handshakes.begin(), handshakes.end(), PeersCompareByTime);

        
        entry.push_back(Pair("addr",naddr.first.ToStringIPPort()));                     
        entry.push_back(Pair("handshakes",handshakes));                     
        if(OutConnectionsAlgoritm == 1)
        {
            if(lastattempt != 0)
            {
                entry.push_back(Pair("lastfailure",lastattempt));
            }
            else
            {
                entry.push_back(Pair("lastfailure",Value::null));                                
            }
            entry.push_back(Pair("failcount",netattempts));                     
        }
        
        switch(flags & MC_AMF_SOURCE_MASK)
        {
            case MC_AMF_SOURCE_ADDED:
                entry.push_back(Pair("source","stored"));                                                 
                break;
            case MC_AMF_SOURCE_SEED:
                entry.push_back(Pair("source","seed"));                                                 
                break;
            default:
                entry.push_back(Pair("source","peers"));                                                 
                break;
        }
        
        map<CService, bool>::const_iterator pit=mConnected.find(naddr.first);
        if(pit != mConnected.end())
        {
            if(pit->second)
            {
                entry.push_back(Pair("status","inbound"));                                                                 
            }
            else
            {
                entry.push_back(Pair("status","outbound"));                                                                                 
            }
        }
        else
        {
            if(flags & MC_AMF_IGNORED)
            {
                entry.push_back(Pair("status","ignored"));                                                                 
            }
            else
            {
                entry.push_back(Pair("status","tryconnect"));                                                                                 
            }
        }
        
        
        bool take_it=true;
        if(!take_ignored)
        {
            if(flags & MC_AMF_IGNORED)
            {
                if(lastattempt < GetAdjustedTime() - mc_PeerStatusDelayForget())
                {
                    take_it=false;
                }
            }
        }            

        
        
        if(take_it)
        {        
            netaddresses.push_back(entry);
        }        
    }
    
    
    return netaddresses;
}

Value addnode(const Array& params, bool fHelp)
{
    string strCommand;
    if (params.size() == 2)
        strCommand = params[1].get_str();
    if (fHelp || params.size() != 2 ||
        (strCommand != "onetry" && strCommand != "add" && strCommand != "remove"))
        throw runtime_error("Help message not found\n");

    string strNode = params[0].get_str();

    int port = Params().GetDefaultPort();
    std::string hostname = "";
    SplitHostPort(strNode, port, hostname);

    bool is_numeric=false;
    struct in_addr ipv4_addr;
#ifdef HAVE_GETADDRINFO_A
#ifdef HAVE_INET_PTON    
    if (inet_pton(AF_INET, hostname.c_str(), &ipv4_addr) > 0) {
        is_numeric=true;
    }

    struct in6_addr ipv6_addr;
    if (inet_pton(AF_INET6, hostname.c_str(), &ipv6_addr) > 0) {
        is_numeric=true;
    }    
#else
    ipv4_addr.s_addr = inet_addr(hostname.c_str());
    if (ipv4_addr.s_addr != INADDR_NONE) {
        is_numeric=true;
    }
#endif
#else
    ipv4_addr.s_addr = inet_addr(hostname.c_str());
    if (ipv4_addr.s_addr != INADDR_NONE) {
        is_numeric=true;
    }
#endif

    if(!is_numeric)
    {
        throw JSONRPCError(RPC_NOT_ALLOWED, "Invalid node, only numeric valid IPv4 addresses are allowed");        
    }
/*    
    struct in_addr ipv4_addr;
    ipv4_addr.s_addr = inet_addr(hostname.c_str());
    if (ipv4_addr.s_addr == INADDR_NONE) 
    {
        throw JSONRPCError(RPC_NOT_ALLOWED, "Invalid node, only numeric IPv4 addresses are allowed");
    }
*/    
    
    if (strCommand == "onetry")
    {
        CAddress addr;
        OpenNetworkConnection(addr, NULL, strNode.c_str());
        return Value::null;
    }
    
    LOCK(cs_vAddedNodes);
    vector<string>::iterator it = vAddedNodes.begin();
    for(; it != vAddedNodes.end(); it++)
        if (strNode == *it)
            break;

    if (strCommand == "add")
    {
        if (it != vAddedNodes.end())
            throw JSONRPCError(RPC_CLIENT_NODE_ALREADY_ADDED, "Error: Node already added");
        vAddedNodes.push_back(strNode);
    }
    else if(strCommand == "remove")
    {
        if (it == vAddedNodes.end())
            throw JSONRPCError(RPC_CLIENT_NODE_NOT_ADDED, "Error: Node has not been added.");
        vAddedNodes.erase(it);
        if(GetBoolArg("-addnodeonly",false))
        {
            LOCK(cs_vNodes);
            CAddress addrNode=CAddress(CService(strNode.c_str(),Params().GetDefaultPort(),0));
            BOOST_FOREACH(CNode* pnode, vNodes)
            {
                if (pnode->addr == addrNode)
                {
                    pnode->fDisconnect=true;
                }
            }
        }
        
    }

    return Value::null;
}

Value storenode(const Array& params, bool fHelp)
{
    string strCommand="tryconnect";
    if (params.size() == 2)
        strCommand = params[1].get_str();
    
    if (fHelp || params.size() < 1 || params.size() > 2 ||
        (strCommand != "tryconnect" && strCommand != "ignore"))
        throw runtime_error("Help message not found\n");
    

    string strNode = params[0].get_str();

    int port = Params().GetDefaultPort();
    std::string hostname = "";
    SplitHostPort(strNode, port, hostname);

    bool is_numeric=false;
    struct in_addr ipv4_addr;
#ifdef HAVE_GETADDRINFO_A
#ifdef HAVE_INET_PTON    
    if (inet_pton(AF_INET, hostname.c_str(), &ipv4_addr) > 0) {
        is_numeric=true;
    }

    struct in6_addr ipv6_addr;
    if (inet_pton(AF_INET6, hostname.c_str(), &ipv6_addr) > 0) {
        is_numeric=true;
    }    
#else
    ipv4_addr.s_addr = inet_addr(hostname.c_str());
    if (ipv4_addr.s_addr != INADDR_NONE) {
        is_numeric=true;
    }
#endif
#else
    ipv4_addr.s_addr = inet_addr(hostname.c_str());
    if (ipv4_addr.s_addr != INADDR_NONE) {
        is_numeric=true;
    }
#endif

    if(!is_numeric)
    {
        throw JSONRPCError(RPC_NOT_ALLOWED, "Invalid node, only numeric valid IPv4 addresses are allowed");        
    }
/*    
    struct in_addr ipv4_addr;
    ipv4_addr.s_addr = inet_addr(hostname.c_str());
    if (ipv4_addr.s_addr == INADDR_NONE) 
    {
        throw JSONRPCError(RPC_NOT_ALLOWED, "Invalid node, only numeric IPv4 addresses are allowed");
    }
*/    
    
    if (strCommand == "tryconnect")
    {
        CAddress addr=CAddress(CService(CNetAddr(hostname),port));
        if(!addr.IsValid())
        {
            throw JSONRPCError(RPC_NOT_ALLOWED, "Invalid address");                    
        }
        CMCAddrInfo *mcaddrinfo=addrman.GetMCAddrMan()->Find(addr,0);
        if(mcaddrinfo)
        {
            uint32_t flags=mcaddrinfo->GetFlags();
            if(flags & MC_AMF_IGNORED)
            {
                mcaddrinfo->SetFlag(MC_AMF_IGNORED,0);
                mcaddrinfo->SetFlag(MC_AMF_SOURCE_ADDED,1);
                mcaddrinfo->ResetLastTry(true);
                if(fDebug)LogPrint("addrman", "Recall forgotten address %s \n", addr.ToString());
            }
            else
            {
                throw JSONRPCError(RPC_NOT_ALLOWED, "Node already is used for outbound connections");      
//                if(fDebug)LogPrint("addrman", "Remember found address %s, ignored\n", addr.ToString());                
            }
        }
        else
        {
            addrman.Add(addr, addr);
            addrman.Good(addr);
            mcaddrinfo=addrman.GetMCAddrMan()->Find(addr,0);
            if(mcaddrinfo == NULL)
            {
                throw JSONRPCError(RPC_INTERNAL_ERROR, "Internal error when adding new address");                    
            }
            mcaddrinfo->SetFlag(MC_AMF_SOURCE_ADDED,1);
            
            if(fDebug)LogPrint("addrman", "Remember new address %s\n", addr.ToString());                
        }        
        return Value::null;
    }
    
    if (strCommand == "ignore")
    {
        CAddress addr=CAddress(CService(CNetAddr(hostname),port));
        if(!addr.IsValid())
        {
            throw JSONRPCError(RPC_NOT_ALLOWED, "Invalid address");                    
        }
        CMCAddrInfo *mcaddrinfo=addrman.GetMCAddrMan()->Find(addr,0);
        if(mcaddrinfo)
        {
            uint32_t flags=mcaddrinfo->GetFlags();
            if(flags & MC_AMF_IGNORED)
            {
                throw JSONRPCError(RPC_NOT_ALLOWED, "Node already ignored");      
//                if(fDebug)LogPrint("addrman", "Ignore forgotten address %s, ignored\n", addr.ToString());
            }
            else
            {
                mcaddrinfo->SetFlag(MC_AMF_IGNORED,1);
                mcaddrinfo->ResetLastTry(false);
                if(fDebug)LogPrint("addrman", "Ignore address %s\n", addr.ToString());                
            }
        }
        else
        {
            throw JSONRPCError(RPC_NOT_ALLOWED, "Node not found");      
//            if(fDebug)LogPrint("addrman", "Ignore unknown address %s, ignored\n", addr.ToString());                
        }        
        return Value::null;
    }
    
    return Value::null;
}

Value getaddednodeinfo(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error("Help message not found\n");

    bool fDns = params[0].get_bool();

    list<string> laddedNodes(0);
    if (params.size() == 1)
    {
        LOCK(cs_vAddedNodes);
        BOOST_FOREACH(string& strAddNode, vAddedNodes)
            laddedNodes.push_back(strAddNode);
    }
    else
    {
        string strNode = params[1].get_str();
        LOCK(cs_vAddedNodes);
        BOOST_FOREACH(string& strAddNode, vAddedNodes)
            if (strAddNode == strNode)
            {
                laddedNodes.push_back(strAddNode);
                break;
            }
        if (laddedNodes.size() == 0)
            throw JSONRPCError(RPC_CLIENT_NODE_NOT_ADDED, "Error: Node has not been added.");
    }

    Array ret;
    if (!fDns)
    {
        BOOST_FOREACH(string& strAddNode, laddedNodes)
        {
            Object obj;
            obj.push_back(Pair("addednode", strAddNode));
            ret.push_back(obj);
        }
        return ret;
    }

    list<pair<string, vector<CService> > > laddedAddreses(0);
    BOOST_FOREACH(string& strAddNode, laddedNodes)
    {
        vector<CService> vservNode(0);
        if(Lookup(strAddNode.c_str(), vservNode, Params().GetDefaultPort(), fNameLookup, 0))
            laddedAddreses.push_back(make_pair(strAddNode, vservNode));
        else
        {
            Object obj;
            obj.push_back(Pair("addednode", strAddNode));
            obj.push_back(Pair("connected", false));
            Array addresses;
            obj.push_back(Pair("addresses", addresses));
        }
    }

    LOCK(cs_vNodes);
    for (list<pair<string, vector<CService> > >::iterator it = laddedAddreses.begin(); it != laddedAddreses.end(); it++)
    {
        Object obj;
        obj.push_back(Pair("addednode", it->first));

        Array addresses;
        bool fConnected = false;
        BOOST_FOREACH(CService& addrNode, it->second)
        {
            bool fFound = false;
            Object node;
            node.push_back(Pair("address", addrNode.ToString()));
            BOOST_FOREACH(CNode* pnode, vNodes)
                if (pnode->addr == addrNode)
                {
                    fFound = true;
                    fConnected = true;
                    node.push_back(Pair("connected", pnode->fInbound ? "inbound" : "outbound"));
                    
                    vector<CAddress>path;
                    vector<CAddress>path_to_follow;
                    vector<unsigned char> payload;
                    
//                    PushMultiChainRelay(pnode, MC_RMT_GLOBAL_PING,path,path_to_follow,payload);

                    break;
                }
            if (!fFound)
                node.push_back(Pair("connected", "false"));
            addresses.push_back(node);
        }
        obj.push_back(Pair("connected", fConnected));
        obj.push_back(Pair("addresses", addresses));
        ret.push_back(obj);
    }

    return ret;
}

Value getnettotals(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 0)
        throw runtime_error("Help message not found\n");

    Object obj;
    obj.push_back(Pair("totalbytesrecv", CNode::GetTotalBytesRecv()));
    obj.push_back(Pair("totalbytessent", CNode::GetTotalBytesSent()));
    obj.push_back(Pair("timemillis", GetTimeMillis()));
    return obj;
}

static Array GetNetworksInfo()
{
    Array networks;
    for(int n=0; n<NET_MAX; ++n)
    {
        enum Network network = static_cast<enum Network>(n);
        if(network == NET_UNROUTABLE)
            continue;
        proxyType proxy;
        Object obj;
        GetProxy(network, proxy);
        obj.push_back(Pair("name", GetNetworkName(network)));
        obj.push_back(Pair("limited", IsLimited(network)));
        obj.push_back(Pair("reachable", IsReachable(network)));
        obj.push_back(Pair("proxy", proxy.IsValid() ? proxy.ToStringIPPort() : string()));
        networks.push_back(obj);
    }
    return networks;
}

Value getnetworkinfo(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error("Help message not found\n");

    Object obj;
    obj.push_back(Pair("version",       CLIENT_VERSION));
/* MCHN START */    
    if(mc_gState->m_NetworkParams->IsProtocolMultichain())
    {
        obj.push_back(Pair("subversion",
            FormatSubVersion("MultiChain", mc_gState->GetProtocolVersion(), std::vector<string>())));
    }
    else
    {
        obj.push_back(Pair("subversion",
            FormatSubVersion(CLIENT_NAME, CLIENT_VERSION, std::vector<string>())));
    }
/* MCHN END */        
    obj.push_back(Pair("protocolversion",PROTOCOL_VERSION));
    obj.push_back(Pair("localservices",       strprintf("%016x", nLocalServices)));
    obj.push_back(Pair("timeoffset",    GetTimeOffset()));
    obj.push_back(Pair("connections",   (int)vNodes.size()));
    obj.push_back(Pair("networks",      GetNetworksInfo()));
    obj.push_back(Pair("relayfee",      ValueFromAmount(::minRelayTxFee.GetFeePerK())));
    Array localAddresses;
    {
        LOCK(cs_mapLocalHost);
        BOOST_FOREACH(const PAIRTYPE(CNetAddr, LocalServiceInfo) &item, mapLocalHost)
        {
            Object rec;
            rec.push_back(Pair("address", item.first.ToString()));
            rec.push_back(Pair("port", item.second.nPort));
            rec.push_back(Pair("score", item.second.nScore));
            localAddresses.push_back(rec);
        }
    }
    obj.push_back(Pair("localaddresses", localAddresses));
    return obj;
}
