#include <gtest/gtest.h>
#include <data_structures.h>
#include <code_executor.h>
class VerdictTest : public ::testing::Test
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

TEST_F(VerdictTest, TestWrongAnswer)
{
    // 测试 a + b - 期望错误答案导致 WrongAnswer
    InputStruct input{
        0,
        "cpp",
        "#include <iostream>\nusing namespace std;\n\nint main() {    \nint a, b;\n    cin >> a >> b;\n    cout << a + b <<endl;\n    return 0;\n}",
        {{1, "1 2", "0"}, {2, "3 4", "0"}},
        {1000, 268435456, 8388608, 1048576}, // 1秒CPU时间(!=物理时间)，256MB内存，8MB栈，1MB输出
        "",
        ""};
    OutputResult output = executor_.execute(input);
    EXPECT_EQ(output.verdict, Verdict::WrongAnswer);
}

TEST_F(VerdictTest, TestAccepted)
{
    // 测试 a + b - 正确答案
    InputStruct input{
        0,
        "cpp",
        "#include <iostream>\nusing namespace std;\n\nint main() {\n    int a, b;\n    cin >> a >> b;\n    cout << a + b << endl;\n    return 0;\n}",
        {{1, "1 2", "3"}, {2, "3 4", "7"}},
        {1000, 268435456, 8388608, 1048576},
        "",
        ""};
    OutputResult output = executor_.execute(input);
    EXPECT_EQ(output.verdict, Verdict::Accepted);
    EXPECT_EQ(output.result.size(), 2u);
    EXPECT_EQ(output.result[0].status, Verdict::Accepted);
    EXPECT_EQ(output.result[1].status, Verdict::Accepted);
}

TEST_F(VerdictTest, TestCompilationError)
{
    // 测试语法错误代码导致 CompilationError
    InputStruct input{
        0,
        "cpp",
        "#include <iostream>\nusing namespace std;\n\nint main() {\n    int a b;  // 缺少逗号\n    cout << a << endl;\n",
        {{1, "1 2", "3"}},
        {1000, 268435456, 8388608, 1048576},
        "",
        ""};
    OutputResult output = executor_.execute(input);
    EXPECT_EQ(output.verdict, Verdict::CompilationError);
}

TEST_F(VerdictTest, TestRuntimeError)
{
    // 测试返回非零退出码导致 RuntimeError
    InputStruct input{
        0,
        "cpp",
        "#include <iostream>\nusing namespace std;\n\nint main() {\n    return 1;\n}",
        {{1, "", ""}},
        {1000, 268435456, 8388608, 1048576},
        "",
        ""};
    OutputResult output = executor_.execute(input);
    EXPECT_EQ(output.verdict, Verdict::RuntimeError);
}

TEST_F(VerdictTest, TestTimeLimitExceeded)
{
    // 测试死循环导致 TimeLimitExceeded
    InputStruct input{
        0,
        "cpp",
        "#include <iostream>\nusing namespace std;\n\nint main() {\n    while (1) {}\n    return 0;\n}",
        {{1, "", ""}},
        {50, 268435456, 8388608, 1048576}, // 50ms CPU时间限制
        "",
        ""};
    OutputResult output = executor_.execute(input);
    EXPECT_EQ(output.verdict, Verdict::TimeLimitExceeded);
}

TEST_F(VerdictTest, TestMultipleTestCasesPartialFailure)
{
    // 测试多个测试用例，第一个通过，第二个失败
    // 整体 verdict 应反映失败的测试用例
    InputStruct input{
        0,
        "cpp",
        "#include <iostream>\nusing namespace std;\n\nint main() {\n    int a, b;\n    cin >> a >> b;\n    cout << a + b << endl;\n    return 0;\n}",
        {{1, "1 2", "3"}, {2, "5 6", "0"}}, // 第二个期望错误
        {1000, 268435456, 8388608, 1048576},
        "",
        ""};
    OutputResult output = executor_.execute(input);
    // 整体 verdict 应该是 WrongAnswer(因为有一个用例失败)
    EXPECT_EQ(output.verdict, Verdict::WrongAnswer);
    EXPECT_EQ(output.result.size(), 2u);
    EXPECT_EQ(output.result[0].status, Verdict::Accepted);
    EXPECT_EQ(output.result[1].status, Verdict::WrongAnswer);
}

TEST_F(VerdictTest, TestMemoryLimitExceeded)
{
    // 测试内存超限导致 MemoryLimitExceeded
    // 分配 100MB vector 并初始化所有元素，cgroup OOM killer 会 SIGKILL 进程
    InputStruct input{
        0,
        "cpp",
        "#include <iostream>\n#include <vector>\nusing namespace std;\n\nint main() {\n    volatile int n = 25 * 1024 * 1024; // 避免编译器优化\n    vector<int> v(n, 42); // 25M * 4bytes = 100MB\n    cout << v[0] << endl;\n    return 0;\n}",
        {{1, "", ""}},
        {1000, 20971520, 8388608, 1048576}, // 20MB 内存限制，小于分配的 100MB
        "",
        ""};
    OutputResult output = executor_.execute(input);
    EXPECT_EQ(output.verdict, Verdict::MemoryLimitExceeded);
}

TEST_F(VerdictTest, TestRestrictedSystemCall)
{
    // 测试调用不允许的系统调用导致 RestrictedSystemCall
    // socket 不在自定义的受限白名单中
    // seccomp_profile 指定白名单（不包含 socket），触发 SIGSYS
    InputStruct input{
        0,
        "cpp",
        "#include <iostream>\n#include <unistd.h>\n#include <sys/syscall.h>\n#include <sys/socket.h>\nusing namespace std;\n\nint main() {\n    syscall(SYS_socket, AF_INET, SOCK_STREAM, 0);\n    cout << \"ok\" << endl;\n    return 0;\n}",
        {{1, "", ""}},
        {1000, 268435456, 8388608, 1048576},
        "",
        R"(["read","write","close","fstat","lseek","mmap","mprotect","munmap","brk","rt_sigaction","rt_sigprocmask","rt_sigreturn","access","exit","exit_group","open","openat","stat","lstat","mremap","madvise","getpid","getuid","getgid","geteuid","getegid","clock_gettime","gettimeofday","arch_prctl","set_tid_address","set_robust_list","futex","sched_getaffinity","prlimit64","getrandom","uname","execve","readv","writev","dup","dup2","pipe","pipe2","getrlimit","setrlimit","getrusage","newfstatat"])"};
    OutputResult output = executor_.execute(input);
    EXPECT_EQ(output.verdict, Verdict::RestrictedSystemCall);
}