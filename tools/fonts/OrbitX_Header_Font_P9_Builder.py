"""OrbitX Header Font P9: enlarge the rounded-letter corner radii of P8.

Runs the existing P8 builder in the same directory to preserve its proven font
structure, all spacing/metrics, the P7 M/N geometry and its supported characters.
For each tangent-continuous curved P8 chamfer, lengthen both tangent segments
along the ORIGINAL straight stroke directions, retaining the original quadratic
control point. This gives larger, smooth corners without increasing glyph width.
"""
from pathlib import Path
import runpy
from fontTools.ttLib import TTFont
from fontTools.pens.ttGlyphPen import TTGlyphPen

HERE = Path(__file__).resolve().parent
P8 = runpy.run_path(str(HERE / 'OrbitX_Header_Font_P8_Builder.py'))
P = P8['P']
ROUND_GLYPHS = P8['ROUND_GLYPHS']
P8_SMOOTH = P8['smooth_chamfer_edges']
font = TTFont(HERE / 'OrbitX_Header_Font_Prototype_8.ttf')
cmap = font.getBestCmap()
scale = font['head'].unitsPerEm / 1000.0

# Reference's main C has an outside corner extending roughly one-third of its
# cap height. P8's C is about one-fifth. A 1.75x chamfer expansion brings the
# outside radius to ~245/713 cap units; the inner corner grows consistently.
# On very short strokes, restrict the enlargement to 40% of the adjacent
# straight run, avoiding overlapping/inside-out segments in B, S, 3, 8, etc.
RADIUS_MULTIPLIER = 1.75
MAX_EXTENSION_FRACTION = 0.40


def polygon_area(shape):
    return sum(x1*y2-x2*y1 for (x1,y1),(x2,y2)
               in zip(shape,shape[1:]+shape[:1]))


def expand_existing_curves(contour, old_curves):
    """Return new vertices with each P8 tangent-continuous curve enlarged.

    If b->c was a P8 diagonal with control at the crossing of adjacent straight
    lines, move b backwards on a->b and c forwards on c->d. The control point
    stays at that SAME crossing: both quadratic endpoint tangents therefore
    remain exactly horizontal/vertical, with no extra kink or interpolation.
    """
    updated = [tuple(point) for point in contour]
    n = len(contour)
    for i, control in old_curves.items():
        bi, ci = i, (i + 1) % n
        a = contour[(i - 1) % n]
        b = contour[bi]
        c = contour[ci]
        d = contour[(i + 2) % n]
        prev_length = abs(b[0]-a[0]) + abs(b[1]-a[1])
        next_length = abs(d[0]-c[0]) + abs(d[1]-c[1])
        assert prev_length > 0 and next_length > 0
        # Increase each tangent radius independently to retain the original
        # slight horizontal/vertical asymmetry of the reference-based outline.
        extend_before = min((RADIUS_MULTIPLIER-1)*abs(c[0]-b[0])
                            if a[1]==b[1] else
                            (RADIUS_MULTIPLIER-1)*abs(c[1]-b[1]),
                            MAX_EXTENSION_FRACTION*prev_length)
        extend_after = min((RADIUS_MULTIPLIER-1)*abs(c[0]-b[0])
                           if c[1]==d[1] else
                           (RADIUS_MULTIPLIER-1)*abs(c[1]-b[1]),
                           MAX_EXTENSION_FRACTION*next_length)
        updated[bi] = (round(b[0] + (a[0]-b[0])*extend_before/prev_length),
                       round(b[1] + (a[1]-b[1])*extend_before/prev_length))
        updated[ci] = (round(c[0] + (d[0]-c[0])*extend_after/next_length),
                       round(c[1] + (d[1]-c[1])*extend_after/next_length))
        # The curve's control must continue to sit at the 90-degree intersection
        # of its endpoint tangent axes. Vertex moves above preserve that point.
        start,end=updated[bi],updated[ci]
        assert (start[1]==control[1] and end[0]==control[0]) or (
               start[0]==control[0] and end[1]==control[1])
    return updated


for ch, source_contours in P.items():
    if ch.upper() not in ROUND_GLYPHS:
        continue  # M, N, angular letters and non-rounded digits untouched.
    glyph_name = cmap.get(ord(ch))
    assert glyph_name is not None, f'Missing glyph for {ch!r}'
    pen=TTGlyphPen(None)
    x_min = min(x for contour in source_contours for x,y in contour)
    padding = 175 if ch.isupper() or ch.isdigit() else 154
    shift_x = padding - x_min
    outer_area=polygon_area(source_contours[0])

    def design_point(pt):
        x,y=pt
        return (round((x+shift_x)*scale),round(y*scale))

    any_curves=False
    for contour_index, src in enumerate(source_contours):
        if len(src)<3:
            continue
        contour=list(src)
        if contour_index and outer_area*polygon_area(contour)>0:
            contour.reverse()  # Keep P8's nonzero-winding counter handling.
        curved_edges=P8_SMOOTH(contour)
        expanded=expand_existing_curves(contour,curved_edges)
        any_curves |= bool(curved_edges)
        pen.moveTo(design_point(expanded[0]))
        for i in range(len(expanded)):
            j=(i+1)%len(expanded)
            if i in curved_edges:
                pen.qCurveTo(design_point(curved_edges[i]), design_point(expanded[j]))
            elif j!=0:
                pen.lineTo(design_point(expanded[j]))
        pen.closePath()
    font['glyf'][glyph_name]=pen.glyph()
    # The expanded curves run INSIDE the original P8 ink bounds. Keep hmtx
    # unchanged: user-approved P7/P8 advance widths and bearings remain intact.
    assert font['hmtx'][glyph_name][0] > 0

family='OrbitX Header Font P9'
names={1:family,2:'Regular',3:'OrbitX-Header-P9-Regular-20260920',
       4:family,6:'OrbitXHeaderFontP9-Regular',16:family,17:'Regular'}
for record in font['name'].names:
    if record.nameID in names:
        val=names[record.nameID]
        record.string=val.encode('utf_16_be') if record.isUnicode() else val.encode('latin-1')
font['head'].fontRevision=0.9
out=HERE/'OrbitX_Header_Font_Prototype_9.ttf'
font.save(out)
font.close()

check=TTFont(out)
assert check['name'].getDebugName(1)==family
assert check['OS/2'].fsType==0
assert all(ord(ch) in check.getBestCmap() for ch in 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789')
assert check['maxp'].numGlyphs>1000
check.close()
print(f'P9 enlarged-radius TTF generated: {out.name}')
print('Windows/Word rendering still requires the user to test the installed TTF.')