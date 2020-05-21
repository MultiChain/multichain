// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "utils/utilparse.h"
#include "version/clientversion.h"

using namespace std;

const unsigned char* GetAddressIDPtr(const CTxDestination& address) 
{
    const CKeyID *lpKeyID=boost::get<CKeyID> (&address);
    const CScriptID *lpScriptID=boost::get<CScriptID> (&address);
    unsigned char *aptr;
    aptr=NULL;
    if(lpKeyID)
    {
        aptr=(unsigned char*)(lpKeyID);
    }
    else
    {
        if(lpScriptID)
        {
            aptr=(unsigned char*)(lpScriptID);
        }                    
    }
    
    return aptr;
}

bool mc_ExtractOutputAssetQuantities(mc_Buffer *assets,string& reason,bool with_followons)
{
    int err;
    uint32_t script_type=MC_SCR_ASSET_SCRIPT_TYPE_TRANSFER;
    if(with_followons)
    {        
        script_type |= MC_SCR_ASSET_SCRIPT_TYPE_FOLLOWON;
    }
    for (int e = 0; e < mc_gState->m_TmpScript->GetNumElements(); e++)
    {
        mc_gState->m_TmpScript->SetElement(e);
        err=mc_gState->m_TmpScript->GetAssetQuantities(assets,script_type);
        if((err != MC_ERR_NOERROR) && (err != MC_ERR_WRONG_SCRIPT))
        {
            reason="Asset transfer script rejected - error in output transfer script";
            return false;                                
        }
    }

    return true;
}

bool mc_VerifyAssetPermissions(mc_Buffer *assets, vector<CTxDestination> addressRets, int required_permissions, uint32_t permission, string& reason)
{
    mc_EntityDetails entity;
    int asset_count=-1;
    
    for(int i=0;i<assets->GetCount();i++)
    {
        if(mc_gState->m_Assets->FindEntityByFullRef(&entity,assets->GetRow(i)))
        {
            if( entity.Permissions() & (MC_PTP_SEND | MC_PTP_RECEIVE) )
            {
                if( (addressRets.size() != 1) || (required_permissions > 1) )
                {
                    reason="Sending restricted asset to non-standard and multisig addresses not allowed";
                    return false;                                                    
                }
                if(assets->GetCount() > 1)
                {
                    if(asset_count < 0)
                    {
                        asset_count=0;
                        for(int j=0;j<assets->GetCount();j++)
                        {
                            if(mc_GetABRefType(assets->GetRow(j)) != MC_AST_ASSET_REF_TYPE_SPECIAL)
                            {
                                asset_count++;
                            }                            
                        }
                    }
                    if(asset_count > 1)
                    {
                        if(permission == MC_PTP_SEND)
                        {
                            reason="One of multiple assets in input has per-asset permissions";
                        }
                        if(permission == MC_PTP_RECEIVE)
                        {
                            reason="One of multiple assets in output has per-asset permissions";
                        }
                        return false;                                
                    }
                }
                if(entity.Permissions() & permission)
                {
                    int found=required_permissions;
                    for(int j=0;j<(int)addressRets.size();j++)
                    {
                        if(mc_gState->m_Permissions->GetPermission(entity.GetTxID(),GetAddressIDPtr(addressRets[j]),permission))
                        {
                            found--;
                        }
                    }
                    if(found > 0)
                    {
                        if(permission == MC_PTP_SEND)
                        {
                            reason="One of the inputs doesn't have per-asset send permission";
                        }
                        if(permission == MC_PTP_RECEIVE)
                        {
                            reason="One of the outputs doesn't have per-asset receive permission";
                        }                    
                        return false;                                
                    }
                }
            }
        }        
    }
    
    return true;
}

bool HasPerOutputDataEntries(const CTxOut& txout,mc_Script *lpScript)
{
    if(mc_gState->m_NetworkParams->IsProtocolMultichain())
    {
        unsigned char *ptr;
        int size;
        const CScript& script1 = txout.scriptPubKey;        
        CScript::const_iterator pc1 = script1.begin();
        lpScript->Clear();
        lpScript->SetScript((unsigned char*)(&pc1[0]),(size_t)(script1.end()-pc1),MC_SCR_TYPE_SCRIPTPUBKEY);

        for (int e = 0; e < lpScript->GetNumElements(); e++)
        {
            lpScript->SetElement(e);
            if(lpScript->GetRawData(&ptr,&size) == 0)      
            {
                return true;
            }
        }        
    }

    return false;
}

bool IsLicenseTokenIssuance(mc_Script *lpScript,uint256 hash)
{
    int64_t quantity;
    mc_EntityDetails entity;
    
    if(lpScript->GetNumElements() == 0)
    {
        return false;
    }
    
    lpScript->SetElement(0);
    if(lpScript->GetAssetGenesis(&quantity) == 0)
    {
        if(quantity == 1)
        {
            if(mc_gState->m_Assets->FindEntityByTxID(&entity,(unsigned char*)&hash))
            {
                if(entity.GetEntityType() == MC_ENT_TYPE_LICENSE_TOKEN)
                {
                    return true;
                }
            }            
        }
    }
    
    return false;
}

bool IsLicenseTokenTransfer(mc_Script *lpScript,mc_Buffer *amounts)
{
    mc_EntityDetails entity;
    
    if(lpScript->GetNumElements() < 2)
    {
        return false;
    }
    
    if(amounts->GetCount() != 1)
    {
        return false;
    }
    
    if(mc_GetABQuantity(amounts->GetRow(0)) != 1)            
    {
        return false;        
    }
    
    if(mc_gState->m_Assets->FindEntityByFullRef(&entity,amounts->GetRow(0)))
    {
        if(entity.GetEntityType() == MC_ENT_TYPE_LICENSE_TOKEN)
        {
            return true;
        }        
    }
    
    return false;
}

void AppendSpecialRowsToBuffer(mc_Buffer *amounts,uint256 hash,int expected_allowed,int expected_required,int *allowed,int *required,CAmount nValue)
{
    unsigned char buf[MC_AST_ASSET_FULLREF_BUF_SIZE];
    int64_t quantity;
    uint32_t type;
    int row;
 
    if( ( (expected_allowed & MC_PTP_ISSUE) && (*allowed & MC_PTP_ISSUE) ) ||
        ( (expected_required & MC_PTP_ISSUE) && (*required & MC_PTP_ISSUE) && (hash == 0) ) )    
    {
            memset(buf,0,MC_AST_ASSET_FULLREF_BUF_SIZE);
            type=MC_PTP_ISSUE | MC_PTP_SEND;
            quantity=1;
            mc_PutLE(buf+4,&type,4);
            mc_SetABRefType(buf,MC_AST_ASSET_REF_TYPE_SPECIAL);
            mc_SetABQuantity(buf,quantity);
            if(amounts->Seek(buf) < 0)
            {
                amounts->Add(buf);                
            }
    }

    if( ( (expected_allowed & MC_PTP_CREATE) && (*allowed & MC_PTP_CREATE) ) ||
        ( (expected_required & MC_PTP_CREATE) && (*required & MC_PTP_CREATE) && (hash == 0) ) )    
    {
            memset(buf,0,MC_AST_ASSET_FULLREF_BUF_SIZE);
            type=MC_PTP_CREATE | MC_PTP_SEND;
            quantity=1;
            mc_PutLE(buf+4,&type,4);
            mc_SetABRefType(buf,MC_AST_ASSET_REF_TYPE_SPECIAL);
            mc_SetABQuantity(buf,quantity);
            if(amounts->Seek(buf) < 0)
            {
                amounts->Add(buf);                
            }
    }

    if( ( (expected_allowed & MC_PTP_ADMIN) && (*allowed & MC_PTP_ADMIN) ) ||
        ( (expected_required & MC_PTP_ADMIN) && (*required & MC_PTP_ADMIN) && (hash == 0) ) )    
    {
            memset(buf,0,MC_AST_ASSET_FULLREF_BUF_SIZE);
            type=MC_PTP_ADMIN | MC_PTP_SEND;
            quantity=1;
            mc_PutLE(buf+4,&type,4);
            mc_SetABRefType(buf,MC_AST_ASSET_REF_TYPE_SPECIAL);
            mc_SetABQuantity(buf,quantity);
            if(amounts->Seek(buf) < 0)
            {
                amounts->Add(buf);                
            }
    }

    if( ( (expected_allowed & MC_PTP_ACTIVATE) && (*allowed & MC_PTP_ACTIVATE) ) ||
        ( (expected_required & MC_PTP_ACTIVATE) && (*required & MC_PTP_ACTIVATE) && (hash == 0) ) )    
    {
            memset(buf,0,MC_AST_ASSET_FULLREF_BUF_SIZE);
            type=MC_PTP_ACTIVATE | MC_PTP_SEND;
            quantity=1;
            mc_PutLE(buf+4,&type,4);
            mc_SetABRefType(buf,MC_AST_ASSET_REF_TYPE_SPECIAL);
            mc_SetABQuantity(buf,quantity);
            if(amounts->Seek(buf) < 0)
            {
                amounts->Add(buf);                
            }
    }

/*                                                                              // Not required, publish addresses are passed explicitly                
    if( ( (expected_allowed & MC_PTP_WRITE) && (*allowed & MC_PTP_WRITE) ) ||
        ( (expected_required & MC_PTP_WRITE) && (*required & MC_PTP_WRITE) && (hash == 0) ) )    
    {
            memset(buf,0,MC_AST_ASSET_FULLREF_BUF_SIZE);
            type=MC_PTP_WRITE | MC_PTP_SEND;
            quantity=1;
            mc_PutLE(buf+4,&type,4);
            mc_PutLE(buf+MC_AST_ASSET_QUANTITY_OFFSET,&quantity,MC_AST_ASSET_QUANTITY_SIZE);
            if(amounts->Seek(buf) < 0)
            {
                amounts->Add(buf,buf+MC_AST_ASSET_QUANTITY_OFFSET);                
            }
    }
*/
    memset(buf,0,MC_AST_ASSET_FULLREF_BUF_SIZE);
    type=MC_PTP_SEND;
    mc_PutLE(buf+4,&type,4);
    mc_SetABRefType(buf,MC_AST_ASSET_REF_TYPE_SPECIAL);
    quantity=nValue;
    row=amounts->Seek(buf);
    if(row >= 0)
    {
        quantity+=mc_GetABQuantity(amounts->GetRow(row));
        mc_SetABQuantity(amounts->GetRow(row),quantity);
    }
    else
    {
        mc_SetABQuantity(buf,quantity);
        amounts->Add(buf);                            
    }                
}

/* 
 * Parses txout script into asset-quantity buffer
 * Use it only with unspent or not yet created outputs
 */


bool ParseMultichainTxOutToBuffer(uint256 hash,                                 // IN, tx hash, if !=0 genesis asset reference is retrieved from asset DB
                                  const CTxOut& txout,                          // IN, tx to be parsed
                                  mc_Buffer *amounts,                           // OUT, output amount buffer
                                  mc_Script *lpScript,                          // TMP, temporary script object
                                  int *allowed,                                 // IN/OUT/NULL returns permissions of output address ANDed with input value 
                                  int *required,                                // IN/OUT/NULL returns permission required by this output, adds special rows to output buffer according to input value   
                                  map<uint32_t, uint256>* mapSpecialEntity,
                                  string& strFailReason)                        // OUT error
{
    unsigned char buf[MC_AST_ASSET_FULLREF_BUF_SIZE];
    int err,row,disallow_if_assets_found;
    int64_t quantity,total,last;
    int expected_allowed,expected_required;
    uint32_t type,from,to,timestamp,type_ored,approval;
    bool issue_found;
    uint32_t new_entity_type;
    
    memset(buf,0,MC_AST_ASSET_FULLREF_BUF_SIZE);
    
    expected_allowed=0;
    expected_required=0;
    
    strFailReason="";
    
    if(allowed)
    {
        expected_allowed=*allowed;
        *allowed=MC_PTP_ALL;
    }
    if(required)
    {
        expected_required=*required;
        *required=0;
    }
    if(mc_gState->m_NetworkParams->IsProtocolMultichain())
    {
        disallow_if_assets_found=0;
        const CScript& script1 = txout.scriptPubKey;        
        CScript::const_iterator pc1 = script1.begin();

        lpScript->Clear();
        lpScript->SetScript((unsigned char*)(&pc1[0]),(size_t)(script1.end()-pc1),MC_SCR_TYPE_SCRIPTPUBKEY);
        if(allowed)                                                             // Checking permissions this output address have
        {
            CTxDestination addressRet;        

            *allowed=0;
            if(ExtractDestinationScriptValid(script1, addressRet))
            {
                *allowed=CheckRequiredPermissions(addressRet,expected_allowed,mapSpecialEntity,NULL);
                if(expected_allowed & MC_PTP_SEND)                
                {
                    if(*allowed & MC_PTP_SEND)
                    {
                        if(*allowed & MC_PTP_RECEIVE)
                        {
                            *allowed -= MC_PTP_RECEIVE;
                        }
                        else
                        {
                            *allowed -= MC_PTP_SEND;
                            if(mc_gState->m_Features->AnyoneCanReceiveEmpty())                                
                            {
                                if(txout.nValue == 0)
                                {
                                    disallow_if_assets_found=1;
                                    *allowed |= MC_PTP_SEND;
                                }
                            }
                        }
                    }
                }
            }
        }    
    
        issue_found=false;
        total=0;
        for (int e = 0; e < lpScript->GetNumElements(); e++)
        {
            lpScript->SetElement(e);
            err=lpScript->GetAssetGenesis(&quantity);
            if(err == 0)
            {
                issue_found=true;
                if(quantity+total<0)
                {
                    strFailReason="Invalid asset genesis script";
                    return false;                                        
                }

                total+=quantity;                    
            }            
            else
            {
                if(err != MC_ERR_WRONG_SCRIPT)
                {
                    strFailReason="Invalid asset genesis script";
                    return false;                                    
                }
            }        
        }
         
        if(issue_found)
        {
            if(IsLicenseTokenIssuance(lpScript,hash))
            {
                issue_found=false;
            }
        }
        
        if(issue_found)                                                         
        {                
            if(hash != 0)
            {
                mc_EntityDetails entity;
                if(mc_gState->m_Assets->FindEntityByTxID(&entity,(unsigned char*)&hash))
                {
                    if(disallow_if_assets_found)
                    {
                        disallow_if_assets_found=0;
                        *allowed -= MC_PTP_SEND;                        
                    }
                    
                    memcpy(buf,entity.GetFullRef(),MC_AST_ASSET_FULLREF_SIZE);
                    row=amounts->Seek(buf);
                    last=0;
                    if(row >= 0)
                    {
                        last=mc_GetABQuantity(amounts->GetRow(row));
                        total+=last;
                        mc_SetABQuantity(amounts->GetRow(row),total);
                    }
                    else
                    {
                        mc_SetABQuantity(buf,total);
                        amounts->Add(buf);                        
                    }

                    if(required)
                    {
                        if(expected_required == 0)                          
                        {
                            *required |= MC_PTP_ISSUE;
                        }                            
                    }
                }                
                else                                                            // Asset not found, no error but the caller should check required field
                {
                    if(required)                                                
                    {
                        *required |= MC_PTP_ISSUE;
                    }
                }
            }
            else                                                                // New unconfirmed genesis                                            
            {
                memset(buf,0,MC_AST_ASSET_FULLREF_SIZE);
                mc_SetABRefType(buf,MC_AST_ASSET_REF_TYPE_GENESIS);
                mc_SetABQuantity(buf,total);
                amounts->Add(buf);
                if(required)
                {
                    *required |= MC_PTP_ISSUE;
                }
            }
        }
        
        if(lpScript->IsOpReturnScript() == 0)
        {
            for (int e = 0; e < lpScript->GetNumElements(); e++)                // Parsing asset quantities
            {
                lpScript->SetElement(e);
                err=lpScript->GetAssetQuantities(amounts,MC_SCR_ASSET_SCRIPT_TYPE_TRANSFER);       // Buffer is updated, new rows are added if needed 
                if((err != MC_ERR_NOERROR) && (err != MC_ERR_WRONG_SCRIPT))
                {
                    strFailReason="Invalid asset transfer script";
                    return false;                                
                }
                
                if(disallow_if_assets_found)                                    // Cannot use in coin selection as this is non-empty output without receive permission
                {
                    if(err != MC_ERR_WRONG_SCRIPT)
                    {
                        disallow_if_assets_found=0;
                        *allowed -= MC_PTP_SEND;                        
                    }
                }
                                
                if(hash != 0)                                                   // Follow-ons
                {
                    err=lpScript->GetAssetQuantities(amounts,MC_SCR_ASSET_SCRIPT_TYPE_FOLLOWON);                  
                    if((err != MC_ERR_NOERROR) && (err != MC_ERR_WRONG_SCRIPT))
                    {
                        strFailReason="Invalid asset followon script";
                        return false;                                
                    }
                    if(disallow_if_assets_found)
                    {
                        if(err != MC_ERR_WRONG_SCRIPT)
                        {
                            disallow_if_assets_found=0;
                            *allowed -= MC_PTP_SEND;                        
                        }
                    }
                }
                else
                {
                    uint32_t script_type=0;
                    unsigned char ref[MC_AST_ASSET_FULLREF_SIZE];
                    err=lpScript->GetFullRef(ref,&script_type);

                    switch(script_type)
                    {
                        case MC_SCR_ASSET_SCRIPT_TYPE_FOLLOWON:
                            mc_EntityDetails entity;
                            if(mc_gState->m_Assets->FindEntityByFullRef(&entity,ref))
                            {
                                if(required)
                                {
                                    *required |= MC_PTP_ISSUE;                    
                                }
                                if(mapSpecialEntity)
                                {
                                    std::map<uint32_t,uint256>::const_iterator it = mapSpecialEntity->find(MC_PTP_ISSUE);
                                    if (it == mapSpecialEntity->end())
                                    {
                                        mapSpecialEntity->insert(make_pair(MC_PTP_ISSUE,*(uint256*)(entity.GetTxID())));
                                    }
                                    else
                                    {
                                        if(it->second != *(uint256*)(entity.GetTxID()))
                                        {
                                            strFailReason="Invalid asset follow-on script, multiple assets";
                                            return false;                                                                                                            
                                        }
                                    }
                                }
                            }
                            else
                            {
                                strFailReason="Invalid asset follow-on script, asset not found";
                                return false;                                                                    
                            }

                            break;
                    }
                }

            }    
        }
        else                                                                    // OP_RETURN outputs
        {
            if(required)
            {
                if(hash == 0)
                {
                    if(lpScript->GetNumElements() == 2)                         // Create entity
                    {
                        lpScript->SetElement(0);
                        if(lpScript->GetNewEntityType(&new_entity_type) == 0)    
                        {
                            if(new_entity_type == MC_ENT_TYPE_STREAM)
                            {
                                *required |= MC_PTP_CREATE;                    
                            }
                            if(new_entity_type == MC_ENT_TYPE_VARIABLE)
                            {
                                *required |= MC_PTP_CREATE;                    
                            }
                            if(new_entity_type == MC_ENT_TYPE_UPGRADE)
                            {
                                *required |= MC_PTP_CREATE | MC_PTP_ADMIN;                    
                            }
                        }
                    }
                    
                    if(lpScript->GetNumElements() == 3) 
                    {
                        lpScript->SetElement(1);

                        if(lpScript->GetApproval(&approval,&timestamp) == 0)
                        {
                            *required |= MC_PTP_ADMIN;                    
                        }
                    }
                    
                    if(lpScript->GetNumElements() == 3)                         // Publish or set variable
                    {
                        unsigned char short_txid[MC_AST_SHORT_TXID_SIZE];
                        lpScript->SetElement(0);
                        if(lpScript->GetEntity(short_txid) == 0)    
                        {
                            mc_EntityDetails entity;
                            if(mc_gState->m_Assets->FindEntityByShortTxID(&entity,short_txid))
                            {
                                if(entity.GetEntityType() == MC_ENT_TYPE_STREAM)
                                {
                                    if(entity.AnyoneCanWrite() == 0)
                                    {
                                        if(mapSpecialEntity)
                                        {
                                            if(required)
                                            {
                                                *required |= MC_PTP_WRITE;                    
                                            }
                                            std::map<uint32_t,uint256>::const_iterator it = mapSpecialEntity->find(MC_PTP_WRITE);
                                            if (it == mapSpecialEntity->end())
                                            {
                                                mapSpecialEntity->insert(make_pair(MC_PTP_WRITE,*(uint256*)(entity.GetTxID())));
                                            }
                                            else
                                            {
                                                if(it->second != *(uint256*)(entity.GetTxID()))
                                                {
                                                    strFailReason="Invalid publish script, multiple streams";
                                                    return false;                                                                                                            
                                                }
                                            }
                                        }                                    
                                    }
                                }
                                if(entity.GetEntityType() == MC_ENT_TYPE_VARIABLE)
                                {
                                    if(mapSpecialEntity)
                                    {
                                        if(required)
                                        {
                                            *required |= MC_PTP_WRITE;                    
                                        }
                                        std::map<uint32_t,uint256>::const_iterator it = mapSpecialEntity->find(MC_PTP_WRITE);
                                        if (it == mapSpecialEntity->end())
                                        {
                                            mapSpecialEntity->insert(make_pair(MC_PTP_WRITE,*(uint256*)(entity.GetTxID())));
                                        }
                                        else
                                        {
                                            if(it->second != *(uint256*)(entity.GetTxID()))
                                            {
                                                strFailReason="Invalid publish script, multiple variables";
                                                return false;                                                                                                            
                                            }
                                        }
                                    }                                    
                                }
                            }                        
                            else
                            {
                                strFailReason="Invalid publish script, stream not found";
                                return false;                                                                    
                            }
                        }
                        else
                        {
                            strFailReason="Invalid publish script";
                            return false;                                                                    
                        }
                    }
                }
            }        
        }
        
        if(required)
        {
            *required |= MC_PTP_SEND;
            
            if(lpScript->IsOpReturnScript() == 0)
            {
                type_ored=0;
                mc_EntityDetails entity;
                uint32_t admin_type;
                entity.Zero();
                for (int e = 0; e < lpScript->GetNumElements(); e++)            // Parsing permissions
                {
                    unsigned char short_txid[MC_AST_SHORT_TXID_SIZE];
                    lpScript->SetElement(e);                    
                    if(lpScript->GetEntity(short_txid) == 0)                    // Entity element
                    {
                        if(mc_gState->m_Assets->FindEntityByShortTxID(&entity,short_txid) == 0)
                        {
                            strFailReason="Entity not found";
                            return false;                                                                    
                        }                        
                    }
                    
                    if(lpScript->GetPermission(&type,&from,&to,&timestamp) == 0)// Permission script found, admin permission needed
                    {
                        if(mc_gState->m_Permissions->IsActivateEnough(type))
                        {
                            admin_type=MC_PTP_ACTIVATE;
                        }
                        else
                        {
                            admin_type=MC_PTP_ADMIN;
                        }                        
                        *required |= admin_type;
                        if( type & (MC_PTP_ADMIN | MC_PTP_MINE) )
                        {
                            if(mc_gState->m_NetworkParams->GetInt64Param("supportminerprecheck"))                                
                            {
                                if(entity.GetEntityType() == MC_ENT_TYPE_NONE)
                                {
                                    *required |= MC_PTP_CACHED_SCRIPT_REQUIRED;
                                }
                            }        
                        }
                        
                        if(hash == 0)
                        {
                            if(entity.GetEntityType())
                            {
                                if(mapSpecialEntity)
                                {
                                    std::map<uint32_t,uint256>::const_iterator it = mapSpecialEntity->find(admin_type);
                                    if (it == mapSpecialEntity->end())
                                    {
                                        mapSpecialEntity->insert(make_pair(admin_type,*(uint256*)(entity.GetTxID())));
                                    }
                                    else
                                    {
                                        if(it->second != *(uint256*)(entity.GetTxID()))
                                        {
                                            strFailReason="Invalid permission script, multiple entities";
                                            return false;                                                                                                            
                                        }
                                    }
                                }                                    
                            }
                        }
                        type_ored |= type;
                        entity.Zero();
                    }                        
                }

                if(expected_required & MC_PTP_RECEIVE)                          // Checking for dust
                {
                    if( (type_ored == 0) && (lpScript->IsOpReturnScript() == 0) )
                    {
                        if (txout.IsDust(::minRelayTxFee))
                        {
                            strFailReason="Transaction output value too small";
                            return false;                                
                        }            
                    }
                }
            }
            
            AppendSpecialRowsToBuffer(amounts,hash,expected_allowed,expected_required,allowed,required,txout.nValue);
        }
        
    }    
    else                                                                        // Protocol != multichain
    {
        if(allowed)
        {
            *allowed=MC_PTP_SEND;
        }    
        if(required)
        {
            *required |= MC_PTP_SEND;
            if(expected_required & MC_PTP_RECEIVE)                              // Checking for dust
            {
                if (txout.IsDust(::minRelayTxFee))
                {
                    strFailReason="Transaction output value too small";
                    return false;                                
                }            
            }
            memset(buf,0,MC_AST_ASSET_FULLREF_BUF_SIZE);
            type=MC_PTP_SEND;
            mc_PutLE(buf+4,&type,4);
            mc_SetABRefType(buf,MC_AST_ASSET_REF_TYPE_SPECIAL);
            quantity=txout.nValue;
            row=amounts->Seek(buf);
            if(row >= 0)
            {
                quantity+=mc_GetABQuantity(amounts->GetRow(row));
                mc_SetABQuantity(amounts->GetRow(row),quantity);
            }
            else
            {
                mc_SetABQuantity(buf,quantity);
                amounts->Add(buf);                            
            }            
        }        
    }

    return true;
}

bool ParseMultichainTxOutToBuffer(uint256 hash,                                 // IN, tx hash, if !=0 genesis asset reference is retrieved from asset DB
                                  const CTxOut& txout,                          // IN, tx to be parsed
                                  mc_Buffer *amounts,                           // OUT, output amount buffer
                                  mc_Script *lpScript,                          // TMP, temporary script object
                                  int *allowed,                                 // IN/OUT/NULL returns permissions of output address ANDed with input value 
                                  int *required,                                // IN/OUT/NULL returns permission required by this output, adds special rows to output buffer according to input value   
                                  string& strFailReason)                        // OUT error
{
    return ParseMultichainTxOutToBuffer(hash,txout,amounts,lpScript,allowed,required,NULL,strFailReason);
}

bool CreateAssetBalanceList(const CTxOut& txout,mc_Buffer *amounts,mc_Script *lpScript,int *required)
{
    string strFailReason;
    
    unsigned char buf[MC_AST_ASSET_FULLREF_BUF_SIZE];
    int err;
    int64_t quantity,total;
    bool issue_found=false;
    uint32_t type,from,to,timestamp;
    
    memset(buf,0,MC_AST_ASSET_FULLREF_BUF_SIZE);
    
    if(required)
    {
        *required=0;
    }
    
    if(mc_gState->m_NetworkParams->IsProtocolMultichain())
    {
        const CScript& script1 = txout.scriptPubKey;        
        CScript::const_iterator pc1 = script1.begin();

        lpScript->Clear();
        lpScript->SetScript((unsigned char*)(&pc1[0]),(size_t)(script1.end()-pc1),MC_SCR_TYPE_SCRIPTPUBKEY);
            
        total=0;
        for (int e = 0; e < lpScript->GetNumElements(); e++)
        {
            lpScript->SetElement(e);
            err=lpScript->GetAssetGenesis(&quantity);
            if(err == 0)
            {
                issue_found=true;
                if(quantity+total<0)
                {
                    return false;                                        
                }

                total+=quantity;                    
            }            
            else
            {
                if(err != MC_ERR_WRONG_SCRIPT)
                {
                    return false;                                    
                }
            }        
                                
            if(required)
            {
                if(lpScript->GetPermission(&type,&from,&to,&timestamp) == 0)    
                {
                    if(mc_gState->m_Permissions->IsActivateEnough(type))
                    {
                        *required |= MC_PTP_ACTIVATE;
                    }
                    else
                    {
                        *required |= MC_PTP_ADMIN;
                    }
                }                        
            }
        }
        
        if(issue_found)                                                         // Checking that genesis was confirmed at least once
        {
            if(required)
            {
                *required |= MC_PTP_ISSUE;
            }
            memset(buf,0,MC_AST_ASSET_FULLREF_SIZE);
            mc_SetABRefType(buf,MC_AST_ASSET_REF_TYPE_GENESIS);
            mc_SetABQuantity(buf,total);
            amounts->Add(buf);
        }

        for (int e = 0; e < lpScript->GetNumElements(); e++)                    // Parsing asset quantities
        {
            lpScript->SetElement(e);
            err=lpScript->GetAssetQuantities(amounts,MC_SCR_ASSET_SCRIPT_TYPE_TRANSFER); // Buffer is updated, new rows are added if needed 
            
            if((err != MC_ERR_NOERROR) && (err != MC_ERR_WRONG_SCRIPT))
            {
                return false;                                
            }

            err=lpScript->GetAssetQuantities(amounts,MC_SCR_ASSET_SCRIPT_TYPE_FOLLOWON);   
            
            if(err == 0)
            {
                if(required)
                {
                    *required |= MC_PTP_ISSUE;
                }                
            }
            if((err != MC_ERR_NOERROR) && (err != MC_ERR_WRONG_SCRIPT))
            {
                return false;                                
            }

        }            
    }    

    return true;

    
}

bool CreateAssetBalanceList(const CTxOut& txout,mc_Buffer *amounts,mc_Script *lpScript)
{
    return CreateAssetBalanceList(txout,amounts,lpScript,NULL);
}

bool FindFollowOnsInScript(const CScript& script1,mc_Buffer *amounts,mc_Script *lpScript)
{
    int err;
    if(mc_gState->m_NetworkParams->IsProtocolMultichain())
    {
        CScript::const_iterator pc1 = script1.begin();

        lpScript->Clear();
        lpScript->SetScript((unsigned char*)(&pc1[0]),(size_t)(script1.end()-pc1),MC_SCR_TYPE_SCRIPTPUBKEY);
    }    
    
    for (int e = 0; e < lpScript->GetNumElements(); e++)                        // Parsing asset quantities
    {
        lpScript->SetElement(e);
        err=lpScript->GetAssetQuantities(amounts,MC_SCR_ASSET_SCRIPT_TYPE_FOLLOWON);   
        if((err != MC_ERR_NOERROR) && (err != MC_ERR_WRONG_SCRIPT))
        {
            return false;                                
        }
    }            
    return true;
}

void LogAssetTxOut(string message,uint256 hash,int index,unsigned char* assetrefbin,int64_t quantity)
{
    string txid=hash.GetHex();
    
    string assetref="";
    if(assetrefbin)
    {
        if(mc_GetABRefType(assetrefbin) == MC_AST_ASSET_REF_TYPE_SHORT_TXID)
        {
            for(int i=0;i<8;i++)
            {
                assetref += strprintf("%02x",assetrefbin[MC_AST_SHORT_TXID_OFFSET+MC_AST_SHORT_TXID_SIZE-i-1]);
            }
        }
        else
        {
            assetref += itostr((int)mc_GetLE(assetrefbin,4));
            assetref += "-";
            assetref += itostr((int)mc_GetLE(assetrefbin+4,4));
            assetref += "-";
            assetref += itostr((int)mc_GetLE(assetrefbin+8,2));        
        }
    }
    else
    {
        assetref += "0-0-2";
    }
    if(fDebug)LogPrint("mcatxo", "mcatxo: %s: %s-%d %s %ld\n",message.c_str(),txid.c_str(),index,assetref.c_str(),quantity);
}

bool AddressCanReceive(CTxDestination address)
{
    if(mc_gState->m_NetworkParams->IsProtocolMultichain() == 0)
    {
        return true;
    }
    CKeyID *lpKeyID=boost::get<CKeyID> (&address);
    CScriptID *lpScriptID=boost::get<CScriptID> (&address);
    if((lpKeyID == NULL) && (lpScriptID == NULL))
    {
        LogPrintf("mchn: Invalid address");
        return false;                
    }
    unsigned char* ptr=NULL;
    CBitcoinAddress addressPrint;
    if(lpKeyID != NULL)
    {
        addressPrint=CBitcoinAddress(*lpKeyID);
        ptr=(unsigned char*)(lpKeyID);
    }
    else
    {
        addressPrint=CBitcoinAddress(*lpScriptID);
        ptr=(unsigned char*)(lpScriptID);
    }
    
    if(mc_gState->m_Permissions->CanReceive(NULL,ptr) == 0)
    {
        LogPrintf("mchn: Destination address doesn't have receive permission: %s\n",
                addressPrint.ToString().c_str());
        return false;
    }
    
    return true;
}

int CheckRequiredPermissions(const CTxDestination& addressRet,int expected_allowed,map<uint32_t, uint256>* mapSpecialEntity, string *strFailReason)
{
    int allowed=0;
    const unsigned char *aptr;

    aptr=GetAddressIDPtr(addressRet);
    if(aptr)
    {
        if(expected_allowed & MC_PTP_SEND)
        {
            if(mc_gState->m_Permissions->CanSend(NULL,aptr))
            {
                allowed |= MC_PTP_SEND;
                if(mc_gState->m_Permissions->CanReceive(NULL,aptr)) 
                {
                    allowed |= MC_PTP_RECEIVE;
                }            
            }    
            if(strFailReason)
            {
                if( (allowed & MC_PTP_SEND) == 0 )
                {
                    *strFailReason="from-address doesn't have send or receive permission";
                }
            }
        }
        if(expected_allowed & MC_PTP_WRITE)
        {
            unsigned char *lpEntity=NULL;
            if(mapSpecialEntity)
            {
                std::map<uint32_t,uint256>::const_iterator it = mapSpecialEntity->find(MC_PTP_WRITE);
                if (it != mapSpecialEntity->end())
                {
                    lpEntity=(unsigned char*)(&(it->second));
                }
            }

            if(mc_gState->m_Permissions->CanWrite(lpEntity,aptr))
            {
                allowed |= MC_PTP_WRITE;
            }                                                 
            if(strFailReason)
            {
                if( (allowed & MC_PTP_WRITE) == 0 )
                {
                    *strFailReason="Publishing in this stream is not allowed from this address";
                }
            }
        }
        if(expected_allowed & MC_PTP_ISSUE)
        {
            unsigned char *lpEntity=NULL;

            if(mapSpecialEntity)
            {
                std::map<uint32_t,uint256>::const_iterator it = mapSpecialEntity->find(MC_PTP_ISSUE);
                if (it != mapSpecialEntity->end())
                {
                    lpEntity=(unsigned char*)(&(it->second));
                }
            }

            if(mc_gState->m_Permissions->CanIssue(lpEntity,aptr))
            {
                allowed |= MC_PTP_ISSUE;
            }                 
            
            if(strFailReason)
            {
                if( (allowed & MC_PTP_ISSUE) == 0 )
                {
                    if(lpEntity)
                    {
                        *strFailReason="Issuing more units for this asset is not allowed from this address";                        
                    }
                    else
                    {
                        *strFailReason="from-address doesn't have issue permission";
                    }
                }
            }            
        }
        if(expected_allowed & MC_PTP_CREATE)
        {
            if(mc_gState->m_Permissions->CanCreate(NULL,aptr))
            {
                allowed |= MC_PTP_CREATE;
            }                         
            if(strFailReason)
            {
                if( (allowed & MC_PTP_CREATE) == 0 )
                {
                    *strFailReason="from-address doesn't have create permission";
                }
            }
        }
        if(expected_allowed & MC_PTP_ADMIN)
        {
            unsigned char *lpEntity=NULL;

            if(mapSpecialEntity)
            {
                std::map<uint32_t,uint256>::const_iterator it = mapSpecialEntity->find(MC_PTP_ADMIN);
                if (it != mapSpecialEntity->end())
                {
                    lpEntity=(unsigned char*)(&(it->second));
                }
            }

            if(mc_gState->m_Permissions->CanAdmin(lpEntity,aptr))
            {
                allowed |= MC_PTP_ADMIN;
            }                                                 
            if(strFailReason)
            {
                if( (allowed & MC_PTP_ADMIN) == 0 )
                {
                    if(lpEntity)
                    {
                        *strFailReason="from-address doesn't have admin permission for this entity";                        
                    }
                    else
                    {
                        *strFailReason="from-address doesn't have admin permission";
                    }
                }
            }            
        }
        if(expected_allowed & MC_PTP_ACTIVATE)
        {
            unsigned char *lpEntity=NULL;
            if(mapSpecialEntity)
            {
                std::map<uint32_t,uint256>::const_iterator it = mapSpecialEntity->find(MC_PTP_ACTIVATE);
                if (it != mapSpecialEntity->end())
                {
                    lpEntity=(unsigned char*)(&(it->second));
                }
            }

            if(mc_gState->m_Permissions->CanActivate(lpEntity,aptr))
            {
                allowed |= MC_PTP_ACTIVATE;
            }                                                 
            if(strFailReason)
            {
                if( (allowed & MC_PTP_ACTIVATE) == 0 )
                {
                    if(lpEntity)
                    {
                        *strFailReason="from-address doesn't have activate or admin permission for this entity";                        
                    }
                    else
                    {
                        *strFailReason="from-address doesn't have admin or activate permission";
                    }
                }
            }            
        }
    }
    
    return allowed;
}

