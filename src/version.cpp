
// const char* RE_COMMIT = "^commit\\s*([a-zA-Z0-9]*)$";
// const char* RE_AUTHOR = "^Author:\\s*(.*)\\s<(.*)>$";
// const char* RE_DATE = "^Date:\\s*(.*)$";


// "commit\s*([a-zA-Z0-9]*)\nAuthor:\s(.*)\s<(.*)>";

// "commit\s*([a-zA-Z0-9]*)\nAuthor:\s(.*)\s<(.*)>\nDate:\s*(.*)\n(.*)"

// static uint8_t* get_file()


#include <iostream>
#include <stdexcept>
#include <stdio.h>
#include <string>
#include "version.h"

static char* run_cmd(const char* cmd, int* outlen) {
    char buffer[256];
    int maxsize = 2*1024;
    char* output = (char*)malloc(maxsize);
    int len = 0;
    FILE* pipe = popen(cmd, "r");
    if (!pipe) {
        printf("Failed to run command '%s'", cmd);
        return 0;
    }
    while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        int readlen = strlen(buffer);
        if (readlen + len >= maxsize) {
            maxsize += 2 * 1024;
            output = (char*)realloc((void*)output, maxsize);
        }
        strcpy(&output[len], buffer);
        len += readlen;
    }
    if (len+1 >= maxsize) {
        maxsize += 1;
        output = (char*)realloc((void*)output, maxsize);
    }
    output[len] = 0;
    *outlen = len;

    pclose(pipe);
    return output;
}

// E.g.: "@@ -26,7 +26,12 @@ void function_name() {"
static char* parse_chunk_header(Chunk* chunk, const char* header) {
    header += 3;
    char* headerend = (char*)strchr(header, '@') - 1;
    *headerend = 0;
    int num = sscanf(header, "-%d,%d +%d,%d", &chunk->before.start, &chunk->before.end, &chunk->after.start, &chunk->after.end);
    assert(num == 4);
    return strchr(headerend+1, '\n') + 1;
}

static void parse_chunks(Revision* revision, const char* sectionstart, const char* sectionend) {
    revision->num_chunks = 0;
    revision->chunks = 0;

    char* chunkstart = (char*)strstr(sectionstart, "@@"); // @@ -26,7 +26,12 @@ void function_name() {
    assert(chunkstart);
    while (chunkstart < sectionend) {

        // allocate chunk
        revision->num_chunks++;
        revision->chunks = (Chunk*)realloc(revision->chunks, revision->num_chunks * sizeof(Chunk));
        Chunk* chunk = &revision->chunks[revision->num_chunks-1];

        // Parse the chunk header
        chunkstart = parse_chunk_header(chunk, chunkstart);
        *(chunkstart-1) = 0; // terminate this chunk

        //printf("MATCH: CHUNK:  -%d,%d +%d,%d\n", chunk->before.start, chunk->before.end, chunk->after.start, chunk->after.end);

        // Parse the chunk body
        chunk->body = chunkstart;

        // Find the next chunk from our command (or 0 if it was the last chunk)
        char* chunkend = strstr(chunkstart, "@@");
        if (!chunkend) {
            break;
        }
    }
}

static char* match_string(const char* pattern, char* s) {
    int len = strlen(pattern);
    if (strncmp(pattern, s, len) == 0)
        return s+len;
    return 0;
}

// git log --pretty=format:"commit:%h%nauthor:%an%ndate:%ad%nemail:%ae%nbody:%B%n"
// git log -p --format=format:"commit:%h%nauthor:%an%ndate:%ad%nemail:%ae%nbody:%B%nchunks:"
const char CMD_GET_REVISIONS_FMT[] = "git log -p --format=format:\"commit:%%h%%nauthor:%%an%%ndate:%%ad%%nemail:%%ae%%nbody:%%B%%nchunks:\" %s";

RevisionCtx* get_revisions(const char* path) {

    // TODO: Check that file exists

    char cmd[4096];
    snprintf(cmd, sizeof(cmd), CMD_GET_REVISIONS_FMT, path);

    int result_len = 0;
    char* result = run_cmd(cmd, &result_len);
    if (!result) {
        return 0;
    }

    if (result_len == 0) {
        printf("No revisions found for file: %s\n", path);
        return 0;
    }

    RevisionCtx* ctx = (RevisionCtx*)malloc(sizeof(RevisionCtx));
    ctx->_internal = result;
    ctx->revisions = 0;
    ctx->num_revisions = 0;

#define COMSUME_LINE(P)                                 \
    {                                                   \
        char* lineend = strchr(P, '\n');                \
        if (lineend == 0) lineend = result+result_len;  \
        *lineend = 0;                                   \
        current = (lineend+1) - result;                 \
    }

#define COMSUME_SEGMENT(P, NEXTTAG)                     \
    {                                                   \
        char* segment_end = strstr(P, NEXTTAG);         \
        if (!segment_end)                               \
            segment_end = result + result_len;          \
        current = segment_end - result;                 \
        *(segment_end-1) = 0;                           \
    }

    Revision* revision = 0;
    int current = 0;
    while (current < result_len) {
        char* line = &result[current];

        char* match = 0;
        if ((match = match_string("commit:", line))) {
            COMSUME_LINE(match);

            ctx->num_revisions++;
            ctx->revisions = (Revision*)realloc(ctx->revisions, ctx->num_revisions * sizeof(Revision));
            revision = &ctx->revisions[ctx->num_revisions-1];
            memset(revision, 0, sizeof(Revision));

            *line = 0; // terminate the previous "chunks" section
            //printf("MATCH: commit: %s\n", match);
            revision->commit = match;
        }
        else if ((match = match_string("author:", line))) {
            COMSUME_LINE(match);
            //printf("MATCH: author: %s\n", match);
            revision->author = match;
        }
        else if ((match = match_string("email:", line))) {
            COMSUME_LINE(match);
            //printf("MATCH: email: %s\n", match);
            revision->email = match;
        }
        else if ((match = match_string("date:", line))) {
            COMSUME_LINE(match);
            //printf("MATCH: date: %s\n", match);
            revision->date = match;
        }
        else if ((match = match_string("body:", line))) {
            //printf("MATCH: body: %s\n", match);
            revision->body = match;
            COMSUME_SEGMENT(match, "chunks:");
        }
        else if ((match = match_string("chunks:", line))) {
            //printf("MATCH: chunks: %s\n", match);
            COMSUME_SEGMENT(match, "commit:");
            char* chunks_end = result+current;
            parse_chunks(revision, match, chunks_end);
        }
    }

    return ctx;
}

void free_revisions(RevisionCtx* ctx) {
    for (int i = 0; i < ctx->num_revisions; ++i) {
        free((void*)ctx->revisions[i].chunks);
    }
    free((void*)ctx->revisions);
    free((void*)ctx->_internal);
    free((void*)ctx);
}

