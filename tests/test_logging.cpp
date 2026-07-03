#include <gtest/gtest.h>

#include <QTemporaryDir>
#include <QFile>
#include <QTextStream>
#include <thread>
#include <vector>

#include "util/log_manager.h"

using namespace swiftsearch;

TEST(LogManagerTest, InstanceIsSingleton) {
  LogManager& a = LogManager::Instance();
  LogManager& b = LogManager::Instance();
  EXPECT_EQ(&a, &b);
}

TEST(LogManagerTest, LogAtAllLevels) {
  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());

  QString log_path = temp_dir.path() + "/test.log";

  LogManager::Instance().SetLogFile(log_path);
  LogManager::Instance().SetMinLevel(LogLevel::Debug);

  SWIFT_LOG_DEBUG("debug message");
  SWIFT_LOG_INFO("info message");
  SWIFT_LOG_WARNING("warning message");
  SWIFT_LOG_ERROR("error message");

  LogManager::Instance().Flush();

  QFile log_file(log_path);
  ASSERT_TRUE(log_file.open(QIODevice::ReadOnly | QIODevice::Text));

  QTextStream stream(&log_file);
  QString content = stream.readAll();

  EXPECT_TRUE(content.contains("DEBUG"));
  EXPECT_TRUE(content.contains("INFO"));
  EXPECT_TRUE(content.contains("WARNING"));
  EXPECT_TRUE(content.contains("ERROR"));
  EXPECT_TRUE(content.contains("debug message"));
  EXPECT_TRUE(content.contains("info message"));
  EXPECT_TRUE(content.contains("warning message"));
  EXPECT_TRUE(content.contains("error message"));

  log_file.close();
  LogManager::Instance().SetLogFile("");
}

TEST(LogManagerTest, LevelFilteringWorks) {
  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());

  QString log_path = temp_dir.path() + "/filtered.log";

  LogManager::Instance().SetLogFile(log_path);
  LogManager::Instance().SetMinLevel(LogLevel::Warning);

  SWIFT_LOG_DEBUG("should be filtered");
  SWIFT_LOG_INFO("should be filtered");
  SWIFT_LOG_WARNING("should appear");
  SWIFT_LOG_ERROR("should appear");

  LogManager::Instance().Flush();

  QFile log_file(log_path);
  ASSERT_TRUE(log_file.open(QIODevice::ReadOnly | QIODevice::Text));

  QTextStream stream(&log_file);
  QString content = stream.readAll();

  EXPECT_FALSE(content.contains("should be filtered"));
  EXPECT_TRUE(content.contains("should appear"));

  log_file.close();
  LogManager::Instance().SetLogFile("");
  LogManager::Instance().SetMinLevel(LogLevel::Debug);
}

TEST(LogManagerTest, FileLineInfoPresent) {
  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());

  QString log_path = temp_dir.path() + "/fileline.log";

  LogManager::Instance().SetLogFile(log_path);

  SWIFT_LOG_INFO("file line test");

  LogManager::Instance().Flush();

  QFile log_file(log_path);
  ASSERT_TRUE(log_file.open(QIODevice::ReadOnly | QIODevice::Text));

  QTextStream stream(&log_file);
  QString content = stream.readAll();

  EXPECT_TRUE(content.contains("test_logging.cpp"));

  log_file.close();
  LogManager::Instance().SetLogFile("");
}

TEST(LogManagerTest, TimestampPresent) {
  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());

  QString log_path = temp_dir.path() + "/timestamp.log";

  LogManager::Instance().SetLogFile(log_path);

  SWIFT_LOG_INFO("timestamp test");

  LogManager::Instance().Flush();

  QFile log_file(log_path);
  ASSERT_TRUE(log_file.open(QIODevice::ReadOnly | QIODevice::Text));

  QTextStream stream(&log_file);
  QString content = stream.readAll();

  EXPECT_TRUE(content.contains("2026"));

  log_file.close();
  LogManager::Instance().SetLogFile("");
}

TEST(LogManagerTest, ThreadSafetyConcurrentLogging) {
  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());

  QString log_path = temp_dir.path() + "/concurrent.log";
  LogManager::Instance().SetLogFile(log_path);

  std::vector<std::thread> threads;
  for (int i = 0; i < 10; ++i) {
    threads.emplace_back([i]() {
      for (int j = 0; j < 50; ++j) {
        SWIFT_LOG_DEBUG(QString("thread %1 msg %2").arg(i).arg(j));
      }
    });
  }

  for (auto& t : threads) {
    t.join();
  }

  LogManager::Instance().Flush();

  QFile log_file(log_path);
  ASSERT_TRUE(log_file.open(QIODevice::ReadOnly | QIODevice::Text));

  QTextStream stream(&log_file);
  QString content = stream.readAll();

  EXPECT_TRUE(content.contains("thread 0"));
  EXPECT_TRUE(content.contains("thread 9"));

  log_file.close();
  LogManager::Instance().SetLogFile("");
}
