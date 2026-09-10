# EXP05 — CAN hata enjeksiyonu ve toparlanma

Durum: Üç sketch derlendi ve aktüatör durum mantığının otomatik testleri geçti
(10 Eylül 2026). Aynı gün kullanıcı üç butonla hata üretme, servo/LED/LCD
tepkileri ve buton bırakıldıktan sonra toparlanma denemelerinin geçtiğini bildirdi.

## Düzen ve yükleme

EXP04'ün iki Uno + klasik 5V Nano, üç MCP2515, pot, servo ve I2C LCD
bağlantıları korunur. [EXP04 bağlantı rehberi](EXP04_Three_Node_CAN_Display.md).
Her modül için kristal kontrolü gerekir; varsayılan 8 MHz, CAN 125 kbit/s.
Üç kartın GND'si ortak, CAN hattının yalnız iki ucunda 120 ohm sonlandırma.
Servo D5 ve alıcı fault LED D7 bağlantıları değişmedi.

Yeni bağlantı yalnız **gönderici Uno A** üzerinde:

| Buton | Bağlantı | Basılı tutulduğunda |
|---|---|---|
| B1 | D2 ↔ buton ↔ GND | Alive counter donar; mesaj yayını sürer |
| B2 | D3 ↔ buton ↔ GND | ADC=2047 gönderilir (geçerli aralık 0–1023) |
| B3 | D4 ↔ buton ↔ GND | Yeni komut yayını durur |

INPUT_PULLUP kullanılır; harici pull-up direnci gerekmez. Düğmeler 5V'a
bağlanmaz. Dört bacaklı butonda aynı elektriksel taraftaki iki bacak zaten
birbirine bağlıdır; basınca birleşen karşı kontakları kullan. Aksi halde
giriş sürekli basılı görünebilir. Mod seçimi 30 ms debounce içerir.
Birden çok buton basılıysa öncelik D4 > D3 > D2. İlk testlerde tek tek bas.
Buton bırakılınca normal yayın geri gelir; aç/kapa kilitlemesi yoktur.

| Kart | Yüklenecek sketch |
|---|---|
| Uno A | firmware/exp05_sender/exp05_sender.ino |
| Uno B | firmware/exp05_actuator/exp05_actuator.ino |
| Nano | firmware/exp05_display/exp05_display.ino |

Aktüatör klasöründeki CommandMonitor.h dosyasını sketch'in yanında tut.
IDE'de Uno/Uno/Nano seç, her karta doğru porttan yükle. Seri monitör 115200.
Kütüphaneler EXP04 ile aynı: autowp MCP2515, Arduino Servo, Bill Perry hd44780.
EXP05 kimlikleri farklıdır; üç karta da EXP05 yükle, EXP04 ile karıştırma.

## Mesaj sözleşmesi

Standart CAN data frame, DLC=4, her kaynak için 100 ms yayın periyodu.

| ID | Kaynak | Bayt 0 | Bayt 1 | Bayt 2 | Bayt 3 |
|---|---|---|---|---|---|
| 0x130 | Uno A | 0xA5 | Komut sayacı | ADC düşük | ADC yüksek |
| 0x140 | Uno B | 0xB5 | Durum sayacı | Uygulanan açı komutu | Durum kodu |

| Kod | Seri log | LCD ikinci satır | Servo |
|---|---|---|---|
| 0 | WAITING | BEKLIYOR | 90° |
| 1 | ACTIVE | UYG:… AKTIF | Pot komutu: 30–150° |
| 2 | COUNTER_ERROR | SAYAC HATASI | 90° |
| 3 | DATA_ERROR | VERI HATASI | 90° |
| 4 | TIMEOUT | TIMEOUT | 90° |
| 5 | RECOVERING | TOPARLANIYOR | 90° |

Alıcı D7 LED'i ACTIVE dışında yanar. Servo enerjisi kesilmez; 90° masaüstü
deneyinin bekleme komutudur. Gerçek mil açısı ölçülmez.

Timeout burada **0x130 standart komut çerçevesinin 500 ms gelmemesi** demektir.
Geçersiz veri/sayaç içeren çerçeveler gelirken ilgili hata korunur; bu mesajlar
harekete izin vermez. Başka ID'ler ve RTR/extended çerçeveler süreyi yenilemez.
Sayaç hatasından sonra üç ardışık +1 adımı gerekir. Veri hatası ve timeout
senkronizasyonu sıfırlar: önce bir referans mesajı, sonra üç +1 adımı gerekir.
Nano, aktüatörün bağımsız durum sayacını da izler; güvenilir durum akışı yoksa
AKTUATOR YOK gösterir. İlk satır kendi izlediği komut sağlığını gösterir;
hata nedeninin kaynağı aktüatörün durum mesajıdır.

## Test planı ve sonuç kaydı

Önce potu orta konumdan uzağa getir (örneğin 130°). Böylece 90°'ye dönüşü
görebilirsin. Her testten önce ACTIVE durumunu bekle, butonu en az 2 saniye
basılı tut ve bıraktıktan sonra toparlanmayı izle.

| Test | İşlem | Beklenen | Gözlenen / sonuç |
|---|---|---|---|
| T01 | Normal pot kontrolü | ACTIVE, servo takip eder, D7 sönük | Geçti — kullanıcı bildirimi; süre ölçülmedi |
| T02 | D2 basılı | SAYAC HATASI, servo 90°, D7 açık; 2 s boyunca hata korunur | Geçti — kullanıcı bildirimi; süre ölçülmedi |
| T03 | D2 bırak | RECOVERING ardından üç +1 adımıyla ACTIVE | Geçti — kullanıcı bildirimi; süre ölçülmedi |
| T04 | D3 basılı | VERI HATASI, servo 90°, D7 açık; 2 s boyunca hata korunur | Geçti — kullanıcı bildirimi; süre ölçülmedi |
| T05 | D3 bırak | Referans + üç doğru adım sonrası ACTIVE | Geçti — kullanıcı bildirimi; süre ölçülmedi |
| T06 | D4 basılı | Son komut çerçevesinden 500 ms sonra TIMEOUT, servo 90° | Geçti — kullanıcı bildirimi; süre ölçülmedi |
| T07 | D4 bırak | Referans + üç doğru adım sonrası ACTIVE | Geçti — kullanıcı bildirimi; süre ölçülmedi |
| T08 | 30 s normal çalışma | 255→0 sayaç taşmasında kesinti yok | Bekliyor |
| T09 | Aktüatör CAN H/L bağlantısını ayır, omurgayı koru | Nano AKTUATOR YOK gösterir | Bekliyor |

Buton algılama 30 ms; yayın 100 ms; durum yayını 100 ms; LCD yenileme 200 ms.
Ekrandaki hata değişimi bu periyotlar nedeniyle gecikebilir. D2 basıldığında
önceden artırılmış sayaçla bir çerçeve daha gidebilir; sonraki tekrarda hata
yakalanır. D4 daha önce MCP2515'e kuyruğa verilmiş mesajları iptal etmez;
timeout son alınan komuttan hesaplanır, butona basma anından değil.

Alıcı logu örnek biçimi (bu satırlar gerçek test kaydı değildir):

```text
1234,STATE,COUNTER_ERROR,ANGLE,90
3456,STATE,RECOVERING,ANGLE,90
3656,STATE,ACTIVE,ANGLE,130
```

Gönderici MODE değerleri: 0=normal, 1=dondur, 2=geçersiz veri, 3=yayın durdur.
Her kartın millis() saati bağımsızdır; ayrı kartların zamanlarını doğrudan
çıkararak gecikme ölçme. Alıcıda yalnız durum geçişleri, Nano'da değişen ekran
ikinci satırı loglanır. Kopyaladığın gerçek logları test numarası ve kart adıyla
kaydet; sonuç tablosunu bunlara göre dolduralım. Terminalin açılması kartı
resetleyebileceğinden önce monitörleri açıp ardından denemeye başla.

Kalıcı TX hatasında bağlantıyı düzeltip kartları resetle. Bu deney otomatik
bus-off toparlanması veya tüm CAN fiziksel hata türlerinin testini kapsamaz.

## Donanım sonucu — 10 Eylül 2026

T01–T07 için kullanıcı başarı bildirdi. T08 ve T09 ayrıca doğrulanmadı.
Seri log veya ölçüm dosyası paylaşılmadı; tabloda verilen zamanlar yazılım
ayarlarıdır. Alıcı denemesinde CommandMonitor.h içeriği geçici Arduino
sketch'ine alınarak tek dosya kullanıldı; depodaki iki dosyalı yapı korundu.

## Yazılım doğrulaması

Üç Arduino hedefi Arduino AVR 1.8.7 ile derlendi:

| Hedef | Flash | Statik RAM |
|---|---|---|
| Uno A | 5080 bayt | 242 bayt |
| Uno B | 6142 bayt | 357 bayt |
| Nano | 11306 bayt | 730 bayt |

`tests/exp05_monitor_test.cpp` doğrudan firmware'in CommandMonitor.h dosyasını
test eder: sayaç donması, uzun süreli geçersiz veri, üç adımlı toparlanma,
499/500 ms sınırı, 255→0 ve millis taşması, yanlış işaret/DLC, sayaç sıçraması
ve başlangıçta mesaj yokluğu. Testler geçti; fiziksel buton ve CAN testlerinin
yerine geçmez.

Repo kökünde çalıştırma:

```sh
c++ -std=c++11 -Wall -Wextra -Werror tests/exp05_monitor_test.cpp -o /tmp/exp05-monitor-test
/tmp/exp05-monitor-test
```
