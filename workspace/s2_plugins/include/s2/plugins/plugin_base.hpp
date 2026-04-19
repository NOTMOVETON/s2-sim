#pragma once

/**
 * @file plugin_base.hpp (s2_plugins)
 * Реэкспортирует из s2_core + объявляет create_plugin.
 */

#include <s2/plugin_base.hpp>
#include <yaml-cpp/yaml.h>
#include <memory>
#include <string>

namespace s2 { namespace plugins { class IAgentPlugin; } }

namespace s2 {
namespace plugins {

/**
 * @brief Создать плагин по типу и сконфигурировать из YAML.
 * Возвращает nullptr если тип неизвестен.
 */
std::unique_ptr<IAgentPlugin> create_plugin(const std::string& type, const YAML::Node& node);

/**
 * @brief Получить реестр схем всех зарегистрированных плагинов в виде JSON-строки.
 *
 * Формат: JSON-массив объектов:
 * [{"type":"diff_drive","label":"DiffDrive (привод)","params":[...]}, ...]
 *
 * Используется фронтендом для построения UI-редактора агентов без хардкодов.
 */
std::string list_plugin_schemas();

} // namespace plugins
} // namespace s2
