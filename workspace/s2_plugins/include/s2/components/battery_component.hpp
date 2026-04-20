#pragma once

namespace s2 {

/// Компонент батареи в SharedState.
/// Инициализируется SceneLoader для агентов с capability "has_battery".
/// Обновляется ChargingEffect каждый тик пока агент в зарядной зоне.
/// Уровень: 0.0 (разряжена) … 1.0 (полная).
struct BatteryComponent {
    double level{1.0};    ///< Текущий уровень [0.0, 1.0]
    bool charging{false}; ///< true пока агент находится в зарядной зоне
};

} // namespace s2
