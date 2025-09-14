# 测试说明

1. ## `core`

    - `test_core_device.cpp`: 测试`SystemDevice`类的基本功能。\
      需要注意的是，在 *Windows* 系统上，创建符号链接需要管理员权限，因此在测试套件初始化时会弹出一个UAC弹窗，点击确定即可创建符号链接并测试，否则会跳过符号链接的相关测试。\
      此外，使用`MinGW`编译时，可能会出现无法删除测试文件夹的情况（疑似无法删除只读文件），删除`cmake-build-debug/bin/test`后重新运行测试即可。\
      由于 *Windows* 系统对文件和文件夹的`access time`更新逻辑问题，虽然正确设置了`access time`，但仍可能出现后面读取验证时时间不一致的可能，因此通过`CHECK_ACCESS_TIME`宏来跳过对`access time`的验证。
