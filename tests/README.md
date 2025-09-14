# 测试说明

1. ## `core`

    - `test_core_device.cpp`: 测试`SystemDevice`类的基本功能。\
      需要注意的是，在 *Windows* 系统上，创建符号链接需要管理员权限，因此在测试套件初始化时会弹出一个UAC弹窗，点击确定即可创建符号链接并测试，否则会跳过符号链接的相关测试。\
      此外，使用`MinGW`编译时，可能会出现无法删除测试文件夹的情况，删除`cmake-build-debug/bin/test`后重新运行测试即可。
