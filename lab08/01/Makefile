CXX      := g++
CXXFLAGS := -Wall -O2 -I include -MMD -MP

TARGET     := build/calc
SRC_DIR    := src
OBJ_DIR    := obj
BUILD_DIR  := build
INCLUDE_DIR:= include

SRCS := $(wildcard $(SRC_DIR)/*.cc)
OBJS := $(patsubst $(SRC_DIR)/%.cc, $(OBJ_DIR)/%.o, $(SRCS))
DEPS := $(OBJS:.o=.d)

all: $(TARGET)

$(TARGET): $(OBJS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cc | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR):
	@mkdir -p $(OBJ_DIR)

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

lint: clean | $(BUILD_DIR)
	@echo "Generating compile_commands.json using bear..."
	bear -- make -B all
	@mv compile_commands.json $(BUILD_DIR)/

clean:
	@rm -f $(OBJS) $(DEPS)

fclean: clean
	@rm -f $(TARGET) $(BUILD_DIR)/compile_commands.json

re: fclean all

-include $(DEPS)

.PHONY: all clean fclean re lint
