// zygisk.cpp
#include <sys/types.h>
#include <zygisk.hpp>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <dlfcn.h>
#include <ctime>

#define LOG_FILE "/sdcard/zygisk_module.log"
#define TARGET_APP "com.dts.freefireth"

static FILE* log_fp = nullptr;

void init_log() {
    log_fp = fopen(LOG_FILE, "a");
    if (!log_fp) {
        log_fp = fopen("/sdcard/zygisk_error.txt", "a");
    }
}

void write_log(const char* format, ...) {
    if (!log_fp) init_log();
    
    if (log_fp) {
        time_t now = time(nullptr);
        struct tm* timeinfo = localtime(&now);
        char time_str[32];
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", timeinfo);
        
        fprintf(log_fp, "[%s] ", time_str);
        
        va_list args;
        va_start(args, format);
        vfprintf(log_fp, format, args);
        va_end(args);
        
        fprintf(log_fp, "\n");
        fflush(log_fp);
    }
}

bool detect_process(const char* app_name) {
    write_log("Detecting: %s", app_name ? app_name : "NULL");
    if (!app_name) return false;
    
    bool match = (strcmp(app_name, TARGET_APP) == 0);
    write_log("Match result: %d", match);
    return match;
}

void* inject_library(const char* lib_path) {
    write_log("Attempting to inject: %s", lib_path);
    
    if (access(lib_path, F_OK) != 0) {
        write_log("Library NOT FOUND at: %s", lib_path);
        return nullptr;
    }
    
    write_log("Library found, loading...");
    void* handle = dlopen(lib_path, RTLD_LAZY);
    
    if (handle) {
        write_log("SUCCESS: Library injected: %s", lib_path);
        return handle;
    } else {
        write_log("FAILED to inject: %s", dlerror());
        return nullptr;
    }
}

class GameModuleInjector : public zygisk::ModuleBase {
private:
    zygisk::Api* api = nullptr;
    JNIEnv* env = nullptr;
    void* lib_handle = nullptr;
    
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
            write_log("App name from args: %s", nice_name);
            
            if (detect_process(nice_name)) {
                write_log("TARGET DETECTED! Preparing injection");
            }
            
            env->ReleaseStringUTFChars(args->nice_name, nice_name);
        }
    }
    
    void postAppSpecialize(const zygisk::AppSpecializeArgs *args) override {
        write_log("POST-SPECIALIZE: Starting");
        
        const char* lib_path = "/sdcard/lib2offs1.so";
        write_log("Looking for library at: %s", lib_path);
        
        lib_handle = inject_library(lib_path);
        
        if (lib_handle) {
            write_log("Library handle valid, injection successful");
        } else {
            write_log("Library injection FAILED");
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
