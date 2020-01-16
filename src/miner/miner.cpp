// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "miner/miner.h"

#include "structs/amount.h"
#include "primitives/block.h"
#include "primitives/transaction.h"
#include "structs/hash.h"
#include "core/main.h"
#include "net/net.h"
#include "structs/base58.h"
#include "chain/pow.h"
#include "utils/timedata.h"
#include "utils/util.h"
#include "utils/utilmoneystr.h"
#ifdef ENABLE_WALLET
#include "wallet/wallet.h"
#endif

#include "multichain/multichain.h"

#include <boost/thread.hpp>
#include <boost/tuple/tuple.hpp>

using namespace std;

bool CanMineWithLockedBlock();
bool IsTxBanned(uint256 txid);
int LastForkedHeight();


//////////////////////////////////////////////////////////////////////////////
//
// BitcoinMiner
//

//
// Unconfirmed transactions in the memory pool often depend on other
// transactions in the memory pool. When we select transactions from the
// pool, we select by highest priority or fee rate, so we might consider
// transactions that depend on transactions that aren't yet in the block.
// The COrphan class keeps track of these 'temporary orphans' while
// CreateBlock is figuring out which transactions to include.
//
class COrphan
{
public:
    const CTransaction* ptx;
    set<uint256> setDependsOn;
    CFeeRate feeRate;
    double dPriority;

    COrphan(const CTransaction* ptxIn) : ptx(ptxIn), feeRate(0), dPriority(0)
    {
    }
};

uint64_t nLastBlockTx = 0;
uint64_t nLastBlockSize = 0;

// We want to sort transactions by priority and fee rate, so:
typedef boost::tuple<double, CFeeRate, const CTransaction*> TxPriority;
class TxPriorityCompare
{
    bool byFee;

public:
    TxPriorityCompare(bool _byFee) : byFee(_byFee) { }

    bool operator()(const TxPriority& a, const TxPriority& b)
    {
        if (byFee)
        {
            if (a.get<1>() == b.get<1>())
                return a.get<0>() < b.get<0>();
            return a.get<1>() < b.get<1>();
        }
        else
        {
            if (a.get<0>() == b.get<0>())
                return a.get<1>() < b.get<1>();
            return a.get<0>() < b.get<0>();
        }
    }
};

bool UpdateTime(CBlockHeader* pblock, const CBlockIndex* pindexPrev)
{
/* MCHN START */    
    uint32_t original_nTime=pblock->nTime;
    uint32_t original_nBits=pblock->nBits;
/* MCHN END */    
    pblock->nTime = std::max(pindexPrev->GetMedianTimePast()+1, GetAdjustedTime());

    // Updating time can change work required on testnet:
    if (Params().AllowMinDifficultyBlocks())
        pblock->nBits = GetNextWorkRequired(pindexPrev, pblock);
/* MCHN START */    
    if((original_nTime != pblock->nTime) || (original_nBits != pblock->nBits))
    {
        return true;
    }
    return false;
/* MCHN END */    
}

/* MCHN START */

bool CreateBlockSignature(CBlock *block,uint32_t hash_type,CWallet *pwallet,uint256 *cachedMerkleRoot)
{
    if(Params().DisallowUnsignedBlockNonce())
    {
        if(hash_type != BLOCKSIGHASH_NO_SIGNATURE)
        {
            return true;
        }
    }
    else
    {
        if(hash_type != BLOCKSIGHASH_NO_SIGNATURE_AND_NONCE)
        {
            return true;
        } 
    }
    
    int coinbase_tx,op_return_output;
    uint256 hash_to_verify;
    vector<uint256> cachedMerkleBranch;
    
    cachedMerkleBranch.clear();
    
    std::vector<unsigned char> vchSigOut;
    std::vector<unsigned char> vchPubKey;
    
    block->nMerkleTreeType=MERKLETREE_FULL;
    block->nSigHashType=BLOCKSIGHASH_NONE;
    
    
    if(!mc_gState->m_NetworkParams->IsProtocolMultichain())
    {
        block->hashMerkleRoot=block->BuildMerkleTree();
        return true;
    }
    
    if(block->vSigner[0] == 0)
    {
        return false;
    }
    
    coinbase_tx=-1;
    op_return_output=-1;
    for (unsigned int i = 0; i < block->vtx.size(); i++)
    {
        if(coinbase_tx<0)
        {
            const CTransaction &tx = block->vtx[i];
            if (block->vtx[i].IsCoinBase())
            {
                coinbase_tx=i;
                for (unsigned int j = 0; j < tx.vout.size(); j++)
                {

                    const CScript& script1 = tx.vout[j].scriptPubKey;        
                    if(script1.IsUnspendable())
                    {
                        op_return_output=j;                        
                    }
                }
            }
        }
    }
    
    if(coinbase_tx<0)
    {
        block->nSigHashType=BLOCKSIGHASH_INVALID;
        return false;
    }
    
    if((hash_type == BLOCKSIGHASH_HEADER) && (op_return_output >= 0))
    {
        block->nSigHashType=BLOCKSIGHASH_INVALID;
        return false;
    }
//    if(op_return_output >= 0)
    {
        CMutableTransaction tx=block->vtx[coinbase_tx];
        tx.vout.clear();
        for(int i=0;i<(int)block->vtx[coinbase_tx].vout.size();i++)
        {
            if((i != op_return_output) && 
               ((block->vtx[coinbase_tx].vout[i].nValue != 0) || (mc_gState->m_Permissions->m_Block == 0)))    
            {
                tx.vout.push_back(block->vtx[coinbase_tx].vout[i]);
            }
        }        
        block->vtx[coinbase_tx]=tx;
    }    
        
    switch(hash_type)
    {
        case BLOCKSIGHASH_HEADER:
            block->nMerkleTreeType=MERKLETREE_NO_COINBASE_OP_RETURN;
            block->nSigHashType=BLOCKSIGHASH_HEADER;
            hash_to_verify=block->GetHash();
            break;
        case BLOCKSIGHASH_NO_SIGNATURE_AND_NONCE:
        case BLOCKSIGHASH_NO_SIGNATURE:
            block->nMerkleTreeType=MERKLETREE_NO_COINBASE_OP_RETURN;
            if(hash_type == BLOCKSIGHASH_NO_SIGNATURE_AND_NONCE)
            {
                block->hashMerkleRoot=block->BuildMerkleTree();
                block->nNonce=0;
            }
            else
            {
                if(*cachedMerkleRoot != 0)
                {
                    block->hashMerkleRoot=*cachedMerkleRoot;
                }
                else
                {
                    block->hashMerkleRoot=block->BuildMerkleTree();
                    *cachedMerkleRoot=block->hashMerkleRoot;
                }
            }
            hash_to_verify=block->GetHash();
            block->nMerkleTreeType=MERKLETREE_FULL;                
            break;
        default:
            block->nSigHashType=BLOCKSIGHASH_INVALID;
            return false;
    }
    
    CMutableTransaction tx=block->vtx[coinbase_tx];
    tx.vout.clear();
    for(int i=0;i<(int)block->vtx[coinbase_tx].vout.size();i++)
    {
        tx.vout.push_back(block->vtx[coinbase_tx].vout[i]);
    }        
    
    CTxOut txOut;
    
    txOut.nValue = 0;
    txOut.scriptPubKey = CScript() << OP_RETURN;

    size_t elem_size;
    const unsigned char *elem;

    vchPubKey=std::vector<unsigned char> (block->vSigner+1, block->vSigner+1+block->vSigner[0]);

    CPubKey pubKeyOut(vchPubKey);
    CKey key;
    if(!pwallet->GetKey(pubKeyOut.GetID(), key))
    {
        return false;
    }
    
    vector<unsigned char> vchSig;
    key.Sign(hash_to_verify, vchSig);
    
    mc_Script *lpScript;
    lpScript=new mc_Script;
    
    lpScript->SetBlockSignature(vchSig.data(),vchSig.size(),hash_type,block->vSigner+1,block->vSigner[0]);

    for(int element=0;element < lpScript->GetNumElements();element++)
    {
        elem = lpScript->GetData(element,&elem_size);
        if(elem)
        {
            txOut.scriptPubKey << vector<unsigned char>(elem, elem + elem_size);
        }
    }
    delete lpScript;
    
    tx.vout.push_back(txOut);
        
    block->vtx[coinbase_tx]=tx;
    
    switch(hash_type)
    {
        case BLOCKSIGHASH_NO_SIGNATURE_AND_NONCE:
            block->hashMerkleRoot=block->BuildMerkleTree();
            break;
        case BLOCKSIGHASH_NO_SIGNATURE:
            block->hashMerkleRoot=block->CheckMerkleBranch(tx.GetHash(),block->GetMerkleBranch(0),0);
            break;
    }

    return true;
}

/* MCHN END */    


/* MCHN START */    
//CBlockTemplate* CreateNewBlock(const CScript& scriptPubKeyIn)
CBlockTemplate* CreateNewBlock(const CScript& scriptPubKeyIn,CWallet *pwallet,CPubKey *ppubkey,int *canMine,CBlockIndex** ppPrev)
/* MCHN END */    
{
    // Create new block
    auto_ptr<CBlockTemplate> pblocktemplate(new CBlockTemplate());
    if(!pblocktemplate.get())
        return NULL;
    CBlock *pblock = &pblocktemplate->block; // pointer for convenience
    // -regtest only: allow overriding block.nVersion with
    // -blockversion=N to test forking scenarios
    if (Params().MineBlocksOnDemand())
        pblock->nVersion = GetArg("-blockversion", pblock->nVersion);

    // Create coinbase tx
    CMutableTransaction txNew;
    txNew.vin.resize(1);
    txNew.vin[0].prevout.SetNull();
    
/* MCHN START */    

    txNew.vout.resize(1);

    int prevCanMine=MC_PTP_MINE;
    if(canMine)
    {
        prevCanMine=*canMine;
    }
    
/* MCHN END */    
    
    txNew.vout[0].scriptPubKey = scriptPubKeyIn;

    // Add dummy coinbase tx as first transaction
    pblock->vtx.push_back(CTransaction());
    pblocktemplate->vTxFees.push_back(-1); // updated at end
    pblocktemplate->vTxSigOps.push_back(-1); // updated at end

    // Largest block you're willing to create:
    unsigned int nBlockMaxSize = GetArg("-blockmaxsize", DEFAULT_BLOCK_MAX_SIZE);
    // Limit to betweeen 1K and MAX_BLOCK_SIZE-1K for sanity:
    nBlockMaxSize = std::max((unsigned int)1000, std::min((unsigned int)(MAX_BLOCK_SIZE-1000), nBlockMaxSize));

    // How much of the block should be dedicated to high-priority transactions,
    // included regardless of the fees they pay
    // unsigned int nBlockPrioritySize = GetArg("-blockprioritysize", DEFAULT_BLOCK_PRIORITY_SIZE);
    // nBlockPrioritySize = std::min(nBlockMaxSize, nBlockPrioritySize);

    // Minimum block size you want to create; block will be filled with free transactions
    // until there are no more or the block reaches this size:
    unsigned int nBlockMinSize = GetArg("-blockminsize", DEFAULT_BLOCK_MIN_SIZE);
    nBlockMinSize = std::min(nBlockMaxSize, nBlockMinSize);

    // Collect memory pool transactions into the block
    CAmount nFees = 0;
    bool fPreservedMempoolOrder=true;

    {
        LOCK2(cs_main, mempool.cs);
                
        CBlockIndex* pindexPrev = chainActive.Tip();
        const int nHeight = pindexPrev->nHeight + 1;
        if(ppPrev)
        {
            *ppPrev=pindexPrev;
        }
        CCoinsViewCache view(pcoinsTip);

        // Priority order to process transactions
        list<COrphan> vOrphan; // list memory doesn't move
        map<uint256, vector<COrphan*> > mapDependers;
        bool fPrintPriority = GetBoolArg("-printpriority", false);

        // This vector will be sorted into a priority queue:
        vector<TxPriority> vecPriority;
        vecPriority.reserve(mempool.mapTx.size());
/* MCHN START */        
// mempool records are processed in the order they were accepted       
        
        set <uint256> setAdded;
/*        
        for (map<uint256, CTxMemPoolEntry>::iterator mi = mempool.mapTx.begin();
             mi != mempool.mapTx.end(); ++mi)
        {
            const CTransaction& tx = mi->second.GetTx();
  */          
            
        double orderPriority=mempool.mapTx.size();
        
        mempool.defragmentHashList();    
        for(int pos=0;pos<mempool.hashList->m_Count;pos++)
        {
            uint256 hash;
            hash=*(uint256*)mempool.hashList->GetRow(pos);
            
            if(!mempool.exists(hash))
            {
                LogPrint("mchn","mchn-miner: Tx not found in the mempool: %s\n",hash.GetHex().c_str());
                fPreservedMempoolOrder=false;
                continue;
            }
            if(IsTxBanned(hash))
            {
                LogPrint("mchn","mchn-miner: Banned Tx: %s\n",hash.GetHex().c_str());
                fPreservedMempoolOrder=false;
                continue;                
            }
            
            const CTransaction& tx = mempool.mapTx[hash].GetTx();
/* MCHN END */        
            
            if (tx.IsCoinBase() || !IsFinalTx(tx, nHeight))
            {
                LogPrint("mchn","mchn-miner: Coinbase or not final tx found: %s\n",tx.GetHash().GetHex().c_str());
                fPreservedMempoolOrder=false;
                continue;
            }
            
            COrphan* porphan = NULL;
            double dPriority = 0;
            CAmount nTotalIn = 0;
            bool fMissingInputs = false;
            BOOST_FOREACH(const CTxIn& txin, tx.vin)
            {
                // Read prev transaction
                if (!view.HaveCoins(txin.prevout.hash))
                {
                    // This should never happen; all transactions in the memory
                    // pool should connect to either transactions in the chain
                    // or other transactions in the memory pool.
                    if (!mempool.mapTx.count(txin.prevout.hash))
                    {
                        LogPrintf("ERROR: mempool transaction missing input\n");
                        if (fDebug) assert("mempool transaction missing input" == 0);
                        fMissingInputs = true;
                        if (porphan)
                            vOrphan.pop_back();
                        break;
                    }

                    // Has to wait for dependencies
/* MCHN START */                    
                    if(setAdded.count(txin.prevout.hash) == 0)
                    {
/* MCHN END */                    
                        if (!porphan)
                        {
                            // Use list for automatic deletion
                            vOrphan.push_back(COrphan(&tx));
                            porphan = &vOrphan.back();
                        }
                        mapDependers[txin.prevout.hash].push_back(porphan);
                        porphan->setDependsOn.insert(txin.prevout.hash);
/* MCHN START */                    
                    }
/* MCHN END */                    
                    nTotalIn += mempool.mapTx[txin.prevout.hash].GetTx().vout[txin.prevout.n].nValue;
                    continue;
                }
                const CCoins* coins = view.AccessCoins(txin.prevout.hash);
                assert(coins);

                CAmount nValueIn = coins->vout[txin.prevout.n].nValue;
                nTotalIn += nValueIn;

                int nConf = nHeight - coins->nHeight;

                dPriority += (double)nValueIn * nConf;
            }
            if (fMissingInputs)
            {
                LogPrint("mchn","mchn-miner: Missing inputs for %s\n",tx.GetHash().GetHex().c_str());
                fPreservedMempoolOrder=false;
                continue;
            }
            
            // Priority is sum(valuein * age) / modified_txsize
            unsigned int nTxSize = ::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION);
            dPriority = tx.ComputePriority(dPriority, nTxSize);

/* MCHN START */            
// Priority ignored - txs are processed in the order they were accepted
            dPriority=orderPriority;
            orderPriority-=1.;            
            
//            uint256 hash = tx.GetHash();
/* MCHN END */                        
            mempool.ApplyDeltas(hash, dPriority, nTotalIn);

            CFeeRate feeRate(nTotalIn-tx.GetValueOut(), nTxSize);
/* MCHN START */
            
/* MCHN END */            
            if (porphan)
            {
                LogPrint("mchn","mchn-miner: Orphan %s\n",tx.GetHash().GetHex().c_str());
                porphan->dPriority = dPriority;
                porphan->feeRate = feeRate;
                fPreservedMempoolOrder=false;
            }
            else
/* MCHN START */            
            {
                setAdded.insert(tx.GetHash());
                vecPriority.push_back(TxPriority(dPriority, feeRate, &tx));
            }
//                vecPriority.push_back(TxPriority(dPriority, feeRate, &mi->second.GetTx()));
/* MCHN END */            
        }

        // Collect transactions into block
        uint64_t nBlockSize = 1000;
        uint64_t nBlockTx = 0;
        int nBlockSigOps = 40;
//        bool fSortedByFee = (nBlockPrioritySize <= 0);

/* MCHN START */            
        TxPriorityCompare comparer(false);
//        TxPriorityCompare comparer(fSortedByFee);
        bool overblocksize_logged=false;
/* MCHN END */            
        std::make_heap(vecPriority.begin(), vecPriority.end(), comparer);

        while (!vecPriority.empty())
        {
            // Take highest priority transaction off the priority queue:
            double dPriority = vecPriority.front().get<0>();
            CFeeRate feeRate = vecPriority.front().get<1>();
            const CTransaction& tx = *(vecPriority.front().get<2>());

            std::pop_heap(vecPriority.begin(), vecPriority.end(), comparer);
            vecPriority.pop_back();

            // Size limits
            unsigned int nTxSize = ::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION);
            if (nBlockSize + nTxSize >= nBlockMaxSize)
            {
                if(!overblocksize_logged)
                {
                    overblocksize_logged=true;
                    LogPrint("mchn","mchn-miner: Over block size: %s\n",tx.GetHash().GetHex().c_str());
                }
                continue;
            }
            // Legacy limits on sigOps:
            unsigned int nTxSigOps = GetLegacySigOpCount(tx);
            if (nBlockSigOps + nTxSigOps >= MAX_BLOCK_SIGOPS)
            {
                LogPrint("mchn","mchn-miner: Over sigop count 1: %s\n",tx.GetHash().GetHex().c_str());
                continue;
            }

            // Skip free transactions if we're past the minimum block size:
            const uint256& hash = tx.GetHash();
            double dPriorityDelta = 0;
            CAmount nFeeDelta = 0;
            mempool.ApplyDeltas(hash, dPriorityDelta, nFeeDelta);
/* MCHN             
            if (fSortedByFee && (dPriorityDelta <= 0) && (nFeeDelta <= 0) && (feeRate < ::minRelayTxFee) && (nBlockSize + nTxSize >= nBlockMinSize))
                continue;

            // Prioritise by fee once past the priority size or we run out of high-priority
            // transactions:
            if (!fSortedByFee &&
                ((nBlockSize + nTxSize >= nBlockPrioritySize) || !AllowFree(dPriority)))
            {
                fSortedByFee = true;
                comparer = TxPriorityCompare(fSortedByFee);
                std::make_heap(vecPriority.begin(), vecPriority.end(), comparer);
            }
*/
            if (!view.HaveInputs(tx))
            {
                LogPrint("mchn","mchn-miner: No inputs for %s\n",tx.GetHash().GetHex().c_str());
                continue;
            }

            CAmount nTxFees = view.GetValueIn(tx)-tx.GetValueOut();

            nTxSigOps += GetP2SHSigOpCount(tx, view);
            if (nBlockSigOps + nTxSigOps >= MAX_BLOCK_SIGOPS)
            {
                LogPrint("mchn","mchn-miner: Over sigop count 2: %s\n",tx.GetHash().GetHex().c_str());
                continue;
            }

            // Note that flags: we don't want to set mempool/IsStandard()
            // policy here, but we still have to ensure that the block we
            // create only contains transactions that are valid in new blocks.
            CValidationState state;
            
            if(!fPreservedMempoolOrder)
            {
/* MCHN START */            
    //            if (!CheckInputs(tx, state, view, true, MANDATORY_SCRIPT_VERIFY_FLAGS, true))// May fail if send permission was lost
                if (!CheckInputs(tx, state, view, false, 0, true))    
/* MCHN END */            
                {
                    LogPrint("mchn","mchn-miner: CheckInput failure %s\n",tx.GetHash().GetHex().c_str());
                    continue;
                }

            }
            CTxUndo txundo;
            UpdateCoins(tx, state, view, txundo, nHeight);

            // Added
            pblock->vtx.push_back(tx);
            pblocktemplate->vTxFees.push_back(nTxFees);
            pblocktemplate->vTxSigOps.push_back(nTxSigOps);
            nBlockSize += nTxSize;
            ++nBlockTx;
            nBlockSigOps += nTxSigOps;
            nFees += nTxFees;

            if (fPrintPriority)
            {
                LogPrintf("priority %.1f fee %s txid %s\n",
                    dPriority, feeRate.ToString(), tx.GetHash().ToString());
            }

            // Add transactions that depend on this one to the priority queue
            if (mapDependers.count(hash))
            {
                BOOST_FOREACH(COrphan* porphan, mapDependers[hash])
                {
                    if (!porphan->setDependsOn.empty())
                    {
                        porphan->setDependsOn.erase(hash);
                        if (porphan->setDependsOn.empty())
                        {
                            vecPriority.push_back(TxPriority(porphan->dPriority, porphan->feeRate, porphan->ptx));
                            std::push_heap(vecPriority.begin(), vecPriority.end(), comparer);
                        }
                    }
                }
            }
        }

        nLastBlockTx = nBlockTx;
        nLastBlockSize = nBlockSize;

/* MCHN START */    
// If block was dropped, this happens too many times        
//        LogPrintf("CreateNewBlock(): total size %u\n", nBlockSize);
/* MCHN END */    

        // Compute final coinbase transaction.
        txNew.vout[0].nValue = GetBlockValue(nHeight, nFees);
        txNew.vin[0].scriptSig = CScript() << nHeight << OP_0;

        pblock->vSigner[0]=ppubkey->size();
        memcpy(pblock->vSigner+1,ppubkey->begin(),pblock->vSigner[0]);
        
        pblock->vtx[0] = txNew;
        pblocktemplate->vTxFees[0] = -nFees;
        
        
        // Fill in header
        pblock->hashPrevBlock  = pindexPrev->GetBlockHash();
        UpdateTime(pblock, pindexPrev);
        pblock->nBits          = GetNextWorkRequired(pindexPrev, pblock);
        pblock->nNonce         = 0;
        pblocktemplate->vTxSigOps[0] = GetLegacySigOpCount(pblock->vtx[0]);

/* MCHN START */    
        bool testValidity=true;
        
// If this node cannot mine for some reason (permission or diversity, block is not tested for validity to avoid exception        
        if(mc_gState->m_NetworkParams->IsProtocolMultichain())
        {
            if(canMine)
            {
//                const unsigned char *pubkey_hash=(unsigned char *)Hash160(ppubkey->begin(),ppubkey->end()).begin();
//                *canMine=mc_gState->m_Permissions->CanMine(NULL,pubkey_hash);
                uint160 pubkey_hash=Hash160(ppubkey->begin(),ppubkey->end());
                *canMine=mc_gState->m_Permissions->CanMine(NULL,&pubkey_hash);
                if((*canMine & MC_PTP_MINE) == 0)
                {
                    if(prevCanMine & MC_PTP_MINE)
                    {
                        LogPrintf("mchn: MultiChainMiner: cannot mine now, waiting...\n");
                    }
                    testValidity=false;
                }
                else
                {
                    if((prevCanMine & MC_PTP_MINE) == 0)
                    {
                        LogPrintf("CreateNewBlock(): total size %u\n", nBlockSize);
                        LogPrintf("mchn: MultiChainMiner: Starting mining...\n");
                    }                    
                }
            }            
        }
        else
        {        
            LogPrintf("CreateNewBlock(): total size %u\n", nBlockSize);
        }

        if(GetBoolArg("-avoidtestingblockvalidity",true))
        {
            testValidity=false;
        }
        
        if(testValidity)
        {            
/* MCHN END */    
            
        CValidationState state;
        if (!TestBlockValidity(state, *pblock, pindexPrev, false, false))
            throw std::runtime_error("CreateNewBlock() : TestBlockValidity failed");
            
/* MCHN START */    
        }
/* MCHN END */    
    }

    return pblocktemplate.release();
}

/* MCHN START */    
CBlockTemplate* CreateNewBlock(const CScript& scriptPubKeyIn)
{
    return CreateNewBlock(scriptPubKeyIn,NULL,NULL,NULL,NULL);
}
/* MCHN END */    


void IncrementExtraNonce(CBlock* pblock, CBlockIndex* pindexPrev, unsigned int& nExtraNonce,CWallet *pwallet)
{
    // Update nExtraNonce
    static uint256 hashPrevBlock;
    if (hashPrevBlock != pblock->hashPrevBlock)
    {
        nExtraNonce = 0;
        hashPrevBlock = pblock->hashPrevBlock;
    }
    ++nExtraNonce;
    unsigned int nHeight = pindexPrev->nHeight+1; // Height first in coinbase required for block.version=2
    CMutableTransaction txCoinbase(pblock->vtx[0]);
    txCoinbase.vin[0].scriptSig = (CScript() << nHeight << CScriptNum(nExtraNonce)) + COINBASE_FLAGS;
    assert(txCoinbase.vin[0].scriptSig.size() <= 100);

    pblock->vtx[0] = txCoinbase;
    
/* MCHN START */    
    CreateBlockSignature(pblock,BLOCKSIGHASH_NO_SIGNATURE_AND_NONCE,pwallet,NULL);
    pblock->hashMerkleRoot = pblock->BuildMerkleTree();
/* MCHN END */    
}

#ifdef ENABLE_WALLET
//////////////////////////////////////////////////////////////////////////////
//
// Internal miner
//
double dHashesPerSec = 0.0;
int64_t nHPSTimerStart = 0;

//
// ScanHash scans nonces looking for a hash with at least some zero bits.
// The nonce is usually preserved between calls, but periodically or if the
// nonce is 0xffff0000 or above, the block is rebuilt and nNonce starts over at
// zero.
//
bool static ScanHash(CBlock *pblock, uint32_t& nNonce, uint256 *phash,uint16_t success_and_mask,CWallet *pwallet)
{
    // Write the first 76 bytes of the block header to a double-SHA256 state.
    CHash256 hasher;
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << pblock->GetBlockHeader();
//    ss << *pblock;
    assert(ss.size() == 80);
    hasher.Write((unsigned char*)&ss[0], 76);
    uint256 cachedMerkleRoot=0;    
    
    while (true) {
        nNonce++;

        if(Params().DisallowUnsignedBlockNonce())
        {
            pblock->nNonce=nNonce;
            CreateBlockSignature(pblock,BLOCKSIGHASH_NO_SIGNATURE,pwallet,&cachedMerkleRoot);
            *phash=pblock->GetHash();
        }
        else
        {
            // Write the last 4 bytes of the block header (the nonce) to a copy of
            // the double-SHA256 state, and compute the result.
            CHash256(hasher).Write((unsigned char*)&nNonce, 4).Finalize((unsigned char*)phash);
        }

        // Return the nonce if the hash has at least some zero bits,
        // caller will check if it has enough to reach the target
/*        
        if (((uint16_t*)phash)[15] == 0) 
            return true;
*/
        if( (((uint16_t*)phash)[15] & success_and_mask) == 0)
        {
            if(Params().DisallowUnsignedBlockNonce())
            {
                pblock->hashMerkleRoot=pblock->BuildMerkleTree();                   
            }
            return true;            
        }
        
        // If nothing found after trying for a while, return -1
        if ((nNonce & 0xffff) == 0)
        {
//        if ((nNonce & 0xff) == 0)
            if(Params().DisallowUnsignedBlockNonce())
            {
                pblock->hashMerkleRoot=pblock->BuildMerkleTree();                   
            }
            return false;
        }
        if ((nNonce & 0xfff) == 0)
            boost::this_thread::interruption_point();
    }
}

CBlockTemplate* CreateNewBlockWithKey(CReserveKey& reservekey)
{
    CPubKey pubkey;
    if (!reservekey.GetReservedKey(pubkey))
        return NULL;

    CScript scriptPubKey = CScript() << ToByteVector(pubkey) << OP_CHECKSIG;    
    return CreateNewBlock(scriptPubKey);
}

/* MCHN START */    
// Block should be mined for specific keys, not just any from pool


CBlockTemplate* CreateNewBlockWithDefaultKey(CWallet *pwallet,int *canMine,const set<CTxDestination>* addresses,CBlockIndex** ppPrev)
{
    CPubKey pubkey;            
    bool key_found;
    
    {
        LOCK(cs_main);
        key_found=pwallet->GetKeyFromAddressBook(pubkey,MC_PTP_MINE,addresses);
    }
    if(!key_found)
    {
        if(canMine)
        {
            if(*canMine & MC_PTP_MINE)
            {
                *canMine=0;
                LogPrintf("mchn: Cannot find address having mining permission\n");        
            }
        }
        return NULL;        
    }
    
//    const unsigned char *pubkey_hash=(unsigned char *)Hash160(pubkey.begin(),pubkey.end()).begin();
    
    unsigned char pubkey_hash[20];
    uint160 pkhash=Hash160(pubkey.begin(),pubkey.end());
    memcpy(pubkey_hash,&pkhash,20);    
    
    CScript scriptPubKey = CScript() << OP_DUP << OP_HASH160 << vector<unsigned char>(pubkey_hash, pubkey_hash + 20) << OP_EQUALVERIFY << OP_CHECKSIG;
    
    return CreateNewBlock(scriptPubKey,pwallet,&pubkey,canMine,ppPrev);
}

/* MCHN END */    

bool ProcessBlockFound(CBlock* pblock, CWallet& wallet, CReserveKey& reservekey)
{
    if(fDebug)LogPrint("mchnminor","%s\n", pblock->ToString());
    if(fDebug)LogPrint("mcminer","mchn-miner: generated %s\n", FormatMoney(pblock->vtx[0].vout[0].nValue));

    // Found a solution
    {
        LOCK(cs_main);
        if(mc_gState->m_NodePausedState & MC_NPS_MINING)
        {
            return error("MultiChainMiner : mining is paused, generated block is dropped");            
        }
        if (pblock->hashPrevBlock != chainActive.Tip()->GetBlockHash())
        {
            return error("MultiChainMiner : generated block is stale");
        }
    }

    // Remove key from key pool
    reservekey.KeepKey();

    // Track how many getdata requests this block gets
    {
        LOCK(wallet.cs_wallet);
        wallet.mapRequestCount[pblock->GetHash()] = 0;
    }

    // Process this block the same as if we had received it from another node
    CValidationState state;
    if (!ProcessNewBlock(state, NULL, pblock))
        return error("MultiChainMiner : ProcessNewBlock, block not accepted");

    return true;
}

set <CTxDestination> LastActiveMiners(CBlockIndex* pindexTip, CPubKey *kLastMiner, int nMinerPoolSize)
{
    int nRelativeWindowSize=5;
    
    int nTotalMiners=mc_gState->m_Permissions->GetMinerCount();
    int nActiveMiners=mc_gState->m_Permissions->GetActiveMinerCount();
    int nDiversityMiners=0;
    int nWindowSize;
    CBlockIndex* pindex;
    set <CTxDestination> sMiners;   
            
    if(mc_gState->m_NetworkParams->IsProtocolMultichain() == 0)
    {
        return sMiners;
    }
    
    if(MCP_ANYONE_CAN_MINE == 0)
    {
       nDiversityMiners=nTotalMiners-nActiveMiners;
    }
    
    nWindowSize=nRelativeWindowSize*nMinerPoolSize+nDiversityMiners;
    
    pindex=pindexTip;
    for(int i=0;i<nWindowSize;i++)
    {
        if((int)sMiners.size() < nMinerPoolSize)
        {
            if(pindex)
            {
                if(!pindex->kMiner.IsValid())
                {
                    CBlock block;
                    if(ReadBlockFromDisk(block,pindex))
                    {
                        if(block.vSigner[0])
                        {
                            pindex->kMiner.Set(block.vSigner+1, block.vSigner+1+block.vSigner[0]);
                            pindex->nStatus |= BLOCK_HAVE_MINER_PUBKEY;
                        }
                    }
                }
                if(pindex->kMiner.IsValid())
                {
                    CKeyID addr=pindex->kMiner.GetID();
                    if(mc_gState->m_Permissions->CanMine(NULL,addr.begin()))
                    {
                        if(sMiners.find(addr) == sMiners.end())
                        {
                            sMiners.insert(addr);
                        }                    
                    }
                }
                if(pindex == pindexTip)
                {
                    *kLastMiner=pindex->kMiner;
                }
                pindex=pindex->pprev;
            }
        }
    }    
    
    return sMiners;
}

int GetMaxActiveMinersCount()
{
    if(mc_gState->m_NetworkParams->IsProtocolMultichain())
    {
        if(MCP_ANYONE_CAN_MINE)
        {
            return 1048576;
        }
        else
        {
            return mc_gState->m_Permissions->GetActiveMinerCount();
        }
    }
    else
    {
        return 1024;
    }
}

double GetMinerAndExpectedMiningStartTime(CWallet *pwallet,CPubKey *lpkMiner,set <CTxDestination> *lpsMinerPool,double *lpdMiningStartTime,double *lpdActiveMiners,uint256 *lphLastBlockHash,int *lpnMemPoolSize,double wAvBlockTime)
{
    int nMinerPoolSizeMin=4;
    int nMinerPoolSizeMax=16;
    double dRelativeSpread=1.;
    double dRelativeMinerPoolSize=0.25;
    double dAverageCreateBlockTime=2;
    double dAverageCreateBlockTimeShift=0;
    double dMinerDriftMin=mc_gState->m_NetworkParams->ParamAccuracy();
    double dEmergencyMinersConvergenceRate=2.;    
    int nPastBlocks=12;
    
    set <CTxDestination> sPrevMinerPool;
    set <CTxDestination> sThisMinerPool;
    CBlockIndex* pindex;
    CBlockIndex* pindexTip;
    CPubKey kLastMiner;
    CPubKey kThisMiner;

    bool fNewBlock=false;
    bool fWaitingForMiner=false;
    bool fInMinerPool;
    int nMinerPoolSize,nStdMinerPoolSize,nWindowSize;
    double dTargetSpacing,dSpread,dExpectedTimeByLast,dExpectedTimeByPast,dEmergencyMiners,dExpectedTime,dExpectedTimeMin,dExpectedTimeMax,dAverageGap;    
    double dMinerDrift,dActualMinerDrift;

    pindexTip = chainActive.Tip();

    if(*lpdMiningStartTime >= 0)
    {
        if(*lphLastBlockHash == pindexTip->GetBlockHash())
        {
            if( (*lpnMemPoolSize > 0) || (mempool.hashList->m_Count == 0) )
            {
                if(lpkMiner->IsValid())
                {
                    return *lpdMiningStartTime;
                }
                else
                {
                    fWaitingForMiner=true;
                    fNewBlock=true;                    
                }
            }                                    
        }
        else
        {
            fNewBlock=true;
        }
    }
    else
    {
        fNewBlock=true;
    }
    
    *lphLastBlockHash=pindexTip->GetBlockHash();
    *lpnMemPoolSize=mempool.hashList->m_Count;
    
    if( (Params().Interval() > 0) ||                                            // POW 
        (mc_gState->m_Permissions->m_Block <= 1) )    
    {
        pwallet->GetKeyFromAddressBook(kThisMiner,MC_PTP_MINE);
        *lpkMiner=kThisMiner;
        *lpdMiningStartTime=mc_TimeNowAsDouble();                               // start mining immediately
        return *lpdMiningStartTime;
    }        
    
    
    dMinerDrift=Params().MiningTurnover();
    if(dMinerDrift > 1.0)
    {
        dMinerDrift=1.0;
    }    
    dMinerDrift-=dMinerDriftMin;
    if(dMinerDrift < 0)
    {
        dMinerDrift=0.;
    }
    
    dTargetSpacing=Params().TargetSpacing();    
    dSpread=dRelativeSpread*dTargetSpacing;

    *lpdMiningStartTime=mc_TimeNowAsDouble() + 0.5 * dTargetSpacing;        
    dExpectedTimeByLast=pindexTip->dTimeReceived+dTargetSpacing;
    if(dExpectedTimeByLast < mc_TimeNowAsDouble() + dTargetSpacing - 0.5 * dSpread) // Can happen while in reorg or if dTimeReceived is not set
    {
        dExpectedTimeByLast=mc_TimeNowAsDouble() + dTargetSpacing;
        *lpdMiningStartTime=dExpectedTimeByLast;
        fNewBlock=false;
    }
    dExpectedTimeMin=dExpectedTimeByLast - 0.5 * dSpread;
    dExpectedTimeMax=dExpectedTimeByLast + 0.5 * dSpread;
    if(dAverageCreateBlockTime < wAvBlockTime)
    {
        LogPrint("mcminer","mchn-miner: increased average block creation time: %8.3fs, adjusting\n",wAvBlockTime);
        dAverageCreateBlockTimeShift=wAvBlockTime-dAverageCreateBlockTime;
        dAverageCreateBlockTime=wAvBlockTime;
        dExpectedTimeMin-=dAverageCreateBlockTimeShift;
    }
    
    dAverageGap=0;
    nWindowSize=0;
    if(fNewBlock)
    {
        pindex=pindexTip;
        dExpectedTimeByPast=0;
        for(int i=0;i<nPastBlocks;i++)
        {
            if(pindex && (pindex->dTimeReceived > 0.5))
            {
                dExpectedTimeByPast+=pindex->dTimeReceived;
                nWindowSize++;
                dAverageGap=pindex->dTimeReceived;
                pindex=pindex->pprev;
            }
        }
                
        dAverageGap=(dExpectedTimeByLast-dAverageGap) / nWindowSize;
        
        dExpectedTimeByPast /= nWindowSize;
        dExpectedTimeByPast += (nWindowSize + 1) * 0.5 * dTargetSpacing;
       
        
        if(dAverageGap < 0.5*dTargetSpacing)                                    // Catching up
        {
            dExpectedTime=mc_TimeNowAsDouble() + dTargetSpacing;
            nWindowSize=0;                          
        }
        else
        {
            if(nWindowSize < nPastBlocks)
            {
                dExpectedTime=dExpectedTimeByLast;
            }
            else
            {
                dExpectedTime=dExpectedTimeByPast;
                if(dExpectedTime > dExpectedTimeMax)
                {
                    dExpectedTime = dExpectedTimeMax;
                }
                if(dExpectedTime < dExpectedTimeMin)
                {
                    dExpectedTime = dExpectedTimeMin;
                }            
            }
        }
        
        *lpdMiningStartTime=dExpectedTime;
    }
       
    fInMinerPool=false;
    if(mc_gState->m_NetworkParams->IsProtocolMultichain())
    {
        sPrevMinerPool=*lpsMinerPool;
        nStdMinerPoolSize=(int)(dRelativeMinerPoolSize * dSpread / dAverageCreateBlockTime);
        if(nStdMinerPoolSize < nMinerPoolSizeMin)
        {
            nStdMinerPoolSize=nMinerPoolSizeMin;
        }
        if(nStdMinerPoolSize > nMinerPoolSizeMax)
        {
            nStdMinerPoolSize=nMinerPoolSizeMax;
        }
        nMinerPoolSize=nStdMinerPoolSize;
        nStdMinerPoolSize=(int)(dMinerDrift * nStdMinerPoolSize) + 1;            
               
        dActualMinerDrift=dMinerDrift;
        if(dActualMinerDrift < dMinerDriftMin)
        {
            dActualMinerDrift=dMinerDriftMin;
        }
        
        sThisMinerPool=LastActiveMiners(pindexTip,&kLastMiner,nStdMinerPoolSize);
        nMinerPoolSize=sThisMinerPool.size();
        *lpsMinerPool=sThisMinerPool;
        
        fInMinerPool=false;
        if(!pwallet->GetKeyFromAddressBook(kThisMiner,MC_PTP_MINE,&sThisMinerPool))
        {
            pwallet->GetKeyFromAddressBook(kThisMiner,MC_PTP_MINE);
        }
        else
        {
            fInMinerPool=true;
        }
        
        if( fInMinerPool ||
            (sPrevMinerPool.find(kLastMiner.GetID()) == sPrevMinerPool.end()) ||
            (*lpdActiveMiners < -0.5) )
        {
            *lpdActiveMiners=(double)GetMaxActiveMinersCount() - nMinerPoolSize;
            *lpdActiveMiners/=dActualMinerDrift;
        }
        *lpdActiveMiners *= (1. - dActualMinerDrift);            
        if(*lpdActiveMiners < 1.0)
        {
            *lpdActiveMiners=1; 
        }
        if(dMinerDrift >= dMinerDriftMin)
        {
            if(!fInMinerPool)
            {            
                if( (*lpdActiveMiners < 0.5) || ( mc_RandomDouble() < dMinerDrift /(*lpdActiveMiners)))
                {
                    fInMinerPool=true;
                    nMinerPoolSize++;
                }
            }
        }
        if(fInMinerPool)
        {
            *lpdActiveMiners=(double)GetMaxActiveMinersCount() - nMinerPoolSize;            
            *lpdActiveMiners/=dActualMinerDrift;
        }

        if(fInMinerPool)
        {
            *lpdMiningStartTime += mc_RandomDouble() * dSpread;
            *lpdMiningStartTime -= dSpread / (nMinerPoolSize + 1);   
            *lpdMiningStartTime -= dAverageCreateBlockTimeShift;
        }
        else
        {
            dEmergencyMiners=(double)GetMaxActiveMinersCount();
            *lpdMiningStartTime=dExpectedTimeMax;
            *lpdMiningStartTime += dSpread;
            *lpdMiningStartTime -= dSpread / (nMinerPoolSize + 1);                                
            *lpdMiningStartTime += dAverageCreateBlockTime + mc_RandomDouble() * dAverageCreateBlockTime;
            while( (dEmergencyMiners > 0.5) && (mc_RandomDouble() > 1./(dEmergencyMiners)))
            {
                *lpdMiningStartTime += dAverageCreateBlockTime;
                dEmergencyMiners /= dEmergencyMinersConvergenceRate;
            }            
        }
    }
    else
    {
        pwallet->GetKeyFromAddressBook(kThisMiner,MC_PTP_MINE);
        dEmergencyMiners=(double)GetMaxActiveMinersCount();
        while(dEmergencyMiners > 0.5)
        {
            *lpdMiningStartTime -= dAverageCreateBlockTime / 2;
            dEmergencyMiners /= dEmergencyMinersConvergenceRate;
        }            
        if(*lpdMiningStartTime < dExpectedTimeMin)
        {
            *lpdMiningStartTime = dExpectedTimeMin;
        }
        dEmergencyMiners=(double)GetMaxActiveMinersCount();
        while( (dEmergencyMiners > 0.5) && (mc_RandomDouble() > 1./(dEmergencyMiners)))
        {
            *lpdMiningStartTime += dAverageCreateBlockTime;
            dEmergencyMiners /= dEmergencyMinersConvergenceRate;
        }            
    }

    if(!fWaitingForMiner)
    {
        if(kThisMiner.IsValid())
        {
            CBitcoinAddress addr=CBitcoinAddress(kThisMiner.GetID());
            LogPrint("mcminer","mchn-miner: delay: %8.3fs, miner: %s, height: %d, gap: %8.3fs, miners: (tot: %d, max: %d, pool: %d)%s\n",
                             *lpdMiningStartTime-mc_TimeNowAsDouble(),addr.ToString().c_str(),
                             chainActive.Tip()->nHeight,dAverageGap,
                             mc_gState->m_Permissions->GetMinerCount(),GetMaxActiveMinersCount(),
                             nMinerPoolSize,fInMinerPool ? ( (nMinerPoolSize > (int)lpsMinerPool->size()) ? " In Pool New" : " In Pool Old" )  : " Not In Pool");
        }
        else
        {
            LogPrint("mcminer","mchn-miner: miner not found, height: %d, gap: %8.3fs, miners: (tot: %d, max: %d, pool: %d)\n",            
                             chainActive.Tip()->nHeight,dAverageGap,
                             mc_gState->m_Permissions->GetMinerCount(),GetMaxActiveMinersCount(),
                             nMinerPoolSize);
        }
    }    
    *lpkMiner=kThisMiner;
    return *lpdMiningStartTime;
}

void static BitcoinMiner(CWallet *pwallet)
{
    LogPrintf("MultiChainMiner started\n");
    SetThreadPriority(THREAD_PRIORITY_LOWEST);
    RenameThread("bitcoin-miner");

    // Each thread has its own key and counter
    CReserveKey reservekey(pwallet);
    unsigned int nExtraNonce = 0;
    
/* MCHN START */            
    int canMine;
    int prevCanMine;
    canMine=MC_PTP_MINE;
    prevCanMine=canMine;
    
    double dActiveMiners=-1;
    double dMiningStartTime=-1.;
    uint256 hLastBlockHash=0;
    int nMemPoolSize=0;    
    set <CTxDestination> sMinerPool;
    CPubKey kMiner;
    
    int wSize=10;
    int wPos=0;
    int EmptyBlockToMine;
    uint64_t wCount[10];
    double wTime[10];
    double wBlockTime[10];
    memset(wCount,0,wSize*sizeof(uint64_t));
    memset(wTime,0,wSize*sizeof(uint64_t));
    memset(wBlockTime,0,wSize*sizeof(uint64_t));
    int wBlockTimePos=0;
    
    uint16_t success_and_mask=0xffff;
    int min_bits;
    
    if(Params().Interval() <= 0)
    {
        min_bits=(int)mc_gState->m_NetworkParams->GetInt64Param("powminimumbits");
        if(min_bits < 16)
        {
            success_and_mask = success_and_mask << (16 - min_bits);
        }
    }
    
/* MCHN END */            
    

    try {
        while (true) {
/* MCHN START */            
            bool not_setup_period=true;
            
            if(mc_gState->m_NodePausedState & MC_NPS_MINING)
            {
                __US_Sleep(1000);
                boost::this_thread::interruption_point();                                    
            }            
            
            if(mc_gState->m_NetworkParams->IsProtocolMultichain())
            {
                if((canMine & MC_PTP_MINE) == 0)
                {
                    if(mc_gState->m_Permissions->m_Block > 1)
                    {
                        __US_Sleep(1000);
                    }
                    boost::this_thread::interruption_point();                    
                }
                if(mc_gState->m_Permissions->IsSetupPeriod())
                {
                    not_setup_period=false;
                }
                if(mc_gState->m_Permissions->m_Block <= 1)
                {
                    not_setup_period=false;
                }
            }
            
            if (Params().MiningRequiresPeers() 
                    && not_setup_period
                    && ( (mc_gState->m_Permissions->GetMinerCount() > 1)
                    || (MCP_ANYONE_CAN_MINE != 0)
                    || (mc_gState->m_NetworkParams->IsProtocolMultichain() == 0)
                    )
                    ) {

                // Busy-wait for the network to come online so we don't waste time mining
                // on an obsolete chain. In regtest mode we expect to fly solo.
                
                bool wait_for_peers=true;
                if(wait_for_peers)
                {
                    int active_nodes=0;
                    while ((active_nodes == 0) && 
                           ( (mc_gState->m_Permissions->GetMinerCount() > 1)
                          || (MCP_ANYONE_CAN_MINE != 0)
                          || (mc_gState->m_NetworkParams->IsProtocolMultichain() == 0)
                           ) && Params().MiningRequiresPeers())
                    {
                        {
                            LOCK(cs_vNodes);
                            vector<CNode*> vNodesCopy = vNodes;
                            BOOST_FOREACH(CNode* pnode, vNodesCopy)
                            {
                                if(pnode->fSuccessfullyConnected)
                                {
                                    active_nodes++;
                                }
                            }
                        }
                    
                        if(active_nodes == 0)
                        {
                            MilliSleep(1000);
                            boost::this_thread::interruption_point();                                    
                        }                        
                    }
                }
            }
            
            //
            // Create new block
            //
            unsigned int nTransactionsUpdatedLast = mempool.GetTransactionsUpdated();
            CBlockIndex* pindexPrev = chainActive.Tip();
            EmptyBlockToMine=0;
            
            bool fMineEmptyBlocks=true;
            if(Params().MineEmptyRounds()+mc_gState->m_NetworkParams->ParamAccuracy()>= 0)
            {
                fMineEmptyBlocks=false;
                CBlockIndex* pindex=pindexPrev;
                int nMaxEmptyBlocks,nEmptyBlocks,nMinerCount;

                nMaxEmptyBlocks=0;
                nEmptyBlocks=0;
                if(mc_gState->m_NetworkParams->IsProtocolMultichain())
                {
                    nMinerCount=1;
                    if(MCP_ANYONE_CAN_MINE == 0)
                    {
                        nMinerCount=mc_gState->m_Permissions->GetMinerCount()-mc_gState->m_Permissions->GetActiveMinerCount()+1;
                    }
                    double d=Params().MineEmptyRounds()*nMinerCount-mc_gState->m_NetworkParams->ParamAccuracy();
                    if(d >= 0)
                    {
                        nMaxEmptyBlocks=(int)d+1;
                    }

                    fMineEmptyBlocks=false;
                    while(!fMineEmptyBlocks && (pindex != NULL) && (nEmptyBlocks < nMaxEmptyBlocks))
                    {
                        if(pindex->nTx > 1)
                        {
                            fMineEmptyBlocks=true;
                        }
                        else
                        {
                            nEmptyBlocks++;
                            pindex=pindex->pprev;
                        }
                    }
                    if(pindex == NULL)
                    {
                        fMineEmptyBlocks=true;
                    }
                }
                else
                {
                    fMineEmptyBlocks=true;
                }                
            }
            if(!fMineEmptyBlocks)
            {
                if(chainActive.Tip()->nHeight <= LastForkedHeight())
                {
                    EmptyBlockToMine=chainActive.Tip()->nHeight+1;
                    LogPrint("mcminer","mchn-miner: Chain is forked on height %d, ignoring mine-empty-rounds, mining on height %d\n", LastForkedHeight(),chainActive.Tip()->nHeight+1);
                    fMineEmptyBlocks=true;
                }
            }
            if(fMineEmptyBlocks)
            {
                nMemPoolSize=1;
            }
            
            double wAvTimePerBlock=0;
            for(int w=0;w<wSize;w++)
            {
                wAvTimePerBlock+=wBlockTime[w];
            }
            wAvTimePerBlock/=wSize;
            
            canMine=MC_PTP_MINE;
            if(mc_TimeNowAsDouble() < GetMinerAndExpectedMiningStartTime(pwallet, &kMiner,&sMinerPool, &dMiningStartTime,&dActiveMiners,&hLastBlockHash,&nMemPoolSize,wAvTimePerBlock))
            {
                canMine=0;
            }
            else
            {
                if(!kMiner.IsValid())
                {
                    canMine=0;                    
                }
                else
                {
                    if( !fMineEmptyBlocks
                            && not_setup_period
                            && (mempool.hashList->m_Count == 0)
                            )
                    {
                        canMine=0;
                    }
                    else
                    {
                        if(!CanMineWithLockedBlock())
                        {
                            canMine=0;
                        }
                    }
                }
            }
            
//            if(mc_gState->m_ProtocolVersionToUpgrade > mc_gState->m_NetworkParams->ProtocolVersion())
            if( (mc_gState->m_ProtocolVersionToUpgrade > 0) && (mc_gState->IsSupported(mc_gState->m_ProtocolVersionToUpgrade) == 0) )
            {
                canMine=0;
            }

            if(mc_gState->m_NodePausedState & MC_NPS_MINING)
            {
                canMine=0;
            }            
            
            if(fReindex)
            {
                canMine=0;                
            }
            
            if(canMine & MC_PTP_MINE)
            {
//                const unsigned char *pubkey_hash=(unsigned char *)Hash160(kMiner.begin(),kMiner.end()).begin();
                unsigned char pubkey_hash[20];
                uint160 pkhash=Hash160(kMiner.begin(),kMiner.end());
                memcpy(pubkey_hash,&pkhash,20);    
                CScript scriptPubKey = CScript() << OP_DUP << OP_HASH160 << vector<unsigned char>(pubkey_hash, pubkey_hash + 20) << OP_EQUALVERIFY << OP_CHECKSIG;
                canMine=prevCanMine;
                auto_ptr<CBlockTemplate> pblocktemplate(CreateNewBlock(scriptPubKey,pwallet,&kMiner,&canMine,&pindexPrev));            
                prevCanMine=canMine;
/* MCHN END */    
            if (!pblocktemplate.get())
            {
                LogPrintf("Error in MultiChainMiner: Keypool ran out, please call keypoolrefill before restarting the mining thread\n");
                return;
            }
            CBlock *pblock = &pblocktemplate->block;
            IncrementExtraNonce(pblock, pindexPrev, nExtraNonce,pwallet);

            LogPrint("mcminer","mchn-miner: Running MultiChainMiner with %u transactions in block (%u bytes)\n", pblock->vtx.size(),
                ::GetSerializeSize(*pblock, SER_NETWORK, PROTOCOL_VERSION));

            //
            // Search
            //
            int64_t nStart = GetTime();
            uint256 hashTarget = uint256().SetCompact(pblock->nBits);
            uint256 hash;
            uint32_t nNonce = 0;
            uint32_t nOldNonce = 0;

            double wStartTime=mc_TimeNowAsDouble();
            uint64_t wThisCount=0;
            while (true) {
                
                if(EmptyBlockToMine)
                {
                    if(chainActive.Tip()->nHeight != EmptyBlockToMine-1)
                    {
                        LogPrint("mcminer","mchn-miner: Avoiding mining block %d, required %d\n", chainActive.Tip()->nHeight+1,EmptyBlockToMine);
                        break;
                    }
                }
                bool fFound = ScanHash(pblock, nNonce, &hash, success_and_mask, pwallet);
                uint32_t nHashesDone = nNonce - nOldNonce;
                nOldNonce = nNonce;

                wThisCount+=nHashesDone;
                        
                // Check if something found
                if (fFound)
                {
                    if (hash <= hashTarget)
                    {
                        // Found a solution
                        pblock->nNonce = nNonce;
                        assert(hash == pblock->GetHash());

                        SetThreadPriority(THREAD_PRIORITY_NORMAL);

                        double wBlockTimeNow=mc_TimeNowAsDouble();
                        if(wBlockTimeNow>wStartTime+0.01)
                        {
                            wBlockTime[wPos]=wBlockTimeNow-wStartTime;
                            wBlockTimePos=(wBlockTimePos+1)%wSize;
                        }
                        LogPrintf("MultiChainMiner: Block Found - %s, prev: %s, height: %d, txs: %d\n",
                                hash.GetHex(),pblock->hashPrevBlock.ToString().c_str(),mc_gState->m_Permissions->m_Block+1,(int)pblock->vtx.size());
/*                        
                        LogPrintf("MultiChainMiner:\n");
                        LogPrintf("proof-of-work found  \n  hash: %s  \ntarget: %s\n", hash.GetHex(), hashTarget.GetHex());
*/                     
/* MCHN START */                        
//                        if(mc_gState->m_ProtocolVersionToUpgrade > mc_gState->m_NetworkParams->ProtocolVersion())
                        if( (mc_gState->m_ProtocolVersionToUpgrade > 0) && (mc_gState->IsSupported(mc_gState->m_ProtocolVersionToUpgrade) == 0) )
                        {
                            LogPrintf("MultiChainMiner: Waiting for upgrade, block is dropped\n");
                        }
                        else
                        {
                            if(!ProcessBlockFound(pblock, *pwallet, reservekey))
                            {
                                __US_Sleep(1000);
                                boost::this_thread::interruption_point();                                                                    
                            }
                            else
                            {
                                LogPrint("mcminer","mchn-miner: Block successfully processed\n");
                            }
                        }
/* MCHN END */                        
                        
                        SetThreadPriority(THREAD_PRIORITY_LOWEST);

                        // In regression test mode, stop mining after a block is found.
                        if (Params().MineBlocksOnDemand())
                            throw boost::thread_interrupted();

                        break;
                    }
                }

                // Meter hashes/sec
                static int64_t nHashCounter;
                if (nHPSTimerStart == 0)
                {
                    nHPSTimerStart = GetTimeMillis();
                    nHashCounter = 0;
                }
                else
                    nHashCounter += nHashesDone;
                if (GetTimeMillis() - nHPSTimerStart > 4000)
                {
                    static CCriticalSection cs;
                    {
                        LOCK(cs);
                        if (GetTimeMillis() - nHPSTimerStart > 4000)
                        {
                            dHashesPerSec = 1000.0 * nHashCounter / (GetTimeMillis() - nHPSTimerStart);
                            nHPSTimerStart = GetTimeMillis();
                            nHashCounter = 0;
                            static int64_t nLogTime;
                            if (GetTime() - nLogTime > 30 * 60)
                            {
                                nLogTime = GetTime();
                                LogPrintf("hashmeter %6.0f khash/s\n", dHashesPerSec/1000.0);
                            }
                        }
                    }
                }

                // Check for stop or if block needs to be rebuilt
                boost::this_thread::interruption_point();
                // Regtest mode doesn't require peers
                if (vNodes.empty() && Params().MiningRequiresPeers() 
                        && not_setup_period
                        && ( (mc_gState->m_Permissions->GetMinerCount() > 1)
                        || (MCP_ANYONE_CAN_MINE != 0)
                        || (mc_gState->m_NetworkParams->IsProtocolMultichain() == 0)
                        )
                        ) 
                {
//                if (vNodes.empty() && Params().MiningRequiresPeers() && not_setup_period)
                    break;
                }
                if (nNonce >= 0xffff0000)
                    break;
                if (mempool.GetTransactionsUpdated() != nTransactionsUpdatedLast && GetTime() - nStart > 60)
                    break;
                if (pindexPrev != chainActive.Tip())
                    break;

                // Update nTime every few seconds
                
                if(UpdateTime(pblock, pindexPrev))
                {
/* MCHN START */                    
                    CreateBlockSignature(pblock,BLOCKSIGHASH_NO_SIGNATURE_AND_NONCE,pwallet,NULL);
/* MCHN END */                    
                }
                if (Params().AllowMinDifficultyBlocks())
                {
                    // Changing pblock->nTime can change work required on testnet:
                    hashTarget.SetCompact(pblock->nBits);
                }
            }
/* MCHN START */    
            double wTimeNow=mc_TimeNowAsDouble();
//            if(wTimeNow>wStartTime+100)
            if(wTimeNow>wStartTime+0.01)
            {
                wCount[wPos]=wThisCount;
                wTime[wPos]=wTimeNow-wStartTime;
                wPos=(wPos+1)%wSize;
                dHashesPerSec=wThisCount/(wTimeNow-wStartTime);
            }
            
            } 
            else
            {
                if( (mc_gState->m_Permissions->m_Block > 1) || !kMiner.IsValid() )
                {
                    __US_Sleep(100);                
                }
            }
/* MCHN END */    
        }
    }
    catch (boost::thread_interrupted)
    {
        LogPrintf("MultiChainMiner terminated\n");
        throw;
    }
}

void GenerateBitcoins(bool fGenerate, CWallet* pwallet, int nThreads)
{
    static boost::thread_group* minerThreads = NULL;

    if (nThreads < 0) {
        // In regtest threads defaults to 1
        if (Params().DefaultMinerThreads())
            nThreads = Params().DefaultMinerThreads();
        else
            nThreads = boost::thread::hardware_concurrency();
    }

    if (minerThreads != NULL)
    {
        minerThreads->interrupt_all();
/* MCHN START */        
        minerThreads->join_all();
/* MCHN END */        
        
        delete minerThreads;
        minerThreads = NULL;
    }

    if (nThreads == 0 || !fGenerate)
        return;

    minerThreads = new boost::thread_group();
    for (int i = 0; i < nThreads; i++)
        minerThreads->create_thread(boost::bind(&BitcoinMiner, pwallet));
}

#endif // ENABLE_WALLET
