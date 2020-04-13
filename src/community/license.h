// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#ifndef LICENSE_H
#define LICENSE_H

#include "multichain/multichain.h"
#include "utils/serialize.h"
#include "utils/util.h"

#define MC_LIC_NONCE_SIZE                                  16
#define MC_LIC_DEFAULT_UNLOCKED_FEATURES                   0xFFFFFFFFFFFFFFFF


#define MC_ENT_SPRM_LICENSE_CHAIN_PARAMS_HASH  0x01
#define MC_ENT_SPRM_LICENSE_PK_HASH_VERSION    0x08
#define MC_ENT_SPRM_LICENSE_SC_HASH_VERSION    0x09
#define MC_ENT_SPRM_LICENSE_CHECKSUM           0x0a
#define MC_ENT_SPRM_LICENSE_REQUEST_MIN        0x10
#define MC_ENT_SPRM_LICENSE_ADDRESS            0x10
#define MC_ENT_SPRM_LICENSE_ADDRESS_TYPE       0x11
#define MC_ENT_SPRM_LICENSE_MAC_ADDRESS        0x12
#define MC_ENT_SPRM_LICENSE_IP_ADDRESS         0x13
#define MC_ENT_SPRM_LICENSE_NODE_VERSION       0x14
#define MC_ENT_SPRM_LICENSE_PROTOCOL_VERSION   0x15
#define MC_ENT_SPRM_LICENSE_REQUEST_NONCE      0x1D
#define MC_ENT_SPRM_LICENSE_REQUEST_DETAILS    0x1E
#define MC_ENT_SPRM_LICENSE_REQUEST_MAX        0x1F
#define MC_ENT_SPRM_LICENSE_LICENSE_MIN        0x20
#define MC_ENT_SPRM_LICENSE_NONCE              0x20
#define MC_ENT_SPRM_LICENSE_START_TIME         0x21
#define MC_ENT_SPRM_LICENSE_END_TIME           0x22
#define MC_ENT_SPRM_LICENSE_UNLOCKED_FEATURES  0x23
#define MC_ENT_SPRM_LICENSE_FLAGS              0x24
#define MC_ENT_SPRM_LICENSE_PARAMS             0x25
#define MC_ENT_SPRM_LICENSE_GRACE              0x26
#define MC_ENT_SPRM_LICENSE_DEGRADED           0x27
#define MC_ENT_SPRM_LICENSE_DETAILS            0x2E
#define MC_ENT_SPRM_LICENSE_LICENSE_MAX        0x2F
#define MC_ENT_SPRM_LICENSE_INFO_MIN           0x30
#define MC_ENT_SPRM_LICENSE_ENCRYPTED_REQUEST  0x30
#define MC_ENT_SPRM_LICENSE_ORIGINAL_HASH      0x31
#define MC_ENT_SPRM_LICENSE_REQUEST_HASH       0x32
#define MC_ENT_SPRM_LICENSE_EXTENSION_DELAY    0x33
#define MC_ENT_SPRM_LICENSE_EXTENSION_INTERVAL 0x34
#define MC_ENT_SPRM_LICENSE_INFO_MAX           0x3F
#define MC_ENT_SPRM_LICENSE_EKEY               0x56
#define MC_ENT_SPRM_LICENSE_EKEY_TYPE          0x57
#define MC_ENT_SPRM_LICENSE_TRANSFER_METHOD    0x58
#define MC_ENT_SPRM_LICENSE_COUNT              0x5D
#define MC_ENT_SPRM_LICENSE_CONFIRMATION_MIN   0x60
#define MC_ENT_SPRM_LICENSE_CONFIRMATION_MAX   0x6F


#define MC_ECF_LICENSE_PURCHASE_NO_ENCRYPTION             0x00000000
#define MC_ECF_LICENSE_PURCHASE_ENCRYPTION                0x00000001
#define MC_ECF_LICENSE_PURCHASE_METHOD_MASK               0x0000000F
#define MC_ECF_LICENSE_TRANSFER_NO_ENCRYPTION             0x00000000
#define MC_ECF_LICENSE_TRANSFER_ENCRYPTION                0x00000010
#define MC_ECF_LICENSE_TRANSFER_METHOD_MASK               0x000000F0

#define MC_ECF_EKEY_PURPOSE_GENERAL                       0x00000000
#define MC_ECF_EKEY_PURPOSE_LICENSE                       0x00000001

class CLicenseRequest
{
    public:
        std::vector<unsigned char> m_Data;
        std::vector<unsigned char> m_PrivateKey;
        uint32_t m_ReferenceCount;
        
    public:
        CLicenseRequest()
        {
            m_Data.clear();
            m_PrivateKey.clear();
            m_ReferenceCount=0;
        }
        
        void Zero();
        void SetData(mc_Script *script);
        void SetData(const unsigned char* ptr,size_t bytes);
        void SetPrivateKey(const std::vector<unsigned char>& private_key);
        bool Verify();
        bool IsZero();
        const unsigned char *GetParam(uint32_t param,size_t *bytes);
        const unsigned char *GetParamToEnd(uint32_t param,size_t *bytes);
        uint256 GetHash(uint32_t from, uint32_t to,bool add_chain);
        uint256 GetLicenseHash();
        uint256 GetConfirmationNameHash();
        std::string GetLicenseName();
        
    ADD_SERIALIZE_METHODS;
    
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(m_Data);
        READWRITE(m_PrivateKey);
        READWRITE(m_ReferenceCount);
    }
};



#endif /* LICENSE_H */

