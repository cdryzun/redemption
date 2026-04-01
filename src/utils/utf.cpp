/*
SPDX-FileCopyrightText: 2026 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "utils/utf.hpp"
#include "utils/mathutils.hpp"
#include "utils/sugar/cast.hpp"
#include "utils/sugar/bytes_copy.hpp"
#include "utils/unicode_case_conversion.hpp"
#include "utils/out_param.hpp"

#include <cassert>
#include <cstring>


using std::size_t;


namespace
{

constexpr int endian_native = __BYTE_ORDER__;
constexpr int endian_big = __ORDER_BIG_ENDIAN__;
constexpr int endian_little = __ORDER_LITTLE_ENDIAN__;

#if defined(__cpp_lib_bitops) && __cpp_lib_bitops >= 201907L
using std::countr_zero;
#else
template<typename T>
constexpr int countr_zero(T x) noexcept
{
    constexpr auto nd = std::numeric_limits<T>::digits;

#if __has_builtin(__builtin_ctzg)
    return __builtin_ctzg(x, nd);
#else
    if (x == 0)
        return nd;

    constexpr auto nd_ull = std::numeric_limits<unsigned long long>::digits;
    constexpr auto nd_ul = std::numeric_limits<unsigned long>::digits;
    constexpr auto nd_u = std::numeric_limits<unsigned>::digits;

    if constexpr (nd <= nd_u)
        return __builtin_ctz(x);
    else if constexpr (nd <= nd_ul)
        return __builtin_ctzl(x);
    else if constexpr (nd <= nd_ull)
        return __builtin_ctzll(x);
    else // (nd > nd_ull)
    {
        static_assert(nd <= (2 * nd_ull),
            "Maximum supported integer size is 128-bit");

        constexpr auto max_ull = std::numeric_limits<unsigned long long>::max;
        unsigned long long low = x & max_ull;
        if (low != 0)
            return __builtin_ctzll(low);
        unsigned long long high = x >> nd_ull;
        return __builtin_ctzll(high) + nd_ull;
    }
#endif
}
#endif

}

// UTF8Len assumes input is valid utf8, zero terminated, that has been checked before
size_t UTF8Len(byte_ptr source) noexcept
{
    size_t len = 0;
    uint8_t c = 0;
    for (size_t i = 0 ; 0 != (c = source[i]) ; i++){
        len += ((c >> 6) == 2)?0:1;
    }
    return len;
}

size_t UTF16ByteLen(bytes_view source) noexcept
{
    uint8_t const* p = source.data();
    uint8_t const* end = p + (source.size() - (source.size() & 1u));

    for (; p != end && (p[0] | p[1]); p += 2) {
    }
    return p - source.data();
}

void UTF16Lower(uint8_t * source, size_t max_len) noexcept
{
    for (size_t i = 0 ; i < max_len ; i += 2){
        unsigned int wc = source[i];
        wc += source[i+1] << 8;

        for (unsigned int  j = 0 ; j < sizeof(uppers)/sizeof(uppers[0]); j++){
            uint16_t c = uppers[j];
            if (wc == c) {
                source[i] = lowers[j] & 0xFF;
                source[i+1] = (lowers[j] >> 8) & 0x00FF;
                break;
            }
        }
    }
}

void UTF16Upper(uint8_t * source, size_t max_len) noexcept
{
    for (size_t i = 0 ; i < max_len ; i += 2){
        unsigned int wc = source[i];
        wc += source[i+1] << 8;

        for (unsigned int  j = 0 ; j < sizeof(lowers)/sizeof(lowers[0]); j++){
            uint16_t c = lowers[j];
            if (wc == c) {
                source[i] = uppers[j] & 0xFF;
                source[i+1] = (uppers[j] >> 8) & 0x00FF;
                break;
            }
        }
    }
}

// UTF8GetLen find the number of bytes of the len first characters of input.
// It assumes input is valid utf8, zero terminated (that has been checked before).
size_t UTF8GetPos(uint8_t const * source, size_t len) noexcept
{
    len += 1;
    uint8_t c = 0;
    size_t i = 0;
    for (; 0 != (c = source[i]) ; i++){
        len -= ((c >> 6) == 2)?0:1;
        if (len == 0) {
            break;
        }
    }
    return i;
}

namespace
{
    constexpr uint8_t utf8_byte_size_table[] {
        // 0xxx x[xxx]
        1, 1, 1, 1,
        1, 1, 1, 1,
        1, 1, 1, 1,
        1, 1, 1, 1,
        // 10xx x[xxx]  invalid value
        2, 2, 2, 2,
        2, 2, 2, 2,
        // 110x x[xxx]
        2, 2, 2, 2,
        // 1110 x[xxx]
        3, 3,
        // 1111 0[xxx]
        4,
        // 1111 1[xxx]  invalid value
        4,
    };
} // anonymous namespace

// UTF8CharNbBytes:
// ----------------
// input: 'source' is the beginning of a char contained in a valid utf8 zero terminated string.
//        (valid means "that has been checked before". It means we are in a secure context).
// output: number of bytes for 'one' char
size_t UTF8CharNbBytes(const uint8_t * source) noexcept
{
    uint8_t c = *source;
    return utf8_byte_size_table[c >> 3];
    // return (c<=0x7F)?1:(c<=0xDF)?2:(c<=0xEF)?3:4;
}

// UTF8Len assumes input is valid utf8, zero terminated, that has been checked before
size_t UTF8StringAdjustedNbBytes(const uint8_t * source, size_t max_len) noexcept
{
    size_t adjust_len = 0;
    while (*source) {
        const size_t char_nb_bytes = UTF8CharNbBytes(source);
        if (adjust_len + char_nb_bytes >= max_len) {
            break;
        }

        adjust_len += char_nb_bytes;
        source += char_nb_bytes;
    }

    return adjust_len;
}

// UTF8Len assumes input is valid utf8, zero terminated, that has been checked before
size_t UTF8StringAdjustedNbBytes(bytes_view source, size_t max_len) noexcept
{
    size_t adjust_len = 0;
    while (adjust_len < source.size()) {
        const size_t char_nb_bytes = UTF8CharNbBytes(&source[adjust_len]);
        if (adjust_len + char_nb_bytes >= max_len) {
            break;
        }

        adjust_len += char_nb_bytes;
    }

    return adjust_len;
}

// UTF8RemoveOne assumes input is valid utf8, zero terminated, that has been checked before
void UTF8RemoveOne(writable_bytes_view source) noexcept
{
    if (source.front()) {
        size_t n = utf8_byte_size_table[(source.front() >> 3)];
        memmove(source.data(), source.data() + n, source.size() - n);
    }
}

// UTF8InsertAtPos assumes input is valid utf8, zero terminated, that has been checked before
// UTF8InsertAtPos won't insert anything and return false if modified string buffer does not have enough space to insert
bool UTF8InsertUtf16(writable_bytes_view source, std::size_t bytes_used, uint16_t unicode_char) noexcept
{
    assert(source.size() >= bytes_used);

    uint8_t utf8[4];
    const auto utf8char = UTF16toUTF8_buf(unicode_char, make_writable_array_view(utf8));

    if (source.size() - bytes_used < utf8char.size()) {
        return false;
    }

    auto* p = source.data();
    memmove(p + utf8char.size(), p, bytes_used);
    memcpy(p, utf8char.data(), utf8char.size());

    return true;
}

size_t UTF8toUTF16(bytes_view source, uint8_t * target, size_t t_len) noexcept
{
    const uint8_t * s = source.as_u8p();

    size_t i_t = 0;
    uint32_t ucode = 0;
    unsigned c = 0;
    for (size_t i = 0; (i < source.size()) && (ucode = c = s[i]) != 0 ; i++){
        switch (c >> 4){
            case 0:
                // allows control characters
                if (c == 0){
                    // should never happen, catched by test above
                    return i_t;
                }

                ucode = c;
            break;
            case 1: case 2: case 3:
            case 4: case 5: case 6: case 7:
            ucode = c;
            break;
            /* handle U+0080..U+07FF inline : 2 bytes sequences */
            case 0xC: case 0xD:
                if (i + 1 > source.size()){
                    return i_t;
                }
                ucode = ((c & 0x1F) << 6)|(s[i+1] & 0x3F);
                i+=1;
            break;
             /* handle U+8FFF..U+FFFF inline : 3 bytes sequences */
            case 0xE:
                if (i + 2 > source.size()){
                    return i_t;
                }
                ucode = ((c & 0x0F) << 12)|((s[i+1] & 0x3F) << 6)|(s[i+2] & 0x3F);
                i+=2;
            break;
            case 0xF:
                if (i + 3 > source.size()){
                    return i_t;
                }
// TODO This is trouble: we may have to use extended UTF16 sequence because the ucode may be more than 16 bits long
                ucode = ((c & 0x07) << 18)|((s[i+1] & 0x3F) << 12)|((s[i+2] & 0x3F) << 6)|(s[i+3] & 0x3F);
                i+=3;
            break;
            case 8: case 9: case 0x0A: case 0x0B:
                // should never happen on valid UTF8
                return i_t;
        }
        if (i_t + 2 > t_len) { return i_t; }
        target[i_t] = ucode & 0xFF;
        target[i_t + 1] = (ucode >> 8) & 0xFF;
        i_t += 2;
    }

    // do not write final 0
    return i_t;
}

size_t UTF8toUTF16(bytes_view source, writable_bytes_view target) noexcept
{
    return UTF8toUTF16(source, target.as_u8p(), target.size());
}


UTF8toUnicodeIterator::UTF8toUnicodeIterator(byte_ptr str) noexcept
: source(str.as_u8p())
{
    ++*this;
}

UTF8toUnicodeIterator & UTF8toUnicodeIterator::operator++() noexcept
{
    auto c = *source;
    this->ucode = c;
    ++source;
    switch (c >> 4) {
        case 0:
        case 1: case 2: case 3:
        case 4: case 5: case 6: case 7:
        break;
        /* handle U+0080..U+07FF inline : 2 bytes sequences */
        case 0xC: case 0xD:
            this->ucode = utf8_2_bytes_to_ucs(c, source[0]);
            source += 1;
        break;
            /* handle U+8FFF..U+FFFF inline : 3 bytes sequences */
        case 0xE:
            this->ucode = utf8_3_bytes_to_ucs(c, source[0], source[1]);
            source += 2;
        break;
        case 0xF:
            this->ucode = utf8_4_bytes_to_ucs(c, source[0], source[1], source[2]);
            source += 3;
        break;
        // these should never happen on valid UTF8
        case 8: case 9: case 0x0A: case 0x0B:
            ucode = 0;
        break;
    }
    return *this;
}


// Return number of UTF8 bytes used to encode UTF16 input
// do not write trailing 0
size_t UTF16toUTF8(const uint8_t * utf16_source, size_t utf16len, uint8_t * utf8_target, size_t target_len) noexcept
{
    size_t i_t = 0;
    size_t i_s = 0;
    for (size_t i = 0 ; i < utf16len ; i++){
        uint8_t lo = utf16_source[i_s];
        uint8_t hi  = utf16_source[i_s+1];
        if (lo == 0 && hi == 0){
            if ((i_t + 1) > target_len) { break; }
            utf8_target[i_t] = 0;
            i_t++;
            break;
        }
        i_s += 2;

        if (hi & 0xF8){
            // 3 bytes
            if ((i_t + 3) > target_len) { break; }
            utf8_target[i_t] = 0xE0 | ((hi >> 4) & 0x0F);
            utf8_target[i_t + 1] = 0x80 | ((hi & 0x0F) << 2) | (lo >> 6);
            utf8_target[i_t + 2] = 0x80 | (lo & 0x3F);
            i_t += 3;
        }
        else if (hi || (lo & 0x80)) {
            // 2 bytes
            if ((i_t + 2) > target_len) { break; }
            utf8_target[i_t] = 0xC0 | ((hi << 2) & 0x1C) | ((lo >> 6) & 3);
            utf8_target[i_t + 1] = 0x80 | (lo & 0x3F);
            i_t += 2;
        }
        else {
            if ((i_t + 1) > target_len) { break; }
            utf8_target[i_t] = lo;
            i_t++;
        }
    }
    return i_t;
}

writable_bytes_view UTF16toUTF8_buf(bytes_view utf16_source, writable_bytes_view utf8_target) noexcept
{
    size_t i_t = 0;
    const auto len = utf16_source.size() - (utf16_source.size() & 1u);
    for (size_t i = 0 ; i < len; i += 2){
        uint8_t lo = utf16_source[i];
        uint8_t hi  = utf16_source[i+1];
        if (lo == 0 && hi == 0){
            break;
        }

        if (hi & 0xF8){
            // 3 bytes
            if ((i_t + 3) > utf8_target.size()) { break; }
            utf8_target[i_t] = 0xE0 | ((hi >> 4) & 0x0F);
            utf8_target[i_t + 1] = 0x80 | ((hi & 0x0F) << 2) | (lo >> 6);
            utf8_target[i_t + 2] = 0x80 | (lo & 0x3F);
            i_t += 3;
        }
        else if (hi || (lo & 0x80)) {
            // 2 bytes
            if ((i_t + 2) > utf8_target.size()) { break; }
            utf8_target[i_t] = 0xC0 | ((hi << 2) & 0x1C) | ((lo >> 6) & 3);
            utf8_target[i_t + 1] = 0x80 | (lo & 0x3F);
            i_t += 2;
        }
        else {
            if ((i_t + 1) > utf8_target.size()) { break; }
            utf8_target[i_t] = lo;
            i_t++;
        }
    }
    return utf8_target.first(i_t);
}

// Return number of UTF8 bytes used to encode UTF16 input
// do not write trailing 0
writable_bytes_view UTF16toUTF8_buf(only_type<uint16_t> utf16_source, writable_bytes_view utf8_target) noexcept
{
    size_t i_t = 0;
    uint8_t lo = utf16_source.value() & 0xff;
    uint8_t hi  = utf16_source.value() >> 8;

    if (hi & 0xF8){
        // 3 bytes
        if (i_t + 3 <= utf8_target.size()) {
            utf8_target[i_t] = 0xE0 | ((hi >> 4) & 0x0F);
            utf8_target[i_t + 1] = 0x80 | ((hi & 0x0F) << 2) | (lo >> 6);
            utf8_target[i_t + 2] = 0x80 | (lo & 0x3F);
            i_t += 3;
        }
    }
    else if (hi || (lo & 0x80)) {
        // 2 bytes
        if (i_t + 2 <= utf8_target.size()) {
            utf8_target[i_t] = 0xC0 | ((hi << 2) & 0x1C) | ((lo >> 6) & 3);
            utf8_target[i_t + 1] = 0x80 | (lo & 0x3F);
            i_t += 2;
        }
    }
    else {
        if (i_t + 1 <= utf8_target.size()) {
            utf8_target[i_t] = lo;
            i_t++;
        }
    }

    return utf8_target.first(i_t);
}

// Return number of UTF8 bytes used to encode UTF16 input
// do not write trailing 0
size_t UTF16toUTF8(const uint16_t * utf16_source, size_t utf16len, uint8_t * utf8_target, size_t target_len) noexcept
{
    size_t i_t = 0;
    for (size_t i = 0 ; i < utf16len ; i++){
        uint8_t lo = utf16_source[i] & 0xff;
        uint8_t hi  = utf16_source[i] >> 8;
        if (lo == 0 && hi == 0){
            if ((i_t + 1) > target_len) { break; }
            utf8_target[i_t] = 0;
            i_t++;
            break;
        }

        if (hi & 0xF8){
            // 3 bytes
            if ((i_t + 3) > target_len) { break; }
            utf8_target[i_t] = 0xE0 | ((hi >> 4) & 0x0F);
            utf8_target[i_t + 1] = 0x80 | ((hi & 0x0F) << 2) | (lo >> 6);
            utf8_target[i_t + 2] = 0x80 | (lo & 0x3F);
            i_t += 3;
        }
        else if (hi || (lo & 0x80)) {
            // 2 bytes
            if ((i_t + 2) > target_len) { break; }
            utf8_target[i_t] = 0xC0 | ((hi << 2) & 0x1C) | ((lo >> 6) & 3);
            utf8_target[i_t + 1] = 0x80 | (lo & 0x3F);
            i_t += 2;
        }
        else {
            if ((i_t + 1) > target_len) { break; }
            utf8_target[i_t] = lo;
            i_t++;
        }
    }
    return i_t;
}

// Return number of UTF8 bytes used to encode UTF32 input
// do not write trailing 0
size_t UTF32toUTF8(uint32_t utf32_char, uint8_t * utf8_target, size_t target_len) noexcept
{
    size_t i_t = 0;

    using u8 = uint8_t;

    // 1 byte (0bbb·bbbb)
    if (utf32_char <= 0x7F) {
        if (i_t + 1 <= target_len) {
            utf8_target[i_t++] = static_cast<u8>(utf32_char);
        }
        return i_t;
    }

    // 2 bytes (110b·bbbb 10bb·bbbb)
    if (utf32_char <= 0x7FF) {
        if (i_t + 2 <= target_len) {
            utf8_target[i_t++] = static_cast<u8>(0b1100'0000u | (utf32_char >> 6));
            utf8_target[i_t++] = static_cast<u8>(0b1000'0000u | (utf32_char & 0b11'1111));
        }
        return i_t;
    }

    // 3 bytes (1110·bbbb 10bb·bbbb 10bb·bbbb)
    if (utf32_char <= 0xFFFF) {
        if (i_t + 3 <= target_len) {
            utf8_target[i_t++] = static_cast<u8>(0b1110'0000u | ( utf32_char >> 12));
            utf8_target[i_t++] = static_cast<u8>(0b1000'0000u | ((utf32_char >> 6) & 0b1'1111));
            utf8_target[i_t++] = static_cast<u8>(0b1000'0000u | ( utf32_char       & 0b11'1111));
        }
        return i_t;
    }

    // 4 bytes (1111·0bbb 10bb·bbbb 10bb·bbbb 10bb·bbbb)
    if (utf32_char <= 0x10FFFF) {
        if (i_t + 4 <= target_len) {
            utf8_target[i_t++] = static_cast<u8>(0b1111'0000u | ( utf32_char >> 18));
            utf8_target[i_t++] = static_cast<u8>(0b1000'0000u | ((utf32_char >> 12) & 0b11'1111));
            utf8_target[i_t++] = static_cast<u8>(0b1000'0000u | ((utf32_char >> 6 ) & 0b11'1111));
            utf8_target[i_t++] = static_cast<u8>(0b1000'0000u | ( utf32_char        & 0b11'1111));
        }
        return i_t;
    }

    if (i_t + 1 <= target_len) {
        utf8_target[i_t++] = utf32_char & 0xff;
    }
    return i_t;
}

bool is_ASCII_string(bytes_view source) noexcept
{
    for (uint8_t c : source) {
        if (c > 0x7F) { return false; }
    }

    return true;
}

bool is_ASCII_string(byte_ptr source) noexcept
{
    auto const* s = source.as_u8p();
    for (; *s; ++s) {
        if (*s > 0x7F) { return false; }
    }

    return true;
}

size_t UTF16toLatin1(const uint8_t * utf16_source_, size_t utf16len, uint8_t * latin1_target, size_t latin1_len) noexcept
{
    utf16len &= ~1;

    auto converter = [](uint16_t src, uint8_t * dst) -> bool {
        if ((src < 0x0080) || ((src > 0x9F) && (src < 0x100))) {
            *dst = src;
            return true;
        }

        // Windows-1252 code page
        static struct UTF16ToLatin1Pair {
            uint16_t utf16;
            uint8_t  latin1;
        } UTF16ToLatin1LUT[] = {
            { 0x0081, 0x81 }, { 0x008D, 0x8D }, { 0x008F, 0x8F }, { 0x0090, 0x90 },
            { 0x009D, 0x9D }, { 0x0152, 0x8C }, { 0x0153, 0x9C }, { 0x0160, 0x8A },
            { 0x0161, 0x9A }, { 0x0178, 0x9F }, { 0x017D, 0x8E }, { 0x017E, 0x9E },
            { 0x0192, 0x83 }, { 0x02C6, 0x88 }, { 0x02DC, 0x98 }, { 0x2013, 0x96 },
            { 0x2014, 0x97 }, { 0x2018, 0x91 }, { 0x2019, 0x92 }, { 0x201A, 0x82 },
            { 0x201C, 0x93 }, { 0x201D, 0x94 }, { 0x201E, 0x84 }, { 0x2020, 0x86 },
            { 0x2021, 0x87 }, { 0x2022, 0x95 }, { 0x2026, 0x85 }, { 0x2030, 0x89 },
            { 0x2039, 0x8B }, { 0x203A, 0x9B }, { 0x20AC, 0x80 }, { 0x2122, 0x99 }
        };

        if (src > UTF16ToLatin1LUT[sizeof(UTF16ToLatin1LUT) / sizeof(UTF16ToLatin1LUT[0]) - 1].utf16) {
            return false;
        }

        for (UTF16ToLatin1Pair const& pair : UTF16ToLatin1LUT) {
            if (pair.utf16 == src) {
                *dst = pair.latin1;
                return true;
            }
            if (pair.utf16 > src) {
                break;
            }
        }

        return false;
    };

    uint8_t  * current_latin1_target = latin1_target;
    for (size_t remaining_utf16len = utf16len / 2, remaining_latin1_len = latin1_len;
         remaining_utf16len && remaining_latin1_len; utf16_source_+=2, remaining_utf16len--) {
        if (converter(utf16_source_[1]*256+utf16_source_[0], current_latin1_target)) {
            current_latin1_target++;
            remaining_latin1_len--;
        }
    }

    return current_latin1_target - latin1_target;
}

size_t Latin1toUTF8(
    const uint8_t * latin1_source, size_t latin1_len,
    uint8_t * utf8_target, size_t utf8_len) noexcept
{
    auto converter = [](uint8_t src, uint8_t *& dst, size_t & remaining_dst_len) -> bool {
        if (src < 0x80) {
            *dst++ = src;
            remaining_dst_len--;
            return true;
        }

        if (remaining_dst_len < 2) {
            return false;
        }

        *dst++ = (src >> 6) | 0xC0;
        *dst++ = (src & 0x3f) | 0x80;
        remaining_dst_len -= 2;
        return true;
    };

    uint8_t * current_utf8_target = utf8_target;
    for (const uint8_t * latin1_source_end = latin1_source + latin1_len
      ; latin1_source != latin1_source_end; ++latin1_source) {
        if (!converter(*latin1_source, current_utf8_target, utf8_len)) {
            break;
        }
    }

    return (current_utf8_target - utf8_target);
}


// CP1252
//@{

namespace
{

// https://fr.wikipedia.org/wiki/Windows-1252#Table_des_caract%C3%A8res

constexpr uint16_t lut_cp1252_to_utf32[] {
    // NUL     SOH     STX     ETX     EOT     ENQ     ACK     BEL
    0x0000, 0x0001, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007,
    //  BS      HT      LF      VT      FF      CR      SO      SI
    0x0008, 0x0009, 0x000a, 0x000b, 0x000c, 0x000d, 0x000e, 0x000f,
    // DLE     DC1     DC2     DC3     DC4     NAK     SYN     ETB
    0x0010, 0x0011, 0x0012, 0x0013, 0x0014, 0x0015, 0x0016, 0x0017,
    // CAN      EM     SUB     ESC      FS      GS      RS      US
    0x0018, 0x0019, 0x001a, 0x001b, 0x001c, 0x001d, 0x001e, 0x001f,
    // space     !       "       #       $       %       &       '
    0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027,
    //   (       )       *       +       ,       -       .       /
    0x0028, 0x0029, 0x002a, 0x002b, 0x002c, 0x002d, 0x002e, 0x002f,
    //   0       1       2       3       4       5       6       7
    0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037,
    //   8       9       :       ;       <       =       >       ?
    0x0038, 0x0039, 0x003a, 0x003b, 0x003c, 0x003d, 0x003e, 0x003f,
    //   @       A       B       C       D       E       F       G
    0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047,
    //   H       I       J       K       L       M       N       O
    0x0048, 0x0049, 0x004a, 0x004b, 0x004c, 0x004d, 0x004e, 0x004f,
    //   P       Q       R       S       T       U       V       W
    0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057,
    //   X       Y       Z       [       \       ]       ^       _
    0x0058, 0x0059, 0x005a, 0x005b, 0x005c, 0x005d, 0x005e, 0x005f,
    //   `       a       b       c       d       e       f       g
    0x0060, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067,
    //   h       i       j       k       l       m       n       o
    0x0068, 0x0069, 0x006a, 0x006b, 0x006c, 0x006d, 0x006e, 0x006f,
    //   p       q       r       s       t       u       v       w
    0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077,
    //   x       y       z       {       |       }       ~     DEL
    0x0078, 0x0079, 0x007a, 0x007b, 0x007c, 0x007d, 0x007e, 0x007f,
    //   €               ‚       ƒ       „       …       †       ‡
    0x20ac, 0x0081, 0x201a, 0x0192, 0x201e, 0x2026, 0x2020, 0x2021,
    //   ˆ       ‰       Š       ‹       Œ               Ž
    0x02c6, 0x2030, 0x0160, 0x2039, 0x0152, 0x008d, 0x017d, 0x008f,
    //           ‘       ’       “       ”       •       –       —
    0x0090, 0x2018, 0x2019, 0x201c, 0x201d, 0x2022, 0x2013, 0x2014,
    //   ˜       ™       š       ›       œ               ž       Ÿ
    0x02dc, 0x2122, 0x0161, 0x203a, 0x0153, 0x009d, 0x017e, 0x0178,
    // NBSP      ¡       ¢       £       ¤       ¥       ¦       §
    0x00a0, 0x00a1, 0x00a2, 0x00a3, 0x00a4, 0x00a5, 0x00a6, 0x00a7,
    //   ¨       ©       ª       «       ¬     SHY       ®       ¯
    0x00a8, 0x00a9, 0x00aa, 0x00ab, 0x00ac, 0x00ad, 0x00ae, 0x00af,
    //   °       ±       ²       ³       ´       µ       ¶       ·
    0x00b0, 0x00b1, 0x00b2, 0x00b3, 0x00b4, 0x00b5, 0x00b6, 0x00b7,
    //   ¸       ¹       º       »       ¼       ½       ¾       ¿
    0x00b8, 0x00b9, 0x00ba, 0x00bb, 0x00bc, 0x00bd, 0x00be, 0x00bf,
    //   À       Á       Â       Ã       Ä       Å       Æ       Ç
    0x00c0, 0x00c1, 0x00c2, 0x00c3, 0x00c4, 0x00c5, 0x00c6, 0x00c7,
    //   È       É       Ê       Ë       Ì       Í       Î       Ï
    0x00c8, 0x00c9, 0x00ca, 0x00cb, 0x00cc, 0x00cd, 0x00ce, 0x00cf,
    //   Ð       Ñ       Ò       Ó       Ô       Õ       Ö       ×
    0x00d0, 0x00d1, 0x00d2, 0x00d3, 0x00d4, 0x00d5, 0x00d6, 0x00d7,
    //   Ø       Ù       Ú       Û       Ü       Ý       Þ       ß
    0x00d8, 0x00d9, 0x00da, 0x00db, 0x00dc, 0x00dd, 0x00de, 0x00df,
    //   à       á       â       ã       ä       å       æ       ç
    0x00e0, 0x00e1, 0x00e2, 0x00e3, 0x00e4, 0x00e5, 0x00e6, 0x00e7,
    //   è       é       ê       ë       ì       í       î       ï
    0x00e8, 0x00e9, 0x00ea, 0x00eb, 0x00ec, 0x00ed, 0x00ee, 0x00ef,
    //   ð       ñ       ò       ó       ô       õ       ö       ÷
    0x00f0, 0x00f1, 0x00f2, 0x00f3, 0x00f4, 0x00f5, 0x00f6, 0x00f7,
    //   ø       ù       ú       û       ü       ý       þ       ÿ
    0x00f8, 0x00f9, 0x00fa, 0x00fb, 0x00fc, 0x00fd, 0x00fe, 0x00ff,
};

constexpr uint8_t lut_cp1252_to_utf16le[] {
    //    NUL        SOH        STX        ETX        EOT        ENQ        ACK        BEL
    0x00,0x00, 0x01,0x00, 0x02,0x00, 0x03,0x00, 0x04,0x00, 0x05,0x00, 0x06,0x00, 0x07,0x00,
    //    BS          HT         LF         VT         FF         CR         SO         SI
    0x08,0x00, 0x09,0x00, 0x0a,0x00, 0x0b,0x00, 0x0c,0x00, 0x0d,0x00, 0x0e,0x00, 0x0f,0x00,
    //    DLE        DC1        DC2        DC3        DC4        NAK        SYN        ETB
    0x10,0x00, 0x11,0x00, 0x12,0x00, 0x13,0x00, 0x14,0x00, 0x15,0x00, 0x16,0x00, 0x17,0x00,
    //    CAN         EM        SUB        ESC         FS         GS         RS         US
    0x18,0x00, 0x19,0x00, 0x1a,0x00, 0x1b,0x00, 0x1c,0x00, 0x1d,0x00, 0x1e,0x00, 0x1f,0x00,
    //   space         !          "          #          $          %          &          '
    0x20,0x00, 0x21,0x00, 0x22,0x00, 0x23,0x00, 0x24,0x00, 0x25,0x00, 0x26,0x00, 0x27,0x00,
    //      (          )          *          +          ,          -          .          /
    0x28,0x00, 0x29,0x00, 0x2a,0x00, 0x2b,0x00, 0x2c,0x00, 0x2d,0x00, 0x2e,0x00, 0x2f,0x00,
    //      0          1          2          3          4          5          6          7
    0x30,0x00, 0x31,0x00, 0x32,0x00, 0x33,0x00, 0x34,0x00, 0x35,0x00, 0x36,0x00, 0x37,0x00,
    //      8          9          :          ;          <          =          >          ?
    0x38,0x00, 0x39,0x00, 0x3a,0x00, 0x3b,0x00, 0x3c,0x00, 0x3d,0x00, 0x3e,0x00, 0x3f,0x00,
    //      @          A          B          C          D          E          F          G
    0x40,0x00, 0x41,0x00, 0x42,0x00, 0x43,0x00, 0x44,0x00, 0x45,0x00, 0x46,0x00, 0x47,0x00,
    //      H          I          J          K          L          M          N          O
    0x48,0x00, 0x49,0x00, 0x4a,0x00, 0x4b,0x00, 0x4c,0x00, 0x4d,0x00, 0x4e,0x00, 0x4f,0x00,
    //      P          Q          R          S          T          U          V          W
    0x50,0x00, 0x51,0x00, 0x52,0x00, 0x53,0x00, 0x54,0x00, 0x55,0x00, 0x56,0x00, 0x57,0x00,
    //      X          Y          Z          [          \          ]          ^          _
    0x58,0x00, 0x59,0x00, 0x5a,0x00, 0x5b,0x00, 0x5c,0x00, 0x5d,0x00, 0x5e,0x00, 0x5f,0x00,
    //      `          a          b          c          d          e          f          g
    0x60,0x00, 0x61,0x00, 0x62,0x00, 0x63,0x00, 0x64,0x00, 0x65,0x00, 0x66,0x00, 0x67,0x00,
    //      h          i          j          k          l          m          n          o
    0x68,0x00, 0x69,0x00, 0x6a,0x00, 0x6b,0x00, 0x6c,0x00, 0x6d,0x00, 0x6e,0x00, 0x6f,0x00,
    //      p          q          r          s          t          u          v          w
    0x70,0x00, 0x71,0x00, 0x72,0x00, 0x73,0x00, 0x74,0x00, 0x75,0x00, 0x76,0x00, 0x77,0x00,
    //      x          y          z          {          |          }          ~        DEL
    0x78,0x00, 0x79,0x00, 0x7a,0x00, 0x7b,0x00, 0x7c,0x00, 0x7d,0x00, 0x7e,0x00, 0x7f,0x00,
    //      €                     ‚          ƒ          „          …          †          ‡
    0xac,0x20, 0x81,0x00, 0x1a,0x20, 0x92,0x01, 0x1e,0x20, 0x26,0x20, 0x20,0x20, 0x21,0x20,
    //      ˆ          ‰          Š          ‹          Œ                     Ž
    0xc6,0x02, 0x30,0x20, 0x60,0x01, 0x39,0x20, 0x52,0x01, 0x8d,0x00, 0x7d,0x01, 0x8f,0x00,
    //                 ‘          ’          “          ”          •          –          —
    0x90,0x00, 0x18,0x20, 0x19,0x20, 0x1c,0x20, 0x1d,0x20, 0x22,0x20, 0x13,0x20, 0x14,0x20,
    //      ˜          ™          š          ›          œ                     ž          Ÿ
    0xdc,0x02, 0x22,0x21, 0x61,0x01, 0x3a,0x20, 0x53,0x01, 0x9d,0x00, 0x7e,0x01, 0x78,0x01,
    //   NBSP          ¡          ¢          £          ¤          ¥          ¦          §
    0xa0,0x00, 0xa1,0x00, 0xa2,0x00, 0xa3,0x00, 0xa4,0x00, 0xa5,0x00, 0xa6,0x00, 0xa7,0x00,
    //      ¨          ©          ª          «          ¬        SHY          ®          ¯
    0xa8,0x00, 0xa9,0x00, 0xaa,0x00, 0xab,0x00, 0xac,0x00, 0xad,0x00, 0xae,0x00, 0xaf,0x00,
    //      °          ±          ²          ³          ´          µ          ¶          ·
    0xb0,0x00, 0xb1,0x00, 0xb2,0x00, 0xb3,0x00, 0xb4,0x00, 0xb5,0x00, 0xb6,0x00, 0xb7,0x00,
    //      ¸          ¹          º          »          ¼          ½            ¾        ¿
    0xb8,0x00, 0xb9,0x00, 0xba,0x00, 0xbb,0x00, 0xbc,0x00, 0xbd,0x00, 0xbe,0x00, 0xbf,0x00,
    //      À          Á          Â          Ã          Ä          Å          Æ          Ç
    0xc0,0x00, 0xc1,0x00, 0xc2,0x00, 0xc3,0x00, 0xc4,0x00, 0xc5,0x00, 0xc6,0x00, 0xc7,0x00,
    //      È          É          Ê          Ë          Ì          Í          Î          Ï
    0xc8,0x00, 0xc9,0x00, 0xca,0x00, 0xcb,0x00, 0xcc,0x00, 0xcd,0x00, 0xce,0x00, 0xcf,0x00,
    //      Ð          Ñ          Ò          Ó          Ô          Õ          Ö          ×
    0xd0,0x00, 0xd1,0x00, 0xd2,0x00, 0xd3,0x00, 0xd4,0x00, 0xd5,0x00, 0xd6,0x00, 0xd7,0x00,
    //      Ø          Ù          Ú          Û          Ü          Ý          Þ          ß
    0xd8,0x00, 0xd9,0x00, 0xda,0x00, 0xdb,0x00, 0xdc,0x00, 0xdd,0x00, 0xde,0x00, 0xdf,0x00,
    //      à          á          â          ã          ä          å          æ          ç
    0xe0,0x00, 0xe1,0x00, 0xe2,0x00, 0xe3,0x00, 0xe4,0x00, 0xe5,0x00, 0xe6,0x00, 0xe7,0x00,
    //      è          é          ê          ë          ì          í          î          ï
    0xe8,0x00, 0xe9,0x00, 0xea,0x00, 0xeb,0x00, 0xec,0x00, 0xed,0x00, 0xee,0x00, 0xef,0x00,
    //      ð          ñ          ò          ó          ô          õ          ö          ÷
    0xf0,0x00, 0xf1,0x00, 0xf2,0x00, 0xf3,0x00, 0xf4,0x00, 0xf5,0x00, 0xf6,0x00, 0xf7,0x00,
    //      ø          ù          ú          û          ü          ý          þ          ÿ
    0xf8,0x00, 0xf9,0x00, 0xfa,0x00, 0xfb,0x00, 0xfc,0x00, 0xfd,0x00, 0xfe,0x00, 0xff,0x00,
};

constexpr uint8_t lut_cp1252_to_utf8_from_x80_to_x9F[] {
    //      €                     ‚          ƒ          „          …          †          ‡
    0x82,0xac, 0xc2,0x81, 0x80,0x9a, 0xc6,0x92, 0x80,0x9e, 0x80,0xa6, 0x80,0xa0, 0x80,0xa1,
    //      ˆ          ‰          Š          ‹          Œ                     Ž
    0xcb,0x86, 0x80,0xb0, 0xc5,0xa0, 0x80,0xb9, 0xc5,0x92, 0xc2,0x8d, 0xc5,0xbd, 0xc2,0x8f,
    //                 ‘          ’          “          ”          •          –          —
    0xc2,0x90, 0x80,0x98, 0x80,0x99, 0x80,0x9c, 0x80,0x9d, 0x80,0xa2, 0x80,0x93, 0x80,0x94,
    //      ˜         ™           š          ›          œ                     ž          Ÿ
    0xcb,0x9c, 0x84,0xa2, 0xc5,0xa1, 0x80,0xba, 0xc5,0x93, 0xc2,0x9d, 0xc5,0xbe, 0xc5,0xb8,
};
// position = code unit - 0x80
constexpr uint32_t is_cp1252_to_utf8_3bytes_mask = 0u
    | (1u <<  0) // €
    | (1u <<  2) // ‚
    | (1u <<  4) // „
    | (1u <<  5) // …
    | (1u <<  6) // †
    | (1u <<  7) // ‡
    | (1u <<  9) // ‰
    | (1u << 11) // ‹
    | (1u << 17) // ‘
    | (1u << 18) // ’
    | (1u << 19) // “
    | (1u << 20) // ”
    | (1u << 21) // •
    | (1u << 22) // –
    | (1u << 23) // —
    | (1u << 25) // ™
    | (1u << 27) // ›
    ;

constexpr uint8_t const lut_utf16le_to_cp1252_from_x80_to_x9F[4][255] {
//  0x00  0x01  0x02  0x03  0x04  0x05  0x06  0x07  0x08  0x09  0x0A  0x0B  0x0C  0x0D  0x0E  0x0F
    { // utfle[1] = 2
// 0x00
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
// 0x10
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
// 0x20
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
// 0x30
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
// 0x40
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
// 0x50
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
// 0x60
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
// 0x70
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
// 0x80
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
// 0x90
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
// 0xA0
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
// 0xB0
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
// 0xC0                                    ˆ
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x88, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
// 0xD0                                                                        ˜
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x98, 0xff, 0xff, 0xff,
// 0xE0
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    },
//  0x00  0x01  0x02  0x03  0x04  0x05  0x06  0x07  0x08  0x09  0x0A  0x0B  0x0C  0x0D  0x0E  0x0F
    { // utfle[1] = 1
// 0x00
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
// 0x10
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
// 0x20
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
// 0x30
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
// 0x40
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
// 0x50            Œ     œ
    0xff, 0xff, 0x8C, 0X9C, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
// 0x60 Š    š
    0x8A, 0x9A, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
// 0x70                                                Ÿ                             Ž     ž
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x9F, 0xff, 0xff, 0xff, 0xff, 0x8E, 0x9E, 0xff,
// 0x80
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
// 0x90            ƒ
    0xff, 0xff, 0x83, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
// 0xA0
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
// 0xB0
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
// 0xC0
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
// 0xD0
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
// 0xE0
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    },
//  0x00  0x01  0x02  0x03  0x04  0x05  0x06  0x07  0x08  0x09  0x0A  0x0B  0x0C  0x0D  0x0E  0x0F
    { // utfle[1] = 0x20
// 0x00
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
// 0x10                  –     —                       ‘     ’     ‚           “     ”     „
    0xff, 0xff, 0xff, 0x96, 0x97, 0xff, 0xff, 0xff, 0x91, 0x92, 0x82, 0xff, 0x93, 0x94, 0x84, 0xff,
// 0x20 †    ‡     •                       …
    0x86, 0x87, 0x95, 0xff, 0xff, 0xff, 0x85, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
// 0x30 ‰                                                    ‹     ›
    0x89, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x8B, 0x9B, 0xff, 0xff, 0xff, 0xff, 0xff,
// 0x40
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
// 0x50
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
// 0x60
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
// 0x70
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
// 0x80
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
// 0x90
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
// 0xA0                                                                        €
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x80, 0xff, 0xff, 0xff,
// 0xB0
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
// 0xC0
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
// 0xD0
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
// 0xE0
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    },
//  0x00  0x01  0x02  0x03  0x04  0x05  0x06  0x07  0x08  0x09  0x0A  0x0B  0x0C  0x0D  0x0E  0x0F
    { // utfle[1] = 0x21
// 0x00
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
// 0x10
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
// 0x20            ™
    0xff, 0xff, 0x99, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
// 0x30
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
// 0x40
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
// 0x50
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
// 0x60
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
// 0x70
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
// 0x80
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
// 0x90
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
// 0xA0
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
// 0xB0
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
// 0xC0
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
// 0xD0
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
// 0xE0
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    },
};


constexpr uint32_t u64_to_first_u32_endian(uint64_t v) noexcept
{
    if constexpr (endian_native == endian_big)
        return (v >> 32);
    else if constexpr (endian_native == endian_little)
        return v & 0xffff'ffff;
}

constexpr uint32_t u64_to_last_u32_endian(uint64_t v) noexcept
{
    if constexpr (endian_native == endian_big)
        return v & 0xffff'ffff;
    else if constexpr (endian_native == endian_little)
        return (v >> 32);
}

constexpr uint16_t u32_to_first_u16_endian(uint32_t v) noexcept
{
    if constexpr (endian_native == endian_big)
        return (v >> 16);
    else if constexpr (endian_native == endian_little)
        return v & 0xffff;
}


template<class UIntT>
constexpr UIntT fill_bytes(uint8_t byte) noexcept
{
    UIntT res = 0;
    for (unsigned i = 0; i < sizeof(UIntT); ++i)
    {
        res <<= 8;
        res |= byte;
    }
    return res;
}

template<class UIntT>
inline constexpr auto msb_filled_bytes_mask = fill_bytes<UIntT>(0x80);

template<class UIntT>
inline constexpr auto lsb_filled_bytes_mask = fill_bytes<UIntT>(0x01);

template<uint8_t c, class UIntT>
UIntT get_has_byte_mask(UIntT input) noexcept
{
    constexpr auto mask = fill_bytes<UIntT>(c);
    return static_cast<UIntT>((input ^ mask) - lsb_filled_bytes_mask<UIntT>);
}

template<class UIntT>
UIntT get_known_not_escapable_mask(UIntT input) noexcept
{
    return static_cast<UIntT>(static_cast<UIntT>(~input) & msb_filled_bytes_mask<UIntT>);
}

template<uint8_t... needle, class UIntT>
UIntT get_escapable_mask(UIntT c) noexcept
{
    auto has = (... | get_has_byte_mask<needle>(c));
    auto known_not_escapable = get_known_not_escapable_mask(c);
    return static_cast<UIntT>(has & known_not_escapable);
}


constexpr uint64_t is_not_ascii_mask_1byte_encoding_64 = 0x8080808080808080;
constexpr uint32_t is_not_ascii_mask_1byte_encoding_32
    = is_not_ascii_mask_1byte_encoding_64 & 0xffffffff;
constexpr uint16_t is_not_ascii_mask_1byte_encoding_16
    = is_not_ascii_mask_1byte_encoding_64 & 0xffff;

constexpr uint64_t is_not_ascii_mask_2bytes_le_encoding_64
    = (endian_native == endian_little)
    ? 0xff80ff80ff80ff80
    : 0x80ff80ff80ff80ff;
constexpr uint32_t is_not_ascii_mask_2bytes_le_encoding_32
    = is_not_ascii_mask_2bytes_le_encoding_64 & 0xffffffff;
constexpr uint16_t is_not_ascii_mask_2bytes_le_encoding_16
    = is_not_ascii_mask_2bytes_le_encoding_64 & 0xffff;

// Na -> N ascii or safe convert from u8
// Nna -> N no ascii
// mNa -> maybe N ascii (potentially not consumed)

struct AsciiToUtf8_impl
{
    template<unsigned n>
    REDEMPTION_ALWAYS_INLINE
    static void copy_and_advance_ascii(uint8_t const * & in, uint8_t * & out) noexcept
    {
        memcpy(out, in, n);
        out += n;
        in += n;
    }
};

struct Cp1252ToUtf8_impl : AsciiToUtf8_impl
{
    template<class UpdateInputEnd>
    [[nodiscard]]
    REDEMPTION_ALWAYS_INLINE
    static bool copy_and_advance_1na(
        uint8_t const * & in,
        uint8_t const * & in_end,
        uint8_t * & out,
        UpdateInputEnd update_input_end
    ) noexcept
    {
        auto c = *in;

        if (c >= 0xA0)
        {
            if (!update_input_end(OutParam{in_end}, out, 2))
            {
                return false;
            }
            *out++ = (c >> 6) | 0xC0;
            *out++ = (c & 0x3f) | 0x80;
        }
        // 0x80-0x9F
        else
        {
            uint32_t pos = c - 0x80;
            bool has_3bytes = is_cp1252_to_utf8_3bytes_mask & (1u << pos);

            if (!update_input_end(OutParam{in_end}, out, 2 + has_3bytes))
            {
                return false;
            }

            if (has_3bytes)
            {
                *out++ = 0xE2;
            }

            memcpy(out, lut_cp1252_to_utf8_from_x80_to_x9F + pos * 2, 2);
            out += 2;
        }

        ++in;

        return true;
    }
};

struct AsciiToUtf16le_impl
{
    template<unsigned n>
    REDEMPTION_ALWAYS_INLINE
    static void copy_and_advance_ascii(uint8_t const * & in, uint8_t * & out) noexcept
    {
        for (unsigned i = 0; i < n; ++i)
        {
            *out++ = *in++;
            *out++ = 0;
        }
    }
};

void copy_and_advance_utf8_2bytes_to_utf16le(uint8_t const * & in, uint8_t * & out) noexcept
{
    auto a = *in++;
    auto b = *in++;
    *out++ = static_cast<uint8_t>((a << 6) | (b & 0x3F));
    *out++ = static_cast<uint8_t>((a >> 2) & 0x7);
}

void copy_and_advance_utf8_3bytes_to_utf16le(uint8_t const * & in, uint8_t * & out) noexcept
{
    auto a = *in++;
    auto b = *in++;
    auto c = *in++;
    *out++ = static_cast<uint8_t>((b << 6) | (c & 0x3F));
    *out++ = static_cast<uint8_t>((a << 4) | ((b >> 2) & 0x0F));
}

void copy_and_advance_utf8_4bytes_to_utf16le(uint8_t const * & in, uint8_t * & out) noexcept
{
    auto a = *in++;
    auto b = *in++;
    auto c = *in++;
    auto d = *in++;
    a -= 0x1;
    *out++ = static_cast<uint8_t>((c << 6) | (d & 0x3F));
    *out++ = static_cast<uint8_t>(0xDC | ((c >> 2) & 0x2)); // low surrogate
    *out++ = static_cast<uint8_t>((b << 2) | ((c >> 4) & 0x3));
    *out++ = static_cast<uint8_t>((a & 0x2) | 0xD8);  // high surrogate
}

template<bool CheckInput>
struct Utf8ToUtf16_impl : AsciiToUtf16le_impl
{
    template<class UpdateInputEnd>
    [[nodiscard]]
    REDEMPTION_ALWAYS_INLINE
    static bool copy_and_advance_1na(
        uint8_t const * & in,
        uint8_t const * & in_end,
        uint8_t * & out,
        UpdateInputEnd update_input_end
    ) noexcept
    {
        auto c = *in;

        assert(c >= 0x80);

        auto masked_len = c >> 4;

        // handle U+0080..U+07FF : 2 bytes sequences to 1 utf16
        if (masked_len == 0xC || masked_len == 0xD) [[likely]]
        {
            if constexpr (CheckInput)
            {
                if (in_end - in < 2)
                {
                    return false;
                }
            }

            copy_and_advance_utf8_2bytes_to_utf16le(in, out);
        }
        // handle U+8FFF..U+FFFF : 3 bytes sequences to 1 utf16
        else if (masked_len == 0xE) [[likely]]
        {
            if constexpr (CheckInput)
            {
                if (in_end - in < 3)
                {
                    return false;
                }
            }

            copy_and_advance_utf8_3bytes_to_utf16le(in, out);
        }
        // handle U+10000..U+10FFFF : 4 bytes sequences to 2 utf16
        else if (masked_len == 0xF)
        {
            if constexpr (CheckInput)
            {
                if (in_end - in < 4)
                {
                    return false;
                }
            }

            if (!update_input_end(OutParam{in_end}, out, 4))
            {
                return false;
            }

            copy_and_advance_utf8_4bytes_to_utf16le(in, out);
        }
        // invalide code (0b1000____ - 0b1011____)
        else [[unlikely]]
        {
            if (!update_input_end(OutParam{in_end}, out, 2))
            {
                return false;
            }

            // replacement char (U+FFFD)
            *out++ = 0xFD;
            *out++ = 0xFF;
            ++in;
        }

        return true;
    }
};

using Utf8ToUtf16_impl_check_input = Utf8ToUtf16_impl<true>;
using Utf8ToUtf16_impl_uncheck_input = Utf8ToUtf16_impl<false>;

struct UpdateInputWhenMultiCodeUnit
{
    std::size_t out_remaining;
    std::size_t code_unit_len;
    uint8_t const * out_end;

    REDEMPTION_ALWAYS_INLINE
    bool operator()(
        OutParam<uint8_t const *> in_end,
        uint8_t const * out_p,
        unsigned nb_bytes
    ) noexcept
    {
        assert(nb_bytes >= code_unit_len);

        if (out_remaining >= nb_bytes - code_unit_len)
        {
            out_remaining -= nb_bytes - code_unit_len;
        }
        // input size is too large for output
        else
        {
            // buffer too short, stop encoder
            if (out_end - out_p < nb_bytes)
            {
                return false;
            }

            // skip the last char of the input
            --in_end.out_value;
        }

        return true;
    }
};

struct NotUpdateInputWhenMultiCodeUnit
{
    REDEMPTION_ALWAYS_INLINE
    bool operator()(
        OutParam<uint8_t const *> /*in_end*/,
        uint8_t const * /*out_p*/,
        unsigned /*nb_bytes*/ = 0
    ) noexcept
    {
        return true;
    }
};


[[nodiscard]]
REDEMPTION_ALWAYS_INLINE
uint8_t * write_cp1252_to_utf16le(uint8_t c, uint8_t * out) noexcept
{
    memcpy(out, lut_cp1252_to_utf16le + c * 2, 2);
    return out + 2;
}

[[nodiscard]]
REDEMPTION_ALWAYS_INLINE
uint8_t * write_utf16le_crlf(uint8_t * out) noexcept
{
    *out++ = '\r';
    *out++ = 0;
    *out++ = '\n';
    *out++ = 0;
    return out;
}


struct InOutEncodingResult
{
    uint8_t const * in;
    uint8_t * out;
};

enum class LoopBlock : uint8_t
{
    No,
    Yes,
    AsciiOnly,
};

struct EncodeToMultiCodeUnitOptions
{
    LoopBlock loop_8_bytes_and_more;
    LoopBlock loop_4_bytes_and_more;
    LoopBlock loop_2_bytes_and_more;
    bool loop_1_byte_and_more;
    bool loop_0_or_1_byte;
    chars_view lf_to_crlf_av;

    constexpr EncodeToMultiCodeUnitOptions lf_to_crlf(chars_view crlf_av) noexcept
    {
        auto ret = *this;
        ret.lf_to_crlf_av = crlf_av;
        return ret;
    }

    constexpr static EncodeToMultiCodeUnitOptions loop_8_4_2_1() noexcept
    {
        return {
            .loop_8_bytes_and_more = LoopBlock::Yes,
            .loop_4_bytes_and_more = LoopBlock::Yes,
            .loop_2_bytes_and_more = LoopBlock::Yes,
            .loop_1_byte_and_more = true,
            .loop_0_or_1_byte = true,
            .lf_to_crlf_av = {},
        };
    }

    constexpr static EncodeToMultiCodeUnitOptions loop_8_4() noexcept
    {
        return {
            .loop_8_bytes_and_more = LoopBlock::Yes,
            .loop_4_bytes_and_more = LoopBlock::Yes,
            .loop_2_bytes_and_more = LoopBlock::No,
            .loop_1_byte_and_more = false,
            .loop_0_or_1_byte = false,
            .lf_to_crlf_av = {},
        };
    }

    constexpr static EncodeToMultiCodeUnitOptions loop_2_ascii_1() noexcept
    {
        return {
            .loop_8_bytes_and_more = LoopBlock::No,
            .loop_4_bytes_and_more = LoopBlock::No,
            .loop_2_bytes_and_more = LoopBlock::AsciiOnly,
            .loop_1_byte_and_more = true,
            .loop_0_or_1_byte = false,
            .lf_to_crlf_av = {},
        };
    }
};

/// \return \c true when new line is found.
template<class Encoder, class Input>
REDEMPTION_ALWAYS_INLINE
bool consume_ascii_until_lf(uint8_t const * & in, uint8_t * & out, Input input) noexcept
{
    auto new_line_mask = get_escapable_mask<'\n'>(input);
    if (!new_line_mask)
    {
        Encoder::template copy_and_advance_ascii<sizeof(input)>(in, out);
        return false;
    }

    auto pos = static_cast<unsigned>(countr_zero(new_line_mask)) / 8u;
    REDEMPTION_ASSUME(pos < sizeof(input));

    #define CASE(case_n, input_len) case case_n:                  \
        if constexpr (sizeof(input) > input_len)                  \
        {                                                         \
            Encoder::template copy_and_advance_ascii<1>(in, out); \
        }                                                         \
        [[fallthrough]]

    switch (pos)
    {
        CASE(7, 4);
        CASE(6, 4);
        CASE(5, 4);
        CASE(4, 4);
        CASE(3, 2);
        CASE(2, 2);
        CASE(1, 1);
        case 0: ;
    }

    #undef CASE

    return true;
}

template<class Encoder, auto make_options, class UpdateInputEnd>
REDEMPTION_ALWAYS_INLINE
InOutEncodingResult
encode_to_multi_code_unit_with_1_code_unit_for_ascii(
    bytes_view in,
    uint8_t * out,
    UpdateInputEnd update_input_end
) noexcept
{
    auto * in_data = in.data();
    auto * in_data_end = in.end();

    constexpr EncodeToMultiCodeUnitOptions options = make_options();
    constexpr bool lf_to_crlf = !options.lf_to_crlf_av.empty();

    auto consume_1_ascii_byte = [&](auto input) noexcept {
        if constexpr (!lf_to_crlf)
        {
            Encoder::template copy_and_advance_ascii<sizeof(input)>(in_data, out);
        }
        else
        {
            if (input != '\n')
            {
                Encoder::template copy_and_advance_ascii<1>(in_data, out);
                return true;
            }

            if (!update_input_end(OutParam{in_data_end}, out, options.lf_to_crlf_av.size()))
            {
                return false;
            }

            out = bytes_copy(out, options.lf_to_crlf_av);
            ++in_data;
        }

        return true;
    };

    uint64_t c8;
    uint32_t c4;
    uint16_t c2;

    #define RETURN_IF(expr) do { if (expr) { return {in_data, out}; } } while (0)

    // do not use do{}while(0) pattern because not work with `continue` here...
    #define CONSUME_N_ASCII_BYTES(input)                                           \
        if constexpr (!lf_to_crlf)                                                 \
        {                                                                          \
            Encoder::template copy_and_advance_ascii<sizeof(input)>(in_data, out); \
        }                                                                          \
        else                                                                       \
        {                                                                          \
            if (consume_ascii_until_lf<Encoder>(in_data, out, input))              \
            {                                                                      \
                RETURN_IF(!update_input_end(                                       \
                    OutParam{in_data_end}, out, options.lf_to_crlf_av.size()       \
                ));                                                                \
                out = bytes_copy(out, options.lf_to_crlf_av);                      \
                ++in_data;                                                         \
                continue;                                                          \
            }                                                                      \
        }                                                                          \
        void()

    // 8 bytes or more
    if constexpr (options.loop_8_bytes_and_more != LoopBlock::No)
    {
        while (in_data_end - in_data >= 8)
        {
            memcpy(&c8, in_data, 8);

            if (!(c8 & is_not_ascii_mask_1byte_encoding_64))
            {
                CONSUME_N_ASCII_BYTES(c8);
                continue;
            }

            if constexpr (options.loop_8_bytes_and_more == LoopBlock::AsciiOnly)
            {
                break;
            }
            else
            {
                c4 = u64_to_first_u32_endian(c8);

                if (!(c4 & is_not_ascii_mask_1byte_encoding_32))
                {
                    CONSUME_N_ASCII_BYTES(c4);
                    c4 = u64_to_last_u32_endian(c8);
                }

                c2 = u32_to_first_u16_endian(c4);

                if (!(c2 & is_not_ascii_mask_1byte_encoding_16))
                {
                    CONSUME_N_ASCII_BYTES(c2);
                }

                if (*in_data < 0x80)
                {
                    RETURN_IF(!consume_1_ascii_byte(*in_data));
                }

                RETURN_IF(
                    !Encoder::copy_and_advance_1na(in_data, in_data_end, out, update_input_end)
                );
            }
        }
    }

    // 4 to 7 bytes
    if constexpr (options.loop_4_bytes_and_more != LoopBlock::No)
    {
        while (in_data_end - in_data >= 4)
        {
            memcpy(&c4, in_data, 4);

            if (!(c4 & is_not_ascii_mask_1byte_encoding_32))
            {
                CONSUME_N_ASCII_BYTES(c4);
                break;
            }

            if constexpr (options.loop_4_bytes_and_more == LoopBlock::AsciiOnly)
            {
                break;
            }
            else
            {
                c2 = u32_to_first_u16_endian(c4);

                if (!(c2 & is_not_ascii_mask_1byte_encoding_16))
                {
                    CONSUME_N_ASCII_BYTES(c2);
                }

                if (*in_data < 0x80)
                {
                    RETURN_IF(!consume_1_ascii_byte(*in_data));
                }

                RETURN_IF(
                    !Encoder::copy_and_advance_1na(in_data, in_data_end, out, update_input_end)
                );
            }
        }
    }

    // 2 or 3 bytes
    if constexpr (options.loop_2_bytes_and_more != LoopBlock::No)
    {
        while (in_data_end - in_data >= 2)
        {
            memcpy(&c2, in_data, 2);

            if (!(c2 & is_not_ascii_mask_1byte_encoding_16))
            {
                CONSUME_N_ASCII_BYTES(c2);
                break;
            }

            if constexpr (options.loop_2_bytes_and_more == LoopBlock::AsciiOnly)
            {
                break;
            }
            else
            {
                if (*in_data < 0x80)
                {
                    RETURN_IF(!consume_1_ascii_byte(*in_data));
                }

                RETURN_IF(
                    !Encoder::copy_and_advance_1na(in_data, in_data_end, out, update_input_end)
                );
            }
        }
    }

    // 0 or 1 byte
    if constexpr (options.loop_1_byte_and_more || options.loop_0_or_1_byte)
    {
        while (in_data < in_data_end)
        {
            if (*in_data < 0x80)
            {
                RETURN_IF(!consume_1_ascii_byte(*in_data));
            }
            else
            {
                RETURN_IF(
                    !Encoder::copy_and_advance_1na(in_data, in_data_end, out, update_input_end)
                );
            }

            if constexpr (options.loop_0_or_1_byte)
            {
                break;
            }
        }
    }

    #undef RETURN_IF
    #undef CONSUME_N_ASCII_BYTES

    return {in_data, out};
}

} // anonymous namespace


uint8_t *
Cp1252ToUtf8Base::unchecked(bytes_view in, uint8_t * out) noexcept
{
    return encode_to_multi_code_unit_with_1_code_unit_for_ascii<
        Cp1252ToUtf8_impl,
        [] { return EncodeToMultiCodeUnitOptions::loop_8_4_2_1(); }
    >(
        in,
        out,
        NotUpdateInputWhenMultiCodeUnit{}
    ).out;
}

StringConvertResult
Cp1252ToUtf8Base::partial(bytes_view in, writable_bytes_view out) noexcept
{
    auto min_len = mmin(in.size(), out.size());
    auto out_remaining = out.size() - min_len;
    auto [last_in, last_out] = encode_to_multi_code_unit_with_1_code_unit_for_ascii<
        Cp1252ToUtf8_impl,
        [] { return EncodeToMultiCodeUnitOptions::loop_8_4_2_1(); }
    >(
        in.first(min_len),
        out.data(),
        UpdateInputWhenMultiCodeUnit {
            .out_remaining = out_remaining,
            .code_unit_len = min_output_buffer_multiplicator,
            .out_end = out.end(),
        }
    );
    return {out.before(last_out), in.from_offset(last_in)};
}


uint8_t *
Cp1252ToUtf16LEBase::unchecked(bytes_view in, uint8_t * out) noexcept
{
    for (auto c : in)
    {
        out = write_cp1252_to_utf16le(c, out);
    }
    return out;
}


StringConvertResult
Cp1252ToUtf16LEBase::partial(bytes_view in, writable_bytes_view out) noexcept
{
    auto cp_len = mmin(in.size(), out.size() / 2);
    auto last_out = unchecked(in.first(cp_len), out.data());
    return {out.before(last_out), in.from_offset(cp_len)};
}


namespace
{

template<class UpdateInputEnd>
REDEMPTION_ALWAYS_INLINE
InOutEncodingResult cp1252_to_utf16le_lf_to_crlf_impl(
    bytes_view in,
    uint8_t * out,
    UpdateInputEnd update_input_end
) noexcept
{
    auto * in_data = in.begin();
    auto * in_data_end = in.end();

    auto write_1c_and_advance = [&]() noexcept
    {
        out = write_cp1252_to_utf16le(*in_data, out);
        ++in_data;
    };

    auto write_crlf_and_advance = [&]() noexcept
    {
        if (!update_input_end(OutParam{in_data_end}, out))
        {
            return false;
        }
        out = write_utf16le_crlf(out);
        ++in_data;
        return true;
    };

    auto consume_n_bytes = [&](auto c) noexcept
    {
        memcpy(&c, in_data, sizeof(c));

        auto escapable_mask = get_escapable_mask<'\n'>(c);

        // no new line
        if (!escapable_mask)
        {
            for (std::size_t i = 0; i < sizeof(c); ++i)
            {
                write_1c_and_advance();
            }
            return true;
        }

        static_assert(endian_native == endian_little);

        auto pos = static_cast<unsigned>(countr_zero(escapable_mask)) / 8u;
        REDEMPTION_ASSUME(pos < sizeof(c));

        switch (pos)
        {
            case 7: if constexpr (sizeof(c) > 4) write_1c_and_advance(); [[fallthrough]];
            case 6: if constexpr (sizeof(c) > 4) write_1c_and_advance(); [[fallthrough]];
            case 5: if constexpr (sizeof(c) > 4) write_1c_and_advance(); [[fallthrough]];
            case 4: if constexpr (sizeof(c) > 4) write_1c_and_advance(); [[fallthrough]];
            case 3: write_1c_and_advance(); [[fallthrough]];
            case 2: write_1c_and_advance(); [[fallthrough]];
            case 1: write_1c_and_advance();
        }
        return write_crlf_and_advance();
    };

    // 8 bytes or more
    while (in_data_end - in_data >= 8)
    {
        if (!consume_n_bytes(uint64_t{}))
        {
            return {in_data, out};
        }
    }

    // between 4 and 7 bytes
    while (in_data_end - in_data >= 4)
    {
        if (!consume_n_bytes(uint32_t{}))
        {
            return {in_data, out};
        }
    }

    // 3 bytes or more
    while (in_data < in_data_end)
    {
        if (*in_data != '\n')
        {
            write_1c_and_advance();
        }
        else
        {
            if (!write_crlf_and_advance())
            {
                return {in_data, out};
            }
        }
    }

    return {in_data, out};
}

} // anonymous namespace


uint8_t *
Cp1252ToUtf16LE_LfToCrLfBase::unchecked(bytes_view in, uint8_t * out) noexcept
{
    return cp1252_to_utf16le_lf_to_crlf_impl(in, out, NotUpdateInputWhenMultiCodeUnit{}).out;
}

StringConvertResult
Cp1252ToUtf16LE_LfToCrLfBase::partial(bytes_view in, writable_bytes_view out) noexcept
{
    auto min_len = mmin(in.size(), out.size() / min_output_buffer_multiplicator);
    auto out_remaining = out.size() / min_output_buffer_multiplicator - min_len;
    auto [last_in, last_out] = cp1252_to_utf16le_lf_to_crlf_impl(
        in.first(min_len),
        out.data(),
        [
            update = UpdateInputWhenMultiCodeUnit {
                .out_remaining = out_remaining * min_output_buffer_multiplicator,
                .code_unit_len = min_output_buffer_multiplicator,
                .out_end = out.end(),
            }
        ](OutParam<uint8_t const *> in_end, uint8_t const * out_p) mutable noexcept {
            return update(in_end, out_p, /*crlf_len=*/4);
        }
    );
    return {out.before(last_out), in.from_offset(last_in)};
}


namespace
{
    constexpr auto utf16le_crlf = "\r\0\n\0"_av;
}

uint8_t *
Utf8ToUtf16LE_LfToCrLfBase::unchecked(bytes_view in, uint8_t * out) noexcept
{
    return encode_to_multi_code_unit_with_1_code_unit_for_ascii<
        Utf8ToUtf16_impl_uncheck_input,
        []{ return EncodeToMultiCodeUnitOptions::loop_8_4_2_1().lf_to_crlf(utf16le_crlf); }
    >(
        in,
        out,
        NotUpdateInputWhenMultiCodeUnit{}
    ).out;
}

StringConvertResult
Utf8ToUtf16LE_LfToCrLfBase::partial(bytes_view in, writable_bytes_view out) noexcept
{
    uint8_t * out_p = out.data();
    auto out_len = out.size();

    // encode with unchecked buffer
    if (in.size() > 3)
    {
        auto min_len = mmin(in.size() - 3u, out.size() / max_output_buffer_multiplicator);

        auto partial_encoded_result = encode_to_multi_code_unit_with_1_code_unit_for_ascii<
            Utf8ToUtf16_impl_uncheck_input,
            []{ return EncodeToMultiCodeUnitOptions::loop_8_4().lf_to_crlf(utf16le_crlf); }
        >(
            in.first(min_len),
            out_p,
            NotUpdateInputWhenMultiCodeUnit{}
        );

        in = in.from_offset(partial_encoded_result.in);
        out_len -= static_cast<std::size_t>(partial_encoded_result.out - out_p);
        out_p = partial_encoded_result.out;
    }

    auto min_len = mmin(in.size(), out_len);
    auto out_remaining = out_len - min_len;

    auto [last_in, last_out] = encode_to_multi_code_unit_with_1_code_unit_for_ascii<
        Utf8ToUtf16_impl_check_input,
        []{ return EncodeToMultiCodeUnitOptions::loop_2_ascii_1().lf_to_crlf(utf16le_crlf); }
    >(
        in.first(min_len),
        out_p,
        UpdateInputWhenMultiCodeUnit {
            .out_remaining = out_remaining,
            .code_unit_len = 1,
            .out_end = out.end(),
        }
    );

    return {out.before(last_out), in.from_offset(last_in)};
}


uint32_t * Cp1252ToUtf32::unchecked(bytes_view in, uint32_t * out) noexcept
{
    for (auto c : in)
    {
        *out++ = lut_cp1252_to_utf32[c];
    }
    return out;
}

StringConvertU32Result
Cp1252ToUtf32::partial(bytes_view in, writable_array_view<uint32_t> out) noexcept
{
    auto cp_len = mmin(in.size(), out.size());
    auto last_out = unchecked(in.first(cp_len), out.data());
    return {out.before(last_out), in.from_offset(cp_len)};
}

uint16_t Cp1252ToUtf32::operator()(uint8_t c) const noexcept
{
    return lut_cp1252_to_utf32[c];
}


namespace
{

REDEMPTION_ALWAYS_INLINE
void copy_and_advance_4utf16le_to_a(uint8_t const * & in, uint8_t * & out) noexcept
{
    *out++ = in[0];
    *out++ = in[2];
    *out++ = in[4];
    *out++ = in[6];
    in += 8;
}

REDEMPTION_ALWAYS_INLINE
void copy_and_advance_2utf16le_to_a(uint8_t const * & in, uint8_t * & out) noexcept
{
    *out++ = in[0];
    *out++ = in[2];
    in += 4;
}

REDEMPTION_ALWAYS_INLINE
void copy_and_advance_1utf16le_to_a(uint8_t const * & in, uint8_t * & out) noexcept
{
    *out++ = in[0];
    in += 2;
}

[[nodiscard]]
REDEMPTION_ALWAYS_INLINE
bool copy_and_advance_utf16le_1na_to_cp1252(uint8_t const * & in, uint8_t * & out) noexcept
{
    auto c0 = in[0];
    auto c1 = in[1];

    assert(c1 || c0 >= 0x80);

    constexpr uint32_t compatible_from_x80_to_x9F = 0u
        | (1u << (0x81 - 0x80))
        | (1u << (0x8d - 0x80))
        | (1u << (0x8f - 0x80))
        | (1u << (0x90 - 0x80))
        | (1u << (0x9d - 0x80))
        ;

    // is 1 byte
    if (!c1
        // is latin1
      && (c0 > 0x9F
        // is 1 byte utf16 compatible
       || ((1u << (c0 - 0x80)) & compatible_from_x80_to_x9F)))
    {
        copy_and_advance_1utf16le_to_a(in, out);
        return true;
    }

    // convert to CP1252 in 0x80-0x9F range

    constexpr uint64_t at_1_position = 0ull
        | (1ull << 0x01)
        | (1ull << 0x02)
        | (1ull << 0x20)
        | (1ull << 0x21)
        ;

    auto at1_ok = (1ull << c1) & at_1_position;
    if (at1_ok)
    {
        auto itab = (c1 >> 4) | (c1 & 1); // shrink to 0-4
        auto code = lut_utf16le_to_cp1252_from_x80_to_x9F[itab][c0];
        if (code != 0xff)
        {
            *out++ = code;
            in += 2;
            return true;
        }
    }

    return false;
}

} // anonymous namespace


PartialResultWithSuccess
Utf16LEToCp1252::unchecked(bytes_view in, uint8_t* out) noexcept
{
    auto * in_data = in.data();
    auto * in_data_end = in.end();

    uint64_t c8;
    uint32_t c4;
    uint16_t c2;

    // 8 bytes or more
    while (in_data_end - in_data >= 8)
    {
        memcpy(&c8, in_data, 8);

        if (!(c8 & is_not_ascii_mask_2bytes_le_encoding_64))
        {
            copy_and_advance_4utf16le_to_a(in_data, out);
            continue;
        }

        c4 = u64_to_first_u32_endian(c8);

        if (!(c4 & is_not_ascii_mask_2bytes_le_encoding_32))
        {
            copy_and_advance_2utf16le_to_a(in_data, out);
        }

        c2 = u32_to_first_u16_endian(c4);

        if (!(c2 & is_not_ascii_mask_2bytes_le_encoding_16))
        {
            copy_and_advance_1utf16le_to_a(in_data, out);
        }

        if (!copy_and_advance_utf16le_1na_to_cp1252(in_data, out))
        {
            return {out, false};
        }
    }

    // 4 or 6 bytes
    if (in_data_end - in_data >= 4)
    {
        memcpy(&c4, in_data, 4);

        if (!(c4 & is_not_ascii_mask_2bytes_le_encoding_32))
        {
            copy_and_advance_2utf16le_to_a(in_data, out);
        }
    }

    // 2, 4 or 6 bytes
    while (in_data_end - in_data >= 2)
    {
        memcpy(&c2, in_data, 2);

        if (!(c2 & is_not_ascii_mask_2bytes_le_encoding_16))
        {
            copy_and_advance_1utf16le_to_a(in_data, out);
        }
        else if (!copy_and_advance_utf16le_1na_to_cp1252(in_data, out))
        {
            return {out, false};
        }
    }

    return {out, true};
}

StringConvertResultWithSuccess
Utf16LEToCp1252::partial(bytes_view in, writable_bytes_view out) noexcept
{
    auto cp_len = mmin(in.size() / 2, out.size()) * 2;
    auto result = unchecked(in.first(cp_len), out.data());
    auto consumed = checked_int{ (result.out - out.data()) * 2 };
    return {out.before(result.out), in.from_offset(consumed), result.success};
}

//@}
