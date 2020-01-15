// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "core/init.h"
#include "rpc/rpcwallet.h"
#include "protocol/relay.h"
#include "wallet/wallettxs.h"
#include "net/net.h"

void parseStreamIdentifier(Value stream_identifier,mc_EntityDetails *entity);

string mcd_ParamStringValue(const Object& params,string name,string default_value)
{
    string value=default_value;
    BOOST_FOREACH(const Pair& d, params) 
    {
        if(d.name_ == name)
        {
            if(d.value_.type() == str_type)
            {
                value=d.value_.get_str();
            }
        }
    }
    return value;
}

int mcd_ParamIntValue(const Object& params,string name,int default_value)
{
    int value=default_value;
    BOOST_FOREACH(const Pair& d, params) 
    {
        if(d.name_ == name)
        {
            if(d.value_.type() == int_type)
            {
                value=d.value_.get_int();
            }
        }
    }
    return value;
}

int64_t mcd_OpenDatabase(const char *name,const char *dbname,int key_size,int value_size,mc_Database **lpDB)
{
    int err,value_len;   
    int64_t size;
    mc_Database *m_DB;
    char m_DBName[MC_DCT_DB_MAX_PATH];     
    char m_DirName[MC_DCT_DB_MAX_PATH];     
    char *kbuf;
    char *vbuf;
    string rel_dbname=strprintf("debug/%s",dbname);
    unsigned char *ptr;
    
    m_DB=new mc_Database;
    
    mc_GetFullFileName(name,"debug","",MC_FOM_RELATIVE_TO_DATADIR | MC_FOM_CREATE_DIR,m_DirName);
    mc_CreateDir(m_DirName);
    mc_GetFullFileName(name,rel_dbname.c_str(),".db",MC_FOM_RELATIVE_TO_DATADIR | MC_FOM_CREATE_DIR,m_DBName);
    
    m_DB->SetOption("KeySize",0,key_size);
    m_DB->SetOption("ValueSize",0,value_size);
    
    
    err=m_DB->Open(m_DBName,MC_OPT_DB_DATABASE_CREATE_IF_MISSING | MC_OPT_DB_DATABASE_TRANSACTIONAL | MC_OPT_DB_DATABASE_LEVELDB);
    if(err)
    {
        return -1;
    }

    kbuf=new char[key_size];
    vbuf=new char[value_size];
    
    memset(kbuf,0,key_size);
    memset(vbuf,0,value_size);
    
    ptr=(unsigned char*)m_DB->Read(kbuf,key_size,&value_len,0,&err);
    if(err)
    {
        return -2;
    }

    size=0;
    if(ptr)                                                                     
    {        
        size=mc_GetLE(ptr,8);
    }
    else
    {
        err=m_DB->Write(kbuf,key_size,vbuf,value_size,MC_OPT_DB_DATABASE_TRANSACTIONAL);
        if(err)
        {
            return -3;
        }        
                
        err=m_DB->Commit(MC_OPT_DB_DATABASE_TRANSACTIONAL);
        if(err)
        {
            return -4;
        }                
    }
    
    delete [] kbuf;
    delete [] vbuf;
    
    *lpDB=m_DB;

    return size;
}

void mcd_CloseDatabase(mc_Database *m_DB)
{
    if(m_DB)
    {
        m_DB->Close();
        delete m_DB;    
    }    
}

int64_t mcd_AddRows(mc_Database *m_DB,int key_size,int value_size,int row_count,int64_t prev_rows)
{
    int err,value_len;
    int per_commit_count=1000;
    int commit_count=row_count/per_commit_count;
    int64_t total_rows=prev_rows;
    char *kbuf;
    char *vbuf;
    kbuf=new char[key_size];
    vbuf=new char[value_size];

    double tb,ta;
    tb=mc_TimeNowAsDouble();
    for(int c=0;c<commit_count;c++)
    {
        for(int r=0;r<per_commit_count;r++)
        {    
            GetRandBytes((unsigned char*)kbuf, key_size);
            GetRandBytes((unsigned char*)vbuf, value_size);
            m_DB->Read(kbuf,key_size,&value_len,0,&err);
            err=m_DB->Write(kbuf,key_size,vbuf,value_size,MC_OPT_DB_DATABASE_TRANSACTIONAL);
            if(err)
            {
                return total_rows;
            }        
        }   
        memset(kbuf,0,key_size);
        memset(vbuf,0,value_size);
        total_rows+=per_commit_count;
        mc_PutLE(vbuf,&total_rows,8);
        err=m_DB->Write(kbuf,key_size,vbuf,value_size,MC_OPT_DB_DATABASE_TRANSACTIONAL);
        if(err)
        {
            return total_rows;
        }        
        err=m_DB->Commit(MC_OPT_DB_DATABASE_TRANSACTIONAL);
        if(err)
        {
            return total_rows;
        }                        
    }
    ta=mc_TimeNowAsDouble();    
    printf("%8.3f\n",ta-tb);
    
    delete [] kbuf;
    delete [] vbuf;
    
    return total_rows; 
}

double mcd_ReadRows(mc_Database *m_DB,int key_size,int row_count,int read_type)
{
    int err,value_len;
    int options;
    char *kbuf;
    char *vbuf;    
    kbuf=new char[key_size];
    int64_t sum=0;
    
    options=0;
    if(read_type == 1 )
    {
        options=MC_OPT_DB_DATABASE_SEEK_ON_READ;
    }
    if(read_type == 2 )
    {
        options=MC_OPT_DB_DATABASE_NEXT_ON_READ;
    }
    double tb,ta;
    tb=mc_TimeNowAsDouble();
    for(int r=0;r<row_count;r++)
    {    
        GetRandBytes((unsigned char*)kbuf, key_size);
        vbuf=(char*)m_DB->Read(kbuf,key_size,&value_len,options,&err);
        if(err)
        {
            return 0;
        }
        if(vbuf)
        {
            if(read_type == 2)
            {
                sum+=mc_GetLE(vbuf,4);
            }
            else
            {
                return 0;
            }
        }
        else
        {
            if(read_type == 2)
            {
                return 0;
            }            
        }
    }   
    ta=mc_TimeNowAsDouble();
    
    delete [] kbuf;
    printf("%8.3f   %ld\n",ta-tb,sum);
    return ta-tb; 
}

Value mcd_DebugIssueLicenseToken(const Object& params)
{
    string name=mcd_ParamStringValue(params,"name","");
    int multiple=mcd_ParamIntValue(params,"multiple",1);
    int64_t quantity=mcd_ParamIntValue(params,"quantity",1);
    
    CBitcoinAddress from_address(mcd_ParamStringValue(params,"from_address",""));
    if (!from_address.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");
    CBitcoinAddress to_address(mcd_ParamStringValue(params,"to_address",""));
    if (!to_address.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");
    CBitcoinAddress req_address(mcd_ParamStringValue(params,"req_address",""));
    if (!req_address.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");
    
    mc_Script *lpScript=mc_gState->m_TmpBuffers->m_RpcScript3;
    lpScript->Clear();

    unsigned char hash[32];
    memset(hash,0xbc,32);
    
    lpScript->SetAssetGenesis(quantity);
    lpScript->AddElement();
    lpScript->SetData(hash,30);
    lpScript->AddElement();
    
    mc_Script *lpDetailsScript=mc_gState->m_TmpBuffers->m_RpcScript1;   
    lpDetailsScript->Clear();
    mc_Script *lpDetails=mc_gState->m_TmpBuffers->m_RpcScript2;
    lpDetails->Clear();
    
    lpDetails->Clear();
    lpDetails->AddElement();
    
    CKeyID KeyID;
    req_address.GetKeyID(KeyID);
    int64_t dummy_int64=0;
    int version=20000202;
    int protocol=20007;
    unsigned int timestamp=mc_TimeNowAsUInt();
    
    lpDetails->SetSpecialParamValue(MC_ENT_SPRM_LICENSE_LICENSE_HASH,hash,32);
    lpDetails->SetSpecialParamValue(MC_ENT_SPRM_LICENSE_ISSUE_ADDRESS,(unsigned char*)&KeyID,20);
    lpDetails->SetSpecialParamValue(MC_ENT_SPRM_NAME,(const unsigned char*)(name.c_str()),name.size());//+1);
    lpDetails->SetSpecialParamValue(MC_ENT_SPRM_ASSET_MULTIPLE,(unsigned char*)&multiple,4);    
    lpDetails->SetSpecialParamValue(MC_ENT_SPRM_LICENSE_CONFIRMATION_TIME,(unsigned char*)&dummy_int64,4);
    lpDetails->SetSpecialParamValue(MC_ENT_SPRM_LICENSE_CONFIRMATION_REF,(unsigned char*)&dummy_int64,2);
    lpDetails->SetSpecialParamValue(MC_ENT_SPRM_LICENSE_PUBKEY,(unsigned char*)&dummy_int64,2);
    lpDetails->SetSpecialParamValue(MC_ENT_SPRM_LICENSE_MIN_NODE,(unsigned char*)&version,4);
    lpDetails->SetSpecialParamValue(MC_ENT_SPRM_LICENSE_MIN_PROTOCOL,(unsigned char*)&protocol,4);
    lpDetails->SetSpecialParamValue(MC_ENT_SPRM_LICENSE_SIGNATURE,(unsigned char*)&dummy_int64,1);
    lpDetails->SetSpecialParamValue(MC_ENT_SPRM_TIMESTAMP,(unsigned char*)&timestamp,4);
    
    int err;
    size_t bytes;
    const unsigned char *script;
    size_t elem_size;
    const unsigned char *elem;
    CScript scriptOpReturn=CScript();
    
            
    vector<CTxDestination> addresses;    
    vector<CTxDestination> fromaddresses;        
    
    
    script=lpDetails->GetData(0,&bytes);
    
    lpDetailsScript->Clear();
        
    err=lpDetailsScript->SetNewEntityType(MC_ENT_TYPE_LICENSE_TOKEN,0,script,bytes);
    if(err)
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid custom fields, too long");                                                        
    }

    elem = lpDetailsScript->GetData(0,&elem_size);
    scriptOpReturn << vector<unsigned char>(elem, elem + elem_size) << OP_DROP << OP_RETURN;                    
        
    

    addresses.push_back(to_address.Get());
    fromaddresses.push_back(from_address.Get());
    CWalletTx wtx;

    EnsureWalletIsUnlocked();
    
    {
        LOCK (pwalletMain->cs_wallet_send);

        SendMoneyToSeveralAddresses(addresses, 0, wtx, lpScript, scriptOpReturn,fromaddresses);
    }
                
    return wtx.GetHash().GetHex();    
}

Value mcd_DebugRequest(string method,const Object& params)
{
    if(method == "issuelicensetoken")
    {
        return mcd_DebugIssueLicenseToken(params);
    }
    if(method == "dbopen")
    {
        mc_Database *m_DB;
        string dbname=mcd_ParamStringValue(params,"dbname","");
        int key_size=mcd_ParamIntValue(params,"keysize",-1);
        int value_size=mcd_ParamIntValue(params,"valuesize",-1);
        int sleep=mcd_ParamIntValue(params,"sleep",1000);
        int64_t res;
        if( (dbname.size() == 0) || (key_size < 0) || (value_size < 0) )
        {            
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameters");                                            
        }
        res=mcd_OpenDatabase(mc_gState->m_NetworkParams->Name(),dbname.c_str(),key_size,value_size,&m_DB);
        __US_Sleep(sleep);
        mcd_CloseDatabase(m_DB);
        return res;
    }
    if(method == "dbwrite")
    {
        mc_Database *m_DB;
        string dbname=mcd_ParamStringValue(params,"dbname","");
        int key_size=mcd_ParamIntValue(params,"keysize",-1);
        int value_size=mcd_ParamIntValue(params,"valuesize",-1);
        int row_count=mcd_ParamIntValue(params,"rows",-1);
        int sleep=mcd_ParamIntValue(params,"sleep",1000);
        int64_t res;
        if( (dbname.size() == 0) || (key_size < 0) || (value_size < 0) || (row_count < 0) )
        {            
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameters");                                            
        }
        res=mcd_OpenDatabase(mc_gState->m_NetworkParams->Name(),dbname.c_str(),key_size,value_size,&m_DB);
        res=mcd_AddRows(m_DB,key_size,value_size,row_count,res);
        __US_Sleep(sleep);
        mcd_CloseDatabase(m_DB);
        return res;
    }
    if(method == "dbread")
    {
        mc_Database *m_DB;
        string dbname=mcd_ParamStringValue(params,"dbname","");
        int key_size=mcd_ParamIntValue(params,"keysize",-1);
        int value_size=mcd_ParamIntValue(params,"valuesize",-1);
        int read_type=mcd_ParamIntValue(params,"type",-1);
        int row_count=mcd_ParamIntValue(params,"rows",-1);
        int sleep=mcd_ParamIntValue(params,"sleep",1000);
        double dres;
        if( (dbname.size() == 0) || (key_size < 0) || (read_type < 0) || (value_size < 0) || (row_count < 0)  )
        {            
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameters");                                            
        }
        mcd_OpenDatabase(mc_gState->m_NetworkParams->Name(),dbname.c_str(),key_size,value_size,&m_DB);
        dres=mcd_ReadRows(m_DB,key_size,row_count,read_type);
        __US_Sleep(sleep);
        mcd_CloseDatabase(m_DB);
        return dres;
    }
    if(method == "chunksdump")
    {
        int force=mcd_ParamIntValue(params,"force",0);
        string message=mcd_ParamStringValue(params,"message","Debug");        
        pwalletTxsMain->m_ChunkDB->Dump("Debug",force);
        return Value::null;
    }
    if(method == "chunkscommit")
    {
        pwalletTxsMain->m_ChunkDB->Commit(-3);
        return Value::null;
    }
    if(method == "walletdump")
    {
        int force=mcd_ParamIntValue(params,"force",0);        
        string message=mcd_ParamStringValue(params,"message","Debug");
        pwalletTxsMain->m_Database->Dump("Debug",force);
        return Value::null;
    }
    if(method == "publishrandom")
    {
        int size=mcd_ParamIntValue(params,"size",-1);
        string stream=mcd_ParamStringValue(params,"stream","");
        string options=mcd_ParamStringValue(params,"options","");
        string key=mcd_ParamStringValue(params,"key","");        
        if( (stream.size() == 0) || (size < 0))
        {            
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameters");                                            
        }
        Array cbc_params;
        string bcname=createbinarycache(cbc_params,false).get_str();
        int part_size=100000;
        int total_size=0;
        int num_parts=(size-1)/part_size+1;
        char *vbuf;
        vbuf=new char[part_size];
        for(int p=0;p<num_parts;p++)
        {
            Array abc_params;
            abc_params.push_back(bcname);
            int this_size=part_size;
            if(p == num_parts-1)
            {
                this_size=size-p*part_size;
            }
            GetRandBytes((unsigned char*)vbuf, this_size);            
            string strHex = HexStr(vbuf, vbuf+this_size);
            abc_params.push_back(strHex);
            total_size+=this_size;
            if(appendbinarycache(abc_params,false).get_int() != total_size)
            {
                throw JSONRPCError(RPC_INTERNAL_ERROR, "Cannot add to binary cache");                                                            
            }
        }
        delete [] vbuf;

        Array p_params;
        p_params.push_back(stream);
        p_params.push_back(key);
        Object c_obj;
        c_obj.push_back(Pair("cache",bcname));
        p_params.push_back(c_obj);
        p_params.push_back(options);
        string strTxID=publish(p_params,false).get_str();        
        Array dbc_params;
        dbc_params.push_back(bcname);
        deletebinarycache(dbc_params,false);
        
        return strTxID;
    }    
    
    throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid request type");         
    
    return Value::null;
}

Value debug(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 4)  
        throw runtime_error("Help message not found\n");
    
    if(!GetBoolArg("-rpcallowdebug",false))
    {
        throw JSONRPCError(RPC_NOT_ALLOWED, "API is not allowed");                 
    }
    
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
            
    delay=1000;
    attempts=10;
    
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
           
    if(params[1].type() != obj_type)
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid request details, should be object");                    
    }
    
    if(request_type == MC_RMT_NONE)
    {
        return mcd_DebugRequest(params[0].get_str(),params[1].get_obj());
//        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid request type");                    
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
                                collector.InsertChunk((unsigned char*)&chunk_hash,&entity,(unsigned char*)&txid,vout,chunk_size,0,0);
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
                                uint256 txid=0;
                                int vout=-1;
                                mc_TxEntity entity;
                                mc_EntityDetails stream_entity;
                                entity.Zero();
                                BOOST_FOREACH(const Pair& dd, cd.get_obj()) 
                                {
                                    if(dd.name_ == "hash")
                                    {
                                        chunk_hash = ParseHashV(dd.value_.get_str(), "hash");
                                    }
                                    if(dd.name_ == "stream")
                                    {
                                        parseStreamIdentifier(dd.value_.get_str(),&stream_entity);           
                                        memcpy(&entity,stream_entity.GetTxID()+MC_AST_SHORT_TXID_OFFSET,MC_AST_SHORT_TXID_SIZE);
                                        entity.m_EntityType=MC_TET_STREAM;
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
                                
                                if(pwalletTxsMain->m_ChunkDB->GetChunkDef(&chunk_def,(unsigned char *)&chunk_hash,
                                        (entity.m_EntityType == MC_TET_NONE) ? NULL: &entity,(txid == 0) ? NULL : (unsigned char*)&txid,vout) == MC_ERR_NOERROR)
                                {
                                    chunk_found=pwalletTxsMain->m_ChunkDB->GetChunk(&chunk_def,0,-1,&chunk_bytes,NULL,NULL);
                                    if(chunk_found)
                                    {
                                        chunk_obj.push_back(Pair("size",(int)chunk_bytes));
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
/*        
        printf("%s",request_str.c_str());        
        printf(". Request ID: %s\n",request_id.ToString().c_str());
 */ 
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
