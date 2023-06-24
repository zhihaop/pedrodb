#ifndef PRACTICE_MATRIX_H
#define PRACTICE_MATRIX_H

#include "proto/matrix.pb.h"

#include <vector>
#include <memory>

class Matrix {
  int rows_{};
  int cols_{};
  std::vector<int> data_;

public:
  Matrix() = default;
  Matrix(int rows, int cols) : rows_(rows), cols_(cols), data_(rows * cols) {}
  Matrix(const pedro::pb::Matrix &matrix) {
    rows_ = matrix.rows();
    cols_ = matrix.cols();

    auto data = matrix.data();
    data_.assign(data.begin(), data.end());
  }

  Matrix(const Matrix &other)
      : rows_(other.rows_), cols_(other.cols_), data_(other.data_) {}
  Matrix(Matrix &&other)
      : rows_(other.rows_), cols_(other.cols_), data_(std::move(other.data_)) {
    other.rows_ = 0;
    other.cols_ = 0;
  }

  Matrix &operator=(const Matrix &other) {
    if (this == &other) {
      return *this;
    }

    cols_ = other.cols_;
    rows_ = other.rows_;
    data_ = other.data_;
    return *this;
  }

  Matrix &operator=(Matrix &&other) {
    if (this == &other) {
      return *this;
    }

    cols_ = other.cols_;
    rows_ = other.rows_;
    data_ = std::move(other.data_);

    other.cols_ = 0;
    other.rows_ = 0;
    return *this;
  }

  template <class M> bool operator==(M &&m) const noexcept {
    if (m.cols() != cols_ || m.rows() != rows_) {
      return false;
    }
    for (int i = 0; i < rows_; ++i) {
      for (int j = 0; j < cols_; ++j) {
        if (m[i][j] != *this[i][j]) {
          return false;
        }
      }
    }
    return true;
  }

  int cols() const noexcept { return cols_; }
  int rows() const noexcept { return rows_; }
  int *operator[](size_t row) noexcept { return &data_[row * cols_]; }
  const int *operator[](size_t row) const noexcept {
    return &data_[row * cols_];
  }

  void CopyTo(pedro::pb::Matrix &other) const noexcept {
    other.set_cols(cols_);
    other.set_rows(rows_);
    auto &data = *other.mutable_data();
    data.Assign(data_.begin(), data_.end());
  }
};

#endif // PRACTICE_MATRIX_H
