#include <windows.h>
#include <d3d9.h>
#include "../3rdparty/libretro.h"
#include "glad.h"
#include "gl_render.h"
#include <math.h>
video g_video;

#pragma comment(lib,"d3d9.lib")

#define VertexFVF (D3DFVF_XYZRHW|D3DFVF_TEX1)
typedef struct
{
    FLOAT x, y, z;
    FLOAT rhw;
    FLOAT u, v;
} VERTEX;
VERTEX verts[4] = { 0 };


int pow2up(int d)
{
    int c = 2048;
    while (d <= c)
        c /= 2;
    return c * 2;
}


static const PIXELFORMATDESCRIPTOR pfd =
{
  sizeof(PIXELFORMATDESCRIPTOR),
  1,
  PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
  PFD_TYPE_RGBA,
  32,
  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0,
  32,             // zbuffer
  0,              // stencil!
  0,
  PFD_MAIN_PLANE,
  0, 0, 0, 0
};

static struct {
    GLuint vao;
    GLuint vbo;
    GLuint program;
    GLint i_pos;
    GLint i_coord;
    GLint u_tex;
    GLint u_mvp;
} g_glshader = { 0 };

static float g_scale = 2;
static bool g_win = false;

static const char *g_vshader_src =
"#version 330\n"
"in vec2 i_pos;\n"
"in vec2 i_coord;\n"
"out vec2 o_coord;\n"
"uniform mat4 u_mvp;\n"
"void main() {\n"
"o_coord = i_coord;\n"
"gl_Position = vec4(i_pos, 0.0, 1.0) * u_mvp;\n"
"}";

static const char *g_fshader_src =
"#version 330\n"
"in vec2 o_coord;\n"
"uniform sampler2D u_tex;\n"
"void main() {\n"
"gl_FragColor = texture2D(u_tex, o_coord);\n"
"}";

void ortho2d(float m[4][4], float left, float right, float bottom, float top) {
    m[0][0] = 1; m[0][1] = 0; m[0][2] = 0; m[0][3] = 0;
    m[1][0] = 0; m[1][1] = 1; m[1][2] = 0; m[1][3] = 0;
    m[2][0] = 0; m[2][1] = 0; m[2][2] = 1; m[2][3] = 0;
    m[3][0] = 0; m[3][1] = 0; m[3][2] = 0; m[3][3] = 1;

    m[0][0] = 2.0f / (right - left);
    m[1][1] = 2.0f / (top - bottom);
    m[2][2] = -1.0f;
    m[3][0] = -(right + left) / (right - left);
    m[3][1] = -(top + bottom) / (top - bottom);
}
GLuint compile_glshader(unsigned type, unsigned count, const char **strings) {
    GLint status;
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, count, strings, NULL);
    glCompileShader(shader);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status == GL_FALSE) {
        char buffer[4096];
        glGetShaderInfoLog(shader, sizeof(buffer), NULL, buffer);
    }
    return shader;
}


void init_glshaders() {
    GLint status;
    GLuint vshader = compile_glshader(GL_VERTEX_SHADER, 1, &g_vshader_src);
    GLuint fshader = compile_glshader(GL_FRAGMENT_SHADER, 1, &g_fshader_src);
    GLuint program = glCreateProgram();

    glAttachShader(program, vshader);
    glAttachShader(program, fshader);
    glLinkProgram(program);
    glDeleteShader(vshader);
    glDeleteShader(fshader);
    glValidateProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &status);

    if (status == GL_FALSE) {
        char buffer[4096] = { 0 };
        glGetProgramInfoLog(program, sizeof(buffer), NULL, buffer);
    }

    g_glshader.program = program;
    g_glshader.i_pos = glGetAttribLocation(program, "i_pos");
    g_glshader.i_coord = glGetAttribLocation(program, "i_coord");
    g_glshader.u_tex = glGetUniformLocation(program, "u_tex");
    g_glshader.u_mvp = glGetUniformLocation(program, "u_mvp");

    glGenVertexArrays(1, &g_glshader.vao);
    glGenBuffers(1, &g_glshader.vbo);
    glUseProgram(g_glshader.program);
    glUniform1i(g_glshader.u_tex, 0);

    float m[4][4];
    if (g_video.hw.bottom_left_origin)
        ortho2d(m, -1, 1, 1, -1);
    else
        ortho2d(m, -1, 1, -1, 1);

    glUniformMatrix4fv(g_glshader.u_mvp, 1, GL_FALSE, (float*)m);
    glUseProgram(0);
}


void refresh_glvbo_data() {

    float bottom = (float)g_video.base_h / g_video.tex_h;
    float right = (float)g_video.base_w / g_video.tex_w;

    typedef struct
    {
        float	x;
        float   y;
        float   z;
        float   s;
        float   t;
    } vertex_data;

    vertex_data vert[] = {
        // pos, coord
        -1.0f,-1.0f,0., 0.0f,  bottom, // left-bottom
        -1.0f, 1.0f,0., 0.0f,  0.0f,   // left-top
        1.0f, -1.0f,0., right,  bottom,// right-bottom
        1.0f,  1.0f,0., right,  0.0f,  // right-top
    };

    glBindVertexArray(g_glshader.vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_glshader.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_data) * 4, vert, GL_STATIC_DRAW);
    glEnableVertexAttribArray(g_glshader.i_pos);
    glEnableVertexAttribArray(g_glshader.i_coord);
    glVertexAttribPointer(g_glshader.i_pos, 3, GL_FLOAT, GL_FALSE, sizeof(vertex_data), (void*)offsetof(vertex_data, x));
    glVertexAttribPointer(g_glshader.i_coord, 2, GL_FLOAT, GL_FALSE, sizeof(vertex_data), (void*)offsetof(vertex_data, s));
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void init_glframebuffer(int width, int height)
{
    if (g_video.fbo_id)
        glDeleteFramebuffers(1, &g_video.fbo_id);
    if (g_video.rbo_id)
        glDeleteRenderbuffers(1, &g_video.rbo_id);

    glGenFramebuffers(1, &g_video.fbo_id);
    glBindFramebuffer(GL_FRAMEBUFFER, g_video.fbo_id);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_video.tex_id, 0);
    if (g_video.hw.depth) {
        glGenRenderbuffers(1, &g_video.rbo_id);
        glBindRenderbuffer(GL_RENDERBUFFER, g_video.rbo_id);
        glRenderbufferStorage(GL_RENDERBUFFER, g_video.hw.stencil ? GL_DEPTH24_STENCIL8 : GL_DEPTH_COMPONENT24, width, height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, g_video.hw.stencil ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, g_video.rbo_id);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
    }
    glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

RECT resize_glcb(int width, int height) {
    RECT clientRect;
    GetClientRect(g_video.hwnd, &clientRect);
    int32_t rectw = clientRect.right - clientRect.left;
    int32_t recth = clientRect.bottom - clientRect.top;
    // figure out the largest area that fits in this resolution at the desired aspect ratio
    if (g_video.last_w != rectw && rectw ||
        g_video.last_h != recth && recth)
    {
        g_video.last_w = rectw;
        g_video.last_h = recth;
    }

    double aspect = (double)g_video.aspect;
    unsigned width_calc = width;
    unsigned height_calc = height;
    if (width / height_calc > aspect)
        width_calc = height_calc * aspect;
    else if (width_calc / height_calc < aspect)
        height_calc = width_calc / aspect;
    unsigned x = (unsigned)rectw / width_calc;
    unsigned y = (unsigned)recth/ height_calc;
    unsigned factor = x < y ? x : y;
    RECT view;
    view.right = (unsigned)(width_calc* factor);
    view.bottom = (unsigned)(height_calc * factor);
    view.left = (rectw - view.right) / 2;
    view.top = (recth - view.bottom) / 2;
    view.right += view.left;
    view.bottom += view.top;
    return view;
}

void create_glwindow(int width, int height, HWND hwnd) {

    g_video.hwnd = hwnd;
    g_video.hDC = GetDC(hwnd);
    unsigned int	PixelFormat;
    PixelFormat = ChoosePixelFormat(g_video.hDC, &pfd);
    SetPixelFormat(g_video.hDC, PixelFormat, &pfd);
    if (!(g_video.hRC = wglCreateContext(g_video.hDC)))return;
    if (!wglMakeCurrent(g_video.hDC, g_video.hRC))return;

    gladLoadGL();
    init_glshaders();

    typedef bool (APIENTRY *PFNWGLSWAPINTERVALFARPROC)(int);
    PFNWGLSWAPINTERVALFARPROC wglSwapIntervalEXT = 0;
    wglSwapIntervalEXT = (PFNWGLSWAPINTERVALFARPROC)wglGetProcAddress("wglSwapIntervalEXT");
    if (wglSwapIntervalEXT) wglSwapIntervalEXT(1);
    g_win = true;
    g_video.last_w = 0;
    g_video.last_h = 0;
}


void resize_to_aspect(double ratio, int sw, int sh, int *dw, int *dh) {
    *dw = sw;
    *dh = sh;

    if (ratio <= 0)
        ratio = (double)sw / sh;

    if ((float)sw / sh < 1)
        *dw = *dh * ratio;
    else
        *dh = *dw / ratio;
}

BOOL CenterWindow(HWND hwndWindow)
{
    HWND hwndParent;
    RECT rectWindow, rectParent;

    // make the window relative to its parent
    if ((hwndParent = GetParent(hwndWindow)) != NULL)
    {
        GetWindowRect(hwndWindow, &rectWindow);
        GetWindowRect(hwndParent, &rectParent);

        int nWidth = rectWindow.right - rectWindow.left;
        int nHeight = rectWindow.bottom - rectWindow.top;

        int nX = ((rectParent.right - rectParent.left) - nWidth) / 2 + rectParent.left;
        int nY = ((rectParent.bottom - rectParent.top) - nHeight) / 2 + rectParent.top;

        int nScreenWidth = GetSystemMetrics(SM_CXSCREEN);
        int nScreenHeight = GetSystemMetrics(SM_CYSCREEN);

        // make sure that the dialog box never moves outside of the screen
        if (nX < 0) nX = 0;
        if (nY < 0) nY = 0;
        if (nX + nWidth > nScreenWidth) nX = nScreenWidth - nWidth;
        if (nY + nHeight > nScreenHeight) nY = nScreenHeight - nHeight;

        MoveWindow(hwndWindow, nX, nY, nWidth, nHeight, FALSE);

        return TRUE;
    }

    return FALSE;
}

void ResizeWindow(HWND hWnd, int nWidth, int nHeight)
{
    RECT rcClient, rcWind;
    POINT ptDiff;
    GetClientRect(hWnd, &rcClient);
    GetWindowRect(hWnd, &rcWind);
    ptDiff.x = (rcWind.right - rcWind.left) - rcClient.right;
    ptDiff.y = (rcWind.bottom - rcWind.top) - rcClient.bottom;
    MoveWindow(hWnd, rcWind.left, rcWind.top, nWidth + ptDiff.x, nHeight + ptDiff.y, TRUE);
}

void video_glinit(const struct retro_game_geometry *geom, HWND hwnd)
{
    int nwidth = 0, nheight = 0;

    resize_to_aspect(geom->aspect_ratio, geom->base_width * 1, geom->base_height * 1, &nwidth, &nheight);

    nwidth *= 1;
    nheight *= 1;

    create_glwindow(nwidth, nheight, hwnd);

    if (g_video.tex_id)
        glDeleteTextures(1, &g_video.tex_id);


    g_video.tex_id = 0;

    if (!g_video.pixfmt)
        g_video.pixfmt = GL_UNSIGNED_SHORT_5_5_5_1;

    CenterWindow(hwnd);

    glGenTextures(1, &g_video.tex_id);

    g_video.pitch = geom->base_width * g_video.bpp;

    glBindTexture(GL_TEXTURE_2D, g_video.tex_id);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, geom->max_width, geom->max_height, 0,
        g_video.pixtype, g_video.pixfmt, NULL);


    glBindTexture(GL_TEXTURE_2D, 0);

    init_glframebuffer(geom->base_width, geom->base_height);


    g_video.tex_w = geom->max_width;
    g_video.tex_h = geom->max_height;
    g_video.base_w = geom->base_width;
    g_video.base_h = geom->base_height;
    g_video.aspect = geom->aspect_ratio;

    refresh_glvbo_data();
    if (g_video.hw.context_reset)g_video.hw.context_reset();
    ResizeWindow(g_video.hwnd, g_video.base_w * 2, g_video.base_h * 2);
}

int created3dtexture(int width, int height)
{
    HRESULT hr;
    if (g_video.tex)
    {
        g_video.tex->Release();
        g_video.tex = NULL;
    }
    hr = g_video.d3ddev->CreateTexture(g_video.tex_w, g_video.tex_h, 1, D3DUSAGE_DYNAMIC, (D3DFORMAT)g_video.pixfmt, D3DPOOL_DEFAULT, &g_video.tex, NULL);
    if (hr != D3D_OK) return -1;
    hr = g_video.d3ddev->SetTexture(0, g_video.tex);
    return 0;
}

void resized3d(int width, int height)
{
    HRESULT hr;
    if (IsIconic(g_video.hwnd))
        return;
    POINT ptOffset = { 0,0 };
    ClientToScreen(g_video.hwnd, &ptOffset);
    RECT r = { 0 };
    GetClientRect(g_video.hwnd, &r);
    OffsetRect(&r, ptOffset.x, ptOffset.y);
    g_video.d3dpp.BackBufferWidth = r.right - r.left;
    g_video.d3dpp.BackBufferHeight = r.bottom - r.top;
    if (g_video.tex)
    {
        g_video.tex->Release();
        g_video.tex = NULL;
    }
    if (g_video.d3ddev)g_video.d3ddev->Reset(&g_video.d3dpp);
    created3dtexture(g_video.tex_w, g_video.tex_h);
    hr = g_video.d3ddev->SetFVF(VertexFVF);
    hr = g_video.d3ddev->SetRenderState(D3DRS_LIGHTING, FALSE);
    hr = g_video.d3ddev->SetVertexShader(NULL);
    double aspect = (double)g_video.aspect;
    unsigned width_calc = width;
    unsigned height_calc = height;
    if (width / height_calc > aspect)
        width_calc = height_calc * aspect;
    else if (width_calc / height_calc < aspect)
        height_calc = width_calc / aspect;
    unsigned x = (unsigned)g_video.d3dpp.BackBufferWidth / width_calc;
    unsigned y = (unsigned)g_video.d3dpp.BackBufferHeight / height_calc;
    unsigned factor = x < y ? x : y;
    RECT r2 = { 0, 0, width_calc*factor, height_calc*factor };
    OffsetRect(&r2, (g_video.d3dpp.BackBufferWidth - r2.right) / 2, (g_video.d3dpp.BackBufferHeight - r2.bottom) / 2);
    float texw = float(g_video.base_w) / float(g_video.tex_w);
    float texh = float(g_video.base_h) / float(g_video.tex_h);
    verts[0] = { (float)r2.left - 0.5f, (float)r2.bottom - 0.5f, 0.0f, 1.0f, 0.0f, texh };
    verts[1] = { (float)r2.left - 0.5f, (float)r2.top - 0.5f, 0.0f, 1.0f, 0.0f, 0.0f };
    verts[2] = { (float)r2.right - 0.5f, (float)r2.bottom - 0.5f, 0.0f, 1.0f, texw, texh };
    verts[3] = { (float)r2.right - 0.5f, (float)r2.top - 0.5f, 0.0f, 1.0f, texw, 0.0f };
}

void video_init(const struct retro_game_geometry *geom, HWND hwnd) {

    g_video.software_rast = !g_video.hw.context_reset;
    if (g_video.software_rast)
    {
        g_video.tex_w = pow2up(geom->max_width);
        g_video.tex_h = pow2up(geom->max_height);
        g_video.base_w = geom->base_width;
        g_video.base_h = geom->base_height;
        g_video.aspect = geom->aspect_ratio;
        g_video.hwnd = hwnd;
        g_video.d3d = NULL;
        g_video.d3ddev = NULL;
        g_video.tex = NULL;
        HRESULT hr;
        POINT ptOffset = { 0,0 };
        ClientToScreen(g_video.hwnd, &ptOffset);
        RECT r = { 0 };
        GetClientRect(g_video.hwnd, &r);
        OffsetRect(&r, ptOffset.x, ptOffset.y);
        memset(&g_video.d3dpp, 0, sizeof(g_video.d3dpp));
        g_video.d3dpp.Flags = D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;
        g_video.d3dpp.BackBufferWidth = r.right - r.left;
        g_video.d3dpp.BackBufferHeight = r.bottom - r.top;
        g_video.d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;
        g_video.d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
        g_video.d3dpp.Windowed = true;
        g_video.d3dpp.hDeviceWindow = hwnd;
        g_video.d3d = Direct3DCreate9(D3D9b_SDK_VERSION);
        if (g_video.d3d == NULL) return;
        hr = g_video.d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &g_video.d3dpp, &g_video.d3ddev);
        if (hr != D3D_OK) return;
       // resized3d(geom->base_width,geom->base_height);
        ResizeWindow(g_video.hwnd, g_video.base_w < 640?g_video.base_w *3 : g_video.base_w, g_video.base_w < 480 ? g_video.base_h * 3 : g_video.base_h);
        CenterWindow(g_video.hwnd);
    }
    else
        video_glinit(geom, hwnd);
}


bool video_set_pixel_format(unsigned format) {
    switch (format) {
    case RETRO_PIXEL_FORMAT_0RGB1555:
        g_video.pixfmt = D3DFMT_A1R5G5B5;
        g_video.bpp = sizeof(uint16_t);
        break;
    case RETRO_PIXEL_FORMAT_XRGB8888:
        g_video.pixfmt = D3DFMT_X8R8G8B8;
        g_video.bpp = sizeof(uint32_t);
        break;
    case RETRO_PIXEL_FORMAT_RGB565:
        g_video.pixfmt = D3DFMT_R5G6B5;
        g_video.bpp = sizeof(uint16_t);
        break;
    default:
        break;
    }

    return true;
}




void video_refresh(const void *data, unsigned width, unsigned height, unsigned pitch) {
    if (data == NULL) return;
    if (g_video.software_rast)
    {
        HRESULT hr;
        if (g_video.base_w != width || g_video.base_h != height)
        {
            g_video.base_w = width;
            g_video.base_h = height;
            resized3d(g_video.base_w, g_video.base_h);
        }

        RECT clientRect = { 0 };
        GetClientRect(g_video.hwnd, &clientRect);
        int32_t w = clientRect.right - clientRect.left;
        int32_t h = clientRect.bottom - clientRect.top;
        if (w != g_video.last_w || h != g_video.last_h)
        {
            resized3d(g_video.base_w,g_video.base_h);
            g_video.last_w = w;
            g_video.last_h = h;
        }
        D3DLOCKED_RECT lock = { 0 };
        g_video.tex->LockRect(0, &lock, NULL, D3DLOCK_DISCARD);
        memset(lock.pBits, 0, lock.Pitch*g_video.tex_h);
        if ((unsigned int)lock.Pitch == pitch) memcpy(lock.pBits, data, pitch*(height - 1) + (width * g_video.bpp));
        else
        {
            for (unsigned int i = 0; i < height; i++)
            {
                memcpy((uint8_t*)lock.pBits + i * lock.Pitch, (uint8_t*)data + i * pitch, width * g_video.bpp);
            }
        }
        g_video.tex->UnlockRect(0);
        g_video.d3ddev->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
        hr = g_video.d3ddev->BeginScene();
        hr = g_video.d3ddev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, verts, sizeof(VERTEX));
        hr = g_video.d3ddev->EndScene();
        hr = g_video.d3ddev->Present(NULL, NULL, NULL, NULL);
    }
    else
    {
        if (g_video.base_w != width || g_video.base_h != height)
        {
            g_video.base_h = height;
            g_video.base_w = width;
            refresh_glvbo_data();
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        RECT view = resize_glcb(width, height);
        RECT clientRect;
        GetClientRect(g_video.hwnd, &clientRect);
        int32_t w = clientRect.right - clientRect.left;
        int32_t h = clientRect.bottom - clientRect.top;
        glViewport(view.left, h - view.bottom, view.right - view.left, view.bottom - view.top);
        glBindTexture(GL_TEXTURE_2D, g_video.tex_id);
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(g_glshader.program);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, g_video.tex_id);
        glBindVertexArray(g_glshader.vao);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray(0);
        glUseProgram(0);
        SwapBuffers(g_video.hDC);
    }
   
}

void video_gldeinit()
{
    if (g_video.tex_id)
    {
        glDeleteTextures(1, &g_video.tex_id);
        g_video.tex_id = 0;

    }
    if (g_video.fbo_id)
    {
        glDeleteFramebuffers(1, &g_video.fbo_id);
        g_video.fbo_id = 0;
    }


    if (g_video.rbo_id)
    {
        glDeleteRenderbuffers(1, &g_video.rbo_id);
        g_video.rbo_id = 0;
    }
    glDisableVertexAttribArray(1);
    glDeleteBuffers(1, &g_glshader.vbo);
    glDeleteVertexArrays(1, &g_glshader.vbo);

    if (g_video.hRC)
    {
        wglMakeCurrent(0, 0);
        wglDeleteContext(g_video.hRC);
    }

    if (g_video.hDC) ReleaseDC(g_video.hwnd, g_video.hDC);
    close_gl();
}

void video_deinit() {
    if (!g_video.software_rast)
        video_gldeinit();
    else
    {
        if (g_video.tex) {
            g_video.tex->Release();
            g_video.tex = 0;
        }

        if (g_video.d3ddev) {
            g_video.d3ddev->Release();
            g_video.d3ddev = 0;
        }

        if (g_video.d3d) {
            g_video.d3d->Release();
            g_video.d3d = 0;
        }

    }

}