/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "utils/image_view.hpp"

namespace gdi
{
    class GraphicApi;
    class ImageFrameApi;
}

class Drawable;

struct TestGraphic
{
    TestGraphic(uint16_t w, uint16_t h);
    ~TestGraphic();

    [[nodiscard]] uint16_t width() const;
    [[nodiscard]] uint16_t height() const;

    operator ImageView () const;
    operator gdi::GraphicApi& ();
    gdi::GraphicApi* operator->();

    Drawable const& drawable() const;

    void resize(uint16_t w, uint16_t h);

    WritableImageView get_writable_image_view();

    void draw_rect(Rect rect, BGRColor color);

private:
    class D;
    D* d;
};
