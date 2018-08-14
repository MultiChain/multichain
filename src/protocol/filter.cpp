// Copyright (c) 2014-2017 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "protocol/filter.h"

int mc_Filter::Zero()
{
    return MC_ERR_NOERROR;
}

int mc_Filter::Destroy()
{
    return MC_ERR_NOERROR;
}


int mc_FilterEngine::Zero()
{
    return MC_ERR_NOERROR;
}

int mc_FilterEngine::Destroy()
{
    return MC_ERR_NOERROR;
}

int mc_FilterEngine::Initialize()
{
    return MC_ERR_NOERROR;
}

int mc_FilterEngine::CreateFilter(const char *script,const char* main_name,mc_Filter *filter,std::string &strResult)
{
    strResult="";
    return MC_ERR_NOERROR;
}

int mc_FilterEngine::RunFilter(const mc_Filter& filter,std::string &strResult)
{
    strResult="";
    return MC_ERR_NOERROR;    
}
