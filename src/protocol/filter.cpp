// Copyright (c) 2014-2018 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "v8/v8engine.h"
#include "v8/v8filter.h"
#include "protocol/filter.h"
#include "utils/util.h"
#include "utils/define.h"

namespace mc_v8
{
class V8Filter;
}

void mc_Filter::Zero()
{
//    LogPrintf("mc_Filter: Zero\n");
    m_Impl = nullptr;
}

int mc_Filter::Destroy()
{
//    LogPrintf("mc_Filter: Destroy\n");
    if (m_Impl != nullptr)
    {
        auto v8filter = static_cast<mc_v8::V8Filter*>(m_Impl);
        delete v8filter;
    }

    this->Zero();
    return MC_ERR_NOERROR;
}

int mc_Filter::Initialize(std::string &strResult)
{
    LogPrintf("mc_Filter: Initialize\n");
    m_Impl = new mc_v8::V8Filter();
    return MC_ERR_NOERROR;
}

void mc_FilterEngine::Zero()
{
//    LogPrintf("mc_FilterEngine: Zero\n");
    m_Impl = nullptr;
}

int mc_FilterEngine::Destroy()
{
//    LogPrintf("mc_FilterEngine: Destroy\n");
    if (m_Impl != nullptr)
    {
        auto v8engine = static_cast<mc_v8::V8Engine*>(m_Impl);
        delete v8engine;
    }

    this->Zero();
    return MC_ERR_NOERROR;
}

int mc_FilterEngine::Initialize(std::string& strResult)
{
    LogPrintf("mc_FilterEngine: Initialize\n");
    auto v8engine = new mc_v8::V8Engine();
    m_Impl = v8engine;
    return v8engine->Initialize(strResult);
}

int mc_FilterEngine::CreateFilter(std::string script, std::string main_name, mc_Filter* filter, std::string& strResult)
{
    LogPrintf("mc_FilterEngine: CreateFilter\n");
    auto v8engine = static_cast<mc_v8::V8Engine*>(m_Impl);
    filter->Destroy();
    int result = filter->Initialize(strResult);
    if (result != MC_ERR_NOERROR)
    {
        return result;
    }
    auto v8filter = static_cast<mc_v8::V8Filter*>(filter->m_Impl);
    return v8engine->CreateFilter(script, main_name, v8filter, strResult);
}

int mc_FilterEngine::RunFilter(const mc_Filter& filter, std::string& strResult)
{
    LogPrintf("mc_FilterEngine: RunFilter\n");
    strResult.clear();
    auto v8filter = static_cast<mc_v8::V8Filter*>(filter.m_Impl);
    return v8filter->Run(strResult);
}
