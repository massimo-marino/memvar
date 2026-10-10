//
// perfTest.cpp
//
#include "perfTest.h"
#include "../memvar.h"

#include <iostream>
#include <iomanip>
#include <stdexcept>
#include <string>
#include <deque>
#include <vector>
////////////////////////////////////////////////////////////////////////////////
using memvarType = int64_t;

void perfTest (const mv::memvar<memvarType>::capacityType historyCapacity) {
  memvarType aVar{};
  mv::memvarTimed<memvarType> mvt {0, historyCapacity};

  std::cout << "History Capacity for mvt: " << mvt.getHistoryCapacity() << "\n";

  for (int i {1}; i <= 10; ++i) {
    mvt = i;
  }

  aVar = mvt;

  std::cout << "\naVar: " << aVar << "\n";
  std::cout << "mvt: "; mvt.printHistoryTimedData();

  // display the last 10 values stored
  for (int i {0}; i < 10; ++i) {
    std::cout << i << ": " << mvt(i) << "\n";
  }

  std::cout << "clearing history in memvar...\n";
  mvt.clearHistory();

  std::cout << "\nafter clearing history mvt: "; mvt.printHistoryTimedData();
  try {
    // try-to display the last 10 values stored, exception triggered
    for (int i {0}; i < 10; ++i) {
      std::cout << i << ": " << mvt(i) << "\n";
    }
    std::cout << "\n";
  } catch (const std::out_of_range& e) {
    std::cout << "\nException caught\n";
  }

  for (int i {1}; i <= 20; ++i) {
    mvt = i;
  }

  std::cout << "mvt: "; mvt.printHistoryTimedData();

  // display the last 20 values stored
  for (int i {0}; i < 20; ++i) {
    std::cout << i << ": " << mvt(i) << "\n";
  }

  std::cout << "clearing history in memvar...\n";
  mvt.clearHistory();

  std::cout << "\nafter clearing history mvt: "; mvt.printHistoryTimedData();

  // try-to display the last 20 values stored, exception triggered
  try {
		for (int i {0}; i < 20; ++i) {
		  std::cout << i << ": " << mvt(i) << "\n";
		}
		std::cout << "\n";
  } catch (const std::out_of_range& e) {
    std::cout << "\nException caught\n";
  }

  //////////////////////////////////////////////////////////////////////////////

  mv::memvar<memvarType> mv {0, historyCapacity};

  std::cout << "History Capacity for mv: " << mv.getHistoryCapacity() << "\n";

  auto simpleAssignment = [&mv] () noexcept {
    memvarType c {0x7FFFFFFFFFFFFFFF};
    while ( !mv.isHistoryFull() ) {
      mv = c;
    }
  };

  auto duration_secs = perftimer::duration(simpleAssignment);
  auto timeSpan = duration_secs.count();

  // display the last 10 values stored
  for (int i {0}; i < 10; ++i) {
    std::cout << i << ": " << std::dec << mv(i) << std::hex << " 0x" << mv(i)<< "\n";
  }

  // min must be 0
  // max must be 0x7FFFFFFFFFFFFFFF
  auto [min, max] = mv.getHistoryMinMax();
  std::cout << std::dec
            << "\n min: " << min << " max: " << max << " 0x" << std::hex << max << std::dec
            << "\nsimple assignment memo loop took: " << timeSpan << " sec - "
            << std::fixed << std::setprecision(4)
            << static_cast<double>(historyCapacity) / timeSpan
            << " int64 assignments per second\n\n";

  // display the last 10 values stored again
  for (int i {0}; i < 10; ++i) {
    std::cout << i << ": " << std::dec << mv(i) << std::hex << " 0x" << mv(i)<< "\n";
  }
  std::cout << std::dec << "\n";

  //////////////////////////////////////////////////////////////////////////////

  auto clearHistory = [&mv] () noexcept {
    mv.clearHistory();
  };

  std::cout << "clearing history in memvar...\n";
  duration_secs = perftimer::duration(clearHistory);
  timeSpan = duration_secs.count();

  // try-to display the last 10 values stored, exception triggered
  try {
		for (int i {0}; i < 10; ++i) {
		  std::cout << i << ": " << mv(i) << "\n";
		}
  } catch (const std::out_of_range& e) {
    std::cout << "\nException caught\n";
  }

  std::cout << "\nclearing history took: " << timeSpan << " sec (" << perftimer::to_msec(duration_secs) << " msec) - "
            << std::fixed << std::setprecision(4)
            << static_cast<double>(historyCapacity) / timeSpan
            << " items per second\n\n";

  //////////////////////////////////////////////////////////////////////////////

  auto assignmentWithIncrement = [&mv] () noexcept {
    memvarType c {0};
    while ( !mv.isHistoryFull() ) {
      mv = ++c;
    }
  };

  timeSpan = perftimer::duration(assignmentWithIncrement).count();

  // display the last 10 values stored
  for (int i {0}; i < 10; ++i) {
    std::cout << i << ": " << mv(i) << "\n";
  }

  // min2 must be 0
  // max2 must be historyCapacity - 1
  auto [min2, max2] = mv.getHistoryMinMax();
  std::cout << "\n" << min2 << " " << max2
            << "\nassignment with increment memo loop took: " << timeSpan << " sec - "
            << std::fixed << std::setprecision(4)
            << static_cast<double>(historyCapacity) / timeSpan
            << " int64 increment+assignments per second\n\n";

  // display the last 10 values stored
  for (int i {0}; i < 10; ++i) {
    std::cout << i << ": " << mv(i) << "\n";
  }
  std::cout << "\n";

  //////////////////////////////////////////////////////////////////////////////

  std::cout << "clearing history in memvar...\n";
  duration_secs = perftimer::duration(clearHistory);
  timeSpan = duration_secs.count();

  // try-to display the last 10 values stored, exception triggered
  try {
		for (int i {0}; i < 10; ++i) {
		  std::cout << i << ": " << mv(i) << "\n";
		}
  } catch (const std::out_of_range& e) {
    std::cout << "\nException caught\n";
  }

  std::cout << "\nclearing history took: " << timeSpan << " sec (" << perftimer::to_msec(duration_secs) << " msec) - "
            << std::fixed << std::setprecision(4)
            << static_cast<double>(historyCapacity) / timeSpan
            << " items per second\n\n";

  //////////////////////////////////////////////////////////////////////////////

  // check that the memory consumption does not change while running this function
  auto assignmentWithIncrementForALongTime = [&mv] () noexcept {
    std::cout << "Looping from 0 to " << std::dec << 0x00000000FFFFFFFF << ", taking a long time performing assignment with increment...\n\n";

    memvarType c {0};
    while ( c < 0x00000000FFFFFFFF ) {
      mv = ++c;
    }
  };

  timeSpan = perftimer::duration(assignmentWithIncrementForALongTime).count();

  std::cout << "memvar mv's history contains "
            << mv.getHistorySize()
            << " elements; its capacity is: "
            << mv.getHistoryCapacity()
            << " and it's full?: "
            << (mv.isHistoryFull() ? "true": "false")
            << "\n";
  auto [min3, max3] = mv.getHistoryMinMax();
  std::cout << "min value: " << min3 
            << " max value: "
            << max3
            << " - is min < max ? : "
            << ((min3 < max3) ? "true" : "false")
            << "\nassignment with increment long loop took: " << timeSpan << " sec - "
            << std::fixed << std::setprecision(4)
            << static_cast<double>(historyCapacity) / timeSpan
            << " int64 increment+assignments per second\n\n";

  // display the last 10s value stored
  for (int i {0}; i < 10; ++i) {
    std::cout << i << ": " << mv(i) << "\n";
  }
  std::cout << "\n\n";

  std::cout << "clearing history in memvar...\n";
  duration_secs = perftimer::duration(clearHistory);
  timeSpan = duration_secs.count();
  
  std::cout << "\nclearing history took: " << timeSpan << " sec (" << perftimer::to_msec(duration_secs) << " msec) - "
          << std::fixed << std::setprecision(4)
          << static_cast<double>(historyCapacity) / timeSpan
          << " items per second\n\n";

  //////////////////////////////////////////////////////////////////////////////

  std::cout << "--- Ended ---" << std::endl;
}

template <template <typename, typename...> class Container>
void logMaxSize(const std::string& containerName) {
  Container<int32_t> vi32;
  Container<int64_t> vi64;
  Container<float>   vf;
  Container<double>  vd;
  std::cout << containerName << "<int32_t> max_size(): " << vi32.max_size() << "\n";
  std::cout << containerName << "<int64_t> max_size(): " << vi64.max_size() << "\n";
  std::cout << containerName << "<float> max_size():   " << vf.max_size()   << "\n";
  std::cout << containerName << "<double> max_size():  " << vd.max_size()  << "\n";
}

int main (int argc, char* argv[]) {
  logMaxSize<std::vector>("std::vector");
  logMaxSize<std::deque>("std::deque");

  using capacityType = mv::memvar<memvarType>::capacityType;

  constexpr capacityType defaultHistoryCapacity {900'000'000};

  capacityType historyCapacity {defaultHistoryCapacity};

  if ( argc > 2 ) {
    std::cerr << "Usage: " << argv[0] << " [historyCapacity]\n";
    return 1;
  }

  if ( argc == 2 ) {
    try {
      size_t charsProcessed {0};
      const long long parsed {std::stoll(argv[1], &charsProcessed)};

      if ( charsProcessed != std::string{argv[1]}.size() ) {
        throw std::invalid_argument("trailing characters after number");
      }

      historyCapacity = static_cast<capacityType>(parsed);
    } catch (const std::exception& e) {
      std::cerr << "Invalid historyCapacity argument '" << argv[1] << "': " << e.what() << "\n";
      std::cerr << "Usage: " << argv[0] << " [historyCapacity]\n";
      return 1;
    }
  }

  perfTest(historyCapacity);
  return 0;
}

