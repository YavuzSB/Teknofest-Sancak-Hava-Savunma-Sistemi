# Balon Tespiti (OpenCV, Python)

Tek renkli bir duvar onundeki farkli renk balonlari tespit eder.
- Kirmizi: kirmizi kutu ve FIRE etiketi (vurulacak hedef)
- Mavi ve sari: yesil kutu ve FRIEND etiketi (vurulmayacak)

Bu dokuman eski Python tabanli balon_tespit aracini aciklar.

Orijinal icerik:

---

````markdown
# Balon Tespiti (OpenCV)

Tek renkli bir duvar onundeki farkli renk balonlari tespit eder.
- Kirmizi: Kirmizi kutu ve FIRE etiketi (vurulacak hedef)
- Mavi & Sari: Yesil kutu ve FRIEND etiketi (vurulmayacak)

Gercek zamanli kamera veya video dosyasindan calisir.

## Kurulum

Asagidaki bagimliliklari yukleyin:

```bash
pip install -r requirements.txt
```

## Calistirma

Kameradan calistirmak icin:

```bash
python detect_balloons.py --source 0
```

...
````
