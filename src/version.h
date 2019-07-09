#pragma once

struct Range {
    int start, length;
};

struct Chunk {
    Range before;
    Range after;
    const char* body;
};

struct Revision {
    const char* commit;
    const char* date;
    const char* author;
    const char* email;
    const char* body;
    Chunk* chunks;
    int num_chunks;
};

struct File {
    const char** line_revisions;
    const char** lines;
    int num_lines;
    int max_lines;
};

struct RevisionCtx {
    void* _internal;
    Revision* revisions;
    int num_revisions;
};

// Get all revisions for a file (earliest to latest)
RevisionCtx* get_revisions(const char* path);

// Releases the internal memory
void free_revisions(RevisionCtx* ctx);

// Finds the latest revision that a line number corresponds to
Revision* get_revision_from_line(RevisionCtx* ctx, int line);


File* get_file_from_revision(RevisionCtx* ctx, int revision);
void free_file(File* file);
