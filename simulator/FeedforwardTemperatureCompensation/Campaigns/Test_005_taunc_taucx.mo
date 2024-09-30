within FeedforwardTemperatureCompensation.Campaigns;

model Test_005_taunc_taucx
  extends BaseClasses.Campaign_base(csvName = "Test-005-taunc-taucx.csv", tauncmin = 100 * 0.75, tauncmax = 100 * 1.25, taucxmin = 5 * 0.75, taucxmax = 5 * 1.125);
  annotation(
    experiment(StartTime = 0, StopTime = 6000, Tolerance = 1e-06, Interval = 1),
    __OpenModelica_commandLineOptions = "--matchingAlgorithm=PFPlusExt --indexReductionMethod=dynamicStateSelection -d=initialization,NLSanalyticJacobian",
    __OpenModelica_simulationFlags(lv = "LOG_STATS", s = "dassl"));
end Test_005_taunc_taucx;