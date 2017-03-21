// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2017 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "core/main.h"
#include "utils/util.h"
#include "multichain/multichain.h"
#include "wallet/wallettxs.h"

extern mc_WalletTxs* pwalletTxsMain;


using namespace std;

bool AcceptMultiChainTransaction(const CTransaction& tx, 
                                 const CCoinsViewCache &inputs,
                                 int offset,
                                 bool accept,
                                 string& reason,
                                 bool *replay);
bool AcceptAssetTransfers(const CTransaction& tx, const CCoinsViewCache &inputs, string& reason);
bool AcceptAssetGenesis(const CTransaction &tx,int offset,bool accept,string& reason);
bool AcceptPermissionsAndCheckForDust(const CTransaction &tx,bool accept,string& reason);
bool IsTxBanned(uint256 txid);


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
                    pool.remove(tx, removed, true, "replay");                    
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
                
                if(mc_gState->m_Features->Streams())
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
                        if(!AcceptMultiChainTransaction(tx,view,-1,accept,reason,NULL))
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
                else
                {                
                    if(removed_type.size() == 0)
                    {
                        if(!AcceptPermissionsAndCheckForDust(tx,accept,reason))
                        {
                            removed_type="permissions";
                        }
                    }
                    if(removed_type.size() == 0)
                    {
                        if(!AcceptAssetGenesis(tx,-1,true,reason))
                        {
                            removed_type="issue";
                        }        
                    }
                    if(removed_type.size() == 0)
                    {
                        LOCK(pool.cs);
                        CCoinsView dummy;
                        CCoinsViewCache view(&dummy);
                        CCoinsViewMemPool viewMemPool(pcoinsTip, pool);
                        view.SetBackend(viewMemPool);
                        if(!AcceptAssetTransfers(tx, view, reason))
                        {
                            removed_type="transfer";
                        }
                    }            
                }
            }

            if(removed_type.size())
            {
                LogPrintf("mchn: Tx %s removed from the mempool (%s), reason: %s\n",tx.GetHash().ToString().c_str(),removed_type.c_str(),reason.c_str());
                pool.remove(tx, removed, true, "replay");                    
            }
            else
            {
                pwalletTxsMain->AddTx(NULL,tx,-1,NULL,-1,0);            
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
/*    
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
                    mc_gState->m_TmpScript->Clear();

                    const CScript& script1 = tx.vout[j].scriptPubKey;        
                    CScript::const_iterator pc1 = script1.begin();

                    mc_gState->m_TmpScript->SetScript((unsigned char*)(&pc1[0]),(size_t)(script1.end()-pc1),MC_SCR_TYPE_SCRIPTPUBKEY);

                    for (int e = 0; e < mc_gState->m_TmpScript->GetNumElements(); e++)
                    {
                        if(block->vSigner[0] == 0)
                        {
                            mc_gState->m_TmpScript->SetElement(e);                        
                            sig_size=255;
                            key_size=255;    
                            if(mc_gState->m_TmpScript->GetBlockSignature(sig,&sig_size,&hash_type,block->vSigner+1,&key_size) == 0)
                            {
                                block->vSigner[0]=(unsigned char)key_size;
                            }            
                        }
                    }
                }
            }
        }    
    }
*/    
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
                
                original_merkle_root=block->hashMerkleRoot;
                original_nonce=block->nNonce;
                
                block->nMerkleTreeType=MERKLETREE_NO_COINBASE_OP_RETURN;
                block->hashMerkleRoot=block->BuildMerkleTree();
                block->nNonce=0;
                hash_to_verify=block->GetHash();
                
                block->hashMerkleRoot=original_merkle_root;
                block->nNonce=original_nonce;
                
                block->nMerkleTreeType=MERKLETREE_FULL;                
                block->BuildMerkleTree();
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
    
    if(mc_gState->m_Features->UnconfirmedMinersCannotMine() == 0)
    {    

        for (unsigned int i = 0; i < block.vtx.size(); i++)
        {
            if(checked)
            {
                const CTransaction &tx = block.vtx[i];
                if (!tx.IsCoinBase())
                {
                    string reason;
                    if(!AcceptPermissionsAndCheckForDust(tx,true,reason))
                    {
                        checked=false;
                    }
                }    
            }
        }
    }

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

