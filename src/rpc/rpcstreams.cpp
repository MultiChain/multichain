// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2014-2016 The Bitcoin Core developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.


#include "rpc/rpcwallet.h"
#include "json/json_spirit_ubjson.h"
#include "json/json_spirit_reader_template.h"
#include "json/json_spirit_writer_template.h"
#include "community/community.h"

#define MC_QPR_MAX_UNCHECKED_TX_LIST_SIZE    1048576
#define MC_QPR_MAX_MERGED_TX_LIST_SIZE          1024
#define MC_QPR_MAX_DIRTY_TX_LIST_SIZE           5000
#define MC_QPR_MAX_CLEAN_TX_LIST_SIZE           5000
#define MC_QPR_MAX_SECONDARY_TX_LIST_SIZE    1048576
#define MC_QPR_TX_CHECK_COST                     100
#define MC_QPR_MAX_TX_PER_BLOCK                    4



Value createupgradefromcmd(const Array& params, bool fHelp);
Value createfilterfromcmd(const Array& params, bool fHelp);
Value createvariablefromcmd(const Array& params, bool fHelp);

void parseStreamIdentifier(Value stream_identifier,mc_EntityDetails *entity)
{
    unsigned char buf[32];
    unsigned char buf_a[MC_AST_ASSET_REF_SIZE];
    unsigned char buf_n[MC_AST_ASSET_REF_SIZE];
    int ret;
    
    if (stream_identifier.type() != null_type && !stream_identifier.get_str().empty())
    {        
        string str=stream_identifier.get_str();
        
        if(AssetRefDecode(buf_a,str.c_str(),str.size()))
        {
            memset(buf_n,0,MC_AST_ASSET_REF_SIZE);
            if(memcmp(buf_a,buf_n,4) == 0)
            {
                unsigned char *root_stream_name;
                int root_stream_name_size;
                root_stream_name=(unsigned char *)mc_gState->m_NetworkParams->GetParam("rootstreamname",&root_stream_name_size);        
                if(mc_gState->m_NetworkParams->IsProtocolMultichain() == 0)
                {
                    root_stream_name_size=0;
                }    
                if( (root_stream_name_size > 1) && (memcmp(buf_a,buf_n,MC_AST_ASSET_REF_SIZE) == 0) )
                {
                    str=strprintf("%s",root_stream_name);
                }
                else
                {
                    throw JSONRPCError(RPC_ENTITY_NOT_FOUND, "Stream with this stream reference not found: "+str);                    
                }
            }
        }
        
        ret=ParseAssetKey(str.c_str(),buf,NULL,NULL,NULL,NULL,MC_ENT_TYPE_STREAM);
        switch(ret)
        {
            case MC_ASSET_KEY_INVALID_TXID:
                throw JSONRPCError(RPC_ENTITY_NOT_FOUND, "Stream with this txid not found: "+str);
                break;
            case MC_ASSET_KEY_INVALID_REF:
                throw JSONRPCError(RPC_ENTITY_NOT_FOUND, "Stream with this stream reference not found: "+str);
                break;
            case MC_ASSET_KEY_INVALID_NAME:
                throw JSONRPCError(RPC_ENTITY_NOT_FOUND, "Stream with this name not found: "+str);
                break;
            case MC_ASSET_KEY_INVALID_SIZE:
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Could not parse stream key: "+str);
                break;
/*                
            case 1:
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Unconfirmed stream: "+str);
                break;
 */ 
        }
    }
    else
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid stream identifier");        
    }
           
    if(entity)
    {
        if(mc_gState->m_Assets->FindEntityByTxID(entity,buf))
        {
            if(entity->GetEntityType() != MC_ENT_TYPE_STREAM)
            {
                throw JSONRPCError(RPC_ENTITY_NOT_FOUND, "Invalid stream identifier, not stream");                        
            }
        }    
    }    
}

Value getstreaminfo(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        mc_ThrowHelpMessage("getstreaminfo");        
//       throw runtime_error("Help message not found\n");
    
    if(params[0].type() != str_type)
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid stream identifier, expected string");                                                        
    }
    
    uint32_t output_level;
    mc_EntityDetails entity;
    ParseEntityIdentifier(params[0].get_str(),&entity, MC_ENT_TYPE_STREAM);           

    output_level=0x06;
    
    if (params.size() > 1)    
    {
        if(paramtobool(params[1]))
        {
            output_level=0x126;            
        }
    }
    
    
    return StreamEntry(entity.GetTxID(),output_level);
}


Value liststreams(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 4)
        throw runtime_error("Help message not found\n");

    Array results;
    mc_Buffer *streams;
    unsigned char *txid;
    uint32_t output_level;
    
    int count,start;
    count=2147483647;
    if (params.size() > 2)    
    {
        count=paramtoint(params[2],true,0,"Invalid count");
    }
    start=-count;
    if (params.size() > 3)    
    {
        start=paramtoint(params[3],false,0,"Invalid start");
    }
    
    streams=NULL;
    
    vector<string> inputStrings;
    if (params.size() > 0 && params[0].type() != null_type && ((params[0].type() != str_type) || (params[0].get_str() !="*" ) ) )
    {        
        if(params[0].type() == str_type)
        {
            inputStrings.push_back(params[0].get_str());
            if(params[0].get_str() == "")
            {
                return results;                
            }
        }
        else
        {
            inputStrings=ParseStringList(params[0]);        
            if(inputStrings.size() == 0)
            {
                return results;
            }
        }
    }
    if(inputStrings.size())
    {
        {
            LOCK(cs_main);
            for(int is=0;is<(int)inputStrings.size();is++)
            {
                string param=inputStrings[is];

                mc_EntityDetails stream_entity;
                parseStreamIdentifier(param,&stream_entity);           

                streams=mc_gState->m_Assets->GetEntityList(streams,stream_entity.GetTxID(),MC_ENT_TYPE_STREAM);
            }
        }
    }
    else
    {        
        {
            LOCK(cs_main);
            streams=mc_gState->m_Assets->GetEntityList(streams,NULL,MC_ENT_TYPE_STREAM);
        }
    }
    
    if(streams == NULL)
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Cannot open entity database");

    output_level=0x9E;
    
    if (params.size() > 1)    
    {
        if(paramtobool(params[1]))
        {
            output_level=0x01BE;           
            if(mc_gState->m_Features->StreamFilters())
            {
                output_level |= 0x40;    
            }
        }
    }
    
    
    int root_stream_name_size;
    mc_gState->m_NetworkParams->GetParam("rootstreamname",&root_stream_name_size);        
    mc_AdjustStartAndCount(&count,&start,streams->GetCount());        
    
    
    Array partial_results;
    int unconfirmed_count=0;
    if(count > 0)
    {
        for(int i=0;i<streams->GetCount();i++)
        {
            Object entry;

            txid=streams->GetRow(i);
            entry=StreamEntry(txid,output_level);
            if(entry.size()>0)
            {
                BOOST_FOREACH(const Pair& p, entry) 
                {
                    if(p.name_ == "streamref")
                    {
                        if(p.value_.type() == str_type)
                        {
                            results.push_back(entry);                        
                        }
                        else
                        {
                            unconfirmed_count++;
                        }
                    }
                }            
            }            
        }

        sort(results.begin(), results.end(), AssetCompareByRef);
        
        for(int i=0;i<streams->GetCount();i++)
        {
            Object entry;

            txid=streams->GetRow(i);

            entry=StreamEntry(txid,output_level);
            if(entry.size()>0)
            {
                BOOST_FOREACH(const Pair& p, entry) 
                {
                    if(p.name_ == "streamref")
                    {
                        if(p.value_.type() != str_type)
                        {
                            results.push_back(entry);                        
                        }
                    }
                }            
            }            
        }
    }
        
    bool return_partial=false;
    if(count != streams->GetCount()-1)
    {
        return_partial=true;
    }
    mc_gState->m_Assets->FreeEntityList(streams);
    if(return_partial)
    {
        for(int i=start;i<start+count;i++)
        {
            partial_results.push_back(results[i]);                                                                
        }
        return partial_results;
    }
     
    return results;
}

Value createstreamfromcmd(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 4)
        throw runtime_error("Help message not found\n");

    if (strcmp(params[1].get_str().c_str(),"stream"))
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid entity type, should be stream");

    CWalletTx wtx;
    
    mc_Script *lpScript=mc_gState->m_TmpBuffers->m_RpcScript3;   
    lpScript->Clear();
    mc_Script *lpDetailsScript=mc_gState->m_TmpBuffers->m_RpcScript1;    
    lpDetailsScript->Clear();
    mc_Script *lpDetails=mc_gState->m_TmpBuffers->m_RpcScript2;
    lpDetails->Clear();
    
    int ret,type;
    bool missing_salted=true;
    string stream_name="";

    if (params[2].type() != str_type)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid stream name, should be string");
            
    if(!params[2].get_str().empty())
    {        
        stream_name=params[2].get_str();
    }
    
    if(stream_name == "*")
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid stream name: *");                                                                                            
    }

    unsigned char buf_a[MC_AST_ASSET_REF_SIZE];    
    if(AssetRefDecode(buf_a,stream_name.c_str(),stream_name.size()))
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid stream name, looks like a stream reference");                                                                                                    
    }
            
    
    if(stream_name.size())
    {
        ret=ParseAssetKey(stream_name.c_str(),NULL,NULL,NULL,NULL,&type,MC_ENT_TYPE_ANY);
        if(ret != MC_ASSET_KEY_INVALID_NAME)
        {
            if(type == MC_ENT_KEYTYPE_NAME)
            {
                throw JSONRPCError(RPC_DUPLICATE_NAME, "Stream or asset with this name already exists");                                    
            }
            else
            {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid stream name");                                    
            }
        }        
    }

    lpScript->Clear();
    
    lpDetails->Clear();
    lpDetails->AddElement();
    
    if(mc_gState->m_Features->OffChainData())
    {
        string strError;
        int errorCode=RPC_INVALID_PARAMETER;
        uint32_t permissions=0;
        uint32_t restrict=0;
        if(params[3].type() != bool_type)
        {
            if(params[3].type() == obj_type)
            {
                BOOST_FOREACH(const Pair& d, params[3].get_obj()) 
                {
                    if(d.name_ == "restrict")
                    {
                        if(!RawDataParseRestrictParameter(d.value_,&restrict,&permissions,&errorCode,&strError))
                        {
                            throw JSONRPCError(errorCode, strError);                                                                           
                        }
                    }
                    else
                    {
                        if(mc_gState->m_Features->SaltedChunks())
                        {                            
                            if(d.name_ == "salted")
                            {
                                if(d.value_.type() == bool_type)
                                {
                                    if(d.value_.get_bool())
                                    {
                                        restrict |= MC_ENT_ENTITY_RESTRICTION_NEED_SALTED;
                                    }
                                    missing_salted=false;
                                }    
                                else
                                {
                                    throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid salted, should be boolean");                                                    
                                }
                            }
                            else
                            {
                                throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid field, should be restrict or salted");               
                            }                        
                        }
                        else
                        {
                            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid field, should be restrict");                                           
                        }
                    }
                }
            }
            else
            {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid open flag, should be boolean or object");               
            }
        }
        else
        {            
            permissions = params[3].get_bool() ? MC_PTP_NONE : MC_PTP_WRITE;
        }
        lpDetails->SetSpecialParamValue(MC_ENT_SPRM_PERMISSIONS,(unsigned char*)&permissions,1);                                
        if(missing_salted)
        {
            if(permissions & MC_PTP_READ)
            {
                restrict |= MC_ENT_ENTITY_RESTRICTION_NEED_SALTED;
            }
        }
        if(permissions & MC_PTP_READ)
        {
            restrict |= MC_ENT_ENTITY_RESTRICTION_ONCHAIN;
/*            
            if( (restrict & MC_ENT_ENTITY_RESTRICTION_ONCHAIN ) == 0 )
            {
                throw JSONRPCError(RPC_NOT_ALLOWED, "onchain restriction should be set for read-permissioned streams");                               
            }
 */ 
        }        
        
        if(restrict & MC_ENT_ENTITY_RESTRICTION_OFFCHAIN)
        {
            if(restrict & MC_ENT_ENTITY_RESTRICTION_ONCHAIN)
            {
                throw JSONRPCError(RPC_NOT_SUPPORTED, "Stream cannot be restricted from both onchain and offchain items");               
            }                        
        }                            
        
        if( restrict != 0 )
        {
            lpDetails->SetSpecialParamValue(MC_ENT_SPRM_RESTRICTIONS,(unsigned char*)&restrict,1);                         
        }
        
    }
    else
    {
        if(params[3].type() != bool_type)
        {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid open flag, should be boolean or object");
        }

        if(params[3].get_bool())
        {
            unsigned char b=1;        
            lpDetails->SetSpecialParamValue(MC_ENT_SPRM_ANYONE_CAN_WRITE,&b,1);        
        }
    }
    if(stream_name.size())
    {        
        lpDetails->SetSpecialParamValue(MC_ENT_SPRM_NAME,(unsigned char*)(stream_name.c_str()),stream_name.size());//+1);
    }
    
    
/*
    if (params.size() > 4)
    {
        if(params[4].type() == obj_type)
        {
            Object objParams = params[4].get_obj();
            BOOST_FOREACH(const Pair& s, objParams) 
            {  
                lpDetails->SetParamValue(s.name_.c_str(),s.name_.size(),(unsigned char*)s.value_.get_str().c_str(),s.value_.get_str().size());                
            }
        }
        else
        {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid custom fields, expecting object");                                        
        }
    }
*/
    
    vector<CTxDestination> addresses;       
    vector<CTxDestination> fromaddresses;
    CScript scriptOpReturn=CScript();
    
    EnsureWalletIsUnlocked();
    int errorCode=RPC_INVALID_PARAMETER;
    string strError;    
    lpDetailsScript->Clear();
    if (params.size() > 4)
    {
        ParseRawDetails(&(params[4]),lpDetails,lpDetailsScript,&errorCode,&strError);        
        if(strError.size())
        {
            goto exitlbl;
        }
    }
    lpDetailsScript->Clear();
    
    int err;
    size_t bytes;
    const unsigned char *script;
    script=lpDetails->GetData(0,&bytes);
    

    size_t elem_size;
    const unsigned char *elem;
    
    err=lpDetailsScript->SetNewEntityType(MC_ENT_TYPE_STREAM,0,script,bytes);
    if(err)
    {
        strError= "Invalid custom fields or stream name, too long";
        goto exitlbl;
//            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid custom fields or stream name, too long");                                                        
    }

    elem = lpDetailsScript->GetData(0,&elem_size);
    scriptOpReturn << vector<unsigned char>(elem, elem + elem_size) << OP_DROP << OP_RETURN;        
    
    
    if(params[0].get_str() != "*")
    {
        fromaddresses=ParseAddresses(params[0].get_str(),false,false);

        if(fromaddresses.size() != 1)
        {
            strError= "Single from-address should be specified";
            goto exitlbl;
//            throw JSONRPCError(RPC_INVALID_PARAMETER, "Single from-address should be specified");                        
        }

        if( (IsMine(*pwalletMain, fromaddresses[0]) & ISMINE_SPENDABLE) != ISMINE_SPENDABLE )
        {
            strError= "Private key for from-address is not found in this wallet";
            errorCode=RPC_WALLET_ADDRESS_NOT_FOUND;
            goto exitlbl;
//            throw JSONRPCError(RPC_WALLET_ADDRESS_NOT_FOUND, "Private key for from-address is not found in this wallet");                        
        }
        
        set<CTxDestination> thisFromAddresses;

        BOOST_FOREACH(const CTxDestination& fromaddress, fromaddresses)
        {
            thisFromAddresses.insert(fromaddress);
        }

        CPubKey pkey;
        if(!pwalletMain->GetKeyFromAddressBook(pkey,MC_PTP_CREATE,&thisFromAddresses))
        {
            strError= "from-address doesn't have create permission";
            errorCode=RPC_INSUFFICIENT_PERMISSIONS;
            goto exitlbl;
//            throw JSONRPCError(RPC_INSUFFICIENT_PERMISSIONS, "from-address doesn't have create permission");                
        }   
    }
    else
    {
        CPubKey pkey;
        if(!pwalletMain->GetKeyFromAddressBook(pkey,MC_PTP_CREATE))
        {
            strError= "This wallet doesn't have keys with create permission";
            errorCode=RPC_INSUFFICIENT_PERMISSIONS;
            goto exitlbl;
            throw JSONRPCError(RPC_INSUFFICIENT_PERMISSIONS, "This wallet doesn't have keys with create permission");                
        }        
    }
    
    
    {
        LOCK (pwalletMain->cs_wallet_send);

        SendMoneyToSeveralAddresses(addresses, 0, wtx, lpScript, scriptOpReturn,fromaddresses);
    }
    
exitlbl:

    if(strError.size())
    {
        throw JSONRPCError(errorCode, strError);            
    }
                
    return wtx.GetHash().GetHex();    
}

Value createfromcmd(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 4)
        throw runtime_error("Help message not found\n");
    
    if (strcmp(params[1].get_str().c_str(),"stream") == 0)
    {
        return createstreamfromcmd(params,fHelp);    
    }
    
    if (strcmp(params[1].get_str().c_str(),"upgrade") == 0)
    {
        return createupgradefromcmd(params,fHelp);    
    }
    if (strcmp(params[1].get_str().c_str(),"txfilter") == 0)
    {
        return createfilterfromcmd(params,fHelp);    
    }
    if (strcmp(params[1].get_str().c_str(),"streamfilter") == 0)
    {
        return createfilterfromcmd(params,fHelp);    
    }
    if (strcmp(params[1].get_str().c_str(),"variable") == 0)
    {
        return createvariablefromcmd(params,fHelp);    
    }
    
    throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid entity type, should be stream");
}

Value createcmd(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 3)
        throw runtime_error("Help message not found\n");
    
    Array ext_params;
    ext_params.push_back("*");
    BOOST_FOREACH(const Value& value, params)
    {
        ext_params.push_back(value);
    }
    
    return createfromcmd(ext_params,fHelp);    
}

Value publish(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 3 || params.size() > 4)
        throw runtime_error("Help message not found\n");
        
    Array ext_params;
    ext_params.push_back("*");
    BOOST_FOREACH(const Value& value, params)
    {
        ext_params.push_back(value);
    }
    
    return publishfrom(ext_params,fHelp);    
}

Value publishmulti(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 3)
        throw runtime_error("Help message not found\n");
        
    Array ext_params;
    ext_params.push_back("*");
    BOOST_FOREACH(const Value& value, params)
    {
        ext_params.push_back(value);
    }
    
    return publishmultifrom(ext_params,fHelp);    
}

Value publishmultifrom(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 3 || params.size() > 4)
        throw runtime_error("Help message not found\n");
    
    Array out_params;
    
    bool from_address_specified=false;
    mc_EntityDetails stream_entity;
    parseStreamIdentifier(params[1],&stream_entity);           
    
    vector<CTxDestination> fromaddresses;        
    set<uint256> stream_hashes;
    set<CTxDestination> valid_addresses;        
    set<CTxDestination> next_addresses;        
    
    string default_options="";
    if(params.size() > 3 )
    {
        if(params[3].type() != str_type)
        {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Stream item options must be offchain or empty");                                                                                                                            
        }
        if( mc_gState->m_Features->OffChainData() == 0 )
        {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Format options are not supported by this protocol version");                                                                                                                            
        }        
        if(params[3].get_str().size())
        {
            if(params[3].get_str() == "offchain")
            {
                default_options = "offchain";
            }
            else
            {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Stream item options must be offchain or empty");                                                                                                                            
            }
        }
    }
    
    if(params[0].get_str() != "*")
    {
        from_address_specified=true;
        fromaddresses=ParseAddresses(params[0].get_str(),false,false);

        if(fromaddresses.size() != 1)
        {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Single from-address should be specified");                        
        }
        if( (IsMine(*pwalletMain, fromaddresses[0]) & ISMINE_SPENDABLE) != ISMINE_SPENDABLE )
        {
            throw JSONRPCError(RPC_WALLET_ADDRESS_NOT_FOUND, "Private key for from-address is not found in this wallet");                        
        }
    }

    FindAddressesWithPublishPermission(fromaddresses,&stream_entity);
    
    BOOST_FOREACH(const CTxDestination& from_address, fromaddresses) 
    {
        if( (IsMine(*pwalletMain, from_address) & ISMINE_SPENDABLE) == ISMINE_SPENDABLE )
        {
            valid_addresses.insert(from_address);
        }
    }    
            
    stream_hashes.insert(*(uint256*)stream_entity.GetTxID());
    if(params[2].type() != array_type)
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Items should be array");                                
    }    

    if((int)params[2].get_array().size() > MCP_MAX_STD_OP_RETURN_COUNT)
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Number of items exceeds %d (max-std-op-returns-count)",MCP_MAX_STD_OP_RETURN_COUNT));                                                    
    }
    
    Array out_items;
    
    
    BOOST_FOREACH(const Value& data, params[2].get_array()) 
    {
        if(data.type() != obj_type)
        {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Items should be array of objects");                                            
        }
        
        bool for_found=false;
        bool options_found=false;
        Object out_item;
        
        BOOST_FOREACH(const Pair& d, data.get_obj()) 
        {
            if(d.name_ == "for")
            {
                uint256 hash;
                mc_EntityDetails item_entity;
                parseStreamIdentifier(d.value_,&item_entity);       
                hash=*(uint256*)item_entity.GetTxID();
                if(stream_hashes.find(hash) == stream_hashes.end())
                {
                    stream_hashes.insert(hash);
                    vector<CTxDestination> other_addresses;        
                    FindAddressesWithPublishPermission(other_addresses,&item_entity);
                    next_addresses.clear();
                    BOOST_FOREACH(const CTxDestination& other_address, other_addresses) 
                    {
                        if(valid_addresses.find(other_address) != valid_addresses.end())
                        {
                            if( (IsMine(*pwalletMain, other_address) & ISMINE_SPENDABLE) == ISMINE_SPENDABLE )
                            {
                                next_addresses.insert(other_address);                            
                            }
                        }
                    }    
                    valid_addresses=next_addresses;
                    if(valid_addresses.size() == 0)
                    {
                        if(from_address_specified)
                        {
                            throw JSONRPCError(RPC_INSUFFICIENT_PERMISSIONS, "Publishing in this stream is not allowed from this address.");                            
                        }
                        throw JSONRPCError(RPC_INSUFFICIENT_PERMISSIONS, "This wallet contains no addresses with permission to write to all streams and global send permission.");                                                                                                                    
                    }
                }
                for_found=true;
            }
            if(d.name_ == "options")
            {
                options_found=true;
            }                    
        }   
        
        out_item=data.get_obj();
        if(!for_found)
        {
            out_item.push_back(Pair("for",params[1]));
        }
        if(default_options.size())
        {
            if(!options_found)
            {
                out_item.push_back(Pair("options",default_options));
            }            
        }
        
        out_items.push_back(out_item);
    }
    
    CTxDestination out_address;
    
    if(fromaddresses.size() == 1)
    {
        out_address=fromaddresses[0];
    }
    else
    {
        set<uint160> setAddressUints;
        set<uint160> *lpSetAddressUint=NULL;

        BOOST_FOREACH(const CTxDestination& from_address, valid_addresses) 
        {
            const CKeyID *lpKeyID=boost::get<CKeyID> (&from_address);
            const CScriptID *lpScriptID=boost::get<CScriptID> (&from_address);
            if(lpKeyID)
            {
                setAddressUints.insert(*(uint160*)lpKeyID);
            }
            else
            {
                if(lpScriptID)
                {
                    setAddressUints.insert(*(uint160*)lpScriptID);
                }
           }        
        }
        lpSetAddressUint=&setAddressUints;

        uint32_t flags= MC_CSF_ALLOW_SPENDABLE_P2SH | MC_CSF_ALLOWED_COINS_ARE_MINE;
        vector<COutput> vecOutputs;

        pwalletMain->AvailableCoins(vecOutputs, false, NULL, true, true, 0, lpSetAddressUint,flags);
        if(vecOutputs.size() == 0)
        {
            if(from_address_specified)
            {
                throw JSONRPCError(RPC_INSUFFICIENT_PERMISSIONS, "from-address doesn't have unlocked unspent outputs.");                            
            }
            throw JSONRPCError(RPC_WALLET_NO_UNSPENT_OUTPUTS, "Addresses with permission to write to all streams don't have unlocked unspent outputs.");
        }

        COutput deepest_coin=vecOutputs[0];
        int max_depth=deepest_coin.nDepth;

        for(int i=1;i<(int)vecOutputs.size();i++)
        {
            if(vecOutputs[i].nDepth > max_depth)
            {
                deepest_coin=vecOutputs[i];
                max_depth=deepest_coin.nDepth;
            }
        }

        CTxOut txout;
        deepest_coin.GetHashAndTxOut(txout);

        if (!ExtractDestination(txout.scriptPubKey, out_address))
        {
            throw JSONRPCError(RPC_INTERNAL_ERROR, "Addresses with permission to write to all streams don't have unlocked unspent outputs.");        
        }
    }
    
    Object empty_object;
    
    out_params.push_back(CBitcoinAddress(out_address).ToString());
    out_params.push_back(empty_object);
    out_params.push_back(out_items);
    out_params.push_back("send");
    
    return createrawsendfrom(out_params,false);
}

Value publishfrom(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 4 || params.size() > 5)
        throw runtime_error("Help message not found\n");

    mc_Script *lpScript=mc_gState->m_TmpBuffers->m_RpcScript3;
    lpScript->Clear();
    
    mc_EntityDetails stream_entity;
    parseStreamIdentifier(params[1],&stream_entity);           
               
    
    // Wallet comments
    CWalletTx wtx;
            
    uint32_t in_options,out_options;
    
    in_options=MC_RFD_OPTION_NONE;
    out_options=MC_RFD_OPTION_NONE;
    
    vector<CTxDestination> addresses;    
    
    vector<CTxDestination> fromaddresses;        
    EnsureWalletIsUnlocked();
    
    if(params[0].get_str() != "*")
    {
        fromaddresses=ParseAddresses(params[0].get_str(),false,false);

        if(fromaddresses.size() != 1)
        {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Single from-address should be specified");                        
        }
        if( (IsMine(*pwalletMain, fromaddresses[0]) & ISMINE_SPENDABLE) != ISMINE_SPENDABLE )
        {
            throw JSONRPCError(RPC_WALLET_ADDRESS_NOT_FOUND, "Private key for from-address is not found in this wallet");                        
        }
    }

    FindAddressesWithPublishPermission(fromaddresses,&stream_entity);
        
    Array keys;
    
    if(params[2].type() == str_type)
    {
        keys.push_back(params[2]);
    }
    else
    {
        if(params[2].type() == array_type)
        {
            keys=params[2].get_array();
        }
        else
        {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Item keys should be either string or array");                                                                                                                
        }
    }
    
    if(keys.size() == 0)
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Item keys array cannot be empty");                                                                                                                
    }
    
    for(int k=0;k<(int)keys.size();k++)
    {
        if(keys[k].type() != str_type)
        {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Item key should be string");                                                                                                                
        }        
        if(keys[k].get_str().size() > MC_ENT_MAX_ITEM_KEY_SIZE)
        {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Item key is too long");                                                                                                    
        }        
        if(keys[k].get_str() == "*")
        {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid item-key-string: *");                
        }
    }

    if(params.size() > 4 )
    {
        if(params[4].type() != str_type)
        {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Stream item options must be offchain or empty");                                                                                                                            
        }
        if( mc_gState->m_Features->OffChainData() == 0 )
        {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Format options are not supported by this protocol version");                                                                                                                            
        }        
        if(params[4].get_str().size())
        {
            if(params[4].get_str() == "offchain")
            {
                in_options |= MC_RFD_OPTION_OFFCHAIN;
            }
            else
            {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Stream item options must be offchain or empty");                                                                                                                            
            }
        }
    }

    if( mc_gState->m_Features->OffChainData() )
    {
        if(in_options & MC_RFD_OPTION_OFFCHAIN)
        {
            if(stream_entity.Restrictions() & MC_ENT_ENTITY_RESTRICTION_OFFCHAIN)
            {
                throw JSONRPCError(RPC_NOT_ALLOWED, "Publishing offchain items is not allowed to this stream");     
            }
        }
        else
        {
            if(stream_entity.Restrictions() & MC_ENT_ENTITY_RESTRICTION_ONCHAIN)
            {
                throw JSONRPCError(RPC_NOT_ALLOWED, "Publishing onchain items is not allowed to this stream");     
            }            
        }
    }
    
    if(keys.size() > 1)
    {
        if( mc_gState->m_Features->MultipleStreamKeys() == 0 )
        {
            throw JSONRPCError(RPC_NOT_SUPPORTED, "Multiple keys are not supported by this protocol version");                            
        }
    }
    
    mc_Script *lpDetailsScript=mc_gState->m_TmpBuffers->m_RpcScript1;
    lpDetailsScript->Clear();
    
    uint32_t data_format=MC_SCR_DATA_FORMAT_UNKNOWN;
    
    vector<unsigned char> dataData;
    lpDetailsScript->Clear();

    string strError;
    int errorCode=RPC_INVALID_PARAMETER;
    vector<uint256> vChunkHashes;
    
    dataData=ParseRawFormattedData(&(params[3]),&data_format,lpDetailsScript,in_options,&out_options,&errorCode,&strError);

    if(strError.size())
    {
        throw JSONRPCError(errorCode, strError);                                                                                                                
    }
    
    size_t elem_size;
    const unsigned char *elem;
    CScript scriptOpReturn=CScript();
    
    lpDetailsScript->Clear();
    lpDetailsScript->SetEntity(stream_entity.GetTxID()+MC_AST_SHORT_TXID_OFFSET);
    for(int k=0;k<(int)keys.size();k++)
    {
        lpDetailsScript->SetItemKey((unsigned char*)keys[k].get_str().c_str(),keys[k].get_str().size());
    }

    if( (in_options & MC_RFD_OPTION_OFFCHAIN) == 0)
    {
        if( data_format != MC_SCR_DATA_FORMAT_UNKNOWN )
        {
            lpDetailsScript->SetDataFormat(data_format);
        }        
    }
    
    for(int e=0;e<lpDetailsScript->GetNumElements();e++)
    {
        elem = lpDetailsScript->GetData(e,&elem_size);
        if(elem_size > 0)
        {
            scriptOpReturn << vector<unsigned char>(elem, elem + elem_size) << OP_DROP;
        }                
    }    
    
    if(stream_entity.AnyoneCanRead() == 0)
    {
        pEF->LIC_RPCVerifyFeature(MC_EFT_STREAM_READ_RESTRICTED_WRITE,"Publishing to read-restricted stream");
    }
    
    if(stream_entity.Restrictions() & MC_ENT_ENTITY_RESTRICTION_NEED_SALTED)
    {
        out_options |= MC_RFD_OPTION_SALTED;
    }
    
    lpDetailsScript->Clear();
    if(in_options & MC_RFD_OPTION_OFFCHAIN)        
    {
        AppendOffChainFormatData(data_format,out_options,lpDetailsScript,dataData,&vChunkHashes,&errorCode,&strError);
        if(strError.size())
        {
            throw JSONRPCError(errorCode, strError);                                                                                                                
        }
        elem = lpDetailsScript->GetData(0,&elem_size);
        scriptOpReturn << vector<unsigned char>(elem, elem + elem_size) << OP_DROP;                    
        scriptOpReturn << OP_RETURN;                                                
    }
    else
    {
        if(out_options & MC_RFD_OPTION_OFFCHAIN)
        {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "chunks data type is not allowed with missing options field");                
        }
        lpDetailsScript->AddElement();
        if(dataData.size())
        {
            lpDetailsScript->SetData(&dataData[0],dataData.size());
        }
        elem = lpDetailsScript->GetData(0,&elem_size);
        if(elem_size > 0)
        {
            scriptOpReturn << OP_RETURN << vector<unsigned char>(elem, elem + elem_size);
        }
        else
        {
            scriptOpReturn << OP_RETURN;
        }
    }
    

    lpScript->Clear();
         
    LOCK (pwalletMain->cs_wallet_send);
    
    SendMoneyToSeveralAddresses(addresses, 0, wtx, lpScript, scriptOpReturn,fromaddresses);

    return wtx.GetHash().GetHex();    
}

Value trimsubscribe(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 2)
        throw runtime_error("Help message not found\n");
    
    pEF->ENT_RPCVerifyEdition("trimsubscribe API");
    
    string indexes=params[1].get_str();
    
    vector<mc_EntityDetails> inputEntities;
    vector<string> inputStrings;
    if(params[0].type() == str_type)
    {
        inputStrings.push_back(params[0].get_str());
    }
    else
    {    
        inputStrings=ParseStringList(params[0]);
    }
    
    for(int is=0;is<(int)inputStrings.size();is++)
    {
        mc_EntityDetails entity_to_subscribe;
        Value param=inputStrings[is];
        ParseEntityIdentifier(param,&entity_to_subscribe, MC_ENT_TYPE_STREAM);           
        inputEntities.push_back(entity_to_subscribe);
    }
    
    for(int is=0;is<(int)inputStrings.size();is++)
    {
        mc_EntityDetails* lpEntity;
        lpEntity=&inputEntities[is];
        
        mc_TxEntity entity;
        entity.Zero();
        memcpy(entity.m_EntityID,lpEntity->GetTxID()+MC_AST_SHORT_TXID_OFFSET,MC_AST_SHORT_TXID_SIZE);
        entity.m_EntityType=MC_TET_STREAM | MC_TET_CHAINPOS;
        pEF->STR_TrimSubscription(&entity,indexes);        
    }
    
    return Value::null;
}


Value subscribe(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 3 )
        throw runtime_error("Help message not found\n");

    if((mc_gState->m_WalletMode & MC_WMD_TXS) == 0)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "API is not supported with this wallet version. To get this functionality, run \"multichaind -walletdbversion=2 -rescan\" ");        
    }   
       
    // Whether to perform rescan after import
    bool fRescan = true;    
    string indexes="all";
    
    if (params.size() > 1)
    {
        if(params[1].type() == bool_type)
        {
            fRescan = params[1].get_bool();
        }
    }

    if (params.size() > 2)
    {
        pEF->ENT_RPCVerifyEdition("Controlled subscriptions");
        indexes=params[2].get_str();
    }
    
    vector<mc_EntityDetails> inputEntities;
    vector<string> inputStrings;
    if(params[0].type() == str_type)
    {
        inputStrings.push_back(params[0].get_str());
    }
    else
    {    
        inputStrings=ParseStringList(params[0]);
    }
    
    for(int is=0;is<(int)inputStrings.size();is++)
    {
        mc_EntityDetails entity_to_subscribe;
        Value param=inputStrings[is];
        ParseEntityIdentifier(param,&entity_to_subscribe, MC_ENT_TYPE_ANY);           
        if(entity_to_subscribe.AnyoneCanRead() == 0)
        {
            pEF->LIC_RPCVerifyFeature(MC_EFT_STREAM_READ_RESTRICTED_READ,"Subscribing to read-restricted stream");
            if(!pEF->WLT_FindReadPermissionedAddress(&entity_to_subscribe).IsValid())
            {
                throw JSONRPCError(RPC_INSUFFICIENT_PERMISSIONS, "This wallet doesn't have keys with read permission for stream "+inputStrings[is]);                
            }
        }
        inputEntities.push_back(entity_to_subscribe);
    }
    
    bool fNewFound=false;
    for(int is=0;is<(int)inputStrings.size();is++)
    {
        mc_EntityDetails* lpEntity;
        lpEntity=&inputEntities[is];
        
        mc_TxEntity entity;
        if(lpEntity->GetEntityType() == MC_ENT_TYPE_STREAM)
        {
            entity.Zero();
            memcpy(entity.m_EntityID,lpEntity->GetTxID()+MC_AST_SHORT_TXID_OFFSET,MC_AST_SHORT_TXID_SIZE);
            entity.m_EntityType=MC_TET_STREAM | MC_TET_CHAINPOS;
            if(pwalletTxsMain->AddEntity(&entity,MC_EFL_NOT_IN_SYNC) != MC_ERR_FOUND)
            {
                entity.m_EntityType=MC_TET_STREAM | MC_TET_TIMERECEIVED;
                pwalletTxsMain->AddEntity(&entity,MC_EFL_NOT_IN_SYNC);
                entity.m_EntityType=MC_TET_STREAM_KEY | MC_TET_CHAINPOS;
                pwalletTxsMain->AddEntity(&entity,MC_EFL_NOT_IN_SYNC);
                entity.m_EntityType=MC_TET_STREAM_KEY | MC_TET_TIMERECEIVED;
                pwalletTxsMain->AddEntity(&entity,MC_EFL_NOT_IN_SYNC);
                entity.m_EntityType=MC_TET_STREAM_PUBLISHER | MC_TET_CHAINPOS;
                pwalletTxsMain->AddEntity(&entity,MC_EFL_NOT_IN_SYNC);
                entity.m_EntityType=MC_TET_STREAM_PUBLISHER | MC_TET_TIMERECEIVED;
                pwalletTxsMain->AddEntity(&entity,MC_EFL_NOT_IN_SYNC);
                fNewFound=true;
            }                
            entity.m_EntityType=MC_TET_STREAM | MC_TET_CHAINPOS;
            if(pEF->STR_CreateSubscription(&entity,indexes) != MC_ERR_FOUND)
            {
                fNewFound=true;                
            }
        }
        else
        {
            if (params.size() > 2)
            {
                throw JSONRPCError(RPC_NOT_ALLOWED, "Subscription parameters can be specified only for streams");                        
            }            
        }

        if(lpEntity->GetEntityType() == MC_ENT_TYPE_ASSET)
        {
            entity.Zero();
            memcpy(entity.m_EntityID,lpEntity->GetShortRef(),mc_gState->m_NetworkParams->m_AssetRefSize);
            entity.m_EntityType=MC_TET_ASSET | MC_TET_CHAINPOS;
            if(pwalletTxsMain->AddEntity(&entity,MC_EFL_NOT_IN_SYNC) != MC_ERR_FOUND)
            {
                entity.m_EntityType=MC_TET_ASSET | MC_TET_TIMERECEIVED;
                pwalletTxsMain->AddEntity(&entity,MC_EFL_NOT_IN_SYNC);
                fNewFound=true;
            }
        }
    }
    
    if (fRescan && fNewFound)
    {
        pwalletMain->ScanForWalletTransactions(chainActive.Genesis(), true, true, true);
    }

    return Value::null;
}


Value unsubscribe(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error("Help message not found\n");

    if((mc_gState->m_WalletMode & MC_WMD_TXS) == 0)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "API is not supported with this wallet version. To get this functionality, run \"multichaind -walletdbversion=2 -rescan\" ");        
    }   
       
    bool purge=false;
    vector<mc_EntityDetails> inputEntities;
    vector<string> inputStrings;
    if(params[0].type() == str_type)
    {
        inputStrings.push_back(params[0].get_str());
    }
    else
    {    
        inputStrings=ParseStringList(params[0]);
    }
    
    if(params.size() > 1)
    {
        if(params[1].type() == bool_type)
        {
            purge=params[1].get_bool();
        }
        else
        {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid value for 'purge' field, should be boolean");                                                                
        }        
    }
    
    for(int is=0;is<(int)inputStrings.size();is++)
    {
        mc_EntityDetails entity_to_subscribe;
        Value param=inputStrings[is];
        ParseEntityIdentifier(param,&entity_to_subscribe, MC_ENT_TYPE_ANY);           
        inputEntities.push_back(entity_to_subscribe);
    }
        
    int32_t buf_mode=MC_BUF_MODE_DEFAULT;
    if(inputStrings.size() > 1)
    {
        buf_mode=MC_BUF_MODE_MAP;
    }
    mc_Buffer *streams=mc_gState->m_TmpBuffers->m_RpcBuffer1;
    streams->Initialize(sizeof(mc_TxEntity),sizeof(mc_TxEntity),buf_mode);
    
    
    bool fNewFound=false;
    for(int is=0;is<(int)inputStrings.size();is++)
    {
        mc_EntityDetails* lpEntity;
        lpEntity=&inputEntities[is];
    
        mc_TxEntity entity;
        if(lpEntity->GetEntityType() == MC_ENT_TYPE_STREAM)
        {
            entity.Zero();
            memcpy(entity.m_EntityID,lpEntity->GetTxID()+MC_AST_SHORT_TXID_OFFSET,MC_AST_SHORT_TXID_SIZE);
            entity.m_EntityType=MC_TET_STREAM | MC_TET_CHAINPOS;
            streams->Add(&entity,NULL);
            entity.m_EntityType=MC_TET_STREAM | MC_TET_TIMERECEIVED;
            streams->Add(&entity,NULL);
            entity.m_EntityType=MC_TET_STREAM_KEY | MC_TET_CHAINPOS;
            streams->Add(&entity,NULL);
            entity.m_EntityType=MC_TET_STREAM_KEY | MC_TET_TIMERECEIVED;
            streams->Add(&entity,NULL);
            entity.m_EntityType=MC_TET_STREAM_PUBLISHER | MC_TET_CHAINPOS;
            streams->Add(&entity,NULL);
            entity.m_EntityType=MC_TET_STREAM_PUBLISHER | MC_TET_TIMERECEIVED;
            streams->Add(&entity,NULL);
            fNewFound=true;
        }

        if(lpEntity->GetEntityType() == MC_ENT_TYPE_ASSET)
        {
            entity.Zero();
            memcpy(entity.m_EntityID,lpEntity->GetShortRef(),mc_gState->m_NetworkParams->m_AssetRefSize);
            entity.m_EntityType=MC_TET_ASSET | MC_TET_CHAINPOS;
            streams->Add(&entity,NULL);
            entity.m_EntityType=MC_TET_ASSET | MC_TET_TIMERECEIVED;
            streams->Add(&entity,NULL);
            fNewFound=true;
        }
    }

    if(fNewFound)
    {
        if(pwalletTxsMain->Unsubscribe(streams,purge))
        {
            throw JSONRPCError(RPC_INTERNAL_ERROR, "Couldn't unsubscribe from stream");                                    
        }
        for(int is=0;is<(int)inputStrings.size();is++)
        {
            mc_EntityDetails* lpEntity;
            lpEntity=&inputEntities[is];

            mc_TxEntity entity;
            entity.Zero();
            memcpy(entity.m_EntityID,lpEntity->GetTxID()+MC_AST_SHORT_TXID_OFFSET,MC_AST_SHORT_TXID_SIZE);
            entity.m_EntityType=MC_TET_STREAM | MC_TET_CHAINPOS;
            pEF->STR_TrimSubscription(&entity,"unsubscribe");        
        }
    }

    return Value::null;
}

Value liststreamtxitems(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 3)
        throw runtime_error("Help message not found\n");
   
    if((mc_gState->m_WalletMode & MC_WMD_TXS) == 0)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "API is not supported with this wallet version. For full streams functionality, run \"multichaind -walletdbversion=2 -rescan\" ");        
    }   
           
    mc_EntityDetails stream_entity;
    parseStreamIdentifier(params[0],&stream_entity);           
    
    mc_TxEntityStat entStat;
    entStat.Zero();
    memcpy(&entStat,stream_entity.GetTxID()+MC_AST_SHORT_TXID_OFFSET,MC_AST_SHORT_TXID_SIZE);
    entStat.m_Entity.m_EntityType=MC_TET_STREAM;
    entStat.m_Entity.m_EntityType |= MC_TET_CHAINPOS;

    if(!pwalletTxsMain->FindEntity(&entStat))
    {
        throw JSONRPCError(RPC_NOT_SUBSCRIBED, "Not subscribed to this stream");                                
    }
    
    bool verbose=false;
    
    if (params.size() > 2)    
    {
        verbose=paramtobool(params[2]);
    }
    
    Array output_array;
    
    vector<string> inputStrings;
    
    inputStrings=ParseStringList(params[1]);
    
    for(int j=0;j<(int)inputStrings.size();j++)
    {
        uint256 hash = ParseHashV(inputStrings[j], "txid");
        
        const CWalletTx& wtx=pwalletTxsMain->GetWalletTx(hash,NULL,NULL);

        int first_output=0;
        int stream_output;
        while(first_output < (int)wtx.vout.size())
        {
            Object entry=StreamItemEntry(wtx,first_output,stream_entity.GetTxID()+MC_AST_SHORT_TXID_OFFSET,verbose,NULL,&stream_output);   

            if(stream_output < (int)wtx.vout.size())
            {
                output_array.push_back(entry);
            }
            first_output=stream_output+1;
        }
    }   
    
//    uint256 hash = ParseHashV(params[1], "parameter 2");
    
    
    
    return output_array;    
}

Value getstreamitem(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 3)
        throw runtime_error("Help message not found\n");
   
    Array items=liststreamtxitems(params,fHelp).get_array();
    
    if(items.size() == 0)
    {
        throw JSONRPCError(RPC_TX_NOT_FOUND, "This transaction was not found in this stream");                        
    }
    if(items.size() > 1)
    {
        throw JSONRPCError(RPC_NOT_ALLOWED, "This transaction has more than one output for this stream, please use liststreamtxitems");                                
    }

    return items[0];    
}

int mc_GetHashAndFirstOutput(mc_TxEntityRow *lpEntTx,uint256 *hash)
{
    int first_output=0;
    int count;
    mc_TxEntityRow erow;

    memcpy(hash,lpEntTx->m_TxId,MC_TDB_TXID_SIZE);        
    if(lpEntTx->m_Flags & MC_TFL_IS_EXTENSION)
    {
        erow.Zero();
        memcpy(&erow.m_Entity,&lpEntTx->m_Entity,sizeof(mc_TxEntity));
        erow.m_Generation=lpEntTx->m_Generation;
        erow.m_Pos=lpEntTx->m_Pos;
        first_output=(int)mc_GetLE(lpEntTx->m_TxId+MC_TEE_OFFSET_IN_TXID,sizeof(uint32_t));
        count=(int)mc_GetLE(lpEntTx->m_TxId+MC_TEE_OFFSET_IN_TXID+sizeof(uint32_t),sizeof(uint32_t));
        if((int)erow.m_Pos > count)
        {
            erow.m_Pos-=count;
            if(pwalletTxsMain->GetRow(&erow) == 0)
            {
                memcpy(hash,erow.m_TxId,MC_TDB_TXID_SIZE);                
            }
        }
    }
    
    return first_output;
}

Value liststreamitems(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 5)
        throw runtime_error("Help message not found\n");

    if((mc_gState->m_WalletMode & MC_WMD_TXS) == 0)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "API is not supported with this wallet version. For full streams functionality, run \"multichaind -walletdbversion=2 -rescan\" ");        
    }   
           
    mc_TxEntityStat entStat;
    
    mc_EntityDetails stream_entity;
    parseStreamIdentifier(params[0],&stream_entity);           

    int count,start;
    bool verbose=false;
    
    if (params.size() > 1)    
    {
        verbose=paramtobool(params[1]);
    }
    
    count=10;
    if (params.size() > 2)    
    {
        count=paramtoint(params[2],true,0,"Invalid count");
    }
    start=-count;
    if (params.size() > 3)    
    {
        start=paramtoint(params[3],false,0,"Invalid start");
    }
    
    bool fLocalOrdering = false;
    if (params.size() > 4)
        fLocalOrdering = params[4].get_bool();
    
    entStat.Zero();
    memcpy(&entStat,stream_entity.GetTxID()+MC_AST_SHORT_TXID_OFFSET,MC_AST_SHORT_TXID_SIZE);
    entStat.m_Entity.m_EntityType=MC_TET_STREAM;
    if(fLocalOrdering)
    {
        entStat.m_Entity.m_EntityType |= MC_TET_TIMERECEIVED;
    }
    else
    {
        entStat.m_Entity.m_EntityType |= MC_TET_CHAINPOS;
    }
    if(!pwalletTxsMain->FindEntity(&entStat))
    {
        throw JSONRPCError(RPC_NOT_SUBSCRIBED, "Not subscribed to this stream");                                
    }
    
    mc_Buffer *entity_rows=mc_gState->m_TmpBuffers->m_RpcEntityRows;
    entity_rows->Clear();
    
    mc_AdjustStartAndCount(&count,&start,entStat.m_LastPos);
    
    Array retArray;
    CheckWalletError(pwalletTxsMain->GetList(&entStat.m_Entity,start+1,count,entity_rows),entStat.m_Entity.m_EntityType,"");

    for(int i=0;i<entity_rows->GetCount();i++)
    {
        mc_TxEntityRow *lpEntTx;
        lpEntTx=(mc_TxEntityRow*)entity_rows->GetRow(i);
        uint256 hash;
        int first_output=mc_GetHashAndFirstOutput(lpEntTx,&hash);
        const CWalletTx& wtx=pwalletTxsMain->GetWalletTx(hash,NULL,NULL);
        Object entry=StreamItemEntry(wtx,first_output,stream_entity.GetTxID()+MC_AST_SHORT_TXID_OFFSET,verbose,NULL,NULL);
        if(entry.size())
        {
            retArray.push_back(entry);                                
        }
    }
    
    return retArray;
}

void getTxsForBlockRange(vector <uint256>& txids,mc_TxEntity *entity,int height_from,int height_to,mc_Buffer *entity_rows)
{
    int first_item,last_item,count,i;
    
    last_item=pwalletTxsMain->GetBlockItemIndex(entity,height_to);
    if(last_item)
    {
        first_item=pwalletTxsMain->GetBlockItemIndex(entity,height_from-1)+1;
        count=last_item-first_item+1;
        if(count > 0)
        {
            CheckWalletError(pwalletTxsMain->GetList(entity,first_item,count,entity_rows),entity->m_EntityType,"");
            
            mc_TxEntityRow *lpEntTx;
            uint256 hash;
            for(i=0;i<count;i++)
            {
                lpEntTx=(mc_TxEntityRow*)entity_rows->GetRow(i);
                if( (lpEntTx->m_Flags & MC_TFL_IS_EXTENSION) == 0 )
                {
                    memcpy(&hash,lpEntTx->m_TxId,MC_TDB_TXID_SIZE);
                    txids.push_back(hash);
                }
            }
        }        
    }
}

Value liststreamblockitems(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 5)
        throw runtime_error("Help message not found\n");

    if((mc_gState->m_WalletMode & MC_WMD_TXS) == 0)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "API is not supported with this wallet version. For full streams functionality, run \"multichaind -walletdbversion=2 -rescan\" ");        
    }   
           
    mc_TxEntityStat entStat;
    
    mc_EntityDetails stream_entity;
    parseStreamIdentifier(params[0],&stream_entity);           

    int count,start;
    bool verbose=false;
    
    if (params.size() > 2)    
    {
        verbose=paramtobool(params[2]);
    }
    
    count=2147483647;
    if (params.size() > 3)    
    {
        count=paramtoint(params[3],true,0,"Invalid count");
    }
    start=-count;
    if (params.size() > 4)    
    {
        start=paramtoint(params[4],false,0,"Invalid start");
    }
    
    entStat.Zero();
    memcpy(&entStat,stream_entity.GetTxID()+MC_AST_SHORT_TXID_OFFSET,MC_AST_SHORT_TXID_SIZE);
    entStat.m_Entity.m_EntityType=MC_TET_STREAM;
    entStat.m_Entity.m_EntityType |= MC_TET_CHAINPOS;
    if(!pwalletTxsMain->FindEntity(&entStat))
    {
        throw JSONRPCError(RPC_NOT_SUBSCRIBED, "Not subscribed to this stream");                                
    }
    
    
    vector <int> heights=ParseBlockSetIdentifier(params[1]);
    vector <uint256> txids;
    
    Array retArray;
    if(heights.size() == 0)
    {
        return retArray;
    }
    
    int height_from,height_to;
    height_from=heights[0];
    height_to=heights[0];

    mc_Buffer *entity_rows=mc_gState->m_TmpBuffers->m_RpcEntityRows;
    entity_rows->Clear();
    
    for(unsigned int i=1;i<heights.size();i++)
    {
        if(heights[i] > height_to + 1)
        {
            getTxsForBlockRange(txids,&entStat.m_Entity,height_from,height_to,entity_rows);
            height_from=heights[i];
        }
        height_to=heights[i];
    }
    
    
    getTxsForBlockRange(txids,&entStat.m_Entity,height_from,height_to,entity_rows);
    
    mc_AdjustStartAndCount(&count,&start,txids.size());
    
    for(int i=start;i<start+count;i++)
    {
        const CWalletTx& wtx=pwalletTxsMain->GetWalletTx(txids[i],NULL,NULL);
        int first_output=0;
        int stream_output;
        while(first_output < (int)wtx.vout.size())
        {
            Object entry=StreamItemEntry(wtx,first_output,stream_entity.GetTxID()+MC_AST_SHORT_TXID_OFFSET,verbose,NULL,&stream_output);
            if(entry.size())
            {
                retArray.push_back(entry);                                
            }
            first_output=stream_output+1;
        }
    }
    
    return retArray;
}


bool getSubKeyEntityFromKey(string str,mc_TxEntityStat entStat,mc_TxEntity *entity,bool ignore_unsubscribed)
{
    if(str == "*")
    {
        return false;
    }
    uint160 key_string_hash;
    uint160 stream_subkey_hash;
    key_string_hash=Hash160(str.begin(),str.end());
    mc_GetCompoundHash160(&stream_subkey_hash,entStat.m_Entity.m_EntityID,&key_string_hash);
    memcpy(entity->m_EntityID,&stream_subkey_hash,MC_TDB_ENTITY_ID_SIZE);
    entity->m_EntityType=entStat.m_Entity.m_EntityType | MC_TET_SUBKEY;   
    if(pEF->STR_IsIndexSkipped(NULL,&(entStat.m_Entity),entity))
    {
        if(ignore_unsubscribed)
        {
            return false;
        }
        CheckWalletError(MC_ERR_NOT_ALLOWED,entStat.m_Entity.m_EntityType,"");
    }
    
    return true;
}

bool getSubKeyEntityFromPublisher(string str,mc_TxEntityStat entStat,mc_TxEntity *entity,bool ignore_unsubscribed)
{
    if(str == "*")
    {
        return false;
    }
    uint160 stream_subkey_hash;
    CBitcoinAddress address(str);
    if (!address.IsValid())
    {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");            
    }
    CTxDestination dest=address.Get();
    CKeyID *lpKeyID=boost::get<CKeyID> (&dest);
    CScriptID *lpScriptID=boost::get<CScriptID> (&dest);

    
    if ((lpKeyID == NULL) && (lpScriptID == NULL) )
    {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");                    
    }

    if(lpKeyID)
    {
        mc_GetCompoundHash160(&stream_subkey_hash,entStat.m_Entity.m_EntityID,lpKeyID);        
    }
    else
    {
        mc_GetCompoundHash160(&stream_subkey_hash,entStat.m_Entity.m_EntityID,lpScriptID);                
    }

    memcpy(entity->m_EntityID,&stream_subkey_hash,MC_TDB_ENTITY_ID_SIZE);
    entity->m_EntityType=entStat.m_Entity.m_EntityType | MC_TET_SUBKEY;    
    
    if(pEF->STR_IsIndexSkipped(NULL,&(entStat.m_Entity),entity))
    {
        if(ignore_unsubscribed)
        {
            return false;
        }
        CheckWalletError(MC_ERR_NOT_ALLOWED,entStat.m_Entity.m_EntityType,"");
    }
    
    return true;
}

Value getstreamsummary(const Array& params, bool fPublisher)
{
    if((mc_gState->m_WalletMode & MC_WMD_TXS) == 0)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "API is not supported with this wallet version. For full streams functionality, run \"multichaind -walletdbversion=2 -rescan\" ");        
    }   

    mc_TxEntityStat entStat;
    mc_TxEntity entity;
    
    mc_EntityDetails stream_entity;
    parseStreamIdentifier(params[0],&stream_entity);           

    entStat.Zero();
    memcpy(&entStat,stream_entity.GetTxID()+MC_AST_SHORT_TXID_OFFSET,MC_AST_SHORT_TXID_SIZE);
    entStat.m_Entity.m_EntityType=MC_TET_STREAM_KEY;
    if(fPublisher)
    {
        entStat.m_Entity.m_EntityType=MC_TET_STREAM_PUBLISHER;        
    }
    entStat.m_Entity.m_EntityType |= MC_TET_CHAINPOS;
    if(!pwalletTxsMain->FindEntity(&entStat))
    {
        throw JSONRPCError(RPC_NOT_SUBSCRIBED, "Not subscribed to this stream");                                
    }

    bool fFirstPublisher=false;
    bool fFirstPublisherAll=false;
    string key_string=params[1].get_str();    
    vector <mc_QueryCondition> conditions;

    if(fPublisher)
    {
        getSubKeyEntityFromPublisher(params[1].get_str(),entStat,&entity,false);    
        conditions.push_back(mc_QueryCondition(MC_QCT_PUBLISHER,params[1].get_str()));
    }
    else
    {
        getSubKeyEntityFromKey(params[1].get_str(),entStat,&entity,false);
        conditions.push_back(mc_QueryCondition(MC_QCT_KEY,params[1].get_str()));
    }
    
    set<string> setFirstPublishers;
    
    vector<string> inputStrings;
    inputStrings=ParseStringList(params[2]);
    uint32_t mode=0;
    for(int j=0;j<(int)inputStrings.size();j++)
    {
        bool found=false;
        if(inputStrings[j]=="jsonobjectmerge")
        {
            mode |= MC_VMM_MERGE_OBJECTS;
            found=true;
        }
        if(inputStrings[j]=="recursive")
        {
            mode |= MC_VMM_RECURSIVE;
            found=true;
        }
        if( (inputStrings[j]=="ignore") || (inputStrings[j]=="ignoreother") )
        {
            mode |= MC_VMM_IGNORE_OTHER;
            found=true;
        }
        if(inputStrings[j]=="ignoremissing")
        {
            mode |= MC_VMM_IGNORE_MISSING;
            found=true;
        }
        if(inputStrings[j]=="noupdate")
        {
            mode |= MC_VMM_TAKE_FIRST;
            mode |= MC_VMM_TAKE_FIRST_FOR_FIELD;
            found=true;
        }
        if(inputStrings[j]=="omitnull")
        {
            mode |= MC_VMM_OMIT_NULL;
            found=true;
        }
        if(!fPublisher)
        {
            if(inputStrings[j]=="firstpublishersany")
            {
                if(fFirstPublisher)
                {
                    throw JSONRPCError(RPC_INVALID_PARAMETER, "firstpublishers* option can appear only once in them mode");                                                                        
                }
                fFirstPublisher=true;
                fFirstPublisherAll=false;                
                found=true;
            }            
            if(inputStrings[j]=="firstpublishersall")
            {
                if(fFirstPublisher)
                {
                    throw JSONRPCError(RPC_INVALID_PARAMETER, "firstpublishers* option can appear only once in them mode");                                                                        
                }
                fFirstPublisher=true;
                fFirstPublisherAll=true;                
                found=true;
            }            
        }
        if(!found)
        {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Unrecognized mode: " + inputStrings[j]);                                            
        }
    }
    
    if( (mode & MC_VMM_MERGE_OBJECTS) == 0)
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "missing jsonobjectmerge");                                                    
    }
    
    mc_Buffer *entity_rows=mc_gState->m_TmpBuffers->m_RpcEntityRows;
    entity_rows->Clear();
        
    Object empty_object;
    Object obj;
    int i,n,c,m,err,pcount;
    bool available;
    bool first_item=true;
    err=MC_ERR_NOERROR;
    n=pwalletTxsMain->GetListSize(&entity,entStat.m_Generation,NULL);
    i=0;
    m=10;
    
    Value result=obj;
    
    while(i<n)
    {
        if((i % m) == 0)
        {
            c=m;
            if(i+c > n)
            {
                c=n-i;
            }
            CheckWalletError(pwalletTxsMain->GetList(&entity,entStat.m_Generation,i+1,c,entity_rows),entity.m_EntityType,"");
        }
        mc_TxEntityRow *lpEntTx;
        lpEntTx=(mc_TxEntityRow*)entity_rows->GetRow(i % m);
        uint256 hash;
        int first_output=mc_GetHashAndFirstOutput(lpEntTx,&hash);
        const CWalletTx& wtx=pwalletTxsMain->GetWalletTx(hash,NULL,NULL);
        Object entry;
        entry=StreamItemEntry(wtx,first_output,stream_entity.GetTxID()+MC_AST_SHORT_TXID_OFFSET,false,&conditions,NULL);
/*        
        if(fPublisher)
        {
            entry=StreamItemEntry(wtx,first_output,stream_entity.GetTxID()+MC_AST_SHORT_TXID_OFFSET,false,NULL,&key_ptr,NULL);
        }
        else
        {
            entry=StreamItemEntry(wtx,first_output,stream_entity.GetTxID()+MC_AST_SHORT_TXID_OFFSET,false,&key_ptr,NULL,NULL);            
        }
*/        
        if(fFirstPublisher)
        {
            pcount=0;
            BOOST_FOREACH(const Pair& a, entry) 
            {
                if(a.name_ == "publishers")
                {
                    Array arr=a.value_.get_array();
                    if(i == 0)
                    {
                        setFirstPublishers.clear();
                        for(unsigned int j=0;j<arr.size();j++) 
                        {
                            setFirstPublishers.insert(arr[j].get_str());
                            pcount++;      
                        }
                    }
                    else
                    {                       
                        for(unsigned int j=0;j<arr.size();j++) 
                        {
                            const set<string>::const_iterator it=setFirstPublishers.find(arr[j].get_str());   
                            if(it != setFirstPublishers.end())
                            {
                                pcount++;      
                            }
                        }
                    }                
                }            
            }
            if( ( fFirstPublisherAll && (pcount != (int)setFirstPublishers.size())) || 
                (!fFirstPublisherAll && (pcount == 0)) )                     
            {
                entry.clear();
            }
        }
        
        available=true;
        BOOST_FOREACH(const Pair& a, entry) 
        {
            if(a.name_ == "offchain")
            {
                available=!a.value_.get_bool();
            }
        }
        
        if(!available)
        {
            BOOST_FOREACH(const Pair& a, entry) 
            {
                if(a.name_ == "available")
                {
                    available=a.value_.get_bool();
                }
            }
        }
                        
        
        BOOST_FOREACH(const Pair& a, entry) 
        {
            if(a.name_ == "data")
            {
                if(available)
                {
                    if(a.value_.type() != null_type)                            // Returned in case of error
                    {
                        if(first_item)
                        {
                            result=a.value_;
                            first_item=false;
                        }
                        else
                        {
                            result=mc_MergeValues(&result,&(a.value_),mode,0,&err);
                        }
                    }
                }
                else
                {
                    available=true;
                    BOOST_FOREACH(const Pair& b, a.value_.get_obj()) 
                    {
                        if(b.name_ == "format")
                        {
                            available=false;
                            if(b.value_.get_str() != "json")
                            {
                                if(mode & MC_VMM_IGNORE_OTHER)
                                {
                                    available=true;
                                }
                                else
                                {
                                    err=MC_ERR_INVALID_PARAMETER_VALUE;                                            
                                    goto exitlbl;
                                }
                            }

                        }
                    }
                    if( (mode & MC_VMM_IGNORE_MISSING) == 0)
                    {
                        if(!available)
                        {
                            throw JSONRPCError(RPC_NOT_ALLOWED, "Some items to be merged are missing (try using \'ignoremissing\')" );                                                                            
                        }
                    }                    
                }
            }
        }    
        if(err)
        {
            goto exitlbl;
        }
        i++;
    }
    
    if(mc_IsJsonObjectForMerge(&result,0))
    {
        Value json=result.get_obj()[0].value_;
        Value empty_value=empty_object;
        json=mc_MergeValues(&json,&empty_value,mode | MC_VMM_TAKE_FIRST,1,&err);     
        obj.push_back(Pair("json", json));        
    }            
    else
    {
        if(!first_item)
        {
            if( (mode & MC_VMM_IGNORE_OTHER) == 0)
            {
                err=MC_ERR_INVALID_PARAMETER_VALUE;
            }
        }
        obj.push_back(Pair("json", empty_object));        
    }
    result=obj;
    
exitlbl:    

    if(err)
    {
        throw JSONRPCError(RPC_NOT_ALLOWED, "Some items to be merged are in the wrong format (try using \'ignoreother\')" );                                                    
    }

    return result;
}

Value getstreamkeysummary(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 3)
        throw runtime_error("Help message not found\n");
    
    return getstreamsummary(params,false);
}

Value getstreampublishersummary(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 3)
        throw runtime_error("Help message not found\n");
    
    return getstreamsummary(params,true);
}

Value liststreamkeyitems(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 6)
        throw runtime_error("Help message not found\n");

    if((mc_gState->m_WalletMode & MC_WMD_TXS) == 0)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "API is not supported with this wallet version. For full streams functionality, run \"multichaind -walletdbversion=2 -rescan\" ");        
    }   
    
    if(params[1].get_str() == "*")
    {
        int count=0;
        Array ext_params;
        BOOST_FOREACH(const Value& value, params)
        {
            if(count != 1)
            {
                ext_params.push_back(value);
            }
            count++;
        }
    
        return liststreamitems(ext_params,fHelp);            
    }
           
    mc_TxEntityStat entStat;
    mc_TxEntity entity;
    
    mc_EntityDetails stream_entity;
    parseStreamIdentifier(params[0],&stream_entity);           

    int count,start;
    bool verbose=false;
    
    if (params.size() > 2)    
    {
        verbose=paramtobool(params[2]);
    }

    count=10;
    if (params.size() > 3)    
    {
        count=paramtoint(params[3],true,0,"Invalid count");
    }
    start=-count;
    if (params.size() > 4)    
    {
        start=paramtoint(params[4],false,0,"Invalid start");
    }
        
    bool fLocalOrdering = false;
    if (params.size() > 5)
        fLocalOrdering = params[5].get_bool();
    
    entStat.Zero();
    memcpy(&entStat,stream_entity.GetTxID()+MC_AST_SHORT_TXID_OFFSET,MC_AST_SHORT_TXID_SIZE);
    entStat.m_Entity.m_EntityType=MC_TET_STREAM_KEY;
    if(fLocalOrdering)
    {
        entStat.m_Entity.m_EntityType |= MC_TET_TIMERECEIVED;
    }
    else
    {
        entStat.m_Entity.m_EntityType |= MC_TET_CHAINPOS;
    }
    if(!pwalletTxsMain->FindEntity(&entStat))
    {
        throw JSONRPCError(RPC_NOT_SUBSCRIBED, "Not subscribed to this stream");                                
    }

    string key_string=params[1].get_str();
    getSubKeyEntityFromKey(params[1].get_str(),entStat,&entity,false);
    
    vector <mc_QueryCondition> conditions;

    conditions.push_back(mc_QueryCondition(MC_QCT_KEY,params[1].get_str()));
   
    mc_Buffer *entity_rows=mc_gState->m_TmpBuffers->m_RpcEntityRows;
    entity_rows->Clear();
    
    mc_AdjustStartAndCount(&count,&start,pwalletTxsMain->GetListSize(&entity,entStat.m_Generation,NULL));
    
    Array retArray;
    CheckWalletError(pwalletTxsMain->GetList(&entity,entStat.m_Generation,start+1,count,entity_rows),entity.m_EntityType,"");
    
    for(int i=0;i<entity_rows->GetCount();i++)
    {
        mc_TxEntityRow *lpEntTx;
        lpEntTx=(mc_TxEntityRow*)entity_rows->GetRow(i);
        uint256 hash;
        int first_output=mc_GetHashAndFirstOutput(lpEntTx,&hash);
        const CWalletTx& wtx=pwalletTxsMain->GetWalletTx(hash,NULL,NULL);
        Object entry=StreamItemEntry(wtx,first_output,stream_entity.GetTxID()+MC_AST_SHORT_TXID_OFFSET,verbose,&conditions,NULL);
        if(entry.size())
        {
            retArray.push_back(entry);                                
        }
    }
    
    return retArray;
}


Value liststreampublisheritems(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 6)
        throw runtime_error("Help message not found\n");

    if((mc_gState->m_WalletMode & MC_WMD_TXS) == 0)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "API is not supported with this wallet version. For full streams functionality, run \"multichaind -walletdbversion=2 -rescan\" ");        
    }   
           
    if(params[1].get_str() == "*")
    {
        int count=0;
        Array ext_params;
        BOOST_FOREACH(const Value& value, params)
        {
            if(count != 1)
            {
                ext_params.push_back(value);
            }
            count++;
        }
    
        return liststreamitems(ext_params,fHelp);            
    }
    
    mc_TxEntityStat entStat;
    mc_TxEntity entity;
    uint160 stream_subkey_hash;
    
    mc_EntityDetails stream_entity;
    parseStreamIdentifier(params[0],&stream_entity);           

    int count,start;
    bool verbose=false;
    
    if (params.size() > 2)    
    {
        verbose=paramtobool(params[2]);
    }

    count=10;
    if (params.size() > 3)    
    {
        count=paramtoint(params[3],true,0,"Invalid count");
    }
    start=-count;
    if (params.size() > 4)    
    {
        start=paramtoint(params[4],false,0,"Invalid start");
    }

    bool fLocalOrdering = false;
    if (params.size() > 5)
        fLocalOrdering = params[5].get_bool();
    
    entStat.Zero();
    memcpy(&entStat,stream_entity.GetTxID()+MC_AST_SHORT_TXID_OFFSET,MC_AST_SHORT_TXID_SIZE);
    entStat.m_Entity.m_EntityType=MC_TET_STREAM_PUBLISHER;
    if(fLocalOrdering)
    {
        entStat.m_Entity.m_EntityType |= MC_TET_TIMERECEIVED;
    }
    else
    {
        entStat.m_Entity.m_EntityType |= MC_TET_CHAINPOS;
    }
    if(!pwalletTxsMain->FindEntity(&entStat))
    {
        throw JSONRPCError(RPC_NOT_SUBSCRIBED, "Not subscribed to this stream");                                
    }

    string key_string=params[1].get_str();
    getSubKeyEntityFromPublisher(params[1].get_str(),entStat,&entity,false);
    
    vector <mc_QueryCondition> conditions;

    conditions.push_back(mc_QueryCondition(MC_QCT_PUBLISHER,params[1].get_str()));
    
    mc_Buffer *entity_rows=mc_gState->m_TmpBuffers->m_RpcEntityRows;
    entity_rows->Clear();
    
    mc_AdjustStartAndCount(&count,&start,pwalletTxsMain->GetListSize(&entity,entStat.m_Generation,NULL));
    
    Array retArray;
    CheckWalletError(pwalletTxsMain->GetList(&entity,entStat.m_Generation,start+1,count,entity_rows),entity.m_EntityType,"");
    
    for(int i=0;i<entity_rows->GetCount();i++)
    {
        mc_TxEntityRow *lpEntTx;
        lpEntTx=(mc_TxEntityRow*)entity_rows->GetRow(i);
        uint256 hash;
        int first_output=mc_GetHashAndFirstOutput(lpEntTx,&hash);
        const CWalletTx& wtx=pwalletTxsMain->GetWalletTx(hash,NULL,NULL);
        Object entry=StreamItemEntry(wtx,first_output,stream_entity.GetTxID()+MC_AST_SHORT_TXID_OFFSET,verbose,&conditions,NULL);
        if(entry.size())
        {
            retArray.push_back(entry);                                
        }
    }
    
    return retArray;
}

bool IsAllowedMapMode(string mode)
{
    if(mode == "list")        return true;
    if(mode == "all")        return true;
    return false;
}

//Value liststreammap_operation(mc_TxEntity *parent_entity,mc_TxEntity *subkey_entity,string subkey_string,int count, int start, string mode)
Value liststreammap_operation(mc_TxEntity *parent_entity,vector<mc_TxEntity>& inputEntities,vector<string>& inputStrings,int count, int start, string mode)
{
    mc_TxEntity entity;
    mc_TxEntityStat entStat;
    Array retArray;
    mc_Buffer *entity_rows=mc_gState->m_TmpBuffers->m_RpcEntityRows;
    mc_TxEntityRow erow;
    uint160 stream_subkey_hash;    
    int row,enitity_count;
    
    entity_rows->Clear();
    enitity_count=inputEntities.size();
    if(enitity_count == 0)
    {
        mc_AdjustStartAndCount(&count,&start,pwalletTxsMain->GetListSize(parent_entity,NULL));
        entity_rows->Clear();
        CheckWalletError(pwalletTxsMain->GetList(parent_entity,start+1,count,entity_rows),parent_entity->m_EntityType,"");
        enitity_count=entity_rows->GetCount();
    }
    else
    {
        mc_AdjustStartAndCount(&count,&start,enitity_count);       
        enitity_count=count;
    }
    
    entStat.Zero();
    if(enitity_count)
    {
        memcpy(&entStat,parent_entity,sizeof(mc_TxEntity));
        pwalletTxsMain->FindEntity(&entStat);
    }
    
    for(int i=0;i<enitity_count;i++)
    {
        mc_TxEntityRow *lpEntTx;
        string key_string;
        if(entity_rows->GetCount())
        {
            lpEntTx=(mc_TxEntityRow*)entity_rows->GetRow(i);
            key_string=pwalletTxsMain->GetSubKey(lpEntTx->m_TxId, NULL,NULL);
            entity.Zero();
            mc_GetCompoundHash160(&stream_subkey_hash,parent_entity->m_EntityID,lpEntTx->m_TxId);
            memcpy(entity.m_EntityID,&stream_subkey_hash,MC_TDB_ENTITY_ID_SIZE);
            entity.m_EntityType=parent_entity->m_EntityType | MC_TET_SUBKEY;
        }
        else
        {
            memcpy(&entity,&(inputEntities[i+start]),sizeof(mc_TxEntity));
            key_string=inputStrings[i+start];
        }
        
        int total,confirmed;
        total=pwalletTxsMain->GetListSize(&entity,entStat.m_Generation,&confirmed);
        
        Object all_entry;
        int shift=total-1;
        if(shift == 0)
        {
            shift=1;
        }
        vector <mc_QueryCondition> conditions;

        if((parent_entity->m_EntityType & MC_TET_TYPE_MASK) == MC_TET_STREAM_PUBLISHER)
        {
            all_entry.push_back(Pair("publisher", key_string));        
            conditions.push_back(mc_QueryCondition(MC_QCT_PUBLISHER,key_string));
        }
        else
        {
            all_entry.push_back(Pair("key", key_string));         
            conditions.push_back(mc_QueryCondition(MC_QCT_KEY,key_string));
        }
        all_entry.push_back(Pair("items", total));                                                                        
        all_entry.push_back(Pair("confirmed", confirmed));                                                                        
        
        if(mode == "all")
        {
            for(row=1;row<=total;row+=shift)
            {
                if( ( (row == 1) && (mode != "last") ) || ( (row == total) && (mode != "first") ) )
                {                    
                    erow.Zero();
                    memcpy(&erow.m_Entity,&entity,sizeof(mc_TxEntity));
                    erow.m_Generation=entStat.m_Generation;
                    erow.m_Pos=row;

                    if(pwalletTxsMain->GetRow(&erow) == 0)
                    {
                        uint256 hash;
                        int first_output=mc_GetHashAndFirstOutput(&erow,&hash);                       
//                        memcpy(&hash,erow.m_TxId,MC_TDB_TXID_SIZE);
                        const CWalletTx& wtx=pwalletTxsMain->GetWalletTx(hash,NULL,NULL);

                        Value item_value;

                        item_value=StreamItemEntry(wtx,first_output,parent_entity->m_EntityID,true,&conditions,NULL);
                        if(row == 1)
                        {
                            all_entry.push_back(Pair("first", item_value));                                                                        
                        }
                        if(row == total)
                        {
                            all_entry.push_back(Pair("last", item_value));                                                                        
                        }
                    }
                }
            }
        }
        retArray.push_back(all_entry);                                
    }

    return retArray;
}

Value liststreamkeys_or_publishers(const Array& params,bool is_publishers)
{
    mc_TxEntity entity;
    mc_TxEntityStat entStat;
    
    mc_EntityDetails stream_entity;
    
    parseStreamIdentifier(params[0],&stream_entity);           

    string mode="list";
    
    if (params.size() > 2)    
    {
        if(paramtobool(params[2]))
        {
            mode="all";            
        }
    }
        
    int count,start;
    count=2147483647;
    if (params.size() > 3)    
    {
        count=paramtoint(params[3],true,0,"Invalid count");
    }
    start=-count;
    if (params.size() > 4)    
    {
        start=paramtoint(params[4],false,0,"Invalid start");
    }
    
    bool fLocalOrdering = false;
    if (params.size() > 5)
        fLocalOrdering = params[5].get_bool();
    
    entStat.Zero();
    memcpy(&entStat,stream_entity.GetTxID()+MC_AST_SHORT_TXID_OFFSET,MC_AST_SHORT_TXID_SIZE);
    if(is_publishers)
    {
        entStat.m_Entity.m_EntityType=MC_TET_STREAM_PUBLISHER;                
    }
    else
    {
        entStat.m_Entity.m_EntityType=MC_TET_STREAM_KEY;        
    }
    if(fLocalOrdering)
    {
        entStat.m_Entity.m_EntityType |= MC_TET_TIMERECEIVED;
    }
    else
    {
        entStat.m_Entity.m_EntityType |= MC_TET_CHAINPOS;
    }
    
    if(!pwalletTxsMain->FindEntity(&entStat))
    {
        throw JSONRPCError(RPC_NOT_SUBSCRIBED, "Not subscribed to this stream");                                
    }

    vector<string> inputStrings;
    vector<mc_TxEntity> inputEntities;
    
    if(params.size() > 1)
    {
        if(!is_publishers && (params[1].type() == str_type) )
        {
            inputStrings.push_back(params[1].get_str());
        }
        else
        {
            inputStrings=ParseStringList(params[1]);
            if(inputStrings.size() == 0)
            {
                Array retArray;                
                return retArray;
            }
        }
        bool take_it=true;
        if( (inputStrings.size() == 1) && (inputStrings[0] == "*") )
        {
            take_it=false;
        }
        if(take_it)
        {            
            for(int is=0;is<(int)inputStrings.size();is++)
            {
                string str=inputStrings[is];
                entity.Zero();

                if(is_publishers)
                {
                    getSubKeyEntityFromPublisher(str,entStat,&entity,false);
                }
                else
                {
                    getSubKeyEntityFromKey(str,entStat,&entity,false);        
                }
                inputEntities.push_back(entity);
            }
        }
    }
    
    return liststreammap_operation(&(entStat.m_Entity),inputEntities,inputStrings,count,start,mode);        
}

Value liststreamkeys(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 6)
        throw runtime_error("Help message not found\n");
    
    if((mc_gState->m_WalletMode & MC_WMD_TXS) == 0)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "API is not supported with this wallet version. For full streams functionality, run \"multichaind -walletdbversion=2 -rescan\" ");        
    }   
           
    return liststreamkeys_or_publishers(params,false);
}

Value liststreampublishers(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 6)
        throw runtime_error("Help message not found\n");
    if((mc_gState->m_WalletMode & MC_WMD_TXS) == 0)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "API is not supported with this wallet version. For full streams functionality, run \"multichaind -walletdbversion=2 -rescan\" ");        
    }   
    
    return liststreamkeys_or_publishers(params,true);
}

int GetAndQueryDirtyList(vector<mc_QueryCondition>& conditions, mc_EntityDetails *stream_entity,bool fLocalOrdering,mc_Buffer *entity_rows)
{
    int i,row,out_row;
    int conditions_count=(int)conditions.size();
    int conditions_used=0;
    int max_size=0;
    int clean_count,dirty_count,last_state;
    vector<mc_TxEntity> vConditionEntities;
    vector<int> vConditionListSizes;
    vector<int> vConditionMerged;
    mc_TxEntityStat entStat;
    bool merge_lists=true;
    bool one_index_found=false;
    bool both_types=false;
    uint32_t error_type=0;
    
    vConditionEntities.resize(conditions_count+1);
    vConditionListSizes.resize(conditions_count+1);
    vConditionMerged.resize(conditions_count+1);
    
    entStat.Zero();
    memcpy(&entStat,stream_entity->GetTxID()+MC_AST_SHORT_TXID_OFFSET,MC_AST_SHORT_TXID_SIZE);
    entStat.m_Entity.m_EntityType=MC_TET_STREAM;
    if(fLocalOrdering)
    {
        entStat.m_Entity.m_EntityType |= MC_TET_TIMERECEIVED;
    }
    else
    {
        entStat.m_Entity.m_EntityType |= MC_TET_CHAINPOS;
    }
    if(!pwalletTxsMain->FindEntity(&entStat))
    {
        throw JSONRPCError(RPC_NOT_SUBSCRIBED, "Not subscribed to this stream");                                
    }
    
    for(i=0;i<=conditions_count;i++)
    {
        vConditionEntities[i].Zero();
        vConditionListSizes[i]=-1;
        vConditionMerged[i]=0;
        
        entStat.m_Entity.m_EntityType &= MC_TET_ORDERMASK;
        bool index_found=true;
        if(i<conditions_count)
        {
            index_found=false;
            switch(conditions[i].m_Type)
            {
                case MC_QCT_KEY:
                    entStat.m_Entity.m_EntityType |= MC_TET_STREAM_KEY;
                    index_found=getSubKeyEntityFromKey(conditions[i].m_Value,entStat,&vConditionEntities[i],true);                
                    break;
                case MC_QCT_PUBLISHER:
                    entStat.m_Entity.m_EntityType |= MC_TET_STREAM_PUBLISHER;
                    index_found=getSubKeyEntityFromPublisher(conditions[i].m_Value,entStat,&vConditionEntities[i],true);                
                    break;
            }
            if(index_found)
            {
                one_index_found=true;                
            }
            else
            {
                if(error_type)
                {
                    if(error_type != entStat.m_Entity.m_EntityType)
                    {
                        both_types=true;
                    }
                }
                else
                {
                    error_type=entStat.m_Entity.m_EntityType;
                }
            }
        }
        else
        {
            entStat.m_Entity.m_EntityType |= MC_TET_STREAM;
            memcpy(&vConditionEntities[i],&entStat.m_Entity,sizeof(mc_TxEntity));
        }
        if(index_found)
        {
            if(vConditionEntities[i].m_EntityType)
            {
                vConditionListSizes[i]=pwalletTxsMain->GetListSize(&vConditionEntities[i],entStat.m_Generation,NULL);     
                if(vConditionListSizes[i]>max_size)
                {
                    max_size=vConditionListSizes[i];
                }
            }
        }
    }
    
    if(!one_index_found)
    {
        CheckWalletError(MC_ERR_NOT_ALLOWED,error_type,both_types ? "Both the keys and publishers indexes are not active for this subscription." : "");        
    }
    
    clean_count=0;
    dirty_count=0;
    
    while(merge_lists)
    {
        int min_size=max_size+1;
        int min_condition=conditions_count;
        for(i=0;i<conditions_count;i++)
        {
            if(vConditionMerged[i] == 0)
            {
                if(vConditionListSizes[i] > 0)
                {
                    if(vConditionListSizes[i]<=min_size)
                    {
                        min_size=vConditionListSizes[i];
                        min_condition=i;
                    }
                }
            }
        }
        
        if(min_condition<conditions_count)
        {
            merge_lists=true;
            if(conditions_used == 0)
            {
                if(min_size > MC_QPR_MAX_UNCHECKED_TX_LIST_SIZE)
                {
                    throw JSONRPCError(RPC_NOT_SUPPORTED, "This query may take too much time");                                                    
                }          
                CheckWalletError(pwalletTxsMain->GetList(&vConditionEntities[min_condition],entStat.m_Generation,1,min_size,entity_rows),vConditionEntities[min_condition].m_EntityType,"");         
                conditions_used++;
                clean_count=0;
                dirty_count=0;
                for(row=0;row<entity_rows->GetCount();row++)
                {
                    mc_TxEntityRow *lpEntTx;
                    lpEntTx=(mc_TxEntityRow*)entity_rows->GetRow(row);
                    lpEntTx->m_TempPos=0;
                    if( (lpEntTx->m_Flags & MC_TFL_IS_EXTENSION) == 0 )
                    {
                        clean_count++;
                    }
                    if(!fLocalOrdering)
                    {
                        if(lpEntTx->m_Block == -1)
                        {
                            lpEntTx->m_Block=chainActive.Height()+1;
                        }
                    }
                }
            }
            else
            {
                merge_lists=false;
            }
        }
        else
        {
            merge_lists=false;            
        }
    }

    last_state=2;
    clean_count=0;
    dirty_count=0;
    out_row=0;
    for(row=0;row<entity_rows->GetCount();row++)
    {
        mc_TxEntityRow *lpEntTx;
        lpEntTx=(mc_TxEntityRow*)entity_rows->GetRow(row);
        if(lpEntTx->m_Flags & MC_TFL_IS_EXTENSION)
        {
            lpEntTx->m_TempPos=last_state;
            if(lpEntTx->m_TempPos == 2)
            {
                dirty_count++;                
            }
        }
        else
        {
            switch(lpEntTx->m_TempPos)
            {
                case 1:
                    break;
                case 2:
                    dirty_count++;
                    break;
                default:
                    if(conditions_used < conditions_count)
                    {
                        lpEntTx->m_TempPos=2;
                        dirty_count++;
                    }
                    else
                    {
                        clean_count++;
                    }
                    break;                    
            }
            last_state=lpEntTx->m_TempPos;
        }
        if(lpEntTx->m_TempPos != 1)
        {
            if(out_row < row)
            {
                memcpy(entity_rows->GetRow(out_row),lpEntTx,entity_rows->m_Size);
            }
            out_row++;
        }
    }
    
    entity_rows->SetCount(out_row);
    
    return dirty_count;
}

void FillConditionsList(vector<mc_QueryCondition>& conditions, Value param)
{
    bool key_found=false;
    bool publisher_found=false;
    bool field_parsed;
    
    if(param.type() != obj_type)
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid query, should be object ");                                                            
    }
    
    BOOST_FOREACH(const Pair& d, param.get_obj()) 
    {
        field_parsed=false;
        if(d.name_ == "key")
        {
            if(key_found)
            {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Only one of the key fields can appear in the object");                                                            
            }
            if(d.value_.type()==str_type)
            {
                conditions.push_back(mc_QueryCondition(MC_QCT_KEY,d.value_.get_str()));
            }
            else
            {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid key, should be string");                                                            
            }
            field_parsed=true;
            key_found=true;
        }
        
        if(d.name_ == "keys")
        {
            if( mc_gState->m_Features->MultipleStreamKeys() == 0 )
            {
                throw JSONRPCError(RPC_NOT_SUPPORTED, "Multiple keys are not supported by this protocol version");                                                            
            }
            if(key_found)
            {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Only one of the key fields can appear in the object");                                                            
            }
            if(d.value_.type() == array_type)
            {
                for(int i=0;i<(int)d.value_.get_array().size();i++)
                {
                    if(d.value_.get_array()[i].type()==str_type)
                    {
                        conditions.push_back(mc_QueryCondition(MC_QCT_KEY,d.value_.get_array()[i].get_str()));
                    }
                    else
                    {
                        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid key, should be string");                                                            
                    }
                }                
            }            
            else
            {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid keys, should be array");                                                            
            }
            field_parsed=true;
            key_found=true;
        }

        if(d.name_ == "publisher")
        {
            if(publisher_found)
            {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Only one of the publisher fields can appear in the object");                                                            
            }
            if(d.value_.type()==str_type)
            {
                conditions.push_back(mc_QueryCondition(MC_QCT_PUBLISHER,d.value_.get_str()));
            }
            else
            {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid publisher, should be string");                                                            
            }
            field_parsed=true;
            publisher_found=true;
        }

        if(d.name_ == "publishers")
        {
            if(publisher_found)
            {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Only one of the publisher fields can appear in the object");                                                            
            }
            if(d.value_.type() == array_type)
            {
                for(int i=0;i<(int)d.value_.get_array().size();i++)
                {
                    if(d.value_.get_array()[i].type()==str_type)
                    {
                        conditions.push_back(mc_QueryCondition(MC_QCT_PUBLISHER,d.value_.get_array()[i].get_str()));
                    }
                    else
                    {
                        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid publisher, should be string");                                                            
                    }
                }                
            }            
            else
            {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid publishers, should be array");                                                            
            }
            field_parsed=true;
            publisher_found=true;
        }
        
        if(!field_parsed)
        {
            throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Invalid field: %s",d.name_.c_str()));                                                            
        }

    }        
}

Value liststreamqueryitems(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 3)
        throw runtime_error("Help message not found\n");

    if((mc_gState->m_WalletMode & MC_WMD_TXS) == 0)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "API is not supported with this wallet version. For full streams functionality, run \"multichaind -walletdbversion=2 -rescan\" ");        
    }   

    vector <mc_QueryCondition> conditions;
    vector <mc_QueryCondition>* lpConditions;
    
    mc_EntityDetails stream_entity;
    parseStreamIdentifier(params[0],&stream_entity);           

    bool verbose=false;
    int dirty_count,max_count;
    
    if (params.size() > 2)    
    {
        verbose=paramtobool(params[2]);
    }
    
    FillConditionsList(conditions,params[1]);    

    if(conditions.size() == 0)
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid query, cannot be empty");                                                            
    }
    
    mc_Buffer *entity_rows=mc_gState->m_TmpBuffers->m_RpcEntityRows;
    entity_rows->Clear();
    
    dirty_count=GetAndQueryDirtyList(conditions,&stream_entity,false,entity_rows);
    max_count=GetArg("-maxqueryscanitems",MAX_STREAM_QUERY_ITEMS);
    if(dirty_count > max_count)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, 
                strprintf("This query requires decoding %d items, which is above the maxqueryscanitems limit of %d.",
                dirty_count,max_count));     
    }          
    
    if(entity_rows->GetCount() > max_count)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "Resulting list is too large");                                                            
    }
    
    Array retArray;
    int last_output;
    uint256 last_hash=0;
    for(int i=0;i<entity_rows->GetCount();i++)
    {
        mc_TxEntityRow *lpEntTx;
        lpEntTx=(mc_TxEntityRow*)entity_rows->GetRow(i);
        lpConditions=NULL;
        if(lpEntTx->m_TempPos != 1)
        {
            if(lpEntTx->m_TempPos == 2)
            {
                lpConditions=&conditions;
            }
            uint256 hash;
            int first_output=mc_GetHashAndFirstOutput(lpEntTx,&hash);
            if(last_hash == hash)
            {
                if(first_output <= last_output)
                {
                    first_output=-1;
                }
            }
            else
            {
                last_output=-1;
            }
            last_hash=hash;
            if(first_output >= 0)
            {
                const CWalletTx& wtx=pwalletTxsMain->GetWalletTx(hash,NULL,NULL);
                Object entry=StreamItemEntry(wtx,first_output,stream_entity.GetTxID()+MC_AST_SHORT_TXID_OFFSET,verbose,lpConditions,&last_output);
                if(entry.size())
                {
                    retArray.push_back(entry);                                
                }                    
                else
                {
                    last_output=-1;
                }
            }
        }
    }
    
    return retArray;
}

Value retrievestreamitems(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 2)
        throw runtime_error("Help message not found\n");
    
    pEF->ENT_RPCVerifyEdition("retrievestreamitems API");
    
    return pEF->STR_RPCRetrieveStreamItems(params);
}

Value purgestreamitems(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 2)
        throw runtime_error("Help message not found\n");
    
    pEF->ENT_RPCVerifyEdition("purgestreamitems API");
    
    return pEF->STR_RPCPurgeStreamItems(params);
}

Value purgepublisheditems(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error("Help message not found\n");
    
    pEF->ENT_RPCVerifyEdition("purgepublisheditems API");
    
    return pEF->STR_RPCPurgePublishedItems(params);
}
