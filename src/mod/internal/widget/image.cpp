/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "mod/internal/widget/image.hpp"
#include "utils/bitmap_from_file.hpp"
#include "utils/sugar/numerics/safe_conversions.hpp"
#include "gdi/graphic_api.hpp"
#include "core/RDP/bitmapupdate.hpp"

WidgetImage::WidgetImage(gdi::GraphicApi& drawable, const char *filename, Color bg_color)
    : Widget(drawable, Focusable::No)
    , bmp(bitmap_from_file(filename, bg_color))
    , last_bmp(bmp)
    , last_rect(-1, -1, 1, 1)  // "invalid" position for update to first draw
{
    set_wh(bmp.cx(), bmp.cy());
}

void WidgetImage::rdp_input_invalidate(Rect clip)
{
    Rect rect_intersect = clip.intersect(this->get_rect()).to_positive();

    auto draw_bitmap = [](gdi::GraphicApi & drawable, Bitmap const & bitmap, Rect const & rect) {
        RDPBitmapData bitmap_data;

        bitmap_data.dest_left       = checked_int{rect.x};
        bitmap_data.dest_top        = checked_int{rect.y};
        bitmap_data.dest_right      = checked_int{rect.x + rect.cx - 1};
        bitmap_data.dest_bottom     = checked_int{rect.y + rect.cy - 1};
        bitmap_data.width           = bitmap.cx();
        bitmap_data.height          = bitmap.cy();
        bitmap_data.bits_per_pixel  = safe_int(bitmap.bpp());
        bitmap_data.flags           = 0;
        bitmap_data.bitmap_length   = bitmap.bmp_size();  // TODO BUG overflow, but not used in Front...

        drawable.draw(bitmap_data, bitmap);
    };

    if (!rect_intersect.isempty()) {
        if (this->last_rect != rect_intersect) {
            this->last_rect = rect_intersect;
            if (rect_intersect != this->get_rect() || this->bmp.cx() % 4) {
                this->last_bmp = Bitmap(
                    this->bmp,
                    Rect(
                        rect_intersect.x - this->x(),
                        rect_intersect.y - this->y(),
                        align4(rect_intersect.cx),
                        rect_intersect.cy
                    )
                );
            }
            else {
                this->last_bmp = this->bmp;
            }
        }
        draw_bitmap(this->drawable, this->last_bmp, rect_intersect);
    }
}
