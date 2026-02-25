## 5) Network Protokolleri (SNK1/SNK2)

> Bu protokoller raspi/include/network/protocol.hpp içinde tanımlanır ve PC tarafında PC/src/NetworkProtocol.hpp üzerinden aynı magic/struct mantığıyla parse edilir.

### 5.1 UDP Video: SNK1 (Fragment’lı JPEG)
Amaç: UDP’nin paket kaybı olabilir doğasına uyacak şekilde, büyük JPEG’i MTU’ya bölüp yollamak.

- Magic: SNK1
- Header: UdpJpegFragmentHeader (packed, 36 byte)
- Her frame için:
  1. BGR frame → cv::imencode(".jpg")
  2. JPEG byte dizisi MTU’ya göre chunk’lara bölünür.
  3. Her chunk bir UDP datagram olarak gönderilir.

Önemli alanlar:
- frame_id: reassembly anahtarı
- timestamp_us: Raspi tarafında system_clock epoch microseconds (PC’de latency hesabı için)
- chunk_index/chunk_count
- jpeg_bytes, chunk_offset, chunk_bytes

PC tarafında reassembly:
- frame_id için jpeg buffer allocate edilir.
- Her chunk geldikçe offset’ine kopyalanır, “got” bitmap’i işaretlenir.
- Tüm chunk’lar gelince JPEG complete olur ve decode edilir.
- 500ms’den eski incomplete frame’ler temizlenir (GC).

Neden UDP + fragment?
- Düşük gecikme (TCP head-of-line blocking yok)
- Kayıp paket olduğunda eski frame’leri beklemek yerine yeni frame’e geçmek mümkün

Alternatifler:
- RTP/RTSP (daha standart ama daha ağır altyapı)
- WebRTC (NAT/ICE vb. karmaşıklık)
- TCP üzerinden MJPEG (kolay ama gecikme/kuyruk büyümesi riski)

### 5.2 TCP Telemetri: SNK2 (Binary frame)

Magic: SNK2
Header: TcpMsgHeader (8 byte)
Mesaj tipi:
- kAimResultV1 (type=1)

Raspi TcpTelemetryServer’ın gönderdiği AimResultV1 payload (28 byte):
- u32 frame_id
- f32 raw_x, raw_y
- f32 corrected_x, corrected_y
- i32 class_id
- u8 valid
- u8 reserved[3]

PC TelemetryClient parse eder:
- Magic resync: magic mismatch → buffer’dan 1 byte sil, tekrar dene
- Version mismatch → aynı resync

Neden TCP?
- Telemetri küçük boyutlu; tam ve sıralı gelmesi tercih edilebilir.
- Basit: tek bağlantı, tek stream.

Alternatifler:
- UDP telemetri (paket kaybına toleranslı tasarım gerekir)
- Flatbuffers/Protobuf (şema evrimi için iyi; ek bağımlılık)

---
