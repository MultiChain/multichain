// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "protocol/relay.h"
#include "structs/base58.h"
#include "wallet/chunkdb.h"
#include "wallet/chunkcollector.h"
#include "wallet/wallettxs.h"
#include "community/community.h"

uint32_t MultichainNextChunkQueryAttempt(uint32_t attempts)
{
    if(attempts <  2)return 0;
    return (uint32_t)(int64_t)(pow(1.5,attempts-1)-1);
}

string mc_MsgTypeStr(uint32_t msg_type)
{
    char *ptr;
    ptr=(char*)&msg_type;
    if(msg_type < 0x01000000)
    {
        return strprintf("%08x",msg_type);        
    }
    return strprintf("%c%c%c%c",ptr[0],ptr[1],ptr[2],ptr[3]);
}

typedef struct CRelayResponsePair
{
    mc_OffchainMessageID request_id;
    int response_id;
    
    friend bool operator<(const CRelayResponsePair& a, const CRelayResponsePair& b)
    {
        return ((a.request_id < b.request_id) || 
                (a.request_id == b.request_id && a.response_id < b.response_id));
    }
    
} CRelayResponsePair;

typedef struct CRelayRequestPairs
{
    map<int,int> m_Pairs;
} CRelayRequestPairs;

int mc_IsReadPermissionedStream(mc_ChunkEntityKey* chunk,map<uint160,int>& cache,set<CPubKey>* sAddressesToSign)
{
    if(chunk->m_Entity.m_EntityType != MC_TET_STREAM)
    {
        return -1;
    }    
    
    unsigned char *ptr=chunk->m_Entity.m_EntityID;
    uint160 enthash=*(uint160*)ptr;
    map<uint160, int>::const_iterator it = cache.find(enthash);
    if(it != cache.end())
    {
        return it->second;        
    }

    mc_EntityDetails entity;
    int result=0;
    
    {
        LOCK(cs_main);                                                          // possible caching improvement here
        if(mc_gState->m_Assets->FindEntityByShortTxID(&entity,chunk->m_Entity.m_EntityID) == 0)
        {
            result=-2;
        }               
    }
    
    if(result)
    {
        return result;        
    }
    
    result=0;
    
    if(entity.AnyoneCanRead() == 0)
    {
        result=1;
        if(sAddressesToSign)
        {
            CKeyID keyID;
            set<CPubKey>::iterator it;
            for (it = sAddressesToSign->begin(); it !=  sAddressesToSign->end(); ++it)
            {
                if(result)
                {
                    keyID=(*it).GetID();
                    if(mc_gState->m_Permissions->CanRead(chunk->m_Entity.m_EntityID,(unsigned char*)(&keyID)))
                    {
                        result=0;
                    }
                }
            }
            if(result)
            {
                CPubKey pubkey=pEF->WLT_FindReadPermissionedAddress(&entity);
                if(pubkey.IsValid())
                {
                    sAddressesToSign->insert(pubkey);
                    result=0;                
                }                
            }
        }
    }
    
    cache.insert(make_pair(enthash,result));
    return result;    
}

bool MultichainProcessChunkResponse(const CRelayResponsePair *response_pair,map <int,int>* request_pairs,mc_ChunkCollector* collector)
{
    mc_RelayRequest *request;
    mc_RelayResponse *response;
    map<uint160,int> mapReadPermissionCache;
    request=pRelayManager->FindRequest(response_pair->request_id);
    if(request == NULL)
    {
        return false;
    }
    response=&(request->m_Responses[response_pair->response_id]);
    
    unsigned char *ptr;
    unsigned char *ptrEnd;
    unsigned char *ptrStart;
    int shift,count,size;
    int shiftOut,countOut,sizeOut;
    int chunk_err;
    mc_ChunkEntityKey *chunk;
    mc_ChunkEntityKey *chunkOut;
    unsigned char *ptrOut;
    unsigned char *ptrOutEnd;
    bool result=false;
    string strError="";
    mc_ChunkCollectorRow *collect_row;
        
    uint32_t total_size=0;
    ptrStart=&(request->m_Payload[0]);
    
    size=sizeof(mc_ChunkEntityKey);
    shift=0;
    count=0;
    
    ptr=ptrStart;
    ptrEnd=ptr+request->m_Payload.size();
        
    while(ptr<ptrEnd)
    {
        switch(*ptr)
        {
            case MC_RDT_EXPIRATION:
                ptr+=5;
                break;
            case MC_RDT_CHUNK_IDS:
                ptr++;
                count=(int)mc_GetVarInt(ptr,ptrEnd-ptr,-1,&shift);
                ptr+=shift;
                if(count*size != (ptrEnd-ptr))
                {
                    goto exitlbl;
                }                
                for(int c=0;c<count;c++)
                {
                    if(mc_IsReadPermissionedStream((mc_ChunkEntityKey*)ptr,mapReadPermissionCache,NULL))
                    {
                        result=pEF->OFF_ProcessChunkResponse(request,response,request_pairs,collector,strError);
                        goto exitlbl;
                    }
                    total_size+=((mc_ChunkEntityKey*)ptr)->m_Size+size;
                    ptr+=size;
                }
                break;
            default:
                result=pEF->OFF_ProcessChunkResponse(request,response,request_pairs,collector,strError);
                goto exitlbl;
        }
    }
    

    if(response->m_Payload.size() < 1+shift+total_size)
    {
        strError="Total size mismatch";
        goto exitlbl;        
    }

    ptrOut=&(response->m_Payload[0]);
    ptrOutEnd=ptrOut+response->m_Payload.size();
    if(*ptrOut != MC_RDT_CHUNKS)
    {
        strError="Unsupported payload format";
        goto exitlbl;                
    }
    
    ptrOut++;
    countOut=(int)mc_GetVarInt(ptrOut,1+shift+total_size,-1,&shiftOut);
    if( (countOut != count) || (shift != shiftOut) )
    {
        strError="Chunk count mismatch";
        goto exitlbl;                        
    }
    ptrOut+=shift;
    
    ptr=ptrStart+1+shift+5;
    for(int c=0;c<count;c++)
    {
        sizeOut=((mc_ChunkEntityKey*)ptr)->m_Size;
        chunk=(mc_ChunkEntityKey*)ptr;
        chunkOut=(mc_ChunkEntityKey*)ptrOut;
        if(ptrOutEnd - ptrOut < size)
        {
            strError="Total size mismatch";
            goto exitlbl;                                                    
        }
        ptrOut+=size;
        if(chunk->m_Size != chunkOut->m_Size)
        {
            for(int k=0;k<2;k++)collector->m_StatTotal[k].m_Baddelivered+=k ? chunk->m_Size : 1;                
            strError="Chunk info size mismatch";
            goto exitlbl;                                        
        }
        if(memcmp(chunk->m_Hash,chunkOut->m_Hash,sizeof(uint256)))
        {
            for(int k=0;k<2;k++)collector->m_StatTotal[k].m_Baddelivered+=k ? chunk->m_Size : 1;                
            strError="Chunk info hash mismatch";
            goto exitlbl;                                                    
        }
        if(memcmp(&(chunk->m_Entity),&(chunkOut->m_Entity),sizeof(mc_TxEntity)))
        {
            for(int k=0;k<2;k++)collector->m_StatTotal[k].m_Baddelivered+=k ? chunk->m_Size : 1;                
            strError="Chunk info entity mismatch";
            goto exitlbl;                                                    
        }
        sizeOut=chunk->m_Size;
        map <int,int>::iterator itreq = request_pairs->find(c);
        if (itreq != request_pairs->end())
        {
            collect_row=(mc_ChunkCollectorRow *)collector->m_MemPool->GetRow(itreq->second);
            if(ptrOutEnd - ptrOut < collect_row->m_SaltSize+sizeOut)
            {
                strError="Total size mismatch";
                goto exitlbl;                                                    
            }
            if(collect_row->m_SaltSize)
            {
                memcpy(collect_row->m_Salt,ptrOut,collect_row->m_SaltSize);
                ptrOut+=collect_row->m_SaltSize;
            }
            uint256 hash;
//            mc_gState->m_TmpBuffers->m_RpcHasher1->DoubleHash(ptrOut,sizeOut,&hash);
            mc_gState->m_TmpBuffers->m_RpcHasher1->DoubleHash(collect_row->m_Salt,collect_row->m_SaltSize,ptrOut,sizeOut,&hash);
            if(memcmp(&hash,chunk->m_Hash,sizeof(uint256)))
            {
                for(int k=0;k<2;k++)collector->m_StatTotal[k].m_Baddelivered+=k ? collect_row->m_ChunkDef.m_Size : 1;                
                strError="Chunk data hash mismatch";
                goto exitlbl;                                        
            }
            if( (collect_row->m_State.m_Status & MC_CCF_DELETED ) == 0 )
            {
                chunk_err=pwalletTxsMain->m_ChunkDB->AddChunk(chunk->m_Hash,&(chunk->m_Entity),(unsigned char*)collect_row->m_TxID,collect_row->m_Vout,
                        ptrOut,NULL,collect_row->m_Salt,sizeOut,0,collect_row->m_SaltSize,collect_row->m_Flags);
                if(chunk_err)
                {
                    if(chunk_err != MC_ERR_FOUND)
                    {
                        strError=strprintf("Internal chunk DB error: %d",chunk_err);
                        goto exitlbl;                    
                    }
                }
                else
                {
                    for(int k=0;k<2;k++)collector->m_StatTotal[k].m_Delivered+=k ? collect_row->m_ChunkDef.m_Size : 1;                
                    unsigned char* ptrhash=chunk->m_Hash;
                    if(fDebug)LogPrint("chunks","Retrieved chunk %s\n",(*(uint256*)ptrhash).ToString().c_str());                
                }
            }
            collect_row->m_State.m_Status |= MC_CCF_DELETED;
        }        
        
        ptr+=size;
        ptrOut+=sizeOut;
    }
    
    result=true;
    
exitlbl:
                
    pRelayManager->UnLock();
                
    if(strError.size())
    {
        if(fDebug)LogPrint("chunks","Bad response from peer %d: %s\n",response->m_NodeFrom,strError.c_str());
    }
    return result;
}

int MultichainResponseScore(mc_RelayResponse *response,mc_ChunkCollectorRow *collect_row,map<int64_t,int64_t>& destination_loads,uint32_t max_total_size)
{
    unsigned char *ptr;
    unsigned char *ptrEnd;
    unsigned char *ptrStart;
    int shift,count,size;
    int64_t total_size;
    mc_ChunkEntityKey *chunk;
    int c;
    if( (response->m_Status & MC_RST_SUCCESS) == 0 )
    {
        return MC_CCW_WORST_RESPONSE_SCORE;
    }

    total_size=0;
    map<int64_t,int64_t>::iterator itdld = destination_loads.find(response->SourceID());
    if (itdld != destination_loads.end())
    {
        total_size=itdld->second;
    }                                    
    
    if(total_size + collect_row->m_ChunkDef.m_Size + sizeof(mc_ChunkEntityKey) > max_total_size)
    {
        return MC_CCW_WORST_RESPONSE_SCORE;                
    }
    
    ptrStart=&(response->m_Payload[0]);
    
    size=sizeof(mc_ChunkEntityKey);
    shift=0;
    count=0;
    
    ptr=ptrStart;
    ptrEnd=ptr+response->m_Payload.size();
    if(*ptr == MC_RDT_ENTERPRISE_FEATURES)
    {
        if(mc_gState->m_Features->ReadPermissions())
        {
            ptr++;
            int length=mc_GetVarInt(ptr,ptrEnd-ptr,-1,&shift);
            ptr+=shift;
            ptr+=length;
        }
        else
        {
            return MC_CCW_WORST_RESPONSE_SCORE;                    
        }
    }
    ptr++;
    count=(int)mc_GetVarInt(ptr,ptrEnd-ptr,-1,&shift);
    ptr+=shift;
    
    c=0;
    while(c<count)
    {
        chunk=(mc_ChunkEntityKey*)ptr;
        if( (memcmp(chunk->m_Hash,collect_row->m_ChunkDef.m_Hash,MC_CDB_CHUNK_HASH_SIZE) == 0) && 
            (memcmp(&(chunk->m_Entity),&(collect_row->m_ChunkDef.m_Entity),sizeof(mc_TxEntity)) == 0))
        {
            if(chunk->m_Flags & MC_CCF_ERROR_MASK)
            {
                return MC_CCW_WORST_RESPONSE_SCORE;
            }
            c=count+1;
        }
        ptr+=size;
        c++;
    }
    if(c == count)
    {
        return MC_CCW_WORST_RESPONSE_SCORE;        
    }
        
    return (response->m_TryCount+response->m_HopCount)*1024*1024+total_size/1024;
}

int MultichainCollectChunks(mc_ChunkCollector* collector)
{
    uint32_t time_now,expiration,dest_expiration;    
    vector <mc_ChunkEntityKey> vChunkDefs;
    int row,last_row,last_count,to_end_of_query;
    uint32_t total_size,max_total_query_size,max_total_destination_size,total_in_queries,max_total_in_queries,query_count;
    mc_ChunkCollectorRow *collect_row;
    mc_ChunkCollectorRow *collect_subrow;
    time_now=mc_TimeNowAsUInt();
    vector<unsigned char> payload;
    unsigned char buf[16];
    int shift,count;
    unsigned char *ptrOut;
    mc_OffchainMessageID query_id,request_id;
    map <mc_OffchainMessageID,bool> query_to_delete;
    map <CRelayResponsePair,CRelayRequestPairs> requests_to_send;    
    map <CRelayResponsePair,CRelayRequestPairs> responses_to_process;    
    map <int64_t,int64_t> destination_loads;    
    mc_RelayRequest *request;
    mc_RelayRequest *query;
    mc_RelayResponse *response;
    CRelayResponsePair response_pair;
    vector<int> vRows;
    CRelayRequestPairs request_pairs;
    int best_score,best_response,this_score,not_processed;
    map<uint160,int> mapReadPermissionCache;
    set<CPubKey> sAddressesToSign;
    
    pRelayManager->CheckTime();
    pRelayManager->InvalidateResponsesFromDisconnected();
    
    collector->Lock();

    for(row=0;row<collector->m_MemPool->GetCount();row++)
    {
        collect_row=(mc_ChunkCollectorRow *)collector->m_MemPool->GetRow(row);
        if( (collect_row->m_State.m_Status & MC_CCF_DELETED ) == 0 )
        {
            if(collect_row->m_State.m_RequestTimeStamp <= time_now)
            {
                if(!collect_row->m_State.m_Request.IsZero())
                {
                    pRelayManager->DeleteRequest(collect_row->m_State.m_Request);
                    collect_row->m_State.m_Request=0;                    
                    for(int k=0;k<2;k++)collector->m_StatTotal[k].m_Undelivered+=k ? collect_row->m_ChunkDef.m_Size : 1;                
                }                
            }
            request=NULL;
            if(!collect_row->m_State.m_Request.IsZero())
            {
                request=pRelayManager->FindRequest(collect_row->m_State.m_Request);
                if(request == NULL)
                {
                    collect_row->m_State.m_Request=0;
                    collect_row->m_State.m_RequestTimeStamp=0;
                    for(int k=0;k<2;k++)collector->m_StatTotal[k].m_Undelivered+=k ? collect_row->m_ChunkDef.m_Size : 1;                                    
                }
            }
            if(request)
            {
                if(request->m_Responses.size())
                {
                    response_pair.request_id=collect_row->m_State.m_Request;
                    response_pair.response_id=0;
//                    printf("coll new rsp: row: %d, id: %s, %d\n",row,collect_row->m_State.m_Request.ToString().c_str(),collect_row->m_State.m_RequestPos);
                    map<CRelayResponsePair,CRelayRequestPairs>::iterator itrsp = responses_to_process.find(response_pair);
                    if (itrsp == responses_to_process.end())
                    {
                        request_pairs.m_Pairs.clear();
                        request_pairs.m_Pairs.insert(make_pair(collect_row->m_State.m_RequestPos,row));
                        responses_to_process.insert(make_pair(response_pair,request_pairs));
                    }       
                    else
                    {
                        itrsp->second.m_Pairs.insert(make_pair(collect_row->m_State.m_RequestPos,row));
                    }                    
                }            
                pRelayManager->UnLock();
            }
        }        
    }

    BOOST_FOREACH(PAIRTYPE(const CRelayResponsePair, CRelayRequestPairs)& item, responses_to_process)    
    {
        MultichainProcessChunkResponse(&(item.first),&(item.second.m_Pairs),collector);
        pRelayManager->DeleteRequest(item.first.request_id);
    }


    max_total_destination_size=collector->m_MaxKBPerDestination*1024;
//    max_total_size/=MC_CCW_QUERY_SPLIT;
/*    
    if(max_total_destination_size > MAX_SIZE-OFFCHAIN_MSG_PADDING)
    {
        max_total_destination_size=MAX_SIZE-OFFCHAIN_MSG_PADDING;        
    }
    
    if(max_total_size < MAX_CHUNK_SIZE + sizeof(mc_ChunkEntityKey))
    {
        max_total_size = MAX_CHUNK_SIZE + sizeof(mc_ChunkEntityKey);
    }
 */ 
    max_total_query_size=MAX_CHUNK_SIZE + sizeof(mc_ChunkEntityKey);
    if(max_total_destination_size<max_total_query_size)
    {
        max_total_destination_size=max_total_query_size;
    }
    
    max_total_in_queries=collector->m_MaxKBPerDestination*1024;
    max_total_in_queries*=collector->m_TimeoutRequest;
    total_in_queries=0;
    query_count=0;
    
    for(row=0;row<collector->m_MemPool->GetCount();row++)
    {
        collect_row=(mc_ChunkCollectorRow *)collector->m_MemPool->GetRow(row);
        if( (collect_row->m_State.m_Status & MC_CCF_DELETED ) == 0 )
        {
            request=NULL;
            if(!collect_row->m_State.m_Request.IsZero())
            {
                request=pRelayManager->FindRequest(collect_row->m_State.m_Request);
                if(request == NULL)
                {
                    collect_row->m_State.m_Request=0;
                    collect_row->m_State.m_RequestTimeStamp=0;
                    for(int k=0;k<2;k++)collector->m_StatTotal[k].m_Undelivered+=k ? collect_row->m_ChunkDef.m_Size : 1;                
                }
            }
            if(request)
            {
                map<int64_t,int64_t>::iterator itdld = destination_loads.find(request->m_DestinationID);
                if (itdld == destination_loads.end())
                {
                    destination_loads.insert(make_pair(request->m_DestinationID,collect_row->m_ChunkDef.m_Size + sizeof(mc_ChunkEntityKey)));
                }       
                else
                {
                    itdld->second+=collect_row->m_ChunkDef.m_Size + sizeof(mc_ChunkEntityKey);
                }  
                pRelayManager->UnLock();
            }
        }
        else
        {
            if(!collect_row->m_State.m_Query.IsZero())
            {
                map<mc_OffchainMessageID, bool>::iterator itqry = query_to_delete.find(collect_row->m_State.m_Query);
                if (itqry == query_to_delete.end())
                {
                    query_to_delete.insert(make_pair(collect_row->m_State.m_Query,true));
                }       
            }            
        }
    }
    
    for(row=0;row<collector->m_MemPool->GetCount();row++)
    {
        collect_row=(mc_ChunkCollectorRow *)collector->m_MemPool->GetRow(row);
        if( (collect_row->m_State.m_Status & MC_CCF_DELETED ) == 0 )
        {
            if(collect_row->m_State.m_Request.IsZero())
            {
                query=NULL;
                if(!collect_row->m_State.m_Query.IsZero())
                {
                    query=pRelayManager->FindRequest(collect_row->m_State.m_Query);
                    if(query == NULL)
                    {
                        collect_row->m_State.m_Query=0;
                        collect_row->m_State.m_QueryNextAttempt=time_now+MultichainNextChunkQueryAttempt(collect_row->m_State.m_QueryAttempts);                                                
                        collect_row->m_State.m_Status |= MC_CCF_UPDATED;
                        for(int k=0;k<2;k++)collector->m_StatTotal[k].m_Unresponded+=k ? collect_row->m_ChunkDef.m_Size : 1;                
                    }
                }
                if(query)
                {
                    best_response=-1;
                    best_score=MC_CCW_WORST_RESPONSE_SCORE;
                    for(int i=0;i<(int)query->m_Responses.size();i++)
                    {
                        this_score=MultichainResponseScore(&(query->m_Responses[i]),collect_row,destination_loads,max_total_destination_size);
                        if(this_score < best_score)
                        {
                            best_score=this_score;
                            best_response=i;
                        }
                    }
                    if(best_response >= 0)
                    {
                        response_pair.request_id=collect_row->m_State.m_Query;
                        response_pair.response_id=best_response;                        
                        map<CRelayResponsePair,CRelayRequestPairs>::iterator itrsp = requests_to_send.find(response_pair);                                                
                        if (itrsp == requests_to_send.end())
                        {                            
//                    printf("coll new req: row: %d, id:  %s, rsps: %d, score (%d,%d)\n",row,collect_row->m_State.m_Query.ToString().c_str(),(int)query->m_Responses.size(),best_score,best_response);
                            request_pairs.m_Pairs.clear();
                            request_pairs.m_Pairs.insert(make_pair(row,0));
                            requests_to_send.insert(make_pair(response_pair,request_pairs));
                        }       
                        else
                        {
//                    printf("coll old req: row: %d, id:  %s, rsps: %d, score (%d,%d)\n",row,collect_row->m_State.m_Query.ToString().c_str(),(int)query->m_Responses.size(),best_score,best_response);
                            itrsp->second.m_Pairs.insert(make_pair(row,0));
                        }                    
                        map<int64_t,int64_t>::iterator itdld = destination_loads.find(query->m_Responses[best_response].SourceID());
                        if (itdld == destination_loads.end())
                        {
                            destination_loads.insert(make_pair(query->m_Responses[best_response].SourceID(),collect_row->m_ChunkDef.m_Size + sizeof(mc_ChunkEntityKey)));
                        }       
                        else
                        {
                            itdld->second+=collect_row->m_ChunkDef.m_Size + sizeof(mc_ChunkEntityKey);
                        }                                    
                    }
                    pRelayManager->UnLock();
                }
            }
            if(!collect_row->m_State.m_Query.IsZero())
            {
                to_end_of_query=collect_row->m_State.m_QueryTimeStamp-time_now;
                if(to_end_of_query<0)to_end_of_query=0;
                if(to_end_of_query>collector->m_TimeoutRequest)to_end_of_query=collector->m_TimeoutRequest;
                total_in_queries+=collect_row->m_ChunkDef.m_Size*to_end_of_query;
            }
        }        
    }
    
    BOOST_FOREACH(PAIRTYPE(const CRelayResponsePair, CRelayRequestPairs)& item, requests_to_send)    
    {
        string strError;
        bool lost_permission=false;
        mapReadPermissionCache.clear();
        int ef_cache_id;
        sAddressesToSign.clear();
        
        vector<unsigned char> vRPPayload;
        BOOST_FOREACH(PAIRTYPE(const int, int)& chunk_row, item.second.m_Pairs)    
        {                            
            collect_subrow=(mc_ChunkCollectorRow *)collector->m_MemPool->GetRow(chunk_row.first);
            if(mc_IsReadPermissionedStream(&(collect_subrow->m_ChunkDef),mapReadPermissionCache,&sAddressesToSign) != 0)
            {
                lost_permission=true;
            }            
        }
        if(!lost_permission)
        {
            vRPPayload.clear();
            request=pRelayManager->FindRequest(item.first.request_id);
            if(request == NULL)
            {
                return false;
            }

            response=&(request->m_Responses[item.first.response_id]);
            
            ef_cache_id=-1;
            if(sAddressesToSign.size())
            {
                if(!pEF->OFF_GetPayloadForReadPermissioned(&vRPPayload,&ef_cache_id,strError))
                {
                    if(fDebug)LogPrint("chunks","Error creating read-permissioned EF payload: %s\n",
                            strError.c_str());                                            
                }
                
            }

            payload.clear();
            shift=mc_PutVarInt(buf,16,item.second.m_Pairs.size());
            payload.resize(5+1+shift+vRPPayload.size()+sizeof(mc_ChunkEntityKey)*item.second.m_Pairs.size());

            expiration=time_now+collector->m_TimeoutRequest;
            dest_expiration=expiration+response->m_MsgID.m_TimeStamp-request->m_MsgID.m_TimeStamp;// response->m_TimeDiff;
            ptrOut=&(payload[0]);
            *ptrOut=MC_RDT_EXPIRATION;
            ptrOut++;
            mc_PutLE(ptrOut,&dest_expiration,sizeof(dest_expiration));
            ptrOut+=sizeof(dest_expiration);
            if(vRPPayload.size())
            {
                memcpy(ptrOut,&(vRPPayload[0]),vRPPayload.size());      
                ptrOut+=vRPPayload.size();
            }
            *ptrOut=MC_RDT_CHUNK_IDS;
            ptrOut++;
            memcpy(ptrOut,buf,shift);
            ptrOut+=shift;
            count=0;
            BOOST_FOREACH(PAIRTYPE(const int, int)& chunk_row, item.second.m_Pairs)    
            {                            
                collect_subrow=(mc_ChunkCollectorRow *)collector->m_MemPool->GetRow(chunk_row.first);
    //            printf("S %d\n",chunk_row.first);
                collect_subrow->m_State.m_RequestPos=count;
                memcpy(ptrOut,&(collect_subrow->m_ChunkDef),sizeof(mc_ChunkEntityKey));
                ptrOut+=sizeof(mc_ChunkEntityKey);
                count++;
            }
    //        mc_DumpSize("req",&(payload[0]),1+shift+sizeof(mc_ChunkEntityKey)*item.second.m_Pairs.size(),64);
            request_id=pRelayManager->SendNextRequest(response,MC_RMT_CHUNK_REQUEST,0,payload,sAddressesToSign,ef_cache_id);
            if(!request_id.IsZero())
            {
                if(fDebug)LogPrint("chunks","New chunk request: %s, response: %s, chunks: %d\n",request_id.ToString().c_str(),response->m_MsgID.ToString().c_str(),item.second.m_Pairs.size());
                BOOST_FOREACH(PAIRTYPE(const int, int)& chunk_row, item.second.m_Pairs)    
                {                
                    collect_subrow=(mc_ChunkCollectorRow *)collector->m_MemPool->GetRow(chunk_row.first);
                    collect_subrow->m_State.m_Request=request_id;
                    collect_subrow->m_State.m_RequestTimeStamp=expiration;
                    for(int k=0;k<2;k++)collector->m_StatTotal[k].m_Requested+=k ? collect_subrow->m_ChunkDef.m_Size : 1;                
        //            printf("T %d %d %s\n",chunk_row.first,collect_subrow->m_State.m_RequestPos,collect_subrow->m_State.m_Request.ToString().c_str());
                }                            
            }
        }
        else
        {
            if(fDebug)LogPrint("chunks","Cannot send chunk request: %s, chunks: %d, lost permission\n",
                    request_id.ToString().c_str(),item.second.m_Pairs.size());                        
        }
    }

    row=0;
    last_row=0;
    last_count=0;
    total_size=0;
    
    mapReadPermissionCache.clear();
    sAddressesToSign.clear();
    
    while(row<=collector->m_MemPool->GetCount())
    {
        string strError;
        collect_row=NULL;
        if(row<collector->m_MemPool->GetCount())
        {
            collect_row=(mc_ChunkCollectorRow *)collector->m_MemPool->GetRow(row);
        }
        
        if( (collect_row == NULL) || 
            (last_count >= MC_CCW_MAX_CHUNKS_PER_QUERY) || 
            (total_size+collect_row->m_ChunkDef.m_Size + sizeof(mc_ChunkEntityKey)> max_total_query_size) || 
            (sAddressesToSign.size() > MC_CCW_MAX_SIGNATURES_PER_REQEST) )
        {
            if(last_count)
            {
                int extra_size=0;
                int ef_size;
                const unsigned char* ef=pEF->OFF_SupportedEnterpriseFeatures(NULL,0,&ef_size);
                if(sAddressesToSign.size())
                {
                    shift=mc_PutVarInt(buf,16,ef_size);
                    extra_size+=1+shift+ef_size;
                }
                payload.clear();
                shift=mc_PutVarInt(buf,16,last_count);
                payload.resize(extra_size+1+shift+sizeof(mc_ChunkEntityKey)*last_count);
                ptrOut=&(payload[0]);
                
                if(sAddressesToSign.size())
                {
                    shift=mc_PutVarInt(buf,16,ef_size);
                    *ptrOut=MC_RDT_ENTERPRISE_FEATURES;
                    ptrOut++;
                    memcpy(ptrOut,buf,shift);
                    ptrOut+=shift;
                    memcpy(ptrOut,ef,ef_size);
                    ptrOut+=ef_size;
                }
                
                shift=mc_PutVarInt(buf,16,last_count);
                *ptrOut=MC_RDT_CHUNK_IDS;
                ptrOut++;
                memcpy(ptrOut,buf,shift);
                ptrOut+=shift;
                for(int r=last_row;r<row;r++)
                {
                    collect_subrow=(mc_ChunkCollectorRow *)collector->m_MemPool->GetRow(r);
                    if(collect_subrow->m_State.m_Status & MC_CCF_SELECTED)
                    {
                        memcpy(ptrOut,&(collect_subrow->m_ChunkDef),sizeof(mc_ChunkEntityKey));
                        ptrOut+=sizeof(mc_ChunkEntityKey);
                    }
                }
                query_id=pRelayManager->SendRequest(NULL,MC_RMT_CHUNK_QUERY,0,payload);
                if(fDebug)LogPrint("chunks","New chunk query: %s, chunks: %d, rows [%d-%d), in queries %d (out of %d), per destination: %dKB, timeout: %d\n",query_id.ToString().c_str(),last_count,last_row,row,
                        total_in_queries,max_total_in_queries,collector->m_MaxKBPerDestination,collector->m_TimeoutRequest);
                for(int r=last_row;r<row;r++)
                {
                    collect_subrow=(mc_ChunkCollectorRow *)collector->m_MemPool->GetRow(r);
                    if(collect_subrow->m_State.m_Status & MC_CCF_SELECTED)
                    {
//                        printf("coll new qry: row: %d, id: %lu: att: %d\n",r,query_id,collect_subrow->m_State.m_QueryAttempts);
                        collect_subrow->m_State.m_Status -= MC_CCF_SELECTED;
                        collect_subrow->m_State.m_Query=query_id;
                        collect_subrow->m_State.m_QueryAttempts+=1;
                        collect_subrow->m_State.m_QueryTimeStamp=time_now+collector->m_TimeoutQuery;
                        collect_subrow->m_State.m_QuerySilenceTimestamp=time_now+collector->m_TimeoutRequest;
                        if(collect_subrow->m_State.m_QueryAttempts>1)
                        {
                            collect_subrow->m_State.m_QuerySilenceTimestamp=collect_subrow->m_State.m_QueryTimeStamp;
                        }
                        collect_subrow->m_State.m_Status |= MC_CCF_UPDATED;
                        to_end_of_query=collect_subrow->m_State.m_QueryTimeStamp-time_now;
                        if(to_end_of_query<0)to_end_of_query=0;
                        if(to_end_of_query>collector->m_TimeoutRequest)to_end_of_query=collector->m_TimeoutRequest;
                        total_in_queries+=(collect_subrow->m_ChunkDef.m_Size+ sizeof(mc_ChunkEntityKey))*to_end_of_query;
                        for(int k=0;k<2;k++)collector->m_StatTotal[k].m_Queried+=k ? collect_subrow->m_ChunkDef.m_Size : 1;                
                    }
                }
                last_row=row;
                last_count=0;     
                total_size=0;
                mapReadPermissionCache.clear();
                sAddressesToSign.clear();
            }
        }
        
        if(collect_row)
        {
            if( (collect_row->m_State.m_Status & MC_CCF_DELETED ) == 0 )
            {
                int expired=0;
                if(collect_row->m_State.m_QueryTimeStamp <= time_now)
                {
                    expired=1;
                }
                else
                {                    
                    if(collect_row->m_State.m_QuerySilenceTimestamp <= time_now)
                    {
                        query=NULL;
                        if(!collect_row->m_State.m_Query.IsZero())
                        {
                            query=pRelayManager->FindRequest(collect_row->m_State.m_Query);
                        }
                        if(query)
                        {
                            if(query->m_Responses.size() == 0)
                            {
                                expired=1;
                            }
                            pRelayManager->UnLock();
                        }                        
                    }                    
                }
                
                if(expired)
                {
                    if(!collect_row->m_State.m_Request.IsZero())
                    {
                        pRelayManager->DeleteRequest(collect_row->m_State.m_Request);
                        collect_row->m_State.m_Request=0;
                        for(int k=0;k<2;k++)collector->m_StatTotal[k].m_Undelivered+=k ? collect_row->m_ChunkDef.m_Size : 1;                
                    }
                    if(!collect_row->m_State.m_Query.IsZero())
                    {
                        map<mc_OffchainMessageID, bool>::iterator itqry = query_to_delete.find(collect_row->m_State.m_Query);
                        if (itqry == query_to_delete.end())
                        {
                            query_to_delete.insert(make_pair(collect_row->m_State.m_Query,true));
                        }       
                        collect_row->m_State.m_Query=0;
                        collect_row->m_State.m_QueryNextAttempt=time_now+MultichainNextChunkQueryAttempt(collect_row->m_State.m_QueryAttempts);      
                        collect_row->m_State.m_Status |= MC_CCF_UPDATED;
                        for(int k=0;k<2;k++)collector->m_StatTotal[k].m_Unresponded+=k ? collect_row->m_ChunkDef.m_Size : 1;                
                    }
                    if((collect_row->m_State.m_QueryNextAttempt <= time_now) && (total_in_queries < max_total_in_queries) && ((int)query_count<collector->m_MaxMemPoolSize))
                    {
                        if( (collect_row->m_State.m_Status & MC_CCF_ERROR_MASK) == 0)
                        {
                            if(mc_IsReadPermissionedStream(&(collect_row->m_ChunkDef),mapReadPermissionCache,&sAddressesToSign) == 0)
                            {
                                if(sAddressesToSign.size() <= MC_CCW_MAX_SIGNATURES_PER_REQEST)
                                {
                                    collect_row->m_State.m_Status |= MC_CCF_SELECTED;
                                    last_count++;
                                    total_size+=collect_row->m_ChunkDef.m_Size + sizeof(mc_ChunkEntityKey);
                                    query_count++;
                                }
                            }
                            else
                            {
                                unsigned char* ptrhash=collect_row->m_ChunkDef.m_Hash;
                                if(fDebug)LogPrint("chunks","Dropped chunk (lost permission) %s\n",(*(uint256*)ptrhash).ToString().c_str());                
                                collect_row->m_State.m_Status |= MC_CCF_DELETED;
                            }
                        }
                    }
                }
                else
                {
                    query_count++;
                }
            }
        }
        row++;
    }
        
    not_processed=0;
    
    for(row=0;row<collector->m_MemPool->GetCount();row++)
    {
        collect_row=(mc_ChunkCollectorRow *)collector->m_MemPool->GetRow(row);
        if( (collect_row->m_State.m_Status & MC_CCF_DELETED ) == 0 )
        {
            if(!collect_row->m_State.m_Query.IsZero())
            {
                map<mc_OffchainMessageID, bool>::iterator itqry = query_to_delete.find(collect_row->m_State.m_Query);
                if (itqry != query_to_delete.end())
                {
                    itqry->second=false;
                }            
            }
            not_processed++;
        }
    }

    BOOST_FOREACH(PAIRTYPE(const mc_OffchainMessageID, bool)& item, query_to_delete)    
    {
        if(item.second)
        {
            pRelayManager->DeleteRequest(item.first);
        }
    }    
    
/*    
    for(int k=0;k<2;k++)collector->m_StatLast[k].Zero();
    collector->m_StatLast[0].m_Pending=collector->m_TotalChunkCount;
    collector->m_StatLast[1].m_Pending=collector->m_TotalChunkSize;
    for(row=0;row<collector->m_MemPool->GetCount();row++)
    {
        collect_row=(mc_ChunkCollectorRow *)collector->m_MemPool->GetRow(row);
        size=collect_row->m_ChunkDef.m_Size;
        if( (collect_row->m_State.m_Status & MC_CCF_DELETED ) == 0 )
        {
            if(!collect_row->m_State.m_Request.IsZero())
            {
                for(int k=0;k<2;k++)collector->m_StatLast[k].m_Requested+=k ? size : 1;                
                for(int k=0;k<2;k++)collector->m_StatLast[k].m_Queried+=k ? size : 1;                                        
            }
            else
            {
                if(!collect_row->m_State.m_Query.IsZero())
                {
                    for(int k=0;k<2;k++)collector->m_StatLast[k].m_Queried+=k ? size : 1;                                        
                }
                else                    
                {
                    for(int k=0;k<2;k++)collector->m_StatLast[k].m_Sleeping+=k ? size : 1;                    
                }                
            }
        }        
        else
        {
            for(int k=0;k<2;k++)collector->m_StatLast[k].m_Pending-=k ? size : 1;                                
        }
    }
 */   
    collector->UnLock();
    if(not_processed < collector->m_MaxMemPoolSize/2)
    {
        if(collector->m_NextAutoCommitTimestamp < GetTimeMillis())
        {
            collector->Commit();
            collector->m_NextAutoCommitTimestamp=GetTimeMillis()+collector->m_AutoCommitDelay;
        }
    }
    
    int err=pEF->FED_EventChunksAvailable();
    if(err)
    {
        LogPrintf("ERROR: Cannot write offchain items to feeds, error %d\n",err);
    }
    
    
    return not_processed;
}

int MultichainCollectChunksQueueStats(mc_ChunkCollector* collector)
{
    int row;
    int delay;
    mc_ChunkCollectorRow *collect_row;
    uint32_t size;
    
    collector->Lock();    
    
    for(int k=0;k<2;k++)collector->m_StatLast[k].Zero();
    collector->m_StatLast[0].m_Pending=collector->m_TotalChunkCount;
    collector->m_StatLast[1].m_Pending=collector->m_TotalChunkSize;
    for(row=0;row<collector->m_MemPool->GetCount();row++)
    {
        collect_row=(mc_ChunkCollectorRow *)collector->m_MemPool->GetRow(row);
        size=collect_row->m_ChunkDef.m_Size;
        for(int k=0;k<2;k++)collector->m_StatLast[k].m_Undelivered+=k ? size : 1;                
        if( (collect_row->m_State.m_Status & MC_CCF_DELETED ) == 0 )
        {
            if(!collect_row->m_State.m_Request.IsZero())
            {
                for(int k=0;k<2;k++)collector->m_StatLast[k].m_Requested+=k ? size : 1;                
                for(int k=0;k<2;k++)collector->m_StatLast[k].m_Queried+=k ? size : 1;                                        
            }
            else
            {
                if(!collect_row->m_State.m_Query.IsZero())
                {
                    for(int k=0;k<2;k++)collector->m_StatLast[k].m_Queried+=k ? size : 1;                                        
                }
                else                    
                {
                    for(int k=0;k<2;k++)collector->m_StatLast[k].m_Sleeping+=k ? size : 1;                    
                }                
            }
        }        
        else
        {
            for(int k=0;k<2;k++)collector->m_StatLast[k].m_Pending-=k ? size : 1;                                
        }
    }

    collector->UnLock();    
    if(collector->m_MemPool->GetCount() > (int)(collector->m_MaxMemPoolSize*1.2))
    {
        if(collector->m_NextAutoCommitTimestamp < GetTimeMillis())
        {
            collector->Commit();
            collector->m_NextAutoCommitTimestamp=GetTimeMillis()+collector->m_AutoCommitDelay;
        }
    }
    
    delay=collector->m_StatLast[0].m_Queried;
    if(delay>MC_CCW_MAX_DELAY_BETWEEN_COLLECTS)
    {
        delay=MC_CCW_MAX_DELAY_BETWEEN_COLLECTS;
    }
    
    return delay;
}

void mc_RelayPayload_ChunkIDs(vector<unsigned char>* payload,vector <mc_ChunkEntityKey>& vChunkDefs,int size,const unsigned char* ef,int ef_size)
{
    unsigned char buf[16];
    int shift,extra_size;
    unsigned char *ptrOut;
    
    if(payload)
    {
        if(vChunkDefs.size())
        {
            extra_size=0;
            if(ef)
            {
                shift=mc_PutVarInt(buf,16,ef_size);
                extra_size+=1+shift+ef_size;
            }
            
            shift=mc_PutVarInt(buf,16,vChunkDefs.size());
            payload->resize(extra_size+1+shift+size*vChunkDefs.size());
            ptrOut=&(*payload)[0];

            if(ef)
            {
                shift=mc_PutVarInt(buf,16,ef_size);
                *ptrOut=MC_RDT_ENTERPRISE_FEATURES;
                ptrOut++;
                memcpy(ptrOut,buf,shift);
                ptrOut+=shift;
                memcpy(ptrOut,ef,ef_size);
                ptrOut+=ef_size;
            }
            
            shift=mc_PutVarInt(buf,16,vChunkDefs.size());
            *ptrOut=MC_RDT_CHUNK_IDS;
            ptrOut++;
            memcpy(ptrOut,buf,shift);
            ptrOut+=shift;

            for(int i=0;i<(int)vChunkDefs.size();i++)
            {
                memcpy(ptrOut,&vChunkDefs[i],size);
                ptrOut+=size;
            }                        
        }
    }
}

bool mc_RelayProcess_Chunk_Query(unsigned char *ptrStart,unsigned char *ptrEnd,vector<unsigned char>* payload_response,vector<unsigned char>* payload_relay,string& strError)
{
    unsigned char *ptr;
    int shift,count,size,subscriber_ef_length,publisher_ef_length;
    vector <mc_ChunkEntityKey> vToRelay;
    vector <mc_ChunkEntityKey> vToRespond;
    map<uint160,int> mapReadPermissionCache;
    mc_ChunkEntityKey chunk;
    mc_ChunkDBRow chunk_def;
    
    unsigned char *subscriber_ef=NULL;
    unsigned char *publisher_ef=NULL;
    subscriber_ef_length=0;
    publisher_ef_length=0;
        
    size=sizeof(mc_ChunkEntityKey);
    ptr=ptrStart;
    while(ptr<ptrEnd)
    {
        switch(*ptr)
        {
            case MC_RDT_ENTERPRISE_FEATURES:
                if(mc_gState->m_Features->ReadPermissions())
                {
                    ptr++;
                    subscriber_ef_length=(int)mc_GetVarInt(ptr,ptrEnd-ptr,-1,&shift);
                    ptr+=shift;
                    if(subscriber_ef_length > MC_CCW_MAX_EF_SIZE)
                    {
                        strError="Bad enterprise features object";
                        return false;                                            
                    }
                    subscriber_ef=ptr;
                    publisher_ef=pEF->OFF_SupportedEnterpriseFeatures(subscriber_ef,subscriber_ef_length,&publisher_ef_length);                    
                    ptr+=subscriber_ef_length;
                }
                else
                {
                    strError=strprintf("Request format (%d, %d) not supported in this protocol version",MC_RMT_CHUNK_QUERY,*ptr);
                    return false;                    
                }
                break;
            case MC_RDT_CHUNK_IDS:
                ptr++;
                count=(int)mc_GetVarInt(ptr,ptrEnd-ptr,-1,&shift);
                ptr+=shift;
                if(count*size != (ptrEnd-ptr))
                {
                    strError="Bad chunk ids query";
                    return false;                    
                }                
                for(int c=0;c<count;c++)
                {
                    string strErrorToIgnore;
                    chunk=*(mc_ChunkEntityKey*)ptr;
                    if( (mc_IsReadPermissionedStream(&chunk,mapReadPermissionCache,NULL) == 0) ||
                        ((pEF->LIC_VerifyFeature(MC_EFT_STREAM_READ_RESTRICTED_DELIVER,strErrorToIgnore) != 0) && 
                         (pEF->LIC_VerifyFeature(MC_EFT_NETWORK_SIGNED_RECEIVE,strErrorToIgnore) != 0) &&                          
                            (publisher_ef != NULL) ))
                    {
                        if(pwalletTxsMain->m_ChunkDB->GetChunkDef(&chunk_def,chunk.m_Hash,NULL,NULL,-1) == MC_ERR_NOERROR)
                        {
                            if(chunk_def.m_Size != chunk.m_Size)
                            {
                                chunk.m_Flags |= MC_CCF_WRONG_SIZE;
                                chunk.m_Size=chunk_def.m_Size;
                            }
                            vToRespond.push_back(chunk);
                        }                    
                        else
                        {
                            vToRelay.push_back(chunk);                                                
                        }
                    }
                    else
                    {
                        vToRelay.push_back(chunk);                                                                        
                    }
                    ptr+=size;
                }

                mc_RelayPayload_ChunkIDs(payload_response,vToRespond,size,publisher_ef,publisher_ef_length);
                mc_RelayPayload_ChunkIDs(payload_relay,vToRelay,size,subscriber_ef,subscriber_ef_length);
                break;
            default:
                strError=strprintf("Unsupported request format (%d, %d)",MC_RMT_CHUNK_QUERY,*ptr);
                return false;
        }
    }
    
    return true;
}


bool mc_RelayProcess_Chunk_Request(unsigned char *ptrStart,unsigned char *ptrEnd,vector<unsigned char>* payload_response,vector<unsigned char>* payload_relay,
        map<uint160,int>& mapReadPermissionCache,bool* read_permissioned,string& strError)
{
    unsigned char *ptr;
    int shift,count,size;
    mc_ChunkEntityKey chunk;
    mc_ChunkDBRow chunk_def;
    const unsigned char *chunk_found;
    unsigned char buf[16];
    size_t chunk_bytes;
    unsigned char *ptrOut;
    
    uint32_t total_size=0;
    uint32_t max_total_size=MAX_SIZE-OFFCHAIN_MSG_PADDING;
    uint32_t expiration=0;
    
    if(read_permissioned)
    {
        *read_permissioned=false;
    }
    
    mc_gState->m_TmpBuffers->m_RelayTmpBuffer->Clear();
    mc_gState->m_TmpBuffers->m_RelayTmpBuffer->AddElement();
            
    size=sizeof(mc_ChunkEntityKey);
    ptr=ptrStart;
    while(ptr<ptrEnd)
    {
        switch(*ptr)
        {
            case MC_RDT_EXPIRATION:
                ptr++;
                expiration=(uint32_t)mc_GetLE(ptr,sizeof(expiration));
                ptr+=sizeof(expiration);
                if(expiration+1 < pRelayManager->m_LastTime)
                {
                    strError="Less than 1s for request expiration";
                    return false;                                        
                }
                if(expiration-35 > pRelayManager->m_LastTime)                   // We are supposed to store query_hit record only for 30s, something is wrong
                {
                    strError="Expiration is too far in the future";
                    return false;                                                            
                }
                max_total_size=pwalletTxsMain->m_ChunkCollector->m_MaxMBPerSecond*(expiration-pRelayManager->m_LastTime)*1024*1024;
                break;
            case MC_RDT_CHUNK_IDS:
                ptr++;
                count=(int)mc_GetVarInt(ptr,ptrEnd-ptr,-1,&shift);
                ptr+=shift;
                if(count*size != (ptrEnd-ptr))
                {
                    strError="Bad chunk ids request";
                    return false;                    
                }                
                for(int c=0;c<count;c++)
                {
                    chunk=*(mc_ChunkEntityKey*)ptr;
                    if(read_permissioned)
                    {
                        if(mc_IsReadPermissionedStream(&chunk,mapReadPermissionCache,NULL) != 0)
                        {
                            *read_permissioned=true;
                            if(!pEF->LIC_VerifyFeature(MC_EFT_STREAM_READ_RESTRICTED_DELIVER,strError))
                            {
                                strError="Request for chunk in read-permissioned stream";
                                return false;                    
                            }
                        }
                    }
                    else
                    {
                        unsigned char* ptrhash=chunk.m_Hash;
                        if(fDebug)LogPrint("chunks","Request for chunk: %s\n",(*(uint256*)ptrhash).ToString().c_str());
                        if(pwalletTxsMain->m_ChunkDB->GetChunkDef(&chunk_def,chunk.m_Hash,NULL,NULL,-1) == MC_ERR_NOERROR)
                        {
                            if(chunk_def.m_Size != chunk.m_Size)
                            {
                                strError="Bad chunk size";
                                return false;                    
                            }
                            total_size+=chunk_def.m_Size+size;
                            if(total_size > MAX_SIZE-OFFCHAIN_MSG_PADDING)
                            {
                                strError="Total size of requested chunks is too big for message";
                                return false;                                                
                            }
                            if(total_size > max_total_size)
                            {
                                strError="Total size of requested chunks is too big for response expiration";
                                return false;                                                
                            }
                            
                            unsigned char salt[MC_CDB_CHUNK_SALT_SIZE];
                            uint32_t salt_size;
                            
                            chunk_found=pwalletTxsMain->m_ChunkDB->GetChunk(&chunk_def,0,-1,&chunk_bytes,salt,&salt_size);
                            mc_gState->m_TmpBuffers->m_RelayTmpBuffer->SetData((unsigned char*)&chunk,size);
                            mc_gState->m_TmpBuffers->m_RelayTmpBuffer->SetData(salt,salt_size);
                            mc_gState->m_TmpBuffers->m_RelayTmpBuffer->SetData(chunk_found,chunk_bytes);
                        }                    
                        else
                        {
                            strError="Chunk not found";
                            return false;                    
                        }
                    }
                    ptr+=size;
                }
                
                if(!read_permissioned)
                {
                    chunk_found=mc_gState->m_TmpBuffers->m_RelayTmpBuffer->GetData(0,&chunk_bytes);
                    shift=mc_PutVarInt(buf,16,count);
                    payload_response->resize(1+shift+chunk_bytes);
                    ptrOut=&(*payload_response)[0];

                    *ptrOut=MC_RDT_CHUNKS;
                    ptrOut++;
                    memcpy(ptrOut,buf,shift);
                    ptrOut+=shift;
                    memcpy(ptrOut,chunk_found,chunk_bytes);
                    ptrOut+=chunk_bytes;
                }
                
                break;
            default:
                if(read_permissioned)
                {
                    *read_permissioned=true;
                    if(!pEF->LIC_VerifyFeature(MC_EFT_STREAM_READ_RESTRICTED_DELIVER,strError))
                    {
                        strError=strprintf("Unsupported request format (%d, %d)",MC_RMT_CHUNK_REQUEST,*ptr);
                    }
                    return false;
                }
        }
    }

    if(total_size > max_total_size)
    {
        strError="Total size of requested chunks is too big for response expiration";
        return false;                                                
    }
    
    return true;
}

bool mc_RelayProcess_Address_Query(unsigned char *ptrStart,unsigned char *ptrEnd,vector<unsigned char>* payload_response,vector<unsigned char>* payload_relay,string& strError)
{
    unsigned char *ptr;
    unsigned char *ptrOut;
    unsigned char buf[16];
    int shift;
    CKey key;
    
    ptr=ptrStart;
    while(ptr<ptrEnd)
    {
        switch(*ptr)
        {
            case MC_RDT_MC_ADDRESS:
                ptr++;
                if(sizeof(CKeyID) != (ptrEnd-ptr))
                {
                    strError="Bad address query request";
                    return false;
                }

                if(pwalletMain->GetKey(*(CKeyID*)ptr, key))
                {
                    if(payload_response)
                    {
                        shift=mc_PutVarInt(buf,16,pRelayManager->m_MyAddress.m_NetAddresses.size());
                        payload_response->resize(1+sizeof(CKeyID)+1+shift+sizeof(CAddress)*pRelayManager->m_MyAddress.m_NetAddresses.size());
                        ptrOut=&(*payload_response)[0];
                        
                        *ptrOut=MC_RDT_MC_ADDRESS;
                        ptrOut++;
                        *(CKeyID*)ptrOut=pRelayManager->m_MyAddress.m_Address;
                        ptrOut+=sizeof(CKeyID);
                        
                        *ptrOut=MC_RDT_NET_ADDRESS;
                        ptrOut++;
                        memcpy(ptrOut,buf,shift);
                        ptrOut+=shift;
                        for(int i=0;i<(int)pRelayManager->m_MyAddress.m_NetAddresses.size();i++)
                        {
                            memcpy(ptrOut,&(pRelayManager->m_MyAddress.m_NetAddresses[i]),sizeof(CAddress));
                            ptrOut+=sizeof(CAddress);
                        }                        
                    }
                }
                else
                {
                    if(payload_relay)
                    {
                        payload_relay->resize(ptrEnd-ptrStart);
                        memcpy(&(*payload_relay)[0],ptrStart,ptrEnd-ptrStart);                    
                    }
                }
                
                ptr+=sizeof(CKeyID);
                break;
            default:
                strError=strprintf("Unsupported request format (%d, %d)",MC_RMT_MC_ADDRESS_QUERY,*ptr);
                return false;
        }
    }
    
    return true;
}

bool MultichainRelayResponse(uint32_t msg_type_stored, CNode *pto_stored,
                             uint32_t msg_type_in, uint32_t  flags, vector<unsigned char>& vPayloadIn,vector<CScript>& vSigScriptsIn,vector<CScript>& vSigScriptsToVerify,
                             uint32_t* msg_type_response,uint32_t  *flags_response,vector<unsigned char>& vPayloadResponse,vector<CScript>& vSigScriptsRespond,
                             uint32_t* msg_type_relay,uint32_t  *flags_relay,vector<unsigned char>& vPayloadRelay,vector<CScript>& vSigScriptsRelay,string& strError)
{
    unsigned char *ptr;
    unsigned char *ptrEnd;
    vector<unsigned char> *payload_relay_ptr=NULL;
    vector<unsigned char> *payload_response_ptr=NULL;
    
    if(msg_type_response)
    {
        payload_response_ptr=&vPayloadResponse;
    }
    
    if(msg_type_relay)
    {
        payload_relay_ptr=&vPayloadRelay;
    }
    
    ptr=&vPayloadIn[0];
    ptrEnd=ptr+vPayloadIn.size();
            
//    mc_DumpSize("H",ptr,ptrEnd-ptr,32);
    strError="";
    switch(msg_type_in)
    {
        case MC_RMT_MC_ADDRESS_QUERY:
            if(msg_type_stored)
            {
                strError=strprintf("Unexpected response message type (%s,%s)",mc_MsgTypeStr(msg_type_stored).c_str(),mc_MsgTypeStr(msg_type_in).c_str());
                goto exitlbl;
            } 
            if(payload_response_ptr == NULL)
            {
                if(payload_relay_ptr)
                {
                    vPayloadRelay=vPayloadIn;
                    *msg_type_relay=msg_type_in;                    
                }
            }
            else
            {
                if(mc_RelayProcess_Address_Query(ptr,ptrEnd,payload_response_ptr,payload_relay_ptr,strError))
                {
                    if(payload_response_ptr && (payload_response_ptr->size() != 0))
                    {
                        *msg_type_response=MC_RMT_NODE_DETAILS;
                    }
                    if(payload_relay_ptr && (payload_relay_ptr->size() != 0))
                    {
                        *msg_type_relay=MC_RMT_MC_ADDRESS_QUERY;
                    }
                }
            }
            break;
        case MC_RMT_NODE_DETAILS:
            if(msg_type_stored != MC_RMT_MC_ADDRESS_QUERY)
            {
                strError=strprintf("Unexpected response message type (%s,%s)",mc_MsgTypeStr(msg_type_stored).c_str(),mc_MsgTypeStr(msg_type_in).c_str());
                goto exitlbl;
            } 
            if(payload_response_ptr)
            {
                *msg_type_response=MC_RMT_ADD_RESPONSE;
            }

            break;
        case MC_RMT_CHUNK_QUERY:
            if(msg_type_stored)
            {
                strError=strprintf("Unexpected response message type (%s,%s)",mc_MsgTypeStr(msg_type_stored).c_str(),mc_MsgTypeStr(msg_type_in).c_str());
                goto exitlbl;
            } 
            if(payload_response_ptr == NULL)
            {
                if(payload_relay_ptr)
                {
                    vPayloadRelay=vPayloadIn;
                    *msg_type_relay=msg_type_in;                    
                }
            }
            else
            {
                if(mc_RelayProcess_Chunk_Query(ptr,ptrEnd,payload_response_ptr,payload_relay_ptr,strError))
                {
                    if(payload_response_ptr && (payload_response_ptr->size() != 0))
                    {
                        *msg_type_response=MC_RMT_CHUNK_QUERY_HIT;
                    }
                    if(payload_relay_ptr && (payload_relay_ptr->size() != 0))
                    {
                        *msg_type_relay=MC_RMT_CHUNK_QUERY;
                    }
                }
            }            
            break;
        case MC_RMT_CHUNK_QUERY_HIT:
            if(msg_type_stored != MC_RMT_CHUNK_QUERY)
            {
                strError=strprintf("Unexpected response message type (%s,%s)",mc_MsgTypeStr(msg_type_stored).c_str(),mc_MsgTypeStr(msg_type_in).c_str());
                goto exitlbl;
            } 
            if(payload_response_ptr)
            {
                *msg_type_response=MC_RMT_ADD_RESPONSE;
            }
            break;
        case MC_RMT_CHUNK_REQUEST:
            if(msg_type_stored != MC_RMT_CHUNK_QUERY_HIT)
            {
                strError=strprintf("Unexpected response message type (%s,%s)",mc_MsgTypeStr(msg_type_stored).c_str(),mc_MsgTypeStr(msg_type_in).c_str());
                goto exitlbl;
            } 
            if(payload_response_ptr == NULL)
            {
                if(payload_relay_ptr)
                {
                    vPayloadRelay=vPayloadIn;
                    *msg_type_relay=msg_type_in;                    
                }
            }
            else
            {
                map<uint160,int> mapReadPermissionCache;
                bool read_permissioned;
                if(!mc_RelayProcess_Chunk_Request(ptr,ptrEnd,payload_response_ptr,payload_relay_ptr,mapReadPermissionCache,&read_permissioned,strError))
                {
                    if(!read_permissioned)
                    {
                        goto exitlbl;                            
                    }
                }
                vSigScriptsToVerify.clear();
                if(read_permissioned)
                {
                    if(!pEF->OFF_ProcessChunkRequest(ptr,ptrEnd,payload_response_ptr,payload_relay_ptr,mapReadPermissionCache,strError))
                    {
                        goto exitlbl;                                                    
                    }
                    if(!pEF->OFF_GetScriptsToVerify(mapReadPermissionCache,vSigScriptsIn,vSigScriptsToVerify,strError))
                    {
                        goto exitlbl;                            
                    }
                }
                else
                {
                    if(!mc_RelayProcess_Chunk_Request(ptr,ptrEnd,payload_response_ptr,payload_relay_ptr,mapReadPermissionCache,NULL,strError))
                    {
                        goto exitlbl;                            
                    }                    
                }
                
                if(payload_response_ptr && (payload_response_ptr->size() != 0))
                {
                    vSigScriptsRespond.clear();
                    *msg_type_response=MC_RMT_CHUNK_RESPONSE;
                }
                if(payload_relay_ptr && (payload_relay_ptr->size() != 0))
                {
                    vSigScriptsRelay=vSigScriptsIn;
                    *msg_type_relay=MC_RMT_CHUNK_REQUEST;
                }
            }         
            
            break;
        case MC_RMT_CHUNK_RESPONSE:
            if(msg_type_stored != MC_RMT_CHUNK_REQUEST)
            {
                strError=strprintf("Unexpected response message type (%s,%s)",mc_MsgTypeStr(msg_type_stored).c_str(),mc_MsgTypeStr(msg_type_in).c_str());
                goto exitlbl;
            } 
            if(payload_response_ptr)
            {
                *msg_type_response=MC_RMT_ADD_RESPONSE;
            }

            break;
    }
    
exitlbl:
            
    if(strError.size())
    {
        return false;
    }

    return true;
}

void mc_Limiter::Zero()
{
    memset(this,0,sizeof(mc_Limiter));    
}

int mc_Limiter::Initialize(int seconds,int measures)
{
    Zero();
    
    if(seconds > MC_LIM_MAX_SECONDS)
    {
        return MC_ERR_INVALID_PARAMETER_VALUE;
    }
    if(measures > MC_LIM_MAX_MEASURES)
    {
        return MC_ERR_INVALID_PARAMETER_VALUE;
    }
    m_SecondCount=seconds;
    m_MeasureCount=measures;
    m_Time=mc_TimeNowAsUInt();
    
    return MC_ERR_NOERROR;
}

int mc_Limiter::SetLimit(int meausure, int64_t limit)
{
    if( (meausure > MC_LIM_MAX_MEASURES) || (meausure <0) )
    {
        return MC_ERR_INVALID_PARAMETER_VALUE;        
    }
    m_Limits[meausure]=limit*m_SecondCount;
    return MC_ERR_NOERROR;    
}

void mc_Limiter::CheckTime()
{
    CheckTime(mc_TimeNowAsUInt());
}

void mc_Limiter::CheckTime(uint32_t time_now)
{
    if(m_SecondCount == 0)
    {
        return;        
    }
    if(time_now == m_Time)
    {
        return;
    }
    if(time_now >= m_Time + m_SecondCount)
    {
        memset(m_Totals,0,MC_LIM_MAX_MEASURES);
        memset(m_Measures,0,MC_LIM_MAX_MEASURES*MC_LIM_MAX_SECONDS);
    }
    
    for(uint32_t t=m_Time;t<time_now;t++)
    {
        int p=(t+1)%m_SecondCount;
        for(int m=0;m<m_MeasureCount;m++)
        {
            m_Totals[m]-=m_Measures[m*MC_LIM_MAX_SECONDS+p];
            m_Measures[m*MC_LIM_MAX_SECONDS+p]=0;
        }
    }
    
    m_Time=time_now;
}

void mc_Limiter::SetEvent(int64_t m1)
{
    SetEvent(m1,0,0,0);
}

void mc_Limiter::SetEvent(int64_t m1,int64_t m2)
{
    SetEvent(m1,m2,0,0);    
}

void mc_Limiter::SetEvent(int64_t m1,int64_t m2,int64_t m3)
{
    SetEvent(m1,m2,m3,0);        
}

void mc_Limiter::SetEvent(int64_t m1,int64_t m2,int64_t m3,int64_t m4)
{
    m_Event[0]=m1;
    m_Event[1]=m2;
    m_Event[2]=m3;
    m_Event[3]=m4;
}

int mc_Limiter::Disallowed()
{
    return Disallowed(mc_TimeNowAsUInt());
}

int mc_Limiter::Disallowed(uint32_t t)
{
    CheckTime(t);
    
    for(int m=0;m<m_MeasureCount;m++)
    {
        if(m_Totals[m]+m_Event[m] > m_Limits[m])
        {
            return 1;
        }
    }
    
    return 0;
}

void mc_Limiter::Increment()
{
    if(m_SecondCount == 0)
    {
        return;        
    }
    
    int p=(m_Time+1)%m_SecondCount;
    for(int m=0;m<m_MeasureCount;m++)
    {
        m_Totals[m]+=m_Event[m];
        m_Measures[m*MC_LIM_MAX_SECONDS+p]+=m_Event[m];
    }    
}

void mc_NodeFullAddress::Zero()
{
    m_Address=CKeyID(0);
    m_NetAddresses.clear();
}

void mc_RelayManager::SetMyIPs(uint32_t *ips,int ip_count)
{
    m_MyIPCount=ip_count;
    for(int i=0;i<ip_count;i++)
    {
        m_MyIPs[i]=ips[i];
    }
}

void mc_RelayManager::InitNodeAddress(mc_NodeFullAddress *node_address,CNode* pto,uint32_t action)
{
    uint32_t pto_address_local;
    in_addr addr;
    CKey key;
    CPubKey pkey;            
    bool key_found=false;
    
    if(action & MC_PRA_MY_ORIGIN_MC_ADDRESS)
    {
        if(pto)
        {
            node_address->m_Address=pto->kAddrLocal;        
        }
    }
    else
    {
        if(pto == NULL)
        {
            if(mapArgs.count("-handshakelocal"))
            {
                CBitcoinAddress address(mapArgs["-handshakelocal"]);
                if (address.IsValid())    
                {
                    CTxDestination dst=address.Get();
                    CKeyID *lpKeyID=boost::get<CKeyID> (&dst);
                    if(lpKeyID)
                    {
                        if(pwalletMain->GetKey(*lpKeyID, key))
                        {
                            node_address->m_Address=*lpKeyID;
                            key_found=true;
                        }
                    }
                }        
            }

            if(!key_found)
            {
                if(!pwalletMain->GetKeyFromAddressBook(pkey,MC_PTP_CONNECT))
                {
                    pkey=pwalletMain->vchDefaultKey;
                }
                node_address->m_Address=pkey.GetID();
            }  
        }
    }
    
    node_address->m_NetAddresses.clear();
    
    pto_address_local=0;
    if(action & MC_PRA_MY_ORIGIN_NT_ADDRESS)
    {
        node_address->m_NetAddresses.push_back(CAddress(pto->addrLocal));

        if(pto->addrLocal.IsIPv4())
        {
            pto_address_local=(pto->addrLocal.GetByte(3)<<24)+(pto->addrLocal.GetByte(2)<<16)+(pto->addrLocal.GetByte(1)<<8)+pto->addrLocal.GetByte(0);            
            addr.s_addr=pto_address_local;
            node_address->m_NetAddresses.push_back(CAddress(CService(addr,GetListenPort())));
        }
    }
    
    
    for(int i=0;i<m_MyIPCount;i++)
    {
        if(m_MyIPs[i] != pto_address_local)
        {
            addr.s_addr=htonl(m_MyIPs[i]);
            node_address->m_NetAddresses.push_back(CAddress(CService(addr,GetListenPort())));
        }
    }        
}

void mc_RelayManager::InitNodeAddress(mc_NodeFullAddress *node_address,CKeyID& mc_address, vector<CAddress>& net_addresses)
{
    node_address->m_Address=mc_address;
    node_address->m_NetAddresses=net_addresses;
}

void mc_RelayManager::MsgTypeSettings(uint32_t msg_type,int latency,int seconds,int64_t serves_per_second,int64_t bytes_per_second)
{
    mc_Limiter limiter;
    
    map<uint32_t, int>::iterator itlat = m_Latency.find(msg_type);
    if (itlat == m_Latency.end())
    {
        m_Latency.insert(make_pair(msg_type,latency));
    }                    
    else
    {
        itlat->second=latency;
    }
    
    limiter.Initialize(seconds,2);
    limiter.SetLimit(0,serves_per_second);
    limiter.SetLimit(1,bytes_per_second);
    
    map<uint32_t, mc_Limiter>::iterator itlim = m_Limiters.find(msg_type);
    if (itlim == m_Limiters.end())
    {
        m_Limiters.insert(make_pair(msg_type,limiter));
    }                    
    else
    {
        itlim->second=limiter;
    }
}



void mc_RelayManager::Zero()
{
    m_Semaphore=NULL;
    m_LockedBy=0;         
    m_LastTime=0;
}

void mc_RelayManager::Destroy()
{
    if(m_Semaphore)
    {
        __US_SemDestroy(m_Semaphore);
    }
    
    Zero();    
}

int mc_RelayManager::Initialize()
{
    m_Semaphore=__US_SemCreate();
    InitNodeAddress(&m_MyAddress,NULL,MC_PRA_NONE);
    SetDefaults();
    return MC_ERR_NOERROR;    
}

int mc_RelayManager::Lock(int write_mode,int allow_secondary)
{        
    uint64_t this_thread;
    this_thread=__US_ThreadID();
    
    if(this_thread == m_LockedBy)
    {
        return allow_secondary;
    }
    __US_SemWait(m_Semaphore); 
    m_LockedBy=this_thread;
    
    return 0;
}

void mc_RelayManager::UnLock()
{    
    m_LockedBy=0;
    __US_SemPost(m_Semaphore);
}

int mc_RelayManager::Lock()
{        
    return Lock(1,0);
}

void mc_RelayManager::SetDefaults()
{
    MsgTypeSettings(MC_RMT_NONE            , 0,10,1000,100*1024*1024);
    MsgTypeSettings(MC_RMT_MC_ADDRESS_QUERY,10,10, 100,  1*1024*1024);
    MsgTypeSettings(MC_RMT_NODE_DETAILS    , 0,10, 100,  1*1024*1024);
    MsgTypeSettings(MC_RMT_CHUNK_QUERY     ,pwalletTxsMain->m_ChunkCollector->m_TimeoutQuery+5,10, 100,  1*1024*1024);
    MsgTypeSettings(MC_RMT_CHUNK_QUERY_HIT ,pwalletTxsMain->m_ChunkCollector->m_TimeoutQuery+5,10, 100,  1*1024*1024);
    MsgTypeSettings(MC_RMT_CHUNK_REQUEST   ,pwalletTxsMain->m_ChunkCollector->m_TimeoutRequest+5,10, 100, (pwalletTxsMain->m_ChunkCollector->m_MaxMBPerSecond+2)*1024*1024);
    MsgTypeSettings(MC_RMT_CHUNK_RESPONSE  , 0,10, 100,100*1024*1024);
    MsgTypeSettings(MC_RMT_ERROR_IN_MESSAGE,30,10,1000,  1*1024*1024);
    MsgTypeSettings(MC_RMT_NEW_REQUEST     ,30,10,1000,  1*1024*1024);
    
    
    m_MinTimeShift=2 * 6 * Params().TargetSpacing();
    m_MaxTimeShift=2 * 6 * Params().TargetSpacing();
    m_MaxResponses=16;
}

void mc_RelayManager::CheckTime()
{
    uint32_t time_now=mc_TimeNowAsUInt();
    if(time_now == m_LastTime)
    {
        return;
    }

    Lock();
    for(map<const mc_RelayRecordKey,mc_RelayRecordValue>::iterator it = m_RelayRecords.begin(); it != m_RelayRecords.end();)
    {
        if(it->second.m_Timestamp < m_LastTime)
        {
/*            
            mc_OffchainMessageID msg_id=it->first.m_ID;
            if(fDebug)LogPrint("offchain","Offchain rrdl:  %s, from: %d, to: %d, msg: %s, now: %d, exp: %d\n",
            msg_id.ToString().c_str(),it->second.m_NodeFrom,it->first.m_NodeTo,mc_MsgTypeStr(it->second.m_MsgType).c_str(),m_LastTime,it->second.m_Timestamp);
*/
            m_RelayRecords.erase(it++);
        }
        else
        {
            it++;
        }
    }        
    m_LastTime=time_now;
    UnLock();
}

void mc_RelayManager::SetRelayRecord(CNode *pto,CNode *pfrom,uint32_t msg_type,mc_OffchainMessageID msg_id)
{
    map<uint32_t, int>::iterator itlat = m_Latency.find(msg_type);
    if (itlat == m_Latency.end())
    {
        return;
    }                    
    if(itlat->second <= 0 )
    {
        return;        
    }    
    
    NodeId pto_id=0;
    if(pto)
    {
        pto_id=pto->GetId();
    }
    const mc_RelayRecordKey key=mc_RelayRecordKey(msg_id,pto_id);
    mc_RelayRecordValue value;
    value.m_NodeFrom=0;
    if(pfrom)
    {
        value.m_NodeFrom=pfrom->GetId();
    }
    value.m_MsgType=msg_type;
    value.m_Timestamp=m_LastTime+itlat->second;
    value.m_Count=1;
    
    map<const mc_RelayRecordKey, mc_RelayRecordValue>::iterator it = m_RelayRecords.find(key);
    if (it == m_RelayRecords.end())
    {
        m_RelayRecords.insert(make_pair(key,value));
    }                    
    else
    {
        value.m_Timestamp=(it->second).m_Timestamp;
        value.m_Count=(it->second).m_Count;
        it->second=value;
    }
/*   
    if(fDebug)LogPrint("offchain","Offchain rrst:  %s, from: %d, to: %d, msg: %s, now: %d, exp: %d\n",
    msg_id.ToString().c_str(),pfrom ? pfrom->GetId() : 0,pto ? pto->GetId() : 0,mc_MsgTypeStr(msg_type).c_str(),m_LastTime,value.m_Timestamp);
*/    

}

int mc_RelayManager::GetRelayRecord(CNode *pfrom,mc_OffchainMessageID msg_id,uint32_t* msg_type,CNode **pto)
{
    NodeId pfrom_id,pto_id;
    
    pfrom_id=0;
    if(pfrom)
    {
        pfrom_id=pfrom->GetId();
    }
//    printf("getrr: %d, ts: %u, nc: %u\n",pfrom_id,timestamp,nonce);
    const mc_RelayRecordKey key=mc_RelayRecordKey(msg_id,pfrom_id);
    map<const mc_RelayRecordKey, mc_RelayRecordValue>::iterator it = m_RelayRecords.find(key);
    if (it == m_RelayRecords.end())
    {
        return MC_ERR_NOT_FOUND;
    }
    
    if(it->second.m_MsgType == MC_RMT_ERROR_IN_MESSAGE)
    {
        return MC_ERR_ERROR_IN_SCRIPT;
    }
    
    if(msg_type)
    {
        *msg_type=it->second.m_MsgType;        
    }
    
    
    if(pto)
    {
        pto_id=it->second.m_NodeFrom;
    
        if(pto_id)
        {
            LOCK(cs_vNodes);
            BOOST_FOREACH(CNode* pnode, vNodes)
            {
                if(pnode->GetId() == pto_id)
                {
                    *pto=pnode;
                    return MC_ERR_NOERROR;
                }
            }
        }
        else
        {
            return MC_ERR_NOERROR;            
        }
    }
    else
    {
        it->second.m_Count+=1;
        if(it->second.m_Count > m_MaxResponses)
        {
            return MC_ERR_NOT_ALLOWED;            
        }
        return MC_ERR_NOERROR;                    
    }
    
    return MC_ERR_NOT_ALLOWED;
}

mc_OffchainMessageID mc_RelayManager::GenerateMsgID(uint32_t timestamp)
{
    mc_OffchainMessageID msg_id;
    msg_id.m_TimeStamp=timestamp;
    GetRandBytes((unsigned char*)&(msg_id.m_Nonce), sizeof(msg_id.m_Nonce));
    return msg_id;
}

mc_OffchainMessageID mc_RelayManager::GenerateMsgID()
{
    return GenerateMsgID(mc_TimeNowAsUInt());
}

uint32_t mc_RelayManager::GenerateNonce()
{
    uint32_t nonce;     
    
    GetRandBytes((unsigned char*)&nonce, sizeof(nonce));
    
    nonce &= 0x7FFFFFFF;
    
    return nonce;
}

mc_OffchainMessageID mc_RelayManager::PushRelay(CNode*    pto, 
                                uint32_t  msg_format,        
                                vector <int32_t> &vHops,
                                vector <int32_t> &vSendPaths,                                
                                uint32_t  msg_type,
                                mc_OffchainMessageID msg_id,
                                mc_OffchainMessageID msg_id_to_respond,
                                uint32_t  flags,
                                vector<unsigned char>& payload,
                                vector<CScript>&  sigScripts_to_relay,
                                CNode*    pfrom, 
                                uint32_t  action)
{
    vector <unsigned char> vOriginAddress;
    vector <unsigned char> vDestinationAddress;    
    vector<CScript>  sigScripts;
    CScript sigScript;
    uint256 message_hash;
    
    if( (action & MC_PRA_SIGN_WITH_HANDSHAKE_ADDRESS) && (MCP_ANYONE_CAN_CONNECT == 0) )
    {
        CHashWriter ssHash(SER_GETHASH, 0);
        ssHash << msg_type;        
        ssHash << msg_id.m_TimeStamp;
        ssHash << msg_id.m_Nonce;
        ssHash << msg_id_to_respond.m_TimeStamp;
        ssHash << msg_id_to_respond.m_Nonce;
        ssHash << flags;
        ssHash << payload;
        
        message_hash=ssHash.GetHash();    
        
        CHashWriter ssSig(SER_GETHASH, 0);
        
        ssSig << message_hash;
        ssSig << vector<unsigned char>((unsigned char*)&msg_id_to_respond, (unsigned char*)&msg_id_to_respond+sizeof(msg_id_to_respond));
        uint256 signed_hash=ssSig.GetHash();
        CKey key;
        CKeyID keyID;
        CPubKey pkey;            

        keyID=pto->kAddrLocal;
        
        if(pwalletMain->GetKey(keyID, key))
        {
            pkey=key.GetPubKey();
            vector<unsigned char> vchSig;
            sigScript.clear();
            if (key.Sign(signed_hash, vchSig))
            {
                vchSig.push_back(0x00);
                sigScript << vchSig;
                sigScript << ToByteVector(pkey);
            }
            else
            {
                LogPrintf("PushRelay(): Internal error: Cannot sign\n");                
            }
        }
        else
        {
            LogPrintf("PushRelay(): Internal error: Cannot find key for signature\n");
        }
        sigScripts.push_back(sigScript);
    }    
    
    for(unsigned int i=0;i<sigScripts_to_relay.size();i++)
    {
        sigScripts.push_back(sigScripts_to_relay[i]);
    }
    
    if(pfrom)
    {
        vHops.push_back((int32_t)pfrom->GetId());
    }
    
//    printf("send: %d, to: %d, from: %d, hc: %d, size: %d, ts: %u, nc: %u\n",msg_type,pto->GetId(),pfrom ? pfrom->GetId() : 0,(int)vHops.size(),(int)payload.size(),timestamp,nonce);
    if(fDebug)LogPrint("offchain","Offchain send: %s, request: %s, to: %d, from: %d, msg: %s, hops: %d, size: %d\n",
            msg_id.ToString().c_str(),msg_id_to_respond.ToString().c_str(),pto->GetId(),pfrom ? pfrom->GetId() : 0,mc_MsgTypeStr(msg_type).c_str(),(int)vHops.size(),(int)payload.size());
    pto->PushMessage("offchain",
                        msg_format,
                        vHops,
                        vSendPaths,
                        msg_type,
                        msg_id.m_TimeStamp,
                        msg_id.m_Nonce,
                        msg_id_to_respond.m_TimeStamp,
                        msg_id_to_respond.m_Nonce,
                        flags,
                        payload,
                        sigScripts);        
    
//    SetRelayRecord(pto,NULL,msg_type,timestamp,nonce);
    SetRelayRecord(pto,pfrom,msg_type,msg_id);
    
    return msg_id;
}
    
bool mc_RelayManager::ProcessRelay( CNode* pfrom, 
                                    CDataStream& vRecv, 
                                    CValidationState &state, 
                                    uint32_t verify_flags_in)
{
    uint32_t  msg_type_in;
    uint32_t verify_flags;
    int err;
    mc_OffchainMessageID msg_id_received;
    mc_OffchainMessageID msg_id_to_respond;
    vector<unsigned char> vPayloadIn;
    vector<CScript> vSigScripts;
    vector<CScript> vSigScriptsToVerify;
    vector<CScript> vSigScriptsRespond;
    vector<CScript> vSigScriptsRelay;
    vector <int32_t> vHops;
    vector <int32_t> vHopsToRelay;
    vector <int32_t> vSendPaths;
    vector <int32_t> vEmptyHops;
    uint256   message_hash;
    uint32_t  flags_in,flags_response,flags_relay;
    vector<unsigned char> vchSigOut;
    vector<unsigned char> vchPubKey;
    CPubKey pubkey;
    vector<CAddress> path;    
    CNode *pto_stored;
    uint32_t msg_type_stored;
    uint32_t msg_format;
    unsigned char hop_count;
    uint32_t msg_type_response,msg_type_relay;
    uint32_t *msg_type_relay_ptr;
    uint32_t *msg_type_response_ptr;
    vector<unsigned char> vPayloadResponse;
    vector<unsigned char> vPayloadRelay;
    string strError;    
    
    msg_type_stored=MC_RMT_NONE;
    msg_type_response=MC_RMT_NONE;
    msg_type_relay=MC_RMT_NONE;
    pto_stored=NULL;
    
    
    flags_response=0x00;
    flags_relay=0x00;
    
    if(true)    
    {
        string strPayload;
        strPayload.resize(2*(int)vRecv.size()+1,'Z');
        mc_BinToHex(&strPayload[0],&vRecv.str()[0],(int)vRecv.size());

        if(fDebug)LogPrint("offchain","Offchain mesg: %s\n",strPayload.c_str());
    }
    
    
    verify_flags=verify_flags_in;
    vRecv >> msg_format;
    if(msg_format != 0)
    {
        LogPrintf("ProcessOffchain() : Unsupported message format %08X\n",msg_format);     
        return false;        
    }
        
    vRecv >> vHops;
    hop_count=(int)vHops.size();
    vRecv >> vSendPaths;
    if(vSendPaths.size())
    {
        LogPrintf("ProcessOffchain() : Unsupported send path\n");     
        return false;                
    }
    vRecv >> msg_type_in;
    switch(msg_type_in)
    {
        case MC_RMT_MC_ADDRESS_QUERY:
            verify_flags |= MC_VRA_IS_NOT_RESPONSE | MC_VRA_NOT_ALLOWED;
            break;
        case MC_RMT_NODE_DETAILS:
            verify_flags |= MC_VRA_IS_RESPONSE | MC_VRA_SIGNATURE_ORIGIN  | MC_VRA_NOT_ALLOWED;
            break;
        case MC_RMT_CHUNK_QUERY:
            verify_flags |= MC_VRA_IS_NOT_RESPONSE;
            break;
        case MC_RMT_CHUNK_QUERY_HIT:
            verify_flags |= MC_VRA_IS_RESPONSE | MC_VRA_SIGNATURE_ORIGIN | MC_VRA_PROCESS_ONCE;
            break;            
        case MC_RMT_CHUNK_REQUEST:
            verify_flags |= MC_VRA_IS_RESPONSE | MC_VRA_DOUBLE_HOP | MC_VRA_SIGNATURE_ORIGIN;
            break;
        case MC_RMT_CHUNK_RESPONSE:
            verify_flags |= MC_VRA_IS_RESPONSE | MC_VRA_DOUBLE_HOP | MC_VRA_SIGNATURE_ORIGIN | MC_VRA_PROCESS_ONCE;
            break;
        default:
            if(verify_flags & MC_VRA_MESSAGE_TYPE)    
            {
                LogPrintf("ProcessOffchain() : Unsupported offchain message type %s\n",mc_MsgTypeStr(msg_type_in).c_str());     
                return false;
            }
            break;
    }
    
    if(verify_flags & MC_VRA_NOT_ALLOWED)
    {
        LogPrintf("ProcessOffchain() : Not allowed offchain message type %s\n",mc_MsgTypeStr(msg_type_in).c_str());             
        return false;            
    }
    
    if(verify_flags & MC_VRA_SINGLE_HOP)
    {
        if(hop_count)
        {
            LogPrintf("ProcessOffchain() : Unsupported hop count %d for msg type %s\n",hop_count,mc_MsgTypeStr(msg_type_in).c_str());     
            return false;            
        }
    }
    
    if(verify_flags & MC_VRA_DOUBLE_HOP)
    {
        if(hop_count > 1)
        {
            LogPrintf("ProcessOffchain() : Unsupported hop count %d for msg type %s\n",hop_count,mc_MsgTypeStr(msg_type_in).c_str());     
            return false;            
        }
    }
        
    CheckTime();
        
    vRecv >> msg_id_received.m_TimeStamp;
    
    if(verify_flags & MC_VRA_TIMESTAMP)
    {
        if(msg_id_received.m_TimeStamp+m_MinTimeShift < m_LastTime)
        {
            LogPrintf("ProcessOffchain() : Timestamp too far in the past: %d\n",msg_id_received.m_TimeStamp);     
            return false;                        
        }
        if(msg_id_received.m_TimeStamp > m_LastTime + m_MaxTimeShift)
        {
            LogPrintf("ProcessOffchain() : Timestamp too far in the future: %d\n",msg_id_received.m_TimeStamp);     
            return false;                        
        }
    }    
    
    vRecv >> msg_id_received.m_Nonce;    
    
    
    msg_type_relay_ptr=&msg_type_relay;
    msg_type_response_ptr=&msg_type_response;
    
    if( verify_flags & (MC_VRA_PROCESS_ONCE | MC_VRA_BROADCAST_ONCE))
    {
        switch(GetRelayRecord(NULL,msg_id_received,NULL,NULL))
        {
            case MC_ERR_NOERROR:
                if(verify_flags & MC_VRA_PROCESS_ONCE)
                {
                    return false;
                }
                if(verify_flags & MC_VRA_BROADCAST_ONCE)
                {
                    msg_type_relay_ptr=NULL;
                }
                break;
            case MC_ERR_ERROR_IN_SCRIPT:                                        // We already processed this message, it has errors
                return false;                                        
            case MC_ERR_NOT_ALLOWED:
                LogPrintf("ProcessOffchain() : Processing this message is not allowed by current limits or requesting peer was disconnected\n");     
                return false;                                        
        }
    }
    
    if(verify_flags & MC_VRA_DOUBLE_HOP)
    {
        if(hop_count)
        {
            msg_type_relay_ptr=NULL;
        }
    }
    
        
    vRecv >> msg_id_to_respond.m_TimeStamp;
    vRecv >> msg_id_to_respond.m_Nonce;
    
    if( verify_flags & MC_VRA_IS_NOT_RESPONSE ) 
    {
        if( (msg_id_to_respond.m_TimeStamp != 0) || (msg_id_to_respond.m_Nonce != 0) )
        {
            return state.DoS(100, error("ProcessOffchain() : This message should not be response"),REJECT_INVALID, "bad-nonce");                
        }
    }
        
    if( verify_flags & MC_VRA_IS_RESPONSE ) 
    {
        if( msg_id_to_respond.m_TimeStamp == 0 )
        {
            return state.DoS(100, error("ProcessOffchain() : This message should be response"),REJECT_INVALID, "bad-nonce");                
        }
        
        if((err=GetRelayRecord(pfrom,msg_id_to_respond,&msg_type_stored,&pto_stored)))
        {
            LogPrintf("ProcessOffchain() : Orphan response: %s, request: %s, from: %d, to: %d, msg: %s, hops: %d, size: %d, error: %d\n",
            msg_id_received.ToString().c_str(),msg_id_to_respond.ToString().c_str(),pfrom->GetId(),pto_stored ? pto_stored->GetId() : 0,mc_MsgTypeStr(msg_type_in).c_str(),hop_count,(int)vPayloadIn.size(),err);
            return false;
        }
    }
    
    SetRelayRecord(NULL,pfrom,MC_RMT_ERROR_IN_MESSAGE,msg_id_received);
    
    if(msg_id_to_respond.m_TimeStamp)
    {
        if(pto_stored)
        {
            msg_type_response_ptr=NULL;        
        }
        else
        {
            msg_type_relay_ptr=NULL;
        }
    }
    
    map<uint32_t, mc_Limiter>::iterator itlim_all = m_Limiters.find(MC_RMT_NONE);
    map<uint32_t, mc_Limiter>::iterator itlim_msg = m_Limiters.find(msg_type_in);
    
    if(itlim_all != m_Limiters.end())
    {
        itlim_all->second.SetEvent(1,vRecv.size());
        if( verify_flags & MC_VRA_LIMIT_ALL ) 
        {
            if(itlim_all->second.Disallowed(m_LastTime))
            {
                return false;
            }
        }        
    }
    
    if(itlim_msg != m_Limiters.end())
    {
        itlim_msg->second.SetEvent(1,vRecv.size());
        if( verify_flags & MC_VRA_LIMIT_MSG_TYPE ) 
        {
            if(itlim_msg->second.Disallowed(m_LastTime))
            {
                return false;
            }
        }        
    }
    
    vRecv >> flags_in;
    vRecv >> vPayloadIn;
    vRecv >> vSigScripts;
            
//    printf("recv: %d, from: %d, to: %d, hc: %d, size: %d, ts: %u, nc: %u\n",msg_type_in,pfrom->GetId(),pto_stored ? pto_stored->GetId() : 0,hop_count,(int)vPayloadIn.size(),timestamp_received,nonce_received);
    if(fDebug)LogPrint("offchain","Offchain recv: %s, request: %s, from: %d, to: %d, msg: %s, hops: %d, size: %d\n",
            msg_id_received.ToString().c_str(),msg_id_to_respond.ToString().c_str(),pfrom->GetId(),pto_stored ? pto_stored->GetId() : 0,mc_MsgTypeStr(msg_type_in).c_str(),hop_count,(int)vPayloadIn.size());
    
    if( verify_flags & MC_VRA_SIGNATURES )
    {
        CHashWriter ssHash(SER_GETHASH, 0);
        ssHash << msg_type_in;        
        ssHash << msg_id_received.m_TimeStamp;
        ssHash << msg_id_received.m_Nonce;
        ssHash << msg_id_to_respond.m_TimeStamp;
        ssHash << msg_id_to_respond.m_Nonce;
        ssHash << flags_in;
        ssHash << vPayloadIn;
        
        message_hash=ssHash.GetHash();                
    
        if( verify_flags & MC_VRA_SIGNATURE_ORIGIN )
        {
            if(vSigScripts.size() < 1)
            {
                return state.DoS(100, error("ProcessOffchain() : Missing sigScript"),REJECT_INVALID, "bad-sigscript");                            
            }
            if(vSigScripts[0].size())
            {
                CScript scriptSig=vSigScripts[0];

                opcodetype opcode;

                CScript::const_iterator pc = scriptSig.begin();

                if (!scriptSig.GetOp(pc, opcode, vchSigOut))
                {
                    return state.DoS(100, error("ProcessOffchain() : Cannot extract signature from sigScript"),REJECT_INVALID, "bad-sigscript-signature");                
                }

                vchSigOut.resize(vchSigOut.size()-1);
                if (!scriptSig.GetOp(pc, opcode, vchPubKey))
                {
                    return state.DoS(100, error("ProcessOffchain() : Cannot extract pubkey from sigScript"),REJECT_INVALID, "bad-sigscript-pubkey");                
                }

                CPubKey pubKeyOut(vchPubKey);
                if (!pubKeyOut.IsValid())
                {
                    return state.DoS(100, error("ProcessOffchain() : Invalid pubkey"),REJECT_INVALID, "bad-sigscript-pubkey");                
                }        

                pubkey=pubKeyOut;

                CHashWriter ss(SER_GETHASH, 0);
                
                ss << vector<unsigned char>((unsigned char*)&message_hash, (unsigned char*)&message_hash+32);
                ss << vector<unsigned char>((unsigned char*)&msg_id_to_respond, (unsigned char*)&msg_id_to_respond+sizeof(msg_id_to_respond));
                uint256 signed_hash=ss.GetHash();

                if(!pubkey.Verify(signed_hash,vchSigOut))
                {
                    return state.DoS(100, error("ProcessOffchain() : Wrong signature"),REJECT_INVALID, "bad-signature");                            
                }        
            }
            else
            {
                return state.DoS(100, error("ProcessOffchain() : Empty sigScript"),REJECT_INVALID, "bad-sigscript");                
            }
        }
    }
    
    if(itlim_all != m_Limiters.end())
    {
        itlim_all->second.Increment();
    }
    
    if(itlim_msg != m_Limiters.end())
    {
        itlim_msg->second.Increment();        
    }    

    if(pto_stored)
    {
        PushRelay(pto_stored,msg_format,vHops,vSendPaths,msg_type_in,msg_id_received,msg_id_to_respond,flags_in,
                  vPayloadIn,vSigScripts,pfrom,MC_PRA_NONE);
    }
    else
    {
        if( (msg_type_relay_ptr != NULL) || (msg_type_response_ptr != NULL) )
        {
            if(MultichainRelayResponse(msg_type_stored,pto_stored,
                                       msg_type_in,flags_in,vPayloadIn,vSigScripts,vSigScriptsToVerify,
                                       msg_type_response_ptr,&flags_response,vPayloadResponse,vSigScriptsRespond,
                                       msg_type_relay_ptr,&flags_relay,vPayloadRelay,vSigScriptsRelay,strError))
            {
                int dos_score=0;
                if(!pEF->OFF_VerifySignatureScripts(msg_type_in,msg_id_received,msg_id_to_respond,flags_in,vPayloadIn,vSigScriptsToVerify,strError,dos_score))
                {
                    LogPrintf("ProcessOffchain() : Error processing %s (request %s) from peer %d: %s\n",mc_MsgTypeStr(msg_type_in).c_str(),
                            msg_id_received.ToString().c_str(),pfrom->GetId(),strError.c_str());     
                    if(dos_score)
                    {
                        return state.DoS(dos_score, error("ProcessOffchain() : Invalid sigScript"),REJECT_INVALID, "bad-sigscript");       
                    }
                    return false;
                }
                if(msg_type_response_ptr && *msg_type_response_ptr)
                {
                    if(*msg_type_response_ptr != MC_RMT_ADD_RESPONSE)
                    {
                        PushRelay(pfrom,0,vEmptyHops,vSendPaths,*msg_type_response_ptr,GenerateMsgID(m_LastTime),msg_id_received,flags_response,
                                  vPayloadResponse,vSigScriptsRespond,NULL,MC_PRA_NONE);                    
                    }
                    else
                    {
                        map<mc_OffchainMessageID, mc_RelayRequest>::iterator itreq = m_Requests.find(msg_id_to_respond);
                        if(itreq != m_Requests.end())
                        {
                            Lock();
                            AddResponse(itreq->second.m_MsgID,pfrom,vHops.size() ? vHops[0] : 0,hop_count,msg_id_received,msg_type_in,flags_in,vPayloadIn,MC_RST_SUCCESS);
                            UnLock();
                        }                        
                        else
                        {
                            if(fDebug)LogPrint("offchain","ProcessOffchain() : Deleted request: %s, request: %s, from: %d, to: %d, msg: %s, hops: %d, size: %d\n",
                            msg_id_received.ToString().c_str(),msg_id_to_respond.ToString().c_str(),pfrom->GetId(),pto_stored ? pto_stored->GetId() : 0,mc_MsgTypeStr(msg_type_in).c_str(),hop_count,(int)vPayloadIn.size());
                            return false;                            
                        }
                    }
                }
                if(msg_type_relay_ptr && *msg_type_relay_ptr)
                {
                    LOCK(cs_vNodes);
                    BOOST_FOREACH(CNode* pnode, vNodes)
                    {
                        if(pnode != pfrom)
                        {
                            vHopsToRelay=vHops;
                            PushRelay(pnode,msg_format,vHopsToRelay,vSendPaths,*msg_type_relay_ptr,msg_id_received,msg_id_to_respond,flags_relay,
                                      vPayloadRelay,vSigScriptsRelay,pfrom,MC_PRA_NONE);
                        }
                    }
                }                
            }
            else
            {
                LogPrintf("ProcessOffchain() : Error processing %s (request %s) from peer %d: %s\n",mc_MsgTypeStr(msg_type_in).c_str(),
                        msg_id_received.ToString().c_str(),pfrom->GetId(),strError.c_str());     
                return false;                
            }
        }        
    }
    
    if(msg_id_to_respond.m_TimeStamp)
    {
        SetRelayRecord(NULL,pfrom,msg_type_in,msg_id_received);        
    }
    else
    {
        SetRelayRecord(NULL,pfrom,MC_RMT_NEW_REQUEST,msg_id_received);
    }
    
    return true;
}

void mc_RelayResponse::Zero()
{
    m_MsgID=0;
    m_MsgType=MC_RMT_NONE;
    m_Flags=0;
    m_NodeFrom=0;
    m_HopCount=0;
    m_TryCount=0;
    m_Source=0;
    m_LastTryTimestamp=0;
    m_Status=MC_RST_NONE;
    m_Payload.clear();
    m_Requests.clear();    
    m_TimeDiff=0;
}

void mc_RelayRequest::Zero()
{
    m_MsgID=0;
    m_MsgType=MC_RMT_NONE;
    m_Flags=0;
    m_NodeTo=0;
    m_LastTryTimestamp=0;
    m_TryCount=0;
    m_Status=MC_RST_NONE;
    m_DestinationID=0;
    m_EFCacheID=-1;
    m_Payload.clear();   
    m_Responses.clear();
}

int mc_RelayManager::AddRequest(CNode *pto,int64_t destination,mc_OffchainMessageID msg_id,uint32_t msg_type,uint32_t flags,vector <unsigned char>& payload,uint32_t status,int ef_cache_id)
{    
    int err=MC_ERR_NOERROR;
 
    map<mc_OffchainMessageID, mc_RelayRequest>::iterator itreq_this = m_Requests.find(msg_id);
    if(itreq_this == m_Requests.end())
    {    
        mc_RelayRequest request;

        request.m_MsgID=msg_id;
        request.m_MsgType=msg_type;
        request.m_Flags=flags;
        request.m_NodeTo=pto ? pto->GetId() : 0;
        request.m_LastTryTimestamp=0;
        request.m_TryCount=0;
        request.m_Status=status;
        request.m_DestinationID=destination;
        request.m_Payload=payload;   
        request.m_EFCacheID=ef_cache_id;
        request.m_Responses.clear();

        if(fDebug)LogPrint("offchain","Offchain rqst: %s, to: %d, msg: %s, size: %d\n",msg_id.ToString().c_str(),pto ? pto->GetId() : 0,mc_MsgTypeStr(msg_type).c_str(),(int)payload.size());
        m_Requests.insert(make_pair(msg_id,request));
    }
    else
    {
        err=MC_ERR_FOUND;
    }    
    
    return err;            
}

int64_t mc_RelayResponse::SourceID()
{
    int64_t result=(int64_t)m_NodeFrom;
    result = (result << 32) + (int64_t)m_Source;
    return result;
}

int mc_RelayManager::AddResponse(mc_OffchainMessageID request,CNode *pfrom,int32_t source,int hop_count,mc_OffchainMessageID msg_id,uint32_t msg_type,uint32_t flags,vector <unsigned char>& payload,uint32_t status)
{
//    printf("resp: %lu, mt: %d, node: %d, size: %d, rn: %lu\n",request,msg_type,pfrom ? pfrom->GetId() : 0,(int)payload.size(),nonce);
    if(fDebug)LogPrint("offchain","Offchain resp: %s, request: %s, from: %d, msg: %s, size: %d\n",
            msg_id.ToString().c_str(),request.ToString().c_str(),pfrom ? pfrom->GetId() : 0,mc_MsgTypeStr(msg_type).c_str(),(int)payload.size());
    if(request.IsZero())
    {
        return MC_ERR_NOERROR;
    }
    
    mc_RelayResponse response;
    response.m_MsgID=msg_id;    
    response.m_MsgType=msg_type;
    response.m_TimeDiff=msg_id.m_TimeStamp-m_LastTime;
    response.m_Flags=flags;
    response.m_NodeFrom=pfrom ? pfrom->GetId() : 0;
    response.m_HopCount=hop_count;
    response.m_Source=source;
    response.m_LastTryTimestamp=0;
    response.m_Status=status;
    response.m_Payload=payload;
    response.m_Requests.clear();    
    
    if(!request.IsZero())
    {
        map<mc_OffchainMessageID, mc_RelayRequest>::iterator itreq = m_Requests.find(request);
        if(itreq != m_Requests.end())
        {    
            itreq->second.m_Responses.push_back(response);
            if(status & MC_RST_SUCCESS)
            {
                itreq->second.m_Status |= MC_RST_SUCCESS;
            }
        }
        else
        {
            return MC_ERR_NOT_FOUND;
        }
    }
    
    return MC_ERR_NOERROR; 
}

int mc_RelayManager::DeleteRequest(mc_OffchainMessageID request)
{
    int err=MC_ERR_NOERROR;

    Lock();
    map<mc_OffchainMessageID, mc_RelayRequest>::iterator itreq = m_Requests.find(request);
    if(itreq != m_Requests.end())
    {
        pEF->OFF_FreeEFCache(itreq->second.m_EFCacheID);
        if(fDebug)LogPrint("offchain","Offchain delete: %s, msg: %s, size: %d. Open requests: %d\n",itreq->second.m_MsgID.ToString().c_str(),
            mc_MsgTypeStr(itreq->second.m_MsgType).c_str(),(int)itreq->second.m_Payload.size(),(int)m_Requests.size());
        m_Requests.erase(itreq);       
    }    
    else
    {
        err= MC_ERR_NOT_FOUND;
    }
    
    UnLock();
    return err;     
}

mc_RelayRequest *mc_RelayManager::FindRequest(mc_OffchainMessageID request)
{
    Lock();
    map<mc_OffchainMessageID, mc_RelayRequest>::iterator itreq = m_Requests.find(request);
    if(itreq != m_Requests.end())
    {
        return &(itreq->second);
    }    
    
    UnLock();
    return NULL;    
}


mc_OffchainMessageID mc_RelayManager::SendRequest(CNode* pto,uint32_t msg_type,uint32_t flags,vector <unsigned char>& payload)
{
    mc_OffchainMessageID msg_id;
    vector <int32_t> vEmptyHops;
    vector <int32_t> vEmptySendPaths;
    vector<CScript> vSigScriptsEmpty;

    msg_id=GenerateMsgID();
    
    Lock();
    {
        LOCK(cs_vNodes);
        BOOST_FOREACH(CNode* pnode, vNodes)
        {
            if( (pto == NULL) || (pnode == pto) )
            {
                bool take_it=false;
                int max_kb_per_destination=pwalletTxsMain->m_ChunkCollector->m_MaxKBPerDestination;
                if(pEF->NET_IsFinalized(pnode))
                {
                    if( (pnode->nMaxKBPerDestination == 0) || (2 * pnode->nMaxKBPerDestination >= max_kb_per_destination) )
                    {
                        take_it=true;
                    }
                    else
                    {
                        if(mc_RandomInRange(1,max_kb_per_destination/pnode->nMaxKBPerDestination) == 1)
                        {
                            take_it=true;                            
                        }
                    }
                }
                if(take_it)
                {
                    PushRelay(pnode,0,vEmptyHops,vEmptySendPaths,msg_type,msg_id,mc_OffchainMessageID(),flags,payload,vSigScriptsEmpty,NULL,0);                    
                }
            }
        }
    }

    if(AddRequest(pto,0,msg_id,msg_type,flags,payload,MC_RST_NONE,-1) != MC_ERR_NOERROR)
    {
        UnLock();
        return mc_OffchainMessageID();
    }
    
    UnLock();
    return msg_id;
}

mc_OffchainMessageID mc_RelayManager::SendNextRequest(mc_RelayResponse* response,uint32_t msg_type,uint32_t flags,vector <unsigned char>& payload,set<CPubKey>& sAddresses,int ef_cache_id)
{
    mc_OffchainMessageID msg_id;
    vector <int32_t> vEmptyHops;
    vector <int32_t> vEmptySendPaths;
    vector<CScript> vSigScripts;
    string strError;
    
    msg_id=GenerateMsgID();

    
    Lock();                                                                     // Secondary lock, to make sure it is locked by this thread
    {
        LOCK(cs_vNodes);
        BOOST_FOREACH(CNode* pnode, vNodes)
        {
            if( pnode->GetId() == response->m_NodeFrom )
            {
                if(pEF->OFF_CreateSignatureScripts(msg_type,msg_id,response->m_MsgID,flags,payload,sAddresses,vSigScripts,strError))
                {
                    msg_id=PushRelay(pnode,0,vEmptyHops,vEmptySendPaths,msg_type,msg_id,response->m_MsgID,
                                         flags,payload,vSigScripts,NULL,MC_PRA_NONE);                    
                    if(AddRequest(pnode,response->SourceID(),msg_id,msg_type,flags,payload,MC_RST_NONE,ef_cache_id) != MC_ERR_NOERROR)
                    {                    
                        UnLock();
                        return mc_OffchainMessageID();
                    }
                }
                else
                {
                    if(fDebug)LogPrint("offchain","Cannot sign offchain request, error: %s\n",strError.c_str());
                    msg_id.m_TimeStamp=0;
                    msg_id.m_Nonce=0;
                }
            }
        }
    }
    
    UnLock();
    return msg_id;
}


void mc_RelayManager::InvalidateResponsesFromDisconnected()
{
    LOCK(cs_vNodes);
    int m=(int)vNodes.size();
    Lock();
    BOOST_FOREACH(PAIRTYPE(const mc_OffchainMessageID,mc_RelayRequest)& item, m_Requests)    
    {
        for(int i=0;i<(int)item.second.m_Responses.size();i++)
        {
            int n=0;
            while(n<m)
            {
                if(vNodes[n]->GetId() == item.second.m_Responses[i].m_NodeFrom)
                {
                    n=m+1;
                }
                n++;
            }
            if(n == m)
            {
                item.second.m_Responses[i].m_Status &= ~MC_RST_SUCCESS;
                item.second.m_Responses[i].m_Status |= MC_RST_DISCONNECTED;
            }
        }
    }    
    UnLock();
}

