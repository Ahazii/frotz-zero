#!/usr/bin/env python3
"""Render the canonical CC0 SVG with system librsvg/Cairo (artwork tools only).

Ubuntu/Debian: apt install python3-gi python3-cairo python3-gi-cairo gir1.2-rsvg-2.0
Normal builds use the checked-in PNGs and need no SVG rendering tools.
"""
import pathlib
import xml.etree.ElementTree as ET
import gi
gi.require_version('Rsvg', '2.0')
from gi.repository import Rsvg
import cairo

root = pathlib.Path(__file__).resolve().parents[1]
source = (root / 'assets/frotz-zero.svg').read_bytes()

def render(data, target, size):
    surface = cairo.ImageSurface(cairo.FORMAT_ARGB32, size, size)
    handle = Rsvg.Handle.new_from_data(data)
    box = Rsvg.Rectangle()
    box.x = box.y = 0
    box.width = box.height = size
    handle.render_document(cairo.Context(surface), box)
    surface.write_to_png(str(target))

render(source, root / 'assets/frotz-zero.png', 256)
mark = ET.fromstring(source)
for child in list(mark):
    if child.get('id') == 'background':
        mark.remove(child)
# Render each actual UI size directly, avoiding nearest-neighbour downsampling.
for size in (24, 40, 96):
    render(ET.tostring(mark), root / f'assets/brand-{size}.png', size)
print('Rendered launcher icon and three UI marks from assets/frotz-zero.svg')
