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
Copyright (C) Wallix 2017
Author(s): Jonathan Poelen
*/

#pragma once

#include "utils/rect.hpp"
#include "utils/sugar/numerics/safe_conversions.hpp"

#include <utility>


struct CxCy
{
    explicit CxCy(checked_int<uint16_t> cx, checked_int<uint16_t> cy) noexcept
      : cx(cx)
      , cy(cy)
    {}

    uint16_t const cx;
    uint16_t const cy;
};

struct SubCxCy
{
    explicit SubCxCy(checked_int<uint16_t> cx, checked_int<uint16_t> cy) noexcept
      : cx(cx)
      , cy(cy)
    {}

    uint16_t const cx;
    uint16_t const cy;
};

/**
 * \brief apply f on each splitted sub-rect with maximal size specified by \c sub_wh
 * \param f  fonctor with a Rect parameter

    +-----+-----+-+
    |     |     | |
    |  1  |  2  |3|
    |     |     | |
    +-----+-----+-+
    |  4  |  5  |6|
    +-----+-----+-┘
*/
template<class F>
void contiguous_sub_rect_f(CxCy wh, SubCxCy sub_wh, F && f)
{
    for (uint16_t y = 0; y < wh.cy; y += sub_wh.cy) {
        uint16_t cy = std::min(sub_wh.cy, uint16_t(wh.cy - y));

        for (uint16_t x = 0; x < wh.cx ; x += sub_wh.cx) {
            uint16_t cx = std::min(sub_wh.cx, uint16_t(wh.cx - x));
            f(Rect{checked_int{x}, checked_int{y}, cx, cy});
        }
    }
}

// template<class F>
// void optimal_sub_rect_f(CxCy wh, std::size_t sub_pixcount, F && f);
