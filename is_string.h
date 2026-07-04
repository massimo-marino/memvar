//
// is_string.h
//
#pragma once

#include <cstdlib>
#include <cstring>
#include <string>
#include <typeinfo>
#include <type_traits>
#include <memory>

// We just cover GCC/Clang for linux
#ifndef NO_DEMANGLE
#include <cxxabi.h>
////////////////////////////////////////////////////////////////////////////////
namespace memvar
{
template <typename T>
std::string type () {
  static_assert(sizeof(T) > 0, "T must be complete");
  auto        tn {typeid(T).name()};
  int         status {0};

  std::unique_ptr<char, decltype(&free)> demangled(abi::__cxa_demangle(tn, nullptr, nullptr, &status), free);
  std::string result = (status == 0 && demangled) ? demangled.get() : tn;

  return result;
}
}
#else
namespace memvar
{
// no demangling
template <typename T>
constexpr std::string type () {
  static_assert(sizeof(T) > 0, "T must be complete");
  return typeid(T).name();
}
}
#endif
namespace memvar
{
template <typename T>
struct is_string {
    using U = std::remove_cvref_t<T>;
    static constexpr bool value =
        std::is_same_v<U, std::string>        ||
        std::is_same_v<U, std::wstring>       ||
        std::is_same_v<U, std::u8string>      ||
        std::is_same_v<U, std::u16string>     ||
        std::is_same_v<U, std::u32string>;
};
template <typename T>
constexpr bool is_string_v = is_string<T>::value;
}  // namespace memvar
