// Copyright (c) 2014-2017 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#ifndef MULTICHAIN_STATE_H
#define	MULTICHAIN_STATE_H

#include "utils/define.h"
#include "chainparams/params.h"
#include "protocol/multichainscript.h"
#include "permissions/permission.h"
#include "entities/asset.h"

#define MC_FAT_UNKNOWN     1
#define MC_FAT_COMMAND     2
#define MC_FAT_NETWORK     3
#define MC_FAT_NETWORKSEED 4

#define MC_NTS_UNCONNECTED             0
#define MC_NTS_WAITING_FOR_SEED        1
#define MC_NTS_SEED_READY              2     
#define MC_NTS_NOT_READY               3
#define MC_NTS_SEED_NO_PARAMS          4     

#define MC_NPS_NONE      0x00000000
#define MC_NPS_NETWORK   0x00000001
#define MC_NPS_INCOMING  0x00000002
#define MC_NPS_MINING    0x00000004
#define MC_NPS_ALL       0xFFFFFFFF

#define MC_WMD_NONE                  0x00000000
#define MC_WMD_TXS                   0x00000001
#define MC_WMD_ADDRESS_TXS           0x00000002
#define MC_WMD_MAP_TXS               0x00010000
#define MC_WMD_MODE_MASK             0x00FFFFFF
#define MC_WMD_DEBUG                 0x01000000
#define MC_WMD_AUTOSUBSCRIBE_STREAMS 0x02000000
#define MC_WMD_AUTOSUBSCRIBE_ASSETS  0x04000000
#define MC_WMD_AUTO                  0x10000000


#ifdef	__cplusplus
extern "C" {
#endif

    
typedef struct mc_Params
{    
    int m_NumArguments;
    char** m_Arguments;
    int m_FirstArgumentType;
    char m_DataDirNetSpecific[MC_DCT_DB_MAX_PATH];
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
    
    void Parse(int argc, const char* const argv[]);
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

typedef struct mc_Features
{    
    int MinProtocolVersion();
    int ActivatePermission();
    int LastVersionNotSendingProtocolVersionInHandShake();
    int VerifySizeOfOpDropElements();
    int PerEntityPermissions();
    int FollowOnIssues();
    int SpecialParamsInDetailsScript();
    int FixedGrantsInTheSameTx();
    int UnconfirmedMinersCannotMine();
    int Streams();
    int OpDropDetailsScripts();
    int ShortTxIDInTx();
    int CachedInputScript();
    int AnyoneCanReceiveEmpty();
    int FixedIn10007();
    int Upgrades();
    int FixedIn10008();
} mc_Features;

typedef struct mc_BlockHeaderInfo
{    
    unsigned char m_Hash[32];
    int32_t m_NodeId;
    int32_t m_Next;
    
} mc_BlockHeaderInfo;

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
    
    mc_Script               *m_TmpScript;
    mc_Script               *m_TmpScript1;
    mc_Buffer               *m_TmpAssetsOut;
    mc_Buffer               *m_TmpAssetsIn;
    
    mc_Buffer               *m_BlockHeaderSuccessors;
    
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
        m_IPv4Address=0;
        m_WalletMode=0;
        m_TmpAssetsOut=new mc_Buffer;
        mc_InitABufferMap(m_TmpAssetsOut);
        m_TmpAssetsIn=new mc_Buffer;
        mc_InitABufferMap(m_TmpAssetsIn);
        
        m_BlockHeaderSuccessors=new mc_Buffer;
        m_BlockHeaderSuccessors->Initialize(sizeof(mc_BlockHeaderInfo),sizeof(mc_BlockHeaderInfo),0);            
        mc_BlockHeaderInfo bhi;
        memset(&bhi,0,sizeof(mc_BlockHeaderInfo));
        m_BlockHeaderSuccessors->Add(&bhi);
        
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
        if(m_BlockHeaderSuccessors)
        {
            delete m_BlockHeaderSuccessors;
        }
    }
    
    const char* GetVersion();
    const char* GetFullVersion();
    int GetNumericVersion();
    int GetWalletDBVersion();
    int GetProtocolVersion();
    const char* GetSeedNode();
} cs_State;


#ifdef	__cplusplus
}
#endif


extern mc_State* mc_gState;

#endif	/* MULTICHAIN_STATE_H */

