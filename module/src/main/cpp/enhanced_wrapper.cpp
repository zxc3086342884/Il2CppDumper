#include "enhanced_wrapper.h"
#include "il2cpp_dump.h"
#include "log.h"
#include "xdl.h"
#include <unistd.h>
#include <fstream>
#include <sstream>
#include <cstring>
#include <sys/stat.h>

// 全局开关，默认关闭
static bool g_enhanced_enabled = false;

bool is_enhanced_dump_enabled() {
    return g_enhanced_enabled;
}

void set_enhanced_dump_enabled(bool enabled) {
    g_enhanced_enabled = enabled;
    LOGI("Enhanced dump %s", enabled ? "ENABLED" : "DISABLED");
}

// 快速统计类数量（不修改原逻辑）
static int get_class_count_quick() {
    int count = 0;
    
    LOGD("Quick counting classes...");
    
    if (!il2cpp_domain_get) {
        LOGW("il2cpp_domain_get not available for counting");
        return -1;
    }
    
    if (!il2cpp_domain_get_assemblies) {
        LOGW("il2cpp_domain_get_assemblies not available for counting");
        return -1;
    }
    
    size_t size = 0;
    auto domain = il2cpp_domain_get();
    if (!domain) {
        LOGW("Domain is null");
        return -1;
    }
    
    auto assemblies = il2cpp_domain_get_assemblies(domain, &size);
    if (!assemblies) {
        LOGW("Assemblies array is null");
        return -1;
    }
    
    LOGD("Found %zu assemblies", size);
    
    for (size_t i = 0; i < size; i++) {
        auto image = il2cpp_assembly_get_image(assemblies[i]);
        if (!image) {
            LOGW("Assembly[%zu] image is null", i);
            continue;
        }
        
        const char* image_name = "unknown";
        if (il2cpp_image_get_name) {
            image_name = il2cpp_image_get_name(image);
        }
        
        int class_in_image = 0;
        if (il2cpp_image_get_class_count) {
            class_in_image = il2cpp_image_get_class_count(image);
            count += class_in_image;
        } else {
            LOGD("il2cpp_image_get_class_count not available for %s", image_name);
        }
        
        LOGD("Assembly[%zu] %s: %d classes", i, image_name, class_in_image);
    }
    
    LOGD("Total classes counted: %d", count);
    return count;
}

// 检查IL2CPP是否稳定
static bool wait_for_stable_state(int max_wait_seconds) {
    LOGI("Waiting for stable IL2CPP state (max %d seconds)...", max_wait_seconds);
    
    int stable_count = 0;
    int last_count = -1;
    int initial_count = -1;
    bool domain_ready = false;
    
    for (int i = 0; i < max_wait_seconds; i++) {
        // 检查domain是否可用
        if (!domain_ready) {
            if (il2cpp_is_vm_thread && il2cpp_is_vm_thread(nullptr)) {
                domain_ready = true;
                LOGI("IL2CPP VM thread detected at second %d", i + 1);
            } else {
                LOGD("Waiting for IL2CPP VM... (%d/%d)", i + 1, max_wait_seconds);
                sleep(1);
                continue;
            }
        }
        
        int current_count = get_class_count_quick();
        
        // 记录初始值
        if (initial_count == -1) {
            initial_count = current_count;
            LOGI("Initial class count: %d", initial_count);
        }
        
        // 检查稳定状态
        if (current_count > 0 && current_count == last_count) {
            stable_count++;
            LOGD("Stable count: %d/%d (classes: %d)", stable_count, 3, current_count);
            
            if (stable_count >= 3) {
                LOGI("IL2CPP state stable after %d seconds", i + 1);
                LOGI("Final class count: %d (initial: %d, growth: %d)", 
                     current_count, initial_count, current_count - initial_count);
                return true;
            }
        } else {
            if (current_count > last_count && last_count > 0) {
                LOGD("Class count increased: %d -> %d", last_count, current_count);
            }
            stable_count = 0;
            last_count = current_count;
        }
        
        sleep(1);
    }
    
    LOGW("Timeout waiting for stable state after %d seconds", max_wait_seconds);
    LOGW("Last class count: %d, initial: %d", last_count, initial_count);
    return false;
}

// 检查API可用性
static void check_api_availability() {
    LOGI("=== IL2CPP API Availability Check ===");
    
    struct ApiCheck {
        const char* name;
        bool available;
        bool critical;  // 关键API
    };
    
    ApiCheck checks[] = {
        {"il2cpp_domain_get", il2cpp_domain_get != nullptr, true},
        {"il2cpp_domain_get_assemblies", il2cpp_domain_get_assemblies != nullptr, true},
        {"il2cpp_assembly_get_image", il2cpp_assembly_get_image != nullptr, true},
        {"il2cpp_image_get_name", il2cpp_image_get_name != nullptr, false},
        {"il2cpp_image_get_class_count", il2cpp_image_get_class_count != nullptr, true},
        {"il2cpp_image_get_class", il2cpp_image_get_class != nullptr, true},
        {"il2cpp_class_get_name", il2cpp_class_get_name != nullptr, true},
        {"il2cpp_class_get_namespace", il2cpp_class_get_namespace != nullptr, false},
        {"il2cpp_class_get_methods", il2cpp_class_get_methods != nullptr, true},
        {"il2cpp_class_get_fields", il2cpp_class_get_fields != nullptr, true},
        {"il2cpp_class_get_properties", il2cpp_class_get_properties != nullptr, false},
        {"il2cpp_method_get_name", il2cpp_method_get_name != nullptr, true},
        {"il2cpp_method_get_param_name", il2cpp_method_get_param_name != nullptr, false},
        {"il2cpp_field_get_name", il2cpp_field_get_name != nullptr, true},
        {"il2cpp_field_get_type", il2cpp_field_get_type != nullptr, true},
    };
    
    int missing_critical = 0;
    int missing_total = 0;
    
    for (auto& check : checks) {
        LOGI("  %s: %s%s", 
             check.available ? "✓" : "✗",
             check.name,
             check.critical ? " [CRITICAL]" : "");
        
        if (!check.available) {
            missing_total++;
            if (check.critical) missing_critical++;
        }
    }
    
    if (missing_critical > 0) {
        LOGE("WARNING: %d critical APIs missing!", missing_critical);
    }
    if (missing_total > 0) {
        LOGW("Total missing APIs: %d/%d", missing_total, sizeof(checks)/sizeof(checks[0]));
    }
}

// 验证dump结果
// 验证dump结果
static bool verify_dump_result(const char* outDir, DumpStatistics* stats) {
    char path[512];
    snprintf(path, sizeof(path), "%s/files/dump.cs", outDir);
    
    LOGI("Verifying dump file: %s", path);
    
    // 检查文件是否存在
    struct stat file_stat;
    if (stat(path, &file_stat) != 0) {
        LOGE("Dump file not found: %s", path);
        return false;
    }
    
    LOGI("Dump file size: %ld bytes", file_stat.st_size);
    
    // 读取文件内容
    std::ifstream file(path);
    if (!file.is_open()) {
        LOGE("Cannot open dump file for verification");
        return false;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    file.close();
    
    // 统计内容
    int class_count = 0, method_count = 0, field_count = 0, property_count = 0;
    std::istringstream stream(content);
    std::string line;
    int line_number = 0;
    
    while (std::getline(stream, line)) {
        line_number++;
        
        // 统计类/结构体/接口/枚举
        if (line.find(" class ") != std::string::npos ||
            line.find(" struct ") != std::string::npos ||
            line.find(" interface ") != std::string::npos ||
            line.find(" enum ") != std::string::npos) {
            class_count++;
            LOGD("Line %d: Found type - %s", line_number, line.c_str());
        }
        
        // 统计方法
        if (line.find("// RVA:") != std::string::npos) {
            method_count++;
        }
        
        // 统计字段
        if (line.find("; // 0x") != std::string::npos) {
            field_count++;
        }
        
        // 统计属性
        if (line.find("get;") != std::string::npos || 
            line.find("set;") != std::string::npos) {
            property_count++;
        }
    }
    
    LOGI("=== Dump Verification Results ===");
    LOGI("  Classes: %d", class_count);
    LOGI("  Methods: %d", method_count);
    LOGI("  Fields: %d", field_count);
    LOGI("  Properties: %d", property_count);
    LOGI("  Total lines: %d", line_number);
    
    // 完整性检查
    std::string missing;
    if (class_count == 0) missing += "classes ";
    if (method_count == 0 && class_count > 0) missing += "methods ";
    if (field_count == 0 && class_count > 0) missing += "fields ";
    
    bool is_complete = (class_count > 0);
    
    // 填充统计信息（如果提供了stats指针）
    if (stats) {
        stats->classes_count = class_count;
        stats->methods_count = method_count;
        stats->fields_count = field_count;
        stats->properties_count = property_count;
        stats->is_complete = is_complete;
        
        if (missing.empty()) {
            strncpy(stats->missing_parts, "none", sizeof(stats->missing_parts) - 1);
        } else {
            strncpy(stats->missing_parts, missing.c_str(), sizeof(stats->missing_parts) - 1);
        }
        stats->missing_parts[sizeof(stats->missing_parts) - 1] = '\0';
    }
    
    // 输出验证结果
    if (is_complete) {
        LOGI("Dump verification: PASSED");
    } else {
        LOGE("Dump verification: FAILED - Missing: %s", missing.c_str());
    }
    
    return is_complete;
}

// 增强dump主函数
bool enhanced_dump_with_fallback(void* il2cpp_handle, const char* outDir, DumpStatistics* stats) {
    LOGI("========================================");
    LOGI("Enhanced IL2CPP Dump Starting...");
    LOGI("========================================");
    
    // 1. 检查API可用性
    LOGI("[Step 1/5] Checking API availability...");
    check_api_availability();
    
    // 2. 等待稳定状态
    LOGI("[Step 2/5] Waiting for stable state...");
    bool stable = wait_for_stable_state(30); // 最多等30秒
    
    if (!stable) {
        LOGW("IL2CPP did not reach stable state, proceeding anyway...");
    }
    
    // 3. 记录dump前状态
    LOGI("[Step 3/5] Recording pre-dump state...");
    int pre_class_count = get_class_count_quick();
    LOGI("Pre-dump class count: %d", pre_class_count);
    
    // 4. 执行原始dump
    LOGI("[Step 4/5] Executing original dump...");
    auto start_time = time(nullptr);
    
    il2cpp_dump(outDir);
    
    auto end_time = time(nullptr);
    LOGI("Dump execution time: %ld seconds", end_time - start_time);
    
    // 5. 验证结果
    LOGI("[Step 5/5] Verifying dump results...");
    DumpStatistics local_stats;
    bool verified = verify_dump_result(outDir, stats ? stats : &local_stats);
    
    // 输出最终统计
    LOGI("========================================");
    LOGI("Enhanced Dump Summary:");
    LOGI("  Pre-dump classes: %d", pre_class_count);
    LOGI("  Dumped classes: %d", stats ? stats->classes_count : local_stats.classes_count);
    LOGI("  Dumped methods: %d", stats ? stats->methods_count : local_stats.methods_count);
    LOGI("  Dumped fields: %d", stats ? stats->fields_count : local_stats.fields_count);
    LOGI("  Verification: %s", verified ? "PASSED" : "FAILED");
    
    if (!verified) {
        LOGW("Dump verified FAILED, but file may still be usable");
        LOGW("Missing parts: %s", stats ? stats->missing_parts : "unknown");
    }
    
    LOGI("========================================");
    
    return verified;
}