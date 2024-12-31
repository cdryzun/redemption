/*
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

Product name: redemption, a FLOSS RDP proxy
Copyright (C) Wallix 2021
Author(s): Proxies Team
*/

#pragma once

#include "keyboard/kbdtypes.hpp"
#include "utils/sugar/zstring_view.hpp"
#include "utils/sugar/bounded_array_view.hpp"

#include <cstdint>
#include <cassert>


//====================================
// SCANCODES PHYSICAL LAYOUT REFERENCE
//====================================
// +----+  +----+----+----+----+  +----+----+----+----+  +----+----+----+----+  +-----+----+-------+
// | 01 |  | 3B | 3C | 3D | 3E |  | 3F | 40 | 41 | 42 |  | 43 | 44 | 57 | 58 |  | 37x | 46 | 1D+45 |
// +----+  +----+----+----+----+  +----+----+----+----+  +----+----+----+----+  +-----+----+-------+
//                                     ***  keycodes suffixed by 'x' are extended ***
// +----+----+----+----+----+----+----+----+----+----+----+----+----+--------+  +----+----+----+  +--------------------+
// | 29 | 02 | 03 | 04 | 05 | 06 | 07 | 08 | 09 | 0A | 0B | 0C | 0D |   0E   |  | 52x| 47x| 49x|  | 45 | 35x| 37 | 4A  |
// +-------------------------------------------------------------------------+  +----+----+----+  +----+----+----+-----+
// |  0F  | 10 | 11 | 12 | 13 | 14 | 15 | 16 | 17 | 18 | 19 | 1A | 1B |      |  | 53x| 4Fx| 51x|  | 47 | 48 | 49 |     |
// +------------------------------------------------------------------+  1C  |  +----+----+----+  +----+----+----| 4E  |
// |  3A   | 1E | 1F | 20 | 21 | 22 | 23 | 24 | 25 | 26 | 27 | 28 | 2B |     |                    | 4B | 4C | 4D |     |
// +-------------------------------------------------------------------------+       +----+       +----+----+----+-----+
// |  2A | 56 | 2C | 2D | 2E | 2F | 30 | 31 | 32 | 33 | 34 | 35 |     36     |       | 48x|       | 4F | 50 | 51 |     |
// +-------------------------------------------------------------------------+  +----+----+----+  +---------+----| 1Cx |
// |  1D  |  5Bx | 38 |           39           |  38x  |  5Cx |  5Dx |  1Dx  |  | 4Bx| 50x| 4Dx|  |    52   | 53 |     |
// +------+------+----+------------------------+-------+------+------+-------+  +----+----+----+  +---------+----+-----+

// http://kbdlayout.info/


struct KeyLayout
{
    static constexpr KeyLayout const& null_layout() noexcept;

    enum class KbdId : uint32_t;

    enum class RCtrlIsCtrl : bool { No, Yes };

    using unicode16_t = uint16_t;

    static constexpr unicode16_t DK = 1u << (sizeof(unicode16_t) * 8 - 1);
    static constexpr unicode16_t HI = 1u << (sizeof(unicode16_t) * 8 - 2);

    struct DKeyTable
    {
        struct DKey
        {
            union Data
            {
                unicode16_t result;
                uint16_t dkeytable_idx; // enable when (second & DK)
            };

            unicode16_t second;
            Data data;
        };

        struct Meta
        {
            uint16_t size;
            unicode16_t accent;
        };

        union Data
        {
            Meta meta;
            DKey dkey;
        };

        Data const* data;

        explicit operator bool () const noexcept
        {
            return data;
        }

        unicode16_t accent() const noexcept
        {
            assert(data);
            return data[0].meta.accent;
        }

        array_view<DKey> dkeys() const noexcept
        {
            assert(data);
            static_assert(sizeof(DKey) == sizeof(Data));
            static_assert(alignof(DKey) == alignof(Data));
            return array_view{reinterpret_cast<DKey const*>(&data[1]), data[0].meta.size}; /*NOLINT*/
        }

        DKey const * find_composition(unicode16_t unicode) const noexcept
        {
            // std::lower_bound()

            auto c = dkeys();
            auto it = c.begin();
            auto count = c.end() - it;

            while (count > 0) {
                auto step = count / 2;

                if (it[step].second < unicode) {
                    it += step + 1;
                    count -= step + 1;
                }
                else {
                    count = step;
                }
            }

            return it == c.end() || it->second != unicode ? nullptr : &*it;
        }
    };

    struct Mods
    {
        enum : unsigned
        {
            Shift,
            Control,
            Menu,
            NumLock,
            CapsLock,
            OEM_8,
            KanaLock,
        };
    };

    using HalfKeymap = sized_array_view<unicode16_t, 128>;
    using HalfKeymapPerBatchOf32 = sized_array_view<HalfKeymap, 32>;

    struct KeymapByMod
    {
        HalfKeymapPerBatchOf32 by_mods[4];

        HalfKeymap operator[](uint8_t i) const noexcept
        {
            return by_mods[i / 32][i % 32];
        }
    };

    using DKeyTableIdxPerBatchOf32 = sized_array_view<uint16_t const*, 32>;

    struct DKeyIdxByMod
    {
        DKeyTableIdxPerBatchOf32 by_mods[4];

        uint16_t const* operator[](uint8_t i) const noexcept
        {
            return by_mods[i / 32][i % 32];
        }
    };

    KbdId kbdid;
    RCtrlIsCtrl right_ctrl_is_ctrl;

    // scancodes used with Session Probe
    kbdtypes::Scancode key_R;
    kbdtypes::Scancode key_C;
    kbdtypes::Scancode key_V;

    // {uni_low(16b) [0-128], uni_low(16b) [128-255], uni_high(16b) from [0-128]}
    KeymapByMod keymap_by_mod[3];
    DKeyIdxByMod dkeymap_idx_by_mod;
    DKeyTable const* dkeymaps;

    zstring_view name;
    zstring_view display_name;
};


// null_layout() implementation

namespace detail
{
    inline constexpr KeyLayout::unicode16_t null_layout_unicodes[128] {};

    inline constexpr KeyLayout::HalfKeymap null_keymap_per_batch_of_32[] {
        null_layout_unicodes, null_layout_unicodes, null_layout_unicodes,
        null_layout_unicodes, null_layout_unicodes, null_layout_unicodes,
        null_layout_unicodes, null_layout_unicodes, null_layout_unicodes,
        null_layout_unicodes, null_layout_unicodes, null_layout_unicodes,
        null_layout_unicodes, null_layout_unicodes, null_layout_unicodes,
        null_layout_unicodes, null_layout_unicodes, null_layout_unicodes,
        null_layout_unicodes, null_layout_unicodes, null_layout_unicodes,
        null_layout_unicodes, null_layout_unicodes, null_layout_unicodes,
        null_layout_unicodes, null_layout_unicodes, null_layout_unicodes,
        null_layout_unicodes, null_layout_unicodes, null_layout_unicodes,
        null_layout_unicodes, null_layout_unicodes,
    };

    inline constexpr uint16_t const* null_dkeymap_idx_per_batch_of_32[32] {};

    inline constexpr KeyLayout null_layout_layout{
        KeyLayout::KbdId(0),
        KeyLayout::RCtrlIsCtrl::Yes,
        kbdtypes::Scancode(),
        kbdtypes::Scancode(),
        kbdtypes::Scancode(),
        {
            {{
                null_keymap_per_batch_of_32, null_keymap_per_batch_of_32,
                null_keymap_per_batch_of_32, null_keymap_per_batch_of_32,
            }},
            {{
                null_keymap_per_batch_of_32, null_keymap_per_batch_of_32,
                null_keymap_per_batch_of_32, null_keymap_per_batch_of_32,
            }},
            {{
                null_keymap_per_batch_of_32, null_keymap_per_batch_of_32,
                null_keymap_per_batch_of_32, null_keymap_per_batch_of_32,
            }},
        },
        {{
            null_dkeymap_idx_per_batch_of_32, null_dkeymap_idx_per_batch_of_32,
            null_dkeymap_idx_per_batch_of_32, null_dkeymap_idx_per_batch_of_32,
        }},
        nullptr,
        "null"_zv,
        "null"_zv,
    };
} // namespace detail

constexpr KeyLayout const& KeyLayout::null_layout() noexcept
{
    return detail::null_layout_layout;
}
