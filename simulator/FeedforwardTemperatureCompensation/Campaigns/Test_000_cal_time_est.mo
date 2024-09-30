within FeedforwardTemperatureCompensation.Campaigns;

model Test_000_cal_time_est
  parameter String csvName    =  "Test-000-cal-time.csv";
  parameter Real   csvTs      =  10;
  parameter Real   f0xtal     =  32768;
  parameter Real   f0xtal_est =  32768;
  parameter Real   T0xtal     =  20;
  parameter Real   T0xtal_est =  20;
  parameter Real   deltaf     =  0.035;
  parameter Real   deltaf_est =  0.035;
  parameter Real   taunc      =  100;
  parameter Real   taucx      =  5;
  parameter Real   Tstart     =  15;
  parameter Real   DTstart    =  -2;
  parameter Real   Tmax       =  45;
  parameter Real   Tend       =  30;
  parameter Real   tramp_up   =  5*60;
  parameter Real   Dramp_up   =  30*60;
  parameter Real   DTmax      =  10*60;
  parameter Real   Dramp_dn   =  2*60;
  parameter Real   AnTn       =  1;
  parameter Real   AnTc       =  3;
  parameter Real   Anf        =  0.015;
  parameter Real   Tsn        =  0.5;
  parameter Real   tauTnCloop =  4;
  

  Modelica.Blocks.Continuous.TransferFunction Mnc(a = {taunc * taucx, taunc + taucx, 1}, initType =  Modelica.Blocks.Types.Init.InitialOutput, y_start = Tstart + DTstart)  annotation(
    Placement(visible = true, transformation(origin = {50, 10}, extent = {{-10, -10}, {10, 10}}, rotation = 0)));
  Modelica.Blocks.Noise.TruncatedNormalNoise noiseTc(samplePeriod = Tsn,useAutomaticLocalSeed = true, useGlobalSeed = true, y_max = AnTc, y_min = -AnTc)  annotation(
    Placement(visible = true, transformation(origin = {10, 50}, extent = {{-10, -10}, {10, 10}}, rotation = 0)));
  Modelica.Blocks.Noise.TruncatedNormalNoise noisef(samplePeriod = Tsn, useAutomaticLocalSeed = true, useGlobalSeed = true, y_max = Anf, y_min = -Anf)  annotation(
    Placement(visible = true, transformation(origin = {10, -30}, extent = {{-10, -10}, {10, 10}}, rotation = 0)));
  Modelica.Blocks.Continuous.FirstOrder FnTc(T = 20 * Tsn)  annotation(
    Placement(visible = true, transformation(origin = {50, 50}, extent = {{-10, -10}, {10, 10}}, rotation = 0)));
  Modelica.Blocks.Continuous.FirstOrder Fnf(T = 2 * Tsn)  annotation(
    Placement(visible = true, transformation(origin = {50, -30}, extent = {{-10, -10}, {10, 10}}, rotation = 0)));
  Modelica.Blocks.Sources.Ramp Tramp_up(duration = Dramp_up, height = Tmax - Tstart, offset = Tstart, startTime = tramp_up)  annotation(
    Placement(visible = true, transformation(origin = {-130, 10}, extent = {{-10, -10}, {10, 10}}, rotation = 0)));
  Modelica.Blocks.Math.Add mTc annotation(
    Placement(visible = true, transformation(origin = {90, 30}, extent = {{-10, -10}, {10, 10}}, rotation = 0)));
  Modelica.Blocks.Math.Add mf annotation(
    Placement(visible = true, transformation(origin = {90, -36}, extent = {{-10, -10}, {10, 10}}, rotation = 0)));
  Modelica.Blocks.Sources.RealExpression tx2f(y = f0xtal * (1 - deltaf * 1e-6 * (Mcx.y - T0xtal) ^ 2))  annotation(
    Placement(visible = true, transformation(origin = {50, -70}, extent = {{-10, -10}, {10, 10}}, rotation = 0)));

  Real Txfromfmeas;
  Modelica.Blocks.Math.Add Tnset annotation(
    Placement(visible = true, transformation(origin = {-90, -10}, extent = {{-10, -10}, {10, 10}}, rotation = 0)));
  Modelica.Blocks.Sources.Ramp Tramp_dn(duration = Dramp_dn, height = Tend -Tmax, offset = 0, startTime = tramp_up + Dramp_up + DTmax) annotation(
    Placement(visible = true, transformation(origin = {-130, -30}, extent = {{-10, -10}, {10, 10}}, rotation = 0)));

  discrete Real sTn(start=Tstart);
  discrete Real sTc(start=Tstart);
  discrete Real sf(start=f0xtal);
  
  Modelica.Blocks.Continuous.TransferFunction Mcx(a = {taunc * taucx, taunc + taucx, 1}, initType = Modelica.Blocks.Types.Init.InitialOutput, y_start = Tstart + DTstart) annotation(
    Placement(visible = true, transformation(origin = {130, 10}, extent = {{-10, -10}, {10, 10}}, rotation = 0)));
  Modelica.Blocks.Math.Add Tn annotation(
    Placement(visible = true, transformation(origin = {10, 10}, extent = {{-10, -10}, {10, 10}}, rotation = 0)));
  Modelica.Blocks.Noise.TruncatedNormalNoise noiseTn(samplePeriod = Tsn, useAutomaticLocalSeed = true, useGlobalSeed = true, y_max = AnTn, y_min = -AnTn) annotation(
    Placement(visible = true, transformation(origin = {-90, 30}, extent = {{-10, -10}, {10, 10}}, rotation = 0)));
  Modelica.Blocks.Continuous.FirstOrder FnTn(T = 50 * Tsn) annotation(
    Placement(visible = true, transformation(origin = {-50, 30}, extent = {{-10, -10}, {10, 10}}, rotation = 0)));
  Modelica.Blocks.Continuous.TransferFunction TnCloop(a = {tauTnCloop, 1}, initType = Modelica.Blocks.Types.Init.InitialOutput, y_start = Tstart + DTstart) annotation(
    Placement(visible = true, transformation(origin = {-50, -10}, extent = {{-10, -10}, {10, 10}}, rotation = 0)));
protected
  String csvLine;
  

equation

  
//  Txfromfmeas = (1000*sqrt(deltaf_est*f0xtal_est^2-deltaf_est*f0xtal_est*mf.y)
//                +T0xtal_est*deltaf_est*f0xtal_est)/(deltaf_est*f0xtal_est);
// *** fxtal = f0xtal*(1-deltaf*1e-6*(Txtal-T0xtal)^2)

  if abs(mf.y-f0xtal_est)>0.01 then
     mf.y = f0xtal_est*(1-deltaf_est*1e-6*(Txfromfmeas-T0xtal_est)^2);
  else 
     Txfromfmeas = mTc.y;
  end if;
  
  connect(noiseTc.y, FnTc.u) annotation(
    Line(points = {{21, 50}, {37, 50}}, color = {0, 0, 127}));
  connect(noisef.y, Fnf.u) annotation(
    Line(points = {{21, -30}, {37, -30}}, color = {0, 0, 127}));
  connect(Mnc.y, mTc.u2) annotation(
    Line(points = {{61, 10}, {68, 10}, {68, 24}, {77, 24}}, color = {0, 0, 127}));
  connect(FnTc.y, mTc.u1) annotation(
    Line(points = {{61, 50}, {69, 50}, {69, 36}, {77, 36}}, color = {0, 0, 127}));
  connect(Fnf.y, mf.u1) annotation(
    Line(points = {{61, -30}, {77, -30}}, color = {0, 0, 127}));
  connect(tx2f.y, mf.u2) annotation(
    Line(points = {{61, -70}, {69, -70}, {69, -42}, {77, -42}}, color = {0, 0, 127}));
  connect(Tramp_up.y, Tnset.u1) annotation(
    Line(points = {{-119, 10}, {-111, 10}, {-111, -4}, {-103, -4}}, color = {0, 0, 127}));
  connect(Tramp_dn.y, Tnset.u2) annotation(
    Line(points = {{-119, -30}, {-111, -30}, {-111, -16}, {-103, -16}}, color = {0, 0, 127}));
  connect(Mnc.y, Mcx.u) annotation(
    Line(points = {{61, 10}, {117, 10}}, color = {0, 0, 127}));
  connect(Tn.y, Mnc.u) annotation(
    Line(points = {{22, 10}, {38, 10}}, color = {0, 0, 127}));
  connect(noiseTn.y, FnTn.u) annotation(
    Line(points = {{-78, 30}, {-62, 30}}, color = {0, 0, 127}));
  connect(FnTn.y, Tn.u1) annotation(
    Line(points = {{-38, 30}, {-20, 30}, {-20, 16}, {-2, 16}}, color = {0, 0, 127}));
  connect(Tnset.y, TnCloop.u) annotation(
    Line(points = {{-78, -10}, {-62, -10}}, color = {0, 0, 127}));
  connect(TnCloop.y, Tn.u2) annotation(
    Line(points = {{-38, -10}, {-20, -10}, {-20, 4}, {-2, 4}}, color = {0, 0, 127}));
algorithm
  when initial() then
    Modelica.Utilities.Files.remove(csvName);
    csvLine := "time,Tn,Tc,f";
    Modelica.Utilities.Streams.print(csvLine,csvName);
  end when;
  when sample(0,csvTs) then
  
    sTn  := Tn.y;
    sTc  := mTc.y;
    sf   := mf.y;
  
  
    csvLine :=  String(time)+","
                +String(sTn,".4f")+","
                +String(sTc,".4f")+","
                +String(sf,".4f");
 
    Modelica.Utilities.Streams.print(csvLine,csvName);
  end when;
  when terminal() then
     Modelica.Utilities.Streams.close(csvName);
  end when;


annotation(
    Diagram(coordinateSystem(extent = {{-200, -100}, {200, 100}})),
    experiment(StartTime = 0, StopTime = 3600, Tolerance = 1e-6, Interval = 7.2),
    __OpenModelica_commandLineOptions = "--matchingAlgorithm=PFPlusExt --indexReductionMethod=dynamicStateSelection -d=initialization,NLSanalyticJacobian",
    __OpenModelica_simulationFlags(lv = "LOG_STATS", s = "dassl"));
end Test_000_cal_time_est;