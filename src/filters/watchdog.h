#ifndef WATCHDOG_H
#define WATCHDOG_H

#include <boost/thread/condition_variable.hpp>
#include <boost/thread/sync_queue.hpp>
#include <boost/thread/synchronized_value.hpp>
#include <boost/thread/thread.hpp>
#include <functional>

class Watchdog
{
  public:
    Watchdog(std::function<void(const char *)> taskTerminator);
    ~Watchdog();

    void PostTaskStarted(int timeout = 0);
    void PostTaskEnded();
    void PostPoisonPill();

  private:
    enum Event
    {
        TASK_STARTED,
        TASK_ENDED,
        POISON_PILL
    };

    enum State
    {
        IDLE,
        TASK_RUNNING,
        TASK_TIMED_OUT
    };

    boost::sync_queue<Event> m_queue;
    boost::mutex m_mutex;
    boost::condition_variable m_cv;
    boost::thread *m_thread;
    std::atomic<State> m_state;
    std::atomic_int m_timeout;
    std::function<void(const char *)> m_taskTerminator;

    static std::string EventStr(Event event);
    void PostEvent(Event event);
    void EventLoop();
};

#endif // WATCHDOG_H
