// Copyright (c) 2014-2017 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "protocol/multichainfilter.h"

using namespace std;

int mc_MultiChainFilterParams::Zero()
{
    m_MaxShownData=-1;
    m_Compatibility=MC_VCM_NONE;
}

int mc_MultiChainFilterParams::Init()
{
    Zero();
    m_Compatibility=mc_gState->m_Compatibility;
    mc_gState->m_Compatibility=MC_VCM_NONE;    
}

int mc_MultiChainFilterParams::Close()
{
    mc_gState->m_Compatibility=m_Compatibility;    
}

int mc_MultiChainFilterParams::Destroy()
{
    Zero();
}

std::vector <uint160>  mc_FillRelevantFilterEntitities(const unsigned char *ptr, size_t value_size)
{
    std::vector <uint160> entities;
    
    if(ptr)
    {
        for(int i=0;i<(int)value_size/MC_AST_SHORT_TXID_SIZE;i++)
        {
            uint160 hash=0;
            memcpy(&hash,ptr+i*MC_AST_SHORT_TXID_SIZE,MC_AST_SHORT_TXID_SIZE);
            entities.push_back(hash);
        }
    }    
    
    return entities;
}

int mc_MultiChainFilter::Zero()
{
    m_RelevantEntities.clear();
    m_CreateError="Not Initialized";
    m_FilterType=MC_FLT_TYPE_TX;
    m_FilterCaption="Unknown";
    m_FilterCode[0]=0x00;
    m_FilterAddress=0;
    
    return MC_ERR_NOERROR;
}

int mc_MultiChainFilter::Destroy()
{
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
    m_FilterCaption=strprintf("%s (txid %s)",
            m_Details.m_Name,txid.ToString().c_str());
    
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
            return MC_ERR_NOT_SUPPORTED;
    }
    
    ptr=(unsigned char *)m_Details.GetSpecialParam(MC_ENT_SPRM_FILTER_ENTITY,&value_size);
    
    if(ptr)
    {
        if(value_size % MC_AST_SHORT_TXID_SIZE)
        {
            return MC_ERR_ERROR_IN_SCRIPT;                        
        }
        m_RelevantEntities=mc_FillRelevantFilterEntitities(ptr, value_size);
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
    m_Workers=NULL;
    
    return MC_ERR_NOERROR;
}

int mc_MultiChainFilterEngine::Destroy()
{
    for(int i=0;i<(int)m_Filters.size();i++)
    {
        if(m_Workers)
        {
            mc_Filter *worker=*(mc_Filter **)m_Workers->GetRow(i);
            delete worker;
        }
        m_Filters[i].Destroy();
    }
    
    if(m_Workers)
    {
        delete m_Workers;
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
    mc_Filter *worker=new mc_Filter;
    m_Workers->Add(&worker);
    std::vector<string> filterFunctionNames;
    
    err=pFilterEngine->CreateFilter(m_Filters.back().m_FilterCode,m_Filters.back().m_MainName.c_str(),
            filterFunctionNames,worker,m_Filters.back().m_CreateError);
    if(err)
    {
        LogPrintf("Couldn't create filter with short txid %s, error: %d\n",filter.m_FilterAddress.ToString().c_str(),err);
        m_Workers->SetCount(m_Workers->GetCount()-1);
        m_Filters.pop_back();
        return err;
    }
    
    if(fDebug)LogPrint("filter","filter: Filter added: %s\n",m_Filters.back().m_FilterCaption.c_str());
    
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
        if(fDebug)LogPrint("filter","filter: Filter rolled back: %s\n",m_Filters.back().m_FilterCaption.c_str());
        mc_Filter *worker=*(mc_Filter **)m_Workers->GetRow(m_Workers->GetCount()-1);
        worker->Destroy();
        m_Workers->SetCount(m_Workers->GetCount()-1);
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
        mc_Filter *worker=*(mc_Filter **)m_Workers->GetRow(i);
        std::vector<string> filterFunctionNames;
        
        err=pFilterEngine->CreateFilter(m_Filters[i].m_FilterCode,m_Filters[i].m_MainName,filterFunctionNames,
                worker,m_Filters[i].m_CreateError);
        if(err)
        {
            LogPrintf("Couldn't prepare filter %s, error: %d\n",m_Filters[i].m_FilterCaption.c_str(),err);
            return err;
        }        
    }    
    
    if(fDebug)LogPrint("filter","filter: Filter engine reset\n");
    return MC_ERR_NOERROR;
}

int mc_MultiChainFilterEngine::Run(const CTransaction& tx,std::set <uint160>& sRelevantEntities,std::string &strResult,mc_MultiChainFilter **lppFilter,int *applied)
{
    int err=MC_ERR_NOERROR;
    strResult="";
    m_Tx=tx;
    m_TxID=m_Tx.GetHash();
    m_Params.Init();
    
    if(applied)
    {
        *applied=0;
    }
    
    for(int i=0;i<(int)m_Filters.size();i++)
    {
        if(m_Filters[i].m_CreateError.size() == 0)
        {
            if(mc_gState->m_Permissions->FilterApproved(NULL,&(m_Filters[i].m_FilterAddress)))
            {
                if(m_Filters[i].HasRelevantEntity(sRelevantEntities))
                {
                    mc_Filter *worker=*(mc_Filter **)m_Workers->GetRow(i);
                    err=pFilterEngine->RunFilter(worker,strResult);
                    if(err)
                    {
                        LogPrintf("Error while running filter %s, error: %d\n",m_Filters[i].m_FilterCaption.c_str(),err);
                        goto exitlbl;
                    }
                    if(strResult.size())
                    {
                        if(lppFilter)
                        {
                            *lppFilter=&(m_Filters[i]);
                        }
                        strResult=strprintf("The transaction did not pass filter %s: %s",m_Filters[i].m_FilterCaption.c_str(),strResult.c_str());
                        if(fDebug)LogPrint("filter","filter: %s\n",strResult.c_str());
                        
                        goto exitlbl;
                    }
                    if(fDebug)LogPrint("filter","filter: Tx %s accepted, filter: %s\n",m_TxID.ToString().c_str(),m_Filters[i].m_FilterCaption.c_str());
                    if(applied)
                    {
                        *applied+=1;
                    }
                }
                else
                {
                    if(fDebug)LogPrint("filter","filter: Irrelevant, filter: %s\n",m_Filters[i].m_FilterCaption.c_str());                    
                }
            }
        }
    }    

exitlbl:
            
    m_Params.Close();
    m_TxID=0;
    return err;
}

int mc_MultiChainFilterEngine::RunFilter(const CTransaction& tx,mc_Filter *filter,std::string &strResult)
{
    int err=MC_ERR_NOERROR;
    m_Tx=tx;
    m_TxID=m_Tx.GetHash();
    m_Params.Init();
    
    err=pFilterEngine->RunFilter(filter,strResult);
    
    m_Params.Close();
    m_TxID=0;
    return err;
}

int mc_MultiChainFilterEngine::RunFilterWithCallbackLog(const CTransaction& tx,mc_Filter *filter,std::string &strResult, json_spirit::Array& callbacks)
{
    int err=MC_ERR_NOERROR;
    m_Tx=tx;
    m_TxID=m_Tx.GetHash();
    m_Params.Init();

    err=pFilterEngine->RunFilterWithCallbackLog(filter,strResult, callbacks);

    m_Params.Close();
    m_TxID=0;
    return err;
}

int mc_MultiChainFilterEngine::Initialize()
{
    mc_Buffer *filters;
    unsigned char *txid;
    int err=MC_ERR_NOERROR;
    mc_EntityDetails entity;
    map <uint64_t, uint256> filter_refs;
    uint64_t max_ref=0;
 
    m_Workers=new mc_Buffer;
    m_Workers->Initialize(sizeof(mc_Filter*),sizeof(mc_Filter*),MC_BUF_MODE_DEFAULT);
    
    
    filters=NULL;
    filters=mc_gState->m_Assets->GetEntityList(filters,NULL,MC_ENT_TYPE_FILTER);
    
    for(int i=0;i<filters->GetCount();i++)
    {
        txid=filters->GetRow(i);
        if(mc_gState->m_Assets->FindEntityByTxID(&entity,txid))
        {        
            if(entity.IsUnconfirmedGenesis() == 0)
            {            
                unsigned char *ptr;
                ptr=(unsigned char *)entity.GetRef();
                uint64_t ref=(mc_GetLE(ptr,4) << 32) + mc_GetLE(ptr+4,4);
                if(ref > max_ref)
                {
                    max_ref=ref;
                }
                filter_refs.insert(pair<uint64_t, uint256>(ref,*(uint256*)txid));
            }
        }
    }

    for(int i=0;i<filters->GetCount();i++)
    {
        txid=filters->GetRow(i);
        if(mc_gState->m_Assets->FindEntityByTxID(&entity,txid))
        {        
            if(entity.IsUnconfirmedGenesis())
            {            
                max_ref++;
                filter_refs.insert(pair<uint64_t, uint256>(max_ref,*(uint256*)txid));
            }
        }
    }
    
    map<uint64_t, uint256>::iterator it;
    
    for(it=filter_refs.begin();it != filter_refs.end();it++)
    {
        err=Add((unsigned char*)&(it->second)+MC_AST_SHORT_TXID_OFFSET);
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
