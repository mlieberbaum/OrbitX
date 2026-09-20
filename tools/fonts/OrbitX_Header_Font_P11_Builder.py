"""OrbitX Header Font P11: individual uppercase/number stroke corrections.

Builds from P10 (NOT P9), retaining its standard Windows-compatible TrueType
structure, all original glyph advances/side bearings, and unmodified uppercase
and digit outlines. P11 edits exactly A B G J K Q R S U 0 3 8, according to
user's annotated repeated-letter Word specimen. Lowercase left untouched.
"""
from pathlib import Path
import math
import runpy
from fontTools.ttLib import TTFont
from fontTools.pens.ttGlyphPen import TTGlyphPen
from fontTools.pens.reverseContourPen import ReverseContourPen
from fontTools.pens.recordingPen import RecordingPen

HERE = Path(__file__).resolve().parent
p10 = runpy.run_path(str(HERE/'OrbitX_Header_Font_P10_Builder.py'))
P = p10['P']
font = TTFont(HERE/'OrbitX_Header_Font_Prototype_10.ttf')
cmap = font.getBestCmap()
scale = font['head'].unitsPerEm/1000.0

# All original design geometry is in 1,000-unit coordinates with the user-approved
# P7/P8 horizontal metrics. One local coordinate system per glyph, matching P10.
def glyph_pen(ch):
    xmin = min(x for contour in P[ch] for x,y in contour)
    shift = 175 - xmin
    def pt(x,y): return (round((x+shift)*scale), round(y*scale))
    return TTGlyphPen(None), pt

def outline(pen,pt,commands,inner=False):
    """Draw one quadratic-outline contour; inner contours wind opposite exterior."""
    target = ReverseContourPen(pen) if inner else pen
    for cmd in commands:
        op,*v=cmd
        if op=='M': target.moveTo(pt(*v))
        elif op=='L': target.lineTo(pt(*v))
        elif op=='Q':
            cx,cy,x,y=v
            target.qCurveTo(pt(cx,cy),pt(x,y))
        elif op=='Z': target.closePath()
        else: raise ValueError(cmd)

def polygon(pen,pt,vertices):
    # Clockwise contour in y-up design coordinates for correct nonzero overlap.
    signed = sum(a[0]*b[1]-b[0]*a[1] for a,b in zip(vertices,vertices[1:]+vertices[:1]))
    if signed > 0: vertices=list(reversed(vertices))
    outline(pen,pt,[('M',*vertices[0])] + [('L',*v) for v in vertices[1:]] + [('Z',)])

def write(ch,pen):
    name=cmap[ord(ch)]
    previous=font['hmtx'][name]
    font['glyf'][name]=pen.glyph()
    assert font['hmtx'][name]==previous

# A: original bases/upper flat cap; diagonal boundaries now true parallels,
# so the perpendicular widths do not flare approaching either baseline.
pen,pt=glyph_pen('A')
polygon(pen,pt,[(282,20),(382,20),(840,655),(1298,20),
                (1398,20),(884,733),(796,733)])
write('A',pen)

# B: replace sharp sideways V with a smooth waist: paired tangent-compatible
# quadratic segments. Counter corners also have deliberately smooth curves.
pen,pt=glyph_pen('B')
outline(pen,pt,[('M',375,733),('L',1132,733),('Q',1288,733,1288,577),
 ('L',1288,504),('Q',1288,418,1240,395),('Q',1210,380,1240,365),
 ('Q',1305,330,1305,251),('L',1305,177),('Q',1305,20,1148,20),
 ('L',375,20),('Z',)])
outline(pen,pt,[('M',468,640),('L',1125,640),('Q',1190,640,1190,575),
 ('L',1190,488),('Q',1190,423,1125,423),('L',468,423),('Z',)],inner=True)
outline(pen,pt,[('M',468,330),('L',1130,330),('Q',1212,330,1212,248),
 ('L',1212,190),('Q',1212,113,1130,113),('L',468,113),('Z',)],inner=True)
write('B',pen)

# G: concentric inside/outside quarter-rounds share the SAME corner centers.
# Outer and inner radii differ by 93 units, matching straight stroke thickness.
pen,pt=glyph_pen('G')
outline(pen,pt,[('M',1290,733),('L',550,733),('Q',375,733,375,558),
 ('L',375,195),('Q',375,20,550,20),('L',1130,20),('Q',1305,20,1305,195),
 ('L',1305,423),('L',933,423),('L',933,330),('L',1212,330),
 ('L',1212,195),('Q',1212,113,1130,113),('L',550,113),
 ('Q',468,113,468,195),('L',468,558),('Q',468,640,550,640),
 ('L',1290,640),('Z',)])
write('G',pen)

# J: matching outside/inside lower-right corner centers eliminates stroke bulge.
pen,pt=glyph_pen('J')
outline(pen,pt,[('M',1165,733),('L',1258,733),('L',1258,180),
 ('Q',1258,20,1098,20),('L',545,20),('Q',422,20,422,143),
 ('L',422,206),('L',514,206),('L',514,155),('Q',514,113,556,113),
 ('L',1098,113),('Q',1165,113,1165,180),('Z',)])
write('J',pen)

# Construct geometric monoline diagonal bands as true parallel offsets.
# Clip at approved top/base heights for horizontal terminal cuts.
def clip_y(poly, y_bound, keep_above):
    result=[]
    for s,e in zip(poly,poly[1:]+poly[:1]):
        inside_s=s[1]>=y_bound if keep_above else s[1]<=y_bound
        inside_e=e[1]>=y_bound if keep_above else e[1]<=y_bound
        if inside_s != inside_e:
            t=(y_bound-s[1])/(e[1]-s[1])
            result.append((s[0]+t*(e[0]-s[0]),float(y_bound)))
        if inside_e: result.append(e)
    return result

def band(pen,pt, start,end,width=93):
    dx,dy=end[0]-start[0],end[1]-start[1]
    length=math.hypot(dx,dy)
    nx,ny=dy/length*width/2, -dx/length*width/2
    verts=[(start[0]+nx,start[1]+ny),(end[0]+nx,end[1]+ny),
           (end[0]-nx,end[1]-ny),(start[0]-nx,start[1]-ny)]
    verts=clip_y(clip_y(verts,20,True),733,False)
    assert len(verts)>=3
    polygon(pen,pt,verts)

# K: each sloping arm is precisely 93 units perpendicular to its centerline.
# Both begin inside the vertical stem; nonzero filling joins them seamlessly.
pen,pt=glyph_pen('K')
polygon(pen,pt,[(406,20),(499,20),(499,733),(406,733)])
band(pen,pt,(463,383),(1260,749),93)
band(pen,pt,(463,383),(1260,-12),93)
write('K',pen)

# Q: retain the APPROVED outer bowl/diagonal descender from P10 unchanged.
# Replace only its narrow counter, expanding the vertical sidewall from 62 to
# 93 units. The new inner quarter-arcs share the exterior's corner centers.
pen,pt=glyph_pen('Q')
record=RecordingPen()
font['glyf'][cmap[ord('Q')]].draw(record,font['glyf'])
assert sum(op=='closePath' for op,args in record.value)==2
for operation,arguments in record.value:
    getattr(pen,operation)(*arguments)
    if operation=='closePath': break
outline(pen,pt,[('M',560,640),('L',1120,640),('Q',1197,640,1197,563),
 ('L',1197,190),('Q',1197,113,1120,113),('L',1082,113),
 ('L',948,268),('L',856,268),('L',856,237),('L',948,113),
 ('L',560,113),('Q',483,113,483,190),('L',483,563),('Q',483,640,560,640),('Z',)],inner=True)
write('Q',pen)

# R: preserve the rounded P10 bowl, but replace the tapered diagonal with an
# even 93-unit thick centerline band; inner counter stays tangent-continuous.
pen,pt=glyph_pen('R')
outline(pen,pt,[('M',375,733),('L',1140,733),('Q',1305,733,1305,568),
 ('L',1305,486),('Q',1305,330,1149,330),('L',468,330),
 ('L',468,20),('L',375,20),('Z',)])
outline(pen,pt,[('M',468,640),('L',1130,640),('Q',1212,640,1212,558),
 ('L',1212,505),('Q',1212,423,1130,423),('L',468,423),('Z',)],inner=True)
band(pen,pt,(1008,376),(1278,-26),93)
write('R',pen)

# S: redrawn lower-right with concentric r=143 / r=50 arcs, leaving a uniform
# 93-unit stroke. Upper shoulder and central passage follow P10 style.
pen,pt=glyph_pen('S')
outline(pen,pt,[('M',1305,733),('L',540,733),('Q',406,733,406,599),
 ('L',406,508),('Q',406,361,553,361),('L',1155,361),
 ('Q',1212,361,1212,304),('L',1212,170),('Q',1212,113,1155,113),
 ('L',375,113),('L',375,20),('L',1155,20),
 ('Q',1305,20,1305,170),('L',1305,320),
 ('Q',1305,454,1171,454),('L',553,454),
 ('Q',499,454,499,508),('L',499,586),
 ('Q',499,640,553,640),('L',1305,640),('Z',)])
write('S',pen)

# U: both lower corners use common centers and outer radius 155 / inner 62,
# exactly 93 units of thickness across vertical, horizontal, and turn.
pen,pt=glyph_pen('U')
outline(pen,pt,[('M',390,733),('L',484,733),('L',484,175),
 ('Q',484,113,546,113),('L',1134,113),('Q',1196,113,1196,175),
 ('L',1196,733),('L',1290,733),('L',1290,175),
 ('Q',1290,20,1135,20),('L',545,20),('Q',390,20,390,175),('Z',)])
write('U',pen)

# Slashed zero: do not use the letter O. Restore the original P10 digit and
# OVERLAY an even-width diagonal within its existing ink bounds. The slash
# intersects the ring at both ends, creating an unambiguous slashed-zero mark.
pen,pt=glyph_pen('0')
source=P['0']
# Reuse P10 numeric geometry/rounding exactly for the outer ring + hole.
outer_area=p10['area'](source[0])
for idx,original in enumerate(source):
    contour=list(original)
    if idx and p10['area'](contour)*outer_area>0: contour.reverse()
    # Digits weren't changed in P10; use P8 smoothing on their original chamfers.
    vertices=list(contour)
    smooth=p10['smooth'](contour)
    target=pen
    target.moveTo(pt(*vertices[0]))
    for i in range(len(vertices)):
        j=(i+1)%len(vertices)
        if i in smooth: target.qCurveTo(pt(*smooth[i]),pt(*vertices[j]))
        elif j: target.lineTo(pt(*vertices[j]))
    target.closePath()
band(pen,pt,(548,80),(1130,553),82)
write('0',pen)

# Digit 3: circular, tangent-continuous right-middle waist replaces the V notch.
pen,pt=glyph_pen('3')
outline(pen,pt,[('M',406,606),('L',1140,606),('Q',1274,606,1274,472),
 ('L',1274,352),('Q',1274,317,1232,285),('Q',1214,271,1232,257),
 ('Q',1274,224,1274,190),('L',1274,153),('Q',1274,20,1141,20),
 ('L',406,20),('L',406,113),('L',1135,113),
 ('Q',1181,113,1181,159),('L',1181,197),('Q',1181,243,1135,243),
 ('L',654,243),('L',654,327),('L',1135,327),
 ('Q',1181,327,1181,373),('L',1181,460),
 ('Q',1181,522,1135,522),('L',406,522),('Z',)])
write('3',pen)

# Digit 8: curved rather than angular mid-waist on BOTH sides; counter lobes
# maintain smooth radius transitions and have opposite winding to outer outline.
pen,pt=glyph_pen('8')
outline(pen,pt,[('M',500,606),('L',1180,606),('Q',1290,606,1290,496),
 ('L',1290,369),('Q',1290,313,1235,286),('Q',1212,271,1235,256),
 ('Q',1290,227,1290,177),('L',1290,125),('Q',1290,20,1185,20),
 ('L',495,20),('Q',390,20,390,125),('L',390,177),
 ('Q',390,223,445,257),('Q',470,271,445,285),('Q',390,319,390,370),
 ('L',390,501),('Q',390,606,500,606),('Z',)])
outline(pen,pt,[('M',530,522),('L',1145,522),('Q',1196,522,1196,471),
 ('L',1196,378),('Q',1196,327,1145,327),('L',530,327),
 ('Q',484,327,484,373),('L',484,476),('Q',484,522,530,522),('Z',)],inner=True)
outline(pen,pt,[('M',530,243),('L',1145,243),('Q',1196,243,1196,192),
 ('L',1196,155),('Q',1196,104,1145,104),('L',530,104),
 ('Q',484,104,484,150),('L',484,197),('Q',484,243,530,243),('Z',)],inner=True)
write('8',pen)

family='OrbitX Header Font P11'
names={1:family,2:'Regular',3:'OrbitX-Header-P11-Regular-20260920',
       4:family,6:'OrbitXHeaderFontP11-Regular',16:family,17:'Regular'}
for rec in font['name'].names:
    if rec.nameID in names:
        s=names[rec.nameID]
        rec.string=s.encode('utf_16_be') if rec.isUnicode() else s.encode('latin-1')
font['head'].fontRevision=1.1
out=HERE/'OrbitX_Header_Font_Prototype_11.ttf'
font.save(out)
font.close()
check=TTFont(out)
assert check['name'].getDebugName(1)==family
assert all(ord(ch) in check.getBestCmap() for ch in 'ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789')
assert check['OS/2'].fsType==0
check.close()
print('Built',out.name)
print('Word compatibility and final glyph design require user review on Windows.')