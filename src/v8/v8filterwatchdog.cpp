#include "v8filterwatchdog.h"
#include "core/init.h"
#include "filters/filter.h"
#include "utils/define.h"
#include "utils/util.h"

namespace mc_v8
{
void V8WatchdogState::Set(V8WatchdogState::State state)
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

std::string V8WatchdogState::Str() const
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

void V8WatchdogState::WaitState(V8WatchdogState::State state)
{
    if (m_state != state)
    {
        boost::unique_lock<boost::mutex> lock(m_mutex);
        m_condVar.wait(lock, [this, state] { return m_state == state; });
    }
}

void V8WatchdogState::WaitNotState(V8WatchdogState::State state)
{
    if (m_state == state)
    {
        boost::unique_lock<boost::mutex> lock(m_mutex);
        m_condVar.wait(lock, [this, state] { return m_state != state; });
    }
}

template <class Rep, class Period>
bool V8WatchdogState::WaitStateFor(V8WatchdogState::State state, const boost::chrono::duration<Rep, Period> &timeout)
{
    boost::unique_lock<boost::mutex> lock(m_mutex);
    return m_condVar.wait_for(lock, timeout, [this, state] { return m_state == state; });
}

template <class Rep, class Period>
bool V8WatchdogState::WaitNotStateFor(V8WatchdogState::State state, const boost::chrono::duration<Rep, Period> &timeout)
{
    boost::unique_lock<boost::mutex> lock(m_mutex);
    return m_condVar.wait_for(lock, timeout, [this, state] { return m_state != state; });
}

void V8FilterWatchdog::V8FilterWatchdog::Zero()
{
    m_thread = nullptr;
    m_timeout = 0;
}

int V8FilterWatchdog::Destroy()
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

void V8FilterWatchdog::FilterStarted(int timeout)
{
    if (fDebug)
        LogPrint("v8filter", "v8filter: Watchdog::FilterStarted(timeout=%d) %s\n", timeout, m_actualState.Str());
    if (m_thread == nullptr)
    {
        if (fDebug)
            LogPrint("v8filter", "v8filter: Watchdog::FilterStarted create thread with watchdogTask\n");
        m_thread = new boost::thread(std::bind(&V8FilterWatchdog::WatchdogTask, this));
    }
    m_actualState.WaitState(V8WatchdogState::State::IDLE);
    m_timeout = timeout;
    m_requestedState.Set(V8WatchdogState::State::RUNNING);
    m_actualState.WaitState(V8WatchdogState::State::RUNNING);
}

void V8FilterWatchdog::FilterEnded()
{
    if (fDebug)
        LogPrint("v8filter", "v8filter: Watchdog::FilterEnded %s\n", m_actualState.Str());
    m_actualState.WaitState(V8WatchdogState::State::RUNNING);
    m_requestedState.Set(V8WatchdogState::State::IDLE);
    m_actualState.WaitState(V8WatchdogState::State::IDLE);
}

void V8FilterWatchdog::Shutdown()
{
    if (fDebug)
        LogPrint("v8filter", "v8filter: Watchdog::Shutdown\n");
    m_requestedState.Set(V8WatchdogState::State::POISON_PILL);
    m_actualState.WaitState(V8WatchdogState::State::POISON_PILL);
    this->Destroy();
}

void V8FilterWatchdog::WatchdogTask()
{
    if (fDebug)
        LogPrint("v8filter", "v8filter: Watchdog::watchdogTask\n");
    while (true)
    {
        std::string msg = tfm::format("v8filter: Watchdog::watchdogTask %s - %%s", m_requestedState.Str());
        switch (m_requestedState.Get())
        {
        case V8WatchdogState::State::POISON_PILL:
            if (fDebug)
                LogPrint("v8filter", msg.c_str(), "committing suicide\n");
            m_actualState.Set(V8WatchdogState::State::POISON_PILL);
            return;

        case V8WatchdogState::State::RUNNING:
            m_actualState.Set(V8WatchdogState::State::RUNNING);
            if (m_timeout > 0)
            {
                if (fDebug)
                    LogPrint("v8filter", msg.c_str(), "entering timed wait\n");
                bool finished = m_requestedState.WaitNotStateFor(V8WatchdogState::State::RUNNING,
                                                                 boost::chrono::milliseconds(m_timeout));
                if (!finished)
                {
                    if (fDebug)
                        LogPrint("v8filter", msg.c_str(), "timeout -> terminating filter\n");
                    pFilterEngine->TerminateFilter(tfm::format("Filter aborted due to timeout after %d ms", m_timeout));
                    m_requestedState.WaitNotState(V8WatchdogState::State::RUNNING);
                }
            }
            else
            {
                if (fDebug)
                    LogPrint("v8filter", msg.c_str(), "entering inifinte wait\n");
                m_requestedState.WaitNotState(V8WatchdogState::State::RUNNING);
            }
            break;

        case V8WatchdogState::State::INIT:
            // fall through
        case V8WatchdogState::State::IDLE:
            if (fDebug)
                LogPrint("v8filter", msg.c_str(), "entering idle state\n");
            m_actualState.Set(V8WatchdogState::State::IDLE);
            m_requestedState.WaitNotState(V8WatchdogState::State::IDLE);
            break;
        }
    }
}

} // namespace mc_v8
