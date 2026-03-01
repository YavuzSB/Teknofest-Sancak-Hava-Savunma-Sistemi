// Arduino/ESP32 UART Receiver örneği (Non-blocking State Machine + Resync + Timeout + CRC16)
// Frame:
//   0x55 0xAA Type Len Payload CRC16(2)  [CRC: H1..Payload dahil]
// CRC: CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF)

#include <Arduino.h>
#include "ProtocolDef.h" // Repo kökünde: protocol/ProtocolDef.h

// Arduino IDE için pratik: Bu dosyayı Receiver.ino ile aynı klasöre kopyalayın
// veya include path'inize repo kökündeki protocol/ klasörünü ekleyin.

static constexpr uint16_t kCrcInit = 0xFFFF;
static constexpr uint32_t kTimeoutMs = 20;
static constexpr uint8_t kMaxPayload = 64;

enum class RxState : uint8_t {
  WAIT_H1,
  WAIT_H2,
  GET_TYPE,
  GET_LEN,
  GET_PAYLOAD,
  GET_CRC_H,
  GET_CRC_L,
};

static RxState g_state = RxState::WAIT_H1;
static uint8_t g_type = 0;
static uint8_t g_len = 0;
static uint8_t g_payload[kMaxPayload];
static uint8_t g_payloadIdx = 0;

static uint16_t g_crcRunning = kCrcInit;
static uint16_t g_crcRx = 0;

static uint32_t g_lastByteMs = 0;
static uint32_t g_crcErrorCount = 0;
static uint32_t g_frameOkCount = 0;
static uint32_t g_lenRejectCount = 0;

static inline void resetRx() {
  g_state = RxState::WAIT_H1;
  g_payloadIdx = 0;
  g_len = 0;
  g_crcRunning = kCrcInit;
  g_crcRx = 0;
}

static void handleFrame(uint8_t type, const uint8_t* payload, uint8_t len) {
  g_frameOkCount++;

  // Örnek: AimPayload parse
  if (type == static_cast<uint8_t>(MsgType::Aim) && len == sizeof(AimPayload)) {
    AimPayload a;
    memcpy(&a, payload, sizeof(AimPayload));
    // TODO: Pan/tilt/distance kullan
    // Örn: motor kontrol setpoint
  }
}

void setup() {
  Serial.begin(115200);
  g_lastByteMs = millis();
  resetRx();
}

void loop() {
  // Timeout: akış koparsa state'i sıfırla (kilitlenme önlemi)
  const uint32_t now = millis();
  if (g_state != RxState::WAIT_H1 && (now - g_lastByteMs) > kTimeoutMs) {
    resetRx();
  }

  while (Serial.available() > 0) {
    const uint8_t b = static_cast<uint8_t>(Serial.read());
    g_lastByteMs = millis();

    switch (g_state) {
      case RxState::WAIT_H1:
        if (b == UART_H1) {
          g_state = RxState::WAIT_H2;
          g_crcRunning = kCrcInit;
          g_crcRunning = crc16_ccitt_false_update(g_crcRunning, b);
        }
        break;

      case RxState::WAIT_H2:
        if (b == UART_H2) {
          g_crcRunning = crc16_ccitt_false_update(g_crcRunning, b);
          g_state = RxState::GET_TYPE;
        } else if (b == UART_H1) {
          // Resync: 0x55 0x55 ... 0xAA kaymasına tolerans
          g_crcRunning = kCrcInit;
          g_crcRunning = crc16_ccitt_false_update(g_crcRunning, b);
          g_state = RxState::WAIT_H2;
        } else {
          resetRx();
        }
        break;

      case RxState::GET_TYPE:
        g_type = b;
        g_crcRunning = crc16_ccitt_false_update(g_crcRunning, b);
        g_state = RxState::GET_LEN;
        break;

      case RxState::GET_LEN:
        g_len = b;
        g_crcRunning = crc16_ccitt_false_update(g_crcRunning, b);

        if (g_len > kMaxPayload) {
          g_lenRejectCount++;
          resetRx();
          break;
        }

        g_payloadIdx = 0;
        g_state = (g_len == 0) ? RxState::GET_CRC_H : RxState::GET_PAYLOAD;
        break;

      case RxState::GET_PAYLOAD:
        g_payload[g_payloadIdx++] = b;
        g_crcRunning = crc16_ccitt_false_update(g_crcRunning, b);
        if (g_payloadIdx >= g_len) {
          g_state = RxState::GET_CRC_H;
        }
        break;

      case RxState::GET_CRC_H:
        g_crcRx = static_cast<uint16_t>(b) << 8;
        g_state = RxState::GET_CRC_L;
        break;

      case RxState::GET_CRC_L:
        g_crcRx |= b;
        if (g_crcRx == g_crcRunning) {
          handleFrame(g_type, g_payload, g_len);
        } else {
          g_crcErrorCount++;
        }
        resetRx();
        break;
    }
  }

  // ... ana uygulama döngüsü
}
