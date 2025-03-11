/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <cstdint>

#include "mod/vnc/rdp_adapters/rdpeclip.hpp"
#include "utils/sugar/static_string_from_enum_flags.hpp"


static_string<232> VNC::channel_flags_to_string(ChannelFlags channel_flags) noexcept
{
    return StaticStringFromEnumFlags::make<
        "CHANNEL_FLAG_FIRST"_name_of(ChannelFlags::First),
        "CHANNEL_FLAG_LAST"_name_of(ChannelFlags::Last),
        "CHANNEL_FLAG_SHOW_PROTOCOL"_name_of(ChannelFlags::ShowProtocol),
        "CHANNEL_FLAG_SUSPEND"_name_of(ChannelFlags::Suspend),
        "CHANNEL_FLAG_RESUME"_name_of(ChannelFlags::Resume),
        "CHANNEL_FLAG_SHADOW_PERSISTENT"_name_of(ChannelFlags::ShadowPersistent),
        "CHANNEL_PACKET_COMPRESSED"_name_of(ChannelFlags::PacketCompressed),
        "CHANNEL_PACKET_AT_FRONT"_name_of(ChannelFlags::PacketAtFront),
        "CHANNEL_PACKET_FLUSHED"_name_of(ChannelFlags::PacketFlushed),
        "CompressionTypeMask"_name_of(ChannelFlags::CompressionTypeMask)
    >(channel_flags);
}

char const * VNC::channel_flags_first_last_to_string(ChannelFlags channel_flags) noexcept
{
    if (flags_test(channel_flags, ChannelFlags::First | ChannelFlags::Last))
    {
        return "FIRST|LAST";
    }

    if (flags_test(channel_flags, ChannelFlags::First))
    {
        return "FIRST";
    }

    if (flags_test(channel_flags, ChannelFlags::Last))
    {
        return "LAST";
    }

    return "";
}


VNC::ChannelFlagsChecker::Status
VNC::ChannelFlagsChecker::next(ChannelFlags channel_flags) noexcept
{
    if (flags_any(channel_flags, ChannelFlags::First))
    {
        auto res = m_is_multi_fragment ? Status::MissingLast : Status::Ok;
        m_is_multi_fragment = !flags_any(channel_flags, ChannelFlags::Last);
        return res;
    }

    if (flags_any(channel_flags, ChannelFlags::Last))
    {
        auto res = m_is_multi_fragment ? Status::Ok : Status::MissingFirst;
        m_is_multi_fragment = false;
        return res;
    }

    return m_is_multi_fragment ? Status::Ok : Status::UnspecifiedFlags;
}
