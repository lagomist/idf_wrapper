#pragma once

#include "bufdef.h"

namespace Wrapper {

namespace Shell {


using Callback = OBuf (*)(int argc, char* argv[]);

OBuf response(IBuf);
void registerCallback(Callback call);

}

}
