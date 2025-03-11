/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "utils/sugar/bounded_bytes_view.hpp"
#include "mod/vnc/encoders/chunk_flags.hpp"

class Buf64k;

namespace Rfb
{

/*

7.5.4   CutTextReader

The server has new ISO 8859-1 (Latin-1) text in its cut buffer.
Ends of lines are represented by the linefeed / newline character (value 10) alone.
No carriage-return (value 13) is needed.

No. of bytes | Type     | [Value] | Description
1            | U8       | 3       | message-type
3            |          |         | padding
4            | U32      |         | length
length       | U8 array |         | text

See also Extended Clipboard Pseudo-Encoding which modifies the behaviour of this message.

*/

/*

7.4.6   ClientCutText

The server has new ISO 8859-1 (Latin-1) text in its cut buffer.
Ends of lines are represented by the linefeed / newline character (value 10) alone.
No carriage-return (value 13) is needed.

No. of bytes | Type     | [Value] | Description
1            | U8       | 6       | message-type
3            |          |         | padding
4            | U32      |         | length
length       | U8 array |         | text

See also Extended Clipboard Pseudo-Encoding which modifies the behaviour of this message.

*/

struct CutTextReader
{
    struct HeaderData
    {
        static constexpr unsigned header_len = 7;

        uint32_t text_len;

        void read(sized_bytes_view<header_len> data) noexcept;
    };

    CutTextReader() noexcept;

    struct ReadPacketResult
    {
        ChunkFlags chunk_flags;
        bounded_bytes_view<0, ~uint32_t{}> partial_data;
    };

    ReadPacketResult read_packet(Buf64k & buf) noexcept;

    uint32_t remaining_len() const noexcept
    {
        return m_header.text_len;
    }

private:
    enum class ParsingState : uint8_t;

    ParsingState m_state;
    HeaderData m_header;
};

struct ServerCutText
{
    static constexpr uint8_t message_type = 3;
};

struct ClientCutText
{
    static constexpr uint8_t message_type = 6;
};

} // namespace RFB
