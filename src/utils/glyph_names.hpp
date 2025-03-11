/*
SPDX-FileCopyrightText: 2026 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <cstdint>

// From font generated with tools/font_parser.py.
struct GlyphNames
{
    enum E : uint32_t
    {
        xmark = 0xf00d, // x

        copy = 0xf0c5,

        network_drive = 0xf6ff, // 🖧  (~ U+1f5a7)
        compact_disc = 0xf51f,  // 🖸  (~ U+1f5b8)
        floppy_disk = 0xf0c7,   // 🖬  (~ U+1f5ac)
        hard_drive = 0xf0a0,    // 🖴  (~ U+1f5b4)
        folder = 0xf07b,        // 🗀  (~ U+1f5c0)
        file = 0xf15b,          // 🖹  (~ U+1f5b9)
        file_line = 0xf15c,     // 🖹  (~ U+1f5b9)

        square = 0xf0c8, // ☐
        square_check = 0xf14a, // ☒

        angles_left = 0xf100,       // «  (~ U+00AB)
        angles_right = 0xf101,      // »  (~ U+00BB)
        thin_angles_left = 0xf102,  // «  (~ U+00AB)
        thin_angles_right = 0xf103, // »  (~ U+00BB)
        angle_left = 0xf104,        // ‹  (~ U+2039)
        angle_right = 0xf105,       // ›  (~ U+203A)

        arrow_down_a_to_z = 0xf15d, // ↓ a-z
        arrow_down_1_to_9 = 0xf162, // ↓ 1-9
        arrow_down_9_to_1 = 0xf886, // ↓ 9-1
        arrow_up_z_to_a = 0xf882, // ↑ z-a
        arrow_up_1_to_9 = 0xf163, // ↑ 1-9
        arrow_up_9_to_1 = 0xf887, // ↑ 9-1
    };
};
