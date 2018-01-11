// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2014-2016 The Bitcoin Core developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2017 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "rpc/rpcutils.h"

#include "utils/util.h"
#include "json/json_spirit_ubjson.h"

#include <boost/assign/list_of.hpp>

using namespace std;
using namespace json_spirit;

uint32_t ParseRawDataParamType(Value *param,mc_EntityDetails *given_entity,mc_EntityDetails *entity,uint32_t *data_format,int *errorCode,string *strError)
{
    uint32_t param_type=MC_DATA_API_PARAM_TYPE_NONE;   
    uint32_t this_param_type;  
    bool missing_data=true;
    *data_format=MC_SCR_DATA_FORMAT_UNKNOWN;
    entity->Zero();
    
    if(param->type() == obj_type)
    {
        BOOST_FOREACH(const Pair& d, param->get_obj()) 
        {
            this_param_type=MC_DATA_API_PARAM_TYPE_NONE;   
            if(d.name_ == "inputcache")
            {
                if( mc_gState->m_Features->CachedInputScript() == 0 )
                {
                    *errorCode=RPC_NOT_SUPPORTED;
                    *strError=string("Cached input scripts are not supported by this protocol version");       
                    goto exitlbl;
                }
                this_param_type=MC_DATA_API_PARAM_TYPE_CIS;
            }            
            if(d.name_ == "create")
            {
                if(d.value_.type() != null_type && !d.value_.get_str().empty())
                {
                    if(d.value_.get_str() == "stream")
                    {
                        if( mc_gState->m_Features->Streams() == 0 )
                        {
                            *errorCode=RPC_NOT_SUPPORTED;
                            *strError=string("Streams are not supported by this protocol version");       
                            goto exitlbl;
                        }
                        this_param_type=MC_DATA_API_PARAM_TYPE_CREATE_STREAM;
                    }
                    if(d.value_.get_str() == "asset")
                    {
                        this_param_type=MC_DATA_API_PARAM_TYPE_ISSUE;
                    }
                    if(d.value_.get_str() == "upgrade")
                    {
                        if( mc_gState->m_Features->Upgrades() == 0 )
                        {
                            *errorCode=RPC_NOT_SUPPORTED;
                            *strError=string("Upgrades are not supported by this protocol version");       
                            goto exitlbl;
                        }
                        this_param_type=MC_DATA_API_PARAM_TYPE_CREATE_UPGRADE;
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
                    ParseEntityIdentifier(d.value_,entity, MC_ENT_TYPE_ASSET);       
                }
                if(entity->GetEntityType() != MC_ENT_TYPE_ASSET)
                {
                    *strError=string("Asset with this identifier not found");                                                                        
                    goto exitlbl;                        
                }                
                this_param_type=MC_DATA_API_PARAM_TYPE_FOLLOWON;                
            }
            if(d.name_ == "for")
            {
                if(d.value_.type() != null_type && !d.value_.get_str().empty())
                {
                    ParseEntityIdentifier(d.value_,entity, MC_ENT_TYPE_ANY);       
                }                
                if(entity->GetEntityType() == MC_ENT_TYPE_STREAM)
                {
                    if( mc_gState->m_Features->Streams() == 0 )
                    {
                        *errorCode=RPC_NOT_SUPPORTED;
                        *strError=string("Upgrades are not supported by this protocol version");       
                        goto exitlbl;
                    }
                    this_param_type=MC_DATA_API_PARAM_TYPE_PUBLISH;                
                }
                if(entity->GetEntityType() == MC_ENT_TYPE_UPGRADE)
                {
                    if( mc_gState->m_Features->Upgrades() == 0 )
                    {
                        *errorCode=RPC_NOT_SUPPORTED;
                        *strError=string("Upgrades are not supported by this protocol version");       
                        goto exitlbl;
                    }
                    this_param_type=MC_DATA_API_PARAM_TYPE_APPROVAL;                
                }
                if(this_param_type == MC_DATA_API_PARAM_TYPE_NONE)
                {
                    *strError=string("Entity with this identifier not found");                            
                    goto exitlbl;                        
                }
            }
/*            
            if(d.name_ == "data")
            {
                if(!missing_data)
                {
                    *strError=string("data field can appear only once in the object");                                                                                        
                    goto exitlbl;                    
                }
                missing_data=false;
                if(d.value_.type() != str_type)
                {
                    if(d.value_.type() != obj_type)
                    {
                        *strError=string("data should be string or object");                                                                                        
                        goto exitlbl;                                            
                    }
                }
            }
 */ 
            if( (d.name_ == "text") || (d.name_ == "json") )
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
/*            
            if(d.name_ == "format")
            {
                if( mc_gState->m_Features->FormattedData() == 0 )
                {
                    *errorCode=RPC_NOT_SUPPORTED;
                    *strError=string("Formatted data is not supported by this protocol version");       
                    goto exitlbl;
                }
                if(*data_format != MC_SCR_DATA_FORMAT_UNKNOWN)
                {
                    *strError=string("format field can appear only once in the object");                                                                                        
                    goto exitlbl;                    
                }
                if(d.value_.type() != null_type && !d.value_.get_str().empty())
                {
                    if(d.value_.get_str() == "hex")
                    {
                        *data_format=MC_SCR_DATA_FORMAT_RAW;                        
                    }
                    if(d.value_.get_str() == "text")
                    {
                        *data_format=MC_SCR_DATA_FORMAT_UTF8;                        
                    }
                    if(d.value_.get_str() == "json")
                    {
                        *data_format=MC_SCR_DATA_FORMAT_UBJSON;                        
                    }
                }
                if(*data_format == MC_SCR_DATA_FORMAT_UNKNOWN)
                {
                    *strError=string("Invalid format");                                                    
                }
            }
 */ 
            if(this_param_type != MC_DATA_API_PARAM_TYPE_NONE)
            {
                if(param_type != MC_DATA_API_PARAM_TYPE_NONE)
                {                
                    *strError=string("Only one of the following keywords can appear in the object: create, update");                                                                                        
                    if(mc_gState->m_Features->Streams())
                    {
                        *strError += string(", for");
                    }
                    if(mc_gState->m_Features->Streams())
                    {
                        *strError += string(", json, text");
                    }
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

vector<unsigned char> ParseRawFormattedData(const Value *value,uint32_t *data_format,mc_Script *lpDetailsScript,bool allow_formatted,int *errorCode,string *strError)
{
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
        if(allow_formatted || (mc_gState->m_Features->FormattedData() != 0) )
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
    /*                    
                        if(d.name_ == "raw")
                        {
                            bool fIsHex;
                            vValue=ParseHex(d.value_.get_str().c_str(),fIsHex);    
                            if(!fIsHex)
                            {
                                *strError=string("value in data object should be hexadecimal string");                            
                            }
                            *data_format=MC_SCR_DATA_FORMAT_RAW;                    
                        }
    */ 
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
                        if(*data_format == MC_SCR_DATA_FORMAT_UNKNOWN)
                        {
                            throw JSONRPCError(RPC_NOT_SUPPORTED, "Unsupported item data type: " + d.name_);                                    
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
    
/*    
    switch(data_format)
    {
        case MC_SCR_DATA_FORMAT_RAW:
        case MC_SCR_DATA_FORMAT_UNKNOWN:
            if(value->type() != null_type && (value->type()==str_type))
            {
                bool fIsHex;
                vValue=ParseHex(value->get_str().c_str(),fIsHex);    
                if(!fIsHex)
                {
                    *strError=string("value should be hexadecimal string");                            
                }
            }
            else
            {
                *strError=string("Invalid value in data field for this format");                            
            }
            break;
        case MC_SCR_DATA_FORMAT_UTF8:
            if(value->type() != null_type && (value->type()==str_type))
            {
                vValue=vector<unsigned char> (value->get_str().begin(),value->get_str().end());    
            }
            else
            {
                *strError=string("Invalid value in data field for this format");                            
            }
            break;
        case MC_SCR_DATA_FORMAT_UBJSON:
            size_t bytes;
            int err;
            const unsigned char *script;
            lpDetailsScript->Clear();
            lpDetailsScript->AddElement();
            if((err = ubjson_write(*value,lpDetailsScript,MAX_FORMATTED_DATA_DEPTH)) != MC_ERR_NOERROR)
            {
                *strError=string("Couldn't transfer JSON object to internal UBJSON format");    
            }
            script = lpDetailsScript->GetData(0,&bytes);
            vValue=vector<unsigned char> (script,script+bytes);                                            
            break;
    }
*/
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
/*        
        if(d.name_ == "data")        
        {
            if(!missing_data)
            {
                *strError=string("data field can appear only once in the object");                                                                                                        
            }
            vValue=ParseRawFormattedData(&(d.value_),data_format,lpDetailsScript,errorCode,strError);
            field_parsed=true;
            missing_data=false;
        }
 */ 
        if( (d.name_ == "text") || (d.name_ == "json") )      
        {
            if(!missing_data)
            {
                *strError=string("data object should have single key - json or text");                                                                                                        
            }
            vValue=ParseRawFormattedData(param,data_format,lpDetailsScript,false,errorCode,strError);
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
                if(entity_name.size())
                {
                    if(mc_gState->m_Features->OpDropDetailsScripts())
                    {
                        lpDetails->SetSpecialParamValue(MC_ENT_SPRM_NAME,(const unsigned char*)(entity_name.c_str()),entity_name.size());
                    }
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
                    if(mc_gState->m_Features->OpDropDetailsScripts())                    
                    {
                        lpDetails->SetSpecialParamValue(MC_ENT_SPRM_ASSET_MULTIPLE,(unsigned char*)&multiple,4);
                    }
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
        if(mc_gState->m_Features->OpDropDetailsScripts())
        {
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
        else
        {
            script=lpDetails->GetData(0,&bytes);
            err=lpDetailsScript->SetAssetDetails(entity_name.c_str(),multiple,script,bytes);                
            script = lpDetailsScript->GetData(0,&bytes);
            if(err)
            {
                *strError=string("Invalid custom fields, too long");                                                            
            }
            else
            {
                if(bytes > 0)
                {
                    scriptOpReturn << OP_RETURN << vector<unsigned char>(script, script + bytes);
                }                    
            }
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
    
    if(mc_gState->m_Features->OpDropDetailsScripts())
    {
        int err;
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
    }
    else
    {
        lpDetailsScript->Clear();
        script=lpDetails->GetData(0,&bytes);
        err=lpDetailsScript->SetGeneralDetails(script,bytes);                
        if(err)
        {
            *strError=string("Invalid custom fields, too long");                                                            
        }
        else
        {
            script = lpDetailsScript->GetData(0,&bytes);
            if(bytes > 0)
            {
                scriptOpReturn << OP_RETURN << vector<unsigned char>(script, script + bytes);
            }                    
        }
    }
    
    return scriptOpReturn;
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
    
    bool missing_name=true;
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
                *strError=string("open field can appear only once in the object");                                                                                                        
            }
            if(d.value_.type() != null_type && !d.value_.get_str().empty())
            {
                entity_name=d.value_.get_str();
                if(entity_name.size())
                {
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
            lpDetails->SetSpecialParamValue(MC_ENT_SPRM_ANYONE_CAN_WRITE,(unsigned char*)&is_open,1); 
            missing_open=false;
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
        if(mc_gState->m_Features->OpDropDetailsScripts())
        {
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
        else
        {
            lpDetailsScript->SetNewEntityType(MC_ENT_TYPE_STREAM);
            script = lpDetailsScript->GetData(0,&bytes);
            scriptOpReturn << vector<unsigned char>(script, script + bytes) << OP_DROP;

            lpDetailsScript->Clear();
            script=lpDetails->GetData(0,&bytes);
            err=lpDetailsScript->SetGeneralDetails(script,bytes);                
            if(err)
            {
                *strError=string("Invalid custom fields, too long");                                                            
            }
            else
            {
                script = lpDetailsScript->GetData(0,&bytes);
                if(bytes > 0)
                {
                    scriptOpReturn << OP_RETURN << vector<unsigned char>(script, script + bytes);
                }
            }
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
                *strError=string("open field can appear only once in the object");                                                                                                        
            }
            if(d.value_.type() != null_type && !d.value_.get_str().empty())
            {
                entity_name=d.value_.get_str();
                if(entity_name.size())
                {
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
                *strError=string("open field can appear only once in the object");                                                                                                        
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

CScript RawDataScriptPublish(Value *param,mc_EntityDetails *entity,uint32_t *data_format,mc_Script *lpDetailsScript,int *errorCode,string *strError)
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
    vKeys.clear();
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
            vValue=ParseRawFormattedData(&(d.value_),data_format,lpDetailsScript,false,errorCode,strError);
            field_parsed=true;
            missing_data=false;
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
            scriptOpReturn=RawDataScriptPublish(&param,&entity,&data_format,lpDetailsScript,&errorCode,&strError);
            break;
        case MC_DATA_API_PARAM_TYPE_CREATE_UPGRADE:
            scriptOpReturn=RawDataScriptCreateUpgrade(&param,lpDetails,lpDetailsScript,&errorCode,&strError);
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
