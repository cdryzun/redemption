/*
SPDX-FileCopyrightText: 2023 Wallix Proxies Team

SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <type_traits>

#include <cstring>


template<class Sig>
struct BasicFunction;

struct NullFunction
{};

struct NullFunctionWithDefaultResult
{};

/// A function wrapper of lambda.
/// sizeof(lambda) should be lower or equal to sizeof(void*)
template<class R, class... Args>
struct BasicFunction<R(Args...)>
{
    using raw_function_pointer = R(*)(void* /*d*/, Args... /*args*/);

    BasicFunction() = delete;

    BasicFunction(BasicFunction &&) noexcept = default;
    BasicFunction(BasicFunction const&) noexcept = default;

    BasicFunction& operator=(BasicFunction &&) noexcept = default;
    BasicFunction& operator=(BasicFunction const&) noexcept = default;

    static raw_function_pointer make_raw_function_pointer() noexcept
    {
        static_assert(std::is_void_v<R>, "please, use make_raw_function_pointer_with_default_result()");
        return make_raw_function_pointer_with_default_result();
    }

    static raw_function_pointer make_raw_function_pointer_with_default_result() noexcept
    {
        return [](void* /*d*/, Args... /*args*/) { return R(); };
    }

    BasicFunction(NullFunction, raw_function_pointer fn) noexcept
        : fn_ptr(fn)
    {}

    BasicFunction(NullFunction) noexcept
        : BasicFunction(NullFunction(), make_raw_function_pointer())
    {}

    BasicFunction(NullFunctionWithDefaultResult) noexcept
        : BasicFunction(NullFunction(), make_raw_function_pointer_with_default_result())
    {}

    template<class Fn, class = std::enable_if_t<!std::is_same_v<Fn, BasicFunction>>>
    BasicFunction(Fn fn) noexcept
    {
        static_assert(sizeof(Fn) <= sizeof(data));
        static_assert(alignof(Fn) <= alignof(raw_function_pointer));
        static_assert(std::is_trivially_copyable_v<Fn>);
        static_assert(std::is_trivially_destructible_v<Fn>);

        static_assert(!std::is_pointer_v<Fn>, "function pointer is not optimized, prefer lambda");

        std::memcpy(data, &fn, sizeof(Fn));
        fn_ptr = +[](void* d, Args... args) {
            return (*static_cast<Fn*>(d))(static_cast<Args&&>(args)...);
        };
    }

    R operator()(Args... args)
    {
        return fn_ptr(data, static_cast<Args&&>(args)...);
    }

    bool is_null_function() const noexcept
    {
        return fn_ptr == BasicFunction(NullFunctionWithDefaultResult()).fn_ptr;
    }

    explicit operator bool () const noexcept
    {
        return !is_null_function();
    }

private:
    raw_function_pointer fn_ptr;
    alignas(raw_function_pointer) char data[sizeof(void*)];
};
