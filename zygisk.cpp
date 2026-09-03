#include <sys/types.h>
#include <unistd.h>
#include <dlfcn.h>
#include <pthread.h>
#include <cstring>
#include <cstdio>
#include <zygisk.hpp>

using zygisk::Api;
using zygisk::AppSpecializeArgs;

#define TARGET_APP "com.dts.freefireth"
#define SOURCE_LIB "/data/local/tmp/lib2offs1.so"

static void* delayed_load(void*) {
    sleep(20); // انتظار 20 ثانية بعد بدء اللعبة
    void* handle = dlopen(SOURCE_LIB, RTLD_NOW);
    if (handle) {
        // بعد التحميل، يمكن حذف الملف لتقليل الأثر
        unlink(SOURCE_LIB);
    }
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
