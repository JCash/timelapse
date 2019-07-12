#pragma once

struct Settings {
    // window
    int width;
    int height;
    // fonts
    float fontsize;
    float fontweight;
    const char* fontpath_regular;
    const char* fontpath_bold;
};

void settings_init(Settings* s);
void settings_destroy(Settings* s);
void settings_read(const char* path, Settings* s);
void settings_write(const char* path, Settings* s);

const char* get_settings_path(char* buffer, int buffer_len);
