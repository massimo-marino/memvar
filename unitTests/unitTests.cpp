//
// unitTests.cpp
//
#include "bigint.h"
#include "../memvar.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <memory>
#include <type_traits>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace ::testing;
using namespace std::string_literals;
////////////////////////////////////////////////////////////////////////////////
// a simple function's performance timer
template <typename Time = std::chrono::nanoseconds>
struct perftimer {
  using Clock = std::chrono::steady_clock;

  template <typename F, typename... Args>
  static Time duration(F&& f, Args&&... args) {
    const auto start = Clock::now();

    std::invoke(std::forward<F>(f), std::forward<Args>(args)...);

    const auto end = Clock::now();

    return std::chrono::duration_cast<Time>(end - start);
  }
};

////////////////////////////////////////////////////////////////////////////////

TEST(memVarTimedTest, timeTaggedTest_0)
{
  using memvarType = int64_t;
  mv::memvarTimed<memvarType> mvt{0,10};

  mvt = 1;
  ASSERT_EQ(mvt[0], 1);
  ASSERT_EQ(mvt[1], 0);

  mvt = 2;
  ASSERT_EQ(mvt[0], 2);
  ASSERT_EQ(mvt[1], 1);

  mvt = 3;
  ASSERT_EQ(mvt[0], 3);
  ASSERT_EQ(mvt[1], 2);

  mvt = 4;
  ASSERT_EQ(mvt[0], 4);
  ASSERT_EQ(mvt[1], 3);

  mvt = 5;
  ASSERT_EQ(mvt[0], 5);
  ASSERT_EQ(mvt[1], 4);

  mvt = 6;
  ASSERT_EQ(mvt[0], 6);
  ASSERT_EQ(mvt[1], 5);

  mvt = 7;
  ASSERT_EQ(mvt[0], 7);
  ASSERT_EQ(mvt[1], 6);

  mvt = 8;
  ASSERT_EQ(mvt[0], 8);
  ASSERT_EQ(mvt[1], 7);

  mvt = 9;
  ASSERT_EQ(mvt[1], 8);

  ASSERT_EQ(mvt[0], 9);
  ASSERT_EQ(mvt[0], mvt(0));

  ASSERT_EQ(mvt[1], 8);
  ASSERT_EQ(mvt[1], mvt(1));

  ASSERT_EQ(mvt[2], 7);
  ASSERT_EQ(mvt[2], mvt(2));

  ASSERT_EQ(mvt[3], 6);
  ASSERT_EQ(mvt[3], mvt(3));

  ASSERT_EQ(mvt[4], 5);
  ASSERT_EQ(mvt[4], mvt(4));

  ASSERT_EQ(mvt[5], 4);
  ASSERT_EQ(mvt[5], mvt(5));

  ASSERT_EQ(mvt[6], 3);
  ASSERT_EQ(mvt[6], mvt(6));

  ASSERT_EQ(mvt[7], 2);
  ASSERT_EQ(mvt[7], mvt(7));

  ASSERT_EQ(mvt[8], 1);
  ASSERT_EQ(mvt[8], mvt(8));

  ASSERT_EQ(mvt[9], 0);
  ASSERT_EQ(mvt[9], mvt(9));
}

TEST(memVarTimedTest, timeTaggedTest_1)
{
  EXPECT_THROW(mv::memvarTimed<int> mvti(0,0), std::invalid_argument);
  EXPECT_THROW(mv::memvarTimed<int> mvti(0,-10), std::invalid_argument);
  EXPECT_NO_THROW(mv::memvarTimed<int> mvti {});
  EXPECT_NO_THROW(mv::memvarTimed<int> mvti(11, 20));

  EXPECT_THROW(mv::memvarTimed<double> mvtd(0.0,0), std::invalid_argument);
  EXPECT_THROW(mv::memvarTimed<double> mvtd(0.0,-10), std::invalid_argument);
  EXPECT_NO_THROW(mv::memvarTimed<double> mvtd {});
  EXPECT_NO_THROW(mv::memvarTimed<double> mvtd(11.2345, 20));

  EXPECT_THROW(mv::memvarTimed<std::string> mvts("Hello World!",0), std::invalid_argument);
  EXPECT_THROW(mv::memvarTimed<std::string> mvts("Hello World!",-10), std::invalid_argument);
  EXPECT_NO_THROW(mv::memvarTimed<std::string> mvts {});
  EXPECT_NO_THROW(mv::memvarTimed<std::string> mvts("Hello World!", 20));

  EXPECT_THROW(mv::memvarTimed<bip::bigint> mvtbi(bip::bigint("1234567890"),0), std::invalid_argument);
  EXPECT_THROW(mv::memvarTimed<bip::bigint> mvtbi(bip::bigint("1234567890"),-10), std::invalid_argument);
  EXPECT_NO_THROW(mv::memvarTimed<bip::bigint> mvtbi {});
  EXPECT_NO_THROW(mv::memvarTimed<bip::bigint> mvtbi(bip::bigint("1234567890"), 20));
}

TEST(memVarTimedTest, timeTaggedTest_2)
{
  mv::memvarTimed<int> mvt{0,2};
  ASSERT_EQ(0, mvt);
  ASSERT_EQ(2, mvt.getHistoryCapacity());
  std::cout << mvt << '\n';
  for (int i = 1; i <= 1'000; ++i)
  {
    mvt = i;
  }
  ASSERT_EQ(2, mvt.getHistoryCapacity());
  ASSERT_EQ(1'000, mvt);

  auto [min, max] = mvt.getHistoryMinMax();
  ASSERT_EQ(999, min);
  ASSERT_EQ(1'000, max);
  std::cout << "mvt: "; mvt.printHistoryTimedData();
  std::cout << mvt << '\n';
}

TEST(memVarTimedTest, timeTaggedTest_3)
{
  mv::memvarTimed<int> mvt {};
  mv::memvarTimed<int> mvt2 {};
  ASSERT_EQ(10, mvt.getHistoryCapacity());
  std::cout << "mvt: "; mvt.printHistoryTimedData();
  ASSERT_EQ(int {}, mvt);

  int v = mvt;
  ASSERT_EQ(int {}, v);
  ASSERT_EQ(0, v);

  v = 123;
  ASSERT_NE(int {}, v);
  ASSERT_NE(v, mvt);

  int w = mvt;
  ASSERT_EQ(int {}, w);
  ASSERT_EQ(0, w);

  int j;
  mvt++;
  ++mvt;
  j = mvt + 20 + mvt;
  ASSERT_EQ(24, j);

  mvt = 120;
  mvt--;
  ASSERT_EQ(119, mvt);
  --mvt;
  ASSERT_EQ(118, mvt);
  ASSERT_EQ(119, mvt(1));
  ASSERT_EQ(120, mvt(2));
  ASSERT_EQ(2, mvt(3));
  ASSERT_EQ(1, mvt(4));
  ASSERT_EQ(0, mvt(5));
  std::cout << "mvt: "; mvt.printHistoryTimedData();
  std::cout << "mvt: "; mvt.printReverseHistoryTimedData();
  std::cout << mvt << '\n';

  mvt2 = mvt;
  ASSERT_EQ(118, mvt2);
  mvt2 += mvt;
  ASSERT_EQ(236, mvt2);

  mvt2 -= 36;
  ASSERT_EQ(200, mvt2);

  mvt2 *= 5;
  ASSERT_EQ(1'000, mvt2);

  mvt2 /= 10;
  ASSERT_EQ(100, mvt2);

  std::cout << "mvt2: "; mvt2.printHistoryTimedData();
  std::cout << "mvt2: "; mvt2.printReverseHistoryTimedData();
  std::cout << mvt2 << '\n';

  auto [min, max] = mvt.getHistoryMinMax();
  ASSERT_EQ(0, min);
  ASSERT_EQ(120, max);
  auto [min2, max2] = mvt2.getHistoryMinMax();
  ASSERT_EQ(0, min2);
  ASSERT_EQ(1000, max2);

  mvt.clearHistory();
  mvt2.clearHistory();
  ASSERT_EQ(0, mvt);
  ASSERT_EQ(0, mvt2);
  ASSERT_EQ(1, mvt.getHistorySize());
  ASSERT_EQ(1, mvt2.getHistorySize());
  std::cout << "mvt: "; mvt.printHistoryTimedData();
  std::cout << "mvt: "; mvt.printReverseHistoryTimedData();
  std::cout << mvt << '\n';
  std::cout << "mvt2: "; mvt2.printHistoryTimedData();
  std::cout << "mvt2: "; mvt2.printReverseHistoryTimedData();
  std::cout << mvt2 << '\n';
}

TEST(memVarTimedTest, timeTaggedTest_4)
{
  mv::memvarTimed<int> mvt {33,9};
  ASSERT_EQ(9, mvt.getHistoryCapacity());

  ASSERT_EQ(33, mvt);
  ASSERT_TRUE( 33 == mvt );

  mvt = 78;
  mvt.printHistoryTimedData();

  ASSERT_EQ(78, mvt);
  ASSERT_TRUE( 78 == mvt );

  mvt = 45;
  mvt.printHistoryTimedData();

  ASSERT_EQ(45, mvt);
  ASSERT_TRUE( 45 == mvt );

  ++mvt;
  mvt.printHistoryTimedData();

  ASSERT_EQ(46, mvt);
  ASSERT_TRUE( 46 == mvt );

  mvt = ++mvt + mvt;
  mvt.printHistoryTimedData();

  ASSERT_EQ(94, mvt);
  ASSERT_TRUE( 94 == mvt );

  mvt++;
  mvt.printHistoryTimedData();

  ASSERT_EQ(95, mvt);
  ASSERT_TRUE( 95 == mvt );

  mvt = ++mvt + mvt + mvt++;
  mvt.printHistoryTimedData();

  ASSERT_EQ(288, mvt);
  ASSERT_TRUE( 288 == mvt );

  mvt--;
  mvt.printHistoryTimedData();

  ASSERT_EQ(287, mvt);
  ASSERT_TRUE( 287 == mvt );

  mvt = --mvt - mvt - mvt--;

  ASSERT_EQ(-286, mvt);
  ASSERT_TRUE( -286 == mvt );

  mvt.printHistoryTimedData();

  auto [min, max] = mvt.getHistoryMinMax();
  ASSERT_EQ(-286, min);
  ASSERT_EQ(288, max);
}

TEST(memVarTimedTest, timeTaggedTest_5)
{
  using memvarType = int64_t;
  // you need enough memory to run this test
  // if not, swap will be used if swap is on
  constexpr mv::memvar<memvarType>::capacityType historyCapacity {1'000};
  mv::memvarTimed<memvarType> mvt {0,historyCapacity};
  ASSERT_EQ(historyCapacity, mvt.getHistoryCapacity());

  auto func = [&mvt] ()
  {
    memvarType c {0};
    while ( !mvt.isHistoryFull() )
    {
      mvt = ++c;
    }
  };
  auto t = perftimer<>::duration(func).count();
  std::cout << "loop took: " << t << " nsec" << '\n';

  ASSERT_EQ(mvt.getHistoryCapacity(), mvt.getHistorySize());
  ASSERT_EQ(historyCapacity - 1, mvt);

  auto clearHistory = [&mvt] ()
  {
    mvt.clearHistory();
  };
  t = perftimer<>::duration(clearHistory).count();
  std::cout << "history clearing took: " << t << " nsec" << '\n';
  ASSERT_EQ(0, mvt());
  ASSERT_EQ(historyCapacity, mvt.getHistoryCapacity());
  ASSERT_EQ(1, mvt.getHistorySize());
  mvt.printHistoryTimedData();
}

TEST(memVarTimedTest, timeTaggedTest_6)
{
  using memvarType = int64_t;
  constexpr mv::memvar<memvarType>::capacityType historyCapacity {100};
  constexpr memvarType maxValue {1'000};
  mv::memvarTimed<memvarType> mvt {0,historyCapacity};
  ASSERT_EQ(historyCapacity, mvt.getHistoryCapacity());

  auto func = [&mvt] ()
  {
    memvarType c {0};
    while ( c < maxValue )
    {
      mvt = ++c;
    }
  };
  auto t = perftimer<>::duration(func).count();
  std::cout << "loop took: " << t << " nsec" << '\n';

  ASSERT_EQ(maxValue, mvt);
  ASSERT_EQ(historyCapacity, mvt.getHistorySize());

  auto clearHistory = [&mvt] ()
  {
    mvt.clearHistory();
  };
  t = perftimer<>::duration(clearHistory).count();
  std::cout << "history clearing took: " << t << " nsec" << '\n';
  ASSERT_EQ(historyCapacity, mvt.getHistoryCapacity());
  ASSERT_EQ(1, mvt.getHistorySize());
  ASSERT_EQ(0, mvt);
  mvt.printHistoryTimedData();
}

TEST(memVarTimedTest, timeTaggedTest_7)
{
  mv::memvarTimed<std::string> mvts {"A"};
  std::string s {"B"};
  ASSERT_EQ("A", mvts());
  mvts = s;
  ASSERT_EQ("B", mvts());
  mvts = mvts + s;
  ASSERT_EQ("BB", mvts());
  ASSERT_TRUE( "BB" == mvts() );

  std::string s2 = mvts + mvts + s + mvts + s;

  ASSERT_TRUE( "BB" == mvts() );
  ASSERT_EQ("BBBBBBBB", s2);
  ASSERT_EQ(3, mvts.getHistorySize());
  mvts.printHistoryTimedData();
}

TEST(memVarTimedTest, timeTaggedTest_8)
{
  using memvarType = char;
  mv::memvarTimed<memvarType> mvtc {'A'};
  memvarType c = mvtc + 1;
  mvtc = c;  // B
  mvtc++;  // C
  mvtc++;  // D
  mvtc--;  // C
  // [ C D C B A  ]
  mvtc.printHistoryTimedData();

  ASSERT_EQ('C', mvtc);
  ASSERT_EQ(5, mvtc.getHistorySize());

  mv::memvarTimed<memvarType>::historyTimedValue hv {};

  hv = mvtc.getHistoryTimedValue(1);
  ASSERT_EQ('D', std::get<memvarType>(hv));
  ASSERT_EQ(false, std::get<bool>(hv));

  hv = mvtc.getHistoryTimedValue(2);
  ASSERT_EQ('C', std::get<memvarType>(hv));
  ASSERT_EQ(false, std::get<bool>(hv));

  memvarType value {};
  std::chrono::nanoseconds timeTag {};
  bool error {};

  std::tie(value, timeTag, error) = mvtc.getHistoryTimedValue(3);
  ASSERT_EQ('B', value);
  ASSERT_EQ(false, error);

  std::tie(value, timeTag, error) = mvtc.getHistoryTimedValue(4);
  ASSERT_EQ('A', value);
  ASSERT_EQ(false, error);

  // trying to access the history out of bound
  EXPECT_THROW(mvtc.getHistoryTimedValue(5), std::out_of_range);

  // trying to access the history out of bound
  EXPECT_THROW(mvtc.getHistoryTimedValue(100000), std::out_of_range);

  // trying to access the history out of bound
  EXPECT_THROW(std::tie(value, timeTag, error) = mvtc.getHistoryTimedValue(-10), std::out_of_range);

  auto [min, max] = mvtc.getHistoryMinMax();
  ASSERT_EQ('A', min);
  ASSERT_EQ('D', max);
}

TEST(memVarTimedTest, timeTaggedTest_9)
{
  using memvarType = int64_t;
  mv::memvarTimed<memvarType> mvt1{10};
  mv::memvarTimed<memvarType> mvt2{0};

  mvt2 = mvt1;
  ASSERT_TRUE(mvt1 == mvt2);
  ASSERT_FALSE(mvt1 != mvt2);
  ASSERT_FALSE(mvt1 > mvt2);
  ASSERT_FALSE(mvt1 < mvt2);
  ASSERT_TRUE(mvt1 >= mvt2);
  ASSERT_TRUE(mvt1 <= mvt2);

  // mvt1 == 10
  mvt2 = 20;
  ASSERT_FALSE(mvt1 == mvt2);
  ASSERT_TRUE(mvt1 != mvt2);
  ASSERT_FALSE(mvt1 > mvt2);
  ASSERT_TRUE(mvt1 < mvt2);
  ASSERT_FALSE(mvt1 >= mvt2);
  ASSERT_TRUE(mvt1 <= mvt2);

  mvt1 = 30;
  // mvt2 = 20
  ASSERT_FALSE(mvt1 == mvt2);
  ASSERT_TRUE(mvt1 != mvt2);
  ASSERT_TRUE(mvt1 > mvt2);
  ASSERT_FALSE(mvt1 < mvt2);
  ASSERT_TRUE(mvt1 >= mvt2);
  ASSERT_FALSE(mvt1 <= mvt2);

  mvt1 = 100;
  mvt1 += 200;
  ASSERT_EQ(300, mvt1);

  //mvt1 == 300, mvt2 == 20
  mvt2 += mvt1;
  ASSERT_EQ(320, mvt2);

  mvt1 -= mvt1;
  ASSERT_EQ(0, mvt1);

  mvt2 /= mvt2;
  ASSERT_EQ(1, mvt2);

  mvt1 = 8;
  mvt1 *= mvt1;
  ASSERT_EQ(64, mvt1);

  mvt1 /= 8;
  ASSERT_EQ(8, mvt1);

  std::cout << "mvt1: "; mvt1.printHistoryTimedData();
  std::cout << "mvt2: "; mvt2.printHistoryTimedData();
}

TEST(memVarTimedTest, timeTaggedTest_10)
{
  mv::memvarTimed<int> mvt{0,20};
  ASSERT_EQ(0, mvt);
  ASSERT_EQ(20, mvt.getHistoryCapacity());
  std::cout << mvt << '\n';
  for (int i = 1; i <= 10; ++i)
  {
    mvt = i;
  }
  ASSERT_EQ(20, mvt.getHistoryCapacity());
  ASSERT_EQ(10, mvt);

  std::cout << "mvt: "; mvt.printHistoryTimedData();
  std::cout << "mvt: " << mvt << '\n';
  auto tpl = mvt.getHistoryTimedValue(0);
  ASSERT_EQ(std::get<0>(tpl), 10);
  std::cout << "mvt(0): " << std::get<0>(tpl) << " " << std::get<1>(tpl) << " " << std::get<2>(tpl) << "\n";
  tpl = mvt.getHistoryTimedValue(3);
  std::cout << "mvt(3): " << std::get<0>(tpl) << " " << std::get<1>(tpl) << " " << std::get<2>(tpl) << "\n";
  ASSERT_EQ(std::get<0>(tpl), 7);
}

// Yet another way to compute the Fibonacci numbers
TEST(memVarTimedTest, fibonacciNumbers)
{
  constexpr auto maxFibNumberToCompute {93};
  using memvarType = uint64_t;
  // store fib(0)
  mv::memvarTimed<memvarType> fibs{0, maxFibNumberToCompute + 1};
  // store fib(1)
  fibs = 1;
  // compute and store fib(2) = fib(1) + fib(0) through fib(93) = fib(92) + fib(91)
  // fib(93) = 12200160415121876738 = 7540113804746346429 + 4660046610375530309
  // cannot compute more because of int overflow
  const memvarType fib93 = (static_cast<memvarType>(7'540'113'804'746'346'429) +
                            static_cast<memvarType>(4'660'046'610'375'530'309));
  for(int i = 1; i < maxFibNumberToCompute; ++i)
  {
    fibs += getHistoryValue(fibs, 1);
    ASSERT_GE(fibs, getHistoryValue(fibs, 1));
  }
  ASSERT_EQ(maxFibNumberToCompute + 1, fibs.getHistorySize());
  ASSERT_EQ(fib93, fibs);

  // print the first 94 Fibonacci numbers
  std::cout << "fibs: ";
  fibs.printHistoryTimedData();
}

// Yet another way to compute the Fibonacci numbers with bigint's
TEST(memVarTimedTest, fibonacciBigInts)
{
  constexpr auto maxFibNumberToCompute {2'000};
  using memvarType = bip::bigint;
  // store fib(0)
  mv::memvarTimed<memvarType> fibs{memvarType(0), maxFibNumberToCompute + 1};
  // store fib(1)
  fibs = memvarType(1);
  // compute and store fib(2) = fib(1) + fib(0) through
  // fib(2000) = fib(1999) + fib(1998)
  for(int i = 1; i < maxFibNumberToCompute; ++i)
  {
    fibs += fibs(1);
    ASSERT_GE(fibs, fibs(1));
  }
  bip::bigint fib_2000("4224696333392304878706725602341482782579852840250681098010280137314308584370130707224123599639141511088446087538909603607640194711643596029271983312598737326253555802606991585915229492453904998722256795316982874482472992263901833716778060607011615497886719879858311468870876264597369086722884023654422295243347964480139515349562972087652656069529806499841977448720155612802665404554171717881930324025204312082516817125");
  ASSERT_EQ(fib_2000, fibs);

  // print the first 2000 Fibonacci numbers
  std::cout << "fibs: ";
  fibs.printReverseHistoryTimedData(std::cout, " ");
}

TEST(memVarTest, crefTest)
{
  using memvarType = int64_t;
  mv::memvar<memvarType> mv{0,10};

  mv = 123;
  memvarType xassign1 = mv;
  memvarType xassign2 = mv();
  auto xcref = mv.cref();

  std::cout << "before: mv       = " << mv << " : ";
  mv.printHistoryData();
  std::cout << "before: xassign1 = " << xassign1 << "\n";
  std::cout << "before: xassign2 = " << xassign2 << "\n";
  std::cout << "before: xcref    = " << xcref << "\n";
  std::cout << "before: xcref+1  = " << xcref + 1 << "\n";
//  std::cout << "before: *xcref   = " << *xcref << "\n";      // Compilation ERROR when MEMVAR_SAFEREF_CHECK == MEMVAR_SAFEREF_NO_CHECK
//  std::cout << "before: *xcref+1 = " << *xcref + 1 << "\n";  // Compilation ERROR when MEMVAR_SAFEREF_CHECK == MEMVAR_SAFEREF_NO_CHECK

  auto xcrefCopy = std::move(xcref);
  for (memvarType i = 1; i <= 10; ++i) {
    mv = i;
    std::cout << "i: " << i << " - mv: ";
    mv.printHistoryData();
  }

  std::cout << "\nafter: mv            = " << mv << " : ";
  mv.printHistoryData();
  std::cout << "after: xassign1      = " << xassign1 << "\n";
  std::cout << "after: xassign2      = " << xassign2 << "\n";

#if MEMVAR_SAFEREF_CHECK == MEMVAR_SAFEREF_THROWS
  EXPECT_THROW(*xcref,     std::logic_error);
  EXPECT_THROW(*xcref + 1, std::logic_error);
  std::cout << "...throwing\n";
#endif
#if MEMVAR_SAFEREF_CHECK == MEMVAR_SAFEREF_ABORT
  EXPECT_DEATH(*xcref,     "the referenced value has been evicted from history");
  EXPECT_DEATH(*xcref + 1, "the referenced value has been evicted from history");
  std::cout << "...aborting\n";
#endif
#if MEMVAR_SAFEREF_CHECK == MEMVAR_SAFEREF_NO_CHECK
  std::cout << "...not checking\n";
  EXPECT_EQ(xcrefCopy, 123);
  EXPECT_EQ(xcrefCopy + 1, 124);
  std::cout << "after: xcrefCopy     = " << xcrefCopy << "\n";
  std::cout << "after: xcrefCopy + 1 = " << xcrefCopy + 1 << "\n";
  // xcref is a dangling reference now;
  // the referenced item by xcref is invalidated since it was popped from the deque;
  // at the risk of UB when uncommented
  //EXPECT_EQ(xcref, 123);  // UB
  //EXPECT_EQ(xcref + 1, 124);  // UB
  //std::cout << "after: xcref     = " << xcref << "\n";  // UB
  //std::cout << "after: xcref + 1 = " << xcref + 1 << "\n";  // UB
#endif
  EXPECT_EQ(xassign1, xassign2);
  EXPECT_EQ(xassign2, 123);
}

TEST(memVarTest, test_0)
{
  using memvarType = int64_t;
  mv::memvar<memvarType> mv{0, 10};

  mv = 1;
  ASSERT_EQ(mv[0], 1);
  ASSERT_EQ(mv[1], 0);

  mv = 2;
  ASSERT_EQ(mv[0], 2);
  ASSERT_EQ(mv[1], 1);

  mv = 3;
  ASSERT_EQ(mv[0], 3);
  ASSERT_EQ(mv[1], 2);

  mv = 4;
  ASSERT_EQ(mv[0], 4);
  ASSERT_EQ(mv[1], 3);

  mv = 5;
  ASSERT_EQ(mv[0], 5);
  ASSERT_EQ(mv[1], 4);

  mv = 6;
  ASSERT_EQ(mv[0], 6);
  ASSERT_EQ(mv[1], 5);

  mv = 7;
  ASSERT_EQ(mv[0], 7);
  ASSERT_EQ(mv[1], 6);

  mv = 8;
  ASSERT_EQ(mv[0], 8);
  ASSERT_EQ(mv[1], 7);

  mv = 9;
  ASSERT_EQ(mv[1], 8);
  ASSERT_EQ(mv[0], 9);
  ASSERT_EQ(mv[0], mv(0));

  ASSERT_EQ(mv[1], 8);
  ASSERT_EQ(mv[1], mv(1));

  ASSERT_EQ(mv[2], 7);
  ASSERT_EQ(mv[2], mv(2));

  ASSERT_EQ(mv[3], 6);
  ASSERT_EQ(mv[3], mv(3));

  ASSERT_EQ(mv[4], 5);
  ASSERT_EQ(mv[4], mv(4));

  ASSERT_EQ(mv[5], 4);
  ASSERT_EQ(mv[5], mv(5));

  ASSERT_EQ(mv[6], 3);
  ASSERT_EQ(mv[6], mv(6));

  ASSERT_EQ(mv[7], 2);
  ASSERT_EQ(mv[7], mv(7));

  ASSERT_EQ(mv[8], 1);
  ASSERT_EQ(mv[8], mv(8));

  ASSERT_EQ(mv[9], 0);
  ASSERT_EQ(mv[9], mv(9));
}

TEST(memVarTest, test_1)
{
  EXPECT_THROW(mv::memvar<int> mvi(0,0), std::invalid_argument);
  EXPECT_THROW(mv::memvar<int> mvi(0,-10), std::invalid_argument);
  EXPECT_NO_THROW(mv::memvar<int> mvi {});
  EXPECT_NO_THROW(mv::memvar<int> mvi(11, 20));

  EXPECT_THROW(mv::memvar<double> mvd(0.0,0), std::invalid_argument);
  EXPECT_THROW(mv::memvar<double> mvd(0.0,-10), std::invalid_argument);
  EXPECT_NO_THROW(mv::memvar<double> mvd {});
  EXPECT_NO_THROW(mv::memvar<double> mvd(11.2345, 20));

  EXPECT_THROW(mv::memvar<std::string> mvs("Hello World!",0), std::invalid_argument);
  EXPECT_THROW(mv::memvar<std::string> mvs("Hello World!",-10), std::invalid_argument);
  EXPECT_NO_THROW(mv::memvar<std::string> mvs {});
  EXPECT_NO_THROW(mv::memvar<std::string> mvs("Hello World!", 20));

  EXPECT_THROW(mv::memvar<bip::bigint> mvbi(bip::bigint("1234567890"),0), std::invalid_argument);
  EXPECT_THROW(mv::memvar<bip::bigint> mvbi(bip::bigint("1234567890"),-10), std::invalid_argument);
  EXPECT_NO_THROW(mv::memvar<bip::bigint> mvbi {});
  EXPECT_NO_THROW(mv::memvar<bip::bigint> mvbi(bip::bigint("1234567890"), 20));
}

TEST(memVarTest, test_2)
{
  mv::memvar<int> mv{0,2};
  ASSERT_EQ(0, mv);
  ASSERT_EQ(2, mv.getHistoryCapacity());
  for (int i = 1; i <= 1'000; ++i)
  {
    mv = i;
  }
  ASSERT_EQ(2, mv.getHistoryCapacity());
  ASSERT_EQ(1'000, mv);

  auto [min, max] = mv.getHistoryMinMax();
  ASSERT_EQ(999, min);
  ASSERT_EQ(1'000, max);
}

TEST(memVarTest, test_3)
{
  mv::memvar<int> mv {};
  ASSERT_EQ(10, mv.getHistoryCapacity());
  mv.printHistoryData();
  ASSERT_EQ(int {}, mv);

  int v = mv;
  ASSERT_EQ(int {}, v);
  ASSERT_EQ(0, v);

  v = 123;
  ASSERT_NE(int {}, v);
  ASSERT_NE(v, mv);

  int w = mv;
  ASSERT_EQ(int {}, w);
  ASSERT_EQ(0, w);

  int j;
  mv++;
  j = mv + 20 + mv;
  ASSERT_EQ(22, j);
}

TEST(memVarTest, test_4)
{
  mv::memvar<int> mv {55};
  mv.printHistoryData();
  ASSERT_EQ(55, mv);
  ASSERT_TRUE( 55 == mv );

  int v = mv;
  ASSERT_EQ(55, v);
  ASSERT_TRUE( v == mv );

  v = 123;
  ASSERT_NE(55, v);
  ASSERT_NE(v, mv);
  ASSERT_TRUE( v != mv );

  int w = mv;
  ASSERT_EQ(55, w);

  mv += 45;
  ASSERT_EQ(100, mv);
  ASSERT_TRUE( 100 == mv );

  mv *= 3;
  ASSERT_TRUE( 300 == mv );
  ASSERT_EQ(300, mv);
  ASSERT_EQ(100, mv(1));
  ASSERT_EQ(55, mv(2));
}

TEST(memVarTest, test_5)
{
  mv::memvar<int> mv {33,9};
  ASSERT_EQ(9, mv.getHistoryCapacity());

  ASSERT_EQ(33, mv);
  ASSERT_TRUE( 33 == mv );

  mv = 78;
  mv.printHistoryData();

  ASSERT_EQ(78, mv);
  ASSERT_TRUE( 78 == mv );

  mv = 45;
  mv.printHistoryData();

  ASSERT_EQ(45, mv);
  ASSERT_TRUE( 45 == mv );

  ++mv;
  mv.printHistoryData();

  ASSERT_EQ(46, mv);
  ASSERT_TRUE( 46 == mv );

  mv = ++mv + mv;
  mv.printHistoryData();

  ASSERT_EQ(94, mv);
  ASSERT_TRUE( 94 == mv );

  mv++;
  mv.printHistoryData();

  ASSERT_EQ(95, mv);
  ASSERT_TRUE( 95 == mv );

  mv = ++mv + mv + mv++;
  mv.printHistoryData();

  ASSERT_EQ(288, mv);
  ASSERT_TRUE( 288 == mv );

  mv--;
  mv.printHistoryData();

  ASSERT_EQ(287, mv);
  ASSERT_TRUE( 287 == mv );

  mv = --mv - mv - mv--;

  ASSERT_EQ(-286, mv);
  ASSERT_TRUE( -286 == mv );

  mv.printHistoryData();

  auto [min, max] = mv.getHistoryMinMax();
  ASSERT_EQ(-286, min);
  ASSERT_EQ(288, max);
}

TEST(memVarTest, test_6)
{
  using memvarType = int64_t;
  // you need enough memory to run this test
  // if not, swap will be used if swap is on
  constexpr mv::memvar<memvarType>::capacityType historyCapacity {1'000'000'000};
  mv::memvar<memvarType> mv {0,historyCapacity};
  ASSERT_EQ(historyCapacity, mv.getHistoryCapacity());

  auto func = [&mv] ()
  {
    memvarType c {0};
    while ( !mv.isHistoryFull() )
    {
      mv = ++c;
    }
  };
  auto t = perftimer<std::chrono::milliseconds>::duration(func).count();
  std::cout << "loop took: " << t << " msec" << '\n';

  ASSERT_EQ(mv.getHistoryCapacity(), mv.getHistorySize());
  ASSERT_EQ(historyCapacity - 1, mv);

  auto clearHistory = [&mv] ()
  {
    mv.clearHistory();
  };
  t = perftimer<std::chrono::milliseconds>::duration(clearHistory).count();
  std::cout << "history clearing took: " << t << " msec" << '\n';
  ASSERT_EQ(0, mv());
  ASSERT_EQ(historyCapacity, mv.getHistoryCapacity());
  ASSERT_EQ(1, mv.getHistorySize());
  mv.printHistoryData();
}

TEST(memVarTest, test_7)
{
  using memvarType = int64_t;
  constexpr mv::memvar<memvarType>::capacityType historyCapacity {100};
  constexpr memvarType maxValue {1'000'000'000};
  mv::memvar<memvarType> mv {0,historyCapacity};
  ASSERT_EQ(historyCapacity, mv.getHistoryCapacity());

  auto func = [&mv] ()
  {
    memvarType c {0};
    while ( c < maxValue )
    {
      mv = ++c;
    }
  };
  auto t = perftimer<std::chrono::milliseconds>::duration(func).count();
  std::cout << "loop took: " << t << " msec" << '\n';

  ASSERT_EQ(maxValue, mv);
  ASSERT_EQ(historyCapacity, mv.getHistorySize());

  auto clearHistory = [&mv] ()
  {
    mv.clearHistory();
  };
  t = perftimer<>::duration(clearHistory).count();
  std::cout << "history clearing took: " << t << " nsec" << '\n';
  ASSERT_EQ(historyCapacity, mv.getHistoryCapacity());
  ASSERT_EQ(1, mv.getHistorySize());
  ASSERT_EQ(0, mv);
  mv.printHistoryData();
}

TEST(memVarTest, test_8)
{
  mv::memvar<std::string> mvs {"A"};
  std::string s {"B"};
  ASSERT_EQ("A", mvs());
  mvs = s;
  ASSERT_EQ("B", mvs());
  mvs = mvs + s;
  ASSERT_EQ("BB", mvs());
  ASSERT_EQ(mvs(), mvs());
  ASSERT_TRUE( "BB" == mvs() );
  ASSERT_TRUE( mvs() == mvs() );

  std::string s2 = mvs + mvs + s + mvs + s;

  ASSERT_TRUE( "BB" == mvs() );
  ASSERT_EQ("BBBBBBBB", s2);
  ASSERT_EQ(3, mvs.getHistorySize());
  mvs.printHistoryData();
}

TEST(memVarTest, test_9)
{
  using memvarType = char;
  mv::memvar<memvarType> mvc {'A'};
  memvarType c = mvc + 1;
  mvc = c;  // B
  mvc++;  // C
  mvc++;  // D
  mvc--;  // C
  // [ C D C B A  ]
  mvc.printHistoryData();

  ASSERT_EQ('C', mvc);
  ASSERT_EQ(5, mvc.getHistorySize());

  mv::memvar<memvarType>::historyValue hv {};

  hv = mvc.getHistoryValue(1);
  ASSERT_EQ('D', std::get<memvarType>(hv));
  ASSERT_EQ(false, std::get<bool>(hv));

  hv = mvc.getHistoryValue(2);
  ASSERT_EQ('C', std::get<memvarType>(hv));
  ASSERT_EQ(false, std::get<bool>(hv));

  memvarType value {};
  bool error {};

  std::tie(value, error) = mvc.getHistoryValue(3);
  ASSERT_EQ('B', value);
  ASSERT_EQ(false, error);

  std::tie(value, error) = mvc.getHistoryValue(4);
  ASSERT_EQ('A', value);
  ASSERT_EQ(false, error);

  // trying to access the history out of bound
  EXPECT_THROW(std::tie(value, error) = mvc.getHistoryValue(5), std::out_of_range);

  // trying to access the history out of bound
  EXPECT_THROW(mvc.getHistoryValue(100000), std::out_of_range);

  // trying to access the history out of bound
  EXPECT_THROW(std::tie(value, error) = mvc.getHistoryValue(-10), std::out_of_range);

  auto [min, max] = mvc.getHistoryMinMax();
  ASSERT_EQ('A', min);
  ASSERT_EQ('D', max);
}

TEST(memVarTest, test_10)
{
  using memvarType = int64_t;
  mv::memvar<memvarType> mv1{10};
  mv::memvar<memvarType> mv2{0};

  mv2 = mv1;
  ASSERT_TRUE(mv1 == mv2);
  ASSERT_FALSE(mv1 != mv2);
  ASSERT_FALSE(mv1 > mv2);
  ASSERT_FALSE(mv1 < mv2);
  ASSERT_TRUE(mv1 >= mv2);
  ASSERT_TRUE(mv1 <= mv2);

  // mv1 == 10
  mv2 = 20;
  ASSERT_FALSE(mv1 == mv2);
  ASSERT_TRUE(mv1 != mv2);
  ASSERT_FALSE(mv1 > mv2);
  ASSERT_TRUE(mv1 < mv2);
  ASSERT_FALSE(mv1 >= mv2);
  ASSERT_TRUE(mv1 <= mv2);

  mv1 = 30;
  // mv2 = 20
  ASSERT_FALSE(mv1 == mv2);
  ASSERT_TRUE(mv1 != mv2);
  ASSERT_TRUE(mv1 > mv2);
  ASSERT_FALSE(mv1 < mv2);
  ASSERT_TRUE(mv1 >= mv2);
  ASSERT_FALSE(mv1 <= mv2);

  mv1 = 100;
  mv1 += 200;
  ASSERT_EQ(300, mv1);

  //mv1 == 300, mv2 == 20
  mv2 += mv1;
  ASSERT_EQ(320, mv2);

  mv1 -= mv1;
  ASSERT_EQ(0, mv1);

  mv2 /= mv2;
  ASSERT_EQ(1, mv2);

  mv1 = 8;
  mv1 *= mv1;
  ASSERT_EQ(64, mv1);

  mv1 /= 8;
  ASSERT_EQ(8, mv1);

  std::cout << "mv1: "; mv1.printHistoryData();
  std::cout << "mv2: "; mv2.printHistoryData();
}

// Yet another way to compute the Fibonacci numbers
TEST(memVarTest, fibonacciNumbers)
{
  constexpr auto maxFibNumberToCompute {93};
  using memvarType = uint64_t;
  // store fib(0)
  mv::memvar<memvarType> fibs{0, maxFibNumberToCompute + 1};
  // store fib(1)
  fibs = 1;
  // compute and store fib(2) = fib(1) + fib(0) through fib(93) = fib(92) + fib(91)
  // fib(93) = 12200160415121876738 = 7540113804746346429 + 4660046610375530309
  // cannot compute more because of int overflow
  const memvarType fib93 = (static_cast<memvarType>(7'540'113'804'746'346'429) +
                            static_cast<memvarType>(4'660'046'610'375'530'309));
  for(int i = 1; i < maxFibNumberToCompute; ++i)
  {
    fibs += fibs(1);
    ASSERT_GE(fibs, fibs(1));
  }
  ASSERT_EQ(maxFibNumberToCompute + 1, fibs.getHistorySize());
  ASSERT_EQ(fib93, fibs);

  // print the first 94 Fibonacci numbers
  std::cout << "fibs: ";
  fibs.printHistoryData();
}

// Yet another way to compute the Fibonacci numbers with bigint's
TEST(memVarTest, fibonacciBigInts)
{
  constexpr auto maxFibNumberToCompute {2'000};
  using memvarType = bip::bigint;
  // store fib(0)
  mv::memvar<memvarType> fibs{memvarType(0), maxFibNumberToCompute + 1};
  // store fib(1)
  fibs = memvarType(1);
  // compute and store fib(2) = fib(1) + fib(0) through
  // fib(2000) = fib(1999) + fib(1998)
  for(int i = 1; i < maxFibNumberToCompute; ++i)
  {
    fibs += fibs(1);
    ASSERT_GE(fibs, fibs(1));
  }
  bip::bigint fib_2000("4224696333392304878706725602341482782579852840250681098010280137314308584370130707224123599639141511088446087538909603607640194711643596029271983312598737326253555802606991585915229492453904998722256795316982874482472992263901833716778060607011615497886719879858311468870876264597369086722884023654422295243347964480139515349562972087652656069529806499841977448720155612802665404554171717881930324025204312082516817125");
  ASSERT_EQ(fib_2000, fibs);

  // print the first 2000 Fibonacci numbers
  std::cout << "fibs: ";
  fibs.printReverseHistoryData();
}

TEST(memVarTest, test_11)
{
  using memvarType = int64_t;
  mv::memvar<memvarType> mv{44, 44};

  auto l = [] (mv::memvar<memvarType>& mv_arg) -> void
          {
            mv_arg = 999;
            mv_arg = 888;
          };
  l(mv);
  mv = 11;
  mv = 22;

  ASSERT_EQ(22,  mv);
  ASSERT_EQ(11,  mv(1));
  ASSERT_EQ(888, mv(2));
  ASSERT_EQ(999, mv(3));
  ASSERT_EQ(44,  mv(4));

  std::cout << "mv: "; mv.printHistoryData(std::cerr, ",");
}

TEST(memVarTest, test_12)
{
  using memvarType = int64_t;
  auto mv_uptr  = std::make_unique<mv::memvar<memvarType>>(10);
  auto mv_shptr = std::make_shared<mv::memvar<memvarType>>(22);

  ASSERT_EQ(10, *mv_uptr);
  ASSERT_EQ(22, *mv_shptr);

  *mv_uptr  = 345;
  *mv_shptr = 7890;
  ASSERT_EQ(345,  *mv_uptr);
  ASSERT_EQ(7890, *mv_shptr);
  ASSERT_EQ(10, (*mv_uptr)(1));
  ASSERT_EQ(22, (*mv_shptr)(1));
}
////////////////////////////////////////////////////////////////////////////////
//
// Memvar Name Hiding Tests
//
// What's true:
//   - Per ordinary C++ member-name-hiding rules, memvarTimed's own
//     operator=(const T&)/(T&&)/(const memvarTimed&) DO hide the
//     base memvar<T>::operator=(const memvar<T>&) from the candidate
//     set (no `using memvar<T>::operator=;` is present).
//
// What's ALSO true, and what makes it a non-issue:
//   - memvar<T> has a non-explicit `operator T() const` conversion.
//     So `timed = plain_memvar;` still compiles: `plain_memvar`
//     undergoes a user-defined conversion to a prvalue T, which binds
//     to memvarTimed's own operator=(T&&) (preferred over
//     operator=(const T&) by the rvalue-reference-binding tie-break
//     rule). The result is functionally identical to what calling the
//     "hidden" overload directly would have produced.
//   - Same story for operator+=/-=/*=//=: memvarTimed's own
//     operator OP=(const T&) is viable via the same conversion, since
//     there's no derived-to-base-then-back conversion needed.
//
// These tests assert this  behavior, so future changes to memvar.h that
// accidentally break this path (e.g. making operator T() explicit) will be caught.
//
using mv::memvar;
using mv::memvarTimed;

namespace {

template <typename L, typename R>
concept HasPlusEquals = requires(L& l, const R& r) { l += r; };

template <typename L, typename R>
concept HasMinusEquals = requires(L& l, const R& r) { l -= r; };

template <typename L, typename R>
concept HasTimesEquals = requires(L& l, const R& r) { l *= r; };

template <typename L, typename R>
concept HasDivEquals = requires(L& l, const R& r) { l /= r; };

}  // namespace

// ---------------------------------------------------------------------
// operator=  --  assigning a bare memvar<T> into a memvarTimed<T>
//                compiles fine (via the operator T() conversion path).
// ---------------------------------------------------------------------

TEST(MemvarTimedAssignFromBareMemvar, IsWellFormed) {
  EXPECT_TRUE((std::is_assignable_v<memvar<int>&, const memvar<int>&>));       // baseline
  EXPECT_TRUE((std::is_assignable_v<memvarTimed<int>&, const memvar<int>&>));  // works
  EXPECT_TRUE((std::is_assignable_v<memvarTimed<int>&, const memvarTimed<int>&>));
  EXPECT_TRUE((std::is_assignable_v<memvarTimed<int>&, const int&>));
  EXPECT_TRUE((std::is_assignable_v<memvarTimed<int>&, int&&>));
}

TEST(MemvarTimedAssignFromBareMemvar, ProducesCorrectValue) {
  memvar<int> plain(42);
  memvarTimed<int> timed(0);
  timed = plain;
  EXPECT_EQ(timed(), 42);
}

TEST(MemvarTimedAssignFromBareMemvar, WorksForStringTooDespiteExtraCopy) {
  // For std::string/bigint T, this path goes through one extra copy
  // (base memvar -> T via operator T(), then T into the history) versus
  // what a direct (const memvar<T>&) overload would have done -- but
  // the resulting value is identical, just measurably less efficient
  // for large T. Not incorrect, just a minor performance note.
  memvar<std::string> plain(std::string("hello"));
  memvarTimed<std::string> timed(std::string(""));
  timed = plain;
  EXPECT_EQ(timed(), "hello");
}

// ---------------------------------------------------------------------
// operator+=  (valid for all MemVarType T, no requires restriction)
// ---------------------------------------------------------------------

TEST(MemvarTimedCompoundAssignFromBareMemvar, PlusEqualsIsWellFormed) {
  EXPECT_TRUE((HasPlusEquals<memvar<int>, memvar<int>>));
  EXPECT_TRUE((HasPlusEquals<memvarTimed<int>, memvar<int>>));
  EXPECT_TRUE((HasPlusEquals<memvarTimed<int>, memvarTimed<int>>));
}

TEST(MemvarTimedCompoundAssignFromBareMemvar, PlusEqualsProducesCorrectValue) {
  memvar<int> plain(10);
  memvarTimed<int> timed(5);
  timed += plain;
  EXPECT_EQ(timed(), 15);
}

// ---------------------------------------------------------------------
// operator-=, operator*=, operator/=
// (requires (!AnyStandardString<T>), so use int, not std::string)
// ---------------------------------------------------------------------

TEST(MemvarTimedCompoundAssignFromBareMemvar, MinusEqualsIsWellFormed) {
  EXPECT_TRUE((HasMinusEquals<memvar<int>, memvar<int>>));
  EXPECT_TRUE((HasMinusEquals<memvarTimed<int>, memvar<int>>));
  EXPECT_TRUE((HasMinusEquals<memvarTimed<int>, memvarTimed<int>>));
}

TEST(MemvarTimedCompoundAssignFromBareMemvar, TimesEqualsIsWellFormed) {
  EXPECT_TRUE((HasTimesEquals<memvar<int>, memvar<int>>));
  EXPECT_TRUE((HasTimesEquals<memvarTimed<int>, memvar<int>>));
  EXPECT_TRUE((HasTimesEquals<memvarTimed<int>, memvarTimed<int>>));
}

TEST(MemvarTimedCompoundAssignFromBareMemvar, DivEqualsIsWellFormed) {
  EXPECT_TRUE((HasDivEquals<memvar<int>, memvar<int>>));
  EXPECT_TRUE((HasDivEquals<memvarTimed<int>, memvar<int>>));
  EXPECT_TRUE((HasDivEquals<memvarTimed<int>, memvarTimed<int>>));
}

TEST(MemvarTimedCompoundAssignFromBareMemvar, ArithmeticProducesCorrectValues) {
  memvar<int> plain(3);
  memvarTimed<int> timed(20);

  timed -= plain;
  EXPECT_EQ(timed(), 17);

  timed *= plain;
  EXPECT_EQ(timed(), 51);

  timed /= plain;
  EXPECT_EQ(timed(), 17);
}

////////////////////////////////////////////////////////////////////////////////
// Failed writes (strong exception guarantee).
//
// A write that throws -- here std::bad_alloc while copying a large string into
// the history -- must leave a memvar / memvarTimed exactly as it was: same
// values, same time tags, and (when SafeRef checking is on) same write count.
// In the former two-deque memvarTimed the time tag was recorded before the
// value, so a failed write left the time history one entry longer than the
// value history, and every later value/tag pair was shifted.
//
// The failure is injected deterministically by replacing the global
// operator new with one that can be armed to fail a single large allocation
// (the 5001-byte string copy; deque nodes and everything gtest allocates are
// far smaller, and the hook is armed only around the one statement under test).
#include <cstdlib>
#include <new>

namespace failinj {
inline bool armed = false;
inline constexpr std::size_t threshold = 1000;
}  // namespace failinj

// The replacement pair is malloc/free based, which is valid; but at -O2/-O3 GCC
// inlines operator new/delete into callers (e.g. std::make_unique) and then
// reports -Wmismatched-new-delete because it sees free() applied to a pointer
// that came from operator new. The warning is a false positive for a matched
// replacement pair, so it is silenced for these definitions only.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmismatched-new-delete"
#endif
void* operator new(std::size_t n) {
  if (failinj::armed && n >= failinj::threshold) {
    failinj::armed = false;               // fail exactly once
    throw std::bad_alloc();
  }
  if (void* p = std::malloc(n ? n : 1)) return p;
  throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

namespace {

// Runs `write` with the next large allocation made to fail; returns true if it
// threw std::bad_alloc.
template <typename F>
bool writeThrowsBadAlloc(F&& write) {
  bool threw = false;
  failinj::armed = true;
  try {
    write();
  }
  catch (const std::bad_alloc&) {
    threw = true;
  }
  failinj::armed = false;
  return threw;
}

const std::string bigString(5000, 'x');   // built before any failure is armed

}  // namespace

// The hook really fires, so the other tests cannot pass by accident.
TEST(MemvarFailedWrite, InjectionWorks) {
  // guards the tests below against passing vacuously
  mv::memvar<std::string> m("first", 10);
  EXPECT_TRUE(writeThrowsBadAlloc([&] { m = bigString; }));
  EXPECT_FALSE(failinj::armed);
}

// A failed write leaves a plain memvar unchanged, and later writes work.
TEST(MemvarFailedWrite, PlainMemvarIsUnchanged) {
  mv::memvar<std::string> m("first", 10);
  m = "second";

  EXPECT_TRUE(writeThrowsBadAlloc([&] { m = bigString; }));

  EXPECT_EQ(m.getHistorySize(), 2);
  EXPECT_EQ(m(0), "second");
  EXPECT_EQ(m(1), "first");
  EXPECT_THROW((void)m(2), std::out_of_range);

  m = "third";
  EXPECT_EQ(m.getHistorySize(), 3);
  EXPECT_EQ(m(0), "third");
  EXPECT_EQ(m(1), "second");
  EXPECT_EQ(m(2), "first");
}

// A failed write on a full history keeps its oldest value.
TEST(MemvarFailedWrite, FullHistoryDoesNotEvictTheOldestValue) {
  mv::memvar<std::string> m("old", 2);
  m = "new";                                 // history is now full: [new, old]
  ASSERT_TRUE(m.isHistoryFull());

  EXPECT_TRUE(writeThrowsBadAlloc([&] { m = bigString; }));

  EXPECT_EQ(m.getHistorySize(), 2);
  EXPECT_EQ(m(0), "new");
  EXPECT_EQ(m(1), "old");                   // still there
}

// A failed += leaves the variable unchanged.
TEST(MemvarFailedWrite, CompoundAssignmentIsUnchanged) {
  mv::memvar<std::string> m("a", 4);
  EXPECT_TRUE(writeThrowsBadAlloc([&] { m += bigString; }));
  EXPECT_EQ(m.getHistorySize(), 1);
  EXPECT_EQ(m(), "a");
}

#if MEMVAR_SAFEREF_CHECK != MEMVAR_SAFEREF_NO_CHECK
// A failed write does not count towards expiring a SafeRef (modes 0 and 1 only).
TEST(MemvarFailedWrite, DoesNotAgeOutstandingSafeRefs) {
  mv::memvar<std::string> m("first", 2);    // handle expires at write #3
  auto h = m.cref();

  EXPECT_TRUE(writeThrowsBadAlloc([&] { m = bigString; }));   // must not count as a write

  m = "second";                             // write #2: the handle is still valid
  EXPECT_EQ(*h, "first");
}
#endif

// This is the bug itself. After a failed write the history size, value and tag
// are unchanged, and getTimeTag(1) throws. After the next good write each value
// still has its own tag.
TEST(MemvarTimedFailedWrite, HistoriesStayInStep) {
  mv::memvarTimed<std::string> m("first", 10);
  const auto tag0 = m.getTimeTag(0);

  ASSERT_EQ(m.getHistorySize(), 1);
  EXPECT_TRUE(writeThrowsBadAlloc([&] { m = bigString; }));

  // value history and time history both unchanged
  EXPECT_EQ(m.getHistorySize(), 1);
  EXPECT_EQ(m(), "first");
  EXPECT_EQ(m.getTimeTag(0), tag0);
  EXPECT_THROW((void)m.getTimeTag(1), std::out_of_range);   // used to succeed
  EXPECT_THROW((void)m.getHistoryTimedValue(1), std::out_of_range);

  // a later good write pairs each value with its own tag
  m = "second";
  EXPECT_EQ(m.getHistorySize(), 2);
  EXPECT_EQ(m(0), "second");
  EXPECT_EQ(m(1), "first");
  EXPECT_EQ(m.getTimeTag(1), tag0);          // tag of "first", not of the failed write
  EXPECT_GE(m.getTimeTag(0), m.getTimeTag(1));
  EXPECT_THROW((void)m.getTimeTag(2), std::out_of_range);

  const auto [v1, t1, e1] = m.getHistoryTimedValue(1);
  EXPECT_EQ(v1, "first");
  EXPECT_EQ(t1, tag0);
  EXPECT_FALSE(e1);
}

// On a full history a failed write changes nothing. Later eviction removes a
// value and its tag together.
TEST(MemvarTimedFailedWrite, FullHistoryKeepsValuesAndTagsTogether) {
  mv::memvarTimed<std::string> m("a", 3);
  m = "b";
  m = "c";                                   // full: [c, b, a]
  ASSERT_TRUE(m.isHistoryFull());
  const auto tagC = m.getTimeTag(0);
  const auto tagB = m.getTimeTag(1);
  const auto tagA = m.getTimeTag(2);

  EXPECT_TRUE(writeThrowsBadAlloc([&] { m = bigString; }));

  ASSERT_EQ(m.getHistorySize(), 3);
  EXPECT_EQ(m(0), "c");  EXPECT_EQ(m.getTimeTag(0), tagC);
  EXPECT_EQ(m(1), "b");  EXPECT_EQ(m.getTimeTag(1), tagB);
  EXPECT_EQ(m(2), "a");  EXPECT_EQ(m.getTimeTag(2), tagA);

  // and eviction afterwards still removes value and tag together
  m = "d";
  ASSERT_EQ(m.getHistorySize(), 3);
  EXPECT_EQ(m(0), "d");
  EXPECT_EQ(m(1), "c");  EXPECT_EQ(m.getTimeTag(1), tagC);
  EXPECT_EQ(m(2), "b");  EXPECT_EQ(m.getTimeTag(2), tagB);
  EXPECT_THROW((void)m.getTimeTag(3), std::out_of_range);
}

// After clearHistory(), tag 0 is exactly zero and a failed write leaves it unchanged.
TEST(MemvarTimedFailedWrite, AfterClearHistory) {
  mv::memvarTimed<std::string> m("a", 4);
  m = "b";
  m.clearHistory();
  ASSERT_EQ(m.getHistorySize(), 1);
  EXPECT_EQ(m.getTimeTag(0), std::chrono::nanoseconds::zero());   // the new epoch

  EXPECT_TRUE(writeThrowsBadAlloc([&] { m = bigString; }));

  EXPECT_EQ(m.getHistorySize(), 1);
  EXPECT_EQ(m(), "");
  EXPECT_EQ(m.getTimeTag(0), std::chrono::nanoseconds::zero());
  EXPECT_THROW((void)m.getTimeTag(1), std::out_of_range);
}

// Without any failure: values and tags are stored and evicted as pairs, and
// the two kinds of accessor agree for every index.
// With no failure, getHistoryTimedValue(i) agrees with valueAt(i) and getTimeTag(i)
// for every index, and tags decrease with age.
TEST(MemvarTimedEntries, ValueAndTagAreAlwaysPaired) {
  mv::memvarTimed<int, std::chrono::microseconds> m(0, 4);
  for (int i = 1; i <= 9; ++i) {
    m = i;
    std::this_thread::sleep_for(std::chrono::microseconds(200));
  }

  ASSERT_EQ(m.getHistorySize(), 4);
  for (mv::memvarTimed<int, std::chrono::microseconds>::capacityType i = 0; i < 4; ++i) {
    const auto [v, t, e] = m.getHistoryTimedValue(i);
    EXPECT_EQ(v, 9 - static_cast<int>(i));
    EXPECT_EQ(v, m.valueAt(i));
    EXPECT_EQ(t, m.getTimeTag(i));
    EXPECT_FALSE(e);
    if (i > 0) {
      EXPECT_GT(m.getTimeTag(i - 1), m.getTimeTag(i));   // newer tag is larger
    }
  }
  EXPECT_THROW((void)m.getTimeTag(4), std::out_of_range);
}

// 	Negative and too-large indices to getTimeTag and getHistoryTimedValue throw
// std::out_of_range.
TEST(MemvarTimedEntries, TimeTagIndexIsCheckedLikeValueIndex) {
  mv::memvarTimed<int> m(1, 3);
  EXPECT_THROW((void)m.getTimeTag(-1), std::out_of_range);
  EXPECT_THROW((void)m.getTimeTag(1), std::out_of_range);
  EXPECT_THROW((void)m.getHistoryTimedValue(-1), std::out_of_range);
  EXPECT_NO_THROW((void)m.getTimeTag(0));
}
