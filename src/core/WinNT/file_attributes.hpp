/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "utils/enum_flags.hpp"
#include "utils/static_string.hpp"

// https://learn.microsoft.com/en-us/windows/win32/fileio/file-attribute-constants
enum class WinNtFileAttributeFlags : uint32_t
{
    /// A file that is read-only.
    /// Applications can read the file, but cannot write to it or delete it.
    ReadOnly = 0x00000001,
    /// The file or directory is hidden.
    /// It is not included in an ordinary directory listing.
    Hidden = 0x00000002,
    /// A file or directory that the operating system uses a part of,
    /// or uses exclusively.
    System = 0x00000004,
    /// Identifies a directory.
    Directory = 0x00000010,
    /// A file or directory that is an archive file or directory.
    /// Applications typically use this attribute to mark files
    /// for backup or removal.
    Archive = 0x00000020,
    /// A file that does not have other attributes set.
    /// This attribute is valid only when used alone.
    Normal = 0x00000080,
};

REDEMPTION_DECLARE_ENUM_FLAGS(WinNtFileAttributeFlags)

static_string<141> file_attribute_flags_to_string(WinNtFileAttributeFlags file_attrs) noexcept;
