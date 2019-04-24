// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.


#ifndef RELAY_H
#define RELAY_H

#include "core/init.h"
#include "core/main.h"
#include "utils/util.h"
#include "wallet/wallet.h"
#include "multichain/multichain.h"
#include "keys/pubkey.h"
#include "keys/key.h"
#include "net/net.h"

#define MC_PRA_NONE                          0x00000000
#define MC_PRA_MY_ORIGIN_MC_ADDRESS          0x00000001
#define MC_PRA_MY_ORIGIN_NT_ADDRESS          0x00000002
#define MC_PRA_USE_DESTINATION_ADDRESS       0x00000004
#define MC_PRA_SIGN_WITH_HANDSHAKE_ADDRESS   0x00000020

#define MC_VRA_NONE                          0x00000000
#define MC_VRA_IS_RESPONSE                   0x00000002
#define MC_VRA_IS_NOT_RESPONSE               0x00000004
#define MC_VRA_SIGSCRIPT                     0x00000080
#define MC_VRA_SIGNATURE_ORIGIN              0x00000100
#define MC_VRA_SIGNATURE_ALL                 0x00000200
#define MC_VRA_MESSAGE_TYPE                  0x00010000
#define MC_VRA_LIMIT_ALL                     0x00020000
#define MC_VRA_LIMIT_MSG_TYPE                0x00040000
#define MC_VRA_SINGLE_HOP                    0x00080000
#define MC_VRA_DOUBLE_HOP                    0x00100000
#define MC_VRA_SIGNATURES                    0x00200000
#define MC_VRA_PROCESS_ONCE                  0x00400000
#define MC_VRA_BROADCAST_ONCE                0x00800000
#define MC_VRA_SINGLE_HOP_BROADCAST          0x01000000
#define MC_VRA_TIMESTAMP                     0x02000000
#define MC_VRA_NOT_ALLOWED                   0x80000000

#define MC_VRA_DEFAULT                       0x03970000

#define MC_RMT_NONE                                   0
#define MC_RMT_MC_ADDRESS_QUERY              0x7971616d     //maqy
#define MC_RMT_NODE_DETAILS                  0x7464646e     //nddt
#define MC_RMT_CHUNK_QUERY                   0x79716863     //chqy
#define MC_RMT_CHUNK_QUERY_HIT               0x68716863     //chqh
#define MC_RMT_CHUNK_REQUEST                 0x71726863     //chrq
#define MC_RMT_CHUNK_RESPONSE                0x73726863     //chrs
#define MC_RMT_ADD_RESPONSE                  0x00800001
#define MC_RMT_ERROR_IN_MESSAGE              0x00800002
#define MC_RMT_NEW_REQUEST                   0x00800003
#define MC_RMT_SPECIAL_MASK                  0x80000000
#define MC_RMT_SPECIAL_COLLECT_CHUNKS        0x80000001
#define MC_RMT_SPECIAL_VIEW_CHUNKS           0x80000002

#define MC_RDT_UNKNOWN                                0
#define MC_RDT_MC_ADDRESS                          0x01
#define MC_RDT_NET_ADDRESS                         0x02
#define MC_RDT_EXPIRATION                          0x03
#define MC_RDT_CHUNK_IDS                           0x11
#define MC_RDT_CHUNKS                              0x12
#define MC_RDT_ENTERPRISE_FEATURES                 0x21

#define MC_LIM_MAX_SECONDS                60
#define MC_LIM_MAX_MEASURES                4



#define MC_RST_NONE                          0x00000000
#define MC_RST_DELETED                       0x00000001
#define MC_RST_SUCCESS                       0x00000002
#define MC_RST_PERMANENT_FAILURE             0x00000004
#define MC_RST_TEMPORARY_FAILURE             0x00000008
#define MC_RST_DISCONNECTED                  0x00000010

using namespace std;

struct mc_ChunkCollector;

bool MultichainRelayResponse(uint32_t msg_type_stored, CNode *pto_stored,
                             uint32_t msg_type_in, uint32_t  flags, vector<unsigned char>& vPayloadIn,vector<CKeyID>&  vAddrIn,
                             uint32_t* msg_type_response,uint32_t  *flags_response,vector<unsigned char>& vPayloadResponse,vector<CKeyID>&  vAddrResponse,
                             uint32_t* msg_type_relay,uint32_t  *flags_relay,vector<unsigned char>& vPayloadRelay,vector<CKeyID>&  vAddrRelay,
                             string& strError);

int MultichainCollectChunks(mc_ChunkCollector* collector);
int MultichainCollectChunksQueueStats(mc_ChunkCollector* collector);



typedef struct mc_Limiter
{
    int m_SecondCount;
    int m_MeasureCount;
    uint32_t m_Time;
    int64_t m_Limits[MC_LIM_MAX_MEASURES];
    int64_t m_Totals[MC_LIM_MAX_MEASURES];
    int64_t m_Event[MC_LIM_MAX_MEASURES];
    int64_t m_Measures[MC_LIM_MAX_MEASURES*MC_LIM_MAX_SECONDS];
    void Zero();
    int Initialize(int seconds,int measures);
    int SetLimit(int meausure,int64_t limit);
    
    void CheckTime();
    void CheckTime(uint32_t t);
    
    void SetEvent(int64_t m1);
    void SetEvent(int64_t m1,int64_t m2);
    void SetEvent(int64_t m1,int64_t m2,int64_t m3);
    void SetEvent(int64_t m1,int64_t m2,int64_t m3,int64_t m4);

    int Disallowed();
    int Disallowed(uint32_t t);
    void Increment();
    
} mc_Limiter;

typedef struct mc_OffchainMessageID
{
    uint32_t m_TimeStamp;
    uint96 m_Nonce;
    
    mc_OffchainMessageID()
    {
        m_TimeStamp=0;
        m_Nonce=0;
    }
       
    
    friend bool operator<(const mc_OffchainMessageID& a, const mc_OffchainMessageID& b)
    {
        return ((a.m_TimeStamp < b.m_TimeStamp) || 
                (a.m_TimeStamp == b.m_TimeStamp && a.m_Nonce < b.m_Nonce));
    }

    friend bool operator==(const mc_OffchainMessageID& a, const mc_OffchainMessageID& b)
    {
        return (a.m_TimeStamp == b.m_TimeStamp && a.m_Nonce == b.m_Nonce);
    }
    
    friend bool operator!=(const mc_OffchainMessageID& a, const mc_OffchainMessageID& b)
    {
        return (a.m_TimeStamp != b.m_TimeStamp || a.m_Nonce != b.m_Nonce);
    }
    
    
    mc_OffchainMessageID& operator=(const mc_OffchainMessageID& b)
    {
        m_TimeStamp=b.m_TimeStamp;
        m_Nonce=b.m_Nonce;
        return *this;
    }
    
    mc_OffchainMessageID& operator=(const uint32_t& b)
    {
        m_TimeStamp=b;
        m_Nonce=0;
        return *this;
    }
    
    bool IsZero()
    {
        return (m_TimeStamp == 0 && m_Nonce == 0);        
    }
    
    std::string ToString()
    {
        return strprintf("%s-%d",m_Nonce.ToString().c_str(),m_TimeStamp);
    }
} mc_OffchainMessageID;


typedef struct mc_NodeFullAddress
{
    CKeyID              m_Address;
    vector<CAddress>    m_NetAddresses;
    
    void Zero();   
    
    mc_NodeFullAddress()
    {
        Zero();
    }
    
} mc_NodeFullAddress;

typedef struct mc_RelayRecordKey
{
    mc_OffchainMessageID m_ID;
    NodeId m_NodeTo;

    mc_RelayRecordKey(mc_OffchainMessageID msg_id,NodeId node)
    {
        m_ID=msg_id;
        m_NodeTo=node;
    }
    
    friend bool operator<(const mc_RelayRecordKey& a, const mc_RelayRecordKey& b)
    {
        return ((a.m_ID < b.m_ID) || 
                (a.m_ID == b.m_ID && a.m_NodeTo < b.m_NodeTo));
    }
    
} mc_RelayRecordKey;

typedef struct mc_RelayRecordValue
{
    uint32_t m_MsgType;
    NodeId m_NodeFrom;
    uint32_t m_Timestamp;
    int m_Count;
    
} mc_RelayRecordValue;

struct mc_RelayRequest;

typedef struct mc_RelayResponse
{
    mc_OffchainMessageID m_MsgID;
    int m_TimeDiff;
    uint32_t m_MsgType;
    uint32_t m_Flags;
    NodeId m_NodeFrom;
    int m_HopCount;
    int m_TryCount;
    int32_t m_Source;
    uint32_t m_LastTryTimestamp;
    uint32_t m_Status;
    vector <unsigned char> m_Payload;
    vector <mc_RelayRequest> m_Requests;
    
    mc_RelayResponse()
    {
        Zero();
    }
    
    void Zero();
    int64_t SourceID();
} mc_RelayResponse;

typedef struct mc_RelayRequest
{
    mc_OffchainMessageID m_MsgID;
    uint32_t m_MsgType;
    uint32_t m_Flags;
    NodeId m_NodeTo;
    uint32_t m_LastTryTimestamp;
    int m_TryCount;
    uint32_t m_Status;
    int64_t m_DestinationID;
    int m_EFCacheID;
    vector <unsigned char> m_Payload;   
    vector <mc_RelayResponse> m_Responses;
    
    mc_RelayRequest()
    {
        Zero();
    }
    
    void Zero();
} mc_RelayRequest;

typedef struct mc_RelayManager
{
    uint32_t m_MyIPs[64];
    int m_MaxResponses;
    int m_MyIPCount;
    uint32_t m_LastTime;    
    uint32_t m_MinTimeShift;
    uint32_t m_MaxTimeShift;
    void *m_Semaphore;                                                          
    uint64_t m_LockedBy;                                                        
    mc_NodeFullAddress m_MyAddress;
            
    mc_RelayManager()
    {
        Zero();        
    };
    
    ~mc_RelayManager()
    {
        Destroy();        
    };
    
    map<uint32_t,int> m_Latency;
    map<uint32_t,mc_Limiter> m_Limiters;
    map<const mc_RelayRecordKey,mc_RelayRecordValue> m_RelayRecords;
    map<mc_OffchainMessageID,mc_RelayRequest> m_Requests;
    
    void Zero();
    void Destroy();
    int Lock();
    int Lock(int write_mode, int allow_secondary);
    void UnLock();        
    int Initialize();
    
    uint32_t GenerateNonce();
    mc_OffchainMessageID GenerateMsgID(uint32_t timestamp);
    mc_OffchainMessageID GenerateMsgID();
    void SetDefaults();
    void SetMyIPs(uint32_t *ips,int ip_count);
    void MsgTypeSettings(uint32_t msg_type,int latency,int seconds,int64_t serves_per_second,int64_t bytes_per_second); 
    void InitNodeAddress(mc_NodeFullAddress* node_address,CNode* pto,uint32_t action);
    void InitNodeAddress(mc_NodeFullAddress* node_address,CKeyID& mc_address, vector<CAddress>& net_addresses);
    void CheckTime();
    void SetRelayRecord(CNode *pto,CNode *pfrom,uint32_t msg_type,mc_OffchainMessageID msg_id);
    int GetRelayRecord(CNode *pfrom,mc_OffchainMessageID msg_id,uint32_t *msg_type,CNode **pto);
    
    mc_OffchainMessageID PushRelay(CNode*    pto, 
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
                                uint32_t  action);
    
    bool ProcessRelay       (   CNode* pfrom, 
                                CDataStream& vRecv, 
                                CValidationState &state, 
                                uint32_t verify_flags_in);

    int AddRequest(CNode *pto,int64_t destination,mc_OffchainMessageID msg_id,uint32_t msg_type,uint32_t flags,vector <unsigned char>& payload,uint32_t status,int ef_cache_id);
//    int AddRequest(int64_t parent_nonce,int parent_response_id,CNode *pto,int64_t nonce,uint32_t msg_type,uint32_t flags,vector <unsigned char>& payload,uint32_t status);
    int AddResponse(mc_OffchainMessageID request,CNode *pfrom,int32_t source,int hop_count,mc_OffchainMessageID msg_id,uint32_t msg_type,uint32_t flags,vector <unsigned char>& payload,uint32_t status);
    //int AddResponse(int64_t request,CNode *pfrom,int32_t source,int hop_count,int64_t nonce,uint32_t msg_type,uint32_t flags,vector <unsigned char>& payload,uint32_t status);
    int DeleteRequest(mc_OffchainMessageID request);
    int ProcessRequest(mc_OffchainMessageID request);
    mc_RelayRequest *FindRequest(mc_OffchainMessageID request);
    void InvalidateResponsesFromDisconnected();
    
    mc_OffchainMessageID SendRequest(CNode* pto,uint32_t msg_type,uint32_t flags,vector <unsigned char>& payload);
//    int64_t SendRequest(CNode* pto,uint32_t msg_type,uint32_t flags,vector <unsigned char>& payload);
    mc_OffchainMessageID SendNextRequest(mc_RelayResponse* response,uint32_t msg_type,uint32_t flags,vector <unsigned char>& payload,set<CPubKey>& vSigScripts,int ef_cache_id);
//    int64_t SendNextRequest(mc_RelayResponse* response,uint32_t msg_type,uint32_t flags,vector <unsigned char>& payload);
}   mc_RelayManager;




#endif /* RELAY_H */

