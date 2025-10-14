$(TARGET): $(OBJ_DIR_1)
	$(CC) $(LDFLAGS) -o $@ $(OBJ_DIR_1) $(LIB)

install:
	sudo cp $(TARGET) /bin/se-boot

uninstall:
	sudo rm /bin/se-boot

systemctl-install: install
	sudo chmod +x ser.sh
	sudo ./ser.sh

systemctl-uninstall:
	sudo ./ser.sh clean
