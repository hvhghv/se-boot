include mkenv.mk

all: $(TARGET)

SRC += $(wildcard $(TOP)/se-boot-src/*.cpp)
SRC_DIR_1 += $(wildcard $(TOP)/se-boot-src/*.cpp)
OBJ += $(patsubst %.cpp, $(BUILD)/se-boot-src/%.cpp.o, $(notdir $(wildcard $(TOP)/se-boot-src/*.cpp)))
OBJ_DIR_1 += $(patsubst %.cpp, $(BUILD)/se-boot-src/%.cpp.o, $(notdir $(wildcard $(TOP)/se-boot-src/*.cpp)))

SRC += $(wildcard $(TOP)/se-boot-src/*.c)
SRC_DIR_1 += $(wildcard $(TOP)/se-boot-src/*.c)
OBJ += $(patsubst %.c, $(BUILD)/se-boot-src/%.c.o,  $(notdir $(wildcard $(TOP)/se-boot-src/*.c)))
OBJ_DIR_1 += $(patsubst %.c, $(BUILD)/se-boot-src/%.c.o,  $(notdir $(wildcard $(TOP)/se-boot-src/*.c)))

SRC += $(wildcard $(TOP)/se-boot-src/*.S)
SRC_DIR_1 += $(wildcard $(TOP)/se-boot-src/*.S)
OBJ += $(patsubst %.S, $(BUILD)/se-boot-src/%.S.o,  $(notdir $(wildcard $(TOP)/se-boot-src/*.S)))
OBJ_DIR_1 += $(patsubst %.S, $(BUILD)/se-boot-src/%.S.o,  $(notdir $(wildcard $(TOP)/se-boot-src/*.S)))

$(BUILD)/se-boot-src/%.cpp.o: $(TOP)/se-boot-src/%.cpp $(BUILD) $(PREBUILD)
	$(CXX) -c $(CXXFLAGS) $(INC) -o $@ $<

$(BUILD)/se-boot-src/%.c.o:  $(TOP)/se-boot-src/%.c $(BUILD) $(PREBUILD)
	$(CC) -c $(CFLAGS) $(INC) -o $@ $<

$(BUILD)/se-boot-src/%.S.o:  $(TOP)/se-boot-src/%.S $(BUILD) $(PREBUILD)
	$(AS) -c $(ASFLAGS) $(INC) -o $@ $<

$(BUILD):
	$(MKDIR) -p $(BUILD)/se-boot-src

clean:
	rm -rf $(BUILD)

totalclean: clean
	rm -rf makefile

include mkrule.mk
