#include "prometheus/family.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>

#include "prometheus/client_metric.h"
#include "prometheus/counter.h"
#include "prometheus/detail/future_std.h"
#include "prometheus/histogram.h"

namespace prometheus {
namespace {

TEST(FamilyTest, labels) {
  auto const_label = ClientMetric::Label{"component", "test"};
  auto dynamic_label = ClientMetric::Label{"status", "200"};

  Family<Counter> family{"total_requests",
                         "Counts all requests",
                         {{const_label.name, const_label.value}}};
  family.Add({{dynamic_label.name, dynamic_label.value}});
  auto collected = family.Collect();
  ASSERT_GE(collected.size(), 1U);
  ASSERT_GE(collected.at(0).metric.size(), 1U);
  EXPECT_THAT(collected.at(0).metric.at(0).label,
              ::testing::ElementsAre(const_label, dynamic_label));
}

TEST(FamilyTest, variable_labels) {
  auto const_label = ClientMetric::Label{"component", "test"};
  auto dynamic_label = ClientMetric::Label{"status", "200"};

  Family<Counter> family{"total_requests",
                         "Counts all requests",
                         {dynamic_label.name},
                         {{const_label.name, const_label.value}}};
  family.WithLabelValues({{dynamic_label.value}});
  auto collected = family.Collect();
  ASSERT_GE(collected.size(), 1U);
  ASSERT_GE(collected.at(0).metric.size(), 1U);
  EXPECT_THAT(collected.at(0).metric.at(0).label,
              ::testing::ElementsAre(const_label, dynamic_label));
}

TEST(FamilyTest, reject_same_label_keys) {
  auto labels = std::map<std::string, std::string>{{"component", "test"}};

  Family<Counter> family{"total_requests", "Counts all requests", labels};
  EXPECT_ANY_THROW(family.Add(labels));
}

TEST(FamilyTest, counter_value) {
  Family<Counter> family{"total_requests", "Counts all requests", {}};
  auto& counter = family.Add({});
  counter.Increment();
  auto collected = family.Collect();
  ASSERT_GE(collected.size(), 1U);
  ASSERT_GE(collected[0].metric.size(), 1U);
  EXPECT_EQ(1, collected[0].metric.at(0).counter.value);
}

TEST(FamilyTest, variable_counter_value) {
  Family<Counter> family{"total_requests", "Counts all requests", {}};
  auto& counter = family.WithLabelValues({});
  counter.Increment();
  auto collected = family.Collect();
  ASSERT_GE(collected.size(), 1U);
  ASSERT_GE(collected[0].metric.size(), 1U);
  EXPECT_EQ(1, collected[0].metric.at(0).counter.value);
}

TEST(FamilyTest, should_assert_error_variable_value_size) {
  Family<Counter> family{"total_requests", "Counts all requests", {}};
  auto create_family_with_invalid_name = [&]() {
    family.WithLabelValues({"haha"});
  };
  EXPECT_ANY_THROW(create_family_with_invalid_name());
}

TEST(FamilyTest, remove) {
  Family<Counter> family{"total_requests", "Counts all requests", {}};
  auto& counter1 = family.Add({{"name", "counter1"}});
  family.Add({{"name", "counter2"}});
  family.Remove(&counter1);
  auto collected = family.Collect();
  ASSERT_GE(collected.size(), 1U);
  EXPECT_EQ(collected[0].metric.size(), 1U);
}

TEST(FamilyTest, removeUnknownMetricMustNotCrash) {
  Family<Counter> family{"total_requests", "Counts all requests", {}};
  family.Remove(nullptr);
}

TEST(FamilyTest, Histogram) {
  Family<Histogram> family{"request_latency", "Latency Histogram", {}};
  auto& histogram1 = family.Add({{"name", "histogram1"}},
                                Histogram::BucketBoundaries{0, 1, 2});
  histogram1.Observe(0);
  auto collected = family.Collect();
  ASSERT_EQ(collected.size(), 1U);
  ASSERT_GE(collected[0].metric.size(), 1U);
  EXPECT_EQ(1U, collected[0].metric.at(0).histogram.sample_count);
}

TEST(FamilyTest, add_twice) {
  Family<Counter> family{"total_requests", "Counts all requests", {}};
  auto& counter = family.Add({{"name", "counter1"}});
  auto& counter1 = family.Add({{"name", "counter1"}});
  ASSERT_EQ(&counter, &counter1);
}

TEST(FamilyTest, throw_on_invalid_metric_name) {
  auto create_family_with_invalid_name = []() {
    return detail::make_unique<Family<Counter>>(
        "", "empty name", std::map<std::string, std::string>{});
  };
  EXPECT_ANY_THROW(create_family_with_invalid_name());
}

TEST(FamilyTest, throw_on_invalid_constant_label_name) {
  auto create_family_with_invalid_labels = []() {
    return detail::make_unique<Family<Counter>>(
        "total_requests", "Counts all requests",
        std::map<std::string, std::string>{{"__inavlid", "counter1"}});
  };
  EXPECT_ANY_THROW(create_family_with_invalid_labels());
}

TEST(FamilyTest, should_throw_on_invalid_labels) {
  Family<Counter> family{"total_requests", "Counts all requests", {}};
  auto add_metric_with_invalid_label_name = [&family]() {
    family.Add({{"__invalid", "counter1"}});
  };
  EXPECT_ANY_THROW(add_metric_with_invalid_label_name());
}

TEST(FamilyTest, should_not_collect_empty_metrics) {
  Family<Counter> family{"total_requests", "Counts all requests", {}};
  auto collected = family.Collect();
  EXPECT_TRUE(collected.empty());
}

}  // namespace
}  // namespace prometheus
