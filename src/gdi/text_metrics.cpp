/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "gdi/text_metrics.hpp"
#include "core/RDP/orders/RDPOrdersCommon.hpp"
#include "core/RDP/orders/RDPOrdersPrimaryGlyphIndex.hpp"
#include "core/RDP/orders/RDPOrdersPrimaryOpaqueRect.hpp"
#include "core/RDP/caches/glyphcache.hpp"
#include "utils/sugar/numerics/safe_conversions.hpp"
#include "utils/utf.hpp"

// new line is a nullptr Char*
static gdi::MultiLineTextMetrics::Line* multi_textmetrics_impl(
    array_view<gdi::MultiLineTextMetrics::Char> fcs,
    FontCharView const* fc_space,
    int const preferred_max_width,
    gdi::MultiLineTextMetrics::Line* line_it,
    int* max_width
) noexcept
{
    auto push_line_and_width = [&](gdi::MultiLineTextMetrics::Line line, int width) {
        // TODO assert(gdi::TextMetrics(font, line).width == width);
        *line_it++ = line;
        *max_width = std::max(width, *max_width);
    };

    int const space_w = fc_space->boxed_width();

    auto fc_it = fcs.begin();
    auto fc_end = fcs.end();

    auto is_newline = [](FontCharView const* fc){ return !fc; };
    auto is_space = [&fc_space](FontCharView const* fc){ return fc == fc_space; };

_start:

    auto* start_line_fc = fc_it;

    // consume spaces and new lines
    for (;;) {
        // left spaces are ignored (empty line)
        if (fc_it == fc_end) {
            return line_it;
        }

        if (is_space(*fc_it)) {
            ++fc_it;
            continue;
        }

        if (is_newline(*fc_it)) {
            *line_it++ = {};
            ++fc_it;
            goto _start;
        }

        break;
    }

_start_at_word:

    auto* start_first_word_fc = fc_it;

    int left_space_width = checked_cast<int>(fc_it - start_line_fc) * space_w;
    int line_width = left_space_width;

    // first word
    for (;;) {
        if (fc_it == fc_end) {
            push_line_and_width({start_line_fc, fc_it}, line_width);
            return line_it;
        }

        if (is_space(*fc_it)) {
            break;
        }

        if (is_newline(*fc_it)) {
            push_line_and_width({start_line_fc, fc_it}, line_width);
            ++fc_it;
            goto _start;
        }

        int w = (*fc_it)->boxed_width();

        // word too long
        if (preferred_max_width < line_width + w) [[unlikely]] {
            if (left_space_width) {
                line_width -= left_space_width;
                left_space_width = 0;

                // insert new line
                *line_it++ = {};
            }

            // alway too long, push partial word
            if (preferred_max_width < line_width + w) {
                if (start_first_word_fc != fc_it) {
                    push_line_and_width({start_first_word_fc, fc_it}, line_width);
                }
                line_width = 0;
                start_line_fc = fc_it;
                start_first_word_fc = fc_it;
            }
        }

        ++fc_it;
        line_width += w;
    }

_word:

    // right space after word
    assert(is_space(*fc_it));
    int line_to_end_word_width = line_width;
    auto* end_word_fc = fc_it;
    while (++fc_it < fc_end && is_space(*fc_it)) {
    }

    auto n_space = fc_it - end_word_fc;
    int sep_width = checked_cast<int>(n_space) * space_w;

    if (fc_it == fc_end || is_newline(*fc_it) || preferred_max_width < line_width + sep_width) {
        push_line_and_width({start_line_fc, end_word_fc}, line_width);
        if (fc_it == fc_end) {
            return line_it;
        }
        if (is_newline(*fc_it)) {
            ++fc_it;
        }
        goto _start;
    }

    line_width += sep_width;

    auto* start_word_fc = fc_it;

    // other words
    for (;;) {
        if (fc_it == fc_end) {
            push_line_and_width({start_line_fc, fc_it}, line_width);
            return line_it;
        }

        if (is_space(*fc_it)) {
            goto _word;
        }

        if (is_newline(*fc_it)) {
            push_line_and_width({start_line_fc, fc_it}, line_width);
            ++fc_it;
            goto _start;
        }

        int w = (*fc_it)->boxed_width();

        // too long
        if (preferred_max_width < line_width + w) [[unlikely]] {
            push_line_and_width({start_line_fc, end_word_fc}, line_to_end_word_width);
            start_line_fc = start_word_fc;
            fc_it = start_word_fc;
            goto _start_at_word;
        }

        ++fc_it;
        line_width += w;
    }
}


namespace gdi
{

TextMetrics::TextMetrics(const Font & font, bytes_view utf8_text)
: height(font.max_height())
{
    auto invalid_char = [&](auto){
        FontCharView const& font_item = font.unknown_glyph();
        width += font_item.boxed_width();
    };
    utf8_for_each(utf8_text,
        [&](uint32_t uc){ width += font.item(uc).view.boxed_width(); },
        invalid_char,
        invalid_char
    );
}

MultiLineTextMetrics::MultiLineTextMetrics(
    const Font& font, unsigned preferred_max_width, bytes_view utf8_text)
{
    set_text(font, utf8_text);
    compute_lines(preferred_max_width);
}

void MultiLineTextMetrics::set_text(Font const& font, bytes_view utf8_text)
{
    clear_text();

    auto max_cap = ~decltype(d.char_capacity){} - 1 /* remove reserved space fc */;

    if (utf8_text.empty() || utf8_text.size() > max_cap) {
        return;
    }

    /*
     * Allocate buffer
     */

    if (utf8_text.size() > d.char_capacity) {
        free(d.data);

        static_assert(alignof(Line) == alignof(Char));

        auto data_len = utf8_text.size() * (sizeof(Char) + sizeof(Line))
            + sizeof(Char) /* reserved for space fc */;
        d.data = aligned_alloc(alignof(Line), data_len);
        if (!d.data) {
            // insuffisant memory, ignore error
            d = {};
            return;
        }

        d.char_capacity = checked_int{utf8_text.size()};
    }

    /*
     * Text to FontChar
     */

    auto* chars_buf = static_cast<Char*>(d.data);
    auto ch_it = chars_buf;

    *ch_it++ = &font.item(' ').view;

    auto invalid_char = [&](auto){ *ch_it++ = &font.unknown_glyph(); };
    utf8_for_each(
        utf8_text,
        [&](uint32_t uc){
            if (uc != '\n') [[likely]] {
                *ch_it++ = &font.item(uc).view;
            }
            else {
                *ch_it++ = nullptr;
            }
        },
        invalid_char,
        invalid_char
    );

    d.nb_chars = checked_int{ch_it - chars_buf};
}

void MultiLineTextMetrics::compute_lines(unsigned preferred_max_width) noexcept
{
    if (!d.nb_chars) {
        return;
    }

    if (!preferred_max_width) {
        d.max_width = 0;
        d.nb_line = 0;
        return;
    }

    auto* ch_it = static_cast<Char*>(d.data);
    auto* ch_end = ch_it + d.nb_chars;
    auto* line_it = reinterpret_cast<Line*>(ch_end);  /* NOLINT */
    auto* sp = *ch_it++;
    int max_width = 0;
    auto* end = multi_textmetrics_impl(
        {ch_it, ch_end},
        sp,
        checked_int(preferred_max_width - 1),  // re-add after
        line_it,
        &max_width
    );

    // otherwise, the last column of some characters are truncated :/
    if (max_width) {
        ++max_width;
    }

    d.max_width = checked_int(max_width);
    d.nb_line = checked_int(end - line_it);
}

void MultiLineTextMetrics::clear_text() noexcept
{
    d.nb_line = 0;
    d.nb_chars = 0;
    d.max_width = 0;
}

array_view<MultiLineTextMetrics::Line> MultiLineTextMetrics::lines() const noexcept
{
    auto* ch_it = static_cast<Char*>(d.data);
    auto* line_it = reinterpret_cast<Line*>(ch_it + d.nb_chars);  /* NOLINT */
    return {line_it, d.nb_line};
}

MultiLineTextMetrics::~MultiLineTextMetrics()
{
    free(d.data);
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
    uint16_t max_height_text,
    DrawTextPadding padding,
    array_view<FontCharView const *> fcs,
    RDPColor fgcolor,
    RDPColor bgcolor,
    Rect clip)
{
    auto it = fcs.begin();
    auto end = fcs.end();

    x += padding.left;
    int x_start = clip.x;

    // skip invisible chars
    if (it < end && x < x_start) {
        do {
            auto nextx = x + (*it)->boxed_width();
            if (nextx >= x_start) {
                break;
            }

            x = nextx;
            ++it;
        } while (it < end);
    }

    if (!(it < end)) {
        if (int w = fcs.empty() ? padding.left + padding.right : padding.right) {
            int px = x;
            if (fcs.empty()) {
                px -= padding.left;
            }
            else {
                w += fcs.back()->incby - fcs.back()->width;
            }
            Rect rect(
                checked_int(px),
                checked_int(y),
                checked_int(w),
                checked_int(max_height_text + padding.top + padding.bottom)
            );
            drawable.draw(RDPOpaqueRect(rect, bgcolor), clip, gdi::ColorCtx::depth24());
            return rect.intersect(clip).eright();
        }
        return x - padding.left;
    }

    y += padding.top;

    int x_end = clip.eright();

    while (it < end) {
        int total_width = 0;
        uint8_t data[256];
        data[1] = 0;
        auto data_it = std::begin(data);
        auto data_end = it + std::min(end - it, (std::end(data) - 2 - data_it) / 2);

        const int cacheId = 7;
        for (; it < data_end && x + total_width <= x_end; ++it) {
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

        Rect bk(
            checked_int(glyph_x - padding.left),
            checked_int(glyph_y - padding.top),
            checked_int(total_width + 2 + padding.left + padding.right), // TODO last loop only
            checked_int(max_height_text + 1 + padding.top + padding.bottom)
        );
        // for freerdp because clip is ignored
        // auto bk = clip.intersect(real_bk);
        // glyph_x += bk.x - real_bk.x;
        // glyph_y += bk.y - real_bk.y;

        RDPGlyphIndex glyphindex(
            cacheId,  // cache_id
            0x03,     // fl_accel
            0x0,      // ui_charinc
            0,        // f_op_redundant,
            fgcolor,  // BackColor (text color)
            bgcolor,  // ForeColor (color of the opaque rectangle)
            bk,       // bk
            bk,       // op
            RDPBrush(0, 0, 0, 0),
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

        padding.left = 0;
    }

    return x + 1 + padding.right;
}

} // namespace gdi
