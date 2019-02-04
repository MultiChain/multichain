// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "rpc/rpcutils.h"


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
    
    close(fHan);
    
    mc_RemoveBinaryCacheFile(params[0].get_str());
    
    return Value::null;    
}


