
// WEB SERVER MODELİ
#include <SPI.h>
#include <Ethernet.h>
#include <SoftwareSerial.h> 

// --- Ethernet Ayarları ---
byte mac[] = { 0, 10, 10, 10, 10, 10 };
IPAddress ip(10, 65, 10, 70); 
EthernetServer server(80);

// --- MAX485 & Modbus Pin Ayarları ---
#define RE_DE_PIN 7         
SoftwareSerial modbus(6, 8); // RO -> D6 (RX), DI -> D8 (TX)

// Modbus Okuma Sorgu Paketi (Adres 1, 5 adet register iste)
const byte sensorSorgu[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x05, 0x85, 0xC9}; 

// Kılavuzdaki Orijinal Kalibrasyon Paketleri
const byte ph401Calib[]  = {0x01, 0x06, 0x00, 0x6A, 0x00, 0x01, 0x68, 0x16}; 
const byte ph686Calib[]  = {0x01, 0x06, 0x00, 0x6A, 0x00, 0x02, 0x28, 0x17}; 
const byte ec1413Calib[] = {0x01, 0x06, 0x00, 0x7F, 0x00, 0x01, 0x79, 0xD2}; 
const byte hafizayaKaydetKomutu[] = {0x01, 0x06, 0x00, 0x33, 0x00, 0x01, 0xB8, 0x04}; 

byte responseBuffer[25]; 

// Küresel sensör değişkenleri
float phDegeri = 0.0;
float iletkenlikDegeri = 0.0; 
float sicaklikDegeri = 0.0;
float tdsDegeri = 0.0;
float tuzlulukDegeri = 0.0;
String kalibrasyonDurumu = ""; 

void setup() {
  pinMode(RE_DE_PIN, OUTPUT);
  digitalWrite(RE_DE_PIN, LOW); 
  
  Serial.begin(9600); 
  modbus.begin(9600); 
  
  Ethernet.begin(mac, ip);
  server.begin();
}

void loop() {
  sensorOku();

  EthernetClient client = server.available();
  if (client) {
    String HTTP_req = ""; 
    boolean currentLineIsBlank = true;
    
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        if (HTTP_req.length() < 100) { HTTP_req += c; }
        
        if (c == '\n' && currentLineIsBlank) {
          
          // --- BUTON KONTROLLERİ ---
          if (HTTP_req.indexOf("GET /?calib=ph401") >= 0) {
            kalibrasyonUygula(ph401Calib, sizeof(ph401Calib));
            kalibrasyonDurumu = "pH 4.01 Gonderildi!";
          }
          else if (HTTP_req.indexOf("GET /?calib=ph686") >= 0) {
            kalibrasyonUygula(ph686Calib, sizeof(ph686Calib));
            kalibrasyonDurumu = "pH 7.01 (6.86) Gonderildi!";
          }
          else if (HTTP_req.indexOf("GET /?calib=ec1413") >= 0) {
            kalibrasyonUygula(ec1413Calib, sizeof(ec1413Calib));
            kalibrasyonDurumu = "EC 1413 Gonderildi!";
          }

          // HTTP Yanıt Başlığı
          client.println(F("HTTP/1.1 200 OK"));
          client.println(F("Content-Type: text/html; charset=utf-8")); 
          client.println(F("Connection: close"));
          if(kalibrasyonDurumu == "") { client.println(F("Refresh: 5")); }
          client.println();
          
          // Bellek Optimizasyonlu HTML (F makrosu ile RAM tüketimi sıfırlandı)
          client.println(F("<!DOCTYPE HTML><html><head><title>Su Kalite Paneli</title>"));
          client.println(F("<style>body{font-family:Arial; margin:30px; background:#f4f7f6; color:#333;} h1,h2{color:#2c3e50;}"));
          client.println(F("table{border-collapse:collapse; width:100%; max-width:600px; background:white; margin-bottom:20px;}"));
          client.println(F("th,td{padding:10px; border-bottom:1px solid #ddd;} th{background:#2c3e50; color:white;}"));
          client.println(F(".btn{background:#e74c3c; color:white; padding:8px 12px; text-decoration:none; border-radius:4px; display:inline-block; margin-right:5px; font-weight:bold;}"));
          client.println(F(".btn-7{background:#2ecc71;} .btn-ec{background:#3498db;} .alr{background:#f8d7da; color:#721c24; padding:8px; border-radius:4px; font-weight:bold;}</style></head><body>"));
          
          client.print(F("<h1>🌊 Su Kalite Otomasyonu</h1><hr><br>"));
          
          if (kalibrasyonDurumu != "") {
            client.print(F("<p style='color:blue;'>📊 ")); client.print(kalibrasyonDurumu); client.println(F(" <a href='/'>[Yenile]</a></p>"));
            kalibrasyonDurumu = ""; 
          }
          
          // Alarm Kontrolü
          if (phDegeri < 6.50 || phDegeri > 8.50 || iletkenlikDegeri > 2000) {
            client.println(F("<div class='alr'>🚨 TEHLİKE: Limit Aşımı Var!</div><br>"));
          }

          // Canlı Değerler Tablosu
          client.println(F("<table><tr><th>Parametre</th><th>Deger</th></tr>"));
          client.print(F("<tr><td>pH Derecesi</td><td><b>")); client.print(phDegeri, 2); client.println(F(" pH</b></td></tr>"));
          client.print(F("<tr><td>Iletkenlik (EC)</td><td><b>")); client.print(iletkenlikDegeri, 0); client.println(F(" uS/cm</b></td></tr>"));
          client.print(F("<tr><td>Su Sicakligi</td><td><b>")); client.print(sicaklikDegeri, 1); client.println(F(" &deg;C</b></td></tr>"));
          client.print(F("<tr><td>TDS</td><td><b>")); client.print(tdsDegeri, 0); client.println(F(" ppm</b></td></tr>"));
          client.print(F("<tr><td>Tuzluluk</td><td><b>")); client.print(tuzlulukDegeri, 2); client.println(F(" ppt</b></td></tr>"));
          client.println(F("</table><br>"));
          
          // Kalibrasyon Butonları (Kesinlikle görünecek şekilde optimize edildi)
          client.println(F("<h2>Kalibrasyon Yonetimi</h2>"));
          client.println(F("<a href='/?calib=ph401' class='btn'>pH 4.01</a>"));
          client.println(F("<a href='/?calib=ph686' class='btn btn-7'>pH 7.01</a>"));
          client.println(F("<a href='/?calib=ec1413' class='btn btn-ec'>EC 1413</a>"));

          client.println(F("</body></html>"));
          break;
        }
        if (c == '\n') currentLineIsBlank = true;
        else if (c != '\r') currentLineIsBlank = false;
      }
    }
    delay(1); 
    client.stop(); 
  }
}

void sensorOku() {
  while(modbus.available() > 0) { modbus.read(); } 

  digitalWrite(RE_DE_PIN, HIGH);
  delay(10);
  modbus.write(sensorSorgu, sizeof(sensorSorgu));
  modbus.flush(); 
  
  digitalWrite(RE_DE_PIN, LOW);
  
  long baslangicZamani = millis();
  int index = 0;
  
  while ((millis() - baslangicZamani < 250) && (index < 25)) {
    if (modbus.available()) {
      responseBuffer[index++] = modbus.read();
    }
  }

  if (index >= 13 && responseBuffer[0] == 0x01 && responseBuffer[1] == 0x03) {
    int rawPH        = (responseBuffer[3] << 8) | responseBuffer[4];   
    int rawEC        = (responseBuffer[5] << 8) | responseBuffer[6];   
    int rawSicaklik  = (responseBuffer[7] << 8) | responseBuffer[8];   
    int rawTDS       = (responseBuffer[9] << 8) | responseBuffer[10];  
    int rawTuzluluk  = (responseBuffer[11] << 8) | responseBuffer[12]; 
    
    if (rawSicaklik > 32767) rawSicaklik = rawSicaklik - 65536;

    phDegeri         = rawPH / 100.0;      
    iletkenlikDegeri = rawEC;              
    sicaklikDegeri   = rawSicaklik / 10.0; 
    tdsDegeri        = rawTDS;             
    tuzlulukDegeri   = rawTuzluluk / 100.0; 
  }
}

void kalibrasyonUygula(const byte* paket, int boyut) {
  while(modbus.available() > 0) { modbus.read(); } 
  digitalWrite(RE_DE_PIN, HIGH); 
  delay(10);
  modbus.write(paket, boyut);
  modbus.flush();
  digitalWrite(RE_DE_PIN, LOW); 
  delay(600); 

  while(modbus.available() > 0) { modbus.read(); } 
  digitalWrite(RE_DE_PIN, HIGH); 
  delay(10);
  modbus.write(hafizayaKaydetKomutu, sizeof(hafizayaKaydetKomutu));
  modbus.flush();
  digitalWrite(RE_DE_PIN, LOW); 
  delay(1200); 
}
