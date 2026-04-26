# --- Setup Variables ---
CXX_RISCV  = riscv64-unknown-elf-g++
CXX_NATIVE = g++

# Compiler Flags
CXXFLAGS_RISCV  = -march=rv64gcv -O3 -Wall
CXXFLAGS_NATIVE = -O3 -Wall

# Directories
SRC_DIR   = src
INC_DIR   = include
BUILD_DIR = build

# --- Targets ---

# 1. Compile for RISC-V
riscv:
	mkdir -p $(BUILD_DIR)
	$(CXX_RISCV) $(CXXFLAGS_RISCV) -I$(INC_DIR) $(SRC_DIR)/*.cpp -o $(BUILD_DIR)/canny_riscv.elf

# 2. Compile and run GoogleTest on host
native:
	mkdir -p $(BUILD_DIR)
	$(CXX_NATIVE) -O2 -I$(INC_DIR) tests/test_canny.cpp -o $(BUILD_DIR)/test_runner -lgtest -lgtest_main -lpthread

test: native
	./$(BUILD_DIR)/test_runner

# 3. Run on QEMU
run:
	qemu-riscv64 -cpu rv64,v=true,vlen=128 $(BUILD_DIR)/canny_riscv.elf

clean:
	rm -rf $(BUILD_DIR)

# --- Git Automation ---
# Usage: make push NAME=Ahmed MSG="Fixed the blur"
NAME ?= AhmedAshraf
MSG  ?= Update

push:
	git add .
	git commit -m "$(NAME): $(MSG)"
	git push

# --- Image Processing ---
# Usage: make test_image IMG=tiger.jpg
IMG        ?= tiger-animals-cat-predator-preview.jpg
RESULT_DIR  = results
L2_OUT      = $(RESULT_DIR)/result_L2.png
L1_OUT      = $(RESULT_DIR)/result_L1.png

test_image: riscv
	@mkdir -p $(RESULT_DIR)
	@echo "Processing $(IMG)..."
	@convert $(IMG) -resize 512x512! -colorspace gray -depth 8 gray:- | \
	qemu-riscv64 -cpu rv64,v=true,vlen=128 $(BUILD_DIR)/canny_riscv.elf > /tmp/both_raw.gray
	@dd if=/tmp/both_raw.gray bs=262144 count=1 | \
	convert -size 512x512 -depth 8 gray:- $(RESULT_DIR)/result_L2.png
	@dd if=/tmp/both_raw.gray bs=262144 skip=1 count=1 | \
	convert -size 512x512 -depth 8 gray:- $(RESULT_DIR)/result_L1.png
	@echo "Done! L2: $(RESULT_DIR)/result_L2.png | L1: $(RESULT_DIR)/result_L1.png"