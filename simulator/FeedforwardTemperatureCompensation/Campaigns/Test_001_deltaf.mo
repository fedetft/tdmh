within FeedforwardTemperatureCompensation.Campaigns;

model Test_001_deltaf
  extends BaseClasses.Campaign_base(
  csvName   = "Test-001-deltaf.csv",
  deltafmin = 0.035-0.008,
  deltafmax = 0.035+0.008
  );


annotation(
    experiment(StartTime = 0, StopTime = 6000, Tolerance = 1e-6, Interval = 1),
    __OpenModelica_commandLineOptions = "--matchingAlgorithm=PFPlusExt --indexReductionMethod=dynamicStateSelection -d=initialization,NLSanalyticJacobian",
    __OpenModelica_simulationFlags(lv = "LOG_STATS", s = "dassl"));
end Test_001_deltaf;