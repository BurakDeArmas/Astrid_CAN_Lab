# EXP09 — Kristal, bit zamanlaması ve sonlandırma

Üç sketch 19 Eylül 2026'da Arduino AVR 1.8.7 ile derlendi. Kullanıcı 21 Eylül 2026 tarihinde deneyi başka sohbette ayrıntılı incelediğini ve tamamladığını bildirdi.
Flash/statik RAM: gönderici 7260/254, alıcı ve gözlemci 6752/238 bayt.

## Düzen

| Kart | Tek dosyalı sketch | Mod |
|---|---|---|
| Uno A | firmware/exp9/sender/sender.ino | Normal, 0x190 yayını |
| Uno B | firmware/exp9/receiver/receiver.ino | Normal, alım ve ACK |
| Nano | firmware/exp9/observer/observer.ino | Listen-only, ACK vermez |

EXP08'in CAN omurgası korunur. Ortak GND, H/H ve L/L; iki uçta 120 ohm.
MCP2515 bağlantıları: CS D10, MOSI D11, MISO D12, SCK D13, INT boş.
5V uyumlu modüle 5V/GND. Servo beslemesi kapalı. Buton ve LCD kullanılmaz.
Kütüphane autowp MCP2515; monitör 115200. Üç karta da EXP09 yükle.

Kod varsayılanı MCP_8MHZ, OSC_HZ=8000000, BUS_SPEED=CAN_125KBPS.
16 MHz kristal varsa MCP_CLOCK ile OSC_HZ birlikte değişmeli. OSC_HZ bir
ölçüm değildir; fiziksel kristal varsayımıdır. Yanlış varsayımla hesaplanan
bitrate de yanlış olur. Kartların gerçek nominal bitrate'leri eşleşmelidir.

## İlk ders: 8 MHz, 125 kbit/s demek değildir

MCP2515 kristali denetleyicinin saat kaynağıdır. CAN bit süresi bu saatten
bölücüler ve zaman dilimleriyle oluşturulur. Kod başlangıçta CNF1/2/3'ü
donanımdan okuyup çözümleyerek değerleri yazdırır.

```text
TQ = 2 × (BRP_register + 1) / Fosc
Bir bit = SyncSeg + PropSeg + PS1 + PS2
Bitrate = 1 / bit süresi
Örnekleme noktası = (SyncSeg + PropSeg + PS1) / toplam TQ
```

Kurulu kütüphanenin 8 MHz / 125 kbit/s tablosu CNF1=01, CNF2=B1, CNF3=85.
Bu değerlerle bölücü 2; Sync=1, Prop=2, PS1=7, PS2=6 ve toplam 16 TQ olur.
TQ=0.5 µs, bit süresi=8 µs, örnekleme noktası=%62.5, SJW=1 TQ.
SJW toplam bit süresine eklenen beşinci bölüm değildir; yeniden senkronizasyon
sırasında izin verilen düzeltme sınırıdır. SAM örnek sayısını belirler.

Kod BTLMODE=0 durumunda PS2=max(PS1,2) kuralını da uygular.
Register çıktısı bir osiloskop ölçümü değildir; hat üzerindeki analog dalga
şeklini veya kristalin gerçek toleransını bu yöntemle ölçmüyoruz.

## Test A — Tüm kartlar 125 kbit/s

Önce alıcı/gözlemci, sonra göndericiyi aç. Başlangıç register ve hesap
satırlarını kaydet. Başlangıç satırları kaçtıysa monitör açıkken ilgili kartın
reset butonuna bas. Alıcıda ve Nano'da yaklaşık 10 mesaj/s beklenir.
Log: ms,submitted_total,received_window,busy_total,flagged_total,TEC,REC,EFLG.
ms açılıştan beri süredir, received_window raporlar arasındaki sayıdır.
submitted teslim onayı değildir. Listen-only sayaçları normal mod hata
sayaçları gibi yorumlanmaz. Dinleyici yalnız ID/DLC/işaret eşleşmesini sayar;
özellikle yanlış bitrate'te tek tük sayım güvenilir haberleşme kanıtı değildir.

## Test B — Yalnız sessiz gözlemci yanlış hızda

Nano sketch'inde yalnız BUS_SPEED değerini CAN_250KBPS yapıp Nano'ya yükle.
İki Uno 125 kbit/s'de kalsın. Nano ROLE=2/listen-only kalmalı.
Nano'daki güvenilir alımın kaybolması, Uno B'de normal alımın sürmesi beklenir.
Bu yöntem yanlış hızdaki düğümün hatta aktif hata bayrağı göndermesini önler.
Sonra Nano'yu tekrar CAN_125KBPS yapıp yükle; alım geri gelmeli.

## Test C — Üç kart 250 kbit/s

Tümünün BUS_SPEED değerini CAN_250KBPS yap, yüklemeleri bitirip üç kartı
resetle. Karışık hızdaki geçiş süresini test sonucu olarak kullanma.
8 MHz varsayımıyla TQ=0.25 µs, bit=4 µs ve aynı örnekleme yüzdesi beklenir.
Mesaj yayın periyodu hâlâ 100 ms: 250 kbit/s'ye çıkınca mesaj sayısı otomatik
20/s olmaz. Daha hızlı taşınan şey bir çerçevenin bitleridir.
Testten sonra kodları 125 kbit/s başlangıç ayarına döndür.

## Sonlandırma kontrolü

Tüm USB ve harici beslemeleri ayırdıktan sonra multimetreyle H–L direncini ölç.
İki adet 120 ohm paralelde yaklaşık 60 ohm; tek 120 ohm yaklaşık 120 ohm;
üç adet 120 ohm yaklaşık 40 ohm verir (devredeki diğer yollar ölçümü etkileyebilir).
Direnç ölçümü enerjili devrede yapılmaz. Yeniden enerji vermeden önce iki uç
sonlandırmasını geri kur. Bu deneyde sonlandırmasız hattı çalıştırmak gerekmez.
Amaç yansıma azaltmaktır; kısa kabloda eksik sonlandırmayla çalışabilmek doğru
fiziksel tasarımın kanıtı değildir. Dalga şeklini daha sonra ölçebiliriz.

| Deneme | Sonuç |
|---|---|
| 125 kbit/s register ve alım | Ayrı ölçüm/log aktarılmadı |
| Yalnız Nano 250 kbit/s, sonra 125'e dönüş | Ayrı ölçüm/log aktarılmadı |
| Üç kart 250 kbit/s | Ayrı ölçüm/log aktarılmadı |
| Enerjisiz H–L direnç kontrolü | Ayrı ölçüm/log aktarılmadı |

Kaynak: [Microchip MCP2515 veri sayfası, bit timing ve CNF register'ları](https://ww1.microchip.com/downloads/aemDocuments/documents/APID/ProductDocuments/DataSheets/MCP2515-Family-Data-Sheet-DS20001801K.pdf).

## Tamamlanma kaydı — 21 Eylül 2026

Genel tamamlanma bildirimi kaydedildi. Diğer sohbetin çıktıları burada yoktur;
belirli CNF değerleri, 250 kbit/s denemesi veya direnç ölçümü bağımsız olarak
doğrulanmış sonuç sayılmamıştır. Yukarıdaki sayısal örnekler beklenen hesaplardır.
