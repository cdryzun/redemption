/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/


#include "mod/vnc/encoders/uvnc_file_transfer.hpp"
#include "core/buf64k.hpp"
#include "utils/stream.hpp"
#include "utils/mathutils.hpp"
#include "utils/tm_to_chars.hpp"
#include "core/WinNT/path.hpp"

#include <ctime>
#include <cstring>

namespace FT = UVNC::FileTransfer;

namespace
{
    inline constexpr uint32_t UVNC_FT_ERROR_CODE_32 = 0xffff'ffffu;
    inline constexpr uint64_t UVNC_FT_ERROR_CODE_64 = 0xffff'ffff'ffff'ffffu;
    inline constexpr uint32_t UVNC_FT_VERSION = 3;

    inline constexpr unsigned UVNC_FT_DRIVE_PART_LEN = 4;  // format: "{letter}:{type}\0"
    inline constexpr std::size_t UVNC_FT_DRIVE_LIST_MAX_PACKET_LEN
        = FT::DrivesList::max_drive * UVNC_FT_DRIVE_PART_LEN;

    bool is_uvnc_error(uint32_t value) noexcept
    {
        return value == UVNC_FT_ERROR_CODE_32;
    }

    bool is_uvnc_error(uint64_t value) noexcept
    {
        return value == UVNC_FT_ERROR_CODE_64;
    }
}


FT::DrivesList::DrivesList(bytes_view list) noexcept
{
    auto len = std::min(UVNC_FT_DRIVE_LIST_MAX_PACKET_LEN, list.size());
    len -= len % UVNC_FT_DRIVE_PART_LEN;
    m_list = list.first(len);
}

FT::DrivesList::DriveItem
FT::DrivesList::iterator::operator*() const noexcept
{
    return {p[0], safe_cast<DriveType>(p[2])};
}

FT::DrivesList::iterator&
FT::DrivesList::iterator::operator++() noexcept
{
    p += UVNC_FT_DRIVE_PART_LEN;
    return *this;
}

// unsigned FT::DrivesList::size() const noexcept
// {
//     return m_list.size() % UVNC_FT_DRIVE_PART_LEN;
// }


uint64_t FT::FileSizeOrError::file_size() const noexcept
{
    return size_or_error;
}

bool FT::FileSizeOrError::is_ok() const noexcept
{
    return !is_uvnc_error(size_or_error);
}

UVNCFileTransferReader::ReadPacketStatus
UVNCFileTransferReader::read_packet(Buf64k& buf, ReceivePacketCallbacks callbacks)
{
    struct PartialData
    {
        writable_bytes_view data;
        ReadPacketStatus res;
    };

    auto consume_partial_data = [&](Buf64k & buf){
        auto len = mmin(m_text_len, buf.remaining());
        m_text_len -= len;
        auto data = buf.get_buffer_and_advance(len);

        auto res = ReadPacketStatus::WaitData;
        // if reading data is completed
        if (!m_text_len)
        {
            m_content_type = 0;
            res = ReadPacketStatus::Completed;
        }

        return PartialData{ data, res };
    };

    // Used only when parsing size error
    uint32_t min_or_max_len_error = 0;
    ProtocolError::Type error_type = ProtocolError::Type::UnknownType;

    if (!m_content_type)
    {
        if (auto d = buf.consume(FileTransferHeader::pdu_len()))
        {
            InStream stream(d.data());

            auto h = FileTransferHeader::from_unchecked_read(stream);

            m_content_type = h.content_type;
            m_content_param = h.content_param;
            m_size_or_other = h.size_or_other;
            m_text_len = h.data_len;

            // check ResponseDrive::File without ResponseDrive::EndList
            if (m_first_pdu_sequence
             && (m_content_type != underlying_cast(ServerToClientContentType::DirPacket)
              || m_content_param == underlying_cast(ResponseDrive::DrivesList)))
            {
                goto invalid_file_list_sequence;
            }

            callbacks.parsing_header(callbacks.ctx);
        }
        else
        {
            return ReadPacketStatus::WaitData;
        }
    }

    #define CHECK_MAX_LEN(maxlen) do {         \
        min_or_max_len_error = maxlen;         \
        if (m_text_len > min_or_max_len_error) \
        {                                      \
            goto text_len_too_long;            \
        }                                      \
    } while (0)

    #define WAIT_PDU(data_name, len, maxlen)   \
        CHECK_MAX_LEN(maxlen);                 \
        if (buf.remaining() < len)             \
        {                                      \
            return ReadPacketStatus::WaitData; \
        }                                      \
        bytes_view data_name = buf.get_buffer_and_advance(len)

    #define READ_PDU(Type, ...) do {                  \
        Type pdu;                                     \
        if (pdu.read(data))                           \
        {                                             \
            __VA_ARGS__                               \
        }                                             \
        else                                          \
        {                                             \
            min_or_max_len_error = Type::pdu_max_len; \
            goto text_len_too_short;                  \
        }                                             \
    } while (0)

    switch (safe_cast<ServerToClientContentType>(m_content_type))
    {
        case ServerToClientContentType::DirPacket:
        {
            switch (safe_cast<ResponseDrive>(m_content_param))
            {
                case ResponseDrive::DrivesList:
                {
                    WAIT_PDU(data, m_text_len, UVNC_FT_DRIVE_LIST_MAX_PACKET_LEN);
                    m_content_type = 0;
                    callbacks.drive_list(callbacks.ctx, DrivesList{data});
                    return ReadPacketStatus::Completed;
                }

                case ResponseDrive::File:
                {
                    WAIT_PDU(data, m_text_len, FileInfoPDU::pdu_max_len);
                    if (m_first_pdu_sequence)
                    {
                        m_content_type = 0;
                        READ_PDU(FileInfoPDU, {
                            callbacks.file_info(callbacks.ctx, pdu);
                        });
                        return ReadPacketStatus::Completed;
                    }
                    else
                    {
                        CHECK_MAX_LEN(max_path_length);
                        m_content_type = 0;
                        // empty path is an error
                        m_first_pdu_sequence = !data.empty();
                        callbacks.start_list_dir(callbacks.ctx, Path::assumed(data));
                        return ReadPacketStatus::Completed;
                    }
                }

                case ResponseDrive::EndList:
                {
                    CHECK_MAX_LEN(0);
                    m_content_type = 0;
                    m_first_pdu_sequence = false;
                    callbacks.end_list_dir(callbacks.ctx);
                    return ReadPacketStatus::Completed;
                }
            }
            goto invalid_subtype;
        }

        case ServerToClientContentType::FileHeader:
        {
            // promote to u64 for dealed with overflow
            WAIT_PDU(data, m_text_len + uint64_t{4}, FileHeaderWithOptionalDataPDU::pdu_max_len);
            m_content_type = 0;
            READ_PDU(FileHeaderWithOptionalDataPDU, {
                callbacks.file_header(
                    callbacks.ctx,
                    pdu.file_name_with_optional_date,
                    FileSizeOrError{ pdu.file_size(m_size_or_other) }
                );
            });
            return ReadPacketStatus::Completed;
        }

        case ServerToClientContentType::FilePacket:
        {
            CHECK_MAX_LEN(m_block_size);

            auto [data, res] = consume_partial_data(buf);

            switch (auto type = safe_cast<FilePacketType>(m_size_or_other))
            {
                case FilePacketType::SkipData:
                case FilePacketType::Compressed:
                case FilePacketType::Uncompressed:
                    callbacks.file_partial_packet(callbacks.ctx, data, type);
                    return res;
            }
            goto invalid_file_packet_type;
        }

        case ServerToClientContentType::EndOfFile:
        {
            CHECK_MAX_LEN(0);
            m_content_type = 0;
            callbacks.end_of_file(callbacks.ctx);
            return ReadPacketStatus::Completed;
        }

        case ServerToClientContentType::AbortFileTransfer:
        {
            CHECK_MAX_LEN(0);
            m_content_type = 0;
            callbacks.aborted_file(callbacks.ctx);
            return ReadPacketStatus::Completed;
        }

        case ServerToClientContentType::FileAcceptHeader:
        {
            WAIT_PDU(data, m_text_len, max_path_length + 52);
            m_content_type = 0;
            callbacks.file_accept_header(callbacks.ctx, data, !is_uvnc_error(m_size_or_other));
            return ReadPacketStatus::Completed;
        }

        case ServerToClientContentType::FileChecksums:
        {
            CHECK_MAX_LEN(m_block_size);
            auto [checksums, res] = consume_partial_data(buf);
            callbacks.file_partial_checksums(callbacks.ctx, checksums, m_text_len);
            return res;
        }

        case ServerToClientContentType::CommandReturn:
        {
            WAIT_PDU(data, m_text_len, max_path_length * 2 + 1);
            m_content_type = 0;
            callbacks.command_return(callbacks.ctx, data, !is_uvnc_error(m_size_or_other));
            return ReadPacketStatus::Completed;
        }

        case ServerToClientContentType::FileTransferAccess:
        {
            CHECK_MAX_LEN(0);
            m_content_type = 0;
            callbacks.file_transfer_access(callbacks.ctx, !is_uvnc_error(m_size_or_other));
            return ReadPacketStatus::Completed;
        }

        case ServerToClientContentType::FileTransferProtocolVersion:
        {
            CHECK_MAX_LEN(0);
            m_content_type = 0;
            m_block_size = max_block_size_uvnc_1_6;

            auto version = m_content_param;
            if (version == 3)
            {
                // uvnc 1.7 server send block size
                if (m_size_or_other)
                {
                    if (m_size_or_other > max_block_size_authorized)
                    {
                        goto block_size_too_high;
                    }
                    m_block_size = m_size_or_other;
                }
            }

            bool is_supported = (version == UVNC_FT_VERSION);
            callbacks.protocol_version(callbacks.ctx, version, is_supported);
            return ReadPacketStatus::Completed;
        }
    }

    #undef READ_PDU
    #undef WAIT_PDU
    #undef CHECK_MAX_LEN

    // UnknownType
    goto error;

    block_size_too_high: {
        error_type = ProtocolError::Type::BlockSizeTooHigh;
        goto error;
    }
    text_len_too_long: {
        error_type = ProtocolError::Type::TooLargeDataLength;
        goto error;
    }
    text_len_too_short: {
        error_type = ProtocolError::Type::TooSmallDataLength;
        goto error;
    }
    invalid_subtype: {
        error_type = ProtocolError::Type::UnknownSubType;
        goto error;
    }
    invalid_file_list_sequence: {
        error_type = ProtocolError::Type::InvalidFileListSequence;
        goto error;
    }
    invalid_file_packet_type: {
        error_type = ProtocolError::Type::UnknownFilePacketType;
        min_or_max_len_error = 0;
        goto error;
    }

    static_assert(
        max_block_size_authorized
            <= std::numeric_limits<decltype(ProtocolError::max_or_min_len)>::max(),
        "adjust max_or_min_len type"
    );

    error: {
        callbacks.error(callbacks.ctx, ProtocolError{
            .type = error_type,
            .max_or_min_len = checked_int{ min_or_max_len_error },
        });
        return ReadPacketStatus::Error;
    }
}

namespace
{
    namespace pkt
    {
        template<class Pkt>
        struct OutsideToTextLen : Pkt
        {
            static constexpr unsigned sub_text_len = Pkt::serialized_len();
        };

        template<class Data>
        struct Bytes
        {
            static constexpr unsigned sub_text_len = 0;

            Data data;

            std::size_t serialized_len() const noexcept
            {
                return data.size();
            }

            void serialize(OutStream & out) noexcept
            {
                out.out_copy_bytes(data);
            }
        };

        template<class Data>
        Bytes(Data) -> Bytes<Data>;

        struct Date
        {
            static constexpr unsigned sub_text_len = 0;

            using DateFormat = dateformats::mm_dd_YYYY_HH_MM;

            WinNtClock::time_point tp;

            constexpr static std::size_t serialized_len() noexcept
            {
                return DateFormat::output_length;
            }

            void serialize(OutStream & out) noexcept
            {
                auto utc = std::chrono::clock_cast<std::chrono::utc_clock>(tp);
                auto epoch = utc.time_since_epoch();
                auto sec_epoch = std::chrono::duration_cast<std::chrono::seconds>(epoch);

                time_t t = sec_epoch.count();
                tm tm {};
                gmtime_r(&t, &tm);

                auto * p = out.out_skip_bytes(DateFormat::output_length).as_charp();
                DateFormat::to_chars(p, tm);
            }
        };

        struct Byte
        {
            static constexpr unsigned sub_text_len = 0;

            uint8_t data;

            constexpr static std::size_t serialized_len() noexcept
            {
                return 1;
            }

            void serialize(OutStream & out) noexcept
            {
                out.out_uint8(data);
            }
        };

        struct U32BE
        {
            // static constexpr unsigned sub_text_len = 0;

            uint32_t n;

            constexpr static std::size_t serialized_len() noexcept
            {
                return 4;
            }

            void serialize(OutStream & out) noexcept
            {
                out.out_uint32_be(n);
            }
        };

        struct Header
        {
            FT::ClientToServerContentType content_type;
            uint16_t content_param;
            uint32_t size_or_other;
            uint32_t text_len = 0;

            constexpr static std::size_t serialized_len() noexcept
            {
                return 12;
            }

            void serialize(OutStream & out) noexcept
            {
                out.out_uint8(UVNCFileTransferReader::message_type);
                out.out_uint8(safe_int{content_type});
                out.out_uint16_le(content_param);
                out.out_uint32_be(size_or_other);
                out.out_uint32_be(text_len);
            }
        };
    }

    template<class... Pkt>
    FT::WriteErrorCode
    pkt_serialize(OutStream & out, pkt::Header header, Pkt... pkt) noexcept
    {
        auto data_len = (std::size_t{} + ... + pkt.serialized_len());
        auto pdu_len = data_len + header.serialized_len();
        if (out.has_room(pdu_len))
        {
            constexpr auto sub_text_len = (std::size_t{} + ... + Pkt::sub_text_len);
            if constexpr (sub_text_len)
            {
                assert(data_len >= sub_text_len);
                data_len -= sub_text_len;
            }
            header.text_len = checked_int{data_len};
            header.serialize(out);
            (..., pkt.serialize(out));
            return FT::WriteErrorCode::NoError;
        }
        return FT::WriteErrorCode::TooSmallBuffer;
    }

    template<auto const& pdu>
    FT::WriteErrorCode copy_pdu(OutStream & out) noexcept
    {
        if (out.has_room(pdu.size()))
        {
            out.out_copy_bytes(pdu);
            return FT::WriteErrorCode::NoError;
        }
        return FT::WriteErrorCode::TooSmallBuffer;
    }
}

FT::WriteErrorCode
FT::write_command_create_directory(OutStream & out, Path path) noexcept
{
    return pkt_serialize(
        out,
        pkt::Header{
            .content_type = ClientToServerContentType::CommandRequest,
            .content_param = safe_int{Command::CreateDirectory},
            .size_or_other = 0,
        },
        pkt::Bytes{path.native()}
    );
}

FT::WriteErrorCode
FT::write_command_create_directory2(
    OutStream & out, bytes_view dirbase, bytes_view path) noexcept
{
    WinNtDirSep dir_sep {dirbase, path};
    if (dir_sep.is_truncated_path)
    {
        return FT::WriteErrorCode::TooLargeDataLength;
    }

    return pkt_serialize(
        out,
        pkt::Header{
            .content_type = ClientToServerContentType::CommandRequest,
            .content_param = safe_int{Command::CreateDirectory},
            .size_or_other = 0,
        },
        pkt::Bytes{dirbase},
        pkt::Bytes{dir_sep.mid_sep()},
        pkt::Bytes{path}
    );
}

FT::WriteErrorCode
FT::write_command_remove_file(OutStream & out, Path path) noexcept
{
    return pkt_serialize(
        out,
        pkt::Header{
            .content_type = ClientToServerContentType::CommandRequest,
            .content_param = safe_int{Command::RemoveFile},
            .size_or_other = 0,
        },
        pkt::Bytes{path.native()}
    );
}

FT::WriteErrorCode
FT::write_command_rename_file(OutStream & out, RenameParams paths) noexcept
{
    return pkt_serialize(
        out,
        pkt::Header{
            .content_type = ClientToServerContentType::CommandRequest,
            .content_param = safe_int{Command::RenameFile},
            .size_or_other = 0,
        },
        pkt::Bytes{paths.old_name.native()},
        pkt::Byte{'*'},
        pkt::Bytes{paths.new_name.native()}
    );
}

FT::WriteErrorCode
FT::write_session_start(OutStream & out) noexcept
{
    return copy_pdu<session_start_pdu>(out);
}

FT::WriteErrorCode
FT::write_session_end(OutStream & out) noexcept
{
    return copy_pdu<session_end_pdu>(out);
}

FT::WriteErrorCode
FT::write_drives_list_request(OutStream & out) noexcept
{
    return copy_pdu<drives_list_request_pdu>(out);
}

FT::WriteErrorCode
FT::write_directory_content_request(OutStream & out, Path path) noexcept
{
    return pkt_serialize(
        out,
        pkt::Header{
            .content_type = ClientToServerContentType::DirContentRequest,
            .content_param = safe_int{RequestDrive::Content},
            .size_or_other = 0,
        },
        pkt::Bytes{path.native()}
    );
}

FT::WriteErrorCode
FT::write_directory_content_request2(OutStream & out, bytes_view dirbase, bytes_view path) noexcept
{
    WinNtDirSep dir_sep {dirbase, path, WinNtDirSep::EndSep::Required};

    if (dir_sep.is_truncated_path)
    {
        return FT::WriteErrorCode::TooLargeDataLength;
    }


    return pkt_serialize(
        out,
        pkt::Header{
            .content_type = ClientToServerContentType::DirContentRequest,
            .content_param = safe_int{RequestDrive::Content},
            .size_or_other = 0,
        },
        pkt::Bytes{dirbase},
        pkt::Bytes{dir_sep.mid_sep()},
        pkt::Bytes{path},
        pkt::Bytes{dir_sep.end_sep()}
    );
}

FT::WriteErrorCode
FT::write_file_transfer_offer(
    OutStream & out, Path path, uint64_t file_size,
    WinNtClock::time_point last_write) noexcept
{
    return pkt_serialize(
        out,
        pkt::Header{
            .content_type = ClientToServerContentType::FileTransferOffer,
            .content_param = safe_int{RequestDrive::Content},
            .size_or_other = checked_int{file_size & 0xffff'ffff},
        },
        pkt::Bytes{path.native()},
        pkt::Byte{','},
        pkt::Date{last_write},
        pkt::OutsideToTextLen{pkt::U32BE{checked_int{file_size >> 32}}}
    );
}

FT::WriteErrorCode
FT::write_file_transfer_offer2(
    OutStream & out, bytes_view dirbase, bytes_view path, uint64_t file_size,
    WinNtClock::time_point last_write) noexcept
{
    WinNtDirSep dir_sep {dirbase, path};
    if (dir_sep.is_truncated_path)
    {
        return FT::WriteErrorCode::TooLargeDataLength;
    }

    return pkt_serialize(
        out,
        pkt::Header{
            .content_type = ClientToServerContentType::FileTransferOffer,
            .content_param = safe_int{RequestDrive::Content},
            .size_or_other = checked_int{file_size & 0xffff'ffffu},
        },
        pkt::Bytes{dirbase},
        pkt::Bytes{dir_sep.mid_sep()},
        pkt::Bytes{path},
        pkt::Byte{','},
        pkt::Date{last_write},
        pkt::OutsideToTextLen{pkt::U32BE{checked_int{file_size >> 32}}}
    );
}

FT::FilePacketResult
FT::write_file_packet(
    OutStream & out, FilePacketType type, bytes_view in_data, uint32_t block_size) noexcept
{
    auto n = out.tailroom();
    if (pkt::Header::serialized_len() < n)
    {
        n -= pkt::Header::serialized_len();
        auto partial_len = mmin(mmin(n, in_data.size()), block_size);

        pkt::Header header {
            .content_type = ClientToServerContentType::FilePacket,
            .content_param = 0,
            .size_or_other = safe_int{type},
            .text_len = partial_len,
        };
        header.serialize(out);
        out.out_copy_bytes(in_data.first(partial_len));

        return {FT::WriteErrorCode::NoError, in_data.drop_front(partial_len)};
    }

    return {FT::WriteErrorCode::TooSmallBuffer, in_data};
}

FT::FilePacketResult
FT::write_multi_uncompressed_file_packets(
    OutStream & out, bytes_view in_data, uint32_t block_size) noexcept
{
    FT::FilePacketResult result { FT::WriteErrorCode::NoError, in_data };
    while (!in_data.empty()
        && (result = write_file_packet(out, FilePacketType::Uncompressed, in_data, block_size)))
    {
        in_data = result.remaining_in_data;
    }
    return result;
}

FT::WriteErrorCode
FT::write_end_of_file(OutStream & out) noexcept
{
    return copy_pdu<end_of_file_pdu>(out);
}

FT::WriteErrorCode
FT::write_abort_file_transfer(OutStream & out) noexcept
{
    return copy_pdu<abort_file_transfer_pdu>(out);
}

FT::WriteErrorCode
FT::write_file_request(OutStream & out, FileRequestedFormat format, Path path) noexcept
{
    return pkt_serialize(
        out,
        pkt::Header{
            .content_type = ClientToServerContentType::FileTransferRequest,
            .content_param = 0,
            .size_or_other = safe_int{format},
        },
        pkt::Bytes{path.native()}
    );
}

FT::WriteErrorCode
FT::write_confirm_requested_file(OutStream & out, bool ok) noexcept
{
    return ok
        ? copy_pdu<confirm_requested_file_ok>(out)
        : copy_pdu<confirm_requested_file_failure>(out);
}
