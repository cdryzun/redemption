/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "test_only/gdi/test_graphic.hpp"

#include "core/RDP/RDPDrawable.hpp"
#include "core/RDP/orders/RDPOrdersPrimaryOpaqueRect.hpp"

struct TestGraphic::D
{
    RDPDrawable gd;
};

TestGraphic::TestGraphic(uint16_t w, uint16_t h)
  : d(new D{{w, h}})
{}

TestGraphic::~TestGraphic()
{
    delete d;
}

uint16_t TestGraphic::width() const
{
    return this->d->gd.width();
}

uint16_t TestGraphic::height() const
{
    return this->d->gd.height();
}

TestGraphic::operator ImageView() const
{
    return this->d->gd;
}

TestGraphic::operator gdi::GraphicApi&()
{
    return this->d->gd;
}

Drawable const& TestGraphic::drawable() const
{
    return this->d->gd.impl();
}

gdi::GraphicApi* TestGraphic::operator->()
{
    return &this->d->gd;
}

void TestGraphic::resize(uint16_t w, uint16_t h)
{
    this->d->gd.resize(w, h);
}

WritableImageView TestGraphic::get_writable_image_view()
{
    return gdi::get_writable_image_view(this->d->gd);
}

void TestGraphic::draw_rect(Rect rect, BGRColor color)
{
    this->d->gd.draw(
        RDPOpaqueRect(rect, encode_color24()(color)),
        rect, gdi::ColorCtx::depth24()
    );
}
