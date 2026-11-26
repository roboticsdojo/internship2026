"""Turn eval_screen.py output into the numbers that matter for the robot."""
import json
import os
import sys

import numpy as np

sys.path.insert(0, sys.argv[2])
from auracle_vision.waste_model import Voter  # noqa: E402

R = json.load(open(sys.argv[1]))
truth = R['truth']
classes = sorted(set(truth))


def balanced(ok):
    return np.mean([np.mean([o for o, t in zip(ok, truth) if t == c]) for c in classes])


def frame_ok(best, t, thr):
    return best is not None and best[1] >= thr and best[0] == t


def vote(seq, window, k, score, thr):
    v = Voter(window, k, score)
    for i, b in enumerate(seq):
        d = v.add(b if b is not None and b[1] >= thr else None)
        if d:
            return d[0], i + 1
    return None, len(seq)


def first_label(seq, thr):
    """What the old runner did: take the first label the detector published (conf >= 0.5)."""
    for b in seq:
        if b is not None and b[1] >= thr:
            return b[0]
    return None


print(f"{'config':32s} {'ms':>5s} {'clean':>6s} | {'frame@.25':>9s} {'none':>5s} | "
      f"{'first@.5':>8s} | {'vote ok':>7s} {'wrong':>5s} {'undec':>5s} {'frames':>6s}")
for name, c in R['configs'].items():
    clean = balanced([frame_ok(b, t, 0.25) for b, t in zip(c['clean'], truth)])
    fr = [[frame_ok(b, t, 0.25) for b in seq] for seq, t in zip(c['sim'], truth)]
    fr_acc = balanced([np.mean(x) for x in fr])
    none = np.mean([b is None or b[1] < 0.25 for seq in c['sim'] for b in seq])
    first = balanced([first_label(seq, 0.5) == t for seq, t in zip(c['sim'], truth)])
    res = [vote(seq, 10, 5, 0.35, 0.25) for seq in c['sim']]
    ok = balanced([d == t for (d, _), t in zip(res, truth)])
    wrong = np.mean([d is not None and d != t for (d, _), t in zip(res, truth)])
    und = np.mean([d is None for d, _ in res])
    nf = np.mean([n for d, n in res if d is not None])
    print(f"{os.path.splitext(name)[0]:32s} {c['ms']:5.1f} {clean:6.1%} | {fr_acc:9.1%} {none:5.1%} | "
          f"{first:8.1%} | {ok:7.1%} {wrong:5.1%} {und:5.1%} {nf:6.1f}")

print('\nper class, 320 area, single frame @0.25:')
c = R['configs']['320 area']
for cl in classes:
    accs = [np.mean([frame_ok(b, t, 0.25) for b in seq]) for seq, t in zip(c['sim'], truth) if t == cl]
    print(f'  {cl:8s} n={len(accs):3d} {np.mean(accs):.1%}')
print('\nvote rules on 320 area (window 10):')
for k, s in [(3, 0.3), (4, 0.35), (5, 0.35), (6, 0.35), (6, 0.45), (7, 0.4)]:
    res = [vote(seq, 10, k, s, 0.25) for seq in c['sim']]
    ok = balanced([d == t for (d, _), t in zip(res, truth)])
    wrong = np.mean([d is not None and d != t for (d, _), t in zip(res, truth)])
    und = np.mean([d is None for d, _ in res])
    print(f'  min_votes {k} min_score {s}: right {ok:.1%}  wrong {wrong:.1%}  undecided {und:.1%}')