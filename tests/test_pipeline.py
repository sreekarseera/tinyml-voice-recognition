import math
from pathlib import Path
import tempfile
import unittest
import struct
import subprocess
from tools.energy import integrate
from tools.train import build_extractor,read_manifest,header

class PipelineTests(unittest.TestCase):
    def test_energy_units(self):
        r=integrate([{'time_s':0,'current_ma':50},{'time_s':.01,'current_ma':50}],3.3,10)
        self.assertAlmostEqual(r['total_energy_mj'],1.65)
        self.assertAlmostEqual(r['baseline_subtracted_energy_mj'],1.32)
    def test_irregular_timestamps(self):
        r=integrate([{'time_s':0,'current_ma':10},{'time_s':.001,'current_ma':20},{'time_s':.01,'current_ma':20}],1)
        self.assertAlmostEqual(r['total_energy_mj'],.195)
    def test_invalid_energy_rejected(self):
        for rows in [[{'time_s':0,'current_ma':10}]*2,[{'time_s':0,'current_ma':10},{'time_s':1,'current_ma':float('nan')}]]:
            with self.assertRaises(ValueError):integrate(rows,3.3)
    def test_shared_mfcc_code_on_silence_and_tone(self):
        with tempfile.TemporaryDirectory() as d:
            exe=Path(d)/'features';build_extractor(exe)
            values=[]
            for tone in [False,True]:
                pcm=struct.pack('<16000f',*(.25*math.sin(2*math.pi*440*n/16000) if tone else 0 for n in range(16000)))
                r=subprocess.run([str(exe)],input=pcm,capture_output=True,check=True)
                features=list(map(float,r.stdout.decode().split(',')))
                self.assertEqual(len(features),26);self.assertTrue(all(math.isfinite(x) for x in features));values.append(features)
            self.assertNotEqual(values[0],values[1])
    def test_exported_int8_weights_match_shared_cpp_classifier(self):
        model={'labels':['off','on'],'mean':[float(j) for j in range(26)],'inv_std':[.5]*26,
               'weights':[list(range(-13,13)),list(range(12,-14,-1))],
               'weight_scale':.01,'bias':[.1,-.2]}
        values=[1+.2*j for j in range(26)]
        root=Path(__file__).resolve().parents[1]
        with tempfile.TemporaryDirectory() as d:
            p=Path(d);(p/'trained_model.h').write_text(header(model,'synthetic'))
            exe=p/'predict'
            subprocess.run(['c++','-O2','-std=c++17','-I',str(p),'-I',str(root/'include'),str(root/'tools/predict.cpp'),'-o',str(exe)],check=True)
            result=subprocess.run([str(exe)],input=' '.join(map(str,values)).encode(),capture_output=True,check=True)
            actual=list(map(float,result.stdout.decode().split(',')))
            for c in range(2):
                expected=model['bias'][c]+sum(model['weights'][c][j]*.01*(values[j]-j)*.5 for j in range(26))
                self.assertAlmostEqual(actual[c],expected,places=4)
    def test_duplicate_content_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d);(p/'same.wav').write_bytes(b'same')
            (p/'manifest.csv').write_text('path,label,speaker,split\nsame.wav,on,a,train\nsame.wav,on,b,test\n')
            with self.assertRaisesRegex(ValueError,'Duplicate'):read_manifest(p/'manifest.csv')

if __name__=='__main__':unittest.main()
