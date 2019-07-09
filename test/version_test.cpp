#include "../src/version.h"
#include <stdio.h>

int main(int argc, char const *argv[])
{
    const char* path = 0;
    if (argc > 1) {
        path = argv[1];
    }
    if (!path) {
        printf("path does not exist: %s\n", path ? path : "null");
        return 1;
    }

    RevisionCtx* ctx = get_revisions(path);
    if (!ctx)
        return 1;

    for (int i = 0; i < ctx->num_revisions; ++i) {
        Revision* revision = &ctx->revisions[i];
        printf("commit:\t\t%s\n", revision->commit);
        printf("date:\t\t%s\n", revision->date);
        printf("author:\t\t%s <%s>\n", revision->author, revision->email);
        printf("body:\t\t%s\n", revision->body);
        for (int c = 0; c < revision->num_chunks; ++c) {
            Chunk* chunk = &revision->chunks[c];

            printf("chunk:\t\t-%d,%d +%d,%d\n", chunk->before.start, chunk->before.end, chunk->after.start, chunk->after.end);
            printf("%s\n", chunk->body);
        }
        printf("<end>\n");
    }

    free_revisions(ctx);
    return 0;
}
