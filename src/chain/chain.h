// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#ifndef BITCOIN_CHAIN_H
#define BITCOIN_CHAIN_H

#include "primitives/block.h"
#include "chain/pow.h"
#include "utils/tinyformat.h"
#include "structs/uint256.h"
#include "keys/pubkey.h"

#include <vector>

#include <boost/foreach.hpp>

struct CDiskBlockPos
{
    int nFile;
    unsigned int nPos;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(VARINT(nFile));
        READWRITE(VARINT(nPos));
    }

    CDiskBlockPos() {
        SetNull();
    }

    CDiskBlockPos(int nFileIn, unsigned int nPosIn) {
        nFile = nFileIn;
        nPos = nPosIn;
    }

    friend bool operator==(const CDiskBlockPos &a, const CDiskBlockPos &b) {
        return (a.nFile == b.nFile && a.nPos == b.nPos);
    }

    friend bool operator!=(const CDiskBlockPos &a, const CDiskBlockPos &b) {
        return !(a == b);
    }

    void SetNull() { nFile = -1; nPos = 0; }
    bool IsNull() const { return (nFile == -1); }
};

enum BlockStatus {
    //! Unused.
    BLOCK_VALID_UNKNOWN      =    0,

    //! Parsed, version ok, hash satisfies claimed PoW, 1 <= vtx count <= max, timestamp not in future
    BLOCK_VALID_HEADER       =    1,

    //! All parent headers found, difficulty matches, timestamp >= median previous, checkpoint. Implies all parents
    //! are also at least TREE.
    BLOCK_VALID_TREE         =    2,

    /**
     * Only first tx is coinbase, 2 <= coinbase input script length <= 100, transactions valid, no duplicate txids,
     * sigops, size, merkle root. Implies all parents are at least TREE but not necessarily TRANSACTIONS. When all
     * parent blocks also have TRANSACTIONS, CBlockIndex::nChainTx will be set.
     */
    BLOCK_VALID_TRANSACTIONS =    3,

    //! Outputs do not overspend inputs, no double spends, coinbase output ok, immature coinbase spends, BIP30.
    //! Implies all parents are also at least CHAIN.
    BLOCK_VALID_CHAIN        =    4,

    //! Scripts & signatures ok. Implies all parents are also at least SCRIPTS.
    BLOCK_VALID_SCRIPTS      =    5,

    //! All validity bits.
    BLOCK_VALID_MASK         =   BLOCK_VALID_HEADER | BLOCK_VALID_TREE | BLOCK_VALID_TRANSACTIONS |
                                 BLOCK_VALID_CHAIN | BLOCK_VALID_SCRIPTS,

    BLOCK_HAVE_DATA          =    8, //! full block available in blk*.dat
    BLOCK_HAVE_UNDO          =   16, //! undo data available in rev*.dat
    BLOCK_HAVE_MASK          =   BLOCK_HAVE_DATA | BLOCK_HAVE_UNDO,

    BLOCK_FAILED_VALID       =   32, //! stage after last reached validness failed
    BLOCK_FAILED_CHILD       =   64, //! descends from failed block
    BLOCK_FAILED_MASK        =   BLOCK_FAILED_VALID | BLOCK_FAILED_CHILD,
    
    BLOCK_HAVE_SIZE          =  128, // block has calculated size
    BLOCK_HAVE_MINER_PUBKEY  =  256, // block has miner pubkey
    BLOCK_HAVE_CHAIN_CACHE   =  512, // block has chain tx count, chain work and index skip hash

    BLOCK_HAVE_SUCCESSOR     =  1024, // there is another block (maybe invalid) for which it is a previous
    
};

/** The block chain is a tree shaped structure starting with the
 * genesis block at the root, with each block potentially having multiple
 * candidates to be the next block. A blockindex may have multiple pprev pointing
 * to it, but at most one of them can be part of the currently active branch.
 */
class CBlockIndex
{
public:
    //! pointer to the hash of the block, if any. memory is owned by this CBlockIndex
    const uint256* phashBlock;

    //! pointer to the index of the predecessor of this block
    CBlockIndex* pprev;
    uint256 hashPrev;
    
    CBlockIndex* getpprev();
    void setpprev(CBlockIndex* p);
    
    //! pointer to the index of some further predecessor of this block
    CBlockIndex* pskip;
    uint256 hashSkip;
    
    CBlockIndex* getpskip();
    void setpskip(CBlockIndex* p);

    //! height of the entry in the chain. The genesis block has height 0
    int nHeight;

    //! Which # file this block is stored in (blk?????.dat)
    int nFile;

    //! Byte offset within blk?????.dat where this block's data is stored
    unsigned int nDataPos;

    //! Byte offset within rev?????.dat where this block's undo data is stored
    unsigned int nUndoPos;

    //! (memory only) Total amount of work (expected number of hashes) in the chain up to and including this block
    uint256 nChainWork;

    //! Number of transactions in this block.
    //! Note: in a potential headers-first mode, this number cannot be relied upon
    unsigned int nTx;

    //! (memory only) Number of transactions in the chain up to and including this block.
    //! This value will be non-zero only if and only if transactions for this block and all its parents are available.
    //! Change to 64-bit type when necessary; won't happen before 2030
    uint64_t nChainTx;

    //! Verification status of this block. See enum BlockStatus
    unsigned int nStatus;

    //! block header
    int nVersion;
    uint256 hashMerkleRoot;
    unsigned int nTime;
    unsigned int nBits;
    unsigned int nNonce;

    //! (memory only) Sequential id assigned to distinguish order in which blocks are received.
    uint32_t nSequenceId;

/* MCHN START */    
    int nHeightMinedByMe;
    uint32_t nCanMine;
    double dTimeReceived;
    CPubKey kMiner;
    unsigned int nSize;
    bool fPassedMinerPrecheck;
    int32_t nFirstSuccessor;
    bool fUpdated;
    
/* MCHN END */
    
    void SetNull()
    {
        phashBlock = NULL;
        pprev = NULL;
        pskip = NULL;
        nHeight = 0;
        nFile = 0;
        nDataPos = 0;
        nUndoPos = 0;
        nChainWork = 0;
        nTx = 0;
        nChainTx = 0;
        nStatus = 0;
        nSequenceId = 0;

        nVersion       = 0;
        hashMerkleRoot = 0;
        nTime          = 0;
        nBits          = 0;
        nNonce         = 0;
        
        hashPrev=0;
        hashSkip=0;
        
/* MCHN START */    
        nHeightMinedByMe=0;
        nCanMine=0;
        dTimeReceived=0.;
        fPassedMinerPrecheck=false;
        nFirstSuccessor=0;
        nSize=0;
        fUpdated=false;
/* MCHN END */
    }

    CBlockIndex()
    {
        SetNull();
    }

    CBlockIndex(const CBlockHeader& block)
    {
        SetNull();

        nVersion       = block.nVersion;
        hashMerkleRoot = block.hashMerkleRoot;
        nTime          = block.nTime;
        nBits          = block.nBits;
        nNonce         = block.nNonce;
    }

    CDiskBlockPos GetBlockPos() const {
        CDiskBlockPos ret;
        if (nStatus & BLOCK_HAVE_DATA) {
            ret.nFile = nFile;
            ret.nPos  = nDataPos;
        }
        return ret;
    }

    CDiskBlockPos GetUndoPos() const {
        CDiskBlockPos ret;
        if (nStatus & BLOCK_HAVE_UNDO) {
            ret.nFile = nFile;
            ret.nPos  = nUndoPos;
        }
        return ret;
    }

    CBlockHeader GetBlockHeader() const
    {
        CBlockHeader block;
        block.nVersion       = nVersion;
/*        
        if (getpprev())
            block.hashPrevBlock = getpprev()->GetBlockHash();
 */ 
        block.hashPrevBlock  = hashPrev; 
        block.hashMerkleRoot = hashMerkleRoot;
        block.nTime          = nTime;
        block.nBits          = nBits;
        block.nNonce         = nNonce;
        return block;
    }

    uint256 GetBlockHash() const
    {
        return *phashBlock;
    }

    int64_t GetBlockTime() const
    {
        return (int64_t)nTime;
    }

    enum { nMedianTimeSpan=11 };

/* BLMP removed const for method */    
    
    int64_t GetMedianTimePast() 
    {
        int64_t pmedian[nMedianTimeSpan];
        int64_t* pbegin = &pmedian[nMedianTimeSpan];
        int64_t* pend = &pmedian[nMedianTimeSpan];

        CBlockIndex* pindex = this;
        for (int i = 0; i < nMedianTimeSpan && pindex; i++, pindex = pindex->getpprev())
            *(--pbegin) = pindex->GetBlockTime();

        std::sort(pbegin, pend);
        return pbegin[(pend - pbegin)/2];
    }

    /**
     * Returns true if there are nRequired or more blocks of minVersion or above
     * in the last Params().ToCheckBlockUpgradeMajority() blocks, starting at pstart 
     * and going backwards.
     */
    static bool IsSuperMajority(int minVersion, CBlockIndex* pstart,
                                unsigned int nRequired);

    std::string ToString() const
    {
        return strprintf("CBlockIndex(hashPrev=%s, nHeight=%d, merkle=%s, hashBlock=%s)",
            hashPrev.ToString(), nHeight,
            hashMerkleRoot.ToString(),
            GetBlockHash().ToString());
    }

    //! Check whether this block index entry is valid up to the passed validity level.
    bool IsValid(enum BlockStatus nUpTo = BLOCK_VALID_TRANSACTIONS) const
    {
        assert(!(nUpTo & ~BLOCK_VALID_MASK)); // Only validity flags allowed.
        if (nStatus & BLOCK_FAILED_MASK)
            return false;
/* MCHN START */        
//        return ((nStatus & BLOCK_VALID_MASK) >= nUpTo);
        return ((nStatus & BLOCK_VALID_MASK) >= (unsigned int)nUpTo);
/* MCHN END */        
    }

    //! Raise the validity level of this block index entry.
    //! Returns true if the validity was changed.
    bool RaiseValidity(enum BlockStatus nUpTo)
    {
        assert(!(nUpTo & ~BLOCK_VALID_MASK)); // Only validity flags allowed.
        if (nStatus & BLOCK_FAILED_MASK)
            return false;
/* MCHN START */        
//        if ((nStatus & BLOCK_VALID_MASK) < nUpTo) {
        if ((nStatus & BLOCK_VALID_MASK) < (unsigned int)nUpTo) {
/* MCHN END */        
            nStatus = (nStatus & ~BLOCK_VALID_MASK) | nUpTo;
            return true;
        }
        return false;
    }

    //! Build the skiplist pointer for this entry.
    void BuildSkip();

    //! Efficiently find an ancestor of this block.
    CBlockIndex* GetAncestor(int height);
    const CBlockIndex* GetAncestor(int height) const;
};

/** Used to marshal pointers into hashes for db storage. */
class CDiskBlockIndex : public CBlockIndex
{
public:
    uint256 hashPrev;

    CDiskBlockIndex() {
        hashPrev = 0;
    }

    explicit CDiskBlockIndex(CBlockIndex* pindex) : CBlockIndex(*pindex) {
        hashPrev = (getpprev() ? getpprev()->GetBlockHash() : 0);
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        if (!(nType & SER_GETHASH))
            READWRITE(VARINT(nVersion));

        READWRITE(VARINT(nHeight));
        READWRITE(VARINT(nStatus));
        READWRITE(VARINT(nTx));
        if (nStatus & (BLOCK_HAVE_DATA | BLOCK_HAVE_UNDO))
            READWRITE(VARINT(nFile));
        if (nStatus & BLOCK_HAVE_DATA)
            READWRITE(VARINT(nDataPos));
        if (nStatus & BLOCK_HAVE_UNDO)
            READWRITE(VARINT(nUndoPos));
        
        if (nStatus & BLOCK_HAVE_SIZE)
            READWRITE(VARINT(nSize));
        if (nStatus & BLOCK_HAVE_MINER_PUBKEY)
            READWRITE(kMiner);

        if (nStatus & BLOCK_HAVE_CHAIN_CACHE)
        {
            READWRITE(nChainWork);
            READWRITE(nChainTx);
            READWRITE(hashSkip);
        }

        // block header
        READWRITE(this->nVersion);
        READWRITE(hashPrev);
        READWRITE(hashMerkleRoot);
        READWRITE(nTime);
        READWRITE(nBits);
        READWRITE(nNonce);
    }

    uint256 GetBlockHash() const
    {
        CBlockHeader block;
        block.nVersion        = nVersion;
        block.hashPrevBlock   = hashPrev;
        block.hashMerkleRoot  = hashMerkleRoot;
        block.nTime           = nTime;
        block.nBits           = nBits;
        block.nNonce          = nNonce;
        return block.GetHash();
    }


    std::string ToString() const
    {
        std::string str = "CDiskBlockIndex(";
        str += CBlockIndex::ToString();
        str += strprintf("\n                hashBlock=%s, hashPrev=%s)",
            GetBlockHash().ToString(),
            hashPrev.ToString());
        return str;
    }
};

class CChainHashStorage{

private:     
    std::vector<uint256> vChain;

public:    
    uint256 gethash(int nHeight);
    void sethash(int nHeight, uint256 hash);
    int getsize() const;
    void setsize(int nHeight);
};

class CChainPtrStorage{

private:     
    std::vector<CBlockIndex *> vChain;
    CChainHashStorage cHashChain;

public:    
    
    CBlockIndex * getptr(int nHeight);
    void setptr(int nHeight, CBlockIndex * ptr);
    int getsize() const;
    void setsize(int nHeight);
    uint256 gethash(int nHeight);
    
};

/** An in-memory indexed chain of blocks. */
class CChain {
private:
    
    CChainPtrStorage cPtrChain;
    
public:
    /** Returns the index entry for the genesis block of this chain, or NULL if none. */
    CBlockIndex *Genesis();
    
    /** Returns the index entry for the tip of this chain, or NULL if none. */
    CBlockIndex *Tip();

    /** Returns the index entry at a particular height in this chain, or NULL if no such height exists. */
    CBlockIndex *operator[](int nHeight);

    /** Compare two chains efficiently. */
//    friend bool operator==(const CChain &a, const CChain &b){
    friend bool operator==(CChain &a, CChain &b){
        return a.Height() == b.Height() &&
               a[a.Height()] == b[b.Height()];

    //        return a.vChain.size() == b.vChain.size() &&
    //               a.vChain[a.vChain.size() - 1] == b.vChain[b.vChain.size() - 1];
    }

    /** Efficiently check whether a block is present in this chain. */
    bool Contains(const CBlockIndex *pindex);

    /** Find the successor of a block in this chain, or NULL if the given index is not found or is the tip. */
    CBlockIndex *Next(const CBlockIndex *pindex);

    /** Return the maximal height in the chain. Is equal to chain.Tip() ? chain.Tip()->nHeight : -1. */
    int Height() const;
    
    /** Set/initialize a chain with a given tip. */
    void SetTip(CBlockIndex *pindex);

    /** Return a CBlockLocator that refers to a block in this chain (by default the tip). */
    CBlockLocator GetLocator(const CBlockIndex *pindex = NULL);

    /** Find the last common block between this chain and a block index entry. */
    CBlockIndex *FindFork(CBlockIndex *pindex);
};

/* Piece of chain */

class CChainPiece {

private:    

    uint256 hashTip;   
    int heightTip;
    
public:    
    
    CChainPiece() {
        hashTip = 0;   
        heightTip = -1;
    }
    
    uint256 TipHash();    
    int Height();
    
    void SetTip(CBlockIndex *pindex);
    
};

#endif // BITCOIN_CHAIN_H
