// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "filters/filter.h"
#include "utils/define.h"
#include "utils/util.h"

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
    strResult.clear();
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

int mc_FilterEngine::Initialize(std::string &strResult)
{
    strResult.clear();
    return MC_ERR_NOERROR;
}

int mc_FilterEngine::CreateFilter(std::string, std::string, std::vector<std::string> &, mc_Filter *,
                                  std::string &strResult)
{
    strResult.clear();
    return MC_ERR_NOERROR;
}

int mc_FilterEngine::CreateFilter(std::string, std::string, std::vector<std::string> &, mc_Filter *, int,
                                  std::string &strResult)
{
    strResult.clear();
    return MC_ERR_NOERROR;
}

int mc_FilterEngine::RunFilter(const mc_Filter *, std::string &strResult, bool, json_spirit::Array *)
{
    strResult.clear();
    return MC_ERR_NOERROR;
}

int mc_FilterEngine::RunFilterWithCallbackLog(const mc_Filter *, std::string &strResult, json_spirit::Array *callbacks)
{
    strResult.clear();
    callbacks->clear();
    return MC_ERR_NOERROR;
}

void mc_FilterEngine::TerminateFilter(std::string)
{
}
