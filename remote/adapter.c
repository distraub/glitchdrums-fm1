/* SPDX-License-Identifier: GPL-3.0-only
 * GlitchDrums bridge: foma's platform API -> RemoteFM1 fm1core ABI 6.
 * The included web HAL supplies drawing/input helpers; we drive time and audio
 * from the host ABI instead of its browser ring. */
#include "../platform/web.c"
#include "fm1core.h"
#include <stddef.h>

#if !defined(GD_FIRMWARE_VERSION) || !defined(GD_FIRMWARE_NAME)
#error "Build through tools/build_remote.py to supply firmware identity"
#endif

#ifndef GD_SOURCE_URL
#define GD_SOURCE_URL "local-development-source"
#endif
static fm1panel_t panel;
/* Flash image has a length header: the host persists flash_size bytes, while
 * foma settings_load requires the exact application record length. */
static uint8_t flash_image[SETTINGS_MAX + 8];
static const int8_t keymap[6][11] = {
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {5, 11, 4, 10, 3, 9, 2, 8, -1, -1, -1},
    {34, 35, 36, 37, 38, 40, 39, 13, 7, 6, 12},
    {23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33},
    {0, 1, 15, 14, 17, 16, 19, 18, 20, 21, 22},
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}};
static const uint8_t btn_ids[14] = {2, 3,  4,  5,  6,  7, 8,
                                    9, 10, 11, 12, 13, 0, 1};
static const uint8_t enc_ids[7] = {0, 1, 6, 2, 3, 4, 5};

static void input(void) {
  web_keys(panel.keys);
  web_buttons(panel.buttons);
  taps_k |= panel.keys_tap;
  taps_b |= panel.buttons_tap;
  panel.keys_tap = panel.buttons_tap = 0;
  for (unsigned i = 0; i < 7; ++i) {
    web_encoder((int)i, panel.enc[i]);
    panel.enc[i] = 0;
  }
  web_master(panel.master < 0 ? 0 : (uint32_t)panel.master);
  input_snapshot();
}
static void output(void) {
  for (unsigned c = 0; c < 11; ++c)
    for (unsigned l = 0; l < 4; ++l) {
      int id = keymap[l + 1][c];
      int lit = id < 0    ? 0
                : id < 14 ? (int)(led_b >> id & 1u)
                          : (int)(led_k >> (id - 14) & 1u);
      panel.led[c][l] = lit ? 255 : 0;
    }
  panel.led_play_green = (led_b >> BTN_PLAY & 1u) ? 255 : 0;
  if (panel.lcd_writes != fb_gen) {
    memcpy(panel.lcd, fb, sizeof fb);
    panel.lcd_writes = fb_gen;
  }
}
EXPORT(rfm1_version) int rfm1_version(void) { return 1; }
EXPORT(rfm1_options)
void rfm1_options(const char *path, int no_flash, int save, int headless) {
  (void)path;
  (void)no_flash;
  (void)save;
  (void)headless;
}
EXPORT(rfm1_boot_options)
int rfm1_boot_options(int fail, const char *reason, int stage) {
  (void)fail;
  (void)reason;
  (void)stage;
  return 1;
}
EXPORT(rfm1_init) void rfm1_init(int demo) {
  (void)demo;
  memcpy(panel.btn_id, btn_ids, sizeof btn_ids);
  memcpy(panel.enc_id, enc_ids, sizeof enc_ids);
  for (unsigned i = 0; i < 7; ++i)
    panel.enc_dir[i] = 1;
  if (!panel.master)
    panel.master = 700;
  web_master((uint32_t)panel.master);
  web_boot();
  output();
  panel.ready = 1;
}
EXPORT(rfm1_tick) void rfm1_tick(uint32_t ms) {
  now_ms = ms;
  now_us = ms * 1000u;
  input();
  loop();
  screen_flush();
  output();
}
EXPORT(rfm1_frame) void rfm1_frame(void) { output(); }
EXPORT(rfm1_idle) void rfm1_idle(void) { output(); }
EXPORT(rfm1_audio) void rfm1_audio(int16_t *out, uint32_t frames) {
  int32_t block[256];
  while (frames) {
    uint32_t n = frames > 128 ? 128 : frames;
    memset(block, 0, n * 2u * sizeof(int32_t));
    audio(block, n);
    uint32_t gain = (web_master_v * web_master_v) >> 8;
    for (uint32_t i = 0; i < n * 2u; i+=2) {
      gd_output_frame(&gd_output_state,&block[i],&block[i+1],gain);
      out[i]=(int16_t)block[i];out[i+1]=(int16_t)block[i+1];
    }
    out += n * 2u;
    frames -= n;
  }
}
EXPORT(rfm1_midi_in) int rfm1_midi_in(uint32_t pkt) {
  if (mi_w - mi_r >= MQ)
    return 0;
  web_midi_in((pkt >> 8) & 255u, (pkt >> 16) & 127u, (pkt >> 24) & 127u);
  return 1;
}
EXPORT(rfm1_midi_out_take) int rfm1_midi_out_take(uint32_t *pkt) {
  int32_t m = web_midi_out();
  if (m < 0)
    return 0;
  uint32_t status = (uint32_t)m & 255u;
  *pkt = (status >> 4) | ((uint32_t)m << 8);
  return 1;
}
EXPORT(rfm1_halted) int rfm1_halted(void) { return 0; }
extern int gd_flush_pending_save(void);
EXPORT(rfm1_shutdown) void rfm1_shutdown(void) { gd_flush_pending_save(); }
EXPORT(rfm1_flash_stage)
int rfm1_flash_stage(const uint8_t *bytes, uint32_t n) {
  if (n > sizeof flash_image)
    return 0;
  memset(store, 255, sizeof store);
  store_len = -1;
  if (bytes && n >= 8) {
    uint32_t magic, length;
    memcpy(&magic, bytes, 4);
    memcpy(&length, bytes + 4, 4);
    if (magic == 0x47444653u && length <= sizeof store && length <= n - 8) {
      memcpy(store, bytes + 8, length);
      store_len = (int32_t)length;
    }
  }
  store_gen = 0;
  return 1;
}
EXPORT(rfm1_flash) uint8_t *rfm1_flash(void) {
  uint32_t magic = 0x47444653u,
           length = store_len < 0 ? 0 : (uint32_t)store_len;
  memcpy(flash_image, &magic, 4);
  memcpy(flash_image + 4, &length, 4);
  memcpy(flash_image + 8, store, sizeof store);
  return flash_image;
}
/* ABI 2 promises a live image, not a snapshot made only on flash(). */
void gd_remote_store_changed(void) { (void)rfm1_flash(); }
EXPORT(rfm1_flash_dirty) uint32_t rfm1_flash_dirty(void) { return store_gen; }
EXPORT(rfm1_flash_sync) int rfm1_flash_sync(void) { return gd_flush_pending_save(); }
EXPORT(rfm1_stats) void rfm1_stats(uint32_t *shed, uint32_t *cpu) {
  if (shed)
    *shed = 0;
  if (cpu)
    *cpu = 0;
}
EXPORT(rfm1_ui_info) void rfm1_ui_info(char *buf, uint32_t n) {
  if (n)
    *buf = 0;
}

/* Host buffers: fixed memory, first-fit allocator with adjacent free merging.
 */
static union {
  uint64_t align;
  uint8_t data[512 * 1024];
} arena;
typedef struct {
  uint32_t size, used;
} Header;
static int arena_ready;
EXPORT(rfm1_alloc) void *rfm1_alloc(uint32_t n) {
  if (!n || n > sizeof arena.data - sizeof(Header))
    return 0;
  if (!arena_ready) {
    ((Header *)arena.data)->size = sizeof arena.data - sizeof(Header);
    arena_ready = 1;
  }
  n = (n + 7u) & ~7u;
  for (uint32_t at = 0; at < sizeof arena.data;) {
    Header *h = (Header *)(arena.data + at);
    if (!h->used && h->size >= n) {
      if (h->size >= n + sizeof(Header) + 8) {
        Header *next = (Header *)(arena.data + at + sizeof(Header) + n);
        next->size = h->size - n - sizeof(Header);
        next->used = 0;
        h->size = n;
      }
      h->used = 1;
      return h + 1;
    }
    at += sizeof(Header) + h->size;
  }
  return 0;
}
EXPORT(rfm1_free) void rfm1_free(void *p) {
  if (!p)
    return;
  ((Header *)p)[-1].used = 0;
  for (uint32_t at = 0; at < sizeof arena.data;) {
    Header *h = (Header *)(arena.data + at);
    uint32_t next_at = at + sizeof(Header) + h->size;
    if (next_at >= sizeof arena.data)
      break;
    Header *next = (Header *)(arena.data + next_at);
    if (!h->used && !next->used)
      h->size += sizeof(Header) + next->size;
    else
      at = next_at;
  }
}

static const fm1core_t core = {
    .abi_version = 6,
    .id = "glitchdrums",
    .name = GD_FIRMWARE_NAME,
    .version = GD_FIRMWARE_VERSION,
    .source_url = GD_SOURCE_URL,
    .family = "foma",
    .license = "GPL-3.0-only",
    .flash_size = SETTINGS_MAX + 8,
    .button_names = {"FX", "SEL", "ENV", "LFO", "EDIT", "GLO", "HOME", "SAVE",
                     "ARP", "SEQ", "PLAY", "REC", "OCT-", "OCT+"},
    .panel = &panel,
    .keymap = &keymap[0][0],
    .options = rfm1_options,
    .boot_options = rfm1_boot_options,
    .init = rfm1_init,
    .tick = rfm1_tick,
    .frame = rfm1_frame,
    .idle = rfm1_idle,
    .audio = rfm1_audio,
    .midi_in = rfm1_midi_in,
    .midi_out_take = rfm1_midi_out_take,
    .stats = rfm1_stats,
    .ui_info = rfm1_ui_info,
    .shutdown = rfm1_shutdown,
    .halted = rfm1_halted,
    .flash_stage = rfm1_flash_stage,
    .flash = rfm1_flash,
    .flash_dirty = rfm1_flash_dirty,
    .flash_sync = rfm1_flash_sync,
    .credits =
        "GlitchDrums FM1 development prototype.\nPlatform: foma / Dinghy / "
        "Felucca / SLOOP.\nABI: Quixotic7 FM1VST.\nGPL-3.0-only; complete "
        "notices accompany the source archive."};
EXPORT(rfm1_get) const fm1core_t *rfm1_get(uint32_t abi) {
  return abi == 6 ? &core : 0;
}
#define OFF(T, field) ((int32_t)offsetof(T, field))
EXPORT(rfm1_layout) int32_t rfm1_layout(uint32_t i) {
  static const int32_t table[] = {sizeof(fm1panel_t),
                                  OFF(fm1panel_t, keys),
                                  OFF(fm1panel_t, buttons),
                                  OFF(fm1panel_t, keys_tap),
                                  OFF(fm1panel_t, buttons_tap),
                                  OFF(fm1panel_t, enc),
                                  OFF(fm1panel_t, master),
                                  OFF(fm1panel_t, key_vel),
                                  OFF(fm1panel_t, key_lean),
                                  OFF(fm1panel_t, led),
                                  OFF(fm1panel_t, led_play_green),
                                  OFF(fm1panel_t, lcd),
                                  OFF(fm1panel_t, lcd_writes),
                                  OFF(fm1panel_t, btn_id),
                                  OFF(fm1panel_t, enc_id),
                                  OFF(fm1panel_t, enc_dir),
                                  OFF(fm1panel_t, ready),
                                  sizeof(fm1core_t),
                                  OFF(fm1core_t, abi_version),
                                  OFF(fm1core_t, id),
                                  OFF(fm1core_t, name),
                                  OFF(fm1core_t, version),
                                  OFF(fm1core_t, source_url),
                                  OFF(fm1core_t, family),
                                  OFF(fm1core_t, license),
                                  OFF(fm1core_t, gated),
                                  OFF(fm1core_t, flash_size),
                                  OFF(fm1core_t, button_names),
                                  OFF(fm1core_t, params),
                                  OFF(fm1core_t, nparams),
                                  OFF(fm1core_t, panel),
                                  OFF(fm1core_t, keymap),
                                  OFF(fm1core_t, options),
                                  OFF(fm1core_t, credits)};
  return i < sizeof table / sizeof table[0] ? table[i] : -1;
}
