#include "agent-c.h"

extern Config config;

int search_rag_files(const char* path, const char* query, char* snippets, size_t size) {
    if (!path || !query || !snippets || size == 0) return -1;

    // Create temporary file for query
    char query_file[] = "/tmp/rag_query_XXXXXX";
    int fd = mkstemp(query_file);
    if (fd == -1) return -1;

    // Write query to temporary file
    ssize_t written = write(fd, query, strlen(query));
    close(fd);
    if (written != (ssize_t)strlen(query)) {
        unlink(query_file);
        return -1;
    }

    // Build grep command
    // Validate and sanitize path parameter to prevent command injection
    if (!path || !*path) {
        unlink(query_file);
        return -1;
    }

    // Check for path traversal attempts and other dangerous characters
    const char* dangerous_chars = ";|&`$(){}[]<>*?";
    for (const char* p = path; *p; p++) {
        if (strchr(dangerous_chars, *p)) {
            fprintf(stderr, "Error: Invalid characters in path\n");
            unlink(query_file);
            return -1;
        }
    }

    // Check for path traversal attempts
    if (strstr(path, "..") || strstr(path, "/../") || strstr(path, "/..")) {
        fprintf(stderr, "Error: Path traversal detected\n");
        unlink(query_file);
        return -1;
    }

    // Ensure path is an absolute path or relative to current directory
    if (path[0] != '/' && path[0] != '.') {
        fprintf(stderr, "Error: Path must be absolute or start with ./\n");
        unlink(query_file);
        return -1;
    }

    // Use realpath to resolve and validate the path
    char resolved_path[MAX_BUFFER];
    if (!realpath(path, resolved_path)) {
        fprintf(stderr, "Error: Invalid or inaccessible path\n");
        unlink(query_file);
        return -1;
    }

    // Verify the resolved path is within allowed directory
    char cwd[MAX_BUFFER];
    if (!getcwd(cwd, sizeof(cwd))) {
        unlink(query_file);
        return -1;
    }

    // Check if resolved path is within current working directory
    if (strncmp(resolved_path, cwd, strlen(cwd)) != 0 ||
        (resolved_path[strlen(cwd)] != '\0' && resolved_path[strlen(cwd)] != '/')) {
        fprintf(stderr, "Error: Path must be within current working directory\n");
        unlink(query_file);
        return -1;
    }

    char grep_cmd[MAX_BUFFER];
    snprintf(grep_cmd, sizeof(grep_cmd),
             "grep -r -i -A 3 -B 3 -F -f %s %s 2>/dev/null | head -n %d",
             query_file, resolved_path, config.rag_snippets * 2);

    // Execute grep
    FILE* pipe = popen(grep_cmd, "r");
    if (!pipe) {
        unlink(query_file);
        return -1;
    }

    // Read results
    size_t bytes_read = fread(snippets, 1, size - 1, pipe);
    snippets[bytes_read] = '\0';

    int result = pclose(pipe);
    unlink(query_file);
    (void)result; // Suppress unused variable warning

    // Clean up results
    char* p = snippets;
    while (*p) {
        if (*p == '\r') *p = '\n';
        p++;
    }

    return 0; // Always return success for Windows compatibility
}
