# 4. Saha Öncesi Testler ve Bana Gönderilecek Veriler

Her testten sonra ilgili dosyaları **dosya olarak** (ekran görüntüsü değil) gönderin. Dosya adına tarih ve test adı yazın.

| # | Test | Yazılım | Süre | Nasıl | Gönderilecek |
|---|---|---|---|---|---|
| T1 | Wi-Fi + dashboard | v0.2-app-mock | 10 dk | Sensörsüz ESP32, kılavuz 03 Bölüm 2–5 | Dashboard ekran görüntüsü + `telemetry_*.csv` |
| T2 | Tüm sensörler masada | v0.2-app | 10 dk | Masada sabit; seri kayıt aracı açık | Seri kayıt `.txt` + SD'deki `SURVEY_*.CSV` |
| T3 | Örnekleme kararlılığı | v0.2-app | **30 dk** | T2 gibi, dokunmadan | `.txt` (içindeki `# STAT` satırları) + CSV |
| T4 | SD çıkar-tak | v0.2-app | 2 dk | Çalışırken SD'yi çıkarın, 20 sn bekleyin, takın | `.txt` (`# LOG ERROR` → `recovered` görünmeli) |
| T5 | Wi-Fi menzili | v0.2-app | 10 dk | Hotspot'tan 5-10-20-40 m uzaklaşın, her mesafede 1 dk | `telemetry_*.csv` + hangi saatte hangi mesafe |
| T6 | Statik GNSS | v0.1.1-diag (`n` modu) **veya** v0.2-app | **≥ 20 dk** | Açık alanda, cihaz sabit, koordinatı bilinen noktada (Geomatik ekibiyle) | `.txt` (seri kayıt) + nokta koordinatı |
| T7 | Buton/gözlem noktası | v0.2-app | 5 dk | 5 kez, araya 10 sn koyarak basın | `.txt` (`# MARK` satırları) |
| T8 | Pil/powerbank süresi | v0.2-app | uzun | Tam dolu powerbank, kapanana kadar | Başlangıç/bitiş saati + CSV |

**Her teste not ekleyin:** tarih/saat, konum (iç/dış mekan), hangi parçalar bağlı, olağan dışı durumlar.

Bu veriler ödevin zorunlu testleri (Bölüm 9) için gerçek ölçümlerdir: T3 → örnekleme performansı,
T5 → gecikme ve veri bütünlüğü, T6 → GNSS doğruluğu, T2/T6 → sensör tekrarlanabilirliği.
