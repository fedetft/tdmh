within FeedforwardTemperatureCompensation.Functions;

impure function rand_uniform_node_Tf_params
  input  Real                      f0xtalmin;
  input  Real                      f0xtalmax;
  input  Real                      T0xtalmin;
  input  Real                      T0xtalmax;
  input  Real                      deltafmin;
  input  Real                      deltafmax;
  input  Real                      tauncmin;
  input  Real                      tauncmax;
  input  Real                      taucxmin;
  input  Real                      taucxmax;
  output Types.NodeThermalFreqData r;
algorithm
  r.f0xtal := Functions.rand_uniform_range(f0xtalmin,f0xtalmax);
  r.T0xtal := Functions.rand_uniform_range(T0xtalmin,T0xtalmax);
  r.deltaf := Functions.rand_uniform_range(deltafmin,deltafmax);
  r.taunc  := Functions.rand_uniform_range(tauncmin,tauncmax);
  r.taucx  := Functions.rand_uniform_range(taucxmin,taucxmax);  
end rand_uniform_node_Tf_params;