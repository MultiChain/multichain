// Copyright (c) 2014-2017 Coin Sciences Ltd
// Copyright (c) 2018 RecordsKeeper
// Rk code distributed under the GPLv3 license, see COPYING file.

#include "rk/rk.h"


#include "version/version.h"

const char* mc_State::GetVersion()
{
    return RK_BUILD_DESC;
}

const char* mc_State::GetFullVersion()
{
    return RK_FULL_VERSION;
}

int mc_State::GetNumericVersion()
{
    return RK_BUILD_DESC_NUMERIC;
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

int mc_State::GetProtocolVersion()
{
    return RK_PROTOCOL_VERSION;
}
