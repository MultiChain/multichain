// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#ifndef MULTICHAINSCRIPT_H
#define	MULTICHAINSCRIPT_H

#include "utils/declare.h"

#define MC_SCR_TYPE_SCRIPTPUBKEY                      0
#define MC_SCR_TYPE_SCRIPTSIG                         1
#define MC_SCR_TYPE_SCRIPTSIGRAW                      2

#define MC_SCR_ASSET_SCRIPT_TYPE_TRANSFER              0x00000001
#define MC_SCR_ASSET_SCRIPT_TYPE_FOLLOWON              0x00000002

#define MC_SCR_DATA_FORMAT_UNKNOWN                 0x00 
#define MC_SCR_DATA_FORMAT_UTF8                    0x01 
#define MC_SCR_DATA_FORMAT_UBJSON                  0x02 
#define MC_SCR_DATA_FORMAT_EXTENDED_MASK           0x80 


typedef struct mc_Script
{    
    int m_Size;
    int m_NumElements;
    int m_CurrentElement;
    unsigned char* m_lpData;
    int *m_lpCoord;
    int m_AllocElements;
    int m_AllocSize;
    int m_ScriptType;
    uint32_t m_Restrictions;
    
    mc_Script()
    {
        Zero();
    }

    ~mc_Script()
    {
        Destroy();
    }
    
    int Zero();
    int Destroy();    
    int Resize(size_t bytes,int elements);
    
    int SetScript(const unsigned char* src,const size_t bytes,int type);
    int IsOpReturnScript();
    int IsDirtyOpReturnScript();
    int Clear();
    
    int GetNumElements();
    int AddElement();
    int DeleteElement(int element);
    int SetSpecialParamValue(unsigned char param,const unsigned char* param_value,const size_t param_value_size);
    int SetParamValue(const char *param_name,const size_t param_name_size,const unsigned char* param_value,const size_t param_value_size);
    size_t GetParamValue(const unsigned char *ptr,size_t total,size_t offset,size_t* param_value_start,size_t *bytes);
    int SetData(const unsigned char* src,const size_t bytes);
    const unsigned char* GetData(int element,size_t *bytes);

    int GetSize();
    int ShrinkCurrentElementSizeBy(int size);
    
    int GetElement();
    int SetElement(int element);
    
    int GetEntity(unsigned char *short_txid);
    int SetEntity(const unsigned char *short_txid);

    int GetNewEntityType(uint32_t *type);
    int SetNewEntityType(const uint32_t type);

    int GetApproval(uint32_t *approval,uint32_t *timestamp);
    int SetApproval( uint32_t approval,uint32_t timestamp);
    
    int GetNewEntityType(uint32_t *type,int *update,unsigned char* script,int *script_size);
    int SetNewEntityType(const uint32_t type,const int update,const unsigned char* script,int script_size);

    int GetItemKey(unsigned char *key,int *key_size);
    int SetItemKey(const unsigned char* key,int key_size);
    
    int GetPermission(uint32_t *type,uint32_t *from,uint32_t *to,uint32_t *timestamp);
    int SetPermission(uint32_t type,uint32_t from,uint32_t to,uint32_t timestamp);
    
    int GetBlockSignature(unsigned char* sig,int *sig_size,uint32_t *hash_type,unsigned char* key,int *key_size);
    int SetBlockSignature(const unsigned char* sig,int sig_size,uint32_t hash_type,const unsigned char* key,int key_size);
        
    int GetAssetGenesis(int64_t *quantity);
    int SetAssetGenesis(int64_t quantity);
    
    int GetAssetDetails(char* name,int* multiple,unsigned char* script,int *script_size);
    int SetAssetDetails(const char*name,int multiple,const unsigned char* script,int script_size);
    
    int GetGeneralDetails(unsigned char* script,int *script_size);
    int SetGeneralDetails(const unsigned char* script,int script_size);
    
    int GetExtendedDetails(unsigned char **script,size_t *script_size);
    int SetExtendedDetails(const unsigned char* script,size_t script_size);
    
    int GetAssetQuantities(mc_Buffer *amounts,uint32_t script_type);
    int SetAssetQuantities(mc_Buffer *amounts,uint32_t script_type);

    int GetFullRef(unsigned char *ref,uint32_t *script_type);
    
    int GetCachedScript(int offset, int *next_offset, int* vin, unsigned char** script, int *script_size);
    int SetCachedScript(int offset, int *next_offset, int vin, unsigned char* script, int script_size);

    int GetRawData(unsigned char **data,int *size);
    int SetRawData(const unsigned char *data,const int size);
    
    int GetDataFormat(uint32_t *format);
    int SetDataFormat(const uint32_t format);
    
    int GetChunkDef(uint32_t *format,unsigned char** hashes,int *chunk_count,int64_t *total_size);
    int GetChunkDef(uint32_t *format,unsigned char** hashes,int *chunk_count,int64_t *total_size,uint32_t *salt_size,int check_sizes);
    int SetChunkDefHeader(const uint32_t format,int chunk_count,uint32_t salt_size);
    int SetChunkDefHash(unsigned char *hash,int size);
    
    int ExtractAndDeleteDataFormat(uint32_t *format);
    int ExtractAndDeleteDataFormat(uint32_t *format,unsigned char** hashes,int *chunk_count,int64_t *total_size);
    int ExtractAndDeleteDataFormat(uint32_t *format,unsigned char** hashes,int *chunk_count,int64_t *total_size,uint32_t *salt_size,int check_sizes);
    int DeleteDuplicatesInRange(int from,int to);
    
    
} mc_Script;


#endif	/* MULTICHAINSCRIPT_H */

