# EXP06 — CAN önceliği, trafik ve alıcı darboğazı

Durum: Üç sketch 11 Eylül 2026 tarihinde Arduino AVR 1.8.7 ile derlendi.
Trafik artırma, alıcı taşması ve düzeltilmiş takip ile toparlanma gözlendi.
Arbitration doğrudan ölçülmedi. Lojik analizör kullanılmadı.

| Hedef | Flash | Statik RAM |
|---|---|---|
| Uno A | 4862 bayt | 264 bayt |
| Uno B | 4864 bayt | 264 bayt |
| Nano (12 Eylül takip düzeltmesi) | 5622 bayt | 291 bayt |

## Kurulum

EXP04/05'in iki Uno + klasik Nano ve üç MCP2515 CAN omurgasını kullan.
Tüm GND'ler ortak; yalnız iki fiziksel uçta 120 ohm sonlandırma.
Üç kartta CS D10, MOSI D11, MISO D12, SCK D13; modül beslemesi 5V uyumlu
modül için 5V. INT boş. Her modülün kristalini kontrol et; kod 8 MHz ve
125 kbit/s kullanır. Servo beslemesini kes; bu deneyde servo/pot/LCD kullanılmaz.
Nano'nun eski LCD'si bağlı kalabilir ama ekran güncellenmez.

| Kart | Kod | İşlev |
|---|---|---|
| Uno A | firmware/exp6/sender_a/sender_a.ino | ID 0x100 gönderir |
| Uno B | firmware/exp6/sender_b/sender_b.ino | ID 0x500 gönderir |
| Nano | firmware/exp6/receiver/receiver.ino | İki kaynağı sayar |

Her sketch tek dosyadır; geçici sketch'e de kopyalanabilir.
Kütüphane: autowp MCP2515 (önceki deneylerle aynı). Seri monitör 115200 baud.
Üç karta da EXP06 yükle; eski deney mesajları birlikte kullanılmaz.

Her göndericide D2–buton–GND ekle. Her basışta seviye 0→1→2→3→0 döner;
30 ms debounce ve INPUT_PULLUP vardır. Nano D2–buton–GND isteğe bağlıdır:
basılı tutmak alıcı yazılımına her döngüde 20 ms gecikme ekler.

## Yayın seviyeleri

| Seviye | İstenen periyot | Tek kart hedefi |
|---|---|---|
| 0 | 100 ms | 10 mesaj/s |
| 1 | 10 ms | 100 mesaj/s |
| 2 | 2 ms | 500 mesaj/s |
| 3 | 0.5 ms | 2000 mesaj/s |

Bunlar zamanlayıcı hedefleridir; gerçekleşen hız değildir. İki kart da seviye
3 olduğunda istenen trafik 125 kbit/s hattın kapasitesini aşar. Meşgul
gönderim fırsatları atlanır; sonradan yakalamak için birikmiş iş kuyruğu yoktur.
SPI ve seri çıktı da yazılım hızını sınırlar. Yalnız TXB0 kullanılarak aynı
kaynağın üç tampon arasında yeniden sıralanması önlenir.

Her çerçeve standart 11 bit data frame ve DLC 8:
0=0xA6, 1=kaynak (1/2), 2–5=32 bit little-endian sıra numarası,
6=seviye, 7=0x5A. Sıra numarası TX isteği verildiğinde artar;
alıcı uygulamasının teslim onayı değildir.

## Öğreneceğimiz ayrım

CAN'da dominant 0, recessive 1'e baskındır. Aynı anda arbitration yapan
iki standart data frame için küçük ID daha önceliklidir:

```text
0x100 = 00100000000
0x500 = 10100000000
        ^ ilk farklı bitte 0x100 dominant gönderir
```

Arbitration kaybeden düğüm çerçeveyi bozmaz; normal modda yeniden gönderme
fırsatını bekler. Başlamış bir çerçeve sonradan gelen yüksek öncelikli mesaj
tarafından yarıda kesilmez. Burada göndericiler senkron başlatılmadığı için
her mesaj çifti arbitration yarışına girmez.

Bu deney arbitration kaybını doğrudan saymaz. Küçük ID'nin daha çok görünmesi
tek başına arbitration kanıtı değildir; yazılım hızı ve alıcı kayıpları da
sonucu etkiler. ID'leri iki gönderici kodunda karşılıklı değiştirerek gözlemi
tekrarlayabilirsin; aynı ID'yi iki farklı veri üreticisine verme.

## Seri çıktı

Gönderici: ms,id,level,submitted,busy,flagged,TEC,REC,EFLG.
submitted/busy/flagged açılıştan beri toplamdır; saniyelik miktar için ardışık
satırların farkını al. busy, TXB0 hâlâ doluyken yeni gönderim fırsatıdır;
CAN hata sayısı değildir. flagged, kütüphane çağrısının hata döndürmesidir;
arbitration veya kesin paket kaybı sayısı olarak yorumlanmaz. TEC/REC CAN
denetleyicisinin hata sayaçları, EFLG anlık onaltılık hata bayraklarıdır.

Nano (v2): window_ms,A_rx,B_rx,A_aged_missing,B_aged_missing,A_repeat,B_repeat,
A_late,B_late,A_old_or_reset,B_old_or_reset,ovr_samples,invalid,other,EFLG,REC,
A_pending,B_pending.

- rx: son rapor penceresinde alınan geçerli çerçeveler. Mesaj/s = rx*1000/window_ms.
- aged_missing: 32 sıra numaralık takip penceresinden görülmeden çıkan numaralar.
  Kesin fiziksel CAN kaybı değildir; daha eski mesaj sonradan gelirse geri alınmaz.
- pending: pencere içinde hâlâ gelebilecek boşluklar; anlık değerdir.
- late: pencere içinde sonradan gelen ve bir boşluğu dolduran mesajlar.
- repeat: pencere içinde daha önce görülmüş sıra numarası.
- old_or_reset: pencere dışındaki eski/geriye giden numara; reset teşhisi değildir.
  Referans geriye çekilmez. Gönderici resetinden sonra üç kartı yeniden başlat.
- ovr_samples: RX0OVR/RX1OVR görülüp temizlenen örnek sayısı. Kaybolan mesaj
  sayısı değildir; bir bayrak birden fazla kaybı temsil edebilir.
- invalid: tanınan ID'de biçim hatası; other: deney dışı ID/çerçeve.

İlk alınan mesaj referanstır; ondan önce kaybolanlar sayılamaz. Gönderici
resetinde sayaç sıfırlanır; ölçüm ortasında resetleme, yeni testte üç kartı
yeniden başlat. Nano normal modda ACK verir. ACK, MCU'nun mesajı okuyup
işlediğini kanıtlamaz: alıcı taşarken diğer düğümler de ACK verebilir.

Kesin hat doluluk yüzdesi ölçülmez. Bit stuffing, arbitration beklemeleri ve
hata/tekrar çerçeveleri sayılmadığı için yalnız mesaj sayısından kesin yüzde
çıkarmayacağız. Seri çıktı da gözlemlediğimiz sistemi bir miktar yavaşlatır.

## Adım adım deneme

1. Üç kartı aç, tümü seviye 0. Nano'da yaklaşık A_rx=10, B_rx=10 bekle.
   Pencere sınırında küçük farklar normaldir. Gap/overflow değerlerini kaydet.
2. İki göndericiyi birer basışla seviye 1'e al, 10 saniye izle.
3. İkisini seviye 2'ye al, 10 saniye izle; meşgul gönderimler/taşmalar başlayabilir.
4. Seviye 1'e dön (gerekirse reset). Nano D2'yi iki saniye tut: alıcı yavaşlar;
   ovr_samples, late ve aged_missing değişimini gözle. Bırakınca yeni pencerelerde düşmesini bekle.
5. İki göndericiyi seviye 3'e al. 10 saniye boyunca submitted farklarını,
   busy ve Nano sayılarını karşılaştır. Önceliğin tek başına kanıtı olarak sunma.
6. İstersen 0x100/0x500 ID'lerini iki kod arasında değiştirip yeniden yükle.
   Nano kaynak alanı doğrulamasını da yeni eşlemeye göre değiştirmek gerekir.
   İlk deneme için bu adımı atla; birlikte düzenleyebiliriz.

TEC/EFLG bus-off belirtirse bağlantı, bitrate, kristal ve sonlandırmayı kontrol
et; düzeltip resetle. Otomatik bus-off toparlanması bu deneyde uygulanmadı.
Her test için iki göndericinin seviyesi ve Nano'dan 5–10 satır log kaydet.
## İlk donanım gözlemleri ve takip düzeltmesi

Kullanıcının paylaştığı seri monitör görüntülerinde:
- Seviye 1 civarında kaynak başına yaklaşık 100 mesaj/s, hata göstergeleri sıfır.
- Seviye 2 denemesinde yaklaşık 483–486 mesaj/s/kaynak.
- Seviye 3 denemesinde yaklaşık 524–529 mesaj/s/kaynak.
- Nano yavaşlatıldığında taşma örnekleri ve geriye giden sıra numaraları görüldü.
  Örnek eski-format satır: `1008,61,78,58,75,0,0,9,26,49,0,0,0,0`.

Eski gap hesabı her mesajı referans yaptığından yeniden sıralanma sırasında
boşlukları tekrar sayabiliyordu. Bu yüzden eski 58/75 gibi gap değerleri kayıp
sayısı olarak raporlanmaz. 12 Eylül 2026 düzeltmesi 32 numaralık bitmap pencere
kullanır; geç gelenler pencere içindeki boşluğu doldurur, referans geriye gitmez.
Pencere dışına çıkan boşluklar aged_missing'e eklenir. Akış durursa pending
boşluklar zamanla otomatik kapatılmaz; bu bir sıra penceresidir, süre penceresi değil.
Sayıların rapor penceresi, mesajın ilk atlandığı saniyeden farklı olabilir.
İlk alınan mesajdan önceki sıra numaraları testin kapsamına alınmaz.

`tests/exp06_sequence_test.cpp` doğrudan receiver.ino içindeki takip sınıfını
Arduino kısmını hariç tutarak test eder. Yeniden sıralanma, tekrar, 32-numara
sınırı, büyük atlama, eski/reset adayı, rapor sıfırlama ve uint32 taşması geçti.
Nano yeniden derlendi. Güncellenen sürümün donanım tekrarı 12 Eylül 2026'da
kullanıcının paylaştığı iki seri monitör görüntüsüyle gözlendi.

## Güncellenen alıcıyla sonuç

Aşağıdaki satırlar ekran görüntülerinden aktarılmış seçili örneklerdir;
kesintisiz log kaydı değildir. Sütunlar yukarıdaki v2 sırasındadır.

```text
1009,68,73,33,28,0,0,15,21,0,0,49,0,0,40,0,11,8
1000,100,101,9,6,0,0,0,0,0,0,0,0,0,0,0,0,0
1000,100,100,0,0,0,0,0,0,0,0,0,0,0,0,0,0
```

İlk örnekte yavaşlatılan alıcıda 49 taşma örneği, 15/21 geç gelen mesaj ve
11/8 bekleyen sıra boşluğu var. EFLG=40 onaltılık bayrak RX0OVR'a karşılık
 gelir; örnekleme/temizleme sonrası yeni taşma oluşabildiğinden bu alan sıfır
olmak zorunda değildir. Buton bırakıldıktan sonra 100/101 alım ve önceki
pencereden yaşlanan 9/6 boşluk görülüyor. Sonraki örnekte 100/100 alım ve
sıfır göstergeler var. Bunlar yazılımsal alıcı darboğazını ve toparlanmayı
 destekler; kesin fiziksel hat kaybı veya arbitration zaferi sayısı değildir.
ID değiştirme karşılaştırması ve doğrudan arbitration gözlemi yapılmadı.

```sh
c++ -std=c++11 -Wall -Wextra -Werror tests/exp06_sequence_test.cpp -o /tmp/exp06-sequence-test
/tmp/exp06-sequence-test
```

## Kaynaklar

- [Microchip MCP2515 veri sayfası](https://ww1.microchip.com/downloads/aemDocuments/documents/APID/ProductDocuments/DataSheets/MCP2515-Family-Data-Sheet-DS20001801K.pdf): arbitration, TXREQ, READ STATUS, EFLG ve RX taşması.
- [autowp MCP2515](https://github.com/autowp/arduino-mcp2515): kullanılan API.
