/*
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   Product name: redemption, a FLOSS RDP proxy
   Copyright (C) Wallix 2010
   Author(s): Christophe Grosjean, Javier Caverni, Jonathan Poelen
   Based on xrdp Copyright (C) Jay Sorg 2004-2010

   log file including syslog
*/

#pragma once

#include <sys/types.h> // getpid
#include <unistd.h> // getpid

#include <type_traits>
#include <cstdint>
#include <cstdio>

#include "cxx/cxx.hpp"
#include "cxx/diagnostic.hpp"

#include <syslog.h>

namespace detail_
{
    template<class T>
    struct vlog_wrap
    {
        T value;
    };
} // namespace detail_

// T* to void* for %p
template<class T> detail_::vlog_wrap<void const*> log_value(T* p) noexcept { return {p}; }
template<class T> detail_::vlog_wrap<void const*> log_value(T const* p) noexcept { return {p}; }
inline detail_::vlog_wrap<char const*> log_value(char* p) noexcept { return {p}; } // NOLINT
inline detail_::vlog_wrap<char const*> log_value(char const* p) noexcept { return {p}; }
inline detail_::vlog_wrap<uint8_t const*> log_value(uint8_t* p) noexcept { return {p}; } // NOLINT
inline detail_::vlog_wrap<uint8_t const*> log_value(uint8_t const* p) noexcept { return {p}; }

template<class T>
auto log_value(T const & x) noexcept
{
    if constexpr (std::is_enum_v<T>) {
        using type = std::underlying_type_t<T>;
        return detail_::vlog_wrap<type>{static_cast<type>(x)};
    }
    else if constexpr (requires { x.c_str(); }) {
        return detail_::vlog_wrap<char const *>{x.c_str()};
    }
    else {
        return detail_::vlog_wrap<T const &>{x};
    }
}

#ifdef IN_IDE_PARSER
# define LOG(priority, format, ...)                  \
    [&](auto const&... elem){                        \
        printf("" format, log_value(elem).value...); \
    }(__VA_ARGS__)

#else
# ifdef NDEBUG
#   define LOG_REDEMPTION_FILENAME(priority)
# else
#   define LOG_REDEMPTION_FILENAME(priority)                 \
    REDEMPTION_DIAGNOSTIC_PUSH()                             \
    REDEMPTION_DIAGNOSTIC_CLANG_IGNORE("-Wunreachable-code") \
    if (priority != LOG_INFO && priority != LOG_DEBUG) {     \
        ::detail::LOG_REDEMPTION_INTERNAL(                   \
            priority, "%s (%d/%d) -- ◢ In %s:%d",           \
            __FILE__, __LINE__                               \
        );                                                   \
    }                                                        \
    REDEMPTION_DIAGNOSTIC_POP()
# endif

# define LOG(priority, format, ...) [&](auto const&... elem){    \
    using ::log_value;                                           \
    LOG_REDEMPTION_FILENAME(priority)                            \
    void(sizeof(printf("" format, log_value(elem).value...))),   \
    ::detail::LOG_REDEMPTION_INTERNAL(priority, "%s (%d/%d) -- " \
        format, log_value(elem).value...);                       \
 }(__VA_ARGS__)
#endif

#define LOG_IF(cond, priority, ...)                                              \
    if (REDEMPTION_UNLIKELY(cond)) /*NOLINT(readability-simplify-boolean-expr)*/ \
        LOG(priority, __VA_ARGS__)

void LOG_REDEMPTION_INTERNAL_IMPL(int priority, char const * format, ...) noexcept;

namespace detail
{
    // LOG_EMERG      system is unusable
    // LOG_ALERT      action must be taken immediately
    // LOG_CRIT       critical conditions
    // LOG_ERR        error conditions
    // LOG_WARNING    warning conditions
    // LOG_NOTICE     normal, but significant, condition
    // LOG_INFO       informational message
    // LOG_DEBUG      debug-level message
    constexpr const char * const prioritynames[] =
    {
        "EMERG"/*, LOG_EMERG*/,
        "ALERT"/*, LOG_ALERT*/,
        "CRIT"/*, LOG_CRIT*/,
        "ERR"/*, LOG_ERR*/,
        "WARNING"/*, LOG_WARNING*/,
        "NOTICE"/*, LOG_NOTICE*/,
        "INFO"/*, LOG_INFO*/,
        "DEBUG"/*, LOG_DEBUG*/,
        //{ nullptr/*, -1*/ }
    };

    template<class... Ts>
    void LOG_REDEMPTION_INTERNAL(int priority, char const * format, Ts const & ... args) noexcept
    {
        int const pid = getpid();
        LOG_REDEMPTION_INTERNAL_IMPL(
            priority,
            format,
            prioritynames[priority],
            pid,
            pid,
            args...
        );
    }
} // namespace detail
