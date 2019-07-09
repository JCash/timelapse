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

static char* find_line_start(char* s, const char* pattern) {
    char* result = strstr(s, pattern);
    while (result) {
        if (*(result-1) == '\n') {
            break; // It has to be at the start of a line!
        }
        result = strstr(result+1, pattern);
    }
    return result;
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
        char* chunkend = find_line_start(chunkstart, "@@");
        if (!chunkend) {
            break;
        }
        chunkstart = chunkend;
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
const char CMD_GET_REVISIONS_FMT[] = "git log -p --format=format:\"timelapse_header:%%ncommit:%%h%%nauthor:%%an%%ndate:%%ad%%nemail:%%ae%%nbody:%%B%%nchunks:\" %s";

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

    //printf("RESULT:\n%s", result);

    RevisionCtx* ctx = (RevisionCtx*)malloc(sizeof(RevisionCtx));
    ctx->_internal = result;
    ctx->revisions = 0;
    ctx->num_revisions = 0;

#define CONSUME_LINE(P)                                 \
    {                                                   \
        char* lineend = strchr(P, '\n');                \
        if (lineend == 0) lineend = result+result_len;  \
        *lineend = 0;                                   \
        current = (lineend+1) - result;                 \
    }

#define CONSUME_SEGMENT(P, NEXTTAG)                     \
    {                                                   \
        char* segment_end = find_line_start(P, NEXTTAG);\
        if (!segment_end)                               \
            segment_end = result + result_len;          \
        current = segment_end - result;                 \
        *(segment_end-1) = 0;                           \
    }

    Revision* revision = 0;
    int current = 0;
    while (current < result_len) {
        char* line = &result[current];

        // char buffer[20];
        // memcpy(buffer, line, sizeof(buffer)-1);
        // buffer[sizeof(buffer)-1] = 0;
        // printf("LINE: %s\n", buffer);

        char* match = 0;

        if ((match = match_string("timelapse_header:", line))) {
            CONSUME_LINE(match);

            ctx->num_revisions++;
            ctx->revisions = (Revision*)realloc(ctx->revisions, ctx->num_revisions * sizeof(Revision));
            // Since the command gives them in reverse order, and we want them chronologically
            memmove(ctx->revisions+1, ctx->revisions, (ctx->num_revisions-1) * sizeof(Revision));
            revision = &ctx->revisions[0];

            memset(revision, 0, sizeof(Revision));
        }
        else if ((match = match_string("commit:", line))) {
            CONSUME_LINE(match);
            //printf("MATCH: commit: %s\n", match);
            revision->commit = match;
        }
        else if ((match = match_string("author:", line))) {
            CONSUME_LINE(match);
            //printf("MATCH: author: %s\n", match);
            revision->author = match;
        }
        else if ((match = match_string("email:", line))) {
            CONSUME_LINE(match);
            //printf("MATCH: email: %s\n", match);
            revision->email = match;
        }
        else if ((match = match_string("date:", line))) {
            CONSUME_LINE(match);
            //printf("MATCH: date: %s\n", match);
            revision->date = match;
        }
        else if ((match = match_string("body:", line))) {
            //printf("MATCH: body: %s\n", match);
            revision->body = match;
            CONSUME_SEGMENT(match, "chunks:");
        }
        else if ((match = match_string("chunks:", line))) {
            //printf("MATCH: chunks: %s\n", match);
            CONSUME_SEGMENT(match, "timelapse_header:");
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

