/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <cstdint>

#include "core/events.hpp"
#include "utils/sugar/bytes_view.hpp"
#include "utils/sugar/flags.hpp"
#include "utils/sugar/not_null_ptr.hpp"
#include "mod/vnc/encoders/chunk_flags.hpp"
#include "mod/vnc/rdp_adapters/rdpeclip.hpp"
#include "configs/autogen/enums.hpp"

class Buf64k;

namespace meta
{
    template<class T>
    concept is_empty = std::is_empty_v<T>;
}

namespace VNC
{

struct CliprdrAdapter
{
    enum class Log : uint8_t
    {
        No,
        Yes,
        Dump,
    };

    enum class RdpToVncOptions : uint8_t
    {
        None,
        NonFileResponse = 1 << 0,
        NonFileRequest  = 1 << 1,
        FileResponse    = 1 << 2,
        FileRequest     = 1 << 3,
    };
    REDEMPTION_DECLARE_ENUM_FLAGS_IN_CLASS(RdpToVncOptions)

    enum class MaxRdpPduLen : uint16_t;

    struct Callbacks
    {
        using FileDescriptorBufferView = writable_sized_bytes_view<FileDescriptor::pdu_len()>;

        using ToFrontFn = void(
            void * ctx,
            bytes_view data,
            size_t total_len,
            ChannelFlags channel_flags
        );

        using ToModFn = void(void * ctx, bytes_view data);

        // total_item is 0 when channel_flags does not contains First.
        // return new buffer offset
        using ReceivePartialFileList = uint16_t(
            void * ctx,
            bytes_view data,
            FileDescriptorBufferView buffer,
            uint16_t buffer_offset,
            ChannelFlags channel_flags,
            uint32_t total_item
        );

        // total_item is 0 when channel_flags does not contains First.
        using ReceiveFileContentsResponse = void(
            void * ctx,
            bytes_view data,
            uint32_t remaining_len,
            bool is_ok,
            ChannelFlags channel_flags
        );

        using ReceiveFileDataRequest = void(void * ctx);
        using ReceiveFileContentsRequest = void(
            void * ctx,
            FileContentsRequest const & req
        );

        using ReceiveCapabilityFlags = void(void * ctx);

        void * ctx;
        not_null_ptr<ToFrontFn> send_to_front_channel;
        not_null_ptr<ToModFn> send_to_mod_channel;
        not_null_ptr<ReceivePartialFileList> receive_partial_file_list;
        not_null_ptr<ReceiveFileContentsResponse> receive_file_contents_response;
        not_null_ptr<ReceiveFileDataRequest> receive_file_data_request;
        not_null_ptr<ReceiveFileContentsRequest> receive_file_contents_request;
        not_null_ptr<ReceiveCapabilityFlags> receive_capability_flags;

        /// Safe maker of Callbacks
        template<class Ctx
          , meta::is_empty ToFront
          , meta::is_empty ToMod
          , meta::is_empty ReceivePartialFileList
          , meta::is_empty ReceiveFileContentsResponse
          , meta::is_empty ReceiveFileDataRequest
          , meta::is_empty ReceiveFileContentsRequest
          , meta::is_empty ReceiveCapabilityFlags
        >
        struct Builder
        {
            Ctx & ctx;
            [[no_unique_address]] ToFront send_to_front_channel;
            [[no_unique_address]] ToMod send_to_mod_channel;
            [[no_unique_address]] ReceivePartialFileList receive_partial_file_list;
            [[no_unique_address]] ReceiveFileContentsResponse receive_file_contents_response;
            [[no_unique_address]] ReceiveFileDataRequest receive_file_data_request;
            [[no_unique_address]] ReceiveFileContentsRequest receive_file_contents_request;
            [[no_unique_address]] ReceiveCapabilityFlags receive_capability_flags;

            operator Callbacks () const
            {
                #define REDEMPTION_CALLBACK_WRAP(name) .name =                     \
                {                                                                  \
                    [](void * ctx, auto... args)                                   \
                        -> typename result_of_mem<decltype(Callbacks::name)>::type \
                    {                                                              \
                        return decltype(name){}(*static_cast<Ctx*>(ctx), args...); \
                    }                                                              \
                }
                return Callbacks {
                    .ctx = &ctx,
                    REDEMPTION_CALLBACK_WRAP(send_to_front_channel),
                    REDEMPTION_CALLBACK_WRAP(send_to_mod_channel),
                    REDEMPTION_CALLBACK_WRAP(receive_partial_file_list),
                    REDEMPTION_CALLBACK_WRAP(receive_file_contents_response),
                    REDEMPTION_CALLBACK_WRAP(receive_file_data_request),
                    REDEMPTION_CALLBACK_WRAP(receive_file_contents_request),
                    REDEMPTION_CALLBACK_WRAP(receive_capability_flags),
                };
                #undef REDEMPTION_CALLBACK_WRAP
            }

        private:
            template<class MemFn, class = void>
            struct result_of_mem;

            template<class Result, class... Ts, class Dummy>
            struct result_of_mem<not_null_ptr<Result(Ts...)>, Dummy>
            {
                using type = Result;
            };
        };
    };


    CliprdrAdapter(
        VncBogusClipboardInfiniteLoopStrategy infinite_loop_strategy,
        VncClipboardEncoding server_encoding,
        RdpToVncOptions rdp_to_vnc_options,
        Log log_mode,
        MaxRdpPduLen max_rdp_pdu_len,
        EventContainer& events,
        // TODO move to each member functions ?
        Callbacks callbacks
    ) noexcept;

    CbCapabilityFlags cb_capabilities() const noexcept
    {
        return m_cb_capability_flags;
    }

    void init_cliprdr_server();

    bool enable_file_transfer() noexcept;

    bool file_transfer_ready() const noexcept;
    bool has_file_capability() const noexcept;

    /// total_len is chunk.size() + remaining_len. This value is ignored
    /// when \c chunk_flags do not contains \c Rfb::ChunkFlags::First.
    void process_vnc_server_cut_text_message(
        bytes_view chunk, uint32_t remaining_len, Rfb::ChunkFlags chunk_flags);

    void process_rdp_client_message(
        bytes_view chunk, uint32_t total_len, ChannelFlags channel_flags);

    CbMsgType rdp_client_last_msg_type() const noexcept
    {
        return m_cliprdr_reader.last_msg_type();
    }

    /// Send FormatDataRequest when available
    /// \return true when FileGroupDescriptorW is available, otherwise false.
    bool request_file_list();
    bool is_requested_file_list() const noexcept;

    bool has_file_group_descriptor_format() const noexcept;

    /// Send FileGroupDescriptorW
    /// \return true when FileGroupDescriptorW is available, otherwise false.
    bool send_format_list_with_files();


private:
    class P;
    friend P;

    void send_to_front_channel(bytes_view chunk);

    void send_client_cut_text_to_mod();

    void request_text_data(char const * extra_msg);

    void push_in_cb_data(bytes_view data);


    static constexpr uint32_t MAX_CLIPBOARD_DATA_SIZE = 0xffff;

    enum class Flags : uint32_t;
    REDEMPTION_DECLARE_ENUM_FLAGS_IN_CLASS(Flags)

    Flags m_internal_flags;
    uint16_t m_max_rdp_pdu_len;
    VncClipboardEncoding m_server_encoding;

    ChannelFlagsChecker m_channel_flags_checker;

    uint16_t m_cb_data_len = 0;

    CbCapabilityFlags m_cb_capability_flags;
    CbFormatID m_client_file_format_id;

    CliprdrReader m_cliprdr_reader;

    Callbacks m_callbacks;

    EventsGuard m_events_guard;
    EventRef m_request_data_timer;
    MonotonicTimePoint m_timestamp_of_last_format_data_response;

    // used as clipboard text data or FileDescriptor buffer
    uint8_t m_cb_data[MAX_CLIPBOARD_DATA_SIZE];
};

} // namespace VNC
