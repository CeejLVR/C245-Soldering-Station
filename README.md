# C245 Soldering Station

CSE321 project - Christian Jagiello and Ryan Luzi

JBC style soldering station for C245 tips using a Raspberry Pi Pico (RP2040) running FreeRTOS. Hardware notes/schematics are in the project docs, this is just for the firmware.

## Building

Open the folder in VS Code with the Raspberry Pi Pico extension and hit Compile (or Run with the pico in BOOTSEL mode). pico-sdk and FreeRTOS-Kernel are submodules in `lib/` so after cloning run:

```
git submodule update --init --recursive
```

Output is `build/solder_station.uf2`.

## Tasks

| Task | Priority | Runs every | What it does |
|---|---|---|---|
| safety | 5 | 25 ms | kicks the 74HC123 watchdog, overtemp/runaway/stall checks, feeds the pico watchdog |
| control | 4 | 100 ms | reads the thermocouple + cold junction, tip check, state machine, PID, heater on time |
| timer task | 2 | - | button debounce, sleep/auto off timers, saving settings |
| ui | 1 | 100 ms | OLED + encoder/buttons |
| log | 1 | 200 ms | prints csv over usb |

Tasks talk through queues (status mailbox, command queue, input events), a task notification from the ADS1220 DRDY interrupt, and a mutex for the I2C bus since the OLED and MCP9808 share it.

## Control loop

Each 100ms cycle goes:

heater off -> wait 1ms -> read MCP9808 -> close relays -> wait ~2ms -> (tip check every 10th cycle) -> ADS1220 conversion (~3ms) -> open relays -> wait 1ms -> PID -> heater on for duty * 100ms

The thermocouple can only be read while the heater is off, which is why GP15 is a normal gpio instead of PWM. heater.c won't let the relays close if the heater is on (or the heater turn on while the relays are closed) and gives a FAULT_INTERLOCK if that ever happens. This is only in software right now, we still need a hardware interlock.

Tip detection uses the ADS1220 burn out current sources. With no tip the reading should go to full scale. Still need to test this.

## Safety stuff

- **74HC123 watchdog** - tested every time it boots (kick it, output goes high, wait, output goes low). Heater stays locked out until this passes. If the safety task stops kicking it the heater gets cut in ~110ms by hardware.
- **Pico watchdog** (500ms) - resets the chip if the safety task freezes. It boots into a WDT RESET fault after that and you have to clear it.
- **Safety task** - overtemp at 480C, control task heartbeat, thermal runaway (40%+ duty for 5s with less than 10C rise), watchdog output dropping while being kicked
- **Control task** - ADC or MCP9808 missing, thermocouple maxed out or reading way too cold
- **HEATER_MAX_DUTY** is 0.5 for now and **CFG_HEATER_ARMED** is 0 by default (heater pin never turns on)

None of this helps if the GATE wire gets shorted to ground on the breadboard, only the fuse does.

## Controls

- turn encoder: setpoint +/- 5C
- PSH (encoder button): heater on/off, clears a fault if there is one
- CON: next preset
- hold CON: save current setpoint to the preset
- BAK: switch between number and graph screen

"DRY" in the top right means the heater isn't armed. Settings save to flash 2 sec after you stop changing things.

## Testing order

Leave `CFG_HEATER_ARMED` at 0 until step 5.

1. Just the pico on USB. Should show FAULT HW WATCHDOG or FAULT ADC since nothing is hooked up. Encoder/buttons should work and the serial log should be printing.
2. Hook up the 74HC123, 74HC08, TC4427 (no perfboard). Fault should go away. Scope GP21 (kicks every 25ms) and U3 pin 13 (should stay high). Pull the GP21 wire and pin 13 should drop after ~100ms and show FAULT HW WATCHDOG.
3. ADS1220 + MCP9808 with the tip in, heater power OFF. Heat the tip with another iron and watch tc_uv in the log, it should go up. If it goes down change TC_POLARITY to -1. Pull the tip out and it should say NO TIP within a second.
4. Scope GP6 and GP15. GP6 should pulse ~6ms every 100ms (~18ms on tip check cycles) and GP15 should never go high.
5. Set `CFG_HEATER_ARMED` to 1. 2.5 ohm dummy load on J2, current limited bench supply at ~5V. GP15 and GP6 should never be high at the same time.
6. Real tip, bench supply ~5V with 1-2A limit, iron in the stand and stay with it. Turn up the voltage slowly and tune the PID from the log.
7. Calibrate with solder melting points (63/37 = 183C, SAC305 = ~217-220C) by changing TC_SENS_UV_PER_C / TC_OFFSET_UV.

## Serial log

```
# ms,state,tip_c,cj_c,tc_uv,set_c,demand,applied,tip,armed,fault
12400,HEATING,214.6,24.50,4006.3,320,0.500,0.500,1,1,none
```

demand is what the PID wants, applied is what actually went to GP15 (0 if not armed)

## Files

- `include/app_config.h` - all the settings (timing, limits, PID, calibration, arm switch)
- `include/PinDefinitions.h` - pin map
- `src/heater.c` - heater pin, relay pin, interlock
- `src/ext_watchdog.c` - 74HC123
- `src/ads1220.c` - thermocouple ADC
- `src/mcp9808.c`, `src/ssd1306.c`, `src/i2c_bus.c` - I2C stuff
- `src/input.c` - encoder and buttons
- `src/pid.c`, `src/thermo.c` - PID and temp conversion
- `src/storage.c` - saving settings/presets to flash
- `src/station.c` - queues shared between tasks
- `src/*_task.c` - the tasks
- `src/gpio_irq.c` - gpio interrupt callback

## TODO

- hardware relay interlock
- stand detection, buzzer/LEDs
- calibration menu (long press PSH/BAK aren't used yet)
- serial commands for changing PID values without reflashing

Font in `font5x7.h` is from the Adafruit GFX library (BSD license).
