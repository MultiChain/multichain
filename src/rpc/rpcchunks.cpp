// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "core/init.h"
#include "wallet/chunkcollector.h"
#include "rpc/rpcutils.h"
#include "wallet/wallettxs.h"


Value getchunkqueueinfo(const Array& params, bool fHelp)
{
    Object result;
    mc_ChunkCollector *collector=pwalletTxsMain->m_ChunkCollector;
    
    collector->Lock();
    
    
    for(int k=0;k<2;k++)
    {
        Object stat;
        stat.push_back(Pair("waiting",collector->m_StatLast[k].m_Pending-collector->m_StatLast[k].m_Queried));
//        stat.push_back(Pair("processing",collector->m_StatLast[k].m_Undelivered));
        stat.push_back(Pair("querying",collector->m_StatLast[k].m_Queried-collector->m_StatLast[k].m_Requested));
        stat.push_back(Pair("retrieving",collector->m_StatLast[k].m_Requested));
        result.push_back(Pair(k ? "bytes" : "chunks",stat));
    }
    
    collector->UnLock();
    
    return result;
}

Value getchunkqueuetotals(const Array& params, bool fHelp)
{
    Object result;
    mc_ChunkCollector *collector=pwalletTxsMain->m_ChunkCollector;
    
    collector->Lock();
        
    for(int k=0;k<2;k++)
    {
        Object stat;
        stat.push_back(Pair("queries",collector->m_StatTotal[k].m_Queried));
        stat.push_back(Pair("responded",collector->m_StatTotal[k].m_Queried-collector->m_StatTotal[k].m_Unresponded-collector->m_StatLast[k].m_Queried));
        stat.push_back(Pair("unresponded",collector->m_StatTotal[k].m_Unresponded));
        stat.push_back(Pair("requests",collector->m_StatTotal[k].m_Requested));
        stat.push_back(Pair("delivered",collector->m_StatTotal[k].m_Delivered));
        stat.push_back(Pair("undelivered",collector->m_StatTotal[k].m_Undelivered));
        stat.push_back(Pair("baddelivered",collector->m_StatTotal[k].m_Baddelivered));
        result.push_back(Pair(k ? "bytes" : "chunks",stat));
    }
    
    collector->UnLock();
    
    return result;
}
