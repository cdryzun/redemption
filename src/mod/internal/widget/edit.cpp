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

#include "core/RDP/orders/RDPOrdersPrimaryOpaqueRect.hpp"
#include "core/font.hpp"
#include "gdi/draw_utils.hpp"
#include "gdi/text.hpp"
#include "keyboard/keymap.hpp"
#include "mod/internal/widget/edit.hpp"
#include "mod/internal/copy_paste.hpp"
#include "utils/colors.hpp"
#include "utils/sugar/cast.hpp"
#include "utils/theme.hpp"


struct WidgetEdit::D
{
    static const uint16_t w_cursor = 1;
    static const uint16_t h_padding = 2;
    static const uint16_t w_padding = 2;
    static const uint16_t border_len = 1;
    static const uint16_t w_cursor_padding = 1;
    static const uint16_t start_x_cursor = w_padding + border_len + w_cursor_padding;

    static constexpr FontCharView empty_ch {};

    static Dimension compute_optimal_dim(int w_text, int h_text) noexcept
    {
        return Dimension{
            checked_int(start_x_cursor * 2 + w_cursor + w_text),
            checked_int((h_padding + border_len) * 2 + h_text),
        };
    }

    static int fc_width(FontCharView const * fc) noexcept
    {
        return fc->boxed_width();
    }

    template<class F>
    static auto sanitized_char(F&& f)
    {
        return [f](uint32_t uc) {
            if (uc >= ' ') {
                // ok
            }
            else if (uc == '\n' || uc == '\t') {
                uc = ' ';
            }
            else {
                // char is ignored
                return ;
            }
            f(uc);
        };
    }
};


/*
        ~                                              ~ - border_len
         ~~~~~                                    ~~~~~  - w_padding
              ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~       - edit zone
        +----+------------------------------------+----+ < y_padding
        |    |               ~~ w_cursor          |    | <
        |    +-+----------+-+##----------+-+-+----------+-+-+----------+-+
        |    |      aa      |##    aa      |      aa      |      aa      |
        |    |     aaaa     |##   aaaa     |     aaaa     |     aaaa     |
        |    |    aa  aa    |##  aa  aa    |    aa  aa    |    aa  aa    |
        |    |   aaaaaaaa   |## aaaaaaaa   |   aaaaaaaa   |   aaaaaaaa   |
        |    |  aa      aa  |##aa      aa  |  aa      aa  |  aa      aa  |
        |    +-+----------+-+##----------+-+-+----------+-+-+----------+-+
        |    ^ x_text = 0    ^ x_cursor (after previous char)
        +----+------------------------------------+----+

x_cursor = border_len + w_padding + w_cursor_padding + shift


cursor 2 times to the right:                    ~~~~~~~~~~~ x_text increment
        +----+------------------------------------+----+
        |    |                                    |    |   ## <-- cursor before
  +-+----------+-+-+----------+-+-+----------+-+##----------+-+   repositioning
  |      aa      |      aa      |      aa      |##    aa      |
  |     aaaa     |     aaaa     |     aaaa     |##   aaaa     |
  |    aa  aa    |    aa  aa    |    aa  aa    |##  aa  aa    |
  |   aaaaaaaa   |   aaaaaaaa   |   aaaaaaaa   |## aaaaaaaa   |
  |  aa      aa  |  aa      aa  |  aa      aa  |##aa      aa  |
  +-+----------+-+-+----------+-+-+----------+-+##----------+-+
        |    |                                    |    |
        +----+------------------------------------+----+
  ~~~~~~~~~~~~ x_text
*/

class WidgetEdit::Buffer : private WidgetEdit::BufferData
{
    FontCharPtr * begin_fc_buffer() noexcept
    {
        return fc_buffer + reserved_index;
    }

    FontCharPtr const * begin_fc_buffer() const noexcept
    {
        return fc_buffer + reserved_index;
    }

    Unicode32 * begin_unicode_buffer() noexcept
    {
        return unicode_buffer + reserved_index;
    }

    Unicode32 const * begin_unicode_buffer() const noexcept
    {
        return unicode_buffer + reserved_index;
    }

    template<class T>
    void mem_move(T* to, array_view<T> from)
    {
        std::memmove(to, from.data(), sizeof(T) * from.size());
    }

public:
    void init() noexcept
    {
        current_fc_it = begin_fc_buffer();
        end_fc_it = current_fc_it;
        current_unicode_it = begin_unicode_buffer();
        fc_buffer[0] = &D::empty_ch;
        unicode_buffer[0] = 0;
    }

    static Buffer & from_data(BufferData & data) noexcept
    {
        return static_cast<Buffer &>(data);
    }

    static Buffer const & from_data(BufferData const & data) noexcept
    {
        return static_cast<Buffer const &>(data);
    }

    bool is_full() const noexcept
    {
        return end_fc_it == std::end(fc_buffer);
    }

    std::size_t remaining() const noexcept
    {
        return checked_int(std::end(fc_buffer) - end_fc_it);
    }

    bool is_movable_to_left() const noexcept
    {
        return current_fc_it > begin_fc_buffer();
    }

    /// \return width of char
    int move_to_left() noexcept
    {
        assert(is_movable_to_left());
        --current_fc_it;
        --current_unicode_it;
        return D::fc_width(*current_fc_it);
    }

    bool is_movable_to_right() const noexcept
    {
        return current_fc_it < end_fc_it;
    }

    /// \return width of char
    int move_to_right() noexcept
    {
        assert(is_movable_to_right());
        int w = D::fc_width(*current_fc_it);
        ++current_fc_it;
        ++current_unicode_it;
        return w;
    }

    void move_to_start() noexcept
    {
        current_fc_it = begin_fc_buffer();
        current_unicode_it = begin_unicode_buffer();
    }

    Unicode32 get_current_unicode() const noexcept
    {
        assert(current_fc_it != end_fc_it);
        return *current_unicode_it;
    }

    FontCharView const& get_current_fc() const noexcept
    {
        assert(current_fc_it != end_fc_it);
        return **current_fc_it;
    }

    Unicode32 get_previous_unicode() const noexcept
    {
        assert(current_fc_it != std::begin(fc_buffer));
        return *(current_unicode_it - 1);
    }

    FontCharView const& get_previous_fc() const noexcept
    {
        assert(current_fc_it != std::begin(fc_buffer));
        return **(current_fc_it - 1);
    }

    array_view<Unicode32> text() const noexcept
    {
        return {begin_unicode_buffer(), font_chars().size()};
    }

    array_view<FontCharPtr> font_chars() const noexcept
    {
        return {begin_fc_buffer(), end_fc_it};
    }

    writable_array_view<FontCharPtr> writable_font_chars() noexcept
    {
        return writable_array_view{begin_fc_buffer(), end_fc_it};
    }

    FontCharPtr const * current() const noexcept
    {
        return current_fc_it;
    }

    std::size_t current_pos() const noexcept
    {
        return checked_int(current_fc_it - begin_fc_buffer());
    }

    void set_current_pos(std::size_t pos) noexcept
    {
        assert(pos <= font_chars().size());
        current_fc_it = begin_fc_buffer() + pos;
        current_unicode_it = begin_unicode_buffer() + pos;
    }

    [[nodiscard]]
    array_view<uint32_t> reserve_space_for(array_view<uint32_t> ucs) noexcept
    {
        std::size_t n = std::min(ucs.size(), remaining());
        std::size_t right_size = checked_int(end_fc_it - current_fc_it);
        mem_move(current_fc_it + n, {current_fc_it, right_size});
        mem_move(current_unicode_it + n, {current_unicode_it, right_size});
        return ucs;
    }

    /// \return width of char
    int replace_and_advance(uint32_t uc, Font const& font) noexcept
    {
        assert(!is_full());
        *current_unicode_it = uc;
        *current_fc_it = &font.item(uc).view;
        int w = D::fc_width(*current_fc_it);
        ++current_unicode_it;
        ++current_fc_it;
        ++end_fc_it;
        return w;
    }

    void remove_right(FontCharPtr const * to) noexcept
    {
        assert(current_fc_it <= to);
        std::size_t n_cp = checked_int(end_fc_it - to);
        std::size_t n_jump = checked_int(to - current_fc_it);
        mem_move(current_fc_it, {to, n_cp});
        mem_move(current_unicode_it, {current_unicode_it + n_jump, n_cp});
        end_fc_it -= to - current_fc_it;
    }

    void remove_left(FontCharPtr const * to) noexcept
    {
        assert(current_fc_it >= to);
        assert(begin_fc_buffer() <= to);
        auto old = current_fc_it;
        auto n = current_fc_it - to;
        current_fc_it -= n;
        current_unicode_it -= n;
        remove_right(old);
    }

    void remove_all() noexcept
    {
        end_fc_it = begin_fc_buffer();
        current_fc_it = begin_fc_buffer();
        current_unicode_it = begin_unicode_buffer();
    }
};


WidgetEdit::WidgetEdit(
    gdi::GraphicApi & gd, Font const & font, CopyPaste & copy_paste,
    Colors colors, WidgetEventNotifier onsubmit
)
    : Widget(gd, Focusable::Yes)
    , x_cursor(D::start_x_cursor)
    , x_text(0)
    , colors(colors)
    , font(&font)
    , h_text(font.max_height())
    , onsubmit(onsubmit)
    , copy_paste(copy_paste)
{
    buffer().init();
    pointer_flag = PointerType::Edit;
    // set a "random" width
    Widget::set_wh(D::compute_optimal_dim(h_text * 10, h_text));
}

WidgetEdit::WidgetEdit(
    gdi::GraphicApi & gd, Font const & font, CopyPaste & copy_paste,
    chars_view text, Colors colors, WidgetEventNotifier onsubmit
)
    : WidgetEdit(gd, font, copy_paste, colors, onsubmit)
{
    if (!text.empty()) {
        set_text(text, {});
    }
}

WidgetEdit::~WidgetEdit()
{
    this->copy_paste.stop_paste_for(*this);
}

uint16_t WidgetEdit::x_padding() noexcept
{
    return D::compute_optimal_dim(0, 0).w;
}

bool WidgetEdit::has_text() const noexcept
{
    return !buffer().font_chars().empty();
}

WidgetEdit::Text WidgetEdit::get_text() const noexcept
{
    return Text::from_builder([this](delayed_build_string_buffer uninit_str) {
        writable_bytes_view av = uninit_str.chars_without_null_terminated();
        for (auto uc : buffer().text()) {
            auto cbuf = writable_sized_bytes_view<4>::assumed(av.first(4));
            av = av.from_offset(utf32_to_utf8(uc, cbuf).end());
        }
        return uninit_str.set_end_string_ptr(av.as_charp());
    });
}

unsigned WidgetEdit::get_text_as_uint() const noexcept
{
    unsigned result = 0;
    for (auto uc : buffer().text()) {
        result *= 10;
        result += uc - '0';
    }
    return result;
}

void WidgetEdit::set_text(bytes_view text, TextOptions opts)
{
    auto const old_pos = buffer().current_pos();

    buffer().remove_all();

    x_cursor = D::start_x_cursor;
    x_text = 0;

    int shift = 0;

    utf8_for_each(
        text,
        [&](uint32_t uc) {
            shift += buffer().replace_and_advance(uc, *font);
        },
        [](utf8_char_invalid) {},
        [](utf8_char_truncated) {},
        [&]{ return !buffer().is_full(); }
    );

    switch (opts.cursor_position) {
        case CusorPosition::KeepCursorPosition: {
            auto const fcs = buffer().font_chars();
            if (fcs.size() > old_pos) {
                buffer().set_current_pos(old_pos);
                auto right_part_is_lower = (fcs.size() - old_pos) < old_pos;
                auto partial_fcs = right_part_is_lower
                    ? fcs.from_offset(old_pos)
                    : fcs.first(old_pos);
                int partial_shift = 0;
                for (auto const * fc : partial_fcs) {
                    partial_shift += D::fc_width(fc);
                }
                shift = right_part_is_lower ? shift - partial_shift : partial_shift;
            }
            [[fallthrough]];
        }

        case CusorPosition::CursorToEnd:
            move_cursor_to_right(shift);
            break;

        case CusorPosition::CursorToBegin:
            buffer().set_current_pos(0);
            break;

    }

    if (opts.redraw == Redraw::Yes) {
        draw_inner(get_rect());
    }
}

void WidgetEdit::update_width(uint16_t width)
{
    Widget::set_wh(width, (D::h_padding + D::border_len) * 2 + h_text);

    int shift = x_text - D::start_x_cursor + x_cursor;
    x_text = 0;
    x_cursor = D::start_x_cursor;
    move_cursor_to_right(shift);
}

void WidgetEdit::insert_chars(array_view<uint32_t> ucs, Redraw redraw)
{
    ucs = buffer().reserve_space_for(ucs);
    auto old_x_cursor = x_cursor;
    auto old_it_ch = buffer().current();

    int shift = 0;
    for (auto uc : ucs) {
        shift += buffer().replace_and_advance(uc, *font);
    }

    bool partial_update = move_cursor_to_right(shift);
    if (redraw == Redraw::Yes) {
        redraw_text({partial_update, old_it_ch, old_x_cursor});
    }
}

void WidgetEdit::rdp_input_invalidate(Rect clip)
{
    draw_inner(clip);
    draw_border(clip, has_focus ? colors.focus_border : colors.border);
}

void WidgetEdit::rdp_input_scancode(
    KbdFlags /*flags*/, Scancode /*scancode*/,
    uint32_t /*event_time*/, Keymap const& keymap)
{
    REDEMPTION_DIAGNOSTIC_PUSH()
    REDEMPTION_DIAGNOSTIC_GCC_IGNORE("-Wswitch-enum")
    switch (keymap.last_kevent()) {
    case Keymap::KEvent::None:
        break;

    case Keymap::KEvent::KeyDown:
        insert_chars(keymap.last_decoded_keys().uchars_av(), Redraw::Yes);
        break;

    case Keymap::KEvent::Enter:
        this->onsubmit();
        break;

    case Keymap::KEvent::LeftArrow:
    case Keymap::KEvent::UpArrow:
        action_move_cursor_left(keymap.is_ctrl_pressed(), Redraw::Yes);
        break;

    case Keymap::KEvent::RightArrow:
    case Keymap::KEvent::DownArrow:
        action_move_cursor_right(keymap.is_ctrl_pressed(), Redraw::Yes);
        break;

    case Keymap::KEvent::Backspace:
        action_backspace(keymap.is_ctrl_pressed(), Redraw::Yes);
        break;

    case Keymap::KEvent::Delete:
        action_delete(keymap.is_ctrl_pressed(), Redraw::Yes);
        break;

    case Keymap::KEvent::End:
        action_move_cursor_to_end_of_line(Redraw::Yes);
        break;

    case Keymap::KEvent::Home:
        action_move_cursor_to_begin_of_line(Redraw::Yes);
        break;

    case Keymap::KEvent::Paste:
        copy_paste.paste(*this);
        break;

    case Keymap::KEvent::Copy:
        if (copy_paste) {
            copy_paste.copy(get_text());
        }
        break;

    case Keymap::KEvent::Cut:
        if (copy_paste) {
            copy_paste.copy(get_text());
        }
        if (has_text()) {
            set_text(""_av, {Redraw::Yes});
        }
        break;

    default:
        return;
    }
    REDEMPTION_DIAGNOSTIC_POP()
}

void WidgetEdit::rdp_input_unicode(KbdFlags flag, uint16_t unicode)
{
    if (bool(flag & KbdFlags::Release)) {
        return;
    }

    auto insert_char = D::sanitized_char([&](uint32_t uc){
        insert_chars({&uc, 1}, Redraw::Yes);
    });
    insert_char(unicode32_decoder.convert(unicode));
}

void WidgetEdit::rdp_input_mouse(uint16_t device_flags, uint16_t x_, uint16_t y)
{
    (void)y;

    if (device_flags != (MOUSE_FLAG_BUTTON1|MOUSE_FLAG_DOWN)) {
        return;
    }

    if (buffer().font_chars().empty()) {
        return;
    }

    int x = x_ - this->x();

    auto fc_mid = [](FontCharView const& fc){
        return fc.offsetx + (fc.width + 1) / 2 - 1;
    };

    // move to left
    if (x < x_cursor) {
        int shift = 0;

        while (buffer().is_movable_to_left()) {
            auto const & fc = buffer().get_previous_fc();

            if (x >= x_cursor - shift - fc.incby + fc_mid(fc)) {
                break;
            }

            shift += buffer().move_to_left();

            if (x_cursor - shift < D::start_x_cursor) {
                break;
            }
        }

        if (shift) {
            move_cursor_to_left_and_redraw(shift, Redraw::Yes);
        }
    }
    // move to right
    else if (x > x_cursor) {
        int shift = 0;
        int end_pos = get_end_pos();

        while (buffer().is_movable_to_right()) {
            auto const & fc = buffer().get_current_fc();

            if (x <= x_cursor + shift + fc_mid(fc)) {
                break;
            }

            shift += buffer().move_to_right();

            if (x_cursor + shift >= end_pos) {
                break;
            }
        }

        if (shift) {
            move_cursor_to_right_and_redraw(shift, Redraw::Yes);
        }
    }
}

void WidgetEdit::clipboard_insert_utf8(zstring_view text)
{
    uint32_t ucs[max_capacity];
    auto remaining = buffer().remaining();
    auto * p = ucs;
    utf8_for_each(
        text,
        D::sanitized_char([&](uint32_t uc) {
            --remaining;
            *p++ = uc;
        }),
        [](utf8_char_invalid) {},
        [](utf8_char_truncated) {},
        [&]{ return remaining; }
    );
    insert_chars({ucs, p}, Redraw::Yes);
}

void WidgetEdit::focus()
{
    if (!has_focus) {
        has_focus = true;
        draw_border(get_rect(), colors.focus_border);
        draw_cursor(get_rect(), colors.cursor);
    }
}

void WidgetEdit::blur()
{
    if (has_focus) {
        has_focus = false;
        draw_border(get_rect(), colors.border);
        draw_cursor(get_rect(), colors.bg);
    }
}

int WidgetEdit::get_end_pos() const
{
    return this->cx() - (D::w_padding + D::border_len + D::w_cursor);
}

bool WidgetEdit::action_move_cursor_left(bool ctrl_is_pressed, Redraw redraw)
{
    if (buffer().is_movable_to_left()) {
        int shift = buffer().move_to_left();

        if (ctrl_is_pressed) {
            while (buffer().is_movable_to_left() && (
                buffer().get_current_unicode() == ' '
                || buffer().get_previous_unicode() != ' '
            )) {
                shift += buffer().move_to_left();
            }
        }

        move_cursor_to_left_and_redraw(shift, redraw);

        return true;
    }

    return false;
}

bool WidgetEdit::action_move_cursor_right(bool ctrl_is_pressed, Redraw redraw)
{
    if (buffer().is_movable_to_right()) {
        int shift = buffer().move_to_right();

        if (ctrl_is_pressed) {
            while (buffer().is_movable_to_right() && (
                buffer().get_previous_unicode() == ' '
                || buffer().get_current_unicode() != ' '
            )) {
                shift += buffer().move_to_right();
            }
        }

        move_cursor_to_right_and_redraw(shift, redraw);

        return true;
    }

    return false;
}

bool WidgetEdit::action_move_cursor_to_begin_of_line(Redraw redraw)
{
    if (buffer().is_movable_to_left()) {
        buffer().move_to_start();
        int shift = x_text + x_cursor - D::start_x_cursor;
        move_cursor_to_left_and_redraw(shift, redraw);
        return true;
    }
    return false;
}

bool WidgetEdit::action_move_cursor_to_end_of_line(Redraw redraw)
{
    if (buffer().is_movable_to_right()) {
        int shift = 0;
        while (buffer().is_movable_to_right()) {
            shift += buffer().move_to_right();
        }

        move_cursor_to_right_and_redraw(shift, redraw);

        return true;
    }
    return false;
}

bool WidgetEdit::action_backspace(bool ctrl_is_pressed, Redraw redraw)
{
    if (buffer().is_movable_to_left()) {
        auto const old_position = buffer().current();

        int shift = 0;
        if (ctrl_is_pressed) {
            while (buffer().is_movable_to_left()
                && buffer().get_previous_unicode() == ' '
            ) {
                shift += buffer().move_to_left();
            }

            while (buffer().is_movable_to_left()
                && buffer().get_previous_unicode() != ' '
            ) {
                shift += buffer().move_to_left();
            }
        }
        else {
            shift += buffer().move_to_left();
        }

        remove_left(old_position, shift, redraw);

        return true;
    }

    return false;
}

bool WidgetEdit::action_delete(bool ctrl_is_pressed, Redraw redraw)
{
    if (buffer().is_movable_to_right()) {
        auto const old_position = buffer().current();

        int shift = 0;
        if (ctrl_is_pressed) {
            while (buffer().is_movable_to_right()
                && buffer().get_current_unicode() != ' '
            ) {
                shift += buffer().move_to_right();
            }

            while (buffer().is_movable_to_right()
                && buffer().get_current_unicode() == ' '
            ) {
                shift += buffer().move_to_right();
            }
        }
        else {
            shift += buffer().move_to_right();
        }

        remove_right(old_position, shift, redraw);

        return true;
    }

    return false;
}

bool WidgetEdit::action_remove_left(Redraw redraw)
{
    if (buffer().is_movable_to_left()) {
        auto const old_position = buffer().current();

        int shift = 0;
        while (buffer().is_movable_to_left()) {
            shift += buffer().move_to_left();
        }

        remove_left(old_position, shift, redraw);

        return true;
    }

    return false;
}

bool WidgetEdit::action_remove_right(Redraw redraw)
{
    if (buffer().is_movable_to_right()) {
        auto const old_position = buffer().current();

        int shift = 0;
        while (buffer().is_movable_to_right()) {
            shift += buffer().move_to_right();
        }

        remove_right(old_position, shift, redraw);

        return true;
    }

    return false;
}

void WidgetEdit::remove_left(FontCharPtr const* old_position, int shift, Redraw redraw)
{
    buffer().remove_right(old_position);

    auto old_x_text = x_text;
    auto partial_update = move_cursor_to_left(shift);

    if (redraw == Redraw::Yes) {
        maybe_redraw_left_empty_padding(old_x_text);
        redraw_removed_right_text(partial_update, shift);
    }
}

void WidgetEdit::remove_right(FontCharPtr const* old_position, int shift, Redraw redraw)
{
    buffer().remove_left(old_position);

    if (redraw == Redraw::Yes) {
        redraw_removed_right_text(true, shift);
    }
}

Rect WidgetEdit::cursor_rect(int x_cursor) noexcept
{
    return Rect(
        checked_int(x() + x_cursor),
        checked_int(y() + D::h_padding + D::border_len),
        D::w_cursor,
        h_text
    );
}

void WidgetEdit::draw_border(Rect clip, Color color)
{
    gdi_draw_border(drawable, color, get_rect(), D::border_len, clip, gdi::ColorCtx::depth24());
}

void WidgetEdit::draw_cursor(Rect clip, Color color)
{
    draw_rect(cursor_rect(x_cursor).intersect(clip), color);
}

void WidgetEdit::draw_inner(Rect clip)
{
    auto rect = get_rect().shrink(D::border_len).intersect(clip);
    draw_rect(rect, colors.bg);
    draw_text(rect);

    if (has_focus) {
        draw_cursor(clip, colors.cursor);
    }
}

void WidgetEdit::draw_text(Rect clip)
{
    int x = this->x() - x_text + D::w_padding + D::border_len;

    gdi::draw_text(
        drawable,
        x,
        y() + D::h_padding + D::border_len,
        font->max_height(),
        gdi::DrawTextPadding::Padding(0),
        buffer().font_chars(),
        colors.fg,
        colors.bg,
        clip
    );
}

void WidgetEdit::draw_rect(Rect rect, Widget::Color color)
{
    if (!rect.isempty()) {
        drawable.draw(RDPOpaqueRect(rect, color), rect, gdi::ColorCtx::depth24());
    }
}

/// \return last pixel drawn
int WidgetEdit::redraw_text(RedrawInfo redraw_info)
{
    Rect clip = get_rect().shrink(D::border_len);
    int x = this->x();
    auto fcs = buffer().font_chars();

    if (redraw_info.partial_update) {
        int shift = redraw_info.x_cursor - D::w_cursor_padding;
        clip.cx -= shift;
        clip.x += shift;
        x += shift;
        fcs = fcs.from_offset(redraw_info.it);
    }
    else {
        x += -x_text + D::w_padding + D::border_len;
    }

    int last_x = gdi::draw_text(
        drawable,
        x,
        y() + D::h_padding + D::border_len,
        font->max_height(),
        gdi::DrawTextPadding::Padding(0),
        fcs,
        colors.fg,
        colors.bg,
        clip
    );
    draw_cursor(clip, colors.cursor);

    return last_x;
}

void WidgetEdit::redraw_cursor(int old_x_cursor)
{
    auto rect = cursor_rect(old_x_cursor);
    draw_rect(rect, colors.bg);
    rect.x = checked_int(x() + x_cursor);
    draw_rect(rect, colors.cursor);
}

void WidgetEdit::redraw_removed_right_text(bool partial_update, int shift)
{
    int last_x = redraw_text({partial_update, buffer().current(), x_cursor});
    auto clip = get_rect().shrink(D::border_len);
    // redraws over removed chars
    if (last_x < clip.eright()) {
        int eright = buffer().is_movable_to_right()
            ? last_x
            : x() + x_cursor + D::w_cursor;
        int w = std::min(clip.eright() - eright, shift);
        clip.x = checked_int(eright);
        clip.cx = checked_int(w);
        draw_rect(clip, colors.bg);
    }
}

void WidgetEdit::redraw_right_empty_padding()
{
    Rect clip = get_rect().shrink(D::border_len);
    clip.x = this->eright() - D::w_padding - D::w_cursor_padding;
    clip.cx = D::w_padding;
    draw_rect(clip, colors.bg);
}

void WidgetEdit::maybe_redraw_left_empty_padding(int old_x_text)
{
    if (old_x_text && !x_text) {
        Rect clip = get_rect().shrink(D::border_len);
        clip.cx = D::w_padding;
        draw_rect(clip, colors.bg);
    }
}

bool WidgetEdit::move_cursor_to_right(int shift)
{
    int end_pos = get_end_pos();
    if (x_cursor + shift <= end_pos) {
        x_cursor += shift;
        return true;
    }
    else {
        x_text += x_cursor + shift - end_pos;
        x_cursor = end_pos;
        return false;
    }
}

void WidgetEdit::move_cursor_to_right_and_redraw(int shift, Redraw redraw)
{
    auto old_x_cursor = x_cursor;
    auto partial_update = move_cursor_to_right(shift);

    if (redraw == Redraw::Yes) {
        if (partial_update) {
            redraw_cursor(old_x_cursor);
        }
        else {
            redraw_text({});
            if (!buffer().is_movable_to_right()) {
                redraw_right_empty_padding();
            }
        }
    }
}

bool WidgetEdit::move_cursor_to_left(int shift)
{
    if (x_cursor - shift >= D::start_x_cursor) {
        x_cursor -= shift;
        return true;
    }
    else {
        x_text -= D::start_x_cursor - (x_cursor - shift);
        x_cursor = D::start_x_cursor;
        return false;
    }
}

void WidgetEdit::move_cursor_to_left_and_redraw(int shift, Redraw redraw)
{
    auto old_x_text = x_text;
    auto old_x_cursor = x_cursor;
    auto partial_update = move_cursor_to_left(shift);

    if (redraw == Redraw::Yes) {
        if (partial_update) {
            redraw_cursor(old_x_cursor);
        }
        else {
            redraw_text({});
            maybe_redraw_left_empty_padding(old_x_text);
        }
    }
}

WidgetEdit::Buffer & WidgetEdit::buffer() noexcept
{
    return Buffer::from_data(editable_buffer);
}

WidgetEdit::Buffer const & WidgetEdit::buffer() const noexcept
{
    return Buffer::from_data(editable_buffer);
}

void WidgetEdit::set_font(Font const & font, Redraw redraw)
{
    auto uc_it = buffer().text().begin();

    auto fcs = buffer().writable_font_chars();
    auto mid = buffer().current();

    auto left = fcs.before(mid);
    auto right = fcs.from_offset(mid);

    int shift = 0;
    for (auto & fc : left) {
        shift -= D::fc_width(fc);
        fc = &font.item(*uc_it).view;
        shift += D::fc_width(fc);
        ++uc_it;
    }
    for (auto & fc : right) {
        fc = &font.item(*uc_it).view;
        ++uc_it;
    }

    if (shift < 0) {
        move_cursor_to_left(-shift);
    }
    else {
        move_cursor_to_right(shift);
    }

    this->font = &font;

    if (redraw == Redraw::Yes) {
        draw_inner(get_rect());
    }
}

WidgetEdit::Colors WidgetEdit::Colors::from_theme(const Theme& theme) noexcept
{
    return {
        .fg = theme.edit.fgcolor,
        .bg = theme.edit.bgcolor,
        .border = theme.edit.border_color,
        .focus_border = theme.edit.focus_border_color,
        .cursor = theme.edit.cursor_color,
    };
}
