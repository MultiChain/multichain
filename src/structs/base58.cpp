// Copyright (c) 2014 The Bitcoin developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "structs/base58.h"

#include "structs/hash.h"
#include "structs/uint256.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <vector>
#include <string>
#include <boost/variant/apply_visitor.hpp>
#include <boost/variant/static_visitor.hpp>

#include "multichain/multichain.h"


/** All alphanumeric characters except for "0", "I", "O", and "l" */
static const char* pszBase58 = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

bool DecodeBase58(const char* psz, std::vector<unsigned char>& vch)
{
    // Skip leading spaces.
    while (*psz && isspace(*psz))
        psz++;
    // Skip and count leading '1's.
    int zeroes = 0;
    while (*psz == '1') {
        zeroes++;
        psz++;
    }
    // Allocate enough space in big-endian base256 representation.
    std::vector<unsigned char> b256(strlen(psz) * 733 / 1000 + 1); // log(58) / log(256), rounded up.
    // Process the characters.
    while (*psz && !isspace(*psz)) {
        // Decode base58 character
        const char* ch = strchr(pszBase58, *psz);
        if (ch == NULL)
            return false;
        // Apply "b256 = b256 * 58 + ch".
        int carry = ch - pszBase58;
        for (std::vector<unsigned char>::reverse_iterator it = b256.rbegin(); it != b256.rend(); it++) {
            carry += 58 * (*it);
            *it = carry % 256;
            carry /= 256;
        }
        assert(carry == 0);
        psz++;
    }
    // Skip trailing spaces.
    while (isspace(*psz))
        psz++;
    if (*psz != 0)
        return false;
    // Skip leading zeroes in b256.
    std::vector<unsigned char>::iterator it = b256.begin();
    while (it != b256.end() && *it == 0)
        it++;
    // Copy result into output vector.
    vch.reserve(zeroes + (b256.end() - it));
    vch.assign(zeroes, 0x00);
    while (it != b256.end())
        vch.push_back(*(it++));
    return true;
}
std::string EncodeBase58(const unsigned char* pbegin, const unsigned char* pend)
{
    // Skip & count leading zeroes.
    int i,j,k;
    int zeroes = 0;
    while (pbegin != pend && *pbegin == 0) {
        pbegin++;
        zeroes++;
    }
        
    int b58_size=(pend - pbegin) * 138 / 100 + 1;// log(256) / log(58), rounded up.
    int bx_power=5;
    int64_t bx_base=656356768;//58^5    
    int bx_size=(b58_size-1)/bx_power+1;
    int bin_power=4;
    int64_t bin_base=4294967296;
    int bin_size=(pend - pbegin-1)/bin_power + 1;    
    
    // Allocate enough space in big-endian base58 representation.
    std::vector<uint32_t> bin(bin_size); 
    std::vector<unsigned char> b58(b58_size); 
    std::vector<int64_t> bx(bx_size); 
    
    const unsigned char* pfirst=pend-bin_size*bin_power;
    while (pbegin != pend) {
        if(pbegin >= pfirst)
        {
            bin[(pbegin-pfirst)/bin_power] |= (*pbegin) << ((bin_power - 1 - (pbegin-pfirst)%bin_power) * 8);
        }
        pbegin++;
    }
    
    // Process the uint32_ts.
    for(i=0;i<bin_size;i++)
    {
        int64_t carry = bin[i];
        // Apply "b58 = b58 * 256 + ch".
        for (std::vector<int64_t>::reverse_iterator it = bx.rbegin(); it != bx.rend(); it++) {
            carry += bin_base * (*it);
            *it = carry % bx_base;
            carry /= bx_base;
        }
        assert(carry == 0);
//        pbegin++;
    }
    
    k=b58_size;
    i=bx_size;
    j=0;
    int64_t value=0;
    while(k > 0)
    {
        k--;
        if(j == 0)
        {
            i--;
            j=bx_power;
            value=bx[i];            
        }
        b58[k]=(unsigned char)(value%58);
        value/=58;
        j--;
    }
    
    // Skip leading zeroes in base58 result.
    std::vector<unsigned char>::iterator it = b58.begin();
    while (it != b58.end() && *it == 0)
        it++;
    // Translate the result into a string.
    std::string str;
    str.reserve(zeroes + (b58.end() - it));
    str.assign(zeroes, '1');
    while (it != b58.end())
        str += pszBase58[*(it++)];
    return str;
}

std::string EncodeBase58_19(const unsigned char* pbegin, const unsigned char* pend)
{
    // Skip & count leading zeroes.
    int zeroes = 0;
    while (pbegin != pend && *pbegin == 0) {
        pbegin++;
        zeroes++;
    }
    
    int b58_size=(pend - pbegin) * 138 / 100 + 1;// log(256) / log(58), rounded up.
    int bx_power=9;
    int64_t bx_base=195112;//58^3
    bx_base=bx_base*bx_base*bx_base;//58^9
    
    int bx_size=(b58_size-1)/bx_power+1;
    // Allocate enough space in big-endian base58 representation.
    std::vector<unsigned char> b58(b58_size); 
    std::vector<int64_t> bx(bx_size); 
    // Process the bytes.
    while (pbegin != pend) {
        int64_t carry = *pbegin;
        // Apply "b58 = b58 * 256 + ch".
        for (std::vector<int64_t>::reverse_iterator it = bx.rbegin(); it != bx.rend(); it++) {
            carry += 256 * (*it);
            *it = carry % bx_base;
            carry /= bx_base;
        }
        assert(carry == 0);
        pbegin++;
    }
    
    int i,j,k;
    k=b58_size;
    i=bx_size;
    j=0;
    int64_t value=0;
    while(k > 0)
    {
        k--;
        if(j == 0)
        {
            i--;
            j=bx_power;
            value=bx[i];            
        }
        b58[k]=(unsigned char)(value%58);
        value/=58;
        j--;
    }
    
    // Skip leading zeroes in base58 result.
    std::vector<unsigned char>::iterator it = b58.begin();
    while (it != b58.end() && *it == 0)
        it++;
    // Translate the result into a string.
    std::string str;
    str.reserve(zeroes + (b58.end() - it));
    str.assign(zeroes, '1');
    while (it != b58.end())
        str += pszBase58[*(it++)];
    return str;
}


std::string EncodeBase58_11(const unsigned char* pbegin, const unsigned char* pend)
{
    // Skip & count leading zeroes.
    int zeroes = 0;
    while (pbegin != pend && *pbegin == 0) {
        pbegin++;
        zeroes++;
    }
    // Allocate enough space in big-endian base58 representation.
    std::vector<unsigned char> b58((pend - pbegin) * 138 / 100 + 1); // log(256) / log(58), rounded up.
    // Process the bytes.
    while (pbegin != pend) {
        int carry = *pbegin;
        // Apply "b58 = b58 * 256 + ch".
        for (std::vector<unsigned char>::reverse_iterator it = b58.rbegin(); it != b58.rend(); it++) {
            carry += 256 * (*it);
            *it = carry % 58;
            carry /= 58;
        }
        assert(carry == 0);
        pbegin++;
    }
    // Skip leading zeroes in base58 result.
    std::vector<unsigned char>::iterator it = b58.begin();
    while (it != b58.end() && *it == 0)
        it++;
    // Translate the result into a string.
    std::string str;
    str.reserve(zeroes + (b58.end() - it));
    str.assign(zeroes, '1');
    while (it != b58.end())
        str += pszBase58[*(it++)];
    return str;
}

std::string EncodeBase58(const std::vector<unsigned char>& vch)
{
    return EncodeBase58(&vch[0], &vch[0] + vch.size());
}

bool DecodeBase58(const std::string& str, std::vector<unsigned char>& vchRet)
{
    return DecodeBase58(str.c_str(), vchRet);
}

std::string EncodeBase58Check(const std::vector<unsigned char>& vchIn)
{
    // add 4-byte hash check to the end
    std::vector<unsigned char> vch(vchIn);
    uint256 hash = Hash(vch.begin(), vch.end());
    
/* MCHN START */
    int32_t checksum=(int32_t)mc_GetLE(&hash,4);
    checksum ^= (int32_t)mc_gState->m_NetworkParams->GetInt64Param("addresschecksumvalue");
    vch.insert(vch.end(), (unsigned char*)&checksum, (unsigned char*)&checksum + 4);
//    vch.insert(vch.end(), (unsigned char*)&hash, (unsigned char*)&hash + 4);
/* MCHN END */    
//    mc_DumpSize("TS",&vch[0],vch.size(),vch.size());
    return EncodeBase58(vch);
}

bool DecodeBase58Check(const char* psz, std::vector<unsigned char>& vchRet)
{
    if (!DecodeBase58(psz, vchRet) ||
        (vchRet.size() < 4)) {
        vchRet.clear();
        return false;
    }
    // re-calculate the checksum, insure it matches the included 4-byte checksum
    uint256 hash = Hash(vchRet.begin(), vchRet.end() - 4);
    
/* MCHN START */
    int32_t checksum=(int32_t)mc_GetLE(&hash,4);
    checksum ^= (int32_t)mc_gState->m_NetworkParams->GetInt64Param("addresschecksumvalue");
    
    if (memcmp((unsigned char*)&checksum, &vchRet.end()[-4], 4) != 0) {
//    if (memcmp(&hash, &vchRet.end()[-4], 4) != 0) {
        vchRet.clear();
        return false;
    }
/* MCHN END */    
    vchRet.resize(vchRet.size() - 4);
    return true;
}

bool DecodeBase58Check(const std::string& str, std::vector<unsigned char>& vchRet)
{
    return DecodeBase58Check(str.c_str(), vchRet);
}

CBase58Data::CBase58Data()
{
    vchVersion.clear();
    vchData.clear();
}

void CBase58Data::SetData(const std::vector<unsigned char>& vchVersionIn, const void* pdata, size_t nSize)
{
    vchVersion = vchVersionIn;
    vchData.resize(nSize);
    if (!vchData.empty())
        memcpy(&vchData[0], pdata, nSize);
}

void CBase58Data::SetData(const std::vector<unsigned char>& vchVersionIn, const unsigned char* pbegin, const unsigned char* pend)
{
    SetData(vchVersionIn, (void*)pbegin, pend - pbegin);
}

bool CBase58Data::SetString(const char* psz, unsigned int nVersionBytes)
{
    std::vector<unsigned char> vchTemp;
    bool rc58 = DecodeBase58Check(psz, vchTemp);
    if ((!rc58) || (vchTemp.size() < nVersionBytes)) {
        vchData.clear();
        vchVersion.clear();
        return false;
    }
    
/* MCHN START */    
    
/*    
    vchVersion.assign(vchTemp.begin(), vchTemp.begin() + nVersionBytes);
    vchData.resize(vchTemp.size() - nVersionBytes);
    if (!vchData.empty())
        memcpy(&vchData[0], &vchTemp[nVersionBytes], vchData.size());
*/

    int shift=(vchTemp.size() - nVersionBytes) / nVersionBytes;
    vchVersion.resize(nVersionBytes);
    vchData.resize(vchTemp.size() - nVersionBytes);
    for(int i=0;i<(int)nVersionBytes;i++)
    {
        vchVersion[i]=vchTemp[i*(shift+1)];
        int size=shift;
        if(i == (int)(nVersionBytes-1))
        {
            size=(vchTemp.size() - nVersionBytes)-i*shift;
        }
        memcpy(&vchData[i*shift],&vchTemp[i*(shift+1)+1],size);
    }
    
    
/* MCHN END */    
//    OPENSSL_cleanse(&vchTemp[0], vchData.size());
    OPENSSL_cleanse(&vchTemp[0], vchTemp.size());
    return true;
}

bool CBase58Data::SetString(const std::string& str)
{
    return SetString(str.c_str(),Params().Base58Prefix(CChainParams::PUBKEY_ADDRESS).size());// MCHN, 1 (default) in original code
}

std::string CBase58Data::ToString() const
{
/* MCHN START */    
/*
    std::vector<unsigned char> vch = vchVersion;    
    vch.insert(vch.end(), vchData.begin(), vchData.end());
 */
    
    std::vector<unsigned char> vch;    
    int nVersionBytes=vchVersion.size();

    int shift=vchData.size() / nVersionBytes;
    vch.resize(vchData.size() + nVersionBytes);
    for(int i=0;i<nVersionBytes;i++)
    {
        vch[i*(shift+1)]=vchVersion[i];
        int size=shift;
        if(i == nVersionBytes-1)
        {
            size=vchData.size()-i*shift;
        }
        memcpy(&vch[i*(shift+1)+1],&vchData[i*shift],size);
    }
//        mc_DumpSize("TS",&vchVersion[0],vchVersion.size(),vchVersion.size());
    
/* MCHN END */    
    return EncodeBase58Check(vch);
}

std::string BurnAddress(const std::vector<unsigned char>& vchVersion)
{
    std::vector<unsigned char> vch;    
    int nVersionBytes=vchVersion.size();
    int nDataBytes=20;
    int nHashBytes=4;
    unsigned char data[20];
    char test[100];    
    char res[100];    
    
    int shift=nDataBytes / nVersionBytes;
    CKeyID kBurn;
    int p;
    
    if(*(uint160*)(mc_gState->m_BurnAddress) == 0)
    {
        vch.resize(nDataBytes + nVersionBytes + nHashBytes);
        for(int i=2;i<nDataBytes + nVersionBytes + nHashBytes;i++)
        {
            vch[i]=0x00;
        }
        vch[0]=vchVersion[0];
        vch[1]=0x80;


        strcpy(res,EncodeBase58(vch).c_str());
        memset(res+1,'X',strlen(res)-1);
        DecodeBase58(res,vch);
        while((int)vch.size() > nDataBytes + nVersionBytes + nHashBytes)
        {
            res[strlen(res)-1]=0x00;
            DecodeBase58(res,vch);
        }

        p=0;
        while(p<nDataBytes + nVersionBytes)
        {
            if( (p % (shift+1)) == 0)
            {
                if(vch[p] != vchVersion[p / (shift+1)])
                {
                    memset(&vch[p+2],0x00,vch.size()-p-2);
                    vch[p] = vchVersion[p / (shift+1)];
                    vch[p+1] = 0x80;
                    strcpy(test,EncodeBase58(vch).c_str());
                    if(strlen(test) != strlen(res))
                    {
                        if(strlen(test) > strlen(res))
                        {
                            res[strlen(res)+1]=0x00;
                            res[strlen(res)]='X';
                        }                        
                        if(strlen(test) < strlen(res))
                        {
                            res[strlen(res)-1]=0x00;
                        }                        
                    }
                    int j=0;
                    while( (j<(int)strlen(res)) && (res[j] == test[j]) )
                    {
                        j++;
                    }
                    int k=0;
                    while( (k<3) && (j<(int)strlen(res)) && ((vch[p] != vchVersion[p / (shift+1)])  || (k ==0)))
                    {
                        res[j]=test[j];
                        DecodeBase58(res,vch);
                        k++;
                        j++;
                    }                
                }
            }
            p++;
        }

    //    strcpy(test,EncodeBase58(vch).c_str());


        for(int i=0;i<(int)nVersionBytes;i++)
        {
            int size=shift;
            if(i == (int)(nVersionBytes-1))
            {
                size=nDataBytes-i*shift;
            }
            memcpy(data+i*shift,&vch[i*(shift+1)+1],size);
        }

        memcpy(&kBurn,data,nDataBytes);
        memcpy(mc_gState->m_BurnAddress,data,nDataBytes);
    }
    else
    {
        memcpy(&kBurn,mc_gState->m_BurnAddress,nDataBytes);        
    }
  
    CBitcoinAddress ba;
    ba.Set(kBurn,vchVersion);
    return ba.ToString();
}

int CBase58Data::CompareTo(const CBase58Data& b58) const
{
    if (vchVersion < b58.vchVersion)
        return -1;
    if (vchVersion > b58.vchVersion)
        return 1;
    if (vchData < b58.vchData)
        return -1;
    if (vchData > b58.vchData)
        return 1;
    return 0;
}

namespace
{
class CBitcoinAddressVisitor : public boost::static_visitor<bool>
{
private:
    CBitcoinAddress* addr;

public:
    CBitcoinAddressVisitor(CBitcoinAddress* addrIn) : addr(addrIn) {}

    bool operator()(const CKeyID& id) const { return addr->Set(id); }
    bool operator()(const CScriptID& id) const { return addr->Set(id); }
    bool operator()(const CNoDestination& no) const { return false; }
};

} // anon namespace

/* MCHN START */
bool CBitcoinAddress::Set(const CKeyID &id,const std::vector<unsigned char>& vchV)
{
    SetData(vchV, &id, 20);
    return true;    
}
/* MCHN END */


bool CBitcoinAddress::Set(const CKeyID& id)
{
    SetData(Params().Base58Prefix(CChainParams::PUBKEY_ADDRESS), &id, 20);
    return true;
}

bool CBitcoinAddress::Set(const CScriptID& id)
{
    SetData(Params().Base58Prefix(CChainParams::SCRIPT_ADDRESS), &id, 20);
    return true;
}

bool CBitcoinAddress::Set(const CTxDestination& dest)
{
    return boost::apply_visitor(CBitcoinAddressVisitor(this), dest);
}

bool CBitcoinAddress::IsValid() const
{
    return IsValid(Params());
}

bool CBitcoinAddress::IsValid(const CChainParams& params) const
{
    bool fCorrectSize = vchData.size() == 20;
    bool fKnownVersion = vchVersion == params.Base58Prefix(CChainParams::PUBKEY_ADDRESS) ||
                         vchVersion == params.Base58Prefix(CChainParams::SCRIPT_ADDRESS);
    return fCorrectSize && fKnownVersion;
}

CTxDestination CBitcoinAddress::Get() const
{
    if (!IsValid())
        return CNoDestination();
    uint160 id;
    memcpy(&id, &vchData[0], 20);
    if (vchVersion == Params().Base58Prefix(CChainParams::PUBKEY_ADDRESS))
        return CKeyID(id);
    else if (vchVersion == Params().Base58Prefix(CChainParams::SCRIPT_ADDRESS))
        return CScriptID(id);
    else
        return CNoDestination();
}

bool CBitcoinAddress::GetKeyID(CKeyID& keyID) const
{
    if (!IsValid() || vchVersion != Params().Base58Prefix(CChainParams::PUBKEY_ADDRESS))
        return false;
    uint160 id;
    memcpy(&id, &vchData[0], 20);
    keyID = CKeyID(id);
    return true;
}

bool CBitcoinAddress::GetScriptID(CScriptID& scriptID) const
{
    if (!IsValid() || vchVersion != Params().Base58Prefix(CChainParams::SCRIPT_ADDRESS))
        return false;
    uint160 id;
    memcpy(&id, &vchData[0], 20);
    scriptID = CScriptID(id);
    return true;
}

bool CBitcoinAddress::IsScript() const
{
    return IsValid() && vchVersion == Params().Base58Prefix(CChainParams::SCRIPT_ADDRESS);
}

void CBitcoinSecret::SetKey(const CKey& vchSecret)
{
    assert(vchSecret.IsValid());
    SetData(Params().Base58Prefix(CChainParams::SECRET_KEY), vchSecret.begin(), vchSecret.size());
    if (vchSecret.IsCompressed())
        vchData.push_back(1);
}

CKey CBitcoinSecret::GetKey()
{
    CKey ret;
    assert(vchData.size() >= 32);
    ret.Set(vchData.begin(), vchData.begin() + 32, vchData.size() > 32 && vchData[32] == 1);
    return ret;
}

bool CBitcoinSecret::IsValid() const
{
    bool fExpectedFormat = vchData.size() == 32 || (vchData.size() == 33 && vchData[32] == 1);
    bool fCorrectVersion = vchVersion == Params().Base58Prefix(CChainParams::SECRET_KEY);
    return fExpectedFormat && fCorrectVersion;
}

bool CBitcoinSecret::SetString(const char* pszSecret)
{
    return CBase58Data::SetString(pszSecret,Params().Base58Prefix(CChainParams::SECRET_KEY).size()) && IsValid(); // MCHN 1 (default) in original code
}

bool CBitcoinSecret::SetString(const std::string& strSecret)
{
    return SetString(strSecret.c_str());
}
