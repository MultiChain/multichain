// Copyright (c) 2014-2017 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#ifndef MULTICHAINFILTER_H
#define MULTICHAINFILTER_H

#include "core/init.h"
#include "core/main.h"
#include "utils/util.h"
#include "utils/utilparse.h"
#include "multichain/multichain.h"
#include "protocol/filter.h"

#define MC_FLT_TYPE_BAD                    0xFFFFFFFF
#define MC_FLT_TYPE_TX                     0

#define MC_FLT_MAIN_NAME_TX                "filtertransaction"

std::vector <uint160>  mc_FillRelevantFilterEntitities(const unsigned char *ptr, size_t value_size);

typedef struct mc_MultiChainFilter
{
    std::vector <uint160> m_RelevantEntities;
    
    mc_EntityDetails m_Details;
    std::string m_CreateError;
    std::string m_MainName;
    std::string m_FilterCaption;
    char m_FilterCode[MC_ENT_MAX_SCRIPT_SIZE+1];
    uint32_t m_FilterType;
    uint160 m_FilterAddress;
    
    mc_MultiChainFilter()
    {
        Zero();
    }
    
    ~mc_MultiChainFilter()
    {
        Destroy();
    }
    
    int Initialize(const unsigned char* short_txid);
    
    int Zero();
    int Destroy();   

    bool HasRelevantEntity(std::set <uint160>& sRelevantEntities);
    
} mc_MultiChainFilter;

typedef struct mc_MultiChainFilterParams
{
    int m_MaxShownData;
    int m_Compatibility;
    mc_MultiChainFilterParams()
    {
        Zero();
    }
    
    ~mc_MultiChainFilterParams()
    {
        Destroy();
    }
    
    int Zero();
    int Destroy();  
    int Init();
    int Close();
}mc_MultiChainFilterParams;

typedef struct mc_MultiChainFilterEngine
{
    std::vector <mc_MultiChainFilter> m_Filters;
    mc_Buffer *m_Workers;
    uint256 m_TxID;
    CTransaction m_Tx;
    mc_MultiChainFilterParams m_Params;

    mc_MultiChainFilterEngine()
    {
        Zero();
    }
    
    ~mc_MultiChainFilterEngine()
    {
        Destroy();
    }
    
    int Initialize();
    int Add(const unsigned char* short_txid);
    int Reset(int block);
    int Run(const CTransaction& tx,std::set <uint160>& sRelevantEntities,std::string &strResult,mc_MultiChainFilter **lppFilter,int *applied);            
    int RunFilter(const CTransaction& tx,mc_Filter *filter,std::string &strResult);            
    int RunFilterWithCallbackLog(const CTransaction& tx,mc_Filter *filter,std::string &strResult, json_spirit::Array& callbacks);

    
    int Zero();
    int Destroy();   
    
} mc_MultiChainFilterEngine;

#endif /* MULTICHAINFILTER_H */

