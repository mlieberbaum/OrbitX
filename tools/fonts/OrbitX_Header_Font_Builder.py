# OrbitX Header Font — reference-traced prototype 4 (font source generator)
# Requires: Python 3.9+ and fontTools:  py -m pip install fonttools
# Run: py OrbitX_Header_Font_Builder.py
# Outputs actual scalable vector OpenType and TrueType fonts beside this file.
from pathlib import Path
from fontTools.fontBuilder import FontBuilder
from fontTools.pens.ttGlyphPen import TTGlyphPen
from fontTools.pens.t2CharStringPen import T2CharStringPen
from fontTools.ttLib import TTFont
import zlib, json, base64, string

PACKED = (
    'c-rk;SGyfYvHdF`G8{p7Pe&2&CFdMv0YWmEWDE`jBYr!xs%lmD>@)jF_V@9^y-!}+J=0UUR`tyB+xK33{KhM<-+S=(z5luQ'
    ';NioEsi&I~AKeH@VFx+X31lhPeiP%iqq2pYw6z`8wB2l5AgZ{o#d?!V25SALw7#RB!%YoiM=_*&vyJ3X?uoI-o7f6qYFVEg'
    'x6uuS(rizK9ar`iZ`uZddKiXkFZQ5Pqde!)qnmsGD@tyy*i}HfF`lakkL@DX7RpV`nK0EZ>oHMSfvN)TGQj|A4!ocRxgeHc'
    '4;_P#A2r)@8N3kUVs(cP^VV+C7-n;|uWV!x+b>e3P<es+XL<@dK^5+*rop;NiklXp)msA9#&SVU+||&g-hgR8ucjUJFOV89'
    '_pE_aa;1&;#fdDK_Ei(MBnvB`605CJzT#5jZZ%~L(e2xV*|G(q+PGR;CEB;tRY(W~J-#qQMAm>#Y?L<>OrT1;T7Hbx2WkiS'
    'Pyz>gPV~Y7Xo(~7)#61^Z5vTwC?V|(<z}nMVhOKZ)i1iL>P{dNb-zkPfF4t-Q~B=H*P?e{W%TZ9F44PWJ!C+59<y_?H{nzt'
    'kji|?<4=oJV*ToQ0U}O`*~O`Kg%;L()EU}$(X0m2RL3JAw#Ygd5m>jRN{m?zd?KqxG2a=-xZAWJ!GrS4q8z-Unu}#L&8E-x'
    'WK`ANX9y3gpvOp3?GQ5)sVh*DPF#f;UkZ(eJD}C-8Tsf5OC-oLkX96|u71oeou#q3RAN@Dx}xmKy9IanSKQ&2sH`bcJ{)KO'
    '_NmoykEg@{6{s<d^%8ygq+C(b!t9bO`Rfj?4`3$?qy6zqXlX!|<f?pCRB3~%1BnbhS@8B=Cty^3O>SESK@2aD&I$Q;jon2u'
    '=SYH7&lw`SBQou=OY%%H6BTtar1`qX=&UsAhH7?1V9p5DRNUCiz!nstiED(x@rn?j8ovloi{#zHm&FP9Y9I?53?y=f-o3S1'
    '?YP#Zg_LNv36T-^f!ruU!`AIZg~4KJZr+i#mOU%!uWzV@Iv*@P5G&WfP(#p6G*`_Vd-GD|rHv_J$llQ6Y-8Wl*w~7zwxzHJ'
    'sX=(v`U;@2v{EhIB>)Arn&_#@x?Q(!^Z;m<I!nmZ7nH)GDx*kdZ}IX}M7@S2g4|*kZQXJd@-3;Qw~H=>rF&B@G$_=b2o8mL'
    'zNN*Zdhw{?L_#a*yM$KcJ4;sE=zg(=##v&K5v!TdTRs(pm18L)rF+7<>*fAi9*_B=TR^36dpCcP&hW0;hUG~do+qWPDRZ5A'
    'kl9)p*Yv1*o$%7LF7aGmiq=Hpz0mO&YfZ4*cie93cW`c&Ew$SIOHW5SjgqDe6(-frC^oewiGHa808^S!=T@E7XQ>*sFC6F='
    '8@j_NY4aK3KsBN1cPB-cn>Xj)%Cqk%_xE#R`!hHT)uiwODVS&f(${KQx%Z)HSx00n6xN4@_#+pV-IoHu$!B9hjiIrFjoewx'
    'W?2K$#*#@_N9^a5$i6qM_q~(Z@ycLef*~?(crt!_bHTPRBJO0|6K1BU<~7nvJm#@XsE%|M?T)v^ap2usIC4P^&Zz_QyvMWF'
    'c$V@^W$SrXmA-F%$Qj;pS)gZuX5+k@egc}eaa0g}aEVt3dYRRZ^NObB+}$#C`U$mS8F%}28apQwQPTSF4=%>YsVqvPB2W~q'
    '2SRy*aTN!;hI#IsGFt&z*>m1*lbXA0@mbMXhr}PA(Cq6)<62R8E@!@#!PltjQY%ctA6+D-v^#ZEjSF!z9<$p(_k~?VSXekQ'
    '#k!wJ*_0zmWX(K|L!`{MV<~^>m5?s^<$fExX&lB@a3EyU*Di@xBT!!xa~D`e0q3-$xbc|ff@x0oSseaYj5B<2%q07;S5IsC'
    'fwdANbUC;a86l4v6*3yjIw9bCR;RfNXgQWgWGw_Va~1ce{7RxaytV_f&&TAP2p!$%;dBn*wBlLlPd0DVa~@jYcuaOoMiNCj'
    'Kc;hY{=~{zXCu2s<~H^mvk?fasSxtCT*wYFO<4!G_K6OdMqdsBmP3Xd4y;p{sPgi_SxUwU4VlI`mMqGgJQXFLtNI~{j2f~A'
    'MsJ}w*(@|5t*W=@<c){M@*AhLQ5}L)SXQ;PbYL6V631kyXK3TVEWhcn(HM&|b_iO=jylgxeyXLxOU*RkB=smX8V@j_KI?+Y'
    '-<rTJs$mZLw#}1LWSi2wb^I!4ArprW><AQ<0|%Ijz*Ec)!F6`+IY~m(pPdXe)+!(xiOQVz@ayt8iN$5I3(PAyv=v85<rn8;'
    'bBm>9J-{PqIV^#m79#s^-KWs&Q?1xmj|9cG$!Annfe{YL(&9y1K?5NS9FyQgMGF_*(t1ifkKD|lVG@@-@&(mK1tN@S;w<Ba'
    'Dktel8HD&%R^C?E6Oy%j23h1hq<5g)m*j(&LE-zgdWUgRlu1!CMc7RtN<~phxkcs4hG-{f<0~5dO%6`v0ZvPVgocBPV5rqN'
    'a?%qH8J&5KG-H)2Y!l*$OjVF`xl(*)_H#Q$5`4X_=X~q2T<NBD%%(MRgXa-?qeODvoTp@GCtGTS^mX!sqNP5|0Ai|cs;YAh'
    'c|CT^N8*m^o}q$~=U=tt6`6EcHVBT}n^SRr!ISw+2b1j1n96?Cm7-6}RD8~;xPUgmU1kID<`P^ZwE^0!Iu{Zf+SwJBVV69|'
    '>EXu^q5*`A)nDTo#MTJUBb|t8LADIh>?R@d6RyfdAGk=`N?0j^ULbES1<3%Px8tcKUKxr#5cunx%W71@N->TDHA9EyA}mnm'
    'n+VN(k1!c0v!ClM!AKvY9XKmc*WwjKUr(l%!!4oK)%o_^WIcVFr)TD4bQ*;y{ko3nSCQ1<#O|{+fq9Z%NX9i%kaVm`$eZMB'
    'h^0F9r64easuv5r19q|sNwu!&oV^1unCDSGEVrnys-W2@x~fX$YsM?8l=;#o_#4S!rMff2WEL{N)?>Y~AWhxloj$&-TCh&@'
    '<MdngMNYA2sj+kwC>30aw0afDS+CNU9M%Fc<}2&quQRQ%@BPeB<+8pzRtfv`U*U0*?k5qimYuI)fOG~K%)K)|AFKPcS!(g5'
    'T(7>;L*&hmlLHeL&NrX6y7nWKEc4#&7BRgiF6-^8_i~-ylTK@UsyCL9S~{P+C)zwaw(bu#?bdv4$t##2ha+`oL|c79M0e#c'
    '<8JTx^(P%M^}B-gxdmw^=L<Dc^~%ht)hoA>Qtn^4!|)M<W~<k07R>UV-7Ncm-m63i)+~D5vZB@nMMV$>ICI&-?j*Yo2;0AO'
    'Pd2UyO&!#7Sx^r$WPuF_-FR%ga`dtwy%MW(<-Ya%Waom%%2jns3u-#TL-?dS$B=mGUNO=Nn|(}^=G8gjJWQRw9pR8fPKtPK'
    'nHQ|kuLXkC@BhDe{)d3s@qL{j>x#O)rc?wAeL(#hQEbJVWkpRw468uu*S&%d({V+KL#aYsTM&KvRhEGm@iN_5SXBrqKKS4C'
    'Ey#yFyEUO;_=z;Nu*%f>upW}-VD8z=EIrkBl!O0xUlTM3Llt3SyL?1b-<{l+{YtQ1+Pt;;cD48>A63y2ovB!vnW%O~Pqo5S'
    'cOT~F@-0x)66-+N`(wU$3SRUa)#`g!+Z#M_=<!A^E6j2&*0aUWjqZ&%Y&suCA`p)IRS&I-AY6Z6jfHpT-A%N>EJqV!-W}nN'
    'kGu3%e-~hdI213$pRjlBRt`!FenefYzVD353jBWk1K&M%-_?<|hM<V~T?*GHs-EA~^81s%^9x*mqc+s%joF?^+F(N4_fuL~'
    'k;JhUrnbU8I`$m$e6My`S_x~=b?ozL^~Bx_w&<6wQ1V;WyVf?~lDvehus6FF2)lnqtJ{<;xwyfFtcik^McPHi&$^6kUHJ$*'
    'w_(H9+q0d`9U$)uv-|ZW(#rvAq<2R8&5K9i=l=hF{5yXi5Bu&tD`{O!E85h;SrIJ!P@`h+l9C!}{#VN`R=RmudZ}hrThaNI'
    'T=na{9UIMi$;*p|UUAka1txiYu>gLg9mcla9kT!xVJsrFKoRAF1zKkb$7^rz*?5@={Bt#x9PMdaBkF}2F$RB=T()IbvB%=o'
    '@)w%bd%oQ_rRx!D(M^hQ$1n90x8l1cB2>8=S>>wv_HApXyBtwJxLJ|*L9BWoL|hJyl@%$~!D_3sW5&##Ceq#cMDE#qBLd^x'
    'MG<8Er&W0NIds#Bx>W{6xc-^FljAdE*bY#&8;f?WQ70*P3sn8EPW?umw0}E5eZ*Kal>6phj?5};>i949xc6)6AlB81#_Z-X'
    'OxU?DaN{?LJM<YxQLF3jii)j5hR2$8_)t)o<PPl2B}^boWv_u<NB?YXyh<C7L`@F8F*!?PXk)Kha;qGFSN)D0XNaAs=@*0`'
    'xAy5=v&Y7SZxXF(728o^l`U~rUgSg#?;Zs;Owm<eO?P<=H2js7m1b7~*~WNo;bW^!z}r5Z55i=FI$m|R2}kE$CKzDul^2vv'
    '6>Ai2$Z3~;b(w9s;$6R@q^mMs;CNBbAa;W%`{)F(9PR6rbdV~R#tY=}?j`I4MYyLLrpO<7RNk_Ptez667M2UP6zwiUIQ`2?'
    'eV?dd@jAKcjMB|?r4Egm8foi@Y1xs+QKYDuybwsc8x|>@yojj0*UY#-!Vpd}#Ei{N+hVlUy74;fsH+eU2ueJi(XnNVjuQiA'
    '|GOH%Xq?)OzQ?j#-%J4KHSRdk3rC*XRMtxj7t!SEbr&4jX2?y0+?BZ66@A$iRZogA_W8Vt05zsmXY$>ttp+X9+JY8LYzf+#'
    '?2tf@qt>KcY)v?o7ig;d+5=AuQ?k&4Umch@Be^(uQ8A&#GXhQ#{un`$8Avl6kAT=B>tGZXFEhpefI%ak$X4<G;*Jn5i3oc$'
    'Ab(>=7t~xVqxm&GhAbQhk0ChBW5+mB?GP~&sVh*4PFy8_P&Dg{H5@=Az>IqIff)yzmlw@^dB0pQYa@bGE0#D^;#Erd#t(EX'
    'SwnVrzvAw;6h)mWtQ!R8?>ZHU9*J?|x<HF(m}Ky`vnoYN3!f}k^4HC~Qgq`=7|g)R6ppI=cFwJ;uzbiW)Xj8~xrcx;@jD$$'
    '7eVB8#e{TCj+dpivbAb*G|v~J&R1;~N7*B)ciN}u6Cld(XLx#vQ6Cg5Ziq<_&Xnj)bp_agBD8Hg3;;PY+8Ql%hjlNRz0ze>'
    '!o3>9f+SZFMPv!`9$>6x<7!`)uRa5LOCpysR9Fc_13{vmx)}FrL$SsK1N}h_G;8pV)F)185HAz?qJnLh;?lX49z>Q&`C82$'
    '^hf(kmBh<p3Ww-42#;Db5vcJdAK_+;JVnh*cXMu5<>Qe-<Q2{`vBU+%awx{Al9BD*o{H&PzaD8s!)vvOx1?0IZ@G3=2ut@i'
    'TsnzjC<`h!54@^HqcPJm^Xe#|7HSCS>7uBWZVqowS;bWLH$>fquG_gFRxy_1Q9M!I?|S=%zs1Xk76Fa^<PH2q+QGXf7?vT$'
    '%U|%Q8XoXxWP>EuinnS~l%5C)J?jn6C85#Bi39U~k#+>5z2-)<PZaX3RvT@n<j09KZdW!c2T0CZ*cDO2teK1{(Wyme%~^^?'
    'SxL_=`g4ax(wqej%o2+Jd6IFt8OX`CYI>!{-`^4&9^tG{<m!Sb*0#1U3xgh4ntdo%)&dy@nd!Vxd}P70`;z&bZ|cT`iRSN-'
    'yo8k%St~)B>v!N|oXs1SRew2|4$t{Nh04_jq%jsUz0m4osGSUS!?}r&uo+|9xX9kIEyz~AVoRE?6x*QFJ`KyM1G9UK+pX|4'
    '(V4*3<IHHU8y|5tw_FxU_T=Ee8|B?Z6El?wL2tp34sI2-T**y1m1r_9TWelUA60qz@8fJ8SNZBPfLNV_(r;W$ic{0(Mg|?x'
    'D$se5(%JR_Y;75KF1WFkjfE!XdYi!9U4G9>%{mFbdBUf!m5OVn;JMWJGH<Ui(xpC_bZ=cGq?}!-i&9@0nsFAgtHwm>441*m'
    'KID79;HhJM+qs>_$m}sp^b3D%`I1G-pWX&;ro;pXJw{EvKZD?^kkDOE&I;L)1fJ56BcGgpvjF_7{@+Ae;bd5O`~z!g6Jyb-'
    'oF`zHc^qedREgs_C1j{u_WujHO|#_;&QesMz47082q)0Yl_%%fyg|rqb^#!pdh%+Hv6U}Yrf<?}#M78}&Tbsj&m>?0eY@?K'
    'Z9ZtHWvKca|I*u|`I5G2E%X^5OjQ*aBfHshblB(EVck-{?C8swzZ@ZGNI1I`#;Nq%2XmPjG+;J{jwF!sKJ+pB|0DE^BM`YW'
    'khohMx`m+t%|6FGAA{43g{?Se^x;eigk_YEang?)M+!~U=ED-SvpB?WVn&?OS!(xcXc;aRU;SN+LWv76&R(;!R~QY3ws90Z'
    'GWOd8w)NoQkqLDeq|xW5b;e5LEL<W}7eS&@3=dAQ60?FAv#C2rMd<mDlc^<7-b1)dbg?dvv%g-M(_1C=kb~GBczU8=4K?~!'
    '&#~6<Sx!O{&nV<K9VPJFwYjzXs{pa9k=7jDuE>qKAxrFXwozw^&3@47S+tN&zo&V@l(uHpeCf>)9tZt6?WH$Uh}%2<KtgWI'
    '@9wt#4!@X@o5s7j>B)cW13B6L>3?+lRNv7@DElobrg!0y5Pqe9f6^e7d@eEY2SASW6(LsT72%_2H}@Vtee3b7PhRH#%q!v>'
    '5d06X6nviG3j|*z_!7aF3BE$`Rf4Y(e4XGM1m7h17Qwd(zC-X`g6|Q0pWp`sKP31O!H)@kLhw_9pAiItNRS9Nf=o~dDnTRY'
    '1cTr`_a4Z9-^(BHZKtC55xk$^0|Xx=_z=N|2|hybQG$;V+z{L+_&C8Q2p$l8lHgMWpC<SW!Dk6RNAS?qc!}r{!OsbPLGVk0'
    'w+a49@QmPJ1dn&HwJxtO6+9t$O7JqluL*ub@Cw0i34TZLdxAd@{E^^Qf<F<wM(}5XzYx4m@CLz~1aA@imEdm#e<%0{0jsnc'
    '(#mw4|JoyM!ym_aQC-%?;a^V<|9ZOqE7j#+Qa|`FEem`8KYR9X>iR`C'
)
DAT=json.loads(zlib.decompress(base64.b85decode(PACKED)).decode())
paths=DAT['paths']; advance=DAT['advances']
order=['.notdef']+[f'uni{ord(c):04X}' for c in paths]
cmap={ord(c):f'uni{ord(c):04X}' for c in paths}
metrics={'.notdef':(1680,0)}
metrics.update({f'uni{ord(c):04X}':(advance[c],0) for c in paths})
notdef=[[(650,20),(1030,20),(1030,760),(650,760)],[(725,685),(955,685),(955,95),(725,95)]]
family='OrbitX Header Font P4'
names={'familyName':family,'styleName':'Regular','uniqueFontIdentifier':'OrbitX Header Font Reference Outline Prototype 4 2026','fullName':family,'psName':'OrbitXHeaderFontP4-Regular','version':'Version 0.4'}

def common(fb):
    fb.setupGlyphOrder(order);fb.setupCharacterMap(cmap);fb.setupHorizontalMetrics(metrics)
    fb.setupHorizontalHeader(ascent=880,descent=-220,lineGap=100)
    fb.setupOS2(sTypoAscender=880,sTypoDescender=-220,sTypoLineGap=100,usWinAscent=880,usWinDescent=220,
                usWeightClass=400,usWidthClass=7,fsSelection=0x40,sCapHeight=780,sxHeight=590)
    fb.setupNameTable(names);fb.setupPost()

def write_path(p,contours):
    for contour in contours:
        if len(contour)<3: continue
        p.moveTo(tuple(contour[0]))
        for xy in contour[1:]:p.lineTo(tuple(xy))
        p.closePath()

out=Path(__file__).resolve().parent
fb=FontBuilder(1000,isTTF=True);common(fb)
glyphs={}
for name in order:
    pen=TTGlyphPen(None)
    write_path(pen,notdef if name=='.notdef' else paths[chr(int(name[3:],16))])
    glyphs[name]=pen.glyph()
fb.setupGlyf(glyphs);fb.setupMaxp()
ttf=out/'OrbitX_Header_Font_Prototype_4.ttf';fb.save(ttf)

fb=FontBuilder(1000,isTTF=False);common(fb)
strings={}
for name in order:
    pen=T2CharStringPen(None,None)
    write_path(pen,notdef if name=='.notdef' else paths[chr(int(name[3:],16))])
    strings[name]=pen.getCharString()
fb.setupCFF('OrbitXHeaderFontP4-Regular',{'FullName':family,'FamilyName':family,'Weight':'Regular','version':'0.4'},strings,{})
otf=out/'OrbitX_Header_Font_Prototype_4.otf';fb.save(otf)
for target in (ttf,otf):
    font=TTFont(target)
    assert all(ord(ch) in font.getBestCmap() for ch in string.ascii_letters+string.digits+string.punctuation+' ')
    font.close()
    print('Created:',target)
print('Install only one format (try TTF first on Windows).')