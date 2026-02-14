# Raspberry Pi Podman/Docker Kullanımı


## 1. Podman imajını oluştur


```bash
podman build -t sancak-raspi .
```


## 2. Container başlat

```bash
podman run --rm -it sancak-raspi
```

## 3. Kamera/USB erişimi (gerçek Pi'de)
- Donanım erişimi için ek parametreler gerekebilir (örn. `--device /dev/video0`)


## Podman Notu
- Podman, Docker ile büyük oranda uyumludur. Tüm komutlar için sadece `docker` yerine `podman` yazmanız yeterlidir.
- Eğer sisteminizde sadece podman varsa, `podman-docker` paketi ile `docker` komutlarını da podman'a yönlendirebilirsiniz.

## Notlar
- Bu imaj ARM64 içindir, PC'de QEMU ile test edilebilir ama yavaş olur.
- Kodlar otomatik build edilir, `main_detector` başlatılır.
