/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "test_only/test_framework/redemption_unit_tests.hpp"
#include "test_only/test_framework/check_img.hpp"
#include "test_only/gdi/test_graphic.hpp"
#include "test_only/core/font.hpp"

#include "core/events.hpp"
#include "keyboard/keymap.hpp"
#include "keyboard/keylayouts.hpp"
#include "mod/vnc/file_transfer/file_transfer_gui.hpp"
#include "utils/sugar/int_to_chars.hpp"

#include <chrono>


#define IMG_TEST_PATH FIXTURES_PATH "/img_ref/mod/vnc/file_transfer_gui/"


inline std::string selected_files_to_string(VNC::FileTransferGui::SelectedVncFiles files)
{
    std::string s;

    s += '(';
    s += int_to_decimal_chars(files.size());
    s += ")\n";

    unsigned counter = 0;

    for (auto file : files)
    {
        auto path = file.file_name.native();
        s += "{.file_name=\"";
        s.append(path.as_charp(), path.size());
        s += "\", .file_size=";
        s += int_to_decimal_chars(file.file_size).sv();
        s += ", .last_access_time=";
        s += int_to_decimal_chars(underlying_cast(file.last_access_time)).sv();
        s += ", .is_dir=";
        s += file.is_dir ? '1' : '0';
        s += "}\n";
        ++counter;
    }

    s += "(total=";
    s += int_to_decimal_chars(counter);
    s += ')';

    return s;
}


RED_AUTO_TEST_CASE(TestFileTransferGui)
{
    using namespace std::chrono_literals;
    using namespace kbdtypes;

    // init
    //@{

    struct Ctx
    {
        int nb_call = 0;
    };
    Ctx ctx;

    constexpr uint16_t width = 1200;
    constexpr uint16_t height = 600;
    constexpr Rect clip {0, 0, width, height};
    constexpr Rect partial_clips[] {
        // chuck of cb part with title
        {23, 10, 304, 116},
        // chuck of progress part
        {80, 180, 200, 200},
        // vnc location dir
        {650, 62, 400, 15},
        // chuck of vnc part
        {698, 130, 304, 45},
        // chuck of copy buttons
        {570, 130, 55, 80},
        // chuck of stop button + chuck of cb part + chuck of vnc part
        {400, 244, 555, 45},
        // chuck count items
        {667, 568, 100, 20},
        // chuck of nav bar
        {852, 562, 280, 20},
    };

    struct Coord
    {
        uint16_t x;
        uint16_t y;
    };

    constexpr Coord coord_close_gui { width - 10, 5 };
    constexpr Coord coord_to_vnc_btn { 568, 150 };
    constexpr Coord coord_to_rdp_btn { 568, 190 };
    constexpr Coord coord_stop_btn { 568, 240 };
    constexpr Coord coord_vnc_sort_name { 925, 95 };
    constexpr Coord coord_vnc_sort_size { 1003, 95 };
    constexpr Coord coord_vnc_sort_date { 1128, 95 };
    constexpr Coord coord_vnc_root_btn { 674, 66 };
    constexpr Coord coord_vnc_parent_btn { 702, 66 };
    constexpr Coord coord_vnc_all_file { 664, 95 };
    constexpr Coord coord_vnc_pagination_prev { 1031, 580 };
    constexpr Coord coord_vnc_pagination_next { 1144, 580 };
    auto coord_vnc_file = [](int n){ return Coord{760, checked_int{ 115 + 22 * n }}; };
    auto coord_vnc_file_checkbox = [](int n){ return Coord{664, checked_int{ 115 + 22 * n }}; };

    auto called = [](void*, auto...){
    };

    #define SHOULD_BE_NOT_CALLED(fn, ...) .fn \
        = [](void*, auto...){                 \
            RED_CHECK(!"unexpected callback: " #fn[0]); return __VA_ARGS__; }

    VNC::FileTransferGui::Callbacks callbacks {
        .ctx = &ctx,
        SHOULD_BE_NOT_CALLED(close_gui),
        SHOULD_BE_NOT_CALLED(open_dir, false),
        SHOULD_BE_NOT_CALLED(copy_cb_to_vnc),
        SHOULD_BE_NOT_CALLED(copy_vnc_to_cb),
        SHOULD_BE_NOT_CALLED(stop_transfer),
    };
    auto callbacks_unset = callbacks;

    #undef SHOULD_BE_NOT_CALLED

    TestGraphic gd{width, height};
    EventManager event_manager;

    #define DISPATCH_FN(fn) .fn = [](void* ctx, auto... xs) {            \
        auto * cbs = static_cast<VNC::FileTransferGui::Callbacks*>(ctx); \
        static_cast<Ctx*>(cbs->ctx)->nb_call++;                          \
        return cbs->fn(cbs->ctx, xs...);                                 \
    }

    VNC::FileTransferGui ft_gui {
        gd,
        global_font_deja_vu_14(),
        event_manager.get_events(),
        100,
        VNC::FileTransferGui::TransferOptions{}
        | VNC::FileTransferGui::TransferOptions::CbToVnc
        | VNC::FileTransferGui::TransferOptions::VncToCb,
        MsgTranslationCatalog::default_catalog(),
        VNC::FileTransferGui::Callbacks {
            .ctx = &callbacks,
            DISPATCH_FN(close_gui),
            DISPATCH_FN(open_dir),
            DISPATCH_FN(copy_cb_to_vnc),
            DISPATCH_FN(copy_vnc_to_cb),
            DISPATCH_FN(stop_transfer),
        },
    };

    #undef DISPATCH_FN

    using Path = VNC::UVncFile::PathView;

    auto input_mouse = [&](uint16_t flags, Coord coord, KeyModFlags mods = {}) {
        ft_gui.input_mouse(flags, coord.x, coord.y, mods);
    };


    // #define LINE_MARKER "  (line " RED_PP_STRINGIFY(__LINE__) ")"

    #define TEST_EVENT(name, body, cb_name, cb_fn) do {  \
        RED_TEST_CONTEXT(name " | callback: " #cb_name)  \
        {                                                \
            ctx.nb_call = 0;                             \
            callbacks.cb_name = cb_fn;                   \
            body                                         \
            RED_CHECK(ctx.nb_call == 1);                 \
            callbacks.cb_name = callbacks_unset.cb_name; \
        }                                                \
    } while (0)

    #define TEST_NO_EVENT(name, body) do { \
        RED_TEST_CONTEXT(name)             \
        {                                  \
            ctx.nb_call = 0;               \
            body                           \
            RED_CHECK(ctx.nb_call == 0);   \
        }                                  \
    } while (0)

    #define TEST_MOUSE(flags, coord, cb_name, cb_fn) \
        TEST_EVENT(#flags " | elem: " #coord, input_mouse(flags, coord);, cb_name, cb_fn)

    #define TEST_MOUSE_NO_EVENT(flags, coord) \
        TEST_NO_EVENT(#flags " | elem: " #coord, input_mouse(flags, coord);)

    #define TEST_COPY_CB_TO_VNC_LIST(files) do {                          \
        input_mouse(left_mouse_down, coord_to_rdp_btn);                   \
        TEST_MOUSE(left_mouse_up, coord_to_rdp_btn, copy_vnc_to_cb, [](   \
            void *, VNC::FileTransferGui::SelectedVncFiles selected_files \
        ){                                                                \
            RED_CHECK(selected_files_to_string(selected_files) == files); \
        });                                                               \
    } while (0)


    constexpr auto left_mouse_down = 0x1000 | 0x8000;
    constexpr auto left_mouse_up   = 0x1000;


    auto draw_partial = [&]{
        gd.draw_rect(clip, NamedBGRColor::YELLOW);
        for (auto rect : partial_clips)
        {
            ft_gui.refresh(rect);
        }
    };

    #define CHECK_IMG(png_name) RED_CHECK_IMG(gd, IMG_TEST_PATH png_name)

    #define CHECK_IMG_AND_PARTIAL(png_name) do { \
        CHECK_IMG(png_name);                     \
        draw_partial();                          \
        CHECK_IMG(png_name ".partial.png");      \
        ft_gui.refresh(clip);                    \
        CHECK_IMG(png_name);                     \
    } while (0)

    //@}


    ft_gui.open(width, height, 0, 0);
    CHECK_IMG_AND_PARTIAL("empty.png");


    RED_TEST_CONTEXT("Draw clipboard part")
    {
        ft_gui.client_cb_file_list_reset();
        CHECK_IMG_AND_PARTIAL("cb1_reset.png");

        ft_gui.client_cb_file_list_requested();
        CHECK_IMG_AND_PARTIAL("cb2_requested.png");

        ft_gui.client_cb_file_list_start(20);
        CHECK_IMG_AND_PARTIAL("cb3_start.png");

        ft_gui.client_cb_file_list_set_nb_item(2);
        CHECK_IMG_AND_PARTIAL("cb4_set_nb_item_2.png");

        ft_gui.client_cb_file_list_set_nb_item(5);
        // unchanged, too short delay
        CHECK_IMG_AND_PARTIAL("cb4_set_nb_item_2.png");

        event_manager.get_writable_time_base().monotonic_time += 2s;
        ft_gui.client_cb_file_list_set_nb_item(10);
        CHECK_IMG_AND_PARTIAL("cb5_set_nb_item_10.png");

        ft_gui.client_cb_file_list_end();
        CHECK_IMG_AND_PARTIAL("cb6_end.png");

        ft_gui.close();
        gd.draw_rect(clip, BGRColor::from_rgb(0));
        // open and redraw
        ft_gui.open(width, height, 0, 0);
        CHECK_IMG_AND_PARTIAL("cb6_end.png");

        ft_gui.client_cb_file_list_reset();
        CHECK_IMG_AND_PARTIAL("cb1_reset.png");
    }

    RED_TEST_CONTEXT("Draw vnc part")
    {
        ft_gui.server_vnc_file_list_start(Path{""_sized_av}); // root
        CHECK_IMG_AND_PARTIAL("vnc_root_list_start.png");
        ft_gui.server_vnc_file_list_add_drive('C', VNC::FileTransferGui::DriveType::LocalDisk);
        ft_gui.server_vnc_file_list_add_drive('D', VNC::FileTransferGui::DriveType::MediaDisk);
        ft_gui.server_vnc_file_list_add_drive('E', VNC::FileTransferGui::DriveType::CDRom);
        ft_gui.server_vnc_file_list_add_drive('F', VNC::FileTransferGui::DriveType::NetworkDisk);
        // with bad type
        ft_gui.server_vnc_file_list_add_drive('G', VNC::FileTransferGui::DriveType(0xff));
        ft_gui.server_vnc_file_list_add_shorcuts();
        ft_gui.server_vnc_file_list_end();
        CHECK_IMG_AND_PARTIAL("vnc_root_list.png");

        RED_TEST_CONTEXT("sort disk")
        {
            RED_TEST_CONTEXT("sort by name (reverse)")
            {
                TEST_MOUSE_NO_EVENT(left_mouse_down, coord_vnc_sort_name);
                CHECK_IMG("vnc_root_sort_name_ascending.png");
                TEST_MOUSE_NO_EVENT(left_mouse_up, coord_vnc_sort_name);
                CHECK_IMG("vnc_root_sort_name_descending.png");

            }

            RED_TEST_CONTEXT("sort by size")
            {
                TEST_MOUSE_NO_EVENT(left_mouse_down, coord_vnc_sort_size);
                CHECK_IMG("vnc_root_sort_name_descending_focus_size.png");
                TEST_MOUSE_NO_EVENT(left_mouse_up, coord_vnc_sort_size);
                CHECK_IMG("vnc_root_sort_size_ascending.png");

            }

            RED_TEST_CONTEXT("sort by size (reverse)")
            {
                TEST_MOUSE_NO_EVENT(left_mouse_down, coord_vnc_sort_size);
                CHECK_IMG("vnc_root_sort_size_ascending.png");
                TEST_MOUSE_NO_EVENT(left_mouse_up, coord_vnc_sort_size);
                CHECK_IMG("vnc_root_sort_size_descending.png");

            }

            RED_TEST_CONTEXT("sort by date")
            {
                TEST_MOUSE_NO_EVENT(left_mouse_down, coord_vnc_sort_date);
                CHECK_IMG("vnc_root_sort_size_descending_focus_date.png");
                TEST_MOUSE_NO_EVENT(left_mouse_up, coord_vnc_sort_date);
                CHECK_IMG("vnc_root_sort_date_ascending.png");

            }

            RED_TEST_CONTEXT("sort by date (reverse)")
            {
                TEST_MOUSE_NO_EVENT(left_mouse_down, coord_vnc_sort_date);
                CHECK_IMG("vnc_root_sort_date_ascending.png");
                TEST_MOUSE_NO_EVENT(left_mouse_up, coord_vnc_sort_date);
                CHECK_IMG("vnc_root_sort_date_descending.png");

            }

            RED_TEST_CONTEXT("sort by name (2)")
            {
                TEST_MOUSE_NO_EVENT(left_mouse_down, coord_vnc_sort_name);
                CHECK_IMG("vnc_root_sort_date_descending_focus_name.png");
                TEST_MOUSE_NO_EVENT(left_mouse_up, coord_vnc_sort_name);
                CHECK_IMG("vnc_root_sort_name_ascending.png");
            }
        }

        ft_gui.server_vnc_file_list_start(Path{"a dir"_sized_av});
        CHECK_IMG_AND_PARTIAL("vnc_file_list_start_a_dir_unfocus_sort_name.png");

        ft_gui.server_vnc_file_list_add({
            .file_name { WinNtPathView { "a file name"_sized_av } },
            .file_size = 1032373,
            .last_access_time = WinNtUTime{13379901460'0000000},
            .is_dir = false,
        });
        ft_gui.server_vnc_file_list_add({
            .file_name { WinNtPathView { "a dir name"_sized_av } },
            .file_size = 0,
            .last_access_time = WinNtUTime{13389803460'0000000},
            .is_dir = true,
        });
        ft_gui.server_vnc_file_list_add({
            .file_name { WinNtPathView { "a second dir name"_sized_av } },
            .file_size = 0,
            .last_access_time = WinNtUTime{13389902460'0000000},
            .is_dir = true,
        });
        ft_gui.server_vnc_file_list_add({
            .file_name { WinNtPathView { "a second file name"_sized_av } },
            .file_size = 653,
            .last_access_time = WinNtUTime{13199902683'0000000},
            .is_dir = false,
        });
        ft_gui.server_vnc_file_list_end(); // take focus
        CHECK_IMG_AND_PARTIAL("vnc_file_list.png");

        TEST_MOUSE_NO_EVENT(left_mouse_down, coord_vnc_file(0));
        CHECK_IMG("vnc_file_list.png");
        TEST_MOUSE_NO_EVENT(left_mouse_up, coord_vnc_file(0));
        CHECK_IMG("vnc_file_list.png");


        ft_gui.server_vnc_file_list_error();
        CHECK_IMG_AND_PARTIAL("vnc_file_list_error.png");


        ft_gui.server_vnc_file_disabled();
        CHECK_IMG_AND_PARTIAL("vnc_file_list_disabled.png");


        ft_gui.server_vnc_file_list_start(Path{"a dir"_sized_av});
        CHECK_IMG_AND_PARTIAL("vnc_file_list_start_a_dir_unfocus_sort_name.png");
        ft_gui.server_vnc_file_list_end();
        CHECK_IMG_AND_PARTIAL("vnc_file_list_a_dir_empty.png");


        ft_gui.server_vnc_file_list_start(Path{ "C:\\sub\\dir"_sized_av});
        CHECK_IMG_AND_PARTIAL("vnc_file_list_start_an_another_dir.png");

        int file_counter = 0;
        int dir_counter = 0;
        char dirname[7] = "a dir ";
        char filename[8] = "a file ";

        for (unsigned i = 0; i < 26; ++i)
        {
            bool is_dir = (i % 5 == 1);
            if (is_dir)
            {
                utils::back(dirname) = static_cast<char>('A' + dir_counter);
                ++dir_counter;
            }
            else
            {
                utils::back(filename) = static_cast<char>('A' + file_counter);
                ++file_counter;
            }
            unsigned j = (i > 0) ? ((i & 1) ? i + 1 : i - 1) : 0;
            auto mul = j < 10 ? 1 : j + 64;
            ft_gui.server_vnc_file_list_add({
                .file_name = is_dir
                    ? Path{make_sized_array_view(dirname)}
                    : Path{make_sized_array_view(filename)},
                .file_size = 242 + 158 * j * j * j * j * mul,
                .last_access_time = WinNtUTime{
                    (13389701460u - (i % 3) * i - (i % 5) * i * i) * 10000000u
                },
                .is_dir = is_dir,
            });
        }
        ft_gui.server_vnc_file_list_end();
        CHECK_IMG_AND_PARTIAL("vnc_file_list2.png");

        RED_TEST_CONTEXT("sort files")
        {
            RED_TEST_CONTEXT("sort by name (reverse)")
            {
                TEST_MOUSE_NO_EVENT(left_mouse_down, coord_vnc_sort_name);
                CHECK_IMG("vnc_list_sort_name_ascending.png");
                TEST_MOUSE_NO_EVENT(left_mouse_up, coord_vnc_sort_name);
                CHECK_IMG("vnc_list_sort_name_descending.png");
            }

            RED_TEST_CONTEXT("sort by size")
            {
                TEST_MOUSE_NO_EVENT(left_mouse_down, coord_vnc_sort_size);
                CHECK_IMG("vnc_list_sort_name_descending_focus_size.png");
                TEST_MOUSE_NO_EVENT(left_mouse_up, coord_vnc_sort_size);
                CHECK_IMG("vnc_list_sort_size_ascending.png");
            }

            RED_TEST_CONTEXT("sort by size (reverse)")
            {
                TEST_MOUSE_NO_EVENT(left_mouse_down, coord_vnc_sort_size);
                CHECK_IMG("vnc_list_sort_size_ascending.png");
                TEST_MOUSE_NO_EVENT(left_mouse_up, coord_vnc_sort_size);
                CHECK_IMG("vnc_list_sort_size_descending.png");
            }

            RED_TEST_CONTEXT("sort by date")
            {
                TEST_MOUSE_NO_EVENT(left_mouse_down, coord_vnc_sort_date);
                CHECK_IMG("vnc_list_sort_size_descending_focus_date.png");
                TEST_MOUSE_NO_EVENT(left_mouse_up, coord_vnc_sort_date);
                CHECK_IMG("vnc_list_sort_date_ascending.png");
            }

            RED_TEST_CONTEXT("sort by date (reverse)")
            {
                TEST_MOUSE_NO_EVENT(left_mouse_down, coord_vnc_sort_date);
                CHECK_IMG("vnc_list_sort_date_ascending.png");
                TEST_MOUSE_NO_EVENT(left_mouse_up, coord_vnc_sort_date);
                CHECK_IMG("vnc_list_sort_date_descending.png");
            }

            RED_TEST_CONTEXT("sort by name (2)")
            {
                TEST_MOUSE_NO_EVENT(left_mouse_down, coord_vnc_sort_name);
                CHECK_IMG("vnc_list_sort_date_descending_focus_name.png");
                TEST_MOUSE_NO_EVENT(left_mouse_up, coord_vnc_sort_name);
                CHECK_IMG("vnc_list_sort_name_ascending.png");
            }
        }
    }

    RED_TEST_CONTEXT("Activate 'copy to vnc' button")
    {
        ft_gui.client_cb_file_list_start(10000);
        ft_gui.client_cb_file_list_set_nb_item(10000);
        ft_gui.client_cb_file_list_end();
        CHECK_IMG_AND_PARTIAL("vnc_file_list_and_cb_item.png");
    }

    RED_TEST_CONTEXT("Click on 'copy to vnc' button")
    {
        input_mouse(left_mouse_down, coord_to_vnc_btn);
        CHECK_IMG("copy_to_vnc_active.png");

        TEST_MOUSE(left_mouse_up, coord_to_vnc_btn, copy_cb_to_vnc, called);
        CHECK_IMG("copy_to_vnc_focus.png");
    }

    RED_TEST_CONTEXT("Click on 'vnc all file'")
    {
        input_mouse(left_mouse_down, coord_vnc_all_file);
        CHECK_IMG("vnc_file_all_unchecked_active.png");

        TEST_MOUSE_NO_EVENT(left_mouse_up, coord_vnc_all_file);
        CHECK_IMG_AND_PARTIAL("vnc_file_all_checked_focus.png");
    }

    RED_TEST_CONTEXT("Click on 'checkbox 1' (3 times)")
    {
        input_mouse(left_mouse_down, coord_vnc_file_checkbox(1));
        CHECK_IMG("vnc_list_file_1_focus.png");

        TEST_MOUSE_NO_EVENT(left_mouse_up, coord_vnc_file_checkbox(1));
        CHECK_IMG("vnc_list_file_1_focus_unchecked.png");

        input_mouse(left_mouse_down, coord_vnc_file_checkbox(1));
        CHECK_IMG("vnc_list_file_1_focus_unchecked.png");

        TEST_MOUSE_NO_EVENT(left_mouse_up, coord_vnc_file_checkbox(1));
        CHECK_IMG("vnc_list_file_1_focus.png");

        input_mouse(left_mouse_down, coord_vnc_file_checkbox(1));
        CHECK_IMG("vnc_list_file_1_focus.png");

        TEST_MOUSE_NO_EVENT(left_mouse_up, coord_vnc_file_checkbox(1));
        CHECK_IMG("vnc_list_file_1_focus_unchecked.png");
    }

    RED_TEST_CONTEXT("Pagination next page")
    {
        TEST_MOUSE_NO_EVENT(left_mouse_down, coord_vnc_pagination_next);
        CHECK_IMG("gui_pagination_next_pressed.png");

        TEST_MOUSE_NO_EVENT(left_mouse_up, coord_vnc_pagination_next);
        CHECK_IMG("gui_pagination_next_released.png");
    }

    RED_TEST_CONTEXT("click on 'file 5' on second page")
    {
        input_mouse(left_mouse_down, coord_vnc_file(4));
        CHECK_IMG("vnc_list_file_5_p2_focus.png");

        input_mouse(left_mouse_up, coord_vnc_file(4));
        CHECK_IMG("vnc_list_file_5_p2_focus.png");
    }

    RED_TEST_CONTEXT("Pagination prev page")
    {
        TEST_MOUSE_NO_EVENT(left_mouse_down, coord_vnc_pagination_prev);
        CHECK_IMG("gui_pagination_prev_pressed.png");

        TEST_MOUSE_NO_EVENT(left_mouse_up, coord_vnc_pagination_prev);
        CHECK_IMG("gui_pagination_prev_released.png");
    }

    RED_TEST_CONTEXT("Range with toggle")
    {
        static constexpr auto expected_list_25_on_26 = "(25)\n"
            "{.file_name=\"a file A\", .file_size=242, .last_access_time=133897014600000000, .is_dir=0}\n"
            "{.file_name=\"a dir A\", .file_size=2770, .last_access_time=133897014580000000, .is_dir=1}\n"
            "{.file_name=\"a file B\", .file_size=400, .last_access_time=133897014480000000, .is_dir=0}\n"
            "{.file_name=\"a file C\", .file_size=40690, .last_access_time=133897014330000000, .is_dir=0}\n"
            "{.file_name=\"a file D\", .file_size=13040, .last_access_time=133897013920000000, .is_dir=0}\n"
            "{.file_name=\"a file E\", .file_size=205010, .last_access_time=133897014500000000, .is_dir=0}\n"
            "{.file_name=\"a file F\", .file_size=647410, .last_access_time=133897013550000000, .is_dir=0}\n"
            "{.file_name=\"a file G\", .file_size=379600, .last_access_time=133897012520000000, .is_dir=0}\n"
            "{.file_name=\"a file H\", .file_size=116920242, .last_access_time=133897011360000000, .is_dir=0}\n"
            "{.file_name=\"a file I\", .file_size=1036880, .last_access_time=133897014500000000, .is_dir=0}\n"
            "{.file_name=\"a dir C\", .file_size=248998130, .last_access_time=133897013170000000, .is_dir=1}\n"
            "{.file_name=\"a file J\", .file_size=173496092, .last_access_time=133897011720000000, .is_dir=0}\n"
            "{.file_name=\"a file K\", .file_size=473439026, .last_access_time=133897009400000000, .is_dir=0}\n"
            "{.file_name=\"a file L\", .file_size=347473368, .last_access_time=133897006480000000, .is_dir=0}\n"
            "{.file_name=\"a file M\", .file_size=828375282, .last_access_time=133897014600000000, .is_dir=0}\n"
            "{.file_name=\"a dir D\", .file_size=631901492, .last_access_time=133897011880000000, .is_dir=1}\n"
            "{.file_name=\"a file N\", .file_size=1360069298, .last_access_time=133897008480000000, .is_dir=0}\n"
            "{.file_name=\"a file O\", .file_size=1068902000, .last_access_time=133897004880000000, .is_dir=0}\n"
            "{.file_name=\"a file P\", .file_size=2123520242, .last_access_time=133896999970000000, .is_dir=0}\n"
            "{.file_name=\"a file Q\", .file_size=1709029836, .last_access_time=133897014200000000, .is_dir=0}\n"
            "{.file_name=\"a dir E\", .file_size=3183070770, .last_access_time=133897010190000000, .is_dir=1}\n"
            "{.file_name=\"a file R\", .file_size=2611880072, .last_access_time=133897004700000000, .is_dir=0}\n"
            "{.file_name=\"a file S\", .file_size=318046450, .last_access_time=133896998270000000, .is_dir=0}\n"
            "{.file_name=\"a file T\", .file_size=3846694628, .last_access_time=133896991560000000, .is_dir=0}\n"
            "{.file_name=\"a file U\", .file_size=2203231666, .last_access_time=133897014350000000, .is_dir=0}\n"
            "(total=25)"
            ""_av;
        TEST_COPY_CB_TO_VNC_LIST(expected_list_25_on_26);

        auto mods = KeyMod::LShift | KeyMod::LCtrl;
        input_mouse(left_mouse_down, coord_vnc_file(5), mods);
        CHECK_IMG("vnc_list_range_toggle_down.png");

        input_mouse(left_mouse_up, coord_vnc_file(5), mods);
        CHECK_IMG("vnc_list_range_toggle_up.png");

        static constexpr auto expected_list_5_on_26 = "(5)\n"
            "{.file_name=\"a dir A\", .file_size=2770, .last_access_time=133897014580000000, .is_dir=1}\n"
            "{.file_name=\"a dir C\", .file_size=248998130, .last_access_time=133897013170000000, .is_dir=1}\n"
            "{.file_name=\"a dir D\", .file_size=631901492, .last_access_time=133897011880000000, .is_dir=1}\n"
            "{.file_name=\"a dir E\", .file_size=3183070770, .last_access_time=133897010190000000, .is_dir=1}\n"
            "{.file_name=\"a file U\", .file_size=2203231666, .last_access_time=133897014350000000, .is_dir=0}\n"
            "(total=5)"
            ""_av;
        TEST_COPY_CB_TO_VNC_LIST(expected_list_5_on_26);
    }

    RED_TEST_CONTEXT("input_scancode()")
    {
        Keymap keymap(*find_layout_by_id(KeyLayout::KbdId(0x409))); // en-US

        auto kbd_event = [&](std::initializer_list<KeyCode> keys, KbdFlags down_up) {
            for (auto key : keys)
            {
                auto flags = down_up | keycode_to_kbdflags(key);
                auto scancode = keycode_to_scancode(key);
                keymap.event(flags, scancode);
                ft_gui.input_scancode(flags, scancode, keymap);
            }
        };

        #define TEST_KBD(extra_name, img, ...) do {            \
            RED_TEST_CONTEXT(extra_name " | " #__VA_ARGS__ "") \
            {                                                  \
                kbd_event({__VA_ARGS__}, KbdFlags::NoFlags);   \
                kbd_event({__VA_ARGS__}, KbdFlags::Release);   \
                CHECK_IMG(img);                                \
            }                                                  \
        } while (0)

        TEST_KBD(
            "to B",
            "vnc_list_kbd_to_b_down_1.png",
            KeyCode::DownArrow
        );

        TEST_KBD(
            "check B and C",
            "vnc_list_kbd_to_c_1.png",
            KeyCode::LShift,
            KeyCode::DownArrow
        );

        TEST_KBD(
            "check D",
            "vnc_list_kbd_to_d_1.png",
            KeyCode::LShift,
            KeyCode::DownArrow
        );

        TEST_KBD(
            "uncheck D",
            "vnc_list_kbd_d_2.png",
            KeyCode::Space
        );

        TEST_KBD(
            "to J (number = 6)",
            "vnc_list_kbd_to_j_1.png",
            KeyCode::Digit6,
            KeyCode::DownArrow
        );

        TEST_KBD(
            "check J, K, L (number = 2)",
            "vnc_list_kbd_to_l_1.png",
            KeyCode::Digit2,
            KeyCode::LShift,
            KeyCode::DownArrow
        );

        TEST_KBD(
            "mark line",
            "vnc_list_kbd_to_l_1.png",
            KeyCode::Key_M
        );

        TEST_KBD(
            "$number",
            "vnc_list_kbd_to_l_1.png",
            KeyCode::LShift,
            KeyCode::Digit4
        );

        TEST_KBD(
            "$ + down => end",
            "vnc_list_kbd_to_o_1.png",
            KeyCode::DownArrow
        );

        TEST_KBD(
            "to page 0 (nothing)",
            "vnc_list_kbd_to_o_1.png",
            KeyCode::LeftArrow
        );

        TEST_KBD(
            "to page 2",
            "vnc_list_kbd_to_p2.png",
            KeyCode::RightArrow
        );

        TEST_KBD(
            "toggle check on page 2",
            "vnc_list_kbd_p2_check_toggle.png",
            KeyCode::Key_I,
            KeyCode::Key_P
        );

        TEST_KBD(
            "to S",
            "vnc_list_kbd_to_s.png",
            KeyCode::Digit3,
            KeyCode::DownArrow
        );

        TEST_KBD(
            "reverse selection all",
            "vnc_list_kbd_i_a.png",
            KeyCode::Key_I,
            KeyCode::Key_A
        );

        TEST_KBD(
            "to page 1",
            "vnc_list_kbd_to_p1.png",
            KeyCode::LeftArrow
        );

        TEST_KBD(
            "reverse marker range selection",
            "vnc_list_kbd_space_i_marked.png",
            KeyCode::Key_I,
            KeyCode::LShift,
            KeyCode::Space
        );

        TEST_KBD(
            "tab to next (begin navigation)",
            "vnc_list_kbd_tab_to_nav_first.png",
            KeyCode::Tab
        );

        TEST_KBD(
            "4 tab (all checkbox)",
            "vnc_list_kbd_tab_to_all_checkbox.png",
            KeyCode::Digit4,
            KeyCode::Tab
        );

        TEST_KBD(
            "4 shift+tab (next navigation)",
            "vnc_list_kbd_tab_to_nav_next.png",
            KeyCode::Digit4,
            KeyCode::LShift,
            KeyCode::Tab
        );

        TEST_KBD(
            "shift+tab (edit navigation)",
            "vnc_list_kbd_tab_to_nav_edit.png",
            KeyCode::LShift,
            KeyCode::Tab
        );

        static constexpr auto expected_list_5_on_26 = "(13)\n"
            "{.file_name=\"a dir A\", .file_size=2770, .last_access_time=133897014580000000, .is_dir=1}\n"
            "{.file_name=\"a file B\", .file_size=400, .last_access_time=133897014480000000, .is_dir=0}\n"
            "{.file_name=\"a file C\", .file_size=40690, .last_access_time=133897014330000000, .is_dir=0}\n"
            "{.file_name=\"a dir C\", .file_size=248998130, .last_access_time=133897013170000000, .is_dir=1}\n"
            "{.file_name=\"a file J\", .file_size=173496092, .last_access_time=133897011720000000, .is_dir=0}\n"
            "{.file_name=\"a file K\", .file_size=473439026, .last_access_time=133897009400000000, .is_dir=0}\n"
            "{.file_name=\"a file L\", .file_size=347473368, .last_access_time=133897006480000000, .is_dir=0}\n"
            "{.file_name=\"a file M\", .file_size=828375282, .last_access_time=133897014600000000, .is_dir=0}\n"
            "{.file_name=\"a dir D\", .file_size=631901492, .last_access_time=133897011880000000, .is_dir=1}\n"
            "{.file_name=\"a file N\", .file_size=1360069298, .last_access_time=133897008480000000, .is_dir=0}\n"
            "{.file_name=\"a file O\", .file_size=1068902000, .last_access_time=133897004880000000, .is_dir=0}\n"
            "{.file_name=\"a dir E\", .file_size=3183070770, .last_access_time=133897010190000000, .is_dir=1}\n"
            "{.file_name=\"a file U\", .file_size=2203231666, .last_access_time=133897014350000000, .is_dir=0}\n"
            "(total=13)"
            ""_av;
        TEST_COPY_CB_TO_VNC_LIST(expected_list_5_on_26);

        #undef TEST_KBD
    }

    RED_TEST_CONTEXT("Click on 'file 2'")
    {
        event_manager.get_writable_time_base().monotonic_time += 10s;

        input_mouse(left_mouse_down, coord_vnc_file(2));
        CHECK_IMG("vnc_list_file_2_focus.png");

        TEST_MOUSE_NO_EVENT(left_mouse_up, coord_vnc_file(2));
        CHECK_IMG("vnc_list_file_2_focus.png");
    }

    RED_TEST_CONTEXT("Double click on 'file 2'")
    {
        TEST_MOUSE(left_mouse_down, coord_vnc_file(2), open_dir, [](void*, Path path){
            RED_CHECK(path.native() == "C:\\sub\\dir\\a dir C\\"_av);
            return true;
        });
        CHECK_IMG("vnc_list_file_2_loading.png");

        input_mouse(left_mouse_up, coord_vnc_file(2));
        CHECK_IMG("vnc_list_file_2_loading.png");
    }

    RED_TEST_CONTEXT("Click on 'close gui'")
    {
        TEST_MOUSE_NO_EVENT(left_mouse_down, coord_close_gui);
        CHECK_IMG("gui_close_btn_pressed.png");

        TEST_MOUSE(left_mouse_up, coord_close_gui, close_gui, called);
        CHECK_IMG("vnc_list_file_2_loading.png");
    }

    RED_TEST_CONTEXT("Progression transfer part")
    {
        ft_gui.server_vnc_file_list_start(Path{"C:\\sub\\dir"_sized_av});
        ft_gui.server_vnc_file_list_end();

        ft_gui.transfer_start(VNC::FileTransferGui::Direction::CbToVnc, {
            .items = 10000,
            .bytes = 12'345'678,
        });
        CHECK_IMG_AND_PARTIAL("transfer_start1.png");

        event_manager.get_writable_time_base().monotonic_time += 2s;
        ft_gui.transfer_progression({
            .state = VNC::FileTransferGui::Progression::State::InProgress,
            .items = 123,
            .bytes = 726'322,
            .total_bytes_adjust {},
        });
        CHECK_IMG_AND_PARTIAL("transfer_progress1.png");

        event_manager.get_writable_time_base().monotonic_time += 2s;
        ft_gui.transfer_progression({
            .state = VNC::FileTransferGui::Progression::State::InProgress,
            .items = 1,
            .bytes = 526'322,
            .total_bytes_adjust = -10'572'638,
        });
        CHECK_IMG_AND_PARTIAL("transfer_progress1_adjust.png");

        event_manager.get_writable_time_base().monotonic_time += 2s;
        ft_gui.transfer_progression({
            .state = VNC::FileTransferGui::Progression::State::InProgress,
            .items = 1,
            .bytes = 326'322,
            .total_bytes_adjust = +21'379'538,
        });
        CHECK_IMG_AND_PARTIAL("transfer_progress2_adjust.png");

        TEST_MOUSE_NO_EVENT(left_mouse_down, coord_stop_btn);
        CHECK_IMG_AND_PARTIAL("stop_transfer_active.png");
        TEST_MOUSE(left_mouse_up, coord_stop_btn, stop_transfer, called);

        ft_gui.transfer_progression({
            .state = VNC::FileTransferGui::Progression::State::Completed,
            .items {},
            .bytes {},
            .total_bytes_adjust {},
        });
        CHECK_IMG_AND_PARTIAL("transfer_completed.png");

        ft_gui.transfer_progression(VNC::FileTransferGui::Progression::abort());
        CHECK_IMG_AND_PARTIAL("transfer_aborted.png");

        ft_gui.transfer_progression(VNC::FileTransferGui::Progression::error(WinNtPathView{
            "my_file\\in\\a\\dir\\has\\an\\error.txt"_sized_av
        }));
        CHECK_IMG_AND_PARTIAL("transfer_error2.png");

        ft_gui.transfer_progression(VNC::FileTransferGui::Progression::error(WinNtPathView{
            "my_file\\in\\error.txt"_sized_av
        }));
        CHECK_IMG_AND_PARTIAL("transfer_error.png");
    }

    RED_TEST_CONTEXT("VNC root button")
    {
        TEST_MOUSE_NO_EVENT(left_mouse_down, coord_vnc_root_btn);
        CHECK_IMG("gui_vnc_root_pressed.png");

        TEST_MOUSE(left_mouse_up, coord_vnc_root_btn, open_dir, [](void*, Path path){
            RED_CHECK(path.native() == ""_av);
            return true;
        });
        CHECK_IMG("gui_vnc_root_released.png");
    }

    RED_TEST_CONTEXT("VNC parent button")
    {
        ft_gui.server_vnc_file_list_start(Path{"C:\\sub\\dir\\dir2"_sized_av});
        ft_gui.server_vnc_file_list_end();
        CHECK_IMG("gui_vnc_lvl_4.png");

        RED_TEST_CONTEXT("C:\\sub\\dir\\dir2\\ --> C:\\sub\\dir\\")
        {
            TEST_MOUSE_NO_EVENT(left_mouse_down, coord_vnc_parent_btn);
            CHECK_IMG("gui_vnc_parent_pressed_lvl_4.png");

            TEST_MOUSE(left_mouse_up, coord_vnc_parent_btn, open_dir, [](void*, Path path){
                RED_CHECK(path.native() == "C:\\sub\\dir\\"_av);
                return false;
            });
            CHECK_IMG("gui_vnc_parent_released_lvl_3_error.png");
        }

        RED_TEST_CONTEXT("C:\\sub\\dir\\ --> C:\\sub\\")
        {
            TEST_MOUSE_NO_EVENT(left_mouse_down, coord_vnc_parent_btn);
            CHECK_IMG("gui_vnc_parent_pressed_lvl_3.png");

            TEST_MOUSE(left_mouse_up, coord_vnc_parent_btn, open_dir, [](void*, Path path){
                RED_CHECK(path.native() == "C:\\sub\\"_av);
                return true;
            });
            ft_gui.server_vnc_file_list_end();
            CHECK_IMG("gui_vnc_parent_released_lvl_2.png");
        }

        RED_TEST_CONTEXT("C:\\sub\\ --> C:\\")
        {
            TEST_MOUSE_NO_EVENT(left_mouse_down, coord_vnc_parent_btn);
            CHECK_IMG("gui_vnc_parent_pressed_lvl_2.png");

            TEST_MOUSE(left_mouse_up, coord_vnc_parent_btn, open_dir, [](void*, Path path){
                RED_CHECK(path.native() == "C:\\"_av);
                return true;
            });
            ft_gui.server_vnc_file_list_end();
            CHECK_IMG("gui_vnc_parent_released_lvl_1.png");
        }

        RED_TEST_CONTEXT("C:\\ --> ''")
        {
            TEST_MOUSE_NO_EVENT(left_mouse_down, coord_vnc_parent_btn);
            CHECK_IMG("gui_vnc_parent_pressed_lvl_1.png");

            TEST_MOUSE(left_mouse_up, coord_vnc_parent_btn, open_dir, [](void*, Path path){
                RED_CHECK(path.native() == ""_av);
                return true;
            });
            ft_gui.server_vnc_file_list_end();
            CHECK_IMG("gui_vnc_parent_released_lvl_0.png");
        }
    }

    RED_TEST_CONTEXT("To paste (vnc -> rdp)")
    {
        ft_gui.server_vnc_file_list_start(Path{"C:\\dir"_sized_av});
        ft_gui.server_vnc_file_list_add({
            .file_name { WinNtPathView { "file1"_sized_av } },
            .file_size = 653,
            .last_access_time = WinNtUTime{13199902683'0000000},
            .is_dir = false,
        });
        ft_gui.server_vnc_file_list_end();

        ft_gui.vnc_to_rdp_file_list_start();
        CHECK_IMG_AND_PARTIAL("vnc_to_rdp_file_list_start.png");

        ft_gui.vnc_to_rdp_file_list_ready();
        CHECK_IMG_AND_PARTIAL("vnc_to_rdp_file_ready.png");
    }

    #undef CHECK_IMG
    #undef CHECK_IMG_AND_PARTIAL
    #undef TEST_EVENT
    #undef TEST_NO_EVENT
    #undef TEST_MOUSE
    #undef TEST_MOUSE_NO_EVENT
    #undef TEST_COPY_CB_TO_VNC_LIST
}
