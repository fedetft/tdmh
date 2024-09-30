#include <stdio.h>
#include <stdlib.h>
#include "Tcomp4sync_funs.h"

double rand_uniform_range(double ymin, double ymax)
{
  return ymin+(ymax-ymin)*(double)rand()/RAND_MAX;
}


