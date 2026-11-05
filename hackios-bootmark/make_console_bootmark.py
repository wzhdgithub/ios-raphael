#!/usr/bin/env python3
"""Put the boot mark inside XNU's video console, the thing that actually draws the boot screen.

The loader only hands the panel over (its print() goes to the log and the serial line); the text the
phone shows while iOS comes up is drawn by osfmk/console/video_console.c from the global `vinfo`,
whose v_baseAddr is the framebuffer once initialize_screen has it mapped. So the mark goes next to
that variable and is drawn at the end of initialize_screen -- which runs again on every re-init, so
a re-acquire by the display driver repaints it rather than losing it.

It is gated on a boot argument (`bootmark=1`) so the payload decides, and it keeps its mouth shut
when there is no framebuffer or the depth is not 32.

usage: make_console_bootmark.py VIDEO_CONSOLE_C BOOTMARK_H [--print-only]
"""
import pathlib
import re
import sys

BLOCK = '''
/* --- 南瓜馅月饼iOS: the picture on the panel while iOS comes up (nangua round) ------------ */
/*
 * Runs of (u16 count, u32 word) in the panel's own order. The loader hands its GOP framebuffer to
 * XNU unchanged, and that framebuffer is B8G8R8X8, so the same bytes work in both stages.
 */
#define BOOTMARK_WIDTH %d
#define BOOTMARK_HEIGHT %d
#define BOOTMARK_RUNS %d
static const unsigned char bootmark_rle[] = {
%s};

static void
hakios_draw_bootmark(void)
{
	uint32_t x, y, top, left, cols, run, word, i;
	volatile uint32_t *fb;
	int want = 0;

	static int asked = -1;

	if (asked == -1) {
		/* on unless the payload says bootmark=0: the argument lives in the boot payload on the
		 * phone's flash, and a kernel that has to be told to draw its picture is not useful. */
		asked = PE_parse_boot_argn("bootmark", &want, sizeof(want)) ? want : 1;
	}
	if (asked == 0 || vinfo.v_depth != 32 || vinfo.v_baseAddr == 0 || vinfo.v_rowbytes < 4) {
		return;
	}
	fb = (volatile uint32_t *)(uintptr_t)vinfo.v_baseAddr;
	cols = vinfo.v_rowbytes / 4;
	for (y = 0; y < vinfo.v_height; y++) {
		for (x = 0; x < cols && x < vinfo.v_width; x++) {
			fb[(uint64_t)y * cols + x] = 0xff000000U;
		}
	}
	left = vinfo.v_width > BOOTMARK_WIDTH ? (vinfo.v_width - BOOTMARK_WIDTH) / 2 : 0;
	top = vinfo.v_height > BOOTMARK_HEIGHT ? (vinfo.v_height - BOOTMARK_HEIGHT) / 2 : 0;
	x = left;
	y = top;
	i = 0;
	while (i + 6 <= sizeof(bootmark_rle)) {
		run = (uint32_t)bootmark_rle[i] | ((uint32_t)bootmark_rle[i + 1] << 8);
		word = (uint32_t)bootmark_rle[i + 2] | ((uint32_t)bootmark_rle[i + 3] << 8) |
		    ((uint32_t)bootmark_rle[i + 4] << 16) | ((uint32_t)bootmark_rle[i + 5] << 24);
		i += 6;
		while (run--) {
			if (y >= top + BOOTMARK_HEIGHT || y >= vinfo.v_height) {
				return;
			}
			if (x >= left + BOOTMARK_WIDTH) {
				x = left;
				y++;
				continue;
			}
			if (x < vinfo.v_width) {
				fb[(uint64_t)y * cols + x] = word;
			}
			x++;
		}
	}
}
/* --- end of the boot mark ------------------------------------------------------------------- */

'''

CALL = '\t\thakios_draw_bootmark();\n'


def brace_end(text, start):
    """Index just after the closing brace of the block that opens at or after start."""
    depth = 0
    seen = False
    i = start
    while i < len(text):
        ch = text[i]
        if ch == '/' and text[i + 1:i + 2] == '*':
            i = text.find('*/', i) + 2
            continue
        if ch == '/' and text[i + 1:i + 2] == '/':
            i = text.find('\n', i) + 1
            continue
        if ch in '"\'':
            quote = ch
            i += 1
            while i < len(text) and text[i] != quote:
                i += 2 if text[i] == '\\' else 1
            i += 1
            continue
        if ch == '{':
            depth += 1
            seen = True
        elif ch == '}':
            depth -= 1
            if seen and depth == 0:
                return i
        i += 1
    raise SystemExit('no closing brace found')


def main():
    target = pathlib.Path(sys.argv[1])
    mark = pathlib.Path(sys.argv[2])
    print_only = '--print-only' in sys.argv
    raw = mark.read_text(encoding='utf-8')
    width = int(re.search(r'#define BOOTMARK_WIDTH (\d+)', raw).group(1))
    height = int(re.search(r'#define BOOTMARK_HEIGHT (\d+)', raw).group(1))
    runs = int(re.search(r'#define BOOTMARK_RUNS (\d+)', raw).group(1))
    body = raw.split('bootmark_rle[] = {', 1)[1].split('};', 1)[0]
    nums = [int(x) for x in re.findall(r'\d+', body)]
    assert len(nums) % 6 == 0 and len(nums) // 6 == runs, (len(nums), runs)
    # the drawing header already carries the panel's byte order; the words stay as they are
    lines = []
    for i in range(0, len(nums), 24):
        lines.append('    ' + ','.join(str(c) for c in nums[i:i + 24]) + ',')
    block = BLOCK % (width, height, runs, '\n'.join(lines) + '\n')

    text = target.read_text(encoding='utf-8')
    if 'hakios_draw_bootmark' in text:
        print('video_console.c already carries the mark')
        return 0
    anchor = 'struct vc_info vinfo;'
    if anchor not in text:
        raise SystemExit('the vinfo declaration moved')
    at = text.index(anchor) + len(anchor) + 1
    definition = 'initialize_screen(PE_Video * boot_vinfo, unsigned int op)\n{'
    if text.count(definition) != 1:
        raise SystemExit('the definition of initialize_screen is not where the patch expects it')
    end = brace_end(text, text.index(definition))
    patched = text[:end] + '\n' + CALL.rstrip('\n') + '\n\t' * 1 + text[end:]
    patched = patched[:at] + '\n' + block + patched[at:]
    if print_only:
        print('block %d bytes; the call lands before the closing brace at %d' % (len(block), end))
        return 0
    target.write_text(patched, encoding='utf-8')
    print('patched %s (%d -> %d bytes)' % (target.name, len(text), len(patched)))
    return 0


if __name__ == '__main__':
    sys.exit(main())
