//
// Created by ycm on 25-9-5.
//
#include <gtest/gtest.h>

#if defined _MSC_VER && defined _DEBUG
// enable CRT debug heap for memory leak detection
#define _CRTDBG_MAP_ALLOC
#include <cstdlib>
#include <crtdbg.h>
#endif

int main(int argc, char **argv) {
#if defined _MSC_VER && defined _DEBUG
    // Dump memory leaks
    _CrtSetDbgFlag ( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_DEBUG);
    _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
    printf("Enable CRT debug heap for memory leak detection.\n");
#endif

    printf("Running main() from %s\n", __FILE__);
    testing::InitGoogleTest(&argc, argv);
    const auto result = RUN_ALL_TESTS();

#if defined _MSC_VER && defined _DEBUG
    // Dump memory leaks
    _CrtDumpMemoryLeaks();
#endif
    return result;
}
