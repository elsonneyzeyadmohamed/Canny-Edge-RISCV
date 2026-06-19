# ====================================================================
# Setup Variables
# ====================================================================

CXX_RISCV ?= riscv64-unknown-elf-g++
CXX_NATIVE ?= g++

SRC_DIR := src
INC_DIR := include
BUILD_DIR := build
RESULT_DIR := results

IMG ?= tiger-animals-cat-predator-preview.jpg

WIDTH  ?= 513
HEIGHT ?= 366
SIZE   := $(WIDTH)x$(HEIGHT)
PIXELS := $(shell echo $$(( $(WIDTH) * $(HEIGHT) )))
SIZE_DEFS := -DIMG_W=$(WIDTH) -DIMG_H=$(HEIGHT)

QEMU_LD_PREFIX ?= /usr/riscv64-linux-gnu
QEMU_CPU ?= rv64,v=true,vlen=128

BASE_FLAGS_RISCV := -march=rv64gcv -Wall
CXXFLAGS_RISCV := $(BASE_FLAGS_RISCV) -O3
CXXFLAGS_NATIVE := -O3 -Wall

RVV_EXTRA_DEFS ?=

CHECK_DIR := /tmp/rvv_check
STAGE_DIR := $(RESULT_DIR)/stages_$(WIDTH)x$(HEIGHT)

.PHONY: all riscv riscv_O0 riscv_O2 riscv_O3 riscv_autovec sweep_build
.PHONY: native test run clean clean_results push
.PHONY: test_image stage_pngs sweep_run manual_rvv compare_rvv check_rvv vlen_sweep
.PHONY: open_images open_stage_images

all: riscv

# ====================================================================
# RISC-V Builds
# ====================================================================

riscv:
	mkdir -p $(BUILD_DIR)
	$(CXX_RISCV) $(CXXFLAGS_RISCV) $(SIZE_DEFS) -I$(INC_DIR) $(SRC_DIR)/*.cpp -o $(BUILD_DIR)/canny_riscv.elf

riscv_O0:
	mkdir -p $(BUILD_DIR)
	$(CXX_RISCV) $(BASE_FLAGS_RISCV) -O0 $(SIZE_DEFS) -I$(INC_DIR) $(SRC_DIR)/*.cpp -o $(BUILD_DIR)/canny_O0.elf

riscv_O2:
	mkdir -p $(BUILD_DIR)
	$(CXX_RISCV) $(BASE_FLAGS_RISCV) -O2 $(SIZE_DEFS) -I$(INC_DIR) $(SRC_DIR)/*.cpp -o $(BUILD_DIR)/canny_O2.elf

riscv_O3:
	mkdir -p $(BUILD_DIR)
	$(CXX_RISCV) $(BASE_FLAGS_RISCV) -O3 $(SIZE_DEFS) -I$(INC_DIR) $(SRC_DIR)/*.cpp -o $(BUILD_DIR)/canny_O3.elf

riscv_autovec:
	mkdir -p $(BUILD_DIR)
	$(CXX_RISCV) $(BASE_FLAGS_RISCV) -O3 -ftree-vectorize -fopt-info-vec-all $(SIZE_DEFS) -I$(INC_DIR) $(SRC_DIR)/*.cpp -o $(BUILD_DIR)/canny_autovec.elf > $(BUILD_DIR)/vector_report.txt 2>&1

sweep_build: riscv_O0 riscv_O2 riscv_O3 riscv_autovec
	@echo "\n===== Phase 4: All 4 Variants Compiled Successfully! ====="

# ====================================================================
# Google Test
# ====================================================================

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

# ====================================================================
# Basic Commands
# ====================================================================

run: riscv
	QEMU_LD_PREFIX=$(QEMU_LD_PREFIX) qemu-riscv64 -cpu $(QEMU_CPU) $(BUILD_DIR)/canny_riscv.elf

clean:
	rm -rf $(BUILD_DIR)

clean_results:
	rm -rf $(RESULT_DIR)
	rm -f /tmp/canny_stages_*.raw
	rm -f /tmp/both_raw.gray
	rm -f /tmp/scalar_output.raw /tmp/rvv_output.raw
	rm -f /tmp/scalar_log.txt /tmp/rvv_log.txt
	rm -f /tmp/nms_u16_*.raw
	rm -f /tmp/dt_out.raw /tmp/nms_out.raw /tmp/hysteresis_out.raw

NAME ?= Zeyad Elsonney
MSG ?= Update

push:
	git add .
	git commit -m "$(NAME): $(MSG)"
	git push

# ====================================================================
# Generate PNGs
# ====================================================================
# Output layout:
# skip=0 -> gaussian
# skip=1 -> mag_l2
# skip=2 -> mag_l1
# skip=3 -> direction
# skip=4 and skip=5 -> nms_out uint16_t
# skip=6 -> double_threshold
# skip=7 -> final_hysteresis

test_image: riscv
	@mkdir -p $(RESULT_DIR)
	@test -f $(IMG) || (echo "ERROR: input image not found: $(IMG)"; exit 1)
	@echo "Processing $(IMG) at $(SIZE)..."
	@convert $(IMG) -resize $(SIZE)! -colorspace gray -depth 8 gray:- | \
	QEMU_LD_PREFIX=$(QEMU_LD_PREFIX) \
	qemu-riscv64 -cpu $(QEMU_CPU) $(BUILD_DIR)/canny_riscv.elf \
	> /tmp/both_raw.gray

	@echo "Creating result_gaussian.png..."
	@dd if=/tmp/both_raw.gray bs=$(PIXELS) count=1 skip=0 2>/dev/null | \
	convert -size $(SIZE) -depth 8 gray:- $(RESULT_DIR)/result_gaussian.png

	@echo "Creating result_magnitude_L2.png..."
	@dd if=/tmp/both_raw.gray bs=$(PIXELS) count=1 skip=1 2>/dev/null | \
	convert -size $(SIZE) -depth 8 gray:- $(RESULT_DIR)/result_magnitude_L2.png

	@echo "Creating result_magnitude_L1.png..."
	@dd if=/tmp/both_raw.gray bs=$(PIXELS) count=1 skip=2 2>/dev/null | \
	convert -size $(SIZE) -depth 8 gray:- $(RESULT_DIR)/result_magnitude_L1.png

	@echo "Skipping result_direction.png..."

	@echo "Creating result_nms.png..."
	@dd if=/tmp/both_raw.gray bs=$(PIXELS) count=2 skip=4 of=/tmp/nms_u16_$(WIDTH)x$(HEIGHT).raw 2>/dev/null
	@convert -size $(SIZE) -depth 16 -endian LSB gray:/tmp/nms_u16_$(WIDTH)x$(HEIGHT).raw -auto-level $(RESULT_DIR)/result_nms.png

	@echo "Creating result_double_threshold.png..."
	@dd if=/tmp/both_raw.gray bs=$(PIXELS) count=1 skip=6 2>/dev/null | \
	convert -size $(SIZE) -depth 8 gray:- $(RESULT_DIR)/result_double_threshold.png

	@echo "Creating result_hysteresis.png..."
	@dd if=/tmp/both_raw.gray bs=$(PIXELS) count=1 skip=7 2>/dev/null | \
	convert -size $(SIZE) -depth 8 gray:- $(RESULT_DIR)/result_hysteresis.png

	@echo "\nGenerated main PNGs:"
	@ls -lh $(RESULT_DIR)/*.png
	@echo "Done! Results saved in $(RESULT_DIR)/"

stage_pngs: riscv
	@mkdir -p $(STAGE_DIR)
	@test -f $(IMG) || (echo "ERROR: input image not found: $(IMG)"; exit 1)
	@echo "Processing $(IMG) at $(SIZE)..."
	@convert $(IMG) -resize $(SIZE)! -colorspace gray -depth 8 gray:- | \
	QEMU_LD_PREFIX=$(QEMU_LD_PREFIX) \
	qemu-riscv64 -cpu $(QEMU_CPU) $(BUILD_DIR)/canny_riscv.elf \
	> /tmp/canny_stages_$(WIDTH)x$(HEIGHT).raw 2> $(STAGE_DIR)/stage_log.txt

	@echo "Creating 00_gaussian.png..."
	@dd if=/tmp/canny_stages_$(WIDTH)x$(HEIGHT).raw bs=$(PIXELS) count=1 skip=0 2>/dev/null | \
	convert -size $(SIZE) -depth 8 gray:- $(STAGE_DIR)/00_gaussian.png

	@echo "Creating 01_magnitude_L2.png..."
	@dd if=/tmp/canny_stages_$(WIDTH)x$(HEIGHT).raw bs=$(PIXELS) count=1 skip=1 2>/dev/null | \
	convert -size $(SIZE) -depth 8 gray:- $(STAGE_DIR)/01_magnitude_L2.png

	@echo "Creating 02_magnitude_L1.png..."
	@dd if=/tmp/canny_stages_$(WIDTH)x$(HEIGHT).raw bs=$(PIXELS) count=1 skip=2 2>/dev/null | \
	convert -size $(SIZE) -depth 8 gray:- $(STAGE_DIR)/02_magnitude_L1.png

	@echo "Skipping 03_direction.png..."

	@echo "Creating 04_nms.png..."
	@dd if=/tmp/canny_stages_$(WIDTH)x$(HEIGHT).raw bs=$(PIXELS) count=2 skip=4 of=/tmp/nms_u16_$(WIDTH)x$(HEIGHT).raw 2>/dev/null
	@convert -size $(SIZE) -depth 16 -endian LSB gray:/tmp/nms_u16_$(WIDTH)x$(HEIGHT).raw -auto-level $(STAGE_DIR)/04_nms.png

	@echo "Creating 05_double_threshold.png..."
	@dd if=/tmp/canny_stages_$(WIDTH)x$(HEIGHT).raw bs=$(PIXELS) count=1 skip=6 2>/dev/null | \
	convert -size $(SIZE) -depth 8 gray:- $(STAGE_DIR)/05_double_threshold.png

	@echo "Creating 06_final_hysteresis.png..."
	@dd if=/tmp/canny_stages_$(WIDTH)x$(HEIGHT).raw bs=$(PIXELS) count=1 skip=7 2>/dev/null | \
	convert -size $(SIZE) -depth 8 gray:- $(STAGE_DIR)/06_final_hysteresis.png

	@echo "\nGenerated stage PNGs:"
	@ls -lh $(STAGE_DIR)/*.png
	@echo "Done! Stage PNGs saved in $(STAGE_DIR)/"

open_images:
	@if command -v explorer.exe >/dev/null 2>&1; then \
		explorer.exe results; \
	elif command -v xdg-open >/dev/null 2>&1; then \
		xdg-open results; \
	else \
		echo "Open this folder manually: $(PWD)/results"; \
	fi

open_stage_images:
	@if command -v explorer.exe >/dev/null 2>&1; then \
		explorer.exe $(STAGE_DIR); \
	elif command -v xdg-open >/dev/null 2>&1; then \
		xdg-open $(STAGE_DIR); \
	else \
		echo "Open this folder manually: $(PWD)/$(STAGE_DIR)"; \
	fi

# ====================================================================
# Phase 4 Sweep
# ====================================================================

sweep_run: sweep_build
	@mkdir -p $(RESULT_DIR)
	@test -f $(IMG) || (echo "ERROR: input image not found: $(IMG)"; exit 1)

	@echo "\n========================================================="
	@echo " RUNNING OPTIMIZATION SWEEP"
	@echo " Image size: $(SIZE)"
	@echo "========================================================="

	@echo "\n>>> Running -O0 Binary:"
	@convert $(IMG) -resize $(SIZE)! -colorspace gray -depth 8 gray:- | \
	QEMU_LD_PREFIX=$(QEMU_LD_PREFIX) qemu-riscv64 -cpu $(QEMU_CPU) $(BUILD_DIR)/canny_O0.elf > /dev/null

	@echo "\n>>> Running -O2 Binary:"
	@convert $(IMG) -resize $(SIZE)! -colorspace gray -depth 8 gray:- | \
	QEMU_LD_PREFIX=$(QEMU_LD_PREFIX) qemu-riscv64 -cpu $(QEMU_CPU) $(BUILD_DIR)/canny_O2.elf > /dev/null

	@echo "\n>>> Running -O3 Binary:"
	@convert $(IMG) -resize $(SIZE)! -colorspace gray -depth 8 gray:- | \
	QEMU_LD_PREFIX=$(QEMU_LD_PREFIX) qemu-riscv64 -cpu $(QEMU_CPU) $(BUILD_DIR)/canny_O3.elf > /dev/null

	@echo "\n>>> Running Auto-Vectorized Binary:"
	@convert $(IMG) -resize $(SIZE)! -colorspace gray -depth 8 gray:- | \
	QEMU_LD_PREFIX=$(QEMU_LD_PREFIX) qemu-riscv64 -cpu $(QEMU_CPU) $(BUILD_DIR)/canny_autovec.elf > /dev/null

	@echo "\n========================================================="

# ====================================================================
# Phase 6 Manual RVV
# ====================================================================

manual_rvv:
	mkdir -p $(BUILD_DIR)
	$(CXX_RISCV) -march=rv64gcv -Wall -O3 \
	-DUSE_MANUAL_RVV -DUSE_RVV_GAUSSIAN $(RVV_EXTRA_DEFS) $(SIZE_DEFS) \
	-I$(INC_DIR) $(SRC_DIR)/*.cpp -o $(BUILD_DIR)/canny_manual_rvv.elf

compare_rvv: riscv manual_rvv
	@test -f $(IMG) || (echo "ERROR: input image not found: $(IMG)"; exit 1)

	@echo "\n================= SCALAR no manual RVV ================="
	@echo "Image size: $(SIZE)"
	@convert $(IMG) -resize $(SIZE)! -colorspace gray -depth 8 gray:- | \
	QEMU_LD_PREFIX=$(QEMU_LD_PREFIX) \
	qemu-riscv64 -cpu $(QEMU_CPU) $(BUILD_DIR)/canny_riscv.elf > /dev/null

	@echo "\n================= MANUAL RVV ================="
	@echo "Image size: $(SIZE)"
	@convert $(IMG) -resize $(SIZE)! -colorspace gray -depth 8 gray:- | \
	QEMU_LD_PREFIX=$(QEMU_LD_PREFIX) \
	qemu-riscv64 -cpu $(QEMU_CPU) $(BUILD_DIR)/canny_manual_rvv.elf > /dev/null

check_rvv: manual_rvv
	mkdir -p $(BUILD_DIR) $(RESULT_DIR)
	$(CXX_RISCV) -march=rv64gcv -Wall -O3 $(SIZE_DEFS) \
	-I$(INC_DIR) $(SRC_DIR)/*.cpp -o $(BUILD_DIR)/canny_O3.elf

	rm -f /tmp/scalar_output.raw /tmp/rvv_output.raw
	rm -f /tmp/scalar_log.txt /tmp/rvv_log.txt

	@test -f $(IMG) || (echo "ERROR: input image not found: $(IMG)"; exit 1)

	convert $(IMG) -resize $(SIZE)! -colorspace gray -depth 8 gray:- | \
	QEMU_LD_PREFIX=$(QEMU_LD_PREFIX) qemu-riscv64 -cpu $(QEMU_CPU) $(BUILD_DIR)/canny_O3.elf \
	> /tmp/scalar_output.raw 2> /tmp/scalar_log.txt

	test -s /tmp/scalar_output.raw || (echo "Scalar output is empty"; cat /tmp/scalar_log.txt; exit 1)

	convert $(IMG) -resize $(SIZE)! -colorspace gray -depth 8 gray:- | \
	QEMU_LD_PREFIX=$(QEMU_LD_PREFIX) qemu-riscv64 -cpu $(QEMU_CPU) $(BUILD_DIR)/canny_manual_rvv.elf \
	> /tmp/rvv_output.raw 2> /tmp/rvv_log.txt

	test -s /tmp/rvv_output.raw || (echo "RVV output is empty"; cat /tmp/rvv_log.txt; exit 1)

	python3 scripts/compare_raw_outputs.py /tmp/scalar_output.raw /tmp/rvv_output.raw $(WIDTH) $(HEIGHT) | tee $(RESULT_DIR)/rvv_comparison_report.txt

	cp /tmp/scalar_log.txt $(RESULT_DIR)/scalar_check_log.txt
	cp /tmp/rvv_log.txt $(RESULT_DIR)/rvv_check_log.txt
	cp /tmp/scalar_output.raw $(RESULT_DIR)/scalar_output.raw
	cp /tmp/rvv_output.raw $(RESULT_DIR)/rvv_output.raw

vlen_sweep: manual_rvv
	@mkdir -p $(CHECK_DIR)
	@test -f $(IMG) || (echo "ERROR: input image not found: $(IMG)"; exit 1)

	@for V in 128 256 512; do \
		echo "\n================= VLEN=$$V ================="; \
		convert $(IMG) -resize $(SIZE)! -colorspace gray -depth 8 gray:- | \
		QEMU_LD_PREFIX=$(QEMU_LD_PREFIX) \
		qemu-riscv64 -cpu rv64,v=true,vlen=$$V $(BUILD_DIR)/canny_manual_rvv.elf \
		> $(CHECK_DIR)/vlen_$$V.raw; \
	done

	@dd if=$(CHECK_DIR)/vlen_128.raw bs=$(PIXELS) skip=0 count=1 of=$(CHECK_DIR)/vlen_128_gaussian.raw 2>/dev/null
	@dd if=$(CHECK_DIR)/vlen_256.raw bs=$(PIXELS) skip=0 count=1 of=$(CHECK_DIR)/vlen_256_gaussian.raw 2>/dev/null
	@dd if=$(CHECK_DIR)/vlen_512.raw bs=$(PIXELS) skip=0 count=1 of=$(CHECK_DIR)/vlen_512_gaussian.raw 2>/dev/null

	@dd if=$(CHECK_DIR)/vlen_128.raw bs=$(PIXELS) skip=1 count=1 of=$(CHECK_DIR)/vlen_128_l2.raw 2>/dev/null
	@dd if=$(CHECK_DIR)/vlen_256.raw bs=$(PIXELS) skip=1 count=1 of=$(CHECK_DIR)/vlen_256_l2.raw 2>/dev/null
	@dd if=$(CHECK_DIR)/vlen_512.raw bs=$(PIXELS) skip=1 count=1 of=$(CHECK_DIR)/vlen_512_l2.raw 2>/dev/null

	@dd if=$(CHECK_DIR)/vlen_128.raw bs=$(PIXELS) skip=2 count=1 of=$(CHECK_DIR)/vlen_128_l1.raw 2>/dev/null
	@dd if=$(CHECK_DIR)/vlen_256.raw bs=$(PIXELS) skip=2 count=1 of=$(CHECK_DIR)/vlen_256_l1.raw 2>/dev/null
	@dd if=$(CHECK_DIR)/vlen_512.raw bs=$(PIXELS) skip=2 count=1 of=$(CHECK_DIR)/vlen_512_l1.raw 2>/dev/null

	@dd if=$(CHECK_DIR)/vlen_128.raw bs=$(PIXELS) skip=3 count=1 of=$(CHECK_DIR)/vlen_128_direction.raw 2>/dev/null
	@dd if=$(CHECK_DIR)/vlen_256.raw bs=$(PIXELS) skip=3 count=1 of=$(CHECK_DIR)/vlen_256_direction.raw 2>/dev/null
	@dd if=$(CHECK_DIR)/vlen_512.raw bs=$(PIXELS) skip=3 count=1 of=$(CHECK_DIR)/vlen_512_direction.raw 2>/dev/null

	@dd if=$(CHECK_DIR)/vlen_128.raw bs=$(PIXELS) skip=4 count=2 of=$(CHECK_DIR)/vlen_128_nms.raw 2>/dev/null
	@dd if=$(CHECK_DIR)/vlen_256.raw bs=$(PIXELS) skip=4 count=2 of=$(CHECK_DIR)/vlen_256_nms.raw 2>/dev/null
	@dd if=$(CHECK_DIR)/vlen_512.raw bs=$(PIXELS) skip=4 count=2 of=$(CHECK_DIR)/vlen_512_nms.raw 2>/dev/null

	@dd if=$(CHECK_DIR)/vlen_128.raw bs=$(PIXELS) skip=6 count=1 of=$(CHECK_DIR)/vlen_128_dt.raw 2>/dev/null
	@dd if=$(CHECK_DIR)/vlen_256.raw bs=$(PIXELS) skip=6 count=1 of=$(CHECK_DIR)/vlen_256_dt.raw 2>/dev/null
	@dd if=$(CHECK_DIR)/vlen_512.raw bs=$(PIXELS) skip=6 count=1 of=$(CHECK_DIR)/vlen_512_dt.raw 2>/dev/null

	@dd if=$(CHECK_DIR)/vlen_128.raw bs=$(PIXELS) skip=7 count=1 of=$(CHECK_DIR)/vlen_128_final.raw 2>/dev/null
	@dd if=$(CHECK_DIR)/vlen_256.raw bs=$(PIXELS) skip=7 count=1 of=$(CHECK_DIR)/vlen_256_final.raw 2>/dev/null
	@dd if=$(CHECK_DIR)/vlen_512.raw bs=$(PIXELS) skip=7 count=1 of=$(CHECK_DIR)/vlen_512_final.raw 2>/dev/null

	@echo "\n================= VLEN-Agnostic Equivalence ================="

	@python3 scripts/compare_raw.py $(CHECK_DIR)/vlen_128_gaussian.raw $(CHECK_DIR)/vlen_256_gaussian.raw 0 "Gaussian: VLEN128 vs VLEN256"
	@python3 scripts/compare_raw.py $(CHECK_DIR)/vlen_256_gaussian.raw $(CHECK_DIR)/vlen_512_gaussian.raw 0 "Gaussian: VLEN256 vs VLEN512"

	@python3 scripts/compare_raw.py $(CHECK_DIR)/vlen_128_l2.raw $(CHECK_DIR)/vlen_256_l2.raw 0 "L2: VLEN128 vs VLEN256"
	@python3 scripts/compare_raw.py $(CHECK_DIR)/vlen_256_l2.raw $(CHECK_DIR)/vlen_512_l2.raw 0 "L2: VLEN256 vs VLEN512"

	@python3 scripts/compare_raw.py $(CHECK_DIR)/vlen_128_l1.raw $(CHECK_DIR)/vlen_256_l1.raw 0 "L1: VLEN128 vs VLEN256"
	@python3 scripts/compare_raw.py $(CHECK_DIR)/vlen_256_l1.raw $(CHECK_DIR)/vlen_512_l1.raw 0 "L1: VLEN256 vs VLEN512"

	@python3 scripts/compare_raw.py $(CHECK_DIR)/vlen_128_direction.raw $(CHECK_DIR)/vlen_256_direction.raw 0 "Direction: VLEN128 vs VLEN256"
	@python3 scripts/compare_raw.py $(CHECK_DIR)/vlen_256_direction.raw $(CHECK_DIR)/vlen_512_direction.raw 0 "Direction: VLEN256 vs VLEN512"

	@python3 scripts/compare_raw.py $(CHECK_DIR)/vlen_128_nms.raw $(CHECK_DIR)/vlen_256_nms.raw 0 "NMS: VLEN128 vs VLEN256"
	@python3 scripts/compare_raw.py $(CHECK_DIR)/vlen_256_nms.raw $(CHECK_DIR)/vlen_512_nms.raw 0 "NMS: VLEN256 vs VLEN512"

	@python3 scripts/compare_raw.py $(CHECK_DIR)/vlen_128_dt.raw $(CHECK_DIR)/vlen_256_dt.raw 0 "Double Threshold: VLEN128 vs VLEN256"
	@python3 scripts/compare_raw.py $(CHECK_DIR)/vlen_256_dt.raw $(CHECK_DIR)/vlen_512_dt.raw 0 "Double Threshold: VLEN256 vs VLEN512"

	@python3 scripts/compare_raw.py $(CHECK_DIR)/vlen_128_final.raw $(CHECK_DIR)/vlen_256_final.raw 0 "Final: VLEN128 vs VLEN256"
	@python3 scripts/compare_raw.py $(CHECK_DIR)/vlen_256_final.raw $(CHECK_DIR)/vlen_512_final.raw 0 "Final: VLEN256 vs VLEN512"
