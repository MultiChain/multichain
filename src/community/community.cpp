// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "community/community.h"

using namespace std;

void mc_EnterpriseFeatures::Zero()
{
    m_Impl=NULL;
}

void mc_EnterpriseFeatures::Destroy()
{
    Zero();
}

int mc_EnterpriseFeatures::Initialize(const char *name,uint32_t mode)
{
    
    return MC_ERR_NOERROR;
}

int mc_EnterpriseFeatures::WLT_CreateSubscription(mc_TxEntity *entity,uint32_t retrieve,uint32_t indexes,uint32_t *rescan_mode)
{
    *rescan_mode=0;
    return MC_ERR_NOERROR;    
}

int mc_EnterpriseFeatures::WLT_DeleteSubscription(mc_TxEntity *entity,uint32_t rescan_mode)
{
    return MC_ERR_NOERROR;        
}

int mc_EnterpriseFeatures::WLT_StartImport()
{
    return MC_ERR_NOERROR;            
}

int mc_EnterpriseFeatures::WLT_CompleteImport()
{
    return MC_ERR_NOERROR;            
}

int mc_EnterpriseFeatures::WLT_NoIndex(mc_TxEntity *entity)
{
    return 0;
}

int mc_EnterpriseFeatures::WLT_NoRetrieve(mc_TxEntity *entity)
{
    return 0;
}

string mc_EnterpriseFeatures::ENT_Edition() 
{
    return "Community";
}

int mc_EnterpriseFeatures::ENT_MinWalletDatVersion()
{
    return 1;
}

void mc_EnterpriseFeatures::ENT_RPCVerifyEdition() 
{
    throw JSONRPCError(RPC_NOT_SUPPORTED, "This feature is available only in Enterprise edition of MultiChain, please call \"help enterprise\" for details");        
}

void mc_EnterpriseFeatures::LIC_RPCVerifyFeature(uint32_t feature)
{
    throw JSONRPCError(RPC_NOT_SUPPORTED, "This feature is available only in Enterprise edition of MultiChain, please call \"help enterprise\" for details");            
}

bool mc_EnterpriseFeatures::LIC_VerifyFeature(uint32_t feature,std::string& reason)
{
    reason="Not available in Community efition";
    return false;
}

int mc_EnterpriseFeatures::LIC_VerifyLicenses()
{
    return MC_ERR_NOERROR;
}

Value mc_EnterpriseFeatures::LIC_RPCDecodeLicenseRequest(const Array& params)
{
    return Value::null;
}

Value mc_EnterpriseFeatures::LIC_RPCActivateLicense(const Array& params)
{
    return Value::null;
}

Value mc_EnterpriseFeatures::LIC_RPCTransferLicense(const Array& params)
{
    return Value::null;
}

Value mc_EnterpriseFeatures::LIC_RPCListLicenseRequests(const Array& params)
{
    return Value::null;
}

Value mc_EnterpriseFeatures::LIC_RPCGetLicenseConfirmation(const Array& params)
{
    return Value::null;
}
