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
  memvarBase(const memvarBase&& rhs) = delete;
  memvarBase& operator=(const memvarBase&& rhs) = delete;

  capacityType getHistoryCapacity() const noexcept;

 protected:
  static const capacityType historyCapacityDefault {10};
  const capacityType historyCapacity_ {historyCapacityDefault};

  memvarBase() = default;
  ~memvarBase() = default;

  explicit memvarBase(const capacityType historyCapacity) noexcept;
};  // memvarBase

// memvar
// a variable with memory of old values
// only strings, integral or floating point numbers allowed
template <typename T>
class memvar final : public memvarBase
{
 private:
  using memvarHistory = std::deque<T>;

  mutable memvarHistory memo_ {};

  static void checkType()
  {
    static_assert( (true == std::is_integral<T>::value ||
                    true == std::is_floating_point<T>::value ||
                    true == is_string<T>::value),
                  "String, integral or floating point types required.");
  }

  static void checkStringNotAllowed()
  {
    static_assert((false == is_string<T>::value),
                  "string not allowed for operator");
  }

  inline void setValue(const T& value) const noexcept
  {
    memo_.push_front(value);
    if ( static_cast<capacityType>(memo_.size()) > getHistoryCapacity() )
    {
      memo_.pop_back();
    }
  }

  inline T getValue() const noexcept
  {
    return memo_.at(0);
  }

  inline T incr1() const noexcept
  {
    T newValue = getValue() + 1;

    setValue(newValue);
    return newValue;    
  }

  inline T decr1() const noexcept
  {
    T newValue = getValue() - 1;

    setValue(newValue);
    return newValue;    
  }

  void printer (const memvarHistory& history, const bool printReverse = false) const noexcept
  {
    if ( true == history.empty() )
    {
      return;
    }

    static auto printItem = [] (const T& item)
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
    setValue(T{});
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
    setValue(value);
  }

  // we don't want these objects allocated on the heap
  void* operator new(std::size_t) = delete;
  void operator delete(void*) = delete;
  ~memvar() = default;
  memvar(const memvar& rhs) = delete;
  memvar(const memvar&& rhs) = delete;
  memvar& operator=(const memvar&& rhs) = delete;

  memvar& operator=(const T& rhs) noexcept
  {
    setValue(rhs);
    return *this;
  }

  memvar& operator=(const memvar& rhs)
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

  T operator()() const noexcept
  {
    return getValue();
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
    return incr1();
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
    return decr1();
  }

  void printHistoryData() const noexcept
  {
    // print history in order (from newest/last value to oldest/first value)
    printer(memo_);
  }

  void printReverseHistoryData() const noexcept
  {
    // print history in reverse order (from oldest/first value to newest/last value)
    printer(memo_, true);
  }

  auto getHistorySize() const noexcept
  {
    return memo_.size();
  }

  void clearHistory() const noexcept
  {
    memo_.erase(std::begin(memo_) + 1, std::end(memo_));
    memo_.shrink_to_fit();
  }

  auto isHistoryFull() const noexcept
  {
    return static_cast<capacityType>(memo_.size()) >= historyCapacity_; 
  }

  auto getMemVarHistory () const noexcept
  {
    return memo_;
  }

  auto getHistoryValue(const capacityType index) const noexcept -> historyValue
  {
    if ( (index < static_cast<capacityType>(getHistorySize())) && (index >= 0) )
    {
      return std::make_tuple(memo_.at(static_cast<size_t>(index)), false);
    }
    return std::make_tuple(T{}, true);
  }
};  // memvar

template <typename T>
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

template <typename T>
inline constexpr
bool operator==(const memvar::memvar<T>& mv1, const memvar::memvar<T>& mv2) noexcept
{
  return mv1() == mv2();
}

template <typename T>
inline constexpr
bool operator!=(const memvar::memvar<T>& mv1, const memvar::memvar<T>& mv2) noexcept
{
  return mv1() != mv2();
}

template <typename T>
inline constexpr
bool operator>(const memvar::memvar<T>& mv1, const memvar::memvar<T>& mv2) noexcept
{
  return mv1() > mv2();
}

template <typename T>
inline constexpr
bool operator<(const memvar::memvar<T>& mv1, const memvar::memvar<T>& mv2) noexcept
{
  return mv1() < mv2();
}

template <typename T>
inline constexpr
bool operator>=(const memvar::memvar<T>& mv1, const memvar::memvar<T>& mv2) noexcept
{
  return mv1() >= mv2();
}

template <typename T>
inline constexpr
bool operator<=(const memvar::memvar<T>& mv1, const memvar::memvar<T>& mv2) noexcept
{
  return mv1() <= mv2();
}

template <typename T>
inline constexpr
memvar::memvar<T>& operator+=(const memvar::memvar<T>& mv1, const memvar::memvar<T>& mv2) noexcept
{
  mv1() += mv2();
  return mv1;
}

template <typename T>
inline constexpr
memvar::memvar<T>& operator-=(const memvar::memvar<T>& mv1, const memvar::memvar<T>& mv2) noexcept
{
  mv1() -= mv2();
  return mv1;
}

template <typename T>
inline constexpr
memvar::memvar<T>& operator*=(const memvar::memvar<T>& mv1, const memvar::memvar<T>& mv2) noexcept
{
  mv1() *= mv2();
  return mv1;
}

template <typename T>
inline constexpr
memvar::memvar<T>& operator/=(const memvar::memvar<T>& mv1, const memvar::memvar<T>& mv2) noexcept
{
  mv1() /= mv2();
  return mv1;
}
#endif /* MEMVAR_H */
