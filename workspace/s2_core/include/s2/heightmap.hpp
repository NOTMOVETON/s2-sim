#pragma once

#include "s2/types.hpp"
#include <vector>
#include <string>
#include <optional>
#include <cmath>
#include <algorithm>

namespace s2 {

/// Heightmap — карта высот для определения поверхности мира.
/// v1 поддерживает только flat plane (z = constant).
/// В будущем добавится загрузка из PNG (grayscale 8/16 бит).
class Heightmap {
public:
    /// Создать плоскую поверхность на заданной высоте.
    /// width/height — размеры области, где heightmap валиден.
    static Heightmap flat(double width, double height, double z = 0.0);

    /// Высота в точке (x, y). Для flat — всегда z.
    double height_at(double x, double y) const;

    /// Нормаль поверхности в точке. Для flat — всегда (0, 0, 1).
    Vec3 normal_at(double x, double y) const;

    /// Внутри ли точка (x, y) границ карты.
    bool in_bounds(double x, double y) const;

    /// Ширина области.
    double width() const { return width_; }

    /// Высота области.
    double height() const { return height_; }

    /// Z уровня поверхности.
    double surface_z() const { return surface_z_; }

    Heightmap() = default;  // нужен для SceneData

private:

    std::vector<float> data_;       // raw height данные (для flat пустой)
    double width_{0.0};
    double height_{0.0};
    double surface_z_{0.0};
    bool is_flat_{true};
    double resolution_{1.0};        // метров на пиксель (для future PNG)
    int grid_width_{0};
    int grid_height_{0};
    Vec3 origin_{0, 0, 0};

    /// Вычислить высоту через билинейную интерполяцию из data_ (для future PNG).
    double interpolate(double x, double y) const;
};

// ─── Implementation ───────────────────────────────────────────────

inline Heightmap Heightmap::flat(double width, double height, double z) {
    Heightmap hm;
    hm.width_ = width;
    hm.height_ = height;
    hm.surface_z_ = z;
    hm.is_flat_ = true;
    return hm;
}

inline double Heightmap::height_at(double x, double y) const {
    if (is_flat_) {
        return surface_z_;
    }
    return interpolate(x, y);
}

inline Vec3 Heightmap::normal_at(double x, double y) const {
    if (is_flat_) {
        return Vec3{0.0, 0.0, 1.0};
    }
    // Для future PNG: вычислить нормаль через градиент
    return Vec3{0.0, 0.0, 1.0};
}

inline bool Heightmap::in_bounds(double x, double y) const {
    double half_w = width_ / 2.0;
    double half_h = height_ / 2.0;
    // Центрируем на origin
    double lx = x - origin_.x();
    double ly = y - origin_.y();
    return (lx >= -half_w && lx <= half_w && ly >= -half_h && ly <= half_h);
}

inline double Heightmap::interpolate(double x, double y) const {
    if (is_flat_ || data_.empty()) {
        return surface_z_;
    }

    // Локальные координаты
    double lx = x - origin_.x();
    double ly = y - origin_.y();

    // Перевести в пиксели
    double px = (lx + width_ / 2.0) / resolution_;
    double py = (ly + height_ / 2.0) / resolution_;

    int x0 = static_cast<int>(std::floor(px));
    int y0 = static_cast<int>(std::floor(py));
    int x1 = std::min(x0 + 1, grid_width_ - 1);
    int y1 = std::min(y0 + 1, grid_height_ - 1);
    x0 = std::max(0, x0);
    y0 = std::max(0, y0);

    double fx = px - std::floor(px);
    double fy = py - std::floor(py);

    // Билинейная интерполяция
    double v00 = data_[y0 * grid_width_ + x0];
    double v10 = data_[y0 * grid_width_ + x1];
    double v01 = data_[y1 * grid_width_ + x0];
    double v11 = data_[y1 * grid_width_ + x1];

    double top = v00 * (1.0 - fx) + v10 * fx;
    double bottom = v01 * (1.0 - fx) + v11 * fx;
    return top * (1.0 - fy) + bottom * fy;
}

} // namespace s2