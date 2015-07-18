# lamixer
Simple GTK2 Alsa mixer

Compile
```sh
gcc -Wall simple.c -o simple `pkg-config --cflags gtk+-2.0` -lasound `pkg-config --libs gtk+-2.0`
```
