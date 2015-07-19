#include <alsa/asoundlib.h>
#include <iostream>

int main()
{
    size_t start, end;
    std::string desc;
    void ** hints, ** str_c;
    char * name_c, * desc_c;
    int index = 0;
    const char * ifaces[] = {
        "card", "pcm", "rawmidi", "timer", "seq", "hwdep", 0
    };

    snd_config_update();

    while (ifaces[index]) {
        std::cout << "---- Trying \"" << ifaces[index] << "\" ----\n\n";
        if (snd_device_name_hint(-1, ifaces[index], &hints) < 0) {
            std::cerr << "hint failed on \"" << ifaces[index] << "\"\n\n";
            ++index;
            continue;
        }

        str_c = hints;
        while (*str_c) {
            name_c = snd_device_name_get_hint(*str_c, "NAME");
            desc_c = snd_device_name_get_hint(*str_c, "DESC");

            std::cout << name_c << "\n";
            start = 0;
            desc = desc_c;
            do {
                end = desc.find("\n", start);
                std::cout << "   " << desc.substr(start, end - start) << "\n";
                start = end + 1;
            } while (end != std::string::npos);
            std::cout << "\n";
            free(name_c);
            free(desc_c);
            ++str_c;
        }
        snd_device_name_free_hint(hints);
        ++index;
    }
    return 0;
}

