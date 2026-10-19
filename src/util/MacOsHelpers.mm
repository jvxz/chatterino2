#include "util/MacOsHelpers.h"

#include <AppKit/AppKit.h>
#include <QImage>
#include <QUrl>

#include <vector>

/// Calls the callback at the clicked menu item's tag
@interface ChatterinoStatusItemTarget : NSObject
{
@public
    std::vector<std::function<void()>> callbacks;
}
- (void)itemClicked:(NSMenuItem *)sender;
@end

@implementation ChatterinoStatusItemTarget
- (void)itemClicked:(NSMenuItem *)sender
{
    const auto index = static_cast<size_t>(sender.tag);
    if (index < self->callbacks.size())
    {
        self->callbacks[index]();
    }
}
@end

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

void setMacOSDockIconVisible(bool visible)
{
    [NSApp setActivationPolicy:visible ? NSApplicationActivationPolicyRegular
                                       : NSApplicationActivationPolicyAccessory];
}

void activateMacOSApp()
{
    if (@available(macOS 14, *))
    {
        [NSApp activate];
    }
    else
    {
        [NSApp activateIgnoringOtherApps:YES];
    }
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

MacOSStatusItem::MacOSStatusItem(const QImage &icon, const QString &toolTip)
{
    // A square item fits the menu bar's height (22pt, 24pt with a notch)
    NSStatusItem *item = [[NSStatusBar.systemStatusBar
        statusItemWithLength:NSSquareStatusItemLength] retain];
    // Brings it back if it was removed by dragging it out of the menu bar
    item.visible = YES;

    CGImageRef cgImage = icon.toCGImage();
    NSImage *image = [[NSImage alloc] initWithCGImage:cgImage
                                                 size:NSMakeSize(18, 18)];
    CGImageRelease(cgImage);
    // Not the property syntax: `template` is a C++ keyword
    [image setTemplate:YES];
    item.button.image = image;
    [image release];
    item.button.toolTip = toolTip.toNSString();

    NSMenu *menu = [[NSMenu alloc] init];
    menu.autoenablesItems = NO;
    item.menu = menu;
    [menu release];

    this->item_ = item;
    this->target_ = [[ChatterinoStatusItemTarget alloc] init];
}

MacOSStatusItem::~MacOSStatusItem()
{
    auto *item = (NSStatusItem *)this->item_;
    [NSStatusBar.systemStatusBar removeStatusItem:item];
    [item release];
    [(ChatterinoStatusItemTarget *)this->target_ release];
}

void MacOSStatusItem::addAction(const QString &title,
                                std::function<void()> callback)
{
    auto *target = (ChatterinoStatusItemTarget *)this->target_;
    NSMenuItem *menuItem =
        [[NSMenuItem alloc] initWithTitle:title.toNSString()
                                   action:@selector(itemClicked:)
                            keyEquivalent:@""];
    menuItem.target = target;
    menuItem.tag = static_cast<NSInteger>(target->callbacks.size());
    target->callbacks.push_back(std::move(callback));
    [((NSStatusItem *)this->item_).menu addItem:menuItem];
    [menuItem release];
}

void MacOSStatusItem::addSeparator()
{
    [((NSStatusItem *)this->item_).menu addItem:[NSMenuItem separatorItem]];
}
#endif

}  // namespace chatterino
