# Active Context — S2

## Текущая работа
Задача 11 ✅ ЗАВЕРШЕНА. Конфигурация транспорта и визуализатора из YAML.

### Что сделано
- `SceneData` расширен структурами `TransportConfig` и `VizConfig`
- `SceneLoader::load()` парсит секции `transport:` и `visualizer:` из YAML
- `main.cpp`: выбор транспортного адаптера и создание `VizServer` управляется через конфиг
- YAML-сцены обновлены: `test_basic.yaml`, `test_dozer.yaml`, `test_ros2_full.yaml`

## Ближайшие шаги
- Задача 05 — Зоны и эффекты (docs/05-zones-effects.md)
