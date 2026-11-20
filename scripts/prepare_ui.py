#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Embed artwork and existing credit sources; no runtime file dependencies."""
import json
import pathlib
import re
import sys

root = pathlib.Path(__file__).resolve().parents[1]
out = pathlib.Path(sys.argv[1])
out.mkdir(parents=True, exist_ok=True)

def plain_markdown(text):
    text = re.sub(r'\[([^\]]+)\]\(([^)]+)\)', r'\1 (\2)', text)
    text = re.sub(r'^#+\s*', '', text, flags=re.M)
    text = text.replace('**', '').replace('`', '')
    lines = []
    for line in text.splitlines():
        if line.startswith('|---'):
            continue
        if line.startswith('|'):
            line = ': '.join(part.strip() for part in line.strip('|').split('|'))
        lines.append(line)
    return '\n'.join(lines)

def literal(text):
    return json.dumps(text, ensure_ascii=False)

author = json.loads((root/'app-builder.json').read_text())['store']['author']
texts = {
    'project_author': author['display_name'],
    'project_email': author['email'],
    'code_authors': 'Frotz Zero: '+author['display_name']+'\n'+(root/'assets/code-authors.txt').read_text(),
    'third_party_notices': plain_markdown((root/'THIRD_PARTY_NOTICES.md').read_text()),
    'gpl_licence': (root/'LICENSE').read_text(),
}
(out/'credits.inc').write_text('// Generated from authoritative project notices.\n'+
    ''.join(f'static const char {name}[] = {literal(value)};\n' for name, value in texts.items()), encoding='utf-8')
with (out/'artwork.inc').open('w', encoding='utf-8') as target:
    target.write('// Generated from original CC0 artwork.\n')
    for size in (24, 40, 96):
        data = (root/f'assets/brand-{size}.png').read_bytes()
        target.write(f'static const unsigned char brand_{size}[] = {{\n')
        for offset in range(0, len(data), 24):
            target.write(','.join(str(x) for x in data[offset:offset+24])+',\n')
        target.write('};\n')
