/*
  ARC-210 DCS controller - Raspberry Pi Pico / Arduino-Pico

  Arduino IDE setup:
    Board: Raspberry Pi Pico
    Tools -> USB Stack: Pico SDK
    Library Manager: install "Adafruit MCP23X17"

  HID layout:
    Buttons  1-14: physical push buttons through the MCP23017
    Buttons 15-28: encoder directions, two buttons per encoder
    Buttons 29-32: selector 1 and 2 next/previous pulses

  Rotary switch positions are absolute in hardware. The HID interface emits
  relative next/previous pulses when a selector position changes, which is
  convenient for DCS bindings. The sim must be synchronized manually after
  boot because an absolute hardware switch cannot know the sim's start state.
*/

#include <Wire.h>
#include "Joystick.h"
#include <Adafruit_MCP23X17.h>

// Arduino-Pico requests a 1ms USB HID polling interval instead of its 10ms default.
int usb_hid_poll_interval = 1;

constexpr bool DEBUG_SERIAL = false;

// Pico wiring allocation.
constexpr uint8_t I2C_SDA_PIN = 4;
constexpr uint8_t I2C_SCL_PIN = 5;
constexpr uint8_t SELECTOR_1_ADC_PIN = 26;
constexpr uint8_t SELECTOR_2_ADC_PIN = 27;

constexpr uint8_t MCP23017_ADDRESS = 0x27;
constexpr uint8_t MCP_BUTTON_COUNT = 16;
constexpr uint32_t MCP_POLL_INTERVAL_MS = 50;
constexpr uint32_t BUTTON_DEBOUNCE_MS = 25;
constexpr uint32_t SELECTOR_DEBOUNCE_MS = 30;
constexpr uint32_t HID_PRESS_MS = 20;
constexpr uint32_t HID_RELEASE_GAP_MS = 10;
constexpr int8_t ENCODER_EDGES_PER_DETENT = 4;


enum HidButton : uint8_t {
  HID_PUSH_BUTTON_1 = 1,
  HID_ENCODER_1_UP = 17,
  HID_ENCODER_1_DOWN = 18,
  HID_ENCODER_2_UP = 19,
  HID_ENCODER_2_DOWN = 20,
  HID_ENCODER_3_UP = 21,
  HID_ENCODER_3_DOWN = 22,
  HID_ENCODER_4_UP = 23,
  HID_ENCODER_4_DOWN = 24,
  HID_ENCODER_5_UP = 25,
  HID_ENCODER_5_DOWN = 26,
  HID_ENCODER_6_UP = 27,
  HID_ENCODER_6_DOWN = 28,
  HID_ENCODER_7_UP = 29,
  HID_ENCODER_7_DOWN = 30,
  HID_SELECTOR_1_OFF = 33,
  HID_SELECTOR_1_TR_G = 34,
  HID_SELECTOR_1_TR = 35,
  HID_SELECTOR_1_ADF = 36,
  HID_SELECTOR_1_CHG_PRST = 37,
  HID_SELECTOR_1_TEST = 38,
  HID_SELECTOR_1_ZERO = 39,
  HID_SELECTOR_2_ECCM_MASTER = 40,
  HID_SELECTOR_2_ECCM = 41,
  HID_SELECTOR_2_PRST = 42,
  HID_SELECTOR_2_MAN = 43,
  HID_SELECTOR_2_MAR = 44,
  HID_SELECTOR_2_243 = 45,
  HID_SELECTOR_2_I2I = 46
};

struct DebouncedInput {
  bool stable;
  bool candidate;
  uint32_t candidateSince;
};

struct EncoderInput {
  uint8_t pinA;
  uint8_t pinB;
  uint8_t positiveButton;
  uint8_t negativeButton;
  uint8_t lastState;
  int8_t edgeAccumulator;
};

struct SelectorInput {
  uint8_t adcPin;
  uint8_t stablePosition;
  uint8_t candidatePosition;
  uint32_t candidateSince;
  uint8_t* buttons;
};

struct PulseButton {
  uint8_t hidButton;
  uint8_t pendingCount;
  bool pressed;
  bool releaseGap;
  uint32_t changedAt;
};

Adafruit_MCP23X17 mcp;
DebouncedInput mcpButtons[MCP_BUTTON_COUNT];

EncoderInput encoders[] = {
  {0, 1, HID_ENCODER_1_UP, HID_ENCODER_1_DOWN, 0, 0},
  {2, 3, HID_ENCODER_2_UP, HID_ENCODER_2_DOWN, 0, 0},
  {6, 7, HID_ENCODER_3_UP, HID_ENCODER_3_DOWN, 0, 0},
  {8, 9, HID_ENCODER_4_UP, HID_ENCODER_4_DOWN, 0, 0},
  {10, 11, HID_ENCODER_5_UP, HID_ENCODER_5_DOWN, 0, 0},
  {12, 13, HID_ENCODER_6_UP, HID_ENCODER_6_DOWN, 0, 0},
  {14, 15, HID_ENCODER_7_UP, HID_ENCODER_7_DOWN, 0, 0},
};
constexpr size_t ENCODER_COUNT = sizeof(encoders) / sizeof(encoders[0]);

uint8_t operationModeSelector[8] = {
  0,
  HID_SELECTOR_1_ZERO,
  HID_SELECTOR_1_TEST,
  HID_SELECTOR_1_CHG_PRST,
  HID_SELECTOR_1_ADF,
  HID_SELECTOR_1_TR,
  HID_SELECTOR_1_TR_G ,
  HID_SELECTOR_1_OFF
};

uint8_t frequencyModeSelector[8] = {
  0,
  HID_SELECTOR_2_I2I,
  HID_SELECTOR_2_243,
  HID_SELECTOR_2_MAR,
  HID_SELECTOR_2_MAN,
  HID_SELECTOR_2_PRST,
  HID_SELECTOR_2_ECCM,
  HID_SELECTOR_2_ECCM_MASTER
};

SelectorInput selectors[] = {
  {SELECTOR_1_ADC_PIN, 0,0,0, frequencyModeSelector},
  {SELECTOR_2_ADC_PIN, 0,0,0, operationModeSelector},
};

constexpr size_t SELECTOR_COUNT = sizeof(selectors) / sizeof(selectors[0]);

PulseButton pulseButtons[] = {
  {HID_ENCODER_1_UP, 0, false, false, 0},
  {HID_ENCODER_1_DOWN, 0, false, false, 0},
  {HID_ENCODER_2_UP, 0, false, false, 0},
  {HID_ENCODER_2_DOWN, 0, false, false, 0},
  {HID_ENCODER_3_UP, 0, false, false, 0},
  {HID_ENCODER_3_DOWN, 0, false, false, 0},
  {HID_ENCODER_4_UP, 0, false, false, 0},
  {HID_ENCODER_4_DOWN, 0, false, false, 0},
  {HID_ENCODER_5_UP, 0, false, false, 0},
  {HID_ENCODER_5_DOWN, 0, false, false, 0},
  {HID_ENCODER_6_UP, 0, false, false, 0},
  {HID_ENCODER_6_DOWN, 0, false, false, 0},
  {HID_ENCODER_7_UP, 0, false, false, 0},
  {HID_ENCODER_7_DOWN, 0, false, false, 0},
  {HID_SELECTOR_1_OFF, 0, false, false, 0},
  {HID_SELECTOR_1_TR_G, 0, false, false, 0},
  {HID_SELECTOR_1_TR, 0, false, false, 0},
  {HID_SELECTOR_1_ADF, 0, false, false, 0},
  {HID_SELECTOR_1_CHG_PRST, 0, false, false, 0},
  {HID_SELECTOR_1_TEST, 0, false, false, 0},
  {HID_SELECTOR_1_ZERO, 0, false, false, 0},
  {HID_SELECTOR_2_ECCM_MASTER, 0, false, false, 0},
  {HID_SELECTOR_2_ECCM, 0, false, false, 0},
  {HID_SELECTOR_2_PRST, 0, false, false, 0},
  {HID_SELECTOR_2_MAN, 0, false, false, 0},
  {HID_SELECTOR_2_MAR, 0, false, false, 0},
  {HID_SELECTOR_2_243, 0, false, false, 0},
  {HID_SELECTOR_2_I2I, 0, false, false, 0}
};
constexpr size_t PULSE_BUTTON_COUNT = sizeof(pulseButtons) / sizeof(pulseButtons[0]);

uint32_t lastMcpPollAt = 0;
uint32_t lastSelectorPollAt = 0;
bool hidDirty = false;

bool elapsed(uint32_t now, uint32_t since, uint32_t interval) {
  return static_cast<uint32_t>(now - since) >= interval;
}

uint8_t readEncoderState(const EncoderInput &encoder) {
  return (digitalRead(encoder.pinA) ? 0b10 : 0) |
         (digitalRead(encoder.pinB) ? 0b01 : 0);
}

PulseButton *pulseButtonFor(uint8_t hidButton) {
  for (size_t index = 0; index < PULSE_BUTTON_COUNT; ++index) {
    if (pulseButtons[index].hidButton == hidButton) {
      return &pulseButtons[index];
    }
  }
  return nullptr;
}

void setHidButton(uint8_t hidButton, bool pressed) {
  Joystick.button(hidButton, pressed);
  if (DEBUG_SERIAL) {
    Serial.printf("Set Button %d \n\r", hidButton);
  }
  hidDirty = true;
}

void queuePulse(uint8_t hidButton, uint32_t now) {
  PulseButton *pulse = pulseButtonFor(hidButton);
  if (pulse == nullptr) {
    return;
  }

  if (!pulse->pressed && !pulse->releaseGap) {
    pulse->pressed = true;
    pulse->changedAt = now;
    setHidButton(hidButton, true);
  } else if (pulse->pendingCount < 255) {
    ++pulse->pendingCount;
  }
}

void servicePulseButtons(uint32_t now) {
  for (size_t index = 0; index < PULSE_BUTTON_COUNT; ++index) {
    PulseButton &pulse = pulseButtons[index];

    if (pulse.pressed && elapsed(now, pulse.changedAt, HID_PRESS_MS)) {
      pulse.pressed = false;
      pulse.releaseGap = true;
      pulse.changedAt = now;
      setHidButton(pulse.hidButton, false);
      continue;
    }

    if (pulse.releaseGap && elapsed(now, pulse.changedAt, HID_RELEASE_GAP_MS)) {
      pulse.releaseGap = false;
      if (pulse.pendingCount > 0) {
        --pulse.pendingCount;
        pulse.pressed = true;
        pulse.changedAt = now;
        setHidButton(pulse.hidButton, true);
      }
    }
  }
}

void initialiseMcpButtons(uint32_t now) {
  const uint16_t rawPins = mcp.readGPIOAB();
  for (uint8_t pin = 0; pin < MCP_BUTTON_COUNT; ++pin) {
    const bool pressed = !(rawPins & (1u << pin));
    mcpButtons[pin] = {pressed, pressed, now};
    setHidButton(HID_PUSH_BUTTON_1 + pin, pressed);
  }
}

void updateMcpButtons(uint32_t now) {
  if (!elapsed(now, lastMcpPollAt, MCP_POLL_INTERVAL_MS)) {
    return;
  }
  lastMcpPollAt = now;

  const uint16_t rawPins = mcp.readGPIOAB();
  for (uint8_t pin = 0; pin < MCP_BUTTON_COUNT; ++pin) {
    const bool pressed = !(rawPins & (1u << pin));
    DebouncedInput &button = mcpButtons[pin];

    if (pressed != button.candidate) {
      button.candidate = pressed;
      button.candidateSince = now;
    }

    if (button.stable != button.candidate &&
      elapsed(now, button.candidateSince, BUTTON_DEBOUNCE_MS)) {
      button.stable = button.candidate;
      setHidButton(HID_PUSH_BUTTON_1 + pin, button.stable);
    }
  }
}

void updateEncoders(uint32_t now) {
  // Transition table for a two-bit quadrature encoder. Reverse the two wires
  // for one encoder, or swap its positive/negative button numbers, if needed.
  static constexpr int8_t QUADRATURE_TRANSITIONS[16] = {
    0, -1, 1, 0,
    1, 0, 0, -1,
    -1, 0, 0, 1,
    0, 1, -1, 0,
  };

  for (size_t index = 0; index < ENCODER_COUNT; ++index) {
    EncoderInput &encoder = encoders[index];
    const uint8_t state = readEncoderState(encoder);
    const int8_t transition = QUADRATURE_TRANSITIONS[(encoder.lastState << 2) | state];
    encoder.lastState = state;

    if (transition == 0) {
      continue;
    }

    encoder.edgeAccumulator += transition;
    if (encoder.edgeAccumulator >= ENCODER_EDGES_PER_DETENT) {
      encoder.edgeAccumulator = 0;
      queuePulse(encoder.positiveButton, now);
    } else if (encoder.edgeAccumulator <= -ENCODER_EDGES_PER_DETENT) {
      encoder.edgeAccumulator = 0;
      queuePulse(encoder.negativeButton, now);
    }
  }
}

uint8_t readSelectorPosition(uint8_t adcPin) {
  const uint16_t raw = analogRead(adcPin);
  const uint8_t position = static_cast<uint8_t>((raw + 480) / 512);
  return position;
}

void emitSelectorSteps(const SelectorInput &selector, uint8_t from, uint8_t to, uint32_t now) {
  const uint8_t pulseButton = selector.buttons[to - 1];
  if (DEBUG_SERIAL) {
    Serial.printf("Selector: pin %d, button: %d, position: %d \n", selector.adcPin, pulseButton, to);
  }
  queuePulse(pulseButton, now);
}

void initialiseSelectors(uint32_t now) {
  for (size_t index = 0; index < SELECTOR_COUNT; ++index) {
    SelectorInput &selector = selectors[index];
    const uint8_t position = readSelectorPosition(selector.adcPin);
    selector.stablePosition = position;
    selector.candidatePosition = position;
    selector.candidateSince = now;
  }
}

void updateSelectors(uint32_t now) {
  if (!elapsed(now, lastSelectorPollAt, MCP_POLL_INTERVAL_MS)) {
    return;
  }
  lastSelectorPollAt = now;

  for (size_t index = 0; index < SELECTOR_COUNT; ++index) {
    SelectorInput &selector = selectors[index];
    const uint8_t position = readSelectorPosition(selector.adcPin);

    if (position != selector.candidatePosition) {
      selector.candidatePosition = position;
      selector.candidateSince = now;
    }

    if (position != selector.stablePosition &&
        elapsed(now, selector.candidateSince, SELECTOR_DEBOUNCE_MS)) {
      const uint8_t previous = selector.stablePosition;
      selector.stablePosition = position;

      // Do not emit a command while a selector is on its invalid terminal.
      if (previous != position) {
        emitSelectorSteps(selector, previous, position, now);
      }
    }
  }
}

void setup() {
  if (DEBUG_SERIAL) {
    Serial.begin(115200);
    delay(250);
    Serial.println("ARC-210 controller starting");
  }

  analogReadResolution(12);

  Wire.setSDA(I2C_SDA_PIN);
  Wire.setSCL(I2C_SCL_PIN);
  Wire.begin();
  Wire.setClock(400000);

  while (!mcp.begin_I2C(MCP23017_ADDRESS, &Wire)) {
    if (DEBUG_SERIAL) {
      Serial.println("MCP23017 not found. Check power, GND, SDA, SCL, and address jumpers.");
    }
    delay(1000);
  }

  for (uint8_t pin = 0; pin < MCP_BUTTON_COUNT; ++pin) {
    mcp.pinMode(pin, INPUT_PULLUP);
  }

  for (size_t index = 0; index < ENCODER_COUNT; ++index) {
    EncoderInput &encoder = encoders[index];
    pinMode(encoder.pinA, INPUT_PULLUP);
    pinMode(encoder.pinB, INPUT_PULLUP);
    encoder.lastState = readEncoderState(encoder);
  }

  Joystick.begin();
  Joystick.useManualSend(true);

  const uint32_t now = millis();
  initialiseMcpButtons(now);
  initialiseSelectors(now);
  Joystick.send_now();
  hidDirty = false;

  if (DEBUG_SERIAL) {
    Serial.println("MCP23017 found; USB joystick ready.");
  }
}

void loop() {
  const uint32_t now = millis();
  updateEncoders(now);
  updateMcpButtons(now);
  updateSelectors(now);
  servicePulseButtons(now);

  if (hidDirty) {
    Joystick.send_now();
    hidDirty = false;
  }
}
