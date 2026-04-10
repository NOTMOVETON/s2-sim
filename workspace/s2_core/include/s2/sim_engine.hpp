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

#include <s2/sim_bus.hpp>
#include <s2/world.hpp>
#include <s2/world_snapshot.hpp>
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
          if (plugin->type() == plugin_type) {
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
      as.visual = agent.visual;
      as.battery_level = 100.0;
      as.effective_speed_scale = 1.0;
      as.motion_locked = false;

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

    // Геометрия
    for (const auto& prim : world_.static_geometry()) {
      GeometrySnapshot gs;
      gs.type = prim.type;
      gs.x = prim.pose.position().x();
      gs.y = prim.pose.position().y();
      gs.z = prim.pose.position().z();
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

    // === Фаза 2: Зоны (проверка входов/выходов) ===
    // Пока пусто — будет в задаче 03

    // === Фаза 3: Для каждого агента ===
    for (auto& agent : world_.agents())
    {
      // 3a. Resource modules — пока пусто
      // 3b. Own effects CONTINUOUS — пока пусто
      // 3c. Zone effects CONTINUOUS — пока пусто

      // 3d. RESOLVER — вычисляем effective constraints из contributions
      agent.state.resolve();

      // 3e. плагины (DiffDrive, GNSS, IMU, Lidar и т.д.)
      // вызываются до кинематики, чтобы они могли установить velocity
      for (auto& plugin : agent.plugins)
      {
          plugin->update(dt_, agent);
      }

      // 3f. Kinematics — обновляем позу на основе скорости
      // world_velocity хранится в локальных координатах корпуса.
      // Для дифф драйва: linear.x = скорость вперёд, linear.y = боковая
      // Преобразуем в мировые координаты с учётом ориентации (yaw)
      double yaw = agent.world_pose.yaw;
      double local_vx = agent.world_velocity.linear.x();
      double local_vy = agent.world_velocity.linear.y();
      double wx = agent.world_velocity.angular.x();
      double wy = agent.world_velocity.angular.y();
      double wz = agent.world_velocity.angular.z();
      // Вращение вокруг Z: 2D-матрица поворота
      double cos_yaw = std::cos(yaw);
      double sin_yaw = std::sin(yaw);
      double vx_world = local_vx * cos_yaw - local_vy * sin_yaw;
      double vy_world = local_vx * sin_yaw + local_vy * cos_yaw;
      agent.world_pose.x += vx_world * dt_;
      agent.world_pose.y += vy_world * dt_;
      agent.world_pose.z += agent.world_velocity.linear.z() * dt_;
      agent.world_pose.yaw += wz * dt_;

      // Нормализация yaw в диапазон [0, 2π)
      agent.world_pose.yaw = std::fmod(agent.world_pose.yaw, 2.0 * 3.14159265358979323846);
      if (agent.world_pose.yaw < 0) {
        agent.world_pose.yaw += 2.0 * 3.14159265358979323846;
      }

      // 3g. Surface snap — пока пусто
      // 3h. Collision detection — пока пусто
      // 3i. Joints — пока пусто
      // 3j. Kinematic tree update — пока пусто
      // 3k. Sensors — пока пусто
      // 3l. Interactions — пока пусто

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
   * @brief Собрать данные плагинов для снапшота.
   * Формат: agent_id -> { plugin_type -> json_string }
   */
  std::map<std::string, std::map<std::string, std::string>> build_plugins_data() const
  {
    std::map<std::string, std::map<std::string, std::string>> result;
    for (const auto& agent : world_.agents()) {
      std::string agent_key = "agent_" + std::to_string(agent.id);
      for (const auto& plugin : agent.plugins) {
        result[agent_key][plugin->type()] = plugin->to_json();
      }
    }
    return result;
  }

  /**
   * @brief Собрать схемы входных данных плагинов для снапшота.
   * Формат: agent_id -> JSON-string { plugin_type -> schema }
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
            agent_schemas[plugin->type()] = schema;
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
