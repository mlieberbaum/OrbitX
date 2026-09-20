"""OrbitX Header Font P7: hand-reconstructed geometric outlines, balanced spacing.

Build on GitHub Actions with no end-user Python installation. Retains the standard
TrueType tables and Windows compatibility approach of P5, but replaces raster-
traced outlines with deliberate straight segments and consistent corners.
Original outline/reference is preserved by keeping the legacy P4 builder as input
for punctuation and any other unmapped characters.
"""
from pathlib import Path
import runpy
from fontTools.ttLib import TTFont
from fontTools.pens.ttGlyphPen import TTGlyphPen

HERE = Path(__file__).resolve().parent
old = runpy.run_path(str(HERE / 'OrbitX_Header_Font_Builder.py'))
paths = old['paths']
# Every interior coordinate below is a *design decision*, not pixel tracing.
# Glyph geometry uses the P4 1,000-upem reference coordinate system, whose cap
# boundaries are y=20 and y=733. Nested contours define counters in B, D, O etc.
P = {
'A': [
 [(282,20),(392,20),(808,640),(872,640),(1288,20),(1398,20),(884,733),(796,733)]
],
'B': [
 [(375,733),(1164,733),(1274,640),(1274,485),(1202,381),(1305,283),(1305,130),(1195,20),(375,20)],
 [(468,640),(1138,640),(1196,578),(1196,490),(1138,423),(468,423)],
 [(468,330),(1138,330),(1212,260),(1212,181),(1138,113),(468,113)]
],
'C': [[(1290,733),(530,733),(390,602),(390,152),(530,20),(1290,20),(1290,113),(554,113),(468,199),(468,554),(554,640),(1290,640)]],
'D': [
 [(375,733),(1150,733),(1305,601),(1305,153),(1150,20),(375,20)],
 [(468,640),(1128,640),(1212,555),(1212,199),(1128,113),(468,113)]
],
'E': [
 [(375,423),(1196,423),(1196,330),(468,330),(468,155),(510,113),(1274,113),(1274,20),(496,20),(375,141)],
 [(375,733),(1274,733),(1274,640),(375,640)]
],
'F': [
 [(390,20),(484,20),(484,299),(515,330),(1166,330),(1166,423),(390,423)],
 [(390,733),(1290,733),(1258,640),(484,640),(484,516),(390,516)]
],
'G': [[(1290,733),(525,733),(375,605),(375,144),(500,20),(1155,20),(1305,145),(1305,423),(933,423),(933,330),(1212,330),(1212,180),(1135,113),(545,113),(468,190),(468,555),(545,640),(1290,640)]],
'H': [[(390,733),(468,733),(468,423),(1212,423),(1212,733),(1290,733),(1290,20),(1212,20),(1212,330),(468,330),(468,20),(390,20)]],
'I': [[(794,733),(886,733),(886,20),(794,20)]],
'J': [[(1166,733),(1258,733),(1258,175),(1104,20),(514,20),(422,113),(422,206),(514,206),(514,175),(552,113),(1074,113),(1166,202)]],
'K': [[(406,733),(499,733),(499,423),(590,423),(1138,733),(1274,733),(817,386),(1274,20),(1130,20),(717,330),(499,330),(499,20),(406,20)]],
'L': [[(484,733),(576,733),(576,152),(616,113),(1196,113),(1196,20),(608,20),(484,141)]],
'M': [[(328,20),(328,733),(438,733),(840,268),(1242,733),(1352,733),(1352,20),(1258,20),(1258,579),(891,175),(789,175),(422,579),(422,20)]],
'N': [[(360,20),(360,733),(454,733),(1226,141),(1226,733),(1320,733),(1320,20),(1226,20),(454,610),(454,20)]],
'O': [
 [(500,733),(1180,733),(1305,608),(1305,145),(1180,20),(500,20),(375,145),(375,608)],
 [(530,640),(1150,640),(1212,578),(1212,175),(1150,113),(530,113),(468,175),(468,578)]
],
'P': [
 [(375,20),(468,20),(468,268),(1179,268),(1305,390),(1305,610),(1180,733),(375,733)],
 [(468,640),(1150,640),(1212,578),(1212,423),(1150,361),(468,361)]
],
'Q': [
 [(515,733),(1165,733),(1290,608),(1290,144),(1200,54),(1290,-42),(1167,-42),(1104,20),(515,20),(390,145),(390,608)],
 [(545,640),(1135,640),(1228,548),(1228,199),(1140,113),(1072,113),(948,268),(856,268),(856,237),(948,113),(545,113),(452,206),(452,548)]
],
'R': [
 [(375,733),(1150,733),(1305,635),(1305,430),(1205,330),(1088,330),(1305,20),(1180,20),(937,330),(468,330),(468,20),(375,20)],
 [(468,640),(1145,640),(1212,573),(1212,492),(1145,423),(468,423)]
],
'S': [[(1305,733),(530,733),(406,640),(406,485),(530,361),(1181,361),(1212,330),(1212,206),(1119,113),(375,113),(375,20),(1181,20),(1305,144),(1305,330),(1181,454),(530,454),(499,485),(499,578),(561,640),(1305,640)]],
'T': [[(375,733),(1305,733),(1305,640),(886,640),(886,20),(794,20),(794,640),(375,640)]],
'U': [[(390,733),(484,733),(484,192),(563,113),(1117,113),(1196,192),(1196,733),(1290,733),(1290,144),(1165,20),(515,20),(390,144)]],
'V': [[(375,733),(482,733),(840,113),(1198,733),(1305,733),(902,20),(778,20)]],
'W': [[(96,733),(194,733),(499,113),(779,702),(902,702),(1181,113),(1486,733),(1584,733),(1274,20),(1134,20),(840,578),(546,20),(406,20)]],
'X': [
 [(298,733),(441,733),(840,441),(1239,733),(1382,733),(915,381),(1382,20),(1239,20),(840,322),(441,20),(298,20),(765,381)],
],
'Y': [[(328,733),(461,733),(855,426),(1249,733),(1352,733),(902,350),(902,20),(808,20),(808,350)]],
'Z': [[(360,733),(1320,733),(1320,640),(540,113),(1320,113),(1320,20),(360,20),(360,113),(1140,640),(360,640)]],
'0': [[(500,606),(1180,606),(1305,490),(1305,136),(1180,20),(500,20),(375,136),(375,490)],[(530,522),(1150,522),(1212,460),(1212,166),(1150,104),(530,104),(468,166),(468,460)]],
'1': [[(716,494),(855,606),(964,606),(964,20),(871,20),(871,490),(716,408)]],
'2': [[(390,606),(1180,606),(1290,500),(1290,348),(1180,243),(546,243),(484,181),(484,113),(1290,113),(1290,20),(390,20),(390,192),(510,327),(1134,327),(1196,389),(1196,460),(1134,522),(484,522)]],
'3': [[(406,606),(1150,606),(1274,492),(1274,352),(1200,271),(1274,190),(1274,130),(1160,20),(406,20),(406,113),(1135,113),(1181,159),(1181,199),(1135,243),(654,243),(654,327),(1135,327),(1181,371),(1181,460),(1135,522),(406,522)]],
'4': [[(748,606),(871,606),(468,280),(468,243),(1119,243),(1119,606),(1212,606),(1212,243),(1305,243),(1305,150),(1212,150),(1212,20),(1119,20),(1119,150),(375,150),(375,299)]],
'5': [[(390,606),(1258,606),(1258,522),(484,522),(484,327),(1150,327),(1290,220),(1290,125),(1170,20),(390,20),(390,113),(1140,113),(1196,168),(1196,191),(1140,243),(468,243),(390,327)]],
'6': [[(546,606),(1258,606),(1258,522),(546,522),(484,460),(484,327),(1150,327),(1290,220),(1290,124),(1170,20),(500,20),(390,124),(390,460)],[(484,243),(1150,243),(1196,197),(1196,150),(1150,104),(530,104),(484,150)]],
'7': [[(406,606),(1274,606),(1274,494),(840,20),(716,20),(1181,522),(406,522)]],
'8': [[(500,606),(1180,606),(1290,512),(1290,360),(1210,271),(1290,182),(1290,115),(1180,20),(500,20),(390,115),(390,182),(470,271),(390,360),(390,512)],[(530,522),(1150,522),(1196,477),(1196,392),(1150,327),(530,327),(484,392),(484,477)],[(530,243),(1150,243),(1196,198),(1196,150),(1150,104),(530,104),(484,150),(484,198)]],
'9': [[(500,606),(1180,606),(1290,490),(1290,124),(1170,20),(422,20),(422,104),(1150,104),(1196,150),(1196,243),(500,243),(390,352),(390,490)],[(530,522),(1150,522),(1196,475),(1196,370),(1150,327),(530,327),(484,370),(484,475)]]
}

# Preserve the design of lowercase P4: small-cap variants of uppercase, not
# a conventional lowercase alphabet. For consistency they inherit clean P7
# contours and are then scaled to the source lowercase's 15..557 vertical box.
for ch in 'abcdefghijklmnopqrstuvwxyz':
    src = P[ch.upper()]
    P[ch] = [[(round(840 + (x-840)*0.77), round(15+(y-20)*(542/713))) for x,y in contour]
             for contour in src]

# Source P4 also supports spaces and punctuation. Those are carried forward;
# simplify nothing that belongs to punctuation, preserving its exact shape.
BASES = [Path('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'),
         Path('/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf')]
base = next((p for p in BASES if p.is_file()), None)
if not base:
    raise RuntimeError('Install fonts-dejavu-core on the GitHub Actions runner')
font = TTFont(base)
scale = font['head'].unitsPerEm / 1000.0
cmap = font.getBestCmap()
assert all(ch in paths for ch in 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789')

# Maintain the glyph-centered arrangement but remove the P4/P5 excess tracking.
# Every horizontal advance is now tied to the *actual ink width*. This fixes
# disproportionately large side gaps around I, J, 1 and slender punctuation.
for ch, contours in {**paths, **P}.items():
    name = cmap.get(ord(ch))
    if not name:
        raise RuntimeError(f'Base font has no code point for {ch!r}')
    pen = TTGlyphPen(None)
    all_x = [x for contour in contours for x, y in contour]
    if not all_x:
        continue
    x_min, x_max = min(all_x), max(all_x)
    padding = 175 if ch.isupper() or ch.isdigit() else 154
    if ch == ' ':
        padding = 230
    # Reposition the drawn outline: visual bearings are equal to padding.
    shift_x = padding - x_min
    def signed_area(shape):
        return sum(x1*y2-x2*y1 for (x1,y1),(x2,y2) in zip(shape,shape[1:]+shape[:1]))
    outer_orientation = signed_area(contours[0]) if contours else 0
    for contour_index, contour in enumerate(contours):
        if len(contour)<3:
            continue
        # TrueType's nonzero fill rule requires the enclosed counters to have
        # winding opposite the outer boundary (otherwise B/D/O/0 fill solid).
        if contour_index and ch in P and outer_orientation*signed_area(contour) > 0:
            contour = list(reversed(contour))
        pen.moveTo((round((contour[0][0]+shift_x)*scale),round(contour[0][1]*scale)))
        for x,y in contour[1:]:
            pen.lineTo((round((x+shift_x)*scale),round(y*scale)))
        pen.closePath()
    font['glyf'][name] = pen.glyph()
    font['hmtx'][name] = (round((x_max-x_min+2*padding)*scale),round(padding*scale))

# Space has no outline and must be defined as a nonzero advance.
space = cmap.get(ord(' '))
if space:
    font['glyf'][space] = TTGlyphPen(None).glyph()
    font['hmtx'][space] = (round(420*scale),0)

family = 'OrbitX Header Font P7'
names = {1:family, 2:'Regular', 3:'OrbitX-Header-P7-Regular-20260920',
         4:family, 6:'OrbitXHeaderFontP7-Regular',16:family,17:'Regular'}
for record in font['name'].names:
    if record.nameID in names:
        value = names[record.nameID]
        record.string = value.encode('utf_16_be') if record.isUnicode() else value.encode('latin-1')
font['head'].fontRevision=0.7
font['head'].macStyle=0
os2=font['OS/2']
os2.usWeightClass=400
os2.usWidthClass=5
os2.fsSelection=0x40
os2.fsType=0
os2.sTypoAscender=round(880*scale)
os2.sTypoDescender=-round(220*scale)
os2.sTypoLineGap=0
os2.usWinAscent=round(1050*scale)
os2.usWinDescent=round(260*scale)
font['hhea'].ascent=round(880*scale)
font['hhea'].descent=-round(220*scale)
font['hhea'].lineGap=0
for tag in ('kern','GSUB','GPOS','GDEF','MATH'):
    if tag in font: del font[tag]
out=HERE/'OrbitX_Header_Font_Prototype_7.ttf'
font.save(out)
font.close()
check=TTFont(out)
assert check['name'].getDebugName(1)==family
assert all(ord(ch) in check.getBestCmap() for ch in 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789')
assert check['OS/2'].fsType == 0
check.close()
print(f'P7 clean-outline font generated: {out.name}')
print('Actual MS Word compatibility must be confirmed on the user Windows machine.')