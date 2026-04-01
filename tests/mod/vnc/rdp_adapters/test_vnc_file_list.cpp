/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "mod/vnc/rdp_adapters/vnc_file_list.hpp"
#include "utils/sugar/bytes_equal.hpp"
#include "utils/sugar/int_to_chars.hpp"
#include "utils/stream.hpp"
#include "utils/strutils.hpp"
#include "utils/literals/utf16.hpp"

#include "test_only/test_framework/redemption_unit_tests.hpp"


namespace
{

struct TransferResult : VNC::VncFileList::TransferResult
{
    friend bool operator == (TransferResult const& a, TransferResult const& b) noexcept
    {
        return a.response_types[0] == b.response_types[0]
            && a.response_types[1] == b.response_types[1]
            && bytes_equal(a.rdp_data, b.rdp_data)
            && bytes_equal(a.vnc_data, b.vnc_data);
    }
};

}


#if !REDEMPTION_UNIT_TEST_FAST_CHECK
# include "utils/sugar/int_to_chars.hpp"
# include "test_only/test_framework/compare_collection.hpp"

namespace
{

static ut::assertion_result test_comp_vnc_file_list_transfer(TransferResult a, TransferResult b)
{
    ut::assertion_result ar(true);

    if (REDEMPTION_UNLIKELY(!(a == b))) {
        ar = false;

        auto put = [&](std::ostream& out, TransferResult const& x){
            out << "{.rdp_data=";
            ut::put_view(x.rdp_data.size(), out, {x.rdp_data, ut::PatternView::printable_ascii, 4});
            out << "_av, .vnc_data=";
            ut::put_view(x.vnc_data.size(), out, {x.vnc_data, ut::PatternView::printable_ascii, 4});
            out << "_av, .resp[0]=" << redemption_unit_test_::Enum{x.response_types[0]};
            out << ", .resp[1]=" << redemption_unit_test_::Enum{x.response_types[1]};
            out << "}";
        };

        auto& out = ar.message().stream();
        out << "[";
        ut::put_data_with_diff(out, a, "!=", b, put);
        out << "]";
    }

    return ar;
}

}

RED_TEST_DISPATCH_COMPARISON_EQ((), (::TransferResult), (::TransferResult),
    ::test_comp_vnc_file_list_transfer)
#endif


RED_AUTO_TEST_CASE(TestVncFileList)
{
    using namespace VNC;
    using namespace std::chrono;

    using ResponseType = VncFileList::TransferResult::ResponseType;
    using FilePathView = UVncFile::PathView;

    auto _ = ut::PatternViewSaver::printable_ascii();

    VncFileList vnc_file_list { {
        .max_nb_files = 10,
        .max_file_size = 20,
    } };

    auto push_ok = VncFileList::PushFileResult::Ok;
    auto push_full = VncFileList::PushFileResult::TooManyFiles;
    auto push_file_size_too_large = VncFileList::PushFileResult::FileSizeTooLarge;
    auto push_final_path_too_large = VncFileList::PushFileResult::FinalPathTooLarge;

    UVncFile file {
        .file_name { WinNtPathView { "file1"_sized_av } },
        .file_size = 12,
        .last_access_time = WinNtUTime { 13389902460'0'000'000 },
        .is_dir = false,
    };

    TransferResult no_transfer {{
        .rdp_data = ""_av,
        .vnc_data = ""_av,
        .response_types {},
    }};

    auto long_path
      = "_"  // 50 chars by line
        "Path\\Path\\Path\\Path\\Path\\Path\\Path\\Path\\Path\\Path\\"
        "Path\\Path\\Path\\Path\\Path\\Path\\Path\\Path\\Path\\Path\\"
        "Path\\Path\\Path\\Path\\Path\\Path\\Path\\Path\\Path\\Path\\"
        "Path\\Path\\Path\\Path\\Path\\Path\\Path\\Path\\Path\\Path\\"
        "Path\\Path\\Path\\Path\\Path\\Path\\Path\\Path\\Path\\Path\\"
        "Path\\Pat\\"_sized_av;
    static_assert(long_path.size() == WINNT_MAX_PATH_SIZE_WITHOUT_NULL + 1);

    RED_TEST_CONTEXT("push big basedir")
    {
        RED_CHECK(TransferResult{vnc_file_list.start_new_list(FilePathView{long_path.drop_front<1>()})}
            == no_transfer);
        RED_CHECK(push_final_path_too_large == vnc_file_list.push_file_in_current_dir(file));

        RED_CHECK(TransferResult{vnc_file_list.start_new_list(FilePathView{long_path.drop_back<1>()})}
            == no_transfer);
        RED_CHECK(push_final_path_too_large == vnc_file_list.push_file_in_current_dir(file));

        // invalid case: empty file name
        RED_CHECK(push_ok == vnc_file_list.push_file_in_current_dir({
            .file_name { WinNtPathView { ""_sized_av } },
            .file_size = 12,
            .last_access_time = WinNtUTime { 13389902460'0'000'000 },
            .is_dir = false,
        }));
    }

    RED_TEST_CONTEXT("push big final path")
    {
        RED_CHECK(TransferResult{vnc_file_list.start_new_list(FilePathView{long_path.drop_front<3>()})}
            == no_transfer);
        RED_CHECK(push_ok == vnc_file_list.push_file_in_current_dir({
            .file_name { WinNtPathView { "fi"_sized_av } },
            .file_size = 12,
            .last_access_time = WinNtUTime { 13389902460'0'000'000 },
            .is_dir = false,
        }));
        RED_CHECK(push_final_path_too_large == vnc_file_list.push_file_in_current_dir({
            .file_name { WinNtPathView { "fil"_sized_av } },
            .file_size = 12,
            .last_access_time = WinNtUTime { 13389902460'0'000'000 },
            .is_dir = false,
        }));
    }

    RED_TEST_CONTEXT("push big file size")
    {
        RED_CHECK(push_file_size_too_large == vnc_file_list.push_file_in_current_dir({
            .file_name { WinNtPathView { "fi"_sized_av } },
            .file_size = 20,
            .last_access_time = WinNtUTime { 13389902460'0'000'000 },
            .is_dir = false,
        }));
    }

    RED_CHECK(TransferResult{vnc_file_list.start_new_list(FilePathView{"C:\\Path"_sized_av})}
        == no_transfer);

    // - dir1
    // - dir2
    // - dir3
    RED_TEST_CONTEXT("push files (top level)")
    {
        RED_CHECK(push_ok == vnc_file_list.push_file_in_current_dir({
            .file_name { WinNtPathView { "file1"_sized_av } },
            .file_size = 12,
            .last_access_time = WinNtUTime { 13389902460'0'000'000 },
            .is_dir = false,
        }));

        RED_CHECK(push_ok == vnc_file_list.push_file_in_current_dir({
            .file_name { WinNtPathView { "dir1"_sized_av } },
            .file_size = 0,
            .last_access_time = WinNtUTime { 13389903460'0'000'000 },
            .is_dir = true,
        }));

        RED_CHECK(push_ok == vnc_file_list.push_file_in_current_dir({
            .file_name { WinNtPathView { "dir2"_sized_av } },
            .file_size = 0,
            .last_access_time = WinNtUTime { 13389903060'0'000'000 },
            .is_dir = true,
        }));

        RED_CHECK(push_ok == vnc_file_list.push_file_in_current_dir({
            .file_name { WinNtPathView { "file2"_sized_av } },
            .file_size = 19,
            .last_access_time = WinNtUTime { 13389902360'0'000'000 },
            .is_dir = false,
        }));

        RED_CHECK(push_ok == vnc_file_list.push_file_in_current_dir({
            .file_name { WinNtPathView { "file3"_sized_av } },
            .file_size = 1,
            .last_access_time = WinNtUTime { 13389908460'0'000'000 },
            .is_dir = false,
        }));

        RED_CHECK(push_ok == vnc_file_list.push_file_in_current_dir({
            .file_name { WinNtPathView { "dir3"_sized_av } },
            .file_size = 0,
            .last_access_time = WinNtUTime { 13389903960'0'000'000 },
            .is_dir = true,
        }));
    }

    {
        StaticOutStream<5> out_stream_5_bytes;
        RED_CHECK(vnc_file_list.write_next_vnc_directory_content_request(out_stream_5_bytes)
            == VNC::VncFileList::NextDirectoryResult::TooSmallBuffer);
    }

    RED_TEST_CONTEXT("request VNC directories (C:\\Path\\dir1\\)")
    {
        StaticOutStream<100> out_stream;
        RED_CHECK(vnc_file_list.write_next_vnc_directory_content_request(out_stream)
            == VNC::VncFileList::NextDirectoryResult::Ok);
        RED_CHECK(out_stream.get_produced_bytes() ==
            "\x07""\x01""\x01\x00""\x00\x00\x00\x00""\x00\x00\x00\x0d""C:\\Path\\dir1\\"_av);
    }

    // * dir1
    // - dir2
    // - dir3
    // - dir1/subdir_d1
    RED_TEST_CONTEXT("push files (dir1)")
    {
        RED_CHECK(push_ok == vnc_file_list.push_file_in_current_dir({
            .file_name { WinNtPathView { "subdir_d1"_sized_av } },
            .file_size = 0,
            .last_access_time = WinNtUTime { 13389903960'0'000'000 },
            .is_dir = true,
        }));
        RED_CHECK(push_ok == vnc_file_list.push_file_in_current_dir({
            .file_name { WinNtPathView { "subfile"_sized_av } },
            .file_size = 0,
            .last_access_time = WinNtUTime { 13389903960'0'000'000 },
            .is_dir = false,
        }));
    }

    RED_TEST_CONTEXT("request VNC directories (C:\\Path\\dir2\\)")
    {
        StaticOutStream<100> out_stream;

        RED_CHECK(vnc_file_list.write_next_vnc_directory_content_request(out_stream)
            == VNC::VncFileList::NextDirectoryResult::Ok);
        RED_CHECK(out_stream.get_produced_bytes() ==
            "\x07""\x01""\x01\x00""\x00\x00\x00\x00""\x00\x00\x00\x0d""C:\\Path\\dir2\\"_av);
    }

    // - dir1
    // * dir2
    // - dir3
    // - dir1/subdir_d1
    // - dir2/subdir_d2
    RED_TEST_CONTEXT("push files (dir2)")
    {
        RED_CHECK(push_ok == vnc_file_list.push_file_in_current_dir({
            .file_name { WinNtPathView { "subdir_d2"_sized_av } },
            .file_size = 0,
            .last_access_time = WinNtUTime { 13389903960'0'000'000 },
            .is_dir = true,
        }));
    }

    RED_TEST_CONTEXT("request VNC directories (C:\\Path\\dir3\\)")
    {
        StaticOutStream<100> out_stream;

        RED_CHECK(vnc_file_list.write_next_vnc_directory_content_request(out_stream)
            == VNC::VncFileList::NextDirectoryResult::Ok);
        RED_CHECK(out_stream.get_produced_bytes() ==
            "\x07""\x01""\x01\x00""\x00\x00\x00\x00""\x00\x00\x00\x0d""C:\\Path\\dir3\\"_av);
    }

    RED_TEST_CONTEXT("request VNC directories (C:\\Path\\dir1\\subdir_d1\\)")
    {
        StaticOutStream<100> out_stream;

        RED_CHECK(vnc_file_list.write_next_vnc_directory_content_request(out_stream)
            == VNC::VncFileList::NextDirectoryResult::Ok);
        RED_CHECK(out_stream.get_produced_bytes() ==
            "\x07""\x01""\x01\x00""\x00\x00\x00\x00""\x00\x00\x00\x17""C:\\Path\\dir1\\subdir_d1\\"_av);
    }

    // - dir1
    // - dir2
    // - dir3
    // * dir1/subdir_d1
    // - dir2/subdir_d2
    // - dir1/subdir_d1/subdir_d1_d1
    // - dir1/subdir_d1/subdir_d1_d2
    RED_TEST_CONTEXT("push files (C:\\Path\\dir1\\subdir\\subdir2)")
    {
        RED_CHECK(push_ok == vnc_file_list.push_file_in_current_dir({
            .file_name { WinNtPathView { "subdir_d1_d1"_sized_av } },
            .file_size = 0,
            .last_access_time = WinNtUTime { 13389903960'0'000'000 },
            .is_dir = true,
        }));

        // full
        RED_CHECK(push_full == vnc_file_list.push_file_in_current_dir({
            .file_name { WinNtPathView { "subdir_d1_d2"_sized_av } },
            .file_size = 0,
            .last_access_time = WinNtUTime { 13389903960'0'000'000 },
            .is_dir = true,
        }));
    }

    RED_TEST_CONTEXT("request VNC directories (C:\\Path\\dir2\\subdir)")
    {
        StaticOutStream<100> out_stream;

        RED_CHECK(vnc_file_list.write_next_vnc_directory_content_request(out_stream)
            == VNC::VncFileList::NextDirectoryResult::Ok);
        RED_CHECK(out_stream.get_produced_bytes() ==
            "\x07""\x01""\x01\x00""\x00\x00\x00\x00""\x00\x00\x00\x17"
            "C:\\Path\\dir2\\subdir_d2\\"_av);

        out_stream.rewind();

        RED_CHECK(vnc_file_list.write_next_vnc_directory_content_request(out_stream)
            == VNC::VncFileList::NextDirectoryResult::Ok);
        RED_CHECK(out_stream.get_produced_bytes() ==
            "\x07""\x01""\x01\x00""\x00\x00\x00\x00""\x00\x00\x00\x24"
            "C:\\Path\\dir1\\subdir_d1\\subdir_d1_d1\\"_av);

        out_stream.rewind();

        RED_CHECK(vnc_file_list.write_next_vnc_directory_content_request(out_stream)
            == VNC::VncFileList::NextDirectoryResult::NoDir);
        RED_CHECK(out_stream.get_produced_bytes() == ""_av);
    }

    RED_TEST_CONTEXT("write rdp file data response")
    {
        char buffer[64 * 1024];

        using RdpFileListStatus = VNC::VncFileList::PartialRdpFileListResult::Status;

        RED_TEST_CONTEXT("too short buffer")
        {
            OutStream out_stream { make_writable_array_view(buffer).first(5) };
            auto rdp_file_list_result = vnc_file_list.write_partial_rdp_file_list(out_stream);

            RED_CHECK(rdp_file_list_result.status == RdpFileListStatus::TooSmallBuffer);
        }

        static const char zeros[590] {};
        auto pad = [](uint32_t n){ return make_array_view(zeros).first(n); };
        auto pad16 = pad(16);
        auto pad32 = pad(32);

        // path without trailing '\' nor dir base
        auto pdu = str_concat(""
            // clipheader
            "\x05\x00\x01\x00\x24\x17\x00\x00"_av
            // file list header
            "\x0a\x00\x00\x00"_av

            // C:\Path\file1
            "\x64\x40\x00\x00"_av,
            pad32,
            "\x20\x00\x00\x00"_av,
            pad16,
            "\x00\xe6\x0d\x15\x74\xb4\xdb\x01"
            "\x00\x00\x00\x00"
            "\x0c\x00\x00\x00"_av,
            "file1"_utf16_le,
            pad(510),

            // C:\Path\dir1
            "\x64\x40\x00\x00"_av,
            pad32,
            "\x10\x00\x00\x00"_av,
            pad16,
            "\x00\xca\x19\x69\x76\xb4\xdb\x01"
            "\x00\x00\x00\x00"
            "\x00\x00\x00\x00"_av,
            "dir1"_utf16_le,
            pad(512),

            // C:\Path\dir2
            "\x64\x40\x00\x00"_av,
            pad32,
            "\x10\x00\x00\x00"_av,
            pad16,
            "\x00\xa2\xae\x7a\x75\xb4\xdb\x01"
            "\x00\x00\x00\x00"
            "\x00\x00\x00\x00"_av,
            "dir2"_utf16_le,
            pad(512),

            // C:\Path\file2
            "\x64\x40\x00\x00"_av,
            pad32,
            "\x20\x00\x00\x00"_av,
            pad16,
            "\x00\x1c\x73\xd9\x73\xb4\xdb\x01"
            "\x00\x00\x00\x00"
            "\x13\x00\x00\x00"_av,
            "file2"_utf16_le,
            pad(510),

            // C:\Path\file3
            "\x64\x40\x00\x00"_av,
            pad32,
            "\x20\x00\x00\x00"_av,
            pad16,
            "\x00\x3e\x55\x0d\x82\xb4\xdb\x01"
            "\x00\x00\x00\x00"
            "\x01\x00\x00\x00"_av,
            "file3"_utf16_le,
            pad(510),

            // C:\Path\dir3
            "\x64\x40\x00\x00"_av,
            pad32,
            "\x10\x00\x00\x00"_av,
            pad16,
            "\x00\xbc\x1f\x93\x77\xb4\xdb\x01"
            "\x00\x00\x00\x00"
            "\x00\x00\x00\x00"_av,
            "dir3"_utf16_le,
            pad(512),

            // C:\Path\dir1\subdir_d1
            "\x64\x40\x00\x00"_av,
            pad32,
            "\x10\x00\x00\x00"_av,
            pad16,
            "\x00\xbc\x1f\x93\x77\xb4\xdb\x01"
            "\x00\x00\x00\x00"
            "\x00\x00\x00\x00"_av,
            "dir1\\subdir_d1"_utf16_le,
            pad(492),

            // C:\Path\dir1\subfile
            "\x64\x40\x00\x00"_av,
            pad32,
            "\x20\x00\x00\x00"_av,
            pad16,
            "\x00\xbc\x1f\x93\x77\xb4\xdb\x01"
            "\x00\x00\x00\x00"
            "\x00\x00\x00\x00"_av,
            "dir1\\subfile"_utf16_le,
            pad(496),

            // C:\Path\dir2\subdir_d2
            "\x64\x40\x00\x00"_av,
            pad32,
            "\x10\x00\x00\x00"_av,
            pad16,
            "\x00\xbc\x1f\x93\x77\xb4\xdb\x01"
            "\x00\x00\x00\x00"
            "\x00\x00\x00\x00"_av,
            "dir2\\subdir_d2"_utf16_le,
            pad(492),

            // C:\Path\dir1\subdir_d1\subdir_d1_d1
            "\x64\x40\x00\x00"_av,
            pad32,
            "\x10\x00\x00\x00"_av,
            pad16,
            "\x00\xbc\x1f\x93\x77\xb4\xdb\x01"
            "\x00\x00\x00\x00"
            "\x00\x00\x00\x00"_av,
            "dir1\\subdir_d1\\subdir_d1_d1"_utf16_le,
            pad(466)
        );

        RED_TEST_CONTEXT("big packet")
        {
            OutStream out_stream { make_writable_array_view(buffer) };
            auto rdp_file_list_result = vnc_file_list.write_partial_rdp_file_list(out_stream);

            RED_CHECK(rdp_file_list_result.status == RdpFileListStatus::Completed);
            RED_CHECK(rdp_file_list_result.channel_flags()
                == (VNC::ChannelFlags::First
                  | VNC::ChannelFlags::Last
                  | VNC::ChannelFlags::ShowProtocol));
            RED_CHECK(rdp_file_list_result.total_len == 5932);
            RED_CHECK(out_stream.get_produced_bytes().size() == rdp_file_list_result.total_len);
            RED_CHECK(out_stream.get_produced_bytes() == pdu);

            InStream in_stream { out_stream.get_produced_bytes() };
        }

        RED_CHECK(TransferResult{vnc_file_list.start_rdp_file_list()}
            == no_transfer);

        RED_TEST_CONTEXT("2 packets")
        {
            OutStream out_stream { make_writable_array_view(buffer).first(3000) };

            auto rdp_file_list_result = vnc_file_list.write_partial_rdp_file_list(out_stream);

            RED_CHECK(rdp_file_list_result.status == RdpFileListStatus::Partial);
            RED_CHECK(rdp_file_list_result.channel_flags()
                == (VNC::ChannelFlags::First
                  | VNC::ChannelFlags::ShowProtocol));
            RED_CHECK(rdp_file_list_result.total_len == 5932);
            RED_CHECK(out_stream.get_produced_bytes() == chars_view(pdu).first(12 + 5*592));

            out_stream.rewind();

            rdp_file_list_result = vnc_file_list.write_partial_rdp_file_list(out_stream);

            RED_CHECK(rdp_file_list_result.status == RdpFileListStatus::Completed);
            RED_CHECK(rdp_file_list_result.channel_flags()
                == (VNC::ChannelFlags::Last
                  | VNC::ChannelFlags::ShowProtocol));
            RED_CHECK(rdp_file_list_result.total_len == 0);
            RED_CHECK(out_stream.get_produced_bytes() == chars_view(pdu).last(5*592));
        }
    }

    auto file_contents_response_failure
        = "\x09\x00""\x02\x00""\x04\x00\x00\x00""\x01\x00\x00\x00"_av;

    auto rdp_transfer_result_unsequenced = TransferResult{{
        .rdp_data = file_contents_response_failure,
        .vnc_data = {},
        .response_types {
            ResponseType::RdpResponseUnsequenced,
        }
    }};

    auto rdp_transfer_result_failure = TransferResult{{
        .rdp_data = file_contents_response_failure,
        .vnc_data = {},
        .response_types {
            ResponseType::RdpResponseFailure,
        }
    }};

    auto rdp_transfer_result_invalid_lindex = TransferResult{{
        .rdp_data = file_contents_response_failure,
        .vnc_data = {},
        .response_types {
            ResponseType::InvalidLindex,
        }
    }};

    RED_TEST_CONTEXT("download file[0]")
    {
        RED_CHECK(
            TransferResult{vnc_file_list.rdp_requested_file(
                make_file_contents_size_request(CbStreamId{1}, CbLindex{0}, ClipDataId{})
            )}
            ==
            (TransferResult{{
                .rdp_data =
                    // FileContentsResponse size=12
                    "\x09\x00""\x01\x00""\x0c\x00\x00\x00""\x01\x00\x00\x00"
                    "\x0c\x00\x00\x00""\x00\x00\x00\x00"_av,
                .vnc_data = {},
                .response_types {
                    ResponseType::RdpResponseSize,
                }
            }})
        );

        RED_TEST_INFO("unsequenced request, start position must be 0");
        RED_CHECK(
            TransferResult{vnc_file_list.rdp_requested_file(FileContentsRequest{
                .streamId = CbStreamId{1},
                .lindex = CbLindex{0},
                .dwFlags = CbFileContentsType::Range,
                .nPositionLow = 10,
                .nPositionHigh = 0,
                .cbRequested = 10,
                .clipDataId = ClipDataId{},
            })}
            == rdp_transfer_result_unsequenced
        );

        RED_TEST_INFO("request file at position 0");
        RED_CHECK(
            TransferResult{vnc_file_list.rdp_requested_file(FileContentsRequest{
                .streamId = CbStreamId{1},
                .lindex = CbLindex{0},
                .dwFlags = CbFileContentsType::Range,
                .nPositionLow = 0,
                .nPositionHigh = 0,
                .cbRequested = 10,
                .clipDataId = ClipDataId{},
            })}
            ==
            (TransferResult{{
                .rdp_data = {},
                // file request
                .vnc_data =
                    "\x07""\x03""\x00\x00""\x00\x00\x00\x00""\x00\x00\x00\x0d"
                    "C:\\Path\\file1"_av,
                .response_types {
                    ResponseType::VncRequestFile,
                }
            }})
        );

        RED_TEST_INFO("request file response -> rejected by VNC server");
        RED_CHECK(
            TransferResult{vnc_file_list.receive_vnc_file_request_response(false)}
            == rdp_transfer_result_failure
        );

        RED_TEST_INFO("request file at position 0 (again)");
        RED_CHECK(
            TransferResult{vnc_file_list.rdp_requested_file(FileContentsRequest{
                .streamId = CbStreamId{1},
                .lindex = CbLindex{0},
                .dwFlags = CbFileContentsType::Range,
                .nPositionLow = 0,
                .nPositionHigh = 0,
                .cbRequested = 10,
                .clipDataId = ClipDataId{},
            })}
            ==
            (TransferResult{{
                .rdp_data = {},
                // file request
                .vnc_data =
                    "\x07""\x03""\x00\x00""\x00\x00\x00\x00""\x00\x00\x00\x0d"
                    "C:\\Path\\file1"_av,
                .response_types {
                    ResponseType::VncRequestFile,
                }
            }})
        );

        RED_TEST_INFO("request file response -> accepted by VNC server");
        RED_CHECK(
            TransferResult{vnc_file_list.receive_vnc_file_request_response(true)}
            ==
            (TransferResult{{
                .rdp_data = {},
                // file confirm
                .vnc_data = "\x07""\x04""\x00\x00""\x00\x00\x00\x00""\x00\x00\x00\x00"_av,
                .response_types {
                    ResponseType::VncConfirmFile,
                }
            }})
        );

        RED_TEST_INFO("receive data, (7 bytes on 10 requested)");
        RED_CHECK(
            TransferResult{vnc_file_list.receive_vnc_file_data("1234567"_av)}
            == TransferResult{}
        );

        RED_TEST_INFO("receive data, (6 bytes (+7) on 10 requested for 12 bytes file size) -> truncated");
        RED_CHECK(
            TransferResult{vnc_file_list.receive_vnc_file_data("890123"_av)}
            ==
            (TransferResult{{
                // send data to rdp
                .rdp_data = "\x09\x00""\x01\x00""\x0e\x00\x00\x00""\x01\x00\x00\x00""1234567890"_av,
                // file abort
                .vnc_data = "\x07""\x07""\x03\x00""\x00\x00\x00""\x00\x00\x00\x00\x00"_av,
                .response_types {
                    ResponseType::RdpResponseData,
                    ResponseType::VncAbortFile,
                }
            }})
        );

        RED_TEST_INFO("request file at position 10 -> send buffered (truncated) data");
        RED_CHECK(
            TransferResult{vnc_file_list.rdp_requested_file(FileContentsRequest{
                .streamId = CbStreamId{1},
                .lindex = CbLindex{0},
                .dwFlags = CbFileContentsType::Range,
                .nPositionLow = 10,
                .nPositionHigh = 0,
                .cbRequested = 10,
                .clipDataId = ClipDataId{},
            })}
            ==
            (TransferResult{{
                // send data to rdp
                .rdp_data = "\x09\x00""\x01\x00""\x06\x00\x00\x00""\x01\x00\x00\x00""12"_av,
                .vnc_data = {},
                .response_types {
                    ResponseType::RdpResponseData,
                }
            }})
        );

        RED_TEST_INFO("receive end of file -> ignored");
        RED_CHECK(
            TransferResult{vnc_file_list.receive_vnc_end_of_file()}
            == TransferResult{}
        );
    }

    RED_TEST_CONTEXT("download file[999] (invalid)")
    {
        RED_TEST_INFO("request file size");
        RED_CHECK(
            TransferResult{vnc_file_list.rdp_requested_file(FileContentsRequest{
                .streamId = CbStreamId{1},
                .lindex = CbLindex{999},
                .dwFlags = CbFileContentsType::Range,
                .nPositionLow = 0,
                .nPositionHigh = 0,
                .cbRequested = 10,
                .clipDataId = ClipDataId{},
            })}
            == rdp_transfer_result_invalid_lindex
        );
    }

    RED_TEST_CONTEXT("download file[1] (dir -> invalid)")
    {
        RED_TEST_INFO("request file size");
        RED_CHECK(
            TransferResult{vnc_file_list.rdp_requested_file(FileContentsRequest{
                .streamId = CbStreamId{1},
                .lindex = CbLindex{999},
                .dwFlags = CbFileContentsType::Range,
                .nPositionLow = 0,
                .nPositionHigh = 0,
                .cbRequested = 10,
                .clipDataId = ClipDataId{},
            })}
            == rdp_transfer_result_invalid_lindex
        );
    }

    RED_TEST_CONTEXT("download file[3]")
    {
        RED_TEST_INFO("request file size");
        RED_CHECK(
            TransferResult{vnc_file_list.rdp_requested_file(FileContentsRequest{
                .streamId = CbStreamId{1},
                .lindex = CbLindex{3},
                .dwFlags = CbFileContentsType::Size,
                .nPositionLow = 0,
                .nPositionHigh = 0,
                .cbRequested = 0,
                .clipDataId = ClipDataId{},
            })}
            ==
            (TransferResult{{
                .rdp_data =
                    "\x09\x00""\x01\x00""\x0c\x00\x00\x00""\x01\x00\x00\x00"
                    "\x13\x00\x00\x00\x00\x00\x00\x00"_av,
                .vnc_data = {},
                .response_types {
                    ResponseType::RdpResponseSize,
                }
            }})
        );

        RED_TEST_INFO("request file at position 0");
        RED_CHECK(
            TransferResult{vnc_file_list.rdp_requested_file(FileContentsRequest{
                .streamId = CbStreamId{1},
                .lindex = CbLindex{3},
                .dwFlags = CbFileContentsType::Range,
                .nPositionLow = 0,
                .nPositionHigh = 0,
                .cbRequested = 19,
                .clipDataId = ClipDataId{},
            })}
            ==
            (TransferResult{{
                .rdp_data = {},
                // file request
                .vnc_data =
                    "\x07""\x03""\x00\x00""\x00\x00\x00\x00""\x00\x00\x00\x0d"
                    "C:\\Path\\file2"_av,
                .response_types {
                    ResponseType::VncRequestFile,
                }
            }})
        );

        RED_TEST_INFO("request file response -> accepted by VNC server");
        RED_CHECK(
            TransferResult{vnc_file_list.receive_vnc_file_request_response(true)}
            ==
            (TransferResult{{
                .rdp_data = {},
                // file confirm
                .vnc_data = "\x07""\x04""\x00\x00""\x00\x00\x00\x00""\x00\x00\x00\x00"_av,
                .response_types {
                    ResponseType::VncConfirmFile,
                }
            }})
        );

        RED_TEST_INFO("receive data: 19 bytes");
        RED_CHECK(
            TransferResult{vnc_file_list.receive_vnc_file_data("abcdefghijklmnopqrs"_av)}
            ==
            (TransferResult{{
                .rdp_data =
                    "\x09\x00""\x01\x00""\x17\x00\x00\x00""\x01\x00\x00\x00"
                    "abcdefghijklmnopqrs"_av,
                .vnc_data = {},
                .response_types {
                    ResponseType::RdpResponseData,
                }
            }})
        );

        RED_TEST_INFO("receive end of file");
        RED_CHECK(
            TransferResult{vnc_file_list.receive_vnc_end_of_file()}
            == TransferResult{}
        );

        RED_TEST_INFO("request file at position 19");
        RED_CHECK(
            TransferResult{vnc_file_list.rdp_requested_file(FileContentsRequest{
                .streamId = CbStreamId{1},
                .lindex = CbLindex{3},
                .dwFlags = CbFileContentsType::Range,
                .nPositionLow = 19,
                .nPositionHigh = 0,
                .cbRequested = 20,
                .clipDataId = ClipDataId{},
            })}
            ==
            (TransferResult{{
                .rdp_data = "\x09\x00""\x01\x00""\x04\x00\x00\x00""\x01\x00\x00\x00"""_av,
                .vnc_data = {},
                .response_types {
                    ResponseType::RdpResponseData,
                }
            }})
        );

        RED_TEST_INFO("request file at position 20 (unsequenced)");
        RED_CHECK(
            TransferResult{vnc_file_list.rdp_requested_file(FileContentsRequest{
                .streamId = CbStreamId{1},
                .lindex = CbLindex{3},
                .dwFlags = CbFileContentsType::Range,
                .nPositionLow = 21,
                .nPositionHigh = 0,
                .cbRequested = 20,
                .clipDataId = ClipDataId{},
            })}
            == rdp_transfer_result_unsequenced
        );
    }
}
