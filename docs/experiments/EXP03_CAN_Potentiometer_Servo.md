# EXP03 — CAN üzerinden potansiyometre ile servo kontrolü

Durum: Kullanıcı 9 Eylül 2026 tarihinde donanım denemesini tamamladığını bildirdi.
Bu ortamda Arduino derlemesi yapılmadı; seri monitör kaydı alınmadı.
EXP02 çalışması ayrı tutuldu. Bu deney iki klasik Uno (ATmega328P) kullanır.

## Malzemeler ve bağlantılar

2 Uno, 2 MCP2515 + transceiver modülü, 10k potansiyometre, SG90,
LED, 330 ohm direnç, jumper ve servo için uygun regüle 5V besleme.
Üçüncü MCP2515 bu deneyde kullanılmaz.

Her iki Uno için aynı SPI bağlantısı:

| MCP2515 modülü | Uno |
|---|---|
| VCC | 5V (5V uyumlu modül için) |
| GND | GND |
| CS | D10 |
| SCK | D13 |
| SI / MOSI | D11 |
| SO / MISO | D12 |
| INT | Bağlanmayacak; kod polling kullanır |

CAN H → CAN H, CAN L → CAN L; iki kartın GND hatlarını da birleştir.
Hattın iki ucunda birer 120 ohm sonlandırma olmalı. Modül üzerinde mevcutsa
ek direnç takma. Güç tamamen kapalıyken H-L arasında yaklaşık 60 ohm beklenir.
Her modülün kristalini kontrol et: kod MCP_8MHZ kullanır; 16.000 yazan modülün
kendi kodunda MCP_16MHZ seçilmeli. Her iki taraf CAN_125KBPS kullanır.

Uno A: potansiyometrenin dış uçları 5V ve GND, orta ucu A0.
Yön tersse dış uçları değiştir.

Uno B: servo sinyal ucu D5, kırmızı besleme ucu harici regüle 5V,
GND ucu besleme GND. Besleme GND ile Uno GND ortak olmalı.
USB ile çalışan Uno'nun 5V hattını harici beslemenin +5V hattına bağlama;
harici besleme yalnızca servoyu beslesin. Servoya 9V verme.
LED: D7 → 330 ohm → anot, katot → GND. Fault/bekleme durumunda yanar.
90° bu masaüstü deneyinin varsayılan konumudur; enerji kesme işlevi değildir.

## Yükleme

1. Arduino IDE'de Arduino AVR Boards / Arduino Uno seç.
2. `mcp2515.h` sağlayan autowp/arduino-mcp2515 kütüphanesini ve Arduino Servo
   kütüphanesini kur. Farklı API kullanan MCP_CAN kütüphanesi uygun değildir.
3. Uno A'ya `firmware/exp3/sender/sender.ino` yükle.
4. Uno B'ye `firmware/exp3/receiver/receiver.ino` yükle.
5. Her yüklemede doğru USB portunu seç. Seri monitör 115200 baud.
6. Alıcı monitöründe önce SYNC, ardından ACTIVE ve ADC/angle satırları beklenir.

## Protokol ve davranış

Standart CAN ID 0x110, DLC 4, gönderim periyodu 100 ms.

| Bayt | İçerik |
|---|---|
| 0 | Sabit 0xA3 |
| 1 | 8 bit alive counter, 255 sonrası 0 |
| 2 | ADC düşük bayt |
| 3 | ADC yüksek bayt |

ADC aralığı 0–1023, servo komutu 30–150°. İlk paket sayaç referansıdır;
ardından üç ardışık +1 sayaç geçişi gelince hareket başlar.
Hatalı uzunluk, işaret baytı, ADC veya sayaç servoyu 90°'ye götürür.
500 ms sayaç ilerlemesi olmazsa timeout oluşur ve yeniden senkronizasyon gerekir.
Başka ID'ler yok sayılır. TX queue/result=0 alıcı uygulamasının kabul onayı değildir.
Alıcının RX satırlarıyla kontrol et. Otomatik bus-off toparlanması bu deneyde yoktur;
kalıcı TX hatasında bağlantıyı düzeltip kartları resetle.

## Test sırası

| Deneme | Beklenen |
|---|---|
| Yalnız alıcı açık | LED yanar, servo 90° |
| Gönderici de açılır | SYNC + üç doğru adım sonrası ACTIVE, LED söner |
| Pot minimum / orta / maksimum | Yaklaşık 30° / 90° / 150° |
| Göndericinin USB beslemesi çıkarılır | Son ilerleyen mesajdan yaklaşık 500 ms sonra 90°, LED açık |
| Gönderici geri takılır | Yeniden senkronizasyon ve üç doğru adım sonrası kontrol |
| En az 30 saniye çalışma | Sayaç 255→0 geçişinde kesinti olmamalı |

Servo titremesi/reset varsa önce servo beslemesini ve ortak GND'yi kontrol et.
CAN INIT ERROR, MCP2515 başlatma sorunudur; RX yoksa ayrıca H/L, sonlandırma,
kristal, iki kartın bitrate ayarı ve doğru sketch/port seçimini kontrol et.

## Donanım denemesi — 9 Eylül 2026

Kullanıcı potansiyometre/servo deneyini tamamladığını bildirdi.
Deneme sırasında yalnız CAN H çıkarıldığında çalışmanın sürdüğü, CAN L de
çıkarıldığında hata oluştuğu bildirildi. Tek iletkenin kopması, mesajların
kesildiğini garanti etmez; bu yazılım geçerli mesaj/sayaç ilerlemesini izler.

LED için alıcı D7 bağlantısı ve göndericinin USB beslemesini çıkararak timeout
denemesi tarif edildi; ardından kullanıcı projenin tamamlandığını bildirdi.
Tek tek test satırları için ölçüm veya log paylaşılmadığından yukarıdaki tablo
beklenen davranışları belirtir; ölçülmüş sonuç tablosu değildir.
500 ms yazılım eşiğidir, ölçülmüş tepki süresi değildir.
Kod 8 MHz kristale ayarlıdır; kristal işareti ve kütüphane sürümleri kesin
olarak kaydedilmedi. Sayaç taşması ve geçersiz paket testleri ayrıca doğrulanmalıdır.

Kaynak: https://github.com/autowp/arduino-mcp2515
