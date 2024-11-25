/*
SPDX-FileCopyrightText: 2024 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "test_only/test_framework/redemption_unit_tests.hpp"

#include "utils/error_message_ctx.hpp"
#include "utils/trkeys.hpp"

RED_AUTO_TEST_CASE(TestErrMsgCtx)
{
    ErrorMessageCtx ctx;
    RED_CHECK(ctx.get_msg() == ""_av);
    RED_CHECK(ctx.get_translated_msg(Language::fr) == ""_av);

    auto k = trkeys::err_transport_tls_certificate_changed;
    // TODO
    // RED_CHECK(TR(k, Language::en) != TR(k, Language::fr));
    ctx.set_msg(k);
    RED_CHECK(ctx.get_msg() == TR(k, Language::en));
    RED_CHECK(ctx.get_translated_msg(Language::fr) == TR(k, Language::fr));

    ctx.set_msg("bla bla"_zv);
    RED_CHECK(ctx.get_msg() == "bla bla");
    RED_CHECK(ctx.get_translated_msg(Language::fr) == "bla bla"_av);

    ctx.clear();
    RED_CHECK(ctx.get_msg() == "");
    RED_CHECK(ctx.get_translated_msg(Language::fr) == ""_av);

    ctx.set_msg("bla bla"_zv);
    RED_CHECK(ctx.get_msg() == "bla bla");
    RED_CHECK(ctx.get_translated_msg(Language::fr) == "bla bla"_av);

    ctx.set_msg(k);
    RED_CHECK(ctx.get_msg() == TR(k, Language::en));
    RED_CHECK(ctx.get_translated_msg(Language::fr) == TR(k, Language::fr));
}
