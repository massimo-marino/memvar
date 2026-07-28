//
// memvar_test.cpp
//
// Comprehensive GoogleTest suite for memvar.h (mv::memvar<T> / mv::memvarTimed<T>)
//
#include <gtest/gtest.h>

#include <cmath>
#include <sstream>
#include <string>
#include <thread>
#include <type_traits>
#include <chrono>
#include <limits>

// -----------------------------------------------------------------------------
// A minimal, self-contained "bigint" type used only to exercise the
// mv::is_bigint<T> specialization point declared in memvar.h. memvar.h only
// forward-declares bip::bigint, so a concrete definition is required here to
// be able to instantiate mv::memvar<bip::bigint>.
// -----------------------------------------------------------------------------
namespace bip {
class bigint {
public:
  bigint() = default;
  bigint(long long v) : value_(v) {}  // NOLINT - intentional implicit conversion

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

TEST(MemvarAccess, IndexOrderingOldestToNewest) {
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
  // operator++(int) calls voidIncr1() (which pushes the new value onto the
  // front of the history) and then returns getHistoryValue(1) - i.e. the
  // entry just behind the new front - which is the pre-increment value.
  // This matches idiomatic C++ postfix semantics.
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
  EXPECT_EQ(os.str(), "[ 42 \b \b ]\n");
}

TEST(MemvarPrinting, PrintHistoryDataMultipleValuesNewestFirst) {
  memvar<int> m(1);
  m = 2;
  m = 3;
  std::ostringstream os;
  m.printHistoryData(os);
  EXPECT_EQ(os.str(), "[ 3 2 1 \b \b ]\n");
}

TEST(MemvarPrinting, PrintReverseHistoryDataOldestFirst) {
  memvar<int> m(1);
  m = 2;
  m = 3;
  std::ostringstream os;
  m.printReverseHistoryData(os);
  EXPECT_EQ(os.str(), "[ 1 2 3 \b \b ]\n");
}

TEST(MemvarPrinting, PrintHistoryDataCustomSeparator) {
  memvar<int> m(1);
  m = 2;
  std::ostringstream os;
  m.printHistoryData(os, ", ");
  EXPECT_EQ(os.str(), "[ 2, 1, \b \b ]\n");
}

// =============================================================================
// Section 8: Protected-member coverage via a testable subclass
// -----------------------------------------------------------------------------
// memvar's empty-history branch inside memvarPrinter() is unreachable through
// the public API (memo_ always holds >= 1 element). A small subclass exposes
// the protected surface so we can exercise that branch and the raw
// incr1/decr1/voidIncr1/voidDecr1/getValue/setValue primitives directly,
// improving coverage without altering memvar.h.
// =============================================================================

class TestableMemvar : public memvar<int> {
public:
  using memvar<int>::memvar;

  using memvar<int>::getValue;
  using memvar<int>::setValue;
  using memvar<int>::incr1;
  using memvar<int>::voidIncr1;
  using memvar<int>::decr1;
  using memvar<int>::voidDecr1;

  void printEmptyHistory(std::ostream& os) const {
    memvarHistory empty{};
    memvarPrinter(empty, os);
  }

  void printReverseFlag(std::ostream& os, bool reverse) const {
    memvarPrinter(memo_, os, reverse);
  }
};

TEST(MemvarProtectedSurface, GetValueReturnsCurrentValue) {
  TestableMemvar m(3);
  EXPECT_EQ(m.getValue(), 3);
}

TEST(MemvarProtectedSurface, SetValueDirectlyPushesHistory) {
  TestableMemvar m(3);
  m.setValue(9);
  EXPECT_EQ(m.getValue(), 9);
  EXPECT_EQ(m.getHistorySize(), 2);
}

TEST(MemvarProtectedSurface, Incr1AndDecr1ReturnNewValue) {
  TestableMemvar m(3);
  EXPECT_EQ(m.incr1(), 4);
  EXPECT_EQ(m.decr1(), 3);
}

TEST(MemvarProtectedSurface, VoidIncr1AndVoidDecr1MutateWithoutReturn) {
  TestableMemvar m(3);
  m.voidIncr1();
  EXPECT_EQ(m.getValue(), 4);
  m.voidDecr1();
  EXPECT_EQ(m.getValue(), 3);
}

TEST(MemvarProtectedSurface, MemvarPrinterEmptyHistoryPrintsEmptyBrackets) {
  TestableMemvar m(3);
  std::ostringstream os;
  m.printEmptyHistory(os);
  EXPECT_EQ(os.str(), "[]\n");
}

TEST(MemvarProtectedSurface, MemvarPrinterReverseFlagControlsOrder) {
  TestableMemvar m(1);
  m.setValue(2);
  m.setValue(3);

  std::ostringstream forward;
  m.printReverseFlag(forward, false);
  EXPECT_EQ(forward.str(), "[ 3 2 1 \b \b ]\n");

  std::ostringstream reverse;
  m.printReverseFlag(reverse, true);
  EXPECT_EQ(reverse.str(), "[ 1 2 3 \b \b ]\n");
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
// Section 10: Arithmetic free operators (+, -, *, /) returning plain T
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
  EXPECT_EQ(os.str(), "[ c b a \b \b ]\n");
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
  // Compile-time guarantee (checked via type traits) that the constrained
  // overloads are excluded for std::string, matching the `requires
  // (!AnyStandardString<T>)` clauses in memvar.h.
  EXPECT_FALSE((std::is_invocable_v<decltype(std::minus<>{}), memvar<std::string>&, std::string>) ||
               false);  // placeholder to document intent; real guarantee is compile-time below.
}

// The following static_asserts double as compile-time regression tests: if
// memvar.h's requires-clauses regress and accidentally allow arithmetic on
// strings, this translation unit would fail to compile with a clearer error
// at the point of instantiation. They are written as SFINAE-friendly checks.
namespace compile_time_checks {
template <typename T, typename = void>
struct has_minus_equals : std::false_type {};
template <typename T>
struct has_minus_equals<T, std::void_t<decltype(std::declval<memvar<T>&>() -= std::declval<T>())>>
    : std::true_type {};

static_assert(has_minus_equals<int>::value, "int should support operator-=");
static_assert(!has_minus_equals<std::string>::value, "std::string must NOT support operator-=");
}  // namespace compile_time_checks

// =============================================================================
// Section 14: Wide/unicode string overloads for operator<< (top-of-file helpers)
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
  EXPECT_EQ(os.str(), "[ def abc \b \b ]\n");
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
  // the first (only) value's time tag should be effectively zero relative
  // to the memvar's own epoch, which is set at that same construction point.
  EXPECT_GE(mt.getTimeTag().count(), 0);
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

TEST(MemvarTimed, IsFinalCannotBeDerivedFrom) {
  EXPECT_TRUE(std::is_final_v<memvarTimed<int>>);
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
  auto epochBefore = mt.getTimeTag(0).count();
  std::this_thread::sleep_for(std::chrono::milliseconds(2));

  mt.clearHistory();

  EXPECT_EQ(mt.getHistorySize(), 1);
  EXPECT_EQ(mt(), 0);
  // after clearing, the sole remaining entry's time tag is relative to the
  // freshly reset epoch, so it should be (near) zero, not the old value.
  EXPECT_LE(mt.getTimeTag(0).count(), epochBefore + std::chrono::milliseconds(1).count() * 1000000LL);
}

TEST(MemvarTimed, ClearHistoryIsPolymorphicThroughBasePointer) {
  memvarTimed<int> mt(5);
  mt = 6;
  memvar<int>* base = &mt;
  base->clearHistory();
  EXPECT_EQ(mt(), 0);
  EXPECT_EQ(mt.getHistorySize(), 1);
}

TEST(MemvarTimed, SetValueIsPolymorphicThroughBasePointer) {
  memvarTimed<int> mt(1);
  memvar<int>* base = &mt;
  *base = 42;  // calls memvar<int>::operator=, which calls the virtual setValue
  EXPECT_EQ(mt(), 42);
  // A time tag should have been recorded for the new value too.
  EXPECT_EQ(mt.getHistorySize(), 2);
  EXPECT_NO_THROW(mt.getHistoryTimedValue(0));
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
// integral type or any floating-point type, but the original suite only
// exercised int/double.
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

// Note: memvarTimed's constructors and clearHistory() assign Clock::now()
// (whose type is Clock::time_point, i.e. time_point<Clock, Clock::duration>)
// directly into a time_point<Clock, Time> member. That only compiles when
// Time is the clock's own native duration (or something it converts to
// implicitly, which durations of differing period generally don't). So a
// "custom" Time/Clock combination must keep the two in sync.
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
// Section 21: memvarTimed increment/decrement operators (inherited from
// memvar<T>, but must still update the time-tagged history polymorphically
// through the overridden virtual setValue()).
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

TEST(MemvarTimed, BracketOperatorInheritedFromBase) {
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
// Section 23: Further compile-time exclusion checks for std::string — the
// original suite only verified operator-=; the same `requires
// (!AnyStandardString<T>)` clause also guards *=, /=, and ++/--.
// =============================================================================

namespace compile_time_checks {

template <typename T, typename = void>
struct has_times_equals : std::false_type {};
template <typename T>
struct has_times_equals<T, std::void_t<decltype(std::declval<memvar<T>&>() *= std::declval<T>())>>
    : std::true_type {};

template <typename T, typename = void>
struct has_divide_equals : std::false_type {};
template <typename T>
struct has_divide_equals<T, std::void_t<decltype(std::declval<memvar<T>&>() /= std::declval<T>())>>
    : std::true_type {};

template <typename T, typename = void>
struct has_preincrement : std::false_type {};
template <typename T>
struct has_preincrement<T, std::void_t<decltype(++std::declval<memvar<T>&>())>>
    : std::true_type {};

template <typename T, typename = void>
struct has_predecrement : std::false_type {};
template <typename T>
struct has_predecrement<T, std::void_t<decltype(--std::declval<memvar<T>&>())>>
    : std::true_type {};

static_assert(has_times_equals<int>::value, "int should support operator*=");
static_assert(!has_times_equals<std::string>::value, "std::string must NOT support operator*=");

static_assert(has_divide_equals<int>::value, "int should support operator/=");
static_assert(!has_divide_equals<std::string>::value, "std::string must NOT support operator/=");

static_assert(has_preincrement<int>::value, "int should support operator++");
static_assert(!has_preincrement<std::string>::value, "std::string must NOT support operator++");

static_assert(has_predecrement<int>::value, "int should support operator--");
static_assert(!has_predecrement<std::string>::value, "std::string must NOT support operator--");

}  // namespace compile_time_checks

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
  m = m;
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

}  // namespace

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
