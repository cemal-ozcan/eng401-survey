# Geomatik Ekibi — Saha Prosedürü, Analiz ve GIS Kılavuzu

Bu kılavuz, cihazın topladığı veriden ödevin **zorunlu testlerini** (Bölüm 9) ve
**GIS ürünlerini** (R9, D4) üretmek içindir. Analizlerin tamamını tek komut yapar;
sizin işiniz doğru **referans verisini** hazırlamak, sahada **prosedüre uymak** ve
sonuçları **yorumlamaktır**.

## 1. Cihazın ürettiği dosyalar
| Dosya | Nereden | İçerik |
|---|---|---|
| `SURVEY_NNN.CSV` | Cihazın SD kartı | Her saniye bir satır: zaman, GNSS, UTM, sensörler (tam veri) |
| `telemetry_*.csv` | Dashboard/alıcı çalışan bilgisayar, `data/processed/` | Wi-Fi ile gelen satırlar + gecikme |

Sütunların anlamı ve birimleri: `docs/ICD.md` (Bölüm 2). Önemli olanlar:
`utc`, `fix` (0 = konum yok), `sats`, `hdop`, `lat`/`lon` (WGS84), `utm_zone`, `easting`, `northing`,
`temp`, `pres`, `hum`, `dist`, `mark` (butona basılan gözlem noktası numarası).

## 2. Sizin hazırlayacağınız referans dosyaları
Hepsi basit CSV. Koordinatlar **WGS84 enlem/boylam** veya **UTM Easting/Northing** olabilir
(aynı dosyada biri). UTM verirseniz cihazın kullandığı dilimle aynı olmalı (Ankara: 36N, İstanbul/İzmir: 35N).

**a) Statik test referans noktası** (`reference.csv`) — koordinatı bilinen tek nokta:
```
id,lat,lon
REF1,39.89000000,32.78000000
```
**b) Gözlem noktaları** (`points.csv`) — sahada butona basacağınız **sırayla**:
```
id,easting,northing
P1,481234.512,4415678.903
P2,481301.220,4415702.118
```
**c) Planlanan güzergâh** (`route.csv`) — köşe noktaları sırayla (veya QGIS'te çizilmiş bir GeoJSON LineString):
```
vertex,lat,lon
0,39.8900,32.7800
1,39.8900,32.7823
```
Referans koordinatların **kaynağını ve doğruluğunu** (ör. RTK ölçümü, ±2 cm; harita sayısallaştırma, ±1 m)
rapora yazın — GNSS hata sonuçları ancak referans kadar güvenilirdir.

## 3. Saha prosedürü
1. **Açılış:** cihazı açık gökyüzünde açın, yeşil LED **sürekli** yanana kadar (fix) bekleyin (ilk açılışta 1–5 dk).
   Dashboard'da "GPS fix" ve HDOP < 2 görün.
2. **Statik test (≥ 10 dk):** cihazı referans noktasının **tam üstüne** sabitleyin (direk/sehpa), dokunmayın.
   Başlangıç/bitiş saatini not edin. Bu kayıt hem GNSS doğruluğu hem **sensör tekrarlanabilirliği** için kullanılır.
3. **Güzergâh:** planlanan rotayı sabit, normal yürüme hızında izleyin.
4. **Gözlem noktaları:** her noktada durun, cihazı noktanın üstünde tutun, **bir kez butona basın**
   (mavi LED kısa yanar, dashboard'da P1, P2… görünür), **en az 10 saniye** hareketsiz bekleyin. Sıra `points.csv` ile aynı olmalı.
   Yanlış basarsanız not alın (analizde düzeltilir).
5. **Bitiş:** cihazı kapatmadan önce 10 saniye bekleyin (SD karta son yazma). SD karttaki dosyaları ve
   bilgisayardaki `telemetry_*.csv` dosyasını kopyalayın.
6. Saha notları: tarih, saat, hava, kim taşıdı, olağan dışı durumlar (ağaç altı, bina yanı → multipath).

## 4. Analizi çalıştırma
Kurulum (bir kez, yazılım sorumlusu yardım eder): proje klasöründe `python3 -m venv .venv` ve
`.venv/bin/pip install -e "pc[dev,gui]"`. Sonra:
```
.venv/bin/python analysis/run_all.py \
    --survey SURVEY_003.CSV \
    --telemetry telemetry_20261210_101500.csv \
    --static SURVEY_002.CSV --reference reference.csv \
    --marks-reference points.csv \
    --route route.csv \
    --out sonuc_saha1
```
Elinizde olmayan girdiyi yazmayın; o bölüm raporda "veri yok" olarak görünür.
Önce örnek veriyle deneyin (Bölüm 7).

## 5. Çıktılar
`sonuc_saha1/report.md` — tüm testlerin tabloları ve şekilleri (rapora aktarılabilir).
`results.json` — aynı sayılar, makine okunur. `figures/` — PNG şekiller. `gis/` — QGIS katmanları.

| Rapor bölümü | Ödev testi | Nasıl hesaplanır |
|---|---|---|
| 1. GNSS accuracy | ortalama hata, RMSE, std, maks, uydu, HDOP | d = √(ΔE²+ΔN²) her epok; ort = mean(d), RMSE = √mean(d²), std = d'nin örneklem std'si; CEP50/95 = d'nin %50/%95 yüzdeliği; bias = ortalama ΔE, ΔN |
| 2. Coordinate transformation | UTM'nin bağımsız doğrulaması | Cihazın UTM'i ile pyproj (PROJ) karşılaştırması; ~1.4 mm'ye kadar fark CSV yuvarlamasıdır |
| 3. Sensor repeatability | ortalama, std, aralık (sabit) | Statik kayıt üzerinde, sadece geçerli bayraklı satırlar |
| 4. Sampling performance | istenen / gerçekleşen aralık | Ardışık `t_us` farkları; kaçırılan örnek sayısı |
| 5. Latency | min / ort / maks | Alıcı CSV'sindeki `latency_ms` (saat senkronu ± belirsizlik) |
| 6. Completeness | geçerli / beklenen × 100 | beklenen = süre / periyot + 1; farklı geçerlilik seviyeleri ayrı ayrı |
| 7. Trajectory | ölçülen vs planlanan rota | Her fix'in rotaya dik uzaklığı: ort, RMSE, %95, maks, 2 m / 5 m içinde kalan % |

**Yorum ipuçları:** Bias ≠ 0 ise sistematik kayma (referans hatası, datum karışıklığı) olabilir.
HDOP yüksek / uydu az olan bölümler hatayla ilişkili mi? (şekildeki zaman serisi). Ölçülen rota uzunluğu
gürültü nedeniyle planlanandan uzun çıkar — bu beklenen bir durumdur, açıklayın.

## 6. QGIS'te harita (D4)
1. **Layer → Add Layer → Add Vector Layer** ile `gis/trajectory.geojson`, `points.geojson`,
   `observations.geojson`, `planned_route.geojson` dosyalarını ekleyin (WGS84, EPSG:4326).
2. Proje CRS'ini UTM dilimine ayarlayın (sağ alt köşe → EPSG:32636 = WGS 84 / UTM 36N).
3. Altlık: **XYZ Tiles → OpenStreetMap** (Browser panelinde).
4. **Tematik harita:** `points` katmanı → Properties → Symbology → **Graduated**, Value = `temp`
   (veya `hum`, `pres`, `dist`, `hdop`), Mode = Natural Breaks, renk rampası seçin → Classify.
5. Gözlem noktalarına etiket: `observations` → Labels → `name`.
6. **Project → New Print Layout:** harita + lejant + ölçek çubuğu + kuzey oku + başlık + CRS bilgisi → PDF/PNG.
7. Alternatif: `points_utm.csv` → Layer → Add Delimited Text Layer, X = `easting`, Y = `northing`,
   CRS = `gis/README.txt`'de yazan EPSG.

## 7. Örnek veriyle deneme (cihaz olmadan)
`data/sample/` simülatörde üretilmiş, gerçek konumu bilinen bir yürüyüştür (Ankara civarında sentetik bir döngü):
```
.venv/bin/python analysis/run_all.py --survey data/sample/mock_survey.csv \
    --telemetry data/sample/mock_telemetry.csv \
    --static data/sample/mock_static.csv --reference data/sample/mock_static_reference.csv \
    --route data/sample/mock_planned_route.csv --truth data/sample/mock_truth.csv \
    --out data/processed/analysis_sample
```
Bu sonuçlar **simülasyondur, rapora saha sonucu olarak konamaz**; sadece araçları öğrenmek içindir.
