within FeedforwardTemperatureCompensation.Components;

model Node_FS2_Tcomp_recparams

  parameter Boolean TcompON       =  true "T compensation on";
  parameter Boolean useTcase      =  true "otherwise Tnode";
  parameter Real    Tstart        =  20;
  parameter Real    T_sens_gain   = 1 "offset in used T measurement, nom=1";
  parameter Real    T_sens_offset = 0 "offset in used T measurement, nom=0";
  
  parameter Types.NodeThermalFreqData Node_nominal_Tf_params;
  parameter Types.NodeThermalFreqData Node_estimated_Tf_params;
  parameter Types.NodeControlParams   Node_control_params;
  
  input  Real Tnode;
  output Real e=efb;
  
protected  
  final parameter Real f0xtal     = Node_nominal_Tf_params.f0xtal;
  final parameter Real T0xtal     = Node_nominal_Tf_params.T0xtal;
  final parameter Real deltaf     = Node_nominal_Tf_params.deltaf;
  final parameter Real taunc      = Node_nominal_Tf_params.taunc;
  final parameter Real taucx      = Node_nominal_Tf_params.taucx;  
  
  final parameter Real f0xtal_hat = Node_estimated_Tf_params.f0xtal;
  final parameter Real T0xtal_hat = Node_estimated_Tf_params.T0xtal;
  final parameter Real deltaf_hat = Node_estimated_Tf_params.deltaf;
  final parameter Real taunc_hat  = Node_estimated_Tf_params.taunc;
  final parameter Real taucx_hat  = Node_estimated_Tf_params.taucx;   
   
  final parameter Real alpha      = Node_control_params.alpha;
  final parameter Real Tsff       = Node_control_params.Tsff;
  final parameter Real Tsfb       = Node_control_params.Tsfb;

  final parameter Real pnc     = exp(-Tsff/taunc) annotation(Evaluate = true);
  final parameter Real pcx     = exp(-Tsff/taucx) annotation(Evaluate = true);
  final parameter Real pnc_hat = exp(-Tsff/taunc_hat) annotation(Evaluate = true);
  final parameter Real pcx_hat = exp(-Tsff/taucx_hat) annotation(Evaluate = true);

  Real Tcase(start=Tstart);  
  Real Txtal(start=Tstart);
  Real fxtal,skew;
  Real offset(start=0);
  
  Real dfb_(start=0);
  Real ectfb(start=0);
  Real ectfbff(start=0);

  discrete Real Tcase_hat(start=Tstart);  
  discrete Real Txtal_hat(start=Tstart);
  discrete Real dfb;
  discrete Real efb(start=0);
  discrete Real efb1(start=0);
  discrete Real efb2(start=0);
  discrete Real fxtal_hat,skew_hat,uff;
  discrete Real uff_Tsfb(start=0);
  discrete Real ufb(start=0);
  discrete Real ufb1(start=0);
  discrete Real ufb2(start=0);
  
equation

  /* <Process> ----------------------------------------------*/
  Tcase+taunc*der(Tcase) = Tnode;
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

  /* <FFcompensation>  T meas err is here -------------------*/
  when sample(0,Tsff) then
    if TcompON then
       if useTcase then
          Txtal_hat := pcx_hat*Txtal_hat+(1-pcx_hat)*(T_sens_gain*Tcase+T_sens_offset);
       else
          Tcase_hat := pnc_hat*Tcase_hat+(1-pnc_hat)*(T_sens_gain*Tnode+T_sens_offset);
          Txtal_hat := pcx_hat*Txtal_hat+(1-pcx_hat)*Tcase_hat;
       end if;
       fxtal_hat := f0xtal_hat*(1-deltaf_hat*1e-6*(Txtal_hat-T0xtal_hat)^2);
       skew_hat  := 1-fxtal_hat/f0xtal_hat;
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

end Node_FS2_Tcomp_recparams;