import AppKit
import CArabicRuntime

let repoRoot = URL(fileURLWithPath: #filePath)
    .deletingLastPathComponent()  // ArabicDemo
    .deletingLastPathComponent()  // Sources
    .deletingLastPathComponent()  // demo
    .deletingLastPathComponent()  // repo

let margin = 4

struct GrayBitmap {
    var width: Int
    var height: Int
    var pixels: [UInt8]  // 0 = background, 255 = ink

    func cgImage() -> CGImage? {
        guard width > 0, height > 0 else { return nil }
        let provider = CGDataProvider(data: Data(pixels) as CFData)!
        return CGImage(width: width, height: height, bitsPerComponent: 8, bitsPerPixel: 8,
                       bytesPerRow: width, space: CGColorSpaceCreateDeviceGray(),
                       bitmapInfo: CGBitmapInfo(rawValue: CGImageAlphaInfo.none.rawValue),
                       provider: provider, decode: nil, shouldInterpolate: false,
                       intent: .defaultIntent)
    }
}

final class Canvas {
    var bitmap: GrayBitmap
    init(width: Int, height: Int) {
        bitmap = GrayBitmap(width: width, height: height,
                            pixels: [UInt8](repeating: 0, count: width * height))
    }
}

func renderBitmapPipeline(_ text: String) -> (GrayBitmap, glyphCount: Int) {
    let font = demoArabicFont()!
    var shaped = [UInt16](repeating: 0, count: 512)
    let count = text.withCString { arabicShape($0, &shaped, 512, 0) }
    let width = Int(arabicMeasure(font, shaped, count)) + 2 * margin
    let height = Int(font.pointee.lineHeight)
    let canvas = Canvas(width: width, height: height)
    let ctx = Unmanaged.passUnretained(canvas).toOpaque()
    arabicRenderAll(font, shaped, count, Int16(margin), 0, { x, y, ctx in
        let c = Unmanaged<Canvas>.fromOpaque(ctx!).takeUnretainedValue()
        guard x >= 0, y >= 0, Int(x) < c.bitmap.width, Int(y) < c.bitmap.height else { return }
        c.bitmap.pixels[Int(y) * c.bitmap.width + Int(x)] = 255
    }, ctx)
    return (canvas.bitmap, count)
}

let vectorFont: CTFont = {
    let url = repoRoot.appendingPathComponent(
        "fonts/noto-naskh-arabic/NotoNaskhArabic-VariableFont_wght.ttf")
    guard let descriptors = CTFontManagerCreateFontDescriptorsFromURL(url as CFURL)
            as? [CTFontDescriptor], let first = descriptors.first else {
        fatalError("Cannot load font at \(url.path)")
    }
    return CTFontCreateWithFontDescriptor(first, 28, nil)
}()

func renderCoreTextPipeline(_ text: String) -> GrayBitmap {
    let font = demoArabicFont()!
    let height = Int(font.pointee.lineHeight)
    let baseline = Int(font.pointee.baseline)
    let attr = NSAttributedString(string: text, attributes: [
        kCTFontAttributeName as NSAttributedString.Key: vectorFont,
        kCTForegroundColorAttributeName as NSAttributedString.Key: CGColor.white,
    ])
    let line = CTLineCreateWithAttributedString(attr)
    let lineWidth = CTLineGetTypographicBounds(line, nil, nil, nil)
    let width = max(1, Int(ceil(lineWidth)) + 2 * margin)
    guard let ctx = CGContext(data: nil, width: width, height: height, bitsPerComponent: 8,
                              bytesPerRow: width, space: CGColorSpaceCreateDeviceGray(),
                              bitmapInfo: CGImageAlphaInfo.none.rawValue) else {
        return GrayBitmap(width: width, height: height,
                          pixels: [UInt8](repeating: 0, count: width * height))
    }
    ctx.setAllowsAntialiasing(true)
    ctx.textPosition = CGPoint(x: CGFloat(margin), y: CGFloat(height - baseline))
    CTLineDraw(line, ctx)
    var pixels = [UInt8](repeating: 0, count: width * height)
    if let data = ctx.data {
        let rowBytes = ctx.bytesPerRow
        let src = data.assumingMemoryBound(to: UInt8.self)
        for row in 0..<height {
            for col in 0..<width {
                pixels[row * width + col] = src[row * rowBytes + col]
            }
        }
    }
    return GrayBitmap(width: width, height: height, pixels: pixels)
}

final class PanelsView: NSView {
    var bitmapImage: GrayBitmap?
    var vectorImage: GrayBitmap?
    var zoom: CGFloat = 8
    var showGrid = true

    override var isFlipped: Bool { true }

    private let pad: CGFloat = 12
    private let labelHeight: CGFloat = 20
    private let gap: CGFloat = 24

    func contentSize() -> NSSize {
        let w = CGFloat(max(bitmapImage?.width ?? 0, vectorImage?.width ?? 0)) * zoom
        let h = CGFloat((bitmapImage?.height ?? 0) + (vectorImage?.height ?? 0)) * zoom
        return NSSize(width: w + 2 * pad, height: h + 2 * (labelHeight + 6) + gap + 2 * pad)
    }

    override func draw(_ dirtyRect: NSRect) {
        NSColor(calibratedWhite: 0.12, alpha: 1).setFill()
        bounds.fill()
        var y = pad
        y = drawPanel(label: "Bitmap (generated)", image: bitmapImage, atY: y)
        y += gap
        _ = drawPanel(label: "CoreText (vector, 28px)", image: vectorImage, atY: y)
    }

    private func drawPanel(label: String, image: GrayBitmap?, atY y: CGFloat) -> CGFloat {
        let attrs: [NSAttributedString.Key: Any] = [
            .font: NSFont.systemFont(ofSize: 13, weight: .semibold),
            .foregroundColor: NSColor.lightGray,
        ]
        (label as NSString).draw(at: NSPoint(x: pad, y: y), withAttributes: attrs)
        var imageTop = y + labelHeight + 6
        guard let image, let cg = image.cgImage() else { return imageTop }
        let rect = NSRect(x: pad, y: imageTop,
                          width: CGFloat(image.width) * zoom, height: CGFloat(image.height) * zoom)

        NSGraphicsContext.current?.imageInterpolation = .none
        let nsImage = NSImage(cgImage: cg, size: NSSize(width: image.width, height: image.height))
        nsImage.draw(in: rect, from: .zero, operation: .sourceOver, fraction: 1,
                     respectFlipped: true, hints: [.interpolation: NSImageInterpolation.none.rawValue])

        if showGrid && zoom >= 8 {
            NSColor(calibratedWhite: 0.5, alpha: 0.25).setStroke()
            let path = NSBezierPath()
            path.lineWidth = 1
            for col in 0...image.width {
                let x = rect.minX + CGFloat(col) * zoom
                path.move(to: NSPoint(x: x, y: rect.minY))
                path.line(to: NSPoint(x: x, y: rect.maxY))
            }
            for row in 0...image.height {
                let yy = rect.minY + CGFloat(row) * zoom
                path.move(to: NSPoint(x: rect.minX, y: yy))
                path.line(to: NSPoint(x: rect.maxX, y: yy))
            }
            path.stroke()
        }

        // Baseline guide at font->baseline rows from the line-box top.
        let baseline = CGFloat(demoArabicFont()!.pointee.baseline)
        NSColor.systemRed.setStroke()
        let guide = NSBezierPath()
        guide.lineWidth = 1
        let by = rect.minY + baseline * zoom + 0.5
        guide.move(to: NSPoint(x: rect.minX, y: by))
        guide.line(to: NSPoint(x: rect.maxX, y: by))
        guide.stroke()

        imageTop += rect.height
        return imageTop
    }
}

final class AppDelegate: NSObject, NSApplicationDelegate, NSTextFieldDelegate {
    let window = NSWindow(contentRect: NSRect(x: 0, y: 0, width: 1100, height: 700),
                          styleMask: [.titled, .closable, .miniaturizable, .resizable],
                          backing: .buffered, defer: false)
    let textField = NSTextField(
        string: UserDefaults.standard.string(forKey: "lastTestString") ?? "مرحبا بالعالم")
    let slider = NSSlider(value: 8, minValue: 4, maxValue: 16, target: nil, action: nil)
    let gridCheckbox = NSButton(checkboxWithTitle: "pixel grid", target: nil, action: nil)
    let panels = PanelsView()
    let scrollView = NSScrollView()

    func applicationDidFinishLaunching(_ notification: Notification) {
        window.title = "Arabic Bitmap vs CoreText"
        window.center()

        textField.delegate = self
        textField.alignment = .right
        textField.baseWritingDirection = .rightToLeft
        textField.font = NSFont.systemFont(ofSize: 18)

        slider.target = self
        slider.action = #selector(controlsChanged)
        slider.allowsTickMarkValuesOnly = false

        gridCheckbox.target = self
        gridCheckbox.action = #selector(controlsChanged)
        gridCheckbox.state = .on

        let zoomLabel = NSTextField(labelWithString: "Zoom")
        let controls = NSStackView(views: [zoomLabel, slider, gridCheckbox])
        controls.orientation = .horizontal
        controls.spacing = 8

        scrollView.documentView = panels
        scrollView.hasHorizontalScroller = true
        scrollView.hasVerticalScroller = true
        scrollView.borderType = .bezelBorder

        let content = window.contentView!
        for v in [textField, controls, scrollView] {
            v.translatesAutoresizingMaskIntoConstraints = false
            content.addSubview(v)
        }
        NSLayoutConstraint.activate([
            textField.topAnchor.constraint(equalTo: content.topAnchor, constant: 12),
            textField.leadingAnchor.constraint(equalTo: content.leadingAnchor, constant: 12),
            textField.trailingAnchor.constraint(equalTo: content.trailingAnchor, constant: -12),
            controls.topAnchor.constraint(equalTo: textField.bottomAnchor, constant: 10),
            controls.leadingAnchor.constraint(equalTo: content.leadingAnchor, constant: 12),
            slider.widthAnchor.constraint(equalToConstant: 220),
            scrollView.topAnchor.constraint(equalTo: controls.bottomAnchor, constant: 10),
            scrollView.leadingAnchor.constraint(equalTo: content.leadingAnchor, constant: 12),
            scrollView.trailingAnchor.constraint(equalTo: content.trailingAnchor, constant: -12),
            scrollView.bottomAnchor.constraint(equalTo: content.bottomAnchor, constant: -12),
        ])

        rerender()
        window.minSize = NSSize(width: 600, height: 400)
        window.setContentSize(NSSize(width: 1240, height: 960))
        window.center()
        window.makeKeyAndOrderFront(nil)
        NSApp.activate(ignoringOtherApps: true)

        if let secs = ProcessInfo.processInfo.environment["ARABIC_DEMO_AUTOEXIT"],
           let t = Double(secs) {
            DispatchQueue.main.asyncAfter(deadline: .now() + t) { NSApp.terminate(nil) }
        }
    }

    func controlTextDidChange(_ obj: Notification) { rerender() }
    @objc func controlsChanged() { rerender() }

    func rerender() {
        let text = textField.stringValue
        UserDefaults.standard.set(text, forKey: "lastTestString")
        let (bitmap, _) = renderBitmapPipeline(text)
        panels.bitmapImage = bitmap
        panels.vectorImage = renderCoreTextPipeline(text)
        panels.zoom = CGFloat(slider.doubleValue)
        panels.showGrid = gridCheckbox.state == .on
        panels.setFrameSize(panels.contentSize())
        panels.needsDisplay = true
    }

    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool { true }
}

func selfCheck() {
    let text = "مرحبا بالعالم"
    let (bitmap, glyphCount) = renderBitmapPipeline(text)
    let vector = renderCoreTextPipeline(text)
    print("self-check: shaped glyphs=\(glyphCount) " +
          "bitmap=\(bitmap.width)x\(bitmap.height) " +
          "coretext=\(vector.width)x\(vector.height)")
}

selfCheck()

let app = NSApplication.shared
app.setActivationPolicy(.regular)
let delegate = AppDelegate()
app.delegate = delegate
app.run()
