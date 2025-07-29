CXX      ?= g++
CXXFLAGS ?= -Wall -Wextra -std=c++14 -MMD -MP

SRC_DIR  ?= src
OBJ_DIR  ?= obj
BUILD_DIR ?= build

ifndef TARGETS
$(error TARGETS not defined.)
endif

define collect_target_objs
OBJ_$(1) := $(addprefix $(OBJ_DIR)/, $(notdir $(SRCS_$(1):%.cc=%.o)))
OBJ_FILES += $$(OBJ_$(1))
endef

$(foreach tgt,$(TARGETS),$(eval $(call collect_target_objs,$(tgt))))

all: $(addprefix $(BUILD_DIR)/, $(TARGETS))

$(foreach tgt,$(TARGETS),\
  $(eval $(BUILD_DIR)/$(tgt): $$(OBJ_$(tgt)) ; $$(CXX) $$(CXXFLAGS) $$^ -o $$@))

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cc | $(OBJ_DIR) $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR):
	@mkdir -p $@

$(BUILD_DIR):
	@mkdir -p $@

clean:
	rm -rf $(OBJ_DIR)

fclean: clean
	rm -rf $(BUILD_DIR)

re: fclean all

lint: clean $(BUILD_DIR)
	@bear --output $(BUILD_DIR)/compile_commands.json -- make all

.PHONY: all clean fclean re lint

-include $(OBJ_FILES:.o=.d)
