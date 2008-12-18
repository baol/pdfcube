// ex: set ts=2: -*- mode: C++; mode: flyspell-prog; mode: flymake; c-basic-offset: 2; indent-tabs-mode: nil -*- 
//
// PDF-Cube source file - pdfcube.cc
// 
// Copyright (C) 2006-2008 
//               Mirko Maischberger <mirko.maischberger@gmail.com>
//               Karol Sokolowski   <sokoow@gmail.com>
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WAPRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//
// Notes: please indent using 2 spaces.

// #define NDEBUG

#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <sstream>
// Gtk+ (pkg-config gtk+-2.0)
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

// GtkGLExt (pkg-config gtkglext-1.0)
#include <gtk/gtkgl.h>

// OpenGL (-lglut)
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glx.h>

// PDF to GdkPixbuf (pkg-config poppler-glib)
#include <poppler.h>

#include <boost/program_options.hpp>

using namespace std;
namespace po = boost::program_options;

//////////////////////////////////////////////////////////////////////////
// Macros

#include <cassert>

#define DEFAULT_WIDTH 640
#define DEFAULT_HEIGHT 480
#define DEFAULT_TITLE  "PDF-Cube"
#define N_FRAMES 30
#define TIMEOUT_INTERVAL 38

//////////////////////////////////////////////////////////////////////////
// Globals (will be moved inside classes some day)

enum animation { ANIM_NONE,
                 CUBE_NEXT, CUBE_PREV,
                 ZOOM0, ZOOM1, ZOOM2, ZOOM3, ZOOM4, ZOOMC,
                 SWITCH_FW, SWITCH_BW
};

static gboolean fullscreen;
static gboolean animating = FALSE;
animation active_animation = ANIM_NONE;
animation previous_animation = ANIM_NONE;
animation last_animation = ANIM_NONE;

// Cube Transitions on the command line 
// (space advances simply or with the rotating cube)
// depending on the values in this array
static bool *page_transition;

//////////////////////////////////////////////////////////////////////////
// Forward declarations

static void timeout_add(GtkWidget * widget);
static void timeout_remove(GtkWidget * widget);

static void start_animation(GtkWidget * widget, animation);
static void stop_animation(GtkWidget * widget);

static GdkGLConfig *configure_gl(void);

static GtkWidget *create_window(GdkGLConfig * glconfig);


static GLfloat clear_color[4] = { 0.6, 0.0, 0.0, 0.0 };
static GLfloat top_color[4] = { 0.7, 0.6, 0.6, 0.0 };
static double animation_emphasis = 3.0;

static bool
sleeping()
{
  return !animating && active_animation == ANIM_NONE;
}

//////////////////////////////////////////////////////////////////////////
//
// pdfcube
//
// This is an attemp to move some dirtyness inside a class
// some work still needs to be done to transform this quick 
// hack in a serious pdf-viewer
//
class pdfcube {
public:
  pdfcube(PopplerDocument* d)
    :doc(d),
     current_page(0),
     current_face(0),
     total_pages(poppler_document_get_n_pages(d)),
     frame(0),
     lookposx(0.0), lookposy(0.0), lookposz(3.48),
     atx(0.0), aty(0.0), atz(0.0), persp(44.0), angle(0.0), pixmap(0) {
    texmap[0] = 0;
    texmap[1] = 1;
    texmap[2] = 2;
    cube_faces=0;
    pixmap =
      gdk_pixbuf_new(GDK_COLORSPACE_RGB, true, 8, tex_width,
                     tex_height);
    steps = new GLfloat[N_FRAMES];
    xsteps = new double[N_FRAMES];
    zsteps = new double[N_FRAMES];
    zoomsteps = new double[N_FRAMES];
    perspsteps = new double[N_FRAMES];
    perspstepsc = new double[N_FRAMES];
  }

  // Dtor.
  ~pdfcube() {
    delete[]steps;
    delete[]xsteps;
    delete[]zsteps;
    delete[]zoomsteps;
    delete[]perspsteps;
    delete[]perspstepsc;
  }

  /// Current page.
  int page() {
    return current_page;
  }

  /// Total pages
  int pages() {
    return total_pages;
  }

  // Cube Normals
  static GLfloat n[6][3];

  // Cube Faces
  static GLint faces[6][4];

  // Cube vertex (filled in pdfcube->initialize())
  GLfloat v[8][3];

  // Cube texture mapping
  static GLfloat mapping[6][8];

  // Cube Rotation Animation steps (17 frames)
  // Cube rotation at each frame
  GLfloat *steps;
  //  ... x camera movement 
  double *xsteps;
  //  ... z camera movement
  double *zsteps;

  // Other animations
  double *zoomsteps;
  double *perspsteps;
  double *perspstepsc;

  // Restart pdf
  void restart(GtkWidget * widget) {
    current_page = 0;
    update_textures(widget);
  }

  // Jump to page
  void go_to(GtkWidget * widget, int page) {
    if (page >= 0 && page < total_pages) {
      current_page = page;
      update_textures(widget);
    }
  }

  // Jump to section (1..9)
  void section(GtkWidget * widget, int section) {
#ifndef NDEBUG
    cerr << "Section: " << section << " total pages: " <<
      total_pages << endl;
#endif
    int ii;
    for (ii = 0; ii < total_pages; ++ii) {
      if (page_transition[ii])
        section--;
      if (section == 0)
        break;
    }
#ifndef NDEBUG
    cerr << "Page: " << ii << endl;
#endif
    if (ii < total_pages) {
      current_page = ii;
      update_textures(widget);
    }
  }

  // OpenGL initialization
  void
  initialize(GtkWidget * widget) {
    GLfloat position[] = { 1.0, 1.0, 0.0, 1.0 };
    GLfloat local_view[] = { 0.0 };

    glShadeModel(GL_SMOOTH);
    glEnable(GL_TEXTURE_RECTANGLE_ARB);

    GLfloat mat_ambient[] = { 0.0, 0.0, 0.0, 1.00 };
    GLfloat mat_specular[] = { 1.0, 1.0, 1.0, 1.00 };
    GLfloat mat_shininess[] = { 3.0 };

    glMaterialfv(GL_FRONT, GL_AMBIENT, mat_ambient);
    // glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
    // glMaterialfv(GL_FRONT, GL_SHININESS, mat_shininess);

    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glEnable(GL_POLYGON_SMOOTH);
    glPolygonMode(GL_FRONT, GL_FILL);
    glEdgeFlag(GL_FALSE);
    
    glClearColor(clear_color[0], clear_color[1], clear_color[2], clear_color[3]);
    glCullFace(GL_FRONT);
    glDisable(GL_DEPTH_TEST);

//    glEnable(GL_DEPTH_TEST);
//    glDepthFunc(GL_LEQUAL);
    glLightfv(GL_LIGHT0, GL_POSITION, position);
    glLightModelfv(GL_LIGHT_MODEL_LOCAL_VIEWER, local_view);

    glFrontFace(GL_CCW);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_AUTO_NORMAL);
    glEnable(GL_NORMALIZE);

    glEnable(GL_COLOR_MATERIAL);

#ifdef ENABLE_FOG
    glEnable(GL_FOG);
    {
      GLfloat fogColor[4] = { 0.6, 0.6, 0.6, 0.5 };

      glFogi(GL_FOG_MODE, GL_LINEAR);
      glFogf(GL_FOG_START, 2.1f);
      glFogf(GL_FOG_END, 6.0f);
      glFogfv(GL_FOG_COLOR, fogColor);
      glFogf(GL_FOG_DENSITY, 0.25);
      glHint(GL_FOG_HINT, GL_DONT_CARE);
      glClearColor(fogColor[0], fogColor[1], 
                   fogColor[2], fogColor[3]);
    }
#endif
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glGenTextures(3, textures);

    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
    glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S,
                    GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T,
                    GL_CLAMP_TO_EDGE);

    glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER,
                    GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER,
                    GL_LINEAR);

    update_textures(widget);

    GLint size;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &size);
#ifndef NDEBUG
    printf("Max-texture size: %upx\n", size);
#endif
    assert(size >= 512);
    assert(glIsTexture(textures[0]));
    // Cube vertex
    v[0][0] = v[1][0] = v[2][0] = v[3][0] = -1;
    v[4][0] = v[5][0] = v[6][0] = v[7][0] = 1;
    v[0][1] = v[1][1] = v[4][1] = v[5][1] = -1;
    v[2][1] = v[3][1] = v[6][1] = v[7][1] = 1;
    v[0][2] = v[3][2] = v[4][2] = v[7][2] = 1;
    v[1][2] = v[2][2] = v[5][2] = v[6][2] = -1;

    glMatrixMode(GL_PROJECTION);
    gluPerspective(persp, 1.0, 0.5, 10.0);

    glMatrixMode(GL_MODELVIEW);
 
    // up is in positive Y direction 
    gluLookAt(lookposx, lookposy, lookposz, atx, aty, atz, 0.0, 1.0, 0.0);        

    matrix_setup();
    buildLists();
  }

  // Redraw scene
  void
  redraw(GtkWidget * widget) {

    double yoffset = 0.1;
    if (animating) {
      switch (active_animation) {
      case ANIM_NONE:
        
#ifndef NDEBUG
        cerr << "No animation... stopping right now." <<
          endl;
#endif
        frame = 0;
        stop_animation(widget);
        break;
      case CUBE_NEXT:
#ifndef NDEBUG
        cerr << "cube+ " << frame << endl;
#endif
        if (frame == N_FRAMES) {
          frame = 0;
          stop_animation(widget);
          quick_reset(widget);
        } else {
          glClear(GL_COLOR_BUFFER_BIT |
                  GL_DEPTH_BUFFER_BIT);
          glMatrixMode(GL_MODELVIEW);
          glLoadIdentity();
          lookposz -= animation_emphasis*zsteps[frame];
          lookposy = 1.5*animation_emphasis*xsteps[frame];
          gluLookAt(lookposx, lookposy, lookposz,
                    atx, aty, atz, 0, 1, 0);
          angle -= steps[frame];
          glRotatef(angle, 0.0, 1.0, 0.0);
          drawCube();
          frame++;
        }
        break;
      case CUBE_PREV:
#ifndef NDEBUG
        cerr << "cube- " << frame << endl;
#endif
        if (frame == N_FRAMES) {
          frame = 0;
          stop_animation(widget);
          quick_reset(widget);
        } else {
          glClear(GL_COLOR_BUFFER_BIT |
                  GL_DEPTH_BUFFER_BIT);
          glMatrixMode(GL_MODELVIEW);
          glLoadIdentity();
          lookposz -= 2.0 * zsteps[frame];
          lookposy = 3.0 * xsteps[frame];
          gluLookAt(lookposx, lookposy, lookposz,
                    atx, aty, atz, 0, 1, 0);
          angle -= steps[frame];
          glRotatef(angle, 0.0, -1.0, 0.0);
          drawCube();
          frame++;
        }
        break;
      case ZOOM0:
        
#ifndef NDEBUG
        cerr << "zoom0 " << frame << endl;
#endif
        if (frame == N_FRAMES) {
          frame = 0;
          stop_animation(widget);
          quick_reset(widget);
        } else {
          glClear(GL_COLOR_BUFFER_BIT |
                  GL_DEPTH_BUFFER_BIT);
          switch (previous_animation) {
          case ZOOM1:
            persp =
              perspsteps[(N_FRAMES - 1) -
                         frame];
            atx = lookposx =
              -(1.3 *
                zoomsteps[(N_FRAMES - 1) -
                          frame]);
            aty = lookposy =
              zoomsteps[(N_FRAMES - 1) -
                        frame] -
              yoffset / N_FRAMES *
              ((N_FRAMES - 1) - frame);
            break;
          case ZOOM2:
            persp =
              perspsteps[(N_FRAMES - 1) -
                         frame];
            atx = lookposx =
              1.3 *
              zoomsteps[(N_FRAMES - 1) -
                        frame];
            aty = lookposy =
              zoomsteps[(N_FRAMES - 1) -
                        frame] -
              yoffset / N_FRAMES *
              ((N_FRAMES - 1) - frame);
            break;
          case ZOOM3:
            persp =
              perspsteps[(N_FRAMES - 1) -
                         frame];
            atx = lookposx =
              -1.3 *
              zoomsteps[(N_FRAMES - 1) -
                        frame];
            aty = lookposy =
              -zoomsteps[(N_FRAMES - 1) -
                         frame] -
              yoffset / N_FRAMES *
              ((N_FRAMES - 1) - frame);
            break;
          case ZOOM4:
            persp =
              perspsteps[(N_FRAMES - 1) -
                         frame];
            atx = lookposx =
              1.3 *
              zoomsteps[(N_FRAMES - 1) -
                        frame];
            aty = lookposy =
              -zoomsteps[(N_FRAMES - 1) -
                         frame] -
              yoffset / N_FRAMES *
              ((N_FRAMES - 1) - frame);
            break;
          case ZOOMC:
            persp =
              perspstepsc[(N_FRAMES - 1) -
                          frame];
            aty = lookposy =
              -zoomsteps[(N_FRAMES - 1) -
                         frame] * 0.38;
            break;
          default:

#ifndef NDEBUG
            cerr << "Should not reach" <<
              endl;
#endif
            break;
          }
          glMatrixMode(GL_PROJECTION);
          glLoadIdentity();
          gluPerspective(persp, 1.0, 0.5, 10.0);
          glMatrixMode(GL_MODELVIEW);
          glLoadIdentity();
          gluLookAt(lookposx, lookposy, lookposz,
                    atx, aty, atz, 0, 1, 0);
          glRotatef(angle, 0.0, 1.0, 0.0);
          drawCube();
          frame++;
        }
        break;
      case ZOOM1:
#ifndef NDEBUG
        cerr << "zoom1 " << frame << endl;
#endif
        if (frame == N_FRAMES) {
          frame = 0;
          stop_animation(widget);
        } else {
          glClear(GL_COLOR_BUFFER_BIT |
                  GL_DEPTH_BUFFER_BIT);
          glMatrixMode(GL_PROJECTION);
          glLoadIdentity();
          persp = perspsteps[frame];
          gluPerspective(persp, 1.0, 0.5, 10.0);
          glMatrixMode(GL_MODELVIEW);
          glLoadIdentity();
          atx = lookposx =
            -1.3 * zoomsteps[frame];
          aty = lookposy =
            zoomsteps[frame] -
            yoffset / N_FRAMES * (frame);
          gluLookAt(lookposx, lookposy, lookposz,
                    atx, aty, atz, 0, 1, 0);
          glRotatef(angle, 0.0, 1.0, 0.0);
          drawCube();
          frame++;
        }
        break;
      case ZOOM2:
#ifndef NDEBUG
        cerr << "zoom1 " << frame << endl;
#endif
        if (frame == N_FRAMES) {
          frame = 0;
          stop_animation(widget);
        } else {
          glClear(GL_COLOR_BUFFER_BIT |
                  GL_DEPTH_BUFFER_BIT);
          glMatrixMode(GL_PROJECTION);
          glLoadIdentity();
          persp = perspsteps[frame];
          gluPerspective(persp, 1.0, 0.5, 10.0);
          glMatrixMode(GL_MODELVIEW);
          glLoadIdentity();
          atx = lookposx = 1.3 * zoomsteps[frame];
          aty = lookposy =
            zoomsteps[frame] -
            yoffset / N_FRAMES * (frame);
          gluLookAt(lookposx, lookposy, lookposz,
                    atx, aty, atz, 0, 1, 0);
          glRotatef(angle, 0.0, 1.0, 0.0);
          drawCube();
          frame++;
        }
        break;
      case ZOOM3:
#ifndef NDEBUG
        cerr << "zoom1 " << frame << endl;
#endif
        if (frame == N_FRAMES) {
          frame = 0;
          stop_animation(widget);
        } else {
          glClear(GL_COLOR_BUFFER_BIT |
                  GL_DEPTH_BUFFER_BIT);
          glMatrixMode(GL_PROJECTION);
          glLoadIdentity();
          persp = perspsteps[frame];
          gluPerspective(persp, 1.0, 0.5, 10.0);
          glMatrixMode(GL_MODELVIEW);
          glLoadIdentity();
          atx = lookposx =
            -1.3 * zoomsteps[frame];
          aty = lookposy =
            -zoomsteps[frame] -
            yoffset / N_FRAMES * (frame);
          gluLookAt(lookposx, lookposy, lookposz,
                    atx, aty, atz, 0, 1, 0);
          glRotatef(angle, 0.0, 1.0, 0.0);
          drawCube();
          frame++;
        }
        break;
      case ZOOM4:
#ifndef NDEBUG
        cerr << "zoom1 " << frame << endl;
#endif
        if (frame == N_FRAMES) {
          frame = 0;
          stop_animation(widget);
        } else {
          glClear(GL_COLOR_BUFFER_BIT |
                  GL_DEPTH_BUFFER_BIT);
          glMatrixMode(GL_PROJECTION);
          glLoadIdentity();
          persp = perspsteps[frame];
          gluPerspective(persp, 1.0, 0.5, 10.0);
          glMatrixMode(GL_MODELVIEW);
          glLoadIdentity();
          atx = lookposx = 1.3 * zoomsteps[frame];
          aty = lookposy =
            -zoomsteps[frame] -
            yoffset / N_FRAMES * (frame);
          gluLookAt(lookposx, lookposy, lookposz,
                    atx, aty, atz, 0, 1, 0);
          glRotatef(angle, 0.0, 1.0, 0.0);
          drawCube();
          frame++;
        }
        break;
      case ZOOMC:
#ifndef NDEBUG
        cerr << "zoomc " << frame << endl;
#endif
        if (frame == N_FRAMES) {
          frame = 0;
          stop_animation(widget);
        } else {
          glClear(GL_COLOR_BUFFER_BIT |
                  GL_DEPTH_BUFFER_BIT);
          persp = perspstepsc[frame];
          aty = lookposy =
            -zoomsteps[frame] * 0.38;
          glMatrixMode(GL_PROJECTION);
          glLoadIdentity();
          gluPerspective(persp, 1.0, 0.5, 10.0);
          glMatrixMode(GL_MODELVIEW);
          glLoadIdentity();
          gluLookAt(lookposx, lookposy, lookposz,
                    atx, aty, atz, 0, 1, 0);
          glRotatef(angle, 0.0, 1.0, 0.0);
          drawCube();
          frame++;
        }
        break;
      case SWITCH_FW:
#ifndef NDEBUG
        cerr << "fw " << frame << endl;
#endif
        if (frame == 1) {
          frame = 0;
          stop_animation(widget);
        } else {
          glClear(GL_COLOR_BUFFER_BIT |
                  GL_DEPTH_BUFFER_BIT);
          glMatrixMode(GL_MODELVIEW);
          glLoadIdentity();
          gluLookAt(lookposx, lookposy, lookposz,
                    atx, aty, atz, 0, 1, 0);
          angle -= 90;
          glRotatef(angle, 0.0, 1.0, 0.0);
          drawCube();
          frame++;
        }
        break;
      case SWITCH_BW:
#ifndef NDEBUG
        cerr << "bw " << frame << endl;
#endif
        if (frame == 1) {
          frame = 0;
          stop_animation(widget);
        } else {
          glClear(GL_COLOR_BUFFER_BIT |
                  GL_DEPTH_BUFFER_BIT);
          glMatrixMode(GL_MODELVIEW);
          glLoadIdentity();
          gluLookAt(lookposx, lookposy, lookposz,
                    atx, aty, atz, 0, 1, 0);
          angle += 90;
          glRotatef(angle, 0.0, 1.0, 0.0);
          drawCube();
          frame++;
        }
        break;
      }
    } else {
      switch (active_animation) {
      case ANIM_NONE:
#ifndef NDEBUG
        cerr << "Redrawing" << endl;
#endif
        break;
      case CUBE_NEXT:
#ifndef NDEBUG
        cerr << "cube stop" << endl;
#endif
        forward(widget);
        current_face = next_face();
        //          quick_reset(widget);
        break;
      case CUBE_PREV:
#ifndef NDEBUG
        cerr << "cube stop" << endl;
#endif
        backward(widget);
        current_face = prev_face();
        //          quick_reset(widget);
        break;
      case SWITCH_FW:
#ifndef NDEBUG
        cerr << "fw stop" << endl;
#endif
        forward(widget);
        current_face = next_face();
        break;
      case SWITCH_BW:
#ifndef NDEBUG
        cerr << "bw stop" << endl;
#endif
        backward(widget);
        current_face = prev_face();
        break;
      case ZOOM0:
      case ZOOM1:
      case ZOOM2:
      case ZOOM3:
      case ZOOM4:
      case ZOOMC:
      default:
#ifndef NDEBUG
        cerr << "default stop" << endl;
#endif
        break;

      }

      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      //      glClear(GL_COLOR_BUFFER_BIT);
      glMatrixMode(GL_PROJECTION);
      glLoadIdentity();
      gluPerspective(persp, 1.0, 0.5, 10.0);
      glMatrixMode(GL_MODELVIEW);
      glLoadIdentity();
      gluLookAt(lookposx, lookposy, lookposz, atx, aty, atz,
                0, 1, 0);
      glRotatef(angle, 0.0, 1.0, 0.0);
      drawCube();

      glRasterPos3f(0, -1.4, 0);
      GLuint rcube[] = {
        0, 0, 0, 127
      };
      glDrawPixels(1, 1, GL_RGBA, GL_UNSIGNED_BYTE, rcube);

      active_animation = ANIM_NONE;

#ifndef NDEBUG
      cerr << "Ok!" << endl;
#endif
      glFlush();
    }
  }

  // Prev face of the cube in [0..3]
  int prev_face() {
    if (current_face - 1 < 0)
      return 3;
    else
      return current_face - 1;
  }

  // Next face of the cube in [0..3]
  int next_face() {
    if (current_face + 1 > 3)
      return 0;
    else
      return current_face + 1;
  }

  // Prev document page
  int prev_page() {
    assert(current_page >= 0);
    if (current_page == 0)
      return total_pages - 1;
    else
      return current_page - 1;
  }

  // Next document page
  int next_page() {
    assert(current_page < total_pages);
    if (current_page == total_pages - 1)
      return 0;
    else
      return current_page + 1;
  }

  void
  forward(GtkWidget * widget) {
    update_textures_dir(widget, true);
#ifndef NDEBUG
    cerr << "Current page: " << current_page << endl;
#endif
  }

  // Initialization of animation vectors
  void matrix_setup() {
    double sum = 0.;
    for(int i=0; i<N_FRAMES; i++)
      {
        steps[i] =std::pow(::sin(i*M_PI/double(N_FRAMES)), 2);
        sum += steps[i];
      } 
    double factor = 90./sum;
    for(int i=0; i<N_FRAMES; i++)
      {
        steps[i] *= factor;
      }
#ifndef NDEBUG
    cout << "Matrix ";
    for (int i = 0; i < N_FRAMES; i++)
      cout << steps[i] << " ";
    cout << endl;
#endif

    float xstep_ratio = 0.4;
    float xstep =
      double (xstep_ratio) / (double(N_FRAMES-1) / 2.0);
    for (int i = 0; i < N_FRAMES / 2; i++) {
      xsteps[i] = i * xstep;
      if (xsteps[i] > xstep_ratio)
        xsteps[i] = xstep_ratio;
    }
    for (int i = N_FRAMES / 2; i < N_FRAMES; i++) {
      xsteps[i] = xstep_ratio - (i - double(N_FRAMES-1) / 2.0) * xstep;
      if (xsteps[i] < 0.01)
        xsteps[i] = 0.0;
    }

#ifndef NDEBUG
    cout << "Step x " << xstep << endl;
    cout << "Matrix2 ";
    for (int i = 0; i < N_FRAMES; i++)
      cout << xsteps[i] << " ";
    cout << endl;
#endif

    float granular = 0.07;
    float zstep = granular / (double(N_FRAMES) / 4.0);
    for (int i = 0; i < N_FRAMES / 4; i++) {
      zsteps[i] = -i * zstep;
    }
    for (int i = N_FRAMES / 4; i < N_FRAMES / 2; i++) {
      zsteps[i] = -granular + (i - double(N_FRAMES) / 4.0) * zstep;
    }
    for (int i = N_FRAMES / 2; i < N_FRAMES; i++) {
      zsteps[i] = -zsteps[i - N_FRAMES / 2];
    }

#ifndef NDEBUG
    cout << "Step z " << zstep << endl;
    cout << "Matrix3 ";
    for (int i = 0; i < N_FRAMES; i++)
      cout << zsteps[i] << " ";
    cout << endl;
#endif

    float zoomstop = 0.38;
    float zoomstep = (zoomstop / double (N_FRAMES-1));
    for (int i = 0; i < N_FRAMES; i++) {
      zoomsteps[i] = i * zoomstep;
    }

#ifndef NDEBUG
    cout << "Step zoom " << zoomstep << endl;
    cout << "Matrix4 ";
    for (int i = 0; i < N_FRAMES; i++)
      cout << zoomsteps[i] << " ";
    cout << endl;
#endif

    float perspstart = 44.00;
    float perspstop = 21.00;
    for (int i = 0; i < N_FRAMES; i++) {
      perspsteps[i] = (cos(i*M_PI/(double(N_FRAMES)*2.0)))*(perspstart - perspstop)+perspstop;
    }
#ifndef NDEBUG
    // cout << "Step persp " << perspstep << endl;
    cout << "Matrix5 ";
    for (int i = 0; i < N_FRAMES; i++)
      cout << perspsteps[i] << " ";
    cout << endl;
#endif

    float perspcstart = 44.00;
    float perspcstop = 30.00;
    for (int i = 0; i < N_FRAMES; i++) {
      perspstepsc[i] = (1.0+cos(i*M_PI/double(N_FRAMES)))/2.0*(perspcstart - perspcstop)+perspcstop;
    }
#ifndef NDEBUG
    cout << "Matrix6 ";
    for (int i = 0; i < N_FRAMES; i++)
      cout << perspstepsc[i] << " ";
    cout << endl;
#endif
  }

  // Go to the previous page
  void
  backward(GtkWidget * widget) {
    update_textures_dir(widget, false);
#ifndef NDEBUG
    cerr << "Current page: " << current_page << endl;
#endif
  }

  // Reset internal status and updates all textures
  void
  reset(GtkWidget * widget) {
    animating = FALSE;
    frame = 0;
    lookposx = 0.0;
    lookposy = 0.0;
    lookposz = 3.48;
    atx = 0.0;
    aty = 0.0;
    atz = 0.0;
    persp = 44.0;
    angle = 0.0;
    current_face = 0;
    active_animation = ANIM_NONE;
    previous_animation = ANIM_NONE;
    last_animation = ANIM_NONE;
    update_textures(widget);
  }

  // Reset status without updating textures
  void
  quick_reset(GtkWidget * widget) {
    animating = FALSE;
    frame = 0;
    lookposx = 0.0;
    lookposy = 0.0;
    lookposz = 3.48;
    atx = 0.0;
    aty = 0.0;
    atz = 0.0;
    persp = 44.0;
    angle = 0.0;
    current_face = 0;
    active_animation = ANIM_NONE;
    previous_animation = ANIM_NONE;
    last_animation = ANIM_NONE;
  }

  // shift old textures and render the new page
  // texmap[0] -> current page
  // texmap[1] -> prev page
  // texmap[2] -> next page
  void update_textures_dir(GtkWidget * widget, bool forward) {
    assert(current_page >= 0);
    assert(current_page < total_pages);
    if (forward) {
      current_page = next_page();
      int tmp = texmap[2];
      texmap[2] = texmap[1];
      texmap[1] = texmap[0];
      texmap[0] = tmp;
      render_page(pixmap, next_page(), tex_width, tex_height);
    } else {
      current_page = prev_page();
      int tmp = texmap[0];
      texmap[0] = texmap[1];
      texmap[1] = texmap[2];
      texmap[2] = tmp;
      render_page(pixmap, prev_page(), tex_width, tex_height);
    }

    glBindTexture(GL_TEXTURE_RECTANGLE_ARB,
                  textures[texmap[forward ? 2 : 1]]);
    glTexImage2D(GL_TEXTURE_RECTANGLE_ARB,
                 0,
                 GL_RGBA,
                 tex_width,
                 tex_height,
                 0,
                 GL_RGBA,
                 GL_UNSIGNED_BYTE, gdk_pixbuf_get_pixels(pixmap));

    gdk_window_invalidate_rect(widget->window, &widget->allocation,
                               FALSE);

  }

  // render all (3) textures
  void update_textures(GtkWidget * widget) {

    assert(current_page >= 0);
    assert(current_page < total_pages);
    render_page(pixmap, current_page, tex_width, tex_height);
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, textures[texmap[0]]);
    glTexImage2D(GL_TEXTURE_RECTANGLE_ARB,
                 0,
                 GL_RGBA,
                 tex_width,
                 tex_height,
                 0,
                 GL_RGBA,
                 GL_UNSIGNED_BYTE, gdk_pixbuf_get_pixels(pixmap));

    render_page(pixmap, prev_page(), tex_width, tex_height);
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, textures[texmap[1]]);
    glTexImage2D(GL_TEXTURE_RECTANGLE_ARB,
                 0,
                 GL_RGBA,
                 tex_width,
                 tex_height,
                 0,
                 GL_RGBA,
                 GL_UNSIGNED_BYTE, gdk_pixbuf_get_pixels(pixmap));

    render_page(pixmap, next_page(), tex_width, tex_height);
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, textures[texmap[2]]);
    glTexImage2D(GL_TEXTURE_RECTANGLE_ARB,
                 0,
                 GL_RGBA,
                 tex_width,
                 tex_height,
                 0,
                 GL_RGBA,
                 GL_UNSIGNED_BYTE, gdk_pixbuf_get_pixels(pixmap));

    gdk_window_invalidate_rect(widget->window, &widget->allocation,
                               FALSE);

  }

protected:
  PopplerDocument * doc;
  int current_page;
  int current_face;
  const int total_pages;
  int frame;
  double lookposx, lookposy, lookposz;
  double atx, aty, atz;
  double persp, angle;
  GdkPixbuf *pixmap;
  int texmap[3];

  // OpenGL Textures
  GLuint textures[3];
  GLuint cube_faces;

  // Width and Height of the rendered pixmap (aspect
  // ratio is fixed, should instead depend on the 
  // aspect ratio of the pdf page)
  static const gint tex_width = (gint) (3 * 1024 / 2);
  static const gint tex_height = (gint) (3 * 768 / 2);

  // renders the poppler page on a pixmap
  void
  render_page(GdkPixbuf * pm, int i, gint iWidth, gint iHeight) {
    PopplerPage *page;
    page = poppler_document_get_page(doc, i);
    double w, h;
    poppler_page_get_size(page, &w, &h);
    poppler_page_render_to_pixbuf(page, 0, 0, iWidth, iHeight,
                                  1.0 * iWidth / w, 0, pm);
  }

  void buildLists()
  {
    int i;
    cube_faces=glGenLists(6);
    
    for (i = 0; i < 6; i++) {
      glNewList(cube_faces+i,GL_COMPILE);
      glBegin(GL_QUADS);
      if(i<4)
        glMultiTexCoord2f(GL_TEXTURE0_ARB,(1.0 - mapping[i][4]) * tex_width,
                          mapping[i][5] * tex_height);
      glVertex3fv(&v[faces[i][0]][0]);
      
      if(i<4)
        glMultiTexCoord2f(GL_TEXTURE0_ARB,(1.0 - mapping[i][6]) * tex_width,
                          mapping[i][7] * tex_height);
      glVertex3fv(&v[faces[i][1]][0]);
      
      if(i<4)
        glMultiTexCoord2f(GL_TEXTURE0_ARB,(1.0 - mapping[i][0]) * tex_width,
                          mapping[i][1] * tex_height);
      glVertex3fv(&v[faces[i][2]][0]);
      
      if(i<4)
        glMultiTexCoord2f(GL_TEXTURE0_ARB,(1.0 - mapping[i][2]) * tex_width,
                          mapping[i][3] * tex_height);
      glVertex3fv(&v[faces[i][3]][0]);
      glEnd();
      glEndList();
    }
    
  }

  void
  drawCube(void) {
    int i;

    for (i = 0; i < 6; i++) {
      if (i == current_face) {
        glEnable(GL_TEXTURE_RECTANGLE_ARB);
	glActiveTextureARB(GL_TEXTURE0_ARB);	
        glBindTexture(GL_TEXTURE_RECTANGLE_ARB,
                      textures[texmap[0]]);
      } else if (i == prev_face()) {
        glEnable(GL_TEXTURE_RECTANGLE_ARB);
	glActiveTextureARB(GL_TEXTURE0_ARB);	
        glBindTexture(GL_TEXTURE_RECTANGLE_ARB,
                      textures[texmap[1]]);
      } else if (i == next_face()) {
        glEnable(GL_TEXTURE_RECTANGLE_ARB);
	glActiveTextureARB(GL_TEXTURE0_ARB);	
        glBindTexture(GL_TEXTURE_RECTANGLE_ARB,
                      textures[texmap[2]]);
      } else if (i <= 3) {
        glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE);
        glDisable(GL_TEXTURE_RECTANGLE_ARB);
        glColor4f(top_color[0], top_color[1], top_color[2], top_color[4]);
      } else {
        glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE);
        glDisable(GL_TEXTURE_RECTANGLE_ARB);
        glColor4f(top_color[0], top_color[1], top_color[2], top_color[4]);
      }
      glPolygonMode(GL_FRONT, GL_FILL);
      glCallList(cube_faces+i);
    }
  }
};

// cube normals
GLfloat
pdfcube::n[6][3] = {
  {0.0, 0.0, -1.0}, 
  {1.0, 0.0, 0.0}, 
  {0.0, 0.0, 1.0}, 
  {-1.0, 0.0, 0.0},
  {0.0, 1.0, 0.0},
  {0.0, -1.0, 0.0}
};
//bcube faces
GLint
pdfcube::faces[6][4] = {
  {7, 4, 0, 3},
  {7, 6, 5, 4},
  {5, 6, 2, 1},
  {0, 1, 2, 3},
  {3, 2, 6, 7},
  {4, 5, 1, 0}
};
// face mapping
GLfloat
pdfcube::mapping[6][8] = {
  {1.0, 1.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0}
  ,
  {0.0, 1.0, 1.0, 1.0, 1.0, 0.0, 0.0, 0.0}
  ,
  {0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 1.0, 0.0}
  ,
  {1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0}
  ,
  {1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0}
  ,                        // top
  {1.0, 1.0, 0.0, 1.0, 0.0, 0.0, 1.0, 0.0}
  ,                        // bottom
};

// the pdf-cube
pdfcube* pc;

//////////////////////////////////////////////////////////////////////////
// Callbacks

/***
 *** The "realize" signal handler. All the OpenGL initialization
 *** should be performed here, such as default background colour,
 *** certain states etc.
 ***/
static void
realize(GtkWidget * widget, gpointer data)
{
  GdkGLContext *
    glcontext = gtk_widget_get_gl_context(widget);
  GdkGLDrawable *
    gldrawable = gtk_widget_get_gl_drawable(widget);

#ifndef NDEBUG
  g_print("%s: \"realize\"\n", gtk_widget_get_name(widget));
#endif
  //g_mutex_lock (gl_mutex);

  /*** OpenGL BEGIN ***/
  if (!gdk_gl_drawable_gl_begin(gldrawable, glcontext))
    return;

  pc->initialize(widget);

  gdk_gl_drawable_gl_end(gldrawable);
  /*** OpenGL END ***/
  //g_mutex_unlock (gl_mutex);
}

/***
 *** The "configure_event" signal handler. Any processing required when
 *** the OpenGL-capable drawing area is re-configured should be done here.
 *** Almost always it will be used to resize the OpenGL viewport when
 *** the window is resized.
 ***/
static
gboolean
configure_event(GtkWidget * widget, GdkEventConfigure * event, gpointer data)
{
  GdkGLContext *
    glcontext = gtk_widget_get_gl_context(widget);
  GdkGLDrawable *
    gldrawable = gtk_widget_get_gl_drawable(widget);

  GLsizei
    w = widget->allocation.width;
  GLsizei
    h = widget->allocation.height;

#ifndef NDEBUG
  g_print("%s: \"configure_event\"\n", gtk_widget_get_name(widget));
#endif
  //g_mutex_lock (gl_mutex);
  /*** OpenGL BEGIN ***/
  if (!gdk_gl_drawable_gl_begin(gldrawable, glcontext))
    return FALSE;

  glViewport(0, 0, w, h);

  gdk_gl_drawable_gl_end(gldrawable);

  /*** OpenGL END ***/
  //g_mutex_unlock (gl_mutex);

  return TRUE;
}

/***
 *** The "expose_event" signal handler. All the OpenGL re-drawing should
 *** be done here. This is repeatedly called as the painting routine
 *** every time the 'expose'/'draw' event is signalled.
 ***/
static
gboolean
expose_event(GtkWidget * widget, GdkEventExpose * event, gpointer data)
{
  GdkGLContext *
    glcontext = gtk_widget_get_gl_context(widget);
  GdkGLDrawable *
    gldrawable = gtk_widget_get_gl_drawable(widget);

  //g_mutex_lock (gl_mutex);
  /*** OpenGL BEGIN ***/
  if (!gdk_gl_drawable_gl_begin(gldrawable, glcontext))
    return FALSE;

  glDrawBuffer(GL_BACK);

  pc->redraw(widget);

  /* Swap buffers */
  if (gdk_gl_drawable_is_double_buffered(gldrawable))
    gdk_gl_drawable_swap_buffers(gldrawable);
  else
    glFlush();

  gdk_gl_drawable_gl_end(gldrawable);
  /*** OpenGL END ***/

  //g_mutex_unlock (gl_mutex);

  return TRUE;
}

/***
 *** The timeout function. Often in animations,
 *** timeout functions are suitable for continous
 *** frame updates.
 ***/
static
gboolean
timeout(GtkWidget * widget)
{
  /* Invalidate the whole window. */
  gdk_window_invalidate_rect(widget->window, &widget->allocation, FALSE);

  /* Update synchronously. */
  gdk_window_process_updates(widget->window, FALSE);

  return TRUE;
}

/***
 *** The "unrealize" signal handler. Any processing required when
 *** the OpenGL-capable window is unrealized should be done here.
 ***/
static void
unrealize(GtkWidget * widget, gpointer data)
{
  GdkGLContext *
    glcontext = gtk_widget_get_gl_context(widget);
  GdkGLDrawable *
    gldrawable = gtk_widget_get_gl_drawable(widget);

#ifndef NDEBUG
  g_print("%s: \"unrealize\"\n", gtk_widget_get_name(widget));
#endif
  //g_mutex_lock (gl_mutex);

  /*** OpenGL BEGIN ***/
  if (!gdk_gl_drawable_gl_begin(gldrawable, glcontext))
    return;

  gdk_gl_drawable_gl_end(gldrawable);
  /*** OpenGL END ***/
  //g_mutex_unlock (gl_mutex);
}

/***
 *** The "motion_notify_event" signal handler. Any processing required when
 *** the OpenGL-capable drawing area is under drag motion should be done here.
 ***/
static
gboolean
motion_notify_event(GtkWidget * widget, GdkEventMotion * event, gpointer data)
{
#ifndef NDEBUG
  g_print("%s: \"motion_notify_event\": button",
          gtk_widget_get_name(widget));
  if (event->state & GDK_BUTTON1_MASK) {
    g_print(" 1");
  }

  if (event->state & GDK_BUTTON2_MASK) {
    g_print(" 2");
  }

  if (event->state & GDK_BUTTON3_MASK) {
    g_print(" 3");
  }

  g_print("\n");
#endif
  return FALSE;
}

/***
 *** The "button_press_event" signal handler. Any processing required when
 *** mouse buttons (only left and middle buttons) are pressed on the OpenGL-
 *** capable drawing area should be done here.
 ***/
static
gboolean
button_press_event(GtkWidget * widget, GdkEventButton * event, gpointer data)
{
#ifndef NDEBUG
  g_print("%s: \"button_press_event\": ", gtk_widget_get_name(widget));
#endif
  if (event->button == 1) {
#ifndef NDEBUG
    g_print("button 1\n");
#endif
    return TRUE;
  }

  if (event->button == 2) {
#ifndef NDEBUG
    g_print("button 2\n");
#endif
    return TRUE;
  }
#ifndef NDEBUG
  g_print("\n");
#endif
  return FALSE;
}

/***
 *** The "key_press_event" signal handler. Any processing required when key
 *** presses occur should be done here.
 ***/
static
gboolean
key_press_event(GtkWidget * widget, GdkEventKey * event, gpointer data)
{
#ifndef NDEBUG
  g_print("%s: \"key_press_event\": ", gtk_widget_get_name(widget));
#endif
  if (event->state == GDK_CONTROL_MASK)
    {
      switch (event->keyval) {
      case GDK_1:
      case GDK_2:
      case GDK_3:
      case GDK_4:
      case GDK_5:
      case GDK_6:
      case GDK_7:
      case GDK_8:
      case GDK_9:
#ifndef NDEBUG
        g_print("Ctrl-n key\n");
#endif
        if (sleeping())
          pc->section(widget, event->keyval - GDK_1 + 1);
        break;
        
        // Let's quit
      case GDK_q:
#ifndef NDEBUG
        g_print("Ctrl-q key\n");
#endif
        gtk_main_quit();
        break;
        
        // Update all textures
      case GDK_l:
#ifndef NDEBUG
        g_print("Ctrl-l key\n");
        cerr << "Pagina: " << pc->page() << endl;
#endif
        pc->reset(widget);
        break;
      }
    } 
  else
    {
      switch (event->keyval) {
        
        // return to page 1
      case GDK_1:
      case GDK_2:
      case GDK_3:
      case GDK_4:
      case GDK_5:
      case GDK_6:
      case GDK_7:
      case GDK_8:
      case GDK_9:
#ifndef NDEBUG
        g_print("n key\n");
#endif
        if (sleeping())
          pc->go_to(widget, (event->keyval - GDK_1) * 5);
        break;
        
        // Animated Cube Advancement
      case GDK_a:
#ifndef NDEBUG
        g_print("a key\n");
#endif
        if (sleeping())
          start_animation(widget, CUBE_PREV);
        
        break;
      case GDK_c:
#ifndef NDEBUG
        g_print("c key\n");
#endif
        if (sleeping())
          start_animation(widget, CUBE_NEXT);
        break;
        
        // Quick switch to next page
      case GDK_Right:
#ifndef NDEBUG
        g_print("- key\n");
#endif
        if (sleeping())
          start_animation(widget, SWITCH_FW);
        break;
        
        // Quick switch to previous page
      case GDK_Page_Up:
      case GDK_Left:
#ifndef NDEBUG
        g_print("+ key\n");
#endif
        if (sleeping())
          start_animation(widget, SWITCH_BW);
        break;
        
      case GDK_g:
#ifndef NDEBUG
        g_print("Zoom0 key\n");
#endif
        if (sleeping())
          if (last_animation >=
              ZOOM1 and last_animation <= ZOOMC)
            start_animation(widget, ZOOM0);
        break;
        
      case GDK_h:
#ifndef NDEBUG
        g_print("Zoom1 key\n");
#endif
        if (sleeping())
          if (last_animation >=
              ZOOM1 and last_animation <= ZOOMC)
            start_animation(widget, ZOOM0);
          else
            start_animation(widget, ZOOM1);
        break;
        
      case GDK_j:
#ifndef NDEBUG
        g_print("Zoom2 key\n");
#endif
        if (sleeping())
          if (last_animation >=
              ZOOM1 and last_animation <= ZOOMC)
            start_animation(widget, ZOOM0);
          else
            start_animation(widget, ZOOM2);
        break;
        
      case GDK_k:
#ifndef NDEBUG
        g_print("Zoom3 key\n");
#endif
        if (sleeping())
          if (last_animation >=
              ZOOM1 and last_animation <= ZOOMC)
            start_animation(widget, ZOOM0);
          else
            start_animation(widget, ZOOM3);
        break;
        
      case GDK_l:
#ifndef NDEBUG
        g_print("Zoom4 key\n");
#endif
        if (sleeping())
          if (last_animation >=
              ZOOM1 and last_animation <= ZOOMC)
            start_animation(widget, ZOOM0);
          else
            start_animation(widget, ZOOM4);
        break;
        
      case GDK_z:
#ifndef NDEBUG
        g_print("Zoom key\n");
#endif
        if (sleeping())
          if (last_animation >=
              ZOOM1 and last_animation <= ZOOMC)
            start_animation(widget, ZOOM0);
          else
            start_animation(widget, ZOOMC);
        break;
        
        // Automatic advance (you should set the Animated slides on the command line)
      case GDK_Page_Down:
      case GDK_space:
#ifndef NDEBUG
        g_print("Advance key\n");
#endif
        if (page_transition[pc->page()]and sleeping())
          start_animation(widget, CUBE_NEXT);
        else if (sleeping())
          start_animation(widget, SWITCH_FW);
        
        break;
        
        // switch fullscreen
      case GDK_f:
#ifndef NDEBUG
        g_print("f key\n");
#endif
        if ((fullscreen = !fullscreen) == true)
          gtk_window_fullscreen((GtkWindow *) (data));
        else
          gtk_window_unfullscreen((GtkWindow *) (data));
        break;
        
        // Let's quit
      case GDK_Escape:
#ifndef NDEBUG
        g_print("Escape key\n");
#endif
        gtk_main_quit();
        break;
        
      default:
#ifndef NDEBUG
        g_print("Unknown key\n");
#endif
        return FALSE;
      }
    }
  return TRUE;
}

//////////////////////////////////////////////////////////////////////////
// Timeout functions

/***
 *** Helper functions to add or remove the timeout function.
 ***/

static guint
timeout_id = 0;

static void
timeout_add(GtkWidget * widget)
{
  if (timeout_id == 0) {
    timeout_id = g_timeout_add(TIMEOUT_INTERVAL,
                               (GSourceFunc) timeout, widget);
  }
}

static void
timeout_remove(GtkWidget * widget)
{
  if (timeout_id != 0) {
    g_source_remove(timeout_id);
    timeout_id = 0;
  }
}

/***
 *** The "map_event" signal handler. Any processing required when the
 *** OpenGL-capable drawing area is mapped should be done here.
 ***/
static
gboolean
map_event(GtkWidget * widget, GdkEvent * event, gpointer data)
{
#ifndef NDEBUG
  g_print("%s: \"map_event\":\n", gtk_widget_get_name(widget));
#endif
  if (animating)
    timeout_add(widget);

  return TRUE;
}

/***
 *** The "unmap_event" signal handler. Any processing required when the
 *** OpenGL-capable drawing area is unmapped should be done here.
 ***/
static
gboolean
unmap_event(GtkWidget * widget, GdkEvent * event, gpointer data)
{
#ifndef NDEBUG
  g_print("%s: \"unmap_event\":\n", gtk_widget_get_name(widget));
#endif
  timeout_remove(widget);

  return TRUE;
}

/***
 *** The "visibility_notify_event" signal handler. Any processing required
 *** when the OpenGL-capable drawing area is visually obscured should be
 *** done here.
 ***/
static
gboolean
visibility_notify_event(GtkWidget * widget,
                        GdkEventVisibility * event, gpointer data)
{
  if (animating) {
    if (event->state == GDK_VISIBILITY_FULLY_OBSCURED)
      timeout_remove(widget);
    else
      timeout_add(widget);
  }

  return TRUE;
}

/**************************************************************************
 * The following section contains some miscellaneous utility functions.
 **************************************************************************/

/***
 *** Toggle animation.
 ***/
static void
start_animation(GtkWidget * widget, enum animation a)
{
  if (sleeping()) {
    animating = true;
    previous_animation = last_animation;
    last_animation = active_animation = a;
    timeout_add(widget);
  }
}

static void
stop_animation(GtkWidget * widget)
{
  animating = false;
  timeout_remove(widget);
  gdk_window_invalidate_rect(widget->window, &widget->allocation, FALSE);
  gdk_window_process_updates(widget->window, FALSE);
}

//////////////////////////////////////////////////////////////////////////
// GTK+ GUI Functions

/***
 *** Creates the simple application window with one
 *** drawing area that has an OpenGL-capable visual.
 ***/
static GtkWidget *
create_window(GdkGLConfig * glconfig)
{
  GtkWidget *
    window;
  GtkWidget *
    vbox;
  GtkWidget *
    drawing_area;

  /*
   * Top-level window.
   */

  window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(window), DEFAULT_TITLE);

  /* Get automatically redrawn if any of their children changed allocation. */
  gtk_container_set_reallocate_redraws(GTK_CONTAINER(window), TRUE);

  /* Connect signal handlers to the window */
  g_signal_connect(G_OBJECT(window), "delete_event",
                   G_CALLBACK(gtk_main_quit), NULL);

  /*
   * VBox.
   */

  vbox = gtk_vbox_new(FALSE, 0);
  gtk_container_add(GTK_CONTAINER(window), vbox);
  gtk_widget_show(vbox);

  /*
   * Drawing area to draw OpenGL scene.
   */

  drawing_area = gtk_drawing_area_new();
  gtk_widget_set_size_request(drawing_area, DEFAULT_WIDTH,
                              DEFAULT_HEIGHT);

  /* Set OpenGL-capability to the widget */
  gtk_widget_set_gl_capability(drawing_area,
                               glconfig, NULL, TRUE, GDK_GL_RGBA_TYPE);

  gtk_widget_add_events(drawing_area,
                        GDK_BUTTON1_MOTION_MASK |
                        GDK_BUTTON2_MOTION_MASK |
                        GDK_BUTTON_PRESS_MASK |
                        GDK_VISIBILITY_NOTIFY_MASK);

  /* Connect signal handlers to the drawing area */
  g_signal_connect_after(G_OBJECT(drawing_area), "realize",
                         G_CALLBACK(realize), NULL);
  g_signal_connect(G_OBJECT(drawing_area), "configure_event",
                   G_CALLBACK(configure_event), NULL);
  g_signal_connect(G_OBJECT(drawing_area), "expose_event",
                   G_CALLBACK(expose_event), NULL);
  g_signal_connect(G_OBJECT(drawing_area), "unrealize",
                   G_CALLBACK(unrealize), NULL);

  g_signal_connect(G_OBJECT(drawing_area), "motion_notify_event",
                   G_CALLBACK(motion_notify_event), NULL);
  g_signal_connect(G_OBJECT(drawing_area), "button_press_event",
                   G_CALLBACK(button_press_event), NULL);

  /* key_press_event handler for top-level window */
  g_signal_connect_swapped(G_OBJECT(window), "key_press_event",
                           G_CALLBACK(key_press_event), drawing_area);

  /* For timeout function. */
  g_signal_connect(G_OBJECT(drawing_area), "map_event",
                   G_CALLBACK(map_event), NULL);
  g_signal_connect(G_OBJECT(drawing_area), "unmap_event",
                   G_CALLBACK(unmap_event), NULL);
  g_signal_connect(G_OBJECT(drawing_area), "visibility_notify_event",
                   G_CALLBACK(visibility_notify_event), NULL);

  gtk_box_pack_start(GTK_BOX(vbox), drawing_area, TRUE, TRUE, 0);

  gtk_widget_show(drawing_area);

  return window;
}

/***
 *** Configure the OpenGL framebuffer.
 ***/
static GdkGLConfig *
configure_gl(void)
{
  GdkGLConfig *
    glconfig;

  /* Try double-buffered visual */
  glconfig = gdk_gl_config_new_by_mode((GdkGLConfigMode)
                                       (GDK_GL_MODE_RGBA |
                                        GDK_GL_MODE_ALPHA |
                                        GDK_GL_MODE_RGB |
                                        GDK_GL_MODE_DEPTH |
                                        GDK_GL_MODE_DOUBLE));
  if (glconfig == NULL) {
    g_print("\n*** Cannot find the double-buffered visual.\n");
    g_print("\n*** Trying single-buffered visual.\n");

    /* Try single-buffered visual */
    glconfig = gdk_gl_config_new_by_mode((GdkGLConfigMode)
                                         (GDK_GL_MODE_RGB |
                                          GDK_GL_MODE_DEPTH));
    if (glconfig == NULL) {
      g_print
        ("*** No appropriate OpenGL-capable visual found.\n");
      exit(1);
    }
  }

  return glconfig;
}

///
/// @brief Gets the absolute path of a filename.
///
/// This function checks if the given @a fileName is an absolute path. If
/// it is then it returns a copy of it, otherwise it prepends the current
/// working directory to it.
///
/// @param fileName The filename to get the absolute path from.
///
/// @return A copy of the absolute path to the file name. This copy must be
///         freed when no longer needed.
///
gchar *
get_absolute_file_name(const gchar * fileName)
{
  gchar *
    absoluteFileName = NULL;
  if (g_path_is_absolute(fileName)) {
    absoluteFileName = g_strdup(fileName);
  } else {
    gchar *
      currentDir = g_get_current_dir();
    absoluteFileName = g_build_filename(currentDir, fileName, NULL);
    g_free(currentDir);
  }

  return absoluteFileName;
}

//////////////////////////////////////////////////////////////////////////
// Main function: should we use getopts? (who doesn't?)
// (update: we do, let's use boost::program_options ;))

int
main(int argc, char *argv[])
{

  po::options_description opts("Available options");

  opts.add_options()
    ("help,h", 
     "This help message.")
    ("version,v",
     "Version information")
    ("bgcolor,b", po::value<std::string>(),
     "Background color is 'r,g,b' with real values between 0.0 and 1.0, no spaces.")
    ("top-color,t", po::value<std::string>(),
     "Cube top color in 'r,g,b' format again with reals in [0,1].")
    ("transitions,c", po::value<std::vector<int> >()->multitoken(), 
     "Pages at wich to do a cube transition by default eg. 2 4 7\nMust be the last option on the command line.")
    ("input-file,i", po::value<std::string>(), "PDF file to show.")
    ("no-fullscreen,n", 
     "Don't activate full-screen mode by default.");

  po::positional_options_description p;
  p.add("input-file", -1);

  po::variables_map vm;
  po::store(po::command_line_parser(argc, argv).
            options(opts).positional(p).run(), vm);
  po::notify(vm);

  string input_file;

  if(vm.count("help")) {
    cout << endl << "PDFCube 0.0.3" << endl;
    cout << "=============" << endl;
    cout << "Copyright (C) 2006-2008 Mirko Maischberger <mirko.maischberger@gmail.com>" << endl;
    cout << "                   2008 Karol Sokolowski   <sokoow@gmail.com>" <<  endl << endl;
    cout << opts << endl;
    cout << endl;
    cout << "Usage examples:" << endl;
    cout << "  $ pdfcube presentation.pdf" << endl;
    cout << "  $ pdfcube presentation.pdf --bgcolor 0,0,0 --top-color 0.6,0.2,0.2 --transitions 1 5 7"
         << endl << endl << endl;
    return 0;
  }
  
  if(vm.count("version")) {
    cout << "pdfcube 0.0.3" << endl;
    return 0;
  }

  if (vm.count("input-file")) {
    input_file = vm["input-file"].as<std::string>();
  } else {
    cerr << "You must specify an input PDF file on the command line." << endl;
    exit(1);
  }

  if(vm.count("bgcolor")) {
    vector<double> cc; 
    string v;
    std::istringstream iss(vm["bgcolor"].as<std::string>());
    while(getline(iss, v, ',')) cc.push_back(::atof(v.c_str())); 
    if(cc.size() != 3)
      {
        cerr << "You should specify 3 values for background-color." << endl;
        exit(1);
      }
    std::copy(&cc[0], &cc[3], &clear_color[0]);
  }

  if(vm.count("top-color")) {
    vector<double> tc; 
    string v;
    std::istringstream iss(vm["top-color"].as<std::string>());
    while(getline(iss, v, ',')) tc.push_back(::atof(v.c_str())); 
    if(tc.size() != 3)
      {
        cerr << "You should specify 3 values for top-color." << endl;
        exit(1);
      }
    std::copy(&tc[0], &tc[3], &top_color[0]);
  }

  GtkWidget *
    window;
  GdkGLConfig *
    glconfig;

  /* Initialize GTK. */
  gtk_init(&argc, &argv);

  /* Initialize GtkGLExt. */
  gtk_gl_init(&argc, &argv);

  /* Configure OpenGL framebuffer. */
  glconfig = configure_gl();

  gchar *
    absoluteFileName = get_absolute_file_name(input_file.c_str());
  gchar *
    filename_uri = g_filename_to_uri(absoluteFileName, NULL, NULL);
  g_free(absoluteFileName);
  if (NULL == filename_uri) {
    cerr << "File name error." << endl;
  }
  PopplerDocument *
    document = poppler_document_new_from_file(filename_uri, NULL, NULL);

  if (document == NULL) {
    perror("Invaild PDF file.");
    exit(1);
  }

  pc = new pdfcube(document);

  page_transition = new bool[pc->pages()];
  std::fill(&page_transition[0], &page_transition[pc->pages()], false);
  if(vm.count("transitions")) {
    vector<int> tr = vm["transitions"].as<std::vector<int> >();
    for (std::vector<int>::iterator ii = tr.begin(); ii != tr.end(); ++ii) {
      if(*ii > pc->pages() || *ii < 1) 
        {
          cerr << "Transision after end of file." << endl;
        }
      else
        {
          page_transition[*ii-1] = true;
#ifndef NDEBUG
          cerr << "Transision at: " << (*ii-1) << endl;
#endif
        }
    }
  }

  /* Create and show the application window. */
  window = create_window(glconfig);
  
  if(vm.count("no-fullscreen"))
    fullscreen = FALSE;
  else
    fullscreen = TRUE;

  if(fullscreen)
    gtk_window_fullscreen((GtkWindow *) (window));
  
  gtk_widget_show(window);

  gtk_main();

  return 0;
}

// EOF
//////////////////////////////////////////////////////////////////////////
