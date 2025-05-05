/*
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *   Product name: redemption, a FLOSS RDP proxy
 *   Copyright (C) Wallix 2010-2014
 *   Author(s): Christophe Grosjean, Meng Tan
 */

#pragma once

#include "utils/colors.hpp"

#include <string>


struct Theme
{
    bool enable_theme = false;
    std::string logo_path;

    struct Global {
        BGRColor bgcolor = NamedBGRColor::BG_BLUE;
        BGRColor fgcolor = NamedBGRColor::WHITE;
        BGRColor separator_color = NamedBGRColor::LIGHT_BLUE;
        BGRColor focus_color = NamedBGRColor::FOCUS_BLUE;
        BGRColor error_color = NamedBGRColor::YELLOW;
    } global;

    struct Edit {
        BGRColor bgcolor = NamedBGRColor::WHITE;
        BGRColor fgcolor = NamedBGRColor::BLACK;
        BGRColor border_color = NamedBGRColor::BG_BLUE;
        BGRColor focus_border_color = NamedBGRColor::FOCUS_BLUE;
        BGRColor placeholder_color = NamedBGRColor::MEDIUM_GREY;
        BGRColor cursor_color = BGRColor{0x888888};
        BGRColor password_toggle_color = NamedBGRColor::MEDIUM_GREY;
    } edit;

    struct Tooltip {
        BGRColor bgcolor = NamedBGRColor::LIGHT_YELLOW;
        BGRColor fgcolor = NamedBGRColor::BLACK;
        BGRColor border_color = NamedBGRColor::BLACK;
    } tooltip;

    struct SelectorLine1 {
        BGRColor bgcolor = NamedBGRColor::PALE_BLUE;
        BGRColor fgcolor = NamedBGRColor::BLACK;
    } selector_line1;
    struct SelectorLine2 {
        BGRColor bgcolor = NamedBGRColor::LIGHT_BLUE;
        BGRColor fgcolor = NamedBGRColor::BLACK;
    } selector_line2;
    struct SelectorSelected {
        BGRColor bgcolor = NamedBGRColor::MEDIUM_BLUE;
        BGRColor fgcolor = NamedBGRColor::WHITE;
    } selector_selected;
    struct SelectorFocus {
        BGRColor bgcolor = NamedBGRColor::FOCUS_BLUE;
        BGRColor fgcolor = NamedBGRColor::WHITE;
    } selector_focus;
    struct SelectorLabel {
        BGRColor bgcolor = NamedBGRColor::MEDIUM_BLUE;
        BGRColor fgcolor = NamedBGRColor::WHITE;
    } selector_label;
};
