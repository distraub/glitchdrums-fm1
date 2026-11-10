/* SPDX-License-Identifier: GPL-3.0-only */
#include "controls.h"
#include "engine.h"
#include "fm1.h"
#include "fx.h"
#include "keys.h"
#include "punch.h"
#include "presets.h"
#include "themes.h"
#include "patterns.h"
#include "remix.h"
#include "storage_fullfx.h"
_Static_assert(sizeof(GDSave) <= SETTINGS_MAX,
               "Project exceeds platform settings storage");

/* Editor owns ui. Only audio owns live, voice state, sequencing, LFOs and FX.
 * Single-producer/single-consumer commands are published by one 32-bit store.
 * Queue cells/counters are volatile: the documented single-core FM-1 IRQ
 * contract orders each cell before its index, without libatomic dependencies.
 * RemoteFM1 serializes all entry points; this is not a multi-core queue.
 */
static GDSave storage;
static union {GDFullSave current;GDMidiSave four;GDChainSave chain;GDOldSave prefix;GDOldABSave ab;GDOldEightSave eight;GDOldSongSave song;} save_scratch;
#define chain_save save_scratch.current.base
#define pattern_save chain_save.base
static GDPattern pattern_bank[16] POOL,pattern_mailbox POOL;
static uint8_t live_chord_count[2][16];
static int8_t live_chord_pitch[2][16][3];
static uint8_t voice_bind[8][2],kit_bind=4,scene_bind=4;
static unsigned import_overflow,import_split;
static GDMidiMap midi_map[16];
static GDMidiTarget midi_target={M_INVALID,0};
static unsigned midi_learn_on,midi_pick,midi_map_cursor,midi_return_view,midi_return_page,midi_fx_held,midi_ui_dirty,midi_header_dirty;
static uint32_t midi_refresh_at;

static GDChain chain_ui,chain_audio;
static GDChainClock chain_clock;
static volatile uint32_t chain_running,chain_generation,chain_pattern,chain_row,chain_pass,chain_finished;
static unsigned chain_seen,chain_cursor;

static void refresh_pattern_size(unsigned slot){(void)slot;}
static volatile uint32_t pattern_busy;
static unsigned active_pattern,pattern_cursor,queued_pattern;

static uint32_t pattern_armed,pattern_immediate;
static unsigned pattern_last_key=255;
static uint32_t pattern_last_tap;
#define ui storage.project
#define preset_bank storage.bank
static GDProject live;
static GDKit recall_mailbox[2],compare_kit;
static uint8_t recall_sends[2][8],compare_sends[8];
static volatile uint32_t recall_busy[2];
static unsigned preset_type,preset_slot,compare_type,compare_lane,compare_valid;
static GDEngine engine;
static GDFX effects;
static GDPunch punch;
static GDPunchGate punch_gate;
static volatile uint32_t punch_request=0x80000000u,punch_active,display_lane=6;
static volatile uint32_t lfo_phase_monitor[8][2],level_monitor[8],scope_write;
static volatile int16_t lfo_value_monitor[8][2],scope_monitor[128];

static volatile uint32_t queue[256], q_write, q_read;
static uint32_t playing, recording, sequence_mode, selected, page, edit_page,
    view, lfo, lfo_display;
static int keyboard_octave;
static int8_t held_pitch[27];static uint8_t held_lane[27],held_note[27];
static uint8_t delay_send[8];
static unsigned old_swing_format=1;
static unsigned roll_ui,old_roll_format,glitch_key_ui,glitch_prepared,glitch_base=1;
static int glitch_offset;
static uint32_t glitch_key_stamp[16],glitch_key_serial;
static GDRemix remix_ui={.depth=127,.dirty=1},remix_audio={.depth=127,.dirty=1};
static unsigned remix_latch,remix_active,remix_print_slot=255;
static uint32_t remix_print_at,remix_print_keys;
static volatile uint32_t clockhead;
static uint32_t lock_edit,voice_focus=1,mix_mode,mix_bank=1,env_consumed;
static uint8_t voice_row[8],voice_style[8]={1,1,1,1,1,1,1,1},lfo_page[8];
static uint8_t mutation_ui[8][16],mutation_audio[8][16];
static unsigned mutation_state,mutation_audio_valid;
static uint32_t solo_ui,sel_pending,sel_lane,sel_down_at;
static uint32_t punch_key_mask,punch_ui_latched, punch_ui_mask, punch_ui_active, punch_latched, punch_latch_mode, fx_selected=28, fx_bank,fx_slot=1;
static int fx_surface(void);
static volatile uint32_t playhead, audio_running, audio_quiet = 1;
static uint32_t step_selected, pending_save, notice_until;
static const char *notice = "";
static uint32_t seed = 0x571aa13u;
static uint8_t need_grid = 1, need_controls = 1, need_header = 1, need_full = 1;
enum { V_VOICE, V_STEP, V_LFO, V_FX, V_GLOBAL, V_MIX, V_HOME, V_PUNCH, V_FXEDIT, V_PRESETS, V_PATTERNS, V_CHAIN, V_REMIX };
static unsigned fx_return_view=V_PUNCH,master_fx_page,global_page;
static int fx_surface(void){return view==V_PUNCH||view==V_FXEDIT;}
enum {
  C_PARAM = 1,
  C_LFO,
  C_STEP,
  C_CHANCE,
  C_RATCHET,
  C_PITCH,
  C_LOCK,
  C_LENGTH,
  C_MUTE,
  C_FX,
  C_GLOBAL,
  C_HIT,
  C_PLAY,
  C_UNLOCK,
  C_PUNCH, C_EFFECT, C_RECALL, C_SOLO, C_MUTATE, C_MUTATE_SWAP, C_RELEASE, C_PATTERN, C_PATTERN_NOW, C_RECORD_CHORD, C_ASSIGN, C_CHAIN_STOP, C_SEND, C_ROLL_GATE, C_REMIX, C_REMIX_GATE
};
static int send(unsigned kind,unsigned lane,unsigned index,unsigned value);
static void offset_glitch(int delta){
 if(!delta)return;int offset=gd_clamp(glitch_offset+delta,-126,126);unsigned rate=(unsigned)gd_clamp((int)glitch_base+offset,1,127);
 if(send(C_FX,0,9,rate)){glitch_offset=offset;ui.fx[9]=(uint8_t)rate;need_controls=1;need_grid=1;}
}
static void select_remix_mode(unsigned mode){
 if(send(C_REMIX,0,0,mode))remix_ui.mode=(uint8_t)mode;
 need_header=need_controls=1;
}
static void pattern_snapshot(uint8_t snapshot[8][16],const GDProject *p){
 for(unsigned lane=0;lane<8;++lane)for(unsigned step=0;step<16;++step)snapshot[lane][step]=p->track[lane].step[step];
}
static void pattern_swap(uint8_t snapshot[8][16],GDProject *p){
 for(unsigned lane=0;lane<8;++lane)for(unsigned step=0;step<16;++step){uint8_t value=p->track[lane].step[step];p->track[lane].step[step]=snapshot[lane][step];snapshot[lane][step]=value;}
}
static void remember_navigation(void){
 if(view==V_GLOBAL)global_page=page;
 if(view==V_VOICE&&!lock_edit){voice_row[selected]=(uint8_t)page;voice_style[selected]=(uint8_t)voice_focus;}
 if(view==V_LFO)lfo_page[selected]=(uint8_t)lfo_display;
 if(fx_surface()||view==V_FX){fx_return_view=view;if(view==V_FX)master_fx_page=page;}
}
static int send_word(uint32_t word) {
  unsigned kind=word>>24;
  /* A frozen mailbox cannot absorb sequence edits across an IRQ boundary.
   * Shared sound/FX controls remain live while the switch is queued. */
  if((pattern_busy||chain_running||(chain_ui.mode&&playing))&&((kind>=C_STEP&&kind<=C_LENGTH)||kind==C_UNLOCK||kind==C_MUTATE||kind==C_MUTATE_SWAP||kind==C_RECORD_CHORD||(kind==C_GLOBAL&&((word>>8)&255)==2)))return 0;
  uint32_t w = q_write;
  if (w - q_read >= 256)
    return 0;
  queue[w & 255] = word;
  q_write = w + 1;
  return 1;
}
static int send(unsigned kind, unsigned lane, unsigned index, unsigned value) {
  return send_word((kind << 24) | (lane << 16) | (index << 8) | (value & 255));
}
static void message(const char *s) {
  notice = s;
  notice_until = millis() + 1800;
  need_header = 1;
}
/* The mailbox stays immutable until audio acknowledges a boundary switch.
 * UI retains the old pattern until that acknowledgement is observed. */
static void finish_pattern_switch(void){
 if(pattern_busy!=2)return;
 __asm__ volatile("" ::: "memory");
 gd_pattern_get(&pattern_bank[active_pattern],&ui);refresh_pattern_size(active_pattern);gd_pattern_put(&ui,&pattern_mailbox);
 active_pattern=queued_pattern;mutation_state=0;edit_page=step_selected=0;
 pattern_busy=0;need_header=need_grid=need_controls=1;
 message("PATTERN ACTIVE");
}
static void adopt_pattern_audio(const GDPattern *p){
 gd_pattern_put(&live,p);
 for(unsigned l=0;l<2;++l)for(unsigned s=0;s<16;++s){live_chord_count[l][s]=p->chord_count[l][s];for(unsigned n=0;n<3;++n)live_chord_pitch[l][s][n]=p->chord_pitch[l][s][n];}
 mutation_audio_valid=0;
}
static void apply_pattern_audio(void){
 adopt_pattern_audio(&pattern_mailbox);pattern_armed=0;__asm__ volatile("" ::: "memory");pattern_busy=2;
}
static void apply_chain_audio(void){
 unsigned slot=chain_audio.row[chain_clock.row]&15;
 adopt_pattern_audio(&pattern_bank[slot]);chain_pattern=slot;
 __asm__ volatile("" ::: "memory");++chain_generation;
}
static void finish_chain_switch(void){
 unsigned generation=chain_generation;
 if(generation!=chain_seen){
  __asm__ volatile("" ::: "memory");unsigned slot=chain_pattern;gd_pattern_put(&ui,&pattern_bank[slot]);active_pattern=slot;mutation_state=0;edit_page=step_selected=0;chain_seen=generation;
  need_header=need_grid=need_controls=1;
 }
 if(chain_finished){chain_finished=0;playing=0;midi_fx_held=0;punch_latched=0;punch_request=0x80000000u;need_header=need_grid=need_controls=1;message("CHAIN COMPLETE");}
}
static void select_pattern(int copy){
 finish_chain_switch();finish_pattern_switch();
 if(copy&&(chain_running||(chain_ui.mode&&playing))){message("STOP CHAIN TO COPY");return;}
 unsigned override=chain_running||(chain_ui.mode&&playing);
 if(!copy&&override){if(!send(C_CHAIN_STOP,0,0,0)){message("AUDIO QUEUE BUSY");return;}chain_ui.mode=0;need_header=need_controls=1;}
 if(pattern_busy){message("PATTERN SWITCH QUEUED");return;}
 if(pattern_cursor==active_pattern&&!override){message("PATTERN ALREADY ACTIVE");return;}
 if(copy){gd_pattern_copy_chords(&pattern_bank[pattern_cursor],&pattern_bank[active_pattern]);gd_pattern_get(&pattern_bank[pattern_cursor],&ui);refresh_pattern_size(pattern_cursor);message("COPIED TO PATTERN");need_controls=1;return;}
 pattern_mailbox=pattern_bank[pattern_cursor];queued_pattern=pattern_cursor;pattern_busy=1;
 __asm__ volatile("" ::: "memory");
 if(!send(C_PATTERN,0,0,0)){pattern_busy=0;message("AUDIO QUEUE BUSY");return;}
 need_header=need_controls=1;
 message(playing?"QUEUED: NEXT PATTERN":"LOADING PATTERN");
}
static void print_remix(unsigned slot){
 finish_chain_switch();finish_pattern_switch();
 if(pattern_busy){message("WAIT FOR PATTERN SWITCH");return;}
 if(chain_running||(chain_ui.mode&&playing)){message("STOP CHAIN TO PRINT");return;}
 if(slot==active_pattern){message("CHOOSE ANOTHER SLOT");return;}
 if(gd_remix_slot_used(&pattern_bank[slot])&&(remix_print_slot!=slot||(uint32_t)(millis()-remix_print_at)>1500)){
  remix_print_slot=slot;remix_print_at=millis();message("SLOT USED: TAP AGAIN");return;
 }
 GDPattern source=pattern_bank[active_pattern];gd_pattern_get(&source,&ui);
 if(!gd_remix_print(&pattern_bank[slot],&source,&remix_ui)){message("PATH PRINT FAILED");return;}
 refresh_pattern_size(slot);remix_print_slot=255;notice_until=0;
 static char result[]="PRINTED A - PRESS SAVE";result[8]=(char)('A'+slot);message(result);need_controls=1;
}
static void hit(unsigned lane, unsigned velocity, int pitch) {
  send(C_HIT, lane, (unsigned)gd_clamp(pitch + 64, 0, 127), velocity);
}
static void set_play(int on) {
  finish_chain_switch();finish_pattern_switch();
  if(on&&chain_ui.mode&&!chain_running){
    if(pattern_busy){message("WAIT FOR PATTERN SWITCH");return;}
    gd_pattern_get(&pattern_bank[active_pattern],&ui);recording=0;
  }
  if (send(C_PLAY, 0, 0, on)) {
    playing = (uint32_t)on;
    if(!on){midi_fx_held=0;punch_latched=0;punch_request=0x80000000u;}
    need_header = need_grid = 1;
  }
}
static void process_commands(void) {
  uint32_t r = q_read, w = q_write;
  while (r != w) {
    uint32_t c = queue[r++ & 255];
    unsigned kind = c >> 24, l = (c >> 16) & 7, i = (c >> 8) & 255, v = c & 255;
    GDTrack *t = &live.track[l];
    if(kind==C_CHAIN_STOP){chain_running=chain_clock.running=0;}
    if(kind==C_PATTERN){__asm__ volatile("" ::: "memory");if(audio_running)pattern_armed=1;else apply_pattern_audio();}
    if(kind==C_PATTERN_NOW&&pattern_armed)pattern_immediate=1;
    if(kind==C_ASSIGN&&i<4&&v<30){unsigned previous=live.slot[i];for(unsigned n=0;n<4;++n)if(live.slot[n]==v)live.slot[n]=(uint8_t)previous;live.slot[i]=(uint8_t)v;}
    if(kind==C_RELEASE)gd_release(&engine,l,(int)i-64);
    if(kind==C_MUTATE){pattern_snapshot(mutation_audio,&live);mutation_audio_valid=1;gd_mutate_pattern(&live,(c>>16)&255,c&65535);}
    if(kind==C_MUTATE_SWAP&&mutation_audio_valid)pattern_swap(mutation_audio,&live);
    if(kind==C_STEP)mutation_audio_valid=0;
    if (kind == C_PARAM && i < 32)
      t->param[i] = (uint8_t)v;
    if (kind == C_LFO && i < 8) {
      unsigned k = i / 4;
      switch (i % 4) {
      case 0:
        t->lfo[k].shape = (uint8_t)v;
        break;
      case 1:
        t->lfo[k].rate = (uint8_t)v;
        break;
      case 2:
        t->lfo[k].depth = (uint8_t)v;
        break;
      case 3:
        t->lfo[k].target = (uint8_t)v;
        break;
      }
    }
    if (i < 64) {
      if (kind == C_STEP){t->step[i]=(uint8_t)v;if(!v&&l>=6)live_chord_count[l-6][i]=0;}
      if(kind==C_RECORD_CHORD&&l>=6){int pitch=(int)(v&127)-64;
        if(v&128){t->pitch[i]=(int8_t)pitch;live_chord_count[l-6][i]=0;}
        else gd_chord_add(&live_chord_count[l-6][i],live_chord_pitch[l-6][i],t->pitch[i],pitch);
      }
      if (kind == C_CHANCE)
        t->chance[i] = (uint8_t)v;
      if (kind == C_RATCHET)
        t->ratchet[i] = (uint8_t)v;
      if (kind == C_PITCH)
        t->pitch[i] = (int8_t)((int)v - 64);
      if (kind == C_LOCK) {
        t->lock_id[i] = (uint8_t)((c >> 19) & 31);
        t->lock_value[i] = (uint8_t)v;
      }
    }
    if (kind == C_LENGTH)
      t->length = (uint8_t)v;
    if (kind == C_MUTE)
      t->mute = (uint8_t)v;
    if (kind == C_UNLOCK && i < 64)
      t->lock_id[i] = 255;
    if(kind==C_RECALL&&i<2&&v<3){__asm__ volatile("" ::: "memory");gd_apply_preset(&live,&recall_mailbox[i],v,l);for(unsigned lane=0;lane<8;++lane)engine.delay_send[lane]=recall_sends[i][lane];__asm__ volatile("" ::: "memory");recall_busy[i]=0;}
    if(kind==C_SEND&&l<8)engine.delay_send[l]=(uint8_t)v;
    if(kind==C_REMIX){if(i==0)remix_audio.mode=(uint8_t)v;if(i==1)remix_audio.variation=(uint8_t)v;if(i==2)remix_audio.depth=(uint8_t)v;remix_audio.dirty=1;}
    if(kind==C_REMIX_GATE)remix_audio.gate=(uint8_t)v;
    if(kind==C_ROLL_GATE)effects.roll_gate=v?1:0;
    if(kind==C_SOLO)engine.solo_mask=v;
    if(kind==C_EFFECT&&i<120)live.effect[i/4][i%4]=(uint8_t)v;
    if(kind==C_PUNCH&&i<4)live.punch[i]=(uint8_t)v;
    if (kind == C_FX && i < 12)
      live.fx[i] = (uint8_t)v;
    if (kind == C_GLOBAL) {
      if(i==4)live.reserved=(uint8_t)v;
      if(i==5)live.swing=(uint8_t)((live.swing&127u)|(v?128u:0u));
      if (i == 0)
        live.theme = (uint8_t)v;
      if (i == 1)
        live.swing = (uint8_t)((live.swing&128u)|v);
      if (i == 2)for(unsigned lane=0;lane<8;++lane)live.track[lane].length=(uint8_t)gd_clamp((int)v,1,16);
      if (i == 3)
        live.bpm = v;
    }
    if (kind == C_HIT)
      gd_hit(&engine, &live, l, v, (int)i - 64, -1, 0);
    if(kind==C_PLAY){
      audio_running=v?1:0;
      if(v&&chain_ui.mode){chain_audio=chain_ui;gd_chain_start(&chain_clock,&chain_audio);chain_running=1;chain_row=0;chain_pass=1;chain_finished=0;apply_chain_audio();}
      if(!v){chain_running=chain_clock.running=0;if(pattern_armed)apply_pattern_audio();for(unsigned s=0;s<8;++s)if(engine.synth[s].kind==7&&engine.synth[s].held)engine.synth[s].releasing=1;}
    }
  }
  q_read = r;
}

static unsigned preset_count(void){return preset_type==0?10:5;}
static unsigned factory_count(void){return preset_type==0?8:4;}
static int preset_custom(unsigned slot){
 if(preset_type==0){for(unsigned n=0;n<2;++n)if((preset_bank.voice_used[selected]&(1u<<n))&&voice_bind[selected][n]==slot)return (int)n;return -1;}
 return preset_type==1?(preset_bank.kit_used&&kit_bind==slot?0:-1):(preset_bank.scene_used&&scene_bind==slot?0:-1);
}
static int preset_exists(void){return preset_slot<factory_count()||preset_custom(preset_slot)>=0;}
static int publish_preset(const GDKit *kit,unsigned type,unsigned lane,const uint8_t *sends){
 for(unsigned i=0;i<2;++i)if(!recall_busy[i]){
  recall_mailbox[i]=*kit;for(unsigned l=0;l<8;++l)recall_sends[i][l]=sends?sends[l]:delay_send[l];recall_busy[i]=1;
  __asm__ volatile("" ::: "memory");
  if(!send(C_RECALL,lane,i,type)){recall_busy[i]=0;return 0;}
  for(unsigned l=0;l<8;++l)delay_send[l]=recall_sends[i][l];
  gd_apply_preset(&ui,kit,type,lane);if(type){punch_latched=0;punch_request=0x80000000u;fx_selected=gd_keyboard_effect(fx_slot);}need_header=need_controls=need_grid=1;return 1;
 }
 return 0;
}
static void load_preset(void){
 if(!preset_exists()){message("EMPTY USER SLOT");return;}
 GDKit before=gd_get_kit(&ui),kit=before;unsigned n=preset_slot;int custom=preset_custom(n);
 if(preset_type==0)kit.sound[selected]=custom>=0?preset_bank.voice[selected][custom]:gd_factory_sound(selected,n);
 else if(preset_type==1)kit=custom>=0?preset_bank.kit[0]:gd_factory_kit(n);
 else kit.scene=custom>=0?preset_bank.scene[0]:gd_factory_scene(n);
 uint8_t before_sends[8],factory_sends[8];for(unsigned l=0;l<8;++l)before_sends[l]=delay_send[l];
 const uint8_t *sends=0;if(preset_type&&custom<0&&n>0){gd_factory_delay_sends(n,preset_type==1?&kit:0,factory_sends);sends=factory_sends;}
 if(publish_preset(&kit,preset_type,selected,sends)){
  for(unsigned l=0;l<8;++l)compare_sends[l]=before_sends[l];
  compare_kit=before;compare_type=preset_type;compare_lane=selected;compare_valid=1;
  message("LOADED - ARP COMPARES");
 }else message("AUDIO QUEUE BUSY");
}
static void load_init_voice(void){
 GDKit before=gd_get_kit(&ui),kit=before;kit.sound[selected]=gd_init_sound(selected);
 if(publish_preset(&kit,0,selected,0)){
  compare_kit=before;compare_type=0;compare_lane=selected;compare_valid=1;
  message("INIT VOICE - ARP A/B");
 }else message("AUDIO QUEUE BUSY");
}
static void compare_preset(void){
 if(!compare_valid){message("LOAD A PRESET FIRST");return;}
 GDKit now=gd_get_kit(&ui);uint8_t now_sends[8];for(unsigned l=0;l<8;++l)now_sends[l]=delay_send[l];
 if(publish_preset(&compare_kit,compare_type,compare_lane,compare_type?compare_sends:0)){compare_kit=now;for(unsigned l=0;l<8;++l)compare_sends[l]=now_sends[l];message("A/B SOUND SWAPPED");}
 else message("AUDIO QUEUE BUSY");
}
/* Also used by the host's explicit/background flash synchronization hook.
 * Flush only an explicit SAVE request; ordinary edits remain unsaved. */
int gd_flush_pending_save(void) {
  if (!pending_save) return 1;
  finish_chain_switch();finish_pattern_switch();
  if(import_overflow){pending_save=0;message("OLD BANK >16 BARS: NO SAVE");return 0;}
  if(!chain_running)gd_pattern_get(&pattern_bank[active_pattern],&ui);
  save_scratch.current=(GDFullSave){0};pattern_save.magic=GD16_SAVE_MAGIC;pattern_save.version=8;
  pattern_save.active=(uint8_t)active_pattern;pattern_save.theme=ui.theme;pattern_save.swing=ui.swing;pattern_save.quality=ui.reserved;pattern_save.bpm=(uint16_t)ui.bpm;
  GDKit kit=gd_get_kit(&ui);pattern_save.project=gd_pack_full_kit(&kit);
  for(unsigned l=0;l<8;++l){pattern_save.voice_used[l]=preset_bank.voice_used[l]&3;for(unsigned n=0;n<2;++n){pattern_save.voice[l][n]=gd_pack_sound(&preset_bank.voice[l][n]);pattern_save.voice_bind[l][n]=voice_bind[l][n];}}
  pattern_save.kit_used=preset_bank.kit_used&1;pattern_save.scene_used=preset_bank.scene_used&1;pattern_save.kit_bind=kit_bind;pattern_save.scene_bind=scene_bind;
  pattern_save.kit=gd_pack_full_kit(&preset_bank.kit[0]);pattern_save.scene=gd_pack_full_scene(&preset_bank.scene[0]);
  for(unsigned slot=0;slot<16;++slot)if(!gd_pattern_encode(&pattern_bank[slot],pattern_save.pattern[slot],GD_PATTERN_BYTES)){pending_save=0;message("INVALID PATTERN - NO SAVE");return 0;}
  for(unsigned lane=0;lane<8;++lane)save_scratch.current.delay_send[lane]=delay_send[lane];
  chain_save.chain=chain_ui;for(unsigned n=0;n<16;++n)save_scratch.current.map[n]=midi_map[n];
  if (settings_save(&save_scratch.current, sizeof save_scratch.current) != 0) {
    message("SAVE FAILED");
    return 0;
  }
  pending_save = 0;
  message("PROJECT + THEME SAVED");
  return 1;
}

static void store_preset(void){
 int custom=preset_custom(preset_slot);
 if(button_held(BTN_ENV)){
  env_consumed=1;
  if(custom>=0){if(preset_type==0)preset_bank.voice_used[selected]&=(uint8_t)~(1u<<custom);else if(preset_type==1)preset_bank.kit_used=0;else preset_bank.scene_used=0;pending_save=1;}
  if(preset_slot<factory_count())load_preset();message("ORIGINAL / EMPTY RESTORED");need_controls=1;return;
 }
 if(preset_type==0){
  if(custom<0)for(unsigned n=0;n<2;++n)if(!(preset_bank.voice_used[selected]&(1u<<n))){custom=(int)n;break;}
  if(custom<0){message("2 CUSTOMS FULL: CLEAR ONE");return;}
  preset_bank.voice[selected][custom]=gd_get_sound(&ui,selected);voice_bind[selected][custom]=(uint8_t)preset_slot;preset_bank.voice_used[selected]|=(uint8_t)(1u<<custom);
 }else if(preset_type==1){if(custom<0&&preset_bank.kit_used){message("CUSTOM KIT FULL: CLEAR IT");return;}preset_bank.kit[0]=gd_get_kit(&ui);kit_bind=(uint8_t)preset_slot;preset_bank.kit_used=1;}
 else{if(custom<0&&preset_bank.scene_used){message("CUSTOM FX FULL: CLEAR IT");return;}preset_bank.scene[0]=gd_get_scene(&ui);scene_bind=(uint8_t)preset_slot;preset_bank.scene_used=1;}
 pending_save=1;need_controls=1;message("CUSTOM PRESET SAVED");
}
static void sequence_note(unsigned lane,unsigned velocity,int pitch,int lock,unsigned value){
 gd_hit(&engine,&live,lane,velocity,pitch,lock,(int)value);
 /* HOLD is a keyboard/MIDI gate mode; sequencer steps remain one-shots. */
 if(lane==7)for(unsigned slot=0;slot<8;++slot)if(engine.voices[slot+6].trigger_serial==engine.trigger_serial)engine.synth[slot].held=0;
}
static void sequence_hit(unsigned lane,unsigned velocity,int pitch,int lock,unsigned value,unsigned step){
 sequence_note(lane,velocity,pitch,lock,value);
 if(lane>=6)for(unsigned n=0;n<live_chord_count[lane-6][step];++n)sequence_note(lane,velocity,live_chord_pitch[lane-6][step][n],lock,value);
}
void audio(int32_t *out, uint32_t frames) {
  static uint32_t running, remaining, ordinal, quiet_blocks, clock_ordinal, swing_delay;
  static uint32_t repeat_wait[8], repeat_left[8], repeat_gap[8], repeat_step[8];
  process_commands();
  if(pattern_immediate){
    pattern_immediate=0;
    if(pattern_armed){
      /* Swap the pattern without retriggering the current step or moving its
       * sample countdown. Discard only ratchets belonging to the old pattern. */
      apply_pattern_audio();for(unsigned l=0;l<8;++l)repeat_left[l]=0;
    }
  }
  if (audio_running && !running) {
    remaining = ordinal = clock_ordinal = swing_delay = 0;
    for(unsigned slot=0;slot<14;++slot)for(unsigned k=0;k<2;++k)engine.voices[slot].lfo_phase[k]=0;
    for (unsigned l = 0; l < 8; ++l) {
      repeat_left[l] = 0;
      for (unsigned k = 0; k < 2; ++k)
        engine.voices[l].lfo_phase[k] = 0;
    }
  }
  if(running&&!audio_running)gd_gate_stop(&punch_gate);
  running = audio_running;
  gd_gate_input(&punch_gate,punch_request,live.punch[1]&&running);
  int32_t peak = 0;
  uint32_t lane_peak[8]={0};
  for (uint32_t f = 0; f < frames; ++f) {
    if (running && remaining == 0) {
      ordinal%=gd_pattern_length(&live);
      if(!ordinal&&chain_running){
        unsigned event=gd_chain_boundary(&chain_clock,&chain_audio);chain_row=chain_clock.row;chain_pass=chain_clock.pass;
        if(event==2){chain_running=audio_running=running=0;chain_finished=1;gd_gate_stop(&punch_gate);for(unsigned l=0;l<8;++l)repeat_left[l]=0;for(unsigned s=0;s<8;++s)if(engine.synth[s].kind==7&&engine.synth[s].held)engine.synth[s].releasing=1;goto render_sample;}
        if(event==1)apply_chain_audio();
      }
      if(!ordinal&&pattern_armed)apply_pattern_audio();
      uint32_t base = 44100u * 60u / (live.bpm * 4u);
      remaining = gd_swing_interval(base,clock_ordinal++,live.swing,&swing_delay);
      clockhead=ordinal;unsigned source=gd_remix_source(&remix_audio,ordinal,gd_pattern_length(&live));
      playhead = source;
      gd_gate_boundary(&punch_gate);
      for (unsigned l = 0; l < 8; ++l) {
        const GDTrack *t = &live.track[l];
        unsigned len = t->length;
        if (len > live.pages * 16u)
          len = live.pages * 16u;
        unsigned s = source % (len ? len : 1);
        repeat_left[l] = 0;
        if (t->step[s] && (!t->mute||(engine.solo_mask&(1u<<l))) && gd_random(&seed) % 100u < t->chance[s]) {
          sequence_hit(l, t->step[s], t->pitch[s], t->lock_id[s],
                 t->lock_value[s],s);
          unsigned count = t->ratchet[s] ? t->ratchet[s] : 1;
          repeat_gap[l] = remaining / count;
          repeat_wait[l] = repeat_gap[l];
          repeat_left[l] = count - 1;
          repeat_step[l] = s;
        }
      }
      ordinal = (ordinal + 1u) % gd_pattern_length(&live);
    }
    for (unsigned l = 0; l < 8; ++l)
      if (running && repeat_left[l]) {
        if (repeat_wait[l])
          --repeat_wait[l];
        else {
          const GDTrack *t = &live.track[l];
          unsigned s = repeat_step[l];
          sequence_hit(l, t->step[s], t->pitch[s], t->lock_id[s],
                 t->lock_value[s],s);
          --repeat_left[l];
          repeat_wait[l] = repeat_gap[l];
        }
      }
    if (running)
      --remaining;
render_sample:;
    int32_t left, right;
    gd_render_sample(&engine, &live, &left, &right);
    gd_fx_send(&effects, &live, &left, &right,engine.delay_input);
    gd_punch(&punch,&live,punch_gate.active,effects.write,&left,&right);
    if((engine.counter&15u)==0){
      int32_t lane_sample[8]={0};
      for(unsigned slot=0;slot<14;++slot){unsigned lane=slot<8?slot:6+(slot-8)/3;lane_sample[lane]+=engine.voices[slot].last_sample;}
      for(unsigned lane=0;lane<8;++lane){
        if(engine.solo_mask?!(engine.solo_mask&(1u<<lane)):live.track[lane].mute)lane_sample[lane]=0;
        uint32_t magnitude=(uint32_t)(lane_sample[lane]<0?-lane_sample[lane]:lane_sample[lane]);
        if(magnitude>lane_peak[lane])lane_peak[lane]=magnitude;
      }
      uint32_t at=scope_write;scope_monitor[at&127]=(int16_t)gd_sat(lane_sample[display_lane&7]);scope_write=at+1;
    }
    out[f * 2] += left;
    out[f * 2 + 1] += right;
    int32_t a = left < 0 ? -left : left, b = right < 0 ? -right : right;
    if (a > peak)
      peak = a;
    if (b > peak)
      peak = b;
  }
  quiet_blocks = peak < 8 ? quiet_blocks + 1 : 0;
  audio_quiet = quiet_blocks > 8;
  punch_active=punch_gate.active;
  for(unsigned l=0;l<8;++l){level_monitor[l]=lane_peak[l]>level_monitor[l]?lane_peak[l]:level_monitor[l]*15/16;
    for(unsigned k=0;k<2;++k){lfo_phase_monitor[l][k]=engine.voices[l].lfo_phase[k];lfo_value_monitor[l][k]=(int16_t)engine.voices[l].lfo_value[k];}}
}

static const GDTheme *theme(void) {
  return &gd_themes[ui.theme % GD_THEME_COUNT];
}
static unsigned sound_pages(void) {
  return selected>=6 ? 8 : selected==5 ? 6 : 5;
}
static unsigned parameter_at(unsigned row, unsigned knob) {
  if (selected == 6)
    return (row % 8) * 4 + knob;
  if (selected == 7)
    return gd_analog_pages[row % 8][knob];
  if(selected==5){static const unsigned mix[4]={29,30,31,28};return row%6<5?(row%6)*4+knob:mix[knob];}
  if(selected<6){static const unsigned mix[4]={29,30,31,28};return row%5<4?(row%5)*4+knob:mix[knob];}
  static const unsigned common[4] = {31, 29, 30, 3};
  return row % 2 ? common[knob] : knob;
}
static unsigned parameter(unsigned knob) {
  return parameter_at(page, knob);
}
static GDControl descriptor_for(unsigned lane,unsigned index) {
  if (lane == 6){GDControl c=gd_gctrl[index];if(index<29&&index!=0&&index!=1&&index!=2&&index!=14&&index!=20&&index!=21&&index!=22)c.format=F_GL+index;return c;}
  if (lane == 7)
    return gd_actrl[index];
  if(lane==5&&index<20)return gd_perc_controls[index];
  if(lane==0&&index<16)return gd_kickctrl[index];
  if(lane>0&&lane<6&&index<16)return gd_ac_controls[lane-1][index];
  if(lane<6&&index==28)return (GDControl){"",0,F_NUMBER};
  if (index < 4)
    return gd_basic[index];
  if (index == 28)
    return (GDControl){"TRANSPOSE", 127, F_BIPOLAR};
  return gd_gctrl[index];
}
static GDControl descriptor(unsigned index){return descriptor_for(selected,index);}
static void string_copy(char *out, const char *s) {
  unsigned n = 0;
  while (s[n] && n < 23) {
    out[n] = s[n];
    ++n;
  }
  out[n] = 0;
}
static unsigned string_length(const char *s) {
  unsigned n = 0;
  while (s[n])
    ++n;
  return n;
}
static int held_step(void) {
  if (view!=V_VOICE || !lock_edit || recording)
    return -1;
  for (int k = 0; k < 27; ++k)
    if (key_held(k) && white_index(k) >= 0)
      return (int)(edit_page * 16) + (int)white_index(k);
  return -1;
}
static void format_hundredths(char *out,int value){
 unsigned n=0;if(value<0){out[n++]='-';value=-value;}
 format_int(out+n,value/100);while(out[n])++n;out[n++]='.';out[n++]=(char)('0'+value/10%10);out[n++]=(char)('0'+value%10);out[n]=0;
}
static void format_value(char *out, GDControl c, int v) {
  if(c.format==F_SWING_GRID){string_copy(out,v?"1/8":"1/16");return;}
  if(c.format>=F_AN&&c.format<F_AN+29){unsigned index=c.format-F_AN;int physical=gd_an_units[index][v];
    if(index==0||index==1||index==24)format_hundredths(out,physical);
    else if(index==9)format_int(out,physical);
    else if(index==15||index==20||index==23||index==26){int ms=(int)((int64_t)physical*100000/44100);if(ms<1000&&ms%100)format_hundredths(out,((ms+5)/10)*10);else format_int(out,(ms+50)/100);unsigned n=string_length(out);out[n++]='m';out[n++]='s';out[n]=0;}
    else format_hundredths(out,(int)(((int64_t)physical*100+(physical<0?-16384:16384))/32768));return;
  }
  if(c.format>=F_GL&&c.format<F_GL+29){unsigned index=c.format-F_GL;int physical=gd_gl_units[index][v];
    if(index==3||index==23)format_hundredths(out,physical);
    else if(index==12)format_int(out,physical);
    else if(index==16||index==17||index==18||index==19){int ms=(int)((int64_t)physical*100000/44100);if(ms<1000&&ms%100)format_hundredths(out,((ms+5)/10)*10);else format_int(out,(ms+50)/100);unsigned n=string_length(out);out[n++]='m';out[n++]='s';out[n]=0;}
    else format_hundredths(out,(int)(((int64_t)physical*100+(physical<0?-16384:16384))/32768));return;
  }
  if(c.format>=F_PERC&&c.format<F_PERC+20){
    unsigned index=c.format-F_PERC;int physical=gd_perc_units[index][v];
    if(index==0)format_hundredths(out,physical);
    else if(index==1||index==16||index==17){format_hundredths(out,(int)((int64_t)physical*100000/44100));unsigned n=string_length(out);string_copy(out+n,"ms");}
    else format_hundredths(out,(int)((int64_t)physical*100/32768));return;
  }
  if(c.format>=F_AC&&c.format<F_AC+16){
    unsigned index=c.format-F_AC,m=selected-1;int physical=gd_ac_units[m][index][v];
    if(index==0||((m==1||m==2)&&(index==4||index==5)))format_hundredths(out,physical);
    else if(index==1||index==12||index==13||(m==0&&index==7)||((m==1||m==2)&&index==9)||(m==3&&index==4)){int ms=(int)((int64_t)physical*100000/44100);if(ms<1000&&ms%100)format_hundredths(out,((ms+5)/10)*10);else format_int(out,(ms+50)/100);unsigned n=string_length(out);out[n++]='m';out[n++]='s';out[n]=0;}
    else if((m==3&&index==5)||(m==4&&index==10)||((m==0||m==3)&&index==15))format_int(out,physical);
    else format_hundredths(out,(int)(((int64_t)physical*100+(physical<0?-16384:16384))/32768));
    return;
  }
  if(c.format>=F_KICK&&c.format<F_KICK+16){
    unsigned index=c.format-F_KICK;int physical=gd_kick_units[index][v];
    if(index==0)format_hundredths(out,physical);
    else if(index==1||index==5||index==7||index==8||index==10||index==12||index==13){
      int hundredths=(int)((int64_t)physical*100000/44100);if(hundredths<1000&&hundredths%100)format_hundredths(out,((hundredths+5)/10)*10);else format_int(out,(hundredths+50)/100);
    }else if(index==4)format_hundredths(out,(physical*100+128)/256);
    else if(index==2||index==3||index==11)format_int(out,(physical*100+16384)/32768);
    else format_hundredths(out,(int)(((int64_t)physical*100+(physical<0?-16384:16384))/32768));
    return;
  }
  static const char *const source[3] = {"GLITCH", "SAW", "SINE"};
  static const char *const motion[4] = {"PITCH", "SIZE", "POSITION", "CUTOFF"};
  switch (c.format) {
  case F_SOURCE:
    string_copy(out, source[v % 3]);
    return;
  case F_MATERIAL:
    string_copy(out, gd_material_names[v % 8]);
    return;
  case F_TUNING:
    string_copy(out, v ? "RAW" : "PITCHED");
    return;
  case F_TARGET:
    string_copy(out, motion[v % 4]);
    return;
  case F_WAVE:
    string_copy(out, v ? "SQUARE" : "TRI");
    return;
  case F_BOOL:
    string_copy(out, v ? "ON" : "OFF");
    return;
  case F_PERCENT:
    v = v * 100 / 127;
    break;
  case F_BIPOLAR:
    v = (v - 64) * 100 / 63;
    break;
  case F_PAN:
    v = (v - 64) * 100 / 63;
    break;
  case F_TRANSPOSE:
    v = (v - 64) * 24 / 63;
    break;
  case F_WIDTH:
    v = v * 100 / 64;
    break;
  case F_TIME:
    v = (int)(gd_time_frames[v] * 1000u / 44100u);
    break;
  case F_HZ:
    v = gd_cutoff_hz[v];
    break;
  }
  format_int(out, v);
}
static GDControl effect_control(unsigned effect,unsigned k){
 static const char *const signature[3][4]={{"SIZE ms","SMEAR","SQUAWK","DECAY"},{"COLOR","GRIT","DRIVE","TONE"},{"SIZE ms","DENSITY","SPRAY","RATE %"}};
 if(effect<8){static const char *const label[4]={"SIZE ms","SPEED %","SEAM ms","BLEND"};return (GDControl){effect==6&&k==1?"BRAKE ms":effect==6&&k==2?"CURVE":label[k],127,k==3?F_PERCENT:F_NUMBER};}
 if(effect==8||effect==9||effect==25||effect==26){static const char *const label[4]={"TIME ms","START Hz","END Hz","BLEND"};return (GDControl){label[k],127,k==1||k==2?F_HZ:k==3?F_PERCENT:F_NUMBER};}
 if(effect==10){static const char *const label[4]={"LOW Hz","HIGH Hz","GAIN %","BLEND"};return (GDControl){label[k],127,k==3?F_PERCENT:F_NUMBER};}
 if(effect==15||effect==21){static const char *const label[4]={"RATE Hz","DEPTH","REGEN %","BLEND"};return (GDControl){label[k],127,k==1||k==3?F_PERCENT:F_NUMBER};}
 if(effect>=16&&effect<=18){static const char *const label[4]={"AMOUNT","CROSS Hz","SLOPE","LEVEL %"};return (GDControl){effect==17&&k==1?"LOW Hz":effect==17&&k==2?"HIGH Hz":label[k],127,k==0||(k==2&&effect!=17)?F_PERCENT:F_NUMBER};}
 if(effect==24){static const char *const label[4]={"RATE","DEPTH","CURVE","LEVEL %"};return (GDControl){label[k],k==0?7:127,k==1?F_PERCENT:F_NUMBER};}
 if(effect>=27)return (GDControl){signature[effect-27][k],127,(k||effect==28)?F_PERCENT:F_NUMBER};
 if(effect==20){static const char *const label[4]={"RATE Hz","DEPTH","WAVE","LEVEL %"};return (GDControl){label[k],k==2?2:127,k==1?F_PERCENT:F_NUMBER};}
 if(effect==11){static const char *const label[4]={"BITS","TONE","BLEND","LEVEL %"};return (GDControl){label[k],k==0?10:127,k==1||k==2?F_PERCENT:F_NUMBER};}
 if(effect==13){static const char *const label[4]={"RATE","WIDTH","FADE","LEVEL %"};return (GDControl){label[k],k==0?7:127,k==1||k==2?F_PERCENT:F_NUMBER};}
 if(effect==14){static const char *const label[4]={"AMOUNT","TIME ms","TONE","WIDTH"};return (GDControl){label[k],k==1?7:127,k==1?F_NUMBER:F_PERCENT};}
 if(effect==22){static const char *const label[4]={"RATE","DEPTH","PHASE","LEVEL %"};return (GDControl){label[k],k==0?7:127,k==0||k==3?F_NUMBER:F_PERCENT};}
 if(effect==12){static const char *const label[4]={"RATE","GRIT","TONE","BLEND"};return (GDControl){label[k],127,F_PERCENT};}
 if(effect==19){static const char *const label[4]={"FOLD","TRIM %","",""};return (GDControl){k==2?"BIAS":k==3?"BLEND":label[k],127,k==3?F_PERCENT:F_NUMBER};}
 if(effect==23){static const char *const label[4]={"PITCH Hz","FREEZE","DAMP","BLEND"};return (GDControl){label[k],127,k?F_PERCENT:F_NUMBER};}
 return (GDControl){k?"":(effect==11||effect==12||effect==14||effect==20?"DEPTH":"FIXED"),k||!(effect==11||effect==12||effect==14||effect==20)?0:127,F_NUMBER};
}
static unsigned effect_step(unsigned effect,unsigned k){
 if(effect==11&&k==0)return 12;
 if(effect==20&&k==2)return 43;
 if((effect==13||effect==22||effect==24)&&k==0)return 16;
 if(effect==14&&k==1)return 16;
 return 1;
}
static void control_info(unsigned k, GDControl *c, int *value) {
  if(view==V_REMIX){static const char *const name[4]={"VAR","GL RATE","GL MIX","DEPTH"};*c=(GDControl){name[k],127,F_NUMBER};*value=k==0?remix_ui.variation:k==3?remix_ui.depth:ui.fx[k==1?9:10];return;}
  GDTrack *t = &ui.track[selected];
  unsigned s = step_selected;
  if((view==V_MIX||view==V_HOME)&&mix_mode!=1){unsigned lane=mix_bank*4+k;*c=(GDControl){gd_lane_names[lane],127,F_PERCENT};*value=mix_mode==2?delay_send[lane]:ui.track[lane].param[29];return;}
  if(view==V_FXEDIT){*c=effect_control(fx_selected,k);*value=ui.effect[fx_selected][k]/effect_step(fx_selected,k);return;}
  if(view==V_PUNCH){static const char *const name[4]={"MIX","QUANT","FADE","DEPTH"};*c=(GDControl){k==3&&page%3==2?"CHAR":name[k],k==1?1:127,k==1?F_BOOL:F_NUMBER};*value=k==3?ui.effect[fx_selected][0]:ui.punch[k];if(k==3&&!effect_control(fx_selected,0).max){c->name="FIXED";c->max=0;*value=0;}return;}
  if (view == V_VOICE) {
    unsigned idx = parameter(k);
    *c = descriptor(idx);
    *value = t->param[idx];
    int held = held_step();
    if (held >= 0 && t->lock_id[held] == idx)
      *value = t->lock_value[held];
    return;
  }
  if (view == V_LFO) {
    static const char *const name[4] = {"WAVE", "SYNC RATE", "DEPTH",
                                        "DESTINATION"};
    static const uint8_t max[4] = {4, 11, 127, 8};
    *c = (GDControl){name[k], max[k], F_NUMBER};
    const GDLFO *m = &t->lfo[lfo];
    *value = k == 0   ? m->shape
             : k == 1 ? m->rate
             : k == 2 ? m->depth
                      : m->target;
    return;
  }
  if (view == V_FX) {
    unsigned idx = gd_master_fx_index(page,k);
    *c = gd_fxctrl[idx];
    *value = ui.fx[idx];
    return;
  }
  if (view == V_STEP) {
    static const char *const name[4] = {"PITCH", "VELOCITY", "CHANCE %",
                                        "RATCHETS"};
    static const uint8_t max[4] = {112, 127, 100, 4};
    *c = (GDControl){name[k], max[k], F_NUMBER};
    *value = k == 0   ? t->pitch[s] + 64
             : k == 1 ? t->step[s]
             : k == 2 ? t->chance[s]
                      : t->ratchet[s];
    return;
  }
  if (view == V_GLOBAL) {
    if(page%2){static const char *const names[4]={"GL QUALITY","GL VOICES","THEME",""};*c=(GDControl){names[k],k==0?2:k==1?4:k==2?37:0,F_NUMBER};*value=k==0?ui.reserved&3:k==1?(ui.reserved>>2?ui.reserved>>2:4):k==2?(int)ui.theme:0;return;}
    static const char *const name[4] = {"SW GRID", "SWING %", "STEPS", "BPM"};
    static const uint8_t max[4] = {1, 100, 16, 240};
    *c = (GDControl){name[k], max[k], k==0?F_SWING_GRID:F_NUMBER};
    *value = k == 0   ? (int)gd_swing_eighth(ui.swing)
             : k == 1 ? (int)gd_swing_amount(ui.swing)
             : k == 2 ? (int)gd_pattern_length(&ui)
                      : (int)ui.bpm;
    return;
  }
  static const char *const mix[4] = {"LEVEL", "PAN", "MUTE", "LANE LENGTH"};
  *c = (GDControl){mix[k],
                   k == 2   ? 1
                   : k == 3 ? 16
                            : 127,
                   k == 2   ? F_BOOL
                   : k == 0 ? F_PERCENT
                   : k == 1 ? F_PAN
                            : F_NUMBER};
  *value = k == 0   ? t->param[29]
           : k == 1 ? t->param[30]
           : k == 2 ? t->mute
                    : t->length;
}
static GDControl midi_control(GDMidiTarget t,int *value,int *minimum,unsigned *step){
 *value=0;*minimum=0;*step=1;if(!gd_midi_target_valid(t))return (GDControl){"",0,F_NUMBER};
 GDControl c={"",0,F_NUMBER};unsigned lane=t.index>>5,index=t.index&31;
 if(t.kind==M_VOICE){c=descriptor_for(lane,index);*value=ui.track[lane].param[index];}
 if(t.kind==M_LFO){static const char *const names[4]={"WAVE","RATE","DEPTH","TARGET"};static const uint8_t max[4]={4,11,127,8};lane=t.index>>3;index=t.index&7;const GDLFO *l=&ui.track[lane].lfo[index/4];const uint8_t *p=(const uint8_t*)l;c=(GDControl){names[index%4],max[index%4],F_NUMBER};*value=p[index%4];}
 if(t.kind==M_MASTER){c=gd_fxctrl[t.index];*value=ui.fx[t.index];}
 if(t.kind==M_EFFECT){unsigned effect=t.index/4,k=t.index%4;c=effect_control(effect,k);*step=effect_step(effect,k);*value=ui.effect[effect][k]/(int)*step;}
 if(t.kind==M_PUNCH){static const char *const name[3]={"MIX","QUANT","FADE"};c=(GDControl){name[t.index],t.index==1?1:127,F_NUMBER};*value=ui.punch[t.index];}
 if(t.kind==M_GLOBAL){c=(GDControl){t.index?"BPM":"SWING",t.index?240:100,F_NUMBER};*value=t.index?(int)ui.bpm:(int)gd_swing_amount(ui.swing);*minimum=t.index?40:0;}
 if(t.kind==M_MUTE){c=(GDControl){"MUTE",1,F_NUMBER};*value=ui.track[t.index].mute;}
 if(t.kind==M_SEND){c=(GDControl){"DLY SEND",127,F_PERCENT};*value=delay_send[t.index];}
 if(t.kind==M_LENGTH){c=(GDControl){"LENGTH",16,F_NUMBER};*value=ui.track[t.index].length;*minimum=1;}
 if(t.kind==M_GATE){c=(GDControl){"GATE",1,F_NUMBER};*value=(midi_fx_held>>t.index)&1;}
 if(c.format==F_NOTE||c.format==F_AN||c.format==F_AN+1||c.format==F_GL+3||c.format==F_GL+23)*minimum=12;
 return c;
}
static void midi_name(char *out,GDMidiTarget t){
 int value,min;unsigned step;GDControl c=midi_control(t,&value,&min,&step);out[0]=0;
 if(t.kind==M_VOICE||t.kind==M_LFO||t.kind==M_MUTE||t.kind==M_LENGTH||t.kind==M_SEND){unsigned lane=t.kind==M_VOICE?t.index>>5:t.kind==M_LFO?t.index>>3:t.index;string_copy(out,gd_lane_names[lane]);unsigned n=string_length(out);out[n++]=' ';out[n]=0;if(t.kind==M_LFO){string_copy(out+n,(t.index&7)<4?"LFO1 ":"LFO2 ");}}
 else if(t.kind==M_EFFECT||t.kind==M_GATE){string_copy(out,gd_punch_names[t.kind==M_GATE?t.index:t.index/4]);unsigned n=string_length(out);out[n++]=' ';out[n]=0;}
 else string_copy(out,t.kind==M_MASTER?"MAIN ":t.kind==M_PUNCH?"PUNCH ":"GLOBAL ");
 string_copy(out+string_length(out),c.name);
}
static int midi_pick_knob(unsigned k){
 GDMidiTarget t={M_INVALID,0};
 if((view==V_MIX||view==V_HOME)&&mix_mode==2)t=(GDMidiTarget){M_SEND,(uint8_t)(mix_bank*4+k)};
 else if((view==V_MIX||view==V_HOME)&&!mix_mode)t=(GDMidiTarget){M_VOICE,(uint8_t)(((mix_bank*4+k)<<5)|29)};
 else if(view==V_REMIX&&(k==1||k==2))t=(GDMidiTarget){M_MASTER,(uint8_t)(k==1?9:10)};
 else if(view==V_MIX||view==V_HOME)t=(GDMidiTarget){k<2?M_VOICE:k==2?M_MUTE:M_LENGTH,(uint8_t)(k<2?(selected<<5)|(29+k):selected)};
 else if(view==V_VOICE&&held_step()<0)t=(GDMidiTarget){M_VOICE,(uint8_t)((selected<<5)|parameter(k))};
 else if(view==V_LFO&&lfo_display<2)t=(GDMidiTarget){M_LFO,(uint8_t)((selected<<3)|(lfo*4+k))};
 else if(view==V_FX)t=(GDMidiTarget){M_MASTER,(uint8_t)gd_master_fx_index(page,k)};
 else if(view==V_FXEDIT)t=(GDMidiTarget){M_EFFECT,(uint8_t)(fx_selected*4+k)};
 else if(view==V_PUNCH)t=(GDMidiTarget){k<3?M_PUNCH:M_EFFECT,(uint8_t)(k<3?k:fx_selected*4)};
 else if(view==V_GLOBAL&&page==0&&(k==1||k==3))t=(GDMidiTarget){M_GLOBAL,(uint8_t)(k==3)};
 int value,min;unsigned step;GDControl c=midi_control(t,&value,&min,&step);
 midi_target=t;midi_pick=c.max!=0;if(!midi_pick)midi_target.kind=M_INVALID;
 need_header=1;if(midi_pick)notice_until=0;else message("CONTROL NOT LEARNABLE");return (int)midi_pick;
}
static void midi_toggle(void){
 sel_pending=0;
 if(midi_learn_on){midi_learn_on=midi_pick=0;message("MIDI LEARN DONE");}
 else{midi_learn_on=1;midi_pick=0;midi_target=(GDMidiTarget){M_INVALID,0};view=midi_return_view;page=midi_return_page;need_full=1;message("LEARN: TURN A KNOB");}
 need_header=need_controls=1;
}
static void midi_clear_target(void){
 unsigned removed=0;
 if(midi_target.kind!=M_INVALID){for(unsigned n=0;n<16;++n)if(gd_midi_target_is(&midi_map[n],midi_target)){midi_map[n]=(GDMidiMap){0};++removed;}}
 else if(midi_map[midi_map_cursor].cc&128){midi_map[midi_map_cursor]=(GDMidiMap){0};removed=1;}
 midi_fx_held=0;need_header=need_controls=1;message(removed?"MIDI MAP CLEARED - SAVE":"NO MAPPING TO CLEAR");
}
static void midi_apply(GDMidiTarget t,unsigned incoming){
 int old,min;unsigned step;GDControl c=midi_control(t,&old,&min,&step);if(!c.max)return;
 unsigned value=(unsigned)min+(incoming*(c.max-(unsigned)min)+63u)/127u;if((int)value==old)return;
 unsigned lane=t.index>>5,index=t.index&31;
 if(t.kind==M_VOICE){if(!send(C_PARAM,lane,index,value))return;ui.track[lane].param[index]=(uint8_t)value;}
 if(t.kind==M_LFO){lane=t.index>>3;index=t.index&7;if(!send(C_LFO,lane,index,value))return;uint8_t *p=(uint8_t*)&ui.track[lane].lfo[index/4];p[index%4]=(uint8_t)value;}
 if(t.kind==M_MASTER){if(!send(C_FX,0,t.index,value))return;ui.fx[t.index]=(uint8_t)value;if(t.index==9)glitch_offset=gd_clamp((int)value-(int)glitch_base,-126,126);if(t.index==10)glitch_prepared=1;}
 if(t.kind==M_EFFECT){unsigned effect=t.index/4;if(!send(C_EFFECT,0,effect*4+t.index%4,value*step))return;ui.effect[effect][t.index%4]=(uint8_t)(value*step);}
 if(t.kind==M_PUNCH){if(!send(C_PUNCH,0,t.index,value))return;ui.punch[t.index]=(uint8_t)value;}
 if(t.kind==M_GLOBAL){if(!send(C_GLOBAL,0,t.index?3:1,value))return;if(t.index)ui.bpm=value;else ui.swing=(uint8_t)((ui.swing&128u)|(unsigned)value);}
 if(t.kind==M_MUTE){if(!send(C_MUTE,t.index,0,value))return;ui.track[t.index].mute=(uint8_t)value;}
 if(t.kind==M_SEND){if(!send(C_SEND,t.index,0,value))return;delay_send[t.index]=(uint8_t)value;}
 if(t.kind==M_LENGTH){if(!send(C_LENGTH,t.index,0,value))return;ui.track[t.index].length=(uint8_t)value;}
 if(t.kind==M_GATE){if(value)midi_fx_held|=1u<<t.index;else midi_fx_held&=~(1u<<t.index);}
 midi_ui_dirty=1;if(t.kind==M_MUTE||t.kind==M_LENGTH||(t.kind==M_GLOBAL&&t.index))midi_header_dirty=1;
}
static void midi_cc(unsigned channel,unsigned cc,unsigned value){
 if(midi_learn_on&&midi_pick){
  int at=gd_midi_bind(midi_map,channel,cc,midi_target);
  if(at<0){message(at==-1?"16 MAPS FULL: CLEAR ONE":"RESERVED CC: TRY ANOTHER");return;}
  midi_map_cursor=(unsigned)at;midi_pick=0;midi_fx_held=0;notice_until=0;need_header=need_controls=1;return;
 }
 for(unsigned n=0;n<16;++n)if((midi_map[n].cc&128)&&(midi_map[n].route&15)==channel&&(midi_map[n].cc&127)==cc){midi_apply((GDMidiTarget){midi_map[n].route>>4,midi_map[n].index},value);return;}
 if(cc==7||cc==10){unsigned lane=channel<8?channel:selected;midi_apply((GDMidiTarget){M_VOICE,(uint8_t)((lane<<5)|(cc==7?29:30))},value);}
}
static uint32_t midi_gate_mask(void){return midi_fx_held&GD_KEYBOARD_FX_MASK;}
static void draw_midi_page(void){
 const GDTheme *t=theme();rect(0,70,240,170,t->bg);
 unsigned used=gd_midi_count(midi_map);char count[24];string_copy(count,"MAPS ");format_int(count+5,(int)used);string_copy(count+string_length(count)," / 16");text(8,74,count,t->accent,FONT_SMALL);
 for(int row=0;row<3;++row){unsigned n=(midi_map_cursor+16+(unsigned)row-1)%16;int y=98+row*25;char b[40];format_int(b,(int)n+1);unsigned at=string_length(b);b[at++]=' ';b[at]=0;
  if(midi_map[n].cc&128){string_copy(b+at,"CH");format_int(b+string_length(b),1+(midi_map[n].route&15));string_copy(b+string_length(b)," CC");format_int(b+string_length(b),midi_map[n].cc&127);}else string_copy(b+at,"EMPTY");
  if(row==1)frame(4,y-2,232,23,t->accent);text(8,y,b,row==1?t->text:t->chrome,FONT_SMALL);
 }
 if(midi_map[midi_map_cursor].cc&128){char b[40];midi_name(b,(GDMidiTarget){midi_map[midi_map_cursor].route>>4,midi_map[midi_map_cursor].index});text(8,172,b,t->alt,FONT_SMALL);}
 text(8,190,"SEL:LEARN KN2:MAP",t->text,FONT_SMALL);text(8,206,"OCT-:CLEAR OCT+:DONE",t->chrome,FONT_SMALL);text(8,222,"SAVE:KEEP / 12268 B",t->chrome,FONT_SMALL);
}
static unsigned effect_count(uint32_t mask){unsigned n=0;while(mask){mask&=mask-1;++n;}return n;}
static const char *effect_state(void){
 uint32_t bit=1u<<fx_selected;
 if(punch_latched&bit)return punch_active&bit?"LOCK":"WAIT";
 if(!punch_latch_mode&&(punch_key_mask&bit))return punch_active&bit?"HELD":"WAIT";
 return punch_active&bit?"TAIL":"OFF";
}
static void draw_header(void) {
  const GDTheme *t = theme();
  char b[12];
  rect(0, 0, 240, 23, t->surface);
  text(6, 3, "GLITCHDRUMS", t->accent, FONT_SMALL);
  format_int(b, (int)ui.bpm);
  text(134, 3, b, t->text, FONT_SMALL);
  text(170, 3, playing ? "PLAY" : "STOP", playing ? t->accent : t->chrome,
       FONT_SMALL);
  if(recording)text(210,3,"REC",t->alt,FONT_SMALL);
  else if(pattern_busy)text(210,3,(const char[]){'Q',':',(char)('A'+queued_pattern),0},t->alt,FONT_SMALL);
  else if(punch_latched){char state[8];state[0]='F';state[1]='X';format_int(state+2,(int)effect_count(punch_latched));text(210,3,state,t->alt,FONT_SMALL);}
  if (notice_until && millis() < notice_until) {
    /* Include the full 16-pixel status line at y=54 when replacing OCT/MUTE/SOLO. */
  rect(0, 25, 240, (view==V_STEP||view==V_REMIX)?20:45, t->bg);
    text(8, (view==V_STEP||view==V_REMIX)?27:30, notice, t->alt, FONT_SMALL);
    return;
  }
  /* Include the full 16-pixel status line at y=54 when replacing OCT/MUTE/SOLO. */
  rect(0, 25, 240, (view==V_STEP||view==V_REMIX)?20:45, t->bg);
  if(view==V_STEP){
    if(midi_learn_on){char label[40];if(midi_target.kind==M_INVALID)string_copy(label,"LEARN: TURN A KNOB");else {string_copy(label,midi_pick?"SEND CC: ":"LEARN: ");midi_name(label+string_length(label),midi_target);}label[29]=0;text(6,27,label,t->text,FONT_SMALL);return;}
    static const char *const names[8]={"KICK","SNARE","C HAT","O HAT","CLAP","PRC","GLITCH","ANALOG"};
    char pattern[]={(char)('A'+active_pattern),0};text(6,27,pattern,t->text,FONT_SMALL);
    b[0]='S';b[1]=(char)('0'+(step_selected+1)/10);b[2]=(char)('0'+(step_selected+1)%10);b[3]=0;text(24,27,b,t->text,FONT_SMALL);
    b[0]='L';b[1]=(char)('0'+ui.track[selected].length/10);b[2]=(char)('0'+ui.track[selected].length%10);text(56,27,b,t->text,FONT_SMALL);
    text(96,27,names[selected],t->accent,FONT_SMALL);
    if(solo_ui){if(solo_ui&(1u<<selected))text(192,27,"SOLO",t->alt,FONT_SMALL);else {b[0]='S';unsigned lane=0;while(!(solo_ui&(1u<<lane)))++lane;format_int(b+1,(int)lane+1);text(192,27,b,t->alt,FONT_SMALL);}}
    else if(ui.track[selected].mute)text(192,27,"MUTE",t->alt,FONT_SMALL);
    else if(selected>=6){b[0]='N';format_int(b+1,ui.track[selected].step[step_selected]?1+pattern_bank[active_pattern].chord_count[selected-6][step_selected]:0);text(192,27,b,t->alt,FONT_SMALL);}
    return;
  }
  if(view==V_REMIX){
    if(midi_learn_on){text(6,27,midi_pick?"LEARN: SEND CC":"LEARN: TURN A KNOB",t->text,FONT_SMALL);return;}
    text(6,27,"SLICE",t->text,FONT_SMALL);text(56,27,gd_remix_names[remix_ui.mode],t->accent,FONT_SMALL);text(176,27,remix_active?(remix_latch?"LATCH":"HOLD"):"READY",t->alt,FONT_SMALL);return;
  }
  if(midi_learn_on){
    char label[40];if(midi_target.kind==M_INVALID)string_copy(label,"LEARN: TURN A KNOB");else midi_name(label,midi_target);text(8,28,label,t->accent,FONT_SMALL);
    string_copy(label,midi_pick?"SEND CC / OCT+:DONE":"TURN KNOB / OCT+:DONE");
    if(!midi_pick)for(unsigned n=0;n<16;++n)if(gd_midi_target_is(&midi_map[n],midi_target)){
      string_copy(label,"CH");format_int(label+2,1+(midi_map[n].route&15));string_copy(label+string_length(label)," CC");format_int(label+string_length(label),midi_map[n].cc&127);string_copy(label+string_length(label)," OCT+:DONE");break;
    }
    text(8,48,label,t->chrome,FONT_SMALL);return;
  }
  if(view==V_GLOBAL&&page==2){text(8,28,"MIDI LEARN",t->accent,FONT_BIG);text(8,54,"1-8:TRACK 9-16:SELECT",t->chrome,FONT_SMALL);return;}
  if(view==V_PATTERNS){text(8,28,"PATTERNS A-P",t->accent,FONT_BIG);text(8,54,"SEL:LOAD / MAX 16",t->chrome,FONT_SMALL);return;}

  if(view==V_CHAIN){
    text(8,28,"CHAIN",t->accent,FONT_BIG);text(120,30,chain_ui.mode==0?"OFF":chain_ui.mode==1?"LOOP":"ONCE",t->alt,FONT_SMALL);
    char state[32];string_copy(state,chain_running?"RUN ":"LEN ");format_int(state+4,(int)(chain_running?chain_row+1:chain_ui.length));
    if(chain_running){unsigned n=(unsigned)string_length(state);string_copy(state+n,"  PASS ");n=(unsigned)string_length(state);format_int(state+n,(int)chain_pass);}
    text(8,54,state,t->chrome,FONT_SMALL);return;
  }
  if(view==V_PRESETS){
    text(8,26,gd_preset_types[preset_type],t->accent,FONT_SMALL);
    format_int(b,(int)preset_slot+1);text(8,48,b,t->alt,FONT_SMALL);
    text(40,48,preset_type==0?gd_lane_names[selected]:"NO PATTERN REPLACE",t->text,FONT_SMALL);return;
  }
  format_int(b, (int)(fx_surface()?fx_slot:selected) + 1);
  text(8, fx_surface()?28:26, b, t->alt, fx_surface()?FONT_SMALL:FONT_BIG);
  if(fx_surface())text(32,28,gd_punch_names[fx_selected],t->accent,FONT_SMALL);
  else text(32, 26, gd_lane_names[selected], t->accent, FONT_BIG);
  char step_title[]={'S','T','E','P',' ',(char)('A'+active_pattern),0};
  const char *title = view == V_VOICE    ? (voice_focus?"SCOPE":"ROWS")
                      : view == V_STEP   ? step_title
                      : view == V_LFO    ? (lfo_display==2?"ORBITS":lfo ? "LFO 2" : "LFO 1")
                      : view == V_FXEDIT ? "FX EDIT"
                      : view == V_PUNCH  ? "PUNCH"
                      : view == V_FX     ? "FX"
                      : view == V_GLOBAL ? "GLOBAL"
                                         : (mix_mode==2?"D SEND":mix_mode?"TRACK":"LEVELS");
  text(8, 54, title, t->text, FONT_SMALL);
  format_int(b, (int)(view == V_VOICE ? page % sound_pages() + 1
                      : view == V_LFO ? lfo_display + 1
                      : view == V_PUNCH ? page % 3 + 1
                      : view == V_FX  ? page % 3 + 1
                      : view==V_MIX||view==V_HOME ? mix_bank+1
                      : view==V_GLOBAL ? page+1
                                      : view==V_STEP ? step_selected+1 : edit_page + 1));
  if(view==V_VOICE){unsigned n=(unsigned)string_length(b);b[n++]='/';format_int(b+n,(int)sound_pages());}
  if(view!=V_FXEDIT)text(64, 54, b, t->alt, FONT_SMALL);
  char lane_length[16];string_copy(lane_length,"LEN ");format_int(lane_length+4,ui.track[selected].length);
  text(96, 54, view==V_STEP?lane_length:view==V_FX&&page%3==2?"GLITCH":fx_surface() ? (punch_latch_mode?"LATCH KEYS":"HOLD KEYS") : view==V_VOICE&&lock_edit?"LOCK KEYS":sequence_mode ? "STEP KEYS" : "LIVE KEYS", t->chrome,
       FONT_SMALL);
  if(fx_surface()){
    rect(96,52,144,14,t->bg);
    text(96,54,punch_latch_mode?"LATCH":"HOLD",t->chrome,FONT_SMALL);
    char status[16];status[0]='L';format_int(status+1,(int)effect_count(punch_latched));text(148,54,status,t->alt,FONT_SMALL);
    status[0]='H';format_int(status+1,(int)effect_count(punch_key_mask&~punch_latched));text(192,54,status,t->accent,FONT_SMALL);
    text(192,28,effect_state(),t->alt,FONT_SMALL);return;
  }
  if(solo_ui){
    if(solo_ui&(1u<<selected))text(192,54,"SOLO",t->alt,FONT_SMALL);
    else {text(192,54,"S",t->alt,FONT_SMALL);unsigned lane=0;while(!(solo_ui&(1u<<lane)))++lane;format_int(b,(int)lane+1);text(208,54,b,t->alt,FONT_SMALL);}
  }
  else if (ui.track[selected].mute)
    text(192, 54, "MUTE", t->alt, FONT_SMALL);
  else if(sequence_mode&&selected>=6){char notes[8];string_copy(notes,"N:");format_int(notes+2,ui.track[selected].step[step_selected]?1+pattern_bank[active_pattern].chord_count[selected-6][step_selected]:0);text(192,54,notes,t->alt,FONT_SMALL);}
  else if (!sequence_mode) {
    text(184, 54, "OCT", t->chrome, FONT_SMALL);
    format_int(b, keyboard_octave);
    text(216, 54, b, t->alt, FONT_SMALL);
  }
}
static uint16_t visual_blend(uint16_t color,uint16_t bg,unsigned amount);
static void draw_grid(void) {
  const GDTheme *t = theme();
  rect(0, 46, 240, 82, t->bg);
  for (unsigned l = 0; l < 8; ++l) {
    const GDTrack *tr = &ui.track[l];
    rect(8, 51 + (int)l * 9, 5, 7, l == selected ? t->alt : t->chrome);
    for (unsigned i = 0; i < 16; ++i) {
      unsigned s = edit_page * 16 + i;
      int x = 20 + (int)i * 13, y = 51 + (int)l * 9;
      uint16_t color =
          tr->step[s] ? (s>=tr->length?t->surface:tr->mute?t->chrome:t->accent) : t->surface;
      if(s>=tr->length){
        rect(x,y,11,7,visual_blend(t->chrome,t->bg,22));
        uint16_t hatch=visual_blend(t->chrome,t->bg,42);
        for(int h=0;h<9;h+=4){line(x+h,y+6,x+h+3,y+3,hatch);line(x+h,y+3,x+h+3,y,hatch);}
      }else rect(x, y, 11, 7, color);
      if (playing && playhead % tr->length == s)
        frame(x, y, 11, 7, t->alt);
      if (l == selected && s == step_selected)
        frame(x - 1, y - 1, 13, 9, t->text);
    }
    if(tr->length<16){int end=18+(int)tr->length*13;line(end,50+(int)l*9,end,58+(int)l*9,t->chrome);}
  }
  int cursor_x=25+(int)(step_selected%16)*13;
  line(cursor_x,47,cursor_x,49,t->text);line(cursor_x,123,cursor_x,126,t->text);
}
static void draw_voice_rows(void) {
  const GDTheme *t = theme();
  unsigned count = sound_pages(), active = page % count;
  unsigned first = active > 0 ? active - 1 : 0;
  if (count <= 4) first = 0;
  else if (first > count - 4) first = count - 4;
  rect(0, 70, 240, 170, t->bg);
  for (unsigned r = first; r < count && r < first + 4; ++r) {
    int y = 71 + (int)(r - first) * 42;
    int focused = r == active;
    if (focused) {
      rect(2, y, 234, 40, t->surface);
      frame(2, y, 234, 40, t->accent);
    }
    for (unsigned k = 0; k < 4; ++k) {
      unsigned idx = parameter_at(r, k);
      GDControl c = descriptor(idx);
      int held=held_step(),locked=held>=0&&ui.track[selected].lock_id[held]==idx;
      int v = locked?ui.track[selected].lock_value[held]:ui.track[selected].param[idx];
      char label[7], value[24];
      unsigned n = 0;
      /* Compact labels retain trailing oscillator numbers in narrow columns. */
      const char *name = c.name;
      if (idx < 28 && selected == 6) {
        static const char *const compact[28] = {
          "SRC 1","TAPE 1","TUNE 1","NOTE 1","SIZE 1","POS 1","EDGE 1","FRZN 1",
          "FRESH","COLOR","FOLD","DRIVE","CUTOFF","RESO","MOT TO","MOTION",
          "MOTDEC","ATTACK","HOLD","DECAY","SRC 2","TAPE 2","TUNE 2","NOTE 2",
          "SIZE 2","POS 2","EDGE 2","FRZN 2"};
        name = compact[idx];
      } else if(selected==7&&idx<29)name=gd_an_names[idx];
      if(selected==0&&idx<16){static const char *const compact[16]={"TUNE","DECAY","TONE","DRIVE","DROP","PITDEC","CLICK","CLKDEC","BD DLY","SHAPE","P HOLD","PUNCH","ATTACK","HOLD","CURVE","COLOR"};name=compact[idx];}
      if(selected==5&&idx<20){static const char *const compact[20]={"TUNE","DECAY","TONE","NOISE","RATIO","XMOD","XM DEC","SHAPE","DUAL","BEND","BN DEC","STRIKE","TN/NS","THUD","SNAP","COLOR","ATTACK","HOLD","CURVE","DRIVE"};name=compact[idx];}
      else if(selected>0&&selected<6&&idx<16)name=gd_ac_controls[selected-1][idx].name;
      if (idx == 28 && selected == 6) name = "MIX";
      if (idx == 31) name = "TRANS";
      while (name[n] && n < 6) { label[n] = name[n]; ++n; }
      label[n] = 0;
      format_value(value, c, v);
      if(!c.max)string_copy(value,"--");
      value[7] = 0;
      int x = 7 + (int)k * 57;
      if(locked)frame(x-2,y+2,55,36,t->alt);
      text(x, y + 5, label, focused ? t->text : t->chrome, FONT_SMALL);
      text(x, y + 20, value, focused ? t->accent : t->text, FONT_SMALL);
      rect(x, y + 35, 49, 2, t->surface);
      if (c.max) rect(x, y + 35, gd_clamp(v * 49 / c.max, 0, 49), 2,
                      focused ? t->alt : t->chrome);
    }
  }
  /* Scroll position stays visible without covering parameter text. */
  rect(238, 71, 2, 166, t->surface);
  rect(238, 71 + (int)(active * 166 / count), 2,
       (int)(166 / count), t->accent);
}
/* Audio publishes only fixed scalar/ring data. The display never renders DSP. */
static void draw_voice_scope(void){
 const GDTheme *t=theme();int16_t samples[128];uint32_t end=scope_write;
 for(unsigned n=0;n<128;++n)samples[n]=scope_monitor[(end+n)&127];
 int amplitude=128;for(unsigned n=0;n<128;++n){int a=samples[n]<0?-samples[n]:samples[n];if(a>amplitude)amplitude=a;}
 rect(0,70,240,58,t->bg);line(8,100,231,100,t->surface);
 for(int x=8;x<=232;x+=56)line(x,80,x,121,t->surface);
 /* A rising crossing near the start steadies pitched voices without a wait. */
 unsigned start=0;for(unsigned n=1;n<32;++n)if(samples[n-1]<=0&&samples[n]>0){start=n;break;}
 int px=8,py=103-samples[start]*18/amplitude;
 for(unsigned n=start+1;n<128;++n){int x=8+(int)(n-start)*223/(127-(int)start),y=103-samples[n]*18/amplitude;line(px,py,x,y,t->accent);px=x;py=y;}
 text(8,71,"PRE FX / AUTO",t->chrome,FONT_SMALL);
}
static void draw_mixer(void){
 const GDTheme *t=theme();static const char *const names[8]={"KIK","SNR","CH","OH","CLP","PRC","GLI","ANA"};
 rect(0,70,240,170,t->bg);
 for(unsigned lane=0;lane<8;++lane){
  const GDTrack *tr=&ui.track[lane];int x=4+(int)lane*29;
  int audible=solo_ui?(solo_ui&(1u<<lane))!=0:!tr->mute;
  uint16_t color=audible?(lane==selected?t->alt:t->accent):t->chrome;
  if(lane==selected)frame(x-2,72,28,126,t->alt);
  if(mix_mode!=1&&lane/4==mix_bank)rect(x,201,23,2,t->accent);
  text(x,76,names[lane],color,FONT_SMALL);
  char b[8];format_int(b,(mix_mode==2?delay_send[lane]:tr->param[29])*100/127);text(x,90,b,t->text,FONT_SMALL);
  rect(x+2,108,10,64,t->surface);int height=(mix_mode==2?delay_send[lane]:tr->param[29])*64/127;
  rect(x+2,172-height,10,height,visual_blend(color,t->bg,64));
  line(x,172-height,x+13,172-height,color);
  int meter=gd_clamp((int)level_monitor[lane]*64/12000,0,64);
  rect(x+17,108,3,64,t->surface);if(meter)rect(x+17,172-meter,3,meter,color);
  line(x,183,x+21,183,t->surface);int pan=tr->param[30]*21/127;rect(x+pan,180,2,6,color);
  text(x,188,solo_ui&(1u<<lane)?"S":tr->mute?"M":"",color,FONT_SMALL);
 }
 static const char *const captions[4]={"LEVEL","PAN","MUTE","LENGTH"};
 static const char *const mixer_names[8]={"KICK","SNARE","C HAT","O HAT","CLAP","PRC","GLITCH","ANALOG"};
 for(unsigned k=0;k<4;++k){GDControl c;int v;char b[24];control_info(k,&c,&v);format_value(b,c,v);int x=5+(int)k*59;text(x,208,mix_mode==1?captions[k]:mixer_names[mix_bank*4+k],t->chrome,FONT_SMALL);text(x,225,b,t->accent,FONT_SMALL);}
}
static void draw_punch_pads(void){
 const GDTheme *t=theme();rect(0,70,240,170,t->bg);
 static const char *const short_name[30]={"1/4","1/8","1/16","1/32","TRIP","REV","STOP","HALF","LP","HP","PHON","CRSH","ALIS","GATE","ECHO","WOB","","","","FOLD","RING","FLNG","PAN","COMB","PUMP","RISE","FALL","ANTI","DIST","GCLD"};
 for(unsigned key=0;key<27;++key){unsigned effect=gd_keyboard_effect(key);int x=3+(int)(key%6)*39,y=72+(int)(key/6)*24,active=(punch_active>>effect)&1u;
  rect(x,y,37,22,active?t->accent:t->surface);frame(x,y,37,22,key==fx_slot?t->alt:t->chrome);text(x+2,y+3,short_name[effect],active?t->bg:t->text,FONT_SMALL);if(punch_latched&(1u<<effect))rect(x+2,y+20,33,2,t->alt);
 }
 text(6,192,"EDIT:TWEAK ENV:LATCH",t->chrome,FONT_SMALL);
 for(unsigned k=0;k<4;++k){GDControl c;int v;char b[24];control_info(k,&c,&v);format_value(b,c,v);text(6+(int)k*58,208,c.name,t->chrome,FONT_SMALL);text(6+(int)k*58,224,b,t->accent,FONT_SMALL);}
}
static const char *preset_name(unsigned slot,char *b){
 if(slot<factory_count())string_copy(b,preset_type==0?gd_factory_voice_names[selected][slot]:preset_type==1?gd_factory_kit_names[slot]:gd_factory_scene_names[slot]);
 else{string_copy(b,"USER ");format_int(b+5,(int)(slot-factory_count()+1));}
 if(preset_custom(slot)>=0){unsigned n=(unsigned)string_length(b);if(n<22){b[n++]='*';b[n]=0;}}return b;
}
static int same_bytes(const void *a,const void *b,unsigned size){
 const uint8_t *x=a,*y=b;for(unsigned i=0;i<size;++i)if(x[i]!=y[i])return 0;return 1;
}
static int preset_matches_current(void){
 if(!preset_exists())return 0;unsigned n=preset_slot;int custom=preset_custom(n);
 if(preset_type==0){GDSound current=gd_get_sound(&ui,selected),target=custom>=0?preset_bank.voice[selected][custom]:gd_factory_sound(selected,n);return same_bytes(&current,&target,sizeof current);}
 if(preset_type==1){GDKit current=gd_get_kit(&ui),target=custom>=0?preset_bank.kit[0]:gd_factory_kit(n);if(custom<0&&n){uint8_t expected[8];gd_factory_delay_sends(n,&target,expected);if(!same_bytes(expected,delay_send,8))return 0;}return same_bytes(&current,&target,sizeof current);}
 GDScene current=gd_get_scene(&ui),target=custom>=0?preset_bank.scene[0]:gd_factory_scene(n);if(custom<0&&n){uint8_t expected[8];gd_factory_delay_sends(n,0,expected);if(!same_bytes(expected,delay_send,8))return 0;}return same_bytes(&current,&target,sizeof current);
}
static void draw_preset_browser(void){
 const GDTheme *t=theme();rect(0,70,240,170,t->bg);
 int current=preset_matches_current();
 text(8,74,!preset_exists()?"EMPTY USER SLOT":current?"CURRENT SOUND / LOADED":"BROWSING / NOT LOADED",current?t->accent:t->chrome,FONT_SMALL);
 unsigned count=preset_count();
 for(int row=0;row<3;++row){
  unsigned slot=(preset_slot+count+(unsigned)row-1)%count;int y=100+row*25;
  char name[24],number[8];number[0]=slot<factory_count()?'F':'U';format_int(number+1,(int)(slot<factory_count()?slot:slot-factory_count())+1);
  if(row==1){rect(4,y-2,232,22,t->surface);frame(4,y-2,232,22,t->accent);}
  text(8,y,number,row==1?t->alt:t->chrome,FONT_SMALL);
  text(40,y,preset_name(slot,name),row==1?t->text:t->chrome,FONT_SMALL);
 }
 text(8,180,"PRESETS:BROWSE SEL:LOAD",t->text,FONT_SMALL);
 text(8,198,"ALGORITHM:VOICE/KIT/FX",t->chrome,FONT_SMALL);
 if(preset_type==0)text(8,211,"ENV+SEL:INIT VOICE",t->chrome,FONT_SMALL);
 text(8,228,"SAVE:STORE ENV+SAVE:RESET",t->alt,FONT_SMALL);
}
static uint16_t visual_blend(uint16_t color,uint16_t bg,unsigned amount){
 unsigned r=(((color>>11)&31)*amount+((bg>>11)&31)*(127-amount))/127;
 unsigned g=(((color>>5)&63)*amount+((bg>>5)&63)*(127-amount))/127;
 unsigned b=((color&31)*amount+(bg&31)*(127-amount))/127;
 return (uint16_t)((r<<11)|(g<<5)|b);
}
static void draw_lfo_visual(void){
 /* Positive runtime divisor avoids a pi32v2 ROM-window immediate. */
 volatile int lfo_depth_scale=32768*127;
 static int16_t history[96];static unsigned history_write,last_lane=99,last_lfo=99;
 static uint32_t orbit_history[2][12];static unsigned orbit_write,orbit_lane=99,last_mode=99;
 const GDTheme *t=theme();const GDLFO *m=&ui.track[selected].lfo[lfo];
 if(lfo_display==2){
   if(last_mode!=2||orbit_lane!=selected){for(unsigned k=0;k<2;++k)for(unsigned n=0;n<12;++n)orbit_history[k][n]=lfo_phase_monitor[selected][k];orbit_write=0;orbit_lane=selected;}
   last_mode=2;
   for(unsigned k=0;k<2;++k)orbit_history[k][orbit_write%12]=lfo_phase_monitor[selected][k];++orbit_write;
   rect(0,70,240,170,t->bg);text(8,72,"DUAL AUDIO PHASE",t->chrome,FONT_SMALL);
   line(42,154,198,154,t->surface);line(120,90,120,218,t->surface);
   for(unsigned k=0;k<2;++k){
     unsigned radius=(k?58:38)+ui.track[selected].lfo[k].depth*6/127;
     uint16_t color=k?t->alt:t->accent;int px=120,py=154-(int)radius;
     for(unsigned n=1;n<=64;++n){uint32_t phase=n*67108864u;
       int x=120+gd_sine(phase)*(int)radius/32768,y=154-gd_sine(phase+0x40000000u)*(int)radius/32768;
       line(px,py,x,y,t->chrome);px=x;py=y;
     }
     uint32_t phase=lfo_phase_monitor[selected][k];
     int x=120+gd_sine(phase)*(int)radius/32768,y=154-gd_sine(phase+0x40000000u)*(int)radius/32768;
     for(unsigned n=0;n<12;++n){uint32_t trail=orbit_history[k][(orbit_write+n)%12];
       int tx=120+gd_sine(trail)*(int)radius/32768,ty=154-gd_sine(trail+0x40000000u)*(int)radius/32768;
       rect(tx-1,ty-1,3,3,visual_blend(color,t->bg,16+n*8));
     }
     line(120,154,x,y,color);rect(x-3,y-3,7,7,color);
   }
   rect(94,143,52,20,t->bg);text(96,145,"1 / 2",t->text,FONT_SMALL);
   text(8,220,"LFO: 1 / 2 / ORBITS",t->chrome,FONT_SMALL);return;
 }

 last_mode=lfo_display;
 if(last_lane!=selected||last_lfo!=lfo){for(unsigned i=0;i<96;++i)history[i]=0;history_write=0;last_lane=selected;last_lfo=lfo;}
 int value=lfo_value_monitor[selected][lfo];
 history[history_write++%96]=(int16_t)value;
 uint32_t phase=lfo_phase_monitor[selected][lfo];
 rect(0,70,240,77,t->bg);
 text(8,72,m->shape==4?"SAMPLE / HOLD HISTORY":"AUDIO PHASE + DEPTH",t->chrome,FONT_SMALL);
 frame(6,91,228,53,t->surface);line(8,117,231,117,t->surface);
 int previous=117,reference=117;
 for(unsigned x=0;x<224;++x){
  uint32_t ph=(uint32_t)(((uint64_t)x<<32)/224u);int y;
  if(m->shape==0)y=gd_sine(ph);
  else if(m->shape==1)y=gd_wave(ph,0);
  else if(m->shape==2)y=(ph>>31)?-32767:32767;
  else if(m->shape==3)y=(int)(ph>>16)-32768;
  else y=history[(history_write+x*95/223)%96];
  int ref=117-y*22/32768,depth=117-y*(int)m->depth*22/lfo_depth_scale;
  if(x){if(m->shape!=4)line(7+(int)x,reference,8+(int)x,ref,t->chrome);line(7+(int)x,previous,8+(int)x,depth,t->accent);}
  reference=ref;previous=depth;
 }
 int cursor=m->shape==4?231:8+(int)(((uint64_t)phase*223)>>32);
 int y=117-value*(int)m->depth*22/lfo_depth_scale;
 if(m->shape!=4)line(cursor,93,cursor,141,t->alt);
 rect(cursor-2,y-2,5,5,t->text);
}
static void format_glitch_rate(char *value,unsigned v){if(!v)string_copy(value,"OFF");else{unsigned tenths=(unsigned)((uint64_t)44100u*65536u*10u/gd_roll_frames(ui.bpm,(unsigned)v));if(tenths<1000){format_int(value,(int)(tenths/10));unsigned n=string_length(value);value[n++]='.';value[n++]=(char)('0'+tenths%10);value[n]=0;}else format_int(value,(int)((tenths+5)/10));unsigned n=string_length(value);string_copy(value+n,"Hz");}}
static void format_glitch_shift(char *out){
 if(roll_ui){out[0]='K';format_int(out+1,(int)glitch_key_ui+1);unsigned n=string_length(out);out[n++]=' ';out[n]=0;}else out[0]=0;
 unsigned n=string_length(out);string_copy(out+n,"SHIFT");n=string_length(out);if(glitch_offset>=0){out[n++]='+';out[n]=0;}format_int(out+n,glitch_offset);
}
static void draw_remix(void){
 const GDTheme *t=theme();rect(0,46,240,194,t->bg);
 GDRemix preview=remix_ui;unsigned length=gd_pattern_length(&ui);gd_remix_make(&preview,length);
 const int gx=20,gy=48,cw=13,ch=8;
 rect(gx,gy,208,128,visual_blend(t->surface,t->bg,50));
 for(unsigned beat=0;beat<4;++beat)rect(gx+(int)beat*52,gy,13,128,visual_blend(t->chrome,t->bg,20));
 if(length<16){rect(gx+(int)length*cw,gy,(int)(16-length)*cw,128,t->bg);rect(gx,gy,208,(int)(16-length)*ch,t->bg);}
 for(unsigned i=0;i<=16;++i){line(gx+(int)i*cw,gy,gx+(int)i*cw,gy+128,t->surface);line(gx,gy+(int)i*ch,gx+208,gy+(int)i*ch,t->surface);}
 for(unsigned i=0;i<length;++i){unsigned source=preview.map[i];rect(gx+(int)i*cw+1,gy+(int)(15-source)*ch+1,11,6,remix_active?t->accent:visual_blend(t->accent,t->bg,65));}
 if(playing){unsigned step=clockhead%length,source=preview.map[step];int x=gx+(int)step*cw,y=gy+(int)(15-source)*ch;frame(x,gy,13,128,t->alt);if(remix_active)frame(x,y,13,8,t->text);}
 text(1,48,"16",t->text,FONT_SMALL);text(8,104,"8",t->chrome,FONT_SMALL);text(8,160,"1",t->text,FONT_SMALL);
 static const char *const names[4]={"VAR","GLITCH","MIX","DEPTH"};
 for(unsigned k=0;k<4;++k){char value[16];unsigned v=k==0?remix_ui.variation:k==3?remix_ui.depth:ui.fx[k==1?9:10];if(k==1)format_glitch_rate(value,v);else format_int(value,k==2||k==3?(int)(v*100/127):(int)v);int x=5+(int)k*59;text(x,183,names[k],t->text,FONT_SMALL);text(x,203,value,k==1?t->alt:t->accent,FONT_SMALL);}
 char hint[40];format_glitch_shift(hint);string_copy(hint+string_length(hint)," ENV+KEY:PRINT");text(8,225,hint,t->chrome,FONT_SMALL);
}
static void draw_controls(void) {
  if(view==V_REMIX){draw_remix();return;}
  if(view==V_GLOBAL&&page==2){draw_midi_page();return;}
  if(view==V_CHAIN){
    const GDTheme *t=theme();rect(0,70,240,170,t->bg);
    for(unsigned row=0;row<16;++row){int x=4+(int)(row%4)*59,y=72+(int)(row/4)*27;frame(x,y,55,24,row==chain_cursor?t->accent:t->chrome);
      if(chain_running&&row==chain_row)frame(x+1,y+1,53,22,t->alt);
      char b[12];format_int(b,(int)row+1);unsigned n=(unsigned)string_length(b);b[n++]=' ';b[n++]=(char)('A'+(chain_ui.row[row]&15));format_int(b+n,1+(chain_ui.row[row]>>4));
      text(x+3,y+4,b,row<chain_ui.length?t->text:t->chrome,FONT_SMALL);
    }
    text(8,184,"SEL:END HERE / SAVE",t->chrome,FONT_SMALL);
    static const char *const names[4]={"ROW","PAT","REPS","MODE"};
    for(unsigned k=0;k<4;++k){char b[12];if(k==0)format_int(b,(int)chain_cursor+1);else if(k==1){b[0]=(char)('A'+(chain_ui.row[chain_cursor]&15));b[1]=0;}else if(k==2)format_int(b,1+(chain_ui.row[chain_cursor]>>4));else string_copy(b,chain_ui.mode==0?"OFF":chain_ui.mode==1?"LOOP":"ONCE");
      text(6+(int)k*58,204,names[k],t->chrome,FONT_SMALL);text(6+(int)k*58,222,b,t->accent,FONT_SMALL);}
    return;
  }
  if(view==V_PATTERNS){
    const GDTheme *t=theme();rect(0,70,240,170,t->bg);
    for(unsigned n=0;n<16;++n){int x=4+(int)(n%4)*59,y=72+(int)(n/4)*28;rect(x,y,55,25,n==active_pattern?t->surface:t->bg);frame(x,y,55,25,n==pattern_cursor?t->accent:t->chrome);
      char name[]={(char)('A'+n),0};text(x+5,y+4,name,n==active_pattern?t->alt:t->text,FONT_SMALL);
      text(x+25,y+8,n==active_pattern?"ACT":pattern_busy&&n==queued_pattern?"QUE":"",t->accent,FONT_SMALL);}
    text(8,186,"KEYS:A-P  2X KEY:NOW",t->text,FONT_SMALL);
    text(8,202,"ENV:SEL COPY ARP CLEAR",t->accent,FONT_SMALL);
    text(8,218,"SAVE 12268 / 12276 B",t->chrome,FONT_SMALL);
    return;
  }
  if(view==V_LFO&&lfo_display==2)return;
  if(view==V_PRESETS){draw_preset_browser();return;}
  if(view==V_PUNCH){draw_punch_pads();return;}

  if (view == V_VOICE && !voice_focus) { draw_voice_rows(); return; }
  if(view==V_MIX||view==V_HOME){draw_mixer();return;}
  const GDTheme *t = theme();
  rect(0, view==V_LFO?148:128, 240, view==V_LFO?92:112, t->bg);
  for (unsigned k = 0; k < 4; ++k) {
    GDControl c;
    int v;
    control_info(k, &c, &v);
    char value[24];
    format_value(value, c, v);
    if (view == V_LFO) {
      if(k==2)format_int(value,v*100/127);
      if (k == 0)
        string_copy(value, gd_lfo_waves[v % 5]);
      if (k == 1)
        string_copy(value, gd_lfo_rates[v % 12]);
      if (k == 3) {
        const char *name = gd_dest[v % 9];
        if (v == 3)
          name = selected == 6 ? "SIZE" : selected == 7 ? "FM" : "DECAY";
        if (v == 4)
          name = selected == 6   ? "POSITION"
                 : selected == 7 ? "OSC MIX"
                                 : "TONE";
        if (v == 5)
          name = selected == 6 ? "FOLD" : selected==1?"NOISE":"DRIVE";
        if (v == 8)
          name = selected == 6 ? "OSC MIX" : selected == 7 ? "NOISE" : "TONE";
        if (v == 2 && selected < 6)
          name = "TONE";
        string_copy(value, name);
      }
    }
    if(view==V_FXEDIT){
      if(fx_selected<8){
        if(k==0)format_int(value,(int)(gd_capture_size(&ui,fx_selected)*1000u/44100u));
        if(k==1){if(fx_selected==6)format_int(value,(int)(gd_brake_time(&ui)*1000u/44100u));else format_int(value,(50+v*100/128)/(fx_selected==7?2:1));}
        if(k==2){if(fx_selected==6)format_hundredths(value,(v-64)*100/(v<64?64:63));else {unsigned seam=2u+(unsigned)v*2u,length=gd_capture_size(&ui,fx_selected);if(seam>length/2)seam=length/2;format_hundredths(value,(int)(seam*100000u/44100u));}}
      }
      else if((fx_selected==8||fx_selected==9||fx_selected==25||fx_selected==26)&&k==0)format_int(value,(int)((44100u*60u/ui.bpm)*(32u+(unsigned)v)/(fx_selected<10?64u:32u)*1000u/44100u));
      else if(fx_selected==10&&k==0)format_int(value,50+v*v/48);
      else if(fx_selected==10&&k==1)format_int(value,512+v*v/2);
      else if(fx_selected==10&&k==2)format_int(value,(64+v)*100/64);
      else if((fx_selected==15||fx_selected==21)&&k==0)format_hundredths(value,fx_selected==15?10+v*30:5+v*3);
      else if((fx_selected==15||fx_selected==21)&&k==2)format_int(value,v*100/170);
      else if(fx_selected>=16&&fx_selected<=18&&k==1)format_int(value,fx_selected==18?256+v*v*9/16:50+v*v/20);
      else if(fx_selected==17&&k==2){int low=50+ui.effect[17][1]*ui.effect[17][1]/20,high=256+v*v*9/16;if(high<low+16)high=low+16;format_int(value,high);}
      else if((fx_selected>=16&&fx_selected<=18&&k==3)||(fx_selected==24&&k==3))format_int(value,v*150/127);
      else if((fx_selected==24&&k==2)||(fx_selected==19&&k==2))format_hundredths(value,(v-64)*100/(v<64?64:63));
      else if(((fx_selected==27||fx_selected==29)&&k==0)|| (fx_selected==23&&k==0)){
        int actual=fx_selected==27?(882+v*48)*1000/44100:fx_selected==29?(530+v*24)*1000/44100:55+v*6;
        format_int(value,actual);
      }else if(fx_selected==20&&k==0)format_int(value,4+v*v/8);
      else if(fx_selected==20&&k==2){static const char *const wave[3]={"SINE","TRI","SQUARE"};string_copy(value,wave[v]);}
      else if((fx_selected==20||fx_selected==11||fx_selected==13||fx_selected==22)&&k==3)format_int(value,v*150/127);
      else if(fx_selected==11&&k==0)format_int(value,15-v);
      else if((fx_selected==13||fx_selected==24)&&k==0){static const char *const rate[8]={"1/32","1/16","3/32","1/8","3/16","1/4","3/8","1/2"};string_copy(value,rate[v]);}
      else if(fx_selected==14&&k==1){unsigned distance=(44100u*60u/(ui.bpm*4u))*gd_time_mult[v];if(distance>32767)distance=32767;format_int(value,(int)(distance*1000u/44100));}
      else if(fx_selected==22&&k==0){static const char *const rate[8]={"4 BAR","2 BAR","4/3 BAR","1 BAR","2/3 BAR","1/2 BAR","1/3 BAR","1/4 BAR"};string_copy(value,rate[v]);}
      else if(fx_selected==19&&k==1)format_int(value,(64+v)*100/128);
      else if(fx_selected==29&&k==3)format_int(value,(32768+v*774)*100/65536);
      else if(!c.max)string_copy(value,"--");
    }
    if (view == V_GLOBAL && page==1 && k == 2)
      string_copy(value, theme()->name);
    if(view==V_GLOBAL&&page%2&&k==0){static const char *const quality[3]={"BAL 2X","HIGH 4X","ECO 1X"};string_copy(value,quality[v%3]);}
    if(view==V_GLOBAL&&page%2&&k>2)string_copy(value,"--");
    if (view == V_STEP && k == 0)
      format_int(value, v - 64);
    if (view == V_FX && page % 3 == 0 && k == 1) {
      static const unsigned numerator[6]={1,1,3,1,3,1}, denominator[6]={16,8,16,4,8,2};
      unsigned folds=0;gd_delay_frames(ui.bpm,v,&folds);
      format_int(value,numerator[v%6]);unsigned n=string_length(value);value[n++]='/';
      format_int(value+n,denominator[v%6]<<folds);
    }
    if (view == V_FX && page % 3 == 2 && k == 1) {
      format_glitch_rate(value,(unsigned)v);
    }
    if (view == V_FX && gd_master_fx_index(page,k)==5)
      format_int(value, 16 - v);
    if (view == V_FX && gd_master_fx_index(page,k)==6)
      format_int(value, 1 + v / 2);
    int x = 8 + (int)(k % 2) * 120, y = view==V_LFO?148+(int)(k/2)*46:130+(int)(k/2)*54;
    text(x, y, c.name, view==V_STEP?t->text:t->chrome, FONT_SMALL);
    text(x, y + 17, value, k % 2 ? t->alt : t->accent,
         view==V_LFO||string_length(value) > 6 ? FONT_SMALL : FONT_BIG);
    rect(x, y + (view==V_LFO?40:49), 104, 2, t->surface);
    if (c.max)
      rect(x, y + (view==V_LFO?40:49), gd_clamp(v * 104 / c.max, 0, 104), 2,
           k % 2 ? t->alt : t->accent);
    if (c.format == F_TIME||(c.format>=F_KICK&&c.format<F_KICK+16&&(c.format==F_KICK+1||c.format==F_KICK+5||c.format==F_KICK+7||c.format==F_KICK+8||c.format==F_KICK+10||c.format==F_KICK+12||c.format==F_KICK+13)))
      text(x + 80, y + 30, "ms", t->chrome, FONT_SMALL);
    if (c.format == F_HZ)
      text(x + 80, y + 30, "Hz", t->chrome, FONT_SMALL);

  }
}
static void redraw(void) {
  static unsigned drawn_view = 255;
  static uint32_t remix_draw_at;
  if (drawn_view != view) { drawn_view = view; need_full = 1; }
  if (need_full) {
    remix_draw_at=0;
    clear(theme()->bg);
    need_header = need_grid = need_controls = 1;
    need_full = 0;
  }
  if (need_header) {
    draw_header();
    need_header = 0;
  }
  if (need_grid) {
    if(view==V_LFO)draw_lfo_visual();
    else if(view==V_FXEDIT){const GDTheme *t=theme();rect(0,70,240,58,t->bg);text(8,74,"SELECT / ALG: EFFECT",t->text,FONT_SMALL);text(8,92,punch_latched&(1u<<fx_selected)?"LOCKED / TAP KEY:OFF":"KEY:PUNCH / ENV:LATCH",t->accent,FONT_SMALL);text(8,110,"SEL:CLEAR ALL LATCHES",t->chrome,FONT_SMALL);}
    else if(view==V_VOICE&&voice_focus)draw_voice_scope();
    else if(view==V_MIX||view==V_HOME)draw_mixer();
    else if(view==V_STEP)draw_grid();
    else if(view==V_FX&&page%3==2){const GDTheme *t=theme();rect(0,70,240,58,t->bg);text(8,74,"KEYS: LARGE -> SMALL",t->text,FONT_SMALL);text(8,94,"KN2:OFFSET KN3:MIX",t->text,FONT_SMALL);char hint[32];format_glitch_shift(hint);text(8,112,hint,roll_ui?t->accent:t->chrome,FONT_SMALL);}
    else if(!(view==V_GLOBAL&&page==2)&&view!=V_VOICE&&view!=V_PUNCH&&view!=V_PRESETS&&view!=V_PATTERNS&&view!=V_CHAIN&&view!=V_REMIX)rect(0,70,240,58,theme()->bg);
    need_grid = 0;
  }
  if (need_controls) {
    if(view==V_REMIX&&remix_draw_at&&(uint32_t)(millis()-remix_draw_at)<33)return;
    if(view==V_REMIX)remix_draw_at=millis();
    draw_controls();
    need_controls = 0;
  }
}

static int project_valid(const GDProject *p) {
  if((p->reserved&3)>2||p->reserved>15)return 0;
  if (p->magic != 0x47444631u || p->version != 9 || p->bpm < 40 ||
      p->bpm > 240 || p->pages != 1 || gd_swing_amount(p->swing) > 100 ||
      p->theme >= 38)
    return 0;
  for (unsigned l = 0; l < 8; ++l) {
    const GDTrack *t = &p->track[l];
    if (!t->length || t->length > 16 || t->mute > 1)
      return 0;
    for (unsigned j = 0; j < 32; ++j) {
      unsigned max = l == 6 ? gd_gctrl[j].max : l == 7 ? gd_actrl[j].max : 127;
      if (l == 7 && max == 0)
        max = 127;
      if (t->param[j] > max)
        return 0;
    }
    for (unsigned k = 0; k < 2; ++k)
      if (t->lfo[k].shape > 4 || t->lfo[k].rate > 11 || t->lfo[k].target > 8 ||
          t->lfo[k].depth > 127)
        return 0;
    for (unsigned s = 0; s < 64; ++s)
      if (t->step[s] > 127 || t->chance[s] > 100 || t->ratchet[s] < 1 ||
          t->ratchet[s] > 4 || t->pitch[s] < -48 || t->pitch[s] > 48 ||
          (t->lock_id[s] != 255 && t->lock_id[s] >= 32) ||
          t->lock_value[s] > 127)
        return 0;
  }
  for (unsigned i = 0; i < 12; ++i)
    if (p->fx[i] > gd_fxctrl[i].max)
      return 0;
  for(unsigned e=0;e<30;++e)for(unsigned k=0;k<4;++k)if(p->effect[e][k]>127)return 0;
  for(unsigned k=0;k<4;++k){if(p->slot[k]>=30)return 0;for(unsigned n=0;n<k;++n)if(p->slot[k]==p->slot[n])return 0;}
  for(unsigned i=0;i<4;++i)if(p->punch[i]>(i==1?1:127))return 0;
  return 1;
}
static int bindings_valid(void){
 for(unsigned l=0;l<8;++l)for(unsigned n=0;n<2;++n)if(preset_bank.voice_used[l]&(1u<<n)){
  if(voice_bind[l][n]>=10)return 0;for(unsigned j=0;j<n;++j)if((preset_bank.voice_used[l]&(1u<<j))&&voice_bind[l][j]==voice_bind[l][n])return 0;
 }
 return (!preset_bank.kit_used||kit_bind<5)&&(!preset_bank.scene_used||scene_bind<5);
}
static int restore_current(void){
 int latest=settings_load(&save_scratch.current,sizeof save_scratch.current)==0&&pattern_save.magic==GD16_SAVE_MAGIC&&(pattern_save.version==5||pattern_save.version==6||pattern_save.version==7||pattern_save.version==8);
 if(!latest&&(settings_load(&save_scratch.current,12260)||pattern_save.magic!=GD16_SAVE_MAGIC||pattern_save.version!=4))return 0;
 old_swing_format=pattern_save.version<8;
 old_roll_format=pattern_save.version<6?1:pattern_save.version==6?2:0;
 if(pattern_save.active>=16)return 0;
 if(latest)for(unsigned lane=0;lane<8;++lane){if(save_scratch.current.delay_send[lane]>127)return 0;delay_send[lane]=save_scratch.current.delay_send[lane];}
 storage=(GDSave){0};gd_defaults(&ui);GDKit kit=gd_unpack_full_kit(&pattern_save.project);gd_apply_preset(&ui,&kit,1,0);
 ui.theme=pattern_save.theme;ui.swing=pattern_save.swing;ui.reserved=pattern_save.quality;ui.bpm=pattern_save.bpm;
 for(unsigned l=0;l<8;++l){preset_bank.voice_used[l]=pattern_save.voice_used[l];if(preset_bank.voice_used[l]>3)return 0;for(unsigned n=0;n<2;++n){preset_bank.voice[l][n]=gd_unpack_sound(&pattern_save.voice[l][n]);voice_bind[l][n]=pattern_save.voice_bind[l][n];}}
 preset_bank.kit_used=pattern_save.kit_used;preset_bank.scene_used=pattern_save.scene_used;if(preset_bank.kit_used>1||preset_bank.scene_used>1)return 0;
 preset_bank.kit[0]=gd_unpack_full_kit(&pattern_save.kit);preset_bank.scene[0]=gd_unpack_full_scene(&pattern_save.scene);kit_bind=pattern_save.kit_bind;scene_bind=pattern_save.scene_bind;
 for(unsigned slot=0;slot<16;++slot)if(!gd_pattern_decode(&pattern_bank[slot],pattern_save.pattern[slot],GD_PATTERN_BYTES))return 0;
 active_pattern=pattern_save.active;gd_pattern_put(&ui,&pattern_bank[active_pattern]);if(gd_chain_valid(&chain_save.chain))chain_ui=chain_save.chain;
 for(unsigned n=0;n<16;++n){GDMidiMap *m=&save_scratch.current.map[n];if((m->cc&128)&&gd_midi_map_valid(m))gd_midi_bind(midi_map,m->route&15,m->cc&127,(GDMidiTarget){m->route>>4,m->index});}
 return project_valid(&ui)&&gd_bank_valid(&preset_bank)&&bindings_valid();
}
static int restore_four(void){
 old_swing_format=1;
 old_roll_format=1;
  int learned=settings_load(&save_scratch.four,sizeof save_scratch.four)==0&&save_scratch.four.base.base.magic==GD16_SAVE_MAGIC&&save_scratch.four.base.base.version==3;
 int chained=learned;
 if(!learned)chained=settings_load(&save_scratch.four.base,sizeof save_scratch.four.base)==0&&save_scratch.four.base.base.magic==GD16_SAVE_MAGIC&&save_scratch.four.base.base.version==2;
 if(!chained&&(settings_load(&save_scratch.four.base.base,sizeof save_scratch.four.base.base)||save_scratch.four.base.base.magic!=GD16_SAVE_MAGIC||save_scratch.four.base.base.version!=1))return 0;
 if(save_scratch.four.base.base.active>=16)return 0;
 if(chained&&gd_chain_valid(&save_scratch.four.base.chain))chain_ui=save_scratch.four.base.chain;

 storage=(GDSave){0};gd_defaults(&ui);GDKit kit=gd_restore_kit(&save_scratch.four.base.base.project);gd_apply_preset(&ui,&kit,1,0);
 ui.theme=save_scratch.four.base.base.theme;ui.swing=save_scratch.four.base.base.swing;ui.reserved=save_scratch.four.base.base.quality;ui.bpm=save_scratch.four.base.base.bpm;
 for(unsigned l=0;l<8;++l){preset_bank.voice_used[l]=save_scratch.four.base.base.voice_used[l];if(preset_bank.voice_used[l]>3)return 0;for(unsigned n=0;n<2;++n){preset_bank.voice[l][n]=save_scratch.four.base.base.voice[l][n];voice_bind[l][n]=save_scratch.four.base.base.voice_bind[l][n];}}
 preset_bank.kit_used=save_scratch.four.base.base.kit_used;preset_bank.scene_used=save_scratch.four.base.base.scene_used;if(preset_bank.kit_used>1||preset_bank.scene_used>1)return 0;
 preset_bank.kit[0]=gd_restore_kit(&save_scratch.four.base.base.kit);preset_bank.scene[0]=gd_restore_scene(&save_scratch.four.base.base.scene);kit_bind=save_scratch.four.base.base.kit_bind;scene_bind=save_scratch.four.base.base.scene_bind;
 for(unsigned slot=0;slot<16;++slot)if(!gd_pattern_decode(&pattern_bank[slot],save_scratch.four.base.base.pattern[slot],GD_PATTERN_BYTES))return 0;
 active_pattern=save_scratch.four.base.base.active;gd_pattern_put(&ui,&pattern_bank[active_pattern]);
 if(learned)for(unsigned n=0;n<16;++n){GDMidiMap m=save_scratch.four.map[n];if(!(m.cc&128))continue;unsigned kind=m.route>>4;
  if(kind==M_EFFECT){if(m.index>=16)continue;m.index=(uint8_t)(ui.slot[m.index/4]*4+m.index%4);}
  if(kind==M_GATE){if(m.index>=4)continue;m.index=ui.slot[m.index];}
  if(gd_midi_map_valid(&m))gd_midi_bind(midi_map,m.route&15,m.cc&127,(GDMidiTarget){kind,m.index});
 }
 return project_valid(&ui)&&gd_bank_valid(&preset_bank)&&bindings_valid();
}
static void import_old_pattern(unsigned slot,const GD64Pattern *old,unsigned *extra){
 gd_import_bar(&pattern_bank[slot],old,0);
 for(unsigned bar=1;bar<4;++bar)if(gd_old_bar_used(old,bar)){
  if(*extra<16)gd_import_bar(&pattern_bank[(*extra)++],old,bar);else import_overflow=1;import_split=1;
 }
}
static int restore_legacy(void){
 old_swing_format=1;
 old_roll_format=1;
 unsigned kind=0;
 if(settings_load(&save_scratch.eight,sizeof save_scratch.eight)==0&&save_scratch.eight.prefix.version==4)kind=4;
 else if(settings_load(&save_scratch.ab,sizeof save_scratch.ab)==0&&save_scratch.ab.prefix.version==3)kind=3;
 else if(settings_load(&save_scratch.song,sizeof save_scratch.song)==0&&save_scratch.song.prefix.version==2)kind=2;
 else if(settings_load(&save_scratch.prefix,sizeof save_scratch.prefix)==0&&save_scratch.prefix.version==1)kind=1;
 if(!kind||save_scratch.prefix.magic!=GD_SAVE_MAGIC||save_scratch.prefix.project_size!=sizeof(GDOldProject))return 0;
 gd_import_prefix(&storage,&save_scratch.prefix);
 if(ui.version==4){gd_upgrade_drum_project(&ui);gd_upgrade_drum_bank(&preset_bank);}
 if(ui.version==5){gd_upgrade_init_kick(&ui);gd_upgrade_init_kick_bank(&preset_bank);}
 if(ui.version==6){gd_upgrade_voice_dsp(&ui);gd_upgrade_voice_bank(&preset_bank);}
 if(ui.version==7){gd_upgrade_performance(&ui);gd_upgrade_performance_bank(&preset_bank);}
 if(ui.version!=8)return 0;ui.version=9;
 static GD64Pattern old;gd64_pattern_blank(&old);gd64_pattern_get(&old,&ui);
 unsigned extra=8;active_pattern=kind==4?save_scratch.eight.active:kind==3?save_scratch.ab.active:0;
 if(active_pattern>=(kind==4?8u:2u))return 0;
 if(kind==4){unsigned at=0;
  for(unsigned slot=0;slot<8;++slot){unsigned size=save_scratch.eight.size[slot];if(size>3712-at)return 0;
   if(slot==active_pattern){gd64_pattern_blank(&old);gd64_pattern_get(&old,&ui);if(!gd64_chords_decode(&old,save_scratch.eight.bytes+at,size))return 0;}
   else if(!gd64_pattern_decode(&old,save_scratch.eight.bytes+at,size))return 0;
   import_old_pattern(slot,&old,&extra);at+=size;
  }
 }else{import_old_pattern(active_pattern,&old,&extra);
  if(kind==3){gd64_pattern_blank(&old);old.pages=save_scratch.ab.alternate.pages;for(unsigned l=0;l<8;++l)old.lane[l]=save_scratch.ab.alternate.lane[l];if(!gd64_pattern_valid(&old))return 0;import_old_pattern(1-active_pattern,&old,&extra);}
 }
 gd_pattern_put(&ui,&pattern_bank[active_pattern]);
 return project_valid(&ui)&&gd_bank_valid(&preset_bank);
}
void setup(void){
 for(unsigned lane=0;lane<8;++lane)delay_send[lane]=127;
 gd_chain_defaults(&chain_ui);
 for(unsigned slot=0;slot<16;++slot)gd_pattern_blank(&pattern_bank[slot]);
 for(unsigned l=0;l<8;++l){voice_bind[l][0]=8;voice_bind[l][1]=9;}
 int restored=restore_current();if(!restored)restored=restore_four();if(!restored)restored=restore_legacy();
 if(restored&&old_swing_format){if(ui.swing>50)restored=0;else ui.swing=(uint8_t)(ui.swing*2u);}
 if(restored&&old_roll_format){ui.fx[9]=(uint8_t)(old_roll_format==1?gd_old_roll_rate(ui.fx[9],ui.bpm):gd_previous_glitch_rate(ui.fx[9],ui.bpm));for(unsigned n=0;n<4;++n){if(preset_bank.kit_used&(1u<<n))preset_bank.kit[n].scene.fx[9]=(uint8_t)(old_roll_format==1?gd_old_roll_rate(preset_bank.kit[n].scene.fx[9],ui.bpm):gd_previous_glitch_rate(preset_bank.kit[n].scene.fx[9],ui.bpm));if(preset_bank.scene_used&(1u<<n))preset_bank.scene[n].fx[9]=(uint8_t)(old_roll_format==1?gd_old_roll_rate(preset_bank.scene[n].fx[9],ui.bpm):gd_previous_glitch_rate(preset_bank.scene[n].fx[9],ui.bpm));}}
 if(!restored){for(unsigned lane=0;lane<8;++lane)delay_send[lane]=127;for(unsigned n=0;n<16;++n)midi_map[n]=(GDMidiMap){0};gd_chain_defaults(&chain_ui);storage=(GDSave){0};gd_defaults(&ui);active_pattern=0;import_overflow=import_split=0;for(unsigned slot=0;slot<16;++slot)gd_pattern_blank(&pattern_bank[slot]);gd_pattern_get(&pattern_bank[0],&ui);}
 for(unsigned lane=0;lane<8;++lane)engine.delay_send[lane]=delay_send[lane];
 storage.magic=GD_SAVE_MAGIC;storage.version=1;storage.project_size=sizeof(GDProject);
 pattern_cursor=active_pattern;live=ui;
 for(unsigned l=0;l<2;++l)for(unsigned s=0;s<16;++s){live_chord_count[l][s]=pattern_bank[active_pattern].chord_count[l][s];for(unsigned n=0;n<3;++n)live_chord_pitch[l][s][n]=pattern_bank[active_pattern].chord_pitch[l][s][n];}
 selected=0;mix_bank=0;display_lane=0;view=V_HOME;fx_slot=3;fx_selected=gd_keyboard_effect(fx_slot);need_full=1;
 if(import_overflow)message("OLD BANK >16 BARS: NO SAVE");else if(import_split)message("LONG PATTERNS SPLIT I-P");redraw();
}
static void set_step(unsigned s, unsigned v) {
  if (send(C_STEP, selected, s, v)) {
    mutation_state=0;
    ui.track[selected].step[s] = (uint8_t)v;
    if(!v&&selected>=6)pattern_bank[active_pattern].chord_count[selected-6][s]=0;
    need_grid = need_controls = 1;
  }
}
/* Four-note step overdub for the two polyphonic lanes; other drums stay mono. */
static void record_poly_note(unsigned lane,unsigned s,unsigned velocity,int pitch){
 if(pattern_busy||chain_running||(chain_ui.mode&&playing)||lane<6||s>=16||q_write-q_read>253)return;
 pitch=gd_clamp(pitch,-48,48);GDTrack *t=&ui.track[lane];GDPattern *p=&pattern_bank[active_pattern];
 unsigned replace=!t->step[s];
 if(!replace){if(pitch==t->pitch[s])return;for(unsigned n=0;n<p->chord_count[lane-6][s];++n)if(p->chord_pitch[lane-6][s][n]==pitch)return;if(p->chord_count[lane-6][s]>=3){message("STEP HAS FOUR NOTES");return;}}
 if(replace&&!send(C_STEP,lane,s,velocity))return;
 if(!send(C_RECORD_CHORD,lane,s,(unsigned)(pitch+64)|(replace?128u:0u)))return;
 if(replace){t->step[s]=(uint8_t)velocity;t->pitch[s]=(int8_t)pitch;p->chord_count[lane-6][s]=0;}
 else gd_chord_add(&p->chord_count[lane-6][s],p->chord_pitch[lane-6][s],t->pitch[s],pitch);
 mutation_state=0;need_header=need_grid=need_controls=1;
}
static void edit_chain(unsigned k,int delta){
 if(!delta)return;
 if(k==0){chain_cursor=(unsigned)gd_clamp((int)chain_cursor+delta,0,15);need_controls=1;return;}
 if(k==3){
  unsigned mode=(unsigned)gd_clamp((int)chain_ui.mode+delta,0,2);
  if(playing||chain_running){if(mode){message("STOP TO ENABLE/EDIT CHAIN");return;}if(!send(C_CHAIN_STOP,0,0,0)){message("AUDIO QUEUE BUSY");return;}}
  chain_ui.mode=(uint8_t)mode;if(mode)recording=0;
 }else{
  if(playing||chain_running){message("STOP TO EDIT CHAIN");return;}
  unsigned v=chain_ui.row[chain_cursor];
  if(k==1)chain_ui.row[chain_cursor]=(uint8_t)((v&240u)|(unsigned)gd_clamp((int)(v&15u)+delta,0,15));
  if(k==2)chain_ui.row[chain_cursor]=(uint8_t)((v&15u)|((unsigned)gd_clamp((int)(v>>4)+delta,0,15)<<4));
 }
 need_header=need_controls=1;
}
static void edit_control(unsigned k, int delta) {
  if(view==V_REMIX){if(!delta)return;if(midi_learn_on)midi_pick_knob(k);if(k==1){offset_glitch(delta);return;}int old=k==0?remix_ui.variation:k==3?remix_ui.depth:ui.fx[k==1?9:10];unsigned v=(unsigned)gd_clamp(old+delta,0,127);if(v==(unsigned)old)return;if(k==0||k==3){unsigned index=k==0?1:2;if(send(C_REMIX,0,index,v)){if(k==0)remix_ui.variation=(uint8_t)v;else remix_ui.depth=(uint8_t)v;remix_ui.dirty=1;}}else {unsigned index=k==1?9:10;if(send(C_FX,0,index,v)){ui.fx[index]=(uint8_t)v;if(index==10)glitch_prepared=1;}}need_controls=1;return;}
  if(view==V_GLOBAL&&page==2){
    if(delta&&k==0){if((delta>0)!=!!midi_learn_on)midi_toggle();}
    if(delta&&k==1){midi_map_cursor=(unsigned)gd_clamp((int)midi_map_cursor+delta,0,15);midi_target.kind=M_INVALID;midi_pick=0;need_controls=1;}
    return;
  }
  if(delta&&midi_learn_on)midi_pick_knob(k);
  if(view==V_CHAIN){edit_chain(k,delta);return;}
  if(view==V_PATTERNS){if(delta&&k==0){pattern_cursor=(unsigned)gd_clamp((int)pattern_cursor+delta,0,15);need_controls=1;}return;}
  if (!delta || view==V_PRESETS || (view==V_LFO&&lfo_display==2))
    return;
  if(view==V_FX&&page%3==2&&k==1){offset_glitch(delta);return;}
  GDControl c;
  int old;
  control_info(k, &c, &old);
  if(!c.max)return;
  int min = 0;
  if (view == V_STEP && k == 0)
    min = 16;
  if (view == V_STEP && k == 3)
    min = 1;
  if (view == V_GLOBAL && page%2==0 && k == 2)
    min = 1;
  if (view == V_GLOBAL && page%2==0 && k == 3)
    min = 40;
  if ((view == V_MIX||view==V_HOME)&&mix_mode==1&&k == 3)
    min = 1;
  if(view==V_GLOBAL&&page%2&&k==1)min=1;
  if (c.format == F_NOTE||c.format==F_AN||c.format==F_AN+1||c.format==F_GL+3||c.format==F_GL+23)
    min = 12;
  int v = gd_clamp(old + delta, min, c.max);
  if (v == old)
    return;
  if(view==V_GLOBAL&&page==0&&k==0){if(send(C_GLOBAL,0,5,(unsigned)v))ui.swing=(uint8_t)((ui.swing&127u)|(v?128u:0u));need_header=need_controls=1;return;}
  if(view==V_GLOBAL&&page==1&&k==2){if(send(C_GLOBAL,0,0,(unsigned)v))ui.theme=(uint8_t)v;need_full=1;return;}
  if(view==V_GLOBAL&&page%2){unsigned value=k==0?(ui.reserved&12u)|(unsigned)v:(ui.reserved&3u)|((v==4?0u:(unsigned)v)<<2);if(send(C_GLOBAL,0,4,value))ui.reserved=(uint8_t)value;need_header=need_controls=1;return;}
  if((view==V_MIX||view==V_HOME)&&mix_mode!=1){
    unsigned lane=mix_bank*4+k;
    if(mix_mode==2){if(send(C_SEND,lane,0,(unsigned)v))delay_send[lane]=(uint8_t)v;}
    else if(send(C_PARAM,lane,29,(unsigned)v))ui.track[lane].param[29]=(uint8_t)v;
    need_grid=need_controls=1;return;
  }
  if(view==V_FXEDIT || (view==V_PUNCH&&k==3)){
    unsigned idx=view==V_PUNCH?0:k;
    if(view==V_FXEDIT)v*=(int)effect_step(fx_selected,k);
    if(send(C_EFFECT,0,fx_selected*4+idx,(unsigned)v))ui.effect[fx_selected][idx]=(uint8_t)v;
    need_controls=1;return;
  }
  if(view==V_PUNCH){if(send(C_PUNCH,0,k,(unsigned)v))ui.punch[k]=(uint8_t)v;need_controls=1;return;}
  GDTrack *t = &ui.track[selected];
  if (view == V_VOICE) {
    unsigned idx = parameter(k);
    int held = held_step();
    if (held >= 0) {
      if (send_word((C_LOCK << 24) | (idx << 19) | (selected << 16) |
                    ((unsigned)held << 8) | (unsigned)v)) {
        t->lock_id[held] = (uint8_t)idx;
        t->lock_value[held] = (uint8_t)v;
        message("STEP LOCKED");
      }
    } else if (send(C_PARAM, selected, idx, (unsigned)v))
      t->param[idx] = (uint8_t)v;
  } else if (view == V_LFO) {
    if (send(C_LFO, selected, lfo * 4 + k, (unsigned)v)) {
      GDLFO *m = &t->lfo[lfo];
      if (k == 0)
        m->shape = (uint8_t)v;
      if (k == 1)
        m->rate = (uint8_t)v;
      if (k == 2)
        m->depth = (uint8_t)v;
      if (k == 3)
        m->target = (uint8_t)v;
    }
  } else if (view == V_FX) {
    unsigned idx = gd_master_fx_index(page,k);
    if (send(C_FX, 0, idx, (unsigned)v)){
      ui.fx[idx] = (uint8_t)v;if(idx==10)glitch_prepared=1;}
  } else if (view == V_GLOBAL) {
    if (send(C_GLOBAL, 0, k, (unsigned)v)) {
      if (k == 0) {
        ui.theme = (uint8_t)v;
        need_full = 1;
      }
      if (k == 1)
        ui.swing = (uint8_t)((ui.swing&128u)|(unsigned)v);
      if (k == 2){for(unsigned lane=0;lane<8;++lane)ui.track[lane].length=(uint8_t)v;need_grid=1;}
      if (k == 3)
        ui.bpm = (unsigned)v;
      need_header = 1;
    }
  } else if (view == V_STEP) {
    unsigned kind = k == 0   ? C_PITCH
                    : k == 1 ? C_STEP
                    : k == 2 ? C_CHANCE
                             : C_RATCHET;
    if (send(kind, selected, step_selected, (unsigned)v)) {
      if(kind==C_STEP)mutation_state=0;
      if (k == 0)
        t->pitch[step_selected] = (int8_t)(v - 64);
      if (k == 1){t->step[step_selected]=(uint8_t)v;if(!v&&selected>=6)pattern_bank[active_pattern].chord_count[selected-6][step_selected]=0;need_header=1;}
      if (k == 2)
        t->chance[step_selected] = (uint8_t)v;
      if (k == 3)
        t->ratchet[step_selected] = (uint8_t)v;
      need_grid = 1;
    }
  } else {
    if (k < 2) {
      unsigned idx = 29 + k;
      if (send(C_PARAM, selected, idx, (unsigned)v))
        t->param[idx] = (uint8_t)v;
    }
    if (k == 2 && send(C_MUTE, selected, 0, (unsigned)v)) {
      t->mute = (uint8_t)v;
      need_grid = 1;
    }
    if (k == 3 && send(C_LENGTH, selected, 0, (unsigned)v))
      t->length = (uint8_t)v;
  }
  need_controls = 1;
}
static int toggle_solo(unsigned lane){
 uint32_t next=solo_ui==(1u<<lane)?0:1u<<lane;
 if(!send(C_SOLO,0,0,next))return 0;
 solo_ui=next;need_header=need_grid=1;message(next?"TRACK SOLO":"FULL MIX RESTORED");return 1;
}
void loop(void) {
  static uint32_t last_step = 99,last_visual,last_chain_row=99,last_chain_pass=99;
  static uint32_t key_step = 99, key_step_at;
  static uint32_t mutation_seed = 0x1234;
  finish_chain_switch();finish_pattern_switch();
  static unsigned last_remix_clock=255;if(view==V_REMIX&&last_remix_clock!=clockhead){last_remix_clock=clockhead;need_controls=1;}
  if(last_chain_row!=chain_row||last_chain_pass!=chain_pass){last_chain_row=chain_row;last_chain_pass=chain_pass;if(view==V_CHAIN)need_header=need_controls=1;}
  if(fx_surface())fx_selected=gd_keyboard_effect(fx_slot);
  remember_navigation();
  if(button_pressed(BTN_ENV))env_consumed=0;
  if (button_pressed(BTN_PLAY))
    set_play(!playing);
  if (button_pressed(BTN_REC)&&view==V_REMIX){message("EXIT SLICE TO RECORD");}
  else if (button_pressed(BTN_REC)) {
    if(chain_ui.mode||chain_running)message("CHAIN OFF TO RECORD");else recording = !recording;
    need_header = 1;
  }
  if (button_pressed(BTN_HOME)) {
    if(view==V_MIX||view==V_HOME)mix_mode=(mix_mode+1)%3;
    view = V_MIX;
    sequence_mode = 0;
    page = 0;
    need_header = need_controls = need_grid = 1;
  }
  if (button_pressed(BTN_EDIT)) {
    if(fx_surface()){view=view==V_PUNCH?V_FXEDIT:V_PUNCH;page=0;}
    else {
      if(view==V_VOICE){if(lock_edit){lock_edit=0;voice_focus=voice_style[selected];page=voice_row[selected];}else voice_focus=!voice_focus;}
      else {lock_edit=view==V_STEP;voice_focus=lock_edit?0:voice_style[selected];page=voice_row[selected];}
      view=V_VOICE;
    }
    need_header = need_controls = need_grid = 1;
  }
  if (button_pressed(BTN_SEQ)) {
    view = view==V_STEP?V_REMIX:view==V_REMIX?V_PATTERNS:view==V_PATTERNS?V_CHAIN:V_STEP;
    if(view==V_REMIX){recording=0;sel_pending=0;if(!glitch_prepared){if(ui.fx[10])glitch_prepared=1;else if(send(C_FX,0,10,127)){ui.fx[10]=127;glitch_prepared=1;}}}
    pattern_cursor=active_pattern;pattern_last_key=255;
    sequence_mode = view==V_STEP;
    need_header = need_controls = 1;
  }
  if (button_pressed(BTN_LFO)) {
    lfo_display=view==V_LFO?(lfo_display+1)%3:lfo_page[selected];
    lfo=lfo_display<2?lfo_display:0;page=lfo_display;
    view = V_LFO;
    need_header = need_grid = need_controls = 1;
  }
  if (button_pressed(BTN_FX)) {
    if(view==V_FXEDIT){view=V_PUNCH;page=fx_bank;}
    else if(view==V_PUNCH){view=V_FX;page=0;}
    else if(view==V_FX&&page<2){++page;}
    else if(view==V_FX){view=V_PUNCH;page=fx_bank;}
    else{view=fx_return_view;page=view==V_FX?master_fx_page:fx_bank;}
    need_header = need_grid = need_controls = 1;
  }
  if (button_pressed(BTN_GLO)) {
    if(view!=V_GLOBAL){midi_return_view=view;midi_return_page=page;}
    page=view==V_GLOBAL?(page+1)%3:global_page;
    view = V_GLOBAL;
    /* The MIDI map page draws into the middle region as well as the controls. */
    need_header = need_grid = need_controls = 1;
  }
  if (button_released(BTN_ENV)&&!env_consumed && fx_surface()){punch_latch_mode^=1;need_header=need_controls=1;}
  else if (button_released(BTN_ENV)&&!env_consumed) {
    view = V_VOICE;
    voice_focus=0;
    page = selected == 6 || selected == 5 ? 4 : selected == 7 ||selected<6 ? 3 : 0;
    need_header = need_controls = 1;
  }
  if(button_pressed(BTN_SEL)&&view==V_REMIX){remix_latch^=1;need_header=need_controls=1;}
  else if(button_pressed(BTN_SEL)&&view==V_GLOBAL&&page==2){midi_toggle();}
  else if(button_pressed(BTN_SEL)&&view==V_CHAIN){
    if(playing||chain_running)message("STOP TO EDIT CHAIN");else{chain_ui.length=(uint8_t)(chain_cursor+1);need_header=need_controls=1;message("CHAIN ENDS HERE");}
  }
  else if(button_pressed(BTN_SEL)&&view==V_PATTERNS){int copy=button_held(BTN_ENV);if(copy)env_consumed=1;select_pattern(copy);}
  else if(button_pressed(BTN_SEL)&&view==V_PRESETS){
    if(preset_type==0&&button_held(BTN_ENV)){env_consumed=1;load_init_voice();}
    else load_preset();
  }
  else if(button_pressed(BTN_SEL)&&fx_surface()){punch_latched=0;punch_request=0x80000000u;message("FX LATCHES CLEARED");}
  else if(button_pressed(BTN_SEL)){sel_pending=1;sel_lane=selected;sel_down_at=millis();}
  if(sel_pending&&(fx_surface()||view==V_PRESETS||view==V_PATTERNS||view==V_CHAIN||view==V_REMIX||(view==V_GLOBAL&&page==2)))sel_pending=0;
  if(sel_pending&&button_held(BTN_SEL)&&(uint32_t)(millis()-sel_down_at)>=400){
    if(toggle_solo(sel_lane))sel_pending=0;
  }
  if(sel_pending&&button_released(BTN_SEL)){
    if((uint32_t)(millis()-sel_down_at)>=400)toggle_solo(sel_lane);
    else {int mute=!ui.track[sel_lane].mute;if(send(C_MUTE,sel_lane,0,(unsigned)mute)){ui.track[sel_lane].mute=(uint8_t)mute;need_header=need_grid=need_controls=1;}}
    sel_pending=0;
  }
  if(button_pressed(BTN_ARP)&&button_held(BTN_ENV)&&view==V_PATTERNS){
    env_consumed=1;
    if(pattern_busy)message("WAIT FOR PATTERN SWITCH");
    else if(chain_running||(chain_ui.mode&&playing))message("STOP CHAIN TO CLEAR");
    else if(pattern_cursor==active_pattern)message("LOAD ANOTHER SLOT TO CLEAR");
    else{gd_pattern_blank(&pattern_bank[pattern_cursor]);refresh_pattern_size(pattern_cursor);need_controls=1;message("PATTERN CLEARED - SAVE");}
  }
  else if(button_pressed(BTN_ARP)&&button_held(BTN_ENV)){
    env_consumed=1;
    if(!mutation_state)message("NO MUTATION TO UNDO");
    else if(send(C_MUTATE_SWAP,0,0,0)){
      pattern_swap(mutation_ui,&ui);mutation_state=mutation_state==1?2:1;
      need_grid=need_controls=1;message(mutation_state==2?"UNDONE / ENV+ARP REDO":"REDONE / ENV+ARP UNDO");
    }else message("BUSY: TRY AGAIN");
  }
  else if(button_pressed(BTN_ARP)&&view==V_PRESETS){compare_preset();}
  else if (button_pressed(BTN_ARP)) {
    unsigned all=button_held(BTN_GLO),scope=all?128u|ui.pages:(selected<<2)|edit_page;
    unsigned random_seed=gd_random(&mutation_seed)&65535u;
    if(send_word((C_MUTATE<<24)|(scope<<16)|random_seed)){
      pattern_snapshot(mutation_ui,&ui);mutation_state=1;
      gd_mutate_pattern(&ui,scope,random_seed);
      need_grid=need_controls=1;
      message(all?"ALL MUTATED / ENV+ARP":"MUTATED / ENV+ARP UNDO");
    }else message("BUSY: TRY AGAIN");
  }
  if(button_pressed(BTN_SAVE)&&view==V_PRESETS){store_preset();}
  else if (button_pressed(BTN_SAVE)) {
    if (button_held(BTN_ENV)) {
      env_consumed=1;
      if (send(C_UNLOCK, selected, step_selected, 0)) {
        ui.track[selected].lock_id[step_selected] = 255;
        message("STEP LOCK CLEARED");
      }
    } else {
      pending_save = 1;
      message(playing ? "SAVE QUEUED: STOP" : "WAITING FOR SILENCE");
    }
  }
  int change = encoder(ENC_SELECT);
  if(change&&view==V_GLOBAL&&page==2){midi_map_cursor=(unsigned)gd_clamp((int)midi_map_cursor+change,0,15);midi_target.kind=M_INVALID;midi_pick=0;need_controls=1;}
  else if(change&&view==V_CHAIN){chain_cursor=(unsigned)gd_clamp((int)chain_cursor+change,0,15);need_controls=1;}
  else if(change&&view==V_PATTERNS){pattern_cursor=(unsigned)gd_clamp((int)pattern_cursor+change,0,15);need_controls=1;}
  else if(change&&fx_surface()){fx_slot=(unsigned)(((int)fx_slot+change%27+27)%27);fx_selected=gd_keyboard_effect(fx_slot);need_header=need_grid=need_controls=1;}
  else if (change) {
    selected = (unsigned)(((int)selected + change % 8 + 8) % 8);
    if(view==V_MIX||view==V_HOME)mix_bank=selected/4;
    if(view==V_VOICE){page=voice_row[selected];if(!lock_edit)voice_focus=voice_style[selected];}
    if(view==V_LFO){lfo_display=lfo_page[selected];page=lfo_display;lfo=page<2?page:0;}
    need_header = need_grid = need_controls = 1;
  }
  change = encoder(ENC_ALGORITHM);
  if(change&&view==V_STEP){step_selected=(unsigned)gd_clamp((int)step_selected+change,0,15);need_header=need_grid=need_controls=1;change=0;}
  if(change&&view==V_REMIX){unsigned mode=(unsigned)(((int)remix_ui.mode+change%11+11)%11);select_remix_mode(mode);change=0;}
  if(change&&view==V_CHAIN){edit_chain(1,change);change=0;}
  if(change&&view==V_PATTERNS){pattern_cursor=(unsigned)gd_clamp((int)pattern_cursor+change,0,15);need_controls=1;change=0;}
  if(change&&view==V_PRESETS){preset_type=(unsigned)(((int)preset_type+change%3+3)%3);preset_slot%=preset_count();need_header=need_controls=1;change=0;}
  if(change&&fx_surface()){fx_slot=(unsigned)(((int)fx_slot+change%27+27)%27);fx_selected=gd_keyboard_effect(fx_slot);need_header=need_grid=need_controls=1;change=0;}
  if(change&&view==V_GLOBAL){page=(unsigned)(((int)page+change%3+3)%3);need_header=need_grid=need_controls=1;change=0;}
  if(change&&(view==V_MIX||view==V_HOME)){mix_bank=(unsigned)(((int)mix_bank+change%2+2)%2);need_header=need_grid=need_controls=1;change=0;}
  if (change) {
    unsigned count = view == V_PUNCH ? 3 : view == V_FX ? 3 : view == V_LFO ? 3 : sound_pages();
    page = (unsigned)(((int)(page % count) + change % (int)count + (int)count) %
                      (int)count);
    if(view==V_PUNCH){fx_bank=page;fx_selected=page==2?16:page==1?27:0;}
    if (view == V_LFO){lfo_display=page;lfo=page<2?page:0;need_grid=1;}
    need_header = need_controls = need_grid = 1;
  }
  change = encoder(ENC_PRESETS);
  if(change){
    if(button_held(BTN_GLO)){
      unsigned next=(unsigned)gd_clamp((int)ui.bpm+change,40,240);
      if(send(C_GLOBAL,0,3,next))ui.bpm=next;
    }else{
      if(view!=V_PRESETS){preset_type=fx_surface()||view==V_FX?2:view==V_MIX||view==V_HOME?1:0;preset_slot=0;view=V_PRESETS;}
      unsigned count=preset_count();preset_slot=(unsigned)(((int)preset_slot+change%(int)count+(int)count)%(int)count);
    }
    need_header=need_controls=need_grid=1;
  }
  unsigned midi_octave=midi_learn_on||(view==V_GLOBAL&&page==2);
  if(midi_octave&&button_pressed(BTN_OCT_UP)){midi_learn_on=midi_pick=0;need_header=1;message("MIDI LEARN DONE");}
  if(midi_octave&&button_pressed(BTN_OCT_DOWN))midi_clear_target();
  int octave_delta = midi_octave?0:button_pressed(BTN_OCT_UP)-button_pressed(BTN_OCT_DOWN);
  if (view != V_STEP && view!=V_PATTERNS && view!=V_CHAIN && view!=V_REMIX && octave_delta) {
    keyboard_octave = gd_clamp(keyboard_octave + octave_delta, -3, 3);
    need_header = 1;
  }
  edit_page=0;

  /* SEQ and its explicit voice lock editor share step keys. */
  if(view!=V_VOICE)lock_edit=0;
  sequence_mode = view == V_STEP || (view==V_VOICE&&lock_edit);
  uint32_t visible_step = playhead % ui.track[selected].length;
  if (key_step != visible_step) {
    key_step = visible_step;
    key_step_at = millis();
  }
  uint32_t punch_mask=0,key_mask=0,roll_keys=0,remix_keys=0,glitch_choice=16,newest_glitch=0;
  for (int k = 0; k < 27; ++k) {
    int w = white_index(k), b = black_index(k);
    if(key_released(k))remix_print_keys&=~(1u<<k);
    if(key_released(k)&&held_note[k]){send(C_RELEASE,held_lane[k],(unsigned)(held_pitch[k]+64),0);held_note[k]=0;}
    if(view==V_VOICE&&lock_edit&&key_released(k))need_controls=1;
    if(view==V_REMIX&&w>=0&&((button_held(BTN_ENV)&&key_pressed(k))||(remix_print_keys&(1u<<k)))){
      if(key_pressed(k)){env_consumed=1;remix_print_keys|=1u<<k;print_remix((unsigned)w);}led_key(k,0);continue;
    }
    if(view==V_REMIX&&b>=0){
      if(key_pressed(k)&&b<11)select_remix_mode((unsigned)b);
      if(b<11&&key_held(k))remix_keys=1;
      led_key(k,remix_active&&b==(int)remix_ui.mode);continue;
    }
    if(view==V_CHAIN){
      if(key_pressed(k)&&w>=0){chain_cursor=(unsigned)w;need_controls=1;}
      led_key(k,w>=0&&(chain_running?w==(int)chain_row:w==(int)chain_cursor));continue;
    }
    if(view==V_PATTERNS){
      if(key_pressed(k)&&w>=0&&w<16){
        unsigned immediate=pattern_last_key==(unsigned)w&&(uint32_t)(millis()-pattern_last_tap)<=300u;
        pattern_last_key=(unsigned)w;pattern_last_tap=millis();pattern_cursor=(unsigned)w;
        select_pattern(0);
        if(immediate&&pattern_busy==1&&queued_pattern==(unsigned)w&&send(C_PATTERN_NOW,0,0,0))message("SWITCH NOW");
        need_controls=1;
      }
      led_key(k,w==(int)active_pattern||(pattern_busy&&w==(int)queued_pattern&&(millis()%500u)<250u));continue;
    }
    if(((view==V_FX&&page%3==2)||view==V_REMIX)&&!midi_learn_on&&w>=0){
      if(key_pressed(k))glitch_key_stamp[w]=++glitch_key_serial;
      if(key_held(k)||key_pressed(k)){roll_keys=1;if(glitch_choice==16||(int32_t)(glitch_key_stamp[w]-newest_glitch)>0){glitch_choice=(unsigned)w;newest_glitch=glitch_key_stamp[w];}}
      if(key_pressed(k)){
        if(!glitch_prepared){if(ui.fx[10])glitch_prepared=1;else if(send(C_FX,0,10,127)){ui.fx[10]=127;glitch_prepared=1;}}
        need_controls=1;
      }
      led_key(k,key_held(k));continue;
    }
    if(fx_surface()){
      unsigned effect=gd_keyboard_effect((unsigned)k);
      if(effect>=30){led_key(k,0);continue;}
      if(key_pressed(k)){
        fx_slot=(unsigned)k;fx_selected=effect;need_controls=need_header=1;
        if(midi_learn_on){midi_target=(GDMidiTarget){M_GATE,(uint8_t)effect};midi_pick=1;notice_until=0;}
        if(punch_latch_mode)punch_latched^=1u<<effect;
      }
      if(key_held(k)||key_pressed(k))key_mask|=1u<<effect;
      if(!punch_latch_mode&&(key_held(k)||key_pressed(k)))punch_mask|=1u<<effect;
      led_key(k,(punch_active>>effect)&1u);
      continue;
    }
    if (key_pressed(k)) {
      if (sequence_mode && !recording) {
        if (b >= 0 && b < 8) {
          selected = (unsigned)b;
          page = voice_row[selected];
          need_header = need_grid = need_controls = 1;
        } else if (w >= 0) {
          unsigned s = edit_page * 16 + (unsigned)w;
          step_selected = s;need_header=1;
          if(button_held(BTN_ENV))env_consumed=1;
          if (view == V_STEP && !button_held(BTN_ENV))
            set_step(s, ui.track[selected].step[s] ? 0 : 100);
          need_grid = need_controls = 1;
        }
      } else {
        int pitch = (selected >= 6 ? k : 0) + keyboard_octave * 12;
        hit(selected, 110, pitch);held_lane[k]=(uint8_t)selected;held_pitch[k]=(int8_t)pitch;held_note[k]=1;
        if (recording && playing) {
          unsigned s = playhead % 16;
          if(selected>=6)record_poly_note(selected,s,110,pitch);
          else{set_step(s,110);if(send(C_PITCH,selected,s,(unsigned)(pitch+64)))ui.track[selected].pitch[s]=(int8_t)pitch;}
        }
      }
    }
    int on =
        sequence_mode
            ? (w >= 0
                   ? (unsigned)w<ui.track[selected].length&&ui.track[selected].step[edit_page * 16 + (unsigned)w] > 0
                   : b == (int)selected)
            : key_held(k);
    if (sequence_mode && w >= 0 && playing &&
        visible_step == edit_page * 16 + (unsigned)w) {
      /* One-bit LEDs: light empty steps and briefly notch lit steps so the
       * cursor remains visible even across a fully programmed pattern. */
      on = !on || (uint32_t)(millis() - key_step_at) >= 35;
    }
    led_key(k, on);
  }
  if(roll_keys&&(glitch_choice!=glitch_key_ui||!roll_ui)){
    unsigned base=gd_glitch_key_rate(glitch_choice),rate=(unsigned)gd_clamp((int)base+glitch_offset,1,127);
    if(send(C_FX,0,9,rate)){glitch_key_ui=glitch_choice;glitch_base=base;ui.fx[9]=(uint8_t)rate;need_controls=need_grid=1;}
  }
  if(view!=V_REMIX||(!playing&&remix_active))remix_latch=0;
  unsigned path_on=view==V_REMIX&&playing&&(remix_latch||remix_keys);
  if(path_on!=remix_active&&send(C_REMIX_GATE,0,0,path_on)){remix_active=path_on;need_header=need_controls=1;}
  if(roll_keys!=roll_ui&&send(C_ROLL_GATE,0,0,roll_keys)){roll_ui=roll_keys;need_grid=1;if(view==V_REMIX)need_controls=1;}
  uint32_t physical_punch=punch_mask|punch_latched;
  punch_mask=physical_punch|midi_gate_mask();
  if(fx_surface()){
    punch_request=punch_mask;
    if(punch_mask!=punch_ui_mask||punch_active!=punch_ui_active||key_mask!=punch_key_mask||punch_latched!=punch_ui_latched)need_header=need_grid=need_controls=1;
    punch_key_mask=key_mask;punch_ui_latched=punch_latched;
    punch_ui_mask=punch_mask;punch_ui_active=punch_active;
  }else{punch_request=punch_latched|midi_gate_mask()|0x40000000u;punch_ui_mask=punch_ui_active=punch_key_mask=0;}
  uint8_t st, note, vel;
  while (midi_read(&st, &note, &vel)) {
    if((st&0xf0)==0xb0){midi_cc(st&15,note,vel);continue;}
    if ((st & 0xf0) == 0x90 && vel) {
      unsigned lane = (st & 15) < 8 ? (st & 7) : selected;
      hit(lane, vel, lane >= 6 ? (int)note - 60 : 0);
      if(recording&&playing&&lane>=6)record_poly_note(lane,playhead%16,vel,(int)note-60);
    }
    if((st&0xf0)==0x80||((st&0xf0)==0x90&&!vel)){unsigned lane=(st&15)<8?(st&7):selected;send(C_RELEASE,lane,(unsigned)gd_clamp((int)note-60+64,0,127),0);}
    if (st == 0xfa)
      set_play(1);
    if (st == 0xfc)
      set_play(0);
  }
  if(!(punch_request&0x80000000u))punch_request=(fx_surface()?physical_punch:punch_latched|0x40000000u)|midi_gate_mask();
  for (unsigned k = 0; k < 4; ++k)
    edit_control(k, encoder(ENC_KNOB1 + (int)k));
  if (last_step != playhead) {
    last_step = playhead;
    if(view==V_REMIX)need_controls=1;
    need_grid = 1;
  }
  if(!fx_surface()&&punch_ui_latched!=punch_latched){punch_ui_latched=punch_latched;need_header=1;}
  display_lane=selected;
  if((view==V_LFO||view==V_MIX||view==V_HOME||(view==V_VOICE&&voice_focus))&&(uint32_t)(millis()-last_visual)>=50){last_visual=millis();need_grid=1;}
  if(midi_ui_dirty&&(uint32_t)(millis()-midi_refresh_at)>=33){
    midi_refresh_at=millis();midi_ui_dirty=0;if(midi_header_dirty){midi_header_dirty=0;need_header=1;}
    if(view==V_HOME||view==V_MIX)need_grid=1;
    else if(!(view==V_GLOBAL&&page==2))need_controls=1;
  }
  if (notice_until && millis() >= notice_until) {
    notice_until = 0;
    need_header = 1;
  }
  /* RemoteFM1 serializes entry points and saves into a small RAM image.
   * Physical NOR writes must still wait until audio is quiet. */
#ifdef GD_REMOTE
  if (pending_save) gd_flush_pending_save();
#else
  if (pending_save && !playing && !audio_running && audio_quiet)
    gd_flush_pending_save();
#endif
  led_button(BTN_PLAY, (int)playing);
  led_button(BTN_REC, (int)recording);
  led_button(BTN_SEL,view==V_REMIX?remix_latch:solo_ui!=0);
  led_button(BTN_SEQ, (int)(sequence_mode||view==V_REMIX));
  finish_chain_switch();finish_pattern_switch();
  if(last_chain_row!=chain_row||last_chain_pass!=chain_pass){last_chain_row=chain_row;last_chain_pass=chain_pass;if(view==V_CHAIN)need_header=need_controls=1;}
  if(fx_surface())fx_selected=gd_keyboard_effect(fx_slot);
  remember_navigation();
  redraw();
}
