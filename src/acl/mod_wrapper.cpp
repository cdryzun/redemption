/*
SPDX-FileCopyrightText: 2022 Wallix Proxies Team

SPDX-License-Identifier: GPL-2.0-or-later
*/


#include "acl/mod_wrapper.hpp"
#include "configs/config.hpp"
#include "core/RDP/bitmapupdate.hpp"
#include "core/RDP/orders/RDPOrdersPrimaryLineTo.hpp"
#include "core/RDP/orders/RDPOrdersPrimaryOpaqueRect.hpp"
#include "core/client_info.hpp"
#include "gdi/subrect4.hpp"
#include "gdi/draw_utils.hpp"
#include "keyboard/keymap.hpp"
#include "mod/null/null.hpp"
#include "translation/trkeys.hpp"
#include "translation/translation.hpp"
#include "utils/sugar/int_to_chars.hpp"
#include "RAIL/client_execute.hpp"

ModWrapper::ModWrapper(
    mod_api& mod, CRef<TimeBase> time_base, CRef<BGRPalette> palette,
    gdi::GraphicApi& graphics, CRef<ClientInfo> client_info,
    CRef<Font> glyphs, CRef<ClientExecute> rail_client_execute,
    CRef<Inifile> ini, Translator translator)
: gfilter(graphics, static_cast<RdpInput&>(*this), Rect{})
, enable_osd_display_remote_target(ini.get().get<cfg::globals::enable_osd_display_remote_target>())
, client_info(client_info)
, rail_client_execute(rail_client_execute)
, palette(palette)
, ini(ini)
, translator(translator)
, glyphs(glyphs)
, modi(&mod)
, time_base(time_base)
{}

void ModWrapper::display_osd_message(std::string_view message, gdi::OsdMsgUrgency omu)
{
    this->clear_osd_message();

    if (!message.empty()) {
        std::string_view prefix = "";
        BitsPerPixel bpp = this->client_info.screen_info.bpp;

        switch (omu)
        {
        case gdi::OsdMsgUrgency::NORMAL:
            this->color = RDPColor::from(0);
            break;

        case gdi::OsdMsgUrgency::INFO:
            this->color = color_encode(NamedBGRColor::BLUE, bpp);
            prefix = "INFO: ";
            break;

        case gdi::OsdMsgUrgency::WARNING:
            this->color = color_encode(NamedBGRColor::ORANGE, bpp);
            prefix = "WARNING: ";
            break;

        case gdi::OsdMsgUrgency::ALERT:
            this->color = color_encode(NamedBGRColor::RED, bpp);
            prefix = "ALERT: ";
            break;
        }

        str_assign(this->osd_message, prefix, message, '\n', tr(trkeys::disable_osd));
        this->osd_multi_line.set_text(this->glyphs, this->osd_message);
        this->osd_message_last_width = 0;
        this->draw_osd_message(true);
    }
}

void ModWrapper::set_mod(mod_api& new_mod, windowing_api* winapi, bool enable_osd)
{
    if (!this->get_protected_rect().isempty()) {
        this->target_info_is_shown = false;
        this->disable_osd(false);
    }
    this->modi = &new_mod;
    this->winapi = winapi;
    this->enable_osd = enable_osd;
}

void ModWrapper::clear_osd_message(bool redraw)
{
    if (!this->get_protected_rect().isempty()) {
        this->target_info_is_shown = false;
        this->disable_osd(redraw);
    }
}

static void append_time_before_closing(std::string& msg, Translator tr, std::chrono::seconds elapsed_time)
{
    const auto hours_ = elapsed_time.count() / 3600;
    const auto minutes_ = elapsed_time.count() / 60 - hours_ * 60;
    const auto seconds_ = elapsed_time.count() - hours_ * 3600 - minutes_ * 60;

    const int hours = static_cast<int>(hours_);
    const int minutes = static_cast<int>(minutes_);
    const int seconds = static_cast<int>(seconds_);

    using TrFmt = Translator::FmtMsg<256>;

    auto becore_closing_msg = (hours && minutes)
        ? TrFmt(tr, trkeys::osd_hour_minute_second_before_closing, hours, minutes, seconds)
        : minutes
        ? TrFmt(tr, trkeys::osd_minute_second_before_closing, minutes, seconds)
        : TrFmt(tr, trkeys::osd_second_before_closing, seconds);

    str_append(msg, "  ["_av, becore_closing_msg, ']');
}

void ModWrapper::rdp_input_scancode(KbdFlags flags, Scancode scancode, uint32_t event_time, Keymap const& keymap)
{
    if (this->is_disable_by_input && keymap.last_kevent() == Keymap::KEvent::Insert) {
        this->disable_osd(true);
        return;
    }

    this->get_mod().rdp_input_scancode(flags, scancode, event_time, keymap);

    if (this->enable_osd && scancode == Scancode::F12 && this->enable_osd_display_remote_target) {
        bool const f12_released = bool(flags & KbdFlags::Release);
        if (this->target_info_is_shown && f12_released) {
            this->clear_osd_message();
        }
        else if (!this->target_info_is_shown && !f12_released) {
            std::string msg;
            msg.reserve(64);

            if (ini.get<cfg::globals::show_target_user_in_f12_message>()) {
                str_append(msg, ini.get<cfg::globals::target_user>(), '@');
            }

            msg += ini.get<cfg::globals::target_device>();

            if (this->end_time_session.time_since_epoch().count()) {
                const auto elapsed_time = this->end_time_session - this->time_base.monotonic_time;
                // only if "reasonable" time (1 year)
                using namespace std::chrono_literals;
                if (elapsed_time < MonotonicTimePoint::duration(60s*60*24*366)) {
                    append_time_before_closing(
                        msg, get_translator(),
                        std::chrono::duration_cast<std::chrono::seconds>(elapsed_time)
                    );
                }
            }

            if (msg != this->osd_message) {
                this->clear_osd_message();
            }

            if (!msg.empty()) {
                this->osd_multi_line.set_text(this->glyphs, msg);
                this->osd_message = std::move(msg);
                this->osd_message_last_width = 0;
                this->color = RDPColor::from(0);
                this->draw_osd_message(false);
                this->target_info_is_shown = true;
            }
        }
    }
}

void ModWrapper::rdp_input_mouse(uint16_t device_flags, uint16_t x, uint16_t y)
{
    if (this->is_disable_by_input
     && this->get_protected_rect().contains_pt(x, y)
     && device_flags == (MOUSE_FLAG_BUTTON1 | MOUSE_FLAG_DOWN)
    ) {
        this->target_info_is_shown = false;
        this->disable_osd(true);
        return;
    }

    this->get_mod().rdp_input_mouse(device_flags, x, y);
}

void ModWrapper::rdp_input_mouse_ex(uint16_t device_flags, uint16_t x, uint16_t y)
{
    this->get_mod().rdp_input_mouse_ex(device_flags, x, y);
}

void ModWrapper::rdp_input_invalidate(Rect r)
{
    if (this->get_protected_rect().isempty() || !r.has_intersection(this->get_protected_rect())) {
        this->get_mod().rdp_input_invalidate(r);
        return;
    }

    this->get_mod().rdp_input_invalidate2(gdi::subrect4(r, this->get_protected_rect()));
}

void ModWrapper::draw_osd_message(bool disable_by_input)
{
    this->is_disable_by_input = disable_by_input;

    static constexpr int padw = 16;
    static constexpr int padh = 16;

    // compute clip

    this->bogus_refresh_rect_ex
      = (this->ini.get<cfg::mod_rdp::bogus_refresh_rect>()
     && this->ini.get<cfg::client::allow_using_multiple_monitors>()
     && (this->client_info.cs_monitor.monitorCount > 1));

    int16_t dx = 0;
    uint16_t w = this->client_info.screen_info.width;

    if (this->client_info.remote_program
     && (this->winapi == static_cast<windowing_api const *>(&this->rail_client_execute)))
    {
        Rect rect = this->rail_client_execute.get_current_work_area_rect();
        w = rect.cx;
        dx = rect.x;
    }

    if (this->osd_message_last_width != w) {
        this->osd_message_last_width = w;
        this->osd_multi_line.update_dimension(w - padw);
    }

    unsigned line_width = this->osd_multi_line.max_width() + padw * 2;
    unsigned line_height = this->osd_multi_line.lines().size() * this->glyphs.max_height()
                         + padh * 2
                         + (this->is_disable_by_input ? 4 : 0)
                         ;

    Rect clip(
        dx + w < line_width ? 0 : (w - line_width) / 2,
        0,
        line_width,
        line_height
    );

    this->set_protected_rect(clip);

    // create window

    if (this->winapi) {
        this->winapi->create_auxiliary_window(clip);
    }

    // draw osd

    gdi::GraphicApi& drawable = this->gfilter.get_graphic_proxy();

    auto const bpp = this->client_info.screen_info.bpp;
    auto const color_ctx = gdi::ColorCtx::from_bpp(bpp, this->palette);
    auto const black = RDPColor::from(0);
    auto const background_color = color_encode(NamedBGRColor::LIGHT_YELLOW, bpp);

    drawable.draw(RDPOpaqueRect(clip, background_color), clip, color_ctx);

    gdi_draw_border(drawable, black, clip, 1, clip, color_ctx);

    const auto fc_height = glyphs.max_height();

    auto draw_text = [&](int16_t y, RDPColor fgcolor, gdi::MultiLineText::Line str) {
        gdi::draw_text(
            drawable,
            clip.x + padw,
            y,
            fc_height,
            gdi::DrawTextPadding{},
            str,
            fgcolor,
            background_color,
            clip
        );
    };

    auto lines = this->osd_multi_line.lines();
    int16_t dy = padh;

    if (this->is_disable_by_input) {
        lines = lines.drop_back(1);
    }

    for (gdi::MultiLineText::Line line : lines) {
        draw_text(dy, this->color, line);
        dy += this->glyphs.max_height();
    }

    if (this->is_disable_by_input) {
        draw_text(dy + 4, black, this->osd_multi_line.lines().back());
    }
}

void ModWrapper::disable_osd(bool redraw)
{
    this->is_disable_by_input = false;
    auto const protected_rect = this->get_protected_rect();
    this->set_protected_rect(Rect{});

    if (this->bogus_refresh_rect_ex) {
        this->get_mod().rdp_suppress_display_updates();
        this->get_mod().rdp_allow_display_updates(0, 0,
            this->client_info.screen_info.width,
            this->client_info.screen_info.height);
    }

    if (this->winapi) {
        this->winapi->destroy_auxiliary_window();
    }

    if (redraw) {
        this->get_mod().rdp_input_invalidate(protected_rect);
    }
}
