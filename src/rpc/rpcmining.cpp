// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2014-2016 The Bitcoin Core developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "structs/amount.h"
#include "chainparams/chainparams.h"
#include "utils/core_io.h"
#include "core/init.h"
#include "net/net.h"
#include "core/main.h"
#include "miner/miner.h"
#include "chain/pow.h"
#include "rpcserver.h"
#include "utils/util.h"
#ifdef ENABLE_WALLET
#include "wallet/dbwrap.h"
#include "wallet/wallet.h"
#endif

#include <stdint.h>

#include <boost/assign/list_of.hpp>

#include "json/json_spirit_utils.h"
#include "json/json_spirit_value.h"
#include "rpcwallet.h"

using namespace json_spirit;
using namespace std;

/**
 * Return average network hashes per second based on the last 'lookup' blocks,
 * or from the last difficulty change if 'lookup' is nonpositive.
 * If 'height' is nonnegative, compute the estimate at the time when a given block was found.
 */
Value GetNetworkHashPS(int lookup, int height) {
    CBlockIndex *pb = chainActive.Tip();

    if (height >= 0 && height < chainActive.Height())
        pb = chainActive[height];

    if (pb == NULL || !pb->nHeight)
        return 0;

    // If lookup is -1, then use blocks since last difficulty change.
    if (lookup <= 0)
        lookup = pb->nHeight % 2016 + 1;

    // If lookup is larger than chain, then set it to chain length.
    if (lookup > pb->nHeight)
        lookup = pb->nHeight;

    CBlockIndex *pb0 = pb;
    int64_t minTime = pb0->GetBlockTime();
    int64_t maxTime = minTime;
    for (int i = 0; i < lookup; i++) {
        pb0 = pb0->pprev;
        int64_t time = pb0->GetBlockTime();
        minTime = std::min(time, minTime);
        maxTime = std::max(time, maxTime);
    }

    // In case there's a situation where minTime == maxTime, we don't want a divide by zero exception.
    if (minTime == maxTime)
        return 0;

    uint256 workDiff = pb->nChainWork - pb0->nChainWork;
    int64_t timeDiff = maxTime - minTime;

    return (int64_t)(workDiff.getdouble() / timeDiff);
}

Value getnetworkhashps(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 2)
        throw runtime_error("Help message not found\n");

    return GetNetworkHashPS(params.size() > 0 ? params[0].get_int() : 120, params.size() > 1 ? params[1].get_int() : -1);
}

#ifdef ENABLE_WALLET
Value getgenerate(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error("Help message not found\n");

    return GetBoolArg("-gen", true);
}


Value setgenerate(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error("Help message not found\n");

    if (pwalletMain == NULL)
        throw JSONRPCError(RPC_NOT_SUPPORTED, "Method not found (disabled)");

    set<CTxDestination> miner_addresses;
    set<CTxDestination> *lpMinerAddresses;
    vector<CTxDestination> addresses;
    
    lpMinerAddresses=NULL;
    
    bool fGenerate = true;
    if (params.size() > 0)
    {
        if(params[0].type() == bool_type)
        {
            fGenerate = params[0].get_bool();            
        }
        else
        {
            if(Params().MineBlocksOnDemand())
            {
                if(params[0].type() == str_type)
                {
                    addresses=ParseAddresses(params[0].get_str(),false,false);
                    if(addresses.size())
                    {
                        for(int i=0;i<(int)addresses.size();i++)
                        {
                            miner_addresses.insert(addresses[i]);
                        }
                        lpMinerAddresses=&miner_addresses;
                    }
                }
                else
                {
                    throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid value for 'generate' field, should be boolean or string");                                                                                
                }
            }
            else
            {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid value for 'generate' field, should be boolean");                                                                                
            }
        }
    }

    int nGenProcLimit = 1;
    if(Params().Interval() > 0)
    {
        nGenProcLimit = -1;
    }
    if (params.size() > 1)
    {
        nGenProcLimit = params[1].get_int();
        if (nGenProcLimit == 0)
            fGenerate = false;
    }

    // -regtest mode: don't return until nGenProcLimit blocks are generated
    if (fGenerate && Params().MineBlocksOnDemand())
    {

        if(fReindex)
        {
            throw JSONRPCError(RPC_NOT_ALLOWED, "Creating new blocks is not allowed while reindexing");            
        }

        int nHeightStart = 0;
        int nHeightEnd = 0;
        int nHeight = 0;
        int nGenerate = (nGenProcLimit > 0 ? nGenProcLimit : 1);
        CReserveKey reservekey(pwalletMain);

        {   // Don't keep cs_main locked
            LOCK(cs_main);
            nHeightStart = chainActive.Height();
            nHeight = nHeightStart;
            nHeightEnd = nHeightStart+nGenerate;
        }
        unsigned int nExtraNonce = 0;
        Array blockHashes;
        while (nHeight < nHeightEnd)
        {
            int canMine=0;
            CBlockIndex *pindexPrev;
            auto_ptr<CBlockTemplate> pblocktemplate(CreateNewBlockWithDefaultKey(pwalletMain,&canMine,lpMinerAddresses,&pindexPrev));
//            auto_ptr<CBlockTemplate> pblocktemplate(CreateNewBlockWithKey(reservekey));
            if (!pblocktemplate.get())
            {
                if(mc_gState->m_NetworkParams->IsProtocolMultichain())
                {
                    if(nGenerate > 1)
                    {
                        throw JSONRPCError(RPC_INSUFFICIENT_PERMISSIONS, "Couldn't find enough wallet addresses with mining permission to mine given number of blocks");
                    }
                    else
                    {
                        throw JSONRPCError(RPC_INSUFFICIENT_PERMISSIONS, "Couldn't find wallet address with mining permission");                        
                    }
                }
                else
                {
                    throw JSONRPCError(RPC_INTERNAL_ERROR, "Wallet keypool empty");
                }
            }
                
            int64_t nStart = GetTimeMillis();
            int64_t nCount=1;
            uint256 cachedMerkleRoot=0;    
            
            CBlock *pblock = &pblocktemplate->block;
            {
                LOCK(cs_main);
/* MCHN START */                
                IncrementExtraNonce(pblock, pindexPrev, nExtraNonce,pwalletMain);
                CreateBlockSignature(pblock,BLOCKSIGHASH_NO_SIGNATURE,pwalletMain,&cachedMerkleRoot);
//                IncrementExtraNonce(pblock, chainActive.Tip(), nExtraNonce);
/* MCHN START */                
            }
            while (!CheckProofOfWork(pblock->GetHash(), pblock->nBits, true)) {
                // Yes, there is a chance every nonce could fail to satisfy the -regtest
                // target -- 1 in 2^(2^32). That ain't gonna happen.
                ++pblock->nNonce;
                CreateBlockSignature(pblock,BLOCKSIGHASH_NO_SIGNATURE,pwalletMain,&cachedMerkleRoot);     
                nCount++;
            }
            if(Params().DisallowUnsignedBlockNonce())
            {
                pblock->hashMerkleRoot=pblock->BuildMerkleTree();                   
            }
            int64_t nEnd = GetTimeMillis();
            LogPrintf("RPC Miner      : %ld hashes were tried in %ldms (%8.3fh/ms)\n",nCount,nEnd-nStart,
                    (nEnd-nStart >0 ) ? (double)nCount/((double)nEnd-(double)nStart) : 0);
            CValidationState state;
            LogPrintf("RPC Miner      : Block Found - %s, prev: %s, height: %d, txs: %d\n",
                    pblock->GetHash().GetHex(),pblock->hashPrevBlock.ToString().c_str(),nHeight+1,(int)pblock->vtx.size());
            if (!ProcessNewBlock(state, NULL, pblock))
                throw JSONRPCError(RPC_INTERNAL_ERROR, "ProcessNewBlock, block not accepted");
            ++nHeight;
            blockHashes.push_back(pblock->GetHash().GetHex());
        }
        return blockHashes;
    }
    else // Not -regtest: start generate thread, return immediately
    {
        mapArgs["-gen"] = (fGenerate ? "1" : "0");
        mapArgs ["-genproclimit"] = itostr(nGenProcLimit);
        GenerateBitcoins(fGenerate, pwalletMain, nGenProcLimit);
    }

    return Value::null;
}

Value gethashespersec(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error("Help message not found\n");
/*
    if (GetTimeMillis() - nHPSTimerStart > 8000)
        return (int64_t)0;
 */ 
    return (int64_t)dHashesPerSec;
}
#endif


Value getmininginfo(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error("Help message not found\n");

    Object obj;
    obj.push_back(Pair("blocks",           (int)chainActive.Height()));
    obj.push_back(Pair("currentblocksize", (uint64_t)nLastBlockSize));
    obj.push_back(Pair("currentblocktx",   (uint64_t)nLastBlockTx));
    obj.push_back(Pair("difficulty",       (double)GetDifficulty()));
    obj.push_back(Pair("errors",           GetWarnings("statusbar")));
    obj.push_back(Pair("genproclimit",     (int)GetArg("-genproclimit", 1)));
    obj.push_back(Pair("networkhashps",    getnetworkhashps(params, false)));
    obj.push_back(Pair("pooledtx",         (uint64_t)mempool.size()));
    obj.push_back(Pair("testnet",          Params().TestnetToBeDeprecatedFieldRPC()));
/* MCHN START */    
//    obj.push_back(Pair("chain",            Params().NetworkIDString()));
    obj.push_back(Pair("chain",Params().TestnetToBeDeprecatedFieldRPC() ? "test" : "main"));
/* MCHN END */    
#ifdef ENABLE_WALLET
    obj.push_back(Pair("generate",         getgenerate(params, false)));
    obj.push_back(Pair("hashespersec",     gethashespersec(params, false)));
#endif
    return obj;
}


// NOTE: Unlike wallet RPC (which use BTC values), mining RPCs follow GBT (BIP 22) in using satoshi amounts
Value prioritisetransaction(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 3)
        throw runtime_error("Help message not found\n");

    throw JSONRPCError(RPC_NOT_SUPPORTED, "Transaction prioritization is not supported in this version of MultiChain");        
    
/* MCHN START */    
//    uint256 hash = ParseHashStr(params[0].get_str(), "txid");

//    CAmount nAmount = params[2].get_int64();

//    mempool.PrioritiseTransaction(hash, params[0].get_str(), params[1].get_real(), nAmount);
//    return true;
    return false;
/* MCHN END */    
}


// NOTE: Assumes a conclusive result; if result is inconclusive, it must be handled by caller
static Value BIP22ValidationResult(const CValidationState& state)
{
    if (state.IsValid())
        return Value::null;

    std::string strRejectReason = state.GetRejectReason();
    if (state.IsError())
        throw JSONRPCError(RPC_VERIFY_ERROR, strRejectReason);
    if (state.IsInvalid())
    {
        if (strRejectReason.empty())
            return "rejected";
        return strRejectReason;
    }
    // Should be impossible
    return "valid?";
}

Value getblocktemplate(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error("Help message not found\n");

    throw JSONRPCError(RPC_NOT_SUPPORTED, "getblocktemplate is not supported in this version of MultiChain");        
    
    std::string strMode = "template";
    Value lpval = Value::null;
    if (params.size() > 0)
    {
        const Object& oparam = params[0].get_obj();
        const Value& modeval = find_value(oparam, "mode");
        if (modeval.type() == str_type)
            strMode = modeval.get_str();
        else if (modeval.type() == null_type)
        {
            /* Do nothing */
        }
        else
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid mode");
        lpval = find_value(oparam, "longpollid");

        if (strMode == "proposal")
        {
            const Value& dataval = find_value(oparam, "data");
            if (dataval.type() != str_type)
                throw JSONRPCError(RPC_TYPE_ERROR, "Missing data String key for proposal");

            CBlock block;
            if (!DecodeHexBlk(block, dataval.get_str()))
                throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Block decode failed");

            uint256 hash = block.GetHash();
            BlockMap::iterator mi = mapBlockIndex.find(hash);
            if (mi != mapBlockIndex.end()) {
                CBlockIndex *pindex = mi->second;
                if (pindex->IsValid(BLOCK_VALID_SCRIPTS))
                    return "duplicate";
                if (pindex->nStatus & BLOCK_FAILED_MASK)
                    return "duplicate-invalid";
                return "duplicate-inconclusive";
            }

            CBlockIndex* const pindexPrev = chainActive.Tip();
            // TestBlockValidity only supports blocks built on the current Tip
            if (block.hashPrevBlock != pindexPrev->GetBlockHash())
                return "inconclusive-not-best-prevblk";
            CValidationState state;
            TestBlockValidity(state, block, pindexPrev, false, true);
            return BIP22ValidationResult(state);
        }
    }

    if (strMode != "template")
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid mode");

    if (vNodes.empty())
        throw JSONRPCError(RPC_CLIENT_NOT_CONNECTED, "MultiChain is not connected!");

    if (IsInitialBlockDownload())
        throw JSONRPCError(RPC_CLIENT_IN_INITIAL_DOWNLOAD, "MultiChain is downloading blocks...");

    static unsigned int nTransactionsUpdatedLast;

    if (lpval.type() != null_type)
    {
        // Wait to respond until either the best block changes, OR a minute has passed and there are more transactions
        uint256 hashWatchedChain;
        boost::system_time checktxtime;
        unsigned int nTransactionsUpdatedLastLP;

        if (lpval.type() == str_type)
        {
            // Format: <hashBestChain><nTransactionsUpdatedLast>
            std::string lpstr = lpval.get_str();

            hashWatchedChain.SetHex(lpstr.substr(0, 64));
            nTransactionsUpdatedLastLP = atoi64(lpstr.substr(64));
        }
        else
        {
            // NOTE: Spec does not specify behaviour for non-string longpollid, but this makes testing easier
            hashWatchedChain = chainActive.Tip()->GetBlockHash();
            nTransactionsUpdatedLastLP = nTransactionsUpdatedLast;
        }

        // Release the wallet and main lock while waiting
#ifdef ENABLE_WALLET
        if(pwalletMain)
            LEAVE_CRITICAL_SECTION(pwalletMain->cs_wallet);
#endif
        LEAVE_CRITICAL_SECTION(cs_main);
        {
            checktxtime = boost::get_system_time() + boost::posix_time::minutes(1);

            boost::unique_lock<boost::mutex> lock(csBestBlock);
            while (chainActive.Tip()->GetBlockHash() == hashWatchedChain && IsRPCRunning())
            {
                if (!cvBlockChange.timed_wait(lock, checktxtime))
                {
                    // Timeout: Check transactions for update
                    if (mempool.GetTransactionsUpdated() != nTransactionsUpdatedLastLP)
                        break;
                    checktxtime += boost::posix_time::seconds(10);
                }
            }
        }
        ENTER_CRITICAL_SECTION(cs_main);
#ifdef ENABLE_WALLET
        if(pwalletMain)
            ENTER_CRITICAL_SECTION(pwalletMain->cs_wallet);
#endif

        if (!IsRPCRunning())
            throw JSONRPCError(RPC_CLIENT_NOT_CONNECTED, "Shutting down");
        // TODO: Maybe recheck connections/IBD and (if something wrong) send an expires-immediately template to stop miners?
    }

    // Update block
    static CBlockIndex* pindexPrev;
    static int64_t nStart;
    static CBlockTemplate* pblocktemplate;
    if (pindexPrev != chainActive.Tip() ||
        (mempool.GetTransactionsUpdated() != nTransactionsUpdatedLast && GetTime() - nStart > 5))
    {
        // Clear pindexPrev so future calls make a new block, despite any failures from here on
        pindexPrev = NULL;

        // Store the pindexBest used before CreateNewBlock, to avoid races
        nTransactionsUpdatedLast = mempool.GetTransactionsUpdated();
        CBlockIndex* pindexPrevNew = chainActive.Tip();
        nStart = GetTime();

        // Create new block
        if(pblocktemplate)
        {
            delete pblocktemplate;
            pblocktemplate = NULL;
        }
        CScript scriptDummy = CScript() << OP_TRUE;
        pblocktemplate = CreateNewBlock(scriptDummy);
        if (!pblocktemplate)
            throw JSONRPCError(RPC_OUT_OF_MEMORY, "Out of memory");

        // Need to update only after we know CreateNewBlock succeeded
        pindexPrev = pindexPrevNew;
    }
    CBlock* pblock = &pblocktemplate->block; // pointer for convenience

    // Update nTime
    UpdateTime(pblock, pindexPrev);
    pblock->nNonce = 0;

    static const Array aCaps = boost::assign::list_of("proposal");

    Array transactions;
    map<uint256, int64_t> setTxIndex;
    int i = 0;
    BOOST_FOREACH (CTransaction& tx, pblock->vtx)
    {
        uint256 txHash = tx.GetHash();
        setTxIndex[txHash] = i++;

        if (tx.IsCoinBase())
            continue;

        Object entry;

        entry.push_back(Pair("data", EncodeHexTx(tx)));

        entry.push_back(Pair("hash", txHash.GetHex()));

        Array deps;
        BOOST_FOREACH (const CTxIn &in, tx.vin)
        {
            if (setTxIndex.count(in.prevout.hash))
                deps.push_back(setTxIndex[in.prevout.hash]);
        }
        entry.push_back(Pair("depends", deps));

        int index_in_template = i - 1;
        entry.push_back(Pair("fee", pblocktemplate->vTxFees[index_in_template]));
        entry.push_back(Pair("sigops", pblocktemplate->vTxSigOps[index_in_template]));

        transactions.push_back(entry);
    }

    Object aux;
    aux.push_back(Pair("flags", HexStr(COINBASE_FLAGS.begin(), COINBASE_FLAGS.end())));

    uint256 hashTarget = uint256().SetCompact(pblock->nBits);

    static Array aMutable;
    if (aMutable.empty())
    {
        aMutable.push_back("time");
        aMutable.push_back("transactions");
        aMutable.push_back("prevblock");
    }

    Object result;
    result.push_back(Pair("capabilities", aCaps));
    result.push_back(Pair("version", pblock->nVersion));
    result.push_back(Pair("previousblockhash", pblock->hashPrevBlock.GetHex()));
    result.push_back(Pair("transactions", transactions));
    result.push_back(Pair("coinbaseaux", aux));
    result.push_back(Pair("coinbasevalue", (int64_t)pblock->vtx[0].vout[0].nValue));
    result.push_back(Pair("longpollid", chainActive.Tip()->GetBlockHash().GetHex() + i64tostr(nTransactionsUpdatedLast)));
    result.push_back(Pair("target", hashTarget.GetHex()));
    result.push_back(Pair("mintime", (int64_t)pindexPrev->GetMedianTimePast()+1));
    result.push_back(Pair("mutable", aMutable));
    result.push_back(Pair("noncerange", "00000000ffffffff"));
    result.push_back(Pair("sigoplimit", (int64_t)MAX_BLOCK_SIGOPS));
    result.push_back(Pair("sizelimit", (int64_t)MAX_BLOCK_SIZE));
    result.push_back(Pair("curtime", pblock->GetBlockTime()));
    result.push_back(Pair("bits", strprintf("%08x", pblock->nBits)));
    result.push_back(Pair("height", (int64_t)(pindexPrev->nHeight+1)));

    return result;
}

class submitblock_StateCatcher : public CValidationInterface
{
public:
    uint256 hash;
    bool found;
    CValidationState state;

    submitblock_StateCatcher(const uint256 &hashIn) : hash(hashIn), found(false), state() {};

protected:
    virtual void BlockChecked(const CBlock& block, const CValidationState& stateIn) {
        if (block.GetHash() != hash)
            return;
        found = true;
        state = stateIn;
    };
};

Value submitblock(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error("Help message not found\n");

    CBlock block;
    if (!DecodeHexBlk(block, params[0].get_str()))
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Block decode failed");

    uint256 hash = block.GetHash();

    bool fBlockPresent = false;
    {
         LOCK(cs_main);    
         
        BlockMap::iterator mi = mapBlockIndex.find(hash);
        if (mi != mapBlockIndex.end()) {
            CBlockIndex *pindex = mi->second;
            if (pindex->IsValid(BLOCK_VALID_SCRIPTS))
                return "duplicate";
            if (pindex->nStatus & BLOCK_FAILED_MASK)
                return "duplicate-invalid";
            // Otherwise, we might only have the header - process the block before returning
        }
        fBlockPresent=true;
    }

    CValidationState state;
    submitblock_StateCatcher sc(block.GetHash());
    RegisterValidationInterface(&sc);
    bool fAccepted = ProcessNewBlock(state, NULL, &block);
    UnregisterValidationInterface(&sc);
    
    if (fBlockPresent)        
    {
        if (fAccepted && !sc.found)
            return "duplicate-inconclusive";
        return "duplicate";
    }
    if (fAccepted)
    {
        if (!sc.found)
            return "inconclusive";
        state = sc.state;
    }
    return BIP22ValidationResult(state);
}

Value estimatefee(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error("Help message not found\n");

    RPCTypeCheck(params, boost::assign::list_of(int_type));

    int nBlocks = params[0].get_int();
    if (nBlocks < 1)
        nBlocks = 1;

    CFeeRate feeRate = mempool.estimateFee(nBlocks);
    if (feeRate == CFeeRate(0))
        return -1.0;

    return ValueFromAmount(feeRate.GetFeePerK());
}

Value estimatepriority(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error("Help message not found\n");

    RPCTypeCheck(params, boost::assign::list_of(int_type));

    int nBlocks = params[0].get_int();
    if (nBlocks < 1)
        nBlocks = 1;

    return mempool.estimatePriority(nBlocks);
}
