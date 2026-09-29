# EXP15 — ISO-TP temel akışı ve Flow Control

29 Eylül 2026: Üç sketch derlendi. Gönderici/alıcı durum makineleri host
ortamında ASan/UBSan ile test edildi. Kullanıcı deneyin tamamlandığını ve
konuların öğrenildiğini bildirdi. Ayrı seri log veya mod bazında ölçüm aktarılmadı.

## Amaç ve kapsam

EXP14'te parçaları kendi başlığımızla taşıdık. Burada ISO-TP'nin SF, FF, CF
ve FC biçimlerini kullanarak alıcının aktarım hızını nasıl yönettiğini çalışıyoruz.
Bu sınırlı eğitim uygulaması tam bir ISO 15765-2 uyumluluk uygulaması değildir.
Normal adresleme, klasik CAN, standart ID ve 8 bayta sıfırla doldurulmuş
çerçeveler kullanılır. Uygulama verisi en fazla 128 bayttır.

- SF (Single Frame): 1–7 bayt mesaj. Host testinde denenir.
- FF (First Frame): toplam mesaj uzunluğu ve ilk altı veri baytı.
- CF (Consecutive Frame): dört bit sıra numarası ve en fazla yedi veri baytı.
- FC (Flow Control): alıcının Continue To Send, Block Size ve STmin bilgisi.

Donanım butonu 24 baytlık `ASTRID ISO MESSAGE 00001` metnini gönderir.
Dolayısıyla normal akış FF + üç CF içerir. CF sıra numarası 1'den başlar,
15'ten sonra 0'a sarılır. Sıra sarması 128 baytlık host testinde denenir.

Kaynak: [Linux Kernel ISO-TP belgesi](https://kernel.org/doc/html/latest/networking/iso15765-2.html).

## Bağlantı ve yükleme

Yeni parça gerekmiyor. EXP14 bağlantıları aynı:

| Kart | Tek dosyalık sketch | Ek bağlantı |
|---|---|---|
| Uno A | [sender.ino](../../firmware/exp15/sender/sender.ino) | D2–buton–GND: aktarımı başlat |
| Uno B | [receiver.ino](../../firmware/exp15/receiver/receiver.ino) | D2–buton–GND: mod; D7–330 ohm–LED anot, katot–GND |
| Nano | [observer.ino](../../firmware/exp15/observer/observer.ino) | Listen-only gözlemci |

Üç MCP2515: 8 MHz kristal, CAN 125 kbit/s. Her kartta CS D10, MOSI D11,
MISO D12, SCK D13; INT boş. 5V uyumlu modüllere 5V, ortak GND, CANH–CANH,
CANL–CANL; yalnız iki fiziksel uçta 120 ohm. Bağlantıyı enerji kesikken değiştir.
Pot kullanılmaz. Butonlar INPUT_PULLUP kullanır. Kodları ayrı geçici sketch'lere
tam olarak kopyalayabilirsin. Üç kartı yükle, monitörleri 115200 baud aç.

## CAN kimlikleri ve baytlar

| Yön | ID | İçerik |
|---|---|---|
| Uno A → Uno B | 0x380 | SF / FF / CF |
| Uno B → Uno A | 0x381 | FC |

Buradaki iki kimlik deneyin adres çiftidir. 0x381 bir uygulama başarı yanıtı
olarak kullanılmaz; FC, veri akışını yönetir. ISO-TP aktarımının bitmesi,
uygulamanın mesajı anlamlandırıp istenen işi yaptığına dair ayrı onay değildir.

| İlk bayt | Tür | Devamı |
|---|---|---|
| 0x03 (örnek) | SF, uzunluk 3 | Üç veri baytı ve padding |
| 0x10 0x18 | FF, toplam uzunluk 24 | Altı veri baytı |
| 0x21, 0x22, 0x23 | CF, sıra 1,2,3 | En fazla yedi veri baytı |
| 0x30 | FC Continue To Send | BS, STmin ve padding |

FF uzunluğu 12 bit olarak okunur. 128 bayttan büyük uzunluk eğitim alıcısında
CAPACITY hatasıyla reddedilir; standart Overflow FC üretimi bu sürümde yoktur.
Son CF'nin mesaj uzunluğunu aşan baytları padding'dir; ana metne eklenmez.

## Modlar

| B modu | Block Size | STmin | Beklenen |
|---|---:|---:|---|
| 0 | 2 | 50 ms | FF sonrası FC, iki CF, yeni FC, son CF |
| 1 | 1 | 100 ms | Her CF'den önce FC izni; daha uzun asgari bekleme |
| 2 | — | — | FC hiç gönderilmez; A FF sonrası bekler ve timeout olur |

BS, bir FC izniyle en fazla kaç CF gönderileceğini belirtir; bayt veya mesaj
uzunluğu değildir. FF blok sayımına dahil değildir. BS=0, kalan CF'ler için
yeni FC beklenmemesini ifade eder; host testinde denenir.
STmin, ardışık CF'ler arasındaki minimum ayrımdır; kesin periyot değildir.
Host yükü, CAN arbitration veya yazılım gecikmesi aralığı uzatabilir.

STmin 0x00–0x7F milisaniye biçimi desteklenir. 0xF1–0xF9 alt milisaniye
biçimi, WAIT/Overflow FC ve diğer FC durumları bu göndericide desteklenmez;
UNSUPPORTED_FC ile oturum durdurulur. Bu sınırlar dışarıdaki bir ISO-TP
uygulamasıyla genel birlikte çalışabilirlik iddiasında bulunmamızı engeller.

## Test 1 — Normal akış

B MODE=0 ile açılır. A butonuna bas. Nano'da şu tür sırası beklenir:

```text
0x380: 10 18 ...       FF: toplam 24 bayt
0x381: 30 02 32 ...    FC: BS=2, STmin=50 ms (0x32)
0x380: 21 ...          CF1
0x380: 22 ...          CF2
0x381: 30 02 32 ...    FC: sonraki blok izni
0x380: 23 ...          CF3: kalan 4 veri baytı + 3 padding
```

B çıktısı (temsili; ölçüm değildir):

```text
COMPLETE length=24 count=1
MESSAGE=ASTRID ISO MESSAGE 00001
```

LED yanar; sonraki başarılı aktarımda söner. A `TX_DONE` yazar. Bu, TX0IF
üzerinden bütün CAN çerçevelerinin iletiminin tamamlandığını gösterir; uygulama
başarı onayı değildir. B'nin COMPLETE kaydıyla ayrı karşılaştır.

## Test 2 — Blok boyutu ve bekleme süresi

B butonuna bir kez bas: MODE=1. A'ya tekrar bas.
Bu kez FC baytları `30 01 64` olmalı: BS=1, STmin=100 ms.
Sıra FF, FC, CF1, FC, CF2, FC, CF3 olur. Metin yine eksiksiz birleşmeli.
Her CF için ayrı FC izni ve daha uzun aralık gözlenir.

Nano zaman damgası seri uygulamanın çerçeveyi okuduğu andır; tel üstünde
mikrosaniye hassasiyetinde STmin ölçümü veya uygunluk testi değildir.

## Test 3 — Flow Control yok

Aktarım tamamlandıktan sonra B'ye tekrar bas: MODE=2. A'ya bas.
Nano'da FF görülmeli; bu aktarım için FC ve CF görülmemeli.
Yaklaşık bir saniye sonra A `FAILED error=1` (FC_TIMEOUT), B `RX_ERROR=1`
(TIMEOUT) yazar. Önceki tamamlanmış mesaj ve LED korunur.

Hata sonrası gönderici yeni oturum başlatmaz: tüm kartları resetle, MODE=0
normal testi tekrarla. Böylece fiziksel olarak bekleyen gönderim veya eski
oturumdan bir çerçevenin yeni oturuma karışmasıyla bu testi sürdürmeyiz.
Bu bir kalıcı oturum/güvenilir yeniden bağlanma çözümü değildir.

## Uygulama ayrıntıları ve sınırlar

- Tek yön, tek aktif oturum; alıcıda mod değişimi aktif aktarımda engellenir.
- Gönderici/alıcı yalnız TXB0 kullanır. API dönüşünden sonra TX0IF beklenir.
  STmin sayımı önceki çerçevenin tamamlandığının yazılımda görüldüğü andan
  başlar; millis çözünürlüğü için 1 ms ek pay vardır. İlk CF de konservatif
  olarak önceki FF tamamlanmasından itibaren bu aralığı bekler.
- Göndericide TX tamamlanması ve FC beklemesi için 1000 ms; alıcıda ilerleme
  veya FC iletimi beklemesi için 1000 ms deney süreleri kullanılır.
  Bunlar kapsamlı standart zamanlayıcı uygulaması değildir.
- TX timeout, MCP2515'in bekleyen çerçevesini iptal etmez. Bu nedenle hata
  sonrası reset gerekir; kablo sökerek hata üretmek bu deneyin adımı değildir.
- Alıcı beklerken yanlış CF sırası veya yeni SF/FF gelirse mevcut birleştirme
  iptal edilir. Son tamamlanmış mesaj yalnız yeni eksiksiz mesajda güncellenir.
- Alıcı yalnız DLC8 kabul eder, padding içeriğini doğrulamaz. Gelen CF'nin
  minimum aralığını bağımsız olarak denetlemez. SF, BS0 ve sıra sarması hostta
  test edildi; mevcut buton akışı bunları donanımda üretmez.
- FC'de işlem numarası yoktur. Eski/geç çerçeveler ve düğüm resetleri için
  kalıcı oturum yönetimi yoktur. CAN ACK ile FC aynı şey değildir.
- UDS servisleri veya başka bir teşhis protokolü uygulanmaz. Burada uygulama
  mesajı, taşıma katmanını incelemek için kullanılan düz metindir.
- RX_OVERFLOW görüldüğünde gözlem eksik olabilir. Nano listen-only çalışır.

Gönderici hata kodları: 1 FC_TIMEOUT, 2 TX_TIMEOUT, 3 BAD_FC, 4 UNSUPPORTED_FC.
Alıcı hata kodları: 1 TIMEOUT, 2 FORMAT, 3 SEQUENCE, 4 CAPACITY, 5 UNEXPECTED.

## Öğrenme kontrolü

1. FF zaten altı veri baytı taşıyorken gönderici neden hemen tüm CF'leri yollamıyor?
2. `30 02 32` içindeki 02 ve 32 neyi belirliyor?
3. CF2'den sonra neden bir FC daha gerekiyor, CF3'ten sonra neden gerekmiyor?
4. CAN ACK, FC izni ve uygulama başarı yanıtı hangi farklı bilgileri veriyor?

## Doğrulama

| Hedef | Flash | RAM |
|---|---:|---:|
| Uno A | 7530 | 398 |
| Uno B | 6272 | 508 |
| Nano | 4074 | 217 |

Arduino AVR 1.8.7 ile üç sketch derlendi. Gerçek sketch sınıflarını kullanan
`tests/exp15_isotp_test.cpp`: 1/7/8/24/128 bayt, SF/FF/CF/FC, BS0/1/2,
STmin sınırı, sıra sarması/hatası, timeout/millis sarması, kısa/uygunsuz FC,
kapasite ve toparlanma denemelerini ASan/UBSan ile geçti. Bu host testi
MCP2515'i, fiziksel CAN hattını veya başka bir ISO-TP yığınıyla uyumluluğu test etmez.
Donanım için kullanıcının genel tamamlanma bildirimi alındı. Mod bazında
ayrı seri kayıtlar arşivlenmedi; yukarıdaki beklenen çıktılar ölçüm kaydı değildir.

```sh
c++ -std=c++11 -Wall -Wextra -pedantic -fsanitize=address,undefined tests/exp15_isotp_test.cpp -o /tmp/exp15_test
/tmp/exp15_test
```
