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

typedef struct mc_MultiChainFilterEngine
{
    std::vector <mc_MultiChainFilter> m_Filters;
    mc_Buffer *m_Workers;
    uint256 m_TxID;

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
    int Run(uint256 txid,std::set <uint160>& sRelevantEntities,std::string &strResult,mc_MultiChainFilter **lppFilter);            
    
    int Zero();
    int Destroy();   
    
} mc_MultiChainFilterEngine;

#endif /* MULTICHAINFILTER_H */

