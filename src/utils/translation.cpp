/*
SPDX-FileCopyrightText: 2024 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "utils/translation.hpp"
#include "utils/sugar/unique_fd.hpp"
#include "utils/sugar/scope_exit.hpp"
#include "utils/log.hpp"
#include "utils/gettext.hpp"
#include "utils/msg_translation_catalog.hpp"
#include "utils/strutils.hpp"
#include "utils/log.hpp"
#include "core/app_path.hpp"

#include <string_view>

#include <cerrno>
#include <cstring>

#include <sys/stat.h>


using namespace std::string_view_literals;

namespace
{

void log_read_file_err(char const* filename, char const* ctx)
{
    int errnum = errno;
    LOG(LOG_WARNING, "i18n: %s: %s error: %s (%d)",
        filename, ctx, strerror(errnum), errnum);
};

unsigned find_msgid_index(std::string_view msgid)
{
    for (auto& s : default_msg_translation_catalog.msgstrs) {
        if (s.msgs[0].to_sv() == msgid) {
            return checked_int(&s - default_msg_translation_catalog.msgstrs.begin());
        }
    }
    return -1u;
}

void push_msg(
    char const* filename,
    MsgTranslationCatalog& catalog,
    std::pmr::monotonic_buffer_resource& mbr,
    unsigned& last_index, std::string_view msgid, MoMsgStrIterator msgs_it)
{
    /*
     * Find msgid index
     */

    unsigned idx = -1u;

    // fast searching (when the next index is the next msgid)
    if (last_index + 1 < MsgTranslationCatalog::translation_count) {
        ++last_index;
        if (default_msg_translation_catalog.msgstrs[last_index].msgs[0].to_sv() == msgid) {
            idx = last_index;
        }
    }

    if (idx == -1u) {
        idx = find_msgid_index(msgid);
        if (idx == -1u) [[unlikely]] {
            LOG(LOG_WARNING, "i18n: %s: unknown msgid: '%.*s'",
                filename, static_cast<int>(msgid.size()), msgid.data());
            return;
        }
    }

    last_index = idx;

    /*
     * Init Plurals
     */

    auto& av = catalog.msgstrs[idx].msgs;
    auto* p = std::begin(av);
    auto* pend = std::end(av);
    while (msgs_it.has_value()) {
        if (p == pend) [[unlikely]] {
            LOG(LOG_WARNING, "i18n: %s: too many msgstr for '%.*s'",
                filename, static_cast<int>(msgid.size()), msgid.data());
            return;
        }
        auto s = msgs_it.next();
        if (!s.empty()) {
            char* buf = static_cast<char*>(mbr.allocate(s.size() + 1, 1));
            memcpy(buf, s.data(), s.size());
            buf[s.size()] = '\0';
            *p = zstring_view::from_null_terminated(buf, s.size());
        }
        ++p;
    }
}

} // anonymous namespace


void MsgTranslationCatalog::init_from_file(
    const char* filename,
    std::pmr::monotonic_buffer_resource& mbr
)
{
    unique_fd ufd{filename};
    if (!ufd) {
        log_read_file_err(filename, "open file");
        return;
    }

    struct stat statbuf;
    if (-1 == fstat(ufd.fd(), &statbuf)) {
        log_read_file_err(filename, "fstat");
        return;
    }

    constexpr int pagesize = 4096;
    std::size_t const aligned_file_size = checked_int((statbuf.st_size + pagesize - 1) / pagesize * pagesize);

    // 10M !!!
    if (aligned_file_size > 1024*1024*10) {
        log_read_file_err(filename, "big size");
        return;
    }

    void* memory = aligned_alloc(pagesize, aligned_file_size);
    if (!memory) {
        log_read_file_err(filename, "allocation");
        return;
    }
    SCOPE_EXIT(free(memory));

    writable_chars_view content{static_cast<char*>(memory), checked_int(statbuf.st_size)};

    if (statbuf.st_size != read(ufd.fd(), content.data(), content.size())) {
        log_read_file_err(filename, "read");
        return;
    }

    unsigned last_index = 0;

    MoParserCallables fns{
        .init = [&](uint32_t /*msgcount*/, uint32_t /*nplurals*/, chars_view plural_expr) {
            if (char const* p = plural.parse(plural_expr)) {
                LOG(LOG_WARNING, "i18n: %s: invalid plural expression: '%.*s' at position %ld", filename, static_cast<int>(plural_expr.size()), plural_expr.data(), p - plural_expr.data());
            }
            return true;
        },
        .push_msg = [&](chars_view msgid, chars_view /*msgid_plurals*/, MoMsgStrIterator msgs_it) {
            push_msg(filename, *this, mbr, last_index, msgid.as<std::string_view>(), msgs_it);
            return true;
        },
    };

    auto const res = parse_mo(content, fns);
    switch (res.ec) {
    case MoParserErrorCode::NoError:
        break;

    case MoParserErrorCode::BadMagicNumber:
        LOG(LOG_WARNING, "i18n: %s: bad magic number: %8X", filename, res.number);
        break;

    case MoParserErrorCode::BadVersionNumber:
        LOG(LOG_WARNING, "i18n: %s: bad version number: %8X", filename, res.number);
        break;

    case MoParserErrorCode::InvalidFormat:
        LOG(LOG_WARNING, "i18n: %s: invalid gettext format (.mo)", filename);
        break;

    case MoParserErrorCode::InvalidNPlurals:
        LOG(LOG_WARNING, "i18n: %s: invalid nplurals expression", filename);
        break;

    case MoParserErrorCode::InitError:
        LOG(LOG_WARNING, "i18n: %s: initialization error", filename);
        break;

    case MoParserErrorCode::InvalidMessage:
        LOG(LOG_WARNING, "i18n: %s: invalid msgstr format", filename);
        break;
    }
}

namespace
{
    TranslationCatalogs catalogs;
    bool loaded_catalogs[2] {};
}

zstring_view Translation::translate(TrKey k) const
{
    if (!loaded_catalogs[underlying_cast(lang)]) [[unlikely]] {
        chars_view str_lang = "en"_av;
        switch (lang) {
        case Language::fr:
            str_lang = "fr"_av;
            break;
        case Language::en:
            break;
        }

        loaded_catalogs[underlying_cast(lang)] = true;
        catalogs.read_file(lang, str_concat(app_path(AppPath::Share), "/locale/"_av, str_lang, "/LC_MESSAGES/redemption.mo"_av).c_str());
    }

    auto const catalog = catalogs.catalogs()[underlying_cast(lang)];
    return catalog.msgstrs[k.index].msgs[0];
}
