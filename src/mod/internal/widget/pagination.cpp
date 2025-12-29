/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "mod/internal/widget/pagination.hpp"
#include "mod/internal/widget/button.hpp" // WidgetButtonEvent
#include "gdi/text.hpp"
#include "core/font.hpp"
#include "utils/theme.hpp"
#include "utils/mathutils.hpp"


WidgetPagination::Colors
WidgetPagination::Colors::from_theme(const Theme& theme) noexcept
{
    return {
        .fg = theme.global.fgcolor,
        .bg = theme.global.bgcolor,
        .focus_fg = theme.global.focus_color,
        .edit = WidgetEdit::Colors::from_theme(theme),
    };
}

enum class WidgetPagination::Item : uint8_t
{
    None,
    /*
     * focusable
     */
    First,
    Prev,
    Edit,
    Next,
    Last,
    /*
     * unfocusable
     */
    Label,
};

struct WidgetPagination::D
{
    static const int16_t X_PAD_BUTTON = 12;
    static const int16_t X_PAD_LABEL_EDIT = 12;
    static const int16_t X_BBOX = 7;

    static_assert(X_BBOX <= X_PAD_BUTTON);
    static_assert(X_BBOX <= X_PAD_LABEL_EDIT);

    // Item as integer constant for index usage
    enum Offset
    {
        First,
        Prev,
        Next,
        Last,
        Edit,
        Label,
    };

    static int16_t label_prefix_w(WidgetPagination const& self) noexcept
    {
        return self.m_chars[5]->offsetx + self.m_chars[5]->incby;
    }

    static int16_t label_last_fc_incby_minus_width(WidgetPagination const& self) noexcept
    {
        auto & last_fc = self.m_chars[4 + self.m_label_total_len - 1];
        return last_fc->incby - last_fc->width;
    }

    static bool is_valid_page(WidgetPagination const& self, uint32_t page) noexcept
    {
        return page != self.m_current && page <= self.m_total && page > 0;
    }

    static bool change_page(WidgetPagination & self, uint32_t page)
    {
        if (is_valid_page(self, page))
        {
            self.m_current = page;
            self.m_update_page_event(page);
            return true;
        }
        return false;
    }

    static bool process_act_button(WidgetPagination & self, Item item)
    {
        uint32_t next_page;

        if (item == Item::First)
        {
            next_page = self.m_total ? 1 : 0;
        }
        else if (item == Item::Last)
        {
            next_page = self.m_total;
        }
        else
        {
            next_page = self.m_current + ((item == Item::Next) ? 1u : ~uint32_t{});
        }

        return change_page(self, next_page);
    }

    static void set_edit_position(WidgetPagination & self)
    {
        self.m_edit.set_xy(self.m_offsets[Edit] + self.x(), self.y());
    }

    struct YInfo
    {
        uint16_t top_pad;
        uint16_t bottom_pad;

        YInfo(FontCharView fc, uint16_t cy) noexcept
        {
            uint16_t h_space = (cy - fc.height + 1) / 2;
            top_pad = checked_int{ h_space - fc.offsety };
            bottom_pad = checked_int{ cy - top_pad - fc.offsety - fc.height };
        }
    };

    static array_view<FontCharPtr> first_button_text(WidgetPagination const & self) noexcept
    {
        return {self.m_chars, 2};
    }

    static array_view<FontCharPtr> prev_button_text(WidgetPagination const & self) noexcept
    {
        return {self.m_chars, 1};
    }

    static array_view<FontCharPtr> next_button_text(WidgetPagination const & self) noexcept
    {
        return {self.m_chars + 3, 1};
    }

    static array_view<FontCharPtr> last_button_text(WidgetPagination const & self) noexcept
    {
        return {self.m_chars + 2, 2};
    }


    static YInfo first_or_prev_y_info(WidgetPagination const & self) noexcept
    {
        return {*self.m_chars[0], self.cy()};
    }

    static YInfo next_or_last_y_info(WidgetPagination const & self) noexcept
    {
        return {*self.m_chars[3], self.cy()};
    }

    enum FocusMode
    {
        Focus,
        Blur,
    };

    static void blur_or_focus(WidgetPagination & self, FocusMode focus_mode)
    {
        auto draw_text = [&](int x, int y, array_view<FontCharPtr> fcs) {
            auto is_pressed = (self.m_focus_item == self.m_pressed_item);
            x += self.x();
            y += self.y();
            gdi::draw_text(
                self.drawable,
                x,
                y,
                self.m_line_h,
                gdi::DrawTextPadding{}.xy(is_pressed),
                fcs,
                (focus_mode == FocusMode::Focus)
                    ? self.m_colors.focus_fg
                    : self.m_colors.fg,
                self.m_colors.bg,
                {checked_int{x}, checked_int{y}, self.cx(), self.cy()}
            );
        };

        switch (self.m_focus_item)
        {
            case Item::None:
            case Item::Label:
                break;

            case Item::Edit:
                set_edit_position(self);
                if (focus_mode == FocusMode::Focus)
                {
                    self.m_edit.focus();
                }
                else
                {
                    self.m_edit.blur();
                }
                break;

            case Item::First:
                draw_text(
                    self.m_offsets[First],
                    first_or_prev_y_info(self).top_pad,
                    first_button_text(self)
                );
                break;

            case Item::Prev:
                draw_text(
                    self.m_offsets[Prev],
                    first_or_prev_y_info(self).top_pad,
                    prev_button_text(self)
                );
                break;

            case Item::Next:
                draw_text(
                    self.m_offsets[Next],
                    next_or_last_y_info(self).top_pad,
                    next_button_text(self)
                );
                break;

            case Item::Last:
                draw_text(
                    self.m_offsets[Last],
                    next_or_last_y_info(self).top_pad,
                    last_button_text(self)
                );
                break;
        }
    }
};

WidgetPagination::WidgetPagination(
    gdi::GraphicApi & gd,
    Font const & font,
    Colors colors,
    RedrawAfterEvent redraw_after_event,
    UpdatePageEvent update_page_event
) noexcept
    : Widget{gd, Focusable::Yes}
    , m_line_h{font.max_height()}
    , m_label_total_len{2}
    , m_redraw_after_event{redraw_after_event == RedrawAfterEvent::Yes}
    /*
        ['◀', '◂', '▸', '▶', ...]
         ~~~                 prev text button
         ~~~~~~~~            first text button
                   ~~~~~~~~  last text button
                        ~~~  next text button
     */
    , m_chars{
        &font.item(0x25C0).view, // ◀
        &font.item(0x25C2).view, // ◂ for ◀◂
        &font.item(0x25B8).view, // ▸ for ▸▶
        &font.item(0x25B6).view, // ▶
        &font.item('/').view,
        &font.item(' ').view,
    }
    , m_update_page_event{update_page_event}
    , m_colors{colors}
    , m_edit{gd, font, colors.edit, [this]{
        if (m_total > 0)
        {
            auto page = m_edit.get_text_as_uint();
            D::change_page(*this, page);
        }
    }}
{
    auto x_pad = mmax({
        // max_offset
        m_chars[0]->offsetx,
        m_chars[2]->offsetx,
        m_chars[3]->offsetx,
        // incby_minus_widths
        checked_cast<int8_t>( m_chars[0]->incby - m_chars[0]->width ),
        checked_cast<int8_t>( m_chars[1]->incby - m_chars[1]->width ),
        checked_cast<int8_t>( m_chars[3]->incby - m_chars[3]->width ),
    });

    auto & slash = m_chars[4];
    auto & space = m_chars[5];
    int16_t label_w = slash->offsetx + slash->incby
                    + space->offsetx + space->incby;
    m_label_w = checked_int{ label_w };

    m_offsets[D::First] = D::X_BBOX + x_pad - m_chars[0]->offsetx;

    m_offsets[D::Prev] = m_offsets[D::First]
                       + x_pad - m_chars[0]->offsetx
                       + m_chars[0]->incby
                       + m_chars[1]->offsetx + m_chars[1]->width
                       + x_pad
                       + D::X_PAD_BUTTON * 2 + 1;

    m_offsets[D::Edit] = m_offsets[D::Prev]
                       + x_pad - m_chars[0]->offsetx
                       + m_chars[0]->width
                       + x_pad
                       + D::X_PAD_LABEL_EDIT * 2 + 1
                       + x_pad;

    m_offsets[D::Label] = checked_int{ m_offsets[D::Edit] + m_edit.cx() };

    m_offsets[D::Next] = m_offsets[D::Label]
                       + x_pad
                       + D::label_prefix_w(*this)
                       + label_w - D::label_last_fc_incby_minus_width(*this)
                       + x_pad - m_chars[3]->offsetx
                       + D::X_PAD_LABEL_EDIT * 2 + 1;

    m_offsets[D::Last] = m_offsets[D::Next]
                       + x_pad - m_chars[3]->offsetx
                       + m_chars[3]->width
                       + x_pad
                       + D::X_PAD_BUTTON * 2 + 1;

    int16_t bbox_x_pad = D::X_BBOX + x_pad;

    m_bbox_offsets[D::First * 2 + 0] = 1;
    m_bbox_offsets[D::First * 2 + 1] = m_bbox_offsets[D::First * 2]
                                     + bbox_x_pad * 2
                                     + m_chars[0]->incby
                                     + m_chars[1]->offsetx + m_chars[1]->width;

    m_bbox_offsets[D::Prev * 2 + 0] = m_offsets[D::Prev] + 1
                                    - (bbox_x_pad - m_chars[0]->offsetx);
    m_bbox_offsets[D::Prev * 2 + 1] = m_bbox_offsets[D::Prev * 2]
                                    + bbox_x_pad * 2
                                    + m_chars[0]->width;

    m_bbox_offsets[D::Next * 2 + 0] = m_offsets[D::Next] + 1
                                    - (bbox_x_pad - m_chars[3]->offsetx);
    m_bbox_offsets[D::Next * 2 + 1] = m_bbox_offsets[D::Next * 2]
                                    + bbox_x_pad * 2
                                    + m_chars[3]->width;

    m_bbox_offsets[D::Last * 2 + 0] = m_offsets[D::Last] + 1
                                    - (bbox_x_pad - m_chars[2]->offsetx);
    m_bbox_offsets[D::Last * 2 + 1] = m_bbox_offsets[D::Last * 2]
                                    + bbox_x_pad * 2
                                    + m_chars[2]->incby
                                    + m_chars[3]->offsetx + m_chars[3]->width;

    set_wh(
        checked_int{
            m_offsets[D::Last]
          + x_pad - m_chars[2]->offsetx
          + m_chars[2]->incby
          + m_chars[3]->offsetx + m_chars[3]->width
          + x_pad
          + D::X_BBOX + 1
        },
        m_edit.cy()
    );
}

void WidgetPagination::update(Data data)
{
    m_total = data.total_page;
    auto total_str = int_to_decimal_chars(m_total);

    auto & font = m_edit.get_font();

    int old_label_incby_minus_width = D::label_last_fc_incby_minus_width(*this);

    int label_total_width = m_chars[4]->boxed_width()
                          + m_chars[5]->boxed_width();
    auto* out = m_chars + 6;
    for (uint8_t uc : bytes_view{total_str.sv()})
    {
        REDEMPTION_ASSUME(uc >= '0' && uc <= '9');
        auto const * fc = &font.item(uc).view;
        label_total_width += fc->boxed_width();
        *out++ = fc;
    }
    m_label_total_len = checked_int{ out - (m_chars + 4) };

    m_current = mmin(data.current_page, m_total);
    auto edit_str = int_to_decimal_chars(m_current);
    int edit_w = checked_cast<int>(edit_str.size() + 1) * font.max_digit_width()
               + WidgetEdit::x_padding();
    int old_edit_w = m_edit.cx();
    m_edit.set_text(edit_str, { WidgetEdit::Redraw::No });
    m_edit.update_width(checked_int{ edit_w });

    int new_label_and_edit_w = label_total_width + edit_w;
    int diff_label_and_edit_w = new_label_and_edit_w - (m_label_w + old_edit_w);
    int shift_button = old_label_incby_minus_width
                     - D::label_last_fc_incby_minus_width(*this)
                     + diff_label_and_edit_w;

    m_label_w = checked_int{ label_total_width };

    m_offsets[D::Label] += edit_w - old_edit_w;
    m_offsets[D::Next] += shift_button;
    m_offsets[D::Last] += shift_button;

    m_bbox_offsets[D::Next*2] += shift_button;
    m_bbox_offsets[D::Last*2] += shift_button;
    m_bbox_offsets[D::Next*2 + 1] += shift_button;
    m_bbox_offsets[D::Last*2 + 1] += shift_button;

    set_wh(checked_int{cx() + diff_label_and_edit_w}, cy());
}

void WidgetPagination::set_page(uint32_t page)
{
    if (m_current != page && D::is_valid_page(*this, page))
    {
        m_current = page;
        m_edit.set_text(int_to_decimal_chars(m_current), { WidgetEdit::Redraw::Yes });
    }
}

void WidgetPagination::set_prev_page(Cycle enable_cycle)
{
    auto current = (m_current <= 1 && enable_cycle) ? m_total : m_current - 1u;
    set_page(current);
}

void WidgetPagination::set_next_page(Cycle enable_cycle)
{
    auto current = (m_current == m_total && enable_cycle) ? 1u : m_current + 1u;
    set_page(current);
}

Widget::NextFocusResult
WidgetPagination::next_focus(FocusDirection dir, FocusStrategy strategy)
{
    Item new_focus;

    if (strategy == FocusStrategy::Next)
    {
        if (dir == FocusDirection::Forward)
        {
            if (m_focus_item == Item::Last)
            {
                return NextFocusResult::Focusable;
            }
            new_focus = checked_int{underlying_cast(m_focus_item) + 1};
        }
        else // if FocusDirection::Backward
        {
            if (m_focus_item <= Item::First)
            {
                return NextFocusResult::Focusable;
            }
            new_focus = checked_int{underlying_cast(m_focus_item) - 1};
        }
    }
    else // if FocusStrategy::Restart
    {
        if (dir == FocusDirection::Forward)
        {
            if (m_focus_item == Item::First)
            {
                return NextFocusResult::Focusable;
            }
            new_focus = Item::First;
        }
        else // if FocusDirection::Backward
        {
            if (m_focus_item == Item::Last)
            {
                return NextFocusResult::Focusable;
            }
            new_focus = Item::Last;
        }
    }

    blur();
    m_focus_item = new_focus;
    focus();

    return NextFocusResult::Focused;
}

void WidgetPagination::init_focus()
{
    if (this->focusable == Focusable::Yes)
    {
        this->has_focus = true;
        m_focus_item = Item::Edit;
        m_edit.init_focus();
    }
}

void WidgetPagination::blur()
{
    has_focus = false;
    D::blur_or_focus(*this, D::FocusMode::Blur);
}

void WidgetPagination::focus()
{
    has_focus = true;
    D::blur_or_focus(*this, D::FocusMode::Focus);
}

void WidgetPagination::rdp_input_mouse(uint16_t device_flags, uint16_t x, uint16_t y)
{
    if (!get_rect().contains_pt(checked_int(x), checked_int(y)))
    {
        if (m_pressed_item != Item::None)
        {
            m_pressed_item = Item::None;
            D::blur_or_focus(*this, D::FocusMode::Focus);
        }
        return;
    }

    if (!(device_flags & MOUSE_FLAG_BUTTON1))
    {
        return ;
    }

    auto in_bbox = [&](D::Offset i){
        return x >= this->x() + m_bbox_offsets[i * 2] && x < this->x() + m_bbox_offsets[i * 2 + 1];
    };

    Item item = Item::None;

    // edit, next, last
    if (x >= m_edit.x())
    {
        if (x <= m_edit.eright())
        {
            has_focus = true;
            m_pressed_item = Item::None;

            if (m_focus_item != Item::None && m_focus_item != Item::Edit)
            {
                D::blur_or_focus(*this, D::FocusMode::Blur);
            }

            m_focus_item = Item::Edit;

            m_edit.rdp_input_mouse(device_flags, x, y);
            m_edit.focus();

            return;
        }
        else if (in_bbox(D::Next))
        {
            item = Item::Next;
        }
        else if (in_bbox(D::Last))
        {
            item = Item::Last;
        }
    }
    // first or prev
    else
    {
        if (in_bbox(D::First))
        {
            item = Item::First;

        }
        else if (in_bbox(D::Prev))
        {

            item = Item::Prev;
        }
    }

    // fire event when click down then up on the same item
    // down -> take focus

    bool takeable_focus = (device_flags == (MOUSE_FLAG_BUTTON1 | MOUSE_FLAG_DOWN));
    bool enable_focus = (takeable_focus && item != Item::None);
    auto new_pressed_item = takeable_focus ? item : Item::None;
    auto new_focus_item = enable_focus ? item : m_focus_item;

    if (enable_focus)
    {
        has_focus = true;
    }

    if (new_pressed_item != m_pressed_item || new_focus_item != m_focus_item)
    {
        bool is_clicked = (item == m_pressed_item && device_flags == MOUSE_FLAG_BUTTON1);
        bool call_blur = takeable_focus
            ? (new_focus_item != m_focus_item)
            : (m_pressed_item != m_focus_item);
        m_pressed_item = new_pressed_item;

        if (is_clicked && D::process_act_button(*this, item))
        {
            if (m_redraw_after_event)
            {
                m_edit.set_text(int_to_decimal_chars(m_current), { WidgetEdit::Redraw::Yes });
            }
            else
            {
                m_focus_item = new_focus_item;
                return ;
            }
        }

        if (call_blur)
        {
            D::blur_or_focus(*this, D::FocusMode::Blur);
        }
        m_focus_item = new_focus_item;
        D::blur_or_focus(*this, D::FocusMode::Focus);
    }
}

void WidgetPagination::rdp_input_unicode(KbdFlags flag, uint16_t unicode)
{
    switch (m_focus_item)
    {
        case Item::None:
        case Item::Label:
            return;

        case Item::Edit:
            m_edit.rdp_input_unicode(flag, unicode);
            return;

        case Item::First:
        case Item::Prev:
        case Item::Next:
        case Item::Last:
            if (WidgetButtonEvent::is_submit_event(flag, unicode))
            {
                D::process_act_button(*this, m_focus_item);
            }
            break;
    }
}

void WidgetPagination::rdp_input_scancode(KbdFlags flags, Scancode scancode, uint32_t event_time, const Keymap& keymap)
{
    switch (m_focus_item)
    {
        case Item::None:
        case Item::Label:
            return;

        case Item::Edit:
            m_edit.rdp_input_scancode(flags, scancode, event_time, keymap);
            return;

        case Item::First:
        case Item::Prev:
        case Item::Next:
        case Item::Last:
            if (WidgetButtonEvent::is_submit_event(keymap))
            {
                D::process_act_button(*this, m_focus_item);
            }
            break;
    }
}

void WidgetPagination::rdp_input_invalidate(Rect r)
{
    auto clip = r.intersect(get_rect());
    if (clip.isempty())
    {
        return ;
    }

    auto draw_text = [&](
        int x1,
        int x2,
        int offset_x,
        int w,
        D::YInfo y_info,
        Item item,
        array_view<FontCharPtr> fcs
    ) {
        x1 += x();
        x2 += x();
        if ((clip.x >= x1 && clip.x < x2)
         || (x1 >= clip.x && x1 < clip.eright()))
        {
            auto is_pressed = (item == m_pressed_item);
            gdi::draw_text(
                drawable,
                x1,
                y(),
                m_line_h,
                gdi::DrawTextPadding{
                    .top = checked_int{ y_info.top_pad + is_pressed },
                    .right = checked_int{ x2 - x1 - w - offset_x },
                    .bottom = y_info.bottom_pad,
                    .left = checked_int{ offset_x + is_pressed }
                },
                fcs,
                (m_focus_item == item)
                    ? m_colors.focus_fg
                    : m_colors.fg,
                m_colors.bg,
                clip
            );
        }
    };

    D::YInfo y_info1 = D::first_or_prev_y_info(*this);
    D::YInfo y_info2 = D::next_or_last_y_info(*this);

    int16_t prev_w = m_chars[0]->offsetx + m_chars[0]->incby + 1;
    int16_t next_w = m_chars[3]->offsetx + m_chars[3]->incby + 1;
    int16_t first_w = prev_w + m_chars[1]->offsetx + m_chars[1]->incby;
    int16_t last_w = next_w + m_chars[2]->offsetx + m_chars[2]->incby;

    // first button
    draw_text(0, m_offsets[D::Prev], m_offsets[D::First], first_w,
              y_info1, Item::First, D::first_button_text(*this));
    // prev button
    draw_text(m_offsets[D::Prev], m_offsets[D::Edit], 0, prev_w,
              y_info1, Item::Prev, D::prev_button_text(*this));
    // next button
    draw_text(m_offsets[D::Next], m_offsets[D::Last], 0, next_w,
              y_info2, Item::Next, D::next_button_text(*this));
    // last button
    draw_text(m_offsets[D::Last], cx(), 0, last_w,
              y_info2, Item::Last, D::last_button_text(*this));

    // edit
    D::set_edit_position(*this);
    m_edit.rdp_input_invalidate(clip);

    // label
    auto label_prefix_w = D::label_prefix_w(*this);
    auto label_x = m_offsets[D::Label];
    auto text = array_view{m_chars}.drop_front(4).first(m_label_total_len);
    draw_text(label_x, m_offsets[D::Next], label_prefix_w, m_label_w + 1,
              D::YInfo{{.height = checked_int{m_line_h}}, cy()}, Item::Label, text);
}
