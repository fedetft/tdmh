within FeedforwardTemperatureCompensation.Components;

model Node_FS2_Tcomp
  parameter Boolean TcompON = true "T compensation on";
  parameter Boolean useTcase = true "otherwise Tnode";
  parameter Real f0xtal = 32768;
  parameter Real T0xtal = 25;
  parameter Real deltaf = 0.035;
  parameter Real taunc = 100;
  parameter Real taucx = 5;
  parameter Real f0xtal_hat = 32768;
  parameter Real T0xtal_hat = 25;
  parameter Real deltaf_hat = 0.035;
  parameter Real taunc_hat = 100;
  parameter Real taucx_hat = 5;
  parameter Real alpha = 0.5;
  parameter Real Tsff = 5;
  parameter Real Tsfb = 60;
  parameter Real Tstart = 25;
  input Real Tnode;
  output Real e = efb;
  //protected
  final parameter Real pnc = exp(-Tsff/taunc) annotation(
    Evaluate = true);
  final parameter Real pcx = exp(-Tsff/taucx) annotation(
    Evaluate = true);
  Real Tcase(start = Tstart);
  Real Txtal(start = Tstart);
  Real fxtal, skew;
  Real offset(start = 0);
  Real dfb_(start = 0);
  Real ectfb(start = 0);
  Real ectfbff(start = 0);
  discrete Real Tcase_hat(start = Tstart);
  discrete Real Txtal_hat(start = Tstart);
  discrete Real dfb;
  discrete Real efb(start = 0);
  discrete Real efb1(start = 0);
  discrete Real efb2(start = 0);
  discrete Real fxtal_hat, skew_hat, uff;
  discrete Real uff_Tsfb(start = 0);
  discrete Real ufb(start = 0);
  discrete Real ufb1(start = 0);
  discrete Real ufb2(start = 0);
equation
/* <Process> ----------------------------------------------*/
  Tcase + taunc*der(Tcase) = Tnode;
  Txtal + taucx*der(Txtal) = Tcase;
  fxtal = f0xtal*(1 - deltaf*1e-6*(Txtal - T0xtal)^2);
  skew = 1 - fxtal/f0xtal;
  der(offset) = skew;
/* </Process> ---------------------------------------------*/
/* <Sensing> ----------------------------------------------*/
  der(dfb_) = skew;
  when sample(Tsfb, Tsfb) then
    dfb = dfb_;
    reinit(dfb_, 0);
  end when;
/* </Sensing> ---------------------------------------------*/
/* <Errors_ct> --------------------------------------------*/
  der(ectfb) = skew - ufb/Tsfb;
  der(ectfbff) = skew - ufb/Tsfb - uff/Tsff;
  when sample(Tsfb, Tsfb) then
    reinit(ectfb, efb);
    reinit(ectfbff, efb);
  end when;
/* </Errors_ct> -------------------------------------------*/
algorithm
/* <FFcompensation> ---------------------------------------*/
  when sample(0, Tsff) then
    if TcompON then
      if useTcase then
        Txtal_hat := pcx*Txtal_hat + (1 - pcx)*Tcase;
      else
        Tcase_hat := pnc*Tcase_hat + (1 - pnc)*Tnode;
        Txtal_hat := pcx*Txtal_hat + (1 - pcx)*Tcase_hat;
      end if;
      fxtal_hat := f0xtal_hat*(1 - deltaf_hat*1e-6*(Txtal_hat - T0xtal_hat)^2);
      skew_hat := 1 - fxtal_hat/f0xtal_hat;
      uff := skew_hat*Tsff;
      uff_Tsfb := uff_Tsfb + uff;
    else
      uff_Tsfb := 0;
    end if;
  end when;
/* </FFcompensation> --------------------------------------*/
/* <FBcontrol> --------------------------------------------*/
  when sample(0, Tsfb) then
    ufb2 := ufb1;
    ufb1 := ufb;
    ufb := 2*ufb1 - ufb2 + 3*(1 - alpha)*efb - 3*(1 - alpha^2)*efb1 + (1 - alpha^3)*efb2;
    efb2 := efb1;
    efb1 := efb;
    efb := efb + dfb - ufb - uff_Tsfb;
    uff_Tsfb := 0;
  end when;
/* <FBcontrol> --------------------------------------------*/
end Node_FS2_Tcomp;