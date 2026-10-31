"""Builds the Lumen manual (PDF) with reportlab.
   python3 docs/make_lumen_manual.py out.pdf screenshot_sounds.png screenshot_edit.png"""
import os, re, sys
from reportlab.lib.pagesizes import A4
from reportlab.lib import colors
from reportlab.lib.units import mm
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
from reportlab.platypus import SimpleDocTemplate, Paragraph, Spacer, Image, Table, TableStyle, CondPageBreak

out, shot, shot2, shot3 = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4]
ACC = colors.HexColor("#d9577a"); INK = colors.HexColor("#1c1220"); DIM = colors.HexColor("#6f5a6c")
ss = getSampleStyleSheet()
H2 = ParagraphStyle("H2", parent=ss["Heading2"], textColor=ACC, fontSize=13.5, spaceBefore=10, spaceAfter=4)
H3 = ParagraphStyle("H3", parent=ss["Heading3"], textColor=INK, fontSize=10.5, spaceBefore=6, spaceAfter=2)
B = ParagraphStyle("B", parent=ss["BodyText"], fontSize=9.6, leading=13.4, textColor=INK)
S = ParagraphStyle("S", parent=B, fontSize=8.4, textColor=DIM, leading=11.2)
TITLE = ParagraphStyle("T", parent=ss["Heading1"], fontSize=34, leading=40, textColor=INK, spaceAfter=4)
TH = ParagraphStyle("th", parent=B, textColor=colors.white, fontName="Helvetica-Bold")
P = lambda t: Paragraph(t, B)

def table(rows, widths, style=B):
    t = Table([[Paragraph(c, style if i else TH) for c in r] for i, r in enumerate(rows)], colWidths=widths, repeatRows=1)
    t.setStyle(TableStyle([("BACKGROUND", (0, 0), (-1, 0), INK), ("ROWBACKGROUNDS", (0, 1), (-1, -1), [colors.HexColor("#f8eef2"), colors.white]),
                           ("VALIGN", (0, 0), (-1, -1), "TOP"), ("LINEBELOW", (0, 0), (-1, -1), 0.25, colors.HexColor("#e6d3dc")),
                           ("LEFTPADDING", (0, 0), (-1, -1), 6), ("RIGHTPADDING", (0, 0), (-1, -1), 6)]))
    return t

story = [Paragraph("Lumen", TITLE), Paragraph("Sample effect instrument &mdash; user manual, version 1.5", S), Spacer(1, 4 * mm),
         Image(shot, width=165 * mm, height=165 * mm * 760 / 1100), Spacer(1, 3 * mm)]

story += [Paragraph("What it is", H2), P(
    "Lumen plays your sample through <b>267 effect sounds in 13 categories</b>, plus your own: "
    "granular clouds, shimmering spaces, pads, rhythmic glitches, tape and lo-fi, pitch and harmony, filter motion and drive, breakcore, dreamy, cinematic and sci-fi sounds &mdash; and <b>Beautiful Transforms</b>: "
    "sounds that turn the sample into a choir, strings, glass, bells or wind chimes. Version 1.5 adds 48 sounds made with four new engine parts: tuned strings and bells (COMB), a talking filter that shapes your sample into vowels (VOWEL), octave fuzz (RECTIFY) and a frequency shifter that makes endless rising and falling spirals (SHIFT). "
    "Pick a sound, turn the four big knobs, and drag the result straight into FL Studio. "
    "It is an instrument (it shows up with the instruments, not under effects). VST3 and CLAP for Windows 10/11 (64-bit). "
    "Loads WAV, AIFF, FLAC, MP3, OGG, Opus, M4A/AAC, ALAC and WMA, and the sound of video files (MP4, MOV, MKV, WEBM, AVI, WMV): "
    "drop a video and Lumen takes its audio.")]

story += [Paragraph("New in 1.5", H2), P(
    "<b>Drag and drop:</b> drag a sample from FL Studio's browser on the left (or from Explorer, the desktop, or anywhere else) straight onto the Lumen window. "
    "The window lights up with <i>DROP TO LOAD</i>; let go and the sample is loaded. The sound you have chosen stays the same.<br/>"
    "<b>48 new sounds</b> with four new engine parts (see EDIT below). The first time you open Lumen 1.5 it shows the new category <b>NEW IN 1.5</b>. "
    "Every new sound has a <b>NEW</b> badge until you have tried it once, and categories that still hold untried sounds show their count in a small glowing pill. "
    "Which sounds you have tried is kept in <font face='Courier'>Documents\\Lumen\\tried.txt</font>.")]

story += [Paragraph("Install", H2), P(
    "Run <b>Lumen-1.5-Windows-Setup.exe</b> (installing over any older Lumen is fine, and your projects keep their sounds; close your DAW first). It installs Lumen.vst3 to <font face='Courier'>C:\\Program Files\\Common Files\\VST3</font>, "
    "Lumen.clap to <font face='Courier'>...\\Common Files\\CLAP</font> and this manual to <font face='Courier'>C:\\Program Files\\Lumen</font>. "
    "<b>FL Studio:</b> Options &rarr; Manage plugins &rarr; <i>Find installed plugins</i>; Lumen appears with the instruments (Generators). "
    "SmartScreen may warn because the installer isn't code-signed: <i>More info</i> &rarr; <i>Run anyway</i>.")]

story += [Paragraph("Quick start", H2), P(
    "1. <b>Drag a sample</b> from FL Studio's browser (or anywhere) onto Lumen, or press <b>LOAD</b> and pick it (Lumen starts with a built-in piano-and-beat loop so you can try it right away).<br/>"
    "2. Press <b>Play</b> (the round button at the top). The sample plays from start to end; with <b>LOOP ON</b> it repeats.<br/>"
    "3. Click a <b>category</b> on the left, then click a <b>sound</b>. You hear it immediately.<br/>"
    "4. Turn the <b>four big knobs</b> &mdash; their names change with every sound (e.g. SHIMMER, SIZE, WOBBLE, AGE).<br/>"
    "5. <b>DRY / WET</b> blends from your plain sample to the full effect. <b>ORIGINAL</b> (top left) lets you compare with the untouched sample.<br/>"
    "6. Grab <b>DRAG TO DAW</b> and drop it into FL Studio's playlist: you get your sample with the effect as a WAV.<br/>"
    "7. Want the sound to change during the sample? Open the <b>TIMELINE</b> tab (see below).")]

story += [Paragraph("The main screen", H2), table([
    ["Part", "What it does"],
    ["ORIGINAL", "Bypasses every effect: you hear the sample exactly as it is. Click again for the effect."],
    ["Sound name, arrows", "The current sound and its category. The arrow buttons (or the mouse wheel over the name, or clicking the name) step "
                                  "through the sounds of the selected category."],
    ["SURPRISE", "Jumps to a random sound."],
    ["Play / STOP", "Plays the sample from the start. With LOOP OFF it plays once and the button pops out at the end. "
             "While anything can be heard (Play, MIDI keys, or echoes and reverb still ringing) the button shows a square: <b>STOP</b> cuts everything "
             "to silence at once (with a 10 ms fade so it doesn't click)."],
    ["DRAG TO DAW", "Press and drag into FL Studio: you get the sample through the current sound (plus the reverb/echo tail) as a 24-bit WAV. "
                    "It is rendered in the background as soon as your mouse reaches the tile (a small bar shows the progress, then <i>ready to drag</i>), "
                    "so the plugin never freezes. A plain click saves it and opens the folder <font face='Courier'>Documents\\Grainfield Renders</font>."],
    ["SAVE AS OWN PRESET", "Appears at the top right as soon as you change a sound (its knobs, or anything on the EDIT page). Type a name and "
                           "press Enter: the sound goes into <b>OWN PRESETS</b>, the last category, in every project. They are kept in "
                           "<font face='Courier'>Documents\\Lumen\\Own Presets.txt</font> (copy that file to take them to another computer); "
                           "projects also keep a copy of the ones they use. Right-click an own preset twice to remove it."],
    ["LEN 1\u00d7 / 2\u00d7 / 4\u00d7", "How many times the sample plays in the render (for looping sounds and rhythmic effects)."],
    ["Waveform", "<b>Click</b> anywhere between the handles to play from that spot (click again somewhere else to jump there). "
                 "<b>Right-click</b> moves the nearest start or end handle to that spot. You can also drag the two triangle handles; "
                 "right-click a triangle to reset it."],
    ["LOOP", "On: the sample repeats (with a tiny crossfade so the loop doesn't click). Off: it plays once."],
    ["KEYS", "MIDI keys also play the sample. <b>PITCH</b>: like a sampler, C3 = original, other keys transpose. <b>TRIGGER</b>: every key "
             "plays it at the original pitch."],
    ["Categories / sounds", "Thirteen categories (19 to 23 sounds each), NEW IN 1.5, OWN PRESETS, or ALL SOUNDS (scroll with the wheel). A dot marks the category of the current sound. "
                            "<b>NEW</b> badges mark sounds you have not tried yet."],
    ["Four big knobs", "Each sound gives you four macros that change several settings at once. Double-click or right-click resets a knob to the sound's default."],
    ["DRY / WET, OUTPUT", "Blend dry/effect, and the overall volume. All sounds are level-matched so switching doesn't jump in loudness."],
    ["Chain (bottom row)", "The nine effect modules in signal order. Lit = on. Click one to switch it on or off."],
], [34 * mm, 136 * mm])]

story += [CondPageBreak(80 * mm), Paragraph("EDIT: every module", H2), P(
    "The <b>EDIT</b> tab shows all nine modules of the current sound with their own knobs, on/off switches and modes. Changes are heard live. "
    "Knobs with a <b>colored dot</b> are driven by one of the four big macro knobs (same color) &mdash; turn the macro on the SOUNDS page to move them. "
    "Choosing another sound replaces these settings; your sample, LOOP, KEYS, start/end and OUTPUT always stay."),
    Image(shot2, width=165 * mm, height=165 * mm * 760 / 1100), Spacer(1, 2 * mm)]

story += [table([
    ["Module", "Controls"],
    ["1 GRAIN", "A cloud of tiny grains made from the last seconds of sound. MIX, SIZE (10&ndash;800 ms), DENSITY (grains per second), PITCH, "
                "SPREAD (how far back and how wide), REVERSE (share of backwards grains), JITTER (random timing, pitch and octave jumps), "
                "FREEZE (holds the moment forever)."],
    ["2 PITCH", "Two pitch-shifted copies: VOICE A/B (&plusmn;24 semitones) with their LEVELs, plus DRY. Octaves, fifths and chords."],
    ["3 DRIVE", "SOFT, HARD, FOLD, FUZZ, TUBE or RECTIFY (octave-up fuzz) distortion with AMOUNT, TONE and MIX. Level is compensated automatically."],
    ["4 FILTER", "LP / HP / BP / NOTCH with CUTOFF and RESO, an LFO (RATE, DEPTH) and ENVELOPE (opens with loudness, or closes when negative). "
                 "<b>COMB</b> tunes the sound like a plucked string or bell: CUTOFF picks the note (shown as e.g. A2 &middot; 110 Hz), RESO how long it rings. "
                 "<b>VOWEL</b> makes the sample talk: CUTOFF slides through a-e-i-o-u, and the LFO and ENVELOPE make it speak by themselves."],
    ["5 LO-FI", "BITS and RATE (bitcrush, sample-rate reduction), WOW and FLUTTER (tape wobble), HISS, CRACKLE (vinyl) and AGE (old-speaker band limit)."],
    ["6 MOD", "CHORUS, FLANGER, PHASER, TREMOLO, PAN, RING (robot), VIBRATO or <b>SHIFT</b> with RATE, DEPTH, FEEDBACK and MIX. "
              "SHIFT moves every frequency up (DEPTH above the middle) or down (below) by the same number of Hz: metallic, inharmonic, and with FEEDBACK an endless rising or falling spiral."],
    ["7 STUTTER", "Tempo-synced: REPEAT (repeats the last piece), GATE (rhythmic chopping, 8 patterns), TAPE STOP, REVERSE (pieces play backwards), "
                  "HALF (half speed) and STUTTER (quick rolls; PATTERN picks 1/16, 1/32 or 1/64 rolls, triplets, speeding up, slowing down, RIPPLE (rising steps) or SCATTER). DIVISION sets the piece length, AMOUNT how often (or how deep for GATE)."],
    ["8 DELAY", "Tempo-synced echo (1/32 to 1/1, dotted values), FEEDBACK, TONE, WOBBLE, MIX and PING-PONG."],
    ["9 REVERB", "SIZE, DECAY (up to 30 s), DAMPING, PREDELAY, SHIMMER (the tail rises an octave), MOTION and MIX."],
], [24 * mm, 146 * mm])]

story += [CondPageBreak(110 * mm), Paragraph("TIMELINE: fade from one sound to the next", H2), P(
    "The <b>TIMELINE</b> tab lets the sound change while the sample plays: sound A at the start, B in the middle, C at the end, each one "
    "fading smoothly into the next. Up to 32 sounds."),
    Image(shot3, width=165 * mm, height=165 * mm * 760 / 1100), Spacer(1, 2 * mm), P(
    "<b>In four clicks:</b> open TIMELINE (it starts with your current sound as section 1) &rarr; <b>double-click</b> in the waveform where "
    "sound B should start &rarr; click B in the list below &rarr; double-click where C should start and click C. Press Play: A fades into B, "
    "B into C. The colored strip under the big waveform at the top shows your sections on every page."), table([
    ["Control", "What it does"],
    ["Double-click", "Adds a new section at that spot. It starts with the same sound and is selected, so the next sound you click goes to it."],
    ["Click a section", "Selects it: the list below and the four knobs on the right now belong to that section."],
    ["Play buttons", "Every sound in the list and every section label has a small play button. It gives that sound to the selected section "
                     "and plays it right away &mdash; click through sounds until you find the right one. <b>PREVIEW: FROM SECTION</b> starts "
                     "at the start of that section, <b>FROM START</b> plays the sample from the beginning, so you hear the fade into it."],
    ["Drag a line", "Moves where a section starts. Right-click a line to remove the section after it (REMOVE removes the selected one)."],
    ["White dots", "Each fade has two white dots at the bottom. Drag them apart for a long, slow fade, together for a quick one. "
                   "All the way in is a hard cut. The colored band between two sections shows the fade."],
    ["FADES", "Sets every fade at once (0 % = hard cuts, 100 % = each fade reaches into the middle of its neighbours)."],
    ["CURVE", "<b>SMOOTH</b>: gentle, constant loudness. <b>LINEAR</b>: a straight blend. <b>SHARP</b>: the change happens quickly in the middle of the fade."],
    ["TAILS", "<b>RING</b>: echoes and reverb of the old sound die out naturally over the new one. <b>CUT</b>: the old sound stops with its fade."],
    ["Four knobs", "Shape the selected section only (each section remembers its own settings)."],
    ["EDIT", "With the timeline on, the EDIT tab changes the <b>selected section's</b> sound only (its name gets a *). "
             "Double-click a knob to go back to the sound's own value. SAVE AS OWN PRESET keeps the result."],
    ["+ ADD SOUND", "Splits the longest section in the middle (up to 32 sections)."],
    ["ON", "Switches the timeline off to hear the single sound again, and back on. Your sections are kept."],
], [30 * mm, 140 * mm]), P(
    "The timeline follows the sample: with LOOP on it starts again at section 1 on every pass, MIDI keys play it too, and DRAG TO DAW renders "
    "it with all the fades. It is saved with your project and stays when you browse sounds. While it is on, clicking a sound on the SOUNDS page "
    "gives it to the selected section, and the header arrows and SURPRISE change that section's sound.")]

story += [Paragraph("Tempo", H2), P(
    "STUTTER and DELAY follow FL Studio's tempo. While the song plays they lock to the song position; when it is stopped they count from "
    "the moment the sample starts. Renders use the project tempo.")]

story += [Paragraph("Tips and troubleshooting", H2), P(
    "<b>No sound:</b> press Play or play a key (C3 = original). Check DRY / WET and OUTPUT. "
    "<b>Too much effect:</b> lower DRY / WET, or compare with ORIGINAL. <b>Clicks at the loop point:</b> move the start/end handles to quieter spots. "
    "<b>Project reload:</b> the sample path is saved with the project, so keep the file where it is. "
    "<b>Drop does nothing:</b> drop audio or video files only (folders and other files are ignored). If FL Studio runs as administrator, Windows blocks dragging from Explorer into it; dragging from FL's own browser still works. "
    "Renders go to <font face='Courier'>Documents\\Grainfield Renders</font>.")]

# ---- all sounds
src = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'plugins', 'Lumen', 'LumenPresets.hpp')).read()
cats = ["Granular", "Shimmer & Space", "Pads & Textures", "Rhythm & Glitch", "Lo-Fi & Tape", "Pitch & Harmony", "Filter & Motion", "Drive & Character", "Breakcore & Chaos", "Dreamy & Nostalgic", "Cinematic & Dark", "Sci-Fi & Experimental", "Beautiful Transforms"]
items = re.findall(r'\{ "((?:[^"\\]|\\.)*)", (\d+), "((?:[^"\\]|\\.)*)", kPV', src)
story += [CondPageBreak(60 * mm), Paragraph("All %d sounds" % len(items), H2)]
for c, cname in enumerate(cats):
    rows = [["Sound", "What it is"]] + [[n, d.replace('\\"', '"')] for n, cc, d in items if int(cc) == c]
    story += [CondPageBreak(40 * mm), Paragraph(cname, H3), table(rows, [42 * mm, 128 * mm], S)]

story += [Paragraph("Credits", H2), Paragraph(
    "Lumen is an original instrument by Grainfield Audio. Built with DPF (ISC), pugl (ISC), NanoVG (zlib), the Inter typeface (SIL OFL) and "
    "dr_wav / dr_flac / dr_mp3 (public domain / MIT-0). VST is a trademark of Steinberg Media Technologies GmbH. Licence texts: THIRD-PARTY.txt.", S)]

def bg(c, d):
    c.saveState(); c.setFillColor(ACC); c.rect(0, A4[1] - 6, A4[0], 6, stroke=0, fill=1)
    c.setFont("Helvetica", 8); c.setFillColor(DIM); c.drawRightString(A4[0] - 20 * mm, 10 * mm, "Lumen 1.5  \u00b7  page %d" % d.page); c.restoreState()

SimpleDocTemplate(out, pagesize=A4, leftMargin=20 * mm, rightMargin=20 * mm, topMargin=15 * mm, bottomMargin=15 * mm,
                  title="Lumen Manual", author="Grainfield Audio").build(story, onFirstPage=bg, onLaterPages=bg)
print("wrote", out)
