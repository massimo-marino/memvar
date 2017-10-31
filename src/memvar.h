/* 
 * File:   memvar.h
 * Author: massimo
 *
 * Created on October 17, 2017, 1:50 PM
 */

#ifndef MEMVAR_H
#define MEMVAR_H

#include "is_string.h"
#include <iostream>
#include <tuple>
#include <deque>
#include <algorithm>
////////////////////////////////////////////////////////////////////////////////
namespace memvar
{
class memvarBase
{
 public:
  // capacityType: this type must be signed
  using capacityType = int64_t;

  // we don't want these objects allocated on the heap
  void* operator new(std::size_t) = delete;
  void operator delete(void*) = delete;
  memvarBase(const memvarBase& rhs) = delete;
  memvarBase& operator=(const memvarBase& rhs) = delete;
  memvarBase(memvarBase&& rhs) = delete;
  memvarBase& operator=(memvarBase&& rhs) = delete;

  inline capacityType getHistoryCapacity() const noexcept
  {
    return historyCapacity_;
  }

 protected:
  static const capacityType historyCapacityDefault {10};
  const capacityType historyCapacity_ {historyCapacityDefault};

  memvarBase() = default;
  ~memvarBase() = default;

  explicit memvarBase(const capacityType historyCapacity) noexcept;
};  // memvarBase

// memvar
// a variable with memory of old values
// only strings, integral or floating point types allowed
template <typename T>
class memvar final : public memvarBase
{
 private:
  using memvarHistory = std::deque<T>;

  mutable memvarHistory memo_ {};

  inline constexpr
  auto& getMemVarHistory_ref () const noexcept
  {
    return memo_;
  }

  inline static
  void checkType()
  {
    static_assert( (true == std::is_integral<T>::value ||
                    true == std::is_floating_point<T>::value ||
                    true == is_string<T>::value),
                  "String, integral or floating point types required.");
  }

  inline static
  void checkStringNotAllowed()
  {
    static_assert((false == is_string<T>::value),
                  "string not allowed for operator");
  }

  inline
  void setValue(const T& value) const noexcept
  {
    getMemVarHistory_ref().push_front(value);
    if ( static_cast<capacityType>(getHistorySize()) > getHistoryCapacity() )
    {
      getMemVarHistory_ref().pop_back();
    }
  }

  inline constexpr
  T incr1() const noexcept
  {
    T newValue = getValue() + 1;

    setValue(newValue);
    return newValue;    
  }

  inline constexpr
  void voidIncr1() const noexcept
  {
    setValue(getValue() + 1);
  }

  inline constexpr
  T decr1() const noexcept
  {
    T newValue = getValue() - 1;

    setValue(newValue);
    return newValue;    
  }

  inline constexpr
  void voidDecr1() const noexcept
  {
    setValue(getValue() - 1);
  }

  void memvarPrinter (const memvarHistory& history, const bool printReverse = false) const noexcept
  {
    if ( true == history.empty() )
    {
      return;
    }

    static auto printItem = [] (const T& item) noexcept
    {
      std::cout << item << " ";
    };

    std::cout << "[ ";
    if ( false == printReverse )
    {
      std::for_each(std::begin(history), std::end(history), printItem);
    }
    else
    {
      std::for_each(std::rbegin(history), std::rend(history), printItem);
    }
    std::cout << "]" << '\n';
  }

 public:
  using historyValue = std::tuple<T, bool>;

  explicit memvar() noexcept
  :
  memvarBase()
  {
    checkType();
    memo_.push_front(T{});
  }

  explicit memvar(const T& value,
                  const capacityType historyCapacity = historyCapacityDefault) noexcept(false)
  :
  memvarBase(historyCapacity)
  {
    checkType();
    if ( historyCapacity_ <= 0 )
    {
      throw std::invalid_argument("ERROR: The history capacity must not be zero or negative");
    }
    memo_.push_front(value);
  }

  // we don't want these objects allocated on the heap
  void* operator new(std::size_t) = delete;
  void operator delete(void*) = delete;
  ~memvar() = default;
  memvar(const memvar& rhs) = delete;
  memvar(memvar&& rhs) = delete;
  memvar& operator=(memvar&& rhs) = delete;

  // conversion operator from memvar::memvar<T> to T
  operator T() const noexcept
  {
    return getValue();
  }

  T operator()() const noexcept
  {
    return getValue();
  }
  T operator()(const capacityType index) noexcept
  {
    return std::get<T>(getHistoryValue(index));
  }

  inline constexpr
  T getValue() const noexcept
  {
    return getMemVarHistory_ref().at(0);
  }

  memvar& operator=(const T& rhs) noexcept
  {
    setValue(rhs);
    return *this;
  }
  memvar& operator=(const memvar::memvar<T>& rhs) noexcept
  {
    setValue(rhs.getValue());
    return *this;
  }

  memvar& operator+=(const T& rhs) noexcept
  {
    setValue(getValue() + rhs);
    return *this;
  }
  memvar& operator+=(const memvar& rhs) noexcept
  {
    setValue(getValue() + rhs.getValue());
    return *this;
  }

  memvar& operator-=(const T& rhs) noexcept
  {
    checkStringNotAllowed();
    setValue(getValue() - rhs);
    return *this;
  }
  memvar& operator-=(const memvar& rhs) noexcept
  {
    checkStringNotAllowed();
    setValue(getValue() - rhs.getValue());
    return *this;
  }

  memvar& operator*=(const T& rhs) noexcept
  {
    checkStringNotAllowed();
    setValue(getValue() * rhs);
    return *this;
  }
  memvar& operator*=(const memvar& rhs) noexcept
  {
    checkStringNotAllowed();
    setValue(getValue() * rhs.getValue());
    return *this;
  }

  memvar& operator/=(const T& rhs) noexcept
  {
    checkStringNotAllowed();
    setValue(getValue() / rhs);
    return *this;
  }
  memvar& operator/=(const memvar& rhs) noexcept
  {
    checkStringNotAllowed();
    setValue(getValue() / rhs.getValue());
    return *this;
  }

  // ++mv
  T operator++() const noexcept
  {
    checkStringNotAllowed();
    return incr1();
  }
  // mv++
  T operator++([[maybe_unused]] int dummy) const noexcept
  {
    checkStringNotAllowed();
    voidIncr1();
    return std::get<T>(getHistoryValue(1));
  }

  // --mv
  T operator--() const noexcept
  {
    checkStringNotAllowed();
    return decr1();
  }
  // mv--
  T operator--([[maybe_unused]] int dummy) const noexcept
  {
    checkStringNotAllowed();
    voidDecr1();
    return std::get<T>(getHistoryValue(1));
  }

  inline
  void printHistoryData() const noexcept
  {
    // print history in order (from newest/last value to oldest/first value)
    memvarPrinter(getMemVarHistory_ref());
  }

  inline
  void printReverseHistoryData() const noexcept
  {
    // print history in reverse order (from oldest/first value to newest/last value)
    memvarPrinter(getMemVarHistory_ref(), true);
  }

  inline constexpr
  auto getHistorySize() const noexcept
  {
    return getMemVarHistory_ref().size();
  }

  inline
  void clearHistory() const noexcept
  {
    getMemVarHistory_ref().erase(std::begin(getMemVarHistory_ref()) + 1, std::end(getMemVarHistory_ref()));
    getMemVarHistory_ref().shrink_to_fit();
  }

  inline constexpr
  auto isHistoryFull() const noexcept
  {
    return static_cast<capacityType>(getHistorySize()) >= getHistoryCapacity();
  }

  inline constexpr
  auto getMemVarHistory () const noexcept
  {
    return memo_;
  }

  inline constexpr
  auto getHistoryValue(const capacityType index) const noexcept -> historyValue
  {
    if ( (index < static_cast<capacityType>(getHistorySize())) && (index >= 0) )
    {
      return std::make_tuple(getMemVarHistory_ref().at(static_cast<size_t>(index)), false);
    }
    return std::make_tuple(T{}, true);
  }

  inline constexpr
  auto getHistoryMinMax() const noexcept
  {
    auto result = std::minmax_element(getMemVarHistory_ref().begin(), getMemVarHistory_ref().end());
    return std::make_tuple(std::get<T>(getHistoryValue(result.first - getMemVarHistory_ref().begin())),
                           std::get<T>(getHistoryValue(result.second - getMemVarHistory_ref().begin())));
  }
};  // memvar

template <typename T>
inline constexpr
T getHistoryValue(const memvar<T>& mv, const memvarBase::capacityType index) noexcept
{
  return std::get<T>(mv.getHistoryValue(index));
}
}  // namespace memvar

template <typename T>
inline
std::ostream& operator<<(std::ostream& os, const memvar::memvar<T>& mv)
{
   return os << mv.getValue();
}

// operator==
template <typename T>
inline constexpr
bool operator==(const memvar::memvar<T>& mv1, const memvar::memvar<T>& mv2) noexcept
{
  return mv1() == mv2();
}
template <typename T>
inline constexpr
bool operator==(const memvar::memvar<T>& mv, const T& v) noexcept
{
  return mv() == v;
}
template <typename T>
inline constexpr
bool operator==(const T& v, const memvar::memvar<T>& mv) noexcept
{
  return v == mv();
}

// operator!=
template <typename T>
inline constexpr
bool operator!=(const memvar::memvar<T>& mv1, const memvar::memvar<T>& mv2) noexcept
{
  return mv1() != mv2();
}
template <typename T>
inline constexpr
bool operator!=(const memvar::memvar<T>& mv, const T& v) noexcept
{
  return mv() != v;
}
template <typename T>
inline constexpr
bool operator!=(const T& v, const memvar::memvar<T>& mv) noexcept
{
  return v != mv();
}

// operator>
template <typename T>
inline constexpr
bool operator>(const memvar::memvar<T>& mv1, const memvar::memvar<T>& mv2) noexcept
{
  return mv1() > mv2();
}
template <typename T>
inline constexpr
bool operator>(const memvar::memvar<T>& mv, const T& v) noexcept
{
  return mv() > v;
}
template <typename T>
inline constexpr
bool operator>(const T& v, const memvar::memvar<T>& mv) noexcept
{
  return v > mv();
}

// operator<
template <typename T>
inline constexpr
bool operator<(const memvar::memvar<T>& mv1, const memvar::memvar<T>& mv2) noexcept
{
  return mv1() < mv2();
}
template <typename T>
inline constexpr
bool operator<(const memvar::memvar<T>& mv, const T& v) noexcept
{
  return mv() < v;
}
template <typename T>
inline constexpr
bool operator<(const T& v, const memvar::memvar<T>& mv) noexcept
{
  return v < mv();
}

// operator>=
template <typename T>
inline constexpr
bool operator>=(const memvar::memvar<T>& mv1, const memvar::memvar<T>& mv2) noexcept
{
  return mv1() >= mv2();
}
template <typename T>
inline constexpr
bool operator>=(const T& v, const memvar::memvar<T>& mv) noexcept
{
  return v >= mv();
}
template <typename T>
inline constexpr
bool operator>=(const memvar::memvar<T>& mv, const T& v) noexcept
{
  return mv() >= v;
}

// operator<=
template <typename T>
inline constexpr
bool operator<=(const memvar::memvar<T>& mv1, const memvar::memvar<T>& mv2) noexcept
{
  return mv1() <= mv2();
}
template <typename T>
inline constexpr
bool operator<=(const memvar::memvar<T>& mv, const T& v) noexcept
{
  return mv() <= v;
}
template <typename T>
inline constexpr
bool operator<=(const T& v, const memvar::memvar<T>& mv) noexcept
{
  return v <= mv();
}

// operator+=
template <typename T>
inline constexpr
memvar::memvar<T>& operator+=(const memvar::memvar<T>& mv1, const memvar::memvar<T>& mv2) noexcept
{
  mv1() += mv2();
  return mv1;
}
template <typename T>
inline constexpr
memvar::memvar<T>& operator+=(const memvar::memvar<T>& mv, const T& v) noexcept
{
  mv() += v;
  return mv;
}
template <typename T>
inline constexpr
T operator+=(const T& v, const memvar::memvar<T>& mv) noexcept
{
  v += mv();
  return v;
}

// operator-=
template <typename T>
inline constexpr
memvar::memvar<T>& operator-=(const memvar::memvar<T>& mv1, const memvar::memvar<T>& mv2) noexcept
{
  mv1() -= mv2();
  return mv1;
}
template <typename T>
inline constexpr
memvar::memvar<T>& operator-=(const memvar::memvar<T>& mv, const T& v) noexcept
{
  mv() -= v;
  return mv;
}
template <typename T>
inline constexpr
T operator-=(const T& v, const memvar::memvar<T>& mv) noexcept
{
  v -= mv();
  return v;
}

// operator*=
template <typename T>
inline constexpr
memvar::memvar<T>& operator*=(const memvar::memvar<T>& mv1, const memvar::memvar<T>& mv2) noexcept
{
  mv1() *= mv2();
  return mv1;
}
template <typename T>
inline constexpr
memvar::memvar<T>& operator*=(const memvar::memvar<T>& mv, const T& v) noexcept
{
  mv() *= v;
  return mv;
}
template <typename T>
inline constexpr
T operator*=(const T& v, const memvar::memvar<T>& mv) noexcept
{
  v *= mv();
  return v;
}

// operator/=
template <typename T>
inline constexpr
memvar::memvar<T>& operator/=(const memvar::memvar<T>& mv1, const memvar::memvar<T>& mv2) noexcept
{
  mv1() /= mv2();
  return mv1;
}
template <typename T>
inline constexpr
memvar::memvar<T>& operator/=(const memvar::memvar<T>& mv, const T& v) noexcept
{
  mv() /= v;
  return mv;
}
template <typename T>
inline constexpr
T operator/=(const T& v, const memvar::memvar<T>& mv) noexcept
{
  v /= mv();
  return v;
}

// operator+
template <typename T>
inline constexpr
T operator+(const memvar::memvar<T>& lhs,
            const memvar::memvar<T>& rhs) noexcept
{
  return lhs.getValue() + rhs.getValue();
}
template <typename T>
inline constexpr
T operator+(const memvar::memvar<T>& lhs,
            const T& rhs) noexcept
{
  return lhs.getValue() + rhs;
}
template <typename T>
inline constexpr
T operator+(const T& lhs,
            const memvar::memvar<T>& rhs) noexcept
{
  return lhs + rhs.getValue();
}

// operator-
template <typename T>
inline constexpr
T operator-(const memvar::memvar<T>& lhs,
            const memvar::memvar<T>& rhs) noexcept
{
  return lhs.getValue() - rhs.getValue();
}
template <typename T>
inline constexpr
T operator-(const memvar::memvar<T>& lhs,
            const T& rhs) noexcept
{
  return lhs.getValue() - rhs;
}
template <typename T>
inline constexpr
T operator-(const T& lhs,
            const memvar::memvar<T>& rhs) noexcept
{
  return lhs - rhs.getValue();
}

// operator*
template <typename T>
inline constexpr
T operator*(const memvar::memvar<T>& lhs,
            const memvar::memvar<T>& rhs) noexcept
{
  return lhs.getValue() * rhs.getValue();
}
template <typename T>
inline constexpr
T operator*(const memvar::memvar<T>& lhs,
            const T& rhs) noexcept
{
  return lhs.getValue() * rhs;
}
template <typename T>
inline constexpr
T operator*(const T& lhs,
            const memvar::memvar<T>& rhs) noexcept
{
  return lhs * rhs.getValue();
}

// operator/
template <typename T>
inline constexpr
T operator/(const memvar::memvar<T>& lhs,
            const memvar::memvar<T>& rhs) noexcept
{
  return lhs.getValue() / rhs.getValue();
}
template <typename T>
inline constexpr
T operator/(const memvar::memvar<T>& lhs,
            const T& rhs) noexcept
{
  return lhs.getValue() / rhs;
}
template <typename T>
inline constexpr
T operator/(const T& lhs,
            const memvar::memvar<T>& rhs) noexcept
{
  return lhs / rhs.getValue();
}

template <typename T>
struct std::is_integral<memvar::memvar<T>>
{
  static inline const bool value = true;
};
#endif /* MEMVAR_H */
