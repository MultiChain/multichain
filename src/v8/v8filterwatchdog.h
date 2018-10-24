#ifndef V8FILTERWATCHDOG_H
#define V8FILTERWATCHDOG_H

#include <condition_variable>
#include <thread>

namespace mc_v8
{
class V8WatchdogState
{
  public:
    enum State
    {
        IDLE,
        RUNNING,
        POISON_PILL
    };

    V8WatchdogState(std::string name, State state = State::IDLE) : m_name(name), m_state(state)
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
    bool WaitStateFor(State state, const std::chrono::duration<Rep, Period> &timeout);
    template <class Rep, class Period>
    bool WaitNotStateFor(State state, const std::chrono::duration<Rep, Period> &timeout);

  protected:
    std::string m_name;
    std::condition_variable m_condVar;
    std::mutex m_mutex;
    State m_state;
};

class V8FilterWatchdog
{
  public:
    V8FilterWatchdog()
    {
        this->Zero();
    }

    ~V8FilterWatchdog()
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
    std::thread *m_thread;
    int m_timeout;
    V8WatchdogState m_requestedState{"requested"};
    V8WatchdogState m_actualState{"actual"};

    void WatchdogTask();
};
} // namespace mc_v8
#endif // V8FILTERWATCHDOG_H
