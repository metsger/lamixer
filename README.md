# lamixer
Simple GTK2 Alsa mixer

Compile
```sh
gcc -std=c99 -Wall lamixer.c -o lamixer `pkg-config --cflags gtk+-2.0` -lasound `pkg-config --libs gtk+-2.0`
```
