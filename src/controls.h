/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stdint.h>
typedef struct {
  const char *name;
  uint8_t max, format;
} GDControl;
enum {
  F_NUMBER,
  F_NOTE,
  F_TIME,
  F_HZ,
  F_BIPOLAR,
  F_SOURCE,
  F_MATERIAL,
  F_TUNING,
  F_TARGET,
  F_WAVE,
  F_BOOL,
  F_PAN,
  F_PERCENT,
  F_TRANSPOSE,
  F_WIDTH,
  F_KICK=64,
  F_AC=96,
  F_AN=128,
  F_GL=160,
  F_PERC=192
};
static const GDControl gd_gctrl[32] = {{"SOURCE 1", 2, F_SOURCE},
                                       {"MATERIAL 1", 7, F_MATERIAL},
                                       {"TUNING 1", 1, F_TUNING},
                                       {"NOTE 1", 108, F_NOTE},
                                       {"SIZE 1", 127, F_PERCENT},
                                       {"POSITION 1", 127, F_PERCENT},
                                       {"EDGE 1", 127, F_PERCENT},
                                       {"FROZEN 1", 127, F_PERCENT},
                                       {"FRESH NOISE", 127, F_PERCENT},
                                       {"NOISE COLOR", 127, F_BIPOLAR},
                                       {"FOLD", 127, F_PERCENT},
                                       {"DRIVE", 127, F_PERCENT},
                                       {"CUTOFF", 127, F_HZ},
                                       {"RESONANCE", 127, F_PERCENT},
                                       {"MOTION TO", 3, F_TARGET},
                                       {"MOTION", 127, F_BIPOLAR},
                                       {"MOTION DEC", 127, F_TIME},
                                       {"ATTACK", 101, F_TIME},
                                       {"HOLD", 127, F_NUMBER},
                                       {"DECAY", 127, F_TIME},
                                       {"SOURCE 2", 2, F_SOURCE},
                                       {"MATERIAL 2", 7, F_MATERIAL},
                                       {"TUNING 2", 1, F_TUNING},
                                       {"NOTE 2", 108, F_NOTE},
                                       {"SIZE 2", 127, F_PERCENT},
                                       {"POSITION 2", 127, F_PERCENT},
                                       {"EDGE 2", 127, F_PERCENT},
                                       {"FROZEN 2", 127, F_PERCENT},
                                       {"OSC MIX", 127, F_PERCENT},
                                       {"LEVEL", 127, F_PERCENT},
                                       {"PAN", 127, F_PAN},
                                       {"TRANSPOSE", 127, F_TRANSPOSE}};
static const GDControl gd_actrl[32]={
{"VCO 1",96,F_AN+0},
{"VCO 2",96,F_AN+1},
{"WAVE 1",1,F_WAVE},
{"WAVE 2",1,F_WAVE},
{"FM",127,F_AN+4},
{"OSC MIX",127,F_AN+5},
{"NOISE",127,F_AN+6},
{"PITCH EG",127,F_AN+7},
{"PITCH DEC",127,F_AN+8},
{"CUTOFF",127,F_AN+9},
{"RESONANCE",127,F_AN+10},
{"FILTER EG",127,F_AN+11},
{"FILTER DEC",127,F_AN+12},
{"VCF MOD",127,F_AN+13},
{"AMP DEC",127,F_AN+14},
{"VCA ATTACK",127,F_AN+15},
{"VCO2 FINE",127,F_AN+16},
{"PITCH TO VCF",127,F_AN+17},
{"VCF DRIVE",127,F_AN+18},
{"VCA CURVE",127,F_AN+19},
{"VCF ATTACK",127,F_AN+20},
{"VCF CURVE",127,F_AN+21},
{"NOISE COLOR",127,F_AN+22},
{"DECAY MACRO",127,F_AN+23},
{"TUNE",127,F_AN+24},
{"SUSTAIN",127,F_AN+25},
{"RELEASE",127,F_AN+26},
{"HOLD MODE",1,F_BOOL},
{"EG TO NOISE",127,F_AN+28},
{"LEVEL",127,F_PERCENT},
{"PAN",127,F_PAN},
{"TRANSPOSE",127,F_TRANSPOSE},
};
static const GDControl gd_basic[4] = {{"NOTE", 96, F_NOTE},
                                      {"DECAY", 127, F_TIME},
                                      {"TONE", 127, F_PERCENT},
                                      {"DRIVE", 127, F_PERCENT}};
static const uint8_t gd_analog_pages[8][4]={{0,1,2,3},{4,5,6,7},{8,9,10,11},{12,13,14,15},{16,18,19,20},{21,22,23,24},{25,26,27,28},{17,29,30,31}};
static const char *const gd_lane_names[8] = {"KICK",     "SNARE", "CLOSED HAT",
                                             "OPEN HAT", "CLAP",  "PRC",
                                             "GLITCH",   "ANALOG"};
static const char *const gd_material_names[8] = {"COMPLEX", "TRANSNT", "METAL",
                                                 "TONAL",   "DUST",    "HOLLOW",
                                                 "DIGITAL", "GLASS"};
static const char *const gd_lfo_waves[5] = {"SINE", "TRI", "SQUARE", "SAW",
                                            "RANDOM"};
static const char *const gd_lfo_rates[12] = {
    "4 BARS", "2 BARS", "1 BAR", "1/2",  "1/4",  "1/8",
    "1/16",   "1/32",   "1/64",  "3/16", "3/32", "3/64"};
static const char *const gd_dest[9] = {"OFF",     "PITCH",   "CUTOFF",
                                       "SIZE/FM", "POS/MIX", "FOLD/DRV",
                                       "PAN",     "LEVEL",   "OSC MIX"};
static inline unsigned gd_master_fx_index(unsigned page,unsigned knob){static const uint8_t ids[3][4]={{0,1,2,3},{7,8,4,5},{6,9,10,11}};return ids[page%3][knob%4];}
static const GDControl gd_fxctrl[12] = {
    {"DELAY MIX", 127, F_PERCENT},  {"DLY SYNC", 5, F_NUMBER},
    {"FEEDBACK", 110, F_PERCENT},   {"DAMP", 127, F_PERCENT},
    {"DRIVE", 127, F_PERCENT},      {"BIT DEPTH", 12, F_NUMBER},
    {"HOLD FRAMES", 127, F_NUMBER}, {"CUTOFF", 127, F_HZ},
    {"RESONANCE", 127, F_PERCENT},  {"GLITCH RATE", 127, F_NUMBER},
    {"GLITCH MIX", 127, F_PERCENT},   {"WIDTH %", 127, F_WIDTH}};
