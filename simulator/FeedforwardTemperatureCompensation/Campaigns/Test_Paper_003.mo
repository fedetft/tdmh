within FeedforwardTemperatureCompensation.Campaigns;

model Test_Paper_003
  extends BaseClasses.Campaign_base(
      logTraces  =  false,
      logTermErr =  true,
      n          =  10,
      csvName    =  "Test-Paper-001-all.csv",
      errfName   =  "Test-Paper-001-all-err.csv",
      deltafmin  =  0.035-0.008,
      deltafmax  =  0.035+0.008,
      T0xtalmin  =  20-2,
      T0xtalmax  =  20+2,
      Tnodemin   =  29,
      Tnodemax   =  42.5,
      tauncmin   =  100 * 0.9,
      tauncmax   =  100 * 1.1,
      taucxmin   =  5 * 0.9,
      taucxmax   =  5 * 1.1,
      gainmin    =  0.9,
      gainmax    =  1.5,
      offsetmin  = -5,
      offsetmax  =  5,
      Tsfb       =  60,
      Tsff       =  1
      );
  annotation(
    experiment(StartTime = 0, StopTime = 6000, Tolerance = 1e-06, Interval = 1),
    __OpenModelica_commandLineOptions = "--matchingAlgorithm=PFPlusExt --indexReductionMethod=dynamicStateSelection -d=initialization,NLSanalyticJacobian",
    __OpenModelica_simulationFlags(lv = "LOG_STATS", s = "dassl"));
end Test_Paper_003;