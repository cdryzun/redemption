/*
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   Product name: redemption, a FLOSS RDP proxy
   Copyright (C) Wallix 2010
   Author(s): Christophe Grosjean, Javier Caverni, Clément Moroldo, David Fort
   Based on xrdp Copyright (C) Jay Sorg 2004-2010

   Vnc module
*/

#include "mod/vnc/vnc.hpp"
#include "mod/vnc/dsm.hpp"
#include "mod/vnc/newline_convert.hpp"
#include "core/log_id.hpp"
#include "core/log_certificate_status.hpp"
#include "core/RDP/rdp_pointer.hpp"
#include "core/RDP/orders/RDPOrdersPrimaryOpaqueRect.hpp"
#include "core/RDP/clipboard/format_list_serialize.hpp"
#include "core/app_path.hpp"
#include "core/file_validator/file_validator_service.hpp"
#include "core/WinNT/chrono.hpp"
#include "gdi/screen_functions.hpp"
#include "RAIL/client_execute.hpp"
#include "utils/sugar/chars_to_int.hpp"
#include "utils/d3des.hpp"
#include "utils/diffiehellman.hpp"
#include "utils/mathutils.hpp"
#include "utils/hexdump.hpp"
#include "utils/sugar/static_array_to_hexadecimal_chars.hpp"
#include "system/tls_check_certificate.hpp"
#include "translation/trkeys.hpp"
#include "capture/fdx_capture.hpp"

#include <new>

using namespace std::literals::chrono_literals;


template<class PDU>
static inline void log_clipboard_pdu(VNCVerbose verbose, PDU const & pdu)
{
    pdu.log_if(
        flags_any(verbose, VNCVerbose::clipboard),
        "mod_vnc: FileTranfer: ",
        LOG_DEBUG
    );
}


namespace
{
    template<class F>
    auto from_vnc_cb(F) noexcept
    {
        return [](void* ctx, auto... xs) {
            return F{}(*static_cast<mod_vnc*>(ctx), xs...);
        };
    }

    void log_uvnc_buffer_error(
        char const * ctx_msg_error,
        UVNC::FileTransfer::WriteErrorCode ec)
    {
        using ErrorCode = UVNC::FileTransfer::WriteErrorCode;

        char const * err_msg = "";
        #define CASE(name) case ErrorCode::name: err_msg = #name; break
        switch (ec)
        {
            case ErrorCode::NoError: break;
            CASE(TooLargeDataLength);
            CASE(TooSmallBuffer);
        }
        #undef CASE
        LOG(LOG_ERR, "mod_vnc::FileTransfer::%s: error to send file transfer data: %s (%d)",
            ctx_msg_error, err_msg, ec);
    }
}


enum class mod_vnc::FtFlags : uint32_t
{
    FtSupported = 1 << 0,
    FtAccessRequested = 1 << 1,
    FtAccessYes = 1 << 2,
    FtAccessNo = 1 << 3,

    // collect files for GUI
    VncDirContent_ForGui = 1 << 6,
    // collect files for cb buffer transfer
    VncDirContent_ForCbList = 1 << 7,
    // cb buffer transfer collect is completed
    VncDirContent_CbListCompleted = 1 << 8,
    // file list requested while VncDirContent_ForCbList
    VncDirContent_WithCbFileRepDelayed = 1 << 9,
};


enum class mod_vnc::TransferValidatorStatus : uint8_t
{
    WaitResponse,
    IsOk,
    IsRejected,
};


struct mod_vnc::TransferedFileCtx::TflFile final
    : FdxCapture::TflFile
{};


mod_vnc::TransferedFileCtx::Utf8FileName::Utf8FileName(const Utf8FileName& other) noexcept
{
    *this = other;
}

mod_vnc::TransferedFileCtx::Utf8FileName &
mod_vnc::TransferedFileCtx::Utf8FileName::operator=(const Utf8FileName& other) noexcept
{
    m_len = other.m_len;
    memcpy(m_buffer, other.m_buffer, m_len);
    return *this;
}


mod_vnc::TransferedFileCtx::File::File() = default;
mod_vnc::TransferedFileCtx::File::~File() = default;


void mod_vnc::TransferedFileCtx::Utf8FileName::reset(VNC::UVncFile::PathView file_name) noexcept
{
    constexpr auto to_utf8 = cp1252_to_utf8;
    using Buffer = decltype(to_utf8.buffer_from(file_name.native()));

    static_assert(Buffer::view_t::at_most == sizeof(m_buffer));

    auto buffer_view = make_writable_bounded_array_view(m_buffer);
    m_len = to_utf8(file_name.native(), buffer_view).msize();
}


// TODO remove some members
struct mod_vnc::FT
{
    static const uint32_t requested_nb_bytes = 256 * 1024;

    using Utf8FileName = TransferedFileCtx::Utf8FileName;

    static VNC::CliprdrAdapter::Callbacks
    make_cliprdr_adapter_callbacks(mod_vnc & self) noexcept
    {
        return VNC::CliprdrAdapter::Callbacks
        {
            .ctx = &self,
            .send_to_front_channel = from_vnc_cb([](
                mod_vnc & self,
                bytes_view data,
                size_t total_len,
                VNC::ChannelFlags channel_flags
            ){
                self.send_to_cliprdr(data, total_len, channel_flags);
            }),
            .send_to_mod_channel = from_vnc_cb([](mod_vnc & self, bytes_view data){
                self.t.send(data);
            }),
            .receive_partial_file_list = from_vnc_cb([](
                mod_vnc & self,
                bytes_view data,
                writable_sized_bytes_view<VNC::FileDescriptor::pdu_len()> buffer,
                uint16_t buffer_offset,
                VNC::ChannelFlags channel_flags,
                uint32_t total_item
            ){
                if (!self.cliprdr.is_requested_file_list())
                {
                    return buffer_offset;
                }

                if (flags_any(channel_flags, VNC::ChannelFlags::First))
                {
                    auto now = self.events_guard.event_container().get_time_base().real_time;
                    auto clock = clock_cast<WinNtClock>(now);
                    self.cliprdr_file_list.start_new_list(to_win_nt_utime(clock), total_item);
                    // TODO error when ^ returns false
                    self.ft_gui.client_cb_file_list_start(total_item);
                }

                if (!self.cliprdr_file_list.is_full())
                {
                    VNC::CliprdrFileList::AddFileResult add_file_result {};

                    auto parse_result = VNC::FileListParser::parse(
                        data, buffer_offset, buffer,
                        [&](VNC::FileDescriptor const& fd){
                            log_clipboard_pdu(self.verbose, fd);

                            if (FT::is_file_too_large(
                                    self,
                                    FileValidatorTargets::Upload,
                                    fd.file_size()
                            ))
                            {
                                // TODO add message error
                                return false;
                            }

                            // TODO log when false
                            // TODO ignore big fail when huge capability not supported
                            add_file_result = self.cliprdr_file_list.add_file(fd);
                            return bool(add_file_result);
                        }
                    );
                    // TODO check add_file_result
                    // TODO error when false
                    // TODO ignore other data when false
                    parse_result.ok;
                    buffer_offset = parse_result.new_buffer_offset;
                    auto nb_files = self.cliprdr_file_list.nb_files();
                    self.ft_gui.client_cb_file_list_set_nb_item(nb_files);
                }
                else
                {
                    // TODO list is full
                }

                if (flags_any(channel_flags, VNC::ChannelFlags::Last))
                {
                    self.ft_gui.client_cb_file_list_end();
                }

                return buffer_offset;
            }),
            .receive_file_contents_response = from_vnc_cb([](
                mod_vnc & self,
                bytes_view data,
                uint32_t remaining_len,
                bool ok,
                VNC::ChannelFlags channel_flags
            ){
                LOG(LOG_DEBUG, "receive_file_contents_response ok = %d", ok);
                hexdump(data); // TODO LOG_DEBUG

                // TODO send abort when error

                // TODO utility log function
                if (flags_any(self.verbose, VNCVerbose::clipboard)
                 && flags_any(channel_flags, VNC::ChannelFlags::First)
                 && ok) [[unlikely]]
                {
                    VNC::FileContentsResponseWithoutData resp;
                    InStream in_stream{data};
                    if (resp.read(in_stream))
                    {
                        log_clipboard_pdu(self.verbose, resp);
                    }
                    else
                    {
                        LOG(LOG_WARNING, "mod_vnc::receive_file_contents_response: invalide FileContentsResponse");
                    }
                }

                namespace UFT = UVNC::FileTransfer;

                if (auto resp_result = self.cliprdr_file_list.receive_cb_file_contents_response(
                    data, remaining_len, ok, channel_flags
                ))
                {
                    data = resp_result.data;

                    FT::file_transfer_update(self, data);

                    StaticOutStream<UFT::max_block_size_authorized> out_stream;
                    auto block_size = self.ft_reader.block_size();

                    // write cb file data to vnc
                    for (UFT::FilePacketResult result
                      ; !(result = UFT::write_multi_uncompressed_file_packets(
                            out_stream, data, block_size));
                    )
                    {
                        assert(result.ec == UFT::WriteErrorCode::TooSmallBuffer);
                        data = result.remaining_in_data;
                        self.t.send(out_stream.get_produced_bytes());
                        out_stream.rewind();
                    }

                    LOG(LOG_DEBUG, "eof: %d", resp_result.file_is_complete);

                    // end of file
                    if (resp_result.file_is_complete)
                    {
                        FT::file_transfer_completed(self, Mwrm3::Direction::ClientToServer);

                        // write end of file
                        while (is_err(UFT::write_end_of_file(out_stream)))
                        {
                            self.t.send(out_stream.get_produced_bytes());
                            out_stream.rewind();
                        }

                        // write next items
                        FT::copy_item_to_vnc(out_stream, self);
                    }

                    // send buffer
                    self.t.send(out_stream.get_produced_bytes());

                    // request more data
                    if (!resp_result.file_is_complete
                     && flags_any(channel_flags, VNC::ChannelFlags::Last))
                    {
                        out_stream.rewind();
                        FT::request_cb_file_range_unchecked(self, out_stream);
                    }

                    FT::cb_to_vnc_progression(self, {
                        .items = resp_result.file_is_complete,
                        .bytes = resp_result.data.size(),
                        .total_bytes_adjust = resp_result.delta_file_size,
                    });
                }
                else
                {
                    self.t.send(UFT::abort_file_transfer_pdu);
                    auto * file = self.cliprdr_file_list.get_file(resp_result.lindex);

                    if (file)
                    {
                        FT::file_transfer_stopped(
                            self,
                            Mwrm3::Direction::ClientToServer,
                            FT::TransferStopReason::ProtocolError
                        );
                    }

                    self.ft_gui.transfer_progression(FT::progress_error(file));
                }
            }),
            .receive_file_data_request = from_vnc_cb([](mod_vnc & self){
                LOG(LOG_DEBUG, "receive_file_data_request | ft_flags=0x%x", self.ft_flags);

                if (flags_any(self.ft_flags, FtFlags::VncDirContent_CbListCompleted))
                {
                    FT::send_cb_file_list(self);
                }
                else if (flags_any(self.ft_flags, FtFlags::VncDirContent_ForCbList))
                {
                    self.ft_flags |= FtFlags::VncDirContent_WithCbFileRepDelayed;
                }
                else
                {
                    // TODO or empty list ?
                    self.send_to_cliprdr(VNC::format_data_response_fail_with_header);
                }
            }),
            .receive_file_contents_request = from_vnc_cb([](
                mod_vnc & self, VNC::FileContentsRequest const & req
            ){
                LOG(LOG_DEBUG, "receive_file_contents_request | ft_flags=0x%x", self.ft_flags);

                FT::send_transfer_result(self, self.uvnc_file_list.rdp_requested_file(req));
            }),
            .receive_capability_flags = from_vnc_cb([](mod_vnc & self) {
                FT::init_max_blocked_file_size_rejected(self);
            })
        };
    }

    static VNC::FileTransferGui::Callbacks
    make_ft_gui_callbacks(mod_vnc & self) noexcept
    {
        return VNC::FileTransferGui::Callbacks
        {
            .ctx = &self,
            .close_gui = from_vnc_cb([](mod_vnc & self) {
                FT::close_gui(self);
            }),
            .open_dir = from_vnc_cb([](mod_vnc & self, VNC::UVncFile::PathView path) {
                auto buffer = cp1252_to_utf8.buffer_from(path.native());
                LOG(LOG_DEBUG, "open_dir: %.*s", static_cast<int>(buffer.av().size()), buffer.av().as_charp());

                // TODO support of multi go to parent

                StaticOutStream<UVNC::FileTransfer::max_no_data_packet_size> out_stream;
                return FT::open_dir_and_send(out_stream, self, path);
            }),
            .copy_cb_to_vnc = from_vnc_cb([](mod_vnc & self) {
                auto current_dir = self.ft_gui.current_directory();

                auto buffer = cp1252_to_utf8.buffer_from(current_dir.native());
                LOG(LOG_DEBUG, "to vnc: current_dir: %.*s",
                    static_cast<int>(buffer.av().size()), buffer.av().as_charp());

                self.cliprdr_file_list.reset_transfer();
                FT::copy_item_to_vnc(self);
                self.ft_gui.transfer_start(
                    VNC::FileTransferGui::Direction::CbToVnc,
                    {
                        .items = self.cliprdr_file_list.nb_files(),
                        .bytes = self.cliprdr_file_list.get_total_file_size()
                    }
                );
            }),
            .copy_vnc_to_cb = from_vnc_cb([](
                mod_vnc & self,
                VNC::FileTransferGui::SelectedVncFiles files
            ){
                LOG(LOG_DEBUG, "to cb | ft_flags=0x%x", self.ft_flags);

                auto current_dir = self.ft_gui.current_directory();
                FT::send_transfer_result(self, self.uvnc_file_list.start_new_list(current_dir));

                auto buffer1 = cp1252_to_utf8.buffer_from(current_dir.native());

                for (auto && file : files)
                {
                    auto buffer2 = cp1252_to_utf8.buffer_from(file.file_name.native());
                    LOG(LOG_DEBUG, "%.*s|%.*s|",
                        static_cast<int>(buffer1.av().size()), buffer1.av().data(),
                        static_cast<int>(buffer2.av().size()), buffer2.av().as_charp());

                    if (FT::is_file_too_large(self, FileValidatorTargets::Download, 1))
                    {
                        self.session_log.log6(LogId::FILE_BLOCKED, {
                            KVLog("direction"_av, "DOWN"_av),
                            KVLog("file_name"_av, FT::Utf8FileName{file.file_name}.av()),
                        });
                        // TODO gui should prevent selection of file too large
                        continue;
                    }

                    // TODO gui error
                    self.uvnc_file_list.push_file_in_current_dir(file);
                }

                // TODO when list not empty
                self.cliprdr.send_format_list_with_files();
                self.ft_gui.vnc_to_rdp_file_list_start();

                FT::req_vnc_files_recursively(self);
            }),
            .stop_transfer = from_vnc_cb([](mod_vnc & self){
                LOG(LOG_DEBUG, "stop transfer");

                if (auto * file = self.cliprdr_file_list.get_current_file())
                {
                    FT::file_transfer_stopped(
                        self,
                        Mwrm3::Direction::ClientToServer,
                        FT::TransferStopReason::UserRequest
                    );
                }

                FT::send_transfer_result(self, self.uvnc_file_list.stop_file_transfer());
                self.ft_gui.transfer_progression(VNC::FileTransferGui::Progression::abort());
            }),
        };
    }

    static UVNCFileTransferReader::ReceivePacketCallbacks
    make_uvnc_file_transfer_reader_callback(mod_vnc & self) noexcept
    {
        return UVNCFileTransferReader::ReceivePacketCallbacks
        {
            .ctx = &self,
            .error = [](void*, UVNCFileTransferReader::ProtocolError err)
            {
                LOG(LOG_DEBUG, "Error: %u | %u", err.type, err.max_or_min_len);
            },
            .parsing_header = from_vnc_cb([](mod_vnc & self)
            {
                auto header = self.ft_reader.header();
                header.log("Header: ", LOG_DEBUG);
            }),
            .drive_list = from_vnc_cb([](mod_vnc & self, UVNC::FileTransfer::DrivesList drives)
            {
                LOG(LOG_DEBUG, "DriveList | ft_flags=0x%x", self.ft_flags);

                if (!flags_any(self.ft_flags, FtFlags::VncDirContent_ForGui))
                {
                    return ;
                }

                self.ft_flags &= ~FtFlags::VncDirContent_ForGui;

                self.ft_gui.server_vnc_file_list_start(VNC::UVncFile::PathView{});
                for (auto drive : drives)
                {
                    LOG(LOG_DEBUG, " - %c: | %c", drive.drive_letter, drive.drive_type);
                    self.ft_gui.server_vnc_file_list_add_drive(drive.drive_letter, drive.drive_type);
                }
                self.ft_gui.server_vnc_file_list_add_shorcuts();
                self.ft_gui.server_vnc_file_list_end();
            }),
            .start_list_dir = from_vnc_cb([](mod_vnc & self, UVNC::FileTransfer::Path path)
            {
                auto buffer = cp1252_to_utf8.buffer_from(path.native());
                LOG(LOG_DEBUG, "Requested dir%s%.*s | ft_flags=0x%x", path.empty() ? " error" : ": ",
                    static_cast<int>(buffer.av().size()), buffer.av().data(), self.ft_flags);

                if (flags_any(self.ft_flags, FtFlags::VncDirContent_ForGui))
                {
                    if (!path.empty())
                    {
                        self.ft_gui.server_vnc_file_list_start(path);
                    }
                    // error
                    else
                    {
                        self.ft_gui.server_vnc_file_list_error();
                    }
                }
                else if (flags_any(self.ft_flags, FtFlags::VncDirContent_ForCbList))
                {
                    // error
                    if (path.empty())
                    {
                        if (flags_any(self.ft_flags, FtFlags::VncDirContent_WithCbFileRepDelayed))
                        {
                            self.ft_flags &= ~FtFlags::VncDirContent_WithCbFileRepDelayed;
                            self.send_to_cliprdr(VNC::format_data_response_fail_with_header);
                        }

                        // TODO specific error
                        self.ft_gui.server_vnc_file_list_error();
                    }
                }
            }),
            .file_info = from_vnc_cb([](mod_vnc & self, UVNC::FileTransfer::FileInfoPDU file_info)
            {
                LOG(LOG_DEBUG, "FileInfo: attrs=%s(%x) | creat=%lu | access=%lu | write=%lu | fsize=%lu | fname=%.*s (len=%zu) | ft_flags=0x%x",
                    file_attribute_flags_to_string(file_info.attributes),
                    file_info.attributes,
                    file_info.creation_time,
                    file_info.last_access_time,
                    file_info.last_write_time,
                    file_info.file_size(),
                    static_cast<int>(file_info.file_name.size()), file_info.file_name.data(),
                    file_info.file_name.size(),
                    self.ft_flags
                );

                // ignore irrelevant state
                if (!flags_any(self.ft_flags,
                        FtFlags::VncDirContent_ForGui
                      | FtFlags::VncDirContent_ForCbList))
                {
                    return ;
                }

                // skip some file type
                if (flags_any(file_info.attributes, WinNtFileAttributeFlags::System))
                {
                    return ;
                }

                // skip parent directory ("..")
                auto is_dir = flags_any(file_info.attributes, WinNtFileAttributeFlags::Directory);
                if (is_dir && bytes_equal(file_info.file_name, ".."_av))
                {
                    return ;
                }

                /*
                 * Push file
                 */

                VNC::UVncFile file {
                    .file_name { file_info.file_name },
                    .file_size = file_info.file_size(),
                    .last_access_time = file_info.last_access_time,
                    .is_dir = is_dir,
                };

                if (flags_any(self.ft_flags, FtFlags::VncDirContent_ForGui))
                {
                    // add a file in the transferable list
                    self.ft_gui.server_vnc_file_list_add(file);
                }
                else
                {
                    // TODO check CB_HUGE_FILE_SUPPORT_ENABLED
                    // TODO gui error
                    self.uvnc_file_list.push_file_in_current_dir(file);
                }
            }),
            .end_list_dir = from_vnc_cb([](mod_vnc & self)
            {
                LOG(LOG_DEBUG, "File end | ft_flags=0x%x", self.ft_flags);

                if (flags_any(self.ft_flags, FtFlags::VncDirContent_ForGui))
                {
                    self.ft_flags &= ~FtFlags::VncDirContent_ForGui;
                    self.ft_gui.server_vnc_file_list_end();
                }
                else if (flags_any(self.ft_flags, FtFlags::VncDirContent_ForCbList))
                {
                    FT::req_vnc_files_recursively(self);
                }
            }),
            // response to file transfer request
            .file_header = from_vnc_cb([](
                mod_vnc & self,
                bytes_view file_name_with_optional_date,
                UVNC::FileTransfer::FileSizeOrError file_size_or_error
            )
            {
                LOG(LOG_DEBUG, "FileHeader: %.*s | fsize=%lu | err=%d | ft_flags=0x%x",
                    static_cast<int>(file_name_with_optional_date.size()), file_name_with_optional_date.data(),
                    file_size_or_error.file_size(), file_size_or_error.is_error(), self.ft_flags);

                auto * file = self.uvnc_file_list.get_current_file();
                bool is_ok = file_size_or_error.is_ok() && file;

                if (file)
                {
                    FT::file_transfer_start(
                        self,
                        is_ok,
                        Mwrm3::Direction::ServerToClient,
                        file->name(),
                        file->file_size
                    );
                }

                FT::send_transfer_result(
                    self,
                    self.uvnc_file_list.receive_vnc_file_request_response(is_ok)
                );
            }),
            .file_partial_packet = from_vnc_cb([](
                mod_vnc & self, bytes_view data, UVNC::FileTransfer::FilePacketType pkt_type)
            {
                LOG(LOG_DEBUG, "FilePacket: type=%u datalen=%zu | ft_flags=0x%x",
                    pkt_type, data.size(), self.ft_flags);
                // hexdump(data);

                FT::file_transfer_update(self, data);
                FT::send_transfer_result(self, self.uvnc_file_list.receive_vnc_file_data(data));
            }),
            .end_of_file = from_vnc_cb([](mod_vnc & self)
            {
                LOG(LOG_DEBUG, "EndOfFile | ft_flags=0x%x", self.ft_flags);

                if (auto * file = self.uvnc_file_list.get_current_file())
                {
                    FT::file_transfer_completed(self, Mwrm3::Direction::ServerToClient);
                }

                FT::send_transfer_result(self, self.uvnc_file_list.receive_vnc_end_of_file());
            }),
            .aborted_file = from_vnc_cb([](mod_vnc & self)
            {
                LOG(LOG_DEBUG, "AbortFileTransfer | ft_flags=0x%x", self.ft_flags);

                auto * file = self.uvnc_file_list.get_current_file();
                auto result = self.uvnc_file_list.receive_vnc_file_abort();

                if (file && result.response_types[0]
                    == VNC::VncFileList::TransferResult::ResponseType::RdpResponseFailure)
                {
                    FT::file_transfer_stopped(
                        self,
                        Mwrm3::Direction::ServerToClient,
                        FT::TransferStopReason::ProtocolError
                    );
                }

                FT::send_transfer_result(self, result);
            }),
            .file_partial_checksums = from_vnc_cb([](
                mod_vnc & self, bytes_view checksums, uint32_t remaining)
            {
                LOG(LOG_DEBUG, "FileChecksums remaining=%u datalen=%zu | ft_flags=0x%x",
                    remaining, checksums.size(), self.ft_flags);
                // hexdump(checksums);
            }),
            // response to file transfer offer
            .file_accept_header = from_vnc_cb([](
                mod_vnc & self,
                bytes_view tmp_file_name,
                bool accepted
            ) {
                LOG(LOG_DEBUG, "FileAcceptHeader: %.*s | accepted = %d | ft_flags=0x%x",
                    static_cast<int>(tmp_file_name.size()), tmp_file_name.data(), accepted,
                    self.ft_flags);

                auto * file = self.cliprdr_file_list.get_current_file();
                bool ok = accepted
                       && file
                       && self.cliprdr_file_list.receive_uvnc_file_accept_response();

                if (file)
                {
                    FT::file_transfer_start(
                        self,
                        ok,
                        Mwrm3::Direction::ClientToServer,
                        file->name(),
                        file->file_size
                    );
                }

                if (ok)
                {
                    // request data
                    if (file->file_size)
                    {
                        StaticOutStream<128> out_stream;
                        FT::request_cb_file_range_unchecked(self, out_stream);
                    }
                    // request next file because no data
                    else
                    {
                        FT::file_transfer_completed(self, Mwrm3::Direction::ClientToServer);
                        self.t.send(UVNC::FileTransfer::end_of_file_pdu);

                        self.cliprdr_file_list.next_file();
                        FT::copy_item_to_vnc(self);

                        FT::cb_to_vnc_progression(self, {
                            .items = 1,
                            .bytes = 0,
                            .total_bytes_adjust = 0,
                        });
                    }
                }
                // file rejected or error
                else
                {
                    self.ft_gui.transfer_progression(FT::progress_error(file));
                    self.cliprdr_file_list.reset_transfer();
                }
            }),
            .command_return = from_vnc_cb([](mod_vnc & self, bytes_view response, bool is_ok)
            {
                LOG(LOG_DEBUG, "CommandReturn: %d | %.*s | ft_flags=0x%x",
                    is_ok, static_cast<int>(response.size()), response.data(), self.ft_flags);

                auto status = is_ok
                    ? self.cliprdr_file_list.receive_uvnc_create_dir_response()
                    : VNC::CliprdrFileList::ReceiveStatus::Error;

                // assume response for create directory
                switch (status)
                {
                    case VNC::CliprdrFileList::ReceiveStatus::Error:
                        self.ft_gui.transfer_progression(
                            FT::progress_error(self.cliprdr_file_list.get_current_file())
                        );
                        self.cliprdr_file_list.reset_transfer();
                        break;
                    case VNC::CliprdrFileList::ReceiveStatus::WaitingResponse:
                        break;
                    case VNC::CliprdrFileList::ReceiveStatus::TransferComplete:
                        self.ft_gui.transfer_progression({
                            .state = VNC::FileTransferGui::Progression::State::Completed,
                            .items = 1,
                            .bytes = 0,
                            .total_bytes_adjust = 0,
                        });
                        break;
                    case VNC::CliprdrFileList::ReceiveStatus::ReadyForNextItems:
                        FT::copy_item_to_vnc(self);
                        self.ft_gui.transfer_progression(
                            VNC::FileTransferGui::Progression::next_item()
                        );
                        break;
                }
            }),
            .file_transfer_access = from_vnc_cb([](mod_vnc & self, bool enabled)
            {
                FT::access_response(self, enabled);
            }),
            .protocol_version = from_vnc_cb([](mod_vnc & self, uint32_t version, bool supported)
            {
                LOG(LOG_INFO, "FileTransferProtocolVersion: %u | supported=%d | block_size=%u", version, supported, self.ft_reader.block_size());

                if (!supported)
                {
                    return ;
                }

                self.ft_flags |= FtFlags::FtSupported;
                FT::init_file_validator_and_storage(self);
                self.cliprdr.enable_file_transfer();
                self.cliprdr.init_cliprdr_server();
            }),
        };
    }

    static void open_gui(mod_vnc & self)
    {
        LOG(LOG_DEBUG, "open_gui");

        self.ft_flags &= ~FtFlags::FtAccessNo;
        self.ft_flags &= ~FtFlags::FtAccessYes;
        self.ft_flags |= FtFlags::FtAccessRequested;

        self.ft_gui.open(self.width, self.height, self.mouse.mouse_x(), self.mouse.mouse_y());
        FT::update_file_group_descriptor(self);

        // request access permission
        self.t.send(UVNC::FileTransfer::abort_file_transfer_pdu);

        self.gd = gdi::null_gd();
    }

    static void close_gui(mod_vnc & self)
    {
        LOG(LOG_DEBUG, "close_gui");
        self.ft_gui.close();

        // remove all flags, excepted FtSupported
        self.ft_flags &= FtFlags::FtSupported;

        self.gd = self.original_gd;
        if (self.has_cursor)
        {
            self.gd->cached_pointer(0);
        }
        self.update_screen(Rect(0, 0, self.width, self.height), 0);
        // TODO vnc transfer stop
    }

    static bool is_loggable(mod_vnc const& self)
    {
        return flags_any(self.verbose, VNCVerbose::clipboard);
    }

    static void skip_error(UVNC::FileTransfer::WriteErrorCode err) noexcept
    {
        assert(is_ok(err));
        (void)err;
    }

    static void access_response(mod_vnc & self, bool enabled)
    {
        LOG_IF(is_loggable(self), LOG_INFO, "FileTransferAccess: %d | ft_flags=0x%x",
            enabled, self.ft_flags);

        if (!flags_any(self.ft_flags, FtFlags::FtAccessRequested))
        {
            return ;
        }

        self.ft_flags &= ~FtFlags::FtAccessRequested;

        // file transfer is disabled
        if (!enabled)
        {
            self.ft_flags |= FtFlags::FtAccessNo;
            self.ft_gui.server_vnc_file_disabled();
            return ;
        }

        /*
         * Open file transfer and req Disk or DirContent
         */

        self.ft_flags |= FtFlags::FtAccessYes;

        StaticOutStream<UVNC::FileTransfer::max_no_data_packet_size * 2> out_stream;

        skip_error(UVNC::FileTransfer::write_session_start(out_stream));

        open_dir_and_send(out_stream, self, VNC::UVncFile::PathView{});
    }

    static bool open_dir_and_send(OutStream & out_stream, mod_vnc & self, VNC::UVncFile::PathView path)
    {
        // req already in progress
        if (flags_any(self.ft_flags,
                FtFlags::VncDirContent_ForGui
              | FtFlags::VncDirContent_ForCbList))
        {
            return true;
        }

        auto result = path.empty()
            ? UVNC::FileTransfer::write_drives_list_request(out_stream)
            : UVNC::FileTransfer::write_directory_content_request(out_stream, path);

        if (is_ok(result))
        {
            self.ft_flags |= FtFlags::VncDirContent_ForGui;
            self.t.send(out_stream.get_produced_bytes());
            return true;
        }

        // TODO add path
        log_uvnc_buffer_error("vnc::gui::open_dir", result);
        return false;
    }

    static void update_file_group_descriptor(mod_vnc & self)
    {
        if (self.cliprdr.has_file_group_descriptor_format())
        {
            if (self.cliprdr.request_file_list())
            {
                LOG(LOG_DEBUG, "file list requested");
                self.ft_gui.client_cb_file_list_requested();
            }
        }
        else
        {
            LOG(LOG_DEBUG, "show no files");
            self.ft_gui.client_cb_file_list_reset();
        }
    }

    static void copy_item_to_vnc(mod_vnc & self)
    {
        StaticOutStream<UVNC::FileTransfer::min_full_packet_size> out_stream;
        if (copy_item_to_vnc(out_stream, self))
        {
            auto d = out_stream.get_produced_bytes();
            if (!d.empty())
            {
                self.t.send(d);
            }
        }
    }

    [[nodiscard]]
    static bool copy_item_to_vnc(OutStream & out_stream, mod_vnc & self)
    {
        assert(out_stream.get_capacity());

        auto current_dir = self.ft_gui.current_directory();

        write_items:
        switch (self.cliprdr_file_list.write_uvnc_items_to_vnc(out_stream, current_dir))
        {
            case VNC::CliprdrFileList::ErrorCode::Ok:
                return true;

            // buffer is full, send then try again
            case VNC::CliprdrFileList::ErrorCode::TooSmallBuffer:
                self.t.send(out_stream.get_produced_bytes());
                out_stream.rewind();
                goto write_items;

            case VNC::CliprdrFileList::ErrorCode::PathTooLong:
                break;
        }

        // TODO message to gui + stop. Check before copy
        auto path = self.cliprdr_file_list.get_current_file_unchecked().name().native();
        LOG(LOG_INFO, "CliprdrFileList::write_uvnc_items_to_vnc: path too long: %.*s/%.*s",
            static_cast<int>(current_dir.native().size()), current_dir.native().as_charp(),
            static_cast<int>(path.size()), path.as_charp());
        return false;
    }

    static void request_cb_file_range_unchecked(mod_vnc & self, OutStream & out_stream)
    {
        auto ec = self.cliprdr_file_list.write_cb_file_range_request(out_stream);
        assert(is_ok(ec));

        self.send_to_cliprdr(out_stream.get_produced_bytes());
    }

    struct CbToVncProgression
    {
        uint32_t items;
        uint64_t bytes;
        int64_t total_bytes_adjust;
    };

    static void cb_to_vnc_progression(mod_vnc & self, CbToVncProgression progression)
    {
        self.ft_gui.transfer_progression({
            .state = self.cliprdr_file_list.is_transfer_complete()
                ? VNC::FileTransferGui::Progression::State::Completed
                : VNC::FileTransferGui::Progression::State::InProgress,
            .items = progression.items,
            .bytes = progression.bytes,
            .total_bytes_adjust = progression.total_bytes_adjust,
        });
    }

    static
    VNC::FileTransferGui::Progression
    progress_error(VNC::CliprdrFileList::File const * file) noexcept
    {
        return VNC::FileTransferGui::Progression::error(file ? file->name() : WinNtPathView{});
    }

    enum class TraversaleState : bool
    {
        Running,
        Completed,
    };

    static void req_vnc_files_recursively(mod_vnc & self)
    {
        StaticOutStream<UVNC::FileTransfer::max_no_data_packet_size> out_stream;

        LOG_IF(bool(self.verbose & VNCVerbose::clipboard),
            LOG_INFO, "mod_vnc::req_vnc_files_recursively: request files in subdirs");

        switch (self.uvnc_file_list.write_next_vnc_directory_content_request(out_stream))
        {
            case VNC::VncFileList::NextDirectoryResult::Ok:
                self.ft_flags |= FtFlags::VncDirContent_ForCbList;
                self.t.send(out_stream.get_produced_bytes());
                return;

            case VNC::VncFileList::NextDirectoryResult::TooSmallBuffer:
                // out_stream has always sufficient space.
                assert(false);
            case VNC::VncFileList::NextDirectoryResult::NoDir:
                break;
        }

        LOG_IF(bool(self.verbose & VNCVerbose::clipboard),
            LOG_INFO, "mod_vnc::req_vnc_files_recursively completed");

        self.ft_gui.vnc_to_rdp_file_list_ready();

        bool has_req = flags_any(self.ft_flags, FtFlags::VncDirContent_WithCbFileRepDelayed);
        self.ft_flags &= ~FtFlags::VncDirContent_WithCbFileRepDelayed;
        self.ft_flags &= ~FtFlags::VncDirContent_ForCbList;
        self.ft_flags |= FtFlags::VncDirContent_CbListCompleted;
        if (has_req)
        {
            FT::send_cb_file_list(self);
        }
    }

    static void send_cb_file_list(mod_vnc & self)
    {
        FT::send_transfer_result(self, self.uvnc_file_list.start_rdp_file_list());

        StaticOutStream<64 * 1024> out_stream;

        using Status = VNC::VncFileList::PartialRdpFileListResult::Status;

        auto result = VNC::VncFileList::PartialRdpFileListResult {};

        do
        {
            result = self.uvnc_file_list.write_partial_rdp_file_list(out_stream);
            assert(result.status != Status::TooSmallBuffer);

            self.send_to_cliprdr(
                out_stream.get_produced_bytes(),
                result.total_len,
                result.channel_flags()
            );
        }
        while (result.status != Status::Completed);
    }

    static void send_transfer_result(mod_vnc & self, VNC::VncFileList::TransferResult result)
    {
        if (bool(self.verbose & VNCVerbose::clipboard)) [[unlikely]]
        {
            constexpr auto total_responses = std::size(result.response_types);
            unsigned i = 0;
            for (auto && response_type : result.response_types)
            {
                char const * s = nullptr;
#define CASE(x) case VNC::VncFileList::TransferResult::ResponseType::x: s = #x; break
                switch (response_type)
                {
                    CASE(Nothing);
                    CASE(InvalidLindex);
                    CASE(RdpResponseUnsequenced);
                    CASE(RdpResponseFailure);
                    CASE(RdpResponseSize);
                    CASE(RdpResponseData);
                    CASE(VncRequestFile);
                    CASE(VncConfirmFile);
                    CASE(VncAbortFile);
#undef CASE
                }
                LOG(LOG_INFO, "mod_vnc::send_transfer_result: %u/%zu. %s", i, total_responses, s);
                ++i;
            }
        }

        if (!result.rdp_data.empty())
        {
            self.send_to_cliprdr(result.rdp_data);
        }

        if (!result.vnc_data.empty())
        {
            self.t.send(result.vnc_data);
        }
    }

    static void init_max_blocked_file_size_rejected(mod_vnc & self) noexcept
    {
        auto huge_flag = VNC::CbCapabilityFlags::HugeFileSupportEnabled;
        auto protocol_limit
            = (flags_any(self.cliprdr.cb_capabilities(), huge_flag))
            ? ~uint64_t{}
            : ~uint32_t{};

        auto & ctx = self.transfered_file_ctx;

        ctx.max_blocked_file_size_rejected_upload
            = flags_any(ctx.block_invalid_file, FileValidatorTargets::Upload)
            ? mmin(protocol_limit, ctx.original_max_blocked_file_size_rejected)
            : protocol_limit;

        ctx.max_blocked_file_size_rejected_download
            = flags_any(ctx.block_invalid_file, FileValidatorTargets::Download)
            ? mmin(protocol_limit, ctx.original_max_blocked_file_size_rejected)
            : protocol_limit;
    }

    static void init_file_validator_and_storage(mod_vnc & self)
    {
        auto & ctx = self.transfered_file_ctx;

        // when already initialized
        if (ctx.get_file_validator_and_storage.is_null_function())
        {
            return ;
        }

        auto [file_validator, file_storage] = ctx.get_file_validator_and_storage();
        ctx.get_file_validator_and_storage = NullFunctionWithDefaultResult{};

        if (file_storage)
        {
            ctx.fdx_capture = file_storage.fdx_capture;
            ctx.file_storage_option = file_storage.always_file_storage
                ? FileStorageOption::Always
                : FileStorageOption::OnInvalidFile;
        }

        if (file_validator)
        {
            ctx.file_validator = file_validator.file_validator_service;
            ctx.validator_targets = file_validator.targets;
            ctx.log_if_accepted = file_validator.log_if_accepted;
            ctx.block_invalid_file = FileValidatorTargets::None
                | (file_validator.block_invalid_file_upload
                    ? FileValidatorTargets::Upload
                    : FileValidatorTargets::None)
                | (file_validator.block_invalid_file_download
                    ? FileValidatorTargets::Download
                    : FileValidatorTargets::None);
            ctx.block_invalid_file &= file_validator.targets;
            ctx.original_max_blocked_file_size_rejected
                = file_validator.max_blocked_file_size_rejected;
            ctx.tmp_dir.init(file_validator.tmp_dir);
            init_max_blocked_file_size_rejected(self);
        }
    }

    static bool is_file_too_large(
        mod_vnc & self, FileValidatorTargets target, uint64_t file_size) noexcept
    {
        auto & ctx = self.transfered_file_ctx;
        auto max_file_size = (target == FileValidatorTargets::Download)
            ? ctx.max_blocked_file_size_rejected_download
            : ctx.max_blocked_file_size_rejected_upload;
        return file_size > max_file_size;
    }

    static FileValidatorTargets direction_to_file_validator_target(Mwrm3::Direction direction) noexcept
    {
        return direction == Mwrm3::Direction::ServerToClient
            ? FileValidatorTargets::Download
            : FileValidatorTargets::Upload;
    }

    static chars_view direction_to_target_name(Mwrm3::Direction direction) noexcept
    {
        return direction == Mwrm3::Direction::ServerToClient ? "DOWN"_av : "UP"_av;
    }

    static chars_view target_to_direction_name(FileValidatorTargets target) noexcept
    {
        return target == FileValidatorTargets::Download ? "DOWN"_av : "UP"_av;
    }

    // Log SIEM:
    //                       +--------------------------+---------------
    //                       |           icap           |   other
    // ----------------------|--------------------------|---------------
    // before req file offer |                          | Rejected (when file too large)
    //                       |                          | blocked by gui (vnc -> rdp)
    // ----------------------|--------------------------|---------------
    // file offer response   |                          | Failed (when error)
    // ----------------------|--------------------------|---------------
    // receive data          |                          | Rejected (when file too large)
    // ----------------------|--------------------------|---------------
    // icap response         | FILE_VERIFICATION        |
    //                       | FILE_BLOCKED (when fail) |
    // ----------------------|--------------------------|---------------
    // eof                   |                          | Completed
    // ----------------------|--------------------------|---------------
    // error                 |                          | Failed
    // ----------------------|--------------------------|---------------
    // user cancellation     |                          | Stopped
    // ----------------------|--------------------------|---------------

    static void file_transfer_start(
        mod_vnc & self,
        bool is_ok,
        Mwrm3::Direction direction,
        VNC::UVncFile::PathView file_name,
        uint64_t file_size)
    {
        auto & ctx = self.transfered_file_ctx;
        auto validator_target = direction_to_file_validator_target(direction);
        bool use_validator = flags_any(ctx.validator_targets, validator_target);

        if (!ctx.current_file.utf8_file_name.is_empty())
        {
            file_transfer_stopped(self, direction, TransferStopReason::UserRequest);
        }

        LOG(LOG_DEBUG, "file started");

        ctx.current_file.file_size = file_size;
        ctx.current_file.utf8_file_name.reset(file_name);
        ctx.current_file.validator_status = TransferValidatorStatus::WaitResponse;
        ctx.sha2.reset();

        if (is_ok)
        {
            if (use_validator)
            {
                ctx.current_file.validator_target = validator_target;
                ctx.current_file.validator_id = ctx.file_validator->open_file(
                    ctx.current_file.utf8_file_name.av(),
                    direction_to_target_name(direction)
                );
            }

            if (ctx.fdx_capture)
            {
                ctx.current_file.tfl_file = std::unique_ptr<TransferedFileCtx::TflFile>(
                    static_cast<TransferedFileCtx::TflFile*>(
                        new FdxCapture::TflFile{
                            ctx.fdx_capture->new_tfl(direction)
                        }
                    )
                );
            }
        }
        else
        {
            self.session_log.log6(
                (direction == Mwrm3::Direction::ClientToServer)
                    ? LogId::CB_COPYING_PASTING_FILE_TO_REMOTE_SESSION
                    : LogId::CB_COPYING_PASTING_FILE_FROM_REMOTE_SESSION,
                {
                    KVLog{"file_name"_av, ctx.current_file.utf8_file_name.av()},
                    KVLog{"size"_av, int_to_decimal_chars(file_size)},
                    KVLog{"status"_av, "Reject"_av},
                }
            );
        }
    }

    static void file_transfer_update(mod_vnc & self, bytes_view data)
    {
        auto & ctx = self.transfered_file_ctx;
        auto & file = ctx.current_file;

        ctx.sha2.update(data);

        if (file.validator_id != FileValidatorId{})
        {
            ctx.file_validator->send_data(file.validator_id, data);
        }

        if (file.tfl_file)
        {
            file.tfl_file->trans.send(data);
        }
    }

    static void close_tfl(
        TransferedFileCtx & ctx,
        TransferedFileCtx::File & file,
        Mwrm3::TransferedStatus status)
    {
        REDEMPTION_ASSUME(ctx.fdx_capture);

        LOG(LOG_DEBUG, "close tfl %lu", file.tfl_file->file_id);

        ctx.fdx_capture->close_tfl(
            *file.tfl_file,
            file.utf8_file_name.av().as<std::string_view>(),
            status,
            Mwrm3::Sha256Signature{ make_sized_array_view(file.hash.buffer) }
        );
        file.tfl_file.reset();
    }

    static void cancel_tfl(TransferedFileCtx & ctx, TransferedFileCtx::File & file)
    {
        REDEMPTION_ASSUME(ctx.fdx_capture);

        LOG(LOG_DEBUG, "cancel tfl %lu", file.tfl_file->file_id);

        ctx.fdx_capture->cancel_tfl(*file.tfl_file);
        file.tfl_file.reset();
    }

    static auto finalize_sha2_sig(TransferedFileCtx & ctx)
    {
        auto & file = ctx.current_file;

        auto hash_buf = make_writable_sized_array_view(file.hash.buffer);
        ctx.sha2.final(hash_buf);

        return hash_buf;
    }

    static void file_transfer_completed(mod_vnc & self, Mwrm3::Direction direction)
    {
        auto & ctx = self.transfered_file_ctx;
        auto & file = ctx.current_file;

        LOG(LOG_DEBUG, "file completed");

        auto hash_buf = finalize_sha2_sig(ctx);

        self.session_log.log6(
            (direction == Mwrm3::Direction::ClientToServer)
                ? LogId::CB_COPYING_PASTING_FILE_TO_REMOTE_SESSION
                : LogId::CB_COPYING_PASTING_FILE_FROM_REMOTE_SESSION,
            {
                KVLog{"file_name"_av, file.utf8_file_name.av()},
                KVLog{"size"_av, int_to_decimal_chars(file.file_size)},
                KVLog{"status"_av, "Completed"_av},
                KVLog{"sha2"_av, static_array_to_hexadecimal_lower_chars(hash_buf)},
            }
        );

        if (file.tfl_file)
        {
            LOG(LOG_DEBUG, "completed id = %lu", file.tfl_file->file_id);
            LOG(LOG_DEBUG, "validator id = %u", file.validator_id);

            if (ctx.file_storage_option == FileStorageOption::Always
             || file.validator_status == TransferValidatorStatus::IsRejected)
            {
                close_tfl(ctx, file, Mwrm3::TransferedStatus::Completed);
            }
            else if (file.validator_id == FileValidatorId{})
            {
                cancel_tfl(ctx, file);
            }
        }

        if (file.validator_id != FileValidatorId{}
         && file.validator_status == TransferValidatorStatus::WaitResponse)
        {
            ctx.file_validator->send_eof(file.validator_id);
            move_to_waiting_file_validator_list(ctx);
        }

        file.utf8_file_name.reset();
    }

    enum class TransferStopReason : bool
    {
        UserRequest,
        ProtocolError,
    };

    static void file_verification_log(
        mod_vnc & self,
        TransferedFileCtx::File const & file,
        chars_view status)
    {
        self.session_log.log6(LogId::FILE_VERIFICATION, {
            KVLog("direction"_av, FT::target_to_direction_name(file.validator_target)),
            KVLog("file_name"_av, file.utf8_file_name.av()),
            KVLog("status"_av, status),
        });
    }

    static void file_transfer_stopped(
        mod_vnc & self,
        Mwrm3::Direction direction,
        TransferStopReason reason)
    {
        auto & ctx = self.transfered_file_ctx;
        auto & file = ctx.current_file;

        LOG(LOG_DEBUG, "file stopped");

        auto requested_by_user = (reason == TransferStopReason::UserRequest);
        self.session_log.log6(
            (direction == Mwrm3::Direction::ClientToServer)
                ? LogId::CB_COPYING_PASTING_FILE_TO_REMOTE_SESSION
                : LogId::CB_COPYING_PASTING_FILE_FROM_REMOTE_SESSION,
            {
                KVLog{"file_name"_av, file.utf8_file_name.av()},
                KVLog{"size"_av, int_to_decimal_chars(file.file_size)},
                KVLog{"status"_av, requested_by_user ? "Stopped"_av : "Failed"_av},
            }
        );

        if (file.tfl_file && ctx.file_storage_option == FileStorageOption::Always)
        {
            REDEMPTION_ASSUME(ctx.fdx_capture);
            finalize_sha2_sig(ctx);
            close_tfl(ctx, file, Mwrm3::TransferedStatus::Broken);
        }

        if (file.validator_id != FileValidatorId{})
        {
            file_verification_log(self, file, "Stopped"_av);

            if (file.tfl_file
             && ctx.file_storage_option == FileStorageOption::OnInvalidFile)
            {
                FT::move_to_waiting_file_validator_list(ctx);
            }
            else
            {
                ctx.file_validator->send_abort(file.validator_id);
                file.validator_id = {};
            }
        }

        file.utf8_file_name.reset();
    }

    static void move_to_waiting_file_validator_list(TransferedFileCtx & ctx)
    {
        LOG(LOG_DEBUG, "move to waiting_validator_file_list");
        ctx.waiting_validator_file_list.push_back(std::move(ctx.current_file));
        ctx.current_file.validator_status = TransferValidatorStatus::WaitResponse;
        ctx.current_file.validator_id = {};
    }

    static void disconnect(mod_vnc & self)
    {
        auto & ctx = self.transfered_file_ctx;

        auto close_file = [&](TransferedFileCtx::File & file)
        {
            if (file.validator_id != FileValidatorId{})
            {
                file_verification_log(self, file, "Connection closed"_av);
            }

            if (file.tfl_file)
            {
                close_tfl(ctx, file, Mwrm3::TransferedStatus::Broken);
            }
        };

        try
        {
            if (ctx.current_file.validator_id != FileValidatorId{})
            {
                finalize_sha2_sig(ctx);
            }

            close_file(ctx.current_file);

            while (!ctx.waiting_validator_file_list.empty())
            {
                close_file(ctx.waiting_validator_file_list.back());
                ctx.waiting_validator_file_list.pop_back();
            }
        }
        catch (Error const& err)
        {
            LOG(LOG_ERR, "mod_vnc: error on close tfls: %s", err.errmsg());
        }
    }
};


mod_vnc::dynamic_non_null_cstring::~dynamic_non_null_cstring()
{
    _free();
}

void mod_vnc::dynamic_non_null_cstring::init(chars_view str)
{
    _free();
    auto * mem = static_cast<char*>(::operator new (str.size() + 1));
    m_str = mem;
    bytes_copy(mem, str);
    mem[str.size()] = '\0';
}

void mod_vnc::dynamic_non_null_cstring::_free() noexcept
{
    if (*m_str)
    {
        ::operator delete (const_cast<char*>(m_str));
        m_str = "";
    }
}




void mod_vnc::VncTransport::send(bytes_view buffer)
{
    if (m_mod.dsmEncryption) {
        BufMaker<0x10000> tmpBuf;
        writable_bytes_view tmp = tmpBuf.dyn_array(buffer.size());

        m_mod.dsm->encrypt(buffer.data(), buffer.size(), tmp);
        m_trans.send(tmp);
    } else {
        m_trans.send(buffer);
    }
}

mod_vnc::VncBuf64k::size_type
mod_vnc::VncBuf64k::read_from(mod_vnc::VncTransport vncTrans)
{
    Transport & trans = vncTrans.get_transport();

    const size_type read_len = Buf64k::read_from(trans);

    if (m_mod.dsmEncryption){
        writable_bytes_view buf = this->av();
        m_mod.dsm->decrypt(buf.data(), read_len, buf);
    }

    return read_len;
}

mod_vnc::mod_vnc( Transport & t
           , Random & rand
           , gdi::GraphicApi & gd
           , Font const& glyphs
           , EventContainer & events
           , const char * username
           , const char * password
           , FrontAPI & front
           // TODO: front width and front height should be provided through info
           , uint16_t front_width
           , uint16_t front_height
           , ModVncParams params
           , const char * encodings
           , KeyLayout const& layout
           , kbdtypes::KeyLocks locks
           , bool server_is_macos
           , bool server_is_unix
           , bool cursor_pseudo_encoding_supported
           , ClientExecute* rail_client_execute
           , VNCVerbose verbose
           , SessionLogApi& session_log
           , ModTlsParams const& tls_params
           , std::string_view force_authentication_method
           , Translator const& translator
           )
    : front(front)
    , t(VncTransport(*this, t))
    , dsm(nullptr)
    , dsmEncryption(false)
    , width(front_width)
    , height(front_height)
    , verbose(verbose)
    , keymapSym(layout, locks,
                KeymapSym::IsApple(server_is_macos),
                KeymapSym::IsUnix(server_is_unix),
                bool(verbose & VNCVerbose::keymap))
    , encodings(encodings)
    , session_time_start(events.get_monotonic_time_since_epoch())
    , rail_client_execute(rail_client_execute)
    , rand(rand)
    , gd(gd)
    , original_gd(gd)
    , events_guard(events)
#ifndef __EMSCRIPTEN__
    , session_log(session_log)
#endif
    , choosenAuth(VNC_AUTH_INVALID)
    , cursor_pseudo_encoding_supported(cursor_pseudo_encoding_supported)

    , cliprdr_chann([&]{
        auto * cliprdr_chann = front.get_channel_list().get_by_name(channel_names::cliprdr);
        // disabled cliprdr when text and file transfer are disabled
        return cliprdr_chann
            && (params.cut_text.enable_upload
             || params.cut_text.enable_download
             || params.file_transfer.enable_upload
             || params.file_transfer.enable_download
            )
            ? cliprdr_chann
            : nullptr;
    }())
    , cliprdr(
        params.cut_text.bogus_infinite_loop_strategy,
        params.cut_text.server_encoding,
        VNC::CliprdrAdapter::RdpToVncOptions::None
        | (cliprdr_chann && params.cut_text.enable_upload
            ? VNC::CliprdrAdapter::RdpToVncOptions::NonFileResponse
            : VNC::CliprdrAdapter::RdpToVncOptions::None)
        | (cliprdr_chann && params.cut_text.enable_download
            ? VNC::CliprdrAdapter::RdpToVncOptions::NonFileRequest
            : VNC::CliprdrAdapter::RdpToVncOptions::None)
        | (cliprdr_chann && params.file_transfer.enable_upload
            ? VNC::CliprdrAdapter::RdpToVncOptions::FileResponse
            : VNC::CliprdrAdapter::RdpToVncOptions::None)
        | (cliprdr_chann && params.file_transfer.enable_download
            ? VNC::CliprdrAdapter::RdpToVncOptions::FileRequest
            : VNC::CliprdrAdapter::RdpToVncOptions::None),
        bool(verbose & VNCVerbose::clipboard_dump)
            ? VNC::CliprdrAdapter::Log::Dump
            : bool(verbose & VNCVerbose::clipboard)
                ? VNC::CliprdrAdapter::Log::Yes
                : VNC::CliprdrAdapter::Log::No,
        VNC::CliprdrAdapter::MaxRdpPduLen(CHANNELS::CHANNEL_CHUNK_LENGTH),
        events,
        FT::make_cliprdr_adapter_callbacks(*this)
    )
    , cliprdr_file_list{ params.file_transfer.max_file_list }
    , uvnc_file_list{ {
        .max_nb_files = params.file_transfer.max_file_list,
        .max_file_size = params.file_transfer.max_file_size,
    } }
    , ft_gui {
        gd,
        glyphs,
        events,
        params.file_transfer.max_file_list,
        VNC::FileTransferGui::TransferOptions::None
        | (cliprdr_chann && params.file_transfer.enable_upload
            ? VNC::FileTransferGui::TransferOptions::CbToVnc
            : VNC::FileTransferGui::TransferOptions::None)
        | (cliprdr_chann && params.file_transfer.enable_download
            ? VNC::FileTransferGui::TransferOptions::VncToCb
            : VNC::FileTransferGui::TransferOptions::None),
        translator,
        FT::make_ft_gui_callbacks(*this)
    }
    , transfered_file_ctx {
        .get_file_validator_and_storage = params.get_file_validator_and_storage,
    }
    , do_tls_params {
        .certif_path = str_concat(app_path(AppPath::Certif), '/', tls_params.device_id),
        .server_cert = tls_params.server_cert,
        .tls_config = tls_params.tls_config,
        .server_cert_check_using_ca = tls_params.ca.enable_ca_certificates,
        .ca_certificates = tls_params.ca.certificates.as<std::string>(),
        .target_host = tls_params.target_host.as<std::string>(),
    }
    , server_data_buf(*this)
    , tlsSwitch(false)
    , frame_buffer_update_ctx(this->zd, verbose)
{
    LOG_IF(bool(verbose), LOG_INFO, "mod_vnc::verbosity=0x%x", underlying_cast(verbose));

    std::snprintf(this->username, sizeof(this->username), "%s", username);
    std::snprintf(this->password, sizeof(this->password), "%s", password);

    this->events_guard.create_event_timeout(
        "VNC Init Event",
        this->events_guard.get_monotonic_time(),
        [this](Event& event)
        {
            // First Timeout Clear Screen
            gdi_clear_screen(*this->gd, this->get_dim());
            event.garbage = true;

            // Following fd timeouts
            this->events_guard.create_event_fd_without_timeout(
                "VNC Fd Event",
                this->t.get_fd(),
                [this](Event& /*event*/)
                {
                    this->draw_event();
                }
            );
        }
    );

    using namespace std::string_view_literals;

    if (!force_authentication_method.empty()) {
        if (force_authentication_method == "none"sv) {
            this->force_authentication_method = VNC_AUTH_NONE;
        }
        else  if (force_authentication_method == "vncauth"sv) {
            this->force_authentication_method = VNC_AUTH_VNC;
        }
        else  if (force_authentication_method == "mslogon"sv) {
            this->force_authentication_method = VNC_AUTH_ULTRA_MS_LOGON;
        }
        else  if (force_authentication_method == "mslogoniiauth"sv) {
            this->force_authentication_method = VNC_AUTH_ULTRA_MsLogonIIAuth;
        }
        else  if (force_authentication_method == "ultravnc_dsm_old"sv) {
            this->force_authentication_method = VNC_AUTH_ULTRA_SecureVNCPluginAuth;
        }
        else  if (force_authentication_method == "ultravnc_dsm_new"sv) {
            this->force_authentication_method = VNC_AUTH_ULTRA_SecureVNCPluginAuth_new;
        }
        else  if (force_authentication_method == "tlsnone"sv) {
            this->force_authentication_method = VeNCRYPT_TLSNone;
        }
        else  if (force_authentication_method == "tlsvnc"sv) {
            this->force_authentication_method = VeNCRYPT_TLSVnc;
        }
        else  if (force_authentication_method == "tlsplain"sv) {
            this->force_authentication_method = VeNCRYPT_TLSPlain;
        }
        else  if (force_authentication_method == "x509none"sv) {
            this->force_authentication_method = VeNCRYPT_X509None;
        }
        else  if (force_authentication_method == "x509vnc"sv) {
            this->force_authentication_method = VeNCRYPT_X509Vnc;
        }
        else  if (force_authentication_method == "x509plain"sv) {
            this->force_authentication_method = VeNCRYPT_X509Plain;
        }
        else {
            LOG(LOG_ERR, "mod_vnc: unknwown force_authentication_method: %.*s",
                static_cast<int>(force_authentication_method.size()),
                force_authentication_method.data());
            throw Error(ERR_VNC);
        }
    }
}

mod_vnc::~mod_vnc()
{
    FT::disconnect(*this);
}

bool mod_vnc::ms_logon(Buf64k & buf)
{
    if (!this->ms_logon_ctx.run(buf)) {
        return false;
    }

    if (bool(this->verbose & VNCVerbose::basic_trace)) {
        LOG(LOG_INFO, "MS-Logon with following values:");
        LOG(LOG_INFO, "Gen=0x%" PRIx64, this->ms_logon_ctx.gen);
        LOG(LOG_INFO, "Mod=0x%" PRIx64, this->ms_logon_ctx.mod);
        LOG(LOG_INFO, "Resp=0x%" PRIx64, this->ms_logon_ctx.resp);
    }

    DiffieHellman dh(this->rand, this->ms_logon_ctx.gen, this->ms_logon_ctx.mod);
    uint64_t pub = dh.createInterKey();

    StaticOutStream<32768> out_stream;
    out_stream.out_uint64_be(pub);

    uint64_t key = dh.createEncryptionKey(this->ms_logon_ctx.resp);
    uint8_t keybuffer[8] = {};
    dh.uint64_to_uint8p(key, keybuffer);

    RfbD3DesEncrypter encrypter(make_bounded_array_view(keybuffer));

    auto out_copy_encrypted = [&](auto chars){
        bounded_array_view bav = to_bounded_u8_av(chars);
        using BAV = decltype(bav);
        using WritableBAV = as_writable_bounded_array_view_t<BAV>;
        static_assert(BAV::at_least == BAV::at_most);
        auto out = out_stream.out_skip_bytes(BAV::at_least);
        auto key = make_bounded_array_view(keybuffer);
        encrypter.encrypt_text(bav, WritableBAV::assumed(out), key);
    };

    out_copy_encrypted(make_bounded_array_view(this->username).first<256>());
    out_copy_encrypted(make_bounded_array_view(this->password).first<64>());

    this->t.send(out_stream.get_produced_bytes());
    // sec result

    return true;
}

void mod_vnc::initial_clear_screen()
{
    LOG_IF(bool(this->verbose & VNCVerbose::connection), LOG_INFO, "state=DO_INITIAL_CLEAR_SCREEN");

    // set almost null cursor, this is the little dot cursor
    this->gd->cached_pointer(PredefinedPointer::Dot);

    this->session_log.log6(LogId::SESSION_ESTABLISHED_SUCCESSFULLY, {});

    Rect const screen_rect(0, 0, this->width, this->height);

    RDPOpaqueRect orect(screen_rect, RDPColor{});
    this->gd->draw(orect, screen_rect, gdi::ColorCtx::from_bpp(this->bpp, this->palette_update_ctx.get_palette()));

    this->state = UP_AND_RUNNING;
    this->front.can_be_start_capture(this->session_log);

    this->session_log.acl_report(AclReport::connect_device_successful());

    this->update_screen(screen_rect, 1);

    LOG_IF(bool(this->verbose & VNCVerbose::connection), LOG_INFO, "VNC screen cleaning ok");

    cliprdr.init_cliprdr_server();
}

void mod_vnc::rdp_input_mouse(uint16_t device_flags, uint16_t x, uint16_t y)
{
    if (this->state != UP_AND_RUNNING) {
        return;
    }

    if (ft_gui.is_open()) [[unlikely]] {
        ft_gui.input_mouse(device_flags, x, y, keymapSym.mods());
        return;
    }

    StaticOutStream<32> out_stream;

    if (device_flags & MOUSE_FLAG_MOVE) {
        this->mouse.move(out_stream, x, y);
    }
    else if (device_flags & MOUSE_FLAG_BUTTON1) {
        this->mouse.click(out_stream, x, y, 1 << 0, device_flags & MOUSE_FLAG_DOWN);
    }
    else if (device_flags & MOUSE_FLAG_BUTTON2) {
        this->mouse.click(out_stream, x, y, 1 << 2, device_flags & MOUSE_FLAG_DOWN);
    }
    else if (device_flags & MOUSE_FLAG_BUTTON3) {
        this->mouse.click(out_stream, x, y, 1 << 1, device_flags & MOUSE_FLAG_DOWN);
    }
    else if (device_flags & MOUSE_FLAG_WHEEL) {
        if (device_flags & MOUSE_FLAG_WHEEL_NEGATIVE) {
            this->mouse.scroll(out_stream, 1 << 4);
        }
        else {
            this->mouse.scroll(out_stream, 1 << 3);
        }
    }
    else if (device_flags & MOUSE_FLAG_HWHEEL) {
        if (device_flags & MOUSE_FLAG_WHEEL_NEGATIVE) {
            this->mouse.scroll(out_stream, 1 << 6);
        }
        else {
            this->mouse.scroll(out_stream, 1 << 5);
        }
    }
    else {
        return ;
    }

    this->t.send(out_stream.get_produced_bytes());
}

void mod_vnc::rdp_input_mouse_ex(uint16_t device_flags, uint16_t x, uint16_t y)
{
    if (ft_gui.is_open()) [[unlikely]] {
        ft_gui.input_mouse_ex(device_flags, x, y);
        return;
    }

    // this->mouse seems that cannot handle extended mouse events, so do nothing
    // TODO requirement: ExtendedMouseButtons Pseudo-encoding
}

void mod_vnc::rdp_input_scancode(KbdFlags flags, Scancode scancode, uint32_t event_time, Keymap const& keymap)
{
    (void)event_time;
    (void)keymap;

    if (this->state != UP_AND_RUNNING) {
        return;
    }

    LOG_IF(bool(this->verbose & VNCVerbose::keymap_stack), LOG_INFO,
        "mod_vnc::rdp_input_scancode(device_flags=0x%x, keycode=0x%x)", flags, scancode);

    auto const keys = this->keymapSym.scancode_to_keysyms(flags, scancode);

    if (ft_gui.is_open()) [[unlikely]] {
        ft_gui.input_scancode(flags, scancode, keymap);
        return;
    }

    using Mod = kbdtypes::KeyMod;

    constexpr auto lmod_accepted = Mod::LCtrl | Mod::LAlt;
    constexpr auto lmod_mask = lmod_accepted | Mod::LMeta | Mod::LShift;

    if (REDEMPTION_UNLIKELY(scancode == Scancode::F7)
     && kbdtypes::is_pressed(flags)
     // ctrl+alt | ctrl+altgr
     && (keymapSym.mods().rmod_as_lmod() & lmod_mask) == lmod_accepted
     && flags_any(ft_flags, FtFlags::FtSupported)
     && cliprdr.has_file_capability()
    )
    {
        // release control keys
        KeymapSym::Keys keys;
        auto mods = keymapSym.mods();
        using Key = KeymapSym::Key;
        if (mods.test(Mod::LCtrl)) keys.push({Key::LCtrl, KeymapSym::VncKeyState::Up});
        if (mods.test(Mod::RCtrl)) keys.push({Key::RCtrl, KeymapSym::VncKeyState::Up});
        if (mods.test(Mod::LAlt)) keys.push({Key::LAlt, KeymapSym::VncKeyState::Up});
        if (mods.test(Mod::RAlt)) keys.push({Key::RAlt, KeymapSym::VncKeyState::Up});
        send_keyevents(keys);

        FT::open_gui(*this);
    }

    this->send_keyevents(keys);
}

void mod_vnc::rdp_input_unicode(KbdFlags flag, uint16_t unicode)
{
    if (this->state != UP_AND_RUNNING) {
        return;
    }

    LOG_IF(bool(this->verbose & VNCVerbose::keymap_stack), LOG_INFO,
        "mod_vnc::rdp_input_unicode(device_flag=0x%x, unicode16=0x%x)",
        underlying_cast(flag), unicode);

    auto keys = this->keymapSym.utf16_to_keysyms(flag, unicode);

    if (ft_gui.is_open()) [[unlikely]] {
        for (auto key : keys) {
            auto flag = (key.down_flag == KeymapSym::VncKeyState::Up)
                ? KbdFlags::Release
                : KbdFlags{};
            ft_gui.input_unicode(flag, key.keysym, keymapSym.mods());
        }
        return;
    }

    this->send_keyevents(keys);
}

void mod_vnc::send_keyevents(KeymapSym::Keys keys)
{
    StaticOutStream<8 * KeymapSym::Keys::max_capacity> stream;

    for (auto key : keys) {
        LOG_IF(bool(verbose & VNCVerbose::keymap_stack), LOG_INFO,
            "keyloop::ksym=%u (0x%x) %s",
            key.keysym, key.keysym, (key.down_flag == KeymapSym::VncKeyState::Up) ? "UP" : "DOWN");

        stream.out_uint8(4);
        stream.out_uint8(underlying_cast(key.down_flag));
        stream.out_clear_bytes(2);
        stream.out_uint32_be(key.keysym);
    }

    this->t.send(stream.get_produced_bytes());
}

void mod_vnc::rdp_input_synchronize(KeyLocks locks)
{
    LOG_IF(bool(this->verbose & VNCVerbose::keymap_stack), LOG_INFO,
        "KeymapSym::synchronize(%04x)", underlying_cast(locks));

    this->send_keyevents(this->keymapSym.reset_mods(locks));
}

void mod_vnc::update_screen(Rect r, uint8_t incr) {
    StaticOutStream<10> stream;
    /* FramebufferUpdateRequest */
    stream.out_uint8(VNC_CS_MSG_FRAME_BUFFER_UPDATE_REQUEST);
    stream.out_uint8(incr);
    stream.out_uint16_be(r.x);
    stream.out_uint16_be(r.y);
    stream.out_uint16_be(r.cx);
    stream.out_uint16_be(r.cy);
    this->t.send(stream.get_produced_bytes());
}

void mod_vnc::rdp_input_invalidate(Rect r) {
    LOG_IF(bool(this->verbose & VNCVerbose::draw_event), LOG_INFO,
        "mod_vnc::rdp_input_invalidate");

    if (this->state != UP_AND_RUNNING) {
        LOG(LOG_INFO, "mod_vnc::rdp_input_invalidate not up and running");
        return;
    }

    Rect r_ = r.intersect(Rect(0, 0, this->width, this->height));

    if (!r_.isempty()) {
        this->update_screen(r_, 0);
    }
}


bool mod_vnc::doTlsSwitch()
{
    auto const anonymous_tls
      = (this->choosenAuth == VeNCRYPT_TLSNone
      || this->choosenAuth == VeNCRYPT_TLSPlain
      || this->choosenAuth == VeNCRYPT_TLSVnc)
        ? AnonymousTls::Yes
        : AnonymousTls::No;

    auto certificate_checker = [this](X509* certificate, STACK_OF(X509)* certificate_chain, char const* addr, int port) {
        auto cert_log = [this](CertificateStatus status, std::string_view error_msg) {
            log_certificate_status(
                this->session_log, status, error_msg,
                bool(this->verbose & VNCVerbose::connection),
                this->do_tls_params.server_cert.notifications
            );
        };

        if (!certificate) {
            cert_log(CertificateStatus::CertError, "no certificate");
            return CertificateResult::Invalid;
        }

        if (this->do_tls_params.server_cert_check_using_ca) {
            if (!this->do_tls_params.ca_certificates.empty()) {
                if (tls_check_ca_signed_certificate(
                        certificate,
                        certificate_chain,
                        cert_log,
                        this->do_tls_params.ca_certificates.c_str(),
                        this->do_tls_params.target_host.c_str()
                    )) {
                    return CertificateResult::Valid;
                }
                else {
                    return CertificateResult::Invalid;
                }
            }

            cert_log(CertificateStatus::CertError, "No CA certificate available");
            throw Error(ERR_TRANSPORT_TLS_NO_CA_CERTIFICATE_AVAILABLE);
        }

        return tls_check_certificate(
            *certificate,
            this->do_tls_params.server_cert.store,
            this->do_tls_params.server_cert.check,
            cert_log,
            this->do_tls_params.certif_path.c_str(),
            "vnc",
            addr,
            port
        )
            ? CertificateResult::Valid
            : CertificateResult::Invalid;
    };

    switch (this->t.get_transport().enable_client_tls(certificate_checker, this->do_tls_params.tls_config, anonymous_tls)) {
        case Transport::TlsResult::WaitExternalEvent:
        case Transport::TlsResult::Want:
            return false;
        case Transport::TlsResult::Fail:
            LOG(LOG_ERR, "mod_vnc::enable_client_tls fail");
            throw Error(ERR_VNC_CONNECTION_ERROR);
        case Transport::TlsResult::Ok:
            return true;
    }

    REDEMPTION_UNREACHABLE();
}


void mod_vnc::draw_event()
{
    bool can_read = true;

    LOG_IF(bool(this->verbose & VNCVerbose::draw_event), LOG_INFO, "vnc::draw_event");

    if (this->tlsSwitch) {
        if (this->doTlsSwitch()) {
            this->tlsSwitch = false;
            can_read = true;

            REDEMPTION_DIAGNOSTIC_PUSH()
            REDEMPTION_DIAGNOSTIC_GCC_IGNORE("-Wswitch-enum")
            switch(this->choosenAuth) {
            case VeNCRYPT_TLSNone:
            case VeNCRYPT_X509None:
                this->state = WAIT_SECURITY_RESULT;
                break;
            case VeNCRYPT_TLSPlain:
            case VeNCRYPT_X509Plain: {
                StaticOutStream<4 + 4 + 256 + 256> ostream;

                ostream.out_uint32_be(strlen(this->username));
                ostream.out_uint32_be(strlen(this->password));
                ostream.out_copy_bytes(this->username, strlen(this->username));
                ostream.out_copy_bytes(this->password, strlen(this->password));

                this->t.send(ostream.get_data(), ostream.get_offset());
                this->state = WAIT_SECURITY_RESULT;
                break;
            }
            case VeNCRYPT_TLSVnc:
            case VeNCRYPT_X509Vnc:
                this->state = WAIT_SECURITY_TYPES_PASSWORD_AND_SERVER_RANDOM;
                break;
            default:
                LOG(LOG_ERR, "auth %d not handled yet", this->choosenAuth);
                break;
            }
            REDEMPTION_DIAGNOSTIC_POP()
        }
        else {
            can_read = false;
        }
    }

    if (can_read) {
        this->server_data_buf.read_from(this->t);
    }

    [[maybe_unused]]
    uint64_t const data_server_before = this->server_data_buf.remaining();

    while (this->draw_event_impl() && !this->tlsSwitch) {
    }

    uint64_t const data_server_after = this->server_data_buf.remaining();

    LOG_IF(bool(this->verbose & VNCVerbose::draw_event), LOG_INFO,
        "Remaining in buffer : %" PRIu64, data_server_after);

    this->front.must_flush_capture();
}

const char *mod_vnc::securityTypeString(int32_t t) {
    static char format[] = "<unknown 0xXXXXXXXX>";

    switch(t) {
    case VNC_AUTH_INVALID: return "invalid";
    case VNC_AUTH_NONE: return "None";
    case VNC_AUTH_VNC: return "VNC";
    case VNC_AUTH_ULTRA: return "Ultra";
    case VNC_AUTH_TIGHT: return "TightVNC";
    case VNC_AUTH_DIFFIE_HELLMAN: return "Diffie-Hellman (unsupported)";
    case VNC_AUTH_APPLE: return "Apple (unsupported)";
    case VNC_AUTH_ULTRA_MsLogonIAuth: return "Ultra MsLogonIAuth";
    case VNC_AUTH_ULTRA_MsLogonIIAuth: return "Ultra MsLogon2Auth";
    case VNC_AUTH_ULTRA_SecureVNCPluginAuth: return "UtraVNC DSM old";
    case VNC_AUTH_ULTRA_SecureVNCPluginAuth_new: return "UtraVNC DSM new";
    case VNC_AUTH_TLS: return "TLS";
    case VNC_AUTH_ULTRA_MS_LOGON: return "Ultra MS-logon";
    case VNC_AUTH_VENCRYPT: return "VeNCrypt";
    case VeNCRYPT_TLSNone: return "TLS none";
    case VeNCRYPT_TLSVnc: return "TLS VNC";
    case VeNCRYPT_TLSPlain: return "TLS plain";
    case VeNCRYPT_X509None: return "X509 none";
    case VeNCRYPT_X509Vnc: return "X509 VNC";
    case VeNCRYPT_X509Plain: return "X509 plain";
    default:
        snprintf(format, sizeof(format), "<unknown 0x%x>", uint32_t(t));
        return format;
    }
}

void mod_vnc::updatePreferedAuth(int32_t authId, VncAuthType &preferedAuth, size_t &preferedAuthIndex)
{
    static VncAuthType preferedAuthTypes[] = {
        VeNCRYPT_X509Plain, VeNCRYPT_X509Vnc, VeNCRYPT_X509None,
        VeNCRYPT_TLSPlain, VeNCRYPT_TLSVnc, VeNCRYPT_TLSNone,
        VNC_AUTH_ULTRA_SecureVNCPluginAuth_new, VNC_AUTH_ULTRA_SecureVNCPluginAuth,
        VNC_AUTH_VENCRYPT, VNC_AUTH_ULTRA_MsLogonIIAuth,
        VNC_AUTH_ULTRA_MS_LOGON, VNC_AUTH_VNC, VNC_AUTH_NONE
    };

    const size_t nauths = std::size(preferedAuthTypes);
    for (size_t i = 0; i < std::min(nauths, preferedAuthIndex); i++) {
        if (preferedAuthTypes[i] == authId) {
            preferedAuth = static_cast<VncAuthType>(authId);
            preferedAuthIndex = i;
            return;
        }
    }
}

bool mod_vnc::readSecurityResult(InStream &s, uint32_t &status, bool &haveReason, std::string &reason, size_t &skipLen) const {
    if (s.in_remain() < 4){
        return false;
    }

    skipLen = 4;
    status = s.in_uint32_be();
    switch(status) {
    case 0: // SUCCESS
        return true;
    case 1:        // Failed
    case 2: {    // too many attempts
        /*   Version 3.8 onwards
         * If unsuccessful, the server sends a string describing the reason for the failure, and then closes the connection:
         * No. of bytes     Type     Description
         *                4     U32     reason-length
         *    reason-length     U8 array     reason-string
         *
         */
        if (this->spokenProtocol >= 3008) {
            haveReason = true;
            if (s.in_remain() < 4) {
                return false;
            }

            uint32_t reasonLen = s.in_uint32_be();
            if (s.in_remain() < reasonLen) {
                return false;
            }

            reason = "";
            reason.append(char_ptr_cast(s.get_current()), reasonLen);
            skipLen = 4 + 4 + reasonLen;
        }
        else {
            haveReason = false;
        }
        return true;
    }
    case 0xffffffff: // UltraVNC continue code
        return true;
    default:
        LOG(LOG_ERR, "unknown auth failed reason 0x%x", status);
        return true;
    }
}


bool mod_vnc::treatVeNCrypt() {
    InStream s(this->server_data_buf.av());

    switch(this->vencryptState) {
    case WAIT_VENCRYPT_VERSION: {
        if (s.in_remain() < 2){
            return false;
        }

        uint8_t major = s.in_uint8();
        uint8_t minor = s.in_uint8();

        if (major != 0 && minor != 2) {
            LOG(LOG_ERR, "unsupported VeNCrypt version %d.%d", major, minor);
            throw Error(ERR_VNC_CONNECTION_ERROR);
        }

        uint8_t clientVersion[2] = {0, 2};
        this->t.send(clientVersion, 2);

        this->server_data_buf.advance(2);
        this->vencryptState = WAIT_VENCRYPT_VERSION_RESPONSE;
        break;
    }

    case WAIT_VENCRYPT_VERSION_RESPONSE: {
        if (s.in_remain() < 1){
            return false;
        }

        uint8_t ack = s.in_uint8();
        if (ack != 0) {
            LOG(LOG_ERR, "server discarded our version");
            throw Error(ERR_VNC_CONNECTION_ERROR);
        }
        this->vencryptState = WAIT_VENCRYPT_SUBTYPES;
        this->server_data_buf.advance(1);
        [[fallthrough]];
    }

    case WAIT_VENCRYPT_SUBTYPES: {
        if (s.in_remain() < 1){
            return false;
        }

        uint8_t nSubTypes = s.in_uint8();
        if (nSubTypes == 0) {
            LOG(LOG_ERR, "no VeNCrypt subtypes");
            throw Error(ERR_VNC_CONNECTION_ERROR);
        }

        if (s.in_remain() / 4 < nSubTypes){
            return false;
        }

        LOG(LOG_DEBUG, "VeNCrypt subtypes:");

        VncAuthType preferedAuth = VNC_AUTH_INVALID;
        size_t preferedAuthIndex = 255;

        for (uint8_t i = 0; i < nSubTypes; i++) {
            uint32_t subtype = s.in_uint32_be();
            bool accept_auth = force_authentication_method == VNC_AUTH_INVALID
                            || subtype == force_authentication_method;
            LOG(LOG_DEBUG, " * %s%s",
                securityTypeString(subtype),
                accept_auth ? "" : " (ignored)");

            if (subtype == VNC_AUTH_VENCRYPT) {
                LOG(LOG_ERR, "VeNCrypt auth type not allowed in VeNCrypt subtypes");
                throw Error(ERR_VNC_CONNECTION_ERROR);
            }

            if (accept_auth) {
                this->updatePreferedAuth(subtype, preferedAuth, preferedAuthIndex);
            }
        }
        this->server_data_buf.advance(1 + nSubTypes * 4);

        LOG(LOG_DEBUG, "selected VeNCrypt security is %s", securityTypeString(preferedAuth));
        if (preferedAuth == VNC_AUTH_INVALID) {
            throw Error(ERR_VNC_CONNECTION_ERROR);
        }

        StaticOutStream<4> outStream;
        outStream.out_uint32_be(static_cast<uint32_t>(preferedAuth));
        this->t.send(outStream.get_produced_bytes());

        this->choosenAuth = preferedAuth;
        this->vencryptState = WAIT_VENCRYPT_AUTH_ANSWER;
        break;
    }
    case WAIT_VENCRYPT_AUTH_ANSWER: {
        REDEMPTION_DIAGNOSTIC_PUSH()
        REDEMPTION_DIAGNOSTIC_GCC_IGNORE("-Wswitch-enum")
        switch(this->choosenAuth ) {
        case VNC_AUTH_NONE:
            this->state = WAIT_SECURITY_RESULT;
            break;
        case VNC_AUTH_VNC:
            this->state = WAIT_SECURITY_TYPES_PASSWORD_AND_SERVER_RANDOM;
            break;

        case VeNCRYPT_TLSNone:
        case VeNCRYPT_TLSPlain:
        case VeNCRYPT_TLSVnc:

        case VeNCRYPT_X509None:
        case VeNCRYPT_X509Plain:
        case VeNCRYPT_X509Vnc: {
            /* only TLS and X509 subtypes have an answer packet */
            if (s.in_remain() < 1){
                return false;
            }
            uint8_t ack = s.in_uint8();
            if (ack != 1) {
                LOG(LOG_ERR, "server not ok with our authType");
                throw Error(ERR_VNC_CONNECTION_ERROR);
            }
            this->server_data_buf.advance(1);
            this->tlsSwitch = !this->doTlsSwitch();
            break;
        }
        default:
            LOG(LOG_ERR, "unknown state");
            throw Error(ERR_VNC_CONNECTION_ERROR);
        }
        REDEMPTION_DIAGNOSTIC_POP()
        break;
    }
    }
    return true;
}


bool mod_vnc::draw_event_impl()
{
    switch (this->state)
    {
    case DO_INITIAL_CLEAR_SCREEN:
        this->initial_clear_screen();
        return false;

    case UP_AND_RUNNING:
        LOG_IF(bool(this->verbose & VNCVerbose::draw_event), LOG_INFO, "state=UP_AND_RUNNING");

        try {
            while (this->up_and_running_ctx.run(this->server_data_buf, *this->gd, *this)) {
                this->up_and_running_ctx.restart();
            }
            // TODO only when data contains a FramebufferUpdate
            this->update_screen(Rect(0, 0, this->width, this->height), 1);
        }
        catch (const Error & e) {
            LOG(LOG_ERR, "VNC Stopped: %s", e.errmsg());
            this->set_mod_signal(BACK_EVENT_NEXT);
            this->front.must_be_stop_capture();
        }
        catch (...) {
            LOG(LOG_ERR, "unexpected exception raised in VNC");
            this->set_mod_signal(BACK_EVENT_NEXT);
            this->front.must_be_stop_capture();
        }

        return false;

    case WAIT_SECURITY_TYPES:
        {
            LOG_IF(bool(this->verbose & VNCVerbose::connection), LOG_INFO,
                "state=WAIT_SECURITY_TYPES");

            size_t const protocol_version_len = 12;

            if (this->server_data_buf.remaining() < protocol_version_len) {
                return false;
            }

            //
            // the buffer is supposed to be
            //      RFB XXX.YYY\n
            // with XXX and YYY being major and minor version
            //
            const uint8_t *rfbString = this->server_data_buf.av().data();

            if (memcmp(rfbString, "RFB ", 4) != 0) {
                LOG(LOG_INFO, "Invalid server handshake");
                throw Error(ERR_VNC_CONNECTION_ERROR);
            }

            if (rfbString[7] != '.') {
                LOG(LOG_INFO, "Invalid server handshake");
                throw Error(ERR_VNC_CONNECTION_ERROR);
            }

            // TODO std::from_chars
            auto versionParser = [](const uint8_t *str, int &v) {
                v = 0;
                for (int i = 0; i < 3; i++) {
                    if (str[i] < '0' || str[i] > '9'){
                        return false;
                    }
                    v = v * 10 + (str[i] - '0');
                }
                return true;
            };

            int major;
            int minor;
            if (!versionParser(&rfbString[4], major) || !versionParser(&rfbString[8], minor)) {
                LOG(LOG_INFO, "Invalid server handshake");
                throw Error(ERR_VNC_CONNECTION_ERROR);
            }
            LOG_IF(bool(this->verbose & VNCVerbose::basic_trace), LOG_INFO,
                "Server Protocol Version=%d.%d", major, minor);

            int serverProtocol = major * 1000 + minor;
            this->spokenProtocol = std::min(maxSpokenVncProcotol, serverProtocol);

            char handshakeAnswer[20];
            snprintf(handshakeAnswer, sizeof(handshakeAnswer), "RFB %.3d.%.3d\n",
                    this->spokenProtocol / 1000, this->spokenProtocol % 1000);
            this->t.send(handshakeAnswer, 12);

            this->server_data_buf.advance(protocol_version_len);

            this->state = WAIT_SECURITY_TYPES_LEVEL;
        }
        [[fallthrough]];

    case WAIT_SECURITY_TYPES_LEVEL:
        {
            InStream s(this->server_data_buf.av());

            if (this->spokenProtocol >= 3007) {
                //
                // in version 3.7 or greater the packet has format
                // uint8         number of security types
                // variable     array of security types
                //
                if (s.in_remain() < 1) {
                    return false;
                }

                uint8_t nAuthTypes = s.in_uint8();
                if (s.in_remain() < nAuthTypes) {
                    return false;
                }

                if (!nAuthTypes) {
                    // 0 authTypes means an error occurred and it's followed by a reason packet:
                    // u32 - reasonLength
                    // u8 array - reason string
                    if (s.in_remain() < 4){
                        return false;
                    }

                    uint32_t reasonLen = s.in_uint32_be();
                    if (s.in_remain() < reasonLen){
                        return false;
                    }

                    std::string reason;
                    reason.append(char_ptr_cast(s.get_current()), reasonLen);

                    LOG(LOG_INFO, "connection failed, reason=%s", reason.c_str());
                    throw Error(ERR_VNC_CONNECTION_ERROR);
                }

                VncAuthType preferedAuth = VNC_AUTH_INVALID;
                size_t preferedAuthIndex = 255;

                LOG_IF(bool(this->verbose & VNCVerbose::basic_trace), LOG_INFO,
                       "got %d security types:", nAuthTypes);

                for (size_t i = 0; i < nAuthTypes; i++) {
                    VncAuthType authType = static_cast<VncAuthType>(s.in_uint8());
                    bool accept_auth = force_authentication_method == VNC_AUTH_INVALID
                                    || authType == VNC_AUTH_VENCRYPT
                                    || authType == force_authentication_method;
                    LOG_IF(bool(this->verbose & VNCVerbose::basic_trace), LOG_INFO,
                           "* %s%s", securityTypeString(authType),
                           accept_auth ? "" : " (ignored)");

                    if (accept_auth) {
                        this->updatePreferedAuth(authType, preferedAuth, preferedAuthIndex);
                    }
                }
                LOG_IF(bool(this->verbose & VNCVerbose::basic_trace), LOG_INFO,
                       "%s security choosen", securityTypeString(preferedAuth));

                this->server_data_buf.advance(1 + nAuthTypes);

                this->choosenAuth = preferedAuth;
                uint8_t authAnswer = static_cast<uint8_t>(preferedAuth);
                this->t.send(bytes_view{&authAnswer, 1});

                REDEMPTION_DIAGNOSTIC_PUSH()
                REDEMPTION_DIAGNOSTIC_GCC_IGNORE("-Wswitch-enum")
                switch (preferedAuth){
                    case VNC_AUTH_INVALID:
                        this->state = WAIT_SECURITY_TYPES_INVALID_AUTH;
                        break;
                    case VNC_AUTH_NONE:
                        this->state = WAIT_SECURITY_RESULT;
                        break;
                    case VNC_AUTH_VNC:
                        this->state = WAIT_SECURITY_TYPES_PASSWORD_AND_SERVER_RANDOM;
                        break;
                    case VNC_AUTH_ULTRA_MS_LOGON:
                        this->state = WAIT_SECURITY_TYPES_MS_LOGON;
                        break;
                    case VNC_AUTH_ULTRA_MsLogonIAuth:
                        LOG(LOG_ERR, "MsLogonIAuth not supported");
                        throw Error(ERR_VNC_CONNECTION_ERROR);
                    case VNC_AUTH_ULTRA_MsLogonIIAuth:
                        this->state = WAIT_SECURITY_TYPES_MS_LOGON;
                        break;
                    case VNC_AUTH_VENCRYPT:
                        this->state = DO_VENCRYPT_HANDSHAKE;
                        break;
                    case VNC_AUTH_ULTRA_SecureVNCPluginAuth:
                    case VNC_AUTH_ULTRA_SecureVNCPluginAuth_new:
                        this->state = WAIT_SECURITY_ULTRA_CHALLENGE;
                        break;
                    default:
                        LOG(LOG_ERR, "internal bug when computing prefered VNC auth");
                        throw Error(ERR_VNC_CONNECTION_ERROR);
                }
                REDEMPTION_DIAGNOSTIC_POP()
            } else {
                // version 3.3 or less, the server decides the security type that
                // is used
                if (s.in_remain() < 4) {
                    return false;
                }
                int32_t security_type = s.in_sint32_be();
                this->server_data_buf.advance(4);

                LOG_IF(bool(this->verbose & VNCVerbose::basic_trace), LOG_INFO,
                    "security level is %d (1 = none, 2 = standard, -6 = mslogon)",
                    security_type);

                switch (security_type) {
                    case VNC_AUTH_INVALID:
                        this->state = WAIT_SECURITY_TYPES_INVALID_AUTH;
                        break;
                    case VNC_AUTH_NONE:
                        this->state = SERVER_INIT;
                        break;
                    case VNC_AUTH_VNC:
                        this->state = WAIT_SECURITY_TYPES_PASSWORD_AND_SERVER_RANDOM;
                        break;
                    case VNC_AUTH_ULTRA_MS_LOGON:
                        this->state = WAIT_SECURITY_TYPES_MS_LOGON;
                        break;
                    case VNC_AUTH_VENCRYPT:
                        this->state = DO_VENCRYPT_HANDSHAKE;
                        break;
                    case VNC_AUTH_ULTRA_SecureVNCPluginAuth_new:
                        this->state = WAIT_SECURITY_ULTRA_CHALLENGE;
                        break;
                    default:
                        LOG(LOG_ERR, "vnc unexpected security level");
                        throw Error(ERR_VNC_CONNECTION_ERROR);
                }

            }
        }
        return true;

    case WAIT_SECURITY_RESULT: {
        uint32_t status;
        bool haveReason = false;
        std::string reason;
        size_t skipLen;
        InStream s(this->server_data_buf.av());

        if (!this->readSecurityResult(s, status, haveReason, reason, skipLen)){
            return false;
        }

        VncState nextState;
        switch(status) {
        case SECURITY_REASON_OK:
            nextState = SERVER_INIT;
            break;
        case SECURITY_REASON_FAILED:
        case SECURITY_REASON_TOO_MANY_ATTEMPTS: {
            const char *authErrorStr = (status == SECURITY_REASON_FAILED) ? "failed" : "failed (too many attempts)";
            if (!haveReason){
                reason = "<no reason>";
            }

            LOG(LOG_ERR, "vnc auth %s, reason=%s", authErrorStr, reason.c_str());
            throw Error(ERR_VNC_CONNECTION_ERROR);
        }
        case SECURITY_REASON_CONTINUE:
            nextState = WAIT_SECURITY_TYPES_LEVEL;
            break;

        default:
            LOG(LOG_ERR, "unknown auth failed reason 0x%x", status);
            throw Error(ERR_VNC_CONNECTION_ERROR);
        }

        this->state = nextState;
        this->server_data_buf.advance(skipLen);
        return true;
    }

    case DO_VENCRYPT_HANDSHAKE:
        return this->treatVeNCrypt();

    case WAIT_SECURITY_TYPES_PASSWORD_AND_SERVER_RANDOM:
        LOG_IF(bool(this->verbose & VNCVerbose::basic_trace), LOG_INFO,
            "Receiving VNC Server Random");

        {
            if (!this->password_ctx.run(this->server_data_buf)) {
                return false;
            }
            this->password_ctx.restart();

            char key[8] = {};

            // key is simply password padded with nulls
            strncpy(key, char_ptr_cast(byte_ptr_cast(this->password)), 8);

            RfbD3DesEncrypter encrypter(to_bounded_u8_av(make_bounded_array_view(key)));
            auto encrypt_block = [&](auto bav){ encrypter.encrypt_block(bav, bav); };

            auto random_buf = writable_sized_array_view<uint8_t, 16>::assumed(
                this->password_ctx.server_random);

            encrypt_block(random_buf.drop_back<8>());
            encrypt_block(random_buf.drop_front<8>());

            LOG_IF(bool(this->verbose & VNCVerbose::basic_trace), LOG_INFO, "Sending Password");
            this->t.send(random_buf);
        }
        this->state = WAIT_SECURITY_TYPES_PASSWORD_AND_SERVER_RANDOM_RESPONSE;
        [[fallthrough]];

    case WAIT_SECURITY_TYPES_PASSWORD_AND_SERVER_RANDOM_RESPONSE:
        {
            // sec result
            LOG_IF(bool(this->verbose & VNCVerbose::basic_trace), LOG_INFO, "Waiting for password ack");

            if (!this->auth_response_ctx.run(this->server_data_buf, [this](bool status, bytes_view bytes){
                if (status) {
                    LOG_IF(bool(this->verbose & VNCVerbose::basic_trace), LOG_INFO, "vnc password ok");
                }
                else {
                    LOG(LOG_INFO, "vnc password failed. Reason: %.*s",
                        int(bytes.size()), bytes.as_charp());
                    throw Error(ERR_VNC_CONNECTION_ERROR);
                }
            })) {
                return false;
            }
            this->auth_response_ctx.restart();

            this->state = SERVER_INIT;
        }
        return true;

    case WAIT_SECURITY_TYPES_MS_LOGON:
        {
            LOG(LOG_INFO, "VNC MS-LOGON Auth");

            if (!this->ms_logon(this->server_data_buf)) {
                return false;
            }
            this->ms_logon_ctx.restart();
        }
        this->state = WAIT_SECURITY_TYPES_MS_LOGON_RESPONSE;
        [[fallthrough]];

    case WAIT_SECURITY_TYPES_MS_LOGON_RESPONSE:
        {
            LOG_IF(bool(this->verbose & VNCVerbose::basic_trace), LOG_INFO, "Waiting for password ack");

            if (!this->auth_response_ctx.run(this->server_data_buf, [this](bool status, bytes_view bytes){
                if (status) {
                    LOG_IF(bool(this->verbose & VNCVerbose::basic_trace), LOG_INFO, "MS LOGON password ok");
                }
                else {
                    LOG(LOG_INFO, "MS LOGON password FAILED. Reason: %.*s",
                        int(bytes.size()), bytes.as_charp());
                }
            })) {
                return false;
            }
            this->auth_response_ctx.restart();
        }
        this->state = SERVER_INIT;
        return true;

    case WAIT_SECURITY_TYPES_INVALID_AUTH:
        {
            LOG(LOG_ERR, "VNC INVALID Auth");

            if (!this->invalid_auth_ctx.run(this->server_data_buf, [](bytes_view av){
                hexdump_c(av);
            })) {
                return false;
            }
            this->invalid_auth_ctx.restart();

            throw Error(ERR_VNC_CONNECTION_ERROR);
            // return true;
        }

    case WAIT_SECURITY_ULTRA_CHALLENGE: {
        if (!this->dsm){
            this->dsm = std::make_unique<UltraDSM>(/*this->password*/);
        }

        InStream challenge(this->server_data_buf.av());
        uint16_t challengeLen;
        uint8_t passPhraseUsed;
        if (!this->dsm->handleChallenge(challenge, challengeLen, passPhraseUsed)){
            return false;
        }

        this->server_data_buf.advance(challengeLen + 1 + 2);

        StaticOutStream<2> lenStream;
        StaticOutReservedStreamHelper<2, 65535> out;
        OutStream &outPacket = out.get_data_stream();
        this->dsm->getResponse(outPacket);

        lenStream.out_uint16_le(outPacket.get_offset());
        out.copy_to_head(lenStream.get_produced_bytes());
        writable_bytes_view packet = out.get_packet();
        this->t.send(packet.begin(), packet.size());

        this->dsmEncryption = true;

        if (passPhraseUsed != 2) {
            lenStream.rewind(0);
            uint16_t passLen = strlen(this->password);
            lenStream.out_uint16_le(passLen);

            this->t.send(lenStream.get_data(), 2);
            this->t.send(this->password, passLen);
        }
        this->state = WAIT_SECURITY_RESULT;
        break;
    }

    case SERVER_INIT:
        this->t.send("\x01", 1); // share flag
        this->state = SERVER_INIT_RESPONSE;
        [[fallthrough]];

    case SERVER_INIT_RESPONSE:
        {
            if (!this->server_init_ctx.run(this->server_data_buf, *this)) {
                return false;
            }
            this->server_init_ctx.restart();
        }

        // should be connected

        {
        // 7.4.1   SetPixelFormat
        // ----------------------
        // Sets the format in which pixel values should be sent in
        // FramebufferUpdate messages. If the client does not send
        // a SetPixelFormat message then the server sends pixel values
        // in its natural format as specified in the ServerInit message
        // (ServerInit).

        // If true-colour-flag is zero (false) then this indicates that
        // a "colour map" is to be used. The server can set any of the
        // entries in the colour map using the SetColourMapEntries
        // message (SetColourMapEntries). Immediately after the client
        // has sent this message the colour map is empty, even if
        // entries had previously been set by the server.

        // Note that a client must not have an outstanding
        // FramebufferUpdateRequest when it sends SetPixelFormat
        // as it would be impossible to determine if the next *
        // FramebufferUpdate is using the new or the previous pixel
        // format.

            StaticOutStream<20> stream;
            // Set Pixel format
            stream.out_uint8(0);

            // Padding 3 bytes
            stream.out_uint8(0);
            stream.out_uint8(0);
            stream.out_uint8(0);

            // VNC pixel_format capabilities
            // -----------------------------
            // bits per pixel  : 1 byte
            // color depth     : 1 byte
            // endianess       : 1 byte (0 = LE, 1 = BE)
            // true color flag : 1 byte
            // red max         : 2 bytes
            // green max       : 2 bytes
            // blue max        : 2 bytes
            // red shift       : 1 bytes
            // green shift     : 1 bytes
            // blue shift      : 1 bytes
            // padding         : 3 bytes

            // 8 bpp
            // -----
            // "\x08\x08\x00"
            // "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
            // "\0\0\0"

            // 15 bpp
            // ------
            // "\x10\x0F\x00"
            // "\x01\x00\x1F\x00\x1F\x00\x1F\x0A\x05\x00"
            // "\0\0\0"

            // 24 bpp
            // ------
            // "\x20\x18\x00"
            // "\x01\x00\xFF\x00\xFF\x00\xFF\x10\x08\x00"
            // "\0\0\0"

            // 16 bpp
            // ------
            // "\x10\x10\x00"
            // "\x01\x00\x1F\x00\x2F\x00\x1F\x0B\x05\x00"
            // "\0\0\0"

            // const char * pixel_format = "\x20\x18\x00"
            //                             "\x01\x00\xFF\x00\xFF\x00\xFF\x10\x08\x00"
            //                             "\0\0\0" ;


            const char * pixel_format =
                "\x10" // bits per pixel  : 1 byte =  16
                "\x10" // color depth     : 1 byte =  16
                "\x00" // endianess       : 1 byte =  LE
                "\x01" // true color flag : 1 byte = yes
                "\x00\x1F" // red max     : 2 bytes = 31
                "\x00\x3F" // green max   : 2 bytes = 63
                "\x00\x1F" // blue max    : 2 bytes = 31
                "\x0B" // red shift       : 1 bytes = 11
                "\x05" // green shift     : 1 bytes =  5
                "\x00" // blue shift      : 1 bytes =  0
                "\0\0\0"; // padding      : 3 bytes

            stream.out_copy_bytes(pixel_format, 16);
            this->t.send(stream.get_produced_bytes());

            this->bpp = BitsPerPixel{16};
            this->depth  = 16;
            this->endianess = 0;
            this->true_color_flag = 1;
            this->red_max       = 0x1F;
            this->green_max     = 0x3F;
            this->blue_max      = 0x1F;
            this->red_shift     = 0x0B;
            this->green_shift   = 0x05;
            this->blue_shift    = 0;
        }

        // 7.4.2   SetEncodings
        // --------------------

        // Sets the encoding types in which pixel data can be sent by
        // the server. The order of the encoding types given in this
        // message is a hint by the client as to its preference (the
        // first encoding specified being most preferred). The server
        // may or may not choose to make use of this hint. Pixel data
        // may always be sent in raw encoding even if not specified
        // explicitly here.

        // In addition to genuine encodings, a client can request
        // "pseudo-encodings" to declare to the server that it supports
        // certain extensions to the protocol. A server which does not
        // support the extension will simply ignore the pseudo-encoding.
        // Note that this means the client must assume that the server
        // does not support the extension until it gets some extension-
        // -specific confirmation from the server.

        // See Encodings for a description of each encoding and Pseudo-encodings for the meaning of pseudo-encodings.

        // No. of bytes     Type     [Value]     Description
        // -------------+---------+-----------+---------------------
        //         1    |    U8   |     2     |  message-type
        //         1    |         |           |    padding
        //         2    |    U16  |           | number-of-encodings
        //
        // followed by number-of-encodings repetitions of the following:
        //
        // No. of bytes     Type     Description
        // ----------------------------------------
        //         4         S32     encoding-type

        {
            // SetEncodings
            StaticOutStream<32768> stream;

            bool support_zrle_encoding          = true;
            bool support_hextile_encoding       = false;
            bool support_rre_encoding           = false;
            bool support_raw_encoding           = true;
            bool support_copyrect_encoding      = true;
            bool support_cursor_pseudo_encoding = this->cursor_pseudo_encoding_supported;
            bool support_pointer_position       = false;
            bool support_file_transfer          = true; // TODO

            char const * p = this->encodings.c_str();
            if (*p){
                support_zrle_encoding = false;
                for (;;){
                    while (*p == ','){
                        ++p;
                    }

                    auto res = string_to_int<int32_t>(p);
                    if (res.ec != std::errc()) {
                        break;
                    }
                    p = res.ptr;

                    switch (res.val) {
                        case HEXTILE_ENCODING: support_hextile_encoding = true; break;
                        case ZRLE_ENCODING: support_zrle_encoding = true; break;
                        case RRE_ENCODING: support_rre_encoding = true; break;
                        case POINTER_POSITION_ENCODING: support_pointer_position = true; break;
                        default: break;
                    }
                }
            }
            else {
                support_hextile_encoding       = true;
                support_rre_encoding           = true;
            }

            uint16_t number_of_encodings = support_zrle_encoding
                                         + support_hextile_encoding
                                         + support_raw_encoding
                                         + support_copyrect_encoding
                                         + support_rre_encoding
                                         + support_cursor_pseudo_encoding
                                         + support_file_transfer
                                         + support_pointer_position
                                         ;

            // LOG(LOG_INFO, "number of encodings=%d", number_of_encodings);

            stream.out_uint8(VNC_CS_MSG_SET_ENCODINGS);
            stream.out_uint8(0);
            stream.out_uint16_be(number_of_encodings);
            if (support_zrle_encoding) {
                LOG(LOG_INFO, "enable ZRLE encoding");
                stream.out_uint32_be(ZRLE_ENCODING);
            }
            if (support_hextile_encoding) {
                LOG(LOG_INFO, "enable hextile encoding");
                stream.out_uint32_be(HEXTILE_ENCODING);
            }
            if (support_raw_encoding) {
                LOG(LOG_INFO, "enable RAW encoding");
                stream.out_uint32_be(RAW_ENCODING);
            }
            if (support_copyrect_encoding) {
                LOG(LOG_INFO, "enable copyrect encoding");
                stream.out_uint32_be(COPYRECT_ENCODING);
            }
            if (support_rre_encoding) {
                LOG(LOG_INFO, "enable rre encoding");
                stream.out_uint32_be(RRE_ENCODING);
            }
            if (support_cursor_pseudo_encoding) {
                LOG(LOG_INFO, "enable cursor pseudo encoding");
                stream.out_uint32_be(CURSOR_PSEUDO_ENCODING);
            }
            if (support_pointer_position) {
                LOG(LOG_INFO, "enable pointer position encoding");
                stream.out_uint32_be(POINTER_POSITION_ENCODING);
            }
            if (support_file_transfer) {
                LOG(LOG_INFO, "enable file transfer encoding");
                stream.out_uint32_be(UVNC_FILE_TRANSFER);
            }

            this->t.send(stream.get_produced_bytes());
        }


        switch (this->front.server_resize({align4(this->width), this->height, this->bpp})){
        case FrontAPI::ResizeResult::remoteapp:
        case FrontAPI::ResizeResult::remoteapp_wait_response:
            LOG_IF(bool(this->verbose & VNCVerbose::basic_trace), LOG_INFO, "resizing remoteapp");
            if (this->rail_client_execute) {
                this->rail_client_execute->adjust_window_to_mod();
            }
            // RZ: Continue with FrontAPI::ResizeResult::no_need
            [[fallthrough]];
        case FrontAPI::ResizeResult::no_need:
        case FrontAPI::ResizeResult::instant_done:
            LOG_IF(bool(this->verbose & VNCVerbose::basic_trace), LOG_INFO, "no resizing needed");
            // no resizing needed
            this->state = DO_INITIAL_CLEAR_SCREEN;
            break;
        case FrontAPI::ResizeResult::wait_response:
            LOG_IF(bool(this->verbose & VNCVerbose::basic_trace), LOG_INFO, "resizing done");
            // resizing done
            this->state = WAIT_CLIENT_UP_AND_RUNNING;
            break;
        case FrontAPI::ResizeResult::fail:
            // resizing failed
            // thow an Error ?
            LOG(LOG_ERR, "Older RDP client can't resize resolution from server, disconnecting");
            throw Error(ERR_VNC_OLDER_RDP_CLIENT_CANT_RESIZE);
        }
        return true;

    case WAIT_CLIENT_UP_AND_RUNNING:
        LOG(LOG_INFO, "Waiting for client become up and running");
        break;
    }

    return false;
}

bool mod_vnc::lib_clip_data(Buf64k & buf)
{
    auto result = server_cut_text_reader.read_packet(buf);

    // TODO disabled when ft_gui.is_open() ?
    cliprdr.process_vnc_server_cut_text_message(
        result.partial_data,
        server_cut_text_reader.remaining_len(),
        result.chunk_flags
    );

    // TODO auto-open gui and request_file_list()

    return flags_any(result.chunk_flags, Rfb::ChunkFlags::Last);
}

bool mod_vnc::consume_file_transfer_packet(Buf64k& buf)
{
    LOG(LOG_DEBUG, "mod_vnc::consume_file_transfer_packet");

    auto status = ft_reader.read_packet(
        buf,
        FT::make_uvnc_file_transfer_reader_callback(*this)
    );

    LOG(LOG_DEBUG, "ft status = %d", status);
    return status == UVNCFileTransferReader::ReadPacketStatus::Completed;
}

void mod_vnc::send_to_cliprdr(
    bytes_view chunk, size_t total_length, VNC::ChannelFlags flags)
{
    if (bool(this->verbose & (VNCVerbose::clipboard | VNCVerbose::clipboard_dump))) [[unlikely]] {
        LOG(LOG_INFO, "mod_vnc::send_to_cliprdr (%savailable) (%zu/%zu bytes) flags=<%s>(%u)",
            cliprdr_chann ? "" : "un", chunk.size(), total_length,
            VNC::channel_flags_to_string(flags), flags);

        if (bool(this->verbose & VNCVerbose::clipboard_dump)) {
            LOG(LOG_INFO, "mod_vnc::send_to_cliprdr: dump vvvvvv");
            hexdump_c(chunk);
            LOG(LOG_INFO, "mod_vnc::send_to_cliprdr: dumped ^^^^");
        }
    }

    if (!cliprdr_chann) {
        return ;
    }

    // Send clipboard as a train of consecutive PDU

    constexpr size_t max_pdu_len = CHANNELS::CHANNEL_CHUNK_LENGTH;

    size_t remaining_pdu_data_length = chunk.size();
    uint8_t const * chunk_data = chunk.data();

    auto channel_flags = (VNC::ChannelFlags::First & flags) | VNC::ChannelFlags::ShowProtocol;

    do
    {
        const auto chunk_size = mmin(max_pdu_len, remaining_pdu_data_length);

        remaining_pdu_data_length -= chunk_size;

        if (!remaining_pdu_data_length) {
            channel_flags |= flags & VNC::ChannelFlags::Last;
        }

        front.send_to_channel(
            *cliprdr_chann,
            {chunk_data, chunk_size},
            total_length,
            underlying_cast(channel_flags)
        );

        channel_flags = VNC::ChannelFlags::ShowProtocol;

        chunk_data += chunk_size;
    }
    while (remaining_pdu_data_length);
}

void mod_vnc::send_to_cliprdr(bytes_view pdu)
{
    auto flags = VNC::ChannelFlags::First
               | VNC::ChannelFlags::Last
               | VNC::ChannelFlags::ShowProtocol;
    send_to_cliprdr(pdu, pdu.size(), flags);
}

void mod_vnc::send_to_mod_channel(
    CHANNELS::ChannelNameId front_channel_name,
    InStream & chunk,
    size_t total_length,
    uint32_t flags)
{
    LOG_IF(bool(this->verbose & VNCVerbose::basic_trace),
        LOG_INFO, "mod_vnc::send_to_mod_channel | ft_flags=0x%x", ft_flags);

    if (this->state != UP_AND_RUNNING) {
        return;
    }

    if (front_channel_name == channel_names::cliprdr) {
        VNC::ChannelFlags channel_flags {};
        channel_flags |= (flags & CHANNELS::CHANNEL_FLAG_FIRST)
            ? VNC::ChannelFlags::First
            : VNC::ChannelFlags::NoFlags;
        channel_flags |= (flags & CHANNELS::CHANNEL_FLAG_LAST)
            ? VNC::ChannelFlags::Last
            : VNC::ChannelFlags::NoFlags;
        cliprdr.process_rdp_client_message(
            chunk.remaining_bytes(),
            total_length,
            channel_flags
        );

        // enable/disable file list
        if (cliprdr.rdp_client_last_msg_type() == VNC::CbMsgType::FormatList
         && flags_any(channel_flags, VNC::ChannelFlags::Last)
         && ft_gui.is_open())
        {
            FT::update_file_group_descriptor(*this);
            // TODO add clip support for no stop transfer
            FT::send_transfer_result(*this, uvnc_file_list.stop_file_transfer());
        }
    }
}

void mod_vnc::rdp_gdi_up_and_running()
{
    if (this->state == WAIT_CLIENT_UP_AND_RUNNING){
        this->state = DO_INITIAL_CLEAR_SCREEN;
        this->initial_clear_screen();
    }
}

void mod_vnc::disconnect()
{
    auto delay = this->events_guard.get_monotonic_time_since_epoch()
                - this->session_time_start;
    long seconds = std::chrono::duration_cast<std::chrono::seconds>(delay).count();

    LOG(LOG_INFO, "Client disconnect from VNC module");

    char duration_str[128];
    int len = snprintf(duration_str, sizeof(duration_str), "%ld:%02ld:%02ld",
        seconds / 3600, (seconds % 3600) / 60, seconds % 60);

    this->session_log.log6(LogId::SESSION_DISCONNECTION,
        {KVLog("duration"_av, {duration_str, std::size_t(len)}),});

    LOG_IF(bool(this->verbose & VNCVerbose::basic_trace), LOG_INFO,
        "type=SESSION_DISCONNECTION duration=%s", duration_str);

    FT::disconnect(*this);
}

void mod_vnc::file_validator_receive_event()
{
    auto file_validator = transfered_file_ctx.file_validator;

    if (!file_validator)
    {
        return ;
    }

    auto receive_data = [&]{
        for (;;)
        {
            switch (file_validator->receive_response())
            {
                case FileValidatorService::ResponseType::WaitingData:
                    return false;
                case FileValidatorService::ResponseType::HasContent:
                    return true;
                case FileValidatorService::ResponseType::Error:
                    ;
            }
        }
    };

    using ValidationResult = LocalFileValidatorProtocol::ValidationResult;

    while (receive_data())
    {
        bool is_accepted = false;

        switch (file_validator->last_result_flag())
        {
            case ValidationResult::Wait:
                return;
            case ValidationResult::IsAccepted:
                is_accepted = true;
                [[fallthrough]];
            case ValidationResult::IsRejected:
            case ValidationResult::Error:
                ;
        }

        LOG_IF(
            flags_any(verbose, VNCVerbose::clipboard),
            LOG_INFO, "DLPAV response: is_accepted=%d", is_accepted
        );

        auto file_validator_id = file_validator->last_file_id();
        auto & result_content = file_validator->get_content();

        auto * file = [&, this] () -> TransferedFileCtx::File *
        {
            if (transfered_file_ctx.current_file.validator_id == file_validator_id)
            {
                if (file_validator_id != FileValidatorId{})
                {
                    return &transfered_file_ctx.current_file;
                }
                return nullptr;
            }

            for (auto & file : transfered_file_ctx.waiting_validator_file_list)
            {
                if (file.validator_id == file_validator_id)
                {
                    return &file;
                }
            }

            return nullptr;
        }();

        LOG_IF(
            flags_any(verbose, VNCVerbose::clipboard),
            LOG_INFO, "DLPAV file: found=%d, validator_id=%u file_id=%lu",
            !!file,
            file ? file->validator_id : FileValidatorId{},
            (file && file->tfl_file) ? file->tfl_file->file_id : FdxCapture::FileId{}
        );

        // when id is already released
        if (!file) [[unlikely]]
        {
            continue;
        }

        file->validator_id = {};

        if (!is_accepted || transfered_file_ctx.log_if_accepted) {
            FT::file_verification_log(*this, *file, result_content);
        }

        auto target = file->validator_target;

        bool file_is_blocked = !is_accepted
                            && flags_any(transfered_file_ctx.block_invalid_file, target);

        if (file_is_blocked)
        {
            session_log.log6(LogId::FILE_BLOCKED, {
                KVLog("direction"_av, FT::target_to_direction_name(target)),
                KVLog("file_name"_av, file->utf8_file_name.av()),
            });
        }

        if (file->tfl_file)
        {
            REDEMPTION_ASSUME(transfered_file_ctx.fdx_capture);

            file->validator_status = is_accepted
                ? TransferValidatorStatus::IsOk
                : TransferValidatorStatus::IsRejected;

            // if file is in waiting_validator_file_list
            bool file_is_complete = (file != &transfered_file_ctx.current_file);

            if (is_accepted
             && transfered_file_ctx.file_storage_option == FileStorageOption::OnInvalidFile)
            {
                FT::cancel_tfl(transfered_file_ctx, *file);
            }
            else if (file_is_blocked
             || (file_is_complete
                && (transfered_file_ctx.file_storage_option == FileStorageOption::Always
                    || !is_accepted
                ))
            )
            {
                if (!file_is_complete)
                {
                    FT::finalize_sha2_sig(transfered_file_ctx);
                }

                FT::close_tfl(
                    transfered_file_ctx,
                    *file,
                    file_is_complete
                        ? Mwrm3::TransferedStatus::Completed
                        : Mwrm3::TransferedStatus::Broken
                );
            }

            // remove file in waiting list
            if (!file->tfl_file && file_is_complete)
            {
                auto & file_list = transfered_file_ctx.waiting_validator_file_list;
                if (file != &file_list.back())
                {
                    auto i = checked_int{ file - file_list.data() };
                    file_list[i] = std::move(file_list.back());
                }
                file_list.pop_back();
            }
        }

        if (file_is_blocked)
        {
            FT::send_transfer_result(*this, uvnc_file_list.stop_file_transfer());
        }
    }
}
