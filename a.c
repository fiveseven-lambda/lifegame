#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <stdio.h>
#include <unistd.h>

#define N 500
#define in(x, y) ((x) >= 0 && (x) < N && (y) >= 0 && (y) < N)

int main(){
	Display *display = XOpenDisplay(NULL);
	if(display == NULL){
		fputs("error: cannot open display server\n", stderr);
		return -1;
	}
	int defaultscreen = XDefaultScreen(display);
	int width = XDisplayWidth(display, defaultscreen) / 2;
	int height = XDisplayHeight(display, defaultscreen) / 2;
	Window rootWindow = XRootWindow(display, defaultscreen);
	int defaultDepth = XDefaultDepth(display, defaultscreen);
	GC defaultGC = XDefaultGC(display, defaultscreen);
	unsigned long black = XBlackPixel(display, defaultscreen);
	unsigned long white = XWhitePixel(display, defaultscreen);
	Window window = XCreateSimpleWindow(
		display,
		rootWindow,
		0, 0,
		width, height,
		0, white,
		white
	);
	XMapWindow(display, window);
	XEvent event;

	XGCValues xgc;
	xgc.foreground = black;
	GC blackGC = XCreateGC(display, window, GCForeground, &xgc);
	xgc.foreground = white;
	GC whiteGC = XCreateGC(display, window, GCForeground, &xgc);

	long scale = 10;
	struct{
		long x;
		long y;
	} center = {
		.x = 0,
		.y = 0
	};

	static char set[N][N];
	static char field[N][N];
	static int count[N][N];

	static char copy[N][N];
	struct{
		int x1, x2;
		int y1, y2;
	} copy_area;

	int sleeptime = 16384;

set:
	XSelectInput(display, window, ExposureMask | KeyPressMask | ButtonPressMask | ButtonMotionMask | ButtonReleaseMask | StructureNotifyMask);
	XClearArea(display, window, 0, 0, width, height, True);
	for(;;){
		XNextEvent(display, &event);
		switch(event.type){
			_Bool no_move, button1pressed, copying;
			struct{
				int x;
				int y;
			} pressed;
			case Expose:
				for(
					int i = (event.xexpose.x - center.x) / scale - 1;
					i <= (event.xexpose.x - center.x + event.xexpose.width) / scale;
					++i
				){
					for(
						int j = (event.xexpose.y - center.y) / scale - 1;
						j <= (event.xexpose.y - center.y + event.xexpose.height) / scale;
						++j
					){
						if(in(i + N / 2, j + N / 2) && !set[i + N / 2][j + N / 2]){
							int size = scale - (scale >= 10);
							XFillRectangle(display, window, blackGC, center.x + i * scale, center.y + j * scale, size, size);
						}
					}
				}
				break;
			case DestroyNotify:
				XFreeGC(display, blackGC);
				XFreeGC(display, whiteGC);
				XCloseDisplay(display);
				return 0;
			case ConfigureNotify:
				width = event.xconfigure.width;
				height = event.xconfigure.height;
				break;
			case ButtonPress:
				if(event.xbutton.button == Button1){
					if(copying){
						int x = event.xbutton.x;
						int y = event.xbutton.y;
						if(x >= 0) x /= scale;
						else x = -(-x - 1)/scale - 1;
						if(y >= 0) y /= scale;
						else y = -(-y - 1)/scale - 1;
						for(int i = copy_area.x1; i <= copy_area.x2; ++i){
							for(int j = copy_area.y1; j <= copy_area.y2; ++j){
								if(in(i + N / 2, j + N / 2)){
									set[i + N / 2][j + N / 2] = 0;
								}
							}
						}
						for(int i = copy_area.x1; i <= copy_area.x2; ++i){
							for(int j = copy_area.y1; j <= copy_area.y2; ++j){
								if(in(i + N / 2, j + N / 2) && in(i + x - copy_area.x1 + N / 2, j + y - copy_area.y1 + N / 2)){
									set[i + x - copy_area.x1 + N / 2][j + y - copy_area.y1 + N / 2]
										= copy[i + N / 2][j + N / 2];
								}
							}
						}
						XClearArea(display, window, 0, 0, width, height, True);
					}else{
						pressed.x = event.xbutton.x - center.x;
						pressed.y = event.xbutton.y - center.y;
						no_move = True;
						button1pressed = True;
					}
				}else if(event.xbutton.button == Button3){
					copy_area.x1 = event.xbutton.x;
					copy_area.y1 = event.xbutton.y;
					if(copy_area.x1 >= 0) copy_area.x1 /= scale;
					else copy_area.x1 = -(-copy_area.x1 - 1)/scale - 1;
					if(copy_area.y1 >= 0) copy_area.y1 /= scale;
					else copy_area.y1 = -(-copy_area.y1 - 1)/scale - 1;
				}else if(event.xbutton.button == Button4){
					center.x = event.xbutton.x - (event.xbutton.x - center.x) * (scale + 1) / scale;
					center.y = event.xbutton.y - (event.xbutton.y - center.y) * (scale + 1) / scale;
					++scale;
					XClearArea(display, window, 0, 0, width, height, True);
				}else if(event.xbutton.button == Button5 && scale > 1){
					center.x = event.xbutton.x - (event.xbutton.x - center.x) * (scale - 1) / scale;
					center.y = event.xbutton.y - (event.xbutton.y - center.y) * (scale - 1) / scale;
					--scale;
					XClearArea(display, window, 0, 0, width, height, True);
				}
				break;
			case MotionNotify:
				if(button1pressed){
					center.x = event.xmotion.x - pressed.x;
					center.y = event.xmotion.y - pressed.y;
					no_move = False;
					XClearArea(display, window, 0, 0, width, height, True);
				}
				break;
			case ButtonRelease:
				if(event.xbutton.button == Button1){
					if(copying){
						copying = False;
					}else{
						if(no_move){
							int i = pressed.x;
							int j = pressed.y;
							if(i >= 0) i /= scale;
							else i = -(-i - 1)/scale - 1;
							if(j >= 0) j /= scale;
							else j = -(-j - 1)/scale - 1;
							if(in(i + N / 2, j + N / 2)) set[i + N / 2][j + N / 2] ^= 1;
							XClearArea(display, window, center.x + i * scale, center.y + j * scale, scale, scale, True);
						}
					}
					button1pressed = False;
				}else if(event.xbutton.button == Button3){
					copy_area.x2 = event.xbutton.x;
					copy_area.y2 = event.xbutton.y;
					if(copy_area.x2 >= 0) copy_area.x2 /= scale;
					else copy_area.x2 = -(-copy_area.x2 - 1)/scale - 1;
					if(copy_area.y2 >= 0) copy_area.y2 /= scale;
					else copy_area.y2 = -(-copy_area.y2 - 1)/scale - 1;
					if(copy_area.x1 > copy_area.x2){
						int tmp = copy_area.x1;
						copy_area.x1 = copy_area.x2;
						copy_area.x2 = tmp;
					}
					if(copy_area.y1 > copy_area.y2){
						int tmp = copy_area.y1;
						copy_area.y1 = copy_area.y2;
						copy_area.y2 = tmp;
					}
					for(int i = copy_area.x1; i <= copy_area.x2; ++i){
						for(int j = copy_area.y1; j <= copy_area.y2; ++j){
							if(in(i + N / 2, j + N / 2)){
								copy[i + N / 2][j + N / 2] = set[i + N / 2][j + N / 2];
							}
						}
					}
					copying = True;
				}
				break;
			case KeyPress:
				switch(XLookupKeysym(&event.xkey, 0)){
					case XK_Return:
						goto run;
					case XK_Escape:
						XDestroyWindow(display, window);
						break;
					case 's':
						sleeptime *= 2;
						break;
					case 'f':
						if(sleeptime > 1) sleeptime /= 2;
						break;
				}
		}
	}
run:
	for(int i = 0; i < N; ++i) for(int j = 0; j < N; ++j){
		field[i][j] = set[i][j];
	}
	XSelectInput(display, window, KeyPressMask | StructureNotifyMask);
	for(;;){
		if(XPending(display)){
			XNextEvent(display, &event);
			switch(event.type){
				case DestroyNotify:
					XFreeGC(display, blackGC);
					XFreeGC(display, whiteGC);
					XCloseDisplay(display);
					return 0;
				case ConfigureNotify:
					width = event.xconfigure.width;
					height = event.xconfigure.height;
					break;
				case KeyPress:
					switch(XLookupKeysym(&event.xkey, 0)){
						case XK_Return:
							goto set;
						case XK_Escape:
							XDestroyWindow(display, window);
							break;
						case 's':
							sleeptime *= 2;
							break;
						case 'f':
							if(sleeptime > 1) sleeptime /= 2;
							break;
					}
			}
		}else{
			for(int i = 0; i < N; ++i) for(int j = 0; j < N; ++j){
				static int eight[] = {-1, -1, 0, 1, -1, 1, 1, 0, -1};
				count[i][j] = 0;
				for(int k = 0; k < 8; ++k){
					if(in(i + eight[k], j + eight[k + 1])) count[i][j] += field[i + eight[k]][j + eight[k + 1]];
				}
			}
			for(int i = 0; i < N; ++i) for(int j = 0; j < N; ++j){
				if(field[i][j] == 0 && count[i][j] == 3 || field[i][j] == 1 && (count[i][j] == 2 || count[i][j] == 3)){
					field[i][j] = 1;
				}else{
					field[i][j] = 0;
				}
			}
			for(
				int i = (-center.x) / scale - 1;
				i <= (-center.x + width) / scale;
				++i
			){
				for(
					int j = (-center.y) / scale - 1;
					j <= (-center.y + height) / scale;
					++j
				){
					int size = scale - (scale >= 10);
					if(in(i + N / 2, j + N / 2))
						XFillRectangle(
							display,
							window,
							field[i + N / 2][j + N / 2]?  whiteGC : blackGC,
							center.x + i * scale,
							center.y + j * scale,
							size,
							size
						);
				}
			}
			usleep(sleeptime);
		}
	}
}
