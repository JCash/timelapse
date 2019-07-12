#include "settings.h"

#include <magu/ini.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void settings_init(Settings* s) {
    s->width = 800;
    s->height = 600;
    s->fontsize = 16.0f;
    s->fontweight = 1.2f;
    s->fontpath_regular = 0;
    s->fontpath_bold = 0;
}

void settings_destroy(Settings* s) {
    free((void*)s->fontpath_regular);
    free((void*)s->fontpath_bold);
}

static void* read_file(const char* path, int* outsize) {
    FILE* fp = fopen(path, "rb");
    if (!fp)
        return 0;
    fseek( fp, 0, SEEK_END );
    int size = ftell( fp );
    fseek( fp, 0, SEEK_SET );
    char* data = (char*)malloc( size + 1 );
    fread( data, 1, size, fp );
    data[ size ] = '\0';
    fclose( fp );
    if (outsize)
        *outsize = size;
    return (void*)data;
}

static const char* settings_read_property(ini_t* ini, const char* section, const char* property) {
    int isection = ini_find_section(ini, section, strlen(section));
    int iproperty = ini_find_property(ini, isection, property, strlen(property));
    char const* value = ini_property_value(ini, isection, iproperty);
    return value ? strdup(value) : 0;
}

static float settings_read_float_property(ini_t* ini, const char* section, const char* property, float defaultvalue) {
    int isection = ini_find_section(ini, section, strlen(section));
    int iproperty = ini_find_property(ini, isection, property, strlen(property));
    char const* value = ini_property_value(ini, isection, iproperty);
    if (value) {
        float fvalue = atof(value);
        return fvalue;
    }
    return defaultvalue;
}

void settings_read(const char* path, Settings* s) {
    int size;
    const char* data = (const char*)read_file(path, &size);
    if (!data)
        return;

    const char* value = 0;

    ini_t* ini = ini_load(data, 0);
    free((void*)data);

    s->fontpath_regular = settings_read_property(ini, "fonts", "regular");
    s->fontpath_bold = settings_read_property(ini, "fonts", "bold");

    s->fontsize = settings_read_float_property(ini, "fonts", "size", s->fontsize);
    s->fontweight = settings_read_float_property(ini, "fonts", "weight", s->fontweight);

    ini_destroy(ini);
}

static int section_add(ini_t* ini, const char* section) {
    return ini_section_add(ini, section, strlen(section));
}

static void property_add(ini_t* ini, int section, const char* property, const char* value) {
    ini_property_add(ini, section, property, strlen(property), value, strlen(value));
}

static void property_add_float(ini_t* ini, int section, const char* property, float value) {
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%.2f", value);
    ini_property_add(ini, section, property, strlen(property), buffer, strlen(buffer));
}

void settings_write(const char* path, Settings* s) {
    ini_t* ini = ini_create(0);
    int fontsection = section_add(ini, "fonts");
    property_add(ini, fontsection, "regular", s->fontpath_regular ? s->fontpath_regular : "");
    property_add(ini, fontsection, "bold", s->fontpath_bold ? s->fontpath_bold : "");
    property_add_float(ini, fontsection, "size", s->fontsize);
    property_add_float(ini, fontsection, "weight", s->fontweight);

    int size = ini_save(ini, NULL, 0); // Find the size needed
    char* data = (char*) malloc(size);
    size = ini_save(ini, data, size); // Actually save the file
    ini_destroy(ini);

    FILE* fp = fopen(path, "wb");
    if (fp) {
        fwrite(data, 1, size, fp);
        fclose(fp);
    } else {
        fprintf(stderr, "Failed to write settings file: %s\n", path);
    }
    free(data);
}


const char* get_settings_path(char* buffer, int buffer_len) {
    const char* SETTINGS_FILE = ".timelapse";
    buffer[0] = 0;
#ifdef _WIN32
    //getenv("HOMEDRIVE") and getenv("HOMEPATH")
#else
    const char* home = getenv("HOME");
    snprintf(buffer, buffer_len, "%s/%s", home, SETTINGS_FILE);
#endif
    return buffer;
}
