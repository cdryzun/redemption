#!/usr/bin/env python3
# Dominique Lafages, Jonathan Poelen
# Copyright WALLIX 2023

###############################################################################################
# script extracting a font definition and converting it to the RBF2 format used by ReDemPtion.
#
# HINTs:
# - Each RBF2 glyph is sketched in a bitmap whose dimensions are mutiples of 8. As PIL glyphes
#   width are not multiple of 8 they have to be padded. By convention, they are padded to left
#   and bottom.
# - The glyphs are not antialiased.
# - The police is variable sized
# - Thus, each pixel in a sketch is represented by only one bit
#
# FORMATs :
# - the RBF2 file always begins by the label "RBF2"
# - Police global informations are :
#     * version (u32)
#     * name (u8[32]) (ex : Deja Vu Sans)
#     * fontsize (u16)
#     * fontstyle (u16) (always '1')
#     * max ascent (u16)
#     * max descent (u16)
#     * number of glyph (u32)
#     * unicode max (u32)
#     * total data len: sum of aligned_of_4(glyph_data_len) (u32)
#     * replacement glyph (assume uni < CONTIGUOUS_LIMIT)
#     * glyph in range [CHARSET_START..CHARSET_END]
# - Individual glyph informations are :
#     ? when uni < CONTIGUOUS_LIMIT
#       * has_glyph (u8 = 1 or 0)
#     ? when has_glyph = 1 or when uni < CONTIGUOUS_LIMIT
#       ? when uni >= CONTIGUOUS_LIMIT
#         * unicode value (u32)
#       * offsetx (u8)
#       * offsety (u8)
#       * incby (u8)
#       * cx (s8)
#       * cy (s8)
#       * data (the bitmap representing the sketch of the glyph, one bit by pixel,
#               0 for background, 1 for foreground)
#
# TECHs :
# - struct.pack formats are :
#     * '<' little endian
#     * 'h' [short] for a two bytes emision
#     * 'B' [unsigned char] for a one byte emission
#     * 'L' [unsigned int] for a four bytes emission
# - the data generation loop print each glyph sketch to sdtout, with each bit represented as
#   follow :
#     * '.' for a PIL background bit
#     * '#' for a PIL foreground bit
#     * 'o' for an horizontal end of line paddind bit
#     * '+' for a vertical paddind line of bits
#     * '>' for a right space
###############################################################################################

from PIL.ImageFont import ImageFont, truetype
from unicodedata import category
from typing import Iterable, NamedTuple
from enum import IntEnum

import os
import sys
import struct
import PIL


class GlyphType(IntEnum):
    Normal = 0
    Replacement = 1
    Unknown = 2

BBox = tuple[int, int, int, int]  # x1, y1, x2, y2
Pixels = bytes
GlyphInfo = tuple[GlyphType, Pixels, BBox, int, int]  # int, int = offsetx, offsety


global_fontsize = 14
fallback_fontpath = ''
name = ''
output = ''

CHARSET_START = 32
CHARSET_END = 0x2fa1e

CONTIGUOUS_LIMIT = 0xD7FC

PRIVATE_UNI_START = 0xF000
PRIVATE_UNI_END = 0xF8FF

# variant of pre-existing character
ALTERNATIVE_GLYPHS: dict[
    int,  # alternative unicode value
    int,  # original unicode value
] = {
    0xf102: 0xf100, # angles-left thin
    0xf103: 0xf101, # angles-right thin
}

DOT_BYTE = b'.'[0]
def to_pixels(img: bytes) -> bytes:
    return bytes(0 if c == DOT_BYTE else 255 for c in img)

CUSTOM_GLYPHS_BY_FONT_SIZE: dict[
    int,  # font size
    tuple[
        int,  # unicode value
        GlyphInfo
    ],
] = {
    14: (

        # X xmark
        (0xf00d, (
            GlyphType.Normal,
            to_pixels(
                b'##.......##'
                b'.##.....##.'
                b'..##...##..'
                b'...##.##...'
                b'....###....'
                b'....###....'
                b'...##.##...'
                b'..##...##..'
                b'.##.....##.'
                b'##.......##'
            ),
            (0, 1, 11, 11),
            -1, 3,
        )),

        # copy
        (0xf0c5, (
            GlyphType.Normal,
            to_pixels(
                b'...#######..'
                b'...#.....##.'
                b'...#......##'
                b'...#.......#'
                b'####.......#'
                b'#..#.......#'
                b'#..#.......#'
                b'#..#.......#'
                b'#..#.......#'
                b'#..#########'
                b'#.......#...'
                b'#.......#...'
                b'#.......#...'
                b'#########...'
            ),
            (0, 0, 12, 14),
            0, 1,
        )),
        
        # ☐ square
        (0xf0c8, (
            GlyphType.Normal,
            to_pixels(
                b'############'
                b'#..........#'
                b'#..........#'
                b'#..........#'
                b'#..........#'
                b'#..........#'
                b'#..........#'
                b'#..........#'
                b'#..........#'
                b'#..........#'
                b'#..........#'
                b'############'
            ),
            (0, 0, 12, 12),
            0, 2,
        )),

        # ☒ square-check
        (0xf14a, (
            GlyphType.Normal,
            to_pixels(
                b'############'
                b'#..........#'
                b'#........#.#'
                b'#.......##.#'
                b'#......##..#'
                b'#.....##...#'
                b'#.##.##....#'
                b'#..###.....#'
                b'#...#......#'
                b'#..........#'
                b'#..........#'
                b'############'
            ),
            (0, 0, 12, 12),
            0, 2,
        )),

        # « angles-left
        (0xf100, (
            GlyphType.Normal,
            to_pixels(
                b'....##...##'
                b'...##...##.'
                b'..##...##..'
                b'.##...##...'
                b'##...##....'
                b'##...##....'
                b'.##...##...'
                b'..##...##..'
                b'...##...##.'
                b'....##...##'
            ),
            (0, 1, 11, 11),
            -1, 3,
        )),

        # « angles-left (alternative thin)
        (0xf102, (
            GlyphType.Normal,
            to_pixels(
                b'....#...#'
                b'...#...#.'
                b'..#...#..'
                b'.#...#...'
                b'#...#....'
                b'#...#....'
                b'.#...#...'
                b'..#...#..'
                b'...#...#.'
                b'....#...#'
            ),
            (0, 1, 9, 11),
            -1, 3,
        )),

        # » angles-right
        (0xf101, (
            GlyphType.Normal,
            to_pixels(
                b'##...##....'
                b'.##...##...'
                b'..##...##..'
                b'...##...##.'
                b'....##...##'
                b'....##...##'
                b'...##...##.'
                b'..##...##..'
                b'.##...##...'
                b'##...##....'
            ),
            (1, 1, 12, 11),
            0, 3,
        )),

        # » angles-right (alternative thin)
        (0xf103, (
            GlyphType.Normal,
            to_pixels(
                b'#...#....'
                b'.#...#...'
                b'..#...#..'
                b'...#...#.'
                b'....#...#'
                b'....#...#'
                b'...#...#.'
                b'..#...#..'
                b'.#...#...'
                b'#...#....'
            ),
            (1, 1, 10, 11),
            0, 3,
        )),

        # ‹ angle-left
        (0xf104, (
            GlyphType.Normal,
            to_pixels(
                b'....##'
                b'...##.'
                b'..##..'
                b'.##...'
                b'##....'
                b'##....'
                b'.##...'
                b'..##..'
                b'...##.'
                b'....##'
            ),
            (0, 1, 6, 11),
            -1, 3,
        )),

        # › angle-right
        (0xf105, (
            GlyphType.Normal,
            to_pixels(
                b'##....'
                b'.##...'
                b'..##..'
                b'...##.'
                b'....##'
                b'....##'
                b'...##.'
                b'..##..'
                b'.##...'
                b'##....'
            ),
            (1, 1, 7, 11),
            0, 3,
        )),

        # 🖹 U+1f5b9 file
        (0xf15b, (
            GlyphType.Normal,
            to_pixels(
                b'########...'
                b'#.....###..'
                b'#.....#.##.'
                b'#.....#..##'
                b'#.....#####'
                b'#.........#'
                b'#.........#'
                b'#.........#'
                b'#.........#'
                b'#.........#'
                b'#.........#'
                b'#.........#'
                b'#.........#'
                b'###########'
            ),
            (0, 0, 11, 14),
            0, 1,
        )),

        # 🖹 U+1f5b9 file-line
        (0xf15c, (
            GlyphType.Normal,
            to_pixels(
                b'########...'
                b'#.....###..'
                b'#.....#.##.'
                b'#.....#..##'
                b'#.....#####'
                b'#.........#'
                b'#.........#'
                b'#.#######.#'
                b'#.........#'
                b'#.........#'
                b'#.#######.#'
                b'#.........#'
                b'#.........#'
                b'###########'
            ),
            (0, 0, 11, 14),
            0, 1,
        )),

        # ↓ arrow-down-a-z
        (0xf15d, (
            GlyphType.Normal,
            to_pixels(
                b'...#......#..'
                b'...#.....###.'
                b'...#.....#.#.'
                b'...#....##.##'
                b'...#....#####'
                b'...#....#...#'
                b'...#.........'
                b'...#....#####'
                b'##.#.##....#.'
                b'.#####....#..'
                b'..###....#...'
                b'...#....#####'
            ),
            (0, 0, 13, 12),
            0, 2,
        )),

        # ↓ arrow-down-1-9
        (0xf162, (
            GlyphType.Normal,
            to_pixels(
                b'...#......##.'
                b'...#.....#.#.'
                b'...#.......#.'
                b'...#.......#.'
                b'...#.....####'
                b'...#.........'
                b'...#.....####'
                b'...#.....#..#'
                b'##.#.##..####'
                b'.#####.....##'
                b'..###.....##.'
                b'...#......#..'
            ),
            (0, 0, 13, 12),
            -1, 2,
        )),

        # ↑ arrow-up-1-9
        (0xf163, (
            GlyphType.Normal,
            to_pixels(
                b'...#......##.'
                b'..###....#.#.'
                b'.#####.....#.'
                b'##.#.##....#.'
                b'...#.....####'
                b'...#.........'
                b'...#.....####'
                b'...#.....#..#'
                b'...#.....####'
                b'...#.......##'
                b'...#......##.'
                b'...#......#..'
            ),
            (0, 0, 13, 12),
            -1, 2,
        )),

        # ↑ arrow-up-z-a
        (0xf882, (
            GlyphType.Normal,
            to_pixels(
                b'...#....#####'
                b'..###......#.'
                b'.#####....#..'
                b'##.#.##..#...'
                b'...#....#####'
                b'...#.........'
                b'...#......#..'
                b'...#.....###.'
                b'...#.....#.#.'
                b'...#....##.##'
                b'...#....#####'
                b'...#....#...#'
            ),
            (0, 0, 13, 12),
            0, 2,
        )),

        # ↓ arrow-down-9-1
        (0xf886, (
            GlyphType.Normal,
            to_pixels(
                b'...#.....####'
                b'...#.....#..#'
                b'...#.....####'
                b'...#.......##'
                b'...#......##.'
                b'...#......#..'
                b'...#.........'
                b'...#......##.'
                b'##.#.##..#.#.'
                b'.#####.....#.'
                b'..###......#.'
                b'...#.....####'
            ),
            (0, 0, 13, 12),
            0, 2,
        )),

        # ↑ arrow-up-9-1
        (0xf887, (
            GlyphType.Normal,
            to_pixels(
                b'...#.....####'
                b'..###....#..#'
                b'.#####...####'
                b'##.#.##....##'
                b'...#......##.'
                b'...#......#..'
                b'...#.........'
                b'...#......##.'
                b'...#.....#.#.'
                b'...#.......#.'
                b'...#.......#.'
                b'...#.....####'
            ),
            (0, 0, 13, 12),
            -1, 2
        )),

        # ⏹ circle-stop
        (0xf28d, (
            GlyphType.Normal,
            to_pixels(
                b'....######....'
                b'..##########..'
                b'.############.'
                b'.############.'
                b'####......####'
                b'####......####'
                b'####......####'
                b'####......####'
                b'####......####'
                b'####......####'
                b'.############.'
                b'.############.'
                b'..##########..'
                b'....######....'
            ),
            (0, 0, 14, 14),
            -1, 2
        )),

    )
}

# based on Font Awesome
def icon(i: int) -> tuple[int, int]:
    assert PRIVATE_UNI_START <= i <= PRIVATE_UNI_END
    return (i, i)

# inclusive range
ICHARS_GEN = (
    (0, 0xD7FB),
    icon(0xf00d),  # X xmark
    icon(0xf07b),  # 🗀 U+1f5c0 folder
    icon(0xf0a0),  # 🖴 U+1f5b4 hard-drive
    icon(0xf0c5),  # copy
    icon(0xf0c7),  # 🖬 U+1f5ac floppy-disk
    icon(0xf0c8),  # ☐ square
    icon(0xf100),  # « angles-left
    icon(0xf101),  # » angles-right
    icon(0xf102),  # « angles-left (alternative thin)
    icon(0xf103),  # » angles-right (alternative thin)
    icon(0xf104),  # ‹ angle-left
    icon(0xf105),  # › angle-right
    icon(0xf14a),  # ☒ square-check
    icon(0xf15b),  # 🖹 U+1f5b9 file
    icon(0xf15c),  # 🖹 U+1f5b9 file-line
    icon(0xf15d),  # ↓ arrow-down-a-z
    icon(0xf162),  # ↓ arrow-down-1-9
    icon(0xf163),  # ↑ arrow-up-1-9
    icon(0xf28d),  # ⏹ circle-stop
    icon(0xf51f),  # 🖸 U+1f5b8 compact-disc
    icon(0xf6ff),  # 🖧 U+1f5a7 network-drive
    icon(0xf882),  # ↑ arrow-up-z-a
    icon(0xf886),  # ↓ arrow-down-9-1
    icon(0xf887),  # ↑ arrow-up-9-1
    (0xF900, 0xFFFC),
    # -- skip replacement char --
    (0xFFFE, 0xFFFF),
    (0x1A000, 0x1B2FF),
    (0x1E000, 0x1EFFF),
    # -- skip symbols, pictograms, etc (U+1F000 - U+1FFFF) --
    (0x20000, 0x3FFFF),  # CJK
)

CUSTOM_GLYPHS: dict[tuple[
    int,  # unicode value
    GlyphInfo
]] = {}  # init later


if len(sys.argv) > 1:
    import argparse
    parser = argparse.ArgumentParser(description='rfb2 font generator')
    parser.add_argument('-o', '--output', metavar='FILENAME', default='')
    parser.add_argument('-r', '--range', nargs=2, type=int, default=(CHARSET_START, CHARSET_END))
    parser.add_argument('-d', '--font-dir', metavar='DIRNAME', type=str, default=fallback_fontpath)
    parser.add_argument('-s', '--font-size', type=int, default=global_fontsize)
    parser.add_argument('-n', '--name', type=str, default=name)

    args = parser.parse_args()

    CHARSET_START = int(args.range[0])
    CHARSET_END = int(args.range[1])
    fallback_fontpath = args.font_dir
    global_fontsize = args.font_size
    output = args.output
    name = args.name


if custom_glyphs := CUSTOM_GLYPHS_BY_FONT_SIZE.get(global_fontsize):
    for uni, glyph_info in custom_glyphs:
        assert uni not in CUSTOM_GLYPHS
        CUSTOM_GLYPHS[uni] = glyph_info


def get_fontpath(filename: str, dirnames: Iterable[str] = ()) -> str:
    if fallback_fontpath:
        path = f'{fallback_fontpath}/{filename}'
        if os.path.exists(path):
            return path

    for d in dirnames:
        path = f'{d}/{filename}'
        if os.path.exists(path):
            return path

    return filename


type FontDesc = tuple[
    int,  # fontsize or 0 for global fontsize
    str,  # path of font
    Iterable[str],  # glyph for invalid char rendering
]

font_descriptions: Iterable[FontDesc] = (
    (global_fontsize, get_fontpath('DejaVuSans.ttf', (
        '/usr/share/fonts/truetype/dejavu',
    )), ('\u02ef', '\u20e3')),

    # https://www.latofonts.com/lato-free-fonts/
    # (global_fontsize, get_fontpath('Lato-Light.ttf', (
    #     '/usr/share/fonts/truetype/lato/'  # ubuntu
    #     '/usr/share/fonts/TTF/',  # arch
    # )), ('\u0370',)),

    # https://github.com/notofonts/noto-cjk/raw/main/Sans/OTC/NotoSansCJK-Regular.ttc
    (global_fontsize - 2, get_fontpath('NotoSansCJK-Regular.ttc', (
        '/usr/share/fonts/opentype/noto/',  # ubuntu
        '/usr/share/fonts/noto-cjk/',  # arch
    )), ('\u0104', '\u0302')),
)

font_icon_descriptions: Iterable[FontDesc] = (
    # https://fontawesome.com/download
    # https://use.fontawesome.com/releases/v7.1.0/fontawesome-free-7.1.0-desktop.zip
    (global_fontsize, get_fontpath('Font Awesome 7 Free-Regular-400.otf'), ('\uFFFD',)),
    (global_fontsize, get_fontpath('Font Awesome 7 Free-Solid-900.otf'), ('\uFFFD',)),
)

if not name:
    name = ','.join(os.path.splitext(os.path.basename(font_desc[1]))[0] for font_desc in font_descriptions)

if not output:
    output = f"{name}_{global_fontsize}.rbf2"


def nbbytes(x: int) -> int:
    return (x + 7) // 8


def align4(x: int) -> int:
    return (x + 3) & ~3


def count_bit_padding(cx: int) -> int:
    return (8 - cx % 8) % 8


class FontInfo(NamedTuple):
    font: ImageFont
    unknown_chars: Iterable[tuple[Pixels, BBox, BBox]]
    ascent: int


def mask_to_tuple(mask: PIL.Image.core) -> Pixels:
    bbox = mask.getbbox()
    yseq = range(bbox[1], bbox[3])
    xseq = range(bbox[0], bbox[2])
    return bytes(bytearray(mask.getpixel((ix, iy))
                           for iy in yseq
                           for ix in xseq))


def load_truetype(fontpath: str, fontsize: int, unknown_unicode_for_glyphs: tuple[str]) -> FontInfo:
    print(f'load {fontpath}')
    font = truetype(fontpath, fontsize or global_fontsize)
    return FontInfo(font,
                    (*((mask_to_tuple(mask := font.getmask(uni, mode='1')),
                        mask.getbbox(),
                        font.getbbox(uni, mode='1'))
                       for uni in unknown_unicode_for_glyphs),),
                    font.getmetrics()[0],
                    )


def load_fonts(font_descriptions: Iterable[FontDesc]) -> list[FontInfo]:
    return [
        load_truetype(fontpath, fontsize or global_fontsize, unknown_unicode_for_glyphs)
        for fontsize, fontpath, unknown_unicode_for_glyphs in font_descriptions
    ]

font_infos = load_fonts(font_descriptions)
font_icon_infos = load_fonts(font_icon_descriptions)


def get_ascent_descent(fonts_infos: list[FontInfo]) -> tuple[int, int]:
    max_ascent = max(font_info.ascent for font_info in font_infos)
    max_descent = max(font_info.font.getmetrics()[1] for font_info in font_infos)
    return (max_ascent, max_descent)


max_ascent, max_descent = get_ascent_descent(font_infos)
icon_max_ascent, icon_max_descent = get_ascent_descent(font_icon_infos)
if max_ascent < icon_max_ascent:
    print(f'ascent is lower that ascent icon: {max_ascent} < {icon_max_ascent}',
          file=sys.stderr)
    exit(1)
if max_descent < icon_max_descent:
    print(f'descent is grater that descent icon: {max_descent} > {icon_max_descent}',
          file=sys.stderr)
    exit(1)


unknown_char = font_infos[0].unknown_chars[0]
unknown_glyph = (GlyphType.Unknown,
                 unknown_char[0],
                 unknown_char[1],
                 unknown_char[2][0],
                 unknown_char[2][1] + max_ascent - font_infos[0].ascent,
                 )

replacement_uni = 0xFFFD
replacement_unicode_char = chr(replacement_uni)
replacement_char: GlyphInfo = (
    GlyphType.Replacement,
    mask_to_tuple(mask := font_infos[0].font.getmask(replacement_unicode_char, mode='1')),
    mask.getbbox(),
    (bbox_font := font_infos[0].font.getbbox(replacement_unicode_char, mode='1'))[0],
    bbox_font[1] + max_ascent - font_infos[0].ascent,
)


def get_glyph_info(char: str, font_infos: Iterable[FontInfo]) -> GlyphInfo:
    for font_info in font_infos:
        mask = font_info.font.getmask(char, mode='1')
        bbox_font = font_info.font.getbbox(char, mode='1')
        x1, y1, x2, y2 = bbox_font
        bbox = mask.getbbox()
        # is None for spaces
        if not bbox:
            # rdesktop require non empty data for glyph
            # create a transparent image with height=1
            y = 0
            bbox = (x1, y - 1, x2, y)
            pixels = b'0' * (x2 * y2)
        else:
            pixels = mask_to_tuple(mask)
            if any(bbox == unknown_char[1] and pixels == unknown_char[0]
                   for unknown_char in font_info.unknown_chars):
                continue

        offsetx = x1
        offsety = y1 + max_ascent - font_info.ascent
        return GlyphType.Normal, pixels, bbox, offsetx, offsety
    return unknown_glyph


def is_printable(char: str) -> bool:
    cat = category(char)
    general_cat = cat[0]
    return general_cat != 'C' and (general_cat != 'Z' or cat == 'Zs')


def serialize_glyph(x1: int, y1: int, cx: int, cy: int, incby: int, offsetx: int, pixels: bytes) -> tuple[bytes, str]:
    data = b''
    padding = count_bit_padding(cx)
    empty_line = '+' * incby + '\n'
    # padding_line = 'o' * padding
    left_empty_line = '+' * offsetx
    right_empty_line = '>' * (incby - cx) + '\n'
    line = empty_line * y1
    for iy in range(y1, y1 + cy):
        line += left_empty_line
        byte = 0
        counter = 0

        for ix in range(x1, x1 + cx):
            pix = pixels[(iy - y1) * cx + (ix - x1)]
            byte <<= 1
            if pix == 255:
                line += '#'
                byte |= 1
            else:
                line += '.'

            counter += 1
            if counter == 8:
                data += struct.pack('<B', byte)
                counter = 0
                byte = 0

        if counter != 0:
            data += struct.pack('<B', byte << padding)

        line += right_empty_line

    return data, line


glyph_graph_adjust_y = {
    0x25b8,  # ▸
    0x25b9,  # ▹
    0x25ba,  # ►
    0x25bb,  # ▻
    0x25c2,  # ◂
    0x25c3,  # ◃
    0x25c4,  # ◄
    0x25c5,  # ◅
}


class Glyphs:
    def __init__(self) -> None:
        self.total_data_len = 0
        self.data_glyphs = []
        self.max_heigth = 0
        self.unicode_values = set()  # for check duplicate

    def add(self, uni: int, char: str, glyph_info: GlyphInfo, force_insert: bool = False) -> None:
        assert uni not in self.unicode_values, uni
        self.unicode_values.add(uni)

        glyph_type, pixels, bbox, offsetx, offsety = glyph_info

        x1, y1, x2, y2 = bbox
        incby = x2 - offsetx
        cx = x2 - x1
        cy = y2 - y1
        offsetx = max(1, offsetx + x1)
        offsety = max(0, offsety + y1)

        self.max_heigth = max(offsety + cy, self.max_heigth)

        if global_fontsize == 14:
            # because space is usually too big
            if uni == 114:  # 'r'
                incby -= 1
            # align with '◀'/'◁'/'▶'/'▷'
            elif uni in glyph_graph_adjust_y:
                offsety += 1


        print(f'{uni:#x}  CHR: {char}  TYPE: {glyph_type}  CX/CY: {cx},{cy}  INCBY: {incby}  OFFSET: {offsetx},{offsety}  BBOX: {bbox}')

        if glyph_type == GlyphType.Normal or force_insert:
            data, line = serialize_glyph(x1, y1, cx, cy, incby, offsetx, pixels)

            if uni < CONTIGUOUS_LIMIT or force_insert:
                datainfo = struct.pack('<bbbBBB', 1, offsetx, offsety, incby, cx, cy)
            else:
                datainfo = struct.pack('<IbbBBB', uni, offsetx, offsety, incby, cx, cy)

            self.total_data_len += align4(nbbytes(cx) * cy)

            print(line, end='\n\n')
            self.data_glyphs.append(datainfo + data)

        elif uni < CONTIGUOUS_LIMIT:
            # replacement
            self.data_glyphs.append(b'\0')


glyphs = Glyphs()

glyphs.add(replacement_uni, 'ReplacementChar', replacement_char, force_insert=True)

for rng in (range(max(CHARSET_START, r[0]), min(CHARSET_END, r[1] + 1)) for r in ICHARS_GEN):
    for uni in rng:
        char = chr(uni)

        if glyph_info := CUSTOM_GLYPHS.get(uni):
            glyphs.add(uni, char, glyph_info)
            continue

        font_char = char

        if uni_ref := ALTERNATIVE_GLYPHS.get(uni):
            font_char = chr(uni_ref)

        if PRIVATE_UNI_START <= uni <= PRIVATE_UNI_END:
            glyphs.add(uni, char, get_glyph_info(font_char, font_icon_infos))
        elif is_printable(char):
            glyphs.add(uni, char, get_glyph_info(font_char, font_infos))
        else:
            if uni < CONTIGUOUS_LIMIT:
                # replacement
                glyphs.data_glyphs.append(b'\0')

            print(f'{uni:#x}  CHR: NonPrintable')


print(f'Output file: {output}')

with open(output, 'wb') as f:
    # Magic number
    f.write("RBF2".encode())

    f.write(struct.pack('<I', 1))  # version
    f.write((name.encode() + b'\0' * 32)[0:32])
    f.write(struct.pack('<H', global_fontsize))
    f.write(struct.pack('<H', 1))  # style
    f.write(struct.pack('<H', max_ascent))
    f.write(struct.pack('<H', max_descent))
    f.write(struct.pack('<I', len(glyphs.data_glyphs)))
    f.write(struct.pack('<I', CHARSET_END))
    f.write(struct.pack('<I', glyphs.total_data_len))

    for data in glyphs.data_glyphs:
        f.write(data)
