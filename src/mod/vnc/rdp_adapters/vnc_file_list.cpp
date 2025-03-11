/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "mod/vnc/rdp_adapters/vnc_file_list.hpp"
#include "mod/vnc/rdp_adapters/rdpeclip.hpp"
#include "mod/vnc/encoders/uvnc_file_transfer.hpp"
#include "utils/sugar/bytes_copy.hpp"
#include "utils/mathutils.hpp"

#include <array>

/*

Download

          RDP Client                      Proxy                   VNC Server
              |                             |                          |
----------------------------------------------------------------------------------------
              |                             |                          |
              |              ┌────────────────────────┬───────────────────────────┐
         FormatList ------>  │ if (status=WaitAccept) │                |          │
              or             ├────────────────────────┘                |          │
           timeout           │              |                          |          │
              |              │              | <------------------- FileHeader     │
              |              │              |                          |          │
              |              │              |    [if resp = ok]        |          │
              |              │              | ------(abort)------> FileHeader     │
              |              │              |                          |          │
              |              │      (status=Nothing)                   |          │
              |              │              |                          |          │
              |              ├────────────────────────────────┬───────────────────┤
              |              │ if (status=TransferInProgress) │        |          │
              |              ├────────────────────────────────┘        |          │
              |              │              |                          |          │
              |              │              | ---------------> AbortFileTransfer  │
              |              │              |                          |          │
              |              │      (status=Nothing)                   |          │
              |              │              |                          |          │
              |              ├───────────────────────────────┬────────────────────┤
              |              │ if (status=WaitAbortResponse) │         |          │
              |              ├───────────────────────────────┘         |          │
              |              │              |                          |          │
              |              │      (status=Nothing)                   |          │
              |              │              |                          |          │
              |              └────────────────────────────────────────────────────┘
              |                             |                          |
----------------------------------------------------------------------------------------
              |                             |                          |
              |              ┌─────────────────────────────────┬─────────────────────┐
      FileContentRequest --> │ if (invalid_lindex or new_file) │       |             │
              |              ├─────────────────────────────────┘       |             │
              |              │              |                          |             │
              |              │  ┌────────────────────────┬────────────────────────┐  │
              |              │  │ if (status=WaitAccept) │             |          │  │
              |              │  ├────────────────────────┘             |          │  │
              |              │  │           |                          |          │  │
              |              │  │           | <------------------- FileHeader     │  │
              |              │  │           |                          |          │  │
              |              │  │           |    [if resp = ok]        |          │  │
              |              │  │           | ------(abort)------> FileHeader     │  │
              |              │  │           |                          |          │  │
              |              │  │   (status=Nothing)                   |          │  │
              |              │  │           |                          |          │  │
              |              │  ├────────────────────────────────┬────────────────┤  │
              |              │  │ if (status=TransferInProgress) │     |          │  │
              |              │  ├────────────────────────────────┘     |          │  │
              |              │  │           |                          |          │  │
              |              │  │           | ---------------> AbortFileTransfer  │  │
              |              │  │           |                          |          │  │
              |              │  │ (status=WaitAbortResponse)           |          │  │
              |              │  │           |                      EndOfFile      │  │
              |              │  │           | <----------------------- or         │  │
              |              │  │           |                  AbortFileTransfer  │  │
              |              │  │   (status=Nothing)                   |          │  │
              |              │  │           |                          |          │  │
              |              │  ├───────────────────────────────┬─────────────────┤  │
              |              │  │ if (status=WaitAbortResponse) │      |          │  │
              |              │  ├───────────────────────────────┘      |          │  │
              |              │  │           |                      EndOfFile      │  │
              |              │  │           | <----------------------- or         │  │
              |              │  │           |                  AbortFileTransfer  │  │
              |              │  │   (status=Nothing)                   |          │  │
              |              │  │           |                          |          │  │
              |              │  └─────────────────────────────────────────────────┘  │
              |              │              |                          |             │
              |              └───────────────────────────────────────────────────────┘
              |                             |                          |
              |              ┌─────────────────────┬─────────────────────────────────┐
              |              │ if (invalid_lindex) │                   |             │
              |              ├─────────────────────┘                   |             │
              |              │              |                          |             │
     FileContentResponse <-------(error)--- |                          |             │
              |              │              |                          |             │
              |              ├───────────────┬───────────────────────────────────────┤
              |              │ if (new_file) │                         |             │
              |              ├───────────────┘                         |             │
              |              │              |                          |             │
              |              │  ┌───────────────────────────┬─────────────────────┐  │
              |              │  │ if (unsequenced_sequence) │          |          │  │
              |              │  ├───────────────────────────┘          |          │  │
              |              │  │           |                          |          │  │
     FileContentResponse <--------(error)-- |                          |          │  │
              |              │  │           |                          |          │  │
              |              │  │   (status=Nothing)                   |          │  │
              |              │  │           |                          |          │  │
              |              │  ├──────┬──────────────────────────────────────────┤  │
              |              │  │ else │    |                          |          │  │
              |              │  ├──────┘    |                          |          │  │
              |              │  │           | --------------> FileTransferRequest │  │
              |              │  │           |                          |          │  │
              |              │  │  (status=WaitAccept)                 |          │  │
              |              │  │           |                          |          │  │
              |              │  └─────────────────────────────────────────────────┘  │
              |              │              |                          |             │
              |              ├───────────────────────────┬───────────────────────────┤
              |              │ if (unsequenced_sequence) │             |             │
              |              ├───────────────────────────┘             |             │
              |              │              |                          |             │
              |              │  ┌────────────────────────┬────────────────────────┐  │
              |              │  │ if (status=WaitAccept) │             |          │  │
              |              │  ├────────────────────────┘             |          │  │
              |              │  │           |                          |          │  │
              |              │  │           | <------------------- FileHeader     │  │
              |              │  │           |                          |          │  │
              |              │  │           |    [if resp = ok]        |          │  │
              |              │  │           | ------(abort)------> FileHeader     │  │
              |              │  │           |                          |          │  │
              |              │  │   (status=Nothing)                   |          │  │
              |              │  │           |                          |          │  │
              |              │  ├────────────────────────────────┬────────────────┤  │
              |              │  │ if (status=TransferInProgress) │     |          │  │
              |              │  ├────────────────────────────────┘     |          │  │
              |              │  │           |                          |          │  │
              |              │  │           | ---------------> AbortFileTransfer  │  │
              |              │  │           |                          |          │  │
              |              │  │ (status=WaitAbortResponse)           |          │  │
              |              │  │           |                          |          │  │
              |              │  └─────────────────────────────────────────────────┘  │
              |              │              |                          |             │
     FileContentResponse <-------(error)--- |                          |             │
              |              │              |                          |             │
              |              ├──────────────────────────┬────────────────────────────┤
              |              │ if (has_sufficient_data) │              |             │
              |              ├──────────────────────────┘              |             │
              |              │              |                          |             │
     FileContentResponse <-------(data)---- |                          |             │
              |              │              |                          |             │
              |              └───────────────────────────────────────────────────────┘
              |                             |                          |
----------------------------------------------------------------------------------------
              |                             |                          |
┌────────────────────────┬────────────────────────────────────┐        |
│ if (status=WaitAccept) │                  |                 │ <- FileHeader
├────────────────────────┘                  |                 │        |
│             |                             |                 │        |
│  ┌───────────────┬───────────────────────────────────────┐  │        |
│  │ if (resp=err) │                        |              │  │        |
│  ├───────────────┘                        |              │  │        |
│  │          |                             |              │  │        |
│  │ FileContentResponse <-----(error)----- |              │  │        |
│  │          |                             |              │  │        |
│  │          |                     (status=Nothing)       │  │        |
│  │          |                             |              │  │        |
│  ├──────────────┬────────────────────────────────────────┤  │        |
│  │ if (resp=ok) │                         |              │  │        |
│  ├──────────────┘                         |              │  │        |
│  │          |                (status=TransferInProgress) │  │        |
│  │          |                             |              │  │        |
│  │          |                             | -----(ok)----------> FileHeader
│  │          |                             |              │  │        |
│  └───────────────────────────────────────────────────────┘  │        |
│             |                             |                 │        |
└─────────────────────────────────────────────────────────────┘        |
              |                             |                          |
----------------------------------------------------------------------------------------
              |                             |                          |
┌────────────────────────────────┬───────────────────┐                 |
│ if (status=TransferInProgress) │          |        │<---------- FilePacket
├────────────────────────────────┘          |        │                 |
│             |                             |        │                 |
│             |                        (buffering)   │                 |
│             |                             |        │                 |
│  ┌──────────────────────────┬────────────────────┐ │                 |
│  │ if (has_sufficient_data) │             |      │ │                 |
│  ├──────────────────────────┘             |      │ │                 |
│  │          |                             |      │ │                 |
│  │ FileContentResponse <------(data)----- |      │ │                 |
│  │          |                             |      │ │                 |
│  └───────────────────────────────────────────────┘ │                 |
│             |                             |        │                 |
└────────────────────────────────────────────────────┘                 |
              |                             |                          |
----------------------------------------------------------------------------------------
              |                             |                          |
┌────────────────────────────────┬───────────────────┐                 |
│ if (status=TransferInProgress) │          |        │ <---------- EndOfFile
├────────────────────────────────┘          |        │                 |
│             |                             |        │                 |
│             |                             |        │                 |
│    FileContentResponse <------(data)----- |        │                 |
│             |                             |        │                 |
│             |                     (status=Nothing) │                 |
│             |                             |        │                 |
└────────────────────────────────────────────────────┘                 |
              |                             |                          |
----------------------------------------------------------------------------------------
              |                             |                          |
┌────────────────────────────────┬───────────────────┐                 |
│ if (status=TransferInProgress) │          |        │ <------ AbortFileTransfer
├────────────────────────────────┘          |        │                 |
│             |                             |        │                 |
│    FileContentResponse <-----(error)----- |        │                 |
│             |                             |        │                 |
│             |                     (status=Nothing) │                 |
│             |                             |        │                 |
└────────────────────────────────────────────────────┘                 |
              |                             |                          |
----------------------------------------------------------------------------------------

*/

namespace
{
    namespace FT = UVNC::FileTransfer;
    using FT_WriteErrorCode = UVNC::FileTransfer::WriteErrorCode;
}


enum VNC::VncFileList::TransferStatus : uint8_t
{
    Nothing,
    WaitAcceptAndContinue,
    WaitAcceptAndReject,
    TransferInProgress,
    // File aborted by client (ex: new file) or when server sent more data that file size.
    // To the second case, m_rdp_req_continuation_lindex stay valid.
    WaitAbortResponse,
    Finished,
};


struct VNC::VncFileList::D
{
    static constexpr VNC::CbLindex INVALID_LINDEX { ~uint32_t{} };

    static constexpr chars_view vnc_abort_file = FT::abort_file_transfer_pdu;
    static constexpr chars_view vnc_confirm_accept = FT::confirm_requested_file_ok;
    static constexpr chars_view vnc_confirm_reject = FT::confirm_requested_file_failure;

    static constexpr uint32_t vnc_pdus_stream_size
        = FT::file_request_pdu_max_len
        + mmax({
            vnc_confirm_accept.size(),
            vnc_confirm_accept.size(),
            vnc_abort_file.size(),
        });
    static constexpr uint32_t rdp_pdus_stream_size
        = 2 * VNC::CliprdrHeader::pdu_len()
        + VNC::FileContentsResponseWithoutData::pdu_len()
        + mmax(
            VNC::FileContentsResponseSize::pdu_len(),
            VNC::FileContentsResponseWithoutData::pdu_len()
        );
    static constexpr uint32_t rdp_file_data_offset = vnc_pdus_stream_size + rdp_pdus_stream_size;

    static OutStream vnc_stream(VncFileList & self) noexcept
    {
        return OutStream {
            writable_bytes_view{ self.m_transfer_buffer }
            .first(D::vnc_pdus_stream_size)
        };
    }


    enum RdpMsgFlag
    {
        RdpMsg_PreviousResponseData = 1 << 0,
        RdpMsg_Response = 1 << 1,
        RdpMsg_WithData = 1 << 2,
    };

    static OutStream rdp_stream(
        VncFileList & self,
        uint32_t rdp_msg_flags,
        bool is_size_request
    ) noexcept
    {
        constexpr auto header_len = VNC::CliprdrHeader::pdu_len();
        constexpr auto pdu_size_len = VNC::FileContentsResponseSize::pdu_len();
        constexpr auto pdu_range_len = VNC::FileContentsResponseWithoutData::pdu_len();

        uint32_t nb_bytes = 0;
        nb_bytes += (rdp_msg_flags & RdpMsg_PreviousResponseData)
            ? header_len + pdu_range_len
            : 0;
        nb_bytes += (rdp_msg_flags & RdpMsg_Response)
            ? header_len + (is_size_request ? pdu_size_len : pdu_range_len)
            : 0;

        return OutStream {
            writable_bytes_view{ self.m_transfer_buffer }
            .drop_front(self.m_transfer_data_offset - nb_bytes)
        };
    }

    static OutStream rdp_stream(VncFileList & self) noexcept
    {
        return OutStream {
            writable_bytes_view{ self.m_transfer_buffer }
            .drop_front(vnc_pdus_stream_size)
        };
    }

    static VNC::VncFileList::TransferResult write_rdp_response_data(VncFileList & self) noexcept
    {
        OutStream rdp_stream = D::rdp_stream(self, D::RdpMsg_Response, false);

        write_rdp_response_range_and_consume_data(self, rdp_stream);

        return {
            .rdp_data = rdp_stream.get_produced_bytes(),
            .vnc_data = {},
            .response_types {
                TransferResult::ResponseType::RdpResponseData,
            },
        };
    }

    static void write_rdp_response_range_and_consume_data(
        VncFileList & self, OutStream & rdp_stream
    ) noexcept
    {
        assert(self.m_transfer_data_offset <= self.m_transfer_buffer.size());

        auto data_len = mmin(
            self.m_transfer_buffer.size() - self.m_transfer_data_offset,
            self.m_rdp_req_requested
        );
        self.m_transfer_data_offset += data_len;

        make_file_contents_response_data_with_header(self.m_rdp_req_stream_id, data_len)
            .write_unchecked(rdp_stream);
        rdp_stream.out_skip_bytes(data_len);
    }

    static bool write_vnc_file_request(VncFileList & self, OutStream & vnc_stream) noexcept
    {
        if (self.m_rdp_req_lindex == D::INVALID_LINDEX)
        {
            return false;
        }

        auto lindex = underlying_cast(self.m_rdp_req_lindex);
        auto & file = self.m_internal_files[lindex];
        [[maybe_unused]]
        auto ec = FT::write_uncompressed_file_request(vnc_stream, file.name());
        assert(is_ok(ec));

        return true;
    }

    static bytes_view set_rdp_response_fail(VncFileList & self) noexcept
    {
        assert(self.m_rdp_req_lindex != D::INVALID_LINDEX);
        assert(self.m_rdp_req_continuation_lindex != D::INVALID_LINDEX);

        self.m_rdp_req_lindex = D::INVALID_LINDEX;
        self.m_rdp_req_continuation_lindex = D::INVALID_LINDEX;

        OutStream rdp_stream {
            writable_bytes_view{ self.m_transfer_buffer }
            .drop_front(vnc_pdus_stream_size)
        };

        make_file_contents_response_error(self.m_rdp_req_stream_id)
            .write_unchecked(rdp_stream);

        return rdp_stream.get_produced_bytes();
    }

    static bool has_sufficient_data(VncFileList & self) noexcept
    {
        assert(self.m_transfer_data_offset <= self.m_transfer_buffer.size());
        return self.m_transfer_buffer.size() - self.m_transfer_data_offset >= self.m_rdp_req_requested;
    }
};


VNC::UVncFile::PathView VNC::VncFileList::File::name() const noexcept
{
    return UVncFile::PathView::assumed(vnc_file_name, vnc_file_name_len);
}


VNC::VncFileList::VncFileList(Params params) noexcept
    : m_max_file_size{ mmin(
        params.max_file_size,
        ~uint64_t{} - D::vnc_pdus_stream_size - D::rdp_pdus_stream_size
    ) }
    , m_max_nb_files{ mmin(params.max_nb_files, FileListWithoutArray::max_items) }
    , m_transfer_status{ TransferStatus::Nothing }
    , m_rdp_req_continuation_lindex{ D::INVALID_LINDEX }
    , m_rdp_req_lindex{ D::INVALID_LINDEX }
{
    m_transfer_data_offset = D::rdp_file_data_offset;
    m_transfer_buffer.resize(m_transfer_data_offset);

    (void)start_rdp_file_list();
}

VNC::VncFileList::~VncFileList() = default;

VNC::VncFileList::File const * VNC::VncFileList::get_current_file() const noexcept
{
    auto lindex = underlying_cast(m_rdp_req_continuation_lindex);
    return lindex < m_internal_files.size()
        ? &m_internal_files[lindex]
        : nullptr;
}

VNC::VncFileList::TransferResult
VNC::VncFileList::start_new_list(UVncFile::PathView dir_base) noexcept
{
    m_vnc_dir_position = 0;
    m_vnc_current_dir_name = nullptr;
    m_vnc_current_dir_name_len = 0;
    m_dir_base_len = 0;

    auto result = start_rdp_file_list();

    /*
     * Release memory
     */

    m_internal_files.clear();
    m_mbr_files.release();

    /*
     * Init current directory
     */

    auto dir = win_dir_remove_end_separator(dir_base.native());
    // add end sep for vnc request ; result maybe greater than max path len
    auto len = dir.size() + 1;

    auto file_name = static_cast<uint8_t*>(m_mbr_files.allocate(len, 1));

    auto * p = bytes_copy_and_advance(file_name, dir);
    *p = '\\';

    m_vnc_current_dir_name = file_name;
    m_vnc_current_dir_name_len = mmin(len, UVncFile::PathView::Bytes::msize_at_most);

    m_dir_base_len = m_vnc_current_dir_name_len;

    return result;
}

VNC::VncFileList::TransferResult
VNC::VncFileList::start_rdp_file_list() noexcept
{
    m_rdp_file_position = 0;
    m_rdp_channel_flags = CompressedChannelFlags::First
                        | CompressedChannelFlags::ShowProtocol;

    return stop_file_transfer();
}

VNC::VncFileList::PushFileResult VNC::VncFileList::push_file_in_current_dir(UVncFile const & file)
{
    if (m_internal_files.size() == m_max_nb_files)
    {
        return PushFileResult::TooManyFiles;
    }

    if (file.file_size >= m_max_file_size)
    {
        return PushFileResult::FileSizeTooLarge;
    }

    auto dir_base = bytes_view{ m_vnc_current_dir_name, m_vnc_current_dir_name_len };
    auto path = win_dir_remove_end_separator(file.file_name.native());
    auto len = dir_base.size() + path.size() + file.is_dir;

    if (UVncFile::is_path_too_large(len))
    {
        return PushFileResult::FinalPathTooLarge;
    }

    auto * file_name = static_cast<uint8_t*>(m_mbr_files.allocate(len, 1));

    auto * p = file_name;
    p = bytes_copy_and_advance(p, dir_base);
    p = bytes_copy_and_advance(p, path);
    if (file.is_dir)
    {
        *p++ = '\\';
    }

    m_internal_files.push_back({
        .vnc_file_name = file_name,
        .file_size = file.file_size,
        .last_access_time = file.last_access_time,
        .vnc_file_name_len = checked_int{ len },
        .is_dir = file.is_dir,
    });

    return PushFileResult::Ok;
}

VNC::VncFileList::NextDirectoryResult
VNC::VncFileList::write_next_vnc_directory_content_request(OutStream & out_stream) noexcept
{
    auto first = m_internal_files.begin() + m_vnc_dir_position;
    auto last = m_internal_files.end();

    for (; first < last; ++first)
    {
        if (first->is_dir)
        {
            switch (FT::write_directory_content_request(out_stream, first->name()))
            {
                case FT_WriteErrorCode::NoError:
                    m_vnc_dir_position = checked_int{ (first - m_internal_files.begin()) + 1 };
                    m_vnc_current_dir_name = first->vnc_file_name;
                    m_vnc_current_dir_name_len = first->vnc_file_name_len;
                    return NextDirectoryResult::Ok;

                case FT_WriteErrorCode::TooLargeDataLength:
                    assert(false);
                case FT_WriteErrorCode::TooSmallBuffer:
                    break;
            }
            return NextDirectoryResult::TooSmallBuffer;
        }
    }

    return NextDirectoryResult::NoDir;
}

VNC::VncFileList::PartialRdpFileListResult
VNC::VncFileList::write_partial_rdp_file_list(OutStream & out_stream) noexcept
{
    auto channel_flags = m_rdp_channel_flags;
    uint32_t total_len = 0;

    /*
     * Write header
     */
    if (!m_rdp_file_position)
    {
        auto header = make_file_list_response_with_header_without_data(
            checked_int{ m_internal_files.size() }
        );

        if (!header.write(out_stream))
        {
            return {
                PartialRdpFileListResult::Status::TooSmallBuffer,
                CompressedChannelFlags::NoFlags,
                total_len,
            };
        }

        total_len = header.total_len();
    }

    /*
     * Write files
     */
    for (
      ; m_rdp_file_position < m_internal_files.size()
     && out_stream.has_room(FileDescriptor::pdu_len())
      ; ++m_rdp_file_position)
    {
        auto & file = m_internal_files[m_rdp_file_position];

        FileDescriptorFileSize file_size { file.file_size };
        FileDescriptorWithoutFileName {
            .flags = FileDescriptorFlags::FileSize
                | FileDescriptorFlags::ShowProgressUI
                | FileDescriptorFlags::Attributes
                | (bool(file.last_access_time)
                        ? FileDescriptorFlags::WriteTime
                        : FileDescriptorFlags{}),
            .fileAttributes = file.is_dir
                ? WinNtFileAttributeFlags::Directory
                : WinNtFileAttributeFlags::Archive,
            .lastWriteTime = file.last_access_time,
            .fileSizeHigh = file_size.high,
            .fileSizeLow = file_size.low,
        }
        .write_unchecked(out_stream);

        constexpr uint32_t null_terminal_len = 2;

        constexpr uint16_t max_file_name {
            file_descriptor_file_name_buffer_size - null_terminal_len
        };

        // vnc path without dir separator
        bytes_view path {
            file.vnc_file_name,
            checked_int { file.vnc_file_name_len - file.is_dir },
        };
        // nor current directory
        path = path.drop_front(m_dir_base_len);

        auto utf16_path_buffer = out_stream.get_tail().first(max_file_name);
        auto len = cp1252_to_utf16le.partial(path, utf16_path_buffer).out.size();

        out_stream.out_skip_bytes(checked_int{len});
        out_stream.out_clear_bytes(max_file_name - len + null_terminal_len);
    }

    /*
     * Deal with channel flags
     */

    bool again = (m_rdp_file_position < m_internal_files.size());

    channel_flags |= again
        ? VNC::CompressedChannelFlags::NoFlags
        : VNC::CompressedChannelFlags::Last;

    m_rdp_channel_flags = VNC::CompressedChannelFlags::ShowProtocol;

    return {
        again
            ? PartialRdpFileListResult::Status::Partial
            : PartialRdpFileListResult::Status::Completed,
        channel_flags,
        total_len,
    };
}

VNC::VncFileList::TransferResult
VNC::VncFileList::rdp_requested_file(FileContentsRequest const & req) noexcept
{
    CbStreamId resp_stream_id1 {};
    CbMsgFlags resp_msg_flags2;
    uint64_t resp_file_size2 = 0;
    uint32_t rdp_msg_flags = 0;
    using ResponseType = TransferResult::ResponseType;
    ResponseType response_types[] {
        ResponseType::Nothing,
        ResponseType::Nothing,
    };
    auto * response_type_it = response_types;

    auto set_msg2 = [&](CbMsgFlags flag, uint64_t file_size) {
        rdp_msg_flags |= D::RdpMsg_Response;
        resp_file_size2 = file_size;
        resp_msg_flags2 = flag;
    };

    auto const lindex = underlying_cast(req.lindex);

    OutStream vnc_stream = D::vnc_stream(*this);

    /*
     * Manage previous request when available (abnormal)
     */

    // request before response to previous request -> abort the previous request
    if (m_rdp_req_lindex != D::INVALID_LINDEX) [[unlikely]]
    {
        // response to previous request
        if (m_rdp_req_stream_id != req.streamId)
        {
            rdp_msg_flags |= D::RdpMsg_PreviousResponseData;
            resp_stream_id1 = m_rdp_req_stream_id;
            *response_type_it++ = ResponseType::RdpResponseFailure;
        }

        m_rdp_req_lindex = D::INVALID_LINDEX;
        m_rdp_req_continuation_lindex = D::INVALID_LINDEX;

        // send abort
        switch (m_transfer_status)
        {
            case TransferStatus::Nothing:
            case TransferStatus::WaitAcceptAndReject:
            case TransferStatus::WaitAbortResponse:

            case TransferStatus::TransferInProgress:
                vnc_stream.out_copy_bytes(D::vnc_abort_file);
                m_transfer_status = TransferStatus::WaitAbortResponse;
                break;

            case TransferStatus::WaitAcceptAndContinue:
                m_transfer_status = TransferStatus::WaitAcceptAndReject;
                break;

            case TransferStatus::Finished:
                m_transfer_status = TransferStatus::Nothing;
                break;
        }
    }

    auto const is_invalid = lindex >= m_internal_files.size()
                         || m_internal_files[lindex].is_dir;
    auto const new_file = (m_rdp_req_continuation_lindex != req.lindex);
    auto const is_size_request = (req.dwFlags == CbFileContentsType::Size);
    auto const position = req.position();

    /*
     * Manage request
     */

    if (is_invalid)
    {
        *response_type_it++ = ResponseType::InvalidLindex;
        set_msg2(CbMsgFlags::ResponseFail, 0);
    }
    else
    {
        auto & file = m_internal_files[lindex];

        // request file size
        if (is_size_request)
        {
            set_msg2(CbMsgFlags::ResponseOk, file.file_size);
            *response_type_it++ = ResponseType::RdpResponseSize;
        }
        // request new file
        else if (new_file || !position)
        {
            m_rdp_req_continuation_lindex = D::INVALID_LINDEX;

            // unsequenced read, send error
            if (position) [[unlikely]]
            {
                set_msg2(CbMsgFlags::ResponseFail, 0);
                *response_type_it++ = ResponseType::RdpResponseUnsequenced;
            }
            // empty file, send empty data
            else if (file.file_size == 0)
            {
                set_msg2(CbMsgFlags::ResponseOk, 0);
                *response_type_it++ = ResponseType::RdpResponseData;
            }
            // init new file and request
            else
            {
                m_rdp_req_lindex = req.lindex;
                m_rdp_req_stream_id = req.streamId;
                m_rdp_req_requested = req.cbRequested;
                m_rdp_req_position = position;

                m_rdp_req_continuation_lindex = m_rdp_req_lindex;
                m_lindex_file_size = file.file_size;

                m_transfer_data_offset = D::rdp_file_data_offset;
                m_transfer_buffer.resize(m_transfer_data_offset);

                switch (m_transfer_status)
                {
                    case TransferStatus::Nothing: {
                        [[maybe_unused]]
                        auto ec = FT::write_uncompressed_file_request(vnc_stream, file.name());
                        assert(is_ok(ec));

                        m_transfer_status = TransferStatus::WaitAcceptAndContinue;
                        *response_type_it++ = ResponseType::VncRequestFile;
                        break;
                    }

                    case TransferStatus::WaitAcceptAndReject:
                    case TransferStatus::WaitAbortResponse:
                        break;

                    case TransferStatus::WaitAcceptAndContinue:
                    case TransferStatus::TransferInProgress:
                    case TransferStatus::Finished:
                        assert(false && "should be reset in first step");
                }
            }
        }
        // request continuation data
        else
        {
            assert(m_rdp_req_continuation_lindex == req.lindex);

            // unsequenced read, send error
            if (position != m_rdp_req_position + m_rdp_req_requested)
            {
                m_rdp_req_lindex = D::INVALID_LINDEX;
                m_rdp_req_continuation_lindex = D::INVALID_LINDEX;
                set_msg2(CbMsgFlags::ResponseFail, 0);
                *response_type_it++ = ResponseType::RdpResponseUnsequenced;
            }
            // process file data
            else
            {
                m_rdp_req_stream_id = req.streamId;
                m_rdp_req_requested = req.cbRequested;
                m_rdp_req_position = position;

                switch (m_transfer_status)
                {
                    case TransferStatus::TransferInProgress:
                        // has sufficient data
                        if (D::has_sufficient_data(*this))
                        {
                            set_msg2(CbMsgFlags::ResponseOk, 0);
                            rdp_msg_flags |= D::RdpMsg_WithData;
                            *response_type_it++ = ResponseType::RdpResponseData;
                        }
                        break;

                    case TransferStatus::WaitAbortResponse:  // file truncated
                    case TransferStatus::Finished:
                    case TransferStatus::Nothing:  // end of file, but no eof by server
                        set_msg2(CbMsgFlags::ResponseOk, 0);
                        rdp_msg_flags |= D::RdpMsg_WithData;
                        *response_type_it++ = ResponseType::RdpResponseData;
                        m_transfer_status = TransferStatus::Nothing;
                        m_rdp_req_lindex = D::INVALID_LINDEX;
                        m_rdp_req_continuation_lindex = D::INVALID_LINDEX;
                        break;

                    case TransferStatus::WaitAcceptAndReject:
                    case TransferStatus::WaitAcceptAndContinue:
                        assert(false);
                        break;
                }
            }
        }
    }

    bytes_view rdp_data;

    if (rdp_msg_flags)
    {
        OutStream rdp_stream = D::rdp_stream(*this, rdp_msg_flags, is_size_request);

        if (rdp_msg_flags & D::RdpMsg_PreviousResponseData)
        {
            unchecked_write_cb_packet_with_header(
                rdp_stream,
                CbMsgFlags::ResponseFail,
                FileContentsResponseWithoutData { resp_stream_id1 }
            );
        }

        if (rdp_msg_flags & D::RdpMsg_Response)
        {
            // size
            if (is_size_request)
            {
                unchecked_write_cb_packet_with_header(
                    rdp_stream,
                    resp_msg_flags2,
                    FileContentsResponseSize { req.streamId, resp_file_size2 }
                );
            }
            // range with data
            else if (rdp_msg_flags & D::RdpMsg_WithData)
            {
                D::write_rdp_response_range_and_consume_data(*this, rdp_stream);
            }
            // range with error
            else
            {
                unchecked_write_cb_packet_with_header(
                    rdp_stream,
                    resp_msg_flags2,
                    FileContentsResponseWithoutData { req.streamId }
                );
            }
        }

        rdp_data = rdp_stream.get_produced_bytes();
    }

    return {
        .rdp_data = rdp_data,
        .vnc_data = vnc_stream.get_produced_bytes(),
        .response_types {
            response_types[0],
            response_types[1],
        },
    };
}

VNC::VncFileList::TransferResult
VNC::VncFileList::receive_vnc_file_request_response(bool ok) noexcept
{
    switch (m_transfer_status)
    {
        case TransferStatus::WaitAcceptAndReject: {
            using ResponseType = TransferResult::ResponseType;
            ResponseType response_type2 = ResponseType::Nothing;

            OutStream vnc_stream = D::vnc_stream(*this);
            vnc_stream.out_copy_bytes(D::vnc_confirm_reject);

            if (D::write_vnc_file_request(*this, vnc_stream))
            {
                m_transfer_status = TransferStatus::WaitAcceptAndContinue;
                response_type2 = ResponseType::VncRequestFile;
            }
            else
            {
                m_transfer_status = TransferStatus::Nothing;
            }

            return {
                .rdp_data = {},
                .vnc_data = vnc_stream.get_produced_bytes(),
                .response_types {
                    ResponseType::VncAbortFile,
                    response_type2,
                },
            };
        }

        case TransferStatus::WaitAcceptAndContinue: {
            using ResponseType = TransferResult::ResponseType;
            ResponseType response_type = ResponseType::VncConfirmFile;

            bytes_view vnc_data;
            bytes_view rdp_data;

            if (ok)
            {
                vnc_data = D::vnc_confirm_accept;
                m_transfer_status = TransferStatus::TransferInProgress;
            }
            else
            {
                rdp_data = D::set_rdp_response_fail(*this);
                m_transfer_status = TransferStatus::Nothing;
                response_type = ResponseType::RdpResponseFailure;
            }

            return {
                .rdp_data = rdp_data,
                .vnc_data = vnc_data,
                .response_types {
                    response_type,
                },
            };
        }

        case TransferStatus::Finished:
            assert(false);
        case TransferStatus::Nothing:
        case TransferStatus::WaitAbortResponse:
        case TransferStatus::TransferInProgress:
            break;
    }

    return {};
}

VNC::VncFileList::TransferResult
VNC::VncFileList::receive_vnc_file_data(bytes_view data) noexcept
{
    TransferResult result {};

    switch (m_transfer_status)
    {
        case TransferStatus::TransferInProgress: {
            auto current_data_len = m_transfer_buffer.size() - D::rdp_file_data_offset;
            bool is_greater_that_file_size = current_data_len + data.size() > m_lindex_file_size;

            // truncate data
            if (is_greater_that_file_size)
            {
                auto remaining_len = m_lindex_file_size - current_data_len;
                data = data.first(mmin(remaining_len, data.size()));
            }

            m_transfer_buffer.insert(m_transfer_buffer.end(), data.begin(), data.end());

            // send data to rdp and abort to vnc
            if (is_greater_that_file_size)
            {
                m_rdp_req_lindex = D::INVALID_LINDEX;
                m_transfer_status = TransferStatus::WaitAbortResponse;
                result = D::write_rdp_response_data(*this);
                result.vnc_data = D::vnc_abort_file;
                result.response_types[1] = TransferResult::ResponseType::VncAbortFile;
            }
            // send data when sufficient
            else if (D::has_sufficient_data(*this))
            {
                m_rdp_req_lindex = D::INVALID_LINDEX;
                m_transfer_status = TransferStatus::Nothing;
                result = D::write_rdp_response_data(*this);
            }

            break;
        }

        case TransferStatus::Finished:
            assert(false);
        case TransferStatus::Nothing:
        case TransferStatus::WaitAbortResponse:
        case TransferStatus::WaitAcceptAndReject:
        case TransferStatus::WaitAcceptAndContinue:
            break;
    }

    return result;
}

VNC::VncFileList::TransferResult
VNC::VncFileList::receive_vnc_end_of_file() noexcept
{
    switch (m_transfer_status)
    {
        case TransferStatus::TransferInProgress: {
            assert(m_rdp_req_lindex != D::INVALID_LINDEX);

            auto result = D::write_rdp_response_data(*this);

            m_rdp_req_lindex = D::INVALID_LINDEX;
            m_transfer_status = (m_transfer_data_offset == m_transfer_buffer.size())
                ? TransferStatus::Nothing
                : TransferStatus::Finished;

            return result;
        }

        // cross message (abort(client) + eof(server))
        case TransferStatus::WaitAbortResponse: {
            OutStream vnc_stream { m_transfer_buffer };
            auto response_type = TransferResult::ResponseType::Nothing;

            m_transfer_status = TransferStatus::Nothing;

            if (D::write_vnc_file_request(*this, vnc_stream))
            {
                m_transfer_status = TransferStatus::WaitAcceptAndContinue;
                response_type = TransferResult::ResponseType::VncRequestFile;
            }

            return {
                .rdp_data = {},
                .vnc_data = vnc_stream.get_produced_bytes(),
                .response_types {
                    response_type,
                    TransferResult::ResponseType::Nothing,
                },
            };
        }

        case TransferStatus::Finished:
        case TransferStatus::WaitAcceptAndReject:
        case TransferStatus::WaitAcceptAndContinue:
            assert(false);
        case TransferStatus::Nothing:
            break;
    }

    return {};
}

VNC::VncFileList::TransferResult
VNC::VncFileList::receive_vnc_file_abort() noexcept
{
    switch (m_transfer_status)
    {
        case TransferStatus::WaitAbortResponse:
            return receive_vnc_end_of_file();

        case TransferStatus::Finished:
        case TransferStatus::WaitAcceptAndReject:
        case TransferStatus::WaitAcceptAndContinue:
            assert(false);

        // read error on server side
        case TransferStatus::TransferInProgress: {
            m_transfer_status = TransferStatus::Nothing;
            return {
                .rdp_data = D::set_rdp_response_fail(*this),
                .vnc_data = {},
                .response_types {
                    TransferResult::ResponseType::RdpResponseFailure,
                },
            };
        }

        case TransferStatus::Nothing:
            break;
    }

    return {};
}

VNC::VncFileList::TransferResult
VNC::VncFileList::stop_file_transfer() noexcept
{
    bytes_view vnc_data {};

    switch (m_transfer_status)
    {
        case TransferStatus::TransferInProgress:
            vnc_data = D::vnc_abort_file;
            m_transfer_status = TransferStatus::WaitAbortResponse;
            break;

        case TransferStatus::WaitAcceptAndContinue:
            m_transfer_status = TransferStatus::WaitAcceptAndReject;
            break;

        case TransferStatus::Finished:
            m_transfer_status = TransferStatus::Nothing;
            break;

        case TransferStatus::Nothing:
        case TransferStatus::WaitAbortResponse:
        case TransferStatus::WaitAcceptAndReject:
            return {};
    }

    assert(m_rdp_req_lindex != D::INVALID_LINDEX);

    auto rdp_data = D::set_rdp_response_fail(*this);

    return {
        .rdp_data = rdp_data,
        .vnc_data = vnc_data,
        .response_types {
            TransferResult::ResponseType::RdpResponseFailure,
            vnc_data.empty()
                ? TransferResult::ResponseType::Nothing
                : TransferResult::ResponseType::VncAbortFile,
        },
    };
}
