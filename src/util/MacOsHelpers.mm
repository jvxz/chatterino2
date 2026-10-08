#include "util/MacOsHelpers.h"

#include <AppKit/AppKit.h>
#include <QUrl>

/// Borderless windows can't become key by default, which they need to take
/// typing.
@interface ChatterinoOverlayPanel : NSPanel
@end

@implementation ChatterinoOverlayPanel
- (BOOL)canBecomeKeyWindow
{
    return YES;
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

NSWindow *overlayPanel(WId contentView)
{
    return ((__bridge NSView *)reinterpret_cast<void *>(contentView)).window;
}

}  // namespace

WId createMacOSOverlayPanel()
{
    // Lives as long as the overlay, which lives as long as the app
    ChatterinoOverlayPanel *panel = [[ChatterinoOverlayPanel alloc]
        initWithContentRect:NSMakeRect(0, 0, 300, 600)
                  styleMask:NSWindowStyleMaskBorderless |
                            NSWindowStyleMaskNonactivatingPanel
                    backing:NSBackingStoreBuffered
                      defer:NO];
    panel.floatingPanel = YES;
    panel.hidesOnDeactivate = NO;
    panel.releasedWhenClosed = NO;
    panel.becomesKeyOnlyIfNeeded = NO;
    panel.collectionBehavior = NSWindowCollectionBehaviorFullScreenAuxiliary |
                               NSWindowCollectionBehaviorMoveToActiveSpace;

    return reinterpret_cast<WId>((__bridge void *)panel.contentView);
}

void showMacOSOverlayPanel(WId contentView, const QRect &rect)
{
    NSWindow *panel = overlayPanel(contentView);

    // Cocoa's origin is the bottom left corner of the primary screen
    const CGFloat primaryHeight = NSScreen.screens.firstObject.frame.size.height;
    const NSRect frame =
        NSMakeRect(rect.x(), primaryHeight - rect.y() - rect.height(),
                   rect.width(), rect.height());
    if (!NSEqualRects(panel.frame, frame))
    {
        [panel setFrame:frame display:YES];
    }
    if (!panel.visible)
    {
        [panel orderFrontRegardless];
    }
}

void hideMacOSOverlayPanel(WId contentView)
{
    [overlayPanel(contentView) orderOut:nil];
}
#endif

}  // namespace chatterino