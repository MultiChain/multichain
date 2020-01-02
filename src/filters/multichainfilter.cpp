// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "filters/multichainfilter.h"
#include "filters/filter.h"

using namespace std;

int mc_MultiChainFilterParams::Zero()
{
    m_MaxShownData=-1;
    m_Compatibility=MC_VCM_NONE;
    return MC_ERR_NOERROR;
}

int mc_MultiChainFilterParams::Init()
{
    Zero();
    m_Compatibility=mc_gState->m_Compatibility;
    mc_gState->m_Compatibility=MC_VCM_NONE;    
    return MC_ERR_NOERROR;
}

int mc_MultiChainFilterParams::Close()
{
    mc_gState->m_Compatibility=m_Compatibility;    
    return MC_ERR_NOERROR;
}

int mc_MultiChainFilterParams::Destroy()
{
    return Zero();
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
    m_FilterAddress=0;
    m_FilterCodeRow=0;
    m_AlreadyUsed=false;
    
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
    if(m_Details.m_Name[0])
    {
        m_FilterCaption=strprintf("%s",
                m_Details.m_Name);
    }
    else
    {
        m_FilterCaption=strprintf("(txid %s)",
                txid.ToString().c_str());        
    }
    
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
        case MC_FLT_TYPE_STREAM:
            m_MainName=MC_FLT_MAIN_NAME_STREAM;
            break;
        default:
            m_CreateError="Unsupported filter type";
            return MC_ERR_NOT_SUPPORTED;
    }
    
    if(m_FilterType == MC_FLT_TYPE_TX)
    {
        ptr=(unsigned char *)m_Details.GetSpecialParam(MC_ENT_SPRM_FILTER_RESTRICTIONS,&value_size);

        if(ptr)
        {
            if(value_size % MC_AST_SHORT_TXID_SIZE)
            {
                return MC_ERR_ERROR_IN_SCRIPT;                        
            }
            m_RelevantEntities=mc_FillRelevantFilterEntitities(ptr, value_size);
        }    
    }
    
    
    ptr=(unsigned char *)m_Details.GetSpecialParam(MC_ENT_SPRM_FILTER_CODE,&value_size,1);
    if(ptr)
    {
        unsigned char ntc=0x00;
        m_FilterCodeRow=pMultiChainFilterEngine->m_CodeLibrary->GetNumElements();
        pMultiChainFilterEngine->m_CodeLibrary->AddElement();
        pMultiChainFilterEngine->m_CodeLibrary->SetData(ptr,value_size);
        pMultiChainFilterEngine->m_CodeLibrary->SetData(&ntc,1);        
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
    m_CallbackNames.clear();
    m_CodeLibrary=NULL;
    
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
    
    if(m_CodeLibrary)
    {
        delete m_CodeLibrary;
    }
    
    Zero();
    
    return MC_ERR_NOERROR;
}

int mc_MultiChainFilterEngine::GetAcceptTimeout()
{    
    return GetArg("-acceptfiltertimeout",DEFAULT_ACCEPT_FILTER_TIMEOUT);
}

int mc_MultiChainFilterEngine::GetSendTimeout()
{    
    int accept_timeout=GetAcceptTimeout();
    int timeout=GetArg("-sendfiltertimeout",DEFAULT_SEND_FILTER_TIMEOUT);
    if(accept_timeout > 0)
    {
        if(accept_timeout < timeout)
        {
            timeout=accept_timeout;
        }
    }
    return timeout;
}

int mc_MultiChainFilterEngine::SetTimeout(int timeout)
{
    for(int i=0;i<(int)m_Filters.size();i++)
    {
        mc_Filter *worker=*(mc_Filter **)m_Workers->GetRow(i);
        worker->SetTimeout(timeout);
    }    
    return MC_ERR_NOERROR;
}


int mc_MultiChainFilterEngine::Add(const unsigned char* short_txid,int for_block)
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
    
    char *code;
    size_t code_size;
    code=(char *)m_CodeLibrary->GetData(m_Filters.back().m_FilterCodeRow,&code_size);
    
    err=pFilterEngine->CreateFilter(code,m_Filters.back().m_MainName.c_str(),
            m_CallbackNames[m_Filters.back().m_FilterType],worker,(for_block == 0) ? GetAcceptTimeout() : 0,m_Filters.back().m_CreateError);
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

int mc_MultiChainFilterEngine::Reset(int block,int for_block)
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
        if(m_Filters.back().m_FilterCodeRow > 0)
        {
            m_CodeLibrary->DeleteElement(m_Filters.back().m_FilterCodeRow);
        }
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
        
        char *code;
        size_t code_size;
        code=(char *)m_CodeLibrary->GetData(m_Filters[i].m_FilterCodeRow,&code_size);
        err=pFilterEngine->CreateFilter(code,m_Filters[i].m_MainName,m_CallbackNames[m_Filters[i].m_FilterType],
                worker,m_Filters[i].m_CreateError);
        if(err)
        {
            LogPrintf("Couldn't prepare filter %s, error: %d\n",m_Filters[i].m_FilterCaption.c_str(),err);
            return err;
        }        
    }    
    
    if(for_block == 0)
    {
        SetTimeout(GetAcceptTimeout());
    }
    
    if(fDebug)LogPrint("filter","filter: Filter engine reset\n");
    return MC_ERR_NOERROR;
}
int mc_MultiChainFilterEngine::NoStreamFilters()
{
    if(mc_gState->m_Features->StreamFilters() == 0)
    {
        return 1;
    }
    
    for(int i=0;i<(int)m_Filters.size();i++)
    {
        if( (m_Filters[i].m_FilterType == MC_FLT_TYPE_STREAM) && (m_Filters[i].m_CreateError.size() == 0) )
        {
            return 0;
        }
    }
    
    return 1;
}

int mc_MultiChainFilterEngine::RunStreamFilters(const CTransaction& tx,int vout, unsigned char *stream_short_txid,int block,int offset,std::string &strResult,mc_MultiChainFilter **lppFilter,int *applied)            
{
    if(mc_gState->m_Features->StreamFilters() == 0)
    {
        return MC_ERR_NOERROR;
    }
    
    bool only_once=false;
    int err=MC_ERR_NOERROR;
    strResult="";
    m_Tx=tx;
    m_TxID=m_Tx.GetHash();
    m_Vout=vout;
    m_Params.Init();
    
    unsigned char *stream_entity_txid=mc_gState->m_Assets->CachedTxIDFromShortTxID(stream_short_txid); 
    
    if(applied)
    {
        *applied=0;
    }
    
    if(stream_entity_txid == NULL)
    {
        goto exitlbl;
    }
    
    mc_gState->m_Permissions->SetRollBackPos(block,offset,(offset != 0) ? 1 : 0);
    mc_gState->m_Assets->SetRollBackPos(block,offset,(offset != 0) ? 1 : 0);
    
    for(int i=0;i<(int)m_Filters.size();i++)
    {
        if( (m_Filters[i].m_FilterType == MC_FLT_TYPE_STREAM) && (m_Filters[i].m_CreateError.size() == 0) )
        {
            if(mc_gState->m_Permissions->FilterApproved(stream_entity_txid,&(m_Filters[i].m_FilterAddress)))
            {
                mc_Filter *worker=*(mc_Filter **)m_Workers->GetRow(i);
                bool run_it=true;
                while(run_it)
                {
                    err=pFilterEngine->RunFilter(worker,strResult);
                    if(err)
                    {
                        LogPrintf("Error while running filter %s, error: %d\n",m_Filters[i].m_FilterCaption.c_str(),err);
                        goto exitlbl;
                    }
                    run_it=false;
                    if(strResult.size())
                    {
                        if(!only_once && !m_Filters[i].m_AlreadyUsed)
                        {
                            if(fDebug)LogPrint("filter","filter: stream filter %s failure on first attempt: %s, retrying\n",m_Filters[i].m_FilterCaption.c_str(),strResult.c_str());
                            run_it=true;
                            strResult="";
                        }
                    }
                    m_Filters[i].m_AlreadyUsed=true;
                }
                if(strResult.size())
                {
                    if(lppFilter)
                    {
                        *lppFilter=&(m_Filters[i]);
                    }
                    if(fDebug)LogPrint("filter","filter: %s: %s\n",m_Filters[i].m_FilterCaption.c_str(),strResult.c_str());

                    goto exitlbl;
                }
                if(fDebug)LogPrint("filter","filter: Tx %s accepted, filter: %s\n",m_TxID.ToString().c_str(),m_Filters[i].m_FilterCaption.c_str());
                if(applied)
                {
                    *applied+=1;
                }
            }
        }
    }    

exitlbl:
    
    mc_gState->m_Assets->ResetRollBackPos();
    mc_gState->m_Permissions->ResetRollBackPos();
    m_Params.Close();
    m_TxID=0;
    m_Vout=-1;
    return err;    
}

int mc_MultiChainFilterEngine::RunTxFilters(const CTransaction& tx,std::set <uint160>& sRelevantEntities,std::string &strResult,mc_MultiChainFilter **lppFilter,int *applied,bool only_once)
{
    int err=MC_ERR_NOERROR;
    strResult="";
    m_Tx=tx;
    m_TxID=m_Tx.GetHash();
    m_Vout=-1;
    m_Params.Init();
    
    if(applied)
    {
        *applied=0;
    }
    int failure=0;
    if(fDebug)LogPrint("filter","filter: Starting filtering for tx %s\n",tx.GetHash().ToString().c_str());
    for(int i=0;i<(int)m_Filters.size();i++)
    {
        if( (m_Filters[i].m_FilterType == MC_FLT_TYPE_TX) && (m_Filters[i].m_CreateError.size() == 0) )
        {
            if(mc_gState->m_Permissions->FilterApproved(NULL,&(m_Filters[i].m_FilterAddress)))
            {
                if(m_Filters[i].HasRelevantEntity(sRelevantEntities))
                {
                    mc_Filter *worker=*(mc_Filter **)m_Workers->GetRow(i);
                    bool run_it=true;
                    while(run_it)
                    {
                        err=pFilterEngine->RunFilter(worker,strResult);
                        if(err)
                        {
                            LogPrintf("Error while running filter %s, error: %d\n",m_Filters[i].m_FilterCaption.c_str(),err);
                            goto exitlbl;
                        }
                        run_it=false;
                        if(strResult.size())
                        {
                            if(!only_once && !m_Filters[i].m_AlreadyUsed)
                            {
                                if(fDebug)LogPrint("filter","filter: tx filter %s failure on first attempt: %s, retrying\n",m_Filters[i].m_FilterCaption.c_str(),strResult.c_str());
                                run_it=true;
                                strResult="";
                            }
                        }
                        m_Filters[i].m_AlreadyUsed=true;
                    }
                    if(strResult.size())
                    {
                        if(lppFilter)
                        {
                            *lppFilter=&(m_Filters[i]);
                        }
                        strResult=strprintf("The transaction did not pass filter %s: %s",m_Filters[i].m_FilterCaption.c_str(),strResult.c_str());
                        if(fDebug)LogPrint("filter","filter: %s\n",strResult.c_str());
                        failure++;
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
    if(fDebug)LogPrint("filter","filter: Applied filters: success - %d, failure - %d\n",*applied,failure);
            
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

int mc_MultiChainFilterEngine::RunFilterWithCallbackLog(const CTransaction& tx,int vout,mc_Filter *filter,std::string &strResult, json_spirit::Array& callbacks)
{
    int err=MC_ERR_NOERROR;
    m_Tx=tx;
    m_TxID=m_Tx.GetHash();
    m_Params.Init();
    m_Vout=vout;

    err=pFilterEngine->RunFilterWithCallbackLog(filter,strResult, &callbacks);

    m_Params.Close();
    m_Vout=-1;
    m_TxID=0;
    return err;
}

void mc_MultiChainFilterEngine::SetCallbackNames()
{
    m_CallbackNames.clear();
    
    std::vector <std::string> callbacks;                                        // Tx filter callbacks
    
    callbacks.clear();
    callbacks.push_back("getfiltertxid");
    callbacks.push_back("getfiltertransaction");
    callbacks.push_back("setfilterparam");
    callbacks.push_back("getfiltertxinput");
    callbacks.push_back("getlastblockinfo");
    callbacks.push_back("getassetinfo");
    callbacks.push_back("getstreaminfo");
    callbacks.push_back("verifypermission");
    callbacks.push_back("verifymessage");    
    
    
    if(mc_gState->m_NetworkParams->ProtocolVersion() >= 20005)
    {
        callbacks.push_back("getfilterassetbalances");
    }
    
    m_CallbackNames.push_back(callbacks);                                       // Stream filters callbacks
    
    callbacks.clear();
    callbacks.push_back("getfiltertxid");
    callbacks.push_back("getfiltertransaction");
    callbacks.push_back("setfilterparam");
    callbacks.push_back("getlastblockinfo");
    callbacks.push_back("getassetinfo");
    callbacks.push_back("getstreaminfo");
    callbacks.push_back("verifypermission");
    callbacks.push_back("verifymessage");    
    
    if(mc_gState->m_NetworkParams->ProtocolVersion() >= 20005)
    {
        callbacks.push_back("getfilterstreamitem");        
    }
    
    m_CallbackNames.push_back(callbacks);    
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
    
    m_CodeLibrary=new mc_Script;
    m_CodeLibrary->Clear();
    m_CodeLibrary->AddElement();
    
    SetCallbackNames();
    
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
        err=Add((unsigned char*)&(it->second)+MC_AST_SHORT_TXID_OFFSET,0);
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
