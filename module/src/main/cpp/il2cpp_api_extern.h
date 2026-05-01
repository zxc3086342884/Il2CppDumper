#ifndef IL2CPP_API_EXTERN_H
#define IL2CPP_API_EXTERN_H

#include <cstddef>
#include <cstdint>

// 这些函数指针在 il2cpp_dump.cpp 中定义并动态赋值
// 其他文件可以通过 extern 声明来使用它们

extern int (*il2cpp_is_vm_thread)(void*);
extern void* (*il2cpp_domain_get)();
extern void* (*il2cpp_domain_get_assemblies)(void*, size_t*);
extern void* (*il2cpp_assembly_get_image)(void*);
extern const char* (*il2cpp_image_get_name)(void*);
extern size_t (*il2cpp_image_get_class_count)(void*);
extern void* (*il2cpp_image_get_class)(void*, size_t);
extern const char* (*il2cpp_class_get_name)(void*);
extern const char* (*il2cpp_class_get_namespace)(void*);
extern void* (*il2cpp_class_get_methods)(void*, void**);
extern void* (*il2cpp_class_get_fields)(void*, void**);
extern void* (*il2cpp_class_get_properties)(void*, void**);
extern const char* (*il2cpp_method_get_name)(void*);
extern const char* (*il2cpp_method_get_param_name)(void*, uint32_t);
extern const char* (*il2cpp_field_get_name)(void*);
extern void* (*il2cpp_field_get_type)(void*);

#endif
