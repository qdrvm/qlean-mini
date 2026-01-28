/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <chrono>
#include <map>
#include <string>

namespace lean::metrics {
  using Labels = std::map<std::string, std::string>;

  /**
   * @brief A counter metric to represent a monotonically increasing value.
   *
   * This class represents the metric type counter:
   * https://prometheus.io/docs/concepts/metric_types/#counter
   */
  class Counter {
   public:
    virtual ~Counter() = default;

    /**
     * @brief Increment the counter by 1.
     */
    virtual void inc() = 0;

    /**
     * The counter will not change if the given amount is negative.
     */
    virtual void inc(double val) = 0;
  };

  /**
   * @brief A gauge metric to represent a value that can arbitrarily go up and
   * down.
   *
   * The class represents the metric type gauge:
   * https://prometheus.io/docs/concepts/metric_types/#gauge
   */
  class Gauge {
   public:
    virtual ~Gauge() = default;

    /**
     * @brief Increment the gauge by 1.
     */
    virtual void inc() = 0;

    /**
     * @brief Increment the gauge by the given amount.
     */
    virtual void inc(double val) = 0;

    /**
     * @brief Decrement the gauge by 1.
     */
    virtual void dec() = 0;

    /**
     * @brief Decrement the gauge by the given amount.
     */
    virtual void dec(double val) = 0;

    /**
     * @brief Set the gauge to the given value.
     */
    virtual void set(double val) = 0;

    template <typename T>
    void set(T val) {
      set(static_cast<double>(val));
    }
  };

  /**
   * @brief A summary metric samples observations over a sliding window of
   * time.
   *
   * This class represents the metric type summary:
   * https://prometheus.io/docs/instrumenting/writing_clientlibs/#summary
   */
  class Summary {
   public:
    virtual ~Summary() = default;

    /**
     * @brief Observe the given amount.
     */
    virtual void observe(const double value) = 0;
  };

  class HistogramTimer;

  /**
   * @brief A histogram metric to represent aggregatable distributions of
   * events.
   *
   * This class represents the metric type histogram:
   * https://prometheus.io/docs/concepts/metric_types/#histogram
   */
  class Histogram {
   public:
    virtual ~Histogram() = default;

    /**
     * @brief Observe the given amount.
     */
    virtual void observe(const double value) = 0;

    /**
     * @brief Create a timer that automatically observes elapsed time.
     * @return HistogramTimer that records duration when it goes out of scope
     */
    HistogramTimer timer();
  };

  /**
   * @brief Metrics interface that holds all application metrics
   *
   * This interface provides access to application-wide metrics collection.
   */
  class Metrics {
   public:
    virtual ~Metrics() = default;

#define METRIC_GAUGE(field, name, help) virtual Gauge *field() const = 0;
#define METRIC_GAUGE_LABELS(field, name, help, ...) \
  virtual Gauge *field(const Labels &labels) = 0;
#define METRIC_COUNTER(field, name, help) virtual Counter *field() const = 0;
#define METRIC_COUNTER_LABELS(field, name, help, ...) \
  virtual Counter *field(const Labels &labels) = 0;
#define METRIC_HISTOGRAM(field, name, help, ...) virtual Histogram *field() const = 0;
#define METRIC_HISTOGRAM_LABELS(field, name, help, ...) \
  virtual Histogram *field(const Labels &labels) = 0;

#include "metrics/all_metrics.def"

#undef METRIC_GAUGE
#undef METRIC_GAUGE_LABELS
#undef METRIC_COUNTER
#undef METRIC_COUNTER_LABELS
#undef METRIC_HISTOGRAM
#undef METRIC_HISTOGRAM_LABELS
  };

  /**
   * @brief timer for histogram metrics
   *
   * Automatically records the elapsed time to a histogram when the timer
   * goes out of scope or when manually stopped.
   */
  class HistogramTimer {
   public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = std::chrono::time_point<Clock>;
    using Duration = std::chrono::duration<double>;

    /**
     * @brief Construct and start a timer for the given histogram
     * @param histogram Histogram to record elapsed time to
     */
    explicit HistogramTimer(Histogram *histogram)
        : histogram_(histogram), start_time_(Clock::now()), running_(true) {}

    /**
     * @brief Destructor - automatically stops timer and records elapsed time
     */
    ~HistogramTimer() {
      if (running_) {
        stop();
      }
    }

    // Disable copying
    HistogramTimer(const HistogramTimer &) = delete;
    HistogramTimer &operator=(const HistogramTimer &) = delete;

    // Enable moving
    HistogramTimer(HistogramTimer &&other) noexcept
        : histogram_(other.histogram_),
          start_time_(other.start_time_),
          running_(other.running_) {
      other.running_ = false;
    }

    HistogramTimer &operator=(HistogramTimer &&other) noexcept {
      if (this != &other) {
        if (running_) {
          stop();
        }
        histogram_ = other.histogram_;
        start_time_ = other.start_time_;
        running_ = other.running_;
        other.running_ = false;
      }
      return *this;
    }

    /**
     * @brief Stop the timer and record the elapsed time to the histogram
     * @return Elapsed time in seconds
     *
     */
    double stop() {
      if (!running_) {
        return 0.0;
      }

      const auto elapsed_time = elapsed();
      running_ = false;

      // Automatically observe the elapsed time
      if (histogram_) {
        histogram_->observe(elapsed_time);
      }

      return elapsed_time;
    }

    /**
     * @brief Stop the timer without recording to the histogram
     * @return Elapsed time in seconds
     *
     */
    double stopWithoutObserving() {
      if (!running_) {
        return 0.0;
      }

      const auto elapsed_time = elapsed();
      running_ = false;
      return elapsed_time;
    }

    /**
     * @brief Check if the timer is currently running
     * @return true if running, false if stopped
     */
    bool isRunning() const {
      return running_;
    }

    /**
     * @brief Get elapsed time without stopping the timer
     * @return Elapsed time in seconds
     */
    double elapsed() const {
      const auto now = Clock::now();
      const Duration duration = now - start_time_;
      return duration.count();
    }

   private:
    Histogram *histogram_;
    TimePoint start_time_;
    bool running_;
  };

  [[nodiscard]] inline HistogramTimer Histogram::timer() {
    return HistogramTimer(this);
  }

}  // namespace lean::metrics
