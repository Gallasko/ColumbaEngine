#include "motion.h"

namespace chronicle
{
    namespace
    {
        bool g_reduced = false;
    }

    bool Motion::reduced() { return g_reduced; }
    void Motion::setReduced(bool value) { g_reduced = value; }
}
