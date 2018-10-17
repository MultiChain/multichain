#include "filterwatchdog.h"
#include "core/init.h"
#include "filter.h"
#include "utils/util.h"
#include "utils/define.h"

void FilterWatchdog::Zero()
{
    m_thread = nullptr;
    m_timeout = 0;
    m_state = FilterWatchdog::State::IDLE;
}

int FilterWatchdog::Destroy()
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

void FilterWatchdog::FilterStarted(int timeout)
{
    LogPrint("v8filter", "v8filter: Watchdog::FilterStarted(timeout=%d) m_state=%s\n", timeout, this->StateStr());
    if (m_state == State::IDLE)
    {
        if (m_thread == nullptr)
        {
            LogPrint("v8filter", "v8filter: Watchdog::FilterStarted create thread with watchdogTask\n");
            m_thread = new boost::thread(boost::bind(&FilterWatchdog::watchdogTask, this));
            boost::this_thread::sleep_for(boost::chrono::microseconds(100));
        }
        {
            boost::lock_guard<boost::mutex> lock(m_mutex);
            LogPrint("v8filter", "v8filter: Watchdog::FilterStarted set timeout and RUNNING\n");
            m_timeout = timeout;
            m_state = State::RUNNING;
        }
        LogPrint("v8filter", "v8filter: Watchdog::FilterStarted notify m_state=%s\n", this->StateStr());
        m_condVar.notify_one();
    }
}

void FilterWatchdog::FilterEnded()
{
    LogPrint("v8filter", "v8filter: Watchdog::FilterEnded m_state=%s\n",  this->StateStr());
    if (m_state == State::RUNNING)
    {
        {
            boost::lock_guard<boost::mutex> lock(m_mutex);
            LogPrint("v8filter", "v8filter: Watchdog::FilterEnded set IDLE\n");
            m_state = State::IDLE;
        }
        LogPrint("v8filter", "v8filter: Watchdog::FilterEnded notify m_state=%s\n", this->StateStr());
        m_condVar.notify_one();
    }
}

void FilterWatchdog::Shutdown()
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

std::string FilterWatchdog::StateStr() const
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
    return tinyformat::format("UNKNOWN (%d)", m_state);
}

void FilterWatchdog::watchdogTask()
{
    LogPrint("v8filter", "v8filter: Watchdog::watchdogTask\n");
    boost::unique_lock<boost::mutex> lock(m_mutex);
    while (true)
    {
        std::string msg = tfm::format("v8filter: Watchdog::watchdogTask %s - ", this->StateStr());
//        LogPrint("v8filter", (msg + "idling\n").c_str());
        m_condVar.wait(lock);
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
                pFilterEngine->TerminateFilter();
            }
        }
    }
}
