# EXP04 — Üç düğümlü CAN ve LCD durum ekranı

Durum: Üç sketch 10 Eylül 2026 tarihinde Arduino AVR 1.8.7 ile başarıyla
derlendi; aynı gün kullanıcı donanım denemesinin başarıyla geçtiğini bildirdi.
EXP03 bağlantısı üzerine üçüncü düğüm eklenir.
Hedef: İki klasik 5V Uno ve bir klasik 5V ATmega328P Nano, üç MCP2515 modülü,
10k pot, SG90, harici servo beslemesi ve I2C arkalıklı 16x2 LCD.
Nano 33 gibi 3.3V kartlar bu bağlantı tablosunun kapsamında değildir.

## Dosyalar ve görevler

| Kart | Sketch | İşlev |
|---|---|---|
| Uno A | firmware/exp4/sender/sender.ino | A0 potunu CAN'a gönderir |
| Uno B | firmware/exp4/receiver/receiver.ino | Servo kontrolü ve durum yayını |
| Nano | firmware/exp4/display/display.ino | Komut ve durumu LCD'de gösterir |

EXP01–03 dosyaları korunur. Bu deney için üç karta da EXP04 sketch'lerini yükle.

## Bağlantı

Üç kartta da MCP2515 CS=D10, SCK=D13, MOSI/SI=D11, MISO/SO=D12,
GND=GND, VCC=5V (5V uyumlu modül için). INT boş; polling kullanılıyor.
Her kart/modül GND'sini ortakla. CAN H ve CAN L'yi birbirine karıştırma.

Hat düzeni:

```text
Uno A + MCP [120 ohm] ==== Uno B + MCP [sonlandırma kapalı] ==== Nano + MCP [120 ohm]
                       ortak CAN H, CAN L ve GND
```

Yalnız fiziksel iki uçta 120 ohm olmalı; ortadaki modülün sonlandırma jumper'ını
çıkar. Jumper yoksa modülün sonlandırma yapısını belirlemeden rastgele parça sökme.
Tüm beslemeler kapalıyken H-L arası yaklaşık 60 ohm; üç adet 120 ohm varsa
yaklaşık 40 ohm olur. Bağlantıları kısa tut, H/L çiftini birlikte taşı.

Uno A: pot dış uçları 5V/GND, orta uç A0.
Uno B: servo sinyali D5; fault LED D7 → 330 ohm → anot, katot → GND.
Servoyu harici regüle 5V ile besle, besleme GND'sini kartlarla ortakla.
USB ile beslenen kartların 5V hatlarını birbirine veya servo beslemesi +5V'a bağlama.
90° deneyde kullanılan bekleme komutudur, servo enerjisini kesmez.

| I2C LCD | Nano |
|---|---|
| VCC | 5V |
| GND | GND |
| SDA | A4 |
| SCL | A5 |

## Kütüphane ve yükleme

- CAN: autowp/arduino-mcp2515 (`mcp2515.h`), EXP03 ile aynı.
- Servo: Arduino Servo.
- LCD: Library Manager'dan **hd44780, Bill Perry**. Kod `hd44780_I2Cexp`
  kullanır; desteklenen I2C arkalığın adresini ve pin eşlemesini otomatik bulur.
  LiquidCrystal_I2C yerine bu kütüphane gereklidir.
- Uno kartları: Arduino AVR Boards → Arduino Uno.
- Nano: Arduino Nano → ATmega328P. Yüklemede senkronizasyon hatası varsa doğru
  port/kablo kontrolünden sonra klon kart için ATmega328P (Old Bootloader) dene.
- Seri monitör: her üç kartta 115200 baud.
- Üç kodda 125 kbit/s ve MCP_8MHZ var. Her modülün kristalini ayrı kontrol et;
  16.000 yazıyorsa yalnız o modülün kodunda MCP_16MHZ seç.

LCD boşsa kontrast potunu ayarla. LCD INIT ERROR varsa hd44780 örneklerindeki
ioClass/hd44780_I2Cexp/I2CexpDiag ile bağlantıyı test et. Seri monitör ekran
bilgilerini LCD başlatılamasa da gösterir.

## Mesajlar

Standart 11 bit ID, DLC=4. Her kaynak 100 ms'de bir kendi mesajını yollar.

| ID | Kaynak | Bayt 0 | Bayt 1 | Bayt 2 | Bayt 3 |
|---|---|---|---|---|---|
| 0x110 | Uno A | 0xA3 | Komut sayacı | ADC düşük | ADC yüksek |
| 0x120 | Uno B | 0xB4 | Durum sayacı | Uygulanan açı komutu | 1=aktif, 0=bekleme/hata |

ADC 0–1023, açı 30–150°. Sayaçlar bağımsızdır, 255→0 geçer.
Alıcı ilk referansın ardından üç ardışık +1 geçişi bekler. Hatalı komutta servo
90°'ye döner; 500 ms ilerleme olmazsa bağlantı timeout olur.
Nano her iki kaynağın sayaç ilerlemesini ayrı izler. Geçersiz paket/sayaç
durumunda güvenilirlik hemen temizlenir; yeniden üç doğru geçiş gerekir.

LCD ilk satır: HEDEF veya KOMUT YOK/HATA.
İkinci satır: UYG (servo yazılım komutu), AKTIF/BEKLE veya AKTUATOR YOK.
AKTUATOR YOK, güvenilir/güncel durum akışı bulunmadığı anlamına gelir;
kartın kesin olarak enerjisiz olduğunu söylemez.
Servo açısı sensörden ölçülmez. Nano kontrol komutu üretmez ama normal CAN
modunda geçerli çerçevelere ACK verir. Bu nedenle TX başarısı servo kartının
mesajı işlediğinin kanıtı değildir; durum mesajına bakılır.

## Donanım test sırası

1. Üç kartı aç: yaklaşık birkaç mesaj sonra HEDEF ve UYG değerleri görünmeli.
2. Potu çevir: servo hareket etmeli, iki değer kısa bir yayın gecikmesiyle değişmeli.
3. Uno A'nın MCP modülünün H ve L bağlantısını ayır, diğer düğümler arasındaki
   omurga ve iki uçtaki sonlandırmaları yerinde tut: servo yaklaşık 500 ms sonra
   90°; LCD KOMUT YOK/HATA ve UYG:90 BEKLE göstermeli.
4. Geri bağla: senkronizasyon sonrası kontrol geri gelmeli.
5. Uno B'nin H ve L bağlantısını ayır (A–Nano omurgasını koru): LCD hedefi
   göstermeye devam etmeli, yaklaşık 500–700 ms içinde AKTUATOR YOK yazmalı.
6. Nano'nun H/L bağlantısını ayır, iki uçtaki sonlandırmayı hatta koru
   (gerekirse sonlandırmayı yeni fiziksel uca taşı): iki Uno'nun servo kontrolü devam etmeli.
7. En az 30 saniye çalıştır: sayaç 255→0 geçişini gözle.

Tek CAN telini çıkarmak iletişimi mutlaka kesmez. LCD 200 ms'de yenilenir;
500 ms timeout ekranda bir sonraki yenilemede görülür.
Kalıcı TX hatasında bağlantıyı düzeltip kartları resetle; otomatik bus-off
toparlanması bu deneyde uygulanmadı.

## Donanım denemesi — 10 Eylül 2026

Projeyi kuran kullanıcı EXP04 denemesinin başarıyla geçtiğini bildirdi.
Bu kayıt genel işlevsel başarı bildirimidir; yukarıdaki her test adımı için
ayrı seri logu, fotoğraf veya ölçüm paylaşılmadı. Timeout süreleri yazılım
ayarlarıdır; ölçülmüş gecikme olarak sunulmaz. Tek tek arıza senaryolarının
çıktıları ve kullanılan kütüphane sürümleri sonraki test kaydında eklenebilir.

## Derleme kontrolü

| Hedef | Flash | Statik RAM |
|---|---|---|
| Uno A | 4666 bayt | 236 bayt |
| Uno B | 6146 bayt | 285 bayt |
| Nano | 11108 bayt | 637 bayt |

Arduino IDE içindeki arduino-cli ile derlendi. LCD kütüphanesi derleme için
geçici klasöre indirildi; kullanıcı Arduino IDE Library Manager üzerinden ayrıca
kurmalıdır. Bu derleme tablosu donanım testinden ayrı bir kontroldür;
kartlara yükleme ve fiziksel deneme kullanıcı tarafından gerçekleştirildi.

Kaynaklar: https://github.com/autowp/arduino-mcp2515 ve
https://github.com/duinoWitchery/hd44780
