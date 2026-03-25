# 项目 CLAUDE.md - 老 C++17 遗留代码维护指南

## 项目类型
这是一个长期维护的 C++17 遗留代码项目（Legacy Codebase）。代码年代较久，包含大量老式 C 风格写法，需要逐步安全现代化，但**绝不能改变外部行为**。

## 核心维护原则（必须始终严格遵守）
1. **内存安全 - 最高优先级**
   - 优先使用 RAII：std::unique_ptr、std::shared_ptr、std::weak_ptr 替代 raw new/delete。
   - 逐步消灭裸指针（raw pointers）、手动 delete、手动内存管理。
   - 防止内存泄漏、use-after-free、double-free、dangling pointers。
   - 每次重构后建议运行 AddressSanitizer (ASan) 验证。

2. **性能优化 - 第二优先级**
   - 遵循零开销原则（zero-overhead）。
   - 优先使用 move semantics、const& 传参、reserve() 减少 realloc。
   - 关注数据局部性、避免 false sharing。
   - **永远先测量后优化**（建议使用 perf / Google Benchmark / VTune）。
   - 不要过早优化，保持代码可读性。

3. **线程安全 - 第三优先级**
   - 严格避免 data race：所有共享可变数据必须保护（std::mutex + std::lock_guard / std::unique_lock / std::scoped_lock）。
   - 统一加锁顺序，防止死锁。
   - 高性能场景可使用 std::atomic（注意 memory_order 和 ABA 问题）。
   - 建议每次改动后运行 ThreadSanitizer (TSan) 测试。
   - thread_local 优先于全局共享变量。

4. **C++17 限制**
   - 严格使用 C++17 标准，不引入 C++20/23 特性（concepts、ranges、std::expected 等除非明确允许）。
   - 允许使用的现代 C++17 特性：auto、range-based for、constexpr、std::optional、std::string_view、std::variant 等（在不破坏兼容性前提下）。

5. **遗留代码重构原则**
   - **增量式**：一次只改一小块（单个函数 → 类 → 文件）。
   - 先写 characterization test（行为保持测试），确保功能不变，再进行修改。
   - 不改变原有公开 API 和外部行为。
   - 新增代码必须添加清晰注释，说明“为什么这样改（安全/性能/线程）”。
   - 保留原有合理注释，逐步清理过时注释。

## 推荐工具与测试流程
- 编译：使用项目现有的构建系统（CMake / Makefile 等）。
- 静态分析：clang-tidy、cppcheck（重点检查内存与线程问题）。
- Sanitizer：-fsanitize=address,thread,undefined
- 测试框架：GoogleTest / Catch2（优先添加/维护测试）。
- 性能测试：Google Benchmark。

## 与 Skill 配合使用
- 默认始终加载 `cpp-legacy-maintain` Skill 的规则。
- 重构任务时优先调用 `/cpp-legacy-maintain`。
- 需要写测试时可叠加 TDD Skill。
- 代码审查时可叠加 code-review Skill。

## 其他项目特定规则
（这里留空，你后续可以根据自己项目补充，例如：
- 本项目不允许使用异常（exceptions）
- 必须使用特定线程池
- 命名规范：xxx
- 禁止修改的遗留模块：src/legacy/）

---

Claude 在处理本项目任何任务时，都必须先回顾以上原则，尤其是内存安全、性能和线程安全。
当用户说“重构”“优化”“修复”“维护”相关词时，自动应用以上规则。