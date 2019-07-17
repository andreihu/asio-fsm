#pragma once

// thirdparty
#include <fmt/format.h>

// std
#include <system_error>

template <> struct fmt::formatter<std::error_code> {
  template <typename ParseContext> constexpr auto parse(ParseContext &ctx) {
    return ctx.begin();
  }

  template <typename FormatContext>
  auto format(const std::error_code &ec, FormatContext &ctx) {
    return format_to(ctx.out(), "error_code({}): {}", ec.value(), ec.message());
  }
};