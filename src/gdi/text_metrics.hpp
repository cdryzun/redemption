/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "utils/sugar/bytes_view.hpp"
#include "gdi/graphic_api.hpp" // ColorCtx


class Font;
class FontCharView;
class GraphicApi;


namespace gdi
{

struct MultiLineTextMetrics
{
    using Char = FontCharView const *;
    using Line = array_view<Char>;

    explicit MultiLineTextMetrics() noexcept = default;
    explicit MultiLineTextMetrics(
        const Font& font, unsigned preferred_max_width, bytes_view utf8_text
    );

    MultiLineTextMetrics(MultiLineTextMetrics const&) = delete;
    MultiLineTextMetrics operator=(MultiLineTextMetrics const&) = delete;

    // MultiLineTextMetrics(MultiLineTextMetrics&& other) noexcept
    //     : d(other.d)
    // {
    //     other.d = Data();
    // }

    // MultiLineTextMetrics& operator=(MultiLineTextMetrics&& other) noexcept
    // {
    //     MultiLineTextMetrics g(std::move(*this));
    //     std::swap(d, other.d);
    //     return *this;
    // }

    ~MultiLineTextMetrics();

    /// Set a new text with a number of lines at 0.
    /// \post lines().size() == 0
    /// \post max_width() == 0
    void set_text(Font const& font, bytes_view utf8_text);

    void compute_lines(unsigned preferred_max_width) noexcept;

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

private:
    struct Data {
        void* data = nullptr;
        unsigned char_capacity = 0;
        unsigned nb_chars = 0;
        unsigned nb_line = 0;
        uint16_t max_width = 0;
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
