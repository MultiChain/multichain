// Copyright (c) 2014-2017 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "protocol/relay.h"
#include "structs/base58.h"

bool mc_RelayProcess_Address_Query(unsigned char *ptrStart,unsigned char *ptrEnd,vector<unsigned char>* payload_response,vector<unsigned char>* payload_relay,string& strError)
{
    unsigned char *ptr;
    unsigned char *ptrOut;
    unsigned char buf[16];
    int shift;
    CKey key;
    
    ptr=ptrStart;
    while(ptr<ptrEnd)
    {
        switch(*ptr)
        {
            case MC_RDT_MC_ADDRESS:
                ptr++;
                if(sizeof(CKeyID) != (ptrEnd-ptr))
                {
                    strError="Bad address query request";
                    return false;
                }

                if(pwalletMain->GetKey(*(CKeyID*)ptr, key))
                {
                    if(payload_response)
                    {
                        shift=mc_PutVarInt(buf,16,pRelayManager->m_MyAddress.m_NetAddresses.size());
                        payload_response->resize(1+sizeof(CKeyID)+1+shift+sizeof(CAddress)*pRelayManager->m_MyAddress.m_NetAddresses.size());
                        ptrOut=&(*payload_response)[0];
                        
                        *ptrOut=MC_RDT_MC_ADDRESS;
                        ptrOut++;
                        *(CKeyID*)ptrOut=pRelayManager->m_MyAddress.m_Address;
                        ptrOut+=sizeof(CKeyID);
                        
                        *ptrOut=MC_RDT_NET_ADDRESS;
                        ptrOut++;
                        memcpy(ptrOut,buf,shift);
                        ptrOut+=shift;
                        for(int i=0;i<(int)pRelayManager->m_MyAddress.m_NetAddresses.size();i++)
                        {
                            memcpy(ptrOut,&(pRelayManager->m_MyAddress.m_NetAddresses[i]),sizeof(CAddress));
                            ptrOut+=sizeof(CAddress);
                        }                        
                    }
                }
                else
                {
                    if(payload_relay)
                    {
                        payload_relay->resize(ptrEnd-ptrStart);
                        memcpy(&(*payload_relay)[0],ptrStart,ptrEnd-ptrStart);                    
                    }
                }
                
                ptr+=sizeof(CKeyID);
                break;
            default:
                strError=strprintf("Unsupported request format (%d, %d)",MC_RMT_MC_ADDRESS_QUERY,*ptr);
                return false;
        }
    }
    
    return true;
}

bool MultichainRelayResponse(uint32_t msg_type_stored, CNode *pto_stored,
                             uint32_t msg_type_in, uint32_t  flags, vector<unsigned char>& vPayloadIn,vector<CKeyID>&  vAddrIn,
                             uint32_t* msg_type_response,uint32_t  *flags_response,vector<unsigned char>& vPayloadResponse,vector<CKeyID>&  vAddrResponse,
                             uint32_t* msg_type_relay,uint32_t  *flags_relay,vector<unsigned char>& vPayloadRelay,vector<CKeyID>&  vAddrRelay,string& strError)
{
    unsigned char *ptr;
    unsigned char *ptrEnd;
    vector<unsigned char> *payload_relay_ptr=NULL;
    vector<unsigned char> *payload_response_ptr=NULL;
    
    if(msg_type_response)
    {
        payload_response_ptr=&vPayloadResponse;
    }
    
    if(msg_type_relay)
    {
        payload_relay_ptr=&vPayloadRelay;
    }
    
    ptr=&vPayloadIn[0];
    ptrEnd=ptr+vPayloadIn.size();
            
    strError="";
    switch(msg_type_in)
    {
        case MC_RMT_MC_ADDRESS_QUERY:
            if(msg_type_stored)
            {
                strError=strprintf("Unexpected response message type (%d,%d)",msg_type_stored,msg_type_in);;
                goto exitlbl;
            } 
            if(payload_response_ptr == NULL)
            {
                if(payload_relay_ptr)
                {
                    vPayloadRelay=vPayloadIn;
                    *msg_type_relay=msg_type_in;                    
                }
            }
            else
            {
                if(mc_RelayProcess_Address_Query(ptr,ptrEnd,payload_response_ptr,payload_relay_ptr,strError))
                {
                    if(payload_response_ptr && (payload_response_ptr->size() != 0))
                    {
                        *msg_type_response=MC_RMT_NODE_DETAILS;
                    }
                    if(payload_relay_ptr && (payload_relay_ptr->size() != 0))
                    {
                        *msg_type_relay=MC_RMT_MC_ADDRESS_QUERY;
                    }
                }
            }
            break;
        case MC_RMT_NODE_DETAILS:
            if(msg_type_stored != MC_RMT_MC_ADDRESS_QUERY)
            {
                strError=strprintf("Unexpected response message type (%d,%d)",msg_type_stored,msg_type_in);;
                goto exitlbl;
            } 
            if(payload_response_ptr)
            {
                *msg_type_response=MC_RMT_ADD_RESPONSE;
            }

            break;
    }
    
exitlbl:
            
    if(strError.size())
    {
        return false;
    }

    return true;
}

void mc_Limiter::Zero()
{
    memset(this,0,sizeof(mc_Limiter));    
}

int mc_Limiter::Initialize(int seconds,int measures)
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
    CKey key;
    CPubKey pkey;            
    bool key_found=false;
    
    if(action & MC_PRA_MY_ORIGIN_MC_ADDRESS)
    {
        if(pto)
        {
            node_address->m_Address=pto->kAddrLocal;        
        }
    }
    else
    {
        if(pto == NULL)
        {
            if(mapArgs.count("-handshakelocal"))
            {
                CBitcoinAddress address(mapArgs["-handshakelocal"]);
                if (address.IsValid())    
                {
                    CTxDestination dst=address.Get();
                    CKeyID *lpKeyID=boost::get<CKeyID> (&dst);
                    if(lpKeyID)
                    {
                        if(pwalletMain->GetKey(*lpKeyID, key))
                        {
                            node_address->m_Address=*lpKeyID;
                            key_found=true;
                        }
                    }
                }        
            }

            if(!key_found)
            {
                if(!pwalletMain->GetKeyFromAddressBook(pkey,MC_PTP_CONNECT))
                {
                    pkey=pwalletMain->vchDefaultKey;
                }
                node_address->m_Address=pkey.GetID();
            }  
        }
    }
    
    node_address->m_NetAddresses.clear();
    
    pto_address_local=0;
    if(action & MC_PRA_MY_ORIGIN_NT_ADDRESS)
    {
        node_address->m_NetAddresses.push_back(CAddress(pto->addrLocal));

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
            addr.s_addr=htonl(m_MyIPs[i]);
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
    
    limiter.Initialize(seconds,2);
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

int64_t mc_RelayManager::AggregateNonce(uint32_t timestamp,uint32_t nonce)
{
    return ((int64_t)nonce<<32)+(int64_t)timestamp;
}

void mc_RelayManager::Zero()
{
    m_Semaphore=NULL;
    m_LockedBy=0;         
}

void mc_RelayManager::Destroy()
{
    if(m_Semaphore)
    {
        __US_SemDestroy(m_Semaphore);
    }
    
    Zero();    
}

int mc_RelayManager::Initialize()
{
    m_Semaphore=__US_SemCreate();
    InitNodeAddress(&m_MyAddress,NULL,MC_PRA_NONE);
    SetDefaults();
    return MC_ERR_NOERROR;    
}

int mc_RelayManager::Lock(int write_mode,int allow_secondary)
{        
    uint64_t this_thread;
    this_thread=__US_ThreadID();
    
    if(this_thread == m_LockedBy)
    {
        return allow_secondary;
    }
    
    __US_SemWait(m_Semaphore); 
    m_LockedBy=this_thread;
    
    return 0;
}

void mc_RelayManager::UnLock()
{    
    m_LockedBy=0;
    __US_SemPost(m_Semaphore);
}

int mc_RelayManager::Lock()
{        
    return Lock(1,0);
}

void mc_RelayManager::SetDefaults()
{
    MsgTypeSettings(MC_RMT_NONE            , 0,10,1000,100*1024*1024);
    MsgTypeSettings(MC_RMT_MC_ADDRESS_QUERY,10,10, 100,  1*1024*1024);
    MsgTypeSettings(MC_RMT_NODE_DETAILS    , 0,10, 100,  1*1024*1024);
    MsgTypeSettings(MC_RMT_REJECT          , 0,10,1000,  1*1024*1024);
    MsgTypeSettings(MC_RMT_CHUNK_QUERY     ,10,10, 100,  1*1024*1024);
    MsgTypeSettings(MC_RMT_CHUNK_QUERY_HIT ,30,10, 100,  1*1024*1024);
    MsgTypeSettings(MC_RMT_CHUNK_REQUEST   ,30,10, 100,  1*1024*1024);
    MsgTypeSettings(MC_RMT_CHUNK_RESPONSE  , 0,10, 100,100*1024*1024);
    MsgTypeSettings(MC_RMT_ERROR_IN_MESSAGE,30,10,1000,  1*1024*1024);
    MsgTypeSettings(MC_RMT_NEW_REQUEST     ,30,10,1000,  1*1024*1024);
    
    
    m_MinTimeShift=180;
    m_MaxTimeShift=180;
    m_MaxResponses=16;
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
    m_LastTime=time_now;
}

void mc_RelayManager::SetRelayRecord(CNode *pto,CNode *pfrom,uint32_t msg_type,uint32_t timestamp,uint32_t nonce)
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
    const mc_RelayRecordKey key=mc_RelayRecordKey(timestamp,nonce,pto_id);
    mc_RelayRecordValue value;
    value.m_NodeFrom=0;
    if(pfrom)
    {
        value.m_NodeFrom=pfrom->GetId();
    }
    value.m_MsgType=msg_type;
    value.m_Timestamp=m_LastTime+itlat->second;
    value.m_Count=1;
    
    map<const mc_RelayRecordKey, mc_RelayRecordValue>::iterator it = m_RelayRecords.find(key);
    if (it == m_RelayRecords.end())
    {
        m_RelayRecords.insert(make_pair(key,value));
    }                    
    else
    {
        value.m_Timestamp=(it->second).m_Timestamp;
        value.m_Count=(it->second).m_Count;
        it->second=value;
    }    
    printf("setrr: %d, ts: %u, nc: %u, mt: %d\n",pto_id,value.m_Timestamp,nonce,msg_type);
}

int mc_RelayManager::GetRelayRecord(CNode *pfrom,uint32_t timestamp,uint32_t nonce,uint32_t* msg_type,CNode **pto)
{
    NodeId pfrom_id,pto_id;
    
    pfrom_id=0;
    if(pfrom)
    {
        pfrom_id=pfrom->GetId();
    }
    printf("getrr: %d, ts: %u, nc: %u\n",pfrom_id,timestamp,nonce);
    const mc_RelayRecordKey key=mc_RelayRecordKey(timestamp,nonce,pfrom_id);
    map<mc_RelayRecordKey, mc_RelayRecordValue>::iterator it = m_RelayRecords.find(key);
    if (it == m_RelayRecords.end())
    {
        return MC_ERR_NOT_FOUND;
    }
    
    if(it->second.m_MsgType == MC_RMT_ERROR_IN_MESSAGE)
    {
        return MC_ERR_ERROR_IN_SCRIPT;
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
        it->second.m_Count+=1;
        if(it->second.m_Count > m_MaxResponses)
        {
            return MC_ERR_NOT_ALLOWED;            
        }
        return MC_ERR_NOERROR;                    
    }
    
    return MC_ERR_NOT_ALLOWED;
}

uint32_t mc_RelayManager::GenerateNonce()
{
    uint32_t nonce;     
    
    GetRandBytes((unsigned char*)&nonce, sizeof(nonce));
    
    return nonce;
}

int64_t mc_RelayManager::PushRelay(CNode*    pto, 
                                uint32_t  msg_format,        
                                vector <int32_t> &vHops,
                                uint32_t  msg_type,
                                uint32_t  timestamp_to_send,
                                uint32_t  nonce_to_send,
                                uint32_t  timestamp_to_respond,
                                uint32_t  nonce_to_respond,
                                uint32_t  flags,
                                vector<unsigned char>& payload,
                                vector<CScript>&  sigScripts_to_relay,
                                CNode*    pfrom, 
                                uint32_t  action)
{
    vector <unsigned char> vOriginAddress;
    vector <unsigned char> vDestinationAddress;    
    vector<CScript>  sigScripts;
    CScript sigScript;
    uint256 message_hash;
    uint32_t timestamp;     
    uint32_t nonce;     
    
    nonce=nonce_to_send;
    timestamp=timestamp_to_send;
    
    if(action & MC_PRA_GENERATE_TIMESTAMP)
    {
        timestamp=mc_TimeNowAsUInt();
    }
    
    if(action & MC_PRA_GENERATE_NONCE)
    {
        nonce=GenerateNonce();
    }
    
    int64_t aggr_nonce=AggregateNonce(timestamp,nonce);
    
    if( (action & MC_PRA_SIGN_WITH_HANDSHAKE_ADDRESS) && (MCP_ANYONE_CAN_CONNECT == 0) )
    {
        CHashWriter ssHash(SER_GETHASH, 0);
        ssHash << msg_type;        
        ssHash << timestamp;
        ssHash << nonce;
        ssHash << timestamp_to_respond;
        ssHash << nonce_to_respond;
        ssHash << flags;
        ssHash << payload;
        
        message_hash=ssHash.GetHash();    
        
        CHashWriter ssSig(SER_GETHASH, 0);
        
        ssSig << message_hash;
        ssSig << vector<unsigned char>((unsigned char*)&aggr_nonce, (unsigned char*)&aggr_nonce+sizeof(aggr_nonce));
        uint256 signed_hash=ssSig.GetHash();
        CKey key;
        CKeyID keyID;
        CPubKey pkey;            

        keyID=pto->kAddrLocal;
        
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
    
    if(pfrom)
    {
        vHops.push_back((int32_t)pfrom->GetId());
    }
    
    printf("send: %d, to: %d, from: %d, hc: %d, size: %d, ts: %u, nc: %u\n",msg_type,pto->GetId(),pfrom ? pfrom->GetId() : 0,(int)vHops.size(),(int)payload.size(),timestamp,nonce);
    pto->PushMessage("offchain",
                        msg_format,
                        vHops,
                        msg_type,
                        timestamp,
                        nonce,
                        timestamp_to_respond,
                        nonce_to_respond,
                        flags,
                        payload,
                        sigScripts);        
    
//    SetRelayRecord(pto,NULL,msg_type,timestamp,nonce);
    SetRelayRecord(pto,pfrom,msg_type,timestamp,nonce);
    
    return aggr_nonce;
}
    
bool mc_RelayManager::ProcessRelay( CNode* pfrom, 
                                    CDataStream& vRecv, 
                                    CValidationState &state, 
                                    uint32_t verify_flags_in)
{
    uint32_t  msg_type_in;
    uint32_t verify_flags;
    uint32_t   timestamp_received;
    uint32_t   nonce_received;
    uint32_t   timestamp_to_respond;
    uint32_t   nonce_to_respond;
    vector<unsigned char> vPayloadIn;
    vector<CScript> vSigScripts;
    vector<CScript> vSigScriptsEmpty;
    vector <int32_t> vHops;
    vector <int32_t> vEmptyHops;
    uint256   message_hash;
    uint32_t  flags_in,flags_response,flags_relay;
    vector<unsigned char> vchSigOut;
    vector<unsigned char> vchPubKey;
    CPubKey pubkey;
    vector<CAddress> path;    
    CNode *pto_stored;
    uint32_t msg_type_stored;
    uint32_t msg_format;
    unsigned char hop_count;
    uint32_t msg_type_response,msg_type_relay;
    uint32_t *msg_type_relay_ptr;
    uint32_t *msg_type_response_ptr;
    vector<unsigned char> vPayloadResponse;
    vector<unsigned char> vPayloadRelay;
    vector<CKeyID>  vAddrIn;    
    vector<CKeyID>  vAddrResponse;    
    vector<CKeyID>  vAddrRelay;    
    string strError;    
    
    msg_type_stored=MC_RMT_NONE;
    msg_type_response=MC_RMT_NONE;
    msg_type_relay=MC_RMT_NONE;
    pto_stored=NULL;
    
    verify_flags=verify_flags_in;
    vRecv >> msg_format;
    if(msg_format != 0)
    {
        LogPrintf("ProcessRelay() : Unsupported message format %08X\n",msg_format);     
        return false;        
    }
        
    vRecv >> vHops;
    hop_count=(int)vHops.size();
    vRecv >> msg_type_in;
    switch(msg_type_in)
    {
        case MC_RMT_MC_ADDRESS_QUERY:
            verify_flags |= MC_VRA_IS_NOT_RESPONSE;
            break;
        case MC_RMT_NODE_DETAILS:
            verify_flags |= MC_VRA_IS_RESPONSE | MC_VRA_SIGNATURE_ORIGIN;
            break;
        case MC_RMT_REJECT:
            verify_flags |= MC_VRA_IS_RESPONSE | MC_VRA_SIGNATURE_ORIGIN;
            break;
        case MC_RMT_CHUNK_QUERY:
            verify_flags |= MC_VRA_IS_NOT_RESPONSE;
            break;
        case MC_RMT_CHUNK_QUERY_HIT:
            verify_flags |= MC_VRA_IS_RESPONSE | MC_VRA_SIGNATURE_ORIGIN | MC_VRA_PROCESS_ONCE;
            break;            
        case MC_RMT_CHUNK_REQUEST:
            verify_flags |= MC_VRA_IS_RESPONSE | MC_VRA_DOUBLE_HOP | MC_VRA_SIGNATURE_ORIGIN;
            break;
        case MC_RMT_CHUNK_RESPONSE:
            verify_flags |= MC_VRA_IS_RESPONSE | MC_VRA_DOUBLE_HOP | MC_VRA_SIGNATURE_ORIGIN | MC_VRA_PROCESS_ONCE;
            break;
        default:
            if(verify_flags & MC_VRA_MESSAGE_TYPE)    
            {
                LogPrintf("ProcessRelay() : Unsupported relay message type %d\n",msg_type_in);     
                return false;
            }
            break;
    }
    
    if(verify_flags & MC_VRA_SINGLE_HOP)
    {
        if(hop_count)
        {
            LogPrintf("ProcessRelay() : Unsupported hop count %d for msg type %d\n",hop_count,msg_type_in);     
            return false;            
        }
    }
    
    if(verify_flags & MC_VRA_DOUBLE_HOP)
    {
        if(hop_count > 1)
        {
            LogPrintf("ProcessRelay() : Unsupported hop count %d for msg type %d\n",hop_count,msg_type_in);     
            return false;            
        }
    }
        
    CheckTime();
        
    vRecv >> timestamp_received;
    
    if(verify_flags & MC_VRA_TIMESTAMP)
    {
        if(timestamp_received+m_MinTimeShift < m_LastTime)
        {
            LogPrintf("ProcessRelay() : Timestamp too far in the past: %d\n",timestamp_received);     
            return false;                        
        }
        if(timestamp_received > m_LastTime + m_MaxTimeShift)
        {
            LogPrintf("ProcessRelay() : Timestamp too far in the future: %d\n",timestamp_received);     
            return false;                        
        }
    }    
    
    vRecv >> nonce_received;    
    
    
    msg_type_relay_ptr=&msg_type_relay;
    msg_type_response_ptr=&msg_type_response;
    
    if( verify_flags & (MC_VRA_PROCESS_ONCE | MC_VRA_BROADCAST_ONCE))
    {
        switch(GetRelayRecord(NULL,timestamp_received,nonce_received,NULL,NULL))
        {
            case MC_ERR_NOERROR:
                if(verify_flags & MC_VRA_PROCESS_ONCE)
                {
                    return false;
                }
                if(verify_flags & MC_VRA_BROADCAST_ONCE)
                {
                    msg_type_relay_ptr=NULL;
                }
                break;
            case MC_ERR_ERROR_IN_SCRIPT:                                        // We already processed this message, it has errors
                return false;                                        
            case MC_ERR_NOT_ALLOWED:
                LogPrintf("ProcessRelay() : Processing this message is not allowed by current limits or requesting peer was disconnected\n");     
                return false;                                        
        }
    }
    
    if(verify_flags & MC_VRA_DOUBLE_HOP)
    {
        if(hop_count)
        {
            msg_type_relay_ptr=NULL;
        }
    }
    
        
    vRecv >> timestamp_to_respond;
    vRecv >> nonce_to_respond;
    
    if( verify_flags & MC_VRA_IS_NOT_RESPONSE ) 
    {
        if( (timestamp_to_respond != 0) || (nonce_to_respond != 0) )
        {
            return state.DoS(100, error("ProcessRelay() : This message should not be response"),REJECT_INVALID, "bad-nonce");                
        }
    }
    
    int64_t aggr_nonce=AggregateNonce(timestamp_to_respond,nonce_to_respond);
    
    if( verify_flags & MC_VRA_IS_RESPONSE ) 
    {
        if( timestamp_to_respond == 0 )
        {
            return state.DoS(100, error("ProcessRelay() : This message should be response"),REJECT_INVALID, "bad-nonce");                
        }
        
        if(GetRelayRecord(pfrom,timestamp_to_respond,nonce_to_respond,&msg_type_stored,&pto_stored))
        {
            LogPrintf("ProcessRelay() : Response without request from peer %d\n",pfrom->GetId());     
            return false;
        }
    }
    
    SetRelayRecord(NULL,pfrom,MC_RMT_ERROR_IN_MESSAGE,timestamp_received,nonce_received);
    
    if(timestamp_to_respond)
    {
        if(pto_stored)
        {
            msg_type_response_ptr=NULL;        
        }
        else
        {
            msg_type_relay_ptr=NULL;
        }
    }
    
    map<uint32_t, mc_Limiter>::iterator itlim_all = m_Limiters.find(MC_RMT_NONE);
    map<uint32_t, mc_Limiter>::iterator itlim_msg = m_Limiters.find(msg_type_in);
    
    if(itlim_all != m_Limiters.end())
    {
        itlim_all->second.SetEvent(1,vRecv.size());
        if( verify_flags & MC_VRA_LIMIT_ALL ) 
        {
            if(itlim_all->second.Disallowed(m_LastTime))
            {
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
                return false;
            }
        }        
    }
    
    vRecv >> flags_in;
    vRecv >> vPayloadIn;
    vRecv >> vSigScripts;
            
    printf("recv: %d, from: %d, to: %d, hc: %d, size: %d, ts: %u, nc: %u\n",msg_type_in,pfrom->GetId(),pto_stored ? pto_stored->GetId() : 0,hop_count,(int)vPayloadIn.size(),timestamp_received,nonce_received);
    
    if( verify_flags & MC_VRA_SIGNATURES )
    {
        CHashWriter ssHash(SER_GETHASH, 0);
        ssHash << msg_type_in;        
        ssHash << timestamp_received;
        ssHash << nonce_received;
        ssHash << timestamp_to_respond;
        ssHash << nonce_to_respond;
        ssHash << flags_in;
        ssHash << vPayloadIn;
        
        message_hash=ssHash.GetHash();                
    
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
                ss << vector<unsigned char>((unsigned char*)&aggr_nonce, (unsigned char*)&aggr_nonce+sizeof(aggr_nonce));
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
    }
    
    if(itlim_all != m_Limiters.end())
    {
        itlim_all->second.Increment();
    }
    
    if(itlim_msg != m_Limiters.end())
    {
        itlim_msg->second.Increment();        
    }    

    if(pto_stored)
    {
        PushRelay(pto_stored,msg_format,vHops,msg_type_in,timestamp_received,nonce_received,timestamp_to_respond,nonce_to_respond,flags_in,
                  vPayloadIn,vSigScripts,pfrom,MC_PRA_NONE);
    }
    else
    {
        if( (msg_type_relay_ptr != NULL) || (msg_type_response_ptr != NULL) )
        {
            if(MultichainRelayResponse(msg_type_stored,pto_stored,
                                       msg_type_in,flags_in,vPayloadIn,vAddrIn,
                                       msg_type_response_ptr,&flags_response,vPayloadResponse,vAddrResponse,
                                       msg_type_relay_ptr,&flags_relay,vPayloadRelay,vAddrRelay,strError))
            {
                if(msg_type_response_ptr && *msg_type_response_ptr)
                {
                    if(*msg_type_response_ptr != MC_RMT_ADD_RESPONSE)
                    {
                        PushRelay(pfrom,0,vEmptyHops,*msg_type_response_ptr,m_LastTime,0,timestamp_received,nonce_received,flags_response,
                                  vPayloadResponse,vSigScriptsEmpty,NULL,MC_PRA_GENERATE_NONCE);                    
                    }
                    else
                    {
                        map<int64_t, mc_RelayRequest>::iterator itreq = m_Requests.find(AggregateNonce(timestamp_to_respond,nonce_to_respond));
                        if(itreq != m_Requests.end())
                        {
                            Lock();
                            AddResponse(itreq->second.m_Nonce,pfrom,vHops.size() ? vHops[0] : 0,hop_count,AggregateNonce(timestamp_received,nonce_received),msg_type_in,flags_in,vPayloadIn,MC_RST_SUCCESS);
                            UnLock();
                        }                        
                        else
                        {
                            LogPrintf("ProcessRelay() : Response without stored request from peer %d\n",pfrom->GetId());     
                            return false;                            
                        }
                    }
                }
                if(msg_type_relay_ptr && *msg_type_relay_ptr)
                {
                    LOCK(cs_vNodes);
                    BOOST_FOREACH(CNode* pnode, vNodes)
                    {
                        if(pnode != pfrom)
                        {
                            PushRelay(pnode,msg_format,vHops,*msg_type_relay_ptr,timestamp_received,nonce_received,timestamp_to_respond,nonce_to_respond,flags_relay,
                                      vPayloadRelay,vSigScriptsEmpty,pfrom,MC_PRA_NONE);
                        }
                    }
                }                
            }
            else
            {
                LogPrintf("ProcessRelay() : Error processing request %08X from peer %d: %s\n",msg_type_in,pfrom->GetId(),strError.c_str());     
                return false;                
            }
        }        
    }
    
    if(timestamp_to_respond)
    {
        SetRelayRecord(NULL,pfrom,msg_type_in,timestamp_received,nonce_received);        
    }
    else
    {
        SetRelayRecord(NULL,pfrom,MC_RMT_NEW_REQUEST,timestamp_received,nonce_received);
    }
    
    return true;
}

/*
typedef struct mc_RelayResponse
{
    int64_t m_Nonce;
    uint32_t m_MsgType;
    uint32_t m_Flags;
    CNode *m_NodeFrom;
    int m_HopCount;
    mc_NodeFullAddress m_Source;
    uint32_t m_LastTryTimestamp;
    vector <unsigned char> m_Payload;
    vector <mc_RelayRequest> m_Requests;
    
    void Zero();
} mc_RelayResponse;

typedef struct mc_RelayRequest
{
    int64_t m_Nonce;
    uint32_t m_MsgType;
    uint32_t m_Flags;
    int64_t m_ParentNonce;
    int m_ParentResponseID;
    uint32_t m_LastTryTimestamp;
    int m_TryCount;
    uint32_t m_Status;
    vector <unsigned char> m_Payload;   
    vector <mc_RelayResponse> m_Responses;
    
    void Zero();
} mc_RelayRequest;
*/

void mc_RelayResponse::Zero()
{
    m_Nonce=0;
    m_MsgType=MC_RMT_NONE;
    m_Flags=0;
    m_NodeFrom=NULL;
    m_HopCount=0;
    m_Source=0;
    m_LastTryTimestamp=0;
    m_Status=MC_RST_NONE;
    m_Payload.clear();
    m_Requests.clear();    
}

void mc_RelayRequest::Zero()
{
    m_Nonce=0;
    m_MsgType=MC_RMT_NONE;
    m_Flags=0;
    m_NodeTo=NULL;
    m_ParentNonce=0;
    m_ParentResponseID=-1;
    m_LastTryTimestamp=0;
    m_TryCount=0;
    m_Status=MC_RST_NONE;
    m_Payload.clear();   
    m_Responses.clear();
}

int mc_RelayManager::AddRequest(int64_t parent_nonce,int parent_response_id,CNode *pto,int64_t nonce,uint32_t msg_type,uint32_t flags,vector <unsigned char>& payload,uint32_t status)
{    
    int err=MC_ERR_NOERROR;
 
    Lock();
    map<int64_t, mc_RelayRequest>::iterator itreq_this = m_Requests.find(nonce);
    if(itreq_this == m_Requests.end())
    {    
        mc_RelayRequest request;

        request.m_Nonce=nonce;
        request.m_MsgType=msg_type;
        request.m_Flags=flags;
        request.m_NodeTo=pto;
        request.m_ParentNonce=parent_nonce;
        request.m_ParentResponseID=parent_response_id;
        request.m_LastTryTimestamp=0;
        request.m_TryCount=0;
        request.m_Status=status;
        request.m_Payload=payload;   
        request.m_Responses.clear();

        if(parent_nonce)
        {
            map<int64_t, mc_RelayRequest>::iterator itreq = m_Requests.find(parent_nonce);
            if(itreq != m_Requests.end())
            {
                if(parent_response_id < (int)(itreq->second.m_Responses.size()))
                {
                    itreq->second.m_Responses[parent_response_id].m_Requests.push_back(request);
                }
            }    
            else
            {
                err=MC_ERR_NOT_FOUND;            
            }
        }

        printf("rqst: %lu, mt: %d, node: %d, size: %d, pr: %lu\n",nonce,msg_type,pto ? pto->GetId() : 0,(int)payload.size(),parent_nonce);
        m_Requests.insert(make_pair(nonce,request));
    }
    else
    {
        err=MC_ERR_FOUND;
    }    
    
    UnLock();
    return err;            
}

int mc_RelayManager::AddResponse(int64_t request,CNode *pfrom,int32_t source,int hop_count,int64_t nonce,uint32_t msg_type,uint32_t flags,vector <unsigned char>& payload,uint32_t status)
{
    printf("resp: %lu, mt: %d, node: %d, size: %d, rn: %lu\n",request,msg_type,pfrom ? pfrom->GetId() : 0,(int)payload.size(),nonce);
    if(request == 0)
    {
        return MC_ERR_NOERROR;
    }
    
    mc_RelayResponse response;
    response.m_Nonce=nonce;
    response.m_MsgType=msg_type;
    response.m_Flags=flags;
    response.m_NodeFrom=pfrom;
    response.m_HopCount=hop_count;
    response.m_Source=source;
    response.m_LastTryTimestamp=0;
    response.m_Status=status;
    response.m_Payload=payload;
    response.m_Requests.clear();    
    
    if(request)
    {
        map<int64_t, mc_RelayRequest>::iterator itreq = m_Requests.find(request);
        if(itreq != m_Requests.end())
        {    
            itreq->second.m_Responses.push_back(response);
            if(status & MC_RST_SUCCESS)
            {
                itreq->second.m_Status |= MC_RST_SUCCESS;
            }
        }
        else
        {
            return MC_ERR_NOT_FOUND;
        }
    }
    
    return MC_ERR_NOERROR; 
}

int mc_RelayManager::DeleteRequest(int64_t request)
{
    int err=MC_ERR_NOERROR;

    Lock();
    map<int64_t, mc_RelayRequest>::iterator itreq = m_Requests.find(request);
    if(itreq != m_Requests.end())
    {
        m_Requests.erase(itreq);       
    }    
    else
    {
        err= MC_ERR_NOT_FOUND;
    }
    
    UnLock();
    return err;     
}

mc_RelayRequest *mc_RelayManager::FindRequest(int64_t request)
{
    Lock();
    map<int64_t, mc_RelayRequest>::iterator itreq = m_Requests.find(request);
    if(itreq != m_Requests.end())
    {
        return &(itreq->second);
    }    
    
    UnLock();
    return NULL;    
}


int64_t mc_RelayManager::SendRequest(CNode* pto,uint32_t msg_type,uint32_t flags,vector <unsigned char>& payload)
{
    uint32_t timestamp=mc_TimeNowAsUInt();
    uint32_t nonce=GenerateNonce();
    int64_t aggr_nonce=AggregateNonce(timestamp,nonce);
    vector <int32_t> vEmptyHops;
    
    vector<CScript> vSigScriptsEmpty;

    {
        LOCK(cs_vNodes);
        BOOST_FOREACH(CNode* pnode, vNodes)
        {
            if( (pto == NULL) || (pnode == pto) )
            {
                PushRelay(pnode,0,vEmptyHops,msg_type,timestamp,nonce,0,0,flags,payload,vSigScriptsEmpty,NULL,0);                    
            }
        }
    }

    if(AddRequest(0,0,pto,aggr_nonce,msg_type,flags,payload,MC_RST_NONE) != MC_ERR_NOERROR)
    {
        return 0;
    }
    
    return aggr_nonce;
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
/*    
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
 */ 
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

/*
void mc_RelayManager::PushRelay(CNode*    pto, 
                                uint32_t  msg_format,        
                                unsigned char hop_count,
                                uint32_t  msg_type,
                                uint32_t  timestamp_to_send,
                                uint32_t  nonce_to_send,
                                uint32_t  timestamp_to_respond,
                                uint32_t  nonce_to_respond,
                                uint32_t  flags,
                                vector<unsigned char>& payload,
                                vector<CScript>&  sigScripts_to_relay,
                                CNode*    pfrom, 
                                uint32_t  action)
{
    vector <unsigned char> vOriginAddress;
    vector <unsigned char> vDestinationAddress;
    vector<CScript>  sigScripts;
    vector<unsigned char>vSigScript;
    CScript sigScript;
    uint256 message_hash;
    uint32_t timestamp;     
    uint32_t nonce;     
    mc_NodeFullAddress origin_to_send;
    mc_NodeFullAddress *origin;
    mc_NodeFullAddress destination_to_send;
    mc_NodeFullAddress *destination;
    
    nonce=nonce_to_send;
    timestamp=timestamp_to_send;
    
    if(action & MC_PRA_GENERATE_TIMSTAMP)
    {
        timestamp=mc_TimeNowAsUInt();
    }
    
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
    
*/