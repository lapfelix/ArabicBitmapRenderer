// Offline glyph generator: renders Arabic presentation forms from a variable
// TTF into a packed 1bpp bitmap font header (see src/ArabicFont.h).
// Usage: swift tools/GenerateArabicFont.swift <font.ttf> --size 18 --weight 450 --out <dir> [--name <FontName>]

import Foundation
import CoreText
import CoreGraphics

func fail(_ msg: String) -> Never {
    FileHandle.standardError.write(("error: " + msg + "\n").data(using: .utf8)!)
    exit(1)
}

// MARK: - CLI

var args = Array(CommandLine.arguments.dropFirst())
guard !args.isEmpty else {
    fail("usage: GenerateArabicFont.swift <font.ttf> --size <px> --weight <wght> --out <dir> [--name <FontName>]")
}
let fontPath = args.removeFirst()
var size = 18
var weight = 450.0
var outDir = "src/generated"
var fontName: String? = nil
var i = 0
while i < args.count {
    let a = args[i]
    func value() -> String {
        i += 1
        guard i < args.count else { fail("missing value for \(a)") }
        return args[i]
    }
    switch a {
    case "--size": size = Int(value()) ?? 0
    case "--weight": weight = Double(value()) ?? 0
    case "--out": outDir = value()
    case "--name": fontName = value()
    default: fail("unknown argument \(a)")
    }
    i += 1
}
guard size > 0, weight > 0 else { fail("invalid --size/--weight") }
let name = fontName ?? "NaskhArabic\(size)"
let varName = name.prefix(1).lowercased() + name.dropFirst() + "Font"

// MARK: - Font instantiation

let fontURL = URL(fileURLWithPath: fontPath)
guard let descriptors = CTFontManagerCreateFontDescriptorsFromURL(fontURL as CFURL) as? [CTFontDescriptor],
      let descriptor = descriptors.first else {
    fail("cannot load font at \(fontPath)")
}
let baseFont = CTFontCreateWithFontDescriptor(descriptor, CGFloat(size), nil)
let wghtTag = 0x77676874 as Int  // 'wght'
let variedDescriptor = CTFontDescriptorCreateCopyWithAttributes(
    CTFontCopyFontDescriptor(baseFont),
    [kCTFontVariationAttribute: [NSNumber(value: wghtTag): weight]] as CFDictionary)
let font = CTFontCreateWithFontDescriptor(variedDescriptor, CGFloat(size), nil)

let axes = CTFontCopyVariationAxes(baseFont) as? [[String: Any]] ?? []
guard let wghtAxis = axes.first(where: { ($0[kCTFontVariationAxisIdentifierKey as String] as? Int) == wghtTag }) else {
    fail("font has no wght axis; is it the variable TTF?")
}
let wghtMin = (wghtAxis[kCTFontVariationAxisMinimumValueKey as String] as? Double) ?? 0
let wghtMax = (wghtAxis[kCTFontVariationAxisMaximumValueKey as String] as? Double) ?? 0
let wghtDefault = (wghtAxis[kCTFontVariationAxisDefaultValueKey as String] as? Double) ?? 0
if weight < wghtMin || weight > wghtMax {
    fail("wght \(weight) outside this font's axis range \(wghtMin)–\(wghtMax)")
}
// CTFontCopyVariation returns empty at the axis default, so only verify off-default weights.
if abs(weight - wghtDefault) >= 0.5 {
    let variation = CTFontCopyVariation(font) as? [NSNumber: NSNumber] ?? [:]
    guard let applied = variation[NSNumber(value: wghtTag)]?.doubleValue, abs(applied - weight) < 0.5 else {
        fail("wght variation not applied: \(variation)")
    }
}
// Cross-check: the weight must actually change outlines vs the default instance.
if abs(weight - 400) > 1 {
    var g = [CGGlyph(0)]
    var ch: [UniChar] = [0x0628]
    CTFontGetGlyphsForCharacters(font, &ch, &g, 1)
    var rBase = CGRect.zero, rVar = CGRect.zero
    _ = CTFontGetBoundingRectsForGlyphs(baseFont, .default, g, &rBase, 1)
    _ = CTFontGetBoundingRectsForGlyphs(font, .default, g, &rVar, 1)
    if rBase == rVar { fail("variable weight had no effect on glyph bounds") }
}

let ascent = CTFontGetAscent(font)
let descent = CTFontGetDescent(font)
let lineHeight = Int(ceil(ascent + descent))
let baseline = Int(ceil(ascent))
guard lineHeight <= 255, baseline <= 255 else { fail("line metrics exceed uint8") }

// MARK: - Glyph set

struct FormEntry {
    let codepoint: Int       // key stored in the table
    let context: [UnicodeScalar]  // string shaped by CoreText
    let targetIndices: Set<Int>   // UTF-16 indices of the wanted char(s)
}

let ZWJ = UnicodeScalar(0x200D)!

// base letter -> [isolated, final, initial, medial] presentation forms (0 = none)
let contextualForms: [(Int, [Int])] = [
    (0x0621, [0xFE80, 0, 0, 0]),
    (0x0622, [0xFE81, 0xFE82, 0, 0]),
    (0x0623, [0xFE83, 0xFE84, 0, 0]),
    (0x0624, [0xFE85, 0xFE86, 0, 0]),
    (0x0625, [0xFE87, 0xFE88, 0, 0]),
    (0x0626, [0xFE89, 0xFE8A, 0xFE8B, 0xFE8C]),
    (0x0627, [0xFE8D, 0xFE8E, 0, 0]),
    (0x0628, [0xFE8F, 0xFE90, 0xFE91, 0xFE92]),
    (0x0629, [0xFE93, 0xFE94, 0, 0]),
    (0x062A, [0xFE95, 0xFE96, 0xFE97, 0xFE98]),
    (0x062B, [0xFE99, 0xFE9A, 0xFE9B, 0xFE9C]),
    (0x062C, [0xFE9D, 0xFE9E, 0xFE9F, 0xFEA0]),
    (0x062D, [0xFEA1, 0xFEA2, 0xFEA3, 0xFEA4]),
    (0x062E, [0xFEA5, 0xFEA6, 0xFEA7, 0xFEA8]),
    (0x062F, [0xFEA9, 0xFEAA, 0, 0]),
    (0x0630, [0xFEAB, 0xFEAC, 0, 0]),
    (0x0631, [0xFEAD, 0xFEAE, 0, 0]),
    (0x0632, [0xFEAF, 0xFEB0, 0, 0]),
    (0x0633, [0xFEB1, 0xFEB2, 0xFEB3, 0xFEB4]),
    (0x0634, [0xFEB5, 0xFEB6, 0xFEB7, 0xFEB8]),
    (0x0635, [0xFEB9, 0xFEBA, 0xFEBB, 0xFEBC]),
    (0x0636, [0xFEBD, 0xFEBE, 0xFEBF, 0xFEC0]),
    (0x0637, [0xFEC1, 0xFEC2, 0xFEC3, 0xFEC4]),
    (0x0638, [0xFEC5, 0xFEC6, 0xFEC7, 0xFEC8]),
    (0x0639, [0xFEC9, 0xFECA, 0xFECB, 0xFECC]),
    (0x063A, [0xFECD, 0xFECE, 0xFECF, 0xFED0]),
    (0x0641, [0xFED1, 0xFED2, 0xFED3, 0xFED4]),
    (0x0642, [0xFED5, 0xFED6, 0xFED7, 0xFED8]),
    (0x0643, [0xFED9, 0xFEDA, 0xFEDB, 0xFEDC]),
    (0x0644, [0xFEDD, 0xFEDE, 0xFEDF, 0xFEE0]),
    (0x0645, [0xFEE1, 0xFEE2, 0xFEE3, 0xFEE4]),
    (0x0646, [0xFEE5, 0xFEE6, 0xFEE7, 0xFEE8]),
    (0x0647, [0xFEE9, 0xFEEA, 0xFEEB, 0xFEEC]),
    (0x0648, [0xFEED, 0xFEEE, 0, 0]),
    (0x0649, [0xFEEF, 0xFEF0, 0, 0]),
    (0x064A, [0xFEF1, 0xFEF2, 0xFEF3, 0xFEF4]),
]

// lam-alef ligatures: (form codepoint, alef variant, final?)
let lamAlefLigatures: [(Int, Int, Bool)] = [
    (0xFEF5, 0x0622, false), (0xFEF6, 0x0622, true),
    (0xFEF7, 0x0623, false), (0xFEF8, 0x0623, true),
    (0xFEF9, 0x0625, false), (0xFEFA, 0x0625, true),
    (0xFEFB, 0x0627, false), (0xFEFC, 0x0627, true),
]

var entries: [FormEntry] = []
for (base, forms) in contextualForms {
    let x = UnicodeScalar(base)!
    let contexts: [(Int, [UnicodeScalar], Int)] = [
        (forms[0], [x], 0),           // isolated
        (forms[1], [ZWJ, x], 1),      // final
        (forms[2], [x, ZWJ], 0),      // initial
        (forms[3], [ZWJ, x, ZWJ], 1), // medial
    ]
    for (cp, ctx, idx) in contexts where cp != 0 {
        entries.append(FormEntry(codepoint: cp, context: ctx, targetIndices: [idx]))
    }
}
for (cp, alef, isFinal) in lamAlefLigatures {
    let lam = UnicodeScalar(0x0644)!
    let a = UnicodeScalar(alef)!
    let ctx: [UnicodeScalar] = isFinal ? [ZWJ, lam, a] : [lam, a]
    let lamIdx = isFinal ? 1 : 0
    entries.append(FormEntry(codepoint: cp, context: ctx, targetIndices: [lamIdx, lamIdx + 1]))
}
var standalone: [Int] = [0x0020, 0x0640, 0x060C, 0x061B, 0x061F]
standalone += (0x0660...0x0669).map { $0 }
for cp in standalone {
    entries.append(FormEntry(codepoint: cp, context: [UnicodeScalar(cp)!], targetIndices: [0]))
}

// MARK: - Shaping + rasterization

struct PlacedGlyph {
    let glyph: CGGlyph
    let font: CTFont
    let position: CGPoint  // line coordinates
    let stringIndex: Int
}

func shapeLine(_ scalars: [UnicodeScalar]) -> (glyphs: [PlacedGlyph], width: CGFloat) {
    let s = String(String.UnicodeScalarView(scalars))
    let attr = NSAttributedString(string: s, attributes: [
        NSAttributedString.Key(kCTFontAttributeName as String): font,
    ])
    let line = CTLineCreateWithAttributedString(attr)
    let width = CGFloat(CTLineGetTypographicBounds(line, nil, nil, nil))
    let runs = CTLineGetGlyphRuns(line) as! [CTRun]
    var placed: [PlacedGlyph] = []
    for run in runs {
        let n = CTRunGetGlyphCount(run)
        guard n > 0 else { continue }
        var glyphs = [CGGlyph](repeating: 0, count: n)
        var positions = [CGPoint](repeating: .zero, count: n)
        var indices = [CFIndex](repeating: 0, count: n)
        CTRunGetGlyphs(run, CFRange(), &glyphs)
        CTRunGetPositions(run, CFRange(), &positions)
        CTRunGetStringIndices(run, CFRange(), &indices)
        let attrs = CTRunGetAttributes(run) as! [NSAttributedString.Key: Any]
        let runFont = attrs[NSAttributedString.Key(kCTFontAttributeName as String)] as! CTFont
        for k in 0..<n {
            placed.append(PlacedGlyph(glyph: glyphs[k], font: runFont,
                                      position: positions[k], stringIndex: indices[k]))
        }
    }
    return (placed, width)
}

let ctxW = 8 * size
let ctxH = 6 * size
let originX = 3 * size
let originY = 2 * size  // baseline height from context bottom

func makeContext() -> CGContext {
    guard let ctx = CGContext(data: nil, width: ctxW, height: ctxH,
                              bitsPerComponent: 8, bytesPerRow: ctxW,
                              space: CGColorSpaceCreateDeviceGray(),
                              bitmapInfo: CGImageAlphaInfo.none.rawValue) else {
        fail("cannot create bitmap context")
    }
    ctx.setAllowsAntialiasing(true)
    ctx.setShouldAntialias(true)
    ctx.setFillColor(gray: 1, alpha: 1)
    return ctx
}

// Returns rows of booleans (top to bottom), thresholded at >=128.
func thresholded(_ ctx: CGContext) -> [[Bool]] {
    let data = ctx.data!.bindMemory(to: UInt8.self, capacity: ctxW * ctxH)
    // Bitmap memory is top-down already (row 0 = highest CG y).
    return (0..<ctxH).map { row in
        (0..<ctxW).map { data[row * ctxW + $0] >= 128 }
    }
}

struct RenderedGlyph {
    let codepoint: Int
    let width: Int
    let height: Int
    let xOffset: Int
    let yOffset: Int
    let advance: Int
    let rows: [[Bool]]  // tight bbox, top to bottom
}

func render(_ entry: FormEntry) -> RenderedGlyph {
    let (placed, lineWidth) = shapeLine(entry.context)
    let candidates = placed.filter { entry.targetIndices.contains($0.stringIndex) }
    guard !candidates.isEmpty else {
        fail(String(format: "no glyph found for U+%04X", entry.codepoint))
    }
    // ZWJ anchors delimit the form's pen slot: a trailing ZWJ (higher string
    // index) sits at the form's left/pen edge, a leading ZWJ (lower string
    // index) at its right edge. Positions are authoritative; per-glyph run
    // advances aren't (marks carry negative deltas).
    let lo = entry.targetIndices.min()!
    let hi = entry.targetIndices.max()!
    let penX = placed.first { $0.stringIndex > hi }?.position.x ?? 0
    let rightX = placed.first { $0.stringIndex < lo }?.position.x ?? lineWidth
    let advance = rightX - penX
    let ctx = makeContext()
    for c in candidates {
        var g = c.glyph
        var pos = CGPoint(x: CGFloat(originX) + c.position.x - penX,
                          y: CGFloat(originY) + c.position.y)
        CTFontDrawGlyphs(c.font, &g, &pos, 1, ctx)
    }
    let pixels = thresholded(ctx)

    var minX = ctxW, maxX = -1, minY = ctxH, maxY = -1
    for y in 0..<ctxH {
        for x in 0..<ctxW where pixels[y][x] {
            minX = min(minX, x); maxX = max(maxX, x)
            minY = min(minY, y); maxY = max(maxY, y)
        }
    }
    let lineTopRow = ctxH - (originY + baseline)
    if maxX < 0 {  // no ink (space)
        return RenderedGlyph(codepoint: entry.codepoint, width: 0, height: 0,
                             xOffset: 0, yOffset: 0,
                             advance: Int((advance).rounded()), rows: [])
    }
    let w = maxX - minX + 1
    let h = maxY - minY + 1
    let xOff = minX - originX
    let yOff = minY - lineTopRow
    let adv = Int(advance.rounded())
    guard w <= 255, h <= 255, adv >= 0, adv <= 255,
          xOff >= -128, xOff <= 127, yOff >= -128, yOff <= 127 else {
        fail(String(format: "metrics out of range for U+%04X: w=%d h=%d xOff=%d yOff=%d adv=%d",
                    entry.codepoint, w, h, xOff, yOff, adv))
    }
    let rows = (minY...maxY).map { y in (minX...maxX).map { pixels[y][$0] } }
    return RenderedGlyph(codepoint: entry.codepoint, width: w, height: h,
                         xOffset: xOff, yOffset: yOff, advance: adv, rows: rows)
}

var rendered = entries.map(render)
rendered.sort { $0.codepoint < $1.codepoint }
let byCodepoint = Dictionary(uniqueKeysWithValues: rendered.map { ($0.codepoint, $0) })

// MARK: - Join self-test: composite a shaped word from our glyphs vs CTLine

func selfTest(word: [UnicodeScalar], forms: [Int]) -> (pass: Bool, detail: String) {
    // Composite from our table: forms given in visual order (left to right).
    let totalAdv = forms.reduce(0) { $0 + byCodepoint[$1]!.advance }
    let bufW = totalAdv + 2 * size
    let bufH = lineHeight + size
    let padX = size, padTop = size / 2
    var mine = [[Bool]](repeating: [Bool](repeating: false, count: bufW), count: bufH)
    var pen = padX
    for cp in forms {
        let g = byCodepoint[cp]!
        for r in 0..<g.height {
            let y = padTop + g.yOffset + r
            guard y >= 0, y < bufH else { continue }
            for c in 0..<g.width where g.rows[r][c] {
                let x = pen + g.xOffset + c
                if x >= 0, x < bufW { mine[y][x] = true }
            }
        }
        pen += g.advance
    }
    // Reference: shape + draw the whole word with CoreText.
    let (placed, _) = shapeLine(word)
    let ctx = makeContext()
    for c in placed {
        var g = c.glyph
        var pos = CGPoint(x: CGFloat(originX) + c.position.x, y: CGFloat(originY) + c.position.y)
        CTFontDrawGlyphs(c.font, &g, &pos, 1, ctx)
    }
    let pixels = thresholded(ctx)
    let lineTopRow = ctxH - (originY + baseline)
    var theirs = [[Bool]](repeating: [Bool](repeating: false, count: bufW), count: bufH)
    for y in 0..<bufH {
        for x in 0..<bufW {
            let sy = lineTopRow - padTop + y
            let sx = originX - padX + x
            if sy >= 0, sy < ctxH, sx >= 0, sx < ctxW { theirs[y][x] = pixels[sy][sx] }
        }
    }
    // Diff with ±1 px neighbor tolerance (rounding noise).
    func hasNeighbor(_ buf: [[Bool]], _ y: Int, _ x: Int) -> Bool {
        for dy in -1...1 {
            for dx in -1...1 {
                let ny = y + dy, nx = x + dx
                if ny >= 0, ny < bufH, nx >= 0, nx < bufW, buf[ny][nx] { return true }
            }
        }
        return false
    }
    var unmatched = 0, ink = 0
    for y in 0..<bufH {
        for x in 0..<bufW {
            if mine[y][x] { ink += 1; if !hasNeighbor(theirs, y, x) { unmatched += 1 } }
            if theirs[y][x] { ink += 1; if !hasNeighbor(mine, y, x) { unmatched += 1 } }
        }
    }
    let pass = ink > 0 && Double(unmatched) <= 0.02 * Double(ink)
    var art = "composite (ours):\n"
    for row in mine { art += String(row.map { $0 ? "#" : " " }) + "\n" }
    art += "reference (CTLine):\n"
    for row in theirs { art += String(row.map { $0 ? "#" : " " }) + "\n" }
    return (pass, "unmatched=\(unmatched) ink=\(ink)\n" + art)
}

// كتب = kaf, teh, beh -> visual (left to right): beh final, teh medial, kaf initial
let word: [UnicodeScalar] = [UnicodeScalar(0x0643)!, UnicodeScalar(0x062A)!, UnicodeScalar(0x0628)!]
let test = selfTest(word: word, forms: [0xFE90, 0xFE98, 0xFEDB])

// MARK: - Pack blob + emit header

var blob: [UInt8] = []
struct TableRow { let cp: Int; let offset: Int; let g: RenderedGlyph }
var table: [TableRow] = []
for g in rendered {
    let offset = blob.count
    let rowBytes = (g.width + 7) / 8
    for row in g.rows {
        var bytes = [UInt8](repeating: 0, count: rowBytes)
        for (x, on) in row.enumerated() where on {
            bytes[x / 8] |= UInt8(0x80 >> (x % 8))
        }
        blob += bytes
    }
    table.append(TableRow(cp: g.codepoint, offset: offset, g: g))
}
guard blob.count < 65536 else { fail("bitmap blob is \(blob.count) bytes; exceeds uint16 offsets") }

var header = """
// Generated by tools/GenerateArabicFont.swift — do not edit.
// Source: \(fontURL.lastPathComponent) at \(size)px, wght=\(Int(weight)).
//
// Bitmap data derived from Noto Naskh Arabic, © Google, licensed under
// SIL OFL 1.1 — see fonts/noto-naskh-arabic/OFL.txt.

#include "../ArabicFont.h"

static const uint8_t \(varName)Bitmaps[] = {

"""
for start in stride(from: 0, to: blob.count, by: 16) {
    let chunk = blob[start..<min(start + 16, blob.count)]
    header += "    " + chunk.map { String(format: "0x%02X,", $0) }.joined(separator: " ") + "\n"
}
header += """
};

static const ArabicGlyph \(varName)Glyphs[] = {

"""
for row in table {
    header += String(format: "    {0x%04X, %5d, %3d, %3d, %4d, %4d, %3d},\n",
                     row.cp, row.offset, row.g.width, row.g.height,
                     row.g.xOffset, row.g.yOffset, row.g.advance)
}
header += """
};

static const ArabicFont \(varName) = {
    \(lineHeight),  // lineHeight
    \(baseline),  // baseline
    \(table.count),
    \(varName)Glyphs,
    \(varName)Bitmaps,
};

"""

let outURL = URL(fileURLWithPath: outDir)
try? FileManager.default.createDirectory(at: outURL, withIntermediateDirectories: true)
let headerURL = outURL.appendingPathComponent("\(name)Font.h")
try! header.write(to: headerURL, atomically: true, encoding: .utf8)

// MARK: - Preview sheet

var preview = "font: \(name)  size: \(size)px  weight: \(Int(weight))\n"
preview += "lineHeight: \(lineHeight)  baseline: \(baseline)  glyphs: \(table.count)  blob: \(blob.count) bytes\n\n"
for g in rendered {
    preview += String(format: "U+%04X  w=%d h=%d xOff=%d yOff=%d adv=%d\n",
                      g.codepoint, g.width, g.height, g.xOffset, g.yOffset, g.advance)
    for row in g.rows { preview += String(row.map { $0 ? "#" : " " }) + "\n" }
    preview += "\n"
}
preview += "self-test (كتب composite vs CTLine): \(test.pass ? "PASS" : "FAIL")\n"
preview += test.detail
try! preview.write(to: outURL.appendingPathComponent("preview.txt"), atomically: true, encoding: .utf8)

print("wrote \(headerURL.path)")
print("glyphs: \(table.count), blob: \(blob.count) bytes, lineHeight: \(lineHeight), baseline: \(baseline)")
print("self-test (كتب composite vs CTLine): \(test.pass ? "PASS" : "FAIL") — \(test.detail.split(separator: "\n").first!)")
if !test.pass { exit(1) }
