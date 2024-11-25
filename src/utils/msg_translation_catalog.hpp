/*
SPDX-FileCopyrightText: 2024 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

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
         std::array<zstring_view, nplural_max> msgs;

         static constexpr Plurals make_with(zstring_view msg) noexcept
         {
             Plurals ret{};
             for (auto& s : ret.msgs) {
                 s = msg;
             }
             return ret;
         }
    };

    GettextPlural plural;

    std::array<Plurals, translation_count> msgstrs;

    void init_from_file(const char* filename, std::pmr::monotonic_buffer_resource& mbr);
};

#define TR_KV(name, msg) MsgTranslationCatalog::Plurals::make_with(msg ""_zv),
#define TR_KV_FMT TR_KV

inline constexpr MsgTranslationCatalog default_msg_translation_catalog {
    GettextPlural::constexpr_t(),
    {
        #include "utils/trkeys_def.hpp"
    }
};

#undef TR_KV
#undef TR_KV_FMT


struct TranslationCatalogs
{
    void read_file(Language lang, char const* filename)
    {
        switch (lang) {
        case Language::en:
        case Language::fr:
            m_catalogs[static_cast<int>(lang)].init_from_file(filename, mbr);
            break;
        }
    }

    sized_array_view<MsgTranslationCatalog, 2> catalogs() const noexcept
    {
        return make_bounded_array_view(m_catalogs);
    }

private:
    std::pmr::monotonic_buffer_resource mbr;
    MsgTranslationCatalog m_catalogs[2]{
        default_msg_translation_catalog,
        default_msg_translation_catalog,
    };
};
