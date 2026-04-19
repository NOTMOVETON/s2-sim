# Task 03 — Геометрия мира: heightmap + примитивы + простой raycast

> **Предыдущий шаг:** `02-tick-loop.md` (SimEngine тикает)
> **Следующий шаг:** `04-agents-basic.md`
> **Контекст:** `ARCHITECTURE.md` разделы 13, 15

## Цель

Мир с геометрией. После этого шага: мир загружается из YAML, есть heightmap (или flat plane), есть статические примитивы (стены-коробки), работает простой raycast (без BVH, brute force — достаточно для начала).

## Что сделать

### 1. Heightmap

```
s2_core/include/s2/heightmap.hpp
```

```cpp
class Heightmap {
public:
    // Загрузка из PNG (grayscale, 8 или 16 бит)
    static Heightmap from_png(const std::string& path, double resolution, 
                               double height_scale, Vec3 origin);

    // Flat plane (z=0 везде)
    static Heightmap flat(double width, double height, double resolution);

    // Высота в точке (x, y). Билинейная интерполяция.
    double height_at(double x, double y) const;

    // Нормаль поверхности в точке (для pitch/roll)
    Vec3 normal_at(double x, double y) const;

    // Внутри ли границ карты
    bool in_bounds(double x, double y) const;

private:
    std::vector<float> data_;
    int width_{0}, height_{0};
    double resolution_{0.1};
    double height_scale_{1.0};
    Vec3 origin_{0, 0, 0};
};
```

**Для загрузки PNG** — использовать stb_image (header-only, добавить в проект). Или начать только с flat() и добавить PNG позже.

**[СПРОСИТЬ]** Нужен ли PNG heightmap в v1 или хватит flat plane + стены из примитивов? Для складской сцены flat может быть достаточно.

### 2. Статические примитивы мира

Добавить в SimWorld:

```cpp
struct WorldPrimitive {
    std::string type;  // "box", "cylinder", "sphere"
    Pose3D pose;
    Vec3 size;         // для box
    double radius;     // для cylinder, sphere
    double height;     // для cylinder
};

class SimWorld {
    // ... existing ...
    Heightmap heightmap;
    std::vector<WorldPrimitive> static_geometry;
};
```

### 3. Простой Raycast (brute force)

```
s2_core/include/s2/raycast_engine.hpp
```

Начинаем БЕЗ BVH. Brute force по всем примитивам. Для 10-20 примитивов и 360 лучей — достаточно.

```cpp
struct Ray {
    Vec3 origin;
    Vec3 direction;  // normalized
    float max_range{30.0f};
};

struct RaycastResult {
    bool hit{false};
    float distance{0.0f};
    Vec3 point;
    Vec3 normal;
};

class RaycastEngine {
public:
    void set_static_geometry(const std::vector<WorldPrimitive>& prims);

    // Одиночный луч
    RaycastResult cast(const Ray& ray) const;

    // Батч (для лидара)
    std::vector<RaycastResult> cast_batch(const std::vector<Ray>& rays) const;

private:
    std::vector<WorldPrimitive> static_prims_;

    // Ray-primitive intersection
    std::optional<float> intersect_box(const Ray& ray, const WorldPrimitive& box) const;
    std::optional<float> intersect_sphere(const Ray& ray, const WorldPrimitive& sphere) const;
    std::optional<float> intersect_cylinder(const Ray& ray, const WorldPrimitive& cyl) const;
};
```

**Реализовать intersect для:**
- Ray-Box (AABB, ось-выровненный — достаточно для v1)
- Ray-Sphere
- Ray-Cylinder (необязательно для v1, но несложно)

Позже добавим BVH (Embree) и динамические объекты. Сейчас — только статика.

### 4. Загрузка мира из YAML

```
s2_core/include/s2/scene_loader.hpp
```

Минимальный лоадер:

```cpp
class SceneLoader {
public:
    struct SceneData {
        SimEngine::Config engine_config;
        Heightmap heightmap;
        std::vector<WorldPrimitive> geometry;
        std::vector<Agent> agents;      // пока только pose + name
        std::vector<Prop> props;
        // зоны, акторы — позже
    };

    static SceneData load(const std::string& yaml_path);
};
```

Минимальный формат YAML:

```yaml
s2:
  update_rate: 100

  world:
    surface: "flat"    # или heightmap с path

    geometry:
      - type: "box"
        pose: {x: 5.0, y: 0.0, z: 0.0}
        size: {x: 10.0, y: 0.2, z: 2.0}    # стена

      - type: "box"
        pose: {x: 0.0, y: 5.0, z: 0.0}
        size: {x: 0.2, y: 10.0, z: 2.0}    # стена

  agents:
    - name: "robot_0"
      pose: {x: 0.0, y: 0.0, z: 0.0, yaw: 0.0}
      collision:
        bounding: {type: "sphere", radius: 0.5}
      visual:
        type: "box"
        size: {x: 0.8, y: 0.5, z: 0.3}
        color: "#FF6B35"
```

### 5. Тестовая сцена

```
s2_config/scenes/test_basic.yaml
```

Комната 20×20м с четырьмя стенами и одним агентом в центре.

## Тесты

```
s2_core/tests/test_heightmap.cpp
s2_core/tests/test_raycast.cpp
s2_core/tests/test_scene_loader.cpp
```

### test_heightmap.cpp
- flat(20, 20, 0.1): height_at(5, 5) == 0.0
- normal_at любая точка flat == (0, 0, 1)
- in_bounds: true для (5,5), false для (100, 100)

### test_raycast.cpp
- Луч → стена (box): hit=true, distance правильная
- Луч в пустоту: hit=false
- Луч → sphere: hit=true
- cast_batch 360 лучей в комнате: все hit=true (мы окружены стенами)
- Расстояние до стены соответствует геометрии

### test_scene_loader.cpp
- Загрузить test_basic.yaml: 4 стены, 1 агент
- Агент на позиции (0,0,0)
- Геометрия содержит 4 box-а

## Критерии приёмки

```bash
docker compose run tests   # все тесты
```

- Heightmap (flat) работает
- Raycast находит пересечения с box и sphere
- YAML сцена загружается

## Чего НЕ делать

- Не добавлять BVH / Embree (позже)
- Не добавлять динамические объекты в raycast (позже)
- Не добавлять per-link shapes агентов (позже)
- PNG heightmap — опционально, можно отложить
