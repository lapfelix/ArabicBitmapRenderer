// Converts a 1x PGM (P5) into a zoomed PNG: black ink on transparent
// background with a faint pixel grid.
// Usage: swift tools/ComposePng.swift <in.pgm> <out.png> <scale>
import Foundation
import CoreGraphics
import ImageIO
import UniformTypeIdentifiers

let args = CommandLine.arguments
guard args.count == 4, let scale = Int(args[3]), scale >= 1 else {
    fputs("usage: ComposePng.swift <in.pgm> <out.png> <scale>\n", stderr)
    exit(1)
}
let data = try Data(contentsOf: URL(fileURLWithPath: args[1]))
let header = String(decoding: data.prefix(32), as: UTF8.self)
let dims = header.split(separator: "\n")[1].split(separator: " ").map { Int($0)! }
let (w, h) = (dims[0], dims[1])
let px = [UInt8](data[(data.count - w * h)...])

let W = w * scale, H = h * scale
let ctx = CGContext(data: nil, width: W, height: H, bitsPerComponent: 8, bytesPerRow: 0,
                    space: CGColorSpaceCreateDeviceRGB(),
                    bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue)!
ctx.setFillColor(CGColor(red: 0, green: 0, blue: 0, alpha: 1))
for y in 0..<h {
    for x in 0..<w where px[y * w + x] > 128 {
        ctx.fill(CGRect(x: x * scale, y: (h - 1 - y) * scale, width: scale, height: scale))
    }
}
if scale >= 6 {
    ctx.setStrokeColor(CGColor(red: 0.5, green: 0.5, blue: 0.5, alpha: 0.25))
    ctx.setLineWidth(1)
    for x in 0...w { ctx.move(to: CGPoint(x: x * scale, y: 0)); ctx.addLine(to: CGPoint(x: x * scale, y: H)) }
    for y in 0...h { ctx.move(to: CGPoint(x: 0, y: y * scale)); ctx.addLine(to: CGPoint(x: W, y: y * scale)) }
    ctx.strokePath()
}
let dest = CGImageDestinationCreateWithURL(URL(fileURLWithPath: args[2]) as CFURL,
                                           UTType.png.identifier as CFString, 1, nil)!
CGImageDestinationAddImage(dest, ctx.makeImage()!, nil)
guard CGImageDestinationFinalize(dest) else { exit(1) }
print("wrote \(args[2]) (\(W)x\(H))")
