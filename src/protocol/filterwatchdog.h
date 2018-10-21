// Copyright (c) 2014-2018 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#ifndef MULTICHAIN_FILTERWATCHDOG_H
#define MULTICHAIN_FILTERWATCHDOG_H

#include <boost/thread.hpp>

/**
 * @brief Monitor filter execution and stop the filer function if it takes more than a specified timeout.
 */
class FilterWatchdog
{
  public:
    FilterWatchdog()
    {
        Zero();
    }
    ~FilterWatchdog()
    {
        Destroy();
    }

    void Zero();
    int Destroy();

    /**
     * @brief Notfies the watchdog that a filter started runnug, with a given timeout.
     * @param timeout   The number of millisecond to allow the filtr to run.
     */
    void FilterStarted(int timeout = 1000);

    /**
     * @brief Notfies the watchdog that a filter stopped running.
     */
    void FilterEnded();

    /**
     * @brief Terminate the watchdog.
     */
    void Shutdown();

  private:
    enum State
    {
        IDLE,
        RUNNING,
        POISON_PILL
    };

    boost::thread *m_thread;
    boost::condition_variable m_condVar;
    boost::mutex m_mutex;
    int m_timeout;
    State m_state;

    std::string StateStr() const;
    void watchdogTask();
};

#endif // MULTICHAIN_FILTERWATCHDOG_H
