// Copyright (c) 2012 Pieter Wuille
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "storage/addrman.h"
#include "net/net.h"
#include "structs/base58.h"

#include "structs/hash.h"
#include "utils/serialize.h"
#include "utils/streams.h"

using namespace std;

int CAddrInfo::GetTriedBucket(const std::vector<unsigned char>& nKey) const
{
    CDataStream ss1(SER_GETHASH, 0);
    std::vector<unsigned char> vchKey = GetKey();
    ss1 << nKey << vchKey;
    uint64_t hash1 = Hash(ss1.begin(), ss1.end()).GetLow64();

    CDataStream ss2(SER_GETHASH, 0);
    std::vector<unsigned char> vchGroupKey = GetGroup();
    ss2 << nKey << vchGroupKey << (hash1 % ADDRMAN_TRIED_BUCKETS_PER_GROUP);
    uint64_t hash2 = Hash(ss2.begin(), ss2.end()).GetLow64();
    return hash2 % ADDRMAN_TRIED_BUCKET_COUNT;
}

int CAddrInfo::GetNewBucket(const std::vector<unsigned char>& nKey, const CNetAddr& src) const
{
    CDataStream ss1(SER_GETHASH, 0);
    std::vector<unsigned char> vchGroupKey = GetGroup();
    std::vector<unsigned char> vchSourceGroupKey = src.GetGroup();
    ss1 << nKey << vchGroupKey << vchSourceGroupKey;
    uint64_t hash1 = Hash(ss1.begin(), ss1.end()).GetLow64();

    CDataStream ss2(SER_GETHASH, 0);
    ss2 << nKey << vchSourceGroupKey << (hash1 % ADDRMAN_NEW_BUCKETS_PER_SOURCE_GROUP);
    uint64_t hash2 = Hash(ss2.begin(), ss2.end()).GetLow64();
    return hash2 % ADDRMAN_NEW_BUCKET_COUNT;
}

bool CAddrInfo::IsTerrible(int64_t nNow) const
{
    if (nLastTry && nLastTry >= nNow - 60) // never remove things tried in the last minute
        return false;

    if (nTime > nNow + 10 * 60) // came in a flying DeLorean
        return true;

    if (nTime == 0 || nNow - nTime > ADDRMAN_HORIZON_DAYS * 24 * 60 * 60) // not seen in recent history
        return true;

    if (nLastSuccess == 0 && nAttempts >= ADDRMAN_RETRIES) // tried N times and never a success
        return true;

    if (nNow - nLastSuccess > ADDRMAN_MIN_FAIL_DAYS * 24 * 60 * 60 && nAttempts >= ADDRMAN_MAX_FAILURES) // N successive failures in the last week
        return true;

    return false;
}

double CAddrInfo::GetChance(int64_t nNow,bool *fDead) const
{
    double fChance = 1.0;

    int64_t nSinceLastSeen = nNow - nTime;
    int64_t nSinceLastTry = nNow - nLastTry;

    if (nSinceLastSeen < 0)
        nSinceLastSeen = 0;
    if (nSinceLastTry < 0)
        nSinceLastTry = 0;

    if(fDead)
    {
        *fDead=false;
        if(nAttempts)
        {
            if(nSinceLastSeen > 86400)
            {
                *fDead=true;
            }
        }
    }
    
    fChance *= 600.0 / (600.0 + nSinceLastSeen);

    // deprioritize very recent attempts away
    if (nSinceLastTry < 60 * 10)
        fChance *= 0.01;

    // deprioritize 50% after each failed attempt
    for (int n = 0; n < nAttempts; n++)
        fChance /= 1.5;
    
/* MCHN START */    
    // Without this, if all nodes are down, fChance goes to 0
    // and it takes several seconds of 100% CPU to choose from 3 addresses.
    if(fChance<0.00001)
    {
        fChance=0.00001;
    }
/* MCHN END */    
    
    return fChance;
}

CAddrInfo* CAddrMan::Find(const CNetAddr& addr, int* pnId)
{
    std::map<CNetAddr, int>::iterator it = mapAddr.find(addr);
    if (it == mapAddr.end())
        return NULL;
    if (pnId)
        *pnId = (*it).second;
    std::map<int, CAddrInfo>::iterator it2 = mapInfo.find((*it).second);
    if (it2 != mapInfo.end())
        return &(*it2).second;
    return NULL;
}

CAddrInfo* CAddrMan::FindWithPort(const CService& addr, int* pnId)
{
    std::map<CService, int>::iterator it = mapAddrWithPort.find(addr);
    if (it == mapAddrWithPort.end())
        return NULL;
    if (pnId)
        *pnId = (*it).second;
    std::map<int, CAddrInfo>::iterator it2 = mapInfo.find((*it).second);
    if (it2 != mapInfo.end())
        return &(*it2).second;
    return NULL;
}

CAddrInfo* CAddrMan::Create(const CAddress& addr, const CNetAddr& addrSource, int* pnId)
{
    int nId = nIdCount++;
    mapInfo[nId] = CAddrInfo(addr, addrSource);
    mapAddr[addr] = nId;
    mapAddrWithPort[addr] = nId;
    mapInfo[nId].nRandomPos = vRandom.size();
    vRandom.push_back(nId);
    if (pnId)
        *pnId = nId;
    return &mapInfo[nId];
}

void CAddrMan::SwapRandom(unsigned int nRndPos1, unsigned int nRndPos2)
{
    if (nRndPos1 == nRndPos2)
        return;

    assert(nRndPos1 < vRandom.size() && nRndPos2 < vRandom.size());

    int nId1 = vRandom[nRndPos1];
    int nId2 = vRandom[nRndPos2];

    assert(mapInfo.count(nId1) == 1);
    assert(mapInfo.count(nId2) == 1);

    mapInfo[nId1].nRandomPos = nRndPos2;
    mapInfo[nId2].nRandomPos = nRndPos1;

    vRandom[nRndPos1] = nId2;
    vRandom[nRndPos2] = nId1;
}

int CAddrMan::SelectTried(int nKBucket)
{
    std::vector<int>& vTried = vvTried[nKBucket];

    // randomly shuffle the first few elements (using the entire list)
    // find the least recently tried among them
    int64_t nOldest = -1;
    int nOldestPos = -1;
    for (unsigned int i = 0; i < ADDRMAN_TRIED_ENTRIES_INSPECT_ON_EVICT && i < vTried.size(); i++) {
        int nPos = GetRandInt(vTried.size() - i) + i;
        int nTemp = vTried[nPos];
        vTried[nPos] = vTried[i];
        vTried[i] = nTemp;
        assert(nOldest == -1 || mapInfo.count(nTemp) == 1);
        if (nOldest == -1 || mapInfo[nTemp].nLastSuccess < mapInfo[nOldest].nLastSuccess) {
            nOldest = nTemp;
            nOldestPos = nPos;
        }
    }

    return nOldestPos;
}

int CAddrMan::ShrinkNew(int nUBucket)
{
    assert(nUBucket >= 0 && (unsigned int)nUBucket < vvNew.size());
    std::set<int>& vNew = vvNew[nUBucket];

    // first look for deletable items
    for (std::set<int>::iterator it = vNew.begin(); it != vNew.end(); it++) {
        assert(mapInfo.count(*it));
        CAddrInfo& info = mapInfo[*it];
        if (info.IsTerrible()) {
            if (--info.nRefCount == 0) {
                SwapRandom(info.nRandomPos, vRandom.size() - 1);
                vRandom.pop_back();
                mapAddr.erase(info);
                mapAddrWithPort.erase(info);
                mapInfo.erase(*it);
                nNew--;
            }
            vNew.erase(it);
            return 0;
        }
    }

    // otherwise, select four randomly, and pick the oldest of those to replace
    int n[4] = {GetRandInt(vNew.size()), GetRandInt(vNew.size()), GetRandInt(vNew.size()), GetRandInt(vNew.size())};
    int nI = 0;
    int nOldest = -1;
    for (std::set<int>::iterator it = vNew.begin(); it != vNew.end(); it++) {
        if (nI == n[0] || nI == n[1] || nI == n[2] || nI == n[3]) {
            assert(nOldest == -1 || mapInfo.count(*it) == 1);
            if (nOldest == -1 || mapInfo[*it].nTime < mapInfo[nOldest].nTime)
                nOldest = *it;
        }
        nI++;
    }
    assert(mapInfo.count(nOldest) == 1);
    CAddrInfo& info = mapInfo[nOldest];
    if (--info.nRefCount == 0) {
        SwapRandom(info.nRandomPos, vRandom.size() - 1);
        vRandom.pop_back();
        mapAddr.erase(info);
        mapAddrWithPort.erase(info);
        mapInfo.erase(nOldest);
        nNew--;
    }
    vNew.erase(nOldest);

    return 1;
}

void CAddrMan::MakeTried(CAddrInfo& info, int nId, int nOrigin)
{
    assert(vvNew[nOrigin].count(nId) == 1);

    // remove the entry from all new buckets
    for (std::vector<std::set<int> >::iterator it = vvNew.begin(); it != vvNew.end(); it++) {
        if ((*it).erase(nId))
            info.nRefCount--;
    }
    nNew--;

    assert(info.nRefCount == 0);

    // which tried bucket to move the entry to
    int nKBucket = info.GetTriedBucket(nKey);
    std::vector<int>& vTried = vvTried[nKBucket];

    // first check whether there is place to just add it
    if (vTried.size() < ADDRMAN_TRIED_BUCKET_SIZE) {
        vTried.push_back(nId);
        nTried++;
        info.fInTried = true;
        return;
    }

    // otherwise, find an item to evict
    int nPos = SelectTried(nKBucket);

    // find which new bucket it belongs to
    assert(mapInfo.count(vTried[nPos]) == 1);
    int nUBucket = mapInfo[vTried[nPos]].GetNewBucket(nKey);
    std::set<int>& vNew = vvNew[nUBucket];

    // remove the to-be-replaced tried entry from the tried set
    CAddrInfo& infoOld = mapInfo[vTried[nPos]];
    infoOld.fInTried = false;
    infoOld.nRefCount = 1;
    // do not update nTried, as we are going to move something else there immediately

    // check whether there is place in that one,
    if (vNew.size() < ADDRMAN_NEW_BUCKET_SIZE) {
        // if so, move it back there
        vNew.insert(vTried[nPos]);
    } else {
        // otherwise, move it to the new bucket nId came from (there is certainly place there)
        vvNew[nOrigin].insert(vTried[nPos]);
    }
    nNew++;

    vTried[nPos] = nId;
    // we just overwrote an entry in vTried; no need to update nTried
    info.fInTried = true;
    return;
}

void CAddrMan::Good_(const CService& addr, int64_t nTime)
{
    int nId;
/* MCHN START */    
//    CAddrInfo* pinfo = Find(addr, &nId);
    CAddrInfo* pinfo = FindWithPort(addr, &nId);
/* MCHN END */    
    // if not found, bail out
    if (!pinfo)
        return;

    CAddrInfo& info = *pinfo;

    // check whether we are talking about the exact same CService (including same port)
    if (info != addr)
        return;

    // update info
    info.nLastSuccess = nTime;
    info.nLastTry = nTime;
    info.nTime = nTime;
    info.nAttempts = 0;

    // if it is already in the tried set, don't do anything else
    if (info.fInTried)
        return;

    // find a bucket it is in now
    int nRnd = GetRandInt(vvNew.size());
    int nUBucket = -1;
    for (unsigned int n = 0; n < vvNew.size(); n++) {
        int nB = (n + nRnd) % vvNew.size();
        std::set<int>& vNew = vvNew[nB];
        if (vNew.count(nId)) {
            nUBucket = nB;
            break;
        }
    }

    // if no bucket is found, something bad happened;
    // TODO: maybe re-add the node, but for now, just bail out
    if (nUBucket == -1)
        return;

    LogPrint("addrman", "Moving %s to tried\n", addr.ToString());

    // move nId to the tried tables
    MakeTried(info, nId, nUBucket);
}

bool CAddrMan::Add_(const CAddress& addr, const CNetAddr& source, int64_t nTimePenalty)
{
    if (!addr.IsRoutable())
        return false;

    bool fNew = false;
    int nId;
    CAddrInfo* pinfo = FindWithPort(addr, &nId);

    if (pinfo) {
        // periodically update nTime
        bool fCurrentlyOnline = (GetAdjustedTime() - addr.nTime < 24 * 60 * 60);
        int64_t nUpdateInterval = (fCurrentlyOnline ? 60 * 60 : 24 * 60 * 60);
        if (addr.nTime && (!pinfo->nTime || pinfo->nTime < addr.nTime - nUpdateInterval - nTimePenalty))
            pinfo->nTime = max((int64_t)0, addr.nTime - nTimePenalty);

        // add services
        pinfo->nServices |= addr.nServices;

        // do not update if no new information is present
        if (!addr.nTime || (pinfo->nTime && addr.nTime <= pinfo->nTime))
            return false;

        // do not update if the entry was already in the "tried" table
        if (pinfo->fInTried)
            return false;

        // do not update if the max reference count is reached
        if (pinfo->nRefCount == ADDRMAN_NEW_BUCKETS_PER_ADDRESS)
            return false;

        // stochastic test: previous nRefCount == N: 2^N times harder to increase it
        int nFactor = 1;
        for (int n = 0; n < pinfo->nRefCount; n++)
            nFactor *= 2;
        if (nFactor > 1 && (GetRandInt(nFactor) != 0))
            return false;
    } else {
        pinfo = Create(addr, source, &nId);
        pinfo->nTime = max((int64_t)0, (int64_t)pinfo->nTime - nTimePenalty);
        nNew++;
        fNew = true;
        cMCAddrMan.Set(addr,0);
    }

    int nUBucket = pinfo->GetNewBucket(nKey, source);
    std::set<int>& vNew = vvNew[nUBucket];
    if (!vNew.count(nId)) {
        pinfo->nRefCount++;
        if (vNew.size() == ADDRMAN_NEW_BUCKET_SIZE)
            ShrinkNew(nUBucket);
        vvNew[nUBucket].insert(nId);
    }
    return fNew;
}

void CAddrMan::Attempt_(const CService& addr, int64_t nTime)
{
//    CAddrInfo* pinfo = Find(addr);
    CAddrInfo* pinfo = FindWithPort(addr);

    // if not found, bail out
    if (!pinfo)
        return;

    CAddrInfo& info = *pinfo;

    // check whether we are talking about the exact same CService (including same port)
    if (info != addr)
        return;

    // update info
    info.nLastTry = nTime;
    info.nAttempts++;
}

void CAddrMan::SCSetSelected_(CAddress addr)
{
    std::map<CService, int>::iterator it = mapAddrWithPort.find(addr);
    if(it != mapAddrWithPort.end())
    {
        nSCSelected=it->second;
    }
    nSCSelected=-1;
}


void CAddrMan::SetSC_(bool invalid,int64_t nNow)
{
    if(nSCSelected >= 0)
    {
        double dOldChance,dNewChance;
        bool fOldInvalid,fNewInvalid;
        bool fOldDead,fNewDead;
        
        std::map<int,CAddrInfo>::iterator it = mapInfo.find(nSCSelected);
        if(it == mapInfo.end())
        {
            return;
        }
                
        dOldChance=it->second.GetSC(&fOldInvalid,&fOldDead);
        it->second.SetSC(invalid,nNow);
        dNewChance=it->second.GetSC(&fNewInvalid,&fNewDead);

        if(!fNewInvalid && fOldInvalid)
        {
            nSCTotalBad--;
        }
        if(!fOldInvalid && fNewInvalid)
        {
            nSCTotalBad++;
        }
        
        std::map<string,int>::iterator scit = mapSCAlive.find(it->second.ToStringIPPort());
        if(scit != mapSCAlive.end())
        {
            dSCTotalChance -= dOldChance;
            if(!fNewInvalid && !fNewDead)
            {
                dSCTotalChance += dNewChance;                
            }
            else
            {
                mapSCAlive.erase(scit);
            }
        }
        else
        {
            if(!fNewInvalid && !fNewDead)
            {
                dSCTotalChance += dNewChance;                
                mapSCAlive.insert(make_pair(it->second.ToStringIPPort(),nSCSelected));
            }            
        }
    }
}

void CAddrMan::SCRecalculate_(int64_t nNow)
{
    if(mapInfo.size() > 10000)
    {
        return;
    }
    
    if(nNow - nLastRecalculate < 2 * 60)
    {
        return;
    }
    
    LogPrint("addrman","Updating address lists for selection\n");
    
    nLastRecalculate=nNow;
    dSCTotalChance=0;
    mapSCAlive.clear();
    
    for (std::map<int, CAddrInfo>::iterator it = mapInfo.begin(); it != mapInfo.end(); ++it)
    {
        double dNewChance;
        bool fNewInvalid;
        bool fNewDead;        
        int64_t nLastTried;
        
        it->second.dSCChance=it->second.GetChance(nNow,NULL);
        dNewChance=it->second.GetSC(&fNewInvalid,&fNewDead,&nLastTried);
                
        if(nLastTried)
        {
            if(!fNewInvalid && !fNewDead)
            {
                dSCTotalChance += dNewChance;              
                LogPrint("addrman","%s: Chance %8.7f, last seen: %ds, last tried: %ds\n",it->second.ToStringIPPort().c_str(),dNewChance,nNow-it->second.nTime,nNow-it->second.nLastTry);
                
                mapSCAlive.insert(make_pair(it->second.ToStringIPPort(),it->first));
            }
        }        
    }    
}


bool CAddrMan::SCSelect_(int nUnkBias,int nNodes,CAddress &addr)
{
    nSCSelected=-1;
    if(mapSCAlive.size() > 200)
    {
        return false;
    }
    if(mapInfo.size() > 10000)
    {
        return false;
    }

    bool fChooseAlive=true;
    
    int nAlive=(int)mapSCAlive.size();
    int nUnknown=(int)mapInfo.size()-nAlive-nSCTotalBad;
    int64_t nNow = GetAdjustedTime();
    int64_t m = 1<<30;
    
    
    if(nAlive <= nNodes) 
    {
        fChooseAlive=false;        
        if(nUnknown <= 0)
        {
            return false;            
        }
    }
    else
    {
        if(nUnknown>0)
        {
            double nCorAlive = sqrt(nAlive) * (100.0 - nUnkBias);               
            double nCorUnknown = sqrt(nUnknown) * nUnkBias;                     // prefer Unknown addresses if we are well connected
            double prob = (double)GetRandInt(m) / (double)(m);
            if ((nCorAlive + nCorUnknown) *prob < nCorUnknown) 
            {
                fChooseAlive=false;
            }        
        }
    }
    
    if(fChooseAlive)
    {
        double prob = (double)GetRandInt(m)* dSCTotalChance / (double)(m);
        double sum=0;
        for (std::map<std::string, int>::const_iterator scit = mapSCAlive.begin(); scit != mapSCAlive.end(); ++scit)
        {
            std::map<int,CAddrInfo>::iterator it = mapInfo.find(scit->second);
            if(it != mapInfo.end())
            {        
                double dNewChance;
                bool fNewInvalid;
                bool fNewDead;

                dNewChance=it->second.GetSC(&fNewInvalid,&fNewDead);
                sum+=dNewChance;
                if(sum >= prob)
                {
                    nSCSelected=scit->second;
                    addr=it->second;
                    LogPrint("addrman","Selected recent address %s, last seen: %ds\n",addr.ToStringIPPort().c_str(),nNow - addr.nTime);
                    return true;
                }
            }    
        }
    }
    else
    {
        int nTriedDead=-1;
        int64_t LastTryDead=nNow;
        
        for (std::map<int, CAddrInfo>::const_iterator it = mapInfo.begin(); it != mapInfo.end(); ++it)
        {
            bool fNewInvalid;
            bool fNewDead;
            int64_t nLastTried;
            it->second.GetSC(&fNewInvalid,&fNewDead,&nLastTried);
            
            if(!fNewInvalid)
            {
                if(nLastTried == 0)
                {
                    nSCSelected=it->first;
                    addr=it->second;
                    LogPrint("addrman","Selected first-attempt address %s\n",addr.ToStringIPPort().c_str());
                    return true;                
                }

                if(fNewDead)
                {
                    if(nLastTried <= LastTryDead)
                    {
                        LastTryDead=nLastTried;
                        nTriedDead=it->first;
                    }                    
                }
            }
        }    
                
        if(nTriedDead >= 0)
        {
            MilliSleep(100);                                                    // All is dead, no need to hurry
            
            if(GetRandInt(m) < (0.09 + 0.01 * nUnkBias) * m)                    // If we are well connected, don't try to connect to confirmed deads too often
            {
                addr=CAddress();
                return true;                
            }
            
            std::map<int,CAddrInfo>::iterator it = mapInfo.find(nTriedDead);
            if(it != mapInfo.end())
            {
                nSCSelected=nTriedDead;
                addr=it->second;
                LogPrint("addrman","Selected old address %s, last seen: %.1fhrs\n",addr.ToStringIPPort().c_str(),
                        (double)(nNow - addr.nTime)/3600.0);
                return true;                
            }            
            
            return false;
        }        
    }
    
    return false;            
}
    

CAddress CAddrMan::Select_(int nUnkBias,int nNodes)
{
    if (size() == 0)
        return CAddress();
    
    LogPrint("addrman","Selecting address, total: %d, live: %d, invalid: %d, connected: %d\n",(int)mapInfo.size(),(int)mapSCAlive.size(),nSCTotalBad,nNodes);
    
    CAddress addr;
    if(SCSelect_(nUnkBias,nNodes,addr))
    {
        return addr;
    }

    double nCorTried = sqrt(nTried) * (100.0 - nUnkBias);
    double nCorNew = sqrt(nNew) * nUnkBias;
    if ((nCorTried + nCorNew) * GetRandInt(1 << 30) / (1 << 30) < nCorTried) {
        // use a tried node
        double fChanceFactor = 1.0;
        while (1) {
            int nKBucket = GetRandInt(vvTried.size());
            std::vector<int>& vTried = vvTried[nKBucket];
            if (vTried.size() == 0)
                continue;
            int nPos = GetRandInt(vTried.size());
            assert(mapInfo.count(vTried[nPos]) == 1);
            CAddrInfo& info = mapInfo[vTried[nPos]];
            if (GetRandInt(1 << 30) < fChanceFactor * info.GetChance() * (1 << 30))
            {
                LogPrint("addrman","Selected tried address %s\n",info.ToStringIPPort().c_str());
                return info;
            }
            fChanceFactor *= 1.2;
        }
    } else {
        // use a new node
        double fChanceFactor = 1.0;
        while (1) {
            int nUBucket = GetRandInt(vvNew.size());
            std::set<int>& vNew = vvNew[nUBucket];
            if (vNew.size() == 0)
                continue;
            int nPos = GetRandInt(vNew.size());
            std::set<int>::iterator it = vNew.begin();
            while (nPos--)
                it++;
            assert(mapInfo.count(*it) == 1);
            CAddrInfo& info = mapInfo[*it];
            if (GetRandInt(1 << 30) < fChanceFactor * info.GetChance() * (1 << 30))
            {
                LogPrint("addrman","Selected new address %s\n",info.ToStringIPPort().c_str());
                return info;
            }
            fChanceFactor *= 1.2;
        }
    }
}

#ifdef DEBUG_ADDRMAN
int CAddrMan::Check_()
{
    std::set<int> setTried;
    std::map<int, int> mapNew;

    if (vRandom.size() != nTried + nNew)
        return -7;

    for (std::map<int, CAddrInfo>::iterator it = mapInfo.begin(); it != mapInfo.end(); it++) {
        int n = (*it).first;
        CAddrInfo& info = (*it).second;
        if (info.fInTried) {
            if (!info.nLastSuccess)
                return -1;
            if (info.nRefCount)
                return -2;
            setTried.insert(n);
        } else {
            if (info.nRefCount < 0 || info.nRefCount > ADDRMAN_NEW_BUCKETS_PER_ADDRESS)
                return -3;
            if (!info.nRefCount)
                return -4;
            mapNew[n] = info.nRefCount;
        }
        if (mapAddr[info] != n)
            return -5;
        if (info.nRandomPos < 0 || info.nRandomPos >= vRandom.size() || vRandom[info.nRandomPos] != n)
            return -14;
        if (info.nLastTry < 0)
            return -6;
        if (info.nLastSuccess < 0)
            return -8;
    }

    if (setTried.size() != nTried)
        return -9;
    if (mapNew.size() != nNew)
        return -10;

    for (int n = 0; n < vvTried.size(); n++) {
        std::vector<int>& vTried = vvTried[n];
        for (std::vector<int>::iterator it = vTried.begin(); it != vTried.end(); it++) {
            if (!setTried.count(*it))
                return -11;
            setTried.erase(*it);
        }
    }

    for (int n = 0; n < vvNew.size(); n++) {
        std::set<int>& vNew = vvNew[n];
        for (std::set<int>::iterator it = vNew.begin(); it != vNew.end(); it++) {
            if (!mapNew.count(*it))
                return -12;
            if (--mapNew[*it] == 0)
                mapNew.erase(*it);
        }
    }

    if (setTried.size())
        return -13;
    if (mapNew.size())
        return -15;

    return 0;
}
#endif

void CAddrMan::GetAddr_(std::vector<CAddress>& vAddr)
{
    unsigned int nNodes = ADDRMAN_GETADDR_MAX_PCT * vRandom.size() / 100;

/* MCHN START */    
    bool addTerrible=false;
    if(nNodes < ADDRMAN_GETADDR_MAX_PCT * ADDRMAN_GETADDR_MAX / 100)
    {
        addTerrible=true;
        nNodes=ADDRMAN_GETADDR_MAX_PCT * ADDRMAN_GETADDR_MAX / 100;
        if(nNodes > vRandom.size())
        {
            nNodes=vRandom.size();
        }
    }
/* MCHN END */    
    
    if (nNodes > ADDRMAN_GETADDR_MAX)
        nNodes = ADDRMAN_GETADDR_MAX;

    
    // gather a list of random nodes, skipping those of low quality
    for (unsigned int n = 0; n < vRandom.size(); n++) {
        if (vAddr.size() >= nNodes)
            break;

        int nRndPos = GetRandInt(vRandom.size() - n) + n;
        SwapRandom(n, nRndPos);
        assert(mapInfo.count(vRandom[n]) == 1);

        const CAddrInfo& ai = mapInfo[vRandom[n]];
        if (addTerrible || !ai.IsTerrible())                                    // MCHN
            vAddr.push_back(ai);
    }
}

void CAddrMan::Connected_(const CService& addr, int64_t nTime)
{
    CAddrInfo* pinfo = Find(addr);

    // if not found, bail out
    if (!pinfo)
        return;

    CAddrInfo& info = *pinfo;

    // check whether we are talking about the exact same CService (including same port)
    if (info != addr)
        return;

    // update info
    int64_t nUpdateInterval = 20 * 60;
    if (nTime - info.nTime > nUpdateInterval)
        info.nTime = nTime;
}

void CMCAddrInfo::Init()
{
    memset(&m_NetAddress,0, sizeof(CService));
    m_MCAddress=0;
    m_LastSuccess=0;
    m_Attempts=0;
    m_Flags=0;
    m_PrevRow=0;
    m_LastRow=0;
    m_LastSuccessRow=0;    
    m_LastTry=0;
    m_Reserved1=0;    
    m_Reserved2=0;    
}

void CMCAddrInfo::SetFlag(uint32_t flag,int set_flag)
{
    if(set_flag)
    {
        m_Flags |= flag;
    }
    else
    {
        m_Flags &= ~flag;
    }    
}

void CMCAddrInfo::SetLastRow(uint32_t row)
{
    m_LastRow=row;
}

void CMCAddrInfo::SetPrevRow(uint32_t row)
{
    m_PrevRow=row;    
}

uint32_t CMCAddrInfo::GetFlags()
{
    return m_Flags;
}

uint32_t CMCAddrInfo::GetLastRow()
{
    return m_LastRow;
}

uint32_t CMCAddrInfo::GetPrevRow()
{
    return m_PrevRow;
}

uint160 CMCAddrInfo::GetMCAddress()
{
    return m_MCAddress;
}

CService CMCAddrInfo::GetNetAddress()
{
    return m_NetAddress;
}

int32_t CMCAddrInfo::GetLastTryInfo(int64_t *last_success,int64_t *last_try,uint32_t *last_success_row)
{
    if(last_success)
    {
        *last_success=m_LastSuccess;
    }
    if(last_try)
    {
        *last_try=m_LastTry;
    }
    if(last_success_row)
    {
        *last_success_row=m_LastSuccessRow;
    }
    return m_Attempts;
}

void CMCAddrInfo::Try()
{
    m_LastTry=GetAdjustedTime();
    m_Attempts++;
}

void CMCAddrInfo::ResetLastTry(bool success)
{
    if(success)
    {
        m_LastSuccess=GetAdjustedTime();
        m_Attempts=0;
    }
    m_LastTry=GetAdjustedTime();
}

void CMCAddrInfo::Set(uint32_t row)
{
    if((row>0) || (m_MCAddress != 0))
    {
        m_LastSuccess=GetAdjustedTime();
    }    
    m_LastSuccessRow=row;
    if(IsNet() && (m_MCAddress != 0))
    {
        m_LastRow=row;
    }
    m_Attempts=0;
}

bool CMCAddrInfo::IsNet()
{    
    return (m_NetAddress.GetPort() != 0 ) || !m_NetAddress.IsZero();
}

void CMCAddrMan::Init()
{
    m_RowSize=sizeof(CMCAddrInfo);
    m_MCAddrs.clear();
    m_HashMap.clear();    
    m_MCAddrs.push_back(CMCAddrInfo(CService(),0));
    m_MCAddrs[0].SetPrevRow(1);                                                 // Version
    m_MCAddrs[0].SetLastRow(m_MCAddrs.size());
    m_HashMap.insert(make_pair(0,0));
    m_CurRow=0;
}

uint256 CMCAddrMan::GetHash(const CService &netaddr, uint160 mcaddr)
{    
    uint256 hash;
    if(netaddr.IsZero())
    {
        hash=0;
        memcpy((unsigned char*)&hash+12,&mcaddr,sizeof(uint160));        
    }
    else
    {
        if(netaddr.IsIPv4())
        {
            hash=0;
            memcpy(&hash,(unsigned char*)&netaddr+sizeof(CService)-12,12);
            memcpy((unsigned char*)&hash+12,&mcaddr,sizeof(uint160));
        }
        else
        {
            unsigned char buf[40];
            memset(buf,0,40);
            memcpy(buf,&netaddr,sizeof(CService));
            memcpy(buf+sizeof(CService),&mcaddr,sizeof(uint160));        
            hash = Hash(buf, buf+40);
        }
    }
//    printf("Hash %s\n",hash.ToString().c_str());
    return hash;
}

CMCAddrInfo *CMCAddrMan::Find(const CService &netaddr, uint160 mcaddr)
{
    uint256 hash=GetHash(netaddr,mcaddr);
//    printf("Hash (%s, %s) -> %s\n",netaddr.ToStringIPPort().c_str(),mcaddr.ToString().c_str(),hash.ToString().c_str());
    std::map<uint256, uint32_t>::iterator it = m_HashMap.find(hash);
    if(it == m_HashMap.end())
    {
        return NULL;
    }
    
    return &m_MCAddrs[it->second];
}

void CMCAddrMan::Try(const CService &netaddr, uint160 mcaddr)
{
    CMCAddrInfo *addr;
    addr=Find(netaddr,mcaddr);
    if(addr)
    {
        addr->Try();
    }
}

void CMCAddrMan::Set(const CService &netaddr, uint160 mcaddr)
{
    LOCK(cs);
    
    CMCAddrInfo *addr;
    CMCAddrInfo *netparent=NULL;
    CMCAddrInfo *mcparent=NULL;
    uint32_t row=0;
    
    addr=Find(netaddr,mcaddr);
    
    if(addr)
    {
        row=addr->GetLastRow();
        if(mcaddr != 0)
        {
            mcparent=Find(CService(),mcaddr);
            netparent=Find(netaddr,0);
        }    
    }
    else
    {
        uint256 hash=GetHash(netaddr,mcaddr);
        row=m_MCAddrs.size();
        m_HashMap.insert(make_pair(hash,row));
        
        CMCAddrInfo newaddr=CMCAddrInfo(netaddr,mcaddr);
        m_MCAddrs.push_back(newaddr);        
        addr=&m_MCAddrs[row];        
        if(mcaddr == 0)
        {
            if(fDebug)LogPrint("addrman","mcaddrman: New network address %s\n",addr->GetNetAddress().ToStringIPPort().c_str());
        }
        else
        {
            if(fDebug)LogPrint("addrman","mcaddrman: New full address %s on %s\n",CBitcoinAddress((CKeyID)addr->GetMCAddress()).ToString().c_str(),addr->GetNetAddress().ToStringIPPort().c_str());            
        }
        if(mcaddr != 0)
        {
            mcparent=Find(CService(),mcaddr);
            netparent=Find(netaddr,0);
        }    
        if(mcparent)
        {
            addr->SetPrevRow(mcparent->GetLastRow());
            mcparent->SetLastRow(row);
        }
        m_MCAddrs[0].SetLastRow(m_MCAddrs.size());
    }
    
    if(mcaddr == 0)
    {
        return;
    }
    
    addr->Set(row);
        
    if(mcparent == NULL)
    {
        m_HashMap.insert(make_pair(GetHash(CService(),mcaddr),m_MCAddrs.size()));
        m_MCAddrs.push_back(CMCAddrInfo(CService(),mcaddr));        
        mcparent=&m_MCAddrs[m_MCAddrs.size()-1];
        mcparent->SetLastRow(row);
        m_MCAddrs[0].SetLastRow(m_MCAddrs.size());
    }
    
    mcparent->Set(row);
        
    if(netparent == NULL)
    {
        m_HashMap.insert(make_pair(GetHash(netaddr,0),m_MCAddrs.size()));
        m_MCAddrs.push_back(CMCAddrInfo(netaddr,0));        
        netparent=&m_MCAddrs[m_MCAddrs.size()-1];
        m_MCAddrs[0].SetLastRow(m_MCAddrs.size());
    }
    
    netparent->Set(row);     
}

bool CMCAddrMan::SetOutcome(const CService &netaddr, uint160 mcaddr,bool outcome)
{
    LOCK(cs);
    
    bool result=false;
    if(outcome)
    {
        if(mcaddr == 0)
        {
            if(m_NetAddrConnected.find(netaddr) != m_NetAddrConnected.end())
            {
                result=true;
            }
            else
            {
                m_NetAddrConnected.insert(netaddr);
            }
            CMCAddrInfo *addr;
            addr=Find(netaddr,mcaddr);
            if(addr)
            {
                addr->ResetLastTry(true);
            }            
        }
        else
        {
            if(m_NetAddrConnected.find(netaddr) == m_NetAddrConnected.end())
            {
                m_NetAddrConnected.insert(netaddr);
            }
            if(m_MCAddrConnected.find(mcaddr) != m_MCAddrConnected.end())
            {
                result=true;
            }
            else
            {
                m_MCAddrConnected.insert(mcaddr);
            }            
        }
        Set(netaddr,mcaddr);        
    }
    else
    {
        Try(netaddr,mcaddr);
    }
    
    return result;
}

uint32_t CMCAddrMan::Load()
{
    LOCK(cs);
    
    int FileHan;
    char FileName[MC_DCT_DB_MAX_PATH];                      
    CMCAddrInfo addr;
    int64_t file_size;
    uint32_t row_count,row;
    
    mc_GetFullFileName(mc_gState->m_Params->NetworkName(),"addrs",".dat",MC_FOM_RELATIVE_TO_DATADIR | MC_FOM_CREATE_DIR,FileName);
    FileHan=open(FileName,_O_BINARY | O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    
    if(FileHan<=0)
    {
        return 0;
    }    
    
    file_size=lseek64(FileHan,0,SEEK_END);
    
    lseek64(FileHan,0,SEEK_SET);
    if(read(FileHan,&addr,m_RowSize) != m_RowSize)
    {        
        close(FileHan);    
        return 0;
    }
     
    row_count=addr.GetLastRow();
    if((addr.GetPrevRow() > 1) || (file_size != m_RowSize * row_count))    // Not supported version or corrupted
    {
        close(FileHan);    
        return 0;        
    }
    
    m_MCAddrs.clear();
    m_HashMap.clear();    
    m_MCAddrs.push_back(addr);
    m_HashMap.insert(make_pair(0,0));
    
    row=1;
    while((read(FileHan,&addr,m_RowSize) == m_RowSize))
    {
        uint256 hash=GetHash(addr.GetNetAddress(),addr.GetMCAddress());
        m_HashMap.insert(make_pair(hash,row));
        m_MCAddrs.push_back(addr);        
        row++;        
    }
        
    if(row != row_count)
    {
        printf("Corrupted \n");
        Init();
    }
    
    if(fDebug)LogPrint("mcaddrman","mchn: Loaded %d addresses\n",(int)m_MCAddrs.size()-1);
        
    close(FileHan);    
    return 0;
}

void CMCAddrMan::Save() const
{
    LOCK(cs);
    
    int FileHan;
    char FileName[MC_DCT_DB_MAX_PATH];                      
    
    mc_GetFullFileName(mc_gState->m_Params->NetworkName(),"addrs",".dat",MC_FOM_RELATIVE_TO_DATADIR | MC_FOM_CREATE_DIR,FileName);
    FileHan=open(FileName,_O_BINARY | O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    
    if(FileHan<=0)
    {
        return;
    }    
    
    for(unsigned int row=0;row<m_MCAddrs.size();row++)        
    {
        write(FileHan,&m_MCAddrs[row],m_RowSize);        
    }
    if(fDebug)LogPrint("mcaddrman","mchn: Saved %d addresses\n",(int)m_MCAddrs.size());
    __US_FlushFile(FileHan);    
    close(FileHan);    
}

CMCAddrInfo *CMCAddrMan::Select(uint160 mcaddr,uint32_t mode)
{
/*    
        if(mcaddr == 0)
        {
            if(m_NetAddrTried.find(netaddr) == m_NetAddrTried.end())
            {
                m_NetAddrTried.insert(netaddr);
            }
        }
        else
        {
            if(m_MCAddrTried.find(mcaddr) == m_MCAddrTried.end())
            {
                m_MCAddrTried.insert(mcaddr);
            }            
        }
 */ 
    return NULL;
}

void CMCAddrMan::Reset()
{
    m_CurRow=0;
}

CMCAddrInfo *CMCAddrMan::Next()
{
    m_CurRow++;
    if(m_CurRow<m_MCAddrs.size())
    {
        return &m_MCAddrs[m_CurRow];
    }
    return NULL;
}

bool CMCAddrInfoCompareByAttempts(CMCAddrInfo a,CMCAddrInfo b)
{ 
    int a_attempts,b_attempts;
    a_attempts=a.GetLastTryInfo(NULL,NULL,NULL);
    b_attempts=b.GetLastTryInfo(NULL,NULL,NULL);
        
    if(a_attempts <= b_attempts)
    {
        return true;
    }
    
    return false;
}

bool CMCAddrInfoCompareByLastSuccess(CMCAddrInfo a,CMCAddrInfo b)
{ 
    int64_t a_lastsuccess,b_lastsuccess;
    a.GetLastTryInfo(&a_lastsuccess,NULL,NULL);
    b.GetLastTryInfo(&b_lastsuccess,NULL,NULL);
    
    if(a_lastsuccess >= b_lastsuccess)
    {
        return true;
    }
    
    return false;
}

int64_t mc_PeerStatusDelayIgnore()
{
    return 90*86400;
}

int64_t mc_PeerStatusDelayLastTry()
{
    return 86400;
}

int64_t mc_PeerStatusDelayForget()
{
    return 90*86400;
}

uint32_t CMCAddrMan::PrepareSelect(set<CService> setLocalAddr,uint32_t *counts)
{
    LOCK(cs);
    
    m_MCAddrConnected.clear();
    m_MCAddrTried.clear();
    m_NetAddrConnected.clear();
    m_NetAddrTried.clear();

    uint32_t nOutbound=0;
    
    {
        LOCK(cs_vNodes);
        for(int pass=0;pass<2;pass++)
        {
            BOOST_FOREACH(CNode* pnode, vNodes) 
            {                
                CService naddr;
                if(pass == 0)
                {
                    if (!pnode->fInbound) 
                    {
                        nOutbound++;
                    }
                }
                if(pnode->kAddrRemote != 0)
                {
                    bool take_it=false;
                    if(pnode->kAddrLocal < pnode->kAddrRemote)
                    {
                        if(pass == 0)
                        {
                            take_it=true;
                        }
                    }
                    else
                    {
                        if(pass == 1)
                        {
                            take_it=true;
                        }                        
                    }
                    if(take_it)
                    {
                        if (pnode->fInbound) 
                        {                
                            if (((CNetAddr)pnode->addr) == (CNetAddr)pnode->addrFromVersion)
                            {
                                naddr=pnode->addrFromVersion;
                            }
                        }
                        else
                        {
                            naddr=pnode->addr;
                        }
                        if(!naddr.IsZero())
                        {
                            if(m_NetAddrConnected.find(naddr) != m_NetAddrConnected.end())
                            {
                                LogPrintf("mchn: Duplicate connection to %s, disconnecting peer %d\n", naddr.ToStringIPPort().c_str(), pnode->id);
                                pnode->fDisconnect=true;
                            }
                        }
/*                                                                              // Allow connection to several network addresses with the same MC address
                        if(m_MCAddrConnected.find((uint160)pnode->kAddrRemote) != m_MCAddrConnected.end())
                        {
                            LogPrintf("mchn: Duplicate connection to %s, disconnecting peer %d\n", CBitcoinAddress((CKeyID)(pnode->kAddrRemote)).ToString().c_str(), pnode->id);
                            pnode->fDisconnect=true;                    
                        }                
*/
                        if(!pnode->fDisconnect)
                        {
                            if(!naddr.IsZero())
                            {
                                m_NetAddrConnected.insert(naddr);
                            }
                            m_MCAddrConnected.insert((uint160)pnode->kAddrRemote);                    
                        }
                    }
                }
            }        
        }
    }
    
    for(int mode=0;mode<MC_AMM_MODE_COUNT;mode++)
    {
        m_Selected[mode].clear();
        m_Position[mode]=0;
    }
    
    for(uint32_t row=0;row<m_MCAddrs.size();row++)        
    {
        int mode=-1;
        CMCAddrInfo *addr;
        addr=&m_MCAddrs[row];
        
        bool take_it=false;
        if(addr->IsNet())
        {
            take_it=true;
            CService naddr=addr->GetNetAddress();            
            if (!naddr.IsValid() || (IsLocal(naddr) && (naddr.GetPort() == GetListenPort())))
            {
                take_it=false;
            }

            if(setLocalAddr.find(naddr) != setLocalAddr.end())
            {
                take_it=false;
            }            
            if(m_NetAddrConnected.find(naddr) != m_NetAddrConnected.end())
            {
                take_it=false;                
            }            
/*            
            if(addr->GetMCAddress() != 0)
            {
                if(m_MCAddrConnected.find(addr->GetMCAddress()) != m_MCAddrConnected.end())
                {
                    take_it=false;                                    
                }                
            }
 */ 
            if( addr->GetFlags() & MC_AMF_IGNORED )
            {
                take_it=false;                                
            }
            else
            {
                int64_t lastsuccess,lasttry;
                int attempts=addr->GetLastTryInfo(&lastsuccess,&lasttry,NULL);
                int64_t time_now=GetAdjustedTime();
                if(attempts > 0)
                {
                    if( (lastsuccess > 0) || (attempts > 120) )
                    {
                        if(lastsuccess < time_now - mc_PeerStatusDelayIgnore())
                        {
                            if(lasttry > time_now - mc_PeerStatusDelayLastTry())
                            {
                                addr->SetFlag(MC_AMF_IGNORED,1);
                                if(addr->GetMCAddress() == 0)
                                {
                                    if(fDebug)LogPrint("addrman", "Address %s didn't respond for %d days, stop trying\n", addr->GetNetAddress().ToStringIPPort().c_str(),(time_now - lastsuccess + 1)/86400);
                                }
                                take_it=false;
                            }
                        }
                    }
                }
            }
        }
        
        if(take_it)
        {
            int64_t lastsuccess,lasttry;
            int attempts;
            attempts=addr->GetLastTryInfo(&lastsuccess,&lasttry,NULL);
            if(addr->GetMCAddress() == 0)
            {
                if(lastsuccess == 0)
                {
                    if(attempts == 0)
                    {
                        mode = MC_AMM_NEW_NET;
                    }
                    else
                    {
                        mode = MC_AMM_TRIED_NET;
                    }
                }
                else
                {
                    if(attempts == 0)
                    {
                        mode = MC_AMM_RECENT_SUCCESS;
                    }
                    else
                    {
                        mode = MC_AMM_RECENT_FAIL;
                    }                    
                }
            }
/*            
            else
            {
                if(attempts == 0)
                {
                    mode = MC_AMM_RECENT_SUCCESS;                    
                }                
                else
                {
                    CMCAddrInfo *netparent=Find(addr->GetNetAddress(),0);
                    CMCAddrInfo *mcparent=Find(CService(),addr->GetMCAddress());
                    uint32_t parent_lastsuccess_row;
                    if(netparent)
                    {
                        netparent->GetLastTryInfo(NULL,NULL,&parent_lastsuccess_row);
                        if(parent_lastsuccess_row == addr->GetLastRow())
                        {
                            if(mcparent)
                            {
//                                mcparent->GetLastTryInfo(NULL,NULL,&parent_lastsuccess_row);
//                                if(parent_lastsuccess_row == addr->GetLastRow())
                                {
                                    if(mcparent->GetLastRow() == addr->GetLastRow())
                                    {
                                        mode = MC_AMM_RECENT_FAIL;
                                    }
                                    else
                                    {
//                                        mode = MC_AMM_OLD_FAIL;               // This net addrsss should appear as MC_AMM_RECENT_FAIL if MC address is not checked in selection      
                                    }
                                }
                            }
                        }
                    }
                }
            }
 */ 
        }
        if(mode>=0)
        {
            m_Selected[mode].push_back(addr);
        }
    }
    
    random_shuffle(m_Selected[MC_AMM_RECENT_SUCCESS].begin(), m_Selected[MC_AMM_RECENT_SUCCESS].end());
    random_shuffle(m_Selected[MC_AMM_NEW_NET].begin(), m_Selected[MC_AMM_NEW_NET].end());
    sort(m_Selected[MC_AMM_TRIED_NET].begin(), m_Selected[MC_AMM_TRIED_NET].end(), CMCAddrInfoCompareByAttempts);
    sort(m_Selected[MC_AMM_RECENT_FAIL].begin(), m_Selected[MC_AMM_RECENT_FAIL].end(), CMCAddrInfoCompareByLastSuccess);
    sort(m_Selected[MC_AMM_OLD_FAIL].begin(), m_Selected[MC_AMM_OLD_FAIL].end(), CMCAddrInfoCompareByLastSuccess);
    
    if(counts)
    {
        for(int mode=0;mode<MC_AMM_MODE_COUNT;mode++)
        {
            counts[mode]=m_Selected[mode].size();
        }
    }
    
   if(fDebug)LogPrint("addrman-minor","Prepared for select: %u recent successes, %u recent fails, %u new, %u old, %u never successful\n",
            m_Selected[MC_AMM_RECENT_SUCCESS].size(),m_Selected[MC_AMM_RECENT_FAIL].size(),m_Selected[MC_AMM_NEW_NET].size(),
            m_Selected[MC_AMM_OLD_FAIL].size(),m_Selected[MC_AMM_TRIED_NET].size());
    
    return nOutbound;
}

CMCAddrInfo *CMCAddrMan::Select(int mode)
{
    if(mode >= MC_AMM_MODE_COUNT)
    {
        return NULL;
    }
    
    while(m_Position[mode] < m_Selected[mode].size())
    {
        CMCAddrInfo *addr=&(m_Selected[mode][m_Position[mode]]);
        m_Position[mode]+=1;
        bool take_it=true;
        
        if(take_it)
        {
            if(m_NetAddrConnected.find(addr->GetNetAddress()) != m_NetAddrConnected.end())
            {
                take_it=false;        
            }
        }
        if(take_it)
        {
            if(m_NetAddrTried.find(addr->GetNetAddress()) != m_NetAddrTried.end())
            {
                take_it=false;        
            }
        }
        if(addr->GetMCAddress() != 0)
        {
            if(take_it)
            {
                if(m_MCAddrConnected.find(addr->GetMCAddress()) != m_MCAddrConnected.end())
                {
                    take_it=false;        
                }
            }
/*            
            if(take_it)
            {
                if(m_MCAddrTried.find(addr->GetMCAddress()) != m_MCAddrTried.end())
                {
                    take_it=false;        
                }
            }
 */ 
        }
        if(take_it)
        {
            m_NetAddrTried.insert(addr->GetNetAddress());
            if(addr->GetMCAddress() != 0)
            {
                if((mode == MC_AMM_RECENT_SUCCESS) || (mode == MC_AMM_RECENT_FAIL))
                {
                    m_MCAddrTried.insert(addr->GetMCAddress());                
                }
            }            
            return addr;
        }
    }    
    
    return NULL;
}
