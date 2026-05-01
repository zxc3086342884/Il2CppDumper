// feature_control.h - 完全header-only版本
#ifndef FEATURE_CONTROL_H
#define FEATURE_CONTROL_H

#include <cstdlib>
#include <cstring>

class FeatureControl {
public:
    // 从环境变量初始化
    static void init_from_env() {
        const char* enhanced = getenv("ENHANCED_IL2CPP_DUMP");
        if (enhanced && strcmp(enhanced, "1") == 0) {
            get_enhanced_dump_ref() = true;
        }
        
        const char* log_level = getenv("IL2CPP_DUMP_LOG_LEVEL");
        if (log_level) {
            get_log_level_ref() = atoi(log_level);
        }
    }
    
    static void set_enhanced_dump(bool enabled) {
        get_enhanced_dump_ref() = enabled;
    }
    
    static bool is_enhanced_dump() {
        return get_enhanced_dump_ref();
    }
    
    static void set_log_level(int level) {
        get_log_level_ref() = level;
    }
    
    static int get_log_level() {
        return get_log_level_ref();
    }

private:
    // 使用函数内静态变量避免多重定义问题
    static bool& get_enhanced_dump_ref() {
        static bool enhanced_dump = false;
        return enhanced_dump;
    }
    
    static int& get_log_level_ref() {
        static int log_level = 3; // 默认INFO
        return log_level;
    }
};

#endif