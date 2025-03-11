/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "mod/vnc/rdp_adapters/cliprdr_adapter.hpp"
#include "utils/sugar/push.hpp"
#include "utils/sugar/bytes_equal.hpp"
#include "utils/sugar/int_to_chars.hpp"
#include "utils/sugar/bounded_array_view.hpp"

#include "test_only/test_framework/redemption_unit_tests.hpp"

#include <vector>

// TODO add tests on encoding, crlf,

namespace
{
    struct TestCliprdrAdapterMsg
    {
        enum class Type : uint8_t
        {
            NoConsumed,
            NoMsg,
            ToRDP,
            ToVNC,
        };

        Type type;
        std::vector<uint8_t> data;
        size_t total_len;
        VNC::ChannelFlags channel_flags;

        static TestCliprdrAdapterMsg
        toRDP(bytes_view msg, size_t total_len, VNC::ChannelFlags channel_flags)
        {
            return {
                Type::ToRDP,
                msg.as<std::vector>(),
                total_len,
                channel_flags | VNC::ChannelFlags::ShowProtocol,
            };
        }

        static TestCliprdrAdapterMsg
        toRDP(bytes_view msg)
        {
            return toRDP(
                msg,
                checked_int{msg.size()},
                VNC::ChannelFlags::First | VNC::ChannelFlags::Last
            );
        }

        static TestCliprdrAdapterMsg toVNC(bytes_view msg)
        {
            return {Type::ToVNC, msg.as<std::vector>(), {}, {}};
        }

        bool operator == (TestCliprdrAdapterMsg const& msg) const noexcept
        {
            return type == msg.type
                && total_len == msg.total_len
                && channel_flags == msg.channel_flags
                && bytes_equal(data, msg.data);
        }
    };
}

#if !REDEMPTION_UNIT_TEST_FAST_CHECK
# include "test_only/test_framework/compare_collection.hpp"

namespace
{

static ut::assertion_result test_comp_msg(TestCliprdrAdapterMsg a, TestCliprdrAdapterMsg b)
{
    ut::assertion_result ar(true);

    if (REDEMPTION_UNLIKELY(!(a == b))) {
        ar = false;

        auto put = [&](std::ostream& out, TestCliprdrAdapterMsg const& msg){
            switch (msg.type)
            {
            case TestCliprdrAdapterMsg::Type::NoConsumed:
                out << "NoConsumed";
                break;

            case TestCliprdrAdapterMsg::Type::NoMsg:
                out << "NoMsg";
                break;

            case TestCliprdrAdapterMsg::Type::ToRDP:
                out
                  << "ToRDP{.data=" << ut::AsHexView{msg.data}
                  << ", .total_len="
                  << int_to_decimal_chars(msg.total_len).sv()
                  << ", .channel_flags=0x"
                  << int_to_hexadecimal_upper_chars(underlying_cast(msg.channel_flags)).sv()
                  << "}"
                ;
                break;

            case TestCliprdrAdapterMsg::Type::ToVNC:
                out << "toVNC{" << ut::AsHexView{msg.data} << "}";
                break;
            }
        };

        auto& out = ar.message().stream();
        out << "[";
        ut::put_data_with_diff(out, a, "!=", b, put);
        out << "]";
    }

    return ar;
}

}

RED_TEST_DISPATCH_COMPARISON_EQ((), (::TestCliprdrAdapterMsg), (::TestCliprdrAdapterMsg), ::test_comp_msg)
#endif


namespace
{
namespace NsTestCliprdrAdapter
{
    using namespace VNC;
    using ChunkFlags = Rfb::ChunkFlags;

    using Msg = TestCliprdrAdapterMsg;
    using Msgs = std::initializer_list<Msg>;

    constexpr uint16_t MAX_PDU_LEN = 100;

    struct Ctx
    {
        size_t nb_call {};
        array_view<Msg> output_msgs {};

        void (*receive_file_data_request)(
            Ctx & ctx
        ) {};

        void (*receive_capability_flags)(
            Ctx & ctx
        ) {};

        void (*receive_file_contents_request)(
            Ctx & ctx,
            FileContentsRequest const & req
        ) {};

        void (*receive_file_contents_response)(
            Ctx & ctx,
            bytes_view data,
            uint32_t remaining_len,
            bool is_ok,
            VNC::ChannelFlags channel_flags
        ) {};

        uint16_t (*receive_partial_file_list)(
            Ctx & ctx,
            bytes_view data,
            CliprdrAdapter::Callbacks::FileDescriptorBufferView buffer,
            uint16_t buffer_offset,
            VNC::ChannelFlags channel_flags,
            uint32_t total_item
        ) {};

        void test(Msg msg)
        {
            auto expected_msg = (nb_call < output_msgs.size())
                ? output_msgs[nb_call]
                : Msg{Msg::Type::NoMsg, {}, {}, {}};
            nb_call++;
            RED_TEST_CONTEXT("msg: " << nb_call << "/" << output_msgs.size())
            {
                RED_CHECK(msg == expected_msg);
            }
        }

        void check_not_consumed()
        {
            for (; nb_call < output_msgs.size(); ++nb_call)
            {
                test(Msg{Msg::Type::NoConsumed, {}, {}, {}});
            }

            RED_CHECK_MESSAGE(!receive_file_contents_response, "receive_partial_file_list() is not called");
            RED_CHECK_MESSAGE(!receive_file_contents_request, "receive_partial_file_list() is not called");
            RED_CHECK_MESSAGE(!receive_file_data_request, "receive_partial_file_list() is not called");
            RED_CHECK_MESSAGE(!receive_partial_file_list, "receive_partial_file_list() is not called");
            RED_CHECK_MESSAGE(!receive_capability_flags, "receive_capability_flags() is not called");

            receive_capability_flags = nullptr;
            receive_partial_file_list = nullptr;
            receive_file_data_request = nullptr;
            receive_file_contents_request = nullptr;
            receive_file_contents_response = nullptr;
        }
    };

    static_assert(underlying_cast(ChannelFlags::First) == underlying_cast(ChunkFlags::First));
    static_assert(underlying_cast(ChannelFlags::Last) == underlying_cast(ChunkFlags::Last));

    struct Data
    {
        unsigned flags;
        uint32_t total_len;
        bytes_view chunk;

        Data(chars_view chunk) noexcept
            : Data(
                chunk,
                checked_int(chunk.size()),
                ChannelFlags::First | ChannelFlags::Last | ChannelFlags::ShowProtocol
            )
        {}

        Data(chars_view chunk, uint32_t total_len, ChannelFlags flags) noexcept
            : flags(underlying_cast(flags))
            , total_len(total_len)
            , chunk(chunk)
        {}
    };

    template<class Fn>
    void test_cb(Ctx & ctx, Msgs output_msgs, Fn fn)
    {
        ctx.nb_call = 0;
        ctx.output_msgs = output_msgs;
        fn();
        ctx.check_not_consumed();
    }

    void test_rdp_msg(Ctx & ctx, CliprdrAdapter & cb, Data data, Msgs output_msgs)
    {
        test_cb(ctx, output_msgs, [&]{
            cb.process_rdp_client_message(
                data.chunk,
                data.total_len + VNC::CliprdrHeader::pdu_len(),
                static_cast<ChannelFlags>(data.flags)
            );
        });
    }

    void test_vnc_msg(Ctx & ctx, CliprdrAdapter & cb, Data data, Msgs output_msgs)
    {
        test_cb(ctx, output_msgs, [&]{
            cb.process_vnc_server_cut_text_message(
                data.chunk,
                data.total_len,
                static_cast<ChunkFlags>(data.flags)
            );
        });
    };

    void test_delay(Ctx & ctx, EventManager & events, Msgs output_msgs){
        RED_CHECK(!output_msgs.size() == events.is_empty());

        test_cb(ctx, output_msgs, [&]{
            events.get_writable_time_base().monotonic_time += std::chrono::seconds{1};
            events.execute_events([](int){ return false; }, false);
        });

        RED_CHECK(events.is_empty());
    };

    static constexpr auto noflags = ChannelFlags::NoFlags;
    static constexpr auto first = ChannelFlags::First;
    static constexpr auto last = ChannelFlags::Last;
    static constexpr auto show_protocol = ChannelFlags::ShowProtocol;

} // namespace NsTestCliprdrAdapter
} // anonymous namespace


RED_AUTO_TEST_CASE(TestCliprdrAdapter)
{

#define LINE_MARKER "  (line " RED_PP_STRINGIFY(__LINE__) ")"

#define TEST_RDP_MSG(desc, ...) \
    do { RED_TEST_CONTEXT(desc LINE_MARKER) { test_rdp_msg(ctx, cb, __VA_ARGS__); } } while (0)

#define TEST_VNC_CUT_TEXT(desc, ...) \
    do { RED_TEST_CONTEXT(desc LINE_MARKER) { test_vnc_msg(ctx, cb, __VA_ARGS__); } } while (0)

#define TEST_DELAY(desc, ...) \
    do { RED_TEST_CONTEXT(desc LINE_MARKER) { test_delay(ctx, events, __VA_ARGS__); } } while (0)

#define TEST_CB(desc, fn, ...) \
    do { RED_TEST_CONTEXT(desc LINE_MARKER) { test_cb(ctx, __VA_ARGS__, fn); } } while (0)

    using namespace NsTestCliprdrAdapter;
    using namespace std::chrono_literals;

    using Opt = CliprdrAdapter::RdpToVncOptions;
    using Loop = VncBogusClipboardInfiniteLoopStrategy;

    constexpr auto file_opts = Opt::FileResponse | Opt::FileRequest;
    constexpr auto non_file_opts = Opt::NonFileResponse | Opt::NonFileRequest;

    struct D
    {
        Loop loop_opt;
        Opt transfer_opts;
    };

    for (auto transfer_opts : {
        /*
         * File and non file
         */
        Opt::NonFileResponse | Opt::NonFileRequest,
        Opt::NonFileResponse | Opt::NonFileRequest | Opt::FileResponse,
        Opt::NonFileResponse | Opt::NonFileRequest | Opt::FileRequest,
        Opt::NonFileResponse | Opt::NonFileRequest | Opt::FileResponse | Opt::FileRequest,

        Opt::NonFileResponse,
        Opt::NonFileResponse | Opt::FileResponse | Opt::FileRequest,
        Opt::NonFileResponse | Opt::FileResponse,
        Opt::NonFileResponse | Opt::FileRequest,

        Opt::NonFileRequest,
        Opt::NonFileRequest | Opt::FileResponse | Opt::FileRequest,
        Opt::NonFileRequest | Opt::FileResponse,
        Opt::NonFileRequest | Opt::FileRequest,

        /*
         * File only
         */
        Opt::FileResponse | Opt::FileRequest,
        Opt::FileResponse,
        Opt::FileRequest,
        Opt{},
    }) {
        for (auto loop_opt : { Loop::delayed, Loop::continued, Loop::duplicated })
        {
            RED_TEST_CONTEXT("params "
                << "= NonFileResp: " << int{flags_any(transfer_opts, Opt::NonFileResponse)}
                << "  NonFileReq: " << int{flags_any(transfer_opts, Opt::NonFileRequest)}
                << "  FileResp: " << int{flags_any(transfer_opts, Opt::FileResponse)}
                << "  FileReq: " << int{flags_any(transfer_opts, Opt::FileRequest)}
                << "  LoopStrat::" << (loop_opt == Loop::delayed
                                      ? "delayed"
                                      : loop_opt == Loop::continued
                                      ? "continued"
                                      : "duplicated"))
            {
                #define INIT_MEM(name)                                       \
                    .name = [](Ctx & ctx, auto... args) {                    \
                        if (ctx.name)                                        \
                        {                                                    \
                            ctx.name(ctx, args...);                          \
                            ctx.name = nullptr;                              \
                        }                                                    \
                        else                                                 \
                        {                                                    \
                            RED_CHECK(!"Unexpected call to " #name "()"[0]); \
                        }                                                    \
                    }

                Ctx ctx;
                EventManager events;
                CliprdrAdapter cb {
                    loop_opt,
                    VncClipboardEncoding::latin1,
                    transfer_opts,
                    // CliprdrAdapter::Log::No,
                    CliprdrAdapter::Log::Yes,
                    CliprdrAdapter::MaxRdpPduLen(MAX_PDU_LEN),
                    events.get_events(),
                    CliprdrAdapter::Callbacks::Builder {
                        .ctx = ctx,
                        .send_to_front_channel = [](Ctx & ctx, auto... args){
                            ctx.test(Msg::toRDP(args...));
                        },
                        .send_to_mod_channel = [](Ctx & ctx, bytes_view data){
                            ctx.test(Msg::toVNC(data));
                        },
                        .receive_partial_file_list = [](Ctx & ctx, auto... args) {
                            uint16_t res = 0;
                            if (ctx.receive_partial_file_list)
                            {
                                res = { ctx.receive_partial_file_list(ctx, args...) };
                                ctx.receive_partial_file_list = nullptr;
                            }
                            else
                            {
                                RED_CHECK(!"Unexpected call to receive_partial_file_list()"[0]);
                            }
                            return res;
                        },
                        INIT_MEM(receive_file_contents_response),
                        INIT_MEM(receive_file_data_request),
                        INIT_MEM(receive_file_contents_request),
                        INIT_MEM(receive_capability_flags),
                    },
                };

                #undef INIT_MEM

                cb.enable_file_transfer();

                events.get_writable_time_base().monotonic_time += std::chrono::seconds{1};

                RED_TEST_CONTEXT("Receive cliprdr data before server_init (should be ignored)")
                {
                    TEST_RDP_MSG("Client FormatList",
                        format_list_unicode_in_long_format_with_header,
                        {}
                    );
                    TEST_VNC_CUT_TEXT("VNC owned + Mod CutText",
                        "XY"_av,
                        {}
                    );
                }

                RED_TEST_CONTEXT("Init")
                {
                    RED_TEST_CONTEXT("Server caps and monitor ready" LINE_MARKER)
                    {
                        static constexpr auto file_caps =
                            "\7\0""\0\0""\x10\0\0\0"
                            "\1\0""\0\0"
                            "\1\0\xC\0\1\0\0\0\x2e\0\0\0"
                            ""_av;
                        static constexpr auto non_file_caps =
                            "\7\0""\0\0""\x10\0\0\0"
                            "\1\0""\0\0"
                            "\1\0\xC\0\1\0\0\0\2\0\0\0"
                            ""_av;

                        Msg output_msgs[] = {
                            Msg::toRDP(flags_any(transfer_opts, file_opts)
                                ? file_caps
                                : non_file_caps
                            ),
                            Msg::toRDP(
                                "\1\0""\0\0""\0\0\0\0"
                                ""_av
                            ),
                        };
                        ctx.nb_call = 0;
                        ctx.output_msgs = flags_any(transfer_opts, non_file_opts | file_opts)
                            ? output_msgs
                            : array_view<Msg>{};
                        cb.init_cliprdr_server();
                        ctx.check_not_consumed();
                    }

                    if (transfer_opts != VNC::CliprdrAdapter::RdpToVncOptions::None)
                    {
                        ctx.receive_capability_flags = [](Ctx &){};
                    }
                    TEST_RDP_MSG("Client send Caps",
                        // header
                        "\7\0""\1\0""\x10\x0\x0\x0"
                        // nb set
                        "\1\0""\0\0"
                        // general capset | UseLongFormatNames | FileStream
                        "\1\0\xC\0\1\0\0\0\x2e\0\0\0"_av,
                        {}
                    );

                    RED_CHECK(cb.has_file_capability() == flags_any(transfer_opts, file_opts));

                    TEST_RDP_MSG("Client send FormatList",
                        format_list_unicode_in_long_format_with_header,
                        flags_any(transfer_opts, Opt::NonFileResponse)
                            ? Msgs {
                                Msg::toRDP(format_list_response_ok_with_header),
                                Msg::toRDP(format_data_request_unicode_with_header),
                            }
                            :
                        flags_any(transfer_opts, Opt::NonFileRequest | file_opts)
                            ? Msgs {
                                Msg::toRDP(format_list_response_ok_with_header),
                            }
                        // else
                            : Msgs {
                            }
                    );

                    RED_TEST_CONTEXT("Get unicode data from RDP (1)")
                    {
                        TEST_RDP_MSG("Client send FormatDataResponse",
                            // header
                            "\5\0""\1\0""\x8\x0\x0\x0"
                            // unicode data
                            "x\0y\0z\0""\0\0"_av,
                            flags_any(transfer_opts, Opt::NonFileResponse)
                                ? Msgs {
                                    Msg::toVNC("\6\0\0\0""\0\0\0\3""xyz"_av),
                                }
                                : Msgs {
                                }
                        );
                    }
                }

                RED_TEST_CONTEXT("Get unicode data (loop with bad client)")
                {
                    events.get_writable_time_base().monotonic_time += 100ms;

                    if (loop_opt == Loop::delayed)
                    {
                        TEST_RDP_MSG("Client send FormatList",
                            format_list_unicode_in_long_format_with_header,
                            flags_any(transfer_opts, non_file_opts | file_opts)
                                ? Msgs {
                                    Msg::toRDP(format_list_response_ok_with_header),
                                }
                            // else
                                : Msgs {
                                }
                        );

                        TEST_DELAY("Send send FormatListResponse",
                            flags_any(transfer_opts, Opt::NonFileResponse)
                                ? Msgs {
                                    Msg::toRDP(format_data_request_unicode_with_header)
                                }
                            // else
                                : Msgs {
                                }
                        );

                        TEST_RDP_MSG("Client send FormatDataResponse",
                            // header
                            "\5\0""\1\0""\x8\x0\x0\x0"
                            // unicode data
                            "a\0b\0c\0""\0\0"_av,
                            flags_any(transfer_opts, Opt::NonFileResponse)
                                ? Msgs {
                                    Msg::toVNC("\6\0\0\0""\0\0\0\3""abc"_av),
                                }
                            // else
                                : Msgs {
                                }
                        );
                    }
                    else
                    {
                        TEST_RDP_MSG("Client send FormatList",
                            format_list_unicode_in_long_format_with_header,
                            flags_any(transfer_opts, Opt::NonFileResponse)
                                ? Msgs {
                                    Msg::toRDP(format_list_response_ok_with_header),
                                    Msg::toRDP(format_list_unicode_in_long_format_with_header)
                                }
                                :
                            flags_any(transfer_opts, Opt::NonFileRequest | file_opts)
                                ? Msgs {
                                    Msg::toRDP(format_list_response_ok_with_header),
                                }
                            // else
                                : Msgs {
                                }
                        );
                    }

                    events.get_writable_time_base().monotonic_time += 1s;
                }

                RED_TEST_CONTEXT("Receive VNC unicode data + get from RDP (1)")
                {
                    TEST_VNC_CUT_TEXT("VNC owned + Mod CutText",
                        "XY"_av,
                        flags_any(transfer_opts, Opt::NonFileRequest)
                            ? Msgs {
                                Msg::toRDP(format_list_unicode_in_long_format_with_header),
                            }
                        // else
                            : Msgs {
                            }
                    );

                    TEST_RDP_MSG("Client send FormatListResponse",
                        format_list_response_ok_with_header,
                        Msgs {
                        }
                    );

                    TEST_RDP_MSG("Client send FormatDataRequest",
                        format_data_request_unicode_with_header,
                        flags_any(transfer_opts, Opt::NonFileRequest)
                            ? Msgs {
                                Msg::toRDP("\5\0""\1\0""\6\0\0\0""X\0Y\0""\0\0"_av)
                            }
                            :
                        flags_any(transfer_opts, Opt::NonFileResponse | file_opts)
                            ? Msgs {
                               Msg::toRDP(format_data_response_fail_with_header),
                            }
                        // else
                            : Msgs {
                            }
                    );
                }

                RED_TEST_CONTEXT("Receive VNC unicode data (3 chunks) + get from RDP (2) (4 chunks)")
                {
                    TEST_VNC_CUT_TEXT("RDP owned + Mod CutText (first)",
                        Data("aaaaaaaaaa""aaaaaaaaaa""aaaaaaaaaa"
                             "aaaaaaaaaa""aaaaaaaaaa""aaaaaaaaaa"_av,
                             60, first),
                        {}
                    );

                    TEST_VNC_CUT_TEXT("RDP owned + Mod CutText (continuation)",
                        Data{"aaaaaaaaaa""aaaaaaaaaa""aaaaaaaaaa"
                             "aaaaaaaaaa""aaaaaaaaaa""aaaaaaaaaa"_av,
                             0, noflags},
                        {}
                    );

                    TEST_VNC_CUT_TEXT("RDP owned + Mod CutText (last)",
                        Data{"aaaaaaaaaa""aaaaaaaaaa""aaaaaaaaaa"
                             "aaaaaaaaaa""aaaaaaaaaa""aaaaaaaaaa"_av,
                             0, last},
                        flags_any(transfer_opts, Opt::NonFileRequest)
                            ? Msgs {
                                Msg::toRDP(format_list_unicode_in_long_format_with_header),
                            }
                        // else
                            : Msgs {
                            }
                    );

                    TEST_RDP_MSG("Client send FormatListResponse",
                        format_list_response_ok_with_header,
                        Msgs {
                        }
                    );

                    // \x6a\x01 == 362 (60 chars * 2 for unicode + null char (2 bytes))
                    static constexpr auto unicode_text_pdu = "\5\0""\1\0""\x6A\x01\0\0"
                        "a\0a\0a\0a\0a\0a\0a\0a\0"
                        "a\0a\0a\0a\0a\0a\0a\0a\0"
                        "a\0a\0a\0a\0a\0a\0a\0a\0"
                        "a\0a\0a\0a\0a\0a\0a\0a\0"
                        "a\0a\0a\0a\0a\0a\0a\0a\0"
                        "a\0a\0a\0a\0a\0a\0a\0a\0"
                        "a\0a\0a\0a\0a\0a\0a\0a\0"
                        "a\0a\0a\0a\0a\0a\0a\0a\0"
                        "a\0a\0a\0a\0a\0a\0a\0a\0"
                        "a\0a\0a\0a\0a\0a\0a\0a\0"
                        "a\0a\0a\0a\0a\0a\0a\0a\0"
                        "a\0a\0a\0a\0a\0a\0a\0a\0"
                        "a\0a\0a\0a\0a\0a\0a\0a\0"
                        "a\0a\0a\0a\0a\0a\0a\0a\0"
                        "a\0a\0a\0a\0a\0a\0a\0a\0"
                        "a\0a\0a\0a\0a\0a\0a\0a\0"
                        "a\0a\0a\0a\0a\0a\0a\0a\0"
                        "a\0a\0a\0a\0a\0a\0a\0a\0"
                        "a\0a\0a\0a\0a\0a\0a\0a\0"
                        "a\0a\0a\0a\0a\0a\0a\0a\0"
                        "a\0a\0a\0a\0a\0a\0a\0a\0"
                        "a\0a\0a\0a\0a\0a\0a\0a\0"
                        "a\0a\0a\0a\0"
                        // null terminated
                        "\x00\x00"_av;

                    TEST_RDP_MSG("Client send FormatDataRequest",
                        format_data_request_unicode_with_header,
                        flags_any(transfer_opts, Opt::NonFileRequest)
                            ? Msgs {
                                Msg::toRDP(unicode_text_pdu),
                            }
                            :
                        flags_any(transfer_opts, non_file_opts | file_opts)
                            ? Msgs {
                                Msg::toRDP(format_data_response_fail_with_header),
                            }
                        // else
                            : Msgs {
                            }
                    );
                }

                RED_TEST_CONTEXT("Cross unicode data receiving, RDP then VNC")
                {
                    TEST_RDP_MSG("Client send FormatList",
                        format_list_unicode_in_long_format_with_header,
                        flags_any(transfer_opts, Opt::NonFileResponse)
                            ? Msgs {
                                Msg::toRDP(format_list_response_ok_with_header),
                                Msg::toRDP(format_data_request_unicode_with_header),
                            }
                            :
                        flags_any(transfer_opts, Opt::NonFileRequest | file_opts)
                            ? Msgs {
                                Msg::toRDP(format_list_response_ok_with_header),
                            }
                        // else
                            : Msgs{
                            }
                    );

                    // total_len = 10
                    TEST_RDP_MSG("Client send FormatDataResponse (1/2)",
                        Data("\x05\x00\x01\x00\x6e\x00\x00\x00"
                             "a\0a\0a\0a\0a\0a\0a\0"
                             "a\0a\0a\0a\0a\0a\0a\0"
                             "a\0a\0a\0a\0a\0a\0a\0"
                             "a\0a\0a\0a\0a\0a\0a\0"
                             "a\0a\0a\0a\0a\0a\0a\0"
                             "a\0a\0a\0a\0a\0a\0a\0"
                             "a\0a\0a\0a\0a\0a\0a\0"_av,
                             MAX_PDU_LEN + 10,
                             first),
                        {}
                    );

                    TEST_VNC_CUT_TEXT("Receive VNC data (1/1), skip RDP data",
                        Data("aaaaaaaaaa""aaaaaaaaaa""aaaaaaaaaa"
                             "aaaaaaaaaa""aaaaaaaaaa""aaaaaaaaaa"
                             "aaaaaaaaaa""aaaaaaaaaa""aaaaaaaaaa"
                             "aaaaaaaaaa"_av),
                        flags_any(transfer_opts, Opt::NonFileRequest)
                            ? Msgs {
                                Msg::toRDP(format_list_unicode_in_long_format_with_header),
                            }
                        // else
                            : Msgs {
                            }
                    );

                    TEST_RDP_MSG("Client send FormatDataResponse (2/2)",
                        Data("a\0a\0a\0a\0a\0a\0"_av, 0, last),
                        flags_any(transfer_opts, Opt::NonFileRequest)
                            ? Msgs {
                                // no toVNC
                            }
                            :
                        flags_any(transfer_opts, Opt::NonFileResponse)
                            ? Msgs {
                                Msg::toVNC(
                                    "\6\0\0\0""\0\0\0\x36"
                                    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                                    ""_av
                                )
                            }
                        // else
                            : Msgs {
                            }
                    );
                }

                RED_TEST_CONTEXT("Cross unicode data receiving, VNC then RDP")
                {
                    events.get_writable_time_base().monotonic_time += 1s;

                    TEST_RDP_MSG("Client send FormatList",
                        format_list_unicode_in_long_format_with_header,
                        flags_any(transfer_opts, Opt::NonFileResponse)
                            ? Msgs {
                                Msg::toRDP(format_list_response_ok_with_header),
                                Msg::toRDP(format_data_request_unicode_with_header),
                            }
                            :
                        flags_any(transfer_opts, Opt::NonFileRequest | file_opts)
                            ? Msgs {
                                Msg::toRDP(format_list_response_ok_with_header),
                            }
                        // else
                            : Msgs {
                            }
                    );

                    TEST_VNC_CUT_TEXT("Receive VNC data (1/2)",
                        Data("aaaaaaaaaa""aaaaaaaaaa""aaaaaaaaaa"
                             "aaaaaaaaaa""aaaaaaaaaa""aaaaaaaaaa"
                             "aaaaaaaaaa""aaaaaaaaaa""aaaaaaaaaa"
                             "aaaaaaaaaa"_av, 1, first),
                        {}
                    );

                    TEST_RDP_MSG("Client send FormatDataResponse (1/3) skip VNC data",
                        // total_len = 16
                        Data(
                            "\x05\x00""\x01\x00""\x10\x00\x00\x00""a\0a\0a\0a\0"_av,
                            16,
                            first
                        ),
                        Msgs {
                        }
                    );

                    TEST_VNC_CUT_TEXT("Receive VNC data (2/3)",
                        Data("a"_av, 0, last),
                        flags_any(transfer_opts, Opt::NonFileRequest)
                            ? Msgs {
                                Msg::toRDP(format_list_unicode_in_long_format_with_header),
                            }
                        // else
                            : Msgs {
                            }
                    );

                    TEST_RDP_MSG("Client send FormatDataResponse (3/3) skip VNC data",
                        // total_len = 16
                        Data("a\0a\0a\0\0\0"_av, 0, last),
                        flags_any(transfer_opts, Opt::NonFileRequest)
                            ? Msgs {
                            }
                            :
                        flags_any(transfer_opts, Opt::NonFileResponse)
                            ? Msgs {
                                Msg::toVNC("\6\0\0\0""\0\0\0\x07""aaaaaaa"_av),
                            }
                        // else
                            : Msgs {
                            }
                    );
                }

                RED_TEST_CONTEXT("Too many VNC unicode data")
                {
                    char bigbuf[0xffff] {};

                    TEST_VNC_CUT_TEXT("Receive big VNC data (1/2)",
                        Data(make_array_view(bigbuf), 2, ChannelFlags::First),
                        {}
                    );

                    TEST_VNC_CUT_TEXT("Receive big VNC data (1/2)",
                        Data("xx"_av, 0, ChannelFlags::Last),
                        flags_any(transfer_opts, Opt::NonFileRequest)
                            ? Msgs {
                                Msg::toRDP(format_list_unicode_in_long_format_with_header),
                            }
                        // else
                            : Msgs {
                            }
                    );

                    TEST_RDP_MSG("Client send FormatListResponse",
                        format_list_response_ok_with_header,
                        Msgs {
                        }
                    );

                    TEST_RDP_MSG("Client send FormatDataRequest (big data)",
                        format_data_request_unicode_with_header,
                        flags_any(transfer_opts, Opt::NonFileRequest)
                            ? Msgs {
                                Msg::toRDP(
                                    "\5\0""\1\0""\xC2\0\0\0"
                                    "T\0h\0e\0 \0t\0e\0x\0t\0 \0w\0a\0s\0 \0t\0o\0o\0 \0l\0o\0"
                                    "n\0g\0 \0t\0o\0 \0f\0i\0t\0 \0i\0n\0 \0t\0h\0e\0 \0c\0l\0"
                                    "i\0p\0b\0o\0a\0r\0d\0 \0b\0u\0f\0f\0e\0r\0.\0 \0T\0h\0e\0"
                                    " \0b\0u\0f\0f\0e\0r\0 \0s\0i\0z\0e\0 \0i\0s\0 \0l\0i\0m\0"
                                    "i\0t\0e\0d\0 \0t\0o\0 \0""6\0""5\0""5\0""3\0""5\0 \0b\0"
                                    "y\0t\0e\0s\0.\0""\0\0"_av
                                ),
                            }
                            :
                        flags_any(transfer_opts, Opt::NonFileResponse | file_opts)
                            ? Msgs {
                                Msg::toRDP(format_data_response_fail_with_header),
                            }
                        // else
                            : Msgs{
                            }
                    );
                }

                RED_TEST_CONTEXT("Too many RDP unicode data")
                {
                    events.get_writable_time_base().monotonic_time += 1s;

                    TEST_RDP_MSG("Client send FormatList",
                        format_list_unicode_in_long_format_with_header,
                        flags_any(transfer_opts, Opt::NonFileResponse)
                            ? Msgs {
                                Msg::toRDP(format_list_response_ok_with_header),
                                Msg::toRDP(format_data_request_unicode_with_header),
                            }
                            :
                        flags_any(transfer_opts, Opt::NonFileRequest | file_opts)
                            ? Msgs {
                                Msg::toRDP(format_list_response_ok_with_header),
                            }
                        // else
                            : Msgs {
                            }
                    );

                    constexpr unsigned buf_len = 0xffff;
                    char bigbuf[buf_len] {};
                    uint32_t total_len = buf_len + 2;

                    OutStream out_stream{bigbuf};
                    CliprdrHeader{
                        CbMsgType::FormatDataResponse,
                        CbMsgFlags::ResponseOk,
                        total_len
                    }.write_unchecked(out_stream);

                    TEST_RDP_MSG("Client big FormatDataResponse (1/2)",
                        Data(make_array_view(bigbuf), total_len, ChannelFlags::First),
                        {}
                    );

                    TEST_RDP_MSG("Client big FormatDataResponse (2/2)",
                        Data("a\0a\0a\0a\0\0\0"_av, total_len, ChannelFlags::Last),
                        flags_any(transfer_opts, Opt::NonFileResponse)
                            ? Msgs {
                                Msg::toVNC(
                                    "\6\0\0\0""\0\0\0\x60"
                                    "The text was too long to fit in the clipboard buffer. "
                                    "The buffer size is limited to 65535 bytes."_av
                                ),
                            }
                        // else
                            : Msgs {
                            }
                    );
                }

                constexpr auto request_file_group_descriptor_w =
                    "\4\0""\0\0""\4\0\0\0" "\xA1\xA4\0\0"_av;

                constexpr auto file_contents_request_size =
                    "\x8\0""\0\0""\x18\0\0\0"
                    "\7\0\0\0" "\5\0\0\0" "\1\0\0\0"
                    "\0\0\0\0" "\0\0\0\0" "\0\0\0\0"_av;

                constexpr auto file_contents_request_range =
                    "\x8\0""\0\0""\x18\0\0\0"
                    "\7\0\0\0" "\5\0\0\0" "\2\0\0\0"
                    "\x53\0\0\0" "\x51\0\0\0" "\6\0\0\0"_av;

                RED_TEST_CONTEXT("Receive VNC files + get from RDP")
                {
                    TEST_CB("Send FormatList to RDP",
                        [&]{ cb.send_format_list_with_files(); },
                        flags_any(transfer_opts, Opt::FileRequest)
                            ? Msgs {
                                Msg::toRDP(format_list_custom_file_group_descriptor_w_in_long_format_with_header)
                            }
                        // else
                            : Msgs {
                            }
                    );

                    TEST_RDP_MSG("Client send FormatListResponse",
                        format_list_response_ok_with_header,
                        Msgs {
                        }
                    );

                    if (flags_any(transfer_opts, Opt::FileRequest))
                    {
                        ctx.receive_file_data_request = [](Ctx & /*ctx*/) {
                            // ... send file list ...
                        };
                    }
                    TEST_RDP_MSG("Client send FormatDataRequest",
                        request_file_group_descriptor_w,
                        flags_any(transfer_opts, Opt::FileRequest)
                            ? Msgs {
                                // ... send file list ...
                            }
                            :
                        flags_any(transfer_opts, non_file_opts | file_opts)
                            ? Msgs {
                                Msg::toRDP(format_data_response_fail_with_header),
                            }
                        // else
                            : Msgs {
                            }
                    );

                    if (flags_any(transfer_opts, Opt::FileRequest))
                    {
                        ctx.receive_file_contents_request = [](
                            Ctx & /*ctx*/,
                            VNC::FileContentsRequest const & contents_req
                        ) {
                            RED_CHECK(contents_req.streamId == VNC::CbStreamId{7});
                            RED_CHECK(contents_req.lindex == VNC::CbLindex{5});
                            RED_CHECK(contents_req.dwFlags == VNC::CbFileContentsType::Size);
                            RED_CHECK(contents_req.nPositionLow == 0);
                            RED_CHECK(contents_req.nPositionHigh == 0);
                            RED_CHECK(contents_req.cbRequested == 0);
                            RED_CHECK(contents_req.clipDataId == VNC::ClipDataId{0});
                            // ... send file size ...
                        };
                    }
                    TEST_RDP_MSG("Client send FileContentsRequest (type = Size)",
                        file_contents_request_size,
                        flags_any(transfer_opts, Opt::FileRequest)
                            ? Msgs {
                                // ... send file size ...
                            }
                            :
                        flags_any(transfer_opts, non_file_opts | file_opts)
                            ? Msgs {
                                // FileContentsResponse error
                                Msg::toRDP("\x9\0""\2\0""\4\0\0\0""\7\0\0\0"_av),
                            }
                        // else
                            : Msgs {
                            }
                    );

                    if (flags_any(transfer_opts, Opt::FileRequest))
                    {
                        ctx.receive_file_contents_request = [](
                            Ctx & /*ctx*/,
                            VNC::FileContentsRequest const & contents_req
                        ) {
                            RED_CHECK(contents_req.streamId == VNC::CbStreamId{7});
                            RED_CHECK(contents_req.lindex == VNC::CbLindex{5});
                            RED_CHECK(contents_req.dwFlags == VNC::CbFileContentsType::Range);
                            RED_CHECK(contents_req.nPositionLow == 0x53);
                            RED_CHECK(contents_req.nPositionHigh == 0x51);
                            RED_CHECK(contents_req.cbRequested == 6);
                            RED_CHECK(contents_req.clipDataId == VNC::ClipDataId{0});
                            // ... send file data ...
                        };
                    }
                    TEST_RDP_MSG("Client FileContentsRequest (type = Range)",
                        file_contents_request_range,
                        flags_any(transfer_opts, Opt::FileRequest)
                            ? Msgs {
                                // ... send file data ...
                            }
                            :
                        flags_any(transfer_opts, non_file_opts | file_opts)
                            ? Msgs {
                                // FileContentsResponse error
                                Msg::toRDP("\x9\0""\2\0""\4\0\0\0""\7\0\0\0"_av),
                            }
                        // else
                            : Msgs {
                            }
                    );
                }

                RED_TEST_CONTEXT("Receive RDP files + get from VNC")
                {
                    TEST_RDP_MSG("Client send FormatList",
                        format_list_custom_file_group_descriptor_w_in_long_format_with_header,
                        flags_any(transfer_opts, non_file_opts | file_opts)
                            ? Msgs {
                                Msg::toRDP(format_list_response_ok_with_header),
                            }
                        // else
                            : Msgs {
                            }
                    );

                    RED_CHECK(!cb.is_requested_file_list());

                    TEST_CB("Send FormatDataRequest to RDP",
                        [&]{ cb.request_file_list(); },
                        flags_any(transfer_opts, Opt::FileResponse)
                            ? Msgs {
                                Msg::toRDP(request_file_group_descriptor_w)
                            }
                        // else
                            : Msgs {
                            }
                    );

                    RED_CHECK(
                        cb.is_requested_file_list() == flags_any(transfer_opts, Opt::FileResponse)
                    );

                    static constexpr auto format_data_response =
                        "\5\0""\1\0""\x53\2\0\0"
                        // cItems
                        "\1\0\0\0"
                        // flags
                        "\x64\x40\0\0"
                        // pad 32
                        "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
                        // fileAttributes
                        "\x20\0\0\0"
                        // pad 16
                        "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
                        // lastWriteTime
                        "\1\2\3\4\5\6\7\0"
                        // fileSizeHigh
                        "\7\3\2\5"
                        // fileSizeLow
                        "\6\3\2\5"
                        // unicodeFileName
                        "A\0f\0i\0l\0e""\0\0\0\0\0\0\0\0\0\0""\0\0\0\0\0\0\0\0\0\0"
                        "\0\0\0\0\0\0\0\0\0\0""\0\0\0\0\0\0\0\0\0\0""\0\0\0\0\0\0\0\0\0\0"
                        "\0\0\0\0\0\0\0\0\0\0""\0\0\0\0\0\0\0\0\0\0""\0\0\0\0\0\0\0\0\0\0"
                        "\0\0\0\0\0\0\0\0\0\0""\0\0\0\0\0\0\0\0\0\0""\0\0\0\0\0\0\0\0\0\0"
                        "\0\0\0\0\0\0\0\0\0\0""\0\0\0\0\0\0\0\0\0\0""\0\0\0\0\0\0\0\0\0\0"
                        "\0\0\0\0\0\0\0\0\0\0""\0\0\0\0\0\0\0\0\0\0""\0\0\0\0\0\0\0\0\0\0"
                        "\0\0\0\0\0\0\0\0\0\0""\0\0\0\0\0\0\0\0\0\0""\0\0\0\0\0\0\0\0\0\0"
                        "\0\0\0\0\0\0\0\0\0\0""\0\0\0\0\0\0\0\0\0\0""\0\0\0\0\0\0\0\0\0\0"
                        "\0\0\0\0\0\0\0\0\0\0""\0\0\0\0\0\0\0\0\0\0""\0\0\0\0\0\0\0\0\0\0"
                        "\0\0\0\0\0\0\0\0\0\0""\0\0\0\0\0\0\0\0\0\0""\0\0\0\0\0\0\0\0\0\0"
                        "\0\0\0\0\0\0\0\0\0\0""\0\0\0\0\0\0\0\0\0\0""\0\0\0\0\0\0\0\0\0\0"
                        "\0\0\0\0\0\0\0\0\0\0""\0\0\0\0\0\0\0\0\0\0""\0\0\0\0\0\0\0\0\0\0"
                        "\0\0\0\0\0\0\0\0\0\0""\0\0\0\0\0\0\0\0\0\0""\0\0\0\0\0\0\0\0\0\0"
                        "\0\0\0\0\0\0\0\0\0\0""\0\0\0\0\0\0\0\0\0\0""\0\0\0\0\0\0\0\0\0\0"
                        "\0\0\0\0\0\0\0\0\0\0""\0\0\0\0\0\0\0\0\0\0""\0\0\0\0\0\0\0\0\0\0"
                        "\0\0\0\0\0\0\0\0\0\0""\0\0\0\0\0\0\0\0\0\0""\0\0\0\0\0\0\0\0\0\0"
                        "\0\0\0\0\0\0\0\0\0\0""\0\0\0\0\0\0\0\0\0\0""\0\0\0\0\0\0\0\0\0\0"
                        "\0\0\0\0\0\0\0\0\0\0"_av;

                    if (flags_any(transfer_opts, Opt::FileResponse))
                    {
                        ctx.receive_partial_file_list = [](
                            Ctx & /*ctx*/,
                            bytes_view data,
                            VNC::CliprdrAdapter::Callbacks::FileDescriptorBufferView /*buffer*/,
                            uint16_t buffer_offset,
                            VNC::ChannelFlags channel_flags,
                            uint32_t total_item
                        ) {
                            RED_CHECK(data == format_data_response.drop_front(12));
                            RED_CHECK(buffer_offset == 0);
                            RED_CHECK(channel_flags == (first | last | show_protocol));
                            RED_CHECK(total_item == 1);
                            return uint16_t{};
                        };
                    }
                    TEST_RDP_MSG("Client send FormatDataResponse",
                        format_data_response,
                        Msgs {
                        }
                    );

                    // assume VNC side send FileContentsRequest

                    if (flags_any(transfer_opts, Opt::FileResponse))
                    {
                        ctx.receive_file_contents_response = [](
                            Ctx & /*ctx*/,
                            bytes_view data,
                            uint32_t remaining_len,
                            bool is_ok,
                            VNC::ChannelFlags channel_flags
                        ) {
                            RED_CHECK(remaining_len == 0);
                            RED_CHECK(is_ok);
                            RED_CHECK(data == "\3\0\0\0abc"_av);
                            RED_CHECK(channel_flags == (first | last | show_protocol));
                        };
                    }
                    TEST_RDP_MSG("Client send FileContentsResponse",
                        "\x9\0" "\1\0" "\7\0\0\0" "\3\0\0\0" "abc"_av,
                        Msgs {
                        }
                    );
                }
            }
        }
    }

#undef TEST_RDP_MSG
#undef TEST_VNC_CUT_TEXT
#undef TEST_DELAY
#undef LINE_MARKER
}
