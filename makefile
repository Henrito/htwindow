CC=cc
OPTIONS=std=c89 g
WARNINGS=all extra pedantic no-four-char-constants no-multichar
DEFINES=GL_SILENCE_DEPRECATION
ASSEMBLY=fverbose-asm
SRC_DIR=src
INC_DIR=include include/$(INCLUDE_DIR) ../include
ASM_DIR=asm
LIB_DIR=lib
ifeq ($(shell uname -s), Darwin)
	INCLUDE_DIR=cocoa
	OPTIONS+=fno-objc-arc
else
	INCLUDE_DIR=x11
	INC_DIR+=/opt/X11/include
endif

CFLAGS:=$(foreach flag,$(OPTIONS) $(foreach flag,$(WARNINGS),W$(flag)) $(foreach flag,$(DEFINES),D$(flag)),-$(flag))
AFLAGS:=$(foreach flag,$(ASSEMBLY),-$(flag))
INC:=$(foreach flag,$(INC_DIR),-I$(flag))
SRC:=$(filter $(SRC_DIR)/$(addsuffix /%,$(INCLUDE_DIR)),$(shell find $(SRC_DIR) -name '*.c'))
OBJ_DIR:=build/$(shell uname -s)
OBJ:=$(foreach file,$(notdir $(SRC:$(SRC_DIR)/%.c=%.o)),$(OBJ_DIR)/$(file))
ASM:=$(foreach file,$(notdir $(SRC:$(SRC_DIR)/%.c=%.s)),$(ASM_DIR)/$(file))
LIB:=$(notdir $(shell pwd))_$(shell uname -s).a

ifeq ($(shell uname -s), Darwin)
	CFLAGS+=-isysroot $(shell xcrun --sdk macosx --show-sdk-path)
	ARCHIVE=libtool -static -o
else
	ARCHIVE=ar -rcs
	RANLIB=ranlib $(LIB_DIR)/$(LIB)
endif

.PHONY: all clean asm dir asm_dir help

all: dir $(LIB_DIR)/$(LIB) ## Build the library file

clean: ## Remove files created from previous build
	rm -rf $(LIB_DIR) $(OBJ_DIR) $(ASM_DIR)

asm: asm_dir $(ASM) ## Compile assembly files

dir:
	@mkdir -p $(LIB_DIR)
	@mkdir -p $(OBJ_DIR)

asm_dir:
	@mkdir -p $(ASM_DIR)

help:
	@grep -E '^[a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | \
		sort | \
		awk 'BEGIN {FS = ":.*?## "}; {printf "\033[32m%-8s\033[0m %s\n", $$1, $$2}'

$(LIB_DIR)/$(LIB): $(OBJ)
	$(ARCHIVE) $@ $(OBJ)
	$(RANLIB)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c 
	$(CC) -o $@ $(CFLAGS) $(INC) -c $<

$(OBJ_DIR)/%.o: $(SRC_DIR)/$(INCLUDE_DIR)/%.c
	$(CC) -o $@ $(CFLAGS) $(INC) -c $<

$(ASM_DIR)/%.s: $(SRC_DIR)/%.c
	$(CC) -o $@ $(CFLAGS) $(INC) -S $<

$(ASM_DIR)/%.s: $(SRC_DIR)/$(INCLUDE_DIR)/%.c
	$(CC) -o $@ $(CFLAGS) $(INC) $(AFLAGS) -S $<
