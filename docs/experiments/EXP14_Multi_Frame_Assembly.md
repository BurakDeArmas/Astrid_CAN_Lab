# EXP14 — Bir mesajı birden fazla CAN çerçevesinde taşıma

29 Eylül 2026: Üç sketch derlendi; yeniden birleştirme host testi ASan/UBSan
ile geçti. Kullanıcı 29 Eylül 2026 tarihinde deneyin tamamlandığını bildirdi;
ayrı seri log aktarılmadı.

## Amaç

Klasik CAN veri alanı en fazla 8 bayt. Bu deneyde 24 baytlık metin, her biri
4 bayt başlık ve 4 bayt mesaj parçası taşıyan altı veri çerçevesine bölünür.
Alıcı parçaları geçici bellekte birleştirir; yalnız altısı da doğru sırada
geldikten sonra tamamlanmış mesajı günceller. Yarım mesaj uygulamaya sunulmaz.

Bu, sabit uzunluklu özel eğitim protokolüdür; ISO-TP uyumlu değildir.
ISO-TP'de First Frame, Consecutive Frame ve Flow Control gibi standart
mekanizmalar bulunur. Bu deney onların çözdüğü problemi öğrenmek içindir.
Kaynak: [Linux Kernel ISO-TP belgesi](https://kernel.org/doc/html/latest/networking/iso15765-2.html).

## Bağlantılar ve yükleme

İki Uno, klasik Nano ve üç 8 MHz MCP2515; CAN 125 kbit/s. Önceki CAN omurgası
korunur: ortak GND, H/H, L/L, yalnız iki uçta 120 ohm. Her kartta CS D10,
MOSI D11, MISO D12, SCK D13. INT boş; 5V uyumlu modüllere 5V besleme.
Bağlantıları enerji kesikken değiştir.

| Kart | Tek dosyalık kod | Ek bağlantı |
|---|---|---|
| Uno A | [sender.ino](../../firmware/exp14/sender/sender.ino) | D2–buton–GND: yeni aktarım |
| Uno B | [receiver.ino](../../firmware/exp14/receiver/receiver.ino) | D2–buton–GND: hata modu; D7–330 ohm–LED anot, katot–GND |
| Nano | [observer.ino](../../firmware/exp14/observer/observer.ino) | Ek bağlantı yok; listen-only |

Pot bu deneyde kullanılmaz. Butonlar INPUT_PULLUP ve 30 ms debounce kullanır.
Üç kodun tamamı ayrı geçici Arduino sketch'lerine kopyalanabilir; ek header
kopyalamak gerekmez. Seri monitörler 115200 baud. Nano'da çalışan bootloader
ayarını koru. Testlere başlamadan monitörleri aç ve başlangıç durumlarını gör.

## Mesaj sözleşmesi

Parçalar: standart DATA 0x360, DLC 8.

| Bayt | Anlam |
|---|---|
| 0 | Sabit 0xE4 |
| 1–2 | uint16 aktarım numarası, little endian |
| 3 | Parça indeksi: 0,1,2,3,4,5 |
| 4–7 | Metnin bu parçaya ait dört baytı |

İlk metin `ASTRID CAN MESSAGE 00001`: tam 24 ASCII bayt. C dizisindeki
sonlandırıcı sıfır gönderilmez. Sonraki aktarım `00002` ile biter.
Altı çerçevenin veri alanları toplam 48 bayttır: 24 mesaj + 24 protokol başlığı.
CAN'in kendi çerçeve alanları buna ayrıca eklenir; 48 sayısı teldeki toplam değildir.

Durum yanıtı: standart DATA 0x361, DLC 4: `0xD4, aktarım_lo, aktarım_hi, sonuç`.

| Sonuç | Anlam |
|---|---|
| 1 COMPLETE | Altı parça birleşti; mesaj yayınlandı |
| 2 ORDER_ERROR | Yanlış sıra, tekrar veya devam eden aktarımda başka numara |
| 3 TIMEOUT | Son kabul edilen parçadan beri en az 300 ms geçti |
| 4 FORMAT_ERROR | Aktarım sırasında yanlış DLC, işaret baytı veya indeks |

Aktif aktarım yokken indeks 0 dışındaki parçalar yok sayılır. Bir hata sonrası
kalan parçalar yeni bir tam mesaj oluşturmaz. Yeni aktarım indeks 0 ile başlar.
Aynı aktarımın tekrarlanan indeks 0'ı da sıra hatasıdır; sessiz yeniden başlatma yoktur.

## Zamanlama ve durum

Uno A tek TXB0 kullanır; boşken en az 50 ms arayla bir parça gönderim isteği
oluşturur. Tek tampon kullanımı, farklı TX tamponlarından sıra değişmesini
bu deneyden uzak tutar. API dönüşü başarılı teslim kanıtı değildir.
Gönderici aynı anda yalnız bir aktarım başlatır; durum alınca veya başlangıçtan
1500 ms sonra beklemeyi bitirir. Hata durumu erken gelirse kalan parçaları
göndermeyi bırakır. Otomatik tekrar gönderme yoktur.

Alıcıda `pending` geçici veriyi, `published` son tam mesajı tutar. Buradaki
bütün halinde güncelleme, tek iş parçacıklı loop içinde yapılan kopyalamadır;
kesme/çok çekirdek için atomik bellek garantisi değildir. D7 LED'i her COMPLETE
sonrası bir kez durum değiştirir; hata veya eksik parçada değişmez.
`published` numarası ve `completed` sayacı seri çıktıda ayrıca izlenir.

## Testler

### 1. Normal aktarım

B başlangıçta MODE=0. A butonuna bir kez bas.

Beklenen (temsili, ölçüm değildir):

```text
RESULT transfer=1 code=1 published=1 completed=1
MESSAGE=ASTRID CAN MESSAGE 00001
```

LED yanar. Nano'da indeksleri 0–5 olan altı 0x360 ve bir 0x361 görülür.
Örnek ilk parça: `E4 01 00 00 41 53 54 52` (ASTR).
İkinci normal aktarım tamamlanınca LED söner ve metin 00002 olur.

### 2. Orta parça eksik

B butonuna bir kez bas: MODE=1. Sonra A'ya bas.
B indeks 2'yi CAN'den alır ama birleştiriciye vermeden `APP_DROP index=2`
yazar. İndeks 3 gelince beklenen 2 olmadığı için ORDER_ERROR (code=2) oluşur.
Önceki `published` ve `completed` korunur; LED değişmez.

Nano indeks 2'yi görebilir: kayıp kabloda veya CAN CRC'sinde değil, alıcı
uygulamasında bilerek üretilir. Gönderici hata yanıtını alınca kalan parçaları
keser; mutlaka altı parçanın tamamını göreceğini varsayma.

### 3. Son parça eksik

B'ye bir kez daha bas: MODE=2. A'ya bas.
İndeks 5 alıcı uygulamasında atılır. Sonraki bir parça gelmediği için sıra
hatası tetiklenmez; indeks 4'ten yaklaşık 300 ms sonra TIMEOUT (code=3) oluşur.
Önceki tamamlanmış mesaj ve LED yine korunmalıdır.

### 4. Toparlanma

TIMEOUT sonrasında B'ye tekrar basarak MODE=0'a dön. A'ya bas: yeni aktarım
başarıyla birleşmeli ve LED bir kez değişmelidir. İlk testi yalnız bir kez
uyguladıysan toparlanma mesajı 00004, completed=2 olur.
Aktarım sürerken B mod değiştirme isteği `MODE_BLOCKED` ile reddedilir.

## Öğrenme kontrolü

1. CAN tek çerçeveyi doğru taşısa bile 24 baytlık mesaj neden yarım kalabilir?
2. Aktarım numarası ile parça indeksi hangi farklı sorunları çözer?
3. Orta parça kaybında sıra hatası, son parça kaybında neden timeout oluşur?
4. Her parça gelir gelmez ana mesajı değiştirsek uygulama ne görebilirdi?

## Sınırlar

- Tek gönderici, tek alıcı, sabit 24 bayt ve sıralı aktarım varsayılır.
- Oturum kimliği, yeniden oynatma koruması, kalıcı önbellek, akış kontrolü,
  değişken uzunluk ve bütün mesaj CRC'si yoktur. CAN'in çerçeve CRC'si ayrıca işler.
- Yeni bir indeks 0, tamamlanmış eski bir aktarımı yeniden başlatabilir. Bu
  protokol exactly-once garantisi veya üretim sistemi için taşıma katmanı değildir.
- Reset RAM durumunu siler. Karşılaştırmalı testler arasında kartları resetleme.
- NO_STATUS, aktarımın kesin başarısız olduğunu kanıtlamaz; durum yanıtı kaybolmuş
  olabilir. Fiziksel kesintide donanımın bekleyen iletimi uygulama timeout'uyla
  iptal edilmez. Bu deney kablo kesmeden uygulamada parça düşürür.
- RX_OVERFLOW varsa gözlem eksik olabilir. Nano listen-only ACK üretmez.

## Doğrulama

| Hedef | Flash | RAM |
|---|---:|---:|
| Uno A | 6592 | 284 |
| Uno B | 5352 | 287 |
| Nano | 4074 | 217 |

Arduino AVR 1.8.7 ile üç derleme geçti. `tests/exp14_assembly_test.cpp`, gerçek
receiver sketch'indeki sınıfı test eder: eksik orta/son parça, timeout sınırı,
millis taşması, yinelenen/karışan parçalar, bozuk uzunluk/indeks ve toparlanma.
ASan/UBSan ile geçti. Kullanıcı genel tamamlanma bildiriminde bulundu;
mod bazında ayrı seri kayıtlar henüz arşivlenmedi.

```sh
c++ -std=c++11 -Wall -Wextra -pedantic -fsanitize=address,undefined tests/exp14_assembly_test.cpp -o /tmp/exp14_test
/tmp/exp14_test
```
