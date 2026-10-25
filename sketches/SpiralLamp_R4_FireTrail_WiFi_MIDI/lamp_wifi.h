// WiFi station, mobile page, and JSON API for SpiralLamp_R4_FireTrail_WiFi_MIDI.
// Included once from the sketch, after the lamp (so Lava / NewWave exist).
//
// Precedence
//   Brightness and speed are independent. A web value holds until that pot
//   moves PHYS_ANALOG_TAKEOVER counts from the reading captured when the
//   web value was applied. Then that pot wins and the other web holds stay.
//   Mode is one group (rocker, toggle, rainbow key, wave, chaos). Any
//   debounced change on that group drops the web mode and the panel's
//   switches are used together, so the lamp matches the box.
//   Lava top/bottom colours apply immediately and do not lock D4/D8.
//   The next press cycles from the colour just set (same order as Lava).
//   Fire trail has no switch. The web value sticks. LAVA_FIRE_TRAIL is boot.
//
// mDNS (sketches/common/mdns_responder.h) answers spirallamp.local and
// spiral-lamp.local on UDP 5353. DHCP hostname is spirallamp, so
// http://spirallamp/ works on routers that publish that name.
// spiral-lamp without .local is not a second DHCP name.
// The AppleMIDI session is still not advertised over Bonjour.

#ifndef LAMP_WIFI_H
#define LAMP_WIFI_H

#include <WiFiS3.h>
#include <Modem.h>
#include "mdns_responder.h"
#include <stdio.h>
#include <string.h>

static const uint32_t WIFI_ACCEPT_MS = 30;
static const uint32_t WIFI_STATUS_ONLINE_MS = 1000;
static const uint32_t WIFI_STATUS_JOIN_MS = 250;
static const uint32_t WIFI_RETRY_MS = 22000;
static const uint32_t WIFI_IP_POLL_MS = 3000;
static const uint16_t HTTP_READ_MAX = 64;
static const uint16_t HTTP_WRITE_MAX = 192;
static const uint32_t HTTP_READ_TIMEOUT_MS = 400;
static const uint32_t MODEM_CMD_BUDGET_MS = 800;

enum WifiPhase : uint8_t { WIFI_OFF = 0, WIFI_JOINING = 1, WIFI_ONLINE = 2 };

static WifiPhase g_wifi = WIFI_OFF;
static WiFiServer g_server(80);
static WiFiClient g_client;
enum HttpPhase : uint8_t { PH_IDLE = 0, PH_READ = 1, PH_WRITE = 2 };
static HttpPhase g_http = PH_IDLE;
static char g_req[480];
static uint16_t g_reqLen = 0;
static uint32_t g_clientMs = 0;
static char g_hdr[240];
static uint16_t g_hdrLen = 0;
static uint16_t g_hdrOff = 0;
static const char* g_body = 0;
static uint16_t g_bodyLen = 0;
static uint16_t g_bodyOff = 0;
static char g_json[896];
static uint8_t g_fail = 0;
static uint16_t g_pageLen = 0;
static uint8_t g_ip[4] = {0, 0, 0, 0};
static int16_t g_rssi = 0;
static uint32_t g_lastStatus = 0;
static uint32_t g_lastAccept = 0;
static uint32_t g_lastRssi = 0;
static uint32_t g_lastIpPoll = 0;
static uint32_t g_joinSince = 0;
static uint8_t g_serverUp = 0;

// The delimiter token must not appear in the page.
static const char PAGE[] = R"PAGE(<!DOCTYPE html>
<html lang=en><head>
<meta charset=utf-8>
<meta name=viewport content="width=device-width,initial-scale=1">
<title>Spiral Lamp</title>
<style>
body{margin:0;background:#14120f;color:#f3efe6;font:18px/1.35 system-ui,sans-serif}
main{max-width:28rem;margin:0 auto;padding:1rem 1rem 2.5rem}
h1{font-size:1.45rem;margin:0 0 .2rem}
label{display:block;margin:.85rem 0 .25rem;font-size:.85rem;color:#c8bba6}
select,button{font:inherit;background:#2a241c;color:#f3efe6;border:1px solid #5c4d38;border-radius:.6rem;padding:.7rem;width:100%;box-sizing:border-box}
button{margin:.3rem 0;min-height:2.9rem}
.row{display:flex;gap:.5rem}
.row button{flex:1;width:auto}
input[type=range]{width:100%;height:2rem;accent-color:#e07030}
pre{background:#0c0b09;border-radius:.6rem;padding:.7rem;white-space:pre-wrap;font:13px/1.45 ui-monospace,monospace}
.note{color:#b5a892;font-size:.85rem}
</style></head><body><main>
<h1>Spiral Lamp</h1>
<p class=note id=link>connecting</p>
<label for=mode>Mode</label>
<select id=mode>
<option value=off>Off</option>
<option value=jumbo>Jumbo</option>
<option value=classic>Classic</option>
<option value=rainbow>Rainbow</option>
<option value=wave>Wave</option>
<option value=wave-mod>Wave modifier</option>
<option value=wave-rainbow>Wave rainbow</option>
<option value=wave-epic>Wave epic</option>
<option value=secret>Secret</option>
<option value=lava>Lava</option>
<option value=music>Music</option>
</select>
<label for=br>Brightness <span id=brv></span></label>
<input id=br type=range min=0 max=100 value=60>
<label for=sp>Speed (knob position) <span id=spv></span></label>
<input id=sp type=range min=0 max=100 value=50>
<div class=row>
<button type=button id=top>Top colour</button>
<button type=button id=bot>Bottom colour</button>
</div>
<div class=row>
<button type=button id=chaos>Chaos</button>
<button type=button id=fire>Fire trail</button>
</div>
<pre id=st>loading</pre>
<p class=note>Web brightness holds until the A0 fader moves, and web speed until the A1 knob moves (about 4% of the travel). Web mode holds until the rocker, toggle, rainbow key, wave switch, or chaos mushroom moves; then the whole mode follows the panel. Lava colours apply at once; the white and red buttons keep cycling from there. Fire trail has no switch, so the page keeps it. Music is the toggle on with the rocker, rainbow key, wave switch, and chaos mushroom all off. A note can select Music from Off when the page is not holding a mode; after about 10 seconds without a note, Music keeps a dim idle pulse.</p>
<script>
var mode=document.getElementById('mode'),br=document.getElementById('br'),sp=document.getElementById('sp');
var st=document.getElementById('st'),brv=document.getElementById('brv'),spv=document.getElementById('spv');
var link=document.getElementById('link'),topb=document.getElementById('top'),botb=document.getElementById('bot');
var chaosb=document.getElementById('chaos'),fireb=document.getElementById('fire');
var pending='',busy=0;
function send(q){pending=q;if(!busy)pump();}
function pump(){
  if(!pending)return;
  var q=pending;pending='';busy=1;
  fetch('/api/set?'+q).then(function(r){return r.json();}).then(show).catch(function(){}).then(function(){busy=0;pump();});
}
function show(s){
  if(!s||!s.mode)return;
  var ae=document.activeElement;
  if(ae!==mode)mode.value=s.mode;
  if(ae!==br)br.value=s.brightness;
  if(ae!==sp)sp.value=s.speed;
  brv.textContent=br.value+'%';
  spv.textContent=sp.value;
  topb.textContent='Top '+s.topName;
  botb.textContent='Bottom '+s.bottomName;
  chaosb.textContent=s.chaos?'Chaos on':'Chaos off';
  fireb.textContent=s.fireTrail?'Fire trail on':'Fire trail off';
  link.textContent=s.wifi+'  '+s.ip+'   '+s.ledCount+' LEDs';
  var last = (s.midiLast < 0) ? 'none' : (s.midiLastName+' ('+s.midiLast+')');
  st.textContent=
    'mode '+s.mode+' ('+s.modeOwner+')\n'+
    'brightness '+s.brightness+' ('+s.brightnessOwner+')\n'+
    'speed '+s.speed+' ('+s.speedOwner+')\n'+
    'chaos '+(s.chaos?'on':'off')+'   fire '+(s.fireTrail?'on':'off')+'\n'+
    'top '+s.topName+' ('+s.top+')   bottom '+s.bottomName+' ('+s.bottom+')\n'+
    'wifi '+s.wifi+'   rssi '+s.rssi+'\n'+
    'midi '+s.midi+'  '+(s.midiSession||'-')+'\n'+
    'last note '+last+'\n'+
    'cap '+s.brightnessCap+'   takeover '+s.analogTakeover+'/1023';
}
mode.onchange=function(){send('mode='+encodeURIComponent(mode.value));};
function slide(el,key){
  var t=0,last=0;
  el.oninput=function(){
    if(el===br)brv.textContent=br.value+'%'; else spv.textContent=sp.value;
    var now=Date.now();
    if(now-last>200){last=now;send(key+'='+el.value);}
    clearTimeout(t);
    t=setTimeout(function(){last=Date.now();send(key+'='+el.value);},220);
  };
  el.onchange=function(){clearTimeout(t);send(key+'='+el.value);};
}
slide(br,'brightness');
slide(sp,'speed');
topb.onclick=function(){send('top=next');};
botb.onclick=function(){send('bottom=next');};
chaosb.onclick=function(){send('chaos=toggle');};
fireb.onclick=function(){send('fire=toggle');};
function poll(){
  if(busy||pending)return;
  fetch('/api/state').then(function(r){return r.json();}).then(show).catch(function(){});
}
setInterval(poll,1000);
poll();
</script>
</main></body></html>
)PAGE";

static const char* modeName(uint8_t m){
  static const char* const N[] = {
    "off", "jumbo", "classic", "rainbow", "wave",
    "wave-mod", "wave-rainbow", "wave-epic", "secret", "lava", "music"
  };
  if (m > WM_MUSIC) return "off";
  return N[m];
}
static const char* lavaName(uint8_t i){
  static const char* const N[] = {
    "red", "purple", "blue", "green", "yellow", "orange", "off"
  };
  return N[i % 7];
}
static bool modeFromName(const char* s, uint8_t* out){
  for (uint8_t i = 0; i <= WM_MUSIC; i++){
    if (!strcmp(s, modeName(i))){ *out = i; return true; }
  }
  return false;
}
static bool parseU100(const char* v, int* out){
  if (!v[0]) return false;
  int n = 0;
  for (const char* p = v; *p; p++){
    if (*p < '0' || *p > '9') return false;
    n = n * 10 + (*p - '0');
    if (n > 100) return false;
  }
  *out = n;
  return true;
}
static bool parseDigit0to6(const char* v, int* out){
  if (v[0] >= '0' && v[0] <= '6' && v[1] == 0){ *out = v[0] - '0'; return true; }
  return false;
}
static int hexVal(char c){
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

static void apiMode(uint8_t m){
  uint8_t cur = webOwnsMode ? webMode : g_reportedMode;
  if (m == WM_LAVA){
    if (cur != WM_LAVA) webModeBeforeLava = cur;
  } else {
    webModeBeforeLava = m;
  }
  webMode = m;
  webOwnsMode = 1;
}
static void apiChaos(int v){
  uint8_t cur = webOwnsMode ? webMode : g_reportedMode;
  if (v == 2) v = (cur == WM_LAVA) ? 0 : 1;
  if (v){
    if (cur != WM_LAVA) webModeBeforeLava = cur;
    webMode = WM_LAVA;
  } else {
    uint8_t back = webModeBeforeLava;
    if (back == WM_LAVA) back = WM_JUMBO;
    webMode = back;
  }
  webOwnsMode = 1;
}
static void apiBrightness(int pct){
  webBrightPct = pct;
  webFader01 = (float)pct / 100.0f;
  webOwnsFader = 1;
#if HAVE_FADER
  faderAnchor = faderPhysical();
#endif
  g_fader = webFader01;
}
static void apiSpeed(int pct){
  webSpeedPct = pct;
  float n = (float)pct / 100.0f;
  int raw = (int)(n * 1023.0f + 0.5f);
  if (raw < 0) raw = 0;
  if (raw > 1023) raw = 1023;
  webKnobRaw = raw;
  webOwnsKnob = 1;
#if HAVE_SPEED_KNOB
  knobAnchor = knobPhysical();
#endif
  g_lastKnobNorm = n;
  g_speedScale = mapNormToScale(n);
  Lava::snapKnob(n);
  NewWave::snapKnob(n);
}
static bool applyOne(const char* key, const char* val, const char** err){
  if (!strcmp(key, "mode")){
    uint8_t m;
    if (!modeFromName(val, &m)){ *err = "mode"; return false; }
    apiMode(m);
    return true;
  }
  if (!strcmp(key, "brightness")){
    int n;
    if (!parseU100(val, &n)){ *err = "brightness"; return false; }
    apiBrightness(n);
    return true;
  }
  if (!strcmp(key, "speed")){
    int n;
    if (!parseU100(val, &n)){ *err = "speed"; return false; }
    apiSpeed(n);
    return true;
  }
  if (!strcmp(key, "chaos")){
    int v;
    if (!strcmp(val, "toggle")) v = 2;
    else if (!strcmp(val, "0")) v = 0;
    else if (!strcmp(val, "1")) v = 1;
    else { *err = "chaos"; return false; }
    apiChaos(v);
    return true;
  }
  if (!strcmp(key, "fire")){
    if (!strcmp(val, "toggle")) g_fireTrail = g_fireTrail ? 0 : 1;
    else if (!strcmp(val, "0")) g_fireTrail = 0;
    else if (!strcmp(val, "1")) g_fireTrail = 1;
    else { *err = "fire"; return false; }
    return true;
  }
  if (!strcmp(key, "top") || !strcmp(key, "bottom")){
    uint8_t ball = (key[0] == 'b') ? 0 : 1;
    if (!strcmp(val, "next")) Lava::cycleColor(ball);
    else {
      int n;
      if (!parseDigit0to6(val, &n)){
        *err = (key[0] == 'b') ? "bottom" : "top";
        return false;
      }
      Lava::setColorIndex(ball, (uint8_t)n);
    }
    return true;
  }
  *err = "key";
  return false;
}
static void applyQuery(const char* q, const char** err){
  while (*q){
    char key[16];
    char val[20];
    uint8_t ki = 0, vi = 0;
    bool trunc = false;
    while (*q && *q != '=' && *q != '&'){
      if (ki + 1 < sizeof key) key[ki++] = *q;
      else trunc = true;
      q++;
    }
    key[ki] = 0;
    if (*q == '=') q++;
    while (*q && *q != '&'){
      char c = *q++;
      if (c == '+') c = ' ';
      else if (c == '%' && q[0] && q[1]){
        int hi = hexVal(q[0]);
        int lo = hexVal(q[1]);
        if (hi >= 0 && lo >= 0){
          c = (char)((hi << 4) | lo);
          q += 2;
        }
      }
      if (vi + 1 < sizeof val) val[vi++] = c;
      else trunc = true;
    }
    val[vi] = 0;
    if (*q == '&') q++;
    if (!key[0]) continue;
    if (trunc){ *err = "key"; return; }
    if (!applyOne(key, val, err)) return;
  }
}

static int buildState(char* b, size_t n, bool ok, const char* err){
  uint8_t mode = webOwnsMode ? webMode : g_reportedMode;
  if (mode > WM_MUSIC) mode = WM_OFF;
  int br = webOwnsFader ? webBrightPct : (int)(g_fader * 100.0f + 0.5f);
  if (br < 0) br = 0;
  if (br > 100) br = 100;
  int sp = webOwnsKnob ? webSpeedPct : (int)(g_lastKnobNorm * 100.0f + 0.5f);
  if (sp < 0) sp = 0;
  if (sp > 100) sp = 100;
  uint8_t top = Lava::colorIndex(1);
  uint8_t bot = Lava::colorIndex(0);
  const char* wifi = (g_wifi == WIFI_ONLINE) ? "connected" :
                     (g_wifi == WIFI_JOINING) ? "joining" : "off";
  return snprintf(b, n,
    "{\"ok\":%s,\"error\":\"%s\",\"mode\":\"%s\",\"modeOwner\":\"%s\","
    "\"brightness\":%d,\"brightnessOwner\":\"%s\","
    "\"speed\":%d,\"speedOwner\":\"%s\","
    "\"chaos\":%s,\"fireTrail\":%s,"
    "\"top\":%u,\"topName\":\"%s\",\"bottom\":%u,\"bottomName\":\"%s\","
    "\"wifi\":\"%s\",\"ip\":\"%u.%u.%u.%u\",\"rssi\":%d,"
    "\"ledCount\":%u,\"brightnessCap\":%u,\"analogTakeover\":%u,"
    "\"midi\":\"%s\",\"midiSession\":\"%s\",\"midiLast\":%d,\"midiLastName\":\"%s\"}",
    ok ? "true" : "false",
    err ? err : "",
    modeName(mode),
    webOwnsMode ? "web" : "physical",
    br,
    webOwnsFader ? "web" : "physical",
    sp,
    webOwnsKnob ? "web" : "physical",
    (mode == WM_LAVA) ? "true" : "false",
    g_fireTrail ? "true" : "false",
    (unsigned)top, lavaName(top), (unsigned)bot, lavaName(bot),
    wifi,
    (unsigned)g_ip[0], (unsigned)g_ip[1], (unsigned)g_ip[2], (unsigned)g_ip[3],
    (int)g_rssi,
    (unsigned)LED_COUNT,
    (unsigned)BRIGHTNESS_CAP,
    (unsigned)PHYS_ANALOG_TAKEOVER,
    Music::linkState(),
    Music::sessionName(),
    Music::lastNote(),
    Music::lastNoteName());
}

static void httpBegin(int code, const char* reason, const char* type, const char* body, uint16_t len){
  int n = snprintf(g_hdr, sizeof g_hdr,
    "HTTP/1.1 %d %s\r\n"
    "Content-Type: %s\r\n"
    "Content-Length: %u\r\n"
    "Connection: close\r\n"
    "Cache-Control: no-store\r\n"
    "Access-Control-Allow-Origin: *\r\n"
    "\r\n",
    code, reason, type, (unsigned)len);
  if (n < 0 || (size_t)n >= sizeof g_hdr){
    g_hdrLen = 0;
    g_body = 0;
    g_bodyLen = 0;
  } else {
    g_hdrLen = (uint16_t)n;
    g_body = body;
    g_bodyLen = len;
  }
  g_hdrOff = 0;
  g_bodyOff = 0;
  g_fail = 0;
  g_http = PH_WRITE;
}
static void httpJson(bool ok, const char* err){
  int n = buildState(g_json, sizeof g_json, ok, err);
  if (n < 0 || (size_t)n >= sizeof g_json){
    n = snprintf(g_json, sizeof g_json, "{\"ok\":false,\"error\":\"full\"}");
    ok = false;
  }
  if (n < 0) n = 0;
  httpBegin(ok ? 200 : 400, ok ? "OK" : "Bad Request", "application/json", g_json, (uint16_t)n);
}
static void httpHandle(){
  if (strncmp(g_req, "GET ", 4) != 0){
    httpJson(false, "method");
    return;
  }
  char path[48];
  char query[180];
  const char* p = g_req + 4;
  const char* end = strpbrk(p, " \r\n");
  if (!end) end = p + strlen(p);
  const char* qmark = 0;
  for (const char* s = p; s < end; s++) if (*s == '?'){ qmark = s; break; }
  size_t plen = (size_t)((qmark ? qmark : end) - p);
  if (plen >= sizeof path) plen = sizeof path - 1;
  memcpy(path, p, plen);
  path[plen] = 0;
  query[0] = 0;
  if (qmark){
    size_t qlen = (size_t)(end - (qmark + 1));
    if (qlen >= sizeof query) qlen = sizeof query - 1;
    memcpy(query, qmark + 1, qlen);
    query[qlen] = 0;
  }
  if (!strcmp(path, "/") || !strcmp(path, "/index.html")){
    httpBegin(200, "OK", "text/html; charset=utf-8", PAGE, g_pageLen);
    return;
  }
  if (!strcmp(path, "/favicon.ico")){
    httpBegin(204, "No Content", "text/plain", "", 0);
    return;
  }
  if (!strcmp(path, "/api/state")){
    httpJson(true, 0);
    return;
  }
  if (!strcmp(path, "/api/set")){
    const char* err = 0;
    applyQuery(query, &err);
    httpJson(err == 0, err);
    return;
  }
  static const char MISS[] = "not found\n";
  httpBegin(404, "Not Found", "text/plain; charset=utf-8", MISS, (uint16_t)(sizeof MISS - 1));
}
static void httpRead(uint32_t now){
  int avail = g_client.available();
  if (avail > 0){
    if (avail > (int)HTTP_READ_MAX) avail = (int)HTTP_READ_MAX;
    uint16_t room = (uint16_t)((sizeof g_req) - 1 - g_reqLen);
    if ((uint16_t)avail > room) avail = (int)room;
    if (avail > 0){
      uint8_t tmp[HTTP_READ_MAX];
      int n = g_client.read(tmp, (size_t)avail);
      if (n > 0){
        memcpy(g_req + g_reqLen, tmp, (size_t)n);
        g_reqLen = (uint16_t)(g_reqLen + n);
        g_req[g_reqLen] = 0;
      }
    }
  }
  bool full = g_reqLen >= (sizeof g_req) - 1;
  bool hdr = g_reqLen > 0 && strstr(g_req, "\r\n\r\n") != 0;
  bool timed = (uint32_t)(now - g_clientMs) >= HTTP_READ_TIMEOUT_MS;
  if (hdr || full || timed){
    if (g_reqLen == 0){
      g_client.stop();
      g_http = PH_IDLE;
      return;
    }
    httpHandle();
  }
}
static void httpWrite(){
  if (g_hdrOff < g_hdrLen){
    uint16_t n = (uint16_t)(g_hdrLen - g_hdrOff);
    if (n > HTTP_WRITE_MAX) n = HTTP_WRITE_MAX;
    size_t w = g_client.write((const uint8_t*)g_hdr + g_hdrOff, n);
    if (w == 0){
      if (++g_fail > 5){ g_client.stop(); g_http = PH_IDLE; }
      return;
    }
    g_fail = 0;
    g_hdrOff = (uint16_t)(g_hdrOff + w);
    return;
  }
  if (g_body && g_bodyOff < g_bodyLen){
    uint16_t n = (uint16_t)(g_bodyLen - g_bodyOff);
    if (n > HTTP_WRITE_MAX) n = HTTP_WRITE_MAX;
    size_t w = g_client.write((const uint8_t*)g_body + g_bodyOff, n);
    if (w == 0){
      if (++g_fail > 5){ g_client.stop(); g_http = PH_IDLE; }
      return;
    }
    g_fail = 0;
    g_bodyOff = (uint16_t)(g_bodyOff + w);
    return;
  }
  g_client.stop();
  g_http = PH_IDLE;
}

static void wifiBegin(){
  if (WIFI_PASS[0] == 0) WiFi.begin(WIFI_SSID);
  else WiFi.begin(WIFI_SSID, WIFI_PASS);
  g_joinSince = millis();
}
// Core 1.6.0 reports WL_CONNECTED when the radio associates. DHCP may still
// be pending, and localIP() is 0.0.0.0 until it finishes (also on a modem
// timeout). Do not treat that as the lamp's address.
static bool ipReady(const IPAddress& ip) {
  if (ip[0] == 0 && ip[1] == 0 && ip[2] == 0 && ip[3] == 0) return false;
  if (ip[0] == 0) return false;
  return true;
}
static void rememberIp(const IPAddress& ip) {
  g_ip[0] = ip[0];
  g_ip[1] = ip[1];
  g_ip[2] = ip[2];
  g_ip[3] = ip[3];
}
static void noteConnected(){
  g_rssi = (int16_t)WiFi.RSSI();
  g_lastRssi = millis();
  Serial.print(F("WiFi "));
  Serial.print(g_ip[0]); Serial.print('.');
  Serial.print(g_ip[1]); Serial.print('.');
  Serial.print(g_ip[2]); Serial.print('.');
  Serial.println(g_ip[3]);
  Serial.print(F("Open http://"));
  Serial.print(g_ip[0]); Serial.print('.');
  Serial.print(g_ip[1]); Serial.print('.');
  Serial.print(g_ip[2]); Serial.print('.');
  Serial.print(g_ip[3]);
  Serial.println(F("/"));
  Serial.println(F("http://spirallamp.local/  and  http://spiral-lamp.local/"));
  Serial.println(F("DHCP name spirallamp (http://spirallamp/ if the router publishes it)."));
  Music::onWifiUp();
}
static void wifiDrop(uint32_t now){
  Serial.println(F("WiFi dropped"));
  mdnsServiceStop();
  Music::onWifiDown();
  g_wifi = WIFI_JOINING;
  g_server.end();
  g_serverUp = 0;
  if (g_http != PH_IDLE){
    g_client.stop();
    g_http = PH_IDLE;
  }
  // Retry on the next status poll, not in this same modem burst.
  g_joinSince = now - WIFI_RETRY_MS;
}
static void adoptIp(const IPAddress& ip){
  bool changed = ip[0] != g_ip[0] || ip[1] != g_ip[1] || ip[2] != g_ip[2] || ip[3] != g_ip[3];
  if (changed) rememberIp(ip);
  if (g_wifi != WIFI_ONLINE){
    g_wifi = WIFI_ONLINE;
    noteConnected();
    mdnsServiceArm(millis());
  } else if (changed){
    Serial.print(F("WiFi IP "));
    Serial.print(g_ip[0]); Serial.print('.');
    Serial.print(g_ip[1]); Serial.print('.');
    Serial.print(g_ip[2]); Serial.print('.');
    Serial.println(g_ip[3]);
    mdnsServiceArm(millis());
  }
  if (!g_serverUp){
    g_server.begin();
    g_serverUp = g_server ? 1 : 0;
  }
}
static void wifiPollStatus(uint32_t now){
  uint8_t st = WiFi.status();
  if (st == WL_CONNECTED){
    bool refresh = (g_wifi != WIFI_ONLINE) || g_ip[0] == 0 ||
                   (uint32_t)(now - g_lastIpPoll) >= WIFI_IP_POLL_MS;
    if (refresh){
      g_lastIpPoll = now;
      IPAddress ip = WiFi.localIP();
      if (ipReady(ip)) adoptIp(ip);
    }
    if (g_wifi == WIFI_ONLINE && g_ip[0] != 0){
      if ((uint32_t)(now - g_lastRssi) >= 5000){
        g_rssi = (int16_t)WiFi.RSSI();
        g_lastRssi = now;
      }
      if (!g_serverUp){
        g_server.begin();
        g_serverUp = g_server ? 1 : 0;
      }
      return;
    }
  } else if (g_wifi == WIFI_ONLINE){
    wifiDrop(now);
    return;
  }
  // Associated with no address yet still counts as joining. Waiting 22 s
  // before another begin() keeps DHCP from being reset mid-handshake.
  if ((uint32_t)(now - g_joinSince) >= WIFI_RETRY_MS) wifiBegin();
}

static void webSetup(){
  g_pageLen = (uint16_t)strlen(PAGE);
  if (WIFI_SSID[0] == 0){
    g_wifi = WIFI_OFF;
    Serial.println(F("WiFi skipped (WIFI_SSID empty in secrets.h)"));
    return;
  }
  g_wifi = WIFI_JOINING;
  WiFi.setTimeout(0);
  WiFi.status();                 // starts the modem (and its soft-reset)
  modem.timeout(MODEM_CMD_BUDGET_MS);
  WiFi.setHostname("spirallamp");
  wifiBegin();
  Serial.println(F("WiFi joining"));
}
static void webPoll(uint32_t now){
  if (g_wifi == WIFI_OFF) return;
  if (g_http == PH_READ){ httpRead(now); return; }
  if (g_http == PH_WRITE){ httpWrite(); return; }
  if (g_wifi == WIFI_ONLINE && g_serverUp && (uint32_t)(now - g_lastAccept) >= WIFI_ACCEPT_MS){
    g_lastAccept = now;
    WiFiClient incoming = g_server.available();
    if (incoming){
      g_client = incoming;
      g_reqLen = 0;
      g_req[0] = 0;
      g_clientMs = now;
      g_http = PH_READ;
      return;
    }
  }
  if (g_wifi == WIFI_ONLINE && g_ip[0] != 0){
    static const char* const kMdnsNames[] = {
      "spirallamp.local",
      "spiral-lamp.local"
    };
    mdnsServicePoll(now, g_ip, kMdnsNames, 2);
  }
  uint32_t period = (g_wifi == WIFI_ONLINE) ? WIFI_STATUS_ONLINE_MS : WIFI_STATUS_JOIN_MS;
  if ((uint32_t)(now - g_lastStatus) >= period){
    g_lastStatus = now;
    wifiPollStatus(now);
  }
}

static uint8_t deriveMode(bool rocker, bool toggle, bool key, bool wave, bool chaos){
  if (chaos) return WM_LAVA;
  if (wave && toggle && rocker && key) return WM_SECRET;
  if (wave || NewWave::isRunning()){
    if (key && toggle) return WM_WAVE_EPIC;
    if (key) return WM_WAVE_RAINBOW;
    if (toggle) return WM_WAVE_MOD;
    return WM_WAVE;
  }
  if (key) return WM_RAINBOW;
  // Toggle on, everything else off used to leave the strip dark.
  if (!rocker && toggle && !wave) return WM_MUSIC;
  if (brState != BR_OFF || hoState != HO_IDLE) return WM_RAINBOW;
  if (g_mode == MODE_CLASSIC && rocker) return WM_CLASSIC;
  if (rocker) return WM_JUMBO;
  return WM_OFF;
}
static void levelsFor(uint8_t mode, bool& rocker, bool& toggle, bool& key, bool& wave, bool& chaos){
  rocker = toggle = key = wave = chaos = false;
  switch (mode){
    case WM_JUMBO: rocker = true; toggle = true; break;
    case WM_CLASSIC: rocker = true; break;
    case WM_RAINBOW: key = true; break;
    case WM_WAVE: wave = true; break;
    case WM_WAVE_MOD: wave = true; toggle = true; break;
    case WM_WAVE_RAINBOW: wave = true; key = true; break;
    case WM_WAVE_EPIC: wave = true; key = true; toggle = true; break;
    case WM_SECRET: rocker = true; toggle = true; key = true; wave = true; break;
    case WM_LAVA: chaos = true; break;
    case WM_MUSIC: toggle = true; break;
    default: break;
  }
}
// Active-low switches: 2 = turned on, 1 = turned off.
// Chaos active-high: 1 = turned on, 2 = turned off (matches the debouncer).
static uint8_t levelEdge(bool wasOn, bool nowOn, bool activeHigh){
  if (wasOn == nowOn) return 0;
  bool turnedOn = (!wasOn && nowOn);
  if (activeHigh) return turnedOn ? 1 : 2;
  return turnedOn ? 2 : 1;
}
static void resolveControls(uint32_t now,
                            uint8_t eJ, uint8_t eT, uint8_t eK, uint8_t eW, uint8_t eC,
                            bool& rockerOn, bool& toggleOn, bool& keyOn, bool& waveOn, bool& chaos,
                            uint8_t& oT, uint8_t& oK, uint8_t& oW, uint8_t& oC){
  bool physR = inJumbo.pressed();
  bool physT = inToggle.pressed();
  bool physK = inKey.pressed();
  bool physW = inWave.pressed();
  bool physC = chaosOn();
  if (webOwnsMode && (eJ || eT || eK || eW || eC)){
    webOwnsMode = 0;
#if DEBUG_SERIAL
    Serial.println(F("mode taken by panel"));
#endif
  }
  if (webOwnsMode) levelsFor(webMode, rockerOn, toggleOn, keyOn, waveOn, chaos);
  else {
    rockerOn = physR;
    toggleOn = physT;
    keyOn = physK;
    waveOn = physW;
    chaos = physC;
  }
  oT = levelEdge(effToggle, toggleOn, false);
  oK = levelEdge(effKey, keyOn, false);
  oW = levelEdge(effWave, waveOn, false);
#if CHAOS_ACTIVE_HIGH
  oC = levelEdge(effChaos, chaos, true);
#else
  oC = levelEdge(effChaos, chaos, false);
#endif
  if (webOwnsMode){
    if (webMode == WM_CLASSIC && g_mode != MODE_CLASSIC) enterClassic(now);
    else if (webMode == WM_JUMBO && g_mode == MODE_CLASSIC) exitClassicToJumboAtRed(now);
    else if (g_mode == MODE_CLASSIC && webMode != WM_CLASSIC && webMode != WM_LAVA && webMode != WM_SECRET)
      g_mode = MODE_JUMBO;
  }
  effRocker = rockerOn;
  effToggle = toggleOn;
  effKey = keyOn;
  effWave = waveOn;
  effChaos = chaos;
  g_reportedMode = webOwnsMode ? webMode : deriveMode(rockerOn, toggleOn, keyOn, waveOn, chaos);
}
static void webCaptureBoot(){
  effRocker = inJumbo.pressed();
  effToggle = inToggle.pressed();
  effKey = inKey.pressed();
  effWave = inWave.pressed();
  effChaos = chaosOn();
  g_reportedMode = deriveMode(effRocker, effToggle, effKey, effWave, effChaos);
}

#endif
