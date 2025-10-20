#include "agent-c.h"
#include <string.h>

extern Config config;

int parse_args(int argc, char* argv[]) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--rag") == 0) {
            if (i + 1 < argc) {
                strncpy(config.rag_path, argv[i + 1], sizeof(config.rag_path) - 1);
                config.rag_path[sizeof(config.rag_path) - 1] = '\0';
                config.rag_enabled = 1;
                i++;
            } else {
                fprintf(stderr, "Error: --rag requires a path argument\n");
                return -1;
            }
        } else if (strcmp(argv[i], "--rag-snippets") == 0) {
            if (i + 1 < argc) {
                int snippets = atoi(argv[i + 1]);
                if (snippets > 0 && snippets <= 20) {
                    config.rag_snippets = snippets;
                    i++;
                } else {
                    fprintf(stderr, "Error: --rag-snippets must be between 1 and 20\n");
                    return -1;
                }
            } else {
                fprintf(stderr, "Error: --rag-snippets requires a number argument\n");
                return -1;
            }
        } else {
            fprintf(stderr, "Unknown argument: %s\n", argv[i]);
            return -1;
        }
    }
    return 0;
}