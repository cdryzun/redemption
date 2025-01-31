/*
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

Product name: redemption, a FLOSS RDP proxy
Copyright (C) Wallix 2021
Author(s): Proxies Team
*/

#include "test_only/test_framework/redemption_unit_tests.hpp"
#include "test_only/test_framework/check_img.hpp"

#include "mod/null/null.hpp"
#include "mod/rdp/channels/rail_session_manager.hpp"
#include "core/RDP/capabilities/window.hpp"

#include "test_only/front/fake_front.hpp"
#include "test_only/core/font.hpp"
#include "test_only/mod/internal/widget/notify_trace.hpp"


#define IMG_TEST_PATH FIXTURES_PATH "/img_ref/mod/rdp/channels/rail_session_manager/"

RED_AUTO_TEST_CASE(TestRailSessionManager)
{
    ScreenInfo screen_info {
        .width = 300,
        .height = 150,
        .bpp = BitsPerPixel::BitsPP24,
    };

    Theme theme;
    EventContainer events;
    auto& evs = detail::ProtectedEventContainer::get_events(events);
    Translator translator = MsgTranslationCatalog::default_catalog();
    FakeFront front(screen_info);
    ClientExecute client_execute(events.get_time_base(), front.gd(), front, {}, {});
    null_mod mod;
    const Font& font = global_font_deja_vu_14();
    RemoteProgramsSessionManager rpsm(
        events, front.gd(), mod, translator, font, theme, "Probe title",
        &client_execute, std::chrono::milliseconds(0), RDPVerbose()
    );

    rpsm.set_drawable(&front.gd());
    client_execute.enable_remote_program(true);
    client_execute.adjust_rect({0, 0, screen_info.width, screen_info.height});

    RDP::RAIL::ActivelyMonitoredDesktop actively_monitor;
    actively_monitor.NumWindowIds(1);
    actively_monitor.header.FieldsPresentFlags(RDP::RAIL::WINDOW_ORDER_FIELD_DESKTOP_ZORDER);
    actively_monitor.ActiveWindowId(0);

    RED_CHECK(evs.size() == 0);
    rpsm.draw(actively_monitor);
    RED_CHECK(evs.size() == 0);

    actively_monitor.NumWindowIds(0);

    rpsm.draw(actively_monitor);
    RED_REQUIRE(evs.size() == 1);
    evs[0]->actions.exec_timeout(*evs[0]);

    RED_CHECK_IMG(front, IMG_TEST_PATH "disconnect_button.png");
    RED_CHECK_THROW(rpsm.input_mouse(SlowPath::PTRFLAGS_BUTTON1, 150, 110), Error);
}
