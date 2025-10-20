#include "agent-c.h"

Agent agent;
Config config;

void cleanup(int sig) {
    (void)sig;
    exit(0);
}

int main(int argc, char* argv[]) {
    signal(SIGINT, cleanup);
    signal(SIGTERM, cleanup);
    
    load_config();
    
    if (parse_args(argc, argv) != 0) {
        return 1;
    }
    
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
