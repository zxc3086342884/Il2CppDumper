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

using zygisk::Api;
using zygisk::AppSpecializeArgs;
using zygisk::ServerSpecializeArgs;

class MyModule : public zygisk::ModuleBase {
public:
    void onLoad(Api *api, JNIEnv *env) override {
        this->api = api;
        this->env = env;
    }

    void preAppSpecialize(AppSpecializeArgs *args) override {
        auto package_name = env->GetStringUTFChars(args->nice_name, nullptr);
        auto app_data_dir = env->GetStringUTFChars(args->app_data_dir, nullptr);
        
        // 保存包名用于日志输出
        game_package_name = new char[strlen(package_name) + 1];
        strcpy(game_package_name, package_name);
        
        preSpecialize(package_name, app_data_dir);
        env->ReleaseStringUTFChars(args->nice_name, package_name);
        env->ReleaseStringUTFChars(args->app_data_dir, app_data_dir);
    }

    void postAppSpecialize(const AppSpecializeArgs *) override {
        if (enable_hack) {
            LOGI("Starting dump for package: %s", game_package_name);
            std::thread hack_thread(hack_prepare, game_data_dir, data, length);
            hack_thread.detach();
        }
    }

private:
    Api *api;
    JNIEnv *env;
    bool enable_hack = true;  // 默认为 true，对所有进程启用
    char *game_data_dir;
    char *game_package_name;
    void *data;
    size_t length;

    void preSpecialize(const char *package_name, const char *app_data_dir) {
        // 移除包名比对，对所有应用生效
        LOGI("Processing app: %s", package_name);
        enable_hack = true;
        game_data_dir = new char[strlen(app_data_dir) + 1];
        strcpy(game_data_dir, app_data_dir);

#if defined(__i386__)
        auto path = "zygisk/armeabi-v7a.so";
#endif
#if defined(__x86_64__)
        auto path = "zygisk/arm64-v8a.so";
#endif
#if defined(__i386__) || defined(__x86_64__)
        int dirfd = api->getModuleDir();
        int fd = openat(dirfd, path, O_RDONLY);
        if (fd != -1) {
            struct stat sb{};
            fstat(fd, &sb);
            length = sb.st_size;
            data = mmap(nullptr, length, PROT_READ, MAP_PRIVATE, fd, 0);
            close(fd);
        } else {
            LOGW("Unable to open arm file");
        }
#endif
        // 移除 setOption(DLCLOSE_MODULE_LIBRARY)，确保模块不会卸载
    }
};

REGISTER_ZYGISK_MODULE(MyModule)