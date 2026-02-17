# DUNA

Raspberry Pi PICO2 drum synthesizer based on Mutable Instruments **Plaits** (Macro-Oscillator)  
and **Grids** (Topographic Drum Sequencer).

This is first test version with 3 drum engines, 1 FM engine bank with 32 instruments and 1 sample loaded. More dx7 syx drum banks and samples will be added in future.   

FM and Sample engines are controlled always by Euclidian sequencer. Engines 1–3, depending on the selector, play in Grids or Euclidean sequencer mode.

![DUNA-web](images/duna-web.jpg)

---

## Features

- 4 x Plaits-based drum engines (Engines 1-3 + FM Engine with 32 instrument .syx bank)  
- 1 x Sample-based Percussion Engine  
- Algorithmic Sequencing: Integrated Euclidean and Topographic (Grids) logic  
- High-Resolution UI: Web-based MIDI Dashboard with XY Mapping
- Export / Load presets in webui

---

## Hardware Mapping

- **I2S DAC:** PCM5102  
- **Controller:** RP2350  
- **MIDI:** USB-MIDI  

---

## Compilation Settings (Recommended)

- **RP2350 Clock:** 300 MHz (Overclock)  
- **Optimization:** Optimize Even More (-O3)  
- **USB Stack:** Adafruit TinyUSB  

---

## MIDI CC Assignments

| Engine         | CC         | Parameter             |
|----------------|------------|----------------------|
| Global         | 8          | Master Volume        |
| Global         | 17         | Clock Divider        |
| Global         | 21         | XY Pad X             |
| Global         | 22         | XY Pad Y             |
| Global         | 23         | Chaos                |
| Global         | 24         | Grids / Euclid       |
| Global         | 25         | Random Velocity      |
| Engines 1–3    | 30 / 40 / 50 | Harmonics           |
| Engines 1–3    | 31 / 41 / 51 | Timbre              |
| Engines 1–3    | 32 / 42 / 52 | Morph               |
| Engines 1–3    | 33 / 43 / 53 | Decay               |
| Engines 1–3    | 34 / 44 / 54 | Pitch               |
| Engines 1–3    | 35 / 45 / 55 | Density             |
| Engines 1–3    | 36 / 46 / 56 | Volume              |
| Engines 1–3    | 37 / 47 / 57 | Model               |
| FM Engine      | 70         | Instrument          |
| FM Engine      | 71         | Timbre              |
| FM Engine      | 72         | Morph               |
| FM Engine      | 73         | Decay               |
| FM Engine      | 74         | Pitch               |
| FM Engine      | 75         | Density             |
| FM Engine      | 76         | Volume              |
| FM Engine      | 77         | Bank                |
| Sample Engine  | 60         | Volume              |
| Sample Engine  | 61         | Pitch               |
| Sample Engine  | 62         | Density             |
| Sample Engine  | 67         | Select              |

---


## Credits

- **PLAITS & GRIDS original DSP:** Émilie Gillet (Mutable Instruments), MIT license  
- **PLAITS & STMLIB Arduino port:** [arduinoMI](https://github.com/poetaster/arduinoMI), Mark Washeim, MIT license

---

## License

This project is licensed under the **MIT License**.
