# C++ HID Wiegand Protocol/Pinpad Reader for Arduino
Arduino Keypad and RFID reading software for HID iClass, PivClass, and MultiClass/SE 
model readers. You can create a serial connection to transmit decoded data to an ACM panel or control system.

**Modified to contain scaffolding for keypress recognition.**

If the bits in this software's keypress readers cannot be decoded, log the raw
bits to the console and change them accordingly. **Implement [broadKeypadRecognition.cpp](broadKeypadRecognition.cpp) in the decoding method to do this consistently.**

Tested on PIVClass Reader with whilst reading 8 bits from the key press.

## Setup:
1. Scan and log raw HID data from the reader to the console. Note the keycard data.
2. Replace the default card data with the scanned master card.
3. Replace the default passcode with desired passcode
4. Reload program onto microcontroller.

## Authentication Method (2FA):
1. Scan the master card. If the LED bar above the keypad illuminates steady yellow, the card has been accepted. If the LED returns to red, the card has been declined.
2. Enter passcode on keypad and press # to accept password. (e.x 2580#). Press * to clear password (keycard MUST be reswiped to reenter passcode).
3. Keypad LED will turn GREEN if the keycard and passcode are accepted (valid credential). RED Keypad LED Indicates that the passcode has been rejected.
