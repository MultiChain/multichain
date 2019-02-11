// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "chainparams/chainparams.h"

#include "utils/random.h"
#include "utils/util.h"
#include "utils/utilstrencodings.h"

#include <assert.h>

#include "multichain/multichain.h"

#include <boost/assign/list_of.hpp>

using namespace std;
using namespace boost::assign;

struct SeedSpec6 {
    uint8_t addr[16];
    uint16_t port;
};

#include "chainparams/chainparamsseeds.h"

/**
 * Main network
 */

//! Convert the pnSeeds6 array into usable address objects.
static void convertSeed6(std::vector<CAddress> &vSeedsOut, const SeedSpec6 *data, unsigned int count)
{
    // It'll only connect to one or two seed nodes because once it connects,
    // it'll get a pile of addresses with newer timestamps.
    // Seed nodes are given a random 'last seen time' of between one and two
    // weeks ago.
    const int64_t nOneWeek = 7*24*60*60;
    for (unsigned int i = 0; i < count; i++)
    {
        struct in6_addr ip;
        memcpy(&ip, data[i].addr, sizeof(ip));
        CAddress addr(CService(ip, data[i].port));
        addr.nTime = GetTime() - GetRand(nOneWeek) - nOneWeek;
        vSeedsOut.push_back(addr);
    }
}

/**
 * What makes a good checkpoint block?
 * + Is surrounded by blocks with reasonable timestamps
 *   (no blocks before with a timestamp after, none after with
 *    timestamp before)
 * + Contains no strange transactions
 */
static Checkpoints::MapCheckpoints mapCheckpoints =
        boost::assign::map_list_of
        ( 11111, uint256("0x0000000069e244f73d78e8fd29ba2fd2ed618bd6fa2ee92559f542fdb26e7c1d"))
        ( 33333, uint256("0x000000002dd5588a74784eaa7ab0507a18ad16a236e7b1ce69f00d7ddfb5d0a6"))
        ( 74000, uint256("0x0000000000573993a3c9e41ce34471c079dcf5f52a0e824a81e7f953b8661a20"))
        (105000, uint256("0x00000000000291ce28027faea320c8d2b054b2e0fe44a773f3eefb151d6bdc97"))
        (134444, uint256("0x00000000000005b12ffd4cd315cd34ffd4a594f430ac814c91184a0d42d2b0fe"))
        (168000, uint256("0x000000000000099e61ea72015e79632f216fe6cb33d7899acb35b75c8303b763"))
        (193000, uint256("0x000000000000059f452a5f7340de6682a977387c17010ff6e6c3bd83ca8b1317"))
        (210000, uint256("0x000000000000048b95347e83192f69cf0366076336c639f9b7228e9ba171342e"))
        (216116, uint256("0x00000000000001b4f4b433e81ee46494af945cf96014816a4e2370f11b23df4e"))
        (225430, uint256("0x00000000000001c108384350f74090433e7fcf79a606b8e797f065b130575932"))
        (250000, uint256("0x000000000000003887df1f29024b06fc2200b55f8af8f35453d7be294df2d214"))
        (279000, uint256("0x0000000000000001ae8c72a0b0c301f67e3afca10e819efa9041e458e9bd7e40"))
        (295000, uint256("0x00000000000000004d9b4ef50f0f9d686fd69db2e03af35a100370c64632a983"))
        ;
static const Checkpoints::CCheckpointData data = {
        &mapCheckpoints,
        1397080064, // * UNIX timestamp of last checkpoint block
        36544669,   // * total number of transactions between genesis and last checkpoint
                    //   (the tx=... number in the SetBestChain debug.log lines)
        60000.0     // * estimated number of transactions per day after checkpoint
    };

static Checkpoints::MapCheckpoints mapCheckpointsTestnet =
        boost::assign::map_list_of
        ( 546, uint256("000000002a936ca763904c3c35fce2f3556c559c0214345d31b1bcebf76acb70"))
        ;
static const Checkpoints::CCheckpointData dataTestnet = {
        &mapCheckpointsTestnet,
        1337966069,
        1488,
        300
    };

/* MCHN START */
static Checkpoints::MapCheckpoints mapCheckpointsMultichain =
        boost::assign::map_list_of
        ( -1, uint256("0f9188f13cb7b2c71f2a335e3a4fc328bf5beb436012afca590b1a11466e2206"))
        ;
static const Checkpoints::CCheckpointData dataMultichain = {
        &mapCheckpointsMultichain,
        0,
        0,
        0
    };
/* MCHN END */
static Checkpoints::MapCheckpoints mapCheckpointsRegtest =
        boost::assign::map_list_of
        ( 0, uint256("0f9188f13cb7b2c71f2a335e3a4fc328bf5beb436012afca590b1a11466e2206"))
        ;
static const Checkpoints::CCheckpointData dataRegtest = {
        &mapCheckpointsRegtest,
        0,
        0,
        0
    };

class CMainParams : public CChainParams {
public:
    CMainParams() {
        networkID = CBaseChainParams::MAIN;
        strNetworkID = "main";
        /** 
         * The message start string is designed to be unlikely to occur in normal data.
         * The characters are rarely used upper ASCII, not valid as UTF-8, and produce
         * a large 4-byte int at any alignment.
         */
        pchMessageStart[0] = 0xf9;
        pchMessageStart[1] = 0xbe;
        pchMessageStart[2] = 0xb4;
        pchMessageStart[3] = 0xd9;
        vAlertPubKey = ParseHex("04fc9702847840aaf195de8442ebecedf5b095cdbb9bc716bda9110971b28a49e0ead8564ff0db22209e0374782c093bb899692d524e9d6a6956e7c5ecbcd68284");
        nDefaultPort = 8333;
        bnProofOfWorkLimit = ~uint256(0) >> 32;
        nSubsidyHalvingInterval = 210000;
        nEnforceBlockUpgradeMajority = 750;
        nRejectBlockOutdatedMajority = 950;
        nToCheckBlockUpgradeMajority = 1000;
        nMinerThreads = 0;
        nTargetTimespan = 14 * 24 * 60 * 60; // two weeks
        nTargetSpacing = 10 * 60;

        /**
         * Build the genesis block. Note that the output of the genesis coinbase cannot
         * be spent as it did not originally exist in the database.
         * 
         * CBlock(hash=000000000019d6, ver=1, hashPrevBlock=00000000000000, hashMerkleRoot=4a5e1e, nTime=1231006505, nBits=1d00ffff, nNonce=2083236893, vtx=1)
         *   CTransaction(hash=4a5e1e, ver=1, vin.size=1, vout.size=1, nLockTime=0)
         *     CTxIn(COutPoint(000000, -1), coinbase 04ffff001d0104455468652054696d65732030332f4a616e2f32303039204368616e63656c6c6f72206f6e206272696e6b206f66207365636f6e64206261696c6f757420666f722062616e6b73)
         *     CTxOut(nValue=50.00000000, scriptPubKey=0x5F1DF16B2B704C8A578D0B)
         *   vMerkleTree: 4a5e1e
         */
        const char* pszTimestamp = "The Times 03/Jan/2009 Chancellor on brink of second bailout for banks";
        CMutableTransaction txNew;
        txNew.vin.resize(1);
        txNew.vout.resize(1);
        txNew.vin[0].scriptSig = CScript() << 486604799 << CScriptNum(4) << vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
        txNew.vout[0].nValue = 50 * COIN;
        txNew.vout[0].scriptPubKey = CScript() << ParseHex("04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38c4f35504e51ec112de5c384df7ba0b8d578a4c702b6bf11d5f") << OP_CHECKSIG;
        genesis.vtx.push_back(txNew);
        genesis.hashPrevBlock = 0;
        genesis.hashMerkleRoot = genesis.BuildMerkleTree();
        genesis.nVersion = 1;
        genesis.nTime    = 1231006505;
        genesis.nBits    = 0x1d00ffff;
        genesis.nNonce   = 2083236893;

        hashGenesisBlock = genesis.GetHash();
        assert(hashGenesisBlock == uint256("0x000000000019d6689c085ae165831e934ff763ae46a2a6c172b3f1b60a8ce26f"));
        assert(genesis.hashMerkleRoot == uint256("0x4a5e1e4baab89f3a32518a88c31bc87f618f76673e2cc77ab2127b7afdeda33b"));

        vSeeds.push_back(CDNSSeedData("bitcoin.sipa.be", "seed.bitcoin.sipa.be"));
        vSeeds.push_back(CDNSSeedData("bluematt.me", "dnsseed.bluematt.me"));
        vSeeds.push_back(CDNSSeedData("dashjr.org", "dnsseed.bitcoin.dashjr.org"));
        vSeeds.push_back(CDNSSeedData("bitcoinstats.com", "seed.bitcoinstats.com"));
        vSeeds.push_back(CDNSSeedData("xf2.org", "bitseed.xf2.org"));

/*        
        base58Prefixes[PUBKEY_ADDRESS] = list_of(0);
        base58Prefixes[SCRIPT_ADDRESS] = list_of(5);
        base58Prefixes[SECRET_KEY] =     list_of(128);
        base58Prefixes[EXT_PUBLIC_KEY] = list_of(0x04)(0x88)(0xB2)(0x1E);
        base58Prefixes[EXT_SECRET_KEY] = list_of(0x04)(0x88)(0xAD)(0xE4);
*/
        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,0);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,5);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,128);
        base58Prefixes[EXT_PUBLIC_KEY].clear();
        base58Prefixes[EXT_PUBLIC_KEY].push_back(0x04);
        base58Prefixes[EXT_PUBLIC_KEY].push_back(0x88);
        base58Prefixes[EXT_PUBLIC_KEY].push_back(0xB2);
        base58Prefixes[EXT_PUBLIC_KEY].push_back(0x1E);
        base58Prefixes[EXT_SECRET_KEY].clear();
        base58Prefixes[EXT_SECRET_KEY].push_back(0x04);
        base58Prefixes[EXT_SECRET_KEY].push_back(0x88);
        base58Prefixes[EXT_SECRET_KEY].push_back(0xAD);
        base58Prefixes[EXT_SECRET_KEY].push_back(0xE4);
                
        
        convertSeed6(vFixedSeeds, pnSeed6_main, ARRAYLEN(pnSeed6_main));

        fRequireRPCPassword = true;
        fMiningRequiresPeers = true;
        dMineEmptyRounds = -1.;
        dMiningTurnover=1.0;
        fDefaultCheckMemPool = false;
        fAllowMinDifficultyBlocks = false;
        fRequireStandard = true;
        fMineBlocksOnDemand = false;
        fSkipProofOfWorkCheck = false;
        fTestnetToBeDeprecatedFieldRPC = false;
        fDisallowUnsignedBlockNonce = false;
    }

    const Checkpoints::CCheckpointData& Checkpoints() const 
    {
        return data;
    }
};
static CMainParams mainParams;

/**
 * Testnet (v3)
 */
class CTestNetParams : public CMainParams {
public:
    CTestNetParams() {
        networkID = CBaseChainParams::TESTNET;
        strNetworkID = "test";
        pchMessageStart[0] = 0x0b;
        pchMessageStart[1] = 0x11;
        pchMessageStart[2] = 0x09;
        pchMessageStart[3] = 0x07;
        vAlertPubKey = ParseHex("04302390343f91cc401d56d68b123028bf52e5fca1939df127f63c6467cdf9c8e2c14b61104cf817d0b780da337893ecc4aaff1309e536162dabbdb45200ca2b0a");
        nDefaultPort = 18333;
        nEnforceBlockUpgradeMajority = 51;
        nRejectBlockOutdatedMajority = 75;
        nToCheckBlockUpgradeMajority = 100;
        nMinerThreads = 0;
        nTargetTimespan = 14 * 24 * 60 * 60; //! two weeks
        nTargetSpacing = 10 * 60;

        //! Modify the testnet genesis block so the timestamp is valid for a later start.
        genesis.nTime = 1296688602;
        genesis.nNonce = 414098458;
        hashGenesisBlock = genesis.GetHash();
        assert(hashGenesisBlock == uint256("0x000000000933ea01ad0ee984209779baaec3ced90fa3f408719526f8d77f4943"));

        vFixedSeeds.clear();
        vSeeds.clear();
        vSeeds.push_back(CDNSSeedData("alexykot.me", "testnet-seed.alexykot.me"));
        vSeeds.push_back(CDNSSeedData("bitcoin.petertodd.org", "testnet-seed.bitcoin.petertodd.org"));
        vSeeds.push_back(CDNSSeedData("bluematt.me", "testnet-seed.bluematt.me"));
        vSeeds.push_back(CDNSSeedData("bitcoin.schildbach.de", "testnet-seed.bitcoin.schildbach.de"));

/*        
        base58Prefixes[PUBKEY_ADDRESS] = list_of(111);
        base58Prefixes[SCRIPT_ADDRESS] = list_of(196);
        base58Prefixes[SECRET_KEY]     = list_of(239);
        base58Prefixes[EXT_PUBLIC_KEY] = list_of(0x04)(0x35)(0x87)(0xCF);
        base58Prefixes[EXT_SECRET_KEY] = list_of(0x04)(0x35)(0x83)(0x94);
*/
        
        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,111);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,196);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,239);
        base58Prefixes[EXT_PUBLIC_KEY].clear();
        base58Prefixes[EXT_PUBLIC_KEY].push_back(0x04);
        base58Prefixes[EXT_PUBLIC_KEY].push_back(0x35);
        base58Prefixes[EXT_PUBLIC_KEY].push_back(0x87);
        base58Prefixes[EXT_PUBLIC_KEY].push_back(0xCF);
        base58Prefixes[EXT_SECRET_KEY].clear();
        base58Prefixes[EXT_SECRET_KEY].push_back(0x04);
        base58Prefixes[EXT_SECRET_KEY].push_back(0x35);
        base58Prefixes[EXT_SECRET_KEY].push_back(0x83);
        base58Prefixes[EXT_SECRET_KEY].push_back(0x94);
        

        convertSeed6(vFixedSeeds, pnSeed6_test, ARRAYLEN(pnSeed6_test));

        fRequireRPCPassword = true;
        fMiningRequiresPeers = true;
        dMineEmptyRounds = -1.;
        dMiningTurnover=1.0;
        fDefaultCheckMemPool = false;
        fAllowMinDifficultyBlocks = true;
        fRequireStandard = false;
        fMineBlocksOnDemand = false;
        fTestnetToBeDeprecatedFieldRPC = true;
    }
    const Checkpoints::CCheckpointData& Checkpoints() const 
    {
        return dataTestnet;
    }
};
static CTestNetParams testNetParams;

/**
 * Regression test
 */
class CRegTestParams : public CTestNetParams {
public:
    CRegTestParams() {
        networkID = CBaseChainParams::REGTEST;
        strNetworkID = "regtest";
        pchMessageStart[0] = 0xfa;
        pchMessageStart[1] = 0xbf;
        pchMessageStart[2] = 0xb5;
        pchMessageStart[3] = 0xda;
        nSubsidyHalvingInterval = 150;
        nEnforceBlockUpgradeMajority = 750;
        nRejectBlockOutdatedMajority = 950;
        nToCheckBlockUpgradeMajority = 1000;
        nMinerThreads = 1;
        nTargetTimespan = 14 * 24 * 60 * 60; //! two weeks
        nTargetSpacing = 10 * 60;
        bnProofOfWorkLimit = ~uint256(0) >> 1;
        genesis.nTime = 1296688602;
        genesis.nBits = 0x207fffff;
        genesis.nNonce = 2;
        hashGenesisBlock = genesis.GetHash();
        nDefaultPort = 18444;
        assert(hashGenesisBlock == uint256("0x0f9188f13cb7b2c71f2a335e3a4fc328bf5beb436012afca590b1a11466e2206"));

        vFixedSeeds.clear(); //! Regtest mode doesn't have any fixed seeds.
        vSeeds.clear();  //! Regtest mode doesn't have any DNS seeds.

        fRequireRPCPassword = false;
        fMiningRequiresPeers = false;
        dMineEmptyRounds = -1.;
        dMiningTurnover=1.0;
        fDefaultCheckMemPool = true;
        fAllowMinDifficultyBlocks = true;
        fRequireStandard = false;
        fMineBlocksOnDemand = true;
        fTestnetToBeDeprecatedFieldRPC = false;
    }
    const Checkpoints::CCheckpointData& Checkpoints() const 
    {
        return dataRegtest;
    }
};
static CRegTestParams regTestParams;

/**
 * Unit test
 */
class CUnitTestParams : public CMainParams, public CModifiableParams {
public:
    CUnitTestParams() {
        networkID = CBaseChainParams::UNITTEST;
        strNetworkID = "unittest";
        nDefaultPort = 18445;
        vFixedSeeds.clear(); //! Unit test mode doesn't have any fixed seeds.
        vSeeds.clear();  //! Unit test mode doesn't have any DNS seeds.

        fRequireRPCPassword = false;
        fMiningRequiresPeers = false;
        dMineEmptyRounds = -1.;
        dMiningTurnover=1.0;
        fDefaultCheckMemPool = true;
        fAllowMinDifficultyBlocks = false;
        fMineBlocksOnDemand = true;
    }

    const Checkpoints::CCheckpointData& Checkpoints() const 
    {
        // UnitTest share the same checkpoints as MAIN
        return data;
    }

    //! Published setters to allow changing values in unit test cases
    virtual void setSubsidyHalvingInterval(int anSubsidyHalvingInterval)  { nSubsidyHalvingInterval=anSubsidyHalvingInterval; }
    virtual void setEnforceBlockUpgradeMajority(int anEnforceBlockUpgradeMajority)  { nEnforceBlockUpgradeMajority=anEnforceBlockUpgradeMajority; }
    virtual void setRejectBlockOutdatedMajority(int anRejectBlockOutdatedMajority)  { nRejectBlockOutdatedMajority=anRejectBlockOutdatedMajority; }
    virtual void setToCheckBlockUpgradeMajority(int anToCheckBlockUpgradeMajority)  { nToCheckBlockUpgradeMajority=anToCheckBlockUpgradeMajority; }
    virtual void setDefaultCheckMemPool(bool afDefaultCheckMemPool)  { fDefaultCheckMemPool=afDefaultCheckMemPool; }
    virtual void setAllowMinDifficultyBlocks(bool afAllowMinDifficultyBlocks) {  fAllowMinDifficultyBlocks=afAllowMinDifficultyBlocks; }
    virtual void setSkipProofOfWorkCheck(bool afSkipProofOfWorkCheck) { fSkipProofOfWorkCheck = afSkipProofOfWorkCheck; }
};
static CUnitTestParams unitTestParams;


static CChainParams *pCurrentParams = 0;

CModifiableParams *ModifiableParams()
{
   assert(pCurrentParams);
   assert(pCurrentParams==&unitTestParams);
   return (CModifiableParams*)&unitTestParams;
}

const CChainParams &Params() {
    assert(pCurrentParams);
    return *pCurrentParams;
}

CChainParams &Params(CBaseChainParams::Network network) {
    switch (network) {
        case CBaseChainParams::MAIN:
            return mainParams;
        case CBaseChainParams::TESTNET:
            return testNetParams;
        case CBaseChainParams::REGTEST:
            return regTestParams;
        case CBaseChainParams::UNITTEST:
            return unitTestParams;
        default:
            assert(false && "Unimplemented network");
            return mainParams;
    }
}

void SelectParams(CBaseChainParams::Network network) {
    SelectBaseParams(network);
    pCurrentParams = &Params(network);
}

bool SelectParamsFromCommandLine()
{
    CBaseChainParams::Network network = NetworkIdFromCommandLine();
    if (network == CBaseChainParams::MAX_NETWORK_TYPES)
        return false;

    SelectParams(network);
    return true;
}

/* MCHN START */

class CMultiChainParams : public CMainParams {
public:
    CMultiChainParams() {    };
    
    void SetFixedParams(const char *NetworkName)
    {
        networkID = CBaseChainParams::MULTICHAIN;
        strNetworkID=NetworkName;
        nMinerThreads = 0;
        fDefaultCheckMemPool = false;
        fRequireRPCPassword = false;
        
        const unsigned char *ucPtr;
    
        nDefaultPort=MC_DEFAULT_NETWORK_PORT;
        
        ucPtr=mc_gState->m_NetworkParams->DefaultMessageStart();
        
        pchMessageStart[0] = ucPtr[0];
        pchMessageStart[1] = ucPtr[1];
        pchMessageStart[2] = ucPtr[2];
        pchMessageStart[3] = ucPtr[3];
        
        vFixedSeeds.clear();
        vSeeds.clear();
    }
    
    void Initialize(){
        
        switch(mc_gState->m_NetworkParams->m_Status)
        {
            case MC_PRM_STATUS_GENERATED:
            case MC_PRM_STATUS_MINIMAL:
            case MC_PRM_STATUS_VALID:
                break;
            default:
                return;
        }
        
        const unsigned char *ucPtr;
        int size;
        
        networkID = CBaseChainParams::MULTICHAIN;
        
        strNetworkID = string(mc_gState->m_NetworkParams->Name());
                
        ucPtr=mc_gState->m_NetworkParams->MessageStart();
        pchMessageStart[0] = ucPtr[0];
        pchMessageStart[1] = ucPtr[1];
        pchMessageStart[2] = ucPtr[2];
        pchMessageStart[3] = ucPtr[3];
        
                
//        vAlertPubKey = ParseHex("04302390343f91cc401d56d68b123028bf52e5fca1939df127f63c6467cdf9c8e2c14b61104cf817d0b780da337893ecc4aaff1309e536162dabbdb45200ca2b0a");
        
        nDefaultPort = (int)mc_gState->m_NetworkParams->GetInt64Param("defaultnetworkport");
        bnProofOfWorkLimit = ~uint256(0) >> (int)mc_gState->m_NetworkParams->GetInt64Param("powminimumbits");
        nSubsidyHalvingInterval = (int)(mc_gState->m_NetworkParams->GetInt64Param("rewardhalvinginterval"));

        
/*  // MCHN-TODO currently copied from main, decide what to do with it later.
        nEnforceBlockUpgradeMajority = 51;
        nRejectBlockOutdatedMajority = 75;
        nToCheckBlockUpgradeMajority = 100;
 */ 
  

        
        nTargetTimespan = (int)mc_gState->m_NetworkParams->GetInt64Param("targetadjustfreq"); 
        nTargetSpacing = (int)mc_gState->m_NetworkParams->GetInt64Param("targetblocktime");

        vFixedSeeds.clear();
        vSeeds.clear();

        
        ucPtr=(const unsigned char*)mc_gState->m_NetworkParams->GetParam("addresspubkeyhashversion",&size);        
        if(ucPtr)
        {
            base58Prefixes[PUBKEY_ADDRESS].clear();
            for(int i=0;i<size;i++)
            {
                base58Prefixes[PUBKEY_ADDRESS].push_back(ucPtr[i]);
            }
        }
                
        ucPtr=(const unsigned char*)mc_gState->m_NetworkParams->GetParam("addressscripthashversion",&size);        
        if(ucPtr)
        {
            base58Prefixes[SCRIPT_ADDRESS].clear();
            for(int i=0;i<size;i++)
            {
                base58Prefixes[SCRIPT_ADDRESS].push_back(ucPtr[i]);
            }
        }

        ucPtr=(const unsigned char*)mc_gState->m_NetworkParams->GetParam("privatekeyversion",&size);        
        if(ucPtr)
        {
            base58Prefixes[SECRET_KEY].clear();
            for(int i=0;i<size;i++)
            {
                base58Prefixes[SECRET_KEY].push_back(ucPtr[i]);
            }
        }

/*  // MCHN-TODO currently copied from main, decide what to do with it later.
        base58Prefixes[EXT_PUBLIC_KEY] = list_of(0x04)(0x35)(0x87)(0xCF);
        base58Prefixes[EXT_SECRET_KEY] = list_of(0x04)(0x35)(0x83)(0x94);
*/
        
//        convertSeed6(vFixedSeeds, pnSeed6_test, ARRAYLEN(pnSeed6_test));

        SetMultiChainParams();
        SetMultiChainRuntimeParams();
        
        fRequireStandard = (mc_gState->m_NetworkParams->GetInt64Param("onlyacceptstdtxs") != 0);
        fRequireStandard=GetBoolArg("-requirestandard", fRequireStandard);
        fTestnetToBeDeprecatedFieldRPC = (mc_gState->m_NetworkParams->GetInt64Param("chainistestnet") != 0);
    }
    
    void SetMultiChainParams()
    {
        fAllowMinDifficultyBlocks=false;
        if(mc_gState->m_Features->FixedIn1000920001())
        {
            fAllowMinDifficultyBlocks = (mc_gState->m_NetworkParams->GetInt64Param("allowmindifficultyblocks") != 0);            
        }
        fDisallowUnsignedBlockNonce=false;
        if(mc_gState->m_Features->NonceInMinerSignature())
        {
            if(nTargetTimespan <= 0)
            {
                if(mc_gState->m_NetworkParams->GetInt64Param("powminimumbits") <= MAX_NBITS_FOR_SIGNED_NONCE)
                {
                    fDisallowUnsignedBlockNonce=true;
                }
            }
        }
    }
    
    void SetMultiChainParam(const char*param_name,int64_t value)
    {
        if(strcmp(param_name,"targetblocktime") == 0)
        {
            nTargetSpacing=value;
        }
    }
    
    void SetMultiChainRuntimeParams()
    {
        fMineBlocksOnDemand = GetBoolArg("-mineblocksondemand", false);
        fMiningRequiresPeers = (mc_gState->m_NetworkParams->GetInt64Param("miningrequirespeers") != 0);
        fMiningRequiresPeers=GetBoolArg("-miningrequirespeers", fMiningRequiresPeers);        
        nLockAdminMineRounds=GetArg("-lockadminminerounds",mc_gState->m_NetworkParams->GetInt64Param("lockadminminerounds"));        
        dMineEmptyRounds=mc_gState->m_NetworkParams->GetDoubleParam("mineemptyrounds");
        string sMineEmptyRounds=GetArg("-mineemptyrounds", "Not Set");
        if(sMineEmptyRounds != "Not Set")
        {
            dMineEmptyRounds=atof(sMineEmptyRounds.c_str());
        }                    
        dMiningTurnover=mc_gState->m_NetworkParams->GetDoubleParam("miningturnover");
        string sMinerDrift=GetArg("-miningturnover", "Not Set");
        if(sMinerDrift != "Not Set")
        {
            dMiningTurnover=atof(sMinerDrift.c_str());
        }                    
    }
    
    void SetGenesis()
    {
        int size;
        const unsigned char *ptrSigScript;
        const unsigned char *ptrPubKeyHash;
//        const unsigned char *ptrOpReturnScript;
        size_t elem_size;
        const unsigned char *elem;
        int root_stream_name_size;
        const unsigned char *root_stream_name;
        mc_Script *lpDetails;
        mc_Script *lpDetailsScript;
        
        switch(mc_gState->m_NetworkParams->m_Status)
        {
            case MC_PRM_STATUS_VALID:
                break;
            default:
                return;
        }        

        genesis.vtx.clear();
        
        CMutableTransaction txNew;
        txNew.vin.resize(1);

        root_stream_name_size=0;
        root_stream_name=(unsigned char *)mc_gState->m_NetworkParams->GetParam("rootstreamname",&root_stream_name_size);        
        if(mc_gState->m_NetworkParams->IsProtocolMultichain() == 0)
        {
            root_stream_name_size=0;
        }    
        if(root_stream_name_size > 1)
        {
            txNew.vout.resize(2);                        
        }
        else
        {
            txNew.vout.resize(1);                                    
        }
                                
        genesis.nBits    = (uint32_t)mc_gState->m_NetworkParams->GetInt64Param("genesisnbits");
        
        ptrSigScript=(unsigned char*)mc_gState->m_NetworkParams->GetParam("chaindescription",&size);        
        txNew.vin[0].scriptSig = CScript() << genesis.nBits << CScriptNum(4) << vector<unsigned char>(ptrSigScript, ptrSigScript + size - 1);
        
        txNew.vout[0].nValue = mc_gState->m_NetworkParams->GetInt64Param("initialblockreward");// * COIN;
        
        if(mc_gState->m_NetworkParams->IsProtocolMultichain())
        {
            ptrPubKeyHash=(unsigned char*)mc_gState->m_NetworkParams->GetParam("genesispubkeyhash",&size);        
            txNew.vout[0].scriptPubKey = CScript() << OP_DUP << OP_HASH160 << vector<unsigned char>(ptrPubKeyHash, ptrPubKeyHash + size) << OP_EQUALVERIFY << OP_CHECKSIG;
        }
        else
        {
            ptrPubKeyHash=(unsigned char*)mc_gState->m_NetworkParams->GetParam("genesispubkey",&size);        
            txNew.vout[0].scriptPubKey = CScript() << vector<unsigned char>(ptrPubKeyHash, ptrPubKeyHash + size) << OP_CHECKSIG;       
        }
        
        if(mc_gState->m_NetworkParams->IsProtocolMultichain())
        {
            mc_Script *lpScript;
            
            lpScript=new mc_Script;
            
            lpScript->SetPermission(MC_PTP_GLOBAL_ALL,0,0xffffffff,(uint32_t)mc_gState->m_NetworkParams->GetInt64Param("genesistimestamp"));
            
            elem = lpScript->GetData(0,&elem_size);
            txNew.vout[0].scriptPubKey << vector<unsigned char>(elem, elem + elem_size) << OP_DROP;
            
            delete lpScript;            
        }
        
        if(root_stream_name_size > 1)
        {        
            txNew.vout[1].nValue=0;
            lpDetails=new mc_Script;
            lpDetails->AddElement();
            if(mc_gState->m_NetworkParams->GetInt64Param("rootstreamopen"))
            {
                unsigned char b=1;        
                lpDetails->SetSpecialParamValue(MC_ENT_SPRM_ANYONE_CAN_WRITE,&b,1);        
            }


            if( (root_stream_name_size > 1) && (root_stream_name[root_stream_name_size - 1] == 0x00) )
            {
                root_stream_name_size--;
            }           

            
            lpDetails->SetSpecialParamValue(MC_ENT_SPRM_NAME,root_stream_name,root_stream_name_size);
    
            size_t bytes;
            const unsigned char *script;
            script=lpDetails->GetData(0,&bytes);
    
            lpDetailsScript=new mc_Script;
            
            lpDetailsScript->SetNewEntityType(MC_ENT_TYPE_STREAM,0,script,bytes);

            elem = lpDetailsScript->GetData(0,&elem_size);
            txNew.vout[1].scriptPubKey=CScript();
            txNew.vout[1].scriptPubKey << vector<unsigned char>(elem, elem + elem_size) << OP_DROP << OP_RETURN;                        
            
            delete lpDetails;
            delete lpDetailsScript;
        }        
        
        
        genesis.vtx.push_back(txNew);
        genesis.hashPrevBlock = 0;
        genesis.hashMerkleRoot = genesis.BuildMerkleTree();
        genesis.nVersion = (uint32_t)mc_gState->m_NetworkParams->GetInt64Param("genesisversion");
        genesis.nTime    = (uint32_t)mc_gState->m_NetworkParams->GetInt64Param("genesistimestamp");
        genesis.nNonce   = (uint32_t)mc_gState->m_NetworkParams->GetInt64Param("genesisnonce");

        hashGenesisBlock = genesis.GetHash();        

        char storedHash[65];
        mc_BinToHex(storedHash,mc_gState->m_NetworkParams->GetParam("genesishash",NULL),32);
               
        assert(strcmp(storedHash,hashGenesisBlock.GetHex().c_str()) == 0);
        
/*        
        mapCheckpointsMultichain =
        boost::assign::map_list_of
        ( 0, uint256(storedHash))
        ;        
 */ 
//        assert(hashGenesisBlock == uint256("0x0f9188f13cb7b2c71f2a335e3a4fc328bf5beb436012afca590b1a11466e2206"));
        
    }
    
    const Checkpoints::CCheckpointData& Checkpoints() const 
    {
        return dataMultichain;
    }
};
static CMultiChainParams multiChainParams;


bool SelectMultiChainParams(const char *NetworkName)
{
    SelectMultiChainBaseParams(NetworkName,(int)mc_gState->m_NetworkParams->GetInt64Param("defaultrpcport"));

    multiChainParams.SetFixedParams(NetworkName);
    
    multiChainParams.Initialize();
    
    pCurrentParams = &multiChainParams;
    
    return true;
}

void SetMultiChainParams()
{
    multiChainParams.SetMultiChainParams();
}

void SetMultiChainParam(const char*param_name,int64_t value)
{
    multiChainParams.SetMultiChainParam(param_name,value);
}


void SetMultiChainRuntimeParams()
{
    multiChainParams.SetMultiChainRuntimeParams();
}

bool InitializeMultiChainParams()
{
    
    multiChainParams.Initialize();
    
    multiChainParams.SetGenesis();
    
    return true;
}

/* MCHN END */


