#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <stdio.h>
#include <unistd.h>

#define N 500
#define IN(x, a, b) (((a) < (b) && (a) <= (x) && (x) <= (b)) || ((a) >= (b) && (b) <= (x) && (x) <= (a)))
#define INFIELD(x, y) (IN(x, 0, N) && IN(y, 0, N))

#define RNG(a, b) ((a) < (b) ? (b) - (a) : (a) - (b))

#define DIV(x, y) ((x) >= 0 ? (x) / (y) : -((-(x) - 1) / (y)) - 1)



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
	XSelectInput(display, window, ExposureMask | KeyPressMask | ButtonPressMask | PointerMotionMask | ButtonReleaseMask | StructureNotifyMask);
	XMapWindow(display, window);
	XEvent event;

	XGCValues xgc;
	xgc.foreground = black;
	GC blackGC = XCreateGC(display, window, GCForeground, &xgc);
	xgc.foreground = white;
	GC whiteGC = XCreateGC(display, window, GCForeground, &xgc);

	long scale = 10;
	struct point{
		int x;
		int y;
	} center = {
		.x = - N / 2 * scale,
		.y = - N / 2 * scale
	};

	static char set[N][N];
	static char field[N][N];
	static int count[N][N];

	static char copy[N][N];
	struct{
		int i1, i2;
		int j1, j2;
	} copy_area;
	enum { not_selecting, selecting, selected, pasting } select_mode;
	struct {
		int i;
		int j;
	} pasting_pointer;

	struct point pressed;
	_Bool button1pressed = False;
	_Bool no_move;

	int sleeptime = 16384;

	char copy_or_cut = 'c';
set:
	for(;;){
		XNextEvent(display, &event);
		switch(event.type){
			case Expose:
				for(
					int i = (event.xexpose.x - center.x) / scale - 1;
					i <= (event.xexpose.x - center.x + event.xexpose.width) / scale;
					++i
				)
				for(
					int j = (event.xexpose.y - center.y) / scale - 1;
					j <= (event.xexpose.y - center.y + event.xexpose.height) / scale;
					++j
				){
					if(INFIELD(i, j)){
						_Bool flag = !set[i][j];
						if(select_mode == selecting || select_mode == selected){
							flag ^= IN(i, copy_area.i1, copy_area.i2) && IN(j, copy_area.j1, copy_area.j2);
						}else if(
							select_mode == pasting &&
							IN(copy_area.i1 + i - pasting_pointer.i, copy_area.i1, copy_area.i2 - 1) &&
							IN(copy_area.j1 + j - pasting_pointer.j, copy_area.j1, copy_area.j2 - 1)
						){
							flag &= !copy[copy_area.i1 + i - pasting_pointer.i][copy_area.j1 + j - pasting_pointer.j];
						}
						int size = scale - (scale >= 10);
						if(flag) XFillRectangle(display, window, blackGC, center.x + i * scale, center.y + j * scale, size, size);
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
					if(event.xbutton.state & ControlMask){
						// Ctrl を押しながらクリック
						// 範囲選択を開始
						copy_area.i1 = copy_area.i2 = DIV(event.xbutton.x - center.x, scale);
						copy_area.j1 = copy_area.j2 = DIV(event.xbutton.y - center.y, scale);
						select_mode = selecting;
						XClearArea(display, window, 0, 0, width, height, True);
					}else if(select_mode == pasting){
						// Ctrl+V を押した後のクリック
						// 貼り付け　または　移動
						no_move = True;
					}else{
						// 白マス/黒マス切り替え　または　移動
						pressed.x = event.xbutton.x - center.x;
						pressed.y = event.xbutton.y - center.y;
						no_move = True;
					}
					button1pressed = True;
				}else if(event.xbutton.button == Button4){
					// 拡大 (今ポインタがある場所を中心に)
					center.x = event.xbutton.x - (event.xbutton.x - center.x) * (scale + 1) / scale;
					center.y = event.xbutton.y - (event.xbutton.y - center.y) * (scale + 1) / scale;
					++scale;
					XClearArea(display, window, 0, 0, width, height, True);
				}else if(event.xbutton.button == Button5 && scale > 1){
					// 縮小 (今ポインタがある場所を中心に)
					center.x = event.xbutton.x - (event.xbutton.x - center.x) * (scale - 1) / scale;
					center.y = event.xbutton.y - (event.xbutton.y - center.y) * (scale - 1) / scale;
					--scale;
					XClearArea(display, window, 0, 0, width, height, True);
				}
				break;
			case MotionNotify:
				if(button1pressed){
					// ドラッグ中に動いた
					if(select_mode == selecting){
						// 範囲選択
						copy_area.i2 = DIV(event.xmotion.x - center.x, scale);
						copy_area.j2 = DIV(event.xmotion.y - center.y, scale);
						XClearArea(display, window, 0, 0, width, height, True);
					}else{
						// 白マス/黒マス切り替え　ではなく　移動
						if(!no_move || RNG(pressed.x, event.xmotion.x - center.x) + RNG(pressed.y, event.xmotion.y - center.y) > 2){
							center.x = event.xmotion.x - pressed.x;
							center.y = event.xmotion.y - pressed.y;
							no_move = False;
							XClearArea(display, window, 0, 0, width, height, True);
						}
					}
				}else if(select_mode == pasting){
					// 貼り付け中に動いた
					pasting_pointer.i = DIV(event.xmotion.x - center.x, scale);
					pasting_pointer.j = DIV(event.xmotion.y - center.y, scale);
					XClearArea(display, window, 0, 0, width, height, True);
				}
				break;
			case ButtonRelease:
				if(event.xbutton.button == Button1){
					if(button1pressed){
						if(select_mode == selecting){
							select_mode = selected;
						}else if(no_move){
							if(select_mode == pasting){
								// 貼り付け
								for(int i = copy_area.i1; i < copy_area.i2; ++i)
								for(int j = copy_area.j1; j < copy_area.j2; ++j){
									set[pasting_pointer.i + i - copy_area.i1][pasting_pointer.j + j - copy_area.j1]
									|= copy[i][j];
								}
								select_mode = not_selecting;
								XClearArea(display, window, 0, 0, width, height, True);
							}else{
								// 白マス/黒マス切り替え
								int i = DIV(pressed.x, scale);
								int j = DIV(pressed.y, scale);
								if(INFIELD(i, j)) set[i][j] ^= 1;
								XClearArea(display, window, center.x + i * scale, center.y + j * scale, scale, scale, True);
							}
						}
						button1pressed = False;
						XClearArea(display, window, 0, 0, width, height, True);
					}
				}
				break;
			case KeyPress:
				switch(XLookupKeysym(&event.xkey, 0)){
					case 'x':
						copy_or_cut = 'x';
					case 'c':
						if(event.xkey.state & ControlMask){
							if(select_mode == selected){
								if(copy_area.i1 > copy_area.i2){
									int tmp = copy_area.i1;
									copy_area.i1 = copy_area.i2;
									copy_area.i2 = tmp;
								}
								if(copy_area.j1 > copy_area.j2){
									int tmp = copy_area.j1;
									copy_area.j1 = copy_area.j2;
									copy_area.j2 = tmp;
								}
								for(int i = copy_area.i1; i <= copy_area.i2; ++i)
								for(int j = copy_area.j1; j <= copy_area.j2; ++j)
								{
									copy[i][j] = set[i][j];
									if(copy_or_cut == 'x') set[i][j] = 0;
								}
								select_mode = not_selecting;
								XClearArea(display, window, 0, 0, width, height, True);
							}
						}
						copy_or_cut = 'c';
						break;
					case 'v':
						if(event.xkey.state & ControlMask){
							select_mode = pasting;
							XClearArea(display, window, 0, 0, width, height, True);
						}
						break;
					case XK_Return:
						goto run;
						break;
					case XK_Escape:
						XDestroyWindow(display, window);
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
							XSelectInput(
								display,
								window,
								ExposureMask |
								KeyPressMask |
								ButtonPressMask |
								PointerMotionMask |
								ButtonReleaseMask |
								StructureNotifyMask
							);
							XClearArea(display, window, 0, 0, width, height, True);
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
					if(INFIELD(i + eight[k], j + eight[k + 1])) count[i][j] += field[i + eight[k]][j + eight[k + 1]];
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
					if(INFIELD(i, j))
						XFillRectangle(
							display,
							window,
							field[i][j] ?  whiteGC : blackGC,
							center.x + i * scale,
							center.y + j * scale,
							size,
							size
						);
				}
			}
			XFlush(display);
			usleep(sleeptime);
		}
	}
}
