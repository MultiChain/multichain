// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2014-2016 The Bitcoin Core developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "keys/enckey.h"
#include "openssl/rsa.h"
#include "openssl/pem.h"
#include "community/license.h"
#include "community/community.h"
#include "rpc/rpcwallet.h"

bool mc_GetLicenseAddress(CBitcoinAddress &license_address,bool create_new)
{
    if(!create_new)
    {
        BOOST_FOREACH(const PAIRTYPE(CBitcoinAddress, CAddressBookData)& item, pwalletMain->mapAddressBook)
        {
            const CBitcoinAddress& address = item.first;
            if(item.second.purpose == "license")
            {
                license_address=address;
                return true;
            }
        }    
    }
    
    if (!pwalletMain->IsLocked())
        pwalletMain->TopUpKeyPool();

    // Generate a new key that is added to wallet
    CPubKey newKey;
    if (!pwalletMain->GetKeyFromPool(newKey))
        throw JSONRPCError(RPC_WALLET_KEYPOOL_RAN_OUT, "Error: Keypool ran out, please call keypoolrefill first");
    CKeyID keyID = newKey.GetID();

    pwalletMain->SetAddressBook(keyID, "", "license");

    if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)
    {
        mc_TxEntity entity;
        entity.Zero();
        memcpy(entity.m_EntityID,&keyID,MC_TDB_ENTITY_ID_SIZE);
        entity.m_EntityType=MC_TET_PUBKEY_ADDRESS | MC_TET_CHAINPOS;
        pwalletTxsMain->AddEntity(&entity,MC_EFL_NOT_IN_LISTS);
        entity.m_EntityType=MC_TET_PUBKEY_ADDRESS | MC_TET_TIMERECEIVED;
        pwalletTxsMain->AddEntity(&entity,MC_EFL_NOT_IN_LISTS);
    }
    
    license_address=CBitcoinAddress(keyID);
    
    return true;
}

bool CEncryptionKey::Generate(uint32_t type)
{
    switch(type)
    {
        case MC_ECF_ASYMMETRIC_ENCRYPTION_RSA_2048_DER:
        case MC_ECF_ASYMMETRIC_ENCRYPTION_RSA_2048_PEM:
            break;
        default:
            return false;
    }
    
    m_Type=type;

    int ret=1;
    RSA *rsa = NULL;
    BIGNUM *bne = NULL;
    BIO *pubkey = NULL;
    BIO *privkey = NULL;
 
    int bits = 2048;
    unsigned long   e = RSA_F4;
 
    
    if(ret == 1)
    {
        bne = BN_new();
        ret=BN_set_word(bne,e);
    }
    
    if(ret == 1)
    {
        rsa = RSA_new();
        ret=RSA_generate_key_ex(rsa, bits, bne, NULL);
    }

    if(ret == 1)
    {
        int nt=0;
        privkey = BIO_new(BIO_s_mem());    
        pubkey = BIO_new(BIO_s_mem());

        if(type == MC_ECF_ASYMMETRIC_ENCRYPTION_RSA_2048_PEM)
        {
            PEM_write_bio_RSAPrivateKey(privkey, rsa, NULL, NULL, 0, NULL, NULL);
            PEM_write_bio_RSAPublicKey(pubkey, rsa);
            nt=1;
        }
        if(type == MC_ECF_ASYMMETRIC_ENCRYPTION_RSA_2048_DER)
        {
            i2d_RSAPrivateKey_bio(privkey, rsa);
            i2d_RSAPublicKey_bio(pubkey, rsa);
        }       

        size_t prilen=BIO_pending(privkey);
        size_t publen=BIO_pending(pubkey);

        m_PrivateKey.resize(prilen+nt);
        m_PublicKey.resize(publen+nt);

        BIO_read(privkey, &m_PrivateKey[0], prilen);
        BIO_read(pubkey, &m_PublicKey[0], publen);

        if(nt)
        {
            m_PrivateKey[prilen]=0x00;
            m_PublicKey[publen]=0x00;
        }
        
//        mc_DumpSize("Private key",&m_PrivateKey[0],prilen+nt,64);
//        mc_DumpSize("Public key",&m_PublicKey[0],publen+nt,64);
//        printf("%s\n",(char*)&m_PrivateKey[0]);
//        printf("%s\n",(char*)&m_PublicKey[0]);
    }    

    BIO_free_all(pubkey);
    BIO_free_all(privkey);
    RSA_free(rsa);
    BN_free(bne);
 
    if(ret != 1)
    {
        return false;
    }
    
    return true;
}

bool mc_GetLicenseEncryptionKey(vector<unsigned char> &vEncryptionPublicKey,uint32_t *encryption_type,bool create_new)
{
    if(!create_new)
    {
        BOOST_FOREACH(const PAIRTYPE(uint256, CEncryptionKey)& item, pwalletMain->mapEKeys)
        {
            if(item.second.m_Purpose & MC_ECF_EKEY_PURPOSE_LICENSE)
            {
                vEncryptionPublicKey=item.second.m_PublicKey;
                *encryption_type=item.second.m_Type;
                return true;                
            }
        }    
    }
    
    CEncryptionKey full_key;
    
    full_key.m_Purpose=MC_ECF_EKEY_PURPOSE_LICENSE;
    *encryption_type=MC_ECF_ASYMMETRIC_ENCRYPTION_RSA_2048_PEM;
    if(!full_key.Generate(*encryption_type))
    {
        return false;
    }
    
    uint256 hash;
    mc_gState->m_TmpBuffers->m_RpcHasher1->DoubleHash(&(full_key.m_PublicKey[0]),full_key.m_PublicKey.size(),&hash);
    
    pwalletMain->SetEKey(hash,full_key);
    
    vEncryptionPublicKey=full_key.m_PublicKey;
    return true;
}

CLicenseRequest mc_GetLicenseRequest(int *errorCode,string *strError)
{
    *errorCode=0;
    *strError="";
    
    CLicenseRequest license_request;
    
    CBitcoinAddress license_address;
    uint32_t encryption_type;
    unsigned char* stored_param;
    int param_size;
    uint32_t value32;
    unsigned char nonce[MC_LIC_NONCE_SIZE];
    
    if(!mc_GetLicenseAddress(license_address,false))
    {
        *errorCode=RPC_INTERNAL_ERROR;
        *strError="Cannot create license address";
        return license_request;
    }
    
    CEncryptionKey full_key;
    
    full_key.m_Purpose=MC_ECF_EKEY_PURPOSE_LICENSE;
    encryption_type=MC_ECF_ASYMMETRIC_ENCRYPTION_RSA_2048_DER;
    if(!full_key.Generate(encryption_type))
    {
        *errorCode=RPC_INTERNAL_ERROR;
        *strError="Cannot create encryption key";
        return license_request;
    }
/*    
    if(!mc_GetLicenseEncryptionKey(vEncryptionPublicKey,&encryption_type,false))
    {
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Cannot create encryption key");                
    }
*/    
    mc_Script *lpScript=mc_gState->m_TmpBuffers->m_LicenseTmpBuffer;
    lpScript->Clear();
    lpScript->AddElement();
    
    GetRandBytes(nonce,MC_LIC_NONCE_SIZE);
    lpScript->SetSpecialParamValue(MC_ENT_SPRM_LICENSE_NONCE,nonce,MC_LIC_NONCE_SIZE);    
    
    stored_param=(unsigned char*)mc_gState->m_NetworkParams->GetParam("chainparamshash",&param_size);
    lpScript->SetSpecialParamValue(MC_ENT_SPRM_LICENSE_CHAIN_PARAMS_HASH,stored_param,param_size);    
    
    stored_param=(unsigned char*)mc_gState->m_NetworkParams->GetParam("addresspubkeyhashversion",&param_size);
    lpScript->SetSpecialParamValue(MC_ENT_SPRM_LICENSE_PK_HASH_VERSION,stored_param,param_size);    

    stored_param=(unsigned char*)mc_gState->m_NetworkParams->GetParam("addressscripthashversion",&param_size);
    lpScript->SetSpecialParamValue(MC_ENT_SPRM_LICENSE_SC_HASH_VERSION,stored_param,param_size);    

    stored_param=(unsigned char*)mc_gState->m_NetworkParams->GetParam("addresschecksumvalue",&param_size);
    lpScript->SetSpecialParamValue(MC_ENT_SPRM_LICENSE_CHECKSUM,stored_param,param_size);    
    
    CTxDestination addressRet=license_address.Get();        
    const CKeyID *lpKeyID=boost::get<CKeyID> (&addressRet);
    
    
    stored_param=(unsigned char*)lpKeyID;
    param_size=sizeof(uint160);
    lpScript->SetSpecialParamValue(MC_ENT_SPRM_LICENSE_ADDRESS,stored_param,param_size);    
    
    value32=0;
    stored_param=(unsigned char*)&value32;
    param_size=1;
    lpScript->SetSpecialParamValue(MC_ENT_SPRM_LICENSE_ADDRESS_TYPE,stored_param,param_size);    
    
    unsigned char *lpMACList;
 
    if(lpMACList)
    {
        if(__US_FindMacServerAddress(&lpMACList,NULL) == MC_ERR_NOERROR)
        {
            int k,n,l,c;
            stored_param=NULL;
            param_size=6;
            n=lpMACList[0];
            n=n*256+lpMACList[1];


            for(k=0;k<n;k++)
            {
                if(stored_param == NULL)
                {
                    c=0;
                    for(l=1;l<6;l++)
                    {
                        if(lpMACList[2+k*6+l] == lpMACList[2+k*6+l-1])c++;
                    }
                    if(c != 5)
                    {
                        stored_param=lpMACList+2+k*6;
                    }
                }
            }
            if(stored_param)
            {
                lpScript->SetSpecialParamValue(MC_ENT_SPRM_LICENSE_MAC_ADDRESS,stored_param,param_size);    

            }
        }
        delete [] lpMACList;        
    }
    
        
    stored_param=(unsigned char*)(&mc_gState->m_IPv4Address);
    param_size=sizeof(uint32_t);
    lpScript->SetSpecialParamValue(MC_ENT_SPRM_LICENSE_IP_ADDRESS,stored_param,param_size);    
    
    value32=mc_gState->GetNumericVersion();
    stored_param=(unsigned char*)&value32;
    param_size=sizeof(uint32_t);
    lpScript->SetSpecialParamValue(MC_ENT_SPRM_LICENSE_NODE_VERSION,stored_param,param_size);    

    value32=mc_gState->GetProtocolVersion();
    stored_param=(unsigned char*)&value32;
    param_size=sizeof(uint32_t);
    lpScript->SetSpecialParamValue(MC_ENT_SPRM_LICENSE_PROTOCOL_VERSION,stored_param,param_size);    
    
    stored_param=&(full_key.m_PublicKey[0]);//vEncryptionPublicKey[0];
    param_size=full_key.m_PublicKey.size();//vEncryptionPublicKey.size();
    lpScript->SetSpecialParamValue(MC_ENT_SPRM_LICENSE_EKEY,stored_param,param_size);    
    
    
    stored_param=(unsigned char*)&encryption_type;
    param_size=1;
    lpScript->SetSpecialParamValue(MC_ENT_SPRM_LICENSE_EKEY_TYPE,stored_param,param_size);    
    
    value32=MC_ECF_LICENSE_PURCHASE_ENCRYPTION | MC_ECF_LICENSE_TRANSFER_ENCRYPTION;
    stored_param=(unsigned char*)&value32;
    param_size=sizeof(uint32_t);
    lpScript->SetSpecialParamValue(MC_ENT_SPRM_LICENSE_TRANSFER_METHOD,stored_param,param_size);    

    GetRandBytes(nonce,MC_LIC_NONCE_SIZE);
    lpScript->SetSpecialParamValue(MC_ENT_SPRM_LICENSE_REQUEST_NONCE,nonce,MC_LIC_NONCE_SIZE);    
    
    license_request.SetData(lpScript);   
    license_request.SetPrivateKey(full_key.m_PrivateKey);
    license_request.m_ReferenceCount=0;
    
    return license_request;
}

Value getlicenserequest(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 0)
        mc_ThrowHelpMessage("getlicenserequest");        
    
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
    
//    return license_address.ToString().c_str();
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
    if (fHelp || params.size() > 1)
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
    if (fHelp || params.size() > 2)
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

