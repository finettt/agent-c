#include "agent-c.h"

extern Config config;

int search_rag_files(const char* path, const char* query, char* snippets, size_t size) {
    if (!path || !query || !snippets || size == 0) return -1;
    
    // Create temporary file for query
    char query_file[] = "/tmp/rag_query_XXXXXX";
    int fd = mkstemp(query_file);
    if (fd == -1) return -1;
    
    // Set secure permissions for temporary file (read/write only for owner)
    chmod(query_file, 0600);
    
    // Write query to temporary file
    ssize_t written = write(fd, query, strlen(query));
    close(fd);
    if (written != (ssize_t)strlen(query)) {
        unlink(query_file);
        return -1;
    }
    
    // Build grep command
    char grep_cmd[MAX_BUFFER];
    snprintf(grep_cmd, sizeof(grep_cmd), 
             "grep -r -i -A 3 -B 3 -F -f %s %s 2>/dev/null | head -n %d", 
             query_file, path, config.rag_snippets * 2);
    
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
    (void)result; // Suppress unused variable warning
    unlink(query_file);
    
    // Clean up results
    char* p = snippets;
    while (*p) {
        if (*p == '\r') *p = '\n';
        p++;
    }
    
    return 0; // Always return success for Windows compatibility
}