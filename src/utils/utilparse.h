// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#ifndef MULTICHAINUTILS_H
#define	MULTICHAINUTILS_H

#include "structs/base58.h"
#include "multichain/multichain.h"
#include "primitives/transaction.h"
#include "keys/key.h"
#include "core/main.h"

#define MC_DRF_SIZE                                               40
#define MC_DRF_TYPE_UNKNOWN                                        1                                               
#define MC_DRF_TYPE_TXOUT_GENERAL                                  1                                               
#define MC_DRF_TYPE_TXOUT_BLOCKS                                   2                                               
#define MC_DRF_TYPE_RAW_BLOCKS                                     3                                               
#define MC_DRF_TYPE_CHUNK_GENERAL                                  4 
#define MC_DRF_TYPE_RAW_CHUNKS                                     5 


bool ExtractDestinationScriptValid(const CScript& scriptPubKey, CTxDestination& addressRet);
const unsigned char* GetAddressIDPtr(const CTxDestination& address);
bool HasPerOutputDataEntries(const CTxOut& txout,mc_Script *lpScript);
bool ParseMultichainTxOutToBuffer(uint256 hash,const CTxOut& txout,mc_Buffer *amounts,mc_Script *lpScript,int *allowed,int *required,std::map<uint32_t, uint256>* mapSpecialEntity,std::string& strFailReason);
bool ParseMultichainTxOutToBuffer(uint256 hash,const CTxOut& txout,mc_Buffer *amounts,mc_Script *lpScript,int *allowed,int *required,std::string& strFailReason);
bool CreateAssetBalanceList(const CTxOut& txout,mc_Buffer *amounts,mc_Script *lpScript,int *required);
bool CreateAssetBalanceList(const CTxOut& txout,mc_Buffer *amounts,mc_Script *lpScript);
void LogAssetTxOut(std::string message,uint256 hash,int index,unsigned char* assetrefbin,int64_t quantity);
bool AddressCanReceive(CTxDestination address);
bool FindFollowOnsInScript(const CScript& script1,mc_Buffer *amounts,mc_Script *lpScript);
int CheckRequiredPermissions(const CTxDestination& addressRet,int expected_allowed,std::map<uint32_t, uint256>* mapSpecialEntity,std::string* strFailReason);
bool mc_VerifyAssetPermissions(mc_Buffer *assets, std::vector<CTxDestination> addressRets, int required_permissions, uint32_t permission, std::string& reason);
bool mc_ExtractOutputAssetQuantities(mc_Buffer *assets,std::string& reason,bool with_followons);

typedef struct  mc_DataRef
{
    unsigned char m_Ref[MC_DRF_SIZE];
    uint256 m_Hash;
    uint32_t m_File;
    uint32_t m_Offset;
    uint32_t m_Size;
    uint32_t m_Format;
    uint32_t m_Type;
    
    mc_DataRef()
    {
        Zero();
    }
    
    ~mc_DataRef()
    {
        Destroy();
    }
    
    void Zero();
    void Destroy();
    
    bool Set(uint256 hash,uint32_t file,uint32_t offset,uint32_t size,uint32_t format,uint32_t type);
    bool Set(void *ref,uint32_t refsize);
    void* Get(uint32_t* refsize);
    bool Read();
    
} mc_DataRef;



#endif	/* MULTICHAINUTILS_H */

