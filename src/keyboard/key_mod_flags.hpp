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

namespace kbdtypes
{

enum class KeyMod : uint16_t
{
    LCtrl,
    RCtrl,
    LShift,
    RShift,
    LAlt,
    RAlt,
    LMeta,
    RMeta,
    NumLock,
    CapsLock,
    ScrollLock,
    KanaLock,
};

struct KeyModFlags
{
    constexpr KeyModFlags() noexcept = default;

    constexpr KeyModFlags(KeyMod mod) noexcept
    {
        set(mod);
    }

    constexpr KeyModFlags(KeyLocks locks) noexcept
    {
        sync_locks(locks);
    }

    constexpr void sync_locks(KeyLocks locks) noexcept
    {
        clear(KeyMod::NumLock);
        clear(KeyMod::CapsLock);
        clear(KeyMod::KanaLock);
        clear(KeyMod::ScrollLock);
        set_if(bool(locks & KeyLocks::NumLock), KeyMod::NumLock);
        set_if(bool(locks & KeyLocks::CapsLock), KeyMod::CapsLock);
        set_if(bool(locks & KeyLocks::KanaLock), KeyMod::KanaLock);
        set_if(bool(locks & KeyLocks::ScrollLock), KeyMod::ScrollLock);
    }

    constexpr unsigned test_as_uint(KeyMod mod) const noexcept
    {
        return (mods >> bitpos(mod)) & 1u;
    }

    constexpr bool test(KeyMod mod) const noexcept
    {
        return test_as_uint(mod);
    }

    constexpr bool test_any(KeyModFlags const & mods) const noexcept
    {
        return (this->mods & mods.mods);
    }

    constexpr bool has_ctrl() const noexcept;
    constexpr bool has_shift() const noexcept;
    constexpr bool has_meta() const noexcept;
    /// equivalent to has_ctrl() || has_shift() || has_meta().
    constexpr bool has_mods() const noexcept;
    constexpr bool has_locks() const noexcept;

    constexpr void set(KeyMod mod) noexcept
    {
        mods |= 1u << bitpos(mod);
    }

    constexpr void set_if(bool b, KeyMod mod) noexcept
    {
        mods |= b ? (1u << bitpos(mod)) : 0u;
    }

    constexpr void flip(KeyMod mod) noexcept
    {
        mods ^= 1u << bitpos(mod);
    }

    constexpr void clear(KeyMod mod) noexcept
    {
        mods &= ~(1u << bitpos(mod));
    }

    constexpr void update(KbdFlags flags, KeyMod mod) noexcept
    {
        clear(mod);
        // 0x8000 (Release) -> 0x1
        mods |= ((~static_cast<unsigned>(flags) >> 15) & 1u) << bitpos(mod);
    }

    constexpr unsigned as_uint() const noexcept
    {
        return mods;
    }

    constexpr KeyModFlags rmod_as_lmod() const noexcept
    {
        auto mask
          = (1u << bitpos(KeyMod::RCtrl))
          | (1u << bitpos(KeyMod::RShift))
          | (1u << bitpos(KeyMod::RAlt))
          | (1u << bitpos(KeyMod::RMeta))
        ;
        return KeyModFlags(mods | ((mods & mask) >> 1));
    }

    constexpr void reset() noexcept
    {
        mods = 0;
    }

    friend constexpr KeyModFlags operator & (KeyModFlags mods1, KeyModFlags mods2) noexcept
    {
        return KeyModFlags(mods1.mods & mods2.mods);
    }

    friend constexpr KeyModFlags operator | (KeyModFlags mods, KeyMod mod) noexcept
    {
        mods.set(mod);
        return mods;
    }

    friend constexpr bool operator == (KeyModFlags a, KeyModFlags b) noexcept
    {
        return a.mods == b.mods;
    }

    friend constexpr bool operator != (KeyModFlags a, KeyModFlags b) noexcept
    {
        return a.mods != b.mods;
    }


private:
    constexpr explicit KeyModFlags(uint16_t mods) noexcept : mods(mods) {}

    constexpr static uint16_t bitpos(KeyMod mod) noexcept
    {
        return static_cast<uint16_t>(mod);
    }

    uint16_t mods = 0;
};

constexpr KeyModFlags operator | (KeyMod mod1, KeyMod mod2) noexcept
{
    KeyModFlags f;
    f.set(mod1);
    f.set(mod2);
    return f;
}

constexpr bool KeyModFlags::has_ctrl() const noexcept
{
    return test_any(kbdtypes::KeyMod::LCtrl | kbdtypes::KeyMod::RCtrl);
}

constexpr bool KeyModFlags::has_shift() const noexcept
{
    return test_any(kbdtypes::KeyMod::LShift | kbdtypes::KeyMod::RShift);
}

constexpr bool KeyModFlags::has_meta() const noexcept
{
    return test_any(kbdtypes::KeyMod::LMeta | kbdtypes::KeyMod::RMeta);
}

constexpr bool KeyModFlags::has_mods() const noexcept
{
    using M = kbdtypes::KeyMod;
    return test_any(M::LCtrl | M::RCtrl | M::LShift | M::RShift | M::LMeta | M::RMeta);
}

constexpr bool KeyModFlags::has_locks() const noexcept
{
    using M = kbdtypes::KeyMod;
    return test_any(M::CapsLock | M::KanaLock | M::NumLock | M::ScrollLock);
}

} // namespace kbdtypes
