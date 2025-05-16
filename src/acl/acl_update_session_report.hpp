/*
SPDX-FileCopyrightText: 2024 Wallix Proxies Team

SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "utils/out_param.hpp"
#include "utils/sugar/array_view.hpp"
#include "utils/strutils.hpp"
#include "acl/acl_report.hpp"

#include <string>
#include <cstring>

struct AclUpdateSessionReport
{
    AclUpdateSessionReport() = default;
    AclUpdateSessionReport(AclUpdateSessionReport const&) = delete;
    AclUpdateSessionReport operator=(AclUpdateSessionReport const&) = delete;

    /// Concat \p reason ':' \p target_device ':' \p message with \p out_str.
    /// When \p first_call is \c true, \p out_str is cleared, otherwise '\x01' is used as report separator.
    /// all '\x01' character in report are removed.
    void update_report(
        OutParam<std::string> out_str, bool first_call,
        AclReport acl_report, chars_view target_device)
    {
        auto& s = out_str.out_value;
        auto id = acl_report.reason_id();

        auto sep = "\x01"_av;
        if (first_call) {
            s.clear();
            sep = ""_av;
            already_set_flags = 0;
        }
        else if (already_set_flags & to_mask(id)) {
            return;
        }

        if (!acl_report.init_with_user_message()) {
            already_set_flags |= to_mask(id);
        }

        auto acl_msg = acl_report.message();
        str_append(s, sep, acl_report.reason(), ':', target_device, ':', acl_msg);
        auto added_mes = make_writable_array_view(s).last(acl_msg.size());

        // check \x01
        auto* p = static_cast<char*>(memchr(added_mes.data(), '\x01', added_mes.size()));
        if (!p) [[likely]] {
            return;
        }

        // remove \x01
        auto* e = s.data() + s.size();
        auto* cp_it = p;
        while (++p < e) {
            if (*p != '\x01') {
                *cp_it++ = *p;
            }
        }
        s.erase(static_cast<std::size_t>(cp_it - s.data()));
    }

private:
    static unsigned to_mask(AclReport::ReasonId id) noexcept
    {
        return 1u << static_cast<unsigned>(id);
    }

    unsigned already_set_flags = 0;
};
