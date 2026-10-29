"""Train a weight-quantized MFCC linear classifier from a speaker-separated manifest."""
import argparse
import csv
import hashlib
import json
import math
import re
import struct
import subprocess
import wave
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]


def build_extractor(output):
    subprocess.run(['c++','-O2','-std=c++17','-I',str(ROOT/'include'),str(ROOT/'tools/extract_features.cpp'),'-o',str(output)],check=True)


def wav_features(path,extractor):
    with wave.open(str(path),'rb') as w:
        if (w.getframerate(),w.getnchannels(),w.getsampwidth(),w.getnframes())!=(16000,1,2,16000):
            raise ValueError(f'{path}: expected exactly 1 second, 16 kHz, mono, 16-bit PCM')
        samples=struct.unpack('<16000h',w.readframes(16000))
    pcm=struct.pack('<16000f',*(s/32768 for s in samples))
    r=subprocess.run([str(extractor)],input=pcm,capture_output=True,check=True)
    return [float(v) for v in r.stdout.decode().strip().split(',')]


def read_manifest(path):
    with Path(path).open(newline='') as f:
        records=list(csv.DictReader(f))
    if not records:raise ValueError('Manifest has no recordings')
    speakers={s:set() for s in ('train','validation','test')}
    hashes=set()
    for r in records:
        if r['split'] not in speakers or not r['speaker'].strip():raise ValueError('Invalid split or missing speaker')
        if not re.fullmatch(r'[A-Za-z0-9_-]{1,32}',r['label']):raise ValueError('Labels must be short ASCII identifiers')
        r['file']=(Path(path).resolve().parent/r['path']).resolve()
        digest=hashlib.sha256(r['file'].read_bytes()).hexdigest()
        if digest in hashes:raise ValueError('Duplicate audio content in dataset')
        hashes.add(digest);speakers[r['split']].add(r['speaker'])
    if any(not s for s in speakers.values()):raise ValueError('Train, validation and test splits are all required')
    if any(speakers[a]&speakers[b] for a,b in [('train','validation'),('train','test'),('validation','test')]):
        raise ValueError('Speaker leakage across splits')
    labels=sorted({r['label'] for r in records if r['split']=='train'})
    if len(labels)<2:raise ValueError('At least two training classes required')
    for split in speakers:
        if {r['label'] for r in records if r['split']==split}!=set(labels):raise ValueError('Every split must contain every class')
    return records,labels


def softmax(scores):
    e=[math.exp(x-max(scores)) for x in scores];return [x/sum(e) for x in e]


def train(records,labels,extractor,epochs=150):
    if not 1<=epochs<=10000:raise ValueError('epochs must be between 1 and 10000')
    rows=[(wav_features(r['file'],extractor),labels.index(r['label']),r['split']) for r in records]
    training=[(x,y) for x,y,s in rows if s=='train']
    mean=[sum(x[j] for x,y in training)/len(training) for j in range(26)]
    inv=[1/max(math.sqrt(sum((x[j]-mean[j])**2 for x,y in training)/len(training)),1e-6) for j in range(26)]
    rows=[([(x[j]-mean[j])*inv[j] for j in range(26)],y,s) for x,y,s in rows]
    w=[[0.0]*26 for _ in labels];bias=[0.0]*len(labels)
    for epoch in range(epochs):
        dw=[[0.0]*26 for _ in labels];db=[0.0]*len(labels)
        for x,y,s in rows:
            if s!='train':continue
            probs=softmax([sum(a*b for a,b in zip(wc,x))+bc for wc,bc in zip(w,bias)])
            for c in range(len(labels)):
                error=probs[c]-(c==y);db[c]+=error
                for j in range(26):dw[c][j]+=error*x[j]
        for c in range(len(labels)):
            bias[c]-=.05*db[c]/len(training)
            for j in range(26):w[c][j]-=.05*(dw[c][j]/len(training)+.001*w[c][j])
    scale=max(max(map(abs,wc)) for wc in w)/127 or 1
    quant=[[max(-127,min(127,round(v/scale))) for v in wc] for wc in w]
    evaluation={}
    for split in ('train','validation','test'):
        confusion=[[0]*len(labels) for _ in labels]
        for x,y,s in rows:
            if s!=split:continue
            scores=[sum(q*scale*v for q,v in zip(qc,x))+b for qc,b in zip(quant,bias)]
            guess=max(range(len(labels)),key=scores.__getitem__);confusion[y][guess]+=1
        total=sum(map(sum,confusion))
        evaluation[split]={'samples':total,'accuracy':sum(confusion[c][c] for c in range(len(labels)))/total,'confusion_matrix':confusion}
    return {'labels':labels,'mean':mean,'inv_std':inv,'weights':quant,'weight_scale':scale,'bias':bias,'evaluation':evaluation}


def header(model,data_kind="unspecified"):
    def floats(xs):return '{'+','.join(f'{v:.9e}f' for v in xs)+'}'
    weights='{'+','.join('{'+','.join(map(str,w))+'}' for w in model['weights'])+'}'
    return ('// Generated from the manifest passed to tools/train.py.\n#pragma once\n#include <cstdint>\n'
            +'const char* const DatasetKind='+json.dumps(data_kind)+';\n'
            +f'constexpr int Classes={len(model["labels"])};\n'
            +'const char* const Labels[Classes]={'+','.join(json.dumps(s) for s in model['labels'])+'};\n'
            +'const float FeatureMean[26]='+floats(model['mean'])+';\n'
            +'const float FeatureInvStd[26]='+floats(model['inv_std'])+';\n'
            +'const int8_t Weights[Classes][26]='+weights+';\n'
            +f'constexpr float WeightScale={model["weight_scale"]:.9e}f;\n'
            +'const float Bias[Classes]='+floats(model['bias'])+';\n')


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('manifest');p.add_argument('--epochs',type=int,default=150)
    p.add_argument('--output',default='include/trained_model.h');p.add_argument('--data-kind',choices=['recorded','synthetic'],required=True)
    a=p.parse_args();work=ROOT/'output';work.mkdir(exist_ok=True)
    try:
        records,labels=read_manifest(a.manifest);extractor=work/'extract_features';build_extractor(extractor)
        model=train(records,labels,extractor,a.epochs)
    except (ValueError,OSError,KeyError,subprocess.CalledProcessError) as e:p.error(str(e))
    out=Path(a.output);out.parent.mkdir(parents=True,exist_ok=True);out.write_text(header(model,a.data_kind))
    report={'architecture':'26 MFCC mean/std features, linear softmax, int8 weights and float DSP/activation',
            'prepared_by':'AI-assisted current implementation','data_kind':a.data_kind,
            'manifest_sha256':hashlib.sha256(Path(a.manifest).read_bytes()).hexdigest(),
            'audio_sha256':[hashlib.sha256(r['file'].read_bytes()).hexdigest() for r in records],**model}
    (work/'training-report.json').write_text(json.dumps(report,indent=2,allow_nan=False)+'\n')
    print(json.dumps({'data_kind':a.data_kind,'evaluation':model['evaluation']},indent=2))

if __name__=='__main__':main()
