// Tiny non-blocking mDNS responder for the Uno R4 WiFi (WiFiS3).
//
// The Renesas core has no mDNS library. ArduinoMDNS is an extra dependency,
// answers one hostname, and its begin() path can sit in a retry loop. This
// header answers two names (matrixlamp.local and matrix-lamp.local, or the
// spiral pair), does not block the lamp loop, and has no library to install.
//
// WiFiS3 on core 1.6.0: WiFiUDP::beginMulticast(224.0.0.251, 5353) sends
// AT+UDPBEGINMULTI. The ESP32-S3 coprocessor joins that group with its own
// WiFiUDP::beginMulticast and parsePacket() hands each datagram to the RA4M1.
// beginMulticastPacket() replies to the joined group, 224.0.0.251:5353.
// A query with the QU bit set, or a source port other than 5353, is answered
// unicast with beginPacket(remoteIP, remotePort) instead.
//
// IPv4 only. An A or ANY question gets an A record (cache-flush, TTL 120).
// An AAAA question gets no AAAA; an NSEC record says the name has A and not
// AAAA, so macOS / iOS does not wait out the AAAA timeout.
//
// Each lamp sketch includes "mdns_responder.h" via a symlink to this file.
// The Arduino builder only puts the sketch folder on the include path.
//
// Host test:
//   g++ -std=c++17 -DMDNS_TEST -x c++ sketches/common/mdns_responder.h -o /tmp/mdns_test && /tmp/mdns_test

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <string.h>

static const uint16_t MDNS_PORT = 5353;
static const uint16_t MDNS_TYPE_A = 1;
static const uint16_t MDNS_TYPE_AAAA = 28;
static const uint16_t MDNS_TYPE_NSEC = 47;
static const uint16_t MDNS_TYPE_ANY = 255;
static const uint16_t MDNS_CLASS_IN = 1;
static const uint16_t MDNS_CLASS_FLUSH = 0x8001;
static const uint32_t MDNS_TTL = 120;

struct MdnsHit {
  uint8_t nameIndex;
  uint8_t wantA;
  uint8_t wantAaaa;
  uint8_t qu;
};

struct MdnsQueryInfo {
  uint16_t id;
  uint8_t qu;
  uint8_t hitCount;
  MdnsHit hit[4];
};

static int mdnsLower(int c) {
  if (c >= 'A' && c <= 'Z') return c - 'A' + 'a';
  return c;
}

static int mdnsEq(const char* a, const char* b) {
  while (*a && *b) {
    if (mdnsLower((unsigned char)*a) != mdnsLower((unsigned char)*b)) return 0;
    a++;
    b++;
  }
  return *a == 0 && *b == 0;
}

// Read a DNS name at pkt[off] into out (dotted). *endOff is the first byte
// after the name on the wire (compression pointers count as two bytes).
static int mdnsReadName(const uint8_t* pkt, int len, int off, char* out, int outCap, int* endOff) {
  int hops = 0;
  int n = 0;
  int jumped = 0;
  int pos = off;
  if (outCap > 0) out[0] = 0;
  while (pos >= 0 && pos < len && hops < 12) {
    uint8_t lab = pkt[pos];
    if (lab == 0) {
      if (!jumped && endOff) *endOff = pos + 1;
      if (n < outCap) out[n] = 0;
      else if (outCap > 0) out[outCap - 1] = 0;
      return 1;
    }
    if ((lab & 0xC0) == 0xC0) {
      if (pos + 1 >= len) return 0;
      int ptr = ((lab & 0x3F) << 8) | pkt[pos + 1];
      if (ptr < 0 || ptr >= len || ptr == pos) return 0;
      if (!jumped && endOff) *endOff = pos + 2;
      jumped = 1;
      pos = ptr;
      hops++;
      continue;
    }
    if ((lab & 0xC0) != 0) return 0;
    if (pos + 1 + lab > len) return 0;
    if (n && n + 1 < outCap) out[n++] = '.';
    for (int i = 0; i < lab; i++) {
      char c = (char)pkt[pos + 1 + i];
      if (n + 1 < outCap) out[n++] = c;
    }
    pos += 1 + lab;
    hops++;
  }
  return 0;
}

static int mdnsNameIndex(const char* name, const char* const* names, int nameCount) {
  for (int i = 0; i < nameCount && i < 4; i++) {
    if (names[i] && mdnsEq(name, names[i])) return i;
  }
  return -1;
}

// 1 when at least one of our names is asked as A, AAAA, or ANY.
static int mdnsParseQuery(const uint8_t* pkt, int len, const char* const* names, int nameCount,
                          MdnsQueryInfo* info) {
  memset(info, 0, sizeof *info);
  if (!pkt || len < 12 || !names || nameCount <= 0) return 0;
  uint16_t flags = (uint16_t)((pkt[2] << 8) | pkt[3]);
  if (flags & 0x8000) return 0;             // response, not a query
  if (((flags >> 11) & 0xF) != 0) return 0; // opcode must be standard query
  info->id = (uint16_t)((pkt[0] << 8) | pkt[1]);
  int qd = (pkt[4] << 8) | pkt[5];
  int off = 12;
  char name[80];
  for (int q = 0; q < qd; q++) {
    int next = 0;
    if (!mdnsReadName(pkt, len, off, name, (int)sizeof name, &next)) return info->hitCount > 0;
    if (next + 4 > len) return info->hitCount > 0;
    uint16_t typ = (uint16_t)((pkt[next] << 8) | pkt[next + 1]);
    uint16_t cls = (uint16_t)((pkt[next + 2] << 8) | pkt[next + 3]);
    off = next + 4;
    if ((cls & 0x7FFF) != MDNS_CLASS_IN) continue;
    int idx = mdnsNameIndex(name, names, nameCount);
    if (idx < 0) continue;
    if (typ != MDNS_TYPE_A && typ != MDNS_TYPE_AAAA && typ != MDNS_TYPE_ANY) continue;
    MdnsHit* hit = 0;
    for (int h = 0; h < info->hitCount; h++) {
      if (info->hit[h].nameIndex == (uint8_t)idx) hit = &info->hit[h];
    }
    if (!hit) {
      if (info->hitCount >= 4) continue;
      hit = &info->hit[info->hitCount++];
      hit->nameIndex = (uint8_t)idx;
    }
    if (typ == MDNS_TYPE_A || typ == MDNS_TYPE_ANY) hit->wantA = 1;
    if (typ == MDNS_TYPE_AAAA) hit->wantAaaa = 1;
    if (cls & 0x8000) {
      hit->qu = 1;
      info->qu = 1;
    }
  }
  return info->hitCount > 0;
}

static int mdnsPutU16(uint8_t* buf, int cap, int off, uint16_t v) {
  if (off < 0 || off + 2 > cap) return -1;
  buf[off] = (uint8_t)(v >> 8);
  buf[off + 1] = (uint8_t)(v & 0xFF);
  return off + 2;
}

static int mdnsPutU32(uint8_t* buf, int cap, int off, uint32_t v) {
  if (off < 0 || off + 4 > cap) return -1;
  buf[off] = (uint8_t)(v >> 24);
  buf[off + 1] = (uint8_t)(v >> 16);
  buf[off + 2] = (uint8_t)(v >> 8);
  buf[off + 3] = (uint8_t)(v & 0xFF);
  return off + 4;
}

static int mdnsPutName(uint8_t* buf, int cap, int off, const char* name) {
  if (!name || !name[0]) return -1;
  const char* p = name;
  while (*p) {
    const char* dot = p;
    while (*dot && *dot != '.') dot++;
    int n = (int)(dot - p);
    if (n <= 0 || n > 63) return -1;
    if (off < 0 || off + 1 + n > cap) return -1;
    buf[off++] = (uint8_t)n;
    memcpy(buf + off, p, (size_t)n);
    off += n;
    p = *dot == '.' ? dot + 1 : dot;
  }
  if (off + 1 > cap) return -1;
  buf[off++] = 0;
  return off;
}

static int mdnsPutQuestion(uint8_t* buf, int cap, int off, const char* name, uint16_t typ) {
  off = mdnsPutName(buf, cap, off, name);
  off = mdnsPutU16(buf, cap, off, typ);
  off = mdnsPutU16(buf, cap, off, MDNS_CLASS_IN);
  return off;
}

// NSEC next-name is this name. Bitmap window 0 covers A (bit 1) and NSEC
// (bit 47). AAAA (bit 28) is clear on purpose.
static int mdnsPutNsec(uint8_t* buf, int cap, int off, const char* name) {
  int start = off;
  off = mdnsPutName(buf, cap, off, name);
  off = mdnsPutU16(buf, cap, off, MDNS_TYPE_NSEC);
  off = mdnsPutU16(buf, cap, off, MDNS_CLASS_FLUSH);
  off = mdnsPutU32(buf, cap, off, MDNS_TTL);
  int lenAt = off;
  off = mdnsPutU16(buf, cap, off, 0);
  int rd = off;
  off = mdnsPutName(buf, cap, off, name);
  if (off < 0 || off + 8 > cap) return -1;
  buf[off++] = 0;     // window 0
  buf[off++] = 6;     // bitmap length
  buf[off++] = 0x40;  // type A
  buf[off++] = 0x00;
  buf[off++] = 0x00;
  buf[off++] = 0x00;  // type AAAA would be 0x08 here
  buf[off++] = 0x00;
  buf[off++] = 0x01;  // type NSEC
  int rdlen = off - rd;
  buf[lenAt] = (uint8_t)(rdlen >> 8);
  buf[lenAt + 1] = (uint8_t)(rdlen & 0xFF);
  (void)start;
  return off;
}

static int mdnsPutA(uint8_t* buf, int cap, int off, const char* name, const uint8_t ip[4]) {
  off = mdnsPutName(buf, cap, off, name);
  off = mdnsPutU16(buf, cap, off, MDNS_TYPE_A);
  off = mdnsPutU16(buf, cap, off, MDNS_CLASS_FLUSH);
  off = mdnsPutU32(buf, cap, off, MDNS_TTL);
  off = mdnsPutU16(buf, cap, off, 4);
  if (off < 0 || off + 4 > cap) return -1;
  memcpy(buf + off, ip, 4);
  return off + 4;
}

// legacyUnicast echoes the questions and keeps the query id (port != 5353).
// A multicast or QU response uses id 0 and an empty question section.
// Returns the packet length, or 0 when there is nothing to say.
static int mdnsBuildResponse(const MdnsQueryInfo* info, const char* const* names, int nameCount,
                             const uint8_t ip[4], int legacyUnicast, uint8_t* out, int cap) {
  if (!info || !names || !ip || !out || cap < 12 || info->hitCount == 0) return 0;
  memset(out, 0, 12);
  uint16_t id = legacyUnicast ? info->id : 0;
  out[0] = (uint8_t)(id >> 8);
  out[1] = (uint8_t)(id & 0xFF);
  out[2] = 0x84; // QR + AA, rcode 0
  out[3] = 0x00;
  int off = 12;
  int qd = 0;
  int an = 0;
  int ar = 0;
  if (legacyUnicast) {
    for (int h = 0; h < info->hitCount; h++) {
      const MdnsHit* hit = &info->hit[h];
      if (hit->nameIndex >= nameCount) continue;
      const char* name = names[hit->nameIndex];
      if (hit->wantA) {
        off = mdnsPutQuestion(out, cap, off, name, MDNS_TYPE_A);
        if (off < 0) return 0;
        qd++;
      }
      if (hit->wantAaaa) {
        off = mdnsPutQuestion(out, cap, off, name, MDNS_TYPE_AAAA);
        if (off < 0) return 0;
        qd++;
      }
    }
  }
  for (int h = 0; h < info->hitCount; h++) {
    const MdnsHit* hit = &info->hit[h];
    if (hit->nameIndex >= nameCount) continue;
    const char* name = names[hit->nameIndex];
    if (hit->wantA) {
      off = mdnsPutA(out, cap, off, name, ip);
      if (off < 0) return 0;
      an++;
    } else if (hit->wantAaaa) {
      off = mdnsPutNsec(out, cap, off, name);
      if (off < 0) return 0;
      an++;
    }
  }
  for (int h = 0; h < info->hitCount; h++) {
    const MdnsHit* hit = &info->hit[h];
    if (!hit->wantA) continue;
    if (hit->nameIndex >= nameCount) continue;
    off = mdnsPutNsec(out, cap, off, names[hit->nameIndex]);
    if (off < 0) return 0;
    ar++;
  }
  if (an == 0 && ar == 0) return 0;
  out[4] = (uint8_t)(qd >> 8);
  out[5] = (uint8_t)(qd & 0xFF);
  out[6] = (uint8_t)(an >> 8);
  out[7] = (uint8_t)(an & 0xFF);
  out[8] = 0;
  out[9] = 0;
  out[10] = (uint8_t)(ar >> 8);
  out[11] = (uint8_t)(ar & 0xFF);
  return off;
}

static int mdnsBuildAnnounce(const char* const* names, int nameCount, const uint8_t ip[4],
                             uint8_t* out, int cap) {
  MdnsQueryInfo info;
  memset(&info, 0, sizeof info);
  int n = nameCount;
  if (n > 4) n = 4;
  for (int i = 0; i < n; i++) {
    if (!names[i]) continue;
    info.hit[info.hitCount].nameIndex = (uint8_t)i;
    info.hit[info.hitCount].wantA = 1;
    info.hitCount++;
  }
  return mdnsBuildResponse(&info, names, nameCount, ip, 0, out, cap);
}

#if !defined(MDNS_TEST)

#include <WiFiS3.h>
#include <stdlib.h>
#include <new>

// One packet per poll, a few polls a second. The modem UART is shared with
// the web server, so this does not run while an HTTP read or write is in
// progress (the sketch skips the call). A burst of queries is drained up to
// three datagrams so an A plus AAAA pair is not left in the coprocessor.
//
// WiFiUDP holds a 1461-byte receive buffer. The matrix lamp's globals leave
// little room under the core's 8 KB heap and 1 KB stack, so the socket lives
// on the heap while WiFi is up instead of in BSS.

static const uint32_t MDNS_POLL_MS = 50;
static const uint32_t MDNS_ANN_GAP_MS = 1000;
static const uint32_t MDNS_RETRY_MS = 1000;
static const int MDNS_DRAIN = 3;

static WiFiUDP* g_mdnsUdp = 0;
static uint8_t g_mdnsPkt[512];
static uint8_t g_mdnsUp = 0;
static uint8_t g_mdnsAnnLeft = 0;
static uint32_t g_mdnsAnnDue = 0;
static uint32_t g_mdnsLastPoll = 0;
static uint32_t g_mdnsRetryDue = 0;

static WiFiUDP* mdnsUdpGet() {
  if (g_mdnsUdp) return g_mdnsUdp;
  void* mem = malloc(sizeof(WiFiUDP));
  if (!mem) return 0;
  g_mdnsUdp = new (mem) WiFiUDP();
  return g_mdnsUdp;
}

static void mdnsServiceStop() {
  if (g_mdnsUdp) {
    if (g_mdnsUp) g_mdnsUdp->stop();
    g_mdnsUdp->~WiFiUDP();
    free(g_mdnsUdp);
    g_mdnsUdp = 0;
  }
  g_mdnsUp = 0;
  g_mdnsAnnLeft = 0;
}

static void mdnsServiceArm(uint32_t now) {
  g_mdnsAnnLeft = 2;
  g_mdnsAnnDue = now;
}

static int mdnsSend(const uint8_t* buf, int len, int unicast, IPAddress ip, uint16_t port) {
  if (!g_mdnsUdp || len <= 0) return 0;
  int ok;
  if (unicast) ok = g_mdnsUdp->beginPacket(ip, port);
  else ok = g_mdnsUdp->beginMulticastPacket();
  if (!ok) return 0;
  if (g_mdnsUdp->write(buf, (size_t)len) != (size_t)len) return 0;
  return g_mdnsUdp->endPacket() ? 1 : 0;
}

static void mdnsServicePoll(uint32_t now, const uint8_t ipb[4], const char* const* names, int nameCount) {
  if (!ipb || ipb[0] == 0 || !names || nameCount <= 0) return;
  if (!g_mdnsUp) {
    if (g_mdnsRetryDue != 0 && (int32_t)(now - g_mdnsRetryDue) < 0) return;
    WiFiUDP* udp = mdnsUdpGet();
    if (!udp) {
      g_mdnsRetryDue = now + MDNS_RETRY_MS;
      return;
    }
    IPAddress group(224, 0, 0, 251);
    if (!udp->beginMulticast(group, MDNS_PORT)) {
      g_mdnsRetryDue = now + MDNS_RETRY_MS;
      return;
    }
    g_mdnsUp = 1;
    if (g_mdnsAnnLeft == 0) mdnsServiceArm(now);
  }
  if (g_mdnsAnnLeft && (int32_t)(now - g_mdnsAnnDue) >= 0) {
    int n = mdnsBuildAnnounce(names, nameCount, ipb, g_mdnsPkt, (int)sizeof g_mdnsPkt);
    if (n > 0) mdnsSend(g_mdnsPkt, n, 0, IPAddress(), 0);
    g_mdnsAnnLeft--;
    g_mdnsAnnDue = now + MDNS_ANN_GAP_MS;
  }
  if ((uint32_t)(now - g_mdnsLastPoll) < MDNS_POLL_MS) return;
  g_mdnsLastPoll = now;
  for (int drain = 0; drain < MDNS_DRAIN; drain++) {
    int pkt = g_mdnsUdp->parsePacket();
    if (pkt <= 0) break;
    int want = pkt;
    if (want > (int)sizeof g_mdnsPkt) want = (int)sizeof g_mdnsPkt;
    int got = g_mdnsUdp->read(g_mdnsPkt, (size_t)want);
    if (pkt > want) {
      uint8_t dump[32];
      int left = pkt - want;
      while (left > 0) {
        int n = left > (int)sizeof dump ? (int)sizeof dump : left;
        int r = g_mdnsUdp->read(dump, (size_t)n);
        if (r <= 0) break;
        left -= r;
      }
    }
    if (got < 12) continue;
    MdnsQueryInfo info;
    if (!mdnsParseQuery(g_mdnsPkt, got, names, nameCount, &info)) continue;
    uint16_t rport = g_mdnsUdp->remotePort();
    int legacy = (rport != 0 && rport != MDNS_PORT);
    int unicast = legacy || info.qu;
    IPAddress rip(0, 0, 0, 0);
    if (unicast) {
      rip = g_mdnsUdp->remoteIP();
      if (!rip) unicast = 0;
    }
    int n = mdnsBuildResponse(&info, names, nameCount, ipb, legacy, g_mdnsPkt, (int)sizeof g_mdnsPkt);
    if (n > 0) mdnsSend(g_mdnsPkt, n, unicast, rip, rport);
  }
}

#endif

#if defined(MDNS_TEST)
#include <stdio.h>
#include <stdlib.h>

static int g_mdnsFails = 0;
static void mdnsExpect(int cond, const char* msg) {
  if (!cond) {
    printf("FAIL %s\n", msg);
    g_mdnsFails++;
  } else {
    printf("ok   %s\n", msg);
  }
}

static int mdnsSkipName(const uint8_t* p, int len, int off) {
  int hops = 0;
  while (off >= 0 && off < len && hops < 16) {
    uint8_t lab = p[off];
    if (lab == 0) return off + 1;
    if ((lab & 0xC0) == 0xC0) return (off + 2 <= len) ? off + 2 : -1;
    if ((lab & 0xC0) != 0) return -1;
    if (off + 1 + lab > len) return -1;
    off += 1 + lab;
    hops++;
  }
  return -1;
}

struct MdnsSeen {
  int aCount;
  int aaaaCount;
  int nsecCount;
  int aClassFlush;
  int aTtl120;
  int aIpMatch;
  int nsecHasA;
  int nsecHasAaaa;
  int nsecHasNsec;
  uint16_t id;
  uint16_t flags;
  int qd;
  int an;
  int ar;
};

static int mdnsBitmapHas(const uint8_t* bm, int len, int typ) {
  int off = 0;
  while (off + 2 <= len) {
    int window = bm[off];
    int bl = bm[off + 1];
    off += 2;
    if (bl < 1 || bl > 32 || off + bl > len) return 0;
    int base = window * 256;
    if (typ >= base && typ < base + bl * 8) {
      int bit = typ - base;
      int byte = bit / 8;
      int mask = 0x80 >> (bit % 8);
      return (bm[off + byte] & mask) != 0;
    }
    off += bl;
  }
  return 0;
}

static int mdnsInspect(const uint8_t* pkt, int len, const uint8_t ip[4], MdnsSeen* s) {
  memset(s, 0, sizeof *s);
  if (len < 12) return 0;
  s->id = (uint16_t)((pkt[0] << 8) | pkt[1]);
  s->flags = (uint16_t)((pkt[2] << 8) | pkt[3]);
  s->qd = (pkt[4] << 8) | pkt[5];
  s->an = (pkt[6] << 8) | pkt[7];
  int ns = (pkt[8] << 8) | pkt[9];
  s->ar = (pkt[10] << 8) | pkt[11];
  int off = 12;
  int total = s->qd + s->an + ns + s->ar;
  for (int i = 0; i < s->qd; i++) {
    off = mdnsSkipName(pkt, len, off);
    if (off < 0 || off + 4 > len) return 0;
    off += 4;
  }
  for (int i = 0; i < s->an + ns + s->ar; i++) {
    off = mdnsSkipName(pkt, len, off);
    if (off < 0 || off + 10 > len) return 0;
    uint16_t typ = (uint16_t)((pkt[off] << 8) | pkt[off + 1]);
    uint16_t cls = (uint16_t)((pkt[off + 2] << 8) | pkt[off + 3]);
    uint32_t ttl = ((uint32_t)pkt[off + 4] << 24) | ((uint32_t)pkt[off + 5] << 16) |
                   ((uint32_t)pkt[off + 6] << 8) | pkt[off + 7];
    int rdlen = (pkt[off + 8] << 8) | pkt[off + 9];
    off += 10;
    if (off + rdlen > len) return 0;
    if (typ == MDNS_TYPE_A) {
      s->aCount++;
      if (cls == MDNS_CLASS_FLUSH) s->aClassFlush++;
      if (ttl == MDNS_TTL) s->aTtl120++;
      if (rdlen == 4 && memcmp(pkt + off, ip, 4) == 0) s->aIpMatch++;
    } else if (typ == MDNS_TYPE_AAAA) {
      s->aaaaCount++;
    } else if (typ == MDNS_TYPE_NSEC) {
      s->nsecCount++;
      int noff = mdnsSkipName(pkt + off, rdlen, 0);
      if (noff >= 0 && noff < rdlen) {
        if (mdnsBitmapHas(pkt + off + noff, rdlen - noff, MDNS_TYPE_A)) s->nsecHasA++;
        if (mdnsBitmapHas(pkt + off + noff, rdlen - noff, MDNS_TYPE_AAAA)) s->nsecHasAaaa++;
        if (mdnsBitmapHas(pkt + off + noff, rdlen - noff, MDNS_TYPE_NSEC)) s->nsecHasNsec++;
      }
    }
    off += rdlen;
    (void)total;
  }
  return 1;
}

// macOS / iOS mDNSResponder query: id 0, two questions, second name compressed
// to offset 12, QTYPE A then AAAA, QCLASS IN with the QU bit (0x8001).
static const uint8_t kMacMatrixAandAaaa[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x0a, 'm', 'a', 't', 'r', 'i', 'x', 'l', 'a', 'm', 'p',
  0x05, 'l', 'o', 'c', 'a', 'l', 0x00,
  0x00, 0x01, 0x80, 0x01,
  0xc0, 0x0c, 0x00, 0x1c, 0x80, 0x01
};

static const uint8_t kMacMatrixHyphenAandAaaa[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x0b, 'm', 'a', 't', 'r', 'i', 'x', '-', 'l', 'a', 'm', 'p',
  0x05, 'l', 'o', 'c', 'a', 'l', 0x00,
  0x00, 0x01, 0x80, 0x01,
  0xc0, 0x0c, 0x00, 0x1c, 0x80, 0x01
};

// AAAA only, same compression style macOS uses when the A answer already
// arrived and it is still waiting on AAAA.
static const uint8_t kMacAaaaOnly[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x0a, 'M', 'a', 'T', 'r', 'I', 'x', 'L', 'a', 'M', 'p',
  0x05, 'L', 'O', 'C', 'A', 'L', 0x00,
  0x00, 0x1c, 0x80, 0x01
};

static const char* const kMatrixNames[] = {"matrixlamp.local", "matrix-lamp.local"};
static const char* const kSpiralNames[] = {"spirallamp.local", "spiral-lamp.local"};

static int mdnsPutQuery(uint8_t* buf, int cap, uint16_t id, const char* name, uint16_t typ, uint16_t cls) {
  if (cap < 12) return -1;
  memset(buf, 0, 12);
  buf[0] = (uint8_t)(id >> 8);
  buf[1] = (uint8_t)(id & 0xFF);
  buf[5] = 1;
  int off = mdnsPutName(buf, cap, 12, name);
  off = mdnsPutU16(buf, cap, off, typ);
  off = mdnsPutU16(buf, cap, off, cls);
  return off;
}

static void mdnsCheckA(const MdnsSeen* s, const char* label) {
  char msg[96];
  snprintf(msg, sizeof msg, "%s has one A", label);
  mdnsExpect(s->aCount == 1 && s->aClassFlush == 1 && s->aTtl120 == 1 && s->aIpMatch == 1, msg);
  snprintf(msg, sizeof msg, "%s has no AAAA", label);
  mdnsExpect(s->aaaaCount == 0, msg);
  snprintf(msg, sizeof msg, "%s NSEC lists A not AAAA", label);
  mdnsExpect(s->nsecCount >= 1 && s->nsecHasA >= 1 && s->nsecHasAaaa == 0 && s->nsecHasNsec >= 1, msg);
  snprintf(msg, sizeof msg, "%s is NOERROR authoritative", label);
  mdnsExpect((s->flags & 0x800F) == 0x8000 && (s->flags & 0x0400) != 0, msg);
}

int main() {
  const uint8_t ip[4] = {192, 168, 4, 20};
  uint8_t out[384];
  MdnsQueryInfo info;
  MdnsSeen seen;

  mdnsExpect(sizeof kMacMatrixAandAaaa == 40, "macOS packet size");
  mdnsExpect(kMacMatrixAandAaaa[12] == 0x0a, "matrixlamp label length");
  mdnsExpect(kMacMatrixAandAaaa[30] == 0x00 && kMacMatrixAandAaaa[31] == 0x01,
             "first question is type A");
  mdnsExpect(kMacMatrixAandAaaa[32] == 0x80 && kMacMatrixAandAaaa[33] == 0x01, "QU bit on A question");
  mdnsExpect(kMacMatrixAandAaaa[34] == 0xc0 && kMacMatrixAandAaaa[35] == 0x0c, "AAAA name is compressed");
  mdnsExpect(kMacMatrixAandAaaa[36] == 0x00 && kMacMatrixAandAaaa[37] == 0x1c, "second question is AAAA");

  mdnsExpect(mdnsParseQuery(kMacMatrixAandAaaa, (int)sizeof kMacMatrixAandAaaa, kMatrixNames, 2, &info) == 1,
             "parse macOS matrixlamp A+AAAA");
  mdnsExpect(info.qu == 1 && info.hitCount == 1 && info.hit[0].nameIndex == 0 &&
             info.hit[0].wantA == 1 && info.hit[0].wantAaaa == 1,
             "macOS query wants A and AAAA with QU");
  int n = mdnsBuildResponse(&info, kMatrixNames, 2, ip, 0, out, (int)sizeof out);
  mdnsExpect(n > 12 && mdnsInspect(out, n, ip, &seen), "build macOS A+AAAA reply");
  mdnsCheckA(&seen, "macOS A+AAAA");
  mdnsExpect(seen.id == 0 && seen.qd == 0 && seen.an == 1 && seen.ar >= 1, "QU reply is multicast-shaped");

  mdnsExpect(mdnsParseQuery(kMacMatrixHyphenAandAaaa, (int)sizeof kMacMatrixHyphenAandAaaa,
                            kMatrixNames, 2, &info) == 1,
             "parse macOS matrix-lamp.local");
  mdnsExpect(info.hit[0].nameIndex == 1 && info.hit[0].wantA && info.hit[0].wantAaaa, "hyphen name matched");
  n = mdnsBuildResponse(&info, kMatrixNames, 2, ip, 0, out, (int)sizeof out);
  mdnsExpect(n > 0 && mdnsInspect(out, n, ip, &seen), "build hyphen reply");
  mdnsCheckA(&seen, "matrix-lamp.local");

  mdnsExpect(mdnsParseQuery(kMacAaaaOnly, (int)sizeof kMacAaaaOnly, kMatrixNames, 2, &info) == 1,
             "parse case-insensitive AAAA-only");
  mdnsExpect(info.hit[0].wantA == 0 && info.hit[0].wantAaaa == 1 && info.qu == 1, "AAAA-only flags");
  n = mdnsBuildResponse(&info, kMatrixNames, 2, ip, 0, out, (int)sizeof out);
  mdnsExpect(n > 0 && mdnsInspect(out, n, ip, &seen), "build AAAA-only reply");
  mdnsExpect(seen.aCount == 0 && seen.aaaaCount == 0, "AAAA-only sends no address record");
  mdnsExpect(seen.nsecCount >= 1 && seen.nsecHasA >= 1 && seen.nsecHasAaaa == 0, "AAAA-only NSEC denies AAAA");
  mdnsExpect(seen.an >= 1 && (seen.flags & 0x000F) == 0, "AAAA-only is NOERROR with an answer");

  uint8_t q[128];
  int qn = mdnsPutQuery(q, (int)sizeof q, 0x1a2b, "matrixlamp.local", MDNS_TYPE_A, MDNS_CLASS_IN);
  mdnsExpect(qn > 0 && mdnsParseQuery(q, qn, kMatrixNames, 2, &info), "parse legacy A query");
  mdnsExpect(info.qu == 0 && info.id == 0x1a2b && info.hit[0].wantA == 1, "legacy A has no QU");
  n = mdnsBuildResponse(&info, kMatrixNames, 2, ip, 1, out, (int)sizeof out);
  mdnsExpect(n > 0 && mdnsInspect(out, n, ip, &seen), "build legacy unicast reply");
  mdnsCheckA(&seen, "legacy A");
  mdnsExpect(seen.id == 0x1a2b && seen.qd == 1, "legacy reply echoes id and question");

  qn = mdnsPutQuery(q, (int)sizeof q, 0, "SpiralLamp.Local", MDNS_TYPE_ANY, 0x8001);
  mdnsExpect(qn > 0 && mdnsParseQuery(q, qn, kSpiralNames, 2, &info), "parse ANY for spirallamp.local");
  mdnsExpect(info.hit[0].nameIndex == 0 && info.hit[0].wantA == 1 && info.qu == 1, "ANY counts as A");
  n = mdnsBuildResponse(&info, kSpiralNames, 2, ip, 0, out, (int)sizeof out);
  mdnsExpect(n > 0 && mdnsInspect(out, n, ip, &seen), "build ANY reply");
  mdnsCheckA(&seen, "ANY");

  qn = mdnsPutQuery(q, (int)sizeof q, 0, "spiral-lamp.local", MDNS_TYPE_A, MDNS_CLASS_IN);
  mdnsExpect(mdnsParseQuery(q, qn, kSpiralNames, 2, &info) && info.hit[0].nameIndex == 1,
             "spiral-lamp.local matches");

  qn = mdnsPutQuery(q, (int)sizeof q, 0, "printer.local", MDNS_TYPE_A, MDNS_CLASS_IN);
  mdnsExpect(!mdnsParseQuery(q, qn, kMatrixNames, 2, &info), "other names are ignored");

  uint8_t response[64];
  memset(response, 0, sizeof response);
  response[2] = 0x84;
  response[5] = 0;
  mdnsExpect(!mdnsParseQuery(response, 12, kMatrixNames, 2, &info), "ignore QR responses");

  n = mdnsBuildAnnounce(kMatrixNames, 2, ip, out, (int)sizeof out);
  mdnsExpect(n > 0 && mdnsInspect(out, n, ip, &seen), "gratuitous announcement");
  mdnsExpect(seen.aCount == 2 && seen.aIpMatch == 2 && seen.aClassFlush == 2 && seen.aaaaCount == 0,
             "announcement has both A records");
  mdnsExpect(seen.nsecCount == 2 && seen.nsecHasAaaa == 0 && seen.id == 0 && seen.qd == 0,
             "announcement NSEC for both names");

  n = mdnsBuildAnnounce(kSpiralNames, 2, ip, out, (int)sizeof out);
  mdnsExpect(n > 0 && mdnsInspect(out, n, ip, &seen) && seen.aCount == 2, "spiral announcement");

  if (g_mdnsFails) {
    printf("%d failed\n", g_mdnsFails);
    return 1;
  }
  printf("all passed\n");
  return 0;
}
#endif
