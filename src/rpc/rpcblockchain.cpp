// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2014-2016 The Bitcoin Core developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "chain/checkpoints.h"
#include "core/main.h"
#include "rpc/rpcserver.h"
#include "rpc/rpcserver.h"
#include "utils/sync.h"
#include "utils/util.h"

#include <stdint.h>

/* MCHN START */
#include "structs/base58.h"
/* MCHN END */

#include "json/json_spirit_value.h"

using namespace json_spirit;
using namespace std;

extern void TxToJSON(const CTransaction& tx, const uint256 hashBlock, Object& entry);

/* MCHN START */
bool ParseMultichainTxOutToBuffer(uint256 hash,const CTxOut& txout,mc_Buffer *amounts,mc_Script *lpScript,int *allowed,int *required,string& strFailReason);
vector<int> ParseBlockSetIdentifier(Value blockset_identifier);
bool CreateAssetBalanceList(const CTxOut& out,mc_Buffer *amounts,mc_Script *lpScript);
Object AssetEntry(const unsigned char *txid,int64_t quantity,uint32_t output_level);
Array PermissionEntries(const CTxOut& txout,mc_Script *lpScript,bool fLong);
Array PerOutputDataEntries(const CTxOut& txout,mc_Script *lpScript,uint256 txid,int vout);
string EncodeHexTx(const CTransaction& tx);
int OrphanPoolSize();
bool paramtobool(Value param);
bool StringToInt(string str,int *value);
/* MCHN END */

void ScriptPubKeyToJSON(const CScript& scriptPubKey, Object& out, bool fIncludeHex);

double GetDifficulty(const CBlockIndex* blockindex)
{
    // Floating point number that is a multiple of the minimum difficulty,
    // minimum difficulty = 1.0.
    if (blockindex == NULL)
    {
        if (chainActive.Tip() == NULL)
            return 1.0;
        else
            blockindex = chainActive.Tip();
    }

    int nShift = (blockindex->nBits >> 24) & 0xff;

    double dDiff =
        (double)0x0000ffff / (double)(blockindex->nBits & 0x00ffffff);

    while (nShift < 29)
    {
        dDiff *= 256.0;
        nShift++;
    }
    while (nShift > 29)
    {
        dDiff /= 256.0;
        nShift--;
    }

    return dDiff;
}

Object blockToJSONForListBlocks(const CBlock& block, const CBlockIndex* blockindex, bool verbose)
{
    Object result;
    result.push_back(Pair("hash", blockindex->GetBlockHash().GetHex()));
/* MCHN START */    
    CKeyID keyID;
    Value miner;
    if(mc_gState->m_NetworkParams->IsProtocolMultichain())
    {
        if(mc_gState->m_Permissions->GetBlockMiner(blockindex->nHeight,(unsigned char*)&keyID) == MC_ERR_NOERROR)
        {
            miner=CBitcoinAddress(keyID).ToString();
        }
    }
    result.push_back(Pair("miner", miner));
/* MCHN END */        
    int confirmations = -1;
    // Only report confirmations if the block is on the main chain
    if (chainActive.Contains(blockindex))
        confirmations = chainActive.Height() - blockindex->nHeight + 1;
    result.push_back(Pair("confirmations", confirmations));
    result.push_back(Pair("height", blockindex->nHeight));
    result.push_back(Pair("time", (int64_t)blockindex->nTime));
    result.push_back(Pair("txcount", (int64_t)blockindex->nTx));
    
    if(verbose)
    {
        result.push_back(Pair("size", (int)::GetSerializeSize(block, SER_NETWORK, PROTOCOL_VERSION)));
        result.push_back(Pair("version", block.nVersion));
        result.push_back(Pair("merkleroot", block.hashMerkleRoot.GetHex()));
        result.push_back(Pair("nonce", (uint64_t)block.nNonce));
        result.push_back(Pair("bits", strprintf("%08x", block.nBits)));
        result.push_back(Pair("difficulty", GetDifficulty(blockindex)));
        result.push_back(Pair("chainwork", blockindex->nChainWork.GetHex()));

        if (blockindex->pprev)
            result.push_back(Pair("previousblockhash", blockindex->pprev->GetBlockHash().GetHex()));
        CBlockIndex *pnext = chainActive.Next(blockindex);
        if (pnext)
            result.push_back(Pair("nextblockhash", pnext->GetBlockHash().GetHex()));
    }
    
    return result;
}



Object blockToJSON(const CBlock& block, const CBlockIndex* blockindex, bool txDetails = false, int verbose_level = 1)
{
    Object result;
    result.push_back(Pair("hash", block.GetHash().GetHex()));
/* MCHN START */    
    CKeyID keyID;
    Value miner;
    if(mc_gState->m_NetworkParams->IsProtocolMultichain())
    {
        if(mc_gState->m_Permissions->GetBlockMiner(blockindex->nHeight,(unsigned char*)&keyID) == MC_ERR_NOERROR)
        {
            miner=CBitcoinAddress(keyID).ToString();
        }
    }
    result.push_back(Pair("miner", miner));
/* MCHN END */        
    int confirmations = -1;
    // Only report confirmations if the block is on the main chain
    if (chainActive.Contains(blockindex))
        confirmations = chainActive.Height() - blockindex->nHeight + 1;
    result.push_back(Pair("confirmations", confirmations));
    result.push_back(Pair("size", (int)::GetSerializeSize(block, SER_NETWORK, PROTOCOL_VERSION)));
    result.push_back(Pair("height", blockindex->nHeight));
    result.push_back(Pair("version", block.nVersion));
    result.push_back(Pair("merkleroot", block.hashMerkleRoot.GetHex()));
    Array txs;
    Object objTx;
    BOOST_FOREACH(const CTransaction&tx, block.vtx)
    {        
        if(txDetails)
        {
            objTx.clear();
            switch(verbose_level)
            {
                case 1:
                    txs.push_back(tx.GetHash().GetHex());
                    break;
                case 2:
                    objTx.push_back(Pair("txid", tx.GetHash().GetHex()));
                    objTx.push_back(Pair("hex", EncodeHexTx(tx)));
                    txs.push_back(objTx);
                    break;                    
                case 3:
                    objTx.push_back(Pair("txid", tx.GetHash().GetHex()));
                    objTx.push_back(Pair("version", tx.nVersion));
                    objTx.push_back(Pair("locktime", (int64_t)tx.nLockTime));
                    objTx.push_back(Pair("hex", EncodeHexTx(tx)));
                    txs.push_back(objTx);
                    break;                                                            
                case 4:
                    TxToJSON(tx, uint256(0), objTx);
                    objTx.push_back(Pair("hex", EncodeHexTx(tx)));
                    txs.push_back(objTx);
                    break;
            }
        }
        else
            txs.push_back(tx.GetHash().GetHex());
    }
    result.push_back(Pair("tx", txs));
    result.push_back(Pair("time", block.GetBlockTime()));
    result.push_back(Pair("nonce", (uint64_t)block.nNonce));
    result.push_back(Pair("bits", strprintf("%08x", block.nBits)));
    result.push_back(Pair("difficulty", GetDifficulty(blockindex)));
    result.push_back(Pair("chainwork", blockindex->nChainWork.GetHex()));

    if(blockindex->nStatus & BLOCK_FAILED_MASK)
    {
        result.push_back(Pair("valid", false));        
    }
    
    if (blockindex->pprev)
        result.push_back(Pair("previousblockhash", blockindex->pprev->GetBlockHash().GetHex()));
    CBlockIndex *pnext = chainActive.Next(blockindex);
    if (pnext)
        result.push_back(Pair("nextblockhash", pnext->GetBlockHash().GetHex()));
    return result;
}


Value getblockcount(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error("Help message not found\n");

    return chainActive.Height();
}

Value getbestblockhash(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error("Help message not found\n");

    return chainActive.Tip()->GetBlockHash().GetHex();
}

Value getdifficulty(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error("Help message not found\n");

    return GetDifficulty();
}


Value getrawmempool(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)                                             // MCHN
        throw runtime_error("Help message not found\n");

    bool fVerbose = false;
    if (params.size() > 0)
        fVerbose = params[0].get_bool();

    if (fVerbose)
    {
        LOCK(mempool.cs);
        Object o;
        BOOST_FOREACH(const PAIRTYPE(uint256, CTxMemPoolEntry)& entry, mempool.mapTx)
        {
            const uint256& hash = entry.first;
            const CTxMemPoolEntry& e = entry.second;
            Object info;
            info.push_back(Pair("size", (int)e.GetTxSize()));
            info.push_back(Pair("fee", ValueFromAmount(e.GetFee())));
            info.push_back(Pair("time", e.GetTime()));
            info.push_back(Pair("height", (int)e.GetHeight()));
            info.push_back(Pair("startingpriority", e.GetPriority(e.GetHeight())));
            info.push_back(Pair("currentpriority", e.GetPriority(chainActive.Height())));
            const CTransaction& tx = e.GetTx();
            set<string> setDepends;
            BOOST_FOREACH(const CTxIn& txin, tx.vin)
            {
                if (mempool.exists(txin.prevout.hash))
                    setDepends.insert(txin.prevout.hash.ToString());
            }
            Array depends(setDepends.begin(), setDepends.end());
            info.push_back(Pair("depends", depends));
            o.push_back(Pair(hash.ToString(), info));
        }
        return o;
    }
    else
    {
        vector<uint256> vtxid;
        mempool.queryHashes(vtxid);

        Array a;
        BOOST_FOREACH(const uint256& hash, vtxid)
            a.push_back(hash.ToString());

        return a;
    }
}

Value getblockhash(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error("Help message not found\n");

    int64_t nHeight = params[0].get_int64();                                    // MCHN - was int
    if (nHeight < 0 || nHeight > chainActive.Height())
        throw JSONRPCError(RPC_BLOCK_NOT_FOUND, "Block height out of range");

    CBlockIndex* pblockindex = chainActive[nHeight];
    return pblockindex->GetBlockHash().GetHex();
}

/* MCHN START */
Value clearmempool(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 0)
        throw runtime_error("Help message not found\n");
    
    uint32_t required_paused_state=MC_NPS_INCOMING | MC_NPS_MINING;
    if((mc_gState->m_NodePausedState & required_paused_state) != required_paused_state)
    {
        throw JSONRPCError(RPC_NOT_ALLOWED, "Local mining and the processing of incoming transactions and blocks should be paused.");        
    }
    
    ClearMemPools();
    
    return "Mempool cleared";
}

Value setlastblock(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)                        // MCHN
        throw runtime_error("Help message not found\n");

    uint32_t required_paused_state=MC_NPS_INCOMING | MC_NPS_MINING;
    if((mc_gState->m_NodePausedState & required_paused_state) != required_paused_state)
    {
        throw JSONRPCError(RPC_NOT_ALLOWED, "Local mining and the processing of incoming transactions and blocks should be paused.");        
    }
    
    
    if(params.size() == 0)
    {
        SetLastBlock(0);
    }
    else
    {
        std::string strHash = params[0].get_str();
        if(strHash.size() < 64)
        {
            int nHeight = atoi(params[0].get_str().c_str());
            if (nHeight <= 0 || nHeight > chainActive.Height())
            {
                nHeight+=chainActive.Height();
                if (nHeight <= 0 || nHeight > chainActive.Height())
                {
                    throw JSONRPCError(RPC_BLOCK_NOT_FOUND, "Block height out of range");
                }
            }

            strHash=chainActive[nHeight]->GetBlockHash().GetHex();            
        }
        
        uint256 hash(strHash);
        bool fNotFound;
        
        string result=SetLastBlock(hash,&fNotFound);

        if(result.size())
        {
            if(fNotFound)
            {
                throw JSONRPCError(RPC_BLOCK_NOT_FOUND, result);                
            }
            else
            {
                throw JSONRPCError(RPC_VERIFY_REJECTED, result);                                
            }
        }        
    }
    
    return chainActive.Tip()->GetBlockHash().GetHex();
}

Value listblocks(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)                        // MCHN
        throw runtime_error("Help message not found\n");
    
    Array result;
    vector <int> heights=ParseBlockSetIdentifier(params[0]);
    
    bool verbose=false;
    
    if (params.size() > 1)    
    {
        verbose=paramtobool(params[1]);
    }
    
    for(unsigned int i=0;i<heights.size();i++)
    {
        CBlock block;
        if(verbose)
        {
            if(!ReadBlockFromDisk(block, chainActive[heights[i]]))
                throw JSONRPCError(RPC_INTERNAL_ERROR, "Can't read block from disk");
        }
        
        result.push_back(blockToJSONForListBlocks(block, chainActive[heights[i]], verbose));
    }
    
    return result;
}

Value getlastblockinfo(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        mc_ThrowHelpMessage("getlastblockinfo");        
//        throw runtime_error("Help message not found\n");
    
    CBlockIndex* pblockindex = chainActive.Tip();
    
    if(params.size() == 1)
    {
        if(params[0].type() != int_type)
        {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Skip should be integer");
        }
        
        int skip=params[0].get_int();
        if (skip < 0 || skip > chainActive.Height())
            throw JSONRPCError(RPC_BLOCK_NOT_FOUND, "Skip out of range");
        
        pblockindex=chainActive[chainActive.Height() - skip];
    }
   
    Object result;
    result.push_back(Pair("hash", pblockindex->GetBlockHash().GetHex()));
    result.push_back(Pair("height", pblockindex->nHeight));
    result.push_back(Pair("time", pblockindex->GetBlockTime()));
    result.push_back(Pair("txcount", (int)pblockindex->nTx));
    
    CKeyID keyID;
    Value miner;
    if(mc_gState->m_NetworkParams->IsProtocolMultichain())
    {
        if(mc_gState->m_Permissions->GetBlockMiner(pblockindex->nHeight,(unsigned char*)&keyID) == MC_ERR_NOERROR)
        {
            miner=CBitcoinAddress(keyID).ToString();
        }
    }
    result.push_back(Pair("miner", miner));
    
    
    return result;    
}

/* MCHN END */


Value getblock(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)                        // MCHN
        throw runtime_error("Help message not found\n");

    int nHeight=-1;
    bool is_hash=true;
    std::string strHash;
    
    if(params[0].type() == int_type)
    {
        nHeight=params[0].get_int();
        is_hash=false;
    }
    else
    {
        strHash = params[0].get_str();
        if( strHash.size() < 64 )
        {
            if(!StringToInt(params[0].get_str(),&nHeight))
            {
                throw JSONRPCError(RPC_BLOCK_NOT_FOUND, "Block height should be integer");
            }
            is_hash=false;
        }        
    }
    
    if(!is_hash)
    {
        if (nHeight < 0 || nHeight > chainActive.Height())
            throw JSONRPCError(RPC_BLOCK_NOT_FOUND, "Block height out of range");

        strHash=chainActive[nHeight]->GetBlockHash().GetHex();                    
    }
        
//        int nHeight = atoi(params[0].get_str().c_str());
    uint256 hash(strHash);
/*
    bool fVerbose = true;
    if (params.size() > 1)
        fVerbose = params[1].get_bool();
*/
    int verbose_level=1;
    if (params.size() > 1)
    {
        if (params[1].type() == bool_type)
        {
            if(!params[1].get_bool())
            {
                verbose_level=0;
            }        
        }
        else
        {
            verbose_level=params[1].get_int();
            if((verbose_level < 0) || (verbose_level >4))
            {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "verbose out of range");                
            }
        }
    }    
    
    if (mapBlockIndex.count(hash) == 0)
        throw JSONRPCError(RPC_BLOCK_NOT_FOUND, "Block not found");
    
    CBlock block;
    CBlockIndex* pblockindex = mapBlockIndex[hash];

    if(pMultiChainFilterEngine->m_TxID != 0)
    {
        if (!chainActive.Contains(pblockindex))
        {
            throw JSONRPCError(RPC_BLOCK_NOT_FOUND, "Block not found in active chain");
        }    
    }
    
    if(!ReadBlockFromDisk(block, pblockindex))
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Can't read block from disk");

//    if (!fVerbose)
    if(verbose_level == 0)
    {
        CDataStream ssBlock(SER_NETWORK, PROTOCOL_VERSION);
        ssBlock << block;
//            std::string strHex = HexStr(ssBlock.begin(), ssBlock.end());
        return HexStr(ssBlock.begin(), ssBlock.end());
    }

    return blockToJSON(block, pblockindex, true, verbose_level);
}

Value gettxoutsetinfo(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error("Help message not found\n");

    Object ret;

    CCoinsStats stats;
    FlushStateToDisk();
    if (pcoinsTip->GetStats(stats)) {
        ret.push_back(Pair("height", (int64_t)stats.nHeight));
        ret.push_back(Pair("bestblock", stats.hashBlock.GetHex()));
        ret.push_back(Pair("transactions", (int64_t)stats.nTransactions));
        ret.push_back(Pair("txouts", (int64_t)stats.nTransactionOutputs));
        ret.push_back(Pair("bytes_serialized", (int64_t)stats.nSerializedSize));
        ret.push_back(Pair("hash_serialized", stats.hashSerialized.GetHex()));
        ret.push_back(Pair("total_amount", ValueFromAmount(stats.nTotalAmount)));
    }
    return ret;
}

Value getfiltertxinput(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)                       
        mc_ThrowHelpMessage("getfiltertxinput");        
//        throw JSONRPCError(RPC_INVALID_PARAMS, "Wrong number of parameters");                    
    
    int64_t vin = params[0].get_int64();                                          
    
    if(pMultiChainFilterEngine->m_Vout >= 0)
    {
        throw JSONRPCError(RPC_NOT_SUPPORTED, "This callback cannot be used in stream filters");                            
    }
    
    if( (vin < 0) || (vin >= (unsigned int)pMultiChainFilterEngine->m_Tx.vin.size()) )
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "vin out of range");                    
    }
    
    
    Array getxoutparams;
    getxoutparams.push_back(pMultiChainFilterEngine->m_Tx.vin[vin].prevout.hash.ToString());
    getxoutparams.push_back((int64_t)pMultiChainFilterEngine->m_Tx.vin[vin].prevout.n);
        
    return gettxout(getxoutparams,false);
}

Value gettxout(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 3)                        // MCHN
        throw runtime_error("Help message not found\n");

    Object ret;

    std::string strHash = params[0].get_str();
    uint256 hash(strHash);
    int64_t n = params[1].get_int64();                                          // MCHN was int
    bool fMempool = true;
    if (params.size() > 2)
        fMempool = params[2].get_bool();

    CCoins coins;
    if (fMempool) {
        LOCK(mempool.cs);
        CCoinsViewMemPool view(pcoinsTip, mempool);
        if (!view.GetCoins(hash, coins))
            return Value::null;
        if(pMultiChainFilterEngine->m_TxID == 0)                                // In filter we already checked this input exists, but mempool is dirty
        {
            mempool.pruneSpent(hash, coins); // TODO: this should be done by the CCoinsViewMemPool
        }
    } else {
        if (!pcoinsTip->GetCoins(hash, coins))
            return Value::null;
    }
    if (n<0 || (unsigned int)n>=coins.vout.size() || coins.vout[n].IsNull())
        return Value::null;

    BlockMap::iterator it = mapBlockIndex.find(pcoinsTip->GetBestBlock());
    CBlockIndex *pindex = it->second;
    ret.push_back(Pair("bestblock", pindex->GetBlockHash().GetHex()));
    if ((unsigned int)coins.nHeight == MEMPOOL_HEIGHT)
        ret.push_back(Pair("confirmations", 0));
    else
        ret.push_back(Pair("confirmations", pindex->nHeight - coins.nHeight + 1));
    ret.push_back(Pair("value", ValueFromAmount(coins.vout[n].nValue)));
    Object o;
    ScriptPubKeyToJSON(coins.vout[n].scriptPubKey, o, true);
    ret.push_back(Pair("scriptPubKey", o));
    ret.push_back(Pair("version", coins.nVersion));
    ret.push_back(Pair("coinbase", coins.fCoinBase));
    
/* MCHN START */        

    mc_Buffer *asset_amounts=mc_gState->m_TmpBuffers->m_RpcABBuffer1;
       
    mc_Script *lpScript=mc_gState->m_TmpBuffers->m_RpcScript3;
    lpScript->Clear();
    
    asset_amounts->Clear();
    CTxOut txout=coins.vout[n];
    if(CreateAssetBalanceList(txout,asset_amounts,lpScript))
    {
        Array assets;
        unsigned char *ptr;

        for(int a=0;a<asset_amounts->GetCount();a++)
        {
            Object asset_entry;
            ptr=(unsigned char *)asset_amounts->GetRow(a);
            const unsigned char *txid;
            
            txid=(unsigned char*)&hash;
            if( (mc_GetABRefType(ptr) != MC_AST_ASSET_REF_TYPE_SPECIAL) &&
                (mc_GetABRefType(ptr) != MC_AST_ASSET_REF_TYPE_GENESIS) )                    
//            if(mc_GetLE(ptr,4) > 0)
            {
                mc_EntityDetails entity;
                if(mc_gState->m_Assets->FindEntityByFullRef(&entity,ptr))
                {
                    txid=entity.GetTxID();
                }
            }                

            asset_entry=AssetEntry(txid,mc_GetABQuantity(ptr),0x05);
            
            if(mc_GetABRefType(ptr) == MC_AST_ASSET_REF_TYPE_GENESIS)
//            if(mc_GetLE(ptr,4) == 0)
            {
                asset_entry.push_back(Pair("issue", true));
            }
            assets.push_back(asset_entry);
        }

        if( (assets.size() > 0) || (mc_gState->m_Compatibility & MC_VCM_1_0) )
        {
            ret.push_back(Pair("assets", assets));
        }
    }
    Array permissions=PermissionEntries(txout,lpScript,false);
    if( (permissions.size() > 0) || (mc_gState->m_Compatibility & MC_VCM_1_0) )
    {
        ret.push_back(Pair("permissions", permissions));
    }
    Array data=PerOutputDataEntries(txout,lpScript,hash,n);
    if(data.size())
    {
        ret.push_back(Pair("data", data));
    }
/* MCHN END */        

    return ret;
}

Value verifychain(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 2)
        throw runtime_error("Help message not found\n");

    int nCheckLevel = GetArg("-checklevel", 3);
    int nCheckDepth = GetArg("-checkblocks", 288);
    if (params.size() > 0)
        nCheckLevel = params[0].get_int();
    if (params.size() > 1)
        nCheckDepth = params[1].get_int();

    return CVerifyDB().VerifyDB(pcoinsTip, nCheckLevel, nCheckDepth);
}

Value getblockchaininfo(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error("Help message not found\n");

    Object obj;
/* MCHN START*/
//    obj.push_back(Pair("chain",                 Params().NetworkIDString()));
    obj.push_back(Pair("chain",Params().TestnetToBeDeprecatedFieldRPC() ? "test" : "main"));
    obj.push_back(Pair("chainname", string(mc_gState->m_NetworkParams->Name())));
    obj.push_back(Pair("description", string((char*)mc_gState->m_NetworkParams->GetParam("chaindescription",NULL))));
    obj.push_back(Pair("protocol", string((char*)mc_gState->m_NetworkParams->GetParam("chainprotocol",NULL))));        
    obj.push_back(Pair("setupblocks", mc_gState->m_NetworkParams->GetInt64Param("setupfirstblocks")));    
    obj.push_back(Pair("reindex",       fReindex));    
/* MCHN END*/
    obj.push_back(Pair("blocks",                (int)chainActive.Height()));
    obj.push_back(Pair("headers",               pindexBestHeader ? pindexBestHeader->nHeight : -1));
    obj.push_back(Pair("bestblockhash",         chainActive.Tip()->GetBlockHash().GetHex()));
    obj.push_back(Pair("difficulty",            (double)GetDifficulty()));
    obj.push_back(Pair("verificationprogress",  Checkpoints::GuessVerificationProgress(chainActive.Tip())));
    obj.push_back(Pair("chainwork",             chainActive.Tip()->nChainWork.GetHex()));
        
    
    double chain_balance=0.;
    if(COIN)
    {
        int chain_height=(int)chainActive.Height();
        int64_t epoch_size=Params().SubsidyHalvingInterval();
        int complete_epochs=chain_height / epoch_size;
        int blocks_in_this_epoch=chain_height%epoch_size+1;
        int64_t total_value=0;
        int64_t epoch_value=MCP_INITIAL_BLOCK_REWARD;
        for(int epoch=0;epoch<complete_epochs;epoch++)
        {
            total_value+=epoch_value*epoch_size;
            epoch_value >>= 1;
        }
        total_value+=epoch_value*(int64_t)blocks_in_this_epoch;
        if(chain_height >= 0)
        {
            total_value-=MCP_INITIAL_BLOCK_REWARD;                                      // Genesis block reward is unspendable
        }
        if(MCP_FIRST_BLOCK_REWARD >= 0)
        {
            if(chain_height >= 1)
            {
                total_value+=MCP_FIRST_BLOCK_REWARD-MCP_INITIAL_BLOCK_REWARD;
            }
        }
        chain_balance=(double)total_value/(double)COIN;
    }
    obj.push_back(Pair("chainrewards",             chain_balance));
    
    return obj;
}

/** Comparison function for sorting the getchaintips heads.  */
struct CompareBlocksByHeight
{
    bool operator()(const CBlockIndex* a, const CBlockIndex* b) const
    {
        /* Make sure that unequal blocks with the same height do not compare
           equal. Use the pointers themselves to make a distinction. */

        if (a->nHeight != b->nHeight)
          return (a->nHeight > b->nHeight);

        return a < b;
    }
};

Value getchaintips(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error("Help message not found\n");

    /* Build up a list of chain tips.  We start with the list of all
       known blocks, and successively remove blocks that appear as pprev
       of another block.  */
    std::set<const CBlockIndex*, CompareBlocksByHeight> setTips;
    BOOST_FOREACH(const PAIRTYPE(const uint256, CBlockIndex*)& item, mapBlockIndex)
        setTips.insert(item.second);
    BOOST_FOREACH(const PAIRTYPE(const uint256, CBlockIndex*)& item, mapBlockIndex)
    {
        const CBlockIndex* pprev = item.second->pprev;
        if (pprev)
            setTips.erase(pprev);
    }

    // Always report the currently active tip.
    setTips.insert(chainActive.Tip());

    /* Construct the output array.  */
    Array res;
    BOOST_FOREACH(const CBlockIndex* block, setTips)
    {
        Object obj;
        obj.push_back(Pair("height", block->nHeight));
        obj.push_back(Pair("hash", block->phashBlock->GetHex()));

        const int branchLen = block->nHeight - chainActive.FindFork(block)->nHeight;
        obj.push_back(Pair("branchlen", branchLen));

        string status;
        if (chainActive.Contains(block)) {
            // This block is part of the currently active chain.
            status = "active";
        } else if (block->nStatus & BLOCK_FAILED_MASK) {
            // This block or one of its ancestors is invalid.
            status = "invalid";
        } else if (block->nChainTx == 0) {
            // This block cannot be connected because full block data for it or one of its parents is missing.
            status = "headers-only";
        } else if (block->IsValid(BLOCK_VALID_SCRIPTS)) {
            // This block is fully validated, but no longer part of the active chain. It was probably the active block once, but was reorganized.
            status = "valid-fork";
        } else if (block->IsValid(BLOCK_VALID_TREE)) {
            // The headers for this block are valid, but it has not been validated. It was probably never part of the most-work chain.
            status = "valid-headers";
        } else {
            // No clue.
            status = "unknown";
        }
        obj.push_back(Pair("status", status));

        res.push_back(obj);
    }

    return res;
}

Value getmempoolinfo(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error("Help message not found\n");

    Object ret;
    ret.push_back(Pair("size", (int64_t) mempool.size()));
    ret.push_back(Pair("bytes", (int64_t) mempool.GetTotalTxSize()));
//    ret.push_back(Pair("orphan", OrphanPoolSize()));

    return ret;
}

Value invalidateblock(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error("Help message not found\n");

    std::string strHash = params[0].get_str();
    uint256 hash(strHash);
    CValidationState state;

    {
        LOCK(cs_main);
        if (mapBlockIndex.count(hash) == 0)
            throw JSONRPCError(RPC_BLOCK_NOT_FOUND, "Block not found");

        CBlockIndex* pblockindex = mapBlockIndex[hash];
        InvalidateBlock(state, pblockindex);
    }

    if (state.IsValid()) {
        ActivateBestChain(state);
    }

    if (!state.IsValid()) {
        throw JSONRPCError(RPC_DATABASE_ERROR, state.GetRejectReason());
    }

    return Value::null;
}

Value reconsiderblock(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error("Help message not found\n");

    std::string strHash = params[0].get_str();
    uint256 hash(strHash);
    CValidationState state;

    {
        LOCK(cs_main);
        if (mapBlockIndex.count(hash) == 0)
            throw JSONRPCError(RPC_BLOCK_NOT_FOUND, "Block not found");

        CBlockIndex* pblockindex = mapBlockIndex[hash];
        ReconsiderBlock(state, pblockindex);
    }

    if (state.IsValid()) {
        ActivateBestChain(state);
    }

    if (!state.IsValid()) {
        throw JSONRPCError(RPC_DATABASE_ERROR, state.GetRejectReason());
    }

    return Value::null;
}


