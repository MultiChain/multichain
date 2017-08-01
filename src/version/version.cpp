// Copyright (c) 2014-2017 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "multichain/multichain.h"
#include "version/version.h"


int mc_State::VersionInfo(int version)
{
    int this_build=20000101;
    int this_protocol=20001;
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
                return 10004;                                                   // first supported version
            case MULTICHAIN_VERSION_CODE_PROTOCOL_MIN_DOWNGRADE:
                return 10008;                                                   // cannot downgrade below this version
        }
        return 0;        
    }
    if(version < 10002)return 10002;                                            // first version
    if(version < 10004)return -10000201;                                        // last build supporting this version (negative)
    if(version < 10009)return -this_build;                                      // supported by this version    
    if(version < 20001)return 20001;                                            // next version
    if(version < 20002)return -this_build;                                      // supported by this version    
        
    return VersionInfo(MULTICHAIN_VERSION_CODE_BUILD)-1;                        // Created by the following builds
}

int mc_State::IsSupported(int version)
{
    if(-VersionInfo(version) == GetNumericVersion())
    {
        return 1;
    }
    return 0;
}

int mc_State::IsDeprecated(int version)
{
    int build=-VersionInfo(version);
    
    if(build > 0)
    {
        if(build < GetNumericVersion())
        {
            return 1;
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
            return 2;                
        }
    }
    
    return 1;
}

