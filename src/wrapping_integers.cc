#include "wrapping_integers.hh"
using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  return zero_point + (uint32_t)n;
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  uint32_t dif = raw_value_ - zero_point.raw_value_ - checkpoint;
  if ( dif <= ( (uint64_t)1 << 31 ) || checkpoint + dif < ( (uint64_t)1 << 32 ) )
    return checkpoint + dif;
  else
    return checkpoint + dif - ( (uint64_t)1 << 32 );
}
