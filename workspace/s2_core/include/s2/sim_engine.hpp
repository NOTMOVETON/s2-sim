#pragma once

/**
 * @file sim_engine.hpp
 * SimEngine — главный цикл симуляции.
 *
 * Управляет тиковой петлёй с фиксированным шагом dt.
 * Каждый тик:
 *  1. Обновляет симуляционное время (sim_time += dt)
 *  2. Проходит по фазам: акторы, зоны, агенты, attachments
 *  3. Для каждого агента: resolver → actuation → kinematics → clear_contributions
 *
 * Пока большинство фаз пустые — будут заполняться в следующих задачах.
 */

#include <s2/collision_system.hpp>
#include <s2/components/pending_teleport.hpp>
#include <s2/raycast_engine.hpp>
#include <s2/sim_bus.hpp>
#include <s2/world.hpp>
#include <s2/world_snapshot.hpp>
#include <s2/zone_system.hpp>
#include <nlohmann/json.hpp>

#include <thread>
#include <atomic>
#include <chrono>
#include <map>
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
 *  - Вызов resolve() и clear_contributions() для каждого агента
 *  - Синхронизацию через SimBus
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
   *
   * Единая точка входа для любых транспортов (VizUI, ROS2, MQTT).
   * Транспорт конвертирует свои сообщения в JSON и вызывает этот метод.
   */
  bool handle_plugin_input(AgentId agent_id, const std::string& plugin_type, const std::string& json_input)
  {
    for (auto& agent : world_.agents()) {
      if (agent.id == agent_id) {
        for (auto& plugin : agent.plugins) {
          // Матчим по типу ("lidar") ИЛИ по полному ключу ("lidar_front_lidar")
          if (plugin->type() == plugin_type || plugin_key(*plugin) == plugin_type) {
            plugin->handle_input(json_input);
            return true;
          }
        }
        // Агент найден, но плагин не найден
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

      // Плагины добавляют свои доменные поля (battery_level, held_objects и т.п.)
      for (const auto& plugin : agent.plugins)
        plugin->contribute_snapshot(as.extra, agent);

      // Состояние шин из SharedState (заполняется TirePunctureEffect при входе в зону)
      const auto* tire = agent.state.get<TirePunctureData>();
      if (tire) as.tire_punctured = tire->punctured;

      // Коллизионный шейп для визуализации
      as.has_collision = agent.has_collision;
      if (agent.has_collision) {
        if (agent.bounding.type == ShapeType::SPHERE) {
          as.bounding_type   = "sphere";
          as.bounding_radius = agent.bounding.radius;
        } else {
          as.bounding_type = "box";
          as.bounding_size = agent.bounding.size * 2.0;  // half-extents → full extents
        }
      }

      // Позы всех звеньев кинематического дерева (включая корень)
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
  /**
   * @brief Один тик симуляции.
   *
   * Порядок фаз (как определено в архитектуре):
   *  1. Акторы (FSM transitions)
   *  2. Зоны (проверка входов/выходов)
   *  3. Для каждого агента:
   *     - Resource modules
   *     - Own effects (CONTINUOUS)
   *     - Zone effects (CONTINUOUS)
   *     - RESOLVER (вычисление effective constraints)
   *     - Actuation
   *     - Kinematics
   *     - Surface snap
   *     - Collision detection
   *     - Joints
   *     - Kinematic tree update
   *     - Sensors
   *     - Interactions
   *     - Clear contributions
   *  4. Attachments
   *  5. Snapshot
   *  6. Viz publish
   *
   * Пока большинство фаз пустые — заглушки для будущих задач.
   */
  void tick()
  {
    // Если на паузе — не обновляем время и не двигаем агентов
    // Но всё равно отправляем снапшоты (визуализатор должен видеть paused)
    if (paused_) {
      viz_timer_ += dt_;
      double viz_interval = config_.viz_rate > 0 ? 1.0 / config_.viz_rate : 0.0;
      if (viz_server_ && viz_interval > 0 && viz_timer_ >= viz_interval) {
        viz_timer_ -= viz_interval;
        publish_viz();
      }
      return;
    }

    // Обновляем симуляционное время
    sim_time_ += dt_;

    // === Фаза 1: Акторы (FSM transitions) ===
    // Пока пусто — будет в задаче 07

    // === Фаза 2: Зоны (проверка входов/выходов и применение эффектов) ===
    zone_system_.tick(world_.agents(), world_.actors(), bus_, sim_time_, dt_);

    // === Фаза 3: Для каждого агента ===
    for (auto& agent : world_.agents())
    {
      // 3a. Resource modules — плагины публикуют contributions до resolve().
      // Плагины переопределяют pre_resolve() если им нужна ранняя фаза.
      // Пример: BatteryPlugin разряжает батарею и добавляет add_scale/add_lock.
      for (auto& plugin : agent.plugins)
          plugin->pre_resolve(dt_, agent);

      // 3b. Own effects CONTINUOUS — пока пусто
      // 3c. Zone effects CONTINUOUS — пока пусто

      // 3d. RESOLVER — вычисляем effective constraints из contributions
      agent.state.resolve();

      // 3e. плагины (DiffDrive, GNSS, IMU, Lidar и т.д.)
      // вызываются до кинематики, чтобы они могли установить velocity.
      // Перед update() передаём CollisionSystem и RaycastEngine;
      // плагины, которым они не нужны, игнорируют вызовы.

      // Собрать bounding-примитивы всех других агентов для LidarPlugin
      {
        std::vector<WorldPrimitive> agent_bounds;
        for (const auto& other : world_.agents()) {
          if (&other == &agent) continue;  // исключить владельца лидара
          if (!other.has_collision) continue;
          WorldPrimitive wp;
          wp.pose = other.world_pose;
          if (other.bounding.type == ShapeType::SPHERE) {
            wp.type   = "sphere";
            wp.radius = other.bounding.radius;
          } else {
            wp.type = "box";
            wp.size = other.bounding.size * 2.0;  // half-extents → full extents
          }
          agent_bounds.push_back(wp);
        }
        raycast_engine_.set_dynamic_agents(agent_bounds);
      }

      for (auto& plugin : agent.plugins)
      {
          plugin->set_collision_system(&collision_system_);
          plugin->set_raycast_engine(&raycast_engine_);
          plugin->update(dt_, agent);
      }

      // 3f. Kinematics — обновляем позу на основе скорости
      // world_velocity хранится в локальных координатах корпуса.
      // Для дифф драйва: linear.x = скорость вперёд, linear.y = боковая
      // Преобразуем body velocity в мировые координаты с учётом полной
      // ориентации (yaw + pitch + roll). На плоском полу (pitch=roll=0)
      // результат идентичен прежней yaw-only ротации.
      double local_vx = agent.world_velocity.linear.x();
      double local_vy = agent.world_velocity.linear.y();
      double wz = agent.world_velocity.angular.z();

      Eigen::Matrix3d R = CollisionSystem::rotation_from_pose(agent.world_pose);
      Vec3 body_vel{local_vx, local_vy, 0.0};
      Vec3 world_vel = R * body_vel;

      // Применяем аддитивную скорость от зон (конвейер, ветер и т.п.).
      // Задаётся в мировых координатах и суммируется поверх actuation.
      const Vec3& additive = agent.state.effective().velocity_addition;

      agent.world_pose.x += (world_vel.x() + additive.x()) * dt_;
      agent.world_pose.y += (world_vel.y() + additive.y()) * dt_;
      // Z управляется GravityPlugin (позиционный контроль), не кинематикой;
      // additive.z() добавляется для воздушных зон
      agent.world_pose.z += (agent.world_velocity.linear.z() + additive.z()) * dt_;
      agent.world_pose.yaw += wz * dt_;

      // Нормализация yaw в диапазон [0, 2π)
      agent.world_pose.yaw = std::fmod(agent.world_pose.yaw, 2.0 * 3.14159265358979323846);
      if (agent.world_pose.yaw < 0) {
        agent.world_pose.yaw += 2.0 * 3.14159265358979323846;
      }

      // 3g. Surface snap — пока пусто

      // 3h. Collision detection
      if (agent.has_collision && agent.bounding.type == ShapeType::SPHERE)
      {
        Vec3 pos = agent.world_pose.position();
        double agent_bottom = pos.z() - agent.bounding.radius;

        // Собираем все контакты, сортированные по убыванию penetration
        auto contacts = collision_system_.check_sphere_all(
            pos, agent.bounding.radius);

        for (const auto& contact : contacts)
        {
          bool walkable = contact.contact_normal.z() >=
                          std::cos(agent.max_slope_rad);

          if (!walkable)
          {
            // Проверяем порог ступеньки: если верхняя грань примитива
            // не выше нижней точки агента более чем на max_step_height,
            // агент просто переезжает (игнорируем коллизию).
            if (contact.obstacle_top_z - agent_bottom <=
                agent.max_step_height)
            {
              continue;
            }
          }

          if (walkable)
          {
            // Walkable поверхность (пандус/пол): только Z push-out, XY — нет.
            // XY push-out мешает заезду на рампу (отталкивает назад вдоль склона).
            //
            // Правильная формула Z-только push-out:
            //   Чтобы при фиксированных X,Y вывести сферу ровно на поверхность
            //   нужно дельта_z = penetration / nz, а не nz * penetration.
            //
            //   Доказательство: плоскость n·x = D, центр сферы (x, y, z).
            //   Текущая дистанция = r - p (проникновение p).
            //   Хотим дистанцию = r при том же X,Y:
            //     nz * z' = D + r - nx*x - ny*y  =>  delta_z = p / nz
            //
            //   При nz = 1 (плоский пол): p / 1 = p  (совпадает с nz*p).
            //   При nz = 0.95 (рампа 18°): p / 0.95 > p * 0.95 — полное снятие.
            //
            // Без этого сфера оставалась внутри рампы на p*sin²(θ) каждый тик,
            // создавая осцилляцию с GravityPlugin.
            if (contact.contact_normal.z() > 1e-4)
              agent.world_pose.z += contact.penetration / contact.contact_normal.z();
            continue;
          }
          else
          {
            // Стена / крутой склон: только горизонтальный slide и push-out
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
              // Z не изменяем
            }
          }

          // Обновляем agent_bottom после push-out для следующего контакта
          agent_bottom = agent.world_pose.z - agent.bounding.radius;
        }

        // Выравнивание ориентации агента по нормали опорной поверхности (задача 20.1).
        //
        // ВАЖНО: нельзя полагаться на collision contacts для получения нормали.
        // GravityPlugin снапит z точно на поверхность (penetration ≈ 0), поэтому
        // check_sphere_all() часто возвращает пустой список — и нормаль не попадает
        // в контакты. Это вызывало фликер pitch/roll между нормалью рампы и (0,0).
        //
        // Решение: использовать find_support_surface(), который надёжно находит
        // нормаль через down-raycast без зависимости от проникновения.
        {
            auto support = collision_system_.find_support_surface(
                agent.world_pose.position(), agent.bounding.radius);
            bool surface_found = false;
            if (support) {
                // Считаем агента «стоящим», если он в пределах небольшого допуска
                // над поверхностью (0.05 м — чуть больше grounded_epsilon гравитации).
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

      // 3i. Joints — пока пусто
      // 3j. Kinematic tree update — пока пусто
      // 3k. Sensors — пока пусто
      // 3l. Interactions — пока пусто

      // 3m. Телепорт — применяем отложенные телепорты (устанавливаются TeleportEffect).
      // Выполняется после коллизий (3h), чтобы телепорт не отменялся push-out.
      // Сброс скорости гарантирует, что агент не улетит на первом тике после телепорта.
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

      // Очищаем contributions для следующего тика
      agent.state.clear_contributions();
    }

    // === Фаза 4: Attachments ===
    // Пока пусто

    // === Фаза 5: Snapshot + Viz publish ===
    viz_timer_ += dt_;
    double viz_interval = config_.viz_rate > 0 ? 1.0 / config_.viz_rate : 0.0;
    if (viz_server_ && viz_interval > 0 && viz_timer_ >= viz_interval) {
      viz_timer_ -= viz_interval;
      publish_viz();
    }

    // === Фаза 6: Transport publish ===
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
   * @brief Опубликовать снапшот визуализатору (вызывается из tick).
   *   Определён в .cpp файле s2_visualizer для доступа к полному типу VizServer.
   */
  void publish_viz();

  Config config_;
  SimWorld world_;
  SimBus bus_;

  double sim_time_{0.0};
  double dt_{0.0};
  std::atomic<bool> running_{false};
  bool paused_{false};

  // Начальные состояния агентов для reset()
  struct AgentInitialState {
    Pose3D pose;
    Velocity velocity;
  };
  std::map<AgentId, AgentInitialState> initial_states_;

  CollisionSystem       collision_system_;  ///< Система коллизий (инициализируется при load_world)
  RaycastEngine         raycast_engine_;   ///< Движок лучей (инициализируется при load_world)
  ZoneSystem            zone_system_;      ///< Система зон и эффектов (инициализируется при load_world)
  EffectFactory             effect_factory_;  ///< Фабрика плагинов эффектов (задаётся до load_world)

  VizServer* viz_server_ = nullptr;
  double viz_timer_{0.0};

  PostTickCallback post_tick_cb_;
  double transport_timer_{0.0};

private:
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
   * Позволяет иметь несколько плагинов одного типа с разными именами.
   */
  static std::string plugin_key(const plugins::IAgentPlugin& p)
  {
    return p.sensor_name().empty() ? p.type() : p.type() + "_" + p.sensor_name();
  }

  /**
   * @brief Собрать данные плагинов для снапшота.
   * Формат: agent_id -> { plugin_key -> json_string }
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
   * Формат: agent_id -> JSON-string { plugin_key -> schema }
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
};

} // namespace s2
