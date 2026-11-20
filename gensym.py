#!/usr/bin/env python3
import bisect, re, subprocess, sys

ELF = "kernel/kernel"
prefix = sys.argv[1] if len(sys.argv) > 1 and sys.argv[1] else "riscv64-linux-gnu-"
empty = "empty" in sys.argv[2:]

def write(entries):
    with open("kernel/symbols.c", "w") as f:
        f.write('#include "types.h"\n#include "symbols.h"\n\n')
        f.write("struct symbol_entry ksymbols[] = {\n")
        for a, fn, fl in entries:
            f.write(f'  {{ 0x{a:x}ULL, "{fn}", "{fl}" }},\n')
        f.write("  { 0, 0, 0 }\n};\n")          # sentinel keeps array non-empty
        f.write(f"int nksymbols = {len(entries)};\n")

if empty:
    write([])
    sys.exit(0)

def run(cmd):
    return subprocess.check_output(cmd).decode()

# function start addresses from nm
funcs = []
for line in run([prefix + "nm", "-n", ELF]).splitlines():
    p = line.split()
    if len(p) == 3 and p[1] in ("T", "t"):
        funcs.append((int(p[0], 16), p[2]))
starts = [a for a, _ in funcs]

# return addresses = address right after every call instruction
rets = []
pat = re.compile(r"^\s*([0-9a-f]+):\s+([0-9a-f]+)\s+(?:c\.)?jalr?\b")
for line in run([prefix + "objdump", "-d", ELF]).splitlines():
    m = pat.match(line)
    if m:
        rets.append(int(m.group(1), 16) + len(m.group(2)) // 2)

# file:line exactly as addr2line reports for the printed return address
inp = "\n".join(hex(r) for r in rets) + "\n"
out = subprocess.run([prefix + "addr2line", "-e", ELF], input=inp.encode(),
                     stdout=subprocess.PIPE, check=True).stdout.decode().splitlines()

entries = []
for r, loc in zip(rets, out):
    # function name: use r-1 so a call that is the last instruction of a
    # function is not attributed to the next function
    i = bisect.bisect_right(starts, r - 1) - 1
    if i < 0:
        continue
    loc = loc.split(" (discriminator")[0]       # strip "(discriminator N)"
    if "kernel/" in loc:
        loc = "kernel/" + loc.split("kernel/")[-1]
    entries.append((r, funcs[i][1], loc))
entries.sort()
write(entries)