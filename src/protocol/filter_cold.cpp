// Copyright (c) 2014-2018 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "protocol/filter.h"
#include "utils/util.h"
#include "utils/define.h"

void mc_Filter::Zero()
{
    m_Impl = nullptr;
}

int mc_Filter::Destroy()
{
    this->Zero();
    return MC_ERR_NOERROR;
}

int mc_Filter::Initialize(std::string &strResult)
{
    return MC_ERR_NOERROR;
}

void mc_FilterEngine::Zero()
{
    m_Impl = nullptr;
}

int mc_FilterEngine::Destroy()
{
    this->Zero();
    return MC_ERR_NOERROR;
}

int mc_FilterEngine::Initialize(std::string& strResult)
{
    strResult.clear();
    return MC_ERR_NOERROR;
}

int mc_FilterEngine::CreateFilter(std::string script, std::string main_name, mc_Filter* filter, std::string& strResult)
{
    strResult.clear();
    return MC_ERR_NOERROR;
}

int mc_FilterEngine::RunFilter(const mc_Filter* filter, std::string& strResult)
{
    strResult.clear();
    return MC_ERR_NOERROR;
}

int mc_FilterEngine::RunFilterWithCallbackLog(const mc_Filter* filter, std::string& strResult, json_spirit::Array& callbacks)
{
    strResult.clear();
    callbacks.clear();
    return MC_ERR_NOERROR;
}
