/*
SPDX-FileCopyrightText: 2023 Wallix Proxies Team

SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

template<class T>
struct OutParam
{
    explicit OutParam(T& out_value) noexcept
    : out_value(out_value)
    {}

    OutParam(OutParam const&) = default;
    OutParam& operator=(OutParam const&) = delete;

    T& out_value;
};

template<class T>
struct InOutParam
{
    explicit InOutParam(T& inout_value) noexcept
    : inout_value(inout_value)
    {}

    InOutParam(InOutParam const&) = default;
    InOutParam& operator=(InOutParam const&) = delete;

    T& inout_value;
};
