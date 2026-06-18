#!/usr/bin/env python3
import sys
from pathlib import Path

def compare_block(name, scalar, rvv, start, size, pixel_size=1):
    a = scalar[start:start + size]
    b = rvv[start:start + size]

    min_len = min(len(a), len(b))
    matches = sum(1 for i in range(min_len) if a[i] == b[i])
    total = max(len(a), len(b))

    if total == 0:
        percent = 0.0
    else:
        percent = (matches / total) * 100.0

    print(f"{name:12s}: {percent:8.4f}% match  ({matches}/{total} bytes)")

    for i in range(min_len):
        if a[i] != b[i]:
            pixel_index = i // pixel_size
            print(f"  First mismatch in {name}: local byte {i}, pixel/index {pixel_index}")
            print(f"  scalar value = {a[i]}, rvv value = {b[i]}")
            return start + i

    if len(a) != len(b):
        print(f"  Size mismatch in {name}: scalar={len(a)}, rvv={len(b)}")
        return start + min_len

    return None


def main():
    if len(sys.argv) != 5:
        print("Usage: compare_raw_outputs.py scalar.raw rvv.raw width height")
        sys.exit(1)

    scalar_path = Path(sys.argv[1])
    rvv_path = Path(sys.argv[2])
    width = int(sys.argv[3])
    height = int(sys.argv[4])

    if not scalar_path.exists():
        print(f"ERROR: missing scalar file: {scalar_path}")
        sys.exit(1)

    if not rvv_path.exists():
        print(f"ERROR: missing RVV file: {rvv_path}")
        sys.exit(1)

    scalar = scalar_path.read_bytes()
    rvv = rvv_path.read_bytes()

    if len(scalar) == 0:
        print(f"ERROR: scalar output is empty: {scalar_path}")
        sys.exit(1)

    if len(rvv) == 0:
        print(f"ERROR: RVV output is empty: {rvv_path}")
        sys.exit(1)

    N = width * height

    expected_size = 5 * N
    print("============================================================")
    print("RVV Correctness Comparison")
    print("============================================================")
    print(f"Image size      : {width} x {height}")
    print(f"N               : {N}")
    print(f"Expected bytes  : {expected_size}")
    print(f"Scalar bytes    : {len(scalar)}")
    print(f"RVV bytes       : {len(rvv)}")
    print("")

    min_len = min(len(scalar), len(rvv))
    full_matches = sum(1 for i in range(min_len) if scalar[i] == rvv[i])
    full_total = max(len(scalar), len(rvv))
    full_percent = (full_matches / full_total) * 100.0

    print(f"{'FULL OUTPUT':12s}: {full_percent:8.4f}% match  ({full_matches}/{full_total} bytes)")
    print("")

    # Output layout:
    # mag_l2  = N bytes
    # mag_l1  = N bytes
    # nms_out = 2N bytes
    # final   = N bytes
    blocks = [
        ("mag_l2", 0, N, 1),
        ("mag_l1", N, N, 1),
        ("nms_out", 2 * N, 2 * N, 2),
        ("final_edge", 4 * N, N, 1),
    ]

    first_global_mismatch = None

    for name, start, size, pixel_size in blocks:
        mismatch = compare_block(name, scalar, rvv, start, size, pixel_size)
        if mismatch is not None and first_global_mismatch is None:
            first_global_mismatch = mismatch

    print("")

    if first_global_mismatch is None and len(scalar) == len(rvv):
        print("RESULT: FULL OUTPUT MATCHES EXACTLY ✅")
    else:
        print("RESULT: OUTPUTS DIFFER ❌")
        if first_global_mismatch is not None:
            print(f"First global mismatch byte: {first_global_mismatch}")

            if first_global_mismatch < N:
                print("Problem starts in: mag_l2")
                print("Likely source: Magnitude L2 RVV differs from scalar.")
            elif first_global_mismatch < 2 * N:
                print("Problem starts in: mag_l1")
                print("Likely source: Magnitude L1 RVV differs from scalar.")
            elif first_global_mismatch < 4 * N:
                print("Problem starts in: nms_out")
                print("Likely source: NMS input changed, usually from magnitude or direction.")
            else:
                print("Problem starts in: final_edge")
                print("Likely source: earlier small differences propagated through threshold/hysteresis.")

    print("============================================================")


if __name__ == "__main__":
    main()
