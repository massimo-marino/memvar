/* 
 * File:   memvar.h
 * Author: massimo
 *
 * Created on October 17, 2017, 1:50 PM
 */
#pragma once

#include "is_string.h"
#include <iostream>
#include <tuple>
#include <deque>
#include <chrono>
#include <algorithm>
////////////////////////////////////////////////////////////////////////////////
namespace memvar
{
class memvarBase
{
 public:
  // capacityType: this type must be signed
  using capacityType = int64_t;

  memvarBase(const memvarBase& rhs) = delete;
  memvarBase& operator=(const memvarBase& rhs) = delete;
  memvarBase(memvarBase&& rhs) = delete;
  memvarBase& operator=(memvarBase&& rhs) = delete;

  inline capacityType getHistoryCapacity() const noexcept
  {
    return historyCapacity_;
  }

 protected:
  static const capacityType historyCapacityDefault_ {10};
  const capacityType historyCapacity_ {historyCapacityDefault_};

  explicit memvarBase(capacityType historyCapacity) noexcept;
  memvarBase() = default;
  virtual ~memvarBase();
};  // memvarBase

// memvar
// a variable with memory of old values
// only strings, integral or floating point types allowed
template <typename T>
class memvar : public memvarBase
{
 protected:
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

  inline constexpr
  T getValue() const noexcept
  {
    return getMemVarHistory_ref().at(0);
  }

  virtual inline
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

  void memvarPrinter (const memvarHistory& history,
                      std::ostream& os = std::cout,
                      const bool printReverse = false,
                      const std::string& separator = " ") const noexcept
  {
    if ( history.empty() )
    {
      return;
    }

    auto printItem = [&separator, &os] (const T& item) noexcept
    {
      os << item << separator;
    };

    os << "[ ";
    if ( printReverse )
    {
      std::for_each(history.crbegin(), history.crend(), printItem);
    }
    else
    {
      std::for_each(history.cbegin(), history.cend(), printItem);
    }
    os << "]" << '\n';
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
                  const capacityType historyCapacity = historyCapacityDefault_) noexcept(false)
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

  memvar(const memvar& rhs) = delete;
  memvar(memvar&& rhs) = delete;
  memvar& operator=(memvar&& rhs) = delete;

  // conversion operator from memvar::memvar<T> to T
  virtual operator T() const noexcept
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

  memvar& operator=(const T& rhs) noexcept
  {
    setValue(rhs);
    return *this;
  }
  memvar& operator=(const memvar& rhs) noexcept
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
  void printHistoryData(std::ostream& os = std::cout, const std::string&& separator = " ") const noexcept
  {
    // print history in order (from newest/last value to oldest/first value)
    memvarPrinter(getMemVarHistory_ref(), os, false, separator);
  }

  inline
  void printReverseHistoryData(std::ostream& os = std::cout, const std::string&& separator = " ") const noexcept
  {
    // print history in reverse order (from oldest/first value to newest/last value)
    memvarPrinter(getMemVarHistory_ref(), os, true, separator);
  }

  inline constexpr
  auto getHistorySize() const noexcept
  {
    return getMemVarHistory_ref().size();
  }

  inline
  void clearHistory() const noexcept
  {
    getMemVarHistory_ref().erase(std::cbegin(getMemVarHistory_ref()) + 1, std::cend(getMemVarHistory_ref()));
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
    auto result = std::minmax_element(getMemVarHistory_ref().cbegin(), getMemVarHistory_ref().cend());
    return std::make_tuple(std::get<T>(getHistoryValue(result.first - getMemVarHistory_ref().cbegin())),
                           std::get<T>(getHistoryValue(result.second - getMemVarHistory_ref().cbegin())));
  }
};  // class memvar

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
   return os << mv();
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
  return lhs() + rhs();
}
template <typename T>
inline constexpr
T operator+(const memvar::memvar<T>& lhs,
            const T& rhs) noexcept
{
  return lhs() + rhs;
}
template <typename T>
inline constexpr
T operator+(const T& lhs,
            const memvar::memvar<T>& rhs) noexcept
{
  return lhs + rhs();
}

// operator-
template <typename T>
inline constexpr
T operator-(const memvar::memvar<T>& lhs,
            const memvar::memvar<T>& rhs) noexcept
{
  return lhs() - rhs();
}
template <typename T>
inline constexpr
T operator-(const memvar::memvar<T>& lhs,
            const T& rhs) noexcept
{
  return lhs() - rhs;
}
template <typename T>
inline constexpr
T operator-(const T& lhs,
            const memvar::memvar<T>& rhs) noexcept
{
  return lhs - rhs();
}

// operator*
template <typename T>
inline constexpr
T operator*(const memvar::memvar<T>& lhs,
            const memvar::memvar<T>& rhs) noexcept
{
  return lhs() * rhs();
}
template <typename T>
inline constexpr
T operator*(const memvar::memvar<T>& lhs,
            const T& rhs) noexcept
{
  return lhs() * rhs;
}
template <typename T>
inline constexpr
T operator*(const T& lhs,
            const memvar::memvar<T>& rhs) noexcept
{
  return lhs * rhs();
}

// operator/
template <typename T>
inline constexpr
T operator/(const memvar::memvar<T>& lhs,
            const memvar::memvar<T>& rhs) noexcept
{
  return lhs() / rhs();
}
template <typename T>
inline constexpr
T operator/(const memvar::memvar<T>& lhs,
            const T& rhs) noexcept
{
  return lhs() / rhs;
}
template <typename T>
inline constexpr
T operator/(const T& lhs,
            const memvar::memvar<T>& rhs) noexcept
{
  return lhs / rhs();
}

template <typename T>
struct std::is_integral<memvar::memvar<T>>
{
  static inline const bool value = true;
};
////////////////////////////////////////////////////////////////////////////////
namespace memvar
{
  // memvar specialization for time tagged memvars
template <typename T,
          typename Time = std::chrono::nanoseconds,
          typename Clock = std::chrono::high_resolution_clock>
class memvarTimed final : public memvar<T>
{
 public:
  using historyTimedValue = std::tuple<T, Time, bool>;
   
  explicit memvarTimed() noexcept
  :
  memvar<T>()
  {
    // store the time point for the first value
    timeMemo_.push_front(Clock::now()),
    // store the time point epoch for the memvar
    memvarEpoch_ = timeMemo_.at(0);
  }

  explicit memvarTimed(const T& value,
                       const memvarBase::capacityType historyCapacity = memvarBase::historyCapacityDefault_) noexcept(false)
  :
  memvar<T>(value, historyCapacity)
  {
    // store the time point for the first value
    timeMemo_.push_front(Clock::now()),
    // store the time point epoch for the memvar
    memvarEpoch_ = timeMemo_.at(0);
  }

  memvarTimed(const memvarTimed& rhs) = delete;
  memvarTimed(memvarTimed&& rhs) = delete;
  memvarTimed& operator=(memvarTimed&& rhs) = delete;

  // conversion operator from memvar::memvarTimed<T> to T
  operator T() const noexcept
  {
    return memvar<T>::getValue();
  }

  T operator()() const noexcept
  {
    return memvar<T>::getValue();
  }
  T operator()(const memvarBase::capacityType index) noexcept
  {
    return std::get<T>(getHistoryValue(index));
  }

  memvarTimed& operator=(const T& rhs) noexcept
  {
    setValue(rhs);
    return *this;
  }
  memvarTimed& operator=(const memvarTimed& rhs) noexcept
  {
    setValue(rhs.getValue());
    return *this;
  }

  memvarTimed& operator+=(const T& rhs) noexcept
  {
    setValue(memvar<T>::getValue() + rhs);
    return *this;
  }
  memvarTimed& operator+=(const memvarTimed& rhs) noexcept
  {
    setValue(memvar<T>::getValue() + rhs.getValue());
    return *this;
  }

  memvarTimed& operator-=(const T& rhs) noexcept
  {
    memvar<T>::checkStringNotAllowed();
    setValue(memvar<T>::getValue() - rhs);
    return *this;
  }
  memvarTimed& operator-=(const memvarTimed& rhs) noexcept
  {
    memvar<T>::checkStringNotAllowed();
    setValue(memvar<T>::getValue() - rhs.getValue());
    return *this;
  }

  memvarTimed& operator*=(const T& rhs) noexcept
  {
    memvar<T>::checkStringNotAllowed();
    setValue(memvar<T>::getValue() * rhs);
    return *this;
  }
  memvarTimed& operator*=(const memvarTimed& rhs) noexcept
  {
    memvar<T>::checkStringNotAllowed();
    setValue(memvar<T>::getValue() * rhs.getValue());
    return *this;
  }

  memvarTimed& operator/=(const T& rhs) noexcept
  {
    memvar<T>::checkStringNotAllowed();
    setValue(memvar<T>::getValue() / rhs);
    return *this;
  }
  memvarTimed& operator/=(const memvarTimed& rhs) noexcept
  {
    memvar<T>::checkStringNotAllowed();
    setValue(memvar<T>::getValue() / rhs.getValue());
    return *this;
  }
    
  // ++mvt
  T operator++() const noexcept
  {
    T result = memvar<T>::operator++();
    setTimeTag();
    return result;
  }
  // mvt++
  T operator++([[maybe_unused]] int dummy) const noexcept
  {
    memvar<T>::operator++(0),
    setTimeTag();
    return std::get<T>(memvar<T>::getHistoryValue(1));
  }

  // --mvt
  T operator--() const noexcept
  {
    T result = memvar<T>::operator--();
    setTimeTag();
    return result;
  }
  // mvt--
  T operator--([[maybe_unused]] int dummy) const noexcept
  {
    memvar<T>::operator--(0),
    setTimeTag();
    return std::get<T>(memvar<T>::getHistoryValue(1));
  }

  // the time tag for the i-th value in the history is the time duration measured 
  // in Time units from the memvar time point epoch
  inline constexpr
  Time getTimeTag(const size_t& index = 0) const noexcept
  {
    return std::chrono::duration_cast<Time>(getMemVarTimeHistory_ref().at(index) - memvarEpoch_);
  }

  void printHistoryTimedData(const char separatorChar = '\n') noexcept
  {
    if ( 0 == memvar<T>::getHistorySize() )
    {
      return;
    }
    std::cout << "{ --- begin ---\n";
    for(size_t i = 0; i < memvar<T>::getHistorySize(); ++i)
    {
      std::cout << "["
                << getTimeTag(i).count()
                << ":"
                << memvar<T>::getMemVarHistory_ref().at(i) << "]"
                << separatorChar;
    }
    std::cout << "  --- end --- }\n\n";
  }
  void printReverseHistoryTimedData(const char separatorChar = '\n') noexcept
  {
    if ( 0 == memvar<T>::getHistorySize() )
    {
      return;
    }
    std::cout << "{ --- begin ---\n";
    for(auto i = static_cast<signed long>(memvar<T>::getHistorySize() - 1); i >= 0; --i)
    {
      std::cout << "["
                << getTimeTag(static_cast<size_t>(i)).count()
                << ":"
                << memvar<T>::getMemVarHistory_ref().at(static_cast<size_t>(i)) << "]"
                << separatorChar;
    }
    std::cout << "  --- end --- }\n\n";
  }

  inline
  void clearHistory() const noexcept
  {
    memvar<T>::clearHistory();
    getMemVarTimeHistory_ref().erase(std::cbegin(getMemVarTimeHistory_ref()) + 1, std::cend(getMemVarTimeHistory_ref()));
    getMemVarTimeHistory_ref().shrink_to_fit();
  }

  inline constexpr
  auto getHistoryValue(const memvarBase::capacityType index) const noexcept -> historyTimedValue
  {
    if ( (index < static_cast<memvarBase::capacityType>(memvar<T>::getHistorySize())) && (index >= 0) )
    {
      return std::make_tuple(memvar<T>::getMemVarHistory_ref().at(static_cast<size_t>(index)),
                             getTimeTag(static_cast<size_t>(index)),
                             false);
    }
    return std::make_tuple(T{}, Time{0}, true);
  }

 private:
  using memvarTimeHistory = std::deque<std::chrono::time_point<Clock, Time>>;

  mutable memvarTimeHistory timeMemo_ {};
  mutable std::chrono::time_point<Clock, Time> memvarEpoch_ {};

  inline constexpr
  auto& getMemVarTimeHistory_ref () const noexcept
  {
    return timeMemo_;
  }

  inline
  void setTimeTag() const noexcept
  {
    getMemVarTimeHistory_ref().push_front(Clock::now());
  }

  inline
  void setValue(const T& value) const noexcept
  {
    setTimeTag(),
    memvar<T>::setValue(value);
  }
};  // class memvarTimed

template <typename T>
inline constexpr
T getHistoryValue(const memvarTimed<T>& mvt, const memvarBase::capacityType index) noexcept
{
  return std::get<T>(mvt.getHistoryValue(index));
}
}  // namespace memvar

template <typename T>
inline
std::ostream& operator<<(std::ostream& os, const memvar::memvarTimed<T>& mvt)
{
  return os << "[" << mvt.getTimeTag().count() << ":" << mvt() << "]";
}

template <typename T>
struct std::is_integral<memvar::memvarTimed<T>>
{
  static inline const bool value = true;
};
