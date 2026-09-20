"""OrbitX Header Font P10 — glyph-by-glyph reference audit and re-drawn curves.

P9 is intentionally NOT used: its radius expansion left asymmetric short horizontal
radii on C and failed to round E. Instead P10 starts from the P8 font, preserves
its hmtx, P7 M/N, ASCII coverage, and standard Windows-friendly TrueType tables.

Reference audit, uppercase A–Z (source: 'Font Reference(1).png', Sept 2026):
 A: straight diagonals, retain P8. B: round two right lobes / two inner counters.
 C: custom wide, visibly circular outer upper/lower-left arcs; smaller concentric
    inner arcs, long horizontal terminals (the user's principal reported issue).
 D: round right-side upper/lower shoulders; keep squared left stem.
 E: CUSTOM: broad lower-left outer arc, subtle inner lower-left arc; horizontal
    center/top bars and right-hand slanted top tip (user's second reported issue).
 F: center-left inside elbow subtly round; keep sharp top/outer left stem.
 G: C-like upper/lower left shoulders and rounded lower-right bowl; keep bar.
 H: straight stem/crossbar, retain P8. I: straight, retain P8.
 J: rounded left-hand bottom hook and inner end; retain upper stem straight.
 K: angular branches, retain P8.
 L: outer and inner lower-left corners curved with distinct, consistent radii.
 M: retain P7 balanced strokes. N: retain P7 balanced strokes.
 O: outer rounded-rect corners and smaller concentric inside corners.
 P: top and lower right shoulder plus inner counter, keep left stem straight.
 Q: rounded O-like bowl with angular diagonal tail preserved.
 R: rounded upper bowl and counter, angular diagonal leg retained.
 S: visibly round left-upper and right-lower outer shoulders plus waist.
 T: straight/angled top bar, retain P8. U: soft lower outer and inner corners.
 V W X Y Z: intentional straight/angular geometry, retain P8.
 Lowercase: as in P8, proportional small-cap descendants of the uppercase.
 Digits: retain P8 design/metrics (not the focus of this alphabet audit).

P10 only changes letters whose reference has curves. Original design is an AI
raster image and cannot provide exact mathematical radius measurements; the
explicit radii below are deliberate visual reconstructions, not pixel matches.
"""
from pathlib import Path
import runpy
from fontTools.ttLib import TTFont
from fontTools.pens.ttGlyphPen import TTGlyphPen

HERE = Path(__file__).resolve().parent
P8 = runpy.run_path(str(HERE/'OrbitX_Header_Font_P8_Builder.py'))
P = P8['P']
smooth = P8['smooth_chamfer_edges']
font = TTFont(HERE/'OrbitX_Header_Font_Prototype_8.ttf')
cmap = font.getBestCmap()
scale = font['head'].unitsPerEm / 1000

# Radius in the ORIGINAL P8 1,000-upem design coordinates. Unlike P9, BOTH
# tangents of a corner receive explicitly equal/appropriate design radii.
# Values are tailored independently after reviewing each reference glyph.
RADII = {
  'B': ((145, 140), (72, 65)),
  'D': ((172, 158), (90, 85)),
  'F': ((42, 40), (42, 40)),
  'G': ((185, 170), (85, 82)),
  'J': ((135, 125), (72, 66)),
  'O': ((162, 145), (82, 80)),
  'P': ((153, 140), (84, 78)),
  'Q': ((165, 150), (88, 83)),
  'R': ((151, 141), (78, 75)),
  'S': ((155, 145), (69, 65)),
  'U': ((146, 138), (70, 65)),
}

# Path commands M / L / Q / Z in design units. Control points sit at the
# ideal perpendicular-line intersections, so the transitions to every straight
# edge are exactly tangent-continuous (no jaggedness, shoulders, or bumps).
# These contours were re-drawn rather than modifying old P8/P9 chamfers.
HAND_DRAWN = {
 'C': [[
  ('M',1290,733),('L',620,733),('Q',390,733,390,520),
  ('L',390,233),('Q',390,20,620,20),('L',1290,20),
  ('L',1290,113),('L',558,113),('Q',468,113,468,205),
  ('L',468,548),('Q',468,640,558,640),('L',1290,640),('Z',)
 ]],
 'E': [
  [('M',375,423),('L',1196,423),('L',1196,330),('L',468,330),
   ('L',468,159),('Q',468,113,517,113),('L',1274,113),
   ('L',1274,20),('L',535,20),('Q',375,20,375,183),('Z',)],
  [('M',375,733),('L',1274,733),('L',1258,640),('L',375,640),('Z',)]
 ],
 'L': [[
  ('M',484,733),('L',576,733),('L',576,158),
  ('Q',576,113,621,113),('L',1196,113),('L',1196,20),
  ('L',619,20),('Q',484,20,484,155),('Z',)
 ]]
}


def area(contour):
    return sum(x1*y2-x2*y1 for (x1,y1),(x2,y2) in
               zip(contour, contour[1:]+contour[:1]))


def radiused_contour(src, glyph, contour_i, is_lower=False):
    """Replace one original diagonal chamfer with a true smooth quadratic.

    Keep its right-angle control exactly at the intersection of the adjoining
    straight runs. Respect both incoming and outgoing available straight runs;
    critically, do NOT use P9's 'move original endpoint by a fraction of
    adjacent segment', which gave C radically different horizontal and
    vertical radii and mishandled shared vertices.
    """
    outline=list(src)
    old_curves=smooth(src)
    if not old_curves:
        return outline, old_curves
    rx, ry = RADII[glyph][min(contour_i,1)]
    if is_lower:
        rx, ry = round(rx*0.77), round(ry*542/713)
    proposals={}
    n=len(src)
    for i,control in old_curves.items():
        a,b,c,d=src[(i-1)%n],src[i],src[(i+1)%n],src[(i+2)%n]
        old_rx=abs(c[0]-b[0]); old_ry=abs(c[1]-b[1])
        # Preserve deliberately tiny inside elbows (e.g. S waist, F crossbar).
        # They must NOT become a full-size exterior radius.
        use_rx=min(rx, round(old_rx*1.22)) if old_rx < 65 else rx
        use_ry=min(ry, round(old_ry*1.22)) if old_ry < 65 else ry
        prev_length=abs(b[0]-a[0])+abs(b[1]-a[1])
        next_length=abs(d[0]-c[0])+abs(d[1]-c[1])
        # Radius reaches from the control intersection, not the old endpoint.
        # Reserve 12% of each originally straight neighboring run.
        br=(control[0]-a[0])**2+(control[1]-a[1])**2
        er=(control[0]-d[0])**2+(control[1]-d[1])**2
        _=prev_length,next_length,br,er
        if a[1]==b[1] and c[0]==d[0]:  # horizontal -> vertical
            before=min(use_rx, int(abs(control[0]-a[0])*.82))
            after=min(use_ry, int(abs(control[1]-d[1])*.82))
            new_b=(control[0] + (1 if b[0]>control[0] else -1)*before,control[1])
            new_c=(control[0],control[1] + (1 if c[1]>control[1] else -1)*after)
        elif a[0]==b[0] and c[1]==d[1]:  # vertical -> horizontal
            before=min(use_ry, int(abs(control[1]-a[1])*.82))
            after=min(use_rx, int(abs(control[0]-d[0])*.82))
            new_b=(control[0],control[1] + (1 if b[1]>control[1] else -1)*before)
            new_c=(control[0] + (1 if c[0]>control[0] else -1)*after,control[1])
        else:
            continue
        proposals[i]=new_b
        proposals[(i+1)%n]=new_c
    for idx,point in proposals.items():
        outline[idx]=point
    return outline, old_curves


def draw_handwritten(pen, commands, ch):
    is_lower=ch.islower()
    def scaled_point(x,y):
        if is_lower:
            x=round(840+(x-840)*.77)
            y=round(15+(y-20)*(542/713))
        shift=154 if is_lower else 175
        x_min = min(x0 for c in P[ch] for x0, y0 in c)
        return round((x+shift-x_min)*scale),round(y*scale)
    for op in commands:
        name,*values=op
        if name=='M':pen.moveTo(scaled_point(*values))
        elif name=='L':pen.lineTo(scaled_point(*values))
        elif name=='Q':
            cx,cy,x,y=values
            pen.qCurveTo(scaled_point(cx,cy),scaled_point(x,y))
        elif name=='Z':pen.closePath()
        else:raise RuntimeError(name)


def draw_regular(pen, ch, glyph):
    src_contours=P[ch]
    x_min=min(x for contour in src_contours for x,y in contour)
    padding=154 if ch.islower() else 175
    shift=padding-x_min
    def pt(point):
        x,y=point
        return round((x+shift)*scale),round(y*scale)
    outside=area(src_contours[0])
    for index, source in enumerate(src_contours):
        contour=list(source)
        if index and area(contour)*outside > 0:
            contour.reverse()  # Enclosed counters must have opposite winding.
        edited, curves=radiused_contour(contour,glyph,index,ch.islower())
        pen.moveTo(pt(edited[0]))
        for i in range(len(contour)):
            j=(i+1)%len(contour)
            if i in curves:
                pen.qCurveTo(pt(curves[i]),pt(edited[j]))
            elif j:
                pen.lineTo(pt(edited[j]))
        pen.closePath()

# Each uppercase is audited above. All intentional angular letter outlines are
# copied verbatim from P8 and must remain geometrically and metrically unchanged.
CHANGED='BCDEFGJLOPQRSU'
assert len(CHANGED)==len(set(CHANGED))
for ch in CHANGED + CHANGED.lower():
    glyph=cmap[ord(ch)]
    old_advance=font['hmtx'][glyph]
    pen=TTGlyphPen(None)
    if ch.upper() in HAND_DRAWN:
        for contour in HAND_DRAWN[ch.upper()]:
            draw_handwritten(pen,contour,ch)
    else:
        draw_regular(pen,ch,ch.upper())
    font['glyf'][glyph]=pen.glyph()
    assert font['hmtx'][glyph]==old_advance

# Word recognizes each new version independently; keep P5-style proven tables.
family='OrbitX Header Font P10'
names={1:family,2:'Regular',3:'OrbitX-Header-P10-Regular-20260920',
       4:family,6:'OrbitXHeaderFontP10-Regular',16:family,17:'Regular'}
for record in font['name'].names:
    if record.nameID in names:
        value=names[record.nameID]
        record.string=(value.encode('utf_16_be') if record.isUnicode()
                       else value.encode('latin-1'))
font['head'].fontRevision=1.0
out=HERE/'OrbitX_Header_Font_Prototype_10.ttf'
font.save(out)
font.close()
verify=TTFont(out)
assert verify['name'].getDebugName(1)==family
assert verify['OS/2'].fsType==0
assert verify['maxp'].numGlyphs>1000
assert all(ord(ch) in verify.getBestCmap() for ch in
           'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789')
verify.close()
print('Created',out.name,'with independently audited uppercase glyphs and P8 spacing')
print('Windows/Word appearance still requires user testing on Windows.')