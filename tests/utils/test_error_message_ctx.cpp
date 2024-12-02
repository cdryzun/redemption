/*
SPDX-FileCopyrightText: 2024 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "test_only/test_framework/redemption_unit_tests.hpp"

#include "translation/trkeys.hpp"
#include "utils/error_message_ctx.hpp"
#include "utils/sugar/int_to_chars.hpp"

RED_AUTO_TEST_CASE(TestErrMsgCtx)
{
    ErrorMessageCtx ctx;

    int_to_zchars_result r;
    auto visitor = [&](TrKey const* k, zstring_view s) {
        if (k) {
            r = int_to_decimal_zchars(k->index);
            return r.zv();
        }
        return s;
    };

    RED_CHECK(ctx.visit_msg(visitor) == ""_av);

    auto k = trkeys::err_transport_tls_certificate_changed;
    auto ki = int_to_decimal_zchars(k.index);

    ctx.set_msg(k);
    RED_CHECK(ctx.visit_msg(visitor) == ki.zv());

    ctx.set_msg("bla bla"_zv);
    RED_CHECK(ctx.visit_msg(visitor) == "bla bla"_av);

    ctx.clear();
    RED_CHECK(ctx.visit_msg(visitor) == ""_av);

    ctx.set_msg("bla bla"_zv);
    RED_CHECK(ctx.visit_msg(visitor) == "bla bla"_av);

    ctx.set_msg(k);
    RED_CHECK(ctx.visit_msg(visitor) == ki.zv());
}
