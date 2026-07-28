//
// memvar.h
//
#pragma once

#include <concepts>
#include <type_traits>
#include <cstdint>
#include <iostream>
#include <string>
#include <codecvt>
#include <locale>
#include <tuple>
#include <deque>
#include <chrono>
#include <algorithm>
#include <iterator>
#include <stdexcept>
////////////////////////////////////////////////////////////////////////////////
// Forward declaration for bigint.h, used in unit tests
namespace bip { class bigint; }

// Overload for std::wstring
inline std::ostream& operator<<(std::ostream& os, const std::wstring& s) {
  // std::wstring_convert: C++17 deprecated, but no replacement defined yet
  std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
  return os << conv.to_bytes(s);
}

// Overload for std::u8string (C++20)
// Memory layout is the same as char, so a simple reinterpret_cast works perfectly.
inline std::ostream& operator<<(std::ostream& os, const std::u8string& s) {
  return os << reinterpret_cast<const char*>(s.c_str());
}

// Overload for std::u16string
inline std::ostream& operator<<(std::ostream& os, const std::u16string& s) {
  // std::wstring_convert: C++17 deprecated, but no replacement defined yet
  std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> conv;
  return os << conv.to_bytes(s);
}

// Overload for std::u32string
inline std::ostream& operator<<(std::ostream& os, const std::u32string& s) {
  // std::wstring_convert: C++17 deprecated, but no replacement defined yet
  std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> conv;
  return os << conv.to_bytes(s);
}

namespace mv
{
// Trait to recognize custom "big integer" types that should be allowed
// in memvar<T>, in addition to integral/floating point/string types.
template <typename T>
struct is_bigint : std::false_type {};

template <>
struct is_bigint<bip::bigint> : std::true_type {};

template <typename T>
inline constexpr bool is_bigint_v = is_bigint<T>::value;

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

// Concept describing the set of types allowed as memvar<T>'s T:
// integral (not bool), floating point, standard string, or bigint types.
template <typename T>
concept MemVarType = (std::is_integral_v<std::remove_cvref_t<T>> &&
                      !std::same_as<std::remove_cv_t<T>, bool>) ||
                      std::is_floating_point_v<std::remove_cvref_t<T>> ||
                      AnyStandardString<T> ||
                      is_bigint_v<std::remove_cvref_t<T>>;

// memvar
// a variable with memory of old values
// only strings, integral or floating point types allowed
template <MemVarType T>
class memvar {
public:
  // capacityType: this type must be signed
  using capacityType = int64_t;

  capacityType getHistoryCapacity() const noexcept {
    return historyCapacity_;
  }

protected:
  using memvarHistory = std::deque<T>;

  static constexpr capacityType minimumHistoryCapacity_ {2};
  static constexpr capacityType historyCapacityDefault_ {10};
  const capacityType historyCapacity_ {historyCapacityDefault_};

  memvarHistory memo_ {};

  const T& getValue() const {
    return memo_.at(0);
  }

  virtual void setValue(const T& value) {
    memo_.emplace_front(value);
    if ( static_cast<capacityType>(memo_.size()) > historyCapacity_ ) {
      memo_.pop_back();
    }
  }

  T incr1() requires (!AnyStandardString<T>) {
    const T newValue {getValue() + static_cast<T>(1)};

    setValue(newValue);
    return newValue;
  }

  void voidIncr1() requires (!AnyStandardString<T>) {
    setValue(getValue() + static_cast<T>(1));
  }

  T decr1() requires (!AnyStandardString<T>) {
    const T newValue {getValue() - static_cast<T>(1)};

    setValue(newValue);
    return newValue;
  }

  void voidDecr1() requires (!AnyStandardString<T>) {
    setValue(getValue() - static_cast<T>(1));
  }

  void memvarPrinter (const memvarHistory& history,
                      std::ostream& os = std::cout,
                      const bool printReverse = false,
                      const std::string& separator = std::string(" ")) const {
    if ( history.empty() ) {
      os << "[]\n";
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
  // historyValue defines the data stored in the history, and it is a 2-tuple:
  // a T value, and an error flag: false, no error; true, error
  using historyValue = std::tuple<T, bool>;

  memvar() {
    memo_.emplace_front(T{});
  }

  explicit memvar(const T& value,
                  const capacityType historyCapacity = historyCapacityDefault_) :
  historyCapacity_ (historyCapacity) {
    if ( historyCapacity_ < minimumHistoryCapacity_ ) {
      throw std::invalid_argument("ERROR: The history capacity must be " + std::to_string(minimumHistoryCapacity_) + " at least, or more");
    }
    memo_.emplace_front(value);
  }

  virtual ~memvar() = default;

  memvar(const memvar& rhs) = delete;
  memvar(memvar&& rhs) = delete;
  memvar& operator=(memvar&& rhs) = delete;

  // conversion operator from mv::memvar<T> to T
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

  historyValue getHistoryValue(const capacityType index) const {
    if ( (index < static_cast<capacityType>(memo_.size())) && (index >= 0) ) {
      return std::make_tuple(memo_.at(static_cast<size_t>(index)), false);
    }

    throw std::out_of_range("Index " +
                            std::to_string(index) +
                            " is out of bounds (size is "
                            + std::to_string(static_cast<capacityType>(memo_.size())) +
                            ").");
  }

  auto getHistoryMinMax() const {
    const auto [min, max] = std::ranges::minmax_element(memo_);
    return std::make_tuple(*min, *max);
  }
};  // class memvar

// memvar specialization for time tagged memvars
template <MemVarType T,
          typename Time = std::chrono::nanoseconds,
          typename Clock = std::chrono::steady_clock>
class memvarTimed final : public memvar<T> {
public:
  // historyTimedValue defines the data stored in the time-tagged history, and
  // it is a 3-tuple:
  // a T value, the time tag, and an error flag: false, no error; true, error
  using historyTimedValue = std::tuple<T, Time, bool>;

  memvarTimed() :
  memvar<T>() {
    // store the time point epoch for the memvar
    memvarEpoch_ = Clock::now();
    // store the time point for the first value
    timeMemo_.emplace_front(memvarEpoch_);
  }

  explicit memvarTimed(const T& value,
                       const memvar<T>::capacityType historyCapacity = memvar<T>::historyCapacityDefault_) :
  memvar<T>(value, historyCapacity) {
    // store the time point epoch for the memvar
    memvarEpoch_ = Clock::now();
    // store the time point for the first value
    timeMemo_.emplace_front(memvarEpoch_);
  }

  memvarTimed(const memvarTimed& rhs) = delete;
  memvarTimed(memvarTimed&& rhs) = delete;
  memvarTimed& operator=(memvarTimed&& rhs) = delete;

  // conversion operator from mv::memvarTimed<T> to T
  operator T() const {
    return memvar<T>::getValue();
  }

  T operator()() const {
    return memvar<T>::getValue();
  }
  T operator()(const memvar<T>::capacityType index) const {
    return std::get<T>(memvar<T>::getHistoryValue(index));
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
    for (auto i {static_cast<memvar<T>::capacityType>(memvar<T>::memo_.size() - 1)}; i >= 0; --i) {
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
    // clearing the timed memvar means also to reset the time point epoch
    // associated to the first 'zero' value
    // store the time point for the first value
    timeMemo_.clear();
    // store the time point epoch for the memvar
    memvarEpoch_ = Clock::now();
    timeMemo_.emplace_front(memvarEpoch_);
  }

  // separate, non-virtual, memvarTimed-only accessor for the richer tuple with Time
  historyTimedValue getHistoryTimedValue(const memvar<T>::capacityType index) const {
    return std::make_tuple(std::get<0>(memvar<T>::getHistoryValue(index)), getTimeTag(static_cast<size_t>(index)), false);
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
    if ( static_cast<memvar<T>::capacityType>(timeMemo_.size()) > memvar<T>::historyCapacity_ ) {
      timeMemo_.pop_back();
    }
    memvar<T>::setValue(value);
  }
};  // class memvarTimed

template <typename T>
T getHistoryValue(const memvar<T>& mv, const typename memvar<T>::capacityType index) {
  return std::get<T>(mv.getHistoryValue(index));
}

template <typename T>
T getHistoryTimedValue(const memvarTimed<T>& mvt, const typename memvar<T>::capacityType index) {
  return std::get<T>(mvt.getHistoryTimedValue(index));
}

}  // namespace mv

// operator==
template <typename T>
bool operator==(const mv::memvar<T>& mv1, const mv::memvar<T>& mv2) {
  return mv1() == mv2();
}
template <typename T>
bool operator==(const mv::memvar<T>& mv, const T& v) {
  return mv() == v;
}
template <typename T>
bool operator==(const T& v, const mv::memvar<T>& mv) {
  return v == mv();
}

// In C++20 operator!= is automatically synthesized from operator==

// operator>
template <typename T>
bool operator>(const mv::memvar<T>& mv1, const mv::memvar<T>& mv2) {
  return mv1() > mv2();
}
template <typename T>
bool operator>(const mv::memvar<T>& mv, const T& v) {
  return mv() > v;
}
template <typename T>
bool operator>(const T& v, const mv::memvar<T>& mv) {
  return v > mv();
}

// operator<
template <typename T>
bool operator<(const mv::memvar<T>& mv1, const mv::memvar<T>& mv2) {
  return mv1() < mv2();
}
template <typename T>
bool operator<(const mv::memvar<T>& mv, const T& v) {
  return mv() < v;
}
template <typename T>
bool operator<(const T& v, const mv::memvar<T>& mv) {
  return v < mv();
}

// operator>=
template <typename T>
bool operator>=(const mv::memvar<T>& mv1, const mv::memvar<T>& mv2) {
  return mv1() >= mv2();
}
template <typename T>
bool operator>=(const T& v, const mv::memvar<T>& mv) {
  return v >= mv();
}
template <typename T>
bool operator>=(const mv::memvar<T>& mv, const T& v) {
  return mv() >= v;
}

// operator<=
template <typename T>
bool operator<=(const mv::memvar<T>& mv1, const mv::memvar<T>& mv2) {
  return mv1() <= mv2();
}
template <typename T>
bool operator<=(const mv::memvar<T>& mv, const T& v) {
  return mv() <= v;
}
template <typename T>
bool operator<=(const T& v, const mv::memvar<T>& mv) {
  return v <= mv();
}

// operator+
template <typename T>
T operator+(const mv::memvar<T>& lhs,
            const mv::memvar<T>& rhs) {
  return lhs() + rhs();
}
template <typename T>
T operator+(const mv::memvar<T>& lhs,
            const T& rhs) {
  return lhs() + rhs;
}
template <typename T>
T operator+(const T& lhs,
            const mv::memvar<T>& rhs) {
  return lhs + rhs();
}

// operator-
template <typename T>
T operator-(const mv::memvar<T>& lhs,
            const mv::memvar<T>& rhs) requires (!mv::AnyStandardString<T>) {
  return lhs() - rhs();
}
template <typename T>
T operator-(const mv::memvar<T>& lhs,
            const T& rhs) requires (!mv::AnyStandardString<T>) {
  return lhs() - rhs;
}
template <typename T>
T operator-(const T& lhs,
            const mv::memvar<T>& rhs) requires (!mv::AnyStandardString<T>) {
  return lhs - rhs();
}

// operator*
template <typename T>
T operator*(const mv::memvar<T>& lhs,
            const mv::memvar<T>& rhs) requires (!mv::AnyStandardString<T>) {
  return lhs() * rhs();
}
template <typename T>
T operator*(const mv::memvar<T>& lhs,
            const T& rhs) requires (!mv::AnyStandardString<T>) {
  return lhs() * rhs;
}
template <typename T>
T operator*(const T& lhs,
            const mv::memvar<T>& rhs) requires (!mv::AnyStandardString<T>) {
  return lhs * rhs();
}

// operator/
template <typename T>
T operator/(const mv::memvar<T>& lhs,
            const mv::memvar<T>& rhs) requires (!mv::AnyStandardString<T>) {
  return lhs() / rhs();
}
template <typename T>
T operator/(const mv::memvar<T>& lhs,
            const T& rhs) requires (!mv::AnyStandardString<T>) {
  return lhs() / rhs;
}
template <typename T>
T operator/(const T& lhs,
            const mv::memvar<T>& rhs) requires (!mv::AnyStandardString<T>) {
  return lhs / rhs();
}

template <typename T>
std::ostream& operator<<(std::ostream& os, const mv::memvar<T>& mv) {
  return os << mv();
}

template <typename T>
std::ostream& operator<<(std::ostream& os, const mv::memvarTimed<T>& mvt) {
  return os << "[" << mvt.getTimeTag().count() << ":" << mvt() << "]";
}

