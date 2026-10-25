#!/usr/bin/env python3
"""Generates LumenPresets.hpp from readable recipes.
   python3 make_presets.py [trims.txt]   (trims.txt: one "index trim_dB" per line, from the level-matching test)"""
import re, sys, os

HERE = os.path.dirname(os.path.abspath(__file__))
PARAMS = open(os.path.join(HERE, '..', 'LumenParams.hpp')).read()
SYM = {}
for m in re.finditer(r'X\((k\w+),\s*"[^"]*",\s*"(\w+)"', PARAMS):
    SYM[m.group(2)] = m.group(1)

P = []   # presets

def preset(cat, name, desc, vals, macros, mdef=(0.5, 0.5, 0.5, 0.5)):
    for k in vals: assert k in SYM, k
    for lab, tg in macros:
        for t in tg: assert t[0] in SYM, t[0]
        assert len(tg) <= 4
    assert len(macros) == 4, name
    on = {k.split('_')[0] for k, v in vals.items() if k.endswith('_on') and v}
    for lab, tg in macros:
        for t in tg:
            mod_ = t[0].split('_')[0]
            assert mod_ in on, '%s: macro %s targets %s but that module is off' % (name, lab, t[0])
            assert t[1] != t[2], '%s: macro %s does nothing' % (name, lab)
    P.append(dict(cat=cat, name=name, desc=desc, vals=vals, macros=macros, mdef=mdef))

# ---- reusable macro pieces
SPACE  = ('SPACE',  [('r_mix', 0.05, 0.6), ('r_decay', 1.2, 8.0)])
SPACEY = ('SPACE',  [('r_mix', 0.05, 0.55), ('y_mix', 0.0, 0.35)])
TONE   = ('TONE',   [('f_cut', 700, 20000)])
ECHO   = ('ECHO',   [('y_mix', 0.0, 0.5), ('y_fb', 0.2, 0.75)])
WIDTH  = ('WIDTH',  [('m_mix', 0.0, 0.7)])
GLOW   = ('GLOW',   [('r_shim', 0.0, 0.7)])

def lp(cut=20000, res=0.15):
    return {'f_on': 1, 'f_type': 0, 'f_cut': cut, 'f_res': res}
def rev(mix=0.3, size=0.6, decay=3.0, damp=0.4, shim=0.0, mod=0.3, pre=20):
    return {'r_on': 1, 'r_mix': mix, 'r_size': size, 'r_decay': decay, 'r_damp': damp, 'r_shim': shim, 'r_mod': mod, 'r_pre': pre}
def dly(div=4, fb=0.4, mix=0.25, ping=1, tone=0.6, mod=0.1):
    return {'y_on': 1, 'y_div': div, 'y_fb': fb, 'y_mix': mix, 'y_ping': ping, 'y_tone': tone, 'y_mod': mod}
def grn(mix=0.7, size=120, dens=16, pitch=0, spread=0.3, rev_=0.0, jit=0.1, freeze=0):
    return {'g_on': 1, 'g_mix': mix, 'g_size': size, 'g_dens': dens, 'g_pitch': pitch, 'g_spread': spread, 'g_rev': rev_, 'g_jit': jit, 'g_freeze': freeze}
def mod(t=0, rate=0.6, depth=0.5, fb=0.3, mix=0.5):
    return {'m_on': 1, 'm_type': t, 'm_rate': rate, 'm_depth': depth, 'm_fb': fb, 'm_mix': mix}
def pit(a=12, b=-12, ma=0.6, mb=0.0, dry=1.0):
    return {'p_on': 1, 'p_a': a, 'p_b': b, 'p_mix_a': ma, 'p_mix_b': mb, 'p_dry': dry}
def drv(mode=0, amt=0.4, tone=0.6, mix=1.0):
    return {'d_on': 1, 'd_mode': mode, 'd_amt': amt, 'd_tone': tone, 'd_mix': mix}
def lofi(bits=16, rate=48000, wow=0, flut=0, noise=0, crackle=0, age=0):
    return {'l_on': 1, 'l_bits': bits, 'l_rate': rate, 'l_wow': wow, 'l_flutter': flut, 'l_noise': noise, 'l_crackle': crackle, 'l_age': age}
def stut(mode=0, div=3, amt=0.5, pat=0, mix=1.0):
    return {'s_on': 1, 's_mode': mode, 's_div': div, 's_amt': amt, 's_pat': pat, 's_mix': mix}
def filt(t=0, cut=2000, res=0.3, rate=0.5, depth=0.0, env=0.0):
    return {'f_on': 1, 'f_type': t, 'f_cut': cut, 'f_res': res, 'f_rate': rate, 'f_depth': depth, 'f_env': env}
def M(*d):
    out = {}
    for x in d: out.update(x)
    return out

# =============================================================================================
# 0 GRANULAR
C = 0
preset(C, 'Start Here', 'A soft grain cloud in a warm room: the sample, but dreamier.',
       M(grn(0.55, 140, 18, 0, 0.25, 0.1, 0.12), rev(0.3, 0.6, 3.0), lp(14000)),
       [('GRAINS', [('g_mix', 0.0, 1.0)]), ('SIZE', [('g_size', 40, 400)]), SPACE, TONE])
preset(C, 'Cloud Nine', 'Long overlapping grains spread wide, floating above the sample.',
       M(grn(0.85, 320, 22, 0, 0.6, 0.2, 0.2), rev(0.45, 0.8, 6.0, 0.5)),
       [('CLOUD', [('g_mix', 0.2, 1.0), ('g_dens', 6, 40)]), ('SCATTER', [('g_spread', 0.0, 1.0)]), SPACE, ('DRIFT', [('g_jit', 0.0, 0.8)])])
preset(C, 'Frozen Moment', 'Freezes the sound into an endless shimmering hold.',
       M(grn(1.0, 260, 30, 0, 0.15, 0.3, 0.25, 1), rev(0.4, 0.75, 7.0, 0.5)),
       [('HOLD MIX', [('g_mix', 0.3, 1.0)]), ('SIZE', [('g_size', 60, 600)]), ('MOTION', [('g_jit', 0.0, 0.9), ('g_spread', 0.0, 0.5)]), SPACE], (0.9, 0.5, 0.4, 0.5))
preset(C, 'Grain Storm', 'Dense, chaotic short grains: a swarm of tiny pieces.',
       M(grn(0.9, 35, 70, 0, 0.7, 0.35, 0.6), rev(0.25, 0.5, 2.0)),
       [('DENSITY', [('g_dens', 10, 80)]), ('CHAOS', [('g_jit', 0.1, 1.0), ('g_rev', 0.0, 0.6)]), ('SIZE', [('g_size', 15, 120)]), SPACE])
preset(C, 'Reverse Rain', 'Backwards grains falling around the sample.',
       M(grn(0.75, 180, 20, 0, 0.5, 0.9, 0.15), rev(0.35, 0.7, 4.0), dly(5, 0.35, 0.2)),
       [('REVERSE', [('g_rev', 0.0, 1.0)]), ('RAIN', [('g_dens', 4, 40)]), SPACEY, ('SIZE', [('g_size', 60, 500)])])
preset(C, 'Octave Dust', 'Grains an octave up sprinkle sparkle over the sound.',
       M(grn(0.55, 90, 24, 12, 0.4, 0.1, 0.2), rev(0.35, 0.7, 4.5, 0.3)),
       [('DUST', [('g_mix', 0.0, 0.9)]), ('PITCH', [('g_pitch', 0, 24)]), ('SPARKLE', [('g_jit', 0.0, 0.7)]), SPACE])
preset(C, 'Sub Grains', 'Grains an octave down for a dark, heavy undertow.',
       M(grn(0.6, 220, 14, -12, 0.25, 0.0, 0.1), lp(6000, 0.2), rev(0.2, 0.5, 2.5)),
       [('WEIGHT', [('g_mix', 0.0, 0.9)]), ('PITCH', [('g_pitch', -24, 0)]), TONE, SPACE])
preset(C, 'Stretch', 'Slow grains smear the sample like it is being time-stretched.',
       M(grn(0.95, 400, 10, 0, 0.08, 0.0, 0.05), rev(0.25, 0.6, 3.0)),
       [('STRETCH', [('g_size', 120, 800)]), ('SMEAR', [('g_spread', 0.0, 0.5)]), ('BLEND', [('g_mix', 0.4, 1.0)]), SPACE])
preset(C, 'Microscope', 'Tiny grains zoom into the texture of the sound.',
       M(grn(0.9, 18, 60, 0, 0.1, 0.2, 0.3), filt(2, 1100, 0.15), rev(0.2, 0.4, 1.5)),
       [('ZOOM', [('g_size', 10, 80)]), ('FOCUS', [('f_cut', 300, 5000)]), ('DENSITY', [('g_dens', 15, 80)]), SPACE])
preset(C, 'Glass Swarm', 'Bright, pitched grains in fifths and octaves.',
       M(grn(0.7, 70, 35, 7, 0.55, 0.15, 0.45), rev(0.4, 0.75, 5.0, 0.2), mod(0, 0.4, 0.4, 0.2, 0.3)),
       [('SWARM', [('g_mix', 0.0, 1.0)]), ('PITCH', [('g_pitch', 0, 19)]), ('CHAOS', [('g_jit', 0.0, 1.0)]), SPACE])
preset(C, 'Grain Delay', 'Grains fed into a tempo-synced echo.',
       M(grn(0.6, 110, 18, 0, 0.35, 0.2, 0.2), dly(4, 0.55, 0.35)),
       [('GRAINS', [('g_mix', 0.0, 1.0)]), ECHO, ('TIME', [('y_div', 1, 7)]), ('PITCH', [('g_pitch', -12, 12)])])
preset(C, 'Particle Pad', 'Turns any sound into a soft, steady pad.',
       M(grn(1.0, 500, 16, 0, 0.35, 0.1, 0.2), lp(8000, 0.1), rev(0.5, 0.85, 8.0, 0.5)),
       [('PAD', [('g_size', 150, 800)]), ('TONE', [('f_cut', 1500, 16000)]), SPACE, ('MOVE', [('g_spread', 0.0, 0.9)])])
preset(C, 'Stutter Grains', 'Short regular grains give a fluttering, rhythmic tremble.',
       M(grn(0.85, 45, 12, 0, 0.0, 0.0, 0.0), rev(0.2, 0.5, 2.0)),
       [('FLUTTER', [('g_dens', 4, 30)]), ('LENGTH', [('g_size', 20, 120)]), ('BLEND', [('g_mix', 0.3, 1.0)]), SPACE])
preset(C, 'Ghost Choir', 'Wide, slow grains with gentle pitch drift, like distant voices.',
       M(grn(0.8, 350, 20, 0, 0.5, 0.25, 0.35), pit(12, -12, 0.2, 0.15, 1.0), rev(0.55, 0.9, 9.0, 0.6, 0.15)),
       [('CHOIR', [('g_mix', 0.2, 1.0)]), ('HEAVEN', [('r_shim', 0.0, 0.6)]), ('DRIFT', [('g_jit', 0.0, 0.8)]), SPACE])
preset(C, 'Granular Ocean', 'Waves of grains rolling in and out.',
       M(grn(0.9, 600, 8, 0, 0.9, 0.4, 0.3), filt(0, 3000, 0.2, 0.08, 0.6), rev(0.45, 0.85, 7.0)),
       [('WAVES', [('f_depth', 0.0, 1.0)]), ('SPEED', [('f_rate', 0.03, 0.6)]), ('DEPTH', [('g_spread', 0.2, 1.0)]), SPACE])

# =============================================================================================
# 1 SHIMMER & SPACE
C = 1
preset(C, 'Shimmer Hall', 'A huge hall whose tail rises an octave: angelic.',
       M(rev(0.45, 0.85, 7.0, 0.35, 0.55, 0.4)),
       [GLOW, ('SIZE', [('r_size', 0.3, 1.0)]), ('DECAY', [('r_decay', 2.0, 20.0)]), ('MIX', [('r_mix', 0.1, 0.8)])])
preset(C, 'Cathedral', 'Long stone reverb, dark and enormous.',
       M(rev(0.5, 1.0, 12.0, 0.6, 0.0, 0.25, 60)),
       [('SIZE', [('r_size', 0.4, 1.0)]), ('DECAY', [('r_decay', 3.0, 25.0)]), ('DARK', [('r_damp', 0.1, 0.9)]), ('MIX', [('r_mix', 0.1, 0.8)])])
preset(C, 'Small Room', 'A natural little room to glue the sound.',
       M(rev(0.25, 0.25, 0.9, 0.5, 0.0, 0.1, 8)),
       [('ROOM', [('r_size', 0.05, 0.5)]), ('MIX', [('r_mix', 0.05, 0.5)]), ('DARK', [('r_damp', 0.1, 0.9)]), ('DECAY', [('r_decay', 0.4, 2.5)])])
preset(C, 'Infinite', 'A reverb that never ends: wash the sample into eternity.',
       M(rev(0.6, 0.95, 30.0, 0.45, 0.2, 0.5)),
       [('WASH', [('r_mix', 0.2, 0.95)]), GLOW, ('DARK', [('r_damp', 0.0, 0.9)]), ('MOTION', [('r_mod', 0.0, 1.0)])])
preset(C, 'Space Echo', 'Ping-pong echoes melting into a big space.',
       M(dly(4, 0.55, 0.35, 1, 0.45, 0.2), rev(0.35, 0.7, 4.0)),
       [ECHO, ('TIME', [('y_div', 1, 7)]), ('SPACE', [('r_mix', 0.0, 0.6)]), ('WARMTH', [('y_tone', 0.9, 0.1)])])
preset(C, 'Ethereal', 'Shimmer, soft chorus and air: floating and pure.',
       M(rev(0.5, 0.9, 8.0, 0.3, 0.4, 0.5), mod(0, 0.3, 0.5, 0.2, 0.4), lp(16000, 0.1)),
       [GLOW, WIDTH, ('SIZE', [('r_size', 0.4, 1.0), ('r_decay', 3, 14)]), ('AIR', [('f_cut', 3000, 20000)])])
preset(C, 'Deep Blue', 'Dark underwater space with slow motion.',
       M(lp(1800, 0.3), rev(0.55, 0.9, 9.0, 0.8, 0.0, 0.8), mod(0, 0.15, 0.7, 0.2, 0.4)),
       [('DEPTH', [('f_cut', 400, 6000)]), ('SPACE', [('r_mix', 0.2, 0.8)]), ('CURRENT', [('r_mod', 0.0, 1.0), ('m_depth', 0.1, 1.0)]), ('DECAY', [('r_decay', 2.0, 20.0)])])
preset(C, 'Heaven Gate', 'Octave-up voices into a shimmering hall.',
       M(pit(12, 24, 0.35, 0.15, 1.0), rev(0.5, 0.9, 9.0, 0.3, 0.5)),
       [('VOICES', [('p_mix_a', 0.0, 0.8), ('p_mix_b', 0.0, 0.4)]), GLOW, ('MIX', [('r_mix', 0.1, 0.8)]), ('SIZE', [('r_size', 0.3, 1.0)])])
preset(C, 'Dub Space', 'Dubby echoes with warm, saturated repeats.',
       M(dly(6, 0.65, 0.4, 0, 0.3, 0.25), filt(1, 150, 0.1), rev(0.25, 0.6, 3.0)),
       [ECHO, ('TIME', [('y_div', 3, 8)]), ('WARMTH', [('y_tone', 0.8, 0.05)]), ('SPACE', [('r_mix', 0.0, 0.6)])])
preset(C, 'Wide Plate', 'A bright, dense plate that makes everything bigger.',
       M(rev(0.35, 0.45, 2.5, 0.15, 0.0, 0.2, 5), mod(0, 0.5, 0.3, 0.1, 0.25)),
       [('MIX', [('r_mix', 0.05, 0.7)]), ('DECAY', [('r_decay', 0.6, 6.0)]), ('BRIGHT', [('r_damp', 0.8, 0.0)]), WIDTH])
preset(C, 'Reverse Swell', 'Backwards grains and a long tail: sounds swell in.',
       M(grn(0.7, 450, 12, 0, 0.2, 1.0, 0.05), rev(0.55, 0.85, 8.0, 0.4)),
       [('SWELL', [('g_mix', 0.0, 1.0)]), ('LENGTH', [('g_size', 150, 800)]), ('SPACE', [('r_mix', 0.1, 0.8)]), GLOW])
preset(C, 'Galaxy', 'Wide stereo motion, shimmer and long echoes in space.',
       M(rev(0.5, 1.0, 14.0, 0.3, 0.35, 0.8), dly(7, 0.5, 0.25, 1, 0.5, 0.4), mod(4, 0.1, 0.6, 0.0, 1.0)),
       [GLOW, ('ORBIT', [('m_rate', 0.03, 1.0)]), ECHO, ('SIZE', [('r_size', 0.5, 1.0), ('r_decay', 4, 25)])])
preset(C, 'Tunnel', 'A metallic tunnel with flanged reflections.',
       M(mod(1, 0.12, 0.7, 0.75, 0.6), rev(0.4, 0.55, 3.5, 0.2), dly(2, 0.5, 0.2, 0)),
       [('METAL', [('m_fb', 0.2, 0.92)]), ('SPEED', [('m_rate', 0.03, 2.0)]), SPACEY, ('ECHO', [('y_mix', 0.0, 0.5)])])
preset(C, 'Bloom', 'Each note blooms into a soft, rising cloud.',
       M(grn(0.5, 300, 16, 12, 0.4, 0.2, 0.2), rev(0.5, 0.85, 7.0, 0.35, 0.3, 0.5)),
       [('BLOOM', [('g_mix', 0.0, 0.9), ('r_shim', 0.0, 0.5)]), ('SIZE', [('r_size', 0.3, 1.0)]), ('PITCH', [('g_pitch', 0, 12)]), ('MIX', [('r_mix', 0.1, 0.8)])])
preset(C, 'Far Away', 'Puts the sample in the distance, filtered and hazy.',
       M(filt(2, 1400, 0.15), rev(0.6, 0.8, 5.0, 0.6, 0.0, 0.3, 80)),
       [('DISTANCE', [('r_mix', 0.2, 0.9), ('f_cut', 4000, 700)]), ('HAZE', [('r_mod', 0.0, 1.0)]), ('SIZE', [('r_size', 0.3, 1.0)]), ('FOCUS', [('f_res', 0.0, 0.6)])])

# =============================================================================================
# 2 PADS & TEXTURES
C = 2
preset(C, 'Silk Pad', 'Smooth, wide and warm: the sample as a pad.',
       M(grn(0.8, 450, 14, 0, 0.3, 0.1, 0.15), mod(0, 0.25, 0.5, 0.2, 0.4), lp(7000, 0.1), rev(0.45, 0.8, 6.0, 0.5)),
       [('SMOOTH', [('g_size', 150, 800)]), WIDTH, ('TONE', [('f_cut', 1500, 16000)]), SPACE])
preset(C, 'Ambient Bed', 'A slow, endless texture for the background.',
       M(grn(1.0, 700, 9, 0, 0.6, 0.3, 0.2), filt(0, 3500, 0.15, 0.05, 0.4), rev(0.55, 0.9, 10.0, 0.5, 0.15)),
       [('TEXTURE', [('g_spread', 0.1, 1.0)]), ('MOTION', [('f_depth', 0.0, 0.8)]), GLOW, SPACE])
preset(C, 'Frozen Pad', 'A frozen, held version of the sound with movement.',
       M(grn(1.0, 380, 24, 0, 0.3, 0.2, 0.25, 1), mod(0, 0.2, 0.6, 0.2, 0.5), rev(0.5, 0.85, 8.0, 0.45)),
       [('HOLD', [('g_mix', 0.4, 1.0)]), ('WIDTH', [('m_mix', 0.0, 0.8)]), ('GRAIN', [('g_size', 80, 700)]), SPACE], (0.9, 0.5, 0.5, 0.5))
preset(C, 'Velvet', 'Dark, soft and close, like heavy curtains.',
       M(lp(1500, 0.25), grn(0.4, 300, 14, 0, 0.2, 0.0, 0.1), rev(0.35, 0.5, 2.5, 0.7)),
       [TONE, ('SOFT', [('g_mix', 0.0, 0.8)]), SPACE, ('WARMTH', [('f_res', 0.0, 0.6)])], (0.25, 0.5, 0.5, 0.5))
preset(C, 'Aurora', 'Shimmering, slowly shifting colors.',
       M(grn(0.6, 350, 18, 12, 0.5, 0.2, 0.3), mod(2, 0.08, 0.7, 0.5, 0.5), rev(0.5, 0.9, 9.0, 0.3, 0.45)),
       [('COLOR', [('m_mix', 0.0, 0.9)]), ('SPEED', [('m_rate', 0.02, 1.0)]), GLOW, ('PARTICLES', [('g_mix', 0.0, 0.9)])])
preset(C, 'Dust Cloud', 'Grainy, broken texture with crackle.',
       M(grn(0.85, 80, 40, 0, 0.8, 0.4, 0.5), lofi(12, 22000, 0, 0, 0.2, 0.3, 0.3), rev(0.3, 0.6, 3.0)),
       [('DUST', [('l_crackle', 0.0, 1.0), ('l_noise', 0.0, 0.5)]), ('GRAIN', [('g_dens', 8, 80)]), ('AGE', [('l_age', 0.0, 0.8)]), SPACE])
preset(C, 'Warm Blanket', 'Saturated, warm and wrapped in a soft room.',
       M(drv(4, 0.35, 0.35, 0.7), lp(5000, 0.1), grn(0.35, 400, 12, 0, 0.25), rev(0.35, 0.55, 3.0, 0.6)),
       [('WARMTH', [('d_amt', 0.0, 0.8)]), TONE, ('SOFT', [('g_mix', 0.0, 0.8)]), SPACE])
preset(C, 'Organ Drone', 'A steady drone with octaves, like a soft organ.',
       M(grn(1.0, 600, 12, 0, 0.1, 0.0, 0.05, 1), pit(12, -12, 0.35, 0.35, 1.0), lp(6000, 0.1), rev(0.45, 0.8, 7.0, 0.5)),
       [('DRONE', [('g_mix', 0.3, 1.0)]), ('OCTAVES', [('p_mix_a', 0.0, 0.7), ('p_mix_b', 0.0, 0.7)]), TONE, SPACE], (0.9, 0.5, 0.5, 0.5))
preset(C, 'Breath', 'Airy, thin texture with movement, like breathing.',
       M(filt(1, 450, 0.2, 0.15, 0.5), grn(0.6, 250, 20, 0, 0.6, 0.3, 0.4), rev(0.45, 0.7, 5.0, 0.2)),
       [('AIR', [('f_cut', 100, 2000)]), ('BREATHE', [('f_depth', 0.0, 1.0)]), ('RATE', [('f_rate', 0.05, 1.0)]), SPACE])
preset(C, 'Cinematic Swell', 'Big, emotional and slow: built for film.',
       M(grn(0.65, 500, 14, 0, 0.3, 0.6, 0.15), pit(-12, 12, 0.25, 0.2, 1.0), rev(0.55, 1.0, 10.0, 0.45, 0.25)),
       [('SWELL', [('g_mix', 0.0, 1.0)]), ('DEPTH', [('p_mix_a', 0.0, 0.7)]), GLOW, SPACE])
preset(C, 'Glacier', 'Cold, clear and very slow.',
       M(grn(0.9, 800, 6, 0, 0.2, 0.0, 0.05), filt(1, 300, 0.1), rev(0.55, 0.95, 14.0, 0.15, 0.3)),
       [('ICE', [('r_damp', 0.6, 0.0), ('f_cut', 50, 1500)]), ('SLOW', [('g_size', 300, 800)]), GLOW, SPACE])
preset(C, 'Nebula', 'Wide, swirling noise-like texture.',
       M(grn(0.95, 150, 50, 0, 1.0, 0.5, 0.7), mod(4, 0.07, 0.8, 0.0, 1.0), rev(0.5, 0.9, 8.0, 0.4, 0.2)),
       [('SWIRL', [('m_rate', 0.02, 0.6)]), ('CHAOS', [('g_jit', 0.2, 1.0)]), ('DENSITY', [('g_dens', 10, 80)]), SPACE])
preset(C, 'Music Box Dream', 'Bright, high and twinkly, like a music box in a dream.',
       M(pit(12, 24, 0.6, 0.25, 0.4), filt(1, 400, 0.1), mod(6, 4.5, 0.15, 0.0, 1.0), rev(0.45, 0.7, 5.0, 0.2, 0.2)),
       [('TWINKLE', [('p_mix_b', 0.0, 0.7)]), ('WOBBLE', [('m_depth', 0.0, 0.5)]), GLOW, SPACE])
preset(C, 'Underground', 'Low, muffled and heavy, like a room next door.',
       M(lp(450, 0.35), drv(0, 0.3, 0.3), rev(0.35, 0.4, 2.0, 0.8)),
       [('MUFFLE', [('f_cut', 2500, 150)]), ('THUMP', [('d_amt', 0.0, 0.8)]), SPACE, ('RESO', [('f_res', 0.0, 0.8)])])
preset(C, 'Lullaby', 'Gentle, soft and slightly wobbly.',
       M(lofi(16, 48000, 0.25, 0.1, 0.05, 0, 0.2), lp(6000, 0.1), rev(0.4, 0.6, 4.0, 0.5)),
       [('WOBBLE', [('l_wow', 0.0, 0.8)]), TONE, ('AGE', [('l_age', 0.0, 0.7)]), SPACE])

# =============================================================================================
# 3 RHYTHM & GLITCH
C = 3
preset(C, 'Beat Repeat', 'Random 1/8 repeats in time with your project.',
       M(stut(0, 3, 0.35), rev(0.15, 0.4, 1.5)),
       [('AMOUNT', [('s_amt', 0.0, 0.9)]), ('DIVISION', [('s_div', 1, 5)]), ('MIX', [('s_mix', 0.3, 1.0)]), SPACE])
preset(C, 'Trance Gate', 'Classic chopping gate pattern on 1/16s.',
       M(stut(1, 4, 0.9, 2), rev(0.2, 0.5, 2.0)),
       [('DEPTH', [('s_amt', 0.0, 1.0)]), ('PATTERN', [('s_pat', 0, 7)]), ('RATE', [('s_div', 2, 5)]), SPACE], (0.9, 0.3, 0.5, 0.5))
preset(C, 'Tape Stop', 'Every now and then the tape slows to a halt.',
       M(stut(2, 1, 0.5), rev(0.1, 0.4, 1.5)),
       [('HOW OFTEN', [('s_amt', 0.0, 1.0)]), ('LENGTH', [('s_div', 0, 3)]), ('MIX', [('s_mix', 0.3, 1.0)]), SPACE])
preset(C, 'Reverse Chops', 'Pieces of the sample flip backwards in time.',
       M(stut(3, 2, 0.45), dly(3, 0.3, 0.15)),
       [('AMOUNT', [('s_amt', 0.0, 0.9)]), ('PIECE', [('s_div', 1, 4)]), ECHO, ('MIX', [('s_mix', 0.3, 1.0)])])
preset(C, 'Glitch Machine', 'Repeats, bitcrush and stutters: broken in the best way.',
       M(stut(0, 4, 0.55), lofi(8, 16000, 0, 0, 0, 0, 0), dly(1, 0.3, 0.15, 1)),
       [('GLITCH', [('s_amt', 0.0, 1.0)]), ('CRUSH', [('l_bits', 16, 4), ('l_rate', 48000, 4000)]), ('SIZE', [('s_div', 2, 5)]), ECHO])
preset(C, 'Half Time', 'Chunks drop to half speed for a slowed-down groove.',
       M(stut(4, 2, 0.5), lp(9000, 0.1), rev(0.15, 0.4, 1.5)),
       [('AMOUNT', [('s_amt', 0.0, 1.0)]), ('CHUNK', [('s_div', 1, 4)]), TONE, SPACE])
preset(C, 'Stutter Roll', 'Fast 1/32 rolls like a drum roll on the sample.',
       M(stut(0, 5, 0.45), filt(1, 120, 0.1)),
       [('ROLL', [('s_amt', 0.0, 1.0)]), ('SPEED', [('s_div', 3, 5)]), ('MIX', [('s_mix', 0.3, 1.0)]), ('THIN', [('f_cut', 20, 1500)])])
preset(C, 'Chopper', 'Syncopated gate pattern on 1/8s.',
       M(stut(1, 3, 1.0, 3), rev(0.15, 0.4, 1.2)),
       [('DEPTH', [('s_amt', 0.0, 1.0)]), ('PATTERN', [('s_pat', 0, 7)]), ('RATE', [('s_div', 2, 5)]), SPACE], (1.0, 0.43, 0.3, 0.5))
preset(C, 'Gated Hall', 'A big reverb chopped by a rhythmic gate.',
       M(rev(0.55, 0.8, 5.0, 0.4), stut(1, 4, 0.95, 6)),
       [('GATE', [('s_amt', 0.0, 1.0)]), ('PATTERN', [('s_pat', 0, 7)]), SPACE, ('RATE', [('s_div', 2, 5)])], (0.95, 0.86, 0.5, 0.67))
preset(C, 'Tremolo Pulse', 'Smooth rhythmic pulsing in volume.',
       M(mod(3, 4.0, 0.7, 0, 1.0), rev(0.2, 0.5, 2.0)),
       [('DEPTH', [('m_depth', 0.0, 1.0)]), ('SPEED', [('m_rate', 0.5, 16.0)]), SPACE, ('WIDTH', [('m_mix', 0.3, 1.0)])])
preset(C, 'Bitcrush Repeat', 'Crushed and repeated, 8-bit style.',
       M(stut(0, 3, 0.4), lofi(6, 11000), drv(1, 0.25, 0.6)),
       [('REPEAT', [('s_amt', 0.0, 1.0)]), ('BITS', [('l_bits', 12, 3)]), ('RATE', [('l_rate', 22000, 2000)]), ('DRIVE', [('d_amt', 0.0, 0.8)])])
preset(C, 'Echo Slap', 'Short tempo echoes that bounce left and right.',
       M(dly(1, 0.45, 0.4, 1, 0.7, 0.0)),
       [ECHO, ('TIME', [('y_div', 0, 5)]), ('TONE', [('y_tone', 0.1, 1.0)]), ('WOBBLE', [('y_mod', 0.0, 0.8)])])
preset(C, 'Broken Tape', 'Tape stops, wow and hiss: a malfunctioning machine.',
       M(stut(2, 2, 0.4), lofi(16, 48000, 0.5, 0.4, 0.2, 0.1, 0.35)),
       [('STOPS', [('s_amt', 0.0, 1.0)]), ('WOBBLE', [('l_wow', 0.0, 1.0), ('l_flutter', 0.0, 0.8)]), ('AGE', [('l_age', 0.0, 0.8)]), ('HISS', [('l_noise', 0.0, 0.8)])])
preset(C, 'Glitch Grains', 'Rhythmic repeats mixed with scattered grains.',
       M(stut(0, 4, 0.4), grn(0.5, 50, 25, 0, 0.6, 0.4, 0.5), rev(0.2, 0.5, 2.0)),
       [('REPEAT', [('s_amt', 0.0, 0.9)]), ('GRAINS', [('g_mix', 0.0, 1.0)]), ('CHAOS', [('g_jit', 0.0, 1.0)]), SPACE])
preset(C, 'Pump', 'Ducking pulse on every beat, like sidechain.',
       M(stut(1, 2, 0.85, 0), mod(3, 2.0, 0.8, 0, 1.0)),
       [('PUMP', [('m_depth', 0.0, 1.0)]), ('SPEED', [('m_rate', 0.5, 8.0)]), ('MIX', [('m_mix', 0.0, 1.0)]), ('GATE', [('s_amt', 0.0, 0.9)])], (0.7, 0.25, 1.0, 0.0))

# =============================================================================================
# 4 LO-FI & TAPE
C = 4
preset(C, 'Old Cassette', 'Warm, wobbly cassette with soft hiss.',
       M(lofi(16, 48000, 0.35, 0.25, 0.25, 0, 0.35), drv(4, 0.2, 0.4, 0.6)),
       [('WOBBLE', [('l_wow', 0.0, 1.0)]), ('AGE', [('l_age', 0.0, 0.9)]), ('HISS', [('l_noise', 0.0, 0.8)]), ('WARMTH', [('d_amt', 0.0, 0.7)])])
preset(C, 'Vinyl Night', 'Crackling record, dusty and warm.',
       M(lofi(16, 48000, 0.1, 0.05, 0.1, 0.5, 0.4), lp(9000, 0.1), rev(0.2, 0.4, 1.5, 0.6)),
       [('CRACKLE', [('l_crackle', 0.0, 1.0)]), ('AGE', [('l_age', 0.0, 0.9)]), TONE, SPACE])
preset(C, 'Lo-Fi Beats', 'The classic chill lo-fi sound: soft, warm and dusty.',
       M(lofi(12, 26000, 0.2, 0.1, 0.12, 0.25, 0.3), lp(5500, 0.15), rev(0.2, 0.45, 1.8, 0.6)),
       [('DUST', [('l_crackle', 0.0, 0.8), ('l_noise', 0.0, 0.5)]), TONE, ('WOBBLE', [('l_wow', 0.0, 0.8)]), SPACE])
preset(C, '8-Bit', 'Retro game console crunch.',
       M(lofi(4, 8000)),
       [('BITS', [('l_bits', 10, 2)]), ('RATE', [('l_rate', 20000, 1500)]), ('NOISE', [('l_noise', 0.0, 0.5)]), ('AGE', [('l_age', 0.0, 0.6)])])
preset(C, 'AM Radio', 'Narrow, distorted, from an old speaker.',
       M(lofi(10, 12000, 0, 0.1, 0.25, 0.05, 0.85), drv(0, 0.4, 0.5)),
       [('NARROW', [('l_age', 0.4, 1.0)]), ('STATIC', [('l_noise', 0.0, 0.9)]), ('DRIVE', [('d_amt', 0.0, 0.9)]), ('WAVER', [('l_flutter', 0.0, 1.0)])])
preset(C, 'Worn VHS', 'Wobbly, blurred and soft like an old tape.',
       M(lofi(14, 30000, 0.6, 0.3, 0.15, 0, 0.45), mod(0, 0.3, 0.5, 0.2, 0.3), lp(6000, 0.1)),
       [('WOBBLE', [('l_wow', 0.0, 1.0)]), ('BLUR', [('m_mix', 0.0, 0.8)]), TONE, ('HISS', [('l_noise', 0.0, 0.7)])])
preset(C, 'Phonograph', 'Gramophone-era sound: thin, crackly and narrow.',
       M(lofi(12, 16000, 0.15, 0.1, 0.2, 0.7, 0.95)),
       [('AGE', [('l_age', 0.6, 1.0)]), ('CRACKLE', [('l_crackle', 0.2, 1.0)]), ('WOBBLE', [('l_wow', 0.0, 0.6)]), ('HISS', [('l_noise', 0.0, 0.7)])])
preset(C, 'Tape Saturation', 'Smooth analog tape warmth, no wobble.',
       M(drv(4, 0.45, 0.45, 0.8), lofi(16, 48000, 0.05, 0.05, 0.05, 0, 0.15)),
       [('SATURATE', [('d_amt', 0.0, 1.0)]), ('TONE', [('d_tone', 0.1, 1.0)]), ('AGE', [('l_age', 0.0, 0.6)]), ('MIX', [('d_mix', 0.0, 1.0)])])
preset(C, 'Melted', 'Heavy wow: the tape is melting in the sun.',
       M(lofi(16, 48000, 1.0, 0.5, 0.1, 0, 0.3), rev(0.3, 0.6, 3.0, 0.5)),
       [('MELT', [('l_wow', 0.2, 1.0)]), ('FLUTTER', [('l_flutter', 0.0, 1.0)]), ('AGE', [('l_age', 0.0, 0.9)]), SPACE])
preset(C, 'Bit Rot', 'Digital decay: crushed and noisy.',
       M(lofi(5, 6000, 0, 0, 0.3, 0.2, 0.2), drv(2, 0.2, 0.5, 0.4)),
       [('ROT', [('l_bits', 12, 3), ('l_rate', 22000, 2500)]), ('NOISE', [('l_noise', 0.0, 0.9)]), ('FOLD', [('d_amt', 0.0, 0.8)]), ('AGE', [('l_age', 0.0, 0.8)])])
preset(C, 'Dusty Sampler', 'Like an old 12-bit sampler from the 90s.',
       M(lofi(12, 26040, 0, 0, 0.02, 0, 0.2), drv(0, 0.2, 0.5, 0.6), lp(12000, 0.1)),
       [('GRIT', [('l_bits', 16, 8)]), ('RATE', [('l_rate', 40000, 12000)]), ('PUNCH', [('d_amt', 0.0, 0.8)]), TONE])
preset(C, 'Telephone', 'Small, boxy and narrow like a phone call.',
       M(filt(2, 1500, 0.4), drv(1, 0.35, 0.6), lofi(12, 8000)),
       [('FOCUS', [('f_cut', 600, 3500)]), ('DRIVE', [('d_amt', 0.0, 0.9)]), ('QUALITY', [('l_rate', 4000, 16000)]), ('RESO', [('f_res', 0.0, 0.8)])])
preset(C, 'Memory', 'Faded and far away, like a half-remembered song.',
       M(lofi(14, 30000, 0.3, 0.15, 0.1, 0.15, 0.5), lp(4000, 0.1), rev(0.45, 0.7, 4.0, 0.6)),
       [('FADE', [('l_age', 0.0, 1.0)]), TONE, SPACE, ('WOBBLE', [('l_wow', 0.0, 0.9)])])
preset(C, 'Walkman', 'Cheap portable tape player, slightly wobbly.',
       M(lofi(16, 48000, 0.2, 0.6, 0.18, 0, 0.3), drv(4, 0.2, 0.4, 0.5)),
       [('FLUTTER', [('l_flutter', 0.0, 1.0)]), ('WOW', [('l_wow', 0.0, 0.8)]), ('HISS', [('l_noise', 0.0, 0.8)]), ('AGE', [('l_age', 0.0, 0.8)])])
preset(C, 'Crushed Room', 'Bitcrushed reverb: grimy and wide.',
       M(rev(0.45, 0.6, 3.0, 0.3), lofi(7, 12000)),
       [('CRUSH', [('l_bits', 14, 3)]), ('RATE', [('l_rate', 30000, 3000)]), SPACE, ('AGE', [('l_age', 0.0, 0.7)])])

# =============================================================================================
# 5 PITCH & HARMONY
C = 5
preset(C, 'Octave Up', 'Adds a clean octave above.',
       M(pit(12, 24, 0.6, 0.0, 1.0), rev(0.15, 0.4, 1.5)),
       [('OCTAVE', [('p_mix_a', 0.0, 1.0)]), ('2ND OCTAVE', [('p_mix_b', 0.0, 0.8)]), ('DRY', [('p_dry', 0.0, 1.0)]), SPACE], (0.6, 0.0, 1.0, 0.3))
preset(C, 'Octave Down', 'Adds a heavy octave below.',
       M(pit(-12, -24, 0.65, 0.0, 1.0), lp(9000, 0.1)),
       [('OCTAVE', [('p_mix_a', 0.0, 1.0)]), ('SUB', [('p_mix_b', 0.0, 0.8)]), ('DRY', [('p_dry', 0.0, 1.0)]), TONE], (0.6, 0.0, 1.0, 0.8))
preset(C, 'Power Fifths', 'A fifth and an octave for a huge stacked sound.',
       M(pit(7, 12, 0.5, 0.4, 1.0), rev(0.2, 0.5, 2.0)),
       [('FIFTH', [('p_mix_a', 0.0, 1.0)]), ('OCTAVE', [('p_mix_b', 0.0, 1.0)]), ('DRY', [('p_dry', 0.0, 1.0)]), SPACE])
preset(C, 'Major Chord', 'Turns a single note into a major chord.',
       M(pit(4, 7, 0.55, 0.5, 1.0), rev(0.25, 0.6, 3.0)),
       [('THIRD', [('p_mix_a', 0.0, 1.0)]), ('FIFTH', [('p_mix_b', 0.0, 1.0)]), ('DRY', [('p_dry', 0.0, 1.0)]), SPACE])
preset(C, 'Minor Chord', 'Turns a single note into a minor chord.',
       M(pit(3, 7, 0.55, 0.5, 1.0), rev(0.25, 0.6, 3.0)),
       [('THIRD', [('p_mix_a', 0.0, 1.0)]), ('FIFTH', [('p_mix_b', 0.0, 1.0)]), ('DRY', [('p_dry', 0.0, 1.0)]), SPACE])
preset(C, 'Chipmunk', 'Pitched way up: high and squeaky.',
       M(pit(12, 19, 1.0, 0.0, 0.0)),
       [('PITCH', [('p_a', 3, 24)]), ('BLEND', [('p_dry', 0.0, 1.0)]), ('HARMONY', [('p_mix_b', 0.0, 0.8)]), ('AIR', [('p_mix_a', 0.3, 1.0)])], (0.4, 0.0, 0.0, 1.0))
preset(C, 'Demon Voice', 'Pitched way down and dark.',
       M(pit(-12, -19, 1.0, 0.3, 0.0), drv(0, 0.25, 0.4), lp(6000, 0.2)),
       [('DEPTH', [('p_a', -24, -3)]), ('GROWL', [('d_amt', 0.0, 0.9)]), ('BLEND', [('p_dry', 0.0, 1.0)]), TONE], (0.5, 0.3, 0.0, 0.6))
preset(C, 'Detune Wide', 'Slightly detuned copies for a thick, wide sound.',
       M(pit(0.15, -0.15, 0.5, 0.5, 1.0), mod(0, 0.4, 0.3, 0.1, 0.3)),
       [('DETUNE', [('p_a', 0.0, 0.5), ('p_b', 0.0, -0.5)]), ('THICK', [('p_mix_a', 0.0, 1.0), ('p_mix_b', 0.0, 1.0)]), WIDTH, ('DRY', [('p_dry', 0.3, 1.0)])])
preset(C, 'Octave Shimmer', 'Octaves up and down blooming into a shimmer hall.',
       M(pit(12, -12, 0.4, 0.3, 1.0), rev(0.45, 0.85, 7.0, 0.35, 0.45)),
       [('UP', [('p_mix_a', 0.0, 1.0)]), ('DOWN', [('p_mix_b', 0.0, 1.0)]), GLOW, SPACE])
preset(C, 'Sus Fourth', 'An open, floating suspended chord.',
       M(pit(5, 7, 0.55, 0.5, 1.0), rev(0.3, 0.7, 4.0, 0.3, 0.15)),
       [('FOURTH', [('p_mix_a', 0.0, 1.0)]), ('FIFTH', [('p_mix_b', 0.0, 1.0)]), GLOW, SPACE])
preset(C, 'Harmonizer', 'A third and a sixth above, like backing vocals.',
       M(pit(4, 9, 0.5, 0.4, 1.0), mod(0, 0.3, 0.3, 0.1, 0.3), rev(0.25, 0.6, 2.5)),
       [('LOW VOICE', [('p_mix_a', 0.0, 1.0)]), ('HIGH VOICE', [('p_mix_b', 0.0, 1.0)]), WIDTH, SPACE])
preset(C, 'Vibrato', 'A singing, wavering pitch.',
       M(mod(6, 5.0, 0.35, 0, 1.0), rev(0.2, 0.5, 2.0)),
       [('DEPTH', [('m_depth', 0.0, 1.0)]), ('SPEED', [('m_rate', 1.0, 9.0)]), ('MIX', [('m_mix', 0.0, 1.0)]), SPACE])
preset(C, 'Sub Bass', 'Adds a big, clean low end two octaves down.',
       M(pit(-12, -24, 0.4, 0.4, 1.0), lp(12000, 0.1)),
       [('LOW', [('p_mix_a', 0.0, 1.0)]), ('SUB', [('p_mix_b', 0.0, 1.0)]), ('DRY', [('p_dry', 0.0, 1.0)]), TONE], (0.4, 0.4, 1.0, 1.0))
preset(C, 'Pitch Grains', 'Grains randomly jump between octaves.',
       M(grn(0.7, 120, 22, 0, 0.3, 0.1, 1.0), rev(0.35, 0.7, 4.0)),
       [('JUMPS', [('g_jit', 0.2, 1.0)]), ('PITCH', [('g_pitch', -12, 12)]), ('GRAINS', [('g_mix', 0.0, 1.0)]), SPACE])
preset(C, 'Robot', 'Metallic ring modulation: a talking machine.',
       M(mod(5, 1.0, 1.0, 0, 0.8), drv(1, 0.2, 0.6)),
       [('METAL', [('m_mix', 0.0, 1.0)]), ('PITCH', [('m_rate', 0.3, 10.0)]), ('DRIVE', [('d_amt', 0.0, 0.8)]), ('TONE', [('d_tone', 0.2, 1.0)])])

# =============================================================================================
# 6 FILTER & MOTION
C = 6
preset(C, 'Filter Sweep', 'A slow lowpass sweep breathing up and down.',
       M(filt(0, 900, 0.45, 0.12, 0.6), rev(0.2, 0.5, 2.0)),
       [('CUTOFF', [('f_cut', 150, 12000)]), ('SWEEP', [('f_depth', 0.0, 1.0)]), ('SPEED', [('f_rate', 0.02, 2.0)]), ('RESO', [('f_res', 0.0, 0.85)])])
preset(C, 'Wah Wah', 'Funky resonant bandpass that talks.',
       M(filt(2, 900, 0.7, 2.0, 0.6)),
       [('WAH', [('f_depth', 0.0, 1.0)]), ('SPEED', [('f_rate', 0.3, 8.0)]), ('CENTER', [('f_cut', 300, 3000)]), ('RESO', [('f_res', 0.3, 0.9)])])
preset(C, 'Auto Filter', 'The filter follows the loudness: plucky and alive.',
       M(filt(0, 400, 0.55, 0.5, 0.0, 0.8)),
       [('FOLLOW', [('f_env', 0.0, 1.0)]), ('CUTOFF', [('f_cut', 100, 4000)]), ('RESO', [('f_res', 0.0, 0.9)]), ('MOTION', [('f_depth', 0.0, 0.6)])])
preset(C, 'Phaser Swirl', 'A classic swirling phaser.',
       M(mod(2, 0.25, 0.7, 0.6, 0.6)),
       [('SWIRL', [('m_mix', 0.0, 1.0)]), ('SPEED', [('m_rate', 0.03, 3.0)]), ('DEPTH', [('m_depth', 0.2, 1.0)]), ('INTENSITY', [('m_fb', 0.0, 0.9)])])
preset(C, 'Jet Flanger', 'A whooshing jet-plane flanger.',
       M(mod(1, 0.12, 0.8, 0.8, 0.6)),
       [('WHOOSH', [('m_fb', 0.2, 0.93)]), ('SPEED', [('m_rate', 0.03, 2.0)]), ('DEPTH', [('m_depth', 0.2, 1.0)]), ('MIX', [('m_mix', 0.2, 1.0)])])
preset(C, 'Chorus Wide', 'A lush, wide chorus.',
       M(mod(0, 0.5, 0.6, 0.2, 0.55), rev(0.15, 0.45, 1.6)),
       [('WIDTH', [('m_mix', 0.0, 1.0)]), ('SPEED', [('m_rate', 0.05, 3.0)]), ('DEPTH', [('m_depth', 0.1, 1.0)]), SPACE])
preset(C, 'Auto Pan', 'The sound moves between left and right.',
       M(mod(4, 0.5, 0.9, 0, 1.0), rev(0.15, 0.5, 1.8)),
       [('WIDTH', [('m_depth', 0.0, 1.0)]), ('SPEED', [('m_rate', 0.05, 6.0)]), ('MIX', [('m_mix', 0.0, 1.0)]), SPACE])
preset(C, 'Highpass Rise', 'Thins the sound out like a build-up before a drop.',
       M(filt(1, 400, 0.45), rev(0.3, 0.6, 3.0)),
       [('RISE', [('f_cut', 20, 6000)]), ('RESO', [('f_res', 0.0, 0.85)]), SPACE, ('SWEEP', [('f_depth', 0.0, 0.5)])], (0.3, 0.5, 0.4, 0.0))
preset(C, 'Lowpass Muffle', 'A smooth lowpass: from muffled to open.',
       M(filt(0, 1200, 0.25)),
       [('OPEN', [('f_cut', 120, 20000)]), ('RESO', [('f_res', 0.0, 0.85)]), ('MOVE', [('f_depth', 0.0, 0.6)]), ('SPEED', [('f_rate', 0.03, 2.0)])], (0.35, 0.3, 0.0, 0.3))
preset(C, 'Resonant Scream', 'A sharp resonant peak sweeping fast.',
       M(filt(0, 1500, 0.9, 1.2, 0.5), drv(0, 0.3, 0.6)),
       [('SCREAM', [('f_res', 0.5, 0.97)]), ('SPEED', [('f_rate', 0.1, 10.0)]), ('CUTOFF', [('f_cut', 300, 6000)]), ('DRIVE', [('d_amt', 0.0, 0.8)])])
preset(C, 'Notch Motion', 'A moving notch carves a hole through the sound.',
       M(filt(3, 1500, 0.5, 0.3, 0.7)),
       [('DEPTH', [('f_depth', 0.0, 1.0)]), ('SPEED', [('f_rate', 0.03, 4.0)]), ('CENTER', [('f_cut', 300, 6000)]), ('WIDTH', [('f_res', 0.0, 0.9)])])
preset(C, 'Underwater', 'Like hearing the sample under water.',
       M(filt(0, 600, 0.5, 0.3, 0.3), mod(6, 0.8, 0.4, 0, 0.7), rev(0.3, 0.5, 2.5, 0.8)),
       [('DEPTH', [('f_cut', 2000, 200)]), ('BUBBLES', [('m_depth', 0.0, 1.0)]), ('CURRENT', [('f_depth', 0.0, 1.0)]), SPACE])
preset(C, 'Breathing Phase', 'Slow phaser plus a gentle filter, like slow breathing.',
       M(mod(2, 0.08, 0.8, 0.4, 0.5), filt(0, 4000, 0.2, 0.1, 0.4), rev(0.3, 0.6, 3.5)),
       [('PHASE', [('m_mix', 0.0, 1.0)]), ('BREATH', [('f_depth', 0.0, 1.0)]), ('SPEED', [('m_rate', 0.02, 1.0), ('f_rate', 0.02, 1.0)]), SPACE])
preset(C, 'Talk Box', 'A vowel-like filter that seems to speak.',
       M(filt(2, 650, 0.6, 1.5, 0.45, 0.4), drv(0, 0.3, 0.6)),
       [('VOWEL', [('f_cut', 400, 2500)]), ('TALK', [('f_depth', 0.0, 1.0)]), ('SPEED', [('f_rate', 0.2, 6.0)]), ('DRIVE', [('d_amt', 0.0, 0.7)])])
preset(C, 'Dreamy Drift', 'Slow chorus, soft filter motion and a gentle space.',
       M(mod(0, 0.15, 0.8, 0.2, 0.6), filt(0, 5000, 0.2, 0.06, 0.5), rev(0.4, 0.75, 5.0, 0.4)),
       [('DRIFT', [('m_depth', 0.0, 1.0)]), ('TONE', [('f_cut', 1000, 16000)]), ('MOTION', [('f_depth', 0.0, 1.0)]), SPACE])

# =============================================================================================
# 7 DRIVE & CHARACTER
C = 7
preset(C, 'Warm Tube', 'Gentle tube warmth that thickens the sound.',
       M(drv(4, 0.35, 0.55, 0.9), rev(0.12, 0.35, 1.2, 0.5)),
       [('WARMTH', [('d_amt', 0.0, 1.0)]), ('TONE', [('d_tone', 0.1, 1.0)]), ('MIX', [('d_mix', 0.0, 1.0)]), SPACE], (0.5, 0.5, 0.9, 0.2))
preset(C, 'Overdrive', 'Classic crunchy overdrive.',
       M(drv(0, 0.6, 0.55), lp(9000, 0.1)),
       [('DRIVE', [('d_amt', 0.1, 1.0)]), ('TONE', [('d_tone', 0.0, 1.0)]), ('MIX', [('d_mix', 0.0, 1.0)]), ('SMOOTH', [('f_cut', 20000, 2500)])])
preset(C, 'Fuzz Box', 'Thick, buzzy, gated fuzz.',
       M(drv(3, 0.75, 0.5), lp(20000, 0.1)),
       [('FUZZ', [('d_amt', 0.2, 1.0)]), ('TONE', [('d_tone', 0.0, 1.0)]), ('MIX', [('d_mix', 0.0, 1.0)]), ('SMOOTH', [('f_cut', 20000, 2500)])], (0.5, 0.5, 1.0, 0.0))
preset(C, 'Wavefolder', 'Rich, metallic folding distortion.',
       M(drv(2, 0.55, 0.6, 0.8), rev(0.15, 0.4, 1.5)),
       [('FOLD', [('d_amt', 0.0, 1.0)]), ('TONE', [('d_tone', 0.1, 1.0)]), ('MIX', [('d_mix', 0.0, 1.0)]), SPACE])
preset(C, 'Hard Clip', 'Loud, aggressive clipping.',
       M(drv(1, 0.7, 0.7), lofi(16)),
       [('CLIP', [('d_amt', 0.1, 1.0)]), ('TONE', [('d_tone', 0.0, 1.0)]), ('MIX', [('d_mix', 0.0, 1.0)]), ('CRUSH', [('l_bits', 16, 4)])], (0.6, 0.7, 1.0, 0.0))
preset(C, 'Distorted Space', 'Drive into a big reverb: noisy and huge.',
       M(drv(0, 0.6, 0.5), rev(0.45, 0.8, 5.0, 0.4)),
       [('DRIVE', [('d_amt', 0.0, 1.0)]), SPACE, ('TONE', [('d_tone', 0.0, 1.0)]), ('SIZE', [('r_size', 0.3, 1.0)])])
preset(C, 'Crunchy Echo', 'Distorted echoes that keep breaking up.',
       M(drv(0, 0.45, 0.5), dly(3, 0.6, 0.35, 1, 0.4, 0.15)),
       [('CRUNCH', [('d_amt', 0.0, 1.0)]), ECHO, ('TIME', [('y_div', 1, 7)]), ('DARK', [('y_tone', 0.9, 0.0)])])
preset(C, 'Broken Speaker', 'Small, torn and buzzing.',
       M(filt(2, 1800, 0.35), drv(3, 0.6, 0.6), lofi(10, 20000, 0, 0.2, 0.05, 0, 0.4)),
       [('BROKEN', [('d_amt', 0.0, 1.0)]), ('SIZE', [('f_cut', 600, 5000)]), ('RATTLE', [('l_flutter', 0.0, 1.0)]), ('AGE', [('l_age', 0.0, 0.9)])])
preset(C, 'Saturated Bass', 'Fat low-end saturation for bass and kicks.',
       M(drv(4, 0.5, 0.3), lp(4000, 0.25)),
       [('FAT', [('d_amt', 0.0, 1.0)]), TONE, ('MIX', [('d_mix', 0.0, 1.0)]), ('RESO', [('f_res', 0.0, 0.7)])])
preset(C, 'Industrial', 'Harsh metal, folded and ring-modulated.',
       M(drv(2, 0.7, 0.6), mod(5, 0.8, 1.0, 0, 0.5), rev(0.25, 0.5, 2.0, 0.2)),
       [('METAL', [('m_mix', 0.0, 1.0)]), ('FOLD', [('d_amt', 0.0, 1.0)]), ('PITCH', [('m_rate', 0.2, 8.0)]), SPACE])
preset(C, 'Exciter', 'Adds sparkle and presence to dull sounds.',
       M(drv(4, 0.3, 1.0, 0.35), filt(1, 60, 0.1)),
       [('SPARKLE', [('d_mix', 0.0, 0.8)]), ('DRIVE', [('d_amt', 0.0, 0.8)]), ('CLEAN LOW', [('f_cut', 20, 300)]), ('TONE', [('d_tone', 0.5, 1.0)])])
preset(C, 'Grit', 'Subtle bitcrush and drive for a gritty edge.',
       M(drv(0, 0.3, 0.6, 0.6), lofi(10, 24000)),
       [('GRIT', [('l_bits', 16, 6)]), ('DRIVE', [('d_amt', 0.0, 0.9)]), ('RATE', [('l_rate', 48000, 8000)]), ('MIX', [('d_mix', 0.0, 1.0)])])
preset(C, 'Chorus Drive', 'Driven and doubled, like a guitar pedalboard.',
       M(drv(0, 0.45, 0.55), mod(0, 0.6, 0.5, 0.2, 0.5), dly(4, 0.3, 0.15)),
       [('DRIVE', [('d_amt', 0.0, 1.0)]), WIDTH, ECHO, ('TONE', [('d_tone', 0.0, 1.0)])])
preset(C, 'Radio Ghost', 'A distorted, filtered voice from another room.',
       M(filt(2, 1200, 0.5), drv(1, 0.5, 0.5), rev(0.4, 0.6, 3.0, 0.5), lofi(12, 16000, 0, 0.2, 0.15, 0, 0.5)),
       [('GHOST', [('r_mix', 0.1, 0.8)]), ('DRIVE', [('d_amt', 0.0, 1.0)]), ('BAND', [('f_cut', 400, 4000)]), ('STATIC', [('l_noise', 0.0, 0.9)])])
preset(C, 'Clean Polish', 'Almost clean: a touch of warmth, width and room.',
       M(drv(4, 0.15, 0.6, 0.5), mod(0, 0.4, 0.25, 0.1, 0.2), rev(0.15, 0.4, 1.4, 0.5)),
       [('WARMTH', [('d_amt', 0.0, 0.6)]), WIDTH, SPACE, ('MIX', [('d_mix', 0.0, 1.0)])])

# =============================================================================================
# 1.2 — new sounds. Appended (never inserted) so saved projects keep their sounds.
# ---- 3 more for each original category
C = 0   # GRANULAR
preset(C, 'Pointillism', 'Sparse single droplets of sound, scattered and bouncing around the stereo field.',
       M(grn(1.0, 40, 5, 0, 1.0, 0.3, 0.25), mod(4, 2.2, 0.8, 0, 1.0), dly(1, 0.45, 0.35, 1, 0.7, 0.0)),
       [('DROPS', [('g_dens', 2, 20)]), ('SIZE', [('g_size', 15, 150)]), ('BOUNCE', [('y_mix', 0.0, 0.7), ('m_depth', 0.0, 1.0)]), ('MIX', [('g_mix', 0.3, 1.0)])], (0.5, 0.5, 0.5, 0.9))
preset(C, 'Frozen Gate', 'A frozen grain hold chopped into a rhythmic pulse.',
       M(grn(1.0, 300, 26, 0, 0.2, 0.15, 0.2, 1), stut(1, 3, 0.95, 2), rev(0.3, 0.6, 3.0)),
       [('HOLD', [('g_mix', 0.3, 1.0)]), ('GATE', [('s_amt', 0.0, 1.0)]), ('PATTERN', [('s_pat', 0, 7)]), ('SPEED', [('s_div', 2, 5)])], (0.9, 0.95, 0.29, 0.33))
preset(C, 'Tape Grains', 'Grains read off a wobbly old tape: warm, smeared and slightly seasick.',
       M(grn(0.75, 260, 14, 0, 0.35, 0.1, 0.15), lofi(14, 30000, 0.55, 0.2, 0.12, 0.0, 0.3), lp(7000, 0.1)),
       [('GRAINS', [('g_mix', 0.0, 1.0)]), ('WOBBLE', [('l_wow', 0.0, 1.0)]), ('AGE', [('l_age', 0.0, 0.9), ('l_noise', 0.0, 0.4)]), TONE])
C = 1   # SHIMMER & SPACE
preset(C, 'Spring Tank', 'The boingy metal spring reverb of old guitar amps and dub studios.',
       M(mod(1, 0.04, 0.15, 0.85, 0.45), dly(0, 0.55, 0.25, 0, 0.8, 0.05), rev(0.3, 0.2, 2.0, 0.1, 0.0, 0.6, 2)),
       [('SPRING', [('m_fb', 0.4, 0.93), ('y_fb', 0.2, 0.8)]), ('MIX', [('r_mix', 0.05, 0.6), ('m_mix', 0.1, 0.7)]), ('BOING', [('m_depth', 0.05, 0.6)]), ('DECAY', [('r_decay', 0.6, 5.0)])])
preset(C, 'Backwards Hall', 'Pieces of the sound swell in backwards out of a big hall.',
       M(stut(3, 1, 0.55), rev(0.5, 0.85, 6.0, 0.4, 0.1)),
       [('REVERSE', [('s_amt', 0.0, 1.0)]), ('PIECE', [('s_div', 0, 3)]), ('SPACE', [('r_mix', 0.1, 0.85)]), GLOW])
preset(C, 'Doppler Space', 'Echoes that bend in pitch and fly past from side to side, like passing cars in a tunnel.',
       M(dly(3, 0.5, 0.35, 1, 0.6, 0.9), mod(4, 0.15, 0.7, 0, 1.0), rev(0.35, 0.8, 4.5, 0.4)),
       [('DOPPLER', [('y_mod', 0.2, 1.0)]), ECHO, ('FLY', [('m_depth', 0.0, 1.0), ('m_rate', 0.05, 1.0)]), SPACE])
C = 2   # PADS & TEXTURES
preset(C, 'Vowel Pad', 'A soft pad that slowly sings between vowels, like a choir going "aah" to "ooh".',
       M(grn(0.9, 500, 14, 0, 0.3, 0.05, 0.1), filt(2, 800, 0.55, 0.08, 0.5), rev(0.45, 0.8, 6.0, 0.4)),
       [('PAD', [('g_mix', 0.2, 1.0)]), ('VOWEL', [('f_depth', 0.0, 1.0)]), ('SPEED', [('f_rate', 0.02, 1.0)]), SPACE])
preset(C, 'Chord Pad', 'Long grains plus a major third and fifth: one note becomes a lush chord.',
       M(grn(0.85, 550, 12, 0, 0.35, 0.05, 0.1), pit(4, 7, 0.45, 0.4, 1.0), rev(0.45, 0.85, 7.0, 0.45)),
       [('PAD', [('g_mix', 0.2, 1.0)]), ('THIRD', [('p_mix_a', 0.0, 0.9)]), ('FIFTH', [('p_mix_b', 0.0, 0.9)]), SPACE])
preset(C, 'Wind Texture', 'Airy whooshing wind made from the brightest parts of the sound.',
       M(filt(1, 1100, 0.35, 0.09, 0.6), grn(0.9, 160, 45, 0, 1.0, 0.5, 0.55), lofi(16, 48000, 0, 0, 0.35), rev(0.45, 0.8, 6.0, 0.2)),
       [('GUSTS', [('f_depth', 0.0, 1.0)]), ('SPEED', [('f_rate', 0.02, 0.6)]), ('AIR', [('l_noise', 0.0, 0.8), ('f_cut', 1800, 300)]), SPACE], (0.5, 0.5, 0.6, 0.5))
C = 3   # RHYTHM & GLITCH
preset(C, 'Triplet Repeat', 'Swinging triplet repeats that roll against the beat.',
       M(stut(0, 6, 0.45), dly(4, 0.3, 0.12), rev(0.12, 0.35, 1.2)),
       [('AMOUNT', [('s_amt', 0.0, 0.95)]), ('ECHO', [('y_mix', 0.0, 0.4)]), ('MIX', [('s_mix', 0.3, 1.0)]), SPACE])
preset(C, 'Gated Echo', 'A choppy gate whose holes are filled by dotted echoes.',
       M(stut(1, 4, 0.9, 3), dly(4, 0.55, 0.4, 1, 0.55, 0.05)),
       [('GATE', [('s_amt', 0.0, 1.0)]), ECHO, ('PATTERN', [('s_pat', 0, 7)]), ('TONE', [('y_tone', 0.1, 1.0)])], (0.9, 0.5, 0.43, 0.5))
preset(C, 'Shuffle Chop', 'A shuffled triplet gate: instantly groovy.',
       M(stut(1, 6, 1.0, 6), drv(4, 0.2, 0.55, 0.6), rev(0.15, 0.4, 1.3)),
       [('DEPTH', [('s_amt', 0.2, 1.0)]), ('PATTERN', [('s_pat', 0, 7)]), ('WARMTH', [('d_amt', 0.0, 0.7)]), SPACE], (1.0, 0.86, 0.3, 0.3))
C = 4   # LO-FI & TAPE
preset(C, 'Cassette Dub', 'A wobbly cassette feeding a dark dub echo.',
       M(lofi(16, 48000, 0.4, 0.2, 0.15, 0.0, 0.3), dly(6, 0.62, 0.35, 0, 0.25, 0.3)),
       [('WOBBLE', [('l_wow', 0.0, 1.0)]), ECHO, ('TIME', [('y_div', 3, 8)]), ('AGE', [('l_age', 0.0, 0.8)])])
preset(C, 'Lo-Fi Chorus', 'Cheap digital chorus: grainy, wide and charming.',
       M(lofi(10, 20000), mod(0, 0.8, 0.7, 0.2, 0.6)),
       [('GRIT', [('l_bits', 16, 6)]), ('WIDTH', [('m_mix', 0.0, 1.0)]), ('SPEED', [('m_rate', 0.1, 3.0)]), ('RATE', [('l_rate', 40000, 8000)])])
preset(C, 'Reel to Reel', 'Big studio tape machine: warm saturation and a tight slap-back.',
       M(drv(4, 0.35, 0.5, 0.7), lofi(16, 48000, 0.08, 0.06, 0.04, 0, 0.1), dly(0, 0.15, 0.18, 0, 0.6, 0.02)),
       [('SATURATE', [('d_amt', 0.0, 0.9)]), ('SLAP', [('y_mix', 0.0, 0.5)]), ('WOW', [('l_wow', 0.0, 0.5)]), ('HISS', [('l_noise', 0.0, 0.4)])])
C = 5   # PITCH & HARMONY
preset(C, 'Harmonic Gate', 'A fifth and an octave stacked on the sound, chopped into a trance-style 1/16 gate.',
       M(pit(7, 12, 0.5, 0.4, 1.0), stut(1, 4, 1.0, 2), mod(0, 0.5, 0.5, 0.1, 0.4), rev(0.25, 0.6, 2.5)),
       [('HARMONY', [('p_mix_a', 0.0, 1.0), ('p_mix_b', 0.0, 0.8)]), ('GATE', [('s_amt', 0.0, 1.0)]), ('PATTERN', [('s_pat', 0, 7)]), SPACE], (0.5, 1.0, 0.29, 0.4))
preset(C, 'Out of Tune Piano', 'Honky-tonk: two slightly mistuned copies, a little tape wobble and a wooden room.',
       M(pit(0.3, -0.25, 0.55, 0.45, 1.0), lofi(16, 48000, 0.2, 0.08), drv(4, 0.15, 0.55, 0.5), rev(0.25, 0.25, 0.9, 0.55, 0.0, 0.1, 6)),
       [('MISTUNE', [('p_a', 0.0, 0.8), ('p_b', 0.0, -0.7)]), ('WOBBLE', [('l_wow', 0.0, 0.8)]), ('ROOM', [('r_mix', 0.0, 0.6)]), ('WARMTH', [('d_amt', 0.0, 0.6)])])
preset(C, 'Seasick Chord', 'A minor chord on a deep, slow vibrato with drifting echoes.',
       M(pit(3, 7, 0.5, 0.45, 1.0), mod(6, 0.45, 0.65, 0, 1.0), dly(6, 0.45, 0.25, 1, 0.45, 0.4)),
       [('CHORD', [('p_mix_a', 0.0, 1.0), ('p_mix_b', 0.0, 1.0)]), ('SEASICK', [('m_depth', 0.0, 1.0)]), ('SPEED', [('m_rate', 0.1, 2.0)]), ECHO])

C = 6   # FILTER & MOTION
preset(C, 'Stereo Filter Dance', 'A resonant band filter swaying while the sound circles the stereo field.',
       M(filt(2, 1200, 0.55, 0.7, 0.6), mod(4, 0.35, 0.8, 0, 1.0)),
       [('DANCE', [('f_depth', 0.0, 1.0)]), ('SPEED', [('f_rate', 0.1, 4.0), ('m_rate', 0.05, 2.0)]), ('CENTER', [('f_cut', 300, 4000)]), ('WIDTH', [('m_depth', 0.0, 1.0)])])
preset(C, 'Ducking Filter', 'Loud hits close the filter, so they stay dark while the echoes between them bloom bright.',
       M(filt(0, 9000, 0.35, 0.5, 0.0, -0.9), dly(4, 0.5, 0.35, 1, 0.8, 0.05)),
       [('DUCK', [('f_env', 0.0, -1.0)]), ('CUTOFF', [('f_cut', 1500, 20000)]), ECHO, ('RESO', [('f_res', 0.0, 0.8)])], (0.9, 0.7, 0.5, 0.4))
preset(C, 'Rotary', 'A spinning speaker: wobbling pitch and swirling tone.',
       M(mod(6, 6.5, 0.25, 0, 1.0), filt(0, 6000, 0.25, 6.5, 0.25), drv(4, 0.2, 0.5, 0.6)),
       [('SPEED', [('m_rate', 0.8, 7.5), ('f_rate', 0.8, 7.5)]), ('DEPTH', [('m_depth', 0.0, 0.6), ('f_depth', 0.0, 0.5)]), ('DRIVE', [('d_amt', 0.0, 0.8)]), ('TONE', [('f_cut', 2000, 16000)])])
C = 7   # DRIVE & CHARACTER
preset(C, 'Fuzz Octave', 'A classic octave-fuzz pedal: buzzing fuzz with a ringing octave on top.',
       M(drv(3, 0.65, 0.6), pit(12, 24, 0.45, 0.0, 1.0)),
       [('FUZZ', [('d_amt', 0.2, 1.0)]), ('OCTAVE', [('p_mix_a', 0.0, 1.0)]), ('TONE', [('d_tone', 0.0, 1.0)]), ('MIX', [('d_mix', 0.0, 1.0)])])
preset(C, 'Shoegaze Wall', 'A wall of sound: fuzz smeared by a wide chorus into a huge, glowing reverb.',
       M(drv(3, 0.55, 0.45, 0.8), mod(0, 0.35, 0.85, 0.3, 0.7), rev(0.5, 0.95, 8.0, 0.45, 0.25)),
       [('FUZZ', [('d_amt', 0.0, 1.0)]), ('SMEAR', [('m_mix', 0.0, 1.0)]), ('WALL', [('r_mix', 0.1, 0.85)]), GLOW])
preset(C, 'Amp in a Room', 'A cranked little amp recorded with a microphone in a small room.',
       M(drv(1, 0.5, 0.5), filt(2, 1700, 0.2), rev(0.28, 0.25, 0.8, 0.5, 0.0, 0.1, 4)),
       [('GAIN', [('d_amt', 0.0, 1.0)]), ('CABINET', [('f_cut', 700, 4000)]), ('ROOM', [('r_mix', 0.0, 0.6)]), ('TONE', [('d_tone', 0.0, 1.0)])])

# ---- 8 BREAKCORE & CHAOS
C = 8
preset(C, 'Break Scramble', 'The beat cut into 1/16-long slices and shuffled into a new, random order, some backwards.',
       M(grn(1.0, 125, 8, 0, 1.0, 0.3, 0.0), drv(1, 0.3, 0.6, 0.8), filt(1, 60, 0.1)),
       [('SCRAMBLE', [('g_spread', 0.1, 1.0)]), ('SLICE', [('g_size', 40, 250)]), ('BACKWARDS', [('g_rev', 0.0, 0.8)]), ('CRUNCH', [('d_amt', 0.0, 0.9)])], (0.9, 0.4, 0.38, 0.33))
preset(C, 'Machine Gun', 'A 1/32 gate firing through fuzz and a short flanger.',
       M(stut(1, 5, 1.0, 1), drv(3, 0.55, 0.6), mod(1, 2.5, 0.4, 0.6, 0.4)),
       [('FIRE', [('s_amt', 0.0, 1.0)]), ('FUZZ', [('d_amt', 0.0, 1.0)]), ('METAL', [('m_mix', 0.0, 0.9)]), ('PATTERN', [('s_pat', 0, 7)])], (1.0, 0.55, 0.45, 0.14))
preset(C, 'Tape Slam', 'Whole bars slam to a halt an octave down, then crash back in.',
       M(stut(2, 0, 0.35), pit(-12, -24, 0.4, 0.0, 1.0), drv(3, 0.4, 0.5, 0.8)),
       [('SLAMS', [('s_amt', 0.0, 0.9)]), ('WEIGHT', [('p_mix_a', 0.0, 0.9)]), ('FUZZ', [('d_amt', 0.0, 0.9)]), ('LENGTH', [('s_div', 0, 2)])])
preset(C, 'Inside Out', 'Backwards 1/16 slivers through a wavefolder and a fast phaser: turned inside out.',
       M(stut(3, 4, 0.6), drv(2, 0.55, 0.6, 0.8), mod(2, 4.0, 0.8, 0.6, 0.5)),
       [('REVERSE', [('s_amt', 0.0, 1.0)]), ('FOLD', [('d_amt', 0.0, 1.0)]), ('SWIRL', [('m_mix', 0.0, 1.0), ('m_rate', 0.5, 8.0)]), ('PIECE', [('s_div', 2, 5)])])
preset(C, 'Glitch Burst', 'Microscopic grains, 1/32 repeats and sample-rate crush: a digital explosion.',
       M(grn(0.8, 15, 80, 0, 0.6, 0.3, 1.0), stut(0, 5, 0.3), lofi(10, 14000)),
       [('BURST', [('g_mix', 0.0, 1.0)]), ('REPEATS', [('s_amt', 0.0, 0.8)]), ('CRUSH', [('l_rate', 40000, 3000)]), ('CHAOS', [('g_jit', 0.2, 1.0)])])
preset(C, 'Octave Jumper', 'Grains that leap between octaves through a rhythmic gate.',
       M(grn(0.9, 70, 30, 0, 0.3, 0.0, 1.0), stut(1, 4, 1.0, 3), rev(0.15, 0.4, 1.5)),
       [('JUMPS', [('g_jit', 0.3, 1.0)]), ('GATE', [('s_amt', 0.0, 1.0)]), ('PATTERN', [('s_pat', 0, 7)]), ('GRAINS', [('g_mix', 0.2, 1.0)])], (0.9, 1.0, 0.43, 0.9))
preset(C, 'Stutter Cathedral', 'Fuzzed 1/8 repeats thrown into a giant hall: chaos with a halo.',
       M(stut(0, 3, 0.5), drv(3, 0.45, 0.6), rev(0.55, 0.9, 8.0, 0.4), filt(1, 150, 0.1)),
       [('STUTTER', [('s_amt', 0.0, 1.0)]), ('FUZZ', [('d_amt', 0.0, 1.0)]), ('HALL', [('r_mix', 0.1, 0.85)]), ('THIN', [('f_cut', 20, 1500)])])
preset(C, 'Gabber Stomp', 'Pounding hardcore distortion: maxed fuzz, a sub octave and a resonant lowpass.',
       M(drv(3, 1.0, 0.45), pit(-12, 0, 0.35, 0.0, 1.0), filt(0, 3200, 0.55), rev(0.12, 0.25, 0.7, 0.5)),
       [('STOMP', [('d_amt', 0.4, 1.0)]), ('SUB', [('p_mix_a', 0.0, 0.9)]), ('CUTOFF', [('f_cut', 600, 12000)]), ('RESO', [('f_res', 0.0, 0.85)])])
preset(C, 'Ring Mod Panic', 'Metallic ring modulation with 1/16 repeats and bouncing echoes.',
       M(mod(5, 6.0, 1.0, 0, 0.6), stut(0, 4, 0.4), dly(1, 0.5, 0.25, 1)),
       [('METAL', [('m_mix', 0.0, 1.0)]), ('PITCH', [('m_rate', 0.5, 15.0)]), ('REPEATS', [('s_amt', 0.0, 0.9)]), ECHO])
preset(C, 'Broken Drum Machine', 'A dying 8-bit drum machine dropping into half speed.',
       M(lofi(8, 6000), stut(4, 3, 0.6), filt(1, 90, 0.2)),
       [('BROKEN', [('s_amt', 0.0, 1.0)]), ('BITS', [('l_bits', 14, 3)]), ('RATE', [('l_rate', 22000, 2000)]), ('THIN', [('f_cut', 20, 800)])])
preset(C, 'Overdrive Chop', 'Hard-clipped stabs on a sparse 1/8 gate with slap echoes.',
       M(drv(1, 0.8, 0.65), stut(1, 3, 1.0, 4), dly(0, 0.35, 0.3, 1, 0.7, 0.0)),
       [('DRIVE', [('d_amt', 0.2, 1.0)]), ('CHOP', [('s_amt', 0.0, 1.0)]), ('PATTERN', [('s_pat', 0, 7)]), ('SLAP', [('y_mix', 0.0, 0.6)])], (0.75, 1.0, 0.57, 0.5))
preset(C, 'Panic Attack', 'Racing tremolo, nervous 1/32 stutters and a thin, driven edge.',
       M(mod(3, 14.0, 0.9, 0, 1.0), stut(0, 5, 0.25), filt(1, 300, 0.2), drv(0, 0.35, 0.6)),
       [('PANIC', [('m_rate', 4.0, 20.0)]), ('DEPTH', [('m_depth', 0.2, 1.0)]), ('STUTTER', [('s_amt', 0.0, 0.8)]), ('EDGE', [('d_amt', 0.0, 0.9)])])
preset(C, 'Digital Meltdown', 'A crushed signal melting with tape wow and stopping dead.',
       M(lofi(4, 3000, 0.6, 0.3, 0.1, 0, 0.0), stut(2, 1, 0.45), drv(1, 0.2, 0.5)),
       [('MELT', [('l_wow', 0.0, 1.0)]), ('BITS', [('l_bits', 10, 2)]), ('STOPS', [('s_amt', 0.0, 1.0)]), ('DRIVE', [('d_amt', 0.0, 0.8)])])
preset(C, 'Rewind', 'A DJ rewind: whole bars now and then spin backwards with a chipmunk edge and a jet swoosh.',
       M(stut(3, 0, 0.4), pit(12, 0, 0.35, 0.0, 1.0), mod(1, 0.4, 0.6, 0.6, 0.4)),
       [('REWINDS', [('s_amt', 0.0, 1.0)]), ('LENGTH', [('s_div', 0, 2)]), ('SQUEAL', [('p_mix_a', 0.0, 1.0)]), ('SWOOSH', [('m_mix', 0.0, 0.9)])])
preset(C, 'Jungle Wobble', 'Triplet repeats over a pumping, wobbling lowpass: rolling jungle energy.',
       M(stut(0, 6, 0.4), filt(0, 700, 0.6, 3.0, 0.5, 0.6), mod(6, 6.0, 0.2, 0, 1.0)),
       [('ROLL', [('s_amt', 0.0, 0.9)]), ('WOBBLE', [('f_depth', 0.0, 1.0)]), ('SPEED', [('f_rate', 0.5, 8.0)]), ('CUTOFF', [('f_cut', 200, 4000)])])

# ---- 9 DREAMY & NOSTALGIC
C = 9
preset(C, 'Rainy Window', 'Heard through a window on a rainy evening: soft, muffled, crackling rain.',
       M(lofi(16, 48000, 0.05, 0, 0.3, 0.8, 0.15), lp(2600, 0.15), mod(0, 0.2, 0.4, 0.1, 0.3), rev(0.25, 0.4, 1.6, 0.7)),
       [('RAIN', [('l_crackle', 0.0, 1.0), ('l_noise', 0.0, 0.6)]), ('WINDOW', [('f_cut', 600, 8000)]), ('BLUR', [('m_mix', 0.0, 0.8)]), SPACE])
preset(C, 'Half Asleep', 'Every now and then the sound drifts to half speed, as if dozing off.',
       M(stut(4, 0, 0.3), lp(5000, 0.1), mod(3, 0.4, 0.3, 0, 1.0), rev(0.45, 0.8, 6.0, 0.5)),
       [('DOZE', [('s_amt', 0.0, 0.8)]), ('DREAM', [('r_mix', 0.1, 0.8)]), ('BREATH', [('m_depth', 0.0, 0.8)]), TONE])
preset(C, 'Daydream', 'Backwards grains, wide chorus and a faint shimmer: drifting away.',
       M(grn(0.55, 380, 12, 0, 0.5, 0.7, 0.15), mod(0, 0.15, 0.8, 0.2, 0.5), dly(6, 0.4, 0.2), rev(0.45, 0.85, 6.5, 0.4, 0.3)),
       [('DRIFT', [('g_mix', 0.0, 1.0)]), ('WIDTH', [('m_mix', 0.0, 0.9)]), ('ECHO', [('y_mix', 0.0, 0.4)]), GLOW])
preset(C, 'Last Summer', 'Warm, golden and slightly worn, like a summer you remember.',
       M(drv(4, 0.3, 0.4, 0.7), lofi(16, 48000, 0.2, 0.05, 0.04, 0.05, 0.15), dly(4, 0.35, 0.18, 1, 0.35), lp(6500, 0.1)),
       [('WARMTH', [('d_amt', 0.0, 0.8)]), ('WOBBLE', [('l_wow', 0.0, 0.7)]), ('ECHO', [('y_mix', 0.0, 0.45)]), TONE])
preset(C, 'Nostalgia Loop', 'A worn memory loop that sometimes slows down and catches itself.',
       M(stut(2, 1, 0.18), lofi(14, 36000, 0.25, 0.1, 0.08, 0.15, 0.4), rev(0.3, 0.55, 2.5, 0.6)),
       [('CATCH', [('s_amt', 0.0, 0.7)]), ('WORN', [('l_age', 0.0, 0.9)]), ('DUST', [('l_crackle', 0.0, 0.8)]), SPACE])
preset(C, 'Snow Globe', 'A frozen, glittering moment two octaves up, floating over the sound.',
       M(grn(0.45, 150, 30, 0, 0.4, 0.1, 0.3, 1), pit(24, 12, 0.4, 0.2, 1.0), filt(1, 500, 0.1), rev(0.45, 0.75, 6.0, 0.15, 0.25)),
       [('SNOW', [('p_mix_a', 0.0, 0.9)]), ('FROZEN', [('g_mix', 0.0, 0.9)]), GLOW, ('SPACE', [('r_mix', 0.1, 0.8)])])
preset(C, 'Tuning In', 'An old radio dial slowly searching for the station.',
       M(filt(2, 1300, 0.6, 0.07, 0.85), lofi(14, 26000, 0, 0.15, 0.3, 0.1, 0.4)),
       [('SEARCH', [('f_depth', 0.0, 1.0)]), ('SPEED', [('f_rate', 0.02, 1.0)]), ('STATIC', [('l_noise', 0.0, 0.9)]), ('FOCUS', [('f_res', 0.2, 0.9)])])
preset(C, 'Dream Sequence', 'Slow, deep pitch drift and a hazy halo: the film goes into a dream.',
       M(mod(6, 0.35, 0.55, 0, 1.0), grn(0.4, 300, 12, 0, 0.3, 0.1, 0.1), rev(0.5, 0.85, 7.0, 0.4, 0.35)),
       [('DRIFT', [('m_depth', 0.0, 1.0)]), ('HAZE', [('g_mix', 0.0, 0.9)]), GLOW, SPACE])
preset(C, 'Rose Tinted', 'A slow phaser on a warm tube with a soft quarter-note echo.',
       M(mod(2, 0.12, 0.6, 0.35, 0.45), drv(4, 0.25, 0.45, 0.6), dly(5, 0.35, 0.18, 1, 0.4)),
       [('TINT', [('m_mix', 0.0, 0.9)]), ('WARMTH', [('d_amt', 0.0, 0.7)]), ('ECHO', [('y_mix', 0.0, 0.45)]), ('SPEED', [('m_rate', 0.03, 1.0)])])
preset(C, 'Hazy Morning', 'Long smeared grains in soft light, with a breath of air.',
       M(grn(0.8, 520, 10, 0, 0.55, 0.15, 0.1), lp(5000, 0.1), lofi(16, 48000, 0, 0, 0.12), rev(0.3, 0.5, 2.5, 0.5)),
       [('HAZE', [('g_mix', 0.2, 1.0), ('g_spread', 0.2, 0.9)]), ('LIGHT', [('f_cut', 1500, 14000)]), ('AIR', [('l_noise', 0.0, 0.5)]), SPACE])
preset(C, 'Melancholy', 'An octave below in the shadows, a slow tremble and a long dark hall.',
       M(pit(-12, -24, 0.35, 0.0, 1.0), drv(4, 0.2, 0.3, 0.5), mod(3, 0.7, 0.35, 0, 1.0), rev(0.5, 0.85, 8.0, 0.75)),
       [('SHADOW', [('p_mix_a', 0.0, 0.8)]), ('TREMBLE', [('m_depth', 0.0, 0.8)]), ('HALL', [('r_mix', 0.1, 0.8)]), ('DARK', [('r_damp', 0.3, 1.0)])])
preset(C, 'Silent Film', 'A flickering projector: fast flutter, a faint flicker in volume and old grain.',
       M(lofi(14, 30000, 0.1, 0.65, 0.15, 0.3, 0.6), mod(3, 18.0, 0.22, 0, 1.0), filt(1, 250, 0.1)),
       [('FLICKER', [('m_depth', 0.0, 0.6)]), ('PROJECTOR', [('l_flutter', 0.0, 1.0)]), ('AGE', [('l_age', 0.2, 1.0)]), ('DUST', [('l_crackle', 0.0, 0.9)])])
preset(C, 'Sunset Drive', 'Chorus, ping-pong echoes and warm drive: a car radio in the golden hour.',
       M(mod(0, 0.6, 0.55, 0.2, 0.5), dly(6, 0.4, 0.25, 1, 0.55, 0.15), drv(4, 0.25, 0.6, 0.6), lp(12000, 0.1)),
       [('CHORUS', [('m_mix', 0.0, 0.9)]), ECHO, ('WARMTH', [('d_amt', 0.0, 0.7)]), TONE])
preset(C, 'Merry-Go-Round', 'An old fairground ride: circling around you, slightly warped, with a faint octave sparkle.',
       M(mod(4, 0.3, 0.85, 0, 1.0), lofi(16, 48000, 0.35, 0.1, 0.05, 0.05, 0.25), pit(12, 0, 0.25, 0.0, 1.0), rev(0.25, 0.5, 2.0, 0.5)),
       [('CIRCLE', [('m_depth', 0.0, 1.0)]), ('SPEED', [('m_rate', 0.05, 1.5)]), ('WARP', [('l_wow', 0.0, 0.9)]), ('SPARKLE', [('p_mix_a', 0.0, 0.7)])])
preset(C, 'Polaroid', 'A frozen snapshot fades in and out under the live sound, faded and dusty.',
       M(grn(0.5, 400, 16, 0, 0.4, 0.1, 0.1, 1), mod(3, 0.2, 0.75, 0, 1.0), lofi(15, 40000, 0.1, 0, 0.06, 0.35, 0.5), lp(7000, 0.1)),
       [('SNAPSHOT', [('g_mix', 0.0, 1.0)]), ('FADE', [('m_depth', 0.0, 1.0), ('m_rate', 0.05, 1.0)]), ('AGE', [('l_age', 0.0, 1.0), ('l_crackle', 0.0, 0.7)]), TONE])

# ---- 10 CINEMATIC & DARK
C = 10
preset(C, 'Tension Drone', 'A frozen, low, dark hold that never resolves.',
       M(grn(1.0, 650, 10, -12, 0.2, 0.1, 0.1, 1), lp(1300, 0.25), rev(0.5, 0.9, 12.0, 0.7)),
       [('DRONE', [('g_mix', 0.3, 1.0)]), ('DEPTH', [('g_pitch', -24, 0)]), ('DARK', [('f_cut', 4000, 300)]), SPACE], (0.9, 0.5, 0.5, 0.5))
preset(C, 'Endless Riser', 'A slow sweep upward with backwards grains and a rising shimmer.',
       M(filt(1, 200, 0.45, 0.025, 1.0), grn(0.5, 300, 18, 0, 0.4, 0.9, 0.1), rev(0.5, 0.9, 9.0, 0.3, 0.55)),
       [('RISE', [('f_cut', 40, 3000)]), ('SPEED', [('f_rate', 0.02, 0.3)]), GLOW, ('BACKWARDS', [('g_mix', 0.0, 0.9)])])
preset(C, 'Distant War', 'Low, saturated thunder echoing from far away.',
       M(pit(-12, -24, 0.5, 0.2, 1.0), drv(4, 0.45, 0.25), lp(2200, 0.2), rev(0.55, 0.95, 6.0, 0.8, 0.0, 0.2, 120)),
       [('WEIGHT', [('p_mix_a', 0.0, 1.0)]), ('DISTANCE', [('r_mix', 0.1, 0.9), ('f_cut', 6000, 600)]), ('DRIVE', [('d_amt', 0.0, 0.9)]), ('SUB', [('p_mix_b', 0.0, 0.8)])])
preset(C, 'Cold Void', 'Thin, bright and empty: an endless space with nothing in it.',
       M(filt(1, 600, 0.15), grn(0.4, 600, 5, 0, 0.9, 0.3, 0.2), rev(0.6, 1.0, 22.0, 0.05, 0.0, 0.6)),
       [('EMPTY', [('r_mix', 0.2, 0.95)]), ('COLD', [('f_cut', 100, 2500)]), ('FRAGMENTS', [('g_mix', 0.0, 0.9)]), ('MOTION', [('r_mod', 0.0, 1.0)])])
preset(C, 'Heartbeat', 'A slow pulse in a muffled, low room: suspense.',
       M(mod(3, 1.15, 0.85, 0, 1.0), lp(900, 0.3), pit(-12, -24, 0.3, 0.0, 1.0), rev(0.2, 0.4, 1.5, 0.8)),
       [('PULSE', [('m_depth', 0.0, 1.0)]), ('TEMPO', [('m_rate', 0.6, 2.5)]), ('MUFFLE', [('f_cut', 3000, 300)]), ('LOW', [('p_mix_a', 0.0, 0.8)])])
preset(C, 'Abyss', 'Two octaves down into a bottomless black hall.',
       M(pit(-24, -12, 0.45, 0.35, 0.6), lp(500, 0.2), drv(3, 0.2, 0.2, 0.4), rev(0.55, 1.0, 15.0, 0.85)),
       [('DEPTH', [('p_mix_a', 0.0, 1.0)]), ('DARKNESS', [('f_cut', 1500, 150)]), ('GRIT', [('d_amt', 0.0, 0.8)]), ('VOID', [('r_mix', 0.1, 0.9)])])
preset(C, 'Dark Matter', 'Slow ring-mod pulsing, reversed grains and a heavy, dark tail.',
       M(mod(5, 0.3, 1.0, 0, 0.55), grn(0.6, 400, 10, 0, 0.5, 1.0, 0.1), rev(0.45, 0.85, 8.0, 0.8)),
       [('MATTER', [('m_mix', 0.0, 1.0)]), ('PULSE', [('m_rate', 0.05, 2.0)]), ('BACKWARDS', [('g_mix', 0.0, 1.0)]), SPACE])
preset(C, 'Thriller Pulse', 'A ticking quarter-note gate into dark dotted echoes.',
       M(stut(1, 3, 0.9, 5), filt(0, 1500, 0.6), dly(4, 0.55, 0.3, 1, 0.2)),
       [('PULSE', [('s_amt', 0.0, 1.0)]), ('DARK', [('f_cut', 5000, 400)]), ECHO, ('PATTERN', [('s_pat', 0, 7)])], (0.9, 0.5, 0.5, 0.71))
preset(C, 'Trailer Boom', 'A huge hit: sub octave, hard drive and an instant giant hall.',
       M(pit(-12, -24, 0.55, 0.0, 1.0), drv(1, 0.55, 0.4), rev(0.45, 0.95, 4.0, 0.6, 0.0, 0.2, 2)),
       [('IMPACT', [('d_amt', 0.0, 1.0)]), ('SUB', [('p_mix_a', 0.0, 1.0)]), ('SIZE', [('r_mix', 0.1, 0.8), ('r_decay', 1.5, 8.0)]), ('TONE', [('d_tone', 0.0, 1.0)])])
preset(C, 'Ghost Whisper', 'Thin, backwards and flanged: something whispering behind you.',
       M(filt(1, 900, 0.3), grn(0.7, 220, 14, 0, 0.6, 1.0, 0.25), mod(1, 0.09, 0.6, 0.6, 0.5), rev(0.55, 0.85, 9.0, 0.3)),
       [('WHISPER', [('f_cut', 150, 2000)]), ('BACKWARDS', [('g_mix', 0.0, 1.0)]), ('HAUNT', [('m_mix', 0.0, 1.0)]), SPACE], (0.4, 0.5, 0.5, 0.5))
preset(C, 'Frozen Wasteland', 'An icy frozen hold with cold wind blowing through it.',
       M(grn(1.0, 450, 18, 0, 0.5, 0.2, 0.2, 1), filt(1, 350, 0.1, 0.06, 0.4), lofi(16, 48000, 0, 0, 0.35), rev(0.5, 0.95, 14.0, 0.1)),
       [('ICE', [('g_mix', 0.3, 1.0)]), ('WIND', [('l_noise', 0.0, 0.9)]), ('COLD', [('f_cut', 50, 2000)]), SPACE], (0.9, 0.5, 0.4, 0.5))
preset(C, 'Dissonance', 'A semitone rubbing against the sound with a slow phase: unease.',
       M(pit(1, -11, 0.45, 0.2, 1.0), mod(2, 0.07, 0.6, 0.4, 0.4), rev(0.4, 0.7, 5.0, 0.6)),
       [('CLASH', [('p_mix_a', 0.0, 1.0)]), ('LOW CLASH', [('p_mix_b', 0.0, 0.8)]), ('UNEASE', [('m_mix', 0.0, 0.9)]), SPACE])
preset(C, 'Bell Toll', 'A metallic, bell-like ring in a dark cathedral.',
       M(mod(5, 4.0, 1.0, 0, 0.35), lp(5000, 0.15), rev(0.55, 1.0, 12.0, 0.5, 0.0, 0.2, 40)),
       [('BELL', [('m_mix', 0.0, 0.8)]), ('PITCH', [('m_rate', 1.0, 9.0)]), ('CATHEDRAL', [('r_mix', 0.1, 0.9)]), TONE])
preset(C, 'Nightmare', 'Grains jumping between octaves a fifth down, with sudden slowdowns.',
       M(grn(0.8, 140, 22, -7, 0.5, 0.4, 1.0), stut(2, 1, 0.2), lp(4000, 0.3), rev(0.45, 0.8, 7.0, 0.7)),
       [('NIGHTMARE', [('g_jit', 0.2, 1.0)]), ('SLOWDOWNS', [('s_amt', 0.0, 0.8)]), ('DARK', [('f_cut', 8000, 800)]), SPACE])
preset(C, 'Siren Sweep', 'A slow, wailing vibrato in a huge, echoing street.',
       M(mod(6, 0.25, 0.9, 0, 1.0), filt(2, 1500, 0.3), dly(7, 0.45, 0.3, 1, 0.4), rev(0.45, 0.9, 6.0, 0.5)),
       [('WAIL', [('m_depth', 0.0, 1.0)]), ('SPEED', [('m_rate', 0.05, 1.5)]), ECHO, ('SPACE', [('r_mix', 0.1, 0.8)])])

# ---- 11 SCI-FI & EXPERIMENTAL
C = 11
preset(C, 'Alien Radio', 'A transmission from far away: a fast-scanning band filter, ring mod and static.',
       M(filt(2, 2000, 0.75, 6.0, 0.6), mod(5, 3.0, 1.0, 0, 0.35), lofi(14, 24000, 0, 0, 0.3)),
       [('SCAN', [('f_rate', 0.5, 14.0)]), ('ALIEN', [('m_mix', 0.0, 0.9)]), ('STATIC', [('l_noise', 0.0, 0.9)]), ('BAND', [('f_cut', 600, 5000)])])
preset(C, 'Robot Choir', 'Ring-modulated voices in fifths, doubled by chorus.',
       M(mod(5, 1.5, 1.0, 0, 0.55), pit(7, 12, 0.45, 0.35, 1.0), rev(0.3, 0.6, 2.5)),
       [('ROBOT', [('m_mix', 0.0, 1.0)]), ('CHOIR', [('p_mix_a', 0.0, 1.0), ('p_mix_b', 0.0, 0.8)]), ('PITCH', [('m_rate', 0.5, 6.0)]), SPACE])
preset(C, 'Laser Echo', 'Fast 1/16 echoes zapping through a screaming flanger.',
       M(dly(1, 0.7, 0.45, 1, 0.9, 0.0), mod(1, 3.0, 0.9, 0.85, 0.5), filt(1, 400, 0.2)),
       [('ZAP', [('m_fb', 0.3, 0.93)]), ECHO, ('SPEED', [('m_rate', 0.5, 10.0)]), ('THIN', [('f_cut', 50, 3000)])])
preset(C, 'Wormhole', 'Being pulled through a tunnel: a deep slow flanger, shimmer and falling grains.',
       M(mod(1, 0.05, 1.0, 0.9, 0.7), grn(0.4, 250, 14, -12, 0.6, 0.3, 0.3), rev(0.5, 0.9, 10.0, 0.3, 0.45)),
       [('PULL', [('m_fb', 0.3, 0.94)]), ('FALL', [('g_mix', 0.0, 0.9)]), GLOW, ('SPEED', [('m_rate', 0.02, 0.6)])])
preset(C, 'Broken Satellite', 'A dying signal: 5-bit crush, sparse dropouts and ring tones.',
       M(lofi(5, 3500), stut(1, 4, 0.95, 4), mod(5, 8.0, 1.0, 0, 0.3)),
       [('SIGNAL', [('l_bits', 12, 2)]), ('DROPOUTS', [('s_amt', 0.0, 1.0)]), ('TONE', [('m_mix', 0.0, 0.8)]), ('PATTERN', [('s_pat', 0, 7)])], (0.5, 0.95, 0.4, 0.57))
preset(C, 'Teleporter', 'Dematerializing: tiny sparkling grains an octave up, flickering at audio speed.',
       M(grn(0.7, 20, 60, 12, 0.5, 0.2, 0.6), mod(3, 25.0, 0.6, 0, 1.0), rev(0.3, 0.5, 2.0, 0.3)),
       [('BEAM', [('g_mix', 0.0, 1.0)]), ('FLICKER', [('m_depth', 0.0, 1.0), ('m_rate', 8.0, 40.0)]), ('SPARKLE', [('g_jit', 0.0, 1.0)]), SPACE])
preset(C, 'Data Stream', 'Tiny bitcrushed grains flying across the stereo field like data.',
       M(grn(0.85, 22, 80, 0, 0.3, 0.2, 0.7), lofi(8, 16000), mod(4, 6.0, 0.9, 0, 1.0), filt(1, 300, 0.2), drv(0, 0.4, 0.6)),
       [('STREAM', [('g_dens', 10, 80)]), ('BITS', [('l_bits', 14, 3)]), ('FLY', [('m_rate', 0.5, 15.0)]), ('THIN', [('f_cut', 50, 2000)])])
preset(C, 'Space Station', 'A hollow metal tube: a frozen resonant flanger inside a big chamber.',
       M(mod(1, 0.02, 0.25, 0.9, 0.55), rev(0.45, 0.85, 7.0, 0.3), lp(9000, 0.1)),
       [('METAL', [('m_fb', 0.5, 0.95)]), ('TUBE', [('m_depth', 0.05, 0.8)]), ('CHAMBER', [('r_mix', 0.1, 0.85)]), TONE])
preset(C, 'Morse Code', 'A narrow resonant beep tapped out by a sparse 1/16 gate.',
       M(stut(1, 4, 1.0, 3), filt(2, 1000, 0.8), drv(0, 0.5, 0.6), dly(3, 0.4, 0.2, 1, 0.7, 0.0)),
       [('CODE', [('s_pat', 0, 7)]), ('BEEP', [('f_res', 0.5, 0.97)]), ('PITCH', [('f_cut', 400, 3000)]), ECHO], (0.43, 0.85, 0.5, 0.3))
preset(C, 'Hologram', 'A shimmering, slightly detuned double that does not quite exist.',
       M(mod(0, 0.9, 0.9, 0.3, 0.6), pit(0.4, -0.4, 0.4, 0.4, 0.7), filt(1, 300, 0.1), rev(0.4, 0.7, 5.0, 0.2, 0.35)),
       [('FLICKER', [('m_mix', 0.0, 1.0), ('m_rate', 0.2, 4.0)]), ('DETUNE', [('p_a', 0.0, 1.0), ('p_b', 0.0, -1.0)]), GLOW, ('LIGHT', [('f_cut', 50, 2000)])])
preset(C, 'Time Warp', 'Time bends: deep slow pitch drift and half-bar tape stops.',
       M(mod(6, 0.15, 1.0, 0, 1.0), stut(2, 1, 0.3), rev(0.35, 0.7, 4.0, 0.4)),
       [('WARP', [('m_depth', 0.0, 1.0)]), ('SPEED', [('m_rate', 0.05, 1.0)]), ('STOPS', [('s_amt', 0.0, 0.9)]), SPACE])
preset(C, 'Quantum Glitch', 'Reversed 1/32 slivers, folded metal and scattered grains.',
       M(stut(3, 5, 0.5), drv(2, 0.5, 0.6, 0.7), grn(0.4, 30, 40, 0, 0.6, 0.5, 0.8)),
       [('GLITCH', [('s_amt', 0.0, 1.0)]), ('FOLD', [('d_amt', 0.0, 1.0)]), ('PARTICLES', [('g_mix', 0.0, 1.0)]), ('CHAOS', [('g_jit', 0.0, 1.0)])])
preset(C, 'Event Horizon', 'Endless whole-bar echoes collapsing into a dark, swirling pit.',
       M(dly(8, 0.88, 0.45, 1, 0.15, 0.3), mod(1, 0.03, 0.7, 0.6, 0.4), pit(-12, -24, 0.25, 0.0, 1.0)),
       [('COLLAPSE', [('y_fb', 0.4, 0.94)]), ('ECHO', [('y_mix', 0.0, 0.7)]), ('SWIRL', [('m_mix', 0.0, 0.9)]), ('GRAVITY', [('p_mix_a', 0.0, 0.8)])])
preset(C, 'Cyborg', 'A low ring-mod buzz through a narrow band and fuzz: half human, half machine.',
       M(mod(5, 1.2, 1.0, 0, 0.6), filt(2, 1500, 0.5), drv(3, 0.45, 0.6)),
       [('MACHINE', [('m_mix', 0.0, 1.0)]), ('BUZZ', [('m_rate', 0.3, 4.0)]), ('FUZZ', [('d_amt', 0.0, 1.0)]), ('BAND', [('f_cut', 500, 4000)])])
preset(C, 'Signal Lost', 'A transmission breaking up: crushed to 3 bits, buzzing and drowning in noise.',
       M(lofi(3, 7000, 0, 0.3, 0.35, 0.1, 0.5), mod(3, 30.0, 0.7, 0, 1.0)),
       [('LOSS', [('l_bits', 10, 2)]), ('NOISE', [('l_noise', 0.0, 1.0)]), ('BUZZ', [('m_depth', 0.0, 1.0)]), ('RATE', [('l_rate', 20000, 1500)])])

# =============================================================================================
def fmt(v):
    s = repr(float(v))
    return s + 'f' if ('.' in s or 'e' in s) else s + '.0f'

def cstr(s):
    return '"' + s.replace('\\', '\\\\').replace('"', '\\"') + '"'

def main():
    trims = {}
    tfile = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, 'trims.txt')   # level-match result, by index
    if os.path.exists(tfile):
        for line in open(tfile):
            a = line.split()
            if len(a) >= 2: trims[int(a[0])] = float(a[1])
    out = ['// GENERATED by tools/make_presets.py -- edit the recipes there, not this file', '#pragma once', '', 'namespace lm {', '']
    for i, p in enumerate(P):
        vals = ', '.join('{ %s, %s }' % (SYM[k], fmt(v)) for k, v in p['vals'].items())
        out.append('inline constexpr PresetVal kPV%d[] = { %s };' % (i, vals))
    out.append('')
    out.append('inline constexpr Preset kPresets[] = {')
    for i, p in enumerate(P):
        ms = []
        for lab, tg in p['macros']:
            ts = ', '.join('{ %s, %s, %s }' % (SYM[t[0]], fmt(t[1]), fmt(t[2])) for t in tg)
            pad = ', '.join(['{ 0, 0, 0 }'] * (4 - len(tg)))
            ms.append('{ %s, %d, { %s } }' % (cstr(lab), len(tg), ', '.join(x for x in [ts, pad] if x)))
        md = ', '.join(fmt(x) for x in p['mdef'])
        out.append('    { %s, %d, %s, kPV%d, %d, { %s }, { %s }, %s },' % (cstr(p['name']), p['cat'], cstr(p['desc']), i, len(p['vals']),
                                                                    ', '.join(ms), md, fmt(trims.get(i, 0.0))))
    out.append('};')
    out.append('constexpr int kNumPresets = %d;' % len(P))
    out.append('')
    out.append('} // namespace lm')
    open(os.path.join(HERE, '..', 'LumenPresets.hpp'), 'w').write('\n'.join(out) + '\n')
    print('presets:', len(P), ' per category:', [sum(1 for p in P if p['cat'] == c) for c in range(12)])

main()
