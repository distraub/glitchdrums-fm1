#include "controls.h"
#include "engine.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "fx.h"
#include "keys.h"
#include "presets.h"
#include <stddef.h>
#include "punch.h"
static uint64_t render(GDEngine *d, const GDProject *p, unsigned frames) {
  uint64_t hash = 1469598103934665603ull;
  for (unsigned i = 0; i < frames; ++i) {
    int32_t l, r;
    gd_render_sample(d, p, &l, &r);
    assert(l >= -30000 && l <= 30000 && r >= -30000 && r <= 30000);
    hash = (hash ^ (uint32_t)l) * 1099511628211ull;
    hash = (hash ^ (uint32_t)r) * 1099511628211ull;
  }
  return hash;
}
static uint64_t voice_hash(unsigned lane, unsigned control, unsigned value) {
  GDProject p;
  gd_defaults(&p);
  GDEngine d = {0};
  if (lane == 6) {
    p.track[lane].param[15] = 100;
    p.track[lane].param[28] = 64;
    p.track[lane].param[8] = 35;
    p.track[lane].param[7] = 20;
    p.track[lane].param[27] = 20;
  }
  if(lane==5&&control==10)p.track[5].param[9]=64;
  if(lane==7&&(control==25||control==26))p.track[7].param[27]=1;
  p.track[lane].param[control] = (uint8_t)value;
  gd_hit(&d, &p, lane, 127, lane==7&&control==17?12:0, -1, 0);
  if(lane==7&&control==26)gd_release(&d,7,0);
  return render(&d, &p, 8192);
}
static uint64_t signature_parameter_hash(unsigned effect,unsigned parameter,unsigned value){
 GDSignature signature={0};GDProject p;gd_defaults(&p);
 p.effect[27+effect][parameter]=(uint8_t)value;uint64_t hash=1469598103934665603ull;
 for(unsigned i=0;i<20000;++i){
  int32_t l=(gd_sine(i*13000000u)+gd_sine(i*43000000u))/3,r=l;
  gd_signature_controls(&signature,1u<<(27+effect),i?0:1u<<(27+effect),&p.effect[27],&l,&r);
  hash=(hash^(uint32_t)l)*1099511628211ull;
 }
 return hash;
}
int main(void) {
  for(unsigned bpm=40;bpm<=240;++bpm)for(unsigned div=0;div<6;++div){
    static const unsigned mult[6]={1,2,3,4,6,8};unsigned folds;
    unsigned frames=gd_delay_frames(bpm,div,&folds);
    assert(frames>0&&frames<=32767);
    assert(frames==((44100u*60u/(bpm*4u)*mult[div])>>folds));
  }
  assert(gd_master_fx_index(1,0)==7&&gd_master_fx_index(1,1)==8);
  GDProject fresh;gd_defaults(&fresh);assert(fresh.fx[0]==0);

  /* App INIT units must reach DSP, not just UI labels. */
  GDProject init_project;gd_defaults(&init_project);GDEngine init_engine={0};
  const uint8_t *init=init_project.track[0].param;
  assert(gd_kick_units[0][init[0]]==90);
  assert(gd_kick_units[1][init[1]]==22050);
  assert(gd_kick_units[5][init[5]]==8820);
  assert(gd_kick_units[6][init[6]]==4*32768);
  assert(gd_kick_units[7][init[7]]==2205);
  assert(gd_kick_units[8][init[8]]==0);
  assert(gd_kick_units[10][init[10]]==370);
  gd_hit(&init_engine,&init_project,0,127,0,-1,0);
  assert(init_engine.voices[0].attack==2690);
  assert(init_engine.voices[0].hold==0);
  assert(init_engine.voices[0].decay==gd_kick_decay_coeff[init[1]]);
  init_engine.voices[0].motion=0;gd_prepare(&init_engine,&init_project);
  assert(init_engine.voices[0].inc[0]==gd_kick_root_inc[init[0]]);
  assert(init_engine.voices[0].gain==gd_kick_level[init[29]]);

  /* F3..G5: verify every key against its musical pitch class. */
  unsigned whites = 0, blacks = 0;
  for (int key = 0; key < 27; ++key) {
    int pitch = (53 + key) % 12;
    int black = pitch == 1 || pitch == 3 || pitch == 6 || pitch == 8 || pitch == 10;
    if (black) {
      assert(black_index(key) == (int)blacks++);
      assert(white_index(key) == -1);
    } else {
      assert(white_index(key) == (int)whites++);
      assert(black_index(key) == -1);
    }
  }
  assert(whites == 16 && blacks == 11);
  assert(black_index(5) == 2); /* third black key selects closed hat */

  for(unsigned bank=0;bank<2;++bank){
    assert(gd_punch_key(bank,-1,0)==27);
    assert(gd_punch_key(bank,-1,1)==28);
    assert(gd_punch_key(bank,-1,2)==29);
    assert(gd_punch_key(bank,-1,3)==19); /* fold stays put */
    assert(gd_punch_key(bank,0,-1)==0); /* loops stay put */
  }
  for(int w=0;w<3;++w)assert(gd_punch_key(2,w,-1)==16u+(unsigned)w);
  assert(gd_punch_key(2,3,-1)==32&&gd_punch_key(2,-1,0)==32);
  _Static_assert(sizeof(GDSave)<=SETTINGS_MAX,"Preset library overflows flash");
  GDPresetBank bank={0};assert(gd_bank_valid(&bank));
  for(unsigned lane=0;lane<8;++lane)for(unsigned preset=0;preset<8;++preset){
    GDSound sound=gd_factory_sound(lane,preset);assert(gd_sound_valid(&sound,lane));
    GDProject voice;gd_defaults(&voice);gd_put_sound(&voice,lane,&sound);GDEngine synth={0};gd_hit(&synth,&voice,lane,110,0,-1,0);
    int audible=0;
    for(unsigned frame=0;frame<22050;++frame){int32_t l,r;gd_render_sample(&synth,&voice,&l,&r);assert(l>=-30000&&l<=30000&&r>=-30000&&r<=30000);audible|=l!=0||r!=0;}
    assert(audible);
    bank.voice[lane][preset]=sound;bank.voice_used[lane]|=(uint8_t)(1u<<preset);
  }
  for(unsigned preset=0;preset<4;++preset){
    GDProject before,after;gd_defaults(&before);after=before;
    bank.kit[preset]=gd_factory_kit(preset);bank.scene[preset]=gd_factory_scene(preset);
    bank.kit_used|=(uint8_t)(1u<<preset);bank.scene_used|=(uint8_t)(1u<<preset);
    gd_apply_preset(&after,&bank.kit[preset],1,0);
    for(unsigned lane=0;lane<8;++lane)
      assert(!memcmp(before.track[lane].step,after.track[lane].step,offsetof(GDTrack,mute)-offsetof(GDTrack,step)));
    assert(before.bpm==after.bpm&&before.swing==after.swing&&before.pages==after.pages&&before.theme==after.theme);
  }
  assert(gd_bank_valid(&bank));bank.voice[6][0].param[0]=255;assert(!gd_bank_valid(&bank));
  printf("Preset storage: %zu / %u bytes\n",sizeof(GDSave),SETTINGS_MAX);
  GDPunchGate gate={0};
  gd_gate_input(&gate,1,1);gd_gate_input(&gate,0,1);
  gd_gate_boundary(&gate);assert(gate.active==1);
  gd_gate_boundary(&gate);assert(gate.active==0);
  gd_gate_input(&gate,2,0);gd_gate_stop(&gate);
  gd_gate_input(&gate,2,0);assert(!gate.active);
  gd_gate_input(&gate,0,0);gd_gate_input(&gate,2,0);assert(gate.active==2);
  gd_gate_input(&gate,0x80000000u,1);assert(!gate.active&&!gate.on);
  GDProject punch_project;gd_defaults(&punch_project);
  for(unsigned effect=0;effect<31;++effect){
    GDPunch punch={0};uint64_t changed=0;
    for(unsigned sample=0;sample<50000;++sample){
      int32_t dry=gd_sine(sample*17000000u)/2,l=dry,r=dry;
      uint32_t mask=sample<10000||sample>=40000?0:effect==30?0x3fffffffu:1u<<effect;
      gd_delay[sample&32767]=(int16_t)dry;
      gd_punch(&punch,&punch_project,mask,sample,&l,&r);
      assert(l>=-30000&&l<=30000&&r>=-30000&&r<=30000);
      if(mask&&(l!=dry||r!=dry))++changed;
    }
    if(changed<=100)fprintf(stderr,"unchanged punch effect %u\n",effect);
    assert(changed>100);assert(punch.mix==0&&punch.previous==0);
  }
  /* Four independent envelope/filter voices per pitched lane, with bounded
   * quietest stealing, drum choke intact and no cross-lane stealing. */
  GDProject polyProject;gd_defaults(&polyProject);polyProject.reserved=0; /* request four Glitch slots */
  GDEngine poly={0};
  for(unsigned lane=6;lane<8;++lane){
    for(unsigned n=0;n<4;++n){
      gd_hit(&poly,&polyProject,lane,100,(int)n*3,-1,0);
      render(&poly,&polyProject,64);
    }
    unsigned active=0;
    for(unsigned slot=0;slot<14;++slot){unsigned l=slot<8?slot:6+(slot-8)/3;if(l==lane&&poly.voices[slot].env>1)++active;}
    assert(active==4);
    gd_hit(&poly,&polyProject,lane,100,19,-1,0);
    active=0;unsigned replacement=0;
    for(unsigned slot=0;slot<14;++slot){unsigned l=slot<8?slot:6+(slot-8)/3;if(l==lane&&poly.voices[slot].env>1){++active;replacement+=poly.voices[slot].pitch_offset==19;}}
    assert(active==4&&replacement==1);
  }
  GDEngine simultaneous={0};
  for(unsigned note=0;note<6;++note)gd_hit(&simultaneous,&polyProject,6,100,(int)note,-1,0);
  unsigned survivors=0;for(unsigned slot=0;slot<14;++slot)if(simultaneous.voices[slot].env>1)survivors|=1u<<simultaneous.voices[slot].pitch_offset;
  assert(survivors==0x3c); /* newest four notes survive a six-note block */
  render(&poly,&polyProject,44100);
  gd_hit(&poly,&polyProject,GD_OH,100,0,-1,0);
  gd_hit(&poly,&polyProject,GD_CH,100,0,-1,0);
  assert(!poly.voices[GD_OH].env);
  for(unsigned effect=0;effect<3;++effect)for(unsigned parameter=0;parameter<4;++parameter)
    assert(signature_parameter_hash(effect,parameter,0)!=signature_parameter_hash(effect,parameter,127));
  GDProject soloProject;gd_defaults(&soloProject);soloProject.track[6].mute=1;
  GDEngine solo={0},soloReference={0};solo.solo_mask=soloReference.solo_mask=1u<<6;
  gd_hit(&solo,&soloProject,6,100,0,-1,0);gd_hit(&solo,&soloProject,7,100,7,-1,0);
  gd_hit(&soloReference,&soloProject,6,100,0,-1,0);
  assert(render(&solo,&soloProject,4096)==render(&soloReference,&soloProject,4096));
  assert(solo.voices[7].age==4096&&soloProject.track[6].mute==1);
  solo.solo_mask=1u<<GD_OH;gd_hit(&solo,&soloProject,GD_OH,100,0,-1,0);gd_hit(&solo,&soloProject,GD_CH,100,0,-1,0);
  assert(solo.voices[GD_OH].env>1);solo.solo_mask=0;gd_hit(&solo,&soloProject,GD_CH,100,0,-1,0);assert(!solo.voices[GD_OH].env);
  /* Fold must preserve silence and never increase an input sample's magnitude.
   * Cover quiet/hot inputs and character extremes, including negative peaks. */
  for(unsigned character=0;character<3;++character){
    GDProject p;gd_defaults(&p);p.effect[19][0]=(uint8_t)(character==2?127:character*64);
    for(unsigned amplitude=0;amplitude<3;++amplitude){
      GDPunch fold={0};
      for(unsigned i=0;i<8192;++i){
        int32_t dry=gd_sine(i*19000000u)*(amplitude==0?0:amplitude==1?1000:29000)/32768;
        int32_t l=dry,r=-dry;
        gd_punch(&fold,&p,1u<<19,i,&l,&r);
        int absDry=dry<0?-dry:dry;
        assert((l<0?-l:l)<=absDry*3/2+1 && (r<0?-r:r)<=absDry*3/2+1);
        if(!dry)assert(!l&&!r);
      }
    }
  }
  /* Each signature effect has a fresh epoch: reopening on silence cannot
   * replay old grains or reflection tails, including across counter wrap. */
  uint64_t signature_hash[3][2];
  for(unsigned effect=0;effect<3;++effect){
    for(unsigned extreme=0;extreme<2;++extreme){
      GDSignature signature={0};uint64_t hash=1469598103934665603ull;
      signature.write=0xfffff000u;
      for(unsigned i=0;i<24000;++i){
        int32_t l=gd_sine(i*11000000u)/2,r=l;
        gd_signature(&signature,1u<<(27+effect),i?0:1u<<(27+effect),extreme?127:0,&l,&r);
        assert(l>=-30000&&l<=30000&&r>=-30000&&r<=30000);
        hash=(hash^(uint32_t)l)*1099511628211ull;
        hash=(hash^(uint32_t)r)*1099511628211ull;
      }
      signature_hash[effect][extreme]=hash;
      for(unsigned i=0;i<12000;++i){
        int32_t l=0,r=0;
        gd_signature(&signature,1u<<(27+effect),i?0:1u<<(27+effect),extreme?127:0,&l,&r);
        assert(l==0&&r==0);
      }
    }
    assert(signature_hash[effect][0]!=signature_hash[effect][1]);
  }
  assert(signature_hash[0][1]!=signature_hash[1][1]);
  assert(signature_hash[1][1]!=signature_hash[2][1]);
  GDProject p;
  gd_defaults(&p);
  GDEngine a = {0}, b = {0};
  int32_t l, r;
  for (int i = 0; i < 128; ++i) {
    gd_render_sample(&a, &p, &l, &r);
    assert(!l && !r);
  }
  for (unsigned lane = 0; lane < 8; ++lane) {
    a = (GDEngine){0};
    gd_hit(&a, &p, lane, 127, 0, -1, 0);
    b = a;
    uint64_t x = render(&a, &p, 8192), y = render(&b, &p, 8192);
    assert(x == y);
    assert(a.voices[lane].env > 0);
    render(&a, &p, 44100 * 3);
    assert(a.voices[lane].env < 32768);
  }
  /* Every newly exposed kick/snare parameter must affect rendered audio. */
  for(unsigned lane=0;lane<2;++lane)for(unsigned param=0;param<16;++param){
    unsigned max=lane?gd_snarectrl[param].max:gd_kickctrl[param].max;
    uint64_t lo=voice_hash(lane,param,0),hi=voice_hash(lane,param,max);
    if(lo==hi)fprintf(stderr,"inactive drum control: lane %u param %u\n",lane,param);
    assert(lo!=hi);
  }
  /* Every Perc control can be locked at trigger without changing base sound. */
  {GDProject p;gd_defaults(&p);GDEngine d={0};
   for(unsigned parameter=0;parameter<20;++parameter){
    unsigned original=p.track[5].param[parameter],locked=original<64?100:20;
    gd_hit(&d,&p,5,110,0,(int)parameter,locked);
    assert(d.acoustic[5].q[parameter]==locked);
    assert(p.track[5].param[parameter]==original);render(&d,&p,1024);
   }
  }
  /* A retrigger must start fresh filter/shape state, like the app voice. */
  {GDProject kick;gd_defaults(&kick);GDEngine fresh={0},retrigger={0};
   gd_hit(&fresh,&kick,0,110,0,-1,0);gd_hit(&retrigger,&kick,0,110,0,-1,0);
   render(&retrigger,&kick,2048);gd_hit(&retrigger,&kick,0,110,0,-1,0);
   assert(render(&fresh,&kick,2048)==render(&retrigger,&kick,2048));}
  /* Exercise all Glitch/Analog parameters in a context where they act. */
  for (unsigned lane = 6; lane < 8; ++lane)
    for (unsigned param = 0; param < 29; ++param) {
      unsigned max = lane == 6 ? gd_gctrl[param].max : gd_actrl[param].max;
      if(lane==6&&(param==5||param==25))max=64; /* 0 and 1 wrap to the same tape position */
      uint64_t x = voice_hash(lane, param, 0), y = voice_hash(lane, param, max);
      if (x == y)
        fprintf(stderr, "no audible difference: lane %u param %u\n", lane,
                param);
      assert(x != y);
    }
  /* Every acoustic control except interactive hat choke changes isolated PCM. */
  for(unsigned lane=1;lane<6;++lane)for(unsigned param=0;param<(lane==5?20u:16u);++param){
    unsigned max=lane==5?127:gd_ac_controls[lane-1][param].max;if(!max||((lane==2||lane==3)&&param==9))continue;
    uint64_t lo=voice_hash(lane,param,0),hi=voice_hash(lane,param,max);
    if(lo==hi)fprintf(stderr,"inactive acoustic control: %u/%u\n",lane,param);assert(lo!=hi);
  }
  {GDProject gate;gd_defaults(&gate);gate.track[7].param[27]=1;gate.track[7].param[31]=80;GDEngine held={0};
   gd_hit(&held,&gate,7,110,5,-1,0);render(&held,&gate,44100*3);assert(held.voices[7].env>1);
   gd_release(&held,7,5);render(&held,&gate,44100);assert(held.voices[7].env==0);}
  {GDProject hats;gd_defaults(&hats);hats.track[3].param[9]=127;GDEngine choke={0};
   gd_hit(&choke,&hats,3,110,0,-1,0);render(&choke,&hats,128);gd_hit(&choke,&hats,2,110,0,-1,0);
   assert(choke.voices[3].env>0);render(&choke,&hats,10000);assert(!choke.voices[3].env);}
  /* Maximum legal controls and LFO depths must remain bounded under UBSan. */
  uint32_t rng = 1234;
  for (unsigned trial = 0; trial < 32; ++trial) {
    gd_defaults(&p);
    a = (GDEngine){0};
    for (unsigned lane = 0; lane < 8; ++lane) {
      for (unsigned j = 0; j < 32; ++j) {
        unsigned max = lane == 6   ? gd_gctrl[j].max
                       : lane == 7 ? gd_actrl[j].max
                                   : 127;
        if (lane == 7 && !max)
          max = 127;
        p.track[lane].param[j] = (uint8_t)(gd_random(&rng) % (max + 1));
      }
      for (unsigned k = 0; k < 2; ++k)
        p.track[lane].lfo[k] =
            (GDLFO){(uint8_t)(trial % 5), (uint8_t)(trial % 12), 127,
                    (uint8_t)(1 + trial % 8)};
      gd_hit(&a, &p, lane, 127, 24, -1, 0);
    }
    render(&a, &p, 2048);
  }
  for (unsigned trial = 0; trial < 24; ++trial) {
    gd_defaults(&p);
    a = (GDEngine){0};
    GDFX fx = {0};
    p.bpm = 40 + trial * 8;
    for (unsigned k = 0; k < 12; ++k)
      p.fx[k] = (uint8_t)(gd_random(&rng) % (gd_fxctrl[k].max + 1));
    for (unsigned lane = 0; lane < 8; ++lane)
      gd_hit(&a, &p, lane, 127, 0, -1, 0);
    for (unsigned i = 0; i < 8192; ++i) {
      gd_render_sample(&a, &p, &l, &r);
      gd_fx(&fx, &p, &l, &r);
      assert(l >= -30000 && l <= 30000 && r >= -30000 && r <= 30000);
    }
    uint32_t cursor = fx.write;
    p.fx[2] /= 2;
    gd_fx(&fx, &p, &l, &r);
    assert(fx.write == cursor + 1);
  }
  puts("PASS: 8 lanes, four-voice Glitch/Analog polyphony, finite tails, every kick/snare/Glitch/Analog control, max-depth "
       "LFOs, FX bounds, 30 punch effects, signature freshness/character and uninterrupted delay state");
}
