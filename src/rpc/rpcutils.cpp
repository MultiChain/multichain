// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2014-2016 The Bitcoin Core developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2017 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "rpc/rpcutils.h"
#ifdef ENABLE_WALLET
#include "wallet/wallet.h"
#endif

#include "utils/util.h"
#include "utils/utilmoneystr.h"
#include "wallet/wallettxs.h"
#include "json/json_spirit_ubjson.h"

#include <boost/assign/list_of.hpp>

using namespace std;
using namespace json_spirit;

uint256 hGenesisCoinbaseTxID=0;

int c_UTF8_charlen[256]={
 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
 2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
 2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
 3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
 4,4,4,4,4,4,4,4,5,5,5,5,6,6,0,0
};

CScript RemoveOpDropsIfNeeded(const CScript& scriptInput)
{
    if (!GetBoolArg("-hideknownopdrops", false))
    {
        return scriptInput;
    }
    
    return scriptInput.RemoveOpDrops();   
}

bool AssetRefDecode(unsigned char *bin, const char* string, const size_t stringLen)
{
    char buffer[1024];
    int txIDPrefixInteger;
    long long blockNum, txOffset;

    if(stringLen <= 0)
        return false;
        
    if (stringLen>=sizeof(buffer))
        return false;
    
    memcpy(buffer, string, stringLen);
    buffer[stringLen]=0; // copy to our buffer and null terminate to allow scanf
    
    if (strchr(buffer, '+')) // special check for '+' character which would be accepted by sscanf() below
        return false;
           
    if (sscanf(buffer, "%lld-%lld-%d", &blockNum, &txOffset, &txIDPrefixInteger)!=3)
        return false;
    
    if ( (txIDPrefixInteger<0) || (txIDPrefixInteger>0xFFFF) )
        return false;
    
    mc_PutLE(bin+0,&blockNum,4);
    mc_PutLE(bin+4,&txOffset,8);
    bin[8]=(unsigned char)(txIDPrefixInteger%256);
    bin[9]=(unsigned char)(txIDPrefixInteger/256);
    
    return true;
}

uint256 mc_GenesisCoinbaseTxID()
{
    if(hGenesisCoinbaseTxID == 0)
    {
        CBlock block;
        if(ReadBlockFromDisk(block, chainActive[0]))
        {
            hGenesisCoinbaseTxID=block.vtx[0].GetHash();
        }        
    }
    
    return hGenesisCoinbaseTxID;
}

Value mc_ExtractDetailsJSONObject(mc_EntityDetails *lpEnt)
{
    size_t value_size;
    int err;
    
    const void* ptr=lpEnt->GetSpecialParam(MC_ENT_SPRM_JSON_DETAILS,&value_size);
    
    if(ptr == NULL)
    {
        return Value::null;        
    }
    
    Value value=ubjson_read((const unsigned char *)ptr,value_size,MAX_FORMATTED_DATA_DEPTH,&err);
    
    if(err)
    {
        return Value::null;        
    }
    
    return value;
}

Value mc_ExtractDetailsJSONObject(const unsigned char *script,uint32_t total)
{
    size_t value_size;
    int err;
       
    uint32_t offset;
    offset=mc_FindSpecialParamInDetailsScript(script,total,MC_ENT_SPRM_JSON_DETAILS,&value_size);
    if(offset == total)
    {
        return Value::null;
    }
    
    const void* ptr=script+offset;
    
    
    Value value=ubjson_read((const unsigned char *)ptr,value_size,MAX_FORMATTED_DATA_DEPTH,&err);
    
    if(err)
    {
        return Value::null;        
    }
    
    return value;
}


int ParseAssetKey(const char* asset_key,unsigned char *txid,unsigned char *asset_ref,char *name,int *multiple,int *type,int entity_type)
{
    int ret=MC_ASSET_KEY_VALID;
    int size=strlen(asset_key);
    unsigned char buf[MC_AST_ASSET_REF_SIZE];
    mc_EntityDetails entity;
    const unsigned char *ptr;
        
    if(size == 0)
    {
        return MC_ASSET_KEY_INVALID_EMPTY;
    }
    
    uint256 hash=0;
    if(size == 64)
    {
        if(type)
        {
            *type=MC_ENT_KEYTYPE_TXID;
        }
        hash.SetHex(asset_key);
        if(!mc_gState->m_Assets->FindEntityByTxID(&entity,(unsigned char*)&hash))
        {
            ret=MC_ASSET_KEY_INVALID_TXID;
        }
        else
        {
            if( (entity_type != MC_ENT_TYPE_ANY) && ((int)entity.GetEntityType() != entity_type) )
            {
                ret=MC_ASSET_KEY_INVALID_TXID;
            }
            else
            {
                int root_stream_name_size;
                mc_gState->m_NetworkParams->GetParam("rootstreamname",&root_stream_name_size);        
                if(root_stream_name_size <= 1)
                {
                    if(hash == mc_GenesisCoinbaseTxID())
                    {
                        ret=MC_ASSET_KEY_INVALID_TXID;                            
                    }
                }
            }
            if(entity.IsFollowOn())
            {
                if(!mc_gState->m_Assets->FindEntityByFollowOn(&entity,(unsigned char*)&hash))
                {
                    ret=MC_ASSET_KEY_INVALID_TXID;
                }                
            }
        }
    }
    else
    {
        if(size<=MC_ENT_MAX_NAME_SIZE)
        {
            if(AssetRefDecode(buf,asset_key,size))
            {
                if(type)
                {
                    *type=MC_ENT_KEYTYPE_REF;
                }
                if(!mc_gState->m_Assets->FindEntityByRef(&entity,buf))
                {
                    ret=MC_ASSET_KEY_INVALID_REF;
                }
                else
                {
                    if( (entity_type != MC_ENT_TYPE_ANY) && ((int)entity.GetEntityType() != entity_type) )
                    {
                        ret=MC_ASSET_KEY_INVALID_REF;
                    }                    
                }
            }
            else
            {
                if(type)
                {
                    *type=MC_ENT_KEYTYPE_NAME;
                }
                if(!mc_gState->m_Assets->FindEntityByName(&entity,(char*)asset_key))
                {
                    ret=MC_ASSET_KEY_INVALID_NAME;
                }    
                else
                {
                    if( (entity_type != MC_ENT_TYPE_ANY) && ((int)entity.GetEntityType() != entity_type) )
                    {
                        ret=MC_ASSET_KEY_INVALID_NAME;
                    }                    
                }
            }
        }
        else
        {
            ret=MC_ASSET_KEY_INVALID_SIZE;
        }
    }

    if(ret == MC_ASSET_KEY_VALID)
    {
        if(txid)
        {
            memcpy(txid,entity.GetTxID(),32);
        }
        ptr=entity.GetRef();
        if(entity.IsUnconfirmedGenesis())
        {
            ret=MC_ASSET_KEY_UNCONFIRMED_GENESIS;                            
        }
        if(asset_ref)
        {
            memcpy(asset_ref,ptr,MC_AST_ASSET_REF_SIZE);            
        }
        if(name)
        {
            strcpy(name,entity.GetName());
        }        
        if(multiple)
        {
            *multiple=entity.GetAssetMultiple();
        }
    }
    
    
    return ret;
}

int ParseAssetKeyToFullAssetRef(const char* asset_key,unsigned char *full_asset_ref,int *multiple,int *type,int entity_type)
{
    int ret;
    if(mc_gState->m_Features->ShortTxIDInTx())
    {
        unsigned char txid[MC_ENT_KEY_SIZE];
        ret=ParseAssetKey(asset_key,txid,NULL,NULL,multiple,type,entity_type);
        if(ret == MC_ASSET_KEY_UNCONFIRMED_GENESIS)
        {
            ret=0;
        }
        memcpy(full_asset_ref+MC_AST_SHORT_TXID_OFFSET,txid+MC_AST_SHORT_TXID_OFFSET,MC_AST_SHORT_TXID_SIZE);
        
        mc_SetABRefType(full_asset_ref,MC_AST_ASSET_REF_TYPE_SHORT_TXID);        
    }
    else
    {
        ret=ParseAssetKey(asset_key,NULL,full_asset_ref,NULL,multiple,type,entity_type);        
    }
    return ret;
}

Array AddressEntries(const CTxIn& txin,txnouttype& typeRet,mc_Script *lpScript)
{
    Array addrs;
    const CScript& script2 = txin.scriptSig;        
    CScript::const_iterator pc2 = script2.begin();

    lpScript->Clear();
    lpScript->SetScript((unsigned char*)(&pc2[0]),(size_t)(script2.end()-pc2),MC_SCR_TYPE_SCRIPTSIGRAW);

    if(lpScript->GetNumElements() < 2)
    {
        typeRet=TX_PUBKEY;
        return addrs;
    }    
    
    size_t elem_size;
    const unsigned char *elem;

    typeRet=TX_PUBKEYHASH;
    elem = lpScript->GetData(0,&elem_size);
    if(elem_size == 1)
    {
        elem = lpScript->GetData(lpScript->GetNumElements()-1,&elem_size);
        if( (elem[0]<=0x50) || (elem[0]>0x60))                                  // it is not redeem script
        {
            typeRet=TX_MULTISIG;                                                // whatever it is there is no address here
            return addrs;
        }
        typeRet=TX_SCRIPTHASH;
    }    
    else
    {
        elem = lpScript->GetData(lpScript->GetNumElements()-1,&elem_size);        
    }
    
    uint160 hash=Hash160(elem,elem+elem_size);
    CBitcoinAddress address;
    
    if(typeRet == TX_PUBKEYHASH)
    {
        address=CBitcoinAddress(CKeyID(hash));        
    }
    if(typeRet == TX_SCRIPTHASH)
    {
        address=CBitcoinAddress(CScriptID(hash));        
    }
    
    addrs.push_back(address.ToString());
    return addrs;    
}

Array AddressEntries(const CTxOut& txout,txnouttype& typeRet)
{
    Array addrs;
    CBitcoinAddress address;
    int nRequiredRet;
    vector<CTxDestination> addressRets;

    if(ExtractDestinations(txout.scriptPubKey,typeRet,addressRets,nRequiredRet))
    {
        for(int d=0;d<(int)addressRets.size();d++)
        {
            CKeyID *lpKeyID=boost::get<CKeyID> (&addressRets[d]);
            CScriptID *lpScriptID=boost::get<CScriptID> (&addressRets[d]);
            if( (lpKeyID != NULL) || (lpScriptID != NULL) )
            {
                if(lpKeyID != NULL)
                {
                    address=CBitcoinAddress(*lpKeyID);
                }
                if(lpScriptID != NULL)
                {
                    address=CBitcoinAddress(*lpScriptID);
                }                
                addrs.push_back(address.ToString());
            }
        }
    }
    
    return addrs;
}

Value PermissionForFieldEntry(mc_EntityDetails *lpEntity)
{
    if(lpEntity->GetEntityType())
    {
        Object entObject;
        unsigned char *ptr;

        if(lpEntity->GetEntityType() == MC_ENT_TYPE_ASSET)
        {
            entObject.push_back(Pair("type", "asset"));      
            ptr=(unsigned char *)lpEntity->GetName();
            if(ptr && strlen((char*)ptr))
            {
                entObject.push_back(Pair("name", string((char*)ptr)));            
            }
            ptr=(unsigned char *)lpEntity->GetRef();
            string assetref="";
            if(lpEntity->IsUnconfirmedGenesis())
            {
                Value null_value;
                entObject.push_back(Pair("assetref",null_value));
            }
            else
            {
                assetref += itostr((int)mc_GetLE(ptr,4));
                assetref += "-";
                assetref += itostr((int)mc_GetLE(ptr+4,4));
                assetref += "-";
                assetref += itostr((int)mc_GetLE(ptr+8,2));
                entObject.push_back(Pair("assetref", assetref));
            }                
        }
        if(lpEntity->GetEntityType() == MC_ENT_TYPE_STREAM)
        {
            entObject.push_back(Pair("type", "stream"));      
            ptr=(unsigned char *)lpEntity->GetName();
            if(ptr && strlen((char*)ptr))
            {
                entObject.push_back(Pair("name", string((char*)ptr)));            
            }
            ptr=(unsigned char *)lpEntity->GetRef();
            string streamref="";
            if(lpEntity->IsUnconfirmedGenesis())
            {
                Value null_value;
                entObject.push_back(Pair("streamref",null_value));
            }
            else
            {
                if((int)mc_GetLE(ptr,4))
                {
                    streamref += itostr((int)mc_GetLE(ptr,4));
                    streamref += "-";
                    streamref += itostr((int)mc_GetLE(ptr+4,4));
                    streamref += "-";
                    streamref += itostr((int)mc_GetLE(ptr+8,2));
                }
                else
                {
                    streamref="0-0-0";                
                }
                entObject.push_back(Pair("streamref", streamref));
            }
        }
        return entObject;
    }
    
    return Value::null;
}

Array PerOutputDataEntries(const CTxOut& txout,mc_Script *lpScript,uint256 txid,int vout)
{
    Array results;
    unsigned char *ptr;
    int size;
    
    if(mc_gState->m_NetworkParams->IsProtocolMultichain() == 0)
    {
        return results;
    }    
    
    const CScript& script1 = txout.scriptPubKey;        
    CScript::const_iterator pc1 = script1.begin();

    lpScript->Clear();
    lpScript->SetScript((unsigned char*)(&pc1[0]),(size_t)(script1.end()-pc1),MC_SCR_TYPE_SCRIPTPUBKEY);
    
    for (int e = 0; e < lpScript->GetNumElements(); e++)
    {
        lpScript->SetElement(e);
        if(lpScript->GetRawData(&ptr,&size) == 0)      
        {
            uint32_t format=MC_SCR_DATA_FORMAT_UNKNOWN;
            if(e > 0)
            {
                lpScript->SetElement(e-1);
                lpScript->GetDataFormat(&format);
            }
            results.push_back(OpReturnFormatEntry(ptr,size,txid,vout,format,NULL));            
        }        
    }
    
    return results;
}


Array PermissionEntries(const CTxOut& txout,mc_Script *lpScript,bool fLong)
{
    Array results;
    
    uint32_t type,from,to,timestamp,full_type;
    unsigned char short_txid[MC_AST_SHORT_TXID_SIZE];
    mc_EntityDetails entity;
    
    if(mc_gState->m_NetworkParams->IsProtocolMultichain() == 0)
    {
        return results;
    }    

    const CScript& script1 = txout.scriptPubKey;        
    CScript::const_iterator pc1 = script1.begin();

    lpScript->Clear();
    lpScript->SetScript((unsigned char*)(&pc1[0]),(size_t)(script1.end()-pc1),MC_SCR_TYPE_SCRIPTPUBKEY);

    entity.Zero();                                                  // Permission processing    
    for (int e = 0; e < lpScript->GetNumElements(); e++)
    {
        lpScript->SetElement(e);
        if(lpScript->GetEntity(short_txid) == 0)      
        {
            if(entity.GetEntityType())
            {
                goto exitlbl;                                                   // invalid transaction                
            }
            if(mc_gState->m_Assets->FindEntityByShortTxID(&entity,short_txid) == 0)
            {
                goto exitlbl;                                                   // invalid transaction                
            }                        
        }
        else                                                        // Not entity element
        {
            if(lpScript->GetPermission(&type,&from,&to,&timestamp) == 0)
            {                
                Object entry;
                entry.push_back(Pair("for", PermissionForFieldEntry(&entity)));            
//                full_type=mc_gState->m_Permissions->GetPossiblePermissionTypes(entity.GetEntityType());
                full_type=mc_gState->m_Permissions->GetPossiblePermissionTypes(&entity);
                if(full_type & MC_PTP_CONNECT)entry.push_back(Pair("connect", (type & MC_PTP_CONNECT) ? true : false));
                if(full_type & MC_PTP_SEND)entry.push_back(Pair("send", (type & MC_PTP_SEND) ? true : false));
                if(full_type & MC_PTP_RECEIVE)entry.push_back(Pair("receive", (type & MC_PTP_RECEIVE) ? true : false));
                if(mc_gState->m_Features->Streams())
                {
                    if(full_type & MC_PTP_WRITE)entry.push_back(Pair("write", (type & MC_PTP_WRITE) ? true : false));                
                    if(full_type & MC_PTP_CREATE)entry.push_back(Pair("create", (type & MC_PTP_CREATE) ? true : false));                
                }
                if(full_type & MC_PTP_ISSUE)entry.push_back(Pair("issue", (type & MC_PTP_ISSUE) ? true : false));
                if(full_type & MC_PTP_MINE)entry.push_back(Pair("mine", (type & MC_PTP_MINE) ? true : false));
                if(full_type & MC_PTP_ADMIN)entry.push_back(Pair("admin", (type & MC_PTP_ADMIN) ? true : false));
                if(full_type & MC_PTP_ACTIVATE)entry.push_back(Pair("activate", (type & MC_PTP_ACTIVATE) ? true : false));                
                entry.push_back(Pair("startblock",(int64_t)from));
                entry.push_back(Pair("endblock",(int64_t)to));
                entry.push_back(Pair("timestamp",(int64_t)timestamp));        
                if(fLong)
                {
                    txnouttype typeRet;
                    entry.push_back(Pair("addresses", AddressEntries(txout,typeRet)));
                }
                results.push_back(entry);
            }            
            else
            {
                goto exitlbl;                                                   // invalid transaction                
            }
            entity.Zero();                                      
        }
    }
    
exitlbl:
                        
    return results;
}



Object StreamEntry(const unsigned char *txid,uint32_t output_level)
{
// output_level constants
// 0x0001 type
// 0x0002 txid
// 0x0004 open/details
// 0x0008 subscribed/synchronized    
// 0x0010 stats
// 0x0020 creators    
    
    Object entry;
    mc_EntityDetails entity;
    mc_TxEntity tmp_entity;
    mc_TxEntityStat entStat;
    unsigned char *ptr;

    if(txid == NULL)
    {
        entry.push_back(Pair("streamref", ""));
        return entry;
    }
    
    uint256 hash=*(uint256*)txid;
    if(mc_gState->m_Assets->FindEntityByTxID(&entity,txid))
    {        
        ptr=(unsigned char *)entity.GetName();
        
        if(output_level & 0x001)
        {
            entry.push_back(Pair("type", "stream"));                        
        }
        
        if(ptr && strlen((char*)ptr))
        {
            entry.push_back(Pair("name", string((char*)ptr)));            
        }
        if(output_level & 0x002)
        {
            entry.push_back(Pair("createtxid", hash.GetHex()));
        }
        ptr=(unsigned char *)entity.GetRef();
        string streamref="";
        if(entity.IsUnconfirmedGenesis())
        {
            Value null_value;
            entry.push_back(Pair("streamref",null_value));
        }
        else
        {
            if((int)mc_GetLE(ptr,4))
            {
                streamref += itostr((int)mc_GetLE(ptr,4));
                streamref += "-";
                streamref += itostr((int)mc_GetLE(ptr+4,4));
                streamref += "-";
                streamref += itostr((int)mc_GetLE(ptr+8,2));
            }
            else
            {
                streamref="0-0-0";                
            }
            entry.push_back(Pair("streamref", streamref));
        }

        if(output_level & 0x0004)
        {
            if(entity.AnyoneCanWrite())
            {
                entry.push_back(Pair("open",true));                                
            }
            else
            {
                entry.push_back(Pair("open",false));                                            
            }
        }
        
        
        size_t value_size;
        int64_t offset,new_offset;
        uint32_t value_offset;
        const unsigned char *ptr;

        ptr=entity.GetScript();
        
        Object fields;
        Array openers;
        if(output_level & 0x0004)
        {
            Value vfields;
            vfields=mc_ExtractDetailsJSONObject(&entity);
            if(vfields.type() == null_type)
            {
                offset=0;
                while(offset>=0)
                {
                    new_offset=entity.NextParam(offset,&value_offset,&value_size);
                    if(value_offset > 0)
                    {
                        if(ptr[offset])
                        {
                            string param_name((char*)ptr+offset);
                            string param_value((char*)ptr+value_offset,(char*)ptr+value_offset+value_size);
                            fields.push_back(Pair(param_name, param_value));                                                                        
                        }
                        else
                        {
                            if(ptr[offset+1] == MC_ENT_SPRM_ISSUER)
                            {
                                if(value_size == 24)
                                {
                                    unsigned char tptr[4];
                                    memcpy(tptr,ptr+value_offset+sizeof(uint160),4);
                                    if(mc_GetLE(tptr,4) & MC_PFL_IS_SCRIPTHASH)
                                    {
                                        openers.push_back(CBitcoinAddress(*(CScriptID*)(ptr+value_offset)).ToString());                                                
                                    }
                                    else
                                    {
                                        openers.push_back(CBitcoinAddress(*(CKeyID*)(ptr+value_offset)).ToString());
                                    }
                                }
                            }                        
                        }
                    }
                    offset=new_offset;
                }      
                vfields=fields;
            }
            
            entry.push_back(Pair("details",vfields));                    
        }

        if(output_level & 0x0020)
        {
            entry.push_back(Pair("creators",openers));                    
        }
        if(output_level & 0x0018)
        {
            entStat.Zero();
            memcpy(&entStat,entity.GetTxID()+MC_AST_SHORT_TXID_OFFSET,MC_AST_SHORT_TXID_SIZE);
            entStat.m_Entity.m_EntityType=MC_TET_STREAM | MC_TET_CHAINPOS;
            if(pwalletTxsMain->FindEntity(&entStat))
            {
                if(output_level & 0x0008)
                {
                    entry.push_back(Pair("subscribed",true));                                            
                    if(entStat.m_Flags & MC_EFL_NOT_IN_SYNC)
                    {
                        entry.push_back(Pair("synchronized",false));                                                            
                    }
                    else
                    {
                        entry.push_back(Pair("synchronized",true));                                                                            
                    }
                }
                if(output_level & 0x0010)
                {
                    entry.push_back(Pair("items",(int)entStat.m_LastPos));        
                    entry.push_back(Pair("confirmed",(int)entStat.m_LastClearedPos));        
                    tmp_entity.Zero();
                    memcpy(tmp_entity.m_EntityID,entity.GetTxID()+MC_AST_SHORT_TXID_OFFSET,MC_AST_SHORT_TXID_SIZE);
                    tmp_entity.m_EntityType=MC_TET_STREAM_KEY | MC_TET_CHAINPOS;
                    entry.push_back(Pair("keys",pwalletTxsMain->GetListSize(&tmp_entity,NULL)));        
                    tmp_entity.m_EntityType=MC_TET_STREAM_PUBLISHER | MC_TET_CHAINPOS;
                    entry.push_back(Pair("publishers",pwalletTxsMain->GetListSize(&tmp_entity,NULL)));        
                }
            }
            else
            {
                entry.push_back(Pair("subscribed",false));                                                        
            }
        }
    }
    else
    {
        Value null_value;
        if(output_level & 0x001)
        {
            entry.push_back(Pair("type", "stream"));                        
        }
        entry.push_back(Pair("name",null_value));
        if(output_level & 0x002)
        {
            entry.push_back(Pair("createtxid",null_value));
        }
        entry.push_back(Pair("streamref", null_value));
    }
    
    return entry;
}

map<string, Value> ParamsToUpgrade(mc_EntityDetails *entity,int version)   
{
    map<string, Value> result;
    int size=0;
    const mc_OneMultichainParam *param;
    char* ptr=(char*)entity->GetParamUpgrades(&size);
    char* ptrEnd;
    string param_name;
    int param_size,given_size;
    int64_t param_value;
    if(ptr)
    {
        ptrEnd=ptr+size;
        while(ptr<ptrEnd)
        {
            param=mc_gState->m_NetworkParams->FindParam(ptr);
            ptr+=mc_gState->m_NetworkParams->GetParamFromScript(ptr,&param_value,&given_size);
            
            if(strcmp(ptr,"protocolversion"))
            {        
                if(param)
                {
                    param_size=mc_gState->m_NetworkParams->CanBeUpgradedByVersion(param->m_Name,version,0);
                    if( (param_size > 0) && (param_size == given_size) )
                    {
                        param_name=string(param->m_DisplayName);
                        if(result.find(param_name) == result.end())
                        {
                            if(param->m_Type & MC_PRM_DECIMAL)
                            {
                                result.insert(make_pair(param_name, mc_gState->m_NetworkParams->Int64ToDecimal(param_value)));
                            }
                            else
                            {
                                switch(param->m_Type & MC_PRM_DATA_TYPE_MASK)
                                {
                                    case MC_PRM_BOOLEAN:
                                        result.insert(make_pair(param_name, (param_value != 0) ));
                                        break;
                                    case MC_PRM_INT32:
                                        result.insert(make_pair(param_name, (int)param_value));
                                    case MC_PRM_UINT32:
                                    case MC_PRM_INT64:
                                        result.insert(make_pair(param_name, param_value));
                                        break;
                                }                                
                            }
                        }
                    }
                }
            }
        }
    }
        
    return result;    
}


Object UpgradeEntry(const unsigned char *txid)
{
    Object entry;
    mc_EntityDetails entity;
    unsigned char *ptr;

    if(txid == NULL)
    {
        Value null_value;
        entry.push_back(Pair("name",null_value));
        entry.push_back(Pair("createtxid",null_value));
        return entry;
    }
    
    uint256 hash=*(uint256*)txid;
    if(mc_gState->m_Assets->FindEntityByTxID(&entity,txid))
    {        
        ptr=(unsigned char *)entity.GetName();
        
        if(ptr && strlen((char*)ptr))
        {
            entry.push_back(Pair("name", string((char*)ptr)));            
        }
        entry.push_back(Pair("createtxid", hash.GetHex()));
        Object fields;
        map<string, Value> params_to_upgrade=ParamsToUpgrade(&entity,0);
        if(entity.UpgradeProtocolVersion())
        {
            fields.push_back(Pair("protocol-version",entity.UpgradeProtocolVersion()));                    
        }
        for(map<string,Value>::iterator it = params_to_upgrade.begin(); it != params_to_upgrade.end(); ++it) 
        {
            fields.push_back(Pair(it->first, it->second));  
        }
        entry.push_back(Pair("params",fields));      
        entry.push_back(Pair("startblock",(int64_t)entity.UpgradeStartBlock()));                    
        
    }
    else
    {
        Value null_value;
        entry.push_back(Pair("name",null_value));
        entry.push_back(Pair("createtxid",null_value));
    }
    
    return entry;
}


Value OpReturnEntry(const unsigned char *elem,size_t elem_size,uint256 txid, int vout)
{
    string metadata="";
    Object metadata_object;
    if((int)elem_size <= GetArg("-maxshowndata",MAX_OP_RETURN_SHOWN))
    {
        metadata=HexStr(elem,elem+elem_size);
        return metadata;
    }
    metadata_object.push_back(Pair("txid", txid.ToString()));
    metadata_object.push_back(Pair("vout", vout));
    metadata_object.push_back(Pair("size", (int)elem_size));
    return metadata_object;    
}

string OpReturnFormatToText(int format)
{
    switch(format)
    {
        case MC_SCR_DATA_FORMAT_UTF8:
            return "text";
        case MC_SCR_DATA_FORMAT_UBJSON:
            return "json";            
    }
    return "raw";
}

int mc_IsUTF8(const unsigned char *elem,size_t elem_size)
{
    unsigned char *ptr=(unsigned char *)elem;
    unsigned char *ptrEnd=ptr+elem_size;
    int size;
    while(ptr<ptrEnd)
    {
        size=c_UTF8_charlen[*ptr];
        if(size==0)
        {
            return 0;
        }
        ptr+=size;
    }
    if(ptr>ptrEnd)
    {
        return 0;        
    }
    return 1;
}

Value OpReturnFormatEntry(const unsigned char *elem,size_t elem_size,uint256 txid, int vout, uint32_t format, string *format_text_out)
{
    string metadata="";
    Object metadata_object;
    Value metadata_value;
    int err;
    if( ((int)elem_size <= GetArg("-maxshowndata",MAX_OP_RETURN_SHOWN)) || (txid == 0) )
    {
        if(format_text_out)
        {
            *format_text_out=OpReturnFormatToText(format);
        }
        switch(format)
        {
            case MC_SCR_DATA_FORMAT_UBJSON:
                metadata_value=ubjson_read(elem,elem_size,MAX_FORMATTED_DATA_DEPTH,&err);
                if(err == MC_ERR_NOERROR)
                {
                    metadata_object.push_back(Pair("json",metadata_value));
                    return metadata_object;
                }
                metadata_object.push_back(Pair("json",Value::null));
                return metadata_object;
/*                
                if(format_text_out)
                {
                    *format_text_out=OpReturnFormatToText(MC_SCR_DATA_FORMAT_UNKNOWN);
                }
                metadata=HexStr(elem,elem+elem_size);    
 */ 
//                metadata_object.push_back(Pair("json",metadata));
                break;
            case MC_SCR_DATA_FORMAT_UTF8:
                if(mc_IsUTF8(elem,elem_size) == 0)
                {
                    metadata_object.push_back(Pair("text",Value::null));
                    return metadata_object;                    
                }
                metadata=string(elem,elem+elem_size);
                metadata_object.push_back(Pair("text",metadata));
                return metadata_object;
//                metadata.push_back(0x00);
                break;
            default:                                                            // unknown
                metadata=HexStr(elem,elem+elem_size);
                break;
        }
        return metadata;
    }    
    if(format_text_out)
    {
        *format_text_out="gettxoutdata";
    }
    metadata_object.push_back(Pair("txid", txid.ToString()));
    metadata_object.push_back(Pair("vout", vout));
    metadata_object.push_back(Pair("format", OpReturnFormatToText(format)));
    metadata_object.push_back(Pair("size", (int)elem_size));
    return metadata_object;    
}

Value OpReturnFormatEntry(const unsigned char *elem,size_t elem_size,uint256 txid, int vout, uint32_t format)
{
    Value format_item_value;
    string format_text_str;
    Object result;

    format_item_value=OpReturnFormatEntry(elem,elem_size,txid,vout,format,&format_text_str);
    
    result.push_back(Pair("format", format_text_str));
    result.push_back(Pair("formatdata", format_item_value));
    
    return result;
}

Value DataItemEntry(const CTransaction& tx,int n,set <uint256>& already_seen,uint32_t stream_output_level)
{
    Object entry;
    Array publishers;
    set<uint160> publishers_set;
    Array keys;
    const unsigned char *ptr;
    unsigned char item_key[MC_ENT_MAX_ITEM_KEY_SIZE+1];    
    int item_key_size;
    Value item_value;
    uint32_t format;
    Value format_item_value;
    string format_text_str;
    
    mc_EntityDetails entity;
    uint256 hash;
    
    const CScript& script1 = tx.vout[n].scriptPubKey;        
    CScript::const_iterator pc1 = script1.begin();

    mc_gState->m_TmpScript->Clear();
    mc_gState->m_TmpScript->SetScript((unsigned char*)(&pc1[0]),(size_t)(script1.end()-pc1),MC_SCR_TYPE_SCRIPTPUBKEY);

    
    if(mc_gState->m_TmpScript->IsOpReturnScript() == 0)                      
    {
        return Value::null;
    }
        
    if(mc_gState->m_TmpScript->GetNumElements() == 0)
    {
        return Value::null;
    }
    
    mc_gState->m_TmpScript->ExtractAndDeleteDataFormat(&format);
    
    unsigned char short_txid[MC_AST_SHORT_TXID_SIZE];
    mc_gState->m_TmpScript->SetElement(0);

    if(mc_gState->m_TmpScript->GetEntity(short_txid))           
    {
        return Value::null;
    }
    
    if(mc_gState->m_Assets->FindEntityByShortTxID(&entity,short_txid) == 0)
    {
        return Value::null;
    }                        

    hash=*(uint256*)entity.GetTxID();
    
/*    
    if(already_seen.find(hash) != already_seen.end())
    {
        return Value::null;
    }
*/
    
    for(int e=1;e<mc_gState->m_TmpScript->GetNumElements()-1;e++)
    {
        mc_gState->m_TmpScript->SetElement(e);
                                                                    // Should be spkk
        if(mc_gState->m_TmpScript->GetItemKey(item_key,&item_key_size))   // Item key
        {
            return Value::null;
        }                                            
        item_key[item_key_size]=0;
        keys.push_back(string(item_key,item_key+item_key_size));
    }

    size_t elem_size;
    const unsigned char *elem;

    elem = mc_gState->m_TmpScript->GetData(mc_gState->m_TmpScript->GetNumElements()-1,&elem_size);
    item_value=OpReturnEntry(elem,elem_size,tx.GetHash(),n);
	format_item_value=OpReturnFormatEntry(elem,elem_size,tx.GetHash(),n,format,&format_text_str);
    
    already_seen.insert(hash);
    
    publishers_set.clear();
    for (int i = 0; i < (int)tx.vin.size(); ++i)
    {
        int op_addr_offset,op_addr_size,is_redeem_script,sighash_type;
                    
        const CScript& script2 = tx.vin[i].scriptSig;        
        CScript::const_iterator pc2 = script2.begin();
                                          
        ptr=mc_ExtractAddressFromInputScript((unsigned char*)(&pc2[0]),(int)(script2.end()-pc2),&op_addr_offset,&op_addr_size,&is_redeem_script,&sighash_type,0);
        if(ptr)
        {
            if( (sighash_type == SIGHASH_ALL) || ( (sighash_type == SIGHASH_SINGLE) && (i == n) ) )
            {
                uint160 publisher_hash=Hash160(ptr+op_addr_offset,ptr+op_addr_offset+op_addr_size);
                if(publishers_set.count(publisher_hash) == 0)
                {
                    publishers_set.insert(publisher_hash);
                    if(is_redeem_script)
                    {
                        publishers.push_back(CBitcoinAddress((CScriptID)publisher_hash).ToString());
                    }
                    else
                    {
                        publishers.push_back(CBitcoinAddress((CKeyID)publisher_hash).ToString());                    
                    }
                }
            }
        }        
    }

    entry=StreamEntry((unsigned char*)&hash,stream_output_level);
    entry.push_back(Pair("publishers", publishers));
    entry.push_back(Pair("keys", keys));
    
    if(mc_gState->m_Compatibility & MC_VCM_1_0)
    {
        entry.push_back(Pair("key", keys[0]));        
    }
    entry.push_back(Pair("data", format_item_value));        
    return entry;
}


Object AssetEntry(const unsigned char *txid,int64_t quantity,uint32_t output_level)
{
// output_level constants
// 0x0000 minimal: name, assetref, non-negative qty, negative actual issueqty    
// 0x0001 raw 
// 0x0002 multiple, units, open, details, permissions
// 0x0004 issuetxid,     
// 0x0008 subscribed/synchronized    
// 0x0020 issuers
// 0x0040 put given quantity into qty field, even negative
// 0x0080 put issueqty into qty field
    Object entry;
    mc_EntityDetails entity;
    mc_EntityDetails genesis_entity;
    mc_TxEntityStat entStat;
    unsigned char *ptr;

    if(txid == NULL)
    {
        entry.push_back(Pair("assetref", ""));
        entry.push_back(Pair("qty", ValueFromAmount(quantity)));
        entry.push_back(Pair("raw", quantity));
        return entry;
    }
    
    uint256 hash=*(uint256*)txid;
    
    if(mc_gState->m_Assets->FindEntityByTxID(&entity,txid))
    {
        if(entity.IsFollowOn())
        {
            mc_gState->m_Assets->FindEntityByFollowOn(&genesis_entity,txid);
        }
        else
        {
            memcpy(&genesis_entity,&entity,sizeof(mc_EntityDetails));
        }
        
        ptr=(unsigned char *)genesis_entity.GetName();
        if(ptr && strlen((char*)ptr))
        {
            entry.push_back(Pair("name", string((char*)ptr)));            
        }
        if(output_level & 0x0004)
        {
            entry.push_back(Pair("issuetxid", hash.GetHex()));
        }
        ptr=(unsigned char *)genesis_entity.GetRef();
        string assetref="";
        if(genesis_entity.IsUnconfirmedGenesis())
        {
            Value null_value;
            entry.push_back(Pair("assetref",null_value));
        }
        else
        {
            assetref += itostr((int)mc_GetLE(ptr,4));
            assetref += "-";
            assetref += itostr((int)mc_GetLE(ptr+4,4));
            assetref += "-";
            assetref += itostr((int)mc_GetLE(ptr+8,2));
            entry.push_back(Pair("assetref", assetref));
        }

        size_t value_size;
        int64_t offset,new_offset;
        int64_t raw_output;
        uint32_t value_offset;
        uint64_t multiple=1;
        const unsigned char *ptr;
        double units=1.;
        uint32_t permissions;

        ptr=entity.GetScript();
        multiple=genesis_entity.GetAssetMultiple();
        permissions=genesis_entity.Permissions();
        units= 1./(double)multiple;
        if(output_level & 0x0002)
        {
            if(!entity.IsFollowOn())
            {
                entry.push_back(Pair("multiple", multiple));                                        
                entry.push_back(Pair("units",units));                              
                if(entity.AllowedFollowOns())
                {
                    entry.push_back(Pair("open",true));                                
                }
                else
                {
                    entry.push_back(Pair("open",false));                                            
                }
                if(mc_gState->m_Features->PerAssetPermissions())
                {
                    Object pObject;
                    pObject.push_back(Pair("send",(permissions & MC_PTP_SEND) ? true : false));
                    pObject.push_back(Pair("receive",(permissions & MC_PTP_RECEIVE) ? true : false));
                    entry.push_back(Pair("restrict",pObject));                                            
                }
            }
        }
        
        Object fields;
        Value vfields;
        if(output_level & 0x0002)
        {
            vfields=mc_ExtractDetailsJSONObject(&entity);
            if(vfields.type() == null_type)
            {
                offset=0;
                while(offset>=0)
                {
                    new_offset=entity.NextParam(offset,&value_offset,&value_size);
                    if(value_offset > 0)
                    {
                        if(ptr[offset])
                        {
                            if(ptr[offset] != 0xff)
                            {
                                string param_name((char*)ptr+offset);
                                string param_value((char*)ptr+value_offset,(char*)ptr+value_offset+value_size);
                                fields.push_back(Pair(param_name, param_value));                                                                        
                            }
                        }
                    }
                    offset=new_offset;
                }      
                vfields=fields;
            }
        }
        if(output_level & 0x0002)
        {
            entry.push_back(Pair("details",vfields));                    
        }

        Array issues;
        int64_t total=0;
        if(( (output_level & 0x0020) !=0 )|| mc_gState->m_Assets->HasFollowOns(txid))
        {
            int64_t qty;
            mc_Buffer *followons;
            followons=mc_gState->m_Assets->GetFollowOns(txid);
            for(int i=followons->GetCount()-1;i>=0;i--)
            {
                Object issue;
                mc_EntityDetails followon;
                if(mc_gState->m_Assets->FindEntityByTxID(&followon,followons->GetRow(i)))
                {
                    qty=followon.GetQuantity();
                    total+=qty;
                    if(output_level & 0x0020)
                    {
                        issue.push_back(Pair("txid", ((uint256*)(followon.GetTxID()))->ToString().c_str()));    
                        issue.push_back(Pair("qty", (double)qty*units));
                        issue.push_back(Pair("raw", qty));                    

                        Object followon_fields;
                        Array followon_issuers;

                        vfields=mc_ExtractDetailsJSONObject(&followon);
                        ptr=followon.GetScript();
                        offset=0;
                        while(offset>=0)
                        {
                            new_offset=followon.NextParam(offset,&value_offset,&value_size);
                            if(value_offset > 0)
                            {
                                if(ptr[offset])
                                {
                                    if(ptr[offset] != 0xff)
                                    {
                                        string param_name((char*)ptr+offset);
                                        string param_value((char*)ptr+value_offset,(char*)ptr+value_offset+value_size);
                                        followon_fields.push_back(Pair(param_name, param_value));                                                                        
                                    }                            
                                }
                                else
                                {
                                    if(ptr[offset+1] == MC_ENT_SPRM_ISSUER)
                                    {
                                        if(value_size == 20)
                                        {
                                            followon_issuers.push_back(CBitcoinAddress(*(CKeyID*)(ptr+value_offset)).ToString());
                                        }
                                        if(value_size == 24)
                                        {
                                            unsigned char tptr[4];
                                            memcpy(tptr,ptr+value_offset+sizeof(uint160),4);
                                            if(mc_GetLE(tptr,4) & MC_PFL_IS_SCRIPTHASH)
                                            {
                                                followon_issuers.push_back(CBitcoinAddress(*(CScriptID*)(ptr+value_offset)).ToString());                                                
                                            }
                                            else
                                            {
                                                followon_issuers.push_back(CBitcoinAddress(*(CKeyID*)(ptr+value_offset)).ToString());
                                            }
                                        }
                                    }
                                }
                            }
                            offset=new_offset;
                        }      

                        if(vfields.type() == null_type)
                        {                        
                            vfields=followon_fields;
                        }
                        issue.push_back(Pair("details",vfields));                    
                        issue.push_back(Pair("issuers",followon_issuers));                    
                        issues.push_back(issue);                    
                    }
                }            
            }
            mc_gState->m_Assets->FreeEntityList(followons);
        }
        else
        {
            total=entity.GetQuantity();
        }
        
        raw_output=quantity;
        if(output_level & 0x0080)
        {
            raw_output=entity.GetQuantity();
            entry.push_back(Pair("qty", (double)raw_output*units));
            entry.push_back(Pair("raw", raw_output));                                    
        }
        else
        {
            if( (raw_output < 0) && ( (output_level & 0x0040) == 0) )
            {
                raw_output=total;
                entry.push_back(Pair("issueqty", (double)raw_output*units));
                entry.push_back(Pair("issueraw", raw_output));                                    
            }
            else
            {
                entry.push_back(Pair("qty", (double)raw_output*units));
                if(output_level & 0x0001)
                {
                    entry.push_back(Pair("raw", raw_output));                                    
                }
            }
        }
        
        
        if( ((output_level & 0x0008) != 0) && ((mc_gState->m_WalletMode & MC_WMD_TXS) != 0) )
        {
            entStat.Zero();
            memcpy(&entStat,genesis_entity.GetShortRef(),mc_gState->m_NetworkParams->m_AssetRefSize);
            entStat.m_Entity.m_EntityType=MC_TET_ASSET | MC_TET_CHAINPOS;
            if(pwalletTxsMain->FindEntity(&entStat))
            {
                entry.push_back(Pair("subscribed",true));                                            
                if(entStat.m_Flags & MC_EFL_NOT_IN_SYNC)
                {
                    entry.push_back(Pair("synchronized",false));                                                            
                }
                else
                {
                    entry.push_back(Pair("synchronized",true));                                                                            
                }
                entry.push_back(Pair("transactions",(int)entStat.m_LastPos));        
                entry.push_back(Pair("confirmed",(int)entStat.m_LastClearedPos));        
            }
            else
            {
                entry.push_back(Pair("subscribed",false));                                                        
            }
        }
        
        if(output_level & 0x0020)
        {
            entry.push_back(Pair("issues",issues));                    
        }
    }
    else
    {
        Value null_value;
        entry.push_back(Pair("name",null_value));
        entry.push_back(Pair("issuetxid",null_value));
        entry.push_back(Pair("assetref", null_value));
        entry.push_back(Pair("qty", null_value));
        entry.push_back(Pair("raw", quantity));
    }
    
    return entry;
}


string ParseRawOutputObject(Value param,CAmount& nAmount,mc_Script *lpScript, int *required,int *eErrorCode)
{
    string strError="";
    unsigned char buf[MC_AST_ASSET_FULLREF_BUF_SIZE];
    mc_Buffer *lpBuffer=mc_gState->m_TmpBuffers->m_RpcABNoMapBuffer1;
    lpBuffer->Clear();
    mc_Buffer *lpFollowonBuffer=mc_gState->m_TmpBuffers->m_RpcABNoMapBuffer2;
    lpFollowonBuffer->Clear();
    int assets_per_opdrop=(MAX_STANDARD_TX_SIZE)/(mc_gState->m_NetworkParams->m_AssetRefSize+MC_AST_ASSET_QUANTITY_SIZE);
    int32_t verify_level=-1;
    int asset_error=0;
    int multiple;
    int64_t max_block=0xffffffff;
    string asset_name;
    string type_string;
    nAmount=0;
    
    if(eErrorCode)
    {
        *eErrorCode=RPC_INVALID_PARAMETER;
    }
    
    memset(buf,0,MC_AST_ASSET_FULLREF_BUF_SIZE);
    
    if(mc_gState->m_Features->VerifySizeOfOpDropElements())
    {        
        if(mc_gState->m_Features->VerifySizeOfOpDropElements())
        {
            assets_per_opdrop=(MAX_SCRIPT_ELEMENT_SIZE-4)/(mc_gState->m_NetworkParams->m_AssetRefSize+MC_AST_ASSET_QUANTITY_SIZE);
        }
    }
    
    BOOST_FOREACH(const Pair& a, param.get_obj()) 
    {
        if( (a.value_.type() == obj_type) ||
            (( (a.value_.type() == str_type) || (a.value_.type() == array_type) ) && (a.name_== "data")) )
        {
            bool parsed=false;
            
            if(!parsed && (a.name_ == "hidden_verify_level"))
            {
                BOOST_FOREACH(const Pair& d, a.value_.get_obj()) 
                {
                    bool field_parsed=false;
                    if(!field_parsed && (d.name_ == "value"))
                    {                        
                        verify_level=d.value_.get_int();
                        field_parsed=true;
                    }
                    if(!field_parsed)
                    {
                        strError=string("Invalid field for object ") + a.name_ + string(": ") + d.name_;
                        goto exitlbl;
                    }
                }
                parsed=true;
            }
            
            if(!parsed && (a.name_ == "issue"))
            {
                int64_t quantity=-1;
                BOOST_FOREACH(const Pair& d, a.value_.get_obj()) 
                {
                    bool field_parsed=false;
                    if(!field_parsed && (d.name_ == "raw"))
                    {                 
                        if (d.value_.type() != null_type)
                        {
                            quantity=d.value_.get_int64();
                            if(quantity<0)
                            {
                                strError=string("Negative value for issue raw qty");
                                goto exitlbl;                                
                            }
                        }                
                        else
                        {
                            strError=string("Invalid value for issue raw qty");
                            goto exitlbl;
                            
                        }
                        field_parsed=true;
                    }
                    if(!field_parsed)
                    {
                        strError=string("Invalid field for object ") + a.name_ + string(": ") + d.name_;
                        goto exitlbl;
                    }
                }
                if(quantity < 0)
                {
                    strError=string("Issue raw qty not specified");
                    goto exitlbl;                    
                }
                lpScript->SetAssetGenesis(quantity);        
                parsed=true;
            }
            
            if(!parsed && (a.name_ == "issuemore"))
            {
                int64_t quantity=-1;
                asset_name="";
                BOOST_FOREACH(const Pair& d, a.value_.get_obj()) 
                {
                    bool field_parsed=false;
                    if(!field_parsed && (d.name_ == "raw"))
                    {                 
                        if (d.value_.type() != null_type)
                        {
                            quantity=d.value_.get_int64();
                            if(quantity<0)
                            {
                                strError=string("Negative value for issuemore raw qty");
                                goto exitlbl;                                
                            }
                        }                
                        else
                        {
                            strError=string("Invalid value for issuemore raw qty");
                            goto exitlbl;
                            
                        }
                        field_parsed=true;
                    }
                    if(!field_parsed && (d.name_ == "asset"))                        
                    {                 
                        if(d.value_.type() != null_type && !d.value_.get_str().empty())
                        {
                            asset_name=d.value_.get_str();
                        }
                        if(asset_name.size())
                        {
                            asset_error=ParseAssetKeyToFullAssetRef(asset_name.c_str(),buf,&multiple,NULL, MC_ENT_TYPE_ASSET);
                            if(asset_error)
                            {
                                goto exitlbl;
                            }                                                    
                            field_parsed=true;
                        }
                    }
                    if(!field_parsed)
                    {
                        strError=string("Invalid field for object ") + a.name_ + string(": ") + d.name_;
                        goto exitlbl;
                    }
                }
                if(asset_name.size() == 0)
                {
                    strError=string("Issuemore asset not specified");
                    goto exitlbl;                    
                }
                if(quantity < 0)
                {
                    strError=string("Issuemore raw qty not specified");
                    goto exitlbl;                    
                }
                if(lpFollowonBuffer->GetCount())
                {
                    if(verify_level & 0x0008)
                    {
                        if(memcmp(buf,lpFollowonBuffer->GetRow(0),MC_AST_ASSET_QUANTITY_OFFSET))
                        {
                            strError=string("Issuemore for different assets");
                            goto exitlbl;                                                
                        }
                    }
                    lpFollowonBuffer->Clear();
                }
                mc_SetABQuantity(buf,quantity);
                lpFollowonBuffer->Add(buf);                    
                lpScript->SetAssetQuantities(lpFollowonBuffer,MC_SCR_ASSET_SCRIPT_TYPE_FOLLOWON);                
                parsed=true;
            }

            if(!parsed && (a.name_ == "data"))
            {
                Array arr;
                if( (a.value_.type() == str_type) || (a.value_.type() == obj_type) )
                {
                    arr.push_back(a.value_);
                }
                else
                {
                    if(a.value_.type() == array_type)
                    {
                        arr=a.value_.get_array();
                    }
                }
                
                for(int i=0;i<(int)arr.size();i++)
                {
                    uint32_t data_format=MC_SCR_DATA_FORMAT_UNKNOWN;
                    int errorCode=RPC_INVALID_PARAMETER;

                    mc_gState->m_TmpScript->Clear();

                    vector<unsigned char> vData=ParseRawFormattedData(&(arr[i]),&data_format,mc_gState->m_TmpScript,true,&errorCode,&strError);
                    if(strError.size())
                    {
                        if(eErrorCode)
                        {
                            *eErrorCode=errorCode;
                        }
                        goto exitlbl;
                    }

                    if(data_format != MC_SCR_DATA_FORMAT_UNKNOWN)
                    {
                        lpScript->SetDataFormat(data_format);                    
                    }
                    lpScript->SetRawData(&(vData[0]),(int)vData.size());                    
                }
                parsed=true;
            }
            
            if(!parsed && (a.name_ == "permissions"))
            {
                uint32_t type,from,to,timestamp;
                int64_t v;
                mc_EntityDetails entity;
                entity.Zero();
                
                type_string="";
                type=0;
                from=0;
                to=4294967295U;
                timestamp=mc_TimeNowAsUInt();
    
                BOOST_FOREACH(const Pair& d, a.value_.get_obj()) 
                {
                    bool field_parsed=false;
                    
                    if(!field_parsed && (d.name_ == "for"))
                    {                 
                        entity.Zero();
                        if(d.value_.type() != null_type && !d.value_.get_str().empty())
                        {
                            ParseEntityIdentifier(d.value_,&entity, MC_ENT_TYPE_ANY);           
                        }
                        field_parsed=true;
                    }                    
                    if(!field_parsed && (d.name_ == "type"))
                    {       
                        if(d.value_.type() == str_type && !d.value_.get_str().empty())
                        {
                            type_string=d.value_.get_str();                            
                        }                
                        else
                        {
                            strError=string("Invalid value for permission type");
                            goto exitlbl;
                            
                        }
                        field_parsed=true;
                    }
                    if(!field_parsed && (d.name_ == "startblock"))
                    {                 
                        if (d.value_.type() != null_type)
                        {
                            
                            v=d.value_.get_int64();
                            if(v<0)
                            {
                                strError=string("Negative value for permissions startblock");
                                goto exitlbl;                                
                            }
                            if(v>max_block)
                            {
                                strError=string("Invalid value for permissions endblock");
                                goto exitlbl;                                
                            }
                            from=v;
                        }                
                        else
                        {
                            strError=string("Invalid value for permissions startblock");
                            goto exitlbl;
                            
                        }
                        field_parsed=true;
                    }
                    if(!field_parsed && (d.name_ == "endblock"))
                    {                 
                        if (d.value_.type() != null_type)
                        {
                            v=d.value_.get_int64();
                            if(v<0)
                            {
                                strError=string("Negative value for permissions endblock");
                                goto exitlbl;                                
                            }
                            if(v>max_block)
                            {
                                strError=string("Invalid value for permissions endblock");
                                goto exitlbl;                                
                            }
                            to=v;
                        }                
                        else
                        {
                            strError=string("Invalid value for permissions endblock");
                            goto exitlbl;
                            
                        }
                        field_parsed=true;
                    }
                    if(!field_parsed && (d.name_ == "timestamp"))
                    {                 
                        if (d.value_.type() != null_type)
                        {
                            timestamp=(uint32_t)d.value_.get_uint64();
                        }                
                        else
                        {
                            strError=string("Invalid value for permissions timestamp");
                            goto exitlbl;
                            
                        }
                        field_parsed=true;
                    }
                    if(!field_parsed)
                    {
                        strError=string("Invalid field for object ") + a.name_ + string(": ") + d.name_;
                        goto exitlbl;
                    }
                }
                
                if(type_string.size())
                {
                    type=mc_gState->m_Permissions->GetPermissionType(type_string.c_str(),&entity);
//                    type=mc_gState->m_Permissions->GetPermissionType(type_string.c_str(),entity.GetEntityType());
                    if(entity.GetEntityType() == MC_ENT_TYPE_NONE)
                    {
                        if(required)
                        {
                            *required |= type;
                        }
                    }
                    if(type == 0)
                    {
                        strError=string("Invalid value for permission type: ") + type_string;
                        goto exitlbl;                                
                    }
                }        
                
                if(type == 0)
                {
                    strError=string("Permission type not specified");
                    goto exitlbl;                    
                }
                
                if(entity.GetEntityType())                
                {
                    lpScript->SetEntity(entity.GetTxID()+MC_AST_SHORT_TXID_OFFSET);                            
                }
                lpScript->SetPermission(type,from,to,timestamp);
                parsed=true;
            }
        
            if(!parsed)
            {
                strError=string("Invalid object: ") + a.name_;
                goto exitlbl;                
            }
        }
        else
        {
            if(a.name_.size())
            {
                asset_name=a.name_;
                asset_error=ParseAssetKeyToFullAssetRef(asset_name.c_str(),buf,&multiple,NULL, MC_ENT_TYPE_ASSET);
                if(asset_error)
                {
                    goto exitlbl;
                }
                int64_t quantity = (int64_t)(a.value_.get_real() * multiple + 0.499999);                        
                if(verify_level & 0x0010)
                {
                    if(quantity < 0)
                    {
                        strError=string("Negative asset quantity");
                        goto exitlbl;                                        
                    }
                }
                mc_SetABQuantity(buf,quantity);
                lpBuffer->Add(buf);                    
                if(verify_level & 0x0001)
                {
                    if(lpBuffer->GetCount() >= assets_per_opdrop)
                    {
                        lpScript->SetAssetQuantities(lpBuffer,MC_SCR_ASSET_SCRIPT_TYPE_TRANSFER);                
                        lpBuffer->Clear();
                    }
                }
            }            
            else
            {
                nAmount += AmountFromValue(a.value_);                
            }
        }
    }
    
    if(lpBuffer->GetCount())
    {
        if(Params().RequireStandard())
        {
            if(verify_level & 0x0002)
            {
                if(lpBuffer->GetCount() > assets_per_opdrop)
                {
                    strError=string("Too many assets in one group");
                    goto exitlbl;                                    
                }
            }
        }
        lpScript->SetAssetQuantities(lpBuffer,MC_SCR_ASSET_SCRIPT_TYPE_TRANSFER);                
        lpBuffer->Clear();
    }

    if(verify_level & 0x0004)
    {
        if(Params().RequireStandard())
        {
            if(lpScript->GetNumElements() > MCP_STD_OP_DROP_COUNT)
            {
                strError=string("Too many objects in output");
                goto exitlbl;                                                
            }
        }
    }
    
exitlbl:
                    
    switch(asset_error)
    {
        case MC_ASSET_KEY_INVALID_TXID:
            if(eErrorCode)
            {
                *eErrorCode=RPC_ENTITY_NOT_FOUND;
            }
            strError=string("Issue transaction with this txid not found: ")+asset_name;
            break;
        case MC_ASSET_KEY_INVALID_REF:
            if(eErrorCode)
            {
                *eErrorCode=RPC_ENTITY_NOT_FOUND;
            }
            strError=string("Issue transaction with this asset reference not found: ")+asset_name;
            break;
        case MC_ASSET_KEY_INVALID_NAME:
            if(eErrorCode)
            {
                *eErrorCode=RPC_ENTITY_NOT_FOUND;
            }
            strError=string("Issue transaction with this name not found: ")+asset_name;
            break;
        case MC_ASSET_KEY_INVALID_SIZE:
            strError=string("Could not parse asset key: ")+asset_name;
            break;
        case MC_ASSET_KEY_UNCONFIRMED_GENESIS:
            if(eErrorCode)
            {
                *eErrorCode=RPC_UNCONFIRMED_ENTITY;
            }
            strError=string("Unconfirmed asset: ")+asset_name;
            break;
    }
                    
    return strError;
    
}

string ParseRawOutputObject(Value param,CAmount& nAmount,mc_Script *lpScript,int *eErrorCode)
{
    return ParseRawOutputObject(param,nAmount,lpScript,NULL,eErrorCode);
}

bool FindPreparedTxOut(CTxOut& txout,COutPoint outpoint,string& reason)
{
    {
        LOCK(mempool.cs); // protect pool.mapNextTx
        if (mempool.mapNextTx.count(outpoint))
        {
            reason="Conflicts with in-memory transactions";
            return false;
        }
    }
    
    {
        CCoinsView dummy;
        CCoinsViewCache view(&dummy);

        {
            LOCK(mempool.cs);
            CCoinsViewMemPool viewMemPool(pcoinsTip, mempool);
            view.SetBackend(viewMemPool);
            
            if (!view.HaveCoins(outpoint.hash)) 
            {
                reason="Missing inputs";
                return false;
            }
            else
            {
                const CCoins* coins = view.AccessCoins(outpoint.hash);
                if (!coins || !coins->IsAvailable(outpoint.n)) 
                {
                    reason="Input already spent";
                    return false;
                }
                else
                {
                    txout=coins->vout[outpoint.n];                        
                }
            }                                

            view.GetBestBlock();

            // we have all inputs cached now, so switch back to dummy, so we don't need to keep lock on mempool
            view.SetBackend(dummy);
        }
    }

    return true;
}

bool GetTxInputsAsTxOuts(const CTransaction& tx, vector <CTxOut>& inputs, vector <string>& errors,string& reason)
{
    bool result=true;
    
    reason="";
    
    inputs.clear();
    errors.clear();
    
    inputs.resize(tx.vin.size());
    errors.resize(tx.vin.size());
    
    // Check for conflicts with in-memory transactions
    {
    LOCK(mempool.cs); // protect pool.mapNextTx
    for (unsigned int i = 0; i < tx.vin.size(); i++)
    {
        COutPoint outpoint = tx.vin[i].prevout;
        if (mempool.mapNextTx.count(outpoint))
        {
            result=false;
            errors[i]="Conflicts with in-memory transactions";
        }
    }
    }
    
    {
        CCoinsView dummy;
        CCoinsViewCache view(&dummy);

        {
            LOCK(mempool.cs);
            CCoinsViewMemPool viewMemPool(pcoinsTip, mempool);
            view.SetBackend(viewMemPool);
            
            for (unsigned int i = 0; i < tx.vin.size(); i++)
            {
                COutPoint outpoint = tx.vin[i].prevout;
                
                if (!view.HaveCoins(outpoint.hash)) 
                {
                    result=false;
                    errors[i]="Missing inputs";                        
                }
                else
                {
                    const CCoins* coins = view.AccessCoins(outpoint.hash);
                    if (!coins || !coins->IsAvailable(outpoint.n)) 
                    {
                        result=false;
                        errors[i]="Input already spent";                        
                    }
                    else
                    {
                        if(errors[i].size() == 0)
                        {
                            inputs[i]=coins->vout[outpoint.n];                        
                        }
                    }
                }                                
            }

            view.GetBestBlock();

            // we have all inputs cached now, so switch back to dummy, so we don't need to keep lock on mempool
            view.SetBackend(dummy);
        }
    }
    
    return result;    
}

CScript GetScriptForString(string source)
{
    vector <string> destinations; 
    
    string tok;
    
    stringstream ss(source); 
    while(getline(ss, tok, ',')) 
    {
        destinations.push_back(tok);
    }    
    
    if(destinations.size() == 0)
    {
        throw runtime_error(" Address cannot be empty");        
    }   
    
    if(destinations.size() == 1)
    {
        CBitcoinAddress address(destinations[0]);
        if (pwalletMain && address.IsValid())
        {
            return GetScriptForDestination(address.Get());
        }
        else
        {
            if (IsHex(destinations[0]))
            {
                CPubKey vchPubKey(ParseHex(destinations[0]));
                if (!vchPubKey.IsFullyValid())
                    throw runtime_error(" Invalid public key: "+destinations[0]);
                return GetScriptForPubKey(vchPubKey);
            }
            else
            {
                throw runtime_error(" Invalid public key: "+destinations[0]);
            }        
        }
    }
    
    int required=atoi(destinations[0]);        
    if( (required <= 0) || (required > 16) )
        throw runtime_error(" Invalid required for bare multisig: "+destinations[0]);

    if(required > (int)destinations.size()-1)
        throw runtime_error(" To few public keys");

    vector <CPubKey> vPubKeys;
    for(int i=1;i<(int)destinations.size();i++)
    {
        CPubKey vchPubKey(ParseHex(destinations[i]));
        if (!vchPubKey.IsFullyValid())
            throw runtime_error(" Invalid public key: "+destinations[i]);
        vPubKeys.push_back(vchPubKey);
    }        
        
    return GetScriptForMultisig(required,vPubKeys);
}



vector <pair<CScript, CAmount> > ParseRawOutputMultiObject(Object sendTo,int *required)
{
    vector <pair<CScript, CAmount> > vecSend;
    
    set<CBitcoinAddress> setAddress;
    BOOST_FOREACH(const Pair& s, sendTo) 
    {        
        if(mc_gState->m_NetworkParams->IsProtocolMultichain() == 0)
        {
            CBitcoinAddress address(s.name_);        
            if (address.IsValid())
            {            
                if (setAddress.count(address))
                    throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, duplicated address: "+s.name_);
                setAddress.insert(address);            
            }
        }
        CScript scriptPubKey = GetScriptForString(s.name_);
        
        CAmount nAmount;
        if (s.value_.type() != obj_type)
        {
            nAmount = AmountFromValue(s.value_);
        }
        else
        {
            mc_Script *lpScript=mc_gState->m_TmpBuffers->m_RpcScript4;
            lpScript->Clear();
//            uint256 offer_hash;
            size_t elem_size;
            const unsigned char *elem;

            nAmount=0;
            int eErrorCode;

            string strError=ParseRawOutputObject(s.value_,nAmount,lpScript, required,&eErrorCode);
            if(strError.size())
            {
                throw JSONRPCError(eErrorCode, strError);                            
            }

/*            
            if(lpScript->GetNumElements() > MCP_STD_OP_DROP_COUNT )
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid number of elements in script");
*/
            
            for(int element=0;element < lpScript->GetNumElements();element++)
            {
                elem = lpScript->GetData(element,&elem_size);
                if(elem)
                {
                    scriptPubKey << vector<unsigned char>(elem, elem + elem_size) << OP_DROP;
                }
                else
                    throw JSONRPCError(RPC_INTERNAL_ERROR, "Invalid script");
            }                
        }

        vecSend.push_back(make_pair(scriptPubKey, nAmount));
        
    }
    
    return vecSend;
}



CScript ParseRawMetadataNotRefactored(Value param,uint32_t allowed_objects,mc_EntityDetails *given_entity,mc_EntityDetails *found_entity)
{
// codes for allowed_objects fields    
// 0x0001 - create    
// 0x0002 - publish    
// 0x0004 - issue
// 0x0008 - follow-on
// 0x0010 - pure details
// 0x0020 - approval
// 0x0040 - create upgrade
// 0x0100 - encode empty hex
// 0x0200 - raw hex
// 0x1000 - cache input script
    
    CScript scriptOpReturn=CScript();
    if(found_entity)
    {
        found_entity->Zero();
    }
    
    if(param.type() == obj_type)
    {
        mc_Script *lpDetailsScript;
        mc_Script *lpDetails;
        lpDetailsScript=new mc_Script;
        lpDetails=new mc_Script;
        lpDetails->AddElement();
        int format=0;
        string entity_name="";
        int new_type=0;
        int multiple=1;
        int is_open=0;
        bool multiple_is_set=false;
        bool open_is_set=false;
        string strError="";
        mc_EntityDetails entity;
        vector<unsigned char> vKey;
        vector<unsigned char> vValue;
        uint32_t data_format;
        Value formatted_data=Value::null;
        data_format=MC_SCR_DATA_FORMAT_UNKNOWN;
        int protocol_version=-1;
        uint32_t startblock=0;
        int approve=-1;
        bool startblock_is_set=false;
//        bool key_is_set=false;
//        bool value_is_set=false;

        
        format=0;
        entity.Zero();
        BOOST_FOREACH(const Pair& d, param.get_obj()) 
        {              
            if(d.name_ == "name")
            {
                format |= 1;
            }
            if(d.name_ == "multiple")
            {
                format |= 2;
            }
        }
        
        BOOST_FOREACH(const Pair& d, param.get_obj()) 
        {              
            bool parsed=false;

            if(d.name_ == "inputcache")
            {
                new_type=-3;
                if( ((allowed_objects & 0x0200) == 0) || (mc_gState->m_Features->CachedInputScript() == 0) )
                {
                    strError=string("Keyword not allowed in this API");                                                
                }
                else
                {
                    if(d.value_.type() != array_type)
                    {
                        strError=string("Array should be specified for inputcache");                                                
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
                            if(strError.size() == 0)
                            {
                                if(csa[csi].type() != obj_type)
                                {
                                    strError=string("Elements of inputcache should be objects");                                                
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
                                            strError=string("vin should be integer");                                                                                            
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
                                            strError=string("scriptPubKey should be string");                                                                                            
                                        }
                                        else
                                        {
                                            cs_script=csf.value_.get_str();
                                            cs_size=cs_script.size()/2;
                                        } 
                                    }
                                    if(!cs_parsed)
                                    {
                                        strError=string("Invalid field: ") + csf.name_;                                                                                    
                                    }
                                }
                                if(strError.size() == 0)
                                {
                                    if(cs_vin<0)
                                    {
                                        strError=string("Missing vin field");                                                                                                                            
                                    }
                                }
                                if(strError.size() == 0)
                                {
                                    if(cs_size<0)
                                    {
                                        strError=string("Missing scriptPubKey field");                                                                                                                            
                                    }
                                }                                
                                if(strError.size() == 0)
                                {
                                    bool fIsHex;
                                    vector<unsigned char> dataData(ParseHex(cs_script.c_str(),fIsHex));    
                                    if(!fIsHex)
                                    {
                                        strError=string("scriptPubKey should be hexadecimal string");                                                                                                                            
                                    }                                    
                                    else
                                    {
                                        lpDetails->SetCachedScript(cs_offset,&cs_offset,cs_vin,&dataData[0],cs_size);                                        
                                    }
                                }
                            }
                        }
                    }
                }
                parsed=true;
            }
            
            if(d.name_ == "format")
            {
                data_format=MC_SCR_DATA_FORMAT_UNKNOWN;
                if(d.value_.type() != null_type && !d.value_.get_str().empty())
                {
                    if(d.value_.get_str() == "text")
                    {
                        data_format=MC_SCR_DATA_FORMAT_UTF8;                        
                    }
                    if(d.value_.get_str() == "json")
                    {
                        data_format=MC_SCR_DATA_FORMAT_UBJSON;                        
                    }
                    if(data_format == MC_SCR_DATA_FORMAT_UNKNOWN)
                    {
                        strError=string("Invalid format");                                                    
                    }
                }
                else
                {
                    strError=string("Invalid format");                            
                }
                parsed=true;
            }
            
            if(d.name_ == "create")
            {
                if(new_type != 0)
                {
                    strError=string("Only one of the following keywords can appear in the object: create, update, for, format");                                                                        
                }
                if(d.value_.type() != null_type && !d.value_.get_str().empty())
                {
                    if(d.value_.get_str() == "stream")
                    {
                        if((allowed_objects & 0x0001) == 0)
                        {
                            strError=string("Keyword not allowed in this API");                                                
                        }
                        new_type=MC_ENT_TYPE_STREAM;
                    }
                    else
                    {
                        if(d.value_.get_str() == "asset")
                        {
                            if((allowed_objects & 0x0004) == 0)
                            {
                                strError=string("Keyword not allowed in this API");                                                
                            }
                            new_type=MC_ENT_TYPE_ASSET;
                        }
                        else
                        {
                            if(d.value_.get_str() == "upgrade")
                            {
                                if((allowed_objects & 0x0040) == 0)
                                {
                                    strError=string("Keyword not allowed in this API");                                                
                                }
                                new_type=MC_ENT_TYPE_UPGRADE;
                            }
                            else
                            {
                                strError=string("Invalid new entity type");                                                    
                            }
                        }
                    }
                }
                else
                {
                    strError=string("Invalid new entity type");                            
                }
                parsed=true;
            }
            
            if(d.name_ == "for")
            {                 
                if(new_type != 0)
                {
                    strError=string("Only one of the following keywords can appear in the object: create, update, for, format");                                                                        
                }
                if(d.value_.type() != null_type && !d.value_.get_str().empty())
                {
                    ParseEntityIdentifier(d.value_,&entity, MC_ENT_TYPE_ANY);       
                    if(found_entity)
                    {
                        memcpy(found_entity,&entity,sizeof(mc_EntityDetails));
                    }                        
                }                
                if(entity.GetEntityType() == MC_ENT_TYPE_STREAM)
                {
                    new_type=-1;
                    if((allowed_objects & 0x0002) == 0)
                    {
                        strError=string("Keyword not allowed in this API");                                                
                    }                        
                }
                else
                {
                    if(entity.GetEntityType() == MC_ENT_TYPE_UPGRADE)
                    {
                        new_type=-5;
                        if((allowed_objects & 0x0020) == 0)
                        {
                            strError=string("Keyword not allowed in this API");                                                
                        }                        
                    }
                    else
                    {
                        strError=string("Entity with this identifier not found");                                                                                                
                    }
                }
                parsed=true;
            }                    
            
            if(d.name_ == "update")
            {                 
                if(new_type != 0)
                {
                    strError=string("Only one of the following keywords can appear in the object: create, update, for, format");                                                                        
                }
                if(d.value_.type() != null_type && !d.value_.get_str().empty())
                {
                    ParseEntityIdentifier(d.value_,&entity, MC_ENT_TYPE_ASSET);       
                    if(found_entity)
                    {
                        memcpy(found_entity,&entity,sizeof(mc_EntityDetails));
                    }                        
                }
                new_type=-2;
                if((allowed_objects & 0x0010) == 0)
                {
                    strError=string("Keyword not allowed in this API");                                                
                }
                else
                {
                    if(entity.GetEntityType() != MC_ENT_TYPE_ASSET)
                    {
                        strError=string("Asset with this identifier not found");                                                                        
                    }
                }
                parsed=true;
            }                    
            if(d.name_ == "key")
            {
                if(d.value_.type() != null_type && (d.value_.type()==str_type))
                {
                    vKey=vector<unsigned char>(d.value_.get_str().begin(), d.value_.get_str().end());    
//                    key_is_set=true;
                }
                else
                {
                    strError=string("Invalid key");                            
                }
                if((allowed_objects & 0x0002) == 0)
                {
                    strError=string("Keyword not allowed in this API");                                                
                }
                parsed=true;
            }
            if(d.name_ == "key-hex")
            {
                if(d.value_.type() != null_type && (d.value_.type()==str_type))
                {
                    bool fIsHex;
                    vKey=ParseHex(d.value_.get_str().c_str(),fIsHex);    
                    if(!fIsHex)
                    {
                        strError=string("key should be hexadecimal string");                            
                    }
//                    key_is_set=true;
                }
                else
                {
                    strError=string("Invalid key");                            
                }
                if((allowed_objects & 0x0002) == 0)
                {
                    strError=string("Keyword not allowed in this API");                                                
                }
                parsed=true;
            }
            if(d.name_ == "data")
            {
/*                
                if(d.value_.type() != null_type && (d.value_.type()==str_type))
                {
                    bool fIsHex;
                    vValue=ParseHex(d.value_.get_str().c_str(),fIsHex);    
                    if(!fIsHex)
                    {
                        strError=string("value should be hexadecimal string");                            
                    }
//                    value_is_set=true;
                }
                else
                {
                    strError=string("Invalid value");                            
                }
 */ 
                formatted_data=d.value_;
                if((allowed_objects & 0x0002) == 0)
                {
                    strError=string("Keyword not allowed in this API");                                                
                }
                parsed=true;
            }
            
            if(d.name_ == "name")
            {
                if(d.value_.type() != null_type && !d.value_.get_str().empty())
                {
                    entity_name=d.value_.get_str().c_str();
                }
                else
                {
                    strError=string("Invalid name");                            
                }
                if((allowed_objects & 0x0005) == 0)
                {
                    strError=string("Keyword not allowed in this API");                                                
                }
                parsed=true;
            }
            if(d.name_ == "multiple")
            {
                if(d.value_.type() == int_type)
                {
                    multiple=d.value_.get_int();
                    multiple_is_set=true;
                }
                else
                {
                    strError=string("Invalid multiple");                            
                }
                if((allowed_objects & 0x0004) == 0)
                {
                    strError=string("Keyword not allowed in this API");                                                
                }
                parsed=true;
            }
            if(d.name_ == "startblock")
            {
                if(d.value_.type() == int_type)
                {
                    if( (d.value_.get_int64() >= 0) && (d.value_.get_int64() <= 0xFFFFFFFF) )
                    {
                        startblock=(uint32_t)(d.value_.get_int64());
                        startblock_is_set=true;
                    }
                    else
                    {
                        strError=string("Invalid startblock");                                                    
                    }
                }
                else
                {
                    strError=string("Invalid startblock");                            
                }
                if((allowed_objects & 0x0040) == 0)
                {
                    strError=string("Keyword not allowed in this API");                                                
                }
                parsed=true;
            }
            if(d.name_ == "open")
            {
                if(d.value_.get_bool())
                {
                    is_open=1;
                }        
                if((allowed_objects & 0x0005) == 0)
                {
                    strError=string("Keyword not allowed in this API");                                                
                }
                open_is_set=true;
                parsed=true;
            }
            if(d.name_ == "approve")
            {
                if(d.value_.get_bool())
                {
                    approve=1;
                }        
                else
                {
                    approve=0;                    
                }
                if((allowed_objects & 0x0020) == 0)
                {
                    strError=string("Keyword not allowed in this API");                                                
                }
                parsed=true;
            }
            if(d.name_ == "details")
            {
                if(new_type == -3)
                {
                    strError=string("details and inputcache not allowed in the same object");                                                
                }
                if(d.value_.type() == obj_type)
                {
                    BOOST_FOREACH(const Pair& p, d.value_.get_obj()) 
                    {              
                        if( (p.name_ == "protocol-version") && (p.value_.type() == int_type) )
                        {
                            if( p.value_.get_int() > 0 )
                            {
                                protocol_version=p.value_.get_int();                                
                            }                            
                            else
                            {
                                strError=string("Invalid protocol-version");                                                                                                            
                            }
                        }
                        else
                        {
                            lpDetails->SetParamValue(p.name_.c_str(),p.name_.size(),(unsigned char*)p.value_.get_str().c_str(),p.value_.get_str().size());                
                        }
                    }
                }                
                if((allowed_objects & 0x000D) == 0)
                {
                    strError=string("Keyword not allowed in this API");                                                
                }
                parsed=true;
            }            
            if(!parsed)
            {
                strError=string("Invalid field: ") + d.name_;                                            
            }
        }
        
        if(strError.size() == 0)
        {
            if(new_type == 0)
            {
//                strError=string("One of the following keywords can appear in the object: create, update, for");                                                                        
                if(data_format == MC_SCR_DATA_FORMAT_UNKNOWN)
                {
                    if(given_entity && given_entity->GetEntityType())
                    {
                        memcpy(&entity,given_entity,sizeof(mc_EntityDetails));
                        new_type=-2;
                    }
                    else
                    {
                        new_type=MC_ENT_TYPE_ASSET; 
                    }
                }
                else
                {
                    new_type=-4;
                }
            }
        }

        if( (data_format != MC_SCR_DATA_FORMAT_UNKNOWN) || (new_type == -1) )
        {
            switch(data_format)
            {
                case MC_SCR_DATA_FORMAT_UNKNOWN:
                    if(formatted_data.type() != null_type && (formatted_data.type()==str_type))
                    {
                        bool fIsHex;
                        vValue=ParseHex(formatted_data.get_str().c_str(),fIsHex);    
                        if(!fIsHex)
                        {
                            strError=string("value should be hexadecimal string");                            
                        }
    //                    value_is_set=true;
                    }
                    else
                    {
                        strError=string("Invalid value");                            
                    }
                    break;
                case MC_SCR_DATA_FORMAT_UTF8:
                    if(formatted_data.type() != null_type && (formatted_data.type()==str_type))
                    {
                        vValue=vector<unsigned char> (formatted_data.get_str().begin(),formatted_data.get_str().end());    
                    }
                    else
                    {
                        strError=string("Invalid value");                            
                    }
                    break;
                case MC_SCR_DATA_FORMAT_UBJSON:
                    size_t bytes;
                    int err;
                    const unsigned char *script;
                    lpDetailsScript->Clear();
                    lpDetailsScript->AddElement();
                    if((err = ubjson_write(formatted_data,lpDetailsScript,MAX_FORMATTED_DATA_DEPTH)) != MC_ERR_NOERROR)
                    {
                        strError=string("Couldn't transfer JSON object to internal UBJSON format");    
                    }
                    script = lpDetailsScript->GetData(0,&bytes);
                    vValue=vector<unsigned char> (script,script+bytes);                                            
                    break;
            }
        }
        
        if(strError.size() == 0)
        {
            if(new_type == -2)
            {
                if((allowed_objects & 0x0008) == 0)
                {
                    strError=string("Follow-on issuance not allowed in this API");                                                
                }
                else
                {                
                    if(vKey.size())
                    {
                        strError=string("Invalid field: key");                                                            
                    }
                    if(vValue.size())
                    {
                        strError=string("Invalid field: value");                                                            
                    }                
                }
            }            
        }
        
        if(strError.size() == 0)
        {
            if(new_type == MC_ENT_TYPE_ASSET)
            {
                if((allowed_objects & 0x0004) == 0)
                {
                    strError=string("Issuing new assets not allowed in this API");                                                
                }
                else
                {
                    if(is_open)
                    {
                        lpDetails->SetSpecialParamValue(MC_ENT_SPRM_FOLLOW_ONS,(unsigned char*)&is_open,1);                
                    }            
                    if(multiple_is_set)
                    {
                        if(mc_gState->m_Features->OpDropDetailsScripts())
                        {
                            lpDetails->SetSpecialParamValue(MC_ENT_SPRM_ASSET_MULTIPLE,(unsigned char*)&multiple,4);
                        }
                    }
                    if(entity_name.size())
                    {
                        if(mc_gState->m_Features->OpDropDetailsScripts())
                        {
                            lpDetails->SetSpecialParamValue(MC_ENT_SPRM_NAME,(const unsigned char*)(entity_name.c_str()),entity_name.size());//+1);
                        }
                    }
                    if(vKey.size())
                    {
                        strError=string("Invalid field: key");                                                            
                    }
                    if(vValue.size())
                    {
                        strError=string("Invalid field: value");                                                            
                    }
                }
            }
        }
        
        if(strError.size() == 0)
        {
            if(new_type == MC_ENT_TYPE_STREAM)
            {
                if((allowed_objects & 0x0001) == 0)
                {
                    strError=string("Creating new streams not allowed in this API");                                                
                }
                else
                {
                    format=1;
                    if(entity_name.size())
                    {
                        lpDetails->SetSpecialParamValue(MC_ENT_SPRM_NAME,(const unsigned char*)(entity_name.c_str()),entity_name.size());//+1);
                    }
                    if(is_open)
                    {
                        lpDetails->SetSpecialParamValue(MC_ENT_SPRM_ANYONE_CAN_WRITE,(unsigned char*)&is_open,1);                
                    }
                    if(multiple_is_set)
                    {
                        strError=string("Invalid field: multiple");                                                            
                    }
                    if(vKey.size())
                    {
                        strError=string("Invalid field: key");                                                            
                    }
                    if(vValue.size())
                    {
                        strError=string("Invalid field: value");                                                            
                    }
                }
            }
            if(new_type == -1)
            {
                format=-1;            
            }
        }

        if(strError.size() == 0)
        {
            if(new_type != MC_ENT_TYPE_UPGRADE)
            {
                if(protocol_version > 0)
                {
                    strError=string("Invalid field: protocol-version");                                                                                
                }
                if(startblock_is_set)
                {
                    strError=string("Invalid field: startblock");
                }
            }
            if(new_type != -5)
            {
                if(approve >= 0)
                {
                    strError=string("Invalid field: approve");                    
                }
            }
        }
        
        if(strError.size() == 0)
        {
            if(new_type == MC_ENT_TYPE_UPGRADE)
            {
                if(mc_gState->m_Features->Upgrades())
                {
                    if((allowed_objects & 0x0040) == 0)
                    {
                        strError=string("Creating new upgrades not allowed in this API");                                                
                    }
                    else
                    {
                        if(lpDetails->m_Size)
                        {
                            strError=string("Invalid fields in details object");                                                            
                        }
                        if(entity_name.size())
                        {
                            lpDetails->SetSpecialParamValue(MC_ENT_SPRM_NAME,(const unsigned char*)(entity_name.c_str()),entity_name.size());//+1);
                        }
                        if(protocol_version > 0)
                        {
                            lpDetails->SetSpecialParamValue(MC_ENT_SPRM_UPGRADE_PROTOCOL_VERSION,(unsigned char*)&protocol_version,4);                                
                        }
                        else
                        {
                            strError=string("Missing protocol-version");                                                                                    
                        }
                        if(startblock > 0)
                        {
                            lpDetails->SetSpecialParamValue(MC_ENT_SPRM_UPGRADE_START_BLOCK,(unsigned char*)&startblock,4);        
                        }                    
                        if(multiple_is_set)
                        {
                            strError=string("Invalid field: multiple");                                                            
                        }
                        if(open_is_set)
                        {
                            strError=string("Invalid field: open");                                                            
                        }
                        if(vKey.size())
                        {
                            strError=string("Invalid field: key");                                                            
                        }
                        if(vValue.size())
                        {
                            strError=string("Invalid field: value");                                                            
                        }
                    }
                }
                else
                {
                    strError=string("Upgrades are not supported by this protocol version"); 
                }
            }
        }
        
        if(strError.size() == 0)
        {
            if(new_type == -5)
            {
                if(mc_gState->m_Features->Upgrades())
                {
                    if(lpDetails->m_Size)
                    {
                        strError=string("Invalid field: details");                                                            
                    }
                    if(multiple_is_set)
                    {
                        strError=string("Invalid field: multiple");                                                            
                    }
                    if(open_is_set)
                    {
                        strError=string("Invalid field: open");                                                            
                    }
                    if(vKey.size())
                    {
                        strError=string("Invalid field: key");                                                            
                    }
                    if(vValue.size())
                    {
                        strError=string("Invalid field: value");                                                            
                    }
                }
                else
                {
                    strError=string("Upgrades are not supported by this protocol version"); 
                }
            }
        }

        if(strError.size() == 0)
        {
            int err;
            size_t bytes;
            const unsigned char *script;
            
            if(new_type == MC_ENT_TYPE_ASSET)
            {
                if(mc_gState->m_Features->OpDropDetailsScripts())
                {
                    script=lpDetails->GetData(0,&bytes);
                    err=lpDetailsScript->SetNewEntityType(MC_ENT_TYPE_ASSET,0,script,bytes);
                    if(err)
                    {
                        strError=string("Invalid custom fields, too long");                                                            
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
                    lpDetailsScript->SetAssetDetails(entity_name.c_str(),multiple,script,bytes);                
                    script = lpDetailsScript->GetData(0,&bytes);
                    if(bytes > 0)
                    {
                        scriptOpReturn << OP_RETURN << vector<unsigned char>(script, script + bytes);
                    }                    
                }
            }
            
            if(new_type == MC_ENT_TYPE_STREAM)
            {
                if(mc_gState->m_Features->OpDropDetailsScripts())
                {
                    int err;
                    script=lpDetails->GetData(0,&bytes);
                    err=lpDetailsScript->SetNewEntityType(MC_ENT_TYPE_STREAM,0,script,bytes);
                    if(err)
                    {
                        strError=string("Invalid custom fields, too long");                                                            
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
                    lpDetailsScript->SetNewEntityType(MC_ENT_TYPE_STREAM);
                    script = lpDetailsScript->GetData(0,&bytes);
                    scriptOpReturn << vector<unsigned char>(script, script + bytes) << OP_DROP;
                    
                    lpDetailsScript->Clear();
                    script=lpDetails->GetData(0,&bytes);
                    lpDetailsScript->SetGeneralDetails(script,bytes);                
                    script = lpDetailsScript->GetData(0,&bytes);
                    if(bytes > 0)
                    {
                        scriptOpReturn << OP_RETURN << vector<unsigned char>(script, script + bytes);
                    }
                }
            }
            
            if(new_type == MC_ENT_TYPE_UPGRADE)
            {
                int err;
                script=lpDetails->GetData(0,&bytes);
                err=lpDetailsScript->SetNewEntityType(MC_ENT_TYPE_UPGRADE,0,script,bytes);
                if(err)
                {
                    strError=string("Invalid custom fields, too long");                                                            
                }
                else
                {
                    script = lpDetailsScript->GetData(0,&bytes);
                    scriptOpReturn << vector<unsigned char>(script, script + bytes) << OP_DROP << OP_RETURN;
                }
            }
            
            if(new_type == -2)
            {
                if(mc_gState->m_Features->OpDropDetailsScripts())
                {
                    int err;
                    lpDetailsScript->Clear();
                    lpDetailsScript->SetEntity(entity.GetTxID()+MC_AST_SHORT_TXID_OFFSET);
                    script = lpDetailsScript->GetData(0,&bytes);
                    scriptOpReturn << vector<unsigned char>(script, script + bytes) << OP_DROP;

                    lpDetailsScript->Clear();
                    script=lpDetails->GetData(0,&bytes);
                    err=lpDetailsScript->SetNewEntityType(MC_ENT_TYPE_ASSET,1,script,bytes);
                    if(err)
                    {
                        strError=string("Invalid custom fields, too long");                                                            
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
                    lpDetailsScript->SetGeneralDetails(script,bytes);                
                    script = lpDetailsScript->GetData(0,&bytes);
                    if(bytes > 0)
                    {
                        scriptOpReturn << OP_RETURN << vector<unsigned char>(script, script + bytes);
                    }                    
                }
            }
                
            if(new_type == -1)
            {
                lpDetailsScript->Clear();
                lpDetailsScript->SetEntity(entity.GetTxID()+MC_AST_SHORT_TXID_OFFSET);
                script = lpDetailsScript->GetData(0,&bytes);
                scriptOpReturn << vector<unsigned char>(script, script + bytes) << OP_DROP;

                lpDetailsScript->Clear();
                if(lpDetailsScript->SetItemKey(&vKey[0],vKey.size()) == MC_ERR_NOERROR)
                {
                    script = lpDetailsScript->GetData(0,&bytes);
                    scriptOpReturn << vector<unsigned char>(script, script + bytes) << OP_DROP;
                }

                if(data_format != MC_SCR_DATA_FORMAT_UNKNOWN)
                {
                    lpDetailsScript->Clear();
                    lpDetailsScript->SetDataFormat(data_format);
                    script = lpDetailsScript->GetData(0,&bytes);
                    scriptOpReturn << vector<unsigned char>(script, script + bytes) << OP_DROP;                    
                }
                
                scriptOpReturn << OP_RETURN << vValue;                    
            }
                
            if(new_type == -4)
            {
                lpDetailsScript->Clear();
                lpDetailsScript->SetDataFormat(data_format);
                script = lpDetailsScript->GetData(0,&bytes);
                scriptOpReturn << vector<unsigned char>(script, script + bytes) << OP_DROP;                    
                
                scriptOpReturn << OP_RETURN << vValue;                    
            }
            
            if(new_type == -3)
            {
                script=lpDetails->GetData(0,&bytes);
                scriptOpReturn << vector<unsigned char>(script, script + bytes) << OP_DROP << OP_RETURN;
            }

            if(new_type == -5)
            {
                if(approve < 0)
                {
                    strError=string("Missing approve field");                                                                                    
                }
                
                lpDetailsScript->Clear();
                lpDetailsScript->SetEntity(entity.GetTxID()+MC_AST_SHORT_TXID_OFFSET);
                script = lpDetailsScript->GetData(0,&bytes);
                scriptOpReturn << vector<unsigned char>(script, script + bytes) << OP_DROP;                    
                
                lpDetailsScript->Clear();
                lpDetailsScript->SetApproval(approve, mc_TimeNowAsUInt());
                script = lpDetailsScript->GetData(0,&bytes);
                scriptOpReturn << vector<unsigned char>(script, script + bytes) << OP_DROP;                    

                scriptOpReturn << OP_RETURN;                    
            }
        }

        delete lpDetails;
        delete lpDetailsScript;
        if(strError.size())
        {
            throw JSONRPCError(RPC_INVALID_PARAMETER, strError);            
        }            
    }
    else
    {
        bool fIsHex;
        if( ((allowed_objects & 0x0100) != 0) || (param.get_str().size() != 0) )
        {
            vector<unsigned char> dataData(ParseHex(param.get_str().c_str(),fIsHex));    
            if(!fIsHex)
            {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "data-hex should be hexadecimal string or recognized object format");                        
            }
            scriptOpReturn << OP_RETURN << dataData;
        }
    }
    
    return scriptOpReturn;
}

vector<string> ParseStringList(Value param)
{
    vector<string> vStrings;
    set<string> setStrings;
    
    if(param.type() == array_type)
    {
        BOOST_FOREACH(const Value& vtok, param.get_array())
        {
            if(vtok.type() == str_type)
            {
                string tok=vtok.get_str();
                if (setStrings.count(tok))
                    throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, duplicate value: "+tok);
                setStrings.insert(tok);
                vStrings.push_back(tok);
            }
            else
            {
                if(vtok.type() == int_type)
                {
                    string tok=strprintf("%ld",vtok.get_int64());
                    setStrings.insert(tok);
                    vStrings.push_back(tok);                    
                }
                else
                {
                    throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid value, expected array of strings");                                                        
                }
            }
        }            
    }
    else
    {
        if(param.type() == str_type)
        {
            stringstream ss(param.get_str()); 
            string tok;
            while(getline(ss, tok, ',')) 
            {
                if (setStrings.count(tok))
                    throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, duplicate value: "+tok);
                setStrings.insert(tok);
                vStrings.push_back(tok);
            }
        }
        else
        {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid value, expected string or array");                                                                    
        }
    }

    return vStrings;
}

void ParseEntityIdentifier(Value entity_identifier,mc_EntityDetails *entity,uint32_t entity_type)
{
    unsigned char buf[32];
    unsigned char buf_a[MC_AST_ASSET_REF_SIZE];
    unsigned char buf_n[MC_AST_ASSET_REF_SIZE];
    int ret;
    string entity_nameU; 
    string entity_nameL; 
    
    switch(entity_type)
    {
        case MC_ENT_TYPE_STREAM:
            entity_nameU="Stream";
            entity_nameL="stream";
            break;
        case MC_ENT_TYPE_ASSET:
            entity_nameU="Asset";
            entity_nameL="asset";
            break;
        case MC_ENT_TYPE_UPGRADE:
            entity_nameU="Upgrade";
            entity_nameL="upgrade";
            break;
        default:
            entity_nameU="Entity";
            entity_nameL="entity";
            break;
    }
    
    if (entity_identifier.type() != null_type && !entity_identifier.get_str().empty())
    {        
        string str=entity_identifier.get_str();
        
        if(entity_type & MC_ENT_TYPE_STREAM)
        {
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
                        throw JSONRPCError(RPC_ENTITY_NOT_FOUND, string("Stream with this stream reference not found: ")+str);                    
                    }
                }
            }
        }
        
        ret=ParseAssetKey(str.c_str(),buf,NULL,NULL,NULL,NULL,entity_type);
        switch(ret)
        {
            case MC_ASSET_KEY_INVALID_TXID:
                throw JSONRPCError(RPC_ENTITY_NOT_FOUND, entity_nameU+string(" with this txid not found: ")+str);
                break;
            case MC_ASSET_KEY_INVALID_REF:
                throw JSONRPCError(RPC_ENTITY_NOT_FOUND, entity_nameU+string(" with this reference not found: ")+str);
                break;
            case MC_ASSET_KEY_INVALID_NAME:
                throw JSONRPCError(RPC_ENTITY_NOT_FOUND, entity_nameU+string(" with this name not found: ")+str);
                break;
            case MC_ASSET_KEY_INVALID_SIZE:
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Could not parse "+entity_nameL+" key: "+str);
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
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid "+entity_nameL+" identifier");        
    }
           
    if(entity)
    {
        if(mc_gState->m_Assets->FindEntityByTxID(entity,buf))
        {
            if((entity_type & entity->GetEntityType()) == 0)
//            if(entity->GetEntityType() != MC_ENT_TYPE_STREAM)
            {
                throw JSONRPCError(RPC_ENTITY_NOT_FOUND, "Invalid "+entity_nameL+" identifier, not "+entity_nameL);                        
            }
        }    
    }    
}

bool AssetCompareByRef(Value a,Value b)
{ 
    Value assetref_a;
    Value assetref_b;

    BOOST_FOREACH(const Pair& p, a.get_obj()) 
    {
        if(p.name_ == "assetref")
        {
            assetref_a=p.value_;
        }
        if(p.name_ == "streamref")
        {
            assetref_a=p.value_;
        }
    }

    BOOST_FOREACH(const Pair& p, b.get_obj()) 
    {
        if(p.name_ == "assetref")
        {
            assetref_b=p.value_;
        }
        if(p.name_ == "streamref")
        {
            assetref_b=p.value_;
        }
    }

    if(assetref_b.type() != str_type)
    {
        return true;
    }
    else
    {
        if(assetref_a.type() != str_type)
        {
            return false;
        }            
    }

    unsigned char buf_a[MC_AST_ASSET_REF_SIZE];
    unsigned char buf_b[MC_AST_ASSET_REF_SIZE];
    
    AssetRefDecode(buf_a,assetref_a.get_str().c_str(),assetref_a.get_str().size());
    AssetRefDecode(buf_b,assetref_b.get_str().c_str(),assetref_b.get_str().size());

    
//    if(strcmp(assetref_a.get_str().c_str(),assetref_b.get_str().c_str()) < 0)
    if(mc_GetLE(buf_a,4) < mc_GetLE(buf_b,4))
    {
        return true;
    }
    else
    {
        if(mc_GetLE(buf_a,4) == mc_GetLE(buf_b,4))
        {
            if(mc_GetLE(buf_a+4,4) < mc_GetLE(buf_b+4,4))
            {
                return true;                
            }            
        }
    }
    
    return false;
}

bool StringToInt(string str,int *value)
{
    char *endptr;
    if(str.size())
    {
        *value=(int)strtol(str.c_str(),&endptr,0);
        if(endptr == (str.c_str() + str.size()) )
        {
            return true;
        }
    }
    return  false;
}

bool ParseIntRange(string str,int *from,int *to)
{
    size_t mpos;
    
    mpos=str.find("-");
    if( (mpos == string::npos) || (mpos == 0) || (mpos == (str.size()-1)) )
    {
        return false;
    }
    if(!StringToInt(str.substr(0,mpos),from))
    {
        return false;        
    }
    if(!StringToInt(str.substr(mpos+1),to))
    {
        return false;        
    }
    return true;
}

vector<int> ParseBlockSetIdentifier(Value blockset_identifier)
{
    vector<int> block_set;
    vector<int> result;
    int last_block;
    
    if(blockset_identifier.type() == obj_type)
    {
        int64_t starttime=-1; 
        int64_t endtime=-1; 
    
        BOOST_FOREACH(const Pair& d, blockset_identifier.get_obj()) 
        {              
            if(d.name_ == "starttime")
            {
                if(d.value_.type() != int_type)
                    throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid starttime");            
                    
                starttime=d.value_.get_int64();
                if( (starttime<0) || (starttime > 0xffffffff))
                {
                    throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid starttime");                                
                }
            }
            else
            {
                if(d.name_ == "endtime")
                {
                    if(d.value_.type() != int_type)
                        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid endtime");            

                    endtime=d.value_.get_int64();
                    if( (endtime<0) || (endtime > 0xffffffff))
                    {
                        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid endtime");                                
                    }
                }
                else
                {
                    throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid time range");            
                }
            }
        }
        
        if(starttime < 0)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Missing starttime");            
        if(endtime < 0)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Missing starttime");            
        
        if(starttime <= endtime)
        {
            for(int block=0; block<chainActive.Height();block++)
            {
                if( (chainActive[block]->nTime >= starttime) && (chainActive[block]->nTime <= endtime) )
                {
                    block_set.push_back(block);                    
                }
            }
        }        
    }
    else
    {
        vector<string> inputStrings;
        inputStrings=ParseStringList(blockset_identifier);

        for(unsigned int i=0;i<inputStrings.size();i++)
        {
            string str=inputStrings[i];
            int value,from,to;
            if(StringToInt(str,&value))
            {
                from=value;
                to=value;
                if(value < 0)
                {
                    to=chainActive.Height();
                    from=chainActive.Height()+value+1;
                }
            }
            else
            {
                if(!ParseIntRange(str,&from,&to))
                {
                    uint256 hash = ParseHashV(str, "Block hash");
                    
                    if (mapBlockIndex.count(hash) == 0)
                        throw JSONRPCError(RPC_BLOCK_NOT_FOUND, "Block " + str + " not found");

                    CBlockIndex* pblockindex = mapBlockIndex[hash];
                    if(!chainActive.Contains(pblockindex))
                    {
                        throw JSONRPCError(RPC_BLOCK_NOT_FOUND, "Block " + str + " not found in active chain");
                    }
                    from=pblockindex->nHeight;
                    to=from;
                }
            }
            if (from > to)
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid range " + str);            
   
            if(from<0)
            {
                from=0;
            }
            if(to>chainActive.Height())
            {
                to=chainActive.Height();
            }
            
            for(int block=from;block<=to;block++)
            {
                block_set.push_back(block);
            }
        }        
    }
    
    if(block_set.size())
    {
        sort(block_set.begin(),block_set.end());
    }
    
    last_block=-1;
    for(unsigned int i=0;i<block_set.size();i++)
    {
        if(block_set[i] != last_block)
        {
            result.push_back(block_set[i]);
            last_block=block_set[i];
        }
    }
    
    return result;
}



Array AssetArrayFromAmounts(mc_Buffer *asset_amounts,int issue_asset_id,uint256 hash,int show_type)
{
    Array assets;
    unsigned char *ptr;
    const unsigned char *txid;
    uint64_t quantity;
            
    for(int a=0;a<asset_amounts->GetCount();a++)
    {
        Object asset_entry;
        ptr=(unsigned char *)asset_amounts->GetRow(a);

        if( (mc_GetABRefType(ptr) != MC_AST_ASSET_REF_TYPE_SPECIAL) && 
            (mc_GetABRefType(ptr) != MC_AST_ASSET_REF_TYPE_GENESIS) )    
        {
            mc_EntityDetails entity;
            if(mc_gState->m_Assets->FindEntityByFullRef(&entity,ptr))
            {
                quantity=mc_GetABQuantity(ptr);
                if(quantity != 0)
                {
                    txid=entity.GetTxID();
                    if(a == issue_asset_id)
                    {
                        asset_entry=AssetEntry(txid,-quantity,0x0F);                        
                    }
                    else
                    {
                        asset_entry=AssetEntry(txid,quantity,0x40);
                    }
                    if(show_type)
                    {
                        switch(mc_GetABScriptType(ptr))
                        {
                            case MC_SCR_ASSET_SCRIPT_TYPE_TRANSFER:
                                asset_entry.push_back(Pair("type", "transfer"));                                            
                                break;
                            case MC_SCR_ASSET_SCRIPT_TYPE_FOLLOWON:
                                asset_entry.push_back(Pair("type", "issuemore"));                                            
                                break;
                            case MC_SCR_ASSET_SCRIPT_TYPE_TRANSFER | MC_SCR_ASSET_SCRIPT_TYPE_FOLLOWON:
                                asset_entry.push_back(Pair("type", "issuemore+transfer"));                                            
                                break;
                            case 0:
                                asset_entry.push_back(Pair("type", "issuefirst"));                                            
                                break;
                        }
                    }
                    assets.push_back(asset_entry);                    
                }
            }
        }            
        else
        {
            if(mc_GetABRefType(ptr) == MC_AST_ASSET_REF_TYPE_GENESIS)
            {
                if(hash != 0)
                {
                    mc_EntityDetails entity;
                    if(mc_gState->m_Assets->FindEntityByTxID(&entity,(unsigned char*)&hash))
                    {
                        quantity=mc_GetABQuantity(ptr);
                        if(quantity != 0)
                        {
                            txid=(unsigned char*)&hash;
                            asset_entry=AssetEntry(txid,quantity,0x40);
                            if(show_type)
                            {
                                asset_entry.push_back(Pair("type", "issuefirst"));                                            
                            }
                            assets.push_back(asset_entry);                    
                        }
                    }
                }
            }            
        }
    }

    return assets;
}

void ParseRawAction(string action,bool& lock_it, bool& sign_it,bool& send_it)
{
    bool valid_action=false;
        
    if(action == "")
    {
        valid_action=true;
    }
    if(action == "lock")
    {
        lock_it=true;
        valid_action=true;
    }
    if(action == "sign")
    {
        sign_it=true;
        valid_action=true;
    }
    if((action == "lock,sign") || (action == "sign,lock"))
    {
        sign_it=true;
        lock_it=true;
        valid_action=true;
    }
    if(action == "send")
    {
        sign_it=true;
        send_it=true;
        valid_action=true;
    }
    
    if(!valid_action)
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid action");
    }    
}

int paramtoint(Value param,bool check_for_min,int min_value,string error_message)
{
    int result;
    
    if(param.type() != int_type)
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, error_message);        
    }
    
    result=param.get_int();
    if(check_for_min)
    {
        if(result < min_value)
        {
            throw JSONRPCError(RPC_INVALID_PARAMETER, error_message);                    
        }
    }
    
    return result;
}

bool mc_IsJsonObjectForMerge(const Value *value,int level)
{
    if(value->type() != obj_type)
    {
        return false;
    }
    if(level == 0)
    {
        if(value->get_obj().size() != 1)
        {
            return false;            
        }
        if((value->get_obj()[0].name_ != "json"))
        {
            return false;            
        }
        if((value->get_obj()[0].value_.type() != obj_type))
        {
            return false;            
        }        
    }
    
    return true;
}

Value mc_MergeValues(const Value *value1,const Value *value2,uint32_t mode,int level,int *error)
{
    int no_merge=0;
    
    if(mode & MC_VMM_OMIT_NULL)
    {
        if(mode & MC_VMM_TAKE_FIRST)
        {
            if(value1->type() == null_type)    
            {
                return Value::null;
            }
        }            
        else
        {
            if(value2->type() == null_type)    
            {
                return Value::null;                
            }            
        }        
    }
    
    bool value1_is_obj=mc_IsJsonObjectForMerge(value1,level);
    bool value2_is_obj=mc_IsJsonObjectForMerge(value2,level);
    if( (mode & MC_VMM_MERGE_OBJECTS) == 0)
    {
        no_merge=1;
    }
    else
    {
        if(level>1)
        {
            if( (mode & MC_VMM_RECURSIVE) == 0 )
            {
                no_merge=1;
            }
        }
    }
/*            
    if(!value1_is_obj)
    {
        if(!value2_is_obj)
        {
            no_merge=1;
        }
    }
*/    
    if(no_merge)
    {
        if(mode & MC_VMM_TAKE_FIRST)
        {
            return *value1;
        }
        return *value2;                
    }
    
        
        
    if(!value1_is_obj)
    {
        if(mode & MC_VMM_IGNORE)
        {
            return *value2; 
        }
        *error=MC_ERR_INVALID_PARAMETER_VALUE;
        return Value::null;
    }
    else
    {
        if(!value2_is_obj)
        {
            if(mode & MC_VMM_IGNORE)
            {
                return *value1;
            }            
            *error=MC_ERR_INVALID_PARAMETER_VALUE;
            return Value::null;
        }       
    }
    
    
    Object result;
    map<string, Value> map1;   
    map<string, Value> map2;   
    
    BOOST_FOREACH(const Pair& a, value1->get_obj()) 
    {
        map<string, Value>::iterator it1 = map1.find(a.name_); 
        if( it1 == map1.end() )
        {
            map1.insert(make_pair(a.name_, a.value_));
        }
        else
        {
            if( (mode & MC_VMM_TAKE_FIRST_FOR_FIELD) == 0 )
            {
                it1->second=a.value_;
            }
        }
    }

    BOOST_FOREACH(const Pair& a, value2->get_obj()) 
    {
        map<string, Value>::iterator it2 = map2.find(a.name_); 
        if( it2 == map2.end() )
        {
            map2.insert(make_pair(a.name_, a.value_));
        }
        else
        {
            if( (mode & MC_VMM_TAKE_FIRST_FOR_FIELD) == 0 )
            {
                it2->second=a.value_;
            }
        }
    }
    
    for(map<string,Value>::iterator it1 = map1.begin(); it1 != map1.end(); ++it1) 
    {
        map<string, Value>::iterator it2 = map2.find(it1->first); 
        if( it2 == map2.end() )
        {
            if( ((mode & MC_VMM_OMIT_NULL) == 0) || (it1->second.type() != null_type) )
            {
                result.push_back(Pair(it1->first, it1->second));  
            }
        }
        else
        {
            Value merged=mc_MergeValues(&(it1->second),&it2->second,mode,level+1,error);
            if(*error)
            {
                return Value::null;                
            }
            if( ((mode & MC_VMM_OMIT_NULL) == 0) || (merged.type() != null_type) )
            {
                result.push_back(Pair(it1->first, merged));  
            }
            map2.erase(it2);
        }
    }
    
    for(map<string,Value>::iterator it2 = map2.begin(); it2 != map2.end(); ++it2) 
    {
        if( ((mode & MC_VMM_OMIT_NULL) == 0) || (it2->second.type() != null_type) )
        {
            result.push_back(Pair(it2->first, it2->second));  
        }
    }
    
    return result;
}
