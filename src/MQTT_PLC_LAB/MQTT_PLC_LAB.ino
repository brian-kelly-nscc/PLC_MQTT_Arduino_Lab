#include <SPI.h>
#include <Ethernet.h>
#include <PubSubClient.h>
#include <string.h>

// -------- Board Configuration --------
// Student Configurable Parameters:
//   - BOARD_NAME: Unique name for this board (used as the MQTT client ID and for broadcasting on boot).
//   - ip: Static IP address for the board.
//         NOTE: For the PLC network, addresses between 192.168.60.170 and 192.168.60.199 are available.
//   - subnet: Subnet mask for your network.
//   - PLC_IP: IP address of the target PLC to control the lamp tags.
// 
// IMPORTANT: Each board on the same network must have a unique MAC address.
// Change the 'mac' value below if using multiple boards.

#define BOARD_NAME "PLC-Board-01"
#define PLC_IP "192.168.1.99" // Target PLC IP address for this board

// Static network configuration
IPAddress ip(192, 168, 1, 177);        // Change as needed for your network
IPAddress subnet(255, 255, 255, 0);    // Network's subnet mask
IPAddress dns(192, 168, 1, 1);
IPAddress gateway(192, 168, 1, 1);

// MQTT Broker IP address (update this to match your VM's broker)
const char* mqtt_server = "192.168.1.131";

// MAC address for the Ethernet shield
// IMPORTANT: Change this if you have multiple boards on the same network.
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

// ----- MQTT & Ethernet Clients -----
EthernetClient ethClient;
PubSubClient client(ethClient);

// ----- I/O Definitions -----
// DIP switches: 8 DIP switches wired to digital pins 2-9 (configured with INPUT_PULLUP)
const int dipPins[8] = {2, 3, 4, 5, 6, 7, 8, 9};

// Trigger button: a separate button wired with a pullup resistor on analog pin A0
const int triggerPin = A0;

// Optional LED for visual feedback (if desired; here using analog pin A1 as digital output)
const int ledPin = A1;
const int flashDuration = 100;

void setup() {
  Serial.begin(9600);
  while (!Serial) {
    ; // Wait for the serial port to connect.
  }
  
  // Configure LED (optional)
  pinMode(ledPin, OUTPUT);
  
  // Configure DIP switch pins as INPUT_PULLUP
  for (int i = 0; i < 8; i++) {
    pinMode(dipPins[i], INPUT_PULLUP);
  }
  
  // Configure trigger button pin (using analog pin A0 as digital input) with pullup
  pinMode(triggerPin, INPUT_PULLUP);

  // Initialize Ethernet with full configuration
  Ethernet.begin(mac, ip, dns, gateway, subnet);
  delay(1000); // Allow time for the Ethernet hardware to initialize

  // Set the MQTT broker server and port (1883 is standard for unencrypted MQTT)
  client.setServer(mqtt_server, 1883);

  Serial.print("Arduino IP address: ");
  Serial.println(Ethernet.localIP());
}

void reconnect() {
  // Loop until reconnected to the MQTT broker
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Use BOARD_NAME as the MQTT client ID for uniqueness
    if (client.connect(BOARD_NAME)) {
      Serial.println(" connected");
      // Publish board name on boot to notify the broker of this board's presence
      client.publish("board/boot", BOARD_NAME);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" - trying again in 5 seconds");
      delay(5000);
    }
  }
}

void loop() {
  // Ensure we remain connected to the MQTT broker
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Check if the trigger button (on triggerPin) is pressed.
  // Because we are using INPUT_PULLUP, a pressed button will read LOW.
  if (digitalRead(triggerPin) == LOW) {
    // Debounce delay
    delay(50);
    if (digitalRead(triggerPin) == LOW) {
      // Read the state of each DIP switch (true if closed/activated)
      bool lampStates[8];
      for (int i = 0; i < 8; i++) {
        // With pullup, a closed switch will read LOW (thus "true")
        lampStates[i] = (digitalRead(dipPins[i]) == LOW);
      }
      
      // Construct JSON payload
      // Example:
      // {"BOARD_NAME":"PLC-Board-01","PLC_IP":"192.168.60.170","LAMP_STATES":{"Lamp1":true,"Lamp2":false,...}}
      char payload[256];
      snprintf(payload, sizeof(payload),
               "{\"BOARD_NAME\":\"%s\",\"PLC_IP\":\"%s\",\"LAMP_STATES\":{",
               BOARD_NAME, PLC_IP);
      
      // Append each lamp state to the JSON string
      for (int i = 0; i < 8; i++) {
        char lampEntry[32];
        snprintf(lampEntry, sizeof(lampEntry), "\"Lamp%d\":%s", i+1, (lampStates[i] ? "true" : "false"));
        strcat(payload, lampEntry);
        if (i < 7) {
          strcat(payload, ",");
        }
      }
      strcat(payload, "}}");

      // Publish the JSON payload to the MQTT topic "plc/lamp_control"
      if (client.publish("plc/lamp_control", payload)) {
        Serial.print("Published payload: ");
        Serial.println(payload);
      } else {
        Serial.println("MQTT publish failed.");
      }
      
      // Debounce: wait 500 ms before checking again.
      delay(500);
    }
  }
}