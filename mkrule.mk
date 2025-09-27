$(TARGET): $(OBJ_DIR_1)
	$(CC) $(LDFLAGS) -o $@ $(OBJ_DIR_1) $(LIB)
