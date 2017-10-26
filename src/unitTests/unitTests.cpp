/* 
 * File:   unitTests.cpp
 * Author: massimo
 *
 * Created on October 04, 2017, 10:43 AM
 */

#include "bigint.h"
#include "../memvar.h"
#include <chrono>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace ::testing;
using namespace std::string_literals;
////////////////////////////////////////////////////////////////////////////////
// a simple function's performance timer
template <typename Time = std::chrono::nanoseconds,
          typename Clock = std::chrono::high_resolution_clock>
struct perftimer
{
  template <typename F, typename... Args>
  static Time duration(F&& f, Args... args)
  {
    auto start = Clock::now();
    // C++17: not yet available
    //std::invoke(std::forward<F>(f), std::forward<Args>(args)...);
    f(std::forward<Args>(args)...);
    auto end = Clock::now();

    return std::chrono::duration_cast<Time>(end - start);
  }
};

// BEGIN: ignore the warnings listed below when compiled with clang from here
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#pragma clang diagnostic ignored "-Wglobal-constructors"
////////////////////////////////////////////////////////////////////////////////
TEST(memVarTest, test_1)
{
  EXPECT_THROW(memvar::memvar<int> mvi(0,0), std::invalid_argument);
  EXPECT_THROW(memvar::memvar<int> mvi(0,-10), std::invalid_argument);
  EXPECT_NO_THROW(memvar::memvar<int> mvi {});
  EXPECT_NO_THROW(memvar::memvar<int> mvi(11, 20));

  EXPECT_THROW(memvar::memvar<double> mvd(0.0,0), std::invalid_argument);
  EXPECT_THROW(memvar::memvar<double> mvd(0.0,-10), std::invalid_argument);
  EXPECT_NO_THROW(memvar::memvar<double> mvd {});
  EXPECT_NO_THROW(memvar::memvar<double> mvd(11.2345, 20));

  EXPECT_THROW(memvar::memvar<std::string> mvs("Hello World!",0), std::invalid_argument);
  EXPECT_THROW(memvar::memvar<std::string> mvs("Hello World!",-10), std::invalid_argument);
  EXPECT_NO_THROW(memvar::memvar<std::string> mvs {});
  EXPECT_NO_THROW(memvar::memvar<std::string> mvs("Hello World!", 20));

  EXPECT_THROW(memvar::memvar<bigint::bigint> mvbi(bigint::bigint("1234567890"),0), std::invalid_argument);
  EXPECT_THROW(memvar::memvar<bigint::bigint> mvbi(bigint::bigint("1234567890"),-10), std::invalid_argument);
  EXPECT_NO_THROW(memvar::memvar<bigint::bigint> mvbi {});
  EXPECT_NO_THROW(memvar::memvar<bigint::bigint> mvbi(bigint::bigint("1234567890"), 20));
}

TEST(memVarTest, test_2_1)
{
  memvar::memvar<int> mv{0,1};
  ASSERT_EQ(0, mv());
  ASSERT_EQ(1, mv.getHistoryCapacity());
  for (int i = 1; i <= 1'000; ++i)
  {
    mv = i;
  }
  ASSERT_EQ(1, mv.getHistoryCapacity());
  ASSERT_EQ(1'000, mv());

  auto [min, max] = mv.getHistoryMinMax();
  ASSERT_EQ(1'000, min);
  ASSERT_EQ(1'000, max);
}

TEST(memVarTest, test_2)
{
  memvar::memvar<int> mv {};
  ASSERT_EQ(10, mv.getHistoryCapacity());
  mv.printHistoryData();
  ASSERT_EQ(int {}, mv());

  int v = mv();
  ASSERT_EQ(int {}, v);
  ASSERT_EQ(0, v);

  v = 123;
  ASSERT_NE(int {}, v);
  ASSERT_NE(v, mv());

  int w = mv();
  ASSERT_EQ(int {}, w);
  ASSERT_EQ(0, w);
}

TEST(memVarTest, test_3)
{
  memvar::memvar<int> mv {55};
  mv.printHistoryData();
  ASSERT_EQ(55, mv());

  int v = mv();
  ASSERT_EQ(55, v);

  v = 123;
  ASSERT_NE(55, v);
  ASSERT_NE(v, mv());

  int w = mv();
  ASSERT_EQ(55, w);
}

TEST(memVarTest, test_4)
{
  memvar::memvar<int> mv {33,9};
  ASSERT_EQ(9, mv.getHistoryCapacity());

  ASSERT_EQ(33, mv());

  mv = 78;
  mv.printHistoryData();

  ASSERT_EQ(78, mv());

  mv = 45;
  mv.printHistoryData();

  ASSERT_EQ(45, mv());

  ++mv;
  mv.printHistoryData();

  ASSERT_EQ(46, mv());

  mv = ++mv + mv();
  mv.printHistoryData();

  ASSERT_EQ(94, mv());

  mv++;
  mv.printHistoryData();

  ASSERT_EQ(95, mv());

  mv = ++mv + mv() + mv++;
  mv.printHistoryData();

  ASSERT_EQ(289, mv());

  mv--;
  mv.printHistoryData();

  ASSERT_EQ(288, mv());

  mv = --mv - mv() - mv--;

  ASSERT_EQ(-286, mv());

  mv.printHistoryData();

  auto [min, max] = mv.getHistoryMinMax();
  ASSERT_EQ(-286, min);
  ASSERT_EQ(289, max);
}

TEST(memVarTest, test_5)
{
  using memvarType = int64_t;
  // you need enough memory to run this test
  // if not, swap will be used if swap is on
  constexpr memvar::memvar<memvarType>::capacityType historyCapacity {1'000'000'000};
  memvar::memvar<memvarType> mv {0,historyCapacity};
  ASSERT_EQ(historyCapacity, mv.getHistoryCapacity());

  auto func = [&mv] ()
  {
    memvarType c {0};
    while ( false == mv.isHistoryFull() )
    {
      mv = ++c;
    }
  };
  auto t = perftimer<>::duration(func).count();
  std::cout << "loop took: " << t << " nsec" << '\n';

  ASSERT_EQ(mv.getHistoryCapacity(), mv.getHistorySize());
  ASSERT_EQ(historyCapacity - 1, mv());

  auto clearHistory = [&mv] ()
  {
    mv.clearHistory();
  };
  t = perftimer<>::duration(clearHistory).count();
  std::cout << "history clearing took: " << t << " nsec" << '\n';
  ASSERT_EQ(historyCapacity - 1, mv());
  ASSERT_EQ(historyCapacity, mv.getHistoryCapacity());
  ASSERT_EQ(1, mv.getHistorySize());
  mv.printHistoryData();
}

TEST(memVarTest, test_6)
{
  using memvarType = int64_t;
  constexpr memvar::memvar<memvarType>::capacityType historyCapacity {100};
  constexpr memvarType maxValue {1'000'000'000};
  memvar::memvar<memvarType> mv {0,historyCapacity};
  ASSERT_EQ(historyCapacity, mv.getHistoryCapacity());

  auto func = [&mv, maxValue] ()
  {
    memvarType c {0};
    while ( c < maxValue )
    {
      mv = ++c;
    }
  };
  auto t = perftimer<>::duration(func).count();
  std::cout << "loop took: " << t << " nsec" << '\n';

  ASSERT_EQ(maxValue, mv());
  ASSERT_EQ(historyCapacity, mv.getHistorySize());
  //mv.printHistoryData();

  auto clearHistory = [&mv] ()
  {
    mv.clearHistory();
  };
  t = perftimer<>::duration(clearHistory).count();
  std::cout << "history clearing took: " << t << " nsec" << '\n';
  ASSERT_EQ(historyCapacity, mv.getHistoryCapacity());
  ASSERT_EQ(1, mv.getHistorySize());
  ASSERT_EQ(maxValue, mv());
  mv.printHistoryData();
}

TEST(memVarTest, test_7)
{
  memvar::memvar<std::string> mvs {"A"};
  std::string s {"B"};
  mvs = s;
  mvs = mvs() + s;
  mvs.printHistoryData();

  ASSERT_EQ("BB", mvs());
  ASSERT_EQ(3, mvs.getHistorySize());
}

TEST(memVarTest, test_8)
{
  using memvarType = char;
  memvar::memvar<memvarType> mvc {'A'};
  memvarType c = mvc() + 1;
  mvc = c;  // B
  mvc++;  // C
  mvc++;  // D
  mvc--;  // C
  // [ C D C B A  ]
  mvc.printHistoryData();

  ASSERT_EQ('C', mvc());
  ASSERT_EQ(5, mvc.getHistorySize());

  memvar::memvar<memvarType>::historyValue hv {};

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
  std::tie(value, error) = mvc.getHistoryValue(5);
  ASSERT_EQ(char {}, value);
  ASSERT_EQ(true, error);

  // trying to access the history out of bound
  auto [v, e] = mvc.getHistoryValue(100000);
  ASSERT_EQ(memvarType {}, v);
  ASSERT_EQ(true, e);

  // trying to access the history out of bound
  std::tie(value, error) = mvc.getHistoryValue(-10);
  ASSERT_EQ(char {}, value);
  ASSERT_EQ(true, error);

  auto [min, max] = mvc.getHistoryMinMax();
  ASSERT_EQ('A', min);
  ASSERT_EQ('D', max);
}

TEST(memVarTest, test_9)
{
  using memvarType = int64_t;
  memvar::memvar<memvarType> mv1{10};
  memvar::memvar<memvarType> mv2{10};

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
  ASSERT_EQ(300, mv1());

  //mv1 == 300, mv2 == 20
  mv2 += mv1;
  ASSERT_EQ(320, mv2());

  mv1 -= mv1;
  ASSERT_EQ(0, mv1());

  mv2 /= mv2;
  ASSERT_EQ(1, mv2());

  mv1 = 8;
  mv1 *= mv1;
  ASSERT_EQ(64, mv1());

  mv1 /= 8;
  ASSERT_EQ(8, mv1());

  std::cout << "mv1: "; mv1.printHistoryData();
  std::cout << "mv2: "; mv2.printHistoryData();
}

// Yet another way to compute the Fibonacci numbers
TEST(memVarTest, fibonacciNumbers)
{
  constexpr auto maxFibNumberToCompute {93};
  using memvarType = uint64_t;
  // store fib(0)
  memvar::memvar<memvarType> fibs{0, maxFibNumberToCompute + 1};
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
    ASSERT_GE(fibs(), getHistoryValue(fibs, 1));
  }
  ASSERT_EQ(maxFibNumberToCompute + 1, fibs.getHistorySize());
  ASSERT_EQ(fib93, fibs());

  // print the first 94 Fibonacci numbers
  std::cout << "fibs: ";
  fibs.printHistoryData();
}

// Yet another way to compute the Fibonacci numbers with bigint's
TEST(memVarTest, fibonacciBigInts)
{
  constexpr auto maxFibNumberToCompute {2'000};
  using memvarType = bigint::bigint;
  // store fib(0)
  memvar::memvar<memvarType> fibs{0, maxFibNumberToCompute + 1};
  // store fib(1)
  fibs = 1;
  // compute and store fib(2) = fib(1) + fib(0) through
  // fib(2000) = fib(1999) + fib(1998)
  for(int i = 1; i < maxFibNumberToCompute; ++i)
  {
    fibs += getHistoryValue(fibs, 1);
    ASSERT_GE(fibs(), getHistoryValue(fibs, 1));
  }
  bigint::bigint fib_2000("4224696333392304878706725602341482782579852840250681098010280137314308584370130707224123599639141511088446087538909603607640194711643596029271983312598737326253555802606991585915229492453904998722256795316982874482472992263901833716778060607011615497886719879858311468870876264597369086722884023654422295243347964480139515349562972087652656069529806499841977448720155612802665404554171717881930324025204312082516817125");
  ASSERT_EQ(fib_2000, fibs());

  // print the first 2000 Fibonacci numbers
  std::cout << "fibs: ";
  fibs.printReverseHistoryData();
}
////////////////////////////////////////////////////////////////////////////////
#pragma clang diagnostic pop
// END: ignore the warnings when compiled with clang up to here
