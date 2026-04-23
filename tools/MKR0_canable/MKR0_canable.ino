#include <Arduino.h>
#include <CAN.h>

// ASCII serial format examples:
//   7FF#112233
//   18DAF110#1122334455667788
//   x18DAF110#112233
//   s7FF#112233
//   123#R
//   123#RTR

static String rxLine;

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
  if (n == 0) {
    outLen = 0;
    return true;
  }

  if ((n & 1) != 0 || n > 16) return false;

  outLen = n / 2;
  for (uint8_t i = 0; i < outLen; i++) {
    int hi = hexNibble(s[2 * i]);
    int lo = hexNibble(s[2 * i + 1]);
    if (hi < 0 || lo < 0) return false;
    outBytes[i] = (uint8_t)((hi << 4) | lo);
  }
  return true;
}

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

  // If not explicitly marked, infer extended if > 11-bit
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

  for (uint8_t i = 0; i < len; i++) {
    data[i] = tmpData[i];
  }

  return true;
}

// ---------- CAN TX ----------
static bool sendFromAsciiFrame(const String& line) {
  uint32_t id;
  bool extended;
  bool rtr;
  uint8_t data[8];
  uint8_t len;

  if (!parseCanLine(line, id, extended, rtr, data, len)) {
    Serial.println("Parse error. Example: 7FF#112233 or 18DAF110#1122334455667788");
    return false;
  }

  int result;

  if (extended) {
    result = CAN.beginExtendedPacket(id, len, rtr);
  } else {
    result = CAN.beginPacket(id, len, rtr);
  }

  if (!result) {
    Serial.println("CAN beginPacket failed");
    return false;
  }

  if (!rtr) {
    for (uint8_t i = 0; i < len; i++) {
      CAN.write(data[i]);
    }
  }

  if (!CAN.endPacket()) {
    Serial.println("CAN TX failed");
    return false;
  }

  Serial.print("CAN TX: ");
  Serial.print(extended ? "EXT" : "STD");
  Serial.print(" ID=0x");
  Serial.print(id, HEX);
  Serial.print(" DLC=");
  Serial.print(len);
  if (rtr) Serial.print(" RTR");
  Serial.println();

  return true;
}

// ---------- CAN RX ----------
static void serviceCanRx() {
  int packetSize = CAN.parsePacket();
  if (!packetSize) return;

  bool extended = CAN.packetExtended();
  bool rtr = CAN.packetRtr();
  unsigned long id = CAN.packetId();

  Serial.print("CAN RX: ");
  Serial.print(extended ? "EXT" : "STD");
  Serial.print(" ID=0x");
  Serial.print(id, HEX);
  Serial.print(" DLC=");
  Serial.print(packetSize);

  if (rtr) {
    Serial.print(" RTR");
  } else {
    Serial.print(" DATA=");
    bool first = true;
    while (CAN.available()) {
      uint8_t b = (uint8_t)CAN.read();
      if (!first) Serial.print(" ");
      if (b < 0x10) Serial.print("0");
      Serial.print(b, HEX);
      first = false;
    }
  }

  Serial.println();
}

// ---------- Serial line reader ----------
static void serviceSerialToCan() {
  while (Serial.available()) {
    char c = (char)Serial.read();

    if (c == '\r') continue;

    if (c == '\n') {
      if (rxLine.length()) {
        sendFromAsciiFrame(rxLine);
      }
      rxLine = "";
      continue;
    }

    if (rxLine.length() < 128) {
      rxLine += c;
    }
  }
}

// ---------- Setup / Loop ----------
void setup() {
  Serial.begin(115200);
  while (!Serial) { }

  delay(200);

  Serial.println();
  Serial.println("=== MKR Zero + MKR CAN Shield Serial <-> CAN ===");
  Serial.println("TX example: 7FF#FFFFFFFFFFFFFFFF");
  Serial.println("RX prints as: CAN RX: STD ID=0x123 DLC=8 DATA=11 22 33 44 55 66 77 88");

  // Start CAN bus at 1 Mbps
  if (!CAN.begin(1000000)) {
    Serial.println("Starting CAN failed!");
    while (1) { }
  }

  Serial.println("Ready.");
}

void loop() {
  serviceSerialToCan();   // Serial -> CAN
  serviceCanRx();         // CAN -> Serial
}