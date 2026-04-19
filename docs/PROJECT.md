# S2 — Проект

## Что это

S2 — лёгкий кинематический симулятор для тестирования высокоуровневой робототехнической логики (fleet management, миссии, взаимодействие с инфраструктурой). Не физический движок. Команды исполняются точно, физика не симулируется, масштабируется до 100 агентов.

**Ключевое упрощение:** всё взаимодействие робота с миром — высокоуровневые факты. Робот не «видит маркер камерой» — он «оказался рядом и получил позицию». Робот не «нажимает кнопку двери» — он «отправляет команду и ждёт смены состояния».

## Технологии

- **Язык:** C++17
- **Сборка:** CMake
- **Среда:** Docker (Ubuntu 22.04)
- **Визуализатор:** браузерный (Three.js), подключается по WebSocket
- **ROS:** НЕ является зависимостью ядра. Только транспортный плагин (позже)
- **Тесты:** Google Test

## Структура проекта

```
s2/
├── docker/
│   ├── Dockerfile
│   └── docker-compose.yml
│
├── workspace/
│   ├── CMakeLists.txt              # корневой CMake
│   │
│   ├── s2_core/                    # ядро симуляции
│   │   ├── CMakeLists.txt
│   │   ├── include/s2/
│   │   │   ├── types.hpp           # Pose3D, Vec3, Id types
│   │   │   ├── agent.hpp           # Agent struct
│   │   │   ├── actor.hpp           # Actor + FSM
│   │   │   ├── prop.hpp            # Prop struct
│   │   │   ├── zone.hpp            # Zone struct
│   │   │   ├── world.hpp           # SimWorld — хранит всё
│   │   │   ├── shared_state.hpp    # SharedState + Contributions + Resolver
│   │   │   ├── sim_engine.hpp      # главный цикл
│   │   │   ├── sim_bus.hpp         # Event Bus
│   │   │   ├── zone_system.hpp     # enter/exit detection
│   │   │   ├── raycast_engine.hpp  # BVH + raycast
│   │   │   ├── kinematic_tree.hpp  # frame hierarchy
│   │   │   ├── attachment.hpp      # attachment system
│   │   │   └── interfaces/         # базовые интерфейсы модулей
│   │   │       ├── actuation_module.hpp
│   │   │       ├── sensor_module.hpp
│   │   │       ├── interaction_module.hpp
│   │   │       ├── resource_module.hpp
│   │   │       ├── effect_plugin.hpp
│   │   │       └── actor_behavior.hpp
│   │   ├── src/
│   │   └── tests/
│   │
│   ├── s2_plugins/                 # встроенные плагины
│   │   ├── actuation/              # DiffDrive, Multicopter, ...
│   │   ├── sensors/                # Lidar2D, GNSS, Odometry
│   │   ├── interactions/           # Grabber, DoorOpener
│   │   ├── resources/              # Battery, Payload
│   │   ├── effects/                # Ice, Wind, Fog, Charging, ...
│   │   └── actors/                 # DoorBehavior, PedestrianBehavior
│   │
│   ├── s2_transport/                # транспортный адаптер
│   │   ├── CMakeLists.txt
│   │   ├── include/s2/
│   │   │   ├── sim_transport_bridge.hpp  # мост SimEngine ↔ ITransportAdapter
│   │   │   └── ros2_transport_adapter.hpp
│   │   └── src/
│   │       ├── sim_transport_bridge.cpp
│   │       ├── ros2_transport_adapter.cpp
│   │       └── ros2_transport_adapter_stub.cpp
│   │
│   ├── s2_msgs/                    # ROS2 кастомные сообщения
│   │   ├── CMakeLists.txt
│   │   ├── package.xml
│   │   └── srv/
│   │       └── PluginCall.srv      # request_json → success + response_json
│   │
│   ├── s2_visualizer/
│   │   ├── CMakeLists.txt
│   │   ├── src/                    # C++ WebSocket server
│   │   │   └── viz_server.cpp
│   │   └── web/                    # HTML + JS клиент
│   │       ├── index.html
│   │       └── js/
│   │
│   └── s2_config/
│       └── scenes/                 # YAML сцены
│           ├── test_scene.yaml
│           └── test_ros2_full.yaml # 3 робота, domain 50/51/52
│
└── docs/
    ├── PROJECT.md                  # этот файл
    ├── ARCHITECTURE.md             # полная архитектура
    └── tasks/
        ├── 00-infrastructure.md
        ├── 01-core-types.md
        ├── ...
```

## Правила разработки

### Для AI-агента

1. **Читай task-файл целиком** перед началом работы. Там всё: контекст, что делать, как проверить.
2. **Иди строго по порядку задач.** 00 → 01 → 02 → ... Не забегай вперёд.
3. **Каждая задача = один рабочий шаг.** После каждого шага — компиляция + тесты должны проходить.
4. **Если видишь `[СПРОСИТЬ]` — спроси пользователя.** Не принимай архитектурные решения самостоятельно.
5. **Не тащи ROS.** Ядро компилируется без ROS. Зависимости: Eigen3, yaml-cpp, nlohmann_json, Google Test. Всё.
6. **Всё в Docker.** `docker compose up` должен собрать и запустить.
7. **Пиши тесты.** Каждый шаг включает тесты. Они должны проходить.
8. **Минимализм.** Не добавляй то, что не просят в текущей задаче. Даже если «потом понадобится».

### Agile-подход

Каждая задача — это:
```
1. Написать код (минимум, который работает)
2. Написать тест (проверить что работает)
3. Запустить (docker compose build && docker compose run tests)
4. Показать результат
```

Не пытайся реализовать всю систему разом. Маленький шаг → проверка → следующий шаг.

### Стиль кода

- C++17, `-Wall -Wextra -Werror`
- `snake_case` для функций и переменных
- `PascalCase` для типов и классов
- `UPPER_CASE` для констант
- namespace `s2`
- `#pragma once` для header guards
- `std::unique_ptr` для ownership, raw pointers для наблюдения
- Никаких `using namespace std;`

## Зависимости (все ставятся в Docker)

| Библиотека | Зачем | Версия |
|-----------|-------|--------|
| Eigen3 | Линейная алгебра, трансформации | 3.4+ |
| yaml-cpp | Парсинг конфигов и сцен | 0.7+ |
| nlohmann/json | JSON для WebSocket протокола | 3.11+ |
| Google Test | Тестирование | 1.14+ |
| uWebSockets | WebSocket сервер для визуализатора | latest |
| (optional) Embree | BVH raycast (можно позже, начать с простого) | 4.x |

## Что уже решено (кратко)

Полная архитектура — в `ARCHITECTURE.md`. Вот ключевые решения:

- **Ядро однопоточное**, детерминированное
- **4 типа модулей:** Actuation, Sensor, Interaction, Resource
- **Модули не знают друг друга**, общаются через Shared State, Event Bus, World Query, Kernel Commands
- **Contribution/Resolver** для multi-source state (scale перемножается, additions складываются, locks через OR)
- **Зоны** — триггеры, эффекты — реакции (MODIFIER, CONTINUOUS, MUTATION, SENSOR)
- **Акторы** — FSM (двери, лифты, пешеходы — единый паттерн)
- **Attachment** — единый механизм привязки (объект к роботу, робот к лифту)
- **Визуализатор** — отдельный процесс, браузер, WebSocket
- **Транспорт** — `ITransportAdapter` + `SimTransportBridge`; сенсор не знает про ROS; `Ros2TransportAdapter` per domain с per-robot TF-деревом
- **TF** — `earth→map` привязан к точке спавна (уникален для каждого робота), `odom→base_link` относительно старта
