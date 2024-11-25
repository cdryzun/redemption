/*
SPDX-FileCopyrightText: 2024 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "configs/autogen/enums.hpp" // Language
#include "cxx/diagnostic.hpp"
#include "utils/trkey.hpp"
#include "utils/sugar/zstring_view.hpp"


struct MsgTranslationCatalog;

struct Translation
{
private:
    Language lang = Language::en;

    Translation() = default;

public:
    Translation(Translation const&) = delete;
    void operator=(Translation const&) = delete;

    static Translation& getInstance()
    {
        static Translation instance;
        return instance;
    }

    void set_lang(Language lang)
    {
        this->lang = lang;
    }

    [[nodiscard]] zstring_view translate(TrKey k) const;

    template<class T, class... Ts>
    auto translate_fmt(char* s, std::size_t n, TrKeyFmt<T> k, Ts const&... xs) const
    -> decltype(T::check_printf_result(s, n, xs...))
    {
        REDEMPTION_DIAGNOSTIC_PUSH()
        REDEMPTION_DIAGNOSTIC_GCC_IGNORE("-Wformat-nonliteral")
        return std::snprintf(s, n, translate(TrKey{k.index}).c_str(), xs...);
        REDEMPTION_DIAGNOSTIC_POP()
    }
};


inline zstring_view TR(TrKey k, Language lang)
{
    Translation::getInstance().set_lang(lang);
    return Translation::getInstance().translate(k);
}

template<class T, class... Ts>
int TR_fmt(char* s, std::size_t n, TrKeyFmt<T> k, Language lang, Ts const&... xs)
{
    Translation::getInstance().set_lang(lang);
    return Translation::getInstance().translate_fmt(s, n, k, xs...);
}

struct Translator
{
    explicit Translator(Language lang)
      : lang(lang)
    {}

    zstring_view operator()(TrKey const & k) const
    {
        return TR(k, this->lang);
    }

    template<class T, class... Ts>
    int fmt(char* s, std::size_t n, TrKeyFmt<T> k, Ts const&... xs) const
    {
        return TR_fmt(s, n, k, this->lang, xs...);
    }

private:
    Language lang;
};
