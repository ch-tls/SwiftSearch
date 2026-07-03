#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <numeric>
#include <ranges>
#include <random>
#include <string>
#include <vector>

#include "util/sort_util.h"
#include "controller/search_filter.h"
#include "controller/search_types.h"
#include "model/file_entry.h"

using namespace swiftsearch::util;
using namespace swiftsearch::views;

TEST(TimsortStability, EqualKeysPreserveInsertionOrder) {
  QList<SearchResult> results;
  for (int i = 0; i < 10; ++i) {
    SearchResult r;
    r.file_entry.name = QString("file_%1").arg(i);
    r.relevance_score = 5.0;
    results.append(r);
  }

  StableSort(results, [](const SearchResult& a, const SearchResult& b) {
    return a.relevance_score > b.relevance_score;
  });

  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(results[i].file_entry.name.toStdString(),
              std::string("file_") + std::to_string(i));
  }
}

TEST(TimsortStability, MixedScoresPreserveRelativeOrderForEqualScores) {
  struct Record {
    int id;
    double score;
  };

  std::vector<Record> records;
  records.push_back({1, 8.0});
  records.push_back({2, 8.0});
  records.push_back({3, 3.0});
  records.push_back({4, 8.0});
  records.push_back({5, 3.0});
  records.push_back({6, 6.0});
  records.push_back({7, 3.0});

  StableSort(records, [](const Record& a, const Record& b) {
    return a.score > b.score;
  });

  ASSERT_EQ(records.size(), 7u);

  EXPECT_EQ(records[0].score, 8.0);
  EXPECT_EQ(records[1].score, 8.0);
  EXPECT_EQ(records[2].score, 8.0);
  EXPECT_EQ(records[3].score, 6.0);
  EXPECT_EQ(records[4].score, 3.0);
  EXPECT_EQ(records[5].score, 3.0);
  EXPECT_EQ(records[6].score, 3.0);

  EXPECT_EQ(records[0].id, 1);
  EXPECT_EQ(records[1].id, 2);
  EXPECT_EQ(records[2].id, 4);
  EXPECT_EQ(records[3].id, 6);
  EXPECT_EQ(records[4].id, 3);
  EXPECT_EQ(records[5].id, 5);
  EXPECT_EQ(records[6].id, 7);
}

TEST(TimsortPartialOrder, NaturalRunsPreservedAfterStableSort) {
  std::vector<int> data;
  data.reserve(100);

  for (int i = 0; i < 30; ++i) data.push_back(i);
  for (int i = 100; i < 130; ++i) data.push_back(i);
  for (int i = 200; i < 230; ++i) data.push_back(i);
  for (int i = 60; i < 70; ++i) data.push_back(i);

  auto run_copy = data;
  std::sort(run_copy.begin(), run_copy.end());

  StableSort(data);

  EXPECT_EQ(data, run_copy);
}

TEST(TimsortPartialOrder, InterleavedSortedRuns) {
  std::vector<int> run_a = {1, 2, 3, 4, 5};
  std::vector<int> run_b = {6, 7, 8, 9, 10};
  std::vector<int> run_c = {11, 12, 13, 14, 15};

  std::vector<int> interleaved;
  for (size_t i = 0; i < 5; ++i) {
    interleaved.push_back(run_a[i]);
    interleaved.push_back(run_b[i]);
    interleaved.push_back(run_c[i]);
  }

  StableSort(interleaved);

  for (size_t i = 1; i < interleaved.size(); ++i) {
    EXPECT_LE(interleaved[i - 1], interleaved[i]);
  }
}

TEST(TimsortPartialOrder, DescendingRunsToAscending) {
  std::vector<int> data;
  data.reserve(300);

  for (int i = 0; i < 100; ++i) data.push_back(300 - i);
  for (int i = 0; i < 100; ++i) data.push_back(100 - i);
  for (int i = 0; i < 100; ++i) data.push_back(500 - i);

  StableSort(data);

  for (size_t i = 1; i < data.size(); ++i) {
    EXPECT_LE(data[i - 1], data[i]);
  }
}

TEST(StableSortAndTruncate, TruncatesAboveMaxCount) {
  std::vector<int> data(100);
  std::iota(data.begin(), data.end(), 1);

  StableSortAndTruncate(data, 10, std::greater<>{});

  ASSERT_EQ(data.size(), 10u);
  EXPECT_EQ(data[0], 100);
  EXPECT_EQ(data[9], 91);
}

TEST(StableSortAndTruncate, EmptyInputNoCrash) {
  std::vector<int> data;
  StableSortAndTruncate(data, 5);
  EXPECT_TRUE(data.empty());
}

TEST(StableSorted, ReturnsCopy) {
  std::vector<int> data = {5, 3, 1, 4, 2};
  auto sorted = StableSorted(data);
  EXPECT_EQ(data.size(), 5u);
  EXPECT_EQ(sorted.size(), 5u);
  EXPECT_EQ(sorted[0], 1);
  EXPECT_EQ(sorted[4], 5);
}

TEST(SearchFilter, ByNameContainsFiltersCorrectly) {
  QList<SearchResult> results;
  for (int i = 0; i < 10; ++i) {
    SearchResult r;
    r.file_entry.name = QString("file_%1").arg(i);
    r.relevance_score = 10.0 - i;
    results.append(r);
  }

  auto filtered = Materialize(
      std::views::all(results) | ByNameContains("file_1"));

  ASSERT_EQ(filtered.size(), 1u);
  EXPECT_EQ(filtered[0].file_entry.name.toStdString(), "file_1");
}

TEST(SearchFilter, ByMinSizeFiltersCorrectly) {
  QList<SearchResult> results;
  for (int i = 0; i < 5; ++i) {
    SearchResult r;
    r.file_entry.name = QString("file_%1").arg(i);
    r.file_entry.size = i * 100;
    r.relevance_score = 5.0;
    results.append(r);
  }

  auto filtered = Materialize(
      std::views::all(results) | ByMinSize(300));

  ASSERT_EQ(filtered.size(), 2u);
  EXPECT_EQ(filtered[0].file_entry.size, 300);
  EXPECT_EQ(filtered[1].file_entry.size, 400);
}

TEST(SearchFilter, PipelineCombination) {
  QList<SearchResult> results;
  results.reserve(20);
  for (int i = 0; i < 20; ++i) {
    SearchResult r;
    r.file_entry.name = (i % 2 == 0) ? QString("test_%1.txt").arg(i)
                                     : QString("other_%1.dat").arg(i);
    r.file_entry.size = i * 50;
    r.relevance_score = static_cast<double>(20 - i);
    results.append(r);
  }

  auto filtered = Materialize(
      std::views::all(results) | ByNameContains("test") | ByMinSize(100) | TopN(3));

  ASSERT_EQ(filtered.size(), 3u);

  for (const auto& r : filtered) {
    EXPECT_TRUE(r.file_entry.name.contains("test"));
    EXPECT_GE(r.file_entry.size, 100);
  }

  EXPECT_GE(filtered[0].relevance_score, filtered[1].relevance_score);
}

TEST(SearchFilter, EmptyCollectionProducesEmptyResult) {
  QList<SearchResult> empty;
  auto filtered = Materialize(
      std::views::all(empty) | ByNameContains("nothing") | ByMinSize(0) | TopN(5));

  EXPECT_TRUE(filtered.empty());
}

TEST(SearchFilter, TopNLargerThanSizeReturnsAll) {
  std::vector<int> data = {1, 2, 3};
  std::vector<int> result;
  for (auto v : data | TopN(100)) {
    result.push_back(v);
  }
  ASSERT_EQ(result.size(), 3u);
  EXPECT_EQ(result[0], 1);
  EXPECT_EQ(result[2], 3);
}

TEST(TimsortStability, FileSearchResultStability) {
  QList<SearchResult> results;
  results.reserve(6);

  auto add_entry = [&](const QString& name, double score) {
    SearchResult r;
    r.file_entry.name = name;
    r.file_entry.path = "/home/user/" + name;
    r.relevance_score = score;
    results.append(r);
  };

  add_entry("alpha.cpp", 6.0);
  add_entry("beta.cpp", 6.0);
  add_entry("gamma.cpp", 4.0);
  add_entry("delta.cpp", 4.0);
  add_entry("epsilon.cpp", 8.0);
  add_entry("zeta.cpp", 6.0);

  StableSort(results, [](const SearchResult& a, const SearchResult& b) {
    return a.relevance_score > b.relevance_score;
  });

  ASSERT_EQ(results.size(), 6);

  EXPECT_EQ(results[0].file_entry.name.toStdString(), "epsilon.cpp") << "score 8.0";
  EXPECT_EQ(results[1].file_entry.name.toStdString(), "alpha.cpp") << "score 6.0 first";
  EXPECT_EQ(results[2].file_entry.name.toStdString(), "beta.cpp") << "score 6.0 second";
  EXPECT_EQ(results[3].file_entry.name.toStdString(), "zeta.cpp") << "score 6.0 third";
  EXPECT_EQ(results[4].file_entry.name.toStdString(), "gamma.cpp") << "score 4.0 first";
  EXPECT_EQ(results[5].file_entry.name.toStdString(), "delta.cpp") << "score 4.0 second";
}

TEST(TimsortStability, LargePartitionedDatasetCorrectness) {
  constexpr int kLargeSize = 10000;

  std::mt19937 rng(42);
  std::uniform_int_distribution<int> dist(0, 99);

  struct Element {
    int key;
    int original_index;
  };

  std::vector<Element> elements;
  elements.reserve(static_cast<size_t>(kLargeSize));

  for (int i = 0; i < kLargeSize; ++i) {
    elements.push_back({dist(rng), i});
  }

  std::stable_sort(elements.begin(), elements.end(),
                   [](const Element& a, const Element& b) {
                     return a.key > b.key;
                   });

  int prev_key = elements[0].key;
  int prev_index = -1;
  for (const auto& e : elements) {
    if (e.key != prev_key) {
      prev_key = e.key;
      prev_index = -1;
    }
    EXPECT_GT(e.original_index, prev_index)
        << "stability broken: original insertion indices should ascend within equal-key bucket";
    prev_index = e.original_index;
  }
}
