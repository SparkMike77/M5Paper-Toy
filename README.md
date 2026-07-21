# M5Paper-Toy

Custom firmware for the M5Paper dev kit v1.1 — ebook reader, notepad, smarthome remote,
and whatever else fits.

## Built on / inspired by

- [tsndr/M5PaperOS](https://github.com/tsndr/M5PaperOS) — base OS shell (MIT)
- [m5stack/M5Paper_FactoryTest](https://github.com/m5stack/M5Paper_FactoryTest) — what
  M5PaperOS itself builds on (MIT)
- [atomic14/diy-esp32-epub-reader](https://github.com/atomic14/diy-esp32-epub-reader) —
  EPUB reader reference
- [juicecultus/EPub-M5Stack-Paper-S3](https://github.com/juicecultus/EPub-M5Stack-Paper-S3) —
  WiFi SD-card file transfer reference
- [dandwhelan/s3paper-fossi-util](https://github.com/dandwhelan/s3paper-fossi-util) —
  calculator/timer/notes/EPUB reader reference

## Building

PlatformIO (`pio run`). See `platformio.ini` for the board definition and upload port notes.
