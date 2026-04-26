#pragma once
/**
 * @file world_query.hpp
 * WorldQuery — read-only API для плагинов и поведений.
 *
 * Позволяет плагинам запрашивать состояние мира без прямого доступа к SimWorld.
 * Передаётся через PluginContext в update() каждого плагина.
 *
 * Архитектурный принцип: никаких записей через WorldQuery. Только чтение.
 * Изменения мира — только через KernelCommand.
 *
 * Реализация: WorldQueryImpl в sim_engine.cpp (Plan 05).
 * В Phase 0 — заглушки (возвращают пустые результаты).
 */

#include <s2/types.hpp>

#include <optional>
#include <string>
#include <vector>

namespace s2
{

// ============================================================================
// Вспомогательные типы для WorldQuery
// ============================================================================

/**
 * @brief Фильтр Entity для поиска.
 * Позволяет ограничивать поиск по типу, тегам, capabilities.
 */
struct EntityFilter
{
  bool include_agents{true};                       ///< Включать ли агентов
  bool include_actors{true};                       ///< Включать ли акторов
  bool include_props{true};                        ///< Включать ли пропы
  std::vector<std::string> required_capabilities;  ///< Все перечисленные должны присутствовать
  std::vector<std::string> required_tags;          ///< Все теги должны присутствовать
  EntityId exclude_self{0};  ///< Исключить эту Entity из результата (0 = не исключать)

  /// Фабричные методы для удобства
  static EntityFilter agents_only()
  {
    return EntityFilter{.include_agents = true, .include_actors = false, .include_props = false};
  }

  static EntityFilter actors_only()
  {
    return EntityFilter{.include_agents = false, .include_actors = true, .include_props = false};
  }

  static EntityFilter all() { return EntityFilter{}; }
};

/**
 * @brief Объём детекции для поиска сигналов.
 */
struct DetectionVolume
{
  enum class Shape { SPHERE, CONE, BOX };

  Shape  shape{Shape::SPHERE};
  double range{10.0};          ///< Дальность (для SPHERE и CONE)
  double fov_rad{1.5708};      ///< Угол конуса (для CONE, ~90°)
  Vec3   direction{1, 0, 0};   ///< Направление конуса в мировых координатах
};

/**
 * @brief AABB-бокс для поиска.
 */
struct Box
{
  Vec3 center;
  Vec3 half_extents{1.0, 1.0, 1.0};
};

/**
 * @brief Результат raycast из WorldQuery.
 * hit == true означает, что луч нашёл пересечение.
 *
 * Отличается от RaycastResult (raycast_engine.hpp) добавлением entity_id.
 * RaycastEngine — низкоуровневый геометрический движок.
 * RaycastQueryResult — ответ высокоуровневого WorldQuery с Entity-контекстом.
 */
struct RaycastQueryResult
{
  bool     hit{false};
  double   distance{0.0};   ///< Расстояние от origin до точки попадания
  Vec3     point;            ///< Точка пересечения в мировых координатах
  Vec3     normal;           ///< Нормаль поверхности в точке пересечения
  EntityId entity_id{0};    ///< Entity, с которой произошло пересечение (0 = геометрия)
};

// ============================================================================
// WorldQuery — read-only API
// ============================================================================

/**
 * @brief Read-only API ядра для плагинов и поведений.
 *
 * Предоставляется плагинам через PluginContext::world.
 * Базовый класс: заглушки. Переопределяется WorldQueryImpl в SimEngine (Plan 05).
 *
 * Использование:
 * @code
 * void MyPlugin::update(double dt, Agent& agent, const PluginContext& ctx) {
 *     auto nearby = ctx.world.find_in_radius(agent.world_pose.position(), 5.0,
 *                                            EntityFilter::agents_only());
 * }
 * @endcode
 */
class WorldQuery
{
public:
  virtual ~WorldQuery() = default;

  // ─── Поиск Entity ──────────────────────────────────────────────────────────

  /**
   * @brief Найти все Entity в радиусе от точки.
   * @param center  Центр поиска (мировые координаты)
   * @param r       Радиус поиска (метры)
   * @param filter  Фильтр по типу/тегам/capabilities
   * @return Список EntityId в радиусе (не отсортирован)
   */
  virtual std::vector<EntityId> find_in_radius(Vec3 center, double r,
                                               EntityFilter filter) const
  {
    (void)center;
    (void)r;
    (void)filter;
    return {};
  }

  /**
   * @brief Найти все Entity в AABB-боксе.
   */
  virtual std::vector<EntityId> find_in_box(Box b, EntityFilter filter) const
  {
    (void)b;
    (void)filter;
    return {};
  }

  /**
   * @brief Найти ближайшую Entity к точке.
   * @return EntityId или nullopt если ничего не найдено
   */
  virtual std::optional<EntityId> find_nearest(Vec3 pos, EntityFilter filter) const
  {
    (void)pos;
    (void)filter;
    return std::nullopt;
  }

  /**
   * @brief Найти Entity непосредственно под точкой (вертикальный raycast вниз).
   * @return EntityId или 0 если ничего не найдено
   */
  virtual EntityId find_entity_below(Vec3 position) const
  {
    (void)position;
    return 0;
  }

  // ─── Сигналы ───────────────────────────────────────────────────────────────

  /**
   * @brief Найти все сигналы заданного типа в объёме детекции.
   * @param signal_type  Тип сигнала: "aruco", "wire", "rfid", ...
   * @param pos          Позиция детектора (мировые координаты)
   * @param volume       Параметры объёма детекции
   * @return Список сигналов (с учётом requires_los если DetectionVolume поддерживает)
   */
  virtual std::vector<Signal> find_signals_of_type(const std::string& signal_type,
                                                   Vec3 pos,
                                                   const DetectionVolume& volume) const
  {
    (void)signal_type;
    (void)pos;
    (void)volume;
    return {};
  }

  // ─── Геометрия ─────────────────────────────────────────────────────────────

  /**
   * @brief Проверить наличие прямой видимости между двумя Entity.
   * Луч из центра from к центру to. Возвращает true если нет препятствий.
   */
  virtual bool has_line_of_sight(EntityId from, EntityId to) const
  {
    (void)from;
    (void)to;
    return true;  // Заглушка: всегда видимость есть
  }

  /**
   * @brief Бросить луч из точки в направлении.
   * @param origin     Начало луча (мировые координаты)
   * @param dir        Направление луча (единичный вектор)
   * @param max_range  Максимальная дальность
   * @return Результат: hit=false если ничего не нашёл
   */
  virtual RaycastQueryResult raycast(Vec3 origin, Vec3 dir, double max_range) const
  {
    (void)origin;
    (void)dir;
    (void)max_range;
    return RaycastQueryResult{};
  }

  // ─── Зоны ──────────────────────────────────────────────────────────────────

  /**
   * @brief Получить список зон, содержащих точку.
   * @param position  Точка в мировых координатах
   * @return Список ZoneId зон, которые содержат position
   */
  virtual std::vector<ZoneId> zones_at(Vec3 position) const
  {
    (void)position;
    return {};
  }

  /**
   * @brief Проверить, находится ли Entity в конкретной зоне.
   */
  virtual bool is_in_zone(EntityId entity_id, const ZoneId& zone_id) const
  {
    (void)entity_id;
    (void)zone_id;
    return false;
  }

  // ─── Пропы ─────────────────────────────────────────────────────────────────

  /**
   * @brief Найти ближайший movable Prop в радиусе от точки.
   *
   * Используется GrabberPlugin для поиска захватываемого объекта.
   * Возвращает ObjectId ближайшего movable пропа или nullopt.
   *
   * Заглушка — реализация в Phase 5 (WorldQueryImpl).
   *
   * @param pos    Центр поиска (мировые координаты)
   * @param radius Радиус поиска (метры)
   * @return ObjectId или nullopt если ничего не найдено
   */
  virtual std::optional<ObjectId> find_nearest_movable_prop(Vec3 pos, double radius) const
  {
    (void)pos;
    (void)radius;
    return std::nullopt;
  }

  // ─── Деформируемые объекты ─────────────────────────────────────────────────

  /**
   * @brief Найти деформируемую Entity в боксе.
   * Используется MaterialTransfer / DeformEntity KernelCommand.
   */
  virtual std::optional<EntityId> find_deformable_in_box(Box b,
                                                         EntityFilter filter) const
  {
    (void)b;
    (void)filter;
    return std::nullopt;
  }
};

}  // namespace s2
