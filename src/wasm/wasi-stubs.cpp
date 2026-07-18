#pragma clang diagnostic ignored "-Wreserved-identifier"
#pragma clang diagnostic ignored "-Wunused-parameter"
#pragma clang diagnostic ignored "-Wmissing-noreturn"

extern "C" {
__attribute__((import_module("zith"), import_name("host_write"))) void
host_write(int stream, const char *ptr, int len);

int __imported_wasi_snapshot_preview1_args_get(int a, int b) {
    return 52;
}
int __imported_wasi_snapshot_preview1_args_sizes_get(int a, int b) {
    return 52;
}
int __imported_wasi_snapshot_preview1_clock_time_get(unsigned int clock_id,
                                                     unsigned long long precision,
                                                     unsigned long long *time_out) {
    static unsigned long long fake_time = 0;
    if (time_out)
        *time_out = fake_time;
    fake_time += 1000000;
    return 0;
}
int __imported_wasi_snapshot_preview1_fd_close(int a) {
    return 52;
}
int __imported_wasi_snapshot_preview1_fd_fdstat_get(int a, int b) {
    return 52;
}
int __imported_wasi_snapshot_preview1_fd_prestat_get(int a, int b) {
    return 52;
}
int __imported_wasi_snapshot_preview1_fd_prestat_dir_name(int a, int b, int c) {
    return 52;
}
int __imported_wasi_snapshot_preview1_fd_readdir(int a, int b, int c, long long d, int e) {
    return 52;
}
int __imported_wasi_snapshot_preview1_fd_seek(int a, long long b, int c, int d) {
    return 52;
}
int __imported_wasi_snapshot_preview1_fd_write(int fd, int iovs_ptr, int iovs_len, int *nwritten) {
    int total = 0;
    struct ciovec_t {
        unsigned int buf;
        unsigned int buf_len;
    };
    auto *iovs = reinterpret_cast<const ciovec_t *>(iovs_ptr);
    for (int i = 0; i < iovs_len; ++i) {
        if (iovs[i].buf_len > 0) {
            host_write(fd, reinterpret_cast<const char *>(iovs[i].buf),
                       static_cast<int>(iovs[i].buf_len));
            total += static_cast<int>(iovs[i].buf_len);
        }
    }
    if (nwritten) {
        *nwritten = total;
    }
    return 0;
}
int __imported_wasi_snapshot_preview1_path_create_directory(int a, int b, int c) {
    return 52;
}
int __imported_wasi_snapshot_preview1_path_filestat_get(int a, int b, int c, int d, int e) {
    return 52;
}
int __imported_wasi_snapshot_preview1_path_open(int a, int b, int c, int d, int e, long long f,
                                                long long g, int h, int i) {
    return 52;
}
int __imported_wasi_snapshot_preview1_path_readlink(int a, int b, int c, int d, int e, int f) {
    return 52;
}

__attribute__((noreturn)) void __imported_wasi_snapshot_preview1_proc_exit(int a) {
    __builtin_trap();
}

int __imported_wasi_snapshot_preview1_environ_get(int environ, int environ_buf) {
    return 52;
}
int __imported_wasi_snapshot_preview1_environ_sizes_get(int environ_count, int environ_size) {
    return 52;
}

int __imported_wasi_snapshot_preview1_random_get(int buf, int buf_len) {
    return 52;
}
}
