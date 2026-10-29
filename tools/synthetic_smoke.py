"""Create synthetic tones to exercise training/builds, never speech evidence."""
import csv
import math
import random
import struct
import wave
from pathlib import Path
p=Path('output/synthetic');p.mkdir(parents=True,exist_ok=True)
with (p/'manifest.csv').open('w',newline='') as f:
    writer=csv.writer(f);writer.writerow(['path','label','speaker','split'])
    for si,split in enumerate(['train','validation','test']):
        for label,freq in [('tone_low',350),('tone_high',1800)]:
            rng=random.Random(si*100+freq)
            samples=[int(7000*math.sin(2*math.pi*freq*n/16000)+rng.uniform(-100,100)) for n in range(16000)]
            name=f'{split}_{label}.wav'
            with wave.open(str(p/name),'wb') as w:
                w.setparams((1,2,16000,0,'NONE','not compressed'));w.writeframes(struct.pack('<16000h',*samples))
            writer.writerow([name,label,f'synthetic-group-{si}',split])
print(p/'manifest.csv')
