within FeedforwardTemperatureCompensation.Campaigns.BaseClasses;

model Campaign_base
  parameter Boolean logTraces   =  true;
  parameter Boolean logTermErr  =  true;
  parameter Integer n           =  5;
  parameter String  csvName     =  "data.txt";
  parameter String  errfName    =  "errors.txt";
  parameter Real    csvTs       =  5;
  parameter Real    f0xtalmin   =  32768;
  parameter Real    f0xtalmax   =  32768;
  parameter Real    T0xtalmin   =  20;
  parameter Real    T0xtalmax   =  20;
  parameter Real    deltafmin   =  0.035;
  parameter Real    deltafmax   =  0.035;
  parameter Real    tauncmin    =  100;
  parameter Real    tauncmax    =  100;
  parameter Real    taucxmin    =  5;
  parameter Real    taucxmax    =  5;
  parameter Real    Tnodemin    =  25;
  parameter Real    Tnodemax    =  45;
  parameter Real    gainmin     =  1;
  parameter Real    gainmax     =  1;
  parameter Real    offsetmin   =  0;
  parameter Real    offsetmax   =  0;
  parameter Real    timeTnstep  =  2000;
  parameter Real    tauTnode1   =  500;
  parameter Real    tauTnode2   =  100;
  parameter Real    toffset     =  1800;
  
  parameter Real    alpha       =  3/8;
  parameter Real    Tsfb        =  60;  
  parameter Real    Tsff        =  10;
  
  parameter Types.NodeThermalFreqData ntf_nom(
                    f0xtal      =  32768,
                    T0xtal      =  20,
                    deltaf      =  0.035,
                    taunc       =  100,
                    taucx       =  5
  );
  
  parameter Types.NodeControlParams ncp(
                    alpha = alpha,
                    Tsff  = Tsff,
                    Tsfb  = Tsfb  
  );
  
  
  Real Tnode(start = Tnodemin);
  Real e_off, e_on_ideal, e_on[n];
  Real t = time - toffset;
  
  Real ISE_off(start=0);
  Real ISE_on_ideal(start=0);
  Real ISE_on[n](each start=0);
  
  discrete Real emax_off(start=-1);
  discrete Real emax_on_ideal(start=-1);
  discrete Real emax_on[n](each start=-1);
  
protected

  Real xTnode(start=Tnodemin);
  String csvLine;

  final parameter Types.NodeThermalFreqData ntf_est[n](
        f0xtal = Functions.rand_uniform_range_vector(f0xtalmin,f0xtalmax,n),
        T0xtal = Functions.rand_uniform_range_vector(T0xtalmin,T0xtalmax,n),
        deltaf = Functions.rand_uniform_range_vector(deltafmin,deltafmax,n),
        taunc  = Functions.rand_uniform_range_vector(tauncmin,tauncmax,n),
        taucx  = Functions.rand_uniform_range_vector(taucxmin,taucxmax,n),
        gain   = Functions.rand_uniform_range_vector(gainmin,gainmax,n),
        offset = Functions.rand_uniform_range_vector(offsetmin,offsetmax,n)
        );
                      
  Components.Node_FS2_Tcomp_recparams n_off
                      (TcompON=false,
                       Tstart=Tnodemin,
                       Node_nominal_Tf_params=ntf_nom,
                       Node_estimated_Tf_params=ntf_nom,
                       Node_control_params=ncp);
                       
  Components.Node_FS2_Tcomp_recparams n_on_ideal
                      (TcompON=true,
                       Tstart=Tnodemin,
                       Node_nominal_Tf_params=ntf_nom,
                       Node_estimated_Tf_params=ntf_nom,
                       Node_control_params=ncp);
                       
  Components.Node_FS2_Tcomp_recparams n_on[n]
                      (each TcompON=true,
                       each useTcase=false,
                       each Tstart=Tnodemin,
                       each Node_nominal_Tf_params=ntf_nom,
                       Node_estimated_Tf_params=ntf_est,
                       each Node_control_params=ncp);


equation

  der(ISE_off) = if t>=0 then e_off^2 else 0;
  der(ISE_on_ideal) = if t>=0 then e_on_ideal^2 else 0;
  for i in 1:n loop
    der(ISE_on[i]) = if t>=0 then e_on[i]^2 else 0;
  end for;
  
  xTnode+tauTnode1*der(xTnode)
    = if time<timeTnstep
      then Tnodemin
      else Tnodemax;
  Tnode+tauTnode2*der(Tnode) = xTnode;  

  n_off.Tnode = Tnode;
  e_off       = n_off.e;
  
  n_on_ideal.Tnode = Tnode;
  e_on_ideal       = n_on_ideal.e;

  for i in 1:n loop
    n_on[i].Tnode = Tnode;
    e_on[i]       = n_on[i].e;
  end for;
  
algorithm




  when initial() then
     if logTraces then
        Modelica.Utilities.Files.remove(csvName);
        csvLine := "time,Tnode,e_off,e_ideal";
        for i in 1:n loop
           csvLine := csvLine+",e_"+String(i);
        end for; 
        Modelica.Utilities.Streams.print(csvLine,csvName);
     end if;
  end when;
  when sample(0,csvTs) then
     if t>=0 and abs(e_off)>emax_off then
        emax_off := abs(e_off);
     end if;
     if t>=0 and abs(e_on_ideal)>emax_on_ideal then
        emax_on_ideal := abs(e_on_ideal);
     end if;
     for i in 1:n loop
        if t>=0 and abs(e_on[i])>emax_on[i] then 
           emax_on[i] := abs(e_on[i]);
        end if;
     end for;   
     if logTraces and t>=0 then
        csvLine :=  String(t)+","
                   +String(Tnode)+","
                   +String(1e6*e_off)+","
                   +String(1e6*e_on_ideal);
        for i in 1:n loop
           csvLine := csvLine+","+String(1e6*e_on[i]);
        end for;
        Modelica.Utilities.Streams.print(csvLine,csvName);
     end if;
  end when;
  when terminal() then
     Modelica.Utilities.Streams.close(csvName);
     if logTermErr then
        Modelica.Utilities.Files.remove(errfName);
        Modelica.Utilities.Streams.print("ISE,emax",errfName);
        Modelica.Utilities.Streams.print(
             String(ISE_off)+","
            +String(emax_off),errfName);
        Modelica.Utilities.Streams.print(
             String(ISE_on_ideal)+","
            +String(emax_on_ideal),errfName);
        for i in 1:n loop
           Modelica.Utilities.Streams.print(
               String(ISE_on[i])+","
              +String(emax_on[i]),errfName);    
        end for;
        Modelica.Utilities.Streams.close(errfName);
     end if;
  end when;
  
  annotation(
    experiment(StartTime = 0, StopTime = 6000, Tolerance = 1e-6, Interval = 1),
    __OpenModelica_commandLineOptions = "--matchingAlgorithm=PFPlusExt --indexReductionMethod=dynamicStateSelection -d=initialization,NLSanalyticJacobian",
    __OpenModelica_simulationFlags(lv = "LOG_STATS", s = "dassl"));
end Campaign_base;