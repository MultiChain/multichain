// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#ifndef MULTICHAIN_STATE_H
#define	MULTICHAIN_STATE_H

#include "utils/define.h"
#include "chainparams/params.h"
#include "protocol/multichainscript.h"
#include "permissions/permission.h"
#include "entities/asset.h"
#include "wallet/wallettxdb.h"

#define MC_FAT_UNKNOWN     1
#define MC_FAT_COMMAND     2
#define MC_FAT_NETWORK     3
#define MC_FAT_NETWORKSEED 4

#define MC_ETP_DAEMON      1
#define MC_ETP_UTIL        2
#define MC_ETP_CLI         3

#define MC_SSF_DEFAULT        0x00000000
#define MC_SSF_COLD           0x00000001

#define MC_NTS_UNCONNECTED             0
#define MC_NTS_WAITING_FOR_SEED        1
#define MC_NTS_SEED_READY              2     
#define MC_NTS_NOT_READY               3
#define MC_NTS_SEED_NO_PARAMS          4     

#define MC_NPS_NONE      0x00000000
#define MC_NPS_NETWORK   0x00000001
#define MC_NPS_INCOMING  0x00000002
#define MC_NPS_MINING    0x00000004
#define MC_NPS_REACCEPT  0x00000008
#define MC_NPS_OFFCHAIN  0x00000010
#define MC_NPS_CHUNKS    0x00000020
#define MC_NPS_ALL       0xFFFFFFFF

#define MC_WMD_NONE                  0x00000000
#define MC_WMD_TXS                   0x00000001
#define MC_WMD_ADDRESS_TXS           0x00000002
#define MC_WMD_FLAT_DAT_FILE         0x00000100
#define MC_WMD_MAP_TXS               0x00010000
#define MC_WMD_MODE_MASK             0x00FFFFFF
#define MC_WMD_DEBUG                 0x01000000
#define MC_WMD_AUTOSUBSCRIBE_STREAMS 0x02000000
#define MC_WMD_AUTOSUBSCRIBE_ASSETS  0x04000000
#define MC_WMD_NO_CHUNK_FLUSH        0x08000000
#define MC_WMD_AUTO                  0x10000000

#define MC_VCM_NONE                  0x00000000
#define MC_VCM_1_0                   0x00000001


#ifdef	__cplusplus
extern "C" {
#endif

    
typedef struct mc_Params
{    
    int m_NumArguments;
    char** m_Arguments;
    int m_FirstArgumentType;
    char m_DataDirNetSpecific[MC_DCT_DB_MAX_PATH];
    char m_LogDirNetSpecific[MC_DCT_DB_MAX_PATH];
    char m_DataDir[MC_DCT_DB_MAX_PATH];
    
    mc_Params()
    {
        InitDefaults();
    }    

    ~mc_Params()
    {
        Destroy();
    }
    
    void  InitDefaults()
    {
        m_NumArguments=0;
        m_Arguments=NULL;
        m_FirstArgumentType=MC_FAT_UNKNOWN;
        m_DataDir[0]=0;
        m_DataDirNetSpecific[0]=0;
    }
    
    void  Destroy()
    {
        if(m_Arguments)
        {
            if(m_Arguments[0])
            {
                mc_Delete(m_Arguments[0]);
            }
            mc_Delete(m_Arguments);
        }
    }
    
    void Parse(int argc, const char* const argv[], int exe_type);
    int ReadConfig(const char *network_name);
    const char* GetOption(const char* strArg,const char* strDefault);
    int64_t GetOption(const char* strArg,int64_t nDefault);
    int64_t HasOption(const char* strArg);
    
    const char *NetworkName();
    const char *SeedNode();
    const char *Command();
    const char *DataDir();
    const char *DataDir(int network_specific,int create);
    
} mc_Params;

typedef struct mc_UpgradeStatus
{    
    unsigned char m_EntityShortTxID[MC_ENT_KEY_SIZE];                                
    uint32_t m_ApprovedBlock;
    uint32_t m_AppliedBlock;
    uint32_t m_FirstParam;
    uint32_t m_LastParam;    
} mc_UpgradeStatus;

typedef struct mc_UpgradedParameter
{    
    const mc_OneMultichainParam *m_Param;
    int64_t m_Value;    
    uint32_t m_Block;
    int32_t m_Skipped; 
} mc_UpgradedParameter;

typedef struct mc_Features
{    
    int LastVersionNotSendingProtocolVersionInHandShake();
    int AnyoneCanReceiveEmpty();
    int FormattedData();
    int FixedDestinationExtraction();
    int FixedIn1000920001();
    int MultipleStreamKeys();
    int FixedIsUnspendable();
    int PerAssetPermissions();
    int ParameterUpgrades();
    int OffChainData();
    int Chunks();
    int FixedIn1001020003();
    int FixedIn1001120003();
    int Filters();
    int CustomPermissions();
    int StreamFilters();
    int FixedIn20005();
    int FilterLimitedMathSet();
    int FixedIn20006();
    int NonceInMinerSignature();
    int ImplicitConnectPermission();
    int LicenseTokens();
    int FixedJSDateFunctions();
    int DisabledJSDateParse();
    int FixedLegacyPermissionRestrictionFlag();
    int ReadPermissions();
    int SaltedChunks();
    int FixedIn20010();
    int License20010();
    int ExtendedEntityDetails();
    int FixedSpendingBigScripts();
    int Variables();
} mc_Features;

typedef struct mc_BlockHeaderInfo
{    
    unsigned char m_Hash[32];
    int32_t m_NodeId;
    int32_t m_Next;
    
} mc_BlockHeaderInfo;

typedef struct mc_TmpBuffers
{
    mc_TmpBuffers()
    {
        Init();
    }
    
    ~mc_TmpBuffers()
    {
        Destroy();
    }
    
    mc_Script               *m_RpcScript1;
    mc_Script               *m_RpcScript2;
    mc_Script               *m_RpcScript3;
    mc_Script               *m_RpcScript4;
    mc_Buffer               *m_RpcABBuffer1;
    mc_Buffer               *m_RpcABBuffer2;
    mc_Buffer               *m_RpcBuffer1;
    mc_Buffer               *m_RpcABNoMapBuffer1;
    mc_Buffer               *m_RpcABNoMapBuffer2;
    mc_Buffer               *m_RpcEntityRows;
    mc_Buffer               *m_RpcEntityRowsToMerge;
    mc_Buffer               *m_RpcEntityRowsFull;
    mc_SHA256               *m_RpcHasher1;
    mc_Script               *m_RpcChunkScript1;
    mc_Script               *m_RelayTmpBuffer;
    mc_Script               *m_LicenseTmpBuffer;
    mc_Script               *m_LicenseTmpBufferForHash;
    
    void  Init()
    {
        m_RpcScript1=new mc_Script();
        m_RpcScript2=new mc_Script();
        m_RpcScript3=new mc_Script();
        m_RpcScript4=new mc_Script();
        m_RpcABBuffer1=new mc_Buffer;
        mc_InitABufferMap(m_RpcABBuffer1);
        m_RpcABBuffer2=new mc_Buffer;
        mc_InitABufferMap(m_RpcABBuffer2);
        m_RpcBuffer1=new mc_Buffer;
        m_RpcABNoMapBuffer1=new mc_Buffer;
        mc_InitABufferDefault(m_RpcABNoMapBuffer1);
        m_RpcABNoMapBuffer2=new mc_Buffer;
        mc_InitABufferDefault(m_RpcABNoMapBuffer2);
        m_RpcEntityRows=new mc_Buffer;
        m_RpcEntityRows->Initialize(MC_TDB_ENTITY_KEY_SIZE,MC_TDB_ROW_SIZE,MC_BUF_MODE_DEFAULT);     
        m_RpcEntityRowsToMerge=new mc_Buffer;
        m_RpcEntityRowsToMerge->Initialize(MC_TDB_ENTITY_KEY_SIZE,MC_TDB_ROW_SIZE,MC_BUF_MODE_DEFAULT);     
        m_RpcEntityRowsFull=new mc_Buffer;
        m_RpcEntityRowsFull->Initialize(MC_TDB_ENTITY_KEY_SIZE,MC_TDB_ROW_SIZE,MC_BUF_MODE_DEFAULT);     
        m_RpcHasher1=new mc_SHA256();
        m_RpcChunkScript1=new mc_Script();
        m_RelayTmpBuffer=new mc_Script();
        m_LicenseTmpBuffer=new mc_Script();
        m_LicenseTmpBufferForHash=new mc_Script();
    }    

    void  Destroy()
    {
        delete m_RpcScript1;
        delete m_RpcScript2;
        delete m_RpcScript3;
        delete m_RpcScript4;
        delete m_RpcABBuffer1;
        delete m_RpcABBuffer2;
        delete m_RpcBuffer1;
        delete m_RpcABNoMapBuffer1;
        delete m_RpcABNoMapBuffer2;
        delete m_RpcEntityRows;
        delete m_RpcEntityRowsToMerge;
        delete m_RpcEntityRowsFull;
        delete m_RpcHasher1;
        delete m_RpcChunkScript1;
        delete m_RelayTmpBuffer;
        delete m_LicenseTmpBuffer;
        delete m_LicenseTmpBufferForHash;
    }
    
} mc_TmpBuffers;

typedef struct mc_State
{    
    mc_State()
    {
        InitDefaults();
    }    

    ~mc_State()
    {
        Destroy();
    }
    
    mc_Params               *m_Params;
    mc_MultichainParams     *m_NetworkParams;
    mc_Permissions          *m_Permissions;
    mc_AssetDB              *m_Assets;
    mc_Features             *m_Features;
    int m_NetworkState;
    uint32_t m_NodePausedState;    
    uint32_t m_IPv4Address;
    uint32_t m_WalletMode;
    int m_ProtocolVersionToUpgrade;
    void *m_pSeedNode;
    uint32_t m_Compatibility;
    uint32_t m_SessionFlags;
    unsigned char m_BurnAddress[20];
    int m_EnterpriseBuild;
    char m_SeedResolvedAddress[256];
    
    mc_Script               *m_TmpScript;
    mc_Script               *m_TmpScript1;
    mc_Buffer               *m_TmpAssetsOut;
    mc_Buffer               *m_TmpAssetsIn;
    mc_Buffer               *m_TmpAssetsTmp;
    
    mc_Buffer               *m_BlockHeaderSuccessors;
    
    mc_TmpBuffers           *m_TmpBuffers;
    
    void  InitDefaults()
    {
        m_Params=new mc_Params;     
        m_Features=new mc_Features;
        m_NetworkParams=new mc_MultichainParams;
        m_Permissions=NULL;
        m_Assets=NULL;
        m_TmpScript=new mc_Script;
        m_TmpScript1=new mc_Script;
        m_NetworkState=MC_NTS_UNCONNECTED;
        m_NodePausedState=MC_NPS_NONE;
        m_ProtocolVersionToUpgrade=0;
        m_SessionFlags=MC_SSF_DEFAULT;
        m_EnterpriseBuild=0;
        memset(m_BurnAddress,0,20);
        memset(m_SeedResolvedAddress,0,256);
        
        m_IPv4Address=0;
        m_WalletMode=0;
        m_TmpAssetsOut=new mc_Buffer;
        mc_InitABufferMap(m_TmpAssetsOut);
        m_TmpAssetsIn=new mc_Buffer;
        mc_InitABufferMap(m_TmpAssetsIn);
        m_TmpAssetsTmp=new mc_Buffer;
        mc_InitABufferMap(m_TmpAssetsTmp);
        m_Compatibility=MC_VCM_NONE;
        
        m_BlockHeaderSuccessors=new mc_Buffer;
        m_BlockHeaderSuccessors->Initialize(sizeof(mc_BlockHeaderInfo),sizeof(mc_BlockHeaderInfo),0);            
        mc_BlockHeaderInfo bhi;
        memset(&bhi,0,sizeof(mc_BlockHeaderInfo));
        m_BlockHeaderSuccessors->Add(&bhi);
        
        m_TmpBuffers=new mc_TmpBuffers;
        
        m_pSeedNode=NULL;
    }
    
    void  Destroy()
    {
        if(m_Params)
        {
            delete m_Params;
        }
        if(m_Features)
        {
            delete m_Features;
        }
        if(m_Permissions)
        {
            delete m_Permissions;
        }
        if(m_Assets)
        {
            delete m_Assets;
        }
        if(m_NetworkParams)
        {
            delete m_NetworkParams;
        }
        if(m_TmpScript)
        {
            delete m_TmpScript;
        }
        if(m_TmpScript1)
        {
            delete m_TmpScript1;
        }
        if(m_TmpAssetsOut)
        {
            delete m_TmpAssetsOut;
        }
        if(m_TmpAssetsIn)
        {
            delete m_TmpAssetsIn;
        }
        if(m_TmpAssetsTmp)
        {
            delete m_TmpAssetsTmp;
        }
        if(m_BlockHeaderSuccessors)
        {
            delete m_BlockHeaderSuccessors;
        }
        if(m_TmpBuffers)
        {
            delete m_TmpBuffers;
        }
    }
    
    int VersionInfo(int version);
    int GetNumericVersion();
    int GetWalletDBVersion();
    int GetProtocolVersion();
    int MinProtocolVersion();
    int MinProtocolDowngradeVersion();
    int MinProtocolForbiddenDowngradeVersion();
    int RelevantParamProtocolVersion();
    int IsSupported(int version);
    int IsDeprecated(int version);
    const char* GetSeedNode();
    int SetSeedNode(const char* seed_resolved);
} cs_State;


#ifdef	__cplusplus
}
#endif


extern mc_State* mc_gState;

#endif	/* MULTICHAIN_STATE_H */

