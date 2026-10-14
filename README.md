# pi-status

A tiny system status server, written twice, in C and in Rust, to compare the two side by side.
Both versions use only their standard libraries (no frameworks) and follow the same spec.

Built to run on Linux (WSL, Debian, Raspberry Pi OS). It reads from `/proc` and `/sys`.

## Spec

- Listens on `0.0.0.0:8080`.
- `GET /status` returns `200` with `Content-Type: application/json`:

```json
{"hostname":"mypc","uptime_seconds":12345.67,"load_avg":[0.12,0.34,0.56],"mem_total_kb":8000000,"mem_available_kb":4000000,"cpu_temp_c":48.3}
```

- Any field that can't be read is `null` (for example `cpu_temp_c` on WSL, which has no thermal sensor).
- Any other path or method returns `404` with `{"error":"not found"}`.
- Each response closes the connection.
- Each connection is handled in its own thread.

## Layout

```
c/       C version
rust/    Rust version
```

## Run

```sh
# C
cd c
gcc -Wall -Wextra -pthread -o pi-status main.c
./pi-status

# Rust
cd rust
cargo run --release

# Test (either one)
curl -i localhost:8080/status
```

## Status

- [x] C version: complete
- [ ] Rust version: in progress

## Ideas for later

- Disk usage
- A thread pool, then epoll (C) and Tokio (Rust)
- Move sensor reads behind functions so a fake version can run on non-Pi machines
- An HTML page that polls `/status`
- A systemd unit to run it on boot
- Compare lines of code, binary size, memory use, and bugs hit along the way