CC = gcc
CFLAGS = `pkg-config --cflags gtk+-3.0 gtksourceview-4`
LIBS = `pkg-config --libs gtk+-3.0 gtksourceview-4`
SRC = dev-daisy.c
OUT = dev-daisy

all: $(OUT)

$(OUT): $(SRC)
	$(CC) $(CFLAGS) -o $(OUT) $(SRC) $(LIBS)

clean:
	rm -f $(OUT)
 
