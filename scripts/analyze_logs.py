import os
import glob
from collections import defaultdict

logs_dir = r'D:\Learning_Programing\New-NYPC\logs'
log_files = sorted(glob.glob(os.path.join(logs_dir, '*.txt')))

for f in log_files[:8]:
    data = open(f).read()
    lines = data.strip().split('\n')
    moves = [l.split() for l in lines if l.startswith(('FIRST ', 'SECOND '))]
    if not moves:
        print(f"--- {os.path.basename(f)}: no moves ---")
        continue

    first_times = [int(m[-1]) for m in moves if m[0] == 'FIRST']
    second_times = [int(m[-1]) for m in moves if m[0] == 'SECOND']

    first_score = None
    second_score = None
    for l in lines:
        if l.startswith('SCOREFIRST'): first_score = l.split()[1]
        if l.startswith('SCORESECOND'): second_score = l.split()[1]

    total_first_moves = len(first_times)
    total_second_moves = len(second_times)

    print(f"--- {os.path.basename(f)} ---")
    print(f"  SCORE: FIRST={first_score} SECOND={second_score}")
    print(f"  Moves: FIRST={total_first_moves}, SECOND={total_second_moves}")

    # Analyze FIRST time in quartiles of total moves
    for label, times in [("FIRST", first_times), ("SECOND", second_times)]:
        if not times:
            print(f"  {label}: no time data")
            continue
        print(f"  {label} ({len(times)} moves):")
        mean = sum(times) / len(times)
        print(f"    overall mean={mean:.0f}ms, min={min(times)}ms, max={max(times)}ms")

        # Show time trend: first move, early, middle, late, final 3
        n = len(times)
        if n >= 5:
            chunks = [
                ("move 1", times[:1]),
                ("moves 2-4", times[1:4] if n > 4 else times[1:]),
                ("mid (25-50%)", times[n//4:n//2] if n >= 8 else []),
                ("mid (50-75%)", times[n//2:3*n//4] if n >= 8 else []),
                ("last 5 moves", times[-5:] if n >= 8 else times[-(n//2):] if n >= 4 else times),
            ]
            for chunk_label, chunk in chunks:
                if chunk:
                    c_mean = sum(chunk) / len(chunk)
                    val_str = str(chunk) if len(chunk) <= 5 else str(chunk[:3]) + f"...({len(chunk)}vals)"
                    print(f"      {chunk_label}: mean={c_mean:.0f}ms {val_str}")

    # Count PASS moves
    first_pass = sum(1 for m in moves if m[0] == 'FIRST' and m[1] == '-1')
    second_pass = sum(1 for m in moves if m[0] == 'SECOND' and m[1] == '-1')
    print(f"  PASS moves: FIRST={first_pass} SECOND={second_pass}")
    print()

    # Estimate: each move harvests 10 score points (rect sum=10)
    # So live cells at each point ≈ initial - my_score - opp_score
    # But we don't know initial — just track per-move score progression
    if total_first_moves > 5 and total_second_moves > 5:
        # Approximate live cells: if rect sum=10 and avg cell value=5, each rect ≈ 2 cells
        # But also not all cells have mushrooms initially
        # Let's just track move count as a proxy
        pass

