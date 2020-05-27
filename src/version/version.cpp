// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "multichain/multichain.h"
#include "version/version.h"
#include "custom/custom.h"


int mc_State::VersionInfo(int version)
{
    int custom_version=custom_version_info(version);
    if(custom_version != 0)
    {
        return custom_version;
    }
    
    int this_build=20007901;
    int this_protocol=20011;   
    
    if(mc_gState->m_EnterpriseBuild)
    {
        this_build=mc_gState->m_EnterpriseBuild;
    }
    
    if(version < 0)
    {
        return 0;
    }
    if(version < MULTICHAIN_VERSION_CODE_MIN_VALID)
    {
        switch(version)
        {
            case MULTICHAIN_VERSION_CODE_BUILD:                                 // last version
                return -this_build;                                               
            case MULTICHAIN_VERSION_CODE_PROTOCOL_THIS:                         // last protocol version
                return this_protocol;
            case MULTICHAIN_VERSION_CODE_PROTOCOL_MIN:
                return 10008;                                                   // first supported version
            case MULTICHAIN_VERSION_CODE_PROTOCOL_MIN_DOWNGRADE:
                return 10008;                                                   // cannot downgrade below this version
            case MULTICHAIN_VERSION_CODE_PROTOCOL_MIN_NO_DOWNGRADE:
                return 20002;                                                   // if we are on this version or above, downgrades are forbidden
            case MULTICHAIN_VERSION_CODE_PROTOCOL_FOR_RELEVANCE:                
                return 0;                                                       // If not 0, defines relevant parameter set
        }
        return 0;        
    }
    if(version < 10002)return 10002;                                            // first version
    if(version < 10008)return -10000201;                                        // last build supporting this version (negative)
    if(version < 10013)return -this_build;                                      // supported by this version    
    if(version < 20001)return 20001;                                            // next version
    if(version < this_protocol+1)return -this_build;                            // supported by this version    
        
    return VersionInfo(MULTICHAIN_VERSION_CODE_BUILD)-1;                        // Created by the following builds
}

int mc_State::IsSupported(int version)
{
    void *ptr=mc_gState->m_NetworkParams->GetParam("chainprotocol",NULL);
    if(ptr)
    {
        if(strcmp((char*)ptr,"multichain"))
        {
            return 1;
        }
    }
    
    if(-VersionInfo(version) == GetNumericVersion())
    {
        return 1;
    }
    return 0;
}

int mc_State::IsDeprecated(int version)
{
    int build=-VersionInfo(version);
    int is_protocol_multichain=1;
    
    void *ptr=mc_gState->m_NetworkParams->GetParam("chainprotocol",NULL);
    if(ptr)
    {
        if(strcmp((char*)ptr,"multichain"))
        {
            is_protocol_multichain=0;
        }
    }
    if(is_protocol_multichain)
    {
        if(build > 0)
        {
            if(build < GetNumericVersion())
            {
                return 1;
            }
        }
    }
    return 0;
}

int mc_State::GetNumericVersion()
{
    return -VersionInfo(MULTICHAIN_VERSION_CODE_BUILD);
}

int mc_State::GetProtocolVersion()
{
    return VersionInfo(MULTICHAIN_VERSION_CODE_PROTOCOL_THIS);
}

int mc_State::MinProtocolVersion()
{
    return VersionInfo(MULTICHAIN_VERSION_CODE_PROTOCOL_MIN);
}

int mc_State::MinProtocolDowngradeVersion()
{
    return VersionInfo(MULTICHAIN_VERSION_CODE_PROTOCOL_MIN_DOWNGRADE);
}

int mc_State::MinProtocolForbiddenDowngradeVersion()
{
    return VersionInfo(MULTICHAIN_VERSION_CODE_PROTOCOL_MIN_NO_DOWNGRADE);    
}

int mc_State::RelevantParamProtocolVersion()
{
    return VersionInfo(MULTICHAIN_VERSION_CODE_PROTOCOL_FOR_RELEVANCE);        
}

int mc_State::GetWalletDBVersion()
{
    if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)
    {
        if(mc_gState->m_WalletMode & MC_WMD_MAP_TXS)
        {
            return -1;                
        }
        else
        {
            if(mc_gState->m_WalletMode & MC_WMD_FLAT_DAT_FILE)
            {
                return 3;
            }
            return 2;
        }
    }
    
    return 1;
}

