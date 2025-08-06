#include <fstream>
#include <memory>

#include <gtest/gtest.h>

#include <base/logging.hpp>
#include <conf/fileLoader.hpp>

// Test fixture for parameterized FileLoader tests
class FileLoaderParamTest : public ::testing::TestWithParam<std::tuple<std::string, std::string, conf::OptionMap>>
{
protected:
    std::filesystem::path internalPath;
    std::filesystem::path localPath;

    void SetUp() override
    {
        logging::testInit();
        internalPath = std::filesystem::temp_directory_path() / "internal_options.conf";
        localPath = std::filesystem::temp_directory_path() / "local_internal_options.conf";

        const auto& [internalContent, localContent, _] = GetParam();
        writeToFile(internalPath, internalContent);
        writeToFile(localPath, localContent);
    }

    void TearDown() override
    {
        std::filesystem::remove(internalPath);
        std::filesystem::remove(localPath);
    }

    void writeToFile(const std::filesystem::path& path, const std::string& content)
    {
        std::ofstream file(path);
        file << content;
    }
};

TEST_P(FileLoaderParamTest, LoadsConfigCorrectly)
{
    const auto& [_, __, expectedMap] = GetParam();

    // Inject temporary file paths
    conf::FileLoader loader {internalPath, localPath};
    auto result = loader.load();

    EXPECT_EQ(result, expectedMap);
}

// Utility function to create an OptionMap from initializer list
conf::OptionMap makeMap(std::initializer_list<std::pair<std::string, std::string>> list)
{
    return conf::OptionMap {list.begin(), list.end()};
}

INSTANTIATE_TEST_SUITE_P(
    FileLoaderTests,
    FileLoaderParamTest,
    ::testing::Values(
        // Case 1: valid internal file, empty local file
        std::make_tuple("engine.threads=4\nengine.mode=active\n",
                        "",
                        makeMap({{"engine.threads", "4"}, {"engine.mode", "active"}})),

        // Case 2: local file overrides internal value
        std::make_tuple("engine.threads=2\n", "engine.threads=8\n", makeMap({{"engine.threads", "8"}})),

        // Case 3: invalid line
        std::make_tuple("engine.threads=4\ninvalidline\n", "", makeMap({{"engine.threads", "4"}})),

        // Case 4: invalid key (not starting with "engine")
        std::make_tuple("manager.port=1514\nengine.enabled=true\n", "", makeMap({{"engine.enabled", "true"}})),

        // Case 5: both files are empty
        std::make_tuple("", "", makeMap({})),

        // Case 6: lines with comments and spaces
        std::make_tuple(
            R"(
                # This is a comment
                engine.threads =  4
                engine.mode=active
                # another comment
            )",
            "",
            makeMap({{"engine.threads", "4"}, {"engine.mode", "active"}})),

        // Case 7: repeated keys in local and internal files
        std::make_tuple("engine.threads=2\nengine.timeout=30\n",
                        "engine.threads=10\nengine.new=true\n",
                        makeMap({{"engine.threads", "10"}, {"engine.timeout", "30"}, {"engine.new", "true"}})),

        // Case 8: empty values
        std::make_tuple("engine.empty=\nengine.full=value\n",
                        "",
                        makeMap({{"engine.empty", ""}, {"engine.full", "value"}})),

        // Case 9: keys and values with spaces and tabs
        std::make_tuple("   engine.key1 =  value1  \nengine.key2\t=\tvalue2\n",
                        "",
                        makeMap({{"engine.key1", "value1"}, {"engine.key2", "value2"}})),

        // Case 10: numeric and boolean values as strings
        std::make_tuple("engine.int=42\nengine.bool=false\n",
                        "",
                        makeMap({{"engine.int", "42"}, {"engine.bool", "false"}}))));
