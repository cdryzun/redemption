/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "utils/enum_flags.hpp"

#include <cstdint>


namespace Rfb
{

enum class ChunkFlags : uint8_t
{
    NoFlags = 0,
    // Indicates that the chunk is the first in a sequence.
    First   = 1 << 0,
    // Indicates that the chunk is the last in a sequence.
    Last    = 1 << 1,
};

REDEMPTION_DECLARE_ENUM_FLAGS_NS(Rfb, ChunkFlags)

}
