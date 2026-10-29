# TinyML Voice Recognition

Two source tracks are kept distinct:

- originals/: supplied microphone and Edge Impulse inference sketches, unmodified.
- src/, include/, tools/: new, reproducible MFCC classifier and energy-analysis
  tools prepared on 29 October 2026. This is a linear baseline, not the original
  report's 1D CNN. It makes no claim to reproduce the reported speech accuracy.

## Train and build the new baseline

Python 3.11+, a C++17 compiler and PlatformIO Core 6.1.18 are needed.
No Python packages are needed. Supply your own consented audio recordings:
exactly one second of mono 16-bit PCM WAV at 16 kHz. Manifest columns:
path,label,speaker,split. Splits are train, validation and test; all must include
every class. Speakers must not cross splits and duplicate audio is rejected.

    python3 tools/train.py recordings/manifest.csv --data-kind recorded
    pio run
    pio run -t upload
    pio device monitor
    python3 -m unittest discover -s tests -v

The portable C++ MFCC implementation is shared by desktop feature extraction
and ESP32-S3 firmware. It computes 13 MFCC means and 13 standard deviations from
20ms frames and trains softmax linear weights. Weights are quantized to int8;
DSP and activations remain floating point. Speaker-separated validation/test
reports and a dataset fingerprint are generated in output/.

For a software-only smoke run without private speech recordings:

    python3 tools/synthetic_smoke.py
    python3 tools/train.py output/synthetic/manifest.csv --data-kind synthetic
    pio run

This trains two tone classes, not speech. Its accuracy is only a software smoke
check and must never be reported as voice recognition performance. A model must
be generated before the firmware can build; none is silently supplied as trained
speech. Delete the synthetic header and retrain before a voice demonstration.

## Hardware and energy

ESP32-S3 DevKitC-1; INMP441 at 3.3V: SCK=14, WS=15, SD=16, GND common, L/R
selects left channel. GPIO4 brackets MFCC extraction; GPIO5 brackets linear
scoring. Wire only supported voltage levels and confirm your board pinout.
Capture, DSP and scoring times are reported separately. No 100ms artificial
profiling pulse is added. The original supplied sketch retains its debug delay
in originals/ so the change can be inspected.

    python3 tools/energy.py selected-trace.csv --voltage 3.3 --baseline-ma 0

CSV columns are time_s,current_ma. Select an actual GPIO-bracketed interval in
the power profiler before export. Total and baseline-subtracted energy are
separate; acquisition, DSP and NN windows must not be confused. No measured
energy, accuracy, battery-life or physical hardware result is claimed here.

## Supplied Edge Impulse library

The source archive contains an exported Edge Impulse SDK/model library. It is
not copied into this repository: its embedded header contains subscription and
licensing restrictions. A public redistribution right has not been established.
The original sketches require that library and are not the default build.
The new baseline has no dependency on it. See docs/attribution.md.
