#pragma once

/**
 * @file sim_engine.hpp
 * SimEngine — главный цикл симуляции.
 *
 * Управляет тиковой петлёй с фиксированным шагом dt.
 * Каждый тик состоит из 9 именованных фаз:
 *  Phase 0: Обработка KernelCommands из очереди (HTTP-тред + плагины)
 *  Phase 1: Входящие команды транспорта (пусто — Phase 5)
 *  Phase 2: Акторы (FSM-переходы, поведения) (пусто — Phase 2)
 *  Phase 3: Агенты — ресурсы, resolve, актуация, кинематика, коллизии
 *  Phase 4: Сенсоры (строго после кинематики в Phase 3)
 *  Phase 5: Interaction-плагины (Grabber, DoorOpener)
 *  Phase 6: Обновление attachments (пусто — Phase 2)
 *  Phase 7: Публикация снапшота и транспорт
 *  Phase 8: Очистка (clear_contributions только здесь)
 */

#include <s2/collision_system.hpp>
#include <s2/components/pending_teleport.hpp>
#include <s2/kernel_command.hpp>
#include <s2/plugin_base.hpp>
#include <s2/raycast_engine.hpp>
#include <s2/sim_bus.hpp>
#include <s2/world.hpp>
#include <s2/world_query.hpp>
#include <s2/world_snapshot.hpp>
#include <s2/zone_system.hpp>
#include <nlohmann/json.hpp>

#include <thread>
#include <atomic>
#include <chrono>
#include <map>
#include <mutex>
#include <iostream>

namespace s2
{

// Forward declare — VizServer определён в s2_visualizer
class VizServer;

/**
 * @brief Главный движок симуляции.
 *
 * Отвечает за:
 *  - Тиковую петлю с фиксированным шагом (update_rate Hz)
 *  - 9-фазный lifecycle: phase0..phase8
 *  - Очередь KernelCommands с mutex-защитой (HTTP thread → sim thread)
 *  - Синхронизацию через EventBus
 *
 * Два режима работы:
 *  - step(n) — для тестов, вызывает tick() n раз
 *  - run() — для runtime, бесконечный цикл с sleep между тиками
 */
class SimEngine
{
public:
  /**
   * @brief Конфигурация симуляции.
   */
  struct Config
  {
    double update_rate{100.0};     ///< Частота обновления симуляции (Гц)
    double viz_rate{30.0};         ///< Частота отправки данных визуализатору (Гц)
    double transport_rate{30.0};   ///< Частота публикации данных транспортному адаптеру (Гц)
  };

  /**
   * @brief Конструктор.
   * @param config Конфигурация симуляции
   */
  explicit SimEngine(Config config)
      : config_(std::move(config))
  {
    dt_ = 1.0 / config_.update_rate;
  }

  /**
   * @brief Загрузить мир в движок.
   * Сохраняет начальные позы и скорости всех агентов для последующего reset().
   * @param world Мир со всеми сущностями
   */
  void load_world(SimWorld world)
  {
    world_ = std::move(world);
    save_initial_states();
    // Передаём статическую геометрию в систему коллизий и raycast
    collision_system_.set_static_geometry(world_.static_geometry());
    raycast_engine_.set_static_geometry(world_.static_geometry());
    // Инициализируем ZoneSystem из зон мира
    zone_system_ = ZoneSystem{};
    if (effect_factory_)
      zone_system_.set_effect_factory(effect_factory_);
    for (auto& zone : world_.zones())
      zone_system_.add_zone(std::move(zone));
    world_.zones().clear();
  }

  /**
   * @brief Заменить статическую геометрию мира и синхронизировать систему коллизий.
   *
   * Используется при редактировании сцены в рантайме через редактор.
   * В отличие от load_world(), не затрагивает агентов и начальные состояния.
   * @param prims Новый список примитивов статической геометрии
   */
  void update_static_geometry(const std::vector<WorldPrimitive>& prims)
  {
    world_.static_geometry().clear();
    for (const auto& p : prims)
      world_.add_static_primitive(p);
    collision_system_.set_static_geometry(world_.static_geometry());
    raycast_engine_.set_static_geometry(world_.static_geometry());
  }

  /**
   * @brief Установить фабрику эффектов зон.
   * Должна быть вызвана ДО load_world(), иначе плагины эффектов не создадутся.
   * Фабрика сохраняется и применяется при каждом вызове load_world().
   * @param factory Функция вида unique_ptr<EffectPlugin>(type, params)
   */
  void set_effect_factory(EffectFactory factory) {
    effect_factory_ = std::move(factory);
  }

  /**
   * @brief Установить указатель на визуализатор (не владеет).
   * @param viz Указатель на VizServer
   */
  void set_viz_server(VizServer* viz) { viz_server_ = viz; }

  /**
   * @brief Установить callback, вызываемый после каждого тика симуляции.
   * Используется транспортным мостом для публикации сенсоров и TF.
   * Вызывается с частотой transport_rate из конфигурации.
   * @param cb Callback вида void(const SimWorld&, double sim_time)
   */
  using PostTickCallback = std::function<void(const SimWorld&, double /*sim_time*/)>;
  void set_post_tick_callback(PostTickCallback cb) { post_tick_cb_ = std::move(cb); }

  /**
   * @brief Добавить команду в очередь ядра.
   *
   * Потокобезопасно — вызывается из HTTP-треда (REST API, VizServer).
   * Команда применяется в Phase 0 следующего тика.
   *
   * Безопасность: mutex защищает от data race между HTTP-тредом (push_command)
   * и sim-тредом (phase0_kernel_commands drain). Соответствует T-00-09.
   *
   * @param cmd Команда ядра (SetPose, SpawnEntity, и т.п.)
   */
  void push_command(KernelCommand cmd)
  {
    std::lock_guard<std::mutex> lock(command_queue_mutex_);
    command_queue_.push_back(std::move(cmd));
  }

  /**
   * @brief Выполнить n тиков (для тестов).
   * @param n Количество тиков (по умолчанию 1)
   */
  void step(int n = 1)
  {
    for (int i = 0; i < n; ++i)
    {
      tick();
    }
  }

  /**
   * @brief Запустить бесконечный цикл симуляции (для runtime).
   *
   * Вызывает tick() с частотой update_rate.
   * Останавливается при вызове stop().
   */
  void run()
  {
    running_ = true;
    auto period = std::chrono::duration<double>(1.0 / config_.update_rate);

    while (running_)
    {
      auto start = std::chrono::steady_clock::now();
      tick();
      auto elapsed = std::chrono::steady_clock::now() - start;
      if (elapsed < period)
      {
        std::this_thread::sleep_for(period - elapsed);
      }
    }
  }

  /**
   * @brief Остановить цикл симуляции.
   *
   * Безопасно прерывает run() из другого потока.
   */
  void stop()
  {
    running_ = false;
  }

  /**
   * @brief Поставить симуляцию на паузу.
   *
   * Tick продолжает вызываться, но время не обновляется и агенты не двигаются.
   */
  void pause() { paused_ = true; }

  /**
   * @brief Возобновить симуляцию после паузы.
   */
  void resume() { paused_ = false; }

  /**
   * @brief Проверить, на паузе ли симуляция.
   */
  bool is_paused() const { return paused_; }

  /**
   * @brief Сбросить симуляцию к начальному состоянию.
   *
   * Восстанавливает все world_pose и world_velocity агентов из начальных значений,
   * сбрасывает sim_time в 0 и ставит симуляцию на паузу.
   */
  void reset()
  {
    restore_initial_states();
    sim_time_ = 0.0;
    paused_ = true;
  }

  /**
   * @brief Установить позу агента по ID (для интерактивного перемещения).
   * @param agent_id ID агента
   * @param pose Новая поза
   * @return true если агент найден и поза установлена
   */
  bool set_agent_pose(AgentId agent_id, const Pose3D& pose)
  {
    for (auto& agent : world_.agents()) {
      if (agent.id == agent_id) {
        agent.world_pose = pose;
        return true;
      }
    }
    return false;
  }

  /**
   * @brief Текущее симуляционное время (секунды).
   */
  double sim_time() const { return sim_time_; }

  /**
   * @brief Шаг симуляции (секунды).
   */
  double dt() const { return dt_; }

  /**
   * @brief Получить доступ к миру.
   */
  const SimWorld& world() const { return world_; }

  /**
   * @brief Получить изменяемый доступ к миру.
   */
  SimWorld& world() { return world_; }

  /**
   * @brief Получить шину событий.
   */
  SimBus& bus() { return bus_; }

  /**
   * @brief Получить константную ссылку на шину событий.
   */
  const SimBus& bus() const { return bus_; }

  /**
   * @brief Получить доступ к системе зон.
   */
  ZoneSystem& zone_system() { return zone_system_; }

  /**
   * @brief Получить константный доступ к системе зон.
   */
  const ZoneSystem& zone_system() const { return zone_system_; }

  /**
   * @brief Передать входные данные конкретному плагину агента.
   * @param agent_id ID агента
   * @param plugin_type Тип плагина (напр. "diff_drive")
   * @param json_input JSON-строка с входными данными
   * @return true если агент и плагин найдены и плагин принял вход
   */
  bool handle_plugin_input(AgentId agent_id, const std::string& plugin_type, const std::string& json_input)
  {
    for (auto& agent : world_.agents()) {
      if (agent.id == agent_id) {
        for (auto& plugin : agent.plugins) {
          if (plugin->type() == plugin_type || plugin_key(*plugin) == plugin_type) {
            plugin->handle_input(json_input);
            return true;
          }
        }
        return false;
      }
    }
    return false;
  }

  /**
   * @brief Получить JSON Schema входных данных всех плагинов агента.
   * @param agent_id ID агента
   * @return JSON-объект: { plugin_type -> inputs_schema }
   */
  std::string get_plugin_inputs_schemas(AgentId agent_id) const
  {
    nlohmann::json schemas;
    for (const auto& agent : world_.agents()) {
      if (agent.id == agent_id) {
        for (const auto& plugin : agent.plugins) {
          if (plugin->has_inputs()) {
            schemas[plugin->type()] = nlohmann::json::parse(plugin->inputs_schema(), nullptr, false);
            if (schemas[plugin->type()].is_discarded()) {
              schemas[plugin->type()] = plugin->inputs_schema();
            }
          }
        }
        return schemas.dump();
      }
    }
    return "{}";
  }

public:
  /**
   * @brief Собрать WorldSnapshot из текущего состояния мира.
   */
  WorldSnapshot build_snapshot() const
  {
    WorldSnapshot snap;
    snap.sim_time = sim_time_;
    snap.paused = paused_;
    snap.plugins_data = build_plugins_data();
    snap.plugin_inputs_schemas = build_plugin_inputs_schemas();

    // Агенты
    for (const auto& agent : world_.agents()) {
      AgentSnapshot as;
      as.id = agent.id;
      as.name = agent.name;
      as.pose = agent.world_pose;
      as.velocity = agent.world_velocity;
      as.velocity_addition = agent.state.effective().velocity_addition;
      as.visual = agent.visual;
      as.effective_speed_scale = agent.state.effective().speed_scale;
      as.motion_locked = agent.state.effective().motion_locked;

      for (const auto& plugin : agent.plugins)
        plugin->contribute_snapshot(as.extra, agent);

      const auto* tire = agent.state.get<TirePunctureData>();
      if (tire) as.tire_punctured = tire->punctured;

      as.has_collision = agent.has_collision;
      if (agent.has_collision) {
        if (agent.bounding.type == ShapeType::SPHERE) {
          as.bounding_type   = "sphere";
          as.bounding_radius = agent.bounding.radius;
        } else {
          as.bounding_type = "box";
          as.bounding_size = agent.bounding.size * 2.0;
        }
      }

      if (agent.kinematic_tree) {
          for (const auto& link : agent.kinematic_tree->links()) {
              LinkFrameSnapshot lfs;
              lfs.name       = link.name;
              lfs.world_pose = agent.kinematic_tree->compute_world_pose(link.name, agent.world_pose);
              lfs.visual     = link.visual;
              as.kinematic_frames.push_back(std::move(lfs));
          }
      }

      snap.agents.push_back(as);
    }

    // Пропы
    for (const auto& prop : world_.props()) {
      PropSnapshot ps;
      ps.id = prop.id;
      ps.type = prop.type;
      ps.pose = prop.world_pose;
      ps.visual = prop.visual;
      ps.movable = prop.movable;
      snap.props.push_back(ps);
    }

    // Акторы
    for (const auto& actor : world_.actors()) {
      ActorSnapshot acs;
      acs.id = actor.id;
      acs.name = actor.name;
      acs.pose = actor.world_pose;
      acs.visual = actor.visual;
      acs.state = actor.current_state;
      snap.actors.push_back(acs);
    }

    // Зоны
    for (const auto& zone : zone_system_.all_zones()) {
      if (!zone.visible) continue;
      ZoneSnapshot zs;
      zs.id          = zone.id;
      zs.enabled     = zone.enabled;
      zs.shape_type  = zone.shape.type;
      zs.center      = zone.shape.center;
      zs.radius      = zone.shape.radius;
      zs.half_size   = zone.shape.half_size;
      zs.half_height = zone.shape.half_height;
      zs.color       = zone.color;
      zs.opacity     = zone.opacity;
      zs.visible     = zone.visible;
      zs.label       = zone.label;
      zs.agents_inside.assign(zone.inside_agents.begin(), zone.inside_agents.end());
      snap.zones.push_back(std::move(zs));
    }

    // Геометрия
    for (const auto& prim : world_.static_geometry()) {
      GeometrySnapshot gs;
      gs.type = prim.type;
      gs.x = prim.pose.x;
      gs.y = prim.pose.y;
      gs.z = prim.pose.z;
      gs.yaw   = prim.pose.yaw;
      gs.pitch = prim.pose.pitch;
      gs.roll  = prim.pose.roll;
      gs.sx = prim.size.x();
      gs.sy = prim.size.y();
      gs.sz = prim.size.z();
      gs.radius = prim.radius;
      gs.height = prim.height;
      gs.color = prim.color;
      snap.geometry.push_back(gs);
    }

    return snap;
  }

private:
  // ─── Заглушка WorldQuery ──────────────────────────────────────────────────
  //
  // Передаётся в PluginContext пока WorldQueryImpl не реализован (Plan 05+).
  // Базовый класс WorldQuery — все методы возвращают пустые результаты.

  // NullWorldQuery — вложенный класс в private-секции SimEngine.
  // Использует методы WorldQuery по умолчанию (заглушки).
  class NullWorldQuery : public WorldQuery {};

  // ─── Тиковый цикл ─────────────────────────────────────────────────────────

  /**
   * @brief Один тик симуляции — 9 именованных фаз.
   *
   * Порядок фаз:
   *  0. Kernel commands (command_queue_ drain)
   *  1. Transport input (пусто — Phase 5 транспортного рефакторинга)
   *  2. Actors (FSM transitions) (пусто — Phase 2 Actor Foundation)
   *  3. Agents (ресурсы, resolve, актуация, кинематика, коллизии)
   *     — SENSOR и INTERACTION плагины пропускаются
   *  4. Sensors (update только для PluginRole::SENSOR)
   *  5. Interactions (update только для PluginRole::INTERACTION)
   *  6. Attachments (обновление поз привязанных объектов) (пусто — Phase 2)
   *  7. Snapshot + Viz + Transport publish
   *  8. Cleanup: clear_contributions() для всех агентов
   */
  void tick()
  {
    // Если на паузе — не обновляем время и не двигаем агентов.
    // Но всё равно отправляем снапшоты (визуализатор должен видеть paused).
    if (paused_) {
      viz_timer_ += dt_;
      double viz_interval = config_.viz_rate > 0 ? 1.0 / config_.viz_rate : 0.0;
      if (viz_server_ && viz_interval > 0 && viz_timer_ >= viz_interval) {
        viz_timer_ -= viz_interval;
        publish_viz();
      }
      return;
    }

    sim_time_ += dt_;

    phase0_kernel_commands();
    phase1_transport_input();
    phase2_actors();
    phase3_agents();
    phase4_sensors();
    phase5_interactions();
    phase6_attachments();
    phase7_snapshot_publish();
    phase8_cleanup();
  }

  // ─── Фазы тика ────────────────────────────────────────────────────────────

  /**
   * @brief Phase 0: Обработка накопленных KernelCommands.
   *
   * Дренирует command_queue_ атомарно (swap под mutex), применяет каждую команду
   * через std::visit. Вызывается первой — гарантирует атомарное применение
   * изменений мира до обновления агентов.
   *
   * Безопасность: swap под mutex защищает от data race (T-00-09).
   */
  void phase0_kernel_commands()
  {
    KernelCommandQueue local_queue;
    {
      std::lock_guard<std::mutex> lock(command_queue_mutex_);
      local_queue.swap(command_queue_);
    }

    for (const auto& cmd : local_queue)
    {
      std::visit([this](const auto& c) { apply_kernel_command(c); }, cmd);
    }
  }

  /**
   * @brief Phase 1: Входящие команды транспорта.
   * Транспорт читает входящие сообщения и распределяет по плагинам.
   * Пока пусто — будет заполнено в Phase 5 (Transport Refactoring).
   */
  void phase1_transport_input()
  {
    // TODO Phase 5: transport.read_input() → handle_input() для каждого плагина
  }

  /**
   * @brief Phase 2: Обновление акторов (FSM-переходы, поведения).
   * Пока пусто — будет заполнено в Phase 2 (Actor Foundation).
   */
  void phase2_actors()
  {
    // TODO Phase 2: ActorRegistry → actor.behavior.update(dt, world_query_)
  }

  /**
   * @brief Phase 3: Обработка агентов — ресурсы, resolve, актуация, кинематика, коллизии.
   *
   * Порядок для каждого агента:
   *  3a. pre_resolve() для всех плагинов (Resource modules)
   *  3d. agent.state.resolve() (вычисление effective constraints)
   *  3e. update() для ACTUATION/UTILITY/RESOURCE плагинов (НЕ SENSOR, НЕ INTERACTION)
   *  3f. Кинематика (pose += velocity * dt)
   *  3h. Collision detection
   *  3m. PendingTeleport
   *
   * ВАЖНО: SENSOR и INTERACTION плагины пропускаются в этой фазе.
   * Сенсоры вызываются в Phase 4 (строго после кинематики, D-19).
   * clear_contributions() НЕ вызывается здесь (D-20) — только в Phase 8.
   */
  void phase3_agents()
  {
    zone_system_.tick(world_.agents(), world_.actors(), bus_, sim_time_, dt_);

    for (auto& agent : world_.agents())
    {
      // 3a. Resource modules — плагины публикуют contributions до resolve()
      for (auto& plugin : agent.plugins)
          plugin->pre_resolve(dt_, agent);

      // 3d. RESOLVER — вычисляем effective constraints из contributions
      agent.state.resolve();

      // Собрать bounding-примитивы других агентов для RaycastEngine
      {
        std::vector<WorldPrimitive> agent_bounds;
        for (const auto& other : world_.agents()) {
          if (&other == &agent) continue;
          if (!other.has_collision) continue;
          WorldPrimitive wp;
          wp.pose = other.world_pose;
          if (other.bounding.type == ShapeType::SPHERE) {
            wp.type   = "sphere";
            wp.radius = other.bounding.radius;
          } else {
            wp.type = "box";
            wp.size = other.bounding.size * 2.0;
          }
          agent_bounds.push_back(wp);
        }
        raycast_engine_.set_dynamic_agents(agent_bounds);
      }

      // 3e. ACTUATION, UTILITY, RESOURCE плагины
      // SENSOR и INTERACTION пропускаются — они вызываются в Phase 4 и Phase 5.
      {
        KernelCommandQueue tick_cmds;
        PluginContext ctx{null_world_query_, bus_, tick_cmds};

        for (auto& plugin : agent.plugins)
        {
          auto r = plugin->role();
          if (r == PluginRole::SENSOR || r == PluginRole::INTERACTION)
            continue;  // Сенсоры — в Phase 4, Interaction — в Phase 5

          plugin->set_collision_system(&collision_system_);
          plugin->set_raycast_engine(&raycast_engine_);
          plugin->update(dt_, agent, ctx);
        }

        // Применить команды, выданные плагинами этой фазы
        for (auto& cmd : tick_cmds)
          std::visit([this](const auto& c) { apply_kernel_command(c); }, cmd);
      }

      // 3f. Кинематика — обновляем позу на основе скорости
      // world_velocity хранится в локальных координатах корпуса.
      {
        double local_vx = agent.world_velocity.linear.x();
        double local_vy = agent.world_velocity.linear.y();
        double wz = agent.world_velocity.angular.z();

        Eigen::Matrix3d R = CollisionSystem::rotation_from_pose(agent.world_pose);
        Vec3 body_vel{local_vx, local_vy, 0.0};
        Vec3 world_vel = R * body_vel;

        // Аддитивная скорость от зон (конвейер, ветер и т.п.)
        const Vec3& additive = agent.state.effective().velocity_addition;

        agent.world_pose.x += (world_vel.x() + additive.x()) * dt_;
        agent.world_pose.y += (world_vel.y() + additive.y()) * dt_;
        agent.world_pose.z += (agent.world_velocity.linear.z() + additive.z()) * dt_;
        agent.world_pose.yaw += wz * dt_;

        // Нормализация yaw в диапазон [0, 2π)
        agent.world_pose.yaw = std::fmod(agent.world_pose.yaw, 2.0 * 3.14159265358979323846);
        if (agent.world_pose.yaw < 0) {
          agent.world_pose.yaw += 2.0 * 3.14159265358979323846;
        }
      }

      // 3h. Collision detection
      if (agent.has_collision && agent.bounding.type == ShapeType::SPHERE)
      {
        Vec3 pos = agent.world_pose.position();
        double agent_bottom = pos.z() - agent.bounding.radius;

        auto contacts = collision_system_.check_sphere_all(
            pos, agent.bounding.radius);

        for (const auto& contact : contacts)
        {
          bool walkable = contact.contact_normal.z() >=
                          std::cos(agent.max_slope_rad);

          if (!walkable)
          {
            if (contact.obstacle_top_z - agent_bottom <=
                agent.max_step_height)
            {
              continue;
            }
          }

          if (walkable)
          {
            // Walkable поверхность: только Z push-out
            if (contact.contact_normal.z() > 1e-4)
              agent.world_pose.z += contact.penetration / contact.contact_normal.z();
            continue;
          }
          else
          {
            // Стена / крутой склон: горизонтальный slide и push-out
            Vec3 normal_h{contact.contact_normal.x(),
                          contact.contact_normal.y(), 0.0};
            double nlen = normal_h.norm();
            if (nlen > 1e-6)
            {
              normal_h /= nlen;
              double proj =
                  agent.world_velocity.linear.x() * normal_h.x() +
                  agent.world_velocity.linear.y() * normal_h.y();
              if (proj < 0.0)
              {
                agent.world_velocity.linear.x() -= normal_h.x() * proj;
                agent.world_velocity.linear.y() -= normal_h.y() * proj;
              }
              agent.world_pose.x +=
                  contact.contact_normal.x() * contact.penetration;
              agent.world_pose.y +=
                  contact.contact_normal.y() * contact.penetration;
            }
          }

          agent_bottom = agent.world_pose.z - agent.bounding.radius;
        }

        // Выравнивание ориентации агента по нормали опорной поверхности.
        // Используем find_support_surface() (надёжнее collision contacts при penetration ≈ 0).
        {
          auto support = collision_system_.find_support_surface(
              agent.world_pose.position(), agent.bounding.radius);
          bool surface_found = false;
          if (support) {
            const double surface_z = support->ground_z + agent.bounding.radius;
            if (agent.world_pose.z <= surface_z + 0.05) {
              const Vec3& n = support->normal;
              const double yaw = agent.world_pose.yaw;
              const double nx_body =  std::cos(yaw) * n.x() + std::sin(yaw) * n.y();
              const double ny_body = -std::sin(yaw) * n.x() + std::cos(yaw) * n.y();
              agent.world_pose.pitch = std::atan2( nx_body, n.z());
              agent.world_pose.roll  = std::atan2(-ny_body, n.z());
              surface_found = true;
            }
          }
          if (!surface_found) {
            agent.world_pose.roll  = 0.0;
            agent.world_pose.pitch = 0.0;
          }
        }
      }

      // 3m. Телепорт — применяем отложенные телепорты (устанавливаются TeleportEffect).
      // Выполняется после коллизий (3h) — телепорт не отменяется push-out.
      {
        auto* pt = agent.state.get<PendingTeleport>();
        if (pt && pt->pending) {
          agent.world_pose.x   = pt->destination.x();
          agent.world_pose.y   = pt->destination.y();
          agent.world_pose.z   = pt->destination.z();
          agent.world_pose.yaw = pt->yaw;
          agent.world_velocity.linear  = Vec3::Zero();
          agent.world_velocity.angular = Vec3::Zero();
          pt->pending = false;
        }
      }

      // ВАЖНО: clear_contributions() НЕ здесь (D-20) — только в Phase 8.
    }
  }

  /**
   * @brief Phase 4: Сенсорные плагины.
   *
   * ВАЖНО: строго после Phase 3 (кинематика + коллизии) — сенсоры видят ФИНАЛЬНУЮ позицию.
   * Lidar бросает лучи из финальной позиции агента.
   * GNSS/IMU читают финальную скорость.
   *
   * Вызывает update() только для плагинов с role() == PluginRole::SENSOR.
   */
  void phase4_sensors()
  {
    for (auto& agent : world_.agents())
    {
      KernelCommandQueue tick_cmds;
      PluginContext ctx{null_world_query_, bus_, tick_cmds};

      for (auto& plugin : agent.plugins)
      {
        if (plugin->role() != PluginRole::SENSOR)
          continue;

        plugin->set_collision_system(&collision_system_);
        plugin->set_raycast_engine(&raycast_engine_);
        plugin->update(dt_, agent, ctx);
      }

      for (auto& cmd : tick_cmds)
        std::visit([this](const auto& c) { apply_kernel_command(c); }, cmd);
    }
  }

  /**
   * @brief Phase 5: Interaction-плагины (Grabber, DoorOpener).
   *
   * После сенсоров: Grabber/DoorOpener читают актуальные данные сенсоров из SharedState.
   * Вызывает update() только для плагинов с role() == PluginRole::INTERACTION.
   */
  void phase5_interactions()
  {
    for (auto& agent : world_.agents())
    {
      KernelCommandQueue tick_cmds;
      PluginContext ctx{null_world_query_, bus_, tick_cmds};

      for (auto& plugin : agent.plugins)
      {
        if (plugin->role() != PluginRole::INTERACTION)
          continue;

        plugin->set_collision_system(&collision_system_);
        plugin->set_raycast_engine(&raycast_engine_);
        plugin->update(dt_, agent, ctx);
      }

      for (auto& cmd : tick_cmds)
        std::visit([this](const auto& c) { apply_kernel_command(c); }, cmd);
    }
  }

  /**
   * @brief Phase 6: Обновление attachments (позы привязанных объектов).
   * Пока пусто — будет заполнено в Phase 2 (Prop Foundation).
   */
  void phase6_attachments()
  {
    // TODO Phase 2: обновить позы props, привязанных к агентам/акторам
  }

  /**
   * @brief Phase 7: Публикация снапшота и данных транспорта.
   */
  void phase7_snapshot_publish()
  {
    viz_timer_ += dt_;
    double viz_interval = config_.viz_rate > 0 ? 1.0 / config_.viz_rate : 0.0;
    if (viz_server_ && viz_interval > 0 && viz_timer_ >= viz_interval) {
      viz_timer_ -= viz_interval;
      publish_viz();
    }

    transport_timer_ += dt_;
    double transport_interval = config_.transport_rate > 0
        ? 1.0 / config_.transport_rate : 0.0;
    if (post_tick_cb_ && transport_interval > 0
        && transport_timer_ >= transport_interval - 1e-9) {
      transport_timer_ -= transport_interval;
      post_tick_cb_(world_, sim_time_);
    }
  }

  /**
   * @brief Phase 8: Очистка состояния для следующего тика.
   *
   * ВАЖНО: clear_contributions() только здесь (D-20).
   * Contributions публикуются в Phase 3 (pre_resolve), разрешаются в Phase 3 (resolve),
   * читаются сенсорами в Phase 4 — и только потом очищаются.
   */
  void phase8_cleanup()
  {
    for (auto& agent : world_.agents())
      agent.state.clear_contributions();

    // TODO Phase 1: удалить зоны с истёкшим lifecycle
  }

  // ─── Обработчик KernelCommand ─────────────────────────────────────────────

  /**
   * @brief Применить одну команду ядра.
   *
   * Вызывается из phase0_kernel_commands() для каждой команды из очереди.
   * Также вызывается из phase3/4/5 для inline-команд плагинов.
   *
   * Безопасность: SetPose с неизвестным id молча игнорируется (T-00-11, нет panic).
   */
  template <typename T>
  void apply_kernel_command(const T& cmd)
  {
    if constexpr (std::is_same_v<T, cmd::SetPose>)
    {
      for (auto& agent : world_.agents())
        if (agent.id == cmd.id) { agent.world_pose = cmd.pose; break; }
    }
    else if constexpr (std::is_same_v<T, cmd::SetEnabled>)
    {
      // TODO Phase 6 (Entity Model): добавить поле enabled в EntityBase.
      // Пока заглушка — ядро получит enabled через EntityBase в Phase 6.
    }
    else if constexpr (std::is_same_v<T, cmd::SpawnEntity>)
    {
      // TODO Phase 0: базовый SpawnEntity через SceneLoader
      std::cout << "[SimEngine] SpawnEntity: " << cmd.entity_type << " (TODO)\n";
    }
    else if constexpr (std::is_same_v<T, cmd::DespawnEntity>)
    {
      // TODO Phase 6: удалить агента/актора/проп по id
      std::cout << "[SimEngine] DespawnEntity: " << cmd.id << " (TODO)\n";
    }
    else if constexpr (std::is_same_v<T, cmd::AddPlugin>)
    {
      // DEFERRED Phase 5 (TRAN-07): динамическое добавление плагина через REST Hot Patch API.
      (void)cmd;
    }
    else if constexpr (std::is_same_v<T, cmd::RemovePlugin>)
    {
      // DEFERRED Phase 5 (TRAN-07): динамическое удаление плагина через REST Hot Patch API.
      (void)cmd;
    }
    else if constexpr (std::is_same_v<T, cmd::ConfigPlugin>)
    {
      // DEFERRED Phase 5 (TRAN-07): обновление конфигурации плагина в рантайме.
      (void)cmd;
    }
    else
    {
      // Все остальные команды (ZoneCommands, Interact, Attach, Scene) — TODO в следующих фазах.
      // Не падаем — молча игнорируем неизвестные команды.
      (void)cmd;
    }
  }

  // ─── Вспомогательные методы ───────────────────────────────────────────────

  /**
   * @brief Опубликовать снапшот визуализатору (вызывается из tick).
   * Определён в s2_visualizer/src для доступа к полному типу VizServer.
   */
  void publish_viz();

  /**
   * @brief Сохранить начальные позы и скорости всех агентов.
   * Вызывается из load_world().
   */
  void save_initial_states()
  {
    initial_states_.clear();
    for (const auto& agent : world_.agents()) {
      initial_states_[agent.id] = AgentInitialState{
        agent.world_pose,
        agent.world_velocity
      };
    }
  }

  /**
   * @brief Восстановить начальные позы и скорости всех агентов.
   * Вызывается из reset().
   */
  void restore_initial_states()
  {
    for (auto& agent : world_.agents()) {
      auto it = initial_states_.find(agent.id);
      if (it != initial_states_.end()) {
        agent.world_pose = it->second.pose;
        agent.world_velocity = it->second.velocity;
      }
    }
  }

  /**
   * @brief Ключ плагина в карте данных/схем.
   * Если задан sensor_name — "type_name", иначе "type".
   */
  static std::string plugin_key(const plugins::IAgentPlugin& p)
  {
    return p.sensor_name().empty() ? p.type() : p.type() + "_" + p.sensor_name();
  }

  /**
   * @brief Собрать данные плагинов для снапшота.
   */
  std::map<std::string, std::map<std::string, std::string>> build_plugins_data() const
  {
    std::map<std::string, std::map<std::string, std::string>> result;
    for (const auto& agent : world_.agents()) {
      std::string agent_key = "agent_" + std::to_string(agent.id);
      for (const auto& plugin : agent.plugins) {
        result[agent_key][plugin_key(*plugin)] = plugin->to_json();
      }
    }
    return result;
  }

  /**
   * @brief Собрать схемы входных данных плагинов для снапшота.
   */
  std::map<std::string, std::string> build_plugin_inputs_schemas() const
  {
    std::map<std::string, std::string> result;
    for (const auto& agent : world_.agents()) {
      std::string agent_key = "agent_" + std::to_string(agent.id);
      nlohmann::json agent_schemas;
      for (const auto& plugin : agent.plugins) {
        if (plugin->has_inputs() && !plugin->inputs_schema().empty()) {
          nlohmann::json schema = nlohmann::json::parse(plugin->inputs_schema(), nullptr, false);
          if (!schema.is_discarded()) {
            agent_schemas[plugin_key(*plugin)] = schema;
          }
        }
      }
      if (!agent_schemas.empty()) {
        result[agent_key] = agent_schemas.dump();
      }
    }
    return result;
  }

  // ─── Поля ─────────────────────────────────────────────────────────────────

  Config   config_;
  SimWorld world_;
  SimBus   bus_;  ///< EventBus (using SimBus = EventBus в sim_bus.hpp)

  double sim_time_{0.0};
  double dt_{0.0};
  std::atomic<bool> running_{false};
  bool paused_{false};

  // Начальные состояния агентов для reset()
  struct AgentInitialState {
    Pose3D    pose;
    Velocity  velocity;
  };
  std::map<AgentId, AgentInitialState> initial_states_;

  CollisionSystem  collision_system_;  ///< Система коллизий
  RaycastEngine    raycast_engine_;    ///< Движок лучей
  ZoneSystem       zone_system_;       ///< Система зон и эффектов
  EffectFactory    effect_factory_;    ///< Фабрика плагинов эффектов

  // ─── Команды ядра (D-05) ──────────────────────────────────────────────────

  KernelCommandQueue command_queue_;        ///< Очередь команд (HTTP-тред + плагины)
  std::mutex         command_queue_mutex_;  ///< Защита от data race (HTTP vs sim thread)

  // ─── WorldQuery + PluginContext ────────────────────────────────────────────

  NullWorldQuery null_world_query_;  ///< Заглушка WorldQuery для PluginContext (Plan 05+)
  EventBus       plugin_bus_;        ///< Шина для плагинов (отдельная от bus_)

  VizServer* viz_server_ = nullptr;
  double viz_timer_{0.0};

  PostTickCallback post_tick_cb_;
  double transport_timer_{0.0};
};

} // namespace s2
