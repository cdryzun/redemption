/*
SPDX-FileCopyrightText: 2024 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "test_only/test_framework/redemption_unit_tests.hpp"

#include "translation/translation.hpp"
#include "translation/trkeys.hpp"


RED_AUTO_TEST_CASE(TestTranslation)
{
    Translator tr(MsgTranslationCatalog::default_catalog());

    RED_CHECK(
        Translator::FmtMsg<128>(tr, trkeys::fmt_field_required, "'XY'").to_av()
        == "Error: 'XY' field is required."_av
    );

    RED_CHECK(
        Translator::FmtMsg<128>(tr, 1, trkeys::close_box_minute_timer, 1).to_av()
        == "1 minute before closing."_av
    );

    RED_CHECK(
        Translator::FmtMsg<128>(tr, 3, trkeys::close_box_minute_timer, 3).to_av()
        == "3 minutes before closing."_av
    );

    RED_CHECK_EQUAL(tr(trkeys::login), "Login");
}
