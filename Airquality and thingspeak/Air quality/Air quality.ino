#include <DHT.h>
#include <MQ135.h>

// Sensor Pins
#define MQ135_PIN A0      // Analog pin for MQ135 sensor
#define DUST_LED 3        // Digital pin for dust sensor LED
#define DUST_AOUT A1      // Analog pin for dust sensor output
#define DHTPIN 2          // Digital pin for DHT22 sensor
#define DHTTYPE DHT22     // Define DHT sensor type

// Calibration Constants
#define RZERO_CLEAN_AIR 82.15    // Calibrated R0 value in clean outdoor air
#define CO2_BASE 410.0           // Atmospheric CO2 baseline in ppm
#define DUST_CALIB_FACTOR 1.12   // Calibration factor for dust density

// NH3 coefficients for home environments (adjusted)
#define NH3_A -0.431             // Adjusted slope for NH3 calculation
#define NH3_B 0.0                // Adjusted intercept for NH3 calculation

// Initialize sensor objects
DHT dht(DHTPIN, DHTTYPE);    // DHT22 sensor
MQ135 mq135(MQ135_PIN);      // MQ135 gas sensor

void setup() {
  Serial.begin(9600);        // For debugging on Serial Monitor
  Serial1.begin(9600);       // Communication with ESP8266 on RX1/TX1
  dht.begin();               // Initialize DHT sensor
  pinMode(DUST_LED, OUTPUT); // Set dust sensor LED pin as output
  
  // Print initial calibration check
  Serial.print("Calibration Ratio: ");
  Serial.println(mq135.getCorrectedRZero(25, 50) / RZERO_CLEAN_AIR);
}

void loop() {
  delay(15000); // Wait 2 seconds between readings (adjust to 15s for ThingSpeak later if needed)

  // Read temperature and humidity from DHT22
  float temp = dht.readTemperature();
  float hum = dht.readHumidity();
  
  // Check if DHT readings are valid
  if (isnan(hum) || isnan(temp)) {
    Serial.println("Invalid DHT readings");
    return; // Skip this loop if readings are invalid
  }

  // Get current resistance (Rs) from MQ135 sensor
  float rs = mq135.getResistance();
  
  // Get corrected R0 based on current temperature and humidity
  float r0_corrected = mq135.getCorrectedRZero(temp, hum);
  
  // Calculate the Rs/R0 ratio
  float ratio = rs / r0_corrected;
  
  // Calculate CO₂ concentration in ppm
  float co2_ppm = CO2_BASE * pow(ratio, -2.15);
  
  // Calculate NH₃ concentration in ppm with a floor limit
  float nh3_ppm = constrain(pow(10, (log10(ratio) - NH3_B) / NH3_A), 0, 20);

  // Measure dust density using the dust sensor
  digitalWrite(DUST_LED, LOW);     // Turn on dust sensor LED
  delayMicroseconds(280);          // Wait for stable reading
  int voRaw = analogRead(DUST_AOUT); // Read analog output
  delayMicroseconds(40);           // Wait before turning off LED
  digitalWrite(DUST_LED, HIGH);    // Turn off dust sensor LED
  delayMicroseconds(9680);         // Complete the 10ms cycle
  
  // Convert raw reading to voltage and calculate dust density
  float voVoltage = voRaw * (5.0 / 1024.0);
  float dustDensity = constrain((voVoltage - 0.59) * 185.0, 0, 500) * DUST_CALIB_FACTOR;

  // Prepare the data string
  String data = String(temp, 1) + "," + String(hum, 1) + "," + String(co2_ppm) + "," + String(nh3_ppm, 1) + "," + String(dustDensity, 1);
  
  // Send data to ESP8266 via Serial1
  Serial1.println(data);
  
  // Debug: Print the data being sent to Serial Monitor
  Serial.print("Sending to ESP8266: ");
  Serial.println(data);
}