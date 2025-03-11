/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "translation/trkeys.hpp"
#include "keyboard/keymap.hpp"
#include "core/RDP/orders/RDPOrdersPrimaryOpaqueRect.hpp"
#include "core/RDP/rdp_pointer.hpp"
#include "core/WinNT/chrono.hpp"
#include "core/error.hpp"
#include "core/font.hpp"
#include "mod/vnc/file_transfer/file_transfer_gui.hpp"
#include "mod/internal/widget/pagination.hpp"
#include "mod/internal/button_state.hpp"
#include "utils/sugar/bytes_copy.hpp"
#include "utils/sugar/bytes_equal.hpp"
#include "utils/allocate_sequence.hpp"
#include "utils/sugar/int_to_chars.hpp"
#include "utils/glyph_names.hpp"
#include "utils/tm_to_chars.hpp"
#include "utils/human_size.hpp"
#include "utils/strutils.hpp"
#include "utils/mathutils.hpp"
#include "utils/utf.hpp"
#include "utils/eta.hpp"
#include "gdi/graphic_api.hpp"
#include "gdi/draw_utils.hpp"
#include "gdi/text.hpp"

#include <new>
#include <bit>
#include <vector>
#include <charconv>


// TODO check draw only when is_open()
// TODO focus with tab, shift+tab
//
// TODO allocate_array + fcs_init => remove extra allocation

namespace
{
    constexpr Widget::Color rgb(uint32_t color) noexcept
    {
        return BGRasRGBColor(BGRColor(color));
    }

    struct Colors
    {
        using Color = Widget::Color;

        struct Text
        {
            Color fg;
            Color bg;
        };

        struct Frame
        {
            Color solid;
            Color shape;
        };

        struct Title
        {
            Color fg;
            Color bg;
            Color bottom_sep;

            operator Text () const noexcept
            {
                return {.fg = fg, .bg = bg};
            }
        };

        struct Button
        {
            Color fg;
            Color bg;
            Color border;
        };

        struct Button3
        {
            Button disabled;
            Button normal;
            Button focus;
        };

        struct IconButton
        {
            Color focus;
            Color normal;
            Color disabled;
            Color activated;
            Color focus_activated;
        };

        struct Item
        {
            Color fg_dir;
            Color fg_icon;
            Color fg;
            Color bg;

            Text to_text_colors() const noexcept
            {
                return {.fg = fg, .bg = bg};
            }
        };

        struct Panel
        {
            Title title;
            Color border;
            Color fg_total_items;
            Color fg_selected_item;
            Color fg_aborted;
            Color fg_error;
            Color fg_ok;
            Color fg;
            Color bg;

            operator Text () const noexcept
            {
                return {.fg = fg, .bg = bg};
            }
        };

        struct List
        {
            Text header;
            std::array<Item, 2> lines;
            Item focus;
            Item selected;
            Color column_sep;
        };

        struct CloseButton
        {
            Color fg;
            Color bg;
            Color active_bg;
        };

        struct Window
        {
            Color bg;
            Title title;
            CloseButton close;
        };

        Window window {
            .bg = rgb(0xF8FAFC),
            .title = {
                .fg = rgb(0),
                .bg = rgb(0xFFFFFF),
                .bottom_sep = rgb(0xF3F4F6),
            },
            .close = {
                .fg = rgb(0xFFFFFF),
                .bg = rgb(0xB72D30),
                .active_bg = rgb(0xB3210D),
            },
        };

        Button3 regular_button {
            .disabled {
                .fg = rgb(0xA5ACC5), // 0x7A8097
                .bg = rgb(0xF8FAFC),
                .border = rgb(0xEAECEE),
            },
            .normal {
                .fg = rgb(0xFFFFFF),
                .bg = rgb(0xF77120),
                .border = rgb(0xF77120),
            },
            .focus {
                .fg = rgb(0xFFFFFF),
                .bg = rgb(0xF77120),
                .border = rgb(0xCF5512),
            },
        };

        Button3 fade_button {
            .disabled {
                .fg = rgb(0xA5ACC5),
                .bg = rgb(0xFBFCFD),
                .border = rgb(0xEAECEE),
            },
            .normal {
                .fg = rgb(0),
                .bg = rgb(0xFBFCFD),
                .border = rgb(0xEAECEE),
            },
            .focus {
                .fg = rgb(0),
                .bg = rgb(0xFBFCFD),
                .border = rgb(0xF77120),
            },
        };

        IconButton icon_sort_button {
            .focus = rgb(0xFF9A4D),
            .normal = rgb(0),
            .disabled = rgb(0xB7B7B7),
            .activated = rgb(0xF77120),
            .focus_activated = rgb(0xCF5512),
        };

        Panel panel {
            .title = {
                .fg = rgb(0x7A8097),
                .bg = rgb(0xFFFFFF),
                .bottom_sep = rgb(0xF3F4F6),
            },
            .border = rgb(0xEAEDF2),
            .fg_total_items = rgb(0x7A8097),
            .fg_selected_item = rgb(0xF77120),
            .fg_aborted = rgb(0xCF5512),
            .fg_error = rgb(0xDA5540),
            .fg_ok = rgb(0x006600),
            .fg = rgb(0),
            .bg = rgb(0xFBFCFD),
        };

        List list {
            .header = {
                .fg = rgb(0x7A8097),
                .bg = rgb(0xFBFCFD),
            },
            .lines = {{
                {
                    .fg_dir = rgb(0xC0C0C0),
                    .fg_icon = rgb(0xC0C0C0),
                    .fg = rgb(0),
                    .bg = rgb(0xFBFCFD),
                },
                {
                    .fg_dir = rgb(0xC0C0C0),
                    .fg_icon = rgb(0xC0C0C0),
                    .fg = rgb(0),
                    .bg = rgb(0xFFFFFF),
                },
            }},
            .focus = {
                .fg_dir = rgb(0xC0C0C0),
                .fg_icon = rgb(0xC0C0C0),
                .fg = rgb(0),
                .bg = rgb(0xFFEBDE),
            },
            .selected = {
                .fg_dir = rgb(0xC0C0C0),
                .fg_icon = rgb(0xC0C0C0),
                .fg = rgb(0),
                .bg = rgb(0xFFEFE5),
            },
            .column_sep = rgb(0xEAECEE),
        };

        WidgetEdit::Colors edit {
            .fg = rgb(0),
            .bg = rgb(0xFFFFFF),
            .border = rgb(0xEAECEE),
            .focus_border = rgb(0xF77120),
            .cursor = rgb(0xF77120),
        };

        WidgetPagination::Colors pagination {
            .fg = rgb(0x7A8097),
            .bg = rgb(0xFBFCFD),
            .focus_fg = rgb(0xF77120),
            .edit = edit,
        };
    };

    using FileDateFormat = dateformats::YYYY_mm_dd_HH_MM_SS;

    dateformats::DateBuffer<FileDateFormat> make_human_date(WinNtUTime nt_time)
    {
        auto tp = std::chrono::clock_cast<std::chrono::system_clock>(to_win_nt_time(nt_time));
        auto seconds = std::chrono::duration_cast<std::chrono::seconds>(tp.time_since_epoch());
        time_t t = seconds.count();
        tm tm {};
        gmtime_r(&t, &tm);
        return {tm, {
            .date_sep = '/',
            .datetime_sep = ' ',
            .time_sep = ':',
        }};
    }

    int middle_pos(int length, int elem_length) noexcept
    {
        return (length - elem_length) / 2;
    }

    /// \return pad
    uint16_t update_width_and_compute_pad(
        OutParam<uint16_t> previous_width, uint16_t new_width) noexcept
    {
        uint16_t pad = (new_width < previous_width.out_value)
            ? previous_width.out_value - new_width
            : 0;
        previous_width.out_value = new_width;
        return pad;
    }
}

enum class VNC::FileTransferGui::Flags : uint8_t
{
    None        = 0,
    IsOpen      = 1 << 1,
    IsPressed   = 1 << 2,
    CbToVnc     = 1 << 3,
    VncToCb     = 1 << 4,
};

struct VNC::FileTransferGui::GuiData
{
    using Fcs = bounded_array_view<FontCharPtr, 0, static_cast<uint16_t>(~0u)>;
    using WritableFcs = writable_bounded_array_view<FontCharPtr, 0, Fcs::at_most>;

    static const std::size_t max_allocated_memory_before_free = 1024 * 1024;

    template<class T>
    static constexpr std::align_val_t align_val { alignof(T) };

    struct MemoryBlock
    {
        static const std::size_t block_size = 128 * 1024;

        MemoryBlock * next_block;

        template<class T>
        static void * aligned_alloc(std::size_t n)
        {
            return operator new (n, std::align_val_t{alignof(T)});
        }

        template<class T>
        static void aligned_dealloc(void * block) noexcept
        {
            return operator delete (block, std::align_val_t{alignof(T)});
        }
    };

    struct Storage
    {
        void * ptr;
        std::size_t free_space;

        MemoryBlock * first_block;
        MemoryBlock ** next_block_ptr;
        void * initial_ptr;
        std::size_t initial_free_space;

        std::size_t allocated;

        void release_blocks() noexcept;

        void allocate_memory(std::size_t data_len, std::size_t align_of_T);
    };

    template<typename T>
    writable_array_view<T> allocate_array(std::size_t n);

    void free_after(void const * ptr) noexcept;

    static GuiData * init_data(FileTransferGui & ft);

    struct FileDisplay
    {
        uint8_t const * file_name;
        FontCharPtr * fcs;
        uint16_t fcs_len;
        uint16_t file_name_len;
        uint16_t pixel_width;

        UVncFile::PathView file_name_av() const noexcept
        {
            return UVncFile::PathView::assumed(file_name, file_name_len);
        }
    };

    enum class FileDataType : uint8_t
    {
        /*
         * Drive types
         */
        LocalDisk,
        MediaDisk,
        NetworkDisk,
        CDRom,
        /*
         * Other types
         */
        Shortcut,
        LAST_SPECIAL_FILE = Shortcut,
        Directory,
        RegularFile,
        LAST_INDEX = RegularFile,
    };

    static bool is_file(FileDataType file_type) noexcept
    {
        return file_type == FileDataType::RegularFile;
    }

    // In an extreme case, maximum is a valid value, it can be ignored
    static constexpr MaxFileIntType INVALID_INDEX = ~MaxFileIntType{};

    struct FileData
    {
        uint64_t file_size;  // for sorting
        WinNtUTime last_access_time;  // for sorting
        uint8_t const * file_name;
        // Regular file and directory:
        //  text with filename + size + date
        //  filename = {fcs, fcs_name_len}
        //  size = {fcs + fcs_name_len, fcs_file_size_len}
        //  date = {fcs + fcs_name_len + fcs_file_size_len, fcs_date_len}
        // Drive
        //  size = based on file_type and m_fcs_offsets
        FontCharPtr const * fcs;
        uint16_t fcs_name_len;
        uint16_t file_name_len;
        uint8_t fcs_file_size_len;
        uint8_t file_size_offset_x;
        FileDataType file_type;
        bool checked;

        UVncFile::PathView file_name_av() const noexcept
        {
            return UVncFile::PathView::assumed(file_name, file_name_len);
        }

        bool is_file() const noexcept
        {
            return GuiData::is_file(file_type);
        }
    };

    struct TextId
    {
        enum E : uint8_t
        {
            // with capacity != len

            cb_list_finished,
            cb_list_counter,
            vnc_list_total_items,
            vnc_list_selected_items,
            transfer_file_error,
            LAST_DYNAMIC = transfer_file_error,

            // with capacity == len

            shortcut_desktop,
            shortcut_document,
            shortcut_network,

            button_root,
            button_parent,

            transfer_to_cb_in_progress,
            transfer_to_cb_completed,
            transfer_to_cb_aborted,
            transfer_to_cb_error,
            transfer_to_vnc_in_progress,
            transfer_to_vnc_completed,
            transfer_to_vnc_aborted,
            transfer_to_vnc_error,

            transfer_item_unit,
            transfer_byte_unit,

            window_title,

            cb_pan_name,
            cb_list_empty,
            cb_list_requested,
            cb_list_loading,
            cb_to_paste_loading,
            cb_to_paste_ready,
            vnc_pan_name,
            vnc_list_disabled,
            vnc_list_loading,
            vnc_list_empty,
            vnc_list_error,
            vnc_header_filename,
            vnc_header_size,
            vnc_header_modification_date,

            folder,

            unit_byte,
            unit_kibibyte,
            unit_mebibyte,
            unit_gibibyte,
            unit_tebibyte,
            unit_pebibyte,
            unit_exbibyte,

            stop_placeholder,
            copy_to_rdp,
            stop_to_rdp,
            copy_to_vnc,
            stop_to_vnc,

            START_DRIVE_TEXT,
            // same order that FileDataType
            vnc_drive_local = START_DRIVE_TEXT,
            vnc_drive_removable,
            vnc_drive_network,
            vnc_drive_cd_rom,

            COUNT
        };
    };

    struct Icons
    {
        struct Sorting
        {
            FontCharPtr ascending;
            FontCharPtr descending;
        };

        FontCharPtr title_icon;
        FontCharPtr close_x;
        FontCharPtr box_checked;
        FontCharPtr box_unchecked;
        Sorting sort_a_to_z;
        Sorting sort_1_to_9;
        Sorting sort_9_to_1;
        FontCharPtr file_icons[underlying_cast(GuiData::FileDataType::LAST_INDEX) + 1];

        uint16_t checkbox_w;
        uint16_t file_icon_w;

        struct FcsExtra : Fcs
        {
            int adjust_middle_height(GuiData const & gui) const noexcept
            {
                return (gui.line_h - front()->height) / 2 - front()->offsety;
            }

            int adjust_file_icon_x(GuiData const & gui) const noexcept
            {
                return (gui.icons.file_icon_w - front()->width) / 2 - front()->offsetx;
            }

            uint16_t boxed_height() const noexcept
            {
                return checked_int { front()->boxed_height() };
            }
        };

        FcsExtra fcs_checked(bool checked) const noexcept
        {
            return {{checked ? &box_checked : &box_unchecked, uint8_t{1}}};
        }

        FcsExtra fcs_file(FileDataType file_type) const noexcept
        {
            return {{&file_icons[underlying_cast(file_type)], uint8_t{1}}};
        }
    };

    enum class VncState : uint8_t
    {
        NoDirectory,
        Disabled,
        Loading,
        Empty,
        Error,
        Ready,
    };

    struct OwnedFile
    {
        uint16_t buffer_len;
        uint8_t buffer[VNC::UVncFile::max_path_length];

        OwnedFile() noexcept
            : buffer_len{}
        {}

        UVncFile::PathView name_av() const noexcept
        {
            return UVncFile::PathView::assumed(buffer, buffer_len);
        }

        bool is_full() const noexcept
        {
            return buffer_len == VNC::UVncFile::max_path_length;
        }

        bool empty() const noexcept
        {
            return !buffer_len;
        }

        void insert_win_sep() noexcept
        {
            if (!is_full())
            {
                buffer[buffer_len] = '\\';
                buffer_len++;
            }
        }
    };

    enum class CheckboxAction : uint8_t
    {
        ToUnchecked,
        ToChecked,
        Toggle,
    };

    enum class VimNumberState : uint8_t
    {
        Empty,
        Stacked,
        Begin,
        End,
    };

    enum class VimCheckboxAction : uint8_t
    {
        Unspecified,
        ToUnchecked,
        ToChecked,
        Toggle,
    };

    enum class VimSlidingDirection : uint8_t
    {
        None,
        ToUp,
        ToDown,
    };

    struct VimModeData
    {
        static const unsigned NAV_ALIGNMENT = 4; // fat copy with 4 alignment

        struct alignas(NAV_ALIGNMENT) Nav
        {
            VimNumberState number_state = VimNumberState::Empty;
            VimCheckboxAction checkbox_action = VimCheckboxAction::Unspecified;
            VimSlidingDirection sliding_direction = VimSlidingDirection::None;

            bool operator == (Nav const &) const noexcept = default;
        };

        static_assert(sizeof(Nav) == NAV_ALIGNMENT);

        Nav nav;
        MaxFileIntType marker_index = INVALID_INDEX;
        MaxFileIntType number = INVALID_INDEX;

        void reset_all() noexcept
        {
            reset_move();
            reset_marker();
        }

        void reset_move() noexcept
        {
            nav = Nav{};
        }

        void reset_marker() noexcept
        {
            marker_index = INVALID_INDEX;
        }

        bool is_empty() const noexcept
        {
            return nav == Nav{} && marker_index == INVALID_INDEX;
        }
    };


    struct VncData
    {
        struct SortedData
        {
            struct Array
            {
                bool initialized = false;
                MaxFileIntType capacity {};
                MaxFileIntType * indices {};
            };

            Array name {};
            Array size {};
            Array date {};
            Array reversed_name {};
            Array reversed_size {};
            Array reversed_date {};
            MaxFileIntType * indices {};
        };

        enum SortedField : uint8_t
        {
            SortByName,
            SortBySize,
            SortByDate,
            SortReverse = 0b100,
        };

        struct SelectedFileArray
        {
            MaxFileIntType size {};
            MaxFileIntType capacity {};
            UVncFile * files {};
        };

        struct VimKeys
        {
            VimNumberState num_state() const noexcept
            {
                return VimNumberState(m_states & number_mask);
            }

            VimCheckboxAction checkbox_action() const noexcept
            {
                return VimCheckboxAction((m_states & action_mask) >> action_shift);
            }

            VimSlidingDirection sliding_direction() const noexcept
            {
                return VimSlidingDirection((m_states & sliding_direction_mask)
                    >> sliding_direction_shift);
            }

            void set(VimNumberState st) noexcept
            {
                m_states = (m_states & ~number_mask)
                         | underlying_cast(st);
            }

            void set(VimCheckboxAction st) noexcept
            {
                m_states = ((m_states & ~action_mask)
                         | (underlying_cast(st) << action_shift)) & 0xff;
            }

            void set(VimSlidingDirection mode) noexcept
            {
                m_states = ((m_states & ~sliding_direction_mask)
                         | (underlying_cast(mode) << sliding_direction_shift)) & 0xff;
            }

            VimKeys reset() noexcept
            {
                auto old = *this;
                m_states = 0;
                return old;
            }

            bool is_empty() const noexcept
            {
                return !m_states;
            }

        private:
            static const uint8_t number_shift = 0;
            static const uint8_t number_mask = 0b111;

            static const uint8_t sliding_direction_shift = 3;
            static const uint8_t sliding_direction_mask = 0b11000;

            static const uint8_t action_shift = 5;
            static const uint8_t action_mask = 0b11100000;

            // VimCheckboxAction  vim_sliding_mode  VimNumberState
            // [3 bits]           [2 bits]                [3 bits]
            uint8_t m_states = 0;
        };

        VncState state {};
        bool all_file_checked = false;
        SortedField sorted_field_with_option = SortedField::SortByName;
        MaxFileIntType current_page {};
        MaxFileIntType selected_index {};
        MaxFileIntType selection_counter {};
        MaxFileIntType last_pressed_index = INVALID_INDEX;
        MaxFileIntType previous_selected_index = INVALID_INDEX;
        VimModeData vim_mode {};
        std::vector<FileData> files {};
        SortedData sorted {};
        SelectedFileArray selected_files {};
        MaxFileIntType directory_counter {};
        MaxFileIntType file_counter {};
        OwnedFile directory {};
        // TODO max capacity is 255 instead of 260 (max_path_length)
        WidgetEdit directory_edit;
        WidgetPagination pagination;

        SortedField sorted_field() const noexcept
        {
            return static_cast<SortedField>(sorted_field_with_option & ~VncData::SortReverse);
        }

        FileData & sorted_file(MaxFileIntType i) noexcept
        {
            return files[sorted.indices[i]];
        }

        array_view<MaxFileIntType> sorted_indices() const noexcept
        {
            return {sorted.indices, files.size()};
        }

        void reset()
        {
            all_file_checked = false;
            vim_mode.reset_all();
            files.clear();
            sorted.name.initialized = false;
            sorted.size.initialized = false;
            sorted.date.initialized = false;
            sorted.reversed_name.initialized = false;
            sorted.reversed_size.initialized = false;
            sorted.reversed_date.initialized = false;
            selected_files.size = 0;
            file_counter = 0;
            directory_counter = 0;
            current_page = 0;
            selected_index = 0;
            selection_counter = 0;
            reset_last_pressed_index();
            reset_previous_selected_index();
        }

        void free() noexcept
        {
            sorted = SortedData{};
            selected_files = SelectedFileArray{};
        }

        MaxFileIntType reset_last_pressed_index() noexcept
        {
            return std::exchange(last_pressed_index, INVALID_INDEX);
        }

        MaxFileIntType reset_previous_selected_index() noexcept
        {
            return std::exchange(previous_selected_index, INVALID_INDEX);
        }

        // special files have not date column
        bool has_date_border() const noexcept
        {
            return underlying_cast(files.front().file_type)
                 > underlying_cast(FileDataType::LAST_SPECIAL_FILE);
        }
    };

    enum class CbState : uint8_t
    {
        None,
        Empty,
        Requested,
        Loading,
        AddItem,
        Ready,
        PopulatedByServerLoading,
        PopulatedByServerReady,
    };

    struct CbData
    {
        MonotonicTimePoint last_time_of_update_nb_file {};
        uint32_t total_file {};
        uint32_t nb_file {};
        CbState display_state {};
        // used in open() and set when not is_open()
        CbState next_display_state {};
    };

    enum class MidState : bool
    {
        Disabled,
        Enabled,
    };

    struct MidData
    {
        bool active_cb_to_vnc;
        bool active_vnc_to_cb;
    };

    enum class ElementId : uint8_t
    {
        None,
        VncRootButton,
        VncParentButton,
        VncEditField,
        VncListAllCheckbox,
        VncIconSortFilename,
        VncIconSortSize,
        VncIconSortDate,
        VncList,
        VncNavigation,
        ToRdpButton,
        ToVncButton,
        StopTransferButton,
        // unfocusable, but pressable
        GuiClose,
    };

    static bool is_vnc_list(ElementId elem) noexcept
    {
        return elem <= ElementId::ToRdpButton && ElementId::VncListAllCheckbox <= elem;
    }

    static bool is_vnc_sort_header(ElementId elem) noexcept
    {
        return elem <= ElementId::VncIconSortDate && ElementId::VncIconSortFilename <= elem;
    }

    struct ElementList
    {
        void add(ElementId elem) noexcept
        {
            m_mask |= _to_flag(elem);
        }

        void remove(ElementId elem) noexcept
        {
            m_mask &= ~_to_flag(elem);
        }

        bool has(ElementId elem) const noexcept
        {
            return m_mask & _to_flag(elem);
        }

        void toggle(ElementId elem) noexcept
        {
            m_mask ^= _to_flag(elem);
        }

        uint32_t raw_mask() const noexcept
        {
            return m_mask;
        }

    private:
        using MaskType = uint16_t;

        static MaskType _to_flag(ElementId elem) noexcept
        {
            return checked_int{ 1u << underlying_cast(elem) };
        }

        MaskType m_mask = _to_flag(ElementId::None);
    };

    // TODO adapt max width clipboard with max width text
    /*
              (rdp pan)         (mid pan)                    (vnc pan)
    .------------------------------------------------------------------------------------.
    |                                     TITLE                                      | X |
    |--------------------------+----------+----------------------------------------------|
    |         Clipboard        |          |                  VNC Server                  |
    |--------------------------|----------|----------------------------------------------|
    |                          |          |   /  ../   dir path                          |
    |                          |          |                                              |
    |         Loading          |          |      Filename  Size      Modification date   |
    |         n / NNN          | Copy >>  | ☒ 🖹 name      3.00 KiB  YYYY/MM/DD HH:MM:SS |
    |   (and other messages)   |  << Copy | ☒ 🗀 name                                    |
    |                          |          |                                              |
    |                          |<< Stop >>|                                              |
    |   Transfer in progress   |          |                                              |
    |      file path error     |          |                                              |
    |                          |          |                                              |
    |            14 %          |          |                                              |
    |         3 / 10 items     |          |                                              |
    |      2834 / 19374 bytes  |          |                                              |
    |                          |          |                                              |
    |                          |          |          <<   <   n / NNN   >   >>           |
    `------------------------------------------------------------------------------------`
    */
    struct Layout
    {
        uint16_t width;
        uint16_t height;

        uint16_t title_h;
        int16_t cb_name_x;
        int16_t vnc_name_x;

        struct CloseBtn
        {
            Rect rect;

            uint16_t x_pad;
            uint16_t y_pad;
        };

        CloseBtn close_btn;

        struct CbRdp
        {
            Rect inner_rect;

            int16_t text1_y;
            int16_t text2_y;

            int16_t progress_msg_y;
            int16_t progress_path_y;
            int16_t progress_percent_y;
            int16_t progress_eta_y;

            int16_t progress_transferred_items_y;
            int16_t progress_transferred_bytes_y;

            int16_t progress_right_text_limit;
        };

        CbRdp cb_rdp;

        struct Mid
        {
            Rect rect;

            int16_t button_x;
            uint16_t button_inner_w;

            int16_t copy_to_vnc_y;
            uint16_t copy_to_vnc_left_pad;

            int16_t copy_to_rdp_y;
            uint16_t copy_to_rdp_left_pad;

            int16_t stop_y;
            uint16_t stop_to_vnc_left_pad;

            uint16_t stop_to_rdp_left_pad;

            uint16_t stop_placeholder_left_pad;
        };

        Mid mid;

        struct Vnc
        {
            Rect inner_rect;

            int16_t top_bar_y;
            int16_t root_x;
            uint16_t root_text_w;
            int16_t parent_x;
            uint16_t parent_text_w;

            int16_t header_y;
            int16_t header_text_y;
            uint16_t header_h;
            int16_t header_filename_x;
            uint16_t header_filename_w;
            int16_t header_size_x;
            int16_t header_date_x;

            int16_t list_y;
            uint16_t list_minus_nav_h;
            int16_t list_checkbox_x;
            int16_t list_file_icon_x;

            // column separator
            int16_t list_vline_icon_x;
            int16_t list_vline_size_x;
            int16_t list_vline_date_x;

            uint16_t list_item_h;

            int16_t list_total_y;
            int16_t list_total_x;
            int16_t list_total_selected_x;
            int16_t list_total_selected_margin_left;

            uint16_t item_by_page_with_nav;

            int16_t loading_x;
            int16_t loading_y;

            int16_t last_y_drawing;
        };

        Vnc vnc;
    };

    struct Progress
    {
        bool show_progress;
        bool enable_eta;
        Direction direction;
        TextId::E msg;
        Widget::Color msg_color {};

        uint16_t previous_width_msg;

        uint16_t transferred_items_previous_left_width;
        uint16_t transferred_items_previous_right_width;

        uint16_t transferred_bytes_previous_left_width;
        uint16_t transferred_bytes_previous_right_width;

        uint16_t error_previous_width;
        uint16_t error_unit_previous_width;

        uint16_t path_error_len;
        uint16_t path_error_width;
        uint16_t path_error_previous_width;

        uint8_t percent_progression;
        uint16_t eta_progression_previous_width;
        uint16_t percent_progression_previous_width;

        uint32_t nb_copied;
        uint32_t old_nb_copied;
        uint32_t total_items;
        uint64_t transferred_bytes;
        uint64_t old_transferred_bytes;
        uint64_t total_bytes;
        MonotonicTimePoint last_time_of_add_progression;
        Eta::Duration eta_duration;
        Eta progression_eta;
    };

    enum class PointerShape : uint8_t
    {
        Unspecified,
        Normal,
        Edit,
        Pointer,
    };

    bool is_open() const noexcept
    {
        return flags_any(flags, Flags::IsOpen);
    }

    Storage m_storage;

    Flags flags {};

    PointerShape current_mouse_pointer = PointerShape::Unspecified;
    uint8_t mouse_pointer_set_mask = 0;

    ElementId pressed_item {};
    ElementId focus_item = ElementId::VncList;
    ElementList disabled_elements {};

    Layout layout {};

    uint16_t mid_button_text_max_w {};
    uint16_t max_date_w {};
    uint16_t max_file_size_w {};
    uint16_t nav_total_page_prefix_w {};
    uint16_t max_digit_w {};
    uint16_t max_unit_w {};
    uint16_t space_w {};
    uint16_t line_h {};

    uint16_t max_unit_nb_fc {};

    // right padding for KiB, MiB, etc
    uint8_t unit_right_pads[7] {};
    // right padding for folder text
    uint8_t folder_right_pad {};

    // pixel width text
    uint16_t text_widths[TextId::COUNT] {};
    // offset start from m_fcs.
    // m_text_offset[n-1] - m_text_offset[n] = capacity
    // first offset is the offset of second string.
    // last offset is the total capacity.
    uint16_t fcs_offsets[TextId::COUNT] {};
    // len for text with dynamic size when capacity != len
    uint8_t fcs_lengths[TextId::LAST_DYNAMIC + 1] {};

    FontCharPtr * fcs {};

    MonotonicTimePoint last_click_time {};

    Translator tr;
    gdi::GraphicApi & gd;

    Progress progress {};

    Font font;
    Icons icons {};
    VncData vnc;
    CbData cb {};
    MidData mid {};
};

struct VNC::FileTransferGui::FileData : VNC::FileTransferGui::GuiData::FileData
{};

VNC::FileTransferGui::GuiData & VNC::FileTransferGui::gui() noexcept
{
    assert(m_gui);
    return *m_gui;
}

VNC::FileTransferGui::GuiData const & VNC::FileTransferGui::gui() const noexcept
{
    assert(m_gui);
    return *m_gui;
}


struct VNC::FileTransferGui::D : GuiData // inherit for import internal type
{
    static constexpr MonotonicTimePoint::duration delay_before_update
        = std::chrono::seconds{1};

    static constexpr MonotonicTimePoint::duration delay_before_db_click
        = std::chrono::milliseconds{600};


    enum class ForceUpdate : bool
    {
        No,
        Yes,
    };

    static ForceUpdate update_delay(FileTransferGui & self, InOutParam<MonotonicTimePoint> tp)
    {
        auto current_time = self.m_event_guard.get_monotonic_time();
        if (current_time - tp.inout_value > D::delay_before_update)
        {
            tp.inout_value = current_time;
            return ForceUpdate::Yes;
        }
        return ForceUpdate::No;
    }

    struct FcsAndWidth
    {
        Fcs fcs;
        uint16_t text_width;
    };

    struct WritableFcsAndWidth
    {
        WritableFcs fcs;
        uint16_t text_width;

        operator FcsAndWidth () const noexcept
        {
            return {fcs, text_width};
        }
    };

    static WritableFcsAndWidth fcs_init(FontCharPtr * fcs, Font const& font, bytes_view str) noexcept
    {
        auto * out = fcs;

        int text_width = 0;

        utf8_for_each(
            str,
            [&](uint32_t uc) {
                auto const * fc = &font.item(uc).view;
                text_width += fc->boxed_width();
                *out++ = fc;
            },
            [](utf8_char_invalid) {},
            [](utf8_char_truncated) {}
        );

        return {
            .fcs = WritableFcs::assumed(fcs, out),
            .text_width = checked_int(text_width + !str.empty()),
        };
    }

    static WritableFcsAndWidth fcs_init(FontCharPtr * fcs, Font const& font, UVncFile::PathView path) noexcept
    {
        auto str = path.native();

        auto * out = fcs;

        int text_width = 0;

        for (auto c : str)
        {
            auto uc = cp1252_to_utf32(c);
            auto const * fc = &font.item(uc).view;
            text_width += fc->boxed_width();
            *out++ = fc;
        }

        return {
            .fcs = WritableFcs::assumed(fcs, out),
            .text_width = checked_int(text_width + !str.empty()),
        };
    }

    static WritableFcsAndWidth fcs_init_printable_ascii(
        FontCharPtr * fcs, Font const& font, bytes_view str
    ) noexcept
    {
        auto* out = fcs;

        int text_width = 0;

        for (uint8_t uc : str)
        {
            REDEMPTION_ASSUME(uc >= 32 && uc < 127);
            auto const * fc = &font.item(uc).view;
            text_width += fc->boxed_width();
            *out++ = fc;
        }

        return {
            .fcs = WritableFcs::assumed(fcs, out),
            .text_width = checked_int{text_width + !str.empty()},
        };
    }

    enum class UncheckedAscii : bool { No, Yes };
    static constexpr UncheckedAscii unchecked_ascii = UncheckedAscii::Yes;

    static WritableFcsAndWidth init_dynamic_fcs_and_lengths(
        GuiData & gui, TextId::E id, bytes_view str,
        UncheckedAscii is_unchecked_ascii = UncheckedAscii::No
    ) noexcept
    {
        assert(id <= TextId::LAST_DYNAMIC);
        auto * fcs = fcs_data(gui, id);
        auto msg = (is_unchecked_ascii == unchecked_ascii)
            ? fcs_init_printable_ascii(fcs, gui.font, str)
            : fcs_init(fcs, gui.font, str.first(mmin(fcs_capacity(gui, id), str.size())));
        gui.fcs_lengths[id] = checked_int{msg.fcs.size()};
        gui.text_widths[id] = msg.text_width;
        return msg;
    }

    static bool has_nav(VNC::FileTransferGui::GuiData const & gui) noexcept
    {
        return gui.layout.vnc.item_by_page_with_nav < gui.vnc.files.size();
    }

    // msg="{nb}/{total}"
    static const uint16_t NB_MSG_CAPACITY = buffer_size_of_uint64_to_chars * 2 + 1;

    static const uint16_t LINE_Y_PADDING = 2;
    static const uint16_t LINE_X_PADDING = 2;

    static const uint16_t COLUMN_X_PADDING = 4;
    static const uint16_t COLUMN_BORDER_LEN = 1;

    static const uint16_t BUTTON_Y_PADDING = 2;
    static const uint16_t BUTTON_X_PADDING = 5;
    static const uint16_t BUTTON_BORDER_LEN = 2;
    static const uint16_t BUTTON_HEIGHT_DECORATION = (BUTTON_Y_PADDING + BUTTON_BORDER_LEN) * 2;
    static const uint16_t BUTTON_WIDTH_DECORATION = (BUTTON_X_PADDING + BUTTON_BORDER_LEN) * 2;

    static const uint16_t ELEMENT_Y_SEPARATOR = 5;
    static const uint16_t ELEMENT_X_SEPARATOR = 5;

    static const uint16_t MID_PAN_X_PADDING = 5;

    static const uint16_t PAN_BORDER_LEN = 1;
    static const uint16_t PAN_Y_MARGIN = 5;
    static const uint16_t PAN_X_MARGIN = 5;

    static const uint16_t TITLE_BOTTOM_SEPARATOR = 1;
    static const uint16_t TITLE_Y_PADDING = 1;
    static const uint16_t TITLE_X_PADDING = 5;

    static constexpr gdi::DrawTextPadding::Padding2 LINE_PADDING {
        .top_bottom = D::LINE_Y_PADDING,
        .left_right = D::LINE_X_PADDING,
    };

    // TODO param / theme ?
    static constexpr Colors colors {};

    static FontCharPtr * fcs_data(GuiData const & gui, TextId::E id) noexcept
    {
        return gui.fcs + (id ? gui.fcs_offsets[id-1] : 0);
    }

    static uint16_t fcs_capacity(GuiData const & gui, TextId::E id) noexcept
    {
        return (id ? gui.fcs_offsets[id-1] : 0) - gui.fcs_offsets[id];
    }

    static Fcs fcs(GuiData const & gui, TextId::E id) noexcept
    {
        auto * fcs = fcs_data(gui, id);
        auto * end_fcs = (id <= TextId::LAST_DYNAMIC)
            ? fcs + gui.fcs_lengths[id]
            : gui.fcs + gui.fcs_offsets[id];
        return Fcs::assumed(fcs, end_fcs);
    }

    static Fcs fcs_for_static_str(GuiData const & gui, TextId::E id) noexcept
    {
        assert(id > TextId::LAST_DYNAMIC);
        return Fcs::assumed(
            gui.fcs + gui.fcs_offsets[id - 1],
            gui.fcs + gui.fcs_offsets[id]
        );
    }

    static Fcs file_unit_to_fcs(GuiData const & gui, HumanSizePowerOf2::Unit unit) noexcept
    {
        auto id = static_cast<TextId::E>(underlying_cast(TextId::unit_byte) + underlying_cast(unit));
        return fcs_for_static_str(gui, id);
    }

    static FcsAndWidth fcs_and_width(GuiData const & gui, TextId::E id) noexcept
    {
        return {fcs(gui, id), gui.text_widths[id]};
    }


    using Text = Colors::Text;

    struct BoxedElement
    {
        ElementId elem;
        Rect box;
    };

    struct Btn
    {
        ElementId elem;
        bool fade;
        int x;
        int y;
        int inner_w;
        int left_pad = 0;
        Fcs fcs;
    };

    struct CheckboxElem
    {
        ElementId elem;
        int x;
        int y;
        uint16_t h;
        Fcs fcs;

        Rect rect() const noexcept
        {
            return {
                checked_int{x + fcs.front()->offsetx + 1},
                checked_int{y + fcs.front()->offsety},
                checked_int{fcs.front()->width},
                checked_int{fcs.front()->height},
            };
        }
    };

    struct Checkbox
    {
        int x;
        int dy;
        uint16_t h;
        Fcs fcs;

        Checkbox(GuiData const & gui, bool checked)
        {
            auto checked_icon = gui.icons.fcs_checked(checked);
            x = gui.layout.vnc.list_checkbox_x;
            dy = checked_icon.adjust_middle_height(gui);
            h = checked_icon.boxed_height();
            fcs = checked_icon;
        }

        Rect boxed_click(GuiData const & gui, int y) const noexcept
        {
            auto & ch = *fcs.front();

            int bx1 = ch.offsetx;
            int bx2 = ch.incby - ch.width;
            int bx = mmin(bx1, bx2);

            int by1 = ch.offsety + dy;
            int by2 = gui.line_h - ch.height - by1;
            int by = mmin(by1, by2) / 2;

            return {
                checked_int{x + ch.offsetx + 1 - bx},
                checked_int{y + dy + ch.offsety - by},
                checked_int{ch.width + bx * 2},
                checked_int{ch.height + by * 2},
            };
        }

        Rect boxed_click_from_y_item(GuiData const & gui, int y_item) const noexcept
        {
            return boxed_click(gui, y_item + D::LINE_Y_PADDING);
        }

        void draw(
            gdi::GraphicApi & gd, int y, Colors::Text colors, Rect clip,
            gdi::DrawTextPadding pad = gdi::DrawTextPadding{})
        {
            pad.left += 1; // TODO fix draw_text() bug
            gdi::draw_text(
                gd,
                x,
                y + dy,
                h,
                pad,
                fcs,
                colors.fg,
                colors.bg,
                clip
            );
        }
    };


    struct DrawCtx
    {
        GuiData & gui;
        gdi::GraphicApi & gd;
        Rect clip;
        uint16_t line_h = gui.line_h;

        DrawCtx(GuiData & gui, Rect clip) noexcept
            : gui(gui)
            , gd(gui.gd)
            , clip(clip)
        {}

        Rect clip_or_intersection(Rect rect) const noexcept
        {
            return clip.contains(rect) ? clip : clip.intersect(rect);
        }

        void draw_rect(Rect rect, RDPColor color)
        {
            if (clip.has_intersection(rect))
            {
                gui.gd.draw(
                    RDPOpaqueRect(clip.intersect(rect), color),
                    clip,
                    gdi::ColorCtx::depth24()
                );
            }
        }

        void draw_text(
            int x, int y, TextId::E item, Text colors,
            gdi::DrawTextPadding padding = gdi::DrawTextPadding{})
        {
            draw_text(x, y, fcs_and_width(gui, item), colors, padding);
        }

        void draw_text(
            int x, int y, FcsAndWidth text, Text colors,
            gdi::DrawTextPadding padding = gdi::DrawTextPadding{})
        {
            Rect box{
                checked_int{x},
                checked_int{y},
                checked_int{text.text_width + padding.left + padding.right},
                checked_int{line_h + padding.top + padding.bottom},
            };

            if (clip.has_intersection(box))
            {
                gdi::draw_text(
                    gd,
                    x,
                    y,
                    line_h,
                    padding,
                    text.fcs,
                    colors.fg,
                    colors.bg,
                    clip_or_intersection(box)
                );
            }
        }

        void draw_border(Rect box, uint16_t border_width, Widget::Color color)
        {
            int bw = border_width;
            auto [x, y, cx, cy] = box;

            auto draw = [&](int x, int y, int cx, int cy) {
                draw_rect(
                    Rect(checked_int{x}, checked_int{y}, checked_int{cx}, checked_int{cy}),
                    color
                );
            };

            // top
            draw(x, y, cx, bw);
            // left
            draw(x, y + bw, bw, cy - bw * 2);
            // right
            draw(x + cx - bw, y + bw, bw, cy - bw * 2);
            // bottom
            draw(x, y + cy - bw, cx, bw);
        }

        void draw_button(Btn btn)
        {
            auto pressed_pad = (gui.pressed_item == btn.elem);
            auto colors = gui.disabled_elements.has(btn.elem)
                ? (btn.fade ? D::colors.fade_button.disabled : D::colors.regular_button.disabled)
                : gui.focus_item == btn.elem
                ? (btn.fade ? D::colors.fade_button.focus : D::colors.regular_button.focus)
                : (btn.fade ? D::colors.fade_button.normal : D::colors.regular_button.normal);

            Rect box = {
                checked_int{btn.x},
                checked_int{btn.y},
                checked_int{btn.inner_w + BUTTON_WIDTH_DECORATION},
                checked_int{line_h + BUTTON_HEIGHT_DECORATION},
            };

            if (clip.has_intersection(box))
            {
                Rect rect_text = box;
                rect_text.x += BUTTON_BORDER_LEN;
                rect_text.y += BUTTON_BORDER_LEN;
                rect_text.cx -= BUTTON_BORDER_LEN * 2;
                rect_text.cy -= BUTTON_BORDER_LEN * 2;

                gdi::draw_text(
                    gd,
                    btn.x + BUTTON_BORDER_LEN,
                    btn.y + BUTTON_BORDER_LEN,
                    line_h,
                    gdi::DrawTextPadding {
                        .top = BUTTON_Y_PADDING,
                        .right = checked_int{ btn.inner_w - btn.left_pad },
                        .bottom = BUTTON_Y_PADDING,
                        .left = checked_int{ BUTTON_X_PADDING + btn.left_pad },
                    }.gap_xy(pressed_pad),
                    btn.fcs,
                    colors.fg,
                    colors.bg,
                    rect_text.intersect(clip)
                );

                draw_border(box, BUTTON_BORDER_LEN, colors.border);
            }
        }

        void draw_vnc_root_button()
        {
            draw_button({
                .elem = ElementId::VncRootButton,
                .fade = true,
                .x = gui.layout.vnc.root_x,
                .y = gui.layout.vnc.top_bar_y,
                .inner_w = gui.layout.vnc.root_text_w,
                .fcs = fcs(gui, D::TextId::button_root),
            });
        }

        void draw_vnc_parent_button()
        {
            draw_button({
                .elem = ElementId::VncParentButton,
                .fade = true,
                .x = gui.layout.vnc.parent_x,
                .y = gui.layout.vnc.top_bar_y,
                .inner_w = gui.layout.vnc.parent_text_w,
                .fcs = fcs(gui, D::TextId::button_parent),
            });
        }

        void draw_vnc_all_file_checkbox()
        {
            Checkbox checkbox{ gui, gui.vnc.all_file_checked };
            auto elem = ElementId::VncListAllCheckbox;
            auto fg_icon = gui.disabled_elements.has(elem)
                ? colors.icon_sort_button.disabled
                : (gui.focus_item == elem)
                ? colors.icon_sort_button.focus
                : colors.icon_sort_button.normal
                ;
            auto text_colors = Colors::Text{.fg = fg_icon, .bg = colors.panel.bg };
            checkbox.draw(gd, gui.layout.vnc.header_text_y, text_colors, clip);
        }

        void draw_to_vnc_button()
        {
            if (!flags_any(gui.flags, Flags::CbToVnc))
            {
                return ;
            }

            draw_button({
                .elem = ElementId::ToVncButton,
                .fade = false,
                .x = gui.layout.mid.button_x,
                .y = gui.layout.mid.copy_to_vnc_y,
                .inner_w = gui.layout.mid.button_inner_w,
                .left_pad = gui.layout.mid.copy_to_vnc_left_pad,
                .fcs = fcs(gui, TextId::copy_to_vnc),
            });
        }

        void draw_to_rdp_button()
        {
            if (!flags_any(gui.flags, Flags::VncToCb))
            {
                return ;
            }

            draw_button({
                .elem = ElementId::ToRdpButton,
                .fade = false,
                .x = gui.layout.mid.button_x,
                .y = gui.layout.mid.copy_to_rdp_y,
                .inner_w = gui.layout.mid.button_inner_w,
                .left_pad = gui.layout.mid.copy_to_rdp_left_pad,
                .fcs = fcs(gui, TextId::copy_to_rdp),
            });
        }

        void draw_stop_transfer_button()
        {
            struct Data {
                TextId::E text_id;
                uint16_t left_pad;
            };
            auto elem_id = ElementId::StopTransferButton;
            auto d
                = gui.disabled_elements.has(elem_id)
                ? Data{TextId::stop_placeholder, gui.layout.mid.stop_placeholder_left_pad}
                : gui.progress.direction == Direction::CbToVnc
                ? Data{TextId::stop_to_vnc, gui.layout.mid.stop_to_vnc_left_pad}
                : Data{TextId::stop_to_rdp, gui.layout.mid.stop_to_rdp_left_pad};
            draw_button({
                .elem = elem_id,
                .fade = false,
                .x = gui.layout.mid.button_x,
                .y = gui.layout.mid.stop_y,
                .inner_w = gui.layout.mid.button_inner_w,
                .left_pad = d.left_pad,
                .fcs = fcs(gui, d.text_id),
            });
        }
    };


    enum class ComputeMode : uint8_t
    {
        // for incremental display update
        Update,
        // for rdp_input_invalidate with unmodified state
        Refresh,
        // for rdp_input_invalidate with modified state
        UpdateRefresh,
    };

    static void draw_cb_part(
        GuiData & gui,
        Rect clip,
        CbState new_state,
        ComputeMode mode,
        ForceUpdate force_update)
    {
        if (!gui.is_open())
        {
            gui.cb.next_display_state = new_state;
            return ;
        }

        if (force_update == ForceUpdate::No && gui.cb.display_state == new_state)
        {
            return ;
        }

        gui.cb.next_display_state = new_state;

        /*
         * Compute old widths
         */

        uint16_t old_widths[2] {};

        switch (gui.cb.display_state)
        {
            case CbState::None:
                break;

            case CbState::Empty:
                old_widths[0] = gui.text_widths[TextId::cb_list_empty];
                break;

            case CbState::Requested:
                old_widths[0] = gui.text_widths[TextId::cb_list_requested];
                break;

            case CbState::Loading:
                old_widths[0] = gui.text_widths[TextId::cb_list_loading];
                break;

            case CbState::AddItem:
                old_widths[0] = gui.text_widths[TextId::cb_list_loading];
                old_widths[1] = gui.text_widths[TextId::cb_list_counter];
                break;

            case CbState::Ready:
                old_widths[0] = gui.text_widths[TextId::cb_list_finished];
                break;

            case CbState::PopulatedByServerLoading:
                old_widths[0] = gui.text_widths[TextId::cb_to_paste_loading];
                break;

            case CbState::PopulatedByServerReady:
                old_widths[0] = gui.text_widths[TextId::cb_to_paste_ready];
                break;
        }

        /*
         * Compute new widths and messages
         */

        Fcs fcs_msgs[2] {};
        uint16_t new_widths[2] {};

        auto set_static_msg = [&](unsigned i, TextId::E id){
            fcs_msgs[i] = fcs(gui, id);
            new_widths[i] = gui.text_widths[id];
        };

        auto set_dynamic_msg = [&](
            unsigned i, TextId::E id, bytes_view str,
            UncheckedAscii is_unchecked_ascii = UncheckedAscii::No
        ){
            auto msg = init_dynamic_fcs_and_lengths(gui, id, str, is_unchecked_ascii);
            fcs_msgs[i] = msg.fcs;
            new_widths[i] = gui.text_widths[id];
        };

        auto set_msg_loading = [&]{
            if (mode != ComputeMode::Update
             || (gui.cb.display_state != CbState::AddItem
              && gui.cb.display_state != CbState::Loading))
            {
                fcs_msgs[0] = fcs(gui, TextId::cb_list_loading);
                new_widths[0] = gui.text_widths[TextId::cb_list_loading];
            }
        };

        switch (new_state)
        {
            case CbState::None:
                break;

            case CbState::Empty:
                set_static_msg(0, TextId::cb_list_empty);
                break;

            case CbState::Requested:
                set_static_msg(0, TextId::cb_list_requested);
                break;

            case CbState::Loading:
                set_msg_loading();
                break;

            case CbState::AddItem: {
                set_msg_loading();

                if (mode == ComputeMode::Refresh)
                {
                    set_static_msg(1, TextId::cb_list_counter);
                }
                else
                {
                    /*
                     * build string: "{n}/{total}"
                     */

                    char buffer[NB_MSG_CAPACITY];
                    auto * p = buffer;
                    auto * pend = buffer + NB_MSG_CAPACITY;
                    p = std::to_chars(p, pend, gui.cb.nb_file).ptr;
                    *p++ = '/';
                    p = std::to_chars(p, pend, gui.cb.total_file).ptr;
                    assert(p - buffer < NB_MSG_CAPACITY);

                    // init fcs, len and width
                    auto str = bytes_view{buffer, p};
                    set_dynamic_msg(1, TextId::cb_list_counter, str, unchecked_ascii);
                }

                break;
            }

            case CbState::Ready: {
                if (mode == ComputeMode::Refresh)
                {
                    set_static_msg(0, TextId::cb_list_finished);
                }
                else
                {
                    char buffer[255];
                    auto str = gui.tr.nfmt(
                        make_writable_array_view(buffer),
                        trkeys::vnc_ft_cb_list_total,
                        gui.cb.nb_file
                    );
                    set_dynamic_msg(0, TextId::cb_list_finished, str);
                }
                break;
            }

            case CbState::PopulatedByServerLoading:
                set_static_msg(0, TextId::cb_to_paste_loading);
                break;

            case CbState::PopulatedByServerReady:
                set_static_msg(0, TextId::cb_to_paste_ready);
                break;
        }

        /*
         * Draw messages
         */

        Rect const & box = gui.layout.cb_rdp.inner_rect;

        auto draw_text = [&](unsigned i, int y){
            int16_t old_offset_x = (box.cx - old_widths[i]) / 2;
            int16_t new_offset_x = (box.cx - new_widths[i]) / 2;
            uint16_t pad_x = checked_int{
                old_offset_x < new_offset_x
                    ? new_offset_x - old_offset_x + 1
                    : 0
            };

            gdi::draw_text(
                gui.gd,
                box.x + new_offset_x - pad_x,
                y,
                gui.line_h,
                gdi::DrawTextPadding::Horizontal{pad_x},
                fcs_msgs[i],
                D::colors.panel.fg,
                D::colors.panel.bg,
                clip
            );
        };

        // when first line changes
        if (!fcs_msgs[0].empty())
        {
            draw_text(0, gui.layout.cb_rdp.text1_y);
        }

        // show second line
        if (!fcs_msgs[1].empty())
        {
            draw_text(1, gui.layout.cb_rdp.text2_y);
        }
        // clear second line
        else if (old_widths[1] && mode == ComputeMode::Update)
        {
            Rect rect {
                checked_int{clip.x + (clip.cx - old_widths[1]) / 2},
                gui.layout.cb_rdp.text2_y,
                old_widths[1],
                gui.line_h
            };
            gui.gd.draw(
                RDPOpaqueRect(rect, D::colors.panel.bg),
                clip,
                gdi::ColorCtx::depth24()
            );
        }

        gui.cb.display_state = new_state;
    }

    static void draw_centered_text(
        DrawCtx & ctx, FcsAndWidth msg, Widget::Color fg_color,
        uint16_t cx, int y, InOutParam<uint16_t> previous_w)
    {
        uint16_t pad = (msg.text_width < previous_w.inout_value)
            ? (previous_w.inout_value - msg.text_width + 1) / 2
            : 0;
        int x = (cx - msg.text_width) / 2 - pad;
        Text text_colors { .fg = fg_color, .bg = colors.panel.bg };
        ctx.draw_text(x, y, msg, text_colors, gdi::DrawTextPadding::X{pad});
        previous_w.inout_value = msg.text_width;
    }

    static void draw_progress_part(GuiData & gui, Rect clip)
    {
        auto & progress = gui.progress;
        auto & cb_rdp = gui.layout.cb_rdp;
        auto & rdp_rect = cb_rdp.inner_rect;

        if (!progress.show_progress || !rdp_rect.has_intersection(clip))
        {
            return ;
        }

        DrawCtx ctx{gui, rdp_rect.intersect(clip)};

        draw_progress_status(ctx);
        draw_progress_percent(ctx);
        draw_progress_eta(ctx);
        draw_progress_items(ctx, RedrawTotal::Yes);
        draw_progress_bytes(ctx, RedrawTotal::Yes);
    }

    static void draw_progress_status(DrawCtx & ctx)
    {
        auto & gui = ctx.gui;
        auto & progress = gui.progress;
        auto & cb_rdp = gui.layout.cb_rdp;
        auto & rdp_rect = cb_rdp.inner_rect;

        if (progress.path_error_width)
        {
            FcsAndWidth msg {
                .fcs = {fcs_data(gui, TextId::transfer_file_error), progress.path_error_len},
                .text_width = progress.path_error_width,
            };
            draw_centered_text(
                ctx, msg, progress.msg_color,
                rdp_rect.cx, cb_rdp.progress_path_y,
                InOutParam{progress.path_error_previous_width}
            );
        }
        // clear msg
        else if (progress.path_error_previous_width)
        {
            ctx.draw_rect(
                {
                    checked_int{ (rdp_rect.cx - progress.path_error_previous_width) / 2 },
                    cb_rdp.progress_path_y,
                    progress.path_error_previous_width,
                    gui.line_h
                },
                colors.panel.bg
            );
            progress.path_error_previous_width = 0;
        }

        draw_centered_text(
            ctx, fcs_and_width(gui, progress.msg), progress.msg_color,
            rdp_rect.cx, cb_rdp.progress_msg_y, InOutParam{progress.previous_width_msg}
        );
    }

    static void draw_progress_percent(DrawCtx & ctx)
    {
        auto & gui = ctx.gui;
        auto & cb_rdp = gui.layout.cb_rdp;
        auto & rdp_rect = cb_rdp.inner_rect;

        if (!ctx.clip.has_intersection({
            rdp_rect.x,
            cb_rdp.progress_percent_y,
            rdp_rect.cx,
            gui.line_h
        }))
        {
            return ;
        }

        FcsStaticBuffer<32> fcs_buf { gui.font };
        fcs_buf.unchecked_push_u64(gui.progress.percent_progression);
        fcs_buf.unchecked_push_ch(gui.font.item('%').view);
        draw_centered_text(
            ctx, fcs_buf.msg(), colors.panel.fg,
            cb_rdp.inner_rect.cx, cb_rdp.progress_percent_y,
            InOutParam{gui.progress.percent_progression_previous_width}
        );
    }

    static void draw_progress_eta(DrawCtx & ctx)
    {
        auto & gui = ctx.gui;
        auto & cb_rdp = gui.layout.cb_rdp;
        auto & rdp_rect = cb_rdp.inner_rect;

        if (!ctx.clip.has_intersection({
            rdp_rect.x,
            cb_rdp.progress_eta_y,
            rdp_rect.cx,
            gui.line_h
        }))
        {
            return ;
        }

        constexpr std::size_t buf_len = 128;

        FontCharPtr fc_buf[buf_len];

        using namespace std::chrono_literals;

        char char_buffer[buf_len];
        auto fmt_buffer = make_writable_array_view(char_buffer);

        auto to_uint = [](auto n) { return static_cast<unsigned>(n.count()); };

        auto eta = gui.progress.eta_duration;

        chars_view str;

        if (!gui.progress.enable_eta)
        {
            // no message
        }
        else if (eta < 1min)
        {
            auto seconds = std::chrono::duration_cast<std::chrono::seconds>(eta);
            str = gui.tr.fmt(
                fmt_buffer,
                trkeys::vnc_ft_transfer_eta_seconds,
                to_uint(seconds)
            );
        }
        else if (eta < 1h)
        {
            auto minutes = std::chrono::duration_cast<std::chrono::minutes>(eta);
            auto seconds = std::chrono::duration_cast<std::chrono::seconds>(eta - minutes);
            str = gui.tr.fmt(
                fmt_buffer,
                trkeys::vnc_ft_transfer_eta_minutes_seconds,
                to_uint(minutes),
                to_uint(seconds)
            );
        }
        else if (eta < eta.max())
        {
            auto hours = std::chrono::duration_cast<std::chrono::hours>(eta);
            auto minutes = std::chrono::duration_cast<std::chrono::minutes>(eta - hours);
            str = gui.tr.fmt(
                fmt_buffer,
                trkeys::vnc_ft_transfer_eta_hours_minutes,
                to_uint(hours),
                to_uint(minutes)
            );
        }
        // else eta == eta.max()
        // => empty string

        FcsAndWidth msg = fcs_init(fc_buf, gui.font, str);

        draw_centered_text(
            ctx, msg, colors.panel.fg,
            cb_rdp.inner_rect.cx, cb_rdp.progress_eta_y,
            InOutParam{gui.progress.eta_progression_previous_width}
        );
    }

    struct FcsBuffer
    {
        FcsBuffer(Font & font, writable_array_view<Fcs::value_type> buf) noexcept
            : m_font(font)
            , m_buf(buf)
        {}

        FcsAndWidth msg() const noexcept
        {
            return {
                Fcs::assumed(m_buf.before(m_end)),
                checked_int{ m_text_width + !!m_text_width },
            };
        }

        FcsAndWidth msg_from_end() const noexcept
        {
            return {
                Fcs::assumed(m_end, m_end),
                0,
            };
        }

        FcsAndWidth msg_after(FcsAndWidth msg) const noexcept
        {
            assert(m_buf.begin() <= msg.fcs.begin());
            assert(msg.fcs.end() <= m_end);
            auto text_width = m_text_width + !!m_text_width - msg.text_width;
            return {
                Fcs::assumed(msg.fcs.end(), m_end),
                checked_int{ text_width + !!text_width },
            };
        }

        std::size_t remaining() const noexcept
        {
            return checked_int{ m_buf.end() - m_end };
        }

        void push_u64(uint64_t n) noexcept
        {
            push_ascii(int_to_decimal_chars(n));
        }

        void push_ascii(chars_view s) noexcept
        {
            unchecked_push_ascii(s.first(mmin(remaining(), s.size())));
        }

        void push_str(bytes_view s) noexcept
        {
            unchecked_push_str(s.first(mmin(remaining(), s.size())));
        }

        void push_ch(const FontCharView& fc) noexcept
        {
            if (remaining())
            {
                unchecked_push_ch(fc);
            }
        }

        void push_fcs(Fcs fcs) noexcept
        {
            unchecked_push_fcs(fcs.first(mmin(remaining(), fcs.size())));
        }

        void unchecked_push_u64(uint64_t n) noexcept
        {
            unchecked_push_ascii(int_to_decimal_chars(n));
        }

        void unchecked_push_ascii(chars_view s) noexcept
        {
            assert(remaining() >= s.size());
            auto msg = fcs_init_printable_ascii(m_end, m_font, s);
            m_text_width += msg.text_width - !s.empty();
            m_end = msg.fcs.end();
        }

        void unchecked_push_str(bytes_view s) noexcept
        {
#ifndef NDEBUG
            std::size_t n = 0;
            utf8_for_each(
                s,
                [&](uint32_t) { ++n; },
                [](utf8_char_invalid) {},
                [](utf8_char_truncated) {}
            );
            assert(remaining() >= n);
#endif
            auto msg = fcs_init(m_end, m_font, s);
            m_text_width += msg.text_width - !s.empty();
            m_end = msg.fcs.end();
        }

        void unchecked_push_str(UVncFile::PathView s) noexcept
        {
            assert(remaining() >= s.native().size());
            auto msg = fcs_init(m_end, m_font, s);
            m_text_width += msg.text_width - !s.native().empty();
            m_end = msg.fcs.end();
        }

        void unchecked_push_ch(const FontCharView& fc) noexcept
        {
            assert(remaining());
            m_text_width += fc.boxed_width();
            *m_end++ = &fc;
        }

        void unchecked_push_fcs(array_view<Fcs::value_type> fcs) noexcept
        {
            assert(remaining() >= fcs.size());
            for (auto * fc : fcs)
            {
                *m_end++ = fc;
                m_text_width += fc->boxed_width();
            }
        }

    private:
        Font & m_font;
        uint16_t m_text_width = 0;
        writable_array_view<Fcs::value_type> m_buf;
        FontCharPtr * m_end = m_buf.data();
    };

    template<std::size_t N>
    struct FcsStaticBuffer : FcsBuffer
    {
        FcsStaticBuffer(Font & font) noexcept
            : FcsBuffer(font, make_writable_array_view(m_fcs))
        {}

    private:
        Fcs::value_type m_fcs[N];
    };

    struct DrawProgressCtx
    {
        int y;
        TextId::E unit_text_id;
        uint64_t nb;
        uint64_t total;
        OutParam<uint16_t> previous_left_width;
        OutParam<uint16_t> previous_right_width;
        bool draw_total;
        bool use_human_size;
    };

    static void _draw_progress_elem(DrawCtx & ctx, DrawProgressCtx progress)
    {
        auto & gui = ctx.gui;

        auto & fc_space = gui.font.item(' ').view;

        FcsStaticBuffer<128> fcs_buf { gui.font };

        // puhs "{nb} {unit}"
        auto push_nb_and_unit = [&](uint64_t n){
            Fcs fcs_unit;
            if (progress.use_human_size && n > 1024)
            {
                HumanSizePowerOf2 human_size { n, '.' };
                fcs_buf.push_ascii(human_size.sv());
                fcs_unit = file_unit_to_fcs(gui, human_size.unit());
            }
            else
            {
                fcs_buf.push_u64(n);
                if (progress.use_human_size)
                {
                    fcs_unit = file_unit_to_fcs(gui, HumanSizePowerOf2::Unit::Byte);
                }
                else
                {
                    fcs_unit = fcs_and_width(gui, progress.unit_text_id).fcs;
                }
            }
            fcs_buf.push_ch(fc_space);
            fcs_buf.push_fcs(fcs_unit);
        };

        if (progress.use_human_size)
        {
            push_nb_and_unit(progress.nb);
        }
        else
        {
            fcs_buf.push_u64(progress.nb);
        }

        auto msg = fcs_buf.msg();
        auto left_text_width = msg.text_width;
        uint16_t right_pad = 0;

        // push " / {total} {unit}"
        if (progress.draw_total)
        {
            auto & fc_slash = gui.font.item('/').view;

            fcs_buf.push_ch(fc_space);
            fcs_buf.push_ch(fc_slash);
            fcs_buf.push_ch(fc_space);
            push_nb_and_unit(progress.total);

            msg = fcs_buf.msg();

            right_pad = update_width_and_compute_pad(
                OutParam{progress.previous_right_width},
                msg.text_width - left_text_width
            );
        }

        int x_text = gui.layout.cb_rdp.progress_right_text_limit - left_text_width;
        uint16_t left_pad = update_width_and_compute_pad(
            OutParam{progress.previous_left_width},
            left_text_width
        );
        int x = x_text - left_pad;
        auto pad = gdi::DrawTextPadding::LeftRight{ left_pad, right_pad };
        ctx.draw_text(x, progress.y, msg, colors.panel, pad);
    }

    enum class RedrawTotal : bool { No, Yes };

    static void draw_progress_items(DrawCtx & ctx, RedrawTotal redraw_total)
    {
        auto & gui = ctx.gui;
        _draw_progress_elem(ctx, {
            .y = gui.layout.cb_rdp.progress_transferred_items_y,
            .unit_text_id = TextId::transfer_item_unit,
            .nb = gui.progress.nb_copied,
            .total = gui.progress.total_items,
            .previous_left_width = OutParam{gui.progress.transferred_items_previous_left_width},
            .previous_right_width = OutParam{gui.progress.transferred_items_previous_right_width},
            .draw_total = (redraw_total == RedrawTotal::Yes),
            .use_human_size = false,
        });
    }

    static void draw_progress_bytes(DrawCtx & ctx, RedrawTotal redraw_total)
    {
        auto & gui = ctx.gui;
        _draw_progress_elem(ctx, {
            .y = gui.layout.cb_rdp.progress_transferred_bytes_y,
            .unit_text_id = TextId::transfer_byte_unit,
            .nb = gui.progress.transferred_bytes,
            .total = gui.progress.total_bytes,
            .previous_left_width = OutParam{gui.progress.transferred_bytes_previous_left_width},
            .previous_right_width = OutParam{gui.progress.transferred_bytes_previous_right_width},
            .draw_total = (redraw_total == RedrawTotal::Yes),
            .use_human_size = true,
        });
    }

    static void update_vnc_start_event(GuiData & gui, VncState vnc_state)
    {
        bool remove_nav = D::has_nav(gui);
        bool remove_vnc_all_checkbox = !gui.disabled_elements.has(D::ElementId::VncListAllCheckbox);

        gui.vnc.reset();

        if (gui.m_storage.allocated > D::max_allocated_memory_before_free)
        {
            gui.vnc.free();
            gui.m_storage.release_blocks();
        }

        bool update_root = false;
        bool update_parent = false;

        switch (vnc_state)
        {
            case VncState::Disabled:
                update_root = !gui.disabled_elements.has(D::ElementId::VncRootButton);
                update_parent = !gui.disabled_elements.has(D::ElementId::VncParentButton);
                break;

            case VncState::NoDirectory:
            case VncState::Loading:
            case VncState::Empty:
            case VncState::Error:
            case VncState::Ready:
                update_root = gui.disabled_elements.has(D::ElementId::VncRootButton);
                update_parent = gui.disabled_elements.has(D::ElementId::VncParentButton)
                             != gui.vnc.directory.empty();
                break;
        }

        if (update_root)
        {
            gui.disabled_elements.toggle(D::ElementId::VncRootButton);
        }

        if (update_parent)
        {
            gui.disabled_elements.toggle(D::ElementId::VncParentButton);
        }

        gui.disabled_elements.add(D::ElementId::VncListAllCheckbox);
        gui.disabled_elements.add(D::ElementId::VncNavigation);
        gui.disabled_elements.add(D::ElementId::VncList);
        gui.vnc.all_file_checked = false;

        auto old_focus = gui.focus_item;
        bool update_vnc_sort_icon = D::is_vnc_sort_header(gui.focus_item);

        if (D::is_vnc_list(gui.focus_item))
        {
            gui.focus_item = ElementId::None;
            gui.pressed_item = ElementId::None;

            // redraw header icon on bluring
            if (gui.is_open())
            {
                if (old_focus == GuiData::ElementId::VncIconSortFilename)
                {
                    draw_vnc_sorting_icon_filename(gui, D::OptionalClip{});
                }
                else if (old_focus == GuiData::ElementId::VncIconSortSize)
                {
                    draw_vnc_sorting_icon_size(gui, D::OptionalClip{});
                }
                else if (old_focus == GuiData::ElementId::VncIconSortDate)
                {
                    draw_vnc_sorting_icon_date(gui, D::OptionalClip{});
                }
            }
        }

        if ((update_root || update_parent || remove_vnc_all_checkbox || remove_nav
          || update_vnc_sort_icon)
         && gui.is_open())
        {
            D::DrawCtx ctx{gui, gui.layout.vnc.inner_rect};

            if (update_root)
            {
                ctx.draw_vnc_root_button();
            }

            if (update_parent)
            {
                ctx.draw_vnc_parent_button();
            }

            if (remove_vnc_all_checkbox)
            {
                ctx.draw_vnc_all_file_checkbox();
            }

            if (remove_nav)
            {
                ctx.draw_rect(gui.vnc.pagination.get_rect(), D::colors.panel.bg);
            }

            if (update_vnc_sort_icon)
            {
                if (old_focus == GuiData::ElementId::VncIconSortFilename)
                {
                    draw_vnc_sorting_icon_filename(gui, D::OptionalClip{});
                }
                else if (old_focus == GuiData::ElementId::VncIconSortSize)
                {
                    draw_vnc_sorting_icon_size(gui, D::OptionalClip{});
                }
                else if (old_focus == GuiData::ElementId::VncIconSortDate)
                {
                    draw_vnc_sorting_icon_date(gui, D::OptionalClip{});
                }
            }
        }

        update_vnc_list(gui, vnc_state);
        update_copy_buttons(gui);
    }

    static void update_vnc_list(GuiData & gui, VncState next_display_state)
    {
        if (gui.is_open())
        {
            clear_vnc_part(gui);
        }

        gui.vnc.state = next_display_state;

        if (gui.is_open())
        {
            draw_vnc_part(gui, gui.layout.vnc.inner_rect, ComputeMode::Update);
        }
    }

    struct VncTextList
    {
        Fcs fcs;
        Rect rect;

        VncTextList(VNC::FileTransferGui::GuiData& gui, TextId::E item)
        {
            auto text = fcs_and_width(gui, item);
            auto & layout = gui.layout;
            int x = layout.vnc.inner_rect.x + middle_pos(layout.vnc.inner_rect.cx, text.text_width);
            int y = layout.vnc.list_y + layout.vnc.list_item_h;
            fcs = text.fcs;
            rect = {
                checked_int{x},
                checked_int{y},
                text.text_width,
                gui.line_h,
            };
        }
    };

    static void clear_vnc_part(GuiData & gui)
    {
        auto & layout = gui.layout;
        auto vnc_pan = layout.vnc.inner_rect;

        Rect cleared_area;

        switch (gui.vnc.state)
        {
            case VncState::NoDirectory:
                return;

            case VncState::Disabled:
                cleared_area = VncTextList{gui, TextId::vnc_list_disabled}.rect;
                break;

            case VncState::Loading:
                cleared_area = VncTextList{gui, TextId::vnc_list_loading}.rect;
                break;

            case VncState::Empty:
                cleared_area = VncTextList{gui, TextId::vnc_list_empty}.rect;
                break;

            case VncState::Error:
                cleared_area = VncTextList{gui, TextId::vnc_list_error}.rect;
                break;

            case VncState::Ready: {
                if (!layout.vnc.last_y_drawing)
                {
                    return ;
                }

                // clear list text info
                if (layout.vnc.list_total_selected_x)
                {
                    gui.gd.draw(
                        RDPOpaqueRect(
                            Rect{
                                layout.vnc.list_total_x,
                                layout.vnc.list_total_y,
                                checked_int {
                                    layout.vnc.list_total_selected_x - layout.vnc.list_total_x
                                    + gui.text_widths[TextId::vnc_list_selected_items]
                                    + 1
                                },
                                gui.line_h,
                            },
                            D::colors.panel.bg
                        ),
                        vnc_pan, gdi::ColorCtx::depth24()
                    );
                }
                gui.text_widths[D::TextId::vnc_list_selected_items] = 0;

                if (has_nav(gui))
                {
                    gui.gd.draw(
                        RDPOpaqueRect(gui.vnc.pagination.get_rect(), D::colors.panel.bg),
                        vnc_pan, gdi::ColorCtx::depth24()
                    );
                }

                clear_vnc_list(gui);
                return;
            }
        }

        gui.gd.draw(
            RDPOpaqueRect(cleared_area, D::colors.panel.bg),
            vnc_pan, gdi::ColorCtx::depth24()
        );
    }

    static void clear_vnc_list(GuiData & gui)
    {
        auto & layout = gui.layout;
        auto vnc_pan = layout.vnc.inner_rect;
        auto y = layout.vnc.header_y + layout.vnc.header_h + D::COLUMN_BORDER_LEN;
        gui.gd.draw(
            RDPOpaqueRect(
                Rect {
                    vnc_pan.x,
                    checked_int{ y },
                    vnc_pan.cx,
                    checked_int{ layout.vnc.last_y_drawing - y },
                },
                D::colors.panel.bg
            ),
            vnc_pan,
            gdi::ColorCtx::depth24()
        );
        layout.vnc.last_y_drawing = 0;
    }

    static void draw_vnc_part(GuiData& gui, Rect clip, ComputeMode mode)
    {
        auto & layout = gui.layout;

        clip = clip.intersect(layout.vnc.inner_rect);

        auto draw_text = [&](TextId::E item) {
            VncTextList text_list{gui, item};
            gdi::draw_text(
                gui.gd, text_list.rect.x, text_list.rect.y, gui.line_h, gdi::DrawTextPadding{},
                text_list.fcs, colors.panel.fg, colors.panel.bg, clip
            );
        };

        switch (gui.vnc.state)
        {
            case VncState::NoDirectory:
                break;

            case VncState::Disabled:
                draw_text(TextId::vnc_list_disabled);
                break;

            case VncState::Loading:
                draw_text(TextId::vnc_list_loading);
                break;

            case VncState::Empty:
                draw_text(TextId::vnc_list_empty);
                break;

            case VncState::Error:
                draw_text(TextId::vnc_list_error);
                break;

            case VncState::Ready:
                draw_vnc_list(gui, clip, mode);
                break;
        }
    }

    struct ItemIndices
    {
        MaxFileIntType current_index;
        MaxFileIntType selected_index;
    };

    struct Item
    {
        Colors::Item colors[3];

        Item(GuiData & gui, MaxFileIntType start_page_index) noexcept
            : colors {
                // lines
                D::colors.list.lines[(start_page_index & 1) ? 1 : 0],
                D::colors.list.lines[(start_page_index & 1) ? 0 : 1],
                // active
                (gui.focus_item == ElementId::VncList)
                    ? D::colors.list.focus
                    : D::colors.list.selected,
            }
        {}

        Colors::Item current_colors(ItemIndices indices) const noexcept
        {
            auto i = (indices.selected_index == indices.current_index)
                ? 2
                : (indices.current_index & 1);
            return colors[i];
        }

        Colors::Item focused_colors() const noexcept
        {
            return colors[2];
        }

        Colors::Item unfocused_colors(MaxFileIntType index) const noexcept
        {
            return colors[index & 1];
        }
    };

    static MaxFileIntType widget_page_to_page_index(GuiData const & gui, uint32_t page) noexcept
    {
        assert(page > 0);
        return (page - 1u) * gui.layout.vnc.item_by_page_with_nav;
    }

    static MaxFileIntType get_current_page_index(GuiData const & gui) noexcept
    {
        return widget_page_to_page_index(gui, gui.vnc.current_page + 1);
    }

    static void set_widget_page(
        GuiData & gui,
        uint32_t new_widget_page,
        uint32_t new_selected_index) noexcept
    {
        LOG(LOG_DEBUG, "page:: %u -> %u", gui.vnc.current_page, new_widget_page - 1u);
        gui.vnc.current_page = new_widget_page - 1u;
        gui.vnc.selected_index = new_selected_index;
        gui.vnc.vim_mode.reset_move();
        D::clear_vnc_list(gui);
        D::draw_vnc_list(gui, gui.layout.vnc.inner_rect, D::ComputeMode::UpdateRefresh);
    }

    struct PageFile
    {
        /// files of the current page
        array_view<MaxFileIntType> file_indices;
        /// index of the current page
        MaxFileIntType page_index;

        PageFile(GuiData & gui)
            : file_indices{ gui.vnc.sorted_indices() }
            , page_index{ get_current_page_index(gui) }
        {
            assert(page_index <= gui.vnc.selected_index);

            // drop previous page
            file_indices = file_indices.drop_front(page_index);
            // keep items for one page
            auto nb_items = mmin(file_indices.size(), gui.layout.vnc.item_by_page_with_nav);
            file_indices = file_indices.first(nb_items);
        }
    };

    static int y_item_from_offset(GuiData const & gui, MaxFileIntType offset) noexcept
    {
        return checked_int {
            gui.layout.vnc.list_y
            + checked_cast<int>(offset * gui.layout.vnc.list_item_h)
        };
    }

    static int y_item_from_selected_index(GuiData const & gui) noexcept
    {
        auto page_index = get_current_page_index(gui);
        auto offset = gui.vnc.selected_index - page_index;
        return D::y_item_from_offset(gui, offset);
    }

    static MaxFileIntType offset_item_from_y(GuiData const & gui, uint16_t y) noexcept
    {
        assert(y >= gui.layout.vnc.list_y);
        return checked_int { (y - gui.layout.vnc.list_y) / gui.layout.vnc.list_item_h };
    }

    static void draw_vnc_list(GuiData& gui, Rect clip, ComputeMode mode)
    {
        auto & layout = gui.layout;

        bool has_nav = D::has_nav(gui);
        auto list_h = layout.vnc.list_minus_nav_h;

        int y = layout.vnc.list_y;

        Rect file_name_clip = clip.intersect({
            layout.vnc.header_filename_x,
            layout.vnc.header_y,
            layout.vnc.header_filename_w,
            checked_int{ y + list_h },
        });

        auto [file_indices, page_index] = PageFile{ gui };
        auto start_idx = page_index;

        /*
         * File list based on clip.y
         */

        if (file_name_clip.y < clip.y)
        {
            // pixel ignored based on line height
            auto n_pixel = clip.y - file_name_clip.y;

            auto skip_elem = mmin(
                file_indices.size(),
                static_cast<unsigned>(n_pixel) / layout.vnc.list_item_h
            );

            file_indices = file_indices.drop_front(skip_elem);
            start_idx += skip_elem;
            y += skip_elem * layout.vnc.list_item_h;
        }

        Item item_colors { gui, page_index };
        ItemIndices item_indices {
            .current_index = start_idx,
            .selected_index = gui.vnc.selected_index,
        };

        /*
         * Draw file list
         */

        for (auto sorted_indice : file_indices)
        {
            auto colors = item_colors.current_colors(item_indices);
            draw_vnc_list_item(gui, gui.vnc.files[sorted_indice], y, colors, file_name_clip, clip);

            ++item_indices.current_index;
            y += layout.vnc.list_item_h;

            if (clip.ebottom() <= y)
            {
                break;
            }
        }

        auto nb_display_item = item_indices.current_index - start_idx;

        if (mode != ComputeMode::Refresh)
        {
            layout.vnc.last_y_drawing = checked_int{
                !file_indices.empty()
                ? y + 1 /* TODO fix bug with draw_text() */
                : 0
            };
        }

        /*
         * Draw column borders
         */
        if (!file_indices.empty())
        {
            draw_vnc_list_item_borders(gui, layout.vnc.list_y, nb_display_item, clip);
        }

        /*
         * Draw list info
         */
        if (mode != ComputeMode::UpdateRefresh)
        {
            gdi::draw_text(
                gui.gd,
                gui.layout.vnc.list_total_x,
                gui.layout.vnc.list_total_y,
                gui.line_h,
                gdi::DrawTextPadding{},
                D::fcs(gui, D::TextId::vnc_list_total_items),
                colors.panel.fg_total_items,
                colors.panel.bg,
                clip
            );

            // draw selected items
            auto x = gui.layout.vnc.list_total_selected_x;
            if (gui.vnc.selection_counter && clip.x <= x && x <= clip.eright()
             && (!has_nav || x < gui.vnc.pagination.x()))
            {
                int16_t x_end_text = checked_int {
                    gui.layout.vnc.list_total_selected_x
                  + gui.text_widths[TextId::vnc_list_selected_items]
                };

                auto x2 = mmin({x_end_text, clip.eright()});
                if (has_nav)
                {
                    x2 = mmin(x2, gui.vnc.pagination.x());
                }

                gdi::draw_text(
                    gui.gd,
                    x,
                    gui.layout.vnc.list_total_y,
                    gui.line_h,
                    gdi::DrawTextPadding{},
                    D::fcs(gui, D::TextId::vnc_list_selected_items),
                    colors.panel.fg_selected_item,
                    colors.panel.bg,
                    Rect{x, clip.y, checked_int{ x2 - x }, clip.cy}
                );

                draw_vnc_selected_items_sep(gui, clip);
            }
        }

        /*
         * Draw navigation bar
         */
        if (has_nav && mode != ComputeMode::UpdateRefresh)
        {
            gui.vnc.pagination.rdp_input_invalidate(clip);
        }
    }

    static void draw_vnc_selected_items_sep(GuiData& gui, Rect clip)
    {
        auto x = gui.layout.vnc.list_total_selected_x
               - gui.layout.vnc.list_total_selected_margin_left / 2;

        gui.gd.draw(
            RDPOpaqueRect(
                Rect{
                    checked_int{x},
                    gui.layout.vnc.list_total_y,
                    1,
                    gui.line_h
                },
                colors.panel.fg_total_items
            ),
            clip,
            gdi::ColorCtx::depth24()
        );
    }

    static void draw_vnc_list_item_borders(
        GuiData& gui,
        int16_t y,
        MaxFileIntType nb_display_item,
        Rect clip)
    {
        auto & layout = gui.layout;

        Rect line_rect {
            layout.vnc.list_vline_icon_x,
            y,
            D::COLUMN_BORDER_LEN,
            checked_int{ nb_display_item * layout.vnc.list_item_h },
        };

        gui.gd.draw(
            RDPOpaqueRect(line_rect, D::colors.list.column_sep),
            clip, gdi::ColorCtx::depth24()
        );

        line_rect.x = layout.vnc.list_vline_size_x;

        gui.gd.draw(
            RDPOpaqueRect(line_rect, D::colors.list.column_sep),
            clip, gdi::ColorCtx::depth24()
        );

        // special files have not date column
        if (gui.vnc.has_date_border())
        {
            line_rect.x = layout.vnc.list_vline_date_x;
            gui.gd.draw(
                RDPOpaqueRect(line_rect, D::colors.list.column_sep),
                clip, gdi::ColorCtx::depth24()
            );
        }
    }

    static void draw_vnc_list_item(
        GuiData& gui,
        FileData & file,
        int y,
        Colors::Item colors,
        Rect file_name_clip,
        Rect clip)
    {
        auto & layout = gui.layout;

        using Color = Widget::Color;

        /*
         * Lazy initialization
         */
        if (!file.fcs)
        {
            initialize_list_item(gui, InOutParam{file});
        }

        uint16_t line_h = gui.line_h - 1; // TODO fix draw_text() bug

        auto draw_text = [&](int x, Fcs fcs, Color fg, Rect clip, uint16_t line_h, int dy = 0) {
            gdi::DrawTextPadding::Y pad { LINE_Y_PADDING };
            gdi::draw_text(gui.gd, x, y+dy, line_h, pad, fcs, fg, colors.bg, clip);
        };

        /*
         * Clear line
         */

        gui.gd.draw(
            RDPOpaqueRect(
                Rect{
                    layout.vnc.list_checkbox_x,
                    checked_int{y},
                    layout.vnc.inner_rect.cx,
                    checked_int{layout.vnc.list_item_h}, // TODO fix draw_text() bug
                },
                colors.bg
            ),
            clip,
            gdi::ColorCtx::depth24()
        );

        /*
         * Draw checkbox
         */

        Checkbox checkbox{ gui, file.checked };
        checkbox.draw(
            gui.gd, y, colors.to_text_colors(), clip,
            gdi::DrawTextPadding::Y{ LINE_Y_PADDING }
        );

        /*
         * Draw file icon
         */

        auto file_icon = gui.icons.fcs_file(file.file_type);
        draw_text(
            layout.vnc.list_file_icon_x + file_icon.adjust_file_icon_x(gui),
            file_icon, colors.fg_icon, clip,
            file_icon.boxed_height(), file_icon.adjust_middle_height(gui)
        );

        /*
         * Draw path
         */

        Fcs path {file.fcs, file.fcs_name_len};
        draw_text(layout.vnc.header_filename_x, path, colors.fg, file_name_clip, line_h);

        /*
         * Get size and modification date text
         */

        Fcs fcs_size;
        Fcs fcs_date;

        Widget::Color size_color = colors.fg;

        switch (file.file_type)
        {
            case FileDataType::LocalDisk:
            case FileDataType::MediaDisk:
            case FileDataType::NetworkDisk:
            case FileDataType::CDRom:
            {
                auto i = TextId::START_DRIVE_TEXT + underlying_cast(file.file_type);
                fcs_size = fcs_for_static_str(gui, static_cast<TextId::E>(i));
                size_color = colors.fg_dir;
                break;
            }

            case FileDataType::Shortcut:
                break;

            case FileDataType::RegularFile:
                fcs_size = {file.fcs + file.fcs_name_len, file.fcs_file_size_len};
                fcs_date = {fcs_size.end(), uint8_t{FileDateFormat::output_length}};
                break;

            case FileDataType::Directory:
                fcs_size = fcs(gui, TextId::folder);
                fcs_date = {file.fcs + file.fcs_name_len, uint8_t{FileDateFormat::output_length}};
                size_color = colors.fg_dir;
                break;
        }

        /*
         * Draw size
         */

        if (!fcs_size.empty())
        {
            int x = layout.vnc.header_size_x + file.file_size_offset_x;
            draw_text(x, fcs_size, size_color, clip, line_h);
        }

        /*
         * Draw modification date
         */

        if (!fcs_date.empty())
        {
            draw_text(layout.vnc.header_date_x, fcs_date, colors.fg, clip, line_h);
        }
    }

    static Rect get_vnc_list_rect(GuiData const & gui) noexcept
    {
        auto item_by_page = gui.layout.vnc.item_by_page_with_nav;
        auto file_start_idx = gui.vnc.current_page * item_by_page;
        auto file_nb = mmin(gui.vnc.files.size() - file_start_idx, item_by_page);

        return Rect {
            gui.layout.vnc.list_checkbox_x,
            gui.layout.vnc.list_y,
            gui.layout.vnc.inner_rect.cx,
            checked_int{ file_nb * gui.layout.vnc.list_item_h },
        };
    }

    struct PageIndices
    {
        MaxFileIntType page_index;
        // inclusive index of last element in the page
        MaxFileIntType end_page_index;

        PageIndices(GuiData const & gui)
            : page_index{ get_current_page_index(gui) }
            , end_page_index {
                mmin(page_index + gui.layout.vnc.item_by_page_with_nav, gui.vnc.files.size())
                - MaxFileIntType{1}
            }
        {}
    };

    static void draw_vnc_list_one_item_with_borders(
        GuiData & gui,
        Rect vnc_list_rect,
        D::FileData & file,
        int y_item,
        Colors::Item colors)
    {
        auto file_name_clip = vnc_list_rect;
        file_name_clip.x = gui.layout.vnc.header_filename_x;
        file_name_clip.cx = gui.layout.vnc.header_filename_w;

        LOG(LOG_DEBUG, "y = %d | list_y=%d | item_h=%d | color = %s",
            y_item,
            gui.layout.vnc.list_y,
            gui.layout.vnc.list_item_h,
            colors.bg.as_bgr().as_u32() == D::colors.list.focus.bg.as_bgr().as_u32()
            ? "focus"
            : colors.bg.as_bgr().as_u32() == D::colors.list.selected.bg.as_bgr().as_u32()
            ? "selected"
            : "regular"
        );

        draw_vnc_list_item(
            gui,
            file,
            y_item,
            colors,
            file_name_clip,
            vnc_list_rect
        );
        draw_vnc_list_item_borders(gui, checked_int{y_item}, 1, vnc_list_rect);
    }

    static void update_vnc_selection(GuiData & gui, MaxFileIntType inc_selection_counter)
    {
        constexpr auto id = TextId::vnc_list_selected_items;

        if (!inc_selection_counter)
        {
            return ;
        }

        gui.vnc.selection_counter += inc_selection_counter;

        /*
         * Toggle all checkbox when necessary
         */

        auto should_be_checked = (gui.vnc.selection_counter == gui.vnc.files.size());
        if (gui.vnc.all_file_checked != should_be_checked)
        {
            gui.vnc.all_file_checked = should_be_checked;
            DrawCtx{gui, gui.layout.vnc.inner_rect}.draw_vnc_all_file_checkbox();
        }

        /*
         * Remove selection part text
         */

        if (!gui.vnc.selection_counter)
        {
            if (gui.text_widths[id])
            {
                auto x_margin = gui.layout.vnc.list_total_selected_margin_left / 2;
                gui.gd.draw(
                    RDPOpaqueRect(
                        Rect{
                            checked_int{ gui.layout.vnc.list_total_selected_x - x_margin },
                            gui.layout.vnc.list_total_y,
                            checked_int{ gui.text_widths[id] + x_margin },
                            gui.line_h,
                        },
                        D::colors.panel.bg
                    ),
                    gui.layout.vnc.inner_rect, gdi::ColorCtx::depth24()
                );
                gui.text_widths[id] = 0;
            }
        }

        /*
         * Update selection part text
         */

        else
        {
            auto old_w = gui.text_widths[id];
            auto msg = init_dynamic_fcs_and_lengths(
                gui, id,
                Translator::FmtMsg<128>{
                    gui.tr, trkeys::vnc_ft_vnc_list_selected_items,
                    gui.vnc.selection_counter,
                }
            );
            auto new_w = msg.text_width;

            uint16_t right_pad = (new_w < old_w) ? old_w - new_w : 0;

            gdi::draw_text(
                gui.gd,
                gui.layout.vnc.list_total_selected_x,
                gui.layout.vnc.list_total_y,
                gui.line_h,
                gdi::DrawTextPadding::Right{right_pad},
                msg.fcs,
                colors.panel.fg_selected_item,
                colors.panel.bg,
                gui.layout.vnc.inner_rect
            );

            // draw separator
            if (!old_w)
            {
                draw_vnc_selected_items_sep(gui, gui.layout.vnc.inner_rect);
            }
        }

        update_to_rdp_copy_button(gui);
    }

    static void initialize_list_item(GuiData & gui, InOutParam<FileData> inout_file)
    {
        auto & file = inout_file.inout_value;

        auto human_date = make_human_date(file.last_access_time);
        HumanSizePowerOf2 human_file_size { file.file_size, '.' };

        UVncFile::PathView file_name = file.file_name_av();

        auto is_file = file.is_file();

        auto extra_len = FileDateFormat::output_length
                       + HumanSizePowerOf2::max_size
                       + gui.max_unit_nb_fc
                       + 1;
        auto fcs = gui.allocate_array<FontCharPtr>(file_name.native().size() + extra_len);

        auto file_size_offset_x = gui.folder_right_pad;

        D::FcsBuffer fcs_buffer{ gui.font, fcs };

        fcs_buffer.unchecked_push_str(file_name);
        auto fcs_file_name = fcs_buffer.msg();

        auto fcs_file_size = fcs_buffer.msg_from_end();
        if (is_file)
        {
            // push size
            fcs_buffer.unchecked_push_ascii(human_file_size.sv());

            // push separator
            fcs_buffer.unchecked_push_ch(gui.font.item(' ').view);

            // push unit
            auto unit_index = underlying_cast(human_file_size.unit());
            fcs_buffer.unchecked_push_fcs(file_unit_to_fcs(gui, human_file_size.unit()));

            // update
            fcs_file_size = fcs_buffer.msg_after(fcs_file_name);

            // align on units
            file_size_offset_x = checked_int{
                gui.max_file_size_w
                - fcs_file_size.text_width
                - gui.unit_right_pads[unit_index]
            };
        }
        fcs_buffer.unchecked_push_str(human_date.sv());
        auto fcs_date = fcs_buffer.msg_after(fcs_file_size);
        assert(fcs_date.fcs.size() == FileDateFormat::output_length);

        gui.free_after(fcs_date.fcs.end());

        file = FileData {
            .file_size = file.file_size,
            .last_access_time = file.last_access_time,
            .file_name = file.file_name,
            .fcs = fcs.data(),
            .fcs_name_len = checked_int{fcs_file_name.fcs.size()},
            .file_name_len = file.file_name_len,
            .fcs_file_size_len = checked_int{fcs_file_size.fcs.size()},
            .file_size_offset_x = file_size_offset_x,
            .file_type = file.file_type,
            .checked = file.checked,
        };
    }

    static void draw_close_button(GuiData const & gui)
    {
        LOG(LOG_DEBUG, "draw_close_button");
        auto btn = gui.icons.close_x;
        gdi::draw_text(
            gui.gd,
            gui.layout.close_btn.rect.x - btn->offsetx - 1,
            gui.layout.close_btn.rect.y - btn->offsety,
            gui.line_h,
            gdi::DrawTextPadding{
                .top = checked_int{gui.layout.close_btn.y_pad},
                .right = checked_int{gui.layout.close_btn.x_pad},
                .bottom = checked_int{gui.layout.close_btn.y_pad + 1},
                .left = checked_int{gui.layout.close_btn.x_pad},
            },
            {&btn, 1},
            colors.window.close.fg,
            gui.pressed_item == ElementId::GuiClose
                ? colors.window.close.active_bg
                : colors.window.close.bg,
            gui.layout.close_btn.rect
          );
    }

    static void activate_sorting_icon_and_draw(
        GuiData & gui, bool triggered, VncData::SortedField selected_field)
    {
        if (!triggered)
        {
            return ;
        }

        auto draw_icon = [&gui](VncData::SortedField field){
            if (field == VncData::SortByName)
            {
                draw_vnc_sorting_icon_filename(gui, D::OptionalClip{});
            }
            else if (field == VncData::SortBySize)
            {
                draw_vnc_sorting_icon_size(gui, D::OptionalClip{});
            }
            else if (field == VncData::SortByDate)
            {
                draw_vnc_sorting_icon_date(gui, D::OptionalClip{});
            }
        };

        auto current_field = gui.vnc.sorted_field();
        auto reversed = VncData::SortedField(selected_field & VncData::SortReverse);

        // toggle ascending <-> descending
        if (selected_field == current_field)
        {
            gui.vnc.sorted_field_with_option = static_cast<VncData::SortedField>(
                gui.vnc.sorted_field_with_option ^ VncData::SortReverse
            );
        }
        // select new column
        else
        {
            gui.vnc.sorted_field_with_option = VncData::SortedField(selected_field | reversed);
            // redraw the previous activated icon
            draw_icon(current_field);
        }

        // draw file list
        if (gui.vnc.state == VncState::Ready)
        {
            auto has_selected_index = (gui.vnc.previous_selected_index != INVALID_INDEX);
            auto ifile_selected = has_selected_index
                ? gui.vnc.sorted_indices()[gui.vnc.previous_selected_index]
                : MaxFileIntType{};

            if (init_vnc_sorted_based_on_field_with_option(gui))
            {
                gui.vnc.previous_selected_index = INVALID_INDEX;

                // compute new selected index
                auto sorted_indices = gui.vnc.sorted_indices();
                for (auto & ifile : sorted_indices)
                {
                    if (ifile == ifile_selected)
                    {
                        gui.vnc.previous_selected_index = checked_int {
                            &ifile - sorted_indices.begin()
                        };
                        break;
                    }
                }

                draw_vnc_list(gui, gui.layout.vnc.inner_rect, ComputeMode::UpdateRefresh);
            }
        }

        draw_icon(selected_field);
    }

    struct OptionalClip
    {
        bool use_clip {};
        Rect clip {};

        explicit OptionalClip() = default;

        OptionalClip(Rect clip) noexcept
            : use_clip(true)
            , clip(clip)
        {}
    };

    static void draw_sorting_icon(
        GuiData const & gui, ElementId elem, VncData::SortedField field, int x_end,
        OptionalClip optional_clip, Icons::Sorting sorting_icons)
    {
        auto active = (field == gui.vnc.sorted_field());
        auto & icon = active && (gui.vnc.sorted_field_with_option & D::VncData::SortReverse)
            ? sorting_icons.descending
            : sorting_icons.ascending;
        int w = icon->offsetx * 2 + icon->width + 2;
        int x = x_end - w;
        int y = gui.layout.vnc.header_text_y;

        Rect rect {
            checked_int{x},
            checked_int{y},
            checked_int{icon->boxed_width() + 1},
            checked_int{icon->boxed_height() + 1},
        };

        if (optional_clip.use_clip)
        {
            rect = rect.intersect(optional_clip.clip);
            if (rect.isempty())
            {
                return;
            }
        }

        auto fg = (gui.focus_item == elem)
            ? (active ? colors.icon_sort_button.focus_activated : colors.icon_sort_button.focus)
            : (active ? colors.icon_sort_button.activated : colors.icon_sort_button.disabled);
        gdi::draw_text(
            gui.gd,
            x,
            y,
            gui.line_h,
            gdi::DrawTextPadding{},
            {&icon, uint8_t{1}},
            fg,
            colors.panel.bg,
            rect
        );
    }

    static void draw_vnc_sorting_icon_filename(GuiData const & gui, OptionalClip optional_clip)
    {
        draw_sorting_icon(gui, D::ElementId::VncIconSortFilename, VncData::SortByName,
                          gui.layout.vnc.header_size_x - COLUMN_X_PADDING, optional_clip,
                          gui.icons.sort_a_to_z);
    }

    static void draw_vnc_sorting_icon_size(GuiData const & gui, OptionalClip optional_clip)
    {
        draw_sorting_icon(gui, D::ElementId::VncIconSortSize, VncData::SortBySize,
                          gui.layout.vnc.header_date_x - COLUMN_X_PADDING, optional_clip,
                          gui.icons.sort_9_to_1);
    }

    static void draw_vnc_sorting_icon_date(GuiData const & gui, OptionalClip optional_clip)
    {
        draw_sorting_icon(gui, D::ElementId::VncIconSortDate, VncData::SortByDate,
                          gui.layout.vnc.inner_rect.eright(), optional_clip,
                          gui.icons.sort_1_to_9);
    }

    static bool init_sorted_array(GuiData & gui, VncData::SortedData::Array & array)
    {
        if (array.initialized)
        {
            gui.vnc.sorted.indices = array.indices;
            return true;
        }

        if (array.capacity < gui.vnc.files.size())
        {
            auto av = gui.allocate_array<MaxFileIntType>(gui.vnc.files.size());
            array.indices = av.data();
            array.capacity = checked_int{ av.size() };
        }
        array.initialized = true;
        gui.vnc.sorted.indices = array.indices;
        return false;
    }

    static void init_vnc_sorted_name(GuiData & gui)
    {
        if (init_sorted_array(gui, gui.vnc.sorted.name))
        {
            return ;
        }

        MaxFileIntType counter = 0;
        auto * dir_indices = gui.vnc.sorted.indices;
        auto * file_indices = dir_indices + gui.vnc.directory_counter;
        for (auto const & file : gui.vnc.files)
        {
            if (file.is_file())
            {
                *file_indices++ = counter;
            }
            else
            {
                *dir_indices++ = counter;
            }
            counter++;
        }
    }

    template<auto Mem>
    static void init_vnc_sorted_by_field(
        GuiData & gui, VncData::SortedData::Array & array, bool sorted_dir)
    {
        if (init_sorted_array(gui, array))
        {
            return ;
        }

        // sorted.name.indices contains folders then files
        // new indices contains files then folders

        auto nb_files = gui.vnc.files.size();

        auto in_start_dir = gui.vnc.sorted.name.indices;
        auto in_end_dir_start_file = in_start_dir + gui.vnc.directory_counter;
        auto in_end_file = in_start_dir + nb_files;

        auto out_start_file = array.indices;
        auto out_end_file_start_dir = out_start_file + gui.vnc.file_counter;
        auto out_end_dir = out_start_file + nb_files;

        std::copy(in_end_dir_start_file, in_end_file, out_start_file);
        std::copy(in_start_dir, in_end_dir_start_file, out_end_file_start_dir);

        auto cmp = [&](MaxFileIntType i, MaxFileIntType j) {
            return gui.vnc.files[i].*Mem > gui.vnc.files[j].*Mem;
        };

        std::stable_sort(out_start_file, out_end_file_start_dir, cmp);

        if (sorted_dir)
        {
            std::stable_sort(out_end_file_start_dir, out_end_dir, cmp);
        }
    }

    /// Init the sorting indices array with Windows behavior.
    /// Separate directories and files.
    /// By size: directories not sorted.
    /// Ascending filename is alphabetic + directory to top.
    /// Ascending size from big to lower + directory to end.
    /// Ascending date from recent to old + directory to end.
    /// \return true when draw is required.
    static bool init_vnc_sorted_based_on_field_with_option(GuiData & gui)
    {
        auto current_field = (gui.vnc.sorted_field_with_option & ~VncData::SortReverse);

        auto reversed_flag = (gui.vnc.sorted_field_with_option & VncData::SortReverse);

        enum class SplitFileAndDir : bool { No, Yes };

        auto reverse_when_enabled = [&](
            VncData::SortedData::Array const & in_array,
            VncData::SortedData::Array & out_array,
            bool sorted_dir
        ){
            if (reversed_flag && !init_sorted_array(gui, out_array))
            {
                // in contains files then folders
                // out contains folders then files
                auto nb_files = gui.vnc.files.size();
                auto * out = out_array.indices;
                auto * in = in_array.indices;

                // copy folders to begin of list without reverse
                if (!sorted_dir && gui.vnc.directory_counter)
                {
                    auto * in_dir = in + gui.vnc.file_counter;
                    auto len = gui.vnc.directory_counter;
                    std::copy(in_dir, in_dir + len, out);
                    nb_files -= len;
                    out += len;
                }

                // copy files to end of list then reverse
                std::copy(in, in + nb_files, out);
                std::reverse(out, out + nb_files);
            }
        };

        if (current_field == VncData::SortByName
            // no files, directories only -> equivalent to sorting by name
         || (current_field == VncData::SortBySize && !gui.vnc.file_counter))
        {
            assert(gui.vnc.sorted.name.initialized);

            // sort by name + reversed
            auto reversed
                = (gui.vnc.sorted_field_with_option == (VncData::SortByName | VncData::SortReverse));

            auto arr = reversed ? gui.vnc.sorted.reversed_name : gui.vnc.sorted.name;
            // already init, no changes
            if (arr.initialized && gui.vnc.sorted.indices == arr.indices)
            {
                return false;
            }

            gui.vnc.sorted.indices = gui.vnc.sorted.name.indices;

            if (reversed && !init_sorted_array(gui, gui.vnc.sorted.reversed_name))
            {
                auto * in = gui.vnc.sorted.name.indices;
                auto * out = gui.vnc.sorted.reversed_name.indices;
                auto nb_files = gui.vnc.files.size();
                std::copy(in, in + nb_files, out);
                std::reverse(out, out + nb_files);
            }

            return true;
        }
        else if (current_field == VncData::SortBySize)
        {
            init_vnc_sorted_by_field<&FileData::file_size>(gui, gui.vnc.sorted.size, false);
            reverse_when_enabled(gui.vnc.sorted.size, gui.vnc.sorted.reversed_size, false);
        }
        else // if (current_field == VncData::SortByDate)
        {
            assert(current_field == VncData::SortByDate);
            init_vnc_sorted_by_field<&FileData::last_access_time>(gui, gui.vnc.sorted.date, true);
            reverse_when_enabled(gui.vnc.sorted.date, gui.vnc.sorted.reversed_date, true);
        }

        return true;
    }

    /// \return \c true when state changes, otherwise \c false.
    static bool set_disable_element_change(GuiData & gui, ElementId item, bool activated)
    {
        if (gui.disabled_elements.has(item) == activated)
        {
            gui.disabled_elements.toggle(item);
            return true;
        }
        return false;
    }

    static void update_to_vnc_copy_button(GuiData & gui)
    {
        bool update_to_vnc = set_disable_element_change(
            gui,
            ElementId::ToVncButton,
            // feature is enabled
            flags_any(gui.flags, Flags::CbToVnc)
            // no transfer
            && gui.disabled_elements.has(D::ElementId::StopTransferButton)
            // state ok
            && (gui.vnc.state == VncState::Ready || gui.vnc.state == VncState::Empty)
            && (gui.cb.display_state != CbState::PopulatedByServerLoading
             && gui.cb.display_state != CbState::PopulatedByServerReady)
            // not in directory -> disabled
            && !gui.vnc.directory.empty()
            // not file in cb -> disabled
            && gui.cb.nb_file
        );

        if (update_to_vnc && gui.is_open())
        {
            DrawCtx ctx{gui, gui.layout.mid.rect};
            ctx.draw_to_vnc_button();
        }
    }

    static void update_to_rdp_copy_button(GuiData & gui)
    {
        bool update_to_rdp = set_disable_element_change(
            gui,
            ElementId::ToRdpButton,
            // feature is enabled
            flags_any(gui.flags, Flags::VncToCb)
            // not selection -> disabled
            && gui.vnc.selection_counter
            // no transfer
            && gui.disabled_elements.has(D::ElementId::StopTransferButton)
            // state ok
            && (gui.cb.display_state == CbState::Empty
             || gui.cb.display_state == CbState::Ready
             || gui.cb.display_state == CbState::PopulatedByServerReady
             || gui.cb.display_state == CbState::PopulatedByServerLoading)
        );

        if (update_to_rdp && gui.is_open())
        {
            DrawCtx ctx{gui, gui.layout.mid.rect};
            ctx.draw_to_rdp_button();
        }
    }

    static void update_copy_buttons(GuiData & gui)
    {
        update_to_vnc_copy_button(gui);
        update_to_rdp_copy_button(gui);
    }

    static void draw_ui(
        GuiData & gui,
        Rect clip,
        ComputeMode mode,
        CbState cb_state
    )
    {
        auto & layout = gui.layout;

        auto const & rdp_pan = layout.cb_rdp.inner_rect;
        auto const & vnc_pan = layout.vnc.inner_rect;

        Rect frame_rect { 0, 0, layout.width, layout.height };

        clip = clip.intersect(frame_rect);

        DrawCtx ctx{gui, clip};

        using Color = Widget::Color;

        struct Frame
        {
            D::TextId::E text_id;
            Colors::Title title_colors;
            Color bg_color;
            Color border_color;
        };

        auto draw_title = [&](Rect title_rect, Colors::Title title_colors, D::TextId::E text_id) {
            uint16_t left_padding = D::TITLE_X_PADDING;

            if (text_id == D::TextId::window_title)
            {
                left_padding += gui.icons.title_icon->width;
                left_padding += gui.space_w;
            }

            // title
            auto fcs_and_width = D::fcs_and_width(gui, text_id);
            ctx.draw_text(
                title_rect.x,
                title_rect.y,
                fcs_and_width,
                title_colors,
                gdi::DrawTextPadding{
                    .top = D::TITLE_Y_PADDING,
                    .right = checked_int{
                        title_rect.cx - fcs_and_width.text_width - left_padding
                    },
                    .bottom = D::TITLE_Y_PADDING,
                    .left = left_padding,
                }
            );

            if (text_id == D::TextId::window_title)
            {
                auto * icon = gui.icons.title_icon;
                ctx.draw_text(
                    title_rect.x + D::TITLE_X_PADDING - icon->offsetx,
                    title_rect.y + (layout.title_h - icon->height) / 2 - icon->offsety,
                    D::FcsAndWidth{D::Fcs::assumed(&icon, 1u), checked_int{icon->boxed_width()}},
                    title_colors
                );
            }

            // title sep
            ctx.draw_rect(
                Rect{
                    title_rect.x,
                    checked_int{ title_rect.y + layout.title_h },
                    title_rect.cx,
                    D::TITLE_BOTTOM_SEPARATOR
                },
                title_colors.bottom_sep
            );
        };

        auto title_h_and_sep = layout.title_h + D::TITLE_BOTTOM_SEPARATOR;

        auto draw_frame = [&](Rect inner_rect, Frame frame) {
            Rect title_rect {
                inner_rect.x,
                checked_int{ inner_rect.y - title_h_and_sep },
                inner_rect.cx,
                layout.title_h,
            };

            draw_title(title_rect, frame.title_colors, frame.text_id);

            // frame border top
            Rect solid_rect {
                checked_int{ inner_rect.x - D::PAN_BORDER_LEN },
                checked_int{ title_rect.y - D::PAN_BORDER_LEN },
                checked_int{ inner_rect.cx + D::PAN_BORDER_LEN * 2 },
                D::PAN_BORDER_LEN
            };

            ctx.draw_rect(solid_rect, frame.border_color);

            // frame border left
            solid_rect.cx = D::PAN_BORDER_LEN;
            solid_rect.cy = checked_int{ inner_rect.cy + D::PAN_BORDER_LEN * 2 + title_h_and_sep };

            ctx.draw_rect(solid_rect, frame.border_color);

            // frame border right
            solid_rect.x += inner_rect.cx + D::PAN_BORDER_LEN;

            ctx.draw_rect(solid_rect, frame.border_color);

            // inner
            ctx.draw_rect(inner_rect, frame.bg_color);

            // frame border bottom
            ctx.draw_rect(
                Rect{
                    checked_int{ inner_rect.x - D::PAN_BORDER_LEN },
                    inner_rect.ebottom(),
                    checked_int{ inner_rect.cx + D::PAN_BORDER_LEN * 2 },
                    D::PAN_BORDER_LEN,
                },
                frame.border_color
            );
        };


        /*
         * Frames
         */

        draw_title(frame_rect, D::colors.window.title, D::TextId::window_title);
        // inner
        ctx.draw_rect(
            Rect{
                frame_rect.x,
                checked_int{ frame_rect.y + title_h_and_sep },
                frame_rect.cx,
                checked_int{ frame_rect.cy - title_h_and_sep },
            },
            D::colors.window.bg
        );

        draw_frame(rdp_pan, Frame {
            .text_id = D::TextId::cb_pan_name,
            .title_colors = D::colors.panel.title,
            .bg_color = D::colors.panel.bg,
            .border_color = D::colors.panel.border,
        });

        draw_frame(vnc_pan, Frame {
            .text_id = D::TextId::vnc_pan_name,
            .title_colors = D::colors.panel.title,
            .bg_color = D::colors.panel.bg,
            .border_color = D::colors.panel.border,
        });

        /*
         * Close button
         */

        if (ctx.clip.has_intersection(gui.layout.close_btn.rect))
        {
            D::draw_close_button(ctx.gui);
        }

        /*
         * Mid panel buttons
         */

        ctx.draw_to_vnc_button();
        ctx.draw_to_rdp_button();
        ctx.draw_stop_transfer_button();


        /*
         * Clipboard message part
         */

        draw_cb_part(gui, clip, cb_state, mode, ForceUpdate::Yes);


        /*
         * Progression message part
         */

        draw_progress_part(gui, clip);


        /*
         * Vnc location
         */

        ctx.draw_vnc_root_button();
        ctx.draw_vnc_parent_button();
        gui.vnc.directory_edit.rdp_input_invalidate(clip);


        /*
         * Vnc table headers
         */

        // all file icon
        ctx.draw_vnc_all_file_checkbox();

        // filename header
        ctx.draw_text(
            layout.vnc.header_filename_x,
            layout.vnc.header_text_y,
            TextId::vnc_header_filename,
            colors.list.header
        );
        draw_vnc_sorting_icon_filename(gui, clip);

        // size header
        ctx.draw_text(
            layout.vnc.header_size_x,
            layout.vnc.header_text_y,
            TextId::vnc_header_size,
            colors.list.header
        );
        draw_vnc_sorting_icon_size(gui, clip);

        // modification date header
        ctx.draw_text(
            layout.vnc.header_date_x,
            layout.vnc.header_text_y,
            TextId::vnc_header_modification_date,
            colors.list.header
        );
        draw_vnc_sorting_icon_date(gui, clip);


        /*
         * Draw header column borders
         */

        for (auto x : {
            layout.vnc.list_vline_icon_x,
            layout.vnc.list_vline_size_x,
            layout.vnc.list_vline_date_x
        })
        {
            Rect line_rect {
                x,
                layout.vnc.header_y,
                D::COLUMN_BORDER_LEN,
                layout.vnc.header_h,
            };
            ctx.draw_rect(line_rect, D::colors.list.column_sep);
        }


        /*
         * Vnc table headers/list separator
         */

        ctx.draw_rect(
            Rect{
                layout.vnc.inner_rect.x,
                checked_int{ layout.vnc.header_y + layout.vnc.header_h },
                layout.vnc.inner_rect.cx,
                D::COLUMN_BORDER_LEN,
            },
            colors.list.column_sep
        );

        /*
         * Vnc file list
         */

        if (clip.has_intersection(layout.vnc.inner_rect))
        {
            draw_vnc_part(gui, clip.intersect(layout.vnc.inner_rect), mode);
        }
    }


    /*
     * Actions
     */
    //@{

    enum class OpenDirErr : bool
    {
        NoError,
        FilePathTruncated,
    };

    static void act_open_dir(FileTransferGui & self, GuiData & gui, OpenDirErr err)
    {
        if (err == OpenDirErr::NoError
         && self.m_callbacks.open_dir(self.m_callbacks.ctx, gui.vnc.directory.name_av()))
        {
            update_vnc_start_event(gui, VncState::Loading);
        }
        else
        {
            update_vnc_start_event(gui, VncState::Error);
        }

        auto buffer = cp1252_to_utf32.buffer_from(gui.vnc.directory.name_av().native());
        gui.vnc.directory_edit.set_text(buffer.av(), { WidgetEdit::Redraw::Yes });
    }

    static void act_vnc_root_button(FileTransferGui & self, GuiData & gui)
    {
        gui.vnc.directory.buffer_len = 0;
        act_open_dir(self, gui, OpenDirErr::NoError);
    }

    static void act_vnc_parent_button(FileTransferGui & self, GuiData & gui, uint32_t step)
    {
        auto & dir = gui.vnc.directory;
        assert(!dir.empty());
        if (!dir.empty()) // defensive check, button must be not activable
        {
            while (step--)
            {
                chars_view path = dir.name_av().native().as_chars();

                // remove trailing dir sep ; absent when path too long
                if (path.back() == '\\')
                {
                    path = path.drop_back(1);
                }

                // keep last dir sep when present
                if (auto * p = strrchr(path, '\\'))
                {
                    dir.buffer_len = checked_int{ p - path.data() + 1 };
                }
                else
                {
                    dir.buffer_len = 0;
                    break;
                }
            }

            act_open_dir(self, gui, OpenDirErr::NoError);
        }
    }

    static void act_vnc_subdir(FileTransferGui & self, GuiData & gui, FileData const & file)
    {
        if (file.is_file())
        {
            return ;
        }

        auto subdir = file.file_name_av().native();

        auto & dir = gui.vnc.directory;

        /*
         * Concat current dir and subdir
         */

        WinNtDirSep dir_sep { dir.name_av().native(), subdir, WinNtDirSep::EndSep::Required };

        if (dir_sep.add_mid_sep)
        {
            dir.insert_win_sep();
        }

        auto uninit_buffer = make_writable_array_view(dir.buffer).drop_front(dir.buffer_len);
        auto src = subdir.first(mmin(uninit_buffer.size(), subdir.size()));
        dir.buffer_len += bytes_copy(uninit_buffer, src);

        if (dir_sep.add_end_sep)
        {
            dir.insert_win_sep();
        }

        /*
         * Request and redraw
         */

        auto err = dir_sep.is_truncated_path
            ? OpenDirErr::FilePathTruncated
            : OpenDirErr::NoError;
        act_open_dir(self, gui, err);
    }

    static void act_vnc_to_cb(FileTransferGui & self, GuiData const & gui)
    {
        auto d = static_cast<FileTransferGui::FileData const*>(gui.vnc.files.data());
        auto n = gui.vnc.files.size();

        assert(n);

        SelectedVncFiles::Iterator it{d, d + n};
        // first file is not selected, go to the next selected item
        if (!d->checked)
        {
            ++it;
        }

        assert(it != SelectedVncFiles::Sentinel{});

        self.m_callbacks.copy_vnc_to_cb(
            self.m_callbacks.ctx,
            SelectedVncFiles{it, gui.vnc.selection_counter}
        );
    }

    static void act_cb_to_vnc(FileTransferGui & self)
    {
        self.m_callbacks.copy_cb_to_vnc(self.m_callbacks.ctx);
    }

    static void act_stop_transfer(FileTransferGui & self)
    {
        self.m_callbacks.stop_transfer(self.m_callbacks.ctx);
    }

    //@}

    struct RangeIndices
    {
        MaxFileIntType page_index;
        MaxFileIntType begin_index;
        MaxFileIntType end_index;
    };

    /// \return difference of selection counter
    static MaxFileIntType checked_visible_range(
        GuiData & gui,
        RangeIndices range_indices,
        Rect vnc_list_rect,
        CheckboxAction act)
    {
        MaxFileIntType selection_counter = 0;

        Item item_colors { gui, range_indices.page_index };
        ItemIndices item_indices {
            .current_index = range_indices.begin_index,
            .selected_index = gui.vnc.selected_index,
        };

        Checkbox checkbox_checked { gui, true };
        Checkbox checkbox_unchecked { gui, false };
        int checkbox_y = checked_int {
            (range_indices.begin_index - range_indices.page_index) * gui.layout.vnc.list_item_h
        };
        checkbox_y += gui.layout.vnc.list_y + LINE_Y_PADDING;

        auto file_indices = gui.vnc.sorted_indices()
            .from_offset(range_indices.begin_index, range_indices.end_index);

        bool toggle_when_checked = (act == CheckboxAction::ToUnchecked);

        for (auto i : file_indices)
        {
            auto & file = gui.vnc.files[i];

            if (act == CheckboxAction::Toggle || toggle_when_checked == file.checked)
            {
                file.checked = !file.checked;
                selection_counter += D::as_counter(file.checked);
                auto & checkbox = file.checked ? checkbox_checked : checkbox_unchecked;
                auto colors = item_colors.current_colors(item_indices).to_text_colors();
                checkbox.draw(gui.gd, checkbox_y, colors, vnc_list_rect);
            }

            checkbox_y += gui.layout.vnc.list_item_h;
            ++item_indices.current_index;
        }

        return selection_counter;
    }

    /// \return difference of selection counter
    static MaxFileIntType checked_invisible_range(
        GuiData & gui,
        MaxFileIntType first, MaxFileIntType last,
        CheckboxAction act)
    {
        MaxFileIntType selection_counter = 0;

        bool toggle_when_checked = (act == CheckboxAction::ToUnchecked);

        if (first < last)
        {
            for (auto i : gui.vnc.sorted_indices().from_offset(first, last))
            {
                auto & file = gui.vnc.files[i];

                if (act == CheckboxAction::Toggle || toggle_when_checked == file.checked)
                {
                    file.checked = !file.checked;
                    selection_counter += D::as_counter(file.checked);
                }
            }
        }

        return selection_counter;
    }

    struct SelectionIndices
    {
        MaxFileIntType istart_range;
        MaxFileIntType iend_range;

        SelectionIndices(MaxFileIntType previous_selected_index, MaxFileIntType selected_index)
            : istart_range(selected_index)
            , iend_range(previous_selected_index)
        {
            if (istart_range > iend_range)
            {
                std::swap(istart_range, iend_range);
            }
            ++iend_range;
        }
    };

    static void select_range(
        GuiData & gui,
        MaxFileIntType previous_selected_index,
        MaxFileIntType selected_index,
        Rect vnc_list_rect,
        CheckboxAction act)
    {
        auto [istart_range, iend_range] = SelectionIndices { previous_selected_index, selected_index };

        gui.vnc.previous_selected_index = selected_index;

        auto [page_index, end_page_index] = PageIndices { gui };
        auto start_visible = mmax(istart_range, page_index);
        auto end_visible = mmin(iend_range, end_page_index + MaxFileIntType{1});

        MaxFileIntType selection_counter = 0;

        selection_counter += checked_invisible_range(gui, istart_range, page_index, act);
        selection_counter += checked_invisible_range(gui, end_visible, iend_range, act);
        selection_counter += checked_visible_range(
            gui,
            {
                .page_index = page_index,
                .begin_index = start_visible,
                .end_index = end_visible,
            },
            vnc_list_rect,
            act
        );

        update_vnc_selection(gui, selection_counter);
    }

    static MaxFileIntType as_counter(bool b) noexcept
    {
        return b ? MaxFileIntType{1} : MaxFileIntType{-1u};
    }

    // mouse / keyboard events
    //@{

    enum class InputEvent
    {
        KeyEvent,
        MouseEvent,
    };

    struct MouseEventData
    {
        uint16_t device_flags;
        uint16_t x;
        uint16_t y;
    };

    static void event_press_or_focus(
        FileTransferGui & self,
        GuiData & gui,
        InputEvent event,
        MouseEventData mouse_data,
        ElementId old_focus_item,
        ElementId elem,
        Rect vnc_list_rect,
        kbdtypes::KeyModFlags mods)
    {
        auto last_pressed_index = gui.vnc.reset_last_pressed_index();

        switch (elem)
        {
            case ElementId::None:
                break;

            case ElementId::GuiClose:
                // unfocusable element
                gui.focus_item = ElementId::None;
                draw_close_button(gui);
                break;

            case ElementId::VncRootButton:
                DrawCtx{gui, gui.layout.vnc.inner_rect}.draw_vnc_root_button();
                break;

            case ElementId::VncParentButton:
                DrawCtx{gui, gui.layout.vnc.inner_rect}.draw_vnc_parent_button();
                break;

            case ElementId::VncListAllCheckbox:
                if (old_focus_item != elem)
                {
                    DrawCtx{gui, gui.layout.vnc.inner_rect}.draw_vnc_all_file_checkbox();
                }
                break;

            case ElementId::ToRdpButton:
                DrawCtx{gui, gui.layout.mid.rect}.draw_to_rdp_button();
                break;

            case ElementId::ToVncButton:
                DrawCtx{gui, gui.layout.mid.rect}.draw_to_vnc_button();
                break;

            case ElementId::StopTransferButton:
                DrawCtx{gui, gui.layout.mid.rect}.draw_stop_transfer_button();
                break;

            case ElementId::VncEditField:
                switch (event)
                {
                    case InputEvent::MouseEvent:
                        gui.vnc.directory_edit.rdp_input_mouse(
                            mouse_data.device_flags, mouse_data.x, mouse_data.y
                        );
                        if (!gui.vnc.directory_edit.has_focus)
                        {
                            gui.vnc.directory_edit.focus();
                        }
                        break;

                    case InputEvent::KeyEvent:
                        gui.vnc.directory_edit.focus();
                        break;
                }
                break;

            case ElementId::VncNavigation:
                switch (event)
                {
                    case InputEvent::MouseEvent:
                        gui.vnc.pagination.rdp_input_mouse(
                            mouse_data.device_flags, mouse_data.x, mouse_data.y
                        );
                        break;

                    case InputEvent::KeyEvent:
                        gui.vnc.pagination.focus();
                        break;
                }
                break;

            case ElementId::VncIconSortFilename:
                draw_vnc_sorting_icon_filename(gui, D::OptionalClip{});
                break;

            case ElementId::VncIconSortSize:
                draw_vnc_sorting_icon_size(gui, D::OptionalClip{});
                break;

            case ElementId::VncIconSortDate:
                draw_vnc_sorting_icon_date(gui, D::OptionalClip{});
                break;

            case ElementId::VncList: {
                if (gui.vnc.files.empty())
                {
                    break;
                }

                if (event == InputEvent::KeyEvent)
                {
                    D::draw_vnc_list_one_item_with_borders(
                        gui,
                        vnc_list_rect,
                        gui.vnc.sorted_file(gui.vnc.selected_index),
                        D::y_item_from_selected_index(gui),
                        D::colors.list.focus
                    );

                    break;
                }

                assert(event == InputEvent::MouseEvent);

                auto offset_item = D::offset_item_from_y(gui, mouse_data.y);
                auto y_item = D::y_item_from_offset(gui, offset_item);
                auto page_file = D::PageFile { gui };
                auto pressed_index = page_file.page_index + offset_item;
                auto & file = gui.vnc.sorted_file(pressed_index);

                LOG(LOG_DEBUG, "pressed_index=%u | selected_index=%u | last_pressed_index=%u", pressed_index, gui.vnc.selected_index, last_pressed_index);

                gui.vnc.last_pressed_index = pressed_index;

                auto draw_changed_focus = [&]{
                    auto should_redraw_line
                        = pressed_index != gui.vnc.selected_index
                       || old_focus_item != ElementId::VncList;

                    LOG(LOG_DEBUG, "should_redraw_line: %d", should_redraw_line);

                    if (!should_redraw_line)
                    {
                        return ;
                    }

                    auto offset_delta = pressed_index - gui.vnc.selected_index;
                    auto blur_index = gui.vnc.selected_index;
                    gui.vnc.selected_index = pressed_index;

                    D::Item item_colors { gui, page_file.page_index };

                    // focus
                    D::draw_vnc_list_one_item_with_borders(
                        gui,
                        vnc_list_rect,
                        file,
                        y_item,
                        D::colors.list.focus
                    );

                    // blur
                    auto blur_y_item = D::y_item_from_offset(gui, offset_item - offset_delta);
                    if (blur_y_item != y_item)
                    {
                        D::draw_vnc_list_one_item_with_borders(
                            gui,
                            vnc_list_rect,
                            gui.vnc.sorted_file(blur_index),
                            D::y_item_from_offset(gui, offset_item - offset_delta),
                            item_colors.unfocused_colors(blur_index)
                        );
                    }
                };

                // on checkbox
                if (D::Checkbox{ gui, file.checked }
                    .boxed_click_from_y_item(gui, y_item)
                    .contains_pt(checked_int{mouse_data.x}, checked_int{mouse_data.y}))
                {
                    draw_changed_focus();
                }
                // db click
                else if (pressed_index == gui.vnc.selected_index
                      && pressed_index == last_pressed_index
                      && !mods.has_mods())
                {
                    auto current_time = self.m_event_guard.get_monotonic_time();
                    // double click triggered
                    if (current_time <= gui.last_click_time + D::delay_before_db_click)
                    {
                        LOG(LOG_DEBUG, "idx=%ld", &file - gui.vnc.files.data());
                        LOG(LOG_DEBUG, "file type=%d", file.file_type);
                        LOG(LOG_DEBUG, "file[0]: %.*s (%zu)",
                            static_cast<int>(file.file_name_av().native().size()),
                            file.file_name_av().native().data(),
                            file.file_name_av().native().size());
                        D::act_vnc_subdir(self, gui, file);
                    }
                    // too long delay
                    else
                    {
                        gui.last_click_time = current_time;
                    }
                }
                // single click
                else
                {
                    gui.last_click_time = mods.has_mods()
                        ? MonotonicTimePoint{}
                        : self.m_event_guard.get_monotonic_time();
                    draw_changed_focus();
                }
                break;
            }
        }
    }

    static void event_mouse_release(
        FileTransferGui & self,
        GuiData & gui,
        MouseEventData mouse_data,
        ElementId pressed_item,
        ElementId elem,
        Rect vnc_list_rect,
        kbdtypes::KeyModFlags mods)
    {
        LOG(LOG_DEBUG, "up | %d", !gui.disabled_elements.has(pressed_item));

        if (gui.disabled_elements.has(pressed_item))
        {
            return ;
        }

        switch (pressed_item)
        {
            case ElementId::None:
                break;

            case ElementId::GuiClose:
                D::draw_close_button(gui);
                if (pressed_item == elem)
                {
                    // TODO confirm when transfer
                    self.m_callbacks.close_gui(self.m_callbacks.ctx);
                }
                break;

            case ElementId::VncRootButton:
                if (pressed_item == elem)
                {
                    D::DrawCtx{gui, gui.layout.vnc.inner_rect}.draw_vnc_root_button();
                    D::act_vnc_root_button(self, gui);
                }
                break;

            case ElementId::VncParentButton:
                if (pressed_item == elem)
                {
                    D::DrawCtx{gui, gui.layout.vnc.inner_rect}.draw_vnc_parent_button();
                    D::act_vnc_parent_button(self, gui, 1);
                }
                break;

            case ElementId::VncEditField:
                if (pressed_item == elem)
                {
                    gui.vnc.directory_edit.rdp_input_mouse(
                        mouse_data.device_flags, mouse_data.x, mouse_data.y
                    );
                }
                break;

            case ElementId::VncIconSortFilename:
                D::activate_sorting_icon_and_draw(
                    gui, pressed_item == elem, D::VncData::SortByName);
                break;

            case ElementId::VncIconSortSize:
                D::activate_sorting_icon_and_draw(
                    gui, pressed_item == elem, D::VncData::SortBySize);
                break;

            case ElementId::VncIconSortDate:
                D::activate_sorting_icon_and_draw(
                    gui, pressed_item == elem, D::VncData::SortByDate);
                break;

            case ElementId::VncListAllCheckbox:
                if (pressed_item == elem)
                {
                    bool toggle = mods.rmod_as_lmod().test(kbdtypes::KeyMod::LCtrl);
                    auto act = toggle
                        ? CheckboxAction::Toggle
                        : gui.vnc.all_file_checked
                        ? CheckboxAction::ToUnchecked
                        : CheckboxAction::ToChecked;
                    event_all_checkbox(gui, vnc_list_rect, act);
                }
                break;

            case ElementId::VncList: {
                if (gui.vnc.files.empty())
                {
                    break;
                }

                if (pressed_item != elem)
                {
                    break;
                }

                auto [file_indices, page_index] = PageFile{ gui };

                MaxFileIntType offset = offset_item_from_y(gui, mouse_data.y);

                auto selected_index = page_index + offset;

                if (selected_index != gui.vnc.last_pressed_index)
                {
                    gui.vnc.reset_last_pressed_index();
                    break;
                }

                FileData & file = gui.vnc.sorted_file(selected_index);

                //              |       ctrl       |    ctrl+shift    |   shift
                // ---------------------------------------------------------------------
                // has previous |     toggle +     |   toggle range   | activate range
                // selection    | update selection |                  |
                // ---------------------------------------------------------------------
                // without      |     toggle +     |  toggle range +  |    activate +
                // selection    | update selection | update selection | update selection
                // ---------------------------------------------------------------------

                auto ctrl = mods.has_ctrl();
                auto shift = mods.has_shift();

                int y_item = D::y_item_from_offset(gui, offset);

                auto is_on_checkbox = [&]{
                    return Checkbox{ gui, file.checked }
                        .boxed_click_from_y_item(gui, y_item)
                        .contains_pt(checked_int{mouse_data.x}, checked_int{mouse_data.y});
                };

                // toggle or activate one item
                if (!shift && (ctrl || is_on_checkbox()))
                {
                    file.checked = !file.checked;
                    int icon_y = y_item + D::LINE_Y_PADDING;
                    auto text_colors = D::colors.list.focus.to_text_colors();
                    D::Checkbox checkbox{ gui, file.checked };
                    checkbox.draw(self.m_gd, icon_y, text_colors, vnc_list_rect);

                    gui.vnc.previous_selected_index = selected_index;
                    D::update_vnc_selection(gui, D::as_counter(file.checked));
                }
                // range (activate or toggle)
                else if (shift)
                {
                    auto old_selected_index
                        = (gui.vnc.previous_selected_index == D::INVALID_INDEX)
                        ? page_index
                        : gui.vnc.previous_selected_index;
                    auto act = ctrl ? CheckboxAction::Toggle : CheckboxAction::ToChecked;
                    D::select_range(gui, old_selected_index, selected_index, vnc_list_rect, act);
                }
                else
                {
                    gui.vnc.previous_selected_index = selected_index;
                }

                break;
            }

            case ElementId::VncNavigation:
                gui.vnc.pagination.rdp_input_mouse(
                    mouse_data.device_flags, mouse_data.x, mouse_data.y
                );
                break;

            case ElementId::ToRdpButton:
                if (pressed_item == elem)
                {
                    D::act_vnc_to_cb(self, gui);
                    D::DrawCtx{gui, gui.layout.mid.rect}.draw_to_rdp_button();
                }
                break;

            case ElementId::ToVncButton:
                if (pressed_item == elem)
                {
                    D::act_cb_to_vnc(self);
                    D::DrawCtx{gui, gui.layout.mid.rect}.draw_to_vnc_button();
                }
                break;

            case ElementId::StopTransferButton:
                if (pressed_item == elem)
                {
                    D::act_stop_transfer(self);
                }
                break;
        }
    }

    static void event_all_checkbox(
        GuiData & gui,
        Rect vnc_list_rect,
        CheckboxAction act)
    {
        auto [page_index, end_page_index] = PageIndices { gui };
        auto after_page_index = end_page_index + MaxFileIntType{1};
        auto end_index = checked_int{ gui.vnc.files.size() };

        MaxFileIntType selection_counter = 0;

        selection_counter += checked_invisible_range(gui, 0, page_index, act);
        selection_counter += checked_invisible_range(gui, after_page_index, end_index, act);
        selection_counter += checked_visible_range(
            gui,
            {
                .page_index = page_index,
                .begin_index = page_index,
                .end_index = after_page_index,
            },
            vnc_list_rect,
            act
        );

        update_vnc_selection(gui, selection_counter);
    }

    static void event_blur(GuiData & gui, ElementId old_focus_item)
    {
        switch (old_focus_item)
        {
            case ElementId::None:
                break;

            case ElementId::GuiClose:
                D::draw_close_button(gui);
                break;

            case ElementId::VncRootButton:
                D::DrawCtx{gui, gui.layout.vnc.inner_rect}.draw_vnc_root_button();
                break;

            case ElementId::VncParentButton:
                D::DrawCtx{gui, gui.layout.vnc.inner_rect}.draw_vnc_parent_button();
                break;

            case ElementId::VncListAllCheckbox:
                D::DrawCtx{gui, gui.layout.vnc.inner_rect}.draw_vnc_all_file_checkbox();
                break;

            case ElementId::ToRdpButton:
                D::DrawCtx{gui, gui.layout.mid.rect}.draw_to_rdp_button();
                break;

            case ElementId::ToVncButton:
                D::DrawCtx{gui, gui.layout.mid.rect}.draw_to_vnc_button();
                break;

            case ElementId::StopTransferButton:
                D::DrawCtx{gui, gui.layout.mid.rect}.draw_stop_transfer_button();
                break;

            case ElementId::VncEditField:
                gui.vnc.directory_edit.blur();
                break;

            case ElementId::VncNavigation:
                gui.vnc.pagination.blur();
                break;

            case ElementId::VncIconSortFilename:
                D::draw_vnc_sorting_icon_filename(gui, D::OptionalClip{});
                break;

            case ElementId::VncIconSortSize:
                D::draw_vnc_sorting_icon_size(gui, D::OptionalClip{});
                break;

            case ElementId::VncIconSortDate:
                D::draw_vnc_sorting_icon_date(gui, D::OptionalClip{});
                break;

            case ElementId::VncList: {
                if (gui.vnc.files.empty())
                {
                    break;
                }

                // TODO duplicate -> draw_vnc_list_selected_item()
                auto page_index = get_current_page_index(gui);
                draw_vnc_list_one_item_with_borders(
                    gui,
                    get_vnc_list_rect(gui),
                    gui.vnc.sorted_file(gui.vnc.selected_index),
                    checked_int{ y_item_from_selected_index(gui) },
                    Item{gui, page_index}.focused_colors()
                );
                break;
            }
        }
    }

    enum SelectorProcessEvent
    {
        Nothing,
        // blur element
        BlurOnly,
        // blur element and focus to vnc list element
        FocusAndBlur,
    };

    static void selector_process_pressed(
        FileTransferGui & self, GuiData & gui, uint32_t uc, kbdtypes::KeyModFlags mods)
    {
        auto event = selector_process_pressed_impl(self, gui, uc, mods);
        switch (event)
        {
            case SelectorProcessEvent::Nothing:
                break;

            case SelectorProcessEvent::BlurOnly:
                // focus to vnc list
                if (gui.focus_item != ElementId::VncList)
                {
                    auto old_focus = gui.focus_item;
                    gui.focus_item = ElementId::VncList;
                    gui.pressed_item = ElementId::None;
                    event_blur(gui, old_focus);
                }
                break;

            case SelectorProcessEvent::FocusAndBlur:
                // focus to vnc list
                if (gui.focus_item != ElementId::VncList)
                {
                    auto old_focus = gui.focus_item;
                    gui.focus_item = ElementId::VncList;
                    gui.pressed_item = ElementId::None;
                    draw_vnc_list_one_item_with_borders(
                        gui,
                        get_vnc_list_rect(gui),
                        gui.vnc.sorted_file(gui.vnc.selected_index),
                        y_item_from_selected_index(gui),
                        colors.list.focus
                    );
                    event_blur(gui, old_focus);
                }
                break;
        }
    }

    static const uint32_t key_to_root = '\\';
    static const uint32_t key_to_parent = '\b';

    static uint32_t kbd_to_vim_shortcut(
        Keymap const & keymap,
        OutParam<VimNumberState> vim_number_state)
    {
        REDEMPTION_DIAGNOSTIC_PUSH()
        REDEMPTION_DIAGNOSTIC_GCC_IGNORE("-Wswitch")
        switch (keymap.last_kevent())
        {
            case Keymap::KEvent::Enter: return '\r';
            case Keymap::KEvent::Backspace: return key_to_parent;
            case Keymap::KEvent::UpArrow: return 'k';
            case Keymap::KEvent::DownArrow: return 'j';
            case Keymap::KEvent::LeftArrow: return 'h';
            case Keymap::KEvent::RightArrow: return 'l';

            case Keymap::KEvent::PgUp:
                vim_number_state.out_value = VimNumberState::Begin;
                return 'k';

            case Keymap::KEvent::PgDown:
                vim_number_state.out_value = VimNumberState::End;
                return 'j';

            case Keymap::KEvent::Home:
                vim_number_state.out_value = VimNumberState::Begin;
                return 'g';

            case Keymap::KEvent::End:
                vim_number_state.out_value = VimNumberState::End;
                return 'g';

            case Keymap::KEvent::KeyDown: {
                auto uc_a2 = keymap.last_decoded_keys().uchars;
                // dead key support `^ + ^ = ^^` / `^ + a = ^a`
                return uc_a2.back() ? uc_a2.back() : uc_a2.front();
            }
        }
        REDEMPTION_DIAGNOSTIC_POP()

        return 0;
    }

    struct VimAction
    {
        CheckboxAction act;
        bool checkable = true;

        VimAction(GuiData const & gui, VimModeData::Nav vim_nav, CheckboxAction default_act) noexcept
            : act{ default_act }
        {
            switch (vim_nav.checkbox_action)
            {
                case VimCheckboxAction::Unspecified:
                    switch (default_act)
                    {
                        case CheckboxAction::Toggle:
                            return;

                        case CheckboxAction::ToChecked:
                            checkable = !gui.vnc.all_file_checked;
                            return;

                        case CheckboxAction::ToUnchecked:
                            checkable = gui.vnc.selection_counter;
                            return;
                    }
                    REDEMPTION_UNREACHABLE();

                case VimCheckboxAction::Toggle:
                    act = CheckboxAction::Toggle;
                    return;

                case VimCheckboxAction::ToChecked:
                    act = CheckboxAction::ToChecked;
                    checkable = !gui.vnc.all_file_checked;
                    return;

                case VimCheckboxAction::ToUnchecked:
                    act = CheckboxAction::ToUnchecked;
                    checkable = gui.vnc.selection_counter;
                    return;
            }

            REDEMPTION_UNREACHABLE();
        }

        explicit operator bool () const noexcept
        {
            return checkable;
        }
    };

    static SelectorProcessEvent selector_process_pressed_impl(
        FileTransferGui & self, GuiData & gui, uint32_t uc, kbdtypes::KeyModFlags mods)
    {
        LOG(LOG_DEBUG, "uc = %u | %c", uc, ((uc & 0xff) > 0) ? static_cast<char>(uc) : ' ');

        if (gui.vnc.files.empty())
        {
            if (uc != key_to_parent && uc != key_to_root)
            {
                return SelectorProcessEvent::Nothing;
            }
        }

        auto [page_index, end_page_index] = PageIndices{gui};

        auto new_selected_index = gui.vnc.selected_index;
        auto new_widget_page = gui.vnc.pagination.current_page();
        auto & vim_mode = gui.vnc.vim_mode;
        auto vim_nav = vim_mode.nav;

        switch (uc)
        {
            /*
             * Vim number
             */

            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                switch (vim_mode.nav.number_state)
                {
                    case VimNumberState::Empty:
                    case VimNumberState::Begin:
                    case VimNumberState::End:
                        vim_mode.number = 0;
                        if (uc == '0')
                        {
                            vim_mode.nav.number_state = VimNumberState::Empty;
                            return SelectorProcessEvent::FocusAndBlur;
                        }
                        vim_mode.nav.number_state = VimNumberState::Stacked;
                        break;
                    case VimNumberState::Stacked:
                        break;
                }
                gui.vnc.vim_mode.number *= 10;
                gui.vnc.vim_mode.number += uc - '0';
                return SelectorProcessEvent::Nothing;

            case '^':
            case '[':
                vim_mode.nav.number_state = VimNumberState::Begin;
                return SelectorProcessEvent::FocusAndBlur;

            case '$':
            case ']':
                vim_mode.nav.number_state = VimNumberState::End;
                return SelectorProcessEvent::FocusAndBlur;

            /*
             * Vim clear
             */

            case 'c':
            case 'C':
                vim_mode.reset_all();
                return SelectorProcessEvent::Nothing;

            /*
             * Vim checkbox action
             */

            case 'i':
                vim_mode.nav.checkbox_action = VimCheckboxAction::Toggle;
                return SelectorProcessEvent::FocusAndBlur;

            case 'y':
                vim_mode.nav.checkbox_action = VimCheckboxAction::ToChecked;
                return SelectorProcessEvent::FocusAndBlur;

            case 'u':
                vim_mode.nav.checkbox_action = VimCheckboxAction::ToUnchecked;
                return SelectorProcessEvent::FocusAndBlur;

            /*
             * Directory buttons
             */

            case key_to_root:
                if (!gui.disabled_elements.has(GuiData::ElementId::VncRootButton))
                {
                    D::act_vnc_root_button(self, gui);
                }
                return SelectorProcessEvent::Nothing;

            case key_to_parent:
                if (!gui.disabled_elements.has(GuiData::ElementId::VncParentButton))
                {
                    vim_mode.reset_move();
                    switch (vim_mode.nav.number_state)
                    {
                        case VimNumberState::End:
                            break;

                        case VimNumberState::Begin:
                            D::act_vnc_root_button(self, gui);
                            break;

                        case VimNumberState::Empty:
                            D::act_vnc_parent_button(self, gui, 1);
                            break;

                        case VimNumberState::Stacked:
                            D::act_vnc_parent_button(self, gui, vim_mode.number);
                            break;
                    }
                }
                return SelectorProcessEvent::Nothing;

            /*
             * Directory item
             */

            case '\r':
                if (gui.focus_item == ElementId::VncList)
                {
                    auto & file = gui.vnc.sorted_file(gui.vnc.selected_index);
                    act_vnc_subdir(self, gui, file);
                }
                return SelectorProcessEvent::Nothing;

            /*
             * Item navigation
             */

            // up
            case 'k':
            case 'K':
                vim_mode.reset_move();
                switch (vim_nav.number_state)
                {
                    case VimNumberState::Begin:
                    case VimNumberState::End:
                        new_selected_index = page_index;
                        break;

                    case VimNumberState::Empty:
                        new_selected_index -= (gui.vnc.selected_index != page_index);
                        break;

                    case VimNumberState::Stacked:
                        new_selected_index -= mmin(
                            gui.vnc.selected_index - page_index,
                            gui.vnc.vim_mode.number
                        );
                        break;
                }
                break;

            // down
            case 'j':
            case 'J':
                vim_mode.reset_move();
                switch (vim_nav.number_state)
                {
                    case VimNumberState::Begin:
                    case VimNumberState::End:
                        new_selected_index = end_page_index;
                        break;

                    case VimNumberState::Empty:
                        new_selected_index += (end_page_index != gui.vnc.selected_index);
                        break;

                    case VimNumberState::Stacked:
                        new_selected_index += mmin(
                            end_page_index - gui.vnc.selected_index,
                            gui.vnc.vim_mode.number
                        );
                        break;
                }
                break;

            // left
            case 'h':
            case 'H': {
                vim_mode.reset_move();
                switch (vim_nav.number_state)
                {
                    case VimNumberState::Begin:
                    case VimNumberState::End:
                        new_widget_page = 1;
                        break;

                    case VimNumberState::Empty:
                        --new_widget_page;
                        break;

                    case VimNumberState::Stacked:
                        new_widget_page -= vim_mode.number;
                        break;
                }
                break;
            }

            // right
            case 'l':
            case 'L': {
                vim_mode.reset_move();
                switch (vim_nav.number_state)
                {
                    case VimNumberState::Begin:
                    case VimNumberState::End:
                        new_widget_page = gui.vnc.pagination.total_page();
                        break;

                    case VimNumberState::Empty:
                        ++new_widget_page;
                        break;

                    case VimNumberState::Stacked: {
                        new_widget_page += vim_mode.number;
                        break;
                    }
                }
                break;
            }

            // goto
            case 'g':
            case 'G': {
                vim_mode.reset_move();
                switch (vim_nav.number_state)
                {
                    case VimNumberState::Begin:
                        new_widget_page = 1;
                        break;

                    case VimNumberState::End:
                        new_widget_page = gui.vnc.pagination.total_page();
                        break;

                    case VimNumberState::Empty:
                        return SelectorProcessEvent::FocusAndBlur;

                    case VimNumberState::Stacked:
                        new_widget_page = gui.vnc.vim_mode.number;
                        break;
                }
                break;
            }

            // marker selection (for ' ')
            case 'm': {
                vim_mode.reset_move();
                vim_mode.marker_index = new_selected_index;
                return SelectorProcessEvent::FocusAndBlur;
            }

            // select many
            case ' ': {
                vim_mode.reset_move();

                if (gui.focus_item != ElementId::VncList)
                {
                    return SelectorProcessEvent::Nothing;
                }

                // single selection
                if (vim_mode.marker_index == D::INVALID_INDEX)
                {
                    FileData & file = gui.vnc.sorted_file(new_selected_index);
                    file.checked = !file.checked;
                    int icon_y = D::y_item_from_selected_index(gui) + D::LINE_Y_PADDING;
                    auto text_colors = D::colors.list.focus.to_text_colors();
                    D::Checkbox checkbox{ gui, file.checked };
                    checkbox.draw(gui.gd, icon_y, text_colors, D::get_vnc_list_rect(gui));

                    update_vnc_selection(gui, D::as_counter(file.checked));
                }
                // multiple selection
                else
                {
                    auto default_act = mods.has_ctrl()
                        ? CheckboxAction::Toggle
                        : CheckboxAction::ToChecked;

                    if (VimAction vim_act { gui, vim_nav, default_act })
                    {
                        auto old_selected_index = vim_mode.marker_index;
                        vim_mode.marker_index = D::INVALID_INDEX;
                        select_range(
                            gui, old_selected_index, new_selected_index,
                            D::get_vnc_list_rect(gui), vim_act.act
                        );
                    }

                    vim_mode.reset_marker();
                }

                return SelectorProcessEvent::Nothing;
            }

            // select all
            case 'a':
            case 'A': {
                vim_mode.reset_move();

                auto default_act = gui.vnc.all_file_checked
                    ? CheckboxAction::ToUnchecked
                    : CheckboxAction::ToChecked;
                VimAction vim_act { gui, vim_nav, default_act };

                auto old_focus = gui.focus_item;
                auto new_focus = (vim_act.act == CheckboxAction::ToChecked)
                    ? ElementId::ToRdpButton
                    : ElementId::VncList;

                gui.focus_item = new_focus;
                gui.pressed_item = ElementId::None;

                if (vim_act.checkable)
                {
                    event_all_checkbox(gui, get_vnc_list_rect(gui), vim_act.act);
                }

                if (new_focus == ElementId::ToRdpButton)
                {
                    DrawCtx{gui, gui.layout.mid.rect}.draw_to_rdp_button();
                }

                if (old_focus != new_focus)
                {
                    if (new_focus == ElementId::VncList)
                    {
                        D::draw_vnc_list_one_item_with_borders(
                            gui,
                            get_vnc_list_rect(gui),
                            gui.vnc.sorted_file(gui.vnc.selected_index),
                            D::y_item_from_selected_index(gui),
                            D::colors.list.focus
                        );
                    }

                    event_blur(gui, old_focus);
                }

                return SelectorProcessEvent::Nothing;
            }

            // select page
            case 'p':
            case 'P': {
                vim_mode.reset_move();

                if (VimAction vim_act { gui, vim_nav, CheckboxAction::ToChecked })
                {
                    LOG(LOG_DEBUG, "%d %d", gui.focus_item, ElementId::VncList);
                    auto selection_counter = D::checked_visible_range(
                        gui,
                        {
                            .page_index = page_index,
                            .begin_index = page_index,
                            .end_index = end_page_index + 1,
                        },
                        D::get_vnc_list_rect(gui),
                        vim_act.act
                    );

                    update_vnc_selection(gui, selection_counter);
                }

                return SelectorProcessEvent::Nothing;
            }

            // sorting
            case 's':
            case 'S': {
                vim_mode.reset_move();

                using Field = VncData::SortedField;

                auto field = Field::SortByName;

                switch (vim_nav.number_state)
                {
                    case VimNumberState::Begin:
                    case VimNumberState::Empty:
                        break;

                    case VimNumberState::End:
                        field = Field::SortByDate;
                        break;

                    case VimNumberState::Stacked:
                        switch (vim_mode.number)
                        {
                            case 0:
                            case 1: field = Field::SortByName; break;
                            case 2: field = Field::SortBySize; break;
                            case 3: field = Field::SortByDate; break;

                            default:
                                return SelectorProcessEvent::Nothing;
                        }
                }

                if (vim_nav.checkbox_action == VimCheckboxAction::ToUnchecked
                 || mods.has_shift())
                {
                    field = Field(field | Field::SortReverse);
                }

                D::activate_sorting_icon_and_draw(gui, true, field);

                return SelectorProcessEvent::Nothing;
            }

            default:
                return SelectorProcessEvent::Nothing;
        }

        /*
         * Check new page and update new_selected_index
         */

        auto has_new_page = (new_widget_page != gui.vnc.pagination.current_page());

        if (has_new_page)
        {
            if (!gui.vnc.pagination.is_new_page(new_widget_page))
            {
                return SelectorProcessEvent::FocusAndBlur;
            }

            new_selected_index = D::widget_page_to_page_index(gui, new_widget_page);
        }

        /*
         * Update selected line
         */

        LOG(LOG_DEBUG, "idx: %u -> %u / %zu", gui.vnc.selected_index, new_selected_index, gui.vnc.files.size());
        LOG(LOG_DEBUG, "page: %u -> %u / %u", gui.vnc.pagination.current_page(), new_widget_page, gui.vnc.pagination.total_page());

        auto old_selected_index = gui.vnc.selected_index;

        if (new_selected_index == gui.vnc.selected_index)
        {
            return SelectorProcessEvent::FocusAndBlur;
        }

        using UpdateEvent = WidgetPagination::TriggerUpdatePageEvent;

        auto current_focus = gui.focus_item;
        gui.focus_item = D::ElementId::VncList;

        if (bool(vim_nav.checkbox_action) || mods.has_shift())
        {
            auto default_act = mods.has_ctrl()
                ? CheckboxAction::Toggle
                : CheckboxAction::ToChecked;

            if (VimAction vim_act { gui, vim_nav, default_act })
            {
                auto start_range_index = old_selected_index;

                if (mods.has_shift())
                {
                    auto sliding_direction = (start_range_index < new_selected_index)
                        ? D::VimSlidingDirection::ToDown
                        : D::VimSlidingDirection::ToUp;

                    /*
                     * Ignore previous item when sliding
                     */
                    if (vim_nav.sliding_direction != D::VimSlidingDirection::None
                     && sliding_direction == vim_nav.sliding_direction)
                    {
                        if (sliding_direction == D::VimSlidingDirection::ToDown)
                        {
                            ++start_range_index;
                        }
                        else
                        {
                            --start_range_index;
                        }
                    }

                    vim_mode.nav.sliding_direction = sliding_direction;
                }

                if (!has_new_page)
                {
                    select_range(
                        gui,
                        start_range_index,
                        new_selected_index,
                        D::get_vnc_list_rect(gui),
                        vim_act.act
                    );
                }
                else
                {
                    has_new_page = false;
                    gui.vnc.previous_selected_index = new_selected_index;
                    SelectionIndices selection {
                        old_selected_index,
                        new_selected_index,
                    };
                    auto selection_counter = checked_invisible_range(
                        gui,
                        selection.istart_range,
                        selection.iend_range,
                        vim_act.act
                    );
                    update_vnc_selection(gui, selection_counter);
                    D::set_widget_page(gui, new_widget_page, new_selected_index);
                    gui.vnc.pagination.set_page(new_widget_page, UpdateEvent::No);
                }
            }
        }

        if (has_new_page)
        {
            LOG(LOG_DEBUG, "go to page");
            gui.vnc.pagination.set_page(new_widget_page, UpdateEvent::Yes);
        }
        else
        {
            auto vnc_list_rect = get_vnc_list_rect(gui);
            auto & file = gui.vnc.sorted_file(new_selected_index);

            Item item_colors { gui, page_index };

            // focus
            draw_vnc_list_one_item_with_borders(
                gui,
                vnc_list_rect,
                file,
                y_item_from_offset(gui, new_selected_index - page_index),
                colors.list.focus
            );

            // blur
            draw_vnc_list_one_item_with_borders(
                gui,
                vnc_list_rect,
                gui.vnc.sorted_file(old_selected_index),
                y_item_from_offset(gui, old_selected_index - page_index),
                item_colors.unfocused_colors(old_selected_index)
            );
        }

        gui.vnc.selected_index = new_selected_index;
        gui.focus_item = current_focus;

        return SelectorProcessEvent::BlurOnly;
    }

    static void focus_by_tab_key(
        FileTransferGui & self,
        GuiData & gui,
        VimModeData::Nav vim_nav,
        Keymap::KeyModFlags mods)
    {
        auto to_m = [](ElementId elem, unsigned n = 1){
            return n << underlying_cast(elem);
        };

        auto n_lo_bits = [](uint32_t n){
            return (1u << n) - 1u;
        };

        auto select_next = [](uint32_t elems) -> uint32_t {
            return checked_int{ std::countr_zero(elems) };
        };

        auto select_prev = [](uint32_t elems) -> uint32_t {
            return checked_int{ 32 - std::countl_zero(elems) };
        };

        constexpr auto mask_nav = to_m(ElementId::VncNavigation);
        constexpr auto mask_before_nav = mask_nav - 1u;
        constexpr auto mask_after_nav = (to_m(ElementId::GuiClose) - 1u)
                                    & ~(mask_nav | mask_before_nav);

        constexpr auto nb_extra_nav_buttons = 4u;
        constexpr auto mask_all_nav = to_m(ElementId::VncNavigation, 0b11111u);
        constexpr auto last_adjusted_elem = underlying_cast(ElementId::GuiClose)
                                          + nb_extra_nav_buttons;

        uint32_t elems = ~gui.disabled_elements.raw_mask();

        // insert navigation buttons
        // [...items, nav, ...items] -> [...items, nav_first ... nav_last, ...items]
        elems
            = (elems & mask_before_nav)
            | ((elems & mask_nav) ? mask_all_nav : 0u)
            | ((elems & mask_after_nav) << nb_extra_nav_buttons)
        ;

        elems &= (1u << last_adjusted_elem) - 1u;

        // TODO
        char buf[32] {};
        std::to_chars(buf, buf+sizeof(buf), elems, 2);
        LOG(LOG_DEBUG, "elems=0x%u | 0b%s", elems, buf);

        uint32_t item_pos = underlying_cast(gui.focus_item);
        // ajust position for navigation and elements after
        if (gui.focus_item >= ElementId::VncNavigation)
        {
            // on navigation
            if (gui.focus_item == ElementId::VncNavigation)
            {
                item_pos += underlying_cast(gui.vnc.pagination.get_focus_elem()) - 1;
            }
            // after navigation
            else
            {
                item_pos += nb_extra_nav_buttons;
            }
        }

        uint32_t next_pos = item_pos;

        bool regular_focus = !mods.has_shift();

        LOG(LOG_DEBUG, "reg=%d state=%d", regular_focus, vim_nav.number_state);

        switch (vim_nav.number_state)
        {
            case D::VimNumberState::Begin:
                next_pos = checked_int{ std::countr_zero(elems) };
                break;

            case D::VimNumberState::End:
                next_pos = checked_int{ 32 - std::countl_zero(elems) };
                break;

            case D::VimNumberState::Empty: {
                if (regular_focus)
                {
                    // (hi) bits after elem
                    // 0bnnnnnnnn
                    //        ^
                    // 0bnnnnn000
                    uint32_t filtered_elems = elems & ~n_lo_bits(item_pos + 1u);
                    next_pos = select_next(filtered_elems);
                    if (next_pos >= last_adjusted_elem)
                    {
                        next_pos = select_next(elems);
                    }
                }
                else
                {
                    // (lo) bits before elem
                    // 0bnnnnnnnn
                    //        ^
                    // 0b000000nn
                    uint32_t filtered_elems = elems & n_lo_bits(item_pos);
                    next_pos = select_prev(filtered_elems);
                    if (next_pos == 0)
                    {
                        next_pos = select_prev(elems);
                    }
                    --next_pos;
                }

                break;
            }

            case D::VimNumberState::Stacked: {
                LOG(LOG_DEBUG, "pos=%u", item_pos);

                uint32_t nb_bits = checked_int{ std::popcount(elems) };

                LOG(LOG_DEBUG, "  %u <= %u = %d", nb_bits, gui.vnc.vim_mode.number, nb_bits <= gui.vnc.vim_mode.number);

                // to begin / last
                if (nb_bits <= gui.vnc.vim_mode.number)
                {
                    if (regular_focus)
                    {
                        next_pos = checked_int{ 32 - std::countl_zero(elems) };
                    }
                    else
                    {
                        next_pos = checked_int{ std::countr_zero(elems) };
                    }

                    break;
                }

                uint32_t filtered_elems = elems;
                uint32_t num = gui.vnc.vim_mode.number;

                if (regular_focus)
                {
                    while (num)
                    {
                        next_pos = select_next(filtered_elems) + num;
                        // 0bnnnnnnnn
                        //        ^
                        // 0bnnnnnn11
                        uint32_t mask = n_lo_bits(next_pos);
                        uint32_t nb_one = checked_int{ std::popcount(filtered_elems & mask) };
                        LOG(LOG_DEBUG, "next_pos: %u | nb_one: %u | mask: %u", next_pos, nb_one, mask);
                        filtered_elems &= ~mask;
                        num -= nb_one;
                    }
                    --next_pos;
                }
                else
                {
                    while (num)
                    {
                        next_pos = select_prev(filtered_elems) - num;
                        // 0bnnnnnnnn
                        //        ^
                        // 0bnnnnnn11
                        uint32_t mask = n_lo_bits(next_pos);
                        uint32_t nb_one = checked_int{ std::popcount(filtered_elems & ~mask) };
                        LOG(LOG_DEBUG, "next_pos: %u | nb_one: %u | mask: %u", next_pos, nb_one, mask);
                        filtered_elems &= mask;
                        num -= nb_one;
                    }
                }

                LOG(LOG_DEBUG, "pos=%u", next_pos);
                break;
            }
        }

        uint32_t nav_pos = underlying_cast(ElementId::VncNavigation);

        // on navigation or after
        if (nav_pos <= next_pos)
        {
            // on navigation element, adjust element position and sub-element
            if (next_pos <= nav_pos + nb_extra_nav_buttons)
            {
                auto nav_elem = next_pos - nav_pos;
                gui.vnc.pagination.set_focus_elem(WidgetPagination::FocusElement(nav_elem + 1));
                next_pos = nav_pos;
            }
            // adjust only the element position
            else
            {
                next_pos -= nb_extra_nav_buttons;
            }
        }

        assert(next_pos < underlying_cast(ElementId::GuiClose));

        ElementId next_focus = ElementId(next_pos);

        // always true ?
        if (!gui.disabled_elements.has(next_focus) && next_focus != gui.focus_item)
        {
            auto old_focus = gui.focus_item;
            gui.focus_item = next_focus;
            gui.pressed_item = ElementId::None;
            D::event_blur(gui, old_focus);
            D::event_press_or_focus(
                self,
                gui,
                D::InputEvent::KeyEvent,
                D::MouseEventData{},
                old_focus,
                next_focus,
                D::get_vnc_list_rect(gui),
                mods
            );
        }
    }

    static ElementId get_elem_bellow_mouse(GuiData & gui, Rect vnc_list_rect, uint16_t x, uint16_t y)
    {
        auto & layout = gui.layout;

        auto in = [](int x, int elem_x, int elem_w) {
            return elem_x <= x && x < elem_x + elem_w;
        };

        int btn_h = gui.line_h + D::BUTTON_HEIGHT_DECORATION;
        int btn_x_pad = D::BUTTON_WIDTH_DECORATION;

        // mid button
        if (x < vnc_list_rect.x)
        {
            if (in(x, layout.mid.button_x, layout.mid.button_inner_w + btn_x_pad))
            {
                // to vnc copy button
                if (in(y, layout.mid.copy_to_vnc_y, btn_h))
                {
                    return ElementId::ToVncButton;
                }
                // to rdp copy button
                else if (in(y, layout.mid.copy_to_rdp_y, btn_h))
                {
                    return ElementId::ToRdpButton;
                }
                // stop transfer button
                else if (in(y, layout.mid.stop_y, btn_h))
                {
                    return ElementId::StopTransferButton;
                }
            }
        }
        // vnc location, checkbox all or close gui button
        else if (y < vnc_list_rect.y)
        {
            auto checkbox_rect = D::Checkbox{gui, gui.vnc.all_file_checked}
                .boxed_click(gui, gui.layout.vnc.header_text_y);

            // checkbox all
            if (checkbox_rect.contains_pt(x, y))
            {
                return ElementId::VncListAllCheckbox;
            }
            // edit
            else if (gui.vnc.directory_edit.get_rect().contains_pt(x, y))
            {
                return ElementId::VncEditField;
            }
            // vnc location
            else if (in(y, gui.layout.vnc.top_bar_y, btn_h))
            {
                if (in(x, gui.layout.vnc.root_x, gui.layout.vnc.root_text_w + btn_x_pad))
                {
                    return ElementId::VncRootButton;
                }
                else if (in(x, gui.layout.vnc.parent_x, gui.layout.vnc.parent_text_w + btn_x_pad))
                {
                    return ElementId::VncParentButton;
                }
            }
            // close gui
            else if (gui.layout.close_btn.rect.contains_pt(x, y))
            {
                return ElementId::GuiClose;
            }
            // header (sorting)
            else if (in(y, layout.vnc.header_y, layout.vnc.header_h))
            {
                // filename
                if (x <= layout.vnc.list_vline_size_x)
                {
                    if (layout.vnc.header_filename_x <= x)
                    {
                        return ElementId::VncIconSortFilename;
                    }
                }
                // size
                else if (x <= layout.vnc.list_vline_date_x)
                {
                    if (layout.vnc.list_vline_size_x < x)
                    {
                        return ElementId::VncIconSortSize;
                    }
                }
                // date
                else if (x <= layout.vnc.inner_rect.eright())
                {
                    if (layout.vnc.list_vline_date_x < x)
                    {
                        return ElementId::VncIconSortDate;
                    }
                }
            }
        }
        // list element
        else if (vnc_list_rect.contains_pt(x, y))
        {
            return ElementId::VncList;
        }
        // navigation
        else if (gui.vnc.pagination.get_rect().contains_pt(x, y))
        {
            return ElementId::VncNavigation;
        }

        return ElementId::None;
    }

    //@}

    static void update_mouse_pointer(GuiData & gui, ElementId elem, uint16_t mouse_x, uint16_t mouse_y)
    {
        constexpr auto to_m = [](ElementId id, PointerShape cursor) {
            return underlying_cast(cursor) << (underlying_cast(id) * 2);
        };
        static constexpr uint32_t cursor_mask = 0u
            | to_m(ElementId::ToRdpButton, PointerShape::Pointer)
            | to_m(ElementId::ToVncButton, PointerShape::Pointer)
            | to_m(ElementId::StopTransferButton, PointerShape::Pointer)

            | to_m(ElementId::VncRootButton, PointerShape::Pointer)
            | to_m(ElementId::VncParentButton, PointerShape::Pointer)
            | to_m(ElementId::VncEditField, PointerShape::Edit)

            | to_m(ElementId::VncListAllCheckbox, PointerShape::Pointer)
            | to_m(ElementId::VncIconSortFilename, PointerShape::Pointer)
            | to_m(ElementId::VncIconSortSize, PointerShape::Pointer)
            | to_m(ElementId::VncIconSortDate, PointerShape::Pointer)
            | to_m(ElementId::VncNavigation, PointerShape::Pointer)

            | to_m(ElementId::GuiClose, PointerShape::Pointer)
            ;

        PointerShape new_cursor = PointerShape::Normal;

        if (!gui.disabled_elements.has(elem))
        {
            if (elem == ElementId::VncNavigation && gui.vnc.pagination.is_on_edit(mouse_x))
            {
                new_cursor = PointerShape::Edit;
            }
            else if (elem == ElementId::VncList)
            {
                auto offset_item = D::offset_item_from_y(gui, mouse_y);
                auto y_item = D::y_item_from_offset(gui, offset_item);
                if (D::Checkbox{ gui, false }
                    .boxed_click_from_y_item(gui, y_item)
                    .contains_pt(checked_int{mouse_x}, checked_int{mouse_y}))
                {
                    new_cursor = PointerShape::Pointer;
                }
            }
            else if (auto filtered = (cursor_mask >> underlying_cast(elem) * 2) & 0b11)
            {
                new_cursor = PointerShape(filtered);
            }
        }

        if (new_cursor != gui.current_mouse_pointer)
        {
            set_pointer(gui, new_cursor);
        }
    }

    static void set_pointer(GuiData & gui, PointerShape new_cursor)
    {
        PredefinedPointer predefined_pointer = PredefinedPointer::Normal;

        switch (new_cursor)
        {
            case PointerShape::Unspecified:
            case PointerShape::Normal:
                break;

            case PointerShape::Pointer:
                predefined_pointer = PredefinedPointer::Pointer;
                break;

            case PointerShape::Edit:
                predefined_pointer = PredefinedPointer::Edit;
                break;
        }

        gui.current_mouse_pointer = new_cursor;

        auto cached_bit_mask = (1u << underlying_cast(new_cursor));
        bool already_cached = gui.mouse_pointer_set_mask & cached_bit_mask;
        auto cursor_id = underlying_cast(new_cursor);

        if (already_cached)
        {
            gui.gd.cached_pointer(cursor_id);
        }
        else
        {
            gui.mouse_pointer_set_mask |= cached_bit_mask;
            auto & pointer = predefined_pointer_to_pointer(predefined_pointer);
            gui.gd.new_pointer(cursor_id, pointer);
        }
    }
};


template<typename T>
writable_array_view<T>
VNC::FileTransferGui::GuiData::allocate_array(std::size_t n)
{
    static_assert(std::is_standard_layout_v<T>);

    std::size_t data_len = n * sizeof(T);
    m_storage.allocate_memory(data_len, alignof(T));

    static_assert(alignof(T) <= alignof(MemoryBlock));
    // ptr is already aligned on T because (alignof(T) <= alignof(MemoryBlock))
    auto av = writable_array_view { std::launder(static_cast<T*>(m_storage.ptr)), n };

    m_storage.ptr = static_cast<char*>(m_storage.ptr) + data_len;

    return av;
}

void VNC::FileTransferGui::GuiData::Storage::allocate_memory(
    std::size_t data_len, std::size_t align_of_T)
{
    if (std::align(align_of_T, data_len, ptr, free_space)) [[likely]]
    {
        free_space -= data_len;
    }
    else
    {
        auto reserved_len = mmax(data_len, MemoryBlock::block_size - sizeof(MemoryBlock));
        auto allocated_len = reserved_len + sizeof(MemoryBlock);
        allocated += allocated_len;
        void * mem = MemoryBlock::aligned_alloc<MemoryBlock>(allocated_len);
        auto * block = new(mem) MemoryBlock {};
        *next_block_ptr = block;
        next_block_ptr = &block->next_block;
        ptr = static_cast<char*>(mem) + sizeof(MemoryBlock);
        free_space = reserved_len - data_len;
    }
}

// remove last bytes allocated by the last allocate_array()
void VNC::FileTransferGui::GuiData::free_after(void const * ptr) noexcept
{
    assert(ptr);
    auto pstorage = static_cast<char*>(m_storage.ptr);
    std::size_t delta = checked_int{ pstorage - static_cast<char const*>(ptr) };
    m_storage.ptr = pstorage - delta;
    m_storage.free_space += delta;
}

REDEMPTION_NOINLINE
VNC::FileTransferGui::GuiData *
VNC::FileTransferGui::GuiData::init_data(FileTransferGui & ft)
{
    /*
     * Extract translated messages
     */

    struct Msg
    {
        chars_view str {};
        std::size_t len = str.size();
    };
    Msg msgs[D::TextId::COUNT] {};
    constexpr size_t extra_len_for_singular_form = 12; // random value

    auto & tr = ft.m_tr;

    // limit size of unit for prevent possibly overflow in initialize_list_item
    auto unit_limit = [&](TrKey k){
        auto s = tr(k);
        return Msg{chars_view{s.data(), mmin(s.size(), 20u)}}; // "random" limit
    };

    auto nfmt_u32 = [&](auto k){
        auto n = tr.nfmt_len(k, ~uint32_t{});
        return Msg{.len = n + extra_len_for_singular_form};
    };

    msgs[D::TextId::button_root] = {"\\"_av};
    msgs[D::TextId::button_parent] = {".."_av};
    msgs[D::TextId::shortcut_desktop] = {tr(trkeys::vnc_ft_shortcut_desktop)};
    msgs[D::TextId::shortcut_document] = {tr(trkeys::vnc_ft_shortcut_document)};
    msgs[D::TextId::shortcut_network] = {tr(trkeys::vnc_ft_shortcut_network)};
    msgs[D::TextId::transfer_to_cb_in_progress] = {tr(trkeys::vnc_ft_transfer_to_cb_in_progress)};
    msgs[D::TextId::transfer_to_cb_completed] = {tr(trkeys::vnc_ft_transfer_to_cb_completed)};
    msgs[D::TextId::transfer_to_cb_aborted] = {tr(trkeys::vnc_ft_transfer_to_cb_aborted)};
    msgs[D::TextId::transfer_to_cb_error] = {tr(trkeys::vnc_ft_transfer_to_cb_error)};
    msgs[D::TextId::transfer_to_vnc_in_progress] = {tr(trkeys::vnc_ft_transfer_to_vnc_in_progress)};
    msgs[D::TextId::transfer_to_vnc_completed] = {tr(trkeys::vnc_ft_transfer_to_vnc_completed)};
    msgs[D::TextId::transfer_to_vnc_aborted] = {tr(trkeys::vnc_ft_transfer_to_vnc_aborted)};
    msgs[D::TextId::transfer_to_vnc_error] = {tr(trkeys::vnc_ft_transfer_to_vnc_error)};
    msgs[D::TextId::transfer_item_unit] = {tr(trkeys::vnc_ft_transfer_item_unit)};
    msgs[D::TextId::transfer_byte_unit] = {tr(trkeys::vnc_ft_transfer_byte_unit)};
    msgs[D::TextId::window_title] = {tr(trkeys::vnc_ft_title)};
    msgs[D::TextId::cb_pan_name] = {tr(trkeys::vnc_ft_cb_pan_name)};
    msgs[D::TextId::cb_list_empty] = {tr(trkeys::vnc_ft_cb_list_empty)};
    msgs[D::TextId::cb_list_loading] = {tr(trkeys::vnc_ft_cb_list_loading)};
    msgs[D::TextId::cb_to_paste_loading] = {tr(trkeys::vnc_ft_cb_to_paste_loading)};
    msgs[D::TextId::cb_to_paste_ready] = {tr(trkeys::vnc_ft_cb_to_paste_ready)};
    msgs[D::TextId::cb_list_requested] = {tr(trkeys::vnc_ft_list_loading)};
    msgs[D::TextId::vnc_pan_name] = {tr(trkeys::vnc_ft_vnc_pan_name)};
    msgs[D::TextId::vnc_list_disabled] = {tr(trkeys::vnc_ft_vnc_list_disabled)};
    msgs[D::TextId::vnc_list_loading] = {tr(trkeys::vnc_ft_list_loading)};
    msgs[D::TextId::vnc_list_empty] = {tr(trkeys::vnc_ft_vnc_list_empty)};
    msgs[D::TextId::vnc_list_error] = {tr(trkeys::vnc_ft_vnc_list_error)};
    msgs[D::TextId::vnc_list_total_items] = nfmt_u32(trkeys::vnc_ft_vnc_list_total_items);
    msgs[D::TextId::vnc_list_selected_items] = nfmt_u32(trkeys::vnc_ft_vnc_list_selected_items);
    msgs[D::TextId::vnc_header_filename] = {tr(trkeys::vnc_ft_vnc_header_filename)};
    msgs[D::TextId::vnc_header_size] = {tr(trkeys::vnc_ft_vnc_header_size)};
    msgs[D::TextId::vnc_header_modification_date] = {tr(trkeys::vnc_ft_vnc_header_modification_date)};
    msgs[D::TextId::folder] = {tr(trkeys::vnc_ft_type_folder)};
    msgs[D::TextId::unit_byte] = unit_limit(trkeys::vnc_ft_file_size_unit_byte);
    msgs[D::TextId::unit_kibibyte] = unit_limit(trkeys::vnc_ft_file_size_unit_kibibyte);
    msgs[D::TextId::unit_mebibyte] = unit_limit(trkeys::vnc_ft_file_size_unit_mebibyte);
    msgs[D::TextId::unit_gibibyte] = unit_limit(trkeys::vnc_ft_file_size_unit_gibibyte);
    msgs[D::TextId::unit_tebibyte] = unit_limit(trkeys::vnc_ft_file_size_unit_tebibyte);
    msgs[D::TextId::unit_pebibyte] = unit_limit(trkeys::vnc_ft_file_size_unit_pebibyte);
    msgs[D::TextId::unit_exbibyte] = unit_limit(trkeys::vnc_ft_file_size_unit_exbibyte);
    msgs[D::TextId::stop_placeholder] = {tr(trkeys::vnc_ft_stop_placeholder)};
    msgs[D::TextId::copy_to_rdp] = {tr(trkeys::vnc_ft_copy_to_rdp)};
    msgs[D::TextId::stop_to_rdp] = {tr(trkeys::vnc_ft_stop_to_rdp)};
    msgs[D::TextId::copy_to_vnc] = {tr(trkeys::vnc_ft_copy_to_vnc)};
    msgs[D::TextId::stop_to_vnc] = {tr(trkeys::vnc_ft_stop_to_vnc)};
    msgs[D::TextId::vnc_drive_network] = {tr(trkeys::vnc_ft_drive_network)};
    msgs[D::TextId::vnc_drive_local] = {tr(trkeys::vnc_ft_drive_local)};
    msgs[D::TextId::vnc_drive_removable] = {tr(trkeys::vnc_ft_drive_removable)};
    msgs[D::TextId::vnc_drive_cd_rom] = {tr(trkeys::vnc_ft_drive_cd_rom)};
    msgs[D::TextId::cb_list_counter] = { .len = D::NB_MSG_CAPACITY };
    msgs[D::TextId::cb_list_finished] = nfmt_u32(trkeys::vnc_ft_cb_list_total);
    msgs[D::TextId::transfer_file_error] = {
        .len = tr(trkeys::vnc_ft_transfer_file_error).size()
             + UVncFile::max_path_length
    };

    /*
     * Compute allocated data
     */

    std::size_t fcs_count = 0;
    for (auto msg : msgs)
    {
        assert(msg.len && "TextId not initialized");
        fcs_count += msg.len;
    }
    using OffsetType = std::remove_cvref_t<decltype(GuiData::fcs_offsets[0])>;
    OffsetType fcs_capacity = mmin(
        fcs_count,
        std::numeric_limits<OffsetType>::max()
    );

    /*
     * Allocated data
     */

    static_assert(alignof(Storage) == alignof(MemoryBlock));
    static_assert(sizeof(Storage) <= MemoryBlock::block_size);
    std::size_t fcs_buffer_len = fcs_capacity * sizeof(FontCharPtr);
    std::size_t allocated_len = fcs_buffer_len
                              + alignof(FontCharPtr) // padding for alignment
                              + sizeof(GuiData);
    std::size_t free_space = mmax(allocated_len, MemoryBlock::block_size);
    void * mem = MemoryBlock::aligned_alloc<GuiData>(free_space);
    free_space -= sizeof(GuiData);

    void * buffer = static_cast<char*>(mem) + sizeof(GuiData);
    std::align(alignof(FontCharPtr), fcs_buffer_len, buffer, free_space);
    auto * fcs = std::launder(static_cast<FontCharPtr*>(buffer));
    void * ptr = static_cast<char*>(buffer) + fcs_buffer_len;
    free_space -= fcs_buffer_len;

    auto & gui = *new (mem) GuiData {
        .m_storage = {
            .ptr = ptr,
            .free_space = free_space,
            .first_block = nullptr,
            .next_block_ptr = nullptr,
            .initial_ptr = ptr,
            .initial_free_space = free_space,
            .allocated = 0,
        },
        .fcs = fcs,
        .tr = ft.m_tr,
        .gd = ft.m_gd,
        .font = ft.m_font,
        .vnc = {
            .directory_edit = {
                ft.m_gd,
                ft.m_font,
                D::colors.edit,
                WidgetEventNotifier { []{
                    // TODO
                    LOG(LOG_DEBUG, "submit directory");
                } },
            },
            .pagination = {
                ft.m_gd,
                ft.m_font,
                D::colors.pagination,
                WidgetPagination::RedrawAfterEvent::Yes,
                [&ft](uint32_t new_widget_page) {
                    auto & gui = ft.gui();
                    auto selected_index = D::widget_page_to_page_index(gui, new_widget_page);
                    D::set_widget_page(gui, new_widget_page, selected_index);
                },
            },
        }
    };
    gui.m_storage.next_block_ptr = &gui.m_storage.first_block;

    /*
     * Init some fields
     */

    gui.disabled_elements.add(D::ElementId::VncRootButton);
    gui.disabled_elements.add(D::ElementId::VncParentButton);
    gui.disabled_elements.add(D::ElementId::ToVncButton);
    gui.disabled_elements.add(D::ElementId::ToRdpButton);
    gui.disabled_elements.add(D::ElementId::StopTransferButton);
    gui.disabled_elements.add(D::ElementId::VncListAllCheckbox);

    /*
     * Init fcs, offsets and widths
     */

    auto * fcs_it = fcs;
    OffsetType offset = 0;
    std::size_t i = 0;

    // init offsets for dynamic string
    for (; i <= D::TextId::LAST_DYNAMIC; ++i)
    {
        auto n = mmin(fcs_capacity, msgs[i].len);
        fcs_capacity -= n;
        offset += n;
        fcs_it += n;
        gui.fcs_offsets[i] = offset;
    }

    // init prefix of dynamic text
    struct TData { D::TextId::E id; TrKey key; };
    for (auto d : {
        TData{D::TextId::transfer_file_error, trkeys::vnc_ft_transfer_file_error},
    })
    {
        auto * fcs = D::fcs_data(gui, d.id);
        auto msg = D::fcs_init(fcs, gui.font, tr(d.key));
        gui.fcs_lengths[d.id] = checked_int{msg.fcs.size()};
        gui.text_widths[d.id] = msg.text_width;
    }

    // init offsets and width for fixed string
    for (; i < D::TextId::COUNT; ++i)
    {
        auto n = mmin(fcs_capacity, msgs[i].len);
        auto fc_msg = D::fcs_init(fcs_it, gui.font, msgs[i].str.first(n));
        uint16_t len = checked_int{fc_msg.fcs.size()};
        fcs_capacity -= len;
        offset += len;
        fcs_it += len;
        gui.fcs_offsets[i] = offset;
        gui.text_widths[i] = fc_msg.text_width;
    }

    // adjust right padding
    for (auto text_id : {
        D::TextId::button_root,
        D::TextId::button_parent,
    })
    {
        gui.text_widths[text_id] += D::fcs(gui, text_id)[0]->offsetx / 2;
    }

    // remove left padding pixel inserted by fcs_init()
    for (auto i = underlying_cast(D::TextId::unit_byte); i <= D::TextId::unit_exbibyte; ++i)
    {
        if (gui.text_widths[i])
        {
            --gui.text_widths[i];
        }
    }

    // release last bytes: fcs_capacity - fcs_count

    /*
     * Init chars (icons) and member widths
     */

    gui.mid_button_text_max_w = mmax({
        gui.text_widths[D::TextId::copy_to_rdp],
        gui.text_widths[D::TextId::copy_to_vnc],
        gui.text_widths[D::TextId::stop_to_rdp],
        gui.text_widths[D::TextId::stop_to_vnc],
        gui.text_widths[D::TextId::stop_placeholder],
    });

    gui.max_unit_nb_fc = checked_int{ mmax({
        D::fcs(gui, D::TextId::unit_byte).size(),
        D::fcs(gui, D::TextId::unit_kibibyte).size(),
        D::fcs(gui, D::TextId::unit_mebibyte).size(),
        D::fcs(gui, D::TextId::unit_gibibyte).size(),
        D::fcs(gui, D::TextId::unit_tebibyte).size(),
        D::fcs(gui, D::TextId::unit_pebibyte).size(),
        D::fcs(gui, D::TextId::unit_exbibyte).size(),
    }) };

    using FileDataType = GuiData::FileDataType;

    auto file_icon = [&](GuiData::FileDataType type) -> FontCharPtr& {
        return gui.icons.file_icons[underlying_cast(type)];
    };

    gui.icons.title_icon = &gui.font.item(GlyphNames::copy).view;
    gui.icons.close_x = &gui.font.item(GlyphNames::xmark).view;
    gui.icons.sort_a_to_z = {
        &gui.font.item(GlyphNames::arrow_down_a_to_z).view, // ↓ a-z
        &gui.font.item(GlyphNames::arrow_up_z_to_a).view, // ↑ z-a
    };
    gui.icons.sort_1_to_9 = {
        &gui.font.item(GlyphNames::arrow_down_1_to_9).view, // ↓ 1-9
        &gui.font.item(GlyphNames::arrow_up_9_to_1).view, // ↑ 9-1
    };
    gui.icons.sort_9_to_1 = {
        &gui.font.item(GlyphNames::arrow_down_9_to_1).view, // ↓ 9-1
        &gui.font.item(GlyphNames::arrow_up_1_to_9).view, // ↑ 1-9
    };
    gui.icons.box_checked = &gui.font.item(GlyphNames::square_check).view; // ☒
    gui.icons.box_unchecked = &gui.font.item(GlyphNames::square).view; // ☐
    gui.icons.checkbox_w = checked_int {
        mmax(
            gui.icons.box_checked->boxed_width(),
            gui.icons.box_unchecked->boxed_width()
        ) + 1
    };
    file_icon(FileDataType::LocalDisk) = &gui.font.item(GlyphNames::hard_drive).view; // 🖴
    file_icon(FileDataType::MediaDisk) = &gui.font.item(GlyphNames::floppy_disk).view; // 🖬
    file_icon(FileDataType::NetworkDisk) = &gui.font.item(GlyphNames::network_drive).view; // 🖧
    file_icon(FileDataType::CDRom) = &gui.font.item(GlyphNames::compact_disc).view; // 🖸
    file_icon(FileDataType::Directory) = &gui.font.item(GlyphNames::folder).view; // 🗀
    file_icon(FileDataType::RegularFile) = &gui.font.item(GlyphNames::file).view; // 🖹

    gui.icons.file_icon_w = 0;
    for (auto & icon : gui.icons.file_icons)
    {
        if (!icon)
        {
            icon = file_icon(FileDataType::Directory);
        }
        gui.icons.file_icon_w = mmax(gui.icons.file_icon_w, icon->width);
    }
    ++gui.icons.file_icon_w;

    uint16_t max_digit_w = gui.font.max_digit_width();
    int dot_w = gui.font.item('.').view.boxed_width();
    int space_w = gui.font.item(' ').view.boxed_width();
    int slash_w = gui.font.item('/').view.boxed_width();
    int date_sep_w = slash_w;
    int time_sep_w = gui.font.item(':').view.boxed_width();
    int sort_size_icon_w
        = mmax(gui.icons.sort_9_to_1.ascending->width, gui.icons.sort_9_to_1.ascending->width)
        + space_w + 1;
    int sort_date_icon_w
        = mmax(gui.icons.sort_1_to_9.ascending->width, gui.icons.sort_1_to_9.ascending->width)
        + space_w + 1;

    uint16_t unit_max_w = mmax({
        gui.text_widths[D::TextId::unit_byte],
        gui.text_widths[D::TextId::unit_kibibyte],
        gui.text_widths[D::TextId::unit_mebibyte],
        gui.text_widths[D::TextId::unit_gibibyte],
        gui.text_widths[D::TextId::unit_tebibyte],
        gui.text_widths[D::TextId::unit_pebibyte],
        gui.text_widths[D::TextId::unit_exbibyte],
    });

    // YYYY/mm/dd HH:MM:SS
    gui.max_date_w = checked_int{
        max_digit_w * 14 + date_sep_w * 2 + time_sep_w * 2 + space_w + 1
    };
    uint16_t header_date_w = checked_int{
        gui.text_widths[D::TextId::vnc_header_modification_date] + sort_size_icon_w
    };
    gui.max_date_w = mmax(gui.max_date_w, header_date_w);

    // NNNN.MM KiB
    int folder_w = gui.text_widths[D::TextId::folder] + unit_max_w;
    gui.max_file_size_w = checked_int{mmax(
        max_digit_w * 6 + dot_w + space_w + unit_max_w + 1,
        folder_w
    )};
    uint16_t header_size_w = checked_int{
        gui.text_widths[D::TextId::vnc_header_size] + sort_date_icon_w
    };
    gui.max_file_size_w = mmax(gui.max_file_size_w, header_size_w);
    gui.folder_right_pad = checked_int{ gui.max_file_size_w - folder_w };

    auto * unit_right_pad_it = gui.unit_right_pads;
    for (auto i = underlying_cast(D::TextId::unit_byte); i <= D::TextId::unit_exbibyte; ++i)
    {
        *unit_right_pad_it++ = checked_int{ unit_max_w - gui.text_widths[i] };
    }

    gui.nav_total_page_prefix_w = checked_int{ slash_w + space_w };

    gui.max_digit_w = checked_int{ max_digit_w };
    gui.space_w = checked_int{ space_w };

    gui.line_h = gui.font.max_height();

    return &gui;
}

void VNC::FileTransferGui::GuiData::Storage::release_blocks() noexcept
{
    auto * next = first_block;
    while (next)
    {
        auto * current = std::exchange(next, next->next_block);
        D::MemoryBlock::aligned_dealloc<D::MemoryBlock>(current);
    }
    ptr = initial_ptr;
    free_space = initial_free_space;
    first_block = nullptr;
    next_block_ptr = &first_block;
    allocated = 0;
}

VNC::FileTransferGui::~FileTransferGui()
{
    if (m_gui)
    {
        m_gui->m_storage.release_blocks();
        m_gui->~GuiData();
        D::MemoryBlock::aligned_dealloc<GuiData>(m_gui);
    }
}


VNC::FileTransferGui::FileTransferGui(
    gdi::GraphicApi & gd, Font const & font, EventContainer & events,
    MaxFileIntType max_file_list, TransferOptions transfer_opts,
    Translator tr, Callbacks callbacks
) noexcept
    : m_flags{
        Flags{}
        | (flags_any(transfer_opts, TransferOptions::CbToVnc) ? Flags::CbToVnc : Flags{})
        | (flags_any(transfer_opts, TransferOptions::VncToCb) ? Flags::VncToCb : Flags{})
    }
    , m_max_file_list(mmin(max_file_list, D::INVALID_INDEX - 1u))
    , m_gd(gd)
    , m_callbacks(callbacks)
    , m_font(font)
    , m_event_guard(events)
    , m_tr(tr)
{}

bool VNC::FileTransferGui::is_open() const noexcept
{
    return flags_any(m_flags, Flags::IsOpen);
}

VNC::UVncFile::PathView VNC::FileTransferGui::current_directory() const noexcept
{
    return gui().vnc.directory.name_av();
}

void VNC::FileTransferGui::open(uint16_t width, uint16_t height, uint16_t mouse_x, uint16_t mouse_y)
{
    if (!m_gui) [[unlikely]]
    {
        m_gui = GuiData::init_data(*this);
    }

    auto & gui = this->gui();
    auto & layout = gui.layout;

    /*
     * Update mouse pointer
     */

    D::update_mouse_pointer(
        gui,
        D::get_elem_bellow_mouse(gui, D::get_vnc_list_rect(gui), mouse_x, mouse_y),
        mouse_x,
        mouse_y
    );

    /*
     * Init layout
     */

    layout.width = width;
    layout.height = height;

    auto line_h = gui.line_h;
    auto text_h = gui.line_h + 1;  // TODO +1 for to fix bug in draw_text()

    layout.title_h = checked_int{ text_h + D::TITLE_Y_PADDING * 2 };

    int button_h = line_h + D::BUTTON_HEIGHT_DECORATION;
    int boxed_button_h = button_h + D::ELEMENT_Y_SEPARATOR * 2;

    int pan_y = (layout.title_h + D::TITLE_BOTTOM_SEPARATOR) * 2
              + D::PAN_Y_MARGIN
              + D::PAN_BORDER_LEN
              ;
    int pan_text_y = pan_y + boxed_button_h;

    int mid_pan_w = gui.mid_button_text_max_w
                  + D::BUTTON_WIDTH_DECORATION
                  ;

    int rdp_pan_w = middle_pos(
        width - (D::PAN_BORDER_LEN + D::PAN_X_MARGIN) * 4 - D::MID_PAN_X_PADDING * 2,
        mid_pan_w
    );

    int pan_h = height
              - pan_y
              - D::PAN_Y_MARGIN
              - D::PAN_BORDER_LEN
              ;

    /*
     * Close button part
     */

    auto const& close_btn = gui.icons.close_x;

    int close_x_pad = mmax({
        static_cast<int>(close_btn->width / 2),
        static_cast<int>(close_btn->incby - close_btn->width),
        static_cast<int>(close_btn->offsetx)
    }) * 3;
    int close_btn_w = close_btn->width + close_x_pad * 2;
    int close_btn_h = text_h + D::TITLE_Y_PADDING * 2;

    int close_y_pad = mmax({
        static_cast<int>((close_btn_h - close_btn->height) / 2),
        static_cast<int>(close_btn->offsety)
    });

    layout.close_btn = {
        .rect = {
            checked_int{gui.layout.width - close_btn_w},
            0,
            checked_int{close_btn_w},
            checked_int{close_btn_h}
        },
        .x_pad = checked_int{close_x_pad},
        .y_pad = checked_int{close_y_pad},
    };

    /*
     * Cb part
     */

    layout.cb_name_x = checked_int{
        middle_pos(rdp_pan_w, gui.text_widths[D::TextId::cb_pan_name])
    };

    int progress_y = pan_text_y + text_h * 5;
    auto next_progress_y = [&](int dy = 0) {
        progress_y += text_h + dy;
        return checked_int { progress_y };
    };

    layout.cb_rdp = {
        .inner_rect = {
            D::PAN_X_MARGIN + D::PAN_BORDER_LEN,
            checked_int{ pan_y },
            checked_int{ rdp_pan_w },
            checked_int{ pan_h },
        },
        .text1_y = checked_int{ pan_text_y },
        .text2_y = checked_int{ pan_text_y + text_h },
        .progress_msg_y = next_progress_y(),
        .progress_path_y = next_progress_y(),
        .progress_percent_y = next_progress_y(line_h / 2),
        .progress_eta_y = next_progress_y(line_h / 4),
        .progress_transferred_items_y = next_progress_y(text_h + line_h / 2),
        .progress_transferred_bytes_y = next_progress_y(line_h / 4),
        .progress_right_text_limit = checked_int {
            rdp_pan_w / 2
            - (gui.space_w * 3
               + gui.text_widths[D::TextId::transfer_item_unit]
            ) / 2
        },
    };

    /*
     * Mid part
     */

    int mid_pan_x = rdp_pan_w
                  + (D::PAN_X_MARGIN + D::PAN_BORDER_LEN) * 2
                  + D::MID_PAN_X_PADDING;
    int mid_box_h = (button_h + D::ELEMENT_Y_SEPARATOR) * 4;
    int mid_box_y = (mid_box_h + pan_text_y + line_h * 6 < pan_h)
        ? pan_text_y + line_h * 3
        : middle_pos(pan_h, mid_box_h);

    layout.mid = {
        .rect = {
            checked_int{ mid_pan_x },
            checked_int{ pan_y },
            checked_int{ mid_pan_w },
            checked_int{ pan_h },
        },

        .button_x = checked_int{ mid_pan_x },
        .button_inner_w = gui.mid_button_text_max_w,

        .copy_to_vnc_y = checked_int{ mid_box_y + D::ELEMENT_Y_SEPARATOR },
        .copy_to_vnc_left_pad = checked_int{
            (gui.mid_button_text_max_w - gui.text_widths[D::TextId::copy_to_vnc]) / 2
        },

        .copy_to_rdp_y = checked_int{ mid_box_y + boxed_button_h },
        .copy_to_rdp_left_pad = checked_int{
            (gui.mid_button_text_max_w - gui.text_widths[D::TextId::copy_to_rdp]) / 2
        },

        .stop_y = checked_int{ mid_box_y + boxed_button_h * 2 + line_h },
        .stop_to_vnc_left_pad = checked_int{
            (gui.mid_button_text_max_w - gui.text_widths[D::TextId::stop_to_vnc]) / 2
        },

        .stop_to_rdp_left_pad = checked_int{
            (gui.mid_button_text_max_w - gui.text_widths[D::TextId::stop_to_rdp]) / 2
        },

        .stop_placeholder_left_pad = checked_int{
            (gui.mid_button_text_max_w - gui.text_widths[D::TextId::stop_placeholder]) / 2
        },
    };

    /*
     * Vnc part
     */

    int vnc_pan_x = rdp_pan_w + mid_pan_w
                  + (D::PAN_X_MARGIN + D::PAN_BORDER_LEN) * 3
                  + D::MID_PAN_X_PADDING * 2;
    int vnc_pan_w = width - vnc_pan_x
                  - (D::PAN_X_MARGIN + D::PAN_BORDER_LEN);

    layout.vnc_name_x = checked_int{
        vnc_pan_x + middle_pos(vnc_pan_w, gui.text_widths[D::TextId::vnc_pan_name])
    };

    int vnc_header_y = pan_text_y;
    int vnc_header_h = line_h + D::LINE_Y_PADDING * 2;
    int vnc_list_y = vnc_header_y + vnc_header_h + D::COLUMN_BORDER_LEN;
    int vnc_list_h = pan_y + pan_h - vnc_list_y;

    int vnc_list_minus_nav_h = vnc_list_h - gui.vnc.pagination.cy() - D::ELEMENT_Y_SEPARATOR;
    int vnc_nav_y = vnc_list_y + vnc_list_minus_nav_h + D::ELEMENT_Y_SEPARATOR;

    gui.vnc.pagination.set_xy(0, checked_int{ vnc_nav_y });

    auto & fc_checkbox = *gui.icons.box_checked;

    int vnc_checkbox_x = vnc_pan_x;
    int checkbox_offset_x = fc_checkbox.offsetx + 1;
    int vnc_vline_icon_x = vnc_checkbox_x + checkbox_offset_x * 2 + gui.icons.checkbox_w;
    int vnc_file_icon_x = vnc_vline_icon_x + D::COLUMN_BORDER_LEN + checkbox_offset_x;
    int vnc_date_x = vnc_pan_x + vnc_pan_w - (gui.max_date_w + D::COLUMN_X_PADDING);
    int vnc_vline_date_x = vnc_date_x - D::COLUMN_X_PADDING - D::COLUMN_BORDER_LEN;
    int vnc_size_x = vnc_vline_date_x - (gui.max_file_size_w + D::COLUMN_X_PADDING);
    int vnc_vline_size_x = vnc_size_x - D::COLUMN_X_PADDING - D::COLUMN_BORDER_LEN;
    int vnc_filename_x = vnc_file_icon_x + gui.icons.file_icon_w + checkbox_offset_x;
    int vnc_filename_w = vnc_vline_size_x - D::COLUMN_X_PADDING - vnc_filename_x;

    int list_item_h = line_h + D::LINE_Y_PADDING * 2;

    auto root_text_w = gui.text_widths[D::TextId::button_root];

    layout.vnc = {
        .inner_rect = {
            checked_int{ vnc_pan_x },
            checked_int{ pan_y },
            checked_int{ vnc_pan_w },
            checked_int{ pan_h },
        },

        .top_bar_y = checked_int{ pan_y + D::ELEMENT_Y_SEPARATOR },
        .root_x = checked_int{ vnc_pan_x + D::ELEMENT_X_SEPARATOR },
        .root_text_w = root_text_w,
        .parent_x = checked_int{
            vnc_pan_x + root_text_w
                + D::ELEMENT_X_SEPARATOR * 2
                + D::BUTTON_WIDTH_DECORATION
        },
        .parent_text_w = gui.text_widths[D::TextId::button_parent],

        .header_y = checked_int{ vnc_header_y },
        .header_text_y = checked_int{ vnc_header_y + D::LINE_Y_PADDING },
        .header_h = checked_int{ vnc_header_h },
        .header_filename_x = checked_int{ vnc_filename_x },
        .header_filename_w = checked_int{ vnc_filename_w },
        .header_size_x = checked_int{ vnc_size_x },
        .header_date_x = checked_int{ vnc_date_x },

        .list_y = checked_int{ vnc_list_y },
        .list_minus_nav_h = checked_int{ vnc_list_minus_nav_h },
        .list_checkbox_x = checked_int{ vnc_checkbox_x },
        .list_file_icon_x = checked_int{ vnc_file_icon_x },
        .list_vline_icon_x = checked_int{ vnc_vline_icon_x },
        .list_vline_size_x = checked_int{ vnc_vline_size_x },
        .list_vline_date_x = checked_int{ vnc_vline_date_x },
        .list_item_h = checked_int{ list_item_h },

        .list_total_y = checked_int{ vnc_nav_y + (gui.vnc.pagination.cy() - line_h) / 2 },
        .list_total_x = checked_int{ vnc_pan_x + 2 },
        .list_total_selected_x = 0,
        .list_total_selected_margin_left = checked_int{ gui.space_w * 3 },

        .item_by_page_with_nav = checked_int{ mmax(1, vnc_list_minus_nav_h / list_item_h) },

        .loading_x = checked_int{
            vnc_pan_x
            + middle_pos(vnc_pan_w, gui.text_widths[D::TextId::vnc_list_loading])
        },
        .loading_y = checked_int{ vnc_list_y + line_h },

        .last_y_drawing = 0,
    };

    /*
     * Vnc location layout
     */

    auto x_edit = layout.vnc.parent_x
                + layout.vnc.parent_text_w
                + D::BUTTON_WIDTH_DECORATION
                + D::ELEMENT_X_SEPARATOR;
    gui.vnc.directory_edit.set_xy(
        checked_int{x_edit},
        layout.vnc.inner_rect.y + D::ELEMENT_Y_SEPARATOR
            + (gui.line_h + D::BUTTON_HEIGHT_DECORATION - gui.vnc.directory_edit.cy()) / 2
    );
    gui.vnc.directory_edit.update_width(checked_int{
        layout.vnc.inner_rect.eright() - x_edit - D::ELEMENT_X_SEPARATOR
    });


    /*
     * Reset states
     */

    m_flags |= Flags::IsOpen;
    gui.flags = m_flags;


    /*
     * Draw
     */

    Rect clip{0, 0, width, height};
    D::draw_ui(gui, clip, D::ComputeMode::UpdateRefresh, gui.cb.next_display_state);
}

void VNC::FileTransferGui::close() noexcept
{
    auto & gui = this->gui();

    m_flags &= ~Flags::IsOpen;
    gui.flags = m_flags;
    gui.current_mouse_pointer = D::PointerShape::Unspecified;
    gui.cb.display_state = D::CbState::None;
    gui.vnc.reset_last_pressed_index();
    gui.vnc.reset_previous_selected_index();
    gui.pressed_item = GuiData::ElementId{};
    gui.last_click_time = {};
    gui.progress.show_progress = false;
}

void VNC::FileTransferGui::server_vnc_file_disabled()
{
    D::update_vnc_start_event(gui(), D::VncState::Disabled);
}

void VNC::FileTransferGui::server_vnc_file_list_error()
{
    D::update_vnc_start_event(gui(), D::VncState::Error);
}

void VNC::FileTransferGui::server_vnc_file_list_start(UVncFile::PathView dir_name)
{
    auto & gui = this->gui();
    auto & dir = gui.vnc.directory;

    if (!bytes_equal(dir_name.native(), dir.name_av().native()))
    {
        dir.buffer_len = checked_int {
            bytes_copy(make_writable_array_view(dir.buffer), dir_name.native())
        };

        auto buffer = cp1252_to_utf32.buffer_from(dir.name_av().native());
        gui.vnc.directory_edit.set_text(buffer.av(), { WidgetEdit::Redraw(is_open()) });
    }

    D::update_vnc_start_event(gui, D::VncState::Loading);
}

bool VNC::FileTransferGui::server_vnc_file_list_add(UVncFile file)
{
    auto & gui = this->gui();

    if (gui.vnc.state != D::VncState::Loading
     || gui.vnc.files.size() == m_max_file_list) [[unlikely]]
    {
        return false;
    }

    auto file_name = gui.allocate_array<uint8_t>(file.file_name.native().size());
    bytes_copy(file_name, file.file_name.native());

    // font chars are lazily initialized in draw_vnc_list()

    gui.vnc.files.push_back(D::FileData{
        .file_size = file.file_size,
        .last_access_time = file.last_access_time,
        .file_name = file_name.data(),
        .fcs = nullptr,
        .fcs_name_len = 0,
        .file_name_len = checked_int{file_name.size()},
        .fcs_file_size_len = 0,
        .file_size_offset_x = 0,
        .file_type = file.is_dir
            ? D::FileDataType::Directory
            : D::FileDataType::RegularFile,
        .checked = false,
    });

    if (file.is_dir)
    {
        ++gui.vnc.directory_counter;
    }
    else
    {
        ++gui.vnc.file_counter;
    }

    return true;
}

void VNC::FileTransferGui::server_vnc_file_list_add_drive(uint8_t letter, DriveType type)
{
    using FileDataType = D::FileDataType;

    // type value:     FileDataType
    // c: 0b11|0001|1   CDRom
    // f: 0b11|0011|0   MediaDisk
    // l: 0b11|0110|0   LocalDisk
    // n: 0b11|0111|0   NetworkDisk
    // \: 0b10|1110|0   LocalDisk
    //         ~~~~
    constexpr auto mask = 0b1111u;
    constexpr auto mask_len = 4u;
    constexpr auto to_b4 = [](DriveType t) {
        return ((static_cast<uint64_t>(t) >> 1) & mask) * mask_len;
    };
    static_assert(underlying_cast(FileDataType::LocalDisk) == 0);
    // default = LocalDisk
    constexpr auto tbl_file_type_map = uint64_t{}
        | (static_cast<uint64_t>(FileDataType::CDRom) << to_b4(DriveType::CDRom))
        | (static_cast<uint64_t>(FileDataType::MediaDisk) << to_b4(DriveType::MediaDisk))
        | (static_cast<uint64_t>(FileDataType::NetworkDisk) << to_b4(DriveType::NetworkDisk))
        ;
    auto file_type = checked_cast<FileDataType>(
        (tbl_file_type_map >> to_b4(type)) & mask
    );
    assert(file_type < FileDataType::Directory);

    auto text_id = D::TextId::START_DRIVE_TEXT + underlying_cast(file_type);

    auto & gui = this->gui();

    LOG(LOG_DEBUG, "add drive: %c", letter);

    auto file_name = gui.allocate_array<uint8_t>(3);
    file_name[0] = letter;
    file_name[1] = ':';
    file_name[2] = '\\';

    auto fcs = gui.allocate_array<FontCharPtr>(2);
    fcs[0] = &gui.font.item(letter).view;
    fcs[1] = &gui.font.item(':').view;

    gui.vnc.files.push_back(D::FileData{
        .file_size = 0,
        .last_access_time = {},
        .file_name = file_name.data(),
        .fcs = fcs.data(),
        .fcs_name_len = checked_int{ fcs.size() },
        .file_name_len = checked_int{ file_name.size() },
        .fcs_file_size_len = checked_int{
            gui.fcs_offsets[text_id] - gui.fcs_offsets[text_id - 1]
        },
        .file_size_offset_x = 0,
        .file_type = file_type,
        .checked = false,
    });
    ++gui.vnc.directory_counter;
}

void VNC::FileTransferGui::server_vnc_file_list_add_shorcuts()
{
    auto & gui = this->gui();

    struct Shortcut
    {
        D::TextId::E text_id;
        UVncFile::PathView::Bytes name;
    };

    for (auto [text_id, shortcut] : {
        Shortcut{D::TextId::shortcut_desktop, "Desktop"_sized_av},
        Shortcut{D::TextId::shortcut_document, "My Documents"_sized_av},
        Shortcut{D::TextId::shortcut_network, "Network Favorites"_sized_av},
    })
    {
        auto fcs = D::fcs(gui, text_id);

        gui.vnc.files.push_back(D::FileData{
            .file_size = 0,
            .last_access_time = {},
            .file_name = shortcut.data(),
            .fcs = fcs.data(),
            .fcs_name_len = fcs.msize(),
            .file_name_len = shortcut.msize(),
            .fcs_file_size_len = 0,
            .file_size_offset_x = 0,
            .file_type = D::FileDataType::Shortcut,
            .checked = false,
        });

        ++gui.vnc.directory_counter;
    }
}

void VNC::FileTransferGui::server_vnc_file_list_end()
{
    auto & gui = this->gui();

    bool has_elem = !gui.vnc.files.empty();

    gui.text_widths[D::TextId::vnc_list_selected_items] = 0;

    if (has_elem)
    {
        /*
         * Compute counter items size
         */

        uint32_t nb_items = checked_int{ gui.vnc.files.size() };

        int list_info_text_x_end = gui.layout.vnc.list_total_x;

        list_info_text_x_end += D::init_dynamic_fcs_and_lengths(
            gui, D::TextId::vnc_list_total_items,
            Translator::FmtMsg<128>(gui.tr, trkeys::vnc_ft_vnc_list_total_items, nb_items)
        ).text_width;

        list_info_text_x_end += gui.layout.vnc.list_total_selected_margin_left;
        gui.layout.vnc.list_total_selected_x = checked_int { list_info_text_x_end };

        /*
         * Compute pagination widget
         */

        if (D::has_nav(gui))
        {
            gui.disabled_elements.remove(D::ElementId::VncNavigation);
            auto nb_page = (gui.vnc.files.size() + gui.layout.vnc.item_by_page_with_nav - 1)
                         / gui.layout.vnc.item_by_page_with_nav;
            gui.vnc.pagination.update({
                .current_page = 1,
                .total_page = checked_int{ nb_page },
            });

            // positioning to right
            int vnc_nav_x = gui.layout.vnc.inner_rect.eright() - gui.vnc.pagination.cx();
            gui.vnc.pagination.set_xy(checked_int{ vnc_nav_x }, gui.vnc.pagination.y());
        }

        /*
         * Init sorting index
         */
        D::init_vnc_sorted_name(gui);
        D::init_vnc_sorted_based_on_field_with_option(gui);

        /*
         * Enable widgets
         */

        gui.disabled_elements.remove(D::ElementId::VncList);
        gui.disabled_elements.remove(D::ElementId::VncListAllCheckbox);

        if (gui.focus_item == D::ElementId::None)
        {
            gui.focus_item = D::ElementId::VncList;
        }

        if (is_open())
        {
            D::DrawCtx ctx{gui, gui.layout.vnc.inner_rect};
            ctx.draw_vnc_all_file_checkbox();
        }
    }

    /*
     * Update other states
     */

    D::update_vnc_list(gui, has_elem ? D::VncState::Ready : D::VncState::Empty);
    D::update_copy_buttons(gui);
}

void VNC::FileTransferGui::client_cb_file_list_reset()
{
    auto & gui = this->gui();
    gui.cb.total_file = 0;
    gui.cb.nb_file = 0;

    D::draw_cb_part(
        gui,
        gui.layout.cb_rdp.inner_rect,
        D::CbState::Empty,
        D::ComputeMode::Update,
        D::ForceUpdate::No
    );

    D::update_copy_buttons(gui);
}

void VNC::FileTransferGui::client_cb_file_list_requested()
{
    auto & gui = this->gui();
    gui.cb.total_file = 0;
    gui.cb.nb_file = 0;

    D::draw_cb_part(
        gui,
        gui.layout.cb_rdp.inner_rect,
        D::CbState::Requested,
        D::ComputeMode::Update,
        D::ForceUpdate::No
    );

    D::update_copy_buttons(gui);
}

void VNC::FileTransferGui::client_cb_file_list_start(uint32_t total_file)
{
    auto & gui = this->gui();
    gui.cb.nb_file = 0;
    gui.cb.total_file = total_file;
    gui.cb.last_time_of_update_nb_file = m_event_guard.get_monotonic_time();

    D::draw_cb_part(
        gui,
        gui.layout.cb_rdp.inner_rect,
        D::CbState::Loading,
        D::ComputeMode::Update,
        D::ForceUpdate::No
    );

    D::update_copy_buttons(gui);
}

void VNC::FileTransferGui::client_cb_file_list_set_nb_item(uint32_t nb_file)
{
    auto & gui = this->gui();

    if (!is_open())
    {
        gui.cb.next_display_state = D::CbState::AddItem;
        return ;
    }

    auto force_update = D::update_delay(*this, InOutParam{gui.cb.last_time_of_update_nb_file});

    gui.cb.nb_file = nb_file;

    D::draw_cb_part(
        gui,
        gui.layout.cb_rdp.inner_rect,
        D::CbState::AddItem,
        D::ComputeMode::Update,
        force_update
    );
}

void VNC::FileTransferGui::client_cb_file_list_end()
{
    auto & gui = this->gui();
    D::draw_cb_part(
        gui,
        gui.layout.cb_rdp.inner_rect,
        D::CbState::Ready,
        D::ComputeMode::Update,
        D::ForceUpdate::No
    );

    D::update_copy_buttons(gui);
}

void VNC::FileTransferGui::vnc_to_rdp_file_list_start()
{
    auto & gui = this->gui();

    D::draw_cb_part(
        gui,
        gui.layout.cb_rdp.inner_rect,
        D::CbState::PopulatedByServerLoading,
        D::ComputeMode::Update,
        D::ForceUpdate::No
    );

    D::update_copy_buttons(gui);
}

void VNC::FileTransferGui::vnc_to_rdp_file_list_ready()
{
    auto & gui = this->gui();

    D::draw_cb_part(
        gui,
        gui.layout.cb_rdp.inner_rect,
        D::CbState::PopulatedByServerReady,
        D::ComputeMode::Update,
        D::ForceUpdate::No
    );

    D::update_copy_buttons(gui);
}

void VNC::FileTransferGui::transfer_start(Direction direction, TransferData total)
{
    auto & gui = this->gui();
    auto now = m_event_guard.get_monotonic_time();
    gui.progress = D::Progress
    {
        .show_progress = true,
        .enable_eta = true,
        .direction = direction,
        .msg = (direction == Direction::VncToCb)
            ? D::TextId::transfer_to_cb_in_progress
            : D::TextId::transfer_to_vnc_in_progress,
        .msg_color = D::colors.panel.fg,
        .previous_width_msg = 0,
        .transferred_items_previous_left_width = 0,
        .transferred_items_previous_right_width = 0,
        .transferred_bytes_previous_left_width = 0,
        .transferred_bytes_previous_right_width = 0,
        .error_previous_width = 0,
        .error_unit_previous_width = 0,
        .path_error_len = 0,
        .path_error_width = 0,
        .path_error_previous_width = 0,
        .percent_progression = 0,
        .eta_progression_previous_width = 0,
        .percent_progression_previous_width = 0,
        .nb_copied = 0,
        .old_nb_copied = 0,
        .total_items = total.items,
        .transferred_bytes = 0,
        .old_transferred_bytes = 0,
        .total_bytes = total.bytes,
        .last_time_of_add_progression = now,
        .eta_duration = Eta::Duration::max(),
        .progression_eta {now},
    };

    D::draw_progress_part(gui, gui.layout.cb_rdp.inner_rect);

    // disable copy buttons and enable stop transfer button
    gui.disabled_elements.remove(D::ElementId::StopTransferButton);
    D::DrawCtx{gui, gui.layout.mid.rect}.draw_stop_transfer_button();
    D::update_copy_buttons(gui);
}

void VNC::FileTransferGui::transfer_progression(Progression progression)
{
    auto & gui = this->gui();

    gui.progress.nb_copied += progression.items;
    gui.progress.transferred_bytes += progression.bytes;

    /*
     * Check refresh
     */

    auto force_update = D::update_delay(*this, InOutParam{gui.progress.last_time_of_add_progression});

    if (progression.state == Progression::State::InProgress
     && progression.total_bytes_adjust == 0
     && force_update == D::ForceUpdate::No
    )
    {
        return ;
    }

    gui.progress.enable_eta = (progression.state == Progression::State::InProgress);

    /*
     * Update progression
     */

    D::DrawCtx ctx{gui, gui.layout.cb_rdp.inner_rect};

    gui.progress.total_bytes = static_cast<uint64_t>(
        static_cast<int64_t>(gui.progress.total_bytes)
        + progression.total_bytes_adjust
    );

    gui.progress.progression_eta.update({
        .time = gui.progress.last_time_of_add_progression,
        .value = gui.progress.transferred_bytes,
    });

    auto eta = (gui.progress.msg == D::TextId::transfer_to_cb_in_progress
                || gui.progress.msg == D::TextId::transfer_to_vnc_in_progress)
        ? gui.progress.progression_eta.compute_eta(gui.progress.total_bytes)
        : Eta::Duration::max();

    bool update_eta = (gui.progress.eta_duration != eta);
    bool update_items = (gui.progress.old_nb_copied != gui.progress.nb_copied);
    bool update_bytes = (gui.progress.old_transferred_bytes != gui.progress.transferred_bytes
                            || progression.total_bytes_adjust);

    // compute percentage
    auto nb = gui.progress.nb_copied + gui.progress.transferred_bytes;
    auto total = gui.progress.total_items + gui.progress.total_bytes;
    uint8_t percent = checked_int{nb * 100 / total};
    bool update_percent = (gui.progress.percent_progression != percent);

    gui.progress.old_nb_copied = gui.progress.nb_copied;

    if (update_percent)
    {
        gui.progress.percent_progression = percent;
        D::draw_progress_percent(ctx);
    }
    if (update_eta || !gui.progress.enable_eta)
    {
        gui.progress.eta_duration = eta;
        D::draw_progress_eta(ctx);
    }
    if (update_items)
    {
        gui.progress.old_nb_copied = gui.progress.nb_copied;
        D::draw_progress_items(ctx, D::RedrawTotal::No);
    }
    if (update_bytes)
    {
        gui.progress.old_transferred_bytes = gui.progress.transferred_bytes;
        auto redraw = progression.total_bytes_adjust ? D::RedrawTotal::Yes : D::RedrawTotal::No;
        D::draw_progress_bytes(ctx, redraw);
    }

    /*
     * Update status message
     */

    switch (progression.state)
    {
        case Progression::State::InProgress:
            // no status -> exit
            return;

        case Progression::State::Completed:
            gui.progress.msg = (gui.progress.direction == Direction::VncToCb)
                ? D::TextId::transfer_to_cb_completed
                : D::TextId::transfer_to_vnc_completed;
            gui.progress.msg_color = D::colors.panel.fg_ok;
            break;

        case Progression::State::Aborted:
            gui.progress.msg = (gui.progress.direction == Direction::VncToCb)
                ? D::TextId::transfer_to_cb_aborted
                : D::TextId::transfer_to_vnc_aborted;
            gui.progress.msg_color = D::colors.panel.fg_aborted;
            break;

        case Progression::State::Error:
            gui.progress.msg = (gui.progress.direction == Direction::VncToCb)
                ? D::TextId::transfer_to_cb_error
                : D::TextId::transfer_to_vnc_error;
            gui.progress.msg_color = D::colors.panel.fg_error;

            if (!progression.path_error.empty())
            {
                constexpr auto id = D::TextId::transfer_file_error;
                auto path = progression.path_error.native();
                auto * fcs_ptr = D::fcs_data(gui, id);
                auto * out = fcs_ptr + gui.fcs_lengths[id];
                auto [fcs, text_width] = D::fcs_init(out, gui.font, path);

                gui.progress.path_error_len = checked_int{ fcs.end() - fcs_ptr };
                gui.progress.path_error_width = checked_int{ text_width + gui.text_widths[id] - 1 };
            }
            break;
    }

    D::draw_progress_status(ctx);

    // disable stop transfer button and enable copy buttons
    gui.disabled_elements.add(D::ElementId::StopTransferButton);
    D::DrawCtx{gui, gui.layout.mid.rect}.draw_stop_transfer_button();
    D::update_copy_buttons(gui);
}

void VNC::FileTransferGui::input_mouse(
    uint16_t device_flags, uint16_t x, uint16_t y, kbdtypes::KeyModFlags mods)
{
    constexpr auto down = MOUSE_FLAG_BUTTON1 | MOUSE_FLAG_DOWN;
    constexpr auto up = MOUSE_FLAG_BUTTON1;

    auto & gui = this->gui();

    using ElementId = D::ElementId;

    if (device_flags != MOUSE_FLAG_MOVE)
    {
        gui.vnc.vim_mode.reset_move();

        /*
         * Ignore the events other than left click up/down
         */

        if (device_flags != down
         && (device_flags != up || gui.pressed_item == ElementId::None))
        {
            return ;
        }
    }

    /*
     * Search element
     */

    Rect vnc_list_rect = D::get_vnc_list_rect(gui);

    auto elem = D::get_elem_bellow_mouse(gui, vnc_list_rect, x, y);
    auto old_focus_item = ElementId::None;

    /*
     * Cursor appareance
     */

    if (device_flags == MOUSE_FLAG_MOVE)
    {
        D::update_mouse_pointer(gui, elem, x, y);
        return ;
    }

    LOG(LOG_DEBUG, "elem=%d | disable=%d", elem, gui.disabled_elements.has(elem));

    /*
     * Apply mouse event on selected event
     */

    if (device_flags == up)
    {
        auto pressed_item = gui.pressed_item;
        gui.pressed_item = ElementId::None;

        LOG(LOG_DEBUG, "up | %d", !gui.disabled_elements.has(pressed_item));

        D::event_mouse_release(
            *this,
            gui,
            {device_flags, x, y},
            pressed_item,
            elem,
            vnc_list_rect,
            mods
        );
    }
    // device_flags == down
    else if (!gui.disabled_elements.has(elem))
    {
        LOG(LOG_DEBUG, "down | 1");

        old_focus_item = gui.focus_item;
        gui.focus_item = elem;
        gui.pressed_item = elem;

        D::event_press_or_focus(
            *this,
            gui,
            D::InputEvent::MouseEvent,
            {device_flags, x, y},
            old_focus_item,
            elem,
            vnc_list_rect,
            mods
        );
    }

    if (old_focus_item != elem)
    {
        D::event_blur(gui, old_focus_item);
    }
}

void VNC::FileTransferGui::input_mouse_ex(uint16_t pointer_flag, uint16_t /*x*/, uint16_t /*y*/)
{
    // is pressed
    if (pointer_flag & 0x8000)
    {
        auto & gui = this->gui();

        if (gui.vnc.files.size() <= 1)
        {
            return;
        }

        auto cycle = WidgetPagination::Cycle::No;
        auto update_event = WidgetPagination::TriggerUpdatePageEvent::Yes;

        auto pointer = (pointer_flag & 0b11);

        if (pointer == 2)
        {
            gui.vnc.pagination.next_page(cycle, update_event);
        }
        else if (pointer == 1)
        {
            gui.vnc.pagination.prev_page(cycle, update_event);
        }
    }
}

void VNC::FileTransferGui::input_scancode(
    kbdtypes::KbdFlags flags, kbdtypes::Scancode scancode, Keymap const & keymap)
{
    using namespace kbdtypes;
    auto & gui = this->gui();

    using ElementId = D::ElementId;

    /*
     * Focus management with tab
     */

    if (scancode == Scancode::Tab) [[unlikely]]
    {
        auto vim_nav = gui.vnc.vim_mode.nav;
        gui.vnc.vim_mode.reset_all();

        if (is_pressed(flags))
        {
            D::focus_by_tab_key(*this, gui, vim_nav, keymap.mods());
        }
    }

    /*
     * Close gui
     */

    else if (is_released(flags) && scancode == Scancode::Esc)
    {
        if (gui.vnc.vim_mode.is_empty())
        {
            // TODO confirm when transfer
            m_callbacks.close_gui(m_callbacks.ctx);
        }
        else
        {
            gui.vnc.vim_mode.reset_all();
        }
    }

    /*
     * Button validation and dispatch key event
     */

    else
    {
        if (gui.disabled_elements.has(gui.focus_item))
        {
            return ;
        }

        switch (gui.focus_item)
        {
            case ElementId::None:
            case ElementId::GuiClose:
                break;

            case ElementId::VncRootButton:
            case ElementId::VncParentButton:
            case ElementId::VncIconSortFilename:
            case ElementId::VncIconSortSize:
            case ElementId::VncIconSortDate:
            case ElementId::VncListAllCheckbox:
            case ElementId::ToRdpButton:
            case ElementId::ToVncButton:
            case ElementId::StopTransferButton:
            case ElementId::VncList:
                if (is_pressed(flags))
                {
                    gui.pressed_item = gui.focus_item;
                    auto uc = D::kbd_to_vim_shortcut(
                        keymap,
                        OutParam{gui.vnc.vim_mode.nav.number_state}
                    );
                    D::selector_process_pressed(*this, gui, uc, keymap.mods());
                }
                // is_released
                else if (gui.focus_item != ElementId::VncList)
                {
                    auto pressed_item = gui.pressed_item;
                    gui.pressed_item = ElementId::None;

                    if (scancode == Scancode::Enter || scancode == Scancode::Space)
                    {
                        // button keyboard event same as mouse event
                        D::event_mouse_release(
                            *this,
                            gui,
                            D::MouseEventData{},
                            pressed_item,
                            gui.focus_item,
                            D::get_vnc_list_rect(gui),
                            keymap.mods()
                        );
                    }
                }
                break;

            case ElementId::VncEditField:
                gui.vnc.directory_edit.rdp_input_scancode(flags, scancode, 0, keymap);
                break;

            case ElementId::VncNavigation: {
                gui.pressed_item = is_pressed(flags) ? gui.focus_item : ElementId::None;

                uint32_t uc = 0;

                // propagate to widget for digits, space and enter
                if (scancode != Scancode::Enter && scancode != Scancode::Space)
                {
                    uc = D::kbd_to_vim_shortcut(
                        keymap,
                        OutParam{gui.vnc.vim_mode.nav.number_state}
                    );

                    if (uc <= '9' && '0' <= uc
                     && gui.vnc.pagination.get_focus_elem() == WidgetPagination::FocusElement::Edit)
                    {
                        gui.vnc.vim_mode.reset_move();
                        uc = 0;
                    }
                }

                if (uc && is_pressed(flags))
                {
                    D::selector_process_pressed(*this, gui, uc, keymap.mods());
                }
                else
                {
                    gui.vnc.pagination.rdp_input_scancode(flags, scancode, 0, keymap);
                }

                break;
            }
        }
    }
}

void VNC::FileTransferGui::input_unicode(
    kbdtypes::KbdFlags flags, uint32_t unicode, kbdtypes::KeyModFlags mods)
{
    auto & gui = this->gui();

    using ElementId = D::ElementId;

    if (gui.disabled_elements.has(gui.focus_item))
    {
        return ;
    }

    switch (gui.focus_item)
    {
        case ElementId::None:
        case ElementId::GuiClose:
        case ElementId::VncRootButton:
        case ElementId::VncParentButton:
        case ElementId::VncIconSortFilename:
        case ElementId::VncIconSortSize:
        case ElementId::VncIconSortDate:
        case ElementId::VncListAllCheckbox:
        case ElementId::ToRdpButton:
        case ElementId::ToVncButton:
        case ElementId::StopTransferButton:
        case ElementId::VncList:
            if (is_pressed(flags))
            {
                gui.pressed_item = gui.focus_item;
                D::selector_process_pressed(*this, gui, unicode, mods);
            }
            // is_released
            else if (gui.focus_item != ElementId::VncList)
            {
                auto pressed_item = gui.pressed_item;
                gui.pressed_item = ElementId::None;

                if (unicode == ' ')
                {
                    // button keyboard event same as mouse event
                    D::event_mouse_release(
                        *this,
                        gui,
                        D::MouseEventData{},
                        pressed_item,
                        gui.focus_item,
                        D::get_vnc_list_rect(gui),
                        mods
                    );
                }
            }
            break;

        case ElementId::VncEditField:
            gui.vnc.directory_edit.rdp_input_unicode(flags, unicode);
            break;

        case ElementId::VncNavigation: {
            gui.pressed_item = is_pressed(flags) ? gui.focus_item : ElementId::None;

            // propagate to widget for digits and space
            if (unicode == ' '
             || (unicode <= '9' && '0' <= unicode
                && gui.vnc.pagination.get_focus_elem() == WidgetPagination::FocusElement::Edit))
            {
                gui.vnc.vim_mode.reset_move();
                gui.vnc.pagination.rdp_input_unicode(flags, unicode);
            }
            else if (is_pressed(flags))
            {
                D::selector_process_pressed(*this, gui, unicode, mods);
            }

            break;
        }
    }
}

void VNC::FileTransferGui::refresh(Rect clip)
{
    auto & gui = this->gui();
    D::draw_ui(gui, clip, D::ComputeMode::Refresh, gui.cb.display_state);
}

VNC::FileTransferGui::SelectedVncFiles::Iterator &
VNC::FileTransferGui::SelectedVncFiles::Iterator::operator++() noexcept
{
    for (++m_begin; m_begin != m_end; ++m_begin)
    {
        if (m_begin->checked)
        {
            return *this;
        }
    }

    return *this;
}

VNC::UVncFile VNC::FileTransferGui::SelectedVncFiles::Iterator::operator*() const noexcept
{
    return {
        .file_name = m_begin->file_name_av(),
        .file_size = m_begin->file_size,
        .last_access_time = m_begin->last_access_time,
        .is_dir = !m_begin->is_file(),
    };
}
