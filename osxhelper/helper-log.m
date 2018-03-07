#import "helper-log.h"

#import <syslog.h>

@interface HelperFileLogger : NSObject
@property NSFileHandle *currentLogFileHandle;
@property NSDateFormatter *dateFormatter;
- (void)log:(NSString *)message;
@end

void HelperLog(NSString *msg, ...) {
  if (!msg) return;
  va_list args;
  va_start(args, msg);

  NSString *string = [[NSString alloc] initWithFormat:msg arguments:args];

  va_end(args);

  // Log to as many places as possible: syslog, stdout and file
  syslog(LOG_NOTICE, "%s", [string UTF8String]);

  NSLog(@"%@", string);

  static dispatch_once_t onceToken;
  static HelperFileLogger *logger;
  dispatch_once(&onceToken, ^{
    logger = [[HelperFileLogger alloc] init];
  });
  [logger log:string];
}

@implementation HelperFileLogger

- (id)init {
  if ((self = [super init])) {
    // Ensure log file exists
    NSString * const logPath = @"/Library/Logs/seadrive-helper.log";
    if (![[NSFileManager defaultManager] fileExistsAtPath:logPath]) {
      if (![[NSFileManager defaultManager] createFileAtPath:logPath contents:nil attributes:nil]) {
        NSLog(@"Unable to create log file: %@", logPath);
      }
    }
    _currentLogFileHandle = [NSFileHandle fileHandleForWritingAtPath:logPath];
    [_currentLogFileHandle seekToEndOfFile];

    _dateFormatter = [[NSDateFormatter alloc] init];
    [_dateFormatter setDateFormat:@"yyyy/MM/dd HH:mm:ss.SSS"];
  }
  return self;
}

- (void)dealloc {
  [_currentLogFileHandle synchronizeFile];
  [_currentLogFileHandle closeFile];
}

/*!
 Log to file.
 */
- (void)log:(NSString *)message {
  NSString *prefix = [_dateFormatter stringFromDate:[NSDate date]];
  NSString *logMessage = [NSString stringWithFormat:@"%@ %@\n", prefix, message];
  NSData *logData = [logMessage dataUsingEncoding:NSUTF8StringEncoding];

  @try {
    [_currentLogFileHandle writeData:logData];
  } @catch (NSException *exception) {
    NSLog(@"Error writing to log: %@", exception);
  }
}

@end
