/*
 * SANCAK - Teknofest Hava Savunma Sistemi (Kas Sistemi)
 * Görev: Raspberry Pi'den gelen seri komutları uygular.
 * * PROTOKOL:
 * 1. TARGET:X,Y  -> Otonom nişan alma (X,Y hata miktarıdır)
 * 2. MOVE:W      -> Manuel Yukarı
 * 3. MOVE:S      -> Manuel Aşağı
 * 4. MOVE:A      -> Manuel Sol
 * 5. MOVE:D      -> Manuel Sağ
 * 6. FIRE        -> Ateşle (Röle Çek)
 */

#include <Servo.h>

// --- AYARLAR ---
#define PAN_PIN   9   // Yatay Servo (Sağ/Sol)
#define TILT_PIN  10  // Dikey Servo (Yukarı/Aşağı)
#define FIRE_PIN  13  // Ateşleme Rölesi veya Motoru (LED pini test için)

// Servo Sınırları (Mekanik sıkışmayı önlemek için)
#define PAN_MIN   0
#define PAN_MAX   180
#define TILT_MIN  20  // Silah çok aşağı inip kasaya çarpmasın
#define TILT_MAX  160

// Başlangıç Pozisyonu
int panPos = 90;
int tiltPos = 90;

// Hassasiyet (Hata miktarını ne kadar böleceğiz?)
// Düşük sayı = Hızlı tepki (Titreme yapabilir)
// Yüksek sayı = Yavaş ve yumuşak tepki
float kP_Pan = 0.12;
float kP_Tilt = 0.12;

// Manuel Hareket Hızı (Derece)
int manualSpeed = 5;

// Nesneler
Servo panServo;
Servo tiltServo;

// Ateşleme Zamanlayıcısı
unsigned long fireTimer = 0;
bool isFiring = false;
const int fireDuration = 500; // Tetik kaç ms basılı kalsın?

String inputString = "";
bool stringComplete = false;

void setup() {
    Serial.begin(9600);

    panServo.attach(PAN_PIN);
    tiltServo.attach(TILT_PIN);
    pinMode(FIRE_PIN, OUTPUT);
    digitalWrite(FIRE_PIN, LOW); // Ateş kapalı başla

    // Başlangıç pozisyonuna git
    panServo.write(panPos);
    tiltServo.write(tiltPos);

    Serial.println("ARDUINO: HAZIR");
}

void loop() {
    // 1. Seri Porttan Veri Okuma
    if (stringComplete) {
        parseCommand(inputString);
        inputString = "";
        stringComplete = false;
    }

    // 2. Ateşleme Kontrolü (Non-blocking)
    if (isFiring && (millis() - fireTimer > fireDuration)) {
        digitalWrite(FIRE_PIN, LOW); // Tetiği bırak
        isFiring = false;
        Serial.println("INFO: Atis Tamamlandi");
    }
}

// Seri porttan her karakter geldiğinde otomatik çağrılır
void serialEvent() {
    while (Serial.available()) {
        char inChar = (char)Serial.read();
        if (inChar == '\n') {
            stringComplete = true;
        } else {
            inputString += inChar;
        }
    }
}

// Gelen komutu ayrıştır ve uygula
void parseCommand(String data) {
    data.trim(); // Boşlukları temizle

    // --- KOMUT: FIRE ---
    if (data == "FIRE") {
        if (!isFiring) {
            digitalWrite(FIRE_PIN, HIGH); // Tetiği çek
            fireTimer = millis();
            isFiring = true;
            Serial.println("ACTION: FIRE!");
        }
    }

    // --- KOMUT: MOVE (Manuel) ---
    else if (data.startsWith("MOVE:")) {
        char direction = data.charAt(5); // MOVE:W -> 'W' al
        switch (direction) {
            case 'W': tiltPos += manualSpeed; break; // Yukarı
            case 'S': tiltPos -= manualSpeed; break; // Aşağı
            case 'A': panPos += manualSpeed; break;  // Sola (veya sağa, servoya göre değişir)
            case 'D': panPos -= manualSpeed; break;  // Sağa
        }
        applyServos();
    }

    // --- KOMUT: TARGET (Otonom) ---
    // Format: TARGET:hataX,hataY (Örn: TARGET:-50,20)
    else if (data.startsWith("TARGET:")) {
        int commaIndex = data.indexOf(',');
        if (commaIndex > 0) {
            String sX = data.substring(7, commaIndex);
            String sY = data.substring(commaIndex + 1);

            int errorX = sX.toInt();
            int errorY = sY.toInt();

            // Proportional Kontrol (Basit PID'nin P'si)
            // Hata ne kadar büyükse o kadar çok dön
            // Negatif/Pozitif yönleri servonun montajına göre gerekirse ters çevir (-= veya +=)
            panPos  -= errorX * kP_Pan;
            tiltPos -= errorY * kP_Tilt; // Kamera görüntüsünde Y ters olabilir, dene-gör

            applyServos();
        }
    }
}

// Servo sınırlarını kontrol et ve yaz
void applyServos() {
    if (panPos > PAN_MAX) panPos = PAN_MAX;
    if (panPos < PAN_MIN) panPos = PAN_MIN;
    if (tiltPos > TILT_MAX) tiltPos = TILT_MAX;
    if (tiltPos < TILT_MIN) tiltPos = TILT_MIN;

    panServo.write(panPos);
    tiltServo.write(tiltPos);
}
