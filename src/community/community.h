// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#ifndef MULTICHAIN_COMMUNITY_H
#define MULTICHAIN_COMMUNITY_H

#include "multichain/multichain.h"
#include "wallet/wallettxs.h"
#include "rpc/rpcutils.h"

#define MC_EFT_NONE                                 0x0000000000000000
#define MC_EFT_STREAM_SELECTIVE_INDEX               0x0000000000000100
#define MC_EFT_STREAM_OFFCHAIN_SELECTIVE_RETRIEVE   0x0000000000000200
#define MC_EFT_STREAM_OFFCHAIN_SELECTIVE_PURGE      0x0000000000000400
#define MC_EFT_STREAM_READ_RESTRICTED_READ          0x0000000000001000
#define MC_EFT_STREAM_READ_RESTRICTED_WRITE         0x0000000000002000
#define MC_EFT_STREAM_READ_RESTRICTED_DELIVER       0x0000000000004000
#define MC_EFT_NETWORK_SIGNED_RECEIVE               0x0000000000010000
#define MC_EFT_NETWORK_SIGNED_SEND                  0x0000000000020000
#define MC_EFT_ALL                                  0xFFFFFFFFFFFFFFFF


typedef struct mc_EnterpriseFeatures
{
    void * m_Impl;
    
    mc_EnterpriseFeatures()
    {
        Zero();
    }
    
    ~mc_EnterpriseFeatures()
    {
        Destroy();
    }
    
    void Zero();
    void Destroy();
    int  Initialize(              
            const char *name,                                                   // Chain name
            uint32_t mode);                                                     // Unused    
    
    int STR_CreateSubscription(mc_TxEntity *entity,const std::string parameters);
    uint32_t STR_CheckAutoSubscription(const std::string parameters,bool check_license);
    int STR_CreateAutoSubscription(mc_TxEntity *entity);
    int STR_TrimSubscription(mc_TxEntity *entity,const std::string parameters);
    int STR_IsIndexSkipped(mc_TxImport *import,mc_TxEntity *parent_entity,mc_TxEntity *entity);
    int STR_NoRetrieve(mc_TxEntity *entity);
    int STR_IsOutOfSync(mc_TxEntity *entity);
    int STR_SetSyncFlag(mc_TxEntity *entity,bool confirm);
    int STR_GetSubscriptions(mc_Buffer *subscriptions);
    int STR_PutSubscriptions(mc_Buffer *subscriptions);
    Value STR_RPCRetrieveStreamItems(const Array& params);
    Value STR_RPCPurgeStreamItems(const Array& params);    
    Value STR_RPCPurgePublishedItems(const Array& params);
    int STR_RemoveDataFromFile(int fHan, uint32_t from, uint32_t size, uint32_t mode);
    
    bool OFF_ProcessChunkRequest(unsigned char *ptrStart,unsigned char *ptrEnd,vector<unsigned char>* payload_response,vector<unsigned char>* payload_relay,
        map<uint160,int>& mapReadPermissionedStreams,string& strError);
    bool OFF_ProcessChunkResponse(mc_RelayRequest *request,mc_RelayResponse *response,map <int,int>* request_pairs,mc_ChunkCollector* collector,string& strError);
    bool OFF_GetScriptsToVerify(map<uint160,int>& mapReadPermissionedStreams,vector<CScript>& vSigScriptsIn,vector<CScript>& vSigScriptsToVerify,string& strError);
    bool OFF_VerifySignatureScripts(uint32_t  msg_type,mc_OffchainMessageID& msg_id,mc_OffchainMessageID& msg_id_to_respond,uint32_t  flags,
            vector<unsigned char>& vPayload,vector<CScript>& vSigScriptsToVerify,string& strError,int& dos_score);
    bool OFF_CreateSignatureScripts(uint32_t  msg_type,mc_OffchainMessageID& msg_id,mc_OffchainMessageID& msg_id_to_respond,uint32_t  flags,
            vector<unsigned char>& vPayload,set<CPubKey>& vAddresses,vector<CScript>& vSigScripts,string& strError);
    bool OFF_GetPayloadForReadPermissioned(vector<unsigned char>* payload,int *ef_cache_id,string& strError);
    void OFF_FreeEFCache(int ef_cache_id);
    unsigned char* OFF_SupportedEnterpriseFeatures(unsigned char* min_ef,int min_ef_size,int *ef_size);
    
    CPubKey WLT_FindReadPermissionedAddress(mc_EntityDetails* entity);
    int WLT_CreateSubscription(mc_TxEntity *entity,uint32_t retrieve,uint32_t indexes,uint32_t *rescan_mode);
    int WLT_DeleteSubscription(mc_TxEntity *entity,uint32_t rescan_mode);
    int WLT_StartImport();
    int WLT_CompleteImport();
    int WLT_NoIndex(mc_TxEntity *entity);
    int WLT_NoRetrieve(mc_TxEntity *entity);
    
    std::string ENT_Edition();
    int ENT_EditionNumeric();
    int ENT_BuildVersion();
    int ENT_MinWalletDatVersion();
    void ENT_RPCVerifyEdition(std::string message);
    std::string ENT_TextConstant(const char* name);
    void ENT_InitRPCHelpMap();
    void ENT_MaybeStop();
    
    void LIC_RPCVerifyFeature(uint64_t feature,std::string message);
    bool LIC_VerifyFeature(uint64_t feature,std::string& reason);
//    bool LIC_VerifyConfirmation(uint160 address,void *confirmation, size_t size,std::string& reason);
//    string LIC_LicenseName(void *confirmation, size_t size);
    int LIC_VerifyLicenses(int block);
    int LIC_VerifyUpdateCoin(int block,mc_Coin *coin,bool is_new);
    std::vector <std::string> LIC_LicensesWithStatus(std::string status);
    Value LIC_RPCDecodeLicenseRequest(const Array& params);
    Value LIC_RPCDecodeLicenseConfirmation(const Array& params);
    Value LIC_RPCActivateLicense(const Array& params);
    Value LIC_RPCTransferLicense(const Array& params);
    Value LIC_RPCListLicenseRequests(const Array& params);
    Value LIC_RPCGetLicenseConfirmation(const Array& params);
    Value LIC_RPCTakeLicense(const Array& params);

    
} mc_EnterpriseFeatures;

#endif /* MULTICHAIN_COMMUNITY_H */

