#pragma once

/**
 * @file shared_state.hpp
 * Shared State — устойчивое состояние на агенте с системой contributions и resolver-ом.
 *
 * Архитектурная мотивация:
 *  - Модули не знают друг о друге (Battery не знает про DiffDrive)
 *  - Несколько источников вносят вклады в одно поле (battery, зона, payload — все влияют на скорость)
 *  - Resolver вычисляет итоговое значение, потребляемое actuation-модулем
 *
 * Три класса состояния:
 *  1. Single-owner — одно поле, один писатель (battery_level, held_objects)
 *  2. Contributions — много авторов, один итог (speed_scale, motion_locked)
 *  3. Effective/Resolved — вычисленное значение из contributions
 *
 * Порядок работы:
 *  1. Каждый тик модули публикуют contributions
 *  2. Resolver вычисляет effective values
 *  3. Actuation читает effective values и ограничивает скорость
 *  4. В начале следующего тика contributions очищаются
 */

#include <s2/types.hpp>

#include <any>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <cmath>
#include <functional>

namespace s2
{

// ============================================================================
// Contribution types
// ============================================================================

/**
 * @brief Вклад в масштаб скорости (multiplicative).
 *
 * Источник (Resource модуль, зональный эффект) публикует свой scale.
 * Resolver перемножает все вклады:
 *   effective = product(all scale values)
 *
 * Пример:  battery=0.85 × ice_zone=0.20 × boost=1.15 = 0.1955
 *
 * Значение по умолчанию = 1.0 (без изменений скорости).
 * source — имя источника, сохраняется для отладки и визуализации.
 */
struct ScaleContribution
{
  double value{1.0};       ///< Множитель скорости (1.0 = нормально)
  std::string source;      ///< "battery", "ice_zone_3", "payload"
};

/**
 * @brief Вклад в аддитивную скорость (конвейер, ветер).
 *
 * Суммируется со всеми другими аддитивными вкладами:
 *   effective = sum(all additive values)
 *
 * Пример: конвейер (1.0, 0, 0) + ветер (0, 0.5, 0) = (1.0, 0.5, 0)
 */
struct AdditiveContribution
{
  Vec3 value{Vec3::Zero()}; ///< Добавляемая скорость к итоговой
  std::string source;       ///< "conveyor_1", "wind_zone"
};

/**
 * @brief Вклад в блокировку движения (e-stop, battery critical).
 *
 * Если хотя бы один источник говорит locked=true — движение запрещено.
 * Resolver: effective = OR(all lock contributions).
 */
struct LockContribution
{
  bool locked{false};      ///< true = движение запрещено
  std::string source;      ///< "estop", "battery_critical"
};

// ============================================================================
// Resolved effective state
// ============================================================================

/**
 * @brief Итоговые ограничения, вычисленные Resolver-ом из contributions.
 *
 * Это то, что читает Actuation модуль. Он не знает, откуда пришли
 * ограничения — от батареи, от зоны, от e-stop. Он видит только итог.
 *
 * Правила вычисления:
 *  - speed_scale         = clamp(product(scales), 0.0, max)
 *  - motion_locked       = OR(any lock == true)
 *  - velocity_addition   = sum(all additive values)
 *
 * Поля *_sources сохраняют все источники для отладки и визуализатора.
 * Оператор может увидеть «почему робот стоит» — список всех contributions.
 *
 * Максимальный scale ограничен 10.0 для защиты от runaway contributions.
 */
struct EffectiveConstraints
{
  double speed_scale{1.0};               ///< Итоговый множитель скорости
  bool motion_locked{false};              ///< Заблокировано ли движение
  Vec3 velocity_addition{Vec3::Zero()};   ///< Сумма аддитивных скоростей

  ///< Источники scale contributions (для отладки/визуализации)
  std::vector<ScaleContribution> scale_sources;
  ///< Источники lock contributions (для отладки/визуализации)
  std::vector<LockContribution> lock_sources;
  ///< Источники additive contributions (для отладки/визуализации)
  std::vector<AdditiveContribution> additive_sources;
};

// ============================================================================
// SharedState
// ============================================================================

/**
 * @brief Разделяемое состояние агента.
 *
 * Централизованный хранитель данных агента, к которому обращаются модули.
 * Два механизма доступа:
 *
 * 1. Single-owner state (type-indexed storage через std::any):
 *    - Модуль регистрирует свой компонент при инициализации
 *    - Модуль читает/пишет свой компонент напрямую
 *    - Другие модули не имеют прямого доступа
 *    - Пример: BatteryResource хранит battery_level, Grabber — held_objects
 *
 * 2. Contribution state:
 *    - Множество модулей публикуют вклады
 *    - Resolver вычисляет итог
 *    - Actuation читает итог, не зная об источниках
 *
 * Жизненный цикл тика:
 *  1. Модули публикуют contributions → add_scale, add_lock, add_velocity_addition
 *  2. resolve() вычисляет effective constraints
 *  3. Actuation читает effective()
 *  4. clear_contributions() перед следующим тиком
 *
 * Потокобезопасность:
 *  - НЕ является потокобезопасным
 *  - Вызывается только в симуляционном потоке во время тика
 *  - Транспортный поток работает через тройной буфер (snapshot), а не напрямую
 *
 * Пример использования:
 * @code
 * shared_state.emplace<BatteryComponent>(100.0);
 * shared_state.add_scale(0.85, "battery");
 * shared_state.add_scale(0.20, "ice_zone");
 * shared_state.add_lock(true, "estop");
 * shared_state.resolve();
 *
 * const auto& eff = shared_state.effective();
 * // eff.speed_scale ≈ 0.17  (0.85 × 0.20)
 * // eff.motion_locked = true
 * @endcode
 */
class SharedState
{
public:
  SharedState() = default;

  // --- Single-owner state (type-safe storage через std::any) ---

  /**
   * @brief Создать поле в storage (если его ещё нет).
   *
   * Модуль вызывает при инициализации. Тип T определяет «владельца» поля.
   * Если поле уже существует — перезаписывает.
   *
   * @tparam T Тип компонента (определяет владельца)
   * @tparam Args Аргументы конструктора
   * @return Ссылка на созданный/обновлённый объект
   */
  template <typename T, typename... Args>
  T& emplace(Args&&... args)
  {
    auto& storage = fields_[typeid(T)];
    storage.emplace<T>(std::forward<Args>(args)...);
    return std::any_cast<T&>(storage);
  }

  /**
   * @brief Получить изменяемую ссылку на поле по типу.
   * @tparam T Тип компонента
   * @return Указатель на объект или nullptr если не найден
   */
  template <typename T>
  T* get()
  {
    auto it = fields_.find(typeid(T));
    if (it == fields_.end())
      return nullptr;
    return std::any_cast<T>(&it->second);
  }

  /**
   * @brief Получить константную ссылку на поле по типу.
   * @tparam T Тип компонента
   * @return Указатель на объект или nullptr если не найден
   */
  template <typename T>
  const T* get() const
  {
    auto it = fields_.find(typeid(T));
    if (it == fields_.end())
      return nullptr;
    return std::any_cast<T>(&it->second);
  }

  /**
   * @brief Проверить наличие поля в storage.
   * @tparam T Тип компонента
   * @return true если поле существует
   */
  template <typename T>
  bool has() const
  {
    return fields_.find(typeid(T)) != fields_.end();
  }

  // --- Contributions ---

  /**
   * @brief Опубликовать вклад в масштаб скорости.
   * @param value Множитель (1.0 = нормально, 0.5 = замедление ×2)
   * @param source Имя источника ("battery", "ice_zone_3")
   */
  void add_scale(double value, const std::string& source)
  {
    scale_contribs_.push_back({value, source});
  }

  /**
   * @brief Опубликовать вклад в аддитивную скорость (конвейер, ветер).
   * @param value Вектор добавляемой скорости
   * @param source Имя источника ("conveyor_1", "wind_zone")
   */
  void add_velocity_addition(const Vec3& value, const std::string& source)
  {
    additive_contribs_.push_back({value, source});
  }

  /**
   * @brief Опубликовать вклад в блокировку движения (e-stop, fault).
   * @param locked true = запретить движение
   * @param source Имя источника ("estop", "battery_critical")
   */
  void add_lock(bool locked, const std::string& source)
  {
    lock_contribs_.push_back({locked, source});
  }

  /**
   * @brief Resolver: вычислить effective constraints из всех contributions.
   *
   * Правила:
   *  - speed_scale = clamp(product(scales), 0.0, 10.0)
   *    Перемножаем все scale contributions, ограничиваем снизу нулём,
   *    сверху — 10.0 (защита от runaway contributions).
   *    Пустой список = 1.0 (без ограничений).
   *
   *  - motion_locked = OR(всех locks)
   *    Любой источник с locked=true блокирует движение.
   *    Пустой список = false (движение разрешено).
   *
   *  - velocity_addition = sum(всех additive)
   *    Складываем все аддитивные векторы.
   *    Пустой список = Vec3::Zero().
   */
  void resolve()
  {
    // Scale: произведение всех значений
    double scale = 1.0;
    for (const auto& c : scale_contribs_)
      scale *= c.value;
    effective_.speed_scale = std::clamp(scale, 0.0, 10.0);

    // Lock: OR всех contributions
    bool locked = false;
    for (const auto& c : lock_contribs_)
      locked = locked || c.locked;
    effective_.motion_locked = locked;

    // Additive: сумма всех векторов
    Vec3 additive = Vec3::Zero();
    for (const auto& c : additive_contribs_)
      additive += c.value;
    effective_.velocity_addition = additive;

    // Сохраняем источники для отладки/визуализации
    effective_.scale_sources = scale_contribs_;
    effective_.lock_sources = lock_contribs_;
    effective_.additive_sources = additive_contribs_;
  }

  /**
   * @brief Очистить все contributions перед новым тиком.
   *
   * Effective values сбрасываются к значениям по умолчанию.
   * Single-owner state НЕ очищается — он сохраняется между тиками.
   */
  void clear_contributions()
  {
    scale_contribs_.clear();
    additive_contribs_.clear();
    lock_contribs_.clear();
    effective_ = EffectiveConstraints{};
  }

  /**
   * @brief Получить resolved effective constraints.
   * @return Константная ссылка на вычисленные ограничения.
   * @warning Должен вызываться после resolve().
   */
  const EffectiveConstraints& effective() const { return effective_; }

  /**
   * @brief Количество активных scale contributions.
   * Полезно для отладки и тестирования.
   */
  std::size_t scale_contrib_count() const { return scale_contribs_.size(); }

  /**
   * @brief Количество активных lock contributions.
   */
  std::size_t lock_contrib_count() const { return lock_contribs_.size(); }

  /**
   * @brief Количество активных additive contributions.
   */
  std::size_t additive_contrib_count() const { return additive_contribs_.size(); }

private:
  // Single-owner: type-indexed storage
  std::unordered_map<std::type_index, std::any> fields_;

  // Contributions (заполняются за тик, очищаются в начале следующего)
  std::vector<ScaleContribution> scale_contribs_;
  std::vector<AdditiveContribution> additive_contribs_;
  std::vector<LockContribution> lock_contribs_;

  // Resolved — результат последнего resolve()
  EffectiveConstraints effective_;
};

}  // namespace s2