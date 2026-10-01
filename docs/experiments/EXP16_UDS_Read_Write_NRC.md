# EXP16 — UDS: ayar okuma, yazma ve negatif yanıt

Üç sketch derlendi; servis mantığı ASan/UBSan host testini geçti.
1 Ekim 2026: Kullanıcı tüm deneyin tamamlandığını bildirdi; ayrı seri kayıt aktarılmadı.

## Amaç

CAN ACK, ISO-TP taşıması ve UDS uygulama yanıtını ayırmak. Mesaj doğru
ulaşmış olsa da istenen değer geçersiz olabilir. ECU o zaman ayarı değiştirmeden
negatif yanıt verir. Timeout ise bir ret yanıtı değildir: süresinde yanıt
alınmadığı için sonuç bilinmez.

UDS (Unified Diagnostic Services) bir teşhis uygulama protokolüdür. Bu deney
iki servisin sınırlı eğitim uygulamasıdır; tam ISO 14229 uyumluluğu iddiası yoktur.

| Servis | İstek SID | Pozitif yanıt SID |
|---|---|---|
| ReadDataByIdentifier | 0x22 | 0x62 |
| WriteDataByIdentifier | 0x2E | 0x6E |

Negatif yanıt: `7F, istek_SID, NRC`. NRC, ret nedenini belirten koddur.
DID, okunacak/yazılacak verinin kimliğidir; CAN ID veya SID ile aynı kavram değildir.
Bu laboratuvarda DID **0x1234**, 0–100 arası bir baytlık örnek ayarı temsil eder.
Bu anlam deneyimize aittir; herhangi bir araçtaki DID anlamını tanımlamaz.
Ayar başlangıçta 25, RAM'de tutulur; resetle 25'e döner. Bir sıcaklık sensörü
veya gerçek araç ayarı değildir. LED ayar >=50 olduğunda yanar.

## Kaynaklar

- [udsoncan ReadDataByIdentifier](https://udsoncan.readthedocs.io/en/latest/_modules/udsoncan/services/ReadDataByIdentifier.html)
- [udsoncan WriteDataByIdentifier](https://udsoncan.readthedocs.io/en/latest/_modules/udsoncan/services/WriteDataByIdentifier.html)
- [UDS response biçimleri](https://uds.readthedocs.io/en/latest/pages/knowledge_base/diagnostic_message.html)

## Bağlantı ve yükleme

EXP15 bağlantıları korunur; yeni parça gerekmez.

| Kart | Kod | Ek bağlantı |
|---|---|---|
| Uno A (tester) | [tester.ino](../../firmware/exp16/tester/tester.ino) | D2–buton–GND: sıradaki isteği gönder |
| Uno B (ECU benzetimi) | [ecu.ino](../../firmware/exp16/ecu/ecu.ino) | D2–buton–GND: yazma koşulunu değiştir; D7–330 ohm–LED anot, katot–GND |
| Nano | [observer.ino](../../firmware/exp16/observer/observer.ino) | Listen-only gözlemci |

Üç 8 MHz MCP2515, CAN 125 kbit/s; CS D10, MOSI D11, MISO D12, SCK D13,
INT boş. 5V uyumlu modüllere 5V, ortak GND, CANH–CANH ve CANL–CANL;
yalnız iki fiziksel uçta 120 ohm sonlandırma. Bağlantıları enerji kesikken yap.
Butonlar INPUT_PULLUP ve 30 ms debounce kullanır. Pot kullanılmaz.
Her kod tek dosyadır; ayrı geçici sketch'e tamamını kopyalayabilirsin.
Üç karta da yükle; seri monitörler 115200 baud. Nano'nun çalışan processor
ayarını koru. Monitörleri testten önce aç; test ortasında açmak kartı resetleyebilir.

## Taşıma ve baytların anlamı

CAN kimlikleri: istekte **0x700**, yanıtta **0x708**. İkisi de standart veri
çerçevesi; bunlar deneyin seçilmiş adres çiftidir. Fonksiyonel yayın yoktur.

Tüm istek/yanıtlar ISO-TP **Single Frame** içine sığar. Bu nedenle bu deneyde
FC görülmez. DLC daima 8, kalan baytlar sıfırla doldurulur. İlk bayt SF PCI:
UDS mesajının uzunluğunu taşır. Sonraki bayt UDS SID'dir.

Örnek okuma:

```text
0x700: 03 22 12 34 00 00 00 00
0x708: 04 62 12 34 19 00 00 00
```

İstekte `03` = üç bayt UDS verisi, `22` = oku, `12 34` = DID 0x1234.
Yanıtta `04` = dört bayt UDS verisi, `62` = olumlu okuma yanıtı,
`19` = hexadecimal 0x19, yani decimal 25. DID bayt sırası **big endian**.

Geçerli 60 yazma ve yanıt:

```text
0x700: 04 2E 12 34 3C 00 00 00
0x708: 03 6E 12 34 00 00 00 00
```

0x3C decimal 60. Yazma yanıtı DID'yi tekrarlar; yazılan değeri taşımaz.
Değeri doğrulamak için sonraki adımda 0x22 ile geri okuruz.

Geçersiz 200 yazma:

```text
0x700: 04 2E 12 34 C8 00 00 00
0x708: 03 7F 2E 31 00 00 00 00
```

0xC8 decimal 200. `7F 2E 31`: yazma isteği RequestOutOfRange nedeniyle
reddedildi. ISO-TP mesajı doğru taşındı; ayar değişmedi.

## Test A — Sekiz basışlık sıra

B'ye henüz basma: WRITES_ALLOWED=1 ile açılır. Her A basışından sonra
UDS_POSITIVE/UDS_NEGATIVE yanıtını görüp sonraki basışa geç.

| A basışı / CASE | İstek | İlk tur beklenen sonuç |
|---|---|---|
| 1 | Ayarı oku | UDS_POSITIVE, value=25 |
| 2 | Ayarı 60 yap | UDS_POSITIVE, LED yanar |
| 3 | Ayarı oku | value=60 |
| 4 | Ayarı 200 yap | UDS_NEGATIVE NRC=0x31; LED/ayar korunur |
| 5 | Ayarı oku | Hâlâ value=60 |
| 6 | Tanımsız DID 0x1235 oku | NRC=0x31 |
| 7 | Yazma isteğinde değer baytını eksik gönder | NRC=0x13 |
| 8 | Desteklenmeyen SID 0x99 gönder | NRC=0x11 |

Sekizden sonra CASE=1'e döner. Resetlemediysen artık ilk okuma 60 verir;
her turda 25 bekleme. İlk turu aynen tekrarlamak için tüm kartları resetle.
CASE=7 geçerli bir ISO-TP SF taşır; yanlış olan içindeki UDS mesaj uzunluğudur.
Bozuk taşıma çerçevesiyle UDS negatif yanıtını birbirine karıştırma.

Nano ham hexadecimal baytları gösterir. A'da `CAN_TX_DONE` ile
`UDS_NEGATIVE` aynı istek için görülebilir: iletim tamamlanmış, uygulama isteği
reddetmiştir. Bu bir çelişki değildir. CAN_TX_DONE da hedef uygulama onayı değildir.

## Test B — Koşullar uygun değil

Tüm kartları resetle. B butonuna bir kez bas: `WRITES_ALLOWED=0`.
A'ya bas: CASE=1 okuması yine 25 vermeli.
A'ya tekrar bas: CASE=2'de normalde geçerli olan 60 yazma isteği bu kez
`7F 2E 22` ile reddedilmeli. CASE=3 geri okuması hâlâ 25 olmalı, LED sönük kalmalı.

Burada 0x22 NRC=ConditionsNotCorrect; request SID=0x22 ile aynı sayısal
değer olsa da farklı alandadır. B'ye yeniden basmak yazma koşulunu açar.
Bu buton güvenlik doğrulaması/UDS SecurityAccess değildir; yalnız uygulama
koşulunu canlandırır. Sonraki 60 yazma denemesi için akışı dolaştırabilir
veya tüm kartları resetleyebilirsin.

## Ret kodları

| NRC | Ad | Deneydeki nedeni |
|---|---|---|
| 0x11 | ServiceNotSupported | SID 0x99 uygulanmadı |
| 0x13 | IncorrectMessageLengthOrInvalidFormat | Yazma değeri eksik |
| 0x22 | ConditionsNotCorrect | B'de yazma koşulu kapalı |
| 0x31 | RequestOutOfRange | Değer 100'den büyük veya DID tanımsız |

## Sınırlar

- Yalnız bir DID'li okuma ve bir bayt değerli yazma; çoklu DID okuma yoktur.
- Yalnız padded DLC8 SF kabul edilir. FF/CF, session control, SecurityAccess,
  TesterPresent, ResponsePending (0x78), P2/P2* anlaşması ve kalıcı kayıt yoktur.
- İstek yanıt bekleme süresi deney için 500 ms. Timeout'ta sonuç bilinmez;
  otomatik yazma tekrarı yapılmaz ve tester resetlenene kadar yeni istek engellenir.
  Yeniden denemeden tüm kartları resetle.
- Negatif yanıtta SID, pozitif yanıtta SID/DID/uzunluk kontrol edilir. Genel
  oturum/işlem kimliği yoktur; geç yanıtlar için üretim düzeyinde yönetim değildir.
- ECU isteği işledikten sonra yanıt gönderimi başarısız olabilir. Bu nedenle
  timeout, ayarın değişmediğini kanıtlamaz. Donanım kuyruğu timeout ile iptal olmaz.
- Ret önceliği bu uygulamada SID → uzunluk → DID → yazma koşulu → değer
  sırasındadır. Aynı istekte birden fazla hata varsa tüm ECU'lar için genel
  NRC önceliği sonucu çıkarma.
- D7 yazılım ayarının >=50 olduğunu gösterir; fiziksel çıkış geri bildirimi yoktur.
- RX_OVERFLOW gözlem kaybı anlamına gelebilir. Nano listen-only, ACK üretmez.

## Öğrenme kontrolü

1. `03 7F 2E 31` içindeki her bayt hangi katmana/alanına ait?
2. CAN_TX_DONE görülürken UDS_NEGATIVE görülmesi neden çelişki değil?
3. 200 yazma reddedildikten sonra geri okuma neden 60 veriyor?
4. Neden bu deneyde FC yok? Mesaj büyüse hangi katman devreye girer?

## Doğrulama

| Hedef | Flash | RAM |
|---|---:|---:|
| Uno A tester | 5778 | 414 |
| Uno B ECU | 5208 | 229 |
| Nano observer | 4070 | 217 |

Arduino AVR 1.8.7 ile üç derleme geçti. `tests/exp16_uds_test.cpp` gerçek
servis sınıfını kullanır: doğru yanıt baytları, 0/100 sınırları, 101/200/255
reddi, NRC 11/13/22/31, eksik/uzun mesaj ve ret sonrası ayarın korunması.
ASan/UBSan testi geçti. Fiziksel CAN, tester'ın zamanlaması ve harici UDS
istemcisiyle uyumluluk bu host testinin kapsamında değil. Kullanıcı donanım
deneyinin tamamlandığını bildirdi; mod bazında ayrı loglar arşivlenmedi.

```sh
c++ -std=c++11 -Wall -Wextra -pedantic -fsanitize=address,undefined tests/exp16_uds_test.cpp -o /tmp/exp16_test
/tmp/exp16_test
```
