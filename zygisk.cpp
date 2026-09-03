#include <sys/types.h>
#include <zygisk.hpp>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <dlfcn.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstdint>
#include <ctime>
#include <cstdarg>

#define LOG_FILE "/data/local/tmp/freefireth_injector.log"
#define TARGET_APP "com.dts.freefireth"
#define PORT 5555
#define SET_TIME_SCALE_OFFSET 0x6508b4c

static FILE* log_fp = nullptr;

void init_log() {
    log_fp = fopen(LOG_FILE, "a");
    if (!log_fp) {
        log_fp = fopen("/data/local/tmp/zygisk_error.txt", "a");
    }
}

void write_log(const char* format, ...) {
    if (!log_fp) init_log();
    if (!log_fp) return;

    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    char time_str[32];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", t);
    fprintf(log_fp, "[%s] ", time_str);

    va_list args;
    va_start(args, format);
    vfprintf(log_fp, format, args);
    va_end(args);

    fprintf(log_fp, "\n");
    fflush(log_fp);
}

static uintptr_t get_module_base(const char* module_name) {
    FILE* fp = fopen("/proc/self/maps", "r");
    if (!fp) return 0;
    char line[512];
    uintptr_t base = 0;
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, module_name)) {
            sscanf(line, "%x", &base);
            break;
        }
    }
    fclose(fp);
    return base;
}

static void set_time_scale(float scale) {
    uintptr_t base = get_module_base("libil2cpp.so");
    if (base == 0) {
        write_log("set_time_scale: libil2cpp.so base not found");
        return;
    }
    uintptr_t funcAddr = base + SET_TIME_SCALE_OFFSET;
    void (*SetTimeScale)(float) = (void(*)(float))funcAddr;
    SetTimeScale(scale);
    write_log(scale == 1.0f ? "Time scale reset to 1.0" : "Time scale set to 2.0");
}

static void* socket_thread(void*) {
    write_log("Socket thread started");
    int server_fd, client_fd;
    struct sockaddr_in address;
    int opt = 1;
    socklen_t addrlen = sizeof(address);
    char buffer[16] = {0};

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        write_log("Socket creation failed");
        return nullptr;
    }

    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        write_log("Socket bind failed");
        close(server_fd);
        return nullptr;
    }

    if (listen(server_fd, 1) < 0) {
        write_log("Socket listen failed");
        close(server_fd);
        return nullptr;
    }

    write_log("Socket listening on port 5555");

    while (true) {
        client_fd = accept(server_fd, (struct sockaddr*)&address, &addrlen);
        if (client_fd < 0) continue;

        memset(buffer, 0, sizeof(buffer));
        recv(client_fd, buffer, sizeof(buffer), 0);
        write_log("Received command: %s", buffer);

        if (strcmp(buffer, "on") == 0) {
            set_time_scale(2.0f);
        } else if (strcmp(buffer, "off") == 0) {
            set_time_scale(1.0f);
        }

        close(client_fd);
    }

    close(server_fd);
    return nullptr;
}

class GameModuleInjector : public zygisk::ModuleBase {
private:
    zygisk::Api* api = nullptr;
    JNIEnv* env = nullptr;
    bool is_target = false;

public:
    void onLoad(zygisk::Api *api, JNIEnv *env) override {
        init_log();
        write_log("===== ZYGISK MODULE LOADED =====");
        this->api = api;
        this->env = env;
    }

    void preAppSpecialize(zygisk::AppSpecializeArgs *args) override {
        write_log("PRE-SPECIALIZE: Starting");
        const char* nice_name = nullptr;
        if (args && args->nice_name) {
            nice_name = env->GetStringUTFChars(args->nice_name, nullptr);
            write_log("Process name: %s", nice_name ? nice_name : "NULL");
            if (nice_name) {
                is_target = (strcmp(nice_name, TARGET_APP) == 0);
                write_log(is_target ? "TARGET DETECTED" : "Not target, skipping");
                env->ReleaseStringUTFChars(args->nice_name, nice_name);
            }
        }
    }

    void postAppSpecialize(const zygisk::AppSpecializeArgs *args) override {
        write_log("POST-SPECIALIZE: Starting");
        if (!is_target) {
            write_log("Not target, exiting post specialize");
            return;
        }
        is_target = false;
        write_log("Target process confirmed, preparing injection");

        const char* lib_path = "/data/local/tmp/lib2offs1.so";
        write_log("Attempting to load library from: %s", lib_path);

        if (access(lib_path, F_OK) != 0) {
            write_log("Library NOT FOUND at: %s", lib_path);
            return;
        }

        void* handle = dlopen(lib_path, RTLD_NOW);
        if (handle) {
            write_log("Library loaded successfully");
            pthread_t tid;
            if (pthread_create(&tid, nullptr, socket_thread, nullptr) == 0) {
                pthread_detach(tid);
                write_log("Socket thread created");
            } else {
                write_log("Failed to create socket thread");
            }
        } else {
            write_log("Failed to load library: %s", dlerror());
        }
        write_log("POST-SPECIALIZE: Completed");
    }

    void postServerSpecialize(const zygisk::ServerSpecializeArgs *args) override {
        write_log("Server specialize called");
        if (log_fp) {
            fclose(log_fp);
            log_fp = nullptr;
        }
    }
};

REGISTER_ZYGISK_MODULE(GameModuleInjector)
