/*
SPDX-FileCopyrightText: 2024 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "test_only/test_framework/redemption_unit_tests.hpp"

#include "utils/translation.hpp"
#include "utils/trkeys.hpp"


RED_AUTO_TEST_CASE(TestTranslation)
{
    Translator tr(MsgTranslationCatalog::default_catalog());

    RED_CHECK(
        Translator::FmtMsg<128>(tr, trkeys::fmt_field_required, "'XY'").to_av()
        == "Error: 'XY' field is required."_av
    );

    RED_CHECK_EQUAL(tr(trkeys::login), "Login");
}
