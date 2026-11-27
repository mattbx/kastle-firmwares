// Compiles TapeLoop.hpp alone under strict warnings, -fno-exceptions and
// -fno-rtti, the way the firmware build treats it.
#include "apps/LastingDamage/TapeLoop.hpp"

void lasting_damage_portability_probe(int16_t *buf)
{
    lasting_damage::TapeLoop loop;
    loop.Init(buf, 1000, 10, 1);
    loop.SetWear(1000);
    int16_t l, r;
    loop.Process(1, 2, &l, &r);
}
