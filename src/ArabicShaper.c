#include "ArabicShaper.h"

#define ZWNJ 0x200C
#define ZWJ 0x200D
#define NONE ((size_t)-1)

enum { JC_U, JC_R, JC_D };            // non-joining, right-joining, dual
enum { F_ISO, F_FIN, F_INIT, F_MEDI };

typedef struct {
  uint16_t iso, fin, init, medi;      // 0 = form absent (fall back iso/fin)
} Forms;

// U+0621..U+064A -> Presentation Forms-B. iso==0 -> emit base codepoint.
static const Forms kForms[0x2A] = {
    {0xFE80, 0, 0, 0},                       // 0621 hamza
    {0xFE81, 0xFE82, 0, 0},                  // 0622 alef madda
    {0xFE83, 0xFE84, 0, 0},                  // 0623 alef hamza above
    {0xFE85, 0xFE86, 0, 0},                  // 0624 waw hamza
    {0xFE87, 0xFE88, 0, 0},                  // 0625 alef hamza below
    {0xFE89, 0xFE8A, 0xFE8B, 0xFE8C},        // 0626 yeh hamza
    {0xFE8D, 0xFE8E, 0, 0},                  // 0627 alef
    {0xFE8F, 0xFE90, 0xFE91, 0xFE92},        // 0628 beh
    {0xFE93, 0xFE94, 0, 0},                  // 0629 teh marbuta
    {0xFE95, 0xFE96, 0xFE97, 0xFE98},        // 062A teh
    {0xFE99, 0xFE9A, 0xFE9B, 0xFE9C},        // 062B theh
    {0xFE9D, 0xFE9E, 0xFE9F, 0xFEA0},        // 062C jeem
    {0xFEA1, 0xFEA2, 0xFEA3, 0xFEA4},        // 062D hah
    {0xFEA5, 0xFEA6, 0xFEA7, 0xFEA8},        // 062E khah
    {0xFEA9, 0xFEAA, 0, 0},                  // 062F dal
    {0xFEAB, 0xFEAC, 0, 0},                  // 0630 thal
    {0xFEAD, 0xFEAE, 0, 0},                  // 0631 reh
    {0xFEAF, 0xFEB0, 0, 0},                  // 0632 zain
    {0xFEB1, 0xFEB2, 0xFEB3, 0xFEB4},        // 0633 seen
    {0xFEB5, 0xFEB6, 0xFEB7, 0xFEB8},        // 0634 sheen
    {0xFEB9, 0xFEBA, 0xFEBB, 0xFEBC},        // 0635 sad
    {0xFEBD, 0xFEBE, 0xFEBF, 0xFEC0},        // 0636 dad
    {0xFEC1, 0xFEC2, 0xFEC3, 0xFEC4},        // 0637 tah
    {0xFEC5, 0xFEC6, 0xFEC7, 0xFEC8},        // 0638 zah
    {0xFEC9, 0xFECA, 0xFECB, 0xFECC},        // 0639 ain
    {0xFECD, 0xFECE, 0xFECF, 0xFED0},        // 063A ghain
    {0, 0, 0, 0},                            // 063B
    {0, 0, 0, 0},                            // 063C
    {0, 0, 0, 0},                            // 063D
    {0, 0, 0, 0},                            // 063E
    {0, 0, 0, 0},                            // 063F
    {0x0640, 0x0640, 0x0640, 0x0640},        // 0640 tatweel
    {0xFED1, 0xFED2, 0xFED3, 0xFED4},        // 0641 feh
    {0xFED5, 0xFED6, 0xFED7, 0xFED8},        // 0642 qaf
    {0xFED9, 0xFEDA, 0xFEDB, 0xFEDC},        // 0643 kaf
    {0xFEDD, 0xFEDE, 0xFEDF, 0xFEE0},        // 0644 lam
    {0xFEE1, 0xFEE2, 0xFEE3, 0xFEE4},        // 0645 meem
    {0xFEE5, 0xFEE6, 0xFEE7, 0xFEE8},        // 0646 noon
    {0xFEE9, 0xFEEA, 0xFEEB, 0xFEEC},        // 0647 heh
    {0xFEED, 0xFEEE, 0, 0},                  // 0648 waw
    {0xFEEF, 0xFEF0, 0, 0},                  // 0649 alef maksura (no B init/medi)
    {0xFEF1, 0xFEF2, 0xFEF3, 0xFEF4},        // 064A yeh
};

static const uint8_t kJoin[0x2A] = {
    JC_U, JC_R, JC_R, JC_R, JC_R, JC_D, JC_R, JC_D, JC_R, JC_D,  // 0621-062A
    JC_D, JC_D, JC_D, JC_D, JC_R, JC_R, JC_R, JC_R, JC_D, JC_D,  // 062B-0634
    JC_D, JC_D, JC_D, JC_D, JC_D, JC_D, JC_U, JC_U, JC_U, JC_U,  // 0635-063E
    JC_U, JC_D, JC_D, JC_D, JC_D, JC_D, JC_D, JC_D, JC_D, JC_R,  // 063F-0648
    JC_D, JC_D,                                                  // 0649-064A
};

int arabicIsMark(uint16_t cp) {
  return (cp >= 0x064B && cp <= 0x0652) || cp == 0x0670;
}

// Same semantics as PicoOled's nextUtf8Codepoint: invalid bytes -> '?'.
// Codepoints above U+FFFF also map to '?' (output is uint16_t).
static uint16_t nextCp(const char **p) {
  const uint8_t *s = (const uint8_t *)*p;
  uint8_t b = s[0];
  uint32_t cp;
  int len;
  if (b < 0x80) { cp = b; len = 1; }
  else if ((b & 0xE0) == 0xC0) { cp = b & 0x1F; len = 2; }
  else if ((b & 0xF0) == 0xE0) { cp = b & 0x0F; len = 3; }
  else if ((b & 0xF8) == 0xF0) { cp = b & 0x07; len = 4; }
  else { (*p)++; return '?'; }
  for (int i = 1; i < len; i++) {
    if ((s[i] & 0xC0) != 0x80) { (*p)++; return '?'; }
    cp = (cp << 6) | (uint32_t)(s[i] & 0x3F);
  }
  *p += len;
  if ((len == 2 && cp < 0x80) || (len == 3 && cp < 0x800) ||
      (len == 4 && cp < 0x10000))
    return '?';  // overlong
  if ((cp >= 0xD800 && cp <= 0xDFFF) || cp > 0xFFFF) return '?';
  return (uint16_t)cp;
}

// ---- bidi-lite ----

enum { D_N, D_L, D_R };

static int dirOf(uint16_t cp) {
  if ((cp >= '0' && cp <= '9') || (cp >= 'A' && cp <= 'Z') ||
      (cp >= 'a' && cp <= 'z'))
    return D_L;
  if (cp >= 0x00C0 && cp <= 0x024F) return D_L;
  if ((cp >= 0x0660 && cp <= 0x0669) || (cp >= 0x06F0 && cp <= 0x06F9))
    return D_L;  // digits render LTR
  if (cp >= 0x0600 && cp <= 0x06FF) return D_R;
  if (cp >= 0x0750 && cp <= 0x077F) return D_R;
  if (cp >= 0xFB50 && cp <= 0xFDFF) return D_R;
  if (cp >= 0xFE70 && cp <= 0xFEFF) return D_R;
  return D_N;
}

static uint16_t mirrorCp(uint16_t cp) {
  switch (cp) {
    case '(': return ')';
    case ')': return '(';
    case '[': return ']';
    case ']': return '[';
    case '{': return '}';
    case '}': return '{';
    case '<': return '>';
    case '>': return '<';
    default: return cp;
  }
}

static void reverseRange(uint16_t *a, size_t lo, size_t hi) {  // [lo, hi)
  while (lo + 1 < hi) {
    uint16_t t = a[lo];
    a[lo++] = a[--hi];
    a[hi] = t;
  }
}

// Logical -> visual assuming RTL base: mirror neutrals resolved RTL,
// reverse everything, then un-reverse LTR-resolved segments.
static void toVisual(uint16_t *a, size_t n) {
  size_t i = 0;
  int prev = D_R;
  while (i < n) {
    int d = dirOf(a[i]);
    if (d != D_N) { prev = d; i++; continue; }
    size_t j = i;
    while (j < n && dirOf(a[j]) == D_N) j++;
    int next = (j < n) ? dirOf(a[j]) : D_R;
    if (!(prev == D_L && next == D_L))
      for (size_t k = i; k < j; k++) a[k] = mirrorCp(a[k]);
    i = j;
  }
  reverseRange(a, 0, n);
  i = 0;
  prev = D_R;
  size_t seg = NONE;
  while (i < n) {
    int res;
    size_t j;
    int d = dirOf(a[i]);
    if (d != D_N) { res = d; j = i + 1; prev = d; }
    else {
      j = i;
      while (j < n && dirOf(a[j]) == D_N) j++;
      int next = (j < n) ? dirOf(a[j]) : D_R;
      res = (prev == D_L && next == D_L) ? D_L : D_R;
    }
    if (res == D_L) {
      if (seg == NONE) seg = i;
    } else if (seg != NONE) {
      reverseRange(a, seg, i);
      seg = NONE;
    }
    i = j;
  }
  if (seg != NONE) reverseRange(a, seg, n);
}

// ---- joining state machine ----

typedef struct {
  uint16_t *out;
  size_t n, cap;
  size_t prevIdx;     // last emitted joinable letter, NONE if join broken
  uint16_t prevBase;
  uint8_t prevForm;
  int linkPrev;       // previous char lets the next letter join backward
} Shaper;

static int emit(Shaper *s, uint16_t cp) {
  if (s->n >= s->cap) return 0;
  s->out[s->n++] = cp;
  return 1;
}

// iso -> init / fin -> medi when a following letter (or ZWJ) joins to prev.
static void upgradePrev(Shaper *s) {
  if (s->prevIdx == NONE) return;
  const Forms *f = &kForms[s->prevBase - 0x0621];
  if (s->prevForm == F_ISO && f->init) {
    s->out[s->prevIdx] = f->init;
    s->prevForm = F_INIT;
  } else if (s->prevForm == F_FIN && f->medi) {
    s->out[s->prevIdx] = f->medi;
    s->prevForm = F_MEDI;
  }
}

static uint16_t lamAlefLig(uint16_t alef) {
  switch (alef) {
    case 0x0622: return 0xFEF5;
    case 0x0623: return 0xFEF7;
    case 0x0625: return 0xFEF9;
    default: return 0xFEFB;  // 0627
  }
}

size_t arabicShape(const char *utf8, uint16_t *out, size_t capacity,
                   uint8_t flags) {
  Shaper s = {out, 0, capacity, NONE, 0, F_ISO, 0};
  const char *p = utf8;
  while (*p) {
    uint16_t cp = nextCp(&p);
    if (arabicIsMark(cp)) {  // transparent: join state untouched
      if (flags & ARABIC_SHAPE_KEEP_HARAKAT)
        if (!emit(&s, cp)) break;
      continue;
    }
    if (cp == ZWJ) {
      upgradePrev(&s);
      s.linkPrev = 1;
      continue;
    }
    if (cp == ZWNJ) {
      s.linkPrev = 0;
      s.prevIdx = NONE;
      continue;
    }
    if (cp >= 0x0621 && cp <= 0x064A) {
      const Forms *f = &kForms[cp - 0x0621];
      uint8_t jc = kJoin[cp - 0x0621];
      if (jc == JC_U) {
        if (!emit(&s, f->iso ? f->iso : cp)) break;
        s.linkPrev = 0;
        s.prevIdx = NONE;
        continue;
      }
      if (s.prevIdx != NONE && s.prevBase == 0x0644 &&
          (cp == 0x0622 || cp == 0x0623 || cp == 0x0625 || cp == 0x0627)) {
        int fin = (s.prevForm == F_FIN || s.prevForm == F_MEDI);
        s.out[s.prevIdx] = (uint16_t)(lamAlefLig(cp) + fin);
        s.prevBase = cp;
        s.prevForm = fin ? F_FIN : F_ISO;
        s.linkPrev = 0;
        continue;
      }
      int joinsBack = s.linkPrev;
      if (joinsBack) upgradePrev(&s);
      uint16_t form = joinsBack && f->fin ? f->fin : f->iso;
      if (!emit(&s, form)) break;
      s.prevIdx = s.n - 1;
      s.prevBase = cp;
      s.prevForm = joinsBack ? F_FIN : F_ISO;
      s.linkPrev = (jc == JC_D);
      continue;
    }
    // Non-joining: space, digits, punct, unknown -> pass through
    if (!emit(&s, cp)) break;
    s.linkPrev = 0;
    s.prevIdx = NONE;
  }
  toVisual(out, s.n);
  return s.n;
}
