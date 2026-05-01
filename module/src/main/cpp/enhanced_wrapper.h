#ifndef ENHANCED_WRAPPER_H
#define ENHANCED_WRAPPER_H

#include <cstddef>
#include <cstdint>

// 增强dump结果统计
struct DumpStatistics {
    int assemblies_count;
    int classes_count;
    int methods_count;
    int fields_count;
    int properties_count;
    bool is_complete;
    char missing_parts[256];  // 修复：使用固定缓冲区
};

// 增强dump，带完整日志和回退
bool enhanced_dump_with_fallback(void* il2cpp_handle, const char* outDir, DumpStatistics* stats = nullptr);

// 检查增强功能是否启用
bool is_enhanced_dump_enabled();

// 设置增强功能开关（用于调试）
void set_enhanced_dump_enabled(bool enabled);

#endif