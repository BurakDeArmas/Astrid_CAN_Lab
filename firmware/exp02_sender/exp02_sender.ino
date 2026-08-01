#include <SPI.h>
#include <mcp2515.h>

// Kristalde 16.000 yazıyorsa MCP_16MHZ yap.
#define MCP_CLOCK MCP_8MHZ

constexpr uint8_t MCP2515_CS_PIN = 10;
constexpr uint16_t MESSAGE_ID = 0x100;
constexpr uint32_t SEND_PERIOD_MS = 500;
constexpr uint8_t COUNTER_FREEZE_BUTTON_PIN = 7;

// Uno için SPI hızını 8 MHz'e düşürüyoruz.
MCP2515 mcp2515(MCP2515_CS_PIN, 8000000);

struct can_frame txFrame;

uint8_t counter = 0;
uint32_t lastSendTimeMs = 0;

bool initializeCan()
{
    Serial.println(F("MCP2515 reset..."));

    MCP2515::ERROR result = mcp2515.reset();

    Serial.print(F("Reset sonucu: "));
    Serial.println(static_cast<int>(result));

    if (result != MCP2515::ERROR_OK)
    {
        Serial.println(F("HATA: MCP2515 reset basarisiz."));
        return false;
    }

    Serial.println(F("CAN bitrate ayarlaniyor..."));

    result = mcp2515.setBitrate(CAN_125KBPS, MCP_CLOCK);

    Serial.print(F("Bitrate sonucu: "));
    Serial.println(static_cast<int>(result));

    if (result != MCP2515::ERROR_OK)
    {
        Serial.println(F("HATA: Bitrate ayarlanamadi."));
        return false;
    }

    Serial.println(F("Normal moda geciliyor..."));

    result = mcp2515.setNormalMode();

    Serial.print(F("Normal mode sonucu: "));
    Serial.println(static_cast<int>(result));

    if (result != MCP2515::ERROR_OK)
    {
        Serial.println(F("HATA: Normal moda gecilemedi."));
        return false;
    }

    return true;
}

void setup()
{
    Serial.begin(115200);
    delay(1500);

    Serial.println();
    Serial.println(F("============================"));
    Serial.println(F("UNO-A CAN GONDERICI"));
    Serial.println(F("============================"));
    pinMode(COUNTER_FREEZE_BUTTON_PIN, INPUT_PULLUP);
    pinMode(MCP2515_CS_PIN, OUTPUT);
    digitalWrite(MCP2515_CS_PIN, HIGH);

    SPI.begin();

    if (!initializeCan())
    {
        Serial.println(F("CAN BASLATILAMADI."));

        while (true)
        {
            delay(1000);
        }
    }

    txFrame.can_id = MESSAGE_ID;
    txFrame.can_dlc = 2;

    Serial.println(F("CAN baslatildi."));
    Serial.println(F("Mesaj gonderimi basliyor..."));
}

void loop()
{
    const uint32_t nowMs = millis();

    if ((nowMs - lastSendTimeMs) < SEND_PERIOD_MS)
    {
        return;
    }

    lastSendTimeMs = nowMs;

    txFrame.data[0] = 0x55;
    txFrame.data[1] = counter;

    const MCP2515::ERROR result =
        mcp2515.sendMessage(&txFrame);

    if (result == MCP2515::ERROR_OK)
    {
        Serial.print(F("TX | ID=0x"));
        Serial.print(txFrame.can_id, HEX);

        Serial.print(F(" | DATA0=0x"));
        Serial.print(txFrame.data[0], HEX);

        Serial.print(F(" | Counter="));
        Serial.println(counter);

        const bool freezeCounter =
    digitalRead(COUNTER_FREEZE_BUTTON_PIN) == LOW;

if (freezeCounter)
{
    Serial.println(F("FAULT INJECTION: ALIVE COUNTER FROZEN"));
}
else
{
    counter++;
}
    }
    else
    {
        Serial.print(F("TX HATASI | Kod="));
        Serial.println(static_cast<int>(result));
    }
}