#pragma once

#include <chrono>
#include <cstring>
#include <ctime>
#include <functional>
#include <optional>
#include <string>

#include <fmt/format.h>

#include <boost/core/typeinfo.hpp>

template<typename T>
std::string type_name() {
    return boost::core::demangled_name(BOOST_CORE_TYPEID(T));
}

struct scope_exit {
    scope_exit(std::function<void (void)> f) : f(f) {}
    ~scope_exit(void) { f(); }
private:
    std::function<void(void)> f;
};

std::string time_point_to_string(const std::chrono::system_clock::time_point &tp) {
    using namespace std::chrono;

    auto ttime_t = system_clock::to_time_t(tp);
    auto tp_sec = system_clock::from_time_t(ttime_t);
    milliseconds ms = duration_cast<milliseconds>(tp - tp_sec);

    std::tm *ttm = std::localtime(&ttime_t);

    char date_time_format[] = "%Y-%m-%d %H:%M:%S";
    char time_str[] = "yyyy-mm-dd HH:MM:SS.fff";

    std::strftime(time_str, std::strlen(time_str), date_time_format, ttm);

    string result(time_str);
    result.append(".");
    result.append(to_string(ms.count()));

    return result;
}

template <typename... Args> void log(const char *fmt, Args &&... args) {
    std::string nstr = time_point_to_string(std::chrono::system_clock::now());
    std::cout << fmt::format("[{}]: {}\n", std::move(nstr), fmt::format(fmt, std::forward<Args>(args)...));
}

// returns a constexpr true if the pack Args contains What
template<typename What, typename ...Args>
struct contains {
    static constexpr bool value {(std::is_same_v<What, Args> || ...)}; 
};

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

struct hostport {
    std::string             host;
    std::optional<uint16_t> port;

    explicit hostport(const std::string& host, std::optional<uint16_t> port = std::nullopt) : host(host), port(port) {}
};
