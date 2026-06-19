import glob

def analyze_log(f):
    data = open(f).read()
    lines = data.strip().split('\n')
    moves = [l.split() for l in lines if l.startswith(('FIRST ', 'SECOND '))]
    if not moves: return None
    
    first_times = [(i, int(m[-1])) for i, m in enumerate(moves) if m[0] == 'FIRST']
    second_times = [(i, int(m[-1])) for i, m in enumerate(moves) if m[0] == 'SECOND']
    
    first_score = second_score = None
    for l in lines:
        if l.startswith('SCOREFIRST'): first_score = int(l.split()[1])
        if l.startswith('SCORESECOND'): second_score = int(l.split()[1])
    if first_score is None: return None
    
    # Calculate % of remaining time used per move
    # BTC default total: 10000ms per player
    # The TIME message gives remaining, and time spent = prev_remaining - new_remaining
    # But we only have the time spent per move from the log
    
    def calc_pct(times):
        """Calculate % of remaining used per move starting from 10000"""
        remaining = 10000
        pcts = []
        for _, t in times:
            if remaining > 0 and t > 0:
                pct = (t / remaining) * 100
                pcts.append(pct)
            remaining -= t
            if remaining < 0: remaining = 0
        return pcts
    
    f_pcts = calc_pct(first_times)
    s_pcts = calc_pct(second_times)
    f_total = sum(t for _, t in first_times)
    s_total = sum(t for _, t in second_times)
    
    return {
        'file': f.split('\\')[-1],
        'first_times': [t for _, t in first_times],
        'second_times': [t for _, t in second_times],
        'first_pcts': f_pcts,
        'second_pcts': s_pcts,
        'first_total': f_total,
        'second_total': s_total,
        'first_total_pct': f_total / 10000 * 100,
        'second_total_pct': s_total / 10000 * 100,
        'first_score': first_score,
        'second_score': second_score,
        'first_won': first_score > second_score,
    }

# Analyze all logs
results = []
for f in sorted(glob.glob(r'D:\Learning_Programing\New-NYPC\logs\*.txt')):
    r = analyze_log(f)
    if r: results.append(r)

print(f"=== TIME ANALYSIS: {len(results)} GAMES ===\n")

# Average % per move for different categories
for label, filt, key in [
    ("ALL FIRST", lambda r: True, 'first_pcts'),
    ("ALL SECOND", lambda r: True, 'second_pcts'),
    ("WINNING FIRST", lambda r: r['first_won'], 'first_pcts'),
    ("LOSING SECOND", lambda r: not r['first_won'], 'second_pcts'),
    ("STRONG FIRST (avg>200ms)", lambda r: sum(r['first_times'])/len(r['first_times']) > 200, 'first_pcts'),
    ("STRONG SECOND (avg>200ms)", lambda r: sum(r['second_times'])/len(r['second_times']) > 200, 'second_pcts'),
]:
    all_pcts = []
    all_times = []
    all_totals = []
    for r in results:
        if filt(r):
            all_pcts.extend(r[key])
            all_times.extend(r[key.replace('_pcts', '_times')])
            name = key.replace('_pcts', '_total')
            all_totals.append(r[name])
    
    if all_pcts:
        print(f"  {label}:")
        n_moves = len(all_pcts)
        avg_pct = sum(all_pcts) / n_moves
        avg_time = sum(all_times) / len(all_times)
        avg_total_pct = sum(all_totals) / len(all_totals)
        print(f"    {n_moves} moves, avg {avg_time:.0f}ms/move = {avg_pct:.2f}% of remaining/move")
        print(f"    Total: {avg_total_pct:.1f}% of 10000ms pool")

print(f"\n=== TOP ENGINES (WINNING, avg>150ms) PER-MOVE % ===")
for label, key in [("FIRST", 'first_pcts'), ("SECOND", 'second_pcts')]:
    winners = [r for r in results if 
               (label == 'FIRST' and r['first_won'] and sum(r['first_times'])/len(r['first_times']) > 150) or
               (label == 'SECOND' and not r['first_won'] and sum(r['second_times'])/len(r['second_times']) > 150)]
    
    if not winners: continue
    all_pcts = []
    for r in winners:
        all_pcts.extend(r[key])
    
    avg_pct = sum(all_pcts) / len(all_pcts)
    
    # Show move-by-move breakdown for the best game
    best_game = max(winners, key=lambda r: max(r['first_score'], r['second_score']) 
                    if label == 'FIRST' else max(r['second_score'], r['first_score']))
    
    print(f"\n  {label} WINNERS ({len(winners)} games):")
    print(f"    Overall: {len(all_pcts)} moves, avg {avg_pct:.2f}%/move")
    
    # Show the best game in detail
    if label == 'FIRST':
        best_times = best_game['first_times']
        best_pcts = best_game['first_pcts']
    else:
        best_times = best_game['second_times']
        best_pcts = best_game['second_pcts']
    
    print(f"\n    Best game: {best_game['file']} (F={best_game['first_score']} S={best_game['second_score']})")
    print(f"    Move | Time | %Remaining | Cumulative")
    remaining = 10000
    for i, (t, p) in enumerate(zip(best_times, best_pcts)):
        remaining -= t
        print(f"    {i+1:4d} | {t:4d}ms | {p:6.2f}% | {remaining:5d}ms")

print(f"\n=== PHASE PATTERNS (WINNING BOTS) ===")
# Track % spent by move number
for label, key in [("FIRST", 'first_pcts'), ("SECOND", 'second_pcts')]:
    winners = [r for r in results if 
               (label == 'FIRST' and r['first_won'] and sum(r['first_times'])/len(r['first_times']) > 150) or
               (label == 'SECOND' and not r['first_won'] and sum(r['second_times'])/len(r['second_times']) > 150)]
    
    if not winners: continue
    
    # Group by move number quartile
    early_pcts = []
    mid_pcts = []
    late_pcts = []
    end_pcts = []
    
    for r in winners:
        pcts = r[key]
        n = len(pcts)
        if n < 4: continue
        q = n // 4
        early_pcts.extend(pcts[:q])
        mid_pcts.extend(pcts[q:2*q])
        late_pcts.extend(pcts[2*q:3*q])
        end_pcts.extend(pcts[3*q:])
    
    if early_pcts:
        print(f"\n  {label} WINNERS:")
        print(f"    Early  (1st quarter, avg {len(early_pcts)//len(winners)} moves): {sum(early_pcts)/len(early_pcts):.2f}%/move")
        print(f"    Mid    (2nd quarter): {sum(mid_pcts)/len(mid_pcts):.2f}%/move")
        print(f"    Mid-late (3rd quarter): {sum(late_pcts)/len(late_pcts):.2f}%/move")
        print(f"    End    (4th quarter): {sum(end_pcts)/len(end_pcts):.2f}%/move")

print(f"\n=== RECOMMENDED BUDGET % ===")
# Average % of remaining per move for strong bots
strong_first_pcts = []
strong_second_pcts = []
for r in results:
    if r['first_won'] and sum(r['first_times'])/len(r['first_times']) > 150:
        strong_first_pcts.extend(r['first_pcts'])
    if not r['first_won'] and sum(r['second_times'])/len(r['second_times']) > 150:
        strong_second_pcts.extend(r['second_pcts'])

if strong_first_pcts:
    f_pct = sum(strong_first_pcts)/len(strong_first_pcts)
    print(f"  FIRST winning bots: {f_pct:.2f}%/move → pool = ~{(f_pct * 22):.0f}% of total (~{(f_pct * 22 / 100 * 10000):.0f}ms/22 moves)")
if strong_second_pcts:
    s_pct = sum(strong_second_pcts)/len(strong_second_pcts)
    print(f"  SECOND winning bots: {s_pct:.2f}%/move → pool = ~{(s_pct * 17):.0f}% of total (~{(s_pct * 17 / 100 * 10000):.0f}ms/17 moves)")
