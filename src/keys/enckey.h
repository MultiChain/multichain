// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#ifndef ENCKEY_H
#define ENCKEY_H

#include "utils/serialize.h"
#include "utils/util.h"

#define MC_ECF_EKEY_PURPOSE_GENERAL                       0x00000000
#define MC_ECF_EKEY_PURPOSE_LICENSE                       0x00000001

#define MC_ECF_ASYMMETRIC_NONE                            0x00000000
#define MC_ECF_ASYMMETRIC_ENCRYPTION_RSA_2048_DER         0x00000001
#define MC_ECF_ASYMMETRIC_ENCRYPTION_RSA_2048_PEM         0x00000002

class CEncryptionKey
{
    public:
        uint32_t m_Type;
        uint32_t m_Purpose;
        std::vector<unsigned char> m_PrivateKey;
        std::vector<unsigned char> m_PublicKey;
        std::vector<unsigned char> m_Details;
        
    public:
        CEncryptionKey()
        {
            m_Type=MC_ECF_ASYMMETRIC_NONE;
            m_Purpose=MC_ECF_EKEY_PURPOSE_GENERAL;
            m_PrivateKey.clear();
            m_PublicKey.clear();
            m_Details.clear();
        }
        bool Generate(uint32_t type);
        
    ADD_SERIALIZE_METHODS;
    
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(m_Type);
        READWRITE(m_Purpose);
        READWRITE(m_PrivateKey);
        READWRITE(m_PublicKey);        
        READWRITE(m_Details);
    }
    
        
};


#endif /* ENCKEY_H */

