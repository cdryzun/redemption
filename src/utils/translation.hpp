/*
SPDX-FileCopyrightText: 2024 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "configs/autogen/enums.hpp" // Language
#include "cxx/diagnostic.hpp"
#include "utils/trkey.hpp"
#include "utils/sugar/zstring_view.hpp"
#include "utils/sugar/bounded_array_view.hpp"
#include "utils/gettext.hpp"

#include <array>
#include <memory_resource>


struct MsgTranslationCatalog
{
    static constexpr std::size_t nplural_max = 1;

    #define TR_KV(name, msg) +1
    #define TR_KV_FMT TR_KV

    static constexpr std::size_t translation_count = 0
        #include "utils/trkeys_def.hpp"
    ;

    #undef TR_KV
    #undef TR_KV_FMT

    struct Plurals
    {
         std::array<zstring_view, nplural_max> plurals;
    };

    GettextPlural plural;

    std::array<Plurals, translation_count> msgstrs;

    void init_from_file(const char* filename, std::pmr::monotonic_buffer_resource& mbr);

    zstring_view msgid(TrKey k) const noexcept
    {
        return msgstrs[k.index].plurals[0];
    }

    static MsgTranslationCatalog const& default_catalog() noexcept;
};


struct Translator
{
    Translator(MsgTranslationCatalog const& catalog) noexcept
      : catalog(&catalog)
    {}

    Translator(MsgTranslationCatalog const&& catalog) = delete;

    [[nodiscard]] zstring_view operator()(TrKey k) const noexcept;

    template<class T, class... Ts>
    auto fmt(writable_chars_view av, TrKeyFmt<T> k, Ts... xs) const noexcept
    -> decltype(zstring_view::from_null_terminated("", T::check_printf_result(av.data(), av.size(), xs...)))
    {
        REDEMPTION_DIAGNOSTIC_PUSH()
        REDEMPTION_DIAGNOSTIC_GCC_IGNORE("-Wformat-nonliteral")
        auto n = std::snprintf(av.data(), av.size(), (*this)(TrKey{k.index}).c_str(), xs...);
        REDEMPTION_DIAGNOSTIC_POP()

        if (n <= 0) [[unlikely]] {
            return ""_zv;
        }
        if (static_cast<std::size_t>(n) >= av.size()) [[unlikely]] {
            if (av.empty()) {
                return ""_zv;
            }
            av.back() = '\0';
            --n;
        }

        return zstring_view::from_null_terminated(av.data(), static_cast<std::size_t>(n));
    }

    template<std::size_t N>
    struct FmtMsg : zstring_view
    {
        static_assert(N > 0);

        template<class T, class... Ts>
        FmtMsg(Translator tr, TrKeyFmt<T> k, Ts... xs) noexcept
            : zstring_view(tr.fmt(writable_chars_view{buffer, N}, k, xs...))
        {}

    private:
        char buffer[N];
    };

private:
    MsgTranslationCatalog const* catalog;
};

template<std::size_t N>
inline constexpr bool is_null_terminated_v<Translator::FmtMsg<N>> = true;


struct TranslationCatalogsRef
{
    using View = sized_array_view<MsgTranslationCatalog, 2>;

    TranslationCatalogsRef(View catalogs) noexcept
    : catalogs(catalogs)
    {}

    Translator operator[](Language lang) const noexcept
    {
        return Translator(catalogs[static_cast<std::size_t>(lang)]);
    }

private:
    View catalogs;
};

struct TranslationCatalogs
{
    TranslationCatalogs() noexcept;

    void init_language(Language lang, char const* filename)
    {
        // note: warn when a value is missing
        switch (lang) {
        case Language::en:
        case Language::fr:
            m_catalogs[static_cast<int>(lang)].init_from_file(filename, m_mbr);
            break;
        }
    }

    TranslationCatalogsRef catalogs() const noexcept
    {
        return make_bounded_array_view(m_catalogs);
    }

private:
    std::pmr::monotonic_buffer_resource m_mbr;
    MsgTranslationCatalog m_catalogs[TranslationCatalogsRef::View::fized_size()];
};
