/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "utils/is_ok.hpp"
#include "utils/enum_flags.hpp"
#include "utils/static_string.hpp"

// TODO namespace CHANNEL::v2 (?)
namespace VNC
{

// [MS-RDPBCGR] 2.2.6.1.1 Channel PDU Header (CHANNEL_PDU_HEADER)
enum class ChannelFlags : uint32_t
{
    NoFlags = 0,

    // Indicates that the chunk is the first in a sequence.
    First = 0x00000001, // CHANNEL_FLAG_FIRST
    // Indicates that the chunk is the last in a sequence.
    Last = 0x00000002, // CHANNEL_FLAG_LAST
    // The Channel PDU Header MUST be visible to the application
    // endpoint (section 2.2.1.3.4.1).
    ShowProtocol = 0x00000010, // CHANNEL_FLAG_SHOW_PROTOCOL
    // All virtual channel traffic MUST be suspended. This flag is only
    // valid in server-to-client virtual channel traffic. It MUST be ignored
    // in client-to-server data.
    Suspend = 0x00000020, // CHANNEL_FLAG_SUSPEND
    // All virtual channel traffic MUST be resumed. This flag is only valid
    // in server-to-client virtual channel traffic. It MUST be ignored in
    // client-to-server data.
    Resume = 0x00000040, // CHANNEL_FLAG_RESUME
    // This flag is unused and its value MUST be ignored by the client
    // and server.
    ShadowPersistent = 0x00000080, // CHANNEL_FLAG_SHADOW_PERSISTENT
    // The virtual channel data is compressed. This flag is equivalent to
    // MPPC bit C (for more information see [RFC2118] section 3.1).
    PacketCompressed = 0x00200000, // CHANNEL_PACKET_COMPRESSED
    // The decompressed packet MUST be placed at the beginning of the
    // history buffer. This flag is equivalent to MPPC bit B (for more
    // information see [RFC2118] section 3.1).
    PacketAtFront = 0x00400000, // CHANNEL_PACKET_AT_FRONT
    // The decompressor MUST reinitialize the history buffer (by filling it
    // with zeros) and reset the HistoryOffset to zero. After it has been
    // reinitialized, the entire history buffer is immediately regarded as
    // valid. This flag is equivalent to MPPC bit A (for more information
    // see [RFC2118] section 3.1). If the
    // CHANNEL_PACKET_COMPRESSED (0x00200000) flag is also
    // present, then the CHANNEL_PACKET_FLUSHED flag MUST be
    // processed first.
    PacketFlushed = 0x00800000, // CHANNEL_PACKET_FLUSHED
    // Indicates the compression package which was used to compress
    // the data. See the discussion which follows this table for a list of
    // compression packages.
    CompressionTypeMask = 0x000F0000, // CompressionTypeMask
};

/// ChannelFlags version without mppc flags
enum class CompressedChannelFlags : uint8_t
{
    NoFlags = 0,
    First = 0x00000001, // CHANNEL_FLAG_FIRST
    Last = 0x00000002, // CHANNEL_FLAG_LAST
    ShowProtocol = 0x00000010, // CHANNEL_FLAG_SHOW_PROTOCOL
    Suspend = 0x00000020, // CHANNEL_FLAG_SUSPEND
    Resume = 0x00000040, // CHANNEL_FLAG_RESUME
};

constexpr ChannelFlags uncompress_channel_flags(CompressedChannelFlags flags) noexcept
{
    return static_cast<ChannelFlags>(flags);
}

constexpr CompressedChannelFlags compress_channel_flags(ChannelFlags flags) noexcept
{
    return static_cast<CompressedChannelFlags>(flags);
}

static_string<232> channel_flags_to_string(ChannelFlags channel_flags) noexcept;
char const * channel_flags_first_last_to_string(ChannelFlags channel_flags) noexcept;

REDEMPTION_DECLARE_ENUM_FLAGS_NS(VNC, ChannelFlags)
REDEMPTION_DECLARE_ENUM_FLAGS_NS(VNC, CompressedChannelFlags)


/// Check the consistency of flags CHANNEL_FLAG_FIRST and CHANNEL_FLAG_LAST.
struct ChannelFlagsChecker
{
    enum class [[nodiscard]] Status : uint8_t
    {
        Ok,
        MissingLast,
        MissingFirst,
        UnspecifiedFlags,
    };

    Status next(ChannelFlags channel_flags) noexcept;

    bool is_multi_fragment() const noexcept
    {
        return m_is_multi_fragment;
    }

private:
    bool m_is_multi_fragment = false;
};

} // namespace VNC

template<>
inline constexpr auto is_ok_v<VNC::ChannelFlagsChecker::Status>
    = VNC::ChannelFlagsChecker::Status::Ok;
