#include "util/MacOsHelpers.h"

#include <AppKit/AppKit.h>
#include <QUrl>

void chatterinoSetMacOsActivationPolicyProhibited()
{
    [[NSApplication sharedApplication] setActivationPolicy:NSApplicationActivationPolicyProhibited];
}


namespace chatterino {

#ifdef Q_OS_DARWIN
QString getMacOSDefaultBrowserPath()
{
    @autoreleasepool {
        NSURL *httpUrl = [NSURL URLWithString:@"https://"];
        NSURL *appUrl = [[NSWorkspace sharedWorkspace] URLForApplicationToOpenURL:httpUrl];
        if (appUrl == nil)
        {
            return {};
        }

        NSBundle *bundle = [NSBundle bundleWithURL:appUrl];
        NSURL *execUrl = bundle.executableURL;
        if (execUrl == nil)
        {
            return {};
        }

        return QUrl::fromNSURL(execUrl).toLocalFile();    }
}

namespace {

/// Number of the frontmost window of another app under the middle of
/// `overlay`, or 0
NSInteger windowNumberUnder(NSWindow *overlay)
{
    NSArray *windows = CFBridgingRelease(CGWindowListCopyWindowInfo(
        kCGWindowListOptionOnScreenOnly | kCGWindowListExcludeDesktopElements,
        kCGNullWindowID));

    // CoreGraphics' origin is the top left corner of the primary screen
    const CGFloat primaryHeight = NSScreen.screens.firstObject.frame.size.height;
    const NSRect frame = overlay.frame;
    const CGPoint middle =
        CGPointMake(NSMidX(frame), primaryHeight - NSMidY(frame));
    const int ownPid = NSProcessInfo.processInfo.processIdentifier;

    // Front to back
    for (NSDictionary *info in windows)
    {
        if ([info[(__bridge id)kCGWindowOwnerPID] intValue] == ownPid ||
            [info[(__bridge id)kCGWindowLayer] intValue] != 0)
        {
            continue;
        }
        CGRect bounds;
        if (CGRectMakeWithDictionaryRepresentation(
                (__bridge CFDictionaryRef)info[(__bridge id)kCGWindowBounds],
                &bounds) &&
            CGRectContainsPoint(bounds, middle))
        {
            return [info[(__bridge id)kCGWindowNumber] integerValue];
        }
    }
    return 0;
}

}  // namespace

void keepMacOSWindowsBehindOverlay(WId overlayWinId)
{
    NSView *view = (__bridge NSView *)reinterpret_cast<void *>(overlayWinId);

    // Qt reorders the windows when the app is about to become active, so this
    // runs after it
    [NSNotificationCenter.defaultCenter
        addObserverForName:NSApplicationDidBecomeActiveNotification
                    object:nil
                     queue:NSOperationQueue.mainQueue
                usingBlock:^(NSNotification *) {
                  NSWindow *overlay = view.window;
                  // Only when the overlay was clicked
                  if (overlay == nil || !overlay.visible ||
                      !NSMouseInRect(NSEvent.mouseLocation, overlay.frame, NO))
                  {
                      return;
                  }
                  const NSInteger browser = windowNumberUnder(overlay);
                  if (browser == 0)
                  {
                      return;
                  }
                  for (NSWindow *window in NSApp.orderedWindows)
                  {
                      if (window != overlay && window.visible &&
                          window.level == NSNormalWindowLevel)
                      {
                          [window orderWindow:NSWindowBelow relativeTo:browser];
                      }
                  }
                }];
}
#endif

}  // namespace chatterino