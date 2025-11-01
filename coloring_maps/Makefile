CC = gcc
CFLAGS = -Wall -std=c99 `sdl2-config --cflags`
LDFLAGS = `sdl2-config --libs` -lSDL2_image -lSDL2_ttf

TARGET = main
SRC = main.c

.PHONY: all
all: $(TARGET)
	@echo "	 To run the program, use one of these commands:"
	@echo "  ./main                                        - Graphical mode (interactive)"
	@echo "  ./main maps/input.bmp output_maps/output.bmp - Console mode (with file processing)"
	@echo "\n===================================================================================================================================================="
	@echo "Available maps in 'maps/' directory:"
	@ls maps/*.bmp 2>/dev/null || echo "  (No map files found in 'maps/' directory)"
	@echo "Save maps to the 'output_maps/' directory:"
	@echo "when the painting process is complete, click ESC"
	@echo "===================================================================================================================================================="

$(TARGET): $(SRC)
	@$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

.PHONY: clean
clean:
	@rm -f $(TARGET)