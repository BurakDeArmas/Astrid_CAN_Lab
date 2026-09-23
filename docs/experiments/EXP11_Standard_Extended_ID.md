# EXP11 — Standart ve genişletilmiş kimlikler

23 Eylül 2026: Üç sketch derlendi; kullanıcı deneyin tamamlandığını bildirdi.
Ayrı seri log veya mod bazında ölçüm aktarılmadı; tablodaki değerler beklenen sonuçlardır.
Uno başına flash/RAM 4754/254 bayt; Nano 5348/260 bayt (Arduino AVR 1.8.7).

## Bağlantı

İki Uno + klasik 5V Nano ve üç MCP2515. Önceki CAN omurgası korunur:
ortak GND, H/H, L/L; yalnız iki uçta 120 ohm. Her kartta CS D10, MOSI D11,
MISO D12, SCK D13, INT boş; 5V uyumlu modül için VCC 5V. Kristal varsayılanı
8 MHz, CAN 125 kbit/s. Servo kapalı, pot ve LCD kullanılmaz.

| Kart | Sketch | Gönderilen ID'ler |
|---|---|---|
| Uno A | firmware/exp11/sender_std/sender_std.ino | STD 0x123, 0x124, 0x12F |
| Uno B | firmware/exp11/sender_ext/sender_ext.ino | EXT 0x123, 0x124, 0x1ABCDE |
| Nano | firmware/exp11/receiver/receiver.ino | Alıcı ve donanım filtresi |

Nano D2–buton–GND ile mod değiştirir (INPUT_PULLUP, 30 ms debounce).
Her sketch tek dosya; geçici sketch'e kopyalanabilir. Kütüphane autowp MCP2515.
Üç karta da EXP11 yükle, monitör 115200 baud. Her kaynak sırayla üç ID yayınlar;
ID başına yaklaşık 10 mesaj/s. DLC 6: 0xAB, kaynak 1/2, dört bayt LE sayaç.

## Temel fikir

Standart kimlik 11 bit (0…0x7FF), genişletilmiş kimlik 29 bit (0…0x1FFFFFFF).
Aynı sayısal 0x123 değeri iki formatta da geçerlidir; format bilgisi de
mesajın tanımının parçasıdır. Genişletilmiş çerçeve daha uzun ID taşır;
klasik CAN veri alanı yine en fazla 8 bayttır, CAN FD'ye dönüşmez.

Kütüphanede `CAN_EFF_FLAG` yazılımsal format işaretidir. Örneğin
`0x123 | CAN_EFF_FLAG` genişletilmiş çerçeve ister. Bu bayrak ID'nin 30. biti
olarak hatta gönderilmez; denetleyici çerçevenin IDE alanını ayarlar.

Bu deney arbitration yarışı değildir. Sayısal STD 0x123 ile EXT 0x123'ün
ilk 11 arbitration biti aynı değildir; genel sayısal karşılaştırmadan öncelik
sonucu çıkarma. Öncelik için çerçevenin bit yerleşimi dikkate alınmalıdır.

## Modlar ve beklenen çıktı

Nano başlığı:
`window_ms,mode,STD123,STD124,STD12F,EXT123,EXT124,EXT1ABCDE,other,ovr`

| Mod | Donanım kabulü | Altı ID sütunu (yaklaşık /s) |
|---|---|---|
| 0 | STD ve EXT | 10,10,10,10,10,10 |
| 1 | Yalnız STD | 10,10,10,0,0,0 |
| 2 | Yalnız EXT | 0,0,0,10,10,10 |
| 3 | Yalnız EXT 0x123 | 0,0,0,10,0,0 |

Her basış 0→1→2→3→0. Mod 0'da maskeler sıfır ve filtreler dönüşümlü
EXIDE=0/1; mod 1/2'de tüm filtreler ilgili formata ayarlanır. Mod 3'te
29 bit tam maske 0x1FFFFFFF ve extended filtre 0x123 kullanılır. MCP2515'te
formatı EXIDE seçer; maske register'ında MIDE adlı bir bit yoktur.
RXM=00 korunur. Altı filtre ve iki maske birlikte ayarlanır.

Mod değişiminde denetleyici resetlenir, eski tamponlar ve sayaçlar temizlenir;
geçişte kesinti normaldir. Her modda 5 saniye bekle ve birkaç tam pencereyi
karşılaştır. other bilinmeyen/yanlış biçimli çerçeve, ovr taşma örnek sayısıdır.
Bu düşük hızda ikisinin de sıfır kalması beklenir. Filtre hattın yayınını
durdurmaz, yalnız bu düğümün kabulünü değiştirir.

Donanım tamamlanma kaydı kullanıcı bildirimine dayanır. Modların ayrı seri
çıktıları henüz arşivlenmedi.

Kaynak: [Microchip MCP2515 veri sayfası](https://ww1.microchip.com/downloads/aemDocuments/documents/APID/ProductDocuments/DataSheets/MCP2515-Family-Data-Sheet-DS20001801K.pdf), çerçeve biçimleri ve RXFnSIDL.EXIDE.
