/*
SPDX-FileCopyrightText: 2024 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "configs/autogen/enums.hpp" // Language
#include "cxx/diagnostic.hpp"
#include "translation/trkey.hpp"
#include "translation/gettext.hpp"
#include "utils/sugar/zstring_view.hpp"
#include "utils/sugar/bounded_array_view.hpp"

#include <array>
#include <memory_resource>


struct MsgTranslationCatalog
{
    static constexpr std::size_t nplural_max = 2;

    #define TR_KV(...) +1
    #define TR_KV_FMT TR_KV
    #define TR_KV_PLURAL_FMT TR_KV

    static constexpr std::size_t translation_count = 0
        #include "translation/trkeys_def.hpp"
    ;

    #undef TR_KV
    #undef TR_KV_FMT
    #undef TR_KV_PLURAL_FMT

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

    [[nodiscard]] zstring_view operator()(TrKey k) const noexcept
    {
        return tr(k);
    }

    [[nodiscard]] zstring_view tr(TrKey k) const noexcept
    {
        return catalog->msgstrs[k.index].plurals[0];
    }

    [[nodiscard]] zstring_view ntr(std::size_t n, TrKey k) const noexcept
    {
        auto nplural = catalog->plural.eval(n);
        if (nplural > MsgTranslationCatalog::nplural_max - 1) {
            nplural = MsgTranslationCatalog::nplural_max - 1;
        }
        return catalog->msgstrs[k.index].plurals[nplural];
    }

    template<class T, class... Ts>
    auto fmt(writable_chars_view av, TrKeyFmt<T> k, Ts... xs) const noexcept
    -> decltype(zstring_view::from_null_terminated("", T::check_printf_result(av.data(), av.size(), xs...)))
    {
        REDEMPTION_DIAGNOSTIC_PUSH()
        REDEMPTION_DIAGNOSTIC_GCC_IGNORE("-Wformat-nonliteral")
        auto len = std::snprintf(av.data(), av.size(), tr(TrKey{k.index}).c_str(), xs...);
        REDEMPTION_DIAGNOSTIC_POP()
        return formatted_to_zstr(av, len);
    }

    struct UInt
    {
        UInt(short n) noexcept : n(static_cast<std::size_t>(n < 0 ? -n : n)) {}
        UInt(int n) noexcept : n(static_cast<std::size_t>(n < 0 ? -n : n)) {}
        UInt(long n) noexcept : n(static_cast<std::size_t>(n < 0 ? -n : n)) {}
        UInt(long long n) noexcept : n(static_cast<std::size_t>(n < 0 ? -n : n)) {}

        UInt(unsigned short n) noexcept : n(n) {}
        UInt(unsigned n) noexcept : n(n) {}
        UInt(unsigned long n) noexcept : n(n) {}
        UInt(unsigned long long n) noexcept : n(n) {}

        std::size_t n;
    };

    /// Translate message and choose plural form.
    template<class T, class... Ts>
    auto nfmt(UInt n, writable_chars_view av, TrKeyPluralFmt<T> k, Ts... xs) const noexcept
    -> decltype(zstring_view::from_null_terminated("", T::check_printf_result(av.data(), av.size(), xs...)))
    {
        REDEMPTION_DIAGNOSTIC_PUSH()
        REDEMPTION_DIAGNOSTIC_GCC_IGNORE("-Wformat-nonliteral")
        auto len = std::snprintf(av.data(), av.size(), ntr(n.n, TrKey{k.index}).c_str(), xs...);
        REDEMPTION_DIAGNOSTIC_POP()
        return formatted_to_zstr(av, len);
    }

    /// Translate message and choose plural form.
    template<class T, class Int>
    auto nfmt(writable_chars_view av, TrKeyPluralFmt<T> k, Int n) const noexcept
    -> decltype(this->nfmt(av, n, k, n))
    {
        return this->nfmt(av, n, k, n);
    }

    template<std::size_t N>
    struct FmtMsg : zstring_view
    {
        static_assert(N > 0);

        template<class T, class... Ts>
        FmtMsg(Translator tr, TrKeyFmt<T> k, Ts... xs) noexcept
            : zstring_view(tr.fmt(writable_chars_view{buffer, N}, k, xs...))
        {}

        /// Translate message and choose plural form.
        template<class T, class Int>
        FmtMsg(Translator tr, TrKeyPluralFmt<T> k, Int n) noexcept
            : FmtMsg(tr, n, k, n)
        {}

        /// Translate message and choose plural form.
        template<class T, class... Ts>
        FmtMsg(Translator tr, UInt n, TrKeyPluralFmt<T> k, Ts... xs) noexcept
            : zstring_view(tr.nfmt(n.n, writable_chars_view{buffer, N}, k, xs...))
        {}

    private:
        char buffer[N];
    };

private:
    static zstring_view formatted_to_zstr(writable_chars_view av, int len)
    {
        if (len <= 0) [[unlikely]] {
            return ""_zv;
        }
        if (static_cast<std::size_t>(len) >= av.size()) [[unlikely]] {
            if (av.empty()) {
                return ""_zv;
            }
            av.back() = '\0';
            --len;
        }

        return zstring_view::from_null_terminated(av.data(), static_cast<std::size_t>(len));
    }

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
