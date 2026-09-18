# EXP08 — ACK, gönderim tamamlanması ve hata sayaçları

Üç sketch 17 Eylül 2026'da Arduino AVR 1.8.7 ile derlendi.
Flash/statik RAM: Uno A 4348/233, Uno B 5098/236, Nano 4418/242 bayt.
Kullanıcı 19 Eylül 2026 tarihinde deneyin tamamlandığını bildirdi. Lojik analizör gerekmez. Amaç ACK'nin uygulama kabulü
olmadığını ve ACK eksikliğinde denetleyicinin davranışını gözlemek.

## Bağlantı ve yükleme

Önceki üç düğümlü omurga: iki Uno ve klasik 5V Nano; üç MCP2515 modülü.
CS D10, MOSI D11, MISO D12, SCK D13, ortak GND; 5V uyumlu modül için VCC 5V.
CAN H/H, L/L bağlı; yalnız iki fiziksel uçta 120 ohm. Kabloları sökmeyeceğiz.
INT boş, servo beslemesi kapalı, LCD kullanılmıyor.
Kristaller ayrı kontrol edilmeli; varsayılan MCP_8MHZ, CAN_125KBPS.

| Kart | Sketch | Görev |
|---|---|---|
| Uno A | firmware/exp8/sender/sender.ino | 0x180 mesajını normal modda gönderir |
| Uno B | firmware/exp8/ack_node/ack_node.ino | Butonla ACK davranışı değiştirilir |
| Nano | firmware/exp8/observer/observer.ino | Yalnız dinler, ACK vermez |

Her sketch tek dosya; geçici sketch'e kopyalanabilir. Kütüphane autowp MCP2515.
Seri monitörler 115200 baud. Uno B'ye D2–buton–GND bağla (INPUT_PULLUP).
Diğer kartlarda buton kullanılmaz. Her basış 0→1→2→0, debounce 30 ms.

Üç karta da EXP08 yükle. Nano'da eski normal-mod yazılımı kalırsa ACK vermeye
devam eder ve ACK eksikliği deneyi çalışmaz. Deneye önce Uno B ve Nano'yu,
ardından göndericiyi açarak başla. Monitör açmak kartı resetleyebilir.

## Üç mod

| Uno B modu | Donanım modu/filtre | Beklenen |
|---|---|---|
| 0 NORMAL_ACCEPT_ACK | Normal, 0x180 kabul | RX yaklaşık 2/s; gönderim tamamlanır |
| 1 NORMAL_FILTER_OUT_ACK | Normal, yalnız 0x700 kabul | RX=0; gönderim yine tamamlanır |
| 2 LISTEN_ONLY_NO_ACK | Listen-only | ACK yok; gönderici tekrar dener |

Mod 1'de altı filtre ve iki maske yapılandırılır. Deney yalnız standart
0x180 üretir; 0x700 yayınlayan düğüm yoktur. Extended/RTR bu deneyin konusu değil.
Mod geçişi MCP2515 reset/config nedeniyle kısa kesinti yaratır; ilk satırları
kararlı durumla karıştırma. Listen-only modunda alınan çerçeveler başarı/ACK
kanıtı değildir; bu modda hata sayaçları da normal moddaki anlamda izlenmez.

## Gönderici logu

`ms,submitted,tx_complete,busy,api_flagged,TXREQ,TEC,REC,EFLG,state`

- submitted: açılıştan beri TXB0'a yazılıp istek verilen çerçeveler.
- tx_complete: normal modda TX0IF üzerinden gözlenen başarılı gönderimler.
  TXB0 yeniden kullanılmadan önce tamamlanma kontrol edilir. Bu, uygulamanın
  komutu kullandığını veya hangi düğümün ACK verdiğini söylemez.
- busy: 500 ms gönderim fırsatında TXB0 hâlâ doluysa artar; hata sayısı değildir.
- api_flagged: kütüphanenin sendMessage dönüşündeki hata bildirimleri;
  tüm tekrarları veya ACK hatalarını saymaz.
- TXREQ: anlık TXB0 gönderim isteği; ACK yokken uzun süre 1 kalabilir.
- TEC/REC: gönderme/alma hata sayaçları. EFLG onaltılık bayraklar.
- state: EFLG'den türetilir; WARNING hâlâ error-active durumudur.

submitted/tx_complete/busy/api_flagged kümülatiftir. Diğer alanlar farklı
register'lardan sırayla okunur; satır atomik donanım görüntüsü değildir.
Gönderim hedefi 500 ms'de bir yeni mesajdır. ACK yoksa mevcut çerçeveyi
donanım çok daha sık tekrar deneyebilir; yeni sıra numarası oluşmaz.

Mesaj: standart ID 0x180, DLC=5, bayt0=0xA8, bayt1–4=32 bit little-endian sıra.

## Hata durumları

TEC/REC 128 eşiğinde error-passive durumu oluşabilir. TEC 255'i aşarsa bus-off
oluşur; 8 bit TEC okumasıyla 256 görmeyi bekleme, TXBO bayrağını izle.
EFLG TXBO=0x20, TXEP=0x10, RXEP=0x08; bitler birlikte set olabilir.

Önemli istisna: Error-passive gönderici ACK alamayıp pasif hata bayrağı sırasında
dominant bit de görmüyorsa TEC artırılmayabilir. Bu yüzden bu deneyde TEC'nin
128 civarında kalması ve BUS_OFF oluşmaması normal bir sonuç olabilir.
BUS_OFF'u zorlamak için kabloları kısa devre etmiyoruz. Bu ders ACK yokluğu
ve error-passive gözlemidir; kontrollü bus-off üretimi kapsam dışıdır.

## Gözlemci logu

`window_ms,received,same_sequence,other,ovr_samples`

Nano listen-only olduğundan hatta ACK/hata bayrağı üretmez. Aynı sıra
numarasının tekrar görünmesi tekrar denemelerle uyumlu olabilir; tam tekrar
sayısı değildir. Tampon taşması, kaçırılan veya hatalı çerçeveler sayıyı etkiler.
Hata anında RX hiç görünmemesi de tek başına hattın sessiz olduğunu kanıtlamaz.
Tanı için göndericinin TXREQ/tx_complete/TEC/EFLG alanları esas alınır.

## Test sırası

| Test | İşlem | Beklenen | Sonuç |
|---|---|---|---|
| T01 | Mod 0, 5 s bekle | Uno B RX≈2/s, tx_complete artar, TEC düşük | Ayrı log aktarılmadı |
| T02 | Bir basış, mod 1 | Uno B RX=0 ama tx_complete artar | Ayrı log aktarılmadı |
| T03 | Bir basış, mod 2, 2–3 s bekle | TXREQ bekler, tx_complete durur, TEC/error-passive gözlenebilir | Ayrı log aktarılmadı |
| T04 | Bir basış, mod 0 | ACK geri gelir; bekleyen çerçeve tamamlanır, normal yayın sürer | Ayrı log aktarılmadı |

Mod 0'a dönünce TEC bir anda sıfırlanmak zorunda değildir; başarılı gönderimler
sonrasında düşmesini izle. Gerçek BUS_OFF görülürse normal ACK dönüşü testinden
ayrı not et; hatayı düzeltip göndericiyi resetleyerek temiz başlangıç yap.
Sayaçların kendiliğinden toparlanması ile kullanıcı resetini ayrı kaydet.
Her testte Uno B modu ve göndericinin birkaç log satırını paylaş.

## Donanım kaydı — 19 Eylül 2026

Kullanıcı deneyi başka bir sohbette ayrıntılı incelediğini ve tamamladığını
bildirdi. O sohbetin logları bu kayda aktarılmadı. Aşağıdaki test planı
beklenen davranışları tarif eder; tek tek sayaç değerleri, error-passive veya
bus-off gözlemi bu kayıtta doğrulanmış sayılmaz.

## Kaynaklar

- [Microchip MCP2515 veri sayfası](https://ww1.microchip.com/downloads/aemDocuments/documents/APID/ProductDocuments/DataSheets/MCP2515-Family-Data-Sheet-DS20001801K.pdf): TX0IF, hata durumları ve listen-only modu.
- [Bosch CAN 2.0 şartnamesi (kopya)](https://www.simmasoftware.com/assets/images/CAN-specification.pdf): Fault Confinement, error-passive ACK istisnası.
