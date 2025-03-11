/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "test_only/test_framework/redemption_unit_tests.hpp"

#include "utils/sugar/static_string_from_enum_flags.hpp"
#include "utils/enum_flags.hpp"

enum class EFlags : uint8_t {};
REDEMPTION_DECLARE_ENUM_FLAGS(EFlags)

RED_AUTO_TEST_CASE(TestSplitter)
{
    using Impl = StaticStringFromEnumFlags::Impl<EFlags>;
    Impl::Item params[] {
        {EFlags{0b0000'0001}, "AAA"_av},
        {EFlags{0b0000'0010}, "BBBBB"_av},
        {EFlags{0b0000'0100}, "CC"_av},
        {EFlags{0b0000'1000}, "DD"_av},
        {EFlags{0b0001'0000}, "EEE"_av},
        {EFlags{0b0010'0000}, "F"_av},
        {EFlags{0b0100'0000}, "GGGGGGG"_av},
        {EFlags{0b1000'0000}, "HHHH"_av},
    };

    RED_CHECK(Impl::compute_max_capacity(params) == 34);
    RED_CHECK(Impl::compute_max_capacity(array_view{params}.first(5)) == 23);
    RED_CHECK(Impl::compute_max_capacity(array_view{params}.first(0)) == 3);

    RED_CHECK(
        (StaticStringFromEnumFlags::make<
            "AAAA"_name_of(uint8_t{0b0000'0001}),
            "BBBB"_name_of(uint8_t{0b0000'0010}),
            "CCCC"_name_of(uint8_t{0b0000'0100}),
            "DDDD"_name_of(uint8_t{0b0000'1000}),
            "EEEE"_name_of(uint8_t{0b0001'0000}),
            "FFFF"_name_of(uint8_t{0b0010'0000}),
            "GGGG"_name_of(uint8_t{0b0100'0000}),
            "HHHH"_name_of(uint8_t{0b1000'0000})
        >(uint8_t{0b1111'1111}))
        == "AAAA|BBBB|CCCC|DDDD|EEEE|FFFF|GGGG|HHHH"_av
    );

    RED_CHECK(
        (StaticStringFromEnumFlags::make<
            "AA"_name_of(uint8_t{0b0000'0001}),
            "BB"_name_of(uint8_t{0b0000'0010}),
            "CC"_name_of(uint8_t{0b0000'0100}),
            "DD"_name_of(uint8_t{0b0000'1000}),
            "EE"_name_of(uint8_t{0b0001'0000}),
            "FF"_name_of(uint8_t{0b0010'0000}),
            "GG"_name_of(uint8_t{0b0100'0000})
        >(uint8_t{0b1111'1111}))
        == "AA|BB|CC|DD|EE|FF|GG|???"_av
    );

    RED_CHECK(
        (StaticStringFromEnumFlags::make<
            "AAA"_name_of(uint8_t{0b0000'0001}),
            "BBBBB"_name_of(uint8_t{0b0000'0010}),
            "CC"_name_of(uint8_t{0b0000'0100}),
            "DD"_name_of(uint8_t{0b0000'1000}),
            "EEE"_name_of(uint8_t{0b0001'0000}),
            "F"_name_of(uint8_t{0b0010'0000}),
            "GGGGGGG"_name_of(uint8_t{0b0100'0000}),
            "HHHH"_name_of(uint8_t{0b1000'0000})
        >(uint8_t{0b0110'0110}))
        == "BBBBB|CC|F|GGGGGGG"_av
    );

    RED_CHECK(
        (StaticStringFromEnumFlags::make<
            "AAA"_name_of(EFlags{0b0000'0001}),
            "BBBBB"_name_of(EFlags{0b0000'0010}),
            "CC"_name_of(EFlags{0b0000'0100}),
            "DD"_name_of(EFlags{0b0000'1000}),
            "EEE"_name_of(EFlags{0b0001'0000})
        >(EFlags{0b0110'0110}))
        == "BBBBB|CC|???"_av
    );

    static_assert(std::is_same_v<
        static_string<39>,
        decltype(StaticStringFromEnumFlags::make<
            "AAAA"_name_of(uint8_t{0b0000'0001}),
            "BBBB"_name_of(uint8_t{0b0000'0010}),
            "CCCC"_name_of(uint8_t{0b0000'0100}),
            "DDDD"_name_of(uint8_t{0b0000'1000}),
            "EEEE"_name_of(uint8_t{0b0001'0000}),
            "FFFF"_name_of(uint8_t{0b0010'0000}),
            "GGGG"_name_of(uint8_t{0b0100'0000}),
            "HHHH"_name_of(uint8_t{0b1000'0000})
        >(uint8_t{0b0110'0110}))
    >);

    static_assert(std::is_same_v<
        static_string<23>,
        decltype(StaticStringFromEnumFlags::make<
            "AAA"_name_of(EFlags{0b0000'0001}),
            "BBBBB"_name_of(EFlags{0b0000'0010}),
            "CC"_name_of(EFlags{0b0000'0100}),
            "DD"_name_of(EFlags{0b0000'1000}),
            "EEE"_name_of(EFlags{0b0001'0000})
        >(EFlags{0b0110'0110}))
    >);
}
