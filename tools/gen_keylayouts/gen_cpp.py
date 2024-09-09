#!/usr/bin/env python3
from typing import Optional, NamedTuple
from kbd_parser import KeymapType, KeyLayout, parse_argv
from collections import OrderedDict
import re

class Key2(NamedTuple):
    codepoint: int
    is_deadkey: int
    text: str

class Keymap2(NamedTuple):
    mod: str
    keymap: KeymapType
    dkeymap: list[int]  # always 128 elements. 0 = no deadkey
    idx: int

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
    'VK_SHIFT VK_OEM_8': True,
    'VK_OEM_8': True,
})

mods_to_mask = {
    '': 0,
    'VK_SHIFT': 1,
    'VK_CONTROL': 2,
    'VK_MENU': 4,
    'VK_NUMLOCK': 8,
    'VK_CAPITAL': 16,
    'VK_OEM_8': 32,
}

display_name_map = {
    'US': 'en-US',
    'United States-International': 'en-US.international',
    'United States-Dvorak': 'en-US.dvorak',
    'United States-Dvorak for left hand': 'en-US.dvorak_left',
    'United States-Dvorak for right hand': 'en-US.dvorak_right',
    'Colemak': 'en-US.colemak',
    'United Kingdom': 'en-GB',
    'Irish': 'en-IE.irish',
    'Scottish Gaelic': 'en-IE',
    'Canadian French': 'en-CA.fr',
    'Canadian Multilingual Standard': 'en-CA.multilingual',
    'French': 'fr-FR',
    'French (Legacy, AZERTY)': 'fr-FR',
    'French (Standard, AZERTY)': 'fr-FR.standard',
    'French (Standard, BÉPO)': 'bépo',
    'Belgian (Comma)': 'fr-BE',
    'Swiss French': 'fr-CH',
    'Belgian French': 'fr-BE.fr',
    'Canadian French (Legacy)': 'fr-CA',
    'Czech': 'cs-CZ',
    'Czech (QWERTY)': 'cs-CZ.qwerty',
    'Czech Programmers': 'cs-CZ.programmers',
    'Danish': 'da-DK',
    'German': 'de-DE',
    'German (IBM)': 'de-DE.ibm',
    'German Extended (E1)': 'de-DE.ex1',
    'German Extended (E2)': 'de-DE.ex2',
    'Swiss German': 'de-CH',
    'Greek': 'el-GR',
    'Greek (220)': 'el-GR.220',
    'Greek (319)': 'el-GR.319',
    'Greek (220) Latin': 'el-GR.220_latin',
    'Greek (319) Latin': 'el-GR.319_latin',
    'Greek Latin': 'el-GR.latin',
    'Greek Polytonic': 'el-GR.polytonic',
    'Spanish': 'es-ES',
    'Spanish Variation': 'es-ES.variation',
    'Finnish': 'fi-FI.finnish',
    'Finnish with Sami': 'fi-SE',
    'Icelandic': 'is-IS',
    'Italian': 'it-IT',
    'Italian (142)': 'it-IT.142',
    'Dutch': 'nl-NL',
    'Belgian (Period)': 'nl-BE',
    'Norwegian': 'nb-NO',
    'Polish (214)': 'pl-PL',
    'Polish (Programmers)': 'pl-PL.programmers',
    'Portuguese': 'pt-PT',
    'Portuguese (Brazil ABNT)': 'pt-BR.abnt',
    'Portuguese (Brazil ABNT2)': 'pt-BR.abnt2',
    'Romanian (Legacy)': 'ro-RO',
    'Russian': 'ru-RU',
    'Russian (Typewriter)': 'ru-RU.typewriter',
    'Standard': 'hr-HR',  # Croatian
    'Slovak': 'sk-SK',
    'Slovak (QWERTY)': 'sk-SK.qwerty',
    'Swedish': 'sv-SE',
    'Turkish Q': 'tr-TR.q',
    'Turkish F': 'tr-TR.f',
    'Ukrainian': 'uk-UA',
    'Slovenian': 'sl-SI',
    'Estonian': 'et-EE',
    'Latvian': 'lv-LV',
    'Latvian (QWERTY)': 'lv-LV.qwerty',
    'Lithuanian': 'lt-LT',
    'Lithuanian IBM': 'lt-LT.ibm',
    'Macedonian': 'mk-MK',
    'Faeroese': 'fo-FO',
    'Maltese 47-Key': 'mt-MT.47',
    'Maltese 48-Key': 'mt-MT.48',
    'Swedish with Sami': 'se-SE',
    'Sami Extended Finland-Sweden': 'se-SE.ext_finland_sweden',
    'Norwegian with Sami': 'se-NO',
    'Sami Extended Norway': 'se-NO.ext_norway',
    'Kazakh': 'kk-KZ',
    'Kyrgyz Cyrillic': 'ky-KG',
    'Tatar (Legacy)': 'tt-RU',
    'Mongolian Cyrillic': 'mn-MN',
    'United Kingdom Extended': 'cy-GB',
    'Luxembourgish': 'lb-LU',
    'Maori': 'mi-NZ',
    'Latin American': 'es-MX',
    'Serbian (Latin)': 'sr-La',
    'Serbian (Cyrillic)': 'sr-Cy',
    'Uzbek Cyrillic': 'uz-Cy',
    'Inuktitut - Latin': 'iu-La',
    'Bosnian (Cyrillic)': 'bs-Cy',
    'Bulgarian': 'bg-BG',
    'Bulgarian (Latin)': 'bg-BG.latin',
    'Hungarian 101-key': 'hu-HU',
}

supported_mods_mask_to_name = {
    sum(mods_to_mask[m] for m in mod.split(' ')): mod
    for mod in supported_mods
}
supported_mods_name_to_mask = {v: k for k, v in supported_mods_mask_to_name.items()}

mods_with_capslock = [name for mods, name in supported_mods_mask_to_name.items() if mods & 16]

capslock_to_nocapslock_mods = {
    mod: supported_mods_mask_to_name[supported_mods_name_to_mask[mod] & ~16]
    for mod in mods_with_capslock
}

def load_layout_infos(layouts: list[KeyLayout],
                      unique_keymap: dict[Optional[tuple], int],
                      unique_deadkeys: dict[tuple, int]) -> list[LayoutInfo]:
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
            dkeys = []
            has_dkidx = False
            for key in keymap:
                idk = 0
                if key and (key.codepoint or key.text):
                    if key.deadkeys:
                        deadkeys = [(dk.accent, dk.with_, dk.codepoint)
                                    for dk in key.deadkeys.values()]
                        deadkeys.sort()
                        idk = unique_deadkeys.setdefault((*deadkeys,), len(unique_deadkeys)+1)
                        has_dkidx = True
                    dkeys.append(idk)
                    keys.append(Key2(codepoint=key.codepoint,
                                     is_deadkey=bool(idk),
                                     text=key.text))
                else:
                    dkeys.append(0)
                    keys.append(None)
            assert (all(i == 0 for i in dkeys[128:]))
            idx = unique_keymap.setdefault((*keys,), len(unique_keymap))
            dkeys = (*dkeys[:128],) if has_dkidx else None
            keymap_for_layout.append(Keymap2(mod=mod, keymap=keymap, idx=idx, dkeymap=dkeys))

        layouts2.append(LayoutInfo(layout=layout, keymaps=keymap_for_layout))
    return layouts2


layouts: list[KeyLayout] = parse_argv()

error_messages = []
for layout in layouts:
    for mod, keymap in layout.keymaps.items():
        if mod in supported_mods:
            # check that codepoint <= 0x7fff
            if not all(not key or key.codepoint <= 0x7fff for key in keymap):
                error_messages.append(f'{mod or "NoMod"} for {layout.klid}/{layout.locale_name} have a codepoint greater that 0x7fff')
            # check that there is no deadkeys of deadkeys
            if not all(not key or key.deadkeys or all(d.deadkeys is None for d in key.deadkeys) for key in keymap):
                error_messages.append(f'{mod or "NoMod"} for {layout.klid}/{layout.locale_name} have a deadkeys of deadkeys')
        # check that unknown mod is empty
        elif not all(key is None for key in keymap):
            error_messages.append(f'{mod or "NoMod"} for {layout.klid}/{layout.locale_name} is not null')
if error_messages:
    raise Exception('\n'.join(error_messages))


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

unique_keymap = {(None,)*256: 0}
unique_deadkeys = {}
layouts2 = load_layout_infos(layouts, unique_keymap, unique_deadkeys)

strings = [
    '#include "keyboard/keylayouts.hpp"\n\n',
    'constexpr auto DK = KeyLayout::DK;\n\n',
    'using KbdId = KeyLayout::KbdId;\n\n',
]

# print keymap (scancodes[256] with DK (mask for deadkey)
for keymap, idx in unique_keymap.items():
    strings.append(f'static constexpr KeyLayout::unicode_t keymap_{idx}[] {{\n')
    for i in range(256//8):
        char_comment = []
        has_char_comment = False
        strings.append(f'/*{i*8:02X}-{i*8+7:02X}*/ ')

        for j in range(i*8, i*8+8):
            if (key := keymap[j]) and (codepoint := key.codepoint):
                if not key.is_deadkey and (0x20 <= codepoint <= 0x7E or 0x7 <= codepoint <= 0xD):
                    c = codepoint_to_char_table.get(codepoint) or chr(codepoint)
                    c = f"'{c}'"
                    strings.append(f"{c:>9}, ")
                    char_comment.append('           ')
                elif key.is_deadkey and 0x20 <= codepoint <= 0x7E:
                    c = codepoint_to_char_table.get(codepoint) or chr(codepoint)
                    c = f"DK|'{c}'"
                    strings.append(f'{c:>9}, ')
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
unique_deadkeys = {v: k for k, v in unique_deadkeys.items()}
for idx,deadkeys in unique_deadkeys.items():
    accent = next(iter(deadkeys))[0]
    strings.append(f'static constexpr KeyLayout::DKeyTable::Data dkeydata_{idx}[] {{\n')
    strings.append(f'    {{.meta={{.size={len(deadkeys)}, .accent=0x{ord(accent):04X} /* {accent} */}}}},\n')
    strings += (f'    {{.dkey={{.second=0x{ord(with_):04X} /* {with_} */, .result=0x{codepoint:04X} /* {chr(codepoint)} */}}}},\n' for accent, with_, codepoint in deadkeys)
    strings.append('};\n\n')

# dkeymap memoization
dktables = {(0,)*128: 0}
for _layout, keymaps in layouts2:
    for _mod, _keymap, dkeymap, _idx in keymaps:
        if dkeymap:
            dktables.setdefault(dkeymap, len(dktables))

# print dkeymap (DKeyTable[])
for deadmap, idx in dktables.items():
    strings.append(f'static constexpr KeyLayout::DKeyTable dkeymap_{idx}[] {{\n')
    for i in range(128//8):
        strings.append('    ')
        strings += (f'{f"{{dkeydata_{deadmap[j]}}},":<16}' if deadmap[j] else '{nullptr},      ' \
                    for j in range(i*8, i*8+8))
        strings.append('\n')
    strings.append('};\n\n')

# prepare keymap_mod and dkeymap_mod
unique_layout_keymap = {}
unique_layout_dkeymap = {}
strings2 = ['static constexpr KeyLayout layouts[] {\n']
keymap_by_names = []
display_names = set()
for layout in layouts2:
    mods_array = [0]*64
    dmods_array = [0]*64
    for mod, _keymap, dkeymap, idx in layout.keymaps:
        mask = supported_mods_name_to_mask[mod]
        mods_array[mask] = idx
        if dkeymap:
            dmods_array[mask] = dktables[dkeymap]
    k1 = (*mods_array,)
    k2 = (*dmods_array,)
    k1 = unique_layout_keymap.setdefault(k1, len(unique_layout_keymap))
    k2 = unique_layout_dkeymap.setdefault(k2, len(unique_layout_dkeymap))
    layout = layout.layout
    display_name = display_name_map.get(layout.origin_display_name, layout.locale_name)

    # display_name must be unique
    if display_name in display_names:
        raise Exception(f"duplicate name: {display_name} ({layout.origin_display_name} / {layout.locale_name})")
    display_names.add(display_name)

    strings2.append(f'    KeyLayout{{KbdId(0x{layout.klid}), KeyLayout::RCtrlIsCtrl({layout.has_right_ctrl_like_oem8 and "false" or "true "}), "{display_name}"_zv/*, "{layout.display_name}"_zv, "{layout.locale_name}"_zv*/, keymap_mod_{k1}, dkeymap_mod_{k2}}},\n')
    keymap_by_names.append((display_name.upper(), f'    layouts[{len(keymap_by_names)}],\n'))
strings2.append('};\n')

keymap_by_names.sort(key=lambda p: p[0])
strings2.extend((
    '\nstatic constexpr KeyLayout layouts_sorted_by_name[] {\n',
    ''.join(p[1] for p in keymap_by_names),
    '};\n'
))

# print layout
for unique_layout,prefix,atype in (
    (unique_layout_keymap, '', 'KeyLayout::unicode_t, 256'),
    (unique_layout_dkeymap, 'd', 'KeyLayout::DKeyTable, 128')
):
    for k,idx in unique_layout.items():
        strings.append(f'static constexpr sized_array_view<{atype}> {prefix}keymap_mod_{idx}[] {{\n')
        for i in range(64//8):
            strings.append('   ')
            strings += (f' {prefix}keymap_{k[i]},' for i in range(i*8, i*8+8))
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
