CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++14 -Iinclude

SRC_DIR := src
OBJ_DIR := build
BUILD_DIR := $(OBJ_DIR)

animal_src       := animal.cc
book_src         := book.cc
animalProxy_src  := animalProxy.cc

TARGETS := animal book animalProxy

animal_obj       := $(addprefix $(OBJ_DIR)/, $(animal_src:.cc=.o))
book_obj         := $(addprefix $(OBJ_DIR)/, $(book_src:.cc=.o))
animalProxy_obj  := $(addprefix $(OBJ_DIR)/, $(animalProxy_src:.cc=.o))

animal: $(animal_obj)
	$(CXX) $(CXXFLAGS) -o $@ $^

book: $(book_obj)
	$(CXX) $(CXXFLAGS) -o $@ $^

animalProxy: $(animalProxy_obj)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cc | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR):
	@mkdir -p $@

all: $(TARGETS)

clean:
	rm -rf $(OBJ_DIR)

fclean: clean
	rm -f $(TARGETS) $(BUILD_DIR)/compile_commands.json

re: fclean all

lint: clean
	@mkdir -p $(BUILD_DIR)
	@BEAR_OUTPUT_DIR=$(BUILD_DIR) bear --output $(BUILD_DIR)/compile_commands.json -- make all

.PHONY: all clean fclean re lint
