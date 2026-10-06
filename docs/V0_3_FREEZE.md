# v0.3 freeze

v0.3 is the passive 4 kHz diagnostic/control-path milestone preceding v0.4.

- all six physical bridge outputs remain disabled;
- control path runs Clarke -> sensorless flux observer -> Park -> PI -> inverse Park -> SVPWM calculations;
- timing is measured with DWT on the STM32F103;
- motor R/L/flux and current scaling are still placeholders;
- no Hall feedback is required;
- ST-Link/SWD is the only supported bring-up path;
- SHU/OTA and 14S voltage scaling are deferred.

The original v0.3 build produced a 3532-byte relocated application image at 0x08001000. Hardware timing still requires measurement on the real controller.
