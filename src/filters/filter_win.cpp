// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "filters/filter.h"
#include "filters/filtercallback.h"
#include "filters/watchdog.h"
#include "chainparams/state.h"
#include "utils/define.h"
#include "utils/util.h"
#include "v8_win/v8_win.h"
#include <boost/bind.hpp>

//using namespace boost::placeholders;

const size_t RESULT_SIZE = 4096;

const char **vec2cstrs(const std::vector<std::string>& vec, size_t &ncstrs)
{
    ncstrs = vec.size();
    const char **cstrs = new const char*[ncstrs];
    for (size_t i = 0; i < ncstrs; ++i)
    {
        cstrs[i] = vec[i].c_str();
    }
    return cstrs;
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
        auto v8filter = static_cast<V8Filter_t *>(m_Impl);
        V8Filter_Delete(&v8filter);
    }
    this->Zero();
    return MC_ERR_NOERROR;
}

int mc_Filter::Initialize(std::string &strResult)
{
    if (fDebug)
        LogPrint("v8filter", "v8filter: mc_Filter::Initialize\n");
    strResult.clear();
    m_Impl = V8Filter_Create();
    return MC_ERR_NOERROR;
}

void mc_FilterEngine::Zero()
{
    m_Impl = nullptr;
    m_runningFilter = nullptr;
    m_watchdog = nullptr;
    m_filterCallback.ResetCallbackLog();
}

int mc_FilterEngine::Destroy()
{
    if (m_Impl != nullptr)
    {
        auto v8engine = static_cast<V8Engine_t *>(m_Impl);
        V8Engine_Delete(&v8engine);
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
    if (fDebug)
        LogPrint("v8filter", "v8filter: mc_FilterEngine::Initialize\n");
    strResult.clear();
    auto v8engine = V8Engine_Create();
    m_Impl = v8engine;
    auto dataDir = GetDataDir();
    if (fDebug)
        LogPrint("v8filter", "v8filter:   dataDir=%s\n", dataDir.string());
    char result[RESULT_SIZE];
    auto v8filterCallback = reinterpret_cast<IFilterCallback_t *>(&m_filterCallback);
    int retval = V8Engine_Initialize(v8engine, v8filterCallback, dataDir.string().c_str(), fDebug, result);
    if (fDebug)
        LogPrint("v8filter", "v8filter:   retval=%d result=%s\n", retval, result);
    strResult = result;
    if (fDebug)
        LogPrint("v8filter", "v8filter: mc_FilterEngine::Initialize - done\n");
    return retval;
}

int mc_FilterEngine::CreateFilter(std::string script, std::string main_name, std::vector<std::string> &callback_names,
                                  mc_Filter *filter, std::string &strResult)
{
    if (fDebug)
        LogPrint("v8filter", "v8filter: mc_FilterEngine::CreateFilter\n");
    strResult.clear();
    auto v8engine = static_cast<V8Engine_t *>(m_Impl);
    filter->Destroy();
    int retval = filter->Initialize(strResult);
    if (retval != MC_ERR_NOERROR || !strResult.empty())
    {
        return retval;
    }
    size_t n_callbackNames;
    const char **callbackNames = vec2cstrs(callback_names, n_callbackNames);
    auto v8filter = static_cast<V8Filter_t *>(filter->m_Impl);
    char result[RESULT_SIZE];
    uint32_t jsInjectionParams=0;
    if(mc_gState->m_Features->FilterLimitedMathSet())
    {
        jsInjectionParams |= MC_V8W_JS_INJECTION_LIMITED_MATH_SET;
    }
    if(mc_gState->m_Features->FixedJSDateFunctions())
    {
        jsInjectionParams |= MC_V8W_JS_INJECTION_FIXED_DATE_FUNCTIONS;
    }
    if(mc_gState->m_Features->DisabledJSDateParse())
    {
        jsInjectionParams |= MC_V8W_JS_INJECTION_DISABLED_DATE_PARSE;
    }
    
    retval = V8Engine_CreateFilter(v8engine, script.c_str(), main_name.c_str(), callbackNames, n_callbackNames,
                                   v8filter, jsInjectionParams, result);
    delete [] callbackNames;
    if (fDebug)
        LogPrint("v8filter", "v8filter:   retval=%d result=%s\n", retval, result);
    if (retval != MC_ERR_NOERROR || (result != nullptr && strlen(result) > 0))
    {
        filter->Destroy();
    }
    strResult = result;
    return retval;
}

int mc_FilterEngine::CreateFilter(std::string script, std::string main_name, std::vector<std::string> &callback_names,
                                  mc_Filter *filter, int timeout, std::string &strResult)
{
    if (fDebug)
        LogPrint("v8filter", "v8filter: mc_FilterEngine::CreateFilter(timeout=%d)\n", timeout);
    int result = CreateFilter(script, main_name, callback_names, filter, strResult);
    if (result == MC_ERR_NOERROR)
    {
        filter->SetTimeout(timeout);
    }
    return result;
}

int mc_FilterEngine::RunFilter(const mc_Filter *filter, std::string &strResult, bool createCallbackLog,
                               json_spirit::Array *callbacks)
{
    if (fDebug)
        LogPrint("v8filter", "v8filter: mc_FilterEngine::RunFilter\n");

    m_filterCallback.ResetCallbackLog();
    m_filterCallback.SetCreateCallbackLog(createCallbackLog);
    auto v8engine = static_cast<V8Engine_t *>(m_Impl);
    auto v8filter = static_cast<V8Filter_t *>(filter->m_Impl);
    SetRunningFilter(filter);
    char result[RESULT_SIZE];
    int retval = V8Engine_RunFilter(v8engine, v8filter, result);
    if (createCallbackLog)
    {
        *callbacks = m_filterCallback.GetCallbackLog();
    }
    ResetRunningFilter();
    strResult = result;
    return retval;
}

int mc_FilterEngine::RunFilterWithCallbackLog(const mc_Filter *filter, std::string &strResult,
                                              json_spirit::Array *callbacks)
{
    if (fDebug)
        LogPrint("v8filter", "v8filter: mc_FilterEngine::RunFilterWithCallbackLog\n");
    int retval = this->RunFilter(filter, strResult, true, callbacks);
    return retval;
}

void mc_FilterEngine::TerminateFilter(std::string reason)
{
    if (fDebug)
        LogPrint("v8filter", "v8filter: mc_FilterEngine::TerminateFilter\n");
    if (m_runningFilter != nullptr)
    {
        auto v8engine = static_cast<V8Engine_t *>(m_Impl);
        auto v8filter = static_cast<V8Filter_t *>(m_runningFilter->m_Impl);
        V8Engine_TerminateFilter(v8engine, v8filter, reason.c_str());
    }
}

void mc_FilterEngine::SetRunningFilter(const mc_Filter *filter)
{
    m_runningFilter = filter;
    if (m_watchdog == nullptr)
    {
        m_watchdog = new Watchdog(boost::bind(&mc_FilterEngine::TerminateFilter, this, _1));
    }
    m_watchdog->FilterStarted(filter->Timeout());
}

void mc_FilterEngine::ResetRunningFilter()
{
    assert(m_watchdog != nullptr);
    m_runningFilter = nullptr;
    m_watchdog->FilterEnded();
}
