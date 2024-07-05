
@interface FDObjectReader : NSObject
{
	NSFileHandle *	pipe;
	NSData *			data;
	CPrecedents *	precedents;
	char				buf[256];
}

- (void) dealloc;
- (Ref) read: (NSString *) path;

- (Ref) readobject;
- (UInt8) readbyte;
- (UInt16) readhalfword;
- (SInt32) readlong;
- (SInt32) readxlong;

@end

