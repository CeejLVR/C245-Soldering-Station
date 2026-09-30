# Closed-Loop C210 Soldering Station


## Team Members:
Christian Jagiello (UBIT: Cjjagiel)
Ryan Luzi (UBIT: ryanluzi)




## Application
  Our project will be best applied as a home lab hobby project for someone who is frequently soldering. Our design aims to improve nearly every aspect of soldering, making it safer and easier. Precise temperature control can make a huge difference in the quality and reliability of our solder joints. More specifically, soldering small joints on audio equipment, electronics, or similar boards requires accurate temperature control. In addition to the smaller joints, large pads or ground planes which have a large thermal mass make soldering with a generic iron considerably more difficult due to the lack of temperature stability. Therefore, any improvement in temperature control will allow us to better keep the iron within both the safest range, but the best temperatures, which then prevents damage to components and pads.


## Hardware Design and RTOS Plan
  The microcontroller we plan on using for this soldering station is a generic version of the raspberry pi Pico. Boards powered by the RP2040 chip are widely available for cheap online. Alongside this, we plan to use a dedicated ADC module for the thermocouple readings, as it will create a much more reliable and noise-free output. Buttons and switches will be used for all user interaction, and a rotary encoder for temperature adjustments alongside a screen to display relevant information. The Iron itself will be a prebuilt T210 handle that can take its respective C210 cartridge tips. The station, however, and all related control systems/logic will be fully designed by us.
  
  The C210 tips have a thermocouple built into the tip itself which will let us accurately measure and record the real-time temperature values. PID Control loops running as a task within the RTOS will constantly monitor the temperature and make active adjustments as needed to keep the desired temperature.
For the software and RTOS side, we will have unique tasks and priorities running in FreeRTOS for the following features:
* read the ADC
* write to an external EEPROM for non-volatile, savable presets,
* PID Control and PWM
* State machine (off, heating, standby, fault, ready)
* Display updates (with different modes of display: numerical vs. graphed)

  We will also have things like software timers to put it in standby/hibernation and buzzer utilization for warnings. The RP2040 gives us a unique advantage with its built-in PIO state machines, though we have not decided entirely how to utilize them.
## Timeline
### Checkpoint 1, due on October 11th (~2-week duration):
* Have all the parts readily accessible
* Removeable and hot-swappable c210 Soldering iron tips
* Input capture and interrupts (via rotary encoder and PIO)
### Checkpoint 2, due on November 1st (3-week duration):
* Active temperature control/PID tuning
* Soldering Tip presence detection and auto-sleep features
* Hardware based over-temp safety cutoff
* EEPROM over I2C for memory presets after power cycles
### Final Submission, due on November 29th (~4-week duration):
* Indication lights and warning sounds for the current state of the iron
* Display for temperature readings, numbered and graphed (available in different modes)
* Final designs for 3d printed housing/enclosure
