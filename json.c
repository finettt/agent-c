#include "agent-c.h"

static char* json_find(const char* json, const char* key, char* out, size_t size) {
    if (!json || !key || !out) return NULL;
    char pattern[64];
    snprintf(pattern, 64, "\"%s\":", key);
    const char* start = strstr(json, pattern);
    if (!start) return NULL;
    start += strlen(pattern);
    while (*start == ' ' || *start == '\t') start++;
    
    if (*start == '"') {
        start++;
        const char* end = start;
        while (*end && *end != '"') {
            if (*end == '\\' && end[1]) end += 2;
            else end++;
        }
        size_t len = end - start;
        if (len >= size) len = size - 1;
        strncpy(out, start, len);
        out[len] = '\0';
        
        for (char* p = out; *p && p < out + size - 1; p++) {
            if (*p == '\\' && p[1] && p < out + size - 2) {
                switch (p[1]) {
                    case 'n':
                        *p = '\n';
                        memmove(p+1, p+2, strlen(p+2) + 1);
                        break;
                    case 't':
                        *p = '\t';
                        memmove(p+1, p+2, strlen(p+2) + 1);
                        break;
                    case 'r':
                        *p = '\r';
                        memmove(p+1, p+2, strlen(p+2) + 1);
                        break;
                    case '\\':
                    case '"':
                        memmove(p, p+1, strlen(p+1) + 1);
                        p--;
                        break;
                }
            }
        }
    } else {
        const char* end = start;
        while (*end && *end != ',' && *end != '}' && *end != ' ' && *end != '\n') end++;
        size_t len = end - start;
        if (len >= size) len = size - 1;
        strncpy(out, start, len);
        out[len] = '\0';
    }
    return out;
}

char* json_request(const Agent* agent, const Config* config, char* out, size_t size) {
    if (!agent || !config || !out || size == 0) return NULL;
    if (!config->model || !*config->model) {
        fprintf(stderr, "Error: model not configured\n");
        return NULL;
    }
    
    char messages[MAX_BUFFER] = "[";
    for (int i = 0; i < agent->msg_count; i++) {
        if (i > 0) {
            if (strlen(messages) + 1 >= sizeof(messages)) break;
            strcat(messages, ",");
        }
        const Message* msg = &agent->messages[i];
        
        char escaped_content[MAX_CONTENT * 2] = {0};
        const char* src = msg->content;
        char* dst = escaped_content;
        size_t escaped_len = 0;
        
        while (*src && escaped_len < sizeof(escaped_content) - 1) {
            switch (*src) {
                case '"':  *dst++ = '\\'; *dst++ = '"'; escaped_len += 2; break;
                case '\\': *dst++ = '\\'; *dst++ = '\\'; escaped_len += 2; break;
                case '\b': *dst++ = '\\'; *dst++ = 'b'; escaped_len += 2; break;
                case '\f': *dst++ = '\\'; *dst++ = 'f'; escaped_len += 2; break;
                case '\n': *dst++ = '\\'; *dst++ = 'n'; escaped_len += 2; break;
                case '\r': *dst++ = '\\'; *dst++ = 'r'; escaped_len += 2; break;
                case '\t': *dst++ = '\\'; *dst++ = 't'; escaped_len += 2; break;
                default:
                    if (*src >= 0 && *src < 32) {
                    } else {
                        *dst++ = *src;
                        escaped_len++;
                    }
                    break;
            }
            src++;
        }
        *dst = '\0';
        
        char temp[MAX_CONTENT + 100];
        if (!strcmp(msg->role, "tool")) {
            snprintf(temp, sizeof(temp), "{\"role\":\"tool\",\"content\":\"%s\"}", escaped_content);
        } else {
            snprintf(temp, sizeof(temp), "{\"role\":\"%s\",\"content\":\"%s\"}",
                    msg->role, escaped_content);
        }
        
        if (strlen(messages) + strlen(temp) + 1 < sizeof(messages)) {
            strcat(messages, temp);
        } else {
            fprintf(stderr, "Warning: message truncated due to size limit\n");
            break;
        }
    }
    
    if (strlen(messages) + 1 >= sizeof(messages)) {
        messages[sizeof(messages) - 1] = '\0';
    } else {
        strcat(messages, "]");
    }
    
    const char* template = "{\"model\":\"%s\",\"messages\":%s,\"temperature\":%.1f,\"max_tokens\":%d,\"stream\":false,"
        "\"tool_choice\":\"auto\","
        "\"tools\":[{\"type\":\"function\",\"function\":{\"name\":\"execute_command\","
        "\"description\":\"Execute shell command\",\"parameters\":{\"type\":\"object\",\"properties\":{\"command\":{\"type\":\"string\"}},"
        "\"required\":[\"command\"]}}}]}";
    
    int result = snprintf(out, size, template, config->model, messages, config->temp, config->max_tokens);
    if (result < 0 || (size_t)result >= size) {
        fprintf(stderr, "Error: JSON request buffer too small\n");
        return NULL;
    }
    
    return out;
}

char* json_content(const char* response, char* out, size_t size) {
    if (!response || !out) return NULL;
    const char* choices = strstr(response, "\"choices\":");
    if (!choices) return NULL;
    const char* message = strstr(choices, "\"message\":");
    if (!message) return NULL;
    return json_find(message, "content", out, size);
}

int extract_command(const char* response, char* cmd, size_t cmd_size) {
    if (!response || !cmd) return 0;
    
    const char* start = strstr(response, "\"tool_calls\":");
    if (!start) return 0;
    
    start = strstr(start, "\"arguments\":");
    if (!start) return 0;
    
    char args[1024];
    if (!json_find(start, "arguments", args, sizeof(args))) return 0;
    
    return json_find(args, "command", cmd, cmd_size) != NULL;
}

