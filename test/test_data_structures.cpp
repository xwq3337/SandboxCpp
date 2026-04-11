#include <gtest/gtest.h>
#include "data_structures.h"


// 测试 TestCase 的 JSON 序列化/反序列化
TEST(DataStructuresTest, TestCaseSerialization) {
    TestCase tc{1, "1 2", "3"};
    json j = tc;

    EXPECT_EQ(j["case_id"], 1);
    EXPECT_EQ(j["stdin"], "1 2");
    EXPECT_EQ(j["expected"], "3");

    TestCase tc2 = j.get<TestCase>();
    EXPECT_EQ(tc2.case_id, tc.case_id);
    EXPECT_EQ(tc2.stdin, tc.stdin);
    EXPECT_EQ(tc2.expected, tc.expected);
}

// 测试 ResourcesLimits 的 JSON 序列化/反序列化
TEST(DataStructuresTest, ResourcesLimitsSerialization) {
    ResourcesLimits rl{1000, 268435456, 8388608, 1048576};
    json j = rl;

    EXPECT_EQ(j["cpu_time"], 1000);
    EXPECT_EQ(j["memory_bytes"], 268435456);
    EXPECT_EQ(j["stack_bytes"], 8388608);
    EXPECT_EQ(j["output_bytes"], 1048576);

    ResourcesLimits rl2 = j.get<ResourcesLimits>();
    EXPECT_EQ(rl2.cpu_time, rl.cpu_time);
    EXPECT_EQ(rl2.memory_bytes, rl.memory_bytes);
}

// 测试 Verdict 枚举序列化
TEST(DataStructuresTest, VerdictSerialization) {
    json j_accepted = Verdict::Accepted;
    EXPECT_EQ(j_accepted, "Accepted");

    json j_wrong = Verdict::WrongAnswer;
    EXPECT_EQ(j_wrong, "WrongAnswer");

    json j_tle = Verdict::TimeLimitExceeded;
    EXPECT_EQ(j_tle, "TimeLimitExceeded");

    json j_mle = Verdict::MemoryLimitExceeded;
    EXPECT_EQ(j_mle, "MemoryLimitExceeded");

    json j_re = Verdict::RuntimeError;
    EXPECT_EQ(j_re, "RuntimeError");

    json j_sce = Verdict::RestrictedSystemCall;
    EXPECT_EQ(j_sce, "RestrictedSystemCall");

    json j_ce = Verdict::CompilationError;
    EXPECT_EQ(j_ce, "CompilationError");

    json j_se = Verdict::SystemError;
    EXPECT_EQ(j_se, "SystemError");
}

// 测试 Verdict 反序列化
TEST(DataStructuresTest, VerdictDeserialization) {
    json j = "Accepted";
    auto v = j.get<Verdict>();
    EXPECT_EQ(v, Verdict::Accepted);

    j = "RuntimeError";
    v = j.get<Verdict>();
    EXPECT_EQ(v, Verdict::RuntimeError);
}

// 测试 InputStruct 的 JSON 解析
TEST(DataStructuresTest, InputStructParsing) {
    std::string json_str = R"({
        "submission_id": 12345,
        "language": "cpp",
        "code": "#include <iostream>\nint main() { return 0; }",
        "test_cases": [
            {"case_id": 1, "stdin": "1 2", "expected": "3"}
        ],
        "resources_limits": {
            "cpu_time": 1000,
            "memory_bytes": 268435456,
            "stack_bytes": 8388608,
            "output_bytes": 1048576
        },
        "message": "",
        "seccomp_profile": ""
    })";

    auto j = json::parse(json_str);
    InputStruct input = j.get<InputStruct>();

    EXPECT_EQ(input.submission_id, 12345);
    EXPECT_EQ(input.language, "cpp");
    EXPECT_EQ(input.test_cases.size(), 1u);
    EXPECT_EQ(input.test_cases[0].case_id, 1);
    EXPECT_EQ(input.resources_limits.cpu_time, 1000);
    EXPECT_EQ(input.resources_limits.memory_bytes, 268435456);
}

// 测试 OutputResult 的 JSON 序列化
TEST(DataStructuresTest, OutputResultSerialization) {
    OutputResult output;
    output.submission_id = 12345;
    output.language = "cpp";
    output.code = "int main() {}";
    output.verdict = Verdict::Accepted;
    output.max_time = 15;
    output.max_memory = 2048000;
    output.result = {
        {"1", "", "3", "", Verdict::Accepted, 15, 2048000, "3"}
    };

    json j = output;

    EXPECT_EQ(j["submission_id"], 12345);
    EXPECT_EQ(j["verdict"], "Accepted");
    EXPECT_EQ(j["max_time"], 15);
    EXPECT_EQ(j["result"].size(), 1u);
    EXPECT_EQ(j["result"][0]["status"], "Accepted");
}

// 测试 LanguageConfig 的 JSON 序列化/反序列化
TEST(DataStructuresTest, LanguageConfigSerialization) {
    LanguageConfig cfg;
    cfg.compile_cmd = "g++ -O2 -std=c++17 -o {output} {source}";
    cfg.run_cmd = "{output}";
    cfg.source_file = "solution.cpp";
    cfg.allow_sys_calls = {"read", "write", "close", "exit"};

    json j = cfg;

    EXPECT_EQ(j["compile_cmd"], "g++ -O2 -std=c++17 -o {output} {source}");
    EXPECT_EQ(j["run_cmd"], "{output}");
    EXPECT_EQ(j["allow_sys_calls"].size(), 4u);

    LanguageConfig cfg2 = j.get<LanguageConfig>();
    EXPECT_EQ(cfg2.compile_cmd, cfg.compile_cmd);
    EXPECT_EQ(cfg2.allow_sys_calls.size(), cfg.allow_sys_calls.size());
}
