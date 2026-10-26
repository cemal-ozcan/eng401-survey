# 1. Yazılımı ESP32'ye Yükleme (kurulum gerektirmez)

Gerekenler: **Chrome veya Edge** tarayıcı (Safari/Firefox çalışmaz), **veri hatlı** USB kablo,
bana gönderilen `.bin` dosyası (ör. `eng401-v0.1-diag.bin`).

## A. Bir kez yapılacak: sürücü
1. ESP32'yi USB ile bilgisayara takın.
2. Kartın USB girişinin yanındaki küçük siyah çipin üstündeki yazıya bakın:
   - **CP2102** yazıyorsa: "CP210x driver" (Silicon Labs) kurun.
   - **CH340** yazıyorsa: "CH340 driver" (WCH) kurun.
3. Windows'ta "Aygıt Yöneticisi → Bağlantı noktaları (COM)" altında yeni bir **COMx** görünmeli.
   Görünmüyorsa: kablo büyük ihtimalle sadece şarj kablosudur → başka kablo deneyin.

## B. Yükleme
1. Tarayıcıda açın: **https://espressif.github.io/esptool-js/**
2. **Baudrate**: `460800` seçin → **Connect** → listeden ESP32'nin portunu seçin → **Bağlan**.
   - Bağlanmazsa: karttaki **BOOT** butonuna basılı tutun, tekrar **Connect**'e basın,
     bağlanınca bırakın.
3. "Flash Address" kutusuna **`0x0`** yazın (sıfır, x, sıfır). Yanındaki **Choose File** ile `.bin` dosyasını seçin.
4. **Program** butonuna basın. İlerleme %100 olana kadar bekleyin (~30 sn). Kabloyu çıkarmayın.
5. **Disconnect**'e basın.

## C. Çıktıyı görme ve bana gönderme
1. Aynı sayfanın alt kısmındaki **Console** bölümünde Baudrate **`115200`** seçin → **Start**.
2. Karttaki **EN** (veya RST) butonuna bir kez basın → yazılar akmaya başlar.
3. `COPY FROM HERE` ile `COPY UNTIL HERE` arasındaki bloğu seçip kopyalayın,
   WhatsApp/e-postayla bana **metin olarak** gönderin (ekran fotoğrafı değil).
   Mümkünse tüm konsol çıktısını da bir `.txt` dosyasına yapıştırıp ekleyin.

## Sorun giderme
| Belirti | Çözüm |
|---|---|
| Port listesi boş | Sürücü (A) kurulu mu? Kablo veri hatlı mı? |
| "Failed to connect" | BOOT'a basılı tutarak Connect (B.2) |
| Konsolda anlamsız karakterler | Console baudrate'i **115200** olmalı |
| Hiç yazı yok | EN/RST butonuna basın |
