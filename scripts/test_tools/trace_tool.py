#!/usr/bin/env python3
import sys
import base64
import argparse
import os
import tempfile
import subprocess
import re
import shutil

def load_trace(trace_file):
    try:
        with open(trace_file, 'r') as f:
            return f.readlines()
    except Exception as e:
        print(f"Error opening trace file: {e}")
        sys.exit(1)

def save_trace(trace_file, lines):
    try:
        with open(trace_file, 'w') as f:
            f.writelines(lines)
    except Exception as e:
        print(f"Error writing to trace file: {e}")
        sys.exit(1)

def parse_line(line, line_num):
    parts = line.strip().split(' ')
    if len(parts) < 3:
        return None
    tag = parts[0]
    b64_content = parts[-1]
    path = " ".join(parts[1:-1])
    return tag, path, b64_content

def cmd_list(args):
    lines = load_trace(args.trace_file)
    print(f"{'Line':<6} {'Type':<6} {'Path'}")
    print("-" * 60)
    for i, line in enumerate(lines, 1):
        parsed = parse_line(line, i)
        if not parsed:
            continue
        tag, path, _ = parsed
        tag_str = "Read" if tag == 'R' else "Write"
        print(f"{i:<6} {tag_str:<6} {path}")

def cmd_extract(args):
    lines = load_trace(args.trace_file)
    if args.line < 1 or args.line > len(lines):
        print(f"Error: Line number {args.line} out of range (1-{len(lines)})")
        sys.exit(1)

    parsed = parse_line(lines[args.line - 1], args.line)
    if not parsed:
        print(f"Error: Invalid line format at line {args.line}")
        sys.exit(1)

    _, _, b64_content = parsed
    try:
        content = base64.b64decode(b64_content)
        with open(args.output_file, 'wb') as f:
            f.write(content)
        print(f"Extracted line {args.line} to {args.output_file}")
    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)

def cmd_replace(args):
    lines = load_trace(args.trace_file)
    if args.line < 1 or args.line > len(lines):
        print(f"Error: Line number {args.line} out of range (1-{len(lines)})")
        sys.exit(1)

    try:
        with open(args.input_file, 'rb') as f:
            content = f.read()
    except Exception as e:
        print(f"Error opening input file: {e}")
        sys.exit(1)

    b64_content = base64.b64encode(content).decode('ascii')
    parsed = parse_line(lines[args.line - 1], args.line)
    if not parsed:
        print(f"Error: Invalid line format at line {args.line}")
        sys.exit(1)

    tag, path, _ = parsed
    lines[args.line - 1] = f"{tag} {path} {b64_content}\n"
    save_trace(args.trace_file, lines)
    print(f"Replaced content at line {args.line} with {args.input_file}")

def cmd_edit(args):
    lines = load_trace(args.trace_file)
    if args.line < 1 or args.line > len(lines):
        print(f"Error: Line number {args.line} out of range (1-{len(lines)})")
        sys.exit(1)

    parsed = parse_line(lines[args.line - 1], args.line)
    if not parsed:
        print(f"Error: Invalid line format at line {args.line}")
        sys.exit(1)

    tag, path, b64_content = parsed
    try:
        content = base64.b64decode(b64_content)
    except Exception as e:
        print(f"Error decoding base64: {e}")
        sys.exit(1)

    fd, temp_path = tempfile.mkstemp()
    try:
        with os.fdopen(fd, 'wb') as tmp:
            tmp.write(content)
        
        editor = os.environ.get('EDITOR', 'joe')
        subprocess.run([editor, temp_path], check=True)
        
        with open(temp_path, 'rb') as f:
            new_content = f.read()
        
        new_b64 = base64.b64encode(new_content).decode('ascii')
        lines[args.line - 1] = f"{tag} {path} {new_b64}\n"
        save_trace(args.trace_file, lines)
        print(f"Successfully edited line {args.line}")
        
    except Exception as e:
        print(f"Error during edit: {e}")
        sys.exit(1)
    finally:
        if os.path.exists(temp_path):
            os.remove(temp_path)

def cmd_search(args):
    lines = load_trace(args.trace_file)
    pattern = re.compile(args.pattern)
    print(f"{'Line':<6} {'Type':<6} {'Path'}")
    print("-" * 60)
    for i, line in enumerate(lines, 1):
        parsed = parse_line(line, i)
        if not parsed: continue
        tag, path, _ = parsed
        if pattern.search(path):
            tag_str = "Read" if tag == 'R' else "Write"
            print(f"{i:<6} {tag_str:<6} {path}")

def cmd_grep(args):
    lines = load_trace(args.trace_file)
    search_bytes = args.string.encode('utf-8')
    print(f"{'Line':<6} {'Type':<6} {'Path'}")
    print("-" * 60)
    for i, line in enumerate(lines, 1):
        parsed = parse_line(line, i)
        if not parsed: continue
        tag, path, b64 = parsed
        try:
            content = base64.b64decode(b64)
            if search_bytes in content:
                tag_str = "Read" if tag == 'R' else "Write"
                print(f"{i:<6} {tag_str:<6} {path}")
        except:
            continue

def cmd_export(args):
    lines = load_trace(args.trace_file)
    if not os.path.exists(args.output_dir):
        os.makedirs(args.output_dir)
    
    for i, line in enumerate(lines, 1):
        parsed = parse_line(line, i)
        if not parsed: continue
        tag, path, b64 = parsed
        
        # Sanitize path for filename
        safe_path = path.replace('/', '_').replace(' ', '_').strip('_')
        filename = f"{i:04d}_{tag}_{safe_path}"
        dest = os.path.join(args.output_dir, filename)
        
        try:
            content = base64.b64decode(b64)
            with open(dest, 'wb') as f:
                f.write(content)
        except Exception as e:
            print(f"Warning: Failed to export line {i}: {e}")
    
    print(f"Exported {len(lines)} entries to {args.output_dir}")

def cmd_validate(args):
    lines = load_trace(args.trace_file)
    errors = 0
    for i, line in enumerate(lines, 1):
        if not line.strip(): continue
        parsed = parse_line(line, i)
        if not parsed:
            print(f"Line {i}: Invalid format (missing parts)")
            errors += 1
            continue
        tag, path, b64 = parsed
        if tag not in ['R', 'W']:
            print(f"Line {i}: Invalid tag '{tag}'")
            errors += 1
        try:
            base64.b64decode(b64)
        except:
            print(f"Line {i}: Invalid base64 content")
            errors += 1
    
    if errors == 0:
        print("Trace file is valid.")
    else:
        print(f"Validation failed with {errors} errors.")
        sys.exit(1)

def main():
    parser = argparse.ArgumentParser(description="PowerTOP Trace Tool")
    subparsers = parser.add_subparsers(dest="command", required=True)

    # list
    subparsers.add_parser("list", help="List all entries").add_argument("trace_file")
    
    # extract
    p = subparsers.add_parser("extract", help="Extract a line")
    p.add_argument("trace_file")
    p.add_argument("line", type=int)
    p.add_argument("output_file")
    
    # replace
    p = subparsers.add_parser("replace", help="Replace a line")
    p.add_argument("trace_file")
    p.add_argument("line", type=int)
    p.add_argument("input_file")
    
    # edit
    p = subparsers.add_parser("edit", help="Edit a line in-place")
    p.add_argument("trace_file")
    p.add_argument("line", type=int)

    # search
    p = subparsers.add_parser("search", help="Search paths by regex")
    p.add_argument("trace_file")
    p.add_argument("pattern")

    # grep
    p = subparsers.add_parser("grep", help="Search content for string")
    p.add_argument("trace_file")
    p.add_argument("string")

    # export
    p = subparsers.add_parser("export", help="Export all entries to a directory")
    p.add_argument("trace_file")
    p.add_argument("output_dir")

    # validate
    p = subparsers.add_parser("validate", help="Validate trace file format").add_argument("trace_file")

    args = parser.parse_args()
    
    cmds = {
        "list": cmd_list, "extract": cmd_extract, "replace": cmd_replace,
        "edit": cmd_edit, "search": cmd_search, "grep": cmd_grep,
        "export": cmd_export, "validate": cmd_validate
    }
    cmds[args.command](args)

if __name__ == "__main__":
    main()
