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

/// `frame` (Cocoa coordinates) in CoreGraphics coordinates, whose origin is
/// the top left corner of the primary screen
CGRect toCGRect(NSRect frame)
{
    const CGFloat primaryHeight = NSScreen.screens.firstObject.frame.size.height;
    return CGRectMake(frame.origin.x, primaryHeight - NSMaxY(frame),
                      frame.size.width, frame.size.height);
}

/// Calls `visit` with the CoreGraphics info of every visible, regular window of
/// other apps over `area` (CoreGraphics coordinates), front to back, until it
/// returns NO
void forEachWindowOver(CGRect area, BOOL (^visit)(NSDictionary *))
{
    NSArray *windows = CFBridgingRelease(CGWindowListCopyWindowInfo(
        kCGWindowListOptionOnScreenOnly | kCGWindowListExcludeDesktopElements,
        kCGNullWindowID));
    const int ownPid = NSProcessInfo.processInfo.processIdentifier;

    for (NSDictionary *info in windows)
    {
        if ([info[(__bridge id)kCGWindowOwnerPID] intValue] == ownPid ||
            [info[(__bridge id)kCGWindowLayer] intValue] != 0 ||
            [info[(__bridge id)kCGWindowAlpha] doubleValue] <= 0)
        {
            continue;
        }
        CGRect bounds;
        if (CGRectMakeWithDictionaryRepresentation(
                (__bridge CFDictionaryRef)info[(__bridge id)kCGWindowBounds],
                &bounds) &&
            CGRectIntersectsRect(bounds, area) &&
            !visit(info))
        {
            return;
        }
    }
}

NSInteger windowNumberOf(NSDictionary *info)
{
    return [info[(__bridge id)kCGWindowNumber] integerValue];
}

/// Number of the frontmost window of another app under the middle of
/// `overlay`, or 0
NSInteger windowNumberUnder(NSWindow *overlay)
{
    const CGRect frame = toCGRect(overlay.frame);
    const CGPoint middle = CGPointMake(CGRectGetMidX(frame), CGRectGetMidY(frame));

    __block NSInteger found = 0;
    forEachWindowOver(CGRectMake(middle.x, middle.y, 1, 1),
                      ^BOOL(NSDictionary *info) {
                        found = windowNumberOf(info);
                        return NO;
                      });
    return found;
}

NSWindow *windowOf(WId winId)
{
    return ((__bridge NSView *)reinterpret_cast<void *>(winId)).window;
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

qint64 getMacOSWindowUnder(WId overlayWinId)
{
    NSWindow *overlay = windowOf(overlayWinId);
    return overlay == nil ? 0 : windowNumberUnder(overlay);
}

QString getMacOSOverlayCover(WId overlayWinId, qint64 window)
{
    NSWindow *overlay = windowOf(overlayWinId);
    if (overlay == nil)
    {
        return QStringLiteral("unknown, the overlay has no window");
    }

    // The frontmost window over the overlay is either `window` or its cover
    __block QString cover =
        QStringLiteral("nothing, the browser window is gone");
    forEachWindowOver(toCGRect(overlay.frame), ^BOOL(NSDictionary *info) {
      if (windowNumberOf(info) == window)
      {
          cover.clear();
      }
      else
      {
          cover = QString::fromNSString([NSString
              stringWithFormat:@"%@ (pid %@, window %@)",
                               info[(__bridge id)kCGWindowOwnerName],
                               info[(__bridge id)kCGWindowOwnerPID],
                               info[(__bridge id)kCGWindowNumber]]);
      }
      return NO;
    });
    return cover;
}

void onMacOSAppActivated(std::function<void(qint64 pid)> callback)
{
    [NSWorkspace.sharedWorkspace.notificationCenter
        addObserverForName:NSWorkspaceDidActivateApplicationNotification
                    object:nil
                     queue:NSOperationQueue.mainQueue
                usingBlock:^(NSNotification *notification) {
                  NSRunningApplication *app =
                      notification.userInfo[NSWorkspaceApplicationKey];
                  callback(app.processIdentifier);
                }];
}
#endif

}  // namespace chatterino
