#include "agent-c.h"

extern Config config;

int http_request(const char* req, char* resp, size_t resp_size) {
    if (!req || !resp || resp_size == 0) return -1;
    
    char temp[] = "/tmp/ai_req_XXXXXX";
    int fd = mkstemp(temp);
    if (fd == -1) return -1;
    
    ssize_t written = write(fd, req, strlen(req));
    close(fd);
    if (written != (ssize_t)strlen(req)) {
        unlink(temp);
        return -1;
    }
    
    static const char* curl_template = "curl -s -X POST '%s/v1/chat/completions' "
        "-H 'Content-Type: application/json' -H 'Authorization: Bearer %s' -d @'%s' --max-time 60";
    char curl[MAX_BUFFER];
    if (snprintf(curl, sizeof(curl), curl_template, config.api_url, config.api_key, temp) >= sizeof(curl)) {
        unlink(temp);
        return -1;
    }
    
    FILE* pipe = popen(curl, "r");
    if (!pipe) {
        unlink(temp);
        return -1;
    }
    
    size_t bytes = fread(resp, 1, resp_size - 1, pipe);
    resp[bytes] = '\0';
    
    int pclose_result = pclose(pipe);
    unlink(temp);
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
    }
}
