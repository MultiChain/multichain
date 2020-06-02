// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "core/main.h"
#include "utils/util.h"
#include "utils/utilparse.h"
#include "community/license.h"
#include "multichain/multichain.h"
#include "structs/base58.h"
#include "custom/custom.h"
#include "filters/multichainfilter.h"

extern mc_MultiChainFilterEngine* pMultiChainFilterEngine;

using namespace std;

string EncodeHexTx(const CTransaction& tx);
bool ExtractDestinations10008(const CScript& scriptPubKey, txnouttype& typeRet, vector<CTxDestination>& addressRet, int& nRequiredRet, bool no_clear, bool *not_cleared);

#define MC_MTX_OUTPUT_DETAIL_FLAG_NONE                   0x00000000                              
#define MC_MTX_OUTPUT_DETAIL_FLAG_OP_RETURN_ENTITY_ITEM  0x00000001                              
#define MC_MTX_OUTPUT_DETAIL_FLAG_PERMISSION_CREATE      0x00000002                              
#define MC_MTX_OUTPUT_DETAIL_FLAG_PERMISSION_ADMIN       0x00000004                              
#define MC_MTX_OUTPUT_DETAIL_FLAG_NOT_PURE_PERMISSION    0x00000008                              
#define MC_MTX_OUTPUT_DETAIL_FLAG_NOT_OP_RETURN          0x00000010                              
#define MC_MTX_OUTPUT_DETAIL_FLAG_FOLLOWON_DETAILS       0x00000020                              
#define MC_MTX_OUTPUT_DETAIL_FLAG_NO_DESTINATION         0x00000040                              
#define MC_MTX_OUTPUT_DETAIL_FLAG_PERMISSION_FILTER      0x00000080                              
#define MC_MTX_OUTPUT_DETAIL_FLAG_PERMISSION_NOT_FILTER  0x00000100                              
#define MC_MTX_OUTPUT_DETAIL_FLAG_LICENSE_TRANSFER       0x00000200

typedef struct CMultiChainTxDetails
{    
    bool fCheckCachedScript;                                                    // If tx contains admin/miner grants, it should have cached input script
    bool fScriptHashAllFound;                                                   // Input script with SIGHASH_ALL found, OP_RETURN metadata data can be transferred in this tx
    bool fRejectIfOpDropOpReturn;                                               // Tx should be rejected of OP_DROP+OP_RETURN script is found
    bool fSeedNodeInvolved;                                                     // Connect permission of seed node changed
    bool fFullReplayCheckRequired;                                              // Tx should be rechecked when mempool is replayed
    bool fAdminMinerGrant;                                                      // Admin/miner grant in this transaction
    bool fAssetIssuance;                                                        // New/followon issuance in this tx 
    bool fIsStandardCoinbase;                                                   // Tx is standard coinbase - filters should not be applied
    bool fLicenseTokenIssuance;                                                 // New license token
    bool fLicenseTokenTransfer;                                                 // License token transfer
    
    vector <txnouttype> vInputScriptTypes;                                      // Input script types
    vector <uint160> vInputDestinations;                                        // Addresses used in input scripts
    vector <int> vInputHashTypes;                                               // Input hash types
    vector <bool> vInputCanGrantAdminMine;                                      // Flags - input can be used as signer for admin/mine grants
    vector <bool> vInputHadAdminPermissionBeforeThisTx;                         // Admin permissions before this tx - for approval
    
    set <string> vAllowedAdmins;                                                // Admin permissions before this tx - for grants
    set <string> vAllowedActivators;                                            // Activate permissions before this tx - for grants
    set <uint160> vRelevantEntities;                                            // Set of entities involved in this transaction
    
    vector <uint32_t> vOutputScriptFlags;                                       // Output script flags, filled when script is processed for the first time
    vector <int> vOutputPermissionRequired;                                     // Number of required receive permissions 
    vector < vector<CTxDestination> > vOutputDestinations;                      // Output destinations
    vector < CTxDestination > vOutputSingleDestination;                         // Single destination for setting permission
    

    unsigned char details_script[MC_ENT_MAX_SCRIPT_SIZE];                       // Entity details script
    int details_script_size;                                                    // Entity details script size
    int details_script_type;                                                    // Entity details script type - new/update
    int extended_script_row;                                                    // Entity details script size
    uint32_t new_entity_type;                                                   // New entity type
    int new_entity_output;                                                      // Output where new entity is defined
    int64_t total_offchain_size;                                                // Total size of offchain items
    int64_t total_value_in;                                                     // Total amount in inputs
    int64_t total_value_out;                                                    // Total amount in outputs
    int emergency_disapproval_output;                                           // Output carrying emergency filter disapproval - bypassed by filters
    
    CMultiChainTxDetails()
    {
        Zero();
    }
    
    ~CMultiChainTxDetails()
    {
        
    }
    
    void Zero();
    bool IsRelevantInput(int vin,int vout);
    void SetRelevantEntity(void *entity);
    bool IsRelevantEntity(uint160 hash);
    
} CMultiChainTxDetails;

void CMultiChainTxDetails::Zero()
{
    vInputScriptTypes.clear();
    vInputDestinations.clear();
    vInputHashTypes.clear();
    vInputCanGrantAdminMine.clear();
    vInputHadAdminPermissionBeforeThisTx.clear();
    
    vAllowedAdmins.clear();
    vAllowedActivators.clear();
    
    vOutputScriptFlags.clear();
    vOutputPermissionRequired.clear();
    vOutputDestinations.clear();
    vOutputSingleDestination.clear();
    
    fCheckCachedScript=false;
    fScriptHashAllFound=false;
    fRejectIfOpDropOpReturn=false;
    fSeedNodeInvolved=false;
    fFullReplayCheckRequired=false;
    fAdminMinerGrant=false;
    fAssetIssuance=false;
    fIsStandardCoinbase=false;
    fLicenseTokenIssuance=false;
    fLicenseTokenTransfer=false;
    
    details_script_size=0;
    details_script_type=-1;
    extended_script_row=0;
    new_entity_type=MC_ENT_TYPE_NONE;
    new_entity_output=-1;
    total_offchain_size=0;
    total_value_in=0;
    total_value_out=0;
    emergency_disapproval_output=-1;
}

bool CMultiChainTxDetails::IsRelevantInput(int vin, int vout)
{
    if( (vInputHashTypes[vin] == SIGHASH_ALL) || ( (vInputHashTypes[vin] == SIGHASH_SINGLE) && ((vin == vout) ) ) )
    {
        return true;
    }
    return false;
}

bool CMultiChainTxDetails::IsRelevantEntity(uint160 hash)
{
    if(vRelevantEntities.find(hash) != vRelevantEntities.end())
    {
        return true;
    }
    return false;
}

void CMultiChainTxDetails::SetRelevantEntity(void *entity)
{
    uint160 hash=0;
    memcpy(&hash,entity,MC_AST_SHORT_TXID_SIZE);
    if(!IsRelevantEntity(hash))
    {
        vRelevantEntities.insert(hash);
    }
}

uint160 mc_GenesisAdmin(const CTransaction& tx)
{
    uint32_t type,from,to,timestamp;    
    for (unsigned int j = 0; j < tx.vout.size(); j++)
    {
        mc_gState->m_TmpScript->Clear();
        const CScript& script1 = tx.vout[j].scriptPubKey;        
        CScript::const_iterator pc1 = script1.begin();
        CTxDestination addressRet;

        mc_gState->m_TmpScript->SetScript((unsigned char*)(&pc1[0]),(size_t)(script1.end()-pc1),MC_SCR_TYPE_SCRIPTPUBKEY);
        for (int e = 0; e < mc_gState->m_TmpScript->GetNumElements(); e++)
        {
            mc_gState->m_TmpScript->SetElement(e);
            if(mc_gState->m_TmpScript->GetPermission(&type,&from,&to,&timestamp) == 0)
            {
                if(type == MC_PTP_GLOBAL_ALL)
                {
                    CTxDestination addressRet;
                    if(ExtractDestination(script1, addressRet))
                    {
                        CKeyID *lpKeyID=boost::get<CKeyID> (&addressRet);
                        if(lpKeyID)
                        {
                            return *(uint160*)lpKeyID;                                                   
                        }
                    }                                        
                }
            }
        }
    }
    return 0;
}

bool mc_ExtractInputAssetQuantities(mc_Buffer *assets, const CScript& script1, uint256 hash, string& reason)
{
    int err;
    int64_t quantity;
    CScript::const_iterator pc1 = script1.begin();

    mc_gState->m_TmpScript->Clear();
    mc_gState->m_TmpScript->SetScript((unsigned char*)(&pc1[0]),(size_t)(script1.end()-pc1),MC_SCR_TYPE_SCRIPTPUBKEY);
        
    for (int e = 0; e < mc_gState->m_TmpScript->GetNumElements(); e++)
    {
        mc_gState->m_TmpScript->SetElement(e);
        err=mc_gState->m_TmpScript->GetAssetQuantities(assets,MC_SCR_ASSET_SCRIPT_TYPE_TRANSFER | MC_SCR_ASSET_SCRIPT_TYPE_FOLLOWON);
        if((err != MC_ERR_NOERROR) && (err != MC_ERR_WRONG_SCRIPT))
        {
            reason="Asset transfer script rejected - error in script";
            return false;                                
        }
        err=mc_gState->m_TmpScript->GetAssetGenesis(&quantity);
        if(err == 0)
        {
            mc_EntityDetails entity;
            unsigned char buf_amounts[MC_AST_ASSET_FULLREF_BUF_SIZE];
            memset(buf_amounts,0,MC_AST_ASSET_FULLREF_BUF_SIZE);
            if(mc_gState->m_Assets->FindEntityByTxID(&entity,(unsigned char*)&hash))
            {
                memcpy(buf_amounts,entity.GetFullRef(),MC_AST_ASSET_FULLREF_SIZE);
                int row=assets->Seek(buf_amounts);
                if(row>=0)
                {
                    int64_t last=mc_GetABQuantity(assets->GetRow(row));
                    quantity+=last;
                    mc_SetABQuantity(assets->GetRow(row),quantity);                        
                }
                else
                {
                    mc_SetABQuantity(buf_amounts,quantity);
                    assets->Add(buf_amounts);
                }
            }                
            else
            {
                reason="Asset transfer script rejected - issue tx not found";
                return false;                                
            }                
        }            
        else
        {
            if(err != MC_ERR_WRONG_SCRIPT)
            {
                reason="Asset transfer script rejected - error in input issue script";
                return false;                                    
            }
        }
    }

    return true;
}

bool mc_CompareAssetQuantities(CMultiChainTxDetails *details,string& reason)
{
    unsigned char *ptrIn;
    unsigned char *ptrOut;
    int64_t quantity;
    mc_EntityDetails entity;

    for(int i=0;i<mc_gState->m_TmpAssetsIn->GetCount();i++)
    {
        ptrIn=mc_gState->m_TmpAssetsIn->GetRow(i);
        int row=mc_gState->m_TmpAssetsOut->Seek(ptrIn);
        quantity=mc_GetABQuantity(ptrIn);
        if(quantity>0)
        {
            if(row>=0)
            {
                ptrOut=mc_gState->m_TmpAssetsOut->GetRow(row);       
                if(memcmp(ptrIn,ptrOut,MC_AST_ASSET_QUANTITY_OFFSET+MC_AST_ASSET_QUANTITY_SIZE))
                {
                    reason="Asset transfer script rejected - mismatch in input/output quantities";
                    return false;                                                                    
                }
            }
            else
            {
                reason="Asset transfer script rejected - mismatch in input/output quantities";
                return false;                                                    
            }
        }
    }
    
    for(int i=0;i<mc_gState->m_TmpAssetsOut->GetCount();i++)
    {
        ptrOut=mc_gState->m_TmpAssetsOut->GetRow(i);
        int row=mc_gState->m_TmpAssetsIn->Seek(ptrOut);
        quantity=mc_GetABQuantity(ptrOut);

        if(mc_gState->m_Assets->FindEntityByFullRef(&entity,ptrOut) == 0)
        {
            reason="Asset transfer script rejected - asset not found";
            return false;                                                    
        }                           
        
        details->SetRelevantEntity((unsigned char*)entity.GetTxID()+MC_AST_SHORT_TXID_OFFSET);
        
        if(quantity>0)
        {
            if(row>=0)
            {
                ptrIn=mc_gState->m_TmpAssetsIn->GetRow(row);       
                if(memcmp(ptrIn,ptrOut,MC_AST_ASSET_QUANTITY_OFFSET+MC_AST_ASSET_QUANTITY_SIZE))
                {
                    reason="Asset transfer script rejected - mismatch in input/output quantities";
                    return false;                                                                    
                }
            }
            else
            {
                reason="Asset transfer script rejected - mismatch in input/output quantities";
                return false;                                                    
            }
        }
    }

    return true;
}

void MultiChainTransaction_SetTmpOutputScript(const CScript& script1)
{
    CScript::const_iterator pc1 = script1.begin();
    mc_gState->m_TmpScript->Clear();
    mc_gState->m_TmpScript->SetScript((unsigned char*)(&pc1[0]),(size_t)(script1.end()-pc1),MC_SCR_TYPE_SCRIPTPUBKEY);
}

bool MultiChainTransaction_CheckCachedScriptFlag(const CTransaction& tx)
{
    bool flag=false;
    if(mc_gState->m_NetworkParams->GetInt64Param("supportminerprecheck"))                                
    {
        flag=true;
    }        
    if(tx.IsCoinBase())
    {
        flag=false;
    }
    return flag;
}

bool MultiChainTransaction_CheckCoinbaseInputs(const CTransaction& tx,     
                                               CMultiChainTxDetails *details)
{
    if(!tx.IsCoinBase())                                                  
    {   
        return false;
    }    
    
    if(mc_gState->m_Permissions->m_Block == -1)                     
    {                    
        details->vInputScriptTypes.push_back(TX_PUBKEYHASH);
        details->vInputDestinations.push_back(mc_GenesisAdmin(tx));             // Genesis admin is considered to be admin/opener of everything in genesis coinbase        
    }
    else
    {
        details->vInputScriptTypes.push_back(TX_NONSTANDARD);
        details->vInputDestinations.push_back(0);                               // Invalid signer, but we have to fill arrays for the input
    }

    details->fScriptHashAllFound=true;                              
    details->vInputHashTypes.push_back(SIGHASH_ALL);                         
    details->vInputCanGrantAdminMine.push_back(!details->fCheckCachedScript);            

    return true;
}


bool MultiChainTransaction_CheckInputs(const CTransaction& tx,                  // Tx to check
                                       const CCoinsViewCache &inputs,           // Tx inputs from UTXO database
                                       CMultiChainTxDetails *details,           // Tx details object
                                       string& reason)                          // Error message
        
{
    for (unsigned int i = 0; i < tx.vin.size(); i++)                            
    {                                                                                                                                                                
        const CScript& script2 = tx.vin[i].scriptSig;        
        CScript::const_iterator pc2 = script2.begin();
        if (!script2.IsPushOnly())
        {
            reason="sigScript should be push-only";
            return false;
        }

        const COutPoint &prevout = tx.vin[i].prevout;
        const CCoins *coins = inputs.AccessCoins(prevout.hash);
        assert(coins);

        const CScript& script1 = coins->vout[prevout.n].scriptPubKey;        
        CScript::const_iterator pc1 = script1.begin();

        details->total_value_in+=coins->vout[prevout.n].nValue;
        
        txnouttype typeRet;
        int nRequiredRet;
        vector<CTxDestination> addressRets;
        int op_addr_offset,op_addr_size,is_redeem_script,sighash_type,check_last;

        sighash_type=SIGHASH_NONE;
        if(ExtractDestinations(script1,typeRet,addressRets,nRequiredRet))       // Standard script
        {
            if (typeRet != TX_NULL_DATA)                                        // Regular script
            {
                CKeyID *lpKeyID=boost::get<CKeyID> (&addressRets[0]);
                CScriptID *lpScriptID=boost::get<CScriptID> (&addressRets[0]);
                if( (lpKeyID == NULL) && (lpScriptID == NULL) )
                {
                    reason="Internal error: cannot extract address from input scriptPubKey";
                    return false;
                }
                else
                {
                    if(typeRet != TX_MULTISIG)
                    {
                        if(lpKeyID)
                        {
                            details->vInputDestinations.push_back(*(uint160*)lpKeyID);                               
                        }
                        if(lpScriptID)
                        {
                            details->vInputDestinations.push_back(*(uint160*)lpScriptID);                               
                        }
                    }
                    else
                    {
                        details->vInputDestinations.push_back(0);               // Multisig scripts cannot be used for signing tx objects - issues, stream items, etc.
                    }
                }

                check_last=0;
                if( (typeRet == TX_PUBKEY) || (typeRet == TX_MULTISIG) )
                {
                    check_last=1;
                    details->fRejectIfOpDropOpReturn=true;                      // pay-to-pubkey and bare multisig  scripts cannot be considered "publisher" for the stream, 
                                                                                // because we cannot extract publisher address from the input script itself. 
                                                                                // Though we can still accept this transaction, it is rejected for consistency with principle
                                                                                // "each input which signs the stream item must have permitted writer"
                }

                                                                                // Find sighash_type
                mc_ExtractAddressFromInputScript((unsigned char*)(&pc2[0]),(int)(script2.end()-pc2),&op_addr_offset,&op_addr_size,&is_redeem_script,&sighash_type,check_last);        
                if(sighash_type == SIGHASH_ALL)
                {
                    details->fScriptHashAllFound=true;
                }
                if(sighash_type == SIGHASH_SINGLE)
                {
                    if(i >= tx.vout.size())
                    {
                        reason="SIGHASH_SINGLE input without matching output";
                        return false;                                
                    }
                }                    
            }
            else                                                                // Null-data script
            {
                details->fRejectIfOpDropOpReturn=true;                          // Null data scripts cannot be used in txs with OP_DROP+OP_RETURN
                                                                                // We should not be there at all 
                details->vInputDestinations.push_back(0);       
            }            
            details->vInputScriptTypes.push_back(typeRet);
        }
        else                                                                    // Non-standard script        
        {
            details->fRejectIfOpDropOpReturn=true;                              // Non-standard inputs cannot be used in txs with OP_DROP+OP_RETURN
                                                                                // We cannot be sure where are the signatures in input script
            details->vInputScriptTypes.push_back(TX_NONSTANDARD);
            details->vInputDestinations.push_back(0);                                
        }            

        details->vInputHashTypes.push_back(sighash_type);        

        if(mc_gState->m_Features->PerAssetPermissions())                        // Checking per-asset send permissions
        {
            mc_gState->m_TmpAssetsTmp->Clear();
            if(!mc_ExtractInputAssetQuantities(mc_gState->m_TmpAssetsTmp,script1,prevout.hash,reason))    
            {
                return false;
            }
            if(!mc_VerifyAssetPermissions(mc_gState->m_TmpAssetsTmp,addressRets,1,MC_PTP_SEND,reason))
            {
                return false;                                
            }
        }

                                                                                // Filling input asset quantity list
        if(!mc_ExtractInputAssetQuantities(mc_gState->m_TmpAssetsIn,script1,prevout.hash,reason))   
        {
            return false;
        }                
       
                                                                                // Initialization, input cannot be used as signer for admin/miner grant
        details->vInputCanGrantAdminMine.push_back(!details->fCheckCachedScript);            
    }    

    return true;
}

bool MultiChainTransaction_CheckNewEntity(int vout,
                                          bool& fScriptParsed,
                                          CMultiChainTxDetails *details,   
                                          string& reason)      
{
    int err;
    int entity_update;
    
    mc_gState->m_TmpScript->SetElement(0);
    err=mc_gState->m_TmpScript->GetNewEntityType(&(details->new_entity_type),&entity_update,details->details_script,&(details->details_script_size));
    if(err == 0)    
    {
        fScriptParsed=true;
        if(details->new_entity_output >= 0)
        {
            reason="Metadata script rejected - too many new entities";
            return false;
       }
        if(entity_update)
        {
            reason="Metadata script rejected - entity update script should be preceded by entityref";
            return false;
        }
        if(details->new_entity_type <= mc_gState->m_Assets->MaxEntityType())
        {
            details->new_entity_output=vout;                                    
            details->details_script_type=entity_update;
        }
        else
        {
            reason="Metadata script rejected - unsupported new entity type";
            return false;            
        }
        unsigned char *ptr;
        size_t bytes;        
        mc_gState->m_TmpScript->SetElement(1);
        err=mc_gState->m_TmpScript->GetExtendedDetails(&ptr,&bytes);
        if(err == 0)
        {
            if(bytes)
            {
                if(mc_gState->m_Features->ExtendedEntityDetails())
                {
                    details->extended_script_row=mc_gState->m_Assets->m_ExtendedScripts->GetNumElements();
                    mc_gState->m_Assets->m_ExtendedScripts->AddElement();
                    mc_gState->m_Assets->m_ExtendedScripts->SetData(ptr,bytes);
                }                
            }
        }
    }   
    else
    {
        if(err != MC_ERR_WRONG_SCRIPT)
        {
            reason="Entity details script rejected - error in script";
            return false;            
        }
    }
    
    return true;
}

bool MultiChainTransaction_CheckCachedScript(const CTransaction& tx, 
                                             const CCoinsViewCache &inputs,   
                                             bool& fScriptParsed,
                                             CMultiChainTxDetails *details,   
                                             string& reason)      
{
    int err;
    int cs_offset,cs_new_offset,cs_size,cs_vin;
    unsigned char *cs_script;
    
    mc_gState->m_TmpScript->SetElement(0);
    cs_offset=0;
    while( (err=mc_gState->m_TmpScript->GetCachedScript(cs_offset,&cs_new_offset,&cs_vin,&cs_script,&cs_size)) != MC_ERR_WRONG_SCRIPT )
    {
        fScriptParsed=true;
        if(err != MC_ERR_NOERROR)
        {
            reason="Metadata script rejected - error in cached script";
            return false;
        }
        if(cs_offset)
        {
            if( cs_vin >= (int)tx.vin.size() )
            {
                reason="Metadata script rejected - invalid input in cached script";
                return false;
            }

            const COutPoint &prevout = tx.vin[cs_vin].prevout;
            const CCoins *coins = inputs.AccessCoins(prevout.hash);

            const CScript& script3 = coins->vout[prevout.n].scriptPubKey;        
            CScript::const_iterator pc3 = script3.begin();

            if(cs_size != (int)script3.size())
            {
                reason="Metadata script rejected - cached script mismatch";
                return false;
            }
            if(memcmp(cs_script,(unsigned char*)&pc3[0],cs_size))
            {
                reason="Metadata script rejected - cached script mismatch";
                return false;
            }
            if(details->fCheckCachedScript)
            {
                if(details->vInputHashTypes[cs_vin] == SIGHASH_ALL)
                {
                    details->vInputCanGrantAdminMine[cs_vin]=true;
                }
            }
        }
        cs_offset=cs_new_offset;
    }    
    
    return true;
}

void MultiChainTransaction_FillAdminPermissionsBeforeTx(const CTransaction& tx,
                                                        CMultiChainTxDetails *details)      
{
    if(details->vInputHadAdminPermissionBeforeThisTx.size() == 0)
    {
        for (unsigned int i = 0; i < tx.vin.size(); i++)
        {
            details->vInputHadAdminPermissionBeforeThisTx.push_back(mc_gState->m_Permissions->CanAdmin(NULL,(unsigned char*)&(details->vInputDestinations[i])) != 0);
        }                                
    }
}

bool MultiChainTransaction_VerifyAndDeleteDataFormatElements(string& reason,int64_t *total_size,uint32_t *salt_size)
{    
    if(mc_gState->m_TmpScript->ExtractAndDeleteDataFormat(NULL,NULL,NULL,total_size,salt_size,1))
    {
        reason="Error in data format script";
        return false;                    
    }

    return true;        
}

bool MultiChainTransaction_CheckOpReturnScript(const CTransaction& tx, 
                                               const CCoinsViewCache &inputs,   
                                               int vout,
                                               CMultiChainTxDetails *details,   
                                               string& reason)      
{
    bool fScriptParsed=false;
    uint32_t timestamp,approval;
    vector<CTxDestination> addressRets;
    CTxDestination single_address;
    int64_t total_offchain_size;
    
    total_offchain_size=0;
    if(!MultiChainTransaction_VerifyAndDeleteDataFormatElements(reason,&total_offchain_size,NULL))
    {
        return false;
    }
    details->total_offchain_size+=total_offchain_size;
    
    if(!details->fScriptHashAllFound)             
    {
                                                                                // SIGHASH_NONE or not signed at all
        if( ((int)details->vInputHashTypes.size() <= vout) || (details->vInputHashTypes[vout] != SIGHASH_SINGLE) )
        {
            reason="Output with metadata should be properly signed";
            return false;
        }                        
    }

    if( mc_gState->m_TmpScript->GetNumElements() > 1 )                          // OP_DROP+OP_RETURN script
    {
        if(details->fRejectIfOpDropOpReturn)                                    // We cannot extract address sighash_type properly from the input script 
                                                                                // as we don't know where are the signatures
        {
            reason="Non-standard, P2PK or bare multisig inputs cannot be used in this tx";
            return false;
        }

        if(mc_gState->m_TmpScript->IsDirtyOpReturnScript())
        {
            reason="Non-standard, Only OP_DROP elements are allowed in metadata outputs with OP_DROP";
            return false;
        }
    }
    
    if( mc_gState->m_TmpScript->GetNumElements() == 2 )                         // Cached input script or new entity
    {
        if(!fScriptParsed)
        {
                                                                                // Cached scripts
            if(!MultiChainTransaction_CheckCachedScript(tx, inputs, fScriptParsed, details, reason))
            {
                return false;
            }
        }

        if(!fScriptParsed)
        {
                                                                                // New entities
            if(!MultiChainTransaction_CheckNewEntity(vout, fScriptParsed, details, reason))
            {
                return false;
            }
        }
        
        if(!fScriptParsed)
        {
            reason="Metadata script rejected - Unrecognized script, should be new entity or input script cache";
            return false;
        }        
    }
    
    if( (mc_gState->m_TmpScript->GetNumElements() > 3) && 
        (mc_gState->m_Features->MultipleStreamKeys() == 0) )                    // More than 2 OP_DROPs
    {
        reason="Metadata script rejected - too many elements";
        return false;
    }

    if(mc_gState->m_TmpScript->GetNumElements() == 3 )                          // 2 OP_DROPs + OP_RETURN - possible upgrade approval
                                                                                // Admin permissions before tx should be used 
                                                                                // Performed only if it is indeed needed
    {
        mc_gState->m_TmpScript->SetElement(1);

        if(mc_gState->m_TmpScript->GetApproval(&approval,&timestamp) == 0)
        {
            MultiChainTransaction_FillAdminPermissionsBeforeTx(tx,details);
        }                        
    }
    
    if(mc_gState->m_TmpScript->GetNumElements() >= 3 )                          // This output should be processed later - after all permissions
    {
        details->vOutputScriptFlags[vout] |= MC_MTX_OUTPUT_DETAIL_FLAG_OP_RETURN_ENTITY_ITEM;
    }
    
    details->vOutputPermissionRequired.push_back(0);                            // Stubs for these arrays elements, not used later
    details->vOutputDestinations.push_back(addressRets);
    details->vOutputSingleDestination.push_back(single_address);
    
    return true;
}

bool MultiChainTransaction_CheckAssetUpdateDetails(mc_EntityDetails *entity,
                                                   int vout,
                                                   CMultiChainTxDetails *details,   
                                                   string& reason)      
{
    int err;
    int entity_update;
    
    if(mc_gState->m_TmpScript->GetNumElements() > 3)                            
    {
        reason="Metadata script rejected - too many elements in asset update script";
        return false;
    }
    
    mc_gState->m_TmpScript->SetElement(1);
    
    err=mc_gState->m_TmpScript->GetNewEntityType(&(details->new_entity_type),&entity_update,details->details_script,&(details->details_script_size));
    
    if(err == 0)    
    {
        if(details->details_script_type >= 0)
        {
            reason="Metadata script rejected - too many new entities/entity updates";
            return false;
        }
        
        if(entity_update == 0)
        {
            reason="Metadata script rejected - wrong element, should be entity update";
            return false;
        }
        if(details->new_entity_type != MC_ENT_TYPE_ASSET)
        {
            if((mc_gState->m_Features->Variables() == 0) || (details->new_entity_type != MC_ENT_TYPE_VARIABLE))
            {
                reason="Metadata script rejected - entity type mismatch in update script";
                return false;
            }
        }      
        details->details_script_type=entity_update;
        
        if(mc_gState->m_Features->Variables())
        {
            unsigned char *ptr;
            size_t bytes;        
            mc_gState->m_TmpScript->SetElement(2);
            err=mc_gState->m_TmpScript->GetExtendedDetails(&ptr,&bytes);
            if(err == 0)
            {
                if(bytes)
                {
                    if(mc_gState->m_Features->ExtendedEntityDetails())
                    {
                        details->extended_script_row=mc_gState->m_Assets->m_ExtendedScripts->GetNumElements();
                        mc_gState->m_Assets->m_ExtendedScripts->AddElement();
                        mc_gState->m_Assets->m_ExtendedScripts->SetData(ptr,bytes);
                    }                
                }
            }
        }        
    }          
    else
    {
        reason="Metadata script rejected - wrong element, should be entity update";
        return false;
    }
                                                                                // The script itself will be processed later
    details->vOutputScriptFlags[vout] |= MC_MTX_OUTPUT_DETAIL_FLAG_FOLLOWON_DETAILS;
    return true;
}

bool MultiChainTransaction_CheckUpgradeApproval(const CTransaction& tx,
                                                mc_EntityDetails *entity,
                                                int offset,  
                                                int vout,
                                                CMultiChainTxDetails *details,   
                                                string& reason)      
{
    uint32_t timestamp,approval;
    uint256 upgrade_hash;
    bool fAdminFound;
    
    if(mc_gState->m_TmpScript->GetNumElements() > 3)                            
    {
        reason="Metadata script rejected - too many elements in upgrade approval script";
        return false;
    }
    
    mc_gState->m_TmpScript->SetElement(1);          

    if(mc_gState->m_TmpScript->GetApproval(&approval,&timestamp))
    {
        reason="Metadata script rejected - wrong element, should be upgrade approval";
        return false;
    }
    
    upgrade_hash=*(uint256*)(entity->GetTxID());
    if(approval)
    {
        LogPrintf("Found approval script in tx %s for %s\n",
                tx.GetHash().GetHex().c_str(),
                upgrade_hash.ToString().c_str());
    }
    else
    {
        LogPrintf("Found disapproval script in tx %s for %s\n",
                tx.GetHash().GetHex().c_str(),
                upgrade_hash.ToString().c_str());                                    
    }

    fAdminFound=false;
    for (unsigned int i = 0; i < tx.vin.size(); i++)
    {
        if(details->vInputHadAdminPermissionBeforeThisTx[i])
        {
            if(details->IsRelevantInput(i,vout))
            {
                if(mc_gState->m_Permissions->SetApproval(entity->GetTxID()+MC_AST_SHORT_TXID_OFFSET,approval,
                                                         (unsigned char*)&(details->vInputDestinations[i]),entity->UpgradeStartBlock(),timestamp,MC_PFL_NONE,1,offset) == 0)
                {
                    fAdminFound=true;
                }                                                                                    
            }                                        
        }
    }   
    
    if(!fAdminFound)
    {
        reason="Inputs don't belong to valid admin for approval script";
        return false;
    }                                

    return true;
}

bool MultiChainTransaction_CheckStreamItem(mc_EntityDetails *entity,
                                           int vout,
                                           CMultiChainTxDetails *details,   
                                           string& reason)      
{
    bool fAllValidPublishers;
    unsigned char item_key[MC_ENT_MAX_ITEM_KEY_SIZE];
    int item_key_size;
    
    if(mc_gState->m_Features->OffChainData())
    {
        if(mc_gState->m_TmpScript->m_Restrictions & entity->m_Restrictions)
        {
            reason="Metadata script rejected - stream restrictions violation";
            return false;        
        }
    }
                                                                                // Multiple keys, if not allowed, check for count is made in different place
    for (int e = 1; e < mc_gState->m_TmpScript->GetNumElements()-1; e++)
    {
        mc_gState->m_TmpScript->SetElement(e);
                                                                                
        if(mc_gState->m_TmpScript->GetItemKey(item_key,&item_key_size))         
        {
            reason="Metadata script rejected - wrong element, should be item key";
            return false;
        }                                            
    }
    
    fAllValidPublishers=true;
    if(entity->AnyoneCanWrite() == 0)
    {
        for (unsigned int i = 0; i < details->vInputDestinations.size(); i++)
        {
            if(details->IsRelevantInput(i,vout))
            {
                if(fAllValidPublishers)
                {
                    if(mc_gState->m_Permissions->CanWrite(entity->GetTxID(),(unsigned char*)&(details->vInputDestinations[i])) == 0)
                    {
                        fAllValidPublishers=false;
                    }
                }
            }
        }                                
    }
    
    if(!fAllValidPublishers)
    {
        reason="Metadata script rejected - Inputs don't belong to valid publisher";
        return false;
    }                    
    
    return true;
}

bool MultiChainTransaction_CheckEntityItem(const CTransaction& tx,
                                           int offset,  
                                           int vout,
                                           CMultiChainTxDetails *details,   
                                           string& reason)      
{
    unsigned char short_txid[MC_AST_SHORT_TXID_SIZE];
    mc_EntityDetails entity;
    uint32_t salt_size=0;
    
    if(!MultiChainTransaction_VerifyAndDeleteDataFormatElements(reason,NULL,&salt_size))
    {
        return false;
    }
    
    if(salt_size)
    {
        if(salt_size > MAX_CHUNK_SALT_SIZE)
        {
            reason="Metadata script rejected - salt size too large";
            return false;            
        }
        if(salt_size < MIN_CHUNK_SALT_SIZE)
        {
            reason="Metadata script rejected - salt size too small";
            return false;            
        }
    }
    
    mc_gState->m_TmpScript->SetElement(0);
                                                                                // Should be spke
    if(mc_gState->m_TmpScript->GetEntity(short_txid))                           // Entity element
    {
        reason="Metadata script rejected - wrong element, should be entityref";
        return false;
    }
    
    if(mc_gState->m_Assets->FindEntityByShortTxID(&entity,short_txid) == 0)
    {
        reason="Metadata script rejected - entity not found";
        return false;
    }               
    
    details->SetRelevantEntity(short_txid);
    
    if((entity.GetEntityType() == MC_ENT_TYPE_ASSET) || 
       (entity.GetEntityType() == MC_ENT_TYPE_VARIABLE))                        // Asset or variable update
    {
        if(!MultiChainTransaction_CheckAssetUpdateDetails(&entity,vout,details,reason))
        {
            return false;            
        }
    }
    else
    {
        if(entity.GetEntityType() == MC_ENT_TYPE_UPGRADE)                       // Upgrade approval
        {
            if(!MultiChainTransaction_CheckUpgradeApproval(tx,&entity,offset,vout,details,reason))
            {
                return false;            
            }
        }
        else                                                                    // (Pseudo)stream item
        {
            if(entity.GetEntityType() <= MC_ENT_TYPE_STREAM_MAX)
            {
                if(mc_gState->m_TmpScript->m_Restrictions & MC_ENT_ENTITY_RESTRICTION_OFFCHAIN)
                {
                    if(mc_gState->m_Features->SaltedChunks())
                    {
                        if(entity.Restrictions() & MC_ENT_ENTITY_RESTRICTION_NEED_SALTED)
                        {
                            if(salt_size == 0)
                            {
                                reason="Metadata script rejected - unsalted offchain items in restricted stream";                                            
                                return false;
                            }
                        }
                    }
                }
                if(!MultiChainTransaction_CheckStreamItem(&entity,vout,details,reason))
                {
                    return false;            
                }            
            }
            else
            {
                reason="Metadata script rejected - too many elements for this entity type";                
                if(mc_gState->m_Features->FixedIn20010())
                {
                    return false;
                }
            }
        }
    }
    
    return true;
}

bool MultiChainTransaction_CheckDestinations(const CScript& script1,
                                             int vout,
                                             CMultiChainTxDetails *details,   
                                             string& reason)      
{
    bool fNoDestinationInOutput;
    bool not_cleared;
    
    txnouttype typeRet;
    int nRequiredRet,receive_required;    
    vector<CTxDestination> addressRets;
    CTxDestination single_destination;

    not_cleared=false;
    fNoDestinationInOutput=false;

    if(mc_gState->m_Features->FixedDestinationExtraction())
    {
        fNoDestinationInOutput=!ExtractDestinations(script1,typeRet,addressRets,nRequiredRet);
    }
    else                                                                        // Bug in 10008, permission was set to address in non-standard output
    {
        fNoDestinationInOutput=!ExtractDestinations10008(script1,typeRet,addressRets,nRequiredRet,true,&not_cleared);
    }
    if(fNoDestinationInOutput)
    {
        details->vOutputScriptFlags[vout] |= MC_MTX_OUTPUT_DETAIL_FLAG_NO_DESTINATION;
    }      

    if(fNoDestinationInOutput && 
      ( (MCP_ANYONE_CAN_RECEIVE == 0) || (MCP_ALLOW_ARBITRARY_OUTPUTS == 0) ) )
    {
        reason="Script rejected - destination required ";
        return false;
    }
    
    if((MCP_ALLOW_ARBITRARY_OUTPUTS == 0) || (mc_gState->m_Features->FixedDestinationExtraction() == 0) )
    {
        if((typeRet == TX_MULTISIG) && (MCP_ALLOW_MULTISIG_OUTPUTS == 0))
        {
            reason="Script rejected - multisig is not allowed";
            return false;
        }

        if((typeRet == TX_SCRIPTHASH) && (MCP_ALLOW_P2SH_OUTPUTS == 0))
        {
            reason="Script rejected - P2SH is not allowed";
            return false;
        }
    }
    
    receive_required=addressRets.size();
    if(typeRet == TX_MULTISIG)
    {
        receive_required-=nRequiredRet;
        receive_required+=1;
        if(receive_required>(int)addressRets.size())
        {
            receive_required=addressRets.size();
        }
    }

    if(addressRets.size())
    {
        single_destination=addressRets[0];
    }
    if(not_cleared)
    {
        receive_required=0;
        addressRets.clear();
    }
    details->vOutputPermissionRequired.push_back(receive_required);
    details->vOutputDestinations.push_back(addressRets);
    details->vOutputSingleDestination.push_back(single_destination);
    
    return true;
}

bool MultiChainTransaction_ProcessPermissions(const CTransaction& tx,
                                              int offset,  
                                              int vout,
                                              uint32_t permission_type,
                                              bool fFirstPass,
                                              CMultiChainTxDetails *details,   
                                              string& reason)      
{
    bool fIsPurePermission;
    bool fNoDestinationInOutput;    
    unsigned char short_txid[MC_AST_SHORT_TXID_SIZE];
    mc_EntityDetails entity;
    uint32_t type,from,to,timestamp,flags;
    
    fIsPurePermission=false;
    if(mc_gState->m_TmpScript->GetNumElements())
    {
        fIsPurePermission=true;
    }
                
    fNoDestinationInOutput=( (details->vOutputScriptFlags[vout] & MC_MTX_OUTPUT_DETAIL_FLAG_NO_DESTINATION) != 0);
    
    entity.Zero();                                                  
    for (int e = 0; e < mc_gState->m_TmpScript->GetNumElements(); e++)
    {
        mc_gState->m_TmpScript->SetElement(e);
        if(mc_gState->m_TmpScript->GetEntity(short_txid) == 0)                  // Entity element
        {
            if(entity.GetEntityType())
            {
                reason="Script rejected - duplicate entity script";
                return false;
            }
            if(mc_gState->m_Assets->FindEntityByShortTxID(&entity,short_txid) == 0)
            {
                reason="Script rejected - entity not found";
                return false;
            }        
            
            details->emergency_disapproval_output=-2;                           // Only global filter disapprovals can bypass filters
            details->SetRelevantEntity(short_txid);            
        }
        else                                                                    // Not entity element
        {   
            if(mc_gState->m_TmpScript->GetPermission(&type,&from,&to,&timestamp) == 0) // Grant script
            {
                if(fNoDestinationInOutput)
                {
                    reason="Script rejected - wrong destination type in output with permission script";
                    return false;
                }
                
                if(fFirstPass)  
                {
                    if( type & ( MC_PTP_CREATE | MC_PTP_ISSUE | MC_PTP_ACTIVATE | MC_PTP_FILTER ) )
                    {
                        details->vOutputScriptFlags[vout] |= MC_MTX_OUTPUT_DETAIL_FLAG_PERMISSION_CREATE;
                    }                    
                    if(type & mc_gState->m_Permissions->GetCustomHighPermissionTypes())
                    {
                        details->vOutputScriptFlags[vout] |= MC_MTX_OUTPUT_DETAIL_FLAG_PERMISSION_CREATE;                        
                    }
                    if( type & MC_PTP_FILTER )
                    {
                        details->vOutputScriptFlags[vout] |= MC_MTX_OUTPUT_DETAIL_FLAG_PERMISSION_FILTER;
                        if( (type == MC_PTP_FILTER) && (details->emergency_disapproval_output == -1) )
                        {
                            details->emergency_disapproval_output=vout;
                        }
                        else
                        {
                            details->emergency_disapproval_output=-2;           // non-standard emergency disapproval
                        }
                    }
                    if( type != MC_PTP_FILTER)
                    {
                        details->vOutputScriptFlags[vout] |= MC_MTX_OUTPUT_DETAIL_FLAG_PERMISSION_NOT_FILTER;
                    }
                    if( type & ( MC_PTP_MINE | MC_PTP_ADMIN ) )
                    {
                        details->fAdminMinerGrant=true;
                        details->vOutputScriptFlags[vout] |= MC_MTX_OUTPUT_DETAIL_FLAG_PERMISSION_ADMIN;
                    }                    
                }
                
                type &= permission_type;                                        // Processing only relevant permissions
                
                                                                                // Admins and activators are those having permissions before thix tx
                for (unsigned int i = 0; i < details->vInputDestinations.size(); i++)
                {
                    if(details->IsRelevantInput(i,vout))
                    {
                        if(mc_gState->m_Permissions->CanAdmin(entity.GetTxID(),(unsigned char*)&(details->vInputDestinations[i])))
                        {
                            details->vAllowedAdmins.insert(strprintf("%d-%d-%d",i,vout,e));
                        }                                        
                        if(mc_gState->m_Permissions->CanActivate(entity.GetTxID(),(unsigned char*)&(details->vInputDestinations[i])))
                        {
                            details->vAllowedActivators.insert(strprintf("%d-%d-%d",i,vout,e));
                        }
                    }
                }
                            
                if(type)
                {
                    CKeyID *lpKeyID=boost::get<CKeyID> (&(details->vOutputSingleDestination[vout]));
                    CScriptID *lpScriptID=boost::get<CScriptID> (&(details->vOutputSingleDestination[vout]));
                    
                                                                                // Permissions cannot be granted to nonstandard outputs or bare multisigs
                    if(((lpKeyID == NULL) && (lpScriptID == NULL)) || (details->vOutputDestinations[vout].size() > 1))
                    {
                        reason="Permission script rejected - wrong destination type";
                        return false;
                    }
                    
                    CBitcoinAddress address;
                    unsigned char* ptr=NULL;
                    flags=MC_PFL_NONE;
                    if(lpKeyID != NULL)
                    {
                        address=CBitcoinAddress(*lpKeyID);
                        ptr=(unsigned char*)(lpKeyID);
                        if(type & MC_PTP_CONNECT)                   
                        {
                            if(mc_gState->m_pSeedNode)
                            {
                                CNode* seed_node;
                                seed_node=(CNode*)(mc_gState->m_pSeedNode);

                                                                                // If connect permission of seed node was involved, we may want to disconnect from it
                                if(memcmp(ptr,seed_node->kAddrRemote.begin(),20) == 0)
                                {
                                    details->fSeedNodeInvolved=true;
                                }
                            }
                        }
                    }
                    else
                    {
                        flags=MC_PFL_IS_SCRIPTHASH;
                        address=CBitcoinAddress(*lpScriptID);
                        ptr=(unsigned char*)(lpScriptID);
                    }
                                
                    if(fDebug)LogPrint("mchn","Found permission script in tx %s for %s - (%08x: %d - %d)\n",
                            tx.GetHash().GetHex().c_str(),
                            address.ToString().c_str(),
                            type, from, to);

                    bool fAdminFound=false;
                    bool fAdminFoundWithoutCachedScript=false;
                    bool fActivateIsEnough=mc_gState->m_Permissions->IsActivateEnough(type);

                    
                    if(details->emergency_disapproval_output == vout)
                    {
                        if(mc_gState->m_Permissions->FilterApproved(NULL,ptr) == 0) // Already disapproved
                        {
                            details->emergency_disapproval_output=-2;
                        }
                        if(mc_gState->m_Features->FixedIn20005())               // Not standard disapproval
                        {
                            if( (to != 0) || (from != 0) )
                            {
                                details->emergency_disapproval_output=-2;                            
                            }
                        }
                    }
                    
                    for (unsigned int i = 0; i < tx.vin.size(); i++)
                    {
                        if( ( !fActivateIsEnough && (details->vAllowedAdmins.count(strprintf("%d-%d-%d",i,vout,e)) > 0)) ||
                            (  fActivateIsEnough && (details->vAllowedActivators.count(strprintf("%d-%d-%d",i,vout,e)) > 0)) )    
                        {                 
                            if(details->vInputDestinations[i] != 0)
                            {
                                if( ( (type & (MC_PTP_ADMIN | MC_PTP_MINE)) == 0) || details->vInputCanGrantAdminMine[i] || (entity.GetEntityType() > 0) )
                                {
                                    details->fFullReplayCheckRequired=true;
                                    if(mc_gState->m_Permissions->SetPermission(entity.GetTxID(),ptr,type,(unsigned char*)&(details->vInputDestinations[i]),
                                            from,to,timestamp,flags,1,offset) == 0)
                                    {
                                        fAdminFound=true;
                                    }
                                }
                                else
                                {
                                    fAdminFoundWithoutCachedScript=true;                                                
                                }
                            }
                        }
                    }
                    
                    if(!fAdminFound)
                    {
                        reason="Inputs don't belong to valid admin";
                        if(fAdminFoundWithoutCachedScript)
                        {
                            reason="Inputs require scriptPubKey cache to support miner precheck";
                        }
                        return false;
                    }
                }      
                entity.Zero();                                                  // Entity element in the non-op-return output should be followed by permission element
                                                                                // So only permission can reset it
            }
            else                                                                // Not permission script
            {
                if(entity.GetEntityType())                              
                {
                    reason="Script rejected - entity script should be followed by permission";
                    return false;
                }
                fIsPurePermission=false;
            }
        }
    }                                                               
                
    if(entity.GetEntityType())
    {
        reason="Script rejected - incomplete entity script";
        return false;
    }
                
    if(fFirstPass)
    {
        if(tx.vout[vout].nValue > 0)
        {
            fIsPurePermission=false;
        }

        if(!fIsPurePermission)
        {
            details->vOutputScriptFlags[vout] |= MC_MTX_OUTPUT_DETAIL_FLAG_NOT_PURE_PERMISSION;        
        }
        details->vOutputScriptFlags[vout] |= MC_MTX_OUTPUT_DETAIL_FLAG_NOT_OP_RETURN;        
    }
    
    return true;
}

bool Is_MultiChainLicenseTokenTransfer(const CTransaction& tx)
{
    mc_EntityDetails entity;
    if(tx.IsCoinBase())
    {
        return false;
    }
    if(tx.vout.size() != 1)
    {
        return false;                                                            
    }
    if(tx.vin.size() != 1)
    {
        return false;                                                            
    }
    if(mc_gState->m_Features->LicenseTokens() == 0)
    {
        return false;
    }
        
    MultiChainTransaction_SetTmpOutputScript(tx.vout[0].scriptPubKey);
    if(mc_gState->m_TmpScript->GetNumElements() == 0)
    {
        return false;        
    }
        
    mc_gState->m_TmpAssetsOut->Clear();
    mc_gState->m_TmpScript->SetElement(0);
    if(mc_gState->m_TmpScript->GetAssetQuantities(mc_gState->m_TmpAssetsOut,MC_SCR_ASSET_SCRIPT_TYPE_TRANSFER) != MC_ERR_NOERROR)
    {
        return false;
    }
    
    if(mc_gState->m_Assets->FindEntityByFullRef(&entity,mc_gState->m_TmpAssetsOut->GetRow(0)))
    {
        if(entity.GetEntityType() == MC_ENT_TYPE_LICENSE_TOKEN)
        {
            return true;
        }
    }
        
    return false;
}

bool MultiChainTransaction_CheckLicenseTokenTransfer(const CTransaction& tx,
                                               int unchecked_row, 
                                               CMultiChainTxDetails *details,   
                                               string& reason)      
{
    bool token_transfer=false;
    mc_EntityDetails entity;
    
    if(mc_gState->m_Features->LicenseTokens() == 0)
    {
        return true;
    }
    
    for(int i=unchecked_row;i<mc_gState->m_TmpAssetsOut->GetCount();i++)
    {
        if(mc_gState->m_Assets->FindEntityByFullRef(&entity,mc_gState->m_TmpAssetsOut->GetRow(i)))
        {
            if(entity.GetEntityType() == MC_ENT_TYPE_LICENSE_TOKEN)
            {
                token_transfer=true;
            }
        }
    }
    
    if(!token_transfer)
    {
        return true;
    }
    
    if(tx.vout.size() != 1)
    {
        reason="License token transfer tx should have one output";
        return false;                                                            
    }

    if(tx.vin.size() != 1)
    {
        reason="License token transfer tx should have one input";
        return false;                                                            
    }        
    
    if(mc_gState->m_Features->License20010())
    {
        if(mc_gState->m_TmpScript->GetNumElements() < 2)
        {
            reason="License token transfer script rejected - wrong number of elements";
            return false;                                                                                                                                                                                
        }        
    }
    else
    {
        if(mc_gState->m_TmpScript->GetNumElements() != 3)
        {
            reason="License token transfer script rejected - wrong number of elements";
            return false;                                                                                                                                                                                
        }
    }
    
    if(details->vInputHashTypes[0] != SIGHASH_ALL)
    {
        reason="License token transfer script rejected - wrong signature type";
        return false;                                                                                                                                                                                        
    }
    
    if(mc_gState->m_TmpAssetsOut->GetCount() != 1)
    {
        reason="License token transfer script rejected - wrong script";
        return false;                                                                                                                                                                                                        
    }
        
    if(mc_GetABQuantity(mc_gState->m_TmpAssetsOut->GetRow(0)) != 1)            
    {
        reason="License token transfer script rejected - wrong number of license token units";
        return false;                                                                                                                                                                                                        
    }

    details->vOutputScriptFlags[0] |= MC_MTX_OUTPUT_DETAIL_FLAG_LICENSE_TRANSFER;
    details->fLicenseTokenTransfer=true;
    return true;
}


bool MultiChainTransaction_CheckAssetTransfers(const CTransaction& tx,
                                               int offset,  
                                               int vout,
                                               CMultiChainTxDetails *details,   
                                               string& reason)      
{
    int receive_required=details->vOutputPermissionRequired[vout];
    
    if(mc_gState->m_Features->PerAssetPermissions())                            // Checking per-asset receive permissions
    {
        mc_gState->m_TmpAssetsTmp->Clear();
        if(!mc_ExtractOutputAssetQuantities(mc_gState->m_TmpAssetsTmp,reason,true))   
        {
            return false;
        }
        if(!mc_VerifyAssetPermissions(mc_gState->m_TmpAssetsTmp,details->vOutputDestinations[vout],receive_required,MC_PTP_RECEIVE,reason))
        {
            return false;                                
        }
    }
    
    int unchecked_row=mc_gState->m_TmpAssetsOut->GetCount();
    if(!mc_ExtractOutputAssetQuantities(mc_gState->m_TmpAssetsOut,reason,false))// Filling output asset quantity list
    {
        return false;                                
    }    
    
    if(!MultiChainTransaction_CheckLicenseTokenTransfer(tx,unchecked_row,details,reason))
    {
        return false;
    }

    
                                                                                // Check for dust and receive permissions
                                                                                // Not required for pure grants
    if( ( (details->vOutputScriptFlags[vout] & MC_MTX_OUTPUT_DETAIL_FLAG_NOT_PURE_PERMISSION) != 0 ) && 
        ( (details->vOutputScriptFlags[vout] & MC_MTX_OUTPUT_DETAIL_FLAG_LICENSE_TRANSFER) == 0 ) )
    {
        if((offset < 0) && Params().RequireStandard())                          // If not in block - part of IsStandard check
        {
            if (tx.vout[vout].IsDust(::minRelayTxFee))             
            {
                if(!tx.IsCoinBase())
                {
                    reason="Transaction amount too small";
                    return false;                                
                }                            
            }
        }
        for(int a=0;a<(int)details->vOutputDestinations[vout].size();a++)
        {                            
            CKeyID *lpKeyID=boost::get<CKeyID> (&(details->vOutputDestinations[vout][a]));
            CScriptID *lpScriptID=boost::get<CScriptID> (&(details->vOutputDestinations[vout][a]));
            if((lpKeyID == NULL) && (lpScriptID == NULL))
            {
                reason="Script rejected - wrong destination type";
                return false;                                
            }
            unsigned char* ptr=NULL;
            if(lpKeyID != NULL)
            {
                ptr=(unsigned char*)(lpKeyID);
            }
            else
            {
                ptr=(unsigned char*)(lpScriptID);
            }

            bool fCanReceive=mc_gState->m_Permissions->CanReceive(NULL,ptr);
            
                                                                                // Miner can send funds to himself in coinbase even without receive permission
                                                                                // It is relevant only for native currency as other assets will be unbalanced
            if(tx.IsCoinBase())                                                 
            {
                fCanReceive |= mc_gState->m_Permissions->CanMine(NULL,ptr);
            }
            if(fCanReceive)                        
            {
                receive_required--;
            }                                    
        }
        if(receive_required>0)
        {
            if( (tx.vout[vout].nValue > 0) || 
                (mc_gState->m_TmpScript->GetNumElements() > 0) ||
                (mc_gState->m_Features->AnyoneCanReceiveEmpty() == 0) )
            {
                reason="One of the outputs doesn't have receive permission";
                return false;                                
            }
        }
    }

    return true;
}

bool MultiChainTransaction_CheckOutputs(const CTransaction& tx,                 // Tx to check
                                        const CCoinsViewCache &inputs,          // Tx inputs from UTXO database
                                        int offset,                             // Tx offset in block, -1 if in memppol
                                        CMultiChainTxDetails *details,          // Tx details object
                                        string& reason)                         // Error message
{
    uint32_t permission_type;
    for (unsigned int vout = 0; vout < tx.vout.size(); vout++)
    {
        details->vOutputScriptFlags.push_back(MC_MTX_OUTPUT_DETAIL_FLAG_NONE);
    }
    
    for (unsigned int vout = 0; vout < tx.vout.size(); vout++)                  // Basic checks, destinations and simple grants
    {
        details->total_value_out+=tx.vout[vout].nValue;
        
        MultiChainTransaction_SetTmpOutputScript(tx.vout[vout].scriptPubKey);

        if(mc_gState->m_TmpScript->IsOpReturnScript())                    
        {
            if(!MultiChainTransaction_CheckOpReturnScript(tx,inputs,vout,details,reason))
            {
                return false;
            }
        }
        else
        {
            if(!MultiChainTransaction_CheckDestinations(tx.vout[vout].scriptPubKey,vout,details,reason))
            {
                return false;
            }            
            
            permission_type=MC_PTP_CONNECT | MC_PTP_SEND | MC_PTP_RECEIVE | MC_PTP_WRITE;
            permission_type |= mc_gState->m_Permissions->GetCustomLowPermissionTypes();
            if(mc_gState->m_Features->ReadPermissions())
            {
                permission_type |= MC_PTP_READ;                
            }
            if(!MultiChainTransaction_ProcessPermissions(tx,offset,vout,permission_type,true,details,reason))
            {
                return false;
            }            
        }
    }
    
    for (unsigned int vout = 0; vout < tx.vout.size(); vout++)                  // create, issue, activate grants (requiring pre-tx permissions)
    {
        if(details->vOutputScriptFlags[vout] & (MC_MTX_OUTPUT_DETAIL_FLAG_PERMISSION_CREATE | MC_MTX_OUTPUT_DETAIL_FLAG_PERMISSION_FILTER) )
        {
            MultiChainTransaction_SetTmpOutputScript(tx.vout[vout].scriptPubKey);
            
            permission_type=MC_PTP_CREATE | MC_PTP_ISSUE | MC_PTP_ACTIVATE | MC_PTP_FILTER;
            permission_type |= mc_gState->m_Permissions->GetCustomHighPermissionTypes();
            if(!MultiChainTransaction_ProcessPermissions(tx,offset,vout,permission_type,false,details,reason))
            {
                return false;
            }                        
        }
    }    

    for (unsigned int vout = 0; vout < tx.vout.size(); vout++)                  // mine, admin grants (requiring cached script) 
    {
        if(details->vOutputScriptFlags[vout] & MC_MTX_OUTPUT_DETAIL_FLAG_PERMISSION_ADMIN)
        {
            MultiChainTransaction_SetTmpOutputScript(tx.vout[vout].scriptPubKey);
            
            permission_type=MC_PTP_MINE | MC_PTP_ADMIN;
            if(!MultiChainTransaction_ProcessPermissions(tx,offset,vout,permission_type,false,details,reason))
            {
                return false;
            }            
        }
    }    

    for (unsigned int vout = 0; vout < tx.vout.size(); vout++)
    {
                                                                                // Entity items (stream items, upgrade approvals, updates)
        if(details->vOutputScriptFlags[vout] & MC_MTX_OUTPUT_DETAIL_FLAG_OP_RETURN_ENTITY_ITEM)
        {
            MultiChainTransaction_SetTmpOutputScript(tx.vout[vout].scriptPubKey);
            
            if(!MultiChainTransaction_CheckEntityItem(tx,offset,vout,details,reason))
            {
                return false;                
            }
        }                                                                                        // Assets quantities and permission checks
    }    

    return true;
}


bool MultiChainTransaction_CheckTransfers(const CTransaction& tx,               // Tx to check
                                        int offset,                             // Tx offset in block, -1 if in memppol
                                        CMultiChainTxDetails *details,          // Tx details object
                                        string& reason)                         // Error message
{
    mc_gState->m_TmpAssetsOut->Clear();
    
    for (unsigned int vout = 0; vout < tx.vout.size(); vout++)
    {
                                                                                // Assets quantities and permission checks
        if(details->vOutputScriptFlags[vout] & MC_MTX_OUTPUT_DETAIL_FLAG_NOT_OP_RETURN)
        {
            MultiChainTransaction_SetTmpOutputScript(tx.vout[vout].scriptPubKey);

            if(!MultiChainTransaction_CheckAssetTransfers(tx,offset,vout,details,reason))
            {
                return false;                
            }
        }        
    }    
    
    if(!mc_CompareAssetQuantities(details,reason))                              // Comparing input/output asset quantities
    {
        return false;                
    }
    
    return true;
}

int64_t MultiChainTransaction_OffchainFee(int64_t total_offchain_size)            // Total size of offchain items
{
    return (MIN_OFFCHAIN_FEE*total_offchain_size + 999)/ 1000;
}

bool MultiChainTransaction_CheckMandatoryFee(CMultiChainTxDetails *details,     // Tx details object
                                             int64_t *mandatory_fee,            // Mandatory Fee    
                                             string& reason)                    // Error message
{
    *mandatory_fee = MultiChainTransaction_OffchainFee(details->total_offchain_size);

    if(*mandatory_fee)
    {
        if(details->total_value_in-details->total_value_out < *mandatory_fee)
        {
            reason="Insufficient mandatory fee";
            return false;
        }
    }
    
    return true;
}

bool MultiChainTransaction_CheckLicenseTokenDetails(CMultiChainTxDetails *details,     // Tx details object
                                             void *token_address,               // Address the token is issued to
                                             string& reason)                    // Error message
{
    uint32_t offset,next_offset,param_value_start;
    unsigned int timestamp;
    size_t param_value_size;        
    size_t value_sizes[256];
    int value_starts[256];
    unsigned char code;
    
    memset(value_sizes,0,256*sizeof(size_t));
    memset(value_starts,0,256*sizeof(int));
    
    offset=0;
            
    while((int)offset<details->details_script_size)
    {
        next_offset=mc_GetParamFromDetailsScript(details->details_script,details->details_script_size,offset,&param_value_start,&param_value_size);
        if(param_value_start > 0)
        {
            if(details->details_script[offset] == 0)
            {                
                code=details->details_script[offset+1];
                if(value_starts[code])
                {
                    reason="License token issue script rejected - multiple values for the same code in details script";
                    return false;                                                                                                                                
                }
                value_sizes[code]=param_value_size;
                value_starts[code]=param_value_start;
            }
        }
        offset=next_offset;
    }
    
    if( (value_starts[MC_ENT_SPRM_NAME] == 0) || (value_sizes[MC_ENT_SPRM_NAME] == 0) )
    {
        reason="License token issue script rejected - no name";
        return false;                                                                                                                                        
    }
    if( value_sizes[MC_ENT_SPRM_LICENSE_LICENSE_HASH] == 0 )
    {
        reason="License token issue script rejected - invalid request hash";
        return false;                                                                                                                                        
    }
    if( value_sizes[MC_ENT_SPRM_LICENSE_ISSUE_ADDRESS] != sizeof(uint160) )
    {
        reason="License token issue script rejected - invalid request address";
        return false;                                                                                                                                        
    }
    if(memcmp(token_address,details->details_script+value_starts[MC_ENT_SPRM_LICENSE_ISSUE_ADDRESS],value_sizes[MC_ENT_SPRM_LICENSE_ISSUE_ADDRESS]))
    {
        reason="License token issue script rejected - request address mismatch";
        return false;                                                                                                                                                
    }
    if( value_sizes[MC_ENT_SPRM_LICENSE_CONFIRMATION_TIME] < 4 )
    {
        reason="License token issue script rejected - invalid confirmation time";
        return false;                                                                                                                                        
    }
    if( value_sizes[MC_ENT_SPRM_LICENSE_CONFIRMATION_REF] == 0 )
    {
        reason="License token issue script rejected - invalid confirmation reference";
        return false;                                                                                                                                        
    }
    if( value_sizes[MC_ENT_SPRM_LICENSE_PUBKEY] == 0 )
    {
        reason="License token issue script rejected - invalid pubkey";
        return false;                                                                                                                                        
    }
    if( (value_sizes[MC_ENT_SPRM_LICENSE_MIN_NODE] < 4 ) || 
        (value_sizes[MC_ENT_SPRM_LICENSE_MIN_NODE] > 8 ))
    {
        reason="License token issue script rejected - invalid version";
        return false;                                                                                                                                        
    }
    if( (value_sizes[MC_ENT_SPRM_LICENSE_MIN_PROTOCOL] < 4 ) || 
        (value_sizes[MC_ENT_SPRM_LICENSE_MIN_PROTOCOL] > 8 ))
    {
        reason="License token issue script rejected - invalid protocol";
        return false;                                                                                                                                        
    }
    if( mc_gState->m_NetworkParams->ProtocolVersion() < mc_GetLE(details->details_script+value_starts[MC_ENT_SPRM_LICENSE_MIN_PROTOCOL],value_sizes[MC_ENT_SPRM_LICENSE_MIN_PROTOCOL]) )
    {
        reason="License token issue script rejected - Not supported in this protocol version";
        return false;                                                                                                                                        
    }
    if( value_sizes[MC_ENT_SPRM_LICENSE_SIGNATURE] == 0 )
    {
        reason="License token issue script rejected - invalid signature";
        return false;                                                                                                                                        
    }
    if( (value_sizes[MC_ENT_SPRM_TIMESTAMP] != 4 ))
    {
        reason="License token issue script rejected - invalid timestamp";
        return false;                                                                                                                                        
    }
    
    timestamp=(unsigned int)mc_GetLE(details->details_script+value_starts[MC_ENT_SPRM_TIMESTAMP],value_sizes[MC_ENT_SPRM_TIMESTAMP]);

    if(timestamp < chainActive.Tip()->nTime-30*86400)
    {
        reason="License token issue script rejected - timestamp is too far in the past";
        return false;                                                                                                                                                
    }
    if(timestamp > chainActive.Tip()->nTime+30*86400)
    {
        reason="License token issue script rejected - timestamp is too far in the future";
        return false;                                                                                                                                                
    }    
    
    if(mc_gState->m_Features->License20010())
    {
        CLicenseRequest confirmation;
        confirmation.SetData(details->details_script,details->details_script_size);
        string license_name=confirmation.GetLicenseName();
        if( (value_sizes[MC_ENT_SPRM_NAME] != license_name.size()) ||
            (memcmp(details->details_script+value_starts[MC_ENT_SPRM_NAME],license_name.c_str(),value_sizes[MC_ENT_SPRM_NAME]) != 0))    
        {
            reason="License token issue script rejected - name mismatch";
            return false;                                                                                                                                                            
        }
    }
    
    return true;
}

bool MultiChainTransaction_ProcessAssetIssuance(const CTransaction& tx,         // Tx to check
                                                int offset,                     // Tx offset in block, -1 if in memppol
                                                bool accept,                    // Accept to mempools if successful
                                                CMultiChainTxDetails *details,  // Tx details object
                                                string& reason)                 // Error message
{
    int update_mempool;
    
    
    mc_EntityDetails entity;
    mc_EntityDetails this_entity;
    char asset_name[MC_ENT_MAX_NAME_SIZE+1];
    int multiple,out_count,issue_vout;
    int err;
    int64_t quantity,total,last_total,left_position;
    int32_t chain_size;
    uint256 txid;
    bool new_issue,follow_on,issue_in_output;
    unsigned char *ptrOut;
    vector <uint160> issuers;
    vector <uint32_t> issuer_flags;
    uint32_t flags;
    uint32_t value_offset;
    size_t value_size;
    unsigned char short_txid[MC_AST_SHORT_TXID_SIZE];
    CTxDestination addressRet;
    unsigned char token_address[20];
   
    if(tx.IsCoinBase())
    {
        return true;
    }
    
    update_mempool=0;
    if(accept)
    {
        update_mempool=1;
    }
    
    total=0;
    
    asset_name[0]=0;
    multiple=1;
    new_issue=false;
    follow_on=false;
    out_count=0;
    issue_vout=-1;
    
    
    if(details->details_script_type == 0)                                       // New asset/variable with details script
    {
        value_offset=mc_FindSpecialParamInDetailsScript(details->details_script,details->details_script_size,MC_ENT_SPRM_NAME,&value_size);
        if(value_offset<(uint32_t)details->details_script_size)
        {
            if(value_size > MC_ENT_MAX_NAME_SIZE)
            {
                if(mc_gState->m_Features->FixedIn1001120003())
                {
                    reason="Metadata script rejected - entity name too long";
                    return false;                    
                }
                value_size=MC_ENT_MAX_NAME_SIZE;
            }
            
            memcpy(asset_name,details->details_script+value_offset,value_size);
            asset_name[value_size]=0x00;
        }
        value_offset=mc_FindSpecialParamInDetailsScript(details->details_script,details->details_script_size,MC_ENT_SPRM_ASSET_MULTIPLE,&value_size);
        if(value_offset<(uint32_t)details->details_script_size)
        {
            multiple=mc_GetLE(details->details_script+value_offset,value_size);
        }                                    
    }

    mc_gState->m_TmpAssetsOut->Clear();

    for (unsigned int vout = 0; vout < tx.vout.size(); vout++)
    {
                                                                                // We already extracted the details, we just have to find entity
        if(details->vOutputScriptFlags[vout] & MC_MTX_OUTPUT_DETAIL_FLAG_FOLLOWON_DETAILS)
        {
            MultiChainTransaction_SetTmpOutputScript(tx.vout[vout].scriptPubKey);
            mc_gState->m_TmpScript->SetElement(0);
                                                                        
            if(mc_gState->m_TmpScript->GetEntity(short_txid))           
            {
                reason="Metadata script rejected - wrong element, should be entityref";
                return false;
            }
        }
        
        if(details->vOutputScriptFlags[vout] & MC_MTX_OUTPUT_DETAIL_FLAG_NOT_OP_RETURN)
        {
            MultiChainTransaction_SetTmpOutputScript(tx.vout[vout].scriptPubKey);

            mc_gState->m_TmpAssetsTmp->Clear();
            issue_in_output=false;
            
            for (int e = 0; e < mc_gState->m_TmpScript->GetNumElements(); e++)
            {
                mc_gState->m_TmpScript->SetElement(e);
                err=mc_gState->m_TmpScript->GetAssetGenesis(&quantity);         
                if(err == 0)                                                    // Asset genesis issuance 
                {
                    out_count++;
                    if(e == 0)
                    {
                        issue_vout=vout;
                    }
                    issue_in_output=true;
                    new_issue=true;
                    if(quantity+total<0)
                    {
                        reason="Asset issue script rejected - overflow";
                        return false;                                        
                    }
                                        
                    total+=quantity;
                    
                    if(details->vOutputDestinations[vout].size() != 1)
                    {
                        reason="Asset issue script rejected - wrong destination type";
                        return false;                
                    }
                    
                    addressRet=details->vOutputDestinations[vout][0];
                    
                    CKeyID *lpKeyID=boost::get<CKeyID> (&addressRet);
                    CScriptID *lpScriptID=boost::get<CScriptID> (&addressRet);
                    if((lpKeyID == NULL) && (lpScriptID == NULL))
                    {
                        reason="Asset issue script rejected - wrong destination type";
                        return false;                
                    }
                    CBitcoinAddress address;
                    if(lpKeyID != NULL)
                    {
                        address=CBitcoinAddress(*lpKeyID);
                        memcpy(token_address,lpKeyID,sizeof(uint160));
                    }
                    else
                    {
                        address=CBitcoinAddress(*lpScriptID);
                        memcpy(token_address,lpScriptID,sizeof(uint160));
                    }
                    if(update_mempool)
                    {
                        if(fDebug)LogPrint("mchn","Found %s issue script in tx %s for %s - (%ld)\n",
                                (details->new_entity_type == MC_ENT_TYPE_LICENSE_TOKEN) ? "license token" : "asset",
                                tx.GetHash().GetHex().c_str(),
                                address.ToString().c_str(),quantity);                    
                    }
                }            
                else
                {
                    if(err != MC_ERR_WRONG_SCRIPT)
                    {
                        reason="Asset issue script rejected - error in script";
                        return false;                                    
                    }
                    if(mc_gState->m_Features->PerAssetPermissions())
                    {
                        err=mc_gState->m_TmpScript->GetAssetQuantities(mc_gState->m_TmpAssetsTmp,MC_SCR_ASSET_SCRIPT_TYPE_TRANSFER);
                        if((err != MC_ERR_NOERROR) && (err != MC_ERR_WRONG_SCRIPT))
                        {
                            reason="Script rejected - error in asset transfer script";
                            return false;                                
                        }
                    }                    

                    err=mc_gState->m_TmpScript->GetAssetQuantities(mc_gState->m_TmpAssetsOut,MC_SCR_ASSET_SCRIPT_TYPE_FOLLOWON);
                    if((err != MC_ERR_NOERROR) && (err != MC_ERR_WRONG_SCRIPT))
                    {
                        reason="Asset follow-on script rejected - error in follow-on script";
                        return false;                                
                    }
                    if(err == MC_ERR_NOERROR)
                    {
                        if(update_mempool)
                        {
                            if(fDebug)LogPrint("mchn","Found asset follow-on script in tx %s\n",
                                    tx.GetHash().GetHex().c_str());                    
                        }
                    }
                }                
            }        
            if(issue_in_output)
            {
                if(mc_gState->m_Features->PerAssetPermissions())
                {
                    if(mc_gState->m_TmpAssetsTmp->GetCount())
                    {
                        reason="Asset issue script rejected - asset transfer in script";
                        return false;                                    
                    }                    
                }
            }            
        }        
    }    

    if(mc_gState->m_Features->Variables())
    {
        if(details->new_entity_type == MC_ENT_TYPE_VARIABLE)                   
        {
            if(new_issue)
            {
                reason="Variable script rejected - genesis script found";
                return false;                                                            
            }
            if(mc_gState->m_TmpAssetsOut->GetCount())
            {
                reason="Variable script rejected - followon script found";
                return false;                                                                            
            }
        }
    }
    
    if(details->details_script_type >= 0)
    {
        if(details->details_script_type)                                        // Updates are allowed only for assets and variables
        {
            follow_on=true;            
        }
        else
        {
            if((details->new_entity_type == MC_ENT_TYPE_ASSET) || 
               (details->new_entity_type == MC_ENT_TYPE_VARIABLE))
                    // If not - we'll deal with it later
            {
                new_issue=true;
            }
        }
    }
    
    if(mc_gState->m_TmpAssetsOut->GetCount())
    {
        follow_on=true;
    }   
    
    last_total=0;
    chain_size=0;
    left_position=0;
    
    if(follow_on)
    {
        total=0;
        if(mc_gState->m_TmpAssetsOut->GetCount() > 1)
        {
            reason="Asset follow-on script rejected - follow-on for several assets";
            return false;                                                
        }
        if(new_issue)
        {
            reason="Asset follow-on script rejected - follow-on and issue in one transaction";
            return false;                                                            
        }
        ptrOut=NULL;
        if(mc_gState->m_TmpAssetsOut->GetCount() == 0)
        {
            if(mc_gState->m_Assets->FindEntityByShortTxID(&entity,short_txid) == 0)
            {
                reason="Details script rejected - entity not found";
                return false;                                    
            }                            
            if(entity.GetEntityType() != details->new_entity_type)              // Cannot happen before variables as asset was the only allowed followon
            {                
                reason="Details script rejected - entity type mismatch";
                return false;                                    
            }
            details->SetRelevantEntity(short_txid);
        }
        else
        {
            ptrOut=mc_gState->m_TmpAssetsOut->GetRow(0);       
            if(mc_gState->m_Assets->FindEntityByFullRef(&entity,ptrOut) == 0)
            {
                reason="Asset follow-on script rejected - asset not found";
                return false;                                                                        
            }            
        }
                
        if(entity.AllowedFollowOns() == 0)
        {
            reason="Asset follow-on script rejected - follow-ons not allowed for this asset";
            return false;                                                                                    
        }
        if(details->details_script_type > 0)
        {
            if(memcmp(entity.GetTxID()+MC_AST_SHORT_TXID_OFFSET,short_txid,MC_AST_SHORT_TXID_SIZE))
            {
                reason="Asset follow-on script rejected - mismatch in follow-on quantity asset and details script";
                return false;                                                                                                    
            }
        }
        if(mc_gState->m_Features->FixedIn20005())                               // Issumore was missed before 20005
        {
            details->SetRelevantEntity((unsigned char*)entity.GetTxID()+MC_AST_SHORT_TXID_OFFSET);
        }
        
        last_total=mc_gState->m_Assets->GetTotalQuantity(&entity,&chain_size);
        if(chain_size > 0)
        {
            left_position=mc_gState->m_Assets->GetChainLeftPosition(&entity,chain_size);
        }
        if(ptrOut)
        {
            total=mc_GetABQuantity(ptrOut);
            if(total+last_total < 0)
            {
                reason="Asset follow-on script rejected - exceeds maximal value for asset";
                return false;                                                                                                                
            }
        }
    }
    
    if(details->new_entity_type == MC_ENT_TYPE_LICENSE_TOKEN)
    {
        if(!new_issue)
        {
            reason="License token issue script rejected - issuance output not found";
            return false;                                                                                                                                            
        }
    }
    
    if(!new_issue && !follow_on)
    {
        return true;
    }
    
    details->fAssetIssuance=true;
    
    if(details->new_entity_output >= 0)
    {
        if( (details->new_entity_type != MC_ENT_TYPE_ASSET) && (details->new_entity_type != MC_ENT_TYPE_VARIABLE) )
        {
            if(details->new_entity_type == MC_ENT_TYPE_LICENSE_TOKEN)
            {                    
                if(mc_gState->m_Features->LicenseTokens())
                {
                    if(out_count != 1)
                    {
                        reason="License token issue script rejected - should have exactly one issuance output";
                        return false;                                                                                                                                
                    }
                    if(multiple != 1)
                    {
                        reason="License token issue script rejected - should have multiple=1";
                        return false;                                                                                                                                
                    }
                    if(total != 1)
                    {
                        reason="License token issue script rejected - should have single unit";
                        return false;                                                                                                                                
                    }
                    if(issue_vout < 0)
                    {
                        reason="License token issue script rejected - issuance element should be the first element in script";
                        return false;                                                                                                                                                        
                    }
                    
                    MultiChainTransaction_SetTmpOutputScript(tx.vout[issue_vout].scriptPubKey);
                    if(mc_gState->m_Features->License20010())
                    {
                        if(mc_gState->m_TmpScript->GetNumElements() < 2)
                        {
                            reason="License token issue script rejected - wrong number of elements";
                            return false;                                                                                                                                                                                
                        }
                    }
                    else
                    {
                        if(mc_gState->m_TmpScript->GetNumElements() != 3)
                        {
                            reason="License token issue script rejected - wrong number of elements";
                            return false;                                                                                                                                                                                
                        }
                    }
                    
                    if(!MultiChainTransaction_CheckLicenseTokenDetails(details,token_address,reason))
                    {
                        return false;                                                                                                                                                        
                    }
                    details->vOutputScriptFlags[issue_vout] |= MC_MTX_OUTPUT_DETAIL_FLAG_LICENSE_TRANSFER;
                    details->fLicenseTokenIssuance=true;
                }
            }
            if(!details->fLicenseTokenIssuance)
            {
                reason="Asset issue script rejected - not allowed in this transaction, conflicts with other entities";
                return false;                                                                                                        
            }
        }
    }
    
    details->fFullReplayCheckRequired=true;
    
    issuers.clear();                                                            // Creating issuers list
    for (unsigned int i = 0; i < tx.vin.size(); i++)
    {
        if(details->vInputHashTypes[i] == SIGHASH_ALL)
        {
            if(details->vInputDestinations[i] != 0)
            {
                bool can_issue=false;
                if(new_issue)
                {
                    if(details->new_entity_type == MC_ENT_TYPE_VARIABLE)                   
                    {
                        if(mc_gState->m_Permissions->CanCreate(NULL,(unsigned char*)&(details->vInputDestinations[i])))
                        {                            
                            can_issue=true;
                        }                                                    
                    }
                    else
                    {
                        if(mc_gState->m_Permissions->CanIssue(NULL,(unsigned char*)&(details->vInputDestinations[i])))
                        {                            
                            can_issue=true;
                        }                            
                        can_issue |= details->fLicenseTokenIssuance;
                    }
                }
                else
                {
                    if(details->new_entity_type == MC_ENT_TYPE_VARIABLE)                   
                    {
                        if(mc_gState->m_Permissions->CanWrite(entity.GetTxID(),(unsigned char*)&(details->vInputDestinations[i])))
                        {                            
                            can_issue=true;
                        }    
                    }
                    else
                    {
                        if(mc_gState->m_Permissions->CanIssue(entity.GetTxID(),(unsigned char*)&(details->vInputDestinations[i])))
                        {
                            can_issue=true;
                        }                                                        
                    }
                }                
                if(can_issue)
                {
                    issuers.push_back(details->vInputDestinations[i]);
                    flags=MC_PFL_NONE;
                    if(details->vInputScriptTypes[i] == TX_SCRIPTHASH)
                    {
                        flags |= MC_PFL_IS_SCRIPTHASH;
                    }
                    issuer_flags.push_back(flags);                        
                }
            }
        }
    }        

    if(issuers.size() == 0)
    {
        reason="Inputs don't belong to valid issuer";
        if(details->new_entity_type == MC_ENT_TYPE_VARIABLE)                   
        {
            reason="Inputs don't belong to valid creator";            
        }
        return false;
    }                

    err=MC_ERR_NOERROR;
    
    last_total+=total;
    
    mc_gState->m_TmpScript->Clear();
    mc_gState->m_TmpScript->AddElement();
    
    if(!details->fLicenseTokenIssuance)
    {
        mc_gState->m_TmpScript->SetSpecialParamValue(MC_ENT_SPRM_ASSET_TOTAL,(unsigned char*)&last_total,sizeof(last_total));                            
        
        if(chain_size >= 0)
        {
            mc_gState->m_TmpScript->SetSpecialParamValue(MC_ENT_SPRM_CHAIN_INDEX,(unsigned char*)&chain_size,sizeof(chain_size));                            
        }
        
        if(left_position > 0)
        {
            mc_gState->m_TmpScript->SetSpecialParamValue(MC_ENT_SPRM_LEFT_POSITION,(unsigned char*)&left_position,sizeof(left_position));                            
        }
        
        unsigned char issuer_buf[24];
        memset(issuer_buf,0,sizeof(issuer_buf));
        flags=MC_PFL_NONE;        
        uint32_t timestamp=0;
        set <uint160> stored_issuers;

                                                                                    // First per-entity record in permission database
                                                                                    // We'll need it for scanning asset-related rows
        if(new_issue)                                                               
        {
            err=MC_ERR_NOERROR;

            txid=tx.GetHash();
            err=mc_gState->m_Permissions->SetPermission(&txid,issuer_buf,MC_PTP_CONNECT,
                    (unsigned char*)issuers[0].begin(),0,(uint32_t)(-1),timestamp,flags | MC_PFL_ENTITY_GENESIS ,update_mempool,offset);
        }

        uint32_t all_permissions=MC_PTP_ADMIN | MC_PTP_ISSUE;
        if(mc_gState->m_Features->PerAssetPermissions())
        {
            all_permissions |= MC_PTP_ACTIVATE | MC_PTP_SEND | MC_PTP_RECEIVE;
        }
        if(details->new_entity_type == MC_ENT_TYPE_VARIABLE)                   
        {
            all_permissions = MC_PTP_ADMIN | MC_PTP_ACTIVATE | MC_PTP_WRITE;
        }

        for (unsigned int i = 0; i < issuers.size(); i++)                           // Setting per-asset permissions and creating issuers script
        {
            if(err == MC_ERR_NOERROR)
            {
                if(stored_issuers.count(issuers[i]) == 0)
                {
                    memcpy(issuer_buf,issuers[i].begin(),sizeof(uint160));
                    mc_PutLE(issuer_buf+sizeof(uint160),&issuer_flags[i],4);
                    if((int)i < mc_gState->m_Assets->MaxStoredIssuers())            // Adding list of issuers to the asset script
                    {
                        mc_gState->m_TmpScript->SetSpecialParamValue(MC_ENT_SPRM_ISSUER,issuer_buf,sizeof(issuer_buf));            
                    }
                    if(new_issue)                                                   // Setting first permission record - to scan from
                    {                    
                        err=mc_gState->m_Permissions->SetPermission(&txid,issuer_buf,all_permissions,
                                (unsigned char*)issuers[0].begin(),0,(uint32_t)(-1),timestamp,flags | MC_PFL_ENTITY_GENESIS ,update_mempool,offset);
                    }
                    stored_issuers.insert(issuers[i]);
                }
            }
        }        

        memset(issuer_buf,0,sizeof(issuer_buf));
        mc_gState->m_TmpScript->SetSpecialParamValue(MC_ENT_SPRM_ISSUER,issuer_buf,1);                    
        if(err)
        {
            reason="Cannot update permission database for issued asset";
            return false;            
        }
    }
    
    const unsigned char *special_script;
    size_t special_script_size=0;
    special_script=mc_gState->m_TmpScript->GetData(0,&special_script_size);
    txid=tx.GetHash();
    if(new_issue)                                                               // Updating entity database
    {        
        int entity_type=details->fLicenseTokenIssuance ? MC_ENT_TYPE_LICENSE_TOKEN : MC_ENT_TYPE_ASSET;
        if(details->new_entity_type == MC_ENT_TYPE_VARIABLE)                   
        {
            entity_type=MC_ENT_TYPE_VARIABLE;
        }
        err=mc_gState->m_Assets->InsertAsset(&txid,offset,entity_type,
                total,asset_name,multiple,details->details_script,details->details_script_size,special_script,special_script_size,details->extended_script_row,update_mempool);                      
    }
    else
    {
        err=mc_gState->m_Assets->InsertAssetFollowOn(&txid,offset,total,details->details_script,details->details_script_size,special_script,special_script_size,details->extended_script_row,entity.GetTxID(),update_mempool);
    }
            
          
    string entity_type_str="Asset";
    if(details->fLicenseTokenIssuance)
    {
        entity_type_str="License token";
    }
    if(details->new_entity_type == MC_ENT_TYPE_VARIABLE)
    {
        entity_type_str="Variable";        
    }
    
    if(err)           
    {
        reason="Asset issue script rejected - could not insert new asset to database";
        if(err == MC_ERR_FOUND)
        {
            reason=entity_type_str + " issue script rejected - entity with this name/asset-ref/txid already exists";                        
            if(mc_gState->m_Assets->FindEntityByTxID(&entity,(unsigned char*)&txid) == 0)
            {
                if(strlen(asset_name) == 0)
                {
                    mc_gState->m_Assets->FindEntityByName(&entity,asset_name);
                }
            }
            
            if(fDebug)LogPrint("mchn","%s already exists. TxID: %s, AssetRef: %d-%d-%d, Name: %s\n",
                    entity_type_str.c_str(),
                    tx.GetHash().GetHex().c_str(),
                    mc_gState->m_Assets->m_Block+1,offset,(int)(*((unsigned char*)&txid+31))+256*(int)(*((unsigned char*)&txid+30)),
                    entity.GetName());                                        
        }
        return false;                                            
    }
        
    
    if(update_mempool)
    {
        if(offset>=0)
        {
            if(mc_gState->m_Assets->FindEntityByTxID(&this_entity,(unsigned char*)&txid))
            {
                if(new_issue)
                {
                    if(fDebug)LogPrint("mchn","New %s. TxID: %s, AssetRef: %d-%d-%d, Name: %s\n",
                            entity_type_str.c_str(),
                            tx.GetHash().GetHex().c_str(),
                            mc_gState->m_Assets->m_Block+1,offset,(int)(*((unsigned char*)&txid+0))+256*(int)(*((unsigned char*)&txid+1)),
                            this_entity.GetName());                                        
                }
                else
                {
                    uint256 otxid;
                    memcpy(&otxid,entity.GetTxID(),32);
                    if(fDebug)LogPrint("mchn","Follow-on issue. TxID: %s,  Original %s issue txid: %s\n",
                            tx.GetHash().GetHex().c_str(),entity_type_str.c_str(),otxid.GetHex().c_str());
                }
            }
            else
            {
                reason="Asset issue script rejected - could not insert new asset to database";
                return false;                                            
            }
        }
    }
    
    return true;    
}


bool MultiChainTransaction_ProcessEntityCreation(const CTransaction& tx,        // Tx to check
                                                 int offset,                    // Tx offset in block, -1 if in memppol
                                                 bool accept,                   // Accept to mempools if successful
                                                 CMultiChainTxDetails *details, // Tx details object
                                                 string& reason)                // Error message
{
    if(details->new_entity_output < 0)
    {
        return true;
    }
    
    if(details->new_entity_type == MC_ENT_TYPE_ASSET)                           // Processed in another place
    {
        return true;        
    }
    
    if(details->new_entity_type == MC_ENT_TYPE_LICENSE_TOKEN)                   // Processed in another place
    {
        return true;        
    }
    
    if(details->new_entity_type == MC_ENT_TYPE_VARIABLE)                        // Processed in another place
    {
        return true;        
    }
    
    vector <uint160> openers;
    vector <uint32_t> opener_flags;
    unsigned char opener_buf[24];

    int err;
    string entity_type_str;
    uint32_t flags=MC_PFL_NONE;        
    uint32_t timestamp=0;
    set <uint160> stored_openers;
    int update_mempool;
    bool check_admin=true;
    uint256 txid;
    mc_EntityDetails entity;
        
    details->fFullReplayCheckRequired=true;
        
    update_mempool=0;
    if(accept)
    {
        update_mempool=1;            
    }
        
    openers.clear();                                                            // List of openers
    opener_flags.clear();
    for (unsigned int i = 0; i < tx.vin.size(); i++)
    {
        if(details->IsRelevantInput(i,details->new_entity_output))
        {
            if(mc_gState->m_Permissions->CanCreate(NULL,(unsigned char*)&(details->vInputDestinations[i])))
            {            
                if(details->new_entity_type <= MC_ENT_TYPE_STREAM_MAX)
                {
                    check_admin=false;
                }
                else
                {                    
                    if(details->new_entity_type == MC_ENT_TYPE_FILTER)
                    {
                        if(mc_gState->m_Features->StreamFilters())
                        {
                            uint32_t value_offset;
                            size_t value_size;
                            value_offset=mc_FindSpecialParamInDetailsScript(details->details_script,details->details_script_size,MC_ENT_SPRM_FILTER_TYPE,&value_size);
                            if(value_offset<(uint32_t)details->details_script_size)
                            {
                                if((uint32_t)mc_GetLE(details->details_script+value_offset,value_size) == MC_FLT_TYPE_STREAM)
                                {
                                    check_admin=false;
                                }
                            }                                    
                            
                        }
                    }
                }
                
//                if( (details->new_entity_type <= MC_ENT_TYPE_STREAM_MAX) ||     // Admin persmission is required for upgrades and filters
                if( !check_admin ||
                    (mc_gState->m_Permissions->CanAdmin(NULL,(unsigned char*)&(details->vInputDestinations[i])) != 0) )
                {
                    openers.push_back(details->vInputDestinations[i]);
                    flags=MC_PFL_NONE;
                    if(details->vInputScriptTypes[i] == TX_SCRIPTHASH)
                    {
                        flags |= MC_PFL_IS_SCRIPTHASH;
                    }
                    opener_flags.push_back(flags);
                }
            }                            
        }
    }                
        
    if(openers.size() == 0)
    {
        reason="Metadata script rejected - Inputs don't belong to valid creator";
        return false;
    }

    err=MC_ERR_NOERROR;

    mc_gState->m_TmpScript->Clear();
    mc_gState->m_TmpScript->AddElement();
    txid=tx.GetHash();                                                          // Setting first record in the per-entity permissions list
    
    if(details->new_entity_type <= MC_ENT_TYPE_STREAM_MAX)
    {
        memset(opener_buf,0,sizeof(opener_buf));
        err=mc_gState->m_Permissions->SetPermission(&txid,opener_buf,MC_PTP_CONNECT,
                (unsigned char*)openers[0].begin(),0,(uint32_t)(-1),timestamp, MC_PFL_ENTITY_GENESIS ,update_mempool,offset);
    }
    
    for (unsigned int i = 0; i < openers.size(); i++)
    {
        if(err == MC_ERR_NOERROR)
        {
            if(stored_openers.count(openers[i]) == 0)
            {
                memcpy(opener_buf,openers[i].begin(),sizeof(uint160));
                mc_PutLE(opener_buf+sizeof(uint160),&opener_flags[i],4);
                if((int)i < mc_gState->m_Assets->MaxStoredIssuers())
                {
                    mc_gState->m_TmpScript->SetSpecialParamValue(MC_ENT_SPRM_ISSUER,opener_buf,sizeof(opener_buf));            
                }
                if(details->new_entity_type <= MC_ENT_TYPE_STREAM_MAX)
                {
                                                                                // Granting default per-entity permissions to openers
                    err=mc_gState->m_Permissions->SetPermission(&txid,opener_buf,MC_PTP_ADMIN | MC_PTP_ACTIVATE | MC_PTP_WRITE | MC_PTP_READ,
                            (unsigned char*)openers[i].begin(),0,(uint32_t)(-1),timestamp,opener_flags[i] | MC_PFL_ENTITY_GENESIS ,update_mempool,offset);
                }
                stored_openers.insert(openers[i]);
            }
        }
    }        
    
    if(err)
    {
        reason=" Cannot update permission database for new entity";
        return false;
    }

    memset(opener_buf,0,sizeof(opener_buf));                                    // Storing opener list in entity metadata
    mc_gState->m_TmpScript->SetSpecialParamValue(MC_ENT_SPRM_ISSUER,opener_buf,1);                    

    const unsigned char *special_script;
    size_t special_script_size=0;
    special_script=mc_gState->m_TmpScript->GetData(0,&special_script_size);
                                                                                // Updating entity datanase
    err=mc_gState->m_Assets->InsertEntity(&txid,offset,details->new_entity_type,details->details_script,details->details_script_size,
            special_script,special_script_size,details->extended_script_row,update_mempool);

    if(err)           
    {
        reason="New entity script rejected - could not insert new entity to database";
        if(err == MC_ERR_ERROR_IN_SCRIPT)
        {
            reason="New entity script rejected - error in script";                        
        }
        if(err == MC_ERR_FOUND)
        {
            reason="New entity script rejected - entity with this name already exists";                        
        }
        return false;
    }

    if(update_mempool)
    {
        if(mc_gState->m_Assets->FindEntityByTxID(&entity,(unsigned char*)&txid))
        {
            entity_type_str="stream";
            if(details->new_entity_type == MC_ENT_TYPE_UPGRADE)
            {
                entity_type_str="upgrade";
            }
            if(details->new_entity_type == MC_ENT_TYPE_FILTER)
            {
                entity_type_str="filter";
            }
            if(offset>=0)
            {
                LogPrintf("New %s. TxID: %s, StreamRef: %d-%d-%d, Name: %s\n",
                        entity_type_str.c_str(),tx.GetHash().GetHex().c_str(),
                        mc_gState->m_Assets->m_Block+1,offset,(int)(*((unsigned char*)&txid+0))+256*(int)(*((unsigned char*)&txid+1)),
                        entity.GetName());                                        
            }
            else
            {
                LogPrintf("New %s. TxID: %s, unconfirmed, Name: %s\n",
                        entity_type_str.c_str(),tx.GetHash().GetHex().c_str(),entity.GetName());                                                            
            }
            if(details->new_entity_type == MC_ENT_TYPE_FILTER)
            {
                pMultiChainFilterEngine->Add((unsigned char*)&txid+MC_AST_SHORT_TXID_OFFSET,(offset < 0) ? 0 : 1);
            }
        }
        else
        {
            reason="New entity script rejected - could not insert new entity to database";
            return false;
        }
    }
    
    return true;
}

bool MultiChainTransaction_VerifyNotFilteredRestrictions(const CTransaction& tx,        // Tx to check
                                                         int offset,                    // Tx offset in block, -1 if in memppol
                                                         bool accept,                   // Accept to mempools if successful
                                                         CMultiChainTxDetails *details, // Tx details object
                                                         string& reason)                // Error message
{
    if(details->emergency_disapproval_output < 0)
    {
        return true;        
    }
    
    if(tx.IsCoinBase())
    {
        details->emergency_disapproval_output=-2;
        return true;
    }
        
    if(tx.vin.size() > 1)
    {
        details->emergency_disapproval_output=-2;
        return true;
    }
    
    if(details->vInputDestinations.size() != 1)
    {
        details->emergency_disapproval_output=-2;
        return true;
    }
    
    if(tx.vout.size() > 2)
    {
        details->emergency_disapproval_output=-2;
        return true;
    }
            
    for (unsigned int vout = 0; vout < tx.vout.size(); vout++)
    {
        if(mc_gState->m_Features->FixedIn20005())
        {
            if( (details->vOutputScriptFlags[vout] & MC_MTX_OUTPUT_DETAIL_FLAG_NOT_OP_RETURN) == 0)
            {
                details->emergency_disapproval_output=-2;
                return true;            
            }            
        }
        else
        {
            if(details->vOutputScriptFlags[vout] & MC_MTX_OUTPUT_DETAIL_FLAG_NOT_OP_RETURN)
            {
                details->emergency_disapproval_output=-2;
                return true;            
            }
        }
                
        MultiChainTransaction_SetTmpOutputScript(tx.vout[vout].scriptPubKey);
        
        if((int)vout == details->emergency_disapproval_output)
        {
            if(mc_gState->m_TmpScript->GetNumElements() > 1)
            {
                details->emergency_disapproval_output=-2;
                return true;                            
            }        
        }
        else
        {
            mc_gState->m_TmpAssetsTmp->Clear();
            for (int e = 0; e < mc_gState->m_TmpScript->GetNumElements(); e++)
            {
                mc_gState->m_TmpScript->SetElement(e);
                if(mc_gState->m_TmpScript->GetAssetQuantities(mc_gState->m_TmpAssetsTmp,MC_SCR_ASSET_SCRIPT_TYPE_TRANSFER) != MC_ERR_NOERROR)
                {
                    details->emergency_disapproval_output=-2;
                    return true;                                                
                }
            }
            
            CKeyID *lpKeyID=boost::get<CKeyID> (&(details->vOutputSingleDestination[vout]));
            CScriptID *lpScriptID=boost::get<CScriptID> (&(details->vOutputSingleDestination[vout]));
            if(((lpKeyID == NULL) && (lpScriptID == NULL)) || (details->vOutputDestinations[vout].size() != 1))
            {
                details->emergency_disapproval_output=-2;
                return true;                                                
            }
            if(lpKeyID)
            {
                if(*(uint160*)lpKeyID != details->vInputDestinations[0])
                {
                    details->emergency_disapproval_output=-2;
                    return true;                                                
                }
            }
            if(lpScriptID)
            {
                if(*(uint160*)lpScriptID != details->vInputDestinations[0])
                {
                    details->emergency_disapproval_output=-2;
                    return true;                                                
                }
            }
        }
    }
    
    if(details->emergency_disapproval_output >= 0)
    {
        if(fDebug)LogPrint("filter","Standard filter disapproval - tx will bypass all filter\n");
    }
    return true;
}

bool MultiChainTransaction_VerifyStandardCoinbase(const CTransaction& tx,        // Tx to check
                                                        CMultiChainTxDetails *details, // Tx details object
                                                        string& reason)                // Error message
{
    details->fIsStandardCoinbase=false;
    
    if(!tx.IsCoinBase())
    {
        return true;
    }
    
    if(mc_gState->m_Features->FixedIn20005() == 0)                              // Coinbase is checked like all other txs before 20005
    {
        return true;        
    }
    
    bool value_output=false;
    bool signature_output=false;
    for (unsigned int vout = 0; vout < tx.vout.size(); vout++)
    {
        MultiChainTransaction_SetTmpOutputScript(tx.vout[vout].scriptPubKey);

        if(mc_gState->m_TmpScript->IsOpReturnScript())                    
        {
            if(signature_output)
            {
                LogPrintf("Non-standard coinbase: Multiple signatures\n");
                return true;                                                    
            }
            if(mc_gState->m_TmpScript->GetNumElements() != 1)
            {
                if(mc_gState->m_Permissions->m_Block >= 0)
                {
                    LogPrintf("Non-standard coinbase: Non-standard signature\n");
                    return true;                                                    
                }
            }
            signature_output=true;
        }   
        else
        {
            if(tx.vout[vout].nValue == 0)
            {
                if(value_output)
                {
                    LogPrintf("Non-standard coinbase: Multiple value outputs\n");
                    return true;                                                // Only single "value" output is allowed
                }
                if(mc_gState->m_Permissions->m_Block > 0)
                {
                    LogPrintf("Non-standard coinbase: Value output for chain without native currency\n");
                    return true;                                                // No "value" outputs for the blocks 2+
                }
            }
            value_output=true;
        }
        
    }
    
    if(!value_output)
    {
        if(mc_gState->m_Permissions->m_Block <= 0)
        {
            LogPrintf("Non-standard coinbase: Missing value output\n");
        }        
    }
    
    if(signature_output == (mc_gState->m_NetworkParams->IsProtocolMultichain() == 0))
    {
        LogPrintf("Non-standard coinbase: Sigature output doesn't match protocol\n");
        return true;                                                            // Signature output should appear only if protocol=multichain
    }
        
    details->fIsStandardCoinbase=true;
    
    return true;    
}

bool AcceptMultiChainTransaction   (const CTransaction& tx,                     // Tx to check
                                    const CCoinsViewCache &inputs,              // Tx inputs from UTXO database
                                    int offset,                                 // Tx offset in block, -1 if in memppol
                                    bool accept,                                // Accept to mempools if successful
                                    string& reason,                             // Error message
                                    int64_t *mandatory_fee_out,                 // Mandatory fee
                                    uint32_t *replay)                           // Replay flag - if tx should be rechecked or only permissions
{
    CMultiChainTxDetails details;
    bool fReject=false;
    int64_t mandatory_fee=0;
            
    if(mc_gState->m_NetworkParams->IsProtocolMultichain() == 0)                 
    {
        return true;
    }    
    
    details.fCheckCachedScript=MultiChainTransaction_CheckCachedScriptFlag(tx);
    
    mc_gState->m_TmpAssetsIn->Clear();
    
    if(!MultiChainTransaction_CheckCoinbaseInputs(tx,&details))                 // Inputs
    {
        if(!MultiChainTransaction_CheckInputs(tx,inputs,&details,reason))
        {
            return false;
        }
    }
   
    mc_gState->m_Permissions->SetCheckPoint();                                  // if there is an error after this point or it is just check, permission mempool should be restored
    mc_gState->m_Assets->SetCheckPoint();    
    
    if(!MultiChainTransaction_CheckOutputs(tx,inputs,offset,&details,reason))   // Outputs        
    {
        fReject=true;
        goto exitlbl;                                                                    
    }
    
    if(!MultiChainTransaction_ProcessAssetIssuance(tx,offset,accept,&details,reason))                // Asset genesis/followon
    {
        fReject=true;
        goto exitlbl;                                                                    
    }

    if(!MultiChainTransaction_CheckMandatoryFee(&details,&mandatory_fee,reason))
    {
        fReject=true;
        goto exitlbl;                                                                            
    }                                                                           

    if(!MultiChainTransaction_CheckTransfers(tx,offset,&details,reason))        // Transfers and receive permissions
    {
        fReject=true;
        goto exitlbl;                                                                    
    }
                                                                                // Creating of (pseudo)streams/upgrades
    if(!MultiChainTransaction_ProcessEntityCreation(tx,offset,accept,&details,reason))           
    {
        fReject=true;
        goto exitlbl;                                                                    
    }

    if(!MultiChainTransaction_VerifyNotFilteredRestrictions(tx,offset,accept,&details,reason))           
    {
        fReject=true;
        goto exitlbl;                                                                                
    }        
    
    if(!MultiChainTransaction_VerifyStandardCoinbase(tx,&details,reason))           
    {
        fReject=true;
        goto exitlbl;                                                                                
    }        
    
    if( (details.emergency_disapproval_output < 0) && !details.fIsStandardCoinbase && !details.fLicenseTokenTransfer)
    {
        if(mc_gState->m_Features->Filters())
        {
            if(pMultiChainFilterEngine)
            {
                mc_MultiChainFilter* lpFilter;
                int applied=0;

                if(pMultiChainFilterEngine->RunTxFilters(tx,details.vRelevantEntities,reason,&lpFilter,&applied,(offset >= 0)) != MC_ERR_NOERROR)
                {
                    reason="Error while running filters";
                    fReject=true;
                    goto exitlbl;                                                                    
                }
                else
                {
                    if(reason.size())
                    {
                        if(fDebug)LogPrint("mchn","mchn: Rejecting filter: %s\n",lpFilter->m_FilterCaption.c_str());
                        fReject=true;
                        goto exitlbl;                                                                                    
                    }
                    if(applied)
                    {
                        details.fFullReplayCheckRequired=true;                        
                    }
                }
            }
        }
    }
                                                                                // Custom filters
    fReject=!custom_accept_transacton(tx,inputs,offset,accept,reason,replay);
    
exitlbl:
                                    
    if(accept)
    {
        if(details.fSeedNodeInvolved)                                           // Checking if we should disconnect from seed node
        {
            CNode* seed_node;
            seed_node=(CNode*)(mc_gState->m_pSeedNode);

            if(!mc_gState->m_Permissions->CanConnect(NULL,seed_node->kAddrRemote.begin()))
            {
                LogPrintf("mchn: Seed node lost connect permission \n");
                mc_gState->m_pSeedNode=NULL;
            }
        }
    }

    if(!accept || fReject)                                                      // Rolling back permission database if we were just checking or error occurred    
    {
        mc_gState->m_Permissions->RollBackToCheckPoint();
        mc_gState->m_Assets->RollBackToCheckPoint();
    }

    if(mandatory_fee_out)
    {
        *mandatory_fee_out=mandatory_fee;
    }

    if(replay)
    {
        *replay=0;
        if(details.fFullReplayCheckRequired)
        {
            *replay |= MC_PPL_REPLAY;
        }
        if(details.fAdminMinerGrant)
        {
            *replay |= MC_PPL_ADMINMINERGRANT;
        }
    }

    if(fReject)
    {
        if(fDebug)LogPrint("mchn","mchn: Tx rejected (%s): %s\n",reason.c_str(),EncodeHexTx(tx));
    }

    return !fReject;
    
}

bool AcceptAdminMinerPermissions(const CTransaction& tx,
                                 int offset,
                                 bool verify_signatures,
                                 string& reason,
                                 uint32_t *result)
{
    vector <txnouttype> vInputScriptTypes;
    vector <uint160> vInputDestinations;
    vector <int> vInputHashTypes;
    vector <bool> vInputCanGrantAdminMine;
    vector <bool> vInputHadAdminPermissionBeforeThisTx;
    vector <CScript> vInputPrevOutputScripts;
    bool fIsEntity;
    bool fReject;    
    bool fAdminFound;
    int err;

    if(result)
    {
        *result=0;
    }
    
    if(tx.IsCoinBase())
    {
        return true;
    }
    
    for (unsigned int i = 0; i < tx.vin.size(); i++)                            
    {                                                                                                                                                                
        vInputCanGrantAdminMine.push_back(false);
        vInputPrevOutputScripts.push_back(CScript());
        vInputDestinations.push_back(0);
    }
    
    fReject=false;
    fAdminFound=false;
    for (unsigned int j = 0; j < tx.vout.size(); j++)
    {
        int cs_offset,cs_new_offset,cs_size,cs_vin;
        unsigned char *cs_script;
            
        const CScript& script1 = tx.vout[j].scriptPubKey;        
        CScript::const_iterator pc1 = script1.begin();


        mc_gState->m_TmpScript->Clear();
        mc_gState->m_TmpScript->SetScript((unsigned char*)(&pc1[0]),(size_t)(script1.end()-pc1),MC_SCR_TYPE_SCRIPTPUBKEY);
                       
        if(mc_gState->m_TmpScript->IsOpReturnScript())                      
        {                
            if( mc_gState->m_TmpScript->GetNumElements() == 2 ) 
            {
                mc_gState->m_TmpScript->SetElement(0);
                                                  
                cs_offset=0;
                while( (err=mc_gState->m_TmpScript->GetCachedScript(cs_offset,&cs_new_offset,&cs_vin,&cs_script,&cs_size)) != MC_ERR_WRONG_SCRIPT )
                {
                    if(err != MC_ERR_NOERROR)
                    {
                        reason="Metadata script rejected - error in cached script";
                        fReject=true;
                        goto exitlbl;                                                                                                                            
                    }
                    if(cs_offset)
                    {
                        if( cs_vin >= (int)tx.vin.size() )
                        {
                            reason="Metadata script rejected - invalid input in cached script";
                            fReject=true;
                            goto exitlbl;                                                                                                                                                                
                        }
                        vInputPrevOutputScripts[cs_vin]=CScript(cs_script,cs_script+cs_size);                            
                        vInputCanGrantAdminMine[cs_vin]=true;
                    }
                    cs_offset=cs_new_offset;
                }
            }
        }
    }
    
    for (unsigned int i = 0; i < tx.vin.size(); i++)        
    {                                                                                                                                                                
        if(vInputCanGrantAdminMine[i])
        {
            vInputCanGrantAdminMine[i]=false;
            const CScript& script2 = tx.vin[i].scriptSig;        
            CScript::const_iterator pc2 = script2.begin();
            if (!script2.IsPushOnly())
            {
                reason="sigScript should be push-only";
                fReject=true;
                goto exitlbl;                                                                                                                                                                
            }

            const CScript& script1 = vInputPrevOutputScripts[i];        
            CScript::const_iterator pc1 = script1.begin();

            txnouttype typeRet;
            int nRequiredRet;
            vector<CTxDestination> addressRets;
            int op_addr_offset,op_addr_size,is_redeem_script,sighash_type,check_last;

            sighash_type=SIGHASH_NONE;
            if(ExtractDestinations(script1,typeRet,addressRets,nRequiredRet)) 
            {
                if ( (typeRet != TX_NULL_DATA) && (typeRet != TX_MULTISIG) )                                  
                {
                    CKeyID *lpKeyID=boost::get<CKeyID> (&addressRets[0]);
                    CScriptID *lpScriptID=boost::get<CScriptID> (&addressRets[0]);
                    if( (lpKeyID == NULL) && (lpScriptID == NULL) )
                    {
                        fReject=true;
                        goto exitlbl;                                                                                                                                                                
                    }
                    if(lpKeyID)
                    {
                        vInputDestinations[i]=*(uint160*)lpKeyID;                               
                    }
                    if(lpScriptID)
                    {
                        vInputDestinations[i]=*(uint160*)lpScriptID;                               
                    }
                    
                    check_last=0;
                    if( typeRet == TX_PUBKEY )
                    {
                        check_last=1;
                    }

                                                                                // Find sighash_type
                    mc_ExtractAddressFromInputScript((unsigned char*)(&pc2[0]),(int)(script2.end()-pc2),&op_addr_offset,&op_addr_size,&is_redeem_script,&sighash_type,check_last);        
                    if(sighash_type == SIGHASH_ALL)
                    {
                        if(mc_gState->m_Permissions->CanAdmin(NULL,(unsigned char*)&vInputDestinations[i]))
                        {
                            vInputCanGrantAdminMine[i]=true;
                            if(verify_signatures)
                            {
                                vInputCanGrantAdminMine[i]=false;
                                if(VerifyScript(script2, script1, STANDARD_SCRIPT_VERIFY_FLAGS, CachingTransactionSignatureChecker(&tx, i, false)))
                                {
                                    vInputCanGrantAdminMine[i]=true;
                                }
                                else
                                {
                                    reason="Signature verification error";
                                    fReject=true;
                                    goto exitlbl;                                                                                                                                                                                                    
                                }
                            }
                        } 
                    }
                }
            }    
        }        
    }    
    
    for (unsigned int j = 0; j < tx.vout.size(); j++)
    {
        unsigned char short_txid[MC_AST_SHORT_TXID_SIZE];
        uint32_t type,from,to,timestamp,flags;
            
        const CScript& script1 = tx.vout[j].scriptPubKey;        
        CScript::const_iterator pc1 = script1.begin();


        mc_gState->m_TmpScript->Clear();
        mc_gState->m_TmpScript->SetScript((unsigned char*)(&pc1[0]),(size_t)(script1.end()-pc1),MC_SCR_TYPE_SCRIPTPUBKEY);
        
        CTxDestination addressRet;

        if(ExtractDestination(script1,addressRet))
        {            
            fIsEntity=false;

            for (int e = 0; e < mc_gState->m_TmpScript->GetNumElements(); e++)
            {
                mc_gState->m_TmpScript->SetElement(e);
                if(mc_gState->m_TmpScript->GetEntity(short_txid) == 0)      
                {
                    if(fIsEntity)
                    {
                        reason="Script rejected - duplicate entity script";
                        fReject=true;
                        goto exitlbl;                                                
                    }
                    if(mc_gState->m_Features->FixedIn1000920001())
                    {
                        fIsEntity=true;
                    }
                }
                else                                                        
                {   
                    if(mc_gState->m_TmpScript->GetPermission(&type,&from,&to,&timestamp) == 0)
                    {                        
                        type &= ( MC_PTP_MINE | MC_PTP_ADMIN );                                        
                        if(fIsEntity)
                        {
                            type=0;
                        }
                        if(type)
                        {
                            CKeyID *lpKeyID=boost::get<CKeyID> (&addressRet);
                            CScriptID *lpScriptID=boost::get<CScriptID> (&addressRet);
                            if((lpKeyID == NULL) && (lpScriptID == NULL))
                            {
                                reason="Permission script rejected - wrong destination type";
                                fReject=true;
                                goto exitlbl;
                            }
                            unsigned char* ptr=NULL;
                            flags=MC_PFL_NONE;
                            if(lpKeyID != NULL)
                            {
                                ptr=(unsigned char*)(lpKeyID);
                            }
                            else
                            {
                                flags=MC_PFL_IS_SCRIPTHASH;
                                ptr=(unsigned char*)(lpScriptID);
                            }
                            fAdminFound=false;
                            for (unsigned int i = 0; i < tx.vin.size(); i++)
                            {
                                if(vInputCanGrantAdminMine[i])
                                {
                                    if(mc_gState->m_Permissions->SetPermission(NULL,ptr,type,(unsigned char*)&vInputDestinations[i],from,to,timestamp,flags,1,offset) == 0)
                                    {
                                        fAdminFound=true;
                                    }                                
                                }
                            }    
                            if(!fAdminFound)
                            {
                                reason="Inputs don't belong to valid admin";
                                fReject=true;
                                goto exitlbl;                                                            
                            }                                
                            else
                            {
                                if(result)
                                {
                                    *result |= MC_PPL_ADMINMINERGRANT;
                                }                                
                            }
                        }
                        fIsEntity=false;
                    }
                    else                                                   
                    {
                        if(fIsEntity)                              
                        {
                            reason="Script rejected - entity script should be followed by permission";
                            fReject=true;
                            goto exitlbl;                                                
                        }
                    }
                }
            }                                                                              
        }
    }    
    
exitlbl:        
    
    if(fReject)
    {
        if(fDebug)LogPrint("mchn","mchn: AcceptAdminMinerPermissions: Tx rejected: %s\n",EncodeHexTx(tx));
    }

    return !fReject;
}


