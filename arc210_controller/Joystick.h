/*
    Joystick.h

    Copyright (c) 2022, Benjamin Aigner <beni@asterics-foundation.org>

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

#pragma once

#ifdef USE_TINYUSB
#error Joystick is not compatible with Adafruit TinyUSB
#endif

#include "arc210_64_button_report.h"
#include <HID_Joystick.h>


//======================================================================
class Joystick_ : public HID_Joystick {
protected:
    arc210_64_button_report_t data;    
public:
    Joystick_();
    void begin() override;
    void end() override;
    void button(uint8_t button, bool val);
    virtual void send_now() override;
private:
    uint8_t _id;
};
extern Joystick_ Joystick;
