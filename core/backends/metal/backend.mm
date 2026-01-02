// Metal backend implementing core/src/backend.h API

#import <Foundation/Foundation.h>
#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import <QuartzCore/QuartzCore.h>

#include <unordered_set>
#include <mutex>
#include "imgui.h"
#include "implot.h"
#include "imgui_impl_metal.h"
#include "imgui_impl_osx.h"
#include "../../src/backend.h"
#include <core.h>
#include <gui/gui.h>

using namespace std;

// Flag set when window wants the app to quit; checked by renderLoop
static volatile bool g_shouldQuit = false;

// Custom MTKView that forwards input events to ImGui via the local NSEvent monitor
@interface BackendMTKView : MTKView
@end

@implementation BackendMTKView
- (BOOL)acceptsFirstResponder { return YES; }
// Helper to map NSEvent subtype to ImGuiMouseSource
- (ImGuiMouseSource)getMouseSourceForEvent:(NSEvent*)event {
    switch (event.subtype) {
        case NSEventSubtypeTabletPoint: return ImGuiMouseSource_Pen;
        default: return ImGuiMouseSource_Mouse;
    }
}

- (NSPoint)convertEventToViewPoint:(NSEvent*)event {
    NSPoint mousePoint = event.locationInWindow;
    if (event.window == nil)
        mousePoint = [[(NSView*)self window] convertPointFromScreen:mousePoint];
    mousePoint = [(NSView*)self convertPoint:mousePoint fromView:nil];
    if ([(NSView*)self isFlipped])
        return NSMakePoint(mousePoint.x, mousePoint.y);
    else
        return NSMakePoint(mousePoint.x, [(NSView*)self bounds].size.height - mousePoint.y);
}

- (void)mouseMoved:(NSEvent*)event {
    NSPoint p = [self convertEventToViewPoint:event];
    ImGuiIO& io = ImGui::GetIO();
    io.AddMouseSourceEvent([self getMouseSourceForEvent:event]);
    io.AddMousePosEvent((float)p.x, (float)p.y);
    [super mouseMoved:event];
}

- (void)mouseDown:(NSEvent*)event {
    NSPoint p = [self convertEventToViewPoint:event];
    ImGuiIO& io = ImGui::GetIO();
    io.AddMouseSourceEvent([self getMouseSourceForEvent:event]);
    io.AddMousePosEvent((float)p.x, (float)p.y);
    int button = (int)[event buttonNumber];
    if (button >= 0 && button < ImGuiMouseButton_COUNT) io.AddMouseButtonEvent(button, true);
    [super mouseDown:event];
}

- (void)mouseUp:(NSEvent*)event {
    NSPoint p = [self convertEventToViewPoint:event];
    ImGuiIO& io = ImGui::GetIO();
    io.AddMouseSourceEvent([self getMouseSourceForEvent:event]);
    io.AddMousePosEvent((float)p.x, (float)p.y);
    int button = (int)[event buttonNumber];
    if (button >= 0 && button < ImGuiMouseButton_COUNT) io.AddMouseButtonEvent(button, false);
    [super mouseUp:event];
}

- (void)rightMouseDown:(NSEvent*)event { [self mouseDown:event]; }
- (void)rightMouseUp:(NSEvent*)event { [self mouseUp:event]; }
- (void)otherMouseDown:(NSEvent*)event { [self mouseDown:event]; }
- (void)otherMouseUp:(NSEvent*)event { [self mouseUp:event]; }

- (void)mouseDragged:(NSEvent*)event { [self mouseMoved:event]; [super mouseDragged:event]; }
- (void)rightMouseDragged:(NSEvent*)event { [self mouseMoved:event]; [super rightMouseDragged:event]; }
- (void)otherMouseDragged:(NSEvent*)event { [self mouseMoved:event]; [super otherMouseDragged:event]; }

- (void)scrollWheel:(NSEvent*)event {
    ImGuiIO& io = ImGui::GetIO();
    if (event.phase == NSEventPhaseCancelled) { [super scrollWheel:event]; return; }
    double wheel_dx = 0.0, wheel_dy = 0.0;
#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1070
    if (floor(NSAppKitVersionNumber) > NSAppKitVersionNumber10_6) {
        wheel_dx = [event scrollingDeltaX];
        wheel_dy = [event scrollingDeltaY];
        if ([event hasPreciseScrollingDeltas]) { wheel_dx *= 0.01; wheel_dy *= 0.01; }
    } else
#endif
    {
        wheel_dx = [event deltaX] * 0.1;
        wheel_dy = [event deltaY] * 0.1;
    }
    if (wheel_dx != 0.0 || wheel_dy != 0.0) io.AddMouseWheelEvent((float)wheel_dx, (float)wheel_dy);
    [super scrollWheel:event];
}

- (void)keyDown:(NSEvent*)event { [super keyDown:event]; }
- (void)keyUp:(NSEvent*)event { [super keyUp:event]; }
- (void)flagsChanged:(NSEvent*)event { [super flagsChanged:event]; }

@end

// Window delegate to terminate the app when the main window is closed
@interface BackendWindowDelegate : NSObject <NSWindowDelegate>
@end

@implementation BackendWindowDelegate
- (void)windowWillClose:(NSNotification*)notification {
    g_shouldQuit = true;
    [NSApp terminate:nil];
}

@end

@interface BackendController : NSViewController
@end

@implementation BackendController
- (void)loadView
{
    self.view = [[BackendMTKView alloc] initWithFrame:NSMakeRect(0, 0, 1200, 800)];
}

@end

namespace backend {

static id<MTLDevice> g_device = nil;
static id<MTLCommandQueue> g_commandQueue = nil;
static MTKView* g_mtkView = nil;
static NSWindow* g_window = nil;
static id g_windowDelegate = nil;
static id<MTLCommandBuffer> g_commandBuffer = nil;
static MTLRenderPassDescriptor* g_renderPassDescriptor = nil;
static double g_mouseX = 0.0, g_mouseY = 0.0;
// Window state (kept similar to Windows backend for parity)
static bool maximized = false;
static bool fullScreen = false;
static int winWidth = 1280;
static int winHeight = 800;
static int _winWidth = 0, _winHeight = 0;
static bool _maximized = false;


int init(std::string resDir)
{
    @autoreleasepool {
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

        // If a resource directory was provided (not using a bundle), try to load
        // a dock/application icon from common filenames and set it for the app.
        if (!resDir.empty()) {
            NSString* nsRes = [NSString stringWithUTF8String:resDir.c_str()];
            NSString* path = [nsRes stringByAppendingPathComponent:@"icons/sdr888.ico"];
            if ([[NSFileManager defaultManager] fileExistsAtPath:path]) {
                NSImage* icon = [[NSImage alloc] initWithContentsOfFile:path];
                if (icon) {
                    [NSApp setApplicationIconImage:icon];
                }
            }
        }

        // Create device and queue
        g_device = MTLCreateSystemDefaultDevice();
        if (!g_device) return -1;
        g_commandQueue = [g_device newCommandQueue];

        // Create window + view
        BackendController *rootVC = [[BackendController alloc] initWithNibName:nil bundle:nil];
        g_window = [[NSWindow alloc] initWithContentRect:NSZeroRect
                                                styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable | NSWindowStyleMaskMiniaturizable
                                                  backing:NSBackingStoreBuffered
                                                    defer:NO];
        g_window.contentViewController = rootVC;
        [g_window center];
        [g_window makeKeyAndOrderFront:nil];
        [NSApp activateIgnoringOtherApps:YES];

        // Create and assign window delegate so closing the main window quits the app
        g_windowDelegate = [BackendWindowDelegate new];
        g_window.delegate = g_windowDelegate;

        g_mtkView = (MTKView *)rootVC.view;
        g_mtkView.device = g_device;
        // Enable mouse moved events and make view first responder so input reaches our handlers
        [g_window setAcceptsMouseMovedEvents:YES];
        [g_window makeFirstResponder:g_mtkView];

        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImPlot::CreateContext();

        ImGuiIO& io = ImGui::GetIO(); (void)io;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

        // Init backends
        ImGui_ImplMetal_Init(g_device);
        ImGui_ImplOSX_Init(g_mtkView);

        return 0;
    }
}

void beginFrame()
{
    if (!g_mtkView || !g_commandQueue) return;

    // start a command buffer and get render pass descriptor
    g_commandBuffer = [g_commandQueue commandBuffer];
    g_renderPassDescriptor = g_mtkView.currentRenderPassDescriptor;
    if (!g_renderPassDescriptor)
    {
        // Nothing to render this frame
        [g_commandBuffer commit];
        g_commandBuffer = nil;
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize.x = g_mtkView.bounds.size.width;
    io.DisplaySize.y = g_mtkView.bounds.size.height;

    CGFloat framebufferScale = g_mtkView.window.screen.backingScaleFactor ?: NSScreen.mainScreen.backingScaleFactor;
    io.DisplayFramebufferScale = ImVec2(framebufferScale, framebufferScale);

    ImGui_ImplMetal_NewFrame(g_renderPassDescriptor);
    ImGui_ImplOSX_NewFrame(g_mtkView);
    ImGui::NewFrame();
}

void render(bool vsync)
{
    if (!g_commandBuffer || !g_renderPassDescriptor || !g_mtkView) return;

    ImGui::Render();
    ImDrawData* draw_data = ImGui::GetDrawData();

    // If vsync requested, obtain a drawable from the underlying CAMetalLayer.
    // Calling `nextDrawable` will block until a drawable is available, effectively
    // synchronizing rendering with the display refresh.
    id<CAMetalDrawable> vsyncDrawable = nil;
    if (vsync) {
        CAMetalLayer *layer = (CAMetalLayer*)g_mtkView.layer;
        if (layer) {
            vsyncDrawable = [layer nextDrawable];
            if (vsyncDrawable) {
                // Ensure the render pass uses the drawable's texture
                g_renderPassDescriptor.colorAttachments[0].texture = vsyncDrawable.texture;
            }
        }
    }

    id<MTLRenderCommandEncoder> renderEncoder = [g_commandBuffer renderCommandEncoderWithDescriptor:g_renderPassDescriptor];
    [renderEncoder pushDebugGroup:@"Dear ImGui rendering"];
    ImGui_ImplMetal_RenderDrawData(draw_data, g_commandBuffer, renderEncoder);
    [renderEncoder popDebugGroup];
    [renderEncoder endEncoding];
    if (vsyncDrawable) {
        [g_commandBuffer presentDrawable:vsyncDrawable];
    } else if (g_mtkView.currentDrawable) {
        [g_commandBuffer presentDrawable:g_mtkView.currentDrawable];
    }
    [g_commandBuffer commit];

    g_commandBuffer = nil;
    g_renderPassDescriptor = nil;
}

void getMouseScreenPos(double& x, double& y)
{
    x = g_mouseX;
    y = g_mouseY;
}

void setMouseScreenPos(double x, double y)
{
    g_mouseX = x;
    g_mouseY = y;
}

int renderLoop()
{
    @autoreleasepool {
        // Main loop: process Cocoa events and mirror the Windows render loop behavior
        while (true) {
            @autoreleasepool {
                // Drain pending events without blocking
                NSEvent *ev = nil;
                while ((ev = [NSApp nextEventMatchingMask:NSEventMaskAny
                                                 untilDate:[NSDate dateWithTimeIntervalSinceNow:0]
                                                    inMode:NSDefaultRunLoopMode
                                                   dequeue:YES])) {
                    [NSApp sendEvent:ev];
                }

                if (!g_window) return 0;

                if (g_shouldQuit) {
                    return 0;
                }

                // Window state and size tracking
                bool nowMax = ([g_window respondsToSelector:@selector(isZoomed)] && [g_window isZoomed]);
                if (nowMax != _maximized) {
                    _maximized = nowMax;
                    core::configManager.acquire();
                    core::configManager.conf["maximized"] = _maximized;
                    core::configManager.release(true);
                }

                NSRect rc = g_mtkView ? g_mtkView.bounds : NSZeroRect;
                _winWidth = (int)rc.size.width;
                _winHeight = (int)rc.size.height;

                if (ImGui::IsKeyPressed(ImGuiKey_F11)) {
                    fullScreen = !fullScreen;
                    if (g_window && [g_window respondsToSelector:@selector(toggleFullScreen:)])
                        [g_window toggleFullScreen:nil];
                    core::configManager.acquire();
                    core::configManager.conf["fullscreen"] = fullScreen;
                    core::configManager.release();
                }

                if ((_winWidth != winWidth || _winHeight != winHeight) && !_maximized && _winWidth > 0 && _winHeight > 0 && !fullScreen) {
                    winWidth = _winWidth;
                    winHeight = _winHeight;
                    core::configManager.acquire();
                    core::configManager.conf["windowSize"]["w"] = winWidth;
                    core::configManager.conf["windowSize"]["h"] = winHeight;
                    core::configManager.release(true);
                }

                beginFrame();

                if (_winWidth > 0 && _winHeight > 0) {
                    ImGui::SetNextWindowPos(ImVec2(0, 0));
                    ImGui::SetNextWindowSize(ImVec2((float)_winWidth, (float)_winHeight));
                    gui::mainWindow.draw();
                }

                render(true);
            }

            // Small sleep to avoid busy loop when idle
            [NSThread sleepForTimeInterval:0.001];
        }

        return 0;
    }
}

int end()
{
    ImGui_ImplMetal_Shutdown();
    ImGui_ImplOSX_Shutdown();
    ImGui::DestroyContext();

    if (g_window) {
        // Clear delegate first to avoid retain cycles, then close window
        g_window.delegate = nil;
        g_windowDelegate = nil;
        [g_window close];
        g_window = nil;
    }
    g_mtkView = nil;
    g_commandQueue = nil;
    g_device = nil;
    return 0;
}

// We represent ImTextureID as a retained Objective-C texture pointer encoded into an integer.
ImTextureID createTexture(int width, int height, const void* data)
{
    if (!g_device) return (ImTextureID)0;
    MTLTextureDescriptor *desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                                                       width:width
                                                                                      height:height
                                                                                   mipmapped:NO];
    id<MTLTexture> tex = [g_device newTextureWithDescriptor:desc];
    if (!tex) return (ImTextureID)0;

    if (data)
    {
        MTLRegion region = MTLRegionMake2D(0, 0, width, height);
        NSUInteger bytesPerRow = 4 * width;
        [tex replaceRegion:region mipmapLevel:0 withBytes:data bytesPerRow:bytesPerRow];
    }

    // Retain the Objective-C object and return its pointer as an integer ImTextureID.
    void* retained = (__bridge_retained void*)tex; // +1 retain transferred to void*
    return (ImTextureID)(intptr_t)retained;
}

void updateTexture(ImTextureID textureId, const void* data)
{
    if (!textureId || !data) return;
    id<MTLTexture> tex = (__bridge id<MTLTexture>)(void*)(intptr_t)textureId;
    if (!tex) return;
    int width = (int)tex.width;
    int height = (int)tex.height;
    MTLRegion region = MTLRegionMake2D(0, 0, width, height);
    NSUInteger bytesPerRow = 4 * width;
    [tex replaceRegion:region mipmapLevel:0 withBytes:data bytesPerRow:bytesPerRow];
}

void deleteTexture(ImTextureID textureId)
{
    if (!textureId) return;
    // Transfer ownership back to ARC and release the texture
    id<MTLTexture> tex = (__bridge_transfer id<MTLTexture>)(void*)(intptr_t)textureId; // releases when out of scope
    (void)tex;
}

} // namespace backend
