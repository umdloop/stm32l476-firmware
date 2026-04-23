#include <Arduino.h>
#include "driver/twai.h"

// ====== PIN MAP (ESP32 GPIO numbers) ======
#define CAN_TX GPIO_NUM_5
#define CAN_RX GPIO_NUM_4

// ====== CAN SETTINGS ======
static const bool CAN_NO_ACK = true;   // blind TX (no ACK required)

// ---------- TWAI helpers ----------
static void printStatus(const char* tag) {
  twai_status_info_t s;
  if (twai_get_status_info(&s) == ESP_OK) {
    Serial.printf("[%s] state=%d txq=%d rxq=%d tx_err=%d rx_err=%d bus_err=%lu\n",
                  tag,
                  (int)s.state,
                  (int)s.msgs_to_tx,
                  (int)s.msgs_to_rx,
                  (int)s.tx_error_counter,
                  (int)s.rx_error_counter,
                  (unsigned long)s.bus_error_count);
  }
}

static bool canSend(uint32_t id, const uint8_t* data, uint8_t len, bool extended = false) {
  if (len > 8) len = 8;

  twai_message_t msg = {};
  msg.identifier = id;
  msg.extd = extended ? 1 : 0;
  msg.rtr  = 0;
  msg.data_length_code = len;
  for (uint8_t i = 0; i < len; i++) msg.data[i] = data ? data[i] : 0;

  return (twai_transmit(&msg, 0) == ESP_OK);
}

// ---------- Parsing helpers ----------
static int hexNibble(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
  if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
  return -1;
}

static bool parseHexU32(const char* s, size_t n, uint32_t& out) {
  out = 0;
  if (n == 0 || n > 8) return false;
  for (size_t i = 0; i < n; i++) {
    int v = hexNibble(s[i]);
    if (v < 0) return false;
    out = (out << 4) | (uint32_t)v;
  }
  return true;
}

static bool parseHexBytes(const char* s, size_t n, uint8_t* outBytes, uint8_t& outLen) {
  if (n == 0) { outLen = 0; return true; }
  if ((n & 1) != 0 || n > 16) return false;

  outLen = (uint8_t)(n / 2);
  for (uint8_t i = 0; i < outLen; i++) {
    int hi = hexNibble(s[2 * i]);
    int lo = hexNibble(s[2 * i + 1]);
    if (hi < 0 || lo < 0) return false;
    outBytes[i] = (uint8_t)((hi << 4) | lo);
  }
  return true;
}

// ---------- ASCII CAN parser ----------
static bool parseCanLine(const String& line, uint32_t& id, bool& extended, bool& rtr,
                         uint8_t* data, uint8_t& len) {
  String s = line;
  s.trim();
  if (s.length() == 0) return false;

  int hash = s.indexOf('#');
  if (hash < 0) return false;

  String idStr   = s.substring(0, hash);
  String dataStr = s.substring(hash + 1);

  idStr.trim();
  dataStr.trim();

  extended = false;
  if (idStr.length() && (idStr[0] == 'x' || idStr[0] == 'X')) {
    extended = true;
    idStr.remove(0, 1);
  } else if (idStr.length() && (idStr[0] == 's' || idStr[0] == 'S')) {
    extended = false;
    idStr.remove(0, 1);
  }

  uint32_t tmpId;
  if (!parseHexU32(idStr.c_str(), idStr.length(), tmpId)) return false;
  if (!extended && idStr.length() > 3) extended = true;

  if ((!extended && tmpId > 0x7FF) || (extended && tmpId > 0x1FFFFFFF)) return false;
  id = tmpId;

  rtr = false;
  if (dataStr.equalsIgnoreCase("R") || dataStr.equalsIgnoreCase("RTR")) {
    rtr = true;
    len = 0;
    return true;
  }

  uint8_t tmpData[8];
  if (!parseHexBytes(dataStr.c_str(), dataStr.length(), tmpData, len)) return false;
  for (uint8_t i = 0; i < len; i++) data[i] = tmpData[i];
  return true;
}

static bool sendFromAsciiFrame(const String& line) {
  uint32_t id;
  bool extended, rtr;
  uint8_t data[8], len;

  if (!parseCanLine(line, id, extended, rtr, data, len)) {
    Serial.println("Parse error. Example: 7FF#112233 or 18DAF110#1122334455667788");
    return false;
  }

  twai_message_t msg = {};
  msg.identifier = id;
  msg.extd = extended;
  msg.rtr  = rtr;
  msg.data_length_code = rtr ? 0 : len;
  for (uint8_t i = 0; i < len; i++) msg.data[i] = data[i];

  esp_err_t err = twai_transmit(&msg, 0);
  if (err == ESP_OK) {
    Serial.printf("CAN TX: %s ID=0x%lX DLC=%u%s\n",
                  extended ? "EXT" : "STD",
                  (unsigned long)id,
                  msg.data_length_code,
                  rtr ? " RTR" : "");
    return true;
  }

  Serial.printf("CAN TX failed: %s\n", esp_err_to_name(err));
  printStatus("tx_fail");
  return false;
}

// ---------- RX “callback” ----------
static void serviceCanRx() {
  twai_message_t msg;

  while (twai_receive(&msg, 0) == ESP_OK) {
    Serial.printf("CAN RX: %s ID=0x%lX DLC=%u",
                  msg.extd ? "EXT" : "STD",
                  (unsigned long)msg.identifier,
                  msg.data_length_code);

    if (msg.rtr) {
      Serial.print(" RTR");
    } else {
      Serial.print(" DATA=");
      for (uint8_t i = 0; i < msg.data_length_code; i++) {
        Serial.printf("%02X", msg.data[i]);
        if (i + 1 < msg.data_length_code) Serial.print(" ");
      }
    }
    Serial.println();
  }
}

// ---------- Serial line reader ----------
static String rxLine;

static void serviceSerialToCan() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      if (rxLine.length()) sendFromAsciiFrame(rxLine);
      rxLine = "";
      continue;
    }
    if (rxLine.length() < 128) rxLine += c;
  }
}

// ====== SETUP ======
void setup() {
  Serial.begin(115200);
  delay(200);

  Serial.println("\n=== ESP32 TWAI Serial <-> CAN ===");
  Serial.println("TX: 7FF#FFFFFFFFFFFFFFFF");
  Serial.println("RX prints as: CAN RX: STD ID=0x123 DLC=8 DATA=...");

  pinMode((int)CAN_RX, INPUT_PULLUP);

  twai_general_config_t g_config =
      TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX, CAN_RX,
                                  CAN_NO_ACK ? TWAI_MODE_NO_ACK : TWAI_MODE_NORMAL);

  g_config.tx_queue_len = 1;
  g_config.rx_queue_len = 8;

  twai_timing_config_t t_config = TWAI_TIMING_CONFIG_1MBITS();
  twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

  twai_driver_install(&g_config, &t_config, &f_config);
  twai_start();

  printStatus("after_start");
  Serial.println("Ready.");
}

// ====== LOOP ======
void loop() {
  serviceSerialToCan();   // Serial -> CAN
  serviceCanRx();         // CAN -> Serial (callback-style)
}
