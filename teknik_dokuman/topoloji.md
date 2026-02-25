## 4) Çalışma Topolojisi (Gerçek Akış)

### 4.1 Temel topoloji
- Raspi:
  - Kamera → CombatPipeline → (opsiyonel) UDP video → PC
  - Kamera → CombatPipeline → (opsiyonel) TCP telemetri → PC
  - TurretController → seri port → mikrodenetleyici

- PC:
  - UDP port dinler → görüntü paneli
  - TCP port bağlanır → telemetri + overlay

### 4.2 Telemetri “server-only” ve “push” ayrımı
Raspi tarafında iki telemetri yaklaşımı var:
- TcpTelemetryServer: PC bağlanır ve Raspi server olarak frame’leri gönderir.
- TelemetrySender: Raspi client olarak PC’ye push atar (config ile kapatılabilir).

Kodda port çakışması riskini azaltmak için telemetry_push_port ayrı bir port olarak tasarlanmıştır.

---
