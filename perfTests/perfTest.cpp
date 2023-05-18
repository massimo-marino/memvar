//
// perfTest.cpp
//
// Created by massimo on 8/9/18.
//
#include "perfTest.h"
#include "../memvar.h"

#include <iostream>
#include <iomanip>
////////////////////////////////////////////////////////////////////////////////
int main ()
{
  using memvarType = int64_t;

  constexpr memvar::memvar<memvarType>::capacityType historyCapacity {900'000'000};
  memvarType aVar{};

  //////////////////////////////////////////////////////////////////////////////

  memvar::memvarTimed<memvarType> mvt {0, historyCapacity};

  std::cout << "History Capacity for mvt: " << mvt.getHistoryCapacity() << "\n";

  for (int i {1}; i <= 10; ++i)
  {
    mvt = i;
  }

  aVar = mvt;

  std::cout << "\naVar: " << aVar << "\n";
  std::cout << "mvt: "; mvt.printHistoryTimedData();

  // display the last 10 values stored
  for (int i {0}; i < 10; ++i)
  {
    std::cout << i << ": " << mvt(i) << "\n";
  }

  mvt.clearHistory();

  std::cout << "\nafter clearing history mvt: "; mvt.printHistoryTimedData();

  // display the last 10 values stored
  for (int i {0}; i < 10; ++i)
  {
    std::cout << i << ": " << mvt(i) << "\n";
  }
  std::cout << "\n";

  for (int i {1}; i <= 20; ++i)
  {
    mvt = i;
  }

  std::cout << "mvt: "; mvt.printHistoryTimedData();

  // display the last 20 values stored
  for (int i {0}; i < 20; ++i)
  {
    std::cout << i << ": " << mvt(i) << "\n";
  }

  mvt.clearHistory();

  std::cout << "\nafter clearing history mvt: "; mvt.printHistoryTimedData();

  // display the last 20 values stored
  for (int i {0}; i < 20; ++i)
  {
    std::cout << i << ": " << mvt(i) << "\n";
  }
  std::cout << "\n";

  //////////////////////////////////////////////////////////////////////////////

  memvar::memvar<memvarType> mv {0, historyCapacity};

  std::cout << "History Capacity for mv: " << mv.getHistoryCapacity() << "\n";

  auto simpleAssignment = [&mv] () noexcept
  {
    memvarType c {0x7FFFFFFFFFFFFFFF};
    while ( !mv.isHistoryFull() )
    {
      mv = c;
    }
  };

  auto timeSpan = perftimer<>::duration(simpleAssignment).count();

  // display the last 10 values stored
  for (int i {0}; i < 10; ++i)
  {
    std::cout << i << ": " << std::dec << mv(i) << std::hex << " 0x" << mv(i)<< "\n";
  }

  // min must be 0
  // max must be 0x7FFFFFFFFFFFFFFF
  auto [min, max] = mv.getHistoryMinMax();
  std::cout << std::dec
            << "\n min: " << min << " max: " << max << " 0x" << std::hex << max << std::dec
            << "\nsimple assignment memo loop took: " << timeSpan << " sec - "
            << std::fixed << std::setprecision(16)
            << static_cast<double>(historyCapacity) / timeSpan
            << " int64 assignments per second\n\n";

  // display the last 10 values stored again
  for (int i {0}; i < 10; ++i)
  {
    std::cout << i << ": " << std::dec << mv(i) << std::hex << " 0x" << mv(i)<< "\n";
  }
  std::cout << std::dec << "\n";

  //////////////////////////////////////////////////////////////////////////////

  auto clearHistory = [&mv] () noexcept
  {
    mv.clearHistory();
  };

  timeSpan = perftimer<>::duration(clearHistory).count();

  // display the last 10 values stored
  for (int i {0}; i < 10; ++i)
  {
    std::cout << i << ": " << mv(i) << "\n";
  }

  std::cout << "\nclearing history took: " << timeSpan << " sec - "
            << std::fixed << std::setprecision(16)
            << static_cast<double>(historyCapacity) / timeSpan
            << " items per second\n\n";

  //////////////////////////////////////////////////////////////////////////////

  auto assignmentWithIncrement = [&mv] () noexcept
  {
    memvarType c {0};
    while ( !mv.isHistoryFull() )
    {
      mv = ++c;
    }
  };

  timeSpan = perftimer<>::duration(assignmentWithIncrement).count();

  // display the last 10 values stored
  for (int i {0}; i < 10; ++i)
  {
    std::cout << i << ": " << mv(i) << "\n";
  }

  // min2 must be 0
  // max2 must be historyCapacity - 1
  auto [min2, max2] = mv.getHistoryMinMax();
  std::cout << "\n" << min2 << " " << max2
            << "\nassignment with increment memo loop took: " << timeSpan << " sec - "
            << std::fixed << std::setprecision(16)
            << static_cast<double>(historyCapacity) / timeSpan
            << " int64 increment+assignments per second\n\n";

  // display the last 10 values stored again
  for (int i {0}; i < 10; ++i)
  {
    std::cout << i << ": " << mv(i) << "\n";
  }
  std::cout << "\n";

  //////////////////////////////////////////////////////////////////////////////

  timeSpan = perftimer<>::duration(clearHistory).count();

  // display the last 10 values stored
  for (int i {0}; i < 10; ++i)
  {
    std::cout << i << ": " << mv(i) << "\n";
  }

  std::cout << "\nclearing history took: " << timeSpan << " sec - "
            << std::fixed << std::setprecision(16)
            << static_cast<double>(historyCapacity) / timeSpan
            << " items per second\n\n";

  //////////////////////////////////////////////////////////////////////////////

  // check the memory consumption does not change while running this function
  auto assignmentWithIncrementForALongTime = [&mv] () noexcept
  {
    std::cout << "Looping for a long time doing assignment with increment...\n\n";

    memvarType c {0};
    while ( c < 0x00000000FFFFFFFF )
    {
      mv = ++c;
    }
  };

  timeSpan = perftimer<>::duration(assignmentWithIncrementForALongTime).count();

  auto [min3, max3] = mv.getHistoryMinMax();
  std::cout << "\n" << min3 << " " << max3
            << "\nassignment with increment memo long loop took: " << timeSpan << " sec - "
            << std::fixed << std::setprecision(16)
            << static_cast<double>(historyCapacity) / timeSpan
            << " int64 increment+assignments per second\n\n";

  // display the last 10s value stored again
  for (int i {0}; i < 10; ++i)
  {
    std::cout << i << ": " << mv(i) << "\n";
  }
  std::cout << "\n\n";

  //////////////////////////////////////////////////////////////////////////////

  std::cout << "--- Ended ---" << std::endl;

  return 0;
}
