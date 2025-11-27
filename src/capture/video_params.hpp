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
*   Copyright (C) Wallix 2010-2016
*   Author(s): Christophe Grosjean
*/

#pragma once

#include <string>
#include <chrono>

#include "utils/sugar/array_view.hpp"

struct VideoParams
{
    struct Thumbnail
    {
        bool enabled;
        unsigned width;
        unsigned height;
        bool use_proportional_geometry;
    };

    unsigned frame_rate;
    std::string codec;
    std::string codec_options;
    bool no_timestamp;
    Thumbnail thumbnail;
    unsigned verbosity;
    array_view<unsigned long long> updatable_frame_marker_end_bitset_view {};
};
