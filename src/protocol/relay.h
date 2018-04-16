// Copyright (c) 2014-2017 Coin Sciences Ltd
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
#define MC_PRA_GENERATE_TIMESTAMP            0x00000040
#define MC_PRA_GENERATE_NONCE                0x00000080

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

#define MC_VRA_DEFAULT                       0x03970000

#define MC_RMT_NONE                                   0
#define MC_RMT_REJECT                        0x00000001
#define MC_RMT_MC_ADDRESS_QUERY              0x00000002
#define MC_RMT_NODE_DETAILS                  0x00000003
#define MC_RMT_CHUNK_QUERY                   0x00000101
#define MC_RMT_CHUNK_QUERY_HIT               0x00000102
#define MC_RMT_CHUNK_REQUEST                 0x00000103
#define MC_RMT_CHUNK_RESPONSE                0x00000104
#define MC_RMT_ADD_RESPONSE                  0x01000001
#define MC_RMT_ERROR_IN_MESSAGE              0x01000002
#define MC_RMT_NEW_REQUEST                   0x01000003

#define MC_RDT_UNKNOWN                                0
#define MC_RDT_MC_ADDRESS                             1
#define MC_RDT_NET_ADDRESS                            2

#define MC_LIM_MAX_SECONDS                60
#define MC_LIM_MAX_MEASURES                4

#define MC_RST_NONE                          0x00000000
#define MC_RST_DELETED                       0x00000001
#define MC_RST_SUCCESS                       0x00000002
#define MC_RST_PERMANENT_FAILURE             0x00000004
#define MC_RST_TEMPORARY_FAILURE             0x00000008

using namespace std;

bool MultichainRelayResponse(uint32_t msg_type_stored, CNode *pto_stored,
                             uint32_t msg_type_in, uint32_t  flags, vector<unsigned char>& vPayloadIn,vector<CKeyID>&  vAddrIn,
                             uint32_t* msg_type_response,uint32_t  *flags_response,vector<unsigned char>& vPayloadResponse,vector<CKeyID>&  vAddrResponse,
                             uint32_t* msg_type_relay,uint32_t  *flags_relay,vector<unsigned char>& vPayloadRelay,vector<CKeyID>&  vAddrRelay,
                             string& strError);





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
    uint32_t  m_TimeStamp;
    uint32_t  m_Nonce;
    NodeId m_NodeTo;

    mc_RelayRecordKey(uint32_t timestamp,uint32_t nonce,NodeId node)
    {
        m_TimeStamp=timestamp;
        m_Nonce=nonce;
        m_NodeTo=node;
    }
    
    friend bool operator<(const mc_RelayRecordKey& a, const mc_RelayRecordKey& b)
    {
        return ((a.m_TimeStamp < b.m_TimeStamp) || 
                (a.m_TimeStamp == b.m_TimeStamp && a.m_Nonce < b.m_Nonce) || 
                (a.m_TimeStamp == b.m_TimeStamp && a.m_Nonce == b.m_Nonce && a.m_NodeTo < b.m_NodeTo));
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
    int64_t m_Nonce;
    uint32_t m_MsgType;
    uint32_t m_Flags;
    CNode *m_NodeFrom;
    int m_HopCount;
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
} mc_RelayResponse;

typedef struct mc_RelayRequest
{
    int64_t m_Nonce;
    uint32_t m_MsgType;
    uint32_t m_Flags;
    CNode *m_NodeTo;
    int64_t m_ParentNonce;
    int m_ParentResponseID;
    uint32_t m_LastTryTimestamp;
    int m_TryCount;
    uint32_t m_Status;
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
    map<int64_t,mc_RelayRequest> m_Requests;
    
    void Zero();
    void Destroy();
    int Lock();
    int Lock(int write_mode, int allow_secondary);
    void UnLock();        
    int Initialize();
    
    int64_t AggregateNonce(uint32_t timestamp,uint32_t nonce);
    uint32_t GenerateNonce();
    void SetDefaults();
    void SetMyIPs(uint32_t *ips,int ip_count);
    void MsgTypeSettings(uint32_t msg_type,int latency,int seconds,int64_t serves_per_second,int64_t bytes_per_second); 
    void InitNodeAddress(mc_NodeFullAddress* node_address,CNode* pto,uint32_t action);
    void InitNodeAddress(mc_NodeFullAddress* node_address,CKeyID& mc_address, vector<CAddress>& net_addresses);
    void CheckTime();
    void SetRelayRecord(CNode *pto,CNode *pfrom,uint32_t msg_type,uint32_t timestamp,uint32_t nonce);
    int GetRelayRecord(CNode *pfrom,uint32_t timestamp,uint32_t nonce,uint32_t *msg_type,CNode **pto);
    
    int64_t PushRelay          (   CNode*    pto, 
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
                                uint32_t  action);
    
    bool ProcessRelay       (   CNode* pfrom, 
                                CDataStream& vRecv, 
                                CValidationState &state, 
                                uint32_t verify_flags_in);

    int AddRequest(int64_t parent_nonce,int parent_response_id,CNode *pto,int64_t nonce,uint32_t msg_type,uint32_t flags,vector <unsigned char>& payload,uint32_t status);
    int AddResponse(int64_t request,CNode *pfrom,int32_t source,int hop_count,int64_t nonce,uint32_t msg_type,uint32_t flags,vector <unsigned char>& payload,uint32_t status);
    int DeleteRequest(int64_t request);
    mc_RelayRequest *FindRequest(int64_t request);
    
    
    int64_t SendRequest(CNode* pto,uint32_t msg_type,uint32_t flags,vector <unsigned char>& payload);
}   mc_RelayManager;




#endif /* RELAY_H */

