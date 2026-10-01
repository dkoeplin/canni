#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string_view>

#define I64 int64_t
#define U64 size_t
#define F64 double

/// Enforces that the result of the function should not be discarded.
#define pure [[nodiscard]]

/// Used to denote a symbol or function should be kept even if unused.
#define keep [[maybe_unused]]

/// Documents that this struct is abstract (has pure virtual methods). No runtime effect.
#define abstract

/// Early return if [cond] is true.
#define return_if(cond, ...)           \
    if ((cond)) {                      \
        return __VA_ARGS__;            \
    }

#define UNREACHABLE std::abort()

#define ASSERT(condition, message)     \
    if (!(condition)) {                \
        std::cerr << message << std::endl; \
        std::abort();                  \
    }

/// Declares a category name for an Account subclass.
/// Defines a static constexpr kCategory string and overrides category().
#define CANNI_CATEGORY(name) \
    static constexpr std::string_view kCategory = #name; \
    [[nodiscard]] std::string_view category() const override { return kCategory; }
