TARGET := traffic

SRC_DIR := src
OBJ_DIR := obj
BUILD_DIR := build

CXX := g++
CXXFLAGS := -Wall -Wextra -std=c++14 -Iinclude -MMD -MP


SRCS := $(wildcard $(SRC_DIR)/*.cc)
OBJS := $(patsubst $(SRC_DIR)/%.cc,$(OBJ_DIR)/%.o,$(SRCS))

EXECUTABLE := $(BUILD_DIR)/$(TARGET)

.PHONY: all clean fclean re lint

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJS) | $(BUILD_DIR)
	@echo "Linking $@"
	$(CXX) $(CXXFLAGS) $(OBJS) -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cc | $(OBJ_DIR)
	@echo "Compiling $<"
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR):
	@mkdir -p $@

$(BUILD_DIR):
	@mkdir -p $@

clean:
	@echo "Cleaning object files..."
	@rm -rf $(OBJ_DIR)

fclean: clean
	@echo "Cleaning executable..."
	@rm -rf $(BUILD_DIR)

re: fclean all

lint: fclean
	@echo "Generating compile_commands.json for linting..."
	@mkdir -p $(BUILD_DIR) # Ensure build dir exists for bear output
	@bear --output $(BUILD_DIR)/compile_commands.json -- make all

-include $(OBJS:.o=.d)