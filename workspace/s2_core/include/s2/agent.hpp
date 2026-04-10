#pragma once

/**
 * @file agent.hpp
 * Структура агента — сущности, которой можно управлять.
 *
 * Agent — это робот или управляемый объект в симуляции.
 * Содержит собственный SharedState, позу, скорость и визуальное/коллизонное описание.
 * Поддерживает систему плагинов (IAgentPlugin).
 */

#include <s2/kinematic_tree.hpp>
#include <s2/shared_state.hpp>
#include <s2/types.hpp>
#include <s2/plugin_base.hpp>

#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

namespace s2
{

/**
 * @brief Сущность агента — основного управляемого объекта.
 */
struct Agent
{
  AgentId id{0};                          ///< Уникальный идентификатор агента
  std::string name;                       ///< Человекочитаемое имя
  int domain_id{0};                       ///< Идентификатор домена симуляции

  Pose3D world_pose;                      ///< Поза в мировых координатах
  Velocity world_velocity;               ///< Текущая скорость в мировых координатах

  SharedState state;                     ///< Разделяемое состояние агента

  std::unordered_set<std::string> capabilities;  ///< Набор «умений» агента

  ///< Плагины агента (дифференциальный привод, GNSS, IMU, Lidar и т.д.)
  std::vector<std::unique_ptr<plugins::IAgentPlugin>> plugins;

  CollisionShape bounding;               ///< Коллизионный bounding volume
  VisualDesc visual;                     ///< Визуальное описание

  /// Кинематическое дерево агента (nullptr = одиночное твёрдое тело).
  /// Описывает иерархию звеньев: base_link → arm_link → camera_link и т.д.
  /// Создаётся SceneLoader'ом при наличии поля links: в конфиге агента.
  std::unique_ptr<KinematicTree> kinematic_tree;
};

} // namespace s2