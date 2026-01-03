#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

enum class Verdict {
    Accepted,
    WrongAnswer,
    TimeLimitExceeded,
    MemoryLimitExceeded,
    RuntimeError,
    RestrictedSystemCall,
    CompilationError,
    SystemError
};

// Verdict 序列化/反序列化
NLOHMANN_JSON_SERIALIZE_ENUM(Verdict, {
    {Verdict::Accepted, "Accepted"},
    {Verdict::WrongAnswer, "WrongAnswer"},
    {Verdict::TimeLimitExceeded, "TimeLimitExceeded"},
    {Verdict::MemoryLimitExceeded, "MemoryLimitExceeded"},
    {Verdict::RuntimeError, "RuntimeError"},
    {Verdict::RestrictedSystemCall, "RestrictedSystemCall"},
    {Verdict::CompilationError, "CompilationError"},
    {Verdict::SystemError, "SystemError"}
})

struct TestCase {
    int case_id;
    std::string stdin;
    std::string expected;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(TestCase, case_id, stdin, expected)
};

struct ResourcesLimits {
    int cpu_time;       // 毫秒
    int memory_bytes;   // 字节
    int stack_bytes;    // 字节
    int output_bytes;   // 字节

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(ResourcesLimits, cpu_time, memory_bytes, stack_bytes, output_bytes)
};

struct LanguageConfig {
    std::string compile_cmd;
    std::string run_cmd;
    std::vector<std::string> allow_sys_calls;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(LanguageConfig, compile_cmd, run_cmd, allow_sys_calls)
};

struct TestCaseResult {
    std::string case_id;
    std::string stdin;
    std::string stdout;
    std::string stderr;
    Verdict status;
    int time;           // 毫秒
    int memory;         // 字节
    std::string expected;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(TestCaseResult, case_id, stdin, stdout, stderr, status, time, memory, expected)
};

struct InputStruct {
    int64_t submission_id;
    std::string language;
    std::string code;
    std::vector<TestCase> test_cases;
    ResourcesLimits resources_limits;
    std::string message;
    std::string seccomp_profile;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(InputStruct, submission_id, language, code, test_cases, resources_limits, message, seccomp_profile)
};

struct OutputResult {
    int64_t submission_id;
    std::string language;
    std::string code;
    Verdict verdict;
    int max_time;
    int max_memory;
    std::vector<TestCaseResult> result;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(OutputResult, submission_id, language, code, verdict, max_time, max_memory, result)
};
