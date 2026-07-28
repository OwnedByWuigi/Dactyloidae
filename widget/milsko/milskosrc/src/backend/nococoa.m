#include <Mw/Milsko.h>
#import <AppKit/AppKit.h>

void MwLLNoCocoaErrorMessage(void) {
	NSAlert* alert = [[NSAlert alloc] init];
	[alert setMessageText:@"Milsko applications currently requires XQuartz or cocoa-way. PRs are welcome for anyone who wants to finish the Cocoa backend (prefarably in Objective-C)"];
	[alert addButtonWithTitle:@"Ok"];
	[alert runModal];
}
