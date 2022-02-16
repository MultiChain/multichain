// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2014-2016 The Bitcoin Core developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "structs/base58.h"
#include "version/clientversion.h"
#include "core/init.h"
#include "core/main.h"
#include "net/net.h"
#include "net/netbase.h"
#include "rpc/rpcserver.h"
#include "utils/timedata.h"
#include "utils/util.h"
#ifdef ENABLE_WALLET
#include "wallet/wallet.h"
#include "wallet/walletdb.h"
#endif
#include "community/community.h"
#include "json/json_spirit_ubjson.h"

#include <stdint.h>

/* MCHN START */
#include "multichain/multichain.h"
#include "wallet/wallettxs.h"
std::string BurnAddress(const std::vector<unsigned char>& vchVersion);
std::string SetBannedTxs(std::string txlist);
std::string SetLockedBlock(std::string hash);
int mc_StoreSetRuntimeParam(std::string param_name);
double mc_GetChainRewards(int chain_height,int64_t *raw_value);
Array listconnect_operation(const Array& params);
Array listminers_operation(const Array& params);
Array listadmins_operation(const Array& params);
Array JSONRPCExecInternalBatch(const Array& vReq);

/* MCHN END */

#include <boost/assign/list_of.hpp>
#include "json/json_spirit_utils.h"
#include "json/json_spirit_value.h"


using namespace boost;
using namespace boost::assign;
using namespace json_spirit;
using namespace std;

bool CBitcoinAddressFromTxEntity(CBitcoinAddress &address,mc_TxEntity *lpEntity);
Object AddressEntry(CBitcoinAddress& address,uint32_t verbose);
int paramtoint(Value param,bool check_for_min,int min_value,string error_message);

/**
 * @note Do not add or change anything in the information returned by this
 * method. `getinfo` exists for backwards-compatibility only. It combines
 * information from wildly different sources in the program, which is a mess,
 * and is thus planned to be deprecated eventually.
 *
 * Based on the source of the information, new information should be added to:
 * - `getblockchaininfo`,
 * - `getnetworkinfo` or
 * - `getwalletinfo`
 *
 * Or alternatively, create a specific query method for the information.
 **/
Value getinfo(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)  
        throw runtime_error("Help message not found\n");

    proxyType proxy;
    GetProxy(NET_IPV4, proxy);

    Object obj;
/* MCHN START */    
//    obj.push_back(Pair("version", CLIENT_VERSION));
//    obj.push_back(Pair("protocolversion", PROTOCOL_VERSION));
    
    obj.push_back(Pair("version", mc_BuildDescription(mc_gState->GetNumericVersion())));
    obj.push_back(Pair("nodeversion", mc_gState->GetNumericVersion()));
    obj.push_back(Pair("edition", pEF->ENT_Edition()));
    obj.push_back(Pair("protocolversion", mc_gState->m_NetworkParams->ProtocolVersion()));
    obj.push_back(Pair("chainname", string(mc_gState->m_NetworkParams->Name())));
    obj.push_back(Pair("description", string((char*)mc_gState->m_NetworkParams->GetParam("chaindescription",NULL))));
    obj.push_back(Pair("protocol", string((char*)mc_gState->m_NetworkParams->GetParam("chainprotocol",NULL))));    
    obj.push_back(Pair("port", GetListenPort()));    
    obj.push_back(Pair("setupblocks", mc_gState->m_NetworkParams->GetInt64Param("setupfirstblocks")));    
    
    obj.push_back(Pair("nodeaddress", MultichainServerAddress(true) + strprintf(":%d",GetListenPort())));       

    obj.push_back(Pair("burnaddress", BurnAddress(Params().Base58Prefix(CChainParams::PUBKEY_ADDRESS))));                
    obj.push_back(Pair("incomingpaused", (mc_gState->m_NodePausedState & MC_NPS_INCOMING) ? true : false));                
    obj.push_back(Pair("miningpaused", (mc_gState->m_NodePausedState & MC_NPS_MINING) ? true : false));                
    obj.push_back(Pair("offchainpaused", (mc_gState->m_NodePausedState & MC_NPS_OFFCHAIN) ? true : false));                

/* MCHN END */    
#ifdef ENABLE_WALLET
    if (pwalletMain) {
        obj.push_back(Pair("walletversion", pwalletMain->GetVersion()));
        obj.push_back(Pair("balance",       ValueFromAmount(pwalletMain->GetBalance())));
        obj.push_back(Pair("walletdbversion", mc_gState->GetWalletDBVersion()));                
    }
#endif
/* MCHN START */    
    obj.push_back(Pair("reindex",       fReindex));    
/* MCHN END */    
    obj.push_back(Pair("blocks",        (int)chainActive.Height()));

    double chain_balance=0.;
    int64_t total_value=0;
    chain_balance=mc_GetChainRewards((int)chainActive.Height(),&total_value);
    obj.push_back(Pair("chainrewards",        chain_balance));
    
    mc_EntityDetails entitylist_entity;
    int32_t entity_count; 
    if(mc_gState->m_Assets->FindEntityList(&entitylist_entity,MC_ENT_TYPE_ASSET))
    {
        mc_gState->m_Assets->GetTotalQuantity(&entitylist_entity,&entity_count);
        obj.push_back(Pair("assets",        (int)entity_count));        
    }
    if(mc_gState->m_Assets->FindEntityList(&entitylist_entity,MC_ENT_TYPE_STREAM))
    {
        mc_gState->m_Assets->GetTotalQuantity(&entitylist_entity,&entity_count);
        obj.push_back(Pair("streams",       (int)entity_count));        
    }

    if(mc_gState->m_WalletMode & MC_WMD_EXPLORER_MASK)
    {
        mc_TxEntity entity;

        entity.Zero();
        entity.m_EntityType=MC_TET_EXP_TX_PUBLISHER;                
        entity.m_EntityType |= MC_TET_CHAINPOS;
        entity_count=pwalletTxsMain->GetListSize(&entity,NULL);

        obj.push_back(Pair("addresses",       (int)entity_count));        

        entity.Zero();
        entity.m_EntityType=MC_TET_EXP_TX;                
        entity.m_EntityType |= MC_TET_CHAINPOS;
        entity_count=pwalletTxsMain->GetListSize(&entity,NULL);

        obj.push_back(Pair("transactions",       (int)entity_count));        

        {
            LOCK(cs_vNodes);
            obj.push_back(Pair("peers",       (int)vNodes.size()));        
        }        
    }
    
    
    
    obj.push_back(Pair("timeoffset",    GetTimeOffset()));
    obj.push_back(Pair("connections",   (int)vNodes.size()));
    obj.push_back(Pair("proxy",         (proxy.IsValid() ? proxy.ToStringIPPort() : string())));
    obj.push_back(Pair("difficulty",    (double)GetDifficulty()));
    obj.push_back(Pair("testnet",       Params().TestnetToBeDeprecatedFieldRPC()));
#ifdef ENABLE_WALLET
    if (pwalletMain) {
        obj.push_back(Pair("keypoololdest", pwalletMain->GetOldestKeyPoolTime()));
        obj.push_back(Pair("keypoolsize",   (int)pwalletMain->GetKeyPoolSize()));
    }
    if (pwalletMain && pwalletMain->IsCrypted())
        obj.push_back(Pair("unlocked_until", nWalletUnlockTime));
    obj.push_back(Pair("paytxfee",      ValueFromAmount(payTxFee.GetFeePerK())));
#endif
    
/* MCHN START */    
    ::minRelayTxFee = CFeeRate(MIN_RELAY_TX_FEE);    
/* MCHN END */    
    
    obj.push_back(Pair("relayfee",      ValueFromAmount(::minRelayTxFee.GetFeePerK())));
    obj.push_back(Pair("errors",        GetWarnings("statusbar")));
    return obj;
}

Value getchaintotals(const json_spirit::Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error("Help message not found\n");

    Object obj;
    mc_TxEntityStat entStat;
    int32_t entity_count;
     
    pwalletTxsMain->WRPReadLock();
    
    obj.push_back(Pair("blocks",       (int)chainActive.Height()));        

    double chain_balance=0.;
    int64_t total_value=0;
    chain_balance=mc_GetChainRewards((int)chainActive.Height(),&total_value);
    obj.push_back(Pair("rewards",        chain_balance));
    

    if(mc_gState->m_WalletMode & MC_WMD_EXPLORER_MASK)
    {
        mc_TxEntity entity;

        entity.Zero();
        entity.m_EntityType=MC_TET_EXP_TX_PUBLISHER;                
        entity.m_EntityType |= MC_TET_CHAINPOS;
        entity_count=pwalletTxsMain->WRPGetListSize(&entity,NULL);

        obj.push_back(Pair("addresses",       (int)entity_count));        

        entity.Zero();
        entity.m_EntityType=MC_TET_EXP_TX;                
        entity.m_EntityType |= MC_TET_CHAINPOS;
        entity_count=pwalletTxsMain->WRPGetListSize(&entity,NULL);

        obj.push_back(Pair("transactions",       (int)entity_count));        
    }
    else
    {
        obj.push_back(Pair("addresses",       Value::null));        
        obj.push_back(Pair("transactions",       Value::null));                
    }
    
    entStat.Zero();
    entStat.m_Entity.m_EntityType=MC_TET_ENTITY_KEY | MC_TET_CHAINPOS;
    
    if(!pwalletTxsMain->WRPFindEntity(&entStat))
    {
        pwalletTxsMain->WRPReadUnLock();
        
        mc_EntityDetails entitylist_entity;
        LOCK(cs_main);
        entity_count=0;
        if(mc_gState->m_Assets->FindEntityList(&entitylist_entity,MC_ENT_TYPE_ASSET))
        {
            mc_gState->m_Assets->GetTotalQuantity(&entitylist_entity,&entity_count);
        }
        obj.push_back(Pair("assets",        (int)entity_count));        
        entity_count=0;
        if(mc_gState->m_Assets->FindEntityList(&entitylist_entity,MC_ENT_TYPE_STREAM))
        {
            mc_gState->m_Assets->GetTotalQuantity(&entitylist_entity,&entity_count);
        }
        obj.push_back(Pair("streams",       (int)entity_count));        
        entity_count=0;
        if(mc_gState->m_Assets->FindEntityList(&entitylist_entity,MC_ENT_TYPE_FILTER))
        {
            mc_gState->m_Assets->GetTotalQuantity(&entitylist_entity,&entity_count);
        }
        obj.push_back(Pair("filters",       (int)entity_count));        
        entity_count=0;
        if(mc_gState->m_Assets->FindEntityList(&entitylist_entity,MC_ENT_TYPE_VARIABLE))
        {
            mc_gState->m_Assets->GetTotalQuantity(&entitylist_entity,&entity_count);
        }
        obj.push_back(Pair("variables",       (int)entity_count));        
        entity_count=0;
        if(mc_gState->m_Assets->FindEntityList(&entitylist_entity,MC_ENT_TYPE_LIBRARY))
        {
            mc_gState->m_Assets->GetTotalQuantity(&entitylist_entity,&entity_count);
        }
        obj.push_back(Pair("libraries",       (int)entity_count));        
        entity_count=0;
        if(mc_gState->m_Assets->FindEntityList(&entitylist_entity,MC_ENT_TYPE_UPGRADE))
        {
            mc_gState->m_Assets->GetTotalQuantity(&entitylist_entity,&entity_count);
        }
        obj.push_back(Pair("upgrades",       (int)entity_count));        
        entity_count=0;
        if(mc_gState->m_Assets->FindEntityList(&entitylist_entity,MC_ENT_TYPE_LICENSE_TOKEN))
        {
            mc_gState->m_Assets->GetTotalQuantity(&entitylist_entity,&entity_count);
        }
        obj.push_back(Pair("licenses",       (int)entity_count));        
    }
    else
    {
        mc_TxEntity entity;

        uint32_t entity_key;


        entity.Zero();
        entity.m_EntityType=entStat.m_Entity.m_EntityType | MC_TET_SUBKEY;    
        
        entity_key=MC_ENT_TYPE_ASSET;
        memcpy(entity.m_EntityID,&entity_key,sizeof(uint32_t));
        obj.push_back(Pair("assets",(int)pwalletTxsMain->WRPGetListSize(&entity,entStat.m_Generation,NULL)));
        
        entity_key=MC_ENT_TYPE_STREAM;
        memcpy(entity.m_EntityID,&entity_key,sizeof(uint32_t));
        obj.push_back(Pair("streams",(int)pwalletTxsMain->WRPGetListSize(&entity,entStat.m_Generation,NULL)));
        
        entity_key=MC_ENT_TYPE_FILTER;
        memcpy(entity.m_EntityID,&entity_key,sizeof(uint32_t));
        obj.push_back(Pair("filters",(int)pwalletTxsMain->WRPGetListSize(&entity,entStat.m_Generation,NULL)));
        
        entity_key=MC_ENT_TYPE_VARIABLE;
        memcpy(entity.m_EntityID,&entity_key,sizeof(uint32_t));
        obj.push_back(Pair("variables",(int)pwalletTxsMain->WRPGetListSize(&entity,entStat.m_Generation,NULL)));
        
        entity_key=MC_ENT_TYPE_LIBRARY;
        memcpy(entity.m_EntityID,&entity_key,sizeof(uint32_t));
        obj.push_back(Pair("libraries",(int)pwalletTxsMain->WRPGetListSize(&entity,entStat.m_Generation,NULL)));
        
        entity_key=MC_ENT_TYPE_UPGRADE;
        memcpy(entity.m_EntityID,&entity_key,sizeof(uint32_t));
        obj.push_back(Pair("upgrades",(int)pwalletTxsMain->WRPGetListSize(&entity,entStat.m_Generation,NULL)));
        
        entity_key=MC_ENT_TYPE_LICENSE_TOKEN;
        memcpy(entity.m_EntityID,&entity_key,sizeof(uint32_t));
        obj.push_back(Pair("licenses",(int)pwalletTxsMain->WRPGetListSize(&entity,entStat.m_Generation,NULL)));
        
        pwalletTxsMain->WRPReadUnLock();
    }
    
    
    return obj;    
}

Value getinitstatus(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)  
        throw runtime_error("Help message not found\n");

    Object obj;
    
    obj.push_back(Pair("version", mc_BuildDescription(mc_gState->GetNumericVersion())));
    obj.push_back(Pair("nodeversion", mc_gState->GetNumericVersion()));

    LOCK(cs_NodeStatus);
    
    if(pNodeStatus)
    {
        obj.push_back(Pair("initialized", false));        
        obj.push_back(Pair("connecttime", (int64_t)(mc_TimeNowAsUInt()-pNodeStatus->tStartConnectTime)));
        obj.push_back(Pair("connectaddress", pNodeStatus->sSeedIP));
        obj.push_back(Pair("connectport", pNodeStatus->nSeedPort));
        obj.push_back(Pair("handshakelocal", pNodeStatus->sAddress));
        obj.push_back(Pair("lasterror", pNodeStatus->sLastError));
    }
    else
    {
        obj.push_back(Pair("initialized", (mc_gState->m_NetworkParams->m_Status == MC_PRM_STATUS_VALID) ));        
    }
    
    return obj;
    
}

Value gethealthcheck(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)  
        throw runtime_error("Help message not found\n");

    pEF->ENT_RPCVerifyEdition("gethealthcheck API");
    
    return pEF->HCH_RPCHealthCheck(params);
}

#ifdef ENABLE_WALLET
class DescribeAddressVisitor : public boost::static_visitor<Object>
{
private:
    isminetype mine;

public:
    DescribeAddressVisitor(isminetype mineIn) : mine(mineIn) {}

    Object operator()(const CNoDestination &dest) const { return Object(); }

    Object operator()(const CKeyID &keyID) const {
        Object obj;
        CPubKey vchPubKey;
        obj.push_back(Pair("isscript", false));
        if (mine == ISMINE_SPENDABLE) {
            pwalletMain->GetPubKey(keyID, vchPubKey);
            obj.push_back(Pair("pubkey", HexStr(vchPubKey)));
            obj.push_back(Pair("iscompressed", vchPubKey.IsCompressed()));
        }
        return obj;
    }

    Object operator()(const CScriptID &scriptID) const {
        Object obj;
        obj.push_back(Pair("isscript", true));
        if (mine != ISMINE_NO) {
            CScript subscript;
            pwalletMain->GetCScript(scriptID, subscript);
            std::vector<CTxDestination> addresses;
            txnouttype whichType;
            int nRequired;
            ExtractDestinations(subscript, whichType, addresses, nRequired);
            obj.push_back(Pair("script", GetTxnOutputType(whichType)));
            obj.push_back(Pair("hex", HexStr(subscript.begin(), subscript.end())));
            Array a;
            BOOST_FOREACH(const CTxDestination& addr, addresses)
                a.push_back(CBitcoinAddress(addr).ToString());
            obj.push_back(Pair("addresses", a));
            if (whichType == TX_MULTISIG)
                obj.push_back(Pair("sigsrequired", nRequired));
        }
        return obj;
    }
};
#endif

/* MCHN START */

string getparamstring(string param)
{
    string str;
    str="";
    if(mapMultiArgs.count(param))
    {
        for(int i=0;i<(int)mapMultiArgs[param].size();i++)
        {
            if(str.size())
            {
                str += ",";
            }
            str += mapMultiArgs[param][i];
        }        
    }
    return str;
}

Value getruntimeparams(const json_spirit::Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)                                            
        throw runtime_error("Help message not found\n");
    
    Object obj;
/*    
    obj.push_back(Pair("daemon",GetBoolArg("-daemon",false)));                    
    obj.push_back(Pair("datadir",mc_gState->m_Params->DataDir(0,0)));                    
    obj.push_back(Pair("debug",getparamstring("-debug")));   
 */ 
    obj.push_back(Pair("port",GetListenPort()));   
    obj.push_back(Pair("reindex",GetBoolArg("-reindex",false)));                    
    obj.push_back(Pair("rescan",GetBoolArg("-rescan",false)));                    
/*    
    obj.push_back(Pair("rpcallowip",getparamstring("-rpcallowip")));   
    obj.push_back(Pair("rpcport",GetArg("-rpcport", BaseParams().RPCPort())));   
 */ 
    obj.push_back(Pair("txindex",GetBoolArg("-txindex",true)));                    
    obj.push_back(Pair("autocombineminconf",GetArg("-autocombineminconf", 1)));                    
    obj.push_back(Pair("autocombinemininputs",GetArg("-autocombinemininputs", 50)));                    
    obj.push_back(Pair("autocombinemaxinputs",GetArg("-autocombinemaxinputs", 100)));                    
    obj.push_back(Pair("autocombinedelay",GetArg("-autocombinedelay", 1)));                    
    obj.push_back(Pair("autocombinesuspend",GetArg("-autocombinesuspend", 15)));                    
    obj.push_back(Pair("autosubscribe",GetArg("-autosubscribe","")));   
    CKeyID keyID;
    CPubKey pkey;            
    if(!pwalletMain->GetKeyFromAddressBook(pkey,MC_PTP_CONNECT))
    {
        LogPrintf("mchn: Cannot find address having connect permission, trying default key\n");
        pkey=pwalletMain->vchDefaultKey;
    }
    keyID=pkey.GetID();    
    obj.push_back(Pair("handshakelocal",GetArg("-handshakelocal",CBitcoinAddress(keyID).ToString())));   
    obj.push_back(Pair("bantx",GetArg("-bantx","")));                    
    obj.push_back(Pair("lockblock",GetArg("-lockblock","")));                        
    obj.push_back(Pair("hideknownopdrops",GetBoolArg("-hideknownopdrops",false)));                    
    obj.push_back(Pair("maxshowndata",GetArg("-maxshowndata",MAX_OP_RETURN_SHOWN)));                    
    obj.push_back(Pair("maxqueryscanitems",GetArg("-maxqueryscanitems",MAX_STREAM_QUERY_ITEMS)));                    
    obj.push_back(Pair("v1apicompatible",GetBoolArg("-v1apicompatible",false)));                    
    obj.push_back(Pair("miningrequirespeers",Params().MiningRequiresPeers()));                    
    obj.push_back(Pair("mineemptyrounds",Params().MineEmptyRounds()));                    
    obj.push_back(Pair("miningturnover",Params().MiningTurnover()));                    
    obj.push_back(Pair("lockadminminerounds",Params().LockAdminMineRounds()));                    
    obj.push_back(Pair("gen",GetBoolArg("-gen", true)));                    
    obj.push_back(Pair("genproclimit",GetArg("-genproclimit", 1)));                    
    obj.push_back(Pair("lockinlinemetadata",GetBoolArg("-lockinlinemetadata", true)));                    
    obj.push_back(Pair("acceptfiltertimeout",GetArg("-acceptfiltertimeout", DEFAULT_ACCEPT_FILTER_TIMEOUT)));                    
    obj.push_back(Pair("sendfiltertimeout",GetArg("-sendfiltertimeout", DEFAULT_SEND_FILTER_TIMEOUT)));                    
/*
    obj.push_back(Pair("shortoutput",GetBoolArg("-shortoutput",false)));                    
    obj.push_back(Pair("walletdbversion", mc_gState->GetWalletDBVersion()));                
*/
    return obj;
}

bool paramtobool(Value param,bool strict)
{
    if(!strict)
    {
        if(param.type() == str_type)
        {
            if(param.get_str() == "true")
            {
                return true;
            }
            return atoi(param.get_str().c_str()) != 0;
        }
        if(param.type() == int_type)
        {
            if(param.get_int())
            {
                return true;
            }
            else
            {
                return false;            
            }
        }
    }
    
    if(param.type() != bool_type)
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter value type, should be boolean");                            
    }    
    
    return param.get_bool();    
}

bool paramtobool(Value param)
{
    return paramtobool(param,true);
}


Value setruntimeparam(const json_spirit::Array& params, bool fHelp)
{
    if (fHelp || params.size() != 2)                                            
        throw runtime_error("Help message not found\n");
    
    if(mc_gState->m_NetworkParams->IsProtocolMultichain() == 0)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "This API is supported only if protocol=multichain");                
    }
    
    string param_name=params[0].get_str();
    string old_value="";
    bool old_value_found=false;
    if (mapArgs.count("-" + param_name))
    {
        old_value_found=true;
        old_value=mapArgs["-" + param_name];
    }
    
    bool fFound=false;
    if(param_name == "miningrequirespeers")
    {
        mapArgs ["-" + param_name]=paramtobool(params[1],false) ? "1" : "0";
        fFound=true;
    }
    if(param_name == "mineemptyblocks")
    {
        mapArgs ["-" + param_name]=paramtobool(params[1],false) ? "1" : "0";
        fFound=true;
    }
    if(param_name == "mineblocksondemand")
    {
        mapArgs ["-" + param_name]=paramtobool(params[1],false) ? "1" : "0";
        fFound=true;
    }
    if(param_name == "hideknownopdrops")
    {
        mapArgs ["-" + param_name]=paramtobool(params[1],false) ? "1" : "0";
        fFound=true;
    }
    if(param_name == "lockinlinemetadata")
    {
        mapArgs ["-" + param_name]=paramtobool(params[1],false) ? "1" : "0";
        fFound=true;
    }
    if(param_name == "mineemptyrounds")
    {
        if( (params[1].type() == real_type) || (params[1].type() == str_type) )
        {
            double dValue;
            if(params[1].type() == real_type)
            {
                dValue=params[1].get_real();
            }
            else
            {
                dValue=atof(params[1].get_str().c_str());
            }
            mapArgs ["-" + param_name]= strprintf("%f", dValue);                                                                
        }
        else
        {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter value type");                                            
        }
        fFound=true;
    }
    if(param_name == "miningturnover")
    {
        if( (params[1].type() == real_type) || (params[1].type() == str_type) )
        {
            double dValue;
            if(params[1].type() == real_type)
            {
                dValue=params[1].get_real();
            }
            else
            {
                dValue=atof(params[1].get_str().c_str());
            }
            if( (dValue >= 0.) && (dValue <= 1.) )
            {
                mapArgs ["-" + param_name]= strprintf("%f", dValue);                                                                
            }
            else
            {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Should be in range (0,1)");                                                
            }
        }
        else
        {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter value type");                                            
        }
        fFound=true;
    }
    if( (param_name == "lockadminminerounds") ||
        (param_name == "maxshowndata") ||
        (param_name == "maxqueryscanitems") ||
        (param_name == "acceptfiltertimeout") ||
        (param_name == "sendfiltertimeout") ||
        (param_name == "dropmessagestest") )
    {
        if( (params[1].type() == int_type) || (params[1].type() == str_type) )
        {
            int nValue;
            if(params[1].type() == int_type)
            {
                nValue=params[1].get_int();
            }
            else
            {
                nValue=atoi(params[1].get_str().c_str());
            }
            if( nValue >= 0 )
            {
                mapArgs ["-" + param_name]=strprintf("%d", nValue);                                
                if(param_name == "acceptfiltertimeout")
                {
                    if(pMultiChainFilterEngine)
                    {
                        pMultiChainFilterEngine->SetTimeout(pMultiChainFilterEngine->GetAcceptTimeout());
                    }
                }
            }
            else
            {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Should be non-negative");                                                
            }
        }
        else
        {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter value type");                                            
        }
        fFound=true;
    }
    if( (param_name == "compatibility") )
    {
        if( (params[1].type() == int_type) || (params[1].type() == str_type) )
        {
            int nValue;
            if(params[1].type() == int_type)
            {
                nValue=params[1].get_int();
            }
            else
            {
                nValue=atoi(params[1].get_str().c_str());
            }
            mc_gState->m_Compatibility=(uint32_t)nValue;
        }        
        else
        {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter value type");                                            
        }
        fFound=true;
    }
    if(param_name == "bantx")
    {
        if(params[1].type() == str_type)
        {
            string error=SetBannedTxs(params[1].get_str());
            if(error.size())
            {
                throw JSONRPCError(RPC_INVALID_PARAMETER, error);                                                                        
            }
            else
            {
                mapArgs ["-" + param_name]=params[1].get_str();                                
            }
        }   
        else
        {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter value type");                                                        
        }
        fFound=true;
    }
    if(param_name == "lockblock")
    {
        if(params[1].type() == str_type)
        {
            string error=SetLockedBlock(params[1].get_str());
            if(error.size())
            {
                throw JSONRPCError(RPC_INVALID_PARAMETER, error);                                                                        
            }
            else
            {
                mapArgs ["-" + param_name]=params[1].get_str();                                
            }
        }   
        else
        {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter value type");                                                        
        }
        fFound=true;
    }
    if(param_name == "handshakelocal")
    {
        if(params[1].type() == str_type)
        {
            CBitcoinAddress address(params[1].get_str());
            if (!address.IsValid())    
            {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");                                                                        
            }
            else
            {
                mapArgs ["-" + param_name]=params[1].get_str();                                
            }
        }   
        else
        {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter value type");                                                        
        }
        fFound=true;
    }
    if(param_name == "autosubscribe")
    {
        if(params[1].type() == str_type)
        {
            string autosubscribe=params[1].get_str();
            uint32_t mode=MC_WMD_NONE;
            
            mode |= mc_AutosubscribeWalletMode(params[1].get_str(),false);

/*
            bool found=false;
            if(autosubscribe=="streams")
            {
                mode |= MC_WMD_AUTOSUBSCRIBE_STREAMS;
                found=true;
            }
            if(autosubscribe=="assets")
            {
                mode |= MC_WMD_AUTOSUBSCRIBE_ASSETS;
                found=true;
            }
            if( (autosubscribe=="assets,streams") || (autosubscribe=="streams,assets"))
            {
                mode |= MC_WMD_AUTOSUBSCRIBE_STREAMS;
                mode |= MC_WMD_AUTOSUBSCRIBE_ASSETS;
                found=true;
            }              
            if( autosubscribe=="" )
            {
                found=true;
            }
            if(!found)
            {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter value");                                                                        
            }
*/
            if(mode == MC_WMD_NONE)
            {
                if(params[1].get_str().size() != 0)
                {
                    throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter value");                                                                                            
                }
            }
            
            if(pwalletTxsMain)
            {
                pwalletTxsMain->SetMode(mode,MC_WMD_AUTOSUBSCRIBE_STREAMS | MC_WMD_AUTOSUBSCRIBE_ASSETS);
            }
            mapArgs ["-" + param_name]=params[1].get_str();                                
        }   
        else
        {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter value type");                                                        
        }
        fFound=true;
    }
    
    if(!fFound)
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Unsupported runtime parameter: " + param_name);                                                    
    }

    if(mc_StoreSetRuntimeParam(param_name))
    {
        if(old_value_found)
        {
            mapArgs["-" + param_name]=old_value;
        }
        else
        {
            mapArgs.erase("-" + param_name);
        }
        throw JSONRPCError(RPC_GENERAL_FILE_ERROR, "Cannot store new parameter value permanently");                                                            
    }
    
    SetMultiChainRuntimeParams();    
    
    
    return Value::null;
}    

Value getblockchainparams(const json_spirit::Array& params, bool fHelp)
{
    if (fHelp || params.size() > 2)                                            // MCHN
        throw runtime_error("Help message not found\n");

    bool fDisplay = true;
    if (params.size() > 0)
        fDisplay = params[0].get_bool();
    
    int nHeight=chainActive.Height();
    if (params.size() > 1)
    {
        if(params[1].type() == bool_type)
        {
            if(!params[1].get_bool())
            {
                nHeight=0;
            }
        }
        else
        {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "with-upgrades should be boolean");                
/*            
            if(params[1].type() == int_type)
            {
                nHeight=params[1].get_int();
            }            
 */ 
        }
    }
    
    if (nHeight < 0 || nHeight > chainActive.Height())
    {
        nHeight+=chainActive.Height();
        if (nHeight <= 0 || nHeight > chainActive.Height())
        {
            throw JSONRPCError(RPC_BLOCK_NOT_FOUND, "Block height out of range");
        }
    }
    
    Object obj;
    int protocol_version=(int)mc_gState->m_NetworkParams->GetInt64Param("protocolversion");
    
    if(nHeight)
    {
        protocol_version=mc_gState->m_NetworkParams->ProtocolVersion();
    }

    for(int i=0;i<mc_gState->m_NetworkParams->m_Count;i++)
    {
        if((mc_gState->m_NetworkParams->m_lpParams+i)->IsRelevant(protocol_version))
        {

            string param_name;
            Value param_value;
            unsigned char* ptr;
            int size;
            bool hidden=false;
            string param_string="";;

            ptr=(unsigned char*)mc_gState->m_NetworkParams->GetParam((mc_gState->m_NetworkParams->m_lpParams+i)->m_Name,&size);
            if(size == 0)
            {
                ptr=NULL;
            }
/*            
            else
            {
                if(((mc_gState->m_NetworkParams->m_lpParams+i)->m_Type & MC_PRM_DATA_TYPE_MASK) == MC_PRM_STRING)
                {                    
                    if(size == 1)
                    {
                        ptr=NULL;                    
                    }
                }
            }
*/
            if(fDisplay)
            {
                param_name=(mc_gState->m_NetworkParams->m_lpParams+i)->m_DisplayName;
            }
            else
            {
                param_name=(mc_gState->m_NetworkParams->m_lpParams+i)->m_Name;            
            }

            if(ptr)
            {
                switch((mc_gState->m_NetworkParams->m_lpParams+i)->m_Type & MC_PRM_DATA_TYPE_MASK)
                {
                    case MC_PRM_BINARY:
                        for(int c=0;c<size;c++)
                        {
                            param_string += strprintf("%02x",*((unsigned char*)ptr+c));
                        }
                        param_value=param_string;
                        break;
                    case MC_PRM_STRING:
                        param_value = string((char*)ptr);
                        break;
                    case MC_PRM_BOOLEAN:
                        if(*(char*)ptr)
                        {
                            param_value=true;                               
                        }
                        else
                        {
                            param_value=false;                               
                        }
                        break;
                    case MC_PRM_INT32:
                        if((mc_gState->m_NetworkParams->m_lpParams+i)->m_Type & MC_PRM_DECIMAL)
                        {
//                            param_value=((double)(int)mc_GetLE(ptr,4))/MC_PRM_DECIMAL_GRANULARITY;
                            int n=(int)mc_GetLE(ptr,4);
                            if(n >= 0)
                            {
//                                param_value=((double)n+mc_gState->m_NetworkParams->ParamAccuracy())/MC_PRM_DECIMAL_GRANULARITY;
                                param_value=((double)n)/MC_PRM_DECIMAL_GRANULARITY;
                            }
                            else
                            {
//                                param_value=-((double)(-n)+mc_gState->m_NetworkParams->ParamAccuracy())/MC_PRM_DECIMAL_GRANULARITY;                                        
                                param_value=-((double)(-n))/MC_PRM_DECIMAL_GRANULARITY;                                        
                            }
                        }
                        else
                        {
                            param_value=(int)mc_GetLE(ptr,4);                                                                
                        }
                        break;
                    case MC_PRM_UINT32:
                        if((mc_gState->m_NetworkParams->m_lpParams+i)->m_Type & MC_PRM_DECIMAL)
                        {
//                            param_value=((double)mc_GetLE(ptr,4)+mc_gState->m_NetworkParams->ParamAccuracy())/MC_PRM_DECIMAL_GRANULARITY;
                            param_value=((double)mc_GetLE(ptr,4))/MC_PRM_DECIMAL_GRANULARITY;
                        }
                        else
                        {
                            param_value=mc_GetLE(ptr,4);                                                                
                            if((mc_gState->m_NetworkParams->m_lpParams+i)->m_Type & MC_PRM_HIDDEN)
                            {
                                if(mc_GetLE(ptr,4) == (mc_gState->m_NetworkParams->m_lpParams+i)->m_DefaultIntegerValue)
                                {
                                    hidden=true;
                                }
                            }
                        }
                        break;
                    case MC_PRM_INT64:
                        param_value=mc_GetLE(ptr,8);                                                                
                        break;
                    case MC_PRM_DOUBLE:
                        param_value=*(double*)ptr;                                                                
                        break;
                }
            }
            else
            {
                param_value=Value::null;
            }
            if(nHeight)
            {
                if(strcmp("protocolversion",(mc_gState->m_NetworkParams->m_lpParams+i)->m_Name) == 0)
                {
                    param_value=mc_gState->m_NetworkParams->m_ProtocolVersion;
                }
                if(strcmp("maximumblocksize",(mc_gState->m_NetworkParams->m_lpParams+i)->m_Name) == 0)
                {
                    param_value=(int)MAX_BLOCK_SIZE;
                }
                if(strcmp("targetblocktime",(mc_gState->m_NetworkParams->m_lpParams+i)->m_Name) == 0)
                {
                    param_value=(int)MCP_TARGET_BLOCK_TIME;
                }
                if(strcmp("maxstdtxsize",(mc_gState->m_NetworkParams->m_lpParams+i)->m_Name) == 0)
                {
                    param_value=(int)MAX_STANDARD_TX_SIZE;
                }
                if(strcmp("maxstdopreturnscount",(mc_gState->m_NetworkParams->m_lpParams+i)->m_Name) == 0)
                {
                    param_value=(int)MCP_MAX_STD_OP_RETURN_COUNT;
                }
                if(strcmp("maxstdopreturnsize",(mc_gState->m_NetworkParams->m_lpParams+i)->m_Name) == 0)
                {
                    param_value=(int)MAX_OP_RETURN_RELAY;
                }
                if(strcmp("maxstdopdropscount",(mc_gState->m_NetworkParams->m_lpParams+i)->m_Name) == 0)
                {
                    param_value=(int)MCP_STD_OP_DROP_COUNT;
                }
                if(strcmp("maxstdelementsize",(mc_gState->m_NetworkParams->m_lpParams+i)->m_Name) == 0)
                {
                    param_value=(int)MAX_SCRIPT_ELEMENT_SIZE;
                }
                if(strcmp("maximumchunksize",(mc_gState->m_NetworkParams->m_lpParams+i)->m_Name) == 0)
                {
                    param_value=(int)MAX_CHUNK_SIZE;
                }
                if(strcmp("maximumchunkcount",(mc_gState->m_NetworkParams->m_lpParams+i)->m_Name) == 0)
                {
                    param_value=(int)MAX_CHUNK_COUNT;
                }
                if(strcmp("anyonecanconnect",(mc_gState->m_NetworkParams->m_lpParams+i)->m_Name) == 0)
                {
                    param_value=(MCP_ANYONE_CAN_CONNECT != 0);
                }
                if(strcmp("anyonecansend",(mc_gState->m_NetworkParams->m_lpParams+i)->m_Name) == 0)
                {
                    param_value=(MCP_ANYONE_CAN_SEND != 0);
                }
                if(strcmp("anyonecanreceive",(mc_gState->m_NetworkParams->m_lpParams+i)->m_Name) == 0)
                {
                    param_value=(MCP_ANYONE_CAN_RECEIVE != 0);
                }
                if(strcmp("anyonecanreceiveempty",(mc_gState->m_NetworkParams->m_lpParams+i)->m_Name) == 0)
                {
                    param_value=(MCP_ANYONE_CAN_RECEIVE_EMPTY != 0);
                }
                if(strcmp("anyonecancreate",(mc_gState->m_NetworkParams->m_lpParams+i)->m_Name) == 0)
                {
                    param_value=(MCP_ANYONE_CAN_CREATE != 0);
                }
                if(strcmp("anyonecanissue",(mc_gState->m_NetworkParams->m_lpParams+i)->m_Name) == 0)
                {
                    param_value=(MCP_ANYONE_CAN_ISSUE != 0);
                }
                if(strcmp("anyonecanactivate",(mc_gState->m_NetworkParams->m_lpParams+i)->m_Name) == 0)
                {
                    param_value=(MCP_ANYONE_CAN_ACTIVATE != 0);
                }
            }
    
            if(!hidden)
            {
                obj.push_back(Pair(param_name,param_value));        
            }
        }
    }
    
    return obj;    
}

void SetSynchronizedFlag(CTxDestination &dest,Object &ret)
{
    if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)
    {
        mc_TxEntityStat entStat;
        CKeyID *lpKeyID=boost::get<CKeyID> (&dest);
        CScriptID *lpScriptID=boost::get<CScriptID> (&dest);
        entStat.Zero();
        if(lpKeyID)
        {
            memcpy(&entStat,lpKeyID,MC_TDB_ENTITY_ID_SIZE);
            entStat.m_Entity.m_EntityType=MC_TET_PUBKEY_ADDRESS | MC_TET_CHAINPOS;
        }
        if(lpScriptID)
        {
            memcpy(&entStat,lpScriptID,MC_TDB_ENTITY_ID_SIZE);
            entStat.m_Entity.m_EntityType=MC_TET_SCRIPT_ADDRESS | MC_TET_CHAINPOS;
        }            
        if(pwalletTxsMain->FindEntity(&entStat))
        {
            if(entStat.m_Flags & MC_EFL_NOT_IN_SYNC)
            {
                ret.push_back(Pair("synchronized",false));                                                            
                ret.push_back(Pair("startblock",entStat.m_LastImportedBlock+1));                                                                            
            }
            else
            {
                ret.push_back(Pair("synchronized",true));                                                                            
            }
        }
    }
}

Object AddressEntry(CBitcoinAddress& address,uint32_t verbose)
{
// 0x01 add isvalid=true
// 0x02 verbose    
    
    Object ret;
    if(verbose & 0x01)
    {
        ret.push_back(Pair("isvalid", true));
    }
    CTxDestination dest = address.Get();
    string currentAddress = address.ToString();
    ret.push_back(Pair("address", currentAddress));
#ifdef ENABLE_WALLET
    isminetype mine = pwalletMain ? IsMine(*pwalletMain, dest) : ISMINE_NO;
    ret.push_back(Pair("ismine", (mine & ISMINE_SPENDABLE) ? true : false));
    if(verbose & 0x02)
    {
        if (mine != ISMINE_NO) {
            ret.push_back(Pair("iswatchonly", (mine & ISMINE_WATCH_ONLY) ? true: false));
            Object detail = boost::apply_visitor(DescribeAddressVisitor(mine), dest);
            ret.insert(ret.end(), detail.begin(), detail.end());
        }
        if (pwalletMain && pwalletMain->mapAddressBook.count(dest))
            ret.push_back(Pair("account", pwalletMain->mapAddressBook[dest].name));
    #endif
        SetSynchronizedFlag(dest,ret);
    }        
    
    return ret;
}


/* MCHN END */

Value validateaddress(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)                                            // MCHN
        throw runtime_error("Help message not found\n");

    string str=params[0].get_str();
    CBitcoinAddress address(str);
    bool isValid = address.IsValid();

    Object ret;
/* MCHN START */        
    if(!isValid)
    {
        CBitcoinSecret vchSecret;
        CPubKey pubkey;
        isValid = vchSecret.SetString(str);

        if (isValid)
        {
            CKey key;
            key = vchSecret.GetKey();
            isValid=key.IsValid();
            if (isValid) 
            {
                pubkey = key.GetPubKey();
                isValid=key.VerifyPubKey(pubkey);
            }
        }        
        else
        {
            if (IsHex(str))
            {
                pubkey=CPubKey(ParseHex(str));
                isValid=pubkey.IsFullyValid();            
            }                
        }        
        if(isValid)
        {
            address=CBitcoinAddress(pubkey.GetID());
        }        
    }
    
    if(!isValid)
    {
        ret.push_back(Pair("isvalid", false));        
        return ret;
    }
    
    return AddressEntry(address,0x03);
/*    
    ret.push_back(Pair("isvalid", isValid));
    if (isValid)
    {
        CTxDestination dest = address.Get();
        string currentAddress = address.ToString();
        ret.push_back(Pair("address", currentAddress));
#ifdef ENABLE_WALLET
        isminetype mine = pwalletMain ? IsMine(*pwalletMain, dest) : ISMINE_NO;
        ret.push_back(Pair("ismine", (mine & ISMINE_SPENDABLE) ? true : false));
        if (mine != ISMINE_NO) {
            ret.push_back(Pair("iswatchonly", (mine & ISMINE_WATCH_ONLY) ? true: false));
            Object detail = boost::apply_visitor(DescribeAddressVisitor(mine), dest);
            ret.insert(ret.end(), detail.begin(), detail.end());
        }
        if (pwalletMain && pwalletMain->mapAddressBook.count(dest))
            ret.push_back(Pair("account", pwalletMain->mapAddressBook[dest].name));
#endif
        SetSynchronizedFlag(dest,ret);
        
    }
*/   
//    return ret;
/* MCHN END */        
}

/* MCHN START */

Value createkeypairs(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)                                            // MCHN
        throw runtime_error("Help message not found\n");

    int count=1;
    if (params.size() > 0)    
    {
        count=paramtoint(params[0],true,0,"Invalid count");
    }
            
    Array retArray;
    bool fCompressed = true;
    
    for(int i=0;i<count;i++)
    {
        CKey secret;
        secret.MakeNewKey(fCompressed);
        
        CPubKey pubkey = secret.GetPubKey();

        Object entry;
        entry.push_back(Pair("address", CBitcoinAddress(pubkey.GetID()).ToString()));
        entry.push_back(Pair("pubkey", HexStr(pubkey)));
        entry.push_back(Pair("privkey", CBitcoinSecret(secret).ToString()));
        
        retArray.push_back(entry);
    }        
    
    
    return retArray;
}

bool mc_GetDefaultLicenseAddress(CBitcoinAddress& license_address)
{
    BOOST_FOREACH(const PAIRTYPE(CBitcoinAddress, CAddressBookData)& item, pwalletMain->mapAddressBook)
    {
        const CBitcoinAddress& address = item.first;
        if(item.second.purpose == "license")
        {
            license_address=address;
            return true;
        }
    }    
    return false;
}

Value getaddresses(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)                                            // MCHN
        throw runtime_error("Help message not found\n");

    int verbose=0x00;
    
    if (params.size() > 0)
    {
        if(params[0].type() == bool_type)
        {
            if(params[0].get_bool())
            {
                verbose=0x02;
            }        
        }
        else
        {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid value for 'verbose' parameter, should be boolean");                                                                
        }        
    }

    Array ret;

    if((mc_gState->m_WalletMode & MC_WMD_TXS))
    {
        int entity_count;
        mc_TxEntityStat *lpEntity;
        CBitcoinAddress license_address;
//        bool has_license=mc_GetDefaultLicenseAddress(license_address);
        bool has_license=false;
        
        entity_count=pwalletTxsMain->GetEntityListCount();
        for(int i=0;i<entity_count;i++)
        {
            lpEntity=pwalletTxsMain->GetEntity(i);        
            CBitcoinAddress address;
            if( ((lpEntity->m_Entity.m_EntityType & MC_TET_ORDERMASK) == MC_TET_CHAINPOS) &&
                ((lpEntity->m_Flags & MC_EFL_LICENSE) == 0 ) )   
            {
                if(CBitcoinAddressFromTxEntity(address,&(lpEntity->m_Entity)))
                {
                    if(!has_license || !(address == license_address) )
                    {
                        if(verbose)
                        {
                            ret.push_back(AddressEntry(address,verbose));
                        }
                        else
                        {
                            ret.push_back(address.ToString());                        
                        }
                    }
                }
            }
        }        
    }
    else
    {        
        // Find all addresses that have the given account
        BOOST_FOREACH(const PAIRTYPE(CBitcoinAddress, CAddressBookData)& item, pwalletMain->mapAddressBook)
        {
            const CBitcoinAddress& address = item.first;

            if(verbose)
            {
                Object addr;
                CTxDestination dest = address.Get();
                string currentAddress = address.ToString();
                addr.push_back(Pair("address", currentAddress));
                isminetype mine = pwalletMain ? IsMine(*pwalletMain, dest) : ISMINE_NO;
                addr.push_back(Pair("ismine", (mine & ISMINE_SPENDABLE) ? true : false));
                if (mine != ISMINE_NO) {
                    addr.push_back(Pair("iswatchonly", (mine & ISMINE_WATCH_ONLY) ? true: false));
                    Object detail = boost::apply_visitor(DescribeAddressVisitor(mine), dest);
                    addr.insert(addr.end(), detail.begin(), detail.end());
                }
                if (pwalletMain && pwalletMain->mapAddressBook.count(dest))
                    addr.push_back(Pair("account", pwalletMain->mapAddressBook[dest].name));

                SetSynchronizedFlag(dest,addr);

                ret.push_back(addr);            
            }
            else
            {
                ret.push_back(address.ToString());            
            }

        }
    }
    return ret;
}

/* MCHN END */
/**
 * Used by addmultisigaddress / createmultisig:
 */
CScript _createmultisig_redeemScript(const Array& params)
{
    int nRequired = params[0].get_int();
    const Array& keys = params[1].get_array();

    // Gather public keys
    if (nRequired < 1)
        throw runtime_error("a multisignature address must require at least one key to redeem");
    if ((int)keys.size() < nRequired)
        throw runtime_error(
            strprintf("not enough keys supplied "
                      "(got %u keys, but need at least %d to redeem)", keys.size(), nRequired));
    if (keys.size() > 16)
        throw runtime_error("Number of addresses involved in the multisignature address creation > 16\nReduce the number");
    std::vector<CPubKey> pubkeys;
    pubkeys.resize(keys.size());
    for (unsigned int i = 0; i < keys.size(); i++)
    {
        const std::string& ks = keys[i].get_str();
#ifdef ENABLE_WALLET
        // Case 1: Bitcoin address and we have full public key:
        CBitcoinAddress address(ks);
        if (pwalletMain && address.IsValid())
        {
            CKeyID keyID;
            if (!address.GetKeyID(keyID))
                throw runtime_error(
                    strprintf("%s does not refer to a key",ks));
            CPubKey vchPubKey;
            if (!pwalletMain->GetPubKey(keyID, vchPubKey))
                throw runtime_error(
                    strprintf("no full public key for address %s",ks));
            if (!vchPubKey.IsFullyValid())
                throw runtime_error(" Invalid public key: "+ks);
            pubkeys[i] = vchPubKey;
        }

        // Case 2: hex public key
        else
#endif
        if (IsHex(ks))
        {
            CPubKey vchPubKey(ParseHex(ks));
            if (!vchPubKey.IsFullyValid())
                throw runtime_error(" Invalid public key: "+ks);
            pubkeys[i] = vchPubKey;
        }
        else
        {
            throw runtime_error(" Invalid public key: "+ks);
        }
    }
    CScript result = GetScriptForMultisig(nRequired, pubkeys);

    if(Params().RequireStandard())
    {    
        if (result.size() > MAX_SCRIPT_ELEMENT_SIZE)
            throw runtime_error(
                    strprintf("redeemScript exceeds size limit: %d > %d", result.size(), MAX_SCRIPT_ELEMENT_SIZE));
    }
    
    return result;
}

Value createmultisig(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 2)                        // MCHN
    {
        throw runtime_error("Help message not found\n");
    }

    
/* MCHN START */    
    if(mc_gState->m_NetworkParams->IsProtocolMultichain())
    {
        if((MCP_ALLOW_ARBITRARY_OUTPUTS == 0) || (mc_gState->m_Features->FixedDestinationExtraction() == 0) )
        {
            if(MCP_ALLOW_P2SH_OUTPUTS == 0)
            {
                throw JSONRPCError(RPC_NOT_ALLOWED, "P2SH outputs are not allowed for this blockchain");
            }
        }
    }
/* MCHN END */    
    
    
    // Construct using pay-to-script-hash:
    CScript inner = _createmultisig_redeemScript(params);
    CScriptID innerID(inner);
    CBitcoinAddress address(innerID);

    Object result;
    result.push_back(Pair("address", address.ToString()));
    result.push_back(Pair("redeemScript", HexStr(inner.begin(), inner.end())));

    return result;
}

Value verifymessage(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 3)                                            // MCHN
        mc_ThrowHelpMessage("verifymessage");

    string strAddress  = params[0].get_str();
    string strSign     = params[1].get_str();
    string strMessage  = params[2].get_str();

    CBitcoinAddress addr(strAddress);
    if (!addr.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");

    CKeyID keyID;
    if (!addr.GetKeyID(keyID))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Address does not refer to key");

    bool fInvalid = false;
    vector<unsigned char> vchSig = DecodeBase64(strSign.c_str(), &fInvalid);

    if (fInvalid)
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Malformed base64 encoding");

    CHashWriter ss(SER_GETHASH, 0);
    ss << strMessageMagic;
    ss << strMessage;

    CPubKey pubkey;
    if (!pubkey.RecoverCompact(ss.GetHash(), vchSig))
        return false;

    return (pubkey.GetID() == keyID);
}

Value setmocktime(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error("Help message not found\n");

    if (!Params().MineBlocksOnDemand())
        throw runtime_error("setmocktime for regression testing (-regtest mode) only");

    RPCTypeCheck(params, boost::assign::list_of(int_type));
    SetMockTime(params[0].get_int64());

    return Value::null;
}

Value getdiagnostics(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error("Help message not found\n");
    
    unsigned char* ptr;
    int size;
    Object result;
    
    Object chain;
    Object runtime;
    Object mempool_info;    
    Object node;
    
    ptr=(unsigned char*)mc_gState->m_NetworkParams->GetParam("chainparamshash",&size);    
    chain.push_back(Pair("chain-params-hash", HexStr(ptr,ptr+size)));    
    chain.push_back(Pair("protocol-version", mc_gState->m_NetworkParams->ProtocolVersion()));
    chain.push_back(Pair("maximum-block-size", (int)MAX_BLOCK_SIZE));    
    chain.push_back(Pair("target-block-time", MCP_TARGET_BLOCK_TIME));    
    chain.push_back(Pair("anyone-can-connect", MCP_ANYONE_CAN_CONNECT ? true : false));    
    chain.push_back(Pair("anyone-can-mine", mc_gState->m_NetworkParams->GetInt64Param("anyonecanmine") ? true : false));    
    chain.push_back(Pair("anyone-can-admin", mc_gState->m_NetworkParams->GetInt64Param("anyonecanadmin") ? true : false));    
    chain.push_back(Pair("support-miner-precheck", mc_gState->m_NetworkParams->GetInt64Param("supportminerprecheck") ? true : false));    
    chain.push_back(Pair("target-adjust-freq", (int)mc_gState->m_NetworkParams->GetInt64Param("targetadjustfreq")));    
    chain.push_back(Pair("setup-blocks", mc_gState->m_NetworkParams->GetInt64Param("setupfirstblocks")));    
    chain.push_back(Pair("mining-diversity",mc_gState->m_NetworkParams->GetDoubleParam("miningdiversity")));    
    chain.push_back(Pair("admin-consensus-admin",mc_gState->m_NetworkParams->GetDoubleParam("adminconsensusadmin")));    
    chain.push_back(Pair("admin-consensus-mine",mc_gState->m_NetworkParams->GetDoubleParam("adminconsensusmine")));    
    ptr=(unsigned char*)mc_gState->m_NetworkParams->GetParam("addresspubkeyhashversion",&size);    
    chain.push_back(Pair("address-pubkeyhash-version", HexStr(ptr,ptr+size)));    
    ptr=(unsigned char*)mc_gState->m_NetworkParams->GetParam("addressscripthashversion",&size);    
    chain.push_back(Pair("address-scripthash-version", HexStr(ptr,ptr+size)));    
    ptr=(unsigned char*)mc_gState->m_NetworkParams->GetParam("addresschecksumvalue",&size);    
    chain.push_back(Pair("address-checksum-value", HexStr(ptr,ptr+size)));    
    result.push_back(Pair("blockchainparams",chain));
    
    node.push_back(Pair("nodeversion", mc_gState->GetNumericVersion()));
    node.push_back(Pair("defaultaddress", CBitcoinAddress(pwalletMain->vchDefaultKey.GetID()).ToString()));
    node.push_back(Pair("incomingpaused", (mc_gState->m_NodePausedState & MC_NPS_INCOMING) ? true : false));                
    node.push_back(Pair("miningpaused", (mc_gState->m_NodePausedState & MC_NPS_MINING) ? true : false));                
    node.push_back(Pair("reindex",       fReindex));    
    node.push_back(Pair("timeoffset",    GetTimeOffset()));    
    
    result.push_back(Pair("nodeinfo",node));
    
    runtime.push_back(Pair("miningrequirespeers",Params().MiningRequiresPeers()));        
    runtime.push_back(Pair("mineemptyrounds", Params().MineEmptyRounds()));        
    runtime.push_back(Pair("miningturnover",Params().MiningTurnover()));                    
    runtime.push_back(Pair("lockadminminerounds", Params().LockAdminMineRounds()));       
    runtime.push_back(Pair("bantx",GetArg("-bantx","")));
    runtime.push_back(Pair("lockblock",GetArg("-lockblock","")));                        
    result.push_back(Pair("runtimeparams",runtime));

    mempool_info.push_back(Pair("size", (int64_t) mempool.size()));
    mempool_info.push_back(Pair("bytes", (int64_t) mempool.GetTotalTxSize()));    
    result.push_back(Pair("mempoolinfo",mempool_info));
//    obj.push_back(Pair("", mc_gState->m_NetworkParams->GetInt64Param("")));    
    
    Array chaintips_params;
    result.push_back(Pair("chaintips",getchaintips(chaintips_params,false)));
    
    Array blocks;
    int height=chainActive.Height();
    set<CKeyID> prev_keyids;
    int32_t from_block=-1;//mcd_ParamIntValue(params,"fromblock",-1);
    bool found=false;
    while((height>=0) && !found)
    {
        CKeyID keyID;
        if(mc_gState->m_Permissions->GetBlockMiner(height,(unsigned char*)&keyID) == MC_ERR_NOERROR)
        {
            Object entry;
            entry.push_back(Pair("height",height));
            entry.push_back(Pair("hash",chainActive[height]->GetBlockHash().ToString()));
            entry.push_back(Pair("miner",CBitcoinAddress(keyID).ToString()));
            entry.push_back(Pair("size",(int)chainActive[height]->nSize));
            entry.push_back(Pair("txs",(int)chainActive[height]->nTx));
            entry.push_back(Pair("time",(int)chainActive[height]->nTime));
            blocks.push_back(entry);
            if(prev_keyids.find(keyID) != prev_keyids.end())
            {
                if((from_block==-1) || (height <= from_block))
                {
                    found=true;
                }
                else
                {
                    height--;                    
                }
            }
            else
            {
                prev_keyids.insert(keyID);
                height--;
            }
        }
        else
        {
            found=true;
        }        
    }
    result.push_back(Pair("blocks",blocks));
    
    Array miner_params;
    miner_params.push_back(false);
    miner_params.push_back(0x03);
    miner_params.push_back(height);    
//    miner_params.push_back(0);
    
    result.push_back(Pair("miners",listminers_operation(miner_params)));
    result.push_back(Pair("admins",listadmins_operation(miner_params)));
    result.push_back(Pair("connect",listconnect_operation(miner_params)));
    

    Array peers;

    {
        LOCK(cs_vNodes);
        BOOST_FOREACH(CNode* pnode, vNodes) 
        {
            CNodeStats stats;
            pnode->copyStats(stats);
            if(stats.fSuccessfullyConnected)
            {
                Object obj;
                obj.push_back(Pair("id", stats.nodeid));
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
                obj.push_back(Pair("inbound", stats.fInbound));
                peers.push_back(obj);
            }
        }
    }
       
    result.push_back(Pair("peers",peers));

    if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)
    {    
        pwalletTxsMain->Lock();
        Object wallet;

        int entity_count,address_count;
        mc_TxEntityStat *lpEntity;

        entity_count=pwalletTxsMain->GetEntityListCount();

        address_count=0;
        for(int i=0;i<entity_count;i++)
        {
            lpEntity=pwalletTxsMain->GetEntity(i);
            if( ((lpEntity->m_Entity.m_EntityType == (MC_TET_PUBKEY_ADDRESS | MC_TET_CHAINPOS)) || 
                (lpEntity->m_Entity.m_EntityType == (MC_TET_SCRIPT_ADDRESS | MC_TET_CHAINPOS))) &&
                ((lpEntity->m_Flags & MC_EFL_LICENSE) == 0 ) )               
            {
                address_count++;
            }
        }

        wallet.push_back(Pair("entities",entity_count));
        wallet.push_back(Pair("addresses",address_count));
        wallet.push_back(Pair("assetgroups",pwalletMain->lpAssetGroups->GroupCount()));
        wallet.push_back(Pair("assetspergroup",pwalletMain->lpAssetGroups->AssetsPerGroup()));

        int max_utxos=0;
        int total_utxos=(int)pwalletTxsMain->m_UTXOs[0].size();
        map <uint160,int> mapAddressCoinCounts;
        for (map<COutPoint, mc_Coin>::const_iterator it = pwalletTxsMain->m_UTXOs[0].begin(); it != pwalletTxsMain->m_UTXOs[0].end(); ++it)
        {
            const mc_Coin& coin = it->second;
            map <uint160,int>::iterator ait=mapAddressCoinCounts.find(coin.m_EntityID);
            if(ait == mapAddressCoinCounts.end())
            {
                mapAddressCoinCounts.insert(make_pair(coin.m_EntityID,1));
                if(max_utxos == 0)
                {
                    max_utxos=1;
                }
            }
            else
            {
                ait->second+=1;
                if(max_utxos < ait->second)
                {
                    max_utxos=ait->second;
                }
            }
        }        
        wallet.push_back(Pair("utxocount",total_utxos));
        wallet.push_back(Pair("maxutxosperaddress",max_utxos));
        result.push_back(Pair("wallet",wallet));
        pwalletTxsMain->UnLock();        
    }
    
    bool fVerbose = false;
    if (params.size() > 0)
        fVerbose=paramtobool(params[0],false);
    
    if(fVerbose)
    {
        return result;
    }
    
    size_t bytes;
    int err;
    const unsigned char *script;
    mc_Script *lpDetailsScript=mc_gState->m_TmpBuffers->m_RpcScript1;   
    lpDetailsScript->Clear();
    lpDetailsScript->Clear();
    lpDetailsScript->AddElement();
    if((err = ubjson_write(result,lpDetailsScript,MAX_FORMATTED_DATA_DEPTH)) != MC_ERR_NOERROR)
    {
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Couldn't transfer JSON object to internal UBJSON format");                                            
    }
    script = lpDetailsScript->GetData(0,&bytes);
    vector<unsigned char> vValue=vector<unsigned char> (script,script+bytes);                                            
    return HexStr(vValue);            
}

Value applycommands(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error("Help message not found\n");
    
    Array solution;

    int err;
    vector<unsigned char> encoded=ParseHex(params[0].get_str());

    Value value=ubjson_read((const unsigned char *)(&encoded[0]),encoded.size(),MAX_FORMATTED_DATA_DEPTH,&err);
    if(err)
    {
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Error decoding UBJSON");                                            
    }
    solution=value.get_array();
    
    Array solution_result=JSONRPCExecInternalBatch(solution);
    
    size_t bytes;
    const unsigned char *script;
    mc_Script *lpDetailsScript=mc_gState->m_TmpBuffers->m_RpcScript1;   
    lpDetailsScript->Clear();
    lpDetailsScript->Clear();
    lpDetailsScript->AddElement();
    if((err = ubjson_write(solution_result,lpDetailsScript,MAX_FORMATTED_DATA_DEPTH)) != MC_ERR_NOERROR)
    {
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Couldn't transfer JSON object to internal UBJSON format");                                            
    }
    script = lpDetailsScript->GetData(0,&bytes);
    vector<unsigned char> vValue=vector<unsigned char> (script,script+bytes);                                            
    return HexStr(vValue);            
}

Value decodehexubjson(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error("Help message not found\n");
    
    int err;
    vector<unsigned char> encoded=ParseHex(params[0].get_str());

    Value value=ubjson_read((const unsigned char *)(&encoded[0]),encoded.size(),MAX_FORMATTED_DATA_DEPTH,&err);
    if(err)
    {
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Error decoding UBJSON");                                            
    }

    return value;
}

Value encodehexubjson(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error("Help message not found\n");
    
    LOCK(cs_main);

    size_t bytes;
    int err;
    const unsigned char *script;
    mc_Script *lpDetailsScript=mc_gState->m_TmpBuffers->m_RpcScript1;   
    lpDetailsScript->Clear();
    lpDetailsScript->Clear();
    lpDetailsScript->AddElement();
    if((err = ubjson_write(params[0],lpDetailsScript,MAX_FORMATTED_DATA_DEPTH)) != MC_ERR_NOERROR)
    {
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Couldn't transfer JSON object to internal UBJSON format");                                            
    }
    script = lpDetailsScript->GetData(0,&bytes);
    vector<unsigned char> vValue=vector<unsigned char> (script,script+bytes);                                            
    return HexStr(vValue);            
}

