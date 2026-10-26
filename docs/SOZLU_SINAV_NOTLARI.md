# Sözlü Sınav Çalışma Notları (tüm ekip için)

Ödev: "Her üye tüm sistemi anlamalı; donanım, yazılım, GNSS, koordinat sistemleri, veri işleme,
GIS ve test hakkında sorgulanabilir." Bu not, en olası soruları ve kısa doğru cevaplarını içerir.
Ayrıntı için ilgili dosya parantez içinde.

## 0. 30 saniyelik özet
Taşınabilir bir ölçüm cihazı yaptık. ESP32 her saniye GNSS konumu, IMU, sıcaklık/basınç/nem ve mesafe
ölçüyor; hepsine aynı anda zaman damgası basıyor, konumu cihaz üzerinde UTM'e çeviriyor, SD karta
yazıyor ve Wi-Fi ile bilgisayara gönderiyor. Bilgisayarda canlı dashboard var; veriler sonra Python
analiz betikleriyle doğrulanıyor ve QGIS'te haritaya dönüşüyor. Yazılım, donanım gelmeden
simülasyonla geliştirildi ve 140'tan fazla otomatik testle doğrulandı.

## 1. Donanım ve haberleşme
**S: Sensörler ESP32'ye nasıl bağlı?**
GNSS: UART (seri, 9600 baud, GPIO16/17). IMU, BME280, VL53L0X: aynı I²C hattı (SDA 21, SCL 22) —
her cihazın farklı adresi var (0x68, 0x76, 0x29). SD kart: SPI (CS 5, SCK 18, MOSI 23, MISO 19).

**S: UART, I²C, SPI farkı?**
UART: iki hat, saat yok, noktadan noktaya, baud önceden anlaşılır. I²C: iki hat (veri + saat), çok
cihaz aynı hatta, adresle seçilir, yavaş (100–400 kHz). SPI: 4 hat, her cihaza ayrı CS, hızlı (MHz) —
SD kart için uygun.

**S: Neden sensörler 3.3 V?** ESP32'nin pinleri 3.3 V; 5 V sinyal ESP32'ye zarar verebilir.
SD modülünün kendi regülatörü ve seviye dönüştürücüsü olduğu için o 5 V'tan beslenir.

**S: Doğru sensör takıldığını nasıl anladınız?** Teşhis yazılımı adrese cevap gelmesiyle yetinmez,
kimlik register'ını okur (BME280 = 0x60; 0x58 dönerse BMP280'dir, nem ölçmez).

## 2. GNSS
**S: NMEA nedir, hangi cümleleri kullandınız?** Alıcının metin çıktısı; her satır `$` ile başlar,
`*HH` checksum ile biter (aradaki karakterlerin XOR'u). GGA: fix kalitesi, uydu, HDOP, yükseklik, konum.
RMC: tarih, durum (A/V), hız, rota. VTG: hız ve rota. (`firmware/lib/nmea`)

**S: `3955.24620,N` nasıl dereceye çevrilir?** ddmm.mmmm: 39 + 55.2462/60 = 39.92077°. S ve W negatif.

**S: HDOP nedir?** Uyduların gökyüzündeki geometrisinin yatay konum hatasını ne kadar büyüttüğü.
Küçük iyi: ≤ 2 iyi, 2–5 orta, > 5 zayıf. Uydular tek tarafta toplanırsa büyür.

**S: Fix yokken ne oluyor?** Konum alanları boş kalır, `fix=0`, bayrak kapalı. Zaman yine akar
(alıcının saati çalışır). Kayıt atılmaz — veri bütünlüğü hesabında görünür.

## 3. Koordinat sistemleri ve UTM
**S: WGS84 nedir?** GPS'in kullandığı yer merkezli datum ve elipsoit (a = 6 378 137 m, f = 1/298.257223563).
Enlem/boylam bu elipsoide göredir.

**S: Neden UTM?** Enlem/boylam açısaldır; mesafe ve alan hesabı zordur. UTM metre cinsinden düzlem
koordinat verir (Easting, Northing), küçük alanlarda bozulma çok azdır.

**S: UTM nasıl çalışır?** Dünya 6°'lik 60 dilime bölünür; her dilim kendi merkez meridyenine teğet
Transverse Mercator projeksiyonudur. Ölçek faktörü k₀ = 0.9996 (merkez meridyende hafif küçültme,
dilim kenarında ≈1). Easting'e 500 000 m eklenir (negatif olmasın), güney yarımkürede Northing'e
10 000 000 m. Dilim = floor((boylam + 180)/6) + 1 → Ankara 36, İstanbul/İzmir 35. İstisnalar: Norveç
(32V), Svalbard (31X–37X).

**S: Hangi formülü kullandınız?** Krüger serisi (6. derece). Enlem önce konform enleme çevrilir,
sonra küresel Mercator koordinatlarına bir seri düzeltmesi eklenir. Dilim içinde hata < 1 mm.
(`firmware/lib/geo/utm.cpp`)

**S: Doğru olduğunu nasıl kanıtladınız?** Bağımsız referans pyproj (PROJ kütüphanesi) ile karşılaştırma:
18 test noktasında (dilim kenarları, ekvator, güney, istisnalar) en büyük fark 0.0006 mm.
Kayıtlı veride ~1 mm fark görünür; bu CSV'deki yuvarlamadandır (enlem 8 ondalık ≈ 1.1 mm).

**S: Rota iki dilime yayılırsa?** Bir dilim seçip "zorla dilim" (forced zone) ile hepsini aynı dilimde
hesaplayabiliyoruz; dashboard dilim değişince izi yeniden başlatır.

## 4. Zaman damgası (R4)
**S: Ortak zaman referansı nasıl sağlandı?** Her kayıt, örneklendiği anda ESP32'nin `esp_timer`
saatiyle (µs, monotonik) damgalanır. GNSS'ten gelen UTC ile bu saat arasındaki fark (offset) sürekli
tahmin edilir: UTC = t_us + offset. Böylece tüm sensörler aynı an ile ilişkilendirilir, fix gitse de UTC sürer.

**S: Offset nasıl tahmin ediliyor?** Her NMEA cümlesi gecikmeli gelir; `UTC − alış zamanı` değeri
gecikme ne kadar azsa o kadar büyük. Son 32 örneğin **maksimumu** = en az gecikmeli ölçüm. (`lib/rt/timebase.cpp`)

**S: Sınırlaması?** PPS hattı olmadan alıcının sabit çıkış gecikmesi (onlarca ms) görülemez → UTC bu
kadar kaymalı. PPS ile µs seviyesine inilebilirdi.

## 5. Gerçek zamanlı firmware (R5) — en çok puanlı bölüm
**S: Neden FreeRTOS / task?** Farklı hızlardaki işler (GNSS 1 Hz, ToF 50 Hz, SD yazma, Wi-Fi) birbirini
beklemeden çalışsın. Bir işin yavaşlığı (ör. SD yazma 50 ms) örneklemeyi geciktirmesin.

**S: Task'larınız?** gnssTask (UART→ayrıştırıcı), sensorTask (sensör okuma), sampleTask (her periyotta
kayıt, en yüksek öncelik), logTask (SD), netTask (Wi-Fi), uiTask (LED/buton), monitorTask (sağlık raporu),
consoleTask (Wi-Fi ayarı).

**S: `delay()` ile `vTaskDelayUntil` farkı?** `delay(1000)` "1 sn bekle" der; işin süresi eklenince
periyot kayar. `vTaskDelayUntil` "bir önceki uyanıştan 1 sn sonra uyan" der; periyot sabit kalır,
beklerken CPU başka task'lara geçer.

**S: Kuyruk (queue) ve mutex ne işe yarar?** Kuyruk: task'lar arası veri aktarımı (sampleTask → logTask),
üretici ve tüketici farklı hızda çalışabilir. Mutex: ortak veriye (son GNSS verisi) aynı anda tek task
erişsin diye kilit; yoksa yarım güncellenmiş veri okunabilir (race condition).

**S: Kuyruk dolarsa?** sampleTask beklemez (zaman aşımı 0), kaydı atar ve `q_drop` sayacını artırır —
örnekleme asla bloklanmaz, kayıp ise görünür kalır.

**S: Sensör koparsa?** Art arda 3 hata → sensör "FAIL", 1-2-4…30 sn aralıkla yeniden başlatma denenir
(exponential back-off). Kırmızı LED yanar, kayıtta o sensörün bayrağı kapanır.

**S: Bloklamadığınızı nasıl ölçtünüz?** monitorTask istenen/gerçekleşen periyodu (ortalama, min, maks, std),
kaçırılan örnekleri ve her task'ın boş yığın alanını raporlar; analizde `t_us` farklarından da hesaplanır.

## 6. Veri kaydı (R6)
**S: SD'ye nasıl yazıyorsunuz?** 4 KB RAM tamponunda biriktirip blok halinde; ayrıca 5 sn'de bir
zorla yazma. Neden: SD küçük yazmalarda yavaş ve aşınır; elektrik kesilirse en fazla 5 sn veri gider.
Her açılışta yeni dosya, eskilerin üzerine yazılmaz. Kart çıkarılırsa tampon RAM'de tutulur, takılınca yeni dosyaya devam.

**S: Veri formatı?** CSV, saniyede bir satır, 29 sütun, birimler `docs/ICD.md`'de. Geçersiz değer boş
bırakılır (0 yazılmaz — "0 °C" ile "ölçüm yok" karışmasın).

## 7. Haberleşme (R7)
**S: Neden UDP, TCP değil?** TCP kaybı yeniden göndererek gizler ve gecikmeyi şişirir; UDP'de her paketin
sıra numarası var, kayıp doğrudan sayılır. Tam veri zaten SD'de. Ayrıca IP ayarı gerekmez: cihaz önce
yayın (broadcast) yapar, PC ping atınca adresi öğrenir.

**S: Gecikmeyi nasıl ölçtünüz? İki saat senkron değil ki.** NTP yöntemi: PC t1'de ping, cihaz t2'de alır
t3'te cevaplar, PC t4'te alır. rtt = (t4−t1) − (t3−t2); offset = ((t2−t1) + (t3−t4)) / 2. Hata ≤ rtt/2;
en küçük rtt'li ping kullanılır. Gecikme = PC alış zamanı − (örnekleme zamanı − offset). Yani sensörün
ölçtüğü andan PC'ye ulaşana kadar geçen süre.

**S: Veri bütünlüğü nasıl hesaplanır?** Geçerli gözlem / beklenen gözlem × 100. Beklenen = süre/periyot + 1.
"Geçerli" farklı seviyelerde verilir (kayıt var, fix var, tüm sensörler OK). Telemetri için sıra numarası boşlukları.

## 8. Dashboard (R8)
**S: Mimari?** UDP alıcısı ayrı thread'de; kayıtları kuyruğa koyar, arayüz 5 Hz'de okur → ağ yavaşlasa da
pencere donmaz. Gösterilen: bağlantı, kayıp, gecikme, konum, UTM, fix/uydu/HDOP, sensörler, UTM'de canlı iz.

## 9. Analiz ve GIS (R9, R11)
**S: RMSE ile standart sapma farkı?** d = √(ΔE² + ΔN²). Ortalama hata = mean(d); RMSE = √mean(d²);
std = d'nin ortalama etrafındaki dağılımı. RMSE sistematik kaymayı (bias) da içerir, std içermez.
Bias = ortalama ΔE, ΔN ayrıca raporlanır. CEP95: noktaların %95'inin içinde kaldığı yarıçap.

**S: GNSS doğruluğunu neye göre ölçtünüz?** Koordinatı bilinen referans noktalara (Geomatik ekibinin
ölçümü): statik testte her fix o noktayla; gözlem noktalarında butona basılan andaki ±5 sn ortalaması.

**S: Rota karşılaştırması?** Her fix'in planlanan rotaya dik uzaklığı (segment üzerine izdüşüm);
ortalama, RMSE, %95, maks, "2 m / 5 m içinde kalan %". Ölçülen yol uzunluğu gürültü yüzünden planlanandan uzun çıkar.

**S: GIS ürünleri?** GeoJSON (WGS84, koordinat sırası boylam-enlem): yürünen iz (fix kesilen yerde bölünür),
öznitelikli noktalar, gözlem noktaları, planlanan rota. QGIS'te UTM projeksiyonunda, "Graduated" sembolojiyle
tematik harita (ör. sıcaklık).

## 10. Test ve geliştirme yöntemi
**S: Donanım yokken nasıl geliştirdiniz?** Donanım soyutlama katmanı: uygulama sensörleri arayüzler
üzerinden görür; tek bir bayrakla gerçek sürücü yerine gerçekçi sahte (mock) sensör kullanılır — gürültü,
fix kaybı, sensör arızası dahil. Mock GNSS gerçek NMEA üretir, ayrıştırıcı hep test edilir. 10 dakikalık
uçtan uca simülasyon ve PC'de "sanal ESP32" ile tüm zincir kart olmadan çalıştırıldı.

**S: Firmware ile PC'nin aynı formatı kullandığından nasıl emindiniz?** Elle yazılmış ortak "golden" dosyalar:
C++ kodlayıcı ve Python çözücü bu dosyaları bayt bayt üretmek/okumak zorunda; testler her seferinde kontrol eder.

**S: Bulduğunuz bir hatayı anlatın.** (Örnekler) SD hatasından sonra yeniden deneme zamanlayıcısı
sıfırlanmıyordu — kart çıkarma testi yakaladı. Wi-Fi bayrağı sadece telemetri kopyasına yazılıyordu, SD
kopyasında yoktu. Dashboard açılışı 4 sn'lik sahte gecikme gösteriyordu — telemetri pencere açıldıktan sonra başlatıldı.
