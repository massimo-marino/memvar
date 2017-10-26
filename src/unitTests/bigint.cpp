// bigint.cpp
//
// This implementation is a 'fork' from:
// https://sites.google.com/site/indy256/algo_cpp/bigint
//
#include "bigint.h"
////////////////////////////////////////////////////////////////////////////////
std::istream& operator>>(std::istream& os, bigint::bigint& v)
  {
    std::string s {};
    os >> s;
    v.read(s);

    return os;
  }

std::ostream& operator<<(std::ostream& os, const bigint::bigint& v)
{
  if ( (-1) == v.get_sign() )
  {
    os << '-';
  }
  os << (v.get_n().empty() ? 0 : v.get_n().back());
  for (int32_t i = static_cast<int32_t>(v.get_n().size()) - 2; i >= 0; --i)
  {
    os << std::setw(bigint::bigint::base_digits)
       << std::setfill('0')
       << v.get_n()[static_cast<size_t>(i)];
  }
  return os;
}

namespace bigint
{
std::tuple<bigint, bigint> divmod(const bigint& a1, const bigint& b1) noexcept
{
  const int32_t norm = bigint::base / (b1.n_.back() + 1);
  const bigint a = a1.abs() * norm;
  const bigint b = b1.abs() * norm;
  bigint q;
  bigint r;
  q.n_.resize(a.n_.size());

  int32_t s1;
  int32_t s2;
  int32_t d;
  for (int32_t i = static_cast<int32_t>(a.n_.size()) - 1; i >= 0; --i)
  {
    r *= bigint::base;
    r += a.n_[static_cast<size_t>(i)];
    s1 = r.n_.size() <= b.n_.size() ? 0 : r.n_[b.n_.size()];
    s2 = r.n_.size() <= b.n_.size() - 1 ? 0 : r.n_[b.n_.size() - 1];
    d = (static_cast<long long>(bigint::base) * s1 + s2) / b.n_.back();
    r -= (b * d);
    while (r < 0)
    {
      r += b;
      --d;
    }
    q.n_[static_cast<size_t>(i)] = d;
  }

  q.sign_ = a1.sign_ * b1.sign_;
  r.sign_ = a1.sign_;
  q.trim();
  r.trim();
  return std::make_tuple(q, r / norm);
}

bigint gcd(const bigint& a, const bigint& b) noexcept
{
  return b.isZero() ? a : gcd(b, a % b);
}

bigint lcm(const bigint& a, const bigint& b) noexcept
{
  return a / gcd(a, b) * b;
}

size_t numberOfDigits(const bigint::bigint& v) noexcept
{
  std::stringstream s {};
  s << v;
  return s.str().size();
}
}  // namespace bigint
