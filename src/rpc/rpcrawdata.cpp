// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2014-2016 The Bitcoin Core developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "rpc/rpcutils.h"
#include "filters/multichainfilter.h"
#include "filters/filter.h"
#include "community/community.h"

#include "utils/util.h"
#include "json/json_spirit_ubjson.h"

#include <boost/assign/list_of.hpp>

using namespace std;
using namespace json_spirit;

void ParseFilterRestrictionsForField(Value param,mc_Script *lpDetailsScript,uint32_t filter_type);

uint32_t ParseRawDataParamType(Value *param,mc_EntityDetails *given_entity,mc_EntityDetails *entity,uint32_t *data_format,int *errorCode,string *strError)
{
    uint32_t param_type=MC_DATA_API_PARAM_TYPE_NONE;   
    uint32_t this_param_type;  
    bool missing_data=true;
    bool txfilter=false;
    *data_format=MC_SCR_DATA_FORMAT_UNKNOWN;
    entity->Zero();
    
    if(param->type() == obj_type)
    {
        BOOST_FOREACH(const Pair& d, param->get_obj()) 
        {
            this_param_type=MC_DATA_API_PARAM_TYPE_NONE;   
            if(d.name_ == "inputcache")
            {
                this_param_type=MC_DATA_API_PARAM_TYPE_CIS;
            }            
            if(d.name_ == "create")
            {
                if(d.value_.type() != null_type && !d.value_.get_str().empty())
                {
                    if(d.value_.get_str() == "stream")
                    {
                        this_param_type=MC_DATA_API_PARAM_TYPE_CREATE_STREAM;
                    }
                    if(d.value_.get_str() == "asset")
                    {
                        this_param_type=MC_DATA_API_PARAM_TYPE_ISSUE;
                    }
                    if(d.value_.get_str() == "upgrade")
                    {
                        this_param_type=MC_DATA_API_PARAM_TYPE_CREATE_UPGRADE;
                    }
                    if(d.value_.get_str() == "txfilter")
                    {
                        if( (param_type == MC_DATA_API_PARAM_TYPE_PUBLISH) ||
                            (param_type == MC_DATA_API_PARAM_TYPE_APPROVAL) ) 
                        {
                            *strError=string("'create' field should preceed 'for'");       
                            goto exitlbl;
                        }
                        this_param_type=MC_DATA_API_PARAM_TYPE_CREATE_FILTER;
                        txfilter=true;
                    }
                    if(d.value_.get_str() == "streamfilter")
                    {
                        this_param_type=MC_DATA_API_PARAM_TYPE_CREATE_FILTER;
                    }
                    if(d.value_.get_str() == "variable")
                    {
                        this_param_type=MC_DATA_API_PARAM_TYPE_CREATE_VAR;
                    }
                }
                if(this_param_type == MC_DATA_API_PARAM_TYPE_NONE)
                {
                    *strError=string("Invalid new entity type");                            
                    goto exitlbl;                        
                }
            }
            if(d.name_ == "update")
            {
                if(d.value_.type() != null_type && !d.value_.get_str().empty())
                {
                    ParseEntityIdentifier(d.value_,entity, MC_ENT_TYPE_ANY);       
                    if(entity->GetEntityType() == MC_ENT_TYPE_ASSET)
                    {
                        this_param_type=MC_DATA_API_PARAM_TYPE_FOLLOWON;                
                    }
                    if(entity->GetEntityType() == MC_ENT_TYPE_VARIABLE)
                    {
                        this_param_type=MC_DATA_API_PARAM_TYPE_UPDATE_VAR;                
                    }
                }
                if(this_param_type == MC_DATA_API_PARAM_TYPE_NONE)
                {
                    *strError=string("Asset or variable with this identifier not found");                            
                    *errorCode=RPC_ENTITY_NOT_FOUND;
                    goto exitlbl;                        
                    }
/*                
                if(entity->GetEntityType() != MC_ENT_TYPE_ASSET)
                {
                    *strError=string("Asset with this identifier not found");                                                           
                    *errorCode=RPC_ENTITY_NOT_FOUND;
                    goto exitlbl;                        
                }                
                this_param_type=MC_DATA_API_PARAM_TYPE_FOLLOWON;                
 */ 
            }
            if(d.name_ == "for")
            {
                if(txfilter)
                {
                    param_type=MC_DATA_API_PARAM_TYPE_NONE;           
                    this_param_type=MC_DATA_API_PARAM_TYPE_CREATE_FILTER;
                }
                else
                {
                    if(d.value_.type() != null_type && !d.value_.get_str().empty())
                    {
                        ParseEntityIdentifier(d.value_,entity, MC_ENT_TYPE_ANY);       
                        if(entity->GetEntityType() == MC_ENT_TYPE_STREAM)
                        {
                            this_param_type=MC_DATA_API_PARAM_TYPE_PUBLISH;                
                        }
                        if(entity->GetEntityType() == MC_ENT_TYPE_UPGRADE)
                        {
                            this_param_type=MC_DATA_API_PARAM_TYPE_APPROVAL;                
                        }
                    }                
                    if(this_param_type == MC_DATA_API_PARAM_TYPE_NONE)
                    {
                        *strError=string("Entity with this identifier not found");                            
                        *errorCode=RPC_ENTITY_NOT_FOUND;
                        goto exitlbl;                        
                    }
                }
            }
            if( (d.name_ == "text") || (d.name_ == "json")  || (d.name_ == "cache") )
            {
                if( mc_gState->m_Features->FormattedData() == 0 )
                {
                    *errorCode=RPC_NOT_SUPPORTED;
                    *strError=string("Formatted data is not supported by this protocol version");       
                    goto exitlbl;
                }
                if(!missing_data)
                {
                    *strError=string("data field can appear only once in the object");                                                                                        
                    goto exitlbl;                    
                }
                missing_data=false;
            }
            if(this_param_type != MC_DATA_API_PARAM_TYPE_NONE)
            {
                if(param_type != MC_DATA_API_PARAM_TYPE_NONE)
                {                
                    *strError=string("Only one of the following keywords can appear in the object: create, update, for, json, text");                                                                                        
                    goto exitlbl;
                }
            }
            if(this_param_type != MC_DATA_API_PARAM_TYPE_NONE)
            {
                param_type=this_param_type;
            }
        }    
        if(param_type == MC_DATA_API_PARAM_TYPE_NONE)
        {                
//            if(*data_format != MC_SCR_DATA_FORMAT_UNKNOWN)
            if(!missing_data)
            {
                param_type=MC_DATA_API_PARAM_TYPE_FORMATTED;                                    
            }
        }

/*        
        if(param_type == MC_DATA_API_PARAM_TYPE_NONE)
        {                
            if(given_entity && given_entity->GetEntityType())
            {
                memcpy(entity,given_entity,sizeof(mc_EntityDetails));
                param_type=MC_DATA_API_PARAM_TYPE_FOLLOWON;                
            }
            else
            {
                param_type=MC_DATA_API_PARAM_TYPE_ISSUE; 
            }            
        }
*/
/*        
        if(param_type == MC_DATA_API_PARAM_TYPE_FOLLOWON)
        {
            if(entity->AllowedFollowOns() == 0)
            {
                *errorCode=RPC_NOT_ALLOWED;
                *strError=string("Issuing more units not allowed for this asset");       
                goto exitlbl;                
            }
        }
 */ 
    }
    else
    {
        if(param->type() == str_type)
        {
            if(param->get_str().size())
            {
                param_type=MC_DATA_API_PARAM_TYPE_RAW;
            }
            else
            {
                param_type=MC_DATA_API_PARAM_TYPE_EMPTY_RAW;                
            }                    
        }
        else
        {
            *strError="Invalid parameter type, should be object or string";
            goto exitlbl;
        }        
    }
    
exitlbl:
    return param_type;
}

CScript RawDataScriptRawHex(Value *param,int *errorCode,string *strError)
{
    bool fIsHex;
    CScript scriptOpReturn=CScript();
    vector<unsigned char> dataData(ParseHex(param->get_str().c_str(),fIsHex));    
    if(!fIsHex)
    {
        *strError="data should be hexadecimal string";
        if(mc_gState->m_Features->FormattedData())
        {
            *strError+=" or recognized object";
        }
    }
    scriptOpReturn << OP_RETURN << dataData;
    return scriptOpReturn;
}

vector<unsigned char> ParseRawFormattedData(const Value *value,uint32_t *data_format,mc_Script *lpDetailsScript,uint32_t in_options,uint32_t* out_options,int *errorCode,string *strError)
{
    if(out_options)
    {
        *out_options=MC_RFD_OPTION_NONE;
    }
    vector<unsigned char> vValue;
    if(value->type() == str_type)
    {
        bool fIsHex;
        vValue=ParseHex(value->get_str().c_str(),fIsHex);    
        if(!fIsHex)
        {
            *strError=string("data should be hexadecimal string");                            
            if(mc_gState->m_Features->FormattedData())
            {
                *strError+=" or recognized object";
            }
        }        
        *data_format=MC_SCR_DATA_FORMAT_UNKNOWN;
    }
    else
    {
        if( (in_options & MC_RFD_OPTION_INLINE) || 
            (mc_gState->m_Features->FormattedData() != 0) || 
            (mc_gState->m_Features->OffChainData() != 0) )
        {
            if(value->type() == obj_type) 
            {
                if(value->get_obj().size() != 1)
                {
                    *strError=string("data should be object with single element");                                                        
                }
                else
                {
                    BOOST_FOREACH(const Pair& d, value->get_obj()) 
                    {
                        if(d.name_ == "text")
                        {
                            if(d.value_.type() == str_type)
                            {
                                vValue=vector<unsigned char> (d.value_.get_str().begin(),d.value_.get_str().end());    
                            }
                            else
                            {
                                *strError=string("value in data object should be string");                            
                            }
                            *data_format=MC_SCR_DATA_FORMAT_UTF8;                    
                        }
                        if(d.name_ == "json")
                        {
                            size_t bytes;
                            int err;
                            const unsigned char *script;
                            lpDetailsScript->Clear();
                            lpDetailsScript->AddElement();
                            if((err = ubjson_write(d.value_,lpDetailsScript,MAX_FORMATTED_DATA_DEPTH)) != MC_ERR_NOERROR)
                            {
                                *strError=string("Couldn't transfer JSON object to internal UBJSON format");    
                            }
                            script = lpDetailsScript->GetData(0,&bytes);
                            vValue=vector<unsigned char> (script,script+bytes);                                            
                            *data_format=MC_SCR_DATA_FORMAT_UBJSON;                    
                        }
                        if(d.name_ == "cache")
                        {
                            if(d.value_.type() == str_type)
                            {
                                vValue=vector<unsigned char> (d.value_.get_str().begin(),d.value_.get_str().end());  
                                vValue.push_back(0);
                                if(in_options & MC_RFD_OPTION_OFFCHAIN)
                                {
                                    if(out_options)
                                    {
                                        *out_options |= MC_RFD_OPTION_CACHE;
                                    }    
                                }
                                else
                                {
                                    int fHan=mc_BinaryCacheFile((char*)&vValue[0],0);
                                    if(fHan <= 0)
                                    {
                                        *strError="Binary cache item with this identifier not found";
                                    }
                                    int64_t total_size=0;
                                    if(strError->size() == 0)
                                    {
                                        total_size=lseek64(fHan,0,SEEK_END);
                                        if(lseek64(fHan,0,SEEK_SET) != 0)
                                        {
                                            *strError="Cannot read binary cache item";
                                            *errorCode=RPC_INTERNAL_ERROR;
                                            close(fHan);
                                        }
                                    }
                                    if(strError->size() == 0)
                                    {
                                        if(total_size > MAX_OP_RETURN_RELAY)
                                        {
                                            *strError="Binary cache item too big";
                                            *errorCode=RPC_NOT_SUPPORTED;
                                            close(fHan);                                        
                                        }
                                    }
                                    if(strError->size() == 0)
                                    {
                                        if(total_size)
                                        {
                                            mc_gState->m_TmpBuffers->m_RpcChunkScript1->Clear();
                                            mc_gState->m_TmpBuffers->m_RpcChunkScript1->Resize(total_size,1);
                                            unsigned char* ptr=mc_gState->m_TmpBuffers->m_RpcChunkScript1->m_lpData;
                                            if(read(fHan,ptr,total_size) != total_size)
                                            {
                                                *errorCode=RPC_INTERNAL_ERROR;
                                                *strError="Cannot read binary cache item";
                                            }
                                            close(fHan);
                                            vValue=vector<unsigned char> (ptr,ptr+total_size);                                              
                                        }
                                        else
                                        {
                                            vValue.clear();
                                        }
                                    }
                                }
                            }
                            else
                            {
                                *strError=string("cache identifier in data object should be string");                            
                            }
                            *data_format=MC_SCR_DATA_FORMAT_UNKNOWN;                                                
                        }
                        else
                        {    
                            if(d.name_ == "chunks")
                            {
                                if(mc_gState->m_Features->OffChainData())
                                {
                                    if(d.value_.type() == array_type)
                                    {
                                        Array arr=d.value_.get_array();
                                        for(int i=0;i<(int)arr.size();i++)
                                        {
                                            if(strError->size() == 0)
                                            {
                                                if(arr[i].type() == str_type)
                                                {
                                                    vector<unsigned char> vHash;
                                                    bool fIsHex;
                                                    vHash=ParseHex(arr[i].get_str().c_str(),fIsHex);    
                                                    if(!fIsHex)
                                                    {
                                                        *strError=string("Chunk hash should be hexadecimal string");                            
                                                    }
                                                    else
                                                    {
                                                        if(vHash.size() != MC_CDB_CHUNK_HASH_SIZE)
                                                        {
                                                            *strError=strprintf("Chunk hash should be %d bytes long",MC_CDB_CHUNK_HASH_SIZE);                                                                                    
                                                        }
                                                        else
                                                        {                                                        
                                                            uint256 hash;
                                                            hash.SetHex(arr[i].get_str());

                                                            vValue.insert(vValue.end(),(unsigned char*)&hash,(unsigned char*)&hash+MC_CDB_CHUNK_HASH_SIZE);
                                                        }
                                                    }                                                
                                                }                                            
                                            }
                                        }
                                    }
                                    else
                                    {
                                        *strError=string("value in data object should be array");                            
                                    }

                                    if(out_options)
                                    {
                                        *out_options |= MC_RFD_OPTION_OFFCHAIN;
                                    }    
                                    *data_format=MC_SCR_DATA_FORMAT_UNKNOWN;
                                }
                                else
                                {
                                    *errorCode=RPC_NOT_SUPPORTED;
                                    *strError="Unsupported item data type: " + d.name_;
                                }
                            }
                            else
                            {
                                if(*data_format == MC_SCR_DATA_FORMAT_UNKNOWN)
                                {
                                    *errorCode=RPC_NOT_SUPPORTED;
                                    *strError="Unsupported item data type: " + d.name_;
                                }                    
                            }
                        }
                    }                
                }
            }   
            else
            {
                *strError=string("data should be hexadecimal string or recognized object");                                        
            }
        }
        else
        {
            *strError=string("data should be hexadecimal string");                                                    
            if(mc_gState->m_Features->FormattedData() == 0)
            {
                *strError+=" for this protocol version";
            }
        }
    }
    
    return vValue;
}

void ParseRawDetails(const Value *value,mc_Script *lpDetails,mc_Script *lpDetailsScript,int *errorCode,string *strError)
{
    if(value->type() == obj_type)
    {
        size_t bytes;
        int err;
        const unsigned char *script;
        lpDetailsScript->Clear();
        lpDetailsScript->AddElement();
        if((err = ubjson_write(*value,lpDetailsScript,MAX_FORMATTED_DATA_DEPTH)) != MC_ERR_NOERROR)
        {
            *strError=string("Couldn't transfer details JSON object to internal UBJSON format");    
        }
        else
        {
            script = lpDetailsScript->GetData(0,&bytes);
            lpDetails->SetSpecialParamValue(MC_ENT_SPRM_JSON_DETAILS,script,bytes);            
        }

/*        
        BOOST_FOREACH(const Pair& p, value->get_obj()) 
        {              
            if(p.value_.type() == str_type)
            {
                lpDetails->SetParamValue(p.name_.c_str(),p.name_.size(),(unsigned char*)p.value_.get_str().c_str(),p.value_.get_str().size());                
            }
            else
            {
                *strError=string("Invalid details value, should be string");                                                                            
            }      
        }
*/ 
    }                
    else
    {
        *strError=string("Invalid details");                                                            
    }    
}

void ParseRawValue(const Value *value,mc_Script *lpDetails,mc_Script *lpDetailsScript,size_t *max_size,int *errorCode,string *strError)
{
    size_t bytes;
    int err;
    const unsigned char *script;
    lpDetailsScript->Clear();
    lpDetailsScript->AddElement();
    if((err = ubjson_write(*value,lpDetailsScript,MAX_FORMATTED_DATA_DEPTH)) != MC_ERR_NOERROR)
    {
        *strError=string("Couldn't transfer value JSON  to internal UBJSON format");    
    }
    else
    {
        script = lpDetailsScript->GetData(0,&bytes);
        if(max_size)
        {
            if(bytes > *max_size)
            {
                *max_size=bytes;
                return;
            }
        }
        lpDetails->SetSpecialParamValue(MC_ENT_SPRM_JSON_VALUE,script,bytes);            
    }
}

CScript RawDataScriptFormatted(Value *param,uint32_t *data_format,mc_Script *lpDetailsScript,int *errorCode,string *strError)
{
    CScript scriptOpReturn=CScript();
    vector<unsigned char> vValue;
    size_t bytes;
    const unsigned char *script;
    bool field_parsed;
    bool missing_data=true;
    BOOST_FOREACH(const Pair& d, param->get_obj()) 
    {
        field_parsed=false;
        if( (d.name_ == "text") || (d.name_ == "json")  || (d.name_ == "cache") )      
        {
            if(!missing_data)
            {
                *strError=string("data object should have single key - json or text");                                                                                                        
            }
            vValue=ParseRawFormattedData(param,data_format,lpDetailsScript,MC_RFD_OPTION_NONE,NULL,errorCode,strError);
            field_parsed=true;
            missing_data=false;
        }
//        if(d.name_ == "format")field_parsed=true;
        if(!field_parsed)
        {
            *strError=strprintf("Invalid field: %s",d.name_.c_str());;                                
        }
    }    
    
    if(missing_data)
    {
        *strError=string("Missing json or text field");            
    }

    if(strError->size() == 0)
    {
        lpDetailsScript->Clear();
        lpDetailsScript->SetDataFormat(*data_format);
        script = lpDetailsScript->GetData(0,&bytes);
        scriptOpReturn << vector<unsigned char>(script, script + bytes) << OP_DROP;                    

        scriptOpReturn << OP_RETURN << vValue;                        
    }
    
    return scriptOpReturn;
}

CScript RawDataScriptIssue(Value *param,mc_Script *lpDetails,mc_Script *lpDetailsScript,int *errorCode,string *strError)
{
    CScript scriptOpReturn=CScript();
    bool field_parsed;
    size_t bytes;
    int err;
    const unsigned char *script;
    string entity_name;
    int multiple=1;
    int is_open=0;
    uint32_t permissions=0;
    bool missing_name=true;
    bool missing_multiple=true;
    bool missing_open=true;
    bool missing_details=true;
    
    lpDetails->Clear();
    lpDetails->AddElement();                   
    
    BOOST_FOREACH(const Pair& d, param->get_obj()) 
    {
        field_parsed=false;
        if(d.name_ == "name")
        {
            if(!missing_name)
            {
                *strError=string("name field can appear only once in the object");                                                                                                        
            }
            if(d.value_.type() != null_type && !d.value_.get_str().empty())
            {
                entity_name=d.value_.get_str();
                                
                if(entity_name == "*")
                {
                    *strError=string("Invalid asset name"); 
                }
                
                if(entity_name.size())
                {
                    if(entity_name.size() > MC_ENT_MAX_NAME_SIZE)
                    {
                        *strError=string("Invalid asset name - too long"); 
                    }
                    lpDetails->SetSpecialParamValue(MC_ENT_SPRM_NAME,(const unsigned char*)(entity_name.c_str()),entity_name.size());
                }
            }
            else
            {
                *strError=string("Invalid name");                            
            }
            missing_name=false;
            field_parsed=true;
        }
        if(d.name_ == "multiple")
        {
            if(!missing_multiple)
            {
                *strError=string("multiple field can appear only once in the object");                                                                                                        
            }
            if(d.value_.type() == int_type)
            {
                multiple=d.value_.get_int();
                if(multiple <= 0)
                {
                    *strError=string("Invalid multiple - should be positive");                                                                                                        
                }
                else
                {
                    lpDetails->SetSpecialParamValue(MC_ENT_SPRM_ASSET_MULTIPLE,(unsigned char*)&multiple,4);
                }
            }
            else
            {
                *strError=string("Invalid multiple");                            
            }
            missing_multiple=false;
            field_parsed=true;
        }
        if(d.name_ == "open")
        {
            if(!missing_open)
            {
                *strError=string("open field can appear only once in the object");                                                                                                        
            }
            if(d.value_.type() == bool_type)
            {
                is_open=d.value_.get_bool();
            }    
            else
            {
                *strError=string("Invalid open");                                            
            }
            lpDetails->SetSpecialParamValue(MC_ENT_SPRM_FOLLOW_ONS,(unsigned char*)&is_open,1);                
            missing_open=false;
            field_parsed=true;
        }
        if(d.name_ == "restrict")
        {
            if(mc_gState->m_Features->PerAssetPermissions() == 0)
            {
                throw JSONRPCError(RPC_NOT_SUPPORTED, "Per-asset permissions not supported for this protocol version");   
            }
            if(permissions == 0)
            {
                if(d.value_.type() == str_type)
                {
                    permissions=mc_gState->m_Permissions->GetPermissionType(d.value_.get_str().c_str(),MC_PTP_SEND | MC_PTP_RECEIVE);
                    if(permissions == 0)
                    {
                        *strError=string("Invalid restrict");                                                                
                    }
                }
                else
                {
                    *strError=string("Invalid restrict");                                                                
                }
            }
            else
            {
                *strError=string("restrict field can appear only once in the object");                                                                                                                        
            }
            if(permissions)
            {
                lpDetails->SetSpecialParamValue(MC_ENT_SPRM_PERMISSIONS,(unsigned char*)&permissions,1);                                
            }
            field_parsed=true;
        }
        
        if(d.name_ == "details")
        {
            if(!missing_details)
            {
                *strError=string("details field can appear only once in the object");                                                                                                        
            }
            ParseRawDetails(&(d.value_),lpDetails,lpDetailsScript,errorCode,strError);
            missing_details=false;
            field_parsed=true;
        }            
        if(d.name_ == "create")field_parsed=true;
        if(!field_parsed)
        {
            *strError=strprintf("Invalid field: %s",d.name_.c_str());
        }
    }    
    
    if(strError->size() == 0)
    {
        lpDetailsScript->Clear();
        script=lpDetails->GetData(0,&bytes);
        err=lpDetailsScript->SetNewEntityType(MC_ENT_TYPE_ASSET,0,script,bytes);
        if(err)
        {
            *strError=string("Invalid custom fields, too long");                                                            
        }
        else
        {
            script = lpDetailsScript->GetData(0,&bytes);
            scriptOpReturn << vector<unsigned char>(script, script + bytes) << OP_DROP << OP_RETURN;
        }
    }
    
    return scriptOpReturn;
}

CScript RawDataScriptFollowOn(Value *param,mc_EntityDetails *entity,mc_Script *lpDetails,mc_Script *lpDetailsScript,int *errorCode,string *strError)
{
    CScript scriptOpReturn=CScript();
    size_t bytes;
    int err;
    const unsigned char *script;
    bool field_parsed;
    bool missing_details=true;
    
    lpDetails->Clear();
    lpDetails->AddElement();                   
    
    BOOST_FOREACH(const Pair& d, param->get_obj()) 
    {
        field_parsed=false;
        if(d.name_ == "details")
        {
            if(!missing_details)
            {
                *strError=string("details field can appear only once in the object");                                                                                                        
            }
            ParseRawDetails(&(d.value_),lpDetails,lpDetailsScript,errorCode,strError);
            missing_details=false;
            field_parsed=true;
        }            
        if(d.name_ == "update")field_parsed=true;
        if(!field_parsed)
        {
            *strError=strprintf("Invalid field: %s",d.name_.c_str());;                                
        }
    }    
    
    lpDetailsScript->Clear();
    lpDetailsScript->SetEntity(entity->GetTxID()+MC_AST_SHORT_TXID_OFFSET);
    script = lpDetailsScript->GetData(0,&bytes);
    scriptOpReturn << vector<unsigned char>(script, script + bytes) << OP_DROP;

    lpDetailsScript->Clear();
    script=lpDetails->GetData(0,&bytes);
    err=lpDetailsScript->SetNewEntityType(MC_ENT_TYPE_ASSET,1,script,bytes);
    if(err)
    {
        *strError=string("Invalid custom fields, too long");                                                            
    }
    else
    {
        script = lpDetailsScript->GetData(0,&bytes);
        scriptOpReturn << vector<unsigned char>(script, script + bytes) << OP_DROP << OP_RETURN;
    }
    
    return scriptOpReturn;
}

bool RawDataParseRestrictParameter(const Value& param,uint32_t *restrict,uint32_t *permissions,int *errorCode,string *strError)
{
    *restrict=0;
    *permissions=0;
 
    uint32_t match;
    char* ptr;
    char* start;
    char* ptrEnd;
    char c;
    
    if(param.type() != str_type)
    {
        *strError="Invalid restrict field, should be string";
        return false;
    }
    
    ptr=(char*)param.get_str().c_str();
    ptrEnd=ptr+strlen(ptr);
    start=ptr;
    
    while(ptr<=ptrEnd)
    {
        c=*ptr;
        if( (c == ',') || (c ==0x00))
        {
            if(ptr > start)
            {
                match=0;
                if(( (ptr-start) ==  5) && (memcmp(start,"write",    ptr-start) == 0) ){match = 1; *permissions |= MC_PTP_WRITE ;}
                if(( (ptr-start) ==  4) && (memcmp(start,"read",     ptr-start) == 0) )if(mc_gState->m_Features->ReadPermissions()){match = 1; *permissions |= MC_PTP_READ ;}
                if(( (ptr-start) ==  7) && (memcmp(start,"onchain",  ptr-start) == 0) ){match = 1; *restrict |= MC_ENT_ENTITY_RESTRICTION_ONCHAIN;}
                if(( (ptr-start) ==  8) && (memcmp(start,"offchain", ptr-start) == 0) ){match = 1; *restrict |= MC_ENT_ENTITY_RESTRICTION_OFFCHAIN;}
                
                if(match == 0)
                {
                    *strError="Unsupported restriction";
                    *errorCode=RPC_NOT_SUPPORTED;
                    return false;
                }
                start=ptr+1;
            }
        }
        ptr++;
    }
    
    return true;    
}

CScript RawDataScriptCreateStream(Value *param,mc_Script *lpDetails,mc_Script *lpDetailsScript,int *errorCode,string *strError)
{
    CScript scriptOpReturn=CScript();
    bool field_parsed;
    size_t bytes;
    int err;
    const unsigned char *script;
    string entity_name;
    int is_open=0;
    int is_salted=0;
    uint32_t restrict=0;
    uint32_t permissions=MC_PTP_WRITE;
    
    bool missing_name=true;
    bool missing_open=true;
    bool missing_details=true;
    bool missing_salted=true;
    
    lpDetails->Clear();
    lpDetails->AddElement();                   
       
    BOOST_FOREACH(const Pair& d, param->get_obj()) 
    {
        field_parsed=false;
        if(d.name_ == "name")
        {
            if(!missing_name)
            {
                *strError=string("name field can appear only once in the object");                                                                                                        
            }
            if(d.value_.type() != null_type && !d.value_.get_str().empty())
            {
                entity_name=d.value_.get_str();
                if(entity_name.size())
                {
                    if(entity_name.size() > MC_ENT_MAX_NAME_SIZE)
                    {
                        *strError=string("Invalid stream name - too long"); 
                    }
                    lpDetails->SetSpecialParamValue(MC_ENT_SPRM_NAME,(const unsigned char*)(entity_name.c_str()),entity_name.size());
                }
            }
            else
            {
                *strError=string("Invalid name");                            
            }
            missing_name=false;
            field_parsed=true;
        }
        if(d.name_ == "open")
        {
            if(!missing_open)
            {
                *strError=string("open/restrict field can appear only once in the object");                                                                                                        
            }
            if(d.value_.type() == bool_type)
            {
                is_open=d.value_.get_bool();
            }    
            else
            {
                *strError=string("Invalid open");                                            
            }
            if(mc_gState->m_Features->OffChainData() == 0)
            {
                lpDetails->SetSpecialParamValue(MC_ENT_SPRM_ANYONE_CAN_WRITE,(unsigned char*)&is_open,1); 
            }
            else
            {
                permissions=is_open ? MC_PTP_NONE : MC_PTP_WRITE;
            }
            missing_open=false;
            field_parsed=true;
        }
        if(d.name_ == "salted")
        {
            if(mc_gState->m_Features->SaltedChunks() == 0)
            {
                *strError=string("Salted chunks not supported for this protocol version");               
                *errorCode=RPC_NOT_SUPPORTED;
            }
            else
            {                
                if(!missing_salted)
                {
                    *strError=string("salted field can appear only once in the object");                                                                                                        
                }
                if(d.value_.type() == bool_type)
                {
                    is_salted=d.value_.get_bool();
                }    
                else
                {
                    *strError=string("Invalid salted");                                            
                }
                missing_salted=false;
                field_parsed=true;
            }
        }
        if(d.name_ == "restrict")
        {
            if(mc_gState->m_Features->OffChainData() == 0)
            {
                *strError=string("Per-stream restrictions not supported for this protocol version");               
                *errorCode=RPC_NOT_SUPPORTED;
            }
            else
            {                
                if(!missing_open)
                {
                    *strError=string("open/restrict field can appear only once in the object");                                                                                                        
                }
                RawDataParseRestrictParameter(d.value_,&restrict,&permissions,errorCode,strError);
/*                
                if(RawDataParseRestrictParameter(d.value_,&restrict,&permissions,errorCode,strError))
                {
                    if(restrict & MC_ENT_ENTITY_RESTRICTION_OFFCHAIN)
                    {
                        if(restrict & MC_ENT_ENTITY_RESTRICTION_ONCHAIN)
                        {
                            *strError=string("Stream cannot be restricted from both onchain and offchain items");               
                            *errorCode=RPC_NOT_SUPPORTED;                            
                        }                        
                    }
                }
 */ 
                missing_open=false;
                field_parsed=true;
            }
        }
        if(d.name_ == "details")
        {
            if(!missing_details)
            {
                *strError=string("details field can appear only once in the object");                                                                                                        
            }
            ParseRawDetails(&(d.value_),lpDetails,lpDetailsScript,errorCode,strError);
            missing_details=false;
            field_parsed=true;
        }            
        if(d.name_ == "create")field_parsed=true;
        if(!field_parsed)
        {
            *strError=strprintf("Invalid field: %s",d.name_.c_str());
        }
    }    
    
    if(missing_salted)
    {
        if(permissions & MC_PTP_READ)
        {
            is_salted=true;
        }
    }
    
    if(mc_gState->m_Features->OffChainData())
    {
        lpDetails->SetSpecialParamValue(MC_ENT_SPRM_PERMISSIONS,(unsigned char*)&permissions,1);                                
    }
    if(is_salted)
    {
        restrict |= MC_ENT_ENTITY_RESTRICTION_NEED_SALTED;
    }
    if(strError->size() == 0)
    {
        if(permissions & MC_PTP_READ)
        {
            restrict |= MC_ENT_ENTITY_RESTRICTION_ONCHAIN;
/*            
            if( (restrict & MC_ENT_ENTITY_RESTRICTION_ONCHAIN ) == 0 )
            {
                *strError="onchain restriction should be set for read-permissioned streams";
                *errorCode=RPC_NOT_ALLOWED;
            }
 */ 
        }        
    }
    
    if(restrict & MC_ENT_ENTITY_RESTRICTION_OFFCHAIN)
    {
        if(restrict & MC_ENT_ENTITY_RESTRICTION_ONCHAIN)
        {
            *strError=string("Stream cannot be restricted from both onchain and offchain items");               
            *errorCode=RPC_NOT_SUPPORTED;                            
        }                        
    }
    
    if( restrict != 0 )
    {
        lpDetails->SetSpecialParamValue(MC_ENT_SPRM_RESTRICTIONS,(unsigned char*)&restrict,1);                         
    }
    
    if(strError->size() == 0)
    {
        lpDetailsScript->Clear();
        script=lpDetails->GetData(0,&bytes);
        err=lpDetailsScript->SetNewEntityType(MC_ENT_TYPE_STREAM,0,script,bytes);
        if(err)
        {
            *strError=string("Invalid custom fields, too long");                                                            
        }
        else
        {
            script = lpDetailsScript->GetData(0,&bytes);
            scriptOpReturn << vector<unsigned char>(script, script + bytes) << OP_DROP << OP_RETURN;
        }
    }
    
    return scriptOpReturn;
}

bool AddParamNameValueToScript(const string  param_name,const Value param_value,mc_Script *lpDetailsScript,int version,int *errorCode,string *strError)
{
    
    int64_t value;
    string name=param_name;   
    name.erase(std::remove(name.begin(), name.end(), '-'), name.end());
    const mc_OneMultichainParam *param=mc_gState->m_NetworkParams->FindParam(name.c_str());
            
    if(param == NULL)
    {
        *errorCode=RPC_INVALID_PARAMETER;
        *strError=string("Invalid parameter name"); 
        return false;                    
    }        
    
    int size;
    unsigned char zero=0;
    switch(param->m_Type & MC_PRM_DATA_TYPE_MASK)
    {
        case MC_PRM_BOOLEAN:
            if(param_value.type() == bool_type)
            {
                value=param_value.get_bool() ? 1 : 0;
            }
            else
            {
                *errorCode=RPC_INVALID_PARAMETER;
                *strError=string("Invalid parameter type, should be boolean");     
                return false;            
            }                
            break;
        case MC_PRM_INT32:
        case MC_PRM_INT64:
        case MC_PRM_UINT32:
            if(param->m_Type & MC_PRM_DECIMAL)            
            {
                if(param_value.type() == real_type)
                {
                    value=mc_gState->m_NetworkParams->DecimalToInt64(param_value.get_real());
                }                
                else
                {
                    *errorCode=RPC_INVALID_PARAMETER;
                    *strError=string("Invalid parameter type, should be numeric");     
                    return false;                            
                }
            }
            else
            {
                if(param_value.type() == int_type)
                {
                    value=param_value.get_int64();
                }
                else
                {
                    *errorCode=RPC_INVALID_PARAMETER;
                    *strError=string("Invalid parameter type, should be integer");     
                    return false;                            
                }
            }
            break;    
        default:
            *errorCode=RPC_NOT_SUPPORTED;
            *strError=string("One of parameters cannot be upgraded by this protocol version"); 
            return false;                            
    }
        
    size=mc_gState->m_NetworkParams->CanBeUpgradedByVersion(name.c_str(),version,0);
    
    if(size < 0)
    {
        *errorCode=RPC_INVALID_PARAMETER;
        *strError=string("Invalid parameter name"); 
        return false;        
    }
    
    if(size == 0)
    {
        *errorCode=RPC_NOT_SUPPORTED;
        *strError=string("One of parameters cannot be upgraded by this protocol version"); 
        return false;                
    }
    
    lpDetailsScript->SetData((unsigned char*)name.c_str(),name.size());
    lpDetailsScript->SetData((unsigned char*)&zero,1);
    lpDetailsScript->SetData((unsigned char*)&size,MC_PRM_PARAM_SIZE_BYTES);
    lpDetailsScript->SetData((unsigned char*)&value,size);
    
    return true;
}


CScript RawDataScriptCreateUpgrade(Value *param,mc_Script *lpDetails,mc_Script *lpDetailsScript,int *errorCode,string *strError)
{
    CScript scriptOpReturn=CScript();
    bool field_parsed;
    size_t bytes;
    const unsigned char *script;
    string entity_name;
    int protocol_version;
    uint32_t startblock;

    bool missing_name=true;
    bool missing_startblock=true;
    bool missing_details=true;
    
    lpDetails->Clear();
    lpDetails->AddElement();                   

    lpDetailsScript->Clear();
    lpDetailsScript->AddElement();                   
    
    protocol_version=-1;
    
    BOOST_FOREACH(const Pair& d, param->get_obj()) 
    {
        field_parsed=false;
        if(d.name_ == "name")
        {
            if(!missing_name)
            {
                *strError=string("name field can appear only once in the object");                                                                                                        
            }
            if(d.value_.type() != null_type && !d.value_.get_str().empty())
            {
                entity_name=d.value_.get_str();
                if(entity_name.size())
                {
                    if(entity_name.size() > MC_ENT_MAX_NAME_SIZE)
                    {
                        *strError=string("Invalid upgrade name - too long"); 
                    }
                    lpDetails->SetSpecialParamValue(MC_ENT_SPRM_NAME,(const unsigned char*)(entity_name.c_str()),entity_name.size());
                }
            }
            else
            {
                *strError=string("Invalid name");                            
            }
            missing_name=false;
            field_parsed=true;
        }
        if(d.name_ == "startblock")
        {
            if(!missing_startblock)
            {
                *strError=string("startblock field can appear only once in the object");                                                                                                        
            }
            if(d.value_.type() == int_type)
            {
                if( (d.value_.get_int64() >= 0) && (d.value_.get_int64() <= 0xFFFFFFFF) )
                {
                    startblock=(uint32_t)(d.value_.get_int64());
                    if(startblock > 0)
                    {
                        lpDetails->SetSpecialParamValue(MC_ENT_SPRM_UPGRADE_START_BLOCK,(unsigned char*)&startblock,4);        
                    }                    
                }
                else
                {
                    *strError=string("Invalid startblock");                                                    
                }
            }
            else
            {
                *strError=string("Invalid startblock");                            
            }
            missing_startblock=false;
            field_parsed=true;
        }
        if(d.name_ == "details")
        {
            if(!missing_details)
            {                    
                *strError=string("details field can appear only once in the object");                                                                                                                        
            }
            if(d.value_.type() == obj_type)
            {
                protocol_version=-1;
                BOOST_FOREACH(const Pair& p, d.value_.get_obj()) 
                {              
                    if(p.name_ == "protocol-version")
                    {
                        if( (p.value_.type() == int_type) && (p.value_.get_int() > 0) )
                        {
                            if(protocol_version < 0)
                            {
                                protocol_version=p.value_.get_int();
                            }
                        }                            
                        else
                        {
                            *strError=string("Invalid protocol-version");                                                                                                            
                        }
                    }
                    else
                    {
                        if(mc_gState->m_Features->ParameterUpgrades())
                        {                        
                            AddParamNameValueToScript(p.name_,p.value_,lpDetailsScript,0,errorCode,strError);
                        }
                        else
                        {
                            *strError=string("Invalid details");     
                        }
                    }
                }
                
                script = lpDetailsScript->GetData(0,&bytes);
                if(strError->size() == 0)
                {                    
                    if( (protocol_version <= 0) && (bytes == 0) )
                    {
                        *strError=string("Missing protocol-version");                                                                                                            
                    }
                }
                                
                if(strError->size() == 0)
                {
                    if(protocol_version > 0)
                    {                    
                        lpDetails->SetSpecialParamValue(MC_ENT_SPRM_UPGRADE_PROTOCOL_VERSION,(unsigned char*)&protocol_version,4);                                
                    }
                    if(bytes)
                    {
                        lpDetails->SetSpecialParamValue(MC_ENT_SPRM_UPGRADE_CHAIN_PARAMS,script,bytes);                                                        
                    }
                }
            }             
            missing_details=false;
            field_parsed=true;
        }            
        if(d.name_ == "create")field_parsed=true;
        if(!field_parsed)
        {
            *strError=strprintf("Invalid field: %s",d.name_.c_str());
        }
    }    
    
    if(strError->size() == 0)
    {
        if(missing_details)
        {                    
            *strError=string("Missing details");                                                                                            
        }
    }
    
    if(strError->size() == 0)
    {
        int err;
        script=lpDetails->GetData(0,&bytes);
        lpDetailsScript->Clear();
        err=lpDetailsScript->SetNewEntityType(MC_ENT_TYPE_UPGRADE,0,script,bytes);
        if(err)
        {
            *strError=string("Invalid custom fields, too long");                                                            
        }
        else
        {
            script = lpDetailsScript->GetData(0,&bytes);
            scriptOpReturn << vector<unsigned char>(script, script + bytes) << OP_DROP << OP_RETURN;
        }        
    }
    
    return scriptOpReturn;
}

bool mc_JSInExtendedScript(size_t size)
{
    if(size > 32768)
    {
        if(mc_gState->m_Features->ExtendedEntityDetails())
        {
            return true;
        }
    }
    return false;
}

CScript RawDataScriptCreateFilter(Value *param,mc_Script *lpDetails,mc_Script *lpDetailsScript,int *errorCode,string *strError)
{
    CScript scriptOpReturn=CScript();
    bool field_parsed;
    size_t bytes;
    const unsigned char *script;
    string entity_name,filter_code,filter_main_name;
    uint32_t filter_type=MC_FLT_TYPE_TX;
    string js;
    bool js_extended=false;

    bool missing_name=true;
    bool missing_code=true;
    bool missing_for=true;
    
    lpDetails->Clear();
    lpDetails->AddElement();                   

    lpDetailsScript->Clear();
    lpDetailsScript->AddElement();                   
        
    BOOST_FOREACH(const Pair& d, param->get_obj()) 
    {
        field_parsed=false;
        if(d.name_ == "name")
        {
            if(!missing_name)
            {
                *strError=string("name field can appear only once in the object");                                                                                                        
            }
            if(d.value_.type() != null_type && !d.value_.get_str().empty())
            {
                entity_name=d.value_.get_str();
                if(entity_name.size())
                {
                    if(entity_name.size() > MC_ENT_MAX_NAME_SIZE)
                    {
                        *strError=string("Invalid filter name - too long"); 
                    }
                    lpDetails->SetSpecialParamValue(MC_ENT_SPRM_NAME,(const unsigned char*)(entity_name.c_str()),entity_name.size());
                }
            }
            else
            {
                *strError=string("Invalid name");                            
            }
            missing_name=false;
            field_parsed=true;
        }
        if(d.name_ == "for")
        {
            if(!missing_for)
            {
                *strError=string("for field can appear only once in the object");                                                                                                        
            }
             
            ParseFilterRestrictionsForField(d.value_,lpDetailsScript,MC_FLT_TYPE_TX);
                        
            script = lpDetailsScript->GetData(0,&bytes);

            if(bytes)
            {
                lpDetails->SetSpecialParamValue(MC_ENT_SPRM_FILTER_RESTRICTIONS,script,bytes);
            }
            
            missing_for=false;
            field_parsed=true;
        }
        
        if(d.name_ == "code")
        {
            if(!missing_code)
            {
                *strError=string("code field can appear only once in the object");                                                                                                        
            }
            if(d.value_.type() == str_type)
            {
                js_extended=mc_JSInExtendedScript(d.value_.get_str().size());
                if(!js_extended)
                {
                    lpDetails->SetSpecialParamValue(MC_ENT_SPRM_FILTER_CODE,(unsigned char*)d.value_.get_str().c_str(),d.value_.get_str().size());                                                        
                }
                else
                {
                    js=d.value_.get_str();
                }
            }
            else
            {
                *strError=string("Invalid code field type");                            
            }
            filter_code=d.value_.get_str();
            missing_code=false;
            field_parsed=true;
        }
        if(d.name_ == "create")
        {
            if (strcmp(d.value_.get_str().c_str(),"streamfilter") == 0)
            {
                filter_type=MC_FLT_TYPE_STREAM;
            }
            lpDetails->SetSpecialParamValue(MC_ENT_SPRM_FILTER_TYPE,(unsigned char*)&filter_type,4);
            
            
            field_parsed=true;
        }
        if(!field_parsed)
        {
            *strError=strprintf("Invalid field: %s",d.name_.c_str());
        }
    }    
    
    filter_main_name=MC_FLT_MAIN_NAME_TX;
    
    if(strError->size() == 0)
    {
        if(filter_type != MC_FLT_TYPE_TX)
        {
            filter_main_name=MC_FLT_MAIN_NAME_STREAM;
            if(!missing_for)
            {
                *strError=string("for field is allowed only for tx filters");                                                                                                        
                *errorCode=RPC_NOT_ALLOWED;                
            }
        }
    }
    
    if(strError->size() == 0)
    {

        if(missing_code)
        {                    
            *strError=string("Missing code");                                                                                            
        }        
        else
        {
            mc_Filter *worker=new mc_Filter;
            string strFilterError;
            int err=pFilterEngine->CreateFilter(filter_code.c_str(),filter_main_name,pMultiChainFilterEngine->m_CallbackNames[filter_type],worker,strFilterError);
            delete worker;
            if(err)
            {
                *strError=string("Couldn't create filter");                                                                                                        
                *errorCode=RPC_INTERNAL_ERROR;                
            }
            else
            {      
                if(strFilterError.size())
                {
                    *strError=strprintf("Couldn't compile filter code: %s",strFilterError.c_str());                                                                                                      
                }
            }            
        }
    }
    
    if(strError->size() == 0)
    {
        int err;
        script=lpDetails->GetData(0,&bytes);
        lpDetailsScript->Clear();
        err=lpDetailsScript->SetNewEntityType(MC_ENT_TYPE_FILTER,0,script,bytes);
        if(err)
        {
            *strError=string("Invalid code, too long");                                                            
        }
        else
        {
            script = lpDetailsScript->GetData(0,&bytes);
            scriptOpReturn << vector<unsigned char>(script, script + bytes) << OP_DROP << OP_RETURN;
            if(js_extended)
            {
                lpDetails->Clear();
                lpDetails->AddElement();
                lpDetails->SetSpecialParamValue(MC_ENT_SPRM_FILTER_CODE,(unsigned char*)js.c_str(),js.size());                                                        

                script=lpDetails->GetData(0,&bytes);
                lpDetailsScript->Clear();
                lpDetailsScript->SetExtendedDetails(script,bytes);
                script = lpDetailsScript->GetData(0,&bytes);
                scriptOpReturn << vector<unsigned char>(script, script + bytes);
//                
//                scriptOpReturn << vector<unsigned char>((unsigned char*)js.c_str(), (unsigned char*)js.c_str() + js.size());
            }
        }        
    }
    
    return scriptOpReturn;
}

CScript RawDataScriptCreateVariable(Value *param,mc_Script *lpDetails,mc_Script *lpDetailsScript,int *errorCode,string *strError)
{
    CScript scriptOpReturn=CScript();
    bool field_parsed;
    size_t bytes;
    const unsigned char *script;
    string entity_name;
    bool js_extended=false;
    Value varvalue=Value::null;
    size_t elem_size;
    const unsigned char *elem;

    bool missing_name=true;
    bool missing_value=true;
    
    lpDetails->Clear();
    lpDetails->AddElement();                   

    lpDetailsScript->Clear();
    lpDetailsScript->AddElement();                   
        
    unsigned char b=1;        
    lpDetails->SetSpecialParamValue(MC_ENT_SPRM_FOLLOW_ONS,&b,1);
    
    BOOST_FOREACH(const Pair& d, param->get_obj()) 
    {
        field_parsed=false;
        if(d.name_ == "name")
        {
            if(!missing_name)
            {
                *strError=string("name field can appear only once in the object");                                                                                                        
            }
            if(d.value_.type() != null_type && !d.value_.get_str().empty())
            {
                entity_name=d.value_.get_str();
                if(entity_name.size())
                {
                    if(entity_name.size() > MC_ENT_MAX_NAME_SIZE)
                    {
                        *strError=string("Invalid variable name - too long"); 
                    }
                    lpDetails->SetSpecialParamValue(MC_ENT_SPRM_NAME,(const unsigned char*)(entity_name.c_str()),entity_name.size());
                }
            }
            else
            {
                *strError=string("Invalid name");                            
            }
            missing_name=false;
            field_parsed=true;
        }        
        if(d.name_ == "value")
        {
            if(!missing_value)
            {
                *strError=string("value field can appear only once in the object");                                                                                                        
            }
            size_t max_size=MC_AST_MAX_NOT_EXTENDED_VARIABLE_SIZE;
            lpDetailsScript->Clear();
            ParseRawValue(&(d.value_),lpDetails,lpDetailsScript,&max_size,errorCode,strError);        
            if(max_size > MC_AST_MAX_NOT_EXTENDED_VARIABLE_SIZE)
            {
                js_extended=true;
                varvalue=d.value_;
            }
            lpDetailsScript->Clear();
            
            missing_value=false;
            field_parsed=true;
        }
        if(d.name_ == "create")field_parsed=true;
        if(!field_parsed)
        {
            *strError=strprintf("Invalid field: %s",d.name_.c_str());
        }
    }    
    
    if(strError->size() == 0)
    {
        int err;
        script=lpDetails->GetData(0,&bytes);
        lpDetailsScript->Clear();
        err=lpDetailsScript->SetNewEntityType(MC_ENT_TYPE_VARIABLE,0,script,bytes);
        if(err)
        {
            *strError=string("Invalid value, too long");                                                            
        }
        else
        {
            script = lpDetailsScript->GetData(0,&bytes);
            scriptOpReturn << vector<unsigned char>(script, script + bytes) << OP_DROP << OP_RETURN;
            if(js_extended)
            {
                lpDetails->Clear();
                lpDetails->AddElement();
                ParseRawValue(&(varvalue),lpDetails,lpDetailsScript,NULL,errorCode,strError);        

                elem=lpDetails->GetData(0,&elem_size);
                lpDetailsScript->Clear();
                lpDetailsScript->SetExtendedDetails(elem,elem_size);
                elem = lpDetailsScript->GetData(0,&elem_size);
                scriptOpReturn << vector<unsigned char>(elem, elem + elem_size);
            }                    
        }        
    }
    
    return scriptOpReturn;
}

CScript RawDataScriptUpdateVariable(Value *param,mc_EntityDetails *entity,mc_Script *lpDetails,mc_Script *lpDetailsScript,int *errorCode,string *strError)
{
    CScript scriptOpReturn=CScript();
    bool field_parsed;
    size_t bytes;
    const unsigned char *script;
    bool js_extended=false;
    Value varvalue=Value::null;
    size_t elem_size;
    const unsigned char *elem;
    int err;

    bool missing_value=true;
    
    lpDetails->Clear();
    lpDetails->AddElement();                   

    lpDetailsScript->Clear();
    lpDetailsScript->AddElement();                   
    
    BOOST_FOREACH(const Pair& d, param->get_obj()) 
    {
        field_parsed=false;
        if(d.name_ == "value")
        {
            if(!missing_value)
            {
                *strError=string("value field can appear only once in the object");                                                                                                        
            }
            size_t max_size=MC_AST_MAX_NOT_EXTENDED_VARIABLE_SIZE;
            lpDetailsScript->Clear();
            ParseRawValue(&(d.value_),lpDetails,lpDetailsScript,&max_size,errorCode,strError);        
            if(max_size > MC_AST_MAX_NOT_EXTENDED_VARIABLE_SIZE)
            {
                js_extended=true;
                varvalue=d.value_;
            }
            lpDetailsScript->Clear();
            
            missing_value=false;
            field_parsed=true;
        }
        if(d.name_ == "update")field_parsed=true;
        if(!field_parsed)
        {
            *strError=strprintf("Invalid field: %s",d.name_.c_str());;                                
        }
    }    
    
    lpDetailsScript->Clear();
    lpDetailsScript->SetEntity(entity->GetTxID()+MC_AST_SHORT_TXID_OFFSET);
    script = lpDetailsScript->GetData(0,&bytes);
    scriptOpReturn << vector<unsigned char>(script, script + bytes) << OP_DROP;

    lpDetailsScript->Clear();
    script=lpDetails->GetData(0,&bytes);
    err=lpDetailsScript->SetNewEntityType(MC_ENT_TYPE_VARIABLE,1,script,bytes);
    if(err)
    {
        *strError=string("Invalid custom fields, too long");                                                            
    }
    else
    {
        script = lpDetailsScript->GetData(0,&bytes);
        scriptOpReturn << vector<unsigned char>(script, script + bytes) << OP_DROP << OP_RETURN;
        if(js_extended)
        {
            lpDetails->Clear();
            lpDetails->AddElement();
            ParseRawValue(&(varvalue),lpDetails,lpDetailsScript,NULL,errorCode,strError);        

            elem=lpDetails->GetData(0,&elem_size);
            lpDetailsScript->Clear();
            lpDetailsScript->SetExtendedDetails(elem,elem_size);
            elem = lpDetailsScript->GetData(0,&elem_size);
            scriptOpReturn << vector<unsigned char>(elem, elem + elem_size);
        }                    
    }
    
    return scriptOpReturn;
}



CScript RawDataScriptPublish(Value *param,mc_EntityDetails *entity,uint32_t *data_format,mc_Script *lpDetailsScript,vector<uint256>* vChunkHashes,int *errorCode,string *strError)
{
    CScript scriptOpReturn=CScript();
    vector<unsigned char> vValue;
    vector<unsigned char> vKey;
    Array vKeys; 
    size_t bytes;
    const unsigned char *script;
    bool field_parsed;
    bool missing_data=true;
    bool missing_key=true;
    uint32_t in_options,out_options;
    in_options=MC_RFD_OPTION_NONE;
    out_options=MC_RFD_OPTION_NONE;
    vKeys.clear();
    BOOST_FOREACH(const Pair& d, param->get_obj()) 
    {
        if(d.name_ == "options")
        {
            if( mc_gState->m_Features->OffChainData() == 0 )
            {
                *errorCode=RPC_NOT_SUPPORTED;
                *strError=string("Format options are not supported by this protocol version");       
                goto exitlbl;
            }
            if(d.value_.type() != null_type && (d.value_.type()==str_type))
            {
                if(d.value_.get_str() == "offchain")
                {
                    in_options |= MC_RFD_OPTION_OFFCHAIN;
                }
                else
                {
                    if(d.value_.get_str().size())
                    {
                        *strError=string("Stream item options must be offchain or empty");                                                
                    }
                }
            }
            else
            {
                *strError=string("Stream item options must be offchain or empty");                            
            }
            field_parsed=true;
        }                
    }
    
    BOOST_FOREACH(const Pair& d, param->get_obj()) 
    {
        field_parsed=false;
        if(d.name_ == "key")
        {
            if(!missing_key)
            {
                *strError=string("only one of the key fields can appear in the object");                                                                                                        
            }
            if(d.value_.type() != null_type && (d.value_.type()==str_type))
            {
                vKeys.push_back(d.value_);
            }
            else
            {
                *strError=string("Invalid key");                            
            }
            field_parsed=true;
            missing_key=false;
        }
        if(d.name_ == "keys")
        {
            if( mc_gState->m_Features->MultipleStreamKeys() == 0 )
            {
                *errorCode=RPC_NOT_SUPPORTED;
                *strError=string("Multiple keys are not supported by this protocol version");       
                goto exitlbl;
            }
            if(!missing_key)
            {
                *strError=string("only one of the key fields can appear in the object");                                                                                                        
            }
            if(d.value_.type() == array_type)
            {
                vKeys=d.value_.get_array();
                if(vKeys.size() == 0)
                {
                    *strError=string("Invalid keys - should be non-empty array");                                                
                }
            }            
            else
            {
                *strError=string("Invalid keys - should be array");                            
            }
            field_parsed=true;
            missing_key=false;
        }
        if(d.name_ == "data")        
        {
            if(!missing_data)
            {
                *strError=string("data field can appear only once in the object");                                                                                                        
            }
            vValue=ParseRawFormattedData(&(d.value_),data_format,lpDetailsScript,in_options,&out_options,errorCode,strError);
            field_parsed=true;
            missing_data=false;
        }
        if(d.name_ == "options")
        {
            field_parsed=true;
        }        
        if(d.name_ == "for")field_parsed=true;
//        if(d.name_ == "format")field_parsed=true;
        if(!field_parsed)
        {
            *strError=strprintf("Invalid field: %s",d.name_.c_str());;                                
        }
    }    
    
    if(missing_data)
    {
        *strError=string("Missing data field");            
    }
    if(missing_key)
    {
        *strError=string("Missing key field");        
        if(mc_gState->m_Features->MultipleStreamKeys())
        {
            *strError=string("Missing keys field");                    
        }
    }

    if(strError->size() == 0)
    {
        lpDetailsScript->Clear();
        lpDetailsScript->SetEntity(entity->GetTxID()+MC_AST_SHORT_TXID_OFFSET);
        script = lpDetailsScript->GetData(0,&bytes);
        scriptOpReturn << vector<unsigned char>(script, script + bytes) << OP_DROP;
        
        for(int i=0;i<(int)vKeys.size();i++)
        {
            lpDetailsScript->Clear();
            if(vKeys[i].type() != null_type && (vKeys[i].type()==str_type))
            {
                vKey=vector<unsigned char>(vKeys[i].get_str().begin(), vKeys[i].get_str().end());    
                if(vKey.size() > MC_ENT_MAX_ITEM_KEY_SIZE)
                {
                    throw JSONRPCError(RPC_INVALID_PARAMETER, "Item key is too long");                                                                                                    
                    goto exitlbl;
                }        
            }
            else
            {
                *strError=string("key should be string");                                            
                goto exitlbl;
            }
        
            if(lpDetailsScript->SetItemKey(&vKey[0],vKey.size()) == MC_ERR_NOERROR)
            {
                script = lpDetailsScript->GetData(0,&bytes);
                scriptOpReturn << vector<unsigned char>(script, script + bytes) << OP_DROP;
            }
        }

        if(entity->AnyoneCanRead() == 0)
        {
            pEF->LIC_RPCVerifyFeature(MC_EFT_STREAM_READ_RESTRICTED_WRITE,"Publishing to read-restricted stream");
        }
        
        if(entity->Restrictions() & MC_ENT_ENTITY_RESTRICTION_NEED_SALTED)
        {
            out_options |= MC_RFD_OPTION_SALTED;
        }
        
        if(in_options & MC_RFD_OPTION_OFFCHAIN)
        {            
            AppendOffChainFormatData(*data_format,out_options,lpDetailsScript,vValue,vChunkHashes,errorCode,strError);
            if(strError->size())
            {
                goto exitlbl;                                
            }
            script = lpDetailsScript->GetData(0,&bytes);
            scriptOpReturn << vector<unsigned char>(script, script + bytes) << OP_DROP;                    
            scriptOpReturn << OP_RETURN;                                        
        }
        else
        {
            if(out_options & MC_RFD_OPTION_OFFCHAIN)
            {
                *strError=string("chunks data type is not allowed with missing options field");                                            
                *errorCode=RPC_NOT_ALLOWED;
                goto exitlbl;                
            }
            if(*data_format != MC_SCR_DATA_FORMAT_UNKNOWN)
            {
                lpDetailsScript->Clear();
                lpDetailsScript->SetDataFormat(*data_format);
                script = lpDetailsScript->GetData(0,&bytes);
                scriptOpReturn << vector<unsigned char>(script, script + bytes) << OP_DROP;                    
            }

            if(vValue.size())
            {
                scriptOpReturn << OP_RETURN << vValue;                            
            }
            else
            {
                scriptOpReturn << OP_RETURN;                                        
            }
        }
    }
    
exitlbl:
            
    return scriptOpReturn;
}

CScript RawDataScriptApprove(Value *param,mc_EntityDetails *entity,mc_Script *lpDetailsScript,int *errorCode,string *strError)
{
    CScript scriptOpReturn=CScript();
    vector<unsigned char> vValue;
    vector<unsigned char> vKey;
    size_t bytes;
    const unsigned char *script;
    bool field_parsed;
    int is_approve=true;
    bool missing_approve=true;
    
    BOOST_FOREACH(const Pair& d, param->get_obj()) 
    {
        field_parsed=false;
        if(d.name_ == "approve")
        {
            if(!missing_approve)
            {
                *strError=string("approve field can appear only once in the object");                                                                                                        
            }
            if(d.value_.type() == bool_type)
            {
                is_approve=d.value_.get_bool();
            }    
            else
            {
                *strError=string("Invalid approve");                                            
            }
            field_parsed=true;
            missing_approve=false;
        }
        if(d.name_ == "for")field_parsed=true;
        if(!field_parsed)
        {
            *strError=strprintf("Invalid field: %s",d.name_.c_str());;                                
        }
    }    
    
    if(missing_approve)
    {
        *strError=string("Missing approve field");            
    }

    if(strError->size() == 0)
    {
        lpDetailsScript->Clear();
        lpDetailsScript->SetEntity(entity->GetTxID()+MC_AST_SHORT_TXID_OFFSET);
        script = lpDetailsScript->GetData(0,&bytes);
        scriptOpReturn << vector<unsigned char>(script, script + bytes) << OP_DROP;                    

        lpDetailsScript->Clear();
        lpDetailsScript->SetApproval(is_approve, mc_TimeNowAsUInt());
        script = lpDetailsScript->GetData(0,&bytes);
        scriptOpReturn << vector<unsigned char>(script, script + bytes) << OP_DROP;                    

        scriptOpReturn << OP_RETURN;                    
    }
    
    return scriptOpReturn;
}


CScript RawDataScriptInputCache(Value *param,mc_Script *lpDetails,int *errorCode,string *strError)
{
    CScript scriptOpReturn=CScript();
    size_t bytes;
    const unsigned char *script;
    bool field_parsed;
    BOOST_FOREACH(const Pair& d, param->get_obj()) 
    {
        field_parsed=false;
        if(d.name_ == "inputcache")
        {
            if(d.value_.type() != array_type)
            {
                *strError=string("Array should be specified for inputcache");                                                
            }
            else
            {
                int cs_offset,cs_vin,cs_size;
                string cs_script="";
                Array csa=d.value_.get_array();
                lpDetails->Clear();
                lpDetails->SetCachedScript(0,&cs_offset,-1,NULL,-1);
                for(int csi=0;csi<(int)csa.size();csi++)
                {
                    if(strError->size() == 0)
                    {
                        if(csa[csi].type() != obj_type)
                        {
                            *strError=string("Elements of inputcache should be objects");                                                
                        }
                        cs_vin=-1;
                        cs_size=-1;
                        BOOST_FOREACH(const Pair& csf, csa[csi].get_obj())                                 
                        {              
                            bool cs_parsed=false;
                            if(csf.name_ == "vin")
                            {
                                cs_parsed=true;
                                if(csf.value_.type() != int_type)
                                {
                                    *strError=string("vin should be integer");                                                                                            
                                }
                                else
                                {
                                    cs_vin=csf.value_.get_int();
                                } 
                            }
                            if(csf.name_ == "scriptPubKey")
                            {
                                cs_parsed=true;
                                if(csf.value_.type() != str_type)
                                {
                                    *strError=string("scriptPubKey should be string");                                                                                            
                                }
                                else
                                {
                                    cs_script=csf.value_.get_str();
                                    cs_size=cs_script.size()/2;
                                } 
                            }
                            if(!cs_parsed)
                            {
                                *strError=string("Invalid field: ") + csf.name_;                                                                                    
                            }
                        }
                        if(strError->size() == 0)
                        {
                            if(cs_vin<0)
                            {
                                *strError=string("Missing vin field");                                                                                                                            
                            }
                        }
                        if(strError->size() == 0)
                        {
                            if(cs_size<0)
                            {
                                *strError=string("Missing scriptPubKey field");                                                                                                                            
                            }
                        }                                
                        if(strError->size() == 0)
                        {
                            bool fIsHex;
                            vector<unsigned char> dataData(ParseHex(cs_script.c_str(),fIsHex));    
                            if(!fIsHex)
                            {
                                *strError=string("scriptPubKey should be hexadecimal string");                                                                                                                            
                            }                                    
                            else
                            {
                                lpDetails->SetCachedScript(cs_offset,&cs_offset,cs_vin,&dataData[0],cs_size);                                        
                            }
                        }
                    }
                }
            }
            field_parsed=true;
        }
        
        if(!field_parsed)
        {
            *strError=strprintf("Invalid field: %s",d.name_.c_str());;                                
        }        
    }    
    
    if(strError->size() == 0)
    {
        script=lpDetails->GetData(0,&bytes);
        scriptOpReturn << vector<unsigned char>(script, script + bytes) << OP_DROP << OP_RETURN;        
    }
    
    return scriptOpReturn;
}

CScript ParseRawMetadata(Value param,uint32_t allowed_objects,mc_EntityDetails *given_entity,mc_EntityDetails *found_entity)
{
    vector<uint256> vChunkHashes;
    string strError="";
    int errorCode=RPC_INVALID_PARAMETER;
    uint32_t data_format;
    mc_EntityDetails entity;
    CScript scriptOpReturn=CScript();
    mc_Script *lpDetailsScript=mc_gState->m_TmpBuffers->m_RpcScript1;
    lpDetailsScript->Clear();
    mc_Script *lpDetails=mc_gState->m_TmpBuffers->m_RpcScript2;
    lpDetails->Clear();
    uint32_t param_type=ParseRawDataParamType(&param,given_entity,&entity,&data_format,&errorCode,&strError);

    if(strError.size())
    {
        goto exitlbl;
    }
    
    if(param_type == MC_DATA_API_PARAM_TYPE_NONE)
    {                
        strError=string("Unrecognized parameter format");       
        goto exitlbl;                
    }
    
    if( (param_type & allowed_objects) == 0 )
    {
        if(param_type != MC_DATA_API_PARAM_TYPE_EMPTY_RAW)
        {
            strError=string("Keyword not allowed in this API");       
            errorCode=RPC_NOT_ALLOWED;
        }
        goto exitlbl;        
    }
    
    if(found_entity)
    {
        memcpy(found_entity,&entity,sizeof(mc_EntityDetails));
    }                        
    
    switch(param_type)
    {
        case MC_DATA_API_PARAM_TYPE_EMPTY_RAW:
        case MC_DATA_API_PARAM_TYPE_RAW:
            scriptOpReturn=RawDataScriptRawHex(&param,&errorCode,&strError);
            break;
        case MC_DATA_API_PARAM_TYPE_FORMATTED:
            scriptOpReturn=RawDataScriptFormatted(&param,&data_format,lpDetailsScript,&errorCode,&strError);
            break;
        case MC_DATA_API_PARAM_TYPE_ISSUE:
            scriptOpReturn=RawDataScriptIssue(&param,lpDetails,lpDetailsScript,&errorCode,&strError);
            break;
        case MC_DATA_API_PARAM_TYPE_FOLLOWON:
            scriptOpReturn=RawDataScriptFollowOn(&param,&entity,lpDetails,lpDetailsScript,&errorCode,&strError);
            break;
        case MC_DATA_API_PARAM_TYPE_CREATE_STREAM:
            scriptOpReturn=RawDataScriptCreateStream(&param,lpDetails,lpDetailsScript,&errorCode,&strError);
            break;
        case MC_DATA_API_PARAM_TYPE_PUBLISH:
            scriptOpReturn=RawDataScriptPublish(&param,&entity,&data_format,lpDetailsScript,&vChunkHashes,&errorCode,&strError);
            break;
        case MC_DATA_API_PARAM_TYPE_CREATE_UPGRADE:
            scriptOpReturn=RawDataScriptCreateUpgrade(&param,lpDetails,lpDetailsScript,&errorCode,&strError);
            break;
        case MC_DATA_API_PARAM_TYPE_CREATE_FILTER:
            scriptOpReturn=RawDataScriptCreateFilter(&param,lpDetails,lpDetailsScript,&errorCode,&strError);
            break;
        case MC_DATA_API_PARAM_TYPE_CREATE_VAR:
            scriptOpReturn=RawDataScriptCreateVariable(&param,lpDetails,lpDetailsScript,&errorCode,&strError);
            break;
        case MC_DATA_API_PARAM_TYPE_UPDATE_VAR:
            scriptOpReturn=RawDataScriptUpdateVariable(&param,&entity,lpDetails,lpDetailsScript,&errorCode,&strError);
            break;
        case MC_DATA_API_PARAM_TYPE_APPROVAL:
            scriptOpReturn=RawDataScriptApprove(&param,&entity,lpDetailsScript,&errorCode,&strError);
            break;
        case MC_DATA_API_PARAM_TYPE_CIS:
            scriptOpReturn=RawDataScriptInputCache(&param,lpDetailsScript,&errorCode,&strError);
            break;
    }
    
exitlbl:
    
    if(strError.size())
    {
        throw JSONRPCError(errorCode, strError);            
    }

    return scriptOpReturn;
}
