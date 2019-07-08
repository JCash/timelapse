#include "misc.h"
#include <stdio.h>
#include <stdlib.h>

void* read_file(const char* path, int* outsize) {
    FILE* fp = fopen( "test.ini", "r" );
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
