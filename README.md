# BCA182 FreeRTOS Multisensor System

A FreeRTOS-based multisensor monitoring system developed using an STM32F103 Blue Pill. The system collects environmental and motion data, displays sensor information on an OLED, provides temperature alarms, and uses FreeRTOS tasks and inter-task communication to manage the application.

## Project Overview

This project demonstrates the use of a real-time operating system in an embedded multisensor application.

The system integrates:

* DHT22 temperature and humidity sensor
* LDR light sensor
* PIR motion sensor
* Rotary encoder for display navigation
* OLED display
* Buzzer alarm
* STM32F103 Blue Pill
* FreeRTOS

The system uses multiple independent FreeRTOS tasks to separate sensing, display, input, motion detection, system-state management, and alarm handling.

## System Architecture

The application is organized into separate modules and FreeRTOS tasks.

| Module                  | Responsibility                               |
| ----------------------- | -------------------------------------------- |
| `sensors.cpp`           | Sensor acquisition and sensor task           |
| `input.cpp`             | Rotary encoder input and display navigation  |
| `motion.cpp`            | PIR motion detection                         |
| `state.cpp`             | Active/inactive system-state management      |
| `alarm.cpp`             | Temperature alarm handling                   |
| `display.cpp`           | OLED display management                      |
| `navigation.cpp`        | Display-mode navigation logic                |
| `temperature_alarm.cpp` | Temperature threshold evaluation             |
| `main.cpp`              | Hardware initialization and FreeRTOS startup |

## FreeRTOS Tasks

### SensorTask

Reads the connected environmental sensors and provides sensor data to other parts of the application using a FreeRTOS queue.

### InputTask

Reads the rotary encoder and changes the OLED display mode.

Clockwise navigation:

```text
TEMPERATURE
     ↓
HUMIDITY
     ↓
MOTION
     ↓
LIGHT
     ↓
ALERT
     ↓
TEMPERATURE
```

Counterclockwise navigation follows the reverse order.

### MotionTask

Monitors the PIR sensor and signals motion events through the FreeRTOS event group.

### StateTask

Controls the system activity state.

The system starts in the `ACTIVE` state. After 15 seconds without motion, the system changes to `INACTIVE`. Motion detection can reactivate the system.

### AlarmTask

Evaluates temperature conditions:

```text
Temperature < 18 °C  → LOW TEMPERATURE
18 °C to 30 °C       → NORMAL
Temperature > 30 °C  → HIGH TEMPERATURE
```

The buzzer is activated when an alarm condition is present while the system is active.

### DisplayTask

Controls the OLED display and presents the currently selected sensor information.

## Hardware Connections

| Device             | STM32 Pin |
| ------------------ | --------- |
| DHT22              | PA1       |
| LDR / ADC          | PA0       |
| PIR                | PA3       |
| Rotary Encoder CLK | PA4       |
| Rotary Encoder DT  | PA5       |
| OLED I2C SCL       | PB6       |
| OLED I2C SDA       | PB7       |

## Software and Tools

* C++
* STM32F103
* STM32Cube framework
* FreeRTOS
* PlatformIO
* Unity unit testing framework
* Cppcheck
* Wokwi

## Building the Project

Clone the repository:

```bash
git clone https://github.com/YOUR-USERNAME/YOUR-REPOSITORY.git
```

Enter the project directory:

```bash
cd bca182-freertos-multisensor
```

Build the firmware with PlatformIO:

```bash
pio run -e bluepill_f103c8
```

## Unit Testing

The project contains automated unit tests for the application logic.

The final test run produced:

```text
15 Tests 0 Failures 0 Ignored
OK
```

### Test Coverage

| Category            |  Tests |
| ------------------- | -----: |
| Temperature alarm   |      5 |
| Display navigation  |      6 |
| System state/events |      4 |
| **Total**           | **15** |

The tests cover temperature threshold behavior, display navigation, state configuration, inactivity timing, and FreeRTOS event-bit definitions.

## Static Analysis

Cppcheck was used for static code analysis.

Final result:

```text
HIGH       0
MEDIUM     0
LOW       35

PASSED
```

The reported low-severity findings were primarily unused-function/style warnings associated with embedded application modules. No high- or medium-severity findings were reported.

## Verification

The final firmware was successfully compiled and linked using PlatformIO.

Testing and static analysis were also completed successfully.

### Results

```text
Firmware build:      SUCCESS
Unit tests:          15 / 15 PASSED
Static analysis:     PASSED
High issues:         0
Medium issues:       0
Low issues:          35
```

## Project Structure

```text
bca182-freertos-multisensor/
│
├── include/
│   ├── FreeRTOSConfig.h
│   └── application headers
│
├── lib/
│   ├── FreeRTOS/
│   └── Unity/
│
├── src/
│   ├── main.cpp
│   ├── sensors.cpp
│   ├── input.cpp
│   ├── motion.cpp
│   ├── state.cpp
│   ├── alarm.cpp
│   ├── display.cpp
│   ├── navigation.cpp
│   └── temperature_alarm.cpp
│
├── test/
│   ├── test_main/
│   └── mocks/
│
├── platformio.ini
├── wokwi.toml
└── README.md
```

## Author

**BCA182 FreeRTOS Multisensor Project**

Developed as part of an embedded systems / real-time operating systems laboratory project.

## License

This project was developed for academic purposes.
