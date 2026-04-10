#include <s2/triple_buffer.hpp>
#include <thread>
#include <atomic>
#include <chrono>
#include <gtest/gtest.h>

namespace s2
{

// Базовый тест: writer пишет, reader читает
TEST(TripleBuffer, BasicWriteRead)
{
    TripleBuffer<int> buffer(0);

    // Writer
    buffer.writer_buffer() = 42;
    buffer.publish();

    // Reader — acquire_read возвращает опубликованное значение
    int val = buffer.acquire_read();
    EXPECT_EQ(val, 42);

    // Ещё одна запись и публикация
    buffer.writer_buffer() = 100;
    buffer.publish();
    val = buffer.acquire_read();
    EXPECT_EQ(val, 100);
}

// Тест: writer и reader работают с разными буферами
TEST(TripleBuffer, WriterReaderNotSame)
{
    TripleBuffer<int> buffer(0);

    buffer.writer_buffer() = 10;
    int& reader = buffer.acquire_read();

    // В начальный момент reader содержит init значение
    EXPECT_EQ(reader, 0);

    // Writer и reader — разные буферы
    buffer.writer_buffer() = 20;
    EXPECT_NE(reader, buffer.writer_buffer());
}

// Тест: read() возвращает последнее опубликованное
TEST(TripleBuffer, ReadReturnsReady)
{
    TripleBuffer<int> buffer(0);

    buffer.writer_buffer() = 55;
    buffer.publish();
    EXPECT_EQ(buffer.read(), 55);

    buffer.writer_buffer() = 77;
    buffer.publish();
    EXPECT_EQ(buffer.read(), 77);
}

// Многопоточный тест: writer публикует, reader читает
TEST(TripleBuffer, MultiThreaded)
{
    TripleBuffer<int> buffer(0);
    std::atomic<int> read_count{0};
    std::atomic<bool> done{false};

    // Reader thread
    std::thread reader_thread([&]() {
        while (!done) {
            int val = buffer.acquire_read();
            if (val > 0) {
                read_count.fetch_add(1);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });

    // Writer thread
    std::thread writer_thread([&]() {
        for (int i = 1; i <= 50; ++i) {
            buffer.writer_buffer() = i;
            buffer.publish();
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    });

    writer_thread.join();
    done = true;
    reader_thread.join();

    // Мы должны были прочитать хотя бы некоторые значения
    EXPECT_GT(read_count.load(), 0);
}

// Тест: default constructible
TEST(TripleBuffer, DefaultConstructible)
{
    TripleBuffer<int> buffer;
    EXPECT_EQ(buffer.read(), 0);
}

// Тест: конструктор с начальным значением
TEST(TripleBuffer, InitialValue)
{
    TripleBuffer<std::string> buffer("hello");
    EXPECT_EQ(buffer.read(), "hello");
    EXPECT_EQ(buffer.acquire_read(), "hello");
}

} // namespace s2