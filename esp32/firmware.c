#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <Adafruit_DotStar.h> // Nuova libreria specifica per la tua scheda
#include <cmath>

// --- 1. SCELTA DELLA ZONA ---
const String TIPO_COMPILAZIONE = "CASA";

// --- 2. CONFIGURAZIONE LED CORRETTA (T-Dongle-S3) ---
#define NUM_PIXELS  1
#define PIN_DATA    40  // Il tuo RGB DIN
#define PIN_CLOCK   39  // Il tuo RGB CLK

// Inizializzazione del LED DotStar (l'ordine dei colori tipico di LilyGo è BRG)
Adafruit_DotStar pixels(NUM_PIXELS, PIN_DATA, PIN_CLOCK, DOTSTAR_BGR);
uint32_t coloreAttuale = 0; // Spento di default

// --- 3. CONFIGURAZIONE INDIRIZZI MAC ---
const String macCasaInt = "a4:c1:38:dd:e1:41";
const String macCasaEst = "a4:c1:38:6e:23:49";

const String macTavInt = "a4:c1:38:zz:zz:zz"; // Inserisci il MAC reale della Taverna Interno
const String macTavEst = "a4:c1:38:kk:kk:kk"; // Inserisci il MAC reale della Taverna Esterno

// --- 4. VARIABILI DI MEMORIZZAZIONE ---
float tInt = 0.0, urInt = 0.0;
float tEst = 0.0, urEst = 0.0;
unsigned long lastUpdateInt = 0;
unsigned long lastUpdateEst = 0;

const unsigned long TIMEOUT = 300000; // 5 minuti

void parsePayload(BLEAdvertisedDevice* device) {
    uint8_t* payload = device->getPayload();
    size_t length = device->getPayloadLength();
    String mac = device->getAddress().toString().c_str();

    if (length >= 17 && payload[5] == 0x1A && payload[6] == 0x18) {
        float t = ((uint16_t)(payload[14] | (payload[13] << 8))) / 10.0;
        float h = (float)payload[15];

        Serial.printf("Temperatura %.2f, Umidità %.2f\n", t, h);

        if (TIPO_COMPILAZIONE == "CASA") {
            if (mac.equalsIgnoreCase(macCasaInt)) {
                tInt = t; 
                urInt = h;
                lastUpdateInt = millis();
            } else if (mac.equalsIgnoreCase(macCasaEst)) {
                tEst = t; 
                urEst = h; 
                lastUpdateEst = millis();
            }
        } else if (TIPO_COMPILAZIONE == "TAVERNA") {
            if (mac.equalsIgnoreCase(macTavInt)) {
                tInt = t; 
                urInt = h; 
                lastUpdateInt = millis();
            } else if (mac.equalsIgnoreCase(macTavEst)) {
                tEst = t; 
                urEst = h; 
                lastUpdateEst = millis();
            }
        }
    }
}

class MyCallbacks : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
        String mac = advertisedDevice.getAddress().toString().c_str();

        if(mac.equalsIgnoreCase(macCasaEst) || mac.equalsIgnoreCase(macCasaInt) || mac.equalsIgnoreCase(macTavEst) || mac.equalsIgnoreCase(macTavInt)) {
            parsePayload(&advertisedDevice);
        }
    }
};

float getDewPoint(float t, float h) {
    float a = 17.27, b = 237.7;
    float alpha = ((a * t) / (b + t)) + log(h / 100.0);

    return (b * alpha) / (a - alpha);
}

void setup() {
    Serial.begin(115200);
    Serial.println("--- AVVIO CONTROLLO FINESTRE: " + TIPO_COMPILAZIONE + " ---");

    pixels.begin(); // Inizializza i pin 39 e 40
    pixels.setBrightness(25); // Luminosità moderata
    pixels.clear();
    pixels.show();

    BLEDevice::init("DEW_POINT_CONTROL");
}

void loop() {
    // ---------------------------------------------------------
    // FASE 1: SCANSIONE (LED BLU)
    // ---------------------------------------------------------
    pixels.setPixelColor(0, 0, 0, 255); // Blu (R=0, G=0, B=255)
    pixels.show();

    BLEScan* pScan = BLEDevice::getScan();
    pScan->setAdvertisedDeviceCallbacks(new MyCallbacks(), true);
    pScan->setActiveScan(true);
    pScan->start(20, false);
    
    unsigned long scanStart = millis();

    while (millis() - scanStart < 20000) {
        delay(100);
    }

    pScan->stop();
    pScan->clearResults();

    // ---------------------------------------------------------
    // FASE 2: CALCOLO DEW POINT E SCELTA COLORE
    // ---------------------------------------------------------
    bool datiValidi = (millis() - lastUpdateInt < TIMEOUT) && (millis() - lastUpdateEst < TIMEOUT);
    
    if (datiValidi) {
        float dewInt = getDewPoint(tInt, urInt);
        float dewEst = getDewPoint(tEst, urEst);

        if (!std::isnan(dewInt) && !std::isnan(dewEst) && !std::isinf(dewInt) && !std::isinf(dewEst)) {
            Serial.printf("Dew point interno: %.2f, Dew point esterno: %.2f\n", dewInt, dewEst);
            
            if (dewInt > (dewEst + 2)) {
                // VERDE -> Aprire (R=0, G=255, B=0)
                coloreAttuale = pixels.Color(0, 255, 0); 
            } else {
                // ROSSO -> Chiudere (R=255, G=0, B=0)
                coloreAttuale = pixels.Color(255, 0, 0); 
            }
        } else {
            Serial.printf("DATI NON VALIDI --> Dew point interno: %.2f, Dew point esterno: %.2f\n", dewInt, dewEst);
            coloreAttuale = pixels.Color(255, 100, 0); 
        }
    } else {
        // ARANCIONE/GIALLO -> Sensori non raggiungibili (R=255, G=100, B=0)
        coloreAttuale = pixels.Color(255, 100, 0); 
    }

    // Applica il colore definitivo
    pixels.setPixelColor(0, coloreAttuale);
    pixels.show();

    // ---------------------------------------------------------
    // FASE 3: MANTENIMENTO (LED FISSO) E PAUSA DI 5 MINUTI
    // ---------------------------------------------------------
    unsigned long waitStart = millis();

    while (millis() - waitStart < TIMEOUT) {
        delay(1000); 
    }
}