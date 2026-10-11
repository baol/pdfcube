#pragma once

#include <array>
#include <cmath>

namespace pdfcube {

struct Matrix {
  std::array<float, 16> value{};

  static Matrix identity() {
    Matrix result;
    result.value = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    return result;
  }
};

inline Matrix operator*(const Matrix& left, const Matrix& right) {
  Matrix result;
  for (int column = 0; column < 4; ++column) {
    for (int row = 0; row < 4; ++row) {
      for (int inner = 0; inner < 4; ++inner) {
        result.value[column * 4 + row] +=
            left.value[inner * 4 + row] * right.value[column * 4 + inner];
      }
    }
  }
  return result;
}

inline Matrix translate(float x, float y, float z) {
  Matrix result = Matrix::identity();
  result.value[12] = x;
  result.value[13] = y;
  result.value[14] = z;
  return result;
}

inline Matrix scale(float x, float y, float z) {
  Matrix result = Matrix::identity();
  result.value[0] = x;
  result.value[5] = y;
  result.value[10] = z;
  return result;
}

inline Matrix rotate_x(float radians) {
  Matrix result = Matrix::identity();
  result.value[5] = std::cos(radians);
  result.value[6] = std::sin(radians);
  result.value[9] = -std::sin(radians);
  result.value[10] = std::cos(radians);
  return result;
}

inline Matrix rotate_y(float radians) {
  Matrix result = Matrix::identity();
  result.value[0] = std::cos(radians);
  result.value[2] = -std::sin(radians);
  result.value[8] = std::sin(radians);
  result.value[10] = std::cos(radians);
  return result;
}

inline Matrix perspective(float fov, float aspect, float near, float far) {
  Matrix result;
  const float tangent = std::tan(fov / 2.0F);
  result.value[0] = 1.0F / (aspect * tangent);
  result.value[5] = 1.0F / tangent;
  result.value[10] = -(far + near) / (far - near);
  result.value[11] = -1.0F;
  result.value[14] = -(2.0F * far * near) / (far - near);
  return result;
}

inline Matrix look_at(float eye_x, float eye_y, float eye_z, float target_x, float target_y,
                      float target_z) {
  const auto normalize = [](std::array<float, 3> value) {
    const float length =
        std::sqrt(value[0] * value[0] + value[1] * value[1] + value[2] * value[2]);
    return std::array<float, 3>{value[0] / length, value[1] / length, value[2] / length};
  };
  const auto cross = [](const std::array<float, 3>& left,
                        const std::array<float, 3>& right) {
    return std::array<float, 3>{left[1] * right[2] - left[2] * right[1],
                                left[2] * right[0] - left[0] * right[2],
                                left[0] * right[1] - left[1] * right[0]};
  };
  const auto dot = [](const std::array<float, 3>& left,
                      const std::array<float, 3>& right) {
    return left[0] * right[0] + left[1] * right[1] + left[2] * right[2];
  };

  const std::array<float, 3> eye{eye_x, eye_y, eye_z};
  const std::array<float, 3> forward =
      normalize({target_x - eye_x, target_y - eye_y, target_z - eye_z});
  const std::array<float, 3> side = normalize(cross(forward, {0.0F, 1.0F, 0.0F}));
  const std::array<float, 3> up = cross(side, forward);

  Matrix result = Matrix::identity();
  result.value[0] = side[0];
  result.value[1] = up[0];
  result.value[2] = -forward[0];
  result.value[4] = side[1];
  result.value[5] = up[1];
  result.value[6] = -forward[1];
  result.value[8] = side[2];
  result.value[9] = up[2];
  result.value[10] = -forward[2];
  result.value[12] = -dot(side, eye);
  result.value[13] = -dot(up, eye);
  result.value[14] = dot(forward, eye);
  return result;
}

}  // namespace pdfcube

