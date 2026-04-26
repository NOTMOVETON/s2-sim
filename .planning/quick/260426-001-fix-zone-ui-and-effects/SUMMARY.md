---
quick_id: 260426-001
status: complete
commit: 2cee76f
date: 2026-04-26
---

# Summary: fix-zone-ui-and-effects

## Исправлено

1. **Лидар в тумане не видит статичные цилиндры** — корень: `parse_geometry` оставлял `prim.height=1.0` вместо `size.z=2.0` для всех YAML-цилиндров с полем `size`, но без `height`. Теперь `prim.height = size.z()` по умолчанию для `cylinder`.

2. **EMI эффект не применялся** — robot_sensor не имел capability `gnss_sensor`/`imu_sensor`, которые требовал EMI. Добавлены в test_phase1.yaml. Дополнительно: дублирующий sensor_mod в EMIEffect убран (было 2, стало 1).

3. **GnssPlugin игнорировал зоны EMI** — добавлено чтение `active_sensor_mods` с `param="noise_std"` в `update()`. Effective accuracy отображается в plugin accordion боковой панели при въезде в emi_zone.

4. **Zone Tab UI** — гизмо трансформации теперь деактивируется при открытии вкладки Зоны. Форма получила CSS стиль, совпадающий с вкладкой Агенты. Добавлена подсказка что SpawnZone/DespawnZone — Wave 2.

## Тесты
Все 3 test suite прошли (s2_core_tests, s2_editor_tests, s2_plugins_tests). EMI тесты обновлены под 1 mod вместо 2.
