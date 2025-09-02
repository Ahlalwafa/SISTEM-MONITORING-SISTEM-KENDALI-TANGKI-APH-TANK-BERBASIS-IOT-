// --------------- Konfigurasi Blynk ---------------
#define BLYNK_TEMPLATE_ID "TMPL680NesYd2" // ID Template dari Blynk
#define BLYNK_TEMPLATE_NAME "Monitoring dan Kontrol PH dan Conductivity" // Nama template di Blynk
#define BLYNK_AUTH_TOKEN "5a9FFsvxjXFVKXPQdJVr4oVaj-_S6DAh" // Token autentikasi dari Blynk
#define BLYNK_PRINT Serial // Menampilkan output log dari Blynk ke Serial Monitor

// ----------- ---- Library yang digunakan ---------------
#include <LiquidCrystal_I2C.h> // Library untuk LCD I2C 16x2
#include <WiFi.h>              // Library koneksi WiFi untuk ESP32
#include <WiFiClient.h>        // Library untuk komunikasi TCP/IP
#include <BlynkSimpleEsp32.h>  // Library Blynk untuk ESP32

// --------------- WiFi Credentials ---------------
char ssid[] = "TP-Link_9736"; // SSID jaringan WiFi
char pass[] = "vibrator11";  // Password jaringan WiFi

// --------------- Deklarasi LCD 16x2 I2C ---------------
LiquidCrystal_I2C lcd(0x27, 16, 2); // Inisialisasi LCD I2C di alamat 0x27 ukuran 16x2

// --------------- Timer untuk Blynk ---------------
BlynkTimer timer; // Timer untuk tugas berkala (misalnya kirim data ke Blynk)

// --------------- Pin Sensor ---------------
#define PH_SENSOR_PIN 32 // Pin analog untuk sensor pH
#define ECHO 18          // Pin ECHO sensor ultrasonik
#define TRIG 19          // Pin TRIG sensor ultrasonik

// --------------- Variabel Sensor pH ---------------
unsigned long int avgval; // Menyimpan rata-rata pembacaan sensor pH
int buffer_arr[10], temp; // Array untuk menyimpan pembacaan analog & variabel bantu sorting
float ph_act;             // Nilai pH aktual setelah dikalibrasi

// --------------- Variabel Sensor TDS/EC ---------------
namespace pin {
  const byte tds_sensor = 33; // Pin analog untuk sensor TDS/EC
}
namespace device {
  float aref = 3.3; // Tegangan referensi ADC pada ESP32
}
namespace sensor {
  float ec = 0; // Nilai EC (Electrical Conductivity)
  unsigned int tds = 0; // Nilai TDS (Total Dissolved Solid)
  float ecCalibration = 0.9; // Faktor kalibrasi untuk sensor EC
}

// --------------- Variabel Level Air ---------------
int ketinggian = 0; // Variabel untuk level air dalam persen

// --------------- Relay ---------------
const int relayout1 = 5; // Pin relay 1
const int relayout2 = 4; // Pin relay 2
bool keadaan1 = 0;       // Status logika relay 1
bool keadaan2 = 0;       // Status logika relay 2
int value1 = 0;          // Nilai dari Blynk untuk relay 1
int value2 = 0;          // Nilai dari Blynk untuk relay 2

// --------------- Fungsi Kontrol Relay dari Blynk ---------------
BLYNK_WRITE(V2) {
  value1 = param.asInt(); // Ambil nilai virtual pin V2 dari Blynk
  keadaan1 = value1;      // Simpan ke status relay 1
}
BLYNK_WRITE(V3) {
  value2 = param.asInt(); // Ambil nilai virtual pin V3 dari Blynk
  keadaan2 = value2;      // Simpan ke status relay 2
}

// --------------- Deklarasi Fungsi (Prototype) ---------------
void readPH();               // Fungsi untuk membaca sensor pH
void readTDS();              // Fungsi untuk membaca sensor TDS
void sendToBlynk();          // Fungsi untuk mengirim data ke Blynk
int level();                 // Fungsi untuk membaca ketinggian air
void displaySensorValues(); // Fungsi menampilkan data ke LCD

// --------------- Setup Awal ---------------
void setup() {
  Serial.begin(115200); // Mulai komunikasi serial

  // Inisialisasi pin sebagai output/input
  pinMode(relayout1, OUTPUT); // Relay 1 sebagai output
  pinMode(relayout2, OUTPUT); // Relay 2 sebagai output
  pinMode(TRIG, OUTPUT);      // TRIG ultrasonik sebagai output
  pinMode(ECHO, INPUT);       // ECHO ultrasonik sebagai input
  digitalWrite(relayout1, HIGH); // Nonaktifkan relay 1 di awal
  digitalWrite(relayout2, HIGH); // Nonaktifkan relay 2 di awal

  // Inisialisasi LCD
  lcd.init();    // Atur ukuran LCD
  lcd.backlight();     // Nyalakan backlight LCD
  lcd.clear();         // Bersihkan layar
  lcd.setCursor(0, 0); lcd.print("    TANK APH    "); // Teks awal baris 1
  lcd.setCursor(0, 1); lcd.print("  berbasis IoT  "); // Teks awal baris 2
  delay(2000);         // Tunggu 2 detik

  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("PT PLN NUSANTARA"); // Teks logo perusahaan
  lcd.setCursor(0, 1); lcd.print("     POWER      ");
  delay(2000);         // Tunggu 2 detik

  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("Connecting..."); // Tampilkan status koneksi

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass); // Hubungkan ke Blynk

  timer.setInterval(2000L, sendToBlynk); // Kirim data ke Blynk setiap 2 detik
}

// --------------- Loop Utama ---------------
void loop() {
  Blynk.run();  // Jalankan layanan Blynk
  timer.run();  // Jalankan semua timer

  readPH();             // Baca nilai pH
  readTDS();            // Baca nilai TDS
  displaySensorValues();// Tampilkan data ke LCD
  delay(2000);          // Tunggu 2 detik

  // Logika pengendalian relay berdasarkan nilai sensor
  if (sensor::tds > 0 || ph_act < 6 || ph_act > 9) {
    if (keadaan1 == 1) {
      digitalWrite(relayout2, LOW);  // Hidupkan relay 2
      digitalWrite(relayout1, HIGH); // Matikan relay 1
    } else if (keadaan1 == 0) {
      digitalWrite(relayout2, HIGH); // Matikan relay 2
    }
  } 
  
  else {
    if (keadaan2 == 1) {
      digitalWrite(relayout2, HIGH); // Matikan relay 2
      digitalWrite(relayout1, LOW);  // Hidupkan relay 1
    } else if (keadaan2 == 0) {
      digitalWrite(relayout1, HIGH); // Matikan relay 1
    }
  }

  // Tampilkan status relay ke Serial Monitor
  Serial.print("relay1: ");
  Serial.println(keadaan1);
  Serial.print("relay2: ");
  Serial.println(keadaan2);
}

// --------------- Tampilkan Nilai Sensor ke LCD ---------------
void displaySensorValues() {
  lcd.clear(); // Bersihkan LCD
  lcd.setCursor(0, 0);
  lcd.print("pH: ");
  lcd.print(ph_act, 2); // Tampilkan nilai pH (2 desimal)

  lcd.setCursor(0, 1);
  lcd.print("CONDT:");
  lcd.print(sensor::tds); // Tampilkan TDS
  lcd.print(" uS/cm");

  ketinggian = level(); // Hitung ketinggian air
  lcd.setCursor(7, 0);
  lcd.print(" LVL");
  lcd.print(ketinggian); // Tampilkan level air (%)
  lcd.print("cm");
}

// --------------- Kirim Data ke Blynk ---------------
void sendToBlynk() {
  Blynk.virtualWrite(V4, ketinggian);  // Kirim level air ke virtual pin V4
  Blynk.virtualWrite(V0, ph_act);      // Kirim nilai pH ke virtual pin V0
  Blynk.virtualWrite(V1, sensor::tds); // Kirim nilai TDS ke virtual pin V1
  Blynk.virtualWrite(V2, keadaan1);    // Kirim status relay 1 ke virtual pin V2
  Blynk.virtualWrite(V3, keadaan2);    // Kirim status relay 2 ke virtual pin V3
}

// --------------- Pembacaan Sensor TDS/EC ---------------
void readTDS() {
  float rawEc = analogRead(pin::tds_sensor) * device::aref / 1024.0; // Konversi ke tegangan analog
  float offset = 0.14; // Koreksi offset sensor
  sensor::ec = (rawEc * sensor::ecCalibration) - offset; // Hitung nilai EC

  if (sensor::ec < 0) sensor::ec = 0; // Cegah nilai negatif

  // Rumus konversi EC ke TDS
  sensor::tds = (133.42 * pow(sensor::ec, 3) - 255.86 * sensor::ec * sensor::ec + 857.39 * sensor::ec) * 0.5;

  Serial.print(F("TDS: "));
  Serial.println(sensor::tds);
  Serial.print(F("EC: "));
  Serial.println(sensor::ec, 2);
}

// --------------- Pembacaan Sensor pH ---------------
void readPH() {
  for (int i = 0; i < 10; i++) {
    buffer_arr[i] = analogRead(PH_SENSOR_PIN); // Baca data analog pH
    delay(30); // Delay antar pembacaan
  }

  // Sorting array untuk ambil nilai tengah
  for (int i = 0; i < 9; i++) {
    for (int j = i + 1; j < 10; j++) {
      if (buffer_arr[i] > buffer_arr[j]) {
        temp = buffer_arr[i];
        buffer_arr[i] = buffer_arr[j];
        buffer_arr[j] = temp;
      }
    }
  }

  avgval = 0;
  for (int i = 2; i < 8; i++) {
    avgval += buffer_arr[i]; // Hitung rata-rata 6 nilai tengah
  }
  avgval = avgval / 6.0;

  float volt = (float)avgval * 3.3 / 4095.0; // Konversi ke tegangan

  ph_act = -3.33 * volt + 14.49 + 0.7; // Kalibrasi pH berdasarkan pengujian

  Serial.print("pH Value: ");
  Serial.println(ph_act);
}

// --------------- Pembacaan Sensor Ultrasonik ---------------
int level() {
  digitalWrite(TRIG, LOW);
  delayMicroseconds(5);
  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG, LOW);

  int duration = pulseIn(ECHO, HIGH); // Waktu pantulan
  float distanceCm = duration * 0.034 / 2; // Hitung jarak

  int levelPercent;

  // Hitung level berdasarkan jarak (19 cm = kosong, 6 cm = penuh)
  if (distanceCm >= 19) {
    levelPercent = 0;
  } else if (distanceCm <= 6) {
    levelPercent = 100;
  } else {
    levelPercent = (int)((19.0 - distanceCm) / 13.0 * 100);
  }
return distanceCm;
  //return levelPercent;
}
