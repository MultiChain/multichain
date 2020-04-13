// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "core/main.h"
#include "utils/util.h"
#include "multichain/multichain.h"
#include "wallet/wallettxs.h"
#include "community/community.h"

#include <boost/assign/list_of.hpp>

extern mc_WalletTxs* pwalletTxsMain;


using namespace std;

bool AcceptMultiChainTransaction(const CTransaction& tx, 
                                 const CCoinsViewCache &inputs,
                                 int offset,
                                 bool accept,
                                 string& reason,
                                 int64_t *mandatory_fee_out,     
                                 uint32_t *replay);
bool AcceptAdminMinerPermissions(const CTransaction& tx,
                                 int offset,
                                 bool verify_signatures,
                                 string& reason,
                                 uint32_t *result);
bool AcceptAssetTransfers(const CTransaction& tx, const CCoinsViewCache &inputs, string& reason);
bool AcceptAssetGenesis(const CTransaction &tx,int offset,bool accept,string& reason);
bool AcceptPermissionsAndCheckForDust(const CTransaction &tx,bool accept,string& reason);
bool IsTxBanned(uint256 txid);


int CreateUpgradeLists(int current_height,vector<mc_UpgradedParameter> *vParams,vector<mc_UpgradeStatus> *vUpgrades)
{
    mc_EntityDetails entity;
    mc_Buffer *upgrades;
    mc_UpgradeStatus upgrade;
    mc_UpgradedParameter param;
            
    upgrades=NULL;
    set <uint160> stored_upgrades;
    map <uint64_t,int> map_sorted;
    map <string,int> map_last_upgrade;
    uint160 hash=0;

    int OriginalProtocolVersion=(int)mc_gState->m_NetworkParams->GetInt64Param("protocolversion");
    int NewProtocolVersion=OriginalProtocolVersion;
    int version;
    int err=MC_ERR_NOERROR;

    if(vUpgrades)
    {
        vUpgrades->clear();
    }
    vParams->clear();
        
    upgrades=mc_gState->m_Permissions->GetUpgradeList(NULL,NULL);

    
    for(int i=0;i<upgrades->GetCount();i++)
    {
        mc_PermissionDetails *plsRow;
        plsRow=(mc_PermissionDetails *)(upgrades->GetRow(i));
        if(plsRow->m_Type == MC_PTP_UPGRADE)
        {            
            if(vParams)
            {
                memcpy(&hash,plsRow->m_Address,sizeof(uint160));
                stored_upgrades.insert(hash);
            }
            map_sorted.insert(std::make_pair(plsRow->m_LastRow,i));
        }        
    }   
    
    if(vParams)
    {
        for(int i=0;i<upgrades->GetCount();i++)
        {
            mc_PermissionDetails *plsRow;
            plsRow=(mc_PermissionDetails *)(upgrades->GetRow(i));
            if(plsRow->m_Type != MC_PTP_UPGRADE)
            {
                memcpy(&hash,plsRow->m_Address,sizeof(uint160));
                if(stored_upgrades.count(hash) == 0)
                {
                    plsRow->m_BlockTo = 0;
                    map_sorted.insert(std::make_pair(plsRow->m_LastRow,i));
                }
            }
        }    
    }
    
/*    
    for(int i=0;i<permissions->GetCount();i++)
    {        
        mc_PermissionDetails *plsRow;
        plsRow=(mc_PermissionDetails *)(permissions->GetRow(i));
        if(plsRow->m_Type == MC_PTP_UPGRADE)
        {
            map_sorted.insert(std::make_pair(plsRow->m_LastRow,i));
        }        
    }
*/    
    BOOST_FOREACH(PAIRTYPE(const uint64_t, int)& item, map_sorted)
    {
        int i=item.second;
        mc_PermissionDetails *plsRow;
        plsRow=(mc_PermissionDetails *)(upgrades->GetRow(i));
//        if(plsRow->m_Type == MC_PTP_UPGRADE)
        if(err == MC_ERR_NOERROR)
        {
            memset(&upgrade,0,sizeof(mc_UpgradeStatus));
            memcpy(upgrade.m_EntityShortTxID,plsRow->m_Address,MC_AST_SHORT_TXID_SIZE);
            upgrade.m_ApprovedBlock=current_height+2;
            upgrade.m_AppliedBlock=current_height+2;
            upgrade.m_FirstParam=(int)vParams->size();
            if(plsRow->m_BlockFrom < plsRow->m_BlockTo) 
            {
                upgrade.m_ApprovedBlock=plsRow->m_BlockReceived;
                if(mc_gState->m_Assets->FindEntityByShortTxID(&entity,plsRow->m_Address))
                {
                    int applied_height=entity.UpgradeStartBlock();
                    if((int)plsRow->m_BlockReceived > applied_height)
                    {
                        applied_height=plsRow->m_BlockReceived;
                    }
                    upgrade.m_AppliedBlock=applied_height;
                    if(current_height >= applied_height)
                    {
                        version=entity.UpgradeProtocolVersion();
                        if(version > 0)
                        {
                            param.m_Param=mc_gState->m_NetworkParams->FindParam("protocolversion");
                            param.m_Value=version;
                            param.m_Block=upgrade.m_AppliedBlock;
                            param.m_Skipped=MC_PSK_APPLIED;
                            if(version >= mc_gState->MinProtocolDowngradeVersion())
                            {
                                if((NewProtocolVersion < mc_gState->MinProtocolForbiddenDowngradeVersion()) || (version >= NewProtocolVersion))
                                {
                                    NewProtocolVersion=version;
                                    if( mc_gState->IsSupported(version) == 0 )
                                    {
                                        err=MC_ERR_NOT_SUPPORTED;
                                    }
                                }
                                else
                                {
                                    param.m_Skipped = MC_PSK_OLD_NOT_DOWNGRADABLE;                                    
                                }
                            }
                            else
                            {
                                param.m_Skipped = MC_PSK_NEW_NOT_DOWNGRADABLE;
                            }
                            vParams->push_back(param);
                        }

                        if(err == MC_ERR_NOERROR)
                        {
                            int size=0;                        
                            char* ptr=(char*)entity.GetParamUpgrades(&size);
                            char* ptrEnd;
                            int param_size,given_size;
                            int64_t param_value;
                            if(ptr)
                            {
                                ptrEnd=ptr+size;
                                while(ptr<ptrEnd)
                                {
                                    param.m_Param=mc_gState->m_NetworkParams->FindParam(ptr);                                
                                    ptr+=mc_gState->m_NetworkParams->GetParamFromScript(ptr,&param_value,&given_size);
                                    param.m_Value=param_value;
                                    param.m_Block=upgrade.m_AppliedBlock;  
                                    param.m_Skipped=MC_PSK_APPLIED;
                                    if(param.m_Param)
                                    {
                                        param_size=mc_gState->m_NetworkParams->CanBeUpgradedByVersion(param.m_Param->m_Name,NewProtocolVersion,0);
                                        if( (param_size > 0) && (param_size == given_size) )
                                        {
                                            if(mc_gState->m_NetworkParams->IsParamUpgradeValueInRange(param.m_Param,NewProtocolVersion,param_value))
                                            {
                                                bool take_it=true;
                                                string param_name=string(param.m_Param->m_Name);
                                                map <string,int>::iterator it = map_last_upgrade.find(param_name); 
                                                
                                                if (it != map_last_upgrade.end())
                                                {
                                                    take_it=false;
                                                    if(( param.m_Param->m_Type & MC_PRM_DATA_TYPE_MASK) == MC_PRM_BOOLEAN )
                                                    {
                                                        take_it=true;                                                        
                                                    }
                                                    else                                                        
                                                    {                                                    
                                                        if( ( (param.m_Param->m_Type & MC_PRM_TIME) == 0 ) ||
                                                               ((*vParams)[it->second].m_Block + MIN_BLOCKS_BETWEEN_UPGRADES <= upgrade.m_AppliedBlock) )
                                                        {
                                                            int64_t old_value=(*vParams)[it->second].m_Value;
                                                            if(param.m_Value >= old_value)
                                                            {
                                                                if(param_value <= 2*old_value)
                                                                {
                                                                    take_it=true;
                                                                }
                                                            }
                                                            else
                                                            {
                                                                if(old_value <= 2*param_value)
                                                                {
                                                                    take_it=true;
                                                                }                                                            
                                                            }
                                                            if(!take_it)
                                                            {
                                                                param.m_Skipped =MC_PSK_DOUBLE_RANGE;
                                                            }
                                                        }
                                                        else
                                                        {
                                                            param.m_Skipped = MC_PSK_FRESH_UPGRADE;
                                                        }
                                                    }
                                                    if(take_it)
                                                    {
                                                        it->second=(int)vParams->size();
                                                    }
                                                }
                                                else
                                                {
                                                    take_it=false;
                                                    if(( param.m_Param->m_Type & MC_PRM_DATA_TYPE_MASK) == MC_PRM_BOOLEAN )
                                                    {
                                                        take_it=true;                                                        
                                                    }
                                                    else                                                        
                                                    {
                                                        int64_t old_value=mc_gState->m_NetworkParams->GetInt64Param(param.m_Param->m_Name);

                                                        if(param.m_Value >= old_value)
                                                        {
                                                            if(param_value <= 2*old_value)
                                                            {
                                                                take_it=true;
                                                            }
                                                        }
                                                        else
                                                        {
                                                            if(old_value <= 2*param_value)
                                                            {
                                                                take_it=true;
                                                            }                                                            
                                                        }
                                                    }
                                                    if(!take_it)
                                                    {
                                                        param.m_Skipped =MC_PSK_DOUBLE_RANGE;
                                                    }
                                                    else
                                                    {
                                                        map_last_upgrade.insert(std::make_pair(param_name,(int)vParams->size()));                                                    
                                                    }
                                                }
                                            }
                                            else
                                            {
                                                param.m_Skipped = MC_PSK_OUT_OF_RANGE;
                                            }
                                        }
                                        else
                                        {
                                            if(param_size > 0)
                                            {
                                                param.m_Skipped = MC_PSK_WRONG_SIZE;
                                            }
                                            else
                                            {
                                                if(param_size < 0)
                                                {
                                                    param.m_Skipped = -param_size;
                                                }
                                                else
                                                {
                                                    param.m_Skipped = MC_PSK_NOT_SUPPORTED;
                                                }
                                            }
                                        }
                                        if(vUpgrades == NULL)                   // Called from MultichainNode_ApplyUpgrades
                                        {
                                            if((int)param.m_Block == current_height)
                                            {
                                                if(param.m_Skipped == MC_PSK_APPLIED)
                                                {
                                                    LogPrintf("PARAMETER UPGRADE: %s = %ld\n",param.m_Param->m_DisplayName,param.m_Value);
                                                }
                                            }
                                        }
                                        vParams->push_back(param);                                    
                                    }
                                    else
                                    {
                                        param.m_Skipped = MC_PSK_NOT_FOUND;
                                    }
                                }                                
                            }                        
                        }
                    }
                }
            }       
            upgrade.m_LastParam=(int)vParams->size();     
            if(vUpgrades)
            {
                vUpgrades->push_back(upgrade);
            }
        }
    }

    
    mc_gState->m_Permissions->FreePermissionList(upgrades);
    mc_gState->m_ProtocolVersionToUpgrade=NewProtocolVersion;
    
    return err;
}

bool ReplayMemPool(CTxMemPool& pool, int from,bool accept)
{
    int pos;
    uint256 hash;
    
    if(mc_gState->m_NetworkParams->IsProtocolMultichain() == 0)
    {
        for(pos=from;pos<pool.hashList->m_Count;pos++)
        {
            hash=*(uint256*)pool.hashList->GetRow(pos);
            if(pool.exists(hash))
            {
                if(IsTxBanned(hash))
                {
                    const CTransaction& tx = pool.mapTx[hash].GetTx();
                    string reason;
                    string removed_type="";
                    list<CTransaction> removed;
                    removed_type="banned";                                    
                    LogPrintf("mchn: Tx %s removed from the mempool (%s), reason: %s\n",tx.GetHash().ToString().c_str(),removed_type.c_str(),reason.c_str());
                    pool.remove(tx, removed, true, "replay: banned");                    
                }
            }
        }
        return true;
    }    
    
    int total_txs=pool.hashList->m_Count;
    
    LogPrint("mchn", "mchn: Replaying memory pool (%d new transactions, total %d)\n",total_txs-from,total_txs);
    mc_gState->m_Permissions->MempoolPermissionsCopy();
    
    for(pos=from;pos<pool.hashList->m_Count;pos++)
    {
        hash=*(uint256*)pool.hashList->GetRow(pos);
        if(pool.exists(hash))
        {
            const CTxMemPoolEntry entry=pool.mapTx[hash];
            const CTransaction& tx = entry.GetTx();            
            string removed_type="";
            string reason;
            list<CTransaction> removed;
            
            if(IsTxBanned(hash))
            {
                removed_type="banned";                                    
            }
            else
            {
                int permissions_from,permissions_to;
                permissions_from=mc_gState->m_Permissions->m_MempoolPermissions->GetCount();
                if(entry.FullReplayRequired())
                {
                    LOCK(pool.cs);
                    CCoinsView dummy;
                    CCoinsViewCache view(&dummy);
                    CCoinsViewMemPool viewMemPool(pcoinsTip, pool);
                    view.SetBackend(viewMemPool);
                    if(!AcceptMultiChainTransaction(tx,view,-1,accept,reason,NULL,NULL))
                    {
                        removed_type="rejected";                    
                    }
                }
                else
                {
                   if(mc_gState->m_Permissions->MempoolPermissionsCheck(entry.ReplayPermissionFrom(),entry.ReplayPermissionTo()) == 0) 
                   {
                        removed_type="rejected";                                               
                   }                        
                }
                if(removed_type.size() == 0)
                {
                    permissions_to=mc_gState->m_Permissions->m_MempoolPermissions->GetCount();
                    pool.mapTx[hash].SetReplayNodeParams(entry.FullReplayRequired(),permissions_from,permissions_to);                    
                }
            }

            if(removed_type.size())
            {
                LogPrintf("mchn: Tx %s removed from the mempool (%s), reason: %s\n",tx.GetHash().ToString().c_str(),removed_type.c_str(),reason.c_str());
                pool.remove(tx, removed, true, "replay: "+removed_type);                    
            }
            else
            {
                if(pwalletTxsMain->AddTx(NULL,tx,-1,NULL,-1,0))
                {
                    removed_type="error";
                    reason="wallet";
                    LogPrintf("mchn: Tx %s removed from the mempool (%s), reason: %s\n",tx.GetHash().ToString().c_str(),removed_type.c_str(),reason.c_str());
                    pool.remove(tx, removed, true, "replay: "+reason);                                        
                }
            }
        }
    }
    
    return true;
}

void FindSigner(CBlock *block,unsigned char *sig,int *sig_size,uint32_t *hash_type)
{
    int key_size;
    block->vSigner[0]=0;
    
    if(mc_gState->m_NetworkParams->IsProtocolMultichain())
    {
        for (unsigned int i = 0; i < block->vtx.size(); i++)
        {
            const CTransaction &tx = block->vtx[i];
            if (tx.IsCoinBase())
            {
                for (unsigned int j = 0; j < tx.vout.size(); j++)
                {
                    mc_gState->m_TmpScript1->Clear();

                    const CScript& script1 = tx.vout[j].scriptPubKey;        
                    CScript::const_iterator pc1 = script1.begin();

                    mc_gState->m_TmpScript1->SetScript((unsigned char*)(&pc1[0]),(size_t)(script1.end()-pc1),MC_SCR_TYPE_SCRIPTPUBKEY);

                    for (int e = 0; e < mc_gState->m_TmpScript1->GetNumElements(); e++)
                    {
                        if(block->vSigner[0] == 0)
                        {
                            mc_gState->m_TmpScript1->SetElement(e);                        
                            *sig_size=255;
                            key_size=255;    
                            if(mc_gState->m_TmpScript1->GetBlockSignature(sig,sig_size,hash_type,block->vSigner+1,&key_size) == 0)
                            {
                                block->vSigner[0]=(unsigned char)key_size;
                            }            
                        }
                    }
                }
            }
        }    
    }
}
    
bool VerifyBlockSignatureType(CBlock *block)
{
    unsigned char sig[255];
    int sig_size;
    uint32_t hash_type;
    
    FindSigner(block, sig, &sig_size, &hash_type);
    if(block->vSigner[0])
    {
        if(Params().DisallowUnsignedBlockNonce())
        {
            if(hash_type == BLOCKSIGHASH_NO_SIGNATURE_AND_NONCE)
            {
                LogPrintf("mchn: Nonce not covered by block signature\n");
                block->nSigHashType=BLOCKSIGHASH_INVALID;
                return false;                
            }
        }
        else
        {
            if(hash_type == BLOCKSIGHASH_NO_SIGNATURE)
            {
                LogPrintf("mchn: Nonce covered by block signature\n");
                block->nSigHashType=BLOCKSIGHASH_INVALID;
                return false;                
            }                
        }
    }
    return true;
}

bool VerifyBlockSignature(CBlock *block,bool force)
{
    unsigned char sig[255];
    int sig_size;//,key_size;
    uint32_t hash_type;
    uint256 hash_to_verify;
    uint256 original_merkle_root;
    uint32_t original_nonce;
    std::vector<unsigned char> vchSigOut;
    std::vector<unsigned char> vchPubKey;
    std::vector<uint256> savedMerkleTree;
    
    if(!force)
    {
        if(block->nMerkleTreeType != MERKLETREE_UNKNOWN)
        {
            if(block->nSigHashType == BLOCKSIGHASH_INVALID)
            {
                return false;
            }
            return true;
        }
    }
            
    block->nMerkleTreeType=MERKLETREE_FULL;
    block->nSigHashType=BLOCKSIGHASH_NONE;
    
    FindSigner(block, sig, &sig_size, &hash_type);
    if(block->vSigner[0])
    {
        switch(hash_type)
        {
            case BLOCKSIGHASH_HEADER:
                block->nMerkleTreeType=MERKLETREE_NO_COINBASE_OP_RETURN;
                block->nSigHashType=BLOCKSIGHASH_HEADER;
                hash_to_verify=block->GetHash();
                break;
            case BLOCKSIGHASH_NO_SIGNATURE_AND_NONCE:
            case BLOCKSIGHASH_NO_SIGNATURE:
                
                original_merkle_root=block->hashMerkleRoot;
                original_nonce=block->nNonce;
                
                block->nMerkleTreeType=MERKLETREE_NO_COINBASE_OP_RETURN;
                savedMerkleTree=block->vMerkleTree;
                block->hashMerkleRoot=block->BuildMerkleTree();
                if(hash_type == BLOCKSIGHASH_NO_SIGNATURE_AND_NONCE)
                {
                    block->nNonce=0;                    
                }
                hash_to_verify=block->GetHash();
                
                block->hashMerkleRoot=original_merkle_root;
                block->nNonce=original_nonce;
                
                block->nMerkleTreeType=MERKLETREE_FULL;                
                if(savedMerkleTree.size())
                {
                    block->vMerkleTree=savedMerkleTree;
                }
                else
                {
                    block->BuildMerkleTree();
                }
                break;
            default:
                LogPrintf("mchn: Invalid hash type received in block signature\n");
                block->nSigHashType=BLOCKSIGHASH_INVALID;
                return false;
        }
        
//        printf("V: %s -> %s\n",hash_to_verify.GetHex().c_str(),block->GetHash().GetHex().c_str());

        vchSigOut=std::vector<unsigned char> (sig, sig+sig_size);
        vchPubKey=std::vector<unsigned char> (block->vSigner+1, block->vSigner+1+block->vSigner[0]);

        CPubKey pubKeyOut(vchPubKey);
        if (!pubKeyOut.IsValid())
        {
            LogPrintf("mchn: Invalid pubkey received in block signature\n");
            block->nSigHashType=BLOCKSIGHASH_INVALID;
            return false;
        }
        if(!pubKeyOut.Verify(hash_to_verify,vchSigOut))
        {
            LogPrintf("mchn: Wrong block signature\n");
            block->nSigHashType=BLOCKSIGHASH_INVALID;
            return false;
        }        
    }
    else
    {
        if(mc_gState->m_NetworkParams->IsProtocolMultichain())
        {
            if(block->hashPrevBlock != uint256(0))
            {
                LogPrintf("mchn: Block signature not found\n");                
                block->nSigHashType=BLOCKSIGHASH_INVALID;
                return false;
            }
        }
    }
    
    return true;
}

/* MCHN END */

bool ReadTxFromDisk(CBlockIndex* pindex,int32_t offset,CTransaction& tx)
{
    CAutoFile file(OpenBlockFile(pindex->GetBlockPos(), true), SER_DISK, CLIENT_VERSION);
    if (file.IsNull())
    {
        LogPrintf("VerifyBlockMiner: Could not load block %s (height %d) from disk\n",pindex->GetBlockHash().ToString().c_str(),pindex->nHeight);
        return false;
    }
    try 
    {
        fseek(file.Get(), offset, SEEK_CUR);
        file >> tx;
    } 
    catch (std::exception &e) 
    {
        LogPrintf("VerifyBlockMiner: Could not deserialize tx at offset %d block %s (height %d) from disk\n",offset,pindex->GetBlockHash().ToString().c_str(),pindex->nHeight);
        return false;
    }

    return true;
}

bool VerifyBlockMiner(CBlock *block_in,CBlockIndex* pindexNew)
{
    if( (mc_gState->m_NetworkParams->IsProtocolMultichain() == 0) ||
        (mc_gState->m_NetworkParams->GetInt64Param("supportminerprecheck") == 0) ||
        (MCP_ANYONE_CAN_MINE) )                               
    {
        pindexNew->fPassedMinerPrecheck=true;
        return true;
    }
    
    if(pindexNew->pprev == NULL)
    {
        pindexNew->fPassedMinerPrecheck=true;
        return true;        
    }
    
    bool fReject=false;
    const CBlockIndex *pindexFork = chainActive.FindFork(pindexNew);
    
    if(pindexFork == pindexNew)
    {        
        pindexNew->fPassedMinerPrecheck=true;
        return true;        
    }
    
    CBlockIndex *pindex;
    const CBlock *pblock;
    CBlock branch_block;
    vector <CBlockIndex *> branch;
    vector <uint160> miners;
    int pos,branch_size,record,i;
    uint32_t last_after_fork;
    bool fVerify;
    bool fRolledBack;
    uint32_t admin_miner_count;
    int32_t offsets[MC_PLS_SIZE_OFFSETS_PER_ROW];
    uint256 block_hash;
    vector<unsigned char> vchPubKey;
    
    fRolledBack=false;
    branch_size=pindexNew->nHeight-pindexFork->nHeight;
    branch.resize(branch_size);
    miners.resize(branch_size);
        
    pos=branch_size-1;
    pindex=pindexNew;

    CBlock last_block;
    CBlock *pblock_last;
    pblock_last=block_in;
    if(block_in == NULL)
    {
        if ( ((pindexNew->nStatus & BLOCK_HAVE_DATA) == 0 ) || !ReadBlockFromDisk(last_block, pindexNew) )
        {
            LogPrintf("VerifyBlockMiner: Block %s (height %d) miner verification skipped - block not found\n",pindexNew->GetBlockHash().ToString().c_str(),pindexNew->nHeight);        
            fReject=true;                                              
        }       
        pblock_last=&last_block;
    }

    vchPubKey=vector<unsigned char> (pblock_last->vSigner+1, pblock_last->vSigner+1+pblock_last->vSigner[0]);
    CPubKey pubKeyNew(vchPubKey);
    CKeyID pubKeyHashNew=pubKeyNew.GetID();
    miners[pos]=*(uint160*)(pubKeyHashNew.begin());      

    if(fReject)
    {
        fReject=false;
        goto exitlbl;                    
    }
    
    LogPrint("mchn","VerifyBlockMiner: Block: %d, Fork: %d, Chain: %d\n",pindexNew->nHeight,pindexFork->nHeight,mc_gState->m_Permissions->m_Block);
    
    while(pindex != pindexFork)
    {
        if(pindex->nStatus & BLOCK_FAILED_MASK)
        {
            LogPrintf("VerifyBlockMiner: Invalid block %s (height %d)\n",pindex->GetBlockHash().ToString().c_str(),pindex->nHeight);
            fReject=true;
            goto exitlbl;            
        }
        
        branch[pos]=pindex;
        pos--;
        pindex=pindex->pprev;
    }

    last_after_fork=0;
    fRolledBack=true;
    mc_gState->m_Permissions->RollBackBeforeMinerVerification(pindexFork->nHeight);
    LogPrint("mchn","VerifyBlockMiner: Rolled back to block %d\n",mc_gState->m_Permissions->m_Block);

    for(pos=0;pos<branch_size;pos++)
    {
        LogPrint("mchn","VerifyBlockMiner: Verifying block %d\n",mc_gState->m_Permissions->m_Block+1);
        pindex=branch[pos];
        block_hash=pindex->GetBlockHash();
        fVerify=false;
        pblock=NULL;
        if(pindex == pindexNew)
        {
            fVerify=true;
            pblock=pblock_last;            
        }
        if(!fVerify && (mc_gState->m_Permissions->GetBlockMiner(&block_hash,(unsigned char*)&miners[pos],&admin_miner_count) == MC_ERR_NOERROR) )
        {
            LogPrint("mchn","VerifyBlockMiner: Verified block %s (height %d)\n",pindex->GetBlockHash().ToString().c_str(),pindex->nHeight);
            if(miners[pos] == miners[branch_size - 1])
            {
                last_after_fork=pindex->nHeight;
            }
            record=1;
            while(mc_gState->m_Permissions->GetBlockAdminMinerGrants(&block_hash,record,offsets) == MC_ERR_NOERROR)
            {
                for(i=0;i<MC_PLS_SIZE_OFFSETS_PER_ROW;i++)
                {
                    if(offsets[i])
                    {
                        CTransaction tx;
                        string reason;
                        if(!ReadTxFromDisk(pindex,offsets[i],tx))
                        {
                            fReject=true;
                            goto exitlbl;                            
                        }
                        LogPrint("mchn","VerifyBlockMiner: Grant tx %s in block %s (height %d)\n",tx.GetHash().ToString().c_str(),pindex->GetBlockHash().ToString().c_str(),pindex->nHeight);
                        if(!AcceptAdminMinerPermissions(tx,offsets[i],false,reason,NULL))
                        {
                            LogPrintf("VerifyBlockMiner: tx %s: %s\n",tx.GetHash().ToString().c_str(),reason.c_str());
                            fReject=true;
                            goto exitlbl;                            
                        }
                    }
                }
                record++;
            }
            mc_gState->m_Permissions->IncrementBlock(admin_miner_count);
        }
        else
        {
            if(pblock == NULL)
            {
                LogPrint("mchn","VerifyBlockMiner: Unverified block %s (height %d)\n",pindex->GetBlockHash().ToString().c_str(),pindex->nHeight);
                if ( ((pindex->nStatus & BLOCK_HAVE_DATA) == 0 ) || !ReadBlockFromDisk(branch_block, pindex) )
                {
                    LogPrintf("VerifyBlockMiner: Could not load block %s (height %d) from disk\n",pindex->GetBlockHash().ToString().c_str(),pindex->nHeight);
                    LogPrintf("VerifyBlockMiner: Block %s (height %d) miner verification skipped\n",pindexNew->GetBlockHash().ToString().c_str(),pindexNew->nHeight);        
                    fReject=false;                                              // We cannot neglect subsequent blocks, but it is unverified
                    goto exitlbl;                    
                }
                pblock = &branch_block;                
                vchPubKey=vector<unsigned char> (branch_block.vSigner+1, branch_block.vSigner+1+branch_block.vSigner[0]);
                CPubKey pubKey(vchPubKey);
                CKeyID pubKeyHash=pubKey.GetID();
                miners[pos]=*(uint160*)(pubKeyHash.begin());      
                if(miners[pos] == miners[branch_size - 1])
                {
                    last_after_fork=pindex->nHeight;
                }
                if(mc_gState->m_Permissions->CanMineBlockOnFork(&miners[pos],pindex->nHeight,last_after_fork) == 0)
                {
                    LogPrintf("VerifyBlockMiner: Permission denied for miner %s received in block signature\n",CBitcoinAddress(pubKeyHash).ToString().c_str());
                    fReject=true;
                    goto exitlbl;                    
                }
            }
            else
            {
                LogPrint("mchn","VerifyBlockMiner: New block %s (height %d)\n",pindex->GetBlockHash().ToString().c_str(),pindex->nHeight);
                if(mc_gState->m_Permissions->CanMineBlockOnFork(&miners[pos],pindex->nHeight,last_after_fork) == 0)
                {
                    LogPrintf("VerifyBlockMiner: Permission denied for miner %s received in block signature\n",CBitcoinAddress(pubKeyHashNew).ToString().c_str());
                    fReject=true;
                    goto exitlbl;                    
                }
            }
            int off=80+GetSizeOfCompactSize(pblock->vtx.size());
            for (unsigned int i = 0; i < pblock->vtx.size(); i++)
            {
                const CTransaction &tx = pblock->vtx[i];
                string reason;
                uint32_t result;
                int mempool_size=mc_gState->m_Permissions->m_MemPool->GetCount();
                if(!AcceptAdminMinerPermissions(tx,off,true,reason,&result))
                {
                    LogPrintf("VerifyBlockMiner: tx %s: %s\n",tx.GetHash().ToString().c_str(),reason.c_str());
                    fReject=true;
                    goto exitlbl;                            
                }
                if(mempool_size != mc_gState->m_Permissions->m_MemPool->GetCount())
                {
                    LogPrint("mchn","VerifyBlockMiner: Grant tx %s in block %s (height %d)\n",tx.GetHash().ToString().c_str(),pindex->GetBlockHash().ToString().c_str(),pindex->nHeight);
                }
                
                off+=tx.GetSerializeSize(SER_NETWORK,tx.nVersion);
            }
            mc_gState->m_Permissions->StoreBlockInfo(&miners[pos],&block_hash);
        }
        pindex->fPassedMinerPrecheck=true;
    }
    
exitlbl:

    if(fRolledBack)
    {
        LogPrint("mchn","VerifyBlockMiner: Restoring chain, block %d\n",mc_gState->m_Permissions->m_Block);
        mc_gState->m_Permissions->RestoreAfterMinerVerification();
        LogPrint("mchn","VerifyBlockMiner: Restored on block %d\n",mc_gState->m_Permissions->m_Block);
    }

    if(fReject)
    {
        LogPrintf("VerifyBlockMiner: Block %s (height %d) miner verification failed\n",pindexNew->GetBlockHash().ToString().c_str(),pindexNew->nHeight);        
    }

    return !fReject;
}


bool CheckBlockPermissions(const CBlock& block,CBlockIndex* prev_block,unsigned char *lpMinerAddress)
{
    bool checked=true;
    const unsigned char *genesis_key;
    int key_size;
    vector<unsigned char> vchSigOut;
    vector<unsigned char> vchPubKey;
    
    LogPrint("mchn","mchn: Checking block signature and miner permissions...\n");
            
    key_size=255;    
    
    if(mc_gState->m_NetworkParams->IsProtocolMultichain() == 0)
    {
        return true;
    }
    
    mc_gState->m_Permissions->CopyMemPool();
    mc_gState->m_Permissions->ClearMemPool();
    
    if(checked)
    {
        if(prev_block)
        {
            if((block.nSigHashType == BLOCKSIGHASH_UNKNOWN) || (block.nSigHashType == BLOCKSIGHASH_INVALID))
            {
                checked=false;
            }        
            else
            {
                vchPubKey=vector<unsigned char> (block.vSigner+1, block.vSigner+1+block.vSigner[0]);

                CPubKey pubKeyOut(vchPubKey);
                if (!pubKeyOut.IsValid())
                {
                    LogPrintf("mchn: Invalid pubkey received in block signature\n");
                    checked = false;
                }
                if(checked)
                {    
                    CKeyID pubKeyHash=pubKeyOut.GetID();
                    memcpy(lpMinerAddress,pubKeyHash.begin(),20);
                    if(!mc_gState->m_Permissions->CanMine(NULL,pubKeyHash.begin()))
                    {
                //                mc_DumpSize("Connection address",pubKeyHash.begin(),20,20);
                        LogPrintf("mchn: Permission denied for miner %s received in block signature\n",CBitcoinAddress(pubKeyHash).ToString().c_str());
                        checked = false;
                    }
                }                
            }
        }
        else
        {
            genesis_key=(unsigned char*)mc_gState->m_NetworkParams->GetParam("genesispubkey",&key_size);
            vchPubKey=vector<unsigned char> (genesis_key, genesis_key+key_size);

            CPubKey pubKeyOut(vchPubKey);
            if (!pubKeyOut.IsValid())
            {
                LogPrintf("mchn: Invalid pubkey received in block signature\n");
                checked = false;
            }
            if(checked)
            {    
                CKeyID pubKeyHash=pubKeyOut.GetID();
                memcpy(lpMinerAddress,pubKeyHash.begin(),20);
            }        
        }
    }
 
    mc_gState->m_Permissions->RestoreMemPool();
    
    return checked;
}

