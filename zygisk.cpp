#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dlfcn.h>
#include <cstring>
#include <cstdio>
#include <pthread.h>
#include <zygisk.hpp>

using zygisk::Api;
using zygisk::AppSpecializeArgs;

#define TARGET_APP "com.dts.freefireth"
#define SOURCE_LIB "/data/local/tmp/lib2offs1.so"

// تعريف يدوي إذا لم تتوفر في headers
#ifndef SYS_memfd_create
#define SYS_memfd_create __NR_memfd_create
#endif

#ifndef MFD_CLOEXEC
#define MFD_CLOEXEC 0x0001U
#endif

static void* load_library_memfd(const char* path) {
    int src = open(path, O_RDONLY);
    if (src < 0) return nullptr;

    int fd = syscall(SYS_memfd_create, "lib2offs1.so", MFD_CLOEXEC);
    if (fd < 0) {
        close(src);
        return nullptr;
    }

    char buf[4096];
    ssize_t n;
    while ((n = read(src, buf, sizeof(buf))) > 0) {
        if (write(fd, buf, n) != n) {
            close(src);
            close(fd);
            return nullptr;
        }
    }
    close(src);

    char fd_path[64];
    snprintf(fd_path, sizeof(fd_path), "/proc/self/fd/%d", fd);
    void* handle = dlopen(fd_path, RTLD_NOW);
    close(fd);

    unlink(SOURCE_LIB);

    return handle;
}

static void* delayed_load(void*) {
    sleep(20);
    load_library_memfd(SOURCE_LIB);
    return nullptr;
}

class GameModule : public zygisk::ModuleBase {
private:
    zygisk::Api* api = nullptr;
    JNIEnv* env = nullptr;
    bool is_target = false;

public:
    void onLoad(Api *api, JNIEnv *env) override {
        this->api = api;
        this->env = env;
    }

    void preAppSpecialize(AppSpecializeArgs *args) override {
        if (!args || !args->nice_name) return;
        const char* process = env->GetStringUTFChars(args->nice_name, nullptr);
        if (process) {
            is_target = (strcmp(process, TARGET_APP) == 0);
            env->ReleaseStringUTFChars(args->nice_name, process);
        }
    }

    void postAppSpecialize(const AppSpecializeArgs *args) override {
        if (!is_target) return;
        is_target = false;

        pthread_t tid;
        if (pthread_create(&tid, nullptr, delayed_load, nullptr) == 0) {
            pthread_detach(tid);
        }
    }
};

REGISTER_ZYGISK_MODULE(GameModule)
