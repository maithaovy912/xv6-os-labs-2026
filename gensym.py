#!/usr/bin/env python3
import subprocess
import os

def main():
    kernel_elf = "kernel/kernel"
    if not os.path.exists(kernel_elf):
        return

    # 1. Extract function addresses and names using nm
    nm_cmd = "riscv64-linux-gnu-nm"
    try:
        subprocess.check_output([nm_cmd, "--version"])
    except FileNotFoundError:
        nm_cmd = "riscv64-unknown-elf-nm"

    nm_out = subprocess.check_output([nm_cmd, "-n", kernel_elf]).decode('utf-8')

    funcs = []
    for line in nm_out.splitlines():
        parts = line.split()
        if len(parts) == 3 and parts[1] in ['T', 't']:
            addr = int(parts[0], 16)
            name = parts[2]
            funcs.append((addr, name))

    # 2. Retrieve file names and line numbers using addr2line
    a2l_cmd = nm_cmd.replace("nm", "addr2line")
    addrs = [hex(f[0]) for f in funcs]
    
    a2l_out = subprocess.check_output([a2l_cmd, "-e", kernel_elf] + addrs).decode('utf-8').splitlines()

    # 3. Write generated symbol data to kernel/symbols.c
    with open("kernel/symbols.c", "w") as f:
        f.write('#include "types.h"\n')
        f.write('#include "symbols.h"\n\n')
        f.write('struct symbol_entry ksymbols[] = {\n')
        
        for i, (addr, name) in enumerate(funcs):
            file_line = a2l_out[i] if i < len(a2l_out) else "unknown:0"
            if "kernel/" in file_line:
                file_line = "kernel/" + file_line.split("kernel/")[-1]
            f.write(f'  {{ 0x{addr:x}ULL, "{name}", "{file_line}" }},\n')
            
        f.write('};\n\n')
        f.write(f'int nksymbols = {len(funcs)};\n')

if __name__ == "__main__":
    main()