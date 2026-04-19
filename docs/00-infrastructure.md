# Task 00 — Инфраструктура: Docker + CMake + пустой проект

> **Предыдущий шаг:** нет (первая задача)
> **Следующий шаг:** `01-core-types.md`
> **Время:** ~30 минут

## Цель

Рабочая среда разработки. После этого шага: `docker compose build` собирает контейнер, `docker compose run dev bash` даёт shell с CMake, компилятором и всеми зависимостями, `docker compose run tests` запускает пустой тест который проходит.

## Что сделать

### 1. Создать структуру папок

```
s2/
├── docker/
│   ├── Dockerfile
│   └── docker-compose.yml
├── workspace/
│   ├── CMakeLists.txt
│   ├── s2_core/
│   │   ├── CMakeLists.txt
│   │   ├── include/s2/
│   │   │   └── .gitkeep
│   │   ├── src/
│   │   │   └── .gitkeep
│   │   └── tests/
│   │       └── test_smoke.cpp
│   ├── s2_plugins/
│   │   └── CMakeLists.txt
│   ├── s2_visualizer/
│   │   ├── CMakeLists.txt
│   │   ├── src/
│   │   └── web/
│   │       └── index.html
│   └── s2_config/
│       └── scenes/
└── docs/
```

### 2. Dockerfile

```dockerfile
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    libeigen3-dev \
    libyaml-cpp-dev \
    nlohmann-json3-dev \
    libgtest-dev \
    libgmock-dev \
    zlib1g-dev \
    libssl-dev \
    pkg-config \
    && rm -rf /var/lib/apt/lists/*

# uWebSockets (для визуализатора, ставим сразу)
RUN git clone --recurse-submodules https://github.com/uNetworking/uWebSockets.git /tmp/uws \
    && cp -r /tmp/uws/src/* /usr/local/include/ \
    && cp -r /tmp/uws/uSockets/src/* /usr/local/include/ \
    && cd /tmp/uws/uSockets && make && cp uSockets.a /usr/local/lib/ \
    && rm -rf /tmp/uws

WORKDIR /workspace
```

**[СПРОСИТЬ]** если uWebSockets не соберётся — можно заменить на libwebsocketpp или Beast (Boost). Спроси пользователя какую альтернативу предпочитает.

### 3. docker-compose.yml

```yaml
version: '3.8'

services:
  dev:
    build:
      context: ../
      dockerfile: docker/Dockerfile
    volumes:
      - ../workspace:/workspace
    ports:
      - "8765:8765"    # WebSocket визуализатора
      - "8080:8080"    # HTTP для web-клиента визуализатора
    stdin_open: true
    tty: true
    command: bash

  build:
    build:
      context: ../
      dockerfile: docker/Dockerfile
    volumes:
      - ../workspace:/workspace
    command: >
      bash -c "cd /workspace && mkdir -p build && cd build 
      && cmake .. -DCMAKE_BUILD_TYPE=Debug 
      && make -j$(nproc)"

  tests:
    build:
      context: ../
      dockerfile: docker/Dockerfile
    volumes:
      - ../workspace:/workspace
    command: >
      bash -c "cd /workspace && mkdir -p build && cd build 
      && cmake .. -DCMAKE_BUILD_TYPE=Debug 
      && make -j$(nproc) 
      && ctest --output-on-failure"
```

### 4. Корневой CMakeLists.txt (workspace/)

```cmake
cmake_minimum_required(VERSION 3.16)
project(s2 VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Зависимости
find_package(Eigen3 3.3 REQUIRED NO_MODULE)
find_package(yaml-cpp REQUIRED)
find_package(nlohmann_json 3.0 REQUIRED)
find_package(GTest REQUIRED)

enable_testing()

add_subdirectory(s2_core)
# add_subdirectory(s2_plugins)      # позже
# add_subdirectory(s2_visualizer)   # позже
```

### 5. s2_core/CMakeLists.txt

```cmake
add_library(s2_core STATIC
    src/placeholder.cpp    # пустой файл чтобы библиотека собралась
)

target_include_directories(s2_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(s2_core PUBLIC
    Eigen3::Eigen
    yaml-cpp
)

# Тесты
add_executable(s2_core_tests
    tests/test_smoke.cpp
)
target_link_libraries(s2_core_tests PRIVATE s2_core GTest::gtest_main)
add_test(NAME s2_core_tests COMMAND s2_core_tests)
```

### 6. Smoke test (s2_core/tests/test_smoke.cpp)

```cpp
#include <gtest/gtest.h>

TEST(Smoke, CompilerWorks) {
    EXPECT_EQ(1 + 1, 2);
}

TEST(Smoke, EigenAvailable) {
    Eigen::Vector3d v(1.0, 2.0, 3.0);
    EXPECT_DOUBLE_EQ(v.norm(), std::sqrt(14.0));
}
```

### 7. Placeholder (s2_core/src/placeholder.cpp)

```cpp
// Placeholder to make the library compile.
// Will be replaced by real code in subsequent tasks.
```

### 8. Минимальная веб-страница визуализатора (s2_visualizer/web/index.html)

```html
<!DOCTYPE html>
<html>
<head><title>S2 Visualizer</title></head>
<body>
    <h1>S2 Visualizer</h1>
    <p>Status: waiting for connection...</p>
    <script>
        // Will be replaced with Three.js in task 09
        console.log("S2 Visualizer loaded");
    </script>
</body>
</html>
```

## Критерии приёмки

```bash
# Из корня проекта s2/
cd docker && docker compose build          # собирается без ошибок
docker compose run tests                    # тесты проходят
docker compose run dev bash -c "echo ok"    # shell работает
```

## Чего НЕ делать

- Не ставить ROS
- Не писать код симуляции
- Не настраивать CI/CD
- Не добавлять Embree (позже)
