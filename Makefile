CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++11 -Iinclude

SRC_DIR := src
OBJ_DIR := build
BUILD_DIR := $(OBJ_DIR)

SRC_FILES := $(wildcard $(SRC_DIR)/*.cc)
OBJ_FILES := $(SRC_FILES:$(SRC_DIR)/%.cc=$(OBJ_DIR)/%.o)

BIN_ANIMAL := animal
BIN_BOOK := book

all: $(BIN_ANIMAL) $(BIN_BOOK)

$(BIN_ANIMAL): $(OBJ_DIR)/animal.o
	$(CXX) $(CXXFLAGS) -o $@ $^

$(BIN_BOOK): $(OBJ_DIR)/book.o
	$(CXX) $(CXXFLAGS) -o $@ $^

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cc $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR):
	@mkdir -p

clean:
	rm -rf $(OBJ_DIR)

fclean: clean
	rm -f $(BIN_ANIMAL) $(BIN_BOOK) $(BUILD_DIR)/compile_commands.json

re: fclean all

# for clangd
lint: clean
	@mkdir -p $(BUILD_DIR)
	@BEAR_OUTPUT_DIR=$(BUILD_DIR) bear --output $(BUILD_DIR)/compile_commands.json -- make all

.PHONY: all clean fclean re lint
