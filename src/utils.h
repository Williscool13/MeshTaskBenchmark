//
// Created by William on 2025-11-22.
//

#ifndef MESHTASKBENCHMARK_UTILS_H
#define MESHTASKBENCHMARK_UTILS_H
#include <array>
#include <vector>
#include <algorithm>


namespace Utils
{
class FrameTimeTracker
{
public:
    explicit FrameTimeTracker(size_t historySize = 100, float spikeThreshold = 1.5f);

    void RecordFrameTime(float frameTimeMs);

    float GetRollingAverage() const { return rollingAverage; }

    float GetLatestFrameTime() const;

    size_t GetSampleCount() const { return sampleCount; }

    void SetSpikeThreshold(float threshold) { spikeThreshold = threshold; }
    float GetSpikeThreshold() const { return spikeThreshold; }

    // Get percentile value (0.0 to 1.0, e.g., 0.5 for median, 0.95 for p95)
    float GetPercentile(float percentile) const;

    float GetMedian() const { return GetPercentile(0.5f); }
    float GetP95() const { return GetPercentile(0.95f); }
    float GetP99() const { return GetPercentile(0.99f); }

private:
    static constexpr size_t MAX_HISTORY_SIZE = 1000;

    std::array<float, MAX_HISTORY_SIZE> history{};
    size_t historySize;
    size_t currentIndex{0};
    size_t sampleCount{0};
    float rollingAverage{0.0f};
    float spikeThreshold;

    void UpdateRollingAverage();

    bool IsSpikeDetected(float frameTimeMs) const;
};
}


#endif //MESHTASKBENCHMARK_UTILS_H