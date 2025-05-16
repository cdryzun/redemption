/*
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
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
 *   Copyright (C) Wallix 2010-2013
 *   Author(s): Christophe Grosjean, Dominique Lafages, Jonathan Poelen,
 *              Meng Tan, Jennifer Inthavong
 */

#pragma once

#include "mod/internal/widget/composite.hpp"
#include "mod/internal/widget/button.hpp"
#include "mod/internal/widget/image.hpp"
#include "mod/internal/widget/label.hpp"
#include "mod/internal/widget/password.hpp"
#include "mod/internal/widget/vertical_scroll_text.hpp"
#include "mod/internal/widget/delegated_copy.hpp"
#include "mod/internal/widget/widget_rect.hpp"
#include <vector>


class Theme;
class CopyPaste;

class WidgetDialogBase : public WidgetComposite
{
public:
    struct WidgetLink
    {
        WidgetVerticalScrollText show;
        WidgetLabel copied_msg;
        WidgetLabel label;
        std::vector<char> link_value;
        WidgetDelegatedCopy copy;
        bool msg_showed = false;
    };

    struct WidgetChallenge
    {
        WidgetPassword challenge_edit;
    };

    struct Events
    {
        WidgetEventNotifier onsubmit;
        WidgetEventNotifier oncancel;
        WidgetEventNotifier onctrl_shift {};
    };

    WidgetDialogBase(
        gdi::GraphicApi & drawable, Rect widget_rect, Events events,
        chars_view caption, chars_view text, Widget * extra_button,
        Theme const & theme, Font const & font, chars_view ok_text,
        std::unique_ptr<WidgetButton> cancel,
        WidgetEdit* challenge,
        WidgetLink* link);

    ~WidgetDialogBase();

    void move_size_widget(int16_t left, int16_t top, uint16_t width, uint16_t height);

    void rdp_input_scancode(KbdFlags flags, Scancode scancode, uint32_t event_time, Keymap const& keymap) override;

protected:
    void show_copied_msg();
    void focus_to_ok();

private:
    WidgetEventNotifier onctrl_shift;

    WidgetLabel        title;
    WidgetRect         separator;
    WidgetVerticalScrollText dialog;

    WidgetLink* link;
    WidgetEdit* challenge;

    WidgetButton   ok;

    std::unique_ptr<WidgetButton> cancel;

    WidgetImage         img;
    Widget *            extra_button;
    WidgetEventNotifier oncancel;
};


class WidgetDialog : public WidgetDialogBase
{
public:
    struct Events
    {
        WidgetEventNotifier onsubmit;
        WidgetEventNotifier oncancel;
    };

    WidgetDialog(
        gdi::GraphicApi& drawable, Rect widget_rect, Events events,
        chars_view caption, chars_view text,
        Theme const& theme, Font const& font,
        chars_view ok_text, chars_view cancel_text);
};


class WidgetDialogWithChallenge : private WidgetDialogBase::WidgetChallenge, public WidgetDialogBase
{
public:
    using Events = WidgetDialogBase::Events;

    enum class ChallengeOpt { Echo, Hide };

    WidgetDialogWithChallenge(
        gdi::GraphicApi & drawable, Rect widget_rect, Events events,
        chars_view caption, chars_view text,
        Widget * extra_button,
        chars_view ok_text,
        Font const & font, Theme const & theme, CopyPaste & copy_paste,
        ChallengeOpt challenge);

    WidgetEdit::Text get_chalenge() const noexcept
    {
        return this->challenge_edit.get_text();
    }
};


class WidgetDialogWithCopyableLink : private WidgetDialogBase::WidgetLink, public WidgetDialogBase
{
public:
    using Events = WidgetDialog::Events;

    WidgetDialogWithCopyableLink(
        gdi::GraphicApi & drawable, Rect widget_rect, Events events,
        chars_view caption, chars_view text,
        chars_view link_value, chars_view link_label,
        chars_view copied_msg_label, chars_view ok_text,
        Font const & font, Theme const & theme, CopyPaste & copy_paste);

private:
    CopyPaste & copy_paste;
};
