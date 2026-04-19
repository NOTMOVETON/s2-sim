#pragma once

/**
 * @file triple_buffer.hpp
 * Тройная буферизация для thread-safe обмена данными между двумя потоками.
 *
 * Один writer (sim thread) и один reader (transport thread).
 * Writer и reader никогда не работают с одним буфером одновременно.
 * Атомарный swap без локов.
 *
 * Архитектура (ARCHITECTURE.md раздел 18):
 *   - writer_buffer() — writer пишет в writer buffer
 *   - publish() — атомарный swap writer <-> ready
 *   - acquire_read() — атомарный swap ready <-> reader, возвращает reader
 */

#include <atomic>
#include <array>
#include <mutex>
#include <utility>

namespace s2
{

/**
 * @brief Тройной буфер для одного writer и одного reader.
 *
 * Three состояния буфера:
 *   - WRITER: поток симуляции пишет сюда
 *   - READY: готовый для чтения (результат последнего publish)
 *   - READER: поток транспорта читает отсюда
 *
 * publish() меняет WRITER <-> READY
 * acquire_read() меняет READY <-> READER и возвращает старое READY
 */
template <typename T>
class TripleBuffer
{
public:
    TripleBuffer()
    {
        buffers_.fill(T{});
    }

    /**
     * @brief Конструктор с начальным значением.
     */
    explicit TripleBuffer(const T& initial)
    {
        buffers_[0] = initial;
        buffers_[1] = initial;
        buffers_[2] = initial;
    }

    /**
     * @brief Получить mutable ссылку на writer buffer для записи.
     * Вызывается из sim thread перед записью данных.
     */
    T& writer_buffer()
    {
        std::lock_guard<std::mutex> lock(writer_mutex_);
        return buffers_[static_cast<size_t>(Index::Writer)];
    }

    /**
     * @brief Опубликовать данные — атомарный swap writer <-> ready.
     * Вызывается из sim thread после записи в writer_buffer().
     */
    void publish()
    {
        std::lock_guard<std::mutex> wlock(writer_mutex_);
        std::lock_guard<std::mutex> rlock(ready_mutex_);
        std::swap(buffers_[static_cast<size_t>(Index::Writer)],
                  buffers_[static_cast<size_t>(Index::Ready)]);
    }

    /**
     * @brief Получить данные для чтения — атомарный swap ready <-> reader.
     * Вызывается из transport thread.
     * Возвращает ссылку на reader buffer (который содержит старые ready данные).
     */
    T& acquire_read()
    {
        std::lock_guard<std::mutex> rlock(ready_mutex_);
        std::lock_guard<std::mutex> rdlock(reader_mutex_);
        std::swap(buffers_[static_cast<size_t>(Index::Ready)],
                  buffers_[static_cast<size_t>(Index::Reader)]);
        return buffers_[static_cast<size_t>(Index::Reader)];
    }

    /**
     * @brief Прочитать данные без swap (для случаев когда не нужен acquire).
     */
    const T& read() const
    {
        std::lock_guard<std::mutex> rlock(ready_mutex_);
        return buffers_[static_cast<size_t>(Index::Ready)];
    }

private:
    enum class Index { Writer = 0, Ready = 1, Reader = 2 };

    std::array<T, 3> buffers_;

    // Мьютексы защищают доступ к каждому слоту
    mutable std::mutex writer_mutex_;
    mutable std::mutex ready_mutex_;
    mutable std::mutex reader_mutex_;
};

} // namespace s2