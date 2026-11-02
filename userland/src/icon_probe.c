/*
 * icon_probe: can this machine make an icon at all?
 *
 * The GPU round says backboardd's icon surfaces (180 x 180, no pixel format) stay empty
 * forever, and the census says nobody ever writes them. This asks the two remaining
 * questions on the phone itself: are the icon files readable and decodable here, and does
 * drawing one into a surface of the icon shape put pixels in it? Pixels here means the
 * producer chain is at fault; nothing here means the machine cannot make icons at all.
 */
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <IOSurface/IOSurfaceRef.h>
#include <dlfcn.h>
#include <stdio.h>
#include <sys/stat.h>

#define SIDE 180

static CFNumberRef
num(int v)
{
	return CFNumberCreate(NULL, kCFNumberIntType, &v);
}

/* A surface of the icon shape, made the way backboardd makes them: no pixel format key. */
static IOSurfaceRef
make_icon_surface(int side)
{
	CFMutableDictionaryRef props = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
	    &kCFTypeDictionaryValueCallBacks);
	CFNumberRef n;
	IOSurfaceRef surface;

	n = num(side); CFDictionarySetValue(props, kIOSurfaceWidth, n); CFRelease(n);
	n = num(side); CFDictionarySetValue(props, kIOSurfaceHeight, n); CFRelease(n);
	n = num(4); CFDictionarySetValue(props, kIOSurfaceBytesPerElement, n); CFRelease(n);
	n = num(side * 4); CFDictionarySetValue(props, kIOSurfaceBytesPerRow, n); CFRelease(n);
	n = num(side * side * 4); CFDictionarySetValue(props, kIOSurfaceAllocSize, n); CFRelease(n);
	surface = IOSurfaceCreate(props);
	CFRelease(props);
	return surface;
}

/* How much of a surface is not zero, and which of the four bytes of a pixel holds it. */
static void
look(const char *what, IOSurfaceRef surface, int side)
{
	uint32_t seed = 0;
	size_t lane[4] = { 0, 0, 0, 0 }, nz = 0;
	size_t used, alloc;
	const unsigned char *pixels;

	IOSurfaceLock(surface, kIOSurfaceLockReadOnly, &seed);
	alloc = IOSurfaceGetAllocSize(surface);
	used = IOSurfaceGetBytesPerRow(surface) * (size_t)(side - 1) + (size_t)side * 4;
	if (used > alloc) {
		used = alloc;
	}
	pixels = (const unsigned char *)IOSurfaceGetBaseAddress(surface);
	for (size_t at = 0; at < used; at++) {
		if (pixels[at] != 0) {
			nz++;
			lane[at & 3]++;
		}
	}
	IOSurfaceUnlock(surface, kIOSurfaceLockReadOnly, &seed);
	printf("PROBE %s: surface %u of %zu bytes, %zu used: non-zero %zu (by place in the pixel"
	    " %zu %zu %zu %zu), seed %u\n", what, IOSurfaceGetID(surface), alloc, used, nz,
	    lane[0], lane[1], lane[2], lane[3], IOSurfaceGetSeed(surface));
}

static void
show_file(const char *path)
{
	unsigned char head[8];
	struct stat st;
	size_t got = 0;
	FILE *f;

	printf("PROBE file %s: ", path);
	if (stat(path, &st) != 0) {
		printf("not there\n");
		return;
	}
	f = fopen(path, "rb");
	if (f != NULL) {
		got = fread(head, 1, sizeof(head), f);
		fclose(f);
	}
	printf("%lld bytes, %s first %zu:%s%02x %02x %02x %02x %02x %02x %02x %02x\n", (long long)st.st_size,
	    f == NULL ? "cannot open," : "", got, head[0], head[1], head[2], head[3], head[4], head[5],
	    head[6], head[7]);
}

/* Decode a picture and draw it into a surface of the icon shape, the two ways cg_test proved. */
static void
try_icon(const char *path)
{
	CFStringRef name = CFStringCreateWithCString(NULL, path, kCFStringEncodingUTF8);
	CFURLRef url = CFURLCreateWithFileSystemPath(NULL, name, kCFURLPOSIXPathStyle, false);
	CGImageSourceRef source = url != NULL ? CGImageSourceCreateWithURL(url, NULL) : NULL;
	CGImageRef image = source != NULL ? CGImageSourceCreateImageAtIndex(source, 0, NULL) : NULL;
	CGColorSpaceRef space = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
	CGBitmapInfo info = kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little;
	CGContextRef (*create)(IOSurfaceRef, size_t, size_t, size_t, size_t, CGColorSpaceRef, CGBitmapInfo) =
	    (CGContextRef (*)(IOSurfaceRef, size_t, size_t, size_t, size_t, CGColorSpaceRef, CGBitmapInfo))
	        dlsym(RTLD_DEFAULT, "CGIOSurfaceContextCreate");
	IOSurfaceRef surface = make_icon_surface(SIDE), plain = make_icon_surface(SIDE);
	CGContextRef context;
	uint32_t seed = 0;

	if (image == NULL) {
		printf("PROBE %s: no CGImage (source %p)\n", path, source);
		goto out;
	}
	printf("PROBE %s: decoded %zu x %zu, %zu bits a component, %zu bytes a row, alpha info %d\n", path,
	    CGImageGetWidth(image), CGImageGetHeight(image), CGImageGetBitsPerComponent(image),
	    CGImageGetBytesPerRow(image), (int)CGImageGetAlphaInfo(image));
	if (surface == NULL || plain == NULL) {
		printf("PROBE %s: no icon surface (%p %p)\n", path, surface, plain);
		goto out;
	}
	look("as made", surface, SIDE);
	if (create != NULL && (context = create(surface, SIDE, SIDE, 8, SIDE * 4, space, info)) != NULL) {
		CGContextDrawImage(context, CGRectMake(0, 0, SIDE, SIDE), image);
		CGContextFlush(context);
		CGContextRelease(context);
		look("after CGIOSurfaceContextDraw", surface, SIDE);
	} else {
		printf("PROBE %s: no CGIOSurfaceContextCreate (%p)\n", path, create);
	}
	IOSurfaceLock(plain, 0, &seed);
	context = CGBitmapContextCreate(IOSurfaceGetBaseAddress(plain), SIDE, SIDE, 8,
	    IOSurfaceGetBytesPerRow(plain), space, info);
	if (context != NULL) {
		CGContextDrawImage(context, CGRectMake(0, 0, SIDE, SIDE), image);
		CGContextRelease(context);
	}
	IOSurfaceUnlock(plain, 0, &seed);
	look("after a bitmap context on locked memory", plain, SIDE);

out:
	if (image != NULL) {
		CGImageRelease(image);
	}
	if (source != NULL) {
		CFRelease(source);
	}
	if (url != NULL) {
		CFRelease(url);
	}
	if (name != NULL) {
		CFRelease(name);
	}
	if (space != NULL) {
		CGColorSpaceRelease(space);
	}
	if (surface != NULL) {
		CFRelease(surface);
	}
	if (plain != NULL) {
		CFRelease(plain);
	}
}

int
main(void)
{
	setvbuf(stdout, NULL, _IONBF, 0);
	printf("PROBE icon_probe starts\n");
	show_file("/Applications/AppStore.app/Assets.car");
	show_file("/Applications/AppStore.app/AppIcon60x60@2x.png");
	show_file("/Applications/Camera.app/AppIcon60x60@2x.png");
	show_file("/Applications/MobileSlideShow.app/Assets.car");
	show_file("/System/Library/CoreServices/Icons/UICloseButton.png");
	try_icon("/Applications/AppStore.app/AppIcon60x60@2x.png");
	try_icon("/Applications/Camera.app/AppIcon60x60@2x.png");
	printf("PROBE done\n");
	return 0;
}
