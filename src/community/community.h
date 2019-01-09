// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#ifndef MULTICHAIN_COMMUNITY_H
#define MULTICHAIN_COMMUNITY_H

#include "multichain/multichain.h"
#include "wallet/wallettxdb.h"
#include "rpc/rpcutils.h"

#define MC_EFT_NONE                            0x0000000000000000
#define MC_EFT_LICENSE_TRANSFER                0x0000000000000002


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
    
    int WLT_CreateSubscription(mc_TxEntity *entity,uint32_t retrieve,uint32_t indexes,uint32_t *rescan_mode);
    int WLT_DeleteSubscription(mc_TxEntity *entity,uint32_t rescan_mode);
    int WLT_StartImport();
    int WLT_CompleteImport();
    int WLT_NoIndex(mc_TxEntity *entity);
    int WLT_NoRetrieve(mc_TxEntity *entity);
    
    std::string ENT_Edition();
    int ENT_MinWalletDatVersion();
    void ENT_RPCVerifyEdition();
    
    void LIC_RPCVerifyFeature(uint32_t feature);
    bool LIC_VerifyFeature(uint32_t feature,std::string& reason);
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

