
#include "stats.h"
#include <algorithm>
#include <cmath>
#include <limits>

using namespace std;

Stats::Stats()
    : minValue(numeric_limits<long long>::max()),
      maxValue(numeric_limits<long long>::min()), mean(0), m2(0), n(0)
{
}

void Stats::add(long long data)
{
    if (isnan(data))
        return;

    minValue = min(minValue, data);
    maxValue = max(maxValue, data);

    // Stable algorithm for computing variance, see wikipedia
    n++;
    long long delta = data - mean;
    mean += delta / n;
    m2 += delta * (data - mean);
}

void Stats::resetStats()
{
    minValue = numeric_limits<long long>::max();
    maxValue = numeric_limits<long long>::min();
    mean     = 0;
    m2       = 0;
    n        = 0;
}

StatsResult Stats::getStats() const
{
    switch (n)
    {
        case 0:
            return {0, 0, 0, 0, n};
        case 1:
            return {minValue, maxValue, mean, 0, n};
        default:
            return {minValue, maxValue, mean, sqrtf(m2 / (n - 1)), n};
    }
}

