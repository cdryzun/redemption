/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "utils/sugar/bounded_bytes_view.hpp"
#include "utils/sugar/not_null_ptr.hpp"
#include "utils/sugar/bytes_view.hpp"
#include "utils/is_ok.hpp"
#include "core/WinNT/path.hpp"
#include "core/WinNT/time.hpp"
#include "core/WinNT/chrono.hpp"
#include "core/WinNT/file_attributes.hpp"
#include "core/protop/propop.hpp"
#include "core/protop/protop_bytes.hpp"
#include "mod/vnc/file_transfer/uvnc_drive_type.hpp"

class Buf64k;
class OutStream;

namespace UVNC::FileTransfer
{

/*

Based on https://github.com/ultravnc/UltraVNC
current tag = 1.5.0.16

== File transfer feature

current version = 3

// block size for packet transfer
nBlockSize = 32768 (v3 / uvnc 1.7) / 8192 (v3 / uvnc <= 1.6) / 4096 (v1)

*/

PROTOCOL_PARSER_DECL_STRUCT(
    FileTransferHeader,
    mem(
        static const uint8_t vnc_type = 7;
    ),
    /// Type of FT packet (see below for flow call).
    /// \see ClientToServerContentType
    /// \see ServerToClientContentType
    field(u8, content_type, i),
    /// Other possible content classification (Dir or File name, etc..) or zero.
    /// \see RequestDrive
    /// \see Command
    field(u16_le, content_param, i), // endianess of target... Assume x86
    /// File size or packet index or error or other.
    field(u32_be, size_or_other, i),
    /// Length of the data that follows.
    /// Except for \c FileHeader, which contains 4 additional bytes at the end.
    field(u32_be, data_len, i)
);

/*
              Client                      Server
                |                           |
                | <------- FileTransferProtocolVersion(17) (first packet)
                |                           |
                |/ optional check transfer \|
    ,-----------------------------------------------------,
    |           |                           |             |
    |  AbortFileTransfer(7) --------------> |             |
    |           |                           |             |
    |           | <-------------- FileTransferAccess(14)  |
    `-----------------------------------------------------'
                |                           |
-------------------------------------------------------------------------
                |                           |
  FileTransferSessionStart(15) -----------> |
                |                           |
-------------------------------------------------------------------------
                |   ,-------------------,   |
                |   | Command execution |   |
                |   '-------------------'   |
                |                           |
       CommandRequest(10) ----------------> |
                |                           |
                | <---------------- CommandReturn(11)
                |                           |
-------------------------------------------------------------------------
                |    ,------------------,   |
                |    | Media / Dir List |   |
                |    '------------------'   |
                |                           |
      DirContentRequest(1) ---------------> |
                |                           |
                | <------------------- DirPacket(2)
                |                           |
-------------------------------------------------------------------------
                |     ,---------------,     |
                |     | Donwload File |     |
                |     '---------------'     |
                |                           |
     FileTransferRequest(3) --------------> |
                |                           |
                | <------------------ FileHeader(4)
                |                           |
        FileChecksums(12) (optional) -----> |
                |                           |
          FileHeader(4) ------------------> |
                |                           |
                |                           |
                | <------------------ FilePacket(5)
                |                           ⋮
                |                           ⋮
     AbortFileTransfer(7) (optional) -----> ⋮
                |                           ⋮
                |                      EndOfFile(6) (when no abort)
                | <------------------------ or
                |                  AbortFileTransfer(7) (when abort + no eof | error)
                |                           |
     ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
                |       ,------------,      |
                |       | Limit case |      |
                |       '------------'      |
                |When AbortFileTransfer sent|
                |but is already EndOfFile or|
                |AbortFileTransfer sent     |
                |                           |
                | <-------------- FileTransferAccess(14)
                |                           |
-------------------------------------------------------------------------
                |       ,-----------,       |
                |       | Send File |       |
                |       '-----------'       |
                |                           |
       FileTransferOffer(8) --------------> |
                |                           |
                | <---------------- FileChecksums(12) (optional)
                |                           |
                | <--------------- FileAcceptHeader(9)
                |                           |
                |                           |
          FilePacket(5) ------------------> |
                ⋮                           |
                ⋮                           |
           EndOfFile(6) ------------------> |
               or                           |
       AbortFileTransfer(7) --------------> |
                |                           |
-------------------------------------------------------------------------
                |                           |
    FileTransferSessionEnd ---------------> |
                |                           |

*/

enum class Command : uint16_t
{
    CreateDirectory = 1,
    RemoveFile = 4,
    RenameFile = 5,
};

enum class RequestDrive : uint16_t
{
    // Request a Server Directory contents
    Content = 1,
    // Request the server's drives list
    DrivesList = 2,
};

enum class ResponseDrive : uint16_t
{
    // Last response after a directory list (response to RequestDrive::Content)
    EndList = 0,
    // file info (response to RequestDrive::Content)
    File = 1,
    // List of drive (response to RequestDrive::DrivesList)
    // data is a sequence of f"{drive_letter}:{DriveType}\0".
    // ex. "C:l<NULL>D:c<NULL>....Z:\<NULL>"
    DrivesList = 3,
};

enum class FilePacketType : uint32_t
{
    Uncompressed = 0,
    Compressed = 1,
    SkipData = 2,
};

enum class FileRequestedFormat : uint32_t
{
    Uncompressed = 0,
    Compressed = 1,
};

enum class ClientToServerContentType : uint8_t
{
    // Request drive or directory contents.
    // Special directory names (shortcut):
    //  - "Desktop"
    //  - "My Documents"
    //  - "Network Favorites"
    /* Msg{
        .content_param = RequestDrive
        .size = 0
        .length = text_len[max = MAX_PATH + 1]
        .text = dirname when RequestDrive::Content
              | ""      when RequestDrive::DrivesList
    } */
    DirContentRequest           = 1,

    // Client asks the server for the transfer of a given file
    /* Msg{
        .content_param = 0
        .size = FileRequestedFormat
        .length = text_len[max = MAX_PATH + 62]
        .text = filename
    } */
    FileTransferRequest         = 3,

    // Response to FileHeader
    /* Msg{
        .content_param = 0
        .size = Ok(!=-1u) | Error(-1u)
        .length = 0 (ignored)
    } */
    FileHeader                  = 4,

    // One chunk of the file
    // After FileAcceptHeader
    /* Msg{
        .content_param = 0
        .size = FilePacketType
        .length = text_len[max = nBlockSize]
        .text = chunk when .size = FilePacketType::Compressed or Uncompressed
              | "" when .size = FilePacketType::SkipData
    } */
    // Uncompressed max len = nBlockSize
    FilePacket                  = 5,

    // End of File Transfer (the file has been received or error)
    // After FilePacket
    /* Msg{
        .content_param = 0
        .size = 0
        .length = 0
    } */
    EndOfFile                   = 6,

    // The File Transfer must be aborted
    /* Msg{
        .content_param = fileTransferVersion(3)
        .size = 0
        .length = 0
    } */
    AbortFileTransfer           = 7,

    // The client offers to send a file to the server
    /* Msg{
        .content_param = 0
        .size = nFileSizeLow
        .length = filename_and_last_write_date_len[max = MAX_PATH + 52]
        .text = filename + last_write_date + nFileSizeHigh(u34)
    } */
    // date format = ",{month:2}/{day:2}/{year:4} {hour:2}:{minute:2}" (len = 17)
    // date is optional, conditionned on `strrchr(s, ',')`
    // size is used for check space disks
    FileTransferOffer           = 8,

    /* Msg{
        .content_param = Command
        .size = 0
        .length = text_len[max = MAX_PATH]
                | text_len[max = MAX_PATH * 2] when Command::RenameFile
        .text = dir or file name
              | f'{oldname}*{newname}' when Command::RenameFile
                (newname truncated when oldname.len = MAX_PATH and newname.len = MAX_PATH)
    } */
    CommandRequest              = 10,  // 0x0A

    // The checksums of the destination file (Delta Transfer)
    // Optional response to FileTransferOffer
    /* Msg{
        .content_param = Command
        .size = 4 * (FileSize / nBlockSize) + 1024  (useless)
        .length = checksum_len[max = 104857600 (100MiB)]
        .text = adler32 checksum by block of nBlockSize
    } */
    FileChecksums               = 12,  // 0x0C

    // Gui open (clipboard and screen refresh are blocked)
    /* Msg{
        .content_param = 0
        .size = 0
        .length = 0
    } */
    FileTransferSessionStart    = 15,  // 0x0F

    // Gui close
    /* Msg{
        .content_param = 0
        .size = 0
        .length = 0
    } */
    FileTransferSessionEnd      = 16,  // 0x10
};

enum class ServerToClientContentType : uint8_t
{
    // Response to DirContentRequest
    /* Msg{
        .content_param = RequestDrive
        .size = 0
        .length = text_len[max = MAX_PATH] first packet of ResponseDrive::File
                | text_len[max = MAX_PATH + 46] with ResponseDrive::File
                | Error(0) when .content_param = ResponseDrive::File
                | 0        when .content_param = ResponseDrive::EndList
        .text = ResponseDrive::DrivesList format
              | dirname first packet of ResponseDrive::File
              | _WIN32_FIND_DATAW with ResponseDrive::File
              | "" when error or ResponseDrive::EndList
    } */
    // When ResponseDrive::File, the first packet is the directory name requested.
    //  then file infos in the directory
    //  then ResponseDrive::EndList (without data)
    //
    // https://learn.microsoft.com/en-us/windows/win32/api/minwinbase/ns-minwinbase-win32_find_dataw
    /* _WIN32_FIND_DATAW{
        .dwFileAttributes: u32 // https://learn.microsoft.com/en-us/windows/win32/fileio/file-attribute-constants
        .ftCreationTime: FILETIME:u64 // https://learn.microsoft.com/en-us/windows/win32/sysinfo/file-times
        .ftLastAccessTime: FILETIME:u64
        .ftLastWriteTime: FILETIME:u64
        .nFileSizeHigh: u32
        .nFileSizeLow: u32
        .dwReserved0: u32
        .dwReserved1: u32
        .filename: u8[] // with 0 or several null character (currently always 2)
    } */
    DirPacket                   = 2,

    // Response to FileTransferRequest
    /* Msg{
        .content_param = 0
        .size = nFileSizeLow | Error(-1u)
        .length = filename_and_last_write_date_len[max = MAX_PATH + 62]
        .text = filename + last_write_date + nFileSizeHigh(u34)
              | filename + u32(-1u) when error
    } */
    // date format = ",{month:2}/{day:2}/{year:4} {hour:2}:{minute:2}" (len = 17)
    // date is optional, not inserted when GetFileTime() fails on server
    FileHeader                  = 4,

    // One chunk of the file
    // Response to FileHeader
    /* Msg{
        .content_param = 0
        .size = FilePacketType::Compressed / Uncompressed based on FileTransferRequest.size
                  + Uncompressed when compressed data is greater than uncompressed
              | FilePacketType::SkipData
        .length = text_len[max = nBlockSize]
        .text = chunk when .size = FilePacketType::Compressed / Uncompressed
              | "" when .size = FilePacketType::SkipData
    } */
    FilePacket                  = 5,

    // End of File Transfer (the file has been received or error)
    // After FilePacket
    /* Msg{
        .content_param = 0
        .size = 0
        .length = 0
    } */
    EndOfFile                   = 6,

    // The File Transfer (the file has an error)
    /* Msg{
        .content_param = 0
        .size = 0
        .length = 0
    } */
    AbortFileTransfer           = 7,

    // The server accepts or rejects the file
    // Response to FileTransferOffer
    /* Msg{
        .content_param = 0
        .size = Ok(0) | Error(-1u)
        .length = text_len[max = MAX_PATH + 52]
        .text = filename from FileTransferOffer (without date)
              | f'{dirs}\\!UVNCPFT-{filename}' when file contains '\' and not dir
    } */
    // presence of date is based on `strrchr(filename, ',')`.
    FileAcceptHeader            = 9,

    // Response to Command
    /* Msg{
        .content_param = Command
        .size = 0 | Error(-1u)
                  | CreateDirectory: already exists | intermediate directories do not exist
        .length = text_len[max = MAX_PATH]
                | text_len[max = MAX_PATH + 5] when Command::RemoveFile and filename is a directory
                                            ^ ' [' + ' ]' + '\0'
                | text_len[max = MAX_PATH * 2] when Command::RenameFile
        .text = dir or file name (from text of Command)
              | f'[ {dirname} ]' when Command::RemoveFile and filename is a directory
              | f'{oldname}*{newname}' when Command::RenameFile
    } */
    CommandReturn               = 11,  // 0x0B

    // The checksums of the destination file (Delta Transfer)
    // Optional response to FileTransferOffer
    /* Msg{
        .content_param = 0
        .size = 4 * (FileSize / nBlockSize) + 1024  (useless)
        .length = checksum_len
        .text = adler32 checksum by block of nBlockSize
    } */
    FileChecksums               = 12,  // 0x0C

    // Request File Transfer authorization
    /* Msg{
        .content_param = 0
        .size = Enabled(!=-1u) | Disabled(-1u)
        .length = 0
    } */
    FileTransferAccess          = 14,  // 0x0E

    // first packet
    /* Msg{
        .content_param = version
        .size = block size (principally for data and checksum)
              | 32768 (uvnc-1.7 [version=4])
              | 0 (uvnc-1.6 and below [version=3])
        .length = 0
    } */
    FileTransferProtocolVersion = 17,  // 0x1B
};

using Path = WinNtPathView;

inline constexpr uint32_t max_block_size_uvnc_1_6 = 8192;
// inline constexpr uint32_t max_block_size_uvnc_1_7 = 32768;
inline constexpr uint32_t max_block_size_authorized = 0xffff; // 65535 | uvnc limit = 1 Gio

inline constexpr uint32_t max_path_length = Path::Bytes::at_most;
static_assert(max_path_length <= 260);

inline constexpr uint32_t min_block_size = max_block_size_uvnc_1_6;
// CommandRequest / CommandReturn
inline constexpr uint32_t max_no_data_block_size = max_path_length * 2;

// inline constexpr uint32_t max_data_block_size = max_block_size_uvnc_1_7;

inline constexpr uint32_t header_packet_size = 12;
inline constexpr uint32_t max_no_data_packet_size = max_no_data_block_size + header_packet_size;
inline constexpr uint32_t min_full_packet_size = min_block_size + header_packet_size;
// inline constexpr uint32_t max_data_packet_size = max_data_block_size + header_packet_size;
// inline constexpr uint32_t max_packet_size = max_block_size + header_packet_size;


PROTOCOL_PARSER_DECL_STRUCT(
    FileInfoPDU,
    mem(
        uint64_t file_size() const noexcept
        {
            return file_size_low + (uint64_t{file_size_high} << 32);
        }

        void set_file_size(uint64_t size) noexcept
        {
            file_size_low = static_cast<decltype(file_size_low)>(size);
            file_size_high = size >> 32;
        }
    ),
    field(u32_le::as<WinNtFileAttributeFlags>, attributes,
        flags(file_attribute_flags_to_string)),
    field(u64_le::as<WinNtUTime>, creation_time, i),
    field(u64_le::as<WinNtUTime>, last_access_time, i),
    field(u64_le::as<WinNtUTime>, last_write_time, i),
    // Contains the most significant 4 bytes of the file size.
    field(u32_le, file_size_high, i),
    // Contains the least significant 4 bytes of the file size.
    field(u32_le, file_size_low, i),
    pad(8),
    field(
        dynamic_bytes<(protop::dynamic_bytes_properties{
            .max = max_path_length,
            .skip_end = 2, // 2 * '\0'
        })>,
        file_name, s
    )
);


PROTOCOL_PARSER_DECL_STRUCT(
    FileHeaderWithOptionalDataPDU,
    mem(
        /// \p file_size_low is the size_or_other member of \c FileTransferHeader.
        uint64_t file_size(uint32_t file_size_low) const noexcept
        {
            return (static_cast<uint64_t>(file_size_high) << 32) | file_size_low;
        }
    ),
    field(
        dynamic_bytes<(protop::dynamic_bytes_properties{
            .max = max_path_length + /* date format len */17 ,
            .remaining_after = 4,
        })>,
        file_name_with_optional_date, s
    ),
    field(u32_be, file_size_high, i)
);



/*
 * Parse DrivesList data without format checking.
 * "C:l<NULL>D:c<NULL>....Z:\<NULL>xxx"
 *  ^        ^            ^            drive_letter
 *    ^        ^            ^          drive_type
 *   ^ ^      ^ ^          ^ ^         ignored / unchecked
 *                                 ^^^ skipped (not multiple of 4)
 *
 * The "\" char following the drive letter and ":" is replaced with
 * letter corresponding to the type of drive: "C:\" -> "C:l"
 */
struct DrivesList
{
    static constexpr unsigned max_drive = 26;  // 26 letters

    struct DriveItem
    {
        uint8_t drive_letter;
        DriveType drive_type;
    };

    struct iterator
    {
        DriveItem operator*() const noexcept;

        iterator& operator++() noexcept;

        bool operator==(iterator const& other) const noexcept
        {
            return p == other.p;
        }

    private:
        friend DrivesList;
        iterator(uint8_t const* p) noexcept
            : p(p)
        {}

        uint8_t const* p;
    };

    explicit DrivesList(bytes_view list) noexcept;

    iterator begin() const noexcept
    {
        return iterator(m_list.begin());
    }

    iterator end() const noexcept
    {
        return iterator(m_list.end());
    }

private:
    bytes_view m_list;
};


struct FileSizeOrError
{
    uint64_t file_size() const noexcept;

    bool is_ok() const noexcept;

    bool is_error() const noexcept
    {
        return !is_ok();
    }

    uint64_t size_or_error;
};


struct UVNCFileTransferReader
{
    static constexpr uint8_t message_type = 7;
    static constexpr unsigned encoding_value = 0xFFFF8002; // -32766

    struct ProtocolError
    {
        enum class Type : uint8_t
        {
            BlockSizeTooHigh,
            TooLargeDataLength,
            TooSmallDataLength,
            UnknownType,
            UnknownSubType,
            InvalidFileListSequence,
            UnknownFilePacketType,
        };

        Type type;
        uint16_t max_or_min_len;
    };

    struct ReceivePacketCallbacks
    {
        void* ctx;

        not_null_ptr<void(void* ctx, ProtocolError err)> error;

        // parse header success
        not_null_ptr<void(void* ctx)> parsing_header;

        // receive drive list
        not_null_ptr<void(void* ctx, DrivesList drives)> drive_list;

        // list files of dir
        //@{
        // an empty path is equivalent to response error
        // file_info() and end_list_dir() are not called with response error
        not_null_ptr<void(void* ctx, Path path)> start_list_dir;
        not_null_ptr<void(void* ctx, FileInfoPDU file_info)> file_info;
        not_null_ptr<void(void* ctx)> end_list_dir;
        //@}

        // receive file
        //@{
        not_null_ptr<void(
            void* ctx,
            bytes_view file_name_with_optional_date,
            FileSizeOrError file_size_or_error
        )> file_header;
        not_null_ptr<void(void* ctx, bytes_view data, FilePacketType pkt_type)> file_partial_packet;
        not_null_ptr<void(void* ctx)> end_of_file;
        not_null_ptr<void(void* ctx)> aborted_file;
        //@}

        // send file
        //@{
        not_null_ptr<void(void* ctx, bytes_view checksums, uint32_t remaining)> file_partial_checksums;
        not_null_ptr<void(void* ctx, bytes_view tmp_file_name, bool accepted)> file_accept_header;
        //@}

        // receive command response
        not_null_ptr<void(void* ctx, bytes_view response, bool is_ok)> command_return;

        // receive permissions
        not_null_ptr<void(void* ctx, bool enabled)> file_transfer_access;

        // first packet
        not_null_ptr<void(void* ctx, uint32_t version, bool supported)> protocol_version;
    };

    UVNCFileTransferReader() noexcept = default;

    enum class ReadPacketStatus : uint8_t
    {
        Error,
        WaitData,
        Completed,
    };

    ReadPacketStatus read_packet(Buf64k & buf, ReceivePacketCallbacks callbacks);

    FileTransferHeader header() const noexcept
    {
        return FileTransferHeader{
            m_content_type,
            m_content_param,
            m_size_or_other,
            m_text_len,
        };
    }

    uint32_t block_size() const noexcept
    {
        return m_block_size;
    }

private:
    bool m_first_pdu_sequence {};
    // FileTransfer header data
    uint8_t m_content_type {};
    uint16_t m_content_param {};
    uint32_t m_size_or_other {};
    uint32_t m_text_len {};
    // context
    uint32_t m_block_size = max_block_size_uvnc_1_6;
};


constexpr inline auto session_start_pdu
  = "\x07""\x0f""\x00\x00""\x00\x00\x00\x00""\x00\x00\x00\x00"_av;

constexpr inline auto session_end_pdu
  = "\x07""\x10""\x00\x00""\x00\x00\x00\x00""\x00\x00\x00\x00"_av;

constexpr inline auto drives_list_request_pdu
  = "\x07""\x01""\x02\x00""\x00\x00\x00\x00""\x00\x00\x00\x00"_av;

constexpr inline auto end_of_file_pdu
  = "\x07""\x06""\x00\x00""\x00\x00\x00\x00""\x00\x00\x00\x00"_av;

constexpr inline auto abort_file_transfer_pdu
  = "\x07""\x07""\x03\x00""\x00\x00\x00\x00""\x00\x00\x00\x00"_av;

constexpr inline auto confirm_requested_file_ok
  = "\x07""\x04""\x00\x00""\x00\x00\x00\x00""\x00\x00\x00\x00"_av;

constexpr inline auto confirm_requested_file_failure
  = "\x07""\x04""\x00\x00""\x00\x00\x00\x00""\xff\xff\xff\xff"_av;


enum class [[nodiscard]] WriteErrorCode : uint8_t
{
    NoError,
    TooLargeDataLength,
    TooSmallBuffer,
};


struct [[nodiscard]] FilePacketResult
{
    WriteErrorCode ec;
    bytes_view remaining_in_data;

    explicit operator bool () const noexcept { return ec == WriteErrorCode::NoError; }
};

constexpr inline uint32_t file_request_pdu_max_len
    = FileTransferHeader::pdu_len() + Path::Bytes::at_most;

/// write CommandRequest.
//@{
struct RenameParams
{
    Path old_name;
    Path new_name;
};

WriteErrorCode write_command_create_directory(OutStream & out, Path path) noexcept;
WriteErrorCode write_command_create_directory2(
    OutStream & out, bytes_view dirbase, bytes_view path) noexcept;
WriteErrorCode write_command_remove_file(OutStream & out, Path path) noexcept;
/// new name must not contains '*'.
WriteErrorCode write_command_rename_file(OutStream & out, RenameParams paths) noexcept;
//@}

/// write FileTransferSessionStart.
WriteErrorCode write_session_start(OutStream & out) noexcept;

/// write FileTransferSessionEnd.
WriteErrorCode write_session_end(OutStream & out) noexcept;

/// write DirContentRequest with RequestDrive::DrivesList.
WriteErrorCode write_drives_list_request(OutStream & out) noexcept;

/// write DirContentRequest with RequestDrive::Content.
WriteErrorCode write_directory_content_request(OutStream & out, Path path) noexcept;

/// write DirContentRequest with RequestDrive::Content.
WriteErrorCode write_directory_content_request2(
    OutStream & out, bytes_view dirbase, bytes_view path) noexcept;

/// send file
//@{
/// write FileTransferOffer.
WriteErrorCode write_file_transfer_offer(
    OutStream & out, Path path, uint64_t file_size,
    WinNtClock::time_point last_write) noexcept;

/// write FileTransferOffer.
WriteErrorCode write_file_transfer_offer2(
    OutStream & out, bytes_view dirbase, bytes_view path, uint64_t file_size,
    WinNtClock::time_point last_write) noexcept;

/// write FilePacket.
/// \return Error::TooSmallBuffer when `min(block_size,in_data_len) <= out_len - header_len`.
/// \pre when \c type is FilePacketType::Compressed,
///     uncompressed data should be less or equal to block_size.
FilePacketResult write_file_packet(
    OutStream & out, FilePacketType type,
    bytes_view in_data, uint32_t block_size) noexcept;

/// write several FilePacket.
/// \return remaining in_data
FilePacketResult write_multi_uncompressed_file_packets(
    OutStream & out, bytes_view in_data, uint32_t block_size) noexcept;

/// write EndOfFile.
WriteErrorCode write_end_of_file(OutStream & out) noexcept;
// @}

/// receive file
//@{
/// write FileTransferRequest.
WriteErrorCode write_file_request(
    OutStream & out, FileRequestedFormat format, Path path) noexcept;

/// write FileTransferRequest with uncompressed format.
inline WriteErrorCode write_uncompressed_file_request(OutStream & out, Path path) noexcept
{
    return write_file_request(out, FileRequestedFormat::Uncompressed, path);
}

/// write FileTransferRequest with compressed format.
inline WriteErrorCode write_compressed_file_request(OutStream & out, Path path) noexcept
{
    return write_file_request(out, FileRequestedFormat::Compressed, path);
}

/// write FileHeader.
WriteErrorCode write_confirm_requested_file(OutStream & out, bool ok) noexcept;
// @}

/// write AbortFileTransfer.
WriteErrorCode write_abort_file_transfer(OutStream & out) noexcept;

} // namespace UVNC::FileTransfer


template<>
inline constexpr auto is_ok_v<UVNC::FileTransfer::WriteErrorCode>
    = UVNC::FileTransfer::WriteErrorCode::NoError;


using UVNC::FileTransfer::UVNCFileTransferReader;
