# S2 Simulator — Архитектурный документ

> **Назначение:** этот документ фиксирует концептуальную и техническую архитектуру S2 — кинематического мультиагентного симулятора для тестирования верхнеуровневой логики роботов. Документ описывает фундамент (то, что не должно меняться без серьёзного обоснования). Конкретные плагины и фичи строятся поверх этого фундамента.

---

## Оглавление

1. [Цели и принципы](#1-цели-и-принципы)
2. [Слоистая архитектура](#2-слоистая-архитектура)
3. [Ядро симуляции (SimEngine)](#3-ядро-симуляции-simengine)
4. [Entity Model — единая база сущностей](#4-entity-model--единая-база-сущностей)
5. [Capabilities и Tags](#5-capabilities-и-tags)
6. [Плагины: IAgentPlugin](#6-плагины-iagentplugin)
7. [Behavior акторов: IActorBehavior](#7-behavior-акторов-iactorbehavior)
8. [SharedState, Contributions и Resolver](#8-sharedstate-contributions-и-resolver)
9. [Зоны и эффекты](#9-зоны-и-эффекты)
10. [Сигналы и детекция](#10-сигналы-и-детекция)
11. [Материалы, передача и деформация](#11-материалы-передача-и-деформация)
12. [Транспортный слой](#12-транспортный-слой)
13. [Визуализация](#13-визуализация)
14. [Control API (REST)](#14-control-api-rest)
15. [Коммуникационные каналы и KernelCommands](#15-коммуникационные-каналы-и-kernelcommands)
16. [Жизненный цикл тика](#16-жизненный-цикл-тика)
17. [Hot Reload и runtime-модификация](#17-hot-reload-и-runtime-модификация)
18. [Время симуляции](#18-время-симуляции)
19. [Глоссарий](#19-глоссарий)
20. [Приложение: что НЕ входит в архитектуру](#20-приложение-что-не-входит-в-архитектуру)

---

## 1. Цели и принципы

### 1.1 Что такое S2

S2 — это **кинематический** мультиагентный симулятор. Он специально создан для тестирования **верхнеуровневой логики** робототехнических стеков (поведение FSM, навигация, мультиагентная координация, реакция на события). Это **не физический движок**: нет жёсткой динамики, нет SPH-жидкостей, нет реалистичной симуляции трения.

**Целевая нагрузка:** до 100 агентов нативно. Управление через любой транспортный плагин (ROS2, HTTP, скриптовое). Работает headless или с веб-визуализатором.

**Назначение:** open-source проект для community. Основной критерий — **лёгкость и модульность**: чтобы пользователь мог дописать своё поведение/плагин/эффект без лезения в ядро.

### 1.2 Базовые принципы

| # | Принцип | Что это значит на практике |
|---|---------|----------------------------|
| 1 | **Ядро не знает реализаций** | SimEngine не знает про ROS2, не знает про Three.js, не знает про DDS-домены |
| 2 | **Расширения через интерфейсы** | Транспорт, визуализация, плагины, поведения — всё через чётко определённые контракты |
| 3 | **WorldSnapshot — единственный язык наружу** | Всё что видит внешний мир — это снимок мира как структура данных |
| 4 | **Плагины общаются только через SharedState/EventBus** | Никакого прямого доступа плагина к плагину |
| 5 | **HTTP REST — единственный канал управления** | Никакого прямого доступа из UI к SimEngine |
| 6 | **Семантика, а не пиксели** | Behavior актора публикует состояние, визуализатор решает как рисовать |
| 7 | **Детерминизм через фиксированный dt** | Ускорение времени = больше тиков/сек, dt не меняется |
| 8 | **Opt-in для эффектов** | По умолчанию эффект НЕ действует на Entity. Нужно явно дать capability |

### 1.3 Что симулятор НЕ делает

- Не симулирует физику падения, отскока, столкновений с импульсами
- Не симулирует свойства сенсоров на низком уровне (шум фотонов, рефлекс ARуCO в искажённой плоскости)
- Не моделирует среды передачи данных (потери пакетов, задержки сети)
- Не претендует на точное воспроизведение реального робота — только на **поведенческое** соответствие

---

## 2. Слоистая архитектура

Система состоит из **трёх независимых слоёв взаимодействия** вокруг ядра:

```
                    ┌─────────────────────────────┐
                    │      S2 Core (SimEngine)    │
                    │   единственный источник     │
                    │           истины            │
                    └──────────────┬──────────────┘
                                   │
              ┌────────────────────┼────────────────────┐
              │                    │                    │
              ▼                    ▼                    ▼
       ┌──────────────┐    ┌──────────────┐    ┌──────────────┐
       │  Transport   │    │ Visualization│    │   Control    │
       │   (роботы)   │    │  (наблюдение)│    │  (управление │
       │              │    │              │    │     ядром)   │
       └──────────────┘    └──────────────┘    └──────────────┘
       ROS2 / HTTP /        Web / Null /         REST API
       Stub / ...            File / RViz2...     (единый)
```

Каждый слой — это **сменный модуль**, который выбирается при запуске и/или per-agent в конфиге. Ядро не знает, какие реализации работают.

---

## 3. Ядро симуляции (SimEngine)

### 3.1 Ответственность ядра

Ядро отвечает за:

- **Жизненный цикл симуляции** — pause/resume/reset/step, скорость
- **Управление мировым временем** — `sim_time`, `dt`, `speed_factor`
- **Цикл тика** — упорядоченный вызов фаз (см. [§16](#16-жизненный-цикл-тика))
- **Регистр сущностей** — реестр Entity, их жизненный цикл (spawn/despawn)
- **Обработку KernelCommands** — единая точка изменения мира
- **Построение WorldSnapshot** — сериализация состояния мира
- **EventBus** — диспетчеризация событий между сущностями
- **WorldQuery** — read-only API для плагинов и поведений

### 3.2 Чего ядро НЕ делает

- Ничего не знает про конкретные плагины (DiffDrive, Lidar, Battery)
- Ничего не знает про конкретные эффекты (ice, fog, wind)
- Ничего не знает про транспорты (ROS2, HTTP)
- Ничего не знает про визуализаторы

Всё это подключается через интерфейсы и регистрируется в реестрах (PluginRegistry, EffectRegistry, TransportRegistry, BehaviorRegistry).

### 3.3 Реестры

```
PluginRegistry      — регистрация типов IAgentPlugin
BehaviorRegistry    — регистрация типов IActorBehavior
EffectRegistry      — регистрация типов EffectPlugin
TransportRegistry   — регистрация типов ITransportAdapter
VizRegistry         — регистрация типов IVizAdapter
```

Регистрация происходит при старте через статические инициализаторы или явный bootstrap. Конфиг ссылается на типы по строковому имени (`type: diff_drive`).

---

## 4. Entity Model — единая база сущностей

### 4.1 Ключевая идея

**Все объекты мира — это Entity.** Различия между Agent / Actor / Prop — это не разные классы, а **типичные комбинации опциональных слоёв** на базовой Entity.

```
Entity (база, есть у всех):
  ├── id          : EntityId
  ├── name        : string
  ├── type        : EntityType { AGENT, ACTOR, PROP }
  ├── world_pose  : Pose3D
  ├── collision   : CollisionShape (optional)
  ├── visual      : VisualDesc
  ├── signals     : vector<Signal>          ← обнаруживаемые сигналы
  ├── capabilities: set<string>             ← для matching эффектов
  ├── tags        : map<string, string>     ← свободные метаданные
  ├── enabled     : bool
  └── owned_zones : vector<ZoneId>          ← зоны принадлежащие этой Entity

Опциональные слои (подключаются по необходимости):
  + PluginHost    — может иметь IAgentPlugin
  + SharedState   — участвует в contribution/resolver
  + TransportLink — подключён к внешнему контроллеру (ROS2/HTTP/...)
  + Behavior      — имеет IActorBehavior (FSM, settling, scripted)
  + LinkTree      — кинематическое дерево (URDF) с per-link свойствами
```

### 4.2 Типичные комбинации

|                | PluginHost | SharedState | TransportLink | Behavior |
|----------------|:---:|:---:|:---:|:---:|
| **Agent (типичный)** | да | да | да | нет |
| **Actor (типичный)** | опционально | да | нет | да |
| **Prop (типичный)** | опционально | **нет** | нет | нет |

> ⚠ **Важно:** это типичные конфигурации, **не жёсткие классы**. Любая Entity может получить любой слой, если это даёт смысл. Тип (AGENT/ACTOR/PROP) — это **семантическая метка** для удобства, а не архитектурное ограничение.

### 4.3 Граница Prop ↔ Actor

Простое правило:

- **Нет update(dt)** → Prop
- **Есть update(dt)** → Actor

| Объект | Тип | Почему |
|--------|-----|--------|
| Ящик стоит на полу | Prop | Просто данные |
| Ящик, который можно подобрать | Prop | Поведение в GripperPlugin агента |
| Ящик с capability `fragile` | Prop | Зональный эффект делает решение бинарно |
| Ящик с таймером — взрывается через 30с | **Actor** | Своё поведение |
| Бочка с зоной poison вокруг | Prop с `owned_zones` | Зона привязана к пропу |
| Бочка, которая течёт и образует лужу | **Actor** | Активная генерация зоны |
| Скоропортящийся груз (freshness deg.) | **Actor** | Непрерывная деградация = поведение |

### 4.4 Пропы — без SharedState

**Решение:** пропы по умолчанию **не имеют SharedState**. Зональные эффекты на пропах работают **бинарно**:

```
capability fragile + zone vibration → мгновенно уничтожить
capability flammable + zone fire    → мгновенно поджечь
```

Если нужно накопление урона / постепенный эффект → это **актор**, не проп. Не размываем границу.

> Технически проп всё ещё *может* получить слой SharedState (Entity — конструктор), но это не дефолт и не рекомендуется. Если ловишь себя на мысли «нужен SharedState на пропе» — скорее всего это actor.

### 4.5 LinkTree (URDF + дополнения)

Если у Entity загружен URDF, она имеет кинематическое дерево с линками. **Каждый линк может иметь:**

- `collision` — отдельная коллизионная форма
- `capabilities: [string]` — свои capabilities (например, `armored`)
- `immune_to_effects: [string]` — иммунитет к тегам эффектов
- `tags: map` — метаданные (например, `tool_type: blade`)

Дополнение делается через `link_overrides` — **точечно, не переписывая весь URDF**:

```yaml
- type: agent
  name: demining_robot
  urdf: demining_robot.urdf          # ← вся кинематика из URDF
  collision: {type: sphere, r: 0.4}  # ← общий collision для зон
  link_overrides:                    # ← дополнения отдельных линков
    rotor_link:
      immune_to_effects: ["blast"]
    bucket_link:
      tags: {tool_type: bucket}
```

---

## 5. Capabilities и Tags

### 5.1 Двухуровневая система

| Уровень | Назначение | Природа |
|---------|------------|---------|
| **Capabilities** | Matching эффектов (включить/выключить) | Конечный стандартный список |
| **Tags** | Детализация и метаданные | Свободные строки |

### 5.2 Стандартные capabilities

Эти capability **знает ядро** и могут использоваться эффектами:

```
Движение и контакт:
  surface_contact   — касается поверхности (для ice, conveyor)
  airborne          — летает (для wind)
  wheeled           — на колёсах (для tire_puncture)
  2d_motion         — движется в плоскости
  3d_motion         — свободное 3D движение

Сенсоры:
  optical_sensor    — оптические сенсоры (для fog)
  gnss_sensor       — GNSS (для электромагнитных помех)
  acoustic_sensor   — акустические

Ресурсы:
  has_battery       — заряжается (для charging zone)

Манипуляции:
  manipulator       — может захватывать

Прочее:
  detectable        — обнаружима EntityDetector (дефолт для Agent)
  perishable        — может портиться
  cold_sensitive    — подвержен теплу/холоду
  fragile           — хрупкая (для бинарного "ломается")
  flammable         — горит
```

> Список расширяется по мере появления новых эффектов. Принцип: capability — это **семантический признак**, не имплементация.

### 5.3 Capabilities — opt-in для эффектов

**Дефолт: эффект НЕ действует на Entity без нужной capability.**

```yaml
# Бочка без capabilities — конвейер её НЕ двигает:
- type: prop
  name: barrel_1

# Бочка С capability — конвейер двигает:
- type: prop
  name: barrel_2
  capabilities: [surface_contact]
```

### 5.4 Иммунитет: effect_tags + immune_to_effects

Эффект объявляет **теги** (что он за эффект). Entity объявляет **иммунитеты** (к каким тегам нечувствительна).

```yaml
# Эффект объявляет теги:
zones:
  - id: ice_patch
    effects:
      - type: ice_modifier
        effect_tags: ["ice", "surface_friction", "environmental"]
        required_capabilities: [surface_contact]

# Entity объявляет иммунитет к тегам:
agents:
  - name: arctic_robot
    capabilities: [surface_contact, wheeled]
    immune_to_effects: ["ice"]    # ← шипованные шины
```

**Правило matching:**

```
1. Эффект.required_capabilities ⊆ Entity.capabilities ?  Нет → пропустить
2. Эффект.excluded_capabilities ∩ Entity.capabilities ≠ ∅ ?  Да → пропустить
3. Эффект.effect_tags ∩ Entity.immune_to_effects ≠ ∅ ?  Да → пропустить (поглощено)
4. Иначе → применить
```

> ⚠ Зона/эффект ничего не знает про конкретные Entity. Entity ничего не знает про конкретные зоны. Matching полностью через теги/capabilities.

### 5.5 Effect Absorption — общий концепт

**Принцип:** эффект всегда *срабатывает*. Иммунитет определяет *поглощение результата*, не предотвращение события.

Это важно для случаев типа мины:

```
Мина взрывается ВСЕГДА при контакте (zone.self_destruct: on_any_contact)
Что происходит с роботом — зависит от per-link immunity:

  Контакт через rotor_link (immune_to ["blast"])  → эффект поглощён, робот цел
  Контакт через base_link  (без immunity)         → эффект применён, робот destroyed

В обоих случаях мина уничтожена.
```

Тот же принцип работает для всех эффектов:

| Ситуация | Эффект | Поглощение |
|----------|--------|------------|
| Лёд + шипованные шины | `ice` | `immune_to: [ice]` → speed_scale=1.0 |
| Туман + ИК-лидар | `optical` | сенсор с immunity → range без изменений |
| ЭМ помехи + экран | `electromagnetic` | экранированный линк |

### 5.6 Per-link immunity и detection_mode зон

Зоны могут детектировать вход **по разным правилам**:

```yaml
detection_mode:
  center      → zone.contains(entity.world_pose)            # центр сущности
  bounding    → zone.intersects(entity.bounding_shape)      # bounding shape
  per_link    → ∀ link in entity.kinematic_tree:            # каждый линк
                  zone.intersects(link.collision_shape)
```

При `per_link` ядро знает **через какой линк произошёл контакт** и проверяет immunity на уровне линка:

```yaml
- name: demining_robot
  link_overrides:
    rotor_link:
      immune_to_effects: ["blast"]   # ← только этот линк броня

zones:
  - id: mine_1
    detection_mode: per_link
    self_destruct: on_any_contact    # см. §9
    effects:
      - type: explosion
        effect_tags: ["blast"]
        action: state_change         # → DestroyedStatus
```

### 5.7 Tags — свободная детализация

Tags **не использует ядро** напрямую. Их читают плагины, эффекты, скрипты:

```yaml
agents:
  - name: robot_0
    capabilities: [surface_contact, wheeled, has_battery, 2d_motion]
    tags:
      drive_type: ackermann       # для логики поворота
      wheel_count: 4              # для деталей прокола
      payload_capacity: 50kg      # для логики загрузки
      team: red                   # для мультиагентных сценариев
```

**Пример использования:** TirePunctureEffect берёт `tags.wheel_count` чтобы решить, как именно симулировать прокол:

```python
TirePunctureEffect.apply():
    wheel_count = ctx.entity.tags.get("wheel_count", "2")
    if wheel_count == "4":
        # Прокол одного из четырёх — лёгкий drift
        contribution(angular_drift, 0.02)
        contribution(speed_scale, 0.7)
    else:
        # 2 колеса — грубый эффект
        contribution(angular_drift, 0.05)
        contribution(speed_scale, 0.5)
```

DiffDrive сам ничего не знает про прокол — он читает только `eff.speed_scale` и `eff.velocity_addition`.

### 5.8 Автоматическое объявление capabilities из плагинов

Плагин может объявить, какие capabilities он добавляет своему Entity:

```cpp
class DiffDrivePlugin : public IAgentPlugin {
    std::vector<std::string> provided_capabilities() const override {
        return {"surface_contact", "wheeled", "2d_motion"};
    }
};
```

При инициализации плагина ядро автоматически добавляет эти capabilities в `entity.capabilities`. Не нужно дублировать в YAML.

---

## 6. Плагины: IAgentPlugin

### 6.1 Один интерфейс — пять ролей

**Решение:** в системе **один** базовый интерфейс плагина — `IAgentPlugin`. Роль декларируется методом `role()`. Это даёт простоту (одна точка наследования) и не лишает контракта (роль документирована).

```cpp
class IAgentPlugin {
public:
    // ─── Идентификация ───
    virtual std::string type() const = 0;
    virtual PluginRole role() const = 0;
    virtual std::string display_label() const { return type(); }

    // ─── Жизненный цикл ───
    virtual void from_config(const YAML::Node&) = 0;
    virtual void initialize(Entity&) {}
    virtual void on_spawn(Entity&) {}        // hot-reload: добавление в мир
    virtual void on_despawn(Entity&) {}      // hot-reload: удаление из мира
    virtual void on_scene_load(World&) {}    // загрузка сцены
    virtual void on_reset() {}               // reset симуляции — сбросить state!

    // ─── Тик ───
    virtual void pre_resolve(double dt, Entity&) {}  // resource: contributions
    virtual void update(double dt, Entity&) {}       // основная логика

    // ─── Снапшот ───
    virtual void contribute_snapshot(json&, Entity&) {}

    // ─── Расширения возможностей ───
    virtual std::vector<std::string> provided_capabilities() const { return {}; }
    virtual std::vector<std::string> command_topics() const { return {}; }
    virtual std::vector<std::string> service_names() const { return {}; }
    virtual std::vector<std::string> subscribe_topics() const { return {}; }
    virtual std::vector<TransportEvent> poll_events() { return {}; }
    virtual void handle_input(const json&) {}

    // ─── Материалы (опциональные) ───
    virtual bool can_release_material() const { return false; }
    virtual bool can_accept_material() const { return false; }
    virtual bool can_actively_acquire() const { return false; }
    virtual bool can_actively_release() const { return false; }
    virtual double release_material(double v) { return 0; }
    virtual double accept_material(double v, const std::string& m, const Vec3& p) { return 0; }
    virtual double material_volume() const { return 0; }
    virtual double remaining_capacity() const { return 0; }
    virtual std::string material_type() const { return ""; }

    // ─── Конфиг для UI редактора ───
    virtual ConfigSchema config_schema() const { return {}; }
};
```

> ⚠ **Важно:** `IMaterialSource`/`IMaterialReceiver`/`IDeformable` — это **НЕ отдельные интерфейсы**. Это опциональные методы внутри `IAgentPlugin` (или `IActorBehavior` для акторов). Один плагин = один тип = одно наследование.

### 6.2 Пять ролей

| Роль | Что делает | Канал записи | Канал чтения |
|------|------------|--------------|--------------|
| **actuation** | Команда → скорость агента | `world_velocity` | effective constraints |
| **sensor** | Мир → данные на транспорт | transport publish | WorldQuery |
| **interaction** | Команды → действия в мире | KernelCommand / EventBus | WorldQuery |
| **resource** | Внутренние ограничения | SharedState contributions | — |
| **utility** | Визуализация, отладка, скрипты | snapshot contributions | (любой read-only) |

**Жёсткое правило:** **у агента максимум один actuation-плагин.** Это его тип движения. Остальные роли — несколько штук без ограничения.

### 6.3 Матрица доступа

|             | SharedState | EventBus | WorldQuery | KernelCommand |
|-------------|:-----------:|:--------:|:----------:|:-------------:|
| Actuation   | READ        | —        | —          | —             |
| Sensor      | —           | —        | READ       | —             |
| Interaction | —           | PUBLISH  | READ       | PUBLISH       |
| Resource    | WRITE       | —        | —          | —             |
| Utility     | (любое read-only) |     |            |               |

> ⚠ **Исключение:** Gravity нарушает матрицу — он Resource, но напрямую двигает Z агента. Это осознанное исключение, потому что гравитация по природе меняет позицию, а не скорость.

### 6.4 Декларации транспорта

Эти методы **абстрактны от транспорта** — плагин говорит «я хочу X», адаптер переводит в свой протокол:

| Декларация | ROS2 | HTTP | MQTT |
|------------|------|------|------|
| `command_topics: ["/cmd_vel"]` | `Subscription<Twist>` | `POST /agents/{id}/cmd_vel` | `subscribe s2/agents/{id}/cmd_vel` |
| `service_names: ["/grab"]` | `Service<PluginCall>` | `POST /agents/{id}/services/grab` | `req-reply s2/agents/{id}/grab` |
| `poll_events()` | `publish` | `SSE event` | `publish s2/events/...` |
| `subscribe_topics: ["/plan"]` | `Subscription` | `GET /agents/{id}/topics/plan` | `subscribe ...` |

Плагин пишется **один раз**, работает с любым транспортом.

### 6.5 Жизненный цикл плагина

```
1. from_config(yaml)        — при загрузке сцены
2. initialize(entity)       — после загрузки всех плагинов
3. on_spawn(entity)         — при добавлении entity в мир (hot reload)
4. Каждый тик:
     a. pre_resolve(dt)     — resource роль: contributions
     b. <Resolver>          — ядро собирает effective
     c. update(dt)          — основная логика
     d. poll_events()       — для transport publish
5. contribute_snapshot()    — при построении снапшота
6. on_reset()               — при reset симуляции (СБРОС внутреннего state!)
7. on_despawn(entity)       — при удалении entity
8. on_scene_load(world)     — при загрузке новой сцены
```

### 6.6 Утилитарные плагины (utility)

Не попадают в actuation/sensor/interaction/resource:

| Плагин | Что делает |
|--------|------------|
| TrajectoryRecorder | Записывает путь для отображения |
| PathDisplay | Показывает запланированный маршрут |
| TopicDisplay | Показывает данные внешнего топика на сцене |
| Color | Меняет цвет агента |
| ScriptedBehavior | Внутренняя логика для скриптовых агентов (см. ниже) |

### 6.7 Scripted agent

Агент, который управляется не извне, а внутренней логикой. Использует `transport: stub` + utility-плагин со скриптом:

```yaml
- type: agent
  name: scripted_robot_0
  transport: stub
  plugins:
    - type: diff_drive
    - type: gnss
    - type: lidar
    - type: scripted_behavior   # utility plugin
      config:
        mode: patrol
        waypoints: [[0,0], [5,0], [5,5], [0,5]]
        speed: 0.5
        pause_at_waypoint: 2.0
```

ScriptedBehavior **не двигает агента напрямую** — он пишет `desired_linear/angular` в SharedState, а DiffDrive читает (как если бы команда пришла извне). DiffDrive не отличает скриптовую команду от ROS2.

**Разница scripted agent vs actor с pedestrian behavior:**

|                       | Scripted Agent | Pedestrian Actor |
|-----------------------|:---:|:---:|
| Имеет плагины         | да (полный набор) | опционально |
| Публикует сенсоры     | да | нет |
| Виден через транспорт | да (другой робот видит в ROS2) | нет |
| Подвержен эффектам    | да (через capabilities) | только если явно включено |
| Управляется извне     | нет (stub) | нет |

---

## 7. Behavior акторов: IActorBehavior

### 7.1 Один behavior + опциональные плагины

Актор имеет **одно** основное поведение (`IActorBehavior`) и **опционально** плагины через PluginHost (для датчиков, ROS2-связи и т.п.).

```cpp
class IActorBehavior {
public:
    // ─── Идентификация ───
    virtual std::string type() const = 0;

    // ─── Жизненный цикл ───
    virtual void on_init(const YAML::Node&) = 0;
    virtual void on_spawn(Entity&) {}
    virtual void on_reset() {}

    // ─── Тик ───
    virtual void update(double dt, Entity&, const WorldContext&) = 0;

    // ─── Сигналы и взаимодействия ───
    virtual void on_signal(const SignalEvent&) {}
    virtual void on_interact(EntityId source, const std::string& action,
                             const json& params) {}

    // ─── Состояние ───
    virtual std::string current_state() const { return ""; }
    virtual json to_json() const { return {}; }

    // ─── Материалы (опциональные) ───
    virtual bool can_release_material() const { return false; }
    virtual bool can_accept_material() const { return false; }
    virtual double release_material(double v) { return 0; }
    virtual double accept_material(double v, const std::string& m, const Vec3& p) { return 0; }

    // ─── Деформация (опциональная) ───
    virtual bool is_deformable() const { return false; }
    virtual void apply_deformation(const DeformationCommand&) {}
};
```

### 7.2 Порядок тика для актора с плагинами

```
plugins.pre_resolve(dt)    →  Resolver  →  behavior.update(dt)  →  plugins.update(dt)
```

Плагины готовят данные в SharedState, behavior читает effective и действует, плагины публикуют сенсорику.

### 7.3 FSM — утилита, не требование

`IActorBehavior` **не привязан к FSM**. Это утилитарный класс:

```cpp
class ActorFSM {
    void add_state(name, on_enter, on_update, on_exit);
    void add_transition(from, to, trigger, guard?);
    void fire(trigger);
    std::string current_state();
    void update(double dt);
};
```

Поведение **само решает**, использовать FSM или нет:

- **DoorBehavior** — классический FSM (closed → opening → open → closing → closed)
- **DirtPileBehavior** — нет FSM, только непрерывный settling
- **ConveyorBehavior** — два состояния (on/off), но логика — в зонном эффекте
- **TrafficLightBehavior** — таймерный FSM

### 7.4 Триггеры FSM

| Триггер | Источник | Пример |
|---------|----------|--------|
| `wire signal` | Spatial signal с infinite range | Кнопка → дверь открывается |
| `timer` | Внутренний таймер | Светофор переключается |
| `proximity` | on_interact от агента | Автоматическая дверь |
| `api command` | Control API | Оператор открыл дверь |
| `effect` | Зонный эффект через SharedState | Заморозка конвейера в ледяной зоне |

### 7.5 Визуальное состояние — ИМПЕРАТИВНОЕ

> ⚠ **Финальное решение** (после обсуждения двух вариантов):
>
> Behavior **напрямую управляет геометрией и collision** актора. Не публикует абстрактное `state: opening`, а сам двигает части и обновляет collision shape.

**Причина:** behavior всё равно влияет на collision (открытая дверь = свободный проход). Делать двойную интерпретацию (behavior → семантика → движок переводит в collision) — лишний слой и источник рассинхрона.

```cpp
DoorBehavior::update(dt, actor, ctx) {
    progress_ += direction_ * speed_ * dt;
    progress_ = std::clamp(progress_, 0.0, 1.0);

    // Императивно меняем геометрию:
    actor.parts["door_panel"].pose.yaw = open_angle_max_ * progress_;

    // Императивно обновляем collision:
    actor.collision = compute_door_collision(progress_);

    // Декоративные эффекты — через visual_hint:
    actor.visual_hint = {
        {"door_progress", progress_},
        {"sound", progress_ > 0 && progress_ < 1 ? "moving" : ""}
    };
}
```

**Разделение:**

- **Геометрия и collision** — императивно из behavior
- **Декоративные эффекты** (анимация ленты конвейера, мигание лампочки, частицы) — через `visual_hint` (см. [§13.5](#135-visual-hints))

---

## 8. SharedState, Contributions и Resolver

### 8.1 Концепция

**SharedState — единственный канал общения между плагинами.** Никто не лезет в чужой код. Плагин публикует contribution → Resolver собирает → Actuation читает effective.

### 8.2 Resolved fields (стандартные)

| Поле | Тип | Merge rule | Пример |
|------|-----|------------|--------|
| `speed_scale` | double | multiplicative | лёд × boost × battery = 0.85 × 1.2 × 0.5 |
| `motion_locked` | bool | OR | e-stop OR battery_critical |
| `velocity_addition` | Vec3 (linear+angular) | sum | конвейер + ветер + tire_drift |
| `manipulation_locked` | bool | OR | "не хватает заряда для захвата" |
| `max_speed_cap` | double | **MIN** | "в этой зоне максимум 0.5 м/с" |
| `all_plugins_disabled` | bool | OR | DestroyedStatus |
| `angular_drift` | double | sum | прокол колеса + ветер |
| `damage_rate` | double | sum | (опционально, для health) |

> `max_speed_cap` — **абсолютное ограничение** скорости в м/с. Применяется как `effective_speed = min(computed, max_speed_cap)`. Не зависит от max_speed конкретного робота.

### 8.3 Single-owner state (через `std::any`)

Помимо contribution-based fields, плагин может иметь **эксклюзивные** компоненты в SharedState:

```cpp
state.emplace<BatteryComponent>({level: 0.85, ...});  // single owner — Battery plugin
state.emplace<TirePunctureData>({punctured: true});   // single owner — TirePunctureEffect
state.emplace<DiffDriveData>({desired_linear, desired_angular});  // single owner — DiffDrive
state.emplace<DestroyedStatus>({reason, time});       // single owner — DestroyMutation
```

Каждый компонент имеет **одного владельца** (того, кто его создал). Другие плагины могут только **читать**.

### 8.4 Пример: Battery блокирует DiffDrive без знания о нём

```
Battery (Resource)                     DiffDrive (Actuation)
     │                                       │
     │ pre_resolve():                        │ update():
     │   level = 0.02 (критический)          │   eff = state.effective()
     │   contribution(motion_lock, true,     │   if (eff.motion_locked)
     │     source="battery_critical")        │     velocity = 0
     │                                       │   else
     ▼                                       │     velocity *= eff.speed_scale
     SharedState ──→ RESOLVER ──→ Effective ─┘
```

**Ни один плагин не лезет в код другого.** Battery даже не знает что DiffDrive существует.

### 8.5 Own effects (динамические)

MUTATION-эффекты могут создавать **постоянные own effects** на Entity. Это эффекты, которые живут на конкретной Entity и публикуют contributions каждый тик:

```
Робот наезжает на nail_strip
  → TirePunctureEffect (ON_ENTER, STATE_CHANGE)
  → state.emplace<TirePunctureData>({punctured: true, wheel: "fl"})
  → entity.add_own_effect(TireDriftContributor)

Каждый последующий тик:
  → TireDriftContributor.contribute():
      speed_scale 0.6
      angular_drift 0.05    ← постоянный поворот
  → DiffDrive видит eff.speed_scale=0.6 и eff.angular_drift=0.05
  → Едет медленнее и тянет вбок
```

DiffDrive **не знает про прокол**. Он видит только итоговые constraints.

### 8.6 Очистка contributions

В конце каждого тика (фаза 8) ядро вызывает `state.clear_contributions()` для каждой Entity. Single-owner state и own effects **не очищаются** — они живут пока их явно не уберут.

---

## 9. Зоны и эффекты

### 9.1 Зона — не Entity

**Зона — это правило пространства, а не объект мира.** Лёд — не предмет, а свойство области. Зону нельзя:
- Видеть лидаром
- Толкнуть
- Захватить
- Иметь у неё коллизию

Зоны хранятся **отдельно** в `ZoneSystem`, не в реестре Entity. Но зона может быть **привязана к Entity** (`attached_to: entity_id`) — двигается вместе с ней.

### 9.2 Структура зоны

```yaml
- id: ice_patch_1
  shape:
    type: sphere | box | infinite | ...
    ...
  attached_to: entity_id        # опционально — привязка к Entity
  enabled: true                  # вкл/выкл
  strength: 1.0                  # 0..1, для плавного fade
  detection_mode: center | bounding | per_link
  lifecycle: { ... }             # см. §9.6
  effects: [ ... ]
```

### 9.3 Эффекты: Trigger + Action

> **Решение** (заменяет старую таксономию MODIFIER/CONTINUOUS/MUTATION/SENSOR):
>
> Эффект описывается **двумя независимыми измерениями**: когда срабатывает (trigger) и что делает (action).

**Triggers:**

| Trigger | Когда вызывается |
|---------|------------------|
| `WHILE_INSIDE` | Каждый тик, пока Entity внутри зоны |
| `ON_ENTER` | Один раз при входе |
| `ON_EXIT` | Один раз при выходе |

**Actions** (одно или несколько в одном эффекте):

| Action | Что делает |
|--------|------------|
| `CONTRIBUTION` | Публикует contribution в SharedState (speed_scale, velocity_addition, ...) |
| `STATE_CHANGE` | Меняет single-owner поле в SharedState (TirePunctureData, ColdStatus) |
| `SENSOR_MOD` | Модифицирует параметры сенсоров (max_range, noise_std) |
| `OWN_EFFECT_SPAWN` | Создаёт own_effect на Entity (для permanent эффектов) |

**Старые типы → новые комбинации:**

| Старый тип | = Trigger + Action |
|------------|---------------------|
| MODIFIER | `WHILE_INSIDE + CONTRIBUTION` |
| CONTINUOUS | `WHILE_INSIDE + STATE_CHANGE` |
| MUTATION | `ON_ENTER + STATE_CHANGE (+ OWN_EFFECT_SPAWN)` |
| SENSOR | `WHILE_INSIDE + SENSOR_MOD` |

**Новые комбинации (раньше невозможные):**

| Комбинация | Пример |
|------------|--------|
| `ON_ENTER + CONTRIBUTION` | Ударная волна — одноразовый импульс velocity_addition |
| `ON_EXIT + STATE_CHANGE` | Дезинфекция — снимается status "contaminated" |
| `ON_ENTER + SENSOR_MOD` | Калибровка — сенсор сбрасывает drift |
| `ON_EXIT + CONTRIBUTION` | Тормозной порог — кратковременное замедление |

### 9.4 Единый интерфейс эффекта

```cpp
class EffectPlugin {
public:
    virtual std::string type() const = 0;
    virtual EffectTrigger trigger() const = 0;
    virtual void apply(SharedState&, const EffectContext&) = 0;

    // Capabilities matching
    virtual std::vector<std::string> required_capabilities() const { return {}; }
    virtual std::vector<std::string> excluded_capabilities() const { return {}; }
    virtual std::vector<std::string> effect_tags() const { return {}; }
};
```

`EffectContext` содержит:
- `entity` — Entity на которой применяется
- `zone` — зона-источник
- `dt` — шаг
- `zone_strength` — текущая сила зоны (0..1, для fade)
- `contact_link` — какой линк контактирует (для per_link mode)

### 9.5 Пример эффекта

```cpp
class IceModifier : public EffectPlugin {
    EffectTrigger trigger() const override { return WHILE_INSIDE; }

    std::vector<std::string> required_capabilities() const override {
        return {"surface_contact"};
    }
    std::vector<std::string> effect_tags() const override {
        return {"ice", "surface_friction", "environmental"};
    }

    void apply(SharedState& state, const EffectContext& ctx) override {
        double scale = 0.2 + 0.8 * (1.0 - ctx.zone_strength);  // strong ice = slow
        state.contribute_speed_scale(scale, "ice_zone:" + ctx.zone.id);
    }
};
```

### 9.6 Lifecycle зоны

Зоны имеют опциональный жизненный цикл — для динамических эффектов (пыль, дым, ударные волны):

```yaml
zone_templates:
  dust_cloud:
    shape: sphere
    detection_mode: center
    lifecycle:
      spawn_trigger: { type: command }     # см. ниже
      initial_radius: 0.5
      growth:
        rate: 2.0           # м/с расширение
        max_radius: 8.0
      decay:
        delay: 3.0          # секунд до начала затухания
        rate: 0.1           # сила/сек
        remove_at: 0.05     # удалить когда strength < 0.05
    effects:
      - type: sensor_mod
        trigger: WHILE_INSIDE
        param: max_range
        multiplier_expr: "1.0 - zone.strength * 0.8"
        required_capabilities: [optical_sensor]
```

**Spawn triggers:**

| Тип | Описание |
|-----|----------|
| `command` | Только программно через KernelCommand SpawnZone |
| `event` | По событию EventBus (с фильтром по типу/источнику) |
| `timer` | Через N секунд от старта симуляции |
| `state_change` | Когда указанная Entity переходит в указанное состояние |

**zone.strength** — параметр силы зоны (0..1), который эффекты могут использовать в формулах для плавного усиления/затухания.

### 9.7 Self-destruct policy

```yaml
self_destruct_policy:
  on_any_contact: true        # удалить зону при любом контакте (мина/обезврежена)
  # ИЛИ
  on_effect_applied: true     # удалить только если эффект сработал (поглощённое = живо)
```

Для разминирования: `on_any_contact` — мина исчезает даже если ротор поглотил взрыв (она «обезврежена»).

### 9.8 Toggle и enabled

Зоны можно вкл/выкл в рантайме через `KernelCommand::ToggleZone(zone_id, enabled)`.

При `enabled: false`:
- Все Entity внутри получают `on_exit` от эффектов
- Зона остаётся в мире (видна полупрозрачной в визуализаторе)

При повторном `enabled: true`:
- Все Entity внутри получают `on_enter` снова

Источники команды: плагины агентов, FSM акторов, REST API, UI визуализатора.

### 9.9 Movement зоны

**Решение (после обсуждения двух подходов):** для свободно движущихся зон используется **Подход A — невидимый проп-носитель**.

```yaml
# При спавне dust_cloud:
KernelCommand: SpawnProp(invisible_dust_carrier, position)
KernelCommand: SpawnZone(dust_cloud, attached_to: invisible_dust_carrier)
```

Проп может иметь плагин `drift_behavior` для движения по ветру. Это переиспользует существующие механизмы вместо введения нового «движения зоны».

### 9.10 Owned zones — зоны принадлежащие Entity

Любая Entity может иметь зоны, привязанные к своим линкам:

```yaml
- type: agent
  name: delivery_robot
  plugins:
    - type: diff_drive
    - type: grabber
      config: { grab_link: cargo_link }
  owned_zones:                      # ← зоны на этом агенте
    - id: freezer_compartment
      shape: { type: sphere, radius: 0.3 }
      attached_to_link: cargo_link  # привязана к линку
      effects:
        - type: cold
          trigger: WHILE_INSIDE
          action: state_change      # → ColdStatus
          effect_tags: ["cold", "temperature"]
```

Когда entity владеющая зоной двигается — зона двигается с ней. Когда меняет состояние (`enabled`) — может включать/выключать свои зоны.

---

## 10. Сигналы и детекция

### 10.1 Сигналы — свойство Entity

**Сигнал — это не отдельный объект, а свойство Entity.** Любая Entity (Agent, Actor, Prop) может нести сигналы.

```cpp
struct Signal {
    std::string signal_type;   // "aruco", "radio", "ir", "rfid", "qr", "wire"
    std::string signal_id;     // "marker_42", "beacon_7", "factory_power"
    Pose3D local_pose;         // относительно носителя
    json params;               // тип-специфичные параметры
    double range;              // дальность распространения (м), inf = бесконечно
    bool requires_los;         // нужна ли прямая видимость
    bool enabled;              // сигнал активен?
};
```

### 10.2 Wire — частный случай сигнала

«Проводные» связи (кнопка → конвейер, реле, сетевые шины) — это **сигналы** с особыми параметрами:

```yaml
signals:
  - type: wire
    signal_id: "factory_power"
    range: infinite      # ← бесконечная дальность
    requires_los: false  # ← через стены
    enabled: false       # включается FSM
```

Не плодим отдельный механизм для проводов — это просто сигнал без пространственных ограничений.

### 10.3 Detector — сенсорный плагин

Детектор — это IAgentPlugin роли `sensor`, который **сканирует мир** на сигналы или сущности:

```cpp
class ArucoDetectorPlugin : public IAgentPlugin {
    // Параметры:
    DetectionVolume volume_;    // CONE, SPHERE, BOX
    bool requires_los_;
    std::string scans_signal_type_ = "aruco";

    void update(double dt, Entity& self) override {
        auto candidates = world_query.find_signals_of_type(
            scans_signal_type_, self.world_pose, volume_);

        for (auto& sig : candidates) {
            if (requires_los_ && !world_query.has_line_of_sight(self, sig))
                continue;
            publish_detection(sig);   // через transport
        }
    }
};
```

### 10.4 Два типа детекции

| Тип | Что ищет | Пример |
|-----|----------|--------|
| **Signal detection** | Сигналы определённого типа на любых Entity | ArucoDetector ищет signal_type="aruco" |
| **Entity detection** | Сущности определённого типа | AgentDetector ищет entity_type=AGENT |

Оба работают через единый паттерн: **Detection Volume + LoS + relative pose**.

### 10.5 Фильтры детектора — в конфиге, не в коде

Не создаём `PedestrianDetector`, `ConveyorDetector` и т.п. **Один EntityDetector** с конфигурируемым фильтром:

```yaml
# Видит только пешеходов:
- type: entity_detector
  filter:
    entity_types: [actor]
    required_tags: { behavior: pedestrian }

# Видит только конвейеры:
- type: entity_detector
  filter:
    entity_types: [actor]
    required_tags: { behavior: conveyor }

# Видит всё кроме статики:
- type: entity_detector
  filter:
    entity_types: [agent, actor, prop]
```

### 10.6 Реакция на сигнал — конкретные controller-плагины

Когда нужна **доменная логика реакции** (дверь открывается особым образом, конвейер тормозит постепенно), используются **конкретные controller-плагины** с общей базой:

```
SignalListenerBase (общая база)
  ├── scan_signals(), filter_by_id(), filter_by_source()
  └── react()  ← virtual

DoorWireController : SignalListenerBase
  └── react(): знает door-specific actions (close_and_lock, force_open)

ConveyorWireController : SignalListenerBase
  └── react(): знает conveyor-specific actions (stop, reverse_direction)

CustomScenarioController : SignalListenerBase
  └── react(): полностью кастомная логика
```

```yaml
# Дверь: слушает два разных провода
- type: actor
  name: door_1
  behavior: door_fsm
  plugins:
    - type: door_wire_controller
      config:
        reactions:
          - signal_id: "factory_power"
            source_entity: button_1
            on_active: close_and_lock
            on_inactive: unlock
          - signal_id: "emergency_open"
            source_entity: any
            on_active: force_open

# Конвейер слушает тот же провод, но реагирует по-своему:
- type: actor
  name: conveyor_1
  behavior: conveyor_fsm
  plugins:
    - type: conveyor_wire_controller
      config:
        reactions:
          - signal_id: "factory_power"
            source_entity: button_1
            on_active: stop
            on_inactive: start
```

Оба слушают один сигнал, но реагируют по-разному, потому что **это разные плагины с разной доменной логикой**.

### 10.7 EventReactor (для простых случаев)

Для тривиальной реакции по сигналу/событию — обобщённый плагин с декларативным конфигом:

```yaml
- type: event_reactor
  config:
    listen: { signal_id: "alarm" }
    on_active: { fire_event: "alarm_active" }
    on_inactive: { fire_event: "alarm_clear" }
```

Сосуществует с конкретными controller'ами. Простые случаи — через EventReactor, сложные — через конкретный плагин.

---

## 11. Материалы, передача и деформация

### 11.1 Концепция

**Ядро не знает про конкретные материалы (грунт, вода, снег, щебень).** Ядро знает только: «передать `volume` единиц `material` от A к B» и «деформировать B инструментом A».

Все детали (как именно сыпется грунт, как тает снег) — в behavior актора-материала или в плагине навесного оборудования.

### 11.2 Опциональные методы IAgentPlugin / IActorBehavior

> ⚠ `IMaterialSource`, `IMaterialReceiver`, `IDeformable` — **НЕ отдельные интерфейсы**. Это опциональные методы в `IAgentPlugin` (для плагинов агентов) и `IActorBehavior` (для акторов).

Что сущность умеет:

| Метод | Возвращает | Смысл |
|-------|-----------|-------|
| `can_release_material()` | bool | может отдать |
| `can_accept_material()` | bool | может принять |
| `can_actively_acquire()` | bool | может сам инициировать забор (ковш — да, куча — нет) |
| `can_actively_release()` | bool | может сам инициировать выдачу (ковш — да, куча — нет) |
| `release_material(v)` | double | забрать объём, вернуть реально забранный |
| `accept_material(v, m, p)` | double | принять объём, вернуть реально принятый |
| `material_volume()` | double | сколько сейчас |
| `remaining_capacity()` | double | сколько ещё вместит |
| `material_type()` | string | "sand", "water", "snow" |
| `is_deformable()` | bool | можно ли деформировать |
| `apply_deformation(cmd)` | void | деформироваться инструментом |

### 11.3 Кто что умеет

|             | release | accept | active acquire | active release | deformable |
|-------------|:---:|:---:|:---:|:---:|:---:|
| **BucketAttachment** (ковш) | да | да | да | да | — |
| **TruckCargo** (кузов) | да | да | нет | да | — |
| **DirtPile** (куча, behavior) | да | да | нет | нет | да |
| **TankAttachment** (цистерна) | да | да | нет | да | — |
| **BladeAttachment** (отвал) | нет | нет | — | — | — |
| **RotorAttachment** (снежный ротор) | нет | нет | — | — | — |

**Blade и Rotor** не хранят материал — они только **двигают/перекидывают** его через `MaterialTransfer` или `DeformEntity`.

### 11.4 KernelCommand: MaterialTransfer

**Единственная команда передачи материалов.** Не `ScoopDirt`, не `DumpWater` — единая `MaterialTransfer`:

```cpp
struct MaterialTransfer {
    EntityId source_entity;
    std::string source_plugin;     // "bucket", "truck_cargo", "dirt_grid"
    EntityId target_entity;         // может быть NULL (на землю)
    std::string target_plugin;
    double volume;
    std::string material;
    Vec3 position;                  // куда (для NULL target — где спавнить)
    json transfer_hint;             // для визуализации, см. §13.5
};
```

**Что делает ядро:**

1. Проверяет существование source / target
2. `actual = source_plugin.release_material(volume)`
3. Если target существует:
   - `accepted = target_plugin.accept_material(actual, material, position)`
4. Если target = NULL:
   - **Displacement check** (см. ниже)
   - Если в позиции есть DirtPile с тем же material → `accept_material`
   - Иначе → `KernelCommand::SpawnActor(dirt_pile, position, volume)`

### 11.5 KernelCommand: DeformEntity

Для инструментов, которые не переносят материал, а **меняют форму**:

```cpp
struct DeformEntity {
    EntityId target_entity;
    Pose3D tool_pose;
    ToolGeometry tool_geometry {
        std::string type;        // "blade", "rake", "drill"
        double width, height, depth;
        bool side_walls;
        double side_wall_height;
        // ... тип-специфичные поля
    };
    Vec3 tool_velocity;
    double dt;
};
```

Ядро вызывает `target.behavior.apply_deformation(cmd)`. Behavior сам решает как реагировать на инструмент данной формы.

> Завтра добавится `RakeAttachment` (грабли) — он шлёт ту же `DeformEntity` с другой `tool_geometry`. DirtPile сам знает, что грабли = мелкое рыхление, отвал = крупное перемещение.

### 11.6 Displacement — материал на занятое пространство

Когда материал должен оказаться в позиции, где стоит Entity без `accept_material`:

```
Снежный ротор выбрасывает снег в точку P
P содержит agent_3 (без accept_material)

→ Ядро: найти ближайшие позиции вокруг agent_3, свободные от его коллизии
→ Распределить материал по этим позициям
→ Если рядом уже есть куча → добавить к ней
→ Если нет → создать новые мелкие кучки вокруг agent_3
```

Снег **обтекает** робота, не «проваливается внутрь».

### 11.7 DirtPile — деформируемый актор

```yaml
- type: actor
  name: dirt_pile_1
  behavior: dirt_grid
  config:
    cell_size: 0.1                # размер вокселя (м)
    grid_size: [20, 20]           # 2×2м
    material: sand
    friction_angle: 30            # угол откоса (градусы)
    settle_rate: 5.0              # как быстро стекает
    min_merge_volume: 0.001
    merge_radius: 0.3
    smoothing: true               # сглаживание коллизии
  initial_shape:
    type: cone
    center: [10, 10]
    radius: 8
    peak_height: 1.5
```

**DirtGridBehavior каждый тик делает:**

1. **Settling** — материал «стекает» в соседние ячейки если уклон > friction_angle
2. **Merge** — мелкие кучки сливаются с большими в радиусе merge_radius
3. **Rebuild collision** — пересчёт коллизионной поверхности (сглаженной билинейной интерполяцией)
4. **Volume conservation** — суммарный объём строго сохраняется

> Это **не физика частиц**, а 2D-клеточный автомат на сетке высот. Дёшево, детерминировано, визуально убедительно.

### 11.8 Навесное оборудование — плагины агента

Bucket, Blade, Rotor, Tank, Drill — это **плагины роли interaction**, привязанные к линку URDF:

```yaml
- type: agent
  name: excavator
  urdf: excavator.urdf
  plugins:
    - type: diff_drive
    - type: bucket_attachment
      config:
        mount_link: bucket_link
        bucket_volume: 0.3
        bucket_width: 0.6
        scoop_depth: 0.2
        accepted_materials: [sand, gravel, snow, dirt]
        interaction_targets: [dirt_pile]
```

**Hot-swap в рантайме:**

```http
PUT /agents/excavator/plugins/bucket_attachment
Content-Type: application/json
{
  "type": "rotor_attachment",       # замена ковша на снежный ротор
  "config": { ... }
}
```

Старый плагин удалён, новый создан на том же линке. Робот стал снегоуборщиком.

### 11.9 Высыпание — только по запросу

> ⚠ **Решение:** высыпание **только по запросу** (команда от стека робота). НЕТ автоматического высыпания при наклоне ковша/кузова.

Стек робота управляет джоинтами и шлёт команду «высыпать»:

```yaml
# Вариант: команда плагину
POST /agents/excavator/services/dump
{ "rate": 0.5, "duration": 2.0 }
```

Плагин начинает порционное высыпание — каждый тик маленькая `MaterialTransfer` с объёмом `dump_rate * dt`. Это даёт визуальное «постепенное высыпание».

**Что НЕ делается:**
- Нет автоматического spill при наклоне
- Нет «угла начала высыпания»
- Высыпание управляется явно

Визуальная анимация (поток падающего материала, дуга от ротора) — это **отдельный визуальный hint** (см. §13.5), не симуляционная физика.

---

## 12. Транспортный слой

### 12.1 Per-agent транспорт

> **Ключевое решение:** каждый агент выбирает свой транспорт. Один глобальный адаптер — недостаточно гибко для open-source.

```yaml
agents:
  - name: robot_0
    transport: ros2
    ros2:
      domain_id: 50

  - name: robot_1
    transport: http        # лёгкая интеграция без ROS

  - name: robot_2
    transport: stub        # управление только из визуализатора/скрипта

# Глобальный default:
defaults:
  transport: ros2
  ros2:
    domain_id_start: 50    # авто-инкремент для остальных
```

### 12.2 Пул адаптеров — шаринг ресурсов

Если 50 агентов используют ROS2, **не нужно** создавать 50 контекстов. Адаптер создаётся **по типу**, обслуживает всех своих:

```
TransportPool:
  ros2_adapter (singleton)  → агенты [0..49]
  http_adapter (singleton)  → агенты [50, 51]
  stub_adapter (singleton)  → агенты [52, 53, 54]
```

Если ROS2 упал — HTTP-агенты продолжают работать.

### 12.3 domain_id — параметр ROS2-адаптера, НЕ ядра

Раньше `domain_id` был полем агента в общем конфиге. **Это нарушение слоистости.** Теперь `domain_id` живёт **только в конфиге ROS2-адаптера**:

```yaml
- name: robot_0
  transport: ros2
  ros2:
    domain_id: 50    # ← параметр ROS2, ядро не знает про DDS
```

Другие транспорты `domain_id` не используют.

### 12.4 ITransportAdapter — интерфейс

```cpp
class ITransportAdapter {
public:
    virtual void register_command_topic(EntityId, const std::string& topic,
                                        std::function<void(const json&)> cb) = 0;
    virtual void register_service(EntityId, const std::string& name,
                                  std::function<json(const json&)> handler) = 0;
    virtual void register_subscription(EntityId, const std::string& topic,
                                       std::function<void(const json&)> cb) = 0;
    virtual void publish_event(EntityId, const TransportEvent&) = 0;
    virtual void tick(double sim_time) = 0;
};
```

`SimTransportBridge` — мост, который итерирует по плагинам всех агентов, читает их `command_topics()` / `service_names()` и регистрирует в адаптере.

**Плагин ничего не знает про конкретный транспорт.** Он говорит «я хочу /cmd_vel» — адаптер решает, что это значит в его протоколе (см. таблицу в §6.4).

### 12.5 Минимальный набор адаптеров

| Адаптер | Когда использовать |
|---------|---------------------|
| `Ros2TransportAdapter` | Полная интеграция с ROS2 |
| `HttpTransportAdapter` | Лёгкая интеграция: Python-скрипты, браузер, нестандартные стеки |
| `StubTransportAdapter` | Тесты, headless, скриптовые агенты |

`HttpTransportAdapter` критичен для open-source — большинство пользователей не захотят тащить ROS2 чтобы потестить логику.

---

## 13. Визуализация

### 13.1 IVizAdapter

Визуализация — это **сменный модуль**, как и транспорт. Не захардкоженный VizServer, а адаптер:

```cpp
class IVizAdapter {
public:
    virtual void publish(const WorldSnapshot&) = 0;
    virtual void on_command(std::function<void(const ControlCommand&)>) = 0;
};
```

**Реализации:**

| Адаптер | Назначение |
|---------|------------|
| `WebVizAdapter` | Текущий браузерный (Three.js + SSE) |
| `NullAdapter` | Headless без визуализации |
| `FileLogAdapter` | Запись снапшотов в файл для replay |
| `RViz2Adapter` | (будущее) интеграция с RViz2 |

### 13.2 WorldSnapshot — единственный «язык наружу»

Это **публичное лицо** симуляции. Структура данных, которую может прочитать любой потребитель:

```json
{
  "sim_time": 12.34,
  "real_time": 13.10,
  "speed_factor": 1.0,
  "tick_index": 1234,
  "entities": [
    {
      "id": "robot_0",
      "type": "agent",
      "world_pose": { "x": 1.0, "y": 0.5, "z": 0.0, "yaw": 0.7 },
      "links": [...],
      "plugins_extra": {
        "diff_drive": { "linear": 0.5, "angular": 0.1 },
        "battery": { "level": 0.85 },
        "lidar": { "points": [...] }
      },
      "visual_hint": { ... }
    }
  ],
  "zones": [...],
  "static_geometry": [...]
}
```

### 13.3 Три канала данных (по частоте обновления)

> **Решение:** разделение каналов решает проблему лагов от тяжёлых данных (лидар, траектории).

```
Канал 1: Core State (30 fps)
  — позы Entity, скорости, состояния акторов
  — маленький JSON, критичная latency

Канал 2: Heavy Data (2-5 fps)
  — точки лидара, траектории, overlay-данные
  — большой JSON, некритичная latency
  — отдельный SSE / polling endpoint

Канал 3: Static Data (по запросу)
  — геометрия сцены, зоны, URDF
  — только при загрузке или изменении
  — обычный GET с ETag
```

**Команды управления идут отдельным каналом** (REST POST). Никогда не конкурируют с потоком снапшотов. Это решает проблему текущего лага «при отображении траектории команды отрабатывают с запаздыванием».

### 13.4 Visual Hints — двухуровневая система

Плагины и эффекты декларируют **как их отображать** через `visual_hint`. Визуализатор не должен лезть в свой код для каждого нового плагина.

#### Уровень 1: Пресеты

```cpp
VisualHint hint() const override {
    return {
        "marker",
        {{"shape", "sphere"}, {"radius", 0.05},
         {"color", "#00FF00"}, {"offset", {0, 0, 0.3}}}
    };
}
```

Визуализатор имеет встроенную библиотеку: `marker`, `particles`, `glow`, `arrows`, `grid`, `trail`, `fan`, `cone`.

**Стандартные пресеты для эффектов:**

| Пресет | Используется для |
|--------|------------------|
| `snow` | Эффект льда / снега |
| `rain` | Дождь, водные зоны |
| `fog` | Туман |
| `glow` | Электромагнитные / свет |
| `dust` | Пыль |
| `smoke` | Дым / разрушение |

#### Уровень 2: Кастомные модули

Когда пресета не хватает — модульные JS-файлы:

```cpp
VisualHint hint() const override {
    return {
        "custom",
        {{"module", "lidar_fan"},
         {"num_rays", 360}, {"max_range", 10.0}}
    };
}
```

```
s2_visualizer/web/js/visuals/
  lidar_fan.js         ← рендерит веер лидарных лучей
  trajectory_trail.js  ← рендерит след
  snow_particles.js    ← снежинки
  default.js           ← дефолтный для неизвестных
```

Каждый файл — функция `render(scene, data, params)`. Новый плагин → новый JS-файл → рендер работает. Не нужно трогать `app.js`.

### 13.5 Transfer hints (анимация передачи материалов)

Плагин при передаче материала публикует `transfer_hint`:

```json
{
  "transfer_hint": {
    "type": "arc | pour | dump | spray | stream",
    "from": [x, y, z],
    "to": [x, y, z],
    "arc_height": 4.0,
    "material": "snow",
    "volume_per_second": 0.5,
    "particle_preset": "snow_spray",
    "active": true
  }
}
```

Визуализатор рисует анимацию **между** мгновенными состояниями (объём в source ↘, объём в target ↗).

| Тип | Визуал |
|-----|--------|
| `arc` | Партикли по параболе (снежный ротор) |
| `pour` | Поток сверху вниз (ковш экскаватора) |
| `dump` | Масса сползает под углом (кузов самосвала) |
| `spray` | Веер частиц (разбрасыватель) |
| `stream` | Непрерывная струя (жидкость из трубы) |

> ⚠ Это **только визуализация**. Симуляция переносит объём мгновенно. Гравитация и settling — на стороне принимающей сущности (DirtPileBehavior).

### 13.6 Управление через визуализатор

Визуализатор **не имеет прямого доступа** к SimEngine. Он шлёт команды через `IVizAdapter.on_command()` → REST API → ядро.

UI визуализатора может управлять:
- Sim control (pause/resume/reset/step/speed)
- World editing (move agent, toggle zone, edit prop)
- Plugin control (отправка команд конкретному плагину)
- Scene management (load/save/new)
- Hot patch (добавить плагин агенту, добавить зону)

---

## 14. Control API (REST)

### 14.1 Единый стиль

> **Решение:** убрать смешанный стиль (query-параметры + REST). Только REST + JSON.

### 14.2 Категории команд

```
1. Sim Control — управление временем
2. World Editing — изменение состояния мира
3. Plugin Control — команды плагинам
4. Scene Management — работа со сценами
5. Registry — справочники типов (для UI редактора)
```

### 14.3 Sim Control

```
POST /sim/pause
POST /sim/resume
POST /sim/reset
POST /sim/step                        { "ticks": 1 }
POST /sim/speed                       { "multiplier": 2.0 }
GET  /sim/status                      → { sim_time, paused, speed_factor, ... }
```

### 14.4 World Editing

```
# Entities
GET    /world/entities
GET    /world/entities/{id}
POST   /world/entities                ← spawn
DELETE /world/entities/{id}           ← despawn
PUT    /world/entities/{id}/pose      { x, y, z, yaw }
PUT    /world/entities/{id}/enabled   { enabled: true }

# Zones
GET    /world/zones
POST   /world/zones                   ← spawn (с lifecycle)
DELETE /world/zones/{id}
PUT    /world/zones/{id}/enabled
PUT    /world/zones/{id}/shape
PUT    /world/zones/{id}/strength

# Plugins (hot reload)
GET    /world/entities/{id}/plugins
POST   /world/entities/{id}/plugins   ← добавить плагин
PUT    /world/entities/{id}/plugins/{plugin_type}    ← обновить конфиг
DELETE /world/entities/{id}/plugins/{plugin_type}
```

### 14.5 Plugin Control

```
POST /agents/{id}/input/{plugin_type}             <json>
POST /agents/{id}/services/{service_name}         <json>
GET  /agents/{id}/topics/{topic_name}             ← последнее значение
```

### 14.6 Scene Management

```
GET  /scenes                          ← список доступных сцен
POST /scenes/load                     { name: "warehouse.yaml" }
POST /scenes/save                     { name: "current.yaml" }
POST /scenes/save-as                  { name: "..." }
GET  /scenes/active                   ← текущее имя
POST /scenes/new                      ← пустая сцена
```

### 14.7 Registry (для UI редактора)

```
GET /api/plugins/registry             → список доступных типов с config_schema
GET /api/behaviors/registry
GET /api/effects/registry
GET /api/transports/registry
```

Каждый тип возвращает `config_schema` для генерации формы редактирования.

---

## 15. Коммуникационные каналы и KernelCommands

### 15.1 Четыре канала

| Канал | Направление | Природа | Кто пишет / читает |
|-------|-------------|---------|---------------------|
| **SharedState** | внутри Entity | накопление contributions | Resource → Resolver → Actuation |
| **EventBus** | между Entity | события (синхронные факты) | Interaction publish, любой subscribe |
| **WorldQuery** | плагин → ядро | read-only запросы к миру | Sensor/Interaction read |
| **KernelCommand** | плагин → ядро | write-операции | Interaction publish, REST API |

### 15.2 EventBus — события

События — это **факты о мире**, не команды. Любая Entity может publish, любая subscribe.

**Типичные события:**

```
EntitySpawned    { id, type }
EntityDespawned  { id }
ActorStateChanged { actor_id, old_state, new_state }
SignalActivated  { signal_id, source_entity }
SignalDeactivated{ signal_id, source_entity }
GrabAttempt      { agent, target }
GrabSucceeded    { agent, target }
GrabFailed       { agent, target, reason }
ZoneEntered      { zone_id, entity_id }
ZoneExited       { zone_id, entity_id }
DamageDealt      { source, target, amount, type }
```

### 15.3 WorldQuery — read-only API

```cpp
class WorldQuery {
public:
    // Поиск
    std::vector<EntityId> find_in_radius(Vec3 center, double r, EntityFilter f);
    std::vector<EntityId> find_in_box(Box, EntityFilter);
    std::optional<EntityId> find_nearest(Vec3, EntityFilter);
    EntityId find_entity_below(Vec3 position);

    // Сигналы
    std::vector<Signal> find_signals_of_type(std::string type, Vec3, DetectionVolume);

    // Геометрия
    bool has_line_of_sight(EntityId from, EntityId to);
    RaycastResult raycast(Vec3 origin, Vec3 dir, double max_range);

    // Зоны
    std::vector<ZoneId> zones_at(Vec3 position);
    bool is_in_zone(EntityId, ZoneId);

    // Деформация
    std::optional<EntityId> find_deformable_in_box(Box, EntityFilter);
};
```

> ⚠ Никаких записей через WorldQuery. Только чтение.

### 15.4 KernelCommands — единая точка изменения мира

**Все изменения мира идут через KernelCommands.** Никакого прямого `world.entities.push_back(...)` из плагинов.

#### Sim control
```
PauseSim
ResumeSim
ResetSim
StepSim        { ticks }
SetSpeed       { multiplier }
```

#### Entity lifecycle
```
SpawnEntity    { type, config_yaml }
DespawnEntity  { id }
SetPose        { id, pose }
SetEnabled     { id, enabled }
```

#### Plugin lifecycle (hot reload)
```
AddPlugin      { entity_id, plugin_type, config }
RemovePlugin   { entity_id, plugin_type }
ConfigPlugin   { entity_id, plugin_type, new_config }
```

#### Zones
```
SpawnZone      { template?, shape, effects, attached_to? }
DespawnZone    { id }
ToggleZone     { id, enabled }
SetZoneShape   { id, shape }
SetZoneStrength{ id, strength }
```

#### Interactions
```
Interact       { source_id, target_id, action, params }   ← ЕДИНАЯ команда взаимодействия
AttachObject   { parent_id, link, child_id, local_pose }
DetachObject   { child_id, drop_pose? }
```

#### Materials
```
MaterialTransfer { source, source_plugin, target, target_plugin, volume, material, position, hint }
DeformEntity     { target, tool_pose, tool_geometry, tool_velocity, dt }
```

#### Scenes
```
LoadScene  { name }
SaveScene  { name }
NewScene
```

### 15.5 Interact — единая точка взаимодействия

> **Финальное решение:** все взаимодействия Entity ↔ Entity идут через `KernelCommand::Interact`. Никаких прямых вызовов `actor.behavior.on_interact()` из плагинов агента.

```
Plugin (Interaction роли) → KernelCommand::Interact{...}
  → Kernel валидирует:
    • source_id и target_id существуют
    • дистанция допустима (если требуется)
    • capabilities target подходят
    • action валиден для target.behavior
  → Kernel вызывает: target.behavior.on_interact(source, action, params)
```

**Гибкость:** любая Entity взаимодействует с любой другой. Агент с актором, актор с актором, актор с пропом. Один механизм, одна точка валидации.

---

## 16. Жизненный цикл тика

### 16.1 Фазы тика

Это **порядок** — не описание чего-либо нового, а закрепление того, в каком порядке всё происходит.

```
═══════════════════════════════════════════════════════════════════════
ФАЗА 0: Накопленные KernelCommands
═══════════════════════════════════════════════════════════════════════
  Все команды, поступившие за прошлый тик (REST API, плагины),
  применяются атомарно перед началом обработки

  → Spawn/Despawn entities
  → Add/Remove plugins
  → Spawn/Despawn zones
  → Hot-patch конфигов

═══════════════════════════════════════════════════════════════════════
ФАЗА 1: Входящие команды (transport)
═══════════════════════════════════════════════════════════════════════
  → Транспорт читает входящие сообщения
  → Распределяет по плагинам через handle_input()

═══════════════════════════════════════════════════════════════════════
ФАЗА 2: Акторы — update своих behavior
═══════════════════════════════════════════════════════════════════════
  Для каждого актора:
    • plugins.pre_resolve(dt)
    • <Resolve SharedState>
    • behavior.update(dt, ctx)            ← FSM, settling, движение
    • plugins.update(dt)

═══════════════════════════════════════════════════════════════════════
ФАЗА 3: Агенты — для каждого агента
═══════════════════════════════════════════════════════════════════════
  3a. Resource plugins — pre_resolve()
        → Battery: drain, contributions
        → Payload: contributions

  3b. Zone effects (для всех Entity внутри зон)
        → ON_ENTER (если только что вошли)
        → WHILE_INSIDE (постоянно)
        → ON_EXIT (если только что вышли)
        → Применение CONTRIBUTION / STATE_CHANGE / SENSOR_MOD / OWN_EFFECT_SPAWN

  3c. Own effects (динамические эффекты на Entity)
        → TireDriftContributor.contribute() и т.п.

  3d. ═══ RESOLVER ═══
        → effective.speed_scale = product(scales)
        → effective.motion_locked = OR(locks)
        → effective.velocity_addition = sum(additions)
        → effective.max_speed_cap = MIN(caps)
        → ...

  3e. Sensor SENSOR_MOD applied
        → FogEffect: модификация параметров лидара

  3f. Actuation plugin — update()
        → DiffDrive читает effective, вычисляет velocity
        → Gravity snap Z, скольжение

  3g. Кинематика
        → position += R * velocity * dt
        → application max_speed_cap

  3h. Collision response
        → slide, push-out, walkable contacts

  3i. Surface alignment
        → pitch/roll из нормали поверхности

═══════════════════════════════════════════════════════════════════════
ФАЗА 4: Sensor plugins
═══════════════════════════════════════════════════════════════════════
  ВАЖНО: после кинематики и коллизий!
  → Lidar: raycast из ФИНАЛЬНОЙ позиции
  → GNSS, IMU, Detectors

═══════════════════════════════════════════════════════════════════════
ФАЗА 5: Interaction plugins
═══════════════════════════════════════════════════════════════════════
  → DoorOpener: проверка proximity, KernelCommand::Interact
  → Grabber: AttachObject / DetachObject
  → BucketAttachment: MaterialTransfer / DeformEntity

═══════════════════════════════════════════════════════════════════════
ФАЗА 6: Attachments
═══════════════════════════════════════════════════════════════════════
  → Обновить позы привязанных объектов

═══════════════════════════════════════════════════════════════════════
ФАЗА 7: Snapshot + публикация
═══════════════════════════════════════════════════════════════════════
  → build_snapshot()
  → viz.publish(snapshot)
  → transport.publish(sensor_data)

═══════════════════════════════════════════════════════════════════════
ФАЗА 8: Очистка
═══════════════════════════════════════════════════════════════════════
  → state.clear_contributions() для каждой Entity
  → Удаление зон с истёкшим lifecycle
  → Удаление Entity, помеченных DespawnEntity
```

### 16.2 Почему сенсоры в фазе 4

Лидар должен бросать лучи из **финальной** позиции агента, а не из позиции до коллизий. Сейчас в текущем коде все плагины вызываются в одном `update()` — это работает по случайности (порядок в массиве), но архитектурно неправильно. Финальная архитектура **выделяет сенсоры в отдельную фазу после кинематики**.

---

## 17. Hot Reload и runtime-модификация

### 17.1 Два режима загрузки

#### Режим 1: Полная загрузка (full reload)

```
POST /scenes/load { name: "warehouse.yaml" }

→ SimEngine останавливается
→ Все Entity удаляются
→ Транспорт переинициализируется
→ Время обнуляется
→ Загрузка новой сцены
```

Используется для смены сцены целиком.

#### Режим 2: Hot patch — runtime-изменение

```
POST /world/entities/{id}/plugins  { add: [{type: "lidar", config: {...}}] }
POST /world/zones                  { add: [...] }
PUT  /world/entities/{id}          { config }
DELETE /world/zones/{id}
POST /scenes/save                  ← сохранить текущее состояние в YAML
```

**Мир продолжает работать.** Симуляция не останавливается (или ставится на паузу на 1 тик для атомарного изменения).

### 17.2 Что разрешено в hot patch

| Операция | Поддерживается |
|----------|:---:|
| Добавить плагин агенту | ✅ |
| Удалить плагин агента | ✅ |
| Изменить конфиг плагина | ✅ |
| Создать Entity | ✅ |
| Удалить Entity | ✅ |
| Создать зону | ✅ |
| Удалить/выключить зону | ✅ |
| Изменить URDF агента | ❌ (full reload) |
| Сменить транспорт агента | ❌ (full reload) |

### 17.3 Жизненный цикл плагина при hot reload

```
AddPlugin   → on_spawn(entity)
RemovePlugin → on_despawn(entity)
ConfigPlugin → from_config(new) + initialize(entity)
ResetSim    → on_reset() для всех плагинов
```

> ⚠ **Важно:** все плагины **обязаны** корректно реализовать `on_reset()`. Сейчас в коде есть баг: при reset DiffDrive не сбрасывает `external_linear_velocity_`, Battery не сбрасывает заряд. Это нужно починить во всех плагинах.

### 17.4 Доступные типы плагинов для UI

```
GET /api/plugins/registry
→ [
    { type: "diff_drive", label: "Diff Drive", role: "actuation",
      config_schema: [...] },
    { type: "lidar", label: "2D Лидар", role: "sensor",
      config_schema: [...] },
    ...
  ]
```

UI редактора генерирует формы из `config_schema`.

---

## 18. Время симуляции

### 18.1 Три времени

```
sim_time      — время симуляции (с), монотонно растёт по dt
real_time     — реальное время (для синхронизации)
speed_factor  — множитель скорости (1.0 = real-time, 2.0 = в 2× быстрее)
dt            — фиксированный шаг (НЕ меняется при ускорении!)
```

### 18.2 Ускорение через частоту тиков

> **Ключевое решение:** ускорение = больше тиков в секунду, **dt не меняется**.

| speed_factor | Real-time tick rate | dt |
|---|---|---|
| 0.5 (slow) | 50 Гц / 2 | 0.01 |
| 1.0 (real) | 50 Гц | 0.01 |
| 2.0 (fast) | 100 Гц | 0.01 |
| 10.0 (max) | 500 Гц (или столько, сколько успеваем) | 0.01 |

**Почему так:**
- Детерминизм — результат не зависит от скорости
- ROS2 stack робота не сбит с толку (sim_time идёт как обычно, просто быстрее)
- Численная стабильность сохраняется

### 18.3 sim_time — primary clock для всех потребителей

```
ROS2 transport  → /clock топик (rosgraph_msgs/Clock)
HTTP transport  → поле sim_time в каждом ответе/событии
Snapshot        → корневое поле sim_time
```

Стек робота получает sim_time как «настоящее» время. Никаких рассинхронов.

---

## 19. Глоссарий

| Термин | Определение |
|--------|-------------|
| **Entity** | Базовая сущность мира (агент, актор, проп) |
| **Agent** | Entity управляемая внешним контроллером (ROS2/HTTP), имеет полный набор слоёв |
| **Actor** | Entity с собственным поведением (FSM/scripted/settling) |
| **Prop** | Пассивная Entity без update(), без SharedState |
| **Zone** | Правило пространства с эффектами (НЕ Entity) |
| **Signal** | Обнаруживаемый сигнал на Entity (aruco/wire/radio/...) |
| **Capability** | Стандартизированный признак Entity для matching эффектов |
| **Tag** | Свободные метаданные для детализации |
| **SharedState** | Канал общения плагинов через contributions |
| **Resolver** | Собирает contributions в effective constraints |
| **Effective** | Итоговые ограничения после resolver (читаются Actuation) |
| **Own Effect** | Динамический эффект на конкретной Entity (после mutation) |
| **Effect Tag** | Семантический тег эффекта (для immune_to_effects) |
| **Visual Hint** | Декларация для визуализатора как отображать |
| **Transfer Hint** | Hint для анимации передачи материала |
| **WorldSnapshot** | Сериализованный снимок мира (публичное лицо симуляции) |
| **WorldQuery** | Read-only API ядра для плагинов |
| **EventBus** | Канал событий (синхронные факты) |
| **KernelCommand** | Команда изменения мира (write-операция) |
| **Hot Patch** | Runtime-изменение мира без перезапуска |
| **MaterialTransfer** | Единая команда передачи материала между сущностями |
| **DeformEntity** | Команда деформации сущности инструментом |
| **Displacement** | Распределение материала вокруг занятого пространства |
| **Per-link detection** | Режим зоны: проверять каждый линк kinematic tree |
| **Effect Absorption** | Эффект всегда срабатывает; immunity определяет поглощение |

---

## 20. Приложение: что НЕ входит в архитектуру

Эти решения **отвергнуты** в ходе обсуждения и должны оставаться отвергнутыми (если только не будет серьёзного обоснования для возврата):

| Решение | Почему отвергнуто |
|---------|-------------------|
| **Heightmap** как отдельная система | Не описывает пещеры/мосты/многоэтажные здания. Заменён на static geometry из примитивов. |
| **Раздельные интерфейсы** IActuationPlugin / ISensorPlugin / IInteractionPlugin / IResourcePlugin | Жёсткость, проблемы с совмещением ролей (Gravity = resource + двигает). Заменено на один IAgentPlugin + role(). |
| **Раздельные интерфейсы** IMaterialSource / IMaterialReceiver / IDeformable | Слишком много интерфейсов. Заменено на опциональные методы внутри IAgentPlugin / IActorBehavior. |
| **Отдельные KernelCommands** ScoopDirt / DumpDirt / ScoopWater / ... | Ядро бы знало про каждый материал. Заменено на единые MaterialTransfer + DeformEntity. |
| **Автоматическое высыпание по углу** наклона ковша/кузова | Лишний слой «физики». Замена: высыпание только по запросу от стека робота. |
| **Полёт частиц материала в пространстве** | Это физический симулятор которым мы не являемся. Заменено на мгновенный transfer + visual hint (анимация). |
| **Жидкостная физика SPH/CFD** | Слишком дорого, не наш уровень абстракции. Заменено на settling 2D-grid с friction angle. |
| **Глобальный единый транспортный адаптер** | Не позволяет mix ROS2+HTTP+stub. Заменено на per-agent transport с пулом. |
| **domain_id как поле агента в общем конфиге** | Нарушение слоистости (ядро не должно знать про DDS). Перенесено в конфиг ROS2-адаптера. |
| **Декларативное визуальное состояние акторов** (state="opening") | Behavior всё равно меняет collision — двойная интерпретация = рассинхрон. Заменено на императивное (behavior двигает геометрию и обновляет collision напрямую). visual_hint остался для декоративного. |
| **SharedState на пропах по умолчанию** | Размывает границу с актором. Если нужно накопление урона / сложные эффекты → это актор. Пропы — бинарные эффекты. |
| **Множественные behavior на акторе** (Door + Lock) | Усложняет порядок. Заменено: одно behavior + плагины через PluginHost для дополнительных слоёв. |
| **Жёсткие 4 типа эффектов** (MODIFIER/CONTINUOUS/MUTATION/SENSOR) | Частный случай. Заменено на trigger × action — даёт новые комбинации (ON_ENTER + CONTRIBUTION и т.п.). |
| **Прямой вызов** actor.behavior.on_interact() из плагина агента | Нет валидации. Заменено на KernelCommand::Interact с проверкой ядром. |
| **Смешанный API** (query-параметры /command?cmd=... + REST /api/...) | Сбивает с толку внешних пользователей. Заменено на единый REST. |
| **Один SSE-поток для всего** (позы + лидар + траектории) | Лидар блокирует команды. Заменено на разделение каналов по частоте обновления. |
| **Захардкоженный VizServer** | Невозможно подключить другой визуализатор. Заменено на IVizAdapter. |
| **Захардкоженный рендер плагинов в app.js** | Каждый новый плагин = правка app.js. Заменено на двухуровневую систему visual hints (presets + custom modules). |
| **Сигнал-driven через произвольный EventBus подписки** на абстрактные события | Теряется доменная логика. Заменено: конкретные controller-плагины (DoorWireController) с общей базой SignalListenerBase. EventReactor оставлен для тривиальных случаев. |
| **PedestrianDetector / ConveyorDetector** как отдельные плагины | Копипаста. Заменено на один EntityDetector с фильтрами в конфиге. |

---

## Приложение А: контрольный список миграции

Чтобы перейти текущую кодовую базу к этой архитектуре, нужно (порядок имеет значение):

1. **Стабилизировать lifecycle плагинов** — добавить on_reset/on_spawn/on_despawn и реализовать во всех существующих плагинах.
2. **Ввести Entity модель** как базу — постепенный рефакторинг Agent/Actor/Prop с общей основой.
3. **Per-agent transport** — выделить TransportPool, перенести domain_id в ROS2-адаптер.
4. **HttpTransportAdapter** — реализовать минимальный, чтобы развязать с ROS2.
5. **Разделить каналы визуализатора** — Core State / Heavy Data / Static.
6. **Trigger × Action для эффектов** — рефакторинг существующих 4-х типов в новую модель.
7. **Visual hints v1** — пресеты + 2-3 кастомных модуля.
8. **MaterialTransfer + DeformEntity** — заменить любые специфичные команды.
9. **Effect Absorption + per-link immunity** — добавить новый detection_mode.
10. **REST API единый** — выпилить `/command?cmd=...`, оставить только REST.
11. **Hot patch базовый** — add/remove plugin, add/remove zone.
12. **Registry endpoints** — для UI редактора.

Каждый шаг — отдельная задача в backlog.

---

*Документ описывает фундамент S2. Конкретные плагины, эффекты, поведения — это надстройка, которая меняется по мере развития проекта. Принципы из §1.2 и негативные решения из §20 — это контракт, нарушение которого требует серьёзного обоснования.*