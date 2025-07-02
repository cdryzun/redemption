/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "test_only/test_framework/redemption_unit_tests.hpp"

#include "mod/vnc/newline_convert.hpp"


RED_AUTO_TEST_CASE(TestInPlaceWindowsToLinuxNewLineConverter0)
{
    char rawbuf[32];
    auto buf = make_writable_array_view(rawbuf);

#define TEST(s, r) do {                                               \
    std::string in = s; /*NOLINT*/                                    \
    std::string in2 = s; /*NOLINT*/                                   \
    auto expected = r ""_av;                                          \
    auto minilen = std::min(expected.size(), size_t(4));              \
    RED_CHECK(windows_to_linux_newline_convert(in, buf) == expected); \
    RED_CHECK(windows_to_linux_newline_convert(in, buf.first(4)) ==   \
        expected.first(minilen));                                     \
    RED_CHECK(windows_to_linux_newline_convert(in,                    \
        make_writable_array_view(in)) == expected);                   \
    RED_CHECK(windows_to_linux_newline_convert(in2,                   \
        writable_array_view(in2.data(), minilen)) ==                  \
            expected.first(minilen));                                 \
} while(0)

    TEST("", "");
    TEST("\r\r", "\r\r");
    TEST("\n\n", "\n\n");
    TEST("\r\r\r\r", "\r\r\r\r");
    TEST("\n\n\n\n", "\n\n\n\n");
    TEST("\n\n\r\n", "\n\n\n");
    TEST("\n\r\n\n", "\n\n\n");
    TEST("\r\r\n\r", "\r\n\r");
    TEST("\r\n\r\r", "\n\r\r");
    TEST("\r \n", "\r \n");
    TEST("\r\r \n", "\r\r \n");
    TEST("\n \r", "\n \r");
    TEST("\n\n \r", "\n\n \r");
    TEST("\r\n", "\n");
    TEST("\r\n\r\n", "\n\n");
    TEST("\r\r\n", "\r\n");
    TEST("\n\r\n", "\n\n");
    TEST("\r\ntoto", "\ntoto");
    TEST("\r\nto\r\nto", "\nto\nto");
    TEST("\r\nto\r\nto\r\n", "\nto\nto\n");
    TEST("\r\nto\r\nto\r\n!", "\nto\nto\n!");
    TEST("toto", "toto");
    TEST("toto\r\n", "toto\n");
    TEST("to\r\nto", "to\nto");
    TEST("to\r\nto\r\n", "to\nto\n");
    TEST("to\r\nto\r\n!", "to\nto\n!");

#undef TEST
}

RED_AUTO_TEST_CASE(TestLinuxToWindowsNewLineConverter)
{
    std::array<char, 15> buf;

    RED_CHECK_EXCEPTION_ERROR_ID(
        linux_to_windows_newline_convert("text"_av, nullptr),
        ERR_STREAM_MEMORY_TOO_SMALL
    );

    RED_CHECK_EXCEPTION_ERROR_ID(
        linux_to_windows_newline_convert("text"_av, make_writable_array_view(
            make_writable_array_view(buf).first(2)
        )),
        ERR_STREAM_MEMORY_TOO_SMALL
    );

    RED_CHECK_EXCEPTION_ERROR_ID(
        linux_to_windows_newline_convert("abcdefg\nABCDEFG"_av, make_writable_array_view(
            make_writable_array_view(buf).first(15)
        )),
        ERR_STREAM_MEMORY_TOO_SMALL
    );

#define TEST(n, input, output)                                                         \
    do {                                                                               \
        char d[n];                                                                     \
        RED_CHECK(                                                                     \
            linux_to_windows_newline_convert(input ""_av, make_writable_array_view(d)) \
            == output ""_av                                                            \
        );                                                                             \
    } while (0)

    TEST(8, "", "");
    TEST(8, "\0", "\0");
    TEST(8, "toto", "toto");
    TEST(8, "toto\n", "toto\r\n");
    TEST(8, "toto\n\0", "toto\r\n\0");
    TEST(8, "\ntoto", "\r\ntoto");
    TEST(8, "to\nto", "to\r\nto");
    TEST(12, "\nto\nto\n", "\r\nto\r\nto\r\n");
    TEST(16, "abcdefg\nABCDEFG", "abcdefg\r\nABCDEFG");

#undef TEST
}
