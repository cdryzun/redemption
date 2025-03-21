/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "mod/internal/widget/group_box.hpp"
#include "core/RDP/orders/RDPOrdersPrimaryOpaqueRect.hpp"
#include "core/font.hpp"
#include "gdi/graphic_api.hpp"
#include "gdi/text_metrics.hpp"
#include "utils/sugar/cast.hpp"
#include "utils/utf.hpp"


WidgetGroupBox::WidgetGroupBox(
    gdi::GraphicApi & drawable, chars_view text,
    Color fgcolor, Color bgcolor, Font const & font
)
  : WidgetComposite(drawable, Focusable::Yes)
  , fg_color(fgcolor)
  , line_height(font.max_height())
  , caption(font, text)
{
    this->set_bg_color(bgcolor);
}

void WidgetGroupBox::rdp_input_invalidate(Rect clip)
{
    Rect rect_intersect = clip.intersect(this->get_rect());

    if (!rect_intersect.isempty()) {
        this->draw_inner_free(rect_intersect, this->get_bg_color());

        // Box.
        const uint16_t border           = 6;
        const uint16_t text_margin      = 6;
        const uint16_t text_indentation = border + text_margin + 4;

        // Top Line and Label
        gdi::draw_text(
            this->drawable,
            this->x() + text_indentation,
            this->y(),
            line_height,
            gdi::DrawTextPadding{}/*::Horizontal{text_indentation}*/,
            this->caption.fcs(),
            this->fg_color,
            this->get_bg_color(),
            rect_intersect
        );

        auto draw_rect = [&](int x, int y, int w, int h){
            auto rect = Rect(
                checked_int{x},
                checked_int{y},
                checked_int{w},
                checked_int{h}
            );
            this->drawable.draw(
                RDPOpaqueRect(rect, this->fg_color),
                rect_intersect, gdi::ColorCtx::depth24()
            );
        };

        auto wlabel = text_margin * 2 + caption.width();
        auto y = this->y() + line_height / 2;
        auto gcy = this->cy() - line_height / 2 - border;
        auto gcx = this->cx() - border * 2 + 1;
        auto px = this->x() + border - 1;

        // Top line (left to text)
        draw_rect(px, y, text_indentation - text_margin - border + 2, 1);
        // Top line (right to text)
        auto right_cx = gcx + 1 - wlabel - 4;
        if (right_cx > 0) {
            draw_rect(px + wlabel + 3, y, right_cx, 1);
        }

        // Bottom line
        draw_rect(px, y + gcy, gcx + 1, 1);

        // Left border
        draw_rect(px, y + 1, 1, gcy - 1);

        // Right Border
        draw_rect(px + gcx, y, 1, gcy);

        this->invalidate_children(rect_intersect);
    }
}
