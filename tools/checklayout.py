#!/usr/bin/env python3
"""Verify the stacked band layout has no overlaps.

The bands are packed within a few pixels of each other so the clock can be as
large as possible, which makes "does anything collide?" impossible to eyeball
from the constants alone. This reads the real values out of ui_bands.h and
ui_fonts.h, computes each row's extent from the *font's own* metrics, and fails
if any two rows touch.

Line height follows lv_tiny_ttf: size * (ascender - descender + lineGap) / unitsPerEm.

usage: tools/checklayout.py
"""
import re
import struct
import sys
import pathlib

ROOT = pathlib.Path(__file__).resolve().parent.parent
SCREEN_H = 600


def defines(path):
    """Pull `#define NAME <int>` pairs out of a header."""
    out = {}
    for m in re.finditer(r'^#define\s+(\w+)\s+(\d+)', path.read_text(), re.M):
        out[m.group(1)] = int(m.group(2))
    return out


def vmetrics(ttf):
    d = ttf.read_bytes()
    tabs = {}
    for i in range(struct.unpack('>H', d[4:6])[0]):
        off = 12 + 16 * i
        tag = d[off:off + 4].decode('latin1')
        tabs[tag] = struct.unpack('>II', d[off + 8:off + 16])
    upm = struct.unpack('>H', d[tabs['head'][0] + 18:tabs['head'][0] + 20])[0]
    asc, desc, gap = struct.unpack('>hhh', d[tabs['hhea'][0] + 4:tabs['hhea'][0] + 10])
    return upm, asc, desc, gap


def line_height(ttf, size):
    upm, asc, desc, gap = vmetrics(ttf)
    return int((size / upm) * (asc - desc + gap))


def main():
    bands = defines(ROOT / 'main/ui/ui_bands.h')
    fonts = defines(ROOT / 'main/ui/ui_fonts.h')
    thin = ROOT / 'main/assets/Ubuntu-Th.ttf'
    mono = ROOT / 'main/assets/UbuntuMono-B.ttf'
    med = ROOT / 'main/assets/Ubuntu-M.ttf'

    # The news slots are clipped to a fixed height rather than to their font.
    slot = int(re.search(r'#define NEWS_SLOT_H (\d+)',
                         (ROOT / 'main/ui/ui_bands.c').read_text()).group(1))

    rows = [
        ('weather main', bands['UI_BAND_WEATHER_MAIN'], line_height(med, fonts['FACE_LABEL_L'])),
        ('weather meta', bands['UI_BAND_WEATHER_META'], line_height(med, fonts['UI_TEXT_SM'])),
        ('weather fcst', bands['UI_BAND_WEATHER_FCST'], line_height(med, fonts['UI_TEXT_SM'])),
        # The tallest family wins: thin has a 1.149x line height, mono 1.0x.
        ('time',         bands['UI_BAND_TIME_Y'],       line_height(thin, fonts['FACE_TIME_THIN'])),
        ('date',         bands['UI_BAND_DATE_Y'],       line_height(med, fonts['UI_TEXT_MD'])),
        ('news 1',       bands['UI_BAND_NEWS_1'],       slot),
        ('news 2',       bands['UI_BAND_NEWS_2'],       slot),
        ('ticker',       bands['UI_BAND_TICKER'],       line_height(med, fonts['UI_TEXT_MD'])),
    ]

    bad = []
    print(f'{"row":<14}{"top":>6}{"height":>8}{"bottom":>8}   gap to next')
    for i, (name, y, h) in enumerate(rows):
        gap = rows[i + 1][1] - (y + h) if i + 1 < len(rows) else SCREEN_H - (y + h)
        flag = ''
        if gap < 0:
            flag = '  <-- OVERLAP'
            bad.append(f'{name} overruns {rows[i + 1][0] if i + 1 < len(rows) else "the screen"} by {-gap}px')
        print(f'{name:<14}{y:>6}{h:>8}{y + h:>8}{gap:>14}{flag}')

    # The time and date must stay inside the slice faces treat as theirs.
    top, bottom = bands['UI_BAND_FACE_TOP'], bands['UI_BAND_FACE_BOTTOM']
    print(f'\nface band: {top}..{bottom} ({bottom - top}px)')
    for name, y, h in rows[3:5]:
        if y < top:
            bad.append(f'{name} starts above FACE_TOP')
        if y + h > bottom:
            bad.append(f'{name} ends below FACE_BOTTOM')

    fcst = rows[2]
    if fcst[1] + fcst[2] > top:
        bad.append('weather forecast runs into the face band')
    if bottom > rows[5][1]:
        bad.append('face band runs into news 1')

    if bad:
        print('\nFAIL')
        for b in bad:
            print(' -', b)
        return 1
    print('\nOK - no overlaps')
    return 0


if __name__ == '__main__':
    sys.exit(main())
