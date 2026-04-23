#pragma once

namespace s2 {

/// Состояние шин агента.
/// Хранится в SharedState как single-owner (MUTATION записывает, DiffDrivePlugin читает).
struct TirePunctureData {
    bool punctured{false};  ///< true если шины проколоты
};

} // namespace s2
