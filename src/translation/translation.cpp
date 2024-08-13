/*
SPDX-FileCopyrightText: 2024 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "translation/translation.hpp"
#include "translation/gettext.hpp"
#include "utils/sugar/unique_fd.hpp"
#include "utils/sugar/scope_exit.hpp"
#include "utils/log.hpp"
#include "utils/strutils.hpp"
#include "utils/log.hpp"
#include "core/app_path.hpp"

#include <string_view>

#include <cassert>
#include <cerrno>
#include <cstring>

#include <sys/stat.h>


using namespace std::string_view_literals;

namespace
{

constexpr MsgTranslationCatalog::Plurals make_plurals(zstring_view msg) noexcept
{
    MsgTranslationCatalog::Plurals ret{};
    for (auto& s : ret.plurals) {
        s = msg;
    }
    return ret;
}

constexpr MsgTranslationCatalog::Plurals make_plurals(zstring_view msg, zstring_view plural_msg) noexcept
{
    MsgTranslationCatalog::Plurals ret{};
    for (auto& s : ret.plurals) {
        s = plural_msg;
    }
    ret.plurals.front() = msg;
    return ret;
}

#define TR_KV(name, msg) make_plurals(msg ""_zv),
#define TR_KV_FMT TR_KV
#define TR_KV_PLURAL_FMT(name, msg, plural_msg) make_plurals(msg ""_zv, plural_msg ""_zv),

inline constexpr MsgTranslationCatalog msgid_catalog {
    GettextPlural::plural_1_neq_n(),
    {
        #include "translation/trkeys_def.hpp"
    }
};

#undef TR_KV
#undef TR_KV_FMT
#undef TR_KV_PLURAL_FMT

void log_read_file_err(char const* filename, char const* ctx)
{
    int errnum = errno;
    LOG(LOG_WARNING, "i18n: %s: %s error: %s (%d)",
        filename, ctx, strerror(errnum), errnum);
};

unsigned find_msgid_index(std::string_view msgid)
{
    for (auto& s : msgid_catalog.msgstrs) {
        if (s.plurals[0].to_sv() == msgid) {
            return checked_int(&s - msgid_catalog.msgstrs.data());
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
        if (msgid_catalog.msgstrs[last_index].plurals[0].to_sv() == msgid) {
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

    auto& av = catalog.msgstrs[idx].plurals;
    auto p = std::begin(av);
    auto pend = std::end(av);
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

TranslationCatalogs::TranslationCatalogs() noexcept
    : m_catalogs{
        msgid_catalog,
        msgid_catalog,
    }
{
    // check that m_catalogs is full initialized
    assert(m_catalogs[std::size(m_catalogs)-1u].msgstrs[0].plurals[0].to_sv() == m_catalogs[0].msgstrs[0].plurals[0].to_sv());
}

const MsgTranslationCatalog & MsgTranslationCatalog::default_catalog() noexcept
{
    return msgid_catalog;
}

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

    auto const res = parse_mo(
        content,
        [&](uint32_t /*msgcount*/, uint32_t /*nplurals*/, chars_view plural_expr) {
            if (char const* p = plural.parse(plural_expr)) {
                LOG(LOG_WARNING, "i18n: %s: invalid plural expression: '%.*s' at position %ld", filename, static_cast<int>(plural_expr.size()), plural_expr.data(), p - plural_expr.data());
            }
            return true;
        },
        [&](chars_view msgid, chars_view /*msgid_plurals*/, MoMsgStrIterator msgs_it) {
            push_msg(filename, *this, mbr, last_index, msgid.as<std::string_view>(), msgs_it);
            return true;
        }
    );

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
