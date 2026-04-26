---
quick_id: 260426-001
slug: fix-zone-ui-and-effects
description: Исправить лидар в зоне тумана, EMI эффект, Zone Tab UI
date: 2026-04-26
status: done
---

# Задача: fix-zone-ui-and-effects

## Диагностика и план

### Bug 1: Лидар не видит цилиндры (включая те, что внутри fog_zone)
- `parse_geometry` использовал `prim.height=1.0` (default) вместо `size.z`
- Для цилиндра center z=1.0, height=1.0 → range=[0.5,1.5]; лидар на z=0 → miss
- Fix: fallback `prim.height = size.z()` для cylinder без explicit `height` в YAML

### Bug 2: EMI эффект не применялся
- `EMIEffect.required_capabilities = ["gnss_sensor","imu_sensor"]`
- robot_sensor имел только `["wheeled","surface_contact","optical_sensor"]` → caps_match = false
- Fix: добавить `gnss_sensor`, `imu_sensor` в capabilities robot_sensor

### Bug 3: GnssPlugin игнорировал active_sensor_mods
- `noise_std_` из конфига использовался напрямую без учёта sensor_mods
- Fix: применять active_sensor_mods в `update()`, хранить в `current_effective_noise_std_`

### Bug 4: Zone Tab UI
- Gizmo не деактивировался при переключении на вкладку Зоны
- Форма не имела CSS стиля (no `#zone-form-view` rules)
- Fix: `transformControls.detach()` в switchEditorTab, добавить CSS

## Файлы изменены
- workspace/s2_core/include/s2/scene_loader.hpp
- workspace/s2_plugins/include/s2/plugins/gnss.hpp
- workspace/s2_plugins/include/s2/effects/emi_effect.hpp
- workspace/s2_plugins/tests/test_effect_fog_emi.cpp
- workspace/s2_config/scenes/test_phase1.yaml
- workspace/s2_visualizer/web/index.html
- workspace/s2_visualizer/web/js/app.js
