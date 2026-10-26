# 3. Asıl Yazılım (v0.2-app) — Kurulum, Wi-Fi ve Dashboard

Bu yazılım cihazı **ölçüm cihazına** dönüştürür: her saniye bir kayıt alır, SD karta yazar,
Wi-Fi ile bilgisayara gönderir. Önce **mock** sürümle (sensör gerekmez) Wi-Fi ve dashboard'u deneyin,
sonra gerçek sürüme geçin.

## 1. Yükleme
Kılavuz 01'deki gibi. Önce `eng401-v0.2-app-mock.bin`, her şey çalışınca `eng401-v0.2-app.bin`.

## 2. Wi-Fi ayarı (bir kez; yeniden yüklemede de korunur)
Cihaz bir **telefon hotspot'una** bağlanır; dashboard'u çalıştıran bilgisayar da **aynı hotspot'a** bağlı olmalı.
1. Telefonda hotspot'u açın. Adını ve şifresini not edin (şifre en az 8 karakter).
   iPhone'da "Maksimum Uyumluluk" açık olsun (2.4 GHz). ESP32 **5 GHz ağlara bağlanamaz**.
2. esptool-js sayfasındaki **Console** bölümünde (115200, Start) aşağıyı yazıp Enter'a basın
   (tırnak işaretleri, adında boşluk varsa gereklidir):
   ```
   wifi "Telefonun Adi" "hotspotsifresi"
   ```
   Cevap: `# CMD saved: wifi ssid="Telefonun Adi" password=(set) ...`
3. `reboot` yazıp Enter. Açılışta `# NET connected, ip 172.20.10.3 rssi -55 dBm` benzeri satır görünmeli.
   Diğer komutlar: `show` (ayarları göster), `help`.

## 3. Seri port çıktısının anlamı
| Satır | Anlamı |
|---|---|
| `# INIT MPU6050 OK` | Sensör bulundu (FAIL ise kablolama — teşhis yazılımına dönün) |
| `# LOG file /SURVEY_003.CSV OK` | Bu oturumun kayıt dosyası |
| `# NET connected ...` / `# NET disconnected` | Wi-Fi durumu |
| `42,43012345,2026-11-...` | Bir ölçüm kaydı (her saniye) |
| `# STAT ...` (10 sn'de bir) | Sağlık raporu: örnekleme aralığı, kayıp, SD, sensör hataları |
| `# MARK 3` | Butona basıldı, 3 numaralı gözlem noktası |

## 4. LED'ler
| LED | Anlamı |
|---|---|
| Yeşil **sürekli** | GNSS konumu var (fix) |
| Yeşil **yanıp sönüyor** | Saat biliniyor, konum yok (açık alana çıkın, bekleyin) |
| Mavi kısa yanıp sönme | Kayıt yazıldı (her saniye) |
| Kırmızı | Bir sensörde veya SD kartta sorun var |

## 5. Bilgisayar tarafı (dashboard) — bir kez kurulum
**Windows:**
1. https://www.python.org/downloads/ → Python 3.11 kurun; kurulumda **"Add python.exe to PATH"** işaretli olsun.
2. Size gönderdiğim proje zip dosyasını masaüstüne açın. Klasörde adres çubuğuna `cmd` yazıp Enter.
3. Şu komutlar (bir kez):
   ```
   py -m venv .venv
   .venv\Scripts\pip install -e "pc[gui]"
   ```
4. Dashboard: `.venv\Scripts\python -m surveypc.dashboard`
   Windows Güvenlik Duvarı sorarsa **"Özel ağlar" için izin verin** (yoksa veri gelmez).

**macOS:** `python3 -m venv .venv && .venv/bin/pip install -e "pc[gui]"`, sonra
`.venv/bin/python -m surveypc.dashboard`.

Cihazdan veri gelince üstte yeşil **Connected** görünür. Dashboard aynı anda `data/processed/telemetry_*.csv`
dosyasına kayıt yapar — **saha sonrası bu dosyayı da gönderin** (gecikme ölçümü bundan çıkar).

**Uzun seri port kaydı** (statik GNSS testi vb.): `.venv\Scripts\python -m surveypc.serial_log --list`
ile portu bulun (ör. COM5), sonra `.venv\Scripts\python -m surveypc.serial_log --port COM5 --out kayit.txt`.
Ctrl+C ile durur. (Kayıt sırasında esptool-js sayfası kapalı olmalı; port aynı anda tek programa açılır.)

## 6. Sorun giderme
| Belirti | Çözüm |
|---|---|
| `# NET no Wi-Fi configured` | Bölüm 2'yi yapın |
| `# NET connecting...` sonrası bağlanmıyor | Şifre? Hotspot 2.4 GHz mi? `show` ile kontrol edin |
| Cihaz bağlı ama dashboard "Waiting for device" | Bilgisayar aynı hotspot'ta mı? Güvenlik duvarı izni? Bazı telefonlar cihazlar arası trafiği engeller → başka telefon deneyin |
| `# LOG ... FAIL (will retry)` | SD kart takılı mı, FAT32 mi? |
| Kırmızı LED | Seri porttaki `# INIT ... FAIL` / `# STAT` satırlarına bakın, bana gönderin |
