within FeedforwardTemperatureCompensation.Campaigns;

model Test_007_revised_paper_all_params
  extends BaseClasses.Campaign_base(
      logTraces  = false,
      logTermErr = true,
      n          = 50,
      csvName    = "Test-007-all.csv",
      errfName   = "Test-007-all-err.csv",
      deltafmin  = 0.035-0.008,
      deltafmax  = 0.035+0.008,
      T0xtalmin  = 18,
      T0xtalmax  = 22,
      tauncmin   = 100 * 0.9,
      tauncmax   = 100 * 1.1,
      taucxmin   = 5 * 0.9,
      taucxmax   = 5 * 1.1
      );
  annotation(
    experiment(StartTime = 0, StopTime = 6000, Tolerance = 1e-06, Interval = 1),
    __OpenModelica_commandLineOptions = "--matchingAlgorithm=PFPlusExt --indexReductionMethod=dynamicStateSelection -d=initialization,NLSanalyticJacobian",
    __OpenModelica_simulationFlags(lv = "LOG_STATS", s = "dassl"));
end Test_007_revised_paper_all_params;