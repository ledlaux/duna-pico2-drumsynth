# DUNA

Raspberry Pi PICO2 drum synthesizer based on Mutable Instruments **Plaits** (Macro-Oscillator)  
and **Grids** (Topographic Drum Sequencer).



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

## Credits

- **PLAITS & GRIDS original DSP:** Émilie Gillet (Mutable Instruments), MIT license  
- **PLAITS & STMLIB Arduino port:** [arduinoMI](https://github.com/poetaster/arduinoMI), Mark Washeim, MIT license

---

## License

This project is licensed under the **MIT License**.
