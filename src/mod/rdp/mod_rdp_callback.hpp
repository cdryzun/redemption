/*
    This program is free software; you can redistribute it and/or modify it
     under the terms of the GNU General Public License as published by the
     Free Software Foundation; either version 2 of the License, or (at your
     option) any later version.

    This program is distributed in the hope that it will be useful, but
     WITHOUT ANY WARRANTY; without even the implied warranty of
     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
     Public License for more details.

    You should have received a copy of the GNU General Public License along
     with this program; if not, write to the Free Software Foundation, Inc.,
     675 Mass Ave, Cambridge, MA 02139, USA.

    Product name: redemption, a FLOSS RDP proxy
    Copyright (C) Wallix 2024
    Author(s): Christophe Grosjean, Raphael Zhou
*/


#pragma once

#include <string_view>

enum class Requester
{
    AppDriver,
    SesProbe
};

struct ModRdpCallbacks {
    virtual void freeze_screen() = 0;
    virtual void disable_graphics_update() = 0;
    virtual void enable_graphics_update() = 0;
    virtual void disable_input_event(Requester r) = 0;
    virtual void enable_input_event(Requester r) = 0;
    virtual void display_osd_message(std::string_view message) = 0;
    virtual ~ModRdpCallbacks() = default;
};
