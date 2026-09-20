"""Package the approved P11 outlines as OrbitX Header for app-only rendering.

Run this from GitHub Actions (not on the end user's computer). P11 remains the
glyph and spacing master: this step ONLY changes the font's internal name and
writes an install-free application asset into Runtime/Fonts.
"""
from pathlib import Path
import runpy
from fontTools.ttLib import TTFont

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
runpy.run_path(str(HERE / "OrbitX_Header_Font_P11_Builder.py"))
source = HERE / "OrbitX_Header_Font_Prototype_11.ttf"
output = ROOT / "Runtime" / "Fonts" / "OrbitXHeader.ttf"
output.parent.mkdir(parents=True, exist_ok=True)

font = TTFont(source)
family = "OrbitX Header"
names = {
    1: family,                     # family
    2: "Regular",                  # subfamily
    3: "OrbitX-Header-Regular-1.0",
    4: family,                     # full font name
    5: "Version 1.000",
    6: "OrbitXHeader-Regular",     # PostScript name
    16: family,                    # typographic family
    17: "Regular",
}
for rec in font["name"].names:
    if rec.nameID in names:
        value = names[rec.nameID]
        rec.string = value.encode("utf_16_be") if rec.isUnicode() else value.encode("latin-1")
for ident, value in names.items():
    font["name"].setName(value, ident, 3, 1, 0x0409)
font["head"].fontRevision = 1.0
font["OS/2"].fsType = 0
font.save(output)
font.close()

original = TTFont(source)
bundled = TTFont(output)
assert bundled["name"].getDebugName(1) == family
assert bundled["name"].getDebugName(4) == family
assert bundled["name"].getDebugName(6) == "OrbitXHeader-Regular"
assert bundled["OS/2"].fsType == 0
assert original["hmtx"].metrics == bundled["hmtx"].metrics, "P11 spacing changed"
for char in "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789":
    old_name = original.getBestCmap()[ord(char)]
    new_name = bundled.getBestCmap()[ord(char)]
    a, b = original["glyf"][old_name], bundled["glyf"][new_name]
    assert list(a.coordinates) == list(b.coordinates), f"{char}: outline changed"
    assert list(a.flags) == list(b.flags), f"{char}: curve flags changed"
original.close()
bundled.close()
print(f"Packaged {output.relative_to(ROOT)} as {family} (exact P11 outlines and metrics)")
