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
 *   Copyright (C) Wallix 2010-2012
 *   Author(s): Christophe Grosjean, Dominique Lafages, Jonathan Poelen,
 *              Meng Tan
 */

#include "core/RDP/orders/RDPOrdersPrimaryOpaqueRect.hpp"
#include "core/font.hpp"
#include "mod/internal/widget/label.hpp"
#include "mod/internal/copy_paste.hpp"
#include "gdi/graphic_api.hpp"
#include "gdi/text_metrics.hpp"
#include "utils/sugar/cast.hpp"
#include "utils/utf.hpp"


[[nodiscard]]
array_view<FontCharView const *> init_widget_text(
    writable_array_view<FontCharView const *> fcs,
    OutParam<uint16_t> width,
    Font const & font,
    chars_view text
)
{
    int w = 1;
    auto out = std::begin(fcs);
    auto end_out = std::end(fcs);
    utf8_for_each(
        text,
        [&](uint32_t uc) {
            auto const * fc = &font.item(uc).view;
            w += fc->boxed_width();
            *out++ = fc;
        },
        [](utf8_char_invalid) {},
        [](utf8_char_truncated) {},
        [&]{ return out < end_out; }
    );

    width.out_value = checked_int(w);
    return fcs.before(out);
}


WidgetLabel::WidgetLabel(
    gdi::GraphicApi & drawable, chars_view text,
    Color fgcolor, Color bgcolor, Font const & font,
    int xtext, int ytext
)
    : Widget(drawable, Focusable::No)
    , x_text(xtext)
    , y_text(ytext)
    , bg_color(bgcolor)
    , fg_color(fgcolor)
    , font(&font)
{
    this->set_text(text);
}

void WidgetLabel::set_text(chars_view text)
{
    this->buffer[0] = 0;
    if (!text.empty()) {
        const size_t remain_n = buffer_size - 1;
        const size_t max = ((remain_n >= text.size()) ? text.size() :
                            UTF8StringAdjustedNbBytes(text, remain_n));
        memcpy(this->buffer, text.data(), max);
        this->buffer[max] = 0;
    }
}

chars_view WidgetLabel::get_text() const
{
    return std::string_view(this->buffer);
}

void WidgetLabel::rdp_input_invalidate(Rect clip)
{
    Rect rect_intersect = clip.intersect(this->get_rect());

    if (!rect_intersect.isempty()) {
        this->draw(
            rect_intersect, this->get_rect(), this->drawable, std::string_view(this->buffer),
            this->fg_color, this->bg_color, gdi::ColorCtx::depth24(),
            *this->font, this->x_text, this->y_text);
    }
}

void WidgetLabel::draw(
    Rect const clip, Rect const rect, gdi::GraphicApi& drawable,
    chars_view text, Color fgcolor, Color bgcolor, gdi::ColorCtx color_ctx,
    Font const & font, int xtext, int ytext)
{
    drawable.draw(RDPOpaqueRect(rect, bgcolor), clip, color_ctx);
    gdi::server_draw_text(
        drawable, font,
        xtext + rect.x, ytext + rect.y,
        text, fgcolor, bgcolor,
        color_ctx, rect.intersect(clip)
    );
}

Dimension WidgetLabel::get_optimal_dim() const
{
    int width = this->buffer[0] ? gdi::TextMetrics(*this->font, std::string_view(this->buffer)).width : 0;
    return Dimension(width + this->x_text * 2, this->font->max_height() + this->y_text * 2);
}

Dimension WidgetLabel::get_optimal_dim(Font const & font, chars_view text, int xtext, int ytext)
{
    size_t len = 0;

    if (!text.empty()) {
        const size_t remain_n = buffer_size - 1;
        const size_t n = text.size();
        const size_t max = ((remain_n >= n) ? n :
                            ::UTF8StringAdjustedNbBytes(text, remain_n));
        len = max;
    }

    int width = gdi::TextMetrics(font, text.first(len)).width;
    return Dimension(width + xtext * 2, font.max_height() + ytext * 2);
}

void WidgetLabel::set_color(Color bg_color, Color fg_color)
{
    this->bg_color = bg_color;
    this->fg_color = fg_color;
}
