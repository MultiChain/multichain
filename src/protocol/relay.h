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
#define MC_PRA_CALCULATE_MESSAGE_HASH        0x00000008
#define MC_PRA_SIGN                          0x00000010
#define MC_PRA_SIGN_WITH_HANDSHAKE_ADDRESS   0x00000020
#define MC_PRA_GENERATE_NONCE                0x00000040

#define MC_VRA_NONE                          0x00000000
#define MC_VRA_MESSAGE_TYPE                  0x00000001
#define MC_VRA_ORIGIN_MC_ADDRESS             0x00000002
#define MC_VRA_ORIGIN_NT_ADDRESS             0x00000004
#define MC_VRA_DESTINATION_MC_ADDRESS        0x00000008
#define MC_VRA_DESTINATION_NT_ADDRESS        0x00000010
#define MC_VRA_RESPONSE_TO_NONCE             0x00000020
#define MC_VRA_MESSAGE_HASH                  0x00000040
#define MC_VRA_SIGSCRIPT                     0x00000080
#define MC_VRA_SIGNATURE_ORIGIN              0x00000100
#define MC_VRA_SIGNATURE_ALL                 0x00000200
#define MC_VRA_LIMIT_ALL                     0x00000400
#define MC_VRA_LIMIT_MSG_TYPE                0x00000800
#define MC_VRA_SINGLE_HOP                    0x00001000
#define MC_VRA_ALREADY_PROCESSED             0x00002000

#define MC_RMT_NONE                                   0
#define MC_RMT_GLOBAL_PING                   0x00000001
#define MC_RMT_GLOBAL_PONG                   0x00000002
#define MC_RMT_GLOBAL_REJECT                 0x00000003
#define MC_RMT_GLOBAL_BUSY                   0x00000004
#define MC_RMT_CHUNK_QUERY                   0x00000101
#define MC_RMT_CHUNK_QUERY_HIT               0x00000102
#define MC_RMT_CHUNK_REQUEST                 0x00000103
#define MC_RMT_CHUNK_RESPONSE                0x00000104




using namespace std;

#define MC_LIM_MAX_SECONDS                60
#define MC_LIM_MAX_MEASURES                4


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
    int Intitialize(int seconds,int measures);
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
    int64_t  m_Nonce;
    NodeId m_NodeTo;

    mc_RelayRecordKey(int64_t nonce,NodeId node)
    {
        m_Nonce=nonce;
        m_NodeTo=node;
    }
    
    friend bool operator<(const mc_RelayRecordKey& a, const mc_RelayRecordKey& b)
    {
        return (a.m_Nonce < b.m_Nonce || (a.m_Nonce == b.m_Nonce && a.m_NodeTo < b.m_NodeTo));
    }
    
} mc_RelayRecordKey;

typedef struct mc_RelayRecordValue
{
    uint32_t m_MsgType;
    NodeId m_NodeFrom;
    uint32_t m_Timestamp;
    
} mc_RelayRecordValue;

typedef struct mc_RelayManager
{
    uint32_t m_MyIPs[64];
    int m_MyIPCount;
    uint32_t m_LastTime;
    
    map<uint32_t,int> m_Latency;
    map<uint32_t,mc_Limiter> m_Limiters;
    map<const mc_RelayRecordKey,mc_RelayRecordValue> m_RelayRecords;
    
    void SetDefaults();
    void SetMyIPs(uint32_t *ips,int ip_count);
    void MsgTypeSettings(uint32_t msg_type,int latency,int seconds,int64_t serves_per_second,int64_t bytes_per_second); 
    void InitNodeAddress(mc_NodeFullAddress* node_address,CNode* pto,uint32_t action);
    void InitNodeAddress(mc_NodeFullAddress* node_address,CKeyID& mc_address, vector<CAddress>& net_addresses);
    void CheckTime();
    void SetRelayRecord(CNode *pto,CNode *pfrom,uint32_t msg_type,int64_t nonce);
    int GetRelayRecord(CNode *pfrom,int64_t nonce,uint32_t *msg_type,CNode **pto);
    
    void PushRelay          (   CNode*    pto, 
                                uint32_t  msg_type,
                                int64_t   nonce_to_send,
                                int64_t   response_to_nonce,
                                mc_NodeFullAddress *origin,
                                mc_NodeFullAddress *destination,
                                uint32_t  flags,
                                vector<unsigned char>& payload,
                                uint256   message_hash_to_relay,
                                vector<CScript>&  sigScripts_to_relay,
                                vector<CAddress>& path,
                                CNode*    pfrom, 
                                uint32_t  action);
    
    bool ProcessRelay       (   CNode* pfrom, 
                                CDataStream& vRecv, 
                                CValidationState &state, 
                                uint32_t verify_flags_in);

}     mc_RelayManager;




#endif /* RELAY_H */

