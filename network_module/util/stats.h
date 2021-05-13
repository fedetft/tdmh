
#pragma once

/**
 * Statisitics computed by the Stats class
 */
struct StatsResult
{
    long long min;   // Min value found so far
    long long max;   // Max value found so far
    long long mean;  // Mean of datased
    long long stdev; // Standard deviation of datset
    unsigned int n;  // Number of samples
};

class Stats
{
public:
    /**
     * Constructor
     */
    Stats();

    /**
     * Add an element
     */
    void add(long long data);

    /**
     * Reset all the Stats
     */
    void resetStats();

    /**
     * Return statistics of the elements added so far
     */
    StatsResult getStats() const;

private:
    long long minValue, maxValue, mean, m2;
    unsigned int n;
};
