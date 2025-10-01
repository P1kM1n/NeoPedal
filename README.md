# NeoPedal

An intelligent LED strip ecosystem for your car. NeoPedal uses sensors and signal taps to dynamically adjust LED strips based on real-time driving conditions and user actions.

✨ Features

Dynamic Lighting – LEDs react instantly to pedal input, speed, or other sensor data.

Modular Setup – Works with multiple ESP32 nodes for distributed control.

Low Latency – ESP-NOW protocol ensures near-instant updates between controller and LED modules.

Custom Animations – Define colour patterns, fades, and transitions to suit your style.

Scalable Design – Add more LED strips or sensors without redesigning the system.

🛠️ Hardware Requirements

ESP32 boards (minimum 2: one controller + one LED node).

WS2812B (or compatible) addressable LED strips.

VL53L1X or similar Time-of-Flight sensor (for pedal tracking).

Car wiring taps (to read signals like brake/accelerator).

5V regulated power supply (capable of handling LED current draw).

📦 Installation

Flash ESP32s – Upload the controller and LED node firmware to your ESP32 boards.

Configure MAC Addresses – Set the controller to broadcast to node MAC addresses.

Connect LEDs – Wire the LED strips to each ESP32 node.

Mount Sensors – Place ToF sensors near pedals (or other chosen input points).

Wire Taps – Connect to vehicle signals where needed (e.g., brake light).

⚙️ Usage

Controller ESP32 reads pedal/sensor data.

Data is broadcast over ESP-NOW to Node ESP32s.

Nodes control their assigned LED strips, applying animations in real-time.

Adjust config files to map which events (pedal, braking, acceleration) trigger which LED behaviours.
