// WiFiS3 WiFiClient::read(buf, n) does not ask the ESP for n bytes.
// WiFiClient::_read() and WiFiSSLClient::_read() always request
// rx fifo freePositions()-1, which is 1023 on an empty 1024-byte fifo.
// The reply is +CLIENTRECEIVE:<len>|<payload>OK. Serial2's ring on this
// core is 512 bytes (SERIAL_BUFFER_SIZE). A 1023-byte payload overflows
// that ring, the UART drops bytes, and the sized AT parser waits for a
// count it will never see. available() can already be 3460 when this
// happens: the whole HTTP response is sitting on the ESP32.
//
// Ask for at most AT_READ_MAX (64) bytes. The framed reply is about 90
// bytes, so it fits in the ring even if the parser is slower than 115200.
#pragma once

#include <string.h>
#include <string>
#include <WiFiClient.h>
#include <WiFiSSLClient.h>
#include "Modem.h"
#include "WiFiCommands.h"

static const size_t AT_READ_MAX = 64;

static int atPull(int sock, const char* prompt, const char* cmd,
                  uint8_t* buf, size_t n) {
  if (sock < 0 || !buf || n == 0) return 0;
  if (n > AT_READ_MAX) n = AT_READ_MAX;
  std::string res;
  modem.avoid_trim_results();
  modem.read_using_size();
  modem.timeout(400);
  bool ok = modem.write(std::string(prompt), res, "%s%d,%u\r\n",
                        cmd, sock, (unsigned)n);
  modem.timeout(800);
  if (!ok || res.size() == 0) return 0;
  size_t m = res.size();
  if (m > n) m = n;
  memcpy(buf, res.data(), m);
  return (int)m;
}

static int fifoTake(arduino::FifoBuffer<uint8_t, 1024>* fifo,
                    uint8_t* buf, size_t n) {
  if (!fifo || fifo->available() <= 0) return 0;
  size_t i = 0;
  while (i < n) {
    bool got = false;
    uint8_t b = fifo->read(&got);
    if (!got) break;
    buf[i++] = b;
  }
  return (int)i;
}

class AtClient : public WiFiClient {
 public:
  AtClient() = default;
  AtClient(const WiFiClient& o) : WiFiClient(o) {}
  AtClient& operator=(const WiFiClient& o) {
    WiFiClient::operator=(o);
    return *this;
  }
  int readAt(uint8_t* buf, size_t n) {
    if (!buf || n == 0 || _sock < 0) return 0;
    if (n > AT_READ_MAX) n = AT_READ_MAX;
    int have = fifoTake(rx_buffer.get(), buf, n);
    if (have > 0) return have;
    return atPull(_sock, PROMPT(_CLIENTRECEIVE), CMD_WRITE(_CLIENTRECEIVE), buf, n);
  }
};

class AtSslClient : public WiFiSSLClient {
 public:
  int readAt(uint8_t* buf, size_t n) {
    if (!buf || n == 0 || _sock < 0) return 0;
    if (n > AT_READ_MAX) n = AT_READ_MAX;
    int have = fifoTake(rx_buffer.get(), buf, n);
    if (have > 0) return have;
    return atPull(_sock, PROMPT(_SSLCLIENTRECEIVE), CMD_WRITE(_SSLCLIENTRECEIVE), buf, n);
  }
};
