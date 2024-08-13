/*
    This program is free software; you can redistribute it and/or modify it
     under the terms of the GNU General Public License as published by the
     Free Software Foundation; either version 2 of the License, or (at your
     option) any later version.

    This program is distributed in the hope that it will be useful, but
     WITHOUT ANY WARRANTY; without even the implied warranty of
     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
     Public License for more details.

    You should have received a copy of the GNU General Public License along
     with this program; if not, write to the Free Software Foundation, Inc.,
     675 Mass Ave, Cambridge, MA 02139, USA.

    Product name: redemption, a FLOSS RDP proxy
    Copyright (C) Wallix 2014
    Author(s): Christophe Grosjean, Raphael Zhou, Jonathan Poelen
*/

#include "test_only/test_framework/redemption_unit_tests.hpp"

#include "capture/utils/pattutils.hpp"
#include "utils/strutils.hpp"

RED_AUTO_TEST_CASE(TestPattern)
{
    auto parse_capture_pattern = [](chars_view patt) {
        CapturePattern const cap_patt = ::parse_capture_pattern(patt);
        return str_concat(
            "CapturePattern{.type='"_av,
            ( cap_patt.is_kbd() && cap_patt.is_ocr() ? "kbd|ocr"_av
            : cap_patt.is_kbd() ? "kbd"_av
            : cap_patt.is_ocr() ? "ocr"_av
            : "?"_av),
            ","_av,
            ( cap_patt.pattern_type() == CapturePattern::PatternType::exact_reg ? "exact_reg"_av
            : cap_patt.pattern_type() == CapturePattern::PatternType::exact_str ? "exact_str"_av
            : cap_patt.pattern_type() == CapturePattern::PatternType::reg ? "reg"_av
            : cap_patt.pattern_type() == CapturePattern::PatternType::str ? "str"_av
            : "?"_av),
            "', .patt='"_av, cap_patt.pattern().as<std::string_view>(), "'}"_av
        );
    };

    RED_CHECK(parse_capture_pattern(""_av)
        == "CapturePattern{.type='ocr,reg', .patt=''}"_av);

    // left spaces are trimmed when no prefix
    RED_CHECK(parse_capture_pattern(" "_av)
        == "CapturePattern{.type='ocr,reg', .patt=''}"_av);

    RED_CHECK(parse_capture_pattern("AT"_av)
        == "CapturePattern{.type='ocr,reg', .patt='AT'}"_av);

    RED_CHECK(parse_capture_pattern("$kbd:gpedit"_av)
        == "CapturePattern{.type='kbd,reg', .patt='gpedit'}"_av);

    // left spaces are trimmed
    RED_CHECK(parse_capture_pattern(" $kbd:gpedit"_av)
        == "CapturePattern{.type='kbd,reg', .patt='gpedit'}"_av);

    RED_CHECK(parse_capture_pattern("$kbd: gpedit"_av)
        == "CapturePattern{.type='kbd,reg', .patt=' gpedit'}"_av);

    RED_CHECK(parse_capture_pattern("$ocr:Bloc-notes"_av)
        == "CapturePattern{.type='ocr,reg', .patt='Bloc-notes'}"_av);

    RED_CHECK(parse_capture_pattern("$content,ocr:cmd"_av)
        == "CapturePattern{.type='ocr,str', .patt='cmd'}"_av);

    RED_CHECK(parse_capture_pattern("$ocr-kbd:cmd"_av)
        == "CapturePattern{.type='kbd|ocr,reg', .patt='cmd'}"_av);

    RED_CHECK(parse_capture_pattern("$exact:cmd"_av)
        == "CapturePattern{.type='ocr,exact_str', .patt='cmd'}"_av);

    // invalid "ocm" rule
    RED_CHECK(parse_capture_pattern("$ocm:10.10.46.0/24:3389"_av)
        == "CapturePattern{.type='?,reg', .patt=''}"_av);

    RED_CHECK(parse_capture_pattern("$content,kbd-ocr:cmd"_av)
        == "CapturePattern{.type='kbd|ocr,str', .patt='cmd'}"_av);

    RED_CHECK(parse_capture_pattern("$exact-content,kbd-ocr:cmd"_av)
        == "CapturePattern{.type='kbd|ocr,exact_str', .patt='cmd'}"_av);

    RED_CHECK(parse_capture_pattern("$exact-regex,kbd-ocr:cmd"_av)
        == "CapturePattern{.type='kbd|ocr,exact_reg', .patt='cmd'}"_av);
}

RED_AUTO_TEST_CASE(TestKbdPattern)
{
    RED_CHECK(!contains_kbd_pattern(""_av));
    RED_CHECK(!contains_kbd_pattern("AT"_av));
    RED_CHECK(!contains_kbd_pattern("Bloc-notes"_av));
    RED_CHECK(contains_kbd_pattern("$kbd:gpedit"_av));
    RED_CHECK(contains_kbd_pattern(" $kbd:gpedit\x01" "AT"_av));
    RED_CHECK(contains_kbd_pattern(" $kbd:kill\x01 " "AT "_av));
    RED_CHECK(contains_kbd_pattern("AT\x01$kbd:kill"_av));
    RED_CHECK(!contains_kbd_pattern("$ocr:Bloc-notes"_av));
    RED_CHECK(contains_kbd_pattern("$ocr-kbd:cmd"_av));
    RED_CHECK(contains_kbd_pattern("$kbd-ocr:cmd"_av));
    RED_CHECK(contains_kbd_pattern("$exact-content,kbd-ocr:cmd"_av));
    RED_CHECK(!contains_kbd_pattern("$content,ocr:cmd"_av));
}

RED_AUTO_TEST_CASE(TestOcrPattern)
{
    RED_CHECK(!contains_ocr_pattern(""_av));
    RED_CHECK(contains_ocr_pattern("AT"_av));
    RED_CHECK(contains_ocr_pattern("Bloc-notes"_av));
    RED_CHECK(contains_ocr_pattern("$ocr:Bloc-notes"_av));
    RED_CHECK(contains_ocr_pattern("$ocr:Bloc-notes\x01" "AT"_av));
    RED_CHECK(contains_ocr_pattern("$kbd:kill\x01" " AT"_av));
    RED_CHECK(contains_ocr_pattern(" AT\x01$kbd:kill"_av));
    RED_CHECK(!contains_ocr_pattern("$kbd:kill"_av));
    RED_CHECK(contains_ocr_pattern("$ocr-kbd:cmd"_av));
    RED_CHECK(contains_ocr_pattern("$kbd-ocr:cmd"_av));
    RED_CHECK(contains_ocr_pattern("$exact-regex,kbd-ocr:cmd"_av));
    RED_CHECK(contains_ocr_pattern("$content,ocr:cmd"_av));
}

RED_AUTO_TEST_CASE(TestKbdOrOcrPattern)
{
    RED_CHECK(!contains_kbd_or_ocr_pattern(""_av));
    RED_CHECK(contains_kbd_or_ocr_pattern("AT"_av));
    RED_CHECK(contains_kbd_or_ocr_pattern("Bloc-notes"_av));
    RED_CHECK(contains_kbd_or_ocr_pattern("$kbd:gpedit"_av));
    RED_CHECK(contains_kbd_or_ocr_pattern(" $kbd:gpedit\x01" "AT"_av));
    RED_CHECK(contains_kbd_or_ocr_pattern(" $kbd:kill\x01 " "AT "_av));
    RED_CHECK(contains_kbd_or_ocr_pattern("AT\x01$kbd:kill"_av));
    RED_CHECK(contains_kbd_or_ocr_pattern("$ocr:Bloc-notes"_av));
    RED_CHECK(contains_kbd_or_ocr_pattern("$ocr-kbd:cmd"_av));
    RED_CHECK(contains_kbd_or_ocr_pattern("$kbd-ocr:cmd"_av));
    // invalid "ocm" rule
    RED_CHECK(!contains_kbd_or_ocr_pattern("$ocm:10.10.46.0/24:3389"_av));
    RED_CHECK(contains_kbd_or_ocr_pattern("$content,kbd-ocr:cmd"_av));
    RED_CHECK(contains_kbd_or_ocr_pattern("$content,ocr:cmd"_av));
}
