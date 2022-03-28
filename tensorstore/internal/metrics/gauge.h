// Copyright 2022 The TensorStore Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef TENSORSTORE_INTERNAL_METRICS_GAUGE_H_
#define TENSORSTORE_INTERNAL_METRICS_GAUGE_H_

#include <atomic>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/debugging/leak_check.h"
#include "absl/memory/memory.h"
#include "tensorstore/internal/metrics/collect.h"
#include "tensorstore/internal/metrics/metadata.h"
#include "tensorstore/internal/metrics/metric_impl.h"
#include "tensorstore/internal/metrics/registry.h"
#include "tensorstore/internal/type_traits.h"
#include "tensorstore/util/str_cat.h"

namespace tensorstore {
namespace internal_metrics {

/// GaugeCell holds an individual gauge metric value.
template <typename T>
class GaugeCell;

/// A Gauge metric represents values that can increase and decrease.
///
/// Gauges are typically used for measured values like temperatures or current
/// memory usage.
///
/// Gauge is parameterized by the type, int64_t or double.
/// Each gauge has one or more Cells, which are described by Fields...,
/// which may be int, string, or bool.
///
/// Example:
///   namespace {
///   auto* temperature = Gauge<double>::New("/my/cpu/temperature");
///   }
///
///   temperature->Set(33.5);
///   temperature->IncrementBy(3.5);
///   temperature->IncrementBy(-3.5);
///
template <typename T, typename... Fields>
class Gauge {
  static_assert(std::is_same_v<T, int64_t> || std::is_same_v<T, double>);
  using Cell = std::conditional_t<std::is_same_v<T, int64_t>,
                                  GaugeCell<int64_t>, GaugeCell<double>>;
  using Impl = AbstractMetric<Cell, Fields...>;

 public:
  using value_type = T;

  static std::unique_ptr<Gauge> Allocate(
      std::string_view metric_name,
      typename internal::FirstType<std::string_view, Fields>... field_names,
      MetricMetadata metadata) {
    return absl::WrapUnique(new Gauge(std::string(metric_name),
                                      std::move(metadata),
                                      {std::string(field_names)...}));
  }

  static Gauge& New(
      std::string_view metric_name,
      typename internal::FirstType<std::string_view, Fields>... field_names,
      MetricMetadata metadata) {
    auto gauge = Allocate(metric_name, field_names..., metadata);
    GetMetricRegistry().Add(gauge.get());
    return *absl::IgnoreLeak(gauge.release());
  }

  const auto tag() const { return Cell::kTag; }
  const auto metric_name() const { return impl_.metric_name(); }
  const auto field_names() const { return impl_.field_names(); }
  const MetricMetadata metadata() const { return impl_.metadata(); }

  /// Increment the counter by 1.
  void Increment(typename FieldTraits<Fields>::param_type... labels) {
    impl_.GetCell(labels...)->IncrementBy(1);
  }

  /// Increment the counter by value .
  void IncrementBy(value_type value,
                   typename FieldTraits<Fields>::param_type... labels) {
    impl_.GetCell(labels...)->IncrementBy(value);
  }

  /// Decrement the counter by 1.
  void Decrement(typename FieldTraits<Fields>::param_type... labels) {
    impl_.GetCell(labels...)->DecrementBy(1);
  }
  /// Decrement the counter by value .
  void DecrementBy(value_type value,
                   typename FieldTraits<Fields>::param_type... labels) {
    impl_.GetCell(labels...)->DecrementBy(value);
  }

  /// Set the counter to the value.
  void Set(value_type value,
           typename FieldTraits<Fields>::param_type... labels) {
    impl_.GetCell(labels...)->Set(value);
  }

  value_type Get(typename FieldTraits<Fields>::param_type... labels) const {
    return impl_.GetCell(labels...)->Get();
  }

  /// Collect the gauge.
  CollectedMetric Collect() const {
    CollectedMetric result{};
    result.tag = Cell::kTag;
    result.metric_name = impl_.metric_name();
    result.metadata = impl_.metadata();
    result.field_names = impl_.field_names_vector();
    impl_.CollectCells([&result](const Cell& cell, const auto& fields) {
      result.gauges.emplace_back(std::apply(
          [&](const auto&... item) {
            std::vector<std::string> fields;
            fields.reserve(sizeof...(item));
            (fields.push_back(tensorstore::StrCat(item)), ...);
            return CollectedMetric::Metric{std::move(fields), cell.Get()};
          },
          fields));
    });
    return result;
  }

  /// Collect the individual Cells: on_cell is invoked for each entry.
  void CollectCells(typename Impl::CollectCellFn on_cell) const {
    return impl_.CollectCells(on_cell);
  }

 private:
  Gauge(std::string metric_name, MetricMetadata metadata,
        typename Impl::field_names_type field_names)
      : impl_(std::move(metric_name), std::move(metadata),
              std::move(field_names)) {}

  Impl impl_;
};

struct GaugeTag {
  static constexpr const char kTag[] = "gauge";
};

template <>
class GaugeCell<double> : public GaugeTag {
 public:
  using value_type = double;
  GaugeCell() = default;

  void IncrementBy(double value) {
    // C++ 20 will add std::atomic::fetch_add support for floating point types
    double v = value_.load();
    while (!value_.compare_exchange_weak(v, v + value)) {
      // repeat
    }
  }
  void DecrementBy(int64_t value) { IncrementBy(-value); }
  void Set(double value) { value_ = value; }

  double Get() const { return value_; }

 private:
  std::atomic<double> value_{0};
};

template <>
class GaugeCell<int64_t> : public GaugeTag {
 public:
  using value_type = int64_t;
  GaugeCell() = default;

  /// Increment the counter by value.
  void IncrementBy(int64_t value) { value_.fetch_add(value); }
  void DecrementBy(int64_t value) { IncrementBy(-value); }
  void Set(int64_t value) { value_ = value; }

  int64_t Get() const { return value_; }

 private:
  std::atomic<int64_t> value_{0};
};

}  // namespace internal_metrics
}  // namespace tensorstore

#endif  // TENSORSTORE_INTERNAL_METRICS_GAUGE_H_
