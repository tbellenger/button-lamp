// Music mode for SpiralLamp_R4_FireTrail_WiFi_MIDI.
// AppleMIDI (rtpMIDI) on WiFiS3 UDP. Control port 5004, data port 5005.
// The UNO R4 core has no mDNS, so the Mac adds the lamp by IP, or this
// sketch calls sendInvite() when secrets.h sets MAC_SESSION_IP.
//
// Included once from the sketch, before lamp_wifi.h.

#ifndef MUSIC_MODE_H
#define MUSIC_MODE_H

#define APPLEMIDI_INITIATOR
#include <WiFiS3.h>
#include <AppleMIDI.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#ifndef MAC_SESSION_IP
#define MAC_SESSION_IP "0.0.0.0"
#endif

// One peer. A full slot makes sendInvite() return false, so a second call
// does not reset an invite that is still in flight (ssrc stays 0 until the
// Mac accepts). After the library gives up it drops the slot and a later
// sendInvite() starts clean.
struct LampMidiSettings : appleMidi::DefaultSettings {
  static const uint8_t MaxNumberOfParticipants = 1;
};

typedef appleMidi::AppleMIDISession<WiFiUDP, LampMidiSettings> LampAppleSession;
LampAppleSession AppleMIDI("Spiral Lamp", 5004);
MIDI_NAMESPACE::MidiInterface<LampAppleSession, appleMidi::AppleMIDISettings>
    MIDI((LampAppleSession&)AppleMIDI);

namespace Music {

static const uint8_t PULSE_N = 32;
static const uint32_t NOTE_FADE_MS = 400;
static const uint32_t DRUM_MS = 140;
static const uint32_t IDLE_GAP_MS = 10000;
static const uint32_t STALE_MS = 500;
static const uint32_t FRAME_MS = 16;
static const uint32_t INVITE_GAP_MS = 16000;
static const float TRAVERSE_S = 1.6f;

static const uint8_t HUE[12][3] = {
  {255,   0,   0}, // C
  {255,  80,   0}, // C#
  {255, 180,   0}, // D
  {160, 255,   0}, // D#
  {  0, 255,   0}, // E
  {  0, 255, 120}, // F
  {  0, 200, 255}, // F#
  {  0,  60, 255}, // G
  {120,   0, 255}, // G#
  {200,   0, 255}, // A
  {255,   0, 160}, // A#
  {255,   0,  60}, // B
};
static const char* const PC_NAME[12] = {
  "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

struct Pulse {
  uint8_t used;
  uint8_t ch;
  uint8_t note;
  uint8_t vel;
  uint8_t held;
  float pos;
  float fade;
  uint32_t touch;
};

static Pulse g_pulses[PULSE_N];
static uint8_t g_begun = 0;
static uint8_t g_linked = 0;
static uint8_t g_drawing = 0;
static uint8_t g_auto = 0;
static uint8_t g_cc1 = 64;
static uint8_t g_cc7 = 127;
static uint8_t g_drumPc = 0;
static float g_drum = 0.f;
static int16_t g_lastNote = -1;
static uint32_t g_lastNoteMs = 0;
static uint32_t g_advMs = 0;
static uint32_t g_drawMs = 0;
static char g_session[25];
static char g_noteName[8];
static uint8_t g_inviteState = 0; // 0 unknown, 1 skip, 2 armed
static uint8_t g_inviteWait = 0;
static uint32_t g_inviteMs = 0;
static IPAddress g_inviteIp;

static void copyName(const char* s) {
  uint8_t n = 0;
  if (s) {
    for (; *s && n < 24; s++) {
      char c = *s;
      bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                (c >= '0' && c <= '9') || c == ' ' || c == '.' || c == '_' || c == '-';
      if (ok) g_session[n++] = c;
    }
  }
  g_session[n] = 0;
}

static void rememberNote(byte note) {
  g_lastNote = (int16_t)note;
  int oct = (int)note / 12 - 1;
  snprintf(g_noteName, sizeof g_noteName, "%s%d", PC_NAME[note % 12], oct);
}

static bool musicLive() {
  uint8_t m = webOwnsMode ? webMode : g_reportedMode;
  return m == WM_MUSIC;
}
static bool musicSoon() {
  return musicLive() || (!webOwnsMode && g_reportedMode == WM_OFF);
}

static int tailLeds(uint8_t vel) {
  int tail = (int)(((uint32_t)LED_COUNT * vel) / 1024u);
  if (tail < 4) tail = 4;
  int cap = (int)LED_COUNT / 2;
  if (cap < 4) cap = 4;
  if (tail > cap) tail = cap;
  return tail;
}
static uint8_t headBright(uint8_t vel) {
  if (vel > 127) vel = 127;
  return (uint8_t)(40u + (uint16_t)vel * 215u / 127u);
}

static int slotFor(uint8_t ch, uint8_t note) {
  int empty = -1, rel = -1, held = -1;
  float worstFade = 2.f;
  float far = -1.f;
  for (int i = 0; i < PULSE_N; i++) {
    Pulse& p = g_pulses[i];
    if (p.used && p.ch == ch && p.note == note) return i;
    if (!p.used) {
      if (empty < 0) empty = i;
      continue;
    }
    if (!p.held) {
      if (p.fade < worstFade) { worstFade = p.fade; rel = i; }
    } else if (p.pos > far) {
      far = p.pos;
      held = i;
    }
  }
  if (empty >= 0) return empty;
  if (rel >= 0) return rel;
  return held;
}

static void spawn(uint8_t ch, byte note, byte vel) {
  int i = slotFor(ch, note);
  if (i < 0) return;
  Pulse& p = g_pulses[i];
  p.used = 1;
  p.ch = ch;
  p.note = note;
  p.vel = vel;
  p.held = 1;
  p.pos = 0.f;
  p.fade = 1.f;
  p.touch = millis();
}

static void release(uint8_t ch, byte note) {
  uint32_t t = millis();
  for (uint8_t i = 0; i < PULSE_N; i++) {
    Pulse& p = g_pulses[i];
    if (p.used && p.ch == ch && p.note == note) {
      p.held = 0;
      p.touch = t;
    }
  }
}

static void armAuto() { g_auto = 1; }

static void onNoteOn(MIDI_NAMESPACE::Channel ch, byte note, byte vel) {
  if (vel == 0) {
    g_lastNoteMs = millis();
    release((uint8_t)ch, note);
    armAuto();
    return;
  }
  rememberNote(note);
  g_lastNoteMs = millis();
  armAuto();
  if ((uint8_t)ch == 10) {
    float peak = (vel > 127 ? 127 : vel) / 127.f;
    if (peak > g_drum) {
      g_drum = peak;
      g_drumPc = (uint8_t)(note % 12);
    }
    return;
  }
  if (musicSoon()) spawn((uint8_t)ch, note, vel);
}

static void onNoteOff(MIDI_NAMESPACE::Channel ch, byte note, byte) {
  g_lastNoteMs = millis();
  release((uint8_t)ch, note);
  armAuto();
}

static void onCC(MIDI_NAMESPACE::Channel, byte cc, byte val) {
  if (val > 127) val = 127;
  if (cc == 1) g_cc1 = val;
  else if (cc == 7) g_cc7 = val;
  else return;
  armAuto();
}

static void onConnected(const appleMidi::ssrc_t&, const char* name) {
  g_linked = 1;
  g_inviteWait = 1;
  copyName(name);
  Serial.print(F("AppleMIDI connected "));
  Serial.println(g_session[0] ? g_session : "(unnamed)");
}

static void onDisconnected(const appleMidi::ssrc_t&) {
  if (g_linked) Serial.println(F("AppleMIDI disconnected"));
  g_linked = 0;
  g_session[0] = 0;
  g_inviteWait = 0;
}

static void prepareInvite() {
  g_inviteState = 1;
  const char* s = MAC_SESSION_IP;
  if (!s || !s[0]) return;
  uint8_t b[4];
  int part = 0;
  int acc = -1;
  for (const char* p = s;; p++) {
    char c = *p;
    if (c >= '0' && c <= '9') {
      if (acc < 0) acc = 0;
      acc = acc * 10 + (c - '0');
      if (acc > 255) return;
    } else if (c == '.' || c == 0) {
      if (acc < 0 || part >= 4) return;
      b[part++] = (uint8_t)acc;
      acc = -1;
      if (c == 0) break;
    } else {
      return;
    }
  }
  if (part != 4) return;
  if ((b[0] | b[1] | b[2] | b[3]) == 0) return;
  g_inviteIp = IPAddress(b[0], b[1], b[2], b[3]);
  g_inviteState = 2;
}

static void maybeInvite(uint32_t now) {
  if (!g_begun || g_linked) return;
  if (g_inviteState == 0) prepareInvite();
  if (g_inviteState != 2) return;
  if (g_inviteWait && (uint32_t)(now - g_inviteMs) < INVITE_GAP_MS) return;
  if (AppleMIDI.sendInvite(g_inviteIp, 5004)) {
    g_inviteWait = 1;
    g_inviteMs = now;
    Serial.print(F("AppleMIDI invite "));
    Serial.println(F(MAC_SESSION_IP));
  }
}

static void takeAuto() {
  if (!g_auto) return;
  g_auto = 0;
  if (webOwnsMode) return;
  if (g_reportedMode != WM_OFF) return;
  webModeBeforeLava = WM_MUSIC;
  webMode = WM_MUSIC;
  webOwnsMode = 1;
#if DEBUG_SERIAL
  Serial.println(F("MIDI auto Music"));
#endif
}

static float ledsPerSec() {
  float knob = mapNormToScale(g_lastKnobNorm);
  float cc;
  if (g_cc1 <= 64) cc = 0.5f + (g_cc1 / 64.f) * 0.5f;
  else cc = 1.f + ((g_cc1 - 64) / 63.f);
  return ((float)LED_COUNT / TRAVERSE_S) * knob * cc;
}

static void advance(uint32_t dt) {
  float step = ledsPerSec() * ((float)dt / 1000.f);
  float top = (LED_COUNT > 0) ? (float)(LED_COUNT - 1) : 0.f;
  for (uint8_t i = 0; i < PULSE_N; i++) {
    Pulse& p = g_pulses[i];
    if (!p.used) continue;
    p.pos += step;
    if (p.held) {
      if (p.pos > top) p.pos = top;
      p.fade = 1.f;
    } else {
      p.fade -= (float)dt / (float)NOTE_FADE_MS;
      if (p.fade <= 0.f) p.used = 0;
    }
  }
  if (g_drum > 0.f) {
    g_drum -= (float)dt / (float)DRUM_MS;
    if (g_drum < 0.f) g_drum = 0.f;
  }
}

static void addGlow(uint16_t& r, uint16_t& g, uint16_t& b,
                    float pos, float tail, float fade, uint8_t head, uint8_t pc, int led) {
  if (pc > 11) pc = 0;
  float d = pos - (float)led;
  if (d < -2.5f || d > tail || tail <= 0.f) return;
  float w = (d < 0.f) ? (1.f + d / 2.5f) : (1.f - d / tail);
  if (w <= 0.f) return;
  w *= fade * ((float)head / 255.f);
  if (w > 1.f) w = 1.f;
  uint16_t sc = (uint16_t)(w * 256.f);
  r = (uint16_t)(r + ((HUE[pc][0] * sc) >> 8));
  g = (uint16_t)(g + ((HUE[pc][1] * sc) >> 8));
  b = (uint16_t)(b + ((HUE[pc][2] * sc) >> 8));
}

static bool anyPulse() {
  for (uint8_t i = 0; i < PULSE_N; i++) if (g_pulses[i].used) return true;
  return false;
}

static void draw(uint32_t now) {
  bool idle = !anyPulse() && g_drum < 0.02f &&
              (g_lastNoteMs == 0 || (uint32_t)(now - g_lastNoteMs) >= IDLE_GAP_MS);
  float idlePos = 0.f;
  int idleTail = 6;
  uint8_t idlePc = 0;
  if (idle) {
    const float period = 7000.f;
    float span = (LED_COUNT > 1) ? (float)(LED_COUNT - 1) : 0.f;
    idlePos = fmodf((float)(now % 7000u), period) / period * span;
    idleTail = (int)LED_COUNT / 10;
    if (idleTail < 6) idleTail = 6;
    idlePc = (uint8_t)((now / 800u) % 12u);
  }
  uint8_t gain = g_cc7;
  for (int i = 0; i < LED_COUNT; i++) {
    uint16_t r = 0, g = 0, b = 0;
    for (uint8_t n = 0; n < PULSE_N; n++) {
      Pulse& p = g_pulses[n];
      if (!p.used) continue;
      addGlow(r, g, b, p.pos, (float)tailLeds(p.vel), p.fade, headBright(p.vel),
              (uint8_t)(p.note % 12), i);
    }
    if (idle) addGlow(r, g, b, idlePos, (float)idleTail, 1.f, 48, idlePc, i);
    if (g_drum > 0.f) {
      uint16_t sc = (uint16_t)(g_drum * 256.f);
      r = (uint16_t)(r + ((HUE[g_drumPc][0] * sc) >> 8));
      g = (uint16_t)(g + ((HUE[g_drumPc][1] * sc) >> 8));
      b = (uint16_t)(b + ((HUE[g_drumPc][2] * sc) >> 8));
    }
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;
    r = (uint16_t)((r * gain) / 127u);
    g = (uint16_t)((g * gain) / 127u);
    b = (uint16_t)((b * gain) / 127u);
    setPixelCalibratedRemember((uint16_t)i, (uint8_t)r, (uint8_t)g, (uint8_t)b);
  }
  strip.show();
}

static void expireStale(uint32_t now) {
  for (uint8_t i = 0; i < PULSE_N; i++) {
    Pulse& p = g_pulses[i];
    if (p.used && (uint32_t)(now - p.touch) > STALE_MS) p.used = 0;
  }
}

void poll(uint32_t now) {
  if (!g_begun) return;
  MIDI.read();
  maybeInvite(now);
  takeAuto();
}

void render(uint32_t now) {
  bool entered = !g_drawing;
  g_drawing = 1;
  if (entered) expireStale(now);
  uint32_t dt = now - g_advMs;
  g_advMs = now;
  if (dt > 40) dt = 40;
  advance(dt);
  if (!entered && (uint32_t)(now - g_drawMs) < FRAME_MS) return;
  g_drawMs = now;
  draw(now);
  for (uint8_t i = 0; i < PULSE_N; i++) if (g_pulses[i].used) g_pulses[i].touch = now;
}

void noteLeft() { g_drawing = 0; }

void onWifiUp() {
  if (g_begun) return;
  AppleMIDI.setHandleConnected(onConnected);
  AppleMIDI.setHandleDisconnected(onDisconnected);
  MIDI.setHandleNoteOn(onNoteOn);
  MIDI.setHandleNoteOff(onNoteOff);
  MIDI.setHandleControlChange(onCC);
  MIDI.begin(MIDI_CHANNEL_OMNI);
  g_begun = 1;
  Serial.println(F("AppleMIDI UDP 5004 (data 5005), session \"Spiral Lamp\"."));
  Serial.println(F("No mDNS. Audio MIDI Setup: Network session, port 5004."));
  if (g_inviteState == 0) prepareInvite();
  if (g_inviteState == 2)
    Serial.println(F("Will invite MAC_SESSION_IP from secrets.h."));
  else
    Serial.println(F("MAC_SESSION_IP is 0.0.0.0. Add the lamp by IP, port 5004."));
}

void onWifiDown() {
  if (g_begun) {
    AppleMIDI.sendEndSession();
    AppleMIDI.end();
  }
  g_begun = 0;
  g_linked = 0;
  g_session[0] = 0;
  g_inviteWait = 0;
}

const char* linkState() {
  if (!g_begun) return "off";
  if (g_linked) return "connected";
  return "waiting";
}
const char* sessionName() { return g_session; }
int lastNote() { return g_lastNote; }
const char* lastNoteName() { return (g_lastNote >= 0) ? g_noteName : ""; }

} // namespace Music

#endif
