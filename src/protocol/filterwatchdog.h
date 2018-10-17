// Copyright (c) 2014-2018 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#ifndef MULTICHAIN_FILTERWATCHDOG_H
#define MULTICHAIN_FILTERWATCHDOG_H
#include <boost/thread.hpp>

class FilterWatchdog
{
    enum State
    {
        IDLE,
        RUNNING,
        POISON_PILL
    };

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

    void FilterStarted(int timeout = 1000);
    void FilterEnded();

    void Shutdown();

    std::string StateStr() const;

  private:
    boost::thread *m_thread;
    boost::condition_variable m_condVar;
    boost::mutex m_mutex;
    int m_timeout;
    State m_state;

    void watchdogTask();
};

#endif // MULTICHAIN_FILTERWATCHDOG_H
