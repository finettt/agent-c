#include "agent-c.h"

extern Agent agent;
extern Config config;

void init_agent(void) {
    strcpy(agent.messages[0].role, "system");
    strcpy(agent.messages[0].content, "You are an AI assistant. For multi-step tasks, chain commands with && (e.g., 'echo content > file.py && python3 file.py'). Use execute_command for shell tasks.");
    agent.msg_count = 1;
}

int execute_command(const char* response) {
    if (!response) return 0;

    char cmd[MAX_CONTENT] = {0};
    if (!extract_command(response, cmd, sizeof(cmd)) || !*cmd) return 0;

    printf("\033[31m$ %s\033[0m\n", cmd);
    char safe_cmd[MAX_CONTENT] = {0};
    const char* src = cmd;
    char* dst = safe_cmd;
    size_t safe_len = 0;

    while (*src && safe_len < sizeof(safe_cmd) - 1) {
        // Allow explicit safe multi-step and redirection operators (&&, >, >>)
        if (*src == '&') {
            if (*(src + 1) == '&') {
                if (safe_len + 2 < sizeof(safe_cmd) - 1) {
                    *dst++ = '&';
                    *dst++ = '&';
                    safe_len += 2;
                }
                src += 2;
                continue;
            }
            src++;
            continue;
        }
        if (*src == '>') {
            *dst++ = '>';
            safe_len++;
            if (*(src + 1) == '>' && safe_len < sizeof(safe_cmd) - 1) {
                *dst++ = '>';
                safe_len++;
                src += 2;
            } else {
                src++;
            }
            continue;
        }

        // Strict whitelist: alnum, space, and selected punctuation
        unsigned char ch = (unsigned char)*src;
        int allowed = 0;
        if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == ' ') {
            allowed = 1;
        } else {
            switch (ch) {
                case '.': case '/': case '-': case '_': case '+': case '=':
                case ',': case ':': case '@': case '%':
                    allowed = 1; break;
#ifdef _WIN32
                case '\\': // Allow backslash for Windows paths
                    allowed = 1; break;
#endif
                default: allowed = 0; break;
            }
        }

        // Explicitly disallow any form of shell expansion or quoting
        if (ch == '$' || ch == '"' || ch == '\'' || ch == '`' || ch == '|' || ch == ';' ||
            ch == '(' || ch == ')' || ch == '<' || ch == '{' || ch == '}' ||
            ch == '[' || ch == ']' || ch == '*' || ch == '?' || ch == '#') {
            allowed = 0;
        }

        if (allowed) {
            *dst++ = *src++;
            safe_len++;
        } else {
            src++;
        }
    }
    *dst = '\0';
    if (!*safe_cmd) return 0;
    FILE* pipe = popen(safe_cmd, "r");
    if (!pipe) {
        fprintf(stderr, "Failed to execute command: %s\n", safe_cmd);
        return 0;
    }

    char output[MAX_BUFFER] = {0};
    size_t bytes = fread(output, 1, sizeof(output) - 1, pipe);
    output[bytes] = '\0';

    int result = pclose(pipe);
    if (result == -1) {
        fprintf(stderr, "Failed to close command pipe\n");
    }
    if (agent.msg_count < MAX_MESSAGES) {
        Message* msg = &agent.messages[agent.msg_count++];
        strcpy(msg->role, "tool");
        snprintf(msg->content, MAX_CONTENT, "Command output:\n%s", output);
    }

    return WIFEXITED(result) ? WEXITSTATUS(result) : 0;
}

int process_agent(const char* task) {
    if (!task || !*task) return -1;
    if (strlen(task) >= MAX_CONTENT) {
        fprintf(stderr, "Task too long\n");
        return -1;
    }

    if (agent.msg_count >= MAX_MESSAGES - 1) {
        int remove_count = (agent.msg_count - 5) > 0 ? (agent.msg_count - 5) : 0;
        for (int i = 1; i < agent.msg_count - remove_count; i++) {
            agent.messages[i] = agent.messages[i + remove_count];
        }
        agent.msg_count -= remove_count;
    }

    char rag_snippets[MAX_BUFFER] = {0};
    if (config.rag_enabled && config.rag_path[0]) {
        if (search_rag_files(config.rag_path, task, rag_snippets, sizeof(rag_snippets)) == 0 && rag_snippets[0]) {
            char enhanced_task[MAX_CONTENT * 2];
            snprintf(enhanced_task, sizeof(enhanced_task),
                    "User asked: %s\n\nRelevant snippets from local documents:\n%s\n\nPlease answer based on the user's request and the provided context.",
                    task, rag_snippets);

            strcpy(agent.messages[agent.msg_count].role, "user");
            strncpy(agent.messages[agent.msg_count].content, enhanced_task, MAX_CONTENT - 1);
            agent.messages[agent.msg_count].content[MAX_CONTENT - 1] = '\0';
            agent.msg_count++;
        } else {
            strcpy(agent.messages[agent.msg_count].role, "user");
            strncpy(agent.messages[agent.msg_count].content, task, MAX_CONTENT - 1);
            agent.messages[agent.msg_count].content[MAX_CONTENT - 1] = '\0';
            agent.msg_count++;
        }
    } else {
        strcpy(agent.messages[agent.msg_count].role, "user");
        strncpy(agent.messages[agent.msg_count].content, task, MAX_CONTENT - 1);
        agent.messages[agent.msg_count].content[MAX_CONTENT - 1] = '\0';
        agent.msg_count++;
    }

    char req[MAX_BUFFER], resp[MAX_BUFFER];
    if (!json_request(&agent, &config, req, sizeof(req))) {
        return -1;
    }

    if (http_request(req, resp, sizeof(resp))) return -1;

    if (has_tool_call(resp)) {
        if (!execute_command(resp)) {
            fprintf(stderr, "Command execution failed, retrying...\n");
            if (!json_request(&agent, &config, req, sizeof(req))) {
                fprintf(stderr, "Failed to create retry request\n");
                return -1;
            }
            if (http_request(req, resp, sizeof(resp))) {
                fprintf(stderr, "HTTP request failed during retry\n");
                return -1;
            }
        }
    }

    char content[MAX_CONTENT];
    if (json_content(resp, content, sizeof(content))) {
        printf("\033[34m%s\033[0m\n", content);

        if (agent.msg_count < MAX_MESSAGES - 1) {
            strcpy(agent.messages[agent.msg_count].role, "assistant");
            strncpy(agent.messages[agent.msg_count].content, content, MAX_CONTENT - 1);
            agent.messages[agent.msg_count].content[MAX_CONTENT - 1] = '\0';
            agent.msg_count++;
        }
    } else {
        fprintf(stderr, "Failed to extract content from response\n");
        return -1;
    }

    return 0;
}
