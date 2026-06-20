import os
d = r'D:\Learning_Programing\New-NYPC\tournament_logs'
for f in sorted(os.listdir(d)):
    if f.endswith('.txt'):
        fp = os.path.join(d, f)
        with open(fp) as fh:
            lines = fh.readlines()
        gc = sum(1 for l in lines if '# GAME ' in l)
        sc = sum(1 for l in lines if 'SCORECORDYCEPS' in l)
        w = os.path.getsize(fp) // 1000
        last = lines[-1].strip()[:60] if lines else ''
        print(f'{f}: {w}KB, {gc} games, {sc} complete, last={last}')
