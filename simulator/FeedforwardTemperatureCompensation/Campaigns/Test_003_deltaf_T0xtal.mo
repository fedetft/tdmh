within FeedforwardTemperatureCompensation.Campaigns;

model Test_003_deltaf_T0xtal
  extends BaseClasses.Campaign_base(
     csvName = "Test-003-deltaf-T0xtal.csv",
     deltafmin = 0.035 - 0.008,
     deltafmax = 0.035 + 0.008,
     T0xtalmin = 18,
     T0xtalmax = 22);
  annotation(
    experiment(StartTime = 0, StopTime = 6000, Tolerance = 1e-06, Interval = 1),
    __OpenModelica_commandLineOptions = "--matchingAlgorithm=PFPlusExt --indexReductionMethod=dynamicStateSelection -d=initialization,NLSanalyticJacobian",
    __OpenModelica_simulationFlags(lv = "LOG_STATS", s = "dassl"));
end Test_003_deltaf_T0xtal;