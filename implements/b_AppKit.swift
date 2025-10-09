import AppKit
import OpenGL.GL3   // GL 3.2 Core on macOS (max is 4.1 core, but 3.2 is widely supported)

final class GWinDelegate: NSObject, NSWindowDelegate {
    var shouldClose = false
    func windowWillClose(_ notification: Notification) { shouldClose = true }
}

// NEW: keep refs to GL bits
private var glView: GLView?
private var glContext: NSOpenGLContext?

final class GLView: NSOpenGLView {
    // Create a 3.2 core profile, double-buffered, 24-bit depth
    static func makePixelFormat() -> NSOpenGLPixelFormat? {
        let attrs: [NSOpenGLPixelFormatAttribute] = [
            UInt32(NSOpenGLPFAAccelerated),
            UInt32(NSOpenGLPFAOpenGLProfile), UInt32(NSOpenGLProfileVersion3_2Core),
            UInt32(NSOpenGLPFADoubleBuffer),
            UInt32(NSOpenGLPFAColorSize), 24,
            UInt32(NSOpenGLPFADepthSize), 24,
            0
        ]
        return NSOpenGLPixelFormat(attributes: attrs)
    }

    override func prepareOpenGL() {
        super.prepareOpenGL()
        // Enable HiDPI rendering
        self.wantsBestResolutionOpenGLSurface = true
        // Default to vsync on (swap interval 1)
        var one: GLint = 1
        self.openGLContext?.setValues(&one, for: .swapInterval)
    }

    // Keep viewport in sync if you want it automatic; otherwise you can set it each frame.
    override func reshape() {
        super.reshape()
        guard let ctx = self.openGLContext else { return }
        ctx.makeCurrentContext()
        let scale = window?.backingScaleFactor ?? 1.0
        let pixelSize = CGSize(width: bounds.size.width * scale,
                               height: bounds.size.height * scale)
        glViewport(0, 0, GLsizei(pixelSize.width), GLsizei(pixelSize.height))
    }

    func setSwapInterval(_ interval: Int32) {
        var v = GLint(interval)
        self.openGLContext?.setValues(&v, for: .swapInterval)
    }

    func swapBuffers() {
        self.openGLContext?.flushBuffer()
    }
}

private var appInitialized = false
private var window: NSWindow?
private var delegateRef = GWinDelegate()

@inline(__always)
private func ensureApp() {
    if !appInitialized {
        let app = NSApplication.shared
        app.setActivationPolicy(.regular)
        let mainMenu = NSMenu(); let appMenuItem = NSMenuItem(); mainMenu.addItem(appMenuItem)
        let appMenu = NSMenu()
        appMenu.addItem(withTitle: "Quit", action: #selector(NSApplication.terminate(_:)), keyEquivalent: "q")
        appMenuItem.submenu = appMenu
        app.mainMenu = mainMenu
        app.finishLaunching()
        app.activate(ignoringOtherApps: true)
        appInitialized = true
    }
}

@_cdecl("InitWindow")
public func InitWindow(_ width: Int32, _ height: Int32, _ title: UnsafePointer<CChar>?) -> Bool {
    ensureApp()

    // If window exists, just retitle/resize
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

    // No autoresizing mask needed if it's the contentView:
    win.contentView = view

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

@_cdecl("DestroyWindow")
public func DestroyWindow() {
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

// --- NEW: GL helpers ---
@_cdecl("GL_SwapBuffers")
public func GL_SwapBuffers() {
    glView?.swapBuffers()
}

@_cdecl("GL_SetSwapInterval")
public func GL_SetSwapInterval(_ interval: Int32) {
    glView?.setSwapInterval(interval)
}
