# Teknofest Sancak HSS — Mülakat / Sınav Soru Arşivi

Bu dosya, projeyle ilgili soru-cevap çalışması için arşivdir.

Kurallar:
- Ben her seferinde 1 soru sorarım (kolay → zor).
- Sen cevap verirsin.
- Sonra ben:
  - Doğruysa: “Doğru” derim ve eksik kalan önemli noktaları eklerim.
  - Yanlışsa: “Yanlış” derim, doğru cevabı açıklarım.
- Her soru ve senin cevabın buraya işlenir.

---

## Soru 1 (Kolay) — Projenin iki ana bileşeni

Soru:
- Bu repo iki ana uygulamadan oluşuyor. Bunlar hangileri ve her birinin temel görevi nedir?

Senin cevabın:
- Biri PC diğeri raspi.
- PC klasörünün olayı kontrol arayüz uygulamasını derlemek ve hiç bir bağımlılığı olmayan tek bir exe dosyasına çevirmek.
- Raspi klasörü asıl proje; Raspberry Pi’de çalışacak tüm görüntü işleme orada.

Değerlendirme:
- Doğru (genel çerçeve). Küçük ek/düzeltmeler:
  - PC tarafı sadece “derlemek” değil; aynı zamanda GCS/arayüz olarak UDP video + TCP telemetri alıp ekranda gösterir ve bazı komutları geri gönderebilir.
  - “Hiç bağımlılığı olmayan tek exe” hedefi pratikte çoğunlukla doğru (tek binary), ama sistemde yine de GPU/OpenGL sürücüleri ve platform runtime’ları gibi dış bağımlılıklar olabilir.

Doğru cevap / ek notlar:
- `raspi/`: Kameradan görüntü alır; YOLO tabanlı tespit + (balon için) HSV segmentasyon + takip/mesafe/balistik ile aim (nişan) üretir; güvenlik katmanları (geofence, tetik disiplini) ile ateş izni hesaplar; telemetriyi TCP ile ve videoyu UDP ile PC’ye aktarabilir; opsiyonel seri port ile taret/Arduino’ya komut yollar.
- `PC/`: GCS/arayüz uygulamasıdır; UDP üzerinden gelen JPEG video akışını alıp çözer ve UI’da gösterir; TCP telemetriyi alır ve overlay/crosshair çizer; kullanıcı girdisiyle bazı komutları TCP üzerinden geri iletebilir.

---

## Soru 2 (Kolay) — Video ve telemetri hangi protokollerle taşınıyor?

Soru:
- Raspi → PC yönünde video akışı ve telemetri hangi taşıma protokollerini kullanır (UDP/TCP)? Kısaca neden bu seçim yapılmış olabilir?

Senin cevabın:
- Video aktarımında UDP kullanıyor, komut aktarımında TCP kullanıyor.
- Videoda UDP kullanma nedenimiz hızlı paket aktarımı yapması; video/görüntülü/ sesli sohbetlerde kullanılır. Dezavantajı bazen paket kaybı olsa da görüntü aktığı için önemsenmez.
- Komut gönderirken TCP kullanma nedenimiz paket kaybı olmaması; paket kaybı yaşarsak verdiğimiz komutlar gitmeyebilir, bu istenmeyen bir durum.

Değerlendirme:
- Doğru. Küçük ek/düzeltmeler:
  - Raspi → PC “telemetri” TCP üstünden gider; PC → Raspi yönünde kullanıcı komutları da (varsa) aynı TCP kanalından satır-bazlı mesajlar olarak gönderilebilir.
  - UDP’nin pratik avantajı sadece hız değil, aynı zamanda gecikme/jitter: kayıp olunca bekleyip gecikmeyi büyütmek yerine (TCP gibi) frame’i atlayıp yenisine geçebilirsin.

Doğru cevap / ek notlar:
- Video: UDP üstünden taşınır (SNK1). JPEG frame’ler MTU’ya sığmadığı için fragmanlanır; PC tarafında fragmanlar birleştirilip JPEG decode edilir.
- Telemetri: TCP üstünden taşınır (SNK2). Stream içinde “frame” kavramını korumak için magic/version/type/size içeren bir çerçeveleme kullanılır; PC tarafı bozulma durumunda resync etmeye çalışır.
- Neden UDP video?: düşük gecikme; paket kaybında “sonraki frame”e hızlı geçiş; gerçek zamanlı akışta daha iyi kullanıcı hissi.
- Neden TCP telemetri/komut?: teslim garantisi + sıralama; küçük ama kritik mesajların kaybolmasını engellemek.

---

## Soru 3 (Kolay-Orta) — UDP video akışında fragmanlama ne demek?

Soru:
- Tek bir JPEG frame’in UDP ile “fragmanlara bölünmesi” ne işe yarar? Neden tek paketle göndermiyoruz? Kısaca MTU kavramını da açıklayarak anlat.

Senin cevabın:
- MTU veri paketlerinin max boyut sınırıdır.
- Dosyaları tek paket yerine bölerek atmamızın nedeni bu sınırdır.
- Paketleri çok fazla bölersek aktarım yavaşlayabilir; o yüzden genelde MTU sınırı büyüklüğündeki parçalara bölünür.

Değerlendirme:
- Doğru. Ek notlar:
  - Büyük bir JPEG’i tek UDP datagramı olarak yollarsan IP seviyesinde parçalanma (IP fragmentation) oluşabilir; bu hem kayıp riskini artırır hem de bazı ağlarda parçalı paketler drop edilebilir.
  - Uygulama seviyesinde fragmanlamak (SNK1) alıcıya “hangi parçalar eksik?” bilgisini ve birleştirme mantığını kontrol etme imkânı verir.

Doğru cevap / ek notlar:
- MTU (Maximum Transmission Unit), bir link üzerinde tek bir frame içinde taşınabilecek maksimum payload boyutudur (Ethernet’te pratikte genelde ~1500 byte IP payload civarı).
- JPEG frame çoğu zaman MTU’dan büyük olduğu için tek UDP datagramına sığmaz.
- Bu yüzden gönderici JPEG’i MTU’ya uygun boyutlarda fragmanlara böler; her fragmana “frame_id + frag_index + frag_count” gibi alanlar koyar.
- Alıcı taraf fragmanları toplar; eksik parça varsa frame’i atlayıp yeni frame’e geçebilir (realtime akış için daha iyi).

---

## Soru 4 (Orta) — TCP telemetride “frame” nasıl korunuyor?

Soru:
- TCP bir byte-stream’tir (mesaj sınırı yok). Bu projede telemetride (SNK2) “tek tek frame” kavramı nasıl sağlanıyor? Magic/version/size gibi alanların amacı nedir?

Senin cevabın:
- Bilmiyorum.

Değerlendirme:
- Yanıt eksik; detaylı açıklama aşağıda.

Doğru cevap / ek notlar:
- TCP, byte stream olduğu için “mesaj sınırı” yoktur; yani bir frame’in nerede başladığı/nerede bittiği kendiliğinden anlaşılmaz.
- Bu projede, her telemetri frame’in başında özel alanlar (magic, version, type, size) bulunur:
  - **magic**: Frame’in başını tanımak için sabit bir sayı/byte dizisi (ör. 0xA1B2C3D4). Yanlışsa, stream bozulmuş demektir; resync için kullanılır.
  - **version**: Protokolün sürümünü belirtir; farklı versiyonlar için uyumluluk kontrolü sağlar.
  - **type**: Frame’in ne tür veri içerdiğini belirtir (ör. AimResult, Telemetry, Command).
  - **size**: Frame’in payload’ının kaç byte olduğunu belirtir; böylece tam frame’i stream’den çekebilirsin.
- Alıcı kod, stream’den sürekli okur; magic doğruysa, version/type/size ile frame’i parse eder. Magic yanlışsa, stream’de kayma olmuş demektir; magic araması yaparak resync etmeye çalışır.

Kod ve dosya referansları:
- Bu mantık genellikle [raspi/src/TelemetryClient.cpp](raspi/src/TelemetryClient.cpp) ve [PC/src/TelemetryClient.cpp](PC/src/TelemetryClient.cpp) dosyalarında uygulanır.
- Frame struct’ları ve çerçeveleme alanları [raspi/include/TelemetryClient.hpp](raspi/include/TelemetryClient.hpp) ve [PC/include/TelemetryClient.hpp](PC/include/TelemetryClient.hpp) dosyalarında tanımlanır.
- Frame okuma/parsing kodu: `TelemetryClient::readFrame()` veya benzer fonksiyonlar; magic/version/type/size alanlarını okur, ardından payload’ı çeker.

Özet:
- TCP stream’de frame sınırı yok; bu yüzden frame başı ve boyutu için magic/version/type/size alanları eklenir.
- Kodda bu alanlar struct olarak tanımlanır ve stream’den okuma/parsing sırasında kullanılır.
- Magic alanı, stream bozulursa resync için kritik önemdedir.

---

## Soru 5 (Orta) — YOLO dedektör pipeline’ı

Soru:
- Raspi tarafında YOLO dedektör pipeline’ı nasıl çalışır? Hangi dosyalarda hangi adımlar var? (Kamera → YOLO → segmentasyon → takip → aim)

Senin cevabın:
- (bekleniyor)

Değerlendirme:
- (bekleniyor)

Doğru cevap / ek notlar:
- (bekleniyor)
