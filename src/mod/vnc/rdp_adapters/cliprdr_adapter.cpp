/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <cstdint>

#include "core/error.hpp"
#include "core/channel_list.hpp"
#include "core/RDP/clipboard/format_name.hpp"
#include "mod/vnc/rdp_adapters/cliprdr_adapter.hpp"
#include "utils/log.hpp"
#include "utils/sugar/numerics/safe_conversions.hpp"
#include "utils/sugar/bytes_copy.hpp"
#include "utils/stream.hpp"
#include "utils/mathutils.hpp"
#include "utils/hexdump.hpp"
#include "utils/sugar/static_string_from_enum_flags.hpp"

// TODO for linux_to_windows_newline_convert2 (strchr)
#include "utils/strutils.hpp"

// .h
#include "utils/monotonic_clock.hpp"


#define REDEMPTION_TOO_MANY_DATA_MSG                         \
    "The text was too long to fit in the clipboard buffer. " \
    "The buffer size is limited to 65535 bytes."

// TODO uvnc: specifiy filename enconding

// TODO disable clipboard on read error

// TODO add logs
// TODO add logs on each send_to_front_channel
// TODO add SIEM logs
// TODO dans les logs des fonctions qui decomposent les flags, ajouter la valeur hexa `name(%x)` / `???(%x)`
// TODO filter unexpected msg


// TODO version de ChannelFlags en u8 qui ne contient que First et Last ? FirstLastFlags

// TODO flags_test(v, consteval value) ?

// TODO activer cliplock, mais 1 fichier à la fois ? transfert en faisant autre chose

// TODO transition when response before end of request

// TODO windows_to_linux_newline_convert not used

// TODO replace UTF16toLatin1
static writable_bytes_view UTF16toLatin1(bytes_view source, writable_bytes_view dest) noexcept
{
    return dest.first(UTF16toLatin1(source.data(), source.size(), dest.data(), dest.size()));
}

// TODO replace Latin1toUTF8
static writable_bytes_view Latin1toUTF8(bytes_view source, writable_bytes_view dest) noexcept
{
    return dest.first(Latin1toUTF8(source.data(), source.size(), dest.data(), dest.size()));
}

// TODO move to newline_convert.hpp and rename
static writable_chars_view linux_to_windows_newline_convert2(
    bytes_view source, writable_bytes_view destination,
    char const ** last_source = nullptr)
{
    char const * s = source.as_charp();
    size_t s_length = source.size();

    char * d = destination.as_charp();
    size_t d_length = destination.size();

    while (char const * p = strchr({s, s_length}, '\n'))
    {
        auto l = static_cast<size_t>(p - s);

        if (l + 2 /* CRLF(2) */ > d_length)
        {
            s_length = l;
            break;
        }

        if (l)
        {
            memcpy(d, s, l);
            d        += l;
            d_length -= l;

            s        += l;
            s_length -= l;
        }

        *d++ = '\r';
        *d++ = '\n';
        d_length -= 2; // CRLF(2)

        s++;        // LF(1)
        s_length--; // LF(1)
    }

    if (s_length > d_length)
    {
        s_length = d_length;
    }

    memcpy(d, s, s_length);

    if (last_source)
    {
        *last_source = s + s_length;
    }

    return destination.as_chars().before(d + s_length);
}


namespace
{
    constexpr VNC::CbCapabilityFlags vnc_cb_capability_flags_text
        = VNC::CbCapabilityFlags::UseLongFormatNames
        ;

    constexpr VNC::CbCapabilityFlags vnc_cb_capability_flags_file
        = VNC::CbCapabilityFlags::UseLongFormatNames
        | VNC::CbCapabilityFlags::FileClipNoFilePaths
        // | VNC::CbCapabilityFlags::CanLockClipData
        | VNC::CbCapabilityFlags::StreamFileClipEnabled
        | VNC::CbCapabilityFlags::HugeFileSupportEnabled
        ;

    constexpr auto cliprdr_general_caps_for_non_file
      = make_general_capability_with_header(vnc_cb_capability_flags_text);

    constexpr auto cliprdr_general_caps_for_file
      = make_general_capability_with_header(vnc_cb_capability_flags_file);
}

enum class VNC::CliprdrAdapter::Flags : uint32_t
{
    NoFlags,

    OwnedByClient = 1 << 0,

    // ignore ServerCutText (data too large)
    VncTextSkipData = 1 << 1, // TODO revert -> VncTextAccumulable
    VncTextShouldNotified = 1 << 2,
    VncTextSendable = 1 << 3,

    EnableRdpToVnc_NonFileResponse = 1 << 6,
    EnableRdpToVnc_NonFileRequest = 1 << 7,
    EnableRdpToVnc_FileContents = 1 << 8,
    EnableRdpToVnc_FileRequest = 1 << 9,

    EnableFile
        = EnableRdpToVnc_FileContents
        | EnableRdpToVnc_FileRequest,
    EnableClipboard
        = EnableRdpToVnc_NonFileResponse
        | EnableRdpToVnc_NonFileRequest
        | EnableRdpToVnc_FileContents
        | EnableRdpToVnc_FileRequest
        ,

    BogusCbInfinitLoop_Delayed = 1 << 10,
    BogusCbInfinitLoop_Duplicated = 1 << 11,

    RdpTextSendable = 1 << 13,
    // ignore FormatDataResponse (data too large or interrupted by ServerCutText)
    RdpTextSkipData = 1 << 14, // TODO revert -> RdpTextAccumulable

    // client with CF_TEXT format
    RdpHasCfTextFormat = 1 << 15,
    // client with CF_UNICODETEXT format
    RdpHasCfUnicodeFormat = 1 << 16,
    // client with FileGroupDescriptorW format
    RdpHasFileFormat = 1 << 17,
    // FormatDataRequest is sending
    RdpFileListRequested = 1 << 18,
    // FormatFileList readable (ignore FormatList (in FormatDataResponse) when not present)
    RdpFileListConsume = 1 << 19,

    RdpAnyTextFormats = RdpHasCfTextFormat | RdpHasCfUnicodeFormat,

    RdpFormats = RdpHasCfTextFormat
               | RdpHasCfUnicodeFormat
               | RdpHasFileFormat
               | RdpFileListRequested,

    VncHasFile = 1 << 21,

    RdpTextIsErrorMessage = 1 << 25,

    Ready = 1 << 26,
    CliprdrReady = 1 << 27,
    FileTransferReady = 1 << 28,

    EnableLog = 1u << 30,
    EnableLogDump = 1u << 31,

    // no modified states (initialized in ctor)
    ImmutableOptions = NoFlags
      | BogusCbInfinitLoop_Delayed
      | BogusCbInfinitLoop_Duplicated
      | EnableRdpToVnc_NonFileResponse
      | EnableRdpToVnc_NonFileRequest
      | EnableRdpToVnc_FileContents
      | EnableRdpToVnc_FileRequest
      | FileTransferReady
      | EnableLog
      | EnableLogDump,
};

VNC::CliprdrAdapter::CliprdrAdapter(
    VncBogusClipboardInfiniteLoopStrategy infinite_loop_strategy,
    VncClipboardEncoding server_encoding,
    RdpToVncOptions rdp_to_vnc_options,
    Log log_mode,
    MaxRdpPduLen max_rdp_pdu_len,
    EventContainer& events,
    Callbacks callbacks
) noexcept
    : m_internal_flags(
        Flags::OwnedByClient
        | Flags::VncTextSkipData  // not ready, ignore ServerCutText PDU
        | [&]{
            switch (infinite_loop_strategy)
            {
                case VncBogusClipboardInfiniteLoopStrategy::delayed:
                    return Flags::BogusCbInfinitLoop_Delayed;
                case VncBogusClipboardInfiniteLoopStrategy::duplicated:
                    return Flags::BogusCbInfinitLoop_Duplicated;
                case VncBogusClipboardInfiniteLoopStrategy::continued:
                    break;
            }
            return Flags::NoFlags;
        }()
        | (log_mode != Log::No ? Flags::EnableLog : Flags::NoFlags)
        | (log_mode == Log::Dump ? Flags::EnableLogDump : Flags::NoFlags)
        | (flags_any(rdp_to_vnc_options, RdpToVncOptions::NonFileResponse)
            ? Flags::EnableRdpToVnc_NonFileResponse
            : Flags::NoFlags)
        | (flags_any(rdp_to_vnc_options, RdpToVncOptions::NonFileRequest)
            ? Flags::EnableRdpToVnc_NonFileRequest
            : Flags::NoFlags)
        | (flags_any(rdp_to_vnc_options, RdpToVncOptions::FileResponse)
            ? Flags::EnableRdpToVnc_FileContents
            : Flags::NoFlags)
        | (flags_any(rdp_to_vnc_options, RdpToVncOptions::FileRequest)
            ? Flags::EnableRdpToVnc_FileRequest
            : Flags::NoFlags)
      )
    , m_max_rdp_pdu_len(underlying_cast(max_rdp_pdu_len))
    , m_server_encoding(server_encoding)
    , m_callbacks(callbacks)
    , m_events_guard(events)
{}

void VNC::CliprdrAdapter::init_cliprdr_server()
{
    m_cb_capability_flags = CbCapabilityFlags{};

    m_internal_flags &= Flags::ImmutableOptions;
    m_internal_flags |= Flags::OwnedByClient;
    m_internal_flags |= Flags::VncTextSkipData;

    if (!flags_any(m_internal_flags, Flags::EnableClipboard)) {
        return ;
    }

    LOG_IF(flags_any(m_internal_flags, Flags::EnableLog), LOG_INFO,
        "VNC::CliprdrAdapter::init_server: Sending CLIPRDR_GENERAL_CAPABILITY"
    );

    // CLIPRDR_GENERAL_CAPABILITY
    send_to_front_channel(
        flags_any(m_internal_flags, Flags::EnableFile)
            ? bytes_view{cliprdr_general_caps_for_file}
            : bytes_view{cliprdr_general_caps_for_non_file}
    );

    LOG_IF(flags_any(m_internal_flags, Flags::EnableLog), LOG_INFO,
        "VNC::CliprdrAdapter::init_server: Sending CLIPRDR_MONITOR_READY"
    );

    // CLIPRDR_MONITOR_READY
    send_to_front_channel(monitor_ready);

    m_internal_flags |= Flags::Ready;
}

bool VNC::CliprdrAdapter::enable_file_transfer() noexcept
{
    if (!flags_any(m_internal_flags, Flags::EnableFile))
    {
        return false;
    }

    m_internal_flags |= Flags::FileTransferReady;

    return true;
}

bool VNC::CliprdrAdapter::file_transfer_ready() const noexcept
{
    return flags_any(m_internal_flags, Flags::FileTransferReady)
        && flags_any(m_cb_capability_flags, CbCapabilityFlags::StreamFileClipEnabled);
}

bool VNC::CliprdrAdapter::has_file_capability() const noexcept
{
    return flags_any(m_cb_capability_flags, CbCapabilityFlags::StreamFileClipEnabled);
}

void VNC::CliprdrAdapter::process_vnc_server_cut_text_message(
    bytes_view chunk, uint32_t remaining_len, Rfb::ChunkFlags chunk_flags)
{
    /*
     * Log
     */

    if (flags_any(m_internal_flags, Flags::EnableLogDump | Flags::EnableLog)) [[unlikely]]
    {
        LOG(LOG_INFO,
            "VNC::CliprdrAdapter::process_vnc_server_cut_text_message: remaining_len=%u chunk_data_length=%zu%s internal_state=0x%x",
            remaining_len,
            chunk.size(),
            // TODO use channel_flags_first_last_to_string like as process_rdp_client_message
            flags_test(chunk_flags, Rfb::ChunkFlags::First | Rfb::ChunkFlags::Last) ? " FIRST|LAST"
            : flags_any(chunk_flags, Rfb::ChunkFlags::First) ? " FIRST"
            : flags_any(chunk_flags, Rfb::ChunkFlags::Last) ? " LAST"
            : "",
            m_internal_flags
        );

        if (flags_any(m_internal_flags, Flags::EnableLogDump)) [[unlikely]]
        {
            hexdump_c(chunk);
            LOG(LOG_INFO, "VNC::CliprdrAdapter::process_vnc_server_cut_text_message: dumped ^^^^");
        }
    }

    /*
     * Parse message
     */

    if (!flags_test(m_internal_flags, Flags::EnableRdpToVnc_NonFileRequest | Flags::CliprdrReady)) {
        LOG_IF(
            flags_any(m_internal_flags, Flags::EnableRdpToVnc_NonFileRequest),
            LOG_INFO,
            "VNC::CliprdrAdapter::process_vnc_server_cut_text_message: Clipboard Channel Redirection unavailable"
        );
        return ;
    }

    // Can stop RDP to VNC clipboard infinite loop.
    m_request_data_timer.garbage();

    if (flags_any(chunk_flags, Rfb::ChunkFlags::First))
    {
        m_internal_flags &= ~Flags::OwnedByClient; // TODO is not locked

        // TODO as CbMsgType::FormatDataResponse

        m_cb_data_len = 0;
        m_internal_flags &= ~Flags::RdpTextIsErrorMessage;
        m_internal_flags &= ~Flags::RdpTextSendable;
        m_internal_flags &= ~Flags::VncTextSkipData;
        m_internal_flags |= Flags::VncTextSendable;
        m_internal_flags |= Flags::RdpTextSkipData; // disable RDP data

        /*
         * Check too many data
         */
        if (remaining_len + chunk.size() > MAX_CLIPBOARD_DATA_SIZE)
        {
            m_internal_flags |= Flags::VncTextSkipData;
            m_internal_flags |= Flags::RdpTextIsErrorMessage;
        }
    }

    if (!flags_any(m_internal_flags, Flags::VncTextSkipData))
    {
        push_in_cb_data(chunk);
    }

    if (flags_any(chunk_flags, Rfb::ChunkFlags::Last)
     && flags_any(m_internal_flags, Flags::VncTextSendable))
    {
        LOG_IF(flags_any(m_internal_flags, Flags::EnableLog), LOG_INFO,
            "VNC::CliprdrAdapter::process_vnc_server_cut_text_message: Sending FORMAT_LIST"
        );

        send_to_front_channel(
            flags_any(m_cb_capability_flags, CbCapabilityFlags::UseLongFormatNames)
                ? format_list_unicode_in_long_format_with_header
                : format_list_unicode_in_short_format_with_header
        );
    }
}

struct VNC::CliprdrAdapter::P
{
    REDEMPTION_NOINLINE
    static void log_process_rdp_client_message(
        CliprdrAdapter const & self,
        bytes_view chunk,
        uint32_t total_len,
        ChannelFlags channel_flags)
    {
        auto cliprdr_reader = self.m_cliprdr_reader;
        auto cb_chunk = cliprdr_reader.read(chunk, total_len, channel_flags);

        auto internal_flags_s = StaticStringFromEnumFlags::make<
            "Ready"_name_of(Flags::Ready),
            "CliprdrReady"_name_of(Flags::CliprdrReady),
            "FileTransferReady"_name_of(Flags::FileTransferReady),
            "EnableNonFileResponse"_name_of(Flags::EnableRdpToVnc_NonFileResponse),
            "EnableNonFileRequest"_name_of(Flags::EnableRdpToVnc_NonFileRequest),
            "EnableFileContents"_name_of(Flags::EnableRdpToVnc_FileContents),
            "EnableFileRequest"_name_of(Flags::EnableRdpToVnc_FileRequest),
            "OwnedByClient"_name_of(Flags::OwnedByClient),
            "VncHasFile"_name_of(Flags::VncHasFile),
            "RdpHasCfUnicodeFormat"_name_of(Flags::RdpHasCfUnicodeFormat),
            "RdpHasFileFormat"_name_of(Flags::RdpHasFileFormat),
            "RdpFileListRequested"_name_of(Flags::RdpFileListRequested)
        >(self.m_internal_flags);

        LOG(LOG_INFO,
            "VNC::CliprdrAdapter::process_rdp_client_message: ec=%d type=<%s>(0x%08X) flags=<%s>(0x%08X) msg_flags=<%s>(0x%08X) data_len=%zu total_len=%u remaining=%u internal_flags=<%s>(0x%08X)",
            cb_chunk.ec,
            msg_type_to_name(self.m_cliprdr_reader.last_msg_type()),
            self.m_cliprdr_reader.last_msg_type(),
            channel_flags_first_last_to_string(channel_flags),
            channel_flags,
            msg_flags_to_string(self.m_cliprdr_reader.last_msg_flags()),
            self.m_cliprdr_reader.last_msg_flags(),
            cb_chunk.partial_data().size(),
            total_len,
            cliprdr_reader.remaining_data_len(),
            internal_flags_s,
            self.m_internal_flags
        );

        if (flags_any(self.m_internal_flags, Flags::EnableLogDump))
        {
            hexdump_c(chunk);
            LOG(LOG_INFO, "VNC::CliprdrAdapter::process_rdp_client_message: dumped ^^^^");
        }
    }
};

void VNC::CliprdrAdapter::process_rdp_client_message(
    const bytes_view chunk,
    const uint32_t total_len,
    const ChannelFlags channel_flags)
{
    /*
     * Log
     */

    if (flags_any(m_internal_flags, Flags::EnableLogDump | Flags::EnableLog)) [[unlikely]]
    {
        P::log_process_rdp_client_message(*this, chunk, total_len, channel_flags);
    }

    /*
     * Check state
     */

    if (!flags_any(m_internal_flags, Flags::Ready)) {
        LOG_IF(flags_any(m_internal_flags, Flags::EnableLog), LOG_INFO,
            "VNC::CliprdrAdapter::process_rdp_client_message: no ready, ignored"
        );
        return ;
    }

    /*
     * Check channel flags integrity
     */

    if (auto status = m_channel_flags_checker.next(channel_flags)
      ; is_err(status)) [[unlikely]]
    {
        using St = ChannelFlagsChecker::Status;
        auto to_s = [](St status){
            switch (status)
            {
                case St::Ok: break;
                case St::MissingFirst: return "missing FIRST flag";
                case St::MissingLast: return "missing LAST flag";
                case St::UnspecifiedFlags: return "Unflaged";
            }
            return "";
        };
        LOG(LOG_WARNING,
            "VNC::CliprdrAdapter::process_rdp_client_message: channel_flags: %s", to_s(status)
        );

        // TODO sub error
        throw Error(ERR_VNC);
    }

    /*
     * Parse header
     */

    auto cb_chunk = m_cliprdr_reader.read(chunk, total_len, channel_flags);

    if (!cb_chunk) [[unlikely]]
    {
        switch (cb_chunk.ec)
        {
            case CliprdrReader::Result::ErrorCode::DataTruncated:
                LOG(LOG_ERR, "VNC::CliprdrAdapter::process_rdp_client_message: data is truncated: remaining=%u", m_cliprdr_reader.remaining_data_len());
                break;

            case CliprdrReader::Result::ErrorCode::InsufficientData:
                LOG(LOG_ERR, "VNC::CliprdrAdapter::process_rdp_client_message: insufficient data: CliprdrHeader: expected=%u remains=%zu",
                    CliprdrHeader::pdu_len(), chunk.size());
                break;

            case CliprdrReader::Result::ErrorCode::TotalLenTooShort:
                LOG(LOG_ERR, "VNC::CliprdrAdapter::process_rdp_client_message: total_len too short: CliprdrHeader: data_len=%u total_len=%u - %u",
                    m_cliprdr_reader.remaining_data_len(), total_len, CliprdrHeader::pdu_len());
                break;

            case CliprdrReader::Result::ErrorCode::Ok:
                assert(false);
        }

        // TODO sub error
        throw Error(ERR_VNC);
    }

    /*
     * Prepare parsing message
     */

    InStream in_stream { cb_chunk.partial_data() };

    switch (m_cliprdr_reader.last_msg_type())
    {
        case CbMsgType::FormatList: {
            m_internal_flags |= Flags::CliprdrReady;
            m_internal_flags &= ~Flags::VncHasFile;
            m_request_data_timer.garbage();

            /*
             * Extract some formats
             */

            // multi chunk is not supported
            if (flags_any(channel_flags, ChannelFlags::First))
            {
                // reset format
                m_internal_flags &= ~Flags::RdpFormats;

                if (flags_any(m_internal_flags, Flags::EnableRdpToVnc_NonFileResponse
                                              | Flags::EnableRdpToVnc_FileContents))
                {
                    format_list_extract(
                        in_stream,
                        m_cb_capability_flags,
                        m_cliprdr_reader.last_msg_flags(),
                        [&](CbFormatID format_id, GenericName name)
                        {
                            if (format_id == CbFormatID::UnicodeText)
                            {
                                if (flags_any(m_internal_flags, Flags::EnableRdpToVnc_NonFileResponse))
                                {
                                    m_internal_flags |= Flags::RdpHasCfUnicodeFormat;
                                }
                            }
                            else if (format_id == CbFormatID::Text)
                            {
                                if (flags_any(m_internal_flags, Flags::EnableRdpToVnc_NonFileResponse))
                                {
                                    m_internal_flags |= Flags::RdpHasCfTextFormat;
                                }
                            }
                            else if (flags_test(m_internal_flags, Flags::FileTransferReady
                                                                | Flags::EnableRdpToVnc_FileContents)
                                && name == predefined_names::file_group_descriptor_w)
                            {
                                m_internal_flags |= Flags::RdpHasFileFormat;
                                m_client_file_format_id = format_id;
                            }

                            if (flags_any(m_internal_flags, Flags::EnableLog)) [[unlikely]]
                            {
                                log_format_name(
                                    "VNC::CliprdrAdapter::process_rdp_client_message: ",
                                    format_id, name
                                );
                            }
                        }
                    );
                }
            }

            // no Last, wait next chunk
            if (!flags_any(channel_flags, ChannelFlags::Last))
            {
                return ;
            }

            /*
             * Send FormatListResponse
             */

            LOG_IF(flags_any(m_internal_flags, Flags::EnableLog), LOG_INFO,
                "VNC::CliprdrAdapter::process_rdp_client_message: Sending FORMAT_LIST_RESPONSE ok"
            );

            send_to_front_channel(format_list_response_ok_with_header);

            /*
             * Request Data
             */

            // TODO disable timer before return ?

            // no supported format
            if (!flags_any(m_internal_flags, Flags::RdpFormats))
            {
                // TODO
                return;
            }

            /* rdp message :
             * --------------------------------------------------------------------------------
             *                rdesktop                |            freerdp / mstsc
             * --------------------------------------------------------------------------------
             *                                    host copy
             * --------------------------------------------------------------------------------
             *  front -> mod : (2) FormatList         | front -> mod : (2) FormatList
             *  front <- mod : (3) FormatListResponse | front <- mod : (3) FormatListResponse
             * --------------------------------------------------------------------------------
             *                          send data to vnc (request data)
             * --------------------------------------------------------------------------------
             *  front <- mod : (4) FormatDataResquest | front <- mod : (4) FormatDataResquest
             *                                        | front -> mod : (2) FormatList (freerdp)
             *                                        | \_ignored by ExpectedClientPDUChecker_/
             *  front -> mod : (5) FormatDataResponse | front -> mod : (5) FormatDataResponse
             * --------------------------------------------------------------------------------
             *                          rdesktop resend format list...
             * --------------------------------------------------------------------------------
             *  front -> mod : (2) FormatList         |
             *  front <- mod : (3) FormatListResponse |
             * --------------------------------------------------------------------------------
             *                           request data indefinitely...
             *                               slowdonw with timer
             */

            // TODO ^^^ except with VNC Extended Clipboard Pseudo-Encoding

            // TODO ctor param
            using namespace std::chrono_literals;
            constexpr MonotonicTimePoint::duration MINIMUM_TIMEVAL(250ms);

            bool already_owned_by_client = flags_any(m_internal_flags, Flags::OwnedByClient);
            m_internal_flags |= Flags::OwnedByClient;

            if (flags_any(m_internal_flags, Flags::RdpHasFileFormat))
            {
                // nothing
            }
            else if (!flags_any(m_internal_flags, Flags::RdpAnyTextFormats))
            {
                // nothing
            }
            else if (
                const auto timeval_diff = m_events_guard.get_monotonic_time()
                                        - m_timestamp_of_last_format_data_response
              ; timeval_diff >= MINIMUM_TIMEVAL
             || !already_owned_by_client)
            {
                request_text_data("");
            }
            else if (flags_any(m_internal_flags, Flags::BogusCbInfinitLoop_Delayed))
            {
                LOG_IF(bool(m_internal_flags & Flags::EnableLog), LOG_INFO,
                    "VNC::CliprdrAdapter::process_rdp_client_message: "
                    "msgType=CB_FORMAT_DATA_REQUEST(4) (delayed)");
                // arms timeout
                m_request_data_timer.set_timeout_or_create_event(
                    MINIMUM_TIMEVAL - timeval_diff,
                    m_events_guard,
                    "VNC Clipboard Timeout Event",
                    [this](Event& e)
                    {
                        e.garbage = true;
                        request_text_data(" (triggered)");
                    }
                );
            }
            else if (!flags_any(m_internal_flags, Flags::BogusCbInfinitLoop_Duplicated)
                    // potentially mstsc
                  && flags_test(m_cb_capability_flags,
                        CbCapabilityFlags::UseLongFormatNames
                      | CbCapabilityFlags::FileClipNoFilePaths
                      | CbCapabilityFlags::FileClipNoFilePaths
                      | CbCapabilityFlags::CanLockClipData))
            {
                LOG_IF(bool(m_internal_flags & Flags::EnableLog), LOG_INFO,
                    "VNC::CliprdrAdapter::process_rdp_client_message: "
                    "duplicated clipboard update event "
                    "from Windows client is ignored");
            }
            // TODO remove ? change behavior of BogusCbInfinitLoop_Duplicated ?
            else
            {
                LOG_IF(bool(m_internal_flags & Flags::EnableLog), LOG_INFO,
                    "VNC::CliprdrAdapter::process_rdp_client_message: "
                    "msgType=CB_FORMAT_LIST(2) (preventive)");

                send_to_front_channel(
                    flags_any(m_internal_flags, Flags::RdpHasCfUnicodeFormat)
                        ? flags_any(m_cb_capability_flags, CbCapabilityFlags::UseLongFormatNames)
                            ? format_list_unicode_in_long_format_with_header
                            : format_list_unicode_in_short_format_with_header
                        : flags_any(m_cb_capability_flags, CbCapabilityFlags::UseLongFormatNames)
                            ? format_list_text_in_long_format_with_header
                            : format_list_text_in_short_format_with_header
                );
            }

            break;
        }

        case CbMsgType::FormatListResponse: {
            // TODO
            break;
        }

        case CbMsgType::FormatDataRequest: {
            /*
             * Check complete packet (multi packet is not supported)
             */

            if (!flags_test(channel_flags, ChannelFlags::First | ChannelFlags::Last))
            {
                send_to_front_channel(format_data_response_fail_with_header);
                return ;
            }

            // TODO set OwnedByClient flag

            if (!flags_test(m_internal_flags, FlagsContraints{
                    .match = Flags::CliprdrReady,
                    .reject = Flags::OwnedByClient,
                }))
            {
                LOG_IF(flags_any(m_internal_flags, Flags::EnableLog), LOG_INFO,
                    "VNC::CliprdrAdapter::process_rdp_client_message: "
                    "Sending FORMAT_DATA_RESPONSE fail (%s)",
                    flags_any(m_internal_flags, Flags::CliprdrReady)
                    ? "owned by client"
                    : "no ready"
                );

                send_to_front_channel(format_data_response_fail_with_header);
                // TODO
                return ;
            }

            FormatDataRequest data_req;
            if (!data_req.read(in_stream)) [[unlikely]]
            {
                // TODO truncated
                return;
            }

            data_req.log_if(
                flags_any(m_internal_flags, Flags::EnableLog),
                "VNC::CliprdrAdapter::process_rdp_client_message: "
            );

            // This is a fake treatment that pretends to send the Request
            // to VNC server. Instead, the RDP PDU is handled localy and
            // the clipboard PDU, if any, is likewise built localy and
            // sent back to front.

            // prefer CP1252, because latin1 is compatible (range 0x80-0x9F not specified)
            // and some Windows servers use windows code page.
            constexpr auto latin1_to_utf16 = cp1252_to_utf16le_lf_to_crlf;
            constexpr auto utf8_to_utf16 = utf8_to_utf16le_lf_to_crlf;
            constexpr auto max_output_buffer_multiplicator = mmax(
                latin1_to_utf16.max_output_buffer_multiplicator,
                utf8_to_utf16.max_output_buffer_multiplicator
            );
            constexpr auto max_encoded_buf_len = MAX_CLIPBOARD_DATA_SIZE
                                               * max_output_buffer_multiplicator;

            bounded_bytes_view clip_data{m_cb_data, m_cb_data_len};

            // size = max(utf8 -> utf16, text -> text_with_crlf)
            StaticOutStream<CliprdrHeader::pdu_len() + max_encoded_buf_len + 2> out_stream;

            auto dest_buffer = make_writable_bounded_array_view(out_stream.internal_array())
                .drop_front<CliprdrHeader::pdu_len()>();
            uint32_t data_len;

            if (data_req.requestedFormatId == CbFormatID::UnicodeText)
            {
                if (flags_any(m_internal_flags, Flags::RdpTextIsErrorMessage))
                {
                    data_len = checked_int {
                        bytes_copy(dest_buffer, REDEMPTION_TOO_MANY_DATA_MSG "\0"_utf16_le)
                    };
                }
                else
                {
                    auto unicode_buffer = dest_buffer.drop_back<2>(); // null character
                    writable_bytes_view rdp_clipdata;

                    switch (m_server_encoding)
                    {
                        case VncClipboardEncoding::utf8:
                            rdp_clipdata = utf8_to_utf16(clip_data, unicode_buffer);
                            break;

                        case VncClipboardEncoding::latin1:
                            rdp_clipdata = latin1_to_utf16(clip_data, unicode_buffer);
                            break;
                    }

                    rdp_clipdata.end()[0] = '\0';
                    rdp_clipdata.end()[1] = '\0';

                    data_len = checked_int{
                        rdp_clipdata.size() + 2 // null character
                    };
                }
            }
            else if (data_req.requestedFormatId == CbFormatID::Text)
            {
                if (flags_any(m_internal_flags, Flags::RdpTextIsErrorMessage))
                {
                    data_len = checked_int {
                        bytes_copy(dest_buffer, REDEMPTION_TOO_MANY_DATA_MSG "\0"_av)
                    };
                }
                else
                {
                    auto rdp_clipdata = linux_to_windows_newline_convert2(
                        clip_data.as_chars(),
                        dest_buffer.drop_back(1) // null character
                    );
                    *rdp_clipdata.end() = '\0';

                    data_len = checked_int{
                        rdp_clipdata.size() + 1 // null character
                    };
                }
            }
            else if (data_req.requestedFormatId == custom_file_group_descriptor_w_id
                  && flags_test(m_internal_flags, Flags::EnableRdpToVnc_FileRequest
                                                | Flags::VncHasFile))
            {
                m_callbacks.receive_file_data_request(m_callbacks.ctx);
                return ;
            }
            else {
                LOG( LOG_WARNING
                   , "VNC::CliprdrAdapter::process_rdp_client_message: resquested clipboard format Id 0x%02x is not supported by VNC PROXY"
                   , data_req.requestedFormatId);
                send_to_front_channel(format_data_response_fail_with_header);
                return ;
            }

            CliprdrHeader::make(
                CbMsgType::FormatDataResponse,
                CbMsgFlags::ResponseOk,
                data_len
            ).write_unchecked(out_stream);
            out_stream.out_skip_bytes(data_len);

            send_to_front_channel(out_stream.get_produced_bytes());

            LOG_IF(flags_any(m_internal_flags, Flags::EnableLog), LOG_INFO,
                "VNC::CliprdrAdapter::process_rdp_client_message: "
                "Sending FORMAT_DATA_RESPONSE (%s) done",
                data_req.requestedFormatId == CbFormatID::Text
                    ? "CF_UNICODETEXT"
                    : "CF_TEXT"
            );

            break;
        }

        case CbMsgType::FormatDataResponse: {
            if (flags_any(m_cliprdr_reader.last_msg_flags(), CbMsgFlags::ResponseOk)
             && flags_any(m_internal_flags, Flags::RdpFormats)
             && flags_any(m_internal_flags, Flags::OwnedByClient))
            {
                uint32_t cItems = 0;

                if (flags_any(channel_flags, ChannelFlags::First))
                {
                    // TODO same as process_vnc_server_cut_text_message

                    m_cb_data_len = 0;
                    m_internal_flags &= ~Flags::RdpTextIsErrorMessage;
                    m_internal_flags &= ~Flags::VncTextSendable;
                    m_internal_flags &= ~Flags::RdpTextSkipData;
                    m_internal_flags &= ~Flags::RdpFileListConsume;
                    m_internal_flags |= Flags::RdpTextSendable;
                    m_internal_flags |= Flags::VncTextSkipData; // disable VNC data

                    // TODO disable text (^) with RdpHasFileFormat ? And when ServerCut ?

                    if (flags_any(m_internal_flags, Flags::RdpHasFileFormat))
                    {
                        m_internal_flags &= ~Flags::RdpTextSendable;
                        m_internal_flags |= Flags::RdpTextSkipData;
                        // TODO read FileListWithoutArray / FileDescriptor and check
                        // TODO FileDescriptorFlags::FileSize -> send FileContentsRequest
                        // TODO with appropriate CbFileContentsType

                        FileListWithoutArray file_list;
                        if (file_list.read(in_stream))
                        {
                            cItems = file_list.cItems;
                            m_internal_flags |= Flags::RdpFileListConsume;
                            file_list.log_if(
                                flags_any(m_internal_flags, Flags::EnableLog),
                                "VNC::CliprdrAdapter::process_rdp_client_message: "
                            );
                            // Note: file_list.cItems is ignored,
                            // file_list.fileDescriptorArray is
                            // consumed based on remaining length.
                            // TODO adapte remaining_len value
                        }
                        // disable receive process on failure
                        else
                        {
                            m_internal_flags &= ~Flags::RdpFormats;
                            LOG(LOG_WARNING, "VNC::CliprdrAdapter::process_rdp_client_message: CLIPRDR_FILELIST too short, PDU is ignored");
                        }
                    }
                    // Check too many data for clipboard text
                    else if (
                        auto total_len = m_cliprdr_reader.remaining_data_len()
                                       + cb_chunk.partial_data().size()
                      ; total_len > MAX_CLIPBOARD_DATA_SIZE)
                    {
                        m_internal_flags |= Flags::RdpTextSkipData;
                        m_internal_flags |= Flags::RdpTextIsErrorMessage;
                    }
                }

                if (!flags_any(m_internal_flags, Flags::RdpTextSkipData))
                {
                    push_in_cb_data(cb_chunk.partial_data());
                }

                // send clipboard text
                if (flags_any(channel_flags, ChannelFlags::Last)
                 && flags_any(m_internal_flags, Flags::RdpTextSendable))
                {
                    send_client_cut_text_to_mod();
                }
                // read FileList
                else if (flags_any(m_internal_flags, Flags::RdpFileListConsume))
                {
                    m_cb_data_len = m_callbacks.receive_partial_file_list(
                        m_callbacks.ctx,
                        in_stream.remaining_bytes(),
                        make_writable_sized_array_view(m_cb_data).first<FileDescriptor::pdu_len()>(),
                        m_cb_data_len,
                        channel_flags,
                        cItems
                    );
                }

                if (flags_any(channel_flags, ChannelFlags::Last))
                {
                    m_timestamp_of_last_format_data_response = m_events_guard.get_monotonic_time();
                    m_internal_flags &= ~Flags::RdpFileListConsume;
                }
            }

            break;
        }

        case CbMsgType::ClipCaps: {
            /*
             * Check complete packet (multi packet is not supported)
             */

            if (!flags_test(channel_flags, ChannelFlags::First | ChannelFlags::Last))
            {
                // TODO
            }

            /*
             * Read capabilities
             */

            auto result = GeneralFlagsCapability::parse(cb_chunk.partial_data());

            if (result.ok)
            {
                auto general_flags = CbCapabilityFlags{result.general_flags_or_expected_len};
                m_cb_capability_flags = general_flags;
                m_cb_capability_flags &= flags_any(m_internal_flags, Flags::EnableFile)
                    ? vnc_cb_capability_flags_file
                    : vnc_cb_capability_flags_text;

                LOG_IF(flags_any(m_internal_flags, Flags::EnableLog), LOG_INFO,
                    "VNC::CliprdrAdapter::process_rdp_client_message: "
                    "CLIPRDR_GENERAL_CAPABILITY: <%s>(0x%X)",
                    capability_flags_to_string(general_flags), general_flags
                );

                m_callbacks.receive_capability_flags(m_callbacks.ctx);
            }
            else
            {
                // TODO
            }

            break;
        }

        // server to client only
        case CbMsgType::MonitorReady:
            break;

        // ignored
        case CbMsgType::TempDirectory:
            break;

        case CbMsgType::FileContentsRequest: {
            FileContentsRequest contents_req;
            if (!contents_req.read(in_stream))
            {
                // TODO error
                LOG(LOG_ERR, "VNC::CliprdrAdapter::process_rdp_client_message: invalid CB_FILECONTENTS_REQUEST");
            }

            contents_req.log_if(
                flags_any(m_internal_flags, Flags::EnableLog),
                "VNC::CliprdrAdapter::process_rdp_client_message: "
            );

            char const * msg_error_ctx = "";

            if (contents_req.dwFlags != VNC::CbFileContentsType::Size
             && contents_req.dwFlags != VNC::CbFileContentsType::Range)
            {
                msg_error_ctx = "invalid dwFlags";
            }
            else if (flags_test(m_internal_flags, FlagsContraints{
                    .match = Flags::CliprdrReady
                           | Flags::FileTransferReady
                           | Flags::EnableRdpToVnc_FileRequest
                           | Flags::VncHasFile,
                    .reject = Flags::OwnedByClient,
                }))
            {
                m_callbacks.receive_file_contents_request(m_callbacks.ctx, contents_req);
                return ;
            }
            else
            {
                msg_error_ctx = flags_any(m_internal_flags, Flags::OwnedByClient)
                    ? "owned by client"
                    : "no ready";
            }

            LOG_IF(flags_any(m_internal_flags, Flags::EnableLog), LOG_WARNING,
                "VNC::CliprdrAdapter::process_rdp_client_message: "
                "Sending CLIPRDR_FILECONTENTS_RESPONSE fail (%s)",
                msg_error_ctx
            );
            contents_req.log("VNC::CliprdrAdapter::process_rdp_client_message: ", LOG_WARNING);

            send_to_front_channel(make_cb_packet_with_header(
                CbMsgFlags::ResponseFail,
                FileContentsResponseWithoutData {
                    contents_req.streamId
                }
            ));
            break;
        }

        case CbMsgType::FileContentsResponse: {
            if (flags_any(m_internal_flags, Flags::EnableRdpToVnc_FileContents))
            {
                m_callbacks.receive_file_contents_response(
                    m_callbacks.ctx,
                    cb_chunk.partial_data(),
                    m_cliprdr_reader.remaining_data_len(),
                    flags_any(m_cliprdr_reader.last_msg_flags(), CbMsgFlags::ResponseOk),
                    channel_flags
                );
            }
            break;
        }

        // unsupported
        case CbMsgType::LockClipdata:
        case CbMsgType::UnlockClipdata:
            break;
    }

    // TODO invalid type
}

void VNC::CliprdrAdapter::send_to_front_channel(bytes_view chunk)
{
    m_callbacks.send_to_front_channel(
        m_callbacks.ctx,
        chunk,
        chunk.size(),
        ChannelFlags::First | ChannelFlags::Last | ChannelFlags::ShowProtocol
    );
}

bool VNC::CliprdrAdapter::request_file_list()
{
    if (flags_test(m_internal_flags, FlagsContraints{
            .match = Flags::RdpHasFileFormat,
            .reject = Flags::RdpFileListRequested,
        }))
    {
        LOG_IF(bool(m_internal_flags & Flags::EnableLog), LOG_INFO,
            "VNC::CliprdrAdapter::request_file_list: Sending CB_FORMAT_DATA_REQUEST(4)");

        m_internal_flags |= Flags::RdpFileListRequested;

        send_to_front_channel(
            make_cb_packet_with_header(FormatDataRequest{
                .requestedFormatId = m_client_file_format_id
            })
        );

        return true;
    }

    return false;
}

bool VNC::CliprdrAdapter::send_format_list_with_files()
{
    if (flags_any(m_internal_flags, Flags::EnableRdpToVnc_FileRequest)
     && flags_any(m_cb_capability_flags, CbCapabilityFlags::UseLongFormatNames))
    {
        LOG_IF(bool(m_internal_flags & Flags::EnableLog), LOG_INFO,
            "VNC::CliprdrAdapter::send_file_list()");

        m_internal_flags &= ~Flags::OwnedByClient;
        m_internal_flags &= ~Flags::RdpHasFileFormat;
        m_internal_flags &= ~Flags::RdpFileListRequested;
        m_internal_flags |= Flags::VncHasFile;

        send_to_front_channel(
            format_list_custom_file_group_descriptor_w_in_long_format_with_header
        );

        return true;
    }

    return false;
}

bool VNC::CliprdrAdapter::is_requested_file_list() const noexcept
{
    return flags_any(m_internal_flags, Flags::RdpFileListRequested);
}

bool VNC::CliprdrAdapter::has_file_group_descriptor_format() const noexcept
{
    return flags_any(m_internal_flags, Flags::RdpHasFileFormat);
}

void VNC::CliprdrAdapter::request_text_data(char const * extra_msg)
{
    m_request_data_timer.garbage();

    LOG_IF(bool(m_internal_flags & Flags::EnableLog), LOG_INFO,
        "VNC::CliprdrAdapter::process_rdp_client_message: Sending CB_FORMAT_DATA_REQUEST(4)%s",
        extra_msg);

    send_to_front_channel(
        flags_any(m_internal_flags, Flags::RdpHasCfUnicodeFormat)
            ? format_data_request_unicode_with_header
            : format_data_request_text_with_header
    );
}

void VNC::CliprdrAdapter::send_client_cut_text_to_mod()
{
    // TODO if is_up_and_running

    m_internal_flags |= Flags::OwnedByClient;

    // TODO use ServerCutText
    constexpr uint16_t server_cut_text_header_len = 8;

    // size = max(utf16 -> utf8, utf16 -> latin1)
    StaticOutStream<
        MAX_CLIPBOARD_DATA_SIZE * 2
        + server_cut_text_header_len
    > stream;
    auto output_buffer = stream.get_tail().from_offset(server_cut_text_header_len);

    uint32_t data_len;

    if (flags_any(m_internal_flags, Flags::RdpTextIsErrorMessage))
    {
        data_len = checked_int {
            bytes_copy(output_buffer, REDEMPTION_TOO_MANY_DATA_MSG ""_av)
        };
    }
    else
    {
        bytes_view clip_data{m_cb_data, m_cb_data_len};

        // remove null character
        if (flags_any(m_internal_flags, Flags::RdpHasCfUnicodeFormat))
        {
            clip_data = clip_data.first(clip_data.size() > 2 ? clip_data.size() - 2 : 0);
        }
        else // if (m_internal_flags & Flags::RdpHasCfTextFormat)
        {
            clip_data = clip_data.first(clip_data.size() > 1 ? clip_data.size() - 1 : 0);
        }

        // TODO CRLF -> LF ?

        switch (m_server_encoding)
        {
            case VncClipboardEncoding::utf8: {
                LOG_IF(flags_any(m_internal_flags, Flags::EnableLog), LOG_INFO,
                    "VNC::CliprdrAdapter::send_client_cut_text_to_mod: CF_UNICODETEXT -> UTF-8");

                data_len = checked_int{
                    flags_any(m_internal_flags, Flags::RdpHasCfUnicodeFormat)
                        ? UTF16toUTF8_buf(clip_data, output_buffer).size()
                        : Latin1toUTF8(clip_data, output_buffer).size()
                };
                break;
            }

            case VncClipboardEncoding::latin1: {
                LOG_IF(flags_any(m_internal_flags, Flags::EnableLog), LOG_INFO,
                    "VNC::CliprdrAdapter::send_client_cut_text_to_mod: CF_UNICODETEXT -> Latin-1");

                if (flags_any(m_internal_flags, Flags::RdpHasCfUnicodeFormat))
                {
                    data_len = checked_int { UTF16toLatin1(clip_data, output_buffer).size() };
                }
                else
                {
                    memcpy(output_buffer.data(), clip_data.data(), clip_data.size());
                    data_len = checked_int { clip_data.size() };
                }
                break;
            }
        }
    }

    // TODO use ClientCutText
    stream.out_uint8(6);              // message-type : ClientCutText
    stream.out_clear_bytes(3);        // padding
    stream.out_uint32_be(data_len);   // length
    stream.out_skip_bytes(data_len);  // text

    m_callbacks.send_to_mod_channel(
        m_callbacks.ctx, stream.get_produced_bytes()
    );
}

void VNC::CliprdrAdapter::push_in_cb_data(bytes_view data)
{
    auto remaining_len = mmin(MAX_CLIPBOARD_DATA_SIZE - m_cb_data_len, data.size());
    memcpy(m_cb_data + m_cb_data_len, data.data(), remaining_len);
    m_cb_data_len += remaining_len;
}

#undef REDEMPTION_TOO_MANY_DATA_MSG
