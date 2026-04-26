---
phase: 01-zone-visual-control-layer
reviewed: 2026-04-26T12:00:00Z
depth: standard
files_reviewed: 21
files_reviewed_list:
  - workspace/s2_core/CMakeLists.txt
  - workspace/s2_core/include/s2/effect_context.hpp
  - workspace/s2_core/include/s2/scene_loader.hpp
  - workspace/s2_core/include/s2/sim_engine.hpp
  - workspace/s2_core/include/s2/world_snapshot.hpp
  - workspace/s2_core/include/s2/zone.hpp
  - workspace/s2_core/include/s2/zone_spawn_system.hpp
  - workspace/s2_core/include/s2/zone_system.hpp
  - workspace/s2_core/src/world_snapshot.cpp
  - workspace/s2_core/src/zone_spawn_system.cpp
  - workspace/s2_core/src/zone_system.cpp
  - workspace/s2_core/tests/test_zone_commands.cpp
  - workspace/s2_core/tests/test_zone_detection_mode.cpp
  - workspace/s2_core/tests/test_zone_lifecycle.cpp
  - workspace/s2_core/tests/test_zone_owned.cpp
  - workspace/s2_core/tests/test_zone_self_destruct.cpp
  - workspace/s2_core/tests/test_zone_spawn_triggers.cpp
  - workspace/s2_visualizer/src/main.cpp
  - workspace/s2_visualizer/src/viz_server.cpp
  - workspace/s2_visualizer/src/viz_server.hpp
  - workspace/s2_visualizer/web/index.html
findings:
  critical: 2
  warning: 4
  info: 3
  total: 9
status: issues_found
---

# Phase 1: Code Review Report

**Reviewed:** 2026-04-26T12:00:00Z
**Depth:** standard
**Files Reviewed:** 21
**Status:** issues_found

## Summary

Phase 1 (Zone Visual & Control Layer) introduces zone data types, lifecycle management, detection modes (CENTER, BOUNDING, PER_LINK), self-destruct policies, spawn triggers (timer/event/state_change), REST/UI endpoints for zone CRUD, and Three.js zone rendering with visual hints.

Архитектурно код хорошо структурирован: зоны move-only (unique_ptr в EffectDesc), lifecycle корректно обновляет strength с ограничениями, self-destruct удаляет зоны после итерации (zones_to_destroy). Тесты покрывают все ключевые сценарии.

Выявлены 2 критических проблемы: command injection в WebSocket handshake и data race при прямом доступе к ZoneSystem из HTTP-потока. Также 4 предупреждения по логике (half_size сериализация, paused_ без атомарности, JSON escaping, EventBus подписки-утечки).

## Critical Issues

### CR-01: Command injection в compute_ws_accept через popen

**File:** `workspace/s2_visualizer/src/viz_server.cpp:32`
**Issue:** WebSocket key от клиента подставляется напрямую в shell-команду через string concatenation и popen(). Злоумышленник может отправить специально сформированный Sec-WebSocket-Key с shell-метасимволами (например `'; rm -rf /; echo '`) и выполнить произвольную команду на сервере.
**Fix:** Заменить popen на прямой вызов OpenSSL C API (EVP_Digest для SHA-1 + base64-кодирование), либо использовать библиотечную реализацию SHA-1. Если popen необходим временно -- экранировать входные данные или использовать whitelist-валидацию base64-символов.

```cpp
// Вариант 1: Валидация key перед popen (временный fix)
static std::string compute_ws_accept(const std::string& key) {
    // WebSocket key -- строго base64, допустимы только [A-Za-z0-9+/=]
    for (char c : key) {
        if (!std::isalnum(c) && c != '+' && c != '/' && c != '=') {
            return ""; // Отклонить невалидный ключ
        }
    }
    // ... далее popen
}

// Вариант 2 (предпочтительный): OpenSSL C API
#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>

static std::string compute_ws_accept(const std::string& key) {
    std::string input = key + "258EAFA5-E914-47DA-95CA-5AB37E8E73AB";
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(input.c_str()), input.size(), hash);
    // base64 encode hash
    BIO* bio = BIO_new(BIO_s_mem());
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    bio = BIO_push(b64, bio);
    BIO_write(bio, hash, SHA_DIGEST_LENGTH);
    BIO_flush(bio);
    BUF_MEM* bufferPtr;
    BIO_get_mem_ptr(bio, &bufferPtr);
    std::string result(bufferPtr->data, bufferPtr->length);
    BIO_free_all(bio);
    return result;
}
```

### CR-02: Data race -- zone_system_ мутируется из HTTP-потока без синхронизации

**File:** `workspace/s2_visualizer/src/main.cpp:142-155`
**Issue:** Методы `on_move_zone`, `on_toggle_zone`, `on_update_zone_visual` вызываются из HTTP-потока сервера (через `serve_http` -> `handle_command`) и напрямую модифицируют `zone_system_` через `engine_->zone_system().move_zone(...)` и т.д. Одновременно sim-поток вызывает `ZoneSystem::tick()`, итерируя и модифицируя тот же вектор `zones_`. Это -- undefined behavior (data race на `std::vector<Zone>`).

Для сравнения: `on_spawn_zone` и `on_despawn_zone` корректно используют `engine_->push_command()` с mutex-защитой.

**Fix:** Перевести move_zone, toggle_zone, update_zone_visual на KernelCommand, аналогично SpawnZone/DespawnZone. Для этого:
1. Добавить команды `cmd::MoveZone`, `cmd::ToggleZone`, `cmd::UpdateZoneVisual` в kernel_command.hpp.
2. Обработать их в `SimEngine::apply_kernel_command`.
3. В `SimEngineCommandAdapter` использовать `engine_->push_command(...)` вместо прямых вызовов.

```cpp
// main.cpp: исправленный on_move_zone
bool on_move_zone(const std::string& zone_id, double x, double y) override {
    if (!engine_) return false;
    s2::cmd::MoveZone cmd;
    cmd.id = zone_id;
    cmd.center = s2::Vec3{x, y, 0.0};
    engine_->push_command(s2::KernelCommand{cmd});
    return true;
}
```

## Warnings

### WR-01: half_size сериализуется как full_size, но JSON-ключ остаётся "half_size"

**File:** `workspace/s2_core/src/world_snapshot.cpp:149`
**Issue:** Строка `j["half_size"] = {zone.half_size.x() * 2, zone.half_size.y() * 2, zone.half_size.z() * 2}` умножает half_size на 2 (получая full extents), но JSON-ключ остаётся `"half_size"`. Клиентский код (app.js:1617-1623) использует эти значения как full extents для размера Box, что визуально корректно, но семантически вводит в заблуждение. Если другой клиент интерпретирует `half_size` буквально, Box будет вдвое больше.

**Fix:** Либо переименовать ключ в `"size"` или `"full_size"`, либо убрать умножение на 2 и пусть клиент сам удваивает.

```cpp
// Вариант A: сериализовать как true half_size
j["half_size"] = {zone.half_size.x(), zone.half_size.y(), zone.half_size.z()};

// Вариант B: переименовать ключ
j["size"] = {zone.half_size.x() * 2, zone.half_size.y() * 2, zone.half_size.z() * 2};
```

### WR-02: paused_ не является atomic -- data race при чтении из HTTP/viz потоков

**File:** `workspace/s2_core/include/s2/sim_engine.hpp:1112`
**Issue:** `paused_` объявлен как `bool` (не `std::atomic<bool>`), но `pause()`, `resume()`, `reset()` вызываются из HTTP-потока (через VizCommandHandler), а `paused_` читается в `tick()` из sim-потока. Это -- data race (UB по стандарту C++). `running_` корректно объявлен atomic, но `paused_` -- нет.

**Fix:**
```cpp
std::atomic<bool> paused_{false};
```

### WR-03: JSON injection в on_get_scene_state при формировании ошибки

**File:** `workspace/s2_visualizer/src/main.cpp:255`
**Issue:** Строка `return std::string("{\"error\":\"") + e.what() + "\"}"` конкатенирует `e.what()` в JSON без экранирования кавычек и backslash. Если сообщение об ошибке содержит `"` или `\`, результирующий JSON будет невалидным, что приведёт к ошибке парсинга на клиенте.

**Fix:** Использовать nlohmann::json для корректного экранирования:
```cpp
} catch (const std::exception& e) {
    nlohmann::json err;
    err["error"] = e.what();
    return err.dump();
}
```

### WR-04: ZoneSpawnSystem EventBus подписки не отписываются при clear() или повторном init()

**File:** `workspace/s2_core/src/zone_spawn_system.cpp:14-37`
**Issue:** `init()` создает подписки на EventBus (subscribe для ZoneEntered, ZoneExited, SignalActivated, GrabSucceeded, GrabFailed, ActorStateChanged). Метод `clear()` очищает `templates_`, но не отписывается от EventBus. Если `init()` вызывается повторно (например, при `load_world` после смены сцены), каждый вызов добавляет новые подписки, а старые callback-и продолжают ссылаться на `this` -- что корректно по указателю, но приводит к N-кратному срабатыванию триггеров при N повторных init().

**Fix:** Добавить механизм отписки (unsubscribe) в EventBus, либо добавить guard в init():
```cpp
void ZoneSpawnSystem::init(SimBus& bus, KernelCommandQueue& out_queue, double) {
    out_queue_ = &out_queue;
    if (initialized_) return; // Не подписываться повторно
    initialized_ = true;
    // ... subscribe ...
}
```

## Info

### IN-01: Unused global variable g_viz_server

**File:** `workspace/s2_visualizer/src/viz_server.cpp:20`
**Issue:** `static VizServer* g_viz_server = nullptr` присваивается в `start()` и обнуляется в `stop()`, но нигде не читается. Рядом есть другая глобальная переменная `g_broadcast_server` (строка 245), которая фактически используется. `g_viz_server` -- мёртвый код.

**Fix:** Удалить `g_viz_server` и связанные присваивания (строки 20, 64, 79).

### IN-02: Magic number 3.14159... вместо M_PI

**File:** `workspace/s2_core/include/s2/sim_engine.hpp:687-689`
**Issue:** Нормализация yaw использует литерал `3.14159265358979323846` вместо `M_PI`, который уже используется в других местах того же проекта (scene_loader.hpp:152).

**Fix:**
```cpp
agent.world_pose.yaw = std::fmod(agent.world_pose.yaw, 2.0 * M_PI);
if (agent.world_pose.yaw < 0) {
    agent.world_pose.yaw += 2.0 * M_PI;
}
```

### IN-03: Неиспользуемый sha1() stub

**File:** `workspace/s2_visualizer/src/viz_server.cpp:23-27`
**Issue:** Функция `sha1()` объявлена и реализована как заглушка (заполняет hash нулями), но нигде не вызывается -- используется `compute_ws_accept` через popen. Мёртвый код.

**Fix:** Удалить функцию sha1().

---

_Reviewed: 2026-04-26T12:00:00Z_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
