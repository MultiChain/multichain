// Copyright (c) 2014-2018 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "filter.h"
#include "core/init.h"
#include "utils/define.h"
#include "utils/util.h"
#include "v8/v8engine.h"
#include "v8/v8filter.h"

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
    m_thread = nullptr;
    m_timeout = 0;
    m_state = mc_FilterWatchdog::State::IDLE;
}

int mc_FilterWatchdog::Destroy()
{
    if (m_thread != nullptr)
    {
        m_thread->interrupt();
        m_thread->join();
        delete m_thread;
    }
    this->Zero();
    return MC_ERR_NOERROR;
}

void mc_FilterWatchdog::FilterStarted(int timeout)
{
    LogPrint("v8filter", "v8filter: Watchdog::FilterStarted(timeout=%d) m_state=%s\n", timeout, this->StateStr());
    if (m_state == State::IDLE)
    {
        if (m_thread == nullptr)
        {
            LogPrint("v8filter", "v8filter: Watchdog::FilterStarted create thread with watchdogTask\n");
            m_thread = new boost::thread(boost::bind(&mc_FilterWatchdog::watchdogTask, this));
            boost::this_thread::sleep_for(boost::chrono::microseconds(100));
        }
        {
            boost::lock_guard<boost::mutex> lock(m_mutex);
            m_timeout = timeout;
            m_state = State::RUNNING;
        }
        LogPrint("v8filter", "v8filter: Watchdog::FilterStarted notify m_state=%s\n", this->StateStr());
        m_condVar.notify_one();
    }
}

void mc_FilterWatchdog::FilterEnded()
{
    LogPrint("v8filter", "v8filter: Watchdog::FilterEnded m_state=%s\n", this->StateStr());
    if (m_state == State::RUNNING)
    {
        {
            boost::lock_guard<boost::mutex> lock(m_mutex);
            m_state = State::IDLE;
        }
        LogPrint("v8filter", "v8filter: Watchdog::FilterEnded notify m_state=%s\n", this->StateStr());
        m_condVar.notify_one();
    }
}

void mc_FilterWatchdog::Shutdown()
{
    LogPrint("v8filter", "v8filter: Watchdog::Shutdown\n");
    {
        boost::lock_guard<boost::mutex> lock(m_mutex);
        m_state = State::POISON_PILL;
    }
    LogPrint("v8filter", "v8filter: Watchdog::Shutdown notify m_state=%s\n", this->StateStr());
    m_condVar.notify_one();
    boost::this_thread::sleep_for(boost::chrono::microseconds(100));
    this->Destroy();
}

std::string mc_FilterWatchdog::StateStr() const
{
    switch (m_state)
    {
    case State::IDLE:
        return "IDLE";
    case State::RUNNING:
        return "RUNNING";
    case State::POISON_PILL:
        return "POISON_PILL";
    }
    return tfm::format("UNKNOWN (%d)", m_state);
}

void mc_FilterWatchdog::watchdogTask()
{
    State known_state = State::IDLE;

    LogPrint("v8filter", "v8filter: Watchdog::watchdogTask\n");
    boost::unique_lock<boost::mutex> lock(m_mutex);
    while (true)
    {
        std::string msg = tfm::format("v8filter: Watchdog::watchdogTask %s - ", this->StateStr());
        while (known_state == m_state)
        {
            m_condVar.wait(lock);
        }
        known_state = m_state;
        if (m_state == State::POISON_PILL)
        {
            LogPrint("v8filter", (msg + "committing suicide\n").c_str());
            return;
        }
        else if (m_state == State::RUNNING && m_timeout > 0)
        {
            LogPrint("v8filter", (msg + "entering timed wait\n").c_str());
            boost::cv_status cvs = m_condVar.wait_for(lock, boost::chrono::milliseconds(m_timeout));
            if (cvs == boost::cv_status::timeout)
            {
                LogPrint("v8filter", (msg + "timeout -> terminating filter\n").c_str());
                pFilterEngine->TerminateFilter(tfm::format("Filter aborted due to timeout after %d ms", m_timeout));
                // Wait for filter to actually end
                m_condVar.wait(lock);
            }
        }
    }
}
