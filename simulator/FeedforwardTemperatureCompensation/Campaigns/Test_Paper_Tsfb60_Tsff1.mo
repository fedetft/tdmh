within FeedforwardTemperatureCompensation.Campaigns;

model Test_Paper_Tsfb60_Tsff1
  extends BaseClasses.Campaign_base(
      logTraces  = true,
      logTermErr = true,
      n          = 200,
      csvName    = "Test-Tsfb60-Tsff1.csv",
      errfName   = "Test-Tsfb60-Tsff1-err.csv",
      Tnodemin   = 30,
      Tnodemax   = 45,
      tauTnode1  = 900,
      tauTnode2  = 150,
      deltafmin  = 0.035-0.008,
      deltafmax  = 0.035+0.008,
      T0xtalmin  = 20-2,
      T0xtalmax  = 20+2,
      tauncmin   = 150 * 0.9,
      tauncmax   = 150 * 1.1,
      taucxmin   = 5 * 0.9,
      taucxmax   = 5 * 1.1,
      Tsfb       = 60,
      Tsff       = 1
      );
  annotation(
    experiment(StartTime = 0, StopTime = 8000, Tolerance = 1e-06, Interval = 1),
    __OpenModelica_commandLineOptions = "--matchingAlgorithm=PFPlusExt --indexReductionMethod=dynamicStateSelection -d=initialization,NLSanalyticJacobian",
    __OpenModelica_simulationFlags(lv = "LOG_STDOUT,LOG_ASSERT,LOG_STATS", s = "dassl", variableFilter = ".*"));
end Test_Paper_Tsfb60_Tsff1;