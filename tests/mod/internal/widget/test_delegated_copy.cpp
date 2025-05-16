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
Copyright (C) Wallix 2021
Author(s): Proxies Team
*/

#include "test_only/test_framework/redemption_unit_tests.hpp"
#include "test_only/test_framework/check_img.hpp"

#include "mod/internal/widget/delegated_copy.hpp"
#include "mod/internal/widget/screen.hpp"
#include "utils/theme.hpp"

#include "test_only/gdi/test_graphic.hpp"
#include "test_only/core/font.hpp"
#include "test_only/mod/internal/widget/notify_trace.hpp"


#define IMG_TEST_PATH FIXTURES_PATH "/img_ref/mod/internal/widget/delegated_copy/"


RED_AUTO_TEST_CASE(TraceWidgetDelegatedCopy)
{
    const uint16_t w = 30;
    const uint16_t h = 30;
    const Font& font = global_font_deja_vu_14();

    TestGraphic gd(w, h);

    WidgetScreen screen(gd, w, h, font, Theme{});

    WidgetDelegatedCopy::Colors colors{
        .fg = NamedBGRColor::RED,
        .bg = NamedBGRColor::YELLOW,
        .active_bg = NamedBGRColor::BLUE,
    };

    NotifyTrace notifier;
    WidgetDelegatedCopy delegated(gd, notifier, colors, font);

    delegated.set_xy(5, 5);

    screen.add_widget(delegated);
    screen.rdp_input_invalidate(screen.get_rect());

    RED_CHECK(notifier.get_and_reset() == 0);
    RED_CHECK_IMG(gd, IMG_TEST_PATH "delegated_copy_1.png");

    delegated.rdp_input_mouse(MOUSE_FLAG_BUTTON1|MOUSE_FLAG_DOWN, 10, 10);

    RED_CHECK(notifier.get_and_reset() == 0);
    RED_CHECK_IMG(gd, IMG_TEST_PATH "delegated_copy_2.png");

    delegated.rdp_input_mouse(MOUSE_FLAG_BUTTON1, 10, 10);

    RED_CHECK(notifier.get_and_reset() == 1);
    RED_CHECK_IMG(gd, IMG_TEST_PATH "delegated_copy_3.png");
}
