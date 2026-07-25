# EXP-01 — CAN Heartbeat ve Timeout Testi

## Amaç

İki Arduino Uno ve iki MCP2515 CAN modülü arasında temel
CAN haberleşmesini doğrulamak, alive counter bilgisini izlemek
ve haberleşme kesildiğinde timeout davranışını test etmek.

## Node dağılımı

- UNO-A: VCU heartbeat göndericisi
- UNO-B: Motor ECU heartbeat alıcısı

## CAN mesajı

| Özellik | Değer |
|---|---|
| Mesaj adı | VCU_Heartbeat |
| CAN ID | 0x100 |
| Frame tipi | Standard CAN |
| DLC | 2 byte |
| Byte 0 | 0x55 test verisi |
| Byte 1 | Alive counter |
| Gönderim periyodu | 100 ms |
| Timeout | 1000 ms |
| CAN bitrate | 125 kbps |

## Bağlantılar

Her iki Arduino Uno için:

| MCP2515 | Arduino Uno |
|---|---|
| VCC | 5V |
| GND | GND |
| CS | D10 |
| SI / MOSI | D11 |
| SO / MISO | D12 |
| SCK | D13 |

CAN hattı:

- CAN_H → CAN_H
- CAN_L → CAN_L
- GND → GND

## Test adımları

1. İki node çalıştırıldı.
2. Alıcıda 0x100 mesajları gözlemlendi.
3. Alive counter değerinin sıralı arttığı doğrulandı.
4. Gönderici Arduino'nun enerjisi kesildi.
5. Alıcının 1000 ms içerisinde timeout oluşturduğu doğrulandı.
6. Fault LED'in yandığı doğrulandı.
7. Gönderici tekrar çalıştırıldı.
8. Haberleşmenin yeniden kurulduğu ve fault durumunun temizlendiği doğrulandı.

## Örnek çıktı

```text
RX | ID=0x100 | DLC=2 | DATA0=0x55 | Counter=108
RX | ID=0x100 | DLC=2 | DATA0=0x55 | Counter=109

FAULT: VCU HEARTBEAT TIMEOUT
SAFE ACTION: MOTOR ENABLE = 0

CAN HABERLESMESI KURULDU - FAULT TEMIZLENDI
RX | ID=0x100 | DLC=2 | DATA0=0x55 | Counter=110


