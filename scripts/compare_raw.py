"""
Compare two equal-length raw byte files, allowing a rounding tolerance.

Usage: python3 scripts/compare_raw.py file_a file_b [tolerance] [label]
Exit code 0 = match within tolerance, 1 = mismatch.
"""
import sys
import numpy as np


def main():
    if len(sys.argv) < 3:
        print("Usage: compare_raw.py file_a file_b [tolerance] [label]")
        sys.exit(2)

    path_a, path_b = sys.argv[1], sys.argv[2]
    tolerance = int(sys.argv[3]) if len(sys.argv) > 3 else 0
    label = sys.argv[4] if len(sys.argv) > 4 else f"{path_a} vs {path_b}"

    a = np.fromfile(path_a, dtype=np.uint8)
    b = np.fromfile(path_b, dtype=np.uint8)

    if a.size != b.size:
        print(f"[{label}] SIZE MISMATCH: {a.size} vs {b.size} bytes")
        sys.exit(1)

    # Widen to int16 before subtracting so unsigned wraparound doesn't hide
    # negative differences (e.g. 0 - 1 would wrap to 255 in uint8 math).
    diff = np.abs(a.astype(np.int16) - b.astype(np.int16))
    max_diff = int(diff.max()) if diff.size else 0
    bad_mask = diff > tolerance
    bad = int(np.sum(bad_mask))

    if bad == 0:
        print(f"[{label}] MATCH    ({a.size} bytes, tolerance=+/-{tolerance}, max diff seen={max_diff})")
        sys.exit(0)
    else:
        idx = int(np.argmax(bad_mask))
        print(f"[{label}] MISMATCH {bad}/{a.size} bytes exceed tolerance +/-{tolerance} (max diff={max_diff})")
        print(f"          first offending byte at index {idx}: a={int(a[idx])} b={int(b[idx])}")
        sys.exit(1)


if __name__ == "__main__":
    main()
