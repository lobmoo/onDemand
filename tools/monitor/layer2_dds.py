"""
OnDemand DDS Monitor - Layer 2: DDS 应用层监控

用 fastdds Python 绑定订阅 onDemand 的核心 topic，提取应用层指标。

注意: fastdds Python 包需要单独安装 (pip install fastdds)。
如果未安装，DDS 层会优雅降级 (跳过，只保留抓包层)。
"""

import logging
import threading
import time

try:
    from .config import MonitorConfig
    from .metrics import MetricsEngine
except ImportError:
    from config import MonitorConfig
    from metrics import MetricsEngine

logger = logging.getLogger(__name__)

# 尝试导入 fastdds, 失败则标记为不可用
try:
    import fastdds
    import fastdds_fastdds_module as fastdds_mod

    FASTDDS_AVAILABLE = True
except ImportError:
    FASTDDS_AVAILABLE = False
    logger.warning("fastdds Python package not installed. DDS layer disabled.")

# 尝试导入 roaringpy (用于反序列化 Roaring64Map)
try:
    import roaringpy

    ROARING_AVAILABLE = True
except ImportError:
    ROARING_AVAILABLE = False
    logger.info("roaringpy not installed. Mask parsing will use fallback (varData count).")


class DDSListener:
    """DDS DataReader 监听器基类"""

    def __init__(self, callback):
        self._callback = callback

    def on_data_available(self, reader):
        """有新数据到达"""
        try:
            info = fastdds.SampleInfo()
            # 尝试读取数据
            # 具体的数据类型由子类处理
            self._callback(reader, info)
        except Exception as e:
            logger.debug("DDS listener error: %s", e)


class DdsWorker:
    """DDS 应用层监控工作线程"""

    def __init__(self, config: MonitorConfig, metrics: MetricsEngine):
        self.config = config
        self.metrics = metrics
        self._running = threading.Event()
        self._thread: threading.Thread | None = None
        self._participant = None
        self._subscriber = None
        self._readers = []
        self._available = FASTDDS_AVAILABLE

    def start(self):
        """启动 DDS 监控线程"""
        if not self._available:
            logger.warning("DDS layer not available (fastdds not installed). Skipping.")
            return
        if self._running.is_set():
            return
        self._running.set()
        self._thread = threading.Thread(target=self._run, daemon=True, name="dds-worker")
        self._thread.start()
        logger.info("DdsWorker started: domain=%d", self.config.domain_id)

    def stop(self):
        """停止 DDS 监控"""
        self._running.clear()
        if self._thread:
            self._thread.join(timeout=5)
            self._thread = None
        self._cleanup_dds()
        logger.info("DdsWorker stopped")

    def _run(self):
        """DDS 监控主循环"""
        try:
            self._setup_dds()
            # 轮询读取数据
            while self._running.is_set():
                self._poll_readers()
                time.sleep(0.01)  # 10ms 轮询间隔
        except Exception as e:
            logger.error("DdsWorker error: %s", e)

    def _setup_dds(self):
        """初始化 DDS participant, subscriber, readers"""
        # 创建 DomainParticipant
        factory = fastdds.DomainParticipantFactory.get_instance()
        qos = fastdds.DomainParticipantQoS()
        qos.name("on_demand_monitor")
        # 忽略本地端点
        qos.properties().properties().push_back(
            {"fastdds.ignore_local_endpoints", "true"}
        )
        self._participant = factory.create_participant(self.config.domain_id, qos)
        if self._participant is None:
            raise RuntimeError("Failed to create DDS participant")

        # 创建 Subscriber
        sub_qos = fastdds.SubscriberQoS()
        self._subscriber = self._participant.create_subscriber(sub_qos)
        if self._subscriber is None:
            raise RuntimeError("Failed to create DDS subscriber")

        # 创建 DataReaders
        self._setup_readers()

    def _setup_readers(self):
        """创建各个 topic 的 DataReader"""
        # 注意: 实际使用时需要注册 type support
        # 这里简化处理，依赖 fastdds 的 type discovery
        # 如果 type support 未注册，reader 创建会失败，不影响其他层

        topics_and_types = [
            (self.config.topic_table_define, "DSF::Var::PubTableDefine"),
            (self.config.topic_sub_register, "DSF::Message::SubTableRegister"),
        ]

        # 添加 bucket_N topics
        for i in range(self.config.bucket_count):
            topic_name = f"{self.config.topic_data_prefix}{i}"
            topics_and_types.append((topic_name, "DSF::Var::TableDataTransfer"))

        for topic_name, type_name in topics_and_types:
            try:
                self._create_reader(topic_name, type_name)
            except Exception as e:
                logger.debug("Failed to create reader for %s: %s", topic_name, e)

    def _create_reader(self, topic_name: str, type_name: str):
        """创建单个 DataReader"""
        # 创建 Topic
        topic_qos = fastdds.TopicQoS()
        topic = self._participant.create_topic(topic_name, type_name, topic_qos)
        if topic is None:
            return

        # 创建 DataReader (RELIABLE, TRANSIENT_LOCAL)
        reader_qos = fastdds.DataReaderQoS()
        reader_qos.reliability().kind = fastdds.RELIABLE_RELIABILITY_QOS
        reader_qos.durability().kind = fastdds.TRANSIENT_LOCAL_DURABILITY_QOS
        reader_qos.history().kind = fastdds.KEEP_LAST_HISTORY_QOS
        reader_qos.history().depth = 1

        listener = MonitorDataReaderListener(
            topic_name=topic_name,
            metrics=self.metrics,
            config=self.config,
        )
        reader = self._subscriber.create_datareader(topic, reader_qos, listener)
        if reader is not None:
            self._readers.append(reader)
            logger.info("DDS reader created: %s", topic_name)

    def _poll_readers(self):
        """轮询所有 reader"""
        for reader in self._readers:
            try:
                # 检查是否有新数据
                if reader.take_next_sample(None, fastdds.SampleInfo())[0] == fastdds.RETCODE_OK:
                    pass  # 数据已在 listener 中处理
            except Exception:
                pass

    def _cleanup_dds(self):
        """清理 DDS 资源"""
        try:
            if self._participant:
                # 删除所有 entities
                self._participant.delete_contained_entities()
                factory = fastdds.DomainParticipantFactory.get_instance()
                factory.delete_participant(self._participant)
                self._participant = None
                self._subscriber = None
                self._readers.clear()
        except Exception as e:
            logger.debug("DDS cleanup error: %s", e)

    def is_alive(self) -> bool:
        if not self._available:
            return False
        return self._thread is not None and self._thread.is_alive()


class MonitorDataReaderListener:
    """DDS DataReader 监听器，处理收到的消息"""

    def __init__(self, topic_name: str, metrics: MetricsEngine, config: MonitorConfig):
        self.topic_name = topic_name
        self.metrics = metrics
        self.config = config

    def on_data_available(self, reader):
        """有新数据到达"""
        try:
            # 读取数据
            data = fastdds.TableDataTransfer()  # 占位，实际类型由 topic 决定
            info = fastdds.SampleInfo()
            ret = reader.take_next_sample(data, info)
            if ret == fastdds.RETCODE_OK and info.valid_data:
                self._process_data(data, info)
        except Exception as e:
            logger.debug("Data processing error for %s: %s", self.topic_name, e)

    def _process_data(self, data, info):
        """根据 topic 类型处理数据"""
        if self.topic_name == self.config.topic_table_define:
            self._process_table_define(data)
        elif self.topic_name == self.config.topic_sub_register:
            self._process_sub_register(data)
        elif self.topic_name.startswith(self.config.topic_data_prefix):
            self._process_data_transfer(data)

    def _process_table_define(self, data):
        """处理 PubTableDefine"""
        try:
            node_name = data.nodeName()
            table_name = data.name()
            var_count = len(data.varDefines())
            self.metrics.on_table_define(node_name, table_name, var_count)
        except Exception as e:
            logger.debug("TableDefine parse error: %s", e)

    def _process_sub_register(self, data):
        """处理 SubTableRegister"""
        try:
            node_name = data.nodeName()
            table_name = data.tableName()
            freqs = {}
            for var_freq in data.varFreqs():
                try:
                    freq_ms = int(var_freq.value())
                    freqs[var_freq.name()] = freq_ms
                except ValueError:
                    pass
            self.metrics.on_sub_register(node_name, table_name, freqs)
        except Exception as e:
            logger.debug("SubRegister parse error: %s", e)

    def _process_data_transfer(self, data):
        """处理 TableDataTransfer"""
        try:
            # 从 topic name 提取 bucket index
            bucket_str = self.topic_name.split("_")[-1]
            bucket_idx = int(bucket_str)

            # 解析 Roaring64Map 获取变量数
            mask_bytes = data.mask()
            var_count = len(data.varData())  # 简化: 用 varData 数量近似

            # 数据总大小
            data_size = sum(len(blob) for blob in data.varData())

            # 时间戳
            ts = data.timestamp()
            ts_sec = ts.tv_sec
            ts_nsec = ts.tv_nsec

            self.metrics.on_data_transfer(bucket_idx, var_count, data_size, ts_sec, ts_nsec)
        except Exception as e:
            logger.debug("DataTransfer parse error: %s", e)
