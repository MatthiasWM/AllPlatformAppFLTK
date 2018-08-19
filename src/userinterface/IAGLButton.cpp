//
//  IAGLButton.cpp
//
//  Copyright (c) 2013-2018 Matthias Melcher. All rights reserved.
//


#include <FL/Fl.H>
#include <FL/gl.H>
#include <FL/Fl_Button.H>
#include <FL/fl_draw.H>

#include "IAGLButton.h"

static uchar active_ramp[24] = {
    FL_GRAY_RAMP+0, FL_GRAY_RAMP+1, FL_GRAY_RAMP+2, FL_GRAY_RAMP+3,
    FL_GRAY_RAMP+4, FL_GRAY_RAMP+5, FL_GRAY_RAMP+6, FL_GRAY_RAMP+7,
    FL_GRAY_RAMP+8, FL_GRAY_RAMP+9, FL_GRAY_RAMP+10,FL_GRAY_RAMP+11,
    FL_GRAY_RAMP+12,FL_GRAY_RAMP+13,FL_GRAY_RAMP+14,FL_GRAY_RAMP+15,
    FL_GRAY_RAMP+16,FL_GRAY_RAMP+17,FL_GRAY_RAMP+18,FL_GRAY_RAMP+19,
    FL_GRAY_RAMP+20,FL_GRAY_RAMP+21,FL_GRAY_RAMP+22,FL_GRAY_RAMP+23};
static uchar inactive_ramp[24] = {
    43, 43, 44, 44,
    44, 45, 45, 46,
    46, 46, 47, 47,
    48, 48, 48, 49,
    49, 49, 50, 50,
    51, 51, 52, 52};
static int draw_it_active = 1;

static uchar *flgl_gray_ramp() {return (draw_it_active?active_ramp:inactive_ramp)-'A';}

void flgl_xyline(int x, int y, int x1) {
    glBegin(GL_LINE_STRIP);
    glVertex2i(x, y);
    glVertex2i(x1+1, y);
    glEnd();
}

void flgl_yxline(int x, int y, int y1) {
    if (y1 < y) y1--;
    else y1++;
    glBegin(GL_LINE_STRIP);
    glVertex2i(x, y);
    glVertex2i(x, y1);
    glEnd();
}

void flgl_rectf(int x, int y, int w, int h) {
    if (w<=0 || h<=0) return;
    glBegin(GL_POLYGON);
    glVertex2i(x, y-1);
    glVertex2i(x+w, y-1);
    glVertex2i(x+w, y+h-1);
    glVertex2i(x, y+h-1);
    glEnd();
}

static void flgl_frame2(const char* s, int x, int y, int w, int h) {
    uchar *g = flgl_gray_ramp();
    if (h > 0 && w > 0) for (;*s;) {
        // draw bottom line:
        gl_color((Fl_Color)g[*s++]);
        flgl_xyline(x, y+h-1, x+w-1);
        if (--h <= 0) break;
        // draw right line:
        gl_color((Fl_Color)g[*s++]);
        flgl_yxline(x+w-1, y+h-1, y);
        if (--w <= 0) break;
        // draw top line:
        gl_color((Fl_Color)g[*s++]);
        flgl_xyline(x, y, x+w-1);
        y++; if (--h <= 0) break;
        // draw left line:
        gl_color((Fl_Color)g[*s++]);
        flgl_yxline(x, y+h-1, y);
        x++; if (--w <= 0) break;
    }
}

static void flgl_up_frame(int x, int y, int w, int h, Fl_Color) {
    flgl_frame2("AAWWMMTT",x,y,w,h);
}

void flgl_down_frame(int x, int y, int w, int h, Fl_Color) {
    flgl_frame2("WWMMPPAA",x,y,w,h);
}


static void flgl_box(Fl_Boxtype b, int x, int y, int w, int h, Fl_Color c)
{
    unsigned char rd, gn, bl;
    switch (b) {
        case FL_UP_BOX:
            flgl_up_frame(x,y,w,h,c);
            Fl::get_color(draw_it_active ? c : fl_inactive(c), rd, gn, bl);
            glColor4ub(rd, gn, bl, 128);
            flgl_rectf(x+2, y+2, w-4, h-4);
            gl_color(fl_color());
            break;
        case FL_DOWN_BOX:
            flgl_down_frame(x,y,w,h,c);
            Fl::get_color(draw_it_active ? c : fl_inactive(c), rd, gn, bl);
            glColor4ub(rd, gn, bl, 128);
            flgl_rectf(x+2, y+2, w-4, h-4);
            break;
        default:
            break;
    }
}


IAGLButton::IAGLButton(int x, int y, int w, int h, const char *label)
: Fl_Button(x, y, w, h, label)
{
}


IAGLButton::~IAGLButton()
{
}


void IAGLButton::draw()
{
    if (type() == FL_HIDDEN_BUTTON) return;
    Fl_Color col = value() ? selection_color() : color();
    draw_box(value() ? (down_box()?down_box():fl_down(box())) : box(), col);
    //draw_label();
    gl_font(labelfont(), labelsize());
    gl_color(labelcolor());
    fl_draw(label(), x(), y()-1, w(), h(), align(), gl_draw);
    if (Fl::focus() == this) draw_focus();
}

void IAGLButton::draw_box() const {
    int t = box();
    if (!t) return;
    draw_box((Fl_Boxtype)t, x(), y(), w(), h(), color());
}

void IAGLButton::draw_box(Fl_Boxtype b, Fl_Color c) const {
    draw_box(b, x(), y(), w(), h(), c);
}

void IAGLButton::draw_box(Fl_Boxtype b, int X, int Y, int W, int H, Fl_Color c)
const {
    draw_it_active = active_r();
    flgl_box(b, X, Y, W, H, c);
    draw_it_active = 1;
}

void IAGLButton::draw_focus() const {
    draw_focus(box(), x(), y(), w(), h());
}

void IAGLButton::draw_focus(Fl_Boxtype B, int X, int Y, int W, int H) const {
    if (!Fl::visible_focus()) return;
    switch (B) {
        case FL_DOWN_BOX:
        case FL_DOWN_FRAME:
        case FL_THIN_DOWN_BOX:
        case FL_THIN_DOWN_FRAME:
            X ++;
            Y ++;
        default:
            break;
    }
    int rx = X + Fl::box_dx(B);
    int ry = Y + Fl::box_dy(B);
    int rr = X + W - Fl::box_dw(B);
    int rb = Y + H - Fl::box_dh(B);

    gl_color(fl_contrast(FL_BLACK, color()));
    glLineStipple(1, 0x5555);
    glEnable(GL_LINE_STIPPLE);
    glBegin(GL_LINE_LOOP);
    glVertex2i(rx, ry);
    glVertex2i(rr, ry);
    glVertex2i(rr, rb);
    glVertex2i(rx, rb);
    glEnd();
    glLineStipple(1, 0xffff);
    glDisable(GL_LINE_STIPPLE);
}



//
// End of "$Id".
//