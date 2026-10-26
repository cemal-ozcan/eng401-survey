# Bağlantı (Pin) Tablosu — v0.1 TASLAK

Kart: **ESP32 DevKit V1 (30 pin)**. Pin isimleri kartın üzerinde yazan **GPIO numaralarıdır**
(ör. "D21" veya "21" yazar). Emin olmadığınız her şeyin fotoğrafını çekip gönderin.

> ⚠️ **Güvenlik kuralları**
> 1. Kablo takıp çıkarırken USB / powerbank **çıkarılmış** olsun.
> 2. Sensör kartlarının **VCC** pini **3V3**'e bağlanır, **5V'a bağlamayın** (aşağıdaki istisnalar hariç).
> 3. Tüm **GND**'ler birbirine bağlıdır (ortak toprak).

## 1. I²C hattı (3 sensör aynı iki kabloyu paylaşır)
| Sensör kartı | Kart pini | → ESP32 pini |
|---|---|---|
| MPU6050 (GY-521) | VCC | 3V3 |
| | GND | GND |
| | SDA | **GPIO 21** |
| | SCL | **GPIO 22** |
| | AD0 | GND (boş bırakmayın; adres 0x68 olur) |
| BME280 (GY-BME280) | VCC / VIN | 3V3 |
| | GND | GND |
| | SDA | **GPIO 21** |
| | SCL | **GPIO 22** |
| | CSB, SDO | boş bırakın (adresi teşhis yazılımı bulur) |
| VL53L0X (GY-530) | VIN / VCC | 3V3 |
| | GND | GND |
| | SDA | **GPIO 21** |
| | SCL | **GPIO 22** |
| | XSHUT | **GPIO 33** |
| | GPIO1 | boş |

## 2. GNSS modülü (UART)
| GNSS kablosu | → ESP32 pini |
|---|---|
| TX (modülün verdiği veri) | **GPIO 16** (RX2) |
| RX | **GPIO 17** (TX2) |
| GND | GND |
| VCC | ⚠️ **Henüz belirlenmedi** — modülün üzerindeki yazının fotoğrafını gönderin (3.3 V mu 5 V mu) |
| SDA / SCL (varsa, pusula) | **Şimdilik bağlamayın** |

Kural: modülün **TX**'i ESP32'nin **RX**'ine gider (çapraz bağlantı).

## 3. microSD modülü (SPI)
| SD modülü pini | → ESP32 pini |
|---|---|
| VCC | **5V (VIN)** — bu modülün kendi 3.3 V regülatörü var |
| GND | GND |
| CS | **GPIO 5** |
| SCK | **GPIO 18** |
| MOSI | **GPIO 23** |
| MISO | **GPIO 19** |

Kart **FAT32** formatlı olmalı.

## 4. LED'ler ve buton
| Parça | Bağlantı |
|---|---|
| Yeşil LED (GNSS fix) | **GPIO 25** → 330 Ω → LED uzun bacak (+); kısa bacak → GND |
| Mavi LED (kayıt) | **GPIO 26** → 330 Ω → LED (+); (−) → GND |
| Kırmızı LED (hata) | **GPIO 27** → 330 Ω → LED (+); (−) → GND |
| Buton (gözlem noktası işaretle) | bir bacak **GPIO 32**, diğer bacak **GND** (direnç gerekmez) |
| Kırmızı 3 mm güç LED'i | 3V3 → 1 kΩ → LED → GND (yazılımla ilgisi yok) |

## 5. Kullanmayın
GPIO 0, 2, 12, 15 (açılışı bozabilir), GPIO 6–11 (dahili flash), GPIO 34–39 (sadece giriş).

## Özet (tek bakışta)
```
GPIO21 SDA  (MPU6050, BME280, VL53L0X)    GPIO16 ← GNSS TX
GPIO22 SCL  (MPU6050, BME280, VL53L0X)    GPIO17 → GNSS RX
GPIO33 VL53L0X XSHUT                       GPIO5  SD CS
GPIO25 yeşil LED  GPIO26 mavi  GPIO27 kırmızı    GPIO18 SD SCK  GPIO23 SD MOSI  GPIO19 SD MISO
GPIO32 buton (→GND)
```
