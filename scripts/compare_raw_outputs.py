#!/usr/bin/env python3

import sys
from pathlib import Path


def compare_block(name, scalar, rvv, start, size, pixel_size=1):
    a = scalar[start:start + size]
    b = rvv[start:start + size]

    min_len = min(len(a), len(b))
    total = max(len(a), len(b))

    matches = sum(1 for i in range(min_len) if a[i] == b[i])
    percent = 0.0 if total == 0 else (matches / total) * 100.0

    print(f"{name:18s}: {percent:8.4f}% match  ({matches}/{total} bytes)")

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

    scalar = scalar_path.read_bytes()
    rvv = rvv_path.read_bytes()

    N = width * height

    expected_size = 8 * N

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
    full_total = max(len(scalar), len(rvv))
    full_matches = sum(1 for i in range(min_len) if scalar[i] == rvv[i])
    full_percent = 0.0 if full_total == 0 else (full_matches / full_total) * 100.0

    print(f"{'FULL OUTPUT':18s}: {full_percent:8.4f}% match  ({full_matches}/{full_total} bytes)")
    print("")

    if len(scalar) != expected_size or len(rvv) != expected_size:
        print("WARNING: Output size does not match the expected layout.")
        print("Expected layout:")
        print("gaussian | mag_l2 | mag_l1 | direction | nms_out | double_threshold | final_hysteresis")
        print("")

    blocks = [
        ("gaussian",          0 * N, N,     1),
        ("mag_l2",            1 * N, N,     1),
        ("mag_l1",            2 * N, N,     1),
        ("direction",         3 * N, N,     1),
        ("nms_out",           4 * N, 2 * N, 2),
        ("double_threshold",  6 * N, N,     1),
        ("final_hysteresis",  7 * N, N,     1),
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

            if first_global_mismatch < 1 * N:
                print("Problem starts in: gaussian")
                print("Likely source: Gaussian RVV differs from scalar.")
            elif first_global_mismatch < 2 * N:
                print("Problem starts in: mag_l2")
                print("Likely source: Magnitude L2 differs, or Gaussian mismatch propagated.")
            elif first_global_mismatch < 3 * N:
                print("Problem starts in: mag_l1")
                print("Likely source: Magnitude L1 differs, or Gaussian mismatch propagated.")
            elif first_global_mismatch < 4 * N:
                print("Problem starts in: direction")
                print("Likely source: Direction RVV differs from scalar.")
            elif first_global_mismatch < 6 * N:
                print("Problem starts in: nms_out")
                print("Likely source: NMS input changed, usually magnitude or direction.")
            elif first_global_mismatch < 7 * N:
                print("Problem starts in: double_threshold")
                print("Likely source: NMS mismatch propagated into thresholding.")
            else:
                print("Problem starts in: final_hysteresis")
                print("Likely source: earlier threshold differences propagated through hysteresis.")

    print("============================================================")


if __name__ == "__main__":
    main()
