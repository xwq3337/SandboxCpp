#include <gtest/gtest.h>
#include <code_executor.h>
#include <data_structures.h>

class LanguagesTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        std::string configPath = std::filesystem::path(__FILE__)
                                     .parent_path()
                                     .parent_path() /
                                 "config" / "languages.json";
        executor_.loadLanguageConfigs(configPath);
    }

    CodeExecutor executor_;
};

TEST_F(LanguagesTest, TestCpp)
{
    // 测试 a + b
    InputStruct input{
        0,
        "cpp",
        "#include <iostream>\nusing namespace std;\n\nint main() {    \nint a, b;\n    cin >> a >> b;\n    cout << a + b <<endl;\n    return 0;\n}",
        {{1, "1 2", "3"}, {2, "3 4", "7"}},
        {1000, 268435456, 8388608, 1048576},
        "",
        ""};
    OutputResult output = executor_.execute(input);
    EXPECT_EQ(output.verdict, Verdict::Accepted);
}

TEST_F(LanguagesTest, TestPython)
{
    // 测试 a + b
    InputStruct input{
        0,
        "python",
        "s = input().split()\nprint(int(s[0]) + int(s[1]))",
        {{1, "1 2", "3"}, {2, "3 4", "7"}},
        {1000, 268435456, 8388608, 1048576},
        "",
        ""};
    OutputResult output = executor_.execute(input);

    EXPECT_EQ(output.verdict, Verdict::Accepted);
}

TEST_F(LanguagesTest, TestGo)
{
    // 测试 a + b
    InputStruct input{
        0,
        "go",
        "package main\n\nimport \"fmt\"\n\nfunc main() {\n    var a, b int\n    fmt.Scanf(\"%d%d\", &a, &b)\n    fmt.Println(a+b)\n}",
        {{1, "1 2", "3"}, {2, "3 4", "7"}},
        {1000, 268435456, 8388608, 1048576},
        "",
        ""};
    OutputResult output = executor_.execute(input);
    std::cout << json(output).dump(2) << std::endl;
    EXPECT_EQ(output.verdict, Verdict::Accepted);
}

TEST_F(LanguagesTest, TestRust)
{
    // 测试 a + b
    InputStruct input{
        0,
        "rust",
        "use std::io;\n\nfn main(){\n    let mut input=String::new();\n    io::stdin().read_line(&mut input).unwrap();\n    let mut s=input.trim().split(' ');\n\n    let a:i32=s.next().unwrap()\n               .parse().unwrap();\n    let b:i32=s.next().unwrap()\n               .parse().unwrap();\n    println!(\"{}\",a+b);\n}",
        {{1, "1 2", "3"}, {2, "3 4", "7"}},
        {1000, 268435456, 8388608, 1048576},
        "",
        ""};
    OutputResult output = executor_.execute(input);
    std::cout << json(output).dump(2) << std::endl;
    EXPECT_EQ(output.verdict, Verdict::Accepted);
}