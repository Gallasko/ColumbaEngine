#pragma once

// Lightweight cross-system events used to coordinate the tutorial flow.
// Kept in its own header so SpotlightOverlaySystem and TutorialSystem can
// communicate without pulling in each other's full headers.

struct TutorialSkipRequested {};
