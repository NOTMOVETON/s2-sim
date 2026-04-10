# System Patterns — S2

## Архитектура компонентов

### Слой ядра (s2_core)
- **World** — контейнер всех сущностей (агенты, пропы, акторы)
- **SimEngine** — главный цикл симуляции с фиксированным шагом dt
- **SimWorld** — контейнер сущностей для симуляции
- **Agent** — агент с plugins, state, pose, velocity, domain_id
- **WorldSnapshot** — снимок состояния для передачи визуализатору
- **TripleBuffer** — lock-free triple buffer для передачи данных между потоками

### Слой плагинов (s2_plugins)
- **IAgentPlugin** — интерфейс плагина агента (update, from_config, to_json, has_inputs, handle_input)
- **DiffDrivePlugin** — дифференциальный привод; поддерживает latch cmd_vel (команда сохраняется до явного {0,0})
- **GnssPlugin** — GNSS с поддержкой LLA координат (GeographicLib)
- **ImuPlugin** — инерциальный модуль (гироскоп + акселерометр)
- **Реестр плагинов** — создание плагинов по имени типа

### Слой визуализатора (s2_visualizer)
- **VizServer** — HTTP + SSE сервер для передачи данных в браузер (порт 1937)
- **SimEngineVizImpl** — мост между SimEngine и VizServer
- **Web UI** — Three.js сцена с OrbitControls и TransformControls

### Слой транспорта (s2_transport)
- **ROS2Transport** — подписка на `/cmd_vel` в изолированных ROS2-доменах
  - Один `rclcpp::Context` + `SingleThreadedExecutor` + spin-поток на каждый `domain_id`
  - Без глобального `rclcpp::init()` — транспорт сам управляет контекстами
  - Топик: `/cmd_vel` (без префикса), изоляция через DDS domain_id
- **Stub-режим** — заглушка без ROS2 для базовой сборки

## Потоки данных

### UI → Робот
```
[VizServer UI] → POST /command → [SimEngineCommandAdapter]
                                            ↓
                                [SimEngine::handle_plugin_input]
                                            ↓
                                [IAgentPlugin::handle_input]
                                            ↓
                                [DiffDrivePlugin::update]
                                            ↓
                                    [WorldSnapshot]
                                            ↓
                                [VizServer::publish] → SSE → [Browser]
```

### ROS2 → Робот
```
[ROS2 Publisher] → /cmd_vel (в domain X)
                         ↓
              [rclcpp::Context, domain X]
                         ↓
              [rclcpp::Subscription callback]
                         ↓
              [CmdVelCallback → SimEngine::handle_plugin_input]
                         ↓
              [DiffDrivePlugin::handle_input (latch)]
                         ↓
              [DiffDrivePlugin::update → world_velocity]
```

## Критические паттерны

### DiffDrive: latch vs one-shot
External input (от ROS2 или UI) сохраняется до получения новой команды (latch). Сброс флага `has_external_input_` намеренно НЕ производится в `update()`. Для остановки нужно явно отправить `{linear_velocity: 0, angular_velocity: 0}`.

**Почему:** при публикации 30 Гц и sim 100 Гц без latch 2/3 тиков без команды → мерцание скорости.

### DiffDrive: SharedState обратная связь
При external input `current_data.desired_linear/angular` НЕ обновляются из external velocity. Только не-external тики обновляют desired из SharedState. Иначе на следующем тике `update()` прочитает external velocity как desired и снова применит его — бесконечное движение.

### ROS2 Domain isolation
Физическая изоляция через отдельные `rclcpp::Context` с разными `domain_id` (не логическая через имена топиков). Каждый контекст — отдельный DDS participant. Агент в domain 1 физически не видит публикации в domain 0.

### ID конфликт во фронтенде
При нескольких агентах с одинаковыми плагинами ID формы включает agentId: `plugin-form-${agentId}-${pluginName}`.
