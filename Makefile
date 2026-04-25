# --- Setup Variables ---
# The compiler we just built and put in our PATH
CXX_RISCV = riscv64-unknown-elf-g++
# The standard compiler on your WSL/Linux for testing
CXX_NATIVE = g++

# Compiler Flags
# -march=rv64gcv tells the compiler to use the Vector (v) extension
CXXFLAGS_RISCV = -march=rv64gcv -O3 -Wall
CXXFLAGS_NATIVE = -O3 -Wall -lgtest -lgtest_main -lpthread

# Directories
SRC_DIR = src
INC_DIR = include
BUILD_DIR = build

# --- Targets ---

# 1. Compile for RISC-V (The Project Goal)
riscv:
	mkdir -p $(BUILD_DIR)
	$(CXX_RISCV) $(CXXFLAGS_RISCV) -I$(INC_DIR) $(SRC_DIR)/*.cpp -o $(BUILD_DIR)/canny_riscv.elf

# 2. Compile for your PC (For Google Test)
native:
	mkdir -p $(BUILD_DIR)
	$(CXX_NATIVE) $(CXXFLAGS_NATIVE) -I$(INC_DIR) $(SRC_DIR)/*.cpp tests/*.cpp -o $(BUILD_DIR)/test_runner

# 3. Run the RISC-V code on QEMU
# cpu=rv64,v=true enables the vector engine in the simulator
run:
	qemu-riscv64 -cpu rv64,v=true,vlen=128 $(BUILD_DIR)/canny_riscv.elf

clean:
	rm -rf $(BUILD_DIR)
# Fast push 
# --- Git Automation ---
# Usage: make push NAME=Ahmed MSG="Fixed the blur"
# Default name is Zeyad if you don't provide one
NAME ?= Zeyad
MSG ?= Update

push:
	git add .
	git commit -m "Architect ($(NAME)): $(MSG)"
	git push

# --- Image Processing Automation ---
# Usage: make test_image IMG=tiger-animals-cat-predator-preview.jpg
IMG ?= tiger-animals-cat-predator-preview.jpg
RESULT_DIR = results
FINAL_OUT = $(RESULT_DIR)/final_result.png

test_image: riscv
	@mkdir -p $(RESULT_DIR)
	@echo "Processing $(IMG)..."
	@convert $(IMG) -resize 512x512! -colorspace gray -depth 8 gray:- | \
	qemu-riscv64 -cpu rv64,v=true,vlen=128 $(BUILD_DIR)/canny_riscv.elf | \
	convert -size 512x512 -depth 8 gray:- $(FINAL_OUT)
	@echo "Done! Result saved in: $(FINAL_OUT)"