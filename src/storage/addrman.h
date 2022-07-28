// Copyright (c) 2012 Pieter Wuille
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#ifndef BITCOIN_ADDRMAN_H
#define BITCOIN_ADDRMAN_H

#include "net/netbase.h"
#include "protocol/netprotocol.h"
#include "utils/random.h"
#include "utils/sync.h"
#include "utils/timedata.h"
#include "utils/util.h"

#include <map>
#include <set>
#include <stdint.h>
#include <vector>


#define MC_AMM_MIN_MODE                         0     
#define MC_AMM_RECENT_SUCCESS                   0     
#define MC_AMM_RECENT_FAIL                      1     
#define MC_AMM_NEW_NET                          2
#define MC_AMM_OLD_FAIL                         3     
#define MC_AMM_TRIED_NET                        4
#define MC_AMM_MODE_COUNT                       5

#define MC_AMF_NONE                           0x000000000
#define MC_AMF_IGNORED                        0x000000001
#define MC_AMF_SOURCE_PEER                    0x000000000
#define MC_AMF_SOURCE_ADDED                   0x000000100
#define MC_AMF_SOURCE_SEED                    0x000000200
#define MC_AMF_SOURCE_MASK                    0x00000FF00

class CMCAddrInfo
{
    private:
        CService m_NetAddress;
        unsigned short m_Reserved1;
        uint160 m_MCAddress;
        int64_t m_LastSuccess;
        int32_t m_Attempts;
        uint32_t m_Flags;
        int64_t m_LastTry;
        uint32_t m_PrevRow;
        uint32_t m_LastRow;
        uint32_t m_LastSuccessRow;
        uint32_t m_Reserved2;
        
    public:
        CMCAddrInfo()
        {
            Init();            
        }
        
        CMCAddrInfo(const CService &netaddr, uint160 mcaddr) 
        {
            Init();
            m_NetAddress=netaddr;
            m_MCAddress=mcaddr;
        }
        CMCAddrInfo(void *raw) 
        {
            Init();
            memcpy(this, raw, sizeof(CMCAddrInfo));
        }
        void Init();
        void SetFlag(uint32_t flag,int set_flag);
        void SetLastRow(uint32_t row);
        void SetPrevRow(uint32_t row);
        void ResetLastTry(bool success);
        uint32_t GetFlags();
        uint32_t GetLastRow();
        uint32_t GetPrevRow();
        CService GetNetAddress();        
        uint160 GetMCAddress();
        int32_t GetLastTryInfo(int64_t *last_success,int64_t *last_try,uint32_t *last_success_row);
        void Try();
        void Set(uint32_t row);
        bool IsNet();
};

class CMCAddrMan
{
    private:
        mutable CCriticalSection cs;
        
        uint32_t m_RowSize;
        uint32_t m_CurRow;
        
        std::vector<CMCAddrInfo> m_MCAddrs;
        std::map<uint256, uint32_t> m_HashMap;
        
        uint256 GetHash(const CService &netaddr, uint160 mcaddr);
        std::vector<CMCAddrInfo> m_Selected[MC_AMM_MODE_COUNT];
        uint32_t m_Position[MC_AMM_MODE_COUNT];
        std::set <uint160> m_MCAddrConnected;
        std::set <uint160> m_MCAddrTried;
        std::set <CService> m_NetAddrConnected;
        std::set <CService> m_NetAddrTried;
        
    public:
        CMCAddrMan()
        {
            Init();            
        }
        void Init();
        void Try(const CService &netaddr, uint160 mcaddr);
        void Set(const CService &netaddr, uint160 mcaddr);
        bool SetOutcome(const CService &netaddr, uint160 mcaddr,bool outcome);
        uint32_t Load();
        void Save() const;
        CMCAddrInfo *Find(const CService &netaddr, uint160 mcaddr);
        CMCAddrInfo *Select(uint160 mcaddr,uint32_t mode);
        void Reset();
        CMCAddrInfo *Next();
        uint32_t PrepareSelect(std::set<CService> setLocalAddr,uint32_t *counts);
        CMCAddrInfo *Select(int mode);        
};

/** 
 * Extended statistics about a CAddress 
 */
class CAddrInfo : public CAddress
{
private:
    //! where knowledge about this address first came from
    CNetAddr source;

    //! last successful connection by us
    int64_t nLastSuccess;

    //! last try whatsoever by us:
    // int64_t CAddress::nLastTry

    //! connection attempts since last successful attempt
    int nAttempts;

    //! reference count in new sets (memory only)
    int nRefCount;

    //! in tried set? (memory only)
    bool fInTried;

    //! position in vRandom
    int nRandomPos;

    friend class CAddrMan;
    
    bool fSCInvalid;
    bool fSCDead;
    double dSCChance;

public:

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(*(CAddress*)this);
        READWRITE(source);
        READWRITE(nLastSuccess);
        READWRITE(nAttempts);
    }

    void Init()
    {
        nLastSuccess = 0;
        nLastTry = 0;
        nAttempts = 0;
        nRefCount = 0;
        fInTried = false;
        nRandomPos = -1;
        fSCInvalid=false;
        fSCDead=false;
        dSCChance=0;
    }

    CAddrInfo(const CAddress &addrIn, const CNetAddr &addrSource) : CAddress(addrIn), source(addrSource)
    {
        Init();
    }

    CAddrInfo() : CAddress(), source()
    {
        Init();
    }

    void SetSC(bool invalid,int64_t nNow)
    {
        fSCInvalid=invalid;
        dSCChance=GetChance(nNow,&fSCDead);
    }

    void SCSetSelected(CAddress addr);
    
    double GetSC(bool *invalid,bool *dead,int64_t *last_attempt = NULL) const
    {
        *invalid=fSCInvalid;
        *dead=fSCDead;
        if(last_attempt)
        {
            *last_attempt=nLastTry;
        }
        return dSCChance;
    }
    
    //! Calculate in which "tried" bucket this entry belongs
    int GetTriedBucket(const std::vector<unsigned char> &nKey) const;

    //! Calculate in which "new" bucket this entry belongs, given a certain source
    int GetNewBucket(const std::vector<unsigned char> &nKey, const CNetAddr& src) const;

    //! Calculate in which "new" bucket this entry belongs, using its default source
    int GetNewBucket(const std::vector<unsigned char> &nKey) const
    {
        return GetNewBucket(nKey, source);
    }

    //! Determine whether the statistics about this entry are bad enough so that it can just be deleted
    bool IsTerrible(int64_t nNow = GetAdjustedTime()) const;

    //! Calculate the relative chance this entry should be given when selecting nodes to connect to
    double GetChance(int64_t nNow = GetAdjustedTime(),bool *fDead = NULL) const;

};

/** Stochastic address manager
 *
 * Design goals:
 *  * Keep the address tables in-memory, and asynchronously dump the entire to able in peers.dat.
 *  * Make sure no (localized) attacker can fill the entire table with his nodes/addresses.
 *
 * To that end:
 *  * Addresses are organized into buckets.
 *    * Address that have not yet been tried go into 256 "new" buckets.
 *      * Based on the address range (/16 for IPv4) of source of the information, 32 buckets are selected at random
 *      * The actual bucket is chosen from one of these, based on the range the address itself is located.
 *      * One single address can occur in up to 4 different buckets, to increase selection chances for addresses that
 *        are seen frequently. The chance for increasing this multiplicity decreases exponentially.
 *      * When adding a new address to a full bucket, a randomly chosen entry (with a bias favoring less recently seen
 *        ones) is removed from it first.
 *    * Addresses of nodes that are known to be accessible go into 64 "tried" buckets.
 *      * Each address range selects at random 4 of these buckets.
 *      * The actual bucket is chosen from one of these, based on the full address.
 *      * When adding a new good address to a full bucket, a randomly chosen entry (with a bias favoring less recently
 *        tried ones) is evicted from it, back to the "new" buckets.
 *    * Bucket selection is based on cryptographic hashing, using a randomly-generated 256-bit key, which should not
 *      be observable by adversaries.
 *    * Several indexes are kept for high performance. Defining DEBUG_ADDRMAN will introduce frequent (and expensive)
 *      consistency checks for the entire data structure.
 */

//! total number of buckets for tried addresses
#define ADDRMAN_TRIED_BUCKET_COUNT 64

//! maximum allowed number of entries in buckets for tried addresses
#define ADDRMAN_TRIED_BUCKET_SIZE 64

//! total number of buckets for new addresses
#define ADDRMAN_NEW_BUCKET_COUNT 256

//! maximum allowed number of entries in buckets for new addresses
#define ADDRMAN_NEW_BUCKET_SIZE 64

//! over how many buckets entries with tried addresses from a single group (/16 for IPv4) are spread
#define ADDRMAN_TRIED_BUCKETS_PER_GROUP 4

//! over how many buckets entries with new addresses originating from a single group are spread
#define ADDRMAN_NEW_BUCKETS_PER_SOURCE_GROUP 32

//! in how many buckets for entries with new addresses a single address may occur
#define ADDRMAN_NEW_BUCKETS_PER_ADDRESS 4

//! how many entries in a bucket with tried addresses are inspected, when selecting one to replace
#define ADDRMAN_TRIED_ENTRIES_INSPECT_ON_EVICT 4

//! how old addresses can maximally be
#define ADDRMAN_HORIZON_DAYS 30

//! after how many failed attempts we give up on a new node
#define ADDRMAN_RETRIES 3

//! how many successive failures are allowed ...
#define ADDRMAN_MAX_FAILURES 10

//! ... in at least this many days
#define ADDRMAN_MIN_FAIL_DAYS 7

//! the maximum percentage of nodes to return in a getaddr call
#define ADDRMAN_GETADDR_MAX_PCT 23

//! the maximum number of nodes to return in a getaddr call
#define ADDRMAN_GETADDR_MAX 2500

/** 
 * Stochastical (IP) address manager 
 */
class CAddrMan
{
private:
    //! critical section to protect the inner data structures
    mutable CCriticalSection cs;

    //! secret key to randomize bucket select with
    std::vector<unsigned char> nKey;

    //! last used nId
    int nIdCount;

    //! table with information about all nIds
    std::map<int, CAddrInfo> mapInfo;

    //! find an nId based on its network address
    std::map<CNetAddr, int> mapAddr;
    std::map<CService, int> mapAddrWithPort;

    //! randomly-ordered vector of all nIds
    std::vector<int> vRandom;

    // number of "tried" entries
    int nTried;

    //! list of "tried" buckets
    std::vector<std::vector<int> > vvTried;

    //! number of (unique) "new" entries
    int nNew;

    //! list of "new" buckets
    std::vector<std::set<int> > vvNew;
    
    std::map<std::string, int> mapSCAlive;    
    int nSCSelected;
    double dSCTotalChance;
    int nSCTotalBad;
    int64_t nLastRecalculate;
    
    CMCAddrMan cMCAddrMan;
    
protected:

    //! Find an entry.
    CAddrInfo* Find(const CNetAddr& addr, int *pnId = NULL);
/* MCHN START */    
    CAddrInfo* FindWithPort(const CService& addr, int *pnId = NULL);
/* MCHN END */    
    //! find an entry, creating it if necessary.
    //! nTime and nServices of the found node are updated, if necessary.
    CAddrInfo* Create(const CAddress &addr, const CNetAddr &addrSource, int *pnId = NULL);

    //! Swap two elements in vRandom.
    void SwapRandom(unsigned int nRandomPos1, unsigned int nRandomPos2);

    //! Return position in given bucket to replace.
    int SelectTried(int nKBucket);

    //! Remove an element from a "new" bucket.
    //! This is the only place where actual deletions occur.
    //! Elements are never deleted while in the "tried" table, only possibly evicted back to the "new" table.
    int ShrinkNew(int nUBucket);

    //! Move an entry from the "new" table(s) to the "tried" table
    //! @pre vvUnkown[nOrigin].count(nId) != 0
    void MakeTried(CAddrInfo& info, int nId, int nOrigin);

    //! Mark an entry "good", possibly moving it from "new" to "tried".
    void Good_(const CService &addr, int64_t nTime);

    //! Add an entry to the "new" table.
    bool Add_(const CAddress &addr, const CNetAddr& source, int64_t nTimePenalty);

    //! Mark an entry as attempted to connect.
    void Attempt_(const CService &addr, int64_t nTime);

    //! Select an address to connect to.
    //! nUnkBias determines how much to favor new addresses over tried ones (min=0, max=100)
    CAddress Select_(int nUnkBias,int nNodes);

    bool SCSelect_(int nUnkBias,int nNodes, CAddress &addr);
    void SCRecalculate_(int64_t nNow);
    
    void SCSetSelected_(CAddress addr);
    void SetSC_(bool invalid,int64_t nNow);
    
    
#ifdef DEBUG_ADDRMAN
    //! Perform consistency check. Returns an error code or zero.
    int Check_();
#endif

    //! Select several addresses at once.
    void GetAddr_(std::vector<CAddress> &vAddr);

    //! Mark an entry as currently-connected-to.
    void Connected_(const CService &addr, int64_t nTime);

public:
    /**
     * serialized format:
     * * version byte (currently 0)
     * * nKey
     * * nNew
     * * nTried
     * * number of "new" buckets
     * * all nNew addrinfos in vvNew
     * * all nTried addrinfos in vvTried
     * * for each bucket:
     *   * number of elements
     *   * for each element: index
     *
     * Notice that vvTried, mapAddr and vVector are never encoded explicitly;
     * they are instead reconstructed from the other information.
     *
     * vvNew is serialized, but only used if ADDRMAN_UNKOWN_BUCKET_COUNT didn't change,
     * otherwise it is reconstructed as well.
     *
     * This format is more complex, but significantly smaller (at most 1.5 MiB), and supports
     * changes to the ADDRMAN_ parameters without breaking the on-disk structure.
     *
     * We don't use ADD_SERIALIZE_METHODS since the serialization and deserialization code has
     * very little in common.
     *
     */
    template<typename Stream>
    void Serialize(Stream &s, int nType, int nVersionDummy) const
    {
        LOCK(cs);

        cMCAddrMan.Save();
        
        unsigned char nVersion = 0;
        s << nVersion;
        s << nKey;
        s << nNew;
        s << nTried;

        int nUBuckets = ADDRMAN_NEW_BUCKET_COUNT;
        s << nUBuckets;
        std::map<int, int> mapUnkIds;
        int nIds = 0;
        for (std::map<int, CAddrInfo>::const_iterator it = mapInfo.begin(); it != mapInfo.end(); it++) {
            if (nIds == nNew) break; // this means nNew was wrong, oh ow
            mapUnkIds[(*it).first] = nIds;
            const CAddrInfo &info = (*it).second;
            if (info.nRefCount) {
                s << info;
                nIds++;
            }
        }
        nIds = 0;
        for (std::map<int, CAddrInfo>::const_iterator it = mapInfo.begin(); it != mapInfo.end(); it++) {
            if (nIds == nTried) break; // this means nTried was wrong, oh ow
            const CAddrInfo &info = (*it).second;
            if (info.fInTried) {
                s << info;
                nIds++;
            }
        }
        for (std::vector<std::set<int> >::const_iterator it = vvNew.begin(); it != vvNew.end(); it++) {
            const std::set<int> &vNew = (*it);
            int nSize = vNew.size();
            s << nSize;
            for (std::set<int>::const_iterator it2 = vNew.begin(); it2 != vNew.end(); it2++) {
                int nIndex = mapUnkIds[*it2];
                s << nIndex;
            }
        }
    }

    template<typename Stream>
    void Unserialize(Stream& s, int nType, int nVersionDummy)
    {
        LOCK(cs);

        cMCAddrMan.Load();
        
        unsigned char nVersion;
        s >> nVersion;
        s >> nKey;
        s >> nNew;
        s >> nTried;

        int nUBuckets = 0;
        s >> nUBuckets;
        nIdCount = 0;
        mapInfo.clear();
        mapAddr.clear();
        mapAddrWithPort.clear();
        vRandom.clear();
        vvTried = std::vector<std::vector<int> >(ADDRMAN_TRIED_BUCKET_COUNT, std::vector<int>(0));
        vvNew = std::vector<std::set<int> >(ADDRMAN_NEW_BUCKET_COUNT, std::set<int>());
        for (int n = 0; n < nNew; n++) {
            CAddrInfo &info = mapInfo[n];
            s >> info;
            cMCAddrMan.Set(info,0);
            mapAddr[info] = n;
            mapAddrWithPort[info] = n;
            info.nRandomPos = vRandom.size();
            vRandom.push_back(n);
            if (nUBuckets != ADDRMAN_NEW_BUCKET_COUNT) {
                vvNew[info.GetNewBucket(nKey)].insert(n);
                info.nRefCount++;
            }
        }
        nIdCount = nNew;
        int nLost = 0;
        for (int n = 0; n < nTried; n++) {
            CAddrInfo info;
            s >> info;
            cMCAddrMan.Set(info,0);
            std::vector<int> &vTried = vvTried[info.GetTriedBucket(nKey)];
            if (vTried.size() < ADDRMAN_TRIED_BUCKET_SIZE) {
                info.nRandomPos = vRandom.size();
                info.fInTried = true;
                vRandom.push_back(nIdCount);
                mapInfo[nIdCount] = info;
                mapAddr[info] = nIdCount;
                mapAddrWithPort[info] = nIdCount;
                vTried.push_back(nIdCount);
                nIdCount++;
            } else {
                nLost++;
            }
        }
        nTried -= nLost;
        for (int b = 0; b < nUBuckets; b++) {
            std::set<int> &vNew = vvNew[b];
            int nSize = 0;
            s >> nSize;
            for (int n = 0; n < nSize; n++) {
                int nIndex = 0;
                s >> nIndex;
                CAddrInfo &info = mapInfo[nIndex];
                if (nUBuckets == ADDRMAN_NEW_BUCKET_COUNT && info.nRefCount < ADDRMAN_NEW_BUCKETS_PER_ADDRESS) {
                    info.nRefCount++;
                    vNew.insert(nIndex);
                }
            }
        }
    }

    unsigned int GetSerializeSize(int nType, int nVersion) const
    {
        return (CSizeComputer(nType, nVersion) << *this).size();
    }

    CAddrMan() : vRandom(0), vvTried(ADDRMAN_TRIED_BUCKET_COUNT, std::vector<int>(0)), vvNew(ADDRMAN_NEW_BUCKET_COUNT, std::set<int>())
    {
         nKey.resize(32);
         GetRandBytes(&nKey[0], 32);

         nIdCount = 0;
         nTried = 0;
         nNew = 0;
         
         nSCSelected=-1;
         dSCTotalChance=0.;
         nSCTotalBad=0;
         nLastRecalculate=0;
    }

    //! Return the number of (unique) addresses in all tables.
    int size()
    {
        return vRandom.size();
    }

    //! Consistency check
    void Check()
    {
#ifdef DEBUG_ADDRMAN
        {
            LOCK(cs);
            int err;
            if ((err=Check_()))
                LogPrintf("ADDRMAN CONSISTENCY CHECK FAILED!!! err=%i\n", err);
        }
#endif
    }

    //! Add a single address.
    bool Add(const CAddress &addr, const CNetAddr& source, int64_t nTimePenalty = 0)
    {
        bool fRet = false;
        {
            LOCK(cs);
            Check();
            fRet |= Add_(addr, source, nTimePenalty);
            Check();
        }
        if (fRet)
            LogPrint("addrman", "Added %s from %s: %i tried, %i new\n", addr.ToStringIPPort(), source.ToString(), nTried, nNew);
        return fRet;
    }

    //! Add multiple addresses.
    bool Add(const std::vector<CAddress> &vAddr, const CNetAddr& source, int64_t nTimePenalty = 0)
    {
        int nAdd = 0;
        {
            LOCK(cs);
            Check();
            for (std::vector<CAddress>::const_iterator it = vAddr.begin(); it != vAddr.end(); it++)
                nAdd += Add_(*it, source, nTimePenalty) ? 1 : 0;
            Check();
        }
        if (nAdd)
            LogPrint("addrman", "Added %i addresses from %s: %i tried, %i new\n", nAdd, source.ToString(), nTried, nNew);
        return nAdd > 0;
    }

    //! Mark an entry as accessible.
    void Good(const CService &addr, int64_t nTime = GetAdjustedTime())
    {
        {
            LOCK(cs);
            Check();
            Good_(addr, nTime);
            Check();
        }
    }

    //! Mark an entry as connection attempted to.
    void Attempt(const CService &addr, int64_t nTime = GetAdjustedTime())
    {
        {
            LOCK(cs);
            Check();
            Attempt_(addr, nTime);
            Check();
        }
    }

    /**
     * Choose an address to connect to.
     * nUnkBias determines how much "new" entries are favored over "tried" ones (0-100).
     */
    CAddress Select(int nUnkBias = 50,int nNodes = 0)
    {
        CAddress addrRet;
        {
            LOCK(cs);
            Check();
            addrRet = Select_(nUnkBias,nNodes);
            Check();
        }
        return addrRet;
    }
    
    void SetSC(bool invalid,int64_t nNow)
    {
        {
            LOCK(cs);
            Check();
            SetSC_(invalid,nNow);
            Check();
        }        
    }
    
    void SCSetSelected(CAddress addr)
    {
        {
            LOCK(cs);
            Check();
            SCSetSelected_(addr);
            Check();
        }                
    }

    void SCRecalculate(int64_t nNow)
    {
        {
            LOCK(cs);
            Check();
            SCRecalculate_(nNow);
            Check();
        }        
    }

    //! Return a bunch of addresses, selected at random.
    std::vector<CAddress> GetAddr()
    {
        Check();
        std::vector<CAddress> vAddr;
        {
            LOCK(cs);
            GetAddr_(vAddr);
        }
        Check();
        return vAddr;
    }

    //! Mark an entry as currently-connected-to.
    void Connected(const CService &addr, int64_t nTime = GetAdjustedTime())
    {
        {
            LOCK(cs);
            Check();
            Connected_(addr, nTime);
            Check();
        }
    }
    
    void Print()
    {
        {
            LOCK(cs);
            
            int64_t nNow = GetAdjustedTime();
            LogPrint("addrman", "Printing %u addresses\n", mapInfo.size());
            for (std::map<int, CAddrInfo>::const_iterator it = mapInfo.begin(); it != mapInfo.end(); ++it)
            {
                LogPrint("addrman", "%d: %s, Last Seen: %lds, Last Tried: %lds\n", it->first,it->second.ToStringIPPort().c_str(),nNow-it->second.nTime,nNow-it->second.nLastTry);                
            }
        }
    }

    CMCAddrMan *GetMCAddrMan()
    {
        return &cMCAddrMan;
    }
    
};

#endif // BITCOIN_ADDRMAN_H
