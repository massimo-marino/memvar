//
// memvar.h
//
#pragma once

#include "is_string.h"
#include <concepts>
#include <type_traits>
#include <cstdint>
#include <iostream>
#include <string>
#include <tuple>
#include <deque>
#include <chrono>
#include <algorithm>
#include <stdexcept>
////////////////////////////////////////////////////////////////////////////////
namespace memvar
{
// Define a concept for readability to tell the compiler to ignore a template
// function for some specific types: std::string, etc.
template <typename T, typename... Types>
concept IsAnyOf = (std::same_as<T, Types> || ...);
template <typename T>
concept AnyStandardString = IsAnyOf<std::remove_cvref_t<T>,
                                    std::string,
                                    std::wstring,
                                    std::u8string,
                                    std::u16string,
                                    std::u32string>;

class memvarBase {
 public:
  // capacityType: this type must be signed
  using capacityType = int64_t;

  memvarBase(const memvarBase& rhs) = delete;
  memvarBase& operator=(const memvarBase& rhs) = delete;
  memvarBase(memvarBase&& rhs) = delete;
  memvarBase& operator=(memvarBase&& rhs) = delete;

  capacityType getHistoryCapacity() const noexcept {
    return historyCapacity_;
  }

 protected:
  static constexpr capacityType minimumHistoryCapacity_ {2};
  static constexpr capacityType historyCapacityDefault_ {10};
  const capacityType historyCapacity_ {historyCapacityDefault_};

  explicit memvarBase(capacityType historyCapacity) noexcept :
  historyCapacity_ (historyCapacity)
  {}

  memvarBase() = default;
  virtual ~memvarBase() = default;
};  // memvarBase

// memvar
// a variable with memory of old values
// only strings, integral or floating point types allowed
template <typename T>
class memvar : public memvarBase {
 protected:
  using memvarHistory = std::deque<T>;

  memvarHistory memo_ {};

  static void checkType() {
    static_assert((std::is_integral_v<T> != false ||
                   std::is_floating_point_v<T> != false ||
                   is_string_v<T> != false),
                  "String, integral or floating point types required.");
  }

  T getValue() const {
    return memo_.at(0);
  }

  virtual void setValue(const T& value) {
    memo_.emplace_front(value);
    if ( static_cast<capacityType>(memo_.size()) > historyCapacity_ ) {
      memo_.pop_back();
    }
  }

  T incr1() requires (!AnyStandardString<T>) {
    const T newValue {static_cast<T>(getValue() + 1)};

    setValue(newValue);
    return newValue;
  }

  void voidIncr1() requires (!AnyStandardString<T>) {
    setValue(getValue() + 1);
  }

  T decr1() requires (!AnyStandardString<T>) {
    const T newValue {static_cast<T>(getValue() - 1)};

    setValue(newValue);
    return newValue;
  }

  void voidDecr1() requires (!AnyStandardString<T>) {
    setValue(getValue() - 1);
  }

  void memvarPrinter (const memvarHistory& history,
                      std::ostream& os = std::cout,
                      const bool printReverse = false,
                      const std::string& separator = std::string(" ")) const {
    if ( history.empty() ) {
      return;
    }

    auto printItem = [&separator, &os] (const T& item) -> void {
      os << item << separator;
    };

    os << "[ ";
    if ( printReverse ) {
      std::for_each(history.crbegin(), history.crend(), printItem);
    }
    else {
      std::for_each(history.cbegin(), history.cend(), printItem);
    }
    os << "\b \b ]\n";
  }

 public:
  using historyValue = std::tuple<T, bool>;

  memvar() :
  memvarBase() {
    checkType();
    memo_.emplace_front(T{});
  }

  explicit memvar(const T& value,
                  const capacityType historyCapacity = historyCapacityDefault_) :
  memvarBase(historyCapacity) {
    checkType();
    if ( historyCapacity_ < minimumHistoryCapacity_ ) {
      throw std::invalid_argument("ERROR: The history capacity must be " + std::to_string(minimumHistoryCapacity_) + " at least, or more");
    }
    memo_.emplace_front(value);
  }

  virtual ~memvar() = default;

  memvar(const memvar& rhs) = delete;
  memvar(memvar&& rhs) = delete;
  memvar& operator=(memvar&& rhs) = delete;

  // conversion operator from memvar::memvar<T> to T
  operator T() const {
    return getValue();
  }

  T operator()() const {
    return getValue();
  }

  T operator()(const capacityType index) const {
    return std::get<T>(getHistoryValue(index));
  }

  T operator[](const capacityType index) const {
    return std::get<T>(getHistoryValue(index));
  }

  memvar& operator=(const T& rhs) {
    setValue(rhs);
    return *this;
  }
  memvar& operator=(const memvar& rhs) {
    setValue(rhs.getValue());
    return *this;
  }

  memvar& operator+=(const T& rhs) {
    setValue(getValue() + rhs);
    return *this;
  }
  memvar& operator+=(const memvar& rhs) {
    setValue(getValue() + rhs.getValue());
    return *this;
  }

  memvar& operator-=(const T& rhs) requires (!AnyStandardString<T>) {
    setValue(getValue() - rhs);
    return *this;
  }
  memvar& operator-=(const memvar& rhs) requires (!AnyStandardString<T>) {
    setValue(getValue() - rhs.getValue());
    return *this;
  }

  memvar& operator*=(const T& rhs) requires (!AnyStandardString<T>) {
    setValue(getValue() * rhs);
    return *this;
  }
  memvar& operator*=(const memvar& rhs) requires (!AnyStandardString<T>) {
    setValue(getValue() * rhs.getValue());
    return *this;
  }

  memvar& operator/=(const T& rhs) requires (!AnyStandardString<T>) {
    setValue(getValue() / rhs);
    return *this;
  }
  memvar& operator/=(const memvar& rhs) requires (!AnyStandardString<T>) {
    setValue(getValue() / rhs.getValue());
    return *this;
  }

  // ++mv
  T operator++() requires (!AnyStandardString<T>) {
    return incr1();
  }
  // mv++
  T operator++([[maybe_unused]] int dummy) requires (!AnyStandardString<T>) {
    voidIncr1();
    return std::get<T>(getHistoryValue(1));
  }

  // --mv
  T operator--() requires (!AnyStandardString<T>) {
    return decr1();
  }
  // mv--
  T operator--([[maybe_unused]] int dummy) requires (!AnyStandardString<T>) {
    voidDecr1();
    return std::get<T>(getHistoryValue(1));
  }

  void printHistoryData(std::ostream& os = std::cout, const std::string& separator = std::string(" ")) const {
    // print history in order (from newest/last value to oldest/first value)
    memvarPrinter(memo_, os, false, separator);
  }

  void printReverseHistoryData(std::ostream& os = std::cout, const std::string& separator = std::string(" ")) const {
    // print history in reverse order (from oldest/first value to newest/last value)
    memvarPrinter(memo_, os, true, separator);
  }

  capacityType getHistorySize() const noexcept {
    return static_cast<capacityType>(memo_.size());
  }

  virtual void clearHistory() {
    memo_.clear();
    setValue(T{});
  }

  auto isHistoryFull() const noexcept {
    return static_cast<capacityType>(memo_.size()) >= historyCapacity_;
  }

  const auto& getMemVarHistory () const noexcept {
    return memo_;
  }

  auto getHistoryValue(const capacityType index) const noexcept -> historyValue {
    if ( (index < static_cast<capacityType>(memo_.size())) && (index >= 0) ) {
      return std::make_tuple(memo_.at(static_cast<size_t>(index)), false);
    }
    return std::make_tuple(T{}, true);
  }

  auto getHistoryMinMax() const {
    auto result = std::minmax_element(memo_.cbegin(), memo_.cend());
    return std::make_tuple(std::get<T>(getHistoryValue(result.first - memo_.cbegin())),
                           std::get<T>(getHistoryValue(result.second - memo_.cbegin())));
  }
};  // class memvar

template <typename T>
T getHistoryValue(const memvar<T>& mv, const memvarBase::capacityType index) {
  return std::get<T>(mv.getHistoryValue(index));
}
}  // namespace memvar

template <typename T>
std::ostream& operator<<(std::ostream& os, const memvar::memvar<T>& mv) {
   return os << mv();
}

// operator==
template <typename T>
bool operator==(const memvar::memvar<T>& mv1, const memvar::memvar<T>& mv2) {
  return mv1() == mv2();
}
template <typename T>
bool operator==(const memvar::memvar<T>& mv, const T& v) {
  return mv() == v;
}
template <typename T>
bool operator==(const T& v, const memvar::memvar<T>& mv) {
  return v == mv();
}

// In C++20 operator!= is automatically synthesized from operator==

// operator>
template <typename T>
bool operator>(const memvar::memvar<T>& mv1, const memvar::memvar<T>& mv2) {
  return mv1() > mv2();
}
template <typename T>
bool operator>(const memvar::memvar<T>& mv, const T& v) {
  return mv() > v;
}
template <typename T>
bool operator>(const T& v, const memvar::memvar<T>& mv) {
  return v > mv();
}

// operator<
template <typename T>
bool operator<(const memvar::memvar<T>& mv1, const memvar::memvar<T>& mv2) {
  return mv1() < mv2();
}
template <typename T>
bool operator<(const memvar::memvar<T>& mv, const T& v) {
  return mv() < v;
}
template <typename T>
bool operator<(const T& v, const memvar::memvar<T>& mv) {
  return v < mv();
}

// operator>=
template <typename T>
bool operator>=(const memvar::memvar<T>& mv1, const memvar::memvar<T>& mv2) {
  return mv1() >= mv2();
}
template <typename T>
bool operator>=(const T& v, const memvar::memvar<T>& mv) {
  return v >= mv();
}
template <typename T>
bool operator>=(const memvar::memvar<T>& mv, const T& v) {
  return mv() >= v;
}

// operator<=
template <typename T>
bool operator<=(const memvar::memvar<T>& mv1, const memvar::memvar<T>& mv2) {
  return mv1() <= mv2();
}
template <typename T>
bool operator<=(const memvar::memvar<T>& mv, const T& v) {
  return mv() <= v;
}
template <typename T>
bool operator<=(const T& v, const memvar::memvar<T>& mv) {
  return v <= mv();
}

// operator+
template <typename T>
T operator+(const memvar::memvar<T>& lhs,
            const memvar::memvar<T>& rhs) {
  return lhs() + rhs();
}
template <typename T>
T operator+(const memvar::memvar<T>& lhs,
            const T& rhs) {
  return lhs() + rhs;
}
template <typename T>
T operator+(const T& lhs,
            const memvar::memvar<T>& rhs) {
  return lhs + rhs();
}

// operator-
template <typename T>
T operator-(const memvar::memvar<T>& lhs,
            const memvar::memvar<T>& rhs) {
  return lhs() - rhs();
}
template <typename T>
T operator-(const memvar::memvar<T>& lhs,
            const T& rhs) {
  return lhs() - rhs;
}
template <typename T>
T operator-(const T& lhs,
            const memvar::memvar<T>& rhs) {
  return lhs - rhs();
}

// operator*
template <typename T>
T operator*(const memvar::memvar<T>& lhs,
            const memvar::memvar<T>& rhs) {
  return lhs() * rhs();
}
template <typename T>
T operator*(const memvar::memvar<T>& lhs,
            const T& rhs) {
  return lhs() * rhs;
}
template <typename T>
T operator*(const T& lhs,
            const memvar::memvar<T>& rhs) {
  return lhs * rhs();
}

// operator/
template <typename T>
T operator/(const memvar::memvar<T>& lhs,
            const memvar::memvar<T>& rhs) {
  return lhs() / rhs();
}
template <typename T>
T operator/(const memvar::memvar<T>& lhs,
            const T& rhs) {
  return lhs() / rhs;
}
template <typename T>
T operator/(const T& lhs,
            const memvar::memvar<T>& rhs) {
  return lhs / rhs();
}

////////////////////////////////////////////////////////////////////////////////
namespace memvar
{
// memvar specialization for time tagged memvars
template <typename T,
          typename Time = std::chrono::nanoseconds,
          typename Clock = std::chrono::high_resolution_clock>
class memvarTimed final : public memvar<T> {
 public:
  using historyTimedValue = std::tuple<T, Time, bool>;

  memvarTimed() :
  memvar<T>() {
    // store the time point epoch for the memvar
    memvarEpoch_ = Clock::now();
    // store the time point for the first value
    timeMemo_.emplace_front(memvarEpoch_);
  }

  explicit memvarTimed(const T& value,
                       const memvarBase::capacityType historyCapacity = memvarBase::historyCapacityDefault_) :
  memvar<T>(value, historyCapacity) {
    // store the time point epoch for the memvar
    memvarEpoch_ = Clock::now();
    // store the time point for the first value
    timeMemo_.emplace_front(memvarEpoch_);
  }

  memvarTimed(const memvarTimed& rhs) = delete;
  memvarTimed(memvarTimed&& rhs) = delete;
  memvarTimed& operator=(memvarTimed&& rhs) = delete;

  // conversion operator from memvar::memvarTimed<T> to T
  operator T() const {
    return memvar<T>::getValue();
  }

  T operator()() const {
    return memvar<T>::getValue();
  }
  T operator()(const memvarBase::capacityType index) const {
    return std::get<T>(getHistoryValue(index));
  }

  memvarTimed& operator=(const T& rhs) {
    setValue(rhs);
    return *this;
  }
  memvarTimed& operator=(const memvarTimed& rhs) {
    setValue(rhs.getValue());
    return *this;
  }

  memvarTimed& operator+=(const T& rhs) {
    setValue(memvar<T>::getValue() + rhs);
    return *this;
  }
  memvarTimed& operator+=(const memvarTimed& rhs) {
    setValue(memvar<T>::getValue() + rhs.getValue());
    return *this;
  }

  memvarTimed& operator-=(const T& rhs) requires (!AnyStandardString<T>) {
    setValue(memvar<T>::getValue() - rhs);
    return *this;
  }
  memvarTimed& operator-=(const memvarTimed& rhs) requires (!AnyStandardString<T>) {
    setValue(memvar<T>::getValue() - rhs.getValue());
    return *this;
  }

  memvarTimed& operator*=(const T& rhs) requires (!AnyStandardString<T>) {
    setValue(memvar<T>::getValue() * rhs);
    return *this;
  }
  memvarTimed& operator*=(const memvarTimed& rhs) requires (!AnyStandardString<T>) {
    setValue(memvar<T>::getValue() * rhs.getValue());
    return *this;
  }

  memvarTimed& operator/=(const T& rhs) requires (!AnyStandardString<T>) {
    setValue(memvar<T>::getValue() / rhs);
    return *this;
  }
  memvarTimed& operator/=(const memvarTimed& rhs) requires (!AnyStandardString<T>) {
    setValue(memvar<T>::getValue() / rhs.getValue());
    return *this;
  }

  // the time tag for the i-th value in the history is the time duration measured
  // in Time units from the memvar time point epoch
  Time getTimeTag(const size_t index = 0) const {
    return std::chrono::duration_cast<Time>(timeMemo_.at(index) - memvarEpoch_);
  }

  void printHistoryTimedData(std::ostream& os = std::cout, const std::string& separator = std::string("\n")) const {
    if ( 0 == memvar<T>::memo_.size() ) {
      return;
    }
    os << "{ --- begin ---\n[TimeTag:Value]\n";
    for (size_t i {0}; i < memvar<T>::memo_.size(); ++i) {
      os << "["
         << getTimeTag(i).count()
         << ":"
         << memvar<T>::memo_.at(i) << "]"
         << separator;
    }
    os << "  --- end --- }\n\n";
  }
  void printReverseHistoryTimedData(std::ostream& os = std::cout, const std::string& separator = std::string("\n")) const {
    if ( 0 == memvar<T>::memo_.size() ) {
      return;
    }
    os << "{ --- begin ---\n[TimeTag:Value]\n";
    for (auto i {static_cast<memvarBase::capacityType>(memvar<T>::memo_.size() - 1)}; i >= 0; --i) {
      os << "["
         << getTimeTag(static_cast<size_t>(i)).count()
         << ":"
         << memvar<T>::memo_.at(static_cast<size_t>(i)) << "]"
         << separator;
    }
    os << "  --- end --- }\n\n";
  }

  void clearHistory() override {
    // clear the memvar's history
    memvar<T>::clearHistory();
    // store the time point epoch for the memvar
    memvarEpoch_ = Clock::now();
    // clearing the timed memvar means also to reset the time point epoch
    // associated to the first 'zero' value
    // store the time point for the first value
    timeMemo_.clear();
    timeMemo_.emplace_front(memvarEpoch_);
  }

  auto getHistoryValue(const memvarBase::capacityType index) const noexcept -> historyTimedValue const {
    if ( (index < static_cast<memvarBase::capacityType>(memvar<T>::memo_.size())) && (index >= 0) ) {
      return std::make_tuple(memvar<T>::memo_.at(static_cast<size_t>(index)),
                             getTimeTag(static_cast<size_t>(index)),
                             false);
    }
    return std::make_tuple(T{}, Time{0}, true);
  }

 private:
  using memvarTimeHistory = std::deque<std::chrono::time_point<Clock, Time>>;

  memvarTimeHistory timeMemo_ {};
  std::chrono::time_point<Clock, Time> memvarEpoch_ {};

  void setTimeTag() {
    timeMemo_.emplace_front(Clock::now());
  }

  void setValue(const T& value) override {
    setTimeTag();
    memvar<T>::setValue(value);
  }
};  // class memvarTimed

template <typename T>
T getHistoryValue(const memvarTimed<T>& mvt, const memvarBase::capacityType index) {
  return std::get<T>(mvt.getHistoryValue(index));
}
}  // namespace memvar

template <typename T>
std::ostream& operator<<(std::ostream& os, const memvar::memvarTimed<T>& mvt) {
  return os << "[" << mvt.getTimeTag().count() << ":" << mvt() << "]";
}
