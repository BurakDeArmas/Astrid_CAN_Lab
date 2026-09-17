# EXP07 — Donanım kabul filtreleri ve maskeler

Üç sketch 12 Eylül 2026'da Arduino AVR 1.8.7 ile derlendi. Kullanıcı 17 Eylül 2026 tarihinde deneyin tamamlandığını bildirdi.
Uno A/B: 4610 bayt flash, 248 bayt statik RAM; Nano: 5672/273 bayt.

## Kurulum

EXP06'nın iki Uno, klasik 5V Nano ve üç MCP2515 düzeni korunur. Her kartta
CS D10, MOSI D11, MISO D12, SCK D13; 5V uyumlu modül için VCC 5V, GND ortak.
INT boş. CAN H/H ve L/L omurgası, yalnız iki fiziksel uçta 120 ohm sonlandırma.
Kristal her modülde kontrol edilmeli; varsayılan MCP_8MHZ ve CAN_125KBPS.
Servo beslemesi kapalı; LCD bu deneyde kullanılmaz.

| Kart | Tek dosyalı sketch | Yayın |
|---|---|---|
| Uno A | firmware/exp7/sender_a/sender_a.ino | 0x100, 0x101, 0x10F |
| Uno B | firmware/exp7/sender_b/sender_b.ino | 0x200, 0x201, 0x20F |
| Nano | firmware/exp7/receiver/receiver.ino | Yayın yok; alır ve raporlar |

Üç karta da EXP07 yükle. Geçici sketch'e tüm kod kopyalanabilir.
autowp MCP2515 kütüphanesi kullanılır. Seri monitör 115200 baud.
Her gönderici sırayla üç ID'yi dolaşır; toplam hedef 30 mesaj/s, ID başına
yaklaşık 10 mesaj/s. EXP06 gönderici butonları bu kodda kullanılmaz.

Nano D2–buton–GND: her basışta mod 0→1→2→3→0. INPUT_PULLUP, 30 ms debounce.
Mod değişiminde MCP2515 sıfırlanıp yeniden yapılandırılır; bekleyen çerçeveler
atılır ve ölçüm penceresi yeniden başlar. Bu geçiş sırasında kesinti vardır.

## Filtre nasıl çalışır?

Standart ID karşılaştırması: `(gelen_ID & maske) == (filtre & maske)`.
Maskede 1 olan bit karşılaştırılır, 0 olan bit önemsenmez.

| Mod | İşlem | Maske | Filtre | Beklenen read/kept/sw_drop (yaklaşık /s) |
|---|---|---|---|---|
| 0 | Tüm standart ID'leri donanımda kabul | 0x000 | 0x000 | 60 / 60 / 0 |
| 1 | Yalnız 0x100 donanımda kabul | 0x7FF | 0x100 | 10 / 10 / 0 |
| 2 | 0x100–0x10F grubunu donanımda kabul | 0x7F0 | 0x100 | 30 / 30 / 0 |
| 3 | Tüm standart ID'leri oku; yazılımda yalnız 0x100 tut | 0x000 | 0x000 | 60 / 10 / 50 |

0x7FF bütün 11 ID bitini karşılaştırır. 0x7F0 alt dört biti serbest bırakır;
bu nedenle 0x100–0x10F aralığındaki 16 ID geçebilir. Deneyde bu gruptan üçü
yayınlandığı için yaklaşık 30 mesaj/s görürüz. 0x200 grubu elenir.

MCP2515'te iki maske ve altı filtre vardır. Bu deneyde iki maskeye aynı değer,
altı filtrenin hepsine aynı hedef yazılır; kullanılmayan bir filtre arka kapı
gibi tüm trafiği kabul etmesin. RXB0 rollover açık kalır. RXM=00 kullanılır;
geçerlilik kontrollerini de aşan receive-any debug modu kullanılmaz.

Bu deney yalnız standart ID'ler içindir. Filtre EXIDE=0 ve maskelerde MIDE=1
ile extended format donanımda dışlanır. Kütüphanenin standart maske API'si MIDE
bitini ayarlamadığından kod config modunda iki SIDL maske register'ını okuyup
MIDE bitini yazar. Veri baytı maskeleri sıfır kalır; payload filtrelenmez.
Extended ID ve remote frame ayrıntıları ayrı deneyde ele alınabilir.

Filtreleme CAN hattındaki trafiği azaltmaz; Nano'nun tamponuna ve SPI üzerinden
yazılıma taşınan mesajları azaltır. Filtre, başka düğümün yayın yapmasını
engellemez. Normal CAN modunda ACK ile uygulamanın kabulü aynı şey değildir.

## Çıktıyı okuma

Nano başlığı:

```text
ms,mode,read,kept,sw_drop,id100,id101,id10F,id200,id201,id20F,unexpected,ovr
```

- ms: rapor penceresi süresi; sayıları saniyeye çevirmek için 1000/ms ile çarp.
- read: donanımdan okunan tüm çerçeveler, yazılım elemesinden önce sayılır.
- kept: deney biçimi doğru olup uygulamanın tuttuğu mesajlar.
- sw_drop: yalnız mod 3'te yazılım tarafından elenen geçerli deney mesajları.
- id...: o ID için donanımdan okunan sayı; mod 3'te elenenler de burada görünür.
- unexpected: bilinmeyen ID/format veya yanlış deney payload'u.
- ovr: taşma bayrağının görüldüğü örnek sayısı; kayıp mesaj sayısı değildir.

Donanımın reddettiği çerçeveleri yazılım okuyamadığından kesin hw_drop sayacı
yoktur. Sürekli yayın yapan kaynakların çıktıları ve modlar arası karşılaştırma
ile davranışı sınarız. Kaynak logundaki submitted/busy/flagged toplamları
açılıştan beri birikir; submitted teslim onayı değildir.

## Test sırası

Her modda 5 saniye bekle ve Nano'dan birkaç satır kaydet. Mod değiştirme
sonrasındaki ilk pencereyi sınır etkileri nedeniyle tek başına değerlendirme.

| Test | Beklenen | Sonuç |
|---|---|---|
| Mod 0 | Altı ID sütunu yaklaşık 10, read yaklaşık 60 | Genel başarı bildirimi; ayrı log yok |
| Mod 1 | Yalnız id100 yaklaşık 10, diğer ID sütunları 0 | Genel başarı bildirimi; ayrı log yok |
| Mod 2 | id100/id101/id10F yaklaşık 10, 0x200 grubu 0 | Genel başarı bildirimi; ayrı log yok |
| Mod 3 | Altı ID okunur; read≈60, kept≈10, sw_drop≈50 | Genel başarı bildirimi; ayrı log yok |
| Tekrar mod 0 | Tüm ID'ler yeniden görünür | Genel başarı bildirimi; ayrı log yok |

Bu düşük hızdaki deneyde ovr ve unexpected sıfır beklenir. Burada taşma
azalması ölçüldüğü iddia edilmez; donanım/yazılım elemesi arasındaki okuma
sayısı farkı sınanır. Yük testi daha sonra aynı filtrelerle genişletilebilir.

## Donanım sonucu — 17 Eylül 2026

Kullanıcı EXP07 denemesinin tamamlandığını bildirdi. Her mod için ayrı seri
monitör çıktısı veya ölçüm paylaşılmadığından tablodaki sayılar beklenen
değerlerdir; ölçülmüş sonuç olarak sunulmaz. Donanımda tek ID/grup seçimi ve
yazılımda eleme deneyi genel kullanıcı başarı bildirimiyle kaydedildi.

## Kaynaklar

- [Microchip MCP2515 veri sayfası](https://ww1.microchip.com/downloads/aemDocuments/documents/APID/ProductDocuments/DataSheets/MCP2515-Family-Data-Sheet-DS20001801K.pdf), kabul filtreleri ve maskeler bölümü.
- [autowp MCP2515](https://github.com/autowp/arduino-mcp2515), setFilterMask/setFilter uygulaması.
