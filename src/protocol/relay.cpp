// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "protocol/relay.h"
#include "structs/base58.h"
#include "wallet/chunkdb.h"
#include "wallet/chunkcollector.h"
#include "wallet/wallettxs.h"
#include "community/community.h"

bool mc_Chunk_RelayResponse(uint32_t msg_type_stored, CNode *pto_stored,
                             uint32_t msg_type_in, uint32_t  flags, vector<unsigned char>& vPayloadIn,vector<CScript>& vSigScriptsIn,vector<CScript>& vSigScriptsToVerify,
                             uint32_t* msg_type_response,uint32_t  *flags_response,vector<unsigned char>& vPayloadResponse,vector<CScript>& vSigScriptsRespond,
                             uint32_t* msg_type_relay,uint32_t  *flags_relay,vector<unsigned char>& vPayloadRelay,vector<CScript>& vSigScriptsRelay,string& strError);


string mc_MsgTypeStr(uint32_t msg_type)
{
    char *ptr;
    ptr=(char*)&msg_type;
    if(msg_type < 0x01000000)
    {
        return strprintf("%08x",msg_type);        
    }
    return strprintf("%c%c%c%c",ptr[0],ptr[1],ptr[2],ptr[3]);
}

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
                             uint32_t msg_type_in, uint32_t  flags, vector<unsigned char>& vPayloadIn,vector<CScript>& vSigScriptsIn,vector<CScript>& vSigScriptsToVerify,
                             uint32_t* msg_type_response,uint32_t  *flags_response,vector<unsigned char>& vPayloadResponse,vector<CScript>& vSigScriptsRespond,
                             uint32_t* msg_type_relay,uint32_t  *flags_relay,vector<unsigned char>& vPayloadRelay,vector<CScript>& vSigScriptsRelay,string& strError)
{
    switch(msg_type_in)
    {
        case MC_RMT_CHUNK_QUERY:
        case MC_RMT_CHUNK_QUERY_HIT:
        case MC_RMT_CHUNK_REQUEST:
        case MC_RMT_CHUNK_RESPONSE:
            return mc_Chunk_RelayResponse(msg_type_stored, pto_stored,
                              msg_type_in,   flags, vPayloadIn, vSigScriptsIn, vSigScriptsToVerify,
                              msg_type_response,  flags_response,vPayloadResponse, vSigScriptsRespond,
                              msg_type_relay,  flags_relay,vPayloadRelay, vSigScriptsRelay, strError);
    }
    
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
            
//    mc_DumpSize("H",ptr,ptrEnd-ptr,32);
    strError="";
    switch(msg_type_in)
    {
        case MC_RMT_MC_ADDRESS_QUERY:
            if(msg_type_stored)
            {
                strError=strprintf("Unexpected response message type (%s,%s)",mc_MsgTypeStr(msg_type_stored).c_str(),mc_MsgTypeStr(msg_type_in).c_str());
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
                strError=strprintf("Unexpected response message type (%s,%s)",mc_MsgTypeStr(msg_type_stored).c_str(),mc_MsgTypeStr(msg_type_in).c_str());
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



void mc_RelayManager::Zero()
{
    m_Semaphore=NULL;
    m_LockedBy=0;         
    m_LastTime=0;
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
    MsgTypeSettings(MC_RMT_CHUNK_QUERY     ,pwalletTxsMain->m_ChunkCollector->m_TimeoutQuery+5,10, 100,  1*1024*1024);
    MsgTypeSettings(MC_RMT_CHUNK_QUERY_HIT ,pwalletTxsMain->m_ChunkCollector->m_TimeoutQuery+5,10, 100,  1*1024*1024);
    MsgTypeSettings(MC_RMT_CHUNK_REQUEST   ,pwalletTxsMain->m_ChunkCollector->m_TimeoutRequest+5,10, 100, (pwalletTxsMain->m_ChunkCollector->m_MaxMBPerSecond+2)*1024*1024);
    MsgTypeSettings(MC_RMT_CHUNK_RESPONSE  , 0,10, 100,100*1024*1024);
    MsgTypeSettings(MC_RMT_ERROR_IN_MESSAGE,30,10,1000,  1*1024*1024);
    MsgTypeSettings(MC_RMT_NEW_REQUEST     ,30,10,1000,  1*1024*1024);
    
    
    m_MinTimeShift=2 * 6 * Params().TargetSpacing();
    m_MaxTimeShift=2 * 6 * Params().TargetSpacing();
    m_MaxResponses=16;
}

void mc_RelayManager::CheckTime()
{
    uint32_t time_now=mc_TimeNowAsUInt();
    if(time_now == m_LastTime)
    {
        return;
    }

    Lock();
    for(map<const mc_RelayRecordKey,mc_RelayRecordValue>::iterator it = m_RelayRecords.begin(); it != m_RelayRecords.end();)
    {
        if(it->second.m_Timestamp < m_LastTime)
        {
/*            
            mc_OffchainMessageID msg_id=it->first.m_ID;
            if(fDebug)LogPrint("offchain","Offchain rrdl:  %s, from: %d, to: %d, msg: %s, now: %d, exp: %d\n",
            msg_id.ToString().c_str(),it->second.m_NodeFrom,it->first.m_NodeTo,mc_MsgTypeStr(it->second.m_MsgType).c_str(),m_LastTime,it->second.m_Timestamp);
*/
            m_RelayRecords.erase(it++);
        }
        else
        {
            it++;
        }
    }        
    m_LastTime=time_now;
    UnLock();
}

void mc_RelayManager::SetRelayRecord(CNode *pto,CNode *pfrom,uint32_t msg_type,mc_OffchainMessageID msg_id)
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
    const mc_RelayRecordKey key=mc_RelayRecordKey(msg_id,pto_id);
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
/*   
    if(fDebug)LogPrint("offchain","Offchain rrst:  %s, from: %d, to: %d, msg: %s, now: %d, exp: %d\n",
    msg_id.ToString().c_str(),pfrom ? pfrom->GetId() : 0,pto ? pto->GetId() : 0,mc_MsgTypeStr(msg_type).c_str(),m_LastTime,value.m_Timestamp);
*/    

}

int mc_RelayManager::GetRelayRecord(CNode *pfrom,mc_OffchainMessageID msg_id,uint32_t* msg_type,CNode **pto)
{
    NodeId pfrom_id,pto_id;
    
    pfrom_id=0;
    if(pfrom)
    {
        pfrom_id=pfrom->GetId();
    }
//    printf("getrr: %d, ts: %u, nc: %u\n",pfrom_id,timestamp,nonce);
    const mc_RelayRecordKey key=mc_RelayRecordKey(msg_id,pfrom_id);
    map<const mc_RelayRecordKey, mc_RelayRecordValue>::iterator it = m_RelayRecords.find(key);
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

mc_OffchainMessageID mc_RelayManager::GenerateMsgID(uint32_t timestamp)
{
    mc_OffchainMessageID msg_id;
    msg_id.m_TimeStamp=timestamp;
    GetRandBytes((unsigned char*)&(msg_id.m_Nonce), sizeof(msg_id.m_Nonce));
    return msg_id;
}

mc_OffchainMessageID mc_RelayManager::GenerateMsgID()
{
    return GenerateMsgID(mc_TimeNowAsUInt());
}

uint32_t mc_RelayManager::GenerateNonce()
{
    uint32_t nonce;     
    
    GetRandBytes((unsigned char*)&nonce, sizeof(nonce));
    
    nonce &= 0x7FFFFFFF;
    
    return nonce;
}

mc_OffchainMessageID mc_RelayManager::PushRelay(CNode*    pto, 
                                uint32_t  msg_format,        
                                vector <int32_t> &vHops,
                                vector <int32_t> &vSendPaths,                                
                                uint32_t  msg_type,
                                mc_OffchainMessageID msg_id,
                                mc_OffchainMessageID msg_id_to_respond,
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
    
    if( (action & MC_PRA_SIGN_WITH_HANDSHAKE_ADDRESS) && (MCP_ANYONE_CAN_CONNECT == 0) )
    {
        CHashWriter ssHash(SER_GETHASH, 0);
        ssHash << msg_type;        
        ssHash << msg_id.m_TimeStamp;
        ssHash << msg_id.m_Nonce;
        ssHash << msg_id_to_respond.m_TimeStamp;
        ssHash << msg_id_to_respond.m_Nonce;
        ssHash << flags;
        ssHash << payload;
        
        message_hash=ssHash.GetHash();    
        
        CHashWriter ssSig(SER_GETHASH, 0);
        
        ssSig << message_hash;
        ssSig << vector<unsigned char>((unsigned char*)&msg_id_to_respond, (unsigned char*)&msg_id_to_respond+sizeof(msg_id_to_respond));
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
    
//    printf("send: %d, to: %d, from: %d, hc: %d, size: %d, ts: %u, nc: %u\n",msg_type,pto->GetId(),pfrom ? pfrom->GetId() : 0,(int)vHops.size(),(int)payload.size(),timestamp,nonce);
    if(fDebug)LogPrint("offchain","Offchain send: %s, request: %s, to: %d, from: %d, msg: %s, hops: %d, size: %d\n",
            msg_id.ToString().c_str(),msg_id_to_respond.ToString().c_str(),pto->GetId(),pfrom ? pfrom->GetId() : 0,mc_MsgTypeStr(msg_type).c_str(),(int)vHops.size(),(int)payload.size());
    pto->PushMessage("offchain",
                        msg_format,
                        vHops,
                        vSendPaths,
                        msg_type,
                        msg_id.m_TimeStamp,
                        msg_id.m_Nonce,
                        msg_id_to_respond.m_TimeStamp,
                        msg_id_to_respond.m_Nonce,
                        flags,
                        payload,
                        sigScripts);        
    
//    SetRelayRecord(pto,NULL,msg_type,timestamp,nonce);
    SetRelayRecord(pto,pfrom,msg_type,msg_id);
    
    return msg_id;
}
    
bool mc_RelayManager::ProcessRelay( CNode* pfrom, 
                                    CDataStream& vRecv, 
                                    CValidationState &state, 
                                    uint32_t verify_flags_in)
{
    uint32_t  msg_type_in;
    uint32_t verify_flags;
    int err;
    mc_OffchainMessageID msg_id_received;
    mc_OffchainMessageID msg_id_to_respond;
    vector<unsigned char> vPayloadIn;
    vector<CScript> vSigScripts;
    vector<CScript> vSigScriptsToVerify;
    vector<CScript> vSigScriptsRespond;
    vector<CScript> vSigScriptsRelay;
    vector <int32_t> vHops;
    vector <int32_t> vHopsToRelay;
    vector <int32_t> vSendPaths;
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
    string strError;    
    
    msg_type_stored=MC_RMT_NONE;
    msg_type_response=MC_RMT_NONE;
    msg_type_relay=MC_RMT_NONE;
    pto_stored=NULL;
    
    
    flags_response=0x00;
    flags_relay=0x00;
    
    if(true)    
    {
        string strPayload;
        strPayload.resize(2*(int)vRecv.size()+1,'Z');
        mc_BinToHex(&strPayload[0],&vRecv.str()[0],(int)vRecv.size());

        if(fDebug)LogPrint("offchain","Offchain mesg: %s\n",strPayload.c_str());
    }
    
    
    verify_flags=verify_flags_in;
    vRecv >> msg_format;
    if(msg_format != 0)
    {
        LogPrintf("ProcessOffchain() : Unsupported message format %08X\n",msg_format);     
        return false;        
    }
        
    vRecv >> vHops;
    hop_count=(int)vHops.size();
    vRecv >> vSendPaths;
    if(vSendPaths.size())
    {
        LogPrintf("ProcessOffchain() : Unsupported send path\n");     
        return false;                
    }
    vRecv >> msg_type_in;
    switch(msg_type_in)
    {
        case MC_RMT_MC_ADDRESS_QUERY:
            verify_flags |= MC_VRA_IS_NOT_RESPONSE | MC_VRA_NOT_ALLOWED;
            break;
        case MC_RMT_NODE_DETAILS:
            verify_flags |= MC_VRA_IS_RESPONSE | MC_VRA_SIGNATURE_ORIGIN  | MC_VRA_NOT_ALLOWED;
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
                LogPrintf("ProcessOffchain() : Unsupported offchain message type %s\n",mc_MsgTypeStr(msg_type_in).c_str());     
                return false;
            }
            break;
    }
    
    if(verify_flags & MC_VRA_NOT_ALLOWED)
    {
        LogPrintf("ProcessOffchain() : Not allowed offchain message type %s\n",mc_MsgTypeStr(msg_type_in).c_str());             
        return false;            
    }
    
    if(verify_flags & MC_VRA_SINGLE_HOP)
    {
        if(hop_count)
        {
            LogPrintf("ProcessOffchain() : Unsupported hop count %d for msg type %s\n",hop_count,mc_MsgTypeStr(msg_type_in).c_str());     
            return false;            
        }
    }
    
    if(verify_flags & MC_VRA_DOUBLE_HOP)
    {
        if(hop_count > 1)
        {
            LogPrintf("ProcessOffchain() : Unsupported hop count %d for msg type %s\n",hop_count,mc_MsgTypeStr(msg_type_in).c_str());     
            return false;            
        }
    }
        
    CheckTime();
        
    vRecv >> msg_id_received.m_TimeStamp;
    
    if(verify_flags & MC_VRA_TIMESTAMP)
    {
        if(msg_id_received.m_TimeStamp+m_MinTimeShift < m_LastTime)
        {
            LogPrintf("ProcessOffchain() : Timestamp too far in the past: %d\n",msg_id_received.m_TimeStamp);     
            return false;                        
        }
        if(msg_id_received.m_TimeStamp > m_LastTime + m_MaxTimeShift)
        {
            LogPrintf("ProcessOffchain() : Timestamp too far in the future: %d\n",msg_id_received.m_TimeStamp);     
            return false;                        
        }
    }    
    
    vRecv >> msg_id_received.m_Nonce;    
    
    
    msg_type_relay_ptr=&msg_type_relay;
    msg_type_response_ptr=&msg_type_response;
    
    if( verify_flags & (MC_VRA_PROCESS_ONCE | MC_VRA_BROADCAST_ONCE))
    {
        switch(GetRelayRecord(NULL,msg_id_received,NULL,NULL))
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
                LogPrintf("ProcessOffchain() : Processing this message is not allowed by current limits or requesting peer was disconnected\n");     
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
    
        
    vRecv >> msg_id_to_respond.m_TimeStamp;
    vRecv >> msg_id_to_respond.m_Nonce;
    
    if( verify_flags & MC_VRA_IS_NOT_RESPONSE ) 
    {
        if( (msg_id_to_respond.m_TimeStamp != 0) || (msg_id_to_respond.m_Nonce != 0) )
        {
            return state.DoS(100, error("ProcessOffchain() : This message should not be response"),REJECT_INVALID, "bad-nonce");                
        }
    }
        
    if( verify_flags & MC_VRA_IS_RESPONSE ) 
    {
        if( msg_id_to_respond.m_TimeStamp == 0 )
        {
            return state.DoS(100, error("ProcessOffchain() : This message should be response"),REJECT_INVALID, "bad-nonce");                
        }
        
        if((err=GetRelayRecord(pfrom,msg_id_to_respond,&msg_type_stored,&pto_stored)))
        {
            LogPrintf("ProcessOffchain() : Orphan response: %s, request: %s, from: %d, to: %d, msg: %s, hops: %d, size: %d, error: %d\n",
            msg_id_received.ToString().c_str(),msg_id_to_respond.ToString().c_str(),pfrom->GetId(),pto_stored ? pto_stored->GetId() : 0,mc_MsgTypeStr(msg_type_in).c_str(),hop_count,(int)vPayloadIn.size(),err);
            return false;
        }
    }
    
    SetRelayRecord(NULL,pfrom,MC_RMT_ERROR_IN_MESSAGE,msg_id_received);
    
    if(msg_id_to_respond.m_TimeStamp)
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
            
//    printf("recv: %d, from: %d, to: %d, hc: %d, size: %d, ts: %u, nc: %u\n",msg_type_in,pfrom->GetId(),pto_stored ? pto_stored->GetId() : 0,hop_count,(int)vPayloadIn.size(),timestamp_received,nonce_received);
    if(fDebug)LogPrint("offchain","Offchain recv: %s, request: %s, from: %d, to: %d, msg: %s, hops: %d, size: %d\n",
            msg_id_received.ToString().c_str(),msg_id_to_respond.ToString().c_str(),pfrom->GetId(),pto_stored ? pto_stored->GetId() : 0,mc_MsgTypeStr(msg_type_in).c_str(),hop_count,(int)vPayloadIn.size());
    
    if( verify_flags & MC_VRA_SIGNATURES )
    {
        CHashWriter ssHash(SER_GETHASH, 0);
        ssHash << msg_type_in;        
        ssHash << msg_id_received.m_TimeStamp;
        ssHash << msg_id_received.m_Nonce;
        ssHash << msg_id_to_respond.m_TimeStamp;
        ssHash << msg_id_to_respond.m_Nonce;
        ssHash << flags_in;
        ssHash << vPayloadIn;
        
        message_hash=ssHash.GetHash();                
    
        if( verify_flags & MC_VRA_SIGNATURE_ORIGIN )
        {
            if(vSigScripts.size() < 1)
            {
                return state.DoS(100, error("ProcessOffchain() : Missing sigScript"),REJECT_INVALID, "bad-sigscript");                            
            }
            if(vSigScripts[0].size())
            {
                CScript scriptSig=vSigScripts[0];

                opcodetype opcode;

                CScript::const_iterator pc = scriptSig.begin();

                if (!scriptSig.GetOp(pc, opcode, vchSigOut))
                {
                    return state.DoS(100, error("ProcessOffchain() : Cannot extract signature from sigScript"),REJECT_INVALID, "bad-sigscript-signature");                
                }

                vchSigOut.resize(vchSigOut.size()-1);
                if (!scriptSig.GetOp(pc, opcode, vchPubKey))
                {
                    return state.DoS(100, error("ProcessOffchain() : Cannot extract pubkey from sigScript"),REJECT_INVALID, "bad-sigscript-pubkey");                
                }

                CPubKey pubKeyOut(vchPubKey);
                if (!pubKeyOut.IsValid())
                {
                    return state.DoS(100, error("ProcessOffchain() : Invalid pubkey"),REJECT_INVALID, "bad-sigscript-pubkey");                
                }        

                pubkey=pubKeyOut;

                CHashWriter ss(SER_GETHASH, 0);
                
                ss << vector<unsigned char>((unsigned char*)&message_hash, (unsigned char*)&message_hash+32);
                ss << vector<unsigned char>((unsigned char*)&msg_id_to_respond, (unsigned char*)&msg_id_to_respond+sizeof(msg_id_to_respond));
                uint256 signed_hash=ss.GetHash();

                if(!pubkey.Verify(signed_hash,vchSigOut))
                {
                    return state.DoS(100, error("ProcessOffchain() : Wrong signature"),REJECT_INVALID, "bad-signature");                            
                }        
            }
            else
            {
                return state.DoS(100, error("ProcessOffchain() : Empty sigScript"),REJECT_INVALID, "bad-sigscript");                
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
        PushRelay(pto_stored,msg_format,vHops,vSendPaths,msg_type_in,msg_id_received,msg_id_to_respond,flags_in,
                  vPayloadIn,vSigScripts,pfrom,MC_PRA_NONE);
    }
    else
    {
        if( (msg_type_relay_ptr != NULL) || (msg_type_response_ptr != NULL) )
        {
            if(MultichainRelayResponse(msg_type_stored,pto_stored,
                                       msg_type_in,flags_in,vPayloadIn,vSigScripts,vSigScriptsToVerify,
                                       msg_type_response_ptr,&flags_response,vPayloadResponse,vSigScriptsRespond,
                                       msg_type_relay_ptr,&flags_relay,vPayloadRelay,vSigScriptsRelay,strError))
            {
                int dos_score=0;
                if(!pEF->OFF_VerifySignatureScripts(msg_type_in,msg_id_received,msg_id_to_respond,flags_in,vPayloadIn,vSigScriptsToVerify,strError,dos_score))
                {
                    LogPrintf("ProcessOffchain() : Error processing %s (request %s) from peer %d: %s\n",mc_MsgTypeStr(msg_type_in).c_str(),
                            msg_id_received.ToString().c_str(),pfrom->GetId(),strError.c_str());     
                    if(dos_score)
                    {
                        return state.DoS(dos_score, error("ProcessOffchain() : Invalid sigScript"),REJECT_INVALID, "bad-sigscript");       
                    }
                    return false;
                }
                if(msg_type_response_ptr && *msg_type_response_ptr)
                {
                    if(*msg_type_response_ptr != MC_RMT_ADD_RESPONSE)
                    {
                        PushRelay(pfrom,0,vEmptyHops,vSendPaths,*msg_type_response_ptr,GenerateMsgID(m_LastTime),msg_id_received,flags_response,
                                  vPayloadResponse,vSigScriptsRespond,NULL,MC_PRA_NONE);                    
                    }
                    else
                    {
                        map<mc_OffchainMessageID, mc_RelayRequest>::iterator itreq = m_Requests.find(msg_id_to_respond);
                        if(itreq != m_Requests.end())
                        {
                            Lock();
                            AddResponse(itreq->second.m_MsgID,pfrom,vHops.size() ? vHops[0] : 0,hop_count,msg_id_received,msg_type_in,flags_in,vPayloadIn,MC_RST_SUCCESS);
                            UnLock();
                        }                        
                        else
                        {
                            if(fDebug)LogPrint("offchain","ProcessOffchain() : Deleted request: %s, request: %s, from: %d, to: %d, msg: %s, hops: %d, size: %d\n",
                            msg_id_received.ToString().c_str(),msg_id_to_respond.ToString().c_str(),pfrom->GetId(),pto_stored ? pto_stored->GetId() : 0,mc_MsgTypeStr(msg_type_in).c_str(),hop_count,(int)vPayloadIn.size());
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
                            vHopsToRelay=vHops;
                            PushRelay(pnode,msg_format,vHopsToRelay,vSendPaths,*msg_type_relay_ptr,msg_id_received,msg_id_to_respond,flags_relay,
                                      vPayloadRelay,vSigScriptsRelay,pfrom,MC_PRA_NONE);
                        }
                    }
                }                
            }
            else
            {
                LogPrintf("ProcessOffchain() : Error processing %s (request %s) from peer %d: %s\n",mc_MsgTypeStr(msg_type_in).c_str(),
                        msg_id_received.ToString().c_str(),pfrom->GetId(),strError.c_str());     
                return false;                
            }
        }        
    }
    
    if(msg_id_to_respond.m_TimeStamp)
    {
        SetRelayRecord(NULL,pfrom,msg_type_in,msg_id_received);        
    }
    else
    {
        SetRelayRecord(NULL,pfrom,MC_RMT_NEW_REQUEST,msg_id_received);
    }
    
    return true;
}

void mc_RelayResponse::Zero()
{
    m_MsgID=0;
    m_MsgType=MC_RMT_NONE;
    m_Flags=0;
    m_NodeFrom=0;
    m_HopCount=0;
    m_TryCount=0;
    m_Source=0;
    m_LastTryTimestamp=0;
    m_Status=MC_RST_NONE;
    m_Payload.clear();
    m_Requests.clear();    
    m_TimeDiff=0;
}

void mc_RelayRequest::Zero()
{
    m_MsgID=0;
    m_MsgType=MC_RMT_NONE;
    m_Flags=0;
    m_NodeTo=0;
    m_LastTryTimestamp=0;
    m_TryCount=0;
    m_Status=MC_RST_NONE;
    m_DestinationID=0;
    m_EFCacheID=-1;
    m_Payload.clear();   
    m_Responses.clear();
}

int mc_RelayManager::AddRequest(CNode *pto,int64_t destination,mc_OffchainMessageID msg_id,uint32_t msg_type,uint32_t flags,vector <unsigned char>& payload,uint32_t status,int ef_cache_id)
{    
    int err=MC_ERR_NOERROR;
 
    map<mc_OffchainMessageID, mc_RelayRequest>::iterator itreq_this = m_Requests.find(msg_id);
    if(itreq_this == m_Requests.end())
    {    
        mc_RelayRequest request;

        request.m_MsgID=msg_id;
        request.m_MsgType=msg_type;
        request.m_Flags=flags;
        request.m_NodeTo=pto ? pto->GetId() : 0;
        request.m_LastTryTimestamp=0;
        request.m_TryCount=0;
        request.m_Status=status;
        request.m_DestinationID=destination;
        request.m_Payload=payload;   
        request.m_EFCacheID=ef_cache_id;
        request.m_Responses.clear();

        if(fDebug)LogPrint("offchain","Offchain rqst: %s, to: %d, msg: %s, size: %d\n",msg_id.ToString().c_str(),pto ? pto->GetId() : 0,mc_MsgTypeStr(msg_type).c_str(),(int)payload.size());
        m_Requests.insert(make_pair(msg_id,request));
    }
    else
    {
        err=MC_ERR_FOUND;
    }    
    
    return err;            
}

int64_t mc_RelayResponse::SourceID()
{
    int64_t result=(int64_t)m_NodeFrom;
    result = (result << 32) + (int64_t)m_Source;
    return result;
}

int mc_RelayManager::AddResponse(mc_OffchainMessageID request,CNode *pfrom,int32_t source,int hop_count,mc_OffchainMessageID msg_id,uint32_t msg_type,uint32_t flags,vector <unsigned char>& payload,uint32_t status)
{
//    printf("resp: %lu, mt: %d, node: %d, size: %d, rn: %lu\n",request,msg_type,pfrom ? pfrom->GetId() : 0,(int)payload.size(),nonce);
    if(fDebug)LogPrint("offchain","Offchain resp: %s, request: %s, from: %d, msg: %s, size: %d\n",
            msg_id.ToString().c_str(),request.ToString().c_str(),pfrom ? pfrom->GetId() : 0,mc_MsgTypeStr(msg_type).c_str(),(int)payload.size());
    if(request.IsZero())
    {
        return MC_ERR_NOERROR;
    }
    
    mc_RelayResponse response;
    response.m_MsgID=msg_id;    
    response.m_MsgType=msg_type;
    response.m_TimeDiff=msg_id.m_TimeStamp-m_LastTime;
    response.m_Flags=flags;
    response.m_NodeFrom=pfrom ? pfrom->GetId() : 0;
    response.m_HopCount=hop_count;
    response.m_Source=source;
    response.m_LastTryTimestamp=0;
    response.m_Status=status;
    response.m_Payload=payload;
    response.m_Requests.clear();    
    
    if(!request.IsZero())
    {
        map<mc_OffchainMessageID, mc_RelayRequest>::iterator itreq = m_Requests.find(request);
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

int mc_RelayManager::DeleteRequest(mc_OffchainMessageID request)
{
    int err=MC_ERR_NOERROR;

    Lock();
    map<mc_OffchainMessageID, mc_RelayRequest>::iterator itreq = m_Requests.find(request);
    if(itreq != m_Requests.end())
    {
        pEF->OFF_FreeEFCache(itreq->second.m_EFCacheID);
        if(fDebug)LogPrint("offchain","Offchain delete: %s, msg: %s, size: %d. Open requests: %d\n",itreq->second.m_MsgID.ToString().c_str(),
            mc_MsgTypeStr(itreq->second.m_MsgType).c_str(),(int)itreq->second.m_Payload.size(),(int)m_Requests.size());
        m_Requests.erase(itreq);       
    }    
    else
    {
        err= MC_ERR_NOT_FOUND;
    }
    
    UnLock();
    return err;     
}

mc_RelayRequest *mc_RelayManager::FindRequest(mc_OffchainMessageID request)
{
    Lock();
    map<mc_OffchainMessageID, mc_RelayRequest>::iterator itreq = m_Requests.find(request);
    if(itreq != m_Requests.end())
    {
        return &(itreq->second);
    }    
    
    UnLock();
    return NULL;    
}


mc_OffchainMessageID mc_RelayManager::SendRequest(CNode* pto,uint32_t msg_type,uint32_t flags,vector <unsigned char>& payload)
{
    mc_OffchainMessageID msg_id;
    vector <int32_t> vEmptyHops;
    vector <int32_t> vEmptySendPaths;
    vector<CScript> vSigScriptsEmpty;

    msg_id=GenerateMsgID();
    
    Lock();
    {
        LOCK(cs_vNodes);
        BOOST_FOREACH(CNode* pnode, vNodes)
        {
            if( (pto == NULL) || (pnode == pto) )
            {
                bool take_it=false;
                int max_kb_per_destination=pwalletTxsMain->m_ChunkCollector->m_MaxKBPerDestination;
                if(pEF->NET_IsFinalized(pnode))
                {
                    if( (pnode->nMaxKBPerDestination == 0) || (2 * pnode->nMaxKBPerDestination >= max_kb_per_destination) )
                    {
                        take_it=true;
                    }
                    else
                    {
                        if(mc_RandomInRange(1,max_kb_per_destination/pnode->nMaxKBPerDestination) == 1)
                        {
                            take_it=true;                            
                        }
                    }
                }
                if(take_it)
                {
                    PushRelay(pnode,0,vEmptyHops,vEmptySendPaths,msg_type,msg_id,mc_OffchainMessageID(),flags,payload,vSigScriptsEmpty,NULL,0);                    
                }
            }
        }
    }

    if(AddRequest(pto,0,msg_id,msg_type,flags,payload,MC_RST_NONE,-1) != MC_ERR_NOERROR)
    {
        UnLock();
        return mc_OffchainMessageID();
    }
    
    UnLock();
    return msg_id;
}

mc_OffchainMessageID mc_RelayManager::SendNextRequest(mc_RelayResponse* response,uint32_t msg_type,uint32_t flags,vector <unsigned char>& payload,set<CPubKey>& sAddresses,int ef_cache_id)
{
    mc_OffchainMessageID msg_id;
    vector <int32_t> vEmptyHops;
    vector <int32_t> vEmptySendPaths;
    vector<CScript> vSigScripts;
    string strError;
    
    msg_id=GenerateMsgID();

    
    Lock();                                                                     // Secondary lock, to make sure it is locked by this thread
    {
        LOCK(cs_vNodes);
        BOOST_FOREACH(CNode* pnode, vNodes)
        {
            if( pnode->GetId() == response->m_NodeFrom )
            {
                if(pEF->OFF_CreateSignatureScripts(msg_type,msg_id,response->m_MsgID,flags,payload,sAddresses,vSigScripts,strError))
                {
                    msg_id=PushRelay(pnode,0,vEmptyHops,vEmptySendPaths,msg_type,msg_id,response->m_MsgID,
                                         flags,payload,vSigScripts,NULL,MC_PRA_NONE);                    
                    if(AddRequest(pnode,response->SourceID(),msg_id,msg_type,flags,payload,MC_RST_NONE,ef_cache_id) != MC_ERR_NOERROR)
                    {                    
                        UnLock();
                        return mc_OffchainMessageID();
                    }
                }
                else
                {
                    if(fDebug)LogPrint("offchain","Cannot sign offchain request, error: %s\n",strError.c_str());
                    msg_id.m_TimeStamp=0;
                    msg_id.m_Nonce=0;
                }
            }
        }
    }
    
    UnLock();
    return msg_id;
}


void mc_RelayManager::InvalidateResponsesFromDisconnected()
{
    LOCK(cs_vNodes);
    int m=(int)vNodes.size();
    Lock();
    BOOST_FOREACH(PAIRTYPE(const mc_OffchainMessageID,mc_RelayRequest)& item, m_Requests)    
    {
        for(int i=0;i<(int)item.second.m_Responses.size();i++)
        {
            int n=0;
            while(n<m)
            {
                if(vNodes[n]->GetId() == item.second.m_Responses[i].m_NodeFrom)
                {
                    n=m+1;
                }
                n++;
            }
            if(n == m)
            {
                item.second.m_Responses[i].m_Status &= ~MC_RST_SUCCESS;
                item.second.m_Responses[i].m_Status |= MC_RST_DISCONNECTED;
            }
        }
    }    
    UnLock();
}

