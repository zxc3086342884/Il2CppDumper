#include <cstring>
#include <thread>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <cinttypes>
#include "hack.h"
#include "zygisk.hpp"
#include "game.h"
#include "log.h"
#include "feature_control.h"  // 新增

using zygisk::Api;
using zygisk::AppSpecializeArgs;
using zygisk::ServerSpecializeArgs;

class MyModule : public zygisk::ModuleBase {
public:
    void onLoad(Api *api, JNIEnv *env) override {
        this->api = api;
        this->env = env;
        LOGI("Module loaded");
    }

    void preAppSpecialize(AppSpecializeArgs *args) override {
        auto package_name = env->GetStringUTFChars(args->nice_name, nullptr);
        auto app_data_dir = env->GetStringUTFChars(args->app_data_dir, nullptr);
        
        LOGI("preAppSpecialize: package=%s, data_dir=%s", package_name, app_data_dir);
        
        preSpecialize(package_name, app_data_dir);
        
        env->ReleaseStringUTFChars(args->nice_name, package_name);
        env->ReleaseStringUTFChars(args->app_data_dir, app_data_dir);
    }

    void postAppSpecialize(const AppSpecializeArgs *) override {
        LOGI("postAppSpecialize: enable_hack=%d", enable_hack);
        
        if (enable_hack) {
            LOGI("Starting hack thread...");
            std::thread hack_thread(hack_prepare, game_data_dir, data, length);
            hack_thread.detach();
            LOGI("Hack thread detached");
        } else {
            LOGI("Hack not enabled for this process");
        }
    }

private:
    Api *api;
    JNIEnv *env;
    bool enable_hack;
    char *game_data_dir;
    void *data;
    size_t length;

    void preSpecialize(const char *package_name, const char *app_data_dir) {
        LOGI("preSpecialize: checking package %s", package_name);
        
        if (strcmp(package_name, GamePackageName) == 0) {
            LOGI("Target game detected: %s", package_name);
            enable_hack = true;
            game_data_dir = new char[strlen(app_data_dir) + 1];
            strcpy(game_data_dir, app_data_dir);
            
            // 初始化特性控制
            FeatureControl::init_from_env();
            LOGI("Enhanced dump mode: %s", FeatureControl::is_enhanced_dump() ? "ON" : "OFF");

#if defined(__i386__)
            auto path = "zygisk/armeabi-v7a.so";
            LOGI("Loading ARM 32-bit library: %s", path);
#endif
#if defined(__x86_64__)
            auto path = "zygisk/arm64-v8a.so";
            LOGI("Loading ARM 64-bit library: %s", path);
#endif
#if defined(__i386__) || defined(__x86_64__)
            int dirfd = api->getModuleDir();
            LOGI("Module dir fd: %d", dirfd);
            
            int fd = openat(dirfd, path, O_RDONLY);
            if (fd != -1) {
                struct stat sb{};
                fstat(fd, &sb);
                length = sb.st_size;
                LOGI("ARM library size: %zu bytes", length);
                
                data = mmap(nullptr, length, PROT_READ, MAP_PRIVATE, fd, 0);
                LOGI("ARM library mapped at: %p", data);
                close(fd);
            } else {
                LOGE("Unable to open ARM file: %s", path);
            }
#else
            LOGI("Running on native ARM architecture");
#endif
        } else {
            LOGI("Not target game, unloading module");
            api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
        }
    }
};

REGISTER_ZYGISK_MODULE(MyModule)