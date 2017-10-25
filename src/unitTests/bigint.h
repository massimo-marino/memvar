// bigint.h
//
// This implementation is a 'fork' from:
// https://sites.google.com/site/indy256/algo_cpp/bigint
//
#include <iomanip>
#include <vector>
////////////////////////////////////////////////////////////////////////////////
namespace bigint
{
using vi = std::vector<int32_t>;
using vll = std::vector<long long>;

class bigint final
{
 private:
  vi n_ {};
  // for padding: unused
  [[maybe_unused]] const char dummy_[4] {};
  int32_t sign_ {1};

  // base and base_digits must be consistent
  static inline const int32_t base {1'000'000'000};

 public:
   static inline const int32_t base_digits {9};

   auto get_n() const noexcept
   {
     return n_;
   }

   auto get_sign() const noexcept
   {
     return sign_;
   }

  ~bigint() = default;

  explicit bigint() = default;

  explicit bigint(const std::string& s) noexcept
  {
    read(s);
  }

  // default move ctor
  bigint(bigint&& rhs) = default;  

  // default move assignment operator
  bigint& operator=(bigint&& rhs) = default;

  bigint operator*(const bigint& rhs) const noexcept
  {
    const vi a6 = convert_base(this->n_, base_digits, 6);
    const vi b6 = convert_base(rhs.n_, base_digits, 6);
    vll a(a6.begin(), a6.end());
    vll b(b6.begin(), b6.end());

    while (a.size() < b.size())
    {
      a.push_back(0);
    }
    while (b.size() < a.size())
    {
      b.push_back(0);
    }
    while (a.size() & (a.size() - 1))
    {
      a.push_back(0);
      b.push_back(0);
    }

    const vll c = karatsubaMultiply(a, b);
    bigint result;

    result.sign_ = sign_ * rhs.sign_;
    int32_t carry {0};
    for (int32_t i {0}; i < static_cast<int32_t>(c.size()); ++i)
    {
      const long long cur = c[static_cast<size_t>(i)] + carry;

      result.n_.push_back(static_cast<int32_t>(cur % 1'000'000));
      carry = static_cast<int32_t>(cur / 1'000'000);
    }
    result.n_ = convert_base(result.n_, 6, base_digits);
    result.trim();

    return result;
  }

  bigint operator/(const bigint& rhs) const  noexcept
  {
    return std::get<0>(divmod(*this, rhs));
  }

  bigint operator%(const bigint& rhs) const noexcept
  {
    return std::get<1>(divmod(*this, rhs));
  }

  // copy ctor
  bigint(const bigint& rhs) = default;

  // copy assignment
  bigint& operator=(const bigint& rhs) noexcept
  {
    this->n_ = rhs.n_;
    this->sign_ = rhs.sign_;

    return *this;
  }

  void operator=(long long rhs) noexcept
  {
    sign_ = 1;
    if (rhs < 0)
    {
      sign_ = -1;
      rhs = -rhs;
    }
    for (; rhs > 0; rhs = rhs / base)
    {
      n_.push_back(rhs % base);
    }
  }

  bigint(const long long rhs) noexcept
  {
    *this = rhs;
  }

  bool operator>(const bigint& rhs) const noexcept
  {
    return rhs < *this;
  }

  bool operator<=(const bigint& rhs) const noexcept
  {
    return !(rhs < *this);
  }

  bool operator>=(const bigint& rhs) const noexcept
  {
    return !(*this < rhs);
  }

  bool operator==(const bigint& rhs) const noexcept
  {
    return !(*this < rhs) && !(rhs < *this);
  }

  bool operator!=(const bigint& rhs) const noexcept
  {
    return *this < rhs || rhs < *this;
  }

  bigint operator+(const bigint& rhs) const noexcept
  {
    if (sign_ == rhs.sign_)
    {
      bigint result = rhs;
      int32_t carry {0};
      for (size_t i {0};
           i < std::max(n_.size(), rhs.n_.size()) || carry;
           ++i)
      {
        if ( i == result.n_.size() )
        {
          result.n_.push_back(0);
        }
        result.n_[i] += carry + (i < n_.size() ? n_[i] : 0);
        carry = result.n_[i] >= base;
        if (carry)
        {
          result.n_[i] -= base;
        }
      }
      return result;
    }
    return *this - (-rhs);
  }

  bigint operator-(const bigint& rhs) const noexcept
  {
    if (sign_ == rhs.sign_)
    {
      if (abs() >= rhs.abs())
      {
        bigint result = *this;
        int32_t carry {0};
        for (size_t i {0}; i < rhs.n_.size() || carry; ++i)
        {
          result.n_[i] -= carry + (i < rhs.n_.size() ? rhs.n_[i] : 0);
          carry = result.n_[i] < 0;
          if (carry)
          {
            result.n_[i] += base;
          }
        }
        result.trim();
        return result;
      }
      return -(rhs - *this);
    }
    return *this + (-rhs);
  }

  void operator*=(int32_t rhs) noexcept
  {
    if (rhs < 0)
    {
      sign_ = -sign_;
      rhs = -rhs;
    }
    int32_t carry {0};
    for (size_t i {0}; i < n_.size() || carry; ++i)
    {
      if ( i == n_.size() )
      {
        n_.push_back(0);
      }
      const long long cur = n_[i] * static_cast<long long>(rhs) + carry;
      carry = static_cast<int32_t>(cur / base);
      n_[i] = static_cast<int32_t>(cur % base);
    }
    trim();
  }

  bigint operator*(const int32_t rhs) const noexcept
  {
    bigint result = *this;
    result *= rhs;
    return result;
  }

  void operator/=(int32_t rhs) noexcept
  {
    if (rhs < 0)
    {
      sign_ = -sign_;
      rhs = -rhs;
    }
    int32_t rem {0};
    for (int32_t i = static_cast<int32_t>(n_.size()) - 1; i >= 0; --i)
    {
      const long long cur = n_[static_cast<size_t>(i)] + rem * static_cast<long long>(base);
      n_[static_cast<size_t>(i)] = static_cast<int32_t>(cur / rhs);
      rem = static_cast<int32_t>(cur % rhs);
    }
    trim();
  }

  bigint operator/(const int32_t rhs) const noexcept
  {
    bigint result = *this;
    result /= rhs;
    return result;
  }

  int32_t operator%(int32_t rhs) const noexcept
  {
    if (rhs < 0)
    {
      rhs = -rhs;
    }
    int32_t m {0};
    for (int32_t i = static_cast<int32_t>(n_.size()) - 1; i >= 0; --i)
    {
      m = (n_[static_cast<size_t>(i)] + m * static_cast<long long>(base)) % rhs;
    }
    return m * sign_;
  }

  void operator+=(const bigint& rhs) noexcept
  {
    *this = *this + rhs;
  }

  void operator-=(const bigint& rhs) noexcept
  {
    *this = *this - rhs;
  }

  void operator*=(const bigint& rhs) noexcept
  {
    *this = *this * rhs;
  }

  void operator/=(const bigint& rhs) noexcept
  {
    *this = *this / rhs;
  }

  bool operator<(const bigint& rhs) const noexcept
  {
    if (sign_ != rhs.sign_)
    {
      return sign_ < rhs.sign_;
    }
    if (n_.size() != rhs.n_.size())
    {
      return static_cast<int32_t>(n_.size()) * sign_ < static_cast<int32_t>(rhs.n_.size()) * rhs.sign_;
    }
    for (int32_t i = static_cast<int32_t>(n_.size()) - 1; i >= 0; i--)
    {
      if (n_[static_cast<size_t>(i)] != rhs.n_[static_cast<size_t>(i)])
      {
        return n_[static_cast<size_t>(i)] * sign_ < rhs.n_[static_cast<size_t>(i)] * sign_;
      }
    }
    return false;
  }

  bigint operator-() const noexcept
  {
    bigint result = *this;
    result.sign_ = -sign_;
    return result;
  }

  bigint abs() const noexcept
  {
    bigint result = *this;
    result.sign_ *= result.sign_;
    return result;
  }

  long long longValue() const noexcept
  {
    long long result {0};

    for (int32_t i = static_cast<int32_t>(n_.size()) - 1; i >= 0; --i)
    {
      result = result * base + n_[static_cast<size_t>(i)];
    }
    return result * sign_;
  }

  void read(const std::string &s) noexcept
  {
    sign_ = 1;
    n_.clear();

    int32_t pos {0};

    while ( (pos < static_cast<int32_t>(s.size())) &&
            ((s[static_cast<size_t>(pos)] == '-') || (s[static_cast<size_t>(pos)] == '+'))
          )
    {
      if (s[static_cast<size_t>(pos)] == '-')
      {
        sign_ = -sign_;
      }
      ++pos;
    }
    for (int32_t i = static_cast<int32_t>(s.size()) - 1; i >= pos; i -= base_digits)
    {
      int32_t x {0};
      for (int32_t j = std::max(pos, i - base_digits + 1); j <= i; j++)
      {
        x = x * 10 + s[static_cast<size_t>(j)] - '0';
      }
      n_.push_back(x);
    }
    trim();
  }

  friend bigint gcd(const bigint& a, const bigint& b) noexcept;
  friend bigint lcm(const bigint& a, const bigint& b) noexcept;

 private:

  friend std::tuple<bigint, bigint> divmod(const bigint& a1, const bigint& b1) noexcept;

  void trim() noexcept
  {
    while (!n_.empty() && !n_.back())
    {
      n_.pop_back();
    }
    if ( n_.empty() )
    {
      sign_ = 1;
    }
  }

  bool isZero() const noexcept
  {
    return n_.empty() || (n_.size() == 1 && !n_[0]);
  }

  static vi convert_base(const vi& a, int32_t old_digits, int32_t new_digits) noexcept
  {
    size_t i;
    vll p(static_cast<size_t>(std::max(old_digits, new_digits) + 1));

    p[0] = 1;
    for (i = 1; i < p.size(); ++i)
    {
      p[i] = p[i - 1] * 10;
    }

    vi result;
    long long cur {0};
    int32_t cur_digits {0};

    for (i = 0; i < a.size(); ++i)
    {
      cur += a[i] * p[static_cast<size_t>(cur_digits)];
      cur_digits += old_digits;
      while (cur_digits >= new_digits)
      {
        result.push_back(static_cast<int32_t>(cur % p[static_cast<size_t>(new_digits)]));
        cur /= p[static_cast<size_t>(new_digits)];
        cur_digits -= new_digits;
      }
    }
    result.push_back(static_cast<int32_t>(cur));
    while (!result.empty() && !result.back())
    {
      result.pop_back();
    }

    return result;
  }

  static vll karatsubaMultiply(const vll& a, const vll& b) noexcept
  {
    const size_t n = a.size();
    size_t i {0};
    size_t j {0};
    vll result(n + n);

    if ( n <= 32 )
    {
      for (i = 0; i < n; ++i)
      {
        for (j = 0; j < n; ++j)
        {
          result[i + j] += (a[i] * b[j]);
        }
      }
      return result;
    }

    const long k = n >> 1;
    const vll a1(a.begin(), a.begin() + k);
    vll a2(a.begin() + k, a.end());
    const vll b1(b.begin(), b.begin() + k);
    vll b2(b.begin() + k, b.end());

    const vll a1b1 = karatsubaMultiply(a1, b1);
    const vll a2b2 = karatsubaMultiply(a2, b2);

    for (i = 0; i < static_cast<size_t>(k); ++i)
    {
      a2[i] += a1[i];
      b2[i] += b1[i];
    }

    vll r = karatsubaMultiply(a2, b2);
    for (i = 0; i < a1b1.size(); ++i)
    {
      r[i] -= a1b1[i];
    }
    for (i = 0; i < a2b2.size(); ++i)
    {
      r[i] -= a2b2[i];
    }

    for (i = 0; i < r.size(); ++i)
    {
      result[i + static_cast<size_t>(k)] += r[i];
    }
    for (i = 0; i < a1b1.size(); ++i)
    {
      result[i] += a1b1[i];
    }
    for (i = 0; i < a2b2.size(); ++i)
    {
      result[i + n] += a2b2[i];
    }
    return result;
  }
};  // class bigint

size_t numberOfDigits(const bigint::bigint& v) noexcept;
}  // namespace bigint

std::istream& operator>>(std::istream& os, bigint::bigint& v);
std::ostream& operator<<(std::ostream& os, const bigint::bigint& v);

template <>
struct std::is_integral<bigint::bigint>
{
  static const bool value = true;
};
