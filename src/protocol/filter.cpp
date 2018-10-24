// Copyright (c) 2014-2018 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "filter.h"
#include "utils/define.h"
#include "utils/util.h"
#include "v8/v8engine.h"
#include "v8/v8filter.h"
#include "v8/v8filterwatchdog.h"

namespace mc_v8
{
class V8Filter;
}

void mc_Filter::Zero()
{
    m_Impl = nullptr;
    m_timeout = 0;
}

int mc_Filter::Destroy()
{
    if (m_Impl != nullptr)
    {
        auto v8filter = static_cast<mc_v8::V8Filter *>(m_Impl);
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
    m_runningFilter = nullptr;
    m_watchdog = nullptr;
}

int mc_FilterEngine::Destroy()
{
    if (m_Impl != nullptr)
    {
        auto v8engine = static_cast<mc_v8::V8Engine *>(m_Impl);
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

int mc_FilterEngine::Initialize(std::string &strResult)
{
    LogPrint("v8filter", "v8filter: mc_FilterEngine::Initialize\n");
    strResult.clear();
    auto v8engine = new mc_v8::V8Engine();
    m_Impl = v8engine;
    return v8engine->Initialize(strResult);
}

int mc_FilterEngine::CreateFilter(std::string script, std::string main_name, std::vector<std::string> &callback_names,
                                  mc_Filter *filter, std::string &strResult)
{
    LogPrint("v8filter", "v8filter: mc_FilterEngine::CreateFilter\n");
    strResult.clear();
    auto v8engine = static_cast<mc_v8::V8Engine *>(m_Impl);
    filter->Destroy();
    int result = filter->Initialize(strResult);
    if (result != MC_ERR_NOERROR || !strResult.empty())
    {
        return result;
    }
    auto v8filter = static_cast<mc_v8::V8Filter *>(filter->m_Impl);
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
    int result = CreateFilter(script, main_name, callback_names, filter, strResult);
    if (result == MC_ERR_NOERROR)
    {
        filter->SetTimeout(timeout);
    }
    return result;
}

int mc_FilterEngine::RunFilter(const mc_Filter *filter, std::string &strResult)
{
    LogPrint("v8filter", "v8filter: mc_FilterEngine::RunFilter\n");

    auto v8engine = static_cast<mc_v8::V8Engine *>(m_Impl);
    auto v8filter = static_cast<mc_v8::V8Filter *>(filter->m_Impl);
    SetRunningFilter(filter);
    int retval = v8engine->RunFilter(v8filter, strResult);
    ResetRunningFilter();
    return retval;
}

int mc_FilterEngine::RunFilterWithCallbackLog(const mc_Filter *filter, std::string &strResult,
                                              json_spirit::Array &callbacks)
{
    LogPrint("v8filter", "v8filter: mc_FilterEngine::RunFilterWithCallbackLog\n");
    auto v8engine = static_cast<mc_v8::V8Engine *>(m_Impl);
    auto v8filter = static_cast<mc_v8::V8Filter *>(filter->m_Impl);
    SetRunningFilter(filter);
    int retval = v8engine->RunFilterWithCallbackLog(v8filter, strResult, callbacks);
    ResetRunningFilter();
    return retval;
}

void mc_FilterEngine::TerminateFilter(std::string reason)
{
    LogPrint("v8filter", "v8filter: mc_FilterEngine::TerminateFilter\n");
    if (m_runningFilter != nullptr)
    {
        auto v8engine = static_cast<mc_v8::V8Engine *>(m_Impl);
        auto v8filter = static_cast<mc_v8::V8Filter *>(m_runningFilter->m_Impl);
        v8engine->TerminateFilter(v8filter, reason);
    }
}

void mc_FilterEngine::SetRunningFilter(const mc_Filter *filter)
{
    m_runningFilter = filter;
    if (m_watchdog == nullptr)
    {
        m_watchdog = new mc_FilterWatchdog();
        m_watchdog->Initialize();
    }
    m_watchdog->FilterStarted(filter->Timeout());
}

void mc_FilterEngine::ResetRunningFilter()
{
    assert(m_watchdog != nullptr);
    m_runningFilter = nullptr;
    m_watchdog->FilterEnded();
}

void mc_FilterWatchdog::Zero()
{
    m_Impl = nullptr;
}

int mc_FilterWatchdog::Destroy()
{
    if (m_Impl != nullptr)
    {
        auto v8watchdog = static_cast<mc_v8::V8FilterWatchdog *>(m_Impl);
        delete v8watchdog;
    }
    this->Zero();
    return MC_ERR_NOERROR;
}

int mc_FilterWatchdog::Initialize()
{
    auto v8watchdog = new mc_v8::V8FilterWatchdog();
    m_Impl = v8watchdog;
    return MC_ERR_NOERROR;
}

void mc_FilterWatchdog::FilterStarted(int timeout)
{
    auto v8watchdog = static_cast<mc_v8::V8FilterWatchdog *>(m_Impl);
    v8watchdog->FilterStarted(timeout);
}

void mc_FilterWatchdog::FilterEnded()
{
    auto v8watchdog = static_cast<mc_v8::V8FilterWatchdog *>(m_Impl);
    v8watchdog->FilterEnded();
}

void mc_FilterWatchdog::Shutdown()
{
    auto v8watchdog = static_cast<mc_v8::V8FilterWatchdog *>(m_Impl);
    v8watchdog->Shutdown();
}
