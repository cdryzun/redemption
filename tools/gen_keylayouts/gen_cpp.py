#!/usr/bin/env python3
from typing import Optional, NamedTuple
from kbd_parser import KeymapType, KeyLayout, DeadKey, parse_argv
from collections import OrderedDict
import re
import operator


class Key2(NamedTuple):
    codepoint: int
    is_deadkey: bool
    is_high: bool
    text: str


class Keymap2(NamedTuple):
    mod: str
    keymap: KeymapType
    dkeymap: list[int]  # always 128 elements. 0 = no deadkey
    idx1: int
    idx2: int
    idx3: int


class LayoutInfo(NamedTuple):
    layout: KeyLayout
    keymaps: list[Keymap2]


supported_mods = OrderedDict({
    '': True,
    'VK_SHIFT': True,
    'VK_SHIFT VK_CAPITAL': True,
    'VK_SHIFT VK_CAPITAL VK_NUMLOCK': True,
    'VK_SHIFT VK_CONTROL': True,
    'VK_SHIFT VK_CONTROL VK_MENU VK_CAPITAL': True,
    'VK_SHIFT VK_CONTROL VK_MENU VK_CAPITAL VK_NUMLOCK': True,
    'VK_SHIFT VK_CONTROL VK_MENU VK_NUMLOCK': True,
    'VK_SHIFT VK_CONTROL VK_MENU': True,
    'VK_SHIFT VK_NUMLOCK': True,
    'VK_CAPITAL': True,
    'VK_CAPITAL VK_NUMLOCK': True,
    'VK_CONTROL': True,
    'VK_CONTROL VK_MENU': True,
    'VK_CONTROL VK_MENU VK_NUMLOCK': True,
    'VK_CONTROL VK_MENU VK_CAPITAL': True,
    'VK_CONTROL VK_MENU VK_CAPITAL VK_NUMLOCK': True,
    'VK_NUMLOCK': True,
    # OEM8
    'VK_SHIFT VK_OEM_8': True,
    'VK_OEM_8': True,
    # Kana
    'VK_CONTROL VK_KANA': True,
    'VK_SHIFT VK_CONTROL VK_KANA': True,
    'VK_SHIFT VK_KANA': True,
    'VK_SHIFT VK_KANA VK_NUMLOCK': True,
    'VK_KANA': True,
    'VK_KANA VK_NUMLOCK': True,
})

mods_to_mask = {
    '': 0,
    'VK_SHIFT': 1,
    'VK_CONTROL': 2,
    'VK_MENU': 4,
    'VK_NUMLOCK': 8,
    'VK_CAPITAL': 16,
    'VK_OEM_8': 32,
    'VK_KANA': 64,
}

display_name_map = {
    'ADLaM': 'ff-Adlm',
    'Albanian': 'sq',
    'Arabic (101)': 'ar-SA.101',
    'Arabic (102) AZERTY': 'ar-SA.102-azerty',
    'Arabic (102)': 'ar-SA.102',
    'Armenian Eastern (Legacy)': 'hy.east',
    'Armenian Phonetic': 'hy.phonetic',
    'Armenian Typewriter': 'hy.typewriter',
    'Armenian Western (Legacy)': 'hy.west',
    'Assamese - INSCRIPT': 'as',
    'Azerbaijani (Standard)': 'az-Latn.standard',
    'Azerbaijani Cyrillic': 'az-Cyrl',
    'Azerbaijani Latin': 'az-Latn',
    'Bangla - INSCRIPT (Legacy)': 'bn-IN.legacy',
    'Bangla - INSCRIPT': 'bn-IN.inscript',
    'Bangla': 'bn-IN',
    'Bashkir': 'ba-Cyrl',
    'Belarusian': 'be',
    'Belgian (Comma)': 'fr-BE',
    'Belgian (Period)': 'nl-BE',
    'Belgian French': 'fr-BE.fr',
    'Bosnian (Cyrillic)': 'bs-Cy',
    'Buginese': 'bug-Bugi',
    'Bulgarian (Latin)': 'bg-BG.latin',
    'Bulgarian (Phonetic Traditional)': 'bg.phonetic.trad',
    'Bulgarian (Phonetic)': 'bg.phonetic',
    'Bulgarian (Typewriter)': 'bg.typewriter',
    'Bulgarian': 'bg-BG',
    'Canadian French (Legacy)': 'fr-CA',
    'Canadian French': 'en-CA.fr',
    'Canadian Multilingual Standard': 'en-CA.multilingual',
    'Central Atlas Tamazight': 'tzm-Latn',
    'Central Kurdish': 'ku-Arab',
    'Cherokee Nation': 'chr-Cher',
    'Cherokee Phonetic': 'chr-Cher.phonetic',
    'Chinese (Simplified) - US': 'zh-Hans-CN',
    'Chinese (Simplified, Singapore) - US': 'zh-Hans-SG',
    'Chinese (Traditional) - US': 'zh-Hant-TW',
    'Chinese (Traditional, Hong Kong S.A.R.) - US': 'zh-Hant-HK',
    'Chinese (Traditional, Macao S.A.R.) - US': 'zh-Hant-MO',
    'Colemak': 'en-US.colemak',
    'Czech (QWERTY)': 'cs-CZ.qwerty',
    'Czech Programmers': 'cs-CZ.programmers',
    'Czech': 'cs-CZ',
    'Danish': 'da-DK',
    'Devanagari - INSCRIPT': 'hi.inscript',
    'Divehi Phonetic': 'dv.phonetic',
    'Divehi Typewriter': 'dv.typewriter',
    'Dutch': 'nl-NL',
    'Dzongkha': 'dz',
    'English (India)': 'en-IN',
    'Estonian': 'et-EE',
    'Faeroese': 'fo-FO',
    'Finnish with Sami': 'fi-SE',
    'Finnish': 'fi-FI.finnish',
    'French (Legacy, AZERTY)': 'fr-FR',
    'French (Standard, AZERTY)': 'fr-FR.standard',
    'French (Standard, BÉPO)': 'bépo',
    'Futhark': 'gem-Runr',
    'Georgian (Ergonomic)': 'ka.ergo',
    'Georgian (Legacy)': 'ka.legacy',
    'Georgian (MES)': 'ka.mes',
    'Georgian (Old Alphabets)': 'ka.oldalpha',
    'Georgian (QWERTY)': 'ka.qwerty',
    'German (IBM)': 'de-DE.ibm',
    'German Extended (E1)': 'de-DE.ex1',
    'German Extended (E2)': 'de-DE.ex2',
    'German': 'de-DE',
    'Gothic': 'got-Goth',
    'Greek (220) Latin': 'el-GR.220_latin',
    'Greek (220)': 'el-GR.220',
    'Greek (319) Latin': 'el-GR.319_latin',
    'Greek (319)': 'el-GR.319',
    'Greek Latin': 'el-GR.latin',
    'Greek Polytonic': 'el-GR.polytonic',
    'Greek': 'el-GR',
    'Greenlandic': 'kl',
    'Guarani': 'gn',
    'Gujarati': 'gu',
    'Hausa': 'ha-Latn',
    'Hawaiian': 'haw-Latn',
    'Hebrew (Standard)': 'he.standard',
    'Hebrew (Standard, 2018)': 'he-IL',
    'Hebrew': 'he',
    'Hindi Traditional': 'hi',
    'Hungarian 101-key': 'hu-HU.101-key',
    'Hungarian': 'hu-HU',
    'Icelandic': 'is-IS',
    'Igbo': 'ig-Latn',
    'Inuktitut - Latin': 'iu-La',
    'Inuktitut - Naqittaut': 'iu-Cans',
    'Inuktitut - Nattilik': 'iu-Cans-CA',
    'Irish': 'en-IE.irish',
    'Italian (142)': 'it-IT.142',
    'Italian': 'it-IT',
    'Japanese': 'ja',
    'Javanese': 'jv-Java',
    'Kannada': 'kn',
    'Kazakh': 'kk-KZ',
    'Khmer (NIDA)': 'km.nida',
    'Khmer': 'km',
    'Korean': 'ko',
    'Kyrgyz Cyrillic': 'ky-KG',
    'Lao': 'lo',
    'Latin American': 'es-MX',
    'Latvian (QWERTY)': 'lv-LV.qwerty',
    'Latvian (Standard)': 'lv',
    'Latvian': 'lv-LV',
    'Lisu (Basic)': 'lis-Lisu.basic',
    'Lisu (Standard)': 'lis-Lisu',
    'Lithuanian IBM': 'lt-LT.ibm',
    'Lithuanian Standard': 'lt',
    'Lithuanian': 'lt-LT',
    'Luxembourgish': 'lb-LU',
    'Macedonian - Standard': 'mk',
    'Macedonian': 'mk-MK',
    'Malayalam': 'ml',
    'Maltese 47-Key': 'mt-MT.47',
    'Maltese 48-Key': 'mt-MT.48',
    'Maori': 'mi-NZ',
    'Marathi': 'mr',
    'Mongolian (Mongolian Script)': 'mn-Mong.script',
    'Mongolian Cyrillic': 'mn-MN',
    'Myanmar (Phonetic order)': 'my.phonetic',
    'Myanmar (Visual order)': 'my.visual',
    'NZ Aotearoa': 'en-NZ',
    'Nepali': 'ne-NP',
    'New Tai Lue': 'khb-Talu',
    'Norwegian with Sami': 'se-NO',
    'Norwegian': 'nb-NO',
    'N’Ko': 'nqo',
    'Odia': 'or',
    'Ogham': 'sga-Ogam',
    'Ol Chiki': 'sat-Olck',
    'Old Italic': 'ett-Ital',
    'Osage': 'osa-Osge',
    'Osmanya': 'so-Osma',
    'Pashto (Afghanistan)': 'ps',
    'Persian (Standard)': 'fa.standard',
    'Persian': 'fa',
    'Phags-pa': 'mn-Phag',
    'Polish (214)': 'pl-PL',
    'Polish (Programmers)': 'pl-PL.programmers',
    'Portuguese (Brazil ABNT)': 'pt-BR.abnt',
    'Portuguese (Brazil ABNT2)': 'pt-BR.abnt2',
    'Portuguese': 'pt-PT',
    'Punjabi': 'pa',
    'Romanian (Legacy)': 'ro-RO.legacy',
    'Romanian (Programmers)': 'ro-RO.programmers',
    'Romanian (Standard)': 'ro-RO',
    'Russian (Typewriter)': 'ru-RU.typewriter',
    'Russian - Mnemonic': 'ru',
    'Russian': 'ru-RU',
    'Sakha': 'sah-Cyrl',
    'Sami Extended Finland-Sweden': 'se-SE.ext_finland_sweden',
    'Sami Extended Norway': 'se-NO.ext_norway',
    'Scottish Gaelic': 'en-IE',
    'Serbian (Cyrillic)': 'sr-Cy',
    'Serbian (Latin)': 'sr-La',
    'Sesotho sa Leboa': 'nso',
    'Setswana': 'tn-ZA',
    'Sinhala - Wij 9': 'si.wij9',
    'Sinhala': 'si',
    'Slovak (QWERTY)': 'sk-SK.qwerty',
    'Slovak': 'sk-SK',
    'Slovenian': 'sl-SI',
    'Sora': 'srb-Sora',
    'Sorbian Extended': 'hsb.ex',
    'Sorbian Standard (Legacy)': 'hsb.legacy',
    'Sorbian Standard': 'hsb',
    'Spanish Variation': 'es-ES.variation',
    'Spanish': 'es-ES',
    'Standard': 'hr-HR',  # Croatian
    'Swedish with Sami': 'se-SE',
    'Swedish': 'sv-SE',
    'Swiss French': 'fr-CH',
    'Swiss German': 'de-CH',
    'Syriac Phonetic': 'syr-Syrc.phonetic',
    'Syriac': 'syr-Syrc',
    'Tai Le': 'tdd-Tale',
    'Tajik': 'tg-Cyrl',
    'Tamil 99': 'ta-IN.99',
    'Tamil Anjal': 'ta-IN.anjal',
    'Tamil': 'ta-IN',
    'Tatar (Legacy)': 'tt-RU',
    'Tatar': 'tt-Cyrl',
    'Telugu': 'te',
    'Thai Kedmanee (non-ShiftLock)': 'th.non-shiftlock',
    'Thai Kedmanee': 'th',
    'Thai Pattachote (non-ShiftLock)': 'th.pattachote.non-shiftlock',
    'Thai Pattachote': 'th.pattachote',
    'Tibetan (PRC) - Updated': 'bo-Tibt.updated',
    'Tibetan (PRC)': 'bo-Tibt',
    'Tifinagh (Basic)': 'tzm-Tfng',
    'Tifinagh (Extended)': 'tzm-Tfng.ex',
    'Traditional Mongolian (MNS)': 'mn-Mong-CN',
    'Traditional Mongolian (Standard)': 'mn-Mong',
    'Turkish F': 'tr-TR.f',
    'Turkish Q': 'tr-TR.q',
    'Turkmen': 'tk-Latn',
    'US English Table for IBM Arabic 238_L': 'en-US.arabic.238L',
    'US': 'en-US',
    'Ukrainian (Enhanced)': 'uk',
    'Ukrainian': 'uk-UA',
    'United Kingdom Extended': 'cy-GB',
    'United Kingdom': 'en-GB',
    'United States-Dvorak for left hand': 'en-US.dvorak_left',
    'United States-Dvorak for right hand': 'en-US.dvorak_right',
    'United States-Dvorak': 'en-US.dvorak',
    'United States-International': 'en-US.international',
    'Urdu': 'ur-PK',
    'Uyghur (Legacy)': 'ug-Arab.legacy',
    'Uyghur': 'ug-Arab',
    'Uzbek Cyrillic': 'uz-Cy',
    'Vietnamese': 'vi',
    'Wolof': 'wo-Latn',
    'Yoruba': 'yo-Latn',
}

display_rename_map = {
    'Standard': 'Croatian',
}

supported_mods_mask_to_name = {
    sum(mods_to_mask[m] for m in mod.split(' ')): mod
    for mod in supported_mods
}
supported_mods_name_to_mask = {v: k for k, v in supported_mods_mask_to_name.items()}

mods_with_capslock = [name
                      for mods, name in supported_mods_mask_to_name.items()
                      if mods & 16]

capslock_to_nocapslock_mods = {
    mod: supported_mods_mask_to_name[supported_mods_name_to_mask[mod] & ~16]
    for mod in mods_with_capslock
}


DeadKeysTuple = tuple[..., tuple[str,  # accent
                                 str,  # with
                                 int,  # codepoint
                                 int,  # dkeys index ref (0 for no ref)
                                 ]]


def push_deadkeys(dkeys: dict[tuple[str, str], DeadKey],
                       unique_deadkeys: dict[DeadKeysTuple, int]) -> int:
    deadkeys = sorted(((dk.accent, dk.with_, dk.codepoint,
                        push_deadkeys(dk.deadkeys, unique_deadkeys) if dk.deadkeys else 0)
                       for dk in dkeys.values()),
                      key=lambda t: (ord(t[1]) | (0x8000 if t[3] else 0), t[2]))
    return unique_deadkeys.setdefault((*deadkeys,), len(unique_deadkeys)+1)


def load_layout_infos(layouts: list[KeyLayout],
                      unique_keymap: dict[tuple[..., Key2 | None], int],
                      unique_deadkeys: dict[DeadKeysTuple, int]) -> list[LayoutInfo]:
    layouts2: list[LayoutInfo] = []
    for layout in layouts:
        keymaps = layout.keymaps
        keymap_for_layout = []

        keymaps_mods = [(keymaps[mod], mod) for mod in supported_mods]

        # add capslock when missing
        keymaps_by_mods = {mod: keymap for keymap, mod in keymaps_mods}
        for i in range(128):
            if all(not keymap[i]
                   for keymap, mod in keymaps_mods
                   if mod in mods_with_capslock):
                for keymap, mod in keymaps_mods:
                    if not keymap[i] and mod in mods_with_capslock:
                        newmod = capslock_to_nocapslock_mods[mod]
                        # if newmod in keymaps_by_mods:
                        keymap[i] = keymaps_by_mods[newmod][i]

        for keymap, mod in keymaps_mods:
            keys = []
            keys_high = []
            dkeys = []
            dkmax = 0
            for i,key in enumerate(keymap):
                idk = 0
                if key and (key.codepoint or key.text):
                    if key.deadkeys:
                        idk = push_deadkeys(key.deadkeys, unique_deadkeys)
                        dkmax = i+1
                    low = key.codepoint & 0x3fff
                    high = key.codepoint >> 14
                    if high:
                        if idk:
                            raise Exception('is_deadkey + is_high (the cpp code must be adapted)')
                        keys.append(Key2(codepoint=low,
                                         is_deadkey=bool(idk),
                                         is_high=True,
                                         text=""))
                        keys_high.append(Key2(codepoint=high,
                                              is_deadkey=False,
                                              is_high=False,
                                              text=""))
                    else:
                        keys.append(Key2(codepoint=low,
                                         is_deadkey=bool(idk),
                                         is_high=False,
                                         text=key.text))
                        keys_high.append(None)
                else:
                    keys.append(None)
                    keys_high.append(None)
                dkeys.append(idk)
            idx1 = unique_keymap.setdefault((*keys[:128],), len(unique_keymap))
            idx2 = unique_keymap.setdefault((*keys[128:],), len(unique_keymap))
            idx3 = unique_keymap.setdefault((*keys_high[:128],), len(unique_keymap))
            dkeys = (*dkeys[:dkmax],) or None
            keymap_for_layout.append(Keymap2(mod=mod, keymap=keymap,
                                             idx1=idx1, idx2=idx2, idx3=idx3,
                                             dkeymap=dkeys))

        layouts2.append(LayoutInfo(layout=layout, keymaps=keymap_for_layout))
    return layouts2


layouts: list[KeyLayout] = parse_argv()

used_names = set()

error_messages = []
for layout in layouts:
    # check new layout names
    if layout.origin_display_name not in display_name_map:
        error_messages.append(f"missing layout: '{layout.origin_display_name}': '{layout.locale_name}'")
    else:
        used_names.add(layout.origin_display_name)

    for mod, keymap in layout.keymaps.items():
        if mod in supported_mods:
            # check that codepoint <= 0x3fffffff in keymap[:128]
            if not all(not key or key.codepoint <= 0x3fffffff for key in keymap[:128]):
                error_messages.append(f'{mod or "NoMod"} for {layout.klid}/{layout.locale_name} have a codepoint greater that 0x3fffffff in the range [0-128]')

            # check that codepoint <= 0x3fff in keymap[128:]
            if not all(not key or key.codepoint <= 0x3fff for key in keymap[128:]):
                error_messages.append(f'{mod or "NoMod"} for {layout.klid}/{layout.locale_name} have a codepoint greater that 0x3fff in the range [128-256]')

        # check that unknown mod is empty
        elif not all(key is None for key in keymap):
            error_messages.append(f'{mod or "NoMod"} for {layout.klid}/{layout.locale_name} is not null')

# for names in display_name_map.keys() - used_names:
#     error_messages.append(f'unknown layout: {names}')

if error_messages:
    import sys
    print(f'{len(error_messages)} error(s):\n- ' + '\n- '.join(error_messages),
          file=sys.stderr)
    exit(1)


codepoint_to_char_table = {
    0x7: '\\a',
    0x8: '\\b',
    0x9: '\\t',
    0xa: '\\n',
    0xb: '\\v',
    0xc: '\\f',
    0xd: '\\r',
    0x27: '\\\'',
    0x5C: '\\\\',
}

unique_keymap = {(None,)*128: 0}
unique_deadkeys = {}
layouts2 = load_layout_infos(layouts, unique_keymap, unique_deadkeys)

strings = [
    '#include "keyboard/keylayouts.hpp"\n\n',
    'constexpr auto DK = KeyLayout::DK;\n\n',
    'constexpr auto HI = KeyLayout::HI;\n\n',
    'using KbdId = KeyLayout::KbdId;\n\n',
]

# print keymap (scancodes[128 * 2]
# with DK (mask for deadkey)
# and HI (mask for high unicode value)
for keymap, idx in unique_keymap.items():
    if idx == 0:
        strings.append('static constexpr auto & keymap_0 = detail::null_layout_unicodes;\n\n')
        continue

    strings.append(f'static constexpr KeyLayout::unicode16_t keymap_{idx}[] {{\n')
    for i in range(128//8):
        char_comment = []
        has_char_comment = False
        strings.append(f'/*{i*8:02X}-{i*8+7:02X}*/ ')

        for j in range(i*8, i*8+8):
            if (key := keymap[j]) and (codepoint := key.codepoint):
                if not key.is_deadkey and not key.is_high and (0x20 <= codepoint <= 0x7E or 0x7 <= codepoint <= 0xD):
                    c = codepoint_to_char_table.get(codepoint) or chr(codepoint)
                    c = f"'{c}'"
                    strings.append(f"{c:>9}, ")
                    char_comment.append('           ')
                elif key.is_high:
                    if key.is_deadkey:
                        char_comment.append('   ')
                        strings.append('DK|')
                    c = codepoint_to_char_table.get(codepoint) or chr(codepoint)
                    c = f"HI|0x{codepoint:04x}"
                    strings.append(f'{c:>9}, ')
                    char_comment.append('           ')
                elif key.is_deadkey and 0x20 <= codepoint <= 0x7E:
                    c = codepoint_to_char_table.get(codepoint) or chr(codepoint)
                    c = f"DK|'{c}'"
                    strings.append(f'{c:>9}, ')
                    char_comment.append('           ')
                else:
                    strings.append(f'{"DK|" if key.is_deadkey else "   "}0x{codepoint:04x}, ')
                    if 0x20 <= codepoint <= 0x7E:
                        char_comment.append(f'{chr(codepoint):>10} ')
                        has_char_comment = True
                    elif 0x07 <= codepoint <= 0x0D:
                        char_comment.append(f'\\{"abtnvfr"[codepoint - 0x7]:>10} ')
                        has_char_comment = True
                    elif codepoint == 0x1b:
                        char_comment.append('       ESC ')
                        has_char_comment = True
                    elif codepoint > 127:
                        char_comment.append(f'{key.text:>10} ')
                        has_char_comment = True
                    else:
                        char_comment.append('           ')
            else:
                strings.append('        0, ')
                char_comment.append('           ')

        strings.append('\n')
        if has_char_comment:
            strings.append('       //')
            strings += char_comment
            strings.append('\n')

    strings.append('};\n\n')

# print deadkeys map (only when a keymap has at least 1 deadkey)
for deadkeys, idx in unique_deadkeys.items():
    accent = next(iter(deadkeys))[0]
    strings.append(f'static constexpr KeyLayout::DKeyTable::Data dkeydata_{idx}[] {{\n')
    strings.append(f'    {{.meta={{.size={len(deadkeys)}, .accent=0x{ord(accent):04X} /* {accent} */}}}},\n')

    for accent, with_, codepoint, idk in deadkeys:
        second = ord(with_)
        if second >= 0x8000:
            raise Exception('deadkey grater than or equal 0x8000')
        strings.append(
            (f'    {{.dkey={{.second=DK|0x{second:04X} /* {with_} */,'
             f' .data={{.dkeytable_idx={idk}}},'
             '}},\n')
            if idk else
            (f'    {{.dkey={{.second=0x{second:04X} /* {with_} */,'
             f' .data={{.result=0x{codepoint:04X} /* {chr(codepoint)} */}},'
             '}},\n')
        )
    strings.append('};\n\n')


strings.append('static constexpr KeyLayout::DKeyTable dkeymaps[] {\n    {nullptr},\n')
for i in range((len(unique_deadkeys) + 4) // 5):
    strings.append('   ')
    strings += (f' {{dkeydata_{f"{j+1}}},":<6}'
                for j in range(i*5, min(i*5+5, len(unique_deadkeys))))
    strings.append('\n')
strings.append('};\n\n')


# dkeymap memoization
dktables = {(0,)*128: 0}
for _layout, keymaps in layouts2:
    for _mod, _keymap, dkeymap, _idx1, _idx2, _idx3 in keymaps:
        if dkeymap:
            dktables.setdefault(dkeymap, len(dktables))

# print dkeymap index (DKeyTable[])
for dkeymap, idx in dktables.items():
    if idx == 0:
        strings.append('static constexpr auto dkeymap_idx_0 = nullptr;\n\n')
        continue

    strings.append(f'static constexpr uint16_t dkeymap_idx_{idx}[] {{\n')
    for i in range((len(dkeymap) + 11) // 12):
        strings.append('    ')
        strings += (f'{f"{dkeymap[j]},":<6}'
                    for j in range(i*12, min(i*12+12, len(dkeymap))))
        strings.append('\n')
    strings.append('};\n\n')


def find_scancode(keymap, text: str, default: int) -> int:
    try:
        return next(i for i, k in enumerate(keymap) if k and k.text == text)
    except StopIteration:
        return default


def in_batches_of_32(ulayout, mods, prefix) -> str:
    l = (ulayout.setdefault((*mods[i:i+32],), len(ulayout)) for i in range(0, 128, 32))
    return ', '.join(f'{prefix}{i}' for i in l)


# prepare keymap_mod and dkeymap_mod
unique_layout_keymap = {}
unique_layout_dkeymap = {}
strings2 = ['using Sc = kbdtypes::Scancode;\n\nstatic constexpr KeyLayout layouts[] {\n']
keymap_by_names = []
display_names = set()
for layout in layouts2:
    mods_array1 = [0]*128
    mods_array2 = [0]*128
    mods_array3 = [0]*128
    dmods_array = [0]*128
    sc_R = 0x13
    sc_C = 0x2E
    sc_V = 0x2F
    for mod, keymap, dkeymap, idx1, idx2, idx3 in layout.keymaps:
        mask = supported_mods_name_to_mask[mod]
        mods_array1[mask] = idx1
        mods_array2[mask] = idx2
        mods_array3[mask] = idx3
        if dkeymap:
            dmods_array[mask] = dktables[dkeymap]
        if mask == 0:
            sc_R = find_scancode(keymap, 'r', sc_R)
            sc_C = find_scancode(keymap, 'c', sc_C)
            sc_V = find_scancode(keymap, 'v', sc_V)

    tk1 = in_batches_of_32(unique_layout_keymap, mods_array1, 'keymap_mod_')
    tk2 = in_batches_of_32(unique_layout_keymap, mods_array2, 'keymap_mod_')
    tk3 = in_batches_of_32(unique_layout_keymap, mods_array3, 'keymap_mod_')
    tkd = in_batches_of_32(unique_layout_dkeymap, dmods_array, 'dkeymap_idx_mod_')

    layout = layout.layout
    name = layout.origin_display_name
    display_name = display_name_map[name]

    # display_name must be unique
    if display_name in display_names:
        raise Exception(f"duplicate name: {display_name} ({name} / {layout.locale_name})")
    display_names.add(display_name)

    strings2.append('    KeyLayout{'
        f'KbdId(0x{layout.klid}), '
        f'KeyLayout::RCtrlIsCtrl::{(layout.has_right_ctrl_like_oem8 and "No ") or "Yes"}, '
        f'Sc(0x{sc_R:x}), '
        f'Sc(0x{sc_C:x}), '
        f'Sc(0x{sc_V:x}),\n'
        f'              {{{{{tk1}}},\n'
        f'               {{{tk2}}},\n'
        f'               {{{tk3}}}}},\n'
        f'              {{{{{tkd}}}}},\n'
        f'              dkeymaps,'
        f' "{display_name}"_zv, "{display_rename_map.get(name, name)}"_zv}},\n'
    )
    keymap_by_names.append((display_name.upper(), f'    layouts[{len(keymap_by_names)}],\n'))
strings2.append('};\n')

keymap_by_names.sort(key=operator.itemgetter(0))
strings2.extend((
    '\nstatic constexpr KeyLayout layouts_sorted_by_name[] {\n',
    ''.join(p[1] for p in keymap_by_names),
    '};\n'
))

null_layout = (0,) * 32

# print layout
for unique_layout, name, atype, n in (
    (unique_layout_keymap, 'keymap', 'KeyLayout::HalfKeymap', 7),
    (unique_layout_dkeymap, 'dkeymap_idx', 'uint16_t const *', 5)
):
    for k,idx in unique_layout.items():
        if k == null_layout:
            strings.append(f'static constexpr auto & {name}_mod_{idx} = detail::null_{name}_per_batch_of_32;\n\n')
            continue
        strings.append(f'static constexpr {atype} {name}_mod_{idx}[] {{\n')
        for i in range((32 + n-1) // n):
            strings.append('   ')
            strings += (f"{f' {name}_{k[i]},':<{len(name) + 7}}"
                        for i in range(i*n, min(i*n+n, 32)))
            strings.append('\n')
        strings.append('};\n\n')

# print layout
strings2.append(
    '\narray_view<KeyLayout> keylayouts() noexcept\n'
    '{\n    return layouts;\n}\n\n'
    '\narray_view<KeyLayout> keylayouts_sorted_by_name() noexcept\n'
    '{\n    return layouts_sorted_by_name;\n}\n\n'
    'KeyLayout const* find_layout_by_id(KeyLayout::KbdId id) noexcept\n'
    '{\n    switch (id)\n    {\n'
)
strings2.extend(f'    case KbdId{{0x{layout.klid}}}: return &layouts[{i}];\n'
                for i,layout in enumerate(layouts))
strings2.append(
    '    }\n    return nullptr;\n}\n'
    """
KeyLayout const* find_layout_by_name(chars_view name) noexcept
{
    auto sv_name = name.as<std::string_view>();
    for (auto&& layout : layouts) {
        if (layout.name.to_sv() == sv_name) {
            return &layout;
        }
    }
    return nullptr;
}
""")
output = f"{''.join(strings)}\n{''.join(strings2)}"
output = re.sub(' +\n', '\n', output)
print(output)
