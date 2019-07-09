#include "../src/version.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char const *argv[])
{
    const char* path = 0;
    int revision_number = 0;//-1; // last version
    if (argc > 1) {
        path = argv[1];
    }
    if (argc > 2) {
        revision_number = atoi(argv[2]);
    }
    if (!path) {
        fprintf(stderr, "path does not exist: %s\n", path ? path : "null");
        return 1;
    }

    RevisionCtx* ctx = get_revisions(path);
    if (!ctx)
        return 1;

    for (int i = 0; i < ctx->num_revisions; ++i) {
        Revision* revision = &ctx->revisions[i];
        fprintf(stderr, "commit:\t\t%s\n", revision->commit);
        fprintf(stderr, "date:\t\t%s\n", revision->date);
        fprintf(stderr, "author:\t\t%s <%s>\n", revision->author, revision->email);
        fprintf(stderr, "body:\t\t%s\n", revision->body);
        for (int c = 0; c < revision->num_chunks; ++c) {
            Chunk* chunk = &revision->chunks[c];

            fprintf(stderr, "chunk:\t\t-%d,%d +%d,%d\n", chunk->before.start, chunk->before.length, chunk->after.start, chunk->after.length);
            fprintf(stderr, "%s\n", chunk->body);
        }
        fprintf(stderr, "<end>\n");
    }

    fprintf(stderr, "\n\n\n");

    File* file = get_file_from_revision(ctx, revision_number);
    if (!file) {
        fprintf(stderr, "Failed to generate file from revision %d\n", revision_number);
        return 1;
    }

    printf("FILE:\n");
    for (int i = 0; i < file->num_lines; ++i) {
        const char* revision = file->line_revisions[i];
        const char* line = file->lines[i];
        const char* lineend = strchr(line, '\n');
        if (lineend) {
            int size = lineend-line;
            char* buffer = (char*)alloca(size+1);
            memcpy(buffer, line, size);
            buffer[size] = 0;
            printf("%s - %s\n", revision, buffer);
        } else {
            printf("%s - %s\n", revision, line);
        }
    }

    free_file(file);
    free_revisions(ctx);
    return 0;
}
