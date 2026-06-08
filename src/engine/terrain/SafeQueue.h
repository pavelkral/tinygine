#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>

template<typename T>
class SafeQueue {
    std::queue<T> m_qQueue;
    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    bool m_bShutdown = false;

public:
    void push(T val) {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_qQueue.push(val);
        }
        m_cv.notify_one();
    }

    bool pop_wait(T& val) {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cv.wait(lock, [this] { return !m_qQueue.empty() || m_bShutdown; });

        if (m_bShutdown && m_qQueue.empty()) return false;

        val = m_qQueue.front();
        m_qQueue.pop();
        return true;
    }

    bool try_pop(T& val) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_qQueue.empty()) return false;
        val = m_qQueue.front();
        m_qQueue.pop();
        return true;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::queue<T> empty;
        std::swap(m_qQueue, empty);
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_qQueue.size();
    }

    void signal_exit() {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_bShutdown = true;
        }
        m_cv.notify_all();
    }
};
