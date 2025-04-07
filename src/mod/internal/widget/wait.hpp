/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team

SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "mod/internal/widget/composite.hpp"
#include "mod/internal/widget/multiline.hpp"
#include "mod/internal/widget/button.hpp"
#include "mod/internal/widget/edit.hpp"
#include "translation/translation.hpp"


class WidgetWait : public WidgetComposite
{
public:
    enum {
        NONE               = 0x00,
        COMMENT_DISPLAY    = 0x01,
        COMMENT_MANDATORY  = 0x02,
        TICKET_DISPLAY     = 0x04,
        TICKET_MANDATORY   = 0x08,
        DURATION_DISPLAY   = 0x10,
        DURATION_MANDATORY = 0x20,

        HIDE_BACK_TO_SELECTOR = 0x10000,

        SHOW_FORM = COMMENT_DISPLAY | TICKET_DISPLAY | DURATION_DISPLAY,
        NOTE_DISPLAY = COMMENT_MANDATORY | TICKET_MANDATORY | DURATION_MANDATORY,
    };

    struct Events
    {
        WidgetEventNotifier onaccept;
        WidgetEventNotifier onrefused;
        WidgetEventNotifier onconfirm;
        WidgetEventNotifier onctrl_shift;
    };


    struct EditTexts
    {
        WidgetEdit::Text comment;
        WidgetEdit::Text ticket;
        WidgetEdit::Text duration;
    };

    WidgetWait(
        gdi::GraphicApi & drawable, CopyPaste & copy_paste, Rect const widget_rect,
        Events events, chars_view caption, chars_view text,
        Widget * extra_button,
        Font const & font, Theme const & theme, Translator tr,
        unsigned flags, std::chrono::minutes duration_max);

    void move_size_widget(int16_t left, int16_t top, uint16_t width, uint16_t height);

    void rdp_input_scancode(KbdFlags flags, Scancode scancode, uint32_t event_time, Keymap const& keymap) override;

    void rdp_input_invalidate(Rect clip) override;

    EditTexts get_edit_texts() const noexcept;

private:
    void check_form_confirmation();

    struct D;
    friend D;

    struct Form
    {
        WidgetLabel  warning_msg;
        WidgetLabel  duration_label;
        WidgetEdit   duration_edit;
        WidgetLabel  duration_format;
        WidgetLabel  ticket_label;
        WidgetEdit   ticket_edit;
        WidgetLabel  comment_label;
        WidgetEdit   comment_edit;
        WidgetLabel  notes;
        WidgetButton confirm;
    };

    WidgetEventNotifier onaccept;
    WidgetEventNotifier onconfirm;
    WidgetEventNotifier onrefused;
    WidgetEventNotifier onctrl_shift;

    Widget * extra_button;

    Color border_color;
    uint16_t group_height = 0;

    Font const& font;
    Translator tr;

    unsigned widget_flags;
    std::chrono::minutes duration_max;

    WidgetLabel caption;
    WidgetMultiLine dialog;

    Form form;

    WidgetButton goselector;

    WidgetButton exit;
};
