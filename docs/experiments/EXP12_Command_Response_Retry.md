# EXP12 — Komut, uygulama yanıtı ve tekrar gönderme

23 Eylül 2026: Üç sketch Arduino AVR 1.8.7 ile derlendi. İşlem önbelleği
host testi geçti.
25 Eylül 2026: Kullanıcı deneyin tamamlandığını ve ayrı sohbette ayrıntılı
incelendiğini bildirdi. O sohbetin içeriği ve ayrı seri kayıtlar bu kayda aktarılmadı.

## Amaç

CAN ACK, en az bir denetleyicinin çerçeveyi doğru aldığını gösterir; hedef
uygulamanın komutu işlediğini kanıtlamaz. Burada uygulama ayrı bir CAN veri
çerçevesiyle yanıtlar. RTR veya CAN ACK bitini yazılımla göndermiyoruz.
Yanıt kaybolduğunda aynı işlem numarasıyla tekrar deniyoruz.

Kaynak: [Kvaser CAN Messages](https://kvaser.com/lesson/can-messages/).

## Donanım ve yükleme

Mevcut iki Uno, bir klasik Nano, üç 8 MHz MCP2515 yeterli. CAN 125 kbit/s.
Her kart: CS D10, MOSI D11, MISO D12, SCK D13; INT kullanılmaz. Mevcut
5V uyumlu modüllerde VCC 5V, ortak GND; CANH–CANH, CANL–CANL. Yalnız hattın
iki fiziksel ucunda 120 ohm sonlandırma bulunmalı.

Enerjiyi keserek şu ek bağlantıları yap:

| Kart | Sketch | Ek bağlantı |
|---|---|---|
| Uno A | [sender.ino](../../firmware/exp12/sender/sender.ino) | D2–buton–GND: yeni komut |
| Uno B | [receiver.ino](../../firmware/exp12/receiver/receiver.ino) | D2–buton–GND: yanıt modu; D7–330 ohm–LED anot, LED katot–GND |
| Nano | [observer.ino](../../firmware/exp12/observer/observer.ino) | Ek parça yok; listen-only gözlemci |

Butonlar INPUT_PULLUP kullanır, harici pull-up gerekmez. Önceki servo/LCD
bu deneyde kullanılmaz. D13 SPI saat hattı olduğu için durum LED'i D7'dedir.
Her sketch tek dosyadır; tamamını ayrı geçici Arduino sketch'ine kopyalayabilirsin.
Seri monitörler 115200 baud. Kart/port eşleşmesini doğrula; Nano'da son çalışan
processor/bootloader ayarını kullan. Test sırasında seri monitörü tekrar açmak
kartı resetleyebilir: tüm monitörleri baştan aç, sonra tüm kartları resetle.

## Mesaj sözleşmesi

İkisi de standart veri çerçevesi, DLC 8.

| Bayt | İstek 0x320 | Yanıt 0x321 |
|---|---|---|
| 0 | 0xC1 | 0xD1 |
| 1 | Protokol sürümü 1 | Protokol sürümü 1 |
| 2–5 | uint32 işlem numarası, little endian | Aynı işlem numarası |
| 6 | İstenen LED durumu: 0/1 | Yazılımın uyguladığı LED durumu: 0/1 |
| 7 | Ayrılmış, 0 | 0 APPLIED, 1 DUPLICATE, 2 CONFLICT |

Komut SET'tir: LED'i açık/kapalı yap. Gönderici her başarılı işlemden sonra
sonraki basışta ters durumu ister. Alıcı aynı işlem numarası ve içerik tekrar
ulaşınca çıkışı yeniden uygulamaz; önbellekteki durumla DUPLICATE yanıtı verir.
Aynı numara farklı durumla gelirse CONFLICT üretir. applied_total yalnız yeni
kabul edilen işlemlerde artar. Yanıt fiziksel LED akımını ölçmez; uygulamanın
D7'ye yazdığı durumu bildirir.

## Zamanlama

Aynı anda yalnız bir işlem beklenir. Her denemeden sonra 200 ms yanıt süresi;
ilk gönderim dahil en fazla üç deneme. Her deneme aynı numara ve içeriği taşır.
Yaklaşık 600 ms sonunda eşleşen yanıt yoksa UNKNOWN olur ve yeni komutlar
engellenir. UNKNOWN, komutun uygulanmadığını söylemez. Geç/ilgisiz yanıtlar
başarıya dönüştürülmez. Yeniden başlamak için tüm kartları resetle.

TX_API_ERROR veya TX_BUSY başarı demek değildir. sendMessage dönüşü uygulama
onayı değildir. Fiziksel hat kesintisinde MCP2515'in donanımsal yeniden iletimi
sürebilir; uygulama timeout'u bekleyen donanım gönderimini iptal etmez.
Bu deneyde CAN kablolarını çıkarmadan yalnız yanıt üretimini bastırıyoruz.

## Adım adım test

Önce sonucu tahmin et, sonra seri çıktı ve LED ile karşılaştır.

1. **Normal:** B başlangıçta MODE=0. A butonuna bir kez bas.
   B LED'i yanar; A `CONFIRMED tx=1 LED=1 result=APPLIED` yazar.
   Nano bir REQ ve bir RSP görür. İkinci basışta tx=2, LED=0 olur.
2. **İlk yanıt kayıp:** B butonuna bir kez bas, MODE=1 olsun. A'ya bas.
   B ilk isteği uygular ama `REPLY_SUPPRESSED` yazar. Yaklaşık 200 ms sonra
   A aynı işlem numarasıyla tekrar dener. B result=1 döner, applied_total
   ilk isteğe göre ikinci kez artmaz. A `CONFIRMED ... result=DUPLICATE` yazar.
3. **Tüm yanıtlar kayıp:** B'ye bir kez daha bas, MODE=2. A'ya bas.
   LED ilk istekte değişir; Nano aynı numarayla üç REQ ve sıfır RSP görür.
   B applied_total yalnız bir artar. A `UNKNOWN` yazar. A'ya tekrar basınca
   `BLOCKED` görülür. B CAN normal modda kalır, CAN ACK işlevi sürer;
   bastırılan şey uygulamanın 0x321 yanıtıdır.
4. **Yeniden başlat:** Tüm kartları resetle. B MODE=0, LED kapalı, A tx=0
   başlangıcına döner. A butonuyla normal testi tekrarla.

Nano başlığı: `ms,kind,transaction,LED,result (255=request)`.
REQ satırında 255 sadece seri çıktı yer tutucusudur; hatta 255 gönderilmez.
RSP sonucu 0/1/2'dir. RX_OVERFLOW görülürse gözlem kaydı eksiktir.

Örnek MODE=1 akışı (temsili, ölçüm değildir):

```text
1000,REQ,3,1,255
1200,REQ,3,1,255
1202,RSP,3,1,1
```

## Kapsam ve sınırlar

Önbellek yalnız son işlemi RAM'de tutar. Tek gönderici, tek bekleyen işlem,
sıralı oturum ve kartların bağımsız resetlenmemesi varsayılır. Eski işlemlerin
sonradan yeniden oynatılmasına veya güç kesintisine karşı genel exactly-once
garantisi sağlamaz. Bağımsız reset için oturum kimliği/kalıcı kayıt gerekir.
SET komutu aynı değere tekrar uygulansa da aynı son durumu verir; buna
idempotent işlem denir. Önbelleğin etkisi applied_total ile ayrıca gözlenir.

## Yazılım doğrulaması

| Hedef | Flash | RAM |
|---|---:|---:|
| Uno A | 5106 | 235 |
| Uno B | 4828 | 234 |
| Nano | 4160 | 217 |

`tests/exp12_transaction_test.cpp`, gerçek receiver sketch'indeki önbelleği
kullanarak kayıp yanıt sonrası tekrarları, değiştirilmiş içerik çakışmasını ve
sonraki işlemi kontrol eder. Donanım zamanlamasını veya fiziksel hattı test etmez.

```sh
c++ -std=c++11 -Wall -Wextra -pedantic tests/exp12_transaction_test.cpp -o /tmp/exp12_test
/tmp/exp12_test
```

Donanım sonuçları: Kullanıcının genel tamamlanma bildirimi alındı (25 Eylül
2026). Normal, ilk yanıt kaybı ve tüm yanıt kaybına ait ayrı loglar henüz
arşivlenmedi. Yukarıdaki örnek akış ölçüm kaydı değildir.
