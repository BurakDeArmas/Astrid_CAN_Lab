# EXP13 — RTR ile veri isteme

25 Eylül 2026: Üç sketch Arduino AVR 1.8.7 ile derlendi. 29 Eylül 2026: Kullanıcı deneyin tamamlandığını bildirdi; ayrı seri log aktarılmadı.

## Amaç

Klasik CAN'in remote frame (RTR) biçimini veri çerçevesinden ayırmak.
RTR çerçevesi veri taşımaz; DLC, istenen veri çerçevesinin uzunluğunu belirtir.
İstek ve veri aynı CAN kimliğini kullanır. Burada standart ID 0x350, DLC 4.
Bu mekanizma CAN FD'de yoktur; MCP2515 ile klasik CAN öğreniyoruz.

EXP12'deki işlem numarası ve komut içeriği RTR içine koyulamaz. Aynı ID'li
verinin istekten sonra görülmesi, verinin özellikle o isteğe yanıt olduğunu
kanıtlamaz. Üçüncü mod bu ayrımı görünür kılar.

Kaynaklar: [Kvaser çerçeve türleri](https://kvaser.com/canlib-webhelp/page_user_guide_can_frame_types_types.htm),
[Kvaser arbitration ve FD](https://kvaser.com/lesson/arbitration/),
[autowp kütüphane uygulaması](https://github.com/autowp/arduino-mcp2515/blob/master/mcp2515.cpp).

## Bağlantı ve yükleme

Mevcut iki Uno, Nano ve üç MCP2515 yeterli. 8 MHz kristal, CAN 125 kbit/s,
seri monitörler 115200 baud. CS D10, MOSI D11, MISO D12, SCK D13; INT boş.
5V uyumlu modüllerin beslemesi 5V, ortak GND, CANH–CANH ve CANL–CANL;
yalnız iki fiziksel uçta 120 ohm. Bağlantı değiştirirken enerjiyi kes.

| Kart | Tek dosyalık sketch | Ek bağlantı |
|---|---|---|
| Uno A | [requester.ino](../../firmware/exp13/requester/requester.ino) | D2–buton–GND: bir RTR isteği |
| Uno B | [responder.ino](../../firmware/exp13/responder/responder.ino) | D2–buton–GND: mod; 10k pot uçları 5V/GND, orta uç A0 |
| Nano | [observer.ino](../../firmware/exp13/observer/observer.ino) | Ek bağlantı yok, listen-only |

Pot kullanmayacaksan A0'ı GND'ye bağla; değer 0 olur. A0'ı boş bırakma.
EXP12'nin D7 LED'i bu deneyde kullanılmaz. Butonlar INPUT_PULLUP ve 30 ms
debounce kullanır. Kodların tamamı ayrı geçici sketch'lere kopyalanabilir.
Üç karta da EXP13 yükle; kart/port ve Nano'nun çalışan bootloader ayarını koru.

## Çerçeveler

| Alan | RTR isteği | Veri yayını |
|---|---|---|
| ID | STD 0x350 | STD 0x350 |
| RTR | 1 | 0 |
| DLC | 4 | 4 |
| Veri alanı | Yok | ADC uint16 LE + örnek sayacı uint16 LE |

ADC 0–1023 aralığında analogRead(A0) sonucudur; kalibre edilmiş voltaj değildir.
Örnek sayacı B'nin veri gönderim denemelerini sayar, 65535'ten sonra sıfıra
sarılır. İstek kimliği veya başarılı teslim sayacı değildir.

Kodda `VALUE_ID | CAN_RTR_FLAG` remote frame seçer. `can_dlc=4` olmasına
rağmen istek dört bayt veri taşımaz. Kütüphane alıcı veri register'larını
okuyabilir; RTR çerçevesinde `frame.data[]` alınmış veri değildir ve kullanılmaz.
Gözlemci bu nedenle RTR satırında yalnız `NO_PAYLOAD` yazar.

Yanıt bu deneyde Uno B uygulamasının `publishValue()` çağrısıyla üretilir.
CAN ACK biti kendi başına veri yayını oluşturmaz. `sendMessage` sonucunun
ERROR_OK olması tamamlanmış iletim veya uygulama yanıtı kanıtı değildir.

## Modlar ve deney adımları

### 1. MODE=0: yalnız RTR gelince veri

Başlangıç modu 0. Butonlara basmadan Nano'da veri akışı olmamalı.
Potu çevir; Uno A butonuna bir kez bas:

- A: `RTR_SUBMITTED`, sonra `DATA_DURING_WAIT ADC=... sample=...`.
- B: `RTR_RECEIVED dlc=4`, sonra `DATA_ATTEMPT ...`.
- Nano: aynı ID ile önce RTR, ardından DATA.

Temsili çıktı (ölçüm değildir):

```text
1000,RTR,350,4,NO_PAYLOAD
1002,DATA,350,4,00 02 00 00
```

DATA içindeki `00 02`, little endian ADC=512; `00 00`, örnek sayacı=0.
İkinci basışta yeni örnek gelir. İstek tarafında otomatik tekrar yoktur.

### 2. MODE=1: uygulama isteği görür ama yayın yapmaz

B butonuna bir kez bas. A'ya basınca Nano yalnız RTR görmeli.
B `RTR_IGNORED_BY_APPLICATION`, A yaklaşık 500 ms sonra
`NO_DATA_IN_WINDOW (500ms)` yazar. B normal CAN modundadır; uygulamanın
isteği görmezden gelmesi CAN ACK mekanizmasını kapatmaz. Kabloları çıkarmıyoruz.
Bu aşama fiziksel ACK ölçümü değildir.

### 3. MODE=2: istekten bağımsız periyodik yayın

B'ye bir kez daha bas. B artık her 250 ms'de veri yayınlar ve RTR'leri
uygulamada görmezden gelir. A butonuna hiç basmadan Nano DATA satırları,
A ise `DATA_WITHOUT_WAIT` gösterir.

Şimdi A butonuna bas. Sonraki periyodik veri bekleme penceresine denk gelince
A `DATA_DURING_WAIT` yazar. B'nin logu isteği görmezden geldiğini gösterir.
Bu nedenle A çıktısına `CONFIRMED` veya `REPLY_MATCHED` demiyoruz: RTR'de
uygulamaya ait işlem numarası yoktur. Bekleme sırasında veri görmek yalnız
zaman ilişkisini gösterir, neden ilişkisini göstermez.

B'ye tekrar basarak MODE=0'a dön. Önceden kuyruğa girmiş bir veri gelebilir;
geçiş sonrası bir saniye bekle, sonra normal testi tekrarla.

## Sınırlar ve öğrenme kontrolü

- 500 ms, yazılımın isteği kuyruğa verme anından başlar; tel üzerindeki
  gönderim veya alım zamanı ölçülmez. RX tamponunda bekleyen veri de olabilir.
- Fiziksel hat kesilirse donanım yeniden iletimi sürebilir; yazılım timeout'u
  MCP2515 kuyruğunu iptal etmez. Bu deney yalnız uygulama modlarını değiştirir.
- Bir kimlik için tek veri üreticisi kullanılır. CAN ID doğrudan düğüm adresi değildir.
- RX_OVERFLOW varsa gözlem eksiktir. Nano listen-only olduğu için ACK üretmez.
- Aynı standart ID'li DATA ve RTR eşzamanlı arbitration'a başlarsa DATA'nın
  dominant RTR biti üstün gelir. Burada eşzamanlı başlangıç oluşturulmadığı
  için seri monitör sırası arbitration ölçümü olarak sunulmaz.

Deney sonunda kendi sözlerinle cevapla:

1. RTR'de DLC 4 iken neden dört veri baytı göstermiyoruz?
2. İstek ve veriyi ID aynı olmasına rağmen hangi alanla ayırıyoruz?
3. MODE=2'de DATA_DURING_WAIT neden istek–yanıt eşleşmesini kanıtlamıyor?
4. LED komutuna işlem numarası eklemek için neden EXP12'deki veri çerçevesi gerekliydi?

## Doğrulama kaydı

| Hedef | Flash (bayt) | RAM (bayt) |
|---|---:|---:|
| Uno A requester | 4992 | 228 |
| Uno B responder | 4916 | 230 |
| Nano observer | 4176 | 217 |

Üç hedef derlendi. Kod incelemesinde RTR verisinin okunmadığı, çerçeve
bayraklarının tam ID karşılaştırmasıyla ayrıldığı ve periyodik yayının
istek işleme yolundan bağımsız olduğu kontrol edildi. Donanım için genel tamamlanma bildirimi alındı; fiziksel RTR gönderimi,
üç mod ve pot okumasına ait ayrı ölçüm kayıtları arşivlenmedi.
