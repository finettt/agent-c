#include "agent-c.h"

extern Agent agent;
extern Config config;

void init_agent(void) {
    strcpy(agent.messages[0].role, "system");
    strcpy(agent.messages[0].content, "You are an AI assistant with Napoleon Dynamite's personality. Say things like 'Gosh!', 'Sweet!', 'Idiot!', and be awkwardly enthusiastic. For multi-step tasks, chain commands with && (e.g., 'echo content > file.py && python3 file.py'). Use execute_command for shell tasks. Answer questions in Napoleon's quirky style.");
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
        if (*src == '`' || *src == '$' || *src == '|' || *src == '&' ||
            *src == ';' || *src == '(' || *src == ')' || *src == '<' ||
            *src == '>' || *src == '{' || *src == '}' || *src == '[' ||
            *src == ']' || *src == '*' || *src == '?' || *src == '#' ||
            *src == '\\' || *src == '\n' || *src == '\r') {
            src++;
            continue;
        }
        if (*src == '$' && *(src + 1) == '(') {
            src += 2;
            int paren_count = 1;
            while (*src && paren_count > 0) {
                if (*src == '(') paren_count++;
                else if (*src == ')') paren_count--;
                src++;
            }
            continue;
        }
        if (*src == '`') {
            src++;
            while (*src && *src != '`') src++;
            if (*src == '`') src++;
            continue;
        }
        
        *dst++ = *src++;
        safe_len++;
    }
    *dst = '\0';
    if (!*safe_cmd) return 0;
    FILE* pipe = popen(safe_cmd, "r");
    if (!pipe) return 0;
    
    char output[MAX_BUFFER] = {0};
    size_t bytes = fread(output, 1, sizeof(output) - 1, pipe);
    output[bytes] = '\0';
    
    int result = pclose(pipe);
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
        for (int i = 1; i < agent.msg_count - 5; i++) agent.messages[i] = agent.messages[i + 5];
        agent.msg_count -= 5;
    }
    
    strcpy(agent.messages[agent.msg_count].role, "user");
    strncpy(agent.messages[agent.msg_count].content, task, MAX_CONTENT - 1);
    agent.messages[agent.msg_count].content[MAX_CONTENT - 1] = '\0';
    agent.msg_count++;
    
    char req[MAX_BUFFER], resp[MAX_BUFFER];
    if (!json_request(&agent, &config, req, sizeof(req))) {
        return -1;
    }
    
    if (http_request(req, resp, sizeof(resp))) return -1;
    
    if (has_tool_call(resp)) {
        if (!execute_command(resp)) {
            if (!json_request(&agent, &config, req, sizeof(req))) {
                return -1;
            }
            if (http_request(req, resp, sizeof(resp))) return -1;
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