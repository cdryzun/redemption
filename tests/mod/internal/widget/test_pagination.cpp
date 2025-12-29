/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "test_only/test_framework/redemption_unit_tests.hpp"
#include "test_only/test_framework/check_img.hpp"
#include "test_only/gdi/test_graphic.hpp"
#include "test_only/core/font.hpp"

#include "mod/internal/widget/pagination.hpp"
#include "keyboard/keymap.hpp"
#include "keyboard/keylayouts.hpp"
#include "utils/theme.hpp"


#define IMG_TEST_PATH FIXTURES_PATH "/img_ref/mod/internal/widget/pagination/"


namespace
{

struct PaginationPageChangeEvent
{
    int nb_call = 0;
    uint32_t page = 0;
    int previous_nb_call = 0;

    bool operator == (PaginationPageChangeEvent const& other) const noexcept = default;
};

}

#if !REDEMPTION_UNIT_TEST_FAST_CHECK
# include "test_only/test_framework/compare_collection.hpp"

namespace
{

ut::assertion_result cmp_page_change_event(
    PaginationPageChangeEvent a,
    PaginationPageChangeEvent b)
{
    ut::assertion_result ar(true);

    if (a != b) [[unlikely]]
    {
        ar = false;

        auto put = [&](std::ostream& out, PaginationPageChangeEvent const& x) {
            out << "{.nb_call=" << x.nb_call << ", .page=" << x.page;
            if (x.previous_nb_call)
            {
                out << ", .previous_nb_call=" << x.previous_nb_call;
            }
            out << "}";
        };

        auto& out = ar.message().stream();
        out << "[";
        ut::put_data_with_diff(out, a, "!=", b, put);
        out << "]";
    }

    return ar;
}

}

RED_TEST_DISPATCH_COMPARISON_EQ(
    (),
    (::PaginationPageChangeEvent),
    (::PaginationPageChangeEvent),
    ::cmp_page_change_event
)
#endif

RED_AUTO_TEST_CASE(TestWidgetPagination)
{
    /*
     * Init
     */

    constexpr uint16_t cx = 250;
    constexpr uint16_t cy = 40;
    TestGraphic gd{cx, cy};

    struct Ctx
    {
        int nb_call = 0;
        uint32_t page = 0;
    };
    Ctx ctx;

    WidgetPagination pag {
        gd, global_font_deja_vu_14(),
        WidgetPagination::Colors::from_theme(Theme{}),
        WidgetPagination::RedrawAfterEvent::Yes,
        [&ctx](uint32_t page) {
            ctx.nb_call++;
            ctx.page = page;
        }
    };

    auto scoped_change_page = [&](auto f){
        PaginationPageChangeEvent event{0, 0, ctx.nb_call};
        ctx = {};
        f();
        event.page = ctx.page;
        event.nb_call = ctx.nb_call;
        ctx.nb_call = 0;
        return event;
    };

    Keymap keymap{*find_layout_by_id(KeyLayout::KbdId(0x409))}; // en-US

    constexpr uint16_t offset_x = 1;
    constexpr uint16_t offset_y = 1;
    pag.update({
        .current_page = 2,
        .total_page = 5,
    });
    pag.set_xy(offset_x, offset_y);


    /*
     * Drawing
     */

    #define CHECK_PART(filename, color, ...) do {  \
        Rect rect = __VA_ARGS__;                   \
        gd.draw_rect(rect, color);                 \
        pag.rdp_input_invalidate(rect);            \
        RED_CHECK_IMG(gd, IMG_TEST_PATH filename); \
    } while (0)

    CHECK_PART("part1.png", NamedBGRColor::RED, {0, 0, 40, cy});
    CHECK_PART("part3.png", NamedBGRColor::RED, {80, 0, 40, cy});
    CHECK_PART("part5.png", NamedBGRColor::RED, {160, 0, 40, cy});

    gd.draw_rect({0, 0, cx, cy}, NamedBGRColor::BLACK);
    CHECK_PART("part2.png", NamedBGRColor::GREEN, {40, 0, 40, cy});
    CHECK_PART("part4.png", NamedBGRColor::GREEN, {120, 0, 40, cy});

    pag.init_focus();
    gd.draw_rect({0, 0, cx, cy}, NamedBGRColor::BLACK);
    // draw each pixel
    for (int16_t x = 0; x < cx; ++x)
    {
        pag.rdp_input_invalidate({x, 0, 1, cy});
    }
    CHECK_PART("edit_text_2.png", NamedBGRColor::RED, pag.get_rect());

    #undef CHECK


    /*
     * Unicode / scancode on edit
     */

    using kbdtypes::KbdFlags;
    using kbdtypes::Scancode;
    auto kbd_down = KbdFlags::NoFlags;

    auto send_scancode = [&](Scancode scancode){
        keymap.event(kbd_down, scancode);
        pag.rdp_input_scancode(kbd_down, scancode, 0, keymap);
    };

    auto send_enter_scancode = [&] {
        send_scancode(Scancode::Enter);
    };

    #define CHECK_VALIDATION(page)                     \
        RED_CHECK((PaginationPageChangeEvent{!!page, page, 0}) \
            == scoped_change_page(send_enter_scancode))

    /*
     * Unicode
     */

    #define CHECK_UNICODE(ch, img_path) do {           \
        RED_TEST_CONTEXT("unicode : " #ch)             \
        {                                              \
            pag.rdp_input_unicode(kbd_down, ch);       \
            RED_CHECK_IMG(gd, IMG_TEST_PATH img_path); \
            CHECK_VALIDATION(0);                       \
        }                                              \
    } while (0)

    CHECK_UNICODE('c', "edit_text_2.png");
    CHECK_UNICODE('1', "edit_text_21.png");

    #undef CHECK_UNICODE

    /*
     * Scancode
     */

    #define CHECK_SCANCODE(page, sc, img_path) do {          \
        RED_TEST_CONTEXT("scancode: " #sc " | page: " #page) \
        {                                                    \
            send_scancode(sc);                               \
            RED_CHECK_IMG(gd, IMG_TEST_PATH img_path);       \
            CHECK_VALIDATION(page);                          \
        }                                                    \
    } while (0)

    CHECK_SCANCODE(0, Scancode::Backspace, "edit_text_2.png");
    CHECK_SCANCODE(0, Scancode::A, "edit_text_2.png");
    CHECK_SCANCODE(0, Scancode::Digit1, "edit_text_21.png");
    CHECK_SCANCODE(0, Scancode::Backspace, "edit_text_2.png");
    CHECK_SCANCODE(0, Scancode::Backspace, "edit_text_empty.png");
    CHECK_SCANCODE(1, Scancode::Digit1, "edit_text_1.png");
    CHECK_SCANCODE(0, Scancode::Backspace, "edit_text_empty.png");
    CHECK_SCANCODE(2, Scancode::Digit2, "edit_text_2.png");

    #undef CHECK_SCANCODE


    /*
     * Focus
     */

    using FocusDirection = Widget::FocusDirection;
    using FocusResult = Widget::NextFocusResult;

    RED_TEST_DATAS(
        // tab
        (IMG_TEST_PATH "focus_next.png", FocusDirection::Forward, FocusResult::Focused)
        (IMG_TEST_PATH "focus_last.png", FocusDirection::Forward, FocusResult::Focused)
        (IMG_TEST_PATH "focus_first.png", FocusDirection::Forward, FocusResult::Focusable)
        (IMG_TEST_PATH "focus_prev.png", FocusDirection::Forward, FocusResult::Focused)
        (IMG_TEST_PATH "edit_text_2.png", FocusDirection::Forward, FocusResult::Focused)
        // alt+tab
        (IMG_TEST_PATH "focus_prev.png", FocusDirection::Backward, FocusResult::Focused)
        (IMG_TEST_PATH "focus_first.png", FocusDirection::Backward, FocusResult::Focused)
        (IMG_TEST_PATH "focus_last.png", FocusDirection::Backward, FocusResult::Focusable)
        (IMG_TEST_PATH "focus_next.png", FocusDirection::Backward, FocusResult::Focused)
        (IMG_TEST_PATH "edit_text_2.png", FocusDirection::Backward, FocusResult::Focused)
    ) >>= [&](char const* img_path, FocusDirection direction, FocusResult focuse_result)
    {
        using Strategy = Widget::FocusStrategy;

        RED_CHECK(focuse_result == pag.next_focus(direction, Strategy::Next));

        if (focuse_result == FocusResult::Focusable)
        {
            RED_CHECK(FocusResult::Focused == pag.next_focus(direction, Strategy::Restart));
        }

        RED_CHECK_IMG(gd, img_path);
    };


    /*
     * Click
     */

    constexpr uint16_t mouse_1_down = MOUSE_FLAG_BUTTON1 | MOUSE_FLAG_DOWN;
    constexpr uint16_t mouse_1_up = MOUSE_FLAG_BUTTON1;

    auto input_mouse_wrapper = [&](uint16_t mouse_flags, uint16_t x, uint16_t y) {
        return [=, &pag]{
            pag.rdp_input_mouse(mouse_flags, x, y);
        };
    };

    #define CHECK_MOUSE_EVENT(ctx, mouse_flags, page, x, img_path) do { \
        RED_TEST_CONTEXT(ctx)                                           \
        {                                                               \
            RED_CHECK((PaginationPageChangeEvent{!!page, page, 0})      \
                == scoped_change_page(input_mouse_wrapper(              \
                    mouse_flags, offset_x + x, offset_y))               \
            );                                                          \
            RED_CHECK_IMG(gd, IMG_TEST_PATH img_path);                  \
            pag.rdp_input_invalidate(pag.get_rect());                   \
            RED_TEST_CONTEXT("redraw")                                  \
            {                                                           \
                RED_CHECK_IMG(gd, IMG_TEST_PATH img_path);              \
            }                                                           \
        }                                                               \
    } while (0)

    #define CHECK_CLICK(page, x, filename_down, filename_up) do {               \
        RED_TEST_CONTEXT("click at x: " #x " | next page: " #page)              \
        {                                                                       \
            CHECK_MOUSE_EVENT("mouse down", mouse_1_down, 0, x, filename_down); \
            CHECK_MOUSE_EVENT("mouse up", mouse_1_up, page, x, filename_up);    \
        }                                                                       \
    } while (0)

    // last -> 5
    CHECK_CLICK(5, 200, "last_pressed_text_2.png", "last_text_5.png");
    // prev -> 4
    CHECK_CLICK(4, 50, "prev_pressed_text_5.png", "prev_text_4.png");
    // first -> 1
    CHECK_CLICK(1, 2, "first_pressed_text_4.png", "first_text_1.png");
    // edit
    CHECK_CLICK(0, 90, "edit_text_1_focus_begin.png", "edit_text_1_focus_begin.png");
    // next -> 2
    CHECK_CLICK(2, 170, "next_pressed_text_1.png", "next_text_2.png");
    // no input
    CHECK_CLICK(0, pag.cx(), "next_text_2.png", "next_text_2.png");

    RED_TEST_CONTEXT("press on prev, release on last")
    {
        CHECK_MOUSE_EVENT("mouse down", mouse_1_down, 0, 50, "prev_pressed_text_2.png");
        CHECK_MOUSE_EVENT("mouse up", mouse_1_up, 0, 200, "prev_text_2.png");
    }

    #undef CHECK_CLICK
    #undef CHECK_MOUSE_EVENT

    #undef CHECK_VALIDATION
}
