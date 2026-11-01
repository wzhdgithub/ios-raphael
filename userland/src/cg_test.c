/*
 * cg_test: CoreGraphics drawing into an IOSurface, the way WebKit's GPU process draws a web page's
 * tiles (WebCore::IOSurface::createPlatformContext: CGIOSurfaceContextCreate), and the plain way
 * for comparison (a bitmap context on the surface's locked memory). Says, for each, whether a red
 * rectangle drawn and flushed ends up in the surface's memory. The icon surfaces backboardd makes
 * for CoreAnimation have no pixel format (180x180, four bytes an element), so the same two ways
 * also run on a surface of that shape.
 */
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <IOSurface/IOSurfaceRef.h>
#include <dlfcn.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

extern char **environ;

#define SIZE 256
#define ICON 180

static CFNumberRef
num(int value)
{
	return CFNumberCreate(NULL, kCFNumberIntType, &value);
}

static IOSurfaceRef
make_surface(void)
{
	CFMutableDictionaryRef props = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
	    &kCFTypeDictionaryValueCallBacks);
	CFNumberRef n;
	IOSurfaceRef surface;

	n = num(SIZE); CFDictionarySetValue(props, kIOSurfaceWidth, n); CFRelease(n);
	n = num(SIZE); CFDictionarySetValue(props, kIOSurfaceHeight, n); CFRelease(n);
	n = num(4); CFDictionarySetValue(props, kIOSurfaceBytesPerElement, n); CFRelease(n);
	n = num(SIZE * 4); CFDictionarySetValue(props, kIOSurfaceBytesPerRow, n); CFRelease(n);
	n = num('BGRA'); CFDictionarySetValue(props, kIOSurfacePixelFormat, n); CFRelease(n);
	n = num(SIZE * SIZE * 4); CFDictionarySetValue(props, kIOSurfaceAllocSize, n); CFRelease(n);
	surface = IOSurfaceCreate(props);
	CFRelease(props);
	return surface;
}

/* the shape of backboardd's icon surfaces: no pixel format among the keys */
static IOSurfaceRef
make_icon_surface(void)
{
	CFMutableDictionaryRef props = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
	    &kCFTypeDictionaryValueCallBacks);
	CFNumberRef n;
	IOSurfaceRef surface;

	n = num(ICON); CFDictionarySetValue(props, kIOSurfaceWidth, n); CFRelease(n);
	n = num(ICON); CFDictionarySetValue(props, kIOSurfaceHeight, n); CFRelease(n);
	n = num(4); CFDictionarySetValue(props, kIOSurfaceBytesPerElement, n); CFRelease(n);
	n = num(ICON * 4); CFDictionarySetValue(props, kIOSurfaceBytesPerRow, n); CFRelease(n);
	n = num(ICON * ICON * 4); CFDictionarySetValue(props, kIOSurfaceAllocSize, n); CFRelease(n);
	surface = IOSurfaceCreate(props);
	CFRelease(props);
	return surface;
}

/* how many pixels are red, and what the one at 10,10 is */
static void
look(const char *what, IOSurfaceRef surface, int size)
{
	uint32_t seed = 0, red = 0, zero = 0, first;
	const uint32_t *pixels;

	IOSurfaceLock(surface, kIOSurfaceLockReadOnly, &seed);
	pixels = IOSurfaceGetBaseAddress(surface);
	for (int i = 0; i < size * size; i++) {
		red += pixels[i] == 0xffff0000;
		zero += pixels[i] == 0;
	}
	first = pixels[10 * size + 10];
	IOSurfaceUnlock(surface, kIOSurfaceLockReadOnly, &seed);
	printf("cg_test: %s: %u of %u pixels red, %u zero, the one at 10,10 %#010x (seed %u)\n", what, red,
	    size * size, zero, first, IOSurfaceGetSeed(surface));
}

/* a second process looks the surface up by its number and says what it sees there */
static void
seen_elsewhere(const char *self, IOSurfaceRef surface)
{
	char number[16];
	char *args[] = { (char *)self, "child", number, NULL };
	pid_t pid;
	int status;

	snprintf(number, sizeof(number), "%u", IOSurfaceGetID(surface));
	if (posix_spawn(&pid, self, NULL, NULL, args, environ) == 0) {
		waitpid(pid, &status, 0);
	}
}

int
main(int argc, char **argv)
{
	if (argc == 3 && strcmp(argv[1], "child") == 0) {
		IOSurfaceRef surface = IOSurfaceLookup((IOSurfaceID)atoi(argv[2]));

		if (surface == NULL) {
			printf("cg_test: (another process) surface %s not found\n", argv[2]);
			return 1;
		}
		look("(another process)", surface, SIZE);
		return 0;
	}
	CGContextRef (*create)(IOSurfaceRef, size_t, size_t, size_t, size_t, CGColorSpaceRef, CGBitmapInfo) =
	    (CGContextRef (*)(IOSurfaceRef, size_t, size_t, size_t, size_t, CGColorSpaceRef, CGBitmapInfo))dlsym(RTLD_DEFAULT,
	    "CGIOSurfaceContextCreate");
	CGColorSpaceRef space = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
	CGBitmapInfo info = kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little;
	IOSurfaceRef surface = make_surface(), plain = make_surface();
	IOSurfaceRef icon = make_icon_surface(), iconplain = make_icon_surface();
	CGContextRef context;
	uint32_t seed = 0;

	if (surface == NULL || plain == NULL || icon == NULL || iconplain == NULL) {
		printf("cg_test: IOSurfaceCreate failed (bgra %p %p, icon %p %p)\n", surface, plain, icon, iconplain);
		return 1;
	}
	printf("cg_test: bgra surfaces %u/%u and icon surfaces %u/%u, %zu and %zu bytes a row; "
	    "CGIOSurfaceContextCreate is %s\n",
	    IOSurfaceGetID(surface), IOSurfaceGetID(plain), IOSurfaceGetID(icon), IOSurfaceGetID(iconplain),
	    IOSurfaceGetBytesPerRow(surface), IOSurfaceGetBytesPerRow(icon), create ? "there" : "NOT THERE");
	look("before", surface, SIZE);

	/* WebKit's way, on a BGRA surface */
	if (create != NULL && (context = create(surface, SIZE, SIZE, 8, 32, space, info)) != NULL) {
		CGContextSetRGBFillColor(context, 1, 0, 0, 1);
		CGContextFillRect(context, CGRectMake(0, 0, SIZE, SIZE));
		CGContextFlush(context);
		look("CGIOSurfaceContext, filled and flushed", surface, SIZE);
		CGContextRelease(context);
		look("CGIOSurfaceContext, released", surface, SIZE);
		seen_elsewhere(argv[0], surface);
	} else {
		printf("cg_test: CGIOSurfaceContextCreate gave no context\n");
	}

	/* the plain way, on a BGRA surface */
	IOSurfaceLock(plain, 0, &seed);
	context = CGBitmapContextCreate(IOSurfaceGetBaseAddress(plain), SIZE, SIZE, 8, IOSurfaceGetBytesPerRow(plain),
	    space, info);
	if (context != NULL) {
		CGContextSetRGBFillColor(context, 1, 0, 0, 1);
		CGContextFillRect(context, CGRectMake(0, 0, SIZE, SIZE));
		CGContextRelease(context);
	} else {
		printf("cg_test: CGBitmapContextCreate gave no context on the bgra surface\n");
	}
	IOSurfaceUnlock(plain, 0, &seed);
	look("a bitmap context on the locked bgra memory", plain, SIZE);
	seen_elsewhere(argv[0], plain);

	/* WebKit's way, on the icon shape (no pixel format) */
	if (create != NULL && (context = create(icon, ICON, ICON, 8, 32, space, info)) != NULL) {
		CGContextSetRGBFillColor(context, 1, 0, 0, 1);
		CGContextFillRect(context, CGRectMake(0, 0, ICON, ICON));
		CGContextFlush(context);
		look("CGIOSurfaceContext on the icon shape, filled and flushed", icon, ICON);
		CGContextRelease(context);
		seen_elsewhere(argv[0], icon);
	} else {
		printf("cg_test: CGIOSurfaceContextCreate gave no context on the icon shape\n");
	}

	/* the plain way, on the icon shape */
	IOSurfaceLock(iconplain, 0, &seed);
	context = CGBitmapContextCreate(IOSurfaceGetBaseAddress(iconplain), ICON, ICON, 8, IOSurfaceGetBytesPerRow(iconplain),
	    space, info);
	if (context != NULL) {
		CGContextSetRGBFillColor(context, 1, 0, 0, 1);
		CGContextFillRect(context, CGRectMake(0, 0, ICON, ICON));
		CGContextRelease(context);
	} else {
		printf("cg_test: CGBitmapContextCreate gave no context on the icon shape\n");
	}
	IOSurfaceUnlock(iconplain, 0, &seed);
	look("a bitmap context on the locked icon memory", iconplain, ICON);
	seen_elsewhere(argv[0], iconplain);
	return 0;
}
