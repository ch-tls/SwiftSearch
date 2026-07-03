#include <gtest/gtest.h>

#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QTextStream>

#include "model/index_engine.h"
#include "model/index_database.h"
#include "controller/search_service.h"
#include "controller/query_parser.h"
#include "controller/result_ranker.h"
#include "model/file_entry.h"

class SearchServiceTest : public ::testing::Test {
 protected:
  void SetUp() override {
    temp_dir_ = std::make_unique<QTemporaryDir>();
    ASSERT_TRUE(temp_dir_->isValid());

    QString db_path = temp_dir_->path() + "/index.db";

    index_engine_ = std::make_unique<IndexEngine>();
    ASSERT_TRUE(index_engine_->Initialize(db_path));

    search_service_ = std::make_unique<SearchService>(index_engine_.get());

    PopulateTestFiles();
  }

  void PopulateTestFiles() {
    QDir dir(temp_dir_->path());

    QStringList names = {"main.cpp", "utils.cpp", "main.h", "README.md",
                         "CMakeLists.txt", "image.png", "doc.pdf"};
    for (int i = 0; i < names.size(); ++i) {
      QString file_path = dir.absoluteFilePath(names[i]);
      QFile file(file_path);
      if (file.open(QIODevice::WriteOnly)) {
        QTextStream stream(&file);
        stream << "test content " << i << "\n";
        file.close();
      }

      FileEntry entry;
      entry.path = file_path;
      entry.name = names[i];
      entry.size = file.size();
      entry.modified_time = QFileInfo(file_path).lastModified();
      entry.is_directory = false;

      index_engine_->GetDatabase()->InsertFile(entry);
    }
  }

  void TearDown() override {
    search_service_.reset();
    index_engine_.reset();
    temp_dir_.reset();
  }

  std::unique_ptr<QTemporaryDir> temp_dir_;
  std::unique_ptr<IndexEngine> index_engine_;
  std::unique_ptr<SearchService> search_service_;
};

TEST(QueryParserTest, ParseSimpleName) {
  SearchQuery query = QueryParser::Parse("main.cpp");
  EXPECT_EQ(query.name_pattern, "main.cpp");
  EXPECT_FALSE(query.use_regex);
}

TEST(QueryParserTest, ParseWithSizeGreater) {
  SearchQuery query = QueryParser::Parse("size>1M *.cpp");
  EXPECT_EQ(query.min_size, 1024 * 1024);
}

TEST(QueryParserTest, ParseWithSizeLess) {
  SearchQuery query = QueryParser::Parse("size<100 *.txt");
  EXPECT_EQ(query.max_size, 100);
}

TEST(ResultRankerTest, ExactMatchHighest) {
  FileEntry entry;
  entry.name = "main.cpp";
  entry.path = "/home/user/main.cpp";
  entry.size = 1024;

  double score = ResultRanker::Score(entry, "main.cpp", "");
  EXPECT_GT(score, 8.0);
}

TEST(ResultRankerTest, NonMatchZero) {
  FileEntry entry;
  entry.name = "main.cpp";
  entry.path = "/home/user/main.cpp";
  entry.size = 1024;

  double score = ResultRanker::Score(entry, "nonexistent", "");
  EXPECT_DOUBLE_EQ(score, 0.0);
}

TEST_F(SearchServiceTest, SearchByName) {
  SearchQuery query;
  query.name_pattern = "main";

  QList<SearchResult> results = search_service_->SearchSync(query);
  ASSERT_GE(results.size(), 2);

  bool found_cpp = false;
  bool found_h = false;
  for (const auto& r : results) {
    if (r.file_entry.name == "main.cpp") found_cpp = true;
    if (r.file_entry.name == "main.h") found_h = true;
  }
  EXPECT_TRUE(found_cpp);
  EXPECT_TRUE(found_h);
}

TEST_F(SearchServiceTest, SearchByPath) {
  SearchQuery query;
  query.path_pattern = temp_dir_->path();

  QList<SearchResult> results = search_service_->SearchSync(query);
  ASSERT_EQ(results.size(), 7);
}

TEST_F(SearchServiceTest, SearchBySize) {
  SearchQuery query;
  query.min_size = 0;
  query.max_size = INT64_MAX;

  QList<SearchResult> results = search_service_->SearchSync(query);
  ASSERT_EQ(results.size(), 7);
}

TEST_F(SearchServiceTest, SearchEmptyQueryReturnsAll) {
  SearchQuery query;
  QList<SearchResult> results = search_service_->SearchSync(query);
  ASSERT_EQ(results.size(), 7);
}
