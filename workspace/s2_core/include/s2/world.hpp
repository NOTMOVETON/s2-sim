#pragma once

/**
 * @file world.hpp
 * SimWorld — контейнер для всех сущностей симуляции.
 *
 * Хранит агентов, пропы, акторов и статическую геометрию мира.
 * SimEngine владеет SimWorld и оперирует его содержимым в тиковом цикле.
 */

#include <s2/agent.hpp>
#include <s2/actor.hpp>
#include <s2/prop.hpp>
#include <s2/heightmap.hpp>

#include <vector>
#include <string>

namespace s2
{

/**
 * @brief Статический примитив мира (стена, колонна, сфера).
 *
 * Используется для коллизий со статикой и визуализации.
 * Типы: "box", "cylinder", "sphere".
 */
struct WorldPrimitive {
    std::string type;       ///< Тип фигуры: "box", "cylinder", "sphere"
    Pose3D pose;            ///< Позиция и ориентация примитива
    Vec3 size{1, 1, 1};     ///< Для box: размеры по X, Y, Z
    double radius{0.5};     ///< Для sphere/cylinder: радиус
    double height{1.0};     ///< Для cylinder: высота
    std::string color{ "#808080" }; ///< Цвет для визуализации
};

/**
 * @brief Контейнер для всех сущностей симуляции.
 *
 * SimWorld — это «сцена» симуляции. Содержит:
 *  - Агентов (управляемые роботы)
 *  - Пропы (пассивные объекты: ящики, бочки)
 *  - Акторов (активные неагентные объекты: двери, лифты)
 *
 * SimEngine владеет SimWorld и проходит по его содержимому каждый тик.
 */
class SimWorld
{
public:
  /**
   * @brief Добавить агента в мир.
   * @param agent Описание агента
   */
  void add_agent(Agent agent)
  {
    agents_.push_back(std::move(agent));
  }

  /**
   * @brief Добавить пассивный объект в мир.
   * @param prop Описание объекта
   */
  void add_prop(Prop prop)
  {
    props_.push_back(std::move(prop));
  }

  /**
   * @brief Добавить актора в мир.
   * @param actor Описание актора
   */
  void add_actor(Actor actor)
  {
    actors_.push_back(std::move(actor));
  }

  /**
   * @brief Найти агента по идентификатору.
   * @param id Идентификатор агента
   * @return Указатель на агента или nullptr если не найден
   */
  Agent* get_agent(AgentId id)
  {
    for (auto& agent : agents_)
    {
      if (agent.id == id)
        return &agent;
    }
    return nullptr;
  }

  /**
   * @brief Найти пассивный объект по идентификатору.
   * @param id Идентификатор объекта
   * @return Указатель на объект или nullptr если не найден
   */
  Prop* get_prop(ObjectId id)
  {
    for (auto& prop : props_)
    {
      if (prop.id == id)
        return &prop;
    }
    return nullptr;
  }

  /**
   * @brief Найти актора по идентификатору.
   * @param id Идентификатор актора
   * @return Указатель на актора или nullptr если не найден
   */
  Actor* get_actor(ActorId id)
  {
    for (auto& actor : actors_)
    {
      if (actor.id == id)
        return &actor;
    }
    return nullptr;
  }

  /**
   * @brief Получить ссылку на вектор агентов.
   */
  std::vector<Agent>& agents() { return agents_; }

  /**
   * @brief Получить константную ссылку на вектор агентов.
   */
  const std::vector<Agent>& agents() const { return agents_; }

  /**
   * @brief Получить ссылку на вектор пропов.
   */
  std::vector<Prop>& props() { return props_; }

  /**
   * @brief Получить константную ссылку на вектор пропов.
   */
  const std::vector<Prop>& props() const { return props_; }

  /**
   * @brief Получить ссылку на вектор акторов.
   */
  std::vector<Actor>& actors() { return actors_; }

  /**
   * @brief Получить константную ссылку на вектор акторов.
   */
  const std::vector<Actor>& actors() const { return actors_; }

  /**
   * @brief Получить ссылку на вектор статики.
   */
  std::vector<WorldPrimitive>& static_geometry() { return static_geometry_; }

  /**
   * @brief Получить константную ссылку на вектор статики.
   */
  const std::vector<WorldPrimitive>& static_geometry() const { return static_geometry_; }

  /**
   * @brief Добавить статический примитив.
   */
  void add_static_primitive(WorldPrimitive prim)
  {
    static_geometry_.push_back(std::move(prim));
  }

  /**
   * @brief Установить heightmap.
   */
  void set_heightmap(Heightmap hm) { heightmap_ = std::move(hm); }

  /**
   * @brief Получить ссылку на heightmap.
   */
  const Heightmap& heightmap() const { return heightmap_; }

  /**
   * @brief Проверить коллизию сферы с статикой.
   * @param center Центр сферы
   * @param radius Радиус сферы
   * @return true если есть пересечение с любым примитивом
   */
  bool check_sphere_collision(const Vec3& center, double radius) const;

private:
  std::vector<Agent> agents_;
  std::vector<Prop> props_;
  std::vector<Actor> actors_;
  std::vector<WorldPrimitive> static_geometry_;
  Heightmap heightmap_ = Heightmap::flat(40.0, 40.0);  // default: плоский мир 40x40
};

// ─── Inline: проверка коллизии сферы со статикой ─────────────

inline bool SimWorld::check_sphere_collision(const Vec3& center, double radius) const
{
    for (const auto& prim : static_geometry_)
    {
        if (prim.type == "box")
        {
            // AABB-sphere тест
            double half_x = prim.size.x() / 2.0;
            double half_y = prim.size.y() / 2.0;
            double half_z = prim.size.z() / 2.0;

            // Ближайшая точка на AABB к центру сферы
            double cx = std::max(prim.pose.x - half_x, std::min(center.x(), prim.pose.x + half_x));
            double cy = std::max(prim.pose.y - half_y, std::min(center.y(), prim.pose.y + half_y));
            double cz = std::max(prim.pose.z - half_z, std::min(center.z(), prim.pose.z + half_z));

            double dx = center.x() - cx;
            double dy = center.y() - cy;
            double dz = center.z() - cz;

            if (dx * dx + dy * dy + dz * dz < radius * radius)
                return true;
        }
        else if (prim.type == "sphere")
        {
            double dx = center.x() - prim.pose.x;
            double dy = center.y() - prim.pose.y;
            double dz = center.z() - prim.pose.z;
            double dist = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (dist < radius + prim.radius)
                return true;
        }
        // Для cylinder — упрощённо как sphere (для v1 достаточно)
        else if (prim.type == "cylinder")
        {
            double dx = center.x() - prim.pose.x;
            double dy = center.y() - prim.pose.y;
            double dz = center.z() - prim.pose.z;
            double dist = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (dist < radius + prim.radius)
                return true;
        }
    }
    return false;
}

} // namespace s2