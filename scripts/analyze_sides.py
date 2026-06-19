import glob

# Collect all games data
games = []
for f in sorted(glob.glob(r'D:\Learning_Programing\New-NYPC\logs\*.txt')):
    data = open(f).read()
    lines = data.strip().split('\n')
    moves = [l.split() for l in lines if l.startswith(('FIRST ', 'SECOND '))]
    if not moves: continue
    
    first_times = [int(m[-1]) for m in moves if m[0] == 'FIRST']
    second_times = [int(m[-1]) for m in moves if m[0] == 'SECOND']
    
    first_score = second_score = None
    for l in lines:
        if l.startswith('SCOREFIRST'): first_score = int(l.split()[1])
        if l.startswith('SCORESECOND'): second_score = int(l.split()[1])
    if first_score is None or second_score is None: continue
    
    games.append({
        'file': f.split('\\')[-1],
        'first_times': first_times,
        'second_times': second_times,
        'first_score': first_score,
        'second_score': second_score,
        'first_moves': len(first_times),
        'second_moves': len(second_times),
    })

print(f"=== PHÂN TÍCH {len(games)} GAMES ===\n")

# 1. FIRST vs SECOND overall
first_avgs = [sum(g['first_times'])/len(g['first_times']) for g in games if g['first_times']]
second_avgs = [sum(g['second_times'])/len(g['second_times']) for g in games if g['second_times']]
print("=== FIRST vs SECOND OVERALL ===")
print(f"FIRST  avg time: {sum(first_avgs)/len(first_avgs):.0f}ms")
print(f"SECOND avg time: {sum(second_avgs)/len(second_avgs):.0f}ms")
print()

# 2. FIRST winning vs losing
print("=== FIRST — WINNING vs LOSING ===")
first_win_avgs = [sum(g['first_times'])/len(g['first_times']) 
                  for g in games if g['first_times'] and g['first_score'] > g['second_score']]
first_lose_avgs = [sum(g['first_times'])/len(g['first_times']) 
                   for g in games if g['first_times'] and g['first_score'] < g['second_score']]
if first_win_avgs:
    print(f"FIRST khi WIN ({first_score}x): {sum(first_win_avgs)/len(first_win_avgs):.0f}ms")
if first_lose_avgs:
    print(f"FIRST khi LOSE: {sum(first_lose_avgs)/len(first_lose_avgs):.0f}ms")
print()

# 3. SECOND winning vs losing
print("=== SECOND — WINNING vs LOSING ===")
second_win_avgs = [sum(g['second_times'])/len(g['second_times']) 
                   for g in games if g['second_times'] and g['second_score'] > g['first_score']]
second_lose_avgs = [sum(g['second_times'])/len(g['second_times']) 
                    for g in games if g['second_times'] and g['second_score'] < g['first_score']]
if second_win_avgs:
    print(f"SECOND khi WIN: {sum(second_win_avgs)/len(second_win_avgs):.0f}ms")
if second_lose_avgs:
    print(f"SECOND khi LOSE: {sum(second_lose_avgs)/len(second_lose_avgs):.0f}ms")
print()

# 4. Time by move number (first 3, middle, last 3)
print("=== TIME BY MOVE QUARTILE ===")
for label, key in [("FIRST", "first_times"), ("SECOND", "second_times")]:
    first3 = []
    mid = []
    last3 = []
    for g in games:
        times = g[key]
        n = len(times)
        if n >= 6:
            first3.append(sum(times[:3])/3)
            mid.append(sum(times[n//3:2*n//3])/(n//3))
            last3.append(sum(times[-3:])/3)
    if first3:
        print(f"{label}: early={sum(first3)/len(first3):.0f}ms mid={sum(mid)/len(mid):.0f}ms end={sum(last3)/len(last3):.0f}ms")
print()

# 5. Bot classification by time pattern
print("=== BOT CLASSIFICATION ===")
# Identify distinct bots based on time patterns
# Series "1" with different game numbers represent different bots/dates
print("Series '1' (FIRST bots):")
for g in games:
    if g['file'].startswith('1'):
        avg = sum(g['first_times'])/len(g['first_times'])
        max_t = max(g['first_times'])
        last_3 = sum(g['first_times'][-3:])/3 if len(g['first_times']) >= 3 else 0
        score_str = f"W{'-'.join(str(x) for x in [g['first_score'], g['second_score']])}"
        print(f"  {g['file']:15s}: {avg:6.0f}ms (max={max_t:5d}ms, last3={last_3:.0f}ms) score={score_str}")

print("\nSeries '1' (SECOND bots):")
for g in games:
    if g['file'].startswith('1'):
        avg = sum(g['second_times'])/len(g['second_times'])
        max_t = max(g['second_times'])
        last_3 = sum(g['second_times'][-3:])/3 if len(g['second_times']) >= 3 else 0
        score_str = f"W{'-'.join(str(x) for x in [g['first_score'], g['second_score']])}"
        print(f"  {g['file']:15s}: {avg:6.0f}ms (max={max_t:5d}ms, last3={last_3:.0f}ms) score={score_str}")

print("\nSeries '2' (FIRST bots):")
for g in games:
    if g['file'].startswith('2'):
        avg = sum(g['first_times'])/len(g['first_times'])
        max_t = max(g['first_times'])
        last_3 = sum(g['first_times'][-3:])/3 if len(g['first_times']) >= 3 else 0
        score_str = f"W{'-'.join(str(x) for x in [g['first_score'], g['second_score']])}"
        print(f"  {g['file']:15s}: {avg:6.0f}ms (max={max_t:5d}ms, last3={last_3:.0f}ms) score={score_str}")

print("\nSeries '2' (SECOND bots):")
for g in games:
    if g['file'].startswith('2'):
        avg = sum(g['second_times'])/len(g['second_times'])
        max_t = max(g['second_times'])
        last_3 = sum(g['second_times'][-3:])/3 if len(g['second_times']) >= 3 else 0
        score_str = f"W{'-'.join(str(x) for x in [g['first_score'], g['second_score']])}"
        print(f"  {g['file']:15s}: {avg:6.0f}ms (max={max_t:5d}ms, last3={last_3:.0f}ms) score={score_str}")

# 6. FIRST win rate
first_wins = sum(1 for g in games if g['first_score'] > g['second_score'])
second_wins = sum(1 for g in games if g['second_score'] > g['first_score'])
print(f"\n=== WIN RATE ===")
print(f"FIRST wins: {first_wins}/{len(games)} ({first_wins/len(games)*100:.0f}%)")
print(f"SECOND wins: {second_wins}/{len(games)} ({second_wins/len(games)*100:.0f}%)")

# 7. Average score
print(f"\n=== AVG SCORE ===")
first_scores = [g['first_score'] for g in games]
second_scores = [g['second_score'] for g in games]
print(f"FIRST avg: {sum(first_scores)/len(first_scores):.0f}")
print(f"SECOND avg: {sum(second_scores)/len(second_scores):.0f}")
print(f"Margin (F-S): {(sum(first_scores)-sum(second_scores))/len(games):.0f}")

# 8. Early game time spent
print(f"\n=== FIRST MOVE TIME ===")
first_first = [g['first_times'][0] if g['first_times'] else 0 for g in games]
second_first = [g['second_times'][0] if g['second_times'] else 0 for g in games]
print(f"FIRST  move 1: avg={sum(first_first)/len(first_first):.0f}ms max={max(first_first)}ms")
print(f"SECOND move 1: avg={sum(second_first)/len(second_first):.0f}ms max={max(second_first)}ms")
