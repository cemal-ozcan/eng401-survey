# 2. Teşhis Yazılımı (v0.1-diag) Test Prosedürü

Bu yazılım her parçayı tek tek dener ve sonucu yazar. Her 30 saniyede testi tekrarlar,
yani kablo düzelttikten sonra **yeniden yükleme gerekmez** — sadece bekleyin
(veya Console'a `r` yazıp Enter).

Bağlantılar için: `docs/PINOUT.md`. **Kablo takıp çıkarırken USB'yi çıkarın.**

## Önerilen sıra (her adımda raporu bana gönderin)
Parçaları **teker teker** ekleyin; bir sorun çıkarsa hangi parçadan olduğu hemen anlaşılır.

| Adım | Ne bağlanacak | Beklenen sonuç |
|---|---|---|
| 1 | Sadece ESP32 (hiçbir şey bağlı değil) | Rapor çıkar; I2C/GNSS/SD **FAIL** olur — **bu normal**. Amaç: yükleme çalışıyor mu |
| 2 | + MPU6050 | `MPU6050 : OK`, ivme büyüklüğü ≈ 9.8 m/s2 |
| 3 | + BME280 | `BME280 : OK`, oda sıcaklığı makul |
| 4 | + VL53L0X | `VL53L0X : OK`; sensörü 30–100 cm uzaktaki duvara tutun |
| 5 | + microSD (FAT32 kart takılı) | `SD : OK ... read-back OK` |
| 6 | + LED'ler ve buton | 3 LED sırayla yanıp söner; butona basınca `BUTTON pressed` |
| 7 | + GNSS, **dışarıda, açık gökyüzü** | Önce `NO FIX yet` (normal), 1–5 dk sonra `FIX OK` |

GNSS bağlamadan önce modülün **iki yüzünün fotoğrafını** bana gönderin (besleme voltajı henüz belirlenmedi).

## Rapor satırlarının anlamı
- `[OK]` çalışıyor. `[WARN]` çalışıyor ama dikkat gerekiyor (açıklamayı okuyun).
- `[FAIL]` çalışmıyor; satırın sonunda neyin kontrol edileceği yazar.
- `[SKIPPED]` LED'ler: hangi renklerin yandığını mesajınıza siz yazın.

## Sık görülen FAIL'ler
| Mesaj | Muhtemel neden |
|---|---|
| `SDA/SCL stuck LOW` | SDA/SCL kablosu GND'ye değiyor veya bir sensör ters takılı |
| `no devices` (I2C) | SDA↔SCL yer değiştirmiş, VCC/GND eksik |
| `chip is BMP280 ... not BME280` | Yanlış sensör gelmiş (nem ölçmez) — satıcıya iade |
| `GNSS no data` | GNSS TX → GPIO16 mı? Besleme var mı (modül LED'i)? TX/RX'i değiştirip deneyin |
| `SD card not detected` | Kart takılı mı, FAT32 mi? SD modülü VCC → **5V** |

## Bana gönderilecekler (her test için)
1. `COPY FROM HERE` … `COPY UNTIL HERE` bloğu (metin olarak)
2. Hangi adımda olduğunuz ve hangi LED'lerin yandığı
3. Kabloların net bir fotoğrafı (özellikle FAIL varsa)
