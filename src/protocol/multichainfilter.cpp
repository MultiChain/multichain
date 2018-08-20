// Copyright (c) 2014-2017 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "protocol/multichainfilter.h"

using namespace std;

int mc_MultiChainFilter::Zero()
{
    m_RelevantEntities.clear();
    m_Filter.Zero();
    m_CreateError="Not Initialized";
    m_FilterType=MC_FLT_TYPE_TX;
    m_FilterCaption="Unknown";
    m_FilterCode[0]=0x00;
    m_FilterAddress=0;
    
    return MC_ERR_NOERROR;
}

int mc_MultiChainFilter::Destroy()
{
    m_Filter.Destroy();

    Zero();
    
    return MC_ERR_NOERROR;
}

int mc_MultiChainFilter::Initialize(const unsigned char* short_txid)
{
    size_t value_size;
    unsigned char *ptr;
    
    m_FilterAddress=0;
    memcpy(&m_FilterAddress,short_txid,MC_AST_SHORT_TXID_SIZE);
    
    if(mc_gState->m_Assets->FindEntityByShortTxID(&m_Details,short_txid) == 0)
    {
        return MC_ERR_NOT_FOUND;
    }
    
    uint256 txid;
    txid=*(uint256*)m_Details.GetTxID();
    m_FilterCaption=strprintf("TxID: %s, Name: %s",
            txid.ToString().c_str(),m_Details.m_Name);
    
    ptr=(unsigned char *)m_Details.GetSpecialParam(MC_ENT_SPRM_FILTER_TYPE,&value_size);
    
    if(ptr)
    {
        if( (value_size <=0) || (value_size > 4) )
        {
            return MC_ERR_ERROR_IN_SCRIPT;                        
        }
        m_FilterType=mc_GetLE(ptr,value_size);
    }                                    
    
    switch(m_FilterType)
    {
        case MC_FLT_TYPE_TX:
            m_MainName=MC_FLT_MAIN_NAME_TX;
            break;
        default:
            m_CreateError="Unsupported filter type";
            break;
    }
    
    ptr=(unsigned char *)m_Details.GetSpecialParam(MC_ENT_SPRM_FILTER_ENTITY,&value_size);
    
    if(ptr)
    {
        if(value_size % MC_AST_SHORT_TXID_SIZE)
        {
            return MC_ERR_ERROR_IN_SCRIPT;                        
        }
        
        for(int i=0;i<(int)value_size/MC_AST_SHORT_TXID_SIZE;i++)
        {
            uint160 hash=0;
            memcpy(&hash,ptr+i*MC_AST_SHORT_TXID_SIZE,MC_AST_SHORT_TXID_SIZE);
            m_RelevantEntities.push_back(hash);
        }
    }                                    
    
    ptr=(unsigned char *)m_Details.GetSpecialParam(MC_ENT_SPRM_FILTER_CODE,&value_size);
    
    if(ptr)
    {
        memcpy(m_FilterCode,ptr,value_size);
        m_FilterCode[value_size]=0x00;    
    }                                    
    else
    {    
        m_CreateError="Empty filter code";
    }
    return MC_ERR_NOERROR;    
}

bool mc_MultiChainFilter::HasRelevantEntity(set <uint160>& sRelevantEntities)
{
    if(m_RelevantEntities.size() == 0)
    {
        return true;
    }
    for(int i=0;i<(int)m_RelevantEntities.size();i++)
    {
        if(sRelevantEntities.find(m_RelevantEntities[i]) != sRelevantEntities.end())
        {
            return true;
        }
    }
    return false;
}

int mc_MultiChainFilterEngine::Zero()
{
    m_Filters.clear();
    m_TxID=0;
    
    return MC_ERR_NOERROR;
}

int mc_MultiChainFilterEngine::Destroy()
{
    for(int i=0;i<(int)m_Filters.size();i++)
    {
        m_Filters[i].Destroy();
    }
    
    Zero();
    
    return MC_ERR_NOERROR;
}

int mc_MultiChainFilterEngine::Add(const unsigned char* short_txid)
{    
    int err;
    mc_MultiChainFilter filter;
    
    err=filter.Initialize(short_txid);
    if(err)
    {
        LogPrintf("Couldn't add filter with short txid %s, error: %d\n",filter.m_FilterAddress.ToString().c_str(),err);
        return err;
    }
    
    m_Filters.push_back(filter);
    
    err=pFilterEngine->CreateFilter(m_Filters.back().m_FilterCode,m_Filters.back().m_MainName.c_str(),&(m_Filters.back().m_Filter),m_Filters.back().m_CreateError);
    if(err)
    {
        LogPrintf("Couldn't create filter with short txid %s, error: %d\n",filter.m_FilterAddress.ToString().c_str(),err);
        m_Filters.pop_back();
        return err;
    }
    
    LogPrint("filter","Filter added: %s\n",m_Filters.back().m_FilterCaption.c_str());
    
    return MC_ERR_NOERROR;
}

int mc_MultiChainFilterEngine::Reset(int block)
{
    int filter_block;
    int err;
    
    if(m_Filters.size() == 0)
    {
        return MC_ERR_NOERROR;        
    }
    
    filter_block=m_Filters.back().m_Details.m_LedgerRow.m_Block;
    if(filter_block<0)
    {
        filter_block=block+1;
    }
    while( (m_Filters.size()>0) && (filter_block > block) )
    {
        LogPrint("filter","Filter rolled back: %s\n",m_Filters.back().m_FilterCaption.c_str());
        m_Filters.back().Destroy();
        m_Filters.pop_back();
        if(m_Filters.size()>0)
        {
            filter_block=m_Filters.back().m_Details.m_LedgerRow.m_Block;
            if(filter_block<0)
            {
                filter_block=block+1;
            }            
        }
    }
    
    for(int i=0;i<(int)m_Filters.size();i++)
    {
        err=pFilterEngine->CreateFilter(m_Filters[i].m_FilterCode,m_Filters[i].m_MainName.c_str(),&(m_Filters[i].m_Filter),m_Filters[i].m_CreateError);
        if(err)
        {
            LogPrintf("Couldn't prepare filter %s, error: %d\n",m_Filters[i].m_FilterCaption.c_str(),err);
            return err;
        }        
    }    
    
    LogPrint("filter","Filter engine reset\n");
    return MC_ERR_NOERROR;
}

int mc_MultiChainFilterEngine::Run(uint256 txid,std::set <uint160>& sRelevantEntities,std::string &strResult,mc_MultiChainFilter **lppFilter)
{
    int err;
    strResult="";
    m_TxID=txid;
    
    for(int i=0;i<(int)m_Filters.size();i++)
    {
        if(m_Filters[i].m_CreateError.size() == 0)
        {
            if(mc_gState->m_Permissions->FilterApproved(NULL,&(m_Filters[i].m_FilterAddress)))
            {
                if(m_Filters[i].HasRelevantEntity(sRelevantEntities))
                {
                    err=pFilterEngine->RunFilter(m_Filters[i].m_Filter,strResult);
                    if(err)
                    {
                        LogPrintf("Error while running filter %s, error: %d\n",m_Filters[i].m_FilterCaption.c_str(),err);
                        return err;
                    }
                    if(strResult.size())
                    {
                        if(lppFilter)
                        {
                            *lppFilter=&(m_Filters[i]);
                        }
                        LogPrint("filter","Tx rejected: %s, filter: %s\n",strResult.c_str(),m_Filters[i].m_FilterCaption.c_str());
                        
                        return MC_ERR_NOERROR;
                    }
                    LogPrint("filter","Tx %s accepted, filter: %s\n",txid.ToString().c_str(),m_Filters[i].m_FilterCaption.c_str());
                }
                else
                {
                    LogPrint("filter","Irrelevant, filter: %s\n",m_Filters[i].m_FilterCaption.c_str());                    
                }
            }
        }
    }    
    
    m_TxID=0;
    return MC_ERR_NOERROR;
}

int mc_MultiChainFilterEngine::Initialize()
{
    mc_Buffer *filters;
    unsigned char *txid;
    int err=MC_ERR_NOERROR;
    
    filters=NULL;
    filters=mc_gState->m_Assets->GetEntityList(filters,NULL,MC_ENT_TYPE_FILTER);
    
    for(int i=0;i<filters->GetCount();i++)
    {
        txid=filters->GetRow(i);
        err=Add(txid+MC_AST_SHORT_TXID_OFFSET);
        if(err)
        {
            goto exitlbl;
        }
    }
    
    LogPrintf("Filter initialization completed\n");
    
exitlbl:
            
    mc_gState->m_Assets->FreeEntityList(filters);
    
    return MC_ERR_NOERROR;    
}