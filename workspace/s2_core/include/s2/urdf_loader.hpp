#pragma once

/**
 * @file urdf_loader.hpp
 * Загрузчик кинематического дерева из URDF-файла.
 *
 * Использует tinyxml2 для парсинга XML.
 * Возвращает KinematicTree с корнем в root_frame (обычно "base_link").
 * Звенья выше root_frame (например base_footprint) игнорируются.
 */

#include <s2/kinematic_tree.hpp>
#include <string>

namespace s2
{

/**
 * @brief Загрузить KinematicTree из URDF-файла.
 *
 * Алгоритм:
 *  1. Парсим все <joint> в карту: child_link → {type, parent, origin, axis, limits}
 *  2. Добавляем root_frame как корневое звено (parent = "")
 *  3. BFS: для каждого добавленного звена ищем джоинты где parent_link == link.name
 *  4. Добавляем дочерние звенья с правильными параметрами джоинта
 *
 * Типы джоинтов URDF:
 *  "fixed"      → JointType::FIXED
 *  "revolute"   → JointType::REVOLUTE
 *  "continuous" → JointType::CONTINUOUS
 *  "prismatic"  → JointType::PRISMATIC
 *
 * @param path        Путь к URDF-файлу
 * @param root_frame  Имя корневого звена (по умолчанию "base_link")
 * @return KinematicTree с заполненными звеньями
 * @throws std::runtime_error если файл не найден или XML некорректен
 */
KinematicTree load_urdf(const std::string& path,
                        const std::string& root_frame = "base_link");

} // namespace s2
