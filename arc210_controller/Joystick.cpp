/*
    Joystick.cpp

    Copyright (c) 2022, Benjamin Aigner <beni@asterics-foundation.org>
    Implementation loosely based on:
    Mouse library from https://github.com/earlephilhower/arduino-pico
    Joystick functions from Teensyduino https://github.com/PaulStoffregen

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "Joystick.h"
#include "Arduino.h"
#include <USB.h>

#include "tusb.h"
#include <tusb-hid.h>
#include "class/hid/hid_device.h"
#include "arc210_64_button.h"

static const uint8_t desc_hid_report_joystick[] = { ARC_210_64_BUTTON(HID_REPORT_ID(1)) };

Joystick_::Joystick_(void) {
}

void Joystick_::begin() {
    if (_running) {
        return;
    }
    USB.disconnect();
    _id = USB.registerHIDDevice(desc_hid_report_joystick, sizeof(desc_hid_report_joystick), 30, 0x0004);
    USB.connect();
    HID_Joystick::begin();
}

void Joystick_::end() {
    if (_running) {
        USB.disconnect();
        USB.unregisterHIDDevice(_id);
        USB.connect();
    }
    HID_Joystick::end();
}

void Joystick_::button(uint8_t button, bool val) {
    if (!_running) {
        return;
    }
    //I've no idea why, but without a second dword, it is not possible.
    //maybe something with the alignment when using bit set/clear?!?
    static uint32_t buttons_local = 0;

    if (button >= 1 && button <= 32) {
        if (val) {
            buttons_local |= (1ULL << (button - 1));
        } else {
            buttons_local &= ~(1ULL << (button - 1));
        }
        Serial.println(buttons_local, BIN);

        data.buttons = buttons_local;
        if (_autosend) {
            send_now();
        }
    }
    if (button >= 33 && button <= 64) {
        if (val) {
            buttons_local |= (1ULL << (button - 33));
        } else {
            buttons_local &= ~(1ULL << (button - 33));
        }
        Serial.println(buttons_local, BIN);

        data.buttons2 = buttons_local;
        if (_autosend) {
            send_now();
        }
    }
}


// immediately send an HID report
void Joystick_::send_now(void) {
    if (!_running) {
        return;
    }
    CoreMutex m(&USB.mutex);
    tud_task();
    if (USB.HIDReady()) {
        tud_hid_n_report(0, USB.findHIDReportID(_id), &data, sizeof(data));
    }
    tud_task();
}

Joystick_ Joystick;
