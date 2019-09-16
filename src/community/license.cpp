// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "community/license.h"

void CLicenseRequest::Zero()
{
    m_Data.clear();
}

bool CLicenseRequest::IsZero()
{
    return (m_Data.size() == 0);
}

void CLicenseRequest::SetData(mc_Script *script)
{
    const unsigned char* ptr;
    size_t bytes;
    
    ptr=script->GetData(0,&bytes);   
    SetData(ptr,bytes);
}

void CLicenseRequest::SetPrivateKey(const std::vector<unsigned char>& private_key)
{
    m_PrivateKey=private_key;
}

void CLicenseRequest::SetData(const unsigned char* ptr,size_t bytes)
{
    m_Data.clear();
    
    m_Data.resize(bytes);
    memcpy(&m_Data[0],ptr,bytes);
}

const unsigned char *CLicenseRequest::GetParam(uint32_t param,size_t *bytes)
{
    uint32_t offset,param_offset;
    size_t src_bytes;
    const unsigned char *ptr;

    src_bytes=m_Data.size();
    *bytes=0;
    if(src_bytes == 0)
    {
        return NULL;
    }
    
    ptr=&m_Data[0];
    
    offset=mc_FindSpecialParamInDetailsScriptFull(ptr,src_bytes,param,bytes,&param_offset);
    if(param_offset == src_bytes)
    {
        return NULL;
    }
    
    return ptr+offset;
}

const unsigned char *CLicenseRequest::GetParamToEnd(uint32_t param,size_t *bytes)
{
    uint32_t param_offset;
    size_t src_bytes;
    const unsigned char *ptr;

    src_bytes=m_Data.size();
    *bytes=0;
    if(src_bytes == 0)
    {
        return NULL;
    }
    
    ptr=&m_Data[0];
    
    mc_FindSpecialParamInDetailsScriptFull(ptr,src_bytes,param,bytes,&param_offset);
    if(param_offset == src_bytes)
    {
        return NULL;
    }
    
    *bytes=src_bytes-param_offset;
    return ptr+param_offset;
}

bool CLicenseRequest::Verify()
{
    uint32_t offset,next_offset,param_value_start;
    size_t param_value_size;        
    size_t src_bytes;
    size_t value_sizes[256];
    int value_starts[256];
    unsigned char code;
    const unsigned char *ptr;
    ptr=&m_Data[0];
    src_bytes=m_Data.size();
    
    if(src_bytes == 0)
    {
        return true;
    }
    
    memset(value_sizes,0,256*sizeof(size_t));
    memset(value_starts,0,256*sizeof(int));
    
    offset=0;
            
    while(offset<src_bytes)
    {
        next_offset=mc_GetParamFromDetailsScript(ptr,src_bytes,offset,&param_value_start,&param_value_size);
        if(param_value_start > 0)
        {
            if(ptr[offset] == 0)
            {                
                code=ptr[offset+1];
                if(value_starts[code])
                {
                    return false;                                                                                                                                
                }
                value_sizes[code]=param_value_size;
                value_starts[code]=param_value_start;
            }
        }
        offset=next_offset;
    }
    
    return true;
}

uint256 CLicenseRequest::GetHash(uint32_t from, uint32_t to,bool add_chain)
{
    uint256 hash=0;
    uint32_t offset,param_offset;
    size_t src_bytes,bytes;
    const unsigned char *ptr;
    
    mc_Script *lpScript=mc_gState->m_TmpBuffers->m_LicenseTmpBufferForHash;
    
    src_bytes=m_Data.size();
    if(src_bytes == 0)
    {
        return hash;
    }
    
    ptr=&m_Data[0];
    
    lpScript->Clear();
    lpScript->AddElement();
    
    if(add_chain)
    {
        offset=mc_FindSpecialParamInDetailsScriptFull(ptr,src_bytes,MC_ENT_SPRM_LICENSE_CHAIN_PARAMS_HASH,&bytes,&param_offset);
        if(param_offset < src_bytes)
        {
            if(bytes)
            {
                lpScript->SetData(ptr+param_offset,bytes+offset-param_offset);
            }
        }                
    }
    
    for(int i=from;i<=to;i++)
    {
        offset=mc_FindSpecialParamInDetailsScriptFull(ptr,src_bytes,i,&bytes,&param_offset);
        if(param_offset < src_bytes)
        {
            if(bytes)
            {
                lpScript->SetData(ptr+param_offset,bytes+offset-param_offset);
            }
        }        
    }
    
    ptr=lpScript->GetData(0,&bytes);
    
    mc_gState->m_TmpBuffers->m_RpcHasher1->DoubleHash(ptr,bytes,&hash);
    
    return hash;
}

uint256 CLicenseRequest::GetLicenseHash()
{
    return GetHash(MC_ENT_SPRM_LICENSE_LICENSE_MIN,MC_ENT_SPRM_LICENSE_LICENSE_MAX,true);
}

uint256 CLicenseRequest::GetConfirmationNameHash()
{
    return GetHash(MC_ENT_SPRM_LICENSE_CONFIRMATION_MIN,MC_ENT_SPRM_LICENSE_CONFIRMATION_MAX,false);    
}

std::string CLicenseRequest::GetLicenseName()
{
    uint256 hash=GetConfirmationNameHash();
    unsigned char *ptr=(unsigned char *)&hash;
    
    return strprintf("license-%02x%02x-%02x%02x-%02x%02x-%02x%02x",ptr[31],ptr[30],ptr[29],ptr[28],ptr[27],ptr[26],ptr[25],ptr[24]);
}
