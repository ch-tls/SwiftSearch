#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "model/indexing_observer.h"
#include "controller/search_observer.h"

using namespace swiftsearch;

class MockIndexingObserver : public IndexingObserver {
 public:
  int progress_calls = 0;
  int finished_calls = 0;
  int error_calls = 0;
  int files_indexed_last = 0;
  int64_t total_indexed_last = 0;
  int64_t total_files_last = 0;
  std::string error_message;

  void OnIndexingProgress(int files_indexed, int64_t total_indexed) override {
    ++progress_calls;
    files_indexed_last = files_indexed;
    total_indexed_last = total_indexed;
  }

  void OnIndexingFinished(int64_t total_files) override {
    ++finished_calls;
    total_files_last = total_files;
  }

  void OnIndexingError(const std::string& error) override {
    ++error_calls;
    error_message = error;
  }
};

class MockSearchObserver : public SearchObserver {
 public:
  int results_calls = 0;
  int error_calls = 0;
  int results_count = 0;
  std::string error_message;

  void OnSearchResultsReady(const std::vector<::SearchResult>& results,
                            const ::SearchQuery& /*query*/) override {
    ++results_calls;
    results_count = static_cast<int>(results.size());
  }

  void OnSearchError(const std::string& error) override {
    ++error_calls;
    error_message = error;
  }
};

TEST(IndexingObserverTest, AddAndNotifyProgress) {
  auto observer = std::make_shared<MockIndexingObserver>();
  auto observer2 = std::make_shared<MockIndexingObserver>();

  observer->OnIndexingProgress(10, 100);
  EXPECT_EQ(observer->progress_calls, 1);
  EXPECT_EQ(observer->files_indexed_last, 10);
  EXPECT_EQ(observer->total_indexed_last, 100);

  observer2->OnIndexingProgress(5, 55);
  EXPECT_EQ(observer2->total_indexed_last, 55);
}

TEST(IndexingObserverTest, NotifyFinished) {
  auto observer = std::make_shared<MockIndexingObserver>();

  observer->OnIndexingFinished(5000);
  EXPECT_EQ(observer->finished_calls, 1);
  EXPECT_EQ(observer->total_files_last, 5000);
}

TEST(IndexingObserverTest, NotifyError) {
  auto observer = std::make_shared<MockIndexingObserver>();

  observer->OnIndexingError("test error");
  EXPECT_EQ(observer->error_calls, 1);
  EXPECT_EQ(observer->error_message, "test error");
}

TEST(IndexingObserverTest, WeakPtrExpiredDoesNotCrash) {
  auto observer = std::make_shared<MockIndexingObserver>();
  std::weak_ptr<IndexingObserver> weak = observer;
  {
    auto locked = weak.lock();
    ASSERT_TRUE(locked);
    locked->OnIndexingProgress(1, 42);
  }

  observer.reset();
  auto locked = weak.lock();
  EXPECT_FALSE(locked);
}

TEST(SearchObserverTest, ResultsNotification) {
  auto observer = std::make_shared<MockSearchObserver>();
  std::vector<::SearchResult> results(3);

  observer->OnSearchResultsReady(results, ::SearchQuery{});
  EXPECT_EQ(observer->results_calls, 1);
  EXPECT_EQ(observer->results_count, 3);
}

TEST(SearchObserverTest, ErrorNotification) {
  auto observer = std::make_shared<MockSearchObserver>();

  observer->OnSearchError("search failed");
  EXPECT_EQ(observer->error_calls, 1);
  EXPECT_EQ(observer->error_message, "search failed");
}

TEST(ObserverThreadSafety, ConcurrentNotifyDoesNotCrash) {
  auto observer = std::make_shared<MockIndexingObserver>();

  std::vector<std::thread> threads;
  for (int i = 0; i < 10; ++i) {
    threads.emplace_back([observer, i]() {
      for (int j = 0; j < 100; ++j) {
        observer->OnIndexingProgress(j, static_cast<int64_t>(i * 100 + j));
      }
    });
  }

  for (auto& t : threads) {
    t.join();
  }

  EXPECT_GT(observer->progress_calls, 0);
}
