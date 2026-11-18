"""Generate the README latency chart (light + dark). Usage: python gen_latency.py"""
import os

HERE = os.path.dirname(os.path.abspath(__file__))
THEMES = {
    "light": dict(bg="#ffffff", border="#e4e4e7", text="#09090b", muted="#71717a", old="#e4e4e7", new="#18181b"),
    "dark": dict(bg="#09090b", border="#27272a", text="#fafafa", muted="#a1a1aa", old="#3f3f46", new="#fafafa"),
}
ROWS = [("USB", 6.5, 1.3), ("Bluetooth", 19.6, 7.8)]
W, H, X0, SCALE = 800, 232, 132, 30  # SCALE: px per ms

for name, t in THEMES.items():
    out = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}" '
           f'font-family="Inter, ui-sans-serif, system-ui, -apple-system, Segoe UI, sans-serif">',
           f'<rect x=".5" y=".5" width="{W-1}" height="{H-1}" rx="16" fill="{t["bg"]}" stroke="{t["border"]}"/>',
           f'<text x="24" y="40" font-size="15" font-weight="600" fill="{t["text"]}">Worst-case delay per key press</text>',
           f'<rect x="{W-196}" y="28" width="10" height="10" rx="3" fill="{t["old"]}"/>'
           f'<text x="{W-180}" y="37" font-size="12" fill="{t["muted"]}">Stock</text>',
           f'<rect x="{W-128}" y="28" width="10" height="10" rx="3" fill="{t["new"]}"/>'
           f'<text x="{W-112}" y="37" font-size="12" fill="{t["muted"]}">This firmware</text>']
    y = 72
    for label, old, new in ROWS:
        out.append(f'<text x="24" y="{y+27}" font-size="13" font-weight="500" fill="{t["text"]}">{label}</text>')
        out.append(f'<rect x="{X0}" y="{y}" width="{old*SCALE:.0f}" height="18" rx="5" fill="{t["old"]}"/>'
                   f'<text x="{X0+old*SCALE+8:.0f}" y="{y+13}" font-size="12" fill="{t["muted"]}">{old} ms</text>')
        out.append(f'<rect x="{X0}" y="{y+24}" width="{new*SCALE:.0f}" height="18" rx="5" fill="{t["new"]}"/>'
                   f'<text x="{X0+new*SCALE+8:.0f}" y="{y+37}" font-size="12" font-weight="600" fill="{t["text"]}">{new} ms</text>')
        y += 64
    out.append(f'<text x="24" y="{H-18}" font-size="11" fill="{t["muted"]}">Estimated from the source code: debounce, Bluetooth wake-up, report interval, matrix scan.</text>')
    out.append("</svg>\n")
    with open(os.path.join(HERE, f"latency-{name}.svg"), "w", encoding="utf-8", newline="\n") as f:
        f.write("".join(out))
    print("wrote", f"latency-{name}.svg")
