/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "core/events.hpp"
#include "keyboard/key_mod_flags.hpp"
#include "translation/translation.hpp"
#include "utils/monotonic_clock.hpp"
#include "utils/sugar/bounded_bytes_view.hpp"
#include "mod/vnc/file_transfer/uvnc_drive_type.hpp"
#include "mod/vnc/file_transfer/vnc_file.hpp"


class FontCharView;
class Keymap;
class Font;

namespace gdi
{
    class GraphicApi;
}

namespace VNC
{

class FileTransferGui
{
    struct FileData;

public:
    using MaxFileIntType = uint32_t;

    struct SelectedVncFiles
    {
        struct Sentinel;

        struct Iterator
        {
            Iterator(FileData const * begin, FileData const * end) noexcept
                : m_begin(begin)
                , m_end(end)
            {}

            bool operator==(Sentinel const&) const noexcept
            {
                return m_begin == m_end;
            }

            Iterator & operator++() noexcept;
            UVncFile operator*() const noexcept;

        private:
            FileData const * m_begin;
            FileData const * m_end;
        };

        struct Sentinel
        {
            bool operator==(Iterator const& it) const noexcept
            {
                return it == *this;
            }
        };

        SelectedVncFiles(Iterator it, MaxFileIntType selection_counter) noexcept
            : m_it(it)
            , m_size(selection_counter)
        {}

        MaxFileIntType size() const noexcept
        {
            return m_size;
        }

        Iterator begin() const noexcept
        {
            return m_it;
        }

        Sentinel end()
        {
            return Sentinel{};
        }

    private:
        Iterator m_it;
        MaxFileIntType m_size;
    };


    struct Callbacks
    {
        void * ctx;
        void (*close_gui)(void * ctx);
        bool (*open_dir)(void * ctx, UVncFile::PathView path);
        void (*copy_cb_to_vnc)(void * ctx);
        void (*copy_vnc_to_cb)(void * ctx, SelectedVncFiles selected_files);
        void (*stop_transfer)(void * ctx);
    };

    enum class TransferOptions : uint8_t
    {
        None,
        CbToVnc = 1 << 0,
        VncToCb = 1 << 1,
    };
    REDEMPTION_DECLARE_ENUM_FLAG_OPS(friend, TransferOptions)


    FileTransferGui(
        gdi::GraphicApi & gd, Font const & font, EventContainer & events,
        MaxFileIntType max_file_list, TransferOptions transfer_opts,
        Translator tr, Callbacks callbacks
    ) noexcept;

    ~FileTransferGui();

    void open(uint16_t width, uint16_t height, uint16_t mouse_x, uint16_t mouse_y);
    void close() noexcept;

    bool is_open() const noexcept;

    UVncFile::PathView current_directory() const noexcept;

    void input_mouse(uint16_t device_flags, uint16_t x, uint16_t y, kbdtypes::KeyModFlags mods);
    void input_mouse_ex(uint16_t pointer_flag, uint16_t x, uint16_t y);
    void input_scancode(kbdtypes::KbdFlags flags, kbdtypes::Scancode scancode, Keymap const & keymap);
    void input_unicode(kbdtypes::KbdFlags flags, uint32_t unicode, kbdtypes::KeyModFlags mods);

    void refresh(Rect clip);

    using DriveType = UVNC::FileTransfer::DriveType;

    // TODO check is_open() for each function

    // VNC server part
    //@{
    void server_vnc_file_disabled();
    void server_vnc_file_list_error();
    void server_vnc_file_list_start(UVncFile::PathView dir_name);
    bool server_vnc_file_list_add(UVncFile file);
    void server_vnc_file_list_add_drive(uint8_t letter, DriveType type);
    void server_vnc_file_list_add_shorcuts();
    void server_vnc_file_list_end();
    //@}

    // RDP Clipboard part
    //@{
    void client_cb_file_list_reset();
    void client_cb_file_list_requested();
    void client_cb_file_list_start(uint32_t total_file);
    void client_cb_file_list_set_nb_item(uint32_t nb_file);
    void client_cb_file_list_end();
    //@}

    // Proxy part
    //@{
    void vnc_to_rdp_file_list_start();
    void vnc_to_rdp_file_list_ready();
    //@}

    // transfer progress messages
    //@{
    enum class Direction : bool
    {
        CbToVnc,
        VncToCb,
    };

    struct TransferData
    {
        uint32_t items;
        uint64_t bytes;
    };

    struct Progression
    {
        enum class State : uint8_t
        {
            InProgress,
            Completed,
            Aborted,
            Error,
        };

        State state;
        uint32_t items;
        uint64_t bytes;
        int64_t total_bytes_adjust;
        UVncFile::PathView path_error {};

        static Progression next_item() noexcept
        {
            return {
                .state = State::InProgress,
                .items = 1,
                .bytes {},
                .total_bytes_adjust {},
                .path_error = UVncFile::PathView{},
            };
        }

        static Progression error(UVncFile::PathView path) noexcept
        {
            return {
                .state = State::Error,
                .items {},
                .bytes {},
                .total_bytes_adjust {},
                .path_error = path,
            };
        }

        static Progression abort() noexcept
        {
            return {
                .state = State::Aborted,
                .items {},
                .bytes {},
                .total_bytes_adjust {},
                .path_error = UVncFile::PathView{},
            };
        }
    };

    void transfer_start(Direction direction, TransferData total);
    void transfer_progression(Progression progression);
    //@}

private:
    struct D;
    friend D;

    struct GuiData;

    GuiData & gui() noexcept;
    GuiData const & gui() const noexcept;

    using FontCharPtr = FontCharView const *;

    enum class Flags : uint8_t;
    REDEMPTION_DECLARE_ENUM_FLAGS_IN_CLASS(Flags)

    Flags m_flags {};

    MaxFileIntType m_max_file_list;

    GuiData * m_gui = nullptr;
    gdi::GraphicApi & m_gd;
    Callbacks m_callbacks;
    Font const & m_font;
    EventsGuard m_event_guard;
    Translator m_tr;
};

}
