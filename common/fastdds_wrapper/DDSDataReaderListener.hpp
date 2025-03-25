/**
 * @file DDSDataReaderListener.hpp
 * @brief 
 * @author wwk (1162431386@qq.com)
 * @version 1.0
 * @date 2025-03-25
 * 
 * @copyright Copyright (c) 2025  by  宝信
 * 
 * @par 修改日志:
 * <table>
 * <tr><th>Date       <th>Version <th>Author  <th>Description
 * <tr><td>2025-03-25     <td>1.0     <td>wwk   <td>修改?
 * </table>
 */
#ifndef DDS_DATAREADER_LISTENER_H
#define DDS_DATAREADER_LISTENER_H

#include <log/logger.h>

#include <fastdds/dds/core/detail/DDSReturnCode.hpp>
#include <fastdds/dds/subscriber/DataReader.hpp>
#include <fastdds/dds/subscriber/DataReaderListener.hpp>
#include <fastdds/dds/subscriber/Subscriber.hpp>
#include <fastdds/dds/topic/TopicDescription.hpp>

template <typename T>
class DDSDataReaderListener : public eprosima::fastdds::dds::DataReaderListener {
  typedef std::function<void(const std::string &, std::shared_ptr<T>)> OnMessageCallback;

 protected:
  virtual void on_subscription_matched
  (eprosima::fastdds::dds::DataReader *reader, const eprosima::fastdds::dds::SubscriptionMatchedStatus &info) override{
    const eprosima::fastdds::dds::TopicDescription *topic = reader->get_topicdescription();

    if (info.current_count_change == 1) {
      LOG(info) << "subscription " << topic->get_name() << " matched";
    } else if (info.current_count_change == -1) {
      LOG(info) << "subscription " << topic->get_name() << " unmatched";
    }
  }

  virtual void on_liveliness_changed
  (eprosima::fastdds::dds::DataReader *reader, const eprosima::fastdds::dds::LivelinessChangedStatus &status) override{
    LOG(info) << "on_liveliness_changed: " << status.alive_count;
  }

  void on_data_available(eprosima::fastdds::dds::DataReader *reader) {
    eprosima::fastdds::dds::SampleInfo info;
    std::shared_ptr<T> message = std::make_shared<T>();
    eprosima::fastdds::dds::ReturnCode_t retCode = reader->take_next_sample(message.get(), &info);
    if (retCode == eprosima::fastdds::dds::RETCODE_OK && info.valid_data) {
      m_callback(reader->get_topicdescription()->get_name(), message);
    } else {
      LOG(error) << "take_next_sample error";
    }
  }

 public:
  OnMessageCallback m_callback;
};

#endif