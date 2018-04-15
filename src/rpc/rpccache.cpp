// Copyright (c) 2014-2017 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "rpc/rpcutils.h"
#include "protocol/relay.h"
#include "net/net.h"


Value createbinarycache(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)  
        throw runtime_error("Help message not found\n");
    
    unsigned char dest[8];
    string str;
    int fHan;
    
    while(true)
    {
        GetRandBytes((unsigned char*)dest, 8);
        str=EncodeBase58((unsigned char*)(&dest[0]),(unsigned char*)(&dest[0])+8);
        fHan=mc_BinaryCacheFile(str,0);
        if(fHan > 0)
        {
            close(fHan);
        }
        else
        {
            fHan=mc_BinaryCacheFile(str,1);
            if(fHan > 0)
            {
                close(fHan);
            }
            else
            {
                throw JSONRPCError(RPC_INTERNAL_ERROR, "Cannot store binary cache item"); 
            }
            return str;
        }
    }
    

    return Value::null;
}

Value appendbinarycache(const Array& params, bool fHelp)
{
    vector<unsigned char> vValue;
    int64_t size;
    
    if (fHelp || params.size() != 2)  
        throw runtime_error("Help message not found\n");
    
    if(params[0].type() != str_type)
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "cache identifier should be string");                                                                                                                
    }

    if(params[1].type() != str_type)
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "data should be hexadecimal string");                                                                                                                
    }
    
    bool fIsHex;
    vValue=ParseHex(params[1].get_str().c_str(),fIsHex);    
    if(!fIsHex)
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "data should be hexadecimal string");                                                                                                                
    }        
    
    int fHan=mc_BinaryCacheFile(params[0].get_str(),2);
    if(fHan <= 0)
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Binary cache item with this identifier not found");                                                                                                                        
    }
    
    size=lseek64(fHan,0,SEEK_END);
    
    if(vValue.size())
    {
        if(write(fHan,&vValue[0],vValue.size()) != (int)vValue.size())
        {
            close(fHan);
            throw JSONRPCError(RPC_INTERNAL_ERROR, "Cannot store binary cache item");                                                                                                                                    
        }
    }
    
    close(fHan);
    
    size+=vValue.size();
    
    return size;
}

Value deletebinarycache(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)  
        throw runtime_error("Help message not found\n");
    
    if(params[0].type() != str_type)
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "cache identifier should be string");                                                                                                                
    }
    
    int fHan=mc_BinaryCacheFile(params[0].get_str(),0);
    if(fHan <= 0)
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Binary cache item with this identifier not found");                                                                                                                        
    }
    
    mc_RemoveBinaryCacheFile(params[0].get_str());
    
    return Value::null;    
}


Value offchain(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 4)  
        throw runtime_error("Help message not found\n");
    
    uint32_t request_type=MC_RMT_NONE;
    int timeout=5;
    vector <unsigned char> payload;
    int64_t request_id;      
    CNode *pto=NULL;
    string request_str="";
    mc_RelayRequest *request;
    mc_RelayResponse *response;
    Object res;
    bool res_found=false;
            
    if(params[0].type() == str_type)
    {
        if(params[0].get_str() == "findaddress")
        {            
            request_type=MC_RMT_MC_ADDRESS_QUERY;
            request_str="query for address ";
        }
    }
    
    if(request_type == MC_RMT_NONE)
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid request type");                    
    }
    
    if(params[1].type() != obj_type)
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid request datails, should be object");                    
    }
    
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
        
        request_id=pRelayManager->SendRequest(pto,request_type,0,payload);
        request_str+=strprintf(". Request ID: %lu",request_id);
        printf("%s\n",request_str.c_str());        
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
                                                for(int a;a<count;a++)
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
                                    for(int a;a<(int)node_addr.m_NetAddresses.size();a++)
                                    {
                                        addresses.push_back(node_addr.m_NetAddresses[a].ToStringIPPort());                                        
                                    }                                    
                                    res.push_back(Pair("addresses",addresses));
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
