/*
SPDX-FileCopyrightText: 2026 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "utils/sugar/bytes_copy.hpp"
#include "utils/sugar/bytes_view.hpp"
#include "utils/sugar/bounded_bytes_view.hpp"

// https://learn.microsoft.com/en-us/windows/win32/fileio/maximum-file-path-limitation?tabs=registry

inline constexpr uint16_t WINNT_MAX_PATH_SIZE_INCLUDING_NULL = 260;
inline constexpr uint16_t WINNT_MAX_PATH_SIZE_WITHOUT_NULL = WINNT_MAX_PATH_SIZE_INCLUDING_NULL - 1;
inline constexpr uint16_t WINNT_MAX_DIRECTORY_PATH_SIZE_WITHOUT_NULL
    = WINNT_MAX_PATH_SIZE_WITHOUT_NULL - 12;


// Path in native code page (generally CP-1252)
struct WinNtPathView
{
    using Bytes = bounded_bytes_view<0, WINNT_MAX_PATH_SIZE_WITHOUT_NULL>;

    explicit WinNtPathView() noexcept = default;

    explicit WinNtPathView(Bytes path) noexcept
        : m_path(path)
    {}

    Bytes native() const noexcept
    {
        return m_path;
    }

    bool empty() const noexcept
    {
        return m_path.empty();
    }

    template<class Cont>
    static WinNtPathView assumed(Cont&& data)
        noexcept(noexcept(Bytes::assumed(data)))
    {
        return WinNtPathView{Bytes::assumed(data)};
    }

    static WinNtPathView assumed(Bytes::const_pointer p, Bytes::const_pointer e) noexcept
    {
        return WinNtPathView{Bytes::assumed(p, e)};
    }

    static WinNtPathView assumed(Bytes::const_pointer p, std::size_t n) noexcept
    {
        return WinNtPathView{Bytes::assumed(p, n)};
    }

private:
    Bytes m_path;
};


inline bool is_win_path_too_large(size_t len) noexcept
{
    return len > WINNT_MAX_PATH_SIZE_WITHOUT_NULL;
}

/// \return true when '\\' is required between \c dirbase and \c path.
inline bool win_dir_require_middle_separator(bytes_view dirbase, bytes_view path) noexcept
{
    return !dirbase.empty()
        && !path.empty()
        && '\\' != dirbase.back()
        && '\\' != path.front();
}

/// \return \c dir without the trailing '\\'
inline bytes_view win_dir_remove_end_separator(bytes_view dir) noexcept
{
    return (dir.empty() || '\\' != dir.back())
        ? dir
        : dir.drop_back(1);
}

/// \return true when '\\' is required at end of \c dir.
inline bool win_dir_require_end_separator(bytes_view dir) noexcept
{
    return !dir.empty()
        && '\\' != dir.back();
}

/// \return true when '\\' is required at end of concat of \c dirbase and \c path.
inline bool win_dir_require_end_separator(bytes_view dirbase, bytes_view path) noexcept
{
    return path.empty()
        ? win_dir_require_end_separator(dirbase)
        : ('\\' != path.back());
}


// for "{dirpath}\\{path}" or "{dirpath}\\{path}\\"
struct WinNtDirSep
{
    bool add_mid_sep;
    bool add_end_sep;
    bool is_truncated_path;

    enum class EndSep
    {
        Unchecked,
        Required,
    };

    WinNtDirSep(bytes_view dirbase, bytes_view path, EndSep end_sep = EndSep::Unchecked) noexcept
    {
        add_mid_sep = win_dir_require_middle_separator(dirbase, path);
        add_end_sep = (end_sep == EndSep::Required)
                   && win_dir_require_end_separator(dirbase, path);
        auto path_len = dirbase.size() + path.size() + add_mid_sep + add_end_sep;
        is_truncated_path = is_win_path_too_large(path_len);
    }

    uint32_t count_inserted_separators() const noexcept
    {
        return add_mid_sep + add_end_sep;
    }

    bytes_view mid_sep() const noexcept
    {
        return add_mid_sep ? "\\"_av : ""_av;
    }

    bytes_view end_sep() const noexcept
    {
        return add_end_sep ? "\\"_av : ""_av;
    }

    uint8_t * mid_sep(uint8_t * p) const noexcept
    {
        if (add_mid_sep)
        {
            *p++ = '\\';
        }
        return p;
    }

    uint8_t * end_sep(uint8_t * p) const noexcept
    {
        if (add_end_sep)
        {
            *p++ = '\\';
        }
        return p;
    }
};

struct WinNtDirSepFacility
{
    bytes_view dirbase;
    bytes_view path;
    WinNtDirSep dir_sep;
    std::size_t total_len;

    WinNtDirSepFacility(
        bytes_view dirbase, bytes_view path,
        WinNtDirSep::EndSep end_sep = WinNtDirSep::EndSep::Unchecked
    ) noexcept
        : dirbase { dirbase }
        , path { path }
        , dir_sep { dirbase, path, end_sep }
        , total_len { dirbase.size() + path.size() + dir_sep.count_inserted_separators() }
    {}

    bool is_total_len_too_large() const noexcept
    {
        return dir_sep.is_truncated_path;
    }

    explicit operator bool () const noexcept
    {
        return !is_total_len_too_large();
    }

    uint8_t * copy_to_unchecked(uint8_t * p) const noexcept
    {
        p = bytes_copy_and_advance(p, dirbase);
        p = dir_sep.mid_sep(p);
        p = bytes_copy_and_advance(p, path);
        p = dir_sep.end_sep(p);
        return p;
    }

    writable_bytes_view copy_to(writable_bytes_view out) const noexcept
    {
        assert(total_len <= out.size());
        return out.before(copy_to_unchecked(out.data()));
    }
};
