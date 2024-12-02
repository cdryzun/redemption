/*
SPDX-FileCopyrightText: 2024 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "translation/trkey.hpp"
#include "utils/sugar/array_view.hpp"
#include "utils/sugar/zstring_view.hpp"


class ErrorMessageCtx
{
public:
    ErrorMessageCtx() = default;
    ErrorMessageCtx(ErrorMessageCtx const&) = delete;
    ErrorMessageCtx& operator=(ErrorMessageCtx const&) = delete;

    void clear()
    {
        tr_index = -1;
        translated_msg_or_extra_message.clear();
    }

    bool has_msg() const noexcept
    {
        return tr_index != -1 || !translated_msg_or_extra_message.empty();
    }

    void set_msg(TrKey k)
    {
        tr_index = static_cast<int>(k.index);
        translated_msg_or_extra_message.clear();
    }

    void set_msg(TrKey k, chars_view extra_msg)
    {
        tr_index = static_cast<int>(k.index);
        translated_msg_or_extra_message.assign(extra_msg.data(), extra_msg.size());
    }

    void set_msg(TrKey k, std::string&& extra_msg)
    {
        tr_index = static_cast<int>(k.index);
        translated_msg_or_extra_message = std::move(extra_msg);
    }

    void set_msg(chars_view msg)
    {
        tr_index = -1;
        translated_msg_or_extra_message.assign(msg.data(), msg.size());
    }

    void set_msg(std::string&& msg)
    {
        tr_index = -1;
        translated_msg_or_extra_message = std::move(msg);
    }

    template<class F>
    decltype(auto) visit_msg(F&& f) const
    {
        auto k = TrKey{static_cast<unsigned>(tr_index)};
        return f(tr_index < 0 ? nullptr : &k, zstring_view(translated_msg_or_extra_message));
    }

private:
    int tr_index = -1;
    // when tr_index = -1, is a translated_msg
    std::string translated_msg_or_extra_message;
};
