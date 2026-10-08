#include "util/MacOsHelpers.h"

#include <AppKit/AppKit.h>
#include <objc/message.h>
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

void makeMacOSWindowNonActivating(WId winId)
{
    NSView *view = (__bridge NSView *)reinterpret_cast<void *>(winId);
    NSWindow *window = view.window;
    if (![window isKindOfClass:[NSPanel class]] ||
        (window.styleMask & NSWindowStyleMaskNonactivatingPanel) != 0)
    {
        return;
    }

    window.styleMask |= NSWindowStyleMaskNonactivatingPanel;

    // AppKit only reads the style when the window is created to decide whether
    // clicking it activates the app. Qt has already created it by now, so tell
    // the window server directly when that private setter exists.
    SEL preventsActivation = NSSelectorFromString(@"_setPreventsActivation:");
    if ([window respondsToSelector:preventsActivation])
    {
        reinterpret_cast<void (*)(id, SEL, BOOL)>(objc_msgSend)(
            window, preventsActivation, YES);
    }
}
#endif

}  // namespace chatterino