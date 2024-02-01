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
*   Copyright (C) Wallix 2010-2015
*   Author(s): Jonathan Poelen
*/

#include "gdi/text_metrics.hpp"
#include "core/RDP/orders/RDPOrdersCommon.hpp"
#include "core/RDP/orders/RDPOrdersPrimaryGlyphIndex.hpp"
#include "core/RDP/caches/glyphcache.hpp"
#include "utils/sugar/numerics/safe_conversions.hpp"
#include "utils/utf.hpp"

namespace
{
    bytes_view* multi_textmetrics_impl(
        const Font& font,
        bytes_view utf8_text,
        const int max_width,
        bytes_view* output,
        int* real_max_width
    ) {
        auto push_line_and_width = [&](bytes_view line, int width) {
            assert(gdi::TextMetrics(font, line).width == width);
            *output++ = line;
            *real_max_width = std::max(width, *real_max_width);
        };

        const int space_w = gdi::TextMetrics::char_width(font, ' ');

        uint8_t const* p = utf8_text.begin();
        uint8_t const* end = utf8_text.end();

        UTF8Reader utf8_reader;

    _start:

        auto* start_line = p;

        // consume spaces and new lines
        for (;;) {
            // left spaces are ignored (empty line)
            if (p == end) {
                return output;
            }

            if (*p == ' ') {
                ++p;
                continue;
            }

            if (*p == '\n') {
                *output++ = {};
                ++p;
                goto _start;
            }

            break;
        }

    _start_at_word:

        auto* start_first_word = p;

        int left_space_width = checked_cast<int>(p - start_line) * space_w;
        int line_width = left_space_width;

        // first word
        for (;;) {
            if (!utf8_reader.next({p, end})) {
                push_line_and_width({start_line, p}, line_width);
                return output;
            }

            auto uc = utf8_reader.unicode();

            if (uc == ' ') {
                break;
            }

            if (uc == '\n') {
                push_line_and_width({start_line, p}, line_width);
                ++p;
                goto _start;
            }

            int w = gdi::TextMetrics::char_width(font, uc);

            auto* new_p = utf8_reader.end_ptr;

            // too long
            if (max_width < line_width + w) [[unlikely]] {
                if (left_space_width) {
                    line_width -= left_space_width;
                    left_space_width = 0;
                    start_line = start_first_word;

                    // insert new line
                    *output++ = {};
                }

                // alway too long, push partial word
                if (max_width < line_width + w) {
                    if (start_first_word != p) {
                        push_line_and_width({start_first_word, p}, line_width);
                    }
                    line_width = 0;
                    start_line = p;
                    start_first_word = p;
                }
            }

            line_width += w;
            p = new_p;
        }

    _word:

        // right space after word
        assert(*p == ' ');
        int line_to_end_word_width = line_width;
        auto* end_word = p;
        while (++p < end && *p == ' ') {
        }

        int sep_width = checked_cast<int>(p - end_word) * space_w;

        if (p == end || *p == '\n' || max_width < line_width + sep_width) {
            push_line_and_width({start_line, end_word}, line_width);
            if (p == end) {
                return output;
            }
            if (*p == '\n') {
                ++p;
            }
            goto _start;
        }

        line_width += sep_width;

        auto* start_word = p;

        // other words
        for (;;) {
            if (!utf8_reader.next({p, end})) {
                push_line_and_width({start_line, p}, line_width);
                return output;
            }

            auto uc = utf8_reader.unicode();

            if (uc == ' ') {
                goto _word;
            }

            if (uc == '\n') {
                push_line_and_width({start_line, p}, line_width);
                ++p;
                goto _start;
            }

            int w = gdi::TextMetrics::char_width(font, uc);

            auto* new_p = utf8_reader.end_ptr;

            // too long
            if (max_width < line_width + w) [[unlikely]] {
                push_line_and_width({start_line, end_word}, line_to_end_word_width);
                start_line = start_word;
                p = start_word;
                goto _start_at_word;
            }

            line_width += w;
            p = new_p;
        }
    }
} // namespace

namespace gdi
{

TextMetrics::TextMetrics(const Font & font, bytes_view utf8_text)
: height(font.max_height())
{
    auto invalid_char = [&](auto){
        FontCharView const& font_item = font.unknown_glyph();
        width += font_item.offsetx + font_item.incby;
    };
    utf8_for_each(utf8_text,
        [&](uint32_t uc){ width += char_width(font, uc); },
        invalid_char,
        invalid_char
    );
}

int TextMetrics::char_width(Font const& font, uint32_t c)
{
    FontCharView const& font_item = font.item(c).view;
    return font_item.offsetx + font_item.incby;
}

MultiLineTextMetrics::MultiLineTextMetrics(
    const Font& font, bytes_view utf8_text, unsigned max_width)
{
    if (utf8_text.empty()) {
        return;
    }

    using Line = bytes_view;

    void* mem = aligned_alloc(alignof(Line), utf8_text.size() * 2 * sizeof(Line));
    d.lines = static_cast<bytes_view*>(mem);

    int real_max_width = 0;
    auto* end = multi_textmetrics_impl(font, utf8_text, int(max_width), d.lines, &real_max_width);
    d.max_width = checked_int(real_max_width);
    d.nb_line = checked_int(end - d.lines);
}

MultiLineTextMetrics::~MultiLineTextMetrics()
{
    free(d.lines);
}

// BUG TODO static not const is a bad idea
static GlyphCache mod_glyph_cache;


// TODO implementation of the server_draw_text function below is a small subset of possibilities text can be packed (detecting duplicated strings). See MS-RDPEGDI 2.2.2.2.1.1.2.13 GlyphIndex (GLYPHINDEX_ORDER)
// TODO: is it still used ? If yes move it somewhere else. Method from internal mods ?
void server_draw_text(
    GraphicApi & drawable, Font const & font,
    int16_t x, int16_t y, bytes_view text,
    RDPColor fgcolor, RDPColor bgcolor,
    ColorCtx color_ctx,
    Rect clip
) {
    UTF8TextReader reader(text);

    int16_t endx = clip.eright();

    if (reader.has_value() && x <= clip.x) {
        do {
            auto old_state_reader = reader;
            const uint32_t charnum = reader.next();

            Font::FontCharElement font_item = font.item(charnum);
            // if (!font_item.is_valid) {
            //     LOG(LOG_WARNING, "server_draw_text() - character not defined >0x%02x<", charnum);
            // }

            auto nextx = x + font_item.view.offsetx + font_item.view.incby;
            if (nextx > clip.x) {
                reader = old_state_reader;
                break;
            }

            x = nextx;
        } while (reader.has_value());
    }

    while (reader.has_value()) {
        int total_width = 0;
        uint8_t data[256];
        data[1] = 0;
        auto data_begin = std::begin(data);
        const auto data_end = std::end(data)-2;

        const int cacheId = 7;
        while (data_begin < data_end && reader.has_value() && x+total_width <= endx) {
            const uint32_t charnum = reader.next();

            int cacheIndex = 0;
            Font::FontCharElement font_item = font.item(charnum);
            // if (!font_item.is_valid) {
            //     LOG(LOG_WARNING, "server_draw_text() - character not defined >0x%02x<", charnum);
            // }

            const GlyphCache::t_glyph_cache_result cache_result =
                mod_glyph_cache.add_glyph(font_item.view, cacheId, cacheIndex);
            (void)cache_result; // supress warning

            *data_begin++ = cacheIndex;
            *data_begin++ += font_item.view.offsetx;
            data_begin[1] = font_item.view.incby;
            total_width += font_item.view.offsetx + font_item.view.incby;
        }

        Rect bk(x, y, total_width + 2, font.max_height());

        RDPGlyphIndex glyphindex(
            cacheId,            // cache_id
            0x03,               // fl_accel
            0x0,                // ui_charinc
            1,                  // f_op_redundant,
            fgcolor,            // BackColor (text color)
            bgcolor,            // ForeColor (color of the opaque rectangle)
            bk,                 // bk
            bk,                 // op
            // brush
            RDPBrush(0, 0, 3, 0xaa,
                byte_ptr_cast("\xaa\x55\xaa\x55\xaa\x55\xaa\x55")),
            x,                  // glyph_x
            y,                  // glyph_y
            data_begin - data,  // data_len in bytes
            data                // data
        );

        drawable.draw(glyphindex, clip, color_ctx, mod_glyph_cache);

        if (x+total_width > endx) {
            break;
        }
        x += total_width;
    }
}

/// \return last pixel drawn
int draw_text(
    GraphicApi & drawable,
    int x,
    int y,
    uint16_t height,
    array_view<FontCharView const *> fcs,
    RDPColor fgcolor,
    RDPColor bgcolor,
    Rect clip)
{
    auto it = fcs.begin();
    auto end = fcs.end();

    int x_start = clip.x;

    // skip invisible chars
    if (it < end && x <= x_start) {
        do {
            auto nextx = x + (*it)->offsetx + (*it)->incby;
            if (nextx > x_start) {
                break;
            }

            x = nextx;
            ++it;
        } while (it < end);
    }

    if (!(it < end)) {
        return x;
    }

    int x_end = clip.eright();
    LOG(LOG_DEBUG, " -> %d / %d", x, x_end);

    while (it < end) {
        int total_width = 0;
        uint8_t data[256];
        data[1] = 0;
        auto data_it = std::begin(data);
        const auto data_end = std::end(data)-2;
        auto partial_end = it + std::min((end - it), (data_end - data_it));

        const int cacheId = 7;
        for (; it < partial_end && x + total_width <= x_end; ++it) {
            int cacheIndex = 0;

            const GlyphCache::t_glyph_cache_result cache_result =
                mod_glyph_cache.add_glyph(**it, cacheId, cacheIndex);
            (void)cache_result; // supress warning

            *data_it++ = cacheIndex;
            *data_it++ += (*it)->offsetx;
            data_it[1] = (*it)->incby;
            total_width += (*it)->offsetx + (*it)->incby;
        }

        int16_t glyph_x = checked_int(x);
        int16_t glyph_y = checked_int(y);

        // TODO glyph_x => + min(x)
        // TODO height => max_height - min(y) - max(cy)
        Rect bk(glyph_x, glyph_y, checked_int(total_width + 1), height);

        RDPGlyphIndex glyphindex(
            cacheId,  // cache_id
            0x03,     // fl_accel
            0x0,      // ui_charinc
            1,        // f_op_redundant,
            fgcolor,  // BackColor (text color)
            bgcolor,  // ForeColor (color of the opaque rectangle)
            bk,       // bk
            bk,       // op
            RDPBrush(0, 0, 3, 0xaa, byte_ptr_cast("\xaa\x55\xaa\x55\xaa\x55\xaa\x55")),
            glyph_x,
            glyph_y,
            checked_int(data_it - data),
            data
        );

        drawable.draw(glyphindex, clip, gdi::ColorCtx::depth24(), mod_glyph_cache);

        x += total_width;
        if (x > x_end) {
            break;
        }
    }

    return x + 1;
}

} // namespace gdi
