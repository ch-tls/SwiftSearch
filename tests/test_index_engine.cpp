#include <gtest/gtest.h>

#include <QTemporaryDir>
#include <QFile>
#include <QDir>

#include "model/index_database.h"
#include "model/file_scanner.h"

TEST(IndexDatabaseTest, InitializeCreatesDatabase) {
  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());

  QString db_path = temp_dir.path() + "/test.db";
  IndexDatabase db(db_path);

  ASSERT_TRUE(db.Initialize());
  ASSERT_EQ(db.TotalCount(), 0);
  ASSERT_EQ(db.TotalSize(), 0);
}

TEST(IndexDatabaseTest, InsertAndQuery) {
  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());

  QString db_path = temp_dir.path() + "/test.db";
  IndexDatabase db(db_path);
  ASSERT_TRUE(db.Initialize());

  FileEntry entry;
  entry.path = "/home/user/test.cpp";
  entry.name = "test.cpp";
  entry.size = 1024;
  entry.modified_time = QDateTime::currentDateTime();
  entry.is_directory = false;

  ASSERT_TRUE(db.InsertFile(entry));
  ASSERT_EQ(db.TotalCount(), 1);

  QList<FileEntry> results = db.QueryByName("test");
  ASSERT_EQ(results.size(), 1);
  ASSERT_EQ(results[0].name, "test.cpp");
  ASSERT_EQ(results[0].size, 1024);
}

TEST(IndexDatabaseTest, BatchInsert) {
  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());

  QString db_path = temp_dir.path() + "/test.db";
  IndexDatabase db(db_path);
  ASSERT_TRUE(db.Initialize());

  QList<FileEntry> entries;
  for (int i = 0; i < 100; ++i) {
    FileEntry entry;
    entry.path = QString("/home/user/file_%1.txt").arg(i);
    entry.name = QString("file_%1.txt").arg(i);
    entry.size = i * 100;
    entry.modified_time = QDateTime::currentDateTime();
    entry.is_directory = false;
    entries.append(entry);
  }

  ASSERT_TRUE(db.InsertFiles(entries));
  ASSERT_EQ(db.TotalCount(), 100);
}

TEST(IndexDatabaseTest, ClearAll) {
  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());

  QString db_path = temp_dir.path() + "/test.db";
  IndexDatabase db(db_path);
  ASSERT_TRUE(db.Initialize());

  FileEntry entry;
  entry.path = "/home/user/test.cpp";
  entry.name = "test.cpp";
  entry.size = 1024;
  entry.modified_time = QDateTime::currentDateTime();
  entry.is_directory = false;

  db.InsertFile(entry);
  ASSERT_EQ(db.TotalCount(), 1);

  ASSERT_TRUE(db.ClearAll());
  ASSERT_EQ(db.TotalCount(), 0);
}

TEST(IndexDatabaseTest, QueryBySize) {
  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());

  QString db_path = temp_dir.path() + "/test.db";
  IndexDatabase db(db_path);
  ASSERT_TRUE(db.Initialize());

  FileEntry small_entry;
  small_entry.path = "/home/user/small.txt";
  small_entry.name = "small.txt";
  small_entry.size = 100;
  small_entry.modified_time = QDateTime::currentDateTime();
  small_entry.is_directory = false;
  db.InsertFile(small_entry);

  FileEntry large_entry;
  large_entry.path = "/home/user/large.txt";
  large_entry.name = "large.txt";
  large_entry.size = 10000;
  large_entry.modified_time = QDateTime::currentDateTime();
  large_entry.is_directory = false;
  db.InsertFile(large_entry);

  QList<FileEntry> results = db.QueryBySize(5000, 20000);
  ASSERT_EQ(results.size(), 1);
  ASSERT_EQ(results[0].name, "large.txt");
}
