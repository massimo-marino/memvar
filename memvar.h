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

namespace mv
{
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
// Tag policy of memvarTimed: how a time tag is produced.
template <typename Time, typename Clock>
struct TimeTagPolicy {
  using tag_type = std::chrono::time_point<Clock, Time>;
  static tag_type now() { return std::chrono::time_point_cast<Time>(Clock::now()); }
};

// A history entry that carries a tag next to its value (timed variables).
template <typename T, typename Tag>
struct Stamped {
  template <typename U>
  Stamped(U&& v, const Tag t) : value(std::forward<U>(v)), tag(t) {}
  T value;
  Tag tag;
};

// Entry type stored in the history: T itself, or Stamped<T, tag> for a policy.
template <typename T, typename Policy>
struct SlotOf { using type = Stamped<T, typename Policy::tag_type>; };
template <typename T>
struct SlotOf<T, void> { using type = T; };
}  // namespace detail

// Policy == void: a plain variable. Otherwise the policy supplies the type of
// a tag (tag_type) and a function that makes one (now()), and every history
// entry holds its value and its tag together, so they are inserted and
// evicted as one unit.
template <MemVarType T, typename Policy = void> class memvar;

// Gives the free operators below access to memvar's protected value/setter.
struct memvarAccess {
  template <MemVarType T, typename P>
  static const T& get(const memvar<T, P>& m) { return m.getValue(); }

  template <MemVarType T, typename P, typename U>
  static void set(memvar<T, P>& m, U&& v) { m.setValue(std::forward<U>(v)); }
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

// memvar
// a variable with memory of old values
// only strings, integral or floating point types allowed
template <MemVarType T, typename Policy>
class memvar {
  friend struct memvarAccess;
public:
  // capacityType: this type must be signed
  using capacityType = int64_t;

  // historyValue is a 2-tuple: a T value, and an error flag (false: no error)
  using historyValue = std::tuple<T, bool>;

  // what the history deque holds: T for a plain memvar
  using Slot = typename detail::SlotOf<T, Policy>::type;

#if MEMVAR_SAFEREF_CHECK != MEMVAR_SAFEREF_NO_CHECK
  // Non-owning handle to the current value at the time cref() was called.
  // It remembers the write count at which its slot will be evicted and checks
  // that against the live write count on each dereference, so a stale handle
  // fails instead of reading reused deque storage. No allocation, no
  // refcounting; a passing check costs one load and one compare. It must not
  // outlive the memvar itself.
  class SafeRef {
  public:
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
    friend class memvar;
    SafeRef(const T* ptr, const capacityType writeSeq, const memvar* owner) noexcept :
      ptr_(ptr), expiryWriteCount_(writeSeq + owner->historyCapacity_), owner_(owner) {}

    void check() const noexcept(MEMVAR_SAFEREF_CHECK != MEMVAR_SAFEREF_THROWS) {
      if ( owner_->writeCount_ >= expiryWriteCount_ ) [[unlikely]] {
#if MEMVAR_SAFEREF_CHECK == MEMVAR_SAFEREF_THROWS
        throw expired_history_reference();
#else
        reportExpiredHistoryReferenceAndAbort();
#endif
      }
    }

    const T* ptr_;
    capacityType expiryWriteCount_;
    const memvar* owner_;
  };

  SafeRef cref() const { return SafeRef(&getValue(), writeCount_, this); }
#else
  const T& cref() const { return getValue(); }
#endif

  memvar() : memvar(T{}) {}

  explicit memvar(const T& value, const capacityType historyCapacity = historyCapacityDefault_) :
    historyCapacity_(validateHistoryCapacity(historyCapacity)) {
    pushFront(value);
  }

  virtual ~memvar() = default;

  memvar(const memvar&) = delete;
  memvar(memvar&&) = delete;
  memvar& operator=(memvar&&) = delete;

  capacityType getHistoryCapacity() const noexcept { return historyCapacity_; }
  capacityType getHistorySize() const noexcept { return static_cast<capacityType>(memo_.size()); }
  bool isHistoryFull() const noexcept { return getHistorySize() >= historyCapacity_; }
  const auto& getMemVarHistory() const noexcept { return memo_; }

  operator T() const { return getValue(); }
  T operator()() const { return getValue(); }
  T operator()(const capacityType index) const { return slotValue(slot(index)); }
  T operator[](const capacityType index) const { return slotValue(slot(index)); }
  T valueAt(const capacityType index) const { return slotValue(slot(index)); }

  historyValue getHistoryValue(const capacityType index) const {
    return std::make_tuple(slotValue(slot(index)), false);
  }

  auto getHistoryMinMax() const {
    const auto [min, max] = std::ranges::minmax_element(
        memo_, std::less<>{}, [](const Slot& s) -> const T& { return slotValue(s); });
    return std::make_tuple(slotValue(*min), slotValue(*max));
  }

  memvar& operator=(const T& rhs) { setValue(rhs); return *this; }
  memvar& operator=(T&& rhs) { setValue(std::move(rhs)); return *this; }
  memvar& operator=(const memvar& rhs) { setValue(rhs.getValue()); return *this; }

  // ++mv, --mv return the new value; mv++, mv-- the old one.
  T operator++() requires (!AnyStandardString<T>) {
    setValue(static_cast<T>(getValue() + 1));
    return getValue();
  }
  T operator--() requires (!AnyStandardString<T>) {
    setValue(static_cast<T>(getValue() - 1));
    return getValue();
  }
  T operator++(int) requires (!AnyStandardString<T>) {
    T old {getValue()};
    setValue(static_cast<T>(old + 1));
    return old;
  }
  T operator--(int) requires (!AnyStandardString<T>) {
    T old {getValue()};
    setValue(static_cast<T>(old - 1));
    return old;
  }

  // newest value first
  void printHistoryData(std::ostream& os = std::cout, const std::string& separator = " ") const {
    print(os, separator, false);
  }
  // oldest value first
  void printReverseHistoryData(std::ostream& os = std::cout, const std::string& separator = " ") const {
    print(os, separator, true);
  }

  // Restarts from T{}. The new entry is inserted before the old ones are
  // erased, so a failure leaves the history untouched.
  virtual void clearHistory() {
    pushFront(T{});
    memo_.erase(std::next(memo_.begin()), memo_.end());
#if MEMVAR_SAFEREF_CHECK != MEMVAR_SAFEREF_NO_CHECK
    // every outstanding SafeRef is now dangling: force them all to expire
    writeCount_ += historyCapacity_;
#endif
  }

protected:
  static constexpr capacityType minimumHistoryCapacity_ {2};
  static constexpr capacityType historyCapacityDefault_ {10};

  // Runs ahead of memo_'s construction (historyCapacity_ is declared first).
  static capacityType validateHistoryCapacity(const capacityType historyCapacity) {
    if ( historyCapacity >= minimumHistoryCapacity_ ) return historyCapacity;
    throw std::invalid_argument("ERROR: The history capacity must be " +
                                std::to_string(minimumHistoryCapacity_) + " at least, or more");
  }

  static const T& slotValue(const Slot& s) noexcept {
    if constexpr ( std::is_void_v<Policy> ) return s;
    else return s.value;
  }

  const capacityType historyCapacity_ {historyCapacityDefault_};
  std::deque<Slot> memo_ {};
#if MEMVAR_SAFEREF_CHECK != MEMVAR_SAFEREF_NO_CHECK
  // writes ever made (construction is #1); only SafeRef::check() reads it
  capacityType writeCount_ {1};
#endif

  // Same-expression access only: the reference dies with the next write.
  const T& getValue() const { return slotValue(memo_.front()); }

  // The entry written i writes ago, bounds-checked.
  const Slot& slot(const capacityType index) const {
    if ( index >= 0 && index < getHistorySize() ) {
      return memo_[static_cast<std::size_t>(index)];
    }
    detail::throwIndexOutOfRange(index, memo_.size());
  }

  // Inserts a new newest entry (value plus, for a policy, its tag). This is
  // the only operation of a write that can throw, and it is a single
  // insertion into a single container, so a failure changes nothing.
  template <typename U>
  void pushFront(U&& value) {
    if constexpr ( std::is_void_v<Policy> ) memo_.emplace_front(std::forward<U>(value));
    else memo_.emplace_front(std::forward<U>(value), Policy::now());
  }

  // Single write path; U is const T& (copy) or T (move).
  template <typename U>
  void setValue(U&& value) {
    pushFront(std::forward<U>(value));
    // nothing below can throw
#if MEMVAR_SAFEREF_CHECK != MEMVAR_SAFEREF_NO_CHECK
    ++writeCount_;
#endif
    if ( getHistorySize() > historyCapacity_ ) memo_.pop_back();
  }

private:
  void print(std::ostream& os, const std::string& separator, const bool reverse) const {
    os << "[ ";
    if ( reverse ) {
      for ( auto it {memo_.crbegin()}; it != memo_.crend(); ++it ) os << slotValue(*it) << separator;
    }
    else {
      for ( const auto& item : memo_ ) os << slotValue(item) << separator;
    }
    os << "]\n";
  }
};  // class memvar

// memvar whose every value also carries a time tag. Each history entry holds
// the value and its time point together (one deque), so the two can never get
// out of step, whether a write succeeds or throws.
template <MemVarType T,
          typename Time = std::chrono::nanoseconds,
          typename Clock = std::chrono::steady_clock>
class memvarTimed final : public memvar<T, detail::TimeTagPolicy<Time, Clock>> {
  using MemvarBase = memvar<T, detail::TimeTagPolicy<Time, Clock>>;
public:
  using capacityType = typename MemvarBase::capacityType;
  // a T value, the time tag, and an error flag (false: no error)
  using historyTimedValue = std::tuple<T, Time, bool>;

  memvarTimed() : memvarTimed(T{}) {}

  explicit memvarTimed(const T& value, const capacityType historyCapacity = MemvarBase::historyCapacityDefault_) :
    MemvarBase(value, historyCapacity), memvarEpoch_(this->memo_.front().tag) {}

  memvarTimed(const memvarTimed&) = delete;
  memvarTimed(memvarTimed&&) = delete;
  memvarTimed& operator=(memvarTimed&&) = delete;

  // Not inherited from MemvarBase on purpose: MemvarBase::operator= would return a MemvarBase&.
  memvarTimed& operator=(const T& rhs) { this->setValue(rhs); return *this; }
  memvarTimed& operator=(T&& rhs) { this->setValue(std::move(rhs)); return *this; }
  memvarTimed& operator=(const memvarTimed& rhs) { this->setValue(rhs.getValue()); return *this; }

  // time elapsed from the memvar's epoch (the time of its first value) to the
  // write of the i-th value
  Time getTimeTag(const capacityType index = 0) const {
    return std::chrono::duration_cast<Time>(this->slot(index).tag - memvarEpoch_);
  }

  historyTimedValue getHistoryTimedValue(const capacityType index) const {
    const auto& s {this->slot(index)};
    return std::make_tuple(s.value, std::chrono::duration_cast<Time>(s.tag - memvarEpoch_), false);
  }

  void printHistoryTimedData(std::ostream& os = std::cout, const std::string& separator = "\n") const {
    printTimed(os, separator, false);
  }
  void printReverseHistoryTimedData(std::ostream& os = std::cout, const std::string& separator = "\n") const {
    printTimed(os, separator, true);
  }

  void clearHistory() override {
    MemvarBase::clearHistory();                // single new entry, tagged now
    memvarEpoch_ = this->memo_.front().tag;    // and it is the new epoch
  }

private:
  void printTimed(std::ostream& os, const std::string& separator, const bool reverse) const {
    const std::size_t n {this->memo_.size()};
    os << "{ --- begin ---\n[TimeTag:Value]\n";
    for ( std::size_t k {0}; k < n; ++k ) {
      const auto& s {this->memo_[reverse ? n - 1 - k : k]};
      os << "[" << std::chrono::duration_cast<Time>(s.tag - memvarEpoch_).count()
         << ":" << s.value << "]" << separator;
    }
    os << "  --- end --- }\n\n";
  }

  typename detail::TimeTagPolicy<Time, Clock>::tag_type memvarEpoch_;
};  // class memvarTimed

template <typename T, typename P>
T getHistoryValue(const memvar<T, P>& mv, const typename memvar<T, P>::capacityType index) {
  return mv(index);
}

template <typename T, typename Time, typename Clock>
T getHistoryTimedValue(const memvarTimed<T, Time, Clock>& mvt, const typename memvarTimed<T, Time, Clock>::capacityType index) {
  return mvt(index);
}

// Binary operators between a memvar (bare or timed) and another memvar or a
// plain value, on either side. One constrained template per operator replaces
// the former three overloads each; operands are read by const reference.
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

template <typename T, typename P>
std::ostream& operator<<(std::ostream& os, const mv::memvar<T, P>& mv) {
  return os << mv::valueOf(mv);
}

template <typename T, typename Time, typename Clock>
std::ostream& operator<<(std::ostream& os, const mv::memvarTimed<T, Time, Clock>& mvt) {
  return os << "[" << mvt.getTimeTag().count() << ":" << mv::valueOf(mvt) << "]";
}
