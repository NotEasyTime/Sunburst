import AppKit
import OpenGL.GL3
import Carbon.HIToolbox   // kVK_* keycodes

// -----------------------------------------------------------------------------
// Mirror your C enum values as Int32 for Swift-side math (must match header)
// -----------------------------------------------------------------------------
enum K { // Key
    static let UNKNOWN: Int32 = 0
    static let A: Int32 = 1            // A..Z  => 1..26
    static let ZERO: Int32 = 27        // 0..9  => 27..36
    static let SPACE: Int32 = 37
    static let ENTER: Int32 = 38
    static let ESCAPE: Int32 = 39
    static let BACKSPACE: Int32 = 40
    static let TAB: Int32 = 41
    static let LEFT: Int32 = 42
    static let RIGHT: Int32 = 43
    static let UP: Int32 = 44
    static let DOWN: Int32 = 45
    static let SHIFT: Int32 = 46
    static let CTRL: Int32  = 47
    static let ALT: Int32   = 48
    static let SUPER: Int32 = 49
    static let PAGEUP: Int32   = 50
    static let PAGEDOWN: Int32 = 51
    static let HOME: Int32     = 52
    static let END: Int32      = 53
    static let INSERT: Int32   = 54
    static let DELETE: Int32   = 55
}

enum MB { // MouseButton
    static let LEFT: Int32 = 0
    static let RIGHT: Int32 = 1
    static let MIDDLE: Int32 = 2
    static let X1: Int32 = 3
    static let X2: Int32 = 4
}

// C functions are imported via bridging header (no @_silgen_name needed):
// void SB_InputSetKey(int k, int down);
// void SB_InputSetMouse(int b, int down);
// void SB_InputSetMousePos(int x, int y);
// void SB_InputAddWheel(int delta);
// void SB_InputPushUTF32(unsigned cp);

// -----------------------------------------------------------------------------
// Window delegate / globals
// -----------------------------------------------------------------------------
final class GWinDelegate: NSObject, NSWindowDelegate {
    var shouldClose = false
    func windowWillClose(_ notification: Notification) { shouldClose = true }
}

private var glView: GLView?
private var glContext: NSOpenGLContext?
private var appInitialized = false
private var window: NSWindow?
private var delegateRef = GWinDelegate()

// -----------------------------------------------------------------------------
// GL view with input forwarding
// -----------------------------------------------------------------------------
final class GLView: NSOpenGLView {

    private var tracking: NSTrackingArea?

    static func makePixelFormat() -> NSOpenGLPixelFormat? {
        let attrs: [NSOpenGLPixelFormatAttribute] = [
            UInt32(NSOpenGLPFAAccelerated),
            UInt32(NSOpenGLPFAOpenGLProfile), UInt32(NSOpenGLProfileVersion4_1Core),
            UInt32(NSOpenGLPFADoubleBuffer),
            UInt32(NSOpenGLPFAColorSize), 24,
            UInt32(NSOpenGLPFADepthSize), 24,
            0
        ]
        return NSOpenGLPixelFormat(attributes: attrs)
    }

    override var acceptsFirstResponder: Bool { true }
    override func viewDidMoveToWindow() {
        super.viewDidMoveToWindow()
        window?.makeFirstResponder(self)
        updateTrackingAreas()
    }

    override func updateTrackingAreas() {
        super.updateTrackingAreas()
        if let tr = tracking { removeTrackingArea(tr) }
        let tr = NSTrackingArea(rect: bounds,
                                options: [.mouseMoved, .activeInActiveApp, .inVisibleRect, .enabledDuringMouseDrag],
                                owner: self, userInfo: nil)
        addTrackingArea(tr)
        tracking = tr
    }

    override func prepareOpenGL() {
        super.prepareOpenGL()
        wantsBestResolutionOpenGLSurface = true
        var one: GLint = 1
        openGLContext?.setValues(&one, for: .swapInterval)
    }

    override func reshape() {
        super.reshape()
        guard let ctx = openGLContext else { return }
        ctx.makeCurrentContext()
        let scale = window?.backingScaleFactor ?? 1.0
        let pixelW = GLsizei(bounds.size.width * scale)
        let pixelH = GLsizei(bounds.size.height * scale)
        glViewport(0, 0, pixelW, pixelH)
    }

    func setSwapInterval(_ interval: Int32) {
        var v = GLint(interval)
        openGLContext?.setValues(&v, for: .swapInterval)
    }

    func swapBuffers() {
        openGLContext?.flushBuffer()
    }

    // ---------------- Input mapping (Swift → C Int32) ----------------
    private func mapKeyInt32(_ e: NSEvent) -> Int32 {
        if let s = e.charactersIgnoringModifiers?.uppercased(),
           s.count == 1, let u = s.unicodeScalars.first {
            let v = u.value // UInt32
            if v >= 65 && v <= 90 { return K.A + Int32(v - 65) }     // A..Z
            if v >= 48 && v <= 57 { return K.ZERO + Int32(v - 48) }  // 0..9
            if v == 32 { return K.SPACE }
        }
        switch e.keyCode {
        case UInt16(kVK_Escape):        return K.ESCAPE
        case UInt16(kVK_Return):        return K.ENTER
        case UInt16(kVK_Delete):        return K.BACKSPACE
        case UInt16(kVK_Tab):           return K.TAB
        case UInt16(kVK_LeftArrow):     return K.LEFT
        case UInt16(kVK_RightArrow):    return K.RIGHT
        case UInt16(kVK_UpArrow):       return K.UP
        case UInt16(kVK_DownArrow):     return K.DOWN
        case UInt16(kVK_Home):          return K.HOME
        case UInt16(kVK_End):           return K.END
        case UInt16(kVK_PageUp):        return K.PAGEUP
        case UInt16(kVK_PageDown):      return K.PAGEDOWN
        case UInt16(kVK_ForwardDelete): return K.DELETE
        case UInt16(kVK_Command):       return K.SUPER
        case UInt16(kVK_Control):       return K.CTRL
        case UInt16(kVK_Option):        return K.ALT
        case UInt16(kVK_Shift):         return K.SHIFT
        default:                        return K.UNKNOWN
        }
    }

    // Keyboard
    override func keyDown(with event: NSEvent) { SB_InputSetKey(mapKeyInt32(event), 1) }
    override func keyUp(with event: NSEvent)   { SB_InputSetKey(mapKeyInt32(event), 0) }
    override func flagsChanged(with event: NSEvent) {
        let f = event.modifierFlags
        SB_InputSetKey(K.SHIFT,  f.contains(.shift)   ? 1 : 0)
        SB_InputSetKey(K.CTRL,   f.contains(.control) ? 1 : 0)
        SB_InputSetKey(K.ALT,    f.contains(.option)  ? 1 : 0)
        SB_InputSetKey(K.SUPER,  f.contains(.command) ? 1 : 0)
    }

    // Text input (UTF-32)
    override func insertText(_ insertString: Any) {
        if let s = insertString as? String {
            for u in s.unicodeScalars {
                let v = u.value
                if v >= 0x20 && v != 0x7F { SB_InputPushUTF32(UInt32(v)) }
            }
        }
    }
    override func doCommand(by selector: Selector) { /* ignore */ }

    // Mouse
    private func toViewPoint(_ e: NSEvent) -> NSPoint { convert(e.locationInWindow, from: nil) }
    private func sendPos(_ e: NSEvent) {
        let p = toViewPoint(e)
        SB_InputSetMousePos(Int32(p.x), Int32(p.y))
    }

    override func mouseMoved(with event: NSEvent)            { sendPos(event) }
    override func mouseDragged(with event: NSEvent)          { sendPos(event) }
    override func rightMouseDragged(with event: NSEvent)     { sendPos(event) }
    override func otherMouseDragged(with event: NSEvent)     { sendPos(event) }

    override func mouseDown(with event: NSEvent)             { SB_InputSetMouse(MB.LEFT, 1) }
    override func mouseUp(with event: NSEvent)               { SB_InputSetMouse(MB.LEFT, 0) }
    override func rightMouseDown(with event: NSEvent)        { SB_InputSetMouse(MB.RIGHT, 1) }
    override func rightMouseUp(with event: NSEvent)          { SB_InputSetMouse(MB.RIGHT, 0) }
    override func otherMouseDown(with event: NSEvent) {
        switch event.buttonNumber {
        case 2: SB_InputSetMouse(MB.MIDDLE, 1)
        case 3: SB_InputSetMouse(MB.X1, 1)
        case 4: SB_InputSetMouse(MB.X2, 1)
        default: break
        }
    }
    override func otherMouseUp(with event: NSEvent) {
        switch event.buttonNumber {
        case 2: SB_InputSetMouse(MB.MIDDLE, 0)
        case 3: SB_InputSetMouse(MB.X1, 0)
        case 4: SB_InputSetMouse(MB.X2, 0)
        default: break
        }
    }

    override func scrollWheel(with event: NSEvent) {
        let dy = event.scrollingDeltaY
        let steps = Int((dy > 0) ? ceil(dy) : floor(dy))
        if steps != 0 { SB_InputAddWheel(Int32(steps)) }
    }
}

// -----------------------------------------------------------------------------
// App bootstrap
// -----------------------------------------------------------------------------
@inline(__always)
private func ensureApp() {
    if !appInitialized {
        let app = NSApplication.shared
        app.setActivationPolicy(.regular)

        let mainMenu = NSMenu()
        let appMenuItem = NSMenuItem()
        mainMenu.addItem(appMenuItem)
        let appMenu = NSMenu()
        appMenu.addItem(withTitle: "Quit", action: #selector(NSApplication.terminate(_:)), keyEquivalent: "q")
        appMenuItem.submenu = appMenu
        app.mainMenu = mainMenu

        app.finishLaunching()
        app.activate(ignoringOtherApps: true)
        appInitialized = true
    }
}

// -----------------------------------------------------------------------------
// C API exports
// -----------------------------------------------------------------------------
@_cdecl("InitWindow")
public func InitWindow(_ width: Int32, _ height: Int32, _ title: UnsafePointer<CChar>?) -> Bool {
    ensureApp()

    if let win = window {
        if let cstr = title { win.title = String(cString: cstr) }
        win.setContentSize(NSSize(width: Int(width), height: Int(height)))
        win.makeKeyAndOrderFront(nil)
        return true
    }

    let style: NSWindow.StyleMask = [.titled, .closable, .miniaturizable, .resizable]
    let rect = NSRect(x: 0, y: 0, width: Int(width), height: Int(height))
    let win = NSWindow(contentRect: rect, styleMask: style, backing: .buffered, defer: false)
    win.center()
    win.isReleasedWhenClosed = false
    win.delegate = delegateRef
    win.title = title != nil ? String(cString: title!) : "Window"
    win.makeKeyAndOrderFront(nil)
    window = win
    return true
}

@_cdecl("CreateGLContext")
public func CreateGLContext() -> Bool {
    guard let win = window else { return false }
    if glContext != nil { return true }

    guard let pf = GLView.makePixelFormat() else { return false }
    guard let view = GLView(frame: win.contentLayoutRect, pixelFormat: pf) else { return false }

    win.contentView = view
    win.makeFirstResponder(view)

    guard let ctx = NSOpenGLContext(format: pf, share: nil) else { return false }
    ctx.makeCurrentContext()
    view.openGLContext = ctx

    glView = view
    glContext = ctx
    return true
}

@_cdecl("PollEvents")
public func PollEvents() {
    guard appInitialized else { return }
    while let event = NSApp.nextEvent(matching: .any,
                                      until: Date.distantPast,
                                      inMode: .default,
                                      dequeue: true) {
        NSApp.sendEvent(event)
    }
}

@_cdecl("WindowShouldClose")
public func WindowShouldClose() -> Bool { delegateRef.shouldClose }

@_cdecl("CloseWindowSB")
public func CloseWindowSB() {
    if let win = window {
        win.orderOut(nil)
        win.close()
        window = nil
    }
    glView = nil
    glContext = nil
    delegateRef.shouldClose = false
}

@_cdecl("SetWindowTitle")
public func SetWindowTitle(_ title: UnsafePointer<CChar>?) {
    guard let win = window, let c = title else { return }
    win.title = String(cString: c)
}

@_cdecl("GetFramebufferSize")
public func GetFramebufferSize(_ outW: UnsafeMutablePointer<Int32>?, _ outH: UnsafeMutablePointer<Int32>?) {
    guard let win = window else { return }
    let sizePts = win.contentLayoutRect.size
    let scale = win.backingScaleFactor
    outW?.pointee = Int32(sizePts.width * scale)
    outH?.pointee = Int32(sizePts.height * scale)
}

@_cdecl("GL_SwapBuffers")
public func GL_SwapBuffers() { glView?.swapBuffers() }

@_cdecl("GL_SetSwapInterval")
public func GL_SetSwapInterval(_ interval: Int32) { glView?.setSwapInterval(interval) }
