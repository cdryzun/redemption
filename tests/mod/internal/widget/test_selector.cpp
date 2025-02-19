/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "test_only/test_framework/redemption_unit_tests.hpp"
#include "test_only/test_framework/check_img.hpp"
#include "test_only/mod/internal/widget/null_tooltip_shower.hpp"

#include "mod/internal/copy_paste.hpp"
#include "mod/internal/widget/selector.hpp"
#include "keyboard/keymap.hpp"
#include "keyboard/keylayouts.hpp"
#include "utils/strutils.hpp"
#include "utils/theme.hpp"

#include "test_only/gdi/test_graphic.hpp"
#include "test_only/core/font.hpp"


#define IMG_TEST_PATH FIXTURES_PATH "/img_ref/mod/internal/widget/selector/"

struct TestWidgetSelectorCtx
{
    CopyPaste copy_paste{false};
    TestGraphic drawable;
    NullTooltipShower tooltip_shower;

    WidgetSelector selector;

    TestWidgetSelectorCtx(uint16_t width, uint16_t height)
    : drawable{width, height}
    , selector(
        drawable, global_font_deja_vu_14(), copy_paste, Theme(),
        tooltip_shower, MsgTranslationCatalog::default_catalog(),
        nullptr, Rect(0, 0, width, height),
        {
            .headers = {
                .authorization = "Authorization"_av,
                .target = "Target"_av,
                .protocol = "Protocol"_av,
            },
            .device_name = "x@127.0.0.1"_av,
        },
        {
            .onconnect = WidgetEventNotifier(),
            .oncancel = WidgetEventNotifier(),
            .onfilter = WidgetEventNotifier(),
            .onfirst_page = WidgetEventNotifier(),
            .onprev_page = WidgetEventNotifier(),
            .oncurrent_page = WidgetEventNotifier(),
            .onnext_page = WidgetEventNotifier(),
            .onlast_page = WidgetEventNotifier(),
            .onprev_page_on_grid = WidgetEventNotifier(),
            .onnext_page_on_grid = WidgetEventNotifier(),
            .onctrl_shift = WidgetEventNotifier(),
        }
    )
    {
        selector.init_focus();
    }

    void add_devices()
    {
        selector.set_page({
            .authorizations = "rdp\nvnc\nrdp\nrdp\nvnc"_av,
            .targets =
                "qa\\administrateur@10.10.14.111\n"
                "administrateur@qa@10.10.14.111\n"
                "administrateur@qa@10.10.14.27\n"
                "administrateur@qa@10.10.14.103\n"
                "administrateur@qa@10.10.14.33\n"
                ""_av,
            .protocols = "RDP\nVNC\nRDP\nVNC\nRDP"_av,
            .current_page = 1,
            .number_of_page = 1,
        });
    }

    struct Kbd
    {
        TestWidgetSelectorCtx & ctx;
        Keymap keymap {*find_layout_by_id(KeyLayout::KbdId(0x040C))};

        void rdp_input_scancode(Keymap::KeyCode keycode)
        {
            auto ukeycode = underlying_cast(keycode);
            auto scancode = Keymap::Scancode(ukeycode & 0x7F);
            auto flags = (ukeycode & 0x80) ? Keymap::KbdFlags::Extended : Keymap::KbdFlags();
            keymap.event(flags, scancode);
            ctx.selector.rdp_input_scancode(flags, scancode, 0, keymap);
        }
    };
};

RED_AUTO_TEST_CASE(TraceWidgetSelector)
{
    TestWidgetSelectorCtx ctx(800, 600);
    auto& selector = ctx.selector;

    auto line1 = "rdp|qa\\administrateur@10.10.14.111"_av;
    auto line2 = "vnc|administrateur@qa@10.10.14.111"_av;
    auto line3 = "rdp|administrateur@qa@10.10.14.27"_av;
    auto line4 = "rdp|administrateur@qa@10.10.14.103"_av;
    auto line5 = "vnc|administrateur@qa@10.10.14.33"_av;

    auto selected_line = [&]{
        auto line = selector.selected_line();
        return str_concat(line.authorization, '|', line.target);
    };

    RED_CHECK(selector.max_line_per_page() == 17);

    selector.rdp_input_invalidate(Rect(
        20 + selector.x(),
        5 + selector.y(),
        30,
        10
    ));

    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "selector_0.png");

    RED_CHECK("|" == selected_line());

    selector.rdp_input_invalidate(selector.get_rect());

    // no line
    selector.set_page({});
    RED_CHECK("|" == selected_line());
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "selector_nodata.png");
    RED_CHECK(selector.max_line_per_page() == 17);

    ctx.add_devices();
    RED_CHECK(selector.max_line_per_page() == 17);

    RED_CHECK(line1 == selected_line());

    selector.rdp_input_invalidate(selector.get_rect());

    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "selector_1.png");

    TestWidgetSelectorCtx::Kbd kbd{ctx};

    kbd.rdp_input_scancode(Keymap::KeyCode::End);
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "selector_2.png");
    RED_CHECK(line5 == selected_line());

    kbd.rdp_input_scancode(Keymap::KeyCode::DownArrow);
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "selector_1.png");
    RED_CHECK(line1 == selected_line());

    kbd.rdp_input_scancode(Keymap::KeyCode::DownArrow);
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "selector_3.png");
    RED_CHECK(line2 == selected_line());

    kbd.rdp_input_scancode(Keymap::KeyCode::DownArrow);
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "selector_4.png");
    RED_CHECK(line3 == selected_line());

    kbd.rdp_input_scancode(Keymap::KeyCode::DownArrow);
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "selector_5.png");
    RED_CHECK(line4 == selected_line());

    kbd.rdp_input_scancode(Keymap::KeyCode::UpArrow);
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "selector_4.png");
    RED_CHECK(line3 == selected_line());

    kbd.rdp_input_scancode(Keymap::KeyCode::Home);
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "selector_1.png");
    RED_CHECK(line1 == selected_line());

    kbd.rdp_input_scancode(Keymap::KeyCode::UpArrow);
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "selector_2.png");
    RED_CHECK(line5 == selected_line());

    kbd.rdp_input_scancode(Keymap::KeyCode::UpArrow);
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "selector_6.png");
    RED_CHECK(line4 == selected_line());

    kbd.rdp_input_scancode(Keymap::KeyCode::Tab);
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "selector_7.png");
    RED_CHECK(line4 == selected_line());
}

RED_AUTO_TEST_CASE(TraceWidgetSelectorFilter)
{
    TestWidgetSelectorCtx ctx(800, 600);
    auto& selector = ctx.selector;

    selector.rdp_input_invalidate(selector.get_rect());
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "selector_filter_0.png");

    selector.set_page({
        .authorizations = "reptile\nbird\nreptile\nfish\nbird"_av,
        .targets =
            "snake@10.10.14.111\n"
            "raven@10.10.14.111\n"
            "lezard@10.10.14.27\n"
            "shark@10.10.14.103\n"
            "eagle@10.10.14.33\n"
            ""_av,
        .protocols = "RDP\nRDP\nVNC\nRDP\nVNC"_av,
        .current_page = 1,
        .number_of_page = 1,
    });

    // authorization edit
    selector.rdp_input_mouse(MOUSE_FLAG_BUTTON1|MOUSE_FLAG_DOWN, 15, 66);

    selector.rdp_input_invalidate(selector.get_rect());

    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "selector_filter_1.png");

    TestWidgetSelectorCtx::Kbd kbd{ctx};

    kbd.rdp_input_scancode(Keymap::KeyCode::Tab);
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "selector_filter_2.png");

    kbd.rdp_input_scancode(Keymap::KeyCode::Tab);
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "selector_filter_3.png");

    kbd.rdp_input_scancode(Keymap::KeyCode::Tab);
    kbd.rdp_input_scancode(Keymap::KeyCode::Tab);
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "selector_filter_4.png");

    kbd.rdp_input_scancode(Keymap::KeyCode::End);
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "selector_filter_5.png");

    kbd.rdp_input_scancode(Keymap::KeyCode::UpArrow);
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "selector_filter_6.png");

    kbd.rdp_input_scancode(Keymap::KeyCode::Tab);
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "selector_filter_7.png");

    kbd.rdp_input_scancode(Keymap::KeyCode::Tab);
    kbd.rdp_input_scancode(Keymap::KeyCode::Tab);
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "selector_filter_8.png");

    kbd.rdp_input_scancode(Keymap::KeyCode::RightArrow);
    kbd.rdp_input_scancode(Keymap::KeyCode::LeftArrow);
    kbd.rdp_input_scancode(Keymap::KeyCode::Enter);
    kbd.rdp_input_scancode(Keymap::KeyCode::Tab);
    kbd.rdp_input_scancode(Keymap::KeyCode::Tab);
    kbd.rdp_input_scancode(Keymap::KeyCode::Tab);
    kbd.rdp_input_scancode(Keymap::KeyCode::Tab);
    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "selector_filter_9.png");
}

RED_AUTO_TEST_CASE(TraceWidgetColumnResize)
{
    TestWidgetSelectorCtx ctx(400, 300);
    auto& selector = ctx.selector;

    selector.set_page({
        .authorizations =
            "reptile reptile reptile reptile\n"
            "bird bird bird bird\n"
            "reptile reptile reptile reptile\n"
            "fish fish fish fish\n"
            "bird bird bird bird"
            ""_av,
        .targets =
            "snakesnakesnake@10.10.14.111\n"
            "ravenravenraven@10.10.14.111\n"
            "lezardlezardlezard@10.10.14.27\n"
            "sharksharkshark@10.10.14.103\n"
            "eagleeagleeagle@10.10.14.33\n"
            ""_av,
        .protocols = "RDP\nRDP\nVNC\nRDP\nVNC"_av,
        .current_page = 1,
        .number_of_page = 1,
    });

    auto click = [&](uint16_t x, uint16_t y){
        selector.rdp_input_mouse(MOUSE_FLAG_BUTTON1|MOUSE_FLAG_DOWN, x, y);
        selector.rdp_input_mouse(MOUSE_FLAG_BUTTON1, x, y);
    };

    selector.rdp_input_invalidate(selector.get_rect());

    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "selector_resize_1.png");

    click(108, 50);

    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "selector_resize_2.png");

    click(322, 50);

    RED_CHECK_IMG(ctx.drawable, IMG_TEST_PATH "selector_resize_1.png");
}
