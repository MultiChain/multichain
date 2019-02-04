// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#ifndef WATCHDOG_H
#define WATCHDOG_H

#include <boost/thread.hpp>

class WatchdogState
{
  public:
    enum State
    {
        INIT,
        IDLE,
        RUNNING,
        POISON_PILL
    };

    WatchdogState(std::string name, State state = State::INIT) : m_name(name), m_state(state)
    {
    }

    State Get() const
    {
        return m_state;
    }
    void Set(State state);
    std::string Str() const;

    void WaitState(State state);
    void WaitNotState(State state);
    template <class Rep, class Period>
    bool WaitStateFor(State state, const boost::chrono::duration<Rep, Period> &timeout);
    template <class Rep, class Period>
    bool WaitNotStateFor(State state, const boost::chrono::duration<Rep, Period> &timeout);

  protected:
    std::string m_name;
    boost::condition_variable m_condVar;
    boost::mutex m_mutex;
    State m_state;
};

class Watchdog
{
  public:
    Watchdog(std::function<void(const char *)> taskTerminator) : m_taskTerminator(taskTerminator)
    {
        this->Zero();
    }

    ~Watchdog()
    {
        this->Destroy();
    }

    void Zero();
    int Destroy();

    /**
     * @brief Notfies the watchdog that a filter started runnug, with a given timeout.
     * @param timeout   The number of millisecond to allow the filtr to run.
     */
    void FilterStarted(int timeout);

    /**
     * @brief Notfies the watchdog that a filter stopped running.
     */
    void FilterEnded();

    /**
     * @brief Terminate the watchdog.
     */
    void Shutdown();

  private:
    boost::thread *m_thread;
    int m_timeout;
    WatchdogState m_requestedState{"requested"};
    WatchdogState m_actualState{"actual"};
    std::function<void(const char *)> m_taskTerminator;

    void WatchdogTask();
};

#endif // V8FILTERWATCHDOG_H
