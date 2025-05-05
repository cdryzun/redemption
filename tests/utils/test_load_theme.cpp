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
  Copyright (C) Wallix 2012-2014
  Author(s): Christophe Grosjean, Raphael Zhou, Meng Tan
*/

#include "test_only/test_framework/redemption_unit_tests.hpp"
#include "configs/config.hpp"
#include "utils/theme.hpp"
#include "utils/load_theme.hpp"
#include "utils/strutils.hpp"

namespace
{
    using Rgb = ::configs::spec_types::rgb;

    inline Rgb to_rgb(NamedBGRColor color)
    {
        return Rgb(BGRColor(BGRasRGBColor(color)).as_u32());
    }
}

RED_AUTO_TEST_CASE(TestLoadTheme_load_hardcoded_default_values)
{
    Theme colors;

    RED_CHECK(!colors.enable_theme);
    RED_CHECK_EQUAL(colors.logo_path, "");

    RED_CHECK_EQUAL(colors.global.bgcolor, NamedBGRColor::BG_BLUE);
    RED_CHECK_EQUAL(colors.global.fgcolor, NamedBGRColor::WHITE);
    RED_CHECK_EQUAL(colors.global.separator_color, NamedBGRColor::LIGHT_BLUE);
    RED_CHECK_EQUAL(colors.global.focus_color, NamedBGRColor::FOCUS_BLUE);
    RED_CHECK_EQUAL(colors.global.error_color, NamedBGRColor::YELLOW);

    RED_CHECK_EQUAL(colors.edit.bgcolor, NamedBGRColor::WHITE);
    RED_CHECK_EQUAL(colors.edit.fgcolor, NamedBGRColor::BLACK);
    RED_CHECK_EQUAL(colors.edit.border_color, NamedBGRColor::BG_BLUE);
    RED_CHECK_EQUAL(colors.edit.focus_border_color, NamedBGRColor::FOCUS_BLUE);

    RED_CHECK_EQUAL(colors.tooltip.bgcolor, NamedBGRColor::LIGHT_YELLOW);
    RED_CHECK_EQUAL(colors.tooltip.fgcolor, NamedBGRColor::BLACK);
    RED_CHECK_EQUAL(colors.tooltip.border_color, NamedBGRColor::BLACK);

    RED_CHECK_EQUAL(colors.selector_line1.bgcolor, NamedBGRColor::PALE_BLUE);
    RED_CHECK_EQUAL(colors.selector_line1.fgcolor, NamedBGRColor::BLACK);

    RED_CHECK_EQUAL(colors.selector_line2.bgcolor, NamedBGRColor::LIGHT_BLUE);
    RED_CHECK_EQUAL(colors.selector_line2.fgcolor, NamedBGRColor::BLACK);

    RED_CHECK_EQUAL(colors.selector_selected.bgcolor, NamedBGRColor::MEDIUM_BLUE);
    RED_CHECK_EQUAL(colors.selector_selected.fgcolor, NamedBGRColor::WHITE);

    RED_CHECK_EQUAL(colors.selector_focus.bgcolor, NamedBGRColor::FOCUS_BLUE);
    RED_CHECK_EQUAL(colors.selector_focus.fgcolor, NamedBGRColor::WHITE);

    RED_CHECK_EQUAL(colors.selector_label.bgcolor, NamedBGRColor::MEDIUM_BLUE);
    RED_CHECK_EQUAL(colors.selector_label.fgcolor, NamedBGRColor::WHITE);
}

RED_AUTO_TEST_CASE(TestLoadTheme_load_hardcoded_default_values_even_if_inifile_is_set)
{
    Inifile ini;

    ini.set<cfg::theme::enable_theme>(false);
    ini.set<cfg::theme::bgcolor>(to_rgb(NamedBGRColor::ORANGE));
    ini.set<cfg::theme::fgcolor>(to_rgb(NamedBGRColor::WHITE));
    ini.set<cfg::theme::separator_color>(to_rgb(NamedBGRColor::BROWN));
    ini.set<cfg::theme::focus_color>(to_rgb(NamedBGRColor::DARK_RED));
    ini.set<cfg::theme::error_color>(to_rgb(NamedBGRColor::RED));
    ini.set<cfg::theme::logo_path>(str_concat(app_path(AppPath::Cfg),
                                              "/themes/test_theme/logo.png"));

    ini.set<cfg::theme::edit_bgcolor>(to_rgb(NamedBGRColor::YELLOW));
    ini.set<cfg::theme::edit_fgcolor>(to_rgb(NamedBGRColor::WHITE));
    ini.set<cfg::theme::edit_border_color>(to_rgb(NamedBGRColor::DARK_RED));

    ini.set<cfg::theme::tooltip_bgcolor>(to_rgb(NamedBGRColor::PALE_BLUE));
    ini.set<cfg::theme::tooltip_fgcolor>(to_rgb(NamedBGRColor::DARK_BLUE));
    ini.set<cfg::theme::tooltip_border_color>(to_rgb(NamedBGRColor::DARK_GREEN));

    ini.set<cfg::theme::selector_line1_bgcolor>(to_rgb(NamedBGRColor::PALE_ORANGE));
    ini.set<cfg::theme::selector_line1_fgcolor>(to_rgb(NamedBGRColor::WHITE));

    ini.set<cfg::theme::selector_line2_bgcolor>(to_rgb(NamedBGRColor::LIGHT_ORANGE));
    ini.set<cfg::theme::selector_line2_fgcolor>(to_rgb(NamedBGRColor::WHITE));

    ini.set<cfg::theme::selector_selected_bgcolor>(to_rgb(NamedBGRColor::MEDIUM_RED));
    ini.set<cfg::theme::selector_selected_fgcolor>(to_rgb(NamedBGRColor::BLACK));

    ini.set<cfg::theme::selector_focus_bgcolor>(to_rgb(NamedBGRColor::DARK_RED));
    ini.set<cfg::theme::selector_focus_fgcolor>(to_rgb(NamedBGRColor::BLACK));

    ini.set<cfg::theme::selector_label_bgcolor>(to_rgb(NamedBGRColor::MEDIUM_RED));
    ini.set<cfg::theme::selector_label_fgcolor>(to_rgb(NamedBGRColor::BLACK));

    Theme colors;

    load_theme(colors, ini);

    RED_CHECK(!colors.enable_theme);
    RED_CHECK_EQUAL(colors.logo_path, "");

    RED_CHECK_EQUAL(colors.global.bgcolor, NamedBGRColor::BG_BLUE);
    RED_CHECK_EQUAL(colors.global.fgcolor, NamedBGRColor::WHITE);
    RED_CHECK_EQUAL(colors.global.separator_color, NamedBGRColor::LIGHT_BLUE);
    RED_CHECK_EQUAL(colors.global.focus_color, NamedBGRColor::FOCUS_BLUE);
    RED_CHECK_EQUAL(colors.global.error_color, NamedBGRColor::YELLOW);

    RED_CHECK_EQUAL(colors.edit.bgcolor, NamedBGRColor::WHITE);
    RED_CHECK_EQUAL(colors.edit.fgcolor, NamedBGRColor::BLACK);
    RED_CHECK_EQUAL(colors.edit.border_color, NamedBGRColor::BG_BLUE);
    RED_CHECK_EQUAL(colors.edit.focus_border_color, NamedBGRColor::FOCUS_BLUE);

    RED_CHECK_EQUAL(colors.tooltip.bgcolor, NamedBGRColor::LIGHT_YELLOW);
    RED_CHECK_EQUAL(colors.tooltip.fgcolor, NamedBGRColor::BLACK);
    RED_CHECK_EQUAL(colors.tooltip.border_color, NamedBGRColor::BLACK);

    RED_CHECK_EQUAL(colors.selector_line1.bgcolor, NamedBGRColor::PALE_BLUE);
    RED_CHECK_EQUAL(colors.selector_line1.fgcolor, NamedBGRColor::BLACK);

    RED_CHECK_EQUAL(colors.selector_line2.bgcolor, NamedBGRColor::LIGHT_BLUE);
    RED_CHECK_EQUAL(colors.selector_line2.fgcolor, NamedBGRColor::BLACK);

    RED_CHECK_EQUAL(colors.selector_selected.bgcolor, NamedBGRColor::MEDIUM_BLUE);
    RED_CHECK_EQUAL(colors.selector_selected.fgcolor, NamedBGRColor::WHITE);

    RED_CHECK_EQUAL(colors.selector_focus.bgcolor, NamedBGRColor::FOCUS_BLUE);
    RED_CHECK_EQUAL(colors.selector_focus.fgcolor, NamedBGRColor::WHITE);

    RED_CHECK_EQUAL(colors.selector_label.bgcolor, NamedBGRColor::MEDIUM_BLUE);
    RED_CHECK_EQUAL(colors.selector_label.fgcolor, NamedBGRColor::WHITE);
}

RED_AUTO_TEST_CASE(TestLoadTheme_load_from_inifile)
{
    Inifile ini;

    ini.set<cfg::theme::enable_theme>(true);
    ini.set<cfg::theme::bgcolor>(Rgb(0xDD8015));
    ini.set<cfg::theme::fgcolor>(Rgb(0xffffff));
    ini.set<cfg::theme::separator_color>(Rgb(0xC56A00));
    ini.set<cfg::theme::focus_color>(Rgb(0xAD1C22));
    ini.set<cfg::theme::error_color>(Rgb(0xff0000));
    ini.set<cfg::theme::logo_path>(str_concat(app_path(AppPath::Cfg),
                                              "/themes/test_theme/logo.png"));

    ini.set<cfg::theme::edit_bgcolor>(to_rgb(NamedBGRColor::WHITE));
    ini.set<cfg::theme::edit_fgcolor>(to_rgb(NamedBGRColor::BLACK));
    ini.set<cfg::theme::edit_border_color>(to_rgb(NamedBGRColor::DARK_RED));

    ini.set<cfg::theme::tooltip_bgcolor>(to_rgb(NamedBGRColor::PALE_BLUE));
    ini.set<cfg::theme::tooltip_fgcolor>(to_rgb(NamedBGRColor::DARK_BLUE));
    ini.set<cfg::theme::tooltip_border_color>(to_rgb(NamedBGRColor::DARK_GREEN));

    ini.set<cfg::theme::selector_line1_bgcolor>(to_rgb(NamedBGRColor::LIGHT_ORANGE));
    ini.set<cfg::theme::selector_line1_fgcolor>(to_rgb(NamedBGRColor::BLACK));

    ini.set<cfg::theme::selector_line2_bgcolor>(to_rgb(NamedBGRColor::LIGHT_ORANGE));
    ini.set<cfg::theme::selector_line2_fgcolor>(to_rgb(NamedBGRColor::BLACK));

    ini.set<cfg::theme::selector_selected_bgcolor>(to_rgb(NamedBGRColor::MEDIUM_RED));
    ini.set<cfg::theme::selector_selected_fgcolor>(to_rgb(NamedBGRColor::WHITE));

    ini.set<cfg::theme::selector_focus_bgcolor>(to_rgb(NamedBGRColor::DARK_RED));
    ini.set<cfg::theme::selector_focus_fgcolor>(to_rgb(NamedBGRColor::WHITE));

    ini.set<cfg::theme::selector_label_bgcolor>(to_rgb(NamedBGRColor::MEDIUM_RED));
    ini.set<cfg::theme::selector_label_fgcolor>(to_rgb(NamedBGRColor::WHITE));

    Theme colors;

    load_theme(colors, ini);

    RED_CHECK(colors.enable_theme);
    RED_CHECK_EQUAL(colors.logo_path,
                    str_concat(app_path(AppPath::Cfg), "/themes/test_theme/logo.png"));

    RED_CHECK_EQUAL(colors.global.bgcolor, NamedBGRColor::ORANGE);
    RED_CHECK_EQUAL(colors.global.fgcolor, NamedBGRColor::WHITE);
    RED_CHECK_EQUAL(colors.global.separator_color, NamedBGRColor::BROWN);
    RED_CHECK_EQUAL(colors.global.focus_color, NamedBGRColor::DARK_RED);
    RED_CHECK_EQUAL(colors.global.error_color, NamedBGRColor::RED);

    RED_CHECK_EQUAL(colors.edit.bgcolor, NamedBGRColor::WHITE);
    RED_CHECK_EQUAL(colors.edit.fgcolor, NamedBGRColor::BLACK);
    RED_CHECK_EQUAL(colors.edit.border_color, NamedBGRColor::DARK_RED);

    RED_CHECK_EQUAL(colors.tooltip.bgcolor, NamedBGRColor::PALE_BLUE);
    RED_CHECK_EQUAL(colors.tooltip.fgcolor, NamedBGRColor::DARK_BLUE);
    RED_CHECK_EQUAL(colors.tooltip.border_color, NamedBGRColor::DARK_GREEN);

    RED_CHECK_EQUAL(colors.selector_line1.bgcolor, NamedBGRColor::LIGHT_ORANGE);
    RED_CHECK_EQUAL(colors.selector_line1.fgcolor, NamedBGRColor::BLACK);

    RED_CHECK_EQUAL(colors.selector_line2.bgcolor, NamedBGRColor::LIGHT_ORANGE);
    RED_CHECK_EQUAL(colors.selector_line2.fgcolor, NamedBGRColor::BLACK);

    RED_CHECK_EQUAL(colors.selector_selected.bgcolor, NamedBGRColor::MEDIUM_RED);
    RED_CHECK_EQUAL(colors.selector_selected.fgcolor, NamedBGRColor::WHITE);

    RED_CHECK_EQUAL(colors.selector_focus.bgcolor, NamedBGRColor::DARK_RED);
    RED_CHECK_EQUAL(colors.selector_focus.fgcolor, NamedBGRColor::WHITE);

    RED_CHECK_EQUAL(colors.selector_label.bgcolor, NamedBGRColor::MEDIUM_RED);
    RED_CHECK_EQUAL(colors.selector_label.fgcolor, NamedBGRColor::WHITE);
}
