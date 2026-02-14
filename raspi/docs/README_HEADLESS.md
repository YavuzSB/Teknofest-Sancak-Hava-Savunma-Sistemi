# TEKNOFEST Balon Tespit Sistemi - Headless Mode Kullanim Kilavuzu (Ozet)

Bu dosya, orijinal README_HEADLESS.md iceriginin kisaltilmis ve ASCII uyumlu bir ozetidir. Tam versiyon eski klasorde saklanmisti.

- Headless mode: Raspberry Pi uzerinde monitorsuz (SSH) calisma modu
- Display mode: Test ve debug icin monitorle calisma

Temel fikirler:
- Headless modda cv2.imshow kullanilmaz, sadece tespit ve konsol ciktilari vardir.
- Display modda goruntu penceresi acilir, `q` veya `ESC` ile cikilir.
- SSH ile bagli iken her zaman headless mod kullanilmalidir.

Calistirma ornekleri (Python tabanli eski sistem icin):

```bash
# Headless
python3 advanced_balloon.py

# Display
python3 advanced_balloon.py --display
```

Bu dosya sadece referans icindir; yeni C++ moduler sistem icin README_MODULAR.md dosyasina bakin.
