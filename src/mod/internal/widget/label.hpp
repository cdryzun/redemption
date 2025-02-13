/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "mod/internal/widget/widget.hpp"
#include "utils/sugar/array_view.hpp"
#include "utils/out_param.hpp"


class Font;
class FontCharView;
class Theme;
namespace gdi
{
    class ColorCtx;
}

[[nodiscard]]
array_view<FontCharView const *> init_widget_text(
    writable_array_view<FontCharView const *> fcs,
    OutParam<uint16_t> width,
    Font const & font,
    chars_view text
);

template<std::size_t BufSize>
class WidgetText
{
public:
    using FontCharPtr = FontCharView const *;

    WidgetText() noexcept
    {}

    WidgetText(Font const & font, chars_view text)
    {
        set_text(font, text);
    }

    void set_text(Font const & font, chars_view text)
    {
        _fc_buffer_len = checked_int(
            init_widget_text(make_writable_array_view(_fc_buffer), OutParam(_width), font, text)
            .size()
        );
    }

    uint16_t width() const noexcept
    {
        return _width;
    }

    array_view<FontCharPtr> fcs() const noexcept
    {
        return {_fc_buffer, _fc_buffer_len};
    }

private:
    uint16_t _width = 0;
    unsigned _fc_buffer_len = 0;
    FontCharPtr _fc_buffer[BufSize];
};


class WidgetLabel : public Widget
{
public:
    struct Colors
    {
        Color fg;
        Color bg;

        static Colors from_theme(Theme const& theme) noexcept;
    };

    WidgetLabel(gdi::GraphicApi & drawable, Font const & font, chars_view text, Colors colors);

    bool is_empty() const noexcept
    {
        return text_label.fcs().empty();
    }

    void set_text(Font const & font, chars_view text);

    void rdp_input_invalidate(Rect clip) override;

public:
    static const size_t buffer_size = 256;

    Colors colors;

    WidgetText<buffer_size> text_label;
};
