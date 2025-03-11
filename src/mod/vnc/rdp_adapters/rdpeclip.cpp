/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <cstdint>

#include "core/channel_list.hpp"
#include "core/RDP/clipboard.hpp"
#include "mod/vnc/rdp_adapters/rdpeclip.hpp"
#include "utils/sugar/numerics/safe_conversions.hpp"
#include "utils/sugar/int_to_chars.hpp"
#include "utils/sugar/static_string_from_enum_flags.hpp"
#include "utils/sugar/overload.hpp"
#include "utils/sugar/bytes_copy.hpp"
#include "utils/stream.hpp"
#include "utils/mathutils.hpp"
#include "utils/static_string.hpp"

// TODO add logs

namespace
{
    constexpr chars_view msg_type_to_name_av(VNC::CbMsgType msg_type) noexcept
    {
        switch (msg_type)
        {
            case VNC::CbMsgType::MonitorReady:         return "MONITOR_READY"_av;
            case VNC::CbMsgType::FormatList:           return "FORMAT_LIST"_av;
            case VNC::CbMsgType::FormatListResponse:   return "FORMAT_LIST_RESPONSE"_av;
            case VNC::CbMsgType::FormatDataRequest:    return "FORMAT_DATA_REQUEST"_av;
            case VNC::CbMsgType::FormatDataResponse:   return "FORMAT_DATA_RESPONSE"_av;
            case VNC::CbMsgType::TempDirectory:        return "TEMP_DIRECTORY"_av;
            case VNC::CbMsgType::ClipCaps:             return "CLIP_CAPS"_av;
            case VNC::CbMsgType::FileContentsRequest:  return "FILECONTENTS_REQUEST"_av;
            case VNC::CbMsgType::FileContentsResponse: return "FILECONTENTS_RESPONSE"_av;
            case VNC::CbMsgType::LockClipdata:         return "LOCK_CLIPDATA"_av;
            case VNC::CbMsgType::UnlockClipdata:       return "UNLOCK_CLIPDATA"_av;
        }

        return "<unknown>"_av;
    }
} // anonymous namespace

const char * VNC::msg_type_to_name(CbMsgType msg_type) noexcept
{
    return msg_type_to_name_av(msg_type).data();
}

static_string<50> VNC::msg_flags_to_string(CbMsgFlags msg_flags) noexcept
{
    return StaticStringFromEnumFlags::make<
        "CB_RESPONSE_OK"_name_of(CbMsgFlags::ResponseOk),
        "CB_RESPONSE_FAIL"_name_of(CbMsgFlags::ResponseFail),
        "CB_ASCII_NAMES"_name_of(CbMsgFlags::AsciiNames)
    >(msg_flags);
}

static_string<131> VNC::capability_flags_to_string(CbCapabilityFlags cap_flags) noexcept
{
    return StaticStringFromEnumFlags::make<
        "CB_USE_LONG_FORMAT_NAMES"_name_of(CbCapabilityFlags::UseLongFormatNames),
        "CB_STREAM_FILECLIP_ENABLED"_name_of(CbCapabilityFlags::StreamFileClipEnabled),
        "CB_FILECLIP_NO_FILE_PATHS"_name_of(CbCapabilityFlags::FileClipNoFilePaths),
        "CB_CAN_LOCK_CLIPDATA"_name_of(CbCapabilityFlags::CanLockClipData),
        "CB_HUGE_FILE_SUPPORT_ENABLED"_name_of(CbCapabilityFlags::HugeFileSupportEnabled)
    >(cap_flags);
}

const char * VNC::format_id_to_string(CbFormatID format_id) noexcept
{
    REDEMPTION_DIAGNOSTIC_PUSH()
    REDEMPTION_DIAGNOSTIC_GCC_IGNORE("-Wswitch")
    switch (format_id) {
        case CbFormatID::Text:          return "CF_TEXT";
        case CbFormatID(2):             return "CF_BITMAP";
        case CbFormatID(3):             return "CF_METAFILEPICT";
        case CbFormatID(4):             return "CF_SYLK";
        case CbFormatID(5):             return "CF_DIF";
        case CbFormatID(6):             return "CF_TIFF";
        case CbFormatID(7):             return "CF_OEMTEXT";
        case CbFormatID(8):             return "CF_DIB";
        case CbFormatID(9):             return "CF_PALETTE";
        case CbFormatID(10):            return "CF_PENDATA";
        case CbFormatID(11):            return "CF_RIFF";
        case CbFormatID(12):            return "CF_WAVE";
        case CbFormatID::UnicodeText:   return "CF_UNICODETEXT";
        case CbFormatID(14):            return "CF_ENHMETAFILE";
        case CbFormatID(15):            return "CF_HDROP";
        case CbFormatID(16):            return "CF_LOCALE";
        case CbFormatID(17):            return "CF_DIBV5";
        case CbFormatID(128):           return "CF_OWNERDISPLAY";
        case CbFormatID(129):           return "CF_DSPTEXT";
        case CbFormatID(130):           return "CF_DSPBITMAP";
        case CbFormatID(131):           return "CF_DSPMETAFILEPICT";
        case CbFormatID(142):           return "CF_DSPENHMETAFILE";
        case CbFormatID(512):           return "CF_PRIVATEFIRST";
        case CbFormatID(767):           return "CF_PRIVATELAST";
        case CbFormatID(768):           return "CF_GDIOBJFIRST";
        case CbFormatID(1023):          return "CF_GDIOBJLAST";
    }
    REDEMPTION_DIAGNOSTIC_PUSH()

    return "<unknown>";
}

const char * VNC::file_contents_type_to_string(CbFileContentsType contents_type) noexcept
{
    switch (contents_type) {
        case CbFileContentsType::Range: return "Range";
        case CbFileContentsType::Size: return "Size";
    }
    return "<unknown>";
}

static_string<61> VNC::file_descriptor_flags_to_string(FileDescriptorFlags fd_flags) noexcept
{
    return StaticStringFromEnumFlags::make<
        "FD_ATTRIBUTES"_name_of(FileDescriptorFlags::Attributes),
        "FD_FILESIZE"_name_of(FileDescriptorFlags::FileSize),
        "FD_WRITESTIME"_name_of(FileDescriptorFlags::WriteTime),
        "FD_SHOWPROGRESSUI"_name_of(FileDescriptorFlags::ShowProgressUI)
    >(fd_flags);
}

void VNC::log_format_name(char const* prefix, CbFormatID format_id, GenericName name) noexcept
{
    char buf[256];
    auto utf8_name = name.is_unicode()
        ? UTF16toUTF8_buf(name.raw_name(), make_writable_array_view(buf))
        : name.raw_name();

    LOG(LOG_INFO, "%sFormatName{formatId=%s(%u), formatName=\"%.*s\", long_name=%d, ascii=%d}",
        prefix, format_id_to_string(format_id), format_id,
        static_cast<int>(utf8_name.size()), utf8_name.data(),
        name.is_long_format(), name.is_ascii());
}

VNC::FileListParser VNC::FileListParser::parse(
    bytes_view data,
    uint16_t buffer_offset,
    writable_sized_bytes_view<FileDescriptor::pdu_len()> buffer,
    FunctionRef<bool(FileDescriptor &)> fd_fn)
{
    FileDescriptor fd;

    InStream in_stream(data);

    // previous call has partial data
    if (buffer_offset)
    {
        assert(buffer_offset < FileDescriptor::pdu_len());
        // copy partial pdu in the temporary buffer
        auto remaining_for_pdu = FileDescriptor::pdu_len() - buffer_offset;
        auto partial_cp_len = mmin(remaining_for_pdu, data.size());
        in_stream.in_copy_bytes(buffer.drop_front(buffer_offset).first(partial_cp_len));
        buffer_offset += partial_cp_len;

        // FileDescriptor is in the temporary buffer, read it
        if (buffer_offset == FileDescriptor::pdu_len())
        {
            buffer_offset = 0;

            InStream fd_stream{buffer};
            // failure is impossible
            (void)fd.read(fd_stream);
            if (!fd_fn(fd))
            {
                return {
                    .ok = false,
                    .new_buffer_offset = buffer_offset,
                };
            }
        }
    }

    // read FileDescriptor in data
    while (fd.read(in_stream))
    {
        if (!fd_fn(fd))
        {
            return {
                .ok = false,
                .new_buffer_offset = buffer_offset,
            };
        }
    }

    // copy remaining data to temporary buffer
    assert(in_stream.in_remain() < FileDescriptor::pdu_len());
    buffer_offset += bytes_copy(
        make_writable_array_view(buffer).drop_front(buffer_offset),
        in_stream.remaining_bytes()
    );

    return {
        .ok = true,
        .new_buffer_offset = buffer_offset,
    };
}


VNC::CliprdrReader::Result
VNC::CliprdrReader::read(bytes_view chunk, uint32_t total_len, ChannelFlags channel_flags) noexcept
{
    InStream in_stream { chunk };

    if (flags_any(channel_flags, ChannelFlags::First))
    {
        if (!m_header.read(in_stream)) [[unlikely]]
        {
            return Result {
                .ec = Result::ErrorCode::InsufficientData,
                .partial_data_len {},
                .partial_data_ptr {},
            };
        }

        if (m_header.total_len() > total_len)
        {
            return Result {
                .ec = Result::ErrorCode::TotalLenTooShort,
                .partial_data_len {},
                .partial_data_ptr {},
            };
        }
    }

    // Some implementation append four bytes to the end of clipboard PDUs.
    // These four bytes are not included in the PDU size and must be ignored.
    auto real_chunk_len = mmin(in_stream.in_remain(), m_header.dataLen);
    m_header.dataLen -= real_chunk_len;

    auto res = Result {
        .ec = Result::ErrorCode::Ok,
        .partial_data_len = real_chunk_len,
        .partial_data_ptr = in_stream.in_skip_bytes(real_chunk_len).data(),
    };

    if (flags_any(channel_flags, ChannelFlags::Last))
    {
        if (m_header.dataLen)
        {
            res.ec = Result::ErrorCode::DataTruncated;
        }
    }

    return res;
}


VNC::GeneralFlagsCapability
VNC::GeneralFlagsCapability::parse(bytes_view data) noexcept
{
    VNC::CbCapabilityFlags general_flags {};

    InStream in_stream{data};

    #define READ_OR_RETURN(type, pkt_name)                \
        type pkt_name;                                    \
        do {                                              \
            if (!pkt_name.read(in_stream)) [[unlikely]] { \
                return {false, caps.pdu_len(), #type};    \
            }                                             \
        } while (0)

    READ_OR_RETURN(VNC::CliprdrCapabilities, caps);

    for (unsigned i = 0; i < caps.cCapabilitiesSets; ++i)
    {
        READ_OR_RETURN(VNC::CliprdrCapabilitiesSet, caps_set);

        if (caps_set.lengthCapability < caps_set.pdu_len()  // inclusive size
         || caps_set.lengthCapability - caps_set.pdu_len() > in_stream.in_remain()) [[unlikely]]
        {
            return {false, caps_set.lengthCapability, "VNC::CliprdrCapabilitiesSet::lengthCapability"};
        }

        unsigned nb_skip = 0;

        if (caps_set.capabilitySetType == VNC::CbCapabilityType::General)
        {
            READ_OR_RETURN(VNC::CliprdrGeneralCapability, general_cap);
            general_flags |= general_cap.generalFlags;
            nb_skip = general_cap.pdu_len();
        }

        in_stream.in_skip_bytes(caps_set.lengthCapability - caps_set.pdu_len() - nb_skip);
    }

    #undef READ_OR_RETURN

    return {true, underlying_cast(general_flags), ""};
}


namespace
{
    using MsgTransitions = unsigned __int128;

    constexpr int shift_part_transitions(VNC::CbMsgType msg) noexcept
    {
        return (underlying_cast(msg) - 1) * 12;
    }

    constexpr unsigned bit_transitions(VNC::CbMsgType msg) noexcept
    {
        return 1u << underlying_cast(msg);
    }

    constexpr MsgTransitions define_transitions(
        VNC::CbMsgType msg,
        std::initializer_list<VNC::CbMsgType> transitions
    )
    {
        uint32_t table = 0;
        for (auto msg_transition : transitions)
        {
            table |= bit_transitions(msg_transition);
        }
        return MsgTransitions{table} << shift_part_transitions(msg);
    }

    uint16_t get_transitions(VNC::CbMsgType msg, MsgTransitions msg_transitions) noexcept
    {
        return static_cast<uint16_t>(msg_transitions >> shift_part_transitions(msg))
                & 0b1111'1111'1111u;
    }

    constexpr auto regular_front_transitions = MsgTransitions{0
        | bit_transitions(VNC::CbMsgType::FileContentsRequest)
        | bit_transitions(VNC::CbMsgType::FormatDataRequest)
        | bit_transitions(VNC::CbMsgType::FormatList)
        | bit_transitions(VNC::CbMsgType::LockClipdata)
        | bit_transitions(VNC::CbMsgType::UnlockClipdata)
    };

    constexpr auto regular_mod_transitions = regular_front_transitions
        | (bit_transitions(VNC::CbMsgType::ClipCaps))
        | (bit_transitions(VNC::CbMsgType::MonitorReady))
    ;

    constexpr MsgTransitions front_msg_accept_transitions
      = define_transitions(VNC::CbMsgType::MonitorReady, {
            VNC::CbMsgType::ClipCaps,
            VNC::CbMsgType::TempDirectory,
            VNC::CbMsgType::FormatList,
        })
      | define_transitions(VNC::CbMsgType::ClipCaps, {
            VNC::CbMsgType::TempDirectory,
            VNC::CbMsgType::FormatList,
            VNC::CbMsgType::LockClipdata,
            VNC::CbMsgType::UnlockClipdata,
        })
      | define_transitions(VNC::CbMsgType::TempDirectory, {
            VNC::CbMsgType::FormatList,
            VNC::CbMsgType::LockClipdata,
            VNC::CbMsgType::UnlockClipdata,
        })
      | define_transitions(VNC::CbMsgType::FormatList, {
            VNC::CbMsgType::FormatListResponse,
            VNC::CbMsgType::LockClipdata,
            VNC::CbMsgType::UnlockClipdata,
        })
      | define_transitions(VNC::CbMsgType::FormatDataRequest, {
            VNC::CbMsgType::FormatDataResponse,
            VNC::CbMsgType::LockClipdata,
            VNC::CbMsgType::UnlockClipdata,
        })
      | define_transitions(VNC::CbMsgType::FileContentsRequest, {
            VNC::CbMsgType::FileContentsResponse,
            VNC::CbMsgType::LockClipdata,
            VNC::CbMsgType::UnlockClipdata,
        })
      | (regular_front_transitions << shift_part_transitions(VNC::CbMsgType::FormatListResponse))
      | (regular_front_transitions << shift_part_transitions(VNC::CbMsgType::FormatDataResponse))
      | (regular_front_transitions << shift_part_transitions(VNC::CbMsgType::FileContentsResponse))
      ;

    constexpr MsgTransitions mod_msg_accept_transitions
      = define_transitions(VNC::CbMsgType::FormatList, {
            VNC::CbMsgType::FormatListResponse,
            VNC::CbMsgType::LockClipdata,
            VNC::CbMsgType::UnlockClipdata,
            VNC::CbMsgType::ClipCaps,
            VNC::CbMsgType::MonitorReady,
        })
      | define_transitions(VNC::CbMsgType::FormatDataRequest, {
            VNC::CbMsgType::FormatDataResponse,
            VNC::CbMsgType::LockClipdata,
            VNC::CbMsgType::UnlockClipdata,
            VNC::CbMsgType::ClipCaps,
            VNC::CbMsgType::MonitorReady,
        })
      | define_transitions(VNC::CbMsgType::FileContentsRequest, {
            VNC::CbMsgType::FileContentsResponse,
            VNC::CbMsgType::LockClipdata,
            VNC::CbMsgType::UnlockClipdata,
            VNC::CbMsgType::ClipCaps,
            VNC::CbMsgType::MonitorReady,
        })
      | (regular_mod_transitions << shift_part_transitions(VNC::CbMsgType::FormatListResponse))
      | (regular_mod_transitions << shift_part_transitions(VNC::CbMsgType::FormatDataResponse))
      | (regular_mod_transitions << shift_part_transitions(VNC::CbMsgType::FileContentsResponse))
      ;

    auto cb_transitions_to_string(uint16_t msg_transitions) noexcept
    {
        using Flags = unsigned;
        using Impl = StaticStringFromEnumFlags::Impl<Flags>;

        auto flag = [](VNC::CbMsgType msg){
            return Impl::Item{
                static_cast<Flags>(1u << underlying_cast(msg)),
                msg_type_to_name_av(msg),
            };
        };

        static constexpr Impl::Item items[] {
            flag(VNC::CbMsgType::MonitorReady),
            flag(VNC::CbMsgType::FormatList),
            flag(VNC::CbMsgType::FormatListResponse),
            flag(VNC::CbMsgType::FormatDataRequest),
            flag(VNC::CbMsgType::FormatDataResponse),
            flag(VNC::CbMsgType::TempDirectory),
            flag(VNC::CbMsgType::ClipCaps),
            flag(VNC::CbMsgType::FileContentsRequest),
            flag(VNC::CbMsgType::FileContentsResponse),
            flag(VNC::CbMsgType::LockClipdata),
            flag(VNC::CbMsgType::UnlockClipdata),
        };

        constexpr auto capacity = Impl::compute_max_capacity(items);

        auto str = static_string<capacity>{
            delayed_build_t{},
            StaticStringFromEnumFlags::string_builder(Flags{msg_transitions}, items)
        };

        if (str.empty())
        {
            str = "(none)"_sized_av;
        }

        return str;
    }

} // anonymous namespace


bool VNC::CliprdrExpectedClientPDUChecker::is_expected_msg(CbMsgType msg_type) const noexcept
{
    return m_transitions & bit_transitions(msg_type);
}

void VNC::CliprdrExpectedClientPDUChecker::set_next_transition(CbMsgType msg_type) noexcept
{
    m_transitions = get_transitions(msg_type, front_msg_accept_transitions);
}

static_string<189> VNC::CliprdrExpectedClientPDUChecker::transitions_as_string() const noexcept
{
    return cb_transitions_to_string(m_transitions);
}


bool VNC::CliprdrExpectedServerPDUChecker::is_expected_msg(CbMsgType msg_type) const noexcept
{
    return m_transitions & bit_transitions(msg_type);
}

void VNC::CliprdrExpectedServerPDUChecker::set_next_transition(CbMsgType msg_type) noexcept
{
    m_transitions = get_transitions(msg_type, mod_msg_accept_transitions);
}

void VNC::CliprdrExpectedServerPDUChecker::start() noexcept
{
    m_transitions = bit_transitions(CbMsgType::ClipCaps)
                  | bit_transitions(CbMsgType::MonitorReady)
                  ;
}

static_string<189> VNC::CliprdrExpectedServerPDUChecker::transitions_as_string() const noexcept
{
    return cb_transitions_to_string(m_transitions);
}
