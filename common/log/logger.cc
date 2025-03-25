/**
 * @file logger.cc
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
#include "logger.h"

#include "logger_impl.h"

Logger::Logger() : pImpl(std::make_unique<LoggerImpl>()) {}
Logger::~Logger() {}

Logger& Logger::Instance() {
  static Logger log;
  return log;
}

void Logger::Uinit()
{
   return pImpl->Uinit();
}

bool Logger::Init(const std::string& fileName, LoggerType type, severity_level level, int maxFileSize, int maxBackupIndex, bool isAsync) {
  return pImpl->Init(fileName, type, level, maxFileSize, maxBackupIndex, isAsync);
}

void Logger::Log(severity_level level, const std::string& msg, const char* file, int line, const char* func) {
  pImpl->log(level, msg, file, line, func);
}


void Logger::setFlushEvery(uint32_t flushEvery)
{
    pImpl->setFlushEvery(flushEvery);
}

void Logger::setFlushOnLevel(Logger::severity_level flushOnLevel)
{
    pImpl->setFlushOnLevel(flushOnLevel);
}