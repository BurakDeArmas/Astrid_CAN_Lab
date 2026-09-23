# EXP10 — CAN mesaj sözleşmesi, byte sırası ve DBC

Durum: Kullanıcı 23 Eylül 2026 tarihinde testin tamamlandığını bildirdi. Üç sketch Arduino AVR 1.8.7 ile derlendi.
Flash/statik RAM: gönderici 4588/237, alıcı 5958/258, Nano 4004/217 bayt.

## Kurulum

Önceki iki Uno + klasik Nano ve üç MCP2515 düzenini kullan. CS D10, MOSI D11,
MISO D12, SCK D13; ortak GND, uygun modüle 5V. H/H, L/L ve iki uçta 120 ohm.
Üç kod yeniden 125 kbit/s ve 8 MHz kristal varsayar. EXP09'dan kalan 250 kbit/s
kodları karışmasın. INT boş, servo beslemesi kapalı, LCD kullanılmaz.

| Kart | Sketch | Görev |
|---|---|---|
| Uno A | firmware/exp10/sender/sender.ino | Pot gerilimi tahmini ve yapay sıcaklık yayınlar |
| Uno B | firmware/exp10/receiver/receiver.ino | Paketi doğrular, fiziksel değerlere çevirir |
| Nano | firmware/exp10/observer/observer.ino | Listen-only, ham hex baytlarını gösterir |

Her kod tek dosyadır, geçici sketch'e kopyalanabilir. Kütüphane autowp MCP2515.
Seri monitör 115200 baud. Uno A'da pot orta uç A0, dış uçlar 5V/GND.
Uno A D2–buton–GND: her basışta sıcaklık -12.3 → 0.0 → 25.3 → 85.0 → başa.
Bu sıcaklıklar sensör ölçümü DEĞİL test değerleridir; pakette synthetic=1 taşınır.
Gerilim hesabı 5.000 V ADC referansı varsayar; kalibre edilmiş ölçüm değildir.

## Mesaj sözleşmesi

Standart ID 0x300 (DBC'de ondalık 768), DLC 8, yayın periyodu 100 ms.

| Bayt | İçerik | Yorum |
|---|---|---|
| 0 | Version=1 | Protokol sürümü |
| 1 | Counter | 0–255, sonra 0 |
| 2–3 | Temperature | signed 16 bit, little-endian; ham × 0.1 °C |
| 4–5 | Voltage | unsigned 16 bit, little-endian; ham × 0.001 V |
| 6 bit 0 | SyntheticTemperature | 1=test sıcaklığı |
| 6 bit 1–7 | ReservedFlags | 0 olmalı |
| 7 | Reserved | 0 olmalı |

Little-endian, çok baytlı sayının düşük baytının önce gelmesidir. CAN hattındaki
bit gönderim sırasıyla aynı kavram değildir. Negatif değer two's complement
ile kodlanır. Ham -123, 16 bit olarak 0xFF85; pakette 85 FF olur.

Örnek (üretilmiş test vektörü; gerçek cihaz kaydı değil):

```text
01 2A 85 FF C4 09 01 00
```

Sürüm 1, sayaç 42, sıcaklık -12.3 °C, gerilim 2.500 V, yapay sıcaklık bayrağı 1.
C4 09, 0x09C4=2500 demektir. Ölçek ve işaret bilgisi olmadan ham baytların
fiziksel anlamı bilinemez. Protokolü tasarlarken alıcı/gönderici bu sözleşmeyi paylaşır.

## DBC ne yapar?

[exp10.dbc](../protocol/exp10.dbc) mesajın ID'sini, boyunu, sinyallerin bit
konumlarını, işaretini ve ölçeklerini makine tarafından okunabilir biçimde tanımlar.
`16|16@1-` = başlangıç bit 16, uzunluk 16 bit, little-endian, signed.
DBC karta ayrıca yüklenmez; Arduino kodu aynı sözleşmeyi elle uygular.
DBC kendi başına sürüm/sayaç/timeout doğrulaması yapmaz; bunlar firmware'dedir.

Alıcı yanlış DLC/sürüm/rezerve bitleri ve aralık dışı değeri reddeder.
Sıcaklık aralığı -40…125 °C, gerilim 0…5 V. Sayaç ilerlemezse COUNTER_ERROR
yazar, tazelik zamanını yenilemez. 500 ms taze veri gelmezse STALE yazar.
İlk mesaj ve timeout sonrası mesaj SYNC olur; bir sonraki adım OK olmalıdır.
Bu görüntüleme deneyidir, aktüatör güvenlik denetleyicisi değildir.

## Deneme

1. Üç kartı yükle. Alıcıda yaklaşık `counter,-12.3,voltage,1,OK` satırları bekle.
2. Potu çevir: voltaj tahmini değişsin, sıcaklık sabit kalsın.
3. D2'ye tek tek bas: dört sıcaklık değerini ve Nano'daki 2–3. baytları karşılaştır.
4. Nano çıktısından bir çerçeveyi birlikte elle çöz. Sayaçlar farklı zamanlarda
   görünürse aynı Counter değerli çerçeveleri karşılaştır.
5. Göndericinin gücünü kes (omurga/sonlandırma korunarak): alıcı STALE yazmalı.
   Geri geldiğinde SYNC ve OK bekle. En az 30 s çalışmada sayaç taşmasını izle.

## Bağımsız DBC kontrolü

`tests/exp10_dbc_test.py`, cantools ile DBC'nin kodlama/çözümlemesini bağımsız
Python struct vektörleriyle karşılaştırır. 54 vektör: negatif/pozitif ve sınır
sıcaklıkları, 0/2.5/5 V, sayaç 0/42/255. Bu test fiziksel donanım testi değildir;
Arduino firmware'in tamamını çalıştırmaz.

```sh
python3 tests/exp10_dbc_test.py
```

Bağımlılık: cantools (doğrulama ortamında 40.7.1). Kurulum sadece bilgisayardaki
DBC testi için gerekir; Arduino'ya yükleme için Python gerekmez.

Kaynak: [cantools dokümantasyonu](https://cantools.readthedocs.io/en/latest/).

## Mac üzerinde canlı DBC okuyucu

Proje kökündeki `EXP10_Canli.command` dosyasına çift tıkla. Önce Arduino IDE'de
Nano'nun Seri Monitörünü kapat; sonra listeden observer kodu yüklü Nano'nun
port numarasını gir. Port açılması Nano'yu resetleyebilir. Okuyucu seri porta
komut yazmaz. CAN verisi Nano'nun observer yazılımından gelir.

Örnek çözümleme:

```text
178591 ms | sayac= 11 | sicaklik= -12.3 C (YAPAY) | gerilim=5.000 V | 01 0B 85 FF 88 13 01 00 | OK
```

`SYNC` ilk satırı, `OK` sayacın bir arttığını, `SIRA UYARISI` tekrar veya
atlamayı belirtir. `RED` biçim/protokol hatasıdır. `VERI YOK` iki saniyedir
geçerli seri veri alınmadığını belirtir; firmware'in 500 ms tazelik kontrolünden
ayrıdır. Sıcaklık sentetiktir; gerilim 5 V ADC referansı varsayar.
Çıkış için Ctrl+C kullan. Arduino Seri Monitörünü sonra yeniden açabilirsin.

Yeni bilgisayarda proje klasöründe bir kere kurulum:

```sh
python3 -m venv .venv
.venv/bin/python -m pip install -r tools/requirements.txt
```

Python 3.9 veya sonrası gerekir. `.venv` Git'e eklenmez. Donanımsız örnek ve testler:

```sh
.venv/bin/python tools/exp10_live.py --demo
.venv/bin/python tests/exp10_dbc_test.py
.venv/bin/python tests/exp10_live_test.py
```

Demo canlı bağlantı testi değildir. Canlı çalıştırmada USB portu kullanıcı seçer.

## Tamamlanma ve canlı okuyucu — 23 Eylül 2026

Genel donanım başarı bildirimi kaydedildi; ayrı ham test logu bu sohbete aktarılmadı.
54 DBC vektörü ve canlı seri çözümleyicinin üç testi geçti.
`tools/exp10_live.py`, Nano'nun seri hex satırlarını DBC ile çözer. Arduino
Seri Monitörünü kapatıp Nano portunu seç; portun açılması Nano'yu resetleyebilir.

```sh
python3 -m venv .venv
.venv/bin/pip install -r tools/requirements.txt
.venv/bin/python tools/exp10_live.py --demo
.venv/bin/python tools/exp10_live.py
.venv/bin/python tests/exp10_live_test.py
```

macOS'ta ortam kurulduktan sonra `EXP10_Canli.command` aynı okuyucuyu açar.
Python okuyucunun 2 s veri-yok uyarısı alıcı firmware'in 500 ms tazelik
denetiminden ayrıdır; sayaç uyarısı ayrıca yazdırılır.
