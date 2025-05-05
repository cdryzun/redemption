/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team

SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "test_only/test_framework/redemption_unit_tests.hpp"
#include "test_only/test_framework/check_img.hpp"

#include "mod/internal/widget/widget_rect.hpp"
#include "test_only/gdi/test_graphic.hpp"


#define IMG_TEST_PATH FIXTURES_PATH "/img_ref/mod/internal/widget/rect/"


RED_AUTO_TEST_CASE(TraceWidgetRect)
{
    TestGraphic drawable{300, 300};

    struct D
    {
        int16_t x;
        int16_t y;
        BGRColor color;
        bool partial = false;
    };

    for (auto d : {
        D{0, 0, BGRColor(0xffffff)},
        D{50, 0, BGRColor(0xff0000)},
        D{0, 50, BGRColor(0x0000ff)},
        D{10, 10, BGRColor(0x00ff00)},
        D{-50, 30, BGRColor(0xffff00)},
        D{-1, -50, BGRColor(0xff00ff)},
        D{100, 100, BGRColor(0xff0000)},
        D{100, 100, BGRColor(0x00ff00), true},
        D{50, 200, BGRColor(0x0000ff)},
        D{250, 250, BGRColor(0x0000ff)},
        D{250, 299, BGRColor(0x0000ff)},
    })
    {
        WidgetRect wrect{drawable, d.color};
        wrect.set_width(100);
        wrect.set_xy(d.x, d.y);
        auto rect = wrect.get_rect();
        if (d.partial) {
            rect.x += rect.cx / 4;
            rect.y += rect.cy / 2;
            rect.cx -= rect.cx / 2;
            rect.cy -= rect.cy / 2;
        }
        wrect.rdp_input_invalidate(rect);
    }

    RED_CHECK_IMG(drawable, IMG_TEST_PATH "rect.png");
}
