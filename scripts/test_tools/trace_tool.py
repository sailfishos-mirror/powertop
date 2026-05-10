#!/usr/bin/env python3
import sys
import base64
import argparse
import os
import textwrap
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
    line = line.strip()
    if not line: return None
    first_space = line.find(' ')
    if first_space == -1: return None
    tag = line[:first_space]
    rest = line[first_space+1:]
    if tag == 'N':
        return tag, rest, None
    if tag == 'M':
        return tag, rest, None
    if tag == 'T':
        return tag, rest, None
    if tag == 'A':
        # Format: A path mode result — e.g. "/sys/foo 4 0"
        # Last two tokens are mode (int) and result (int); path may contain spaces
        last_space = rest.rfind(' ')
        if last_space == -1:
            return tag, rest, None
        prev_space = rest.rfind(' ', 0, last_space)
        if prev_space == -1:
            return tag, rest, None
        path = rest[:prev_space]
        b64 = rest[prev_space+1:last_space] + ' ' + rest[last_space+1:]  # "mode result"
        return tag, path, b64
    if tag == 'L':
        # Format: L b64(target) path
        # b64 comes FIRST (may be empty = failed readlink: "L  path")
        # path comes SECOND and may contain spaces
        sep = rest.find(' ')
        if sep == -1:
            return tag, '', rest   # degenerate: no space
        b64 = rest[:sep]
        path = rest[sep+1:]
        return tag, path, b64
    if tag == 'D':
        # b64 comes last (may be absent = empty/missing directory)
        last_space = rest.rfind(' ')
        if last_space == -1:
            return tag, rest, ''
        path = rest[:last_space]
        b64 = rest[last_space+1:]
        return tag, path, b64
    last_space = rest.rfind(' ')
    if last_space == -1: return None
    path = rest[:last_space]
    b64 = rest[last_space+1:]
    return tag, path, b64

def get_tag_str(tag):
    if tag == 'R': return "Read"
    if tag == 'W': return "Write"
    if tag == 'N': return "Miss"
    if tag == 'M': return "MSR"
    if tag == 'T': return "Time"
    if tag == 'L': return "Link"
    if tag == 'D': return "Dir"
    if tag == 'A': return "Access"
    return "????"

def decode_content(tag, b64):
    """Return human-readable content for display, or None if not applicable."""
    if tag == 'N' or b64 is None:
        return None
    if tag == 'A':
        # b64 holds "mode result" (already decoded)
        parts = b64.split()
        if len(parts) == 2:
            mode_names = {4: 'R_OK', 2: 'W_OK', 1: 'X_OK', 6: 'R_OK|W_OK', 7: 'R_OK|W_OK|X_OK'}
            try:
                mode_int = int(parts[0])
                result = '0 (ok)' if parts[1] == '0' else '-1 (fail)'
                return f"mode={mode_names.get(mode_int, parts[0])} result={result}"
            except ValueError:
                return b64
        return b64
    if tag == 'M':
        # b64 is actually the raw "cpu offset value" string (stored in path slot)
        parts = b64.split()
        if len(parts) == 3:
            return f"cpu={parts[0]} offset=0x{parts[1]} value=0x{parts[2]}"
        return b64
    if tag == 'T':
        # For T records, content lives in the path slot ("sec usec")
        parts = (b64 or "").split()
        if len(parts) == 2:
            try:
                return f"t={int(parts[0])}.{int(parts[1]):06d}s"
            except ValueError:
                pass
        return b64
    if tag == 'L':
        if not b64:
            return "(broken link)"
        try:
            return base64.b64decode(b64).decode('utf-8', errors='replace').strip()
        except Exception:
            return None
    try:
        return base64.b64decode(b64).decode('utf-8', errors='replace').strip()
    except Exception:
        return None

def cmd_list(args):
    lines = load_trace(args.trace_file)
    show_content = getattr(args, 'content', False)
    path_filter = getattr(args, 'path', None)
    if show_content:
        print(f"{'Line':<6} {'Type':<6} {'Path':<55} {'Content'}")
        print("-" * 100)
    else:
        print(f"{'Line':<6} {'Type':<6} {'Path'}")
        print("-" * 60)
    for i, line in enumerate(lines, 1):
        parsed = parse_line(line, i)
        if not parsed:
            continue
        tag, path, b64 = parsed
        if path_filter and path_filter not in path:
            continue
        if show_content:
            # For M and T records the decoded content lives in path, not b64
            content = decode_content(tag, path if tag in ('M', 'T') else b64) or ""
            # Truncate long content for display
            if len(content) > 40:
                content = content[:37] + "..."
            print(f"{i:<6} {get_tag_str(tag):<6} {path:<55} {content}")
        else:
            print(f"{i:<6} {get_tag_str(tag):<6} {path}")

def cmd_extract(args):
    lines = load_trace(args.trace_file)
    if args.line < 1 or args.line > len(lines):
        print(f"Error: Line number {args.line} out of range (1-{len(lines)})")
        sys.exit(1)

    parsed = parse_line(lines[args.line - 1], args.line)
    if not parsed:
        print(f"Error: Invalid line format at line {args.line}")
        sys.exit(1)

    tag, path, b64_content = parsed
    if tag == 'N':
        print(f"Error: Line {args.line} is a 'File Not Found' (Miss) entry. No content to extract.")
        sys.exit(1)
    if tag == 'L':
        if not b64_content:
            print(f"Line {args.line} is a broken symlink (empty target). Writing empty file.")
            with open(args.output_file, 'wb') as f:
                pass
            return
    if tag in ('M', 'T'):
        print(f"Error: Line {args.line} is a {get_tag_str(tag)} entry; extract writes decoded bytes.")

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
    if tag == 'N':
        # Miss → Read with content
        lines[args.line - 1] = f"R {path} {b64_content}\n"
    elif tag == 'L':
        # Replace symlink target; preserve path
        lines[args.line - 1] = f"L {b64_content} {path}\n"
    else:
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
    if tag == 'N':
        print(f"Error: Line {args.line} is a 'File Not Found' (Miss) entry. No content to edit.")
        sys.exit(1)
    if tag in ('M', 'T'):
        print(f"Error: Line {args.line} is a {get_tag_str(tag)} entry; use direct file editing.")
        sys.exit(1)

    try:
        content = base64.b64decode(b64_content) if b64_content else b''
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
        if tag == 'L':
            lines[args.line - 1] = f"L {new_b64} {path}\n"
        else:
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
            print(f"{i:<6} {get_tag_str(tag):<6} {path}")

def cmd_grep(args):
    lines = load_trace(args.trace_file)
    search_bytes = args.string.encode('utf-8')
    print(f"{'Line':<6} {'Type':<6} {'Path'}")
    print("-" * 60)
    for i, line in enumerate(lines, 1):
        parsed = parse_line(line, i)
        if not parsed: continue
        tag, path, b64 = parsed
        if tag == 'N': continue
        try:
            content = base64.b64decode(b64)
            if search_bytes in content:
                print(f"{i:<6} {get_tag_str(tag):<6} {path}")
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
        if tag == 'N': continue
        
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
    
    print(f"Exported entries to {args.output_dir}")

def cmd_validate(args):
    lines = load_trace(args.trace_file)
    errors = 0
    for i, line in enumerate(lines, 1):
        if not line.strip(): continue
        parsed = parse_line(line, i)
        if not parsed:
            print(f"Line {i}: Invalid format")
            errors += 1
            continue
        tag, path, b64 = parsed
        if tag not in ['R', 'W', 'N', 'M', 'T', 'L', 'D', 'A']:
            print(f"Line {i}: Invalid tag '{tag}'")
            errors += 1
        if tag == 'M':
            parts = path.split(' ')
            if len(parts) != 3:
                print(f"Line {i}: Invalid MSR format (expected cpu offset value)")
                errors += 1
            else:
                try:
                    int(parts[1], 16)
                    int(parts[2], 16)
                except:
                    print(f"Line {i}: Invalid MSR hex value")
                    errors += 1
        if tag == 'T':
            parts = path.split(' ')
            if len(parts) != 2:
                print(f"Line {i}: Invalid Time format (expected sec usec)")
                errors += 1
            else:
                try:
                    int(parts[0])
                    int(parts[1])
                except:
                    print(f"Line {i}: Invalid Time decimal value")
                    errors += 1
        if tag == 'L':
            # b64 may be empty (broken link); if non-empty, must be valid base64
            if b64:
                try:
                    base64.b64decode(b64)
                except:
                    print(f"Line {i}: Invalid base64 in link target")
                    errors += 1
            if not path:
                print(f"Line {i}: Link record missing path")
                errors += 1
        if tag == 'A':
            # b64 field holds "mode result"
            if b64:
                parts = b64.split()
                if len(parts) != 2:
                    print(f"Line {i}: Invalid A record (expected 'mode result')")
                    errors += 1
                else:
                    try:
                        int(parts[0])
                        if parts[1] not in ('0', '-1'):
                            print(f"Line {i}: A record result must be 0 or -1")
                            errors += 1
                    except ValueError:
                        print(f"Line {i}: A record mode must be an integer")
                        errors += 1
        if tag in ['R', 'W', 'D'] and b64:
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

def cmd_add(args):
    """Append a new record to a trace file, creating it if necessary."""
    record_type = args.record_type.upper()
    path = args.path
    # Join all remaining tokens into one string so callers never need to
    # quote multi-token arguments like "sec usec" or "mode result".
    value_str = " ".join(args.value)

    if record_type == 'R':
        b64 = base64.b64encode(value_str.encode('utf-8')).decode('ascii')
        record = f"R {path} {b64}\n"
    elif record_type == 'W':
        b64 = base64.b64encode(value_str.encode('utf-8')).decode('ascii')
        record = f"W {path} {b64}\n"
    elif record_type == 'N':
        record = f"N {path}\n"
    elif record_type == 'L':
        # path = symlink path, value = target (empty = broken link)
        b64 = base64.b64encode(value_str.encode('utf-8')).decode('ascii') if value_str else ""
        record = f"L {b64} {path}\n"
    elif record_type == 'T':
        # Accept:  T <sec> <usec>   (two separate tokens, no quoting needed)
        parts = (path + " " + value_str).strip().split()
        if len(parts) != 2:
            print("Error: T record requires exactly two integers:  T <sec> <usec>")
            print("  Example:  add trace.ptrecord T 10 500000")
            sys.exit(1)
        try:
            sec = int(parts[0])
            usec = int(parts[1])
        except ValueError:
            print("Error: T record sec and usec must be integers.")
            sys.exit(1)
        record = f"T {sec} {usec}\n"
    elif record_type == 'M':
        # Accept:  M <cpu> <hex-offset> [<hex-value>]  (three separate tokens, no quoting)
        parts = (path + " " + value_str).strip().split()
        if len(parts) < 2 or len(parts) > 3:
            print("Error: M record requires cpu, hex-offset, and optional hex-value:")
            print("  Example:  add trace.ptrecord M 0 611 deadbeef")
            sys.exit(1)
        cpu = parts[0]
        offset = parts[1].lstrip('0x').lstrip('0X') or '0'
        hex_value = parts[2].lstrip('0x').lstrip('0X') if len(parts) > 2 else "0"
        try:
            int(cpu)
            int(offset, 16)
            int(hex_value, 16)
        except ValueError:
            print("Error: M record: cpu must be decimal, offset and value must be hex.")
            sys.exit(1)
        record = f"M {cpu} {offset} {hex_value}\n"
    elif record_type == 'A':
        # Accept:  A <path> <mode> <result>  (mode and result as separate tokens)
        parts = value_str.split()
        if len(parts) != 2:
            print("Error: A record requires mode and result after the path:")
            print("  Example:  add trace.ptrecord A /sys/foo 4 0")
            sys.exit(1)
        try:
            int(parts[0])
            if parts[1] not in ('0', '-1'):
                print("Error: A record result must be 0 (accessible) or -1 (not accessible).")
                sys.exit(1)
        except ValueError:
            print("Error: A record mode must be an integer.")
            sys.exit(1)
        record = f"A {path} {parts[0]} {parts[1]}\n"
    elif record_type == 'D':
        # value tokens are directory entries (may be zero or more)
        entries = sorted(value_str.split()) if value_str else []
        content = '\n'.join(entries)
        b64 = base64.b64encode(content.encode('utf-8')).decode('ascii')
        record = f"D {path} {b64}\n" if b64 else f"D {path}\n"
    else:
        print(f"Error: Unknown record type '{record_type}'. Use R, W, N, L, T, D, M, or A.")
        sys.exit(1)

    try:
        with open(args.trace_file, 'a') as f:
            f.write(record)
        print(f"Added: {record.strip()}")
    except Exception as e:
        print(f"Error writing to trace file: {e}")
        sys.exit(1)

def main():
    parser = argparse.ArgumentParser(description="PowerTOP Trace Tool")
    subparsers = parser.add_subparsers(dest="command", required=True)

    # list
    p = subparsers.add_parser("list", help="List all entries")
    p.add_argument("trace_file")
    p.add_argument("--content", "-c", action="store_true",
                   help="Show decoded content inline")
    p.add_argument("--path", "-p", metavar="SUBSTR",
                   help="Only show entries whose path contains SUBSTR")

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
    subparsers.add_parser("validate", help="Validate trace file format").add_argument("trace_file")

    # add
    p = subparsers.add_parser("add",
        help="Append a record to a trace file (creates file if needed)",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        description=textwrap.dedent("""\
            Append one record to a trace file (file is created if it does not exist).

            Record types and syntax:
              R <path> <content>          sysfs read  (include trailing \\n in content)
              W <path> <content>          sysfs write
              N <path>                    read miss (file not found)
              L <symlink-path> <target>   symlink readlink result
              D <dir-path> [entry ...]    directory listing (omit entries for empty)
              A <path> <mode> <result>    access(2) check  (mode=4 for R_OK; result: 0=ok, -1=fail)
              T <sec> <usec>             timestamp from pt_gettime()
              M <cpu> <hex-offset> [<hex-value>]  MSR read (value defaults to 0)

            Examples:
              add trace.ptrecord R /sys/class/drm/card0/device/hwmon/hwmon0/name "xe\\n"
              add trace.ptrecord D /sys/class/drm card0
              add trace.ptrecord D /sys/class/drm card0 card1 card2
              add trace.ptrecord A /sys/class/drm/card0/power1_label 4 0
              add trace.ptrecord T 10 500000
              add trace.ptrecord M 0 611 deadbeef
        """))
    p.add_argument("trace_file")
    p.add_argument("record_type", metavar="type", choices=["R", "W", "N", "L", "T", "D", "M", "A"],
                   help="Record type (see description above)")
    p.add_argument("path", help="Path / first argument (see description above)")
    p.add_argument("value", nargs="*", default=[],
                   help="Remaining arguments (content, entries, mode+result, sec+usec, etc.)")

    args = parser.parse_args()
    
    cmds = {
        "list": cmd_list, "extract": cmd_extract, "replace": cmd_replace,
        "edit": cmd_edit, "search": cmd_search, "grep": cmd_grep,
        "export": cmd_export, "validate": cmd_validate, "add": cmd_add,
    }
    cmds[args.command](args)

if __name__ == "__main__":
    main()
