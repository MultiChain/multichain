// Copyright (c) 2014-2017 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "core/init.h"
#include "rpc/rpcutils.h"
#include "protocol/relay.h"
#include "wallet/wallettxs.h"
#include "net/net.h"
void parseStreamIdentifier(Value stream_identifier,mc_EntityDetails *entity);



Value debug(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 4)  
        throw runtime_error("Help message not found\n");
    
    uint32_t request_type=MC_RMT_NONE;
    int timeout=5;
    vector <unsigned char> payload;
    mc_OffchainMessageID request_id;      
    CNode *pto=NULL;
    string request_str="";
    mc_RelayRequest *request;
    mc_RelayResponse *response;
    int attempts,delay; 
    Object res;
    mc_ChunkCollector collector;
    bool res_found=false;
            
    if(params[0].type() == str_type)
    {
        if(params[0].get_str() == "findaddress")
        {            
            request_type=MC_RMT_MC_ADDRESS_QUERY;
            request_str="query for address ";
        }
        if(params[0].get_str() == "getchunks")
        {            
            request_type=MC_RMT_SPECIAL_COLLECT_CHUNKS;
            request_str="query for address ";
        }        
        if(params[0].get_str() == "viewchunks")
        {            
            request_type=MC_RMT_SPECIAL_VIEW_CHUNKS;
        }        
    }
    
    if(request_type == MC_RMT_NONE)
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid request type");                    
    }
    
    if(params[1].type() != obj_type)
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid request details, should be object");                    
    }
    
    if(request_type & MC_RMT_SPECIAL_MASK)
    {
        switch(request_type)
        {
            case MC_RMT_SPECIAL_COLLECT_CHUNKS:
                collector.Initialize(NULL,NULL,0);
                attempts=10;
                delay=1000;
                BOOST_FOREACH(const Pair& d, params[1].get_obj()) 
                {
                    if(d.name_ == "attempts")
                    {
                        if(d.value_.type() == int_type)
                        {
                            attempts=d.value_.get_int();
                        }
                    }
                    if(d.name_ == "delay")
                    {
                        if(d.value_.type() == int_type)
                        {
                            delay=d.value_.get_int();
                        }
                    }
                    if(d.name_ == "chunks")
                    {
                        if(d.value_.type() == array_type)
                        {
                            for(int c=0;c<(int)d.value_.get_array().size();c++)
                            {
                                Value cd=d.value_.get_array()[c];
                                if(cd.type() != obj_type)
                                {
                                    throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid chunk");                                
                                }
                                uint256 chunk_hash=0;
                                int chunk_size=0;
                                mc_EntityDetails stream_entity;
                                
                                uint256 txid=0;
                                int vout=0;
                                mc_TxEntity entity;
                                entity.Zero();
                                BOOST_FOREACH(const Pair& dd, cd.get_obj()) 
                                {
                                    if(dd.name_ == "stream")
                                    {
                                        parseStreamIdentifier(dd.value_.get_str(),&stream_entity);           
                                        memcpy(&entity,stream_entity.GetTxID()+MC_AST_SHORT_TXID_OFFSET,MC_AST_SHORT_TXID_SIZE);
                                        entity.m_EntityType=MC_TET_STREAM;
                                    }
                                    if(dd.name_ == "hash")
                                    {
                                        chunk_hash = ParseHashV(dd.value_.get_str(), "hash");
                                    }
                                    if(dd.name_ == "txid")
                                    {
                                        txid = ParseHashV(dd.value_.get_str(), "hash");
                                    }
                                    if(dd.name_ == "vout")
                                    {
                                        if(dd.value_.type() != int_type)
                                        {
                                            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid vout");                                                                        
                                        }
                                        vout=dd.value_.get_int();
                                    }
                                    if(dd.name_ == "size")
                                    {
                                        if(dd.value_.type() != int_type)
                                        {
                                            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid size");                                                                        
                                        }
                                        chunk_size=dd.value_.get_int();
                                    }
                                }
                                if(chunk_hash == 0)
                                {
                                    throw JSONRPCError(RPC_INVALID_PARAMETER, "Missing hash");
                                }
                                if(chunk_size == 0)
                                {
                                    throw JSONRPCError(RPC_INVALID_PARAMETER, "Missing size");
                                }
                                if(entity.m_EntityType == MC_TET_NONE)
                                {
                                    throw JSONRPCError(RPC_INVALID_PARAMETER, "Missing stream");                                    
                                }
                                collector.InsertChunk((unsigned char*)&chunk_hash,&entity,(unsigned char*)&txid,vout,chunk_size);
                            }
                        }
                    }            
                }    

                if(collector.m_MemPool->GetCount() == 0)
                {
                    throw JSONRPCError(RPC_INVALID_PARAMETER, "Missing chunks");                
                }

                break;
            case MC_RMT_SPECIAL_VIEW_CHUNKS:
                Array arr_res;
                BOOST_FOREACH(const Pair& d, params[1].get_obj()) 
                {
                    if(d.name_ == "chunks")
                    {
                        if(d.value_.type() == array_type)
                        {
                            for(int c=0;c<(int)d.value_.get_array().size();c++)
                            {
                                Value cd=d.value_.get_array()[c];
                                if(cd.type() != obj_type)
                                {
                                    throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid chunk");                                
                                }
                                uint256 chunk_hash=0;                                
                                BOOST_FOREACH(const Pair& dd, cd.get_obj()) 
                                {
                                    if(dd.name_ == "hash")
                                    {
                                        chunk_hash = ParseHashV(dd.value_.get_str(), "hash");
                                    }
                                }
                                if(chunk_hash == 0)
                                {
                                    throw JSONRPCError(RPC_INVALID_PARAMETER, "Missing hash");
                                }
                                
                                unsigned char *chunk_found;
                                size_t chunk_bytes;
                                mc_ChunkDBRow chunk_def;
                                
                                Object chunk_obj;
                                chunk_obj.push_back(Pair("hash",chunk_hash.ToString()));
                                
                                if(pwalletTxsMain->m_ChunkDB->GetChunkDef(&chunk_def,(unsigned char *)&chunk_hash,NULL,NULL,-1) == MC_ERR_NOERROR)
                                {
                                    chunk_found=pwalletTxsMain->m_ChunkDB->GetChunk(&chunk_def,0,-1,&chunk_bytes);
                                    if(chunk_found)
                                    {
                                        chunk_obj.push_back(Pair("size",chunk_bytes));
                                        chunk_obj.push_back(Pair("data",HexStr(chunk_found,chunk_found+chunk_bytes)));
                                    }
                                    else
                                    {
                                        chunk_obj.push_back(Pair("error","Internal error"));
                                    }
                                }
                                else
                                {
                                    chunk_obj.push_back(Pair("error","Chunk not found"));
                                }
                                arr_res.push_back(chunk_obj);
                            }
                        }
                    }                    
                }     
                return arr_res;
        }        
    }
    else
    {
        switch(request_type)
        {
            case MC_RMT_MC_ADDRESS_QUERY:
                string addr_to_find="";
                BOOST_FOREACH(const Pair& d, params[1].get_obj()) 
                {
                    if(d.name_ == "address")
                    {
                        if(d.value_.type() ==str_type)
                        {
                            addr_to_find=d.value_.get_str();
                            request_str+=addr_to_find;
                        }
                    }            
                }    

                CBitcoinAddress address(addr_to_find);
                CKeyID keyID;
                if (!address.GetKeyID(keyID))
                    throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");

                payload.resize(1+sizeof(CKeyID));
                payload[0]=MC_RDT_MC_ADDRESS;
                memcpy(&payload[1],&keyID,sizeof(CKeyID));            
                break;
        }
    }
    
    if(params.size() > 2)
    {
        if(params[2].type() != int_type)
        {
            timeout=params[2].get_int();
            if( (timeout <= 0) || (timeout > 15 ) )
            {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid timeout");                                
            }
        }    
        else
        {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid timeout");                                            
        }
    }
    
    if(request_type & MC_RMT_SPECIAL_MASK)
    {
        int remaining=0;
        for(int a=0;a<attempts;a++)
        {
            if(a)
            {
                __US_Sleep(delay);
            }
            remaining=MultichainCollectChunks(&collector);
            if(remaining == 0)
            {
                return 0;
            }
        }
        return remaining;
    }
    else        
    {
        {
            LOCK(cs_vNodes);
            if(params.size() > 3)
            {
                if(params[3].type() != str_type)
                {
                    throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid destination");                                            
                }

                BOOST_FOREACH(CNode* pnode, vNodes) 
                {
                    CNodeStats stats;
                    pnode->copyStats(stats);
                    if( (params[3].get_str() == stats.addrName) || 
                        (params[3].get_str() == CBitcoinAddress(stats.kAddrRemote).ToString()) )
                    {
                        pto=pnode;
                    }
                }
            }

            if(pto)
            {
                request_str="Sending " + request_str + strprintf(" to node %d",pto->GetId());
            }
            else
            {
                request_str="Broadcasting " + request_str;
            }
        }
        
        request_id=pRelayManager->SendRequest(pto,request_type,0,payload);
        printf("%s",request_str.c_str());        
        printf(". Request ID: %s\n",request_id.ToString().c_str());
    }    

    uint32_t time_now;
    uint32_t time_stop;
    
    time_now=mc_TimeNowAsUInt();
    time_stop=time_now+timeout;
    res_found=false;
    
    while(time_now<time_stop)
    {
        request=pRelayManager->FindRequest(request_id);
        if(request)
        {
            if(request->m_Responses.size())
            {
                switch(request_type)
                {
                    case MC_RMT_MC_ADDRESS_QUERY:
                        mc_NodeFullAddress node_addr;
                        int count,shift;
                        bool take_it=true;
                        unsigned char *ptr;
                        unsigned char *ptrEnd;
                        for(int r=0;r<(int)request->m_Responses.size();r++)
                        {
                            if(!res_found)
                            {
                                response=&(request->m_Responses[r]);
                                ptr=&(response->m_Payload[0]);
                                ptrEnd=ptr+response->m_Payload.size();
                                while( (ptr<ptrEnd) && take_it )
                                {
                                    switch(*ptr)
                                    {
                                        case MC_RDT_MC_ADDRESS:
                                            ptr++;
                                            if((int)sizeof(CKeyID) > (ptrEnd-ptr))
                                            {
                                                take_it=false;
                                            }
                                            else
                                            {
                                                node_addr.m_Address=*(CKeyID*)ptr;
                                                ptr+=sizeof(CKeyID);
                                            }
                                            break;
                                        case MC_RDT_NET_ADDRESS:
                                            ptr++;
                                            count=(int)mc_GetVarInt(ptr,ptrEnd-ptr,-1,&shift);
                                            ptr+=shift;
                                            if(count*(int)sizeof(CAddress) > (ptrEnd-ptr))
                                            {
                                                take_it=false;
                                            }
                                            else
                                            {
                                                for(int a=0;a<count;a++)
                                                {
                                                    node_addr.m_NetAddresses.push_back(*(CAddress*)ptr);
                                                    ptr+=sizeof(CAddress);
                                                }
                                            }
                                            break;
                                        default:
                                            take_it=false;
                                            break;
                                    }
                                }
                                
                                if(take_it)
                                {
                                    Array addresses;
                                    res.push_back(Pair("handshake",CBitcoinAddress(node_addr.m_Address).ToString()));
                                    for(int a=0;a<(int)node_addr.m_NetAddresses.size();a++)
                                    {
                                        addresses.push_back(node_addr.m_NetAddresses[a].ToStringIPPort());                                        
                                    }                                    
                                    res.push_back(Pair("addresses",addresses));
                                    res.push_back(Pair("neighbour",response->m_Source ? false : true));
                                    res_found=true;
                                }
                            }
                        }                        
                        
                        break;
                }
            }
            pRelayManager->UnLock();
            if(res_found)
            {
                pRelayManager->DeleteRequest(request_id);
                return res; 
            }
        }
        __US_Sleep(100);
        time_now=mc_TimeNowAsUInt();
    }
    
    return Value::null;    
}
