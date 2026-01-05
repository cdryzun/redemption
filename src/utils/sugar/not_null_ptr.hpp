/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "cxx/cxx.hpp"

#include <type_traits>
#include <cassert>
#include <cstddef>


template<class T>
struct not_null_ptr
{
    using pointer = T*;
    using element_type = T;

    not_null_ptr(decltype(nullptr)) = delete;
    not_null_ptr(int) = delete;

    template<class U>
        requires (!std::is_same_v<T, U>) && std::is_base_of_v<T, U>
    constexpr not_null_ptr(not_null_ptr<U> ptr) noexcept
        : ptr_(ptr.get())
    {}

    template<class U>
        requires std::is_convertible_v<U*, T*>
    REDEMPTION_ATTRIBUTE_NONNULL_ARGS
    explicit constexpr not_null_ptr(U * ptr) noexcept
        : ptr_(ptr)
    {
        assert(ptr);
    }

    template<class U, std::size_t N>
        requires std::is_convertible_v<U(&)[N], T*>
    not_null_ptr(U (& array)[N]) noexcept
        : ptr_(array)
    {}

    constexpr not_null_ptr(T & ref) noexcept
        : ptr_(&ref)
    {}

    template<class U>
        requires std::is_function_v<T>
    constexpr not_null_ptr(U && fn) noexcept
        : ptr_(fn)
    {}

    not_null_ptr(T && ref) = delete;


    T * get() const noexcept REDEMPTION_ATTRIBUTE_RETURNS_NONNULL { return this->ptr_; }

    T & operator*() const noexcept { return *this->ptr_; }

    T * operator->() const noexcept REDEMPTION_ATTRIBUTE_RETURNS_NONNULL { return this->ptr_; }
    operator T * () const noexcept REDEMPTION_ATTRIBUTE_RETURNS_NONNULL { return this->ptr_; }

private:
    T * ptr_;
};


template<class T>
not_null_ptr(T*) -> not_null_ptr<T>;
