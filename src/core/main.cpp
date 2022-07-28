// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "core/main.h"

#include "storage/addrman.h"
#include "structs/alert.h"
#include "chainparams/chainparams.h"
#include "chain/checkpoints.h"
#include "checkqueue.h"
#include "core/init.h"
#include "chain/merkleblock.h"
#include "net/net.h"
#include "chain/pow.h"
#include "storage/txdb.h"
#include "chain/txmempool.h"
#include "ui/ui_interface.h"
#include "utils/util.h"
#include "utils/utilmoneystr.h"

/* MCHN START */

#include "structs/base58.h"
#include "keys/pubkey.h"
#include "keys/key.h"
#include "wallet/wallet.h"
#include "multichain/multichain.h"
#include "community/community.h"
#include "wallet/wallettxs.h"
#include "script/script.h"
#include "protocol/relay.h"


extern mc_WalletTxs* pwalletTxsMain;
extern mc_RelayManager* pRelayManager;
extern mc_FilterEngine* pFilterEngine;
extern mc_MultiChainFilterEngine* pMultiChainFilterEngine;

/* MCHN END */

#include <sstream>

#include <boost/algorithm/string/replace.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/thread.hpp>

using namespace boost;
using namespace std;

bool Is_MultiChainLicenseTokenTransfer(const CTransaction& tx);
bool ExtractDestinationScriptValid(const CScript& scriptPubKey, CTxDestination& addressRet);
bool AcceptAssetTransfers(const CTransaction& tx, const CCoinsViewCache &inputs, string& reason);
bool AcceptAssetGenesis(const CTransaction &tx,int offset,bool accept,string& reason);
bool AcceptPermissionsAndCheckForDust(const CTransaction &tx,bool accept,string& reason);
bool ReplayMemPool(CTxMemPool& pool, int from,bool accept);
int64_t TotalMempoolsSize();
int64_t TotalMempoolTxSize();
bool VerifyBlockSignatureType(CBlock *block);
bool VerifyBlockSignature(CBlock *block,bool force);
bool VerifyBlockMiner(CBlock *block,CBlockIndex* pindexNew);
bool CheckBlockPermissions(const CBlock& block,CBlockIndex* prev_block,unsigned char *lpMinerAddress);
bool ProcessMultichainRelay(CNode* pfrom, CDataStream& vRecv, CValidationState &state);
bool ProcessMultichainVerack(CNode* pfrom, CDataStream& vRecv,bool fIsVerackack,bool *disconnect_flag);
bool PushMultiChainVerack(CNode* pfrom, bool fIsVerackack);
bool MultichainNode_CanConnect(CNode *pnode);
bool MultichainNode_DisconnectRemote(CNode *pnode);
bool MultichainNode_DisconnectLocal(CNode *pnode);
bool MultichainNode_RespondToGetData(CNode *pnode);
bool MultichainNode_SendInv(CNode *pnode);
bool MultichainNode_AcceptData(CNode *pnode);
bool MultichainNode_IgnoreIncoming(CNode *pnode);
bool MultichainNode_IsLocal(CNode *pnode);
bool MultichainNode_CollectChunks();
bool IsTxBanned(uint256 txid);
int CreateUpgradeLists(int current_height,vector<mc_UpgradedParameter> *vParams,vector<mc_UpgradeStatus> *vUpgrades);
void SetRPCNewTxFlag();




#if defined(NDEBUG)
# error "Bitcoin cannot be compiled without assertions."
#endif

/**
 * Global state
 */

CCriticalSection cs_main;

BlockMap mapBlockIndex;
CChain chainActive;
CBlockIndex *pindexBestHeader = NULL;
int64_t nTimeBestReceived = 0;
CWaitableCriticalSection csBestBlock;
CConditionVariable cvBlockChange;
int nScriptCheckThreads = 0;
bool fImporting = false;
bool fReindex = false;
bool fRescan = false;
bool fTxIndex = false;
bool fAcceptOnlyRequestedTxs = false;
bool fIsBareMultisigStd = true;
unsigned int nCoinCacheSize = 5000;
int GenesisBlockSize=0;
int nLastForkedHeight=0;
int64_t nTotalMempoolsSize=0;
int64_t nTotalNodeBuffersSize=0;
double dBlockProcessingTime[100];
int nBlockTimeMeasured=0;
double dAverageBlockTime=0;
uint256 GenesisCoinBaseTxID=0;
uint32_t nMessageHandlerThreads=MC_MHT_DEFAULT;
CTransaction GenesisCoinBaseTx;

vector<CBlockIndex*> vFirstOnThisHeight;

/** Fees smaller than this (in satoshi) are considered zero fee (for relaying and mining) */
/* MCHN START */
//CFeeRate minRelayTxFee = CFeeRate(1000);
CFeeRate minRelayTxFee = CFeeRate(MIN_RELAY_TX_FEE);
/* MCHN END */

CTxMemPool mempool(::minRelayTxFee);

struct COrphanTx {
    CTransaction tx;
    NodeId fromPeer;
    int64_t time;
    int32_t size;
};
int OrphanHandlerVersion=1;
int32_t nOrphanPoolSize=0;
map<uint256, COrphanTx> mapOrphanTransactions;
map<uint256, set<uint256> > mapOrphanTransactionsByPrev;
set <uint256> setBannedTxs;
set <uint256> setBannedTxBlocks;
uint256 hLockedBlock;
CBlockIndex *pindexLockedBlock;

int EraseOrphansFor(NodeId peer);

/* MCHN START */
#define MC_TXSET_BLOCKS 50
set<uint256> setBlockTransactions[MC_TXSET_BLOCKS];
/* MCHN END */

/** Constant stuff for coinbase transactions we create: */
CScript COINBASE_FLAGS;

const string strMessageMagic = "MultiChain Signed Message:\n";

// Internal stuff
namespace {

    struct CBlockIndexWorkComparator
    {
        bool operator()(CBlockIndex *pa, CBlockIndex *pb) {
            // First sort by most total work, ...
            if (pa->nChainWork > pb->nChainWork) return false;
            if (pa->nChainWork < pb->nChainWork) return true;
            
            
/* MCHN START */
            // Prefer chains we mined long time ago 
            if(mc_gState->m_NetworkParams->IsProtocolMultichain())
            {
                if((pa->nCanMine > 0) && (pb->nCanMine == 0)) return false;
                if((pa->nCanMine == 0) && (pb->nCanMine > 0)) return true;
                if((pa->nCanMine == 0) && (pb->nCanMine == 0))
                {
                    if (pa->nHeightMinedByMe < pb->nHeightMinedByMe) return false;
                    if (pa->nHeightMinedByMe > pb->nHeightMinedByMe) return true;
                }
            }
/* MCHN END */
            
            // ... then by earliest time received, ...
            if (pa->nSequenceId < pb->nSequenceId) return false;
            if (pa->nSequenceId > pb->nSequenceId) return true;

            // Use pointer address as tie breaker (should only happen with blocks
            // loaded from disk, as those all have id 0).
            if (pa < pb) return false;
            if (pa > pb) return true;

            // Identical blocks.
            return false;
        }
    };

    CBlockIndex *pindexBestInvalid;

    /**
     * The set of all CBlockIndex entries with BLOCK_VALID_TRANSACTIONS or better that are at least
     * as good as our current tip. Entries may be failed, though.
     */
    set<CBlockIndex*, CBlockIndexWorkComparator> setBlockIndexCandidates;
    /** Number of nodes with fSyncStarted. */
    int nSyncStarted = 0;
    /** All pairs A->B, where A (or one if its ancestors) misses transactions, but B has transactions. */
    multimap<CBlockIndex*, CBlockIndex*> mapBlocksUnlinked;

    CCriticalSection cs_LastBlockFile;
    std::vector<CBlockFileInfo> vinfoBlockFile;
    int nLastBlockFile = 0;

    /**
     * Every received block is assigned a unique and increasing identifier, so we
     * know which one to give priority in case of a fork.
     */
    CCriticalSection cs_nBlockSequenceId;
    /** Blocks loaded from disk are assigned id 0, so start the counter at 1. */
    uint32_t nBlockSequenceId = 1;

    /**
     * Sources of received blocks, to be able to send them reject messages or ban
     * them, if processing happens afterwards. Protected by cs_main.
     */
    map<uint256, NodeId> mapBlockSource;

    /** Blocks that are in flight, and that are in the queue to be downloaded. Protected by cs_main. */
    struct QueuedBlock {
        uint256 hash;
        CBlockIndex *pindex;  //! Optional.
        int64_t nTime;  //! Time of "getdata" request in microseconds.
        int nValidatedQueuedBefore;  //! Number of blocks queued with validated headers (globally) at the time this one is requested.
        bool fValidatedHeaders;  //! Whether this block has validated headers at the time of request.
    };
    map<uint256, pair<NodeId, list<QueuedBlock>::iterator> > mapBlocksInFlight;

    /** Number of blocks in flight with validated headers. */
    int nQueuedValidatedHeaders = 0;

    /** Number of preferable block download peers. */
    int nPreferredDownload = 0;

    /** Dirty block index entries. */
    set<CBlockIndex*> setDirtyBlockIndex;

    /** Dirty block file entries. */
    set<int> setDirtyFileInfo;
} // anon namespace

//////////////////////////////////////////////////////////////////////////////
//
// dispatching functions
//

// These functions dispatch to one or all registered wallets

namespace {

struct CMainSignals {
    /** Notifies listeners of updated transaction data (transaction, and optionally the block it is found in. */
    boost::signals2::signal<void (const CTransaction &, const CBlock *)> SyncTransaction;
    /** Notifies listeners of an erased transaction (currently disabled, requires transaction replacement). */
    boost::signals2::signal<void (const uint256 &)> EraseTransaction;
    /** Notifies listeners of an updated transaction without new data (for now: a coinbase potentially becoming visible). */
    boost::signals2::signal<void (const uint256 &)> UpdatedTransaction;
    /** Notifies listeners of a new active block chain. */
    boost::signals2::signal<void (const CBlockLocator &)> SetBestChain;
    /** Notifies listeners about an inventory item being seen on the network. */
    boost::signals2::signal<void (const uint256 &)> Inventory;
    /** Tells listeners to broadcast their data. */
    boost::signals2::signal<void (bool fForce)> Broadcast;
    /** Notifies listeners of a block validation result */
    boost::signals2::signal<void (const CBlock&, const CValidationState&)> BlockChecked;
} g_signals;

} // anon namespace

void RegisterValidationInterface(CValidationInterface* pwalletIn) {
    g_signals.SyncTransaction.connect(boost::bind(&CValidationInterface::SyncTransaction, pwalletIn, _1, _2));
    g_signals.EraseTransaction.connect(boost::bind(&CValidationInterface::EraseFromWallet, pwalletIn, _1));
    g_signals.UpdatedTransaction.connect(boost::bind(&CValidationInterface::UpdatedTransaction, pwalletIn, _1));
    g_signals.SetBestChain.connect(boost::bind(&CValidationInterface::SetBestChain, pwalletIn, _1));
    g_signals.Inventory.connect(boost::bind(&CValidationInterface::Inventory, pwalletIn, _1));
    g_signals.Broadcast.connect(boost::bind(&CValidationInterface::ResendWalletTransactions, pwalletIn, _1));
    g_signals.BlockChecked.connect(boost::bind(&CValidationInterface::BlockChecked, pwalletIn, _1, _2));
}

void UnregisterValidationInterface(CValidationInterface* pwalletIn) {
    g_signals.BlockChecked.disconnect(boost::bind(&CValidationInterface::BlockChecked, pwalletIn, _1, _2));
    g_signals.Broadcast.disconnect(boost::bind(&CValidationInterface::ResendWalletTransactions, pwalletIn, _1));
    g_signals.Inventory.disconnect(boost::bind(&CValidationInterface::Inventory, pwalletIn, _1));
    g_signals.SetBestChain.disconnect(boost::bind(&CValidationInterface::SetBestChain, pwalletIn, _1));
    g_signals.UpdatedTransaction.disconnect(boost::bind(&CValidationInterface::UpdatedTransaction, pwalletIn, _1));
    g_signals.EraseTransaction.disconnect(boost::bind(&CValidationInterface::EraseFromWallet, pwalletIn, _1));
    g_signals.SyncTransaction.disconnect(boost::bind(&CValidationInterface::SyncTransaction, pwalletIn, _1, _2));
}

void UnregisterAllValidationInterfaces() {
    g_signals.BlockChecked.disconnect_all_slots();
    g_signals.Broadcast.disconnect_all_slots();
    g_signals.Inventory.disconnect_all_slots();
    g_signals.SetBestChain.disconnect_all_slots();
    g_signals.UpdatedTransaction.disconnect_all_slots();
    g_signals.EraseTransaction.disconnect_all_slots();
    g_signals.SyncTransaction.disconnect_all_slots();
}

void SyncWithWallets(const CTransaction &tx, const CBlock *pblock) {
    g_signals.SyncTransaction(tx, pblock);
}

//////////////////////////////////////////////////////////////////////////////
//
// Registration of network node signals.
//

namespace {

struct CBlockReject {
    unsigned char chRejectCode;
    string strRejectReason;
    uint256 hashBlock;
};

/**
 * Maintain validation-specific state about nodes, protected by cs_main, instead
 * by CNode's own locks. This simplifies asynchronous operation, where
 * processing of incoming data is done after the ProcessMessage call returns,
 * and we're no longer holding the node's locks.
 */
struct CNodeState {
    //! Accumulated misbehaviour score for this peer.
    int nMisbehavior;
    //! Whether this peer should be disconnected and banned (unless whitelisted).
    bool fShouldBan;
    //! String name of this peer (debugging/logging purposes).
    std::string name;
    //! List of asynchronously-determined block rejections to notify this peer about.
    std::vector<CBlockReject> rejects;
    //! The best known block we know this peer has announced.
    CBlockIndex *pindexBestKnownBlock;
    //! The hash of the last unknown block this peer has announced.
    uint256 hashLastUnknownBlock;
    //! The last full block we both have.
    CBlockIndex *pindexLastCommonBlock;
    //! Whether we've started headers synchronization with this peer.
    bool fSyncStarted;
    //! Since when we're stalling block download progress (in microseconds), or 0.
    int64_t nStallingSince;
    list<QueuedBlock> vBlocksInFlight;
    int nBlocksInFlight;
    //! Whether we consider this a preferred download peer.
    bool fPreferredDownload;

    CNodeState() {
        nMisbehavior = 0;
        fShouldBan = false;
        pindexBestKnownBlock = NULL;
        hashLastUnknownBlock = uint256(0);
        pindexLastCommonBlock = NULL;
        fSyncStarted = false;
        nStallingSince = 0;
        nBlocksInFlight = 0;
        fPreferredDownload = false;
    }
};

/** Map maintaining per-node state. Requires cs_main. */
map<NodeId, CNodeState> mapNodeState;

/* MCHN START */

int SetUpgradedParamValue(const mc_OneMultichainParam *param,int64_t value)
{
    if(mc_gState->m_Features->ParameterUpgrades() == 0)
    {
        return MC_ERR_NOERROR;        
    }
    
    if(strcmp(param->m_Name,"maximumblocksize") == 0)
    {
        MAX_BLOCK_SIZE=(unsigned int)value;    
        DEFAULT_BLOCK_MAX_SIZE=MAX_BLOCK_SIZE;    
        while(MAX_BLOCK_SIZE>MAX_BLOCKFILE_SIZE)
        {
            MAX_BLOCKFILE_SIZE *= 2;
        }
        while(MAX_BLOCK_SIZE>MAX_SIZE)
        {
            MAX_SIZE *= 2;
        }
        MAX_BLOCK_SIGOPS = MAX_BLOCK_SIZE/50;
        MAX_TX_SIGOPS = MAX_BLOCK_SIGOPS/5;        
    }

    if(strcmp(param->m_Name,"targetblocktime") == 0)
    {
        MCP_TARGET_BLOCK_TIME=value;
        SetMultiChainParam("targetblocktime",value);
    }   
    
    if(strcmp(param->m_Name,"maxstdtxsize") == 0)
    {
        MAX_STANDARD_TX_SIZE=value;
    }   
    
    if(strcmp(param->m_Name,"maxstdopreturnscount") == 0)
    {
        MCP_MAX_STD_OP_RETURN_COUNT=value;
    }   
    
    if(strcmp(param->m_Name,"maxstdopreturnsize") == 0)
    {
        MAX_OP_RETURN_RELAY=value;    
        MAX_OP_RETURN_RELAY=GetArg("-datacarriersize", MAX_OP_RETURN_RELAY);
    }   
    
    if(strcmp(param->m_Name,"maxstdopdropscount") == 0)
    {
        MCP_STD_OP_DROP_COUNT=value;
        pwalletMain->InitializeUnspentList();        
    }   
    
    if(strcmp(param->m_Name,"maxstdelementsize") == 0)
    {
        MAX_SCRIPT_ELEMENT_SIZE=value;
        pwalletMain->InitializeUnspentList();        
    }   
    
    if(strcmp(param->m_Name,"maximumchunksize") == 0)
    {
        int old_value=MAX_CHUNK_SIZE;
        MAX_CHUNK_SIZE=(unsigned int)value;    

        while(MAX_CHUNK_SIZE+OFFCHAIN_MSG_PADDING>MAX_SIZE)
        {
            MAX_SIZE *= 2;
        }

        if(MAX_CHUNK_SIZE > old_value)
        {
            if(pwalletTxsMain)
            {
                if(pwalletTxsMain->m_ChunkBuffer)
                {
                    mc_Delete(pwalletTxsMain->m_ChunkBuffer);
                    pwalletTxsMain->m_ChunkBuffer=(unsigned char*)mc_New(MAX_CHUNK_SIZE);                        
                }
            }
        }
    }

    if(strcmp(param->m_Name,"maximumchunkcount") == 0)
    {
        MAX_CHUNK_COUNT=value;
    }           
    
    if(strcmp(param->m_Name,"anyonecanconnect") == 0)
    {
        MCP_ANYONE_CAN_CONNECT=value;
    }   
    
    if(strcmp(param->m_Name,"anyonecansend") == 0)
    {
        MCP_ANYONE_CAN_SEND=value;
    }   
    
    if(strcmp(param->m_Name,"anyonecanreceive") == 0)
    {
        MCP_ANYONE_CAN_RECEIVE=value;
    }   
    
    if(strcmp(param->m_Name,"anyonecanreceiveempty") == 0)
    {
        MCP_ANYONE_CAN_RECEIVE_EMPTY=value;
    }   
    
    if(strcmp(param->m_Name,"anyonecancreate") == 0)
    {
        MCP_ANYONE_CAN_CREATE=value;
    }   
    
    if(strcmp(param->m_Name,"anyonecanissue") == 0)
    {
        MCP_ANYONE_CAN_ISSUE=value;
    }   
    
    if(strcmp(param->m_Name,"anyonecanactivate") == 0)
    {
        MCP_ANYONE_CAN_ACTIVATE=value;
    }   
    
    return MC_ERR_NOERROR;
}

int MultichainNode_ApplyUpgrades(int current_height)
{
    vector<mc_UpgradedParameter> vParams;
    int err=MC_ERR_NOERROR;
    
    err=CreateUpgradeLists(current_height,&vParams,NULL);
    
    int OriginalProtocolVersion=(int)mc_gState->m_NetworkParams->GetInt64Param("protocolversion");
    int CurrentProtocolVersion=mc_gState->m_NetworkParams->ProtocolVersion();//mc_gState->m_ProtocolVersionToUpgrade;
    
    mc_gState->m_NetworkParams->m_ProtocolVersion=OriginalProtocolVersion;
    mc_gState->m_NetworkParams->SetGlobals();
    for(int p=0;p<(int)vParams.size();p++)
    {
        if(vParams[p].m_Skipped == MC_PSK_APPLIED)
        {
            if(strcmp(vParams[p].m_Param->m_Name,"protocolversion") == 0)
            {
                mc_gState->m_NetworkParams->m_ProtocolVersion=(int)vParams[p].m_Value;
                mc_gState->m_NetworkParams->SetProtocolGlobals();
            }
            else
            {
                SetUpgradedParamValue(vParams[p].m_Param,vParams[p].m_Value);
            }
        }
    }
    SetMultiChainParams();            
    if(pMultiChainFilterEngine)                                                 // Added from version 20012. 
    {
        pMultiChainFilterEngine->SetCallbackNames();
    }
    mc_gState->m_ProtocolVersionToUpgrade=mc_gState->m_NetworkParams->m_ProtocolVersion;
    
    
    if(mc_gState->m_ProtocolVersionToUpgrade != CurrentProtocolVersion)
    {
        LogPrintf("New protocol upgrade version: %d (was %d)\n",mc_gState->m_ProtocolVersionToUpgrade,CurrentProtocolVersion);
        if( (err == MC_ERR_NOT_SUPPORTED) || ((mc_gState->m_ProtocolVersionToUpgrade > 0) && (mc_gState->IsSupported(mc_gState->m_ProtocolVersionToUpgrade) == 0)) )
        {
            mc_gState->m_NetworkParams->m_ProtocolVersion=CurrentProtocolVersion;
            LogPrintf("NODE SHOULD BE UPGRADED FROM %d TO %d\n",mc_gState->GetProtocolVersion(),mc_gState->m_ProtocolVersionToUpgrade);
        }
        else
        {
            LogPrintf("NODE IS UPGRADED FROM %d TO %d\n",CurrentProtocolVersion,mc_gState->m_ProtocolVersionToUpgrade);
/*            
            if(mc_gState->m_ProtocolVersionToUpgrade != mc_gState->m_NetworkParams->ProtocolVersion())
            {
                LogPrintf("NODE IS UPGRADED FROM %d TO %d\n",mc_gState->m_NetworkParams->ProtocolVersion(),mc_gState->m_ProtocolVersionToUpgrade);
            }        
 */ 
        }
    }
    else
    {
        mc_gState->m_ProtocolVersionToUpgrade=0;        
    }
    
    return MC_ERR_NOERROR;
}


void MultichainNode_UpdateBlockByHeightList(CBlockIndex *pindex)
{
    if(pindex->nHeight < 0)
    {
        return;
    }
    unsigned int old_size=vFirstOnThisHeight.size();
    if(pindex->nHeight + 1 > (int)old_size)
    {
        vFirstOnThisHeight.resize(pindex->nHeight + 1);
        for(unsigned int i=old_size+1;i<(unsigned int)(pindex->nHeight+1);i++)
        {
            vFirstOnThisHeight[i]=NULL;
        }
    }
    if(vFirstOnThisHeight[pindex->nHeight])
    {
        CBlockIndex *pTmp;
        pTmp=vFirstOnThisHeight[pindex->nHeight];
        while(pTmp->pNextOnThisHeight)
        {
            pTmp=pTmp->pNextOnThisHeight;
        }
        pTmp->pNextOnThisHeight=pindex;
    }
    else
    {
        vFirstOnThisHeight[pindex->nHeight]=pindex;
    }    
}

bool PushMempoolToInv(CNode *pnode)
{
    if(OrphanHandlerVersion != 1)    
        return false;
        
    if(!pnode->fRelayTxes)
        return false;
    
    {
        LOCK(pnode->cs_filter);
        if( (pnode->pfilter != NULL) && (!pnode->pfilter->IsFull()) )
            return false;
    }    
    
    LOCK2(pnode->cs_inventory,mempool.cs);    
    
    int push_count=0;
    std::vector<uint256> vtxid;
    mempool.queryHashes(vtxid);
    vector<CInv> vInv;
    BOOST_FOREACH(uint256& hash, vtxid) {
        CInv inv(MSG_TX, hash);
        pnode->PushInventory(inv);
        push_count++;
    }
    
    pnode->fReadyForTxInv=true;
    LogPrintf("mchn: pushed %d inv records to queue\n",push_count);
    return true;
}

bool MultichainNode_IsBlockChainSynced(CNode *pnode)
{
    if(pnode->fSyncedOnce)
    {
        return true;
    }
    
    int this_height=(int)chainActive.Height();
    map<NodeId, CNodeState>::iterator it = mapNodeState.find(pnode->id);
    if (it == mapNodeState.end())
        return false;
    CNodeState *state = &it->second;
    int sync_height = state->pindexBestKnownBlock ? state->pindexBestKnownBlock->nHeight : -1;
    int common_height = state->pindexLastCommonBlock ? state->pindexLastCommonBlock->nHeight : -1;

    if(common_height <= 0)
    {
        if(state->pindexBestKnownBlock)
        {
            if(chainActive.Contains(state->pindexBestKnownBlock))
            {
                common_height=sync_height;
                LogPrint("mcblockperf","mchn-block-perf: Lagging node %d, heights: our: %d,  known: %d\n",pnode->id,this_height,sync_height);        
            }
        }
        else
        {
            if(pnode->fEmptyHeaders)
            {
                pnode->fSyncedOnce=true;
                LogPrint("mcblockperf","mchn-block-perf: Lagging node %d, received empty headers, considered synced\n",pnode->id);  
                return pnode->fSyncedOnce;
            }
        }
    }
    
    if((this_height > 0) && (common_height>0))
    {
        if(this_height == common_height)
        {
            if(sync_height == common_height)
            {
                if(!pnode->fReadyForTxInv)
                {
                    LogPrintf("mchn: In sync with %d on block %d - start pushing txs\n",pnode->id,mc_gState->m_Permissions->m_Block);
                    PushMempoolToInv(pnode);
                }        
                pnode->fSyncedOnce=true;
            }
        }
        if((this_height > common_height) && (this_height > sync_height))
        {
            pnode->fSyncedOnce=true;
        }
    }
    
    if(pnode->fSyncedOnce)
    {
        LogPrint("mcblockperf","mchn-block-perf: Synced with node %d, heights: our: %d, common: %d, known: %d\n",pnode->id,this_height,common_height,sync_height);        
    }
    
    return pnode->fSyncedOnce;
}
        
/* MCHN END */


// Requires cs_main.
CNodeState *State(NodeId pnode) {
    map<NodeId, CNodeState>::iterator it = mapNodeState.find(pnode);
    if (it == mapNodeState.end())
        return NULL;
    return &it->second;
}

int GetHeight()
{
    LOCK(cs_main);
    return chainActive.Height();
}

void UpdatePreferredDownload(CNode* node, CNodeState* state)
{
    nPreferredDownload -= state->fPreferredDownload;

    // Whether this node should be marked as a preferred download node.
    state->fPreferredDownload = (!node->fInbound || node->fWhitelisted) && !node->fOneShot && !node->fClient;

    nPreferredDownload += state->fPreferredDownload;
}

void InitializeNode(NodeId nodeid, const CNode *pnode) {
    LOCK(cs_main);
    CNodeState &state = mapNodeState.insert(std::make_pair(nodeid, CNodeState())).first->second;
    state.name = pnode->addrName;
}

void FinalizeNode(NodeId nodeid) {
    LOCK(cs_main);
    CNodeState *state = State(nodeid);

    if (state->fSyncStarted)
        nSyncStarted--;

    BOOST_FOREACH(const QueuedBlock& entry, state->vBlocksInFlight)
        mapBlocksInFlight.erase(entry.hash);
    EraseOrphansFor(nodeid);
    nPreferredDownload -= state->fPreferredDownload;

    mapNodeState.erase(nodeid);
}

// Requires cs_main.
void MarkBlockAsReceived(const uint256& hash) {
    map<uint256, pair<NodeId, list<QueuedBlock>::iterator> >::iterator itInFlight = mapBlocksInFlight.find(hash);
    if (itInFlight != mapBlocksInFlight.end()) {
        CNodeState *state = State(itInFlight->second.first);
        nQueuedValidatedHeaders -= itInFlight->second.second->fValidatedHeaders;
        state->vBlocksInFlight.erase(itInFlight->second.second);
        state->nBlocksInFlight--;
        state->nStallingSince = 0;
        mapBlocksInFlight.erase(itInFlight);
    }
}

// Requires cs_main.
void MarkBlockAsInFlight(NodeId nodeid, const uint256& hash, CBlockIndex *pindex = NULL) {
    CNodeState *state = State(nodeid);
    assert(state != NULL);

    // Make sure it's not listed somewhere already.
    MarkBlockAsReceived(hash);

    QueuedBlock newentry = {hash, pindex, GetTimeMicros(), nQueuedValidatedHeaders, pindex != NULL};
    nQueuedValidatedHeaders += newentry.fValidatedHeaders;
    list<QueuedBlock>::iterator it = state->vBlocksInFlight.insert(state->vBlocksInFlight.end(), newentry);
    state->nBlocksInFlight++;
    mapBlocksInFlight[hash] = std::make_pair(nodeid, it);
}

/** Check whether the last unknown block a peer advertized is not yet known. */
void ProcessBlockAvailability(NodeId nodeid) {
    CNodeState *state = State(nodeid);
    assert(state != NULL);

    if (state->hashLastUnknownBlock != 0) {
        BlockMap::iterator itOld = mapBlockIndex.find(state->hashLastUnknownBlock);
        if (itOld != mapBlockIndex.end() && itOld->second->nChainWork > 0) {
            if (state->pindexBestKnownBlock == NULL || itOld->second->nChainWork >= state->pindexBestKnownBlock->nChainWork)
                state->pindexBestKnownBlock = itOld->second;
            state->hashLastUnknownBlock = uint256(0);
        }
    }
}

/** Update tracking information about which blocks a peer is assumed to have. */
void UpdateBlockAvailability(NodeId nodeid, const uint256 &hash) {
    CNodeState *state = State(nodeid);
    assert(state != NULL);

    ProcessBlockAvailability(nodeid);

    BlockMap::iterator it = mapBlockIndex.find(hash);
    if (it != mapBlockIndex.end() && it->second->nChainWork > 0) {
        // An actually better block was announced.
        if (state->pindexBestKnownBlock == NULL || it->second->nChainWork >= state->pindexBestKnownBlock->nChainWork)
            state->pindexBestKnownBlock = it->second;
    } else {
        // An unknown block was announced; just assume that the latest one is the best one.
        state->hashLastUnknownBlock = hash;
    }
}

/** Find the last common ancestor two blocks have.
 *  Both pa and pb must be non-NULL. */
CBlockIndex* LastCommonAncestor(CBlockIndex* pa, CBlockIndex* pb) {
    if (pa->nHeight > pb->nHeight) {
        pa = pa->GetAncestor(pb->nHeight);
    } else if (pb->nHeight > pa->nHeight) {
        pb = pb->GetAncestor(pa->nHeight);
    }

    while (pa != pb && pa && pb) {
        pa = pa->pprev;
        pb = pb->pprev;
    }

    // Eventually all chain branches meet at the genesis block.
    assert(pa == pb);
    return pa;
}

/** Update pindexLastCommonBlock and add not-in-flight missing successors to vBlocks, until it has
 *  at most count entries. */
void FindNextBlocksToDownload(NodeId nodeid, unsigned int count, std::vector<CBlockIndex*>& vBlocks, NodeId& nodeStaller) {
    if (count == 0)
        return;

    vBlocks.reserve(vBlocks.size() + count);
    CNodeState *state = State(nodeid);
    assert(state != NULL);

    // Make sure pindexBestKnownBlock is up to date, we'll need it.
    ProcessBlockAvailability(nodeid);

    CBlockIndex *pindexBestKnownBlock=state->pindexBestKnownBlock;
    
    if(pindexLockedBlock)
    {
        if(pindexBestKnownBlock)
        {
            CBlockIndex *pindexCommonAncestor;
            pindexCommonAncestor=LastCommonAncestor(state->pindexBestKnownBlock,pindexLockedBlock);
            if(pindexCommonAncestor != pindexLockedBlock)
            {
                pindexBestKnownBlock=pindexCommonAncestor;
            }            
        }
    }
    
    if (pindexBestKnownBlock == NULL || pindexBestKnownBlock->nChainWork < chainActive.Tip()->nChainWork) {
        // This peer has nothing interesting.
        return;
    }

    if (state->pindexLastCommonBlock == NULL) {
        // Bootstrap quickly by guessing a parent of our best tip is the forking point.
        // Guessing wrong in either direction is not a problem.
        state->pindexLastCommonBlock = chainActive[std::min(pindexBestKnownBlock->nHeight, chainActive.Height())];
    }

    // If the peer reorganized, our previous pindexLastCommonBlock may not be an ancestor
    // of their current tip anymore. Go back enough to fix that.
    state->pindexLastCommonBlock = LastCommonAncestor(state->pindexLastCommonBlock, pindexBestKnownBlock);
    if (state->pindexLastCommonBlock == pindexBestKnownBlock)
        return;

    std::vector<CBlockIndex*> vToFetch;
    CBlockIndex *pindexWalk = state->pindexLastCommonBlock;
    // Never fetch further than the best block we know the peer has, or more than BLOCK_DOWNLOAD_WINDOW + 1 beyond the last
    // linked block we have in common with this peer. The +1 is so we can detect stalling, namely if we would be able to
    // download that next block if the window were 1 larger.
    int nWindowEnd = state->pindexLastCommonBlock->nHeight + BLOCK_DOWNLOAD_WINDOW;
    int nMaxHeight = std::min<int>(pindexBestKnownBlock->nHeight, nWindowEnd + 1);
    NodeId waitingfor = -1;
    while (pindexWalk->nHeight < nMaxHeight) {
        // Read up to 128 (or more, if more blocks than that are needed) successors of pindexWalk (towards
        // pindexBestKnownBlock) into vToFetch. We fetch 128, because CBlockIndex::GetAncestor may be as expensive
        // as iterating over ~100 CBlockIndex* entries anyway.
        int nToFetch = std::min(nMaxHeight - pindexWalk->nHeight, std::max<int>(count - vBlocks.size(), 128));
        vToFetch.resize(nToFetch);
        pindexWalk = pindexBestKnownBlock->GetAncestor(pindexWalk->nHeight + nToFetch);
        vToFetch[nToFetch - 1] = pindexWalk;
        for (unsigned int i = nToFetch - 1; i > 0; i--) {
            vToFetch[i - 1] = vToFetch[i]->pprev;
        }

        // Iterate over those blocks in vToFetch (in forward direction), adding the ones that
        // are not yet downloaded and not in flight to vBlocks. In the mean time, update
        // pindexLastCommonBlock as long as all ancestors are already downloaded.
        BOOST_FOREACH(CBlockIndex* pindex, vToFetch) {
            if (!pindex->IsValid(BLOCK_VALID_TREE)) {
                // We consider the chain that this peer is on invalid.
                return;
            }
/* MCHN START */                        
            if(setBannedTxBlocks.size())
            {
                if(setBannedTxBlocks.find(pindex->GetBlockHash()) != setBannedTxBlocks.end())
                {
                    return;
                }
            }
/* MCHN END */            
            if (pindex->nStatus & BLOCK_HAVE_DATA) {
                if (pindex->nChainTx)
                    state->pindexLastCommonBlock = pindex;
            } else if (mapBlocksInFlight.count(pindex->GetBlockHash()) == 0) {
                // The block is not already downloaded, and not yet in flight.
                if (pindex->nHeight > nWindowEnd) {
                    // We reached the end of the window.
                    if (vBlocks.size() == 0 && waitingfor != nodeid) {
                        // We aren't able to fetch anything, but we would be if the download window was one larger.
                        nodeStaller = waitingfor;
                    }
                    return;
                }
                vBlocks.push_back(pindex);
                if (vBlocks.size() == count) {
                    return;
                }
            } else if (waitingfor == -1) {
                // This is the first already-in-flight block.
                waitingfor = mapBlocksInFlight[pindex->GetBlockHash()].first;
            }
        }
    }
}

} // anon namespace

bool GetNodeStateStats(NodeId nodeid, CNodeStateStats &stats) {
    LOCK(cs_main);
    CNodeState *state = State(nodeid);
    if (state == NULL)
        return false;
    stats.nMisbehavior = state->nMisbehavior;
    stats.nSyncHeight = state->pindexBestKnownBlock ? state->pindexBestKnownBlock->nHeight : -1;
    stats.nCommonHeight = state->pindexLastCommonBlock ? state->pindexLastCommonBlock->nHeight : -1;
    BOOST_FOREACH(const QueuedBlock& queue, state->vBlocksInFlight) {
        if (queue.pindex)
            stats.vHeightInFlight.push_back(queue.pindex->nHeight);
    }
    return true;
}

void RegisterNodeSignals(CNodeSignals& nodeSignals)
{
    nodeSignals.GetHeight.connect(&GetHeight);
    nodeSignals.ProcessMessages.connect(&ProcessMessages);
    nodeSignals.ProcessDataMessage.connect(&ProcessDataMessage);
    nodeSignals.ProcessGetData.connect(&ProcessGetData);
    nodeSignals.SendMessages.connect(&SendMessages);
    nodeSignals.InitializeNode.connect(&InitializeNode);
    nodeSignals.FinalizeNode.connect(&FinalizeNode);
}

void UnregisterNodeSignals(CNodeSignals& nodeSignals)
{
    nodeSignals.GetHeight.disconnect(&GetHeight);
    nodeSignals.ProcessMessages.disconnect(&ProcessMessages);
    nodeSignals.ProcessDataMessage.disconnect(&ProcessDataMessage);
    nodeSignals.ProcessGetData.disconnect(&ProcessGetData);
    nodeSignals.SendMessages.disconnect(&SendMessages);
    nodeSignals.InitializeNode.disconnect(&InitializeNode);
    nodeSignals.FinalizeNode.disconnect(&FinalizeNode);
}

CBlockIndex* FindForkInGlobalIndex(const CChain& chain, const CBlockLocator& locator)
{
    // Find the first block the caller has in the main chain
    BOOST_FOREACH(const uint256& hash, locator.vHave) {
        BlockMap::iterator mi = mapBlockIndex.find(hash);
        if (mi != mapBlockIndex.end())
        {
            CBlockIndex* pindex = (*mi).second;
            if (chain.Contains(pindex))
                return pindex;
        }
    }
    return chain.Genesis();
}

CCoinsViewCache *pcoinsTip = NULL;
CBlockTreeDB *pblocktree = NULL;

//////////////////////////////////////////////////////////////////////////////
//
// mapOrphanTransactions
//

bool AddOrphanTx(const CTransaction& tx, NodeId peer)
{
    uint256 hash = tx.GetHash();
    if (mapOrphanTransactions.count(hash))
        return true;

    // Ignore big transactions, to avoid a
    // send-big-orphans memory exhaustion attack. If a peer has a legitimate
    // large transaction with a missing parent then we assume
    // it will rebroadcast it later, after the parent transaction(s)
    // have been mined or received.
    // 10,000 orphans, each of which is at most 5,000 bytes big is
    // at most 500 megabytes of orphans:
    unsigned int sz = tx.GetSerializeSize(SER_NETWORK, CTransaction::CURRENT_VERSION);
    
    if(OrphanHandlerVersion == 1)
    {
        int nMaxOrphanPoolSize = (int)std::max((int64_t)0, GetArg("-maxorphansize", DEFAULT_MAX_ORPHAN_POOL_SIZE) * 1048576);

        if((int)sz + nOrphanPoolSize > nMaxOrphanPoolSize)
        {
            if(fDebug)LogPrint("mempool", "ignoring orphan tx (size: %u, hash: %s), orphan pool size: %d\n", sz, hash.ToString(),nOrphanPoolSize);
            return false;        
        }        
    }
    else
    {
        if (sz > 5000)
        {
            if(fDebug)LogPrint("mempool", "ignoring large orphan tx (size: %u, hash: %s)\n", sz, hash.ToString());
            return false;
        }
    }

    
    mapOrphanTransactions[hash].tx = tx;
    mapOrphanTransactions[hash].fromPeer = peer;
    mapOrphanTransactions[hash].size=sz;
    mapOrphanTransactions[hash].time=GetTimeMicros();
    nOrphanPoolSize+=sz;
    
    BOOST_FOREACH(const CTxIn& txin, tx.vin)
        mapOrphanTransactionsByPrev[txin.prevout.hash].insert(hash);

    if(fDebug)if(fDebug)LogPrint("mempool", "stored orphan tx %s (mapsz %u prevsz %u)\n", hash.ToString(),
             mapOrphanTransactions.size(), mapOrphanTransactionsByPrev.size());
    return true;
}

void static EraseOrphanTx(uint256 hash)
{
    map<uint256, COrphanTx>::iterator it = mapOrphanTransactions.find(hash);
    if (it == mapOrphanTransactions.end())
        return;
    BOOST_FOREACH(const CTxIn& txin, it->second.tx.vin)
    {
        map<uint256, set<uint256> >::iterator itPrev = mapOrphanTransactionsByPrev.find(txin.prevout.hash);
        if (itPrev == mapOrphanTransactionsByPrev.end())
            continue;
        itPrev->second.erase(hash);
        if (itPrev->second.empty())
            mapOrphanTransactionsByPrev.erase(itPrev);
    }
    nOrphanPoolSize-=it->second.size;
    mapOrphanTransactions.erase(it);
}

int EraseOrphansFor(NodeId peer)
{
    int nErased = 0;
    map<uint256, COrphanTx>::iterator iter = mapOrphanTransactions.begin();
    while (iter != mapOrphanTransactions.end())
    {
        map<uint256, COrphanTx>::iterator maybeErase = iter++; // increment to avoid iterator becoming invalid
        if (maybeErase->second.fromPeer == peer)
        {
            EraseOrphanTx(maybeErase->second.tx.GetHash());
            ++nErased;
        }
    }
    if (nErased > 0) if(fDebug)LogPrint("mempool", "Erased %d orphan tx from peer %d\n", nErased, peer);
    return nErased;
}

unsigned int CheckOrphanPeers()
{
    unsigned int nEvicted = 0;
    
    if(OrphanHandlerVersion != 1)
        return 0;
    if(mapOrphanTransactions.size() == 0)
        return 0;
    
    int64_t mintime=GetTimeMicros();
    int peertoerase=0;
    map<uint256, COrphanTx>::iterator iter = mapOrphanTransactions.begin();
    while (iter != mapOrphanTransactions.end())
    {
        if(iter->second.time<=mintime)
        {
            peertoerase=iter->second.fromPeer;
            mintime=iter->second.time;
        }
        iter++;
    }            
    if(peertoerase > 0)
    {
//        if(0.000001*(GetTimeMicros()-mintime) > GetArg("-orphanmaxtxage",DEFAULT_MAX_ORPHAN_TX_AGE))
        if(0.000001*(GetTimeMicros()-mintime) > GetArg("-orphanmaxtxage",2 * Params().TargetSpacing()))
        {
            nEvicted=EraseOrphansFor(peertoerase);

            LOCK(cs_vNodes);
            BOOST_FOREACH(CNode* pnode, vNodes)
            {
                if(pnode->id == peertoerase)
                {
                    LogPrintf("mchn: Stuck transactions in orphan pool, disconnecting peer %d, removed %d txs, first is %8.3fs old\n",
                            peertoerase,nEvicted,0.000001*(double)(GetTimeMicros()-mintime));        
                    pnode->fDisconnect=true;
                }
            }            
        }
    }    
    return nEvicted;
}

unsigned int LimitOrphanTxSize(unsigned int nMaxOrphans)
{
    unsigned int nEvicted = 0;
    
    if(OrphanHandlerVersion == 1)
    {
        if(mapOrphanTransactions.size() > nMaxOrphans)
        {
            int64_t mintime=GetTimeMicros();
            int peertoerase=0;
            map<uint256, COrphanTx>::iterator iter = mapOrphanTransactions.begin();
            while (iter != mapOrphanTransactions.end())
            {
                if(iter->second.time<=mintime)
                {
                    peertoerase=iter->second.fromPeer;
                    mintime=iter->second.time;
                }
                iter++;
            }            
            if(peertoerase > 0)
            {
                nEvicted=EraseOrphansFor(peertoerase);
                
                LOCK(cs_vNodes);
                BOOST_FOREACH(CNode* pnode, vNodes)
                {
                    if(pnode->id == peertoerase)
                    {
                        LogPrintf("mchn: Orphan pool full, disconnecting peer %d, removed %d txs, first is %8.3fs old\n",
                                peertoerase,nEvicted,0.000001*(double)(GetTimeMicros()-mintime));        
                        pnode->fDisconnect=true;
                    }
                }
            }
        }        
    }
    else
    {
        while (mapOrphanTransactions.size() > nMaxOrphans)
        {
            // Evict a random orphan:
            uint256 randomhash = GetRandHash();
            map<uint256, COrphanTx>::iterator it = mapOrphanTransactions.lower_bound(randomhash);
            if (it == mapOrphanTransactions.end())
                it = mapOrphanTransactions.begin();
            EraseOrphanTx(it->first);
            ++nEvicted;
        }
    }
    return nEvicted;
}

int OrphanPoolSize()
{
    return (int)mapOrphanTransactions.size();
}

int LastForkedHeight()
{
    return nLastForkedHeight;
}


/* MCHN START */
bool IsStandardTx(const CTransaction& tx, string& reason,bool check_for_dust)
/* MCHN END */
{
    AssertLockHeld(cs_main);
    if (tx.nVersion > CTransaction::CURRENT_VERSION || tx.nVersion < 1) {
        reason = "version";
        return false;
    }

    // Treat non-final transactions as non-standard to prevent a specific type
    // of double-spend attack, as well as DoS attacks. (if the transaction
    // can't be mined, the attacker isn't expending resources broadcasting it)
    // Basically we don't want to propagate transactions that can't be included in
    // the next block.
    //
    // However, IsFinalTx() is confusing... Without arguments, it uses
    // chainActive.Height() to evaluate nLockTime; when a block is accepted, chainActive.Height()
    // is set to the value of nHeight in the block. However, when IsFinalTx()
    // is called within CBlock::AcceptBlock(), the height of the block *being*
    // evaluated is what is used. Thus if we want to know if a transaction can
    // be part of the *next* block, we need to call IsFinalTx() with one more
    // than chainActive.Height().
    //
    // Timestamps on the other hand don't get any special treatment, because we
    // can't know what timestamp the next block will have, and there aren't
    // timestamp applications where it matters.
    if (!IsFinalTx(tx, chainActive.Height() + 1)) {
        reason = "non-final";
        return false;
    }

    // Extremely large transactions with lots of inputs can cost the network
    // almost as much to process as they cost the sender in fees, because
    // computing signature hashes is O(ninputs*txsize). Limiting transactions
    // to MAX_STANDARD_TX_SIZE mitigates CPU exhaustion attacks.
    unsigned int sz = tx.GetSerializeSize(SER_NETWORK, CTransaction::CURRENT_VERSION);
    if (sz >= MAX_STANDARD_TX_SIZE) {
        reason = "tx-size";
        return false;
    }
    
/* MCHN START */    
    // This transaction will not fit any block
    if (sz > MAX_BLOCK_SIZE-81) {
        reason = "tx-size";
        return false;
    }
/* MCHN END */    

    
    BOOST_FOREACH(const CTxIn& txin, tx.vin)
    {
        // Biggest 'standard' txin is a 15-of-15 P2SH multisig with compressed
        // keys. (remember the 520 byte limit on redeemScript size) That works
        // out to a (15*(33+1))+3=513 byte redeemScript, 513+1+15*(73+1)+3=1627
        // bytes of scriptSig, which we round off to 1650 bytes for some minor
        // future-proofing. That's also enough to spend a 20-of-20
        // CHECKMULTISIG scriptPubKey, though such a scriptPubKey is not
        // considered standard)
        if (txin.scriptSig.size() > 1650) {
            reason = "scriptsig-size";
            return false;
        }
        if (!txin.scriptSig.IsPushOnly()) {
            reason = "scriptsig-not-pushonly";
            return false;
        }
    }

    unsigned int nDataOut = 0;
    txnouttype whichType;
    BOOST_FOREACH(const CTxOut& txout, tx.vout) {
        if (!::IsStandard(txout.scriptPubKey, whichType)) {
            reason = "scriptpubkey";
            return false;
        }

        if (whichType == TX_NULL_DATA)
            nDataOut++;
        else if ((whichType == TX_MULTISIG) && (!fIsBareMultisigStd)) {
            reason = "bare-multisig";
            return false;
        } else if (txout.IsDust(::minRelayTxFee)) {
/* MCHN START */
            if(check_for_dust)
            {
/* MCHN END */
            reason = "dust";
            return false;
/* MCHN START */
            }
/* MCHN END */
        }
    }

    // only one OP_RETURN txout is permitted
    int max_op_returns=MCP_MAX_STD_OP_RETURN_COUNT;
    
    if ((int)nDataOut > max_op_returns) {
        reason = "multi-op-return";
        return false;
    }

    return true;
}

/* MCHN START */
bool IsStandardTx(const CTransaction& tx, string& reason)
{
    return IsStandardTx(tx,reason,true);
}
/* MCHN START */

bool IsFinalTx(const CTransaction &tx, int nBlockHeight, int64_t nBlockTime)
{
    AssertLockHeld(cs_main);
    // Time based nLockTime implemented in 0.1.6
    if (tx.nLockTime == 0)
        return true;
    if (nBlockHeight == 0)
        nBlockHeight = chainActive.Height();
    if (nBlockTime == 0)
        nBlockTime = GetAdjustedTime();
    if ((int64_t)tx.nLockTime < ((int64_t)tx.nLockTime < LOCKTIME_THRESHOLD ? (int64_t)nBlockHeight : nBlockTime))
        return true;
    BOOST_FOREACH(const CTxIn& txin, tx.vin)
        if (!txin.IsFinal())
            return false;
    return true;
}

/**
 * Check transaction inputs to mitigate two
 * potential denial-of-service attacks:
 * 
 * 1. scriptSigs with extra data stuffed into them,
 *    not consumed by scriptPubKey (or P2SH script)
 * 2. P2SH scripts with a crazy number of expensive
 *    CHECKSIG/CHECKMULTISIG operations
 */
bool AreInputsStandard(const CTransaction& tx, const CCoinsViewCache& mapInputs)
{
    if (tx.IsCoinBase())
        return true; // Coinbases don't use vin normally

    for (unsigned int i = 0; i < tx.vin.size(); i++)
    {
        const CTxOut& prev = mapInputs.GetOutputFor(tx.vin[i]);

        txnouttype whichType;
        const CScript& prevScript = prev.scriptPubKey;
        
        vector<vector<unsigned char> > vSolutions;
/*        
        // get the scriptPubKey corresponding to this input:
        if (!Solver(prevScript, whichType, vSolutions))
            return false;
        int nArgsExpected = ScriptSigArgsExpected(whichType, vSolutions);
*/
        vector<CTxDestination> addressRets;
        if(!IsStandard(prevScript,whichType))
        {
            return false; 
        }
        
        int nArgsExpected=-1;
        switch (whichType)
        {
            case TX_PUBKEYHASH:
                nArgsExpected=2;
                break;
            case TX_SCRIPTHASH:
                nArgsExpected=1;
                break;
            case TX_PUBKEY:
                nArgsExpected=1;
                break;
            case TX_MULTISIG:
                Solver(prevScript, whichType, vSolutions);
                nArgsExpected = ScriptSigArgsExpected(whichType, vSolutions);
                break;
            case TX_NONSTANDARD:
            case TX_NULL_DATA:
                nArgsExpected=-1;
                break;
        }
        if (nArgsExpected < 0)
            return false;
        
        // Transactions with extra stuff in their scriptSigs are
        // non-standard. Note that this EvalScript() call will
        // be quick, because if there are any operations
        // beside "push data" in the scriptSig
        // IsStandard() will have already returned false
        // and this method isn't called.
        vector<vector<unsigned char> > stack;
        if (!EvalScript(stack, tx.vin[i].scriptSig, false, BaseSignatureChecker()))
            return false;

        if (whichType == TX_SCRIPTHASH)
        {
            if (stack.empty())
                return false;
            CScript subscript(stack.back().begin(), stack.back().end());
            vector<vector<unsigned char> > vSolutions2;
            txnouttype whichType2;
            if (Solver(subscript, whichType2, vSolutions2))
            {
                int tmpExpected = ScriptSigArgsExpected(whichType2, vSolutions2);
                if (tmpExpected < 0)
                    return false;
                nArgsExpected += tmpExpected;
            }
            else
            {
                // Any other Script with less than 15 sigops OK:
                unsigned int sigops = subscript.GetSigOpCount(true);
                // ... extra data left on the stack after execution is OK, too:
                return (sigops <= MAX_P2SH_SIGOPS);
            }
        }

        if (stack.size() != (unsigned int)nArgsExpected)
            return false;
    }

    return true;
}

unsigned int GetLegacySigOpCount(const CTransaction& tx)
{
    unsigned int nSigOps = 0;
    BOOST_FOREACH(const CTxIn& txin, tx.vin)
    {
        nSigOps += txin.scriptSig.GetSigOpCount(false);
    }
    BOOST_FOREACH(const CTxOut& txout, tx.vout)
    {
        nSigOps += txout.scriptPubKey.GetSigOpCount(false);
    }
    return nSigOps;
}

unsigned int GetP2SHSigOpCount(const CTransaction& tx, const CCoinsViewCache& inputs)
{
    if (tx.IsCoinBase())
        return 0;

    unsigned int nSigOps = 0;
    for (unsigned int i = 0; i < tx.vin.size(); i++)
    {
        const CTxOut &prevout = inputs.GetOutputFor(tx.vin[i]);
        if (prevout.scriptPubKey.IsPayToScriptHash())
            nSigOps += prevout.scriptPubKey.GetSigOpCount(tx.vin[i].scriptSig);
    }
    return nSigOps;
}








bool CheckTransaction(const CTransaction& tx, CValidationState &state)
{
    // Basic checks that don't depend on any context
    if (tx.vin.empty())
        return state.DoS(10, error("CheckTransaction() : vin empty"),
                         REJECT_INVALID, "bad-txns-vin-empty");
    if (tx.vout.empty())
        return state.DoS(10, error("CheckTransaction() : vout empty"),
                         REJECT_INVALID, "bad-txns-vout-empty");
    // Size limits
    if (::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION) > MAX_BLOCK_SIZE)
        return state.DoS(100, error("CheckTransaction() : size limits failed"),
                         REJECT_INVALID, "bad-txns-oversize");

    // Check for negative or overflow output values
    CAmount nValueOut = 0;
    BOOST_FOREACH(const CTxOut& txout, tx.vout)
    {
        if (txout.nValue < 0)
            return state.DoS(100, error("CheckTransaction() : txout.nValue negative"),
                             REJECT_INVALID, "bad-txns-vout-negative");
        if (txout.nValue > MAX_MONEY)
            return state.DoS(100, error("CheckTransaction() : txout.nValue too high"),
                             REJECT_INVALID, "bad-txns-vout-toolarge");
        nValueOut += txout.nValue;
        if (!MoneyRange(nValueOut))
            return state.DoS(100, error("CheckTransaction() : txout total out of range"),
                             REJECT_INVALID, "bad-txns-txouttotal-toolarge");
    }

    // Check for duplicate inputs
    set<COutPoint> vInOutPoints;
    BOOST_FOREACH(const CTxIn& txin, tx.vin)
    {
        if (vInOutPoints.count(txin.prevout))
            return state.DoS(100, error("CheckTransaction() : duplicate inputs"),
                             REJECT_INVALID, "bad-txns-inputs-duplicate");
        vInOutPoints.insert(txin.prevout);
    }

    if (tx.IsCoinBase())
    {
/* MCHN START */        
/*        
        string reason;
        if(!AcceptPermissionsAndCheckForDust(tx,false,reason))
        {
            return state.DoS(100, error("CheckTransaction() : coinbase permission script - %s",reason),
                             REJECT_INVALID, "bad-perm-script");            
        }
 */ 
/* MCHN END */        
        if (tx.vin[0].scriptSig.size() < 2 || tx.vin[0].scriptSig.size() > 100)
            return state.DoS(100, error("CheckTransaction() : coinbase script size"),
                             REJECT_INVALID, "bad-cb-length");
    }
    else
    {
        BOOST_FOREACH(const CTxIn& txin, tx.vin)
            if (txin.prevout.IsNull())
                return state.DoS(10, error("CheckTransaction() : prevout is null"),
                                 REJECT_INVALID, "bad-txns-prevout-null");
    }

    return true;
}

CAmount GetMinRelayFee(const CTransaction& tx, unsigned int nBytes, bool fAllowFree)
{
    {
        LOCK(mempool.cs);
        uint256 hash = tx.GetHash();
        double dPriorityDelta = 0;
        CAmount nFeeDelta = 0;
        mempool.ApplyDeltas(hash, dPriorityDelta, nFeeDelta);
        if (dPriorityDelta > 0 || nFeeDelta > 0)
            return 0;
    }

    CAmount nMinFee = ::minRelayTxFee.GetFee(nBytes);

    if (fAllowFree)
    {
        // There is a free transaction area in blocks created by most miners,
        // * If we are relaying we allow transactions up to DEFAULT_BLOCK_PRIORITY_SIZE - 1000
        //   to be considered to fall into this category. We don't want to encourage sending
        //   multiple transactions instead of one big transaction to avoid fees.
        if (nBytes < (MAX_BLOCK_SIZE / 20 - 1000))
            nMinFee = 0;
    }

    if (!MoneyRange(nMinFee))
        nMinFee = MAX_MONEY;
    return nMinFee;
}

int TxThrottlingDelay(bool print)
{    
    if( (nMessageHandlerThreads & MC_MHT_PROCESSTXDATA ) == 0)                
    {
        return 0;
    }
    
    int64_t one_mb=1048576;
    
    double dDelay;
    int nDelay;
    
    size_t max_memory=2048;//MB
    int64_t max_mempool_blocks=((int64_t)GetArg("-mempoolthrottle", DEFAULT_MEMPOOL_THROTTLE)*one_mb)/MAX_BLOCK_SIZE;
    if(max_mempool_blocks <= 0)
    {
        max_mempool_blocks=1;
    }
            
    double dMemory,dTime,dSize,dMax;
    dMax=0;
    
    size_t total_memory=nTotalMempoolsSize/one_mb;
    total_memory*=3;
    if(nMessageHandlerThreads == ( MC_MHT_GETDATA | MC_MHT_PROCESSDATA | MC_MHT_PROCESSTXDATA ) )                
    {
        total_memory+=2*nTotalNodeBuffersSize/one_mb;
    }
    dMemory=(double)total_memory/(double)max_memory;
/*    
    if(dMemory>dMax)
    {
        dMax=dMemory;
    }
*/    
    dTime=dAverageBlockTime/Params().TargetSpacing();
    if(dTime>dMax)
    {
        dMax=dTime;
    }
    
    dSize=((double)TotalMempoolTxSize()/(double)MAX_BLOCK_SIZE)/max_mempool_blocks;
    if(dSize>dMax)
    {
        dMax=dSize;
    }
    
    dDelay=0;
    if(dMax > 0.25)
    {
        dDelay=(dMax-0.25)*(dMax-0.25)*160;
    }
    
    if(dDelay>500)
    {
        dDelay=500;
    }
    nDelay=(int)dDelay;
    
    if(print)
    {
        LogPrint("mcblockperf","mchn-block-perf: Block: %d, Tx delay %3dms (%5.2f). "
                "Memory: %4.2f (%4dMB: Mempools: %3d, Network: %3d/%2u), "
                "Time: %4.2f (%6.2fs), "
                "Size: %4.2f (%6.2f)\n",
                chainActive.Height(),nDelay, dMax, dMemory, total_memory, nTotalMempoolsSize/one_mb, nTotalNodeBuffersSize/one_mb, vNodes.size(), dTime, dAverageBlockTime,
                dSize,(double)TotalMempoolTxSize()/(double)MAX_BLOCK_SIZE);            
        
    }
    return nDelay;
}


bool AcceptToMemoryPool(CTxMemPool& pool, CValidationState &state, const CTransaction &tx, bool fLimitFree,
                        bool* pfMissingInputs, bool fRejectInsaneFee,CWalletTx *wtx)
{
    AssertLockHeld(cs_main);
    if (pfMissingInputs)
        *pfMissingInputs = false;
    
//    if(mc_gState->m_ProtocolVersionToUpgrade > mc_gState->m_NetworkParams->ProtocolVersion())
    if( (mc_gState->m_ProtocolVersionToUpgrade > 0) && (mc_gState->IsSupported(mc_gState->m_ProtocolVersionToUpgrade) == 0) )
    {
        return false;
    }
    
    if (!CheckTransaction(tx, state))
        return error("AcceptToMemoryPool: : CheckTransaction failed");

    if(IsTxBanned(tx.GetHash()))
    {
        return state.DoS(0, error("AcceptToMemoryPool: banned transaction: %s",tx.GetHash().ToString()),
                         REJECT_INVALID, "banned");
//        return error("AcceptToMemoryPool: banned transaction: %s",tx.GetHash().ToString());
    }
    
    // Coinbase is only valid in a block, not as a loose transaction
    if (tx.IsCoinBase())
        return state.DoS(100, error("AcceptToMemoryPool: : coinbase as individual tx"),
                         REJECT_INVALID, "coinbase");

/* MCHN START */    
    bool check_for_dust=true;
    
    if(mc_gState->m_NetworkParams->IsProtocolMultichain())
    {
        check_for_dust=false;
    }
 
/* MCHN END */    
    
    // Rather not work on nonstandard transactions (unless -testnet/-regtest)
    string reason;
    if (Params().RequireStandard() && !IsStandardTx(tx, reason, check_for_dust))//MCHN
        return state.DoS(0,
                         error("AcceptToMemoryPool : nonstandard transaction: %s", reason),
                         REJECT_NONSTANDARD, reason);

    // is it already in the memory pool?
    uint256 hash = tx.GetHash();
    if (pool.exists(hash))
        return false;
    
    // Check for conflicts with in-memory transactions
    {
    LOCK(pool.cs); // protect pool.mapNextTx
    for (unsigned int i = 0; i < tx.vin.size(); i++)
    {
        COutPoint outpoint = tx.vin[i].prevout;
        if (pool.mapNextTx.count(outpoint))
        {
            if(fDebug)LogPrint("mchn","Conflicting with in-memory %s\n",tx.vin[i].ToString().c_str());
            // Disable replacement feature for now
            return state.Invalid(error("AcceptToMemoryPool : Conflicting with in-memory tx"),
                                 REJECT_DUPLICATE, "bad-txns-inputs-spent");
        }
    }
    }

    {
        CCoinsView dummy;
        CCoinsViewCache view(&dummy);

        CAmount nValueIn = 0;
        {
        LOCK(pool.cs);
        CCoinsViewMemPool viewMemPool(pcoinsTip, pool);
        view.SetBackend(viewMemPool);

        // do we already have it?
        if (view.HaveCoins(hash))
            return false;

        // do all inputs exist?
        // Note that this does not check for the presence of actual outputs (see the next check for that),
        // only helps filling in pfMissingInputs (to determine missing vs spent).
        BOOST_FOREACH(const CTxIn txin, tx.vin) {
            if (!view.HaveCoins(txin.prevout.hash)) {
                if(fDebug)LogPrint("mchn","Missing tx (%s)\n",txin.prevout.hash.ToString().c_str());
                if (pfMissingInputs)
                    *pfMissingInputs = true;
/* MCHN START */                
                return false;
/*                
                return state.Invalid(error("AcceptToMemoryPool : inputs already spent"),
                                 REJECT_DUPLICATE, "bad-txns-inputs-spent");
 */ 
/* MCHN END */                
            }
        }

        // are the actual inputs available?
        if (!view.HaveInputs(tx))
/* MCHN START */            
        {
            if (!tx.IsCoinBase()) {
                for (unsigned int i = 0; i < tx.vin.size(); i++) {
                    const COutPoint &prevout = tx.vin[i].prevout;
                    const CCoins* coins = view.AccessCoins(prevout.hash);
                    if (!coins || !coins->IsAvailable(prevout.n)) {
                        if(fDebug)LogPrint("mchn","Missing coin (%s,%d)\n",prevout.hash.ToString().c_str(),prevout.n);
                        return state.Invalid(error("AcceptToMemoryPool : inputs already spent"),
                                 REJECT_DUPLICATE, "bad-txns-inputs-spent");
                    }
                }
            }
/* MCHN END */            
            return state.Invalid(error("AcceptToMemoryPool : inputs already spent"),
                                 REJECT_DUPLICATE, "bad-txns-inputs-spent");
/* MCHN START */            
        }
/* MCHN END */            

        // Bring the best block into scope
        view.GetBestBlock();

        nValueIn = view.GetValueIn(tx);

        // we have all inputs cached now, so switch back to dummy, so we don't need to keep lock on mempool
        view.SetBackend(dummy);
        }

        // Check for non-standard pay-to-script-hash in inputs
        if (Params().RequireStandard() && !AreInputsStandard(tx, view))
/* MCHN START */            
//            return error("AcceptToMemoryPool: : nonstandard transaction input");
            return state.DoS(0,error("AcceptToMemoryPool: : nonstandard transaction input"),REJECT_NONSTANDARD,"Nonstandard transaction input");
/* MCHN END */            

        // Check that the transaction doesn't have an excessive number of
        // sigops, making it impossible to mine. Since the coinbase transaction
        // itself can contain sigops MAX_TX_SIGOPS is less than
        // MAX_BLOCK_SIGOPS; we still consider this an invalid rather than
        // merely non-standard transaction.
        unsigned int nSigOps = GetLegacySigOpCount(tx);
        nSigOps += GetP2SHSigOpCount(tx, view);
        if (nSigOps > MAX_TX_SIGOPS)
            return state.DoS(0,
                             error("AcceptToMemoryPool : too many sigops %s, %d > %d",
                                   hash.ToString(), nSigOps, MAX_TX_SIGOPS),
                             REJECT_NONSTANDARD, "bad-txns-too-many-sigops");

        CAmount nValueOut = tx.GetValueOut();
        CAmount nFees = nValueIn-nValueOut;
        double dPriority = view.GetPriority(tx, chainActive.Height());

        CTxMemPoolEntry entry(tx, nFees, GetTime(), dPriority, chainActive.Height());
        unsigned int nSize = entry.GetTxSize();

        ::minRelayTxFee = CFeeRate(MIN_RELAY_TX_FEE);    
        
        // Don't accept it if it can't get into a block
        CAmount txMinFee = GetMinRelayFee(tx, nSize, true);
        if (fLimitFree && nFees < txMinFee)
            return state.DoS(0, error("AcceptToMemoryPool : not enough fees %s, %d < %d",
                                      hash.ToString(), nFees, txMinFee),
                             REJECT_INSUFFICIENTFEE, "insufficient fee");


        // Require that free transactions have sufficient priority to be mined in the next block.
        if (GetBoolArg("-relaypriority", true) && nFees < ::minRelayTxFee.GetFee(nSize) && !AllowFree(view.GetPriority(tx, chainActive.Height() + 1))) {
            return state.DoS(0, false, REJECT_INSUFFICIENTFEE, "insufficient priority");
        }

        // Continuously rate-limit free (really, very-low-fee) transactions
        // This mitigates 'penny-flooding' -- sending thousands of free transactions just to
        // be annoying or make others' transactions take longer to confirm.
        if (fLimitFree && nFees < ::minRelayTxFee.GetFee(nSize))
        {
            static CCriticalSection csFreeLimiter;
            static double dFreeCount;
            static int64_t nLastTime;
            int64_t nNow = GetTime();

            LOCK(csFreeLimiter);

            // Use an exponentially decaying ~10-minute window:
            dFreeCount *= pow(1.0 - 1.0/600.0, (double)(nNow - nLastTime));
            nLastTime = nNow;
            // -limitfreerelay unit is thousand-bytes-per-minute
            // At default rate it would take over a month to fill 1GB
            if (dFreeCount >= GetArg("-limitfreerelay", 0)*10*1000)
                return state.DoS(0, error("AcceptToMemoryPool : free transaction rejected by rate limiter"),
                                 REJECT_INSUFFICIENTFEE, "rate limited free transaction");
            if(fDebug)LogPrint("mempool", "Rate limit dFreeCount: %g => %g\n", dFreeCount, dFreeCount+nSize);
            dFreeCount += nSize;
        }

/* MCHN START */        
        if(MIN_RELAY_TX_FEE != 0)
        {
/* MCHN END */        
        if (fRejectInsaneFee && nFees > ::minRelayTxFee.GetFee(nSize) * 10000)
/* MCHN START */            
/*            
            return error("AcceptToMemoryPool: : insane fees %s, %d > %d",
                         hash.ToString(),
                         nFees, ::minRelayTxFee.GetFee(nSize) * 10000);
 */ 
            return state.DoS(0,
                    error("AcceptToMemoryPool: : insane fees %s, %d > %d",
                         hash.ToString(),
                         nFees, ::minRelayTxFee.GetFee(nSize) * 10000),
                    REJECT_INVALID,"Insane fees");
        
/* MCHN START */        
        }
/* MCHN END */        
        
        unsigned int scriptVerifyFlags = STANDARD_SCRIPT_VERIFY_FLAGS;
        if (!Params().RequireStandard()) 
        {
            scriptVerifyFlags = GetArg("-promiscuousmempoolflags", scriptVerifyFlags);
        }
        
        // Check against previous transactions
        // This is done last to help prevent CPU exhaustion denial-of-service attacks.
        if (!CheckInputs(tx, state, view, true, scriptVerifyFlags, true))
        {
//            return error("AcceptToMemoryPool: : ConnectInputs failed %s", hash.ToString());    
            string strError=state.GetRejectReason();
            if(strError.size() == 0)
            {
                strError="ConnectInputs failed";
            }
            else
            {
                strError="ConnectInputs failed: " + strError;
            }
            return state.DoS(0,error("AcceptToMemoryPool: : ConnectInputs failed %s", hash.ToString()),REJECT_INVALID,strError);
        }
        

        // Check again against just the consensus-critical mandatory script
        // verification flags, in case of bugs in the standard flags that cause
        // transactions to pass as valid when they're actually invalid. For
        // instance the STRICTENC flag was incorrectly allowing certain
        // CHECKSIG NOT scripts to pass, even though they were invalid.
        //
        // There is a similar check in CreateNewBlock() to prevent creating
        // invalid blocks, however allowing such transactions into the mempool
        // can be exploited as a DoS attack.
        if (!CheckInputs(tx, state, view, true, MANDATORY_SCRIPT_VERIFY_FLAGS, true))
        {
/* MCHN START */        
//            return error("AcceptToMemoryPool: : BUG! PLEASE REPORT THIS! ConnectInputs failed against MANDATORY but not STANDARD flags %s", hash.ToString());
            return state.DoS(0,error("AcceptToMemoryPool: : BUG! PLEASE REPORT THIS! ConnectInputs failed against MANDATORY but not STANDARD flags %s", hash.ToString()),REJECT_INVALID,"ConnectInputs failed");
/* MCHN END */        
        }
                
/* MCHN START */
        
        uint32_t replay=0;
        int64_t mandatory_fee;
        int permissions_from,permissions_to;
        permissions_from=mc_gState->m_Permissions->m_MempoolPermissions->GetCount();
        
        if(!AcceptMultiChainTransaction(tx,view,-1,MC_AMT_DEFAULT,reason, &mandatory_fee, &replay))
        {
            return state.DoS(0,
                             error("AcceptToMemoryPool: : AcceptMultiChainTransaction failed %s : %s", hash.ToString(),reason),
                             REJECT_NONSTANDARD, reason);
        }
        
        if(mandatory_fee)
        {
            txMinFee += mandatory_fee;
            if (fLimitFree && nFees < txMinFee)
                return state.DoS(0, error("AcceptToMemoryPool : not enough fees (including mandatory) %s, %d < %d",
                                          hash.ToString(), nFees, txMinFee),
                                 REJECT_INSUFFICIENTFEE, "insufficient fee");
        }
        
        
        int err=MC_ERR_NOERROR;
        
        err=pEF->FED_EventTx(tx,-1,NULL,-1,0,0);
        if(err)
        {
            LogPrintf("ERROR: Cannot write tx %s to feeds, error %d\n",hash.ToString().c_str(),err);
        }
        
        err=MC_ERR_NOERROR;
        if(wtx)
        {
            err=pwalletTxsMain->AddTx(NULL,*wtx,-1,NULL,-1,0);
        }
        else
        {                
            err=pwalletTxsMain->AddTx(NULL,tx,-1,NULL,-1,0);
        }
        if(err)
        {
            reason=strprintf("Wallet error %d",err);
            return state.DoS(0,
                             error("AcceptToMemoryPool: : AcceptMultiChainTransaction failed %s : %s", hash.ToString(),reason),
                             REJECT_INVALID, reason);            
        }
        err=MC_ERR_NOERROR;
        
        permissions_to=mc_gState->m_Permissions->m_MempoolPermissions->GetCount();
        entry.SetReplayNodeParams(( (replay & MC_PPL_REPLAY) != 0) ? true : false,permissions_from,permissions_to);
        
/* MCHN END */    
        // Store transaction in memory
        pool.addUnchecked(hash, entry);
        err=pEF->FED_EventChunksAvailable();
        if(err)
        {
            LogPrintf("ERROR: Cannot write offchain items from tx %s to feeds, error %d\n",hash.ToString().c_str(),err);
        }        
    }

    if(((mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS) == 0) || (mc_gState->m_WalletMode & MC_WMD_MAP_TXS))
    {
        SyncWithWallets(tx, NULL);
    }

    SetRPCNewTxFlag();
    
    return true;
}

/** Return transaction in tx, and if it was found inside a block, its hash is placed in hashBlock */
bool GetTransaction(const uint256 &hash, CTransaction &txOut, uint256 &hashBlock, bool fAllowSlow)
{
    CBlockIndex *pindexSlow = NULL;
    {
        LOCK(cs_main);
        {
            if (mempool.lookup(hash, txOut))
            {
                return true;
            }
        }

        if (fTxIndex) {
            CDiskTxPos postx;
            if (pblocktree->ReadTxIndex(hash, postx)) {
                CAutoFile file(OpenBlockFile(postx, true), SER_DISK, CLIENT_VERSION);
                if (file.IsNull())
                    return error("%s: OpenBlockFile failed", __func__);
                CBlockHeader header;
                try {
                    file >> header;
                    fseek(file.Get(), postx.nTxOffset, SEEK_CUR);
                    file >> txOut;
                } catch (std::exception &e) {
                    return error("%s : Deserialize or I/O error - %s", __func__, e.what());
                }
                hashBlock = header.GetHash();
                if (txOut.GetHash() != hash)
                    return error("%s : txid mismatch", __func__);
                return true;
            }
        }

        if (fAllowSlow) { // use coin database to locate block that contains transaction, and scan it
            int nHeight = -1;
            {
                CCoinsViewCache &view = *pcoinsTip;
                const CCoins* coins = view.AccessCoins(hash);
                if (coins)
                    nHeight = coins->nHeight;
            }
            if (nHeight > 0)
                pindexSlow = chainActive[nHeight];
        }
    }

    if (pindexSlow) {
        CBlock block;
        if (ReadBlockFromDisk(block, pindexSlow)) {
            BOOST_FOREACH(const CTransaction &tx, block.vtx) {
                if (tx.GetHash() == hash) {
                    txOut = tx;
                    hashBlock = pindexSlow->GetBlockHash();
                    return true;
                }
            }
        }
    }

    if (hash == GenesisCoinBaseTxID)
    {
        txOut = GenesisCoinBaseTx;
        hashBlock=chainActive.Genesis()->GetBlockHash();
        return true;
    }
    
    return false;
}






//////////////////////////////////////////////////////////////////////////////
//
// CBlock and CBlockIndex
//

bool WriteBlockToDisk(CBlock& block, CDiskBlockPos& pos)
{
    // Open history file to append
    CAutoFile fileout(OpenBlockFile(pos), SER_DISK, CLIENT_VERSION);
    if (fileout.IsNull())
        return error("WriteBlockToDisk : OpenBlockFile failed");

    // Write index header
    unsigned int nSize = fileout.GetSerializeSize(block);
    fileout << FLATDATA(Params().MessageStart()) << nSize;

    // Write block
    long fileOutPos = ftell(fileout.Get());
    if (fileOutPos < 0)
        return error("WriteBlockToDisk : ftell failed");
    pos.nPos = (unsigned int)fileOutPos;
    fileout << block;

    return true;
}

bool ReadBlockFromDisk(CBlock& block, const CDiskBlockPos& pos)
{
    block.SetNull();

    // Open history file to read
    CAutoFile filein(OpenBlockFile(pos, true), SER_DISK, CLIENT_VERSION);
    if (filein.IsNull())
        return error("ReadBlockFromDisk : OpenBlockFile failed");

    // Read block
    try {
        filein >> block;
    }
    catch (std::exception &e) {
        return error("%s : Deserialize or I/O error - %s", __func__, e.what());
    }

    // Check the header
    if (!CheckProofOfWork(block.GetHash(), block.nBits))
        return error("ReadBlockFromDisk : Errors in block header");

    return true;
}

bool ReadBlockFromDisk(CBlock& block, const CBlockIndex* pindex)
{
    if (!ReadBlockFromDisk(block, pindex->GetBlockPos()))
        return false;
    if (block.GetHash() != pindex->GetBlockHash())
        return error("ReadBlockFromDisk(CBlock&, CBlockIndex*) : GetHash() doesn't match index");
/* MCHN START */    
    VerifyBlockSignature(&block,true);
/* MCHN END */    
    return true;
}

CAmount GetBlockValue(int nHeight, const CAmount& nFees)
{
/* MCHN START */    
//    CAmount nSubsidy = 50 * COIN;
    CAmount nSubsidy = MCP_INITIAL_BLOCK_REWARD;// * COIN;
    if(nHeight == 1)
    {
        if(MCP_FIRST_BLOCK_REWARD >= 0)
        {
            nSubsidy = MCP_FIRST_BLOCK_REWARD;// * COIN;
        }
    }
/* MCHN END */    
    int halvings = nHeight / Params().SubsidyHalvingInterval();

    // Force block reward to zero when right shift is undefined.
    if (halvings >= 64)
        return nFees;

    // Subsidy is cut in half every 210,000 blocks which will occur approximately every 4 years.
    nSubsidy >>= halvings;

    return nSubsidy + nFees;
}

bool IsInitialBlockDownloadNoCheckpoints()
{
    if (fImporting || fReindex )
        return true;
    
    return false;
}

bool IsInitialBlockDownload()
{
    LOCK(cs_main);
    if (fImporting || fReindex || chainActive.Height() < Checkpoints::GetTotalBlocksEstimate())
        return true;
    static bool lockIBDState = false;
    if (lockIBDState)
        return false;
/* MCHN START */    
/* We cannot make these checks for private network when chain/nodes can go down for weeks  
    bool state = (chainActive.Height() < pindexBestHeader->nHeight - 24 * 6 ||
            pindexBestHeader->GetBlockTime() < GetTime() - 24 * 60 * 60);
    bool state = (chainActive.Height() < pindexBestHeader->nHeight - 86400 / MCP_TARGET_BLOCK_TIME ||
            pindexBestHeader->GetBlockTime() < GetTime() - 24 * 60 * 60);
    if (!state)
        lockIBDState = true;
    return state;
 */ 
/* MCHN END */    
    return false;
}

bool fLargeWorkForkFound = false;
bool fLargeWorkInvalidChainFound = false;
CBlockIndex *pindexBestForkTip = NULL, *pindexBestForkBase = NULL;

void CheckForkWarningConditions()
{
    AssertLockHeld(cs_main);
    // Before we get past initial download, we cannot reliably alert about forks
    // (we assume we don't get stuck on a fork before the last checkpoint)
    if (IsInitialBlockDownload())
        return;

    // If our best fork is no longer within 72 blocks (+/- 12 hours if no one mines it)
    // of our head, drop it
/* MCHN START */    
//    if (pindexBestForkTip && chainActive.Height() - pindexBestForkTip->nHeight >= 72)
    if (pindexBestForkTip && chainActive.Height() - pindexBestForkTip->nHeight >= 43200 / MCP_TARGET_BLOCK_TIME)
        pindexBestForkTip = NULL;
/* MCHN END */    

    if (pindexBestForkTip || (pindexBestInvalid && pindexBestInvalid->nChainWork > chainActive.Tip()->nChainWork + (GetBlockProof(*chainActive.Tip()) * 6)))
    {
        if (!fLargeWorkForkFound && pindexBestForkBase)
        {
            std::string warning = std::string("'Warning: Large-work fork detected, forking after block ") +
                pindexBestForkBase->phashBlock->ToString() + std::string("'");
            CAlert::Notify(warning, true);
        }
        if (pindexBestForkTip && pindexBestForkBase)
        {
            LogPrintf("CheckForkWarningConditions: Warning: Large valid fork found\n  forking the chain at height %d (%s)\n  lasting to height %d (%s).\nChain state database corruption likely.\n",
                   pindexBestForkBase->nHeight, pindexBestForkBase->phashBlock->ToString(),
                   pindexBestForkTip->nHeight, pindexBestForkTip->phashBlock->ToString());
            fLargeWorkForkFound = true;
        }
        else
        {
            LogPrintf("CheckForkWarningConditions: Warning: Found invalid chain at least ~6 blocks longer than our best chain.\nChain state database corruption likely.\n");
            fLargeWorkInvalidChainFound = true;
        }
    }
    else
    {
        fLargeWorkForkFound = false;
        fLargeWorkInvalidChainFound = false;
    }
}

void CheckForkWarningConditionsOnNewFork(CBlockIndex* pindexNewForkTip)
{
    AssertLockHeld(cs_main);
    // If we are on a fork that is sufficiently large, set a warning flag
    CBlockIndex* pfork = pindexNewForkTip;
    CBlockIndex* plonger = chainActive.Tip();
    while (pfork && pfork != plonger)
    {
        while (plonger && plonger->nHeight > pfork->nHeight)
            plonger = plonger->pprev;
        if (pfork == plonger)
            break;
        pfork = pfork->pprev;
    }

    // We define a condition which we should warn the user about as a fork of at least 7 blocks
    // who's tip is within 72 blocks (+/- 12 hours if no one mines it) of ours
    // We use 7 blocks rather arbitrarily as it represents just under 10% of sustained network
    // hash rate operating on the fork.
    // or a chain that is entirely longer than ours and invalid (note that this should be detected by both)
    // We define it this way because it allows us to only store the highest fork tip (+ base) which meets
    // the 7-block condition and from this always have the most-likely-to-cause-warning fork
    if (pfork && (!pindexBestForkTip || (pindexBestForkTip && pindexNewForkTip->nHeight > pindexBestForkTip->nHeight)) &&
            pindexNewForkTip->nChainWork - pfork->nChainWork > (GetBlockProof(*pfork) * 7) &&
/* MCHN START */    
//            chainActive.Height() - pindexNewForkTip->nHeight < 72)
            chainActive.Height() - pindexNewForkTip->nHeight < 43200 / MCP_TARGET_BLOCK_TIME)
/* MCHN END */    
    {
        pindexBestForkTip = pindexNewForkTip;
        pindexBestForkBase = pfork;
    }

    CheckForkWarningConditions();
}

// Requires cs_main.
void Misbehaving(NodeId pnode, int howmuch)
{
    if (howmuch == 0)
        return;

    CNodeState *state = State(pnode);
    if (state == NULL)
        return;

    state->nMisbehavior += howmuch;
    int banscore = GetArg("-banscore", 100);
    if (state->nMisbehavior >= banscore && state->nMisbehavior - howmuch < banscore)
    {
        LogPrintf("Misbehaving: %s (%d -> %d) BAN THRESHOLD EXCEEDED\n", state->name, state->nMisbehavior-howmuch, state->nMisbehavior);
        state->fShouldBan = true;
    } else
        LogPrintf("Misbehaving: %s (%d -> %d)\n", state->name, state->nMisbehavior-howmuch, state->nMisbehavior);
}

void static InvalidChainFound(CBlockIndex* pindexNew)
{
    if (!pindexBestInvalid || pindexNew->nChainWork > pindexBestInvalid->nChainWork)
        pindexBestInvalid = pindexNew;
    LogPrintf("InvalidChainFound: invalid block=%s  height=%d  log2_work=%.8g  date=%s\n",
      pindexNew->GetBlockHash().ToString(), pindexNew->nHeight,
      log(pindexNew->nChainWork.getdouble())/log(2.0), DateTimeStrFormat("%Y-%m-%d %H:%M:%S",
      pindexNew->GetBlockTime()));
/* MCHNS START */    
    if(chainActive.Height() >= 0)                                               // Crashes if genesis is invalid
    {
/* MCHNS END */    
        LogPrintf("InvalidChainFound:  current best=%s  height=%d  log2_work=%.8g  date=%s\n",
          chainActive.Tip()->GetBlockHash().ToString(), chainActive.Height(), log(chainActive.Tip()->nChainWork.getdouble())/log(2.0),
          DateTimeStrFormat("%Y-%m-%d %H:%M:%S", chainActive.Tip()->GetBlockTime()));
/* MCHNS START */    
    }
/* MCHNS END */    
    CheckForkWarningConditions();
}

void static InvalidBlockFound(CBlockIndex *pindex, const CValidationState &state) {
    int nDoS = 0;
    if (state.IsInvalid(nDoS)) {
        std::map<uint256, NodeId>::iterator it = mapBlockSource.find(pindex->GetBlockHash());
        if (it != mapBlockSource.end() && State(it->second)) {
            CBlockReject reject = {state.GetRejectCode(), state.GetRejectReason().substr(0, MAX_REJECT_MESSAGE_LENGTH), pindex->GetBlockHash()};
            State(it->second)->rejects.push_back(reject);
            if (nDoS > 0)
                Misbehaving(it->second, nDoS);
        }
    }
    if (!state.CorruptionPossible()) {
        pindex->nStatus |= BLOCK_FAILED_VALID;
        setDirtyBlockIndex.insert(pindex);
        setBlockIndexCandidates.erase(pindex);
        InvalidChainFound(pindex);
    }
}

void UpdateCoins(const CTransaction& tx, CValidationState &state, CCoinsViewCache &inputs, CTxUndo &txundo, int nHeight)
{
    // mark inputs spent
    if (!tx.IsCoinBase()) {
        txundo.vprevout.reserve(tx.vin.size());
        BOOST_FOREACH(const CTxIn &txin, tx.vin) {
            txundo.vprevout.push_back(CTxInUndo());
            bool ret = inputs.ModifyCoins(txin.prevout.hash)->Spend(txin.prevout, txundo.vprevout.back());
            assert(ret);
        }
    }

    // add outputs
    inputs.ModifyCoins(tx.GetHash())->FromTx(tx, nHeight);
}

bool CScriptCheck::operator()() {
    const CScript &scriptSig = ptxTo->vin[nIn].scriptSig;
/*    
    printf("VerifyScript %d %d\n",scriptSig.end()-scriptSig.begin(),scriptPubKey.end()-scriptPubKey.begin());
//    mc_DumpSize("sigScript",(unsigned char*)(&scriptSig.begin()[0]),scriptSig.end()-scriptSig.begin(),scriptSig.end()-scriptSig.begin());
//    mc_DumpSize("scriptPubKey",(unsigned char*)(&scriptPubKey.begin()[0]),scriptPubKey.end()-scriptPubKey.begin(),scriptPubKey.end()-scriptPubKey.begin());
 */ 
    if (!VerifyScript(scriptSig, scriptPubKey, nFlags, CachingTransactionSignatureChecker(ptxTo, nIn, cacheStore), &error)) {
        return ::error("CScriptCheck(): %s:%d VerifySignature failed: %s", ptxTo->GetHash().ToString(), nIn, ScriptErrorString(error));
    }
    return true;
}

bool CheckInputs(const CTransaction& tx, CValidationState &state, const CCoinsViewCache &inputs, bool fScriptChecks, unsigned int flags, bool cacheStore, std::vector<CScriptCheck> *pvChecks)
{
    if (!tx.IsCoinBase())
    {
/*                
        if (pvChecks)
            pvChecks->reserve(tx.vin.size());
*/
                
        // This doesn't trigger the DoS code on purpose; if it did, it would make it easier
        // for an attacker to attempt to split the network.
        if (!inputs.HaveInputs(tx))
            return state.Invalid(error("CheckInputs() : %s inputs unavailable", tx.GetHash().ToString()));

        // While checking, GetBestBlock() refers to the parent block.
        // This is also true for mempool checks.
        CBlockIndex *pindexPrev = mapBlockIndex.find(inputs.GetBestBlock())->second;
        int nSpendHeight = pindexPrev->nHeight + 1;
        CAmount nValueIn = 0;
        CAmount nFees = 0;
        int async_count=0;
        bool fIsLicenseTokenTransfer=Is_MultiChainLicenseTokenTransfer(tx);
        
        vector <unsigned int> vSendPermissionFlags;
        
        for (unsigned int i = 0; i < tx.vin.size(); i++)
        {
            const COutPoint &prevout = tx.vin[i].prevout;
            const CCoins *coins = inputs.AccessCoins(prevout.hash);
            assert(coins);

            // If prev is coinbase, check that it's matured
            if (coins->IsCoinBase()) {
                if (nSpendHeight - coins->nHeight < COINBASE_MATURITY)
                    return state.Invalid(
                        error("CheckInputs() : tried to spend coinbase at depth %d", nSpendHeight - coins->nHeight),
                        REJECT_INVALID, "bad-txns-premature-spend-of-coinbase");
            }

            // Check for negative or overflow input values
            nValueIn += coins->vout[prevout.n].nValue;
            if (!MoneyRange(coins->vout[prevout.n].nValue) || !MoneyRange(nValueIn))
                return state.DoS(100, error("CheckInputs() : txin values out of range"),
                                 REJECT_INVALID, "bad-txns-inputvalues-outofrange");
            unsigned int send_permission_flags=0;

            if(fScriptChecks)
            {
                CTxDestination addressRet;     
                if(ExtractDestinationScriptValid(coins->vout[prevout.n].scriptPubKey, addressRet))
                {
                    if(!fIsLicenseTokenTransfer)
                    {                        
                        CKeyID *lpKeyID=boost::get<CKeyID> (&addressRet);
                        if(lpKeyID != NULL)
                        {
                            if(!mc_gState->m_Permissions->CanSend(NULL,(unsigned char*)(lpKeyID)))
                            {
                                return state.Invalid(error("CheckInputs() : %s input %d doesn't have send permission", tx.GetHash().ToString(),i));
                            }                            
                            send_permission_flags=SCRIPT_VERIFY_SKIP_SEND_PERMISSION_CHECK;
                            async_count++;
                        }
                        else
                        {
                            CScriptID *lpScriptID=boost::get<CScriptID> (&addressRet);
                            if(lpScriptID)
                            {
                                if(mc_gState->m_Permissions->CanSend(NULL,(unsigned char*)(lpScriptID)))
                                {
                                    send_permission_flags=SCRIPT_VERIFY_SKIP_SEND_PERMISSION_CHECK;
                                    async_count++;
                                }                
                            }
                        }                                            
                    }
                    else
                    {
                        send_permission_flags=SCRIPT_VERIFY_SKIP_SEND_PERMISSION_CHECK;
                        async_count++;                        
                    }
                }
            }

            vSendPermissionFlags.push_back(send_permission_flags);
        }

        if(async_count)
        {
            if (pvChecks)
                pvChecks->reserve(async_count);
        }        

        if (nValueIn < tx.GetValueOut())
            return state.DoS(100, error("CheckInputs() : %s value in (%s) < value out (%s)",
                                        tx.GetHash().ToString(), FormatMoney(nValueIn), FormatMoney(tx.GetValueOut())),
                             REJECT_INVALID, "bad-txns-in-belowout");

        // Tally transaction fees
        CAmount nTxFee = nValueIn - tx.GetValueOut();
        if (nTxFee < 0)
            return state.DoS(100, error("CheckInputs() : %s nTxFee < 0", tx.GetHash().ToString()),
                             REJECT_INVALID, "bad-txns-fee-negative");
        nFees += nTxFee;
        if (!MoneyRange(nFees))
            return state.DoS(100, error("CheckInputs() : nFees out of range"),
                             REJECT_INVALID, "bad-txns-fee-outofrange");

        // The first loop above does all the inexpensive checks.
        // Only if ALL inputs pass do we perform expensive ECDSA signature checks.
        // Helps prevent CPU exhaustion attacks.

        // Skip ECDSA signature verification when connecting blocks
        // before the last block chain checkpoint. This is safe because block merkle hashes are
        // still computed and checked, and any change will be caught at the next checkpoint.
        if (fScriptChecks) {
            for (unsigned int i = 0; i < tx.vin.size(); i++) {
                const COutPoint &prevout = tx.vin[i].prevout;
                const CCoins* coins = inputs.AccessCoins(prevout.hash);
                assert(coins);

                // Verify signature
                CScriptCheck check(*coins, tx, i, flags | vSendPermissionFlags[i], cacheStore);
//                if (pvChecks) {
                if ( (pvChecks != NULL) && (vSendPermissionFlags[i] != 0) ) {
                    pvChecks->push_back(CScriptCheck());
                    check.swap(pvChecks->back());
                } else if (!check()) {
                    if (flags & STANDARD_NOT_MANDATORY_VERIFY_FLAGS) {
                        // Check whether the failure was caused by a
                        // non-mandatory script verification check, such as
                        // non-standard DER encodings or non-null dummy
                        // arguments; if so, don't trigger DoS protection to
                        // avoid splitting the network between upgraded and
                        // non-upgraded nodes.
                        CScriptCheck check2(*coins, tx, i,
                                flags & ~STANDARD_NOT_MANDATORY_VERIFY_FLAGS, cacheStore);
                        if (check2())
                            return state.Invalid(false, REJECT_NONSTANDARD, strprintf("non-mandatory-script-verify-flag (%s)", ScriptErrorString(check.GetScriptError())));
                    }
                    // Failures of other flags indicate a transaction that is
                    // invalid in new blocks, e.g. a invalid P2SH. We DoS ban
                    // such nodes as they are not following the protocol. That
                    // said during an upgrade careful thought should be taken
                    // as to the correct behavior - we may want to continue
                    // peering with non-upgraded nodes even after a soft-fork
                    // super-majority vote has passed.
/* MCHN START */                    
//                    return state.DoS(100,false, REJECT_INVALID, strprintf("mandatory-script-verify-flag-failed (%s)", ScriptErrorString(check.GetScriptError())));
                    return state.DoS(1,false, REJECT_INVALID, strprintf("mandatory-script-verify-flag-failed (%s)", ScriptErrorString(check.GetScriptError())));
/* MCHN END */                    
                }
            }
        }
    }

    return true;
}



bool DisconnectBlock(CBlock& block, CValidationState& state, CBlockIndex* pindex, CCoinsViewCache& view, bool* pfClean)
{
    assert(pindex->GetBlockHash() == view.GetBestBlock());

    if (pfClean)
        *pfClean = false;

    bool fClean = true;

    CBlockUndo blockUndo;
    CDiskBlockPos pos = pindex->GetUndoPos();
    if (pos.IsNull())
        return error("DisconnectBlock() : no undo data available");
    if (!blockUndo.ReadFromDisk(pos, pindex->pprev->GetBlockHash()))
        return error("DisconnectBlock() : failure reading undo data");

    if (blockUndo.vtxundo.size() + 1 != block.vtx.size())
        return error("DisconnectBlock() : block and undo data inconsistent");

    // undo transactions in reverse order
    for (int i = block.vtx.size() - 1; i >= 0; i--) {
        const CTransaction &tx = block.vtx[i];
        uint256 hash = tx.GetHash();

        // Check that all outputs are available and match the outputs in the block itself
        // exactly. Note that transactions with only provably unspendable outputs won't
        // have outputs available even in the block itself, so we handle that case
        // specially with outsEmpty.
        {
        CCoins outsEmpty;
        CCoinsModifier outs = view.ModifyCoins(hash);
        outs->ClearUnspendable();

        CCoins outsBlock(tx, pindex->nHeight);
        // The CCoins serialization does not serialize negative numbers.
        // No network rules currently depend on the version here, so an inconsistency is harmless
        // but it must be corrected before txout nversion ever influences a network rule.
        if (outsBlock.nVersion < 0)
            outs->nVersion = outsBlock.nVersion;
        if (*outs != outsBlock)
            fClean = fClean && error("DisconnectBlock() : added transaction mismatch? database corrupted");

        // remove outputs
        outs->Clear();
        }

        // restore inputs
        if (i > 0) { // not coinbases
            const CTxUndo &txundo = blockUndo.vtxundo[i-1];
            if (txundo.vprevout.size() != tx.vin.size())
                return error("DisconnectBlock() : transaction and undo data inconsistent");
            for (unsigned int j = tx.vin.size(); j-- > 0;) {
                const COutPoint &out = tx.vin[j].prevout;
                const CTxInUndo &undo = txundo.vprevout[j];
                CCoinsModifier coins = view.ModifyCoins(out.hash);
                if (undo.nHeight != 0) {
                    // undo data contains height: this is the last output of the prevout tx being spent
                    if (!coins->IsPruned())
                        fClean = fClean && error("DisconnectBlock() : undo data overwriting existing transaction");
                    coins->Clear();
                    coins->fCoinBase = undo.fCoinBase;
                    coins->nHeight = undo.nHeight;
                    coins->nVersion = undo.nVersion;
                } else {
                    if (coins->IsPruned())
                        fClean = fClean && error("DisconnectBlock() : undo data adding output to missing transaction");
                }
                if (coins->IsAvailable(out.n))
                    fClean = fClean && error("DisconnectBlock() : undo data overwriting existing output");
                if (coins->vout.size() < out.n+1)
                    coins->vout.resize(out.n+1);
                coins->vout[out.n] = undo.txout;
            }
        }
    }

    // move best block pointer to prevout block
    view.SetBestBlock(pindex->pprev->GetBlockHash());

    if (pfClean) {
        *pfClean = fClean;
        return true;
    } else {
        return fClean;
    }
}

void static FlushBlockFile(bool fFinalize = false)
{
    LOCK(cs_LastBlockFile);

    CDiskBlockPos posOld(nLastBlockFile, 0);

    FILE *fileOld = OpenBlockFile(posOld);
    if (fileOld) {
        if (fFinalize)
            TruncateFile(fileOld, vinfoBlockFile[nLastBlockFile].nSize);
        FileCommit(fileOld);
        fclose(fileOld);
    }

    fileOld = OpenUndoFile(posOld);
    if (fileOld) {
        if (fFinalize)
            TruncateFile(fileOld, vinfoBlockFile[nLastBlockFile].nUndoSize);
        FileCommit(fileOld);
        fclose(fileOld);
    }
}

bool FindUndoPos(CValidationState &state, int nFile, CDiskBlockPos &pos, unsigned int nAddSize);

static CCheckQueue<CScriptCheck> scriptcheckqueue(128);

void ThreadScriptCheck() {
    RenameThread("bitcoin-scriptch");
    scriptcheckqueue.Thread();
}

static int64_t nTimeVerify = 0;
static int64_t nTimeConnect = 0;
static int64_t nTimeIndex = 0;
static int64_t nTimeCallbacks = 0;
static int64_t nTimeTotal = 0;

bool ConnectBlock(const CBlock& block, CValidationState& state, CBlockIndex* pindex, CCoinsViewCache& view, bool fJustCheck)
{
    AssertLockHeld(cs_main);
    // Check it again in case a previous version let a bad block in
    // But if pindex->kMiner is set we got this block in this version
    if(!pindex->kMiner.IsValid() || (setBannedTxs.size() != 0) )
    {
        if (!CheckBlock(block, state, !fJustCheck, !fJustCheck))
        {
            return state.DoS(0, error("ConnectBlock() : error on CheckBlock"),
                             REJECT_INVALID, "bad-check-block");            
        }
    }

    if(!CheckBlockForUpgardableConstraints(block,state,"maximum-block-size",true))
    {
        return state.DoS(100, error("ConnectBlock() : above maximum-block-size"),
                         REJECT_INVALID, "bad-block-size");                        
    }
    if(!CheckBlockForUpgardableConstraints(block,state,"maximum-block-sigops",true))
    {
        return state.DoS(100, error("ConnectBlock() : above maximum-block-sigops"),
                         REJECT_INVALID, "bad-block-sigops");                        
    }
    
/* MCHN START */    
    uint256 block_hash;
    unsigned char miner_address[20];
/* MCHN START */        
    int offset = 80 + 1;
    if(block.vtx.size() >= 0xfd)
    {
        offset+=2;
    }
    if(block.vtx.size() > 0xffff)
    {
        offset+=2;
    }    
    int coinbase_offset=offset;
/* MCHN END */        
    
    block_hash=block.GetHash();
    if(!fJustCheck)
    {
        if(pindex->pprev)
        {
            if(fDebug)LogPrint("mchn","mchn: Connecting block %s (height %d) ...\n",block.GetHash().ToString().c_str(),pindex->pprev->nHeight+1);
        }
        else
        {
            if(fDebug)LogPrint("mchn","mchn: Connecting genesis block...\n");        
        }
        if(!CheckBlockPermissions(block,pindex->pprev,miner_address))
        {
            return state.DoS(100, error("ConnectBlock() : invalid permission changes or miner has no permission"),
                             REJECT_INVALID, "bad-perm-chngs");        
        }
    }
/* MCHN END */  
    
    
    // verify that the view's current state corresponds to the previous block
    uint256 hashPrevBlock = pindex->pprev == NULL ? uint256(0) : pindex->pprev->GetBlockHash();
    assert(hashPrevBlock == view.GetBestBlock());
    
/* MCHN START */        
    if(!fJustCheck)
    {
        mc_gState->m_Permissions->ClearMemPool();
        mc_gState->m_Assets->ClearMemPool();
        if(pMultiChainFilterEngine)
        {
            pMultiChainFilterEngine->Reset(pindex->nHeight-1,1);
        }
    }
/* MCHN END */        
    
    // Special case for the genesis block, skipping connection of its transactions
    // (its coinbase is unspendable)
/* MCHN START */        
    if (block.GetHash() == Params().HashGenesisBlock()){        
        if(fDebug)LogPrint("mchn","mchn: Checking block permission transactions for genesis block...\n");
        for (unsigned int i = 0; i < block.vtx.size(); i++)
        {
            const CTransaction &tx = block.vtx[i];
            string reason;
            if(!AcceptMultiChainTransaction(tx,view,offset,MC_AMT_DEFAULT,reason,NULL,NULL))
            {
                return state.DoS(100, error(reason.c_str()),
                             REJECT_INVALID, "bad-transaction");            
            }
//                unsigned char *root_stream_name;
            int root_stream_name_size;
            mc_gState->m_NetworkParams->GetParam("rootstreamname",&root_stream_name_size);        
            if(mc_gState->m_NetworkParams->IsProtocolMultichain() == 0)
            {
                root_stream_name_size=0;
            }    
            if(root_stream_name_size > 1)
            {
                if(pwalletTxsMain)
                {
                    if(mc_gState->m_WalletMode & MC_WMD_TXS)
                    {                        
                        mc_TxEntity entity;
                        uint256 genesis_hash=block.vtx[0].GetHash();
                        entity.Zero();

                        memcpy(entity.m_EntityID,(unsigned char*)&genesis_hash+MC_AST_SHORT_TXID_OFFSET,MC_AST_SHORT_TXID_SIZE);
                        entity.m_EntityType=MC_TET_STREAM | MC_TET_CHAINPOS;
                        pwalletTxsMain->AddEntity(&entity,0);
                        entity.m_EntityType=MC_TET_STREAM | MC_TET_TIMERECEIVED;
                        pwalletTxsMain->AddEntity(&entity,0);
                        entity.m_EntityType=MC_TET_STREAM_KEY | MC_TET_CHAINPOS;
                        pwalletTxsMain->AddEntity(&entity,0);
                        entity.m_EntityType=MC_TET_STREAM_KEY | MC_TET_TIMERECEIVED;
                        pwalletTxsMain->AddEntity(&entity,0);
                        entity.m_EntityType=MC_TET_STREAM_PUBLISHER | MC_TET_CHAINPOS;
                        pwalletTxsMain->AddEntity(&entity,0);
                        entity.m_EntityType=MC_TET_STREAM_PUBLISHER | MC_TET_TIMERECEIVED;
                        pwalletTxsMain->AddEntity(&entity,0);
                    }
                }
            }
            offset+=tx.GetSerializeSize(SER_NETWORK,tx.nVersion);
        }
        if(mc_gState->m_Permissions->Commit(miner_address,&block_hash) != 0)
        {
            return state.DoS(100, error("ConnectBlock() : error on permission commit for the genesis block"),
                             REJECT_INVALID, "bad-prm-commit");            
        }
        if(mc_gState->m_Assets->Commit() != 0)
        {
            return state.DoS(100, error("ConnectBlock() : error on asset commit for the genesis block"),
                             REJECT_INVALID, "bad-prm-commit");            
        }
/* MCHN END */        
        view.SetBestBlock(pindex->GetBlockHash());
        return true;
    }

    bool fScriptChecks = pindex->nHeight >= Checkpoints::GetTotalBlocksEstimate();
    
/* MCHN START */    
    
    fScriptChecks &= !fJustCheck;                                               // When miner checks the blocks before submission 
                                                                                // signature verification can fail because of lost send permission
    
    if(mc_gState->m_NetworkParams->IsProtocolMultichain() == 0)
    {
/* MCHN END */        

    // Do not allow blocks that contain transactions which 'overwrite' older transactions,
    // unless those are already completely spent.
    // If such overwrites are allowed, coinbases and transactions depending upon those
    // can be duplicated to remove the ability to spend the first instance -- even after
    // being sent to another address.
    // See BIP30 and http://r6.ca/blog/20120206T005236Z.html for more information.
    // This logic is not necessary for memory pool transactions, as AcceptToMemoryPool
    // already refuses previously-known transaction ids entirely.
    // This rule was originally applied all blocks whose timestamp was after March 15, 2012, 0:00 UTC.
    // Now that the whole chain is irreversibly beyond that time it is applied to all blocks except the
    // two in the chain that violate it. This prevents exploiting the issue against nodes in their
    // initial block download.
    bool fEnforceBIP30 = (!pindex->phashBlock) || // Enforce on CreateNewBlock invocations which don't have a hash.
                          !((pindex->nHeight==91842 && pindex->GetBlockHash() == uint256("0x00000000000a4d0a398161ffc163c503763b1f4360639393e0e4c8e300e0caec")) ||
                           (pindex->nHeight==91880 && pindex->GetBlockHash() == uint256("0x00000000000743f190a18c5577a3c2d2a1f610ae9601ac046a38084ccb7cd721")));
    if (fEnforceBIP30) {
        BOOST_FOREACH(const CTransaction& tx, block.vtx) {
            const CCoins* coins = view.AccessCoins(tx.GetHash());
            if (coins && !coins->IsPruned())
                return state.DoS(100, error("ConnectBlock() : tried to overwrite transaction"),
                                 REJECT_INVALID, "bad-txns-BIP30");
        }
    }

/* MCHN START */        
    }
/* MCHN END */        
    
    // BIP16 didn't become active until Apr 1 2012
    int64_t nBIP16SwitchTime = 1333238400;
    bool fStrictPayToScriptHash = (pindex->GetBlockTime() >= nBIP16SwitchTime);

    unsigned int flags = fStrictPayToScriptHash ? SCRIPT_VERIFY_P2SH : SCRIPT_VERIFY_NONE;

    // Start enforcing the DERSIG (BIP66) rules, for block.nVersion=3 blocks, when 75% of the network has upgraded:
    if (block.nVersion >= 3 && CBlockIndex::IsSuperMajority(3, pindex->pprev, Params().EnforceBlockUpgradeMajority())) {
        flags |= SCRIPT_VERIFY_DERSIG;
    }

    CBlockUndo blockundo;

    CCheckQueueControl<CScriptCheck> control(fScriptChecks && nScriptCheckThreads ? &scriptcheckqueue : NULL);

    int64_t nTimeStart = GetTimeMicros();
    CAmount nFees = 0;
    int nInputs = 0;
    unsigned int nSigOps = 0;

    CDiskTxPos pos(pindex->GetBlockPos(), GetSizeOfCompactSize(block.vtx.size()));
    std::vector<std::pair<uint256, CDiskTxPos> > vPos;
    vPos.reserve(block.vtx.size());
    blockundo.vtxundo.reserve(block.vtx.size() - 1);
    
    if(fDebug)LogPrint("mchn","mchn: Checking Block with %d transactions\n",block.vtx.size());
    
    for (unsigned int i = 0; i < block.vtx.size(); i++)
    {
        const CTransaction &tx = block.vtx[i];
        
        nInputs += tx.vin.size();
        nSigOps += GetLegacySigOpCount(tx);
        if (nSigOps > MAX_BLOCK_SIGOPS)
            return state.DoS(100, error("ConnectBlock() : too many sigops"),
                             REJECT_INVALID, "bad-blk-sigops");

/* MCHN START */    
        if(fDebug)LogPrint("mccoin", "COIN: NW Write  %s\n", tx.GetHash().ToString().c_str());
/* MCHN END */    
        if (!tx.IsCoinBase())
        {
            if (!view.HaveInputs(tx))
                return state.DoS(100, error("ConnectBlock() : inputs missing/spent"),
                                 REJECT_INVALID, "bad-txns-inputs-missingorspent");

            if (fStrictPayToScriptHash)
            {
                // Add in sigops done by pay-to-script-hash inputs;
                // this is to prevent a "rogue miner" from creating
                // an incredibly-expensive-to-validate block.
                nSigOps += GetP2SHSigOpCount(tx, view);
                if (nSigOps > MAX_BLOCK_SIGOPS)
                    return state.DoS(100, error("ConnectBlock() : too many sigops"),
                                     REJECT_INVALID, "bad-blk-sigops");
            }

            nFees += view.GetValueIn(tx)-tx.GetValueOut();

            std::vector<CScriptCheck> vChecks;
            if (!CheckInputs(tx, state, view, fScriptChecks, flags, false, nScriptCheckThreads ? &vChecks : NULL))
                return false;
            control.Add(vChecks);
            
/* MCHN START */        
            string reason;
            if(!fJustCheck)
            {
                if(!AcceptMultiChainTransaction(tx,view,offset,MC_AMT_DEFAULT,reason,NULL,NULL))
                {
                    return state.DoS(0,
                                     error("ConnectBlock: : AcceptMultiChainTransaction failed %s : %s", tx.GetHash().ToString(),reason),
                                     REJECT_INVALID, reason);
                }
            }
/* MCHN END */                    
        }
        
        offset+=tx.GetSerializeSize(SER_NETWORK,tx.nVersion);
                
        CTxUndo undoDummy;
        if (i > 0) {
            blockundo.vtxundo.push_back(CTxUndo());
        }
        UpdateCoins(tx, state, view, i == 0 ? undoDummy : blockundo.vtxundo.back(), pindex->nHeight);

        vPos.push_back(std::make_pair(tx.GetHash(), pos));
        pos.nTxOffset += ::GetSerializeSize(tx, SER_DISK, CLIENT_VERSION);
    }

/* MCHN START */        
    for (unsigned int i = 0; i < block.vtx.size(); i++)
    {
        const CTransaction &tx = block.vtx[i];
        if (tx.IsCoinBase())
        {
            string reason;
            if(!fJustCheck)
            {
                if(!AcceptMultiChainTransaction(tx,view,coinbase_offset,MC_AMT_DEFAULT,reason,NULL,NULL))
                {
                    return state.DoS(0,
                                     error("ConnectBlock: : AcceptMultiChainTransaction failed %s : %s", tx.GetHash().ToString(),reason),
                                     REJECT_INVALID, reason);
//                    return false;       
                }
            }
        }            
    }
/* MCHN END */        
    
    
    int64_t nTime1 = GetTimeMicros(); nTimeConnect += nTime1 - nTimeStart;
    if(fDebug)LogPrint("bench", "      - Connect %u transactions: %.2fms (%.3fms/tx, %.3fms/txin) [%.2fs]\n", (unsigned)block.vtx.size(), 0.001 * (nTime1 - nTimeStart), 0.001 * (nTime1 - nTimeStart) / block.vtx.size(), nInputs <= 1 ? 0 : 0.001 * (nTime1 - nTimeStart) / (nInputs-1), nTimeConnect * 0.000001);

    if (block.vtx[0].GetValueOut() > GetBlockValue(pindex->nHeight, nFees))
        return state.DoS(100,
                         error("ConnectBlock() : coinbase pays too much (actual=%d vs limit=%d)",
                               block.vtx[0].GetValueOut(), GetBlockValue(pindex->nHeight, nFees)),
                               REJECT_INVALID, "bad-cb-amount");

    if (!control.Wait())
        return state.DoS(100, false);
    int64_t nTime2 = GetTimeMicros(); nTimeVerify += nTime2 - nTimeStart;
    if(fDebug)LogPrint("bench", "    - Verify %u txins: %.2fms (%.3fms/txin) [%.2fs]\n", nInputs - 1, 0.001 * (nTime2 - nTimeStart), nInputs <= 1 ? 0 : 0.001 * (nTime2 - nTimeStart) / (nInputs-1), nTimeVerify * 0.000001);

    if (fJustCheck)
        return true;

    // Write undo information to disk
    if (pindex->GetUndoPos().IsNull() || !pindex->IsValid(BLOCK_VALID_SCRIPTS))
    {
        if (pindex->GetUndoPos().IsNull()) {
            CDiskBlockPos pos;
            if (!FindUndoPos(state, pindex->nFile, pos, ::GetSerializeSize(blockundo, SER_DISK, CLIENT_VERSION) + 40))
                return error("ConnectBlock() : FindUndoPos failed");
            if (!blockundo.WriteToDisk(pos, pindex->pprev->GetBlockHash()))
                return state.Abort("Failed to write undo data");

            // update nUndoPos in block index
            pindex->nUndoPos = pos.nPos;
            pindex->nStatus |= BLOCK_HAVE_UNDO;
        }

        pindex->RaiseValidity(BLOCK_VALID_SCRIPTS);
        setDirtyBlockIndex.insert(pindex);
    }

    if (fTxIndex)
        if (!pblocktree->WriteTxIndex(vPos))
            return state.Abort("Failed to write transaction index");

/* MCHN START */        
    if(fDebug)LogPrint("mchn","mchn: Committing permission changes for block %d...\n",mc_gState->m_Permissions->m_Block+1);
    if(mc_gState->m_Permissions->Commit(miner_address,&block_hash) != 0)
    {
        return state.DoS(100, error("ConnectBlock() : error on permission commit"),
                 REJECT_INVALID, "bad-prm-commit");            
    }
    if(mc_gState->m_Assets->Commit() != 0)
    {
        mc_gState->m_Permissions->RollBack();
        return state.DoS(100, error("ConnectBlock() : error on asset commit"),
                 REJECT_INVALID, "bad-prm-commit");            
    }
    
    setBlockTransactions[mc_gState->m_Permissions->m_Block%MC_TXSET_BLOCKS].clear();
    for (unsigned int i = 0; i < block.vtx.size(); i++)
    {
        setBlockTransactions[mc_gState->m_Permissions->m_Block%MC_TXSET_BLOCKS].insert(block.vtx[i].GetHash());
    }

/* MCHN END */        
    
    // add this block to the view's block chain
    view.SetBestBlock(pindex->GetBlockHash());

    int64_t nTime3 = GetTimeMicros(); nTimeIndex += nTime3 - nTime2;
    if(fDebug)LogPrint("bench", "    - Index writing: %.2fms [%.2fs]\n", 0.001 * (nTime3 - nTime2), nTimeIndex * 0.000001);

    // Watch for changes to the previous coinbase transaction.
    static uint256 hashPrevBestCoinBase;
    g_signals.UpdatedTransaction(hashPrevBestCoinBase);
    hashPrevBestCoinBase = block.vtx[0].GetHash();

    int64_t nTime4 = GetTimeMicros(); nTimeCallbacks += nTime4 - nTime3;
    if(fDebug)LogPrint("bench", "    - Callbacks: %.2fms [%.2fs]\n", 0.001 * (nTime4 - nTime3), nTimeCallbacks * 0.000001);

    return true;
}

enum FlushStateMode {
    FLUSH_STATE_IF_NEEDED,
    FLUSH_STATE_PERIODIC,
    FLUSH_STATE_ALWAYS
};

/**
 * Update the on-disk chain state.
 * The caches and indexes are flushed if either they're too large, forceWrite is set, or
 * fast is not set and it's been a while since the last write.
 */
bool static FlushStateToDisk(CValidationState &state, FlushStateMode mode) {
    LOCK(cs_main);
    static int64_t nLastWrite = 0;
    try {
    if ((mode == FLUSH_STATE_ALWAYS) ||
        ((mode == FLUSH_STATE_PERIODIC || mode == FLUSH_STATE_IF_NEEDED) && pcoinsTip->GetCacheSize() > nCoinCacheSize) ||
        (mode == FLUSH_STATE_PERIODIC && GetTimeMicros() > nLastWrite + DATABASE_WRITE_INTERVAL * 1000000)) {
        // Typical CCoins structures on disk are around 100 bytes in size.
        // Pushing a new one to the database can cause it to be written
        // twice (once in the log, and once in the tables). This is already
        // an overestimation, as most will delete an existing entry or
        // overwrite one. Still, use a conservative safety factor of 2.
        if (!CheckDiskSpace(100 * 2 * 2 * pcoinsTip->GetCacheSize()))
            return state.Error("out of disk space");
        // First make sure all block and undo data is flushed to disk.
        FlushBlockFile();
        // Then update all block file information (which may refer to block and undo files).
        bool fileschanged = false;
        for (set<int>::iterator it = setDirtyFileInfo.begin(); it != setDirtyFileInfo.end(); ) {
            if (!pblocktree->WriteBlockFileInfo(*it, vinfoBlockFile[*it])) {
                return state.Abort("Failed to write to block index");
            }
            fileschanged = true;
            setDirtyFileInfo.erase(it++);
        }
        if (fileschanged && !pblocktree->WriteLastBlockFile(nLastBlockFile)) {
            return state.Abort("Failed to write to block index");
        }
        for (set<CBlockIndex*>::iterator it = setDirtyBlockIndex.begin(); it != setDirtyBlockIndex.end(); ) {
             if (!pblocktree->WriteBlockIndex(CDiskBlockIndex(*it))) {
                 return state.Abort("Failed to write to block index");
             }
             setDirtyBlockIndex.erase(it++);
        }
        pblocktree->Sync();
        // Finally flush the chainstate (which may refer to block index entries).
        if (!pcoinsTip->Flush())
            return state.Abort("Failed to write to coin database");
        // Update best block in wallet (so we can detect restored wallets).
        if (mode != FLUSH_STATE_IF_NEEDED) {
            g_signals.SetBestChain(chainActive.GetLocator());
        }
        nLastWrite = GetTimeMicros();
    }
    } catch (const std::runtime_error& e) {
        return state.Abort(std::string("System error while flushing: ") + e.what());
    }
    return true;
}

void FlushStateToDisk() {
    CValidationState state;
    FlushStateToDisk(state, FLUSH_STATE_ALWAYS);
}

/** Update chainActive and related internal data structures. */
void static UpdateTip(CBlockIndex *pindexNew) {
    chainActive.SetTip(pindexNew);

    // New best block
    nTimeBestReceived = GetTime();
    mempool.AddTransactionsUpdated(1);

    LogPrintf("UpdateTip:            new best=%s  height=%d  log2_work=%.8g  tx=%lu  date=%s progress=%f  cache=%u\n",
      chainActive.Tip()->GetBlockHash().ToString(), chainActive.Height(), log(chainActive.Tip()->nChainWork.getdouble())/log(2.0), (unsigned long)chainActive.Tip()->nChainTx,
      DateTimeStrFormat("%Y-%m-%d %H:%M:%S", chainActive.Tip()->GetBlockTime()),
      Checkpoints::GuessVerificationProgress(chainActive.Tip()), (unsigned int)pcoinsTip->GetCacheSize());
    
    if(chainActive.Tip()->kMiner.IsValid())
    {
        CBitcoinAddress addr=CBitcoinAddress(chainActive.Tip()->kMiner.GetID());
        if(fDebug)LogPrint("mcblock","mchn-block: height: %d, miner: %s\n", chainActive.Tip()->nHeight,addr.ToString().c_str());
    }
    cvBlockChange.notify_all();

    // Check the version of the last 100 blocks to see if we need to upgrade:
    static bool fWarned = false;
    if (!IsInitialBlockDownload() && !fWarned)
    {
        int nUpgraded = 0;
        const CBlockIndex* pindex = chainActive.Tip();
        for (int i = 0; i < 100 && pindex != NULL; i++)
        {
            if (pindex->nVersion > CBlock::CURRENT_VERSION)
                ++nUpgraded;
            pindex = pindex->pprev;
        }
        if (nUpgraded > 0)
            LogPrintf("SetBestChain: %d of last 100 blocks above version %d\n", nUpgraded, (int)CBlock::CURRENT_VERSION);
        if (nUpgraded > 100/2)
        {
            // strMiscWarning is read by GetWarnings(), called by Qt and the JSON-RPC code to warn the user:
            strMiscWarning = _("Warning: This version is obsolete, upgrade required!");
            CAlert::Notify(strMiscWarning, true);
            fWarned = true;
        }
    }
}

/** Disconnect chainActive's tip. */
bool static DisconnectTip(CValidationState &state) {
    CBlockIndex *pindexDelete = chainActive.Tip();
    assert(pindexDelete);
    mempool.check(pcoinsTip);
    // Read block from disk.
    CBlock block;
    if(fDebug)LogPrint("mcblockperf","mchn-block-perf: Disconnecting block %s (height %d), %d transactions in mempool\n",pindexDelete->GetBlockHash().ToString(),pindexDelete->nHeight,(int)mempool.size());
    if (!ReadBlockFromDisk(block, pindexDelete))
        return state.Abort("Failed to read block");
    // Apply the block atomically to the chain state.
    int64_t nStart = GetTimeMicros();
    {
        CCoinsViewCache view(pcoinsTip);
        int err;
        err=pEF->FED_EventBlock(block, state, pindexDelete,"remove",false,false);
        if(err)
        {
            LogPrintf("ERROR: Cannot disconnect(before) block %s in feeds, error %d\n",pindexDelete->GetBlockHash().ToString().c_str(),err);
        }        
        
        if (!DisconnectBlock(block, state, pindexDelete, view))
        {
            err=pEF->FED_EventBlock(block, state, pindexDelete,"remove",true,true);
            if(err)
            {
                LogPrintf("ERROR: Cannot disconnect(error) block %s in feeds, error %d\n",pindexDelete->GetBlockHash().ToString().c_str(),err);
            }        
            return error("DisconnectTip() : DisconnectBlock %s failed", pindexDelete->GetBlockHash().ToString());
        }
        
        err=pEF->FED_EventBlock(block, state, pindexDelete,"remove",true,false);
        if(err)
        {
            LogPrintf("ERROR: Cannot disconnect(after) block %s in feeds, error %d\n",pindexDelete->GetBlockHash().ToString().c_str(),err);
        }        
        assert(view.Flush());
    }
    if(fDebug)LogPrint("bench", "- Disconnect block: %.2fms\n", (GetTimeMicros() - nStart) * 0.001);
    // Write the chain state to disk, if necessary.
    if (!FlushStateToDisk(state, FLUSH_STATE_ALWAYS))// MCHN was FLUSH_STATE_IF_NEEDED
        return false;
    
/* MCHN START */    
    if(fDebug)LogPrint("mcblockperf","mchn-block-perf: Mempool hash list defragmentation\n");
    int old_height=pindexDelete->nHeight;
    mempool.defragmentHashList();
    int new_txs=mempool.hashList->m_Count;
    if(fDebug)LogPrint("mchn","mchn: Disconnecting block %s (height %d) from permission DB (%d transactions in mempool)\n",pindexDelete->GetBlockHash().ToString(),old_height,new_txs);
    mempool.shiftHashList(block.vtx.size());
    setBlockTransactions[old_height%MC_TXSET_BLOCKS].clear();
    if(fDebug)LogPrint("mcblockperf","mchn-block-perf: Rolling back permission and asset databases\n");
    mc_gState->m_Permissions->RollBack(old_height-1);
    mc_gState->m_Assets->RollBack(old_height-1);
    if(pMultiChainFilterEngine)
    {
        pMultiChainFilterEngine->Reset(old_height-1,0);
    }
    
    MultichainNode_ApplyUpgrades(old_height-1);        
    if(mc_gState->m_WalletMode & MC_WMD_TXS)
    {
        if(fDebug)LogPrint("mcblockperf","mchn-block-perf: Rolling back wallet             (%s)\n",pwalletTxsMain->Summary());
        pwalletTxsMain->RollBack(NULL,old_height-1);
        if(fDebug)LogPrint("mcblockperf","mchn-block-perf: Rolling back wallet completed   (%s)\n",pwalletTxsMain->Summary());
    }
    if(fDebug)LogPrint("mcblockperf","mchn-block-perf: Resurrecting mempool transactions from the disconnected block\n");
/* MCHN END */    
    
    // Resurrect mempool transactions from the disconnected block.
    BOOST_FOREACH(const CTransaction &tx, block.vtx) {
        // ignore validation errors in resurrected transactions
        list<CTransaction> removed;
        CValidationState stateDummy;
        if (tx.IsCoinBase() || !AcceptToMemoryPool(mempool, stateDummy, tx, false, NULL))
        {
            LogPrintf("Tx not accepted in resurrection after block: %s\n",tx.GetHash().ToString().c_str());
            string reason=(stateDummy.GetRejectReason().size() > 0) ? stateDummy.GetRejectReason() : "unknown";
            mempool.remove(tx, removed, true, "resurrection: "+reason);
        }
    }
/* MCHN START */    
    int new_shift=mempool.hashList->m_Count-new_txs;
/* MCHN END */    
    mempool.removeCoinbaseSpends(pcoinsTip, pindexDelete->nHeight);
/* MCHN START */    
//    mempool.defragmentHashList();
    if(fDebug)LogPrint("mcblockperf","mchn-block-perf: Replaying mempool               (%s)\n",(mc_gState->m_WalletMode & MC_WMD_TXS) ? pwalletTxsMain->Summary() : "");
        ReplayMemPool(mempool,new_shift,true);
    if(fDebug)LogPrint("mcblockperf","mchn-block-perf: Replaying mempool completed     (%s)\n",(mc_gState->m_WalletMode & MC_WMD_TXS) ? pwalletTxsMain->Summary() : "");
    if(fDebug)LogPrint("mcblockperf","mchn-block-perf: Mempool hash list defragmentation\n");
    mempool.defragmentHashList();
/* MCHN END */    
    
    mempool.check(pcoinsTip);
    // Update chainActive and related variables.
    UpdateTip(pindexDelete->pprev);
    // Let wallets know transactions went from 1-confirmed to
    // 0-confirmed or conflicted:
    BOOST_FOREACH(const CTransaction &tx, block.vtx) {
        if(((mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS) == 0) || (mc_gState->m_WalletMode & MC_WMD_MAP_TXS))
        {
            SyncWithWallets(tx, NULL);
        }
    }
    if(fDebug)LogPrint("mcblockperf","mchn-block-perf: Disconnecting block completed, %d transactions in mempool\n",(int)mempool.size());
    return true;
}

static int64_t nTimeReadFromDisk = 0;
static int64_t nTimeConnectTotal = 0;
static int64_t nTimeFlush = 0;
static int64_t nTimeChainState = 0;
static int64_t nTimePostConnect = 0;

/** 
 * Connect a new block to chainActive. pblock is either NULL or a pointer to a CBlock
 * corresponding to pindexNew, to bypass loading it again from disk.
 */
bool static ConnectTip(CValidationState &state, CBlockIndex *pindexNew, CBlock *pblock) {
    assert(pindexNew->pprev == chainActive.Tip());
    mempool.check(pcoinsTip);
    // Read block from disk.
    int64_t nTime1 = GetTimeMicros();
    CBlock block;
    if (!pblock) {
        if (!ReadBlockFromDisk(block, pindexNew))
            return state.Abort("Failed to read block");
        pblock = &block;
    }        
    LogPrint("mcblockperf","mchn-block-perf: Connecting block %s (height %d), %d transactions in mempool, %d in orphan pool (%d bytes)\n",
            pindexNew->GetBlockHash().ToString().c_str(),pindexNew->nHeight,(int)mempool.size(),(int)OrphanPoolSize(),nOrphanPoolSize);
    // Apply the block atomically to the chain state.
    int64_t nTime2 = GetTimeMicros(); nTimeReadFromDisk += nTime2 - nTime1;
    int64_t nTime3;
    if(fDebug)LogPrint("bench", "  - Load block from disk: %.2fms [%.2fs]\n", (nTime2 - nTime1) * 0.001, nTimeReadFromDisk * 0.000001);
/*    
    if(pMultiChainFilterEngine)
    {
        pMultiChainFilterEngine->Reset(pindexNew->nHeight-1,1);
    }
 */ 
    if(pindexNew->nHeight == 0)
    {
        pindexNew->nSize=GenesisBlockSize;
    }
    {
        CCoinsViewCache view(pcoinsTip);
        CInv inv(MSG_BLOCK, pindexNew->GetBlockHash());
        int err;
        err=pEF->FED_EventBlock(*pblock, state, pindexNew,"check",false,false);
        if(err)
        {
            LogPrintf("ERROR: Cannot connect(before) block %s in feeds, error %d\n",pindexNew->GetBlockHash().ToString().c_str(),err);
        }        
        if(pMultiChainFilterEngine)
        {
            pMultiChainFilterEngine->m_CoinsCache=&view;
        }
        bool rv = ConnectBlock(*pblock, state, pindexNew, view);        
        if(pMultiChainFilterEngine)
        {
            pMultiChainFilterEngine->m_CoinsCache=NULL;
        }
        if(rv)
        {
            if(!VerifyBlockSignatureType(pblock))
            {
                rv=state.DoS(100, error("ConnectBlock() : bad miner signature type"),
                         REJECT_INVALID, "bad-miner-signature-type");                        
            }
        }
        g_signals.BlockChecked(*pblock, state);
        
        if (!rv) {            
            err=pEF->FED_EventBlock(*pblock, state, pindexNew,"check",true,true);
            if(err)
            {
                LogPrintf("ERROR: Cannot connect(error) block %s in feeds, error %d\n",pindexNew->GetBlockHash().ToString().c_str(),err);
            }        
            if (state.IsInvalid())
                InvalidBlockFound(pindexNew, state);
            return error("ConnectTip() : ConnectBlock %s failed", pindexNew->GetBlockHash().ToString());
        }
        
        mapBlockSource.erase(inv.hash);
        nTime3 = GetTimeMicros(); nTimeConnectTotal += nTime3 - nTime2;
        if(fDebug)LogPrint("bench", "  - Connect total: %.2fms [%.2fs]\n", (nTime3 - nTime2) * 0.001, nTimeConnectTotal * 0.000001);
        assert(view.Flush());
        err=pEF->FED_EventBlock(*pblock, state, pindexNew,"check",true,false);
        if(err)
        {
            LogPrintf("ERROR: Cannot connect(before) block %s in feeds, error %d\n",pindexNew->GetBlockHash().ToString().c_str(),err);
        }        
    }
    int64_t nTime4 = GetTimeMicros(); nTimeFlush += nTime4 - nTime3;
    if(fDebug)LogPrint("bench", "  - Flush: %.2fms [%.2fs]\n", (nTime4 - nTime3) * 0.001, nTimeFlush * 0.000001);
    // Write the chain state to disk, if necessary.
    if (!FlushStateToDisk(state, FLUSH_STATE_ALWAYS))// MCHN was FLUSH_STATE_IF_NEEDED
        return false;
    int64_t nTime5 = GetTimeMicros(); nTimeChainState += nTime5 - nTime4;
    if(fDebug)LogPrint("bench", "  - Writing chainstate: %.2fms [%.2fs]\n", (nTime5 - nTime4) * 0.001, nTimeChainState * 0.000001);
    // Remove conflicting transactions from the mempool.
    list<CTransaction> txConflicted;
    if(fDebug)LogPrint("mcblockperf","mchn-block-perf: Updating feeds\n");

    int err=MC_ERR_NOERROR;
    if(pindexNew->nHeight)
    {
        err=pEF->FED_EventBlock(*pblock, state, pindexNew,"add",false,false);
        if(err)
        {
            LogPrintf("ERROR: Cannot add(before) block %s in feeds, error %d\n",pindexNew->GetBlockHash().ToString().c_str(),err);
        }        
        CDiskTxPos pos1(pindexNew->GetBlockPos(), 80+GetSizeOfCompactSize(pblock->vtx.size()));
        for (unsigned int i = 0; i < pblock->vtx.size(); i++)
        {
            const CTransaction &tx = pblock->vtx[i];
            err=pEF->FED_EventTx(tx,pindexNew->nHeight,&pos1,i,pindexNew->GetBlockHash(),pblock->nTime);
            if(err)
            {
                LogPrintf("ERROR: Cannot write tx %s to feeds in block, error %d\n",tx.GetHash().ToString().c_str(),err);
            }
            pos1.nTxOffset += ::GetSerializeSize(tx, SER_DISK, CLIENT_VERSION);
        }
        err=pEF->FED_EventBlock(*pblock, state, pindexNew,"add",true,false);
        if(err)
        {
            LogPrintf("ERROR: Cannot add(after) block %s in feeds, error %d\n",pindexNew->GetBlockHash().ToString().c_str(),err);
        }        
    }

    if(fDebug)LogPrint("mcblockperf","mchn-block-perf: Removing block txs from mempool\n");
    mempool.removeForBlock(pblock->vtx, pindexNew->nHeight, txConflicted);
    mempool.check(pcoinsTip);
/* MCHN START */    
    BOOST_FOREACH(const CTransaction &tx, pblock->vtx) {
        EraseOrphanTx(tx.GetHash());
    }    

    if(fDebug)LogPrint("wallet","wtxs: Committing block %d\n",pindexNew->nHeight);
    
    err=MC_ERR_NOERROR;
    if(fDebug)LogPrint("mcblockperf","mchn-block-perf: Wallet, before commit           (%s)\n",(mc_gState->m_WalletMode & MC_WMD_TXS) ? pwalletTxsMain->Summary() : "");
    err=pwalletTxsMain->BeforeCommit(NULL);
    if(fDebug)LogPrint("mcblockperf","mchn-block-perf: Wallet, before commit completed (%s)\n",(mc_gState->m_WalletMode & MC_WMD_TXS) ? pwalletTxsMain->Summary() : "");
    if(err)
    {
        LogPrintf("ConnectTip() : ConnectBlock %s failed, Wtxs BeforeCommit, error: %d", pindexNew->GetBlockHash().ToString(),err);
    }
    CDiskTxPos pos(pindexNew->GetBlockPos(), GetSizeOfCompactSize(pblock->vtx.size()));
    if(fDebug)LogPrint("mcblockperf","mchn-block-perf: Adding block txs to wallet\n");
    for (unsigned int i = 0; i < pblock->vtx.size(); i++)
    {
        if(err == MC_ERR_NOERROR)
        {
            const CTransaction &tx = pblock->vtx[i];
            err=pwalletTxsMain->AddTx(NULL,tx,pindexNew->nHeight,&pos,i,pindexNew->GetBlockHash());
            if(err)
            {
                LogPrintf("Wallet error when connecting block %s, Tx %s, error: %d\n", pindexNew->GetBlockHash().ToString(),tx.GetHash().ToString(),err);
            }
            pos.nTxOffset += ::GetSerializeSize(tx, SER_DISK, CLIENT_VERSION);
        }
    }
    
    if(pindexNew->nHeight)
    {
        err=pEF->FED_EventChunksAvailable();
        if(err)
        {
            LogPrintf("ERROR: Cannot write offchain items in block, error %d\n",err);
        }
    }
    
    int blk_sleep=GetArg("-debug-block-total-sleep",0);
    int wrp_sleep=GetArg("-debug-block-wallet-sleep",0);
    blk_sleep-=wrp_sleep;
    if(blk_sleep > 0)
    {
        LogPrintf("DEBUG: block-no-wallet sleep - start\n");
        MilliSleep(blk_sleep);
        LogPrintf("DEBUG: block-no-wallet sleep - end\n");
    }
    
    pwalletTxsMain->WRPWriteLock();
    if(wrp_sleep > 0)
    {
        LogPrintf("DEBUG: block-wallet sleep - start\n");
        MilliSleep(wrp_sleep);
        LogPrintf("DEBUG: block-wallet sleep - end\n");
    }
    if(fDebug)LogPrint("mcwrp","mcwrp: DB synchronization for block %d\n",pindexNew->nHeight);
    
    if(fDebug)LogPrint("mcblockperf","mchn-block-perf: Wallet, commit                  (%s)\n",(mc_gState->m_WalletMode & MC_WMD_TXS) ? pwalletTxsMain->Summary() : "");
    if(err == MC_ERR_NOERROR)
    {
        err=pwalletTxsMain->Commit(NULL);    
        if(err)
        {
            LogPrintf("ConnectTip() : ConnectBlock %s failed, Wtxs Commit, error: %d", pindexNew->GetBlockHash().ToString(),err);
        }    
    }
    if(fDebug)LogPrint("mcblockperf","mchn-block-perf: Wallet, commit completed        (%s)\n",(mc_gState->m_WalletMode & MC_WMD_TXS) ? pwalletTxsMain->Summary() : "");
    if(fDebug)LogPrint("mcblockperf","mchn-block-perf: Wallet cleanup\n");
    if(err == MC_ERR_NOERROR)
    {
        err=pwalletTxsMain->CleanUpAfterBlock(NULL,pindexNew->nHeight,pindexNew->nHeight-1);
        if(err)
        {
            LogPrintf("ConnectTip() : ConnectBlock %s failed, Wtxs CleanUpAfterBlock, error: %d", pindexNew->GetBlockHash().ToString(),err);
        }    
    }
    
    vector<uint256> vBlockTxs;
    BOOST_FOREACH(const CTransaction &tx, pblock->vtx) {
        vBlockTxs.push_back(tx.GetHash());
    }    
    
    {
        LOCK(cs_vNodes);
        BOOST_FOREACH(CNode* pnode, vNodes)
        {
            pnode->RemoveTxsInFlight(vBlockTxs);
        }        
    }
            
/* MCHN END */    
    // Update chainActive & related variables.
    UpdateTip(pindexNew);
    // Tell wallet about transactions that went from mempool
    // to conflicted:
    BOOST_FOREACH(const CTransaction &tx, txConflicted) {
        if(((mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS) == 0) || (mc_gState->m_WalletMode & MC_WMD_MAP_TXS))
        {        
            SyncWithWallets(tx, NULL);
        }
    }
    // ... and about transactions that got confirmed:
/* MCHN START */        
    VerifyBlockSignature(pblock,false);
    MultichainNode_ApplyUpgrades(chainActive.Height());    
/* MCHN END */    
    
    BOOST_FOREACH(const CTransaction &tx, pblock->vtx) {
        if(((mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS) == 0) || (mc_gState->m_WalletMode & MC_WMD_MAP_TXS))
        {        
            SyncWithWallets(tx, pblock);
        }
    }
    
/* MCHN START */        
//    CTransaction emptyTx;                                                       // Triggering wallet optimization
//    SyncWithWallets(emptyTx, pblock);
/* MCHN END */    
    if(fDebug)LogPrint("mcblockperf","mchn-block-perf: Connecting block completed, %d transactions in mempool\n",(int)mempool.size());

    int64_t nTime6 = GetTimeMicros(); nTimePostConnect += nTime6 - nTime5; nTimeTotal += nTime6 - nTime1;
    if(fDebug)LogPrint("bench", "  - Connect postprocess: %.2fms [%.2fs]\n", (nTime6 - nTime5) * 0.001, nTimePostConnect * 0.000001);
    if(fDebug)LogPrint("bench", "- Connect block: %.2fms [%.2fs]\n", (nTime6 - nTime1) * 0.001, nTimeTotal * 0.000001);
    return true;
}

bool RecoverAfterCrash()
{
    LOCK(cs_main);
    
    CBlock block;
    CBlock *pblock;
    CValidationState state;
    CBlockIndex *pindexNew=chainActive.Tip();
    int err;
    
    if(chainActive.Height() <= 0)
    {
        return true;
    }
    
    if (!ReadBlockFromDisk(block, pindexNew))
    {
        LogPrintf("ERROR: Cannot load last block from disk\n");
        return false;
    }
    pblock=&block;
    
    err=pEF->FED_EventBlock(*pblock, state, pindexNew,"check",true,true);
    if(err)
    {
        LogPrintf("ERROR: Cannot connect(error) block %s in feeds, error %d\n",pindexNew->GetBlockHash().ToString().c_str(),err);
        return false;
    }        
    
    err=pEF->FED_EventBlock(*pblock, state, pindexNew,"check",true,false);
    if(err)
    {
        LogPrintf("ERROR: Cannot connect(before) block %s in feeds, error %d\n",pindexNew->GetBlockHash().ToString().c_str(),err);
        return false;
    }        
    
    err=pEF->FED_EventBlock(*pblock, state, pindexNew,"add",false,false);
    if(err)
    {
        LogPrintf("ERROR: Cannot add(before) block %s in feeds, error %d\n",pindexNew->GetBlockHash().ToString().c_str(),err);
        return false;
    }        
    
    CDiskTxPos pos1(pindexNew->GetBlockPos(), 80+GetSizeOfCompactSize(pblock->vtx.size()));
    for (unsigned int i = 0; i < pblock->vtx.size(); i++)
    {
        const CTransaction &tx = pblock->vtx[i];
        err=pEF->FED_EventTx(tx,pindexNew->nHeight,&pos1,i,pindexNew->GetBlockHash(),pblock->nTime);
        if(err)
        {
            LogPrintf("ERROR: Cannot write tx %s to feeds in block, error %d\n",tx.GetHash().ToString().c_str(),err);
            return false;
        }
        pos1.nTxOffset += ::GetSerializeSize(tx, SER_DISK, CLIENT_VERSION);
    }
    
    err=pEF->FED_EventBlock(*pblock, state, pindexNew,"add",true,false);
    if(err)
    {
        LogPrintf("ERROR: Cannot add(after) block %s in feeds, error %d\n",pindexNew->GetBlockHash().ToString().c_str(),err);
        return false;
    }        

    for(int pos=0;pos<mempool.hashList->m_Count;pos++)
    {
        uint256 hash=*(uint256*)mempool.hashList->GetRow(pos);
        if(mempool.exists(hash))
        {
            const CTxMemPoolEntry entry=mempool.mapTx[hash];
            const CTransaction& tx = entry.GetTx();            
            err=pEF->FED_EventTx(tx,-1,NULL,-1,0,0);            
            if(err)
            {
                LogPrintf("ERROR: Cannot write tx %s to feeds in mempool, error %d\n",tx.GetHash().ToString().c_str(),err);
                return false;
            }
        }
    }
    
    return true;
}


/**
 * Return the tip of the chain with the most work in it, that isn't
 * known to be invalid (it's however far from certain to be valid).
 */
static CBlockIndex* FindMostWorkChain() {
    do {
        CBlockIndex *pindexNew = NULL;

        // Find the best candidate header.
        {
/* MCHN START */            
            bool take_it=false;
            uint32_t max_work,work;
            int max_count;
            if(setBlockIndexCandidates.size() > 1)
            {
                max_work=0;
                std::set<CBlockIndex*, CBlockIndexWorkComparator>::iterator it;
                for (it = setBlockIndexCandidates.begin(); it != setBlockIndexCandidates.end(); ++it)
                {
                    CBlockIndex* pindex=*it;
                    work=(uint32_t)mc_GetLE(&(pindex->nChainWork),32);
                    if(work > max_work)
                    {
                        max_work=work;
                    }
                }
                max_count=0;
                for (it = setBlockIndexCandidates.begin(); it != setBlockIndexCandidates.end(); ++it)
                {
                    CBlockIndex* pindex=*it;
                    work=(uint32_t)mc_GetLE(&(pindex->nChainWork),32);
                    if(work == max_work)
                    {
                        max_count++;
                        VerifyBlockMiner(NULL,pindex);                          // Optimization in case of very long forks - start from the end
                    }                            
                }
                if(max_count>1)
                {
                    take_it=true;
                }
                
                if(take_it)
                {
                    if(fDebug)LogPrint("mcblock","mchn-block: Choosing chain from %d candidates, current height: %d\n",(int)setBlockIndexCandidates.size(),chainActive.Tip()->nHeight);
                    for (it = setBlockIndexCandidates.begin(); it != setBlockIndexCandidates.end(); ++it)
                    {
                        CBlockIndex* pindex=*it;
                        work=(uint32_t)mc_GetLE(&(pindex->nChainWork),32);
                        if(fDebug)LogPrint("mcblock","mchn-block: Forked block index: %s, work: %d, height: %d, mined-by-me: %d, can-mine: %d\n",pindex->GetBlockHash().ToString().c_str(),
                                work, pindex->nHeight,pindex->nHeightMinedByMe,pindex->nCanMine);                    
                    }                
                }
            }
            
            set<CBlockIndex*, CBlockIndexWorkComparator> setTempBlockIndexCandidates;
            std::set<CBlockIndex*, CBlockIndexWorkComparator>::iterator fit;
            
/*            
            const CBlockIndex *pindexLockedFork;
            pindexLockedFork=NULL;
            if(pindexLockedBlock)
            {
                if(!chainActive.Contains(pindexLockedBlock))
                {
                    pindexLockedFork=chainActive.FindFork(pindexLockedBlock);                
                }
            }
*/
            
            for (fit = setBlockIndexCandidates.begin(); fit != setBlockIndexCandidates.end(); ++fit)
            {
//                bool fLocked=false;                
                CBlockIndex *pindexCandidate=*fit;
                if(pindexLockedBlock)
                {
                    CBlockIndex *pindexCommonAncestor;
                    pindexCommonAncestor=LastCommonAncestor(pindexCandidate,pindexLockedBlock);
                    if(pindexCommonAncestor != pindexLockedBlock)
                    {
                        pindexCandidate=pindexCommonAncestor;
                    }
/*                    
                    const CBlockIndex *pindexFork;
                    pindexFork=chainActive.FindFork(*fit);
                    if(pindexLockedFork)
                    {
                        if(pindexFork->nHeight < pindexLockedFork->nHeight)
                        {
                            fLocked=true;
                        }                        
                    }
                    else
                    {
                        if(pindexFork->nHeight < pindexLockedBlock->nHeight)
                        {
                            fLocked=true;
                        }            
                    }                    
 */ 
                }
                if(!pindexCandidate->fPassedMinerPrecheck)
                {
                    if(!VerifyBlockMiner(NULL,pindexCandidate))
                    {
                        pindexCandidate->nStatus |= BLOCK_FAILED_VALID;
                        setDirtyBlockIndex.insert(pindexCandidate);
                    }
                }

                
//                if(!fLocked)
                if(pindexCandidate->fPassedMinerPrecheck)
                {
                    if(setTempBlockIndexCandidates.find(pindexCandidate) == setTempBlockIndexCandidates.end())
                    {
    //                    setTempBlockIndexCandidates.insert(*fit);
                        setTempBlockIndexCandidates.insert(pindexCandidate);
                    }
                }
            }
            
/* MCHN END */                        
/*            
            std::set<CBlockIndex*, CBlockIndexWorkComparator>::reverse_iterator it = setBlockIndexCandidates.rbegin();
            if (it == setBlockIndexCandidates.rend())
                return NULL;
 */ 
            std::set<CBlockIndex*, CBlockIndexWorkComparator>::reverse_iterator it = setTempBlockIndexCandidates.rbegin();
            if (it == setTempBlockIndexCandidates.rend())
                return NULL;
            pindexNew = *it;
/* MCHN START */            
            if(take_it)
            {
                CBlockIndex* pindex=*it;
                work=(uint32_t)mc_GetLE(&(pindex->nChainWork),32);
                if(fDebug)LogPrint("mcblock","mchn-block: Selected forked block index: %s, Active chain tip: %s\n",pindex->GetBlockHash().ToString().c_str(),
                        chainActive.Tip()->GetBlockHash().ToString().c_str());                    
                
            }
/* MCHN END */                        
        }

        // Check whether all blocks on the path between the currently active chain and the candidate are valid.
        // Just going until the active chain is an optimization, as we know all blocks in it are valid already.
        CBlockIndex *pindexTest = pindexNew;
        bool fInvalidAncestor = false;
        while (pindexTest && !chainActive.Contains(pindexTest)) {
            assert(pindexTest->nStatus & BLOCK_HAVE_DATA);
            assert(pindexTest->nChainTx || pindexTest->nHeight == 0);
            if ( (pindexTest->nStatus & BLOCK_FAILED_MASK) || ( setBannedTxBlocks.find(pindexTest->GetBlockHash()) != setBannedTxBlocks.end())){
                // Candidate has an invalid ancestor, remove entire chain from the set.
                if (pindexBestInvalid == NULL || pindexNew->nChainWork > pindexBestInvalid->nChainWork)
                    pindexBestInvalid = pindexNew;
                CBlockIndex *pindexFailed = pindexNew;
                while (pindexTest != pindexFailed) {
                    pindexFailed->nStatus |= BLOCK_FAILED_CHILD;
                    setBlockIndexCandidates.erase(pindexFailed);
                    pindexFailed = pindexFailed->pprev;
                }
                setBlockIndexCandidates.erase(pindexTest);
                fInvalidAncestor = true;
                break;
            }
            pindexTest = pindexTest->pprev;
        }
        if (!fInvalidAncestor)
            return pindexNew;
    } while(true);
}

/** Delete all entries in setBlockIndexCandidates that are worse than the current tip. */
static void PruneBlockIndexCandidates() {
    // Note that we can't delete the current block itself, as we may need to return to it later in case a
    // reorganization to a better block fails.
    std::set<CBlockIndex*, CBlockIndexWorkComparator>::iterator it = setBlockIndexCandidates.begin();
    while (it != setBlockIndexCandidates.end() && setBlockIndexCandidates.value_comp()(*it, chainActive.Tip())) {
        setBlockIndexCandidates.erase(it++);
    }
    // Either the current tip or a successor of it we're working towards is left in setBlockIndexCandidates.
    assert(!setBlockIndexCandidates.empty());
}

/* MCHN START */

void UpdateChainMiningStatus(const CBlock &block,CBlockIndex *pindexNew) 
{
    if(mc_gState->m_NetworkParams->IsProtocolMultichain())
    {
        std::vector<unsigned char> vchPubKey=std::vector<unsigned char> (block.vSigner+1, block.vSigner+1+block.vSigner[0]);
        CPubKey pubKeyOut(vchPubKey);
        CKeyID keyID=pubKeyOut.GetID();
        CKey key;
        pindexNew->nCanMine=mc_gState->m_Permissions->CanMine(NULL,keyID.begin());
        if(pwalletMain)
        {
            if(pwalletMain->GetKey(keyID,key))
            {
                pindexNew->nHeightMinedByMe=pindexNew->nHeight;
                if(pindexNew->pprev)
                {
                    pindexNew->nCanMine=pindexNew->pprev->nCanMine;
                }                
                else
                {
                    pindexNew->nCanMine=MC_PTP_MINE;                    
                }
                if(mc_gState->m_Permissions->GetActiveMinerCount()<mc_gState->m_Permissions->m_MinerCount)
                {
                    pindexNew->nCanMine=0;
                } 
            }
            else
            {
                if(pindexNew->pprev)
                {
                    pindexNew->nHeightMinedByMe=pindexNew->pprev->nHeightMinedByMe;
                    pindexNew->nCanMine=pindexNew->pprev->nCanMine;
                    if(pindexNew->nHeightMinedByMe+mc_gState->m_Permissions->m_MinerCount-mc_gState->m_Permissions->GetActiveMinerCount() > pindexNew->nHeight)
                    {
                        pindexNew->nCanMine=0;
                    }
                }
                else
                {
                    pindexNew->nCanMine=0;
                }
            }
        }
        if(pindexNew->pprev)
        {
            if(fDebug)LogPrint("mcblock","mchn-block: New block index:   %s, prev: %s, height: %d, mined-by-me: %d, can-mine: %d\n",pindexNew->GetBlockHash().ToString().c_str(),
                    pindexNew->pprev->GetBlockHash().ToString().c_str(),
                    pindexNew->nHeight,pindexNew->nHeightMinedByMe,pindexNew->nCanMine);
        }
    }    
}
/* MCHN END */

/**
 * Try to make some progress towards making pindexMostWork the active block.
 * pblock is either NULL or a pointer to a CBlock corresponding to pindexMostWork.
 */
static bool ActivateBestChainStep(CValidationState &state, CBlockIndex *pindexMostWork, CBlock *pblock, bool* fInvalidFoundOut) {
    AssertLockHeld(cs_main);
    bool fInvalidFound = false;
    const CBlockIndex *pindexOldTip = chainActive.Tip();
    const CBlockIndex *pindexFork = chainActive.FindFork(pindexMostWork);
    
    if(fDebug)LogPrint("mcblockperf","mchn-block-perf: Best chain activation\n");
    // Disconnect active blocks which are no longer in the best chain.
    if(pindexFork)
    {
        if(fDebug)LogPrint("mcwrp","mcwrp: Activating best chain %d -> %d -> %d\n",chainActive.Height(),pindexFork->nHeight,pindexMostWork->nHeight);
        if(pindexFork->nHeight < chainActive.Height())
        {
            if(fDebug)LogPrint("mcwrp","mcwrp: Reorg on block (%d -> %d)\n",pindexFork->nHeight+1,chainActive.Height());        
        }
    }
    while (chainActive.Tip() && chainActive.Tip() != pindexFork) {
        if(fDebug)LogPrint("mcwrp","mcwrp: Locking wallet for disconnection of block %d\n",chainActive.Height());
        pwalletTxsMain->WRPWriteLock();
        if (!DisconnectTip(state))
        {
            if(fDebug)LogPrint("mcwrp","mcwrp: Wallet unlocked after failed disconnection\n");
            pwalletTxsMain->WRPWriteUnLock();
            return false;
        }
    }

    // Build list of new blocks to connect.
    std::vector<CBlockIndex*> vpindexToConnect;
    bool fContinue = true;
    int nHeight = pindexFork ? pindexFork->nHeight : -1;
    while (fContinue && nHeight != pindexMostWork->nHeight) {
    // Don't iterate the entire list of potential improvements toward the best tip, as we likely only need
    // a few blocks along the way.
    int nTargetHeight = std::min(nHeight + 32, pindexMostWork->nHeight);
    vpindexToConnect.clear();
    vpindexToConnect.reserve(nTargetHeight - nHeight);
    CBlockIndex *pindexIter = pindexMostWork->GetAncestor(nTargetHeight);
    while (pindexIter && pindexIter->nHeight != nHeight) {
        vpindexToConnect.push_back(pindexIter);
        pindexIter = pindexIter->pprev;
    }
    nHeight = nTargetHeight;
    // Connect new blocks.
    BOOST_REVERSE_FOREACH(CBlockIndex *pindexConnect, vpindexToConnect) {
        if (!ConnectTip(state, pindexConnect, pindexConnect == pindexMostWork ? pblock : NULL)) {
            if (state.IsInvalid()) {
                // The block violates a consensus rule.
                if (!state.CorruptionPossible())
                    InvalidChainFound(vpindexToConnect.back());
                state = CValidationState();
                fInvalidFound = true;
                fContinue = false;
                break;
            } else {
                // A system error occurred (disk space, database error, ...).
                if(fDebug)LogPrint("mcwrp","mcwrp: Wallet unlocked after failed connection\n");
                pwalletTxsMain->WRPWriteUnLock();
                return false;
            }
        } else {
            PruneBlockIndexCandidates();
/*            
            if(!GetBoolArg("-waitforbetterblock",false))
            {
*/            
            if (!pindexOldTip || chainActive.Tip()->nChainWork > pindexOldTip->nChainWork) {
                // We're in a better position than we were. Return temporarily to release the lock.
                fContinue = false;
                break;
            }
            
//            }
//            if(mc_gState->m_ProtocolVersionToUpgrade > mc_gState->m_NetworkParams->ProtocolVersion())
            if( (mc_gState->m_ProtocolVersionToUpgrade > 0) && (mc_gState->IsSupported(mc_gState->m_ProtocolVersionToUpgrade) == 0) )
            {
                LogPrintf("Cannot connect more blocks, required protocol version upgrade %d -> %d\n",mc_gState->m_NetworkParams->ProtocolVersion(),mc_gState->m_ProtocolVersionToUpgrade);
                fContinue = false;
                break;                
            }
        }
        
    }
    }
    
/* MCHN START */    
//    if (!fInvalidFound)
    {
        mc_gState->m_Permissions->ClearMemPool();
        mc_gState->m_Assets->ClearMemPool();
        if(pMultiChainFilterEngine)
        {
            pMultiChainFilterEngine->Reset(chainActive.Height(),0);
        }
        
        if(fDebug)LogPrint("mcblockperf","mchn-block-perf: Replaying mempool started\n");
        ReplayMemPool(mempool,0,true);
        if(fDebug)LogPrint("mcblockperf","mchn-block-perf: Defragmenting mempool hash list\n");
        mempool.defragmentHashList();
        
        CheckOrphanPeers();
        
        if(fDebug)LogPrint("mcblockperf","mchn-block-perf: Reaccepting wallet transactions\n");
        if(pwalletMain)
        {
            if( (mc_gState->m_NodePausedState & MC_NPS_REACCEPT) == 0 )
            {
                pwalletMain->ReacceptWalletTransactions();                      // Some wallet transactions may become invalid in reorg            
                                                                                // Some may become invalid if not confirmed in time
            }
        }
        if(fDebug)LogPrint("mcblockperf","mchn-block-perf: Best chain activation completed\n");

/* MCHN START */        
        if (!fImporting && !fReindex)
        {
            if(pwalletMain)
            {
                CTransaction emptyTx;                                                   // Triggering wallet optimization
                SyncWithWallets(emptyTx, pblock);
            }
        }
        
        pwalletTxsMain->WRPSync(1);
        
        if(fDebug)LogPrint("mcwrp","mcwrp: Wallet unlocked after block %d\n",chainActive.Height());
        pwalletTxsMain->WRPWriteUnLock();
        
/* MCHN END */    
        
    }
/* MCHN END */    

    // Callbacks/notifications for a new best chain.
    if (fInvalidFound)
        CheckForkWarningConditionsOnNewFork(vpindexToConnect.back());
    else
        CheckForkWarningConditions();

/* MCHN START */
    
    if (fInvalidFound)
    {
        if(chainActive.Height() < 0)                                            // Error on genesis block
        {
            return false;
        }
    }
    
    if(fInvalidFoundOut)
    {
        *fInvalidFoundOut=fInvalidFound;
    }
/* MCHN END */
    
    return true;
}

/**
 * Make the best chain active, in multiple steps. The result is either failure
 * or an activated best chain. pblock is either NULL or a pointer to a block
 * that is already loaded (to avoid loading it again from disk).
 */
bool ActivateBestChain(CValidationState &state, CBlock *pblock) {
    CBlockIndex *pindexNewTip = NULL;
    CBlockIndex *pindexMostWork = NULL;
    int attempt=0;
    do {
        boost::this_thread::interruption_point();

        uint32_t nCanMine;
        
        bool fInitialDownload;
        {
            LOCK(cs_main);
            
/* MCHN START */            
            if(pwalletMain)
            {
                CPubKey pubkey;            
                if(chainActive.Tip())
                {
                    chainActive.Tip()->nCanMine=pwalletMain->GetKeyFromAddressBook(pubkey,MC_PTP_MINE) ? MC_PTP_MINE : 0;
                    if(chainActive.Tip()->nHeightMinedByMe == 0)                    // Can happen if several blocks received in the wrong order 
                    {
                        if(chainActive.Tip()->pprev)                                
                        {
                            chainActive.Tip()->nHeightMinedByMe=chainActive.Tip()->pprev->nHeightMinedByMe;
                        }
                    }
                }
            }
/* MCHN END */            
            
            pindexMostWork = FindMostWorkChain();
            // Whether we have anything to do at all.
            if (pindexMostWork == NULL || pindexMostWork == chainActive.Tip())
                return true;
            
            
            nCanMine=pindexMostWork->nCanMine;
/* MCHN START */            
            
//            if( (mc_gState->m_ProtocolVersionToUpgrade > mc_gState->m_NetworkParams->ProtocolVersion()) && 
            if( (mc_gState->m_ProtocolVersionToUpgrade > 0) && (mc_gState->IsSupported(mc_gState->m_ProtocolVersionToUpgrade) == 0) &&
                chainActive.FindFork(pindexMostWork) == chainActive.Tip() )
            {
                LogPrintf("Cannot connect blocks, required protocol version upgrade %d -> %d\n",mc_gState->m_NetworkParams->ProtocolVersion(),mc_gState->m_ProtocolVersionToUpgrade);
                return true;                
            }            
            
            if(pindexMostWork->pprev != chainActive.Tip())
            {
                if(chainActive.Tip())
                {
                    if(fDebug)LogPrint("mcblock","mchn-block: Possible reorg: %d %d->%d\n",attempt,chainActive.Tip()->nHeight,pindexMostWork->nHeight);
                    if(chainActive.Tip()->nHeight == pindexMostWork->nHeight)
                    {
                        if(fDebug)LogPrint("mcblock","mchn-block: Same-height reorg: %d %d(%d)->%d(%d)\n",attempt,chainActive.Tip()->nCanMine,chainActive.Tip()->nHeight-chainActive.Tip()->nHeightMinedByMe,
                                pindexMostWork->nCanMine,pindexMostWork->nHeight-pindexMostWork->nHeightMinedByMe);                        
                    }
                }
                attempt++;
            }
/* MCHN END */            

            if (!ActivateBestChainStep(state, pindexMostWork, pblock && pblock->GetHash() == pindexMostWork->GetBlockHash() ? pblock : NULL, NULL))
                return false;
/* MCHN START */            
            if(pindexMostWork == chainActive.Tip())
            {
                if(pwalletMain)
                {
                    CPubKey pubkey;            
                    chainActive.Tip()->nCanMine=pwalletMain->GetKeyFromAddressBook(pubkey,MC_PTP_MINE) ? MC_PTP_MINE : 0;
                    
                    if(fDebug)LogPrint("mcblock","mchn-block: Chain activated:   %s (height %d), can-mine: %d\n",
                            chainActive.Tip()->GetBlockHash().ToString().c_str(), chainActive.Tip()->nHeight,chainActive.Tip()->nCanMine);
                    
                    if(nCanMine != chainActive.Tip()->nCanMine)
                    {
//                        if(!pwalletMain->GetKeyFromAddressBook(pubkey,MC_PTP_MINE))
                        {
                            if(fDebug)LogPrint("mcblock","mchn-block: Wallet mine permission changed on block: %s (height %d), reactivating best chain\n",
                                    chainActive.Tip()->GetBlockHash().ToString().c_str(), chainActive.Tip()->nHeight);
                            pindexMostWork=NULL;
//                            continue;
                        }
                    }
                }
            }
/* MCHN END */            
            
            pindexNewTip = chainActive.Tip();
            fInitialDownload = IsInitialBlockDownload();
        }
        // When we reach this point, we switched to a new tip (stored in pindexNewTip).

        // Notifications/callbacks that can run without cs_main
        if (!fInitialDownload) {
            uint256 hashNewTip = pindexNewTip->GetBlockHash();
            // Relay inventory, but don't relay old inventory during initial block download.
            int nBlockEstimate = Checkpoints::GetTotalBlocksEstimate();
            {
                LOCK(cs_vNodes);
                BOOST_FOREACH(CNode* pnode, vNodes)
                    if (chainActive.Height() > (pnode->nStartingHeight != -1 ? pnode->nStartingHeight - 2000 : nBlockEstimate))
                        pnode->PushInventory(CInv(MSG_BLOCK, hashNewTip));
                }
            // Notify external listeners about the new tip.
            uiInterface.NotifyBlockTip(hashNewTip);
        }
    } while(pindexMostWork != chainActive.Tip());

    // Write changes periodically to disk, after relay.
    if (!FlushStateToDisk(state, FLUSH_STATE_PERIODIC)) {
        return false;
    }

    return true;
}

/* MCHN START */

void ClearMemPools()
{
    {
        LOCK(cs_main);
        mempool.clear();
        mc_gState->m_Permissions->ClearMemPool();
        mc_gState->m_Assets->ClearMemPool();
        LogPrintf("mempool cleared\n");
    }
}
string SetLastBlock(uint256 hash)
{
    return SetLastBlock(hash,NULL);
}

string SetLastBlock(uint256 hash,bool *fNotFound)
{
    if(fNotFound)
    {
        *fNotFound=false;
    }
    
    {
        LOCK(cs_main);
        
        CValidationState state;       
        if(hash == 0)
        {
            ActivateBestChain(state);
            return "";            
        }
        
        if (mapBlockIndex.count(hash) == 0)
        {
            if(fNotFound)
            {
                *fNotFound=true;
            }
            return "Block not found";
        }
        
        CBlock block;
        CBlockIndex* pblockindex = mapBlockIndex[hash];
        const CBlockIndex *pindexFork = chainActive.FindFork(pblockindex);

        CBlockIndex *pindex;
        pindex=pblockindex;
        while(pindex != pindexFork)
        {
            if (pblockindex->nStatus & BLOCK_FAILED_MASK)
            {
                return "Block is invalid";
            }        
            if ( (pblockindex->nStatus & BLOCK_HAVE_DATA) == 0 )
    //        if (!pblockindex->IsValid(BLOCK_VALID_SCRIPTS))
            {
                if(fNotFound)
                {
                    *fNotFound=true;
                }
                return "Block is invalid, probably we have only header";            
            }

            if(pindex == pblockindex)
            {
                if(!ReadBlockFromDisk(block, pblockindex))
                {
                    if(fNotFound)
                    {
                        *fNotFound=true;
                    }
                    return "Block not found";
                }
            }
            
            pindex=pindex->pprev;
        }
        
        while(pblockindex != chainActive.Tip())
        {
            bool fInvalidFound;
            if(!ActivateBestChainStep(state,pblockindex,NULL,&fInvalidFound) || fInvalidFound)
            {
                string error=state.GetRejectReason();
                ActivateBestChain(state);
                return error;
            }        
        }
        
        setBlockIndexCandidates.insert(pblockindex);

        LogPrintf("Set active chain tip: %s\n",hash.GetHex().c_str());
        if(pblockindex->nHeightMinedByMe == pblockindex->nHeight)
        {
            if(fDebug)LogPrint("mchn","mchn: New block %s is mined by me, relay it anyway\n",hash.GetHex().c_str());
            {
                LOCK(cs_vNodes);
                BOOST_FOREACH(CNode* pnode, vNodes)
                {
                    pnode->PushInventory(CInv(MSG_BLOCK, hash));
                }
            }
        }
        
    }    
    return "";
}

string SetBannedTxs(string txlist)
{
    set<string> setStrings;
    vector <uint256> vTxs;
    stringstream ss(txlist); 
    string tok;
    
    vTxs.clear();
    
    while(getline(ss, tok, ',')) 
    {
        if(tok.size())
        {
            if (setStrings.count(tok))
            {
                return string("Invalid parameter, duplicate banned transaction: ")+tok;
            }
            if (!IsHex(tok))
            {
                return string("Invalid parameter, -bantx element must be hexadecimal string (not '")+tok+"')";                
            }
            if (tok.size() != 64)
            {
                return string("Invalid parameter, -bantx element must be 32-byte hexadecimal string (not '")+tok+"')";                
            }
            
            uint256 result;
            result.SetHex(tok);

            setStrings.insert(tok);
            vTxs.push_back(result);
        }
    }
    
    LogPrintf("Setting banned transaction list: %4d transactions\n",(int)vTxs.size());
    BOOST_FOREACH(uint256 hash, setBannedTxBlocks)
    {
        BlockMap::iterator mi = mapBlockIndex.find(hash);
        if (mi != mapBlockIndex.end()) 
        {
            if(setBlockIndexCandidates.find(mi->second) == setBlockIndexCandidates.end())
            {
                setBlockIndexCandidates.insert(mi->second);
            }
        }
    }
    
    setBannedTxBlocks.clear();
    setBannedTxs.clear();
    for(unsigned int i=0;i<vTxs.size();i++)
    {
        setBannedTxs.insert(vTxs[i]);
        LogPrintf("Banned transaction set: %4d %s\n",i,vTxs[i].ToString().c_str());
    }
    return "";
}

bool IsTxBanned(uint256 txid)
{
    if(setBannedTxs.size())
    {
        if(setBannedTxs.find(txid) != setBannedTxs.end())
        {
            return true;
        }
    }
    return false;
}

string SetLockedBlock(string hash)
{
    uint256 hashOld;
    CBlockIndex* pindexLockedBlockOld;
    
    hashOld=hLockedBlock;
    pindexLockedBlockOld=pindexLockedBlock;
    
    if(hash.size())
    {
        if (!IsHex(hash))
        {
            return string("Invalid parameter, -lockblock must be hexadecimal string (not '")+hash+"')";                
        }
        if (hash.size() != 64)
        {
            return string("Invalid parameter, -lockblock must be 32-byte hexadecimal string (not '")+hash+"')";                
        }

        hLockedBlock.SetHex(hash);            
    }
    else
    {
        if(hLockedBlock != 0)
        {
            pindexLockedBlock=NULL;
            LogPrintf("Removing locked block, activating best chain...\n");                
            hLockedBlock=0;
            SetLastBlock(0);                
        }
    }

    pindexLockedBlock=NULL;
    
    if(hLockedBlock != 0)
    {
        LogPrintf("Setting locked block %s\n",hLockedBlock.ToString().c_str());
        BlockMap::iterator mi = mapBlockIndex.find(hLockedBlock);
        if (mi != mapBlockIndex.end()) 
        {
            pindexLockedBlock = mi->second;
        }
        else
        {
            LogPrintf("Block %s not found, chain will be switched if it will appear on alternative chain\n",hLockedBlock.ToString().c_str());     
            LOCK(cs_vNodes);
            BOOST_FOREACH(CNode* pnode, vNodes)
            {
                pnode->PushMessage("getheaders", chainActive.GetLocator(chainActive.Tip()), uint256(0));                
            }
        }
        if(pindexLockedBlock)
        {
            if(!chainActive.Contains(pindexLockedBlock))
            {
                const CBlockIndex *pindexFork;
                pindexFork=chainActive.FindFork(pindexLockedBlock);
                
                CBlockIndex *pindexWalk;
                pindexWalk=pindexLockedBlock;
                while( (pindexWalk != pindexFork) && ( (pindexWalk->nStatus & BLOCK_HAVE_DATA) == 0 ) )
                {
                    pindexWalk=pindexWalk->pprev;
                }
                
                if(pindexWalk == pindexLockedBlock)
                {
                    BlockMap::iterator it = mapBlockIndex.begin();
                    while (it != mapBlockIndex.end()) 
                    {
                        if (it->second->IsValid(BLOCK_VALID_TRANSACTIONS) && it->second->nChainTx && setBlockIndexCandidates.value_comp()(pindexWalk, it->second)) 
                        {
                            CBlockIndex *pindexCandidate=it->second;
                            if(pindexCandidate->GetAncestor(pindexWalk->nHeight) == pindexWalk)
                            {
                                pindexWalk=pindexCandidate;
                            }
                        }
                        it++;
                    }
                }
                
                LogPrintf("Block %s found on alternative chain at height %d\n",
                        hLockedBlock.ToString().c_str(),pindexLockedBlock->nHeight);                
                LogPrintf("Fork: %s at height %d\n",
                        pindexFork->GetBlockHash().ToString().c_str(),pindexFork->nHeight);                
                LogPrintf("Switching to best known block %s at height %d\n",
                        pindexWalk->GetBlockHash().ToString().c_str(),pindexWalk->nHeight);                
                
                string error=SetLastBlock(pindexWalk->GetBlockHash());                
                if(error.size())
                {
                    LogPrintf("ERROR: Cannot switch to chain with block %s: %d\n",hLockedBlock.ToString().c_str(),error.c_str());                                    
                    hLockedBlock=hashOld;
                    pindexLockedBlock=pindexLockedBlockOld;
                    return string("Cannot switch to locked block: ")+error;                
                }
            }
            else
            {
                LogPrintf("Block %s already in active chain at height %d\n",hLockedBlock.ToString().c_str(),pindexLockedBlock->nHeight);                
            }
        }
    }
    
    return "";
}

bool CanMineWithLockedBlock()
{
    if(pindexLockedBlock)
    {
        if(!chainActive.Contains(pindexLockedBlock))
        {
            return false;
        }        
    }
    
    return true;
}

void InvalidateBlockIfFoundInBlockIndex(const CBlock& block)
{
    uint256 hash = block.GetHash();
    BlockMap::iterator miSelf = mapBlockIndex.find(hash);
    CBlockIndex *pindex = NULL;
    if (miSelf != mapBlockIndex.end()) 
    {
        CValidationState state;
        
        pindex = miSelf->second;
        InvalidateBlock(state,pindex);
    }    
}

/* MCHN END */



bool InvalidateBlock(CValidationState& state, CBlockIndex *pindex) {
    AssertLockHeld(cs_main);

    // Mark the block itself as invalid.
    pindex->nStatus |= BLOCK_FAILED_VALID;
    setDirtyBlockIndex.insert(pindex);
    setBlockIndexCandidates.erase(pindex);

    while (chainActive.Contains(pindex)) {
        CBlockIndex *pindexWalk = chainActive.Tip();
        pindexWalk->nStatus |= BLOCK_FAILED_CHILD;
        setDirtyBlockIndex.insert(pindexWalk);
        setBlockIndexCandidates.erase(pindexWalk);
        // ActivateBestChain considers blocks already in chainActive
        // unconditionally valid already, so force disconnect away from it.
        if (!DisconnectTip(state)) {
            return false;
        }
    }

    // The resulting new best tip may not be in setBlockIndexCandidates anymore, so
    // add them again.
    BlockMap::iterator it = mapBlockIndex.begin();
    while (it != mapBlockIndex.end()) {
        if (it->second->IsValid(BLOCK_VALID_TRANSACTIONS) && it->second->nChainTx && setBlockIndexCandidates.value_comp()(chainActive.Tip(), it->second)) {
            setBlockIndexCandidates.insert(pindex);
        }
        it++;
    }

    InvalidChainFound(pindex);
    return true;
}

bool ReconsiderBlock(CValidationState& state, CBlockIndex *pindex) {
    AssertLockHeld(cs_main);

    int nHeight = pindex->nHeight;

    // Remove the invalidity flag from this block and all its descendants.
    BlockMap::iterator it = mapBlockIndex.begin();
    while (it != mapBlockIndex.end()) {
        if (!it->second->IsValid() && it->second->GetAncestor(nHeight) == pindex) {
            it->second->nStatus &= ~BLOCK_FAILED_MASK;
            setDirtyBlockIndex.insert(it->second);
            if (it->second->IsValid(BLOCK_VALID_TRANSACTIONS) && it->second->nChainTx && setBlockIndexCandidates.value_comp()(chainActive.Tip(), it->second)) {
                setBlockIndexCandidates.insert(it->second);
            }
            if (it->second == pindexBestInvalid) {
                // Reset invalid block marker if it was pointing to one of those.
                pindexBestInvalid = NULL;
            }
        }
        it++;
    }

    // Remove the invalidity flag from all ancestors too.
    while (pindex != NULL) {
        if (pindex->nStatus & BLOCK_FAILED_MASK) {
            pindex->nStatus &= ~BLOCK_FAILED_MASK;
            setDirtyBlockIndex.insert(pindex);
        }
        pindex = pindex->pprev;
    }
    return true;
}

CBlockIndex* AddToBlockIndex(const CBlockHeader& block)
{
    // Check for duplicate
    uint256 hash = block.GetHash();
    BlockMap::iterator it = mapBlockIndex.find(hash);
    if (it != mapBlockIndex.end())
        return it->second;

    // Construct new block index object
    CBlockIndex* pindexNew = new CBlockIndex(block);
    assert(pindexNew);
    // We assign the sequence id to blocks only when the full data is available,
    // to avoid miners withholding blocks but broadcasting headers, to get a
    // competitive advantage.
    pindexNew->nSequenceId = 0;
    if(fDebug)LogPrint("mcblock","mchn-block: Adding block %s\n",hash.ToString().c_str());
    mc_gState->ChainLock();
    BlockMap::iterator mi = mapBlockIndex.insert(make_pair(hash, pindexNew)).first;
    mc_gState->ChainUnLock();
    if(fDebug)LogPrint("mcblock","mchn-block: Added block %s\n",hash.ToString().c_str());
    pindexNew->phashBlock = &((*mi).first);
    BlockMap::iterator miPrev = mapBlockIndex.find(block.hashPrevBlock);
    if (miPrev != mapBlockIndex.end())
    {
        pindexNew->pprev = (*miPrev).second;
        pindexNew->nHeight = pindexNew->pprev->nHeight + 1;
        pindexNew->BuildSkip();
    }
    pindexNew->nChainWork = (pindexNew->pprev ? pindexNew->pprev->nChainWork : 0) + GetBlockProof(*pindexNew);
    pindexNew->RaiseValidity(BLOCK_VALID_TREE);
    if (pindexBestHeader == NULL || pindexBestHeader->nChainWork < pindexNew->nChainWork)
        pindexBestHeader = pindexNew;

    MultichainNode_UpdateBlockByHeightList(pindexNew);
    setDirtyBlockIndex.insert(pindexNew);

    return pindexNew;
}

/** Mark a block as having its data received and checked (up to BLOCK_VALID_TRANSACTIONS). */
bool ReceivedBlockTransactions(const CBlock &block, CValidationState& state, CBlockIndex *pindexNew, const CDiskBlockPos& pos)
{
    mc_gState->ChainLock();
    pindexNew->nTx = block.vtx.size();
    pindexNew->nChainTx = 0;
    pindexNew->nFile = pos.nFile;
    pindexNew->nDataPos = pos.nPos;
    pindexNew->nUndoPos = 0;
    pindexNew->nStatus |= BLOCK_HAVE_DATA;
    pindexNew->RaiseValidity(BLOCK_VALID_TRANSACTIONS);
    mc_gState->ChainUnLock();
    
/*    
    {
         LOCK(cs_nBlockSequenceId);
         pindexNew->nSequenceId = nBlockSequenceId++;
    }
 */ 
    
/* MCHN START */    
    UpdateChainMiningStatus(block,pindexNew);

/*    
    if(mc_gState->m_NetworkParams->IsProtocolMultichain())
    {
        std::vector<unsigned char> vchPubKey=std::vector<unsigned char> (block.vSigner+1, block.vSigner+1+block.vSigner[0]);
        CPubKey pubKeyOut(vchPubKey);
        CKeyID keyID=pubKeyOut.GetID();
        CKey key;
        pindexNew->nCanMine=mc_gState->m_Permissions->CanMine(NULL,keyID.begin());
        if(pwalletMain)
        {
            if(pwalletMain->GetKey(keyID,key))
            {
                pindexNew->nHeightMinedByMe=pindexNew->nHeight;
                if(pindexNew->pprev)
                {
                    pindexNew->nCanMine=pindexNew->pprev->nCanMine;
                }                
                else
                {
                    pindexNew->nCanMine=MC_PTP_MINE;                    
                }
                if(mc_gState->m_Permissions->GetActiveMinerCount()<mc_gState->m_Permissions->m_MinerCount)
                {
                    pindexNew->nCanMine=0;
                } 
            }
            else
            {
                if(pindexNew->pprev)
                {
                    pindexNew->nHeightMinedByMe=pindexNew->pprev->nHeightMinedByMe;
                    pindexNew->nCanMine=pindexNew->pprev->nCanMine;
                    if(pindexNew->nHeightMinedByMe+mc_gState->m_Permissions->m_MinerCount-mc_gState->m_Permissions->GetActiveMinerCount() > pindexNew->nHeight)
                    {
                        pindexNew->nCanMine=0;
                    }
                }
                else
                {
                    pindexNew->nCanMine=0;
                }
            }
        }
        if(pindexNew->pprev)
        {
            if(fDebug)LogPrint("mchn","mchn: New block index: %s, prev: %s, height: %d, mined-by-me: %d, can-mine: %d\n",block.GetHash().ToString().c_str(),
                    pindexNew->pprev->GetBlockHash().ToString().c_str(),
                    pindexNew->nHeight,pindexNew->nHeightMinedByMe,pindexNew->nCanMine);
        }
    }    
 */ 
/* MCHN END */    
    
    setDirtyBlockIndex.insert(pindexNew);

    if (pindexNew->pprev == NULL || pindexNew->pprev->nChainTx) {
        // If pindexNew is the genesis block or all parents are BLOCK_VALID_TRANSACTIONS.
        deque<CBlockIndex*> queue;
        queue.push_back(pindexNew);

        // Recursively process any descendant blocks that now may be eligible to be connected.
        while (!queue.empty()) {
            CBlockIndex *pindex = queue.front();
            queue.pop_front();
            pindex->nChainTx = (pindex->pprev ? pindex->pprev->nChainTx : 0) + pindex->nTx;
            {
                LOCK(cs_nBlockSequenceId);
                pindex->nSequenceId = nBlockSequenceId++;
            }
               
            setBlockIndexCandidates.insert(pindex);
            std::pair<std::multimap<CBlockIndex*, CBlockIndex*>::iterator, std::multimap<CBlockIndex*, CBlockIndex*>::iterator> range = mapBlocksUnlinked.equal_range(pindex);
            while (range.first != range.second) {
                std::multimap<CBlockIndex*, CBlockIndex*>::iterator it = range.first;
                queue.push_back(it->second);
                range.first++;
                mapBlocksUnlinked.erase(it);
            }
        }
    } else {
        if (pindexNew->pprev && pindexNew->pprev->IsValid(BLOCK_VALID_TREE)) {
            mapBlocksUnlinked.insert(std::make_pair(pindexNew->pprev, pindexNew));
        }
    }

    return true;
}

bool FindBlockPos(CValidationState &state, CDiskBlockPos &pos, unsigned int nAddSize, unsigned int nHeight, uint64_t nTime, bool fKnown = false)
{
    LOCK(cs_LastBlockFile);

    unsigned int nFile = fKnown ? pos.nFile : nLastBlockFile;
    if (vinfoBlockFile.size() <= nFile) {
        vinfoBlockFile.resize(nFile + 1);
    }

    if (!fKnown) {
        while (vinfoBlockFile[nFile].nSize + nAddSize >= MAX_BLOCKFILE_SIZE) {
            LogPrintf("Leaving block file %i: %s\n", nFile, vinfoBlockFile[nFile].ToString());
            FlushBlockFile(true);
            nFile++;
            if (vinfoBlockFile.size() <= nFile) {
                vinfoBlockFile.resize(nFile + 1);
            }
        }
        pos.nFile = nFile;
        pos.nPos = vinfoBlockFile[nFile].nSize;
    }

    nLastBlockFile = nFile;
//    vinfoBlockFile[nFile].nSize += nAddSize;
    vinfoBlockFile[nFile].AddBlock(nHeight, nTime);
    
    if (fKnown)                                                                 // BCC 002c8a2
        vinfoBlockFile[nFile].nSize = std::max(pos.nPos + nAddSize, vinfoBlockFile[nFile].nSize);
    else
        vinfoBlockFile[nFile].nSize += nAddSize;
    
    if (!fKnown) {
        unsigned int nOldChunks = (pos.nPos + BLOCKFILE_CHUNK_SIZE - 1) / BLOCKFILE_CHUNK_SIZE;
        unsigned int nNewChunks = (vinfoBlockFile[nFile].nSize + BLOCKFILE_CHUNK_SIZE - 1) / BLOCKFILE_CHUNK_SIZE;
        if (nNewChunks > nOldChunks) {
            if (CheckDiskSpace(nNewChunks * BLOCKFILE_CHUNK_SIZE - pos.nPos)) {
                FILE *file = OpenBlockFile(pos);
                if (file) {
                    LogPrintf("Pre-allocating up to position 0x%x in blk%05u.dat\n", nNewChunks * BLOCKFILE_CHUNK_SIZE, pos.nFile);
                    AllocateFileRange(file, pos.nPos, nNewChunks * BLOCKFILE_CHUNK_SIZE - pos.nPos);
                    fclose(file);
                }
            }
            else
                return state.Error("out of disk space");
        }
    }

    setDirtyFileInfo.insert(nFile);
    return true;
}

bool FindUndoPos(CValidationState &state, int nFile, CDiskBlockPos &pos, unsigned int nAddSize)
{
    pos.nFile = nFile;

    LOCK(cs_LastBlockFile);

    unsigned int nNewSize;
    pos.nPos = vinfoBlockFile[nFile].nUndoSize;
    nNewSize = vinfoBlockFile[nFile].nUndoSize += nAddSize;
    setDirtyFileInfo.insert(nFile);

    unsigned int nOldChunks = (pos.nPos + UNDOFILE_CHUNK_SIZE - 1) / UNDOFILE_CHUNK_SIZE;
    unsigned int nNewChunks = (nNewSize + UNDOFILE_CHUNK_SIZE - 1) / UNDOFILE_CHUNK_SIZE;
    if (nNewChunks > nOldChunks) {
        if (CheckDiskSpace(nNewChunks * UNDOFILE_CHUNK_SIZE - pos.nPos)) {
            FILE *file = OpenUndoFile(pos);
            if (file) {
                LogPrintf("Pre-allocating up to position 0x%x in rev%05u.dat\n", nNewChunks * UNDOFILE_CHUNK_SIZE, pos.nFile);
                AllocateFileRange(file, pos.nPos, nNewChunks * UNDOFILE_CHUNK_SIZE - pos.nPos);
                fclose(file);
            }
        }
        else
            return state.Error("out of disk space");
    }

    return true;
}

bool CheckBlockHeader(const CBlockHeader& block, CValidationState& state, bool fCheckPOW)
{
    // Check proof of work matches claimed amount
    if (fCheckPOW && !CheckProofOfWork(block.GetHash(), block.nBits))
        return state.DoS(50, error("CheckBlockHeader() : proof of work failed"),
                         REJECT_INVALID, "high-hash");

    // Check timestamp
/* MCHN START */    
//    if (block.GetBlockTime() > GetAdjustedTime() + 2 * 60 * 60)
    if (block.GetBlockTime() > GetAdjustedTime() + 2 * 6 * Params().TargetSpacing())
/* MCHN END */    
    {
        LogPrintf("ERROR: Block time: %u. Node time: %u, Offset: %u\n",block.GetBlockTime(), GetAdjustedTime(), GetTimeOffset());
        return state.Invalid(error("CheckBlockHeader() : block timestamp too far in the future"),
                             REJECT_INVALID, "time-too-new");
    }
    return true;
}

/* MCHN START */

bool CheckBlockForUpgardableConstraints(const CBlock& block, CValidationState& state, string parameter, bool in_sync)
{
    if(!in_sync)
    {
        return true;
    }
    
    if(parameter == "maximum-block-size")
    {
        if (block.vtx.empty() || block.vtx.size() > MAX_BLOCK_SIZE || ::GetSerializeSize(block, SER_NETWORK, PROTOCOL_VERSION) > MAX_BLOCK_SIZE)
            return state.DoS(100, error("CheckBlock() : size limits failed"),
                             REJECT_INVALID, "bad-blk-length");        
    }
    if(parameter == "maximum-block-sigops")
    {
        unsigned int nSigOps = 0;
        BOOST_FOREACH(const CTransaction& tx, block.vtx)
        {
            nSigOps += GetLegacySigOpCount(tx);
        }
        if (nSigOps > MAX_BLOCK_SIGOPS)
            return state.DoS(100, error("CheckBlock() : out-of-bounds SigOpCount"),
                             REJECT_INVALID, "bad-blk-sigops", true);        
    }
    return true;
}

//bool CheckBlock(CBlock& block, CValidationState& state, bool fCheckPOW, bool fCheckMerkleRoot)
bool CheckBlock(const CBlock& block, CValidationState& state, bool fCheckPOW, bool fCheckMerkleRoot)
/* MCHN END */
{
    // These are checks that are independent of context.

    // Check that the header is valid (particularly PoW).  This is mostly
    // redundant with the call in AcceptBlockHeader.
    if (!CheckBlockHeader(block, state, fCheckPOW))
        return false;

    // Check the merkle root.
    if (fCheckMerkleRoot) {
        bool mutated;        
        uint256 hashMerkleRoot2 = block.BuildMerkleTree(&mutated);
        if (block.hashMerkleRoot != hashMerkleRoot2)
            return state.DoS(100, error("CheckBlock() : hashMerkleRoot mismatch"),
                             REJECT_INVALID, "bad-txnmrklroot", true);

        // Check for merkle tree malleability (CVE-2012-2459): repeating sequences
        // of transactions in a block without affecting the merkle root of a block,
        // while still invalidating it.
        if (mutated)
            return state.DoS(100, error("CheckBlock() : duplicate transaction"),
                             REJECT_INVALID, "bad-txns-duplicate", true);
    }

    // All potential-corruption validation must be done before we do any
    // transaction validation, as otherwise we may mark the header as invalid
    // because we receive the wrong transactions for it.

    // Size limits
    if(!CheckBlockForUpgardableConstraints(block,state,"maximum-block-size",false))
    {
        return false;
    }
/*    
    if (block.vtx.empty() || block.vtx.size() > MAX_BLOCK_SIZE || ::GetSerializeSize(block, SER_NETWORK, PROTOCOL_VERSION) > MAX_BLOCK_SIZE)
        return state.DoS(100, error("CheckBlock() : size limits failed"),
                         REJECT_INVALID, "bad-blk-length");
*/
    
    // First transaction must be coinbase, the rest must not be
    if (block.vtx.empty() || !block.vtx[0].IsCoinBase())
        return state.DoS(100, error("CheckBlock() : first tx is not coinbase"),
                         REJECT_INVALID, "bad-cb-missing");
    for (unsigned int i = 1; i < block.vtx.size(); i++)
        if (block.vtx[i].IsCoinBase())
            return state.DoS(100, error("CheckBlock() : more than one coinbase"),
                             REJECT_INVALID, "bad-cb-multiple");

    // Check transactions
    BOOST_FOREACH(const CTransaction& tx, block.vtx)
        if (!CheckTransaction(tx, state))
            return error("CheckBlock() : CheckTransaction failed");

    if(setBannedTxs.size())
    {
        BOOST_FOREACH(const CTransaction& tx, block.vtx)
        {
            if(IsTxBanned(tx.GetHash()))
            {
                if(setBannedTxBlocks.find(block.GetHash()) == setBannedTxBlocks.end())
                {
                    setBannedTxBlocks.insert(block.GetHash());
                }
                return error("CheckBlock() : banned transaction: %s",tx.GetHash().ToString());
            }
        }
    }
    
    if(!CheckBlockForUpgardableConstraints(block,state,"maximum-block-sigops",false))
    {
        return false;
    }
    
/*    
    unsigned int nSigOps = 0;
    BOOST_FOREACH(const CTransaction& tx, block.vtx)
    {
        nSigOps += GetLegacySigOpCount(tx);
    }
    if (nSigOps > MAX_BLOCK_SIGOPS)
        return state.DoS(100, error("CheckBlock() : out-of-bounds SigOpCount"),
                         REJECT_INVALID, "bad-blk-sigops", true);
*/
    return true;
}

bool CheckBranchForInvalidBlocks(CBlockIndex * const pindexPrev)
{
    if(pindexPrev == NULL)
    {
        return true;
    }
    
    const CBlockIndex *pindexFork;
    pindexFork=chainActive.FindFork(pindexPrev);
    
    CBlockIndex *pindexTest;
    pindexTest=pindexPrev;
    while(pindexTest != pindexFork)
    {
        if(pindexTest->nStatus & BLOCK_FAILED_MASK)
        {
            LogPrintf("Block is on branch containing invalid block %s (height %d)\n",pindexTest->GetBlockHash().ToString().c_str(),pindexTest->nHeight);
            return false;
        }

        pindexTest=pindexTest->pprev;
    }
        
    return true;
}



bool ContextualCheckBlockHeader(const CBlockHeader& block, CValidationState& state, CBlockIndex * const pindexPrev, CBlockIndex *pindexChecked)
{
    uint256 hash = block.GetHash();
    if (hash == Params().HashGenesisBlock())
        return true;

    assert(pindexPrev);

    int nHeight = pindexPrev->nHeight+1;

    // Check proof of work
    if ((!Params().SkipProofOfWorkCheck()) &&
       (block.nBits != GetNextWorkRequired(pindexPrev, &block)))
        return state.DoS(100, error("%s : incorrect proof of work", __func__),
                         REJECT_INVALID, "bad-diffbits");

    // Check timestamp against prev
    if (block.GetBlockTime() <= pindexPrev->GetMedianTimePast())
        return state.Invalid(error("%s : block's timestamp is too early", __func__),
                             REJECT_INVALID, "time-too-old");

    // Check that the block chain matches the known block chain up to a checkpoint
    if (!Checkpoints::CheckBlock(nHeight, hash))
        return state.DoS(100, error("%s : rejected by checkpoint lock-in at %d", __func__, nHeight),
                         REJECT_CHECKPOINT, "checkpoint mismatch");

    bool fWaitingForLocked=false;
    if(pindexLockedBlock)
    {
        if(!chainActive.Contains(pindexLockedBlock))
        {
            fWaitingForLocked=true;
        }
    }
    else
    {
        if(hLockedBlock != 0)
        {
            fWaitingForLocked=true;            
        }        
    }
    if(mc_gState->m_NetworkParams->IsProtocolMultichain())
    {
        const CBlockIndex *pindexFork;
        pindexFork=chainActive.FindFork(pindexPrev);
        
        if( (Params().Interval() <= 0) && !fWaitingForLocked )
        {
            if(pindexPrev != chainActive.Tip())
            {
                if( (MCP_ANYONE_CAN_ADMIN == 0) && 
                    (MCP_ANYONE_CAN_MINE == 0) )
                {
                    int nMinerCount=mc_gState->m_Permissions->GetMinerCount()-mc_gState->m_Permissions->GetActiveMinerCount()+1;
                    int nMaxHeight=chainActive.Height()-Params().LockAdminMineRounds()*nMinerCount;
                    int nMinHeight=pindexFork->nHeight;
                    if( (nMinHeight <= nMaxHeight) && (nMinHeight > 0) )
                    {
                        int nGovernanceModelChangeHeight=mc_gState->m_Permissions->FindGovernanceModelChange(nMinHeight,nMaxHeight);
                        if(nGovernanceModelChangeHeight)
                        {
                            if(fDebug)LogPrint("mcblock","mchn-block: Deep fork rejected: block %s, tip: %d, height: %d, fork: %d, rounds: %d; stop: %d\n",block.GetHash().ToString().c_str(),
                                    chainActive.Height(),nHeight,pindexFork->nHeight,Params().LockAdminMineRounds(),nGovernanceModelChangeHeight);
                            return state.Invalid(error("%s : rejected by lockadminrounds, fork: %d, change: %d", __func__,nMinHeight,nGovernanceModelChangeHeight),
                                                 REJECT_INVALID, "reorg-too-deep");                        
                        }
                        if(fDebug)LogPrint("mcblock","mchn-block: Deep fork accepted: block %s, tip: %d, height: %d, fork: %d, rounds: %d\n",block.GetHash().ToString().c_str(),
                                chainActive.Height(),nHeight,pindexFork->nHeight,Params().LockAdminMineRounds());
                    }
                }                                
            }
        }
    }

    if(pindexChecked != pindexPrev)                                         
    {
        if(!CheckBranchForInvalidBlocks(pindexPrev))
        {
            return state.Invalid(error("%s : %s rejected - invalid branch", __func__,block.GetHash().ToString().c_str()),
                                 REJECT_INVALID, "reorg-invalid branch");                                
        }
    }

    
    // Don't accept any forks from the main chain prior to last checkpoint
    CBlockIndex* pcheckpoint = Checkpoints::GetLastCheckpoint();
    if (pcheckpoint && nHeight < pcheckpoint->nHeight)
        return state.DoS(100, error("%s : forked chain older than last checkpoint (height %d)", __func__, nHeight));

    // Reject block.nVersion=1 blocks when 95% (75% on testnet) of the network has upgraded:
    if (block.nVersion < 2 && 
        CBlockIndex::IsSuperMajority(2, pindexPrev, Params().RejectBlockOutdatedMajority()))
    {
        return state.Invalid(error("%s : rejected nVersion=1 block", __func__),
                             REJECT_OBSOLETE, "bad-version");
    }

    // Reject block.nVersion=2 blocks when 95% (75% on testnet) of the network has upgraded:
    if (block.nVersion < 3 && CBlockIndex::IsSuperMajority(3, pindexPrev, Params().RejectBlockOutdatedMajority()))
    {
        return state.Invalid(error("%s : rejected nVersion=2 block", __func__),
                             REJECT_OBSOLETE, "bad-version");
    }

    return true;
}

bool ContextualCheckBlock(const CBlock& block, CValidationState& state, CBlockIndex * const pindexPrev)
{
    const int nHeight = pindexPrev == NULL ? 0 : pindexPrev->nHeight + 1;

    // Check that all transactions are finalized
    BOOST_FOREACH(const CTransaction& tx, block.vtx)
        if (!IsFinalTx(tx, nHeight, block.GetBlockTime())) {
            return state.DoS(10, error("%s : contains a non-final transaction", __func__), REJECT_INVALID, "bad-txns-nonfinal");
        }

    // Enforce block.nVersion=2 rule that the coinbase starts with serialized block height
    // if 750 of the last 1,000 blocks are version 2 or greater (51/100 if testnet):
    if (block.nVersion >= 2 && 
        CBlockIndex::IsSuperMajority(2, pindexPrev, Params().EnforceBlockUpgradeMajority()))
    {
        CScript expect = CScript() << nHeight;
        if (block.vtx[0].vin[0].scriptSig.size() < expect.size() ||
            !std::equal(expect.begin(), expect.end(), block.vtx[0].vin[0].scriptSig.begin())) {
            return state.DoS(100, error("%s : block height mismatch in coinbase", __func__), REJECT_INVALID, "bad-cb-height");
        }
    }

    
    return true;
}

bool AcceptBlockHeader(const CBlockHeader& block, CValidationState& state, CBlockIndex** ppindex, int node_id, CBlockIndex *pindexChecked)
{
    AssertLockHeld(cs_main);
    // Check for duplicate
    uint256 hash = block.GetHash();
    BlockMap::iterator miSelf = mapBlockIndex.find(hash);
    CBlockIndex *pindex = NULL;
    if (miSelf != mapBlockIndex.end()) {
        // Block header is already known.
        pindex = miSelf->second;
        if (ppindex)
            *ppindex = pindex;
        if (pindex->nStatus & BLOCK_FAILED_MASK)
            return state.Invalid(error("%s : block is marked invalid", __func__), 0, "duplicate");
        if(pindexChecked != pindex->pprev)                                         
        {
            if(!CheckBranchForInvalidBlocks(pindex->pprev))
            {
                return state.Invalid(error("%s : %s rejected - invalid branch", __func__,block.GetHash().ToString().c_str()),
                                     REJECT_INVALID, "reorg-invalid branch");                                
            }
        }
        return true;
    }

    if (!CheckBlockHeader(block, state))
        return false;

    // Get prev block index
    CBlockIndex* pindexPrev = NULL;
    if (hash != Params().HashGenesisBlock()) {
        BlockMap::iterator mi = mapBlockIndex.find(block.hashPrevBlock);
        if (mi == mapBlockIndex.end())
            return state.DoS(10, error("%s : prev block not found", __func__), 0, "bad-prevblk");
        pindexPrev = (*mi).second;
        if (pindexPrev->nStatus & BLOCK_FAILED_MASK)
            return state.DoS(10, error("%s : prev block invalid", __func__), REJECT_INVALID, "bad-prevblk");// MCHN was 100 before, softened for reorgs due to mining diversity change
    }

    if (!ContextualCheckBlockHeader(block, state, pindexPrev, pindexChecked))
        return false;

    int successor=0;
    int successors_from_this_node=0;
    mc_BlockHeaderInfo *lpNext;
    mc_BlockHeaderInfo *lpNextForPrev;
    mc_BlockHeaderInfo new_info;
    lpNext=NULL;    
    if( (node_id > 0) && (pindexPrev != NULL) )
    {
        CBlockIndex *pindexTmp=NULL;
        if( ((int)vFirstOnThisHeight.size() <= pindexPrev->nHeight) || (vFirstOnThisHeight[pindexPrev->nHeight] == NULL))
        {
            MultichainNode_UpdateBlockByHeightList(pindexPrev);
        }
        lpNextForPrev=NULL;
        pindexTmp=vFirstOnThisHeight[pindexPrev->nHeight];
        while(pindexTmp)
        {
            successor=pindexTmp->nFirstSuccessor;
            while(successor)
            {                
                lpNext=(mc_BlockHeaderInfo *)mc_gState->m_BlockHeaderSuccessors->GetRow(successor);
                if(lpNext->m_NodeId == node_id)
                {
                    successors_from_this_node++;
                }
                successor=lpNext->m_Next;
                if(pindexTmp == pindexPrev)
                {
                    lpNextForPrev=lpNext;
                }
            }
            pindexTmp=pindexTmp->pNextOnThisHeight;
        }
        if(successors_from_this_node >= GetArg("-maxheadersfrompeer", DEFAULT_MAX_SUCCESSORS_FROM_ONE_NODE))
        {
            return state.Invalid(error("%s : %s rejected - too many headers from node %d", __func__,block.GetHash().ToString().c_str(),node_id),
                                 REJECT_INVALID, "too-man-headers");                                            
        }
        successor=mc_gState->m_BlockHeaderSuccessors->GetCount();
        memset(&new_info,0,sizeof(mc_BlockHeaderInfo));
        memcpy(new_info.m_Hash,&hash,sizeof(uint256));
        new_info.m_NodeId=node_id;
        
        mc_gState->m_BlockHeaderSuccessors->Add(&new_info);
        if(lpNextForPrev)
        {
            lpNextForPrev->m_Next=successor;
        }
        else
        {
            pindexPrev->nFirstSuccessor=successor;
        }
    }

    if (pindex == NULL)
        pindex = AddToBlockIndex(block);

    if (ppindex)
        *ppindex = pindex;

    if(pindexLockedBlock == NULL)
    {
        if(hLockedBlock == pindex->GetBlockHash())
        {
            pindexLockedBlock=pindex;
            if(!chainActive.Contains(pindexLockedBlock))
            {
                const CBlockIndex *pindexFork;
                pindexFork=chainActive.FindFork(pindexLockedBlock);

                LogPrintf("Accepted header for block %s found on alternative chain at height %d, rewinding to fork block %s at height %d\n",
                        hLockedBlock.ToString().c_str(),pindexLockedBlock->nHeight,pindexFork->GetBlockHash().ToString().c_str(),pindexFork->nHeight);                
                string error=SetLastBlock(pindexFork->GetBlockHash());                                
            }
        }
    }
    
    return true;
}

bool AcceptBlock(CBlock& block, CValidationState& state, CBlockIndex** ppindex, CDiskBlockPos* dbp, int node_id)
{
    AssertLockHeld(cs_main);

    CBlockIndex *&pindex = *ppindex;

    if (!AcceptBlockHeader(block, state, &pindex, node_id))
        return false;

    if (pindex->nStatus & BLOCK_HAVE_DATA) {
        // TODO: deal better with duplicate blocks.
        // return state.DoS(20, error("AcceptBlock() : already have block %d %s", pindex->nHeight, pindex->GetBlockHash().ToString()), REJECT_DUPLICATE, "duplicate");
        return true;
    }

/* MCHN START*/    
    pindex->dTimeReceived=mc_TimeNowAsDouble();
            
    if(!VerifyBlockSignature(&block,false))
    {
        return false;
    }    
    
    if(block.vSigner[0])
    {
        pindex->kMiner.Set(block.vSigner+1, block.vSigner+1+block.vSigner[0]);
        pindex->nStatus |= BLOCK_HAVE_MINER_PUBKEY;
    }
            
/* MCHN END*/    
    

//    if ((!CheckBlock(block, state)) || !ContextualCheckBlock(block, state, pindex->pprev)) {
// AcceptBlock is called only in context of ProcessNewBlock. CheckBlock is called before. No reason to call it again
    if (!ContextualCheckBlock(block, state, pindex->pprev)) {
        if (state.IsInvalid() && !state.CorruptionPossible()) {
            pindex->nStatus |= BLOCK_FAILED_VALID;
            setDirtyBlockIndex.insert(pindex);
        }
        return false;
    }

    int nHeight = pindex->nHeight;

    if(!VerifyBlockMiner(&block,pindex))
    {
        pindex->nStatus |= BLOCK_FAILED_VALID;
        setDirtyBlockIndex.insert(pindex);
        return false;
    }
    
    CBlockIndex *pindexTip=pindex;
    while((pindexTip != NULL) && pindexTip->nHeight > chainActive.Height())
    {
        pindexTip=pindexTip->pprev;
    }
    if(pindexTip == chainActive.Tip())                                          // If we are on main chain
                                                                                // If it is the next block, we can check upgradable constraints
                                                                                // If some blocks are missing and constraints are broken - ignore block
                                                                                // If it is not an attack and upgrade parameters were indeed upgraded,
                                                                                // we will download this block again after missing blocks.
    {        
        if(!CheckBlockForUpgardableConstraints(block,state,"maximum-block-size",true))
        {
            return false;
        }
    
        if(!CheckBlockForUpgardableConstraints(block,state,"maximum-block-sigops",true))
        {
            return false;
        }        
    }
    else
    {
        LogPrintf("Accepted header for block %s found on alternative chain at height %d\n",
                pindex->GetBlockHash().ToString().c_str(),pindex->nHeight);                        
    }
    
    
    // Write block to history file
    try {
        unsigned int nBlockSize = ::GetSerializeSize(block, SER_DISK, CLIENT_VERSION);
        pindex->nSize=nBlockSize;
        pindex->nStatus |= BLOCK_HAVE_SIZE;

        CDiskBlockPos blockPos;
        if (dbp != NULL)
            blockPos = *dbp;
        if (!FindBlockPos(state, blockPos, nBlockSize+8, nHeight, block.GetBlockTime(), dbp != NULL))
            return error("AcceptBlock() : FindBlockPos failed");
        if (dbp == NULL)
            if (!WriteBlockToDisk(block, blockPos))
                return state.Abort("Failed to write block");
        if (!ReceivedBlockTransactions(block, state, pindex, blockPos))
            return error("AcceptBlock() : ReceivedBlockTransactions failed");
    } catch(std::runtime_error &e) {
        return state.Abort(std::string("System error: ") + e.what());
    }


    return true;
}

bool CBlockIndex::IsSuperMajority(int minVersion, const CBlockIndex* pstart, unsigned int nRequired)
{
    unsigned int nToCheck = Params().ToCheckBlockUpgradeMajority();
    unsigned int nFound = 0;
    for (unsigned int i = 0; i < nToCheck && nFound < nRequired && pstart != NULL; i++)
    {
        if (pstart->nVersion >= minVersion)
            ++nFound;
        pstart = pstart->pprev;
    }
    return (nFound >= nRequired);
}

/** Turn the lowest '1' bit in the binary representation of a number into a '0'. */
int static inline InvertLowestOne(int n) { return n & (n - 1); }

/** Compute what height to jump back to with the CBlockIndex::pskip pointer. */
int static inline GetSkipHeight(int height) {
    if (height < 2)
        return 0;

    // Determine which height to jump back to. Any number strictly lower than height is acceptable,
    // but the following expression seems to perform well in simulations (max 110 steps to go back
    // up to 2**18 blocks).
    return (height & 1) ? InvertLowestOne(InvertLowestOne(height - 1)) + 1 : InvertLowestOne(height);
}

CBlockIndex* CBlockIndex::GetAncestor(int height)
{
    if (height > nHeight || height < 0)
        return NULL;

    CBlockIndex* pindexWalk = this;
    int heightWalk = nHeight;
    while (heightWalk > height) {
        int heightSkip = GetSkipHeight(heightWalk);
        int heightSkipPrev = GetSkipHeight(heightWalk - 1);
        if (heightSkip == height ||
            (heightSkip > height && !(heightSkipPrev < heightSkip - 2 &&
                                      heightSkipPrev >= height))) {
            // Only follow pskip if pprev->pskip isn't better than pskip->pprev.
            pindexWalk = pindexWalk->pskip;
            heightWalk = heightSkip;
        } else {
            pindexWalk = pindexWalk->pprev;
            heightWalk--;
        }
    }
    return pindexWalk;
}

const CBlockIndex* CBlockIndex::GetAncestor(int height) const
{
    return const_cast<CBlockIndex*>(this)->GetAncestor(height);
}

void CBlockIndex::BuildSkip()
{
    if (pprev)
        pskip = pprev->GetAncestor(GetSkipHeight(nHeight));
}

bool ProcessNewBlock(CValidationState &state, CNode* pfrom, CBlock* pblock, CDiskBlockPos *dbp)
{
/* MCHN START*/    
    {
        LOCK(cs_main);
        if(!VerifyBlockSignature(pblock,true))
        {
            return false;
        }
    }
/* MCHN END*/    
    int old_height=chainActive.Height();
    double start_time=mc_TimeNowAsDouble();
    // Preliminary checks
    bool checked = CheckBlock(*pblock, state);
    bool activate=true;
        
    {
        LOCK(cs_main);
        MarkBlockAsReceived(pblock->GetHash());
        if (!checked) {
            return error("%s : CheckBlock FAILED", __func__);
        }

        // Store to disk
        CBlockIndex *pindex = NULL;
        bool ret = AcceptBlock(*pblock, state, &pindex, dbp, pfrom ? pfrom->GetId() : 0);
        if (pindex && pfrom) {
            mapBlockSource[pindex->GetBlockHash()] = pfrom->GetId();
        }
        if(pindexLockedBlock == NULL)
        {
            if(hLockedBlock == pblock->GetHash())
            {
                pindexLockedBlock=pindex;
            }
        }
        if (!ret)
            return error("%s : AcceptBlock FAILED", __func__);
        if(pindex)
        {
//            if(mc_gState->m_ProtocolVersionToUpgrade > mc_gState->m_NetworkParams->ProtocolVersion())
            if( (mc_gState->m_ProtocolVersionToUpgrade > 0) && (mc_gState->IsSupported(mc_gState->m_ProtocolVersionToUpgrade) == 0) )
            {
                if(chainActive.FindFork(pindex) == chainActive.Tip())
                {
                    if(fDebug)LogPrint("mcblock","Block %s is not connected, required protocol version upgrade %d -> %d\n",pindex->GetBlockHash().ToString().c_str(),
                            mc_gState->m_NetworkParams->ProtocolVersion(),mc_gState->m_ProtocolVersionToUpgrade);
                    activate=false;
                }
            }
            if(pindex->nHeight > nLastForkedHeight)
            {
                if(pindex->nHeight <= chainActive.Height())
                {
                    if(pindex != chainActive.Tip())
                    {
                        nLastForkedHeight=pindex->nHeight;
                    }
                }
            }
        }
    }

    
/*    
    if(GetBoolArg("-waitforbetterblock",false))
    {
        uint256 hash=pblock->GetHash();
        BOOST_FOREACH(const PAIRTYPE(uint256, CBlockIndex*)& item, mapBlockIndex)
        {            
            if(item.second->pprev)
            {
                if(item.second->pprev->GetBlockHash() == hash)
                {
                    activate=false;
                    if(fDebug)LogPrint("mcblockperf","mchn-block-perf: Found better block than %s (height %d) - %s\n",
                            hash.ToString().c_str(),item.second->pprev->nHeight,item.second->GetBlockHash().ToString().c_str());
                }
            }
        }
    }
 */ 
    if(activate)
    {
        {
            LOCK(cs_main);
            pEF->LIC_VerifyLicenses(-1);
        }
        if (!ActivateBestChain(state, pblock))
            return error("%s : ActivateBestChain failed", __func__);    
        {
            LOCK(cs_main);
            pEF->LIC_VerifyLicenses(chainActive.Height());
            pEF->NET_CheckConnections();
        }
    }

    nTotalMempoolsSize=TotalMempoolsSize();        

    int node_count=0;
    nTotalNodeBuffersSize=0;
    {
        LOCK(cs_vNodes);
        BOOST_FOREACH(CNode* pnode, vNodes)
        {
            nTotalNodeBuffersSize+=pnode->nTotalBuffersSize;
            node_count++;
        }
    }   

    int new_height=chainActive.Height();
    if(new_height>old_height)
    {
        double end_time=mc_TimeNowAsDouble();
        int nBlockWindow=20;
        for(int b=nBlockWindow-1;b>0;b--)
        {
            dBlockProcessingTime[b]=dBlockProcessingTime[b-1];
        }
        dBlockProcessingTime[0]=end_time-start_time;
        nBlockTimeMeasured++;
        int nb=nBlockWindow;
        if(nb>nBlockTimeMeasured)
        {
           nb=nBlockTimeMeasured; 
        }
        double weight=0;
        double total=0;
        for(int b=0;b<nb;b++)
        {
            total+=(nb-b)*dBlockProcessingTime[b];
            weight+=nb-b;
        }
        dAverageBlockTime=total/weight;

        TxThrottlingDelay(true);
    }
    
    
/* MCHN START */    
/*
    CBlockIndex* pblockindex = mapBlockIndex[pblock->GetHash()];
    if(pblockindex != chainActive.Tip())
    {
        if(fDebug)LogPrint("mchn","mchn: New block rejected as chain tip\n");
        if(pblockindex->nHeightMinedByMe == pblockindex->nHeight)
        {
            if(fDebug)LogPrint("mchn","mchn: New block is mined by me, relay it anyway\n");
            if (!IsInitialBlockDownload()) 
            {
                {
                    LOCK(cs_vNodes);
                    BOOST_FOREACH(CNode* pnode, vNodes)
                    {
                        pnode->PushInventory(CInv(MSG_BLOCK, pblock->GetHash()));
                    }
                }
            }
        }
    }    
 */ 
//    if(GetBoolArg("-shrinkdebugfilesize",false))
    if (GetBoolArg("-shrinkdebugfile", !fDebug))
    {
        ShrinkDebugFile();
    }
    
    pEF->ENT_MaybeStop();
    
/* MCHN END */    
    
    return true;
}

bool TestBlockValidity(CValidationState &state, const CBlock& block, CBlockIndex * const pindexPrev, bool fCheckPOW, bool fCheckMerkleRoot)
{
    AssertLockHeld(cs_main);
    assert(pindexPrev == chainActive.Tip());

    CCoinsViewCache viewNew(pcoinsTip);
    CBlockIndex indexDummy(block);
    indexDummy.pprev = pindexPrev;
    indexDummy.nHeight = pindexPrev->nHeight + 1;

    // NOTE: CheckBlockHeader is called by CheckBlock
    if (!ContextualCheckBlockHeader(block, state, pindexPrev))
        return false;
    if (!CheckBlock(block, state, fCheckPOW, fCheckMerkleRoot))
        return false;
    if (!ContextualCheckBlock(block, state, pindexPrev))
        return false;
    if (!ConnectBlock(block, state, &indexDummy, viewNew, true))
        return false;
    assert(state.IsValid());

    return true;
}








bool AbortNode(const std::string &strMessage, const std::string &userMessage) {
    strMiscWarning = strMessage;
    LogPrintf("*** %s\n", strMessage);
    uiInterface.ThreadSafeMessageBox(
        userMessage.empty() ? _("Error: A fatal internal error occured, see debug.log for details") : userMessage,
        "", CClientUIInterface::MSG_ERROR);
    StartShutdown();
    return false;
}

bool CheckDiskSpace(uint64_t nAdditionalBytes)
{
    uint64_t nFreeBytesAvailable = filesystem::space(GetDataDir()).available;

    // Check for nMinDiskSpace bytes (currently 50MB)
    if (nFreeBytesAvailable < nMinDiskSpace + nAdditionalBytes)
        return AbortNode("Disk space is low!", _("Error: Disk space is low!"));

    return true;
}

FILE* OpenDiskFile(const CDiskBlockPos &pos, const char *prefix, bool fReadOnly)
{
    if (pos.IsNull())
        return NULL;
    boost::filesystem::path path = GetBlockPosFilename(pos, prefix);
    boost::filesystem::create_directories(path.parent_path());
    FILE* file = fopen(path.string().c_str(), "rb+");
    if (!file && !fReadOnly)
        file = fopen(path.string().c_str(), "wb+");
    if (!file) {
        LogPrintf("Unable to open file %s\n", path.string());
        return NULL;
    }
    if (pos.nPos) {
        if (fseek(file, pos.nPos, SEEK_SET)) {
            LogPrintf("Unable to seek to position %u of %s\n", pos.nPos, path.string());
            fclose(file);
            return NULL;
        }
    }
    return file;
}

FILE* OpenBlockFile(const CDiskBlockPos &pos, bool fReadOnly) {
    return OpenDiskFile(pos, "blk", fReadOnly);
}

FILE* OpenUndoFile(const CDiskBlockPos &pos, bool fReadOnly) {
    return OpenDiskFile(pos, "rev", fReadOnly);
}

boost::filesystem::path GetBlockPosFilename(const CDiskBlockPos &pos, const char *prefix)
{
    return GetDataDir() / "blocks" / strprintf("%s%05u.dat", prefix, pos.nFile);
}

CBlockIndex * InsertBlockIndex(uint256 hash)
{
    if (hash == 0)
        return NULL;

    // Return existing
    BlockMap::iterator mi = mapBlockIndex.find(hash);
    if (mi != mapBlockIndex.end())
        return (*mi).second;

    // Create new
    CBlockIndex* pindexNew = new CBlockIndex();
    if (!pindexNew)
        throw runtime_error("LoadBlockIndex() : new CBlockIndex failed");
    mc_gState->ChainLock();
    mi = mapBlockIndex.insert(make_pair(hash, pindexNew)).first;
    mc_gState->ChainUnLock();
    pindexNew->phashBlock = &((*mi).first);

    return pindexNew;
}

bool static LoadBlockIndexDB(std::string& strError)
{
    if (!pblocktree->LoadBlockIndexGuts())
        return false;

    boost::this_thread::interruption_point();

    // Calculate nChainWork
    vector<pair<int, CBlockIndex*> > vSortedByHeight;
    vSortedByHeight.reserve(mapBlockIndex.size());
    BOOST_FOREACH(const PAIRTYPE(uint256, CBlockIndex*)& item, mapBlockIndex)
    {
        CBlockIndex* pindex = item.second;
        vSortedByHeight.push_back(make_pair(pindex->nHeight, pindex));
    }
    sort(vSortedByHeight.begin(), vSortedByHeight.end());
    BOOST_FOREACH(const PAIRTYPE(int, CBlockIndex*)& item, vSortedByHeight)
    {
        CBlockIndex* pindex = item.second;
        pindex->nChainWork = (pindex->pprev ? pindex->pprev->nChainWork : 0) + GetBlockProof(*pindex);
        if (pindex->nStatus & BLOCK_HAVE_DATA) {
            if (pindex->pprev) {
                if (pindex->pprev->nChainTx) {
                    pindex->nChainTx = pindex->pprev->nChainTx + pindex->nTx;
                } else {
                    pindex->nChainTx = 0;
                    mapBlocksUnlinked.insert(std::make_pair(pindex->pprev, pindex));
                }
            } else {
                pindex->nChainTx = pindex->nTx;
            }
        }
        if (pindex->IsValid(BLOCK_VALID_TRANSACTIONS) && (pindex->nChainTx || pindex->pprev == NULL))
            setBlockIndexCandidates.insert(pindex);
        if (pindex->nStatus & BLOCK_FAILED_MASK && (!pindexBestInvalid || pindex->nChainWork > pindexBestInvalid->nChainWork))
            pindexBestInvalid = pindex;
        if (pindex->pprev)
            pindex->BuildSkip();
        if (pindex->IsValid(BLOCK_VALID_TREE) && (pindexBestHeader == NULL || CBlockIndexWorkComparator()(pindexBestHeader, pindex)))
            pindexBestHeader = pindex;
    }

    // Load block file info
    pblocktree->ReadLastBlockFile(nLastBlockFile);
    vinfoBlockFile.resize(nLastBlockFile + 1);
    LogPrintf("%s: last block file = %i\n", __func__, nLastBlockFile);
    for (int nFile = 0; nFile <= nLastBlockFile; nFile++) {
        pblocktree->ReadBlockFileInfo(nFile, vinfoBlockFile[nFile]);
    }
    LogPrintf("%s: last block file info: %s\n", __func__, vinfoBlockFile[nLastBlockFile].ToString());
    for (int nFile = nLastBlockFile + 1; true; nFile++) {
        CBlockFileInfo info;
        if (pblocktree->ReadBlockFileInfo(nFile, info)) {
            vinfoBlockFile.push_back(info);
        } else {
            break;
        }
    }

    // Check presence of blk files
    LogPrintf("Checking all blk files are present...\n");
    set<int> setBlkDataFiles;
    BOOST_FOREACH(const PAIRTYPE(uint256, CBlockIndex*)& item, mapBlockIndex)
    {
        CBlockIndex* pindex = item.second;
        if (pindex->nStatus & BLOCK_HAVE_DATA) {
            setBlkDataFiles.insert(pindex->nFile);
        }
    }
    for (std::set<int>::iterator it = setBlkDataFiles.begin(); it != setBlkDataFiles.end(); it++)
    {
        CDiskBlockPos pos(*it, 0);
        if (CAutoFile(OpenBlockFile(pos, true), SER_DISK, CLIENT_VERSION).IsNull()) {
            return false;
        }
    }

    // Check whether we need to continue reindexing
    bool fReindexing = false;
    pblocktree->ReadReindexing(fReindexing);
    fReindex |= fReindexing;

    // Check whether we have a transaction index
    pblocktree->ReadFlag("txindex", fTxIndex);
    LogPrintf("LoadBlockIndexDB(): transaction index %s\n", fTxIndex ? "enabled" : "disabled");

    // Load pointer to end of best chain
    BlockMap::iterator it = mapBlockIndex.find(pcoinsTip->GetBestBlock());
    if (it == mapBlockIndex.end())
        return true;
    chainActive.SetTip(it->second);

    PruneBlockIndexCandidates();
/* MCHN START */
    
    bool corrupted=false;
    if(mc_gState->m_Permissions->m_Block < chainActive.Height())
    {
        corrupted=true;
        LogPrintf("mchn: Permission DB is behind current chain tip. Permission DB: %d, Chain tip: %d\n",mc_gState->m_Permissions->m_Block,chainActive.Height());        
    }
    if(mc_gState->m_Assets->m_Block < chainActive.Height())
    {
        corrupted=true;
        LogPrintf("mchn: Entities DB is behind current chain tip. Entities DB: %d, Chain tip: %d\n",mc_gState->m_Assets->m_Block,chainActive.Height());        
    }
    if(mc_gState->m_Permissions->m_Block != mc_gState->m_Assets->m_Block)
    {
        corrupted=true;
        LogPrintf("mchn: Permission and Entities DB have different heights. Permission DB: %d, Entities DB: %d, Chain tip: %d\n",mc_gState->m_Permissions->m_Block,mc_gState->m_Assets->m_Block,chainActive.Height());                
    }
    if(mc_gState->m_WalletMode & MC_WMD_TXS)
    {
        if(!GetBoolArg("-rescan", false))
        {
            if(pwalletTxsMain->GetBlock() < chainActive.Height())
            {
                corrupted=true;
                LogPrintf("mchn: Wallet Tx DB is behind current chain tip. Wallet Tx DB: %d, Chain tip: %d\n",pwalletTxsMain->GetBlock(),chainActive.Height());        
            }
            if(mc_gState->m_Permissions->m_Block != pwalletTxsMain->GetBlock())
            {
                corrupted=true;
                LogPrintf("mchn: Permission and Wallet Tx DB have different heights. Permission DB: %d, Wallet Tx  DB: %d, Chain tip: %d\n",mc_gState->m_Permissions->m_Block,pwalletTxsMain->GetBlock(),chainActive.Height());                
            }
        }
    }
    if(corrupted)
    {
        corrupted=false;
        int block_to_rollback=mc_gState->m_Permissions->m_Block;
        if(mc_gState->m_Assets->m_Block < block_to_rollback)
        {
            block_to_rollback=mc_gState->m_Assets->m_Block;
        }
        if(mc_gState->m_WalletMode & MC_WMD_TXS)
        {
            if(!GetBoolArg("-rescan", false))
            {
                if(pwalletTxsMain->GetBlock() < block_to_rollback)
                {
                    block_to_rollback=pwalletTxsMain->GetBlock();
                }
            }
        }
        if(block_to_rollback < 0)
        {
            block_to_rollback=0;
        }
        if(block_to_rollback < chainActive.Height())
        {
            LogPrintf("mchn: Permission/Entities/WalletTx DB is behind current chain tip. Shifting chain tip to %d\n",block_to_rollback);    
            SetLastBlock(chainActive[block_to_rollback]->GetBlockHash());
            SetLastBlock(0);
        }
    }
        
    
    if(fDebug)LogPrint("mchn","mchn: Rolling back permission DB to height %d\n",chainActive.Height());
    if(mc_gState->m_Permissions->RollBack(chainActive.Height()) != MC_ERR_NOERROR)
    {
        LogPrintf("ERROR: Couldn't roll back permission DB to height %d\n",chainActive.Height());                                    
        return false;        
    }
    if(fDebug)LogPrint("mchn","mchn: Rolling back asset DB to height %d\n",chainActive.Height());
    if(mc_gState->m_Assets->RollBack(chainActive.Height()) != MC_ERR_NOERROR)
    {
        LogPrintf("ERROR: Couldn't roll back asset DB to height %d\n",chainActive.Height());                                    
        return false;        
    }
    if(mc_gState->m_WalletMode & MC_WMD_TXS)
    {
        if(fDebug)LogPrint("mchn","mchn: Rolling back wallet txs DB to height %d\n",chainActive.Height());
        if(pwalletTxsMain->RollBack(NULL,chainActive.Height()) != MC_ERR_NOERROR)
        {
            LogPrintf("ERROR: Couldn't roll back wallet txs DB to height %d\n",chainActive.Height());                                    
            if(!GetBoolArg("-skipwalletchecks",false) && !GetBoolArg("-rescan", false))
            {
                strError="Error: The wallet database is inconsistent. Restart MultiChain with -rescan to rebuild the wallet database from the blockchain (can take hours), or with -skipwalletchecks to continue operating anyway";
                return false;
            }
        }
    }
    MultichainNode_ApplyUpgrades(chainActive.Height());        
    
/* MCHN END */
    LogPrintf("LoadBlockIndexDB(): hashBestChain=%s height=%d date=%s progress=%f\n",
        chainActive.Tip()->GetBlockHash().ToString(), chainActive.Height(),
        DateTimeStrFormat("%Y-%m-%d %H:%M:%S", chainActive.Tip()->GetBlockTime()),
        Checkpoints::GuessVerificationProgress(chainActive.Tip()));

    return true;
}

CVerifyDB::CVerifyDB()
{
    uiInterface.ShowProgress(_("Verifying blocks..."), 0);
}

CVerifyDB::~CVerifyDB()
{
    uiInterface.ShowProgress("", 100);
}

bool CVerifyDB::VerifyDB(CCoinsView *coinsview, int nCheckLevel, int nCheckDepth)
{
    LOCK(cs_main);
    if (chainActive.Tip() == NULL || chainActive.Tip()->pprev == NULL)
        return true;

    // Verify blocks in the best chain
    if (nCheckDepth <= 0)
        nCheckDepth = 1000000000; // suffices until the year 19000
    if (nCheckDepth > chainActive.Height())
        nCheckDepth = chainActive.Height();
    nCheckLevel = std::max(0, std::min(4, nCheckLevel));
    LogPrintf("Verifying last %i blocks at level %i\n", nCheckDepth, nCheckLevel);
    CCoinsViewCache coins(coinsview);
    CBlockIndex* pindexState = chainActive.Tip();
    CBlockIndex* pindexFailure = NULL;
    int nGoodTransactions = 0;
    CValidationState state;
    for (CBlockIndex* pindex = chainActive.Tip(); pindex && pindex->pprev; pindex = pindex->pprev)
    {
        boost::this_thread::interruption_point();
        uiInterface.ShowProgress(_("Verifying blocks..."), std::max(1, std::min(99, (int)(((double)(chainActive.Height() - pindex->nHeight)) / (double)nCheckDepth * (nCheckLevel >= 4 ? 50 : 100)))));
        if (pindex->nHeight < chainActive.Height()-nCheckDepth)
            break;
        CBlock block;
        // check level 0: read from disk
        if (!ReadBlockFromDisk(block, pindex))
            return error("VerifyDB() : *** ReadBlockFromDisk failed at %d, hash=%s", pindex->nHeight, pindex->GetBlockHash().ToString());
        // check level 1: verify block validity
        if (nCheckLevel >= 1 && !CheckBlock(block, state))
            return error("VerifyDB() : *** found bad block at %d, hash=%s\n", pindex->nHeight, pindex->GetBlockHash().ToString());
        
/* MCHN START */        
        if (nCheckLevel >= 1 && (pindex->nHeight <= mc_gState->m_Permissions->m_Block) && (mc_gState->m_Permissions->VerifyBlockHash(pindex->nHeight,pindex->GetBlockHash().begin()) == 0))
            return error("VerifyDB() : *** found bad permission data at %d, hash=%s\n", pindex->nHeight, pindex->GetBlockHash().ToString());        
/* MCHN END */        
        // check level 2: verify undo validity
        if (nCheckLevel >= 2 && pindex) {
            CBlockUndo undo;
            CDiskBlockPos pos = pindex->GetUndoPos();
            if (!pos.IsNull()) {
                if (!undo.ReadFromDisk(pos, pindex->pprev->GetBlockHash()))
                    return error("VerifyDB() : *** found bad undo data at %d, hash=%s\n", pindex->nHeight, pindex->GetBlockHash().ToString());
            }
        }
        // check level 3: check for inconsistencies during memory-only disconnect of tip blocks
        if (nCheckLevel >= 3 && pindex == pindexState && (coins.GetCacheSize() + pcoinsTip->GetCacheSize()) <= nCoinCacheSize) {
            bool fClean = true;
            int err;
            err=pEF->FED_EventBlock(block, state, pindex,"remove",false,false);
            if(err)
            {
                LogPrintf("ERROR: Cannot disconnect(before) block %s in feeds, error %d\n",pindex->GetBlockHash().ToString().c_str(),err);
            }        
            if (!DisconnectBlock(block, state, pindex, coins, &fClean))
            {
                err=pEF->FED_EventBlock(block, state, pindex,"remove",false,true);
                if(err)
                {
                    LogPrintf("ERROR: Cannot disconnect(error) block %s in feeds, error %d\n",pindex->GetBlockHash().ToString().c_str(),err);
                }        
                return error("VerifyDB() : *** irrecoverable inconsistency in block data at %d, hash=%s", pindex->nHeight, pindex->GetBlockHash().ToString());
            }
            err=pEF->FED_EventBlock(block, state, pindex,"remove",false,false);
            if(err)
            {
                LogPrintf("ERROR: Cannot disconnect(after) block %s in feeds, error %d\n",pindex->GetBlockHash().ToString().c_str(),err);
            }        
            pindexState = pindex->pprev;
            if (!fClean) {
                nGoodTransactions = 0;
                pindexFailure = pindex;
            } else
                nGoodTransactions += block.vtx.size();
        }
        if (ShutdownRequested())
            return true;
    }
    if (pindexFailure)
        return error("VerifyDB() : *** coin database inconsistencies found (last %i blocks, %i good transactions before that)\n", chainActive.Height() - pindexFailure->nHeight + 1, nGoodTransactions);

    // check level 4: try reconnecting blocks
    if (nCheckLevel >= 4) {
        CBlockIndex *pindex = pindexState;
        while (pindex != chainActive.Tip()) {
            boost::this_thread::interruption_point();
            uiInterface.ShowProgress(_("Verifying blocks..."), std::max(1, std::min(99, 100 - (int)(((double)(chainActive.Height() - pindex->nHeight)) / (double)nCheckDepth * 50))));
            pindex = chainActive.Next(pindex);
            CBlock block;
            if (!ReadBlockFromDisk(block, pindex))
                return error("VerifyDB() : *** ReadBlockFromDisk failed at %d, hash=%s", pindex->nHeight, pindex->GetBlockHash().ToString());
            if(pindex->nHeight == 0)
            {
                pindex->nSize=GenesisBlockSize;
            }
            int err;
            err=pEF->FED_EventBlock(block, state, pindex,"check",false,false);
            if(err)
            {
                LogPrintf("ERROR: Cannot connect(before) block %s in feeds, error %d\n",pindex->GetBlockHash().ToString().c_str(),err);
            }        
            if(pMultiChainFilterEngine)
            {
                pMultiChainFilterEngine->m_CoinsCache=&coins;
            }            
            if (!ConnectBlock(block, state, pindex, coins))
            {
                if(pMultiChainFilterEngine)
                {
                    pMultiChainFilterEngine->m_CoinsCache=NULL;
                }
                err=pEF->FED_EventBlock(block, state, pindex,"check",true,true);
                if(err)
                {
                    LogPrintf("ERROR: Cannot connect(before) block %s in feeds, error %d\n",pindex->GetBlockHash().ToString().c_str(),err);
                }        
                return error("VerifyDB() : *** found unconnectable block at %d, hash=%s", pindex->nHeight, pindex->GetBlockHash().ToString());
            }
            if(pMultiChainFilterEngine)
            {
                pMultiChainFilterEngine->m_CoinsCache=NULL;
            }
            err=pEF->FED_EventBlock(block, state, pindex,"check",true,false);
            if(err)
            {
                LogPrintf("ERROR: Cannot connect(before) block %s in feeds, error %d\n",pindex->GetBlockHash().ToString().c_str(),err);
            }        
        }
    }

    LogPrintf("No coin database inconsistencies in last %i blocks (%i transactions)\n", chainActive.Height() - pindexState->nHeight, nGoodTransactions);

    return true;
}

void UnloadBlockIndex()
{
    mapBlockIndex.clear();
    setBlockIndexCandidates.clear();
    chainActive.SetTip(NULL);
    pindexBestInvalid = NULL;
}

bool LoadBlockIndex(std::string& strError)
{
    // Load block index from databases
    if (!fReindex && !LoadBlockIndexDB(strError))
        return false;
    return true;
}


bool InitBlockIndex() {
    LOCK(cs_main);
    // Check whether we're already initialized
    if (chainActive.Genesis() != NULL)
        return true;

    // Use the provided setting for -txindex in the new database
/* MCHN START */    
/* Default was false */    
    fTxIndex = GetBoolArg("-txindex", true);
/* MCHN END */    
    pblocktree->WriteFlag("txindex", fTxIndex);
    LogPrintf("Initializing databases...\n");

    // Only add the genesis block if not reindexing (in which case we reuse the one already on disk)
    if (!fReindex) {
        try {
            CBlock &block = const_cast<CBlock&>(Params().GenesisBlock());
            // Start new block file
            unsigned int nBlockSize = ::GetSerializeSize(block, SER_DISK, CLIENT_VERSION);
            CDiskBlockPos blockPos;
            CValidationState state;
            if (!FindBlockPos(state, blockPos, nBlockSize+8, 0, block.GetBlockTime()))
                return error("LoadBlockIndex() : FindBlockPos failed");
            if (!WriteBlockToDisk(block, blockPos))
                return error("LoadBlockIndex() : writing genesis block to disk failed");
            CBlockIndex *pindex = AddToBlockIndex(block);
            if (!ReceivedBlockTransactions(block, state, pindex, blockPos))
                return error("LoadBlockIndex() : genesis block not accepted");
            if (!ActivateBestChain(state, &block))
                return error("LoadBlockIndex() : genesis block cannot be activated");
            // Force a chainstate write so that when we VerifyDB in a moment, it doesnt check stale data
            return FlushStateToDisk(state, FLUSH_STATE_ALWAYS);
        } catch(std::runtime_error &e) {
            return error("LoadBlockIndex() : failed to initialize block database: %s", e.what());
        }
    }
    
    return true;
}



bool LoadExternalBlockFile(FILE* fileIn, CDiskBlockPos *dbp)
{
    // Map of disk positions for blocks with unknown parent (only used for reindex)
    static std::multimap<uint256, CDiskBlockPos> mapBlocksUnknownParent;
    int64_t nStart = GetTimeMillis();

    uint32_t effective_max_block_size=GetArg("-loadblockmaxsize",2*MAX_BLOCK_SIZE);
    
    int nLoaded = 0;
    try {
        // This takes over fileIn and calls fclose() on it in the CBufferedFile destructor
        CBufferedFile blkdat(fileIn, 2*effective_max_block_size, effective_max_block_size+8, SER_DISK, CLIENT_VERSION);
        uint64_t nRewind = blkdat.GetPos();
        while (!blkdat.eof()) {
            boost::this_thread::interruption_point();

            blkdat.SetPos(nRewind);
            nRewind++; // start one byte further next time, in case of failure
            blkdat.SetLimit(); // remove former limit
            unsigned int nSize = 0;
            try {
                // locate a header
                unsigned char buf[MESSAGE_START_SIZE];
                blkdat.FindByte(Params().MessageStart()[0]);
                nRewind = blkdat.GetPos()+1;
                blkdat >> FLATDATA(buf);
                if (memcmp(buf, Params().MessageStart(), MESSAGE_START_SIZE))
                    continue;
                // read size
                blkdat >> nSize;
                if (nSize < 80 || nSize > MAX_BLOCK_SIZE)
                    continue;
            } catch (const std::exception &) {
                // no valid block header found; don't complain
                break;
            }
            try {
                // read block
                uint64_t nBlockPos = blkdat.GetPos();
                if (dbp)
                    dbp->nPos = nBlockPos;
                blkdat.SetLimit(nBlockPos + nSize);
                blkdat.SetPos(nBlockPos);
                CBlock block;
                blkdat >> block;
                nRewind = blkdat.GetPos();

                // detect out of order blocks, and store them for later
                uint256 hash = block.GetHash();
                if (hash != Params().HashGenesisBlock() && mapBlockIndex.find(block.hashPrevBlock) == mapBlockIndex.end()) {
                    if(fDebug)LogPrint("reindex", "%s: Out of order block %s, parent %s not known\n", __func__, hash.ToString(),
                            block.hashPrevBlock.ToString());
                    if (dbp)
                        mapBlocksUnknownParent.insert(std::make_pair(block.hashPrevBlock, *dbp));
                    continue;
                }

                // process in case the block isn't known yet
                if (mapBlockIndex.count(hash) == 0 || (mapBlockIndex[hash]->nStatus & BLOCK_HAVE_DATA) == 0) {
                    CValidationState state;
                    if (ProcessNewBlock(state, NULL, &block, dbp))
                        nLoaded++;
                    if (state.IsError())
                        break;
                } else if (hash != Params().HashGenesisBlock() && mapBlockIndex[hash]->nHeight % 1000 == 0) {
                    LogPrintf("Block Import: already had block %s at height %d\n", hash.ToString(), mapBlockIndex[hash]->nHeight);
                }

                // Recursively process earlier encountered successors of this block
                deque<uint256> queue;
                queue.push_back(hash);
                while (!queue.empty()) {
                    uint256 head = queue.front();
                    queue.pop_front();
                    std::pair<std::multimap<uint256, CDiskBlockPos>::iterator, std::multimap<uint256, CDiskBlockPos>::iterator> range = mapBlocksUnknownParent.equal_range(head);
                    while (range.first != range.second) {
                        std::multimap<uint256, CDiskBlockPos>::iterator it = range.first;
                        if (ReadBlockFromDisk(block, it->second))
                        {
                            LogPrintf("%s: Processing out of order child %s of %s\n", __func__, block.GetHash().ToString(),
                                    head.ToString());
                            CValidationState dummy;
                            if (ProcessNewBlock(dummy, NULL, &block, &it->second))
                            {
                                nLoaded++;
                                queue.push_back(block.GetHash());
                            }
                        }
                        range.first++;
                        mapBlocksUnknownParent.erase(it);
                    }
                }
            } catch (std::exception &e) {
                LogPrintf("%s : Deserialize or I/O error - %s", __func__, e.what());
            }
        }
    } catch(std::runtime_error &e) {
        AbortNode(std::string("System error: ") + e.what());
    }
    if (nLoaded > 0)
        LogPrintf("Loaded %i blocks from external file in %dms\n", nLoaded, GetTimeMillis() - nStart);
    return nLoaded > 0;
}

//////////////////////////////////////////////////////////////////////////////
//
// CAlert
//

string GetWarnings(string strFor)
{
    int nPriority = 0;
    string strStatusBar;
    string strRPC;

    if (!CLIENT_VERSION_IS_RELEASE)
        strStatusBar = _("This is a pre-release test build - use at your own risk - do not use for mining or merchant applications");

    if (GetBoolArg("-testsafemode", false))
        strStatusBar = strRPC = "testsafemode enabled";

    // Misc warnings like out of disk space and clock is wrong
    if (strMiscWarning != "")
    {
        nPriority = 1000;
        strStatusBar = strMiscWarning;
    }

    if (fLargeWorkForkFound)
    {
        nPriority = 2000;
        strStatusBar = strRPC = _("Warning: The network does not appear to fully agree! Some miners appear to be experiencing issues.");
    }
    else if (fLargeWorkInvalidChainFound)
    {
        nPriority = 2000;
        strStatusBar = strRPC = _("Warning: We do not appear to fully agree with our peers! You may need to upgrade, or other nodes may need to upgrade.");
    }

    // Alerts
    {
        LOCK(cs_mapAlerts);
        BOOST_FOREACH(PAIRTYPE(const uint256, CAlert)& item, mapAlerts)
        {
            const CAlert& alert = item.second;
            if (alert.AppliesToMe() && alert.nPriority > nPriority)
            {
                nPriority = alert.nPriority;
                strStatusBar = alert.strStatusBar;
            }
        }
    }

    if (strFor == "statusbar")
        return strStatusBar;
    else if (strFor == "rpc")
        return strRPC;
    assert(!"GetWarnings() : invalid parameter");
    return "error";
}








//////////////////////////////////////////////////////////////////////////////
//
// Messages
//


bool static AlreadyHave(const CInv& inv)
{
    switch (inv.type)
    {
    case MSG_TX:
        {
            bool txInMap = false;
            txInMap = mempool.exists(inv.hash);
            return txInMap || mapOrphanTransactions.count(inv.hash) ||
                pcoinsTip->HaveCoins(inv.hash);
        }
    case MSG_BLOCK:
        return mapBlockIndex.count(inv.hash);
    }
    // Don't know what it is, just say we already got one
    return true;
}


bool ProcessGetData(CNode* pfrom)
{
    std::deque<CInv>::iterator it = pfrom->vRecvGetData.begin();

/* MCHN START */    
    if(!MultichainNode_RespondToGetData(pfrom)) 
    {
        pfrom->vRecvGetData.erase(pfrom->vRecvGetData.begin(), pfrom->vRecvGetData.end());        
        return true;
    }
/* MCHN END */    
    
    vector<CInv> vNotFound;

//    LOCK(cs_main);

    while (it != pfrom->vRecvGetData.end()) {
        // Don't bother if send buffer is too full to respond anyway
        if (pfrom->nSendSize >= SendBufferSize())
        {
            if(fDebug)LogPrint("mcminor","mchn: Send buffer full on getdata (%ld bytes) for peer %d\n", pfrom->nSendSize, pfrom->id);            
            break;
        }

        const CInv &inv = *it;
        {
            boost::this_thread::interruption_point();
            it++;

            if (inv.type == MSG_BLOCK || inv.type == MSG_FILTERED_BLOCK)
            {
                bool send = false;
                
                CDiskBlockPos block_pos;
                int block_height;
                
                mc_gState->ChainLock();
                BlockMap::iterator mi = mapBlockIndex.find(inv.hash);
                if (mi != mapBlockIndex.end())
                {
                    block_pos=mi->second->GetBlockPos();
                    block_height=mi->second->nHeight;
                    
                    // If the requested block is at a height below our last
                    // checkpoint, only serve it if it's in the checkpointed chain
                    block_height = mi->second->nHeight;
                    CBlockIndex* pcheckpoint = Checkpoints::GetLastCheckpoint();
                    if (pcheckpoint && block_height < pcheckpoint->nHeight) {
                        if (!chainActive.Contains(mi->second))
                        {
                            LogPrintf("ProcessGetData(): ignoring request for old block that isn't in the main chain\n");
                        } else {
                            send = true;
                        }
                    } else {
                        send = true;
                    }
                }
                mc_gState->ChainUnLock();
                if (send)
                {
                    // Send block from disk
                    CBlock block;
                    if (!ReadBlockFromDisk(block, block_pos))
                        assert(!"cannot load block from disk");
                    
                    if(fDebug)LogPrint("mcnet","mcnet: Sending block: %s (height %d), to peer=%d\n",inv.hash.ToString().c_str(),block_height,pfrom->id);            
                    LogPrint("mcblockperf","mchn-block-perf: Sending block %s (height %d), to node %d\n",
                            inv.hash.ToString().c_str(),block_height,pfrom->id);                    
                    if (inv.type == MSG_BLOCK)
                        pfrom->PushMessage("block", block);
                    else // MSG_FILTERED_BLOCK)
                    {
                        LOCK(pfrom->cs_filter);
                        if (pfrom->pfilter)
                        {
                            CMerkleBlock merkleBlock(block, *pfrom->pfilter);
                            pfrom->PushMessage("merkleblock", merkleBlock);
                            // CMerkleBlock just contains hashes, so also push any transactions in the block the client did not see
                            // This avoids hurting performance by pointlessly requiring a round-trip
                            // Note that there is currently no way for a node to request any single transactions we didnt send here -
                            // they must either disconnect and retry or request the full block.
                            // Thus, the protocol spec specified allows for us to provide duplicate txn here,
                            // however we MUST always provide at least what the remote peer needs
                            typedef std::pair<unsigned int, uint256> PairType;
                            BOOST_FOREACH(PairType& pair, merkleBlock.vMatchedTxn)
                                if (!pfrom->setInventoryKnown.count(CInv(MSG_TX, pair.second)))
                                    pfrom->PushMessage("tx", block.vtx[pair.first]);
                        }
                        // else
                            // no response
                    }

                    // Trigger them to send a getblocks request for the next batch of inventory
                    if (inv.hash == pfrom->hashContinue)
                    {
                        // Bypass PushInventory, this must send even if redundant,
                        // and we want it right after the last block so they don't
                        // wait for other stuff first.
                        vector<CInv> vInv;
                        vInv.push_back(CInv(MSG_BLOCK, chainActive.Tip()->GetBlockHash()));
                        pfrom->PushMessage("inv", vInv);
                        pfrom->hashContinue = 0;
                    }
                }
            }
            else if (inv.IsKnownType())
            {
                // Send stream from relay memory
                bool pushed = false;
                {
                    LOCK(cs_mapRelay);
                    map<CInv, CDataStream>::iterator mi = mapRelay.find(inv);
                    if (mi != mapRelay.end()) {
                        pfrom->PushMessage(inv.GetCommand(), (*mi).second);
                        pushed = true;
                    }
                }
                if (!pushed && inv.type == MSG_TX) {
                    CTransaction tx;
                    if (mempool.lookup(inv.hash, tx)) {
                        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
                        ss.reserve(1000);
                        ss << tx;
                        pfrom->PushMessage("tx", ss);
                        pushed = true;
                    }
                }
                if (!pushed) {
                    vNotFound.push_back(inv);
                }
            }

            // Track requests for our stuff.
            g_signals.Inventory(inv.hash);

            if (inv.type == MSG_BLOCK || inv.type == MSG_FILTERED_BLOCK)
                break;
        }
    }

    pfrom->vRecvGetData.erase(pfrom->vRecvGetData.begin(), it);

    if (!vNotFound.empty()) {
        // Let the peer know that we didn't find what it asked for, so it doesn't
        // have to wait around forever. Currently only SPV clients actually care
        // about this message: it's needed when they are recursively walking the
        // dependencies of relevant unconfirmed transactions. SPV clients want to
        // do that because they want to know about (and store and rebroadcast and
        // risk analyze) the dependencies of transactions relevant to them, without
        // having to download the entire memory pool.
        pfrom->PushMessage("notfound", vNotFound);
    }
    
    return true;
}


/* MCHN START */

void CompleteProcessVersion(CNode* pfrom)
{
    if (pfrom->fInbound)
    {
        if (((CNetAddr)pfrom->addr) == (CNetAddr)pfrom->addrFromVersion)
        {
            CMCAddrInfo *mcaddrinfo=addrman.GetMCAddrMan()->Find(pfrom->addrFromVersion,0);
            if(mcaddrinfo)
            {
                mcaddrinfo->ResetLastTry(true);
                mcaddrinfo->SetFlag(MC_AMF_IGNORED,0);
            }
            if(pfrom->addrFromVersion.GetPort() != MC_DEFAULT_NETWORK_PORT)
            {
                addrman.Add(pfrom->addrFromVersion, pfrom->addrFromVersion);
                addrman.Good(pfrom->addrFromVersion);
            }
        }         
        if (addrman.size() < 1000)
        {
            pfrom->PushMessage("getaddr");
            pfrom->fGetAddr = true;
        }        
    }
    
    if (!pfrom->fInbound)
    {
        // Advertise our address
        if (fListen && !IsInitialBlockDownload())
        {
            CAddress addr = GetLocalAddress(&pfrom->addr);
            if (addr.IsRoutable())
            {
                pfrom->PushAddress(addr);
            } else if (IsPeerAddrLocalGood(pfrom)) {
                addr.SetIP(pfrom->addrLocal);
                pfrom->PushAddress(addr);
            }
        }

        // Get recent addresses
        if (pfrom->fOneShot || pfrom->nVersion >= CADDR_TIME_VERSION || addrman.size() < 1000)
        {
            pfrom->PushMessage("getaddr");
            pfrom->fGetAddr = true;
        }
        if(mc_gState->GetSeedNode())
        {
            if(strcmp(mc_gState->GetSeedNode(),pfrom->addr.ToStringIPPort().c_str()) == 0)
            {
                if(fDebug)LogPrint("mchn","Adding seed address %s\n",pfrom->addr.ToStringIPPort().c_str());
                addrman.Add(pfrom->addr, CNetAddr("127.0.0.1"));
                CMCAddrInfo *mcaddrinfo=addrman.GetMCAddrMan()->Find(pfrom->addr,0);
                if(mcaddrinfo)
                {
                    mcaddrinfo->SetFlag(MC_AMF_SOURCE_SEED,1);
                }                
            }
        }
        addrman.Good(pfrom->addr);
    }
    // Relay alerts
    {
        LOCK(cs_mapAlerts);
        BOOST_FOREACH(PAIRTYPE(const uint256, CAlert)& item, mapAlerts)
            item.second.RelayTo(pfrom);
    }

    pfrom->fSuccessfullyConnected = true;
}

/* MCHN END */


bool static ProcessMessage(CNode* pfrom, string strCommand, CDataStream& vRecv, int64_t nTimeReceived)
{
/* MCHN START */
    static int siBlockCount=0;
    static int siBlocksInWindow=10;
    static int siPos=-1;
    static double sdTxLockTime[10];
    static double sdBlockLockTime[10];
    static double sdStartTime[10];
/* MCHN END */

//    RandAddSeedPerfmon();
    if(fDebug)LogPrint("net", "received: %s (%u bytes) peer=%d\n", SanitizeString(strCommand), vRecv.size(), pfrom->id);
    if (mapArgs.count("-dropmessagestest") && (atoi(mapArgs["-dropmessagestest"]) > 0) && (GetRand(atoi(mapArgs["-dropmessagestest"])) == 0) )
    {
        LogPrintf("dropmessagestest DROPPING RECV MESSAGE\n");
        return true;
    }

    
    if (strCommand == "version")
    {
        // Each connection can only send one version message
        if (pfrom->nVersion != 0)
        {
            pfrom->PushMessage("reject", strCommand, REJECT_DUPLICATE, string("Duplicate version message"));
            Misbehaving(pfrom->GetId(), 1);
            return false;
        }

        int64_t nTime;
        CAddress addrMe;
        CAddress addrFrom;
        uint64_t nNonce = 1;
        vRecv >> pfrom->nVersion >> pfrom->nServices >> nTime >> addrMe;
        if (pfrom->nVersion < MIN_PEER_PROTO_VERSION)
        {
            // disconnect from peers older than this proto version
            LogPrintf("peer=%d using obsolete version %i; disconnecting\n", pfrom->id, pfrom->nVersion);
            pfrom->PushMessage("reject", strCommand, REJECT_OBSOLETE,
                               strprintf("Version must be %d or greater", MIN_PEER_PROTO_VERSION));
            pfrom->fDisconnect = true;
            return false;
        }

        if (pfrom->nVersion == 10300)
            pfrom->nVersion = 300;
        if (!vRecv.empty())
            vRecv >> addrFrom >> nNonce;
        if (!vRecv.empty()) {
            vRecv >> LIMITED_STRING(pfrom->strSubVer, 256);
            pfrom->cleanSubVer = SanitizeString(pfrom->strSubVer);
        }
        if (!vRecv.empty())
            vRecv >> pfrom->nStartingHeight;
        if (!vRecv.empty())
            vRecv >> pfrom->fRelayTxes; // set to true after we get the first filter* message
        else
            pfrom->fRelayTxes = true;

        pfrom->nMultiChainServices=0;
        if (!vRecv.empty())
        {
            vRecv >> pfrom->nMultiChainServices;
        }
        
        // Disconnect if we connected to ourself
        if (nNonce == nLocalHostNonce && nNonce > 1)
        {            
            SeenLocalAddr(addrMe);
            LogPrintf("connected to self at %s (me: %s), disconnecting\n", pfrom->addr.ToString(),addrMe.ToStringIPPort().c_str());
            pfrom->fDisconnect = true;
            return true;
        }
        
        pfrom->addrLocal = addrMe;
        if (pfrom->fInbound && addrMe.IsRoutable())
        {
            SeenLocal(addrMe);
        }

        // Be shy and don't send version until we hear
        if (pfrom->fInbound)
            pfrom->PushVersion();

        pfrom->fClient = !(pfrom->nServices & NODE_NETWORK);

        // Potentially mark this peer as a preferred download peer.
        UpdatePreferredDownload(pfrom, State(pfrom->GetId()));

        // Change version
/* MCHN START */
        pfrom->nVersionNonceReceived=nNonce; 
        pfrom->fVerackackReceived=false;
        if(mc_gState->m_NetworkParams->IsProtocolMultichain())
        {            
            if(GetBoolArg("-bitcoinstylehandshake", false))
            {
                LogPrintf("mchn: bitcoin-style-handshake, sending empty verack to peer %d... \n", pfrom->id);
                pfrom->fParameterSetVerified=true;
                pfrom->PushMessage("verack");                
            }
            else
            {
                PushMultiChainVerack(pfrom,false);
            }
        }
        else
        {
            pfrom->fParameterSetVerified=true;
            pfrom->PushMessage("verack");
        }
        
        if(mc_gState->m_NetworkParams->m_Status != MC_PRM_STATUS_VALID)
        {
            return true;                                                   
        }

/* MCHN END */
        
        pfrom->ssSend.SetVersion(min(pfrom->nVersion, PROTOCOL_VERSION));
        
        pfrom->addrFromVersion=addrFrom;
        
        if(pfrom->fParameterSetVerified)
        {
            CompleteProcessVersion(pfrom);
        }
        
        string remoteAddr;
        if (fLogIPs)
            remoteAddr = ", peeraddr=" + pfrom->addr.ToString();

        LogPrintf("receive version message: %s: version %d, blocks=%d, us=%s, peer=%d%s\n",
                  pfrom->cleanSubVer, pfrom->nVersion,
                  pfrom->nStartingHeight, addrMe.ToString(), pfrom->id,
                  remoteAddr);

        AddTimeData(pfrom->addr, nTime);
    }


    else if (pfrom->nVersion == 0)
    {
        // Must have a version message before anything else
        Misbehaving(pfrom->GetId(), 1);
        return false;
    }

    else if (strCommand == "offchain")
    {
        CValidationState state;        
        if(pRelayManager)
        {
            if( (mc_gState->m_NodePausedState & MC_NPS_OFFCHAIN) == 0 )
            {
                bool msg_success=pRelayManager->ProcessRelay(pfrom,vRecv,state,MC_VRA_DEFAULT);
                if(!msg_success)
                {
                    int nDos = 0;
                    if (state.IsInvalid(nDos) && nDos > 0)
                    {
                        Misbehaving(pfrom->GetId(), nDos);
                    }
                }
                pwalletTxsMain->m_ChunkCollector->AdjustKBPerDestination(pfrom,msg_success);
            }
        }
    }

    else if (strCommand == "verack")
    {
        pfrom->SetRecvVersion(min(pfrom->nVersion, PROTOCOL_VERSION));
/* MCHN START */
        if(mc_gState->m_NetworkParams->IsProtocolMultichain())
        {
            if(vRecv.size() == 0)
            {
                if(mc_gState->m_NetworkState == MC_NTS_WAITING_FOR_SEED)
                {
                    pfrom->fDisconnect = true;
                    mc_gState->m_NetworkState = MC_NTS_SEED_NO_PARAMS;
                    return true;                                                                       
                }
                else
                {
                    if(MCP_ANYONE_CAN_CONNECT != 0)
                    {
                        LogPrintf("mchn: bitcoin-style verack received from peer %d, parameter set NOT VERIFIED, connecting... \n", pfrom->id);
                        pfrom->fParameterSetVerified=true;                    
                        CompleteProcessVersion(pfrom);                
                    }
                }
            }
            else
            {
                if(GetBoolArg("-bitcoinstylehandshake", false))
                {
                    LogPrintf("mchn: bitcoin-style-handshake, ignoring verack from peer %d \n", pfrom->id);                    
                }
                else
                {
                    bool disconnect_flag=false;
                    if(!ProcessMultichainVerack(pfrom,vRecv,false,&disconnect_flag))
                    {
                        LogPrintf("mchn: Invalid verack message from peer=%d, disconnecting\n", pfrom->id);
                        pfrom->fDisconnect = true;
                        mc_gState->m_NetworkState = MC_NTS_SEED_NO_PARAMS;
                        return true;                                                   
                    }
                    else
                    {
                        if(!pfrom->fDisconnect)
                        {
                            pfrom->fDisconnect = !PushMultiChainVerack(pfrom,true);
                        }
                        else
                        {
                            mc_gState->m_NetworkState = MC_NTS_SEED_READY;
                            return true;                                                   
                        }
                    }
                    if(pfrom->fDisconnect)
                    {
                        mc_gState->m_NetworkState = MC_NTS_SEED_READY;
                        return false;                                
                    }
                }
            }
        }
    }

    else if (strCommand == "verackack")
    {
        if(mc_gState->m_NetworkParams->IsProtocolMultichain())
        {
            bool disconnect_flag=true;
            if(!ProcessMultichainVerack(pfrom,vRecv,true,&disconnect_flag))
            {
                pfrom->fDisconnect |= disconnect_flag;
                if(pfrom->fDisconnect)
                {
                    LogPrintf("mchn: Invalid verackack message from peer=%d, disconnecting\n", pfrom->id);
                }
                else
                {
                    LogPrintf("mchn: Parameter set from peer=%d verified\n", pfrom->id);
                    pfrom->fParameterSetVerified=true;                    
                    CompleteProcessVersion(pfrom);
                }
            }
            else
            {
                pfrom->fDisconnect |= !PushMultiChainVerack(pfrom,false);       // MCHN |= to avoid setting fDisconnect to false
            }
            if(pfrom->fDisconnect)
            {
                mc_gState->m_NetworkState = MC_NTS_SEED_READY;
                return false;                                
            }
        }
        
/* MCHN END */
    }


    else if (strCommand == "addr")
    {
        vector<CAddress> vAddr;
        vRecv >> vAddr;

        
        // Don't want addr from older versions unless seeding
        if (pfrom->nVersion < CADDR_TIME_VERSION && addrman.size() > 1000)
            return true;
        if (vAddr.size() > 1000)
        {
            Misbehaving(pfrom->GetId(), 20);
            return error("message addr size() = %u", vAddr.size());
        }

        // Store the new addresses
        vector<CAddress> vAddrOk;
        int64_t nNow = GetAdjustedTime();
        int64_t nSince = nNow - 10 * 60;
        if(fDebug)LogPrint("mchn","mchn: received addr: %d\n",vAddr.size());
        BOOST_FOREACH(CAddress& addr, vAddr)
        {
            boost::this_thread::interruption_point();

            if (addr.nTime <= 100000000 || addr.nTime > nNow + 10 * 60)
                addr.nTime = nNow - 5 * 24 * 60 * 60;
            pfrom->AddAddressKnown(addr);
            bool fReachable = IsReachable(addr);
            if (addr.nTime > nSince && !pfrom->fGetAddr && vAddr.size() <= 10 && addr.IsRoutable())
            {
                // Relay to a limited number of other nodes
                {
                    LOCK(cs_vNodes);
                    // Use deterministic randomness to send to the same nodes for 24 hours
                    // at a time so the setAddrKnowns of the chosen nodes prevent repeats
                    static uint256 hashSalt;
                    if (hashSalt == 0)
                        hashSalt = GetRandHash();
                    uint64_t hashAddr = addr.GetHash();
                    uint256 hashRand = hashSalt ^ (hashAddr<<32) ^ ((GetTime()+hashAddr)/(24*60*60));
                    hashRand = Hash(BEGIN(hashRand), END(hashRand));
                    multimap<uint256, CNode*> mapMix;
                    BOOST_FOREACH(CNode* pnode, vNodes)
                    {
                        if (pnode->nVersion < CADDR_TIME_VERSION)
                            continue;
                        unsigned int nPointer;
                        memcpy(&nPointer, &pnode, sizeof(nPointer));
                        uint256 hashKey = hashRand ^ nPointer;
                        hashKey = Hash(BEGIN(hashKey), END(hashKey));
                        mapMix.insert(make_pair(hashKey, pnode));
                    }
                    int nRelayNodes = fReachable ? 2 : 1; // limited relaying of addresses outside our network(s)
                    for (multimap<uint256, CNode*>::iterator mi = mapMix.begin(); mi != mapMix.end() && nRelayNodes-- > 0; ++mi)
                        ((*mi).second)->PushAddress(addr);
                }
            }
            // Do not store addresses outside our network
            if (fReachable)
            {
                if(fLogIPs)
                 if(fDebug)LogPrint("mchn","mchn: Got new address %s\n",addr.ToStringIPPort().c_str());
                vAddrOk.push_back(addr);
            }
        }
        addrman.Add(vAddrOk, pfrom->addr, 2 * 60 * 60);
        if (vAddr.size() < 1000)
            pfrom->fGetAddr = false;
        if (pfrom->fOneShot)
            pfrom->fDisconnect = true;
    }


    else if (strCommand == "inv")
    {
        vector<CInv> vInv;
        vRecv >> vInv;
        if (vInv.size() > MAX_INV_SZ)
        {
            Misbehaving(pfrom->GetId(), 20);
            return error("message inv size() = %u", vInv.size());
        }
        
        LOCK(cs_main);

        std::vector<CInv> vToFetch;
        std::vector<uint256> vTxsToFetch;

        for (unsigned int nInv = 0; nInv < vInv.size(); nInv++)
        {
            const CInv &inv = vInv[nInv];

            boost::this_thread::interruption_point();
            pfrom->AddInventoryKnown(inv);

            bool fAlreadyHave = AlreadyHave(inv);
            if(fDebug)LogPrint("net", "got inv: %s  %s peer=%d\n", inv.ToString(), fAlreadyHave ? "have" : "new", pfrom->id);

            if (!fAlreadyHave && !fImporting && !fReindex && inv.type != MSG_BLOCK)
            {
                pfrom->AskFor(inv);
                if(inv.type == MSG_TX)
                {
                    vTxsToFetch.push_back(inv.hash);
                }
            }
            
            if (inv.type == MSG_BLOCK) {
                if (!fAlreadyHave)
                {
                    if(!pfrom->fReadyForTxInv)
                    {
                        LogPrintf("mchn: New unknown header from node %d on block %d - start pushing txs\n",pfrom->id,mc_gState->m_Permissions->m_Block);
                        PushMempoolToInv(pfrom);
                    }
                }
                UpdateBlockAvailability(pfrom->GetId(), inv.hash);
                if (!fAlreadyHave && !fImporting && !fReindex && !mapBlocksInFlight.count(inv.hash)) {
                    // First request the headers preceeding the announced block. In the normal fully-synced
                    // case where a new block is announced that succeeds the current tip (no reorganization),
                    // there are no such headers.
                    // Secondly, and only when we are close to being synced, we request the announced block directly,
                    // to avoid an extra round-trip. Note that we must *first* ask for the headers, so by the
                    // time the block arrives, the header chain leading up to it is already validated. Not
                    // doing this will result in the received block being rejected as an orphan in case it is
                    // not a direct successor.
                    pfrom->PushMessage("getheaders", chainActive.GetLocator(pindexBestHeader), inv.hash);
                    CNodeState *nodestate = State(pfrom->GetId());
                    if(!MultichainNode_IgnoreIncoming(pfrom))
                    {
                        if (chainActive.Tip()->GetBlockTime() > GetAdjustedTime() - Params().TargetSpacing() * 20 &&
                            nodestate->nBlocksInFlight < MAX_BLOCKS_IN_TRANSIT_PER_PEER) {
                            vToFetch.push_back(inv);                            
                            // Mark block as in flight already, even though the actual "getdata" message only goes out
                            // later (within the same cs_main lock, though).
                            MarkBlockAsInFlight(pfrom->GetId(), inv.hash);
                            if(fDebug)LogPrint("mcblockperf", "mchn-block-perf: Requesting block (inv) %s peer=%d\n", inv.hash.ToString().c_str(), pfrom->id);
                            
                        }
                    }
                    if(fDebug)LogPrint("net", "getheaders (%d) %s to peer=%d\n", pindexBestHeader->nHeight, inv.hash.ToString(), pfrom->id);
                }
            }

            // Track requests for our stuff
            g_signals.Inventory(inv.hash);

            if (pfrom->nSendSize > (SendBufferSize() * 2)) {
                Misbehaving(pfrom->GetId(), 50);
                return error("send buffer size() = %u", pfrom->nSendSize);
            }
        }

        if (!vToFetch.empty() && !MultichainNode_IgnoreIncoming(pfrom))      // MCHN
            pfrom->PushMessage("getdata", vToFetch);
        
        pfrom->AddTxsInFlight(vTxsToFetch);
    }


    else if (strCommand == "getdata")
    {
        vector<CInv> vInv;
        vRecv >> vInv;
        if (vInv.size() > MAX_INV_SZ)
        {
            Misbehaving(pfrom->GetId(), 20);
            return error("message getdata size() = %u", vInv.size());
        }

        if (fDebug || (vInv.size() != 1))
            if(fDebug)LogPrint("net", "received getdata (%u invsz) peer=%d\n", vInv.size(), pfrom->id);

        if ((fDebug && vInv.size() > 0) || (vInv.size() == 1))
            if(fDebug)LogPrint("net", "received getdata for: %s peer=%d\n", vInv[0].ToString(), pfrom->id);

        if( nMessageHandlerThreads & MC_MHT_GETDATA )
        {
            LOCK(pfrom->cs_vRecvGetData);
            pfrom->vRecvGetDataBuf.insert(pfrom->vRecvGetDataBuf.end(), vInv.begin(), vInv.end());            
        }
        else
        {        
            pfrom->vRecvGetData.insert(pfrom->vRecvGetData.end(), vInv.begin(), vInv.end());
            ProcessGetData(pfrom);
        }
    }


    else if (strCommand == "getblocks")
    {
        CBlockLocator locator;
        uint256 hashStop;
        vRecv >> locator >> hashStop;

//        LOCK(cs_main);
        mc_gState->ChainLock();
        // Find the last block the caller has in the main chain
        CBlockIndex* pindex = FindForkInGlobalIndex(chainActive, locator);

        // Send the rest of the chain
        if (pindex)
            pindex = chainActive.Next(pindex);
        int nLimit = 500;
        if(fDebug)LogPrint("net", "getblocks %d to %s limit %d from peer=%d\n", (pindex ? pindex->nHeight : -1), hashStop==uint256(0) ? "end" : hashStop.ToString(), nLimit, pfrom->id);
        for (; pindex; pindex = chainActive.Next(pindex))
        {
            if (pindex->GetBlockHash() == hashStop)
            {
                if(fDebug)LogPrint("net", "  getblocks stopping at %d %s\n", pindex->nHeight, pindex->GetBlockHash().ToString());
                break;
            }
            pfrom->PushInventory(CInv(MSG_BLOCK, pindex->GetBlockHash()));
            if (--nLimit <= 0)
            {
                // When this block is requested, we'll send an inv that'll make them
                // getblocks the next batch of inventory.
                if(fDebug)LogPrint("net", "  getblocks stopping at limit %d %s\n", pindex->nHeight, pindex->GetBlockHash().ToString());
                pfrom->hashContinue = pindex->GetBlockHash();
                break;
            }
        }
        mc_gState->ChainUnLock();
    }


    else if (strCommand == "getheaders")
    {
        CBlockLocator locator;
        uint256 hashStop;
        vRecv >> locator >> hashStop;

//        LOCK(cs_main);
        if (IsInitialBlockDownloadNoCheckpoints())
            return true;

          
        mc_gState->ChainLock();
        CBlockIndex* pindex = NULL;
        if (locator.IsNull())
        {
            // If locator is null, return the hashStop block
            BlockMap::iterator mi = mapBlockIndex.find(hashStop);
            if (mi == mapBlockIndex.end())
            {
                mc_gState->ChainUnLock();
                return true;
            }
            pindex = (*mi).second;
        }
        else
        {
            // Find the last block the caller has in the main chain
            pindex = FindForkInGlobalIndex(chainActive, locator);
            if (pindex)
                pindex = chainActive.Next(pindex);
        }

        // we must use CBlocks, as CBlockHeaders won't include the 0x00 nTx count at the end
        vector<CBlock> vHeaders;
        int nLimit = MAX_HEADERS_RESULTS;
        if(fDebug)LogPrint("net", "getheaders %d to %s from peer=%d\n", (pindex ? pindex->nHeight : -1), hashStop.ToString(), pfrom->id);
        for (; pindex; pindex = chainActive.Next(pindex))
        {
            vHeaders.push_back(pindex->GetBlockHeader());
            if (--nLimit <= 0 || pindex->GetBlockHash() == hashStop)
                break;
        }
        mc_gState->ChainUnLock();
        pfrom->PushMessage("headers", vHeaders);
    }


    else if (strCommand == "tx" && MultichainNode_AcceptData(pfrom))            // MCHN
    {
/* MCHN START */        
        if(!MultichainNode_IgnoreIncoming(pfrom) && !pfrom->fDisconnect)
        {
/* MCHN END */        
        vector<uint256> vWorkQueue;
        vector<uint256> vEraseQueue;
        CTransaction tx;
        vRecv >> tx;

        CInv inv(MSG_TX, tx.GetHash());
        pfrom->AddInventoryKnown(inv);
        
        {
        LOCK(cs_main);

        if( !fAcceptOnlyRequestedTxs  || pfrom->IsTxInFlight(inv.hash) )
        {
            
        double start_time=mc_TimeNowAsDouble();
        
        bool fMissingInputs = false;
        CValidationState state;

        mapAlreadyAskedFor.erase(inv);
        
/* MCHN START */        
        CPubKey pubkey;            
        uint32_t fCanMine=((pwalletMain != NULL) && pwalletMain->GetKeyFromAddressBook(pubkey,MC_PTP_MINE)) ? MC_PTP_MINE : 0;
/* MCHN END */        
        
        if (AcceptToMemoryPool(mempool, state, tx, true, &fMissingInputs))
        {
            pwalletTxsMain->WRPWriteLock();        
            pwalletTxsMain->WRPSync(0);
            pwalletTxsMain->WRPWriteUnLock();
            mempool.check(pcoinsTip);
            RelayTransaction(tx);
            vWorkQueue.push_back(inv.hash);

            if(fDebug)LogPrint("mempool", "AcceptToMemoryPool: peer=%d %s : accepted %s (poolsz %u)\n",
                pfrom->id, pfrom->cleanSubVer,
                tx.GetHash().ToString(),
                mempool.mapTx.size());

            // Recursively process any orphan transactions that depended on this one
            set<NodeId> setMisbehaving;
            for (unsigned int i = 0; i < vWorkQueue.size(); i++)
            {
                map<uint256, set<uint256> >::iterator itByPrev = mapOrphanTransactionsByPrev.find(vWorkQueue[i]);
                if (itByPrev == mapOrphanTransactionsByPrev.end())
                    continue;
                for (set<uint256>::iterator mi = itByPrev->second.begin();
                     mi != itByPrev->second.end();
                     ++mi)
                {
                    const uint256& orphanHash = *mi;
                    const CTransaction& orphanTx = mapOrphanTransactions[orphanHash].tx;
                    NodeId fromPeer = mapOrphanTransactions[orphanHash].fromPeer;
                    bool fMissingInputs2 = false;
                    // Use a dummy CValidationState so someone can't setup nodes to counter-DoS based on orphan
                    // resolution (that is, feeding people an invalid transaction based on LegitTxX in order to get
                    // anyone relaying LegitTxX banned)
                    CValidationState stateDummy;


                    if (setMisbehaving.count(fromPeer))
                        continue;
                    if (AcceptToMemoryPool(mempool, stateDummy, orphanTx, true, &fMissingInputs2))
                    {
                        pwalletTxsMain->WRPWriteLock();        
                        pwalletTxsMain->WRPSync(0);
                        pwalletTxsMain->WRPWriteUnLock();
                        if(fDebug)LogPrint("mempool", "   accepted orphan tx %s\n", orphanHash.ToString());
                        RelayTransaction(orphanTx);
                        vWorkQueue.push_back(orphanHash);
                        vEraseQueue.push_back(orphanHash);
                    }
                    else if (!fMissingInputs2)
                    {
                        int nDos = 0;
                        if (stateDummy.IsInvalid(nDos) && nDos > 0)
                        {
                            // Punish peer that gave us an invalid orphan tx
                            Misbehaving(fromPeer, nDos);
                            setMisbehaving.insert(fromPeer);
                            if(fDebug)LogPrint("mempool", "   invalid orphan tx %s\n", orphanHash.ToString());
                        }
                        // Has inputs but not accepted to mempool
                        // Probably non-standard or insufficient fee/priority
                        if(fDebug)LogPrint("mempool", "   removed orphan tx %s\n", orphanHash.ToString());
                        vEraseQueue.push_back(orphanHash);
                    }
                    mempool.check(pcoinsTip);
                }
            }

            BOOST_FOREACH(uint256 hash, vEraseQueue)
                EraseOrphanTx(hash);
            
/* MCHN START */            
            if(pwalletMain)
            {
                if(fCanMine)
                {
                    if(!pwalletMain->GetKeyFromAddressBook(pubkey,MC_PTP_MINE))
                    {
                        if(fDebug)LogPrint("mchn","mchn: Wallet lost mine permission on tx: %s (height %d) - message, reactivating best chain\n",
                                tx.GetHash().ToString().c_str(), chainActive.Tip()->nHeight);
                        if (!ActivateBestChain(state, NULL))
                            return error("%s : ActivateBestChain failed", __func__);                    
                    }
                }
            }
            
        
            pEF->LIC_VerifyLicenses(-1);            
/* MCHN END */            
            
            
        }
        else if (fMissingInputs)
        {
/* MCHN START */            
            bool found_in_oldblocks=false;
            for(int d=0;d<MC_TXSET_BLOCKS;d++)
            {
                if(!found_in_oldblocks)
                {
                    if(mc_gState->m_Permissions->m_Block>d)
                    {
                        if(setBlockTransactions[(mc_gState->m_Permissions->m_Block-d) % MC_TXSET_BLOCKS].count(tx.GetHash()))
                        {
                            found_in_oldblocks=true;
                        }
                    }
                }
            }
            
            if(!found_in_oldblocks)
            {
                bool added=AddOrphanTx(tx, pfrom->GetId());
                if(!added)
                {
                    if(OrphanHandlerVersion == 1)
                    {
                        LogPrintf("mchn: Couldn't add tx from peer %d to orphan pool, disconnecting.\n",pfrom->id);        
                        pfrom->fDisconnect=true;                    
                    }
                }
                // DoS prevention: do not allow mapOrphanTransactions to grow unbounded
                unsigned int nMaxOrphanTx = (unsigned int)std::max((int64_t)0, GetArg("-maxorphantx", DEFAULT_MAX_ORPHAN_TRANSACTIONS));
                unsigned int nEvicted = LimitOrphanTxSize(nMaxOrphanTx);
                if (nEvicted > 0)
                    if(fDebug)LogPrint("mempool", "mapOrphan overflow, removed %u tx\n", nEvicted);
            }
/* MCHN END */            
        } else if (pfrom->fWhitelisted) {
            // Always relay transactions received from whitelisted peers, even
            // if they are already in the mempool (allowing the node to function
            // as a gateway for nodes hidden behind it).
            RelayTransaction(tx);
        }
        int nDoS = 0;
        if (state.IsInvalid(nDoS))
        {
            if(fDebug)LogPrint("mempool", "%s from peer=%d %s was not accepted into the memory pool: %s\n", tx.GetHash().ToString(),
                pfrom->id, pfrom->cleanSubVer,
                state.GetRejectReason());
            pfrom->PushMessage("reject", strCommand, state.GetRejectCode(),
                               state.GetRejectReason().substr(0, MAX_REJECT_MESSAGE_LENGTH), inv.hash);
            if (nDoS > 0)
                Misbehaving(pfrom->GetId(), nDoS);
        }
        
        double end_time=mc_TimeNowAsDouble();
        if(siPos>=0)
        {
            sdTxLockTime[siPos]+=end_time-start_time;
        }
        
        }
        pfrom->RemoveTxInFlight(inv.hash);
        }
        
        int tx_throttling_delay=TxThrottlingDelay(false);
        if(tx_throttling_delay > 0)
        {
            MilliSleep(tx_throttling_delay);
        }
    
        
/* MCHN START */            
        }        
/* MCHN END */            
    }


    else if (strCommand == "headers" && !fImporting && !fReindex) // Ignore headers received while importing
    {
        std::vector<CBlockHeader> headers;

        // Bypass the normal CBlock deserialization, as we don't want to risk deserializing 2000 full blocks.
        unsigned int nCount = ReadCompactSize(vRecv);
        if (nCount > MAX_HEADERS_RESULTS) {
            Misbehaving(pfrom->GetId(), 20);
            return error("headers message size = %u", nCount);
        }
        headers.resize(nCount);
        for (unsigned int n = 0; n < nCount; n++) {
            vRecv >> headers[n];
            ReadCompactSize(vRecv); // ignore tx count; assume it is 0.
        }

        LOCK(cs_main);

        if (nCount == 0) {
            // Nothing interesting. Stop asking this peers for more headers.
            if(chainActive.Height())
            {
                pfrom->fEmptyHeaders=true;
            }
            return true;
        }

        CBlockIndex *pindexLast = NULL;
        int first_height=-1;
        BOOST_FOREACH(const CBlockHeader& header, headers) {
            CValidationState state;
            if (pindexLast != NULL && header.hashPrevBlock != pindexLast->GetBlockHash()) {
                Misbehaving(pfrom->GetId(), 20);
                return error("non-continuous headers sequence");
            }
            if (!AcceptBlockHeader(header, state, &pindexLast, pfrom->GetId(),pindexLast)) {
                int nDoS;
                if (state.IsInvalid(nDoS)) {
                    if (nDoS > 0)
                        Misbehaving(pfrom->GetId(), nDoS);
                    return error("invalid header received");
                }
            }
            if(first_height < 0)
            {
                first_height=pindexLast->nHeight;
            }
        }
        if (pindexLast)
        {
            if(fDebug)LogPrint("mcblock","mchn-block: Received headers: %d-%d,  peer=%d\n",first_height,pindexLast->nHeight,pfrom->id);            
            UpdateBlockAvailability(pfrom->GetId(), pindexLast->GetBlockHash());
        }
        if (nCount == MAX_HEADERS_RESULTS && pindexLast) {
            // Headers message had its maximum size; the peer may have more headers.
            // TODO: optimize: if pindexLast is an ancestor of chainActive.Tip or pindexBestHeader, continue
            // from there instead.
            if(fDebug)LogPrint("net", "more getheaders (%d) to end to peer=%d (startheight:%d)\n", pindexLast->nHeight, pfrom->id, pfrom->nStartingHeight);
            pfrom->PushMessage("getheaders", chainActive.GetLocator(pindexLast), uint256(0));
        }
    }
                                                                                // MCHN
    else if (strCommand == "block" && !fImporting && !fReindex && MultichainNode_AcceptData(pfrom)) // Ignore blocks received while importing
    {
        CBlock block;
        vRecv >> block;
/* MCHN START */        
        CNode* seed_node;
        bool seed_could_connect=false;
        {
            LOCK(cs_main);
            seed_node=(CNode*)(mc_gState->m_pSeedNode);
            if(seed_node)
            {
                seed_could_connect=mc_gState->m_Permissions->CanConnect(NULL,seed_node->kAddrRemote.begin());
            }            
        }
/* MCHN END */        

        CInv inv(MSG_BLOCK, block.GetHash());

/* MCHN START */        
        if(!MultichainNode_IgnoreIncoming(pfrom))
        {
/* MCHN END */        
            
        if(fDebug)LogPrint("net", "received block %s peer=%d\n", inv.hash.ToString(), pfrom->id);
        if(fDebug)LogPrint("mcblock","mchn-block: Received block:   %s,  peer=%d\n",inv.hash.ToString().c_str(),pfrom->id);

        pfrom->AddInventoryKnown(inv);
        
        double start_time=mc_TimeNowAsDouble();

        CValidationState state;       
        ProcessNewBlock(state, pfrom, &block);
        int nDoS;
        if (state.IsInvalid(nDoS)) {
            pfrom->PushMessage("reject", strCommand, state.GetRejectCode(),
                               state.GetRejectReason().substr(0, MAX_REJECT_MESSAGE_LENGTH), inv.hash);
            if (nDoS > 0) {
                LOCK(cs_main);
                Misbehaving(pfrom->GetId(), nDoS);
            }
        }

/* MCHN START */        
        
        {
            LOCK(cs_main);
            seed_node=(CNode*)(mc_gState->m_pSeedNode);
            if(seed_node)
            {
                LOCK(cs_main);
                if(seed_could_connect && !mc_gState->m_Permissions->CanConnect(NULL,seed_node->kAddrRemote.begin()))
                {
                    LOCK(cs_vNodes);
                    if(vNodes.size() > 1)
                    {
                        LogPrintf("mchn: Seed node lost connect permission on block %d\n",mc_gState->m_Permissions->m_Block);
                        mc_RemoveFile(mc_gState->m_NetworkParams->Name(),"seed",".dat",MC_FOM_RELATIVE_TO_DATADIR);
                        mc_gState->m_pSeedNode=NULL;
                    }
                }
            }
        }
        
        double end_time=mc_TimeNowAsDouble();
        if(siPos>=0)
        {
            sdBlockLockTime[siPos]+=end_time-start_time;
        }
        
        if(siPos>=0)
        {
            int block_count=siBlockCount;
            if(block_count>siBlocksInWindow)
            {
                block_count=siBlocksInWindow;                
            }
            double tx_lock_time=0.;
            double block_lock_time=0.;
            double block_start_time=0;
            for(int i=0;i<block_count;i++)
            {
                int pos=siPos-i;
                if(pos<0)
                {
                    pos+=siBlocksInWindow;
                }
                tx_lock_time+=sdTxLockTime[pos];
                block_lock_time+=sdBlockLockTime[pos];
                block_start_time=sdStartTime[pos];
            }
            double block_total_time=end_time-block_start_time;
             
            if(fDebug)LogPrint("mchn","mchn-P: Block %4d (%4d); Txs: %6d; T: %8.3f; TX: %8.3f (%8.3f%%); BL: %8.3f (%8.3f%%);\n",chainActive.Tip()->nHeight,siBlockCount,(int)block.vtx.size(),
                    block_total_time/block_count,
                    tx_lock_time/block_count,100.*tx_lock_time/block_total_time,block_lock_time/block_count,100.*block_lock_time/block_total_time);

        }
        
        siPos=(siPos+1)%siBlocksInWindow;
        siBlockCount++;
        sdTxLockTime[siPos]=0.;
        sdBlockLockTime[siPos]=0.;
        sdStartTime[siPos]=end_time;
/* MCHN END */        
/* MCHN START */        
        }
        else
        {
            if(fDebug)LogPrint("net", "ignored block %s peer=%d\n", inv.hash.ToString(), pfrom->id);
            pfrom->AskFor(inv);
        }
/* MCHN END */        
    }


    else if (strCommand == "getaddr")
    {
        pfrom->vAddrToSend.clear();
        vector<CAddress> vAddr = addrman.GetAddr();
        BOOST_FOREACH(const CAddress &addr, vAddr)
            pfrom->PushAddress(addr);
/* MCHN START */        
        if(fDebug)LogPrint("mchn","mchn: Sent %d known addresses\n",vAddr.size());
/* MCHN END */        
    }


    else if (strCommand == "mempool" && MultichainNode_SendInv(pfrom))          // MCHN
    {
//        LOCK2(cs_main, pfrom->cs_filter);
        
        bool pushed=false;
        if(!pfrom->fReadyForTxInv)
        {
            LogPrintf("mchn: mempool request from node %d on block %d - start pushing txs\n",pfrom->id,mc_gState->m_Permissions->m_Block);           
        }
        pushed=PushMempoolToInv(pfrom);
        
        if(!pushed)
        {
            LOCK(pfrom->cs_filter);

            int push_count=0;
            std::vector<uint256> vtxid;
            mempool.queryHashes(vtxid);
            vector<CInv> vInv;
            BOOST_FOREACH(uint256& hash, vtxid) {
                CInv inv(MSG_TX, hash);
                CTransaction tx;
                bool fInMemPool = mempool.lookup(hash, tx);
                if (!fInMemPool) continue; // another thread removed since queryHashes, maybe...
                if ((pfrom->pfilter && pfrom->pfilter->IsRelevantAndUpdate(tx)) ||
                   (!pfrom->pfilter))
                    vInv.push_back(inv);
                if (vInv.size() == MAX_INV_SZ) {
                    push_count+=vInv.size();
                    pfrom->PushMessage("inv", vInv);
                    vInv.clear();
                }
            }
            if (vInv.size() > 0)
            {
                pfrom->PushMessage("inv", vInv);
                push_count+=vInv.size();
            }
            
            LogPrintf("mchn: mempool request from node %d on block %d - start pushing txs\n",pfrom->id,mc_gState->m_Permissions->m_Block);
            LogPrintf("mchn: pushed %d inv records directly\n",push_count);
        }
    }


    else if (strCommand == "ping")
    {
        if (pfrom->nVersion > BIP0031_VERSION)
        {
            uint64_t nonce = 0;
            vRecv >> nonce;
            // Echo the message back with the nonce. This allows for two useful features:
            //
            // 1) A remote node can quickly check if the connection is operational
            // 2) Remote nodes can measure the latency of the network thread. If this node
            //    is overloaded it won't respond to pings quickly and the remote node can
            //    avoid sending us more work, like chain download requests.
            //
            // The nonce stops the remote getting confused between different pings: without
            // it, if the remote node sends a ping once per second and this node takes 5
            // seconds to respond to each, the 5th ping the remote sends would appear to
            // return very quickly.
            pfrom->PushMessage("pong", nonce);
        }
    }


    else if (strCommand == "pong")
    {
        int64_t pingUsecEnd = nTimeReceived;
        uint64_t nonce = 0;
        size_t nAvail = vRecv.in_avail();
        bool bPingFinished = false;
        std::string sProblem;

        if (nAvail >= sizeof(nonce)) {
            vRecv >> nonce;

            // Only process pong message if there is an outstanding ping (old ping without nonce should never pong)
            if (pfrom->nPingNonceSent != 0) {
                if (nonce == pfrom->nPingNonceSent) {
                    // Matching pong received, this ping is no longer outstanding
                    bPingFinished = true;
                    int64_t pingUsecTime = pingUsecEnd - pfrom->nPingUsecStart;
                    if (pingUsecTime > 0) {
                        // Successful ping time measurement, replace previous
                        pfrom->nPingUsecTime = pingUsecTime;
                    } else {
                        // This should never happen
                        sProblem = "Timing mishap";
                    }
                } else {
                    // Nonce mismatches are normal when pings are overlapping
                    sProblem = "Nonce mismatch";
                    if (nonce == 0) {
                        // This is most likely a bug in another implementation somewhere, cancel this ping
                        bPingFinished = true;
                        sProblem = "Nonce zero";
                    }
                }
            } else {
                sProblem = "Unsolicited pong without ping";
            }
        } else {
            // This is most likely a bug in another implementation somewhere, cancel this ping
            bPingFinished = true;
            sProblem = "Short payload";
        }

        if (!(sProblem.empty())) {
            if(fDebug)LogPrint("net", "pong peer=%d %s: %s, %x expected, %x received, %u bytes\n",
                pfrom->id,
                pfrom->cleanSubVer,
                sProblem,
                pfrom->nPingNonceSent,
                nonce,
                nAvail);
        }
        if (bPingFinished) {
            pfrom->nPingNonceSent = 0;
        }
    }


    else if (strCommand == "alert")
    {
        CAlert alert;
        vRecv >> alert;

        uint256 alertHash = alert.GetHash();
        if (pfrom->setKnown.count(alertHash) == 0)
        {
            if (alert.ProcessAlert())
            {
                // Relay
                pfrom->setKnown.insert(alertHash);
                {
                    LOCK(cs_vNodes);
                    BOOST_FOREACH(CNode* pnode, vNodes)
                        alert.RelayTo(pnode);
                }
            }
            else {
                // Small DoS penalty so peers that send us lots of
                // duplicate/expired/invalid-signature/whatever alerts
                // eventually get banned.
                // This isn't a Misbehaving(100) (immediate ban) because the
                // peer might be an older or different implementation with
                // a different signature key, etc.
                Misbehaving(pfrom->GetId(), 10);
            }
        }
    }


    else if (strCommand == "filterload")
    {
        CBloomFilter filter;
        vRecv >> filter;

        if (!filter.IsWithinSizeConstraints())
            // There is no excuse for sending a too-large filter
            Misbehaving(pfrom->GetId(), 100);
        else
        {
            LOCK(pfrom->cs_filter);
            delete pfrom->pfilter;
            pfrom->pfilter = new CBloomFilter(filter);
            pfrom->pfilter->UpdateEmptyFull();
        }
        pfrom->fRelayTxes = true;
    }


    else if (strCommand == "filteradd")
    {
        vector<unsigned char> vData;
        vRecv >> vData;

        // Nodes must NEVER send a data item > 520 bytes (the max size for a script data object,
        // and thus, the maximum size any matched object can have) in a filteradd message
        if (vData.size() > MAX_SCRIPT_ELEMENT_SIZE)
        {
            Misbehaving(pfrom->GetId(), 100);
        } else {
            LOCK(pfrom->cs_filter);
            if (pfrom->pfilter)
                pfrom->pfilter->insert(vData);
            else
                Misbehaving(pfrom->GetId(), 100);
        }
    }


    else if (strCommand == "filterclear")
    {
        LOCK(pfrom->cs_filter);
        delete pfrom->pfilter;
        pfrom->pfilter = new CBloomFilter();
        pfrom->fRelayTxes = true;
    }


    else if (strCommand == "reject")
    {
        if (fDebug) {
            try {
                string strMsg; unsigned char ccode; string strReason;
                vRecv >> LIMITED_STRING(strMsg, CMessageHeader::COMMAND_SIZE) >> ccode >> LIMITED_STRING(strReason, MAX_REJECT_MESSAGE_LENGTH);

                ostringstream ss;
                ss << strMsg << " code " << itostr(ccode) << ": " << strReason;

                if (strMsg == "block" || strMsg == "tx")
                {
                    uint256 hash;
                    vRecv >> hash;
                    ss << ": hash " << hash.ToString();
                }
                if(fDebug)LogPrint("net", "Reject %s\n", SanitizeString(ss.str()));
            } catch (std::ios_base::failure& e) {
                // Avoid feedback loops by preventing reject messages from triggering a new reject message.
                if(fDebug)LogPrint("net", "Unparseable reject message received\n");
            }
        }
    }

    else
    {
        // Ignore unknown commands for extensibility
        if(fDebug)LogPrint("net", "Unknown command \"%s\" from peer=%d\n", SanitizeString(strCommand), pfrom->id);
    }


    // Update the last seen time for this node's address
    if (pfrom->fNetworkNode)
        if (strCommand == "version" || strCommand == "addr" || strCommand == "inv" || strCommand == "getdata" || strCommand == "ping")
            AddressCurrentlyConnected(pfrom->addr);


    return true;
}

bool SplitInvMessage(CNode* pfrom, CNetMessage&  InvMessage)
{    
    CMessageHeader hdr("inv", 0);
    
    vector <CInv> vInv; 
    InvMessage.vRecv >> vInv;
    
    vector <CInv> vInvTxs; 
    vector <CInv> vInvBlocks; 
    for (unsigned int nInv = 0; nInv < vInv.size(); nInv++)
    {
        const CInv &inv = vInv[nInv];
        if(inv.type == MSG_TX)
        {
            vInvTxs.push_back(inv);
        }
        else
        {
            vInvBlocks.push_back(inv);
        }
    }
     
    if(vInvTxs.size())
    {
        CNetMessage msg(SER_NETWORK, PROTOCOL_VERSION);
        msg.readHeader((const char*)&hdr,sizeof(CMessageHeader));
        msg.vRecv << vInvTxs;
        
        LOCK(pfrom->cs_vRecvTxDataMsg);
        pfrom->vRecvTxDataMsg.push_back(msg);        
    }
    
    if(vInvBlocks.size())
    {
        CNetMessage msg(SER_NETWORK, PROTOCOL_VERSION);
        msg.readHeader((const char*)&hdr,sizeof(CMessageHeader));
        msg.vRecv << vInvBlocks;
        
        LOCK(pfrom->cs_vRecvDataMsg);
        pfrom->vRecvDataMsg.push_back(msg);        
    }
    
    return true;
}

// requires LOCK(cs_vRecvMsg)
bool ProcessMessages(CNode* pfrom)
{
    //if (fDebug)
    //    LogPrintf("ProcessMessages(%u messages)\n", pfrom->vRecvMsg.size());

    //
    // Message format
    //  (4) message start
    //  (12) command
    //  (4) size
    //  (4) checksum
    //  (x) data
    //
    bool fOk = true;

    if((nMessageHandlerThreads & MC_MHT_GETDATA) == 0)
    {
        if (!pfrom->vRecvGetData.empty())
            ProcessGetData(pfrom);

        // this maintains the order of responses
        if (!pfrom->vRecvGetData.empty()) return fOk;
    }

/* MCHN START */    
    {        
//        LOCK(cs_main);
        
        if(!pfrom->fDisconnect)
        {
            if(MultichainNode_DisconnectRemote(pfrom))
            {            
                    pfrom->fDisconnect=true;
                    LogPrintf("mchn: Address %s lost connect permission on peer=%d, diconnecting...\n",CBitcoinAddress(pfrom->kAddrRemote).ToString().c_str(), pfrom->id);            

            }
            if(!pfrom->fDisconnect)
            {
                if(pfrom->fCanConnectLocal && MultichainNode_DisconnectLocal(pfrom))
                {
                        pfrom->fDisconnect=true;
                        LogPrintf("mchn: Local address %s lost connect permission. disconnecting peer %d...\n",CBitcoinAddress(pfrom->kAddrLocal).ToString().c_str(), pfrom->id);            
                }
            }
        }
    }
        
/* MCHN END */    
    
    std::deque<CNetMessage>::iterator it = pfrom->vRecvMsg.begin();
    while (!pfrom->fDisconnect && it != pfrom->vRecvMsg.end()) {
        // Don't bother if send buffer is too full to respond anyway
        int32_t fProcessInDataThread=nMessageHandlerThreads & MC_MHT_PROCESSDATA;
        int32_t fProcessTxInSeparateDataThread=nMessageHandlerThreads & MC_MHT_PROCESSTXDATA;
        CNetMessage& msg1 = *it;
        if(msg1.complete())
        {
            if(msg1.hdr.GetCommand() == "block")
            {
                if(fDebug)if(fProcessInDataThread == 0)LogPrint("mcblockperf","mchn-block-perf: New block, peer=%d\n", pfrom->id);
            }                
        }
        
        if (pfrom->nSendSize >= SendBufferSize())
        {
            if(fDebug)LogPrint("mcminor","mchn: Send buffer full on processmessages (%ld bytes) for peer %d\n", pfrom->nSendSize, pfrom->id);            
            
            CNetMessage& msg1 = *it;
            if(msg1.complete())
            {
                if(msg1.hdr.GetCommand() == "block")
                {
                    if(fDebug)if(fProcessInDataThread == 0)LogPrint("mcblockperf","mchn-block-perf: Processing block, though send buffer is full (%d), peer=%d\n", (int)pfrom->nSendSize,pfrom->id);
                }                
                else
                {
                    break;
                }
            }
            else
            {
                break;
            }
        }
        // get next message
        CNetMessage& msg = *it;

        //if (fDebug)
        //    LogPrintf("ProcessMessages(message %u msgsz, %u bytes, complete:%s)\n",
        //            msg.hdr.nMessageSize, msg.vRecv.size(),
        //            msg.complete() ? "Y" : "N");

        // end, if an incomplete message is found
        if (!msg.complete())
            break;

        if(msg.hdr.GetCommand() == "block")
        {
            if(fDebug)if(fProcessInDataThread == 0)LogPrint("mcblockperf","mchn-block-perf: Processing new block, peer=%d\n",pfrom->id);
        }
        // at this point, any failure means we can delete the current message
        it++;

        // Scan for message start
/* MCHN START */            
        
        bool fSkipMessageStartCheck=false;
        
        pfrom->fDefaultMessageStart=false;

        if (memcmp(msg.hdr.pchMessageStart, Params().MessageStart(), MESSAGE_START_SIZE) != 0) {
            fSkipMessageStartCheck=true;
            if (memcmp(msg.hdr.pchMessageStart, mc_gState->m_NetworkParams->DefaultMessageStart(), MESSAGE_START_SIZE) != 0) {
                if(mc_gState->m_NetworkParams->m_Status != MC_PRM_STATUS_EMPTY)
                {
                    LogPrintf("mchn: PROCESSMESSAGE: INVALID MESSAGESTART %s peer=%d\n", SanitizeString(msg.hdr.GetCommand()), pfrom->id);
                    fOk = false;
                    break;
                }
            }
            else
            {
                pfrom->fDefaultMessageStart=true;
            }
/* MCHN END */            
        }

        // Read header
        CMessageHeader& hdr = msg.hdr;
        if (!hdr.IsValid(fSkipMessageStartCheck))
        {
            LogPrintf("mchn: PROCESSMESSAGE: ERRORS IN HEADER %s peer=%d\n", SanitizeString(hdr.GetCommand()), pfrom->id);
            continue;
        }
        string strCommand = hdr.GetCommand();
        
/* MCHN START */            
        if(fDebug)LogPrint("mchnminor","mchn: RECV: %s, peer=%d\n", SanitizeString(hdr.GetCommand()), pfrom->id);
        if(InitialNetLogTime)if(GetTime()-pfrom->nTimeConnected<InitialNetLogTime)LogPrintf("mchn-inl: RECV: %s, peer=%d\n", SanitizeString(hdr.GetCommand()), pfrom->id);
        if(mc_gState->m_NetworkParams->m_Status == MC_PRM_STATUS_EMPTY)
        {
            if((strCommand != "verack") && (strCommand != "version"))
            {
                LogPrintf("IGNORED %s, peer=%d\n", SanitizeString(hdr.GetCommand()), pfrom->id);                        
                fOk = false;
                break;                
            }
        }
        
        if(!pfrom->fParameterSetVerified)
        {
            if((strCommand != "verackack") && (strCommand != "verack") && (strCommand != "version"))
            {
                LogPrintf("mchn: IGNORED %s, peer=%d\n", SanitizeString(hdr.GetCommand()), pfrom->id);                        
                fOk = false;
                break;                
            }
        }

        
/* MCHN END */            
        // Message size
        unsigned int nMessageSize = hdr.nMessageSize;

        // Checksum
        CDataStream& vRecv = msg.vRecv;
        uint256 hash = Hash(vRecv.begin(), vRecv.begin() + nMessageSize);
        unsigned int nChecksum = 0;
        memcpy(&nChecksum, &hash, sizeof(nChecksum));
        if (nChecksum != hdr.nChecksum)
        {
            LogPrintf("ProcessMessages(%s, %u bytes) : CHECKSUM ERROR nChecksum=%08x hdr.nChecksum=%08x\n",
               SanitizeString(strCommand), nMessageSize, nChecksum, hdr.nChecksum);
            continue;
        }

        // Process message
        bool fRet = false;
        if( fProcessInDataThread && (strCommand != "block") && (strCommand != "tx") && (strCommand != "headers") && (strCommand != "inv") )
        {
            fProcessInDataThread=false;
        }
        try
        {
            if(pfrom->fDisconnect || !MultichainNode_DisconnectRemote(pfrom))
            {
                if(fProcessInDataThread)
                {
                    if(fProcessTxInSeparateDataThread)
                    {
                        if(strCommand != "inv")
                        {
                            if(strCommand == "tx")
                            {
                                LOCK(pfrom->cs_vRecvTxDataMsg);
                                pfrom->vRecvTxDataMsg.push_back(msg);
                            }
                            else
                            {
                                LOCK(pfrom->cs_vRecvDataMsg);
                                pfrom->vRecvDataMsg.push_back(msg);
                            }
                        }
                        else
                        {
                            SplitInvMessage(pfrom, msg);
                        }
                    }
                    else
                    {
                        LOCK(pfrom->cs_vRecvDataMsg);
                        pfrom->vRecvDataMsg.push_back(msg);                        
                    }
                    fRet=true;                            
                }
                else
                {
                    fRet = ProcessMessage(pfrom, strCommand, vRecv, msg.nTime);
                }
            }
            boost::this_thread::interruption_point();
        }
        catch (std::ios_base::failure& e)
        {
            pfrom->PushMessage("reject", strCommand, REJECT_MALFORMED, string("error parsing message"));
            if (strstr(e.what(), "end of data"))
            {
                // Allow exceptions from under-length message on vRecv
                LogPrintf("ProcessMessages(%s, %u bytes) : Exception '%s' caught, normally caused by a message being shorter than its stated length\n", SanitizeString(strCommand), nMessageSize, e.what());
            }
            else if (strstr(e.what(), "size too large"))
            {
                // Allow exceptions from over-long size
                LogPrintf("ProcessMessages(%s, %u bytes) : Exception '%s' caught\n", SanitizeString(strCommand), nMessageSize, e.what());
            }
            else
            {
                PrintExceptionContinue(&e, "ProcessMessages()");
            }
        }
        catch (boost::thread_interrupted) {
            throw;
        }
        catch (std::exception& e) {
            PrintExceptionContinue(&e, "ProcessMessages()");
        } catch (...) {
            PrintExceptionContinue(NULL, "ProcessMessages()");
        }

        if (!fRet)
            LogPrintf("ProcessMessage(%s, %u bytes) FAILED peer=%d\n", SanitizeString(strCommand), nMessageSize, pfrom->id);

        if(OrphanHandlerVersion != 1)
        {
            break;
        }
    }

    // In case the connection got shut down, its receive buffer was wiped
    if (!pfrom->fDisconnect)
        pfrom->vRecvMsg.erase(pfrom->vRecvMsg.begin(), it);

    return fOk;
}

bool ProcessDataMessage(CNode* pfrom,CNetMessage& msg)
{
    if(!msg.complete())
    {
        return false;
    }

    
    if(msg.hdr.GetCommand() == "block")
    {
        if(fDebug)LogPrint("mcblockperf","mchn-block-perf: New block, peer=%d\n", pfrom->id);
    }                
        
    if (pfrom->nSendSize >= SendBufferSize())
    {
        if(fDebug)LogPrint("mcminor","mchn: Send buffer full on processdatamessage (%ld bytes), msg %s for peer %d\n", msg.hdr.GetCommand().c_str(),pfrom->nSendSize, pfrom->id);            

        if(msg.hdr.GetCommand() == "block")
        {
            if(fDebug)LogPrint("mcblockperf","mchn-block-perf: Processing block, though send buffer is full (%d), peer=%d\n", (int)pfrom->nSendSize,pfrom->id);
        }                
        else
        {
            return false;
        }
    }
                
    if(msg.hdr.GetCommand() == "inv")
    {
        if(pfrom->mapAskFor.size() >= MAX_INV_SZ)
        {                
            return false;
        }
    }

    if(msg.hdr.GetCommand() == "block")
    {
        if(fDebug)LogPrint("mcblockperf","mchn-block-perf: Processing new block, peer=%d\n",pfrom->id);
    }
        // Read header
    CMessageHeader& hdr = msg.hdr;
    string strCommand = hdr.GetCommand();

    if(fDebug)LogPrint("mchnminor","mchn: DRCV: %s, peer=%d\n", SanitizeString(hdr.GetCommand()), pfrom->id);
    if(InitialNetLogTime)if(GetTime()-pfrom->nTimeConnected<InitialNetLogTime)LogPrintf("mchn-inl: DRCV: %s, peer=%d\n", SanitizeString(hdr.GetCommand()), pfrom->id);

    // Message size
    unsigned int nMessageSize = hdr.nMessageSize;

    CDataStream& vRecv = msg.vRecv;

    // Process message
    bool fRet = false;
    try
    {
        if(pfrom->fDisconnect || !MultichainNode_DisconnectRemote(pfrom))
        {
            fRet = ProcessMessage(pfrom, strCommand, vRecv, msg.nTime);
        }
        boost::this_thread::interruption_point();
    }
    catch (std::ios_base::failure& e)
    {
        pfrom->PushMessage("reject", strCommand, REJECT_MALFORMED, string("error parsing message"));
        if (strstr(e.what(), "end of data"))
        {
            // Allow exceptions from under-length message on vRecv
            LogPrintf("ProcessDataMessage(%s, %u bytes) : Exception '%s' caught, normally caused by a message being shorter than its stated length\n", SanitizeString(strCommand), nMessageSize, e.what());
        }
        else if (strstr(e.what(), "size too large"))
        {
            // Allow exceptions from over-long size
            LogPrintf("ProcessDataMessage(%s, %u bytes) : Exception '%s' caught\n", SanitizeString(strCommand), nMessageSize, e.what());
        }
        else
        {
            PrintExceptionContinue(&e, "ProcessDataMessage()");
        }
    }
    catch (boost::thread_interrupted) {
        throw;
    }
    catch (std::exception& e) {
        PrintExceptionContinue(&e, "ProcessDataMessage()");
    } catch (...) {
        PrintExceptionContinue(NULL, "ProcessDataMessage()");
    }

    if (!fRet)
    {
        LogPrintf("ProcessDataMessage(%s, %u bytes) FAILED peer=%d\n", SanitizeString(strCommand), nMessageSize, pfrom->id);
        pfrom->fDisconnect=true;
    }

    return true;
}

bool SendMessages(CNode* pto, bool fSendTrickle)
{
    {
        // Don't send anything until we get their version message
        if (pto->nVersion == 0)
            return true;

        if(!pEF->NET_IsFinalized(pto))
        {
            return true;            
        }
        //
        // Message: ping
        //
        bool pingSend = false;
        if (pto->fPingQueued) {
            // RPC ping request by user
            pingSend = true;
        }
        
//        if (pto->nPingNonceSent == 0 && pto->nPingUsecStart + PING_INTERVAL * 1000000 < GetTimeMicros()) 
        if (pto->nPingUsecStart + PING_INTERVAL * 1000000 < GetTimeMicros()) 
        {
            // Ping automatically sent as a latency probe & keepalive.
            pingSend = true;
        }
        if (pingSend) {
            uint64_t nonce = 0;
            while (nonce == 0) {
                GetRandBytes((unsigned char*)&nonce, sizeof(nonce));
            }
            pto->fPingQueued = false;
            pto->nPingUsecStart = GetTimeMicros();
            if (pto->nVersion > BIP0031_VERSION) {
                pto->nPingNonceSent = nonce;
                pto->PushMessage("ping", nonce);
            } else {
                // Peer is too old to support ping command with nonce, pong will never arrive.
                pto->nPingNonceSent = 0;
                pto->PushMessage("ping");
            }
        }

        TRY_LOCK(cs_main, lockMain); // Acquire cs_main for IsInitialBlockDownload() and CNodeState()
        if (!lockMain)
            return true;

        // Address refresh broadcast
        static int64_t nLastRebroadcast;
        if (!IsInitialBlockDownload() && (GetTime() - nLastRebroadcast > 24 * 60 * 60))
        {
            LOCK(cs_vNodes);
            BOOST_FOREACH(CNode* pnode, vNodes)
            {
                // Periodically clear setAddrKnown to allow refresh broadcasts
                if (nLastRebroadcast)
                    pnode->setAddrKnown.clear();

                // Rebroadcast our address
                AdvertizeLocal(pnode);
            }
            if (!vNodes.empty())
                nLastRebroadcast = GetTime();
        }

        //
        // Message: addr
        //
        if (fSendTrickle)
        {
            vector<CAddress> vAddr;
            vAddr.reserve(pto->vAddrToSend.size());
            BOOST_FOREACH(const CAddress& addr, pto->vAddrToSend)
            {
                // returns true if wasn't already contained in the set
                if (pto->setAddrKnown.insert(addr).second)
                {
                    vAddr.push_back(addr);
                    // receiver rejects addr messages larger than 1000
                    if (vAddr.size() >= 1000)
                    {
                        pto->PushMessage("addr", vAddr);
                        vAddr.clear();
                    }
                }
            }
            pto->vAddrToSend.clear();
            if (!vAddr.empty())
                pto->PushMessage("addr", vAddr);
        }

        CNodeState &state = *State(pto->GetId());
        if (state.fShouldBan) {
            if (pto->fWhitelisted)
                LogPrintf("Warning: not punishing whitelisted peer %s!\n", pto->addr.ToString());
            else {
                pto->fDisconnect = true;
                if (pto->addr.IsLocal())
                    LogPrintf("Warning: not banning local peer %s!\n", pto->addr.ToString());
                else
                {
                    CNode::Ban(pto->addr);
                }
            }
            state.fShouldBan = false;
        }

        BOOST_FOREACH(const CBlockReject& reject, state.rejects)
            pto->PushMessage("reject", (string)"block", reject.chRejectCode, reject.strRejectReason, reject.hashBlock);
        state.rejects.clear();

        // Start block sync
        if (pindexBestHeader == NULL)
            pindexBestHeader = chainActive.Tip();
//        bool fFetch = state.fPreferredDownload || (nPreferredDownload == 0 && !pto->fClient && !pto->fOneShot); // Download if this is a nice peer, or we have no nice peers and this one might do.
        
        bool fFetch = true;                                                     // fPreferredDownload is too dangerous in small chains
        if (!state.fSyncStarted && !pto->fClient && fFetch && !fImporting && !fReindex) {
            // Only actively request headers from a single peer, unless we're close to today.
            if (nSyncStarted == 0 || pindexBestHeader->GetBlockTime() > GetAdjustedTime() - 24 * 60 * 60) {
                state.fSyncStarted = true;
                nSyncStarted++;
                CBlockIndex *pindexStart = pindexBestHeader->pprev ? pindexBestHeader->pprev : pindexBestHeader;
                if(fDebug)LogPrint("net", "initial getheaders (%d) to peer=%d (startheight:%d)\n", pindexStart->nHeight, pto->id, pto->nStartingHeight);
                pto->PushMessage("getheaders", chainActive.GetLocator(pindexStart), uint256(0));
            }
        }

        // Resend wallet transactions that haven't gotten in a block yet
        // Except during reindex, importing and IBD, when old wallet
        // transactions become unconfirmed and spams other nodes.
        if (!fReindex && !fImporting && !IsInitialBlockDownload())
        {
/*            
            if(pwalletMain)
            {
                pwalletMain->ResendWalletTransactions();
            }
            else
            {
 */ 
                g_signals.Broadcast(false);
/*                
            }
 */ 
        }

        //
        // Message: inventory
        //

        unsigned int sent_count=0;
        vector<CInv> vInv;
        vector<CInv> vInvWait;
        {
            LOCK(pto->cs_inventory);
            vInv.reserve(pto->vInventoryToSend.size());
            vInvWait.reserve(pto->vInventoryToSend.size());
            int i=0;
            int hash_stop_pos=(int)pto->vInventoryToSend.size();
            
            uint256 mempool_hash_stop=0;
            
            {
                LOCK(mempool.cs);
                mempool_hash_stop=mempool.hashSendStop;
            }
            
            if(OrphanHandlerVersion == 1)
            {
                if(mempool_hash_stop != 0)
                {
                    while(i<(int)pto->vInventoryToSend.size())
                    {
                        if(pto->vInventoryToSend[i].hash == mempool_hash_stop)
                        {
                            hash_stop_pos=i;
                            i=pto->vInventoryToSend.size();
                        }
                        i++;
                    }
                }            
            }            
            
            i=0;
            while(i<(int)pto->vInventoryToSend.size())
            {
                CInv inv=pto->vInventoryToSend[i];
                if( (i<hash_stop_pos) || (inv.type != MSG_TX) )
                {
                    if (pto->setInventoryKnown.count(inv) == 0)
                    {
                        if (pto->setInventoryKnown.insert(inv).second)
                        {
                            sent_count++;
                            vInv.push_back(inv);
                            if (vInv.size() >= 1000)
                            {
                                if(MultichainNode_SendInv(pto))                 
                                    pto->PushMessage("inv", vInv);
                                vInv.clear();
                            }
                        }    
                    }
                }                
                else
                {
                    vInvWait.push_back(inv);                        
                }
                
                i++;
            } 
            pto->vInventoryToSend = vInvWait;                
        }
        if (!vInv.empty())
            if(MultichainNode_SendInv(pto))                                     // MCNN
                pto->PushMessage("inv", vInv);
        if( (sent_count > 30000) || ( (sent_count > 0) && (pto->vInventoryToSend.size() > 0) ) )
        {
            LogPrint("mcblockperf","mchn-block-perf: Sent %d inv records to node %d, %d waiting\n",
                    sent_count,pto->id,(int)pto->vInventoryToSend.size());                    
        }
        // Detect whether we're stalling
        int64_t nNow = GetTimeMicros();
        if (!pto->fDisconnect && state.nStallingSince && state.nStallingSince < nNow - 1000000 * BLOCK_STALLING_TIMEOUT) {
            // Stalling only triggers when the block download window cannot move. During normal steady state,
            // the download window should be much larger than the to-be-downloaded set of blocks, so disconnection
            // should only happen during initial block download.
            LogPrintf("Peer=%d is stalling block download, disconnecting\n", pto->id);
            pto->fDisconnect = true;
        }
/* MCHN START */        
        bool ignore_incoming=false;
        {
            LOCK(cs_main);
            ignore_incoming=MultichainNode_IgnoreIncoming(pto);
        }
        if(!ignore_incoming)
        {
            if(pto->fLastIgnoreIncoming)
            {
                BOOST_FOREACH(const QueuedBlock& entry, state.vBlocksInFlight)
                    mapBlocksInFlight.erase(entry.hash);
                
                LOCK(pto->cs_askfor);
                state.vBlocksInFlight.clear();//.front().nTime=nNow;
                vector <CInv> currentAskFor;
                while (!pto->mapAskFor.empty())
                {
                    const CInv& inv = (*pto->mapAskFor.begin()).second;
                    if (!AlreadyHave(inv) || (inv.type == MSG_BLOCK))
                    {
                        currentAskFor.push_back(inv);
                    }
                    pto->mapAskFor.erase(pto->mapAskFor.begin());                    
                }
                for(int i=0;i<(int)currentAskFor.size();i++)
                {
                    pto->mapAskFor.insert(std::make_pair(nNow, currentAskFor[i]));
                }
                LogPrintf("Resuming incoming, %d inventory items will be requested\n", (int) pto->mapAskFor.size());
            }
/* MCHN END */        
        // In case there is a block that has been in flight from this peer for (2 + 0.5 * N) times the block interval
        // (with N the number of validated blocks that were in flight at the time it was requested), disconnect due to
        // timeout. We compensate for in-flight blocks to prevent killing off peers due to our own downstream link
        // being saturated. We only count validated in-flight blocks so peers can't advertize nonexisting block hashes
        // to unreasonably increase our timeout.            
        if (!pto->fDisconnect && state.vBlocksInFlight.size() > 0 && state.vBlocksInFlight.front().nTime < nNow - 500000 * Params().TargetSpacing() * (4 + 1)) {
//        if (!pto->fDisconnect && state.vBlocksInFlight.size() > 0 && state.vBlocksInFlight.front().nTime < nNow - 500000 * Params().TargetSpacing() * (4 + state.vBlocksInFlight.front().nValidatedQueuedBefore)) {
            bool fTimeout=true;
            
            if(fDebug)LogPrint("mcblockperf","mchn-block-perf: Block timeout, delay: %ld, in flight: %d, peer=%d\n",
                    nNow-state.vBlocksInFlight.front().nTime,state.vBlocksInFlight.size(),pto->id);
            if(!pto->vRecvMsg.empty())
            {
                std::deque<CNetMessage>::iterator it = pto->vRecvMsg.begin();
                CNetMessage& msg1 = *it;
                if(msg1.complete())
                {
                    if(msg1.hdr.GetCommand() == "block")
                    {
                        if(fDebug)LogPrint("mcblockperf","mchn-block-perf: There is still block to process from peer=%d, no timeout\n",pto->id);
                        fTimeout=false;
                    }
                }
            }
            if(!fTimeout)
            {
                if (state.vBlocksInFlight.front().nTime < nNow - 500000 * Params().TargetSpacing() * (4 + state.vBlocksInFlight.front().nValidatedQueuedBefore)) 
                {
                    fTimeout=true;
                }
            }
            if(fTimeout)
            {
                LogPrintf("Timeout downloading block %s from peer=%d, disconnecting\n", state.vBlocksInFlight.front().hash.ToString(), pto->id);
                pto->fDisconnect = true;
            }
        }
/* MCHN START */        
        }
/* MCHN END */        

        //
        // Message: getdata (blocks)
        //
        vector<CInv> vGetData;
/* MCHN START */        
        bool fFetchBlocks=true;
        if(!ignore_incoming)
        {
/* MCHN END */        
        if (!pto->fDisconnect && !pto->fClient && fFetchBlocks && state.nBlocksInFlight < MAX_BLOCKS_IN_TRANSIT_PER_PEER) {
            vector<CBlockIndex*> vToDownload;
            NodeId staller = -1;            
            FindNextBlocksToDownload(pto->GetId(), MAX_BLOCKS_IN_TRANSIT_PER_PEER - state.nBlocksInFlight, vToDownload, staller);
            BOOST_FOREACH(CBlockIndex *pindex, vToDownload) {
                LogPrint("mcblockperf","mchn-block-perf: Request for block %s (A), to node %d\n",
                        pindex->GetBlockHash().ToString().c_str(),pto->id);                    
                vGetData.push_back(CInv(MSG_BLOCK, pindex->GetBlockHash()));
                MarkBlockAsInFlight(pto->GetId(), pindex->GetBlockHash(), pindex);
                if(fDebug)LogPrint("net", "Requesting block %s (%d) peer=%d\n", pindex->GetBlockHash().ToString(),
                    pindex->nHeight, pto->id);
                if(fDebug)LogPrint("mcblockperf", "mchn-block-perf: Requesting block %s (%d) peer=%d\n", pindex->GetBlockHash().ToString(),
                    pindex->nHeight, pto->id);
            }
            if (state.nBlocksInFlight == 0 && staller != -1) {
                if (State(staller)->nStallingSince == 0) {
                    State(staller)->nStallingSince = nNow;
                    if(fDebug)LogPrint("net", "Stall started peer=%d\n", staller);
                }
            }
        }
/* MCHN START */        
        }
/* MCHN END */        
        
/* MCHN START */        
        {
            LOCK(cs_main);
            CNode* seed_node;
            if(!pto->fSyncedOnce && MultichainNode_IsBlockChainSynced(pto))
            {
                LogPrintf("mchn: Synced with node %d on block %d - requesting mempool\n",pto->id,mc_gState->m_Permissions->m_Block);
                pto->PushMessage("mempool");
                seed_node=(CNode*)(mc_gState->m_pSeedNode);
                if(seed_node == pto)
                {
                    if(!MultichainNode_IsLocal(pto))
                    {
                        LogPrintf("mchn: Synced with seed node on block %d\n",mc_gState->m_Permissions->m_Block);
                        mc_RemoveFile(mc_gState->m_NetworkParams->Name(),"seed",".dat",MC_FOM_RELATIVE_TO_DATADIR);
                        mc_gState->m_pSeedNode=NULL;                    
                        LOCK(cs_vNodes);
                        if(vNodes.size() > 1)
                        {
                            LogPrintf("mchn: Disconnecting seed node\n");
                            pto->fDisconnect=true;
                        }
                    }
                }
            }
        }
/* MCHN END */        

        //
        // Message: getdata (non-blocks)
        //
/* MCHN START */        
        if(!ignore_incoming)
        {
            LOCK(pto->cs_askfor);
/* MCHN END */        
        while (!pto->fDisconnect && !pto->mapAskFor.empty() && (*pto->mapAskFor.begin()).first <= nNow)
        {
            const CInv& inv = (*pto->mapAskFor.begin()).second;
            if (!AlreadyHave(inv) || (inv.type == MSG_BLOCK))                   // MCHN +ignored blocks
            {
                if (fDebug)
                    if(fDebug)LogPrint("net", "Requesting %s peer=%d\n", inv.ToString(), pto->id);
                
                if(inv.type == MSG_BLOCK)
                {
                    LogPrint("mcblockperf","mchn-block-perf: Request for block %s (B), to node %d\n",
                            inv.ToString().c_str(),pto->id);                    
                }
                
                vGetData.push_back(inv);
                if (vGetData.size() >= 1000)
                {
                    pto->PushMessage("getdata", vGetData);
                    vGetData.clear();
                }
            }
            pto->mapAskFor.erase(pto->mapAskFor.begin());
        }
        if (!vGetData.empty())
            pto->PushMessage("getdata", vGetData);
/* MCHN START */        
        }
        pto->fLastIgnoreIncoming=ignore_incoming;
        
        if(pwalletTxsMain->m_ChunkCollector)
        {
            int64_t time_millis_now=GetTimeMillis();

            if(pwalletTxsMain->m_ChunkCollector->m_NextTryTimestamp < time_millis_now)
            {
                if(MultichainNode_CollectChunks())
                {
                    MultichainCollectChunks(pwalletTxsMain->m_ChunkCollector);
                }                
                pwalletTxsMain->m_ChunkCollector->m_NextTryTimestamp=time_millis_now+MultichainCollectChunksQueueStats(pwalletTxsMain->m_ChunkCollector);
                
//                    if(fDebug)LogPrint("chunks", "Chunks to collect: %d\n", still_to_collect);
//                pwalletTxsMain->m_ChunkCollector->m_NextTryTimestamp=time_millis_now+GetArg("-offchainrequestfreq",MC_CCW_TIMEOUT_BETWEEN_COLLECTS_MILLIS);
            }
        }
/* MCHN END */                
    }
    return true;
}


bool CBlockUndo::WriteToDisk(CDiskBlockPos &pos, const uint256 &hashBlock)
{
    // Open history file to append
    CAutoFile fileout(OpenUndoFile(pos), SER_DISK, CLIENT_VERSION);
    if (fileout.IsNull())
        return error("CBlockUndo::WriteToDisk : OpenUndoFile failed");

    // Write index header
    unsigned int nSize = fileout.GetSerializeSize(*this);
    fileout << FLATDATA(Params().MessageStart()) << nSize;

    // Write undo data
    long fileOutPos = ftell(fileout.Get());
    if (fileOutPos < 0)
        return error("CBlockUndo::WriteToDisk : ftell failed");
    pos.nPos = (unsigned int)fileOutPos;
    fileout << *this;

    // calculate & write checksum
    CHashWriter hasher(SER_GETHASH, PROTOCOL_VERSION);
    hasher << hashBlock;
    hasher << *this;
    fileout << hasher.GetHash();

    return true;
}

bool CBlockUndo::ReadFromDisk(const CDiskBlockPos &pos, const uint256 &hashBlock)
{
    // Open history file to read
    CAutoFile filein(OpenUndoFile(pos, true), SER_DISK, CLIENT_VERSION);
    if (filein.IsNull())
        return error("CBlockUndo::ReadFromDisk : OpenBlockFile failed");

    // Read block
    uint256 hashChecksum;
    try {
        filein >> *this;
        filein >> hashChecksum;
    }
    catch (std::exception &e) {
        return error("%s : Deserialize or I/O error - %s", __func__, e.what());
    }

    // Verify checksum
    CHashWriter hasher(SER_GETHASH, PROTOCOL_VERSION);
    hasher << hashBlock;
    hasher << *this;
    if (hashChecksum != hasher.GetHash())
        return error("CBlockUndo::ReadFromDisk : Checksum mismatch");

    return true;
}

 std::string CBlockFileInfo::ToString() const {
     return strprintf("CBlockFileInfo(blocks=%u, size=%u, heights=%u...%u, time=%s...%s)", nBlocks, nSize, nHeightFirst, nHeightLast, DateTimeStrFormat("%Y-%m-%d", nTimeFirst), DateTimeStrFormat("%Y-%m-%d", nTimeLast));
 }



class CMainCleanup
{
public:
    CMainCleanup() {}
    ~CMainCleanup() {
        // block headers
        BlockMap::iterator it1 = mapBlockIndex.begin();
        for (; it1 != mapBlockIndex.end(); it1++)
            delete (*it1).second;
        mapBlockIndex.clear();

        // orphan transactions
        mapOrphanTransactions.clear();
        mapOrphanTransactionsByPrev.clear();
    }
} instance_of_cmaincleanup;
