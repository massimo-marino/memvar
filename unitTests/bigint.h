//
// bigint.h
//
// This implementation is a re-implementation from:
// https://sites.google.com/site/indy256/algo_cpp/bigint
// However, this site is no more reachable.
//
// You can get back past snapshots from Internet Archive:
// https://web.archive.org/web/20210301000000*/https://sites.google.com/site/indy256/algo_cpp/bigint
//
// March 19 2021
// https://web.archive.org/web/20210319023614/https://sites.google.com/site/indy256/algo_cpp/bigint
//
// October 24 2011
// https://web.archive.org/web/20111024050800/https://sites.google.com/site/indy256/algo_cpp/bigint
//
// February 8 2011
// https://web.archive.org/web/20110208155854/https://sites.google.com/site/indy256/algo_cpp/bigint
//
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <iostream>
#include <sstream>
#include <cctype>
#include <iomanip>
#include <cstdint>
#include <string>
#include <vector>
#include <tuple>
#include <algorithm>
#include <span>
#include <memory>
#include <concepts>
#include <type_traits>
#include <stdexcept>
////////////////////////////////////////////////////////////////////////////////
namespace bip  // bip stands for big integer precision
{
class bigint;

// Example of usage:
// template <typename T>
// requires bip::ExtendedIntegral<T>
// void processNumber(T value) { /* ... */ }
//
// 1. Create a custom type trait exclusively for bigint
template <typename T>
struct is_bigint : std::false_type {};

template <>
struct is_bigint<bigint> : std::true_type {};

template <typename T>
inline constexpr bool is_bigint_v = is_bigint<T>::value;

// 2. Create a C++20 concept combining standard integers and bigint
template <typename T>
concept ExtendedIntegral = std::integral<T> || is_bigint_v<std::remove_cvref_t<T>>;

  [[nodiscard]] const bigint& get_zero();

using vi = std::vector<int32_t>;
using vll = std::vector<int64_t>;

class bigint final {
private:
  // When karatsuba gets to numbers with at most karatsubaCutOff_ digits,
  // it reverts to straight multiplication.
  // This helps because karatsuba is slower than straight multiplication
  // for tiny values of digits
  static constexpr size_t karatsubaCutOff_ {96};

  vi n_ {};

  int32_t sign_ {1};

  // base_ and base_digits_ must be consistent
  static constexpr int32_t base_ {1'000'000'000};

public:
  static constexpr int32_t base_digits_ {9};

  bigint() = default;

  ~bigint() = default;

  // default move ctor
  bigint(bigint&& rhs) = default;

  // default move assignment operator
  bigint& operator=(bigint&& rhs) = default;

  // copy ctor
  bigint(const bigint& rhs) = default;

  [[nodiscard]] const auto& get_n() const noexcept {
    return n_;
  }

  [[nodiscard]] auto get_sign() const noexcept {
    return sign_;
  }

  explicit bigint(const std::string& s) {
    read(s);
  }

  explicit bigint(const int64_t rhs) {
    *this = rhs;
  }

  // copy assignment
  bigint& operator=(const bigint& rhs) = default;

  bigint& operator=(int64_t rhs) {
    n_.clear();
    sign_ = 1;
    auto u_rhs = static_cast<uint64_t>(rhs);
    if (rhs < 0) {
      sign_ = -1;
      u_rhs = -u_rhs; // Safe: unsigned wrap-around
    }
    for (; u_rhs > 0; u_rhs = u_rhs / base_) {
      n_.push_back(static_cast<int32_t>(u_rhs % base_));
    }
    return *this;
  }

  [[nodiscard]] bigint operator*(const bigint& rhs) const {
    // We can safely keep the original 10^6 base (6 digits) without UB
    vll a = convert_base<vll>(this->n_, base_digits_, 6);
    vll b = convert_base<vll>(rhs.n_, base_digits_, 6);

    while (a.size() < b.size()) {
      a.push_back(0);
    }
    while (b.size() < a.size()) {
      b.push_back(0);
    }
    while (a.size() & (a.size() - 1)) {
      a.push_back(0);
      b.push_back(0);
    }

    const vll c = karatsubaMultiply(a, b);
    bigint result;

    result.sign_ = sign_ * rhs.sign_;

    // Upgraded to int64_t to safely process large borrows
    int64_t carry {0};
    int64_t cur;

    for (size_t i {0}; i < c.size(); ++i) {
      cur = c[i] + carry;

      // Safely propagate negative values as borrows
      if (cur < 0) {
        int64_t borrows = (1'000'000 - 1 - cur) / 1'000'000;
        carry = -borrows;
        cur += borrows * 1'000'000;
      } else {
        carry = cur / 1'000'000;
        cur %= 1'000'000;
      }

      result.n_.push_back(static_cast<int32_t>(cur));
    }

    // Flush any remaining positive carry
    while (carry > 0) {
      result.n_.push_back(static_cast<int32_t>(carry % 1'000'000));
      carry /= 1'000'000;
    }

    result.n_ = convert_base(result.n_, 6, base_digits_);
    result.trim();

    return result;
  }

  [[nodiscard]] bigint operator/(const bigint& rhs) const {
    if (rhs.isZero()) {
      throw std::invalid_argument("Division by zero");
    }
    return std::get<0>(divmod(*this, rhs));
  }

  [[nodiscard]] bigint operator%(const bigint& rhs) const {
    if (rhs.isZero()) {
      throw std::invalid_argument("Division by zero");
    }
    return std::get<1>(divmod(*this, rhs));
  }

  [[nodiscard]] bigint operator+(int32_t rhs) const {
    return *this + bigint(static_cast<int64_t>(rhs));
  }

  [[nodiscard]] bigint operator+(const bigint& rhs) const {
    // Short-circuit if either operand is zero to prevent infinite recursion
    if (isZero()) {
      return rhs;
    }
    if (rhs.isZero()) {
      return *this;
    }

    if (sign_ == rhs.sign_) {
      bigint result = rhs;
      int32_t carry {0};

      for (size_t i {0}; carry || (i < n_.size()); ++i) {
        if ( i == result.n_.size() ) {
          result.n_.push_back(0);
        }
        result.n_[i] += carry + (i < n_.size() ? n_[i] : 0);
        carry = result.n_[i] >= base_;
        if (carry) {
          result.n_[i] -= base_;
        }
      }
      return result;
    }
    return *this - (-rhs);
  }

  bigint& operator+=(int32_t rhs) {
    // Explicitly construct a temporary bigint to satisfy the explicit constructor,
    // then rely on your existing addition logic.
    *this = *this + bigint(static_cast<int64_t>(rhs));
    return *this;
  }

  bigint& operator-=(int32_t rhs) {
    *this = *this - bigint(static_cast<int64_t>(rhs));
    return *this;
  }

  [[nodiscard]] bigint operator-(int32_t rhs) const {
    return *this - bigint(static_cast<int64_t>(rhs));
  }

  [[nodiscard]] bigint operator-(const bigint& rhs) const {
    // Short-circuit if either operand is zero to prevent infinite recursion
    if (isZero()) {
      return -rhs;
    }
    if (rhs.isZero()) {
      return *this;
    }

    if (sign_ == rhs.sign_) {
      if (abs_greater_or_equal(rhs)) {
        bigint result = *this;
        int32_t carry {0};
        for (size_t i {0}; carry || (i < rhs.n_.size()); ++i) {
          result.n_[i] -= carry + (i < rhs.n_.size() ? rhs.n_[i] : 0);
          carry = result.n_[i] < 0;
          if (carry) {
            result.n_[i] += base_;
          }
        }
        result.trim();
        return result;
      }
      return -(rhs - *this);
    }
    return *this + (-rhs);
  }

	bigint& operator*=(int32_t rhs) {
		auto u_rhs = static_cast<uint32_t>(rhs);
		if (rhs < 0) {
		  sign_ = -sign_;
		  u_rhs = -u_rhs; // Safe: unsigned wrap-around
		}
		int32_t carry {0};
		int64_t cur;
		for (size_t i {0}; carry || (i < n_.size()); ++i) {
		  if ( i == n_.size() ) {
		    n_.push_back(0);
		  }
		  cur = n_[i] * static_cast<int64_t>(u_rhs) + carry;
		  carry = static_cast<int32_t>(cur / base_);
		  n_[i] = static_cast<int32_t>(cur % base_);
		}
		trim();
		return *this;
	}

  [[nodiscard]] bigint operator*(int32_t rhs) const {
    bigint result = *this;
    result *= rhs;
    return result;
  }

	bigint& operator/=(int32_t rhs) {
		if (rhs == 0) {
		  throw std::invalid_argument("Division by zero");
		}
		auto u_rhs = static_cast<uint32_t>(rhs);
		if (rhs < 0) {
		  sign_ = -sign_;
		  u_rhs = -u_rhs; // Safe: unsigned wrap-around
		}
		int32_t rem {0};
		int64_t cur;

		for (int32_t i = static_cast<int32_t>(n_.size()) - 1; i >= 0; --i) {
		  cur = n_[static_cast<size_t>(i)] + rem * static_cast<int64_t>(base_);
		  n_[static_cast<size_t>(i)] = static_cast<int32_t>(cur / u_rhs);
		  rem = static_cast<int32_t>(cur % u_rhs);
		}
		trim();
		return *this;
	}

  [[nodiscard]] bigint operator/(int32_t rhs) const {
    bigint result = *this;
    result /= rhs;
    return result;
  }

  [[nodiscard]] int32_t operator%(int32_t rhs) const {
    if (rhs == 0) {
      throw std::invalid_argument("Division by zero in modulo operation");
    }
    auto u_rhs = static_cast<uint32_t>(rhs);
    if (rhs < 0) {
      u_rhs = -u_rhs; // Safe: unsigned wrap-around
    }
    uint32_t m {0};
    for (int32_t i = static_cast<int32_t>(n_.size()) - 1; i >= 0; --i) {
      // Cast base_ and n_ to uint64_t for safe, positive modulo arithmetic
      m = static_cast<uint32_t>((n_[static_cast<size_t>(i)] + static_cast<uint64_t>(m) * base_) % u_rhs);
    }
    return static_cast<int32_t>(m) * sign_;
  }

  bigint& operator+=(const bigint& rhs) {
		*this = *this + rhs;
		return *this;
	}

  bigint& operator-=(const bigint& rhs) {
		*this = *this - rhs;
		return *this;
	}

  bigint& operator*=(const bigint& rhs) {
		*this = *this * rhs;
		return *this;
	}

  bigint& operator/=(const bigint& rhs) {
		*this = *this / rhs;
		return *this;
	}

  bool operator<(int32_t rhs) const noexcept {
    // 1. Short-circuit and guard against zeroes
    const bool lhs_zero = isZero();
    const bool rhs_zero = (rhs == 0);

    if (lhs_zero && rhs_zero) {
      return false; // 0 is never less than 0
    }
    if (lhs_zero) {
      return rhs > 0; // 0 < positive is true, 0 < negative is false
    }
    if (rhs_zero) {
      return sign_ == -1; // negative < 0 is true, positive < 0 is false
    }

    // 2. Differing signs
    const int32_t rhs_sign = (rhs < 0) ? -1 : 1;
    if (sign_ != rhs_sign) {
      return sign_ < rhs_sign;
    }

    // 3. Evaluate absolute value of rhs into base_-10^9 blocks
    // Cast to int64_t first to safely handle INT_MIN absolute value
    const int64_t abs_rhs = rhs < 0 ? -static_cast<int64_t>(rhs) : static_cast<int64_t>(rhs);
    const auto rhs_block0 = static_cast<int32_t>(abs_rhs % base_);
    const auto rhs_block1 = static_cast<int32_t>(abs_rhs / base_);
    const size_t rhs_size = (rhs_block1 > 0) ? 2 : 1;

    // 4. Differing sizes (Magnitudes)
    if (n_.size() != rhs_size) {
      return sign_ == 1 ? n_.size() < rhs_size : n_.size() > rhs_size;
    }

    // 5. Same sizes and signs: Compare blocks from most to least significant
    if (rhs_size == 2) {
      if (n_[1] != rhs_block1) {
        return sign_ == 1 ? n_[1] < rhs_block1 : n_[1] > rhs_block1;
      }
    }
    if (n_[0] != rhs_block0) {
      return sign_ == 1 ? n_[0] < rhs_block0 : n_[0] > rhs_block0;
    }

    // 6. Values are perfectly identical
    return false;
  }

  bool operator<(const bigint& rhs) const noexcept {
    // 1. Short-circuit and guard against non-canonical zeroes (e.g., [0], -0)
    const bool lhs_zero = isZero();
    const bool rhs_zero = rhs.isZero();

    if (lhs_zero && rhs_zero) {
      return false;  // 0 is never less than 0
    }
    if (lhs_zero) {
      return rhs.sign_ == 1;  // 0 < positive is true, 0 < negative is false
    }
    if (rhs_zero) {
      return sign_ == -1;  // negative < 0 is true, positive < 0 is false
    }

    // 2. Differing signs (Since neither is zero, signs are strictly meaningful)
    if (sign_ != rhs.sign_) {
      return sign_ < rhs.sign_;
    }

    // 3. Differing sizes
    if (n_.size() != rhs.n_.size()) {
      return sign_ == 1 ? n_.size() < rhs.n_.size()
                        : n_.size() > rhs.n_.size();
    }

    // 4. Same sizes and signs: Compare blocks from most to least significant.
    auto it_lhs = n_.crbegin();
    auto it_rhs = rhs.n_.crbegin();

    for (; it_lhs != n_.crend(); ++it_lhs, ++it_rhs) {
      if (*it_lhs != *it_rhs) {
        return sign_ == 1 ? *it_lhs < *it_rhs
                          : *it_lhs > *it_rhs;
      }
    }

    // 5. Vectors are perfectly identical
    return false;
  }

  bool operator>(int32_t rhs) const noexcept {
    // 1. Short-circuit and guard against zeroes
    const bool lhs_zero = isZero();
    const bool rhs_zero = (rhs == 0);

    if (lhs_zero && rhs_zero) {
      return false; // 0 is never greater than 0
    }
    if (lhs_zero) {
      return rhs < 0; // 0 > negative is true, 0 > positive is false
    }
    if (rhs_zero) {
      return sign_ == 1; // positive > 0 is true, negative > 0 is false
    }

    // 2. Differing signs
    const int32_t rhs_sign = (rhs < 0) ? -1 : 1;
    if (sign_ != rhs_sign) {
      return sign_ > rhs_sign;
    }

    // 3. Evaluate absolute value of rhs into base_-10^9 blocks
    // Cast to int64_t first to safely handle INT_MIN absolute value
    const int64_t abs_rhs = rhs < 0 ? -static_cast<int64_t>(rhs) : static_cast<int64_t>(rhs);
    const auto rhs_block0 = static_cast<int32_t>(abs_rhs % base_);
    const auto rhs_block1 = static_cast<int32_t>(abs_rhs / base_);
    const size_t rhs_size = (rhs_block1 > 0) ? 2 : 1;

    // 4. Differing sizes (Magnitudes)
    if (n_.size() != rhs_size) {
      return sign_ == 1 ? n_.size() > rhs_size : n_.size() < rhs_size;
    }

    // 5. Same sizes and signs: Compare blocks from most to least significant
    if (rhs_size == 2) {
      if (n_[1] != rhs_block1) {
        return sign_ == 1 ? n_[1] > rhs_block1 : n_[1] < rhs_block1;
      }
    }
    if (n_[0] != rhs_block0) {
      return sign_ == 1 ? n_[0] > rhs_block0 : n_[0] < rhs_block0;
    }

    // 6. Values are perfectly identical
    return false;
  }

  bool operator>(const bigint& rhs) const noexcept {
    return rhs < *this;
  }

  bool operator<=(const bigint& rhs) const noexcept {
    return !(rhs < *this);
  }

  bool operator<=(int32_t rhs) const noexcept {
    return !(*this > rhs);
  }

  bool operator>=(const bigint& rhs) const noexcept {
    return !(*this < rhs);
  }

  bool operator>=(int32_t rhs) const noexcept {
    return !(*this < rhs);
  }

  bool operator==(const bigint& rhs) const noexcept {
    return sign_ == rhs.sign_ && n_.size() == rhs.n_.size() && n_ == rhs.n_;
  }

  bool operator==(int32_t rhs) const noexcept {
    // 1. Short-circuit and guard against zeroes
    const bool lhs_zero = isZero();
    const bool rhs_zero = (rhs == 0);

    if (lhs_zero && rhs_zero) {
      return true; // 0 == 0 is true
    }
    if (lhs_zero || rhs_zero) {
      return false; // One is zero, the other is not
    }

    // 2. Differing signs (neither is zero here)
    const int32_t rhs_sign = (rhs < 0) ? -1 : 1;
    if (sign_ != rhs_sign) {
      return false;
    }

    // 3. Evaluate absolute value of rhs into base_-10^9 blocks
    // Cast to int64_t first to safely handle INT_MIN absolute value
    const int64_t abs_rhs = rhs < 0 ? -static_cast<int64_t>(rhs) : static_cast<int64_t>(rhs);
    const auto rhs_block0 = static_cast<int32_t>(abs_rhs % base_);
    const auto rhs_block1 = static_cast<int32_t>(abs_rhs / base_);
    const size_t rhs_size = (rhs_block1 > 0) ? 2 : 1;

    // 4. Differing sizes (Magnitudes)
    if (n_.size() != rhs_size) {
      return false;
    }

    // 5. Same sizes and signs: Compare blocks
    if (rhs_size == 2) {
      if (n_[1] != rhs_block1) {
        return false;
      }
    }
    if (n_[0] != rhs_block0) {
      return false;
    }

    // 6. Values are perfectly identical
    return true;
  }

  bool operator!=(const bigint& rhs) const noexcept {
    return !(*this == rhs);
  }

  bool operator!=(int32_t rhs) const noexcept {
    return !(*this == rhs);
  }

  [[nodiscard]] bigint operator-() const {
    bigint result = *this;

    result.sign_ = -sign_;
    result.trim();

    return result;
  }

  [[nodiscard]] bigint abs() const {
    bigint result = *this;
    result.sign_ = 1;
    return result;
  }

  [[nodiscard]] int64_t longValue() const {
    uint64_t result {0};

    for (int32_t i = static_cast<int32_t>(n_.size()) - 1; i >= 0; --i) {
      // 1. Guard against accumulation overflow
      if (result > UINT64_MAX / base_) {
        throw std::out_of_range("bigint is too large to fit in int64_t");
      }
      result *= base_;

      uint64_t next_block = static_cast<uint64_t>(n_[static_cast<size_t>(i)]);
      if (UINT64_MAX - result < next_block) {
        throw std::out_of_range("bigint is too large to fit in int64_t");
      }
      result += next_block;
    }

    // 2. Guard against crossing INT64_MAX / INT64_MIN boundaries
    if (sign_ == 1 && result > static_cast<uint64_t>(INT64_MAX)) {
      throw std::out_of_range("bigint exceeds maximum int64_t value");
    }
    if (sign_ == -1 && result > static_cast<uint64_t>(INT64_MAX) + 1) {
      throw std::out_of_range("bigint falls below minimum int64_t value");
    }

    if (sign_ == 1) {
      return static_cast<int64_t>(result);
    }
    return static_cast<int64_t>(~result + 1);
  }

  void read(const std::string &s) {
    sign_ = 1;
    n_.clear();

    int32_t pos {0};

    // Skip and process leading signs
    while ( (pos < static_cast<int32_t>(s.size())) &&
            ((s[static_cast<size_t>(pos)] == '-') || (s[static_cast<size_t>(pos)] == '+'))
          ) {
      if (s[static_cast<size_t>(pos)] == '-') {
        sign_ = -sign_;
      }
      ++pos;
          }

    // Validate that all remaining characters are actual digits
    for (int32_t k = pos; k < static_cast<int32_t>(s.size()); ++k) {
      if (!std::isdigit(static_cast<unsigned char>(s[static_cast<size_t>(k)]))) {
        throw std::invalid_argument("bip::read: string contains non-digit characters");
      }
    }

    int32_t x {0};
    for (int32_t i = static_cast<int32_t>(s.size()) - 1; i >= pos; i -= base_digits_) {
      x = 0;
      for (int32_t j = std::max(pos, i - base_digits_ + 1); j <= i; ++j) {
        x = x * 10 + s[static_cast<size_t>(j)] - '0';
      }
      n_.push_back(x);
    }
    trim();
  }

  static bigint gcd(const bigint& a, const bigint& b) {
    bigint x = a.abs();
    bigint y = b.abs();

    while (!y.isZero()) {
      // Use move semantics to avoid copying the large internal vectors
      // during the swap operation.
      bigint temp = std::move(y);
      y = x % temp;
      x = std::move(temp);
    }

    return x;
  }

/* Not used so far, so commented out
  static bigint lcm(const bigint& a, const bigint& b) {
    // Prevent division by zero if gcd() returns 0, 
    // and correctly define the LCM involving zero as 0.
    if (a.isZero() || b.isZero()) {
      return ZERO;
    }
    return (a / gcd(a, b)) * b;
  }
*/

 private:
  friend std::tuple<bigint, bigint> divmod(const bigint& a1, const bigint& b1);

  // Subtracts (b * d) * (base^offset) from *this.
  // Handles borrows and cleanly underflows to a negative value if b*d > *this.
  void sub_scaled(const bigint& b, int32_t d, size_t offset) {
    if (d == 0) return;
    int64_t carry = 0;

    // Prevent infinite loops when b * d > *this by strictly bounding iterations.
    // b * d produces at most b.n_.size() + 1 digits.
    size_t max_len = std::max(n_.size() > offset ? n_.size() - offset : 0, b.n_.size() + 1);

    while (n_.size() < max_len + offset) {
      n_.push_back(0);
    }

    for (size_t i = 0; i < max_len; ++i) {
      size_t current_idx = i + offset;
      int64_t val = (i < b.n_.size() ? static_cast<int64_t>(b.n_[i]) * d : 0) + carry;

      if (n_[current_idx] < (val % base_)) {
        n_[current_idx] = static_cast<int32_t>(n_[current_idx] + base_ - (val % base_));
        carry = (val / base_) + 1;
      } else {
        n_[current_idx] -= static_cast<int32_t>(val % base_);
        carry = (val / base_);
      }
    }

    // If a carry remains after evaluating all available digits, it indicates an underflow
    // meaning the result of the subtraction is natively negative.
    if (carry > 0) {
      sign_ = -sign_;
      int32_t c_borrow = 0;

      // Apply base_ complement to convert the two's complement form into absolute magnitude
      for (size_t i = 0; i < n_.size(); ++i) {
        int32_t diff = 0 - n_[i] - c_borrow;
        if (diff < 0) {
          n_[i] = diff + base_;
          c_borrow = 1;
        } else {
          n_[i] = diff;
          c_borrow = 0;
        }
      }
    }
    trim();
  }

  // Compares absolute values without allocating memory or making copies.
  // Returns true if |this| >= |rhs|, false otherwise.
  [[nodiscard]] bool abs_greater_or_equal(const bigint& rhs) const noexcept {
    if (n_.size() != rhs.n_.size()) {
      return n_.size() > rhs.n_.size();
    }
    
    // Prevent underflow when both vectors are empty (both are zero)
    if (n_.empty()) {
      return true;
    }

    // Compare from most significant block to least significant
    size_t i = n_.size();
    do {
      --i;
      if (n_[i] != rhs.n_[i]) {
        return n_[i] > rhs.n_[i];
      }
    } while (i != 0);
    return true; // They are equal in absolute value
  }

  void trim() noexcept {
    while (!n_.empty() && !n_.back()) {
      n_.pop_back();
    }
    if ( n_.empty() ) {
      sign_ = 1;
    }
  }

  [[nodiscard]] bool isZero() const noexcept {
    return n_.empty() || (n_.size() == 1 && !n_[0]);
  }

  template <typename OutputContainer = vi>
  static OutputContainer convert_base(const vi& a, int32_t old_digits, int32_t new_digits) {
    size_t i;
    vll p(static_cast<size_t>(std::max(old_digits, new_digits) + 1));

    p[0] = 1;
    for (i = 1; i < p.size(); ++i) {
      p[i] = p[i - 1] * 10;
    }

    OutputContainer result;
    int64_t cur {0};
    int32_t cur_digits {0};

    for (i = 0; i < a.size(); ++i) {
      cur += a[i] * p[static_cast<size_t>(cur_digits)];
      cur_digits += old_digits;
      while (cur_digits >= new_digits) {
        result.push_back(static_cast<typename OutputContainer::value_type>(cur % p[static_cast<size_t>(new_digits)]));
        cur /= p[static_cast<size_t>(new_digits)];
        cur_digits -= new_digits;
      }
    }
    result.push_back(static_cast<typename OutputContainer::value_type>(cur));
    while (!result.empty() && !result.back()) {
      result.pop_back();
    }

    return result;
  }

  // See:
  // https://en.wikipedia.org/wiki/Karatsuba_algorithm
  // Normalized coefficients to prevent INT64_MAX overflow
  static vll karatsubaMultiply(std::span<const int64_t> a, std::span<const int64_t> b) {
    const size_t n = a.size();
    vll result(n + n, 0);

    // straight multiplication if the number of digits is below the threshold
    if (n <= karatsubaCutOff_) {
      for (size_t i = 0; i < n; ++i) {
        for (size_t j = 0; j < n; ++j) {
          result[i + j] += (a[i] * b[j]);
        }
      }
      return result;
    }

    const size_t k = n >> 1;

    // O(1) zero-allocation slicing using std::span
    std::span<const int64_t> a1 = a.subspan(0, k);
    std::span<const int64_t> b1 = b.subspan(0, k);
    std::span<const int64_t> a2 = a.subspan(k);
    std::span<const int64_t> b2 = b.subspan(k);

    const vll a1b1 = karatsubaMultiply(a1, b1);
    const vll a2b2 = karatsubaMultiply(a2, b2);

    vll a_sum(k);
    vll b_sum(k);
    int64_t c_a = 0;
    int64_t c_b = 0;

    // Normalize the polynomial additions to strictly bound coefficients to < 10^6
    for (size_t i = 0; i < k; ++i) {
      int64_t sA = a1[i] + a2[i] + c_a;
      c_a = sA / 1'000'000;
      a_sum[i] = sA % 1'000'000;

      int64_t sB = b1[i] + b2[i] + c_b;
      c_b = sB / 1'000'000;
      b_sum[i] = sB % 1'000'000;
    }

    vll r = karatsubaMultiply(a_sum, b_sum);

    // Integrate the carry cross-terms mathematically
    if (c_a > 0 || c_b > 0) {
      if (r.size() < 2 * k + 1) {
        r.resize(2 * k + 1, 0);
      }
      for (size_t i = 0; i < k; ++i) {
        r[i + k] += a_sum[i] * c_b + b_sum[i] * c_a;
      }
      r[2 * k] += c_a * c_b;
    }

    // Combine the results (Subtraction can now yield negative coefficients)
    for (size_t i = 0; i < a1b1.size(); ++i) {
      r[i] -= a1b1[i];
    }
    for (size_t i = 0; i < a2b2.size(); ++i) {
      r[i] -= a2b2[i];
    }

    for (size_t i = 0; i < r.size(); ++i) {
      result[i + k] += r[i];
    }

    for (size_t i = 0; i < a1b1.size(); ++i) {
      result[i] += a1b1[i];
    }
    for (size_t i = 0; i < a2b2.size(); ++i) {
      result[i + n] += a2b2[i];
    }

    return result;
  }
};  // class bigint

size_t numberOfDigits(const bigint& v);
std::tuple<bigint, bigint> divmod(const bigint& a1, const bigint& b1);

// create an object of type bigint and return a std::unique_ptr to it
template <typename... Args>
auto createUniquePtr(Args&&... args) -> std::unique_ptr<bigint> {
  return std::make_unique<bigint>(std::forward<Args>(args)...);
}

bigint operator+(int32_t lhs, const bigint& rhs);
bigint operator-(int32_t lhs, const bigint& rhs);
bigint operator*(int32_t lhs, const bigint& rhs);
bigint operator/(int32_t lhs, const bigint& rhs);
bigint operator%(int32_t lhs, const bigint& rhs);
bool operator>(int32_t lhs, const bigint& rhs) noexcept;
bool operator<(int32_t lhs, const bigint& rhs) noexcept;
bool operator==(int32_t lhs, const bigint& rhs) noexcept;
bool operator>=(int32_t lhs, const bigint& rhs) noexcept;
bool operator<=(int32_t lhs, const bigint& rhs) noexcept;
bool operator!=(int32_t lhs, const bigint& rhs) noexcept;
}  // namespace bip

std::istream& operator>>(std::istream& is, bip::bigint& v);
std::ostream& operator<<(std::ostream& os, const bip::bigint& v);
