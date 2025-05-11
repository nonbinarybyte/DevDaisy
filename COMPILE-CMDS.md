# WINDOWS FROM LINUX

```bash
sudo apt install mingw-w64
export PKG_CONFIG_PATH=/path/to/gtk-windows-bundle/lib/pkgconfig
x86_64-w64-mingw32-gcc -o devdaisy.exe dev-daisy.c --static pkg-config --cflags --libs gtk+-3.0 gtksourceview-4
```

# LINUX FROM LINUX

```bash
gcc -o devdaisy dev-daisy.c --static pkg-config --cflags --libs gtk+-3.0 gtksourceview-4
./devdaisy
```
