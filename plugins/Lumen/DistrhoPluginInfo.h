#ifndef DISTRHO_PLUGIN_INFO_H_INCLUDED
#define DISTRHO_PLUGIN_INFO_H_INCLUDED

#define DISTRHO_PLUGIN_BRAND   "Grainfield Audio"
#define DISTRHO_PLUGIN_NAME    "Lumen"
#define DISTRHO_PLUGIN_URI     "https://grainfield.audio/plugins/lumen"
#define DISTRHO_PLUGIN_CLAP_ID "audio.grainfield.lumen"

#define DISTRHO_PLUGIN_BRAND_ID  GrFa
#define DISTRHO_PLUGIN_UNIQUE_ID GrLm

#define DISTRHO_PLUGIN_HAS_UI            1
#define DISTRHO_PLUGIN_IS_RT_SAFE        1
#define DISTRHO_PLUGIN_IS_SYNTH          1
#define DISTRHO_PLUGIN_NUM_INPUTS        0
#define DISTRHO_PLUGIN_NUM_OUTPUTS       2
#define DISTRHO_PLUGIN_WANT_MIDI_INPUT   1
#define DISTRHO_PLUGIN_WANT_TIMEPOS      1
#define DISTRHO_PLUGIN_WANT_STATE        1
#define DISTRHO_PLUGIN_WANT_FULL_STATE   1
#define DISTRHO_PLUGIN_WANT_PROGRAMS     1
#define DISTRHO_UI_USE_NANOVG            1
#define DISTRHO_UI_FILE_BROWSER          1
#define DISTRHO_UI_USER_RESIZABLE        1
#define DISTRHO_UI_DEFAULT_WIDTH         1100
#define DISTRHO_UI_DEFAULT_HEIGHT        760

#define DISTRHO_PLUGIN_VST3_CATEGORIES "Instrument|Sampler"
#define DISTRHO_PLUGIN_CLAP_FEATURES   "instrument", "sampler", "stereo"
#define DISTRHO_PLUGIN_DESCRIPTION     "Sample effect instrument: load a sound and play it through 219 effect presets."

#endif
