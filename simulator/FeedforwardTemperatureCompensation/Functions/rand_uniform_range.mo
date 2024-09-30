within FeedforwardTemperatureCompensation.Functions;

impure function rand_uniform_range
  input  Real ymin;
  input  Real ymax;
  output Real y;
external "C" y=rand_uniform_range(ymin,ymax) annotation(
      IncludeDirectory="modelica://FeedforwardTemperatureCompensation/Resources/Source/",
      Include="#include \"Tcomp4sync_funs.h\"",
      LibraryDirectory="modelica://FeedforwardTemperatureCompensation/Resources/Source/",
      Library="Tcomp4sync_funs");
end rand_uniform_range;