/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "utils/sugar/bytes_view.hpp"
#include "utils/is_ok.hpp"
#include "mod/vnc/file_transfer/vnc_file.hpp"
#include "mod/vnc/rdp_adapters/rdp_channel.hpp"
#include "mod/vnc/rdp_adapters/rdpeclip.hpp"

#include <vector>
#include <memory_resource>


class OutStream;

namespace VNC
{

struct FileDescriptor;

/// Build PDU exchanged between VNC and RDP for downloading VNC files
/// (file list, file content and directory request).
class VncFileList
{
public:
    enum class [[nodiscard]] NextDirectoryResult : uint8_t
    {
        Ok,
        NoDir,
        TooSmallBuffer,
    };

    enum class [[nodiscard]] PushFileResult : uint8_t
    {
        Ok,
        FinalPathTooLarge,
        FileSizeTooLarge,
        TooManyFiles,
    };

    struct [[nodiscard]] PartialRdpFileListResult
    {
        enum class [[nodiscard]] Status : uint8_t
        {
            Partial,
            Completed,
            TooSmallBuffer,
        };

        Status status;
        CompressedChannelFlags compressed_channel_flags;
        // 0 when channel_flags do not contains First
        uint32_t total_len;

        ChannelFlags channel_flags() const noexcept
        {
            return uncompress_channel_flags(compressed_channel_flags);
        }
    };

    struct [[nodiscard]] TransferResult
    {
        enum class [[nodiscard]] ResponseType : uint8_t
        {
            Nothing,
            InvalidLindex,
            RdpResponseUnsequenced,
            RdpResponseFailure,
            RdpResponseSize,
            RdpResponseData,
            VncRequestFile,
            VncConfirmFile,
            VncAbortFile,
        };

        bytes_view rdp_data;
        bytes_view vnc_data;
        // for log
        ResponseType response_types[2];
    };

    struct File
    {
        uint8_t const * vnc_file_name;
        uint64_t file_size;
        WinNtUTime last_access_time;
        uint16_t vnc_file_name_len;
        bool is_dir;

        UVncFile::PathView name() const noexcept;
    };


public:
    struct Params
    {
        uint32_t max_nb_files;
        uint64_t max_file_size;
    };

    VncFileList(Params params) noexcept;

    ~VncFileList();

    File const * get_current_file() const noexcept;

    /// New list from \c dir_base directory.
    /// RDP file list do not contains this directory part.
    TransferResult start_new_list(UVncFile::PathView dir_base) noexcept;

    /// Push a file from the latest directory processed by
    /// \c write_next_vnc_directory_content_request().
    /// \return \c false when file list is full, otherwise \c true.
    PushFileResult push_file_in_current_dir(UVncFile const & file);

    /// Build a VNC directory content request from the directories
    /// inserted by \c add_item_in_current_dir.
    /// \return \c NoDir when all directories are consumed.
    NextDirectoryResult write_next_vnc_directory_content_request(OutStream & out_stream) noexcept;


    /// Reset the RDP file list data response state.
    TransferResult start_rdp_file_list() noexcept;

    /// Build a RDP file list data response.
    PartialRdpFileListResult write_partial_rdp_file_list(OutStream & out_stream) noexcept;

    /// Transfer functions.
    //@{
    TransferResult rdp_requested_file(FileContentsRequest const & req) noexcept;

    TransferResult receive_vnc_file_request_response(bool ok) noexcept;
    TransferResult receive_vnc_file_data(bytes_view data) noexcept;
    TransferResult receive_vnc_end_of_file() noexcept;
    TransferResult receive_vnc_file_abort() noexcept;

    TransferResult stop_file_transfer() noexcept;
    //@}

private:
    struct D;
    friend D;

    enum TransferStatus : uint8_t;

    uint64_t m_max_file_size;
    uint32_t m_max_nb_files;

    // vnc part
    //@{
    uint32_t m_vnc_dir_position = 0;
    uint8_t const * m_vnc_current_dir_name = nullptr;
    uint16_t m_vnc_current_dir_name_len = 0;
    //@}

    // rdp part
    //@{
    uint16_t m_dir_base_len = 0;
    uint32_t m_rdp_file_position = 0;
    CompressedChannelFlags m_rdp_channel_flags {};
    //@}

    // transfer
    //@{
    TransferStatus m_transfer_status;

    CbLindex m_rdp_req_continuation_lindex;
    CbLindex m_rdp_req_lindex;
    CbStreamId m_rdp_req_stream_id;
    uint32_t m_rdp_req_requested;
    uint64_t m_rdp_req_position;

    uint64_t m_lindex_file_size;

    uint64_t m_transfer_data_offset;
    // TODO replace that
    std::vector<uint8_t> m_transfer_buffer;
    //@}

    std::vector<File> m_internal_files;
    // TODO used a more specialized allocator (scrash allocator)
    std::pmr::monotonic_buffer_resource m_mbr_files {};
};

} // namespace VNC


template<> inline constexpr auto is_ok_v<VNC::VncFileList::NextDirectoryResult>
    = VNC::VncFileList::NextDirectoryResult::Ok;

template<> inline constexpr auto is_ok_v<VNC::VncFileList::PushFileResult>
    = VNC::VncFileList::PushFileResult::Ok;
