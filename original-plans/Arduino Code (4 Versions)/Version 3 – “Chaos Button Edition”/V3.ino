/* ========= Spiral Lamp — Main + Dots + Chaos =========
   Dots appear over: Jumbo (incl. Classic), Secret, and black/off.
   Dots are disabled over Wave and Rainbow — except Chaos Q2 allows dots.

   White (PIN_RIGHT=4): spawn dot (when allowed), Wave pause (white), Scrub w/ both held in Rainbow
   Red   (PIN_LEFT =8): delete oldest; hold ≥500ms deletes all (when allowed). In Wave: hold+white toggles reverse.

   Chaos mode (PIN_CHAOS=11): ON partitions the strip into 4 vertical quarters:
     Q0 bottom: Wave + Toggle Modifier (uses Wave buttons: white pause, red-hold + white toggles reverse)
     Q1 mid:    Rainbow (moves; buttons adjust HOLD/FADE/STAG like normal rainbow; respects toggle: reverse & 2×)
     Q2 mid:    Jumbo + Toggle Modifier (Dots overlay ENABLED in this quarter only)
     Q3 top:    Epic Rainbow
*/

#include <Adafruit_NeoPixel.h>
#include <math.h>
#include <string.h>
#include <avr/pgmspace.h>

// ================== Hardware ==================
#define PIN_LED         9
#define LED_COUNT       60
Adafruit_NeoPixel strip(LED_COUNT, PIN_LED, NEO_GRB + NEO_KHZ800);

// Shared inputs (from full sketch)
#define PIN_ROCKER      7     // Active LOW (jumbo rocker main power)
#define PIN_TOGGLE      6     // Active LOW (jumbo modifier toggle)
#define PIN_LEFT        8     // Active LOW (arcade red   -)  -> also Dots RED
#define PIN_RIGHT       4     // Active LOW (arcade white +)  -> also Dots WHITE (spawn)
#define PIN_KEY         5     // Active LOW (Rainbow key)
#define PIN_FADER       A0
#define PIN_SPEED_KNOB  A1
#define PIN_WAVE       10
#define PIN_CHAOS      11

// ================== Small helpers ==================
static inline float clamp01(float x){ return x<0?0:(x>1?1:x); }
static inline float ease01(float u){ u=clamp01(u); return 0.5f*(1.0f - cosf(3.14159265f*u)); }
static inline uint8_t mix8(uint8_t a,uint8_t b,float t){ t=clamp01(t); return (uint8_t)((1.f-t)*a+t*b+0.5f); }
static inline int32_t wrapLoop(int32_t v, int32_t mod){ v%=mod; if(v<0) v+=mod; return v; }
static inline uint16_t clampU16(uint16_t v,uint16_t lo,uint16_t hi){ if(v<lo) return lo; if(v>hi) return hi; return v; }
static inline uint8_t clamp8i(int v){ return (v<0)?0:((v>255)?255:(uint8_t)v); }
static inline uint16_t STAG_BOTTOM_MS(uint16_t i){ return (uint16_t)(i * 17U); }
static inline uint16_t centerDist(uint16_t i){ int dA=abs((int)i-29), dB=abs((int)i-30); return (uint16_t)((dA<dB)?dA:dB); }
static inline uint8_t  STAG_CENTER8_MS (uint16_t i){ return (uint8_t )(centerDist(i) *  8U); }
static inline uint16_t STAG_CENTER20_MS(uint16_t i){ return (uint16_t)(centerDist(i) * 20U); }

// ================== Debouncer ==================
struct DebouncedInput{
  uint8_t pin; bool activeLow; uint8_t stable,lastStable; uint32_t lastChangeMs;
  void begin(uint8_t p,bool isActiveLow){ pin=p; activeLow=isActiveLow; pinMode(pin,INPUT_PULLUP); stable=lastStable=digitalRead(pin); lastChangeMs=millis(); }
  // 0 none, 1 rose, 2 fell
  uint8_t update(uint32_t now,uint16_t dbMs=15){
    uint8_t raw=digitalRead(pin);
    if(raw!=stable && (now-lastChangeMs)>=dbMs){
      lastStable=stable; stable=raw; lastChangeMs=now;
      if(lastStable==LOW && stable==HIGH) return 1; // rose (OFF for active-low switch)
      if(lastStable==HIGH && stable==LOW)  return 2; // fell (ON for active-low)
    }
    return 0;
  }
  bool pressed() const { return activeLow ? (stable==LOW) : (stable==HIGH); }
};

// ================== Global fader / speed ==================
float  g_lastKnobNorm = 0.5f;
float  g_speedScale   = 1.0f; // 0.25x..8x
float  g_fader        = 1.0f; // 0..1
const float SPEED_EMA = 0.45f, SPEED_DB=0.006f, FADER_EMA=0.35f;

static inline float mapNormToScale(float n){
  return (n<=0.5f)?(0.25f+(n/0.5f)*(1.0f-0.25f)):(1.0f+((n-0.5f)/0.5f)*(8.0f-1.0f));
}
static inline void updateSpeed(){
  float n=analogRead(PIN_SPEED_KNOB)/1023.0f;
  if (fabsf(n-g_lastKnobNorm)<SPEED_DB) n=g_lastKnobNorm;
  g_lastKnobNorm=(1.0f-SPEED_EMA)*g_lastKnobNorm+SPEED_EMA*n;
  g_speedScale=mapNormToScale(g_lastKnobNorm);
}
static inline void updateFader(){
  float n=analogRead(PIN_FADER)/1023.0f;
  g_fader=(1.0f-FADER_EMA)*g_fader+FADER_EMA*n;
  g_fader=clamp01(g_fader);
}

// ================== Color calibration ==================
float CAL_R_GAIN = 1.06f;
float CAL_G_GAIN = 0.94f;
float CAL_B_GAIN = 1.00f;

// ================== ---------- JUMBO ROCKER MODE ---------- ==================
const uint8_t  GLOBAL_BRIGHT     = 255;
const uint16_t JUMBO_FADE_MS     = 30;
const uint32_t SEQ_TOTAL_MS      = (uint32_t)(LED_COUNT-1)*17 + JUMBO_FADE_MS;

uint8_t  g_bright[LED_COUNT]; uint32_t g_lastMsBright=0; float g_progressMs=0.f; bool g_wantOn=false;

uint8_t g_lastJumboR=255, g_lastJumboG=255, g_lastJumboB=255;

static inline void renderJumboBright(float progress){
  if(progress<0) progress=0;
  if(progress>SEQ_TOTAL_MS) progress=SEQ_TOTAL_MS;
  for(int i=0;i<LED_COUNT;i++){
    float local=progress - STAG_BOTTOM_MS(i);
    float e = (local<=0)?0.f : (local>=JUMBO_FADE_MS?1.f : (local/(float)JUMBO_FADE_MS));
    g_bright[i]=(uint8_t)(e*255.f+0.5f);
  }
}

// --- Jumbo color engine (modifier toggle cycling) ---
struct ColorSeq{
  const uint8_t W[3]={255,255,255}, R[3]={255,0,0}, P[3]={170,0,255}, B[3]={0,0,255},
                G[3]={0,255,0}, Y[3]={255,112,0}, O[3]={255,50,0};
  bool startedOnce=false, enabled=false, inOnPhase=false;
  uint32_t lastMs=0; float tOn_ms=0.f, tHold_ms=0.f;
  uint8_t curR=255,curG=255,curB=255;

  void hardOff(uint32_t now){ enabled=false; inOnPhase=false; startedOnce=false; tOn_ms=0; tHold_ms=0; lastMs=now; curR=W[0]; curG=W[1]; curB=W[2]; }
  void onActiveRising(uint32_t now){ enabled=true; lastMs=now; if(!startedOnce){ inOnPhase=true; tOn_ms=0; } }
  void onActiveFalling(uint32_t now){ enabled=false; lastMs=now; }
  void forceStartHoldAtRed(uint32_t now){ enabled=true; startedOnce=true; inOnPhase=false; tHold_ms=0; lastMs=now; curR=R[0];curG=R[1];curB=R[2]; }

  void update(uint32_t now,float speed){
    uint32_t dt_ms=now-lastMs; lastMs=now; float dt=dt_ms*speed;
    if(!enabled) return;
    if(inOnPhase){
      const float FADE=2300.f;
      tOn_ms+=dt; if(tOn_ms>FADE) tOn_ms=FADE;
      float u=ease01(tOn_ms/FADE);
      curR=mix8(W[0],R[0],u); curG=mix8(W[1],R[1],u); curB=mix8(W[2],R[2],u);
      if(tOn_ms>=FADE){ inOnPhase=false; startedOnce=true; tHold_ms=0; }
      return;
    }
    const uint8_t* RING[6]={R,P,B,G,Y,O};
    const float HOLD=85.f, FADE=2300.f, TOTAL=HOLD+FADE, LOOP=TOTAL*6.f;
    tHold_ms+=dt; if(tHold_ms>=LOOP) tHold_ms=fmodf(tHold_ms,LOOP);
    uint32_t k=(uint32_t)(tHold_ms/TOTAL); float tIn=fmodf(tHold_ms,TOTAL);
    const uint8_t* A=RING[k]; const uint8_t* Bn=RING[(k+1)%6];
    if(tIn<HOLD){ curR=A[0];curG=A[1];curB=A[2]; }
    else{ float u=ease01((tIn-HOLD)/FADE); curR=mix8(A[0],Bn[0],u); curG=mix8(A[1],Bn[1],u); curB=mix8(A[2],Bn[2],u); }
  }
  void get(uint8_t& r,uint8_t& g,uint8_t& b) const{ r=curR; g=curG; b=curB; }
} colorSeq;

// Classic Lamp (secret)
enum Mode:uint8_t{ MODE_JUMBO=0, MODE_CLASSIC=1, MODE_SECRET=2 };
Mode g_mode=MODE_JUMBO;
char seqBuf[9]={0}; const char* TARGET="rlrrlrll"; bool g_seqMatched=false;
static inline void pushSeqChar(char c){ memmove(seqBuf,seqBuf+1,7); seqBuf[7]=c; seqBuf[8]='\0'; g_seqMatched=(memcmp(seqBuf,TARGET,8)==0); if(g_seqMatched) memset(seqBuf,0,sizeof(seqBuf)); }
static inline void classicColorFromKnob(float u,uint8_t& r,uint8_t& g,uint8_t& b){
  const uint8_t O_R=255,O_G=50,O_B=0, Y_R=255,Y_G=112,Y_B=0, W_R=255,W_G=255,W_B=255;
  u=clamp01(u);
  if(u<=0.5f){ float t=u/0.5f; r=mix8(O_R,Y_R,t); g=mix8(O_G,Y_G,t); b=mix8(O_B,Y_B,t); }
  else{ float t=(u-0.5f)/0.5f; r=mix8(Y_R,W_R,t); g=mix8(Y_G,W_G,t); b=mix8(Y_B,W_B,t); }
}
struct BridgeSeq{ bool active=false; uint32_t lastMs=0; float t_ms=0.f; uint8_t r0=255,g0=255,b0=255;
  void start(uint8_t r,uint8_t g,uint8_t b,uint32_t now){ r0=r;g0=g;b0=b; t_ms=0; lastMs=now; active=true; }
  bool update(uint32_t now,float speed,uint8_t& r,uint8_t& g,uint8_t& b){
    if(!active) return false; t_ms+=(now-lastMs)*speed; lastMs=now; const float FADE=2300.f;
    if(t_ms>=FADE){ r=255; g=0; b=0; active=false; return false; }
    float u=ease01(t_ms/FADE); r=mix8(r0,255,u); g=mix8(g0,0,u); b=mix8(b0,0,u); return true;
  }
} bridge;
static inline void enterClassic(uint32_t now){ g_mode=MODE_CLASSIC; bridge.active=false; colorSeq.hardOff(now); }
static inline void exitClassicToJumboAtRed(uint32_t now){ g_mode=MODE_JUMBO; colorSeq.forceStartHoldAtRed(now); }

// ================== ---------- RAINBOW (KEY) MODE ---------- ==================
const uint16_t DEFAULT_STAG_MS=177, DEFAULT_HOLD_MS=60, DEFAULT_FADE_MS=200;
uint16_t STAG_MS=DEFAULT_STAG_MS, HOLD_MS=DEFAULT_HOLD_MS, FADE_MS=DEFAULT_FADE_MS;
static inline uint16_t blockMs(){ return (uint16_t)(HOLD_MS+FADE_MS); }
static inline uint32_t loopMs(){ return (uint32_t)7*blockMs(); }
static inline void enforceMinBlock(){ uint16_t blk=blockMs(); if(blk<10){ FADE_MS=(uint16_t)(FADE_MS+(10-blk)); } }
static inline uint16_t addClamped(uint16_t v,uint16_t add,uint16_t vmax){ uint32_t t=(uint32_t)v+add; if(t>vmax) t=vmax; return (uint16_t)t; }

enum BrState:uint8_t{ BR_OFF=0, BR_ON_ANIM, BR_HOLD, BR_OFF_ANIM };
BrState brState=BR_OFF;
uint32_t brLastMs=0, brT_ms=0;
uint32_t phaseMs=0, lastMs=0;

// Buttons / interaction for Rainbow path
const uint16_t LONG_HOLD_MS=500, MAX_TIME_MS=5000;
uint32_t rightPressStart=0, leftPressStart=0;
bool rightLong=false, leftLong=false;
bool inScrub=false; float scrubPrev=0.5f;
const float SCRUB_DELTA_DB=0.003f, SCRUB_GAIN=1.875f;
const uint16_t STAG_MIN_MS = 20; const uint16_t STAG_MAX_MS = 1000;

namespace KR{
  struct RGB{ uint8_t r,g,b; };
  static const RGB RED={255,0,0}, ORANGE={255,50,0}, YELLOW={255,112,0},
                   GREEN={0,255,0}, BLUE={0,0,255}, INDIGO={75,0,130}, VIOLET={148,0,211};
  static const RGB RING[7]={RED,ORANGE,YELLOW,GREEN,BLUE,INDIGO,VIOLET};
  static inline RGB mix(const RGB&a,const RGB&b,float t){ RGB o; o.r=mix8(a.r,b.r,t); o.g=mix8(a.g,b.g,t); o.b=mix8(a.b,b.b,t); return o; }
  static inline RGB colorAt(uint32_t t_ms,uint16_t HOLD,uint16_t FADE){
    uint16_t BLOCK=(uint16_t)(HOLD+FADE); uint32_t LOOP=(uint32_t)7*BLOCK;
    uint32_t t= t_ms%LOOP; uint32_t blk=t/BLOCK; uint32_t ti=t%BLOCK;
    const RGB&A=RING[blk]; const RGB&B=RING[(blk+1)%7];
    if(ti<HOLD) return A; float u=ease01((float)(ti-HOLD)/(float)FADE); return mix(A,B,u);
  }
}
static inline float fadeStep(uint32_t t, uint16_t stag, uint16_t fadeMs, bool invert){
  int32_t local=(int32_t)t-(int32_t)stag;
  if(invert){ if(local<=0) return 1.f; if(local>=fadeMs) return 0.f; return 1.f - (float)local/(float)fadeMs; }
  else{       if(local<=0) return 0.f; if(local>=fadeMs) return 1.f; return (float)local/(float)fadeMs; }
}
static inline float ledBrightAtRainbow(uint8_t i){
  switch(brState){
    case BR_HOLD: return 1.f;
    case BR_OFF:  return 0.f;
    case BR_ON_ANIM:  return fadeStep(brT_ms, STAG_CENTER20_MS(i), 30, false);
    case BR_OFF_ANIM: return fadeStep(brT_ms, STAG_CENTER20_MS(i), 30, true );
  }
  return 1.f;
}

// ========================= NEW WAVE (integrated) =========================
namespace NewWave {
  static inline void colorDefault(uint32_t, uint8_t&, uint8_t&, uint8_t&);
  static inline void colorModifier(uint32_t, uint8_t, uint8_t&, uint8_t&, uint8_t&);
  static inline void colorRainbow(uint32_t, uint8_t, uint8_t&, uint8_t&, uint8_t&);
  static inline void colorEpic(uint32_t, int64_t, uint8_t&, uint8_t&, uint8_t&);
  static inline uint8_t brightnessAt(uint32_t);
  static inline uint8_t brightnessEpic(uint32_t);

  const uint32_t FADE_US        = 650000UL;
  const uint32_t PAUSE_US       = 860000UL;
  const uint32_t ZERO_PAUSE_US  =  20000UL;
  const uint32_t STAGGER_US     =  30000UL;
  const uint32_t CYCLE_US = FADE_US + PAUSE_US + FADE_US + ZERO_PAUSE_US;

  const uint32_t RAIN_HOLD_US = 1210000UL;
  const uint32_t RAIN_FADE_US =   90000UL;
  const uint32_t RAIN_CYCLE_US = RAIN_HOLD_US + RAIN_FADE_US + RAIN_HOLD_US + RAIN_FADE_US;

  const uint32_t EPIC_FLASH_US =  90000UL;
  const uint32_t EPIC_FADE1_US =  40000UL;
  const uint32_t EPIC_HOLD_US  =2550000UL;
  const uint32_t EPIC_STEPC_US =  40000UL;
  const uint32_t EPIC_CYCLE_US = EPIC_FLASH_US + EPIC_FADE1_US + EPIC_HOLD_US + EPIC_STEPC_US;

  const uint32_t EPIC_BR_FULL_A = 100000UL;
  const uint32_t EPIC_BR_FADE_D =2150000UL;
  const uint32_t EPIC_BR_ZERO_H = 430000UL;
  const uint32_t EPIC_BR_FADE_U =  40000UL;

  const uint8_t BLUE_R=0,  BLUE_G=0,   BLUE_B=255;
  const uint8_t GREEN_R=0, GREEN_G=255, GREEN_B=0;
  const uint8_t YELL_R=255, YELL_G=130, YELL_B=0;
  const uint8_t ORNG_R=255, ORNG_G=50,  ORNG_B=0;
  const uint8_t RED_R=255,  RED_G=0,    RED_B=0;
  const uint8_t PURP_R=170, PURP_G=0,   PURP_B=255;

  static uint32_t lastRealUs = 0;
  static double   vNowUs     = 0.0;
  static double   vColorUs   = 0.0;

  static bool     running     = false;
  static bool     pendingStop = false;
  static bool     paused      = false;
  static bool     latchedReverse = false;

  static uint8_t haveLitBits[(LED_COUNT+7)/8];
  static uint8_t lockedBits [(LED_COUNT+7)/8];
  #define NW_BIT_SET(arr,i)   ( (arr)[(i)>>3] |=  (uint8_t)(1u<<((i)&7)) )
  #define NW_BIT_CLR(arr,i)   ( (arr)[(i)>>3] &= (uint8_t)~(1u<<((i)&7)) )
  #define NW_BIT_TST(arr,i)   ( ((arr)[(i)>>3] & (uint8_t)(1u<<((i)&7))) != 0 )

  static bool     finishOffThenRestart = false;
  static bool     restartWantModifier  = false;
  static bool     restartWantKey       = false;

  enum ModeSel : uint8_t { M_DEFAULT=0, M_MOD=1, M_RAINBOW=2, M_EPIC=3 };
  static ModeSel  curMode[LED_COUNT];

  static inline uint8_t ease_to_byte(float u){
    if(u<0)u=0; if(u>1)u=1;
    float e = u*u*(3.f - 2.f*u);
    int v = (int)(e*255.f + 0.5f);
    if(v<0)v=0; if(v>255)v=255; return (uint8_t)v;
  }

  const float KNOB_EMA  = 0.45f;
  const float KNOB_DB   = 0.006f;
  const float SPEED_FAST_OTHER = 1.0f/8.0f;
  const float SPEED_FAST_EPIC  = 1.0f/16.0f;
  const float SPEED_SLOW_MAX   = 6.0f;
  const float KNOB_CENTER = 0.30f;
  static float lastNorm = 0.30f;

  static inline float readKnobNorm(){
    int raw = analogRead(PIN_SPEED_KNOB);
    float norm = raw / 1023.0f;
    if (fabsf(norm - lastNorm) < KNOB_DB) norm = lastNorm;
    norm = (1.0f - KNOB_EMA) * lastNorm + KNOB_EMA * norm;
    lastNorm = norm; return norm;
  }
  static inline float mapNormToScale(float norm, bool epic){
    if (norm < KNOB_CENTER){
      float spanL = (KNOB_CENTER > 0.0f) ? KNOB_CENTER : 1.0f;
      float t = (KNOB_CENTER - norm) / spanL;
      return 1.0f + t * (SPEED_SLOW_MAX - 1.0f);
    } else if (norm > KNOB_CENTER){
      float spanR = (1.0f - KNOB_CENTER > 0.0f) ? (1.0f - KNOB_CENTER) : 1.0f;
      float fastMin = epic ? SPEED_FAST_EPIC : SPEED_FAST_OTHER;
      float t = (norm - KNOB_CENTER) / spanR;
      return 1.0f + t * (fastMin - 1.0f);
    } else return 1.0f;
  }

  static inline uint8_t brightnessAt(uint32_t t){
    if (t < FADE_US){ float u=(float)t/(float)FADE_US; return ease_to_byte(u); }
    t -= FADE_US;
    if (t < PAUSE_US) return 255; t -= PAUSE_US;
    if (t < FADE_US){ float u=(float)t/(float)FADE_US; return ease_to_byte(1.0f - u); }
    t -= FADE_US;
    if (t < ZERO_PAUSE_US) return 0;
    return 0;
  }
  static inline bool inZeroWindow(uint32_t localUs){
    uint32_t t=localUs;
    if (t < FADE_US) return false; t -= FADE_US;
    if (t < PAUSE_US) return false; t -= PAUSE_US;
    if (t < FADE_US) return false; t -= FADE_US;
    return (t < ZERO_PAUSE_US);
  }
  static inline bool inZeroWindowEpic(uint32_t localUs){
    uint32_t t=localUs;
    if (t < EPIC_BR_FULL_A) return false; t -= EPIC_BR_FULL_A;
    if (t < EPIC_BR_FADE_D) return false; t -= EPIC_BR_FADE_D;
    return (t < EPIC_BR_ZERO_H);
  }

  static inline void colorDefault(uint32_t, uint8_t &r, uint8_t &g, uint8_t &b){
    if (g_mode==MODE_CLASSIC){
      uint8_t rr,gg,bb;
      if(bridge.active){ uint8_t rb,gb,bb2; uint32_t hold=bridge.lastMs; bridge.update(bridge.lastMs,0.f,rb,gb,bb2); bridge.lastMs=hold; rr=rb; gg=gb; bb=bb2; }
      else             { classicColorFromKnob(g_lastKnobNorm, rr,gg,bb); }
      r=rr; g=gg; b=bb;
    }else{
      r = g_lastJumboR; g = g_lastJumboG; b = g_lastJumboB;
    }
  }
  static inline void mixColors(uint8_t aR,uint8_t aG,uint8_t aB,
                               uint8_t bR,uint8_t bG,uint8_t bB,
                               uint8_t w, uint8_t &r,uint8_t &g,uint8_t &b){
    r = (uint8_t)(((uint16_t)(255-w)*aR + (uint16_t)w*bR)/255);
    g = (uint8_t)(((uint16_t)(255-w)*aG + (uint16_t)w*bG)/255);
    b = (uint8_t)(((uint16_t)(255-w)*aB + (uint16_t)w*bB)/255);
  }
  static inline void selectModifierPair(uint8_t step, uint8_t &aR,uint8_t &aG,uint8_t &aB, uint8_t &bR,uint8_t &bG,uint8_t &bB){
    switch (step % 6){
      case 0: aR=BLUE_R;  aG=BLUE_G;  aB=BLUE_B;  bR=GREEN_R; bG=GREEN_G; bB=GREEN_B; break;
      case 1: aR=GREEN_R; aG=GREEN_G; aB=GREEN_B; bR=YELL_R;  bG=YELL_G;  bB=YELL_B;  break;
      case 2: aR=YELL_R;  aG=YELL_G;  aB=YELL_B;  bR=ORNG_R;  bG=ORNG_G;  bB=ORNG_B;  break;
      case 3: aR=ORNG_R;  aG=ORNG_G;  aB=ORNG_B;  bR=RED_R;   bG=RED_G;   bB=RED_B;   break;
      case 4: aR=RED_R;   aG=RED_G;   aB=RED_B;   bR=PURP_R;  bG=PURP_G;  bB=PURP_B;  break;
      default: aR=PURP_R; aG=PURP_G;  aB=PURP_B;  bR=BLUE_R;  bG=BLUE_G;  bB=BLUE_B;  break;
    }
  }
  static inline void colorModifier(uint32_t tLocalUs, uint8_t step, uint8_t &r, uint8_t &g, uint8_t &b){
    uint8_t aR,aG,aB,bR,bG,bB; selectModifierPair(step, aR,aG,aB, bR,bG,bB);
    uint32_t t=tLocalUs;
    const uint32_t COL_HOLD_A_US  = 500000UL;
    const uint32_t COL_XFADE_US   =1180000UL;
    const uint32_t COL_HOLD_B_US  = 500000UL;
    if (t < COL_HOLD_A_US){ r=aR; g=aG; b=aB; return; }
    t -= COL_HOLD_A_US;
    if (t < COL_XFADE_US){ uint8_t w=ease_to_byte((float)t/(float)COL_XFADE_US);
      mixColors(aR,aG,aB, bR,bG,bB, w, r,g,b); return; }
    t -= COL_XFADE_US;
    if (t < COL_HOLD_B_US){ r=bR; g=bG; b=bB; return; }
    r=bR; g=bG; b=bB;
  }
  static inline void colorRainbow(uint32_t tLocalUs, uint8_t step, uint8_t &r, uint8_t &g, uint8_t &b){
    uint8_t A1r,A1g,A1b, B1r,B1g,B1b, nextAr,nextAg,nextAb;
    switch (step % 3){
      case 0: A1r=255; A1g=  0; A1b=  0;  B1r=255; B1g= 50; B1b=  0;  nextAr=255; nextAg=130; nextAb=0; break;
      case 1: A1r=255; A1g=130; A1b=  0;  B1r=  0; B1g=255; B1b=  0;  nextAr=  0; nextAg=  0; nextAb=255; break;
      default:A1r=  0; A1g=  0; A1b=255;  B1r=170; B1g=  0; B1b=255;  nextAr=255; nextAg=  0; nextAb=  0; break;
    }
    uint32_t t=tLocalUs;
    const uint32_t HOLD = RAIN_HOLD_US, FADE = RAIN_FADE_US;
    if (t < HOLD) { r=A1r; g=A1g; b=A1b; return; }
    t -= HOLD;
    if (t < FADE){ uint8_t w=ease_to_byte((float)t/(float)FADE);
      mixColors(A1r,A1g,A1b, B1r,B1g,B1b, w, r,g,b); return; }
    t -= FADE;
    if (t < HOLD) { r=B1r; g=B1g; b=B1b; return; }
    t -= HOLD;
    if (t < FADE){ uint8_t w=ease_to_byte((float)t/(float)FADE);
      mixColors(B1r,B1g,B1b, nextAr,nextAg,nextAb, w, r,g,b); return; }
    r=nextAr; g=nextAg; b=nextAb;
  }
  static inline void colorEpic(uint32_t tLocalUs, int64_t cyclesWhole, uint8_t &r, uint8_t &g, uint8_t &b){
    const uint8_t Acol[6][3] = {
      {  0,  0,255}, {  0,255,  0}, {  0,  0,255},
      {255,  0,  0}, {255,  0,  0}, {255,130,  0}
    };
    const uint8_t Bcol[6][3] = {
      {255,  0,  0}, {255, 50,  0}, {255,130,  0},
      {  0,255,  0}, {  0,  0,255}, {170,  0,255}
    };
    const uint8_t Ccol[6][3] = {
      {  0,255,  0}, {  0,  0,255}, {255,  0,  0},
      {255,  0,  0}, {255,130,  0}, {  0,  0,255}
    };
    uint8_t step = (uint8_t)(((cyclesWhole % 6) + 6) % 6);
    uint32_t t=tLocalUs;
    if (t <  EPIC_FLASH_US){ r=Ccol[step][0]; g=Ccol[step][1]; b=Ccol[step][2]; return; }
    t -= EPIC_FLASH_US;
    if (t < EPIC_FADE1_US) { uint8_t w=ease_to_byte((float)t/(float)EPIC_FADE1_US);
      mixColors(Ccol[step][0],Ccol[step][1],Ccol[step][2], Bcol[step][0],Bcol[step][1],Bcol[step][2], w, r,g,b); return; }
    t -= EPIC_FADE1_US;
    if (t < EPIC_HOLD_US) { r=Bcol[step][0]; g=Bcol[step][1]; b=Bcol[step][2]; return; }
    t -= EPIC_HOLD_US;
    if (t < EPIC_STEPC_US){ r=Ccol[step][0]; g=Ccol[step][1]; b=Ccol[step][2]; return; }
    r=Ccol[step][0]; g=Ccol[step][1]; b=Ccol[step][2];
  }
  static inline uint8_t brightnessEpic(uint32_t tLocalUs){
    uint32_t t=tLocalUs;
    if (t < EPIC_BR_FULL_A) return 255; t -= EPIC_BR_FULL_A;
    if (t < EPIC_BR_FADE_D) { float u=(float)t/(float)EPIC_BR_FADE_D; uint8_t w=(uint8_t)(u*255.f+0.5f); return (uint8_t)(255 - w); }
    t -= EPIC_BR_FADE_D;
    if (t < EPIC_BR_ZERO_H) return 0; t -= EPIC_BR_ZERO_H;
    if (t < EPIC_BR_FADE_U) { float u=(float)t/(float)EPIC_BR_FADE_U; return (uint8_t)(u*255.f+0.5f); }
    return 255;
  }

  static inline double pos_mod(double x, double m){ double r=fmod(x,m); if(r<0) r+=m; return r; }
  static inline int64_t floor_div(double x, double d){ return (int64_t)floor(x / d); }

  void initOnce(){ lastRealUs = micros(); vNowUs = 0.0; vColorUs = 0.0; lastNorm = 0.30f; }

  void startFresh(bool modifierOn, bool keyOn){
    running = true; pendingStop=false; paused=false; latchedReverse=false;
    vNowUs=0.0; vColorUs=0.0; lastRealUs=micros();
    memset(haveLitBits,0,sizeof(haveLitBits));
    memset(lockedBits, 0,sizeof(lockedBits));
    for (uint16_t i=0;i<LED_COUNT;i++){
      curMode[i] = (keyOn && modifierOn) ? M_EPIC
                   : (keyOn ? M_RAINBOW : (modifierOn ? M_MOD : M_DEFAULT));
    }
    strip.clear();
  }
  void requestStop(){ pendingStop = true; }
  bool isPendingStop(){ return pendingStop; }
  bool isRunning(){ return running || pendingStop; }
  void scheduleFinishThenRestart(bool modifierOn, bool keyOn){
    finishOffThenRestart = true; restartWantModifier=modifierOn; restartWantKey=keyOn;
  }
  static inline bool anyLocked(){ for(uint16_t i=0;i<LED_COUNT;i++) if(NW_BIT_TST(lockedBits,i)) return true; return false; }
  void maybeFinishStop(){
    for (uint16_t i=0;i<LED_COUNT;i++){ if (!NW_BIT_TST(lockedBits,i)) return; }
    running=false; pendingStop=false; strip.clear(); strip.show();
    if (finishOffThenRestart){ finishOffThenRestart=false; startFresh(restartWantModifier, restartWantKey); }
  }

  void tickAndRender(bool modifierDesired, bool keyDesired, uint8_t whiteEdge, bool redHeld){
    if (!isRunning()) return;
    if (whiteEdge == 1) { if (redHeld) { latchedReverse = !latchedReverse; } else { paused = !paused; } }

    uint32_t nowUs = micros();
    uint32_t dtUs  = nowUs - lastRealUs;
    lastRealUs = nowUs; if (dtUs > 50000UL) dtUs = 50000UL;

    const bool epicDesired           = (keyDesired && modifierDesired);
    const bool desiredRainbowFamily  = (keyDesired || epicDesired);

    float norm  = readKnobNorm();
    float scale = mapNormToScale(norm, epicDesired);

    bool wantReverse = latchedReverse ^ redHeld;

    double baseAdvance = (double)dtUs / (double)scale;
    double signedAdvance = paused ? (redHeld ? (wantReverse ? -baseAdvance : baseAdvance) : 0.0)
                                  : (wantReverse ? -baseAdvance : baseAdvance);

    vNowUs   += signedAdvance;
    if (desiredRainbowFamily) vColorUs += signedAdvance;
    else                      vColorUs  = vNowUs;

    const uint8_t faderByte=(uint8_t)(clamp01(g_fader)*255.f+0.5f);

    for (uint16_t i=0;i<LED_COUNT;i++){
      if (NW_BIT_TST(lockedBits,i)) { strip.setPixelColor(i,0,0,0); continue; }

      ModeSel desired = epicDesired ? M_EPIC
                         : (keyDesired ? M_RAINBOW : (modifierDesired ? M_MOD : M_DEFAULT));

      uint16_t phys = i;
      uint32_t offsetUs = (uint32_t)phys * STAGGER_US;

      bool isRainbowNow = (desired == M_RAINBOW || desired == M_EPIC);
      double baseClock = isRainbowNow ? vColorUs : vNowUs;

      bool reached = (baseClock >= (double)offsetUs);
      if (!reached && (signedAdvance >= 0) && !NW_BIT_TST(haveLitBits,i)) { strip.setPixelColor(i,0,0,0); continue; }

      double rel = baseClock - (double)offsetUs;

      uint32_t cycleUs = (desired == M_RAINBOW) ? RAIN_CYCLE_US
                           : (desired == M_EPIC ? EPIC_CYCLE_US : CYCLE_US);

      uint32_t localUs     = (uint32_t)(pos_mod(rel, (double)cycleUs) + 0.5);
      int64_t  cyclesWhole = (int64_t)floor(rel / (double)cycleUs);

      bool zeroNow = (desired == M_EPIC) ? inZeroWindowEpic(localUs) : inZeroWindow(localUs);
      if (curMode[i] != desired && zeroNow) curMode[i] = desired;

      uint8_t cr=255,cg=255,cb=255;
      uint8_t br=255;

      if (curMode[i] == M_RAINBOW) {
        uint8_t step3 = (uint8_t)(((cyclesWhole % 3) + 3) % 3);
        colorRainbow(localUs, step3, cr,cg,cb); br=255;
      } else if (curMode[i] == M_EPIC) {
        colorEpic(localUs, cyclesWhole, cr,cg,cb); br = brightnessEpic(localUs);
      } else if (curMode[i] == M_MOD) {
        uint8_t step6 = (uint8_t)(((cyclesWhole % 6) + 6) % 6);
        colorModifier(localUs, step6, cr,cg,cb); br = brightnessAt(localUs);
      } else {
        colorDefault(localUs, cr,cg,cb); br = brightnessAt(localUs);
      }

      if (br > 0) NW_BIT_SET(haveLitBits,i);

      if (pendingStop && NW_BIT_TST(haveLitBits,i)) {
        bool zeroForStop = (curMode[i] == M_EPIC) ? inZeroWindowEpic(localUs) : inZeroWindow(localUs);
        if (zeroForStop){ NW_BIT_SET(lockedBits,i); strip.setPixelColor(i,0,0,0); continue; }
      }

      uint16_t r = (uint16_t)cr * br / 255;
      uint16_t g = (uint16_t)cg * br / 255;
      uint16_t b = (uint16_t)cb * br / 255;

      r = (uint16_t)r * faderByte / 255;
      g = (uint16_t)g * faderByte / 255;
      b = (uint16_t)b * faderByte / 255;

      strip.setPixelColor(i, clamp8i((int)(r*CAL_R_GAIN+0.5f)),
                             clamp8i((int)(g*CAL_G_GAIN+0.5f)),
                             clamp8i((int)(b*CAL_B_GAIN+0.5f)));
    }

    strip.show();
    if (pendingStop) maybeFinishStop();
  }
} // namespace NewWave

// ================== Jumbo↔Rainbow handoff ==================
enum HandoverState:uint8_t{ HO_IDLE=0, HO_J2R_OFF, HO_R2J_ON };
HandoverState hoState=HO_IDLE; uint32_t hoLastMs=0, hoT_ms=0;
static inline float ledBrightHandoff(uint8_t i, bool offPhase){
  return fadeStep(hoT_ms, STAG_CENTER8_MS(i), 30, offPhase);
}

// ================== Globals / IO ==================
DebouncedInput inJumbo,inToggle,inLeft,inRight,inKey,inWave,inChaos;

// ========= remembered framebuffer =========
uint8_t g_lastShownR[LED_COUNT], g_lastShownG[LED_COUNT], g_lastShownB[LED_COUNT];

static inline void setPixelRGB(uint16_t i, uint8_t r, uint8_t g, uint8_t b){
  uint8_t f=(uint8_t)(g_fader*255.f+0.5f);
  r = (uint8_t)((uint16_t)r*f/255);
  g = (uint8_t)((uint16_t)g*f/255);
  b = (uint8_t)((uint16_t)b*f/255);
  uint8_t rc = clamp8i((int)(r * CAL_R_GAIN + 0.5f));
  uint8_t gc = clamp8i((int)(g * CAL_G_GAIN + 0.5f));
  uint8_t bc = clamp8i((int)(b * CAL_B_GAIN + 0.5f));
  strip.setPixelColor(i, rc, gc, bc);
}
static inline void setPixelCalibratedRemember(uint16_t i, uint8_t r, uint8_t g, uint8_t b){
  g_lastShownR[i]=r; g_lastShownG[i]=g; g_lastShownB[i]=b;
  setPixelRGB(i,r,g,b);
}
static inline void clearFrame(){ for (uint16_t i=0;i<LED_COUNT;i++){ g_lastShownR[i]=g_lastShownG[i]=g_lastShownB[i]=0; strip.setPixelColor(i,0,0,0);} }
static inline void setPixelCalibratedNoFader(uint16_t i, uint8_t r, uint8_t g, uint8_t b){
  uint8_t rc = clamp8i((int)(r * CAL_R_GAIN + 0.5f));
  uint8_t gc = clamp8i((int)(g * CAL_G_GAIN + 0.5f));
  uint8_t bc = clamp8i((int)(b * CAL_B_GAIN + 0.5f));
  strip.setPixelColor(i, rc, gc, bc);
}

// ================= Wave preroll / postroll (unchanged from prior) =================
struct WavePreroll {
  bool active=false;
  uint32_t startMs=0;
  bool wantModifier=false, wantKey=false;

  void begin(uint32_t now, bool modifierOn, bool keyOn){
    active=true; startMs=now; wantModifier=modifierOn; wantKey=keyOn;
  }
  bool tick(uint32_t now){
    if(!active) return false;
    uint32_t t = now - startMs;
    if (t >= 800){ active=false; return false; }
    uint8_t seg = (t<400)?0:1;
    float u = ease01((float)(t - (seg?400:0)) / 400.0f);

    for (int i=0;i<LED_COUNT;i++){
      bool firstPair = ((i/2)%2==0);
      float a = (seg==0) ? (firstPair ? u : 0.f)
                         : (firstPair ? 1.f : u);
      uint8_t r = (uint8_t)((1.f - a) * g_lastShownR[i] + 0.5f);
      uint8_t g = (uint8_t)((1.f - a) * g_lastShownG[i] + 0.5f);
      uint8_t b = (uint8_t)((1.f - a) * g_lastShownB[i] + 0.5f);
      setPixelCalibratedRemember(i, r,g,b);
    }
    strip.show();
    return true;
  }
  bool anyLitBeforeStart() const {
    for(int i=0;i<LED_COUNT;i++){
      if(g_lastShownR[i]|g_lastShownG[i]|g_lastShownB[i]) return true;
    }
    return false;
  }
} g_wavePreroll;

struct WavePostrollUp {
  bool active=false;
  uint32_t startMs=0;
  enum PT:uint8_t{ PT_NONE=0, PT_RAINBOW, PT_CLASSIC, PT_JUMBO } type=PT_NONE;
  uint32_t snapPhaseMs=0;
  uint16_t snapHOLD=0, snapFADE=0, snapSTAG=0;
  uint8_t  snapCr=255, snapCg=255, snapCb=255;

  void begin(uint32_t now, bool rockerOn, bool /*toggleOn*/, bool keyOn){
    if (keyOn){
      type=PT_RAINBOW;
      snapPhaseMs = phaseMs;
      snapHOLD    = HOLD_MS;
      snapFADE    = FADE_MS;
      snapSTAG    = STAG_MS;
    } else if (g_mode==MODE_CLASSIC){
      type=PT_CLASSIC;
      uint8_t rc,gc,bc;
      if (bridge.active){
        uint8_t rb,gb,bb; uint32_t hold=bridge.lastMs;
        bridge.update(now, 0.f, rb,gb,bb); bridge.lastMs=hold;
        rc=rb; gc=gb; bc=bb;
      } else {
        classicColorFromKnob(g_lastKnobNorm, rc,gc,bc);
      }
      snapCr=rc; snapCg=gc; snapCb=bc;
    } else if (rockerOn){
      type=PT_JUMBO;
      colorSeq.update(now, g_speedScale);
      colorSeq.get(snapCr, snapCg, snapCb);
    } else {
      type=PT_NONE;
    }

    if (type==PT_NONE){ active=false; return; }
    for (int i=0;i<LED_COUNT;i++) setPixelCalibratedRemember(i, 0,0,0);
    strip.show();
    active=true; startMs=now;
  }

  inline void targetAt(uint16_t i, uint8_t& r,uint8_t& g,uint8_t& b){
    if (type==PT_RAINBOW){
      uint32_t LOOP = (uint32_t)7*((uint32_t)snapHOLD + (uint32_t)snapFADE);
      int32_t tLocal = wrapLoop((int32_t)snapPhaseMs - (int32_t)i*(int32_t)snapSTAG, (int32_t)LOOP);
      KR::RGB c = KR::colorAt((uint32_t)tLocal, snapHOLD, snapFADE);
      r=c.r; g=c.g; b=c.b;
    } else {
      r=snapCr; g=snapCg; b=snapCb;
    }
  }

  bool tick(uint32_t now){
    if(!active) return false;
    uint32_t t = now - startMs;
    if (t >= 800){
      for (int i=0;i<LED_COUNT;i++){
        uint8_t r,g,b; targetAt(i,r,g,b);
        setPixelCalibratedRemember(i, r,g,b);
      }
      strip.show(); active=false; return false;
    }
    uint8_t seg = (t<400)?0:1;
    float u = ease01((float)(t - (seg?400:0)) / 400.0f);

    for (int i=0;i<LED_COUNT;i++){
      bool firstPair = ((i/2)%2==0);
      float a = (seg==0) ? (firstPair ? u : 0.f)
                         : (firstPair ? 1.f : u);
      uint8_t r,g,b; targetAt(i,r,g,b);
      uint8_t R = (uint8_t)(r*a + 0.5f);
      uint8_t G = (uint8_t)(g*a + 0.5f);
      uint8_t B = (uint8_t)(b*a + 0.5f);
      setPixelCalibratedRemember(i, R,G,B);
    }
    strip.show();
    return true;
  }
} g_wavePostUp;

bool g_startPostrollPending = false;

// ---------- SECRET MODE ----------
float g_secretEntryFader = 1.0f;
static inline void secretGradientAtIndex(uint16_t i, uint8_t& r, uint8_t& g, uint8_t& b){
  namespace K = KR;
  const K::RGB GRAD[7] = { K::VIOLET, K::INDIGO, K::BLUE, K::GREEN, K::YELLOW, K::ORANGE, K::RED };
  if (LED_COUNT <= 1){ r=GRAD[0].r; g=GRAD[0].g; b=GRAD[0].b; return; }
  float pos = (float)i / (float)(LED_COUNT - 1);
  float f   = pos * 6.0f;
  int   k   = (int)f; if(k<0) k=0; if(k>5) k=5;
  float t   = f - (float)k;
  K::RGB A = GRAD[k], C = GRAD[k+1];
  K::RGB M = K::mix(A, C, t);
  r=M.r; g=M.g; b=M.b;
}
static inline void secretSolidFromFader(float u, uint8_t& r, uint8_t& g, uint8_t& b){
  namespace K = KR;
  const K::RGB STOPS[6] = { K::BLUE, K::GREEN, K::YELLOW, K::ORANGE, K::RED, K::VIOLET };
  u = clamp01(u);
  float f = u * 5.0f;
  int   k = (int)f; if(k<0) k=0; if(k>4) k=4;
  float t = f - (float)k;
  K::RGB A = STOPS[k], B = STOPS[k+1];
  K::RGB M = K::mix(A, B, t);
  r=M.r; g=M.g; b=M.b;
}
static inline uint8_t scaleSecret(uint8_t v){
  uint16_t s = (uint16_t)(clamp01(g_secretEntryFader)*255.f + 0.5f);
  return (uint8_t)((uint16_t)v * s / 255);
}
static inline void renderSecret(){
  int lit = (int)lrintf(g_lastKnobNorm * (float)LED_COUNT);
  if(lit < 0) lit = 0; if(lit > LED_COUNT) lit = LED_COUNT;

  const float u = clamp01(g_fader);
  const float RAINBOW_ZONE_END = 0.15f;
  float w = 0.0f;
  if (u <= RAINBOW_ZONE_END){
    float t = 1.0f - (u / RAINBOW_ZONE_END);
    w = ease01(t);
  }
  uint8_t sR,sG,sB; secretSolidFromFader(u, sR,sG,sB);

  for(int i=0;i<LED_COUNT;i++){
    if(i < lit){
      uint8_t R,G,B;
      if (w <= 0.0f){ R=sR; G=sG; B=sB; }
      else if (w >= 1.0f){ secretGradientAtIndex(i, R,G,B); }
      else{
        uint8_t gR,gG,gB; secretGradientAtIndex(i, gR,gG,gB);
        R = mix8(sR, gR, w); G = mix8(sG, gG, w); B = mix8(sB, gB, w);
      }
      uint8_t Rb = scaleSecret(R);
      uint8_t Gb = scaleSecret(G);
      uint8_t Bb = scaleSecret(B);
      setPixelCalibratedNoFader(i, Rb,Gb,Bb);
      g_lastShownR[i]=Rb; g_lastShownG[i]=Gb; g_lastShownB[i]=Bb;
    }else{
      setPixelCalibratedNoFader(i, 0,0,0);
      g_lastShownR[i]=g_lastShownG[i]=g_lastShownB[i]=0;
    }
  }
  strip.show();
}

// ========================= Dots overlay (unchanged) =========================
namespace Dots {
  uint16_t BASE_IN_MS=5, BASE_HOLD_MS=30, BASE_OUT_MS=5, BASE_STAG_MS=25;
  float L_MULT_IN=15.0f, L_MULT_HOLD=4.0f, L_MULT_OUT=15.0f, L_MULT_STAG=4.0f;
  float R_MULT_IN=0.125f, R_MULT_HOLD=(1.0f/6.0f), R_MULT_OUT=0.125f, R_MULT_STAG=0.125f;

  uint16_t TO_CLR_TO_COL_MS = 5, HOLD_COL_MS = 30, TO_COL_TO_CLR_MS = 5, g_staggerMs = 25;
  uint16_t g_t1 = 5, g_t2 = 35, g_t3 = 40;

  float    g_ledsPerMs        = 1.0f / 25.0f;
  float    g_invSpeedMsPerLED = 25.0f;

  uint32_t redHoldStartMs = 0;
  bool redHoldArmed=false, redHoldFired=false;

  struct Dot {
    uint8_t  active = 0;
    float    pos    = 0.f;
    int8_t   dir    = +1;
    uint32_t lastMs = 0;
    uint8_t  r=255, g=255, b=255;
    uint16_t seq    = 0;
  };
  static const uint8_t MAX_DOTS = 16;
  Dot dots[MAX_DOTS];
  uint16_t g_seqCounter = 1;

  const uint8_t PAL[6][3] PROGMEM = {
    {170,  0,255}, {0,0,255}, {0,255,0},
    {255,130,0}, {255,50,0}, {255,0,0}
  };
  uint8_t palIdx = 0;

  static inline float clamp01f(float x){ return x<0.f?0.f:(x>1.f?1.f:x); }
  static float smoother5(float u){ u=clamp01f(u); return u*u*u*(u*(u*6.f - 15.f) + 10.f); }
  static uint16_t iroundf16(float x){ return (uint16_t)(x + 0.5f); }

  static float scaleFromT(float t, float Lmult, float Rmult){
    t = clamp01f(t);
    if (t <= 0.5f){ float u=t/0.5f; return Lmult + u*(1.0f - Lmult); }
    else { float u=(t - 0.5f)/0.5f; return 1.0f + u*(Rmult - 1.0f); }
  }
  static void updateTimingsFromKnob(){
    float t = analogRead(PIN_SPEED_KNOB) / 1023.0f;
    uint16_t inMs   = iroundf16(BASE_IN_MS   * scaleFromT(t, L_MULT_IN,   R_MULT_IN));
    uint16_t holdMs = iroundf16(BASE_HOLD_MS * scaleFromT(t, L_MULT_HOLD, R_MULT_HOLD));
    uint16_t outMs  = iroundf16(BASE_OUT_MS  * scaleFromT(t, L_MULT_OUT,  R_MULT_OUT));
    uint16_t stagMs = iroundf16(BASE_STAG_MS * scaleFromT(t, L_MULT_STAG, R_MULT_STAG));
    if (stagMs < 1) stagMs = 1;
    if (inMs!=TO_CLR_TO_COL_MS || holdMs!=HOLD_COL_MS || outMs!=TO_COL_TO_CLR_MS || stagMs!=g_staggerMs){
      TO_CLR_TO_COL_MS = inMs; HOLD_COL_MS = holdMs; TO_COL_TO_CLR_MS = outMs; g_staggerMs = stagMs;
      g_ledsPerMs        = 1.0f / (float)g_staggerMs;
      g_invSpeedMsPerLED = (float)g_staggerMs;
      g_t1 = TO_CLR_TO_COL_MS; g_t2 = g_t1 + HOLD_COL_MS; g_t3 = g_t2 + TO_COL_TO_CLR_MS;
    }
  }
  static float kernelWeight(float dt_ms){
    if (dt_ms <= 0.f) return 0.f;
    if (dt_ms < (float)g_t1){ if (g_t1==0) return 1.f; return smoother5(dt_ms/(float)g_t1); }
    if (dt_ms < (float)g_t2) return 1.f;
    if (dt_ms < (float)g_t3){
      uint16_t fd = TO_COL_TO_CLR_MS; if (fd==0) return 0.f;
      float u=(dt_ms-(float)g_t2)/(float)fd; return 1.f - smoother5(u);
    }
    return 0.f;
  }
  static float trailFrom(float ledIndex, float headPos, int8_t headDir){
    const float stepMs = g_invSpeedMsPerLED;
    if (headDir < 0){
      if (ledIndex + 0.5f < headPos) return 0.f;
      float dist = (ledIndex - headPos);
      return kernelWeight(dist * stepMs);
    }else{
      if (ledIndex - 0.5f > headPos) return 0.f;
      float dist = (headPos - ledIndex);
      return kernelWeight(dist * stepMs);
    }
  }
  static void reflectAdvance(Dot& d, float dt_ms){
    if (dt_ms <= 0.f) return;
    const float L = (float)(LED_COUNT - 1);
    float v = (d.dir >= 0) ? +g_ledsPerMs : -g_ledsPerMs;
    float p = d.pos + v * dt_ms;
    for (uint8_t g=0; g<6; ++g){
      if (p < 0.f)   { p = -p;        d.dir = -d.dir; continue; }
      if (p > L)     { p = 2.f*L - p; d.dir = -d.dir; continue; }
      break;
    }
    if (p < 0.f) p = 0.f; else if (p > L) p = L;
    d.pos = p;
  }
  static void initDot(Dot& d, uint32_t now, int8_t dir, uint8_t r,uint8_t g,uint8_t b, uint16_t seq){
    d.active=1; d.pos=0.f; d.dir=dir; d.lastMs=now; d.r=r; d.g=g; d.b=b; d.seq=seq;
  }
  static void spawnDot(uint32_t now){
    uint8_t r = pgm_read_byte(&PAL[palIdx][0]);
    uint8_t g = pgm_read_byte(&PAL[palIdx][1]);
    uint8_t b = pgm_read_byte(&PAL[palIdx][2]);
    palIdx = (uint8_t)((palIdx + 1) % 6);
    for (uint8_t i=0;i<MAX_DOTS;i++){
      if (!dots[i].active){ initDot(dots[i], now, +1, r,g,b, g_seqCounter++); return; }
    }
    initDot(dots[0], now, +1, r,g,b, g_seqCounter++);
  }
  static bool deleteOldest(){
    int8_t best=-1; uint16_t bestSeq=0xFFFFu;
    for (uint8_t i=0;i<MAX_DOTS;i++) if (dots[i].active && dots[i].seq<bestSeq){ bestSeq=dots[i].seq; best=i; }
    if (best>=0){ dots[best].active=0; return true; } return false;
  }
  static void deleteAll(){ for (uint8_t i=0;i<MAX_DOTS;i++) dots[i].active=0; }

  static inline uint8_t cal(uint8_t v, float gain){
    int x = (int)(v * gain + 0.5f); if (x<0) x=0; if (x>255) x=255; return (uint8_t)x;
  }

  void updateAndOverlay(uint32_t now, bool allowInput, uint8_t edgeLeft, uint8_t edgeRight, bool redHeld){
    updateTimingsFromKnob();
    if (allowInput){
      if (edgeRight==2){ spawnDot(now); }
      if (edgeLeft==2){ deleteOldest(); redHoldArmed=true; redHoldFired=false; redHoldStartMs=now; }
      if (redHeld && redHoldArmed && !redHoldFired && (now - redHoldStartMs >= 500)){ deleteAll(); redHoldFired=true; }
      if (edgeLeft==1){ redHoldArmed=false; redHoldFired=false; }
    }
    const uint32_t MAX_DT = 40;
    for (uint8_t i=0;i<MAX_DOTS;i++){
      Dot& d = dots[i]; if (!d.active) continue;
      uint32_t dt = now - d.lastMs; d.lastMs = now; if (dt > MAX_DT) dt = MAX_DT;
      reflectAdvance(d, (float)dt);
    }
    for (int i=0;i<LED_COUNT;i++){
      float prod = 1.f, accR=0.f, accG=0.f, accB=0.f;
      for (uint8_t k=0;k<MAX_DOTS;k++){
        const Dot& d = dots[k]; if (!d.active) continue;
        float w = trailFrom((float)i, d.pos, d.dir); if (w <= 0.f) continue;
        prod *= (1.f - w);
        accR += w * cal(d.r, CAL_R_GAIN);
        accG += w * cal(d.g, CAL_G_GAIN);
        accB += w * cal(d.b, CAL_B_GAIN);
      }
      float wTotal = 1.f - prod;
      uint8_t outR, outG, outB;
      if (wTotal > 0.f){
        float inv = 1.f / wTotal;
        float tr = accR * inv, tg = accG * inv, tb = accB * inv;
        float baseR = (float)g_lastShownR[i];
        float baseG = (float)g_lastShownG[i];
        float baseB = (float)g_lastShownB[i];
        outR = (uint8_t)(baseR*(1.f - wTotal) + tr*wTotal + 0.5f);
        outG = (uint8_t)(baseG*(1.f - wTotal) + tg*wTotal + 0.5f);
        outB = (uint8_t)(baseB*(1.f - wTotal) + tb*wTotal + 0.5f);
      } else {
        outR = g_lastShownR[i]; outG = g_lastShownG[i]; outB = g_lastShownB[i];
      }
      strip.setPixelColor(i, outR, outG, outB);
      g_lastShownR[i]=outR; g_lastShownG[i]=outG; g_lastShownB[i]=outB;
    }
    strip.show();
  }

  void updateAndOverlayRange(uint32_t now, bool allowInput, uint8_t edgeLeft, uint8_t edgeRight, bool redHeld,
                             uint16_t iStart, uint16_t iEnd){
    updateTimingsFromKnob();
    if (allowInput){
      if (edgeRight==2){ spawnDot(now); }
      if (edgeLeft==2){ deleteOldest(); redHoldArmed=true; redHoldFired=false; redHoldStartMs=now; }
      if (redHeld && redHoldArmed && !redHoldFired && (now - redHoldStartMs >= 500)){ deleteAll(); redHoldFired=true; }
      if (edgeLeft==1){ redHoldArmed=false; redHoldFired=false; }
    }
    const uint32_t MAX_DT = 40;
    for (uint8_t i=0;i<MAX_DOTS;i++){
      Dot& d = dots[i]; if (!d.active) continue;
      uint32_t dt = now - d.lastMs; d.lastMs = now; if (dt > MAX_DT) dt = MAX_DT;
      reflectAdvance(d, (float)dt);
    }
    for (uint16_t i=iStart;i<iEnd;i++){
      float prod = 1.f, accR=0.f, accG=0.f, accB=0.f;
      for (uint8_t k=0;k<MAX_DOTS;k++){
        const Dot& d = dots[k]; if (!d.active) continue;
        float w = trailFrom((float)i, d.pos, d.dir); if (w <= 0.f) continue;
        prod *= (1.f - w);
        accR += w * cal(d.r, CAL_R_GAIN);
        accG += w * cal(d.g, CAL_G_GAIN);
        accB += w * cal(d.b, CAL_B_GAIN);
      }
      float wTotal = 1.f - prod;
      uint8_t outR, outG, outB;
      if (wTotal > 0.f){
        float inv = 1.f / wTotal;
        float tr = accR * inv, tg = accG * inv, tb = accB * inv;
        float baseR = (float)g_lastShownR[i];
        float baseG = (float)g_lastShownG[i];
        float baseB = (float)g_lastShownB[i];
        outR = (uint8_t)(baseR*(1.f - wTotal) + tr*wTotal + 0.5f);
        outG = (uint8_t)(baseG*(1.f - wTotal) + tg*wTotal + 0.5f);
        outB = (uint8_t)(baseB*(1.f - wTotal) + tb*wTotal + 0.5f);
      } else {
        outR = g_lastShownR[i]; outG = g_lastShownG[i]; outB = g_lastShownB[i];
      }
      strip.setPixelColor(i, outR, outG, outB);
      g_lastShownR[i]=outR; g_lastShownG[i]=outG; g_lastShownB[i]=outB;
    }
    strip.show();
  }
} // namespace Dots

// ========================= CHAOS MODE =========================
struct ChaosState {
  double waveUs=0.0;
  double colorUs=0.0;
  uint32_t lastUs=0;
  uint32_t phaseMs=0;   // Rainbow quarter phase
  bool paused=false;
  bool reverse=false;
} g_chaos;

// [CHAOS BUTTON FIX] — move Chaos Rainbow button state to globals
bool     gChaosInScrub=false;
uint32_t gChaosLeftStart=0, gChaosRightStart=0;
bool     gChaosLeftLong=false, gChaosRightLong=false;
float    gChaosScrubPrev=0.5f;

// Advance clocks with g_speedScale (faster to right)
static inline void chaosAdvanceClocks(){
  uint32_t nowUs = micros();
  uint32_t dtUs  = (g_chaos.lastUs==0) ? 0 : (nowUs - g_chaos.lastUs);
  if (dtUs > 50000UL) dtUs = 50000UL;
  g_chaos.lastUs = nowUs;

  double dir = g_chaos.reverse ? -1.0 : +1.0;
  double adv = g_chaos.paused ? 0.0 : dir * ((double)dtUs * (double)g_speedScale);

  g_chaos.waveUs  += adv;
  g_chaos.colorUs += adv;

  // Rainbow quarter uses ms phase
  uint32_t LOOP = loopMs();
  int32_t advMs = (int32_t)lrintf((float)(dtUs/1000.0f) * g_speedScale);
  g_chaos.phaseMs = (uint32_t)wrapLoop((int32_t)g_chaos.phaseMs + advMs, (int32_t)LOOP);
}

static void renderChaosFrame(uint32_t nowMs, uint32_t dtMs, bool toggleOn,
                             uint8_t eL, uint8_t eR, bool leftDown, bool rightDown) {
  (void)nowMs; (void)dtMs;
  const uint8_t f=(uint8_t)(g_fader*255.f+0.5f);
  const uint16_t q = LED_COUNT / 4;
  const uint16_t Q0A = 0,     Q0B = q;
  const uint16_t Q1A = q,     Q1B = 2*q;
  const uint16_t Q2A = 2*q,   Q2B = 3*q;
  const uint16_t Q3A = 3*q,   Q3B = LED_COUNT;

  // ---- Quarter 0: Wave + Modifier ----
  for (uint16_t i=Q0A;i<Q0B;i++){
    uint16_t phys = i;
    uint32_t cycleUs = NewWave::CYCLE_US;
    double   rel = g_chaos.waveUs - (double)phys * (double)NewWave::STAGGER_US;
    uint32_t localUs     = (uint32_t)(NewWave::pos_mod(rel, (double)cycleUs) + 0.5);
    int64_t  cyclesWhole = (int64_t)floor(rel / (double)cycleUs);

    uint8_t r,g,b; NewWave::colorModifier(localUs, (uint8_t)(((cyclesWhole%6)+6)%6), r,g,b);
    uint8_t br = NewWave::brightnessAt(localUs);
    uint8_t R=(uint8_t)((uint16_t)((uint16_t)r*br/255)*f/255);
    uint8_t G=(uint8_t)((uint16_t)((uint16_t)g*br/255)*f/255);
    uint8_t B=(uint8_t)((uint16_t)((uint16_t)b*br/255)*f/255);
    strip.setPixelColor(i,
      clamp8i((int)(R*CAL_R_GAIN+0.5f)),
      clamp8i((int)(G*CAL_G_GAIN+0.5f)),
      clamp8i((int)(B*CAL_B_GAIN+0.5f)));
    g_lastShownR[i]=R; g_lastShownG[i]=G; g_lastShownB[i]=B;
  }

  // ---- Quarter 1: Rainbow (moving; responds to buttons) ----
  bool bothDown = leftDown && rightDown;

  // presses
  if (eR==2){ gChaosRightStart=nowMs; gChaosRightLong=false; }
  if (eL==2){ gChaosLeftStart =nowMs; gChaosLeftLong =false; }

  // enter/exit scrub
  if (bothDown && !gChaosInScrub){ gChaosInScrub=true; gChaosScrubPrev=analogRead(PIN_SPEED_KNOB)/1023.f; gChaosLeftLong=false; gChaosRightLong=false; }
  else if(!bothDown && gChaosInScrub){ gChaosInScrub=false; }

  // long-hold tracking
  if(!bothDown){
    if (leftDown  && !gChaosLeftLong  && (nowMs-gChaosLeftStart  >= LONG_HOLD_MS))  gChaosLeftLong  = true;
    if (rightDown && !gChaosRightLong && (nowMs-gChaosRightStart >= LONG_HOLD_MS))  gChaosRightLong = true;
  }

  // [CHAOS BUTTON FIX] — clear long flags on release regardless of bothDown
  if (eR==1) gChaosRightLong = false;
  if (eL==1) gChaosLeftLong  = false;

  // Adjust params (HOLD/FADE/STAG)
  if(!bothDown){
    if (gChaosLeftLong){
      uint16_t step=(uint16_t)((float)dtMs*0.35f);
      HOLD_MS=(HOLD_MS>=step)?(uint16_t)(HOLD_MS-step):0;
      FADE_MS=(FADE_MS>=step)?(uint16_t)(FADE_MS-step):0;
      enforceMinBlock();
    }
    if (gChaosRightLong){
      uint16_t step=(uint16_t)((float)dtMs*0.35f);
      HOLD_MS=addClamped(HOLD_MS,step,MAX_TIME_MS);
      FADE_MS=addClamped(FADE_MS,step,MAX_TIME_MS);
      enforceMinBlock();
    }
    if((eR==1) && !gChaosRightLong){
      if(toggleOn){ STAG_MS = clampU16((uint16_t)(STAG_MS+20), STAG_MIN_MS, STAG_MAX_MS); }
      else{ HOLD_MS=addClamped(HOLD_MS,10,MAX_TIME_MS); FADE_MS=addClamped(FADE_MS,10,MAX_TIME_MS); enforceMinBlock(); }
    }
    if((eL==1) && !gChaosLeftLong ){
      if(toggleOn){ STAG_MS = clampU16((uint16_t)(STAG_MS>=20?STAG_MS-20:0), STAG_MIN_MS, STAG_MAX_MS); }
      else{ HOLD_MS=(HOLD_MS>=10)?(uint16_t)(HOLD_MS-10):0; FADE_MS=(FADE_MS>=10)?(uint16_t)(FADE_MS-10):0; enforceMinBlock(); }
    }
  }

  // Play vs Scrub for Chaos Rainbow
  uint32_t LOOP = loopMs();
  if(gChaosInScrub){
    float raw=analogRead(PIN_SPEED_KNOB)/1023.f;
    float delta=raw-gChaosScrubPrev;
    if(fabsf(delta)>SCRUB_DELTA_DB){
      int32_t dMs=(int32_t)(delta*(float)LOOP*SCRUB_GAIN);
      g_chaos.phaseMs=(uint32_t)wrapLoop((int32_t)g_chaos.phaseMs + dMs, (int32_t)LOOP);
      gChaosScrubPrev=raw;
    }
  }else{
    // normal advance already applied in chaosAdvanceClocks()
    // [CHANGE: CHAOS toggle dir+speed]
    // When toggle is ON, Q1 rainbow runs reverse at 2×: net effect vs base (+1×) is an extra (-3×) step.
    if (toggleOn){
      int32_t S = (int32_t)lrintf((float)dtMs * g_speedScale);
      int32_t delta = -3 * S;
      g_chaos.phaseMs = (uint32_t)wrapLoop((int32_t)g_chaos.phaseMs + delta, (int32_t)LOOP);
    }
  }

  for (uint16_t i=Q1A;i<Q1B;i++){
    int32_t tLocal = wrapLoop((int32_t)g_chaos.phaseMs - (int32_t)i*(int32_t)STAG_MS, (int32_t)LOOP);
    KR::RGB c = KR::colorAt((uint32_t)tLocal, HOLD_MS, FADE_MS);
    uint8_t R=(uint8_t)((uint16_t)c.r*f/255);
    uint8_t G=(uint8_t)((uint16_t)c.g*f/255);
    uint8_t B=(uint8_t)((uint16_t)c.b*f/255);
    strip.setPixelColor(i,
      clamp8i((int)(R*CAL_R_GAIN+0.5f)),
      clamp8i((int)(G*CAL_G_GAIN+0.5f)),
      clamp8i((int)(B*CAL_B_GAIN+0.5f)));
    g_lastShownR[i]=R; g_lastShownG[i]=G; g_lastShownB[i]=B;
  }

  // ---- Quarter 2: Jumbo + Toggle Modifier (Dots overlay enabled here) ----
  static bool q2Prev=false;
  bool seqActive = toggleOn; // respect modifier: OFF=frozen; ON=fade through
  if(!q2Prev && seqActive) colorSeq.onActiveRising(nowMs);
  else if(q2Prev && !seqActive) colorSeq.onActiveFalling(nowMs);
  q2Prev=seqActive; colorSeq.update(nowMs,g_speedScale);
  uint8_t jr,jg,jb; colorSeq.get(jr,jg,jb);

  for (uint16_t i=Q2A;i<Q2B;i++){
    uint8_t vF=f;
    uint8_t R=(uint8_t)((uint16_t)jr*vF/255);
    uint8_t G=(uint8_t)((uint16_t)jg*vF/255);
    uint8_t B=(uint8_t)((uint16_t)jb*vF/255);
    strip.setPixelColor(i,
      clamp8i((int)(R*CAL_R_GAIN+0.5f)),
      clamp8i((int)(G*CAL_G_GAIN+0.5f)),
      clamp8i((int)(B*CAL_B_GAIN+0.5f)));
    g_lastShownR[i]=R; g_lastShownG[i]=G; g_lastShownB[i]=B;
  }
  Dots::updateAndOverlayRange(nowMs, /*allowInput=*/true, eL, eR, leftDown, Q2A, Q2B);

  // ---- Quarter 3: Epic Rainbow ----
  for (uint16_t i=Q3A;i<Q3B;i++){
    uint16_t phys = i;
    uint32_t cycleUs = NewWave::EPIC_CYCLE_US;
    double   rel = g_chaos.waveUs - (double)phys * (double)NewWave::STAGGER_US;
    uint32_t localUs     = (uint32_t)(NewWave::pos_mod(rel, (double)cycleUs) + 0.5);
    int64_t  cyclesWhole = (int64_t)floor(rel / (double)cycleUs);

    uint8_t r,g,b; NewWave::colorEpic(localUs, cyclesWhole, r,g,b);
    uint8_t br = NewWave::brightnessEpic(localUs);
    uint8_t R=(uint8_t)((uint16_t)((uint16_t)r*br/255)*f/255);
    uint8_t G=(uint8_t)((uint16_t)((uint16_t)g*br/255)*f/255);
    uint8_t B=(uint8_t)((uint16_t)((uint16_t)b*br/255)*f/255);
    strip.setPixelColor(i,
      clamp8i((int)(R*CAL_R_GAIN+0.5f)),
      clamp8i((int)(G*CAL_G_GAIN+0.5f)),
      clamp8i((int)(B*CAL_B_GAIN+0.5f)));
    g_lastShownR[i]=R; g_lastShownG[i]=G; g_lastShownB[i]=B;
  }
  strip.show();
}

// ================== Setup ==================
void setup(){
  strip.begin(); strip.setBrightness(255); clearFrame(); strip.show();

  inJumbo.begin(PIN_ROCKER,true); inToggle.begin(PIN_TOGGLE,true);
  inLeft.begin(PIN_LEFT,true);    inRight.begin(PIN_RIGHT,true);
  inKey.begin(PIN_KEY,true);      inWave.begin(PIN_WAVE,true);
  inChaos.begin(PIN_CHAOS,true);

  uint32_t now=millis(); g_lastMsBright=now; colorSeq.lastMs=now; lastMs=now; brLastMs=now; hoLastMs=now;

  bool rockerOn=inJumbo.pressed();
  bool toggleOn=inToggle.pressed();
  bool keyOn=inKey.pressed();
  bool waveOn=inWave.pressed();

  g_wantOn=rockerOn;
  if(rockerOn && toggleOn && (g_mode==MODE_JUMBO)) colorSeq.onActiveRising(now);

  if(keyOn){ brState=BR_HOLD; phaseMs=((LED_COUNT-1)*DEFAULT_STAG_MS)%loopMs(); }
  else      brState=BR_OFF;

  NewWave::initOnce();
  if (waveOn){
    if (g_wavePreroll.anyLitBeforeStart()){
      g_wavePreroll.begin(now, toggleOn, keyOn);
    }else{
      NewWave::startFresh(toggleOn, keyOn);
    }
  }
}

// ================== Main loop ==================
void loop(){
  uint32_t now=millis(); uint32_t dt=now-lastMs; lastMs=now;
  updateSpeed(); updateFader();

  // Edges
  uint8_t eJ=inJumbo.update(now);
  uint8_t eT=inToggle.update(now);
  uint8_t eL=inLeft.update(now);
  uint8_t eR=inRight.update(now);
  uint8_t eK=inKey.update(now);
  uint8_t eW=inWave.update(now);
  uint8_t eC=inChaos.update(now);

  bool rockerOn=inJumbo.pressed();
  bool toggleOn=inToggle.pressed();
  bool keyOn=inKey.pressed();
  bool waveOn=inWave.pressed();

  // [CHANGE: reverse chaos switch] — interpret chaos as inverted pressed()
  bool chaosOn = !inChaos.pressed();

  // Reset Chaos Rainbow phase & knobs when Chaos toggles OFF or ON
  if (eC==1 || eC==2){
    g_chaos.phaseMs = 0;
    STAG_MS = DEFAULT_STAG_MS;
    HOLD_MS = DEFAULT_HOLD_MS;
    FADE_MS = DEFAULT_FADE_MS;
    enforceMinBlock();
    gChaosInScrub = false;
    gChaosLeftLong = gChaosRightLong = false;
    gChaosLeftStart = gChaosRightStart = 0;
  }

  // ========== CHAOS priority ==========
  if (chaosOn){
    // Q0 Wave controls (pause/reverse)
    if (eR==1){
      if (inLeft.pressed()) g_chaos.reverse = !g_chaos.reverse;
      else                  g_chaos.paused  = !g_chaos.paused;
    }
    chaosAdvanceClocks();
    renderChaosFrame(now, dt, toggleOn, eL, eR, inLeft.pressed(), inRight.pressed());
    return;
  }

  // ==================== SECRET PRIORITY ====================
  if (waveOn && toggleOn && rockerOn && keyOn){
    if (g_mode != MODE_SECRET){
      g_secretEntryFader = g_fader;
    }
    g_mode = MODE_SECRET;
    renderSecret();
    Dots::updateAndOverlay(now, /*allowInput=*/true, eL, eR, inLeft.pressed());
    return;
  } else if (g_mode == MODE_SECRET){
    g_mode = MODE_JUMBO;
  }

  // ==================== WAVE path ====================
  if (eW==2){
    if (g_wavePreroll.anyLitBeforeStart()){
      g_wavePreroll.begin(now, toggleOn, keyOn);
    } else {
      if (NewWave::isPendingStop()) NewWave::scheduleFinishThenRestart(toggleOn, keyOn);
      else if (!NewWave::isRunning()) NewWave::startFresh(toggleOn, keyOn);
    }
  } else if (eW==1){
    if (NewWave::isRunning()) NewWave::requestStop();
    g_startPostrollPending = true;
  }

  if (g_wavePreroll.active){
    if (!g_wavePreroll.tick(now)){
      if (NewWave::isPendingStop()) NewWave::scheduleFinishThenRestart(g_wavePreroll.wantModifier, g_wavePreroll.wantKey);
      else                          NewWave::startFresh(g_wavePreroll.wantModifier, g_wavePreroll.wantKey);
    }
    return;
  }

  if (waveOn || NewWave::isRunning()){
    NewWave::tickAndRender(toggleOn, keyOn, /*whiteEdge*/ eR, /*redHeld*/ inLeft.pressed());
    return;
  }

  if (g_startPostrollPending && !g_wavePostUp.active){
    bool backgroundWillLight = keyOn || (g_mode==MODE_CLASSIC) || rockerOn;
    if (backgroundWillLight){
      g_wavePostUp.begin(now, rockerOn, toggleOn, keyOn);
    }
    g_startPostrollPending = false;
  }

  if (g_wavePostUp.active){
    g_wavePostUp.tick(now);
    if (g_wavePostUp.type==WavePostrollUp::PT_JUMBO || g_wavePostUp.type==WavePostrollUp::PT_CLASSIC){
      Dots::updateAndOverlay(now, /*allowInput=*/true, eL, eR, inLeft.pressed());
    }
    return;
  }

  // ==================== Non-wave: Handover + Rainbow + Jumbo ====================
  if(hoState==HO_IDLE && rockerOn && (eK==2) && (brState==BR_OFF)){ hoState=HO_J2R_OFF; hoT_ms=0; hoLastMs=now; }
  uint32_t brDt=now-brLastMs; brLastMs=now;
  bool blockRainbowOn=(hoState==HO_J2R_OFF);
  BrState brPrev=brState;

  switch(brState){
    case BR_OFF:
      if(keyOn && !blockRainbowOn){ brState=BR_ON_ANIM; brT_ms=0; phaseMs=((LED_COUNT-1)*STAG_MS)%loopMs(); }
      break;
    case BR_ON_ANIM:
      if(keyOn){ brT_ms+=brDt; if(brT_ms >= (uint32_t)STAG_CENTER20_MS(0) + 30) brState=BR_HOLD; }
      else{ brState=BR_OFF_ANIM; brT_ms=0; }
      break;
    case BR_HOLD:
      if(!keyOn){ brState=BR_OFF_ANIM; brT_ms=0; }
      break;
    case BR_OFF_ANIM:
      brT_ms+=brDt; if(brT_ms >= (uint32_t)STAG_CENTER20_MS(0) + 30) brState=BR_OFF;
      if(keyOn && brState==BR_OFF_ANIM){ brState=BR_ON_ANIM; brT_ms=0; }
      break;
  }
  if(brPrev==BR_OFF_ANIM && brState==BR_OFF){
    STAG_MS=DEFAULT_STAG_MS; HOLD_MS=DEFAULT_HOLD_MS; FADE_MS=DEFAULT_FADE_MS; enforceMinBlock();
    inScrub=false; rightLong=false; leftLong=false;
    if(rockerOn){ hoState=HO_R2J_ON; hoT_ms=0; hoLastMs=now; }
  }

  bool rainbowActive = (hoState!=HO_IDLE) || (brState==BR_ON_ANIM)||(brState==BR_HOLD)||(brState==BR_OFF_ANIM) || keyOn;

  if(!rainbowActive){ if(eL==2) pushSeqChar('l'); if(eR==2) pushSeqChar('r'); }

  if(hoState!=HO_IDLE){
    uint32_t hoDt=now-hoLastMs; hoLastMs=now; hoT_ms+=hoDt;

    uint8_t jr=255,jg=255,jb=255;
    if(g_mode==MODE_JUMBO){
      bool seqActive=rockerOn && toggleOn;
      static bool prev=false;
      if(!prev && seqActive) colorSeq.onActiveRising(now);
      else if(prev && !seqActive) colorSeq.onActiveFalling(now);
      prev=seqActive; colorSeq.update(now,g_speedScale); colorSeq.get(jr,jg,jb);
    }else{
      uint8_t rb,gb,bb;
      if(bridge.update(now,g_speedScale,rb,gb,bb)){ jr=rb;jg=gb;jb=bb; }
      else if(!bridge.active && toggleOn){ exitClassicToJumboAtRed(now); colorSeq.get(jr,jg,jb); }
      else{ classicColorFromKnob(g_lastKnobNorm,jr,jg,jb); }
    }

    const uint8_t f=(uint8_t)(g_fader*255.f+0.5f);
    bool offPhase=(hoState==HO_J2R_OFF);
    for(int i=0;i<LED_COUNT;i++){
      float br=ledBrightHandoff((uint8_t)i, offPhase);
      uint8_t R=(uint8_t)((uint16_t)((uint8_t)(jr*br+0.5f))*f/255);
      uint8_t G=(uint8_t)((uint16_t)((uint8_t)(jg*br+0.5f))*f/255);
      uint8_t B=(uint8_t)((uint16_t)((uint8_t)(jb*br+0.5f))*f/255);
      strip.setPixelColor(i,R,G,B);
      g_lastShownR[i]=R; g_lastShownG[i]=G; g_lastShownB[i]=B;
    }
    strip.show();

    uint32_t hoMax=(uint32_t)STAG_CENTER8_MS(0)+30;
    if(hoT_ms>=hoMax){
      if(hoState==HO_J2R_OFF){ hoState=HO_IDLE; if(keyOn){ brState=BR_ON_ANIM; brT_ms=0; phaseMs=((LED_COUNT-1)*STAG_MS)%loopMs(); } }
      else                    { hoState=HO_IDLE; }
    }

    if (!rainbowActive){
      Dots::updateAndOverlay(now, /*allowInput=*/true, eL, eR, inLeft.pressed());
    }
    return;
  }

  if(rainbowActive){
    bool leftDown=inLeft.pressed(), rightDown=inRight.pressed(), bothDown=leftDown && rightDown;
    if((eR==2)){ rightPressStart=now; rightLong=false; }
    if((eL==2)){  leftPressStart=now;  leftLong=false; }

    if((brState==BR_ON_ANIM || brState==BR_HOLD) && bothDown && !inScrub){ inScrub=true; scrubPrev=analogRead(PIN_SPEED_KNOB)/1023.f; rightLong=leftLong=false; }
    else if((!(brState==BR_ON_ANIM || brState==BR_HOLD) || !bothDown) && inScrub){ inScrub=false; }

    if(brState==BR_ON_ANIM || brState==BR_HOLD){
      if(leftDown && !rightDown){
        if(!leftLong && (now-leftPressStart>=LONG_HOLD_MS)) leftLong=true;
        if(leftLong){ uint16_t step=(uint16_t)((float)dt*0.35f);
          HOLD_MS=(HOLD_MS>step)?(uint16_t)(HOLD_MS-step):0; FADE_MS=(FADE_MS>step)?(uint16_t)(FADE_MS-step):0; enforceMinBlock(); }
      }
      if(rightDown && !leftDown){
        if(!rightLong && (now-rightPressStart>=LONG_HOLD_MS)) rightLong=true;
        if(rightLong){ uint16_t step=(uint16_t)((float)dt*0.35f);
          HOLD_MS=addClamped(HOLD_MS,step,MAX_TIME_MS); FADE_MS=addClamped(FADE_MS,step,MAX_TIME_MS); enforceMinBlock(); }
      }
    } else { rightLong=false; leftLong=false; }

    if(!bothDown){
      if((eR==1) && !rightLong){
        if(toggleOn){ STAG_MS = clampU16((uint16_t)(STAG_MS+20), STAG_MIN_MS, STAG_MAX_MS); }
        else{ HOLD_MS=addClamped(HOLD_MS,10,MAX_TIME_MS); FADE_MS=addClamped(FADE_MS,10,MAX_TIME_MS); enforceMinBlock(); }
      }
      if((eL==1) && !leftLong ){
        if(toggleOn){ STAG_MS = clampU16((uint16_t)(STAG_MS>=20?STAG_MS-20:0), STAG_MIN_MS, STAG_MAX_MS); }
        else{ HOLD_MS=(HOLD_MS>=10)?(uint16_t)(HOLD_MS-10):0; FADE_MS=(FADE_MS>=10)?(uint16_t)(FADE_MS-10):0; enforceMinBlock(); }
      }
    }else{
      if(eR==1) rightLong=false;
      if(eL==1) leftLong=false;
    }

    uint32_t LOOP2=loopMs();
    if(inScrub){
      float raw=analogRead(PIN_SPEED_KNOB)/1023.f;
      float delta=raw-scrubPrev;
      if(fabsf(delta)>SCRUB_DELTA_DB){
        int32_t dMs=(int32_t)(delta*(float)LOOP2*SCRUB_GAIN);
        phaseMs=(uint32_t)wrapLoop((int32_t)phaseMs + dMs, (int32_t)LOOP2);
        scrubPrev=raw;
      }
    }else if(brState==BR_ON_ANIM || brState==BR_HOLD){
      float scale = toggleOn ? (-2.0f*g_speedScale) : (g_speedScale);
      int32_t adv=(int32_t)lrintf((float)dt*scale);
      phaseMs=(uint32_t)wrapLoop((int32_t)phaseMs + adv, (int32_t)LOOP2);
    }

    const uint8_t f2=(uint8_t)(g_fader*255.f+0.5f);
    for(int i=0;i<LED_COUNT;i++){
      int32_t tLocal = wrapLoop((int32_t)phaseMs - (int32_t)i*(int32_t)STAG_MS, (int32_t)LOOP2);
      KR::RGB c = KR::colorAt((uint32_t)tLocal, HOLD_MS, FADE_MS);
      float br  = ledBrightAtRainbow((uint8_t)i);
      uint8_t R=(uint8_t)((uint16_t)((uint8_t)(c.r*br+0.5f))*f2/255);
      uint8_t G=(uint8_t)((uint16_t)((uint8_t)(c.g*br+0.5f))*f2/255);
      uint8_t B=(uint8_t)((uint16_t)((uint8_t)(c.b*br+0.5f))*f2/255);
      strip.setPixelColor(i,R,G,B);
      g_lastShownR[i]=R; g_lastShownG[i]=G; g_lastShownB[i]=B;
    }
    strip.show();
    return;
  }

  // ===== Jumbo / Classic =====
  g_wantOn=rockerOn;
  if(g_seqMatched){ if(!toggleOn) enterClassic(now); g_seqMatched=false; }
  if(g_mode==MODE_CLASSIC && eT==2){
    uint8_t rc,gc,bb; classicColorFromKnob(g_lastKnobNorm,rc,gc,bb); bridge.start(rc,gc,bb,now);
  }

  { uint32_t d=now-g_lastMsBright; g_lastMsBright=now; float ds=d*g_speedScale;
    float target=g_wantOn ? (float)SEQ_TOTAL_MS : 0.0f;
    if(g_progressMs<target){ g_progressMs+=ds; if(g_progressMs>target) g_progressMs=target; }
    else if(g_progressMs>target){ g_progressMs-=ds; if(g_progressMs<target) g_progressMs=target; }
  }
  renderJumboBright(g_progressMs);

  uint8_t r=255,g=255,b=255;
  if(g_mode==MODE_CLASSIC){
    uint8_t rb,gb,bb;
    if(bridge.update(now,g_speedScale,rb,gb,bb)){ r=rb; g=gb; b=bb; }
    else if(!bridge.active && toggleOn){ exitClassicToJumboAtRed(now); colorSeq.get(r,g,b); }
    else{ classicColorFromKnob(g_lastKnobNorm,r,g,b); }
  }else{
    bool seqActive=rockerOn && toggleOn;
    static bool prev=false;
    if(!prev && seqActive) colorSeq.onActiveRising(now);
    else if(prev && !seqActive) colorSeq.onActiveFalling(now);
    prev=seqActive; colorSeq.update(now,g_speedScale); colorSeq.get(r,g,b);
  }

  if (g_mode==MODE_JUMBO){ g_lastJumboR=r; g_lastJumboG=g; g_lastJumboB=b; }

  const uint8_t f3=(uint8_t)(g_fader*255.f+0.5f);
  for(int i=0;i<LED_COUNT;i++){
    uint16_t vF=(uint16_t)g_bright[i]*f3/255;
    uint8_t R=(uint8_t)((uint16_t)r*vF/255);
    uint8_t G=(uint8_t)((uint16_t)g*vF/255);
    uint8_t B=(uint8_t)((uint16_t)b*vF/255);
    strip.setPixelColor(i,R,G,B);
    g_lastShownR[i]=R; g_lastShownG[i]=G; g_lastShownB[i]=B;
  }
  Dots::updateAndOverlay(now, /*allowInput=*/true, eL, eR, inLeft.pressed());
}
