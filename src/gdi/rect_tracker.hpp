#pragma once

#include "gdi/clip_from_cmd.hpp"
#include "gdi/graphic_api.hpp"

class RectTracker final : public gdi::GraphicApi
{
public:
    RectTracker(uint16_t width, uint16_t height)
        : rect(0,0,width,height)
        , width(width)
        , height(height)
    {
    }

    using gdi::GraphicApi::draw;

    void draw(RDPDstBlt           const & cmd, Rect clip) override { this->rect = clip_from_cmd(cmd).intersect(clip).disjunct(rect); }
    void draw(RDPMultiDstBlt      const & cmd, Rect clip) override { this->rect = clip_from_cmd(cmd).intersect(clip).disjunct(rect); }
    void draw(RDPPatBlt           const & cmd, Rect clip, gdi::ColorCtx /*color_ctx*/) override { this->rect = clip_from_cmd(cmd).intersect(clip).disjunct(rect); }
    void draw(RDP::RDPMultiPatBlt const & cmd, Rect clip, gdi::ColorCtx /*color_ctx*/) override { this->rect = clip_from_cmd(cmd).intersect(clip).disjunct(rect); }
    void draw(RDPOpaqueRect       const & cmd, Rect clip, gdi::ColorCtx /*color_ctx*/) override { this->rect = clip_from_cmd(cmd).intersect(clip).disjunct(rect); }
    void draw(RDPMultiOpaqueRect  const & cmd, Rect clip, gdi::ColorCtx /*color_ctx*/) override { this->rect = clip_from_cmd(cmd).intersect(clip).disjunct(rect); }
    void draw(RDPScrBlt           const & cmd, Rect clip) override { this->rect = clip_from_cmd(cmd).intersect(clip).disjunct(rect); }
    void draw(RDP::RDPMultiScrBlt const & cmd, Rect clip) override { this->rect = clip_from_cmd(cmd).intersect(clip).disjunct(rect); }
    void draw(RDPLineTo           const & cmd, Rect clip, gdi::ColorCtx /*color_ctx*/) override { this->rect = clip_from_cmd(cmd).intersect(clip).disjunct(rect); }
    void draw(RDPPolygonSC        const & cmd, Rect clip, gdi::ColorCtx /*color_ctx*/) override { this->rect = clip_from_cmd(cmd).intersect(clip).disjunct(rect); }
    void draw(RDPPolygonCB        const & cmd, Rect clip, gdi::ColorCtx /*color_ctx*/) override { this->rect = clip_from_cmd(cmd).intersect(clip).disjunct(rect); }
    void draw(RDPPolyline         const & cmd, Rect clip, gdi::ColorCtx /*color_ctx*/) override { this->rect = clip_from_cmd(cmd).intersect(clip).disjunct(rect); }
    void draw(RDPEllipseSC        const & cmd, Rect clip, gdi::ColorCtx /*color_ctx*/) override { this->rect = clip_from_cmd(cmd).intersect(clip).disjunct(rect); }
    void draw(RDPEllipseCB        const & cmd, Rect clip, gdi::ColorCtx /*color_ctx*/) override { this->rect = clip_from_cmd(cmd).intersect(clip).disjunct(rect); }
    void draw(RDPBitmapData       const & cmd, Bitmap const & bmp) override;

    void draw(RDPMemBlt           const & cmd, Rect clip, Bitmap const & /*bmp*/) override { this->rect = clip_from_cmd(cmd).intersect(clip).disjunct(rect); }
    void draw(RDPMem3Blt          const & cmd, Rect clip, gdi::ColorCtx /*color_ctx*/, Bitmap const & /*bmp*/) override { this->rect = clip_from_cmd(cmd).intersect(clip).disjunct(rect); }

    void draw(RDPGlyphIndex       const & cmd, Rect clip, gdi::ColorCtx /*color_ctx*/, GlyphCache const & /*gly_cache*/) override { this->rect = clip_from_cmd(cmd).intersect(clip).disjunct(rect); }

    void draw(RDP::FrameMarker const & cmd) override
    {
        (void)cmd;
    }

    void draw(RDPSetSurfaceCommand const & cmd) override
    {
        (void)cmd;
        this->rect = Rect(0, 0, width, height);
    }

    void draw(RDPSetSurfaceCommand const & cmd, RDPSurfaceContent const & content) override
    {
        (void)cmd;
        (void)content;
        this->rect = Rect(0, 0, width, height);
    }

    void draw(const RDP::RAIL::NewOrExistingWindow & cmd) override
    {
        (void)cmd;
    }

    void draw(const RDP::RAIL::WindowIcon & cmd) override
    {
        (void)cmd;
    }

    void draw(const RDP::RAIL::CachedIcon & cmd) override
    {
        (void)cmd;
    }

    void draw(const RDP::RAIL::DeletedWindow & cmd) override
    {
        (void)cmd;
    }

    void draw(const RDP::RAIL::NewOrExistingNotificationIcons & cmd) override
    {
        (void)cmd;
    }

    void draw(const RDP::RAIL::DeletedNotificationIcons & cmd) override
    {
        (void)cmd;
    }

    void draw(const RDP::RAIL::ActivelyMonitoredDesktop & cmd) override
    {
        (void)cmd;
    }

    void draw(const RDP::RAIL::NonMonitoredDesktop & cmd) override
    {
        (void)cmd;
    }

    void draw(RDPColCache const & cmd) override
    {
        (void)cmd;
    }

    void draw(RDPBrushCache const & cmd) override
    {
        (void)cmd;
    }

    void cached_pointer(gdi::CachePointerIndex cache_idx) override
    {
        (void)cache_idx;
        this->rect = Rect(0, 0, width, height);
    }

    void new_pointer(gdi::CachePointerIndex cache_idx, RdpPointerView const& cursor) override
    {
        (void)cache_idx;
        (void)cursor;
    }

    void set_palette(BGRPalette const & palette) override
    {
        (void)palette;
    }

    void sync() override
    {}

    void set_row(std::size_t rownum, bytes_view data) override
    {
        (void)rownum;
        (void)data;
        this->rect = Rect(0, 0, width, height);
    }

    Rect get_rect() const
    {
        return this->rect;
    }

    void set_area(uint16_t width, uint16_t height)
    {
        this->rect = Rect(0, 0, width, height);
    }

    bool has_drawing_event() const
    {
        return !this->rect.isempty();
    }

    void reset()
    {
        this->rect = Rect();
    }

private:
    Rect rect;
    uint16_t width;
    uint16_t height;
};