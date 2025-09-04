// macwin.m — build this with clang as Objective-C and link with -framework Cocoa
#import <Cocoa/Cocoa.h>
#import "../sunburst.h"

@interface MacWinDelegate : NSObject <NSApplicationDelegate, NSWindowDelegate>
@property(nonatomic, assign) BOOL shouldKeepRunning;
@end

@implementation MacWinDelegate
- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    self.shouldKeepRunning = YES;
}
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
    return YES;
}
- (void)windowWillClose:(NSNotification *)notification {
    // If last window closes, let app terminate
}
@end

struct macwin_App {
    NSApplication *nsapp;
    MacWinDelegate *delegate;
};

struct macwin_Window {
    NSWindow *nswin;
};

static inline NSString* mw_NSStringFromUTF8(const char* s) {
    if (!s) return @"";
    return [NSString stringWithUTF8String:s];
}

macwin_App* macwin_app_create(void) {
    @autoreleasepool {
        // Cocoa must run on main thread
        if (![NSThread isMainThread]) {
            fprintf(stderr, "macwin_app_create must be called on the main thread.\n");
            return NULL;
        }
        macwin_App* app = (macwin_App*)calloc(1, sizeof(*app));
        app->nsapp = [NSApplication sharedApplication];

        // Show in Dock / app switcher
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

        app->delegate = [MacWinDelegate new];
        [NSApp setDelegate:app->delegate];

        // Create a menu bar so Cmd+Q etc. work nicely (minimal)
        id menubar = [[NSMenu new] autorelease];
        id appMenuItem = [[NSMenuItem new] autorelease];
        [menubar addItem:appMenuItem];
        [NSApp setMainMenu:menubar];

        id appMenu = [[NSMenu new] autorelease];
        id quitTitle = [@"Quit" stringByAppendingString:
                        [@" " stringByAppendingString:
                         [[NSProcessInfo processInfo] processName]]];
        id quitItem = [[[NSMenuItem alloc] initWithTitle:quitTitle
                                                  action:@selector(terminate:)
                                           keyEquivalent:@"q"] autorelease];
        [appMenu addItem:quitItem];
        [appMenuItem setSubmenu:appMenu];

        [NSApp finishLaunching];
        [NSApp activateIgnoringOtherApps:YES];
        return app;
    }
}

macwin_Window* macwin_window_create(int width, int height, const char* title, int resizable) {
    @autoreleasepool {
        NSRect rect = NSMakeRect(0, 0, width, height);
        NSUInteger style = (NSWindowStyleMaskTitled |
                            NSWindowStyleMaskClosable |
                            NSWindowStyleMaskMiniaturizable);
        if (resizable) style |= NSWindowStyleMaskResizable;

        NSWindow* w = [[NSWindow alloc] initWithContentRect:rect
                                                  styleMask:style
                                                    backing:NSBackingStoreBuffered
                                                      defer:NO];
        [w center];
        [w setTitle:mw_NSStringFromUTF8(title)];
        [w setReleasedWhenClosed:NO];

        macwin_Window* wrap = (macwin_Window*)calloc(1, sizeof(*wrap));
        wrap->nswin = w;
        return wrap;
    }
}

void macwin_window_show(macwin_Window* win) {
    if (!win) return;
    @autoreleasepool {
        [win->nswin makeKeyAndOrderFront:nil];
        [NSApp activateIgnoringOtherApps:YES];
    }
}

void macwin_window_set_title(macwin_Window* win, const char* title) {
    if (!win) return;
    @autoreleasepool {
        [win->nswin setTitle:mw_NSStringFromUTF8(title)];
    }
}

void macwin_window_destroy(macwin_Window* win) {
    if (!win) return;
    @autoreleasepool {
        [win->nswin close];
        [win->nswin release];
        free(win);
    }
}

void macwin_app_run(macwin_App* app) {
    if (!app) return;
    if (![NSThread isMainThread]) {
        fprintf(stderr, "macwin_app_run must be called on the main thread.\n");
        return;
    }
    [NSApp run]; // blocks until terminate:
}

int macwin_app_poll(macwin_App* app) {
    if (!app) return 0;
    @autoreleasepool {
        NSEvent* event;
        // Pump at most one event; use a tiny timeout
        event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                   untilDate:[NSDate dateWithTimeIntervalSinceNow:0.001]
                                      inMode:NSDefaultRunLoopMode
                                     dequeue:YES];
        if (event) [NSApp sendEvent:event];
        [NSApp updateWindows];

        // If there are no windows and policy is to terminate after last window, allow quit
        if ([[NSApp windows] count] == 0) return 0;
        return 1;
    }
}

void macwin_app_quit(macwin_App* app) {
    if (!app) return;
    dispatch_async(dispatch_get_main_queue(), ^{
        [NSApp terminate:nil];
    });
}
