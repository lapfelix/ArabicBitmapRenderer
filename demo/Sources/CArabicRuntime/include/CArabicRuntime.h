#ifndef C_ARABIC_RUNTIME_H
#define C_ARABIC_RUNTIME_H

#include "../../../../src/ArabicFont.h"
#include "../../../../src/ArabicShaper.h"
#include "../../../../src/ArabicRenderer.h"

#ifdef __cplusplus
extern "C" {
#endif

// The generated 28px font (tables are static in the generated header).
const ArabicFont *demoArabicFont(void);

#ifdef __cplusplus
}
#endif

#endif
