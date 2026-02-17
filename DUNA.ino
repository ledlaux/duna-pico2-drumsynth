/*
  DUNA -
  
  Raspberry PI PICO2 drum synthesizer based on Mutable Instruments Plaits (Macro-Oscillator) 
  and Grids (Topographic Drum Sequencer).

  Features:
  - 4 x Plaits-based drum engines (Engines 1-3 + FM Engine with syx banks)
  - 1 x Sample-based Percussion Engine
  - Algorithmic Sequencing: Integrated Euclidean and Topographic (Grids) logic
  - WebUI dashboard with midi cc controls 
  
  Hardware Mapping:
  - I2S DAC: PCM5102 
  - Controller: RP2350
  - MIDI: USB-MIDI 

  Compilation Settings (Recommended):
  - RP2350 Clock: 300MHz (Overclock)
  - Optimize: Optimize Even More (-O3)
  - USB stack: Adafruit Tiny USB 

  Credits:
  - PLAITS & GRIDS original DSP: Émilie Gillet (Mutable Instruments), MIT licence
  - PLAITS & STMLIB (ArduinoMI ported libraries) by Mark Washeim, MIT licence
  

  This project is licensed under the MIT License.
*/


#include <Arduino.h>
#include <I2S.h>
#include <Adafruit_TinyUSB.h>
#include <STMLIB.h>
#include <PLAITS.h>
#include <atomic>
#include "grids_resources.h"
#include "sample.h" 

#define SAMPLE_RATE 44100
#define AUDIO_BLOCK 24
#define I2S_DATA_PIN 9
#define I2S_BCLK_PIN 10

std::atomic<int> safe_mode{0};
std::atomic<int> safe_map_x{64}, safe_map_y{64}, safe_randomness{0};
std::atomic<int> safe_density[5];
std::atomic<float> safe_volumes[5], safe_pitches[5];
std::atomic<bool> trig_ready[4] = {false,false,false,false};
std::atomic<float> vel_shared[4] = {0,0,0,0};
std::atomic<int> engine_shared[4] = {13,14,15,17};
std::atomic<bool> running{false};
std::atomic<int> clock_ticks{0};
std::atomic<int> safe_clock_div{1};
std::atomic<int> safe_euclid_shift{0}, safe_euclid_skip{0}, safe_euclid_add{0};
std::atomic<int> safe_vel_rand{0}, safe_swing{0};
std::atomic<bool> sample_trig_pending{false};
float sample_playhead = -1.0f;
float masterVolume = 0.8f;

struct DrumParams { float harmonics; float timbre; float morph; float decay; };
volatile DrumParams patches[4] = {
  {0.5,0.5,0.5,0.3},{0.5,0.5,0.5,0.3},{0.5,0.5,0.5,0.2},{0.2,0.5,0.5,0.4}
};

I2S i2s(OUTPUT);
Adafruit_USBD_MIDI usb_midi;

struct DrumVoice {
    plaits::Voice voice;
    plaits::Patch patch;
    plaits::Modulations mod;
    plaits::Voice::Frame buffer[AUDIO_BLOCK];
};
static uint8_t workspaces[4][16384] __attribute__((aligned(8)));
static DrumVoice voices[4];

uint32_t __not_in_flash_func(fast_rand)() {
    static uint32_t x = 123456789;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return x;
}

bool __not_in_flash_func(euclidean_step_calc)(int step, int length, int pulses) {
    if(length<=0||pulses<=0) return false;
    return ((step*pulses)/length) > (((step-1)*pulses)/length);
}

void __not_in_flash_func(handle_midi_packet)(uint8_t* packet) {
    uint8_t status = packet[1];
    uint8_t d1 = packet[2]; 
    uint8_t d2 = packet[3];
    float fval = d2 / 127.0f;
    
    if ((status & 0xF0) == 0xB0) { 
        switch(d1) {

            case 8:  masterVolume = fval; break;
            case 17: safe_clock_div.store(map(d2,0,127,1,8)); break;
            case 18: safe_euclid_shift.store(d2); break;
            case 19: safe_euclid_skip.store(d2); break;
            case 20: safe_euclid_add.store(d2); break;
            case 21: safe_map_x.store(d2); break;
            case 22: safe_map_y.store(d2); break;
            case 23: safe_randomness.store(d2); break;
            case 24: safe_mode.store(d2<64?0:1); break;
            case 25: safe_vel_rand.store(d2); break;
            case 26: safe_swing.store(d2); break;
            case 60: safe_volumes[4].store(fval); break;
            case 61: safe_pitches[4].store(0.5f + fval*1.5f); break;
            case 62: safe_density[4].store(d2); break;
            default:
                int idx=-1;
                if(d1>=30&&d1<=37) idx=0;
                else if(d1>=40&&d1<=47) idx=1;
                else if(d1>=50&&d1<=57) idx=2;
                else if(d1>=70&&d1<=77) idx=3;
                if(idx!=-1){
                    int offset = (idx==0)?d1-30:(idx==1)?d1-40:(idx==2)?d1-50:d1-70;
                    switch(offset){
                        case 0: patches[idx].harmonics=fval; break;
                        case 1: patches[idx].timbre=fval; break;
                        case 2: patches[idx].morph=fval; break;
                        case 3: patches[idx].decay=0.02f + fval*0.98f; break;
                        case 4: safe_pitches[idx].store(12.0f + fval*60.0f); break;
                        case 5: safe_density[idx].store(d2); break;
                        case 6: safe_volumes[idx].store(fval); break;
                        case 7: engine_shared[idx].store(10 + (int)(fval*6.99f)); break;
                    }
                }
                break;
        }
    }
}

void __not_in_flash_func(do_sequencer_step)(int step_num) {
    int mode  = safe_mode.load();        // 0 = Grids, 1 = Euclid
    bool use_xy = (mode == 1);

    int chaos = safe_randomness.load();
    int v_amt = safe_vel_rand.load();
    int local_step = step_num & 31;

    static const uint8_t euclid_lengths[] = {4, 5, 7, 8, 11, 16, 24, 32};

    for (int inst = 0; inst < 5; inst++) {
        bool hit = false;
        float base_vel = 0.6f;

        if (mode == 1 || inst >= 3) {

            int len = euclid_lengths[
    use_xy ? (safe_map_x.load() >> 4) : 5
];

            int pulses =
                max(1, (safe_density[inst].load() * len) / 127);

            int phase =
    use_xy ? ((safe_map_y.load() * len) >> 7) : 0;
            int step  = (local_step + phase) % len;

            bool euclid_hit = euclidean_step_calc(step, len, pulses);

            if (chaos > 0) {
                uint8_t r = fast_rand() & 0x7F;

                if (euclid_hit && r < chaos) {
                    euclid_hit = false;              
                } else if (!euclid_hit && r < (chaos >> 2)) {
                    euclid_hit = true;               
                }
            }

            hit = euclid_hit;
        }

        else {
            int gx = constrain(safe_map_x.load() >> 5, 0, 3);
            int gy = constrain(safe_map_y.load() >> 5, 0, 3);

            uint8_t val =
                drum_map[gx][gy][(inst * 32) + local_step];

            int chaos_jitter =
                chaos ? ((int)(fast_rand() % chaos) - (chaos >> 1)) : 0;

            if ((int)val + chaos_jitter >
                (255 - (safe_density[inst].load() * 2))) {
                hit = true;
                base_vel = val / 255.0f;
            }
        }

        if (hit) {
            if (inst == 4) {
                sample_trig_pending.store(true);
            } else {
                float v_noise =
                    v_amt ? (((fast_rand() % 200) / 100.0f - 1.0f) *
                             (v_amt * 0.004f)) : 0.0f;

                float final_vel =
                    constrain(base_vel + v_noise, 0.1f, 1.0f);

                vel_shared[inst].store(final_vel);
                trig_ready[inst].store(true);
            }
        }
    }
}

void setup(){

    TinyUSBDevice.setManufacturerDescriptor("LEDLAUX");
    TinyUSBDevice.setProductDescriptor("DUNA-PICO2");
    TinyUSBDevice.setSerialDescriptor("RP2350-01");
    usb_midi.begin();

    i2s.setFrequency(SAMPLE_RATE);
    i2s.setDATA(I2S_DATA_PIN); i2s.setBCLK(I2S_BCLK_PIN); i2s.begin();

    for(int i=0;i<4;i++){
        stmlib::BufferAllocator alloc(workspaces[i], sizeof(workspaces[i]));
        voices[i].voice.Init(&alloc);
        voices[i].mod.trigger_patched = true;
        voices[i].mod.level_patched = true;
        safe_density[i].store(80); safe_volumes[i].store(0.6f); safe_pitches[i].store(36.0f+(i*12.0f));
    }
    safe_density[4].store(64); safe_volumes[4].store(0.8f); safe_pitches[4].store(1.0f);
}

void loop(){
    for(int i=0;i<4;i++){
        voices[i].patch.engine = engine_shared[i].load();
        voices[i].patch.note = safe_pitches[i].load();
        voices[i].patch.harmonics = patches[i].harmonics;
        voices[i].patch.timbre = patches[i].timbre;
        voices[i].patch.morph = patches[i].morph;
        voices[i].patch.decay = patches[i].decay;

        if(trig_ready[i].exchange(false)){
            voices[i].mod.trigger = 1.0f;
            voices[i].mod.level = vel_shared[i].load();
        } else voices[i].mod.trigger=0.0f;

        voices[i].voice.Render(voices[i].patch, voices[i].mod, voices[i].buffer, AUDIO_BLOCK);
    }

    if(sample_trig_pending.exchange(false)) sample_playhead = 0.0f;
    float s_p_step = safe_pitches[4].load();

    for(int s=0;s<AUDIO_BLOCK;s++){
        float mixed=0.0f;
        for(int i=0;i<4;i++)
            mixed += (voices[i].buffer[s].out/32768.0f) * safe_volumes[i].load() * 2.5f; // FM boost

        if(sample_playhead>=0 && (uint32_t)sample_playhead<SAMPLE_LEN-1){
            uint32_t idx=(uint32_t)sample_playhead;
            mixed += (sample_data[idx]/32768.0f)*safe_volumes[4].load();
            sample_playhead += s_p_step;
        } else if((uint32_t)sample_playhead>=SAMPLE_LEN-1) sample_playhead=-1.0f;

        float finalGain = masterVolume * 0.6f * 32767.0f;
        int16_t outS = (int16_t)constrain(mixed * finalGain, -32768, 32767);
        i2s.write16(outS,outS);
    }

    static int current_step=0, div_counter=0;
    static uint32_t execute_step_at=0;
    static bool step_pending=false;

    uint32_t now = millis();
    if(step_pending && now>=execute_step_at){
        do_sequencer_step(current_step);
        current_step=(current_step+1)%32;
        step_pending=false;
    }

    uint8_t packet[4];
    if(usb_midi.readPacket(packet)){
        uint8_t status=packet[1];
        if(status==0xF8){ // clock
            int ticks = clock_ticks.load()+1;
            if(ticks>=6){
                ticks=0;
                div_counter++;
                if(div_counter>=safe_clock_div.load()){
                    div_counter=0;
                    if(running.load()){
                        int swing=safe_swing.load();
                        if(current_step%2!=0 && swing>0){
                            execute_step_at=now+map(swing,0,127,0,50);
                            step_pending=true;
                        } else {
                            do_sequencer_step(current_step);
                            current_step=(current_step+1)%32;
                        }
                    }
                }
            }
            clock_ticks.store(ticks);
        } else if(status==0xFA){ running.store(true); current_step=0; }
        else if(status==0xFC){ running.store(false); }
        else handle_midi_packet(packet);
    }
}
