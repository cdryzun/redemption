/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "test_only/test_framework/redemption_unit_tests.hpp"

#include "core/buf64k.hpp"
#include "utils/pp.hpp"
#include "mod/vnc/encoders/uvnc_file_transfer.hpp"
#include "utils/function_ref.hpp"

#include <array>


static int64_t time_to_i(WinNtUTime tp)
{
    return static_cast<int64_t>(tp);
}

RED_AUTO_TEST_CASE(TestVnCFileTransfer)
{
    using namespace std::chrono;
    namespace FT = UVNC::FileTransfer;

    // init
    //@{
    struct Ctx
    {
        int nb_receive = 0;
    };
    Ctx ctx;

    #define SHOULD_BE_NOT_CALLED(fn) .fn \
        = [](void*, auto...){ RED_CHECK(!"unexpected callback: " #fn[0]); }

    UVNCFileTransferReader::ReceivePacketCallbacks const callbacks_base
    {
        .ctx = &ctx,
        .error = [](void*, UVNCFileTransferReader::ProtocolError err) {
            RED_FAIL("type: " << err.type << " max_or_min: " << err.max_or_min_len);
        },
        .parsing_header = [](void*) {},
        SHOULD_BE_NOT_CALLED(drive_list),
        SHOULD_BE_NOT_CALLED(start_list_dir),
        SHOULD_BE_NOT_CALLED(file_info),
        SHOULD_BE_NOT_CALLED(end_list_dir),
        SHOULD_BE_NOT_CALLED(file_header),
        SHOULD_BE_NOT_CALLED(file_partial_packet),
        SHOULD_BE_NOT_CALLED(end_of_file),
        SHOULD_BE_NOT_CALLED(aborted_file),
        SHOULD_BE_NOT_CALLED(file_partial_checksums),
        SHOULD_BE_NOT_CALLED(file_accept_header),
        SHOULD_BE_NOT_CALLED(command_return),
        SHOULD_BE_NOT_CALLED(file_transfer_access),
        SHOULD_BE_NOT_CALLED(protocol_version),
    };
    #undef SHOULD_BE_NOT_CALLED

    constexpr auto block_size = FT::min_block_size;
    constexpr auto packet_size = FT::min_full_packet_size;

    std::array<uint8_t, 256> random_data {};
    for (std::size_t i = 1; i < random_data.size(); ++i)
    {
        random_data[i] = random_data[i-1] + 1;
    }
    {
        auto vers_pkt = "\x11""\x03\x00""\x00\x00\x00\x00""\x00\x00\x00\x00"_av;
        memcpy(random_data.data(), vers_pkt.data(), vers_pkt.size());
    }

    Buf64k buf;
    auto write_in_buf = [&](bytes_view data) {
        buf.read_with([&](uint8_t* p, std::size_t len) {
            RED_REQUIRE(len >= data.size());
            memcpy(p, data.data(), data.size());
            return data.size();
        });
    };

    using ReadPacketStatus = UVNCFileTransferReader::ReadPacketStatus;
    using ErrorCode = UVNC::FileTransfer::WriteErrorCode;

    UVNCFileTransferReader ft;

    enum class SkipPartialPacket { No, Yes };

    auto test_receive = [&](
        bytes_view data,
        UVNCFileTransferReader::ReceivePacketCallbacks callbacks,
        ReadPacketStatus status,
        SkipPartialPacket skip_partial_pkt = SkipPartialPacket::No
    ){
        RED_REQUIRE(!data.empty());

        auto nb_error = RED_ERROR_COUNT();

        auto ft_saved = ft;

        RED_TEST_CONTEXT("full packet")
        {
            write_in_buf(data);
            ctx.nb_receive = 0;
            RED_CHECK(ft.read_packet(buf, callbacks) == status);
            RED_CHECK(buf.remaining() == 0);
            RED_CHECK(ctx.nb_receive == 1);
            if (nb_error != RED_ERROR_COUNT())
            {
                return;
            }
            if (status == ReadPacketStatus::Error)
            {
                return ;
            }
        }

        ft = ft_saved;

        RED_TEST_CONTEXT("full packet + 256 bytes")
        {
            write_in_buf(data);
            write_in_buf(random_data);
            ctx.nb_receive = 0;
            RED_CHECK(ft.read_packet(buf, callbacks) == ReadPacketStatus::Completed);
            RED_CHECK(buf.remaining() == random_data.size());
            RED_CHECK(ctx.nb_receive == 1);
            if (nb_error != RED_ERROR_COUNT())
            {
                return;
            }
        }

        buf.advance(buf.remaining());

        if (skip_partial_pkt == SkipPartialPacket::Yes)
        {
            return;
        }

        ft = ft_saved;

        ctx.nb_receive = 0;

        // empty packet
        RED_TEST_CONTEXT("byte one by one: 0 / " << data.size())
        {
            RED_CHECK(ft.read_packet(buf, callbacks) == ReadPacketStatus::WaitData);
            RED_CHECK(ctx.nb_receive == 0);
            if (nb_error != RED_ERROR_COUNT())
            {
                return;
            }
        }

        // insert byte one by one (without last byte)
        for (auto& c : data.drop_back(1))
        {
            write_in_buf({&c, 1});
            auto current_len = &c - data.data() + 1;
            RED_TEST_CONTEXT("byte one by one: " << current_len << " / " << data.size())
            {
                ctx.nb_receive = 0;
                RED_CHECK(ft.read_packet(buf, callbacks) == ReadPacketStatus::WaitData);
                RED_CHECK(ctx.nb_receive == 0);
                // 11 is header size
                auto remaining_bytes = (current_len < 11 ? current_len : current_len - 11);
                RED_CHECK(buf.remaining() == remaining_bytes);
                if (nb_error != RED_ERROR_COUNT())
                {
                    return;
                }
            }
        }

        // insert last byte
        write_in_buf(data.last(1));
        RED_TEST_CONTEXT("byte one by one: " << data.size() << " / " << data.size())
        {
            ctx.nb_receive = 0;
            RED_CHECK(ft.read_packet(buf, callbacks) == ReadPacketStatus::Completed);
            RED_CHECK(buf.remaining() == 0);
            RED_CHECK(ctx.nb_receive == 1);
        }
    };

    auto test_receive_exactly = [&](
        bytes_view data,
        UVNCFileTransferReader::ReceivePacketCallbacks callbacks,
        ReadPacketStatus status
    ){
        write_in_buf(data);
        ctx.nb_receive = 0;
        RED_CHECK(ft.read_packet(buf, callbacks) == status);
        RED_CHECK(buf.remaining() == 0);
        RED_CHECK(ctx.nb_receive == 1);
    };

    // callback wrapper
    auto fn = [](auto cb){
        return [](void* ctx, auto... xs){
            static_cast<Ctx*>(ctx)->nb_receive++;
            decltype(cb){}(nullptr, xs...);
        };
    };

    #define LINE_MARKER "  (line " RED_PP_STRINGIFY(__LINE__) ")"

    #define TEST_RECEIVE_EXACTLY(name, data, cb, status) do { \
        RED_TEST_CONTEXT("receive: " name LINE_MARKER)        \
        {                                                     \
            auto callbacks = callbacks_base;                  \
            callbacks.cb;                                     \
            test_receive_exactly(data, callbacks, status);    \
        }                                                     \
    } while (0)

    #define TEST_RECEIVE(name, data, cb, /*SkipPartialPacket::Yes*/...) do { \
        RED_TEST_CONTEXT("receive: " name LINE_MARKER)                       \
        {                                                                    \
            auto callbacks = callbacks_base;                                 \
            callbacks.cb;                                                    \
            test_receive(data, callbacks, ReadPacketStatus::Completed        \
                __VA_OPT__(,) __VA_ARGS__);                                  \
        }                                                                    \
    } while (0)

    #define TEST_RECEIVE_ERROR(name, data, cb, /*SkipPartialPacket::Yes*/...) do { \
        RED_TEST_CONTEXT("receive: " name LINE_MARKER)                             \
        {                                                                          \
            auto callbacks = callbacks_base;                                       \
            callbacks.cb;                                                          \
            test_receive(data, callbacks, ReadPacketStatus::Error                  \
                __VA_OPT__(,) __VA_ARGS__);                                        \
        }                                                                          \
    } while (0)

    auto test_write = [&](
        FunctionRef<UVNC::FileTransfer::WriteErrorCode(OutStream &)> fn,
        bytes_view expected
    ){
        auto nb_error = RED_ERROR_COUNT();

        auto ft_saved = ft;

        StaticOutStream<255> out;
        RED_CHECK(fn(out) == ErrorCode::NoError);
        RED_CHECK(out.get_produced_bytes() == expected);

        if (nb_error != RED_ERROR_COUNT())
        {
            return;
        }

        auto ft_final = ft;
        ft = ft_saved;

        auto n = out.get_produced_bytes().size() - 1;
        for (size_t i = 0; i < n; ++i)
        {
            RED_TEST_CONTEXT("byte one by one: " << i << " / " << n)
            {
                OutStream truncated_out {{out.get_data(), i}};
                RED_CHECK(fn(truncated_out) == ErrorCode::TooSmallBuffer);
                if (nb_error != RED_ERROR_COUNT())
                {
                    break;
                }
            }
        }

        ft = ft_final;
    };

    #define TEST_WRITE(write_expr, expected) do {                         \
        RED_TEST_CONTEXT(#write_expr LINE_MARKER)                         \
        {                                                                 \
            auto fn = [&](OutStream & out){ return write_expr; }; \
            test_write(fn, expected);                                     \
        }                                                                 \
    } while (0)
    //@}

    /*
     * Tests
              Client                      Server
                |                           |
     */

    /*          |                           |
                | <------- FileTransferProtocolVersion(17) (first packet)
    */
    TEST_RECEIVE(
        "FileTransferProtocolVersion",
        "\x11""\x03\x00""\x00\x00\x00\x00""\x00\x00\x00\x00"_av,
        protocol_version = fn([](void*, uint32_t version, bool supported)
        {
            RED_CHECK(version == 3);
            RED_CHECK(supported);
        })
    );

    /*          |                           |
       AbortFileTransfer(7) --------------> |
    */
    TEST_WRITE(
        FT::write_abort_file_transfer(out),
        "\x07""\x07""\x03\x00""\x00\x00\x00\x00""\x00\x00\x00\x00"_av
    );

    auto ft_saved = ft;
    auto ft_saved2 = ft;

    /*          |                           |
                | <-------------- FileTransferAccess(14)
                                  (with error)
    */
    TEST_RECEIVE(
        "FileTransferAccess failure",
        "\x0e""\x00\x00""\xff\xff\xff\xff""\x00\x00\x00\x00"_av,
        file_transfer_access = fn([](void*, bool is_ok) { RED_CHECK(!is_ok); })
    );

    /*          |                           |
                | <-------------- FileTransferAccess(14)
                                  (with success)
    */
    ft = ft_saved;
    // receive ok
    TEST_RECEIVE(
        "FileTransferAccess ok",
        "\x0e""\x00\x00""\x00\x00\x00\x01""\x00\x00\x00\x00"_av,
        file_transfer_access = fn([](void*, bool is_ok) { RED_CHECK(is_ok); })
    );


    /*          |                           |
        FileTransferSessionStart(15) -----> |
    */
    TEST_WRITE(
        FT::write_session_start(out),
        "\x07""\x0F""\x00\x00""\x00\x00\x00\x00""\x00\x00\x00\x00"_av
    );


    /*          |   ,-------------------,   |
                |   | Command execution |   |
                |   '-------------------'   |
    */

    /*          |                           |
       CommandRequest(10) ----------------> |
          (create dir)                      |
    */
    TEST_WRITE(
        FT::write_command_create_directory(out, FT::Path{"my_dir"_sized_av}),
        "\x07""\x0a""\x01\x00""\x00\x00\x00\x00""\x00\x00\x00\x06""my_dir"_av
    );
    TEST_WRITE(
        FT::write_command_create_directory2(out, "my_dir"_sized_av, "my_dir2"_sized_av),
        "\x07""\x0a""\x01\x00""\x00\x00\x00\x00""\x00\x00\x00\x0e""my_dir\\my_dir2"_av
    );
    TEST_WRITE(
        FT::write_command_create_directory2(out, {}, "my_dir"_sized_av),
        "\x07""\x0a""\x01\x00""\x00\x00\x00\x00""\x00\x00\x00\x06""my_dir"_av
    );
    TEST_WRITE(
        FT::write_command_create_directory2(out, "my_dir"_sized_av, {}),
        "\x07""\x0a""\x01\x00""\x00\x00\x00\x00""\x00\x00\x00\x06""my_dir"_av
    );
    /*          |                           |
                | <---------------- CommandReturn(11)
                |                     (create dir)
    */
    ft_saved = ft;
    TEST_RECEIVE(
        "CommandReturn (create dir)",
        "\x0b""\x01\x00""\x00\x00\x00\x00""\x00\x00\x00\x06""my_dir"_av,
        command_return = fn([](void*, bytes_view response, bool is_ok)
        {
            RED_CHECK(is_ok);
            RED_CHECK(response == "my_dir"_av);
        })
    );
    /*          |                           |
                | <---------------- CommandReturn(11)
                |                   (create dir error)
    */
    ft = ft_saved;
    TEST_RECEIVE(
        "CommandReturn (create dir error)",
        "\x0b""\x01\x00""\xff\xff\xff\xff""\x00\x00\x00\x06""my_dir"_av,
        command_return = fn([](void*, bytes_view response, bool is_ok)
        {
            RED_CHECK(!is_ok);
            RED_CHECK(response == "my_dir"_av);
        })
    );

    /*          |                           |
       CommandRequest(10) ----------------> |
            (remove)                        |
    */
    TEST_WRITE(
        FT::write_command_remove_file(out, FT::Path{"my_dir"_sized_av}),
        "\x07""\x0a""\x04\x00""\x00\x00\x00\x00""\x00\x00\x00\x06""my_dir"_av
    );
    /*          |                           |
                | <---------------- CommandReturn(11)
                |                       (remove)
    */
    TEST_RECEIVE(
        "CommandReturn (remove)",
        "\x0b""\x04\x00""\x00\x00\x00\x00""\x00\x00\x00\x06""my_dir"_av,
        command_return = fn([](void*, bytes_view response, bool is_ok)
        {
            RED_CHECK(is_ok);
            RED_CHECK(response == "my_dir"_av);
        })
    );

    /*          |                           |
       CommandRequest(10) ----------------> |
            (rename)                        |
    */
    TEST_WRITE(
        FT::write_command_rename_file(out, {
            .old_name { "my_dir"_sized_av },
            .new_name { "new_dir"_sized_av },
        }),
        "\x07""\x0a""\x05\x00""\x00\x00\x00\x00""\x00\x00\x00\x0e""my_dir*new_dir"_av
    );
    /*          |                           |
                | <---------------- CommandReturn(11)
                |                       (rename)
    */
    ft_saved = ft;
    TEST_RECEIVE(
        "CommandReturn (rename)",
        "\x0b""\x05\x00""\x00\x00\x00\x00""\x00\x00\x00\x0e""my_dir*new_dir"_av,
        command_return = fn([](void*, bytes_view response, bool is_ok)
        {
            RED_CHECK(is_ok);
            RED_CHECK(response == "my_dir*new_dir"_av);
        })
    );
    /*          |                           |
                | <---------------- CommandReturn(11)
                |                 (create, bad response)
    */
    ft = ft_saved;
    TEST_RECEIVE(
        "CommandReturn (create, expected rename, but not checked)",
        "\x0b""\x01\x00""\x00\x00\x00\x00""\x00\x00\x00\x06""my_dir"_av,
        command_return = fn([](void*, bytes_view response, bool is_ok)
        {
            RED_CHECK(is_ok);
            RED_CHECK(response == "my_dir"_av);
        })
    );


    /*          |    ,------------------,   |
                |    | Media / Dir List |   |
                |    '------------------'   |
    */

    /*          |                           |
      DirContentRequest(1) ---------------> |
          (drive list)
    */
    TEST_WRITE(
        FT::write_drives_list_request(out),
        "\07""\x01""\x02\x00""\x00\x00\x00\x00""\x00\x00\x00\x00"_av
    );
    /*          |                           |
                | <------------------- DirPacket(2)
                |                      (drive list)
    */
    TEST_RECEIVE(
        "DirContentRequest + DrivesList",
        "\x02""\x03\x00""\x00\x00\x00\x00""\x00\x00\x00\x0c""C:l\x00""D:c\x00""Z:\\\x00"_av,
        drive_list = fn([](void*, FT::DrivesList drives)
        {
            std::string str_drive_list;
            for (auto drive : drives)
            {
                str_drive_list += static_cast<char>(drive.drive_letter);
                str_drive_list += static_cast<char>(drive.drive_type);
                str_drive_list += '|';
            }
            RED_CHECK(str_drive_list == "Cl|Dc|Z\\|"_av);
        })
    );

    /*          |                           |
      DirContentRequest(1) ---------------> |
          (dir list)
    */
    TEST_WRITE(
        FT::write_directory_content_request2(out, "C:\\"_av, "my_dir"_av),
        "\x07""\x01""\x01\x00""\x00\x00\x00\x00""\x00\x00\x00\x0a""C:\\my_dir\\"_av
    );
    TEST_WRITE(
        FT::write_directory_content_request(out, FT::Path{"my_dir"_sized_av}),
        "\x07""\x01""\x01\x00""\x00\x00\x00\x00""\x00\x00\x00\x06""my_dir"_av
    );
    /*          |                           |
                | <------------------- DirPacket(2)
                |               (dir list ; first packet)
    */
    TEST_RECEIVE(
        "DirContentRequest + Content (first packet)",
        "\x02""\x01\x00""\x00\x00\x00\x00""\x00\x00\x00\x06""my_dir"_av,
        start_list_dir = fn([](void*, FT::Path dirname)
        {
            RED_CHECK(dirname.native() == "my_dir"_av);
        })
    );
    /*          |                           |
                | <------------------- DirPacket(2)
                |               (dir list ; receive file 1)
    */
    TEST_RECEIVE(
        "DirContentRequest + Content (file 1)",
        "\x02""\x01\x00""\x00\x00\x00\x00""\x00\x00\x00\x33"
        "\x01\x02\x03\x04" // dwFileAttributes
        "\x05\x06\x07\x08\x09\x0a\x0b\x0c" // ftCreationTime
        "\x0d\x0e\x0f\x10\x11\x12\x13\x14" // ftLastAccessTime
        "\x15\x16\x17\x18\x19\x1a\x1b\x1c" // ftLastWriteTime
        "\x1d\x1e\x1f\x20" // nFileSizeHigh
        "\x21\x22\x23\x24" // nFileSizeLow
        "\x25\x26\x27\x28" // dwReserved0
        "\x29\x2a\x2b\x2c" // dwReserved1
        "file1\0\0"_av,
        file_info = fn([](void*, FT::FileInfoPDU file_info)
        {
            RED_CHECK_HEX32(file_info.attributes == WinNtFileAttributeFlags{0x04'03'02'01});
            RED_CHECK_HEX32(time_to_i(file_info.creation_time) == 0x0C'0B'0A'09'08'07'06'05);
            RED_CHECK_HEX64(time_to_i(file_info.last_access_time) == 0x14'13'12'11'10'0F'0E'0D);
            RED_CHECK_HEX64(time_to_i(file_info.last_write_time) == 0x1C'1B'1A'19'18'17'16'15);
            RED_CHECK_HEX64(file_info.file_size() == 0x20'1F'1E'1D'24'23'22'21);
            RED_CHECK(file_info.file_name == "file1"_av);
        })
    );
    /*          |                           |
                | <------------------- DirPacket(2)
                |               (dir list ; receive file 2)
    */
    TEST_RECEIVE(
        "DirContentRequest + Content (file 2)",
        "\x02""\x01\x00""\x00\x00\x00\x00""\x00\x00\x00\x38"
        "\x11\x12\x13\x14" // dwFileAttributes
        "\x15\x16\x17\x18\x19\x1a\x1b\x1c" // ftLastAccessTime
        "\x05\x06\x07\x08\x09\x0a\x0b\x0c" // ftLastWriteTime
        "\x0d\x0e\x0f\x10\x11\x12\x13\x14" // ftCreationTime
        "\x21\x22\x23\x24" // nFileSizeHigh
        "\x1d\x1e\x1f\x20" // nFileSizeLow
        "\x25\x26\x27\x28" // dwReserved0
        "\x29\x2a\x2b\x2c" // dwReserved1
        "other_file\0\0"_av,
        file_info = fn([](void*, FT::FileInfoPDU file_info)
        {
            RED_CHECK_HEX32(file_info.attributes == WinNtFileAttributeFlags{0x14'13'12'11});
            RED_CHECK_HEX32(time_to_i(file_info.creation_time) == 0x1C'1B'1A'19'18'17'16'15);
            RED_CHECK_HEX64(time_to_i(file_info.last_access_time) == 0x0C'0B'0A'09'08'07'06'05);
            RED_CHECK_HEX64(time_to_i(file_info.last_write_time) == 0x14'13'12'11'10'0F'0E'0D);
            RED_CHECK_HEX64(file_info.file_size() == 0x24'23'22'21'20'1F'1E'1D);
            RED_CHECK(file_info.file_name == "other_file"_av);
        })
    );
    ft_saved = ft;
    // receive drive list, but File / EndFile expected
    TEST_RECEIVE_ERROR(
        "DirContentRequest + no EndList",
        // drive list response (without data)
        "\x02""\x03\x00""\x00\x00\x00\x00""\x00\x00\x00\x0c"_av,
        error = fn([](void*, UVNCFileTransferReader::ProtocolError err)
        {
            RED_CHECK(err.type == UVNCFileTransferReader::ProtocolError::Type::InvalidFileListSequence);
            RED_CHECK(err.max_or_min_len == 0);
        })
    );
    ft = ft_saved;
    // receive FileTransferAccess, but File / EndFile expected
    TEST_RECEIVE_ERROR(
        "FileTransferAccess failure",
        "\x0e""\x00\x00""\xff\xff\xff\xff""\x00\x00\x00\x00"_av,
        error = fn([](void*, UVNCFileTransferReader::ProtocolError err)
        {
            RED_CHECK(err.type == UVNCFileTransferReader::ProtocolError::Type::InvalidFileListSequence);
            RED_CHECK(err.max_or_min_len == 0);
        })
    );
    ft = ft_saved;
    /*          |                           |
                | <------------------- DirPacket(2)
                |                    (dir list ; end)
    */
    TEST_RECEIVE(
        "DirContentRequest + Content (end)",
        "\x02""\x00\x00""\x00\x00\x00\x00""\x00\x00\x00\x00"_av,
        end_list_dir = fn([](void*) {})
    );
    ft_saved = ft;
    // unknown subtype
    TEST_RECEIVE_ERROR(
        "DirContentRequest + no EndList",
        "\x02""\x0f\x00""\x00\x00\x00\x00""\x00\x00\x00\x00"_av,
        error = fn([](void*, UVNCFileTransferReader::ProtocolError err)
        {
            RED_CHECK(err.type == UVNCFileTransferReader::ProtocolError::Type::UnknownSubType);
            RED_CHECK(err.max_or_min_len == 0);
        })
    );
    ft = ft_saved;


    /*
                |       ,-----------,       |
                |       | Send File |       |
                |       '-----------'       |
    */

    /*          |                           |
       FileTransferOffer(8) --------------> |
    */
    TEST_WRITE(
        FT::write_file_transfer_offer(
            out, FT::Path{"my_file"_sized_av}, 0x0000'1020'0000'3040,
            clock_cast<WinNtClock>(sys_days{2025y / April / 23d} + 17h + 21min)
        ),
        "\x07""\x08""\x01\x00""\x00\x00\x30\x40""\x00\x00\x00\x18""my_file"
        ",04/23/2025 17:21""\0\0\x10\x20"_av
    );
    ft_saved = ft;
    /*          |                           |
                | <--------------- FileAcceptHeader(9)
    */
    TEST_RECEIVE(
        "FileAcceptHeader Ok, without checksums",
        "\x09""\x00\x00""\x00\x00\x00\x00""\x00\x00\x00\x07""my_file"_av,
        file_accept_header = fn([](void*, bytes_view file_name, bool accepted) {
            RED_CHECK(accepted);
            RED_CHECK(file_name == "my_file"_av);
        })
    );
    ft = ft_saved;
    TEST_RECEIVE(
        "FileAcceptHeader Failure, without checksums",
        "\x09""\x00\x00""\xff\xff\xff\xff""\x00\x00\x00\x07""my_file"_av,
        file_accept_header = fn([](void*, bytes_view file_name, bool accepted) {
            RED_CHECK(!accepted);
            RED_CHECK(file_name == "my_file"_av);
        })
    );
    ft = ft_saved;
    /*          |                           |
                | <---------------- FileChecksums(12) (optional)
    */
    TEST_RECEIVE(
        "FileChecksums",
        "\x0C""\x00\x00""\x00\x00\x30\x00""\x00\x00\x00\x08""abcdefgh"_av,
        file_partial_checksums = fn([](void*, bytes_view checksums, uint32_t remaining) {
            RED_CHECK(remaining == 0);
            RED_CHECK(checksums == "abcdefgh"_av);
        }),
        SkipPartialPacket::Yes
    );
    // partial checksums
    ft = ft_saved;
    RED_TEST_CONTEXT("receive: FileChecksums (partial checksums)" LINE_MARKER)
    {
        auto data = "\x0C""\x00\x00""\x00\x00\x30\x00""\x00\x00\x00\x0c""abcdefghijkl"_av;

        TEST_RECEIVE_EXACTLY(
            "packet 1/2",
            data.drop_back(4),
            file_partial_checksums = fn([](void*, bytes_view checksums, uint32_t remaining) {
                RED_CHECK(remaining == 4);
                RED_CHECK(checksums == "abcdefgh"_av);
            }),
            ReadPacketStatus::WaitData
        );

        TEST_RECEIVE_EXACTLY(
            "packet 2/2",
            data.last(4),
            file_partial_checksums = fn([](void*, bytes_view checksums, uint32_t remaining) {
                RED_CHECK(remaining == 0);
                RED_CHECK(checksums == "ijkl"_av);
            }),
            ReadPacketStatus::Completed
        );
    }
    /*          |                           |
                | <--------------- FileAcceptHeader(9)
    */
    TEST_RECEIVE(
        "FileAcceptHeader Ok after checksums",
        "\x09""\x00\x00""\x00\x00\x00\x00""\x00\x00\x00\x07""my_file"_av,
        file_accept_header = fn([](void*, bytes_view file_name, bool accepted) {
            RED_CHECK(accepted);
            RED_CHECK(file_name == "my_file"_av);
        })
    );

    /*          |                           |
          FilePacket(5) ------------------> |
    */
    RED_TEST_CONTEXT("write_file_packet little packet" LINE_MARKER)
    {
        StaticOutStream<255> out;
        auto result = FT::write_file_packet(
            out, FT::FilePacketType::Uncompressed, "abcdefghijkl"_av, block_size);
        RED_CHECK(result.ec == ErrorCode::NoError);
        RED_CHECK(out.get_produced_bytes()
            == "\x07""\x05""\x00\x00""\x00\x00\x00\x00""\x00\x00\x00\x0c""abcdefghijkl"_av);
        RED_CHECK(result.remaining_in_data == ""_av);
    }
    RED_TEST_CONTEXT("write_file_packet too short buffer" LINE_MARKER)
    {
        StaticOutStream<FT::header_packet_size> out;
        auto data = "012345678901234567890123456789"_av;
        auto result
            = FT::write_file_packet(
                out, FT::FilePacketType::Uncompressed, data, block_size
            );
        RED_CHECK(result.ec == ErrorCode::TooSmallBuffer);
    }
    RED_TEST_CONTEXT("write_file_packet big packet" LINE_MARKER)
    {
        StaticOutStream<packet_size> out;
        // ['a' * block_size, 'b', 'c' * 99]
        std::vector<char> big_data;
        big_data.resize(block_size, 'a');
        big_data.resize(block_size + 100, 'c');
        big_data[block_size] = 'b';

        std::vector<char> expected_pkt;
        bytes_view d = "\x07""\x05""\x00\x00""\x00\x00\x00\x00""\x00\x00\x20\x00"_av;
        expected_pkt.insert(expected_pkt.end(), d.begin(), d.end());
        auto written_part = bytes_view{big_data}.first(block_size);
        expected_pkt.insert(expected_pkt.end(), written_part.begin(), written_part.end());
        auto expected_first_pkt_size = expected_pkt.size();

        auto result
            = FT::write_file_packet(
                out, FT::FilePacketType::Uncompressed, big_data, block_size
            );
        RED_CHECK(result.ec == ErrorCode::NoError);
        RED_CHECK(out.get_produced_bytes() == expected_pkt);

        RED_TEST_CONTEXT("write_multi_uncompressed_file_packets (2 packets)" LINE_MARKER)
        {
            d = "\x07""\x05""\x00\x00""\x00\x00\x00\x00""\x00\x00\x00\x64"_av;
            expected_pkt.insert(expected_pkt.end(), d.begin(), d.end());
            expected_pkt.resize(expected_pkt.size() + 100, 'c');
            expected_pkt[expected_pkt.size() - 100] = 'b';

            StaticOutStream<packet_size * 2> out_stream;
            auto result
                = FT::write_multi_uncompressed_file_packets(
                    out_stream, big_data, block_size
                );
            RED_CHECK(result.ec == ErrorCode::NoError);
            RED_CHECK(result.remaining_in_data == ""_av);
            RED_CHECK(out_stream.get_produced_bytes() == expected_pkt);
        }

        RED_TEST_CONTEXT("write_multi_uncompressed_file_packets (1.2 packets)" LINE_MARKER)
        {
            expected_pkt.resize(expected_first_pkt_size);
            d = "\x07""\x05""\x00\x00""\x00\x00\x00\x00""\x00\x00\x00\x4E"_av;
            expected_pkt.insert(expected_pkt.end(), d.begin(), d.end());
            expected_pkt.resize(expected_pkt.size() + 78, 'c');
            expected_pkt[expected_pkt.size() - 78] = 'b';

            StaticOutStream<packet_size + 90> out_stream;
            auto result
                = FT::write_multi_uncompressed_file_packets(
                    out_stream, big_data, FT::min_block_size
                );
            RED_CHECK(result.ec == ErrorCode::TooSmallBuffer);
            RED_CHECK(result.remaining_in_data == "cccccccccccccccccccccc"_av);
            RED_CHECK(out_stream.get_produced_bytes() == expected_pkt);
        }
    }

    /*          ⋮                           |
           EndOfFile(6) ------------------> |
    */
    ft_saved = ft;
    TEST_WRITE(
        FT::write_end_of_file(out),
        "\x07""\x06""\x00\x00""\x00\x00\x00\x00""\x00\x00\x00\x00"_av
    );
    ft = ft_saved;
    /*         or                           |
       AbortFileTransfer(7) --------------> |
    */
    TEST_WRITE(
        FT::write_abort_file_transfer(out),
        "\x07""\x07""\x03\x00""\x00\x00\x00\x00""\x00\x00\x00\x00"_av
    );


    /*
                |     ,---------------,     |
                |     | Donwload File |     |
                |     '---------------'     |
    */

    /*          |                           |
     FileTransferRequest(3) --------------> |
    */
    TEST_WRITE(
        FT::write_file_request(
            out, FT::FileRequestedFormat::Uncompressed, FT::Path{"my_file"_sized_av}
        ),
        "\x07""\x03""\x00\x00""\x00\x00\x00\x00""\x00\x00\x00\x07""my_file"_av
    );
    /*          |                           |
                | <------------------ FileHeader(4)
    */
    ft_saved = ft;
    TEST_RECEIVE(
        "FileHeader (failure)",
        "\x04""\x00\x00""\xff\xff\xff\xff""\x00\x00\x00\x07""my_file""\xff\xff\xff\xff"_av,
        file_header = fn([](
            void*,
            bytes_view file_name_with_optional_date,
            UVNC::FileTransfer::FileSizeOrError file_size_or_error)
        {
            RED_CHECK(file_size_or_error.is_error());
            RED_CHECK(file_name_with_optional_date == "my_file"_av);
        })
    );
    ft = ft_saved;
    TEST_RECEIVE(
        "FileHeader (ok)",
        "\x04""\x00\x00""\x00\x00\x67\x21""\x00\x00\x00\x07""my_file""\x00\x00\xb0\x00"_av,
        file_header = fn([](void*,
            bytes_view file_name_with_optional_date,
            UVNC::FileTransfer::FileSizeOrError file_size_or_error)
        {
            RED_CHECK(!file_size_or_error.is_error());
            RED_CHECK(file_name_with_optional_date == "my_file"_av);
            RED_CHECK_HEX64(file_size_or_error.file_size() == 0xb000'00006721);
        })
    );

    /*          |                           |
        FileChecksums(12) (optional) -----> |
    */
    // (unimplemented)

    /*          |                           |
          FileHeader(4) ------------------> |
    */
    TEST_WRITE(
        FT::write_confirm_requested_file(out, true),
        "\x07""\x04""\x00\x00""\x00\x00\x00\x00""\x00\x00\x00\x00"_av
    );
    /*          |                           |
                | <------------------ FilePacket(5)
    */
    ft_saved = ft;
    TEST_RECEIVE_ERROR(
        "FilePacket (download with invalid FilePacketType)",
        "\x05""\x00\x00""\x00\x00\x00\x11""\x00\x00\x00\x06""blabla"_av,
        error = fn([](void*, UVNCFileTransferReader::ProtocolError err)
        {
            RED_CHECK(err.type == UVNCFileTransferReader::ProtocolError::Type::UnknownFilePacketType);
            RED_CHECK(err.max_or_min_len == 0);
        })
    );
    ft = ft_saved;
    RED_TEST_CONTEXT("FilePacket (download ; read partial)" LINE_MARKER)
    {
        TEST_RECEIVE_EXACTLY(
            "packet 1/2",
            "\x05""\x00\x00""\x00\x00\x00\x00""\x00\x00\x00\x06""1234"_av,
            file_partial_packet = fn([](void*, bytes_view data, FT::FilePacketType file)
            {
                RED_CHECK(data == "1234"_av);
                RED_CHECK(file == FT::FilePacketType::Uncompressed);
            }),
            ReadPacketStatus::WaitData
        );

        TEST_RECEIVE_EXACTLY(
            "packet 2/2",
            "56"_av,
            file_partial_packet = fn([](void*, bytes_view data, FT::FilePacketType file)
            {
                RED_CHECK(data == "56"_av);
                RED_CHECK(file == FT::FilePacketType::Uncompressed);
            }),
            ReadPacketStatus::Completed
        );
    }
    ft = ft_saved;
    TEST_RECEIVE(
        "FilePacket (download)",
        "\x05""\x00\x00""\x00\x00\x00\x00""\x00\x00\x00\x06""blabla"_av,
        file_partial_packet = fn([](void*, bytes_view data, FT::FilePacketType file)
        {
            RED_CHECK(data == "blabla"_av);
            RED_CHECK(file == FT::FilePacketType::Uncompressed);
        }),
        SkipPartialPacket::Yes
    );
    /*          |                           ⋮
                | <------------------- EndOfFile(6) (when no abort)
    */
    ft_saved2 = ft;
    TEST_RECEIVE(
        "EndOfFile (download)",
        "\x06""\x00\x00""\x00\x00\x00\x00""\x00\x00\x00\x00"_av,
        end_of_file = fn([](void*) {})
    );
    /*
                |                           or
                | <--------------- AbortFileTransfer(7) (when no abort)
    */
    ft = ft_saved2;
    TEST_RECEIVE(
        "AbortFileTransfer (download)",
        "\x07""\x00\x00""\x00\x00\x00\x00""\x00\x00\x00\x00"_av,
        aborted_file = fn([](void*) {})
    );

    /*          |                           |
    FileTransferSessionEnd ---------------> |
    */
    TEST_WRITE(
        FT::write_session_end(out),
        "\07""\x10""\x00\x00""\x00\x00\x00\x00""\x00\x00\x00\x00"_av
    );

#undef TEST_WRITE
#undef TEST_RECEIVE
#undef LINE_MARKER
}
