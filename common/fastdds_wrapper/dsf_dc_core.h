/**
 * @file dsf_dc_core.h
 * @date 2025-02-24
 * Copyright (c) 2025 by BAOSIGHT, All Rights Reserved.
 * @brief DFS_DC_ Core 接口定义
 */
  
  
 #ifndef DSF_DC_CORE_H
 #define DSF_DC_CORE_H
   
 #include <stdint.h>
 #include "dsf_dc_error_code.h"
   
 #ifdef __cplusplus
 extern "C"
 {
 #endif
   
     //-----------------------------------------------------------------
     // 类型安全句柄定义
     //-----------------------------------------------------------------
   
     /** 参与者句柄（不透明指针） */
     typedef struct dsf_dc_participant *dsf_dc_participant_t;
   
     /** 主题句柄 */
     typedef struct dsf_dc_topic *dsf_dc_topic_t;
   
     /** 发布者句柄 */
     typedef struct dsf_dc_publisher *dsf_dc_publisher_t;
   
     /** 订阅者句柄 */
     typedef struct dsf_dc_subscriber *dsf_dc_subscriber_t;
   
     /** 数据写入器句柄 */
     typedef struct dsf_dc_datawriter *dsf_dc_datawriter_t;
   
     /** 数据读取器句柄 */
     typedef struct dsf_dc_datareader *dsf_dc_datareader_t;
   
     /** 不透明句柄，表示 IDL 数据类型 */
     typedef void *dsf_dc_data_type_t;
   
     //-----------------------------------------------------------------
     // QoS 策略定义
     //-----------------------------------------------------------------
   
     /**
      * @brief 可靠性 QoS 策略
      */
     typedef enum {
         DFS_DC_QOS_RELIABILITY_BEST_EFFORT, ///< 尽力而为传输
         DFS_DC_QOS_RELIABILITY_RELIABLE     ///< 可靠传输
     } dsf_dc_qos_reliability_e;
   
     /**
      * @brief 持久性 QoS 策略
      */
     typedef enum {
        DFS_DC_QOS_DURABILITY_VOLATILE, ///< 数据不持久化
        DFS_DC_QOS_DURABILITY_TRANSIENT ///< 数据持久化
     } dsf_dc_qos_durability_e;
   
     /**
      * @brief QoS 配置结构体
      */
     typedef struct {
         dsf_dc_qos_reliability_t reliability; ///< 可靠性策略
         dsf_dc_qos_durability_t durability;   ///< 持久性策略
         int32_t history_depth;                  ///< 历史深度（默认 1）
     } dsf_dc_qos_t;
   
     //-----------------------------------------------------------------
     // 数据类型定义
     //-----------------------------------------------------------------
   
     /**
      * @brief 通用数据容器（示例）
      * @note 用户可根据实际需求扩展此结构体
      */
     typedef struct {
         uint32_t32_t id;           ///< 数据 ID
         uint32_t8_t *payload;      ///< 数据负载指针
         uint32_t32_t payload_size; ///< 数据负载大小（字节）
     } dsf_dc_data_t;
   
     //-----------------------------------------------------------------
     // 初始化与全局配置
     //-----------------------------------------------------------------
   
     /**
      * @brief 初始化 DFS_DC Core 库
      * @param config_file 配置参数（可为 NULL，使用默认配置）
      * @return 错误码
      */
     dsf_dc_error_t dsf_dc_init(const char *config_file);
   
     /**
      * @brief 清理 DFS_DC Core 库资源
      */
     void dsf_dc_fini();
   
     //-----------------------------------------------------------------
     // 参与者管理
     //-----------------------------------------------------------------
   
     /**
      * @brief 创建 DDS 参与者
      * @param domain_id 域 ID（0-232，符合 DDS 规范）
      * @return 参与者句柄，失败返回 NULL
      */
     dsf_dc_participant_t dsf_dc_create_participant(uint32_t32_t domain_id);
   
     /**
      * @brief 删除 DDS 参与者
      * @param participant 参与者句柄
      */
     void dsf_dc_destroy_participant(dsf_dc_participant_t participant);
   
     //-----------------------------------------------------------------
     // 主题管理
     //-----------------------------------------------------------------
   
     /**
      * @brief 创建主题
      * @param participant 参与者句柄
      * @param topic_name 主题名称（非空）
      * @param type_name 数据类型名称（需提前注册）
      * @return 主题句柄，失败返回 NULL
      */
     dsf_dc_topic_t dsf_dc_create_topic(dsf_dc_participant_t participant, const char *topic_name,
                                         const char *type_name);
   
     /**
      * @brief 删除主题
      * @param topic 主题句柄
      */
     void dsf_dc_destroy_topic(dsf_dc_topic_t topic);
   
     //-----------------------------------------------------------------
     // 发布者/订阅者管理
     //-----------------------------------------------------------------
   
     /**
      * @brief 创建发布者
      * @param participant 参与者句柄
      * @return 发布者句柄，失败返回 NULL
      */
     dsf_dc_publisher_t dsf_dc_create_publisher(dsf_dc_participant_t participant);
   
     /**
      * @brief 删除发布者
      * @param publisher 要删除的发布者句柄
      */
     void dsf_dc_destroy_publisher(dsf_dc_publisher_t publisher);
   
     /**
      * @brief 创建订阅者
      * @param participant 参与者句柄
      * @return 订阅者句柄，失败返回 NULL
      */
     dsf_dc_subscriber_t dsf_dc_create_subscriber(dsf_dc_participant_t participant);
   
     /**
      * @brief 删除订阅者
      * @param subscriber 要删除的订阅者句柄
      */
     void dsf_dc_destroy_subscriber(dsf_dc_subscriber_t subscriber);
   
     //-----------------------------------------------------------------
     // 数据写入器/读取器管理
     //-----------------------------------------------------------------
   
     /**
      * @brief 创建数据写入器
      * @param publisher 发布者句柄
      * @param topic 关联的主题句柄
      * @return 数据写入器句柄，失败返回 NULL
      */
     dsf_dc_datawriter_t dsf_dc_create_datawriter(dsf_dc_publisher_t publisher, dsf_dc_topic_t topic);
   
     /**
      * @brief 删除数据写入器
      * @param datawriter 要删除的数据写入器句柄
      */
     void dsf_dc_destroy_datawriter(dsf_dc_datawriter_t datawriter);
   
     /**
      * @brief 创建数据读取器
      * @param subscriber 订阅者句柄
      * @param topic 关联的主题句柄
      * @return 数据读取器句柄，失败返回 NULL
      */
     dsf_dc_datareader_t dsf_dc_create_datareader(dsf_dc_subscriber_t subscriber, dsf_dc_topic_t topic);
   
     /**
      * @brief 删除数据读取器
      * @param datareader 要删除的数据读取器句柄
      */
     void dsf_dc_destroy_datareader(dsf_dc_datareader_t datareader);
   
     //-----------------------------------------------------------------
     // 数据类型注册接口
     //-----------------------------------------------------------------
   
     /**
      * @brief 注册 IDL 生成的数据类型
      * @param type_name 类型名称（唯一标识）
      * @return 错误码
      */
     dsf_dc_error_t dsf_dc_register_type(const char *type_name);
   
     //-----------------------------------------------------------------
     // 数据操作接口
     //-----------------------------------------------------------------
   
     /**
      * @brief 发布数据（C++ 对象 -> 字节流）
      * @param datawriter 数据写入器句柄
      * @param type_name 类型名称（唯一标识）
      * @param data_ptr 指向 IDL 生成对象的指针（需在 C++ 层转换为具体类型）
      * @return 错误码
      */
     dfs_dc_error_t dfs_dc_publish(dsf_dc_datawriter_t datawriter, const char *type_name,
                                    const void *data_ptr);
   
     /**
      * @brief 请求回调函数类型
      * @param message_data 接收到的请求数据
      * @param user_data 用户自定义上下文
      */
     typedef void (*dsf_dc_message_listener_t)(const dsf_dc_data_t *message_data,
                                                  void *user_data);
   
     /**
      * @brief 订阅数据
      * @param datareader 数据读取器句柄
      * @param type_name 类型名称（唯一标识）
      * @param listener 回调函数
      * @param timeout_ms 超时时间
      * @param user_data 用户自定义上下文（可传递 NULL）
      * @return 错误码
      */
     dsf_dc_error_t dsf_dc_subscribe(dsf_dc_datareader_t datareader, const char *type_name,
                                      dsf_dc_message_listener_t listener, int32_t timeout_ms,
                                      void *user_data);
   
     /**
      * @brief 发送控制请求
      * @param datawriter 数据写入器句柄
      * @param type_name 类型名称（唯一标识）
      * @param request_data_ptr 指向 IDL 生成对象的指针（需在 C++ 层转换为具体类型）
      * @param [out] response_data 接收到的应答数据
      * @param timeout_ms 超时时间
      * @return 错误码
      */
     dsf_dc_error_t dsf_dc_control(dsf_dc_datawriter_t datawriter, const char *type_name,
                                    const void *request_data_ptr, dsf_dc_data_t *response_data,
                                    int32_t timeout_ms);
   
     //-----------------------------------------------------------------
     // QoS 控制
     //-----------------------------------------------------------------
   
     dsf_dc_error_t
     dsf_dc_set_qos(void *entity, // 支持 Participant/Publisher/Subscriber/DataWriter/DataReader
                       const dsf_dc_qos_t *qos);
   
     dsf_dc_error_t dsf_dc_get_qos(void *entity, dsf_dc_qos_t *qos);
   
 #ifdef __cplusplus
 }
 #endif
   
 #endif // DSF_DC_CORE_H