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

}  // namespace pdfcube

