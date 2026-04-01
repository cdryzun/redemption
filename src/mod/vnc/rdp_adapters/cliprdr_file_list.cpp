/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "mod/vnc/rdp_adapters/cliprdr_file_list.hpp"
#include "mod/vnc/encoders/uvnc_file_transfer.hpp"
#include "mod/vnc/rdp_adapters/rdpeclip.hpp"
#include "utils/allocate_sequence.hpp"


enum class VNC::CliprdrFileList::State : uint8_t
{
    NoTransfer,
    // to vnc
    CreateDirOrFileOffer,
    // to rdp
    FileRequest,
    // to rdp
    OnError,
};

struct VNC::CliprdrFileList::D
{
    // TODO utility + duplicate selector.cpp
    template<class T>
    static writable_array_view<T>
    allocate_view(std::pmr::monotonic_buffer_resource & mbr, std::size_t n)
    {
        auto * p = static_cast<T*>(mbr.allocate(n * sizeof(T), alignof(T)));
        return writable_array_view{p, n};
    }

    // TODO utility (conflict with other allocate_sequence)
    template<class T>
    static T*
    allocate_sequence(OutParam<T*> ptr, std::pmr::monotonic_buffer_resource & mbr, std::size_t n)
    {
        return ptr.out_value = allocate_view<T>(mbr, n).data();
    }
};


WinNtPathView VNC::CliprdrFileList::File::name() const noexcept
{
    return WinNtPathView::assumed(file_name, file_name_len);
}


VNC::CliprdrFileList::CliprdrFileList(uint32_t max_nb_file) noexcept
    : m_max_nb_file{max_nb_file}
{}

VNC::CliprdrFileList::~CliprdrFileList() = default;

bool VNC::CliprdrFileList::start_new_list(WinNtUTime default_time, uint32_t total_nb_file)
{
    if (m_memory_used > 1024 * 1024) // 1MB
    {
        m_allocated_nb_file = 0;
        m_remaining_char = 0;
        m_memory_used = 0;
        m_chars = nullptr;
        m_mbr.release();
    }

    m_state = State::NoTransfer;
    m_process_file_data = false;
    m_lindex = 0;
    m_file_offset = 0;
    m_real_file_size = 0;
    m_end_processes_lindex = 0;
    m_nb_files = 0;
    m_capacity_nb_file = 0;
    m_total_file_size = 0;

    m_default_time = default_time;

    if (total_nb_file > m_max_nb_file)
    {
        return false;
    }

    m_capacity_nb_file = total_nb_file;

    if (m_allocated_nb_file < m_capacity_nb_file) [[unlikely]]
    {
        D::allocate_sequence(OutParam{m_file_infos}, m_mbr, m_capacity_nb_file);
        m_allocated_nb_file = m_capacity_nb_file;
    }

    return true;
}

void VNC::CliprdrFileList::reset_transfer() noexcept
{
    m_process_file_data = false;
    m_lindex = 0;
    m_file_offset = 0;
    m_real_file_size = 0;
    m_end_processes_lindex = 0;
}

bool VNC::CliprdrFileList::is_full() const noexcept
{
    return m_nb_files == m_capacity_nb_file;
}

VNC::CliprdrFileList::AddFileResult
VNC::CliprdrFileList::add_file(FileDescriptor const& fd)
{
    if (is_full())
    {
        return {0, AddFileErrorCode::Full};
    }

    auto output_fname_size = fd.unicodeFileName.size() / utf16le_to_cp1252.output_buffer_divisor;
    // insuffisant memory, allocate new block
    if (output_fname_size > m_remaining_char) [[unlikely]]
    {
        constexpr auto chunk_size = static_cast<uint16_t>(~uint16_t{});
        static_assert(decltype(fd.unicodeFileName)::msize_at_least < chunk_size);

        m_remaining_char = chunk_size;
        m_chars = static_cast<uint8_t*>(m_mbr.allocate(m_remaining_char, 1));
        m_memory_used += m_remaining_char;
    }

    auto cp1252_data = m_chars;
    auto result = utf16le_to_cp1252.unchecked(fd.unicodeFileName, cp1252_data);
    auto cp1252_len = result.out - cp1252_data;

    if (!result)
    {
        return {checked_int{cp1252_len * 2}, AddFileErrorCode::DecodeError};
    }

    m_chars = result.out;
    m_remaining_char -= cp1252_len;

    bool is_dir = flags_any(fd.flags, FileDescriptorFlags::Attributes)
               && flags_any(fd.fileAttributes, WinNtFileAttributeFlags::Directory);
    bool has_file_size = flags_any(fd.flags, FileDescriptorFlags::FileSize);

    m_file_infos[m_nb_files] = File{
        // no size -> will be consumed until end of file event
        .file_size = is_dir ? 0 : has_file_size ? fd.file_size() : ~uint64_t{},
        .last_write_time = flags_any(fd.flags, FileDescriptorFlags::WriteTime)
            ? fd.lastWriteTime
            : m_default_time,
        .file_name = cp1252_data,
        .file_name_len = checked_int{cp1252_len},
        .has_file_size = has_file_size || is_dir,
        .is_directory = is_dir,
    };
    ++m_nb_files;

    m_total_file_size += (is_dir || !has_file_size) ? 0 : fd.file_size();

    return {0, AddFileErrorCode::Ok};
}

array_view<VNC::CliprdrFileList::File> VNC::CliprdrFileList::files() const noexcept
{
    return {m_file_infos, m_nb_files};
}

uint32_t VNC::CliprdrFileList::nb_files() const noexcept
{
    return m_nb_files;
}

VNC::CliprdrFileList::File const *
VNC::CliprdrFileList::get_file(uint32_t idx) const noexcept
{
    return idx < m_nb_files ? &m_file_infos[idx] : nullptr;
}

VNC::CliprdrFileList::File const *
VNC::CliprdrFileList::get_current_file() const noexcept
{
    return get_file(m_lindex);
}

VNC::CliprdrFileList::File const &
VNC::CliprdrFileList::get_current_file_unchecked() const noexcept
{
    assert(m_lindex < m_nb_files);
    assert(m_lindex < m_end_processes_lindex);
    return m_file_infos[m_lindex];
}

VNC::CliprdrFileList::ErrorCode
VNC::CliprdrFileList::write_uvnc_items_to_vnc(
    OutStream & out_stream, WinNtPathView current_dir) noexcept
{
    namespace FT = UVNC::FileTransfer;

    m_file_offset = 0;
    m_end_processes_lindex = m_lindex;

    m_state = State::NoTransfer;

    while (m_end_processes_lindex < m_nb_files)
    {
        auto & file = m_file_infos[m_end_processes_lindex];
        auto path = file.name();

        auto ec = FT::WriteErrorCode::TooLargeDataLength;

        // CbCapabilityFlags::FileClipNoFilePaths is ignored

        if (file.is_directory)
        {
            ec = FT::write_command_create_directory2(out_stream, current_dir.native(), path.native());
        }
        // if file
        else
        {
            ec = FT::write_file_transfer_offer2(
                out_stream,
                current_dir.native(),
                path.native(),
                file.file_size,
                to_win_nt_time(file.last_write_time)
            );
        }

        switch (ec)
        {
            case FT::WriteErrorCode::NoError:
                ++m_end_processes_lindex;
                m_state = State::CreateDirOrFileOffer;
                if (file.is_directory)
                {
                    continue;
                }
                break;

            // when many directory
            case FT::WriteErrorCode::TooSmallBuffer:
                return (m_end_processes_lindex != m_nb_files)
                    ? ErrorCode::Ok
                    : ErrorCode::TooSmallBuffer;

            // current_dir + path are too long
            case FT::WriteErrorCode::TooLargeDataLength:
                m_state = State::OnError;
                return ErrorCode::PathTooLong;
        }

        return ErrorCode::Ok;
    }

    return ErrorCode::Ok;
}

VNC::CliprdrFileList::ErrorCode
VNC::CliprdrFileList::write_cb_file_range_request(OutStream & out_stream) noexcept
{
    // next id (O -> 1 -> 0 -> 1)
    m_stream_id ^= 1u;

    bool ok = write_cb_packet_with_header(out_stream, VNC::FileContentsRequest {
        .streamId = VNC::CbStreamId{ m_stream_id },
        .lindex = VNC::CbLindex{ m_lindex },
        .dwFlags = VNC::CbFileContentsType::Range,
        .nPositionLow = checked_int{ m_file_offset & 0xffff'ffffu },
        .nPositionHigh = checked_int{ m_file_offset >> 32 },
        .cbRequested = requested_nb_bytes,
        .clipDataId = VNC::ClipDataId{},
    });

    if (ok)
    {
        m_state = State::FileRequest;
        return ErrorCode::Ok;
    }

    // restore id
    m_stream_id ^= 1u;
    m_state = State::OnError;
    return ErrorCode::TooSmallBuffer;
}

VNC::CliprdrFileList::ReceiveStatus
VNC::CliprdrFileList::receive_uvnc_create_dir_response() noexcept
{
    if (m_state == State::CreateDirOrFileOffer && m_lindex < m_end_processes_lindex)
    {
        auto & file = m_file_infos[m_lindex];

        if (!file.is_directory)
        {
            return ReceiveStatus::Error;
        }

        ++m_lindex;

        return m_lindex < m_end_processes_lindex
            ? ReceiveStatus::WaitingResponse
            : m_lindex < m_nb_files
            ? ReceiveStatus::ReadyForNextItems
            : ReceiveStatus::TransferComplete;
    }

    return ReceiveStatus::Error;
}

bool VNC::CliprdrFileList::receive_uvnc_file_accept_response() noexcept
{
    if (m_state == State::CreateDirOrFileOffer && m_lindex < m_end_processes_lindex)
    {
        auto & file = m_file_infos[m_lindex];

        if (file.is_directory || m_process_file_data)
        {
            return false;
        }

        m_process_file_data = true;

        return true;
    }

    return false;
}

VNC::CliprdrFileList::ReceiveCbFileContentsResponseResult
VNC::CliprdrFileList::receive_cb_file_contents_response(
    bytes_view data, uint32_t remaining_len, bool is_ok, ChannelFlags channel_flags) noexcept
{
    if (m_state != State::FileRequest || m_lindex >= m_nb_files)
    {
        return {false, false, m_lindex, 0, data};
    }

    if (!is_ok)
    {
        m_state = State::OnError;
    }
    else if (flags_any(channel_flags, VNC::ChannelFlags::First))
    {
        VNC::FileContentsResponseWithoutData resp;
        InStream in_stream{data};
        if (!resp.read(in_stream))
        {
            m_state = State::OnError;
        }
        else if (resp.streamId != VNC::CbStreamId{m_stream_id})
        {
            m_state = State::OnError;
        }
        else
        {
            data = in_stream.remaining_bytes();
            auto total_len = remaining_len + data.size();

            m_real_file_size += total_len;

            if (total_len != requested_nb_bytes)
            {
                // no more data
                m_file_offset = ~uint64_t{};
            }
            else
            {
                m_file_offset += total_len;
            }
        }
    }

    bool file_is_complete = false;
    bool ok = (m_state == State::FileRequest);
    int64_t delta_file_size = 0;

    if (flags_any(channel_flags, VNC::ChannelFlags::Last))
    {
        auto file_size = m_file_infos[m_lindex].file_size;
        delta_file_size = static_cast<int64_t>(m_real_file_size - file_size);
        file_is_complete = m_file_offset >= file_size;
        m_state = State::NoTransfer;
        m_real_file_size = 0;
        m_lindex += file_is_complete;
        m_process_file_data = !file_is_complete;
    }

    return {ok, file_is_complete, m_lindex - file_is_complete, delta_file_size, data};
}

void VNC::CliprdrFileList::next_file() noexcept
{
    assert(m_process_file_data);

    m_process_file_data = false;
    m_state = State::NoTransfer;
    m_real_file_size = 0;
    ++m_lindex;
}
