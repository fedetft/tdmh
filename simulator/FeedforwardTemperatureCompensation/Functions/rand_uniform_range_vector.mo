within FeedforwardTemperatureCompensation.Functions;

impure function rand_uniform_range_vector
  input  Real ymin;
  input  Real ymax;
  input  Integer n;
  output Real y[n];
algorithm
  for i in 1:n loop
    y[i] := rand_uniform_range(ymin,ymax);
  end for;
end rand_uniform_range_vector;