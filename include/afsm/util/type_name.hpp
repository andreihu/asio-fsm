#pragma once

#include <cxxabi.h>
#include <typeinfo>
#include <string>
#include <memory>
#include <cstdlib>

namespace afsm {
namespace util {

inline
std::string demangle(const char* mangled)
{
      int status = 0;
      std::unique_ptr<char[], void (*)(void*)> result(abi::__cxa_demangle(mangled, 0, 0, &status), std::free);
      return result.get() ? std::string(result.get()) : throw std::runtime_error("demangle failed");
}

template<class T>
std::string type_name() { 
    return demangle(typeid(T).name()); 
}

} // namespace util
} // namespace afsm
