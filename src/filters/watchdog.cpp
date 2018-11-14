#include "filters/watchdog.h"
#include "utils/util.h"

Watchdog::Watchdog(std::function<void(const char *)> taskTerminator)
    : m_thread(nullptr), m_state(Watchdog::State::IDLE), m_timeout(0), m_taskTerminator(taskTerminator)
{
    m_thread = new boost::thread(&Watchdog::EventLoop, this);
}

Watchdog::~Watchdog()
{
    if (m_thread != nullptr)
    {
        this->PostPoisonPill();
    }
}

void Watchdog::PostTaskStarted(int timeout)
{
    if (fDebug)
        LogPrint("v8", "Watchdog::PostStarted(timeout=%d)\n", timeout);

    m_timeout.store(timeout);
    this->PostEvent(Event::TASK_STARTED);
}

void Watchdog::PostTaskEnded()
{
    if (fDebug)
        LogPrint("v8", "Watchdog::PostEnded()\n");

    m_timeout.store(0);
    this->PostEvent(Event::TASK_ENDED);
    boost::unique_lock<boost::mutex> lock(m_mutex);
    m_cv.wait(lock, [this] { return m_state.load() != State::TASK_RUNNING; });
}

void Watchdog::PostPoisonPill()
{
    if (fDebug)
        LogPrint("v8", "Watchdog::PostPoison()\n");

    this->PostEvent(Event::POISON_PILL);
    m_thread->join();
    delete m_thread;
    m_thread = nullptr;
}

std::string Watchdog::EventStr(Watchdog::Event event)
{
    std::string eventStr;
    switch (event)
    {
    case Event::TASK_STARTED:
        eventStr = "task started";
        break;
    case Event::TASK_ENDED:
        eventStr = "task ended";
        break;
    case Event::POISON_PILL:
        eventStr = "take poison";
        break;
    }
    return eventStr;
}

void Watchdog::PostEvent(Watchdog::Event event)
{
    if (fDebug)
        LogPrint("v8", "Watchdog::PostEvent(event=%s)\n", EventStr(event));

    {
        boost::lock_guard<boost::mutex> lock(m_mutex);
        m_queue.push(event);
    }
    m_cv.notify_one();
}

void Watchdog::EventLoop()
{
    Event event;
    while (true)
    {
        if (fDebug)
            LogPrint("v8", "Watchdog::EventLoop(): wait for queue\n");
        {
            boost::unique_lock<boost::mutex> lock(m_mutex);
            m_cv.wait(lock, [this] { return !m_queue.empty(); });
        }
        {
            boost::lock_guard<boost::mutex> lock(m_mutex);
            event = m_queue.pull();
        }

        switch (event)
        {
        case Event::TASK_STARTED:
            if (fDebug)
                LogPrint("v8", "Watchdog::EventLoop(): task started timeout=%d\n", m_timeout.load());
            m_state.store(State::TASK_RUNNING);
            {
                boost::unique_lock<boost::mutex> lock(m_mutex);
                if (m_timeout.load() > 0)
                {
                    if (fDebug)
                        LogPrint("v8", "Watchdog::EventLoop(): timed wait\n");
                    if (m_cv.wait_for(lock, boost::chrono::milliseconds(m_timeout.load())) == boost::cv_status::timeout)
                    {
                        if (fDebug)
                            LogPrint("v8", "Watchdog::EvenTLoop(): timeout\n");
                        m_state.store(State::TASK_TIMED_OUT);
                        m_taskTerminator(
                            tinyformat::format("Filter aborted due to timeout after %d ms", m_timeout).c_str());
                    }
                }
            }
            break;

        case Event::TASK_ENDED:
            if (fDebug)
                LogPrint("v8", "Watchdog::EventLoop(): task ended\n");
            m_state.store(State::IDLE);
            m_cv.notify_one();
            break;

        case Event::POISON_PILL:
            if (fDebug)
                LogPrint("v8", "Watchdog::EvenTLoop(): swallowed poison pill\n");
            return;
        }
    }
}
