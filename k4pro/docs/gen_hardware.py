"""Generate the README hardware diagram (light + dark). Usage: python gen_hardware.py"""
import os

HERE = os.path.dirname(os.path.abspath(__file__))
THEMES = {
    "light": dict(bg="#ffffff", border="#e4e4e7", box="#fafafa", text="#09090b", muted="#71717a", line="#a1a1aa", hl="#18181b", hltext="#fafafa", hlmuted="#a1a1aa"),
    "dark": dict(bg="#09090b", border="#27272a", box="#18181b", text="#fafafa", muted="#a1a1aa", line="#52525b", hl="#fafafa", hltext="#09090b", hlmuted="#52525b"),
}
W, H = 800, 248
FONT = "Inter, ui-sans-serif, system-ui, -apple-system, Segoe UI, sans-serif"


def box(t, x, y, w, h, title, sub, hl=False):
    fill, tc, mc, st = (t["hl"], t["hltext"], t["hlmuted"], t["hl"]) if hl else (t["box"], t["text"], t["muted"], t["border"])
    return (f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="12" fill="{fill}" stroke="{st}"/>'
            f'<text x="{x+w/2}" y="{y+h/2-3}" font-size="14" font-weight="600" fill="{tc}" text-anchor="middle">{title}</text>'
            f'<text x="{x+w/2}" y="{y+h/2+16}" font-size="12" fill="{mc}" text-anchor="middle">{sub}</text>')


def arrow(t, x1, y1, x2, y2, both=False):
    s = f' marker-start="url(#m)"' if both else ""
    return f'<path d="M{x1} {y1} L{x2} {y2}" stroke="{t["line"]}" stroke-width="1.5" fill="none" marker-end="url(#m)"{s}/>'


for name, t in THEMES.items():
    o = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}" font-family="{FONT}">',
         f'<defs><marker id="m" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="7" markerHeight="7" orient="auto-start-reverse">'
         f'<path d="M0 0 L10 5 L0 10 z" fill="{t["line"]}"/></marker></defs>',
         f'<rect x=".5" y=".5" width="{W-1}" height="{H-1}" rx="16" fill="{t["bg"]}" stroke="{t["border"]}"/>',
         box(t, 24, 40, 196, 64, "Key matrix", "6 × 18, shift register"),
         box(t, 24, 144, 196, 64, "LED drivers", "2 chips, I²C"),
         box(t, 302, 40, 196, 168, "STM32L432", "runs this firmware", hl=True),
         box(t, 580, 40, 196, 64, "USB-C", "1000 Hz"),
         box(t, 580, 144, 196, 64, "CKBT51", "Bluetooth, closed firmware"),
         arrow(t, 220, 72, 300, 72),
         arrow(t, 300, 176, 222, 176),
         arrow(t, 500, 72, 578, 72, both=True),
         arrow(t, 500, 176, 578, 176, both=True),
         "</svg>\n"]
    with open(os.path.join(HERE, f"hardware-{name}.svg"), "w", encoding="utf-8", newline="\n") as f:
        f.write("".join(o))
    print("wrote", f"hardware-{name}.svg")
