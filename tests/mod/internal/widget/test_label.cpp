/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "test_only/test_framework/redemption_unit_tests.hpp"
#include "test_only/test_framework/check_img.hpp"

#include "test_only/gdi/test_graphic.hpp"
#include "test_only/core/font.hpp"

#include "mod/internal/widget/label.hpp"
#include "mod/internal/widget/composite.hpp"


#define IMG_TEST_PATH FIXTURES_PATH "/img_ref/mod/internal/widget/label/"


RED_AUTO_TEST_CASE(TraceWidgetLabelAndComposite)
{
    struct WComposite : WidgetComposite
    {
        using WidgetComposite::WidgetComposite;
        using WidgetComposite::set_wh;
    };

    TestGraphic drawable(200, 100);

    WComposite wcomposite(drawable, Widget::Focusable::No, NamedBGRColor::BLACK);
    wcomposite.set_wh(drawable.width(), drawable.height());
    wcomposite.set_xy(0, 0);

    WidgetLabel wlabel1(drawable, global_font_deja_vu_14(), "abababab"_av,
                        {NamedBGRColor::YELLOW, NamedBGRColor::BLACK});
    wlabel1.set_xy(0, 0);

    WidgetLabel wlabel2(drawable, global_font_deja_vu_14(), "ggghdgh"_av,
                        {NamedBGRColor::WHITE, NamedBGRColor::BLUE});
    wlabel2.set_xy(0, 30);

    WidgetLabel wlabel3(drawable, global_font_deja_vu_14(), "lldlslql"_av,
                        {NamedBGRColor::BLUE, NamedBGRColor::RED});
    wlabel3.set_xy(100, 25);

    WidgetLabel wlabel4(drawable, global_font_deja_vu_14(), "LLLLMLLM"_av,
                        {NamedBGRColor::PINK, NamedBGRColor::DARK_GREEN});
    wlabel4.set_xy(150, 50);

    WidgetLabel wlabel5(drawable, global_font_deja_vu_14(), "dsdsdjdjs"_av,
                        {NamedBGRColor::LIGHT_GREEN, NamedBGRColor::DARK_BLUE});
    wlabel5.set_xy(120, -10);

    WidgetLabel wlabel6(drawable, global_font_deja_vu_14(), "xxwwp"_av,
                        {NamedBGRColor::ANTHRACITE, NamedBGRColor::PALE_GREEN});
    wlabel6.set_xy(-10, 50);

    wcomposite.add_widget(wlabel1);
    wcomposite.add_widget(wlabel2);
    wcomposite.add_widget(wlabel3);
    wcomposite.add_widget(wlabel4);
    wcomposite.add_widget(wlabel5);
    wcomposite.add_widget(wlabel6);

    //ask to widget to redraw at position 100,25 and of size 100x100.
    wcomposite.rdp_input_invalidate(Rect(50, 25, 100, 100));

    RED_CHECK_IMG(drawable, IMG_TEST_PATH "label_1.png");

    //ask to widget to redraw at it's current position
    wcomposite.rdp_input_invalidate(wcomposite.get_rect());

    RED_CHECK_IMG(drawable, IMG_TEST_PATH "label_2.png");
}

RED_AUTO_TEST_CASE(TraceWidgetLabelMax)
{
    auto text =
        "éàéàéàéàéàéàéàéàéàéàéàéàéàéàéàéà"
        "éàéàéàéàéàéàéàéàéàéàéàéàéàéàéàéà"
        "éàéàéàéàéàéàéàéàéàéàéàéàéàéàéàéà"
        "éàéàéàéàéàéàéàéàéàéàéàéàéàéàéàéà"_av;

    TestGraphic drawable{1120, 40};
    WidgetLabel wlabel(
        drawable, global_font_deja_vu_14(), text,
        {.fg = NamedBGRColor::RED, .bg = NamedBGRColor::YELLOW}
    );

    wlabel.set_xy(10, 10);

    // ask to widget to redraw at it's current position
    wlabel.rdp_input_invalidate(wlabel.get_rect());

    RED_CHECK_IMG(drawable, IMG_TEST_PATH "label_3.png");
}
