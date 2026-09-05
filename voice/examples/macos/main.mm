#import <AppKit/AppKit.h>
#import <AVFoundation/AVFoundation.h>
#include <msime/voice/audio_capture.h>
#include <msime/voice/cloud_stt_worker.h>
#include <algorithm>
#include <mutex>
using namespace metasequoia::voice;
struct Recording {
    std::mutex mutex;
    std::vector<float> samples;
    bool overflow = false;
};
@interface VoiceExample : NSObject <NSApplicationDelegate, NSWindowDelegate>
@end
@implementation VoiceExample {
    NSWindow* _window;
    NSButton* _button;
    NSTextView* _text;
    AudioCapture _capture;
    std::shared_ptr<Recording> _recording;
    std::shared_ptr<std::atomic_bool> _cancelled;
    RequestOptions _options;
    BOOL _isRecording;
    BOOL _closed;
}
- (void)applicationDidFinishLaunching:(NSNotification*)notification {
    (void)notification;
    auto env = [](const char* name) { const char* value = getenv(name); return value ? std::string(value) : std::string(); };
    _options = {env("MSIME_ASR_ENDPOINT"), env("MSIME_ASR_MODEL"), env("MSIME_ASR_TOKEN"), 10000, {}};
    _window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 560, 300)
        styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable backing:NSBackingStoreBuffered defer:NO];
    _window.title = @"MSIME Voice Example";
    _window.delegate = self;
    _window.releasedWhenClosed = NO;
    _button = [NSButton buttonWithTitle:@"Start recording" target:self action:@selector(toggle:)];
    _button.frame = NSMakeRect(20, 250, 200, 30);
    [_window.contentView addSubview:_button];
    _text = [[NSTextView alloc] initWithFrame:NSMakeRect(20, 20, 520, 220)];
    _text.editable = NO;
    _text.font = [NSFont systemFontOfSize:17];
    _text.string = @"Press Start, speak, then press Transcribe. Maximum recording: 60 seconds.";
    if (_options.endpoint.empty() || _options.model.empty() || _options.token.empty()) {
        _text.string = @"Configure MSIME_ASR_ENDPOINT, MSIME_ASR_MODEL and MSIME_ASR_TOKEN as described in voice/README.md.";
        _button.enabled = NO;
    }
    [_window.contentView addSubview:_text];
    [_window center]; [_window makeKeyAndOrderFront:nil]; [NSApp activateIgnoringOtherApps:YES];
}
- (void)toggle:(id)sender {
    (void)sender;
    if (_isRecording) { [self transcribe]; return; }
    _button.enabled = NO;
    __weak VoiceExample* weakSelf = self;
    [AVCaptureDevice requestAccessForMediaType:AVMediaTypeAudio completionHandler:^(BOOL granted) {
        dispatch_async(dispatch_get_main_queue(), ^{
            VoiceExample* host = weakSelf;
            if (!host || host->_closed) return;
            if (!granted) {
                host->_text.string = @"Microphone permission was denied.";
                host->_button.enabled = YES;
                return;
            }
            [host beginRecording];
        });
    }];
}
- (void)beginRecording {
    _recording = std::make_shared<Recording>();
    const auto recording = _recording;
    const bool started = _capture.start([recording](const float* samples, std::size_t count) {
        std::lock_guard<std::mutex> lock(recording->mutex);
        const auto available = maximum_samples - recording->samples.size();
        const auto accepted = std::min(count, available);
        recording->samples.insert(recording->samples.end(), samples, samples + accepted);
        if (accepted != count) recording->overflow = true;
    });
    _isRecording = started;
    _button.enabled = YES;
    _button.title = started ? @"Transcribe" : @"Start recording";
    _text.string = started ? @"Recording…" : @"Could not start the microphone.";
}
- (void)transcribe {
    _capture.stop();
    _isRecording = NO;
    _button.title = @"Start recording";
    // stop() joins device callbacks; no capture thread can modify these samples now.
    if (_recording->overflow || _capture.callback_failed()) {
        _text.string = @"Recording failed or exceeded 60 seconds. Please try again.";
        _recording.reset();
        return;
    }
    auto samples = std::move(_recording->samples);
    _recording.reset();
    _button.enabled = NO;
    _text.string = @"Transcribing…";
    _cancelled = std::make_shared<std::atomic_bool>(false);
    auto options = _options;
    options.cancelled = _cancelled;
    __weak VoiceExample* weakSelf = self;
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        std::string text;
        try { CloudSttWorker provider(options); text = provider.recognize(samples); }
        catch (const std::exception& error) { text = error.what(); }
        NSString* result = [[NSString alloc] initWithBytes:text.data() length:text.size() encoding:NSUTF8StringEncoding];
        dispatch_async(dispatch_get_main_queue(), ^{
            VoiceExample* host = weakSelf;
            if (!host || host->_closed || options.cancelled->load()) return;
            host->_text.string = result ?: @"Invalid UTF-8 transcription.";
            host->_button.enabled = YES;
        });
    });
}
- (void)windowWillClose:(NSNotification*)notification {
    (void)notification;
    _closed = YES;
    _capture.stop();
    if (_cancelled) _cancelled->store(true);
}
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)sender { (void)sender; return YES; }
@end
int main() {
    @autoreleasepool {
        NSApplication* app = [NSApplication sharedApplication];
        app.activationPolicy = NSApplicationActivationPolicyRegular;
        VoiceExample* delegate = [[VoiceExample alloc] init];
        app.delegate = delegate;
        [app run];
    }
}
