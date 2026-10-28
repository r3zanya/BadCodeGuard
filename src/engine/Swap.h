#pragma once

#include <QMutex>
#include <memory>

namespace bcg {

// Holds an immutable object that can be replaced while other threads keep
// using the previous one (they hold their own shared_ptr until they are done).
template <typename T>
class Swap {
public:
    std::shared_ptr<const T> get() const
    {
        QMutexLocker lock(&m_mutex);
        return m_value;
    }
    void set(std::shared_ptr<const T> value)
    {
        QMutexLocker lock(&m_mutex);
        m_value = std::move(value);
    }

private:
    mutable QMutex m_mutex;
    std::shared_ptr<const T> m_value;
};

} // namespace bcg
