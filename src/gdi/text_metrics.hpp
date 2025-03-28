/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "utils/sugar/array_view.hpp"
#include "gdi/graphic_api.hpp" // ColorCtx


class Font;
class FontCharView;
class GraphicApi;


namespace gdi
{

class MultiLineText
{
public:
    using Char = FontCharView const *;
    using Line = array_view<Char>;

    explicit MultiLineText() noexcept = default;

    explicit MultiLineText(Font const & font, unsigned preferred_max_width, chars_view text)
    {
        set_text(font, text);
        update_dimension(preferred_max_width);
    }

    ~MultiLineText();

    MultiLineText(MultiLineText const&) = delete;
    MultiLineText operator=(MultiLineText const&) = delete;

    // MultiLineText(MultiLineText&& other) noexcept
    //     : d(other.d)
    // {
    //     other.d = Data();
    // }

    // MultiLineText& operator=(MultiLineText&& other) noexcept
    // {
    //     MultiLineText g(std::move(*this));
    //     std::swap(d, other.d);
    //     return *this;
    // }

    void set_text(Font const & font, chars_view text);

    void update_dimension(unsigned preferred_max_width) noexcept;

    Dimension dimension() const noexcept;

    /// Equivalent of a call to \c set_text() with an empty text.
    void clear_text() noexcept;

    bool has_text() const noexcept
    {
        return d.nb_chars;
    }

    array_view<Line> lines() const noexcept;

    uint16_t max_width() const noexcept
    {
        return d.max_width;
    }

    /// line size separator for \c draw()
    static uint16_t line_sep() noexcept
    {
        return 1;
    }

    struct WithLineSep { bool with_line_sep; };

    uint16_t line_height(WithLineSep with_sep) noexcept
    {
        auto h = d.line_height;
        if (with_sep.with_line_sep) {
            h += line_sep();
        }
        return h;
    }

    struct DrawingData
    {
        int16_t x;
        int16_t y;
        Rect clip;
        RDPColor fgcolor;
        RDPColor bgcolor;
        bool draw_bg_rect;
    };

    void draw(gdi::GraphicApi & drawable, DrawingData data);

private:
    struct Data
    {
        void* data = nullptr;
        unsigned char_capacity = 0;
        unsigned nb_chars = 0;
        unsigned nb_line = 0;
        uint16_t max_width = 0;
        uint16_t line_height = 0;
    };

    Data d;
};


struct DrawTextPadding
{
    uint16_t top;
    uint16_t right;
    uint16_t bottom;
    uint16_t left;

    struct Padding
    {
        uint16_t top_right_bottom_left;

        Padding(uint16_t padding) noexcept : top_right_bottom_left{padding} {}

        operator DrawTextPadding () const noexcept
        {
            return {
                .top = top_right_bottom_left,
                .right = top_right_bottom_left,
                .bottom = top_right_bottom_left,
                .left = top_right_bottom_left,
            };
        }
    };

    struct Horizontal
    {
        uint16_t left_right;

        operator DrawTextPadding () const noexcept
        {
            return {
                .top = 0,
                .right = left_right,
                .bottom = 0,
                .left = left_right,
            };
        }
    };

    struct Vertical
    {
        uint16_t top_bottom;

        operator DrawTextPadding () const noexcept
        {
            return {
                .top = top_bottom,
                .right = 0,
                .bottom = top_bottom,
                .left = 0,
            };
        }
    };

    struct Padding2
    {
        uint16_t top_bottom;
        uint16_t left_right;

        operator DrawTextPadding () const noexcept
        {
            return {
                .top = top_bottom,
                .right = left_right,
                .bottom = top_bottom,
                .left = left_right,
            };
        }
    };

    struct Right
    {
        uint16_t right;

        operator DrawTextPadding () const noexcept
        {
            return {
                .top = 0,
                .right = right,
                .bottom = 0,
                .left = 0,
            };
        }
    };
};

/// \return last pixel drawn
int draw_text(
    GraphicApi & drawable,
    int x,
    int y,
    uint16_t max_height_text,
    DrawTextPadding padding,
    array_view<FontCharView const *> fcs,
    RDPColor fgcolor,
    RDPColor bgcolor,
    Rect clip
);

}  // namespace gdi
