"""Integrate exported PPK2 current samples, in explicit seconds and milliamps."""
import argparse
import csv
import json
import math

def integrate(rows,voltage,baseline_ma=0):
    if len(rows)<2:raise ValueError('At least two samples are required')
    if not math.isfinite(voltage) or voltage<=0 or not math.isfinite(baseline_ma) or baseline_ma<0:
        raise ValueError('Invalid supply voltage or baseline')
    values=[(float(r['time_s']),float(r['current_ma'])) for r in rows]
    if any(not math.isfinite(t) or not math.isfinite(i) or i<0 for t,i in values):raise ValueError('Invalid current/time')
    if any(b[0]<=a[0] for a,b in zip(values,values[1:])):raise ValueError('Time must strictly increase')
    # V * mA * s = mJ. Integrate actual timestamp intervals, not an assumed rate.
    integral=sum((b[0]-a[0])*(a[1]+b[1])/2 for a,b in zip(values,values[1:]))
    duration=values[-1][0]-values[0][0]
    return {'duration_s':duration,'total_energy_mj':voltage*integral,
            'baseline_subtracted_energy_mj':voltage*(integral-baseline_ma*duration),
            'mean_current_ma':integral/duration,'supply_v':voltage,'baseline_ma':baseline_ma}

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('csv');p.add_argument('--voltage',type=float,required=True)
    p.add_argument('--baseline-ma',type=float,default=0);a=p.parse_args()
    try:
        with open(a.csv,newline='') as f:result=integrate(list(csv.DictReader(f)),a.voltage,a.baseline_ma)
    except (ValueError,KeyError,OSError) as e:p.error(str(e))
    print(json.dumps(result,indent=2,allow_nan=False))
if __name__=='__main__':main()
