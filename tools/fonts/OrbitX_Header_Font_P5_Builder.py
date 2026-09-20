# Build OrbitX Header Font P5 from the traced P4 outlines, using a standard,
# widely supported TrueType font as the structural template.
# Intended for GitHub Actions; no Python installation needed on user's PC.
from pathlib import Path
import runpy
from fontTools.ttLib import TTFont
from fontTools.pens.ttGlyphPen import TTGlyphPen
from fontTools.ttLib.tables._n_a_m_e import makeName

HERE = Path(__file__).resolve().parent
# P4 generator contains the reference-traced contour coordinates; it is source,
# not an installed font, and runs locally on the GitHub Actions runner.
p4 = runpy.run_path(str(HERE / 'OrbitX_Header_Font_Builder.py'))
paths = p4['paths']
advance = p4['advance']

BASES = [
    Path('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'),
    Path('/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf'),
]
base = next((p for p in BASES if p.is_file()), None)
if base is None:
    raise RuntimeError('Font template not found; install fonts-dejavu-core in CI')
font = TTFont(base)
units = font['head'].unitsPerEm
scale = units / 1000.0
cmap = font.getBestCmap()

for ch, contours in paths.items():
    gn = cmap.get(ord(ch))
    if gn is None:
        raise RuntimeError(f'Template missing character {ch!r}')
    pen = TTGlyphPen(None)
    for contour in contours:
        if len(contour) < 3:
            continue
        first = (round(contour[0][0] * scale), round(contour[0][1] * scale))
        pen.moveTo(first)
        for x, y in contour[1:]:
            pen.lineTo((round(x * scale), round(y * scale)))
        pen.closePath()
    font['glyf'][gn] = pen.glyph()
    font['hmtx'][gn] = (round(advance[ch] * scale), 0)

family = 'OrbitX Header Font P5'
ps_name = 'OrbitXHeaderFontP5-Regular'
for record in font['name'].names:
    values = {1: family, 2: 'Regular', 3: 'OrbitX-Header-P5-Regular-20260919',
              4: family, 6: ps_name, 16: family, 17: 'Regular'}
    if record.nameID in values:
        record.string = values[record.nameID].encode('utf_16_be') if record.isUnicode() else values[record.nameID].encode('latin-1')
font['head'].fontRevision = 0.5
font['head'].macStyle = 0
os2 = font['OS/2']
os2.usWeightClass = 400
os2.usWidthClass = 5
os2.fsSelection = 0x40
os2.fsType = 0
os2.sTypoAscender = round(880 * scale)
os2.sTypoDescender = -round(220 * scale)
os2.sTypoLineGap = 0
os2.usWinAscent = round(1050 * scale)
os2.usWinDescent = round(260 * scale)
font['hhea'].ascent = round(880 * scale)
font['hhea'].descent = -round(220 * scale)
font['hhea'].lineGap = 0
# Remove template kerning and substitution rules: its original character
# shapes/spacing are unrelated to our reference-traced glyphs.
for tag in ('kern','GSUB','GPOS','GDEF','MATH'):
    if tag in font:
        del font[tag]
# Preserve the template's complete cmap and well-supported TrueType tables.
out = HERE / 'OrbitX_Header_Font_Prototype_5.ttf'
font.save(out)
font.close()

check = TTFont(out)
assert all(ord(c) in check.getBestCmap() for c in 'ORBITXabcdefghijklmnopqrstuvwxyz0123456789')
assert check['name'].getDebugName(1) == family
assert check['OS/2'].fsType == 0
assert check['head'].unitsPerEm > 0
check.close()
print(f'P5 built and structurally verified: {out}')
print('Actual rendering in Microsoft Word remains to be verified on Windows.')