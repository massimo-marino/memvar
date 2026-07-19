//
// bigint.cpp
//
#include "bigint.h"
#include <array>
////////////////////////////////////////////////////////////////////////////////
std::istream& operator>>(std::istream& is, bip::bigint& v) {
  // 1. Use sentry to handle whitespace skipping and stream initialization
  std::istream::sentry sentry(is);
  if (!sentry) {
    return is;
  }

  // 2. Peek to check for a sign, and consume if present
  std::string s;
  char c;

  if (is.peek() == '-' || is.peek() == '+') {
    is.get(c);
    s += c;
  }

  // 3. Extract digits one by one
  bool has_digits = false;
  while (is.get(c)) {
    if (std::isdigit(static_cast<unsigned char>(c))) {
      s += c;
      has_digits = true;
    } else {
      // 4. Hit a non-digit: put it back into the stream
      is.unget();
      break;
    }
  }

  // 5. Validation: Ensure we actually read a valid number
  if (has_digits) {
    v.read(s);

    // If extraction stopped strictly due to EOF, the stream sets both
    // eofbit and failbit. We clear the failbit to indicate a successful read.
    if (is.eof()) {
      is.clear(is.rdstate() & ~std::ios_base::failbit);
    }
  } else {
    // Set failbit if no digits were extracted
    is.setstate(std::ios_base::failbit);
  }

  return is;
}

std::ostream& operator<<(std::ostream& os, const bip::bigint& v) {
  const auto& n = v.get_n();

  // Fix: Use integer 0 instead of char '0' so standard formatting 
  // (like width and showpos) naturally applies to zero.
  if (n.empty()) {
    return os << 0;
  }

  // 1. Calculate the total length of the formatted number
  size_t total_length = bip::numberOfDigits(v);
  const bool is_negative = (v.get_sign() == -1);
  
  // Detect if we need to print a positive sign
  const bool show_pos = !is_negative && (os.flags() & std::ios_base::showpos);

  if (is_negative || show_pos) {
    total_length++;
  }

  // 2. Determine required padding based on os.width()
  const std::streamsize width = os.width();
  std::streamsize padding = 0;
  if (width > static_cast<std::streamsize>(total_length)) {
    padding = width - static_cast<std::streamsize>(total_length);
  }

  // 3. Extract formatting flags and the current fill character
  const std::ios_base::fmtflags adjustfield = os.flags() & std::ios_base::adjustfield;
  const char fill_char = os.fill();

  // Crucial: Reset the stream's width to 0 so our manual output pieces aren't padded
  os.width(0);

  // Helper lambda to apply padding dynamically without allocation
  auto apply_padding = [&]() {
    for (std::streamsize i = 0; i < padding; ++i) {
      os.put(fill_char);
    }
  };

  const bool is_left = (adjustfield == std::ios_base::left);
  const bool is_internal = (adjustfield == std::ios_base::internal);

  // Print left-side padding (Right alignment is standard/default if no flags are set)
  if (!is_left && !is_internal) {
    apply_padding();
  }

  // Print the sign BEFORE internal padding
  if (is_negative) {
    os.put('-');
  } else if (show_pos) {
    os.put('+');
  }

  // Print internal padding (Between the sign and the digits)
  if (is_internal) {
    apply_padding();
  }

  // Use the existing class constant
  constexpr int digits_per_block = bip::bigint::base_digits_;

  // Temporarily clear showpos so os << n.back() doesn't print a second '+'
  std::ios_base::fmtflags original_flags = os.flags();
  if (show_pos) {
    os.flags(original_flags & ~std::ios_base::showpos);
  }

  // Print the Most Significant Block (MSB) normally
  os << n.back();

  // Restore the original stream flags
  if (show_pos) {
    os.flags(original_flags);
  }

  // Optimized manual conversion for the remaining digits_per_block-digit blocks
  // Use a stack-allocated array for the buffer
  std::array<char, digits_per_block> buffer;

  for (int32_t i = static_cast<int32_t>(n.size()) - 2; i >= 0; --i) {
    int32_t val = n[static_cast<size_t>(i)];

    // Fill buffer from back to front
    for (int j = digits_per_block - 1; j >= 0; --j) {
      constexpr int decimal_base = 10;

      buffer[static_cast<size_t>(j)] = '0' + (val % decimal_base);
      val /= decimal_base;
    }

    // Write directly to the stream without string formatting overhead
    os.write(buffer.data(), buffer.size());
  }

  // Print right-side padding
  if (is_left) {
    apply_padding();
  }

  return os;
}

namespace bip
{
const bigint& get_zero() {
  static const bigint zero{"0"};
  return zero;
}

bigint operator+(int32_t lhs, const bigint& rhs) {
  return bigint(static_cast<int64_t>(lhs)) + rhs;
}

bigint operator-(int32_t lhs, const bigint& rhs) {
  return bigint(static_cast<int64_t>(lhs)) - rhs;
}

bigint operator*(int32_t lhs, const bigint& rhs) {
  // Multiplication is commutative, so we can just reverse the operands
  // to utilize the existing bigint::operator*(int32_t) member function.
  return rhs * lhs;
}

bigint operator/(int32_t lhs, const bigint& rhs) {
  // Convert the primitive integer to a bigint and use the member operator
  return bigint(static_cast<int64_t>(lhs)) / rhs;
}

bigint operator%(int32_t lhs, const bigint& rhs) {
  // Convert the primitive integer to a bigint and use the member operator
  return bigint(static_cast<int64_t>(lhs)) % rhs;
}

bool operator>(int32_t lhs, const bigint& rhs) noexcept {
  return rhs < lhs;
}

bool operator<(int32_t lhs, const bigint& rhs) noexcept {
  return rhs > lhs;
}

bool operator==(int32_t lhs, const bigint& rhs) noexcept {
  return rhs == lhs;
}

bool operator>=(int32_t lhs, const bigint& rhs) noexcept {
  return !(lhs < rhs);
}

bool operator<=(int32_t lhs, const bigint& rhs) noexcept {
  return !(lhs > rhs);
}

bool operator!=(int32_t lhs, const bigint& rhs) noexcept {
  return !(lhs == rhs);
}

std::tuple<bigint, bigint> divmod(const bigint& a1, const bigint& b1) {
  // if b1 is zero is checked in the callers.
  // Early return if the dividend is zero.
  if (a1.isZero()) {
    return std::make_tuple(get_zero(), get_zero());
  }

  // Quick return if the magnitude of the dividend is strictly less than the divisor
  if (!a1.abs_greater_or_equal(b1)) {
    return std::make_tuple(get_zero(), a1);
  }

  const int32_t norm = bigint::base_ / (b1.n_.back() + 1);
  bigint a = a1.abs() * norm; // `a` will be modified in-place to become the remainder
  const bigint b = b1.abs() * norm;
  bigint q;
  q.n_.resize(a.n_.size(), 0);

  int32_t s1;
  int32_t s2;
  int32_t d;
  size_t i = a.n_.size();

  do {
    --i;
    // Calculate the quotient estimate using the top blocks relative to offset i
    s1 = a.n_.size() <= i + b.n_.size() ? 0 : a.n_[i + b.n_.size()];
    s2 = a.n_.size() <= i + b.n_.size() - 1 ? 0 : a.n_[i + b.n_.size() - 1];
    d = (static_cast<int64_t>(bigint::base_) * s1 + s2) / b.n_.back();

    // Debug instrumentation
    /*
    if (d >= bigint::base_) {
      std::cerr << "CRITICAL: Quotient estimate d=" << d
                << " exceeds base_ " << bigint::base_
                << ". This will cause a hang!" << std::endl;
    }
    */
    // Strictly cap the quotient digit to base_ - 1 to prevent over-subtraction
    if (d >= bigint::base_) {
      d = bigint::base_ - 1;
    }

    // Perform subtraction directly on a shifted window of the dividend
    a.sub_scaled(b, d, i);

    // Restore the remainder if the estimated quotient digit `d` was too large.
    // (a.sign_ becomes -1 on underflow. Using += correctly processes negative + positive).
    while (a.sign_ == -1) {
      bigint b_shifted = b;
      if (i > 0) {
        // Shift is heavily amortized as this fallback loop is extremely rarely entered
        b_shifted.n_.insert(b_shifted.n_.begin(), i, 0);
      }
      a += b_shifted;
      --d;
    }
    q.n_[i] = d;
  } while (i != 0);

  q.sign_ = a1.sign_ * b1.sign_;
  a.sign_ = a1.sign_; // Inherit the remainder's sign from the original dividend
  q.trim();
  a.trim();

  return std::make_tuple(q, a / norm);
}

size_t numberOfDigits(const bigint& v) {
  const auto& n = v.get_n();

  // Handle the edge case for zero
  if (n.empty() || (n.size() == 1 && n[0] == 0)) {
    return 1;
  }

  // Calculate digits from all fully packed inner blocks
  size_t digits = (n.size() - 1) * bigint::base_digits_;

  // Calculate the number of digits in the most significant (last) block
  int32_t last_block = n.back();
  while (last_block > 0) {
    digits++;
    last_block /= 10;
  }

  // Note: The original stringstream implementation implicitly counted
  // the '-' character as a "digit" for negative numbers.
  // If you need strict behavioral parity with the old code, uncomment this:
  // if (v.get_sign() == -1)
  // {
  //   digits++;
  // }

  return digits;
}

}  // namespace bip
