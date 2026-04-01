/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "test_only/test_framework/redemption_unit_tests.hpp"

#include "utils/utf.hpp"
#include "utils/sugar/cast.hpp"
#include "utils/sugar/int_to_chars.hpp"
#include <string_view>
#include <vector>
#include <cstring>

using namespace std::string_view_literals;


RED_AUTO_TEST_CASE(TestUTF8Len)
{
    RED_CHECK_EQUAL(2u, UTF8Len("a\xC3\xA9"));
    RED_CHECK_EQUAL(11u, UTF8Len("abcedef\xC3\xA9\xC3\xA7\xC3\xA0@"));
}

RED_AUTO_TEST_CASE(TestUTF16ByteLen)
{
    RED_CHECK_EQUAL(0u, UTF16ByteLen("\0\0"_av));
    RED_CHECK_EQUAL(0u, UTF16ByteLen("\0"_av));
    RED_CHECK_EQUAL(6u, UTF16ByteLen("a\0b\0c\0\0\0"_av));
    RED_CHECK_EQUAL(6u, UTF16ByteLen("a\0b\0c\0\0\0\0"_av));
    RED_CHECK_EQUAL(8u, UTF16ByteLen("a\0\xe9\0\xe7\0\x0e\0"_av));
}

RED_AUTO_TEST_CASE(TestUTF16ToUTF8_buf)
{
    uint8_t source[24];
    auto buf = make_writable_array_view(source);
    RED_CHECK(""_av == UTF16toUTF8_buf("\0\0"_av, buf));
    RED_CHECK(""_av == UTF16toUTF8_buf("\0"_av, buf));
    RED_CHECK("abc"_av == UTF16toUTF8_buf("a\0b\0c\0\0\0"_av, buf));
    RED_CHECK("abc"_av == UTF16toUTF8_buf("a\0b\0c\0\0\0\0"_av, buf));
    RED_CHECK("a\xc3\xa9\xc3\xa7\xc3\xa0"_av == UTF16toUTF8_buf("a\0\xe9\0\xe7\0\xe0\0"_av, buf));
}

RED_AUTO_TEST_CASE(TestUTF8InsertUtf16)
{
    uint8_t source[255] = { 'a', 'b', 'c', 'e', 'd', 'e', 'f'};
    RED_CHECK(UTF8InsertUtf16(make_writable_array_view(source), 8, 'x'));
    RED_CHECK(make_array_view(source).first(9) == "xabcedef\x00"_av);
    RED_CHECK(!UTF8InsertUtf16(make_writable_array_view(source).first(9), 9, 'y'));
    RED_CHECK(make_array_view(source).first(9) == "xabcedef\x00"_av);
}

RED_AUTO_TEST_CASE(TestUTF8RemoveOneAtPos0)
{
    uint8_t source[255] = { 'a', 'b', 'c', 'e', 'd', 'e', 'f'};
    UTF8RemoveOne(make_writable_array_view(source).first(8));
    RED_CHECK(make_array_view(source).first(8) == "bcedef\x00\x00"_av);
    source[7] = 'x';
    UTF8RemoveOne(make_writable_array_view(source).drop_front(6));
    RED_CHECK(make_array_view(source).first(9) == "bcedef\x00x\00"_av);
}

RED_AUTO_TEST_CASE(TestUTF8_UTF16)
{
    uint8_t source[] = { 'a', 'b', 'c', 'e', 'd', 'e', 'f', 0xC3, 0xA9, 0xC3, 0xA7, 0xC3, 0xA0, '@', 0};
    uint8_t expected_target[] = { 'a', 0, 'b', 0, 'c', 0, 'e', 0, 'd', 0,
                                  'e', 0, 'f', 0,
                                  0xE9, 0 /* é */,
                                  0xE7, 0 /* ç */,
                                  0xE0, 0 /* à */,
                                  '@', 0, };
    const size_t target_length = sizeof(expected_target)/sizeof(expected_target[0]);
    uint8_t target[target_length];

    size_t nbbytes_utf16 = UTF8toUTF16({std::data(source), std::size(source)-1}, target, target_length);

    // Check result
    RED_CHECK_EQUAL(target_length, nbbytes_utf16);
    RED_CHECK_EQUAL_RANGES(target, expected_target);

    uint8_t source_round_trip[15];

    size_t nbbytes_utf8 = UTF16toUTF8(target, nbbytes_utf16 / 2, source_round_trip, sizeof(source_round_trip));
    RED_CHECK_EQUAL(14u, nbbytes_utf8);
}

RED_AUTO_TEST_CASE(TestUTF8_UTF16_witch_control_character)
{
    uint8_t source[] = { 'a', 'b', 'c', 'e', 'd', 'e', 'f', 0x0A, 0xC3, 0xA9, 0xC3, 0xA7, 0xC3, 0xA0, '@', 0};
    uint8_t expected_target[] = { 'a', 0, 'b', 0, 'c', 0, 'e', 0, 'd', 0,
                                  'e', 0, 'f', 0,
                                  0x0A, 0 /* newline */,
                                  0xE9, 0 /* é */,
                                  0xE7, 0 /* ç */,
                                  0xE0, 0 /* à */,
                                  '@', 0, };
    const size_t target_length = sizeof(expected_target)/sizeof(expected_target[0]);
    uint8_t target[target_length];

    size_t nbbytes_utf16 = UTF8toUTF16({std::data(source), std::size(source)-1}, target, target_length);

    // Check result
    RED_CHECK_EQUAL(target_length, nbbytes_utf16);
    RED_CHECK_EQUAL_RANGES(target, expected_target);

    uint8_t source_round_trip[16];

    size_t nbbytes_utf8 = UTF16toUTF8(target, nbbytes_utf16 / 2, source_round_trip, sizeof(source_round_trip));
    RED_CHECK_EQUAL(15u, nbbytes_utf8);
}

RED_AUTO_TEST_CASE(TestUTF8_UTF16_2)
{
    uint8_t source[] = { 'a', 'b', 'c', 'e', 'd', 'e', 'f', 0x0A, '@', 0};
    uint8_t expected_targetCr[] =   { 'a', 0, 'b', 0, 'c', 0, 'e', 0, 'd', 0,
                                      'e', 0, 'f', 0,
                                      0x0A, 0 /* newline */,
                                      '@', 0, };
    const size_t target_lengthCr   = sizeof(expected_targetCr)/sizeof(expected_targetCr[0]);
    uint8_t targetCr[target_lengthCr];

    size_t nbbytes_utf16 = UTF8toUTF16({std::data(source), std::size(source)-1}, targetCr, target_lengthCr);

    // Check result
    RED_CHECK_EQUAL(target_lengthCr, nbbytes_utf16);
    RED_CHECK_EQUAL_RANGES(targetCr, expected_targetCr);
}

RED_AUTO_TEST_CASE(TestUTF32toUTF8) {
    uint8_t buf[5]{};

    RED_REQUIRE_EQUAL(1u, UTF32toUTF8('a', buf, 4));
    RED_REQUIRE_EQUAL('a', buf[0]);

    RED_REQUIRE_EQUAL(2u, UTF32toUTF8(0xBF, buf, 4));
    RED_REQUIRE_EQUAL("¿"_av, bytes_view(buf, 2));

    RED_REQUIRE_EQUAL(3u, UTF32toUTF8(0x20AC, buf, 4));
    RED_REQUIRE_EQUAL("€"_av, bytes_view(buf, 3));

    RED_REQUIRE_EQUAL(4u, UTF32toUTF8(0x1F680, buf, 4));
    RED_REQUIRE_EQUAL("🚀"_av, bytes_view(buf, 4));
}

RED_AUTO_TEST_CASE(TestUTF8toUnicodeIterator) {
    const char * s = "Ëa\nŒo";
    UTF8toUnicodeIterator u(s);
    RED_CHECK(*u);            ++u;
    RED_CHECK_EQUAL(*u, uint8_t('a')); ++u;
    RED_CHECK_EQUAL(*u, uint8_t('\n'));++u;
    RED_CHECK(*u);            ++u;
    RED_CHECK_EQUAL(*u, uint8_t('o')); ++u;
    RED_CHECK_EQUAL(*u, uint8_t(0));
}

RED_AUTO_TEST_CASE(TestUTF16ToLatin1) {
    const uint8_t utf16_src[] = "\x74\x00\x72\x00\x61\x00\x70\x00"  // "trapézoïdal"
                                "\xe9\x00\x7a\x00\x6f\x00\xef\x00"
                                "\x64\x00\x61\x00\x6c\x00\x00\x00";

    const size_t number_of_characters = sizeof(utf16_src) / 2;

    uint8_t latin1_dst[32];

    RED_CHECK_EQUAL(
        UTF16toLatin1(utf16_src, number_of_characters * 2, latin1_dst, sizeof(latin1_dst)),
        number_of_characters);

    RED_CHECK(char_ptr_cast(latin1_dst) == "trap\xe9zo\xef" "dal"sv);
}

RED_AUTO_TEST_CASE(TestUTF16ToLatin1_1) {
    const uint8_t utf16_src[] = "\x31\x00\x30\x00\x30\x00\x20\x00"  // "100 €"
                                "\xac\x20\x00\x00";

    const size_t number_of_characters = sizeof(utf16_src) / 2;

    uint8_t latin1_dst[32] = {};

    auto x = UTF16toLatin1(utf16_src, number_of_characters * 2, latin1_dst, sizeof(latin1_dst));

    RED_CHECK_EQUAL(x, number_of_characters);

    RED_CHECK(char_ptr_cast(latin1_dst) == "100 \x80"sv);
}

RED_AUTO_TEST_CASE(TestLatin1ToUTF8) {
    auto latin1_src = "100 \x80"                 // "100 €"
                      "\n"                       // \n
                      "trap\xe9zo\xef" "dal"_av; // "trapézoïdal"

    auto utf8_expected = "100 \xc2\x80"
                         "\x0a"
                         "trap\xc3\xa9zo\xc3\xaf" "dal"_av;

    uint8_t utf8_dst[64];

    RED_CHECK(
        array_view(utf8_dst, Latin1toUTF8(byte_ptr_cast(latin1_src.data()), latin1_src.size(), utf8_dst, sizeof(utf8_dst))) ==
        utf8_expected);
}

RED_AUTO_TEST_CASE(TestUTF8StringAdjustedNbBytes) {
    RED_CHECK_EQUAL(UTF8StringAdjustedNbBytes(byte_ptr_cast(""), 6), 0u);
    RED_CHECK_EQUAL(UTF8StringAdjustedNbBytes(byte_ptr_cast("èè"), 6), 4u);
    RED_CHECK_EQUAL(UTF8StringAdjustedNbBytes(byte_ptr_cast("èè"), 3), 2u);
    RED_CHECK_EQUAL(UTF8StringAdjustedNbBytes(byte_ptr_cast("èè"), 1), 0u);
    RED_CHECK_EQUAL(UTF8StringAdjustedNbBytes(byte_ptr_cast("èè"), 0), 0u);
}

RED_AUTO_TEST_CASE(TestUtf16LowerCase)
{
    uint8_t test[] =  "\xb7\x01\x8A\x03\xda\x03\x01\x04"  /* Ʒ Ί Ϛ Ё */
                      "\x25\x04\x34\x05\x10\x1e\x09\x1f"  /* Х Դ Ḑ Ἁ */
                      "\x49\x1f\x25\xff\x52\x00\x74\x00"  /* Ὁ Ｅ R t */
                      "\x66\x2c\xc3\x1f\x3a\xff";      /* Ⱦ ΗΙ Ｚ  */
    /*
    Upper    Lower case
    0x01B7 ; 0x0292     # LATIN SMALL LETTER EZH
    0x038A ; 0x03AF     # GREEK SMALL LETTER IOTA WITH TONOS
    0x03DA ; 0x03DB     # GREEK LETTER STIGMA
    0x0401 ; 0x0451     # CYRILLIC SMALL LETTER IO

    0x0425 ; 0x0445     # CYRILLIC SMALL LETTER HA
    0x0534 ; 0x0564     # ARMENIAN SMALL LETTER DA
    0x1E10 ; 0x1E11     # LATIN SMALL LETTER D WITH CEDILLA
    0x1F09 ; 0x1F01     # GREEK SMALL LETTER ALPHA WITH DASIA

    0x1F49 ; 0x1F41     # GREEK SMALL LETTER OMICRON WITH DASIA
    0xFF25 ; 0xFF45     # FULLWIDTH LATIN SMALL LETTER E
    0x0052 ; 0x0072     # LATIN SMALL LETTER R
    0x0054 ; 0x0074     # LATIN SMALL LETTER T

    0x2C66 ; 0x023E     # LATIN SMALL LETTER T WITH DIAGONAL STROKE
    0x1FC3 ; 0x1FCC     # GREEK SMALL LETTER ETA WITH PROSGEGRAMMENI
    0xFF3A ; 0xFF5A     # FULLWIDTH LATIN SMALL LETTER Z
    */
    int number_of_elements = sizeof(test)/sizeof(test[0])-2;
    auto expected = "\x92\x02\xaf\x03\xdb\x03\x51\x04"  /* ʒ ί ϛ ё */
                    "\x45\x04\x64\x05\x11\x1e\x01\x1f"  /* х դ ḑ ἁ */
                    "\x41\x1f\x45\xff\x72\x00\x74\x00"  /* ὁ ｅ r t */
                    "\x3e\x02\xcc\x1f\x5a\xff\x00"      /* ⱦ ῃ ｚ  */
                      ""_av;
    UTF16Lower(test, number_of_elements);

    RED_CHECK(make_array_view(test) == expected);
}

RED_AUTO_TEST_CASE(TestUtf16UpperCase)
{
    uint8_t test[] =  "\x92\x02\xaf\x03\xdb\x03\x51\x04"  /* ʒ ί ϛ ё */
                      "\x45\x04\x64\x05\x11\x1e\x01\x1f"  /* х դ ḑ ἁ */
                      "\x41\x1f\x45\xff\x72\x00\x54\x00"  /* ὁ ｅ r T */
                      "\x3e\x02\xcc\x1f\x5a\xff";         /* ⱦ ῃ ｚ  */
    /*
    Lower    Upper case
    0x0292 ; 0x01B7     # LATIN CAPITAL LETTER EZH
    0x03AF ; 0x038A     # GREEK CAPITAL LETTER IOTA WITH TONOS
    0x03DB ; 0x03DA     # GREEK LETTER STIGMA
    0x0451 ; 0x0401     # CYRILLIC CAPITAL LETTER IO

    0x0445 ; 0x0425     # CYRILLIC CAPITAL LETTER HA
    0x0564 ; 0x0534     # ARMENIAN CAPITAL LETTER DA
    0x1E11 ; 0x1E10     # LATIN CAPITAL LETTER D WITH CEDILLA
    0x1F01 ; 0x1F09     # GREEK CAPITAL LETTER ALPHA WITH DASIA

    0x1F41 ; 0x1F49     # GREEK CAPITAL LETTER OMICRON WITH DASIA
    0xFF45 ; 0xFF25     # FULLWIDTH LATIN CAPITAL LETTER E
    0x0072 ; 0x0052     # LATIN CAPITAL LETTER R
    0x0074 ; 0x0054     # LATIN CAPITAL LETTER T

    0x023E ; 0x2C66     # LATIN SMALL LETTER T WITH DIAGONAL STROKE
    0x1FCC ; 0x1FC3     # GREEK SMALL LETTER ETA WITH PROSGEGRAMMENI
    0xFF5A ; 0xFF3A     # FULLWIDTH LATIN CAPITAL LETTER Z
    */
    int number_of_elements = sizeof(test)/sizeof(test[0])-2;
    auto expected = "\xb7\x01\x8A\x03\xda\x03\x01\x04"  /* Ʒ Ί Ϛ Ё */
                    "\x25\x04\x34\x05\x10\x1e\x09\x1f"  /* Х Դ Ḑ Ἁ */
                    "\x49\x1f\x25\xff\x52\x00\x54\x00"  /* Ὁ Ｅ R T */
                    "\x66\x2c\xc3\x1f\x3a\xff\x00"      /* Ⱦ ΗΙ Ｚ  */
                    ""_av;
    UTF16Upper(test, number_of_elements);

    RED_CHECK(make_array_view(test) == expected);
}


RED_AUTO_TEST_CASE(TestUTF16ToUTF8)
{
    {
        uint8_t u16_1[]{'a', 0, 'b', 0, 'c', 0, 0, 0};
        uint8_t dest[32]{'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x'};

        RED_CHECK(UTF16toUTF8(u16_1, 4, dest, sizeof(dest)) == 4u);
        RED_CHECK(char_ptr_cast(dest) == "abc"sv);
    }
    {
        uint16_t u16_2[]{'a', 'b', 'c', 0};
        uint8_t dest[32]{'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x'};

        RED_CHECK(UTF16toUTF8(u16_2, 4, dest, sizeof(dest)) == 4u);
        RED_CHECK(char_ptr_cast(dest) == "abc"sv);
    }
}

RED_AUTO_TEST_CASE(TestUTF16ToResizableUTF8)
{
    RED_CHECK(UTF16toResizableUTF8<std::vector<char>>("a\0b\0c\0"_av) == "abc"_av);
    RED_CHECK(UTF16toResizableUTF8_zstring<std::vector<char>>("a\0b\0c\0"_av) == "abc\0"_av);

    std::vector<char> result;
    UTF16toResizableUTF8_zstring("a\0b\0c\0"_av, result);
    RED_CHECK(result == "abc\0"_av);

    UTF16toResizableUTF8("a\0b\0c\0"_av, result);
    RED_CHECK(result == "abc"_av);
}

RED_AUTO_TEST_CASE(TestUTF8ToUTF16Limit)
{
    uint8_t expected_target[]{ 'a', 0, 'b', 0, 'c', 0, 'd', 0 };

    const size_t target_length = 8;
    uint8_t target[target_length];

    memset(target, 0xfe, sizeof(target));

    size_t nbbytes_utf16 = UTF8toUTF16("abcdef"_av, target, target_length);

    // Check result
    RED_CHECK_EQUAL(target_length, nbbytes_utf16);      // 8
    RED_CHECK_EQUAL_RANGES(target, expected_target);    // "\x61\x00\x62\x00\x63\x00\x64\x00"
}

RED_AUTO_TEST_CASE(TestUTF8ToUTF16)
{
    uint8_t expected_target[]{ 'a', 0, 'b', 0, 'c', 0, 0xfe, 0xfe };

    const size_t target_length = 8;
    uint8_t target[target_length];

    memset(target, 0xfe, sizeof(target));

    size_t nbbytes_utf16 = UTF8toUTF16("abc"_av, target, target_length);

    // Check result
    RED_CHECK_EQUAL(6u, nbbytes_utf16);      // 6
    RED_CHECK_EQUAL_RANGES(target, expected_target);    // "\x61\x00\x62\x00\x63\x00\x64\x00"
}

RED_AUTO_TEST_CASE(TestUTF8ToResizableUTF16)
{
    RED_CHECK(UTF8toResizableUTF16<std::vector<char>>("abc"_av) == "a\0b\0c\0"_av);
    RED_CHECK(UTF8toResizableUTF16_zstring<std::vector<char>>("abc"_av) == "a\0b\0c\0\0\0"_av);

    std::vector<char> result;
    UTF8toResizableUTF16_zstring("abc"_av, result);
    RED_CHECK(result == "a\0b\0c\0\0\0"_av);

    UTF8toResizableUTF16("abc"_av, result);
    RED_CHECK(result == "a\0b\0c\0"_av);
}

RED_AUTO_TEST_CASE(Test_is_ASCII_string)
{
    RED_CHECK(is_ASCII_string(byte_ptr_cast("abcd")));
    RED_CHECK(!is_ASCII_string(byte_ptr_cast("éric")));
    RED_CHECK(is_ASCII_string(byte_ptr_cast("")));
}

static auto push_utf8_char(std::string& s, char prefix)
{
    return [&, prefix](utf8_char_generic utf8_char) {
        s += prefix;
        s += utf8_char.bytes().as_chars().as<std::string_view>();
        s += '[';
        s += int_to_hexadecimal_upper_chars(utf8_char.unicode()).sv();
        s += ']';
    };
};

RED_AUTO_TEST_CASE(Test_utf8_for_each)
{
    std::string result;

    auto push_utf_truncated = [&](bytes_view remaining){
        result += '?'; result += remaining.as_chars().as<std::string_view>();
    };

    auto utf8_for_each_fn = [&](bytes_view utf8)
    {
        result.clear();
        utf8_for_each(utf8,
            push_utf8_char(result, '='),
            push_utf8_char(result, '!'),
            push_utf_truncated
        );
        return result;
    };

    auto utf8_for_each_er = [&](bytes_view utf8)
    {
        result.clear();
        utf8_for_each(utf8,
            push_utf8_char(result, '='),
            [&](auto ch) { push_utf8_char(result, '!')(ch); return false; },
            push_utf_truncated
        );
        return result;
    };

    auto utf8_for_each_c1 = [&](bytes_view utf8)
    {
        result.clear();
        utf8_for_each(utf8,
            [&](auto ch) {
                push_utf8_char(result, '=')(ch);
                return utf8_decode_new_offset{ch.bytes().data() + 1};
            },
            push_utf8_char(result, '!'),
            push_utf_truncated
        );
        return result;
    };

    RED_CHECK(utf8_for_each_fn("a"_av) == "=a[61]"_av);
    RED_CHECK(utf8_for_each_er("a"_av) == "=a[61]"_av);
    RED_CHECK(utf8_for_each_fn("abc"_av) == "=a[61]=b[62]=c[63]"_av);
    RED_CHECK(utf8_for_each_er("abc"_av) == "=a[61]=b[62]=c[63]"_av);
    RED_CHECK(utf8_for_each_fn("abcde"_av) == "=a[61]=b[62]=c[63]=d[64]=e[65]"_av);
    RED_CHECK(utf8_for_each_er("abcde"_av) == "=a[61]=b[62]=c[63]=d[64]=e[65]"_av);

    RED_CHECK(utf8_for_each_fn("€"_av) == "=€[20AC]"_av);
    RED_CHECK(utf8_for_each_er("€"_av) == "=€[20AC]"_av);
    RED_CHECK(utf8_for_each_fn("🚀"_av) == "=🚀[1F680]"_av);
    RED_CHECK(utf8_for_each_er("🚀"_av) == "=🚀[1F680]"_av);
    RED_CHECK(utf8_for_each_fn("𡞰"_av) == "=𡞰[217B0]"_av);
    RED_CHECK(utf8_for_each_er("𡞰"_av) == "=𡞰[217B0]"_av);
    RED_CHECK(utf8_for_each_fn("\x80"_av) == "!\x80[FFFD]"_av);
    RED_CHECK(utf8_for_each_er("\x80"_av) == "!\x80[FFFD]"_av);
    RED_CHECK(utf8_for_each_fn("\xC0"_av) == "?\xC0"_av);
    RED_CHECK(utf8_for_each_er("\xC0"_av) == "?\xC0"_av);
    RED_CHECK(utf8_for_each_fn("\xF0\xAA"_av) == "?\xF0\xAA"_av);
    RED_CHECK(utf8_for_each_er("\xF0\xAA"_av) == "?\xF0\xAA"_av);
    RED_CHECK(utf8_for_each_fn("a𡞰b"_av) == "=a[61]=𡞰[217B0]=b[62]"_av);
    RED_CHECK(utf8_for_each_er("a𡞰b"_av) == "=a[61]=𡞰[217B0]=b[62]"_av);
    RED_CHECK(utf8_for_each_fn("a\xA0""b"_av) == "=a[61]!\xA0[FFFD]=b[62]"_av);
    RED_CHECK(utf8_for_each_er("a\xA0""b"_av) == "=a[61]!\xA0[FFFD]"_av);
    RED_CHECK(utf8_for_each_fn("a\xA0""abcde"_av) == "=a[61]!\xA0[FFFD]=a[61]=b[62]=c[63]=d[64]=e[65]"_av);
    RED_CHECK(utf8_for_each_er("a\xA0""abcde"_av) == "=a[61]!\xA0[FFFD]"_av);
    RED_CHECK(utf8_for_each_fn("abc𡞰b"_av) == "=a[61]=b[62]=c[63]=𡞰[217B0]=b[62]"_av);
    RED_CHECK(utf8_for_each_er("abc𡞰b"_av) == "=a[61]=b[62]=c[63]=𡞰[217B0]=b[62]"_av);

    RED_CHECK(utf8_for_each_fn("abc€"_av) == "=a[61]=b[62]=c[63]=€[20AC]"_av);
    RED_CHECK(utf8_for_each_er("abc€"_av) == "=a[61]=b[62]=c[63]=€[20AC]"_av);
    RED_CHECK(utf8_for_each_fn("abc🚀"_av) == "=a[61]=b[62]=c[63]=🚀[1F680]"_av);
    RED_CHECK(utf8_for_each_er("abc🚀"_av) == "=a[61]=b[62]=c[63]=🚀[1F680]"_av);
    RED_CHECK(utf8_for_each_fn("abc𡞰"_av) == "=a[61]=b[62]=c[63]=𡞰[217B0]"_av);
    RED_CHECK(utf8_for_each_er("abc𡞰"_av) == "=a[61]=b[62]=c[63]=𡞰[217B0]"_av);
    RED_CHECK(utf8_for_each_fn("abc\x80"_av) == "=a[61]=b[62]=c[63]!\x80[FFFD]"_av);
    RED_CHECK(utf8_for_each_er("abc\x80"_av) == "=a[61]=b[62]=c[63]!\x80[FFFD]"_av);
    RED_CHECK(utf8_for_each_fn("abc\xC0"_av) == "=a[61]=b[62]=c[63]?\xC0"_av);
    RED_CHECK(utf8_for_each_er("abc\xC0"_av) == "=a[61]=b[62]=c[63]?\xC0"_av);
    RED_CHECK(utf8_for_each_fn("abc\xF0\xAA"_av) == "=a[61]=b[62]=c[63]?\xF0\xAA"_av);
    RED_CHECK(utf8_for_each_er("abc\xF0\xAA"_av) == "=a[61]=b[62]=c[63]?\xF0\xAA"_av);
    RED_CHECK(utf8_for_each_fn("abc\xA0""abcde"_av) == "=a[61]=b[62]=c[63]!\xA0[FFFD]=a[61]=b[62]=c[63]=d[64]=e[65]"_av);
    RED_CHECK(utf8_for_each_er("abc\xA0""abcde"_av) == "=a[61]=b[62]=c[63]!\xA0[FFFD]"_av);

    RED_CHECK(utf8_for_each_c1("a"_av) == "=a[61]"_av);
    RED_CHECK(utf8_for_each_c1("abc"_av) == "=a[61]=b[62]=c[63]"_av);
    RED_CHECK(utf8_for_each_c1("€"_av) == "=€[20AC]!\x82[FFFD]!\xAC[FFFD]"_av);
}

RED_AUTO_TEST_CASE(Test_utf8_read_on_char)
{
    std::string result;
    auto utf8_read_char = [&](bytes_view s) {
        auto fn = [&result](char c) {
            return [&result, c](auto ch) -> chars_view {
                push_utf8_char(result, c)(ch);
                return result;
            };
        };
        result.clear();
        return utf8_read_one_char(s, []{ return "null"_av; }, fn('='), fn('!'), fn('?'));
    };

    RED_CHECK(utf8_read_char(""_av) == "null"_av);
    RED_CHECK(utf8_read_char("a"_av) == "=a[61]"_av);
    RED_CHECK(utf8_read_char("ab"_av) == "=a[61]"_av);
    RED_CHECK(utf8_read_char("abc"_av) == "=a[61]"_av);
    RED_CHECK(utf8_read_char("🚀"_av) == "=🚀[1F680]"_av);
    RED_CHECK(utf8_read_char("𡞰"_av) == "=𡞰[217B0]"_av);
    RED_CHECK(utf8_read_char("🚀𡞰"_av) == "=🚀[1F680]"_av);
    RED_CHECK(utf8_read_char("𡞰🚀"_av) == "=𡞰[217B0]"_av);
    RED_CHECK(utf8_read_char("\x80"_av) == "!\x80[FFFD]"_av);
    RED_CHECK(utf8_read_char("\x80X"_av) == "!\x80[FFFD]"_av);
    RED_CHECK(utf8_read_char("\xAA"_av) == "!\xAA[FFFD]"_av);
    RED_CHECK(utf8_read_char("\xAAX"_av) == "!\xAA[FFFD]"_av);
    RED_CHECK(utf8_read_char("\xF0\xAA"_av) == "?\xF0\xAA[FFFD]"_av);
    RED_CHECK(utf8_read_char("\xF0\xAAX"_av) == "?\xF0\xAAX[FFFD]"_av);
}

RED_AUTO_TEST_CASE(Test_utf16_to_unicode32)
{
    Utf16ToUnicodeConverter decoder;
    RED_CHECK(decoder.convert('a') == 'a');
    RED_CHECK(decoder.previous_codepoint() == 0);

    RED_CHECK(decoder.convert(0x80) == 0x80);
    RED_CHECK(decoder.previous_codepoint() == 0);

    // 🚀 (rocket)
    RED_CHECK(decoder.convert(0xd83d) == 0);
    RED_CHECK(decoder.previous_codepoint() == 0xd83d);
    RED_CHECK(decoder.convert(0xde80) == 0x1F680);
    RED_CHECK(decoder.previous_codepoint() == 0);

    // invalid (2 surrogage low)
    RED_CHECK(decoder.convert(0xd83d) == 0);
    RED_CHECK(decoder.previous_codepoint() == 0xd83d);
    RED_CHECK(decoder.convert(0xd83d) == 0);
    RED_CHECK(decoder.previous_codepoint() == 0xd83d);
    decoder.clear();

    // invalid (2 surrogage high)
    RED_CHECK(decoder.convert(0xde80) == 0);
    RED_CHECK(decoder.previous_codepoint() == 0);
}

RED_AUTO_TEST_CASE(Test_cp1252_to_utf_8_16_32)
{
    auto _pattern = ut::PatternViewSaver::ascii();
    auto _min_len = ut::AsciiMinLenSaver{0};

    uint8_t dst_buffer[64];
    auto dst_view = make_writable_array_view(dst_buffer);

    StringConvertResult result;

    struct D {
        chars_view input;
        chars_view expected_utf8;
        chars_view expected_utf16;
    };

    for (auto d : {
        D{
            .input = "trap\xe9zo\xef""dal"_av,
            .expected_utf8 = "trapézoïdal"_av,
            .expected_utf16 =
                "t\0r\0""a\0p\0"
                "\xe9\0z\0o\0\xef\0"
                "d\0""a\0l\0"_av,
        },
        D{
            .input = "a ascii sequence !!"_av,
            .expected_utf8 = "a ascii sequence !!"_av,
            .expected_utf16 =
                "a\0 \0a\0s\0c\0i\0i\0 \0s\0e\0q\0u\0e\0n\0c\0e\0 \0!\0!\0"_av,
        },
        D{
            .input = "100 \x80"_av,
            .expected_utf8 = "100 €"_av,
            .expected_utf16 = "1\0""0\0""0\0 \0\xac\x20"_av
        },
        D{
            .input = "100 \x80\ntrap\xe9zo\xef""dal"_av,
            .expected_utf8 = "100 €\ntrapézoïdal"_av,
            .expected_utf16 =
                "1\0""0\0""0\0 \0\xac\x20\n\0"
                "t\0r\0""a\0p\0"
                "\xe9\0z\0o\0\xef\0"
                "d\0""a\0l\0"_av
        },
        D{
            .input = "\x87\x8c\x87"_av,
            .expected_utf8 = "‡Œ‡"_av,
            .expected_utf16 = "\x21\x20""\x52\x01""\x21\x20"_av
        },
    })
    {
        RED_TEST_INFO_SCOPE("input = " << d.input.as<std::string_view>());
        RED_TEST_CONTEXT("cp1252 to utf16")
        {
            RED_CHECK(
                (result = cp1252_to_utf16le.partial(d.input, dst_view)).out
                ==
                d.expected_utf16
            );
            RED_CHECK(result.in == ""_av);
        }
        RED_TEST_CONTEXT("cp1252 to utf8")
        {
            RED_CHECK(
                (result = cp1252_to_utf8.partial(d.input, dst_view)).out
                ==
                ut::utf8(d.expected_utf8)
            );
            RED_CHECK(result.in == ""_av);
        }
    }

    /*
     * Partial utf16
     */

    auto input = "ascii with not ascii\xe9 etc"_av;
    auto expected_utf16 =
        "a\0s\0c\0i\0i\0 \0w\0i\0t\0h\0 \0n\0o\0t\0 "
        "\0a\0s\0c\0i\0i\0\xe9\0 \0e\0t\0c\0"_av;

    for (unsigned i = 0; i <= input.size(); ++i)
    {
        auto input2 = input.drop_front(i);
        auto expected2 = expected_utf16.drop_front(i * 2);
        RED_TEST_INFO_SCOPE("input = " << input2.as<std::string_view>());
        RED_CHECK(
            (result = cp1252_to_utf16le.partial(input2, dst_view)).out
            ==
            expected2
        );
        RED_CHECK(result.in == ""_av);
    }

    {
        RED_TEST_INFO_SCOPE("input = " << input.as<std::string_view>());
        RED_CHECK(
            (result = cp1252_to_utf16le.partial(input, dst_view.first(9))).out
            ==
            expected_utf16.first(8)
        );
        RED_CHECK(result.in == input.drop_front(4));
    }

    {
        auto _pattern = ut::PatternViewSaver::utf8();

        RED_CHECK((result = cp1252_to_utf8.partial("abc"_av, dst_view.first(1))).out == "a"_av);
        RED_CHECK(result.in == "bc"_av);
        RED_CHECK((result = cp1252_to_utf8.partial("abc"_av, dst_view.first(2))).out == "ab"_av);
        RED_CHECK(result.in == "c"_av);
        RED_CHECK((result = cp1252_to_utf8.partial("abc"_av, dst_view.first(3))).out == "abc"_av);
        RED_CHECK(result.in == ""_av);

        RED_CHECK((result = cp1252_to_utf8.partial("\xE9"_av, dst_view.first(1))).out == ""_av);
        RED_CHECK(result.in == "\xE9"_av);
        RED_CHECK((result = cp1252_to_utf8.partial("\xE9"_av, dst_view.first(2))).out == "é"_av);
        RED_CHECK(result.in == ""_av);
        RED_CHECK((result = cp1252_to_utf8.partial("\xE9"_av, dst_view.first(3))).out == "é"_av);
        RED_CHECK(result.in == ""_av);

        RED_CHECK((result = cp1252_to_utf8.partial("\x8c"_av, dst_view.first(1))).out == ""_av);
        RED_CHECK(result.in == "\x8c"_av);
        RED_CHECK((result = cp1252_to_utf8.partial("\x8c"_av, dst_view.first(2))).out == "Œ"_av);
        RED_CHECK(result.in == ""_av);
        RED_CHECK((result = cp1252_to_utf8.partial("\x8c"_av, dst_view.first(3))).out == "Œ"_av);
        RED_CHECK(result.in == ""_av);

        RED_CHECK((result = cp1252_to_utf8.partial("\x87"_av, dst_view.first(2))).out == ""_av);
        RED_CHECK(result.in == "\x87"_av);
        RED_CHECK((result = cp1252_to_utf8.partial("\x87"_av, dst_view.first(2))).out == ""_av);
        RED_CHECK(result.in == "\x87"_av);
        RED_CHECK((result = cp1252_to_utf8.partial("\x87"_av, dst_view.first(3))).out == "‡"_av);
        RED_CHECK(result.in == ""_av);
    }

    /*
     * utf32
     */

    RED_CHECK_HEX16(cp1252_to_utf32(0x80) == 0x20ac);
    RED_CHECK_HEX16(cp1252_to_utf32(0xe9) == 0x00e9);
    RED_CHECK_HEX16(cp1252_to_utf32(0x8c) == 0x0152);
}

RED_AUTO_TEST_CASE(Test_utf16le_to_cp1252)
{
    auto _pattern = ut::PatternViewSaver::ascii();
    auto _min_len = ut::AsciiMinLenSaver{0};

    auto is_not_latin1_valid_code = [](unsigned i) {
        return i == 0x81 || i == 0x8D || i == 0x8F || i == 0x90 || i == 0x9D;
    };

    // check ascii
    for (unsigned i = 0; i <= 0xff; ++i)
    {
        // skip invalid code
        if (i >= 0x80 && i <= 0x9F && !is_not_latin1_valid_code(i))
        {
            continue;
        }

        #if !REDEMPTION_UNIT_TEST_FAST_CHECK
        char str[2] {};
        if (i >= 20 && i < 0x80)
        {
            str[0] = static_cast<char>(i);
        }
        #endif

        auto c = static_cast<uint8_t>(i);
        RED_TEST_CONTEXT("cp1252 = " << c << " '" << str << "'")
        {
            uint8_t input[] { c, 0 };
            uint8_t output[1];
            uint8_t expected[1] { c };
            auto result = utf16le_to_cp1252.partial(
                make_array_view(input),
                make_writable_array_view(output)
            );
            RED_CHECK(result.success);
            RED_CHECK(result.in == ""_av);
            RED_CHECK(result.out == make_array_view(expected));
        }
    }

    // encoding error
    for (unsigned i = 0x80; i <= 0x9F; ++i)
    {
        if (is_not_latin1_valid_code(i))
        {
            continue;
        }

        auto c = static_cast<uint8_t>(i);
        RED_TEST_CONTEXT("encoding error (range 0x80 - 0x9F) = " << c)
        {
            uint8_t input[] { c, 0 };
            uint8_t output[1];
            auto result = utf16le_to_cp1252.partial(
                make_array_view(input),
                make_writable_array_view(output)
            );
            RED_CHECK(!result.success);
            RED_CHECK(result.in == make_array_view(input));
            RED_CHECK(result.out == ""_av);
        }
    }

    struct D
    {
        uint8_t utf16_char[2];
    };

    uint8_t expected = 0x80;

    // cp1252 range in 2 btyes
    for (auto d : {
        // €
        D{{0xac,0x20}},
        D{{0x81,0x00}},
        // ‚
        D{{0x1a,0x20}},
        // ƒ
        D{{0x92,0x01}},
        // „
        D{{0x1e,0x20}},
        // …
        D{{0x26,0x20}},
        // †
        D{{0x20,0x20}},
        // ‡
        D{{0x21,0x20}},
        // ˆ
        D{{0xc6,0x02}},
        // ‰
        D{{0x30,0x20}},
        // Š
        D{{0x60,0x01}},
        // ‹
        D{{0x39,0x20}},
        // Œ
        D{{0x52,0x01}},
        D{{0x8d,0x00}},
        // Ž
        D{{0x7d,0x01}},
        D{{0x8f,0x00}},
        D{{0x90,0x00}},
        // ‘
        D{{0x18,0x20}},
        // ’
        D{{0x19,0x20}},
        // “
        D{{0x1c,0x20}},
        // ”
        D{{0x1d,0x20}},
        // •
        D{{0x22,0x20}},
        // –
        D{{0x13,0x20}},
        // —
        D{{0x14,0x20}},
        // ˜
        D{{0xdc,0x02}},
        // ™
        D{{0x22,0x21}},
        // š
        D{{0x61,0x01}},
        // ›
        D{{0x3a,0x20}},
        // œ
        D{{0x53,0x01}},
        D{{0x9d,0x00}},
        // ž
        D{{0x7e,0x01}},
        // Ÿ
        D{{0x78,0x01}},
    })
    {
        RED_TEST_CONTEXT("utf16 = " << d.utf16_char[0] << " " << d.utf16_char[1])
        {
            uint8_t output[1];
            auto result = utf16le_to_cp1252.partial(
                make_array_view(d.utf16_char),
                make_writable_array_view(output)
            );
            RED_CHECK(result.success);
            RED_CHECK(result.in == ""_av);
            RED_CHECK(result.out == bytes_view(&expected, 1));
        }
        uint8_t buf8[] {
            d.utf16_char[0], d.utf16_char[1],
            d.utf16_char[0], d.utf16_char[1],
            d.utf16_char[0], d.utf16_char[1],
            d.utf16_char[0], d.utf16_char[1],
        };
        RED_TEST_CONTEXT("utf16 = " << d.utf16_char[0] << " " << d.utf16_char[1] << " (4 times)")
        {
            uint8_t output[4];
            uint8_t expected4[4] { expected, expected, expected, expected };
            auto result = utf16le_to_cp1252.partial(
                make_array_view(buf8),
                make_writable_array_view(output)
            );
            RED_CHECK(result.success);
            RED_CHECK(result.in == ""_av);
            RED_CHECK(result.out == make_array_view(expected4));
        }
        ++expected;
    }

    // uncorrespondancy utf16 to cp1252
    for (auto d : {
        D{{0x20,0x02}},
        D{{0x78,0x03}},
    })
    {
        RED_TEST_CONTEXT("utf16 = " << d.utf16_char[0] << ' ' << d.utf16_char[1])
        {
            uint8_t output[1];
            auto result = utf16le_to_cp1252.partial(
                make_array_view(d.utf16_char),
                make_writable_array_view(output)
            );
            RED_CHECK(!result.success);
            RED_CHECK(result.in == make_array_view(d.utf16_char));
            RED_CHECK(result.out == ""_av);
        }
        uint8_t buf8[] {
            d.utf16_char[0], d.utf16_char[1],
            d.utf16_char[0], d.utf16_char[1],
            d.utf16_char[0], d.utf16_char[1],
            d.utf16_char[0], d.utf16_char[1],
        };
        RED_TEST_CONTEXT("utf16 = " << d.utf16_char[0] << ' ' << d.utf16_char[1] << " (4 times)")
        {
            uint8_t output[4];
            auto result = utf16le_to_cp1252.partial(
                make_array_view(buf8),
                make_writable_array_view(output)
            );
            RED_CHECK(!result.success);
            RED_CHECK(result.in == make_array_view(buf8));
            RED_CHECK(result.out == ""_av);
        }
    }

    auto str = bounded_bytes_view{"\xAC\x20 \0a\0 \0t\0e\0s\0t\0 \0\xbd\0!\0"_sized_av};
    RED_TEST_CONTEXT("utf16 = " << str.as_chars().as<std::string_view>())
    {
        auto buffer = utf16le_to_cp1252.buffer_from(str);
        RED_CHECK(buffer.result().success);
        RED_CHECK(buffer.result().out == "\x80 a test \xbd!"_av);
        writable_bounded_array_view<uint8_t, 11, 11> _ = buffer.result().out;
        (void)_;
    }
}

RED_AUTO_TEST_CASE(Test_cp1252_to_utf16le_lf_to_crlf)
{
    auto _pattern = ut::PatternViewSaver::ascii();
    auto _min_len = ut::AsciiMinLenSaver{0};

    constexpr size_t output_buffer_len = 64;

    struct D
    {
        bytes_view in;
        bytes_view expected_out_utf16le;
        size_t buf_len = output_buffer_len;
        bytes_view expected_in_cp1252_result = ""_av;
    };

    for (auto d : {
        D{
            .in = "100 \x80"_av, // 100 €
            .expected_out_utf16le = "1\0""0\0""0\0 \0\xac\x20"_av,
        },
        D{
            .in = "trap\xe9zo\xef""dal"_av,
            .expected_out_utf16le = "t\0r\0a\0p\0\xe9\0z\0o\0\xef\0d\0a\0l\0"_av,
        },
        D{
            .in = "100 \x80\ntrap\xe9zo\xef""dal"_av, // 100 €\n .....
            .expected_out_utf16le =
                "1\0""0\0""0\0 \0\xac\x20""\r\0\n\0"
                "t\0r\0a\0p\0\xe9\0z\0o\0\xef\0d\0a\0l\0"_av,
        },
        D{
            .in = "1\n234567"_av,
            .expected_out_utf16le = "1\0""\r\0\n\0""2\0""3\0""4\0""5\0""6\0""7\0"_av,
        },
        D{
            .in = "123456\n7"_av,
            .expected_out_utf16le = "1\0""2\0""3\0""4\0""5\0""6\0""\r\0\n\0""7\0"_av,
        },
        D{
            .in = "\n\n\n\n\nE"_av,
            .expected_out_utf16le = "\r\0\n\0""\r\0\n\0""\r\0\n\0""\r\0\n\0""\r\0\n\0""E\0"_av,
        },
        D{
            .in = "a"_av,
            .expected_out_utf16le = "a\0"_av,
            .buf_len = 2,
        },
        D{
            .in = "\n"_av,
            .expected_out_utf16le = ""_av,
            .buf_len = 3,
            .expected_in_cp1252_result = "\n"_av,
        },
        D{
            .in = "\n\n\n\n\nE"_av,
            .expected_out_utf16le = "\r\0\n\0""\r\0\n\0""\r\0\n\0"_av,
            .buf_len = 14,
            .expected_in_cp1252_result = "\n\nE"_av,
        },
    })
    {
        RED_TEST_CONTEXT("input = " << d.in.as_chars().as<std::string_view>() << " (cp1252 to utf16le+CrLf) | buf_len = " << d.buf_len)
        {
            uint8_t output[output_buffer_len] {};
            auto result = cp1252_to_utf16le_lf_to_crlf.partial(
                d.in,
                make_writable_array_view(output).first(d.buf_len)
            );
            RED_CHECK(result.in == d.expected_in_cp1252_result);
            RED_CHECK(result.out == d.expected_out_utf16le);
        }
    }
}

RED_AUTO_TEST_CASE(Test_utf8_to_utf16le_lf_to_crlf)
{
    auto _pattern = ut::PatternViewSaver::ascii();
    auto _min_len = ut::AsciiMinLenSaver{0};

    constexpr size_t output_buffer_len = 64;

    struct D
    {
        bytes_view in;
        bytes_view expected_out_utf16le;
        size_t buf_len = output_buffer_len;
        bytes_view expected_in_cp1252_result = ""_av;
    };

    for (auto d : {
        D{
            .in = "100 €"_av, // 100 €
            .expected_out_utf16le = "1\0""0\0""0\0 \0\xac\x20"_av,
        },
        D{
            .in = "trapézoïdal"_av,
            .expected_out_utf16le = "t\0r\0a\0p\0\xe9\0z\0o\0\xef\0d\0a\0l\0"_av,
        },
        D{
            .in = "100 €\ntrapézoïdal"_av, // 100 €\n .....
            .expected_out_utf16le =
                "1\0""0\0""0\0 \0\xac\x20""\r\0\n\0"
                "t\0r\0a\0p\0\xe9\0z\0o\0\xef\0d\0a\0l\0"_av,
        },
        D{
            .in = "1\n234567"_av,
            .expected_out_utf16le = "1\0""\r\0\n\0""2\0""3\0""4\0""5\0""6\0""7\0"_av,
        },
        D{
            .in = "123456\n7"_av,
            .expected_out_utf16le = "1\0""2\0""3\0""4\0""5\0""6\0""\r\0\n\0""7\0"_av,
        },
        D{
            .in = "\n\n\n\n\nE"_av,
            .expected_out_utf16le = "\r\0\n\0""\r\0\n\0""\r\0\n\0""\r\0\n\0""\r\0\n\0""E\0"_av,
        },
        D{
            .in = "a"_av,
            .expected_out_utf16le = "a\0"_av,
            .buf_len = 2,
        },
        D{
            .in = "\n"_av,
            .expected_out_utf16le = ""_av,
            .buf_len = 3,
            .expected_in_cp1252_result = "\n"_av,
        },
        D{
            .in = "\n\n\n\n\nE"_av,
            .expected_out_utf16le = "\r\0\n\0""\r\0\n\0""\r\0\n\0"_av,
            .buf_len = 14,
            .expected_in_cp1252_result = "\n\nE"_av,
        },
        D{
            .in = "abcedef\n@\0"_av,
            .expected_out_utf16le = "a\0b\0c\0e\0d\0e\0f\0\r\0\n\0@\0\0\0"_av,
        },
    })
    {
        RED_TEST_CONTEXT("input = " << ut::utf8(d.in) << " (utf8 to utf16le+CrLf) | buf_len = " << d.buf_len)
        {
            uint8_t output[output_buffer_len] {};
            auto result = utf8_to_utf16le_lf_to_crlf.partial(
                d.in,
                make_writable_array_view(output).first(d.buf_len)
            );
            RED_CHECK(result.in == d.expected_in_cp1252_result);
            RED_CHECK(result.out == d.expected_out_utf16le);
        }
    }
}
