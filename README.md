# Device-Condition-Monitor
Device Condition Monitor is a desktop application built in C++ (wxWidgets) that collects and logs device condition and usage metrics. The project demonstrates an end-to-end workflow — from data collection in a C++ UI to data visualization in Power BI, aligning with Siemens Energy’s focus on data-driven grid technology solutions.

# MiniGridMonitor

**Author:** Vaishnavi Manogaran  
**Date:** November 2025

## Overview
MiniGridMonitor is a C++ desktop app (wxWidgets) that captures device condition and exports CSV for Power BI dashboards.

## Contents
- `src/` — C++ source files
- `docs/` — documentation (Project Documentation – Device Condition Capture.docx)
- `build/` — recommended build directory (ignored)
- `examples/` — sample CSV data used by the Power BI dashboard
- <img width="1917" height="1027" alt="image" src="https://github.com/user-attachments/assets/2769cce0-0da4-4b88-a212-4c117999c4a5" />

- <img width="1133" height="646" alt="image" src="https://github.com/user-attachments/assets/27fb4c89-b9e3-45b0-8ac5-396c2dcb7c0b" />


## Build (example)
This project uses [CMake]. Basic steps (Linux/macOS):
```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
