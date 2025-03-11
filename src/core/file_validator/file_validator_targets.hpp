/*
SPDX-FileCopyrightText: 2026 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "utils/enum_flags.hpp"
#include "utils/sugar/zstring_view.hpp"

#include <cstdint>


enum class FileValidatorTargets : uint8_t
{
    None,
    // client to server
    Upload = 0b01,
    // server to client
    Download = 0b10,
};

REDEMPTION_DECLARE_ENUM_FLAGS(FileValidatorTargets)

inline zstring_view file_validator_target_to_string(FileValidatorTargets target) noexcept
{
    switch (target)
    {
        case FileValidatorTargets::None: return ""_zv;
        case FileValidatorTargets::Upload: return "up"_zv;
        case FileValidatorTargets::Download: return "down"_zv;
    }
    assert(false && "should an unique value (None, Upload or Download)");
    return ""_zv;
}
