// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2014-2016 The Bitcoin Core developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "community/community.h"
#include "rpc/rpcwallet.h"

Value getlicenserequest(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        mc_ThrowHelpMessage("getlicenserequest");        
    
    pEF->ENT_RPCVerifyEdition("getlicenserequest API");
    
    return pEF->LIC_RPCGetLicenseRequest(params);    
/*    
    if((mc_gState->m_WalletMode & MC_WMD_TXS) == 0)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "API is not supported with this wallet version. To get this functionality, run \"multichaind -walletdbversion=2 -rescan\" ");        
    }   
    
    int errCode;
    string strError;
    
    CLicenseRequest license_request=mc_GetLicenseRequest(&errCode,&strError);
    
    if(strError.size())
    {
        throw JSONRPCError(errCode,strError);                        
    }
    
    if(!pwalletMain->SetLicenseRequest(license_request.GetLicenseHash(),license_request,0))
    {
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Cannot save license request");                        
    }
    
    return HexStr(license_request.m_Data);    
*/    

}

Value decodelicenserequest(const json_spirit::Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error("Help message not found\n");
    
    pEF->ENT_RPCVerifyEdition("decodelicenserequest API");
    
    return pEF->LIC_RPCDecodeLicenseRequest(params);    
}

Value decodelicenseconfirmation(const json_spirit::Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error("Help message not found\n");
    
    pEF->ENT_RPCVerifyEdition("decodelicenseconfirmation API");
    
    return pEF->LIC_RPCDecodeLicenseConfirmation(params);    
}

Value activatelicense(const json_spirit::Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error("Help message not found\n");
    
    pEF->ENT_RPCVerifyEdition("activatelicense API");
    
    Array ext_params;
    ext_params.push_back("*");
    BOOST_FOREACH(const Value& value, params)
    {
        ext_params.push_back(value);
    }
    
    return activatelicensefrom(ext_params,false);
    
}

Value activatelicensefrom(const json_spirit::Array& params, bool fHelp)
{
    if (fHelp || params.size() != 2)
        throw runtime_error("Help message not found\n");
    
    pEF->ENT_RPCVerifyEdition("activatelicensefrom API");
    
    return pEF->LIC_RPCActivateLicense(params);        
}

Value transferlicense(const json_spirit::Array& params, bool fHelp)
{
    if (fHelp || params.size() != 2)
        throw runtime_error("Help message not found\n");
    
    pEF->ENT_RPCVerifyEdition("transferlicense API");
    
    return pEF->LIC_RPCTransferLicense(params);            
}

Value listlicenses(const json_spirit::Array& params, bool fHelp)
{
    if (fHelp || params.size() > 2)
        throw runtime_error("Help message not found\n");
    
    pEF->ENT_RPCVerifyEdition("listlicenses API");
    
    return pEF->LIC_RPCListLicenseRequests(params);                
}

Value getlicenseconfirmation(const json_spirit::Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error("Help message not found\n");
    
    pEF->ENT_RPCVerifyEdition("getlicenseconfirmation API");
    
    return pEF->LIC_RPCGetLicenseConfirmation(params);                    
}

Value takelicense(const json_spirit::Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error("Help message not found\n");
    
    pEF->ENT_RPCVerifyEdition("takelicense API");
    
    return pEF->LIC_RPCTakeLicense(params);            
}

Value importlicenserequest(const json_spirit::Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error("Help message not found\n");
    
    pEF->ENT_RPCVerifyEdition("importlicenserequest API");
    
    return pEF->LIC_RPCImportLicenseRequest(params);            
}

