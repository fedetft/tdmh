within FeedforwardTemperatureCompensation.Components;

model Example_exact_compensation
  parameter Boolean TcompON =  true;
  parameter Real    f0xtal  =  32768;
  parameter Real    T0xtal  =  25;
  parameter Real    deltaf  =  0.1;
  parameter Real    Tstart  =  25;
  parameter Real    tauec   =  100;
  parameter Real    taucx   =  5;
  parameter Real    alpha   =  0.5;
  parameter Real    Tsff    =  5;
  parameter Real    Tsfb    =  60;

  Real Text;
  Real Txtal(start=Tstart);
  Real Tcase(start=Tstart);
  Real fxtal,skew;
  Real offset(start=0);
  
  Real dfb_(start=0);
  Real ectfb(start=0);
  Real ectfbff(start=0);
  
  discrete Real dfb;
  discrete Real efb(start=0);
  discrete Real efb1(start=0);
  discrete Real efb2(start=0);
  discrete Real Txtal_hat,fxtal_hat,skew_hat,uff,uff_Tsfb(start=0);
  discrete Real ufb(start=0);
  discrete Real ufb1(start=0);
  discrete Real ufb2(start=0);
  
equation
  /* <Inputs> -----------------------------------------------*/
  // Text = if sin(2*Modelica.Constants.pi/5000*time)>0
  //        then  25+20
  //        else  25-20;

  Text = max(0,
             min(50,
                 25+100*sin(2*Modelica.Constants.pi/5000*time)
             )
         );
  /* </Inputs> ----------------------------------------------*/

  /* <Process> ----------------------------------------------*/
  Tcase+tauec*der(Tcase) = Text;
  Txtal+taucx*der(Txtal) = Tcase;
  fxtal                  = f0xtal*(1-deltaf*1e-6*(Txtal-T0xtal)^2);
  skew                   = 1-fxtal/f0xtal;
  der(offset)            = skew;
  /* </Process> ---------------------------------------------*/  
  
  /* <Sensing> ----------------------------------------------*/
  der(dfb_)    = skew;
  when sample(Tsfb,Tsfb) then
       dfb = dfb_;
       reinit(dfb_,0);
  end when;
  /* </Sensing> ---------------------------------------------*/
  
  /* <Errors_ct> --------------------------------------------*/
  der(ectfb)   = skew-ufb/Tsfb;
  der(ectfbff) = skew-ufb/Tsfb-uff/Tsff;
  when sample(Tsfb,Tsfb) then
       reinit(ectfb,efb);
       reinit(ectfbff,efb);
  end when;
  /* </Errors_ct> -------------------------------------------*/

algorithm

  /* <FFcompensation> ---------------------------------------*/
  when sample(0,Tsff) then
    if TcompON then
       Txtal_hat := Txtal;
       fxtal_hat := f0xtal*(1-deltaf*1e-6*(Txtal_hat-T0xtal)^2);
       skew_hat  := 1-fxtal_hat/f0xtal;
       uff       := skew_hat*Tsff;
       uff_Tsfb  := uff_Tsfb+uff;
     else
       uff_Tsfb  := 0;
     end if;
  end when;
  /* </FFcompensation> --------------------------------------*/
  
  /* <FBcontrol> --------------------------------------------*/
  when sample(0,Tsfb) then
    ufb2      :=  ufb1;
    ufb1      :=  ufb;    
    ufb       :=  2*ufb1-ufb2+3*(1-alpha)*efb
                 -3*(1-alpha^2)*efb1+(1-alpha^3)*efb2;
    efb2      :=  efb1;
    efb1      :=  efb;
    efb       :=  efb+dfb-ufb-uff_Tsfb;
    uff_Tsfb  :=  0;
  end when;
  /* <FBcontrol> --------------------------------------------*/

annotation(
    experiment(StartTime = 0, StopTime = 10000, Tolerance = 1e-6, Interval = 1),
    __OpenModelica_commandLineOptions = "--matchingAlgorithm=PFPlusExt --indexReductionMethod=dynamicStateSelection -d=initialization,NLSanalyticJacobian",
    __OpenModelica_simulationFlags(lv = "LOG_STATS", s = "dassl"));

end Example_exact_compensation;