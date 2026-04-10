#pragma once

/**
 * @file prop.hpp
 * Структура пропа — пассивного объекта в симуляции.
 *
 * Prop — это бочка, ящик, паллета и т.д.
 * Может быть перемещаемым (movable) или статичным.
 */

#include <s2/types.hpp>

#include <string>
#include <unordered_map>

namespace s2
{

/**
 * @brief Пассивный объект — мебель, ящики, декорации.
 *
 * Может быть movable = true, тогда его можно толкать/переносить.
 * properties — произвольные ключ-значение для логики (например "type": "explosive").
 */
struct Prop
{
  ObjectId id{0};                          ///< Уникальный идентификатор объекта
  std::string type;                        ///< Тип объекта ("barrel", "crate", ...)
  Pose3D world_pose;                       ///< Поза в мировых координатах
  bool movable{false};                     ///< Можно ли перемещать

  CollisionShape collision;               ///< Коллизионное описание
  VisualDesc visual;                       ///< Визуальное описание

  /// Произвольные свойства для логики (например "type": "explosive")
  std::unordered_map<std::string, std::string> properties;
};

} // namespace s2