#include "agent-c.h"

extern Config config;

// Cross-platform temporary file creation that returns a named file path
// The created file will be opened exclusively and closed by the caller
// Returns 0 on success, -1 on failure
static int create_temp_file(char* out_path, size_t out_size, const char* prefix) {
    if (!out_path || out_size == 0) return -1;
    out_path[0] = '\0';

#ifdef _WIN32
    // Seed the random number generator to ensure unpredictable temp file names
    srand((unsigned int)time(NULL));

    const char* temp_dir = getenv("TEMP");
    if (!temp_dir || !*temp_dir) temp_dir = getenv("TMP");
    if (!temp_dir || !*temp_dir) temp_dir = "C:\\Windows\\Temp";

    // Try a few times to avoid collisions
    for (int attempt = 0; attempt < 10; attempt++) {
        unsigned int r = (unsigned int)rand();
        if (snprintf(out_path, out_size, "%s\\%s_%08x.json", temp_dir, prefix, r) >= (int)out_size) {
            return -1;
        }

        FILE* f = fopen(out_path, "wbx");
        if (f) {
            fclose(f);
            return 0;
        }
    }
    return -1;
#else
    const char* base = getenv("TMPDIR");
    if (!base || !*base) base = "/tmp";
    char pattern[256];
    if (snprintf(pattern, sizeof(pattern), "%s/%s_XXXXXX.json", base, prefix) >= (int)sizeof(pattern)) {
        return -1;
    }
    // mkstemp modifies the template in place
    char tmp[256];
    strncpy(tmp, pattern, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';
    int fd = mkstemp(tmp);
    if (fd == -1) return -1;
    close(fd);
    // Secure permissions (owner rw)
    chmod(tmp, 0600);
    if (snprintf(out_path, out_size, "%s", tmp) >= (int)out_size) {
        // Best effort cleanup
        remove(tmp);
        return -1;
    }
    return 0;
#endif
}

int http_request(const char* req, char* resp, size_t resp_size) {
    if (!req || !resp || resp_size == 0) return -1;

    char temp_path[256];
    if (create_temp_file(temp_path, sizeof(temp_path), "ai_req") != 0) {
        return -1;
    }

    FILE* tf = fopen(temp_path, "wb");
    if (!tf) {
        remove(temp_path);
        return -1;
    }
    size_t to_write = strlen(req);
    if (fwrite(req, 1, to_write, tf) != to_write) {
        fclose(tf);
        remove(temp_path);
        return -1;
    }
    fclose(tf);

    static const char* curl_template = "curl -s -X POST '%s/v1/chat/completions' "
        "-H 'Content-Type: application/json' -H 'Authorization: Bearer %s' -d @'%s' --max-time 60";
    // Normalize API base to avoid duplicate /v1
    char api_base[128];
    strncpy(api_base, config.api_url, sizeof(api_base) - 1);
    api_base[sizeof(api_base) - 1] = '\0';
    size_t base_len = strlen(api_base);
    if (base_len >= 3) {
        // strip trailing slashes
        while (base_len > 0 && api_base[base_len - 1] == '/') {
            api_base[--base_len] = '\0';
        }
        if (base_len >= 3 && (strcmp(api_base + base_len - 3, "/v1") == 0)) {
            api_base[base_len - 3] = '\0';
        }
    }

    char curl[MAX_BUFFER];
    if (snprintf(curl, sizeof(curl), curl_template, api_base, config.api_key, temp_path) >= (int)sizeof(curl)) {
        remove(temp_path);
        return -1;
    }

    FILE* pipe = popen(curl, "r");
    if (!pipe) {
        remove(temp_path);
        return -1;
    }

    size_t bytes = fread(resp, 1, resp_size - 1, pipe);
    resp[bytes] = '\0';

    int pclose_result = pclose(pipe);
    remove(temp_path);
    if (WIFEXITED(pclose_result)) {
        int exit_status = WEXITSTATUS(pclose_result);
        if (exit_status != 0) {
            return -1;
        }
    } else {
        return -1;
    }

    return 0;
}

void load_config(void) {
    config.temp = 0.1;
    config.max_tokens = 1000;
    config.api_key[0] = '\0';
    config.api_url[0] = '\0';
    config.model[0] = '\0';
    config.rag_path[0] = '\0';
    config.rag_enabled = 0;
    config.rag_snippets = 5;

    // Set default API base URL to OpenAI (without trailing /v1)
    strncpy(config.api_url, "https://api.openai.com", 127);
    config.api_url[127] = '\0';

    char* key = getenv("OPENAI_KEY");
    if (key && strlen(key) > 0 && strlen(key) < 127) {
        strncpy(config.api_key, key, 127);
        config.api_key[127] = '\0';
    }
    char* url = getenv("OPENAI_BASE");
    if (url && strlen(url) > 0 && strlen(url) < 127) {
        if (strstr(url, "http://") || strstr(url, "https://")) {
            strncpy(config.api_url, url, 127);
            config.api_url[127] = '\0';
        }
    }
    char* model = getenv("OPENAI_MODEL");
    if (model && strlen(model) > 0 && strlen(model) < 63) {
        strncpy(config.model, model, 63);
        config.model[63] = '\0';
    } else {
        // Provide a sensible default model if not specified via env
        strncpy(config.model, "gpt-3.5-turbo", 63);
        config.model[63] = '\0';
    }
    char* rag_path = getenv("RAG_PATH");
    if (rag_path && strlen(rag_path) > 0 && strlen(rag_path) < 255) {
        strncpy(config.rag_path, rag_path, 255);
        config.rag_path[255] = '\0';
    }
    char* rag_enabled = getenv("RAG_ENABLED");
    if (rag_enabled && strcmp(rag_enabled, "1") == 0) {
        config.rag_enabled = 1;
    }
    char* rag_snippets = getenv("RAG_SNIPPETS");
    if (rag_snippets && atoi(rag_snippets) > 0) {
        config.rag_snippets = atoi(rag_snippets);
    }
}
