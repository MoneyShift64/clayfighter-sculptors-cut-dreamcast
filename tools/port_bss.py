#!/usr/bin/env python3
"""asm/bss.s for the ports: the same labels and sizes (SYM() adds SH-4's leading underscore)."""
import re, sys
out = ["/* made by tools/port_bss.py from asm/bss.s */", "#ifdef __sh__", "#define SYM(x) _##x",
       "#else", "#define SYM(x) x", "#endif", ".section .bss", ".balign 16"]
for line in open(sys.argv[1]):
    line = line.rstrip()
    m = re.match(r"\.global (\w+)$", line)
    if m: out.append(f".global SYM({m.group(1)})"); continue
    m = re.match(r"(\w+):$", line)
    if m: out.append(f"SYM({m.group(1)}):"); continue
    m = re.match(r"\s*\.space (\w+)$", line)
    if m: out.append(f"    .space {m.group(1)}"); continue
open(sys.argv[2], "w").write("\n".join(out) + "\n")
