#pragma once

/**
 * @file prop.hpp
 * Структура пропа — пассивного объекта в симуляции.
 *
 * Prop — это бочка, ящик, паллета и т.д.
 * Может быть перемещаемым (movable) или статичным.
 *
 * Расширения Phase 2:
 *  - signals:            обнаруживаемые сигналы (wire, aruco и т.п.)
 *  - capabilities/tags:  для capability matching (Phase 6)
 *  - attachment:         привязка к агенту (GrabberPlugin)
 *  - has_collision:      участие в CollisionSystem
 */

#include <s2/types.hpp>

#include <map>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace s2
{

/**
 * @brief Пассивный объект — мебель, ящики, декорации.
 *
 * Граница Prop и Actor (D-17):
 *  - Нет update(dt) → Prop
 *  - Есть update(dt) → Actor
 *
 * Props с has_collision=true и без attached_to_agent участвуют
 * в CollisionSystem как статические объекты (D-18).
 *
 * Захваченные пропы (attached_to_agent != nullopt) пропускаются
 * в CollisionSystem — не блокируют движение (D-21).
 */
struct Prop
{
  ObjectId id{0};                          ///< Уникальный идентификатор объекта
  std::string type;                        ///< Тип объекта ("barrel", "crate", ...)
  Pose3D world_pose;                       ///< Поза в мировых координатах
  bool movable{false};                     ///< Можно ли перемещать (GrabberPlugin)
  bool has_collision{true};                ///< Участвует ли в CollisionSystem

  CollisionShape collision;               ///< Коллизионное описание
  VisualDesc visual;                       ///< Визуальное описание

  /// Произвольные свойства для логики (например "type": "explosive")
  std::unordered_map<std::string, std::string> properties;

  // ─── Сигналы (ARCH-03) ────────────────────────────────────────────────────

  /// Обнаруживаемые сигналы пропа (wire-сигнал и т.п.)
  std::vector<Signal> signals;

  // ─── Capabilities и теги (PROP-01, Phase 6) ───────────────────────────────

  /// Capabilities для capability matching (Phase 6: ENTY-01)
  /// Примеры: "fragile", "flammable", "conductive"
  std::set<std::string> capabilities;

  /// Произвольные теги для фильтрации и логики
  /// Примеры: {"material": "wood", "weight": "heavy"}
  std::map<std::string, std::string> tags;

  // ─── Attachment (PROP-02) ──────────────────────────────────────────────────

  /// Идентификатор родительской Entity (агент или актор)
  /// nullopt = проп не привязан (свободен)
  std::optional<EntityId> attached_to_agent;

  /// Имя link'а родителя ("gripper", "base_link")
  std::string attach_link;

  /// Смещение пропа относительно link'а родителя (local_pose)
  Pose3D attach_offset;
};

} // namespace s2
