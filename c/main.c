#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdarg.h>

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

static void append(char *out, size_t cap, size_t *len, const char *fmt, ...) {
    if (*len >= cap) return;
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(out + *len, cap - *len, fmt, ap);
    va_end(ap);
    if (n > 0) {
        *len += (size_t)n;
        if (*len > cap) *len = cap;
    }
}

static int read_load_avg(double *l1, double *l5, double *l15) {
    char buf[256];
    if (read_file("/proc/loadavg", buf, sizeof buf) != 0) return -1;
    if (sscanf(buf, "%lf %lf %lf", l1, l5, l15) != 3) return -1;
    return 0;
}

static void build_status_json(char *out, size_t cap) {
    char buf[256];

    /* ---- phase 1: read the values ---- */
    char host[256] = "unknown";
    if (read_file("/proc/sys/kernel/hostname", buf, sizeof buf) == 0) {
        buf[strcspn(buf, "\r\n")] = '\0';        // cut off the trailing newline
        if (buf[0] != '\0') snprintf(host, sizeof host, "%s", buf);
    }

    double uptime = 0;
    int have_uptime = 0;
    if (read_file("/proc/uptime", buf, sizeof buf) == 0
        && sscanf(buf, "%lf", &uptime) == 1) {
        have_uptime = 1;
    }

    double l1 = 0, l5 = 0, l15 = 0;
    int have_load = (read_load_avg(&l1, &l5, &l15) == 0);

    /* ---- phase 2: build the JSON ---- */
    size_t len = 0;
    append(out, cap, &len, "{\"hostname\":\"%s\",", host);
    if (have_uptime) append(out, cap, &len, "\"uptime_seconds\":%.2f,", uptime);
    else             append(out, cap, &len, "\"uptime_seconds\":null,");
    if (have_load)   append(out, cap, &len, "\"load_avg\":[%.2f,%.2f,%.2f]", l1, l5, l15);
    else             append(out, cap, &len, "\"load_avg\":null");
    append(out, cap, &len, "}\n");
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
