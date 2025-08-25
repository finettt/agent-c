#include "agent-c.h"

extern Config config;

int http_request(const char* req, char* resp, size_t resp_size) {
    char temp[] = "/tmp/ai_req_XXXXXX";
    int fd = mkstemp(temp);
    if (fd == -1) return -1;
    write(fd, req, strlen(req));
    close(fd);
    
    static const char* curl_template = "curl -s -X POST 'https://openrouter.ai/api/v1/chat/completions' "
        "-H 'Content-Type: application/json' -H 'Authorization: Bearer %s' -d @'%s' --max-time 60";
    char curl[MAX_BUFFER];
    snprintf(curl, sizeof(curl), curl_template, config.api_key, temp);
    
    FILE* pipe = popen(curl, "r");
    if (!pipe) { unlink(temp); return -1; }
    
    size_t bytes = fread(resp, 1, resp_size - 1, pipe);
    resp[bytes] = '\0';
    pclose(pipe);
    unlink(temp);
    return 0;
}

void load_config(void) {
    strcpy(config.model, "qwen/qwen3-coder");
    config.temp = 0.1;
    config.max_tokens = 1000;
    config.api_key[0] = '\0';
    
    char* key = getenv("OR_KEY");
    if (key) strncpy(config.api_key, key, 127);
}
