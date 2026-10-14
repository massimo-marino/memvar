//
// memvar.h
//
#pragma once

#include <algorithm>
#include <chrono>
#include <concepts>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
////////////////////////////////////////////////////////////////////////////////
// MEMVAR_SAFEREF_CHECK selects how memvar<T>::cref() protects against a handle
// being used after its history slot has been evicted:
//
//   MEMVAR_SAFEREF_ABORT    (0): cref() returns a checked SafeRef; an expired
//                                handle reports to stderr and calls abort().
//                                The check path is noexcept, so callers need
//                                no unwind bookkeeping (that is what keeps it
//                                as cheap as a plain const T&).
//   MEMVAR_SAFEREF_THROWS   (1): same handle, but an expired handle throws
//                                mv::expired_history_reference.
//   MEMVAR_SAFEREF_NO_CHECK (2, default): no handle, no counter, no check.
//                                SafeRef is preprocessed out and cref()
//                                returns a plain const T&.
//
// Define it (to the macro or its 0/1/2 value) before including this header, or
// with -DMEMVAR_SAFEREF_CHECK=... .
#define MEMVAR_SAFEREF_ABORT    0
#define MEMVAR_SAFEREF_THROWS   1
#define MEMVAR_SAFEREF_NO_CHECK 2

#ifndef MEMVAR_SAFEREF_CHECK
  #define MEMVAR_SAFEREF_CHECK MEMVAR_SAFEREF_NO_CHECK
#endif

#if (MEMVAR_SAFEREF_CHECK != MEMVAR_SAFEREF_ABORT) && \
    (MEMVAR_SAFEREF_CHECK != MEMVAR_SAFEREF_THROWS) && \
    (MEMVAR_SAFEREF_CHECK != MEMVAR_SAFEREF_NO_CHECK)
  #error "MEMVAR_SAFEREF_CHECK must be MEMVAR_SAFEREF_ABORT, MEMVAR_SAFEREF_THROWS, or MEMVAR_SAFEREF_NO_CHECK"
#endif

// Forward declaration for bigint.h, used in unit tests
namespace bip { class bigint; }

namespace mv::detail {
// Encodes one code point as UTF-8 (invalid code points become U+FFFD).
inline void appendUtf8(std::string& out, char32_t cp) {
  if ( cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF) ) cp = 0xFFFD;
  if ( cp <= 0x7F ) {
    out.push_back(static_cast<char>(cp));
  }
  else if ( cp <= 0x7FF ) {
    out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  }
  else if ( cp <= 0xFFFF ) {
    out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
    out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  }
  else {
    out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
    out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  }
}

// wstring / u16string / u32string -> UTF-8. 16-bit code units (char16_t, and
// wchar_t on Windows) are combined into surrogate pairs; wider ones are code
// points already.
template <typename CharT>
std::string toUtf8(const std::basic_string<CharT>& s) {
  using Unit = std::make_unsigned_t<CharT>;
  std::string out;
  out.reserve(s.size());
  for ( std::size_t i {0}; i < s.size(); ) {
    char32_t cp {static_cast<Unit>(s[i++])};
    if constexpr ( sizeof(CharT) == 2 ) {
      if ( cp >= 0xD800 && cp <= 0xDBFF && i < s.size() ) {
        const char32_t low {static_cast<Unit>(s[i])};
        if ( low >= 0xDC00 && low <= 0xDFFF ) {
          cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
          ++i;
        }
      }
    }
    appendUtf8(out, cp);
  }
  return out;
}

[[noreturn]] inline void throwIndexOutOfRange(const int64_t index, const std::size_t size) {
  throw std::out_of_range("Index " + std::to_string(index) +
                          " is out of bounds (size is " + std::to_string(size) + ").");
}
}  // namespace mv::detail

// Stream output for the non-char standard strings (UTF-8 on the wire).
template <typename CharT>
  requires (std::same_as<CharT, wchar_t> || std::same_as<CharT, char16_t> || std::same_as<CharT, char32_t>)
  std::ostream& operator<<(std::ostream& os, const std::basic_string<CharT>& s) {
    return os << mv::detail::toUtf8(s);
}

inline std::ostream& operator<<(std::ostream& os, const std::u8string& s) {
  return os << std::string_view(reinterpret_cast<const char*>(s.data()), s.size());
}

namespace mv {
// Custom "big integer" types allowed in memvar<T>, besides arithmetic/strings.
template <typename T>
struct is_bigint : std::false_type {};

template <>
struct is_bigint<bip::bigint> : std::true_type {};

template <typename T>
inline constexpr bool is_bigint_v = is_bigint<T>::value;

template <typename T, typename... Types>
concept IsAnyOf = (std::same_as<T, Types> || ...);

template <typename T>
concept AnyStandardString = IsAnyOf<std::remove_cvref_t<T>,
                                    std::string,
                                    std::wstring,
                                    std::u8string,
                                    std::u16string,
                                    std::u32string>;

// Types allowed as memvar<T>'s T: integral (not bool), floating point,
// standard string, or bigint.
template <typename T>
concept MemVarType = (std::is_integral_v<std::remove_cvref_t<T>> &&
                      !std::same_as<std::remove_cvref_t<T>, bool>) ||
                      std::is_floating_point_v<std::remove_cvref_t<T>> ||
                      AnyStandardString<T> ||
                      is_bigint_v<std::remove_cvref_t<T>>;

#if MEMVAR_SAFEREF_CHECK != MEMVAR_SAFEREF_NO_CHECK
// Thrown by an expired SafeRef when MEMVAR_SAFEREF_CHECK == THROWS.
class expired_history_reference : public std::logic_error {
public:
  expired_history_reference() :
    std::logic_error("mv::memvar::cref(): the referenced value has been "
                     "evicted from history (too many writes, or a "
                     "clearHistory(), happened since cref() was called)") {}
};

#if MEMVAR_SAFEREF_CHECK == MEMVAR_SAFEREF_ABORT
// Failure path for MEMVAR_SAFEREF_ABORT. Never throws, so nothing calling it
// needs to be compiled as unwind-safe.
[[noreturn]] inline void reportExpiredHistoryReferenceAndAbort() noexcept {
  std::fprintf(stderr,
               "FATAL: mv::memvar::cref(): the referenced value has been "
               "evicted from history (too many writes, or a clearHistory(), "
               "happened since cref() was called). Terminating.\n");
  std::abort();
}
#endif
#endif  // MEMVAR_SAFEREF_CHECK != MEMVAR_SAFEREF_NO_CHECK


namespace detail {
// Smallest history capacity a variable accepts, and the default one.
inline constexpr int64_t minimumHistoryCapacity {2};
inline constexpr int64_t defaultHistoryCapacity {10};

// Used in the member-initialiser list, so it runs (and can throw) before the
// history container is constructed.
inline int64_t validateHistoryCapacity(const int64_t historyCapacity) {
  if ( historyCapacity >= minimumHistoryCapacity ) return historyCapacity;
  throw std::invalid_argument("ERROR: The history capacity must be " +
                              std::to_string(minimumHistoryCapacity) + " at least, or more");
}

// Prints "[ v v v ]\n"; the values are given by the two iterators.
template <typename It, typename Proj>
void printValues(std::ostream& os, const std::string& separator, It first, It last, Proj value) {
  os << "[ ";
  for ( ; first != last; ++first ) os << value(*first) << separator;
  os << "]\n";
}

#if MEMVAR_SAFEREF_CHECK != MEMVAR_SAFEREF_NO_CHECK
// Non-owning handle to the current value at the time cref() was called.
// It remembers the write count at which its slot will be evicted and checks
// that against the live write count of the variable on each dereference, so a
// stale handle fails instead of reading reused deque storage. No allocation,
// no refcounting; a passing check costs one load and one compare. It must not
// outlive the variable itself. Shared by memvar and memvarTimed, which hand it
// the address of their own write counter.
template <typename T>
class SafeRef {
public:
  SafeRef(const T* ptr, const int64_t writeSeq, const int64_t historyCapacity,
          const int64_t* writeCount) noexcept :
    ptr_(ptr), expiryWriteCount_(writeSeq + historyCapacity), writeCount_(writeCount) {}

  const T& operator*() const noexcept(MEMVAR_SAFEREF_CHECK != MEMVAR_SAFEREF_THROWS) {
    check();
    return *ptr_;
  }
  const T* operator->() const noexcept(MEMVAR_SAFEREF_CHECK != MEMVAR_SAFEREF_THROWS) {
    check();
    return ptr_;
  }
  operator const T&() const noexcept(MEMVAR_SAFEREF_CHECK != MEMVAR_SAFEREF_THROWS) {
    return operator*();
  }

private:
  void check() const noexcept(MEMVAR_SAFEREF_CHECK != MEMVAR_SAFEREF_THROWS) {
    if ( *writeCount_ >= expiryWriteCount_ ) [[unlikely]] {
#if MEMVAR_SAFEREF_CHECK == MEMVAR_SAFEREF_THROWS
      throw expired_history_reference();
#else
      reportExpiredHistoryReferenceAndAbort();
#endif
    }
  }

  const T* ptr_;
  int64_t expiryWriteCount_;
  const int64_t* writeCount_;
};
#endif  // MEMVAR_SAFEREF_CHECK != MEMVAR_SAFEREF_NO_CHECK

// The history shared by memvar and memvarTimed: a bounded deque of entries,
// newest at the front, plus the capacity and (when SafeRef checking is on) the
// write counter. Every invariant lives here, once:
//   - a write is a single insertion, the only step that can throw, so a
//     failed push() or reset() changes nothing (strong exception guarantee);
//   - the capacity is validated before the deque exists;
//   - indexing is bounds-checked in one place.
// What an entry is (a bare value, or a value with its time tag) is the
// owner's business: push()/reset() forward their arguments to the Entry
// constructor.
template <typename Entry>
class History {
public:
  using capacityType = int64_t;

  // validates the capacity first, then stores the first entry
  template <typename... Args>
  explicit History(const capacityType historyCapacity, Args&&... args) :
    capacity_(validateHistoryCapacity(historyCapacity)) {
    memo_.emplace_front(std::forward<Args>(args)...);
  }

  // new newest entry; the oldest one is dropped when the capacity is exceeded
  template <typename... Args>
  void push(Args&&... args) {
    memo_.emplace_front(std::forward<Args>(args)...);
    // nothing below can throw
#if MEMVAR_SAFEREF_CHECK != MEMVAR_SAFEREF_NO_CHECK
    ++writeCount_;
#endif
    if ( size() > capacity_ ) memo_.pop_back();
  }

  // Restarts from the given entry. It is inserted before the old ones are
  // erased, so a failure leaves the history untouched.
  template <typename... Args>
  void reset(Args&&... args) {
    memo_.emplace_front(std::forward<Args>(args)...);
    memo_.erase(std::next(memo_.begin()), memo_.end());
#if MEMVAR_SAFEREF_CHECK != MEMVAR_SAFEREF_NO_CHECK
    // every outstanding SafeRef is now dangling: force them all to expire
    writeCount_ += capacity_;
#endif
  }

  // The entry written i writes ago, bounds-checked.
  const Entry& at(const capacityType index) const {
    if ( index >= 0 && index < size() ) {
      return memo_[static_cast<std::size_t>(index)];
    }
    throwIndexOutOfRange(index, memo_.size());
  }

  // Same-expression access only: the reference dies with the next write.
  const Entry& front() const { return memo_.front(); }

  capacityType size() const noexcept { return static_cast<capacityType>(memo_.size()); }
  capacityType capacity() const noexcept { return capacity_; }
  bool full() const noexcept { return size() >= capacity_; }
  const std::deque<Entry>& entries() const noexcept { return memo_; }

#if MEMVAR_SAFEREF_CHECK != MEMVAR_SAFEREF_NO_CHECK
  // handle to a value inside the current front entry
  template <typename T>
  SafeRef<T> safeRef(const T* ptr) const {
    return SafeRef<T>(ptr, writeCount_, capacity_, &writeCount_);
  }
#endif

private:
  // declared first: its initialiser runs, and can throw, before memo_ exists
  const capacityType capacity_;
  std::deque<Entry> memo_ {};
#if MEMVAR_SAFEREF_CHECK != MEMVAR_SAFEREF_NO_CHECK
  // writes ever made (construction is #1); only SafeRef::check() reads it
  capacityType writeCount_ {1};
#endif
};
}  // namespace detail

template <MemVarType T> class memvar;
template <MemVarType T,
          typename Time = std::chrono::nanoseconds,
          typename Clock = std::chrono::steady_clock> class memvarTimed;

// Gives the free operators below access to the protected value/setter of both
// kinds of variable.
struct memvarAccess {
  template <MemVarType T>
  static const T& get(const memvar<T>& m) { return m.getValue(); }

  template <MemVarType T, typename Time, typename Clock>
  static const T& get(const memvarTimed<T, Time, Clock>& m) { return m.getValue(); }

  template <MemVarType T, typename U>
  static void set(memvar<T>& m, U&& v) { m.setValue(std::forward<U>(v)); }

  template <MemVarType T, typename Time, typename Clock, typename U>
  static void set(memvarTimed<T, Time, Clock>& m, U&& v) { m.setValue(std::forward<U>(v)); }
};

template <typename X>
concept IsMemvar = requires(const X& x) { memvarAccess::get(x); };

// The current value if x is a memvar (no copy), otherwise x itself.
template <typename X>
constexpr const auto& valueOf(const X& x) {
  if constexpr ( IsMemvar<X> ) return memvarAccess::get(x);
  else return x;
}

// "L Op R is valid on the underlying values and at least one side is a memvar".
template <typename Op, typename L, typename R>
concept MemvarOperands =
  (IsMemvar<L> || IsMemvar<R>) &&
  std::is_invocable_v<Op, decltype(valueOf(std::declval<const L&>())),
                          decltype(valueOf(std::declval<const R&>()))>;

namespace detail {

// One history entry of a memvarTimed: a value and the time it was written.
template <typename T, typename TimePoint>
struct TimedEntry {
  template <typename U>
  TimedEntry(U&& v, const TimePoint t) : value(std::forward<U>(v)), tag(t) {}
  T value;
  TimePoint tag;
};

// The interface shared by memvar and memvarTimed: owns the history and gives
// read access, assignment, ++/--, min/max and printing of the values. Entry is
// T itself (memvar) or TimedEntry<T, ...> (memvarTimed); only its value is used
// here. It is a CRTP mixin, not a polymorphic base: nothing is virtual, and
// Derived is never used through a pointer or reference to it. Derived
// supplies one private hook, setValue(U&&), because only it knows what else a
// write must store (the time tag).
template <typename Derived, MemVarType T, typename Entry>
class ValueInterface {
public:
  // capacityType: this type must be signed
  using capacityType = int64_t;

  // historyValue is a 2-tuple: a T value, and an error flag (false: no error)
  using historyValue = std::tuple<T, bool>;

#if MEMVAR_SAFEREF_CHECK != MEMVAR_SAFEREF_NO_CHECK
  using SafeRef = detail::SafeRef<T>;
  SafeRef cref() const { return history_.safeRef(&getValue()); }
#else
  const T& cref() const { return getValue(); }
#endif

  capacityType getHistoryCapacity() const noexcept { return history_.capacity(); }
  capacityType getHistorySize() const noexcept { return history_.size(); }
  bool isHistoryFull() const noexcept { return history_.full(); }
  const std::deque<Entry>& getMemVarHistory() const noexcept { return history_.entries(); }

  operator T() const { return getValue(); }
  T operator()() const { return getValue(); }
  T operator()(const capacityType index) const { return valueIn(history_.at(index)); }
  T operator[](const capacityType index) const { return valueIn(history_.at(index)); }
  T valueAt(const capacityType index) const { return valueIn(history_.at(index)); }

  historyValue getHistoryValue(const capacityType index) const {
    return std::make_tuple(valueIn(history_.at(index)), false);
  }

  auto getHistoryMinMax() const {
    const auto [min, max] = std::ranges::minmax_element(history_.entries(), std::less<>{}, valueIn);
    return std::make_tuple(valueIn(*min), valueIn(*max));
  }

  Derived& operator=(const T& rhs) { derived().setValue(rhs); return derived(); }
  Derived& operator=(T&& rhs) { derived().setValue(std::move(rhs)); return derived(); }

  // ++mv, --mv return the new value; mv++, mv-- the old one.
  T operator++() requires (!AnyStandardString<T>) {
    derived().setValue(static_cast<T>(getValue() + 1));
    return getValue();
  }
  T operator--() requires (!AnyStandardString<T>) {
    derived().setValue(static_cast<T>(getValue() - 1));
    return getValue();
  }
  T operator++(int) requires (!AnyStandardString<T>) {
    T old {getValue()};
    derived().setValue(static_cast<T>(old + 1));
    return old;
  }
  T operator--(int) requires (!AnyStandardString<T>) {
    T old {getValue()};
    derived().setValue(static_cast<T>(old - 1));
    return old;
  }

  // newest value first
  void printHistoryData(std::ostream& os = std::cout, const std::string& separator = " ") const {
    const auto& h {history_.entries()};
    printValues(os, separator, h.cbegin(), h.cend(), valueIn);
  }
  // oldest value first
  void printReverseHistoryData(std::ostream& os = std::cout, const std::string& separator = " ") const {
    const auto& h {history_.entries()};
    printValues(os, separator, h.crbegin(), h.crend(), valueIn);
  }

protected:
  // the history is built from the arguments of its first entry
  template <typename... Args>
  explicit ValueInterface(const capacityType historyCapacity, Args&&... firstEntry) :
    history_(historyCapacity, std::forward<Args>(firstEntry)...) {}

  // not deletable through this type; Derived's own destructor is trivial
  ~ValueInterface() = default;

  static const T& valueIn(const Entry& e) noexcept {
    if constexpr ( std::is_same_v<Entry, T> ) return e;
    else return e.value;
  }

  const T& getValue() const { return valueIn(history_.front()); }

  History<Entry> history_;

private:
  Derived& derived() noexcept { return static_cast<Derived&>(*this); }
};

}  // namespace detail

// memvar
// a variable with memory of old values
// only strings, integral or floating point types allowed
// The history is a std::deque<T>, newest value at the front.
template <MemVarType T>
class memvar : public detail::ValueInterface<memvar<T>, T, T> {
  using Base = detail::ValueInterface<memvar<T>, T, T>;
  friend Base;
  friend struct memvarAccess;
public:
  using typename Base::capacityType;
  using Base::operator=;

  memvar() : memvar(T{}) {}

  explicit memvar(const T& value, const capacityType historyCapacity = detail::defaultHistoryCapacity) :
    Base(historyCapacity, value) {}

  memvar(const memvar&) = delete;
  memvar(memvar&&) = delete;
  memvar& operator=(memvar&&) = delete;
  memvar& operator=(const memvar& rhs) { setValue(rhs.getValue()); return *this; }

  // Restarts from T{}.
  void clearHistory() { this->history_.reset(T{}); }

private:
  // U is const T& (copy) or T (move)
  template <typename U>
  void setValue(U&& value) { this->history_.push(std::forward<U>(value)); }
};  // class memvar

// memvarTimed
// Same as memvar, but every value also carries the time of its write. The
// history is a std::deque<Entry>; an Entry holds a value and its time point
// together, so the two are inserted and evicted as one unit and can never get
// out of step, whether a write succeeds or throws.
template <MemVarType T, typename Time, typename Clock>
class memvarTimed : public detail::ValueInterface<memvarTimed<T, Time, Clock>, T,
                      detail::TimedEntry<T, std::chrono::time_point<Clock, Time>>> {
public:
  using time_point = std::chrono::time_point<Clock, Time>;
  using Entry = detail::TimedEntry<T, time_point>;

private:
  using Base = detail::ValueInterface<memvarTimed, T, Entry>;
  friend Base;
  friend struct memvarAccess;

public:
  using typename Base::capacityType;
  using Base::operator=;

  // a T value, the time tag, and an error flag (false: no error)
  using historyTimedValue = std::tuple<T, Time, bool>;

  memvarTimed() : memvarTimed(T{}) {}

  // the epoch is the time of the first value
  explicit memvarTimed(const T& value, const capacityType historyCapacity = detail::defaultHistoryCapacity) :
    Base(historyCapacity, value, now()),
    memvarEpoch_(this->history_.front().tag) {}

  memvarTimed(const memvarTimed&) = delete;
  memvarTimed(memvarTimed&&) = delete;
  memvarTimed& operator=(memvarTimed&&) = delete;
  memvarTimed& operator=(const memvarTimed& rhs) { setValue(rhs.getValue()); return *this; }

  // time elapsed from the memvar's epoch (the time of its first value) to the
  // write of the i-th value
  Time getTimeTag(const capacityType index = 0) const {
    return std::chrono::duration_cast<Time>(this->history_.at(index).tag - memvarEpoch_);
  }

  historyTimedValue getHistoryTimedValue(const capacityType index) const {
    const Entry& e {this->history_.at(index)};
    return std::make_tuple(e.value, std::chrono::duration_cast<Time>(e.tag - memvarEpoch_), false);
  }

  void printHistoryTimedData(std::ostream& os = std::cout, const std::string& separator = "\n") const {
    printTimed(os, separator, false);
  }
  void printReverseHistoryTimedData(std::ostream& os = std::cout, const std::string& separator = "\n") const {
    printTimed(os, separator, true);
  }

  // Restarts from T{}; the new entry's time becomes the new epoch.
  void clearHistory() {
    this->history_.reset(T{}, now());
    memvarEpoch_ = this->history_.front().tag;
  }

private:
  static time_point now() { return std::chrono::time_point_cast<Time>(Clock::now()); }

  // U is const T& (copy) or T (move)
  template <typename U>
  void setValue(U&& value) { this->history_.push(std::forward<U>(value), now()); }

  void printTimed(std::ostream& os, const std::string& separator, const bool reverse) const {
    const auto& h {this->history_.entries()};
    const std::size_t n {h.size()};
    os << "{ --- begin ---\n[TimeTag:Value]\n";
    for ( std::size_t k {0}; k < n; ++k ) {
      const Entry& e {h[reverse ? n - 1 - k : k]};
      os << "[" << std::chrono::duration_cast<Time>(e.tag - memvarEpoch_).count()
         << ":" << e.value << "]" << separator;
    }
    os << "  --- end --- }\n\n";
  }

  time_point memvarEpoch_;
};  // class memvarTimed

template <typename T>
T getHistoryValue(const memvar<T>& mv, const typename memvar<T>::capacityType index) {
  return mv(index);
}

template <typename T, typename Time, typename Clock>
T getHistoryValue(const memvarTimed<T, Time, Clock>& mvt, const typename memvarTimed<T, Time, Clock>::capacityType index) {
  return mvt(index);
}

template <typename T, typename Time, typename Clock>
T getHistoryTimedValue(const memvarTimed<T, Time, Clock>& mvt, const typename memvarTimed<T, Time, Clock>::capacityType index) {
  return mvt(index);
}

// Binary operators between a memvar (bare or timed) and another memvar or a
// plain value, on either side. One constrained template per operator; operands
// are read by const reference.
#define MEMVAR_BINARY_OPERATOR(OP, FUNCTOR)                                       \
  template <typename L, typename R>                                               \
    requires MemvarOperands<FUNCTOR, L, R>                                        \
  auto operator OP(const L& lhs, const R& rhs) {                                  \
    return valueOf(lhs) OP valueOf(rhs);                                          \
  }

MEMVAR_BINARY_OPERATOR(==, std::equal_to<>)
MEMVAR_BINARY_OPERATOR(<,  std::less<>)
MEMVAR_BINARY_OPERATOR(>,  std::greater<>)
MEMVAR_BINARY_OPERATOR(<=, std::less_equal<>)
MEMVAR_BINARY_OPERATOR(>=, std::greater_equal<>)
MEMVAR_BINARY_OPERATOR(+,  std::plus<>)
MEMVAR_BINARY_OPERATOR(-,  std::minus<>)
MEMVAR_BINARY_OPERATOR(*,  std::multiplies<>)
MEMVAR_BINARY_OPERATOR(/,  std::divides<>)
#undef MEMVAR_BINARY_OPERATOR

// Compound assignment: the left side must be a memvar; it records a new value.
#define MEMVAR_COMPOUND_OPERATOR(OP, FUNCTOR)                                     \
  template <typename L, typename R>                                               \
    requires (IsMemvar<L> && MemvarOperands<FUNCTOR, L, R>)                       \
  L& operator OP##=(L& lhs, const R& rhs) {                                       \
    memvarAccess::set(lhs, valueOf(lhs) OP valueOf(rhs));                         \
    return lhs;                                                                   \
  }

MEMVAR_COMPOUND_OPERATOR(+, std::plus<>)
MEMVAR_COMPOUND_OPERATOR(-, std::minus<>)
MEMVAR_COMPOUND_OPERATOR(*, std::multiplies<>)
MEMVAR_COMPOUND_OPERATOR(/, std::divides<>)
#undef MEMVAR_COMPOUND_OPERATOR

}  // namespace mv

template <typename T>
std::ostream& operator<<(std::ostream& os, const mv::memvar<T>& mv) {
  return os << mv::valueOf(mv);
}

template <typename T, typename Time, typename Clock>
std::ostream& operator<<(std::ostream& os, const mv::memvarTimed<T, Time, Clock>& mvt) {
  return os << "[" << mvt.getTimeTag().count() << ":" << mv::valueOf(mvt) << "]";
}
