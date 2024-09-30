within FeedforwardTemperatureCompensation.Campaigns;

model Test_006_all_params
  extends BaseClasses.Campaign_base(
      logTraces  = false,
      logTermErr = true,
      n          = 500,
      csvName    = "Test-006-all.csv",
      errfName   = "Test-006-all-err.csv",
      deltafmin  = 0.035*0.95,
      deltafmax  = 0.035*1.05,
      T0xtalmin  = 20*0.95,
      T0xtalmax  = 20*1.05,
      tauncmin   = 100 * 0.9,
      tauncmax   = 100 * 1.1,
      taucxmin   = 5 * 0.9,
      taucxmax   = 5 * 1.1
      );
  annotation(
    experiment(StartTime = 0, StopTime = 6000, Tolerance = 1e-06, Interval = 1),
    __OpenModelica_commandLineOptions = "--matchingAlgorithm=PFPlusExt --indexReductionMethod=dynamicStateSelection -d=initialization,NLSanalyticJacobian",
    __OpenModelica_simulationFlags(lv = "LOG_STATS", s = "dassl"));
end Test_006_all_params;