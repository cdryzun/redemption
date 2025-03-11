/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "mod/vnc/encoders/rfb/cut_text.hpp"
#include "utils/mathutils.hpp"
#include "utils/stream.hpp"
#include "core/buf64k.hpp"

enum class Rfb::CutTextReader::ParsingState : uint8_t
{
    ReadHeader,
    ReadData,
};

Rfb::CutTextReader::CutTextReader() noexcept
  : m_state(ParsingState::ReadHeader)
{
}

void Rfb::CutTextReader::HeaderData::read(sized_bytes_view<header_len> data) noexcept
{
    InStream stream{data};
    stream.in_skip_bytes(3); // padding
    text_len = stream.in_uint32_be();
}

Rfb::CutTextReader::ReadPacketResult
Rfb::CutTextReader::read_packet(Buf64k& buf) noexcept
{
    auto chunk_flags = ChunkFlags::NoFlags;

    if (m_state == ParsingState::ReadHeader)
    {
        if (auto d = buf.consume<HeaderData::header_len>())
        {
            m_header.read(d.data());
            m_state = ParsingState::ReadData;
            chunk_flags |= ChunkFlags::First;
        }
        else
        {
            return {chunk_flags, {}};
        }
    }

    /*
     * Read (partial) data
     */

    auto n = mmin(m_header.text_len, buf.remaining());
    m_header.text_len -= n;

    auto data = buf.consume_at_most(n);

    if (m_header.text_len == 0)
    {
        m_state = ParsingState::ReadHeader;
        chunk_flags |= ChunkFlags::Last;
    }
    return {chunk_flags, data};
}
