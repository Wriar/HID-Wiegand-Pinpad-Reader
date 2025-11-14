/*
 * Pinout:
 * D2: Wiegand DATA0 (Interrupt 0)
 * D3: Wiegand DATA1 (Interrupt 1)
 * D5: (Used for 'authenticated' output)
 * D6: (Used for 'authenticated' output)
 * D10: Beeper (Active LOW)
 * D11: Green LED (Active LOW)
 * D12: Red LED (Active LOW)
 */

// --- Constants ---
#define MAX_BITS 100            // Max number of bits to store
#define WEIGAND_WAIT_TIME 3000  // Milliseconds to wait for next Wiegand pulse

// --- Pin Definitions ---
const int PIN_LED_GREEN = 11;
const int PIN_LED_RED = 12;
const int PIN_BEEPER = 10;
const int PIN_DATA0 = 2; // INT0
const int PIN_DATA1 = 3; // INT1
const int PIN_AUTH_OUT_1 = 5;
const int PIN_AUTH_OUT_2 = 6;

// --- Global Variables ---
unsigned char databits[MAX_BITS]; // Stores Wiegand data bits
unsigned char bitCount;           // Number of bits currently captured
unsigned char flagDone;           // Flag: 1 = data ready to be processed
unsigned int weigand_counter;     // Countdown timer for Wiegand timeout

unsigned long facilityCode = 0;   // Decoded facility code
unsigned long cardCode = 0;       // Decoded card code

String keySequence;               // Stores the sequence of keys pressed
String passwordKey = "12345";     // The correct password
bool acceptCard = false;          // True if a valid card is presented, enabling pin entry

/**
 * @brief Interrupt Service Routine for Wiegand DATA0 (0 bit)
 */
void ISR_INT0() {
    // databits[bitCount] is 0 by default (from clearing)
    bitCount++;
    flagDone = 0; // Not done, actively receiving data
    weigand_counter = WEIGAND_WAIT_TIME; // Reset timeout
}

/**
 * @brief Interrupt Service Routine for Wiegand DATA1 (1 bit)
 */
void ISR_INT1() {
    databits[bitCount] = 1;
    bitCount++;
    flagDone = 0; // Not done, actively receiving data
    weigand_counter = WEIGAND_WAIT_TIME; // Reset timeout
}

/**
 * @brief Performs actions for a successful authentication (card + pin)
 */
void processAuthenticated() {
    Serial.println("### User Authenticated ###");
    digitalWrite(PIN_AUTH_OUT_1, HIGH);
    keySequence = ""; // Reset key sequence
    
    // Beep and turn LED Green
    digitalWrite(PIN_BEEPER, LOW);
    digitalWrite(PIN_LED_RED, HIGH); // Red Off
    digitalWrite(PIN_LED_GREEN, LOW); // Green On
    digitalWrite(PIN_AUTH_OUT_2, LOW);

    delay(1000); // Hold success indication

    // Reset to default state
    digitalWrite(PIN_BEEPER, HIGH);
    digitalWrite(PIN_AUTH_OUT_1, LOW);
    digitalWrite(PIN_AUTH_OUT_2, HIGH);
    
    // Set LED back to Red
    digitalWrite(PIN_LED_GREEN, HIGH); // Green Off
    digitalWrite(PIN_LED_RED, LOW);   // Red On

    acceptCard = false; // Require a new card scan
}

/**
 * @brief NEW! Converts a binary string to its decimal integer equivalent.
 * @param inputBits A string containing '0' and '1' characters.
 * @return The decimal integer value, or -1 on error.
 */
int keypadBitToInteger(String inputBits) {
    int decimalValue = 0;
    int length = inputBits.length();

    for (int i = 0; i < length; i++) {
        char bit = inputBits.charAt(i);
        if (bit == '1') {
            decimalValue = decimalValue * 2 + 1;
        } else if (bit == '0') {
            decimalValue = decimalValue * 2;
        } else {
            return -1; // Invalid character in string
        }
    }
    return decimalValue;
}

/**
 * @brief Initial setup for pins, serial, and interrupts
 */
void setup() {
    // --- Pin Modes ---
    pinMode(PIN_LED_RED, OUTPUT);
    pinMode(PIN_LED_GREEN, OUTPUT);
    pinMode(PIN_BEEPER, OUTPUT);
    pinMode(PIN_AUTH_OUT_1, OUTPUT);
    pinMode(PIN_AUTH_OUT_2, OUTPUT);

    pinMode(PIN_DATA0, INPUT); // DATA0 (INT0)
    pinMode(PIN_DATA1, INPUT); // DATA1 (INT1)

    // --- Initial Pin States ---
    // LEDs and Beeper are Active LOW (HIGH = Off)
    digitalWrite(PIN_LED_RED, LOW);    // Default to Red On
    digitalWrite(PIN_LED_GREEN, HIGH); // Green Off
    digitalWrite(PIN_BEEPER, HIGH);    // Beeper Off
    
    digitalWrite(PIN_AUTH_OUT_2, HIGH);

    // --- Serial ---
    Serial.begin(9600);
    Serial.println("RFID Reader Initialized");

    // --- Interrupts ---
    // Attach ISRs to the falling edge of DATA0 and DATA1
    attachInterrupt(digitalPinToInterrupt(PIN_DATA0), ISR_INT0, FALLING);
    attachInterrupt(digitalPinToInterrupt(PIN_DATA1), ISR_INT1, FALLING);

    weigand_counter = WEIGAND_WAIT_TIME;
    flagDone = 1; // Ready for first read
    bitCount = 0;
}

/**
 * @brief Main application loop
 */
void loop() {
    // This waits for the Wiegand pulse train to finish
    if (!flagDone) {
        if (--weigand_counter == 0) {
            flagDone = 1; // Timer expired, data is ready
        }
    }

    // If we have bits and the timeout has expired, process the data
    if (bitCount > 0 && flagDone) {
        unsigned char i;

        Serial.print("Read ");
        Serial.print(bitCount);
        Serial.print(" bits. ");

        // --- 35-bit HID Corporate 1000 format ---
        if (bitCount == 35) {
            // Facility code = bits 2 to 14
            for (i = 2; i < 14; i++) {
                facilityCode <<= 1;
                facilityCode |= databits[i];
            }
            // Card code = bits 15 to 34
            for (i = 14; i < 34; i++) {
                cardCode <<= 1;
                cardCode |= databits[i];
            }
            printBits();
        }
        // --- 26-bit standard format ---
        else if (bitCount == 26) {
            // Facility code = bits 2 to 9
            for (i = 1; i < 9; i++) {
                facilityCode <<= 1;
                facilityCode |= databits[i];
            }
            // Card code = bits 10 to 23
            for (i = 9; i < 25; i++) {
                cardCode <<= 1;
                cardCode |= databits[i];
            }
            printBits();
        }
        // --- 8-bit Keypad format (4-bit data + 4-bit parity) ---
        // Note: This reader seems to output 8 bits for a key.
        else if (bitCount == 8) {
            Serial.println("Detected Key Press");
            String dbits;

            // Get bits. Note: loop starts at 1, skipping first bit (parity?)
            // This loop is (1..8), using 8 bits from databits[1]..databits[8]
            // We assume databits[8] is 0 from cleanup.
            for (i = 1; i < 9; i++) {
                dbits += databits[i];
            }
            Serial.println("Registered 8-bit code: " + dbits);

            // Extract the 4 data bits from the 8-bit code.
            // Based on observed pattern, data is at indices 3, 4, 5, 6.
            String dataBitsOnly = dbits.substring(3, 7);
            Serial.println("Extracted 4-bit data: " + dataBitsOnly);

            // Convert data bits to integer
            int keyValue = keypadBitToInteger(dataBitsOnly);

            switch (keyValue) {
                case 0:
                    Serial.println("Registered Key Press: 0");
                    keySequence += "0";
                    if (keySequence == passwordKey && acceptCard == true) {
                        processAuthenticated();
                    }
                    break;
                case 1:
                    Serial.println("Registered Key Press: 1");
                    keySequence += "1";
                    if (keySequence == passwordKey && acceptCard == true) {
                        processAuthenticated();
                    }
                    break;
                case 2:
                    Serial.println("Registered Key Press: 2");
                    keySequence += "2";
                    if (keySequence == passwordKey && acceptCard == true) {
                        processAuthenticated();
                    }
                    break;
                case 3:
                    Serial.println("Registered Key Press: 3");
                    keySequence += "3";
                    if (keySequence == passwordKey && acceptCard == true) {
                        processAuthenticated();
                    }
                    break;
                case 4:
                    Serial.println("Registered Key Press: 4");
                    keySequence += "4";
                    if (keySequence == passwordKey && acceptCard == true) {
                        processAuthenticated();
                    }
                    break;
                case 5:
                    Serial.println("Registered Key Press: 5");
                    keySequence += "5";
                    if (keySequence == passwordKey && acceptCard == true) {
                        processAuthenticated();
                    }
                    break;
                case 6:
                    Serial.println("Registered Key Press: 6");
                    keySequence += "6";
                    if (keySequence == passwordKey && acceptCard == true) {
                        processAuthenticated();
                    }
                    break;
                case 7:
                    Serial.println("Registered Key Press: 7");
                    keySequence += "7";
                    if (keySequence == passwordKey && acceptCard == true) {
                        processAuthenticated();
                    }
                    break;
                case 8:
                    Serial.println("Registered Key Press: 8");
                    keySequence += "8";
                    if (keySequence == passwordKey && acceptCard == true) {
                        processAuthenticated();
                    }
                    break;
                case 9:
                    Serial.println("Registered Key Press: 9");
                    keySequence += "9";
                    if (keySequence == passwordKey && acceptCard == true) {
                        processAuthenticated();
                    }
                    break;
                case 10: // Asterisk (*)
                    Serial.println("Registered Key Press: * (Reset)");
                    acceptCard = false;
                    keySequence = "";
                    // Set LED back to Red
                    digitalWrite(PIN_LED_GREEN, HIGH); // Green Off
                    digitalWrite(PIN_LED_RED, LOW);   // Red On
                    break;
                case 11: // Pound (#)
                    Serial.println("Registered Key Press: #");
                    keySequence += "#";
                    if (keySequence == passwordKey && acceptCard == true) {
                        processAuthenticated();
                    }
                    break;
                default:
                    Serial.println("Unrecognized key value: " + String(keyValue));
                    break;
            }
        }
        // --- Unknown format ---
        else {
            Serial.println("Unable to decode.");
        }

        // Cleanup and get ready for the next card
        bitCount = 0;
        facilityCode = 0;
        cardCode = 0;
        for (i = 0; i < MAX_BITS; i++) {
            databits[i] = 0;
        }
    } // end if (bitCount > 0 && flagDone)
}

/**
 * @brief Print decoded card info and update LED state
 */
void printBits() {
    Serial.print("FC = ");
    Serial.print(facilityCode);
    Serial.print(", CC = ");
    Serial.println(cardCode);

    // Turn on Green LED (Red is already on), making Yellow/Orange
    digitalWrite(PIN_LED_GREEN, LOW);

    // Check if this is the "master" card that enables pin entry
    if (cardCode == 0) { // 00000 is just 0
        acceptCard = true;
        Serial.println("Card Accepted. Enter PIN.");
        // LED stays Yellow, waiting for pin
    } else {
        acceptCard = false;
        Serial.println("Card not accepted. Resetting.");
        delay(500);
        // Set LED back to Red
        digitalWrite(PIN_LED_GREEN, HIGH); // Green Off
        digitalWrite(PIN_LED_RED, LOW);   // Red On
    }
}
