#include <SPI.h>
#include <mcp2515.h>

#define MCP_CLOCK MCP_8MHZ

constexpr uint8_t MCP2515_CS_PIN = 10;
constexpr uint8_t FAULT_LED_PIN = 7;

constexpr uint16_t EXPECTED_MESSAGE_ID = 0x100;
constexpr uint32_t HEARTBEAT_TIMEOUT_MS = 1000;

MCP2515 mcp2515(MCP2515_CS_PIN, 8000000);

struct can_frame rxFrame;

uint32_t lastMessageTimeMs = 0;
bool messageReceivedBefore = false;
bool timeoutActive = true;

bool initializeCan()
{
    MCP2515::ERROR result;

    Serial.println(F("MCP2515 reset..."));
    result = mcp2515.reset();

    Serial.print(F("Reset sonucu: "));
    Serial.println(static_cast<int>(result));

    if (result != MCP2515::ERROR_OK)
    {
        return false;
    }

    Serial.println(F("CAN bitrate ayarlaniyor..."));
    result = mcp2515.setBitrate(CAN_125KBPS, MCP_CLOCK);

    Serial.print(F("Bitrate sonucu: "));
    Serial.println(static_cast<int>(result));

    if (result != MCP2515::ERROR_OK)
    {
        return false;
    }

    Serial.println(F("Normal moda geciliyor..."));
    result = mcp2515.setNormalMode();

    Serial.print(F("Normal mode sonucu: "));
    Serial.println(static_cast<int>(result));

    return result == MCP2515::ERROR_OK;
}

void activateFault()
{
    timeoutActive = true;
    digitalWrite(FAULT_LED_PIN, HIGH);
}

void clearFault()
{
    timeoutActive = false;
    digitalWrite(FAULT_LED_PIN, LOW);
}

void processMessage(const struct can_frame& frame)
{
    if (frame.can_id != EXPECTED_MESSAGE_ID)
    {
        return;
    }

    if (frame.can_dlc < 2)
    {
        Serial.println(F("HATA: DLC yetersiz."));
        return;
    }

    lastMessageTimeMs = millis();
    messageReceivedBefore = true;

    if (timeoutActive)
    {
        clearFault();
        Serial.println(F("CAN HABERLESMESI KURULDU - FAULT TEMIZLENDI"));
    }

    Serial.print(F("RX | ID=0x"));
    Serial.print(frame.can_id, HEX);

    Serial.print(F(" | DLC="));
    Serial.print(frame.can_dlc);

    Serial.print(F(" | DATA0=0x"));
    Serial.print(frame.data[0], HEX);

    Serial.print(F(" | Counter="));
    Serial.println(frame.data[1]);
}

void checkTimeout()
{
    if (!messageReceivedBefore)
    {
        return;
    }

    if (timeoutActive)
    {
        return;
    }

    const uint32_t nowMs = millis();

    if ((nowMs - lastMessageTimeMs) >= HEARTBEAT_TIMEOUT_MS)
    {
        activateFault();

        Serial.println();
        Serial.println(F("FAULT: VCU HEARTBEAT TIMEOUT"));
        Serial.println(F("SAFE ACTION: MOTOR ENABLE = 0"));
        Serial.println(F("FAULT LED SUREKLI YANIYOR"));
        Serial.println();
    }
}

void setup()
{
    Serial.begin(115200);
    delay(1500);

    Serial.println();
    Serial.println(F("============================"));
    Serial.println(F("UNO-B MOTOR ECU / ALICI"));
    Serial.println(F("============================"));

    pinMode(FAULT_LED_PIN, OUTPUT);

    
    activateFault();

    pinMode(MCP2515_CS_PIN, OUTPUT);
    digitalWrite(MCP2515_CS_PIN, HIGH);

    SPI.begin();

    if (!initializeCan())
    {
        Serial.println(F("CAN BASLATILAMADI."));

        while (true)
        {
            digitalWrite(FAULT_LED_PIN, HIGH);
            delay(1000);
        }
    }

    Serial.println(F("CAN baslatildi."));
    Serial.println(F("0x100 heartbeat mesaji bekleniyor..."));
}

void loop()
{
    const MCP2515::ERROR result =
        mcp2515.readMessage(&rxFrame);

    if (result == MCP2515::ERROR_OK)
    {
        processMessage(rxFrame);
    }

    checkTimeout();
}