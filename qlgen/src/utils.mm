#import "utils.h"

extern "C" {

NSString *SFURLToPath(NSURL *url)
{
    return [[url absoluteURL] path];
}

NSURL *SFPathToURL(NSString *path)
{
    return [[NSURL alloc] initFileURLWithPath:path];
}

}
