#include <SPI.h>
#include <mcp2515.h>
#include <Servo.h>

// MCP2515 kristalinde 16.000 yazıyorsa MCP_16MHZ yap.
#define MCP_CLOCK MCP_8MHZ

// ----------------------------------------------------
// PIN TANIMLARI
// ----------------------------------------------------

constexpr uint8_t MCP2515_CS_PIN = 10;

constexpr uint8_t SERVO_PIN = 5;

// Sarı LED: Alive counter hatası
constexpr uint8_t COUNTER_FAULT_LED_PIN = 6;

// Kırmızı LED: Heartbeat timeout
constexpr uint8_t TIMEOUT_FAULT_LED_PIN = 7;

// ----------------------------------------------------
// CAN MESAJ AYARLARI
// ----------------------------------------------------

constexpr uint16_t EXPECTED_MESSAGE_ID = 0x100;
constexpr uint32_t HEARTBEAT_TIMEOUT_MS = 1000;

// ----------------------------------------------------
// SERVO AYARLARI
// ----------------------------------------------------

// Herhangi bir fault durumunda servo bu konuma gelir.
constexpr uint8_t SAFE_SERVO_ANGLE = 90;

// Sağlıklı durumda radar tarama sınırları.
constexpr uint8_t RADAR_MIN_ANGLE = 30;
constexpr uint8_t RADAR_MAX_ANGLE = 150;

// Her 20 ms'de servoyu 1 derece ilerlet.
constexpr uint32_t SERVO_STEP_PERIOD_MS = 20;

// Counter fault sonrasında kaç doğru mesajla toparlanacağız?
constexpr uint8_t REQUIRED_VALID_MESSAGES = 3;

// ----------------------------------------------------
// NESNELER
// ----------------------------------------------------

MCP2515 mcp2515(MCP2515_CS_PIN, 8000000);
Servo actuatorServo;

struct can_frame rxFrame;

// ----------------------------------------------------
// HABERLEŞME DEĞİŞKENLERİ
// ----------------------------------------------------

uint32_t lastMessageTimeMs = 0;

uint8_t lastAliveCounter = 0;
uint8_t consecutiveValidCounterCount = 0;

uint16_t aliveCounterErrorCount = 0;

bool heartbeatSeen = false;
bool timeoutFaultActive = true;

bool counterSynchronized = false;
bool aliveCounterFaultActive = false;

// ----------------------------------------------------
// SERVO DEĞİŞKENLERİ
// ----------------------------------------------------

uint32_t lastServoStepMs = 0;

uint8_t currentServoAngle = 255;
int8_t servoDirection = 1;

bool radarModeActive = false;

// ----------------------------------------------------
// CAN BAŞLATMA
// ----------------------------------------------------

bool initializeCan()
{
    Serial.println(F("MCP2515 resetleniyor..."));

    MCP2515::ERROR result = mcp2515.reset();

    Serial.print(F("Reset sonucu: "));
    Serial.println(static_cast<int>(result));

    if (result != MCP2515::ERROR_OK)
    {
        Serial.println(F("HATA: MCP2515 reset basarisiz."));
        return false;
    }

    Serial.println(F("CAN bitrate ayarlaniyor..."));

    result = mcp2515.setBitrate(
        CAN_125KBPS,
        MCP_CLOCK
    );

    Serial.print(F("Bitrate sonucu: "));
    Serial.println(static_cast<int>(result));

    if (result != MCP2515::ERROR_OK)
    {
        Serial.println(F("HATA: CAN bitrate ayarlanamadi."));
        return false;
    }

    Serial.println(F("Normal moda geciliyor..."));

    result = mcp2515.setNormalMode();

    Serial.print(F("Normal mode sonucu: "));
    Serial.println(static_cast<int>(result));

    if (result != MCP2515::ERROR_OK)
    {
        Serial.println(F("HATA: CAN normal moda gecemedi."));
        return false;
    }

    Serial.println(F("CAN baslatma basarili."));
    return true;
}

// ----------------------------------------------------
// SERVO FONKSİYONLARI
// ----------------------------------------------------

void writeServoAngle(uint8_t angle)
{
    if (currentServoAngle == angle)
    {
        return;
    }

    currentServoAngle = angle;
    actuatorServo.write(angle);
}

void enterSafeServoMode()
{
    if (radarModeActive)
    {
        Serial.println(F("SERVO MODE: SAFE POSITION 90 DEGREE"));
    }

    radarModeActive = false;
    servoDirection = 1;

    writeServoAngle(SAFE_SERVO_ANGLE);
}

// Aktüatörün çalışabilmesi için bütün şartlar sağlanmalı.
bool actuatorCanOperate()
{
    return heartbeatSeen &&
           !timeoutFaultActive &&
           counterSynchronized &&
           !aliveCounterFaultActive &&
           consecutiveValidCounterCount >= REQUIRED_VALID_MESSAGES;
}

void updateServoMotion()
{
    if (!actuatorCanOperate())
    {
        enterSafeServoMode();
        return;
    }

    // Sağlıklı duruma ilk geçiş.
    if (!radarModeActive)
    {
        radarModeActive = true;
        servoDirection = 1;

        writeServoAngle(SAFE_SERVO_ANGLE);

        lastServoStepMs = millis();

        Serial.println(F("SERVO MODE: RADAR SWEEP"));
    }

    const uint32_t nowMs = millis();

    if ((nowMs - lastServoStepMs) < SERVO_STEP_PERIOD_MS)
    {
        return;
    }

    lastServoStepMs = nowMs;

    int16_t nextAngle =
        static_cast<int16_t>(currentServoAngle) +
        servoDirection;

    if (nextAngle >= RADAR_MAX_ANGLE)
    {
        nextAngle = RADAR_MAX_ANGLE;
        servoDirection = -1;
    }
    else if (nextAngle <= RADAR_MIN_ANGLE)
    {
        nextAngle = RADAR_MIN_ANGLE;
        servoDirection = 1;
    }

    writeServoAngle(
        static_cast<uint8_t>(nextAngle)
    );
}

// ----------------------------------------------------
// LED / SAFETY ÇIKIŞLARI
// ----------------------------------------------------

void updateSafetyOutputs()
{
    digitalWrite(
        TIMEOUT_FAULT_LED_PIN,
        timeoutFaultActive ? HIGH : LOW
    );

    digitalWrite(
        COUNTER_FAULT_LED_PIN,
        aliveCounterFaultActive ? HIGH : LOW
    );
}

// ----------------------------------------------------
// ALIVE COUNTER KONTROLÜ
// ----------------------------------------------------

void processAliveCounter(uint8_t currentCounter)
{
    // İlk alınan counter'ın bir öncesini bilmediğimiz için
    // sadece referans olarak kaydediyoruz.
    if (!counterSynchronized)
    {
        lastAliveCounter = currentCounter;
        counterSynchronized = true;

        consecutiveValidCounterCount = 0;

        Serial.print(
            F("COUNTER SENKRONIZE EDILDI | Ilk deger=")
        );
        Serial.println(currentCounter);

        return;
    }

    // uint8_t olduğu için 255 + 1 otomatik olarak 0 olur.
    const uint8_t expectedCounter =
        static_cast<uint8_t>(lastAliveCounter + 1U);

    if (currentCounter == expectedCounter)
    {
        if (consecutiveValidCounterCount <
            REQUIRED_VALID_MESSAGES)
        {
            consecutiveValidCounterCount++;
        }

        Serial.print(F("COUNTER VALID | Beklenen="));
        Serial.print(expectedCounter);

        Serial.print(F(" | Gelen="));
        Serial.print(currentCounter);

        Serial.print(F(" | Dogru seri="));
        Serial.println(consecutiveValidCounterCount);

        if (aliveCounterFaultActive &&
            consecutiveValidCounterCount >=
                REQUIRED_VALID_MESSAGES)
        {
            aliveCounterFaultActive = false;

            Serial.println(
                F("ALIVE COUNTER FAULT TEMIZLENDI")
            );
        }
    }
    else
    {
        aliveCounterErrorCount++;

        consecutiveValidCounterCount = 0;
        aliveCounterFaultActive = true;

        Serial.println();
        Serial.println(F("FAULT: ALIVE COUNTER HATASI"));

        Serial.print(F("Beklenen: "));
        Serial.println(expectedCounter);

        Serial.print(F("Gelen: "));
        Serial.println(currentCounter);

        if (currentCounter == lastAliveCounter)
        {
            Serial.println(
                F("Hata tipi: COUNTER FROZEN / REPEATED")
            );
        }
        else
        {
            Serial.println(
                F("Hata tipi: COUNTER JUMP")
            );
        }

        Serial.print(F("Toplam counter hatasi: "));
        Serial.println(aliveCounterErrorCount);

        Serial.println(
            F("SAFE ACTION: SERVO = 90 DEGREE")
        );
        Serial.println();
    }

    // Bir sonraki mesaj için son alınan counter referans olur.
    lastAliveCounter = currentCounter;
}

// ----------------------------------------------------
// CAN MESAJ İŞLEME
// ----------------------------------------------------

void processCanMessage(const struct can_frame &frame)
{
    if (frame.can_id != EXPECTED_MESSAGE_ID)
    {
        return;
    }

    if (frame.can_dlc != 2)
    {
        Serial.print(F("HATA: Yanlis DLC | Gelen="));
        Serial.println(frame.can_dlc);
        return;
    }

    const uint8_t testData = frame.data[0];
    const uint8_t currentCounter = frame.data[1];

    // Doğru ID ve DLC ile mesaj fiziksel olarak geldi.
    lastMessageTimeMs = millis();
    heartbeatSeen = true;

    if (timeoutFaultActive)
    {
        timeoutFaultActive = false;

        Serial.println(
            F("CAN HABERLESMESI KURULDU")
        );
    }

    Serial.print(F("RX | ID=0x"));
    Serial.print(frame.can_id, HEX);

    Serial.print(F(" | DATA0=0x"));
    Serial.print(testData, HEX);

    Serial.print(F(" | Counter="));
    Serial.println(currentCounter);

    processAliveCounter(currentCounter);

    updateSafetyOutputs();
}

// ----------------------------------------------------
// HEARTBEAT TIMEOUT KONTROLÜ
// ----------------------------------------------------

void checkHeartbeatTimeout()
{
    if (!heartbeatSeen || timeoutFaultActive)
    {
        return;
    }

    const uint32_t nowMs = millis();

    if ((nowMs - lastMessageTimeMs) >=
        HEARTBEAT_TIMEOUT_MS)
    {
        timeoutFaultActive = true;
        heartbeatSeen = false;

        // Haberleşme geri geldiğinde counter'a
        // yeniden senkronize olacağız.
        counterSynchronized = false;
        consecutiveValidCounterCount = 0;

        // Timeout aktifken ana sorun mesajın hiç gelmemesi.
        // Sarı counter LED'ini kapatıyoruz.
        aliveCounterFaultActive = false;

        Serial.println();
        Serial.println(
            F("FAULT: VCU HEARTBEAT TIMEOUT")
        );
        Serial.println(
            F("SAFE ACTION: SERVO = 90 DEGREE")
        );
        Serial.println();

        updateSafetyOutputs();
        enterSafeServoMode();
    }
}

// ----------------------------------------------------
// SETUP
// ----------------------------------------------------

void setup()
{
    Serial.begin(115200);
    delay(1500);

    Serial.println();
    Serial.println(F("================================"));
    Serial.println(F("EXP-02 ALIVE COUNTER RECEIVER"));
    Serial.println(F("================================"));

    pinMode(COUNTER_FAULT_LED_PIN, OUTPUT);
    pinMode(TIMEOUT_FAULT_LED_PIN, OUTPUT);

    // Başlangıçta mesaj yok:
    // kırmızı LED açık, sarı LED kapalı.
    digitalWrite(COUNTER_FAULT_LED_PIN, LOW);
    digitalWrite(TIMEOUT_FAULT_LED_PIN, HIGH);

    // Servo başlangıçta güvenli konumda.
    actuatorServo.attach(SERVO_PIN);
    writeServoAngle(SAFE_SERVO_ANGLE);

    pinMode(MCP2515_CS_PIN, OUTPUT);
    digitalWrite(MCP2515_CS_PIN, HIGH);

    SPI.begin();

    if (!initializeCan())
    {
        Serial.println(F("CAN BASLATILAMADI."));

        timeoutFaultActive = true;

        updateSafetyOutputs();
        enterSafeServoMode();

        while (true)
        {
            delay(1000);
        }
    }

    Serial.println(F("CAN baslatildi."));
    Serial.println(
        F("0x100 heartbeat mesaji bekleniyor...")
    );

    updateSafetyOutputs();
}

// ----------------------------------------------------
// LOOP
// ----------------------------------------------------

void loop()
{
    const MCP2515::ERROR result =
        mcp2515.readMessage(&rxFrame);

    if (result == MCP2515::ERROR_OK)
    {
        processCanMessage(rxFrame);
    }

    checkHeartbeatTimeout();
    updateServoMotion();
}