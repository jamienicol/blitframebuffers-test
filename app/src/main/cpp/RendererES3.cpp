/*
 * Copyright 2013 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "gles3jni.h"
#include <EGL/egl.h>
#include <vector>

#define STR(s) #s
#define STRV(s) STR(s)

struct Texture {
  Texture(int width, int height, int layers)
    : width(width), height(height)
  {
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D_ARRAY, id);

    glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA8, width, height, layers);

    fbos.resize(layers);
    glGenFramebuffers(layers, fbos.data());
    for (int i = 0; i < layers; i++) {
      glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbos[i]);
      glFramebufferTextureLayer(GL_DRAW_FRAMEBUFFER,
                                GL_COLOR_ATTACHMENT0,
                                id,
                                0,
                                i);
    }
  }

  GLuint id;
  int width;
  int height;
  std::vector<GLuint> fbos;
};

static void upload_rgb_layers(Texture* tex) {
    std::vector<unsigned char> buf(tex->width * tex->height * 4);

    // Upload red to layer 0
    for (int i = 0; i < buf.size(); i+=4) {
      buf[i]   = 0xFF;
      buf[i+1] = 0x00;
      buf[i+2] = 0x00;
      buf[i+3] = 0xFF;
    }
    glBindTexture(GL_TEXTURE_2D_ARRAY, tex->id);
    glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0,
                    0, 0, 0, tex->width, tex->height, 1,
                    GL_RGBA, GL_UNSIGNED_BYTE, buf.data());

    // Upload green to layer 1
    for (int i = 0; i < buf.size(); i+=4) {
      buf[i]   = 0x00;
      buf[i+1] = 0xFF;
      buf[i+2] = 0x00;
      buf[i+3] = 0xFF;
    }
    glBindTexture(GL_TEXTURE_2D_ARRAY, tex->id);
    glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0,
                    0, 0, 1, tex->width, tex->height, 1,
                    GL_RGBA, GL_UNSIGNED_BYTE, buf.data());

    // Upload blue to layer 2
    for (int i = 0; i < buf.size(); i+=4) {
      buf[i]   = 0x00;
      buf[i+1] = 0x00;
      buf[i+2] = 0xFF;
      buf[i+3] = 0xFF;
    }
    glBindTexture(GL_TEXTURE_2D_ARRAY, tex->id);
    glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0,
                    0, 0, 2, tex->width, tex->height, 1,
                    GL_RGBA, GL_UNSIGNED_BYTE, buf.data());
}

static void blit_src_to_dst(Texture* src, Texture* dst) {
  for (int i = 0; i < src->fbos.size(); i++) {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, src->fbos[i]);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dst->fbos[i]);
    glBlitFramebuffer(0, 0, src->width, src->height,
                      0, 0, dst->width, dst->height,
                      GL_COLOR_BUFFER_BIT,
                      GL_NEAREST);
  }
}

static void bind_src_then_copytexsubimage(Texture* src, Texture* dst) {
  glBindTexture(GL_TEXTURE_2D_ARRAY, dst->id);
  for (int i = 0; i < src->fbos.size(); i++) {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, src->fbos[i]);
    glCopyTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0,
                        0, 0, i,
                        0, 0, src->width, src->height);
  }
}


static void blit_src_to_renderbuffer_then_copytexsubimage(Texture* src, Texture* dst) {
  GLuint renderbuffer;
  glGenRenderbuffers(1, &renderbuffer);
  glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, src->width, src->height);

  GLuint fbo;
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);
  glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, renderbuffer);

  glBindTexture(GL_TEXTURE_2D_ARRAY, dst->id);

  for (int i = 0; i < src->fbos.size(); i++) {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, src->fbos[i]);
    glBlitFramebuffer(0, 0, src->width, src->height,
                      0, 0, src->width, src->height,
                      GL_COLOR_BUFFER_BIT,
                      GL_NEAREST);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
    glCopyTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0,
                        0, 0, i,
                        0, 0, src->width, src->height);
  }
}

static void copy_image_sub_data(Texture* src, Texture* dst) {
  glCopyImageSubData(src->id, GL_TEXTURE_2D_ARRAY, 0,
                     0, 0, 0,
                     dst->id, GL_TEXTURE_2D_ARRAY, 0,
                     0, 0, 0,
                     src->width, src->height, src->fbos.size());
}

class RendererES3: public Renderer {
public:
    RendererES3();
    virtual ~RendererES3();
    bool init();

private:
    virtual void draw();

    const EGLContext mEglContext;

  Texture* src;
  Texture* dst_blitframebuffer;
  Texture* dst_copytexsubimage;
  Texture* dst_renderbuffer_copytexsubimage;
  Texture* dst_copyimagesubdata;
};

Renderer* createES3Renderer() {
    RendererES3* renderer = new RendererES3;
    if (!renderer->init()) {
        delete renderer;
        return NULL;
    }
    return renderer;
}

RendererES3::RendererES3()
:   mEglContext(eglGetCurrentContext())
{
}

bool RendererES3::init() {
    ALOGV("Using OpenGL ES 3.0 renderer");

    src = new Texture(256, 256, 3);
    dst_blitframebuffer = new Texture(256, 256, 3);
    dst_copytexsubimage = new Texture(256, 256, 3);
    dst_renderbuffer_copytexsubimage = new Texture(256, 256, 3);
    dst_copyimagesubdata = new Texture(256, 256, 3);

    upload_rgb_layers(src);
    blit_src_to_dst(src, dst_blitframebuffer);
    bind_src_then_copytexsubimage(src, dst_copytexsubimage);
    blit_src_to_renderbuffer_then_copytexsubimage(src, dst_renderbuffer_copytexsubimage);
    copy_image_sub_data(src, dst_copyimagesubdata);

    return true;
}

RendererES3::~RendererES3() {
  if (eglGetCurrentContext() != mEglContext)
    return;

}

void draw_layers(Texture* tex, int y) {
  for (int i = 0; i < tex->fbos.size(); i++) {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, tex->fbos[i]);
    glBlitFramebuffer(0, 0, tex->width, tex->height,
                      i * tex->width, y, (i + 1) * tex->width, y + tex->height,
                      GL_COLOR_BUFFER_BIT, GL_NEAREST);
  }
}

void RendererES3::draw() {
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

    draw_layers(src, 0);
    draw_layers(dst_blitframebuffer, 256);
    draw_layers(dst_copytexsubimage, 512);
    draw_layers(dst_renderbuffer_copytexsubimage, 768);
    draw_layers(dst_copyimagesubdata, 1024);
}
