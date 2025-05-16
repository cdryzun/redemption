/*
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 1 of the License, or
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
 *   Copyright (C) Wallix 2010-2014
 *   Author(s): Christophe Grosjean, Dominique Lafages, Jonathan Poelen,
 *              Meng Tan
 */

#pragma once

#include "mod/internal/widget/widget.hpp"
#include "mod/internal/widget/event_notifier.hpp"
#include "utils/sugar/bytes_view.hpp"
#include "utils/static_string.hpp"
#include "utils/utf.hpp"

class Font;
class CopyPaste;
class FontCharView;
class Theme;

class WidgetEdit : public Widget
{
public:
    static constexpr int max_capacity = 255;

    struct Colors
    {
        Color fg;
        Color bg;
        Color border;
        Color focus_border;
        Color cursor;

        static Colors from_theme(Theme const& theme) noexcept;
    };

    enum class Redraw : bool { No, Yes, };

    enum class CusorPosition : uint8_t
    {
        CursorToEnd,
        CursorToBegin,
        KeepCursorPosition,
    };

    struct TextOptions
    {
        Redraw redraw;
        CusorPosition cursor_position = CusorPosition::CursorToEnd;
    };

    using Text = static_string<max_capacity * 4>;

    WidgetEdit(
        gdi::GraphicApi & gd, Font const & font, CopyPaste & copy_paste,
        Colors colors, WidgetEventNotifier onsubmit
    );

    WidgetEdit(
        gdi::GraphicApi & gd, Font const & font, CopyPaste & copy_paste,
        chars_view text, Colors colors, WidgetEventNotifier onsubmit
    );

    ~WidgetEdit();

    static uint16_t x_padding() noexcept;

    bool has_text() const noexcept;
    Text get_text() const noexcept;
    /// \result unspecifed behavior when text is not a number
    unsigned get_text_as_uint() const noexcept;

    Font const& get_font() const noexcept { return *font; }
    Colors get_colors() const noexcept { return colors; }

    void set_text(bytes_view text, TextOptions opts);

    void update_width(uint16_t width);

    void insert_chars(array_view<uint32_t> ucs, Redraw redraw);

    void rdp_input_invalidate(Rect clip) override;

    void rdp_input_scancode(
        KbdFlags flags, Scancode scancode,
        uint32_t event_time, Keymap const& keymap) override;

    void rdp_input_unicode(KbdFlags flag, uint16_t unicode) override;

    void rdp_input_mouse(uint16_t device_flags, uint16_t x_, uint16_t y) override;

    void clipboard_insert_utf8(zstring_view text) override;

    void focus() override;

    void blur() override;

    void set_font(Font const & font, Redraw redraw);

    bool action_move_cursor_right(bool ctrl_is_pressed, Redraw redraw);
    bool action_move_cursor_left(bool ctrl_is_pressed, Redraw redraw);
    bool action_move_cursor_to_begin_of_line(Redraw redraw);
    bool action_move_cursor_to_end_of_line(Redraw redraw);
    bool action_backspace(bool ctrl_is_pressed, Redraw redraw);
    bool action_delete(bool ctrl_is_pressed, Redraw redraw);
    bool action_remove_right(Redraw redraw);
    bool action_remove_left(Redraw redraw);

    void submit()
    {
        onsubmit();
    }

    void draw_current_cursor(Rect clip)
    {
        draw_cursor(clip, colors.cursor);
    }

    void hidden_current_cursor(Rect clip)
    {
        draw_cursor(clip, colors.bg);
    }

private:
    struct D;

    using FontCharPtr = FontCharView const *;

    struct BufferData
    {
        // first char is a reserved empty char used as sentinel
        // (see action_move_cursor_right and get_previous_char)
        static constexpr std::ptrdiff_t reserved_index = 1;

        using Unicode32 = uint32_t;

        FontCharPtr * current_fc_it;
        FontCharPtr * end_fc_it;
        Unicode32 * current_unicode_it;
        FontCharPtr fc_buffer[max_capacity + reserved_index];
        Unicode32 unicode_buffer[max_capacity + reserved_index];

        BufferData() noexcept {}
        BufferData(BufferData const&) = delete;
        BufferData& operator=(BufferData const&) = delete;
    };

    struct Buffer;

    Buffer & buffer() noexcept;
    Buffer const & buffer() const noexcept;

    int get_end_pos() const;

    Rect cursor_rect(int x_cursor) noexcept;
    void draw_border(Rect clip, Color color);
    void draw_cursor(Rect clip, Color color);
    void draw_inner(Rect clip);
    void draw_text(Rect clip);

    void draw_rect(Rect rect, Widget::Color color);

    struct RedrawInfo
    {
        bool partial_update;
        FontCharPtr const * it;  // no null when partial_update is true
        int x_cursor;
    };

    int redraw_text(RedrawInfo redraw_info);

    void redraw_cursor(int old_x_cursor);
    void redraw_removed_right_text(bool partial_update, int shift);
    void redraw_right_empty_padding();

    void maybe_redraw_left_empty_padding(int old_x_text);

    bool move_cursor_to_right(int shift);
    void move_cursor_to_right_and_redraw(int shift, Redraw redraw);
    bool move_cursor_to_left(int shift);
    void move_cursor_to_left_and_redraw(int shift, Redraw redraw);

    void remove_right(FontCharPtr const* old_position, int shift, Redraw redraw);
    void remove_left(FontCharPtr const* old_position, int shift, Redraw redraw);


    int x_cursor;
    int x_text;
    Colors colors;
    Font const * font;

    uint16_t h_text;
    Utf16ToUnicodeConverter unicode32_decoder;
    WidgetEventNotifier onsubmit;

    BufferData editable_buffer;

protected:
    CopyPaste & copy_paste;
};
