#!/usr/bin/env python3
# equivalent to
#   runecho xgettext --c++ --sort-by-file \
#       --keyword=TR_KV:2 \
#       --keyword=TR_KV_FMT:2 \
#       --keyword=TR_KV_PLURAL_FMT:2,3 \
#       --output="$pot_file" "$trkeys_file"

import re
import sys
import time
from datetime import datetime
from zoneinfo import ZoneInfo

input_filename = sys.argv[1]
output_filename = '/dev/stdout' if '-' == sys.argv[2] else sys.argv[2]

with open(input_filename) as f:
    contents = f.read()

desintent_patt = re.compile(r'\n\s+')


def msgid_fmt(s: str) -> str:
    s = desintent_patt.sub('\n', s.strip())
    return s if '\n' not in s else '""\n' + s


nline = 1
last_comment = ''

prefix = 'tests/includes/fixtures/img_ref/mod/internal/'

extract_mod = re.compile(r'ModuleName::(\w+)|\w+ (error)\b|(\bOSD\b|\bosd\b)')
close_box = f'# image(s): {prefix}close_mod\n# image(s): {prefix}widget/wab_close'
paths_mapping = {
    'close': close_box,
    'error': close_box,
    'local_err': close_box,
    'autotest': close_box,
    'selector': f'# image(s): {prefix}widget/selector',
    'valid': f'# image(s): {prefix}widget/dialog',
    'link_confirm': f'# image(s): {prefix}widget/dialog',
    'challenge': f'# image(s): {prefix}widget/dialog',
    'login': f'# image(s): {prefix}widget/login',
    'interactive_target': f'# image(s): {prefix}widget/interactive_target',
    'osd': f'# image(s): {prefix}acl/',
    'OSD': f'# image(s): {prefix}acl/',
}

now = datetime.now(tz=ZoneInfo(time.tzname[0]))
output_parts = [
fr'''# SOME DESCRIPTIVE TITLE.
# Copyright (C) YEAR THE PACKAGE'S COPYRIGHT HOLDER
# This file is distributed under the same license as the redemption package.
# FIRST AUTHOR <EMAIL@ADDRESS>, YEAR.
#
#, fuzzy
msgid ""
msgstr ""
"Project-Id-Version: redemption\n"
"Report-Msgid-Bugs-To: \n"
"POT-Creation-Date: {now.strftime('%Y-%m-%d %H:%M%:z')}\n"
"PO-Revision-Date: YEAR-MO-DA HO:MI+ZONE\n"
"Last-Translator: FULL NAME <EMAIL@ADDRESS>\n"
"Language-Team: LANGUAGE <LL@li.org>\n"
"Language: \n"
"MIME-Version: 1.0\n"
"Content-Type: text/plain; charset=CHARSET\n"
"Content-Transfer-Encoding: 8bit\n"
"Plural-Forms: nplurals=INTEGER; plural=EXPRESSION;\n"
'''
]

str_patt = r'((?:\s*"(?:\\.|[^"])*")+\s*)'
for m in re.finditer(
    r'\n//\s*([^\n]+)'
    rf'|\nTR_KV\(\s*(\w+)\s*,{str_patt}\)'
    rf'|\nTR_KV_FMT\(\s*(\w+)\s*,{str_patt}\)'
    rf'|\nTR_KV_PLURAL_FMT\(\s*(\w+)\s*,{str_patt},{str_patt}\)'
    r'|\n',
    contents
):
    nline += 1
    if m[1]:
        last_comment = m[1]
        paths = {path for mod in extract_mod.findall(last_comment)
                 if (path := paths_mapping.get(''.join(mod)))}
        if paths:
            tmp = "\n".join(sorted(paths))
            last_comment = f'{last_comment}\n{tmp}'
    elif m[2]:
        output_parts.append(
            f'\n# id: {m[2]}'
            f'\n# ctx: {last_comment}'
            f'\n#: {input_filename}:{nline}'
            f'\nmsgid {msgid_fmt(m[3])}'
            f'\nmsgstr ""'
            '\n'
        )
        nline += m[3].count('\n')
    elif m[4]:
        output_parts.append(
            f'\n# id: {m[4]}'
            f'\n# ctx: {last_comment}'
            f'\n#: {input_filename}:{nline}'
            f'\n#, c-format'
            f'\nmsgid {msgid_fmt(m[5])}'
            f'\nmsgstr ""'
            '\n'
        )
        nline += m[5].count('\n')
    elif m[6]:
        output_parts.append(
            f'\n# id: {m[6]}'
            f'\n# ctx: {last_comment}'
            f'\n#: {input_filename}:{nline}'
            f'\n#, c-format'
            f'\nmsgid {msgid_fmt(m[7])}'
            f'\nmsgid_plural {msgid_fmt(m[8])}'
            f'\nmsgstr[0] ""'
            f'\nmsgstr[1] ""'
            '\n'
        )
        nline += m[7].count('\n')
        nline += m[8].count('\n')

with open(output_filename, 'w') as f:
    f.write(''.join(output_parts))
