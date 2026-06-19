import glob

logs = sorted(glob.glob(r'D:\Learning_Programing\New-NYPC\logs\2*'))
print(f"Found {len(logs)} files:", [l.split('\\')[-1] for l in logs])

for f in logs:
    data = open(f).read()
    lines = data.strip().split('\n')
    moves = [l.split() for l in lines if l.startswith(('FIRST ', 'SECOND '))]
    if not moves:
        print(f"\n--- {f.split(chr(92))[-1]}: no moves ---")
        continue
    
    first_times = [int(m[-1]) for m in moves if m[0] == 'FIRST']
    second_times = [int(m[-1]) for m in moves if m[0] == 'SECOND']
    
    first_score = second_score = None
    for l in lines:
        if l.startswith('SCOREFIRST'): first_score = l.split()[1]
        if l.startswith('SCORESECOND'): second_score = l.split()[1]
    
    name = f.split(chr(92))[-1]
    print(f"\n--- {name} ---")
    print(f"  SCORE: FIRST={first_score} SECOND={second_score}")
    print(f"  Moves: FIRST={len(first_times)}, SECOND={len(second_times)}")
    
    for label, times in [('FIRST', first_times), ('SECOND', second_times)]:
        if not times: continue
        mean = sum(times) / len(times)
        n = len(times)
        early = sum(times[:n//3])/(n//3) if n >= 3 else mean
        mid = sum(times[n//3:2*n//3])/(n//3) if n >= 6 else mean
        late = sum(times[-(n//3):])/(n//3) if n >= 3 else mean
        print(f"  {label}: avg={mean:.0f}ms early={early:.0f}ms mid={mid:.0f}ms late={late:.0f}ms max={max(times)}ms")
    
    first_pass = sum(1 for m in moves if m[0]=='FIRST' and m[1]=='-1')
    second_pass = sum(1 for m in moves if m[0]=='SECOND' and m[1]=='-1')
    print(f"  PASS: FIRST={first_pass} SECOND={second_pass}")
    print()
