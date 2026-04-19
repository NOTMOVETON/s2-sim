# Быстрый старт для AI-агента

## Прочитай перед работой

1. `AGENTS.md` — методология работы, память и правила общения
2. `memory-bank/*.md` — постоянная память между сессиями и агентами
3. `PROJECT.md` — что за проект, структура, правила
4. `ARCHITECTURE.md` — полная архитектура (читай разделы по мере необходимости)
5. `docs/00-*.md` — текущая задача

## Порядок задач

```
00-infrastructure     Docker, CMake, пустой проект
01-core-types         Vec3, Pose3D, SharedState, SimBus
02-tick-loop          SimEngine, World, Agent shell, tick()
03-world-geometry     Heightmap, примитивы, raycast
04-agents-basic       DiffDrive actuation, кинематика, коллизии
05-zones-effects      ZoneSystem, эффекты, contribution/resolver
06-sensors-resources  Lidar2D, Battery, AgentSnapshot
07-actors-door        FSM, Door actor, Pedestrian actor
08-interactions       Grabber, DoorOpener, Attachment
09-visualizer         WebSocket server + Three.js client
10-transport-ros2     ROS2 adapter, тройная буферизация
```

## Главные правила

- **Один шаг за раз.** Не забегай вперёд.
- **Тесты обязательны.** `docker compose run tests` после каждого шага.
- **[СПРОСИТЬ]** = спроси пользователя, не решай сам.
- **Без ROS** до задачи 10.
- **C++17, минимальные зависимости.**
- **После значимого шага обновляй docs и memory-bank.**

## Ключевые архитектурные решения (не нарушай)

| Решение | Суть |
|---------|------|
| Однопоточное ядро | Весь тик в одном потоке, без синхронизации |
| 4 типа модулей | Actuation, Sensor, Interaction, Resource |
| Модули изолированы | Не знают друг друга, общаются через SharedState |
| Contribution/Resolver | scale перемножается, additions складываются, locks через OR |
| Зона = триггер | Зона детектирует enter/exit, эффект — реакция |
| Актор = FSM | Дверь, лифт, пешеход — единый паттерн |
| Мир меняется через ядро | Kernel Commands, не напрямую |
