# --- Setup Variables ---
CXX_RISCV  = riscv64-unknown-elf-g++
CXX_NATIVE = g++

# Base Compiler Flags for RISC-V (Extracted optimization to allow sweeps)
BASE_FLAGS_RISCV = -march=rv64gcv -Wall

# Compiler Flags
CXXFLAGS_RISCV  = $(BASE_FLAGS_RISCV) -O3
CXXFLAGS_NATIVE = -O3 -Wall

# Directories
SRC_DIR   = src
INC_DIR   = include
BUILD_DIR = build

# --- Targets ---

# 1. Compile for RISC-V (Your original baseline)
riscv:
	mkdir -p $(BUILD_DIR)
	$(CXX_RISCV) $(CXXFLAGS_RISCV) -I$(INC_DIR) $(SRC_DIR)/*.cpp -o $(BUILD_DIR)/canny_riscv.elf

# ====================================================================
# PHASE 4: COMPILER OPTIMIZATION SWEEP TARGETS
# ====================================================================

# Compile with NO optimization (-O0)
riscv_O0:
	mkdir -p $(BUILD_DIR)
	$(CXX_RISCV) $(BASE_FLAGS_RISCV) -O0 -I$(INC_DIR) $(SRC_DIR)/*.cpp -o $(BUILD_DIR)/canny_O0.elf

# Compile with standard optimization (-O2)
riscv_O2:
	mkdir -p $(BUILD_DIR)
	$(CXX_RISCV) $(BASE_FLAGS_RISCV) -O2 -I$(INC_DIR) $(SRC_DIR)/*.cpp -o $(BUILD_DIR)/canny_O2.elf

# Compile with aggressive optimization (-O3)
riscv_O3:
	mkdir -p $(BUILD_DIR)
	$(CXX_RISCV) $(BASE_FLAGS_RISCV) -O3 -I$(INC_DIR) $(SRC_DIR)/*.cpp -o $(BUILD_DIR)/canny_O3.elf

# Compile with auto-vectorization flags and capture the optimization report
riscv_autovec:
	mkdir -p $(BUILD_DIR)
	$(CXX_RISCV) $(BASE_FLAGS_RISCV) -O3 -ftree-vectorize -fopt-info-vec-all -I$(INC_DIR) $(SRC_DIR)/*.cpp -o $(BUILD_DIR)/canny_autovec.elf > $(BUILD_DIR)/vector_report.txt 2>&1

# Compiles all 4 optimization variants at once
sweep_build: riscv_O0 riscv_O2 riscv_O3 riscv_autovec
	@echo "\n===== Phase 4: All 4 Variants Compiled Successfully! ====="

# ====================================================================

# 2. Compile and run GoogleTest on host
native:
	mkdir -p $(BUILD_DIR)
	$(CXX_NATIVE) -O2 -I$(INC_DIR) tests/test_canny.cpp \
		-o $(BUILD_DIR)/test_canny -lgtest -lgtest_main -lpthread
	$(CXX_NATIVE) -O2 -I$(INC_DIR) tests/test_nms.cpp src/nms.cpp \
		-o $(BUILD_DIR)/test_nms -lgtest -lgtest_main -lpthread
	$(CXX_NATIVE) -O2 -I$(INC_DIR) tests/test_double_threshold.cpp src/double_threshold.cpp \
		-o $(BUILD_DIR)/test_dt -lgtest -lgtest_main -lpthread
	$(CXX_NATIVE) -O2 -I$(INC_DIR) tests/test_hysteresis.cpp src/hysteresis.cpp \
		-o $(BUILD_DIR)/test_hysteresis -lgtest -lgtest_main -lpthread

test: native
	@echo "\n===== Running Canny Tests ====="
	@./$(BUILD_DIR)/test_canny
	@echo "\n===== Running NMS Tests ====="
	@./$(BUILD_DIR)/test_nms
	@echo "\n===== Running Double Threshold Tests ====="
	@./$(BUILD_DIR)/test_dt
	@echo "\n===== Running Hysteresis Tests ====="
	@./$(BUILD_DIR)/test_hysteresis
	@echo "\n===== All Tests Done ====="

# 3. Run on QEMU
run:
	qemu-riscv64 -cpu rv64,v=true,vlen=128 $(BUILD_DIR)/canny_riscv.elf

clean:
	rm -rf $(BUILD_DIR)

# --- Git Automation ---
NAME ?= Zeyad Elsonney
MSG  ?= Update

push:
	git add .
	git commit -m "$(NAME): $(MSG)"
	git push

# --- Image Processing ---
IMG        ?= tiger-animals-cat-predator-preview.jpg
RESULT_DIR  = results
L2_OUT      = $(RESULT_DIR)/result_L2.png
L1_OUT      = $(RESULT_DIR)/result_L1.png

test_image: riscv
	@mkdir -p $(RESULT_DIR)
	@echo "Processing $(IMG)..."
	@convert $(IMG) -resize 512x512! -colorspace gray -depth 8 gray:- | \
	QEMU_LD_PREFIX=/usr/riscv64-linux-gnu \
	qemu-riscv64 -cpu rv64,v=true,vlen=128 $(BUILD_DIR)/canny_riscv.elf > /tmp/both_raw.gray
	@dd if=/tmp/both_raw.gray bs=262144 count=1 skip=0 | \
	convert -size 512x512 -depth 8 gray:- $(RESULT_DIR)/result_L2.png
	@dd if=/tmp/both_raw.gray bs=262144 count=1 skip=1 | \
	convert -size 512x512 -depth 8 gray:- $(RESULT_DIR)/result_L1.png
	@dd if=/tmp/both_raw.gray bs=262144 count=1 skip=3 | \
	convert -size 512x512 -depth 8 gray:- $(RESULT_DIR)/result_hysteresis.png
	@echo "Done! Results in $(RESULT_DIR)/"

# --- Phase 4 Execution Sweep Automation ---
# Usage: make sweep_run IMG=your_image.jpg
sweep_run: sweep_build
	@mkdir -p $(RESULT_DIR)
	@echo "\n========================================================="
	@echo "   RUNNING OPTIMIZATION SWEEP (100 Iterations Per Stage)"
	@echo "========================================================="
	@echo "\n>>> Running -O0 Binary:"
	@convert $(IMG) -resize 512x512! -colorspace gray -depth 8 gray:- | QEMU_LD_PREFIX=/usr/riscv64-linux-gnu qemu-riscv64 -cpu rv64,v=true,vlen=128 $(BUILD_DIR)/canny_O0.elf > /dev/null
	@echo "\n>>> Running -O2 Binary:"
	@convert $(IMG) -resize 512x512! -colorspace gray -depth 8 gray:- | QEMU_LD_PREFIX=/usr/riscv64-linux-gnu qemu-riscv64 -cpu rv64,v=true,vlen=128 $(BUILD_DIR)/canny_O2.elf > /dev/null
	@echo "\n>>> Running -O3 Binary:"
	@convert $(IMG) -resize 512x512! -colorspace gray -depth 8 gray:- | QEMU_LD_PREFIX=/usr/riscv64-linux-gnu qemu-riscv64 -cpu rv64,v=true,vlen=128 $(BUILD_DIR)/canny_O3.elf > /dev/null
	@echo "\n>>> Running Auto-Vectorized Binary:"
	@convert $(IMG) -resize 512x512! -colorspace gray -depth 8 gray:- | QEMU_LD_PREFIX=/usr/riscv64-linux-gnu qemu-riscv64 -cpu rv64,v=true,vlen=128 $(BUILD_DIR)/canny_autovec.elf > /dev/null
	@echo "\n========================================================="

# Phase 6 compiling makefile :

# Phase 6 compiling makefile :

manual_rvv:
	mkdir -p build
	$(CXX_RISCV) -march=rv64gcv -Wall -O3 -DUSE_MANUAL_RVV -Iinclude src/*.cpp -o build/canny_manual_rvv.elf

# ====================================================================
# Phase 6: RVV vs Scalar comparison targets
# ====================================================================

PIXELS    = 262144
CHECK_DIR = /tmp/rvv_check

# 1) Side-by-side timing: scalar vs RVV, same image, same VLEN.
#    Both binaries print their Phase 5 timing breakdown to stderr, so
#    just read the two blocks top to bottom and compare line by line.

compare_rvv: riscv manual_rvv
	@echo "\n================= SCALAR (no RVV) ================="
	@convert $(IMG) -resize 512x512! -colorspace gray -depth 8 gray:- | \
	QEMU_LD_PREFIX=/usr/riscv64-linux-gnu \
	qemu-riscv64 -cpu rv64,v=true,vlen=128 $(BUILD_DIR)/canny_riscv.elf > /dev/null
	@echo "\n================= MANUAL RVV ================="
	@convert $(IMG) -resize 512x512! -colorspace gray -depth 8 gray:- | \
	QEMU_LD_PREFIX=/usr/riscv64-linux-gnu \
	qemu-riscv64 -cpu rv64,v=true,vlen=128 $(BUILD_DIR)/canny_manual_rvv.elf > /dev/null

# 2) Correctness: does the RVV build produce the same result as scalar?
#    Checks L1 (exact match expected), L2 (float rounding -> +/-1 tolerance),
#    and the actual final hysteresis output (exact match expected, since
#    hysteresis is just a binary edge/no-edge decision).


.PHONY: check_rvv

check_rvv: manual_rvv
	mkdir -p build results

	riscv64-unknown-elf-g++ -march=rv64gcv -Wall -O3 -Iinclude src/*.cpp -o build/canny_O3.elf

	rm -f /tmp/scalar_output.raw /tmp/rvv_output.raw
	rm -f /tmp/scalar_log.txt /tmp/rvv_log.txt

	convert tiger-animals-cat-predator-preview.jpg -resize 512x512! -colorspace gray -depth 8 gray:- | \
	QEMU_LD_PREFIX=/usr/riscv64-linux-gnu qemu-riscv64 -cpu rv64,v=true,vlen=128 build/canny_O3.elf \
	> /tmp/scalar_output.raw 2> /tmp/scalar_log.txt

	test -s /tmp/scalar_output.raw || (echo "Scalar output is empty"; cat /tmp/scalar_log.txt; exit 1)

	convert tiger-animals-cat-predator-preview.jpg -resize 512x512! -colorspace gray -depth 8 gray:- | \
	QEMU_LD_PREFIX=/usr/riscv64-linux-gnu qemu-riscv64 -cpu rv64,v=true,vlen=128 build/canny_manual_rvv.elf \
	> /tmp/rvv_output.raw 2> /tmp/rvv_log.txt

	test -s /tmp/rvv_output.raw || (echo "RVV output is empty"; cat /tmp/rvv_log.txt; exit 1)

	python3 scripts/compare_raw_outputs.py /tmp/scalar_output.raw /tmp/rvv_output.raw 512 512 | tee results/rvv_comparison_report.txt

	cp /tmp/scalar_log.txt results/scalar_check_log.txt
	cp /tmp/rvv_log.txt results/rvv_check_log.txt
	cp /tmp/scalar_output.raw results/scalar_output.raw
	cp /tmp/rvv_output.raw results/rvv_output.raw

# 3) VLEN scaling: does the RVV build give identical output at VLEN
#    128/256/512? If your kernels are truly vector-length-agnostic, every
#    one of these should report an exact match (tolerance 0), regardless
#    of L1/L2/float involved - VLEN must never change the answer.

vlen_sweep: manual_rvv
	@mkdir -p $(CHECK_DIR)
	@for V in 128 256 512; do \
		echo "\n================= VLEN=$$V ================="; \
		convert $(IMG) -resize 512x512! -colorspace gray -depth 8 gray:- | \
		QEMU_LD_PREFIX=/usr/riscv64-linux-gnu \
		qemu-riscv64 -cpu rv64,v=true,vlen=$$V $(BUILD_DIR)/canny_manual_rvv.elf > $(CHECK_DIR)/vlen_$$V.raw; \
		cp /tmp/hysteresis_out.raw $(CHECK_DIR)/vlen_$${V}_hyst.raw; \
	done
	@dd if=$(CHECK_DIR)/vlen_128.raw bs=$(PIXELS) skip=0 count=1 of=$(CHECK_DIR)/vlen_128_l2.raw 2>/dev/null
	@dd if=$(CHECK_DIR)/vlen_256.raw bs=$(PIXELS) skip=0 count=1 of=$(CHECK_DIR)/vlen_256_l2.raw 2>/dev/null
	@dd if=$(CHECK_DIR)/vlen_512.raw bs=$(PIXELS) skip=0 count=1 of=$(CHECK_DIR)/vlen_512_l2.raw 2>/dev/null
	@dd if=$(CHECK_DIR)/vlen_128.raw bs=$(PIXELS) skip=1 count=1 of=$(CHECK_DIR)/vlen_128_l1.raw 2>/dev/null
	@dd if=$(CHECK_DIR)/vlen_256.raw bs=$(PIXELS) skip=1 count=1 of=$(CHECK_DIR)/vlen_256_l1.raw 2>/dev/null
	@dd if=$(CHECK_DIR)/vlen_512.raw bs=$(PIXELS) skip=1 count=1 of=$(CHECK_DIR)/vlen_512_l1.raw 2>/dev/null
	@echo "\n================= VLEN-Agnostic Equivalence ================="
	@python3 scripts/compare_raw.py $(CHECK_DIR)/vlen_128_l1.raw   $(CHECK_DIR)/vlen_256_l1.raw   0 "L1: VLEN128 vs VLEN256"
	@python3 scripts/compare_raw.py $(CHECK_DIR)/vlen_256_l1.raw   $(CHECK_DIR)/vlen_512_l1.raw   0 "L1: VLEN256 vs VLEN512"
