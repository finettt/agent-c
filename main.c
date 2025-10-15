#include "agent-c.h"

Agent agent;
Config config;

void cleanup(int sig) {
    (void)sig;
    // Здесь можно добавить очистку ресурсов при необходимости
    _exit(0);
}

int main(void) {
    signal(SIGINT, cleanup);
    signal(SIGTERM, cleanup);
    
    load_config();
    
    if (!config.api_key[0]) {
        fprintf(stderr, "OPENAI_KEY required\n");
        return 1;
    }
    if (!config.api_url[0]) {
        fprintf(stderr, "OPENAI_BASE required\n");
        return 1;
    }
    
    init_agent();
    run_cli();
    
    return 0;
}
