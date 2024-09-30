within FeedforwardTemperatureCompensation.Campaigns;

model Test_004_deltaf_T0xtal_largeTsfb
  extends BaseClasses.Campaign_base(csvName = "Test-004-deltaf-T0xtal-largeTsfb.csv", csvTs = 20, deltafmin = 0.035 - 0.008, deltafmax = 0.035 + 0.008, T0xtalmin = 18, T0xtalmax = 22, Tsfb = 120);
  annotation(
    experiment(StartTime = 0, StopTime = 15000, Tolerance = 1e-06, Interval = 1),
    __OpenModelica_commandLineOptions = "--matchingAlgorithm=PFPlusExt --indexReductionMethod=dynamicStateSelection -d=initialization,NLSanalyticJacobian",
    __OpenModelica_simulationFlags(lv = "LOG_STATS", s = "dassl"));
end Test_004_deltaf_T0xtal_largeTsfb;