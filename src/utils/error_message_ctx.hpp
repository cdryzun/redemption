/*
SPDX-FileCopyrightText: 2024 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "utils/trkey.hpp"
#include "utils/sugar/zstring_view.hpp"


class ErrorMessageCtx
{
public:
    ErrorMessageCtx() = default;
    ErrorMessageCtx(ErrorMessageCtx const&) = delete;
    ErrorMessageCtx& operator=(ErrorMessageCtx const&) = delete;

    void set_msg(TrKey k)
    {
        tr_index = static_cast<int>(k.index);
        translated_msg.clear();
    }

    void set_msg(zstring_view msg)
    {
        tr_index = -1;
        translated_msg = msg;
    }

    void set_msg(std::string&& msg)
    {
        tr_index = -1;
        translated_msg = std::move(msg);
    }

    void clear()
    {
        tr_index = -1;
        translated_msg.clear();
    }

    template<class F>
    decltype(auto) visit_msg(F&& f) const
    {
        if (tr_index < 0) {
            return f(zstring_view(translated_msg));
        }
        return f(TrKey{static_cast<unsigned>(tr_index)});
    }

private:
    int tr_index = -1;
    std::string translated_msg;
};
