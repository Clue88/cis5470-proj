#!/usr/bin/env python3
import glob
import re
from collections import defaultdict

# Accept both Juliet-style `s*/...` layout and the flat `.out` files produced
# by the Makefile.pa in this directory.
OUT_GLOBS = [
    "s*/CWE476_NULL_Pointer_Dereference__*.out",
    "CWE476_NULL_Pointer_Dereference__*.out",
]
RUN_FN_RE = re.compile(r"^Running PointerAnalysis on (\S+)")
PA_WARN_RE = re.compile(r"Possible null dereference")


def parse_out_file(path):
    """
    Parse a Juliet CWE476 .out file and return:
        { function_name: has_warning }
    We read the .out to discover function names, then look into the
    corresponding .err file for any PointerAnalysis warnings. If a .err
    contains any PA_WARN_RE, all functions in that .out are conservatively
    marked as warned (mirrors CWE415 evaluator approach).
    """
    fn_warn = {}
    current_fn = None

    with open(path, "r", errors="ignore") as f:
        for line in f:
            line = line.rstrip("\n")
            m = RUN_FN_RE.match(line)
            if m:
                current_fn = m.group(1)
                fn_warn.setdefault(current_fn, False)

    err_path = path[:-4] + ".err"
    warned = False
    try:
        with open(err_path, "r", errors="ignore") as ef:
            for line in ef:
                if PA_WARN_RE.search(line):
                    warned = True
                    break
    except FileNotFoundError:
        warned = False

    if warned:
        for k in fn_warn.keys():
            fn_warn[k] = True

    return fn_warn


def is_bad_fn(name: str) -> bool:
    return "CWE476_NULL_Pointer_Dereference__" in name and ("_bad" in name or name.endswith("_bad"))


def is_good_fn(name: str) -> bool:
    return "CWE476_NULL_Pointer_Dereference__" in name and ("_good" in name or name.endswith("_good"))


def main():
    # Collect files from both glob patterns, de-duplicate and sort
    out_set = set()
    for g in OUT_GLOBS:
        for p in glob.glob(g):
            out_set.add(p)
    out_files = sorted(out_set)
    if not out_files:
        print("No .out files found. Run `make -f Makefile.pa` first.")
        return

    all_fn_warn = defaultdict(bool)

    for out in out_files:
        per_file = parse_out_file(out)
        for fn, warned in per_file.items():
            all_fn_warn[fn] = all_fn_warn[fn] or warned

    tp = fp = tn = fn = 0
    examples_tp = []
    examples_fp = []
    examples_fn = []

    for fn_name, warned in all_fn_warn.items():
        if is_bad_fn(fn_name):
            if warned:
                tp += 1
                if len(examples_tp) < 5:
                    examples_tp.append(fn_name)
            else:
                fn += 1
                if len(examples_fn) < 5:
                    examples_fn.append(fn_name)
        elif is_good_fn(fn_name):
            if warned:
                fp += 1
                if len(examples_fp) < 5:
                    examples_fp.append(fn_name)
            else:
                tn += 1

    total = tp + fp + tn + fn
    print(f"Total labeled functions: {total}")
    print(f"  TP (bad & warned): {tp}")
    print(f"  FN (bad & silent): {fn}")
    print(f"  FP (good & warned): {fp}")
    print(f"  TN (good & silent): {tn}")

    precision = tp / (tp + fp) if (tp + fp) > 0 else 0.0
    recall = tp / (tp + fn) if (tp + fn) > 0 else 0.0

    print(f"\nPrecision: {precision:.3f}")
    print(f"Recall:    {recall:.3f}")

    if examples_tp:
        print("\nExample TPs:")
        for name in examples_tp:
            print("  ", name)

    if examples_fp:
        print("\nExample FPs:")
        for name in examples_fp:
            print("  ", name)

    if examples_fn:
        print("\nExample FNs:")
        for name in examples_fn:
            print("  ", name)


if __name__ == "__main__":
    main()
#!/usr/bin/env python3
import glob
import re
from collections import defaultdict

OUT_GLOB = "s*/CWE476_NULL_Pointer_Dereference__*.out"
RUN_FN_RE = re.compile(r"^Running PointerAnalysis on (\S+)")
PA_WARN_RE = re.compile(r"Possible null dereference")


def parse_out_file(path):
    """
    Parse a Juliet CWE476 .out file and return:
        { function_name: has_warning }
    This mirrors the CWE415 eval approach: we read the .out to discover function
    names and then look into the corresponding .err file for pointer-analysis
    warnings. If a .err contains any PA_WARN_RE, all functions in that .out
    are conservatively marked as warned.
    """
    fn_warn = {}
    current_fn = None

    # First pass: collect function names from the .out (we expect lines
    # like "Running PointerAnalysis on <function>")
    with open(path, "r", errors="ignore") as f:
        for line in f:
            line = line.rstrip("\n")
            m = RUN_FN_RE.match(line)
            if m:
                current_fn = m.group(1)
                fn_warn.setdefault(current_fn, False)

    # Second pass: check the corresponding .err for any PA warnings. If any
    # warning exists, mark all functions from this file as warned (conservative).
    err_path = path[:-4] + ".err"
    warned = False
    try:
        with open(err_path, "r", errors="ignore") as ef:
            for line in ef:
                if PA_WARN_RE.search(line):
                    warned = True
                    break
    except FileNotFoundError:
        warned = False

    if warned:
        for k in fn_warn.keys():
            fn_warn[k] = True

    return fn_warn


def is_entry_fn(name: str) -> bool:
    return name.startswith("CWE476_NULL_Pointer_Dereference__")


def is_bad_fn(name: str) -> bool:
    return "CWE476_NULL_Pointer_Dereference__" in name and "_bad" in name


def is_good_fn(name: str) -> bool:
    return "CWE476_NULL_Pointer_Dereference__" in name and "_good" in name


def main():
    out_files = sorted(glob.glob(OUT_GLOB))
    if not out_files:
        print("No .out files found. Run `make -f Makefile.pa` first.")
        return

    all_fn_warn = defaultdict(bool)

    for out in out_files:
        per_file = parse_out_file(out)
        for fn, warned in per_file.items():
            all_fn_warn[fn] = all_fn_warn[fn] or warned

    tp = fp = tn = fn = 0
    examples_tp = []
    examples_fp = []
    examples_fn = []

    for fn_name, warned in all_fn_warn.items():
        if is_bad_fn(fn_name):
            if warned:
                tp += 1
                if len(examples_tp) < 5:
                    examples_tp.append(fn_name)
            else:
                fn += 1
                if len(examples_fn) < 5:
                    examples_fn.append(fn_name)
        elif is_good_fn(fn_name):
            if warned:
                fp += 1
                if len(examples_fp) < 5:
                    examples_fp.append(fn_name)
            else:
                tn += 1
        # Ignore helper functions with neither _bad nor _good

    # Print results
    total = tp + fp + tn + fn
    print(f"Total labeled functions: {total}")
    print(f"  TP (bad & warned): {tp}")
    print(f"  FN (bad & silent): {fn}")
    print(f"  FP (good & warned): {fp}")
    print(f"  TN (good & silent): {tn}")

    precision = tp / (tp + fp) if (tp + fp) > 0 else 0.0
    recall = tp / (tp + fn) if (tp + fn) > 0 else 0.0

    print(f"\nPrecision: {precision:.3f}")
    print(f"Recall:    {recall:.3f}")

    if examples_tp:
        print("\nExample TPs:")
        for name in examples_tp:
            print("  ", name)

    if examples_fp:
        print("\nExample FPs:")
        for name in examples_fp:
            print("  ", name)

    if examples_fn:
        print("\nExample FNs:")
        for name in examples_fn:
            print("  ", name)


if __name__ == "__main__":
    main()
