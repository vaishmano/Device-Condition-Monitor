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

## Build (example)
This project uses [CMake]. Basic steps (Linux/macOS):
```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
