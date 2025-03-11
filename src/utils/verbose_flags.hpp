/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <cinttypes>
#include "utils/enum_flags.hpp"


#define REDEMPTION_VERBOSE_FLAGS_DEC_OPS(Prefix, enum_name) \
    enum class enum_name : uint32_t;                        \
    REDEMPTION_DECLARE_ENUM_FLAG_OPS(Prefix, enum_name)


#define REDEMPTION_VERBOSE_FLAGS(visibility, verbose_member_name) \
    REDEMPTION_VERBOSE_FLAGS_DEC_OPS(constexpr friend, Verbose)   \
    visibility: Verbose const verbose_member_name;                \
    public: enum class Verbose : uint32_t


#define REDEMPTION_VERBOSE_FLAGS_DEF(enum_name)            \
    REDEMPTION_VERBOSE_FLAGS_DEC_OPS(constexpr, enum_name) \
    enum class enum_name : uint32_t
