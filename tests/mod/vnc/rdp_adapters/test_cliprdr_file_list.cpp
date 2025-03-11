/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "mod/vnc/rdp_adapters/cliprdr_file_list.hpp"
#include "mod/vnc/rdp_adapters/rdpeclip.hpp"

#include "test_only/test_framework/redemption_unit_tests.hpp"

namespace
{

struct CliprdrFileListAddResult
{
    VNC::CliprdrFileList::AddFileResult value;

    bool operator == (CliprdrFileListAddResult const &) const = default;
};


#if !REDEMPTION_UNIT_TEST_FAST_CHECK
std::ostream& boost_test_print_type(std::ostream& ostr, CliprdrFileListAddResult const& result) {
    ostr << "{ec: " << result.value.ec << ", decode_err_pos: " << result.value.decode_error_position << "}";
    return ostr;
}
#endif

}

RED_AUTO_TEST_CASE(TestCliprdrFileList)
{
    using namespace VNC;
    using namespace std::chrono;

    using Path = WinNtPathView;

    auto _ = ut::PatternViewSaver::printable_ascii();

    CliprdrFileList cb_file_list { 6 };

    constexpr auto flags_first = VNC::ChannelFlags::First;
    constexpr auto flags_last = VNC::ChannelFlags::Last;
    constexpr auto flags_first_last = flags_first | flags_last;

    RED_TEST_CONTEXT("push files")
    {
        FileDescriptor fd{
            .flags = FileDescriptorFlags::ShowProgressUI
                   | FileDescriptorFlags::FileSize
                   | FileDescriptorFlags::WriteTime
                   | FileDescriptorFlags::Attributes,
            .fileAttributes = FileAttributeFlags::Normal,
            .lastWriteTime = WinNtUTime{13389902460'0'000'000},
            .fileSizeHigh = 0,
            .fileSizeLow = 10,
            .unicodeFileName = "a\0b\0c\0"_sized_av,
        };

        using FLR = CliprdrFileListAddResult;
        using AddFileErrorCode = VNC::CliprdrFileList::AddFileErrorCode;
        auto no_err = FLR{{0, AddFileErrorCode::Ok}};
        auto full_err = FLR{{0, AddFileErrorCode::Full}};

        // not init
        RED_CHECK(FLR{cb_file_list.add_file(fd)} == full_err);

        RED_CHECK(!cb_file_list.start_new_list(WinNtUTime{13389802460}, 8));
        RED_CHECK(cb_file_list.start_new_list(WinNtUTime{13389802460}, 6));

        // file
        RED_CHECK(FLR{cb_file_list.add_file(fd)} == no_err);

        // err
        RED_CHECK(FLR{cb_file_list.add_file({
            .flags = FileDescriptorFlags::ShowProgressUI
                | FileDescriptorFlags::FileSize
                | FileDescriptorFlags::Attributes,
            .fileAttributes = FileAttributeFlags::ReadOnly,
            .lastWriteTime = WinNtUTime{1},
            .fileSizeHigh = 72,
            .fileSizeLow = 11,
            .unicodeFileName = "f\0i\0l\0e\0 \0\x80\0\x82\0"_sized_av, // invalid name
        })} == (FLR{{10, AddFileErrorCode::DecodeError}}));

        // file
        RED_CHECK(FLR{cb_file_list.add_file({
            .flags = FileDescriptorFlags::ShowProgressUI
                | FileDescriptorFlags::FileSize
                | FileDescriptorFlags::Attributes,
            .fileAttributes = FileAttributeFlags::ReadOnly,
            .lastWriteTime = WinNtUTime{1},
            .fileSizeHigh = 72,
            .fileSizeLow = 11,
            .unicodeFileName = "A\0B\0"_sized_av,
        })} == no_err);

        // dir
        RED_CHECK(FLR{cb_file_list.add_file({
            .flags = FileDescriptorFlags::Attributes,
            .fileAttributes = FileAttributeFlags::Directory,
            .lastWriteTime = WinNtUTime{1},
            .fileSizeHigh = 0,
            .fileSizeLow = 0,
            .unicodeFileName = "d\0i\0r\0"_sized_av,
        })} == no_err);

        // dir
        RED_CHECK(FLR{cb_file_list.add_file({
            .flags = FileDescriptorFlags::Attributes,
            .fileAttributes = FileAttributeFlags::Directory,
            .lastWriteTime = WinNtUTime{1},
            .fileSizeHigh = 0,
            .fileSizeLow = 0,
            .unicodeFileName = "d\0i\0r\0""2\0"_sized_av,
        })} == no_err);

        // file
        RED_CHECK(FLR{cb_file_list.add_file({
            .flags = FileDescriptorFlags::ShowProgressUI
                | FileDescriptorFlags::FileSize
                | FileDescriptorFlags::Attributes,
            .fileAttributes = FileAttributeFlags::ReadOnly,
            .lastWriteTime = WinNtUTime{1},
            .fileSizeHigh = 0,
            .fileSizeLow = 7,
            .unicodeFileName = "f\0i\0l\0e\0""2"_sized_av,
        })} == no_err);

        // dir
        RED_CHECK(FLR{cb_file_list.add_file({
            .flags = FileDescriptorFlags::Attributes,
            .fileAttributes = FileAttributeFlags::Directory,
            .lastWriteTime = WinNtUTime{1},
            .fileSizeHigh = 0,
            .fileSizeLow = 0,
            .unicodeFileName = "d\0i\0r\0""3\0"_sized_av,
        })} == no_err);

        RED_CHECK(FLR{cb_file_list.add_file(fd)} == full_err);
    }

    RED_TEST_CONTEXT("create dirs and file transfer offer")
    {
        RED_TEST_CONTEXT("1 file path too long (lindex=0)")
        {
            RED_REQUIRE(!cb_file_list.is_waiting_response());

            StaticOutStream<300> out_stream;

            RED_CHECK(
                cb_file_list.write_uvnc_items_to_vnc(
                    out_stream,
                    Path{
                        // 257 chars + "/abc" -> path too long
                        "C:\\Desktop\\aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"_sized_av
                    }
                )
                == CliprdrFileList::ErrorCode::PathTooLong
            );
            RED_CHECK(out_stream.get_produced_bytes() == ""_av);
        }

        RED_TEST_CONTEXT("1 file (lindex=0)")
        {
            RED_REQUIRE(!cb_file_list.is_waiting_response());

            RED_TEST_CONTEXT("send file offer")
            {
                StaticOutStream<300> out_stream;

                RED_CHECK(cb_file_list.write_uvnc_items_to_vnc(out_stream, Path{"C:\\Desktop"_sized_av})
                    == CliprdrFileList::ErrorCode::Ok);

                RED_CHECK(out_stream.get_produced_bytes() ==
                    "\x07\x08\x01\x00\x00\x00\x00\x0a\x00\x00\x00\x1f"
                    "C:\\Desktop\\abc,04/23/2025 17:21\x00\x00\x00\x00"_av);
            }

            RED_TEST_CONTEXT("receive file accept ok")
            {
                RED_CHECK(cb_file_list.receive_uvnc_create_dir_response()
                    == CliprdrFileList::ReceiveStatus::Error);

                RED_CHECK(cb_file_list.receive_uvnc_file_accept_response());
                RED_CHECK(!cb_file_list.receive_uvnc_file_accept_response());
            }

            RED_TEST_CONTEXT("write_cb_file_range_request")
            {
                StaticOutStream<300> out_stream;

                RED_CHECK(cb_file_list.write_cb_file_range_request(out_stream)
                    == CliprdrFileList::ErrorCode::Ok);
                RED_CHECK(out_stream.get_produced_bytes() ==
                    "\x08\x00\x00\x00\x1c\x00\x00\x00"
                    "\x01\x00\x00\x00""\x00\x00\x00\x00""\x02\x00\x00\x00"
                    "\x00\x00\x00\x00""\x00\x00\x00\x00""\x00\x00\x04\x00"
                    "\x00\x00\x00\x00"_av);
            }

            RED_TEST_CONTEXT("receive_cb_file_contents_response")
            {
                auto result = cb_file_list.receive_cb_file_contents_response(
                    "\1\0\0\0""abcdef"_av, 0, true, flags_first_last
                );
                RED_CHECK(result.ok);
                RED_CHECK(result.file_is_complete);
                RED_CHECK(result.data == "abcdef"_av);
            }
        }

        RED_TEST_CONTEXT("1 file (lindex=1)")
        {
            RED_REQUIRE(!cb_file_list.is_waiting_response());

            StaticOutStream<300> out_stream;

            RED_CHECK(cb_file_list.write_uvnc_items_to_vnc(out_stream, Path{"C:\\Desktop"_sized_av})
                == CliprdrFileList::ErrorCode::Ok);

            RED_CHECK(out_stream.get_produced_bytes() ==
                "\x07\x08\x01\x00\x00\x00\x00\x0b\x00\x00\x00\x1e"
                "C:\\Desktop\\AB,01/01/9793 00:22\x00\x00\x00\x48"_av);

            RED_TEST_CONTEXT("skip file")
            {
                RED_CHECK(cb_file_list.receive_uvnc_create_dir_response()
                    == CliprdrFileList::ReceiveStatus::Error);

                RED_CHECK(cb_file_list.receive_uvnc_file_accept_response());
                RED_CHECK(!cb_file_list.receive_uvnc_file_accept_response());

                cb_file_list.next_file();
            }
        }

        RED_TEST_CONTEXT("2 dirs + 1 file (lindex=2)")
        {
            RED_REQUIRE(!cb_file_list.is_waiting_response());

            StaticOutStream<300> out_stream;

            RED_CHECK(cb_file_list.write_uvnc_items_to_vnc(out_stream, Path{"C:\\Desktop"_sized_av})
                == CliprdrFileList::ErrorCode::Ok);

            RED_CHECK(out_stream.get_produced_bytes() ==
                "\x07\x0a\x01\x00\x00\x00\x00\x00\x00\x00\x00\x0e"
                "C:\\Desktop\\dir"
                "\x07\x0a\x01\x00\x00\x00\x00\x00\x00\x00\x00\x0f"
                "C:\\Desktop\\dir2"
                "\x07\x08\x01\x00\x00\x00\x00\x07\x00\x00\x00\x20"
                "C:\\Desktop\\file,01/01/9793 00:22\x00\x00\x00\x00"_av);

            RED_TEST_CONTEXT("receive 2 dir response and skip 1 file")
            {
                RED_CHECK(cb_file_list.receive_uvnc_create_dir_response()
                    == CliprdrFileList::ReceiveStatus::WaitingResponse);
                RED_CHECK(cb_file_list.receive_uvnc_create_dir_response()
                    == CliprdrFileList::ReceiveStatus::WaitingResponse);
                RED_CHECK(cb_file_list.receive_uvnc_create_dir_response()
                    == CliprdrFileList::ReceiveStatus::Error);

                RED_CHECK(cb_file_list.receive_uvnc_file_accept_response());
                RED_CHECK(!cb_file_list.receive_uvnc_file_accept_response());

                cb_file_list.next_file();
            }
        }

        RED_TEST_CONTEXT("1 dir (lindex=5)")
        {
            RED_REQUIRE(!cb_file_list.is_waiting_response());

            StaticOutStream<300> out_stream;

            RED_CHECK(cb_file_list.write_uvnc_items_to_vnc(out_stream, Path{"C:\\Desktop"_sized_av})
                == CliprdrFileList::ErrorCode::Ok);

            RED_CHECK(out_stream.get_produced_bytes() ==
                "\x07\x0a\x01\x00\x00\x00\x00\x00\x00\x00\x00\x0f"
                "C:\\Desktop\\dir3"_av);

            RED_TEST_CONTEXT("receive 1 dir")
            {
                RED_CHECK(cb_file_list.receive_uvnc_create_dir_response()
                    == CliprdrFileList::ReceiveStatus::TransferComplete);
                RED_CHECK(cb_file_list.receive_uvnc_create_dir_response()
                    == CliprdrFileList::ReceiveStatus::Error);

                RED_CHECK(!cb_file_list.receive_uvnc_file_accept_response());
            }
        }

        RED_TEST_CONTEXT("no item (lindex=6)")
        {
            RED_REQUIRE(!cb_file_list.is_waiting_response());

            StaticOutStream<300> out_stream;

            RED_CHECK(cb_file_list.is_transfer_complete());

            RED_CHECK(cb_file_list.write_uvnc_items_to_vnc(out_stream, Path{"C:\\Desktop"_sized_av})
                == CliprdrFileList::ErrorCode::Ok);

            RED_CHECK(out_stream.get_produced_bytes() == ""_av);
        }
    }
}
