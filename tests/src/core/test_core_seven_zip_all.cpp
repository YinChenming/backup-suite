#include <gtest/gtest.h>
#include <filesystem>
#include <string>
#include <vector>
#include <algorithm>

#include "filesystem/seven_zip_device.h"
#include "filesystem/system_device.h"
#include "backup/backup_controller.h"
#include "core/core_utils.h"
#include "utils/tmpfile.h"

namespace fs = std::filesystem;
using namespace tmpfile_utils;

// 检测 7z/7za CLI 是否可用（用于真实 .7z 写入用例的跳过控制）
static bool has_7z_cli() {
#ifdef _WIN32
    int code7z  = std::system("where 7z >nul 2>&1");
    int code7za = std::system("where 7za >nul 2>&1");
    return code7z == 0 || code7za == 0;
#else
    int code7z  = std::system("which 7z >/dev/null 2>&1");
    int code7za = std::system("which 7za >/dev/null 2>&1");
    return code7z == 0 || code7za == 0;
#endif
}

// 1) 基础构造与默认行为
TEST(CoreSevenZipDevice, ConstructAndDefaults)
{
    const auto tmp7z = fs::temp_directory_path() / "test_sevenzip_defaults.7z";
    if (fs::exists(tmp7z)) fs::remove(tmp7z);
    SevenZipDevice dev{tmp7z, SevenZipDevice::Mode::WriteOnly};
    EXPECT_TRUE(dev.is_open());
    dev.set_compression(CompressionMethod::LZMA2);
    dev.set_encryption(EncryptionMethod::AES256);
    dev.set_password(std::vector<uint8_t>{'p','a','s','s'});
    EXPECT_FALSE(dev.is_invalid_password());
}

// 用于读写往返的测试桩
class TestReadableFile final : public ReadableFile {
    std::vector<std::byte> data_{}; size_t cursor_ = 0;
public:
    TestReadableFile(fs::path p, std::string s) {
        FileEntityMeta m{}; m.path = std::move(p); m.type = FileEntityType::RegularFile; m.size = s.size();
        data_.resize(s.size()); std::memcpy(data_.data(), s.data(), s.size()); this->File::FileEntity::meta = m;
    }
    [[nodiscard]] std::unique_ptr<std::vector<std::byte>> read() override {
        if (cursor_ >= data_.size()) return nullptr;
        auto out = std::make_unique<std::vector<std::byte>>(data_.begin() + static_cast<long long>(cursor_), data_.end());
        cursor_ = data_.size(); return out; }
    [[nodiscard]] std::unique_ptr<std::vector<std::byte>> read(size_t n) override {
        if (cursor_ >= data_.size() || n == 0) return nullptr; const auto remain = data_.size() - cursor_;
        const auto take = (std::min)(remain, n);
        auto out = std::make_unique<std::vector<std::byte>>(data_.begin() + static_cast<long long>(cursor_), data_.begin() + static_cast<long long>(cursor_ + take));
        cursor_ += take; return out; }
    void close() override { cursor_ = data_.size(); }
};

// 2) 写入并读取（进程级内存目录）
TEST(CoreSevenZipDevice, WriteReadRoundtripInMemoryBackend)
{
    const auto tmp7z = fs::temp_directory_path() / "test_sevenzip_inmem.7z";
    if (fs::exists(tmp7z)) fs::remove(tmp7z);
    SevenZipDevice writer{tmp7z, SevenZipDevice::Mode::WriteOnly};
    ASSERT_TRUE(writer.is_open());
    writer.set_compression(CompressionMethod::LZMA2);
    writer.set_encryption(EncryptionMethod::None);

    TestReadableFile rf{"docs/foo.txt", "Hello 7z in-memory"};
    ASSERT_TRUE(writer.write_file(rf));

    SevenZipDevice reader{tmp7z, SevenZipDevice::Mode::ReadOnly};
    ASSERT_TRUE(reader.is_open());
    ASSERT_TRUE(reader.exists("docs/foo.txt"));
    auto file = reader.get_file("docs/foo.txt");
    ASSERT_TRUE(file != nullptr);
    auto buf = file->read();
    ASSERT_TRUE(buf != nullptr);
    std::string s(reinterpret_cast<const char*>(buf->data()), buf->size());
    EXPECT_EQ(s, "Hello 7z in-memory");
}

// 3) 仿照 Tar/Zip 的用例，针对 SevenZipDevice 进行压缩/解压验证（需 7z CLI）
TEST_F(TestSystemDevice, Test7zDevice)
{
    if (!has_7z_cli()) {
        GTEST_SKIP() << "7z/7za 不在 PATH 中，跳过 Test7zDevice";
    }
    const auto tmp_7z_file = TmpFile::create();
    GTEST_LOG_(INFO) << "Test7zDevice tmp path: " << tmp_7z_file->path() << "\n";

    {
        SevenZipDevice seven_zip(tmp_7z_file->path(), SevenZipDevice::Mode::WriteOnly);
        seven_zip.set_compression(CompressionMethod::LZMA2);
        BackupController controller{};
        controller.run_backup(device, seven_zip);
    }

    {
        SevenZipDevice reader(tmp_7z_file->path(), SevenZipDevice::Mode::ReadOnly);
        EXPECT_TRUE(reader.is_open());
        EXPECT_TRUE(reader.exists(test_folder / "test_file.txt"));
        EXPECT_TRUE(reader.exists(test_folder));
        auto file = reader.get_file(test_folder / "test_file.txt");
        ASSERT_NE(file, nullptr);
        auto content = file->read();
        ASSERT_NE(content, nullptr);
        std::string file_content(reinterpret_cast<const char*>(content->data()), content->size());
        EXPECT_EQ(file_content, test_file_content);
        auto folder = reader.get_folder(test_folder);
        EXPECT_NE(folder, nullptr);
    }
}

TEST_F(TestSystemDevice, Test7zEncDevice)
{
    if (!has_7z_cli()) {
        GTEST_SKIP() << "7z/7za 不在 PATH 中，跳过 Test7zEncDevice";
    }
    const auto tmp_7z_file = TmpFile::create();
    GTEST_LOG_(INFO) << "Test7zEncDevice tmp path: " << tmp_7z_file->path() << "\n";
    const std::vector<uint8_t> password = {'p','a','s','s','w','o','r','d'};

    {
        SevenZipDevice seven_zip(tmp_7z_file->path(), SevenZipDevice::Mode::WriteOnly);
        seven_zip.set_compression(CompressionMethod::LZMA2);
        seven_zip.set_encryption(EncryptionMethod::AES256);
        seven_zip.set_password(password);
        BackupController controller{};
        controller.run_backup(device, seven_zip);
    }

    {
        SevenZipDevice reader(tmp_7z_file->path(), SevenZipDevice::Mode::ReadOnly);
        reader.set_password(password);
        EXPECT_TRUE(reader.is_open());
        auto file = reader.get_file(test_folder / "test_file.txt");
        ASSERT_NE(file, nullptr);
        auto content = file->read();
        ASSERT_NE(content, nullptr);
        std::string file_content(reinterpret_cast<const char*>(content->data()), content->size());
        EXPECT_EQ(file_content, test_file_content);
    }

    {
        SevenZipDevice reader(tmp_7z_file->path(), SevenZipDevice::Mode::ReadOnly);
        const std::vector<uint8_t> bad_pwd = {'x','x','x'};
        reader.set_password(bad_pwd);
        EXPECT_TRUE(reader.is_open());
    }
}

TEST_F(TestSystemDevice, TestBackupFromPhysicalTo7z)
{
    if (!has_7z_cli()) {
        GTEST_SKIP() << "7z/7za 不在 PATH 中，跳过 TestBackupFromPhysicalTo7z";
    }
    const auto tmp_7z_file = TmpFile::create();
    GTEST_LOG_(INFO) << "TestBackupFromPhysicalTo7z tmp path: " << tmp_7z_file->path() << "\n";

    {
        SevenZipDevice seven_zip(tmp_7z_file->path(), SevenZipDevice::Mode::WriteOnly);
        BackupController controller{};
        controller.run_backup(device, seven_zip);
    }

    {
        SevenZipDevice verify(tmp_7z_file->path(), SevenZipDevice::Mode::ReadOnly);
        EXPECT_TRUE(verify.is_open());
        EXPECT_TRUE(verify.exists(test_folder));
        EXPECT_TRUE(verify.exists(test_folder / "test_file.txt"));
        auto file = verify.get_file(test_folder / "test_file.txt");
        ASSERT_NE(file, nullptr);
        auto content = file->read();
        ASSERT_NE(content, nullptr);
        std::string file_content(reinterpret_cast<const char*>(content->data()), content->size());
        EXPECT_EQ(file_content, test_file_content);
    }
}

// 4) 使用项目根目录 test_data 进行端到端备份与恢复（目标目录必须为空）
TEST(CoreSevenZipDevice, BackupAndRestoreWithTestData)
{
    if (!has_7z_cli()) {
        GTEST_SKIP() << "7z/7za 不在 PATH 中，跳过 BackupAndRestoreWithTestData";
    }

#ifdef PROJECT_ROOT_DIR
    const fs::path project_root = PROJECT_ROOT_DIR;
#else
    const fs::path project_root = fs::current_path();
#endif
    const fs::path source_dir = project_root / "test_data" / "source";
    ASSERT_TRUE(fs::exists(source_dir)) << "source_dir not found: " << source_dir.string();

    // 目标恢复目录：确保为空
    const fs::path restore_dir = fs::temp_directory_path() / "backup_suite_restore_test";
    if (fs::exists(restore_dir)) fs::remove_all(restore_dir);
    fs::create_directory(restore_dir);
    ASSERT_TRUE(fs::is_empty(restore_dir));

    // 临时 7z 文件
    const auto tmp_7z_file = TmpFile::create();
    GTEST_LOG_(INFO) << "BackupAndRestoreWithTestData 7z: " << tmp_7z_file->path() << "\n";

    // 备份：从物理源到 7z
    {
        SystemDevice src(source_dir);
        SevenZipDevice out7z(tmp_7z_file->path(), SevenZipDevice::Mode::WriteOnly);
        out7z.set_compression(CompressionMethod::LZMA2);
        BackupController ctl{};
        ctl.run_backup(src, out7z);
    }

    // 恢复：从 7z 到空目录
    {
        SevenZipDevice in7z(tmp_7z_file->path(), SevenZipDevice::Mode::ReadOnly);
        GTEST_LOG_(INFO) << "Restore: in7z open=" << (in7z.is_open() ? "true" : "false") << "\n";
        ASSERT_TRUE(in7z.is_open());
        auto root_folder = in7z.get_folder("");
        GTEST_LOG_(INFO) << "Restore: root_folder=" << (root_folder ? "ok" : "null") << "\n";
        ASSERT_TRUE(root_folder != nullptr);
        SystemDevice dst(restore_dir);
        BackupController ctl{};
        const bool ok = ctl.run_restore(in7z, dst);
        GTEST_LOG_(INFO) << "Restore: run_restore ok=" << (ok ? "true" : "false") << "\n";
        ASSERT_TRUE(ok);
    }

    // 验证：restore_dir 与 source_dir 的文件结构与内容一致
    auto compare_dirs = [](const fs::path& a, const fs::path& b) {
        // 比较 a 中的每个条目是否在 b 中存在且内容一致
        for (auto it = fs::recursive_directory_iterator(a); it != fs::recursive_directory_iterator(); ++it) {
            const auto rel = fs::relative(it->path(), a);
            const auto bp = b / rel;
            if (it->is_directory()) {
                if (!fs::exists(bp) || !fs::is_directory(bp)) return false;
            } else if (it->is_regular_file()) {
                if (!fs::exists(bp) || !fs::is_regular_file(bp)) return false;
                if (fs::file_size(it->path()) != fs::file_size(bp)) return false;
                // 简单内容比对：读取两侧并逐字节比较
                std::ifstream fa(it->path(), std::ios::binary);
                std::ifstream fb(bp, std::ios::binary);
                if (!fa.is_open() || !fb.is_open()) return false;
                constexpr size_t BUFSZ = 8192;
                std::vector<char> ba(BUFSZ), bb(BUFSZ);
                while (fa && fb) {
                    fa.read(ba.data(), BUFSZ); fb.read(bb.data(), BUFSZ);
                    const auto na = fa.gcount(); const auto nb = fb.gcount();
                    if (na != nb) return false;
                    if (std::memcmp(ba.data(), bb.data(), static_cast<size_t>(na)) != 0) return false;
                }
            }
        }
        return true;
    };

    const bool same = compare_dirs(source_dir, restore_dir);
    GTEST_LOG_(INFO) << "Compare result (source vs restore): " << (same ? "same" : "diff") << "\n";
    EXPECT_TRUE(same);
}
