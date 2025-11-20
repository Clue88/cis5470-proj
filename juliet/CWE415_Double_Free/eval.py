#!/usr/bin/env python3
import glob
import re
from collections import defaultdict

OUT_GLOB = "s*/CWE415_Double_Free__*.out"
RUN_FN_RE = re.compile(r"^Running DoubleFree on (\S+)")
POTENTIAL_RE = re.compile(r"^Potential Instructions by DoubleFree:")


def parse_out_file(path):
    """
    Parse a Juliet CWE415 .out file and return:
        { function_name: has_warning }
    """
    fn_warn = {}
    current_fn = None
    in_potential_block = False

    with open(path, "r", errors="ignore") as f:
        for line in f:
            line = line.rstrip("\n")

            m = RUN_FN_RE.match(line)
            if m:
                current_fn = m.group(1)
                fn_warn.setdefault(current_fn, False)
                in_potential_block = False
                continue

            if POTENTIAL_RE.match(line):
                in_potential_block = True
                continue

            if in_potential_block:
                if line.startswith("Running DoubleFree on ") or line.startswith(
                    "Running Double-free Analysis on "
                ):
                    in_potential_block = False
                    continue

                if line.strip():
                    fn_warn[current_fn] = True

    return fn_warn


def is_entry_fn(name: str) -> bool:
    return name.startswith("CWE415_Double_Free__")


def is_bad_fn(name: str) -> bool:
    return "CWE415_Double_Free__" in name and "_bad" in name


def is_good_fn(name: str) -> bool:
    return "CWE415_Double_Free__" in name and "_good" in name


def main():
    out_files = sorted(glob.glob(OUT_GLOB))
    if not out_files:
        print("No .out files found. Run `make` first.")
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
