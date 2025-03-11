/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "core/WinNT/file_attributes.hpp"
#include "utils/sugar/static_string_from_enum_flags.hpp"

static_string<141> file_attribute_flags_to_string(WinNtFileAttributeFlags file_attrs) noexcept
{
    return StaticStringFromEnumFlags::make<
        "FILE_ATTRIBUTE_READONLY"_name_of(WinNtFileAttributeFlags::ReadOnly),
        "FILE_ATTRIBUTE_HIDDEN"_name_of(WinNtFileAttributeFlags::Hidden),
        "FILE_ATTRIBUTE_SYSTEM"_name_of(WinNtFileAttributeFlags::System),
        "FILE_ATTRIBUTE_DIRECTORY"_name_of(WinNtFileAttributeFlags::Directory),
        "FILE_ATTRIBUTE_ARCHIVE"_name_of(WinNtFileAttributeFlags::Archive),
        "FILE_ATTRIBUTE_NORMAL"_name_of(WinNtFileAttributeFlags::Normal)
    >(file_attrs);
}
