### Operasyonel Test Senaryoları (Adım Adım)

Bu bölüm, sahada “çalışıyor mu?” sorusunu hızlı yanıtlamak için **gözlemlenebilir çıktılara** dayanır:
- PC tarafı: status bar metinleri + canlı log konsolu + video paneli overlay.
- Raspi tarafı: stderr’e basılan `SANCAK_LOG_*` loglar.

#### PC uygulamasını doğru hedefe bağlama (konfigürasyon testi)
1) PC uygulamasını başlatmadan önce (opsiyonel) ortam değişkenlerini ayarla:
   - `RASPI_HOST` (ör. `192.168.1.50`)
   - `VIDEO_UDP_PORT` (varsayılanla aynı olmalı)
   - `TEL_TCP_PORT` (varsayılanla aynı olmalı)

Örnek (PowerShell, sadece o terminal oturumu için)
```
$env:RASPI_HOST="192.168.1.50"
$env:VIDEO_UDP_PORT="5001"
$env:TEL_TCP_PORT="5002"
```

Örnek (CMD, sadece o oturum için)
```
set RASPI_HOST=192.168.1.50
set VIDEO_UDP_PORT=5001
set TEL_TCP_PORT=5002
```

Beklenen çıktı (PC status bar)
- Uygulama açılır açılmaz:
  - `Video: Dinleniyor`
  - `Telemetri: Baglaniyor...` (hemen ardından bağlanırsa `Bagli`)

Beklenen çıktı (PC log konsolu)
- En azından şu satırları görmen beklenir:
  - `Sunucuya baglaniliyor: <RASPI_HOST>:<TEL_TCP_PORT>`
  - `TCP baglandi`
  - Durum değişimi logları: `Telemetri baglaniyor...` → `Telemetri baglandi`

Sorun/teşhis
- `TCP baglanti denemesi basarisiz` görüyorsan: IP/port, firewall veya Raspi tarafında server’ın dinlememesi.

#### Telemetri protokolü (SNK2) “sağlamlık” testi
Amaç: TCP stream bozulsa bile PC’nin resync olup devam edebilmesi.

Beklenen çıktı (PC log konsolu)
- Zaman zaman (özellikle hatalı veri gelirse) şu uyarılar görülebilir:
  - `Bozuk telemetri paketi alindi (Magic mismatch)!`
  - `Bozuk telemetri paketi alindi (Version mismatch)!`
  - `Bozuk telemetri paketi alindi (Frame type/size)!`

Yorum
- Bu uyarılar **her frame’de** sürekli akıyorsa sistem yanlış porta bağlanıyor olabilir veya karşı uç SNK2 frame göndermiyordur.

#### AimResult overlay testi (hedef varken/yokken)
Amaç: Raspi’nin ürettiği nişan noktasının PC video panelinde doğru şekilde çizildiğini doğrulamak.

1) Hedef yokken
- Beklenen davranış:
  - Video panelinde ya overlay hiç çizilmez ya da “son bilinen noktaya” kırmızımsı crosshair görülebilir.
  - `aim.valid` sahte true olmamalı.

2) Hedef varken (balon tespit + segment + aim)
- Beklenen davranış:
  - Video panelinde **yeşil crosshair** görünür.
  - Crosshair yakınında etiket: `ID:<class_id>  (x,y)`

Beklenen telemetri alanları (PC tarafı parse edilen AimResult)
- `AimResult.valid == true`
- `AimResult.corrected_x/y` görüntü boyutları içinde olmalı:
  - $0 \le x < width$
  - $0 \le y < height$
- `AimResult.frame_id` zamanla artar (her aim frame’de).

Sorun/teşhis
- Crosshair panelin yanlış yerinde ise: kamera çözünürlüğü/telemetrideki koordinatların aynı referansta olduğundan emin ol (Raspi frame boyutu ile PC’nin aldığı video boyutu eşleşmeli).

#### UDP video akışı testi (SNK1)
Amaç: UDP fragment’lı JPEG akışının PC tarafında toparlanıp stabil görüntü verdiğini doğrulamak.

Beklenen çıktı (PC status bar)
- Raspi video gönderiyorsa: `Video: Aliniyor`
- Gönderim yoksa: `Video: Dinleniyor` veya `Bekleniyor`

Beklenen davranış
- Görüntü “takıla takıla” gelse bile zamanla akıcı bir şekilde yenilenmeli.
- Çok paket kaybında bazı frame’ler atlanabilir; bu normaldir (TCP gibi bekleyip gecikmeyi büyütmez).

Sorun/teşhis
- `Video: Hata!` → PC’nin UDP bind IP/port’u, izinler veya port çakışması.

#### Güvenlik senaryoları (fail-safe) — gözlemlenebilir çıktılar
1) Watchdog (kamera kesilmesi)
- Adım: Raspi’de kamerayı geçici olarak devre dışı bırak (çek/kapat).
- Beklenen:
  - Raspi kontrol döngüsü SafeLock’a gider ve fire false olur.
  - PC tarafında kısa süre sonra yeşil overlay kaybolmalı (veya `valid=false` nedeniyle kırmızı “son nokta” davranışına düşmeli).

2) Lost-target (hedef kadrajdan çıkması)
- Adım: Balonu kadrajdan çıkar.
- Beklenen:
  - Engaging devam etmemeli; ateş komutu kesilmeli.
  - PC’de `AimResult.valid` true kalıyorsa bile crosshair “hedef dışına” sürüklenmemeli (pratikte valid=false’a düşmesi tercih edilir).

3) Geofence
- Adım: Hedefi görüntünün uçlarına taşı (servo limitlerine zorlayacak şekilde).
- Beklenen:
  - Geofence dışı nişan isteklerinde SafeLock + fire=false.
  - PC tarafında overlay güvenli şekilde kaybolmalı veya valid=false’a düşmeli.
