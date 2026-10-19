#!/usr/bin/env python3
"""WCAG contrast check for every theme: main text needs 4.5:1, inactive titles and large UI text 3:1.
Translucent panel colours are judged over the theme's own wallpaper colours (both ends)."""
import configparser, glob, os, sys

def parse(c):
    c = c.strip().lstrip('#')
    if len(c) == 3: c = ''.join(x * 2 for x in c)
    if len(c) not in (6, 8): return None
    v = [int(c[i:i + 2], 16) / 255 for i in range(0, len(c), 2)]
    return v if len(v) == 4 else v + [1.0]

def over(top, bottom):
    a = top[3]
    return [top[i] * a + bottom[i] * (1 - a) for i in range(3)] + [1.0]

def lum(c):
    f = lambda x: x / 12.92 if x <= 0.03928 else ((x + 0.055) / 1.055) ** 2.4
    return 0.2126 * f(c[0]) + 0.7152 * f(c[1]) + 0.0722 * f(c[2])

def ratio(a, b):
    la, lb = sorted((lum(a), lum(b)), reverse=True)
    return (la + 0.05) / (lb + 0.05)

CHECKS = [  # fg, bg, minimum
    ("fg", "bg", 4.5), ("window_fg", "window_bg", 4.5), ("title_active_fg", "title_active", 4.5),
    ("title_inactive_fg", "title_inactive", 3.0), ("button_fg", "button_bg", 4.5),
    ("accent_fg", "accent", 3.0), ("panel_fg", "panel_bg", 4.5),
]

failed = 0
root = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'themes')
for path in sorted(glob.glob(os.path.join(root, '*.theme'))):
    ini = configparser.ConfigParser(inline_comment_prefixes=(';',), interpolation=None)
    ini.read(path)
    col = dict(ini['colors']) if ini.has_section('colors') else {}
    col.setdefault('window_bg', col.get('bg', '#000')); col.setdefault('window_fg', col.get('fg', '#fff'))
    desk = [parse(ini.get('desktop', k, fallback=ini.get('desktop', 'color1', fallback='#000'))) for k in ('color1', 'color2')]
    name = os.path.basename(path)[:-6]
    for f, b, need in CHECKS:
        if f not in col or b not in col or parse(col[f]) is None or parse(col[b]) is None:
            continue
        fc, bc = parse(col[f]), parse(col[b])
        bgs = [over(bc, d) for d in desk] if bc[3] < 1 else [bc]
        worst = min(ratio(fc, x) for x in bgs)
        if worst < need:
            failed += 1
            print(f"{name:16} {f} on {b}: {worst:.2f} (needs {need})")
print(f"{failed} contrast problem(s)")
sys.exit(1 if failed else 0)
