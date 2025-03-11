/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "test_only/test_framework/redemption_unit_tests.hpp"

#include "core/WinNT/path.hpp"

RED_AUTO_TEST_CASE(TestWinNtPath)
{
    char buf[] = "truncated=0 mid(0)='  '| end(0)='  '|";

    auto push_sep = [&](unsigned i, bytes_view sep){
        if (sep.empty())
        {
            buf[i + 0] = '\'';
            buf[i + 1] = ' ';
            buf[i + 2] = ' ';
        }
        else if (sep.size() == 1)
        {
            buf[i + 0] = sep.as_charp()[0];
            buf[i + 1] = '\'';
            buf[i + 2] = ' ';
        }
        else
        {
            buf[i + 0] = sep.as_charp()[0];
            buf[i + 1] = sep.as_charp()[1];
            buf[i + 2] = '\'';
        }
        return chars_view{buf, i+4};
    };

    static constexpr auto end_nocheck = WinNtDirSep::EndSep::Unchecked;
    static constexpr auto end_slash = WinNtDirSep::EndSep::Required;

    auto dir = [&](bytes_view dir, bytes_view path, WinNtDirSep::EndSep end_sep) {
        WinNtDirSep dir_sep {dir, path, end_sep};
        buf[10] = dir_sep.is_truncated_path ? '1' : '0';
        buf[16] = dir_sep.add_mid_sep ? '1' : '0';
        push_sep(20, dir_sep.mid_sep());
        buf[29] = dir_sep.add_end_sep ? '1' : '0';
        return push_sep(33, dir_sep.end_sep());
    };

    char buf_without_sep[WINNT_MAX_PATH_SIZE_WITHOUT_NULL] {};
    char buf_with_sep[WINNT_MAX_PATH_SIZE_WITHOUT_NULL] {};
    buf_with_sep[0] = '\\';
    buf_with_sep[WINNT_MAX_PATH_SIZE_WITHOUT_NULL - 1] = '\\';
    auto max_with_sep = make_array_view(buf_with_sep);
    auto max_without_sep = make_array_view(buf_without_sep);
    auto lsep = max_with_sep.first(10);
    auto rsep = max_with_sep.last(10);
    auto empty = ""_av;
    auto a = "a"_av;
    auto b = "b"_av;

    RED_CHECK(dir(empty, empty, end_slash)        == "truncated=0 mid(0)=''  | end(0)=''  |"_av);
    RED_CHECK(dir(a, empty, end_slash)            == "truncated=0 mid(0)=''  | end(1)='\\' |"_av);
    RED_CHECK(dir("a\\"_av, empty, end_slash)     == "truncated=0 mid(0)=''  | end(0)=''  |"_av);
    RED_CHECK(dir(empty, b, end_slash)            == "truncated=0 mid(0)=''  | end(1)='\\' |"_av);
    RED_CHECK(dir(empty, "b\\"_av, end_slash)     == "truncated=0 mid(0)=''  | end(0)=''  |"_av);
    RED_CHECK(dir(a, b, end_slash)                == "truncated=0 mid(1)='\\' | end(1)='\\' |"_av);
    RED_CHECK(dir(a, "b\\"_av, end_slash)         == "truncated=0 mid(1)='\\' | end(0)=''  |"_av);
    RED_CHECK(dir("a\\"_av, "b\\"_av, end_slash)  == "truncated=0 mid(0)=''  | end(0)=''  |"_av);
    RED_CHECK(dir("a\\"_av, b, end_slash)         == "truncated=0 mid(0)=''  | end(1)='\\' |"_av);
    RED_CHECK(dir(max_with_sep, empty, end_slash) == "truncated=0 mid(0)=''  | end(0)=''  |"_av);
    RED_CHECK(dir(empty, max_with_sep, end_slash) == "truncated=0 mid(0)=''  | end(0)=''  |"_av);
    RED_CHECK(dir(max_with_sep, a, end_slash)     == "truncated=1 mid(0)=''  | end(1)='\\' |"_av);
    RED_CHECK(dir(a, max_with_sep, end_slash)     == "truncated=1 mid(0)=''  | end(0)=''  |"_av);
    RED_CHECK(dir(lsep, empty, end_slash)         == "truncated=0 mid(0)=''  | end(1)='\\' |"_av);
    RED_CHECK(dir(empty, lsep, end_slash)         == "truncated=0 mid(0)=''  | end(1)='\\' |"_av);
    RED_CHECK(dir(lsep, a, end_slash)             == "truncated=0 mid(1)='\\' | end(1)='\\' |"_av);
    RED_CHECK(dir(a, lsep, end_slash)             == "truncated=0 mid(0)=''  | end(1)='\\' |"_av);
    RED_CHECK(dir(rsep, empty, end_slash)         == "truncated=0 mid(0)=''  | end(0)=''  |"_av);
    RED_CHECK(dir(empty, rsep, end_slash)         == "truncated=0 mid(0)=''  | end(0)=''  |"_av);
    RED_CHECK(dir(rsep, a, end_slash)             == "truncated=0 mid(0)=''  | end(1)='\\' |"_av);
    RED_CHECK(dir(a, rsep, end_slash)             == "truncated=0 mid(1)='\\' | end(0)=''  |"_av);
    RED_CHECK(dir(max_without_sep, empty, end_slash) == "truncated=1 mid(0)=''  | end(1)='\\' |"_av);
    RED_CHECK(dir(empty, max_without_sep, end_slash) == "truncated=1 mid(0)=''  | end(1)='\\' |"_av);
    RED_CHECK(dir(max_without_sep, a, end_slash)     == "truncated=1 mid(1)='\\' | end(1)='\\' |"_av);
    RED_CHECK(dir(a, max_without_sep, end_slash)     == "truncated=1 mid(1)='\\' | end(1)='\\' |"_av);

    RED_CHECK(dir(empty, empty, end_nocheck)        == "truncated=0 mid(0)=''  | end(0)=''  |"_av);
    RED_CHECK(dir(a, empty, end_nocheck)            == "truncated=0 mid(0)=''  | end(0)=''  |"_av);
    RED_CHECK(dir("a\\"_av, empty, end_nocheck)     == "truncated=0 mid(0)=''  | end(0)=''  |"_av);
    RED_CHECK(dir(empty, b, end_nocheck)            == "truncated=0 mid(0)=''  | end(0)=''  |"_av);
    RED_CHECK(dir(empty, "b\\"_av, end_nocheck)     == "truncated=0 mid(0)=''  | end(0)=''  |"_av);
    RED_CHECK(dir(a, b, end_nocheck)                == "truncated=0 mid(1)='\\' | end(0)=''  |"_av);
    RED_CHECK(dir(a, "b\\"_av, end_nocheck)         == "truncated=0 mid(1)='\\' | end(0)=''  |"_av);
    RED_CHECK(dir("a\\"_av, "b\\"_av, end_nocheck)  == "truncated=0 mid(0)=''  | end(0)=''  |"_av);
    RED_CHECK(dir("a\\"_av, b, end_nocheck)         == "truncated=0 mid(0)=''  | end(0)=''  |"_av);
    RED_CHECK(dir(max_with_sep, empty, end_nocheck) == "truncated=0 mid(0)=''  | end(0)=''  |"_av);
    RED_CHECK(dir(empty, max_with_sep, end_nocheck) == "truncated=0 mid(0)=''  | end(0)=''  |"_av);
    RED_CHECK(dir(max_with_sep, a, end_nocheck)     == "truncated=1 mid(0)=''  | end(0)=''  |"_av);
    RED_CHECK(dir(a, max_with_sep, end_nocheck)     == "truncated=1 mid(0)=''  | end(0)=''  |"_av);
    RED_CHECK(dir(lsep, empty, end_nocheck)         == "truncated=0 mid(0)=''  | end(0)=''  |"_av);
    RED_CHECK(dir(empty, lsep, end_nocheck)         == "truncated=0 mid(0)=''  | end(0)=''  |"_av);
    RED_CHECK(dir(lsep, a, end_nocheck)             == "truncated=0 mid(1)='\\' | end(0)=''  |"_av);
    RED_CHECK(dir(a, lsep, end_nocheck)             == "truncated=0 mid(0)=''  | end(0)=''  |"_av);
    RED_CHECK(dir(rsep, empty, end_nocheck)         == "truncated=0 mid(0)=''  | end(0)=''  |"_av);
    RED_CHECK(dir(empty, rsep, end_nocheck)         == "truncated=0 mid(0)=''  | end(0)=''  |"_av);
    RED_CHECK(dir(rsep, a, end_nocheck)             == "truncated=0 mid(0)=''  | end(0)=''  |"_av);
    RED_CHECK(dir(a, rsep, end_nocheck)             == "truncated=0 mid(1)='\\' | end(0)=''  |"_av);
    RED_CHECK(dir(max_without_sep, empty, end_nocheck) == "truncated=0 mid(0)=''  | end(0)=''  |"_av);
    RED_CHECK(dir(empty, max_without_sep, end_nocheck) == "truncated=0 mid(0)=''  | end(0)=''  |"_av);
    RED_CHECK(dir(max_without_sep, a, end_nocheck)     == "truncated=1 mid(1)='\\' | end(0)=''  |"_av);
    RED_CHECK(dir(a, max_without_sep, end_nocheck)     == "truncated=1 mid(1)='\\' | end(0)=''  |"_av);

    char copy_buffer[64];
    auto cp_buf = make_writable_array_view(copy_buffer);

    auto mk_path = [&](bytes_view dirbase, bytes_view path, WinNtDirSep::EndSep end_sep) {
        WinNtDirSepFacility win_dir_sep(dirbase, path, end_sep);
        RED_CHECK(bool(win_dir_sep));
        RED_CHECK(!win_dir_sep.is_total_len_too_large());
        return win_dir_sep.copy_to(cp_buf);
    };

    RED_CHECK(mk_path("dir"_av, ""_av, WinNtDirSep::EndSep::Unchecked) == "dir"_av);
    RED_CHECK(mk_path("dir"_av, ""_av, WinNtDirSep::EndSep::Required) == "dir\\"_av);
    RED_CHECK(mk_path(""_av, "subpath"_av, WinNtDirSep::EndSep::Unchecked) == "subpath"_av);
    RED_CHECK(mk_path(""_av, "subpath"_av, WinNtDirSep::EndSep::Required) == "subpath\\"_av);
    RED_CHECK(mk_path("dir"_av, "subpath"_av, WinNtDirSep::EndSep::Unchecked) == "dir\\subpath"_av);
    RED_CHECK(mk_path("dir"_av, "subpath"_av, WinNtDirSep::EndSep::Required) == "dir\\subpath\\"_av);

    char big_buf[WINNT_MAX_PATH_SIZE_WITHOUT_NULL + 1];
    RED_CHECK(!WinNtDirSepFacility(make_array_view(big_buf), ""_av));
}
