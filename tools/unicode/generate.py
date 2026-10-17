#!/usr/bin/env python3
"""Generate Unicode 17.0.0 width and grapheme tables for width.c.

Download the files in SHA256 below from https://www.unicode.org/Public/17.0.0/ucd/
into a directory, preserving subdirectories, then run:
  python3 tools/unicode/generate.py /path/to/ucd > width_data.inc

The build uses the checked-in output and needs neither Python nor the UCD.
Unicode data is covered by tools/unicode/LICENSE.txt.
"""

from collections import defaultdict
import hashlib
import pathlib
import sys

SHA256 = {
    'UnicodeData.txt': '2e1efc1dcb59c575eedf5ccae60f95229f706ee6d031835247d843c11d96470c',
    'EastAsianWidth.txt': 'ea7ce50f3444a050333448dffef1cadd9325af55cbb764b4a2280faf52170a33',
    'DerivedCoreProperties.txt': '24c7fed1195c482faaefd5c1e7eb821c5ee1fb6de07ecdbaa64b56a99da22c08',
    'emoji/emoji-data.txt': '2cb2bb9455cda83e8481541ecf5b6dfda66a3bb89efa3fa7c5297eccf607b72b',
    'LineBreak.txt': 'e6a18fa91f8f6a6f8e534b1d3f128c21ada45bfe152eb6b1bcc5e15fd8ac92e6',
    'auxiliary/GraphemeBreakProperty.txt': 'd6b51d1d2ae5c33b451b7ed994b48f1f4dc62b2272a5831e7fd418514a6bae89',
}
MAX = 0x10ffff

GCB = [
    'Other', 'CR', 'LF', 'Control', 'Extend', 'ZWJ', 'Regional_Indicator',
    'Prepend', 'SpacingMark', 'L', 'V', 'T', 'LV', 'LVT',
]
INCB = ['None', 'Linker', 'Consonant', 'Extend']
NO_START = {'CL', 'CP', 'EX', 'IS', 'NS', 'CJ'}
NO_END = {'OP'}

WIDE_DEFAULTS = [
    (0x3400, 0x4dbf), (0x4e00, 0x9fff), (0xf900, 0xfaff),
    (0x20000, 0x2fffd), (0x30000, 0x3fffd),
]

SHIFT = 7


def records(text):
    for line in text.splitlines():
        line = line.split('#')[0].strip()
        if line:
            yield [field.strip() for field in line.split(';')]


def interval(text):
    parts = text.split('..')
    return int(parts[0], 16), int(parts[-1], 16)


def load(directory):
    files = {}
    for name, digest in SHA256.items():
        data = (pathlib.Path(directory) / name).read_bytes()
        if digest is not None and hashlib.sha256(data).hexdigest() != digest:
            raise ValueError(f'Expected unmodified Unicode 17.0.0 {name}')
        files[name] = data.decode('utf-8')
    return files


def general_categories(text):
    gc = ['Cn'] * (MAX + 1)
    first = None
    for fields in records(text):
        cp = int(fields[0], 16)
        if fields[1].endswith(', First>'):
            first = cp
            continue
        start = first if fields[1].endswith(', Last>') else cp
        first = None
        gc[start:cp + 1] = [fields[2]] * (cp + 1 - start)
    return gc


def ranges(text, value_filter):
    for fields in records(text):
        if value_filter(fields):
            yield interval(fields[0])


def properties(files):
    gc = general_categories(files['UnicodeData.txt'])

    eaw = ['N'] * (MAX + 1)
    for start, end in WIDE_DEFAULTS:
        for cp in range(start, end + 1):
            if gc[cp] == 'Cn':
                eaw[cp] = 'W'
    for codes, value in records(files['EastAsianWidth.txt']):
        start, end = interval(codes)
        eaw[start:end + 1] = [value] * (end + 1 - start)

    gcb = [0] * (MAX + 1)
    for codes, value in records(files['auxiliary/GraphemeBreakProperty.txt']):
        start, end = interval(codes)
        gcb[start:end + 1] = [GCB.index(value)] * (end + 1 - start)

    incb = [0] * (MAX + 1)
    ignorable = bytearray(MAX + 1)
    for fields in records(files['DerivedCoreProperties.txt']):
        start, end = interval(fields[0])
        if fields[1] == 'InCB':
            incb[start:end + 1] = [INCB.index(fields[2])] * (end + 1 - start)
        elif fields[1] == 'Default_Ignorable_Code_Point':
            ignorable[start:end + 1] = b'\1' * (end + 1 - start)

    emoji = bytearray(MAX + 1)
    pict = bytearray(MAX + 1)
    for codes, name in records(files['emoji/emoji-data.txt']):
        start, end = interval(codes)
        if name == 'Emoji':
            emoji[start:end + 1] = b'\1' * (end + 1 - start)
        elif name == 'Extended_Pictographic':
            pict[start:end + 1] = b'\1' * (end + 1 - start)

    kinsoku = [0] * (MAX + 1)
    for codes, value in records(files['LineBreak.txt']):
        start, end = interval(codes)
        k = 1 if value in NO_START else 2 if value in NO_END else 0
        kinsoku[start:end + 1] = [k] * (end + 1 - start)

    zero_gcb = {GCB.index('V'), GCB.index('T'), GCB.index('Control'),
                GCB.index('CR'), GCB.index('LF')}
    ri = GCB.index('Regional_Indicator')

    props = [0] * (MAX + 1)
    for cp in range(MAX + 1):
        if cp == 0xad:
            width = 1
        elif (
            gc[cp] in ('Mn', 'Me', 'Cf', 'Cc', 'Cs')
            or ignorable[cp]
            or gcb[cp] in zero_gcb
        ):
            width = 0
        elif eaw[cp] in ('W', 'F') or gcb[cp] == ri:
            width = 2
        else:
            width = 1
        props[cp] = (
            width
            | (gcb[cp] << 2)
            | (pict[cp] << 6)
            | (emoji[cp] << 7)
            | (incb[cp] << 8)
            | (kinsoku[cp] << 10)
        )
    return props


def emit(name, ctype, values, per_line, fmt):
    print(f'static {ctype} const {name}[{len(values)}] = {{')
    for i in range(0, len(values), per_line):
        row = ', '.join(fmt.format(v) for v in values[i:i + per_line])
        print(f'        {row},')
    print('};\n')


def generate(directory):
    props = properties(load(directory))

    values = sorted(set(props))
    index = {v: i for i, v in enumerate(values)}
    assert len(values) <= 256

    size = 1 << SHIFT
    blocks, stage1, stage2 = {}, [], []
    for base in range(0, MAX + 1, size):
        block = bytes(index[p] for p in props[base:base + size])
        if block not in blocks:
            blocks[block] = len(blocks)
            stage2.extend(block)
        stage1.append(blocks[block])
    assert len(blocks) <= 256

    print('// Generated by tools/unicode/generate.py; do not edit.')
    print('// Unicode 17.0.0; https://www.unicode.org/Public/17.0.0/ucd/')
    print('// See tools/unicode/LICENSE.txt for the Unicode data license.\n')
    print(f'#define PROP_SHIFT {SHIFT}\n')
    emit('PropValues', 'u16', values, 8, '0x{:03X}')
    emit('PropStage1', 'u8', stage1, 16, '{:3d}')
    emit('PropStage2', 'u8', stage2, 16, '{:3d}')


if __name__ == '__main__':
    generate(sys.argv[1])
