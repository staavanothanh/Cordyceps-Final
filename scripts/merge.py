#!/usr/bin/env python3
"""merge.py — Flatten multi-file src/ → single main.cpp for BTC submission.

Usage:
    python merge.py src/ submission/main.cpp

Rules:
- Reads all .hpp and .cpp files from src/ recursively
- Inlines #include "..." (project headers), preserves #include <...> (stdlib)
- Strips header guards (#ifndef, #define, #endif)
- Deduplicates #include <...>
- Outputs clean single-file compilable with: g++-14 -O3 -std=c++20
"""

import sys
import os
import re
from pathlib import Path
from collections import OrderedDict

HEADER_GUARD_START = re.compile(r'^#ifndef\s+\w+_HPP\s*$')
HEADER_GUARD_DEFINE = re.compile(r'^#define\s+\w+_HPP\s*$')
HEADER_GUARD_END = re.compile(r'^#endif\s*//?\s*\w*\s*$')
PRAGMA_ONCE = re.compile(r'^#pragma\s+once\s*$')
INCLUDE_LOCAL = re.compile(r'^#include\s+"(.+)"\s*$')
INCLUDE_SYSTEM = re.compile(r'^#include\s+<(.+)>\s*$')

def read_file(path):
    with open(path, 'r', encoding='utf-8') as f:
        return f.readlines()

def is_header_guard_line(line):
    return (HEADER_GUARD_START.match(line) or 
            HEADER_GUARD_DEFINE.match(line) or 
            HEADER_GUARD_END.match(line) or
            PRAGMA_ONCE.match(line))

def resolve_include(include_path, src_dir, current_file):
    """Resolve a local include relative to src_dir or current file dir"""
    # Try relative to current file first
    local = os.path.join(os.path.dirname(current_file), include_path)
    if os.path.exists(local):
        return os.path.normpath(local)
    
    # Try relative to src_dir
    full = os.path.join(src_dir, include_path)
    if os.path.exists(full):
        return os.path.normpath(full)
    
    # Try walking src_dir
    for root, dirs, files in os.walk(src_dir):
        if include_path in files:
            return os.path.normpath(os.path.join(root, include_path))
        basename = os.path.basename(include_path)
        if basename in files:
            candidate = os.path.join(root, basename)
            rel = os.path.relpath(candidate, src_dir)
            expected = include_path.replace('\\', '/').split('/')[-1]
            if basename == expected:
                return os.path.normpath(candidate)
    
    return None

def flatten_file(filepath, src_dir, processed, output_lines, system_includes, depth=0):
    """Recursively process a file, inlining all local includes"""
    if filepath in processed:
        return
    processed.add(filepath)
    
    lines = read_file(filepath)
    
    for line in lines:
        stripped = line.rstrip('\n\r')
        
        # Handle header guards / pragma once
        if is_header_guard_line(stripped):
            continue
        
        # Handle local includes
        m = INCLUDE_LOCAL.match(stripped)
        if m:
            include_path = m.group(1)
            resolved = resolve_include(include_path, src_dir, filepath)
            if resolved:
                flatten_file(resolved, src_dir, processed, output_lines, system_includes, depth + 1)
            else:
                output_lines.append(f"// WARNING: could not resolve {include_path}\n")
            continue
        
        # Collect system includes
        m = INCLUDE_SYSTEM.match(stripped)
        if m:
            system_includes[stripped] = True
            continue
        
        output_lines.append(line)

    # Also merge matching .cpp for this .hpp (to catch includes only in .cpp)
    if filepath.endswith('.hpp'):
        cpp_path = filepath[:-4] + '.cpp'
        if os.path.exists(cpp_path) and cpp_path not in processed:
            flatten_file(cpp_path, src_dir, processed, output_lines, system_includes, depth + 1)

def main():
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} src_dir/ output_file")
        sys.exit(1)
    
    src_dir = sys.argv[1]
    output_file = sys.argv[2]
    
    main_path = None
    for root, dirs, files in os.walk(src_dir):
        for f in files:
            if f == 'main.cpp':
                main_path = os.path.join(root, f)
                break
    
    if not main_path:
        print("ERROR: main.cpp not found in", src_dir)
        sys.exit(1)
    
    processed = set()
    output_lines = []
    system_includes = OrderedDict()
    
    flatten_file(main_path, src_dir, processed, output_lines, system_includes)
    
    # Write output
    os.makedirs(os.path.dirname(output_file) or '.', exist_ok=True)
    with open(output_file, 'w', encoding='utf-8') as f:
        # System includes first
        for inc in system_includes:
            f.write(inc + '\n')
        f.write('\n')
        # Then all code
        for line in output_lines:
            f.write(line)
    
    size_kb = os.path.getsize(output_file) / 1024.0
    print(f"Merged {len(processed)} files → {output_file} ({size_kb:.1f} KiB)")

if __name__ == '__main__':
    main()
