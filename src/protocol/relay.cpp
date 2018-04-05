// Copyright (c) 2014-2017 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "protocol/relay.h"

void mc_Limiter::Zero()
{
    memset(this,0,sizeof(mc_Limiter));    
}

int mc_Limiter::Intitialize(int seconds,int measures)
{
    Zero();
    
    if(seconds > MC_LIM_MAX_SECONDS)
    {
        return MC_ERR_INVALID_PARAMETER_VALUE;
    }
    if(measures > MC_LIM_MAX_MEASURES)
    {
        return MC_ERR_INVALID_PARAMETER_VALUE;
    }
    m_SecondCount=seconds;
    m_MeasureCount=measures;
    m_Time=mc_TimeNowAsUInt();
    
    return MC_ERR_NOERROR;
}

int mc_Limiter::SetLimit(int meausure, int64_t limit)
{
    if( (meausure > MC_LIM_MAX_MEASURES) || (meausure <0) )
    {
        return MC_ERR_INVALID_PARAMETER_VALUE;        
    }
    m_Limits[meausure]=limit*m_SecondCount;
    return MC_ERR_NOERROR;    
}

void mc_Limiter::CheckTime()
{
    CheckTime(mc_TimeNowAsUInt());
}

void mc_Limiter::CheckTime(uint32_t time_now)
{
    if(m_SecondCount == 0)
    {
        return;        
    }
    if(time_now == m_Time)
    {
        return;
    }
    if(time_now >= m_Time + m_SecondCount)
    {
        memset(m_Totals,0,MC_LIM_MAX_MEASURES);
        memset(m_Measures,0,MC_LIM_MAX_MEASURES*MC_LIM_MAX_SECONDS);
    }
    
    for(uint32_t t=m_Time;t<time_now;t++)
    {
        int p=(t+1)%m_SecondCount;
        for(int m=0;m<m_MeasureCount;m++)
        {
            m_Totals[m]-=m_Measures[m*MC_LIM_MAX_SECONDS+p];
            m_Measures[m*MC_LIM_MAX_SECONDS+p]=0;
        }
    }
    
    m_Time=time_now;
}

void mc_Limiter::SetEvent(int64_t m1)
{
    SetEvent(m1,0,0,0);
}

void mc_Limiter::SetEvent(int64_t m1,int64_t m2)
{
    SetEvent(m1,m2,0,0);    
}

void mc_Limiter::SetEvent(int64_t m1,int64_t m2,int64_t m3)
{
    SetEvent(m1,m2,m3,0);        
}

void mc_Limiter::SetEvent(int64_t m1,int64_t m2,int64_t m3,int64_t m4)
{
    m_Event[0]=m1;
    m_Event[1]=m2;
    m_Event[2]=m3;
    m_Event[3]=m4;
}

int mc_Limiter::Disallowed()
{
    return Disallowed(mc_TimeNowAsUInt());
}

int mc_Limiter::Disallowed(uint32_t t)
{
    CheckTime(t);
    
    for(int m=0;m<m_MeasureCount;m++)
    {
        if(m_Totals[m]+m_Event[m] > m_Limits[m])
        {
            return 1;
        }
    }
    
    return 0;
}

void mc_Limiter::Increment()
{
    if(m_SecondCount == 0)
    {
        return;        
    }
    
    int p=(m_Time+1)%m_SecondCount;
    for(int m=0;m<m_MeasureCount;m++)
    {
        m_Totals[m]+=m_Event[m];
        m_Measures[m*MC_LIM_MAX_SECONDS+p]+=m_Event[m];
    }    
}

void mc_NodeFullAddress::Zero()
{
    m_Address=CKeyID(0);
    m_NetAddresses.clear();
}

void mc_RelayManager::SetMyIPs(uint32_t *ips,int ip_count)
{
    m_MyIPCount=ip_count;
    for(int i=0;i<ip_count;i++)
    {
        m_MyIPs[i]=ips[i];
    }
}

void mc_RelayManager::InitNodeAddress(mc_NodeFullAddress *node_address,CNode* pto,uint32_t action)
{
    uint32_t pto_address_local;
    in_addr addr;
    
    if(action & MC_PRA_MY_ORIGIN_MC_ADDRESS)
    {
        node_address->m_Address=pto->kAddrLocal;        
    }
    else
    {
        node_address->m_Address=CKeyID(0);                
    }
    
    node_address->m_NetAddresses.clear();
    
    if(action & MC_PRA_MY_ORIGIN_NT_ADDRESS)
    {
        node_address->m_NetAddresses.push_back(CAddress(pto->addrLocal));

        pto_address_local=0;
        if(pto->addrLocal.IsIPv4())
        {
            pto_address_local=(pto->addrLocal.GetByte(3)<<24)+(pto->addrLocal.GetByte(2)<<16)+(pto->addrLocal.GetByte(1)<<8)+pto->addrLocal.GetByte(0);            
            addr.s_addr=pto_address_local;
            node_address->m_NetAddresses.push_back(CAddress(CService(addr,GetListenPort())));
        }
    }
    
    
    for(int i=0;i<m_MyIPCount;i++)
    {
        if(m_MyIPs[i] != pto_address_local)
        {
            addr.s_addr=m_MyIPs[i];
            node_address->m_NetAddresses.push_back(CAddress(CService(addr,GetListenPort())));
        }
    }        
}

void mc_RelayManager::InitNodeAddress(mc_NodeFullAddress *node_address,CKeyID& mc_address, vector<CAddress>& net_addresses)
{
    node_address->m_Address=mc_address;
    node_address->m_NetAddresses=net_addresses;
}

void mc_RelayManager::MsgTypeSettings(uint32_t msg_type,int latency,int seconds,int64_t serves_per_second,int64_t bytes_per_second)
{
    mc_Limiter limiter;
    
    map<uint32_t, int>::iterator itlat = m_Latency.find(msg_type);
    if (itlat == m_Latency.end())
    {
        m_Latency.insert(make_pair(msg_type,latency));
    }                    
    else
    {
        itlat->second=latency;
    }
    
    limiter.Intitialize(seconds,2);
    limiter.SetLimit(0,serves_per_second);
    limiter.SetLimit(1,bytes_per_second);
    
    map<uint32_t, mc_Limiter>::iterator itlim = m_Limiters.find(msg_type);
    if (itlim == m_Limiters.end())
    {
        m_Limiters.insert(make_pair(msg_type,limiter));
    }                    
    else
    {
        itlim->second=limiter;
    }
}

void mc_RelayManager::SetDefaults()
{
    MsgTypeSettings(MC_RMT_NONE           , 0,10,1000,100*1024*1024);
    MsgTypeSettings(MC_RMT_GLOBAL_PING    ,10,10, 100,  1*1024*1024);
    MsgTypeSettings(MC_RMT_GLOBAL_PONG    , 0,10, 100,  1*1024*1024);
    MsgTypeSettings(MC_RMT_GLOBAL_REJECT  , 0,10,1000,  1*1024*1024);
    MsgTypeSettings(MC_RMT_GLOBAL_BUSY    , 0,10,1000,  1*1024*1024);
    MsgTypeSettings(MC_RMT_CHUNK_QUERY    ,10,10, 100,  1*1024*1024);
    MsgTypeSettings(MC_RMT_CHUNK_QUERY_HIT,30,10, 100,  1*1024*1024);
    MsgTypeSettings(MC_RMT_CHUNK_REQUEST  ,30,10, 100,  1*1024*1024);
    MsgTypeSettings(MC_RMT_CHUNK_RESPONSE , 0,10, 100,100*1024*1024);
}

void mc_RelayManager::CheckTime()
{
    uint32_t time_now=mc_TimeNowAsUInt();
    if(time_now == m_LastTime)
    {
        return;
    }
    
    for(map<mc_RelayRecordKey,mc_RelayRecordValue>::iterator it = m_RelayRecords.begin(); it != m_RelayRecords.end();)
    {
        if(it->second.m_Timestamp < m_LastTime)
        {
            m_RelayRecords.erase(it++);
        }
        else
        {
            it++;
        }
    }        
}

void mc_RelayManager::SetRelayRecord(CNode *pto,CNode *pfrom,uint32_t msg_type,int64_t nonce)
{
    map<uint32_t, int>::iterator itlat = m_Latency.find(msg_type);
    if (itlat == m_Latency.end())
    {
        return;
    }                    
    if(itlat->second <= 0 )
    {
        return;        
    }    
    
    NodeId pto_id=0;
    if(pto)
    {
        pto_id=pto->GetId();
    }
    const mc_RelayRecordKey key=mc_RelayRecordKey(nonce,pto_id);
    mc_RelayRecordValue value;
    value.m_NodeFrom=0;
    if(pfrom)
    {
        value.m_NodeFrom=pfrom->GetId();
    }
    value.m_MsgType=msg_type;
    value.m_Timestamp=m_LastTime+itlat->second;
    
    map<const mc_RelayRecordKey, mc_RelayRecordValue>::iterator it = m_RelayRecords.find(key);
    if (it == m_RelayRecords.end())
    {
        m_RelayRecords.insert(make_pair(key,value));
    }                    
    else
    {
        it->second=value;
    }    
}

int mc_RelayManager::GetRelayRecord(CNode *pfrom,int64_t nonce,uint32_t* msg_type,CNode **pto)
{
    NodeId pfrom_id,pto_id;
    
    pfrom_id=0;
    if(pfrom)
    {
        pfrom_id=pfrom->GetId();
    }
    const mc_RelayRecordKey key=mc_RelayRecordKey(nonce,pfrom_id);
    map<mc_RelayRecordKey, mc_RelayRecordValue>::iterator it = m_RelayRecords.find(key);
    if (it == m_RelayRecords.end())
    {
        return MC_ERR_NOT_FOUND;
    }
    
    if(msg_type)
    {
        *msg_type=it->second.m_MsgType;        
    }
    
    
    if(pto)
    {
        pto_id=it->second.m_NodeFrom;
    
        if(pto_id)
        {
            LOCK(cs_vNodes);
            BOOST_FOREACH(CNode* pnode, vNodes)
            {
                if(pnode->GetId() == pto_id)
                {
                    *pto=pnode;
                    return MC_ERR_NOERROR;
                }
            }
        }
        else
        {
            return MC_ERR_NOERROR;            
        }
    }
    else
    {
        return MC_ERR_NOERROR;                    
    }
    
    return MC_ERR_NOT_ALLOWED;
}

void mc_RelayManager::PushRelay(CNode*    pto, 
                                uint32_t  msg_type,
                                int64_t   nonce_to_send,
                                int64_t   response_to_nonce,
                                mc_NodeFullAddress *origin_to_relay,
                                mc_NodeFullAddress *destination_to_relay,
                                uint32_t  flags,
                                vector<unsigned char>& payload,
                                uint256   message_hash_to_relay,
                                vector<CScript>&  sigScripts_to_relay,
                                vector<CAddress>& path,
                                CNode*    pfrom, 
                                uint32_t  action)
{
    vector <unsigned char> vOriginAddress;
    vector <unsigned char> vDestinationAddress;
    vector<CScript>  sigScripts;
    vector<unsigned char>vSigScript;
    CScript sigScript;
    uint256 message_hash;
    int64_t nonce;     
    mc_NodeFullAddress origin_to_send;
    mc_NodeFullAddress *origin;
    mc_NodeFullAddress destination_to_send;
    mc_NodeFullAddress *destination;
    
    nonce=nonce_to_send;
    
    if(action & MC_PRA_GENERATE_NONCE)
    {
        GetRandBytes((unsigned char*)&nonce, sizeof(nonce));
    }

    if(action & (MC_PRA_MY_ORIGIN_MC_ADDRESS | MC_PRA_MY_ORIGIN_NT_ADDRESS) )
    {
        InitNodeAddress(&origin_to_send,pto,action);
        origin=&origin_to_send;
    }    
    else
    {
        if(origin_to_relay)
        {
            origin=origin_to_relay;
        }
        else
        {
            origin=&origin_to_send;            
        }
    }
    
    destination=&destination_to_send;
    if(action & MC_PRA_USE_DESTINATION_ADDRESS)
    {
        if(destination_to_relay)
        {
            destination=destination_to_relay;
        }
    }
    
    if(MCP_ANYONE_CAN_CONNECT == 0)
    {
        vOriginAddress.resize(sizeof(CKeyID));
        memcpy(&vOriginAddress[0],&(origin->m_Address),sizeof(CKeyID));    
        if(destination->m_Address != 0)
        {
            vDestinationAddress.resize(sizeof(CKeyID));
            memcpy(&vOriginAddress[0],&(origin->m_Address),sizeof(CKeyID));        
        }
    }    
    
    message_hash=message_hash_to_relay;
    
    if(action & MC_PRA_CALCULATE_MESSAGE_HASH)
    {
        CHashWriter ssHash(SER_GETHASH, 0);
        ssHash << nonce;
        ssHash << msg_type;
        ssHash << vOriginAddress;
        ssHash << origin->m_NetAddresses;
        ssHash << vDestinationAddress;
        ssHash << destination->m_NetAddresses;
        ssHash << response_to_nonce;
        ssHash << flags;
        ssHash << payload;
        message_hash=ssHash.GetHash();        
    }
    
    if( (action & (MC_PRA_SIGN | MC_PRA_SIGN_WITH_HANDSHAKE_ADDRESS)) && (MCP_ANYONE_CAN_CONNECT == 0) )
    {
        CHashWriter ssSig(SER_GETHASH, 0);
        ssSig << message_hash;
        ssSig << vector<unsigned char>((unsigned char*)&response_to_nonce, (unsigned char*)&response_to_nonce+sizeof(response_to_nonce));
        uint256 signed_hash=ssSig.GetHash();
        CKey key;
        CKeyID keyID;
        CPubKey pkey;            

        keyID=origin->m_Address;
        if(action & MC_PRA_SIGN_WITH_HANDSHAKE_ADDRESS)
        {
            if(MCP_ANYONE_CAN_CONNECT == 0)
            {
                keyID=pto->kAddrLocal;
            }
        }                    
        
        if(pwalletMain->GetKey(keyID, key))
        {
            pkey=key.GetPubKey();
            vector<unsigned char> vchSig;
            sigScript.clear();
            if (key.Sign(signed_hash, vchSig))
            {
                vchSig.push_back(0x00);
                sigScript << vchSig;
                sigScript << ToByteVector(pkey);
            }
            else
            {
                LogPrintf("PushRelay(): Internal error: Cannot sign\n");                
            }
        }
        else
        {
            LogPrintf("PushRelay(): Internal error: Cannot find key for signature\n");
        }
        sigScripts.push_back(sigScript);
    }    
    
    for(unsigned int i=0;i<sigScripts_to_relay.size();i++)
    {
        sigScripts.push_back(sigScripts_to_relay[i]);
    }
    
    pto->PushMessage("relay",
                        msg_type,
                        nonce,
                        vOriginAddress,
                        origin->m_NetAddresses,
                        vDestinationAddress,
                        destination->m_NetAddresses,
                        response_to_nonce,
                        flags,
                        payload,
                        message_hash,
                        path,
                        sigScripts);        
    
    SetRelayRecord(pto,NULL,msg_type,nonce);
    SetRelayRecord(pto,pfrom,msg_type,nonce);
}
    
bool mc_RelayManager::ProcessRelay( CNode* pfrom, 
                                    CDataStream& vRecv, 
                                    CValidationState &state, 
                                    uint32_t verify_flags_in)
{
    uint32_t  msg_type_in;
    uint32_t verify_flags;
    int64_t   nonce_received;
    int64_t   response_to_nonce;
    vector<unsigned char> vOriginMCAddressIn;
    vector<unsigned char> vDestinationMCAddressIn;
    vector<CAddress> vOriginNetAddressIn;
    vector<CAddress> vDestinationNetAddressIn;
    vector<unsigned char> vPayloadIn;
    vector<CScript> vSigScripts;
    uint256   message_hash,message_hash_in;
    uint32_t  flags_in;
    CKeyID   origin_mc_address;
    CKeyID   destination_mc_address;
    vector<unsigned char> vchSigOut;
    vector<unsigned char> vchPubKey;
    CPubKey pubkey;
    vector<CAddress> path;    
    mc_NodeFullAddress origin_to_relay;
    mc_NodeFullAddress destination_to_relay;
    vector<unsigned char> vPayloadResponse;
    vector<CScript>  vSigScriptsResponse;
    vector<CAddress> vPathResponse;
    CNode *pto_stored;
    uint32_t msg_type_stored;
    
    msg_type_stored=MC_RMT_NONE;
    pto_stored=NULL;
    
    verify_flags=verify_flags_in;
    vRecv >> msg_type_in;
    switch(msg_type_in)
    {
        case MC_RMT_GLOBAL_PING:
            break;
        case MC_RMT_GLOBAL_PONG:
            verify_flags |= MC_VRA_ORIGIN_MC_ADDRESS | MC_VRA_ORIGIN_NT_ADDRESS | MC_VRA_RESPONSE_TO_NONCE | MC_VRA_MESSAGE_HASH | MC_VRA_SIGNATURE_ORIGIN;
            break;
        case MC_RMT_GLOBAL_REJECT:
        case MC_RMT_GLOBAL_BUSY:
            verify_flags |= MC_VRA_ORIGIN_MC_ADDRESS | MC_VRA_RESPONSE_TO_NONCE | MC_VRA_MESSAGE_HASH | MC_VRA_SIGNATURE_ORIGIN;
            break;
        case MC_RMT_CHUNK_QUERY:
            verify_flags |= MC_VRA_ORIGIN_MC_ADDRESS | MC_VRA_MESSAGE_HASH;
            break;
        case MC_RMT_CHUNK_QUERY_HIT:
            verify_flags |= MC_VRA_ORIGIN_MC_ADDRESS | MC_VRA_ORIGIN_NT_ADDRESS | MC_VRA_DESTINATION_MC_ADDRESS | MC_VRA_RESPONSE_TO_NONCE | MC_VRA_MESSAGE_HASH | 
                            MC_VRA_SIGNATURE_ORIGIN;
        case MC_RMT_CHUNK_REQUEST:
        case MC_RMT_CHUNK_RESPONSE:
            verify_flags |= MC_VRA_ORIGIN_MC_ADDRESS | MC_VRA_ORIGIN_NT_ADDRESS | MC_VRA_DESTINATION_MC_ADDRESS | MC_VRA_RESPONSE_TO_NONCE | MC_VRA_MESSAGE_HASH | 
                            MC_VRA_SIGNATURE_ORIGIN | MC_VRA_SINGLE_HOP;
            break;
        default:
            if(verify_flags & MC_VRA_MESSAGE_TYPE)    
            {
                LogPrintf("ProcessRelay() : Unsupported relay message type %d\n",msg_type_in);     
                return false;
            }
            break;
    }
    
    CheckTime();
    
    vRecv >> nonce_received;
    
    if(verify_flags & MC_VRA_ALREADY_PROCESSED)
    {
        if(GetRelayRecord(NULL,nonce_received,NULL,NULL) == MC_ERR_NOERROR)
        {
            return false;
        }
    }
    
    vRecv >> vOriginMCAddressIn;
    vRecv >> vOriginNetAddressIn;
    vRecv >> vDestinationMCAddressIn;
    vRecv >> vDestinationNetAddressIn;
    vRecv >> response_to_nonce;
    vRecv >> flags_in;
    vRecv >> vPayloadIn;
    vRecv >> message_hash_in;
    vRecv >> path;
    vRecv >> vSigScripts;
    
    
    if(vOriginMCAddressIn.size() == sizeof(uint160))
    {
        origin_mc_address=CKeyID(*(uint160*)&vOriginMCAddressIn[0]);                
    }
    else
    {        
        if(verify_flags & MC_VRA_ORIGIN_MC_ADDRESS)
        {
            if(MCP_ANYONE_CAN_CONNECT == 0)
            {
                return state.DoS(100, error("ProcessRelay() : Bad origin address"),REJECT_INVALID, "bad-origin-address");                                
            }        
        }
    }
    
    if(verify_flags & MC_VRA_ORIGIN_NT_ADDRESS)
    {
        if(vOriginNetAddressIn.size() == 0)
        {
            return state.DoS(100, error("ProcessRelay() : No origin network address"),REJECT_INVALID, "bad-origin-net-address");                            
        }        
    }    
    
    InitNodeAddress(&origin_to_relay,origin_mc_address,vOriginNetAddressIn);
    
    if(vDestinationMCAddressIn.size() == sizeof(uint160))
    {
        destination_mc_address=CKeyID(*(uint160*)&vDestinationMCAddressIn[0]);                
    }
    else
    {        
        if(verify_flags & MC_VRA_DESTINATION_MC_ADDRESS)
        {
            if(MCP_ANYONE_CAN_CONNECT == 0)
            {
                return state.DoS(100, error("ProcessRelay() : Bad destination address"),REJECT_INVALID, "bad-destination-address");                                
            }        
        }
    }
    
    if(verify_flags & MC_VRA_DESTINATION_NT_ADDRESS)
    {
        if(vDestinationNetAddressIn.size() == 0)
        {
            return state.DoS(100, error("ProcessRelay() : No destination network address"),REJECT_INVALID, "bad-destination-net-address");                            
        }        
    }    
    
    InitNodeAddress(&destination_to_relay,destination_mc_address,vDestinationNetAddressIn);
    
    map<uint32_t, mc_Limiter>::iterator itlim_all = m_Limiters.find(MC_RMT_NONE);
    map<uint32_t, mc_Limiter>::iterator itlim_msg = m_Limiters.find(msg_type_in);
    
    if(itlim_all != m_Limiters.end())
    {
        itlim_all->second.SetEvent(1,vRecv.size());
        if( verify_flags & MC_VRA_LIMIT_ALL ) 
        {
            if(itlim_all->second.Disallowed(m_LastTime))
            {
                PushRelay(pfrom,MC_RMT_GLOBAL_BUSY,0,nonce_received,NULL,NULL,0,vPayloadResponse,0,vSigScriptsResponse,vPathResponse,NULL,
                                MC_PRA_GENERATE_NONCE | MC_PRA_MY_ORIGIN_MC_ADDRESS | MC_PRA_CALCULATE_MESSAGE_HASH | MC_PRA_SIGN | MC_PRA_SIGN_WITH_HANDSHAKE_ADDRESS);
                return false;
            }
        }        
    }
    
    if(itlim_msg != m_Limiters.end())
    {
        itlim_msg->second.SetEvent(1,vRecv.size());
        if( verify_flags & MC_VRA_LIMIT_MSG_TYPE ) 
        {
            if(itlim_msg->second.Disallowed(m_LastTime))
            {
                PushRelay(pfrom,MC_RMT_GLOBAL_BUSY,0,nonce_received,NULL,NULL,0,vPayloadResponse,0,vSigScriptsResponse,vPathResponse,NULL,
                                MC_PRA_GENERATE_NONCE | MC_PRA_MY_ORIGIN_MC_ADDRESS | MC_PRA_CALCULATE_MESSAGE_HASH | MC_PRA_SIGN | MC_PRA_SIGN_WITH_HANDSHAKE_ADDRESS);
                return false;
            }
        }        
    }
    
    
    if( verify_flags & MC_VRA_RESPONSE_TO_NONCE ) 
    {
        if(response_to_nonce == 0)
        {
            return state.DoS(100, error("ProcessRelay() : Missing response_to_nonce"),REJECT_INVALID, "bad-nonce");                
        }
        if(GetRelayRecord(pfrom,response_to_nonce,&msg_type_stored,&pto_stored))
        {
            LogPrintf("ProcessRelay() : Response without request from peer %d\n",pfrom->GetId());     
            return false;
        }
    }
    
    if( verify_flags & (MC_VRA_MESSAGE_HASH | MC_VRA_SIGNATURE_ORIGIN | MC_VRA_SIGNATURE_ALL) )
    {
        CHashWriter ssHash(SER_GETHASH, 0);
        ssHash << nonce_received;
        ssHash << msg_type_in;
        ssHash << vOriginMCAddressIn;
        ssHash << vOriginNetAddressIn;
        ssHash << vDestinationMCAddressIn;
        ssHash << vDestinationNetAddressIn;
        ssHash << response_to_nonce;
        ssHash << flags_in;
        ssHash << vPayloadIn;
        message_hash=ssHash.GetHash();                
        if(message_hash != message_hash_in)
        {
            return state.DoS(100, error("ProcessRelay() : Payload hash mismatch"),REJECT_INVALID, "bad-payload-hash");                            
        }
    }    
    
    message_hash=message_hash_in;
    
    if( verify_flags & MC_VRA_SIGNATURE_ORIGIN )
    {
        if(vSigScripts.size() < 1)
        {
            return state.DoS(100, error("ProcessRelay() : Missing sigScript"),REJECT_INVALID, "bad-sigscript");                            
        }
        if(vSigScripts[0].size())
        {
            CScript scriptSig=vSigScripts[0];

            opcodetype opcode;

            CScript::const_iterator pc = scriptSig.begin();

            if (!scriptSig.GetOp(pc, opcode, vchSigOut))
            {
                return state.DoS(100, error("ProcessRelay() : Cannot extract signature from sigScript"),REJECT_INVALID, "bad-sigscript-signature");                
            }

            vchSigOut.resize(vchSigOut.size()-1);
            if (!scriptSig.GetOp(pc, opcode, vchPubKey))
            {
                return state.DoS(100, error("ProcessRelay() : Cannot extract pubkey from sigScript"),REJECT_INVALID, "bad-sigscript-pubkey");                
            }

            CPubKey pubKeyOut(vchPubKey);
            if (!pubKeyOut.IsValid())
            {
                return state.DoS(100, error("ProcessRelay() : Invalid pubkey"),REJECT_INVALID, "bad-sigscript-pubkey");                
            }        

            pubkey=pubKeyOut;
            
            CHashWriter ss(SER_GETHASH, 0);
            ss << vector<unsigned char>((unsigned char*)&message_hash, (unsigned char*)&message_hash+32);
            ss << vector<unsigned char>((unsigned char*)&response_to_nonce, (unsigned char*)&response_to_nonce+sizeof(response_to_nonce));
            uint256 signed_hash=ss.GetHash();

            if(!pubkey.Verify(signed_hash,vchSigOut))
            {
                return state.DoS(100, error("ProcessRelay() : Wrong signature"),REJECT_INVALID, "bad-signature");                            
            }        
        }
        else
        {
            return state.DoS(100, error("ProcessRelay() : Empty sigScript"),REJECT_INVALID, "bad-sigscript");                
        }
    }
    
    if(itlim_all != m_Limiters.end())
    {
        itlim_all->second.Increment();
    }
    
    if(itlim_msg != m_Limiters.end())
    {
        itlim_msg->second.Increment();        
    }    
    
    return true;
}

/*
bool ProcessMultichainRelay(CNode* pfrom, CDataStream& vRecv, CValidationState &state, uint32_t verify_flags_in)
{
    uint32_t  msg_type_in;
    uint32_t verify_flags;
    int64_t   nonce_received;
    int64_t   response_to_nonce;
    vector<unsigned char> vOriginMCAddressIn;
    vector<unsigned char> vDestinationMCAddressIn;
    vector<CAddress> vOriginNetAddressIn;
    vector<CAddress> vDestinationNetAddressIn;
    vector<unsigned char> vPayloadIn;
    vector<unsigned char> vSigScript;
    uint256   payload_hash,payload_hash_in;
    uint32_t  flags_in;
    CKeyID   origin_mc_address;
    CKeyID   destination_mc_address;
    bool destination_cannot_be_empty;
    vector<unsigned char> vchSigOut;
    vector<unsigned char> vchPubKey;
    CPubKey pubkey;
    
    vRecv >> msg_type_in;
    
    vRecv >> nonce_received;
    vRecv >> vOriginMCAddressIn;
    vRecv >> vOriginNetAddressIn;
    vRecv >> vDestinationMCAddressIn;
    vRecv >> vDestinationNetAddressIn;
    vRecv >> flags_in;
    vRecv >> response_to_nonce;
    vRecv >> payload_hash_in;
    vRecv >> vSigScript;
    vRecv >> vPayloadIn;

    if(verify_flags & MC_VRA_MESSAGE_TYPE)    
    {
        switch(msg_type_in)
        {
            case MC_RMT_CHUNK_QUERY:
            case MC_RMT_CHUNK_QUERY_HIT:
            case MC_RMT_CHUNK_REQUEST:
            case MC_RMT_CHUNK_RESPONSE:
                break;
            default:
                LogPrintf("ProcessMultichainRelay() : Unsupported relay message type %d\n",msg_type_in);     
                return false;
        }
    }
    
    if(verify_flags & MC_VRA_ORIGIN_ADDRESS)
    {
        if(vOriginNetAddressIn.size() == 0)
        {
            return state.DoS(100, error("ProcessMultichainRelay() : No origin network address"),REJECT_INVALID, "bad-origin-net-address");                            
        }
        if(MCP_ANYONE_CAN_CONNECT == 0)
        {
            if(vOriginMCAddressIn.size() != sizeof(uint160))
            {
                return state.DoS(100, error("ProcessMultichainRelay() : Bad origin address"),REJECT_INVALID, "bad-origin-address");                
            }
            origin_mc_address=CKeyID(*(uint160*)&vOriginMCAddressIn[0]);
        }
    }
    
    if(verify_flags & MC_VRA_DESTINATION_ADDRESS)
    {
        destination_cannot_be_empty=false;
        switch(msg_type_in)
        {
            case MC_RMT_CHUNK_QUERY_HIT:
            case MC_RMT_CHUNK_RESPONSE:
                destination_cannot_be_empty=true;
                break;
        }        
        if(destination_cannot_be_empty)
        {
            if( ((vDestinationMCAddressIn.size()) == 0) &&  (MCP_ANYONE_CAN_CONNECT == 0) )
            {
                return state.DoS(100, error("ProcessMultichainRelay() : Empty destination address"),REJECT_INVALID, "bad-destination-address");                                
            }
            if(vOriginNetAddressIn.size() == 0)
            {
                return state.DoS(100, error("ProcessMultichainRelay() : No destination network address"),REJECT_INVALID, "bad-destination-net-address");                            
            }            
        }
        if(vDestinationMCAddressIn.size())
        {
            if(MCP_ANYONE_CAN_CONNECT == 0)
            {
                if(vDestinationMCAddressIn.size() != sizeof(uint160))
                {
                    return state.DoS(100, error("ProcessMultichainRelay() : Bad destination address"),REJECT_INVALID, "bad-destination-address");                
                }
                destination_mc_address=CKeyID(*(uint160*)&vDestinationMCAddressIn[0]);
            }        
        }        
    }

    if( verify_flags & MC_VRA_SIGSCRIPT)
    {
        if(vSigScript.size())
        {
            CScript scriptSig((unsigned char*)&vSigScript[0],(unsigned char*)&vSigScript[0]+vSigScript.size());

            opcodetype opcode;

            CScript::const_iterator pc = scriptSig.begin();

            if (!scriptSig.GetOp(pc, opcode, vchSigOut))
            {
                return state.DoS(100, error("ProcessMultichainRelay() : Cannot extract signature from sigScript"),REJECT_INVALID, "bad-sigscript-signature");                
            }

            vchSigOut.resize(vchSigOut.size()-1);
            if (!scriptSig.GetOp(pc, opcode, vchPubKey))
            {
                return state.DoS(100, error("ProcessMultichainRelay() : Cannot extract pubkey from sigScript"),REJECT_INVALID, "bad-sigscript-pubkey");                
            }

            CPubKey pubKeyOut(vchPubKey);
            if (!pubKeyOut.IsValid())
            {
                return state.DoS(100, error("ProcessMultichainRelay() : Invalid pubkey"),REJECT_INVALID, "bad-sigscript-pubkey");                
            }        

            pubkey=pubKeyOut;
        }
        else
        {
            switch(msg_type_in)
            {
                case MC_RMT_CHUNK_QUERY:
                    break;
                default:
                    return state.DoS(100, error("ProcessMultichainRelay() : Missing sigScript"),REJECT_INVALID, "bad-sigscript");                
            }            
        }
    }
    
    if( verify_flags & MC_VRA_RESPONSE_TO_NONCE ) 
    {
        if(response_to_nonce == 0)
        {
            switch(msg_type_in)
            {
                case MC_RMT_CHUNK_QUERY:
                    break;
                default:
                    return state.DoS(100, error("ProcessMultichainRelay() : Missing response_to_nonce"),REJECT_INVALID, "bad-nonce");                
            }            
        }
    }
    
    if( verify_flags & (MC_VRA_PAYLOAD_HASH | MC_VRA_SIGNATURE) )
    {
        CHashWriter ssHash(SER_GETHASH, 0);
        ssHash << vPayloadIn;
        payload_hash=ssHash.GetHash();        
        
        if(payload_hash != payload_hash_in)
        {
            return state.DoS(100, error("ProcessMultichainRelay() : Bad payload hash"),REJECT_INVALID, "bad-payload-hash");                            
        }
    }
    
    if( verify_flags & MC_VRA_SIGNATURE )
    {
        if(vSigScript.size())
        {
            CHashWriter ss(SER_GETHASH, 0);
            ss << vector<unsigned char>((unsigned char*)&payload_hash, (unsigned char*)&payload_hash+32);
            ss << vector<unsigned char>((unsigned char*)&response_to_nonce, (unsigned char*)&response_to_nonce+sizeof(response_to_nonce));
            uint256 signed_hash=ss.GetHash();

            if(!pubkey.Verify(signed_hash,vchSigOut))
            {
                return state.DoS(100, error("ProcessMultichainRelay() : Wrong signature"),REJECT_INVALID, "bad-signature");                            
            }        
        }        
    }    
    
    return true;
}
*/
void PushMultiChainRelay(CNode* pto, uint32_t msg_type,vector<CAddress>& path,vector<CAddress>& path_to_follow,vector<unsigned char>& payload)
{
    pto->PushMessage("relay", msg_type, path, path_to_follow, payload);
}


void PushMultiChainRelay(CNode* pto, uint32_t msg_type,vector<CAddress>& path,vector<CAddress>& path_to_follow,unsigned char *payload,size_t size)
{
    vector<unsigned char> vPayload= vector<unsigned char>((unsigned char *)payload,(unsigned char *)payload+size);    
    PushMultiChainRelay(pto,msg_type,path,path_to_follow,vPayload);
}

bool MultichainRelayResponse(uint32_t msg_type_in,vector<CAddress>& path,vector<unsigned char>& vPayloadIn,
                             uint32_t* msg_type_response,vector<unsigned char>& vPayloadResponse,
                             uint32_t* msg_type_relay,vector<unsigned char>& vPayloadRelay)
{
    switch(msg_type_in)
    {
        case MC_RMT_GLOBAL_PING:
            *msg_type_response=MC_RMT_GLOBAL_PONG;
            vPayloadResponse=vPayloadIn;
            if(path.size() < 4)
            {
                *msg_type_relay=MC_RMT_GLOBAL_PING;
                vPayloadRelay=vPayloadIn;
            }
            break;
        case MC_RMT_GLOBAL_PONG:
            string str_path="";
            for(int i=(int)path.size()-1;i>=0;i--)
            {
                str_path+="->" + path[i].ToString();
            }
            LogPrintf("PONG : %s\n",str_path.c_str());
            break;
    }
    return true;
}

bool ProcessMultichainRelay(CNode* pfrom, CDataStream& vRecv, CValidationState &state)
{
    uint32_t msg_type_in,msg_type_response,msg_type_relay;    
    size_t common_size;
    CNode* pto;
    
    vector<CAddress> path;
    vector<CAddress> path_to_follow;
    vector<CAddress> path_response;
    vector<CAddress> path_to_follow_response;    
    vector<unsigned char> vPayloadIn;
    vector<unsigned char> vPayloadResponse;
    vector<unsigned char> vPayloadRelay;
    
    vRecv >> msg_type_in;
    vRecv >> path;
    vRecv >> path_to_follow;
    vRecv >> vPayloadIn;
    
    msg_type_response=MC_RMT_NONE;
    msg_type_relay=MC_RMT_NONE;
    pto=NULL;

    common_size=path_to_follow.size();
    if(common_size > path.size())
    {
        common_size=path.size();
        if(pfrom->addrLocal != (CService)path_to_follow[common_size])
        {
            return state.DoS(100, error("ProcessMultichainRelay() : Bad path"),REJECT_INVALID, "bad-relay-path");
        }
    }
    
    path.push_back(pfrom->addr);
    common_size++;
    
    if(common_size < path_to_follow.size())
    {
        LOCK(cs_vNodes);
        BOOST_FOREACH(CNode* pnode, vNodes)
        {
            if(pnode->addr == path_to_follow[common_size])
            {
                pto=pnode;
            }
        }
        if(pto == NULL)
        {
            LogPrintf("Request to relay to unconnected peer %s\n",path_to_follow[common_size].ToString().c_str());     
            return false;
        }
    }
    
    if(pto)
    {
        PushMultiChainRelay(pto, msg_type_in,path,path_to_follow,vPayloadIn);
        return true;
    }
    
    if(pto == NULL)
    {
        if(MultichainRelayResponse(msg_type_in,path,vPayloadIn,&msg_type_response,vPayloadResponse,&msg_type_relay,vPayloadRelay))
        {
            if(msg_type_relay)
            {
                BOOST_FOREACH(CNode* pnode, vNodes)
                {
                    int path_index=-1;
                    for(unsigned int i=0;i<path.size()-1;i++)
                    {
                        if(path_index < 0)
                        {
                            if(pnode->addr == path[i]) 
                            {
                                path_index=i;
                            }
                        }
                    }                    
                    if(path_index < 0)
                    {
                        PushMultiChainRelay(pnode, msg_type_relay,path,path_to_follow,vPayloadRelay);                        
                    }
                }
            }
            if(msg_type_response)
            {
                for(int i=(int)path.size()-1;i>=0;i--)
                {
                    path_to_follow_response.push_back(path[i]);
                    PushMultiChainRelay(pfrom, msg_type_response,path_response,path_to_follow_response,vPayloadResponse);                        
                }
            }
        }       
        else
        {
            return false;
        }
    }
    
    return true;
}


