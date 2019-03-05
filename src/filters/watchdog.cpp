// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "watchdog.h"
#include "core/init.h"
//#include "protocol/filter.h"
#include "utils/define.h"
#include "utils/util.h"

void WatchdogState::Set(WatchdogState::State state)
{
    if (m_state != state)
    {
        {
            boost::lock_guard<boost::mutex> lock(m_mutex);
            m_state = state;
        }
        m_condVar.notify_all();
    }
}

std::string WatchdogState::Str() const
{
    std::string stateStr;
    switch (m_state)
    {
    case State::INIT:
        stateStr = "INIT";
        break;
    case State::IDLE:
        stateStr = "IDLE";
        break;
    case State::RUNNING:
        stateStr = "RUNNING";
        break;
    case State::POISON_PILL:
        stateStr = "POISON_PILL";
        break;
    }
    return tfm::format("%s %s", m_name, stateStr);
}

void WatchdogState::WaitState(WatchdogState::State state)
{
    if (m_state != state)
    {
        boost::unique_lock<boost::mutex> lock(m_mutex);
        m_condVar.wait(lock, [this, state] { return m_state == state; });
    }
}

void WatchdogState::WaitNotState(WatchdogState::State state)
{
    if (m_state == state)
    {
        boost::unique_lock<boost::mutex> lock(m_mutex);
        m_condVar.wait(lock, [this, state] { return m_state != state; });
    }
}

template <class Rep, class Period>
bool WatchdogState::WaitStateFor(WatchdogState::State state, const boost::chrono::duration<Rep, Period> &timeout)
{
    boost::unique_lock<boost::mutex> lock(m_mutex);
    return m_condVar.wait_for(lock, timeout, [this, state] { return m_state == state; });
}

template <class Rep, class Period>
bool WatchdogState::WaitNotStateFor(WatchdogState::State state, const boost::chrono::duration<Rep, Period> &timeout)
{
    boost::unique_lock<boost::mutex> lock(m_mutex);
    return m_condVar.wait_for(lock, timeout, [this, state] { return m_state != state; });
}

void Watchdog::Watchdog::Zero()
{
    m_thread = nullptr;
    m_timeout = 0;
}

int Watchdog::Destroy()
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

void Watchdog::FilterStarted(int timeout)
{
    if (fDebug)
        LogPrint("", ": Watchdog::FilterStarted(timeout=%d) %s\n", timeout, m_actualState.Str());
    if (m_thread == nullptr)
    {
        if (fDebug)
            LogPrint("", ": Watchdog::FilterStarted create thread with watchdogTask\n");
        m_thread = new boost::thread(std::bind(&Watchdog::WatchdogTask, this));
    }
    m_actualState.WaitState(WatchdogState::State::IDLE);
    m_timeout = timeout;
    m_requestedState.Set(WatchdogState::State::RUNNING);
    m_actualState.WaitState(WatchdogState::State::RUNNING);
}

void Watchdog::FilterEnded()
{
    if (fDebug)
        LogPrint("", ": Watchdog::FilterEnded %s\n", m_actualState.Str());
    m_actualState.WaitState(WatchdogState::State::RUNNING);
    m_requestedState.Set(WatchdogState::State::IDLE);
    m_actualState.WaitState(WatchdogState::State::IDLE);
}

void Watchdog::Shutdown()
{
    if (fDebug)
        LogPrint("", ": Watchdog::Shutdown\n");
    m_requestedState.Set(WatchdogState::State::POISON_PILL);
    m_actualState.WaitState(WatchdogState::State::POISON_PILL);
    this->Destroy();
}

void Watchdog::WatchdogTask()
{
    if (fDebug)
        LogPrint("", ": Watchdog::watchdogTask\n");
    while (true)
    {
        std::string msg = tfm::format(": Watchdog::watchdogTask %s - %%s", m_requestedState.Str());
        switch (m_requestedState.Get())
        {
        case WatchdogState::State::POISON_PILL:
            if (fDebug)
                LogPrint("", msg.c_str(), "committing suicide\n");
            m_actualState.Set(WatchdogState::State::POISON_PILL);
            return;

        case WatchdogState::State::RUNNING:
            m_actualState.Set(WatchdogState::State::RUNNING);
            if (m_timeout > 0)
            {
                if (fDebug)
                    LogPrint("", msg.c_str(), "entering timed wait\n");
                bool finished = m_requestedState.WaitNotStateFor(WatchdogState::State::RUNNING,
                                                                 boost::chrono::milliseconds(m_timeout));
                if (!finished)
                {
                    if (fDebug)
                        LogPrint("", msg.c_str(), "timeout -> terminating filter\n");
                    m_taskTerminator(tfm::format("Filter aborted due to timeout after %d ms", m_timeout).c_str());
                    m_requestedState.WaitNotState(WatchdogState::State::RUNNING);
                }
            }
            else
            {
                if (fDebug)
                    LogPrint("", msg.c_str(), "entering inifinte wait\n");
                m_requestedState.WaitNotState(WatchdogState::State::RUNNING);
            }
            break;

        case WatchdogState::State::INIT:
            // fall through
        case WatchdogState::State::IDLE:
            if (fDebug)
                LogPrint("", msg.c_str(), "entering idle state\n");
            m_actualState.Set(WatchdogState::State::IDLE);
            m_requestedState.WaitNotState(WatchdogState::State::IDLE);
            break;
        }
    }
}
