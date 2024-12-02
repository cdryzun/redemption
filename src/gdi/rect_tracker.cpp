#include "core/RDP/bitmapupdate.hpp"
#include "utils/bitmap.hpp"

#include "gdi/rect_tracker.hpp"

void RectTracker::draw(RDPBitmapData const &cmd, Bitmap const &bmp)
{
    const uint16_t dw = cmd.dest_right - cmd.dest_left + 1;
    const uint16_t dh = cmd.dest_bottom - cmd.dest_top + 1;
    this->rect = rect.disjunct(Rect(
        cmd.dest_left,
        cmd.dest_top,
        std::min(bmp.cx(), dw),
        std::min(bmp.cy(), dh)
    ));
}