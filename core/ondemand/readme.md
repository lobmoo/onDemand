
TODO:
1.pub下面立马就启动，可能没等到sub清理数据，这种异常需要处理
2.支持@key字段，IDL 加 @key，QoS depth 从 20 改 1。
3.优化内存
4.init start stop 接口功能描述不清晰 是否需要uinit？

内存占用：
VarStore Arena（最大头）

每个变量的 slot 大小（variable_store.h:70-73）：


slot_bytes(size) = align64(64 + 2 * size)
变量大小	slot 占用	500k 变量
4B	128B	64MB
8B	128B	64MB
100B	320B	160MB
500B	1088B	544MB
光 arena 就能到 500MB+，如果你的变量平均 ~500B 的话。

VarMetadata 索引（第二大头）

每个 VarMetadata（on_demand_common.h:280-354）：

shared_ptr<DSF::Var::Define> = 16B + 堆上的 Define 对象
realVarName (std::string) = 32B + 堆上字符串内容
freqSubs (std::vector) = 24B + 堆分配
其他字段 ~30B
unordered_map 节点本身 ~40B overhead
粗估每个变量 ~200-300B，500k 变量 = 100-150MB

BucketManager

20 个桶，每个桶是 unordered_set<string>，存了 500k 个完整变量名字符串，每个 string ~50-80B，= 25-40MB

结论：600MB 基本合理，但有明显的浪费点

主要问题：

Slot 64B header 对小变量是灾难：4B 变量用 128B slot，32倍开销。pad_[48] 是为了让 header 恰好 64B，但这个设计对小变量极不友好。

双缓冲 2x 数据：每个变量存两份，对大变量影响显著。

VarMetadata 里的 shared_ptr<Define>：每个变量都持有一个 shared_ptr，500k 个 Define 对象全在堆上。

BucketManager 存完整字符串：只需要 hash 就够了，字符串是冗余的。