# SDR-Based Industrial IoT Dashboard

## Overview
This project presents a real-time industrial monitoring system using ESP32 nodes, ESP-NOW communication, MQTT protocol, and SDR-based RF analysis. The system collects environmental and motor data, transmits it wirelessly, and visualizes it on a live dashboard.

## System Components
- ESP32 Node1 → Environmental monitoring (temperature, gas, vibration)
- ESP32 Motor → Motor control and vibration sensing
- LILYGO Gateway → ESP-NOW to MQTT bridge
- RTL-SDR → RF signal monitoring
- Dashboard → Node-RED visualization

## Technologies Used
- C/C++ (ESP32 firmware)
- Python (SDR processing)
- MQTT protocol
- Node-RED (dashboard)

## Project Structure
- ESP32-Node1/
- ESP32-Motor/
- LILYGO/
- RTL-SDR/
- Dashboard/

## Features
- Real-time sensor monitoring
- Wireless communication (ESP-NOW)
- MQTT data transmission
- Live dashboard visualization
- Motor control system
- RF signal monitoring using SDR
