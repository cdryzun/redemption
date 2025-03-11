/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "mod/vnc/rdp_adapters/rdpeclip.hpp"
#include "mod/vnc/rdp_adapters/rdp_channel.hpp"
#include "utils/sugar/int_to_chars.hpp"
#include "utils/sugar/push.hpp"

#include "test_only/test_framework/redemption_unit_tests.hpp"
#include "test_only/log_buffered.hpp"

#include <vector>

namespace
{
    using VNC::ChannelFlags;

    constexpr auto first = ChannelFlags::First;
    constexpr auto last = ChannelFlags::Last;
}

RED_AUTO_TEST_CASE(TestNotOverflowWithCbFlagsToStr)
{
    using namespace VNC;

    RED_CHECK(channel_flags_to_string(~ChannelFlags()) ==
        "CHANNEL_FLAG_FIRST|"
        "CHANNEL_FLAG_LAST|"
        "CHANNEL_FLAG_SHOW_PROTOCOL|"
        "CHANNEL_FLAG_SUSPEND|"
        "CHANNEL_FLAG_RESUME|"
        "CHANNEL_FLAG_SHADOW_PERSISTENT|"
        "CHANNEL_PACKET_COMPRESSED|"
        "CHANNEL_PACKET_AT_FRONT|"
        "CHANNEL_PACKET_FLUSHED|"
        "CompressionTypeMask|"
        "???"_av);

    RED_CHECK(msg_flags_to_string(~CbMsgFlags()) ==
        "CB_RESPONSE_OK|"
        "CB_RESPONSE_FAIL|"
        "CB_ASCII_NAMES|"
        "???"_av);

    RED_CHECK(capability_flags_to_string(~CbCapabilityFlags()) ==
        "CB_USE_LONG_FORMAT_NAMES|"
        "CB_STREAM_FILECLIP_ENABLED|"
        "CB_FILECLIP_NO_FILE_PATHS|"
        "CB_CAN_LOCK_CLIPDATA|"
        "CB_HUGE_FILE_SUPPORT_ENABLED|"
        "???"_av);
}

RED_AUTO_TEST_CASE(TestLogUnicodeToAscii)
{
    char buf[20];
    auto alpha
      = "A\0B\0C\0D\0E\0F\0G\0H\0I\0J\0K\0L\0M\0N\0O\0P\0Q\0R\0S\0T\0U\0V\0W\0X\0Y\0Z\0"_av;
    RED_CHECK(protop_fmt::init_utf16_to_utf8(buf, 20, alpha) == "ABCDEFGHIJKLMNOPQRS");
    RED_CHECK(protop_fmt::init_utf16_to_utf8(buf, 20, "a\0""\x23\xFF""b\0c\0"_av) == "a?bc");
}

RED_AUTO_TEST_CASE(TestLogFormatName)
{
    using namespace VNC;

    ut::log_buffered log_buffered;

    auto log = [&](CbFormatID format_id, auto name) -> std::string const & {
        log_buffered.clear();
        log_format_name("ctx: ", format_id, name);
        return log_buffered.buf();
    };

    RED_CHECK(log(CbFormatID::Text, AsciiName{""_sized_av})
        == "INFO -- ctx: FormatName{formatId=CF_TEXT(1), formatName=\"\", long_name=0, ascii=1}\n"_av);

    RED_CHECK(log(CbFormatID(5242), AsciiName{"MyCustomFormat"_sized_av})
        == "INFO -- ctx: FormatName{formatId=<unknown>(5242), formatName=\"MyCustomFormat\", long_name=0, ascii=1}\n"_av);

    RED_CHECK(log(CbFormatID::Text, UnicodeShortName{""_sized_av})
        == "INFO -- ctx: FormatName{formatId=CF_TEXT(1), formatName=\"\", long_name=0, ascii=0}\n"_av);

    RED_CHECK(log(CbFormatID(5242), UnicodeShortName{"M\0y\0C\0u\0s\0t\0o\0m\0F\0o\0r\0m\0a\0t\0"_sized_av})
        == "INFO -- ctx: FormatName{formatId=<unknown>(5242), formatName=\"MyCustomFormat\", long_name=0, ascii=0}\n"_av);

    RED_CHECK(log(CbFormatID::Text, UnicodeLongName{""_sized_av})
        == "INFO -- ctx: FormatName{formatId=CF_TEXT(1), formatName=\"\", long_name=1, ascii=0}\n"_av);

    RED_CHECK(log(CbFormatID(5242), UnicodeLongName{"M\0y\0C\0u\0s\0t\0o\0m\0F\0o\0r\0m\0a\0t\0"_sized_av})
        == "INFO -- ctx: FormatName{formatId=<unknown>(5242), formatName=\"MyCustomFormat\", long_name=1, ascii=0}\n"_av);

    RED_CHECK(log(CbFormatID::Text, UnicodeName{""_av, IsLongFormat::Yes})
        == "INFO -- ctx: FormatName{formatId=CF_TEXT(1), formatName=\"\", long_name=1, ascii=0}\n"_av);

    RED_CHECK(log(CbFormatID(5242), UnicodeName{"M\0y\0C\0u\0s\0t\0o\0m\0F\0o\0r\0m\0a\0t\0"_av, IsLongFormat::Yes})
        == "INFO -- ctx: FormatName{formatId=<unknown>(5242), formatName=\"MyCustomFormat\", long_name=1, ascii=0}\n"_av);

    RED_CHECK(log(CbFormatID::Text, UnicodeName{""_av, IsLongFormat::No})
        == "INFO -- ctx: FormatName{formatId=CF_TEXT(1), formatName=\"\", long_name=0, ascii=0}\n"_av);

    RED_CHECK(log(CbFormatID(5242), UnicodeName{"M\0y\0C\0u\0s\0t\0o\0m\0F\0o\0r\0m\0a\0t\0"_av, IsLongFormat::No})
        == "INFO -- ctx: FormatName{formatId=<unknown>(5242), formatName=\"MyCustomFormat\", long_name=0, ascii=0}\n"_av);
}

RED_AUTO_TEST_CASE(TestChannelFlagsChecker)
{
    using St = VNC::ChannelFlagsChecker::Status;
    VNC::ChannelFlagsChecker flags_checker;

    RED_CHECK(flags_checker.next(ChannelFlags{}) == St::UnspecifiedFlags);
    RED_CHECK(!flags_checker.is_multi_fragment());

    RED_CHECK(flags_checker.next(last) == St::MissingFirst);
    RED_CHECK(!flags_checker.is_multi_fragment());

    RED_CHECK(flags_checker.next(first | last) == St::Ok);
    RED_CHECK(!flags_checker.is_multi_fragment());

    RED_CHECK(flags_checker.next(first) == St::Ok);
    RED_CHECK(flags_checker.is_multi_fragment());

    RED_CHECK(flags_checker.next(first) == St::MissingLast);
    RED_CHECK(flags_checker.is_multi_fragment());

    RED_CHECK(flags_checker.next(last) == St::Ok);
    RED_CHECK(!flags_checker.is_multi_fragment());

    RED_CHECK(flags_checker.next(first) == St::Ok);
    RED_CHECK(flags_checker.is_multi_fragment());

    RED_CHECK(flags_checker.next(first | last) == St::MissingLast);
    RED_CHECK(!flags_checker.is_multi_fragment());
}

RED_AUTO_TEST_CASE(TestCliprdrReader)
{
    VNC::CliprdrReader header_reader;

    auto to_str = [&](VNC::CliprdrReader::Result r)
    {
        std::vector<char> v;
        switch (r.ec)
        {
            case VNC::CliprdrReader::Result::ErrorCode::Ok:
                push(v, "Ok"_av);
                break;

            case VNC::CliprdrReader::Result::ErrorCode::InsufficientData:
                push(v, "InsufficientData"_av);
                break;

            case VNC::CliprdrReader::Result::ErrorCode::DataTruncated:
                push(v, "DataTruncated"_av);
                break;

            case VNC::CliprdrReader::Result::ErrorCode::TotalLenTooShort:
                push(v, "TotalLenTooShort"_av);
                break;
        }
        push(v, " msg_type=0x"_av);
        push(v, int_to_fixed_hexadecimal_upper_chars(underlying_cast(header_reader.last_msg_type())));
        push(v, " msg_flags=0x"_av);
        push(v, int_to_fixed_hexadecimal_upper_chars(underlying_cast(header_reader.last_msg_flags())));
        push(v, " data="_av);
        push(v, r.partial_data().as_chars());
        return v;
    };

    // missing 1 byte
    RED_CHECK(
        to_str(header_reader.read("\x2\x0""\x1\x0""\x8\x0\x0"_av, 500, first | last))
        == "InsufficientData msg_type=0x0000 msg_flags=0x0000 data="_av
    );

    RED_CHECK(
        to_str(header_reader.read("\x2\x0""\x1\x0""\x8\x0\x0\x0""123"_av, 5, first | last))
        == "TotalLenTooShort msg_type=0x0002 msg_flags=0x0001 data="_av
    );

    RED_CHECK(
        to_str(header_reader.read("\x2\x0""\x1\x0""\x8\x0\x0\x0""123456789"_av, 500, first | last))
        // '9' is skipped
        == "Ok msg_type=0x0002 msg_flags=0x0001 data=12345678"_av
    );

    RED_CHECK(
        to_str(header_reader.read("\x2\x0""\x1\x0""\x8\x0\x0\x0""1234567"_av, 500, first | last))
        // last before consume header.dataLen
        == "DataTruncated msg_type=0x0002 msg_flags=0x0001 data=1234567"_av
    );

    RED_CHECK(
        to_str(header_reader.read("\x2\x0""\x1\x0""\x5\x0\x0\x0""123"_av, 500, first))
        == "Ok msg_type=0x0002 msg_flags=0x0001 data=123"_av
    );
    RED_TEST_INFO("previous flags: First");
    RED_CHECK(
        to_str(header_reader.read("78"_av, 500, last))
        == "Ok msg_type=0x0002 msg_flags=0x0001 data=78"_av
    );

    RED_CHECK(
        to_str(header_reader.read("\x2\x0""\x1\x0""\x8\x0\x0\x0""123"_av, 500, first))
        == "Ok msg_type=0x0002 msg_flags=0x0001 data=123"_av
    );
    RED_TEST_INFO("previous flags: First");
    RED_CHECK(
        to_str(header_reader.read("456"_av, 500, ChannelFlags::NoFlags))
        == "Ok msg_type=0x0002 msg_flags=0x0001 data=456"_av
    );
    RED_TEST_INFO("previous flags: First, NoFlags");
    RED_CHECK(
        to_str(header_reader.read("789"_av, 500, last))
        // '9' is skipped
        == "Ok msg_type=0x0002 msg_flags=0x0001 data=78"_av
    );

    RED_CHECK(
        to_str(header_reader.read("\x2\x0""\x1\x0""\x8\x0\x0\x0""123"_av, 500, first))
        == "Ok msg_type=0x0002 msg_flags=0x0001 data=123"_av
    );
    RED_TEST_INFO("previous flags: First");
    RED_CHECK(
        to_str(header_reader.read("456"_av, 500, ChannelFlags::NoFlags))
        == "Ok msg_type=0x0002 msg_flags=0x0001 data=456"_av
    );
    RED_TEST_INFO("previous flags: First, NoFlags");
    RED_CHECK(
        to_str(header_reader.read("789"_av, 500, ChannelFlags::NoFlags))
        // '9' is skipped
        == "Ok msg_type=0x0002 msg_flags=0x0001 data=78"_av
    );
    RED_TEST_INFO("previous flags: First, NoFlags, NoFlags");
    RED_CHECK(
        to_str(header_reader.read("0"_av, 500, last))
        // no data
        == "Ok msg_type=0x0002 msg_flags=0x0001 data="_av
    );
}

RED_AUTO_TEST_CASE(TestCliprdrExpectedClientPDUChecker)
{
    using VNC::CbMsgType;

    VNC::CliprdrExpectedClientPDUChecker expected_pdu_checker;

    RED_CHECK(!expected_pdu_checker.is_expected_msg(CbMsgType::ClipCaps));
    RED_CHECK(!expected_pdu_checker.is_expected_msg(CbMsgType::FormatList));
    RED_CHECK(!expected_pdu_checker.is_expected_msg(CbMsgType::FormatDataRequest));

    expected_pdu_checker.set_next_transition(CbMsgType::ClipCaps);
    RED_CHECK(!expected_pdu_checker.is_expected_msg(CbMsgType::ClipCaps));
    RED_CHECK(!expected_pdu_checker.is_expected_msg(CbMsgType::FormatDataRequest));
    RED_CHECK(expected_pdu_checker.is_expected_msg(CbMsgType::TempDirectory));
    RED_CHECK(expected_pdu_checker.is_expected_msg(CbMsgType::FormatList));
    RED_CHECK(expected_pdu_checker.transitions_as_string() == "FORMAT_LIST|TEMP_DIRECTORY|LOCK_CLIPDATA|UNLOCK_CLIPDATA"_av);
}

RED_AUTO_TEST_CASE(TestGeneralFlagsCapability)
{
    using VNC::GeneralFlagsCapability;

    auto to_str = [](GeneralFlagsCapability r)
    {
        std::vector<char> v;
        v.push_back(r.ok ? 'Y' : 'N');
        push(v, "|general_flags_or_expected_len=0x"_av);
        push(v, int_to_fixed_hexadecimal_upper_chars(r.general_flags_or_expected_len));
        push(v, "|"_av);
        push(v, std::string_view(r.ctx));
        return v;
    };

    RED_CHECK(
        to_str(VNC::GeneralFlagsCapability::parse(""_av))
        == "N|general_flags_or_expected_len=0x00000004|VNC::CliprdrCapabilities"_av
    );

    RED_CHECK(
        to_str(VNC::GeneralFlagsCapability::parse(
            // nb set
            "\1\0""\0\0"
            // bad length
            "\1\0\5\0"
            ""_av
        )) == "N|general_flags_or_expected_len=0x00000005|VNC::CliprdrCapabilitiesSet::lengthCapability"_av
    );

    RED_CHECK(
        to_str(VNC::GeneralFlagsCapability::parse(
            // nb set
            "\1\0""\0\0"
            // bad length with unknown set
            "\2\0\5\0"
            ""_av
        )) == "N|general_flags_or_expected_len=0x00000005|VNC::CliprdrCapabilitiesSet::lengthCapability"_av
    );

    RED_CHECK(
        to_str(VNC::GeneralFlagsCapability::parse(
            // nb set
            "\1\0""\0\0"
            // partial paquet (missing 5 bytes)
            "\1\0\x9\0"
            ""_av
        )) == "N|general_flags_or_expected_len=0x00000009|VNC::CliprdrCapabilitiesSet::lengthCapability"_av
    );

    RED_CHECK(
        to_str(VNC::GeneralFlagsCapability::parse(
            // nb set
            "\3\0""\0\0"
            // general capset | UseLongFormatNames
            "\1\0\xC\0\1\0\0\0\2\0\0\0"
            // unknown set
            "\2\0\x8\0\2\0\0\0"
            // general capset | FileClipNoFilePaths | HugeFileSupportEnabled
            "\1\0\xC\0\2\0\0\0\x28\0\0\0"
            ""_av
        )) == "Y|general_flags_or_expected_len=0x0000002A|"_av
    );
}

RED_AUTO_TEST_CASE(Test_format_list_extract)
{
    enum class Mode
    {
        LongFormat,
        AsciiShortFormat,
        ShortFormat,
    };

    std::vector<char> v;

    auto extract = [&v](Mode mode, bytes_view data){
        v.clear();

        InStream in_stream{data};
        VNC::format_list_extract(
            in_stream,
            mode == Mode::LongFormat
                ? VNC::CbCapabilityFlags::UseLongFormatNames
                : VNC::CbCapabilityFlags(),
            mode == Mode::AsciiShortFormat
                ? VNC::CbMsgFlags::AsciiNames
                : VNC::CbMsgFlags(),
            [&](VNC::CbFormatID format_id, VNC::GenericName name)
            {
                push(v, int_to_decimal_chars(underlying_cast(format_id)));
                push(v, name.is_ascii() ? ": ascii=1 name="_av : ": ascii=0 name="_av);
                push(v, name.raw_name().as_chars());
                v.push_back('|');
            }
        );

        return chars_view(v);
    };

    RED_CHECK(
        extract(Mode::LongFormat,
            // CF_TEXT
            "\x01\x00\x00\x00""\0\0"
            // CF_UNICODETEXT
            "\x0D\x00\x00\x00""\0\0"
            // custom name
            "\x12\x34\x00\x00""M.y.C.u.s.t.o.m.N.a.m.e.\0\0"
            ""_av
        )
        ==
        "1: ascii=0 name=|"
        "13: ascii=0 name=|"
        "13330: ascii=0 name=M.y.C.u.s.t.o.m.N.a.m.e.|"
        ""_av
    );

    RED_CHECK(
        extract(Mode::AsciiShortFormat,
            // CF_TEXT
            "\x01\x00\x00\x00""\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
            // CF_UNICODETEXT
            "\x0D\x00\x00\x00""\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
            // custom name
            "\x12\x34\x00\x00""MyCustomName\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
            ""_av
        )
        ==
        "1: ascii=1 name=|"
        "13: ascii=1 name=|"
        "13330: ascii=1 name=MyCustomName|"
        ""_av
    );

    RED_CHECK(
        extract(Mode::ShortFormat,
            // CF_TEXT
            "\x01\x00\x00\x00""\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
            // CF_UNICODETEXT
            "\x0D\x00\x00\x00""\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
            // custom name
            "\x12\x34\x00\x00""M.y.C.u.s.t.o.m.N.a.m.e.\0\0\0\0\0\0\0\0"
            ""_av
        )
        ==
        "1: ascii=0 name=|"
        "13: ascii=0 name=|"
        "13330: ascii=0 name=M.y.C.u.s.t.o.m.N.a.m.e.|"
        ""_av
    );

    RED_TEST_CONTEXT("names without null character")
    {
        // no name
        RED_CHECK(extract(Mode::LongFormat, "\x01\x00\x00\x00"_av) == ""_av);

        RED_CHECK(
            extract(Mode::LongFormat,
                // CF_TEXT
                "\x01\x00\x00\x00""\0\0"
                // CF_UNICODETEXT
                "\x0D\x00\x00\x00""\0\0"
                // custom name
                "\x12\x34\x00\x00""M.y.C.u.s.t.o.m.N.a.m.e."
                ""_av
            )
            ==
            "1: ascii=0 name=|"
            "13: ascii=0 name=|"
            "13330: ascii=0 name=M.y.C.u.s.t.o.m.N.a.m.|"
            ""_av
        );

        RED_CHECK(
            extract(Mode::AsciiShortFormat,
                // CF_TEXT
                "\x01\x00\x00\x00""\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
                // CF_UNICODETEXT
                "\x0D\x00\x00\x00""\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
                // custom name
                "\x12\x34\x00\x00""MyCustomNameMyCustomNameMyCustom"
                ""_av
            )
            ==
            "1: ascii=1 name=|"
            "13: ascii=1 name=|"
            "13330: ascii=1 name=MyCustomNameMyCustomNameMyCusto|"
            ""_av
        );

        RED_CHECK(
            extract(Mode::ShortFormat,
                // CF_TEXT
                "\x01\x00\x00\x00""\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
                // CF_UNICODETEXT
                "\x0D\x00\x00\x00""\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
                // custom name
                "\x12\x34\x00\x00""M.y.C.u.s.t.o.m.N.a.m.e.M.y.c.u."
                ""_av
            )
            ==
            "1: ascii=0 name=|"
            "13: ascii=0 name=|"
            "13330: ascii=0 name=M.y.C.u.s.t.o.m.N.a.m.e.M.y.c.|"
            ""_av
        );
    }
}

RED_AUTO_TEST_CASE(Test_FormatListSerializer)
{
    uint8_t buffer[100];
    OutStream out_stream{buffer};

    using namespace VNC;


    out_stream.out_skip_bytes(95);

    RED_CHECK(!FormatListSerializer::serialize(
        out_stream, CbFormatID{0x10300608}, UnicodeLongName{"blabla"_av}));
    RED_CHECK(out_stream.tailroom() == 5);

    RED_CHECK(!FormatListSerializer::serialize(
        out_stream, CbFormatID{0x10300608}, AsciiName{"blabla"_sized_av}));
    RED_CHECK(out_stream.tailroom() == 5);

    RED_CHECK(!FormatListSerializer::serialize(
        out_stream, CbFormatID{0x10300608}, UnicodeShortName{"blabla"_sized_av}));
    RED_CHECK(out_stream.tailroom() == 5);


    out_stream.rewind();
    RED_CHECK(FormatListSerializer::serialize(
        out_stream, CbFormatID{0x10300608}, UnicodeLongName{"blabla"_av}));
    RED_CHECK(out_stream.get_produced_bytes() == "\x08\x06\x30\x10""blabla\x00\x00"_av);

    out_stream.rewind();
    RED_CHECK(FormatListSerializer::serialize(
        out_stream, CbFormatID{0x10300608}, AsciiName{"blabla"_sized_av}));
    RED_CHECK(out_stream.get_produced_bytes()
        == "\x08\x06\x30\x10""blabla\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"_av);

    out_stream.rewind();
    RED_CHECK(FormatListSerializer::serialize(
        out_stream, CbFormatID{0x10300608}, UnicodeShortName{"blabla"_sized_av}));
    RED_CHECK(out_stream.get_produced_bytes()
        == "\x08\x06\x30\x10""blabla\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"_av);
}

RED_AUTO_TEST_CASE(TestCliprdrPkt)
{
    using namespace VNC;

    RED_CHECK(
        make_cb_packet_with_header(CbMsgFlags::ResponseOk, FormatListResponse{})
        == format_list_response_ok_with_header
    );
    RED_CHECK(
        make_cb_packet_with_header(CbMsgFlags::ResponseFail, FormatListResponse{})
        == format_list_response_fail_with_header
    );

    RED_CHECK(
        make_cb_packet_with_header(CbMsgFlags::ResponseFail, FormatDataResponseWithoutData{})
        == format_data_response_fail_with_header
    );
}

