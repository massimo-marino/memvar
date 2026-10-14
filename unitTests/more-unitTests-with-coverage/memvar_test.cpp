//
// memvar_test.cpp
//
// GoogleTest suite for memvar.h: mv::memvar<T> and mv::memvarTimed<T, Time, Clock>
// (two independent, non-polymorphic classes sharing a CRTP interface mixin and a
// detail::History container).
//
// Build notes
//  * The behaviour of memvar<T>::cref() depends on MEMVAR_SAFEREF_CHECK
//    (0 = abort, 1 = throw, 2 = no check, the default). The suite adapts to the
//    mode it is compiled with, so build it three times to run everything:
//      -DMEMVAR_SAFEREF_CHECK=0   (abort path, covered with death tests)
//      -DMEMVAR_SAFEREF_CHECK=1   (expired_history_reference)
//      -DMEMVAR_SAFEREF_CHECK=2   (plain const T&)
//  * Coverage: build with --coverage and run the three modes. The abort path of
//    mode 0 only ever runs inside death-test child processes, which die before
//    gcov can write their counters; compile with -DMEMVAR_TEST_GCOV_DUMP (together
//    with --coverage) to make those children dump their counters on SIGABRT.
//  * Avoid mixing signed and unsigned operands in comparisons here: memvar.h
//    forwards them unchanged, so -Wsign-compare would fire inside the header.
//
#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdint>
#include <deque>
#include <functional>
#include <iostream>
#include <limits>
#include <ratio>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>

// -----------------------------------------------------------------------------
// A minimal, self-contained "bigint" type used only to exercise the
// mv::is_bigint<T> specialization point declared in memvar.h. memvar.h only
// forward-declares bip::bigint, so a concrete definition is required here to
// be able to instantiate mv::memvar<bip::bigint>.
//
// It is also instrumented for the tests: it counts copies and moves (to prove
// which overload of operator= / which constructor ran) and it can be told to
// throw from its copy and move constructors (to prove that a failed write
// leaves the variable untouched).
// -----------------------------------------------------------------------------
namespace bip {
class bigint {
public:
  struct construction_error : std::runtime_error {
    using std::runtime_error::runtime_error;
  };

  static inline int copies {0};
  static inline int moves {0};
  static inline bool failOnCopyOrMove {false};
  static void resetCounters() { copies = 0; moves = 0; }

  bigint() = default;
  bigint(long long v) : value_(v) {}  // NOLINT - intentional implicit conversion
  bigint(const bigint& o) : value_(o.value_) {
    if (failOnCopyOrMove) throw construction_error("bigint copy failed");
    ++copies;
  }
  bigint(bigint&& o) : value_(o.value_) {
    if (failOnCopyOrMove) throw construction_error("bigint move failed");
    ++moves;
  }
  bigint& operator=(const bigint&) = default;
  bigint& operator=(bigint&&) = default;

  long long value() const { return value_; }

  bigint operator+(const bigint& rhs) const { return bigint(value_ + rhs.value_); }
  bigint operator-(const bigint& rhs) const { return bigint(value_ - rhs.value_); }
  bigint operator*(const bigint& rhs) const { return bigint(value_ * rhs.value_); }
  bigint operator/(const bigint& rhs) const { return bigint(value_ / rhs.value_); }

  bool operator==(const bigint& rhs) const { return value_ == rhs.value_; }
  auto operator<=>(const bigint& rhs) const { return value_ <=> rhs.value_; }

private:
  long long value_ {0};
};

inline std::ostream& operator<<(std::ostream& os, const bigint& b) {
  return os << b.value();
}
}  // namespace bip

// memvar.h must be included AFTER bip::bigint is fully defined so that the
// is_bigint<bip::bigint> specialization refers to a complete type consistently
// (the forward declaration in memvar.h is sufficient for the specialization
// itself, but our test code needs the full type to construct values).
#include "memvar.h"

namespace {

using mv::memvar;
using mv::memvarTimed;

// -----------------------------------------------------------------------------
// Detection concepts used by the compile-time checks and by tests that must
// prove an expression is NOT available. They are SFINAE-friendly: an
// unavailable operator makes the concept false instead of failing to compile.
// -----------------------------------------------------------------------------
template <typename L, typename R> concept CanPlusEq   = requires(L& l, const R& r) { l += r; };
template <typename L, typename R> concept CanMinusEq  = requires(L& l, const R& r) { l -= r; };
template <typename L, typename R> concept CanTimesEq  = requires(L& l, const R& r) { l *= r; };
template <typename L, typename R> concept CanDivideEq = requires(L& l, const R& r) { l /= r; };
template <typename L, typename R> concept CanAdd      = requires(const L& l, const R& r) { l + r; };
template <typename L, typename R> concept CanSub      = requires(const L& l, const R& r) { l - r; };
template <typename L, typename R> concept CanEqual    = requires(const L& l, const R& r) { l == r; };
template <typename L, typename R> concept CanLess     = requires(const L& l, const R& r) { l < r; };
template <typename M> concept CanPreInc  = requires(M& m) { ++m; };
template <typename M> concept CanPreDec  = requires(M& m) { --m; };
template <typename M> concept CanPostInc = requires(M& m) { m++; };
template <typename M> concept CanPostDec = requires(M& m) { m--; };

// -----------------------------------------------------------------------------
// A deterministic clock for memvarTimed. Time only moves when a test says so, so
// time tags can be checked exactly. Its native duration is 1 ms.
// -----------------------------------------------------------------------------
struct FakeClock {
  using rep = std::int64_t;
  using period = std::milli;
  using duration = std::chrono::duration<rep, period>;
  using time_point = std::chrono::time_point<FakeClock>;
  [[maybe_unused]] static constexpr bool is_steady = true;

  static inline rep ticks {1000};
  static time_point now() noexcept { return time_point{duration{ticks}}; }
  static void advance(const rep ms) noexcept { ticks += ms; }
};

using FakeTimed = memvarTimed<int, std::chrono::milliseconds, FakeClock>;

// Resets the fake clock before every test of a fixture.
class FakeClockTest : public ::testing::Test {
protected:
  void SetUp() override { FakeClock::ticks = 1000; }
};

// m = m without tripping -Wself-assign-overloaded (the self-assignment is the
// point of the tests that use it).
template <typename M>
void selfAssign(M& m) {
  M& same = m;
  m = same;
}

// Death tests: with MEMVAR_TEST_GCOV_DUMP (coverage builds only) the dying child
// writes its gcov counters before it terminates, so the abort path is measured.
#ifdef MEMVAR_TEST_GCOV_DUMP
extern "C" void __gcov_dump(void);
inline void dumpCoverageAndExit(int) {
  __gcov_dump();
  std::_Exit(134);
}
#define MEMVAR_DIE_WITH_COVERAGE(statement) \
  do { std::signal(SIGABRT, dumpCoverageAndExit); statement; } while (false)
#else
#define MEMVAR_DIE_WITH_COVERAGE(statement) statement
#endif

// While alive, copying or moving a bip::bigint throws. This is how a write
// that fails half way is provoked.
struct FailCopiesAndMoves {
  FailCopiesAndMoves() { bip::bigint::failOnCopyOrMove = true; }
  ~FailCopiesAndMoves() { bip::bigint::failOnCopyOrMove = false; }
  FailCopiesAndMoves(const FailCopiesAndMoves&) = delete;
  FailCopiesAndMoves& operator=(const FailCopiesAndMoves&) = delete;
};

// Redirects std::cout into a string for the lifetime of the object.
class CoutCapture {
public:
  CoutCapture() : old_(std::cout.rdbuf(buf_.rdbuf())) {}
  ~CoutCapture() { std::cout.rdbuf(old_); }
  CoutCapture(const CoutCapture&) = delete;
  CoutCapture& operator=(const CoutCapture&) = delete;
  std::string str() const { return buf_.str(); }
private:
  std::ostringstream buf_;
  std::streambuf* old_;
};

// =============================================================================
// Section 1: Construction / basic accessors (int specialization)
// =============================================================================

TEST(MemvarConstruction, DefaultConstructorInitializesToZeroValue) {
  memvar<int> m;
  EXPECT_EQ(m(), 0);
  EXPECT_EQ(static_cast<int>(m), 0);
  EXPECT_EQ(m.getHistorySize(), 1);
}

TEST(MemvarConstruction, DefaultConstructorUsesDefaultCapacity) {
  memvar<int> m;
  EXPECT_EQ(m.getHistoryCapacity(), 10);
}

TEST(MemvarConstruction, ValueConstructorSetsInitialValue) {
  memvar<int> m(42);
  EXPECT_EQ(m(), 42);
  EXPECT_EQ(m.getHistorySize(), 1);
}

TEST(MemvarConstruction, ValueConstructorWithExplicitCapacity) {
  memvar<int> m(5, 3);
  EXPECT_EQ(m(), 5);
  EXPECT_EQ(m.getHistoryCapacity(), 3);
}

TEST(MemvarConstruction, ValueConstructorAtMinimumCapacityIsAllowed) {
  EXPECT_NO_THROW({ memvar<int> m(1, 2); });
  memvar<int> m(1, 2);
  EXPECT_EQ(m.getHistoryCapacity(), 2);
}

TEST(MemvarConstruction, ValueConstructorBelowMinimumCapacityThrows) {
  EXPECT_THROW({ memvar<int> m(1, 1); }, std::invalid_argument);
  EXPECT_THROW({ memvar<int> m(1, 0); }, std::invalid_argument);
  EXPECT_THROW({ memvar<int> m(1, -5); }, std::invalid_argument);
}

TEST(MemvarConstruction, ValueConstructorThrowMessageMentionsMinimum) {
  try {
    memvar<int> m(1, 1);
    FAIL() << "expected std::invalid_argument";
  } catch (const std::invalid_argument& e) {
    EXPECT_NE(std::string(e.what()).find("2"), std::string::npos);
  }
}

TEST(MemvarConstruction, IsNotCopyConstructible) {
  EXPECT_FALSE(std::is_copy_constructible_v<memvar<int>>);
}

TEST(MemvarConstruction, IsNotMoveConstructible) {
  EXPECT_FALSE(std::is_move_constructible_v<memvar<int>>);
}

TEST(MemvarConstruction, IsNotMoveAssignable) {
  EXPECT_FALSE(std::is_move_assignable_v<memvar<int>>);
}

TEST(MemvarConstruction, IsCopyAssignable) {
  // operator=(const memvar&) is defined (though it is a "value push", not a
  // full history copy) so copy-assignment is expected to be available.
  EXPECT_TRUE(std::is_copy_assignable_v<memvar<int>>);
}

// =============================================================================
// Section 2: Reading current / historical values
// =============================================================================

TEST(MemvarAccess, FunctionCallOperatorReturnsCurrentValue) {
  memvar<int> m(7);
  EXPECT_EQ(m(), 7);
}

TEST(MemvarAccess, ConversionOperatorReturnsCurrentValue) {
  memvar<int> m(7);
  int v = m;
  EXPECT_EQ(v, 7);
}

TEST(MemvarAccess, IndexZeroIsMostRecentValue) {
  memvar<int> m(1);
  m = 2;
  m = 3;
  EXPECT_EQ(m(0), 3);
  EXPECT_EQ(m[0], 3);
}

TEST(MemvarAccess, IndexOrderingNewestToOldest) {
  memvar<int> m(1);
  m = 2;
  m = 3;
  // history (front..back) = [3, 2, 1]
  EXPECT_EQ(m(0), 3);
  EXPECT_EQ(m(1), 2);
  EXPECT_EQ(m(2), 1);
  EXPECT_EQ(m[0], 3);
  EXPECT_EQ(m[1], 2);
  EXPECT_EQ(m[2], 1);
}

TEST(MemvarAccess, OperatorParenAndBracketAgree) {
  memvar<int> m(10);
  m = 20;
  for (memvar<int>::capacityType i = 0; i < m.getHistorySize(); ++i) {
    EXPECT_EQ(m(i), m[i]);
  }
}

TEST(MemvarAccess, NegativeIndexThrowsOutOfRange) {
  memvar<int> m(1);
  EXPECT_THROW(m(-1), std::out_of_range);
  EXPECT_THROW(m[-1], std::out_of_range);
}

TEST(MemvarAccess, IndexEqualToSizeThrowsOutOfRange) {
  memvar<int> m(1);
  EXPECT_THROW(m(m.getHistorySize()), std::out_of_range);
}

TEST(MemvarAccess, IndexFarBeyondSizeThrowsOutOfRange) {
  memvar<int> m(1);
  EXPECT_THROW(m(1000), std::out_of_range);
}

TEST(MemvarAccess, GetHistoryValueReturnsFalseErrorFlagOnSuccess) {
  memvar<int> m(9);
  auto [value, error] = m.getHistoryValue(0);
  EXPECT_EQ(value, 9);
  EXPECT_FALSE(error);
}

TEST(MemvarAccess, FreeFunctionGetHistoryValueMatchesMemberIndexing) {
  memvar<int> m(1);
  m = 2;
  m = 3;
  EXPECT_EQ(mv::getHistoryValue(m, 0), 3);
  EXPECT_EQ(mv::getHistoryValue(m, 1), 2);
  EXPECT_EQ(mv::getHistoryValue(m, 2), 1);
}

// =============================================================================
// Section 3: Assignment / history growth and capacity eviction
// =============================================================================

TEST(MemvarAssignment, AssignFromValueUpdatesCurrentAndGrowsHistory) {
  memvar<int> m(1);
  m = 2;
  EXPECT_EQ(m(), 2);
  EXPECT_EQ(m.getHistorySize(), 2);
}

TEST(MemvarAssignment, AssignFromAnotherMemvarPushesItsCurrentValue) {
  memvar<int> a(1);
  memvar<int> b(99);
  a = b;
  EXPECT_EQ(a(), 99);
  // history grew by one entry (push), it did not replace a's whole history
  EXPECT_EQ(a.getHistorySize(), 2);
}

TEST(MemvarAssignment, HistoryCapacityEvictsOldestValue) {
  memvar<int> m(0, 3);  // capacity 3
  m = 1;
  m = 2;
  m = 3;  // history should now be capped at 3 entries: [3,2,1] -> 0 evicted
  EXPECT_EQ(m.getHistorySize(), 3);
  EXPECT_TRUE(m.isHistoryFull());
  EXPECT_EQ(m(0), 3);
  EXPECT_EQ(m(1), 2);
  EXPECT_EQ(m(2), 1);
  EXPECT_THROW(m(3), std::out_of_range);
}

TEST(MemvarAssignment, IsHistoryFullFalseBelowCapacity) {
  memvar<int> m(0, 5);
  EXPECT_FALSE(m.isHistoryFull());
  m = 1;
  m = 2;
  EXPECT_FALSE(m.isHistoryFull());
}

TEST(MemvarAssignment, ManyAssignmentsRespectMinimumCapacity) {
  memvar<int> m(0, 2);
  for (int i = 1; i <= 50; ++i) {
    m = i;
  }
  EXPECT_EQ(m.getHistorySize(), 2);
  EXPECT_EQ(m(0), 50);
  EXPECT_EQ(m(1), 49);
}

TEST(MemvarAssignment, DefaultCapacityEvictionAfterElevenPushes) {
  memvar<int> m;  // starts with one value: 0, default capacity 10
  for (int i = 1; i <= 10; ++i) {
    m = i;
  }
  // 11 values pushed total (0..10), capacity 10 -> oldest (0) evicted
  EXPECT_EQ(m.getHistorySize(), 10);
  EXPECT_EQ(m(9), 1);  // oldest remaining
  EXPECT_EQ(m(0), 10);  // newest
}

// =============================================================================
// Section 4: Compound assignment operators
// =============================================================================

TEST(MemvarCompoundAssignment, PlusEqualsValue) {
  memvar<int> m(5);
  m += 3;
  EXPECT_EQ(m(), 8);
}

TEST(MemvarCompoundAssignment, PlusEqualsMemvar) {
  memvar<int> a(5);
  memvar<int> b(3);
  a += b;
  EXPECT_EQ(a(), 8);
  EXPECT_EQ(b(), 3);  // rhs unaffected
}

TEST(MemvarCompoundAssignment, MinusEqualsValue) {
  memvar<int> m(5);
  m -= 3;
  EXPECT_EQ(m(), 2);
}

TEST(MemvarCompoundAssignment, MinusEqualsMemvar) {
  memvar<int> a(5);
  memvar<int> b(3);
  a -= b;
  EXPECT_EQ(a(), 2);
}

TEST(MemvarCompoundAssignment, TimesEqualsValue) {
  memvar<int> m(5);
  m *= 4;
  EXPECT_EQ(m(), 20);
}

TEST(MemvarCompoundAssignment, TimesEqualsMemvar) {
  memvar<int> a(5);
  memvar<int> b(4);
  a *= b;
  EXPECT_EQ(a(), 20);
}

TEST(MemvarCompoundAssignment, DivideEqualsValue) {
  memvar<int> m(20);
  m /= 4;
  EXPECT_EQ(m(), 5);
}

TEST(MemvarCompoundAssignment, DivideEqualsMemvar) {
  memvar<int> a(20);
  memvar<int> b(4);
  a /= b;
  EXPECT_EQ(a(), 5);
}

TEST(MemvarCompoundAssignment, FloatingPointDivideByZeroYieldsInfNoThrow) {
  memvar<double> m(1.0);
  EXPECT_NO_THROW(m /= 0.0);
  EXPECT_TRUE(std::isinf(m()));
}

TEST(MemvarCompoundAssignment, CompoundAssignmentsGrowHistory) {
  memvar<int> m(1, 5);
  m += 1;
  m -= 1;
  m *= 2;
  m /= 2;
  EXPECT_EQ(m.getHistorySize(), 5);
}

// =============================================================================
// Section 5: Increment / decrement operators
// =============================================================================

TEST(MemvarIncrement, PrefixIncrementReturnsNewValue) {
  memvar<int> m(5);
  int result = ++m;
  EXPECT_EQ(result, 6);
  EXPECT_EQ(m(), 6);
}

TEST(MemvarIncrement, PostfixIncrementReturnsPreIncrementValue) {
  // Idiomatic postfix semantics: the old value is returned, the new one is
  // pushed onto the history.
  memvar<int> m(5);
  int result = m++;
  EXPECT_EQ(result, 5);
  EXPECT_EQ(m(), 6);
}

TEST(MemvarIncrement, PrefixDecrementReturnsNewValue) {
  memvar<int> m(5);
  int result = --m;
  EXPECT_EQ(result, 4);
  EXPECT_EQ(m(), 4);
}

TEST(MemvarIncrement, PostfixDecrementReturnsPreDecrementValue) {
  memvar<int> m(5);
  int result = m--;
  EXPECT_EQ(result, 5);
  EXPECT_EQ(m(), 4);
}

TEST(MemvarIncrement, IncrementGrowsHistory) {
  memvar<int> m(0, 10);
  ++m;
  m++;
  --m;
  m--;
  EXPECT_EQ(m.getHistorySize(), 5);
  EXPECT_EQ(m(), 0);
}

TEST(MemvarIncrement, RepeatedIncrementSequenceIsCorrect) {
  memvar<int> m(0);
  for (int i = 0; i < 5; ++i) {
    ++m;
  }
  EXPECT_EQ(m(), 5);
  EXPECT_EQ(m(1), 4);
  EXPECT_EQ(m(5), 0);
}

// =============================================================================
// Section 6: clearHistory / getHistorySize / getMemVarHistory / getHistoryMinMax
// =============================================================================

TEST(MemvarHistoryOps, ClearHistoryResetsToSingleDefaultValue) {
  memvar<int> m(5);
  m = 6;
  m = 7;
  m.clearHistory();
  EXPECT_EQ(m.getHistorySize(), 1);
  EXPECT_EQ(m(), 0);
}

TEST(MemvarHistoryOps, GetMemVarHistoryReflectsPushedValues) {
  memvar<int> m(1);
  m = 2;
  m = 3;
  const auto& hist = m.getMemVarHistory();
  ASSERT_EQ(hist.size(), 3u);
  EXPECT_EQ(hist[0], 3);
  EXPECT_EQ(hist[1], 2);
  EXPECT_EQ(hist[2], 1);
}

TEST(MemvarHistoryOps, GetHistoryMinMaxOnSingleValue) {
  memvar<int> m(42);
  auto [minV, maxV] = m.getHistoryMinMax();
  EXPECT_EQ(minV, 42);
  EXPECT_EQ(maxV, 42);
}

TEST(MemvarHistoryOps, GetHistoryMinMaxAcrossMultipleValues) {
  memvar<int> m(5);
  m = 1;
  m = 9;
  m = 3;
  auto [minV, maxV] = m.getHistoryMinMax();
  EXPECT_EQ(minV, 1);
  EXPECT_EQ(maxV, 9);
}

TEST(MemvarHistoryOps, GetHistoryMinMaxWithNegativeValues) {
  memvar<int> m(-5);
  m = -100;
  m = 3;
  auto [minV, maxV] = m.getHistoryMinMax();
  EXPECT_EQ(minV, -100);
  EXPECT_EQ(maxV, 3);
}

// =============================================================================
// Section 7: Printing (printHistoryData / printReverseHistoryData)
// =============================================================================

TEST(MemvarPrinting, PrintHistoryDataSingleValue) {
  memvar<int> m(42);
  std::ostringstream os;
  m.printHistoryData(os);
  EXPECT_EQ(os.str(), "[ 42 ]\n");
}

TEST(MemvarPrinting, PrintHistoryDataMultipleValuesNewestFirst) {
  memvar<int> m(1);
  m = 2;
  m = 3;
  std::ostringstream os;
  m.printHistoryData(os);
  EXPECT_EQ(os.str(), "[ 3 2 1 ]\n");
}

TEST(MemvarPrinting, PrintReverseHistoryDataOldestFirst) {
  memvar<int> m(1);
  m = 2;
  m = 3;
  std::ostringstream os;
  m.printReverseHistoryData(os);
  EXPECT_EQ(os.str(), "[ 1 2 3 ]\n");
}

TEST(MemvarPrinting, PrintHistoryDataCustomSeparator) {
  memvar<int> m(1);
  m = 2;
  std::ostringstream os;
  m.printHistoryData(os, ", ");
  EXPECT_EQ(os.str(), "[ 2, 1, ]\n");
}

// =============================================================================
// Section 9: Comparison operators (free functions)
// =============================================================================

TEST(MemvarComparison, EqualityMemvarToMemvar) {
  memvar<int> a(5);
  memvar<int> b(5);
  memvar<int> c(6);
  EXPECT_TRUE(a == b);
  EXPECT_FALSE(a == c);
}

TEST(MemvarComparison, InequalityIsSynthesizedFromEquality) {
  memvar<int> a(5);
  memvar<int> b(6);
  EXPECT_TRUE(a != b);
  EXPECT_FALSE(a != a);
}

TEST(MemvarComparison, EqualityMemvarToValueBothDirections) {
  memvar<int> a(5);
  EXPECT_TRUE(a == 5);
  EXPECT_TRUE(5 == a);
  EXPECT_FALSE(a == 6);
  EXPECT_FALSE(6 == a);
}

TEST(MemvarComparison, GreaterThanAllOverloads) {
  memvar<int> a(10);
  memvar<int> b(5);
  EXPECT_TRUE(a > b);
  EXPECT_TRUE(a > 5);
  EXPECT_TRUE(10 > b);
  EXPECT_FALSE(b > a);
}

TEST(MemvarComparison, LessThanAllOverloads) {
  memvar<int> a(5);
  memvar<int> b(10);
  EXPECT_TRUE(a < b);
  EXPECT_TRUE(a < 10);
  EXPECT_TRUE(5 < b);
  EXPECT_FALSE(b < a);
}

TEST(MemvarComparison, GreaterOrEqualAllOverloads) {
  memvar<int> a(5);
  memvar<int> b(5);
  EXPECT_TRUE(a >= b);
  EXPECT_TRUE(a >= 5);
  EXPECT_TRUE(5 >= a);
  memvar<int> c(4);
  EXPECT_FALSE(c >= a);
}

TEST(MemvarComparison, LessOrEqualAllOverloads) {
  memvar<int> a(5);
  memvar<int> b(5);
  EXPECT_TRUE(a <= b);
  EXPECT_TRUE(a <= 5);
  EXPECT_TRUE(5 <= a);
  memvar<int> c(6);
  EXPECT_FALSE(c <= a);
}

// =============================================================================
// Section 10: Arithmetic free operators (+, -, *, /); the result is whatever
// the operation on the underlying values yields
// =============================================================================

TEST(MemvarArithmeticFreeOps, PlusAllOverloads) {
  memvar<int> a(3);
  memvar<int> b(4);
  EXPECT_EQ(a + b, 7);
  EXPECT_EQ(a + 4, 7);
  EXPECT_EQ(3 + b, 7);
  // memvar itself is unmodified by free operators
  EXPECT_EQ(a(), 3);
  EXPECT_EQ(b(), 4);
}

TEST(MemvarArithmeticFreeOps, MinusAllOverloads) {
  memvar<int> a(10);
  memvar<int> b(4);
  EXPECT_EQ(a - b, 6);
  EXPECT_EQ(a - 4, 6);
  EXPECT_EQ(10 - b, 6);
}

TEST(MemvarArithmeticFreeOps, TimesAllOverloads) {
  memvar<int> a(3);
  memvar<int> b(4);
  EXPECT_EQ(a * b, 12);
  EXPECT_EQ(a * 4, 12);
  EXPECT_EQ(3 * b, 12);
}

TEST(MemvarArithmeticFreeOps, DivideAllOverloads) {
  memvar<int> a(12);
  memvar<int> b(4);
  EXPECT_EQ(a / b, 3);
  EXPECT_EQ(a / 4, 3);
  EXPECT_EQ(12 / b, 3);
}

// =============================================================================
// Section 11: Streaming operator<<
// =============================================================================

TEST(MemvarStreaming, StreamOperatorPrintsCurrentValue) {
  memvar<int> m(123);
  std::ostringstream os;
  os << m;
  EXPECT_EQ(os.str(), "123");
}

TEST(MemvarStreaming, StreamOperatorAfterAssignmentReflectsLatest) {
  memvar<int> m(1);
  m = 2;
  std::ostringstream os;
  os << m;
  EXPECT_EQ(os.str(), "2");
}

// =============================================================================
// Section 12: Floating point specialization
// =============================================================================

TEST(MemvarFloatingPoint, BasicConstructionAndArithmetic) {
  memvar<double> m(1.5);
  m += 2.25;
  EXPECT_DOUBLE_EQ(m(), 3.75);
}

TEST(MemvarFloatingPoint, IncrementDecrementOnDouble) {
  memvar<double> m(1.0);
  ++m;
  EXPECT_DOUBLE_EQ(m(), 2.0);
  --m;
  EXPECT_DOUBLE_EQ(m(), 1.0);
}

TEST(MemvarFloatingPoint, MinMaxWithFractionalValues) {
  memvar<double> m(1.1);
  m = 5.5;
  m = -3.3;
  auto [minV, maxV] = m.getHistoryMinMax();
  EXPECT_DOUBLE_EQ(minV, -3.3);
  EXPECT_DOUBLE_EQ(maxV, 5.5);
}

// =============================================================================
// Section 13: std::string specialization (no arithmetic -,*,/ or ++/--)
// =============================================================================

TEST(MemvarString, ConstructionAndCurrentValue) {
  memvar<std::string> m(std::string("hello"));
  EXPECT_EQ(m(), "hello");
}

TEST(MemvarString, PlusEqualsConcatenatesValue) {
  memvar<std::string> m(std::string("foo"));
  m += std::string("bar");
  EXPECT_EQ(m(), "foobar");
}

TEST(MemvarString, PlusEqualsConcatenatesMemvar) {
  memvar<std::string> a(std::string("foo"));
  memvar<std::string> b(std::string("bar"));
  a += b;
  EXPECT_EQ(a(), "foobar");
}

TEST(MemvarString, FreePlusOperatorAllOverloads) {
  memvar<std::string> a(std::string("foo"));
  memvar<std::string> b(std::string("bar"));
  EXPECT_EQ(a + b, "foobar");
  EXPECT_EQ(a + std::string("baz"), "foobaz");
  EXPECT_EQ(std::string("qux") + b, "quxbar");
}

TEST(MemvarString, ComparisonOperators) {
  memvar<std::string> a(std::string("abc"));
  memvar<std::string> b(std::string("abd"));
  EXPECT_TRUE(a < b);
  EXPECT_TRUE(b > a);
  EXPECT_FALSE(a == b);
  EXPECT_TRUE(a == std::string("abc"));
}

TEST(MemvarString, HistoryAndPrinting) {
  memvar<std::string> m(std::string("a"));
  m = std::string("b");
  m = std::string("c");
  std::ostringstream os;
  m.printHistoryData(os);
  EXPECT_EQ(os.str(), "[ c b a ]\n");
}

TEST(MemvarString, MinMaxLexicographic) {
  memvar<std::string> m(std::string("banana"));
  m = std::string("apple");
  m = std::string("cherry");
  auto [minV, maxV] = m.getHistoryMinMax();
  EXPECT_EQ(minV, "apple");
  EXPECT_EQ(maxV, "cherry");
}

TEST(MemvarString, ArithmeticSubtractMultiplyDivideAreNotAvailable) {
  // Run-time mirror of the compile-time guarantee below: the operators that
  // make no sense for strings are not viable, while += (concatenation) is.
  using S = memvar<std::string>;
  EXPECT_TRUE((CanPlusEq<S, std::string>));
  EXPECT_FALSE((CanMinusEq<S, std::string>));
  EXPECT_FALSE((CanTimesEq<S, std::string>));
  EXPECT_FALSE((CanDivideEq<S, std::string>));
}

// The static_asserts further below (Section 23) double as compile-time
// regression tests: if memvar.h's requires-clauses regress and accidentally
// allow arithmetic on strings, this translation unit stops compiling.

// =============================================================================
// Section 14: Wide/unicode string overloads for operator<<
// =============================================================================

TEST(MemvarWideStringStreaming, WStringStreamsAsUtf8) {
  std::wstring ws = L"hello";
  std::ostringstream os;
  os << ws;
  EXPECT_EQ(os.str(), "hello");
}

TEST(MemvarWideStringStreaming, U8StringStreamsAsUtf8) {
  std::u8string us = u8"hello";
  std::ostringstream os;
  os << us;
  EXPECT_EQ(os.str(), "hello");
}

TEST(MemvarWideStringStreaming, U16StringStreamsAsUtf8) {
  std::u16string us = u"hello";
  std::ostringstream os;
  os << us;
  EXPECT_EQ(os.str(), "hello");
}

TEST(MemvarWideStringStreaming, U32StringStreamsAsUtf8) {
  std::u32string us = U"hello";
  std::ostringstream os;
  os << us;
  EXPECT_EQ(os.str(), "hello");
}

TEST(MemvarWideStringMemvar, MemvarOfWStringPrints) {
  memvar<std::wstring> m(std::wstring(L"abc"));
  m = std::wstring(L"def");
  std::ostringstream os;
  m.printHistoryData(os);
  EXPECT_EQ(os.str(), "[ def abc ]\n");
}

// =============================================================================
// Section 15: bip::bigint specialization (custom user type via is_bigint)
// =============================================================================

TEST(MemvarBigint, ConstructionAndValue) {
  memvar<bip::bigint> m(bip::bigint(100));
  EXPECT_EQ(m().value(), 100);
}

TEST(MemvarBigint, ArithmeticCompoundAssignment) {
  memvar<bip::bigint> m(bip::bigint(10));
  m += bip::bigint(5);
  EXPECT_EQ(m().value(), 15);
  m -= bip::bigint(3);
  EXPECT_EQ(m().value(), 12);
  m *= bip::bigint(2);
  EXPECT_EQ(m().value(), 24);
  m /= bip::bigint(4);
  EXPECT_EQ(m().value(), 6);
}

TEST(MemvarBigint, IncrementDecrement) {
  memvar<bip::bigint> m(bip::bigint(0));
  ++m;
  EXPECT_EQ(m().value(), 1);
  --m;
  EXPECT_EQ(m().value(), 0);
}

TEST(MemvarBigint, ComparisonOperators) {
  memvar<bip::bigint> a(bip::bigint(5));
  memvar<bip::bigint> b(bip::bigint(10));
  EXPECT_TRUE(a < b);
  EXPECT_TRUE(b > a);
  EXPECT_FALSE(a == b);
}

TEST(MemvarBigint, HistoryAndMinMax) {
  memvar<bip::bigint> m(bip::bigint(5));
  m = bip::bigint(1);
  m = bip::bigint(9);
  auto [minV, maxV] = m.getHistoryMinMax();
  EXPECT_EQ(minV.value(), 1);
  EXPECT_EQ(maxV.value(), 9);
}

// =============================================================================
// Section 16: mv::is_bigint / mv::MemVarType trait sanity (compile-time)
// =============================================================================

static_assert(mv::is_bigint_v<bip::bigint>);
static_assert(!mv::is_bigint_v<int>);
static_assert(mv::MemVarType<int>);
static_assert(mv::MemVarType<double>);
static_assert(mv::MemVarType<std::string>);
static_assert(mv::MemVarType<bip::bigint>);
static_assert(!mv::MemVarType<bool>);  // bool explicitly excluded

// =============================================================================
// Section 17: memvarTimed<T> tests
// =============================================================================

TEST(MemvarTimed, DefaultConstructionStartsAtZeroTimeTag) {
  memvarTimed<int> mt;
  EXPECT_EQ(mt(), 0);
  // the epoch is the time tag of the first value, so that tag is exactly 0
  EXPECT_EQ(mt.getTimeTag().count(), 0);
}

TEST(MemvarTimed, ValueConstructorSetsInitialValue) {
  memvarTimed<int> mt(42);
  EXPECT_EQ(mt(), 42);
}

TEST(MemvarTimed, IsNotCopyOrMoveConstructible) {
  EXPECT_FALSE(std::is_copy_constructible_v<memvarTimed<int>>);
  EXPECT_FALSE(std::is_move_constructible_v<memvarTimed<int>>);
  EXPECT_FALSE(std::is_move_assignable_v<memvarTimed<int>>);
}

TEST(MemvarTimed, IsNotFinal) {
  EXPECT_FALSE(std::is_final_v<memvarTimed<int>>);
}

TEST(MemvarTimed, TimeTagsAreMonotonicallyNonDecreasingAsHistoryAges) {
  memvarTimed<int> mt(1);
  std::this_thread::sleep_for(std::chrono::milliseconds(2));
  mt = 2;
  std::this_thread::sleep_for(std::chrono::milliseconds(2));
  mt = 3;

  // index 0 = newest = largest time tag; index size-1 = oldest = smallest.
  EXPECT_GE(mt.getTimeTag(0).count(), mt.getTimeTag(1).count());
  EXPECT_GE(mt.getTimeTag(1).count(), mt.getTimeTag(2).count());
}

TEST(MemvarTimed, GetHistoryTimedValueReturnsValueTimeAndErrorFlag) {
  memvarTimed<int> mt(5);
  mt = 6;
  auto [value, time, error] = mt.getHistoryTimedValue(0);
  EXPECT_EQ(value, 6);
  EXPECT_GE(time.count(), 0);
  EXPECT_FALSE(error);
}

TEST(MemvarTimed, FreeFunctionGetHistoryTimedValueMatchesCurrent) {
  memvarTimed<int> mt(5);
  mt = 6;
  EXPECT_EQ(mv::getHistoryTimedValue(mt, 0), 6);
}

TEST(MemvarTimed, CompoundAssignmentOperators) {
  memvarTimed<int> mt(10);
  mt += 5;
  EXPECT_EQ(mt(), 15);
  mt -= 3;
  EXPECT_EQ(mt(), 12);
  mt *= 2;
  EXPECT_EQ(mt(), 24);
  mt /= 4;
  EXPECT_EQ(mt(), 6);
}

TEST(MemvarTimed, CompoundAssignmentWithAnotherMemvarTimed) {
  memvarTimed<int> a(10);
  memvarTimed<int> b(5);
  a += b;
  EXPECT_EQ(a(), 15);
  a -= b;
  EXPECT_EQ(a(), 10);
  a *= b;
  EXPECT_EQ(a(), 50);
  a /= b;
  EXPECT_EQ(a(), 10);
}

TEST(MemvarTimed, AssignmentOperatorFromValueAndFromMemvarTimed) {
  memvarTimed<int> a(1);
  a = 2;
  EXPECT_EQ(a(), 2);

  memvarTimed<int> b(99);
  a = b;
  EXPECT_EQ(a(), 99);
}

TEST(MemvarTimed, HistoryCapacityEvictsOldestValueAndTimeTagTogether) {
  memvarTimed<int> mt(0, 3);  // capacity 3
  mt = 1;
  mt = 2;
  mt = 3;
  EXPECT_EQ(mt.getHistorySize(), 3);
  EXPECT_THROW(mt.getHistoryTimedValue(3), std::out_of_range);
  EXPECT_NO_THROW(mt.getHistoryTimedValue(2));
}

TEST(MemvarTimed, ClearHistoryResetsValueAndTimeTagsAndEpoch) {
  memvarTimed<int> mt(5);
  mt = 6;
  mt = 7;
  std::this_thread::sleep_for(std::chrono::milliseconds(2));
  EXPECT_GT(mt.getTimeTag(0).count(), 0);

  mt.clearHistory();

  EXPECT_EQ(mt.getHistorySize(), 1);
  EXPECT_EQ(mt(), 0);
  // the sole remaining entry defines the new epoch, so its tag is exactly 0
  EXPECT_EQ(mt.getTimeTag(0).count(), 0);
}

TEST(MemvarTimed, PrintHistoryTimedDataProducesNonEmptyBracketedOutput) {
  memvarTimed<int> mt(1);
  mt = 2;
  std::ostringstream os;
  mt.printHistoryTimedData(os);
  const std::string out = os.str();
  EXPECT_NE(out.find("--- begin ---"), std::string::npos);
  EXPECT_NE(out.find("--- end ---"), std::string::npos);
  EXPECT_NE(out.find("[TimeTag:Value]"), std::string::npos);
  EXPECT_NE(out.find(":2]"), std::string::npos);
  EXPECT_NE(out.find(":1]"), std::string::npos);
}

TEST(MemvarTimed, PrintReverseHistoryTimedDataProducesNonEmptyBracketedOutput) {
  memvarTimed<int> mt(1);
  mt = 2;
  std::ostringstream os;
  mt.printReverseHistoryTimedData(os);
  const std::string out = os.str();
  EXPECT_NE(out.find("--- begin ---"), std::string::npos);
  EXPECT_NE(out.find("--- end ---"), std::string::npos);
}

TEST(MemvarTimed, StreamOperatorPrintsBracketedTimeTagAndValue) {
  memvarTimed<int> mt(77);
  std::ostringstream os;
  os << mt;
  const std::string out = os.str();
  EXPECT_NE(out.find(":77]"), std::string::npos);
  EXPECT_EQ(out.front(), '[');
  EXPECT_EQ(out.back(), ']');
}

TEST(MemvarTimed, IndexedFunctionCallOperatorReturnsHistoricalValue) {
  memvarTimed<int> mt(1);
  mt = 2;
  mt = 3;
  EXPECT_EQ(mt(0), 3);
  EXPECT_EQ(mt(1), 2);
  EXPECT_EQ(mt(2), 1);
}

TEST(MemvarTimed, ConversionOperatorReturnsCurrentValue) {
  memvarTimed<int> mt(55);
  int v = mt;
  EXPECT_EQ(v, 55);
}

TEST(MemvarTimed, ValueConstructorBelowMinimumCapacityThrows) {
  EXPECT_THROW({ memvarTimed<int> mt(1, 1); }, std::invalid_argument);
}

TEST(MemvarTimed, DoubleSpecializationBasicFlow) {
  memvarTimed<double> mt(1.5);
  mt += 0.5;
  EXPECT_DOUBLE_EQ(mt(), 2.0);
}

TEST(MemvarTimed, StringSpecializationConcatenation) {
  memvarTimed<std::string> mt(std::string("foo"));
  mt += std::string("bar");
  EXPECT_EQ(mt(), "foobar");
}

// =============================================================================
// Section 18: Additional numeric-type coverage (char, unsigned, long long,
// float, long double) — the MemVarType concept accepts any non-bool
// integral type or any floating-point type.
// =============================================================================

TEST(MemvarAdditionalTypes, CharTypeConstructionAndIncrement) {
  memvar<char> m('a');
  EXPECT_EQ(m(), 'a');
  ++m;
  EXPECT_EQ(m(), 'b');
  --m;
  EXPECT_EQ(m(), 'a');
}

TEST(MemvarAdditionalTypes, UnsignedIntArithmeticAndWraparound) {
  memvar<unsigned int> m(10u);
  m += 5u;
  EXPECT_EQ(m(), 15u);
  // memvar performs no bounds/overflow checking of its own; subtracting past
  // zero on an unsigned type wraps around exactly like plain unsigned
  // arithmetic would.
  const unsigned int expected = 15u - 20u;
  m -= 20u;
  EXPECT_EQ(m(), expected);
}

TEST(MemvarAdditionalTypes, UnsignedDecrementBelowZeroWraps) {
  memvar<unsigned int> m(0u);
  --m;
  EXPECT_EQ(m(), std::numeric_limits<unsigned int>::max());
}

TEST(MemvarAdditionalTypes, LongLongConstructionAndArithmetic) {
  memvar<long long> m(1000000000000LL);
  m += 1;
  EXPECT_EQ(m(), 1000000000001LL);
}

TEST(MemvarAdditionalTypes, FloatConstructionAndArithmetic) {
  memvar<float> m(1.5f);
  m += 2.5f;
  EXPECT_FLOAT_EQ(m(), 4.0f);
}

TEST(MemvarAdditionalTypes, LongDoubleConstructionAndArithmetic) {
  memvar<long double> m(1.5L);
  m *= 2.0L;
  EXPECT_DOUBLE_EQ(static_cast<double>(m()), 3.0);
}

// =============================================================================
// Section 19: Default construction for non-int specializations
// =============================================================================

TEST(MemvarDefaultConstruction, StringDefaultIsEmpty) {
  memvar<std::string> m;
  EXPECT_EQ(m(), "");
  EXPECT_EQ(m.getHistorySize(), 1);
}

TEST(MemvarDefaultConstruction, DoubleDefaultIsZero) {
  memvar<double> m;
  EXPECT_DOUBLE_EQ(m(), 0.0);
}

TEST(MemvarDefaultConstruction, BigintDefaultIsZero) {
  memvar<bip::bigint> m;
  EXPECT_EQ(m().value(), 0);
}

// =============================================================================
// Section 20: memvarTimed with non-default Time / Clock template parameters
// =============================================================================

// memvarTimed converts Clock::now() to time_point<Clock, Time> with
// time_point_cast, so Time may be any duration, coarser or finer than the
// clock's native one (Section 28 checks the arithmetic exactly, with a fake
// clock).
TEST(MemvarTimedCustomParams, SystemClockWithItsNativeDuration) {
  memvarTimed<int, std::chrono::system_clock::duration, std::chrono::system_clock> mt(5);
  mt = 6;
  EXPECT_EQ(mt(), 6);
  EXPECT_GE(mt.getTimeTag(0).count(), 0);
}

TEST(MemvarTimedCustomParams, ValueConstructorWithCapacityAndCustomClock) {
  memvarTimed<int, std::chrono::high_resolution_clock::duration, std::chrono::high_resolution_clock> mt(1, 4);
  EXPECT_EQ(mt.getHistoryCapacity(), 4);
  EXPECT_EQ(mt(), 1);
}

// =============================================================================
// Section 21: memvarTimed increment/decrement operators (provided by the
// shared interface mixin, but they must still store the time tag, because
// they write through memvarTimed's own setValue()).
// =============================================================================

TEST(MemvarTimedIncrement, PrefixIncrementUpdatesValueAndRecordsTimeTag) {
  memvarTimed<int> mt(5);
  const int result = ++mt;
  EXPECT_EQ(result, 6);
  EXPECT_EQ(mt(), 6);
  EXPECT_EQ(mt.getHistorySize(), 2);
  EXPECT_NO_THROW(mt.getHistoryTimedValue(0));
  EXPECT_NO_THROW(mt.getHistoryTimedValue(1));
}

TEST(MemvarTimedIncrement, PostfixDecrementUpdatesValueAndRecordsTimeTag) {
  memvarTimed<int> mt(5);
  const int result = mt--;
  EXPECT_EQ(result, 5);
  EXPECT_EQ(mt(), 4);
  EXPECT_EQ(mt.getHistorySize(), 2);
}

TEST(MemvarTimed, BracketOperator) {
  memvarTimed<int> mt(1);
  mt = 2;
  mt = 3;
  EXPECT_EQ(mt[0], 3);
  EXPECT_EQ(mt[1], 2);
  EXPECT_EQ(mt[2], 1);
}

TEST(MemvarTimed, GetHistoryCapacityDefaultAndCustom) {
  memvarTimed<int> mtDefault;
  EXPECT_EQ(mtDefault.getHistoryCapacity(), 10);
  memvarTimed<int> mtCustom(1, 5);
  EXPECT_EQ(mtCustom.getHistoryCapacity(), 5);
}

TEST(MemvarTimed, StringSpecializationHistoryTimedValue) {
  memvarTimed<std::string> mt(std::string("a"));
  mt = std::string("b");
  auto [value, time, error] = mt.getHistoryTimedValue(0);
  EXPECT_EQ(value, "b");
  EXPECT_GE(time.count(), 0);
  EXPECT_FALSE(error);
}

// =============================================================================
// Section 22: Direct calls to the public getHistoryValue() accessor —
// out-of-range behavior and the shape of the returned (value, errorFlag)
// tuple, independent of operator() / operator[].
// =============================================================================

TEST(MemvarHistoryValueDirect, NegativeIndexThrowsWithDescriptiveMessage) {
  memvar<int> m(1);
  try {
    m.getHistoryValue(-1);
    FAIL() << "expected std::out_of_range";
  } catch (const std::out_of_range& e) {
    const std::string what {e.what()};
    EXPECT_NE(what.find("out of bounds"), std::string::npos);
    EXPECT_NE(what.find("-1"), std::string::npos);
  }
}

TEST(MemvarHistoryValueDirect, OutOfRangeMessageMentionsActualSize) {
  memvar<int> m(1);
  m = 2;
  m = 3;  // history size is now 3
  try {
    m.getHistoryValue(100);
    FAIL() << "expected std::out_of_range";
  } catch (const std::out_of_range& e) {
    const std::string what {e.what()};
    EXPECT_NE(what.find("100"), std::string::npos);
    EXPECT_NE(what.find("3"), std::string::npos);
  }
}

TEST(MemvarHistoryValueDirect, TupleStructureHasValueAndFalseErrorFlag) {
  memvar<int> m(1);
  m = 2;
  const auto tup = m.getHistoryValue(0);
  EXPECT_EQ(std::get<0>(tup), 2);
  EXPECT_FALSE(std::get<1>(tup));
}

// =============================================================================
// Section 23: Further compile-time exclusion checks for std::string
// The `requires (!AnyStandardString<T>)` clause guards +=, -=, *=, /=, ++/--.
// =============================================================================

static_assert(CanPlusEq<memvar<int>, int>, "int should support operator+=");
static_assert(CanMinusEq<memvar<int>, int>, "int should support operator-=");
static_assert(!CanMinusEq<memvar<std::string>, std::string>, "std::string must NOT support operator-=");

static_assert(CanTimesEq<memvar<int>, int>, "int should support operator*=");
static_assert(!CanTimesEq<memvar<std::string>, std::string>, "std::string must NOT support operator*=");

static_assert(CanDivideEq<memvar<int>, int>, "int should support operator/=");
static_assert(!CanDivideEq<memvar<std::string>, std::string>, "std::string must NOT support operator/=");

static_assert(CanPreInc<memvar<int>>, "int should support prefix operator++");
static_assert(!CanPreInc<memvar<std::string>>, "std::string must NOT support prefix operator++");
static_assert(CanPreDec<memvar<int>>, "int should support prefix operator--");
static_assert(!CanPreDec<memvar<std::string>>, "std::string must NOT support prefix operator--");
static_assert(CanPostInc<memvar<int>>, "int should support postfix operator++");
static_assert(!CanPostInc<memvar<std::string>>, "std::string must NOT support postfix operator++");
static_assert(CanPostDec<memvar<int>>, "int should support postfix operator--");
static_assert(!CanPostDec<memvar<std::string>>, "std::string must NOT support postfix operator--");

// The single-argument value constructor is `explicit`, so implicit
// conversion from a bare T to memvar<T> must not compile.
static_assert(!std::is_convertible_v<int, memvar<int>>,
              "memvar's value constructor is explicit and must not allow implicit conversion");

// =============================================================================
// Section 24: Self-assignment and chained assignment
// =============================================================================

TEST(MemvarAssignment, SelfAssignmentPushesCurrentValueAgain) {
  // operator=(const memvar&) is a "value push", not a full history copy or a
  // no-op guard, so even m = m grows the history by re-pushing the current
  // value.
  memvar<int> m(5);
  selfAssign(m);
  EXPECT_EQ(m(), 5);
  EXPECT_EQ(m.getHistorySize(), 2);
}

TEST(MemvarAssignment, ChainedAssignmentFromValuePropagatesThroughMemvars) {
  memvar<int> a(1);
  memvar<int> b(2);
  memvar<int> c(3);
  a = (b = (c = 42));
  EXPECT_EQ(c(), 42);
  EXPECT_EQ(b(), 42);
  EXPECT_EQ(a(), 42);
}

// =============================================================================
// Section 25: isHistoryFull() boundary transition
// =============================================================================

TEST(MemvarAssignment, IsHistoryFullBecomesTrueExactlyAtCapacityAndStaysTrue) {
  memvar<int> m(0, 3);  // capacity 3, size 1
  EXPECT_FALSE(m.isHistoryFull());
  m = 1;  // size 2
  EXPECT_FALSE(m.isHistoryFull());
  m = 2;  // size 3 == capacity
  EXPECT_TRUE(m.isHistoryFull());
  m = 3;  // eviction keeps size == capacity
  EXPECT_TRUE(m.isHistoryFull());
}

// =============================================================================
// Section 26: Construction edge cases (capacity validation, public types)
// =============================================================================

TEST(MemvarConstructionEdges, HugeCapacityIsAcceptedAndNothingIsPreallocated) {
  memvar<int> m(1, std::numeric_limits<std::int64_t>::max());
  EXPECT_EQ(m.getHistoryCapacity(), std::numeric_limits<std::int64_t>::max());
  EXPECT_EQ(m.getHistorySize(), 1);
  EXPECT_FALSE(m.isHistoryFull());
}

TEST(MemvarConstructionEdges, MostNegativeCapacityThrows) {
  EXPECT_THROW(memvar<int>(1, std::numeric_limits<std::int64_t>::min()), std::invalid_argument);
  EXPECT_THROW(memvarTimed<int>(1, std::numeric_limits<std::int64_t>::min()), std::invalid_argument);
}

TEST(MemvarConstructionEdges, InvalidCapacityMessageIsExact) {
  try {
    memvar<int> m(1, 0);
    FAIL() << "expected std::invalid_argument";
  } catch (const std::invalid_argument& e) {
    EXPECT_STREQ(e.what(), "ERROR: The history capacity must be 2 at least, or more");
  }
}

TEST(MemvarConstructionEdges, InvalidCapacityThrowsForTimedWithSameMessage) {
  for (const std::int64_t bad : {1, 0, -1}) {
    try {
      memvarTimed<int> mt(1, bad);
      FAIL() << "expected std::invalid_argument for capacity " << bad;
    } catch (const std::invalid_argument& e) {
      EXPECT_STREQ(e.what(), "ERROR: The history capacity must be 2 at least, or more");
    }
  }
}

TEST(MemvarConstructionEdges, InvalidCapacityThrowsForEveryValueType) {
  EXPECT_THROW(memvar<double>(1.0, 1), std::invalid_argument);
  EXPECT_THROW(memvar<std::string>(std::string("x"), 1), std::invalid_argument);
  EXPECT_THROW(memvar<bip::bigint>(bip::bigint(1), 1), std::invalid_argument);
}

TEST(MemvarConstructionEdges, PublicTypeAliases) {
  static_assert(std::is_same_v<memvar<int>::capacityType, std::int64_t>);
  static_assert(std::is_signed_v<memvar<int>::capacityType>);
  static_assert(std::is_same_v<memvar<int>::historyValue, std::tuple<int, bool>>);
  static_assert(std::is_same_v<memvar<std::string>::historyValue, std::tuple<std::string, bool>>);
  static_assert(std::is_same_v<memvarTimed<int>::capacityType, std::int64_t>);
  static_assert(std::is_same_v<memvarTimed<int>::historyValue, std::tuple<int, bool>>);
  static_assert(std::is_same_v<memvarTimed<int>::historyTimedValue,
                               std::tuple<int, std::chrono::nanoseconds, bool>>);
  static_assert(std::is_same_v<memvarTimed<int>::time_point,
                               std::chrono::time_point<std::chrono::steady_clock, std::chrono::nanoseconds>>);
  static_assert(std::is_same_v<FakeTimed::historyTimedValue,
                               std::tuple<int, std::chrono::milliseconds, bool>>);
  SUCCEED();
}

TEST(MemvarConstructionEdges, ClassTemplateArgumentDeduction) {
  memvar m(5);
  static_assert(std::is_same_v<decltype(m), memvar<int>>);
  EXPECT_EQ(m(), 5);
  memvarTimed mt(2.5, 4);
  static_assert(std::is_same_v<decltype(mt), memvarTimed<double>>);
  EXPECT_DOUBLE_EQ(mt(), 2.5);
  EXPECT_EQ(mt.getHistoryCapacity(), 4);
}

TEST(MemvarConstructionEdges, ConstructibleFromValueAndOptionalCapacityOnly) {
  static_assert(std::is_constructible_v<memvar<int>, int>);
  static_assert(std::is_constructible_v<memvar<int>, int, std::int64_t>);
  static_assert(!std::is_constructible_v<memvar<int>, const char*>);
  static_assert(!std::is_constructible_v<memvar<int>, int, int, int>);
  static_assert(std::is_constructible_v<memvarTimed<int>, int, std::int64_t>);
  static_assert(!std::is_convertible_v<int, memvarTimed<int>>);
  SUCCEED();
}

TEST(MemvarConstructionEdges, MemvarAndMemvarTimedAreUnrelatedNonPolymorphicTypes) {
  static_assert(!std::is_polymorphic_v<memvar<int>>);
  static_assert(!std::is_polymorphic_v<memvarTimed<int>>);
  static_assert(!std::is_abstract_v<memvar<int>>);
  static_assert(!std::is_base_of_v<memvar<int>, memvarTimed<int>>);
  static_assert(!std::is_base_of_v<memvarTimed<int>, memvar<int>>);
  static_assert(!std::is_convertible_v<memvarTimed<int>*, memvar<int>*>);
  static_assert(!std::is_convertible_v<memvarTimed<int>&, memvar<int>&>);
  SUCCEED();
}

TEST(MemvarConstructionEdges, MemvarIsConvertibleToItsValueType) {
  static_assert(std::is_convertible_v<memvar<int>, int>);
  static_assert(std::is_convertible_v<memvar<std::string>, std::string>);
  static_assert(std::is_convertible_v<memvarTimed<double>, double>);
  SUCCEED();
}

TEST(MemvarConstructionEdges, TimedIsCopyAssignableButNotMoveOrCopyConstructible) {
  static_assert(std::is_copy_assignable_v<memvarTimed<int>>);
  static_assert(!std::is_move_assignable_v<memvarTimed<int>>);
  static_assert(!std::is_copy_constructible_v<memvarTimed<int>>);
  static_assert(!std::is_move_constructible_v<memvarTimed<int>>);
  SUCCEED();
}

TEST(MemvarConstructionEdges, InitialValueOfStringMayBeEmptyOrLong) {
  memvar<std::string> empty(std::string{});
  EXPECT_EQ(empty(), "");
  const std::string longText(1000, 'x');
  memvar<std::string> big(longText);
  EXPECT_EQ(big(), longText);
  EXPECT_EQ(big().size(), 1000u);
}

// =============================================================================
// Section 27: Accessors on const objects, valueAt(), out-of-range behaviour of
// every accessor, live history reference
// =============================================================================

TEST(MemvarConstAccess, EveryReadAccessorWorksOnAConstObject) {
  memvar<int> m(1, 5);
  m = 2;
  m = 3;
  const memvar<int>& cm = m;
  EXPECT_EQ(cm(), 3);
  EXPECT_EQ(cm(1), 2);
  EXPECT_EQ(cm[2], 1);
  EXPECT_EQ(cm.valueAt(0), 3);
  EXPECT_EQ(static_cast<int>(cm), 3);
  EXPECT_EQ(std::get<0>(cm.getHistoryValue(1)), 2);
  EXPECT_EQ(cm.getHistorySize(), 3);
  EXPECT_EQ(cm.getHistoryCapacity(), 5);
  EXPECT_FALSE(cm.isHistoryFull());
  EXPECT_EQ(cm.getMemVarHistory().size(), 3u);
  const int viaCref = cm.cref();
  EXPECT_EQ(viaCref, 3);
  EXPECT_TRUE(cm == 3);
  EXPECT_EQ(cm + 1, 4);
  const auto [lo, hi] = cm.getHistoryMinMax();
  EXPECT_EQ(lo, 1);
  EXPECT_EQ(hi, 3);
  std::ostringstream os;
  cm.printHistoryData(os);
  cm.printReverseHistoryData(os);
  os << cm;
  EXPECT_EQ(os.str(), "[ 3 2 1 ]\n[ 1 2 3 ]\n3");
}

TEST(MemvarConstAccess, EveryReadAccessorWorksOnAConstTimedObject) {
  FakeClock::ticks = 1000;
  FakeTimed mt(1, 5);
  FakeClock::advance(10);
  mt = 2;
  const FakeTimed& cmt = mt;
  EXPECT_EQ(cmt(), 2);
  EXPECT_EQ(cmt(1), 1);
  EXPECT_EQ(cmt[0], 2);
  EXPECT_EQ(cmt.valueAt(1), 1);
  EXPECT_EQ(cmt.getTimeTag(0).count(), 10);
  EXPECT_EQ(std::get<1>(cmt.getHistoryTimedValue(0)).count(), 10);
  EXPECT_EQ(std::get<0>(cmt.getHistoryValue(0)), 2);
  EXPECT_EQ(cmt.getHistorySize(), 2);
  const int viaCref = cmt.cref();
  EXPECT_EQ(viaCref, 2);
  const auto [lo, hi] = cmt.getHistoryMinMax();
  EXPECT_EQ(lo, 1);
  EXPECT_EQ(hi, 2);
  EXPECT_EQ(cmt.getMemVarHistory().size(), 2u);
}

TEST(MemvarValueAt, ReturnsSameValuesAsCallAndBracketOperators) {
  memvar<int> m(10, 4);
  m = 20;
  m = 30;
  for (memvar<int>::capacityType i = 0; i < m.getHistorySize(); ++i) {
    EXPECT_EQ(m.valueAt(i), m(i));
    EXPECT_EQ(m.valueAt(i), m[i]);
  }
  EXPECT_EQ(m.valueAt(0), 30);
  EXPECT_EQ(m.valueAt(2), 10);
}

TEST(MemvarValueAt, OutOfRangeThrows) {
  memvar<int> m(1);
  EXPECT_THROW(m.valueAt(-1), std::out_of_range);
  EXPECT_THROW(m.valueAt(1), std::out_of_range);
}

TEST(MemvarOutOfRange, EveryIndexedAccessorThrowsOnTheSameBadIndexes) {
  memvar<int> m(1);
  m = 2;  // size 2
  const std::int64_t bad[] = {-1, 2, 3, 1000000,
                              std::numeric_limits<std::int64_t>::max(),
                              std::numeric_limits<std::int64_t>::min()};
  for (const std::int64_t i : bad) {
    EXPECT_THROW(m(i), std::out_of_range) << i;
    EXPECT_THROW(m[i], std::out_of_range) << i;
    EXPECT_THROW(m.valueAt(i), std::out_of_range) << i;
    EXPECT_THROW(m.getHistoryValue(i), std::out_of_range) << i;
    EXPECT_THROW(mv::getHistoryValue(m, i), std::out_of_range) << i;
  }
}

TEST(MemvarOutOfRange, EveryIndexedAccessorOfTimedThrowsOnBadIndexes) {
  memvarTimed<int> mt(1);
  mt = 2;  // size 2
  const std::int64_t bad[] = {-1, 2, 3, std::numeric_limits<std::int64_t>::max(),
                              std::numeric_limits<std::int64_t>::min()};
  for (const std::int64_t i : bad) {
    EXPECT_THROW(mt(i), std::out_of_range) << i;
    EXPECT_THROW(mt[i], std::out_of_range) << i;
    EXPECT_THROW(mt.valueAt(i), std::out_of_range) << i;
    EXPECT_THROW(mt.getHistoryValue(i), std::out_of_range) << i;
    EXPECT_THROW(mt.getTimeTag(i), std::out_of_range) << i;
    EXPECT_THROW(mt.getHistoryTimedValue(i), std::out_of_range) << i;
    EXPECT_THROW(mv::getHistoryValue(mt, i), std::out_of_range) << i;
    EXPECT_THROW(mv::getHistoryTimedValue(mt, i), std::out_of_range) << i;
  }
}

TEST(MemvarOutOfRange, MessageIsExact) {
  memvar<int> m(1);
  m = 2;
  m = 3;
  try {
    m(7);
    FAIL() << "expected std::out_of_range";
  } catch (const std::out_of_range& e) {
    EXPECT_STREQ(e.what(), "Index 7 is out of bounds (size is 3).");
  }
  try {
    m(-4);
    FAIL() << "expected std::out_of_range";
  } catch (const std::out_of_range& e) {
    EXPECT_STREQ(e.what(), "Index -4 is out of bounds (size is 3).");
  }
}

TEST(MemvarOutOfRange, TimedMessageReportsTimedSize) {
  memvarTimed<int> mt(1);
  mt = 2;
  try {
    mt.getTimeTag(2);
    FAIL() << "expected std::out_of_range";
  } catch (const std::out_of_range& e) {
    EXPECT_STREQ(e.what(), "Index 2 is out of bounds (size is 2).");
  }
}

TEST(MemvarOutOfRange, FirstAndLastValidIndexesAreAccepted) {
  memvar<int> m(0, 3);
  m = 1;
  m = 2;
  EXPECT_EQ(m(0), 2);
  EXPECT_EQ(m(m.getHistorySize() - 1), 0);
}

TEST(MemvarOutOfRange, EvictedIndexBecomesInvalidAfterWrapAround) {
  memvar<int> m(0, 2);
  m = 1;
  EXPECT_EQ(m(1), 0);
  m = 2;  // 0 evicted
  EXPECT_EQ(m(1), 1);
  EXPECT_THROW(m(2), std::out_of_range);
}

TEST(MemvarHistoryContainer, GetMemVarHistoryIsALiveViewOfTheSameDeque) {
  memvar<int> m(1, 3);
  const std::deque<int>& h1 = m.getMemVarHistory();
  EXPECT_EQ(h1.size(), 1u);
  m = 2;
  m = 3;
  m = 4;  // 1 evicted
  const std::deque<int>& h2 = m.getMemVarHistory();
  EXPECT_EQ(&h1, &h2);
  ASSERT_EQ(h1.size(), 3u);
  EXPECT_EQ(h1.front(), 4);
  EXPECT_EQ(h1.back(), 2);
}

TEST(MemvarHistoryContainer, GetMemVarHistoryReturnsDequeOfT) {
  static_assert(std::is_same_v<decltype(std::declval<const memvar<int>&>().getMemVarHistory()),
                               const std::deque<int>&>);
  static_assert(std::is_same_v<decltype(std::declval<const memvar<std::string>&>().getMemVarHistory()),
                               const std::deque<std::string>&>);
  static_assert(std::is_same_v<decltype(std::declval<const memvarTimed<int>&>().getMemVarHistory()),
                               const std::deque<memvarTimed<int>::Entry>&>);
  SUCCEED();
}

TEST(MemvarHistoryContainer, ValueOfReferencesTheStoredFrontValueWithoutCopy) {
  memvar<std::string> m(std::string("front"));
  EXPECT_EQ(&mv::valueOf(m), &m.getMemVarHistory().front());
  memvarTimed<std::string> mt(std::string("front"));
  EXPECT_EQ(&mv::valueOf(mt), &mt.getMemVarHistory().front().value);
  const int plain = 5;
  EXPECT_EQ(&mv::valueOf(plain), &plain);
}

// =============================================================================
// Section 28: memvarTimed with a deterministic clock
// =============================================================================

using MemvarTimedFake = FakeClockTest;

TEST_F(MemvarTimedFake, FirstValueDefinesTheEpoch) {
  FakeTimed mt(7);
  EXPECT_EQ(mt.getTimeTag().count(), 0);
  EXPECT_EQ(mt.getTimeTag(0).count(), 0);
  const auto& e = mt.getMemVarHistory().front();
  EXPECT_EQ(e.value, 7);
  EXPECT_EQ(e.tag.time_since_epoch().count(), 1000);
}

TEST_F(MemvarTimedFake, TagsAreRelativeToTheEpochAndAscendTowardsTheNewestEntry) {
  FakeTimed mt(1, 10);
  FakeClock::advance(5);
  mt = 2;
  FakeClock::advance(20);
  mt = 3;
  EXPECT_EQ(mt.getTimeTag(2).count(), 0);
  EXPECT_EQ(mt.getTimeTag(1).count(), 5);
  EXPECT_EQ(mt.getTimeTag(0).count(), 25);
  EXPECT_EQ(mt.getTimeTag().count(), 25);
}

TEST_F(MemvarTimedFake, WritesAtTheSameInstantShareATag) {
  FakeTimed mt(1);
  mt = 2;
  mt = 3;
  EXPECT_EQ(mt.getTimeTag(0).count(), 0);
  EXPECT_EQ(mt.getTimeTag(1).count(), 0);
  EXPECT_EQ(mt.getTimeTag(2).count(), 0);
  EXPECT_EQ(mt.getHistorySize(), 3);
}

TEST_F(MemvarTimedFake, GetHistoryTimedValueReturnsValueExactTagAndFalseFlag) {
  FakeTimed mt(10);
  FakeClock::advance(7);
  mt = 20;
  const auto [v0, t0, e0] = mt.getHistoryTimedValue(0);
  const auto [v1, t1, e1] = mt.getHistoryTimedValue(1);
  EXPECT_EQ(v0, 20);
  EXPECT_EQ(t0, std::chrono::milliseconds(7));
  EXPECT_FALSE(e0);
  EXPECT_EQ(v1, 10);
  EXPECT_EQ(t1, std::chrono::milliseconds(0));
  EXPECT_FALSE(e1);
}

TEST_F(MemvarTimedFake, EntryHoldsValueAndAbsoluteTimePoint) {
  FakeTimed mt(1, 3);
  FakeClock::advance(4);
  mt = 2;
  const auto& h = mt.getMemVarHistory();
  ASSERT_EQ(h.size(), 2u);
  EXPECT_EQ(h[0].value, 2);
  EXPECT_EQ(h[0].tag.time_since_epoch().count(), 1004);
  EXPECT_EQ(h[1].value, 1);
  EXPECT_EQ(h[1].tag.time_since_epoch().count(), 1000);
  static_assert(std::is_same_v<decltype(h[0].tag), FakeTimed::time_point>);
}

TEST_F(MemvarTimedFake, EvictionRemovesValueAndTagTogetherAndKeepsTheOriginalEpoch) {
  FakeTimed mt(0, 2);
  FakeClock::advance(10);
  mt = 1;
  FakeClock::advance(10);
  mt = 2;  // first value (and epoch entry) evicted
  ASSERT_EQ(mt.getHistorySize(), 2);
  EXPECT_EQ(mt(0), 2);
  EXPECT_EQ(mt(1), 1);
  // the epoch does not move on eviction: tags stay relative to construction
  EXPECT_EQ(mt.getTimeTag(0).count(), 20);
  EXPECT_EQ(mt.getTimeTag(1).count(), 10);
  EXPECT_THROW(mt.getTimeTag(2), std::out_of_range);
}

TEST_F(MemvarTimedFake, ClearHistoryStartsANewEpoch) {
  FakeTimed mt(5, 4);
  FakeClock::advance(100);
  mt = 6;
  FakeClock::advance(50);
  mt.clearHistory();
  EXPECT_EQ(mt.getHistorySize(), 1);
  EXPECT_EQ(mt.getHistoryCapacity(), 4);
  EXPECT_EQ(mt(), 0);
  EXPECT_EQ(mt.getTimeTag().count(), 0);
  FakeClock::advance(3);
  mt = 9;
  EXPECT_EQ(mt.getTimeTag(0).count(), 3);
  EXPECT_EQ(mt.getTimeTag(1).count(), 0);
}

TEST_F(MemvarTimedFake, ClearHistoryTwiceInARowIsHarmless) {
  FakeTimed mt(5);
  mt.clearHistory();
  mt.clearHistory();
  EXPECT_EQ(mt.getHistorySize(), 1);
  EXPECT_EQ(mt(), 0);
  EXPECT_EQ(mt.getTimeTag().count(), 0);
}

TEST_F(MemvarTimedFake, IncrementAndDecrementRecordTags) {
  FakeTimed mt(0, 10);
  FakeClock::advance(2);
  ++mt;
  FakeClock::advance(2);
  mt++;
  FakeClock::advance(2);
  --mt;
  FakeClock::advance(2);
  mt--;
  EXPECT_EQ(mt.getHistorySize(), 5);
  EXPECT_EQ(mt(), 0);
  EXPECT_EQ(mt.getTimeTag(0).count(), 8);
  EXPECT_EQ(mt.getTimeTag(1).count(), 6);
  EXPECT_EQ(mt.getTimeTag(2).count(), 4);
  EXPECT_EQ(mt.getTimeTag(3).count(), 2);
  EXPECT_EQ(mt.getTimeTag(4).count(), 0);
}

TEST_F(MemvarTimedFake, CompoundAssignmentAndCopyAssignmentRecordTags) {
  FakeTimed a(1, 10);
  FakeTimed b(100, 10);
  FakeClock::advance(4);
  a += 1;
  FakeClock::advance(4);
  a = b;
  FakeClock::advance(4);
  a *= b;
  EXPECT_EQ(a(), 10000);
  EXPECT_EQ(a.getTimeTag(0).count(), 12);
  EXPECT_EQ(a.getTimeTag(1).count(), 8);
  EXPECT_EQ(a.getTimeTag(2).count(), 4);
  EXPECT_EQ(a.getTimeTag(3).count(), 0);
  EXPECT_EQ(b.getHistorySize(), 1);  // right-hand side untouched
}

TEST_F(MemvarTimedFake, StreamOperatorPrintsNewestTagAndValue) {
  FakeTimed mt(77);
  std::ostringstream os;
  os << mt;
  EXPECT_EQ(os.str(), "[0:77]");
  FakeClock::advance(12);
  mt = 78;
  std::ostringstream os2;
  os2 << mt;
  EXPECT_EQ(os2.str(), "[12:78]");
}

TEST_F(MemvarTimedFake, PrintHistoryTimedDataExactFormatNewestFirst) {
  FakeTimed mt(1);
  FakeClock::advance(5);
  mt = 2;
  std::ostringstream os;
  mt.printHistoryTimedData(os);
  EXPECT_EQ(os.str(),
            "{ --- begin ---\n"
            "[TimeTag:Value]\n"
            "[5:2]\n"
            "[0:1]\n"
            "  --- end --- }\n\n");
}

TEST_F(MemvarTimedFake, PrintReverseHistoryTimedDataExactFormatOldestFirst) {
  FakeTimed mt(1);
  FakeClock::advance(5);
  mt = 2;
  std::ostringstream os;
  mt.printReverseHistoryTimedData(os);
  EXPECT_EQ(os.str(),
            "{ --- begin ---\n"
            "[TimeTag:Value]\n"
            "[0:1]\n"
            "[5:2]\n"
            "  --- end --- }\n\n");
}

TEST_F(MemvarTimedFake, PrintTimedCustomSeparator) {
  FakeTimed mt(1);
  FakeClock::advance(5);
  mt = 2;
  std::ostringstream os;
  mt.printHistoryTimedData(os, " | ");
  EXPECT_EQ(os.str(),
            "{ --- begin ---\n"
            "[TimeTag:Value]\n"
            "[5:2] | [0:1] |   --- end --- }\n\n");
  std::ostringstream rev;
  mt.printReverseHistoryTimedData(rev, ";");
  EXPECT_EQ(rev.str(),
            "{ --- begin ---\n"
            "[TimeTag:Value]\n"
            "[0:1];[5:2];  --- end --- }\n\n");
}

TEST_F(MemvarTimedFake, PrintTimedSingleEntryReverseEqualsForward) {
  FakeTimed mt(3);
  std::ostringstream fwd, rev;
  mt.printHistoryTimedData(fwd);
  mt.printReverseHistoryTimedData(rev);
  EXPECT_EQ(fwd.str(), rev.str());
}

TEST_F(MemvarTimedFake, PrintTimedDefaultsGoToStdCoutWithNewlineSeparator) {
  FakeTimed mt(1);
  CoutCapture cap;
  mt.printHistoryTimedData();
  mt.printReverseHistoryTimedData();
  EXPECT_EQ(cap.str(),
            "{ --- begin ---\n[TimeTag:Value]\n[0:1]\n  --- end --- }\n\n"
            "{ --- begin ---\n[TimeTag:Value]\n[0:1]\n  --- end --- }\n\n");
}

TEST_F(MemvarTimedFake, PlainPrintOfValuesIgnoresTags) {
  FakeTimed mt(1);
  FakeClock::advance(9);
  mt = 2;
  std::ostringstream fwd, rev;
  mt.printHistoryData(fwd);
  mt.printReverseHistoryData(rev, ",");
  EXPECT_EQ(fwd.str(), "[ 2 1 ]\n");
  EXPECT_EQ(rev.str(), "[ 1,2,]\n");
}

TEST_F(MemvarTimedFake, TimeUnitCoarserThanClockTruncatesTowardsZero) {
  FakeClock::ticks = 1999;  // 1.999 s
  memvarTimed<int, std::chrono::seconds, FakeClock> mt(1);
  // epoch is truncated to 1 s
  FakeClock::advance(2);  // 2.001 s -> 2 s
  mt = 2;
  EXPECT_EQ(mt.getTimeTag(0), std::chrono::seconds(1));
  EXPECT_EQ(mt.getTimeTag(1), std::chrono::seconds(0));
  FakeClock::advance(998);  // 2.999 s -> 2 s
  mt = 3;
  EXPECT_EQ(mt.getTimeTag(0), std::chrono::seconds(1));
}

TEST_F(MemvarTimedFake, TimeUnitFinerThanClockScalesExactly) {
  memvarTimed<int, std::chrono::microseconds, FakeClock> mt(1);
  FakeClock::advance(3);
  mt = 2;
  EXPECT_EQ(mt.getTimeTag(0), std::chrono::microseconds(3000));
  const auto [v, t, err] = mt.getHistoryTimedValue(0);
  EXPECT_EQ(v, 2);
  EXPECT_EQ(t.count(), 3000);
  EXPECT_FALSE(err);
}

TEST_F(MemvarTimedFake, DefaultConstructorUsesFakeClockToo) {
  FakeClock::advance(40);
  FakeTimed mt;
  EXPECT_EQ(mt(), 0);
  EXPECT_EQ(mt.getHistoryCapacity(), 10);
  EXPECT_EQ(mt.getMemVarHistory().front().tag.time_since_epoch().count(), 1040);
}

TEST_F(MemvarTimedFake, ClockGoingBackwardsGivesNegativeTags) {
  FakeTimed mt(1);
  FakeClock::advance(-5);
  mt = 2;
  EXPECT_EQ(mt.getTimeTag(0).count(), -5);
}

TEST_F(MemvarTimedFake, MinMaxIgnoresTagsAndUsesValues) {
  FakeTimed mt(5, 10);
  FakeClock::advance(1);
  mt = -3;
  FakeClock::advance(1);
  mt = 9;
  FakeClock::advance(1);
  mt = 2;
  const auto [lo, hi] = mt.getHistoryMinMax();
  EXPECT_EQ(lo, -3);
  EXPECT_EQ(hi, 9);
  static_assert(std::is_same_v<std::remove_cv_t<decltype(lo)>, int>);
}

TEST_F(MemvarTimedFake, FreeFunctionsMatchMembers) {
  FakeTimed mt(1);
  FakeClock::advance(2);
  mt = 2;
  EXPECT_EQ(mv::getHistoryValue(mt, 0), 2);
  EXPECT_EQ(mv::getHistoryValue(mt, 1), 1);
  EXPECT_EQ(mv::getHistoryTimedValue(mt, 1), 1);
  EXPECT_EQ(mv::getHistoryTimedValue(mt, 0), 2);
}

TEST_F(MemvarTimedFake, TimedWorksForStringsAndWideStrings) {
  memvarTimed<std::string, std::chrono::milliseconds, FakeClock> ms(std::string("a"));
  FakeClock::advance(1);
  ms = std::string("b");
  ms += "c";
  EXPECT_EQ(ms(), "bc");
  EXPECT_EQ(ms.getTimeTag(0).count(), 1);
  std::ostringstream os;
  ms.printHistoryTimedData(os, "/");
  EXPECT_EQ(os.str(),
            "{ --- begin ---\n[TimeTag:Value]\n[1:bc]/[1:b]/[0:a]/  --- end --- }\n\n");

  memvarTimed<std::wstring, std::chrono::milliseconds, FakeClock> mw(std::wstring(L"é"));
  std::ostringstream ow;
  mw.printHistoryTimedData(ow, "");
  EXPECT_EQ(ow.str(),
            "{ --- begin ---\n[TimeTag:Value]\n[0:\xC3\xA9]  --- end --- }\n\n");
  std::ostringstream o2;
  o2 << mw;
  EXPECT_EQ(o2.str(), "[0:\xC3\xA9]");
}

TEST(MemvarTimedRealClock, CustomTimeUnitWithDefaultSteadyClock) {
  memvarTimed<int, std::chrono::milliseconds> mt(1);
  std::this_thread::sleep_for(std::chrono::milliseconds(5));
  mt = 2;
  static_assert(std::is_same_v<decltype(mt.getTimeTag()), std::chrono::milliseconds>);
  EXPECT_GE(mt.getTimeTag(0).count(), 5);
  EXPECT_EQ(mt.getTimeTag(1).count(), 0);
}

TEST(MemvarTimedRealClock, MicrosecondsAndSystemClockTogether) {
  memvarTimed<long, std::chrono::microseconds, std::chrono::system_clock> mt(1L);
  mt = 2L;
  EXPECT_GE(mt.getTimeTag(0).count(), 0);
  EXPECT_EQ(mt(), 2L);
}

TEST(MemvarTimedRealClock, DefaultTimeUnitIsNanosecondsOnSteadyClock) {
  static_assert(std::is_same_v<memvarTimed<int>, memvarTimed<int, std::chrono::nanoseconds, std::chrono::steady_clock>>);
  memvarTimed<int> mt(1);
  static_assert(std::is_same_v<decltype(mt.getTimeTag()), std::chrono::nanoseconds>);
  SUCCEED();
}

// =============================================================================
// Section 29: Operators across kinds (memvar / memvarTimed / plain values),
// result types, mixed value types and operator return values
// =============================================================================

TEST(MemvarOperatorsMixed, ResultTypesFollowTheUnderlyingOperation) {
  static_assert(std::is_same_v<decltype(std::declval<memvar<int>&>() + std::declval<memvar<int>&>()), int>);
  static_assert(std::is_same_v<decltype(std::declval<memvar<int>&>() + 1.5), double>);
  static_assert(std::is_same_v<decltype(1.5f * std::declval<memvar<double>&>()), double>);
  static_assert(std::is_same_v<decltype(std::declval<memvar<int>&>() < 1), bool>);
  static_assert(std::is_same_v<decltype(std::declval<memvar<int>&>() == std::declval<memvar<int>&>()), bool>);
  static_assert(std::is_same_v<decltype(std::declval<memvar<std::string>&>() + std::string()), std::string>);
  // integral promotion applies, exactly as for the plain values
  static_assert(std::is_same_v<decltype(std::declval<memvar<char>&>() + std::declval<memvar<char>&>()), int>);
  SUCCEED();
}

TEST(MemvarOperatorsMixed, BetweenMemvarAndMemvarTimed) {
  memvar<int> a(6);
  memvarTimed<int> b(3);
  EXPECT_EQ(a + b, 9);
  EXPECT_EQ(b + a, 9);
  EXPECT_EQ(a - b, 3);
  EXPECT_EQ(a * b, 18);
  EXPECT_EQ(a / b, 2);
  EXPECT_TRUE(a > b);
  EXPECT_TRUE(b < a);
  EXPECT_TRUE(a >= b);
  EXPECT_TRUE(b <= a);
  EXPECT_FALSE(a == b);
  EXPECT_TRUE(a != b);
  memvarTimed<int> c(6);
  EXPECT_TRUE(a == c);
  EXPECT_TRUE(c == a);
}

TEST(MemvarOperatorsMixed, CompoundAcrossKinds) {
  memvar<int> a(6);
  memvarTimed<int> b(3);
  a += b;
  EXPECT_EQ(a(), 9);
  b *= a;
  EXPECT_EQ(b(), 27);
  a -= b;
  EXPECT_EQ(a(), -18);
  b /= a;
  EXPECT_EQ(b(), -1);
  EXPECT_EQ(a.getHistorySize(), 3);
  EXPECT_EQ(b.getHistorySize(), 3);
}

TEST(MemvarOperatorsMixed, DifferentValueTypes) {
  memvar<int> i(3);
  memvar<double> d(1.5);
  EXPECT_DOUBLE_EQ(i + d, 4.5);
  EXPECT_DOUBLE_EQ(d * i, 4.5);
  EXPECT_TRUE(d < i);
  EXPECT_TRUE(i > d);
  EXPECT_TRUE(i == 3.0);
  EXPECT_TRUE(3.0 == i);
  memvar<long long> big(1LL << 40);
  EXPECT_EQ(big + 1, (1LL << 40) + 1);
}

TEST(MemvarOperatorsMixed, CompoundNarrowsToTheMemvarsOwnType) {
  // The right-hand side may be wider; the result is converted back to T.
  memvar<int> i(1);
  i += 1.5;  // 2.5 -> 2
  EXPECT_EQ(i(), 2);
  i *= 2.9;  // 5.8 -> 5
  EXPECT_EQ(i(), 5);
  memvar<double> d(1.0);
  d += 1;
  EXPECT_DOUBLE_EQ(d(), 2.0);
}

TEST(MemvarOperatorsMixed, CompoundAssignmentReturnsTheLeftOperand) {
  memvar<int> m(1, 10);
  memvar<int>& r = (m += 2);
  EXPECT_EQ(&r, &m);
  ((m += 1) *= 3) -= 1;  // (1+2+1)*3-1
  EXPECT_EQ(m(), 11);
  EXPECT_EQ(m.getHistorySize(), 5);
  memvarTimed<int> t(1, 10);
  memvarTimed<int>& rt = (t /= 1);
  EXPECT_EQ(&rt, &t);
  (t += 1) += 1;
  EXPECT_EQ(t(), 3);
}

TEST(MemvarOperatorsMixed, AssignmentReturnsTheAssignedObject) {
  memvar<int> m(1);
  memvar<int>& r1 = (m = 5);
  EXPECT_EQ(&r1, &m);
  memvar<int> n(2);
  memvar<int>& r2 = (m = n);
  EXPECT_EQ(&r2, &m);
  memvarTimed<int> t(1);
  memvarTimed<int>& r3 = (t = 7);
  EXPECT_EQ(&r3, &t);
  memvarTimed<int> u(3);
  memvarTimed<int>& r4 = (t = u);
  EXPECT_EQ(&r4, &t);
  EXPECT_EQ(t(), 3);
}

TEST(MemvarOperatorsMixed, ComparisonsWithStringLiterals) {
  memvar<std::string> s(std::string("abc"));
  EXPECT_TRUE(s == "abc");
  EXPECT_TRUE("abc" == s);
  EXPECT_FALSE(s == "abd");
  EXPECT_TRUE(s < "abd");
  EXPECT_TRUE("abb" < s);
  EXPECT_EQ(s + "def", "abcdef");
  EXPECT_EQ("xyz" + s, "xyzabc");
  s += "!";
  EXPECT_EQ(s(), "abc!");
}

TEST(MemvarOperatorsMixed, NaNComparisonsFollowTheUnderlyingType) {
  memvar<double> nan(std::numeric_limits<double>::quiet_NaN());
  EXPECT_FALSE(nan == nan);
  EXPECT_TRUE(nan != nan);
  EXPECT_FALSE(nan < 1.0);
  EXPECT_FALSE(nan > 1.0);
  EXPECT_FALSE(nan <= nan);
  EXPECT_FALSE(nan >= nan);
}

TEST(MemvarOperatorsMixed, InfinityAndDivisionByZeroForDoubles) {
  memvar<double> a(1.0);
  memvar<double> zero(0.0);
  EXPECT_TRUE(std::isinf(a / zero));
  EXPECT_TRUE(std::isinf(-a / zero));
  a /= zero;
  EXPECT_TRUE(std::isinf(a()));
  EXPECT_TRUE(a > std::numeric_limits<double>::max());
}

TEST(MemvarOperatorsMixed, ReadingDoesNotWrite) {
  memvar<int> a(3);
  memvar<int> b(4);
  (void)(a + b);
  (void)(a == b);
  (void)(a < 1);
  (void)static_cast<int>(a);
  (void)a(0);
  EXPECT_EQ(a.getHistorySize(), 1);
  EXPECT_EQ(b.getHistorySize(), 1);
}

TEST(MemvarOperatorsMixed, OperatorsAreNotViableWhenTheValueOperationIsNot) {
  static_assert(CanAdd<memvar<int>, int>);
  static_assert(CanAdd<int, memvar<int>>);
  static_assert(CanAdd<memvar<int>, memvarTimed<double>>);
  static_assert(CanAdd<memvar<std::string>, std::string>);
  static_assert(!CanAdd<memvar<std::string>, int>);
  static_assert(!CanAdd<memvar<int>, std::string>);
  static_assert(!CanSub<memvar<std::string>, memvar<std::string>>);
  static_assert(!CanEqual<memvar<int>, std::string>);
  static_assert(!CanEqual<memvar<std::string>, memvar<int>>);
  static_assert(!CanLess<memvar<std::string>, int>);
  static_assert(!CanPlusEq<memvar<int>, std::string>);
  static_assert(!CanPlusEq<memvar<std::string>, int>);
  SUCCEED();
}

TEST(MemvarOperatorsMixed, WideStringsConcatenateAndCompare) {
  memvar<std::wstring> a(std::wstring(L"ab"));
  memvar<std::wstring> b(std::wstring(L"cd"));
  EXPECT_TRUE(a + b == std::wstring(L"abcd"));
  EXPECT_TRUE(a < b);
  a += b;
  EXPECT_TRUE(a() == std::wstring(L"abcd"));
}

TEST(MemvarOperatorsMixed, ConversionOperatorInFunctionArguments) {
  const auto twice = [](int x) { return 2 * x; };
  memvar<int> m(21);
  EXPECT_EQ(twice(m), 42);
  memvarTimed<int> t(5);
  EXPECT_EQ(twice(t), 10);
  const auto length = [](const std::string& s) { return s.size(); };
  memvar<std::string> s(std::string("four"));
  EXPECT_EQ(length(s), 4u);
}

// =============================================================================
// Section 30: Concepts and helpers: IsMemvar, valueOf, MemvarOperands,
// memvarAccess, MemVarType, AnyStandardString, IsAnyOf
// =============================================================================

TEST(MemvarConcepts, IsMemvar) {
  static_assert(mv::IsMemvar<memvar<int>>);
  static_assert(mv::IsMemvar<const memvar<std::string>>);
  static_assert(mv::IsMemvar<memvarTimed<double>>);
  static_assert(mv::IsMemvar<FakeTimed>);
  static_assert(!mv::IsMemvar<int>);
  static_assert(!mv::IsMemvar<std::string>);
  static_assert(!mv::IsMemvar<bip::bigint>);
  SUCCEED();
}

TEST(MemvarConcepts, MemvarOperands) {
  static_assert(mv::MemvarOperands<std::plus<>, memvar<int>, int>);
  static_assert(mv::MemvarOperands<std::plus<>, int, memvar<int>>);
  static_assert(mv::MemvarOperands<std::plus<>, memvar<int>, memvarTimed<int>>);
  static_assert(!mv::MemvarOperands<std::plus<>, int, int>);              // no memvar involved
  static_assert(!mv::MemvarOperands<std::minus<>, memvar<std::string>, std::string>);  // not valid on values
  static_assert(mv::MemvarOperands<std::equal_to<>, memvar<std::string>, const char*>);
  SUCCEED();
}

TEST(MemvarConcepts, ValueOfPassesPlainValuesThrough) {
  const int x = 4;
  const std::string s = "str";
  EXPECT_EQ(mv::valueOf(x), 4);
  EXPECT_EQ(mv::valueOf(s), "str");
  memvar<int> m(8);
  EXPECT_EQ(mv::valueOf(m), 8);
  memvarTimed<int> t(9);
  EXPECT_EQ(mv::valueOf(t), 9);
}

TEST(MemvarAccessHelper, GetReturnsTheCurrentValueByReference) {
  memvar<std::string> m(std::string("a"));
  m = std::string("b");
  const std::string& r = mv::memvarAccess::get(m);
  EXPECT_EQ(r, "b");
  EXPECT_EQ(&r, &m.getMemVarHistory().front());
  memvarTimed<int> t(1);
  t = 2;
  EXPECT_EQ(mv::memvarAccess::get(t), 2);
}

TEST(MemvarAccessHelper, SetPushesANewValueForBothKinds) {
  memvar<int> m(1, 3);
  mv::memvarAccess::set(m, 2);
  EXPECT_EQ(m(), 2);
  EXPECT_EQ(m.getHistorySize(), 2);
  const int lvalue = 3;
  mv::memvarAccess::set(m, lvalue);
  EXPECT_EQ(m(), 3);
  memvarTimed<std::string> t(std::string("a"), 3);
  mv::memvarAccess::set(t, std::string("b"));
  std::string s = "c";
  mv::memvarAccess::set(t, s);
  mv::memvarAccess::set(t, std::move(s));
  EXPECT_EQ(t(), "c");
  EXPECT_EQ(t.getHistorySize(), 3);
  EXPECT_EQ(t(2), "b");
}

TEST(MemvarConcepts, MemVarTypeAcceptsExactlyTheDocumentedTypes) {
  using namespace mv;
  static_assert(MemVarType<char>);
  static_assert(MemVarType<signed char>);
  static_assert(MemVarType<unsigned char>);
  static_assert(MemVarType<short>);
  static_assert(MemVarType<unsigned long long>);
  static_assert(MemVarType<wchar_t>);
  static_assert(MemVarType<char8_t>);
  static_assert(MemVarType<char16_t>);
  static_assert(MemVarType<char32_t>);
  static_assert(MemVarType<float>);
  static_assert(MemVarType<long double>);
  static_assert(MemVarType<std::string>);
  static_assert(MemVarType<std::wstring>);
  static_assert(MemVarType<std::u8string>);
  static_assert(MemVarType<std::u16string>);
  static_assert(MemVarType<std::u32string>);
  static_assert(MemVarType<const std::string>);  // cv/ref are ignored
  static_assert(MemVarType<const int&>);
  static_assert(!MemVarType<bool>);
  static_assert(!MemVarType<const bool>);
  static_assert(!MemVarType<void*>);
  static_assert(!MemVarType<int*>);
  static_assert(!MemVarType<std::string_view>);
  static_assert(!MemVarType<std::nullptr_t>);
  static_assert(!MemVarType<std::chrono::nanoseconds>);
  SUCCEED();
}

TEST(MemvarConcepts, AnyStandardStringAndIsAnyOf) {
  using namespace mv;
  static_assert(AnyStandardString<std::string>);
  static_assert(AnyStandardString<const std::wstring&>);
  static_assert(AnyStandardString<std::u8string>);
  static_assert(AnyStandardString<std::u16string>);
  static_assert(AnyStandardString<std::u32string>);
  static_assert(!AnyStandardString<std::string_view>);
  static_assert(!AnyStandardString<const char*>);
  static_assert(!AnyStandardString<int>);
  static_assert(IsAnyOf<int, char, int>);
  static_assert(IsAnyOf<int, int>);
  static_assert(!IsAnyOf<int, char, long>);
  static_assert(!IsAnyOf<int>);
  static_assert(is_bigint_v<bip::bigint>);
  static_assert(!is_bigint_v<const bip::bigint>);  // exact type only; MemVarType strips cv itself
  static_assert(!is_bigint_v<long long>);
  SUCCEED();
}

TEST(MemvarConcepts, SafeRefMacroConstantsAndSelectedMode) {
  static_assert(MEMVAR_SAFEREF_ABORT == 0);
  static_assert(MEMVAR_SAFEREF_THROWS == 1);
  static_assert(MEMVAR_SAFEREF_NO_CHECK == 2);
  static_assert(MEMVAR_SAFEREF_CHECK == MEMVAR_SAFEREF_ABORT ||
                MEMVAR_SAFEREF_CHECK == MEMVAR_SAFEREF_THROWS ||
                MEMVAR_SAFEREF_CHECK == MEMVAR_SAFEREF_NO_CHECK);
  SUCCEED();
}

// =============================================================================
// Section 31: Increment / decrement edge cases
// =============================================================================

TEST(MemvarIncrementEdges, SmallIntegerTypesWrapThroughTheCastToT) {
  memvar<unsigned char> u(255);
  ++u;
  EXPECT_EQ(u(), 0);
  --u;
  EXPECT_EQ(u(), 255);
  memvar<short> s(std::numeric_limits<short>::max());
  ++s;
  EXPECT_EQ(s(), std::numeric_limits<short>::min());
  memvar<std::int8_t> b(std::numeric_limits<std::int8_t>::min());
  --b;
  EXPECT_EQ(b(), std::numeric_limits<std::int8_t>::max());
}

TEST(MemvarIncrementEdges, PostfixOnSmallTypeReturnsOldValue) {
  memvar<unsigned char> u(255);
  const unsigned char old = u++;
  EXPECT_EQ(old, 255);
  EXPECT_EQ(u(), 0);
}

TEST(MemvarIncrementEdges, FloatingPointPostfix) {
  memvar<double> d(1.5);
  const double old = d++;
  EXPECT_DOUBLE_EQ(old, 1.5);
  EXPECT_DOUBLE_EQ(d(), 2.5);
  const double oldDec = d--;
  EXPECT_DOUBLE_EQ(oldDec, 2.5);
  EXPECT_DOUBLE_EQ(d(), 1.5);
}

TEST(MemvarIncrementEdges, PrefixReturnsAValueNotAReference) {
  static_assert(std::is_same_v<decltype(++std::declval<memvar<int>&>()), int>);
  static_assert(std::is_same_v<decltype(--std::declval<memvar<int>&>()), int>);
  static_assert(std::is_same_v<decltype(std::declval<memvar<int>&>()++), int>);
  static_assert(std::is_same_v<decltype(std::declval<memvar<int>&>()--), int>);
  static_assert(std::is_same_v<decltype(++std::declval<memvarTimed<int>&>()), int>);
  SUCCEED();
}

TEST(MemvarIncrementEdges, TimedIncrementsAreConstrainedLikeTheUntimedOnes) {
  static_assert(CanPreInc<memvarTimed<int>>);
  static_assert(CanPreDec<memvarTimed<int>>);
  static_assert(CanPostInc<memvarTimed<int>>);
  static_assert(CanPostDec<memvarTimed<int>>);
  static_assert(!CanPreInc<memvarTimed<std::string>>);
  static_assert(!CanPreDec<memvarTimed<std::string>>);
  static_assert(!CanPostInc<memvarTimed<std::string>>);
  static_assert(!CanPostDec<memvarTimed<std::string>>);
  static_assert(!CanPreInc<memvar<std::wstring>>);
  static_assert(!CanPostDec<memvar<std::u8string>>);
  static_assert(!CanPreInc<memvar<std::u16string>>);
  static_assert(!CanPostInc<memvar<std::u32string>>);
  static_assert(CanPreInc<memvar<bip::bigint>>);
  static_assert(CanPostDec<memvar<bip::bigint>>);
  SUCCEED();
}

TEST(MemvarIncrementEdges, BigintPostfixAndPrefixValues) {
  memvar<bip::bigint> m(bip::bigint(5));
  const bip::bigint old = m++;
  EXPECT_EQ(old.value(), 5);
  EXPECT_EQ(m().value(), 6);
  const bip::bigint oldDec = m--;
  EXPECT_EQ(oldDec.value(), 6);
  EXPECT_EQ(m().value(), 5);
  const bip::bigint fresh = --m;
  EXPECT_EQ(fresh.value(), 4);
  EXPECT_EQ(m.getHistorySize(), 4);
}

TEST(MemvarIncrementEdges, IncrementEvictsWhenFull) {
  memvar<int> m(0, 2);
  ++m;
  ++m;
  ++m;
  EXPECT_EQ(m(0), 3);
  EXPECT_EQ(m(1), 2);
  EXPECT_EQ(m.getHistorySize(), 2);
}

TEST(MemvarIncrementEdges, PostfixOnTimedWithEvictionKeepsValueAndTagPaired) {
  FakeClock::ticks = 1000;
  FakeTimed mt(0, 2);
  FakeClock::advance(1);
  mt++;
  FakeClock::advance(1);
  const int old = mt++;
  EXPECT_EQ(old, 1);
  EXPECT_EQ(mt(0), 2);
  EXPECT_EQ(mt(1), 1);
  EXPECT_EQ(mt.getTimeTag(0).count(), 2);
  EXPECT_EQ(mt.getTimeTag(1).count(), 1);
}

// =============================================================================
// Section 32: Copy / move overload selection and self-assignment
// (bip::bigint counts its copies and moves)
// =============================================================================

TEST(MemvarCopyMove, ConstructionFromAnLvalueCopiesOnce) {
  bip::bigint::resetCounters();
  const bip::bigint v(7);
  memvar<bip::bigint> m(v);
  EXPECT_EQ(bip::bigint::copies, 1);
  EXPECT_EQ(bip::bigint::moves, 0);
}

TEST(MemvarCopyMove, AssignmentFromAnLvalueCopies) {
  memvar<bip::bigint> m(bip::bigint(1));
  const bip::bigint v(2);
  bip::bigint::resetCounters();
  m = v;
  EXPECT_EQ(bip::bigint::copies, 1);
  EXPECT_EQ(bip::bigint::moves, 0);
  EXPECT_EQ(m().value(), 2);
}

TEST(MemvarCopyMove, AssignmentFromAnRvalueMoves) {
  memvar<bip::bigint> m(bip::bigint(1));
  bip::bigint::resetCounters();
  m = bip::bigint(2);
  EXPECT_EQ(bip::bigint::moves, 1);
  EXPECT_EQ(bip::bigint::copies, 0);
  EXPECT_EQ(m().value(), 2);
}

TEST(MemvarCopyMove, TimedAssignmentFromAnRvalueMovesAndFromAnLvalueCopies) {
  memvarTimed<bip::bigint> m(bip::bigint(1));
  bip::bigint::resetCounters();
  m = bip::bigint(2);
  EXPECT_EQ(bip::bigint::moves, 1);
  EXPECT_EQ(bip::bigint::copies, 0);
  const bip::bigint v(3);
  bip::bigint::resetCounters();
  m = v;
  EXPECT_EQ(bip::bigint::copies, 1);
  EXPECT_EQ(bip::bigint::moves, 0);
  EXPECT_EQ(m().value(), 3);
}

TEST(MemvarCopyMove, CopyAssignmentBetweenMemvarsCopiesTheCurrentValue) {
  memvar<bip::bigint> a(bip::bigint(1));
  memvar<bip::bigint> b(bip::bigint(2));
  bip::bigint::resetCounters();
  a = b;
  EXPECT_EQ(bip::bigint::copies, 1);
  EXPECT_EQ(bip::bigint::moves, 0);
  EXPECT_EQ(a().value(), 2);
  EXPECT_EQ(b().value(), 2);
}

TEST(MemvarCopyMove, CompoundAssignmentMovesTheComputedResult) {
  memvar<bip::bigint> m(bip::bigint(1));
  bip::bigint::resetCounters();
  m += bip::bigint(2);
  EXPECT_EQ(bip::bigint::copies, 0);
  EXPECT_GE(bip::bigint::moves, 1);
  EXPECT_EQ(m().value(), 3);
}

TEST(MemvarCopyMove, ClearHistoryMovesADefaultConstructedValue) {
  memvar<bip::bigint> m(bip::bigint(9));
  bip::bigint::resetCounters();
  m.clearHistory();
  EXPECT_EQ(bip::bigint::copies, 0);
  EXPECT_EQ(m().value(), 0);
}

TEST(MemvarCopyMove, MoveOfAStringIntoTheHistory) {
  memvar<std::string> m(std::string("a"));
  std::string source(200, 'z');
  const char* heap = source.data();
  m = std::move(source);
  ASSERT_EQ(m.getMemVarHistory().front().size(), 200u);
  // the buffer was stolen, not copied
  EXPECT_EQ(m.getMemVarHistory().front().data(), heap);
}

TEST(MemvarSelfAssignment, WhenFullStillKeepsTheValueAndTheCapacity) {
  memvar<int> m(1, 2);
  m = 2;  // full: [2,1]
  selfAssign(m);  // push 2 again: [2,2]
  EXPECT_EQ(m.getHistorySize(), 2);
  EXPECT_EQ(m(0), 2);
  EXPECT_EQ(m(1), 2);
}

TEST(MemvarSelfAssignment, StringSelfAssignmentKeepsContent) {
  memvar<std::string> m(std::string("keep"), 3);
  selfAssign(m);
  selfAssign(m);
  EXPECT_EQ(m(), "keep");
  EXPECT_EQ(m(1), "keep");
  EXPECT_EQ(m(2), "keep");
  selfAssign(m);  // evicts the oldest "keep"
  EXPECT_EQ(m.getHistorySize(), 3);
}

TEST(MemvarSelfAssignment, TimedSelfAssignmentPushesAgain) {
  FakeClock::ticks = 1000;
  FakeTimed mt(5, 4);
  FakeClock::advance(2);
  selfAssign(mt);
  EXPECT_EQ(mt.getHistorySize(), 2);
  EXPECT_EQ(mt(0), 5);
  EXPECT_EQ(mt.getTimeTag(0).count(), 2);
  EXPECT_EQ(mt.getTimeTag(1).count(), 0);
}

TEST(MemvarSelfAssignment, CompoundWithItselfUsesTheValueBeforeTheWrite) {
  memvar<int> m(3, 4);
  m += m;
  EXPECT_EQ(m(), 6);
  m *= m;
  EXPECT_EQ(m(), 36);
  m -= m;
  EXPECT_EQ(m(), 0);
  EXPECT_EQ(m.getHistorySize(), 4);
  memvarTimed<int> t(4, 4);
  t += t;
  t *= t;
  EXPECT_EQ(t(), 64);
}

// =============================================================================
// Section 33: Strong exception guarantee. A write is a single emplace_front; if
// constructing the new entry throws, nothing has changed. bip::bigint can be
// told to throw from its copy and move constructors.
// =============================================================================

TEST(MemvarStrongGuarantee, FailedCopyWriteLeavesHistoryUntouched) {
  memvar<bip::bigint> m(bip::bigint(1), 3);
  m = bip::bigint(2);
  const bip::bigint v(99);
  {
    FailCopiesAndMoves fail;
    EXPECT_THROW(m = v, bip::bigint::construction_error);
  }
  EXPECT_EQ(m.getHistorySize(), 2);
  EXPECT_EQ(m().value(), 2);
  EXPECT_EQ(m(1).value(), 1);
  EXPECT_FALSE(m.isHistoryFull());
}

TEST(MemvarStrongGuarantee, FailedMoveWriteLeavesHistoryUntouched) {
  memvar<bip::bigint> m(bip::bigint(1), 2);
  m = bip::bigint(2);  // now full
  {
    FailCopiesAndMoves fail;
    EXPECT_THROW(m = bip::bigint(3), bip::bigint::construction_error);
  }
  // a failed write must not evict the oldest value
  ASSERT_EQ(m.getHistorySize(), 2);
  EXPECT_EQ(m(0).value(), 2);
  EXPECT_EQ(m(1).value(), 1);
}

TEST(MemvarStrongGuarantee, FailedIncrementAndCompoundLeaveHistoryUntouched) {
  memvar<bip::bigint> m(bip::bigint(5), 4);
  {
    FailCopiesAndMoves fail;
    EXPECT_THROW(++m, bip::bigint::construction_error);
    EXPECT_THROW(m--, bip::bigint::construction_error);
    EXPECT_THROW(m += bip::bigint(1), bip::bigint::construction_error);
    EXPECT_THROW(m *= bip::bigint(2), bip::bigint::construction_error);
  }
  EXPECT_EQ(m.getHistorySize(), 1);
  EXPECT_EQ(m().value(), 5);
}

TEST(MemvarStrongGuarantee, FailedClearHistoryKeepsTheOldHistory) {
  memvar<bip::bigint> m(bip::bigint(1), 4);
  m = bip::bigint(2);
  m = bip::bigint(3);
  {
    FailCopiesAndMoves fail;
    EXPECT_THROW(m.clearHistory(), bip::bigint::construction_error);
  }
  ASSERT_EQ(m.getHistorySize(), 3);
  EXPECT_EQ(m(0).value(), 3);
  EXPECT_EQ(m(1).value(), 2);
  EXPECT_EQ(m(2).value(), 1);
}

TEST(MemvarStrongGuarantee, FailedConstructionThrowsAndDoesNotLeak) {
  const bip::bigint v(1);
  FailCopiesAndMoves fail;
  EXPECT_THROW(memvar<bip::bigint>(v, 4), bip::bigint::construction_error);
  EXPECT_THROW(memvarTimed<bip::bigint>(v, 4), bip::bigint::construction_error);
}

TEST(MemvarStrongGuarantee, TimedFailedWriteLeavesValuesTagsAndEpochUntouched) {
  FakeClock::ticks = 1000;
  memvarTimed<bip::bigint, std::chrono::milliseconds, FakeClock> mt(bip::bigint(1), 3);
  FakeClock::advance(5);
  mt = bip::bigint(2);
  FakeClock::advance(5);
  {
    FailCopiesAndMoves fail;
    EXPECT_THROW(mt = bip::bigint(3), bip::bigint::construction_error);
    const bip::bigint v(4);
    EXPECT_THROW(mt = v, bip::bigint::construction_error);
    EXPECT_THROW(++mt, bip::bigint::construction_error);
  }
  ASSERT_EQ(mt.getHistorySize(), 2);
  EXPECT_EQ(mt(0).value(), 2);
  EXPECT_EQ(mt(1).value(), 1);
  EXPECT_EQ(mt.getTimeTag(0).count(), 5);
  EXPECT_EQ(mt.getTimeTag(1).count(), 0);
  // and the variable is fully usable afterwards
  mt = bip::bigint(7);
  EXPECT_EQ(mt().value(), 7);
  EXPECT_EQ(mt.getTimeTag(0).count(), 10);
}

TEST(MemvarStrongGuarantee, TimedFailedClearHistoryKeepsTheOldEpoch) {
  FakeClock::ticks = 1000;
  memvarTimed<bip::bigint, std::chrono::milliseconds, FakeClock> mt(bip::bigint(1), 3);
  FakeClock::advance(8);
  mt = bip::bigint(2);
  FakeClock::advance(8);
  {
    FailCopiesAndMoves fail;
    EXPECT_THROW(mt.clearHistory(), bip::bigint::construction_error);
  }
  ASSERT_EQ(mt.getHistorySize(), 2);
  EXPECT_EQ(mt(0).value(), 2);
  // the epoch did not move: tags are still relative to the original first write
  EXPECT_EQ(mt.getTimeTag(0).count(), 8);
  EXPECT_EQ(mt.getTimeTag(1).count(), 0);
}

TEST(MemvarStrongGuarantee, MemvarStaysUsableAfterAFailedWrite) {
  memvar<bip::bigint> m(bip::bigint(1), 2);
  {
    FailCopiesAndMoves fail;
    EXPECT_THROW(m = bip::bigint(2), bip::bigint::construction_error);
  }
  m = bip::bigint(3);
  m = bip::bigint(4);
  EXPECT_EQ(m(0).value(), 4);
  EXPECT_EQ(m(1).value(), 3);
  EXPECT_EQ(m.getHistorySize(), 2);
}

// =============================================================================
// Section 34: cref() and SafeRef. What it is depends on MEMVAR_SAFEREF_CHECK.
// =============================================================================

#if MEMVAR_SAFEREF_CHECK == MEMVAR_SAFEREF_NO_CHECK

TEST(MemvarCref, NoCheckModeReturnsAPlainConstReference) {
  static_assert(std::is_same_v<decltype(std::declval<const memvar<int>&>().cref()), const int&>);
  static_assert(std::is_same_v<decltype(std::declval<const memvarTimed<int>&>().cref()), const int&>);
  static_assert(std::is_same_v<decltype(std::declval<const memvar<std::string>&>().cref()), const std::string&>);
  memvar<std::string> m(std::string("v"));
  EXPECT_EQ(&m.cref(), &m.getMemVarHistory().front());
  memvarTimed<std::string> t(std::string("v"));
  EXPECT_EQ(&t.cref(), &t.getMemVarHistory().front().value);
}

TEST(MemvarCref, NoCheckModeReferenceStaysValidForCapacityMinusOneWrites) {
  memvar<int> m(10, 3);
  const int& r = m.cref();
  m = 11;
  m = 12;  // r's slot is now the oldest one, but still stored
  EXPECT_EQ(r, 10);
}

TEST(MemvarCref, NoCheckModeAfterClearHistoryCrefIsTheNewValue) {
  memvar<int> m(10, 3);
  m = 11;
  m.clearHistory();
  EXPECT_EQ(m.cref(), 0);
}

#else  // checked modes (0 = abort, 1 = throw)

TEST(MemvarCref, CheckedModeReturnsASafeRef) {
  static_assert(std::is_same_v<memvar<int>::SafeRef, mv::detail::SafeRef<int>>);
  static_assert(std::is_same_v<decltype(std::declval<const memvar<int>&>().cref()), memvar<int>::SafeRef>);
  static_assert(std::is_same_v<memvarTimed<std::string>::SafeRef, mv::detail::SafeRef<std::string>>);
  static_assert(std::is_same_v<decltype(std::declval<const memvarTimed<int>&>().cref()), memvarTimed<int>::SafeRef>);
  static_assert(std::is_copy_constructible_v<memvar<int>::SafeRef>);
  static_assert(std::is_convertible_v<memvar<int>::SafeRef, const int&>);
  SUCCEED();
}

TEST(MemvarCref, ValidHandleReadsTheValueThreeWays) {
  memvar<std::string> m(std::string("hello"));
  const auto r = m.cref();
  EXPECT_EQ(*r, "hello");
  EXPECT_EQ(r->size(), 5u);
  const std::string& asRef = r;
  EXPECT_EQ(asRef, "hello");
  EXPECT_EQ(&*r, &m.getMemVarHistory().front());
}

TEST(MemvarCref, ValidHandleForTimedVariable) {
  memvarTimed<int> t(5);
  const auto r = t.cref();
  EXPECT_EQ(*r, 5);
  const int viaConversion = r;
  EXPECT_EQ(viaConversion, 5);
  EXPECT_EQ(&*r, &t.getMemVarHistory().front().value);
}

TEST(MemvarCref, HandleStaysValidForCapacityMinusOneWrites) {
  memvar<int> m(10, 3);
  const auto r = m.cref();
  m = 11;
  m = 12;
  EXPECT_EQ(*r, 10);  // still the oldest stored entry
}

TEST(MemvarCref, HandleOfAnOlderValueRemainsValidWhileItsSlotIsStored) {
  memvar<int> m(1, 4);
  m = 2;
  const auto r = m.cref();  // refers to the value 2
  m = 3;
  m = 4;
  m = 5;  // 3 writes since cref(): capacity 4 -> still stored
  EXPECT_EQ(*r, 2);
}

TEST(MemvarCref, CopiedHandleHasTheSameExpiry) {
  memvar<int> m(1, 3);
  const auto r1 = m.cref();
  const auto r2 = r1;
  m = 2;
  m = 3;
  EXPECT_EQ(*r1, 1);
  EXPECT_EQ(*r2, 1);
}

TEST(MemvarCref, FreshHandleAfterEvictionIsValid) {
  memvar<int> m(1, 2);
  m = 2;
  m = 3;
  const auto r = m.cref();
  EXPECT_EQ(*r, 3);
}

TEST(MemvarCref, FailedWriteDoesNotAgeAHandle) {
  memvar<bip::bigint> m(bip::bigint(1), 2);
  const auto r = m.cref();
  for (int i = 0; i < 5; ++i) {
    FailCopiesAndMoves fail;
    EXPECT_THROW(m = bip::bigint(2), bip::bigint::construction_error);
  }
  EXPECT_EQ(r->value(), 1);  // five failed writes: the handle is as young as before
  m = bip::bigint(2);  // one real write: capacity 2 still not reached
  EXPECT_EQ(r->value(), 1);
}

TEST(MemvarCref, TimedFailedWriteDoesNotAgeAHandle) {
  memvarTimed<bip::bigint> m(bip::bigint(1), 2);
  const auto r = m.cref();
  {
    FailCopiesAndMoves fail;
    EXPECT_THROW(m = bip::bigint(2), bip::bigint::construction_error);
    EXPECT_THROW(m = bip::bigint(3), bip::bigint::construction_error);
  }
  EXPECT_EQ(r->value(), 1);
}

#endif  // MEMVAR_SAFEREF_CHECK modes

#if MEMVAR_SAFEREF_CHECK == MEMVAR_SAFEREF_THROWS

TEST(MemvarCrefThrows, DereferenceAfterEvictionThrows) {
  memvar<int> m(1, 3);
  const auto r = m.cref();
  m = 2;
  m = 3;
  EXPECT_NO_THROW((void)*r);
  m = 4;  // third write since cref(): the slot is gone
  EXPECT_THROW((void)*r, mv::expired_history_reference);
}

TEST(MemvarCrefThrows, AllThreeAccessPathsThrowWhenExpired) {
  memvar<std::string> m(std::string("x"), 2);
  const auto r = m.cref();
  m = std::string("y");
  EXPECT_NO_THROW((void)*r);  // capacity 2: still stored after one write
  m = std::string("z");
  EXPECT_THROW((void)*r, mv::expired_history_reference);
  EXPECT_THROW((void)r->size(), mv::expired_history_reference);
  EXPECT_THROW((void)static_cast<const std::string&>(r), mv::expired_history_reference);
}

TEST(MemvarCrefThrows, ExceptionIsALogicErrorWithADescriptiveMessage) {
  static_assert(std::is_base_of_v<std::logic_error, mv::expired_history_reference>);
  memvar<int> m(1, 2);
  const auto r = m.cref();
  m = 2;
  m = 3;
  try {
    (void)*r;
    FAIL() << "expected mv::expired_history_reference";
  } catch (const std::logic_error& e) {
    const std::string what {e.what()};
    EXPECT_NE(what.find("evicted from history"), std::string::npos);
    EXPECT_NE(what.find("clearHistory()"), std::string::npos);
  }
}

TEST(MemvarCrefThrows, ClearHistoryExpiresEveryOutstandingHandle) {
  memvar<int> m(1, 100);  // huge capacity: only clearHistory() can expire it
  const auto r1 = m.cref();
  m = 2;
  const auto r2 = m.cref();
  m.clearHistory();
  EXPECT_THROW((void)*r1, mv::expired_history_reference);
  EXPECT_THROW((void)*r2, mv::expired_history_reference);
  const auto fresh = m.cref();
  EXPECT_EQ(*fresh, 0);
}

TEST(MemvarCrefThrows, ClearHistoryOnTimedExpiresHandles) {
  memvarTimed<int> t(1, 100);
  const auto r = t.cref();
  t.clearHistory();
  EXPECT_THROW((void)*r, mv::expired_history_reference);
  EXPECT_EQ(*t.cref(), 0);
}

TEST(MemvarCrefThrows, TimedHandleExpiresAfterCapacityWrites) {
  memvarTimed<int> t(1, 2);
  const auto r = t.cref();
  EXPECT_EQ(*r, 1);
  t = 2;
  EXPECT_EQ(*r, 1);  // capacity 2: still stored after one write
  t = 3;
  EXPECT_THROW((void)*r, mv::expired_history_reference);
}

TEST(MemvarCrefThrows, IncrementsAndCompoundAssignmentsAgeHandles) {
  memvar<int> m(1, 4);
  const auto r = m.cref();
  ++m;
  m++;
  m += 1;
  EXPECT_EQ(*r, 1);
  m -= 1;  // fourth write
  EXPECT_THROW((void)*r, mv::expired_history_reference);
}

TEST(MemvarCrefThrows, DereferenceIsNotNoexceptInThisMode) {
  memvar<int> m(1);
  const auto r = m.cref();
  static_assert(!noexcept(*r));
  static_assert(!noexcept(r.operator->()));
  static_assert(!noexcept(static_cast<const int&>(r)));
  SUCCEED();
}

TEST(MemvarCrefThrows, ExpiredHandleKeepsThrowingEveryTime) {
  memvar<int> m(1, 2);
  const auto r = m.cref();
  m = 2;
  m = 3;
  for (int i = 0; i < 3; ++i) {
    EXPECT_THROW((void)*r, mv::expired_history_reference);
  }
}

#endif  // THROWS

#if MEMVAR_SAFEREF_CHECK == MEMVAR_SAFEREF_ABORT

TEST(MemvarCrefAbort, DereferenceIsNoexceptInThisMode) {
  memvar<int> m(1);
  const auto r = m.cref();
  static_assert(noexcept(*r));
  static_assert(noexcept(r.operator->()));
  static_assert(noexcept(static_cast<const int&>(r)));
  SUCCEED();
}

TEST(MemvarCrefAbort, ValidHandleDoesNotAbort) {
  memvar<int> m(1, 3);
  const auto r = m.cref();
  m = 2;
  m = 3;
  EXPECT_EQ(*r, 1);
}

TEST(MemvarCrefAbortDeathTest, ExpiredDereferenceAborts) {
  memvar<int> m(1, 2);
  const auto r = m.cref();
  m = 2;
  m = 3;
  EXPECT_DEATH_IF_SUPPORTED(MEMVAR_DIE_WITH_COVERAGE((void)*r), "FATAL: mv::memvar::cref\\(\\): the referenced value has been evicted");
}

TEST(MemvarCrefAbortDeathTest, ExpiredArrowAndConversionAbort) {
  memvar<std::string> m(std::string("x"), 2);
  const auto r = m.cref();
  m = std::string("y");
  m = std::string("z");
  EXPECT_DEATH_IF_SUPPORTED(MEMVAR_DIE_WITH_COVERAGE((void)r->size()), "Terminating");
  EXPECT_DEATH_IF_SUPPORTED(MEMVAR_DIE_WITH_COVERAGE((void)static_cast<const std::string&>(r)), "Terminating");
}

TEST(MemvarCrefAbortDeathTest, ClearHistoryMakesOutstandingHandlesAbort) {
  memvar<int> m(1, 100);
  const auto r = m.cref();
  m.clearHistory();
  EXPECT_DEATH_IF_SUPPORTED(MEMVAR_DIE_WITH_COVERAGE((void)*r), "evicted from history");
}

TEST(MemvarCrefAbortDeathTest, TimedExpiredHandleAborts) {
  memvarTimed<int> t(1, 2);
  const auto r = t.cref();
  t = 2;
  t = 3;
  EXPECT_DEATH_IF_SUPPORTED(MEMVAR_DIE_WITH_COVERAGE((void)*r), "Terminating");
}

#endif  // ABORT

// =============================================================================
// Section 35: UTF-8 conversion for the non-char strings (operator<< overloads
// in the global namespace, mv::detail::toUtf8 / appendUtf8)
// =============================================================================

TEST(Utf8Streaming, EmptyStringsStreamNothing) {
  std::ostringstream os;
  os << std::wstring() << std::u8string() << std::u16string() << std::u32string();
  EXPECT_EQ(os.str(), "");
}

TEST(Utf8Streaming, OneToFourByteSequencesFromU32) {
  const std::u32string s {U'A', char32_t{0xE9}, char32_t{0x20AC}, char32_t{0x1D11E}};
  std::ostringstream os;
  os << s;
  // 'A' (1 byte), e-acute (2), euro sign (3), musical G clef (4)
  EXPECT_EQ(os.str(), "A\xC3\xA9\xE2\x82\xAC\xF0\x9D\x84\x9E");
}

TEST(Utf8Streaming, EncodingBoundaries) {
  const std::pair<char32_t, std::string> cases[] = {
      {0x00, std::string(1, '\0')},
      {0x7F, "\x7F"},
      {0x80, "\xC2\x80"},
      {0x7FF, "\xDF\xBF"},
      {0x800, "\xE0\xA0\x80"},
      {0xD7FF, "\xED\x9F\xBF"},
      {0xE000, "\xEE\x80\x80"},
      {0xFFFF, "\xEF\xBF\xBF"},
      {0x10000, "\xF0\x90\x80\x80"},
      {0x10FFFF, "\xF4\x8F\xBF\xBF"},
  };
  for (const auto& [cp, utf8] : cases) {
    std::ostringstream os;
    os << std::u32string(1, cp);
    EXPECT_EQ(os.str(), utf8) << "code point " << static_cast<std::uint32_t>(cp);
  }
}

TEST(Utf8Streaming, InvalidCodePointsBecomeReplacementCharacter) {
  const std::string replacement = "\xEF\xBF\xBD";  // U+FFFD
  for (const char32_t bad : {char32_t{0xD800}, char32_t{0xDBFF}, char32_t{0xDC00}, char32_t{0xDFFF},
                             char32_t{0x110000}, char32_t{0xFFFFFFFF}}) {
    std::ostringstream os;
    os << std::u32string(1, bad);
    EXPECT_EQ(os.str(), replacement) << static_cast<std::uint32_t>(bad);
  }
}

TEST(Utf8Streaming, U16BmpCharactersAreEncodedDirectly) {
  const std::u16string s {u'A', char16_t{0xE9}, char16_t{0x20AC}};
  std::ostringstream os;
  os << s;
  EXPECT_EQ(os.str(), "A\xC3\xA9\xE2\x82\xAC");
}

TEST(Utf8Streaming, U16SurrogatePairIsCombinedIntoOneCodePoint) {
  const std::u16string s {char16_t{0xD834}, char16_t{0xDD1E}};  // U+1D11E
  std::ostringstream os;
  os << s;
  EXPECT_EQ(os.str(), "\xF0\x9D\x84\x9E");
}

TEST(Utf8Streaming, U16SurrogatePairBoundaries) {
  const std::u16string lowest {char16_t{0xD800}, char16_t{0xDC00}};   // U+10000
  const std::u16string highest {char16_t{0xDBFF}, char16_t{0xDFFF}};  // U+10FFFF
  std::ostringstream a, b;
  a << lowest;
  b << highest;
  EXPECT_EQ(a.str(), "\xF0\x90\x80\x80");
  EXPECT_EQ(b.str(), "\xF4\x8F\xBF\xBF");
}

TEST(Utf8Streaming, U16LoneHighSurrogateAtTheEndBecomesReplacement) {
  const std::u16string s {u'a', char16_t{0xD834}};
  std::ostringstream os;
  os << s;
  EXPECT_EQ(os.str(), "a\xEF\xBF\xBD");
}

TEST(Utf8Streaming, U16HighSurrogateFollowedByANonLowSurrogateIsReplacedAlone) {
  const std::u16string s {char16_t{0xD834}, u'b'};
  std::ostringstream os;
  os << s;
  EXPECT_EQ(os.str(), "\xEF\xBF\xBD" "b");
}

TEST(Utf8Streaming, U16HighSurrogateFollowedByAnotherHighSurrogate) {
  const std::u16string s {char16_t{0xD834}, char16_t{0xD834}, char16_t{0xDD1E}};
  std::ostringstream os;
  os << s;
  // first high surrogate is alone (replacement), the second pairs with the low one
  EXPECT_EQ(os.str(), "\xEF\xBF\xBD\xF0\x9D\x84\x9E");
}

TEST(Utf8Streaming, U16LoneLowSurrogateBecomesReplacement) {
  const std::u16string s {char16_t{0xDD1E}};
  std::ostringstream os;
  os << s;
  EXPECT_EQ(os.str(), "\xEF\xBF\xBD");
}

TEST(Utf8Streaming, WideStringUsesTheNativeCodeUnitWidth) {
  const std::wstring s = L"Aé€";
  std::ostringstream os;
  os << s;
  EXPECT_EQ(os.str(), "A\xC3\xA9\xE2\x82\xAC");
  if constexpr (sizeof(wchar_t) == 4) {
    const std::wstring clef(1, static_cast<wchar_t>(0x1D11E));
    std::ostringstream o2;
    o2 << clef;
    EXPECT_EQ(o2.str(), "\xF0\x9D\x84\x9E");
  }
}

TEST(Utf8Streaming, U8StringIsWrittenVerbatim) {
  const std::u8string s = u8"Aé€\U0001D11E";
  std::ostringstream os;
  os << s;
  EXPECT_EQ(os.str(), "A\xC3\xA9\xE2\x82\xAC\xF0\x9D\x84\x9E");
}

TEST(Utf8Streaming, U8StringWithEmbeddedNulKeepsAllBytes) {
  const std::u8string s {u8'a', u8'\0', u8'b'};
  std::ostringstream os;
  os << s;
  EXPECT_EQ(os.str(), std::string("a\0b", 3));
}

TEST(Utf8Streaming, StreamingToUtf8ChainsWithOtherOutput) {
  std::ostringstream os;
  os << 1 << std::u16string(u"é") << "|" << std::wstring(L"x") << 2;
  EXPECT_EQ(os.str(), "1\xC3\xA9|x2");
}

TEST(Utf8Streaming, DetailHelpersDirectly) {
  std::string out;
  mv::detail::appendUtf8(out, 0x41);
  mv::detail::appendUtf8(out, 0x110000);
  EXPECT_EQ(out, "A\xEF\xBF\xBD");
  EXPECT_EQ(mv::detail::toUtf8(std::basic_string<char32_t>(U"€")), "\xE2\x82\xAC");
  EXPECT_EQ(mv::detail::toUtf8(std::basic_string<char16_t>()), "");
}

// =============================================================================
// Section 36: memvar of the other string types, printing and streaming
// =============================================================================

TEST(MemvarStringKinds, U8StringHistoryAndPrinting) {
  memvar<std::u8string> m(std::u8string(u8"a"));
  m = std::u8string(u8"é");
  std::ostringstream os;
  m.printHistoryData(os, "|");
  EXPECT_EQ(os.str(), "[ \xC3\xA9|a|]\n");
  std::ostringstream o2;
  o2 << m;
  EXPECT_EQ(o2.str(), "\xC3\xA9");
}

TEST(MemvarStringKinds, U16StringHistoryAndPrinting) {
  memvar<std::u16string> m(std::u16string(u"a"));
  m = std::u16string(u"€");
  std::ostringstream os;
  m.printReverseHistoryData(os);
  EXPECT_EQ(os.str(), "[ a \xE2\x82\xAC ]\n");
  std::ostringstream o2;
  o2 << m;
  EXPECT_EQ(o2.str(), "\xE2\x82\xAC");
}

TEST(MemvarStringKinds, U32StringHistoryAndMinMax) {
  memvar<std::u32string> m(std::u32string(U"b"));
  m = std::u32string(U"a");
  m = std::u32string(U"c");
  const auto [lo, hi] = m.getHistoryMinMax();
  EXPECT_TRUE(lo == std::u32string(U"a"));
  EXPECT_TRUE(hi == std::u32string(U"c"));
  std::ostringstream os;
  os << m;
  EXPECT_EQ(os.str(), "c");
}

TEST(MemvarStringKinds, WStringStreamAndTimedStream) {
  memvar<std::wstring> m(std::wstring(L"é"));
  std::ostringstream os;
  os << m;
  EXPECT_EQ(os.str(), "\xC3\xA9");
  memvarTimed<std::wstring> t(std::wstring(L"é"));
  std::ostringstream o2;
  o2 << t;
  EXPECT_EQ(o2.str(), "[0:\xC3\xA9]");
}

TEST(MemvarStringKinds, EmptyStringsAreFine) {
  memvar<std::string> m(std::string{}, 2);
  m = std::string{};
  std::ostringstream os;
  m.printHistoryData(os, ",");
  EXPECT_EQ(os.str(), "[ ,,]\n");
  const auto [lo, hi] = m.getHistoryMinMax();
  EXPECT_EQ(lo, "");
  EXPECT_EQ(hi, "");
}

TEST(MemvarStringKinds, EmbeddedNulCharactersSurvive) {
  const std::string withNul("a\0b", 3);
  memvar<std::string> m(withNul);
  EXPECT_EQ(m().size(), 3u);
  EXPECT_EQ(m(), withNul);
}

// =============================================================================
// Section 37: Printing: default arguments (std::cout), empty/odd separators
// =============================================================================

TEST(MemvarPrintingDefaults, PrintHistoryDataDefaultsToStdCoutWithSpaceSeparator) {
  memvar<int> m(1);
  m = 2;
  CoutCapture cap;
  m.printHistoryData();
  EXPECT_EQ(cap.str(), "[ 2 1 ]\n");
}

TEST(MemvarPrintingDefaults, PrintReverseHistoryDataDefaultsToStdCout) {
  memvar<int> m(1);
  m = 2;
  CoutCapture cap;
  m.printReverseHistoryData();
  EXPECT_EQ(cap.str(), "[ 1 2 ]\n");
}

TEST(MemvarPrintingDefaults, TimedPlainPrintersDefaultToStdCout) {
  memvarTimed<int> mt(1);
  mt = 2;
  CoutCapture cap;
  mt.printHistoryData();
  mt.printReverseHistoryData();
  EXPECT_EQ(cap.str(), "[ 2 1 ]\n[ 1 2 ]\n");
}

TEST(MemvarPrintingDefaults, EmptySeparatorGluesValuesTogether) {
  memvar<int> m(1);
  m = 2;
  m = 3;
  std::ostringstream os;
  m.printHistoryData(os, "");
  EXPECT_EQ(os.str(), "[ 321]\n");
}

TEST(MemvarPrintingDefaults, MultiCharacterSeparatorAfterEveryValue) {
  memvar<int> m(1);
  m = 2;
  std::ostringstream os;
  m.printReverseHistoryData(os, " -> ");
  EXPECT_EQ(os.str(), "[ 1 -> 2 -> ]\n");
}

TEST(MemvarPrintingDefaults, PrintsFloatingPointAndNegativeValues) {
  memvar<double> m(-1.5);
  m = 2.25;
  std::ostringstream os;
  m.printHistoryData(os);
  EXPECT_EQ(os.str(), "[ 2.25 -1.5 ]\n");
}

TEST(MemvarPrintingDefaults, PrintingFullHistoryAfterWrapAround) {
  memvar<int> m(0, 3);
  for (int i = 1; i <= 5; ++i) m = i;
  std::ostringstream fwd, rev;
  m.printHistoryData(fwd);
  m.printReverseHistoryData(rev);
  EXPECT_EQ(fwd.str(), "[ 5 4 3 ]\n");
  EXPECT_EQ(rev.str(), "[ 3 4 5 ]\n");
}

TEST(MemvarPrintingDefaults, PrintingDoesNotModifyTheHistory) {
  memvar<int> m(1);
  std::ostringstream os;
  m.printHistoryData(os);
  m.printReverseHistoryData(os);
  EXPECT_EQ(m.getHistorySize(), 1);
}

TEST(MemvarStreamingMore, StreamsStringsDoublesAndChains) {
  memvar<std::string> s(std::string("txt"));
  memvar<double> d(2.5);
  memvar<int> i(7);
  std::ostringstream os;
  os << s << ' ' << d << ' ' << i;
  EXPECT_EQ(os.str(), "txt 2.5 7");
}

TEST(MemvarStreamingMore, StreamsBigint) {
  memvar<bip::bigint> m(bip::bigint(123456789012LL));
  std::ostringstream os;
  os << m;
  EXPECT_EQ(os.str(), "123456789012");
  memvarTimed<bip::bigint> t(bip::bigint(5));
  std::ostringstream o2;
  o2 << t;
  EXPECT_EQ(o2.str(), "[0:5]");
}

TEST(MemvarStreamingMore, StreamsCharAsACharacter) {
  memvar<char> m('x');
  std::ostringstream os;
  os << m;
  EXPECT_EQ(os.str(), "x");
}

// =============================================================================
// Section 38: getHistoryMinMax edge cases
// =============================================================================

TEST(MemvarMinMaxEdges, ReturnsTheFirstOfEqualValuesWithoutFailing) {
  memvar<int> m(4);
  m = 4;
  m = 4;
  const auto [lo, hi] = m.getHistoryMinMax();
  EXPECT_EQ(lo, 4);
  EXPECT_EQ(hi, 4);
}

TEST(MemvarMinMaxEdges, EvictedExtremesNoLongerCount) {
  memvar<int> m(100, 3);  // 100 will be evicted
  m = 5;
  m = 6;
  m = 7;
  const auto [lo, hi] = m.getHistoryMinMax();
  EXPECT_EQ(lo, 5);
  EXPECT_EQ(hi, 7);
}

TEST(MemvarMinMaxEdges, ClearHistoryResetsTheRange) {
  memvar<int> m(100);
  m = -100;
  m.clearHistory();
  const auto [lo, hi] = m.getHistoryMinMax();
  EXPECT_EQ(lo, 0);
  EXPECT_EQ(hi, 0);
}

TEST(MemvarMinMaxEdges, ReturnTypeIsATupleOfCopies) {
  static_assert(std::is_same_v<decltype(std::declval<const memvar<int>&>().getHistoryMinMax()),
                               std::tuple<int, int>>);
  static_assert(std::is_same_v<decltype(std::declval<const memvar<std::string>&>().getHistoryMinMax()),
                               std::tuple<std::string, std::string>>);
  static_assert(std::is_same_v<decltype(std::declval<const memvarTimed<double>&>().getHistoryMinMax()),
                               std::tuple<double, double>>);
  SUCCEED();
}

TEST(MemvarMinMaxEdges, ExtremeIntegerValues) {
  memvar<std::int64_t> m(std::numeric_limits<std::int64_t>::max());
  m = std::numeric_limits<std::int64_t>::min();
  m = 0;
  const auto [lo, hi] = m.getHistoryMinMax();
  EXPECT_EQ(lo, std::numeric_limits<std::int64_t>::min());
  EXPECT_EQ(hi, std::numeric_limits<std::int64_t>::max());
}

TEST(MemvarMinMaxEdges, InfinitiesAndNegativeZero) {
  memvar<double> m(0.0);
  m = std::numeric_limits<double>::infinity();
  m = -std::numeric_limits<double>::infinity();
  const auto [lo, hi] = m.getHistoryMinMax();
  EXPECT_TRUE(std::isinf(lo) && lo < 0);
  EXPECT_TRUE(std::isinf(hi) && hi > 0);
}

TEST(MemvarMinMaxEdges, TimedOnStringsIsLexicographic) {
  memvarTimed<std::string> t(std::string("m"));
  t = std::string("a");
  t = std::string("z");
  const auto [lo, hi] = t.getHistoryMinMax();
  EXPECT_EQ(lo, "a");
  EXPECT_EQ(hi, "z");
}

TEST(MemvarMinMaxEdges, TimedOnBigint) {
  memvarTimed<bip::bigint> t(bip::bigint(3));
  t = bip::bigint(-8);
  t = bip::bigint(11);
  const auto [lo, hi] = t.getHistoryMinMax();
  EXPECT_EQ(lo.value(), -8);
  EXPECT_EQ(hi.value(), 11);
}

// =============================================================================
// Section 39: clearHistory() edge cases
// =============================================================================

TEST(MemvarClearHistory, KeepsTheCapacity) {
  memvar<int> m(5, 4);
  m = 6;
  m = 7;
  m.clearHistory();
  EXPECT_EQ(m.getHistoryCapacity(), 4);
  EXPECT_EQ(m.getHistorySize(), 1);
  EXPECT_FALSE(m.isHistoryFull());
}

TEST(MemvarClearHistory, HistoryCanFillAgainAfterwards) {
  memvar<int> m(5, 3);
  m = 6;
  m = 7;
  EXPECT_TRUE(m.isHistoryFull());
  m.clearHistory();
  EXPECT_FALSE(m.isHistoryFull());
  m = 1;
  m = 2;
  EXPECT_TRUE(m.isHistoryFull());
  EXPECT_EQ(m(0), 2);
  EXPECT_EQ(m(1), 1);
  EXPECT_EQ(m(2), 0);
}

TEST(MemvarClearHistory, OnAFreshObjectJustWritesDefaultValue) {
  memvar<int> m(9);
  m.clearHistory();
  EXPECT_EQ(m(), 0);
  EXPECT_EQ(m.getHistorySize(), 1);
}

TEST(MemvarClearHistory, ResetsStringsAndDoublesToTheirDefaults) {
  memvar<std::string> s(std::string("abc"));
  s.clearHistory();
  EXPECT_EQ(s(), "");
  memvar<double> d(1.5);
  d.clearHistory();
  EXPECT_DOUBLE_EQ(d(), 0.0);
  memvar<bip::bigint> b(bip::bigint(9));
  b.clearHistory();
  EXPECT_EQ(b().value(), 0);
}

TEST(MemvarClearHistory, ThePreviousHistoryIsNoLongerReachable) {
  memvar<int> m(1, 5);
  m = 2;
  m = 3;
  m.clearHistory();
  EXPECT_THROW(m(1), std::out_of_range);
  ASSERT_EQ(m.getMemVarHistory().size(), 1u);
  EXPECT_EQ(m.getMemVarHistory().front(), 0);
}

TEST(MemvarClearHistory, TimedClearKeepsCapacityAndEmptiesTheHistory) {
  memvarTimed<int> mt(5, 3);
  mt = 6;
  mt = 7;
  EXPECT_TRUE(mt.isHistoryFull());
  mt.clearHistory();
  EXPECT_EQ(mt.getHistoryCapacity(), 3);
  EXPECT_EQ(mt.getHistorySize(), 1);
  EXPECT_FALSE(mt.isHistoryFull());
  EXPECT_THROW(mt.getTimeTag(1), std::out_of_range);
}

// =============================================================================
// Section 40: More value types, as far as MemVarType allows
// =============================================================================

TEST(MemvarMoreTypes, ShortAndUnsignedLongLong) {
  memvar<short> s(-5);
  s += 10;
  EXPECT_EQ(s(), 5);
  memvar<unsigned long long> u(1ULL << 63);
  u += 1;
  EXPECT_EQ(u(), (1ULL << 63) + 1);
}

TEST(MemvarMoreTypes, WideCharacterTypes) {
  memvar<wchar_t> w(L'a');
  ++w;
  EXPECT_EQ(w(), L'b');
  memvar<char16_t> c16(u'a');
  ++c16;
  EXPECT_EQ(c16(), u'b');
  memvar<char32_t> c32(U'a');
  c32 += 2;
  EXPECT_EQ(c32(), U'c');
  memvar<char8_t> c8(u8'a');
  --c8;
  EXPECT_TRUE(c8() == u8'`');
}

TEST(MemvarMoreTypes, FloatingPointSpecials) {
  memvar<float> f(1.5f);
  f /= 0.0f;
  EXPECT_TRUE(std::isinf(f()));
  memvar<long double> ld(2.0L);
  ld -= 0.5L;
  EXPECT_TRUE(ld == 1.5L);
}

TEST(MemvarMoreTypes, TimedWithEveryKindOfValue) {
  memvarTimed<char> c('a');
  ++c;
  EXPECT_EQ(c(), 'b');
  memvarTimed<unsigned> u(1u);
  u += 1u;
  EXPECT_EQ(u(), 2u);
  memvarTimed<float> f(1.0f);
  f *= 3.0f;
  EXPECT_FLOAT_EQ(f(), 3.0f);
  memvarTimed<std::u16string> s(std::u16string(u"a"));
  s += std::u16string(u"b");
  EXPECT_TRUE(s() == std::u16string(u"ab"));
  memvarTimed<bip::bigint> b(bip::bigint(2));
  b *= bip::bigint(21);
  EXPECT_EQ(b().value(), 42);
}

TEST(MemvarMoreTypes, BigintTimedCompoundAndIncrement) {
  memvarTimed<bip::bigint> b(bip::bigint(10));
  b += bip::bigint(5);
  b -= bip::bigint(3);
  b /= bip::bigint(4);
  const bip::bigint old = b++;
  EXPECT_EQ(old.value(), 3);
  EXPECT_EQ(b().value(), 4);
  EXPECT_EQ(b.getHistorySize(), 5);
}

TEST(MemvarMoreTypes, BigintComparisonsWithPlainBigint) {
  memvar<bip::bigint> m(bip::bigint(5));
  EXPECT_TRUE(m == bip::bigint(5));
  EXPECT_TRUE(bip::bigint(5) == m);
  EXPECT_TRUE(m < bip::bigint(6));
  EXPECT_TRUE(bip::bigint(4) < m);
  EXPECT_TRUE(m >= bip::bigint(5));
  EXPECT_TRUE(m <= bip::bigint(5));
  EXPECT_FALSE(m > bip::bigint(5));
  EXPECT_EQ((m + bip::bigint(1)).value(), 6);
  EXPECT_EQ((m * m).value(), 25);
  EXPECT_EQ((m - bip::bigint(5)).value(), 0);
  EXPECT_EQ((m / bip::bigint(5)).value(), 1);
}

// =============================================================================
// Section 41: Wrap-around of the bounded history, long runs and capacity 2
// =============================================================================

TEST(MemvarWrapAround, ThousandsOfWritesKeepOnlyTheNewestCapacityValues) {
  memvar<int> m(0, 7);
  for (int i = 1; i <= 5000; ++i) m = i;
  ASSERT_EQ(m.getHistorySize(), 7);
  for (int k = 0; k < 7; ++k) {
    EXPECT_EQ(m(k), 5000 - k) << k;
  }
  EXPECT_TRUE(m.isHistoryFull());
}

TEST(MemvarWrapAround, MinimumCapacityKeepsTheCurrentAndThePreviousValue) {
  memvar<std::string> m(std::string("a"), 2);
  m = std::string("b");
  m = std::string("c");
  EXPECT_EQ(m(0), "c");
  EXPECT_EQ(m(1), "b");
  EXPECT_THROW(m(2), std::out_of_range);
}

TEST(MemvarWrapAround, TimedThousandsOfWritesPairValuesAndTags) {
  FakeClock::ticks = 1000;
  FakeTimed mt(0, 5);
  for (int i = 1; i <= 1000; ++i) {
    FakeClock::advance(1);
    mt = i;
  }
  ASSERT_EQ(mt.getHistorySize(), 5);
  for (int k = 0; k < 5; ++k) {
    EXPECT_EQ(mt(k), 1000 - k);
    EXPECT_EQ(mt.getTimeTag(k).count(), 1000 - k);
  }
}

TEST(MemvarWrapAround, HistorySizeNeverExceedsCapacityForAnyOperator) {
  memvar<int> m(1, 3);
  for (int i = 0; i < 20; ++i) {
    m += 1;
    ++m;
    m--;
    selfAssign(m);
    m *= 1;
    EXPECT_LE(m.getHistorySize(), 3);
  }
  EXPECT_EQ(m.getHistorySize(), 3);
}

}  // namespace

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
