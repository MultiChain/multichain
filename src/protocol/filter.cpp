// Copyright (c) 2014-2018 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "filter.h"
#include "filterwatchdog.h"
#include "v8/v8engine.h"
#include "v8/v8filter.h"
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
    strResult.clear();
    m_Impl = new mc_v8::V8Filter();
    return MC_ERR_NOERROR;
}

void mc_FilterEngine::Zero()
{
    m_Impl = nullptr;
    m_timeout = 0;
    m_runningFilter = nullptr;
    m_watchdog = nullptr;
}

int mc_FilterEngine::Destroy()
{
    if (m_Impl != nullptr)
    {
        auto v8engine = static_cast<mc_v8::V8Engine*>(m_Impl);
        delete v8engine;
    }
    if (m_watchdog != nullptr)
    {
        m_watchdog->Shutdown();
        delete m_watchdog;
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

int mc_FilterEngine::CreateFilter(std::string script, std::string main_name, std::vector<std::string> &callback_names,
                                  mc_Filter *filter, int timeout, std::string &strResult)
{
    LogPrintf("v8filter: mc_FilterEngine::CreateFilter(timeout=%d)\n", timeout);
    m_timeout = timeout;
    return CreateFilter(script, main_name, callback_names, filter, strResult);
}

int mc_FilterEngine::RunFilter(const mc_Filter* filter, std::string& strResult)
{
    LogPrint("v8filter", "v8filter: mc_FilterEngine::RunFilter\n");

    auto v8engine = static_cast<mc_v8::V8Engine*>(m_Impl);
    auto v8filter = static_cast<mc_v8::V8Filter*>(filter->m_Impl);
    SetRunningFilter(filter);
    int retval = v8engine->RunFilter(v8filter, strResult);
    ResetRunningFilter();
    return retval;
}

int mc_FilterEngine::RunFilterWithCallbackLog(const mc_Filter* filter, std::string& strResult, json_spirit::Array& callbacks)
{
    LogPrint("v8filter", "v8filter: mc_FilterEngine::RunFilterWithCallbackLog\n");
    auto v8engine = static_cast<mc_v8::V8Engine*>(m_Impl);
    auto v8filter = static_cast<mc_v8::V8Filter*>(filter->m_Impl);
    SetRunningFilter(filter);
    int retval = v8engine->RunFilterWithCallbackLog(v8filter, strResult, callbacks);
    ResetRunningFilter();
    return retval;
}

void mc_FilterEngine::TerminateFilter()
{
    LogPrint("v8filter", "v8filter: mc_FilterEngine::TerminateFilter\n");
    if (m_runningFilter != nullptr)
    {
        auto v8engine = static_cast<mc_v8::V8Engine*>(m_Impl);
        auto v8filter = static_cast<mc_v8::V8Filter*>(m_runningFilter->m_Impl);
        v8engine->TerminateFilter(v8filter);
    }
}

void mc_FilterEngine::SetRunningFilter(const mc_Filter *filter)
{
    m_runningFilter = filter;
    if (m_watchdog == nullptr)
    {
        m_watchdog = new FilterWatchdog();
    }
    m_watchdog->FilterStarted(m_timeout);
}

void mc_FilterEngine::ResetRunningFilter()
{
    assert(m_watchdog != nullptr);
    m_runningFilter = nullptr;
    m_watchdog->FilterEnded();
}
