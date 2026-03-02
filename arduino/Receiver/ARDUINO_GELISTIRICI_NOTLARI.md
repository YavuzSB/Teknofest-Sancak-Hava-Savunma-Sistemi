# Arduino Geliştiricisi için Proje Notları (Sancak Hava Savunma Sistemi)

Bu notlar, Arduino tarafında UART protokolünü ve genel entegrasyonu geliştirirken dikkat edilmesi gereken önemli noktaları özetler. Lütfen aşağıdaki maddeleri göz önünde bulundur:

---

## 1. CRC-16-CCITT (FALSE) Kullanımı
- **Başlangıç Değeri (Seed):** CRC hesaplamasında hem Raspberry Pi 5 (PacketBuilder) hem de Arduino tarafında başlangıç değeri (seed) olarak 0xFFFF kullanılmalıdır.
- **Polinom:** 0x1021 (standart CCITT-FALSE)
- **RefIn/RefOut:** false (bit sırası çevrilmez)
- **XorOut:** 0x0000
- **CRC Hesaplama Sırası:** CRC, header (H1, H2), type, len ve payload dahil tüm frame üzerinde hesaplanır. CRC alanı dahil edilmez.
- **Byte Sırası:** CRC sonucu iki byte olarak gönderilir: önce yüksek byte, sonra düşük byte (big-endian).

## 2. Frame Formatı
- **Başlangıç Baytları:** Her paket 0x55 (H1) ve 0xAA (H2) ile başlar.
- **Tip (Type):** Mesaj türünü belirtir (ör. Aim, Status, Trigger, SafeLock).
- **Uzunluk (Len):** Payload uzunluğu (byte cinsinden).
- **Payload:** Mesaj tipine göre değişen veri.
- **CRC:** 2 byte, yukarıda açıklanan şekilde hesaplanır.

## 3. Paket Senkronizasyonu ve Hata Toleransı
- **Header Resync:** Alıcı, header (0x55, 0xAA) dışında bir veriyle karşılaşırsa parser'ı başa döndürmelidir.
- **CRC Hatası:** CRC doğrulaması başarısızsa paket atılır ve parser başa döner.
- **Timeout:** Paket ortasında uzun süre veri gelmezse (ör. 100ms) parser resetlenmelidir.

## 4. Mesaj Tipleri ve Payload Yapıları
- **Aim:** float pan_deg, float tilt_deg, float distance_m
- **Status:** uint8_t system_ok, uint16_t error_code
- **Trigger:** uint8_t fire
- **SafeLock:** uint8_t enable
- (Genişletilebilir, ProtocolDef.h dosyasına bakınız)

## 5. Protokol Dosyası Senkronizasyonu
- **ProtocolDef.h:** Arduino, PC ve RasPi tarafında bu dosyanın içeriği birebir aynı olmalıdır. Bir değişiklik yapılırsa tüm platformlarda güncellenmelidir.

## 6. UART Ayarları
- **Baud Rate:** 115200 (varsayılan, config ile değiştirilebilir)
- **8N1:** 8 data bit, no parity, 1 stop bit
- **Akış Kontrolü:** Yok (CRTSCTS devre dışı)

## 7. Kodlama Tarzı ve Güvenlik
- **Non-blocking Okuma:** UART okuması mümkünse non-blocking/parçalı yapılmalı, ana loop'u kilitlememeli.
- **Buffer Taşması:** Payload uzunluğu kontrol edilmeli, buffer overflow'a karşı koruma eklenmeli.
- **RAII ve Temizlik:** Thread/interrupt kullanılıyorsa, kapanışta düzgün şekilde durdurulmalı.

## 8. Test ve Debug
- **Test için Pi 5'ten örnek paket gönderilebilir.**
- **UART hattı osiloskop/logic analyzer ile izlenebilir.**
- **Debug için LED veya seri monitör kullanılabilir.**

---

Daha fazla detay için: ProtocolDef.h dosyasına ve ana projenin README/TEKNIK_DOKUMAN.md dosyalarına bakınız.

> Not: Protokol veya CRC algoritmasında yapılacak değişiklikler, tüm platformlarda aynı şekilde uygulanmalıdır.
