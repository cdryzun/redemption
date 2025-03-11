/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "core/WinNT/file_attributes.hpp"
#include "core/WinNT/time.hpp"
#include "utils/log.hpp"
#include "utils/literals/utf16.hpp"
#include "utils/stream.hpp"
#include "utils/enum_flags.hpp"
#include "utils/sugar/bytes_view.hpp"
#include "utils/sugar/bytes_equal.hpp"
#include "utils/sugar/bounded_array_view.hpp"
#include "utils/static_string.hpp"
#include "utils/utf.hpp"
#include "utils/function_ref.hpp"
#include "mod/vnc/rdp_adapters/rdp_channel.hpp"

#include "core/protop/propop.hpp"
#include "core/protop/protop_utf16.hpp"


// TODO optimize UTF16ByteLen

// TODO namespace cliprdr (?)
// TODO namespace CLIPRDR::v2 (?)
namespace VNC
{

// [MS-RDPECLIP]

/* 1.3.2.1 Initialization Sequence

Client                                 Server
   |<-- Server Clipboard Capabilities ---|
   |<-- Monitor Ready -------------------|
   |--- Client Clipboard Capabilities -->| (optional)
   |--- Temporary Directory ------------>| (optional)
   |--- Format List -------------------->|
   |<-- Format List Response ------------|

When processing a clipboard PDU, the msgType field in the header MUST first
be examined to determine if the PDU is within the subset of expected
messages. If the PDU is not expected, it SHOULD be ignored.

After determining that the PDU is in the correct sequence, the dataLen field
MUST be examined to make sure that it is consistent with the amount of data
read from the "CLIPRDR" static virtual channel. If this is not the case, the
connection SHOULD be dropped.
*/

/* 1.3.2.2 Data Transfer Sequences

Shared Clipboard Owner            Local Clipboard Owner
   |                                        |
   |--- Format List ----------------------->| --+ Copy Sequence
   |<-- Format List Response ---------------| _/
   |                                        |
   |<-- Lock Clipboard Data (Optional) -----|
   |                                        |
   |<-- Format Data Request ----------------| --+ Paste Sequence for Generic, Palette
   |--- Format Data Response -------------->| _/  Metafile and File List Data
   |                                        |
   |<-- File Contents Request --------------| --+ Paste Sequence
   |--- File Contents Response ------------>| _/  for File Stream Data
   |                                        |
   |<-- Unlock Clipboard Data (Optional) ---|
*/

// TODO strong type for msg_type, type, general_flags, etc
// TODO add PDU suffix ? In PDUs namespace ?
// TODO remove Cb prefix in enum ?
// TODO add everywhere Cliprdr preffix or not ?

/// Type of the clipboard PDU.
enum class CbMsgType : uint16_t
{
    MonitorReady = 0x0001, // CB_MONITOR_READY
    FormatList = 0x0002, // CB_FORMAT_LIST
    FormatListResponse = 0x0003, // CB_FORMAT_LIST_RESPONSE
    FormatDataRequest = 0x0004, // CB_FORMAT_DATA_REQUEST
    FormatDataResponse = 0x0005, // CB_FORMAT_DATA_RESPONSE
    TempDirectory = 0x0006, // CB_TEMP_DIRECTORY
    ClipCaps = 0x0007, // CB_CLIP_CAPS
    FileContentsRequest = 0x0008, // CB_FILECONTENTS_REQUEST
    FileContentsResponse = 0x0009, // CB_FILECONTENTS_RESPONSE
    LockClipdata = 0x000A, // CB_LOCK_CLIPDATA
    UnlockClipdata = 0x000B, // CB_UNLOCK_CLIPDATA
};

const char * msg_type_to_name(CbMsgType msg_type) noexcept;

enum class CbMsgFlags : uint16_t
{
    None = 0,
    /// Used by the Format List Response PDU, Format Data Response PDU, and File
    /// Contents Response PDU to indicate that the associated request Format List PDU,
    /// Format Data Request PDU, and File Contents Request PDU were processed
    /// successfully.
    ResponseOk = 0x0001, // CB_RESPONSE_OK
    /// Used by the Format List Response PDU, Format Data Response PDU, and File
    /// Contents Response PDU to indicate that the associated Format List PDU, Format
    /// Data Request PDU, and File Contents Request PDU were not processed successfully.
    ResponseFail = 0x0002, // CB_RESPONSE_FAIL
    /// Used by the Short Format Name variant of the Format List Response PDU to indicate
    /// that the format names are in ASCII 8.
    AsciiNames = 0x0004, // CB_ASCII_NAMES
};

static_string<50> msg_flags_to_string(CbMsgFlags msg_flags) noexcept;

REDEMPTION_DECLARE_ENUM_FLAGS_NS(VNC, CbMsgFlags)


/// Specifies the general capability flags.
enum class CbCapabilityFlags : uint32_t
{
    /// The Long Format Name variant of the Format List PDU is supported
    /// for exchanging updated format names. If this flag is not set, the
    /// Short Format Name variant MUST be used. If this flag is set by both
    /// protocol endpoints, then the Long Format Name variant MUST be
    /// used.
    UseLongFormatNames = 0x00000002, // CB_USE_LONG_FORMAT_NAMES
    /// File copy and paste using stream-based operations are supported
    /// using the File Contents Request PDU and File Contents Response
    /// PDU.
    StreamFileClipEnabled = 0x00000004, // CB_STREAM_FILECLIP_ENABLED
    /// Indicates that any description of files to copy and paste MUST NOT
    /// include the source path of the files.
    FileClipNoFilePaths = 0x00000008, // CB_FILECLIP_NO_FILE_PATHS
    /// Locking and unlocking of File Stream data on the clipboard is
    /// supported using the Lock Clipboard Data PDU and Unlock Clipboard
    /// Data PDU.
    CanLockClipData = 0x00000010, // CB_CAN_LOCK_CLIPDATA
    /// Indicates support for transferring files that are larger than
    /// 4,294,967,295 bytes in size. If this flag is not set, then only files of
    /// size less than or equal to 4,294,967,295 bytes can be exchanged
    /// using the File Contents Request PDU and File Contents
    /// Response PDU.
    HugeFileSupportEnabled = 0x00000020, // CB_HUGE_FILE_SUPPORT_ENABLED
};

REDEMPTION_DECLARE_ENUM_FLAGS_NS(VNC, CbCapabilityFlags)

static_string<131> capability_flags_to_string(CbCapabilityFlags cap_flags) noexcept;


/// Type identifier of the capability set.
enum class CbCapabilityType : uint16_t
{
    /// General Capability Set (CLIPRDR_GENERAL_CAPABILITY)
    General = 0x0001,
};

/// Specifies the Clipboard Format ID of the clipboard data.
/// The Clipboard Format ID MUST be one listed previously in the Format List PDU.
enum class CbFormatID : uint32_t
{
    /// Predefined Clipboard Formats (WinUser.h)
    Text            = 1,
    /// Bitmap          = 2,
    /// MetaFilePict    = 3,
    /// Sylk            = 4,
    /// Dif             = 5,
    /// Tiff            = 6,
    /// Oemtext         = 7,
    /// Dib             = 8,
    /// Palette         = 9,
    /// Pendata         = 10,
    /// Riff            = 11,
    /// Wave            = 12,
    UnicodeText     = 13,
    /// EnhMetaFile     = 14,
    /// Hdrop           = 15,
    /// Locale          = 16,
    /// Dibv5           = 17,
    /// OwnerDisplay    = 128,
    /// DspText         = 129,
    /// DspBitmap       = 130,
    /// DspMetaFilePict = 131,
    /// DspEnhMetaFile  = 142,
    /// PrivateFirst    = 512,
    /// PrivateLast     = 767,
    /// GdIObjFirst     = 768,
    /// GdIObjLast      = 1023,
};

const char * format_id_to_string(CbFormatID format_id) noexcept;

/// Format ID used to associate the File Contents Request PDU
/// with the corresponding File Contents Response PDU. The File Contents
/// Response PDU is sent as a reply and contains an identical value in
/// the streamId field.
enum class CbStreamId : uint32_t;

/// Specifies the numeric ID of the remote file that is the target of
/// the File Contents Request PDU. This field is used as an index that
/// identifies a particular file in a File List. This File List SHOULD
/// have been obtained as clipboard data in a prior Format Data Request
/// PDU and Format Data Response PDU exchange.
enum class CbLindex : uint32_t;

/// Specifies the type of operation to be performed by the recipient.
/// dwFlags field of CLIPRDR_FILECONTENTS_REQUEST
enum class CbFileContentsType : uint32_t
{
    /// A request for the size of the file identified by the lindex field. The size MUST be
    /// returned as a 64-bit, unsigned integer. The cbRequested field MUST be set to
    /// 0x00000008 and both the nPositionLow and nPositionHigh fields MUST be
    /// set to 0x00000000.
    Size = 0x00000001,
    /// A request for the data present in the file identified by the lindex field. The data
    /// to be retrieved is extracted starting from the offset given by the nPositionLow
    /// and nPositionHigh fields. The maximum number of bytes to extract is specified
    /// by the cbRequested field.
    Range = 0x00000002,
};

const char * file_contents_type_to_string(CbFileContentsType contents_type) noexcept;

/// Is used to tag File Stream data on the Shared Owner clipboard so that it can
/// be requested in a subsequent File Contents Request PDU (section 2.2.5.3).
enum class ClipDataId : uint32_t;


/// Specifies which fields of FileDescriptor contain valid data
/// and the usage of progress UI during a copy operation.
enum class FileDescriptorFlags : uint32_t
{
    /// The fileAttributes field contains valid data.
    Attributes = 0x00000004,
    /// The fileSizeHigh and fileSizeLow fields contain valid data.
    FileSize = 0x00000040,
    /// The lastWriteTime field contains valid data.
    WriteTime = 0x00000020,
    /// A progress indicator SHOULD be shown when copying the file.
    ShowProgressUI = 0x00004000,
};

REDEMPTION_DECLARE_ENUM_FLAGS_NS(VNC, FileDescriptorFlags)

static_string<61> file_descriptor_flags_to_string(FileDescriptorFlags cap_flags) noexcept;


using FileAttributeFlags = WinNtFileAttributeFlags;


inline constexpr uint32_t format_list_short_name_len = 32;

struct UnicodeLongName
{
    explicit constexpr UnicodeLongName(bytes_view name) noexcept
        : m_name(name)
    {}

    static constexpr bool is_ascii() noexcept { return false; }
    static constexpr bool is_unicode() noexcept { return true; }

    constexpr bool is_long_format() const noexcept { return true; }

    constexpr bytes_view raw_name() const noexcept { return m_name; }

    constexpr bytes_view unicode_name() const noexcept { return m_name; }

private:
    bytes_view m_name;
};

struct UnicodeShortName
{
    // CLIPRDR_SHORT_FORMAT_NAME in unicode format is 30 characters + null terminated (2 bytes)
    static constexpr std::size_t max_name_len = format_list_short_name_len - 2;
    using view_name_t = bounded_bytes_view<0, max_name_len>;

    explicit constexpr UnicodeShortName(view_name_t name) noexcept
        : m_name(name)
    {}

    static constexpr bool is_ascii() noexcept { return false; }
    static constexpr bool is_unicode() noexcept { return true; }

    constexpr bool is_long_format() const noexcept { return false; }

    constexpr bytes_view raw_name() const noexcept { return m_name; }

    constexpr view_name_t unicode_name() const noexcept { return m_name; }

private:
    view_name_t m_name;
};

enum class IsLongFormat : bool
{
    No,
    Yes,
};

struct UnicodeName
{
    explicit constexpr UnicodeName(bytes_view name, IsLongFormat is_long_format) noexcept
        : m_name(name)
        , m_is_long_format(static_cast<bool>(is_long_format))
    {}

    explicit constexpr UnicodeName(UnicodeLongName name) noexcept
        : m_name(name.unicode_name())
        , m_is_long_format(name.is_long_format())
    {}

    explicit constexpr UnicodeName(UnicodeShortName name) noexcept
        : m_name(name.unicode_name())
        , m_is_long_format(name.is_long_format())
    {}

    static constexpr bool is_ascii() noexcept { return false; }
    static constexpr bool is_unicode() noexcept { return true; }

    constexpr bool is_long_format() const noexcept { return m_is_long_format; }

    constexpr bytes_view raw_name() const noexcept { return m_name; }

    constexpr bytes_view unicode_name() const noexcept { return m_name; }

    UnicodeShortName as_short_name() const noexcept
    {
        assert(!is_long_format());
        return UnicodeShortName(UnicodeShortName::view_name_t::assumed(m_name));
    }

    UnicodeLongName as_long_name() const noexcept
    {
        assert(is_long_format());
        return UnicodeLongName(m_name);
    }

private:
    bytes_view m_name;
    bool m_is_long_format;
};

struct AsciiName
{
    // CLIPRDR_SHORT_FORMAT_NAME in ascii format is 31 characters + null terminated
    static constexpr std::size_t max_name_len = format_list_short_name_len - 1;
    using view_name_t = bounded_bytes_view<0, max_name_len>;

    explicit constexpr AsciiName(view_name_t name) noexcept
        : m_name(name)
    {}

    static constexpr bool is_ascii() noexcept { return true; }
    static constexpr bool is_unicode() noexcept { return false; }

    static constexpr bool is_long_format() noexcept { return false; }

    constexpr bytes_view raw_name() const noexcept { return m_name; }

    constexpr view_name_t ascii_name() const noexcept { return m_name; }

private:
    view_name_t m_name;
};

struct GenericName
{
    constexpr GenericName(UnicodeName name) noexcept
        : m_name(name.raw_name())
        , m_is_long_format(name.is_long_format())
        , m_is_ascii(name.is_ascii())
    {}

    constexpr GenericName(UnicodeLongName name) noexcept
        : m_name(name.raw_name())
        , m_is_long_format(name.is_long_format())
        , m_is_ascii(name.is_ascii())
    {}

    constexpr GenericName(UnicodeShortName name) noexcept
        : m_name(name.raw_name())
        , m_is_long_format(name.is_long_format())
        , m_is_ascii(name.is_ascii())
    {}

    constexpr GenericName(AsciiName name) noexcept
        : m_name(name.raw_name())
        , m_is_long_format(name.is_long_format())
        , m_is_ascii(name.is_ascii())
    {}

    constexpr bool is_ascii() const noexcept { return m_is_ascii; }
    constexpr bool is_unicode() const noexcept { return !m_is_ascii; }

    constexpr bool is_long_format() const noexcept { return m_is_long_format; }

    constexpr bytes_view raw_name() const noexcept { return m_name; }

    AsciiName as_ascii_name() const noexcept
    {
        assert(is_ascii());
        return AsciiName(AsciiName::view_name_t::assumed(m_name));
    }

    UnicodeName as_unicode_name() const noexcept
    {
        assert(is_ascii());
        return UnicodeName(m_name, IsLongFormat{is_long_format()});
    }

private:
    bytes_view m_name;
    bool m_is_long_format;
    bool m_is_ascii;
};

void log_format_name(char const* prefix, CbFormatID format_id, GenericName name) noexcept;

struct PredefinedNames
{
    chars_view ascii_name;
    chars_view unicode_name;
};

inline bool operator==(GenericName name, PredefinedNames predefined) noexcept
{
    auto raw_name
        = name.is_unicode()
            ? (name.is_long_format()
                ? predefined.unicode_name
                // short name is truncated to 15 characters + null terminated
                : predefined.unicode_name.first(format_list_short_name_len - 2))
            : predefined.ascii_name
    ;
    return bytes_equal(name.raw_name(), raw_name);
}

inline bool operator==(PredefinedNames predefined, GenericName name) noexcept
{
    return name == predefined;
}

namespace predefined_names
{
    #define REDEMPTION_RDPECLIP_PREDEFINED_NAME(name) \
        PredefinedNames{.ascii_name = name ""_av, .unicode_name = name ""_utf16_le}

    inline constexpr auto file_group_descriptor_w
        = REDEMPTION_RDPECLIP_PREDEFINED_NAME("FileGroupDescriptorW");
    inline constexpr auto preferred_drop_effect
        = REDEMPTION_RDPECLIP_PREDEFINED_NAME("Preferred DropEffect");

    #undef REDEMPTION_RDPECLIP_PREDEFINED_NAME
}


/// 2.2.1 Clipboard PDU Header (CLIPRDR_HEADER).
/// Is present in all clipboard PDUs. It is used to identify the PDU type,
/// specify the length of the PDU, and convey message flags.
PROTOCOL_PARSER_DECL_STRUCT(
    CliprdrHeader,
    mem(
        // u64 for prevent overflow
        uint64_t total_len() const noexcept { return dataLen + pdu_len(); }
    ),
    field(u16_le::as<CbMsgType>, msgType, named(msg_type_to_name)),
    field(u16_le::as<CbMsgFlags>, msgFlags, flags(msg_flags_to_string)),
    /// Size, in bytes, of the data which follows the Clipboard PDU Header
    field(u32_le, dataLen, i)
);


/// 2.2.2.1 Clipboard Capabilities PDU (CLIPRDR_CAPS).
/// Optional PDU used to exchange capability information. It is first sent
/// from the server to the client. Upon receipt of the Monitor Ready PDU,
/// the client sends the Clipboard Capabilities PDU to the server.
/// If this PDU is not sent by a Remote Desktop Protocol: Clipboard Virtual
/// Channel Extension endpoint, it is assumed that the endpoint is using
/// the default values for each capability field.
///
/// The msgType field of the Clipboard PDU Header MUST be set to
/// CB_CLIP_CAPS (0x0007), while the msgFlags field MUST be set to 0x0000.
PROTOCOL_PARSER_DECL_STRUCT(
    CliprdrCapabilities,
    mem(
        static const CbMsgType msg_type = CbMsgType::ClipCaps;
        static const bool msg_flags_required = false;
    ),
    /// Number of CLIPRDR_CAPS_SETs, present in the capabilitySets field.
    field(u16_le, cCapabilitiesSets, i),
    pad(2)
);


/// 2.2.2.1.1 Capability Set (CLIPRDR_CAPS_SET).
/// Used to wrap capability set data and to specify the type and size
/// of this data exchanged between the client and the server.
/// All capability sets conform to this basic structure.
PROTOCOL_PARSER_DECL_STRUCT(
    CliprdrCapabilitiesSet,
    field(u16_le::as<CbCapabilityType>, capabilitySetType, i),
    /// Specifies the combined length, in bytes, of the capabilitySetType,
    /// lengthCapability and capabilityData fields.
    field(u16_le, lengthCapability, i)
);


/// 2.2.2.1.1.1 General Capability Set (CLIPRDR_GENERAL_CAPABILITY).
/// Is used to advertise general clipboard settings.
PROTOCOL_PARSER_DECL_STRUCT(
    CliprdrGeneralCapability,
    /// Specifies the Remote Desktop Protocol: Clipboard Virtual Channel
    /// Extension version number. This field is for informational purposes
    /// and MUST NOT be used to make protocol capability decisions.
    /// The actual features supported are specified in the generalFlags field.
    field(u32_le, version, no),
    field(u32_le::as<CbCapabilityFlags>, generalFlags, flags(capability_flags_to_string))
);


// 2.2.2.2 Server Monitor Ready PDU (CLIPRDR_MONITOR_READY)
// Is sent from the server to the client to indicate that the server is
// initialized and ready. This PDU is transmitted by the server after it
// has sent the Clipboard Capabilities PDU to the client.
//
// The msgType field of the Clipboard PDU Header MUST be set to
// CB_MONITOR_READY (0x0001), while the msgFlags field MUST be set to 0x0000.
// struct ServerMonitorReady {};
inline constexpr auto monitor_ready = "\1\0""\0\0""\0\0\0\0"_av;

// 2.2.2.3 Client Temporary Directory PDU (CLIPRDR_TEMP_DIRECTORY)
// Is an optional PDU sent from the client to the server. This PDU informs the
// server of a location on the client file system that MUST be used to deposit
// files being copied to the client.
// The location MUST be accessible by the server to be useful. Section 3.1.1.3
// specifies how direct file access impacts file copy and paste. This PDU is
// sent by the client after receiving the Monitor Ready PDU.
// PROTOCOL_PARSER_DECL_STRUCT(
//     ClientTemporaryDirectory,
//     /// Contains a null-terminated string that represents the directory on
//     /// the client that MUST be used to store temporary clipboard related
//     /// information. The supplied path MUST be absolute and relative to the
//     /// local client system, for example, "c:\temp\clipdata". Any space not
//     /// used in this field SHOULD be filled with null characters.
//     (view<520>, wszTempDir, U)
// );


/// 2.2.3.1 Format List PDU (CLIPRDR_FORMAT_LIST)
/// Is sent by either the client or the server when its local system clipboard
/// is updated with new clipboard data. This PDU contains the Clipboard Format
/// ID and name pairs of the new Clipboard Formats on the clipboard.
///
/// The msgType field of the Clipboard PDU Header MUST be set to CB_FORMAT_LIST
/// (0x0002), while the msgFlags field MUST be set to 0x0000 or CB_ASCII_NAMES
/// (0x0004) depending on the type of data present in the formatListData field.
///
/// \tparam ProcessFormat [](CbFormatID, AsciiName | UnicodeShortName | UnicodeLongName) -> bool|void
// TODO parsing don't support multi-packet
template<class ProcessFormat>
void format_list_extract(
    InStream& in_stream,
    CbCapabilityFlags general_flags,
    CbMsgFlags msg_flags,
    ProcessFormat&& process_format)
{
    #define REDEMPTION_IF_OR_CONTINUE(exp) do {               \
            if constexpr (std::is_void_v<decltype(exp)>) exp; \
            else if (!exp) break;                             \
        } while (0)

    // 2.2.3.1.2 Long Format Names (CLIPRDR_LONG_FORMAT_NAMES)
    if (flags_any(general_flags, CbCapabilityFlags::UseLongFormatNames))
    {
        // 2.2.3.1.2.1 Long Format Name (CLIPRDR_LONG_FORMAT_NAME)
        //
        // formatId (4 bytes): An unsigned, 32-bit integer that specifies the
        //      Clipboard Format ID.
        //
        // wszFormatName (variable): A variable length null-terminated Unicode
        //      string name that contains the Clipboard Format name. Not all
        //      Clipboard Formats have a name; in such cases, the formatName
        //      field MUST consist of a single Unicode null character.

        // formatId(4) + wszFormatName with null character(min=2)
        constexpr size_t min_pdu_len = 6;

        while (in_stream.in_check_rem(min_pdu_len))
        {
            auto format_id = CbFormatID{in_stream.in_uint32_le()};

            // null character is dropped and not checked
            auto byte_len = UTF16ByteLen(in_stream.remaining_bytes().drop_back(2));
            // skip name and null character ; keep name without null character
            auto name = in_stream.in_skip_bytes(byte_len + 2).drop_back(2);

            REDEMPTION_IF_OR_CONTINUE(process_format(format_id, UnicodeLongName(name)));
        }
    }
    // 2.2.3.1.1 Short Format Names (CLIPRDR_SHORT_FORMAT_NAMES)
    else
    {
        // 2.2.3.1.1.1 Short Format Name (CLIPRDR_SHORT_FORMAT_NAME)
        //
        // formatId (4 bytes): An unsigned, 32-bit integer specifying the
        //      Clipboard Format ID.
        //
        // formatName (32 bytes): A 32-byte block containing the null-terminated
        //      name assigned to the Clipboard Format (32 ASCII 8 characters or
        //      16 Unicode characters). If the name does not fit, it MUST be
        //      truncated. Not all Clipboard Formats have a name, and in that
        //      case the formatName field MUST contain only zeros.

        constexpr size_t pdu_len = format_list_short_name_len + 4 /* format_id */;

        if (flags_any(msg_flags, CbMsgFlags::AsciiNames))
        {
            while (in_stream.in_check_rem(pdu_len))
            {
                auto format_id = CbFormatID{in_stream.in_uint32_le()};

                auto data = in_stream.in_skip_bytes(format_list_short_name_len);
                // null character is dropped and not checked
                auto name = data.first(strnlen(data.as_charp(), AsciiName::max_name_len));
                auto bounded_name = AsciiName::view_name_t::assumed(name);

                REDEMPTION_IF_OR_CONTINUE(process_format(format_id, AsciiName(bounded_name)));
            }
        }
        else
        {
            while (in_stream.in_check_rem(pdu_len))
            {
                auto format_id = CbFormatID{in_stream.in_uint32_le()};

                auto data = in_stream.in_skip_bytes(format_list_short_name_len);
                // null character is dropped and not checked
                auto name = data.first(UTF16ByteLen(data.drop_back(2)));
                auto bounded_name = UnicodeShortName::view_name_t::assumed(name);

                REDEMPTION_IF_OR_CONTINUE(process_format(
                    format_id,
                    UnicodeShortName(bounded_name)
                ));
            }
        }
    }

    #undef REDEMPTION_IF_OR_CONTINUE
}

struct FormatListSerializer
{
    struct LongFormatItem
    {
        static std::size_t required_size(UnicodeLongName unicode_name) noexcept
        {
            return 4  // format_id
                 + 2  // null-character
                 + unicode_name.raw_name().size();
        }

        static void serialize_unchecked(
            OutStream& out_stream,
            CbFormatID format_id,
            UnicodeLongName unicode_name) noexcept
        {
            out_stream.out_uint32_le(underlying_cast(format_id));
            out_stream.out_copy_bytes(unicode_name.raw_name());
            // null character
            out_stream.out_uint16_le(0);
        }
    };

    struct ShortFormatItem
    {
        static std::size_t required_size() noexcept
        {
            return 4 + format_list_short_name_len; // format_id + name
        }

        static void serialize_unchecked(
            OutStream& out_stream,
            CbFormatID format_id,
            bytes_view name) noexcept
        {
            assert(name.size() <= format_list_short_name_len);
            out_stream.out_uint32_le(underlying_cast(format_id));
            out_stream.out_copy_bytes(name);
            out_stream.out_clear_bytes(format_list_short_name_len - name.size());
        }
    };

    static bool serialize(
        OutStream& out_stream,
        CbFormatID format_id,
        UnicodeLongName unicode_name) noexcept
    {
        if (out_stream.has_room(LongFormatItem::required_size(unicode_name)))
        {
            LongFormatItem::serialize_unchecked(out_stream, format_id, unicode_name);
            return true;
        }
        return false;
    }

    static bool serialize(
        OutStream& out_stream,
        CbFormatID format_id,
        AsciiName ascii_name) noexcept
    {
        if (out_stream.has_room(ShortFormatItem::required_size()))
        {
            ShortFormatItem::serialize_unchecked(out_stream, format_id, ascii_name.raw_name());
            return true;
        }
        return false;
    }

    static bool serialize(
        OutStream& out_stream,
        CbFormatID format_id,
        UnicodeShortName unicode_name) noexcept
    {
        if (out_stream.has_room(ShortFormatItem::required_size()))
        {
            ShortFormatItem::serialize_unchecked(out_stream, format_id, unicode_name.raw_name());
            return true;
        }
        return false;
    }
};

bool format_list_long_format_item_serialize(
    OutStream& out_stream,
    CbFormatID format_id,
    bytes_view unicode_name);

bool format_list_short_ascii_format_item_serialize(
    OutStream& out_stream,
    CbFormatID format_id,
    bytes_view ascii_name);

bool format_list_short_unicode_format_item_serialize(
    OutStream& out_stream,
    CbFormatID format_id,
    bytes_view unicode_name);


inline constexpr auto format_list_text_in_long_format_with_header
  = "\2\0""\0\0""\x06\0\0\0" "\x1\0\0\0""\0\0"_av;
inline constexpr auto format_list_unicode_in_long_format_with_header
  = "\2\0""\0\0""\x06\0\0\0" "\xD\0\0\0""\0\0"_av;
inline constexpr auto format_list_text_in_short_format_with_header
  = "\2\0""\4\0""\x24\0\0\0" "\x1\0\0\0"
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"_av;
inline constexpr auto format_list_unicode_in_short_format_with_header
  = "\2\0""\4\0""\x24\0\0\0" "\xD\0\0\0"
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"_av;

inline constexpr auto custom_file_group_descriptor_w_id = CbFormatID{42145}; // 0xa4a1 (random value)
inline constexpr auto
    format_list_custom_file_group_descriptor_w_in_long_format_with_header
  = "\2\0""\0\0""\x2e\0\0\0"
    "\xa1\xa4\0\0""F\0i\0l\0e\0G\0r\0o\0u\0p\0D\0e\0s\0c\0r\0i\0p\0t\0o\0r\0W\0\0\0"_av;


/// 2.2.3.2 Format List Response PDU (FORMAT_LIST_RESPONSE)
/// Is sent as a reply to the Format List PDU. It is used to indicate whether
/// processing of the Format List PDU was successful.
///
/// The msgType field of the Clipboard PDU Header MUST be set to
/// CB_FORMAT_LIST_RESPONSE (0x0003). The CB_RESPONSE_OK (0x0001) or
/// CB_RESPONSE_FAIL (0x0002) flag MUST be set in the msgFlags field
/// of the Clipboard PDU Header.
PROTOCOL_PARSER_DECL_STRUCT(
    FormatListResponse,
    mem(
        static const CbMsgType msg_type = CbMsgType::FormatListResponse;
        static const bool msg_flags_required = true;
    )
);

inline constexpr auto format_list_response_ok_with_header   = "\3\0""\1\0""\0\0\0\0"_av;
inline constexpr auto format_list_response_fail_with_header = "\3\0""\2\0""\0\0\0\0"_av;


/// 2.2.4.1 Lock Clipboard Data PDU (CLIPRDR_LOCK_CLIPDATA).
/// Can be sent at any point in time after the clipboard capabilities and
/// temporary directory have been exchanged in the Clipboard Initialization
/// Sequence (section 1.3.2.1) by a Local Clipboard Owner (section 1.3.2.2.1).
/// The purpose of this PDU is to request that the Shared Clipboard Owner
/// (section 1.3.2.2.1) retain all File Stream (section 1.3.1.1.5) data on the
/// clipboard until the Unlock Clipboard Data PDU (section 2.2.4.2) is
/// received. This ensures that File Stream data can be requested by the Local
/// Owner in a subsequent File Contents Paste Sequence (section 1.3.2.2.3) by
/// using the File Contents Request PDU (section 2.2.5.3) even when the Shared
/// Owner clipboard has changed and the File Stream data is no longer available.
///
/// The msgType field of the Clipboard PDU Header MUST be set to
/// CB_LOCK_CLIPDATA (0x000A), while the msgFlags field MUST be set to 0x0000.
PROTOCOL_PARSER_DECL_STRUCT(
    LockClipboardData,
    mem(
        static const CbMsgType msg_type = CbMsgType::LockClipdata;
        static const bool msg_flags_required = false;
    ),
    /// Is used to tag File Stream data on the Shared Owner clipboard so that
    /// it can be requested in a subsequent File Contents Request PDU
    /// (section 2.2.5.3).
    field(u32_le::as<ClipDataId>, clipDataId, i)
);


/// 2.2.4.2 Unlock Clipboard Data PDU (CLIPRDR_UNLOCK_CLIPDATA).
/// Can be sent at any point in time after the Clipboard Initialization
/// Sequence (section 1.3.2.1) by a Local Clipboard Owner (section 1.3.2.2.1).
/// The purpose of this PDU is to notify the Shared Clipboard Owner
/// (section 1.3.2.2.1) that File Stream data that was locked in response to
/// the Lock Clipboard Data PDU (section 2.2.4.1) can be released.
///
/// The msgType field of the Clipboard PDU Header MUST be set to
/// CB_UNLOCK_CLIPDATA (0x000B), while the msgFlags field MUST be set to 0x0000.
PROTOCOL_PARSER_DECL_STRUCT(
    UnlockClipboardData,
    mem(
        static const CbMsgType msg_type = CbMsgType::UnlockClipdata;
        static const bool msg_flags_required = false;
    ),
    /// Identifies the File Stream data that was locked by the Lock Clipboard
    /// Data PDU (section 2.2.4.1) and can now be released.
    field(u32_le::as<ClipDataId>, clipDataId, i)
);


/// 2.2.5.1 Format Data Request PDU (CLIPRDR_FORMAT_DATA_REQUEST).
/// Is sent by the recipient of the Format List PDU. It is used to request the
/// data for one of the formats that was listed in the Format List PDU.
///
/// The msgType field of the Clipboard PDU Header MUST be set to
/// CB_FORMAT_DATA_REQUEST (0x0004), while the msgFlags field MUST be set to 0x0000.
PROTOCOL_PARSER_DECL_STRUCT(
    FormatDataRequest,
    mem(
        static const CbMsgType msg_type = CbMsgType::FormatDataRequest;
        static const bool msg_flags_required = false;
    ),
    /// Specifies the Clipboard Format ID of the clipboard data. The Clipboard
    /// Format ID MUST be one listed previously in the Format List PDU.
    field(u32_le::as<CbFormatID>, requestedFormatId, named(format_id_to_string))
);

inline constexpr auto format_data_request_text_with_header
    = "\4\0""\0\0""\4\0\0\0" "\x1\0\0\0"_av;
inline constexpr auto format_data_request_unicode_with_header
    = "\4\0""\0\0""\4\0\0\0" "\xD\0\0\0"_av;


/// 2.2.5.2 Format Data Response PDU (CLIPRDR_FORMAT_DATA_RESPONSE)
/// Is sent as a reply to the Format Data Request PDU. It is used to indicate
/// whether processing of the Format Data Request PDU was successful. If the
/// processing was successful, the Format Data Response PDU includes the
/// contents of the requested clipboard data.
///
/// The msgType field of the Clipboard PDU Header MUST be set to
/// CB_FORMAT_DATA_RESPONSE (0x0005). The CB_RESPONSE_OK (0x0001) or
/// CB_RESPONSE_FAIL (0x0002) flag MUST be set in the msgFlags field
/// of the Clipboard PDU Header structure.
PROTOCOL_PARSER_DECL_STRUCT(
    FormatDataResponseWithoutData,
    mem(
        static const CbMsgType msg_type = CbMsgType::FormatDataResponse;
        static const bool msg_flags_required = true;
    )
    // requestedFormatData (variable): The contents of this field
    // MUST be one of the following types: generic, Packed Metafile
    // Payload, or Packed Palette Payload.
);

inline constexpr auto format_data_response_fail_with_header = "\5\0""\2\0""\0\0\0\0"_av;


/// 2.2.5.2.3 Packed File List (CLIPRDR_FILELIST)
/// The CLIPRDR_FILELIST structure is used to describe a list of files,
/// each file in the list being represented by a File Descriptor
/// (section 2.2.5.2.3.1).
PROTOCOL_PARSER_DECL_STRUCT(
    FileListWithoutArray,
    mem(
        // no static msg_type, is in requestedFormatData of FormatDataResponse

        static const uint32_t max_items = 7255012u;
        // = (~uint32_t{}
        //    - CliprdrHeader::pdu_len()
        //    - FormatDataResponseWithoutData::pdu_len()
        //    - FileListWithoutArray::pdu_len()
        //   )
        //   / FileDescriptor::pdu_len()
    ),
    /// Specifies the number of entries in the fileDescriptorArray field.
    field(u32_le, cItems, i)
    // fileDescriptorArray (variable): An array of File Descriptors
    // (section 2.2.5.2.3.1). The number of elements in the array is specified
    // by the cItems field.
);


/// 2.2.5.2.3.1 File Descriptor (CLIPRDR_FILEDESCRIPTOR)
PROTOCOL_PARSER_DECL_STRUCT(
    FileDescriptor,
    mem(
        uint64_t file_size() const noexcept
        {
            return fileSizeLow + (uint64_t{fileSizeHigh} << 32);
        }

        void set_file_size(uint64_t size) noexcept
        {
            fileSizeLow = static_cast<decltype(fileSizeLow)>(size);
            fileSizeHigh = size >> 32;
        }

        // no static data, is in fileDescriptorArray of FileListWithoutArray
    ),
    /// specifies the number of entries in the fileDescriptorArray field.
    field(u32_le::as<FileDescriptorFlags>, flags, flags(file_descriptor_flags_to_string)),
    /// MUST be initialized with zeros when sent and MUST be ignored on receipt.
    pad(32),
    /// Integer that specifies file attribute flags.
    field(u32_le::as<FileAttributeFlags>, fileAttributes, flags(file_attribute_flags_to_string)),
    /// MUST be initialized with zeros when sent and MUST be ignored on receipt.
    pad(16),
    /// time of the last write operation on the file.
    field(u64_le::as<WinNtUTime>, lastWriteTime, i),
    // Contains the most significant 4 bytes of the file size.
    field(u32_le, fileSizeHigh, i),
    // Contains the least significant 4 bytes of the file size.
    field(u32_le, fileSizeLow, i),
    /// A null-terminated 260 character Unicode string that contains
    /// the name of the file.
    field(null_terminated_utf16<520>, unicodeFileName, utf16_to_utf8(384))
);


struct FileDescriptorFileSize
{
    uint32_t high;
    uint32_t low;

    FileDescriptorFileSize(uint64_t file_size) noexcept
      : high(file_size >> 32)
      , low(static_cast<uint32_t>(file_size))
    {}
};

PROTOCOL_PARSER_DECL_STRUCT(
    FileDescriptorWithoutFileName,
    mem(
        uint64_t file_size() const noexcept
        {
            return fileSizeLow + (uint64_t{fileSizeHigh} << 32);
        }

        void set_file_size(uint64_t size) noexcept
        {
            fileSizeLow = static_cast<decltype(fileSizeLow)>(size);
            fileSizeHigh = size >> 32;
        }

        // no static data, is in fileDescriptorArray of FileListWithoutArray
    ),
    /// specifies the number of entries in the fileDescriptorArray field.
    field(u32_le::as<FileDescriptorFlags>, flags, flags(file_descriptor_flags_to_string)),
    /// MUST be initialized with zeros when sent and MUST be ignored on receipt.
    pad(32),
    /// Integer that specifies file attribute flags.
    field(u32_le::as<FileAttributeFlags>, fileAttributes, flags(file_attribute_flags_to_string)),
    /// MUST be initialized with zeros when sent and MUST be ignored on receipt.
    pad(16),
    /// time of the last write operation on the file.
    field(u64_le::as<WinNtUTime>, lastWriteTime, i),
    // Contains the most significant 4 bytes of the file size.
    field(u32_le, fileSizeHigh, i),
    // Contains the least significant 4 bytes of the file size.
    field(u32_le, fileSizeLow, i)
);

inline constexpr uint16_t file_descriptor_file_name_buffer_size
    = FileDescriptor::pdu_len() - FileDescriptorWithoutFileName::pdu_len();

/// \param cItems See \c FileListWithoutArray.
inline auto make_file_list_response_with_header_without_data(uint32_t nb_items) noexcept
{
    assert(nb_items <= FileListWithoutArray::max_items);

    return protop::MultiPdus{
        CliprdrHeader {
            .msgType = FormatDataResponseWithoutData::msg_type,
            .msgFlags = CbMsgFlags::ResponseOk,
            .dataLen = FormatDataResponseWithoutData::pdu_len()
                        + FileListWithoutArray::pdu_len()
                        + nb_items * FileDescriptor::pdu_len(),
        },
        FormatDataResponseWithoutData {
        },
        FileListWithoutArray {
            .cItems = nb_items,
        },
    };
}


struct FileListParser
{
    bool ok;
    uint16_t new_buffer_offset;

    /// \pre buffer_offset < FileDescriptor::pdu_len()
    /// \post new_buffer_offset < FileDescriptor::pdu_len()
    static FileListParser parse(
        bytes_view data,
        uint16_t buffer_offset,
        writable_sized_bytes_view<FileDescriptor::pdu_len()> buffer,
        FunctionRef<bool(FileDescriptor &)> fd_fn
    );
};

/// 2.2.5.3 File Contents Request PDU (CLIPRDR_FILECONTENTS_REQUEST).
/// Is sent by the recipient of the Format List PDU. It is used to request
/// either the size of a remote file copied to the clipboard or a portion
/// of the data in the file.
///
/// The msgType field of the Clipboard PDU Header MUST be set to
/// CB_FILECONTENTS_REQUEST (0x0008), while the msgFlags field MUST be set to 0x0000.
PROTOCOL_PARSER_DECL_STRUCT(
    FileContentsRequest,
    mem(
        static const CbMsgType msg_type = CbMsgType::FileContentsRequest;
        static const bool msg_flags_required = false;

        uint64_t position() const noexcept
        {
            return nPositionLow + (uint64_t{nPositionHigh} << 32);
        }

        void set_position(uint64_t size) noexcept
        {
            nPositionLow = static_cast<decltype(nPositionLow)>(size);
            nPositionHigh = size >> 32;
        }
    ),
    /// Used to associate the File Contents Request PDU with the corresponding
    /// File Contents Response PDU. The File Contents Response PDU is sent as
    /// a reply and contains an identical value in the streamId field.
    field(u32_le::as<CbStreamId>, streamId, i),
    /// Specifies the numeric ID of the remote file that is the target of the
    /// File Contents Request PDU. This field is used as an index that
    /// identifies a particular file in a File List. This File List SHOULD
    /// have been obtained as clipboard data in a prior Format Data Request
    /// PDU and Format Data Response PDU exchange.
    field(u32_le::as<CbLindex>, lindex, i),
    /// that specifies the type of operation to be performed by the recipient.
    field(u32_le::as<CbFileContentsType>, dwFlags, named(file_contents_type_to_string)),
    /// specifies the low bytes of the offset into the remote file, identified
    /// by the lindex field, from where the data needs to be extracted to
    /// satisfy a FILECONTENTS_RANGE operation. This field SHOULD be set to a
    /// value less than 2,147,483,648 unless the recipient of the
    /// FILECONTENTS_RANGE operation has specified support for huge files by
    /// setting the CB_HUGE_FILE_SUPPORT_ENABLED (0x00000020) flag in the
    /// General Capability Set (section 2.2.2.1.1.1) of the Clipboard
    /// Capabilities PDU (section 2.2.2.1).
    /// The operating systems Windows 10 v1803 operating system, Windows
    /// Server v1803 operating system, Windows 10 v1809 operating system,
    /// and Windows Server 2019 support values larger than 2,147,483,647
    /// and less than or equal to 4,294,967,295 in the nPositionLow field
    /// irrespective of the advertised huge file support.
    field(u32_le, nPositionLow, i),
    /// Specifies the high bytes of the offset into the remote file,
    /// identified by the lindex field, from where the data needs to be
    /// extracted to satisfy a FILECONTENTS_RANGE operation. This field
    /// SHOULD be set to zero unless the recipient of the FILECONTENTS_RANGE
    /// operation has specified support for huge files by setting the
    /// CB_HUGE_FILE_SUPPORT_ENABLED (0x00000020) flag in the General
    /// Capability Set (section 2.2.2.1.1.1) of the Clipboard Capabilities
    /// PDU (section 2.2.2.1).
    field(u32_le, nPositionHigh, i),
    /// Specifies the size, in bytes, of the data to retrieve. For a
    /// FILECONTENTS_SIZE operation, this field MUST be set to 0x00000008.
    /// In the case of a FILECONTENTS_RANGE operation, this field contains
    /// the maximum number of bytes to read from the remote file.
    field(u32_le, cbRequested, i),
    /// Identifies File Stream data which was tagged in a prior Lock Clipboard
    /// Data PDU (section 2.2.4.1).
    field(optional<protop::u32_le::as<ClipDataId>>, clipDataId, i)
);

constexpr FileContentsRequest make_file_contents_size_request(
    CbStreamId streamId,
    CbLindex lindex,
    ClipDataId clipDataId
) noexcept
{
    return {
        .streamId = streamId,
        .lindex = lindex,
        .dwFlags = CbFileContentsType::Size,
        .nPositionLow = 0,
        .nPositionHigh = 0,
        .cbRequested = 8,
        .clipDataId = clipDataId,
    };
}


/// 2.2.5.4 File Contents Response PDU (CLIPRDR_FILECONTENTS_RESPONSE).
/// Is sent as a reply to the File Contents Request PDU. It is used to
/// indicate whether processing of the File Contents Request PDU was
/// successful. If the processing was successful, the File Contents Response
/// PDU includes either a file size or extracted file data, based on the
/// operation requested in the corresponding File Contents Request PDU.
///
/// The msgType field of the Clipboard PDU Header MUST be set to
/// CB_FILECONTENTS_RESPONSE (0x0009). The CB_RESPONSE_OK (0x0001)
/// or CB_RESPONSE_FAIL (0x0002) flag MUST be set in the msgFlags
/// field of the Clipboard PDU Header.
PROTOCOL_PARSER_DECL_STRUCT(
    FileContentsResponseWithoutData,
    mem(
        static const CbMsgType msg_type = CbMsgType::FileContentsResponse;
        static const bool msg_flags_required = true;
    ),
    /// ID used to associate the File Contents Response PDU with the
    /// corresponding File Contents Request PDU. The File Contents Request
    /// PDU that triggered the response MUST contain an identical value in
    /// the streamId field.
    field(u32_le::as<CbStreamId>, streamId, i)
    // requestedFileContentsData (variable):  If the response is to a
    // FILECONTENTS_SIZE (0x00000001) operation, the requestedFileContentsData
    // field holds a 64-bit, unsigned integer containing the size of the file.
    // In the case of a FILECONTENTS_RANGE (0x00000002) operation, the
    // requestedFileContentsData field contains a byte-stream of data
    // extracted from the file
);


/// Is a CLIPRDR_FILECONTENTS_RESPONSE with FILECONTENTS_SIZE requested.
PROTOCOL_PARSER_DECL_STRUCT(
    FileContentsResponseSize,
    mem(
        static const CbMsgType msg_type = CbMsgType::FileContentsResponse;
        static const bool msg_flags_required = true;
    ),
    /// see FileContentsResponseWithoutData.
    field(u32_le::as<CbStreamId>, streamId, i),
    /// The size of the file.
    field(u64_le, fileSize, i)
);

/// \param stream_id See \c FileContentsResponseWithoutData.
inline auto make_file_contents_response_data_with_header(
    CbStreamId stream_id, uint32_t data_len
) noexcept
{
    auto total_data_len = data_len + FileContentsResponseWithoutData::pdu_len();
    assert(total_data_len > data_len);

    return protop::MultiPdus {
        CliprdrHeader {
            .msgType = FileContentsResponseWithoutData::msg_type,
            .msgFlags = CbMsgFlags::ResponseOk,
            .dataLen = total_data_len,
        },
        FileContentsResponseWithoutData {
            stream_id
        },
    };
}

/// \param stream_id See \c FileContentsResponseWithoutData.
inline auto make_file_contents_response_error(CbStreamId stream_id) noexcept
{
    auto total_data_len = FileContentsResponseWithoutData::pdu_len();

    return protop::MultiPdus {
        CliprdrHeader {
            .msgType = FileContentsResponseWithoutData::msg_type,
            .msgFlags = CbMsgFlags::ResponseFail,
            .dataLen = total_data_len,
        },
        FileContentsResponseWithoutData {
            stream_id
        },
    };
}


/// Build packet
//@{
namespace detail
{
    template<class Pkt>
    struct get_msg_len
    {
        static const uint32_t value = Pkt::pdu_len();
    };

    template<>
    struct get_msg_len<FileContentsRequest>
    {
        static const uint32_t value = FileContentsRequest::pdu_max_len;
    };
}


template<class Msg>
void unchecked_write_cb_packet_with_header(
    OutStream & out_stream, CbMsgFlags msg_flags, Msg const& msg
) noexcept
{
    auto pdu_len = detail::get_msg_len<Msg>::value;
    assert(CliprdrHeader::pdu_len() + pdu_len <= out_stream.tailroom());

    CliprdrHeader{ Msg::msg_type, msg_flags, pdu_len }.write_unchecked(out_stream);
    msg.write_unchecked(out_stream);
}

template<class Msg>
    requires (!Msg::msg_flags_required)
void unchecked_write_cb_packet_with_header(OutStream & out_stream, Msg const& msg) noexcept
{
    unchecked_write_cb_packet_with_header(out_stream, CbMsgFlags::None, msg);
}


template<class Msg>
[[nodiscard]] bool write_cb_packet_with_header(
    OutStream & out_stream, CbMsgFlags msg_flags, Msg const& msg
) noexcept
{
    auto pdu_len = detail::get_msg_len<Msg>::value;
    if (CliprdrHeader::pdu_len() + pdu_len > out_stream.tailroom())
    {
        return false;
    }

    CliprdrHeader{ Msg::msg_type, msg_flags, pdu_len }.write_unchecked(out_stream);
    msg.write_unchecked(out_stream);
    return true;
}

template<class Msg>
    requires (!Msg::msg_flags_required)
[[nodiscard]] bool write_cb_packet_with_header(OutStream & out_stream, Msg const& msg) noexcept
{
    return write_cb_packet_with_header(out_stream, CbMsgFlags::None, msg);
}


template<class Msg>
auto make_cb_packet_with_header(CbMsgFlags msg_flags, Msg const& msg) noexcept
{
    constexpr auto pdu_len = detail::get_msg_len<Msg>::value;
    std::array<uint8_t, pdu_len + CliprdrHeader::pdu_len()> buf;
    OutStream out_stream{buf};

    CliprdrHeader{ Msg::msg_type, msg_flags, pdu_len }.write_unchecked(out_stream);
    msg.write_unchecked(out_stream);

    return buf;
}

template<class Msg>
    requires (!Msg::msg_flags_required)
auto make_cb_packet_with_header(Msg const& msg) noexcept
{
    return make_cb_packet_with_header(CbMsgFlags::None, msg);
}


inline constexpr auto make_general_capability_with_header(CbCapabilityFlags caps) noexcept
{
    std::array<uint8_t, 24> pdu {};
    pdu[0] = 7; // ClipCaps
    pdu[4] = 0x10; // pdu length
    pdu[8] = 1; // cCapabilitiesSets
    pdu[12] = 1; // CbCapabilityType::General
    pdu[14] = 12; // lengthCapability
    pdu[16] = 1; // version
    pdu[20] = static_cast<uint8_t>(caps);
    return pdu;
}
//@}


/// Extract general_flags from CbMsgType::ClipCaps.
struct GeneralFlagsCapability
{
    // false when multi fragment or too few data
    bool ok;
    uint32_t general_flags_or_expected_len;
    char const * ctx;

    static GeneralFlagsCapability parse(bytes_view data) noexcept;
};

// TODO remove RDPECLIP::RecvPredictor ?
// TODO remove format_name.hpp
// TODO remove format_list_extract.hpp
// TODO remove format_list_serialize.hpp

// TODO remove
struct FormatListItem
{
    CbFormatID format_id;
    bool is_ascii;
    // TODO bounded + inline data to reduce struct len
    bytes_view unicode_or_ascii_format_name;
};


// 2.2.1 Clipboard PDU Header (CLIPRDR_HEADER)
struct CliprdrReader
{
    struct [[nodiscard]] Result
    {
        enum class ErrorCode : uint8_t
        {
            /// no error
            Ok,
            /// Parsing error.
            InsufficientData,
            /// \c CliprdrHeader::total_len() is greater that \c total_len
            /// when \c First flag is encountered.
            TotalLenTooShort,
            /// Incoherence with \c CliprdrHeader::dataLen when \c Last flag is encountered.
            DataTruncated,
        };

        ErrorCode ec;
        uint32_t partial_data_len;
        uint8_t const * partial_data_ptr;

        bytes_view partial_data() const noexcept
        {
            return {partial_data_ptr, partial_data_len};
        }

        explicit operator bool () const noexcept
        {
            return ec == ErrorCode::Ok;
        }
    };

    /// Read chunk without checking the consistency of \c channel_flags.
    /// A new PDU should start with CHANNEL_FLAG_FIRST and end with CHANNEL_FLAG_LAST.
    /// Since consistency is not checked, a new PDU can start without the previous one
    /// having indicated CHANNEL_FLAG_LAST.
    Result read(bytes_view chunk, uint32_t total_len, ChannelFlags channel_flags) noexcept;

    uint32_t remaining_data_len() const noexcept
    {
        return m_header.dataLen;
    }

    CbMsgType last_msg_type() const noexcept
    {
        return m_header.msgType;
    }

    CbMsgFlags last_msg_flags() const noexcept
    {
        return m_header.msgFlags;
    }

private:
    CliprdrHeader m_header {};
};


// TODO remove ?
/// Check if a client msg type should be ignored or not.
struct CliprdrExpectedClientPDUChecker
{
    bool is_expected_msg(CbMsgType msg_type) const noexcept;
    void set_next_transition(CbMsgType msg_type) noexcept;

    static_string<189> transitions_as_string() const noexcept;

private:
    std::underlying_type_t<CbMsgType> m_transitions {};
};

// TODO remove ?
/// Check if a server msg type should be ignored or not.
struct CliprdrExpectedServerPDUChecker
{
    bool is_expected_msg(CbMsgType msg_type) const noexcept;
    void set_next_transition(CbMsgType msg_type) noexcept;

    void start() noexcept;

    static_string<189> transitions_as_string() const noexcept;

private:
    std::underlying_type_t<CbMsgType> m_transitions {};
};

} // namespace VNC

template<>
inline constexpr auto is_ok_v<VNC::CliprdrReader::Result::ErrorCode>
    = VNC::CliprdrReader::Result::ErrorCode::Ok;
