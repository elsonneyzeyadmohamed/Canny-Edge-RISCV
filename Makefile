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

manual_rvv:
	mkdir -p build
	riscv64-unknown-elf-g++ -march=rv64gcv -Wall -O3 -DUSE_MANUAL_RVV -Iinclude src/*.cpp -o build/canny_manual_rvv.elf
