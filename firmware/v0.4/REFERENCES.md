# v0.4 hardware/register references

- STMicroelectronics RM0008, STM32F101/102/103/105/107 reference manual: ADC injected external trigger table maps JEXTSEL=001 to TIM1_CC4.
- Vicy-DE/Ninebot-G30-Custom `boards/esc-motor/PINOUT.md`: G30 STM32F103 bridge mapping PA8/PA9/PA10 and PB13/PB14/PB15, current sense PA3/PA4/PA5, PB1 motor/gate-driver enable, PA11 power hold.
- VESC firmware observer architecture was used as a conceptual reference for the sensorless observer path. v0.4 still does not apply observer output to the bridge.
