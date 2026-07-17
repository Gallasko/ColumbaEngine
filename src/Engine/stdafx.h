#pragma once

// C++ standard
#include <vector>
#include <string>
#include <map>
#include <memory>
#include <algorithm>
#include <functional>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <variant>
#include <unordered_map>
#include <cmath>

#include "Memory/elementtype.h"
#include "Files/filemanager.h"

#include "logger.h"
#include "serialization.h"
#include "configuration.h"
#include "pgconstant.h"

#include "Maths/geometry.h"

#include "ECS/entitysystem_fwd.h"
#include "ECS/system.h"

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
