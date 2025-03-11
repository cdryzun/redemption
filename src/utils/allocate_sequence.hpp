/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "utils/out_param.hpp"

#include <new>


template<class T>
[[nodiscard]]
T * allocate_sequence(std::size_t n)
{
    return static_cast<T*>(operator new(sizeof(T) * n, std::align_val_t{alignof(T)}));
}

template<class T>
T * allocate_sequence(OutParam<T*> p, std::size_t n)
{
    return p.out_value = allocate_sequence<T>(n);
}

template<class T>
void deallocate_sequence(T * p) noexcept
{
    operator delete(p, std::align_val_t{alignof(T)});
}
