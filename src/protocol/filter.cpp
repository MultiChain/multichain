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
    m_Impl = nullptr;
}

int mc_Filter::Destroy()
{
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
    m_Impl = new mc_v8::V8Filter();
    return MC_ERR_NOERROR;
}

void mc_FilterEngine::Zero()
{
    m_Impl = nullptr;
}

int mc_FilterEngine::Destroy()
{
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
    LogPrint("v8filter", "v8filter: mc_FilterEngine::Initialize\n");
    strResult.clear();
    auto v8engine = new mc_v8::V8Engine();
    m_Impl = v8engine;
    return v8engine->Initialize(strResult);
}

int mc_FilterEngine::CreateFilter(std::string script, std::string main_name, std::vector<std::string>& callback_names,
        mc_Filter* filter, std::string& strResult)
{
    LogPrint("v8filter", "v8filter: mc_FilterEngine::CreateFilter\n");
    strResult.clear();
    auto v8engine = static_cast<mc_v8::V8Engine*>(m_Impl);
    filter->Destroy();
    int result = filter->Initialize(strResult);
    if (result != MC_ERR_NOERROR || !strResult.empty())
    {
        return result;
    }
    auto v8filter = static_cast<mc_v8::V8Filter*>(filter->m_Impl);
    result = v8engine->CreateFilter(script, main_name, callback_names, v8filter, strResult);
    if (result != MC_ERR_NOERROR || !strResult.empty())
    {
        filter->Destroy();
    }
    return result;
}

int mc_FilterEngine::RunFilter(const mc_Filter* filter, std::string& strResult)
{
    LogPrint("v8filter", "v8filter: mc_FilterEngine::RunFilter\n");

    auto v8engine = static_cast<mc_v8::V8Engine*>(m_Impl);
    auto v8filter = static_cast<mc_v8::V8Filter*>(filter->m_Impl);
    return v8engine->RunFilter(v8filter, strResult);
}

int mc_FilterEngine::RunFilterWithCallbackLog(const mc_Filter* filter, std::string& strResult, json_spirit::Array& callbacks)
{
    LogPrint("v8filter", "v8filter: mc_FilterEngine::RunFilterWithCallbackLog\n");
    auto v8engine = static_cast<mc_v8::V8Engine*>(m_Impl);
    auto v8filter = static_cast<mc_v8::V8Filter*>(filter->m_Impl);
    return v8engine->RunFilterWithCallbackLog(v8filter, strResult, callbacks);
}
