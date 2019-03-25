// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#ifndef MULTICHAIN_COMMUNITY_H
#define MULTICHAIN_COMMUNITY_H

#include "multichain/multichain.h"
#include "wallet/wallettxs.h"
#include "rpc/rpcutils.h"

#define MC_EFT_NONE                            0x0000000000000000
#define MC_EFT_LICENSE_TRANSFER                0x0000000000000002
#define MC_EFT_STREAM_CONDITIONAL_INDEXING     0x0000000000000100
#define MC_EFT_STREAM_MANUAL_RETRIEVAL         0x0000000000000200
#define MC_EFT_STREAM_READ_PERMISSIONS         0x0000000000001000
#define MC_EFT_ALL                             0xFFFFFFFFFFFFFFFF


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
    
    int WLT_CreateSubscription(mc_TxEntity *entity,uint32_t retrieve,uint32_t indexes,uint32_t *rescan_mode);
    int WLT_DeleteSubscription(mc_TxEntity *entity,uint32_t rescan_mode);
    int WLT_StartImport();
    int WLT_CompleteImport();
    int WLT_NoIndex(mc_TxEntity *entity);
    int WLT_NoRetrieve(mc_TxEntity *entity);
    
    std::string ENT_Edition();
    int ENT_EditionNumeric();
    int ENT_MinWalletDatVersion();
    void ENT_RPCVerifyEdition();
    std::string ENT_TextConstant(const char* name);
    void ENT_InitRPCHelpMap();
    
    void LIC_RPCVerifyFeature(uint64_t feature);
    bool LIC_VerifyFeature(uint64_t feature,std::string& reason);
//    bool LIC_VerifyConfirmation(uint160 address,void *confirmation, size_t size,std::string& reason);
//    string LIC_LicenseName(void *confirmation, size_t size);
    int LIC_VerifyLicenses();
    Value LIC_RPCDecodeLicenseRequest(const Array& params);
    Value LIC_RPCActivateLicense(const Array& params);
    Value LIC_RPCTransferLicense(const Array& params);
    Value LIC_RPCListLicenseRequests(const Array& params);
    Value LIC_RPCGetLicenseConfirmation(const Array& params);

    
} mc_EnterpriseFeatures;

#endif /* MULTICHAIN_COMMUNITY_H */

