# Verification — 29 October 2026

6 automated tests passed locally using Python 3.14.6:

    python3 -m unittest discover -s tests -v

Host tests checked MFCC execution, exported quantized model/C++ parity, energy integration units, irregular timestamps and duplicate-content rejection. The complete training pipeline also ran on six synthetic tone recordings across three disjoint synthetic groups. This is not evidence of speech accuracy.

No physical hardware validation or new instrument measurement is claimed.

ESP32 target compilation was not completed: installing the PlatformIO toolchain
exceeded free disk space on the Mac. The build was stopped and its temporary
package cache removed. The configured GitHub Actions firmware job has not been
run. Passing desktop tests does not prove the ESP32 build or physical operation.

## GitHub verification after publication

Initial Linux CI exposed a collision between project features.h and the system
header of that name. The project header is now voice_features.h. GitHub Actions
run 40865896610 passed all six tests and built the new ESP32-S3 baseline using a
model trained on synthetic tones. This supersedes the earlier target-build
limitation above. The supplied Edge Impulse sketches/library were not the targets
of that build, and synthetic-tone training does not establish speech accuracy.

https://github.com/sreekarseera/tinyml-voice-recognition/actions/runs/40865896610

Compilation is not a physical microphone/board/power-profiler test. No measured
energy or voice performance is claimed.
