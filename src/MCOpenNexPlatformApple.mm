#include "MCOpenNexPlatform.h"

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>

namespace {

NSURL *validatedUrl(const char *value) {
  if (!value || value[0] == '\0') {
    return nil;
  }
  NSString *text = [NSString stringWithUTF8String:value];
  NSURL *url = text ? [NSURL URLWithString:text] : nil;
  NSString *scheme = [[url scheme] lowercaseString];
  if (![scheme isEqualToString:@"https"] &&
      ![scheme isEqualToString:@"mcnexus"]) {
    return nil;
  }
  return url;
}

} // namespace

namespace mcopen {

bool canOpenUrl(const char *value) {
  @autoreleasepool {
    NSURL *url = validatedUrl(value);
    if (!url) {
      return false;
    }
    if ([[[url scheme] lowercaseString] isEqualToString:@"https"]) {
      return true;
    }
    return [[NSWorkspace sharedWorkspace] URLForApplicationToOpenURL:url] !=
           nil;
  }
}

bool openUrl(const char *value) {
  @autoreleasepool {
    NSURL *url = validatedUrl(value);
    return url && [[NSWorkspace sharedWorkspace] openURL:url];
  }
}

bool openMCNexusApplication() {
  @autoreleasepool {
    NSWorkspace *workspace = [NSWorkspace sharedWorkspace];
    NSURL *applicationURL =
        [workspace URLForApplicationWithBundleIdentifier:@"com.MCAppsTools"];
    if (!applicationURL) {
      NSURL *fallbackURL =
          [NSURL fileURLWithPath:@"/Applications/MCNexus.app"];
      if ([[NSFileManager defaultManager]
              fileExistsAtPath:[fallbackURL path]]) {
        applicationURL = fallbackURL;
      }
    }
    if (!applicationURL) {
      return false;
    }
    [workspace
        openApplicationAtURL:applicationURL
               configuration:[NSWorkspaceOpenConfiguration configuration]
           completionHandler:nil];
    return true;
  }
}

} // namespace mcopen
