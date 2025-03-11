/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <memory_resource>

#include "core/WinNT/time.hpp"
#include "core/WinNT/path.hpp"
#include "utils/is_ok.hpp"
#include "utils/sugar/bytes_view.hpp"
#include "mod/vnc/rdp_adapters/rdp_channel.hpp"


class OutStream;

namespace VNC
{

struct FileDescriptor;

/// Exchanged between RDP and VNC for downloading RDP files.
class CliprdrFileList
{
public:
    static const uint32_t requested_nb_bytes = 256 * 1024;

    struct File
    {
        uint64_t file_size;
        WinNtUTime last_write_time;
        uint8_t const * file_name;
        uint16_t file_name_len;
        bool has_file_size;
        bool is_directory;

        WinNtPathView name() const noexcept;
    };

    CliprdrFileList(uint32_t max_nb_file) noexcept;

    ~CliprdrFileList();

    /// Mark building list as incomplete.
    /// \param default_time time used when \c FileDescriptor::lastWriteTime
    ///     is invalid in \c add_file().
    /// \return \c false when \p total_nb_file is greater of \c max_nb_file supported.
    [[nodiscard]]
    bool start_new_list(WinNtUTime default_time, uint32_t total_nb_file);

    void reset_transfer() noexcept;

    enum class [[nodiscard]] AddFileErrorCode : uint8_t
    {
        Ok,
        Full,
        DecodeError,
    };

    struct [[nodiscard]] AddFileResult
    {
        uint16_t decode_error_position;
        AddFileErrorCode ec;

        explicit operator bool () const noexcept
        {
            return ec == AddFileErrorCode::Ok;
        }

        bool operator == (AddFileResult const &) const = default;
    };

    /// Push a file.
    /// If fd.lastWriteTime is invalid, \c default_time of \c start_new_list() is used.
    /// If fd.fileAttributes is invalid, \c fd is assumed to be a file.
    AddFileResult add_file(FileDescriptor const& fd);

    /// \return true when list length is equal to \c max_nb_file.
    bool is_full() const noexcept;

    uint64_t get_total_file_size() const noexcept { return m_total_file_size; }

    bool is_transfer_complete() const noexcept { return m_lindex >= m_nb_files; }
    bool is_waiting_response() const noexcept { return m_lindex < m_end_processes_lindex; }

    array_view<File> files() const noexcept;
    uint32_t nb_files() const noexcept;

    File const * get_file(uint32_t idx) const noexcept;
    File const * get_current_file() const noexcept;
    File const & get_current_file_unchecked() const noexcept;

    enum class [[nodiscard]] ErrorCode : uint8_t
    {
        Ok,
        TooSmallBuffer,
        PathTooLong,
    };

    // write optional many create dir with zero or one file transfer offer
    ErrorCode write_uvnc_items_to_vnc(OutStream & out_stream, WinNtPathView current_dir) noexcept;

    ErrorCode write_cb_file_range_request(OutStream & out_stream) noexcept;

    enum class [[nodiscard]] ReceiveStatus : uint8_t
    {
        // all items are transferred
        TransferComplete,
        // current item has an error
        Error,
        // wait response for create dir or file accept
        WaitingResponse,
        // wait next group (require a call of write_uvnc_items_to_vnc())
        ReadyForNextItems,
    };

    ReceiveStatus receive_uvnc_create_dir_response() noexcept;
    /// When true, wait data for file item (require a call of \b skip_file()
    /// or \b receive_cb_file_contents_response())
    bool receive_uvnc_file_accept_response() noexcept;

    void next_file() noexcept;

    struct [[nodiscard]] ReceiveCbFileContentsResponseResult
    {
        bool ok;
        bool file_is_complete;
        uint32_t lindex;
        // diff between file size and data len received
        int64_t delta_file_size;
        bytes_view data;

        explicit operator bool () const noexcept
        {
            return ok;
        }
    };

    ReceiveCbFileContentsResponseResult receive_cb_file_contents_response(
        bytes_view data,
        uint32_t remaining_len,
        bool is_ok,
        ChannelFlags channel_flags
    ) noexcept;

private:
    struct D;
    friend D;

    enum class State : uint8_t;

    State m_state {};
    // wait a call to skip_file() or receive_cb_file_contents_response().file_is_complete
    bool m_process_file_data {};
    uint32_t m_lindex {};
    uint32_t m_end_processes_lindex {};
    uint32_t m_stream_id {};
    uint64_t m_file_offset {};
    uint64_t m_real_file_size {};

    File * m_file_infos {};
    // TODO used a more specialized allocator (scrash allocator)
    std::pmr::monotonic_buffer_resource m_mbr {};
    uint64_t m_memory_used {};
    uint64_t m_total_file_size {};
    // TODO duplicate m_mbr
    uint8_t * m_chars {};
    WinNtUTime m_default_time {};
    uint32_t m_max_nb_file;
    uint32_t m_allocated_nb_file {};
    uint32_t m_capacity_nb_file {};
    uint32_t m_nb_files {};
    uint16_t m_remaining_char {};
};

} // namespace VNC

template<> inline constexpr auto is_ok_v<VNC::CliprdrFileList::ErrorCode>
    = VNC::CliprdrFileList::ErrorCode::Ok;
