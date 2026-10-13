#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

static void send_response(int fd, const char* status, const char* body) {
    char response[1024];
        int len = snprintf(response, sizeof response,
        "HTTP/1.1 %s\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s",
        status, strlen(body), body);
        
    if (len < 0 || (size_t)len >= sizeof response) return;  // would not fit
    write(fd, response, len);
}

static int read_file(const char* path, char* buf, size_t cap) {
    FILE *f = fopen(path, "r");
    if (!f)  return -1;
    size_t n = fread(buf, 1, cap - 1, f);
    fclose(f);
    buf[n] = '\0';
    return 0;
}

static void build_status_json(char *out, size_t cap) {
    char buf[256];
    double uptime;

    if (read_file("/proc/uptime", buf, sizeof buf) == 0
        && sscanf(buf, "%lf", &uptime) == 1) {
        snprintf(out, cap, "{\"uptime_seconds\":%.2f}\n", uptime);
    } else {
        snprintf(out, cap, "{\"uptime_seconds\":null}\n");
    }
}

int main() {

    int server = socket(AF_INET, SOCK_STREAM, 0);
    if (server < 0) { perror("socket"); return 1; }

    int yes = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(8080);

    if (bind(server, (struct sockaddr *)&addr, sizeof addr) < 0) { perror("bind"); return 1; }
    
    if (listen(server, 16) < 0) { perror("listen"); return 1; }
    printf("listening on port 8080\n");
    fflush(stdout);

    for (;;) {
        int client = accept(server, NULL, NULL);
        if (client < 0) { perror("accept"); continue; }

    char req[2048];
    ssize_t n = read(client, req, sizeof req - 1);
    if (n > 0) {
        req[n] = '\0';   // read() does not add a terminator, so you must

        char method[16], path[256];
        if (sscanf(req, "%15s %255s", method, path) == 2
            && strcmp(method, "GET") == 0
            && strcmp(path, "/status") == 0) {
                char body[512];
                build_status_json(body, sizeof body);
                send_response(client, "200 OK", body);
        } else {
            send_response(client, "404 Not Found", "{\"error\":\"not found\"}\n");
        }
    }
    close(client);
    }
}
